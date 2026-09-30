# TH18 stage 1（1 面）道中出怪 dump —— 给「道中小怪掉动物灵」定规则用

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th08:0x…`。
> 由 dump-st01 生成；复现命令见文末。

### 每段子程序的出怪（`enmCreate*` 调用，按出现顺序）

- **MainBoss**：1 只 —— Boss×1
- **MainBossTest**：1 只 —— Boss×1
- **MainMBossDebug**：1 只 —— MBoss×1
- **MainSub00**：20 只 —— Cir00p×10、Cir00m×10
- **MainSub01**：2 只 —— GirlRedA01×1、GirlBlueA01×1
- **MainSub01b**：2 只 —— GirlRedA01×1、GirlBlueA01×1
- **MainSub01c**：1 只 —— GirlC01×1
- **MainSub02**：8 只 —— GirlB01×8
- **MainSub02b**：4 只 —— GirlRedA01n×2、GirlBlueA01n×2
- **MainSub03**：3 只 —— GirlD01×3
- **MainSub04**：18 只 —— Cir01a×9、Cir02a×9
- **MainSub05**：20 只 —— Cir00bp×10、Cir00bm×10
- **MainSub06**：3 只 —— GirlD02×3
- **MainSub06b**：8 只 —— GirlB01×8
- **main**：2 只 —— MapleEnemy×1、MBoss×1

### 敌人类型总表（stage 1）

- `GirlB01`：16
- `Cir00p`：10
- `Cir00m`：10
- `Cir00bp`：10
- `Cir00bm`：10
- `Cir01a`：9
- `Cir02a`：9
- `GirlD01`：3
- `GirlD02`：3
- `Boss`：2
- `MBoss`：2
- `GirlRedA01`：2
- `GirlBlueA01`：2
- `GirlRedA01n`：2
- `GirlBlueA01n`：2
- `GirlC01`：1
- `MapleEnemy`：1
## 怎么复现这份 dump

```bash
# 全仓库路径；⚠ `-d 18` 必须相邻，其它 flag 放前面
local/vendor/thtk/build/thecl/thecl -j -d 18 -m local/vendor/eclmap/eclmap/th18.eclm \
    local/th18.v1.00a/dat/st01.ecl /tmp/st01.txt
