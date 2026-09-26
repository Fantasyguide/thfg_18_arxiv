# TH18 boss 攻击段（中断槽）与符卡对象：超时 / 击破怎么判、mod 怎么「跳过一张符卡」

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：一手反编译 `EnemyData__step_game_logic` `0x42ED40`、`EnemyManager__get_boss_enemy_full` `0x4237F0`、
> ECL 指令 523 / 542 的 case（`FUN_00430d30` 内 `0x434FEC` / `0x435067`）、符卡开始 `0x42A320` / 结束 `0x42A780`；
> 结构体偏移对 ExpHP `zEnemy` / `zEnemyData` / `zEnemyInterrupt` / `zEnemyManager`（`local/vendor/th-re-data`），三处交叉核对。
> **可信度**：机制 ✅；标 🟡 处逐条注明。起因：卡牌扩展 mod 的「神之宣告」（`mods/th18.v1.00a/card-expand`，AUDIT O28）。

## 0. 一句话结论

boss 的每段攻击是敌人对象里的一个**中断槽** `{hp_value, time, sub_life, sub_timeout}`：血量掉到 `hp_value` 走 `sub_life`，
`time_in_ecl` 计时到 `time` 走 `sub_timeout`。符卡只是这段攻击上叠的一个全局 `Spellcard` 对象（`0x4cf2c0`），
自己不判胜负——**胜负由中断槽决定**：击破 = 血量路径，超时 = 计时路径。要「跳过」一张符卡，把计时写到阈值即可，
引擎在同一帧的 EnemyManager tick 里自己按超时收场。

## 1. 对象与偏移（ExpHP 命名 + 一手核对）

| 对象 | 字段 | 语义 |
| --- | --- | --- |
| `zEnemy` | `+0x122c` | `zEnemyData` 内嵌于此（`enemy+0x6374` = `data.interrupts`、`enemy+0x14ec` = `data.time_in_ecl.cur`、`enemy+0x6220` = `data.life`，`0x42ED40` 三处相减一致）|
| `zEnemy` | `+0x6830` | `enemy_id` |
| `zEnemyData` | `+0x2bc` | `time_in_ecl`：zTimer `{prev, cur, cur_f, …}` |
| `zEnemyData` | `+0x2d0` | `time_alive`：zTimer（`cur` 在 `+0x2d4` = `enemy+0x1500`）。**每帧 +1**（见 §1a）|
| `zEnemyData` | `+0x4ff4` | `life {current, maximum, remaining_for_cur_attack, current_scaled_by_seven, starting_value_for_next_attack, total_damage, is_spell}` |
| `zEnemyData` | `+0x50d4` | 「下一帧附加伤害」：`0x42ED40` 读到就加进本帧伤害并清零（ExpHP `__some_kind_of_extra_damage`）|
| `zEnemyData` | `+0x5148` | `interrupts[8]`，stride `0x88`：`+0 hp_value`（-1 = 空槽）、`+4 time`（帧；0 = 无超时）、`+8 sub_life[0x40]`、`+0x48 sub_timeout[0x40]` |
| `zEnemyManager` | `+0x44` | `can_still_capture_spell` |
| `zEnemyManager` | `+0x48` | `boss_ids[4]`（`enemy_id`；0 = 空）|
| `zEnemyManager` | `+0x18c` | 活动敌人链表头 `{entry, next, prev}` |
| `zSpellcard` | `+0x24` | **int32：符卡已经经过的帧数** ✅ 用途 =「超时 60 帧内不清奖励」那个门（见 §2 ①）|
| `zSpellcard` | `+0x74` | 符卡 id |
| `zSpellcard` | `+0x78` | 标志：bit0 进行中、bit1 奖励存活、bit3 耐久符卡（ECL 542）、bit4（ECL 另一 case）、bit5 用过炸弹、bit7 已超时、bit8 ECL 开关 |
| `zSpellcard` | `+0x7c` / `+0x80` | 奖励分（`0x42A320` 按难度表 × 关卡号；`+0x80` 封顶 999999999）|

### 1a. `time_alive`（`zEnemyData+0x2d0`）每帧 +1 ✅（一手，2026-09-10）

敌人每帧的入口是 `FUN_0042ff80`（`EnemyManager::on_tick` → `FUN_0042fe80(enemy)` → `FUN_0042ff80(enemy+0x122c)`，
即**它拿到的 `this` 就是 `zEnemyData` 基址**）。它先调 `EnemyData::step_game_logic` `0x42ed40`，
**尾段**再刷四个计时器：

