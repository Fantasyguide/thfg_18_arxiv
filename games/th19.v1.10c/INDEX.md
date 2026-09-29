# th19.v1.10c —— 東方獣王園 ～ Unfinished Dream of All Living Ghost.（样本登记 + 导航）

> **版本**：TH19 v1.10c（`th19.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。

> **本文只做「登记与导航」**（样本路径 / 版本 / 覆盖统计 / 待挖图）。
> **引擎结论一律写回 `engine/<子系统>/th19/`**。

## 1. 样本与工程

| 项 | 值 |
| --- | --- |
| 版本 | `th19.v1.10c`（正式版 2023/08/13；官方 readme 日期同）|
| 来源（本机）| `D:\SteamLibrary\steamapps\common\th19\`（Steam 版；另有体验版 `D:\117\桌面\迫真车万\th19tr\`）|
| 原始 exe（**加壳**）| `th19.steamstub.exe` **2,163,416 B**，md5 **`8147b8d2bd8fe67b8d186e7fb3b5804d`**（PE 段含 `.bind` = SteamStub）|
| 脱壳（本仓副本）| `local/th19.v1.10c/th19.exe` **1,993,728 B**，md5 **`dd0e1ff8c02327bf6408f763b8beedc9`** |
| 脱壳判据 | `.bind` 段消失、入口点回到 `.text`（`0x638b8`）、`.text` 熵 **6.542**（正常代码；加壳态 ≈8.0）|
| 脱壳工具 | `tooling/steamless`（`~/.dotnet/dotnet bin/Release/net8.0/steamless-linux.dll <exe>`）|
| 数据 | `th19.dat` **115,306,902 B**；同目录另有 `thbgm.dat` / `custom.exe` / `omake.txt` / `readme.txt` |
| ⚠ 安装纯度 | 该 Steam 安装里另有**未打包的 `data/`（352 文件）与 `ANM/`**，像是打过补丁/解包过的安装 ⇒ 已核对：脱壳后 exe 的代码面可用；**若要当正式样本发布前请复核版本** |
| Ghidra 工程 | ✅ **已建**：`local/th19.v1.10c/ghidra_projects/th19.exe.gpr` |
| MCP `database_id` | `th19.v1.10c` |
| exe 内函数总数 | **4740**（已命名 **526** / 🔬 真·待挖 **4214**）|
| dump | `local/th19.v1.10c/th19.v1.10c-funcs.json` |
| SJIS 字符串导入 | `[sjis] found=128 applied=116 already=0 occupied=0 failed=12` |
| 符号导出 | ⏳ 未做（干完活：`tooling/ghidra/symbols.py export th19.v1.10c`）|

> ⚠ 样本是 ZUN 版权商业软件：**只放 `local/`，永不入库**；本文件里只记「路径 + md5 + 尺寸」这类事实。

### 1a. ★ 版本勘误：本构建是 **ver 1.10c**，不是 1.00a（2026-09-28 资产侧发现）

| 证据 | 内容 |
| --- | --- |
| exe 内 UTF-16 版本资源 `0x593dd4` | `東方獣王園　～ Unfinished Dream of All Living Ghost. ver **1.10c**` |
| ANSI 串 `0x5a05e4` | `1.10c` |
| `th19.dat` 内清单 | **`th19_110c.ver`**（已解到 `local/th19.v1.10c/th19_110c.ver`）|
| D 盘安装的**未打包** `data/` | `th19_100a.ver` ⇒ 那个目录是 **1.00a** 的旧料 |

⇒ **exe / `th19.dat` / Ghidra 工程 / 反编译语料库 = 1.10c**，故本目录与 `database_id` 都记作 `th19.v1.10c`。
⇒ 仓里另存两份**能力卡文案**以便对照：`local/th19.v1.10c/ability.txt`（**1.10c，md5 `8720e840…`，从 dat 解出**）与
`ability.100a.txt`（1.00a，md5 `ce0ff4ea…`，来自 D 盘 `data/`）。两者**只差 1 行**：
`@TOUTETSU1` 说明文（1.00a「送り込んだオオワシ霊が強くなる」→ 1.10c「自分のオオワシ霊が弱くなる」）。
⚠ 同理 `abcard.anm` 也有 1.00a/1.10c 两版，差异**全部**落在 entry27–30（Nazrin 两张卡的名字+贴图互换，
见 [`engine/card/th19/02-ability-assets.md`](../../engine/card/th19/02-ability-assets.md) §6）。

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
| `tooling/ghidra/th20_show.py --corpus local/th19.v1.10c/decomp` | 从语料库取函数 / grep |

> ⚠️ 那批脚本的 `th20_` 前缀是历史遗留（先给 TH20 写的），**它们本身版本参数化**。
> ⚠ GCC/Ghidra 工程锁独占：并行分析只能「一个人生成语料库，其余人读文件」。

### 2a. 类地图（RTTI 复原出的 **178 个 ZUN 类**，这是本作挖库的索引）

RTTI 链子跑完（`local/th19.v1.10c/th19-rtti.json`，448 个 vftable），ZUN 自己的类按功能分族：

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
| **对战型弹幕** | 全系列第二个对战作（第一个 = TH09 花映塚）；画面构成大体基于 TH09，系统重构 | 官方四方山新闻（二手转述）+ 一手：两对称场地（见 [`versus/th19/03`](../../engine/versus/th19/03-sending-and-rules.md)）|
| **能力卡（アビリティ）** | 沿用 **TH18/TH18.5 的卡牌系统**；每玩家 1 张（角色 2 选 1）| 静态表 `0x5c5640`（40 项 × `0x20`）、23 槽虚表 — 见 [`card/th19/01`](../../engine/card/th19/01-ability-cards.md) |
| **動物霊送り込み** | 把 オオカミ／カワウソ／オオワシ 灵**送进对手场地** | 换生成侧 `0x5d19bc+0x710 = 1-owner` + 挂对手 EnemyCtrl — 见 [`versus/th19/03`](../../engine/versus/th19/03-sending-and-rules.md) |
| **除霊オーラ / バリア / 蓄力** | 自身场地的循环：`AuraCtrlInf`（`0x94` = 蓄力 5 段）、独立バリア对象 | [`versus/th19/02`](../../engine/versus/th19/02-self-field-systems.md) |
| **霊撃 == Bomb** | ★ 两者是**同一个资源字段** `S+0x68`（不是两条资源）| 同文 §2 |
| 无分数 / 无 Replay | 二手口径；一手旁证：`ReplayInf` / `SmallScoreInf` 在语料里**只剩构造函数**（各 2–3 处 vftable 赋值）| `local/th19.v1.10c/decomp/all.c` |

## 4. 覆盖统计（挖了多少）

| 子系统 | 状态 |
| --- | --- |
| 样本 / 工程 / 命名 | ✅ 样本（已脱壳，**1.10c**）+ ✅ 工程（4740 函数）+ ✅ RTTI/assert 管线（178 vftable 标签、18 处 FID 假阳性已清）|
| 能力卡系统 | ✅ [`card/th19/01`](../../engine/card/th19/01-ability-cards.md)（静态表 / 装配链 / 23 槽 / 生效点）|
| 能力卡资产 | ✅ [`card/th19/02`](../../engine/card/th19/02-ability-assets.md)（`abcard.anm` 85 entry / `abmenu.anm` / 卡面尺寸）|
| 自身场地系统 | ✅ [`versus/th19/02`](../../engine/versus/th19/02-self-field-systems.md)（除霊オーラ / 霊撃==Bomb / 蓄力 / バリア）|
| 对战骨架 + 動物霊送り込み | ✅ [`versus/th19/03`](../../engine/versus/th19/03-sending-and-rules.md)（**胜负判定仍 ❓**）|
| 剧情模式 / 网络 | ⏳ 未挖（本工单不需要）|

## 5. 待挖图（按「109 合成卡需要什么」排序）

1. ~~能力卡系统~~ ✅ 已挖完（表 `0x5c5640`、id→类 `0x4deba0`、装配 `0x4e2c08`）；**剩**：`[ctx+0x2c]` 那个参数对象的类名与字段图。
2. ~~動物霊送り込み~~ ✅ 已挖完（换生成侧机制）；**剩**：`+0xab/ac/ad`「被攻撃弱体化」的读取点。
3. ~~自身场地循环~~ ✅ 已挖完（オーラ / 霊撃 / 蓄力 / バリア / Bomb）；**剩**：`S+0x84/88/8c/90` 初值 100 的写点（疑在角色参数表 `*(stage+0x2238)`）。
4. **对战规则与胜负**：`0x5185c0`–`0x518dff`（建两侧 + 比赛启动，**不在语料库里**）、`0x516450` 返回码 0/1/3 语义 ❓。
5. 一手补全「无分数 / 无 Replay」的结论（现在只有「类还在、引用极少」的旁证）。
6. **`th20_rtti.py` 漏类**：TH19 有 16–23 个类（`BombSakiInf`/`BombPL14..18Inf`/`EXSakiInf`/`EXPL14..18Inf` 等）
   未被复原 ⇒ 值得把「`.rdata` 反查 COL→类型名」那条扫描并进工具（两个 agent 各自用脚本补出）。

## 关联

- **引擎结论（本次 119 工单的全部一手）**：
  [`engine/card/th19/01`](../../engine/card/th19/01-ability-cards.md)（能力卡系统）、
  [`engine/card/th19/02`](../../engine/card/th19/02-ability-assets.md)（能力卡资产）、
  [`engine/versus/th19/02`](../../engine/versus/th19/02-self-field-systems.md)（自身场地系统）、
  [`engine/versus/th19/03`](../../engine/versus/th19/03-sending-and-rules.md)（对战骨架 + 動物霊）+
  门面 [`engine/versus/OVERVIEW.md`](../../engine/versus/OVERVIEW.md)、
  [`engine/card/OVERVIEW.md`](../../engine/card/OVERVIEW.md) 的 th19 小节。
- 109 合成卡工单与 owner 口径：`mods/th18.v1.00a/card-expand/SINGLE-CARD-TEST.md` 的 `§th19`（含**合成方案候选表**）、
  [`games/th20.v1.00a/INDEX.md`](../th20.v1.00a/INDEX.md) §2b-3 / §2b-4。
- 兄弟作（TH09 花映塚）：[`games/th09.v1.50a/INDEX.md`](../th09.v1.50a/INDEX.md)、
  [`engine/versus/th09/01`](../../engine/versus/th09/01-versus-loop.md)。
- 挖库手册：[`tooling/ghidra/README.md`](../../tooling/ghidra/README.md)。
