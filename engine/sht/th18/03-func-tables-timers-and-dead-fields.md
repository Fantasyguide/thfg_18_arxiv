# 03 · TH18 SHT：行为函数表 / 发射计时器 / 头部死字段 / 子机位置块
> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

> 一手：2026-09-06，Ghidra（ghidra-re MCP）。目的：给 [`../WIKI-DRAFT.md`](../WIKI-DRAFT.md) 补齐 TH16 已有而 TH18 缺的四块
> （[`../th16/03`](../th16/03-th16-funcstar-jumptables.md) 的 func 表、[`../th16/05`](../th16/05-th16-flags-no-runtime-read.md) 的死字段、
> [`../th16/07`](../th16/07-th16-shooterset-organization.md) 的计时器、option 位置块）。承接 [`01`](01-file-layout-and-shooterset-index.md)、[`02`](02-shooter-record.md)。

## 1. 发射计时器：15 帧 / 120 帧 ✅

`Player::tick_shooting_state` `0x45ea00` 尾部：

```c
short = player+0x476d4;  if (short < 14)  Timer__increment(short); else if (shoot held) short -= 14; else short = -1;
long  = player+0x476e8;  if (long  < 119) Timer__increment(long);  else if (shoot held) long  -= 119; else long = -1;
```

（`FUN_00452bf0(timer, n)` = 计时器减 n，乘游戏速度。）→ 短计时器 0..14 循环 = **15 帧**，长计时器 0..119 = **120 帧**。
按下射击键时若两者 `< 0` 则都置 0（`Player::set_shoot_key_short_timer` `0x45a550`）；松开后 `-1`。
发射判定同 [`02`](02-shooter-record.md) §2。社区（sht-webedit README）「LoLK 起 120 帧」在 TH18 上一手坐实。

## 2. 头部 +0x04 / +0x08 / +0x0c 在 TH18 **无人读** ✅

- `Player::initialize` `0x45a7a0` 只改写 `+0x24 = 100`，**不覆写** +0x04/+0x08/+0x0c（TH16 会，见 `th16:0x440fb0`）。
- 全程序对 `player+0x47940`（sht 基址）的引用共 **32 处 / 20 个函数**（`search_text "0x47940"`）：
  `Player::initialize / reset / operator_delete`、`repopulate_options`（读 +0x40 / +0x90）、`tick_shooting_state`（+0x20 / +0xe0）、
  `shoot_one_bullet`、`PlayerBullet::create`、`tick_bullets` `0x45edb0`、`GameThread::on_tick`（+0x28）、
  五张卡（`CardSaki/Suwako::__on_shoot_2`、`CardAya::on_tick`、`CardSumireko::__on_tick_2`、`CardMomoyo::on_bullet_init`）、
  五个行为函数（`0x45fc80`、`0x460320`、`0x4609c0`、`0x460b60`、`0x460cb0`）。
  其中读头部的只有：initialize / reset 读 +0x10..+0x1c（移速，×128 定点）、+0x20、+0x24、+0x02；`GameThread::on_tick` 读 +0x28。
  **没有任何函数读 +0x04 / +0x08 / +0x0c**。
- 判定与道具半径改由 `Player::reset` `0x45abb0` 用常量设：`+0x4799c = 3.0`、`+0x479a4 = 1.5`（ExpHP 名 `__init_3f` / `__init_1f`，🟡 哪个是判定点未细看）、
  `item_collect_radius = 30`、`item_attraction_radius = 70`（低速 / 高速同）、`poc_height = 128`、吸附速度 5.0。

→ 对 wiki：TH16 是「读了再用常量覆写」，TH18 是「干脆不读」；两作的 `.sht` 判定字段都是死的。

## 3. 子机位置块：三角索引 ✅

`Player::repopulate_options_and_notify_cards` `0x45d5e0`：

```c
lvl = CURRENT_POWER / 100;                       // 火力档 0..4
for (i = 0; i < lvl; i++) {
    k = TRI[CHARACTER*8 + lvl] + i;              // TRI @0x4cdb5c = {0, 0, 1, 3, 6, 10, 11, 13}（四角色相同）
    option[i].pos_unfocus = sht[0x40 + k*8] * 128,  sht[0x44 + k*8] * 128;
    option[i].pos_focus   = sht[0x90 + k*8] * 128,  sht[0x94 + k*8] * 128;
    option[i].anm_script  = (0x4b7020 + CHARACTER*4)  → {15, 17, 12, 10};  // 0x4b7030 → {16, 18, 13, 12}（另一形态）
}
```

