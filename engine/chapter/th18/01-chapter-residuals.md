# th18 章节系统残骸 —— ECL 524 还在，结算没了

> **版本**：TH18 v1.00a（`th18.exe`，md5 `9969cac756098c1da05a81de45437a70`，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th15:0x…`。
> **方法**：一手反汇编 `local/_scratch/th18_full.asm` + pyghidra 反编译 + ExpHP `th-re-data`；
> ECL 侧用 `thecl -d 18` 解原生 `st*.ecl`。
> **可信度**：524 链路与脚本用法 ✅；「没有结算端」🟡（是「没找到」，不是「证明不存在」）。

## 0. 一句话结论

TH18 **仍然保留 TH15 的 ECL opcode 524 `setChapter`**，而且**原版脚本还在调它**——
但引擎只剩「记下章号」这一半：没有 TH15 的延迟标志位、没有每章结算、没有章节奖。
配套的 `GRAZE_IN_CHAPTER` 也只写不读。**这套东西在 TH18 是死代码，正是 TH15 章节系统被阉割后的残骸。**

## 1. opcode 524 的链路 ✅

| 环节 | 地址 | 一手证据 |
| --- | --- | --- |
| opcode ≥ 300 分派器 | `0x430d30` | `movzx edx,WORD [eax+4]`；`add eax,-300`；`movzx eax,BYTE [eax+0x4366f4]`；`jmp [eax*4+0x436438]` |
| 524 的 case | `0x435b23` | 一级表 `0x4366f4`[524−300]=69 → 二级表 `0x436438`[69] |
| case 本体 | `0x435b23`–`0x435b4a` | `ecl_get_int_arg(0)` → `call 0x439e80` → `EnemyData+0x55f8 = n`（`own_chapter`）、`+0x55fc = 0` |
| `set_chapter` 等价物 | `0x439e80` | 全长 0x16 字节，只有两条写（下） |

```
439e80: mov eax, ds:0x4cf2e4        ; GAME_THREAD_PTR
439e88: mov ecx, [ebp+0x8]          ; n
439e8b: mov ds:0x4ccce4, ecx        ; GLOBALS.inner + 0x24 = CURRENT_CHAPTER
439e91: mov [eax+0x24], ecx         ; GameThread + 0x24
439e95: ret 0x4
```

**与 TH15 的差别**（[`th15/01 §1`](../th15/01-chapter-and-bonus.md)）：

| 项 | TH15 | TH18 |
| --- | --- | --- |
| 章号写在哪 | `GameThread+0xb0` | `GLOBALS.inner+0x24`（`0x4ccce4`）**和** `GameThread+0x24` |
| 延迟标志位 | `GameThread+0x90 \|= 0x8000`，下一帧 `th15:0x43cc50` 消费 | **无**（`0x439e80` 只有两条写） |
| 每章结算 | `th15:0x43ab70`（章节奖） | **没有找到** 🟡 |
| 每章计数器清零 | `th15:0x43dd30` | 无（`GRAZE_IN_CHAPTER` 只写不读，见 §2） |

> TH18 的 `zGameThread` 字段图**未反**（ExpHP 数据里没有这个结构体）——
> 「`+0xb0` 是否存在」❓ 未证。别拿 TH17 的布局当 TH18 的结论。

## 2. `GRAZE_IN_CHAPTER` 是死字段 ✅

| 全局 | 地址 | 语义 |
| --- | --- | --- |
| `GRAZE` | `0x4ccd0c`（inner+0x30） | 总擦弹数；`Player__do_graze` `0x45e160` +1、封顶 99999999；每 run 由 `GameThread__thread_start` 归零 |
| `GRAZE_IN_CHAPTER` | `0x4ccd10`（inner+0x34） | 同上 +1 —— 但**全 exe 只有 2 处引用，都在 `do_graze` 内部**（一处读来 clamp、一处写） |

→ 引擎 / ECL / HUD **从不读它、也从不归零它**。想按章统计擦弹必须自建快照
（TH15 有 `th15:0x43dd30` 每章清零，TH18 没有）。

## 2b. 每章「刷怪 / 击破 / 道具」计数的槽还在，喂数的代码没了 ✅

TH15 的 `zGlobalsInner` 里 +0x90/+0x94/+0x98 是三个每章计数器
（`__DDC_ITEM_SPAWN_COUNT` / `ENEMIES_SPAWNED_IN_CHAPTER` / `ENEMIES_DESTROYED_IN_CHAPTER`）。
TH18 同偏移（`inner` 基址 `0x4cccdc`）：

| 槽 | TH18 地址 | TH15 里叫什么 | TH18 的现状 |
| --- | --- | --- | --- |
| `+0x90` | `0x4ccd6c` | `__DDC_ITEM_SPAWN_COUNT` | 只有两处 `add`（`0x42f033`、`0x42f435`；后者带 `enemy+0x6824 == CURRENT_CHAPTER` 门），**全 exe 无读者**；被加的 `enemy+0x6828` **只被清零、从不递增** |
| `+0x94` | `0x4ccd70` | `ENEMIES_SPAWNED_IN_CHAPTER` | 全 exe 只有一处 `mov …,0`（`0x4427eb`，每 run 初始化），从不累加、无读者 |
| `+0x98` | `0x4ccd74` | `ENEMIES_DESTROYED_IN_CHAPTER` | 全 exe**零引用** |

→ 这直接解释了为什么复刻章节奖时「击破率」只能自建（`enemy_count_real` 差分近似，
见 `docs/superpowers/specs/2026-09-09-lolk-chapter-design.md` §3.2）：**原生没有在数**。
跨版本台账见 [`_shared/version-residuals.md`](../../_shared/version-residuals.md)。

## 3. 原生脚本还在调 524 ✅

`thecl -d 18` 解 `local/th18.v1.00a/dat/*.ecl`（`ins_524` = opcode 524）：

| 文件 | `ins_524(n)` 的 n | 位置特征 |
| --- | --- | --- |
| `st01bs.ecl` … `st07bs.ecl`（关底 boss） | **43** | `Boss()` 开头一次、`BossEscape()` 一次；`st07bs` 中间还有一次 **62** |
| `st01mbs.ecl` … `st06mbs.ecl`（道中 boss） | **2** | 道中 boss 出现/退场 |
| `st01.ecl` … `st07.ecl`（关卡脚本） | **0 / 22 / 41 / 43 / 81** | 开场 0、道中段 22、41、boss 前 43、81 |

→ **43（`0x2b`）就是 TH15 `GameThread::on_tick` 里那个 `chapter >= 0x2b` 的分界值**，
22（`0x16`）落在 TH15 的 `0x16..0x28` 区间里，41（`0x29`）与 TH18 `FUN_004422c0` 的
`cmp …,0x29` 对得上 —— 章号语义是跨版本沿用下来的，但**粒度只到「整场 boss 战 / 关卡段」**，
切不出「一个非符 + 一张符卡」这一级。

## 4. 对卡牌实现的影响

| 想做的事 | 结论 |
| --- | --- |
| 拿「章节边界」事件 | 可以读 `0x4ccce4` 的跳变，或 hook `0x439e80`（**当场生效、无延迟标志**）|
| 拿「非符 + 符卡 = 一波」的边界 | 原生没有 —— 用符卡位 `SPELLCARD_PTR+0x78 & 1` 的下降沿自建（[`player/th18/02`](../../player/th18/02-graze-and-item-slowdown.md) §3）|
| 拿章节奖 | 引擎不给，全自建（公式照 TH15）|
| 拿每章擦弹 | `GRAZE_IN_CHAPTER` 是死的 —— 自己差分 `0x4ccd0c` |
