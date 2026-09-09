# th18 波次 / 相位信号 —— 引擎自己就在判「击破」还是「超时」

> **版本**：TH18 v1.00a（`th18.exe`，md5 `9969cac756098c1da05a81de45437a70`，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th15:0x…`。
> **方法**：Ghidra 12.1.3 反编译（`0x42eff4` / `0x430240` / `0x42a320` / `0x42a780` / `0x42d640…`）
> + `th-re-data` 结构体（`zEnemyData` / `zEnemyLife` / `zEnemyInterrupt`）+ 零售 ECL（`thecl -d 18`）。
> **可信度**：三个信号（符卡位 / 相位槽 / 超时持续位）与它们的读写点 ✅ 一手；
> 「用相位槽指纹当波边界」是**本仓的用法**，不是引擎语义 🟡。

## 0. 一句话结论

TH18 的章节奖被阉割了（[`01-chapter-residuals.md`](01-chapter-residuals.md)），
但**判定相位成败的那套机器还在**，而且比 TH15 的计数器更直接：

- 敌机身上有 8 个**中断槽** `{hp_value, time, sub_life, sub_timeout}`，
  引擎每帧拿 `life.current` 与 `slot.time` 跟它们比 → **`life.current <= hp_value` = 击破**、
  **`slot.time <= time_in_ecl` = 超时**，两条路分别跳到不同子程序；
- 超时那条路顺手把 `zEnemyData.flags_lo` **bit24** 置 1（击破那条路清 0），
  符卡超时还会把 `zSpellcard+0x78` **bit7** 置 1；
- 符卡是不是**耐久符卡（时符）**由 ECL 542 打一个位（`zSpellcard+0x78` **bit3**），
  引擎对耐久符卡的超时**不置 bit7、不清奖励位** —— 也就是「超时按收卡算」。

复刻章节奖时不必自己猜击破率：读这三个位就够了。

## 1. 符卡状态 `zSpellcard+0x78`（`0x4cf2c0` 指向）✅

| 位 | 含义 | 置 / 清 |
| --- | --- | --- |
| 0x01 | 符卡进行中 | 置：`0x42a320`（符卡开始，`& 0xffffff67 \| 3` = 清 bit3/4/7、置 bit0/1）；清：`0x42a780`（`spellEnd`，`& 0xfffffffe`）|
| 0x02 | 奖励存活（= 收卡判定） | 置：同上；清：超时路（`0x42eff4` 段，`& 0xffffffdd`）、玩家中弹 / 放 B 的路 |
| 0x08 | **耐久符卡（时符）** | 置：ECL **542** → `0x42d650`（`\|= 8`）；清：下一次符卡开始 `0x42a320` |
| 0x80 | **已超时** | 置：超时路 `0x42eff4`；清：下一次符卡开始 `0x42a320` |

- `0x42a320`（符卡开始，`this` = zSpellcard）里那一句是
  `flags = (flags & 0xffffff67) | 3` —— **bit3 / bit4 / bit7 一律清掉**，
  所以「时符」标记只对**当前这张**符卡有效；
- `0x42a780`（`spellEnd`，ECL 523）用 **bit1** 决定发不发奖励：
  `(flags & 2) == 0` → 销毁奖励 VM；否则加分 + 记 spell history + 播 se 0x2e；
  `(flags & 0x80)` → 另播 se 0x45（超时音）；
- 超时路对**非耐久符卡**还会把 `gui+0x7c`（符卡奖励值）清零并清 bit1 —— 即「超时 = 没收到」。

> 对复刻的意义：**符卡波的击破率** = `bit3`（时符）→ 100％；
> 否则 `bit7`（超时）→ 0％，没超时 → 100％。bit1 更严（中弹也清），
> 但用户 2026-09-09 的口径是「没击破、时间到了 = 0」，所以取 bit7。

## 2. 相位槽与两个判定 ✅

`zEnemyData.interrupts` 在 `zEnemy+0x6374`（`zEnemyData+0x5148`），
`zEnemyInterrupt[8]`，stride `0x88`：

| 偏移 | 字段 | 说明 |
| --- | --- | --- |
| `+0x00` | `hp_value` | **绝对** HP 阈值；`-1` = 槽空 |
| `+0x04` | `time` | 计时阈值（帧）|
| `+0x08` | `sub_life[0x40]` | HP 打空时跳的子程序名（`ins_514` 的第 4 个参数）|
| `+0x48` | `sub_timeout[0x40]` | 超时跳的子程序名（ECL `521 setTimeout` 写）|

**活动槽 = 第一个 `hp_value > -1 && time > 0` 的槽**（引擎在 `EnemyData__step_game_logic` 里扫两遍：击破分支与超时分支各一遍，顺序相同）。

击破判定（`EnemyData__step_game_logic` 的 `0x42ed40` 段，伤害结算完）：

```c
iVar3 = enemy->life.current;                 /* zEnemy+0x6220 */
if (iVar3 <= slot->hp_value) {               /* 越过阈值 = 这一相位被击破 */
    enemy->life.current = slot->hp_value;    /* 夹到阈值 */
    slot->hp_value = -1;                     /* 清槽 */
    enemy->flags_lo &= ~0x1000000;           /* ★ 清「超时」位 */
    jump_sub(slot->sub_life);                /* 跳下一相位 */
}
```

超时判定（同一个函数 `EnemyData__step_game_logic` 的 `0x42eff4` 段，每帧一遍）：

```c
if (slot->hp_value > -1 && slot->time > 0 && slot->time <= enemy->time_in_ecl) {
    enemy->life.current = slot->hp_value;
    slot->hp_value = -1;
    enemy->flags_lo |= 0x1000000;            /* ★ 置「超时」位 */
    if ((SPELLCARD.flags & 8) == 0) {        /* 非耐久符卡才叫超时 */
        SPELLCARD.flags |= 0x80;             /* ★ 符卡超时位 */
        if (SPELLCARD.flags & 1) { gui->spell_bonus = 0; SPELLCARD.flags &= ~0x22; }
    }
    jump_sub(slot->sub_timeout);
}
```

关键地址：

| 东西 | 地址 |
| --- | --- |
| `life`（`zEnemyLife`）| `zEnemy+0x6220`（`current` / `+4 maximum` / `+8 remaining_for_cur_attack` / `+0x18 is_spell`）|
| `time_in_ecl.cur` | `zEnemy+0x14ec`（`zEnemyData+0x2c0`）|
| `flags_lo` | `zEnemy+0x635c`（`zEnemyData+0x5130`），bit24 = 相位超时 |
| 中断槽数组 | `zEnemy+0x6374` |
| 伤害 / 击破判定 | `0x42ed40` 段（在 `EnemyData__step_game_logic` 内）、`0x430240`（死亡处理：音效 + 死亡特效 + 掉道具 + `on_death_callback`）|
| 超时判定 | `0x42eff4` 段（同函数）|
| 符卡开始 / 结束 | `0x42a320` / `0x42a780` |
| 耐久符卡置位 | `0x42d650`（ECL 542）|

## 3. 相位指纹（本仓用法 🟡）

槽被清空的那一帧 `hp_value` 变 `-1`，下一相位登记新槽时 `(hp_value, time, sub_life)` 又变一次。
把「活动槽的 `(index, hp_value, time, sub_life 前 4 字节)`」当**相位指纹**：

- **只在有活动槽时更新**指纹 → 「清空 → 新槽」这条链只报一次，且报到的是新相位；
- 指纹变了 = 相位换了 = 波边界。

这是本仓的接线手法，不是引擎语义（引擎没有「波」这个概念，只有槽）。
零售 boss 的实测形态（`st01bs.ecl`）：`Boss1`（非符）→ `BossCard1`（符卡）→
`Boss2`（非符）→ `BossCard2`（符卡）→ `BossDead`，每个相位各 `ins_514` 一次，
所以指纹 + 符卡位两套信号都能切出「一个非符一波、一张符卡一波」。

## 4. 道中：ECL 524 切**不出来** ✅（2026-09-10 全量复核）

`ins_524(n)` → `0x439e80` → `GLOBALS.inner+0x24`（`0x4ccce4`）+ `GameThread+0x24`。
把 8 关 `st*.ecl` / `st*bs.ecl` / `st*mbs.ecl` 全量解出来看，524 只出现在：

- 关卡脚本 `main` 的开场 `0`、进关底 boss 前 `41`；
- `MainBoss` 的 `43`（boss 入场）与 `81`（`bossWait()` 后）；
- `MainBossDebug` / `MainBossDebug2` / `MainMBossDebug` / `MainBossTest`（**debug 子程序**）`41`/`22`/`43`/`81`；
- `st0Xbs.ecl` 的 `Boss`/`BossEscape` `43`、st07 `BossCard1` `62`；
- `st0Xmbs.ecl` 的 `MBossDead`/`MBossEscape` `2`。

**`MainFront` / `MainLatter` / `MainSubNN` 里一个 524 都没有** —— 道中切不出来。

> ⚠️ 本节 2026-09-09 的旧口径（「道中段 `0 / 22 / 41 / 81`，轮询 `0x4ccce4` 跳变即可当道中波边界」）
> **已证伪**：`22` 是 `MainMBossDebug`、`81` 是 boss 对话、`41` 是 boss 入场，没有一个是道中敌人组；
> 而且 TH18 的 `0x439e80` 没有 TH15 的延迟标志位，章号会重复（TH15 st05 就重复过 `7`），
> 轮询数值本来就会漏。道中的等价信号见 [`03-midstage-waves.md`](03-midstage-waves.md)：
> **关卡脚本 ECL 子程序号 + 刷出**。

## 5. 与 TH15 的对照

| 想要的东西 | TH15 | TH18 |
| --- | --- | --- |
| 章边界 | ECL 524 + 延迟标志位 `th15:0x43cc50` | ECL 524 只写章号；**无结算** |
| 刷出 / 击破计数 | `ENEMIES_SPAWNED/DESTROYED_IN_CHAPTER`（`th15:0x42b6bd` 等 ECL 侧累加，`th15:0x428b70` 死亡时按 `own_chapter` 累加）| 槽还在（`0x4ccd70/0x4ccd74`）**没人写** → 只能自己差分 `enemy_count_real` |
| 「击破」的判据 | 敌机死亡函数被调用 | **中断槽 `life.current <= hp_value`**（更直接，且区分超时）|
| 时符 | 无显式标记 | `SPELLCARD+0x78` bit3（ECL 542）|

## 6. 复刻口径（用户 2026-09-09 定案 → 落在 `lolk_core.c`）

| 波种 | 击破率 |
| --- | --- |
| 道中章 | 击破数 / 本章刷出总数；分母 0 → 100％ |
| boss 非符 | 见过击破条件 → 100％；只见过超时 → 0％ |
| boss 符卡 | 同上，超时另有 `SPELLCARD` bit7 |
| 时符（bit3） | 恒 100％ |

> 「见过」= 本波开着期间采样到一次即可（相位本地条件只在收尾那帧成立）。
> 边界帧也要采样（击破 / 超时就发生在切换那一帧），但**符卡位属于新波**。