- 高速块 `+0x40`（10 对）、低速块 `+0x90`（10 对），每对 `(x, y)` float，×128 转定点；`TRI[lvl]` = 三角数 0/1/3/6。
- 咲夜（CHARACTER == 2）额外把 `+0x40` 的 x 当**角度**用（归一到 (−π, π]），并挂钩子 `FUN_0045dd10`——她的子机绕圈。
  零售 `pl02.sht` 的位置块确实是 `(0, 38) (0.3, 38) (−0.3, 38) …`：x 是弧度、y 是半径。

## 4. `tick_bullets` `0x45edb0` 对 shooter `+0x21` 的读取 ✅

每帧逐弹：先调 `func_on_tick`（非 0 返回 = 弹作废），然后**出屏检测**：
`if (shooter[0x21] == 2 || 存活 < 15 帧 || 四角有一角在屏内) 继续; else 销毁`。
→ `+0x21 == 2` 的弹**永不因出屏销毁**（激光类）。与 [`02`](02-shooter-record.md) §1 的 `mode` 语义互证。
同时把弹位置 / 尺寸 / dmg 拷进伤害源（`damage_sources[idx-1]`），与 [`../th16/08`](../th16/08-th16-player-damage-pipeline.md) 同构。

## 5. 四张行为函数表 ✅（地址）

PE 直读（`0x4b41f0` 起 0x60 字节连续三张，draw 表在 .data）：

```
hit  @0x4b41f0 : [0]=0 [1]=0x45f9d0 [2]=0x460320 [3]=0x4609c0 [4]=0x460cb0 [5]=0x460f30 [6]=0x461080 [7]=0
tick @0x4b4210 : [0]=0 [1]=0x45f7c0 [2]=0x45fc80 [3]=0x4609a0 [4]=0x460db0 [5]=0x460ae0 [6]=0 [7]=0
init @0x4b4230 : [0]=0 [1]=0x45f7b0 [2]=0x45fc20 [3]=0x460b60 [4]=0x460da0 [5]=0x4612d0 [6]=0x461330 [7]=0x4613b0
draw @0x4cf414 : 全 0（唯一引用点 = 装载循环；TH18 与 TH16 一样从不派发 draw）
```

零售四文件用到：init {0..7 全}、tick {0..5 全}、hit {0..5}、draw 恒 0（`shtstat18.py` 268 条）。主炮组合：

| 机体 | 高速子机 (init, tick, hit) | 低速子机 |
| --- | --- | --- |
| 灵梦 pl00 | (1, 1, 0) | (0, 0, 1) |
| 魔理沙 pl01 | (2, 2, 2) | (0, 3, 3) |
| 咲夜 pl02 | (3, 0, 0) | (3, 0, 0) |
| 早苗 pl03 | (0, 0, 4) | (4, 4, 5) |

本体弹（`option = 0`）四机体全为 (0, 0, 0)。

### 5.1 逐项行为（一手反编译；子 agent 只给硬判据、不喂 TH16 标签独立读，8 个裸码项已 `create_function`）

弹对象字段：`+0x60` 速度、`+0x64` 角度、`+0x8c` 状态（1 活动 / 2 完成）、`+0x90` 寻的目标 id、`+0xa0` 激光长度、`+0xac` packed id、`+0xb0` 伤害源索引、`+0x0` 位 2–5 子状态。

