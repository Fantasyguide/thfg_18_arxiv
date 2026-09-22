# TH18 主动卡 — C 键释放的完整链路

> **版本**：TH18 v1.00a（`th18.exe`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：一手反编译 12 张主动卡的 `c_press` / `__on_tick_2` / `method_4C` / `__on_load__2`，
> 加上输入层、玩家状态机、HUD 与 replay 四侧的调用点。
> **可信度**：全链路 ✅；物理按键名 🟡（社区来源，见 §1）。
> **前置**：[`01-object-model.md`](01-object-model.md)、[`03-hooks.md`](03-hooks.md)。

## 0. 一句话结论

按一次 C 走完这条链：
**原始按键位 → `INPUT_PRESSED` 上升沿 → 玩家 case 1 的四重门控 → 选中卡的 `vtable+0x08` →
卡把状态机置 1、清「经过帧」计时器、装填「充能倒计时」、置 `flags |= 0x20`**。
之后每帧由 `AbilityManager__on_tick` 调 `vtable+0x2c` 推进效果；效果结束回到 state 0，
从这一刻起充能倒计时才开始递减，减到 0 才能再按。

## 1. 输入层：`INPUT_PRESSED` 是上升沿，不是按住

`Input__compute_edges` `0x42ABC0`（本仓命名，原 `FUN_0042abc0`）每帧从
`INPUT_HELD` `0x4ca428` 和上一帧 `INPUT_HELD_PREV` `0x4ca42c` 算出四个派生掩码：

| 全局 | 地址 | 定义 |
| --- | --- | --- |
| `INPUT_HELD` | `0x4ca428` | 本帧按住（录制时 `= DAT_004ca210` 的原始位；回放时由 replay 写入）|
| `INPUT_PRESSED` | `0x4ca434` | `(held ^ prev) & held` = **本帧刚按下** |
| `INPUT_RELEASED` | `0x4ca438` | `~held & (held ^ prev)` = 本帧刚松开 |
| `INPUT_REPEAT` | `0x4ca430` | 按住 0x19 帧后每 8 帧重触发（菜单用）|
| `INPUT_HELD_OVER_7F` | `0x4ca440` | 按住超过 7 帧 |

**位义（一手，逐位有证据）**：

| 位 | 语义 | 证据 |
| --- | --- | --- |
| `0x001` | 射击 | `Player__tick_shooting_state` `0x45EA06` 起多处 |
| `0x002` | 炸弹 | `Player__on_tick__body` case 1/4：`(INPUT_PRESSED & 2)` → `do_bomb` |
| `0x008` | 低速/聚焦 | `Player__sub_45b170` `0x45B3FA`：`player+0x476cc = INPUT_HELD >> 3 & 1` |
| `0x010/0x020/0x040/0x080` | 上 / 下 / 左 / 右 | 同上的八向分支 `0x45B2BD` 起 |
| `0x400` | **使用卡** | `0x45C048` 的 `c_press` 分派 |
| `0x800` | **切换选中卡** | 同处 `set_selected_active_card(-1)` |

> **物理按键**：社区（THBWiki）记为 **C = 使用、D = 切换**。引擎侧只到「位」为止，
> 键位映射走可配置的输入层，本文不把它当一手结论 —— 标 🟡。
> 顺带一手：`SUPERVISOR.config` 的 `0x200` 位开启时，「按住射击超过 9 帧自动补上 `0x008`」
> （`0x462940` 内），即「长按射击自动低速」。

## 2. 玩家侧的分派与四重门控（`Player__on_tick__body` `0x45BE90`，case 1）

```c
if (ABILITY_MANAGER_PTR != 0) {
    if (   GUI_PTR != 0
        && GUI_PTR->msg == 0                      // ① 没有对话
        && ENEMY_MANAGER_PTR != 0
        && *(int*)(ENEMY_MANAGER_PTR + 0x198) != 0 // ② 关卡在跑
        && (INPUT_PRESSED & 0x400)                 // ③ C 键上升沿
        && ABILITY_MANAGER_PTR->selected_active_card != 0)  // ④ 有选中卡
        selected->vtable[0x08]();                  // c_press
    if ((INPUT_PRESSED & 0x800) && set_selected_active_card(mgr, -1))
        play_sound(0x4e);                          // 切卡音
}
...
for (card in list) card->vtable[0x24]();           // on_tick(每帧，不受上面门控)
```

四条门控里有两条（无对话、关卡在跑）**同样出现在卡内的充能递减与经过帧递增里**——
即**对话中和关卡未开始时，卡既不能放也不充能**。