```asm
004301de  CMP dword ptr [ESI+0x50f4],0x0    ; invuln_timer.current > 0 ?
004301e5  JLE 0x004301f3
004301e8  LEA ECX,[ESI+0x50f0] / CALL 0x00409750   ; Timer::decrement(invuln_timer)
004301f3  CMP dword ptr [ESI+0x5108],0x0    ; no_hitbox_timer.current > 0 ?
004301fa  JLE 0x00430208
004301fd  LEA ECX,[ESI+0x5104] / CALL 0x00409750   ; Timer::decrement(no_hitbox_timer)
00430209  LEA ECX,[ESI+0x2d0] / CALL 0x00405990    ; Timer::increment(time_alive)
00430215  LEA ECX,[ESI+0x2bc] / CALL 0x00405990    ; Timer::increment(time_in_ecl)
```

- `[ESI+0x50f0]` / `[ESI+0x5104]` 正是 ExpHP 结构体里的 `invuln_timer` / `no_hitbox_timer`
  ⇒ 后两条 increment 的对象就是 `time_alive` 与 `time_in_ecl`（同一基址、相邻字段）。
- `Timer::increment` `0x405990` 一手：`prev = cur; cur += 1`（`0x4ccbf0` = 游戏速度，非 1 时按浮点累加）
  ⇒ **正常速度下 `time_alive.current` = 该敌人已经活了多少帧**。
- 绝对偏移：`zEnemy` + `0x122c` + `0x2d4` = **`enemy+0x1500`**（`current` 在 zTimer 的 `+0x4`）。
- 用途：TH18 自己没有 `dropSeason`，卡侧复刻 TH16 的掉落计时衰减时拿它当计时器
  （见 [`season/th18/01`](../../season/th18/01-season-residuals.md) §4）。

## 2. 每帧判定（`EnemyData__step_game_logic` `0x42ED40`，一手逐段）

```c
// ① 超时：取第一个 hp_value > -1 && time > 0 的槽 i
if (slot.time <= data.time_in_ecl.cur) {
    data.life.current = slot.hp_value;  slot.hp_value = -1;      // 血量直接落到阈值
    time_in_ecl 归零；enemy+0x635c |= 0x1000000（超时标志）
    if (!(spell.flags & 8)) {                                  // 非耐久符卡
        spell.flags |= 0x80;                                   // 已超时
        if (spell.flags & 1 && spell.elapsed(+0x24) > 0x3b) { spell.bonus = 0; spell.flags &= ~0x22; }
        enemy_mgr.can_still_capture_spell = 0;
    } else if ((spell.flags & 9) == 9) { … 耐久符卡超时 = 收下 … }
    跑 slot.sub_timeout（Enemy__ecl_run）
}
// ② 伤害：附加伤害 +0x50d4 → 本帧伤害；符卡且用过炸弹（flags & 0x21 == 0x21）伤害 /30；写 life
// ③ 击破：血量 <= slot.hp_value 的槽 → life.current = hp_value；跑 slot.sub_life
```

- **结论**：超时与击破都只是「把血量钉到阈值 + 跑对应子程序」，符卡的胜负标志在超时路径里顺手置 ✅。
- **证据**：`0x42EDF5..0x42F0B0`（超时段）、`0x42F1A0..0x42F2C0`（伤害与击破段）。

## 3. 符卡对象的生命周期

| 事件 | 谁 | 做什么 |
| --- | --- | --- |
| 宣言 | ECL 宣言 case（`0x430D30` 内 `0x434F88` 一带，🟡 指令号未核）→ `0x42A320`（`Spellcard__start(this; id, name, bonus, boss_idx)`）| `flags = (flags & ~0x98) \| 3`（进行中 + 奖励存活）、存档计数 +1、起名字 / 背景 / 立绘 VM、奖励 = 难度表 × 关卡 |
| 结束 | ECL 523 case `0x434FEC` | `if (flags & 1 && flags & 2 && card_exists(40 KISERU)) 掉命碎片道具(0x3c)`；然后 `0x42A780`：`flags & 2` ? 计分 + 存档「收下」计数 + 音效 0x2e : 「失败」演出（fronttr script 0x32 / 0x54）；`flags & 0x80` 再放 0x45 |
| 耐久 | ECL 542 `0x435067` → `0x42D650` | `flags \|= 8` |
| 其他 | ECL case `0x435077` → `0x42D670` | `flags \|= 0x10`，杀 `+0x1c` VM |
| 开关 | ECL case `0x435090` → `0x42D610(x)` | 写 bit8 |

- 通过 `SPELLCARD_PTR+0x78 & 1` 判「符卡进行中」（`FUN_00409b10`，Utsuho / Tsukasa / 决死救命卡都这么问）✅。

## 4. mod 怎么「跳过」一张符卡（两条路，都不用新断点）

| 路 | 写什么 | 结果 |
| --- | --- | --- |
| **超时**（神之宣告用）| `data.time_in_ecl.cur = cur_f = slot.time`；再清 `spell.flags & 2`、`spell.bonus = 0`（耐久符卡超时本来算收下）| 同帧 §2 ① 原样跑（Player tick 0x17 写、EnemyManager tick 0x1b 判；Enemy tick 体 `0x42FF80` 里比较先于 `Timer__increment`）：血条落到阈值、`sub_timeout`、失败演出、无奖励、无 Sannyo 碎片。已超时（bit7）而 ECL 未到 523 的窗口要自己挡 |
| **击破** | `data + 0x50d4 = 大数` | 同帧 §2 ② ③：按收符卡算，给奖励 / 碎片 |