| 表/idx | 地址 | 一手行为 | 判据结论 | 零售用 |
| --- | --- | --- | --- | --- |
| init1 | `0x45f7b0` | `+0x90 = 0`（清寻的目标） | — | 灵梦高速 |
| init2 | `0x45fc20` | `+0xa0 = 0`；伤害源 `ds+0x80 = 1`（线型）、`ds+0x14 = 0`；清 flag bit0；SE 0x14 | 激光初始化 | 魔理沙高速 |
| init3 | `0x460b60` | 读 shooter `+0x14` 与 **`+0x4c`**：`角 = norm(PLAYER+0x4790c + sh.angle) + PLAYER+0x47910 × sh[0x4c]`；位置从子机定点数换算 | **发射瞬间定向**（随自机横移倾斜：`+0x4790c` 由左右键推向 ±常量，`+0x47910` 横移时向 1 增长、静止时衰减——`tick_shooting_state` 头部） | 咲夜全部子机 |
| init4 | `0x460da0` | 清子状态位 2–5、`+0x90 = 0` | — | 早苗低速 |
| init5 | `0x4612d0` | `+0x64 = norm(PLAYER+0x479cc)` | 发射瞬间定向（卡写入的瞄准角）| 爱丽丝卡 |
| init6 | `0x461330` | `+0x64 = norm(rand[−π,π) × 0.5 − π/2)` = 上半平面随机角 | — | 卡 |
| init7 | `0x4613b0` | `+0x64 = atan2(弹 − 自机) ± 2°`（重合时 π/2）；**`+0x60 = 16`** | 发射瞬间定向（径向）| 卡（索引 0x16 那组）|
| tick1 | `0x45f7c0` | `find_nearest_enemy_id` 半径 **256**；`diff = wrap(atan2(敌−弹) − 角)`；60 帧内每帧 `角 += diff × 0.08`；`\|diff\|<15°` 加速 +0.2（上限 16）、`≥45°` 减速 −0.2（下限 4）；60 帧后只加速不再转；无目标 `+0.1` | **寻的** | 灵梦高速 |
| tick2 | `0x45fc80` | 位置钉在子机；目标角 = `sh.angle`，**低速（`PLAYER+0x476cc`）时 = −π/2 竖直**；`\|diff\|≥0.001` 直接贴否则 lerp 0.1；`+0xa0 += 18` 至 **512**；anm 精灵按长度拉伸；`ds` 端点 = 中点；按 `sh+0x20` 槽位与火力档决定开关 | **激光** | 魔理沙高速 |
| tick3 | `0x4609a0` | 活动态 `+0x60 += 0.3` | 匀加速 | 魔理沙低速 |
| tick4 | `0x460db0` | 子状态 0 时扫敌：`\|ey−by\| ≤ 16 且 \|ex−bx\| ≥ 16` → 锁定：速度 0、记敌坐标、子状态 4；4 帧后 `+0x64 = 0 或 −π`（敌在右/左）、**`+0x60 = 14`**、子状态 8 | 锁敌→水平冲刺（一次性，非逐帧追踪）| 早苗低速 |
| tick5 | `0x460ae0` | `x < −192 或 > 192` → `+0x64 = norm(−π − 角)` | 左右墙反弹 | 卡 |
| hit1 | `0x45f9d0` | 生成 anm 脚本 **0x95** 特效，朝向 `角 + 180° ± 20°`，颜色随机；弹 `+0x60 ×= 0.125`、`+0x8c = 2` | 命中生成特效 | 灵梦低速 |
| hit2 | `0x460320` | 激光↔目标几何求交（圆用 `ds+0x18`，矩形另算），把 `+0xa0` / `ds+0x14` 截到接触点（+8）；每 2 帧在束端生成 **`sh+0x26`** 号 anm 特效（`0x429ad0` 20 帧位移） | 激光命中 | 魔理沙高速 |
| hit3 | `0x4609c0` | `create_damage_source(pos, 半径 10, dmg = sh+0x02)`，`ds+0x80 = 4`；弹 `+0x60 = 0.3`；SE 0x41 | 伤害源子体 | 魔理沙低速 |
| hit4 | `0x460cb0` | 同 hit3 但半径 **20**、`+0x60 = 2.0` | 伤害源子体 | 早苗高速 |
| hit5 | `0x460f30` | 特效 0x95，朝向 `角 ± 20°`（不翻转、不染色） | 命中生成特效 | 早苗低速 |
| hit6 | `0x461080` | 同 hit5 + 20 帧内缩放 1→3 + 染色 | 命中生成特效 | 卡 |

**回读 shooter 记录的只有 5 个**：init3（`+0x14`、`+0x4c`）、tick2（`+0x14`、`+0x20`）、hit2（`+0x26`）、hit3 / hit4（`+0x02`）。
其余 13 个只操作弹对象。**读 `PLAYER+0x479cc` 的只有 init5。**

### 5.2 ★ flags 段在 TH18 **有一个读取点**

`init3` `0x460b60` 读 `shooter+0x4c`（float）当「横移倾斜系数」。这是 TH16 里
「0x38 起整段无人读」（[`../th16/05`](../th16/05-th16-flags-no-runtime-read.md)）的负结论**在 TH18 不成立**的唯一反例——
零售数据里也只有 `+0x4c` 非零（[`02`](02-shooter-record.md) §1），两侧吻合。其余 `+0x3c..+0x5b` 仍未见读取（🟡：本篇只穷举了
20 个 sht 基址消费者 + 18 个表项，未做 TH16 那样的全指令扫描）。

`+0x26`（sht-webedit 的 `new_th18_int32`）也是活的：hit2 把它当 **激光命中特效的 anm 脚本号**。

### 5.3 ★ 零售怎么用它：魔理沙「ミニ八卦炉」= 卡 10 = `pl0X.sht` set `0xb` ✅

