# th19.v1.00a —— 東方獣王園 ～ Unfinished Dream of All Living Ghost.（样本登记 + 导航）

> **版本**：TH19 v1.00a（`th19.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。

> **本文只做「登记与导航」**（样本路径 / 版本 / 覆盖统计 / 待挖图）。
> **引擎结论一律写回 `engine/<子系统>/th19/`**。

## 1. 样本与工程

| 项 | 值 |
| --- | --- |
| 版本 | `th19.v1.00a`（正式版 2023/08/13；官方 readme 日期同）|
| 来源（本机）| `D:\SteamLibrary\steamapps\common\th19\`（Steam 版；另有体验版 `D:\117\桌面\迫真车万\th19tr\`）|
| 原始 exe（**加壳**）| `th19.steamstub.exe` **2,163,416 B**，md5 **`8147b8d2bd8fe67b8d186e7fb3b5804d`**（PE 段含 `.bind` = SteamStub）|
| 脱壳（本仓副本）| `local/th19.v1.00a/th19.exe` **1,993,728 B**，md5 **`dd0e1ff8c02327bf6408f763b8beedc9`** |
| 脱壳判据 | `.bind` 段消失、入口点回到 `.text`（`0x638b8`）、`.text` 熵 **6.542**（正常代码；加壳态 ≈8.0）|
| 脱壳工具 | `tooling/steamless`（`~/.dotnet/dotnet bin/Release/net8.0/steamless-linux.dll <exe>`）|
| 数据 | `th19.dat` **115,306,902 B**；同目录另有 `thbgm.dat` / `custom.exe` / `omake.txt` / `readme.txt` |
| ⚠ 安装纯度 | 该 Steam 安装里另有**未打包的 `data/`（352 文件）与 `ANM/`**，像是打过补丁/解包过的安装 ⇒ 已核对：脱壳后 exe 的代码面可用；**若要当正式样本发布前请复核版本** |
| Ghidra 工程 | ✅ **已建**：`local/th19.v1.00a/ghidra_projects/th19.exe.gpr` |
| MCP `database_id` | `th19.v1.00a` |
| exe 内函数总数 | **4740**（已命名 **526** / 🔬 真·待挖 **4214**）|
| dump | `local/th19.v1.00a/th19.v1.00a-funcs.json` |
| SJIS 字符串导入 | `[sjis] found=128 applied=116 already=0 occupied=0 failed=12` |
| 符号导出 | ⏳ 未做（干完活：`tooling/ghidra/symbols.py export th19.v1.00a`）|

> ⚠ 样本是 ZUN 版权商业软件：**只放 `local/`，永不入库**；本文件里只记「路径 + md5 + 尺寸」这类事实。

## 2. 命名金矿：**RTTI + assert 串**（比 TH20 还厚）

和 TH20 一样，本作 exe 里留着 **MSVC RTTI（395 条 `.?AV`）** 与 **ZUN 的 assert 串（251 条
`…\src\…`）**，后者还给出**每角色一个源文件**的地图（`pl02func.cpp` = BombSanaeInf、
`pl11func.cpp` = BombSakiInf …）。挖法照抄 TH20 那条链（脚本是**版本参数化**的，
`--version th19` 即可）：

| 工具 | 干什么 |
| --- | --- |
| `tooling/ghidra/th20_anchors.py --version th19` | 字符串 → 引用它的函数（assert 锚点）|
| `tooling/ghidra/th20_rtti.py --version th19` | RTTI 复原：类名 → COL → vftable → 逐槽 |
| `tooling/ghidra/th20_names.py --version th19` | 写库：清 FID 假阳性 + `Class::vftable` + assert 注释 |
| `tooling/ghidra/th20_decomp_corpus.py --version th19` | 按区间落反编译语料库（**不占锁地并行分析**）|
| `tooling/ghidra/th20_show.py --corpus local/th19.v1.00a/decomp` | 从语料库取函数 / grep |

> ⚠️ 那批脚本的 `th20_` 前缀是历史遗留（先给 TH20 写的），**它们本身版本参数化**。
> ⚠ GCC/Ghidra 工程锁独占：并行分析只能「一个人生成语料库，其余人读文件」。

### 2a. 类地图（RTTI 复原出的 **178 个 ZUN 类**，这是本作挖库的索引）

RTTI 链子跑完（`local/th19.v1.00a/th19-rtti.json`，448 个 vftable），ZUN 自己的类按功能分族：

| 族 | 类数 | 代表 | 对应系统 |
| --- | :-: | --- | --- |
| `AbilityCard*Inf` | 41 | `AbilityCardAun1Inf`、`AbilityCardSaki2Inf`、`AbilityCardInf`（基类）、`AbilityCardNullInf` | **能力卡**：一张卡一个类（19 角色 × 2 张 + 特殊）|
| `Ability*Inf` | 3 | `AbilityInf`、`AbilityMenuInf`、`AbilityUnlockInf` | 能力系统核心 / 菜单 / 解锁 |
| `Aura*Inf` | 21 | `AuraCtrlInf`（管理器）、`AuraReimuInf`、`AuraRanInf`… | **除霊オーラ**（每角色一套）|
| `EX*Inf` | 34 | **`EXReigekiInf`**（＝霊撃）、`EXLaserInf`、`EXOnmyoInf`、`EX*ExtraInf` | 追加攻击 / 霊撃 / EX |
| `ExtraAttack*Inf` | 3 | `ExtraAttackBaseInf`、`ExtraAttackCharaBaseInf`、`ExtraAttackInf` | 追加攻击基类族（疑＝動物霊送り込み）|
| `Bomb*Inf` | 14 | `BombReimuInf`、`BombSakiInf`… | 每角色的 Bomb |
| `Ghost*Inf` | 1 | `GhostCtrlInf` | **霊（幽灵）管理** |
| 规则 | 1 | `GameRuleTaskInf` | **对战规则 task**（胜负 / 阶段）|
| 故事模式 AI | 8 | `CPUHitInf`、`CPUStoryInf`、`CPUStage1..6StoryInf`、`CPUHitReimu/MarisaInf` | 剧情模式的 CPU |
| 网络 | 3+ | `NetworkInf`、`NetworkLogicInf`、`NetworkTestInf` | Steam 在线对战（本工单不需要）|
| 通用 | — | `PlayerInf`/`EnemyInf`/`BulletInf`/`ItemInf`/`SmallScoreInf`/`EffectInf`/`FrontInf`… | 与其它作同名同族 |

> **源文件锚点偏少**：TH19 的 251 条 `src\…` 串里**只有 5 条**被函数引用（TH20 是 156 条），
> 所以本作主要靠 **RTTI 类名 + vftable 槽位**定位，不能靠 assert 串。

## 3. 本作的特殊系统（导航用）

| 系统 | 一句话 | 一手线索 |
| --- | --- | --- |
| **对战型弹幕** | 全系列第二个对战作（第一个 = TH09 花映塚）；画面构成大体基于 TH09，系统重构 | 官方四方山新闻（二手转述）|
| **能力卡（アビリティ）** | 沿用 **TH18/TH18.5 的卡牌系统**；每角色 2 张能力，选 1 用 | `data/ability.txt`（本仓 `local/th19.v1.00a/ability.txt`）；`ability.anm` / `abcard.anm` / `abmenu.anm` |
| **動物霊送り込み** | 把 カワウソ／オオカミ／オオワシ 灵**送进对手场地**（对战核心动作）| `ability.txt` 的 `@YACHIE1` / `@SAKI1` / `@TOUTETSU1` 等 |
| **霊撃 / チャージ / バリア / 除霊オーラ** | 自身场地的资源循环（`@NAZRIN1/2`、`@AUN1/2`、`@MAMIZOU2`）| 同上 |
| 无分数 / 无 Replay | 官方口径：取消分数系统、无 Replay | 二手转述；待一手核 |

## 4. 覆盖统计（挖了多少）

| 子系统 | 状态 |
| --- | --- |
| 样本 / 工程 / 命名 | ✅ 样本（已脱壳）+ ✅ 工程（4740 函数）+ ⏳ 命名层（RTTI/assert 管线待跑完）|
| 能力卡系统 / 動物霊 / 霊撃 | ⏳ 未挖（本次 119 工单的主目标）|
| 对战循环（CH 送り込み・场地・霊）| ⏳ 未挖 |
| 其余 | ⏳ 未挖 |

## 5. 待挖图（按「119 合成卡需要什么」排序）

1. **能力卡（アビリティ）系统**：能力枚举 / 表地址 / 「选 1 张」的装配链 / 每张能力挂在哪（与 TH18 的卡牌系统同源到什么程度）。
2. **動物霊**：種類（カワウソ/オオカミ/オオワシ）、生成、送进对手场地的机制（「送り込む」到底改的是对手哪个字段）、强化/弱化能力怎么作用。
3. **自身场地循环**：霊撃（次数/时长）、チャージ（速度）、バリア（破坏时消弹）、除霊オーラ（範囲）。
4. **对战骨架**：双方的场地对象、胜负判定、Boss/幽灵的交换（TH09 是「送 Boss 攻击」，TH19 是「送動物霊」）。
5. 一手核「无分数 / 无 Replay」（二手口径）。

## 关联

- 119 合成卡工单与 owner 口径：`mods/th18.v1.00a/card-expand/SINGLE-CARD-TEST.md` 的 `§th20` 第 6/8 条、
  [`games/th20.v1.00a/INDEX.md`](../th20.v1.00a/INDEX.md) §2b-3 / §2b-4。
- 兄弟作（TH09 花映塚）：[`games/th09.v1.50a/INDEX.md`](../th09.v1.50a/INDEX.md)。
- 挖库手册：[`tooling/ghidra/README.md`](../../tooling/ghidra/README.md)。