boss 找法照 `0x4237F0`：`boss_ids[i]` → 走 `+0x18c` 链表比 `enemy+0x6830`。

## 5. 中断槽的命名约定：怎么认出「最后一张符卡」（2026-09-07，ECL 一手）

起因：卡牌扩展 mod 的「古锭刀」（终符期间伤害翻倍）。方法：`thecl -d 18 -m th18.eclm` 反出 `st01–07bs.ecl` 与 `st01–07mbs.ecl`
（`local/th18.v1.00a/ecl/`），列出每张符卡前的 `setInterrupt(0, 0, time, "子程序")`。

| 文件 | 符卡的中断槽子程序名（文件顺序）|
| --- | --- |
| st01bs | `Boss2` `BossDead` |
| st02bs / st03bs / st04bs | `Boss2` `Boss3` `BossDead` |
| st05bs | `Boss2` `Boss3` `Boss4` `BossDead` |
| st06bs | `Boss2` … `Boss6` `BossDead` |
| st07bs | `Boss2` … `Boss10` `BossDead`（Card9 耐久符 → `Boss10`，Card10 终符 → `BossDead`）|
| st07mbs | `MBoss2` `MBoss3` `MBossDead` |
| st01–06mbs | 无 `spell` / `spellEx`：主线道中 boss 只有计时非符（`setInterrupt(1, 0, t, "MBossDead")`）|

- **结论 ✅（th18 全覆盖）**：boss 当前活动槽（§2 的「第一个 `hp_value > -1 && time > 0`」）的子程序名含 `Dead` ⇔ 这段攻击结束即 boss 死亡 ⇔ 最后一张符卡。
  Extra 道中槽 1 常驻 `MBossDead`（整体计时），但符卡用槽 0、序号在前，不会被误取。
- **不要用 `stars`（ECL 540）**：它只是血条旁的星数显示。主线 boss 严格（`stars(N)` = 本符之后还剩 N 张），但 st07mbs 的 `MBoss2` / `MBoss3` 都写 `stars(0)`、`MBoss1` 不写——按它判，Extra 道中第二张符卡就会被当成终符。
- 🟡 4 参 `setInterrupt`（ECL 514）把名字写进 `sub_life`（`+8`）还是 `sub_timeout`（`+0x48`）未逐行反；终符击破 / 超时都进 `BossDead`，所以两个都是它。mod 两个都查。
- 用法：`mods/th18.v1.00a/card-expand/native/cards/guding_core.c`（`gd_find_active_slot` / `gd_slot_is_final`，主机单测）。

## 6. Follow-up

- 🟡 `0x4cf280` = REPLAY_UNSAFE_RNG、`0x4cf288` = REPLAY_SAFE_RNG（ExpHP 相邻命名规律 + 使用者分布），未登记进 `engine.h`。
- ✅ ~~🟡 `zSpellcard+0x24` 的语义（超时 60 帧内不清奖励的那个门）未追。~~ 2026-09-27 结案：`+0x24` 是
  **int 的经过帧**（`0x42a356` 宣言清零；`0x42efb0` 先把 `[0x4cf2c0]` 装进 `edx`，`0x42efd2` 紧接着
  `cmp DWORD PTR [edx+0x24],0x3c` / `jl`；同段读 `[edx+0x78]`/`[edx+0x7c]` ⇒ `edx` 确为 zSpellcard）——
  **不是** zTimer 结构体基址（若按 zTimer 解，`cur` 该在 `+0x28`，而 `+0x28` 在宣言时也被清零、
  目前只见于「与 `+0x24` 成对」）。`engine.h` 增 `CE_SPELL_ELAPSED`；卡 108 的符卡额度探针读它。
- 🟡 **「限时」存不存在于 zSpellcard 里未定**：`Spellcard__start` `0x42a320` 只写 `+0x20`(-1)/`+0x24`(0)/
  `+0x28`(0)/`+0x74`(id)/`+0x78`(flags)/`+0x7c`+`+0x80`(bonus)/名字；**没看到任何限时字段** ⇒ 限时只有
  中断槽的 `time` 一处。但「收尾那一刻哪个槽属于这张符卡」尚未实跑核对（见卡 108 的探针）。
- ⏳ ECL 522（宣言）case 的参数表与 `boss_idx` 的来源未逐行反。
- ⏳ 中断槽由 ECL `setInterrupt(槽, 血量, 超时, 子)` 写入（`mods/…/assets/ecl/make_dev_ecl.py` 已在改它）；指令号未在本文核对。