`dat/ability.txt` 的 `@MARISA_OP` = **ミニ八卦炉**（「レーザーを撃つ霧雨魔理沙の能力」，
[`../../card/th18/08-catalog.md`](../../card/th18/08-catalog.md) §D 的 id 10）。四个角色的
`pl0X.sht` set **0xb** 逐字段相同（一手逐字节）：

| 字段 | 值 | 说明 |
| --- | --- | --- |
| `+0x00` rate / `+0x01` delay | **5 / 0** | 每 5 帧「发」一发（激光是长驻的，见下）|
| `+0x02` damage | 13 | |
| `+0x14` angle | **−π/2** | 竖直向上 |
| `+0x18` speed | **0.0** | 激光不飞 —— tick2 每帧把位置钉回子机 |
| `+0x20` opt / `+0x21` mode | `0x00` / **2** | mode 2 = 子机火力分配（激光分支）|
| `+0x22` anm | **3** | **`ability.anm` script3 = 激光束** |
| `+0x24` sfx | −1 | |
| `+0x26` hit_fx | **5** | `ability.anm` script5 = 命中特效（⚠️ 不是本文早前写的 8）|
| `+0x2c`/`+0x30`/`+0x38` func | **2 / 2 / 2** | init2 + tick2 + hit2 |

`ability.anm` 里的两支脚本（一手 `thanm spec`）：

```
script3 (束)                       script5 (命中特效，由 hit2 每 2 帧在束端起)
  scriptNew(script4)                 layer(15); sprite(sprite10);
  layer(15); sprite(sprite8);        alpha(192); alphaTime(20,0,0); blendMode(1);
  type(1); anchor(1, 0);             anchor(1,0);
  scale(1.0, 0.0);                   color(255,32,255); colorTime(20,0,32,32,255);
  scaleTime(4,1, 1.0, 0.5);          scale(1,1); scaleTime(20,1, 0.0,0.0);
  alpha(160); blendMode(1);        +20: delete()
  rotateAuto(1); fset(%F0,%RANDF);
  scrollX(%F0); +1: scrollX(-0.04761905); stop()
  interrupt 2 → colorTime(8,0,64,64,255)   （被挡 / 充能中的蓝调）
  interrupt 3 → colorTime(2,0,255,255,255)
  interrupt 1 → scaleTime(8,1,1.0,0.0) → delete
```

- `sprite8` = `player/option/op00.png` **(0,16) 512×16** —— 就是那条激光贴图；
  `sprite10` = (32,32) 32×32 星形迸溅；`script4` = 束根那颗自转脉动的光斑（`sprite9`）。
- **整条链**：`Player__tick_shooters_for_ability_card` 按 `fire_rate` 发一颗 `mode=2` 的「弹」
  → `init2` 把它变成激光（长度 0、伤害源线型）→ `tick2` 每帧钉在子机上、长度 `+18` 到 512、
  按长度拉伸 `script3` 的贴图、按 `sh+0x20` 槽位与火力档开关 → `hit2` 求交截断 + 束端特效。
- ⇒ **激光不是「弹」，是一条挂在子机上每帧重画的束**；`fire_rate` 只是它的「重新起束」节奏。
- TH16 的同款（`pl0Xsub.sht` set 1..6，[`../th16/01`](../../season/th16/01-season-data-and-tables.md) §5.6）
  除 `hit_fx = 0` 外完全同构，脚本在 `pl0Xsub.anm`（script 0/3/6/9/12/15 按档变长）。
- **给 card-expand 的结论**：冬的激光照这套填就行 —— `mode=2` + `func 2/2/2` + `anm=<束脚本>`
  + `hit_fx=<命中脚本>`，束脚本放 `ability.anm` 里（装备卡子机的 `+0x22` 就是 ability.anm 的脚本号）。
  之前「激光损坏内存」不是这条路的锅，是参数没照零售填（`mode=0` / 无 `hit_fx`）。

### 5.4 激光的**寿命只跟射击键走**，不跟子机（一手，2026-09-10）✅

`tick2` `0x45fc80` 尾部（`0x4600xx` 段）才是束的生死判据：

```c
if (bullet+0x8c /* 状态 */ == 1) {                       // 活动态才判
    if (shoot_key_short_timer.current >= 0                // ★ 只要射击键还按着
        && player->state != 2 && player->state != 4       // 不在死亡/决死态
        && (player->field_470d8[0xa4] & 0x40) == 0        // 不在过场
        && (GUI->msg == 0) && ENEMY_MANAGER_PTR != 0) {
        if (shooter[+0x20] /* opt 槽位 */ == 0) goto alive;   // 本体弹：一直活
        uVar9 = shooter[+0x20];
        if ((uVar9 - 1) < player->field_470d8                 // 子机还在 → 继续
            && ...) goto alive;
    }
    /* —— 否则收束 —— */
    *(uint *)ds_active &= ~1;                 // 清伤害源
    bullet+0x8c = 2;                          // 状态 = 完成
    AnmManager__interrupt_tree(bullet+8, 1);  // 束 VM（bullet+8）发 interrupt 1 → 脚本自己淡出 delete
    player->field_470d8[opt] = 0;             // 清这个槽的发射额度
}
```