**切卡** `AbilityManager__set_selected_active_card` `0x408B00`：
`param == -1` 时从当前选中卡的 `list_node.prev` 往前找下一张 `flags & 8` 的卡（到头绕回链表尾），
命中即写 `mgr+0x38` 并刷 HUD，返回 1；`param >= 0` 时按 card_id 定位。
**遍历上限是 `num_total`（+0x28），不是 `num_active`** —— 所以卡组里没有主动卡时它安全返回 0。

## 3. `c_press` 模板（12 张主动卡共用；以 `CardTenshi__c_press` `0x40EBF0` 为样本）

```c
if (card->state(+0x54) == 0 && card->recharge_cur(+0x38) < 1) {     // ★ 唯一门控
    card+0x58 = 玩家坐标（部分卡加固定偏移）;  card+0x64 = 同上（跟随用的目标点）
    card->anm_id(+0x1c) = 生成效果 ANM VM(脚本号逐卡不同, Tenshi = 0x1c);
    card->state(+0x54) = 1;
    Timer_reset(card + 0x20);                     // 经过帧清零：previous=-1, current=0, current_f=0
    play_sound(0x4d);                             // 逐卡不同
    dur = (float)card->recharge_time(+0x48) * ABILITY_MANAGER_PTR[+0xc58];   // ★ 充能倍率
    card->recharge_cur(+0x38)   = (int)dur;
    card->recharge_cur_f(+0x3c) = dur;
    card->recharge_prev(+0x34)  = (int)dur - 1;
    card->flags(+0x50) |= 0x20;                   // 正在释放
    card+0x70 = 0;                                // 逐卡的效果累加器（Tenshi:已清弹数）
}
return 0;                                          // 返回值不被调用方使用
```

三个要点：

1. **门控只看两件事**：状态机空闲 + 充能到底。**不检查金钱、火力、残机**——
   主动卡的成本完全体现在充能时长里（`CardUtusho` 的每 6 帧扣 1 power 是效果本身，不是门槛）。
2. **`ABILITY_MANAGER_PTR + 0xc58` 是全局充能倍率**，`reset_cards` 置 `1.0`，
   `CardByakuren__on_load` `0x40CBD0`（id 26 魔法卷物）置 `0.8` → 全部主动卡充能 −20%。
   它在**装填时**相乘，所以换卡/丢卡不会追溯修改已在走的倒计时。
3. **`flags & 0x20` 是「正在释放」**，`method_40`（+0x40）读它，HUD 用来切图标配色。

## 4. 状态机与效果推进（`__on_tick_2`，+0x2c，由 `AbilityManager__on_tick` 每帧调）

`card+0x54` 是三态（样本 `CardTenshi____on_tick_2` `0x40E8C0`）：

| state | 做什么 | 转移 |
| --- | --- | --- |
| **0 空闲** | 清 `flags & ~0x20`；若无对话 + 关卡在跑 + `+0x38 > 0` → `Timer__decrement(card+0x34)` | `c_press` → 1 |
| **1 激活** | 推进效果：跟随玩家、驱动 ANM、逐帧清弹并累加战果 | 到时长上限或战果满 → 2 |
| **2 收尾** | 清 `flags & ~0x20` | `card+0x24`（经过帧 current）> 8 → 0，并销毁效果 VM |

函数**末尾无条件**（同样受无对话 + 关卡在跑门控）对 `card+0x20` 调
`Timer__increment` `0x405990` —— 这就是「经过帧」的来源，state 1/2 的时长判据都读它。

Tenshi 的具体效果（作为「主动卡怎么写」的完整样例）：
state 1 每帧把要石位置向玩家插值（系数 `0x4b90b8`），对 `card+0x58` 调
`BulletManager__cancel_radius_as_bomb(pos, 0, 99999, 0)` + `LaserManager__cancel_in_radius`，
把 `BULLET_MANAGER->__some_cancel_related_counter` 累加进 `card+0x70`；
`card+0x70 > 0xf9`（**250 发**）或经过帧 `> 0x708`（**1800 帧 = 30 秒**）→ 进 state 2 并放音 `0x29`。
一手地址（2026-09-04）：`cancel_radius_as_bomb` `0x429370` stdcall(pos, mode, max, tag) + XMM2 半径 = 18.0（`0x4b9290`），
计数器 `BULLET_MANAGER+0x7a41e8`（`0x40eb56`），有命中那帧 `vm+0x524`（D3DCOLOR ARGB）写 `0xff0080ff` = 蓝 (0,128,255)（`0x40eb60`）；跟随目标 `(x, y − 80)`、系数 0.04（`0x4b90b8`）。

## 5. 重置与存取：另外三个槽