```

## 出怪指令的变体与参数（eclmap 一手）

| opcode | 名 | 语义（一手）|
|---|---|---|
| 300 | `enmCreate` | 坐标 = **本机坐标 + 参数**（相对）|
| 301 | `enmCreateA` | 坐标 = **参数原样**（绝对）⇒ **A = absolute**，不是 async |
| 304 | `enmCreateM` | 相对 + **镜像标记**（M = mirror）|
| 305 | `enmCreateAM` | 绝对 + 镜像标记 |
| 309 | `enmCreateF` | 相对；**额外门**：`boss_ids[0]` 那只敌机还在活动链表里就**不建** |
| 310 | `enmCreateAF` | 绝对 + 同上的 F 门 |
| 311 | `enmCreateMF` | 相对 + 镜像 + F 门 |
| 312 | `enmCreateAMF` | 绝对 + 镜像 + F 门 |

> ✅ **2026-10-01 一手补（原先的 ❓ 与「A = async」口述一并订正）**：**A = 绝对坐标、M = 镜像、F = 「boss 不在场才建」**。
> 证据：分派表 = 字节表 `th18:0x4366f4` + dword 表 `th18:0x436438`（index = **opcode − 300**）——
> 300/301/304/305 全落**同一个** case `th18:0x430df1`，309/310/311/312 全落 `th18:0x430ddb`；
> 后者只多一条门：`push [mgr+0x48]`（= `zEnemyManager.boss_ids[0]`）`call th18:0x42d500`
> （按 id 在 `mgr+0x18c` 活动链表里查敌机，返回非 0 = 还活着）→ `test/jne th18:0x436417` 跳过建敌。
> 坐标/镜像的差别在共用 wrapper `FUN_004369e0`（`th18:0x4369e0`）：`0x436a96/0x436a9b`（300/309）、
> `0x436ab2/0x436ab7`（311/304）走 `th18:0x436ae4` 的 `addss xmm1,[esi+0x44] / addss xmm0,[esi+0x48]`
> （**加本机坐标**），301/310/305/312 直接跳 `th18:0x436b0e`（**不加**）；304/311/305/312 另把镜像参数置 1，
> 且本机 `flags_lo` bit19（`0x80000`，`flagMirror` op558 写它的那一位）置位时 `th18:0x436b1a mulss xmm1,[0x4b9418]`
> 把 x 取反。**「同步/异步」这条没有落点**：300 与 301 共用同一 handler case ⇒ 阻塞与否不可能是这对 opcode 的差别
> （🟡 若确有该语义，落点在别处，未读到）。
> 旁证（第三方移植的实际用法，`engine/enemy/th08/04-mystia-boss-familiars-leveledit-port.md` §3.2）：
> TH08 的 `familiarCreateF("Sub2",0,0,…)` 被翻成 `enmCreate("MystiaChild28",0,0,…)`（相对 ⇒ 落在父敌身上），
> TH08 的 `familiarCreateA("Sub37",%SELF_X,%SELF_Y,…)` 被翻成 `enmCreateA("MystiaChild37",%Q-192,%R,…)`
> （自己换算成绝对坐标）⇒ 与上面 A/无 A 的判读逐条一致。

## 1 面出怪的几个观察（给你定掉落规则用）

- **成对 / 成群的「杂鱼」占绝大多数**：`Cir00p/Cir00m`（各 10，MainSub00 一次撒 20）、
  `Cir00bp/Cir00bm`（各 10，MainSub05）、`Cir01a/Cir02a`（各 9，MainSub04）—— 这些是典型小怪。
- **中量级**：`GirlB01` **16 只**（MainSub02 一次 8 只、MainSub06b 再 8 只）、`GirlD01/D02` 各 3 只。
- **队长型**：`GirlRedA01/GirlBlueA01`（MainSub01/01b 成对出现）、`GirlC01`、`GirlRedA01n/GirlBlueA01n`。
- 道中一共 ~**89 只**（不含 boss / 中boss / 标题用 `MapleEnemy`）。

## 掉落规则的几个可选口径（挑一个我再实现）

| 口径 | 规则 | 优点 / 风险 |
|---|---|---|
| A. 只掉「中量级 + 队长」 | `Cir*` 全不掉；`GirlB01/GirlD*/GirlRedA*/GirlBlueA*/GirlC01` 各掉概率 p | 场上灵数量可控；但杂鱼完全不掉，手感偏"看敌不看我" |
| B. 每 N 只必掉（计数器） | 不管类型，累计击破 N 只掉 1 只（N 可按关卡/章节调） | 最稳、最好调；但杀掉"精英怪才有奖励"的直觉 |
| C. 按波结算 | 每个 `MainSub` 清完时，按这波敌人数掉落（波大掉多） | 节奏感好（一波一波给）；实现要在波边界挂钩 |
| D. 混合（推荐先试） | `Cir*` 概率很低；`GirlB01` / `GirlD*` / 队长型 概率高；再叠一个「每 20 只保底 1 只」 | 兼顾直觉与节奏；参数多但都可调 |

> TH17 一手已核到的只有**相位/符卡边界**那条（boss 相位结束掉 2 资源灵、符卡边界掉 1 动物灵 —— 我们现在就是这么做的）；
> **道中小怪怎么掉，TH17 侧还没核**（正是你列的那条已知未解决）。要一手核就去 `local/th17.v1.00b/dat/st01.ecl`：
> 找 `dropMain`/`dropItems` 系列 opcode 在道中敌机子程序里的调用，看它是否按敌机类型/章节号给掉落表。