- **子机被收走（卡主动 `delete` VM）时这条链不会触发** —— 判据里只有「射击键 + 自机状态 + 玩家自己的
  option 槽额度」，**没有「装备卡子机是否还在」**。装备卡的子机由 `Player__allocate_option`
  （`0x40a790`，12 槽公共池）管，池里的记录与 `bullet+0xe4` 是唯一的联系。
- 弹与子机的联系：`PlayerBullet__create` `0x45e3ae` 把第 5 个栈参（子机指针）写进 **`bullet+0xe4`**，
  `tick2` 每帧用它取子机定点坐标（`option+0x5c/+0x60 × 1/128`）钉住束根。
- 弹池（`Player__tick_bullets` `0x45edb0` 一手）：`this = PLAYER_PTR`，池基址 **`PLAYER+0x1570`**、
  **512 槽 × `0xf8`**（`LEA EDI,[ECX+0x1570]`，循环尾 `ADD EDI,0xf8`；循环里 `EDI = 槽+0x14`，
  所以 `[EDI+0x78]` = 槽+0x8c = 状态）。束 VM 被删后这一帧 `get_vm_with_id(bullet+8)` 返回 0
  → `slot+0x8c = 0` 回收槽位。
- **给 mod 的结论**：装备卡收子机时必须**自己把该子机的激光收掉**
  （`sdk.c` 的 `ce_option_kill_bullets`：扫池、`bullet+0xe4 == 该子机` 的按上面那条序列收），
  否则玩家按住射击键会看到一条钉在旧位置的残束（用户 2026-09-10 实跑）。

## 6. 与 TH16 表的对照（[`../th16/03`](../th16/03-th16-funcstar-jumptables.md) §6.4）

| 索引 | TH16 | TH18 | 同？ |
| --- | --- | --- | --- |
| init1 / tick1 / hit1 | 清目标 / 寻的（59 帧、转速常量）/ 命中散射 ±20° + 染色 | 清目标 / 寻的（60 帧、0.08、半径 256）/ 同 | ✅ 同族，常量微调 |
| init2 / tick2 / hit2 | 激光 init / 激光（18→512，低速竖直）/ 激光命中截断 | 同 | ✅ |
| init3 | 清 flag bit2–5 + 清 +0x90 | **咲夜定向**（读 +0x4c） | ❌ 不同 |
| tick3 | 匀加速 +1.0 | 匀加速 +0.3 | ✅ 同族 |
| hit3 / hit4 | 伤害源子体（子速 0.3 / 2.0）| 同（半径 10 / 20）| ✅ |
| init4 / tick4 | 置池对象 +0xd110=2 / 锁敌水平冲刺 14.0 | 清子状态 / 锁敌水平冲刺 14.0 | tick ✅，init ❌ |
| init5 | 发射角 ±15° 抖动 | 读卡瞄准角 | ❌ |
| tick5 | 匀加速 +0.5 | 左右墙反弹 | ❌ |
| hit5 / hit6 | 散射 ±20°（无染色 / 缩放 3→1）| 同（缩放 1→3）| ✅ |
| init6 / init7 | 无 | 随机上半角 / 径向 ±2° 速 16 | TH18 新增 |

→ **编号在两作间只有 1/2 两个「行为包」和 hit 表稳定**，init/tick 的 3/4/5 语义各作自定——wiki 不能给一张跨作的行为索引表，只能逐作列。

## 证据指针

`Player__tick_shooting_state` `0x45ea00` · `Player__set_shoot_key_short_timer` `0x45a550` · `FUN_00452bf0` `0x452bf0`（Timer 减） ·
`Player__initialize` `0x45a7a0` · `Player__reset` `0x45abb0` · `Player__repopulate_options_and_notify_cards` `0x45d5e0` · 三角表 `0x4cdb5c` ·
子机 anm 脚本表 `0x4b7020` / `0x4b7030` · `FUN_0045edb0`（tick_bullets）· 表 `0x4b41f0` / `0x4b4210` / `0x4b4230` / `0x4cf414` ·
样本统计 `scratchpad/shtstat18.py`（`local/th18.v1.00a/dat/pl00–03.sht`）。