| 槽 | 实现（Tenshi） | 做什么 |
| --- | --- | --- |
| `__on_load__2`(+0x34) | `0x40E7F0` | 清 `flags & ~2`、清经过帧、`state = 0`、销毁效果 VM。**不动充能** |
| `method_4C`(+0x4c) | `0x40E840` | 同上，**再把充能倒计时也清零** → 局末重置 |
| `method_38`(+0x38) | 基类 `0x4130F0` | `set_recharge_timer(n)`：`current = n; previous = n−1; current_f = n` |

→ **`__on_load__2` 与 `method_4C` 的差别只有「要不要清充能」**，这正是
「跨关保留充能进度 vs 局末清账」的分界。

## 6. HUD 与 replay 都读同一个量

- **HUD**：`AbilityManager__on_tick` `0x408640` 对选中卡和 `mgr+0x458/+0x858` 里的每张主动卡
  调 `AbilityManager__draw_active_card_hud_entry` `0x408890`，后者算
  `fill = 1.0 − card+0x3c / card+0x48`，并用 `vtable+0x40`（`is_firing`）选配色。
- **replay**：`AbilityManager__dump_recharge_timers` `0x408BA0` 用 `vtable+0x3c` 把每张卡的
  充能剩余写成 −1 终止的数组存进 replay `+0xe64`；回放时用 `vtable+0x38` 灌回。

两者都指向 **+0x34 那组计时器 = 充能**，这是 [`01-object-model.md`](01-object-model.md) §3
那条订正的独立佐证。

> ★ 左下角那块「当前卡」UI 的**全链路**（anm 脚本号 / VM 树 / 显隐判据 / 出场与压暗的一手数值 /
> 屏幕矩形）单开了一篇：[`15-selected-card-hud.md`](15-selected-card-hud.md)。本文 §6 只到「充能比从哪来」。

## 7. 12 张主动卡（`c_press` 实现地址一览）

| card_id | 内部名 | 类 | `c_press` | `__on_tick_2` | 充能（帧/秒）|
| --- | --- | --- | --- | --- | --- |
| 41 | WARP | `CardYukari` | `0x40A1B0` | `0x40A180` | 非帧倒计时 🟡 |
| 42 | KOZUCHI | `CardShinmyoumaru` | `0x40F0F0` | `0x40EED0` | 2400 / 40 |
| 43 | KANAME | `CardTenshi` | `0x40EBF0` | `0x40E8C0` | 3600 / 60 |
| 44 | MOON | `CardClownpiece` | `0x40E040` | `0x40DCE0` | 2700 / 45 |
| 45 | MIKOFLASH | `CardMiko` | `0x40E5C0` | `0x40E3D0` | 1800 / 30 |
| 46 | VAMPIRE | `CardRemilia` | `0x40F670` | `0x40F3A0` | 1200 / 20 |
| 47 | SUN | `CardUtusho` | `0x40FB60` | `0x40F920` | 18000 / 300 |
| 48 | LILY | `CardLilyWhite` | `0x40FE70` | `0x40FE20` | 7200 / 120 |
| 49 | BASSDRUM | `CardRaiko` | `0x410250` | `0x410110` | 600 / 10 |
| 50 | PSYCO | `CardSumireko` | `0x410780` | `0x410500` | 1500 / 25 |
| 52 | CYLINDER | `CardTsukasa` | `0x410E60` | `0x410E10` | 构造未设 🟡 |
| 53 | RICEBALL | `CardMegumu` | `0x410BD0` | `0x410A90` | 5400 / 90 |

效果描述与社区对账见 [`08-catalog.md`](08-catalog.md) §A、[`09-community-crosscheck.md`](09-community-crosscheck.md)。
`CardYukari` 和 `CardTsukasa` 的充能字段没在构造里赋值，是仍待确认的两处 🟡。

## 8. 给改造者的接缝清单

想**替换一张主动卡的行为**，最小改动面是三处（每处都要按
[`mods/_template/AUDIT-checklist.md`](../../../mods/_template/AUDIT-checklist.md) 做对抗审计）：

| 想改什么 | 改哪 |
| --- | --- |
| 释放条件（成本、冷却之外的门槛）| 该卡 `c_press` 开头的 `state == 0 && +0x38 < 1` |
| 效果本身 | 该卡 `__on_tick_2` 的 state 1 分支 |
| 冷却时长 | `allocate_new_card` 该 case 里写 `card+0x48` 的那条指令；或全局改 `mgr+0xc58` |
| 让新按键触发 | `Player__on_tick__body` `0x45C048` 的位掩码 `0x400` |

⚠️ **别在 `c_press` 里假设返回值有意义**——调用方 `0x45C048` 直接丢弃它。
⚠️ **`__on_tick_2` 受 `mgr+0xc60 != 0` 与 `GAME_THREAD+4 → +4 & 2` 门控**，
在非游戏态（菜单、商店）里根本不跑；把逻辑放这里的卡在商店界面是冻结的。
