# HANDOVER-108 —— 子机随 P 变化的**回收缺口**（108 有一处，未改；111 已修）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）——本文裸地址默认属该版本。
> **谁写的**：110/111/107 这一条线（2026-09-23）。**为什么写**：用户实跑报的 bug 会在**所有「子机数随火力档变化」的卡**
> 上复发，用户点名「**108 写进 handover 里面**」（117 我自查过，结论见 §5；116 查了，本来就是对的，见 §4）。
> ⚠ 本文件**原本只登记、不改 108 的代码**（`youkai.c` 那一批有自己的 owner 在动）。
> **2026-09-25 更新**：108 owner 接手后**已按 §3 改掉**（含 §6 那三条警告），改法与一手依据见 §3 / §6。

## 1. 用户报的现象（逐字，2026-09-23）

> 「另外，经典bug之，魔理沙用C放灵击之后，有一个我们改出来的子机就定在原地了。大概是——放灵击的几帧里面，
>   三个子机都被定在原地，但是只有两个子机一段时间后被重建了，然后留下那个子机没有被回收」

## 2. 根因（已定位，111 上实测复现 → 修好）

**卡侧自建子机的「申请循环」只建不收**：`want = pose->count; for (i = 0; i < want; ++i) ce_option_get(...)`
—— 当 `want` **变小**（灵击扣火力、解放归零、换阵型、换机体）时，`i >= want` 那些槽的 `ptr` 与它们的 ANM VM
**没有任何人去收** ⇒ 那台子机冻在最后被摆的位置上，永远不会再动（也永远不会回到池里）。

为什么这条链会在**灵击**上暴露：110 的 C 灵击 → `bomb10.c` 扣火力后调 `ce_repopulate_options()`
（`sdk.h` 的 `ce_repopulate_options()` = `Player__repopulate_options_and_notify_cards` `th18:0x45d5e0`）
⇒ 档数 3 → 2 ⇒ 子机卡下次 tick 只申请 2 台 ⇒ 第 3 台被留下。实跑日志的形状（本机 `game/th18_card_expand.log`）：

```
signal: option allocated (ptr 1846cdc8, anm id 64897fc4)   ← 三台一起
signal: option allocated (ptr 1846ceb8, anm id 6489ffc9)
signal: option allocated (ptr 1846cfa8, anm id 648a7fdc)
…（灵击扣火力）…
signal: option allocated (ptr 1846cdc8, anm id 66077932)   ← 只剩两台被重建
signal: option allocated (ptr 1846ceb8, anm id 6607f963)
```
⇒ `cfa8` 那台（第 3 台）就是用户看到的「定在原地、没人回收」。

**一手对位**：引擎自己的 `repopulate` 是「**按当前档重建 + 清掉多出来的**」成对做的
（`th18:0x45d5e0` 一族；我们的 `ce_repopulate_options()` 只借了「重建 + 广播 `on_power_level_change`」那半）
⇒ 卡侧自己申请子机时，**必须自己补齐「收掉多出来的」那半**。

**回收必须用 `ce_option_drop()`**（`native/sdk.c`）：它删 VM（按**该槽自己**的 id `option+0xb0`，不是 `card+0x1c`）
**并且真的松开池槽**（`option+0x00 = 0`）—— 只把 `o->ptr = 0` 是**漏槽**（12 格用完后新子机再也申请不到，
日志会现 `option allocated FAILED (pool full)`；这条在 `ce_option_drop()` 的注释里有一手出处）。

## 3. 108 的落点 —— ✅ **已改（2026-09-25，108 owner 接手后第一件事）**

`mods/th18.v1.00a/card-expand/native/cards/youkai.c` 的 `on_option_pose_apply()`：

```c
    want = pose->count;
    if (want > YK_OPTION_MAX) want = YK_OPTION_MAX;
    /* ★★ 补齐「收掉多出来的」这半（照 116/111 的形状）*/
    for (i = want; i < YK_OPTION_MAX; ++i) {
        if (!s->opt[i].ptr) { s->opt[i].retry = 0; continue; }
        ce_option_drop(c, &s->opt[i]);
        s->opt[i].ptr = 0;
        s->opt[i].retry = 0;
        s->opt_pos[i][0] = s->opt_pos[i][1] = 0.0f;   /* 弹簧状态也清：下次按「新槽」从自机位置起步 */
        s->opt_vel[i][0] = s->opt_vel[i][1] = 0.0f;
        ce_log("youkai: option #%d reclaimed (only %d wanted now; frame %u) …", i, want, ce_sdk_frame());
    }
    for (i = 0; i < want; ++i) { … 原来的申请循环不动 … }
```

**落地细节（与本文档原建议的两处差异，都是有意的）**：

1. **只补一条路就够**：本文原建议「照 116 那样『档位变化时收 + 每帧兜底再收一次』两条路」。108 不需要第二条 ——
   `on_option_pose_apply()` 本身**被声明层每帧调一次**（当选才调，退位走 `yk_option_release`）⇒ 档数一掉，
   下一帧就收干净。116 需要兜底是因为它挂在 `on_power_level_change` 那种**边沿**回调上。
2. **顺带修一个必然的次生现象**：收掉再重建的那一格，弹簧位置还是「冻住时的旧坐标」⇒ 新子机会从旧位置飞出来。
   加了 `was_held`（申请循环里判「这一格本帧是不是新申请」）⇒ 新格从**自机位置**起步（TH08 state 1，与
   卡级 `opt_spring_live` 那条同义）。这一条是**同一处**的收尾，不是新机制。

