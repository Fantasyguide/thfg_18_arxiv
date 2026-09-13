# TH18 stage 1（1 面）道中出怪 dump —— 给「道中小怪掉动物灵」定规则用

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

| opcode | 参数 | 备注 |
|---|---|---|
| `enmCreate` | `301 enmCreateA` | 同步建敌（等它结束） |
| `enmCreateA` | `302 anmSelect` | A = async：不等待，主流程继续 |
| `enmCreateM` | `305 enmCreateAM` | M = 绑定/镜像？ |
| `enmCreateAM` | `306 anmSetMain` | A + M |
| `enmCreateF` | `310 enmCreateAF` | F 变体 |
| `enmCreateAF` | `311 enmCreateMF` | A + F |
| `enmCreateMF` | `312 enmCreateAMF` | M + F |
| `enmCreateAMF` | `313 anmSelectedPlay` | 三者叠加 |

> 说明：`A/AF/AM/AMF` 只差在「是否 async」与 `F/M` 两个变体（本 dump 里 1 面只用到
> `enmCreate` / `enmCreateF` / `enmCreateM` / `enmCreateA` 四种）。参数里前两个是**逻辑坐标**，
> 后面几项是**子程序号 / 血量 / 掉落表号**一类（具体逐位含义见 `engine/ecl/th18/` 与这份 dump 的实际调用）。

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