**一手对位（2026-09-25 本人在 `th18.exe` 上逐指令读，比本文 §2 那句更细）**：引擎自己就是成对做 ——
`Player__repopulate_options_and_notify_cards` `th18:0x45d5e0` 里 ① 恒扫 4 格拆 VM（`0x45d818` / `0x45d81e mov edi,4`）；
② `0x45dbe0 mov edi,4` + `0x45dbeb sub edi,edx`（`edx` = 档数）⇒ **`4 − lvl` 次**循环把多余格的 `+0xb0` VM 删掉、
`+0x00` 状态清零；③ 重建只建 `i < lvl` 格。回流见
[`engine/sht/th18/03-func-tables-timers-and-dead-fields.md`](../../../engine/sht/th18/03-func-tables-timers-and-dead-fields.md) §3。

**验收判据**（与本文 §7 同，实跑时看日志）：
`youkai: option #N reclaimed (only M wanted now; frame F)` 应只在**档数下降**那一帧各出现一次；
`option allocated` 的台数应恒等于当前档；反复扣/加火力 10 次，`option allocated FAILED (pool full)` **永不出现**；
场上目视：子机数 = 档数，没有冻在旧位置的那一台。

## 4. 参考实现（两条都在本仓，已实跑/已合入）

| 卡 | 位置 | 形状 |
| --- | --- | --- |
| **116**（本来就是对的 ✅）| `cards/season.c` 的 `alloc_options()`（`:435` 的 `else if (s->opt[i].ptr) ce_option_drop(c, &s->opt[i]);`）与每帧兜底（`:1351`，注释：「档位掉下来但 `on_power_level_change` / 释放路径没跑到时，每帧也把多余的收掉」）| **两个路径都收**：档位变化时收 + 每帧兜底再收一次 |
| **111**（本次修的 ✅）| `cards/signal.c` 的 `on_option_pose_apply()`（`want` 计算之后、申请循环之前新增同一个 else 分支）| 同 116；`SIGNAL_OPTION_MAX = 4` |

## 5. 117 自查结论（用户点名「117 自查一下」）——**未发现同类缺口**

逐条查过，117 **不申请原生子机**，所以这条链的入口根本不存在；它的「随 P 变化」是**别的形态**：

| 117 的东西 | 随 P 变化？ | 收尾形状 | 结论 |
| --- | --- | --- | --- |
| `on_shot_set_index(power_level)`（`:2421`）| ✅ 随档换**主炮组号** | 只是一个整数表索引，没有 VM | ✅ 无泄漏面 |
| 水獭 3 光球 `s->orb[3]`（`:582`，脚本 149）| ❌ 随「咆哮中 / 兽灵形态」 | `orb_kill()` 对 **3 个索引全扫**（`ce_anm_delete` / `interrupt(1)` + 清句柄，`:2949`–`:2953`）；`orb_spawn` 只补缺的那些（`:2964`）| ✅ 全有全无，**不会**留下落单的一台 |
| 兽灵灵珠池 `s_tok[40]`（`:474`）| ❌ 一颗一条命 | 逐颗生命周期（`token_*`），没有「档数下降」这回事 | ✅ |
| `ce_option_*` 全族 | — | 全文件**零调用** | ✅ |

⇒ 117 不需要动。若以后 117 改成「按档挂子机」，**必须**照 §4 的形状写。

## 6. 顺带登记（108 的文件，本轮**没改**）—— ✅ **三条都已在 2026-09-25 清掉**

`make` 重编 `youkai.c` 时曾出 3 条警告（不影响功能，但仓库的「0 警告」卫生要求它们被处理）：

- `cards/youkai.c:5` `warning: '/*' within block comment`（文件头注释里嵌了 `/*` ⇒ 注释提前闭合，检查那段的说明是否还在注释里）；
- `cards/youkai.c:2940` `unused function 'on_native_shot_suppress'`（要么挂上、要么标 `unused` 属性）；
- `cards/youkai.c:1670` `variable 's_popup_dropped' set but not used`（诊断计数没被读 ⇒ 要么打进日志、要么删）。

**处置（2026-09-25，逐条）**：

| 条 | 怎么修的 | 在哪儿 |
| --- | --- | --- |
| `/*` 嵌注释 | 第 5 行的 `` `src/*.cpp` `` 就是那个 `/*` ⇒ 改写成「`` `src/` 下的 `.cpp` 源码 ``」（语义不变，注释不再是提前闭合的）| `youkai.c` 文件头 |
| `unused on_native_shot_suppress` | 保留为**历史参考**并显式标 `__attribute__((unused))`（照 `cards/trance_bgm.c:234` 的先例）；函数头注里写明「**不要再挂回去**：2026-09-22 P7 之后妖侧主炮靠 set 认领」| `youkai.c` 该函数前的声明 |
| `s_popup_dropped` | 改成**首次 + 每 60 次**各打一行 `youkai: popup pool full, dropped N popup(s) so far (YK_POPUP_MAX 3)` —— 池满 = 「弹字被吃掉」的唯一可观测信号，据此决定要不要抬那个 ❓ 自定上限 | `youkai.c` 的 `yk_popup_spawn()` |

⇒ 现在 `make cards/youkai.o` **零警告**（`gcc -O2 -Wall -Wextra`，无输出）。

## 7. 验证判据（108 owner 改完照这个看）

1. 装 108 + 110，用魔理沙按 C 放灵击（或把火力从 3 档扣到 2 档）；
2. 日志里 `option allocated` 的**台数应等于当前档**，且**不再有落单的那一台**；
3. 场上目视：子机数 = 档数，没有冻在旧位置的子机；
4. 反复扣/加火力 10 次，`option allocated FAILED (pool full)` 应**永不出现**（出现即漏槽）。
