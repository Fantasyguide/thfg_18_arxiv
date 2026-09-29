# th20.v1.00a —— 東方錦上京 ～ Fossilized Wonders.（样本登记 + 导航）

> **版本**：TH20 v1.00a（`th20.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。

> **本文只做「登记与导航」**（样本路径 / 版本 / 覆盖统计 / 待挖图）。
> **引擎结论一律写回 `engine/<子系统>/th20/`**，卡侧的复刻规格写在
> `mods/th18.v1.00a/card-expand/SINGLE-CARD-TEST.md` 的 `§th20` 段。

## 1. 样本与工程

| 项 | 值 |
| --- | --- |
| 版本 | `th20.v1.00a`（正式版；官方 readme 日期 **2025/08/17**）|
| 来源（本机）| `D:\117\桌面\迫真车万\th20\th20\`（另有体验版 `th20tr\`，2025-04-16）|
| 本仓副本 | `local/th20.v1.00a/th20.exe` + `th20.dat`（`local/*` 已 gitignore ⇒ 不入库）|
| exe | `th20.exe` **1,858,560 B**，md5 **`f6bdf3535813a5d8c29a1ec429c72934`** |
| 数据 | `th20.dat` 150,943,726 B（144 MB）；同目录另有 `thbgm.dat` / `custom.exe` / `omake.txt` / `readme.txt` |
| Ghidra 工程 | ✅ **已建**：`local/th20.v1.00a/ghidra_projects/th20.exe.gpr`（`bootstrap.py th20.v1.00a` 5 步全过）|
| MCP `database_id` | `th20.v1.00a` |
| exe 内函数总数 | **7315**（已命名 **1120** / 🔬 真·待挖 **6195**）|
| dump | `local/th20.v1.00a/th20.v1.00a-funcs.json`（7315 个）|
| SJIS 字符串导入 | `[sjis] found=62 applied=62 already=0 occupied=0 failed=0` |
| 符号导出 | ✅ **已导出**：`games/th20.v1.00a/symbols.json`（475 条 —— 具名来自 MSVC RTTI + ZUN 的 assert 串，见 §2d）|

> ⚠ 样本是 ZUN 版权商业软件：**只放 `local/`，永不入库**；本文件里只记「路径 + md5 + 尺寸」这类事实。

## 2. 本作的特殊系统（导航用；细节一律回 `engine/` 与卡侧规格）

官方 wiki 与 `omake.txt` 都写到的四件事（**owner 2026-09-23 给的复刻对应关系也记在这里**）：

| 系统 | 一句话 | owner 指定的复刻参照 |
| --- | --- | --- |
| **异变石（4 槽装备）** | 主（上，定**异变攻击**样式 + 存档/Replay 的「机体类型」）/ 扩散（左，高速射击样式）/ 集中（右，低速射击样式）/ 支援（下，支援功能）；三槽每槽消耗 1 块，主槽不消耗；通关得 1 块与主石同种 | ❓ 待 owner 拍（装备界面在**商店/编成**里？还是卡内自建 UI？）|
| **异变石碎片（道具）** | 擦弹与持续命中掉落；收集蓄**异变攻击槽** | ≈ **116 的季节道具**（`§th16`）|
| **异变攻击** | 槽满**自动发动**；样式由主石决定（槽长/持续各不相同）；**发动中中弹 ⇒ 立即结束本次异变攻击来抵消该次 Miss** | ≈ **117 的暴走咆哮**（`§th17`）|
| **异变敌人** | 竖直槽（连续击破 / 擦弹累积；Boss 战中自然上涨）满 ⇒ 出现一名干扰者；击破解体掉大量道具 + 碎片；**同色每次出现都升级**（耐久↑ 弹幕↑），Miss 降级；颜色由石决定 | ≈ **112 的飞碟**（`§th12`）|
| **异变值** | 右侧；击破异变敌人 / 发动异变攻击提升；影响**得分道具的倍率** | ≈ **113 的计分族 / 110 的信仰倍率**（`§th13` / `§th10`）|

## 2b. owner 的裁决（2026-09-23）—— **本阶段只调研**

1. **不做异变石装备界面**：「异变石 = 过往异变的能力，**拥有那一作的卡 = 拥有这个石头的支援效果**」
   ⇒ 石槽效果**由「装了哪几张卡」决定** ⇒ wiki 里「四槽界面 / 数量消耗 / 通关奖励」那一整套**不用做**。
2. **抵消 Miss 的优先级** = **107 → 117 → 120 → 决死**（异变攻击插在 117 之后、决死之前）。
3. **实现授权**：**复制粘贴骨架**（116 碎片 / 117 异变攻击 / 112 异变敌人），只改贴图与 anm/ECL 脚本；
   gate / 动态 / mode 这些**完全照抄**。
4. **118 / 119 空号不动**。★ **本阶段仅调研** ⇒ 只建工程 + 挖表 + 结论回流 `engine/`，**不写实现**。

### 2b-2. owner 的实机口径（2026-09-28）—— **两条槽的归属**

> 「它有两个槽，一个是**异变敌召唤**，一个是**异变攻击储存**」

这句把调研期唯一一处「一手无法区分」的地方收口了（wiki 的两根槽 ↔ 游戏对象上的两对字段）：

| 槽（owner 口径） | 字段（一手） | clamp / 阈值 | 文档 |
| --- | --- | --- | --- |
| **异变敌召唤** | `game+0x5c`（当前） / `game+0x60`（阈值） | `[0,10000]` / `[0,5000]`，阈值随难度 1100→1400、击破 +150 | [`engine/stone/th20/03`](../../engine/stone/th20/03-stone-enemy.md) |
| **异变攻击储存** | `game+0x4c`（蓄积） / `game+0x50`（满槽） | `[0,500]` / `[100,500]`，满槽随石 `{130,130,150,150,180,130,130,150}` | [`engine/stone/th20/05`](../../engine/stone/th20/05-shards-and-items.md) |
| 「异变值」（wiki 右侧那个数） | `game+0x44`（分母 `+0x48`） | `[0,1000000]` / `[5000,10000]`；得分道具倍率 = `1/2 + (+0x44)/(+0x48)` | [`engine/stone/th20/04`](../../engine/stone/th20/04-incident-value.md) |

⇒ 两条槽都不是「异变值」⇒ **异变值 = `+0x44`** 定案（该文档 §1.2 的 ❓ 已关）。

### 2b-3. 石 ↔ 卡号 映射（owner 2026-09-28 定案）

owner 口径：「**兽之冷硬是 119**，认为是 **109 花映塚可触发**」（兽之冷硬 = ビーストハードネス / Beast Hardness）。

| 石（日文名） | English | 卡号 | 依据 |
| --- | --- | --- | --- |
| スカーレットデビル | Scarlet Devil | 106 紅魔郷 | 名字 = 副标题 |
| スノーブロッサム | Snow Blossom | 107 妖々夢 | 名字 = 副标题 |
| インペリシャブルムーン | Imperishable Moon | 108 永夜抄 | 名字 = 副标题 |
| シントイズムウィンド | Shintoism Wind | 110 風神録 | 名字 = 副标题 |
| イエローサブタレイニアン | Yellow Subterranean | 111 地霊殿 | 名字 = 副标题 |
| ブルーシーズン | Blue Season | 116 天空璋 | 名字 = 副标题 |
| クリーチャーレッド | Creature Red | 117 鬼形獣 | 名字（Weakest Creature）+ EX 提「勁牙組組長/袿姫」|
| ビーストハードネス | Beast Hardness | **并入 109**（109 = 花映塚 × 獣王園 合成卡）| owner 口径 + EX 提「地獄の鬼、**残無**」= TH19 的日白残無 |

- ⇒ 8 块石 ↔ **106 / 107 / 108 / 109 / 110 / 111 / 116 / 117**（**109 是合成卡**；缺 112 / 113 / 114 / 115 没有石）。
- ⚠ **118 / 119 的口径**：**两张都保持空号**（owner 2026-09-28 订正：「**注意，没有 119，它就是 109**」）⇒ 兽之冷硬的效果并入 **109**。
- ★ **owner 追加口径（2026-09-28）**：「119 和 109 **都是对战作**，并且从原始的 th09–th19 编号来看
  就**有纪念意义**，我打算实际上**合成一张卡**。不过我几乎不打对战作，具体怎么回事还得再看」
  ⇒ **109 这张卡 = 花映塚（TH09）+ 獣王園（TH19）**合成**（owner 2026-09-28 追加：「没有 119，它就是 109」）**；机制细节**待调研**（见下方「样本侦察」）。
- 细节与证据见 [`engine/stone/th20/01`](../../engine/stone/th20/01-stone-table-and-slots.md) §6。

### 2b-4. 109 合成卡的样本侦察（2026-09-28，**只查了在不在、没建库**）

| 项 | 事实 |
| --- | --- |
| TH09 花映塚 | ✅ 本仓已有：`local/th09.v1.50a/th09.exe` + `th09.dat`（**尚未建 Ghidra 工程**）|
| TH19 獣王園 | ✅ **已建库**（`local/th19.v1.10c/`，4740 函数）：原始 exe 2,163,416 B md5 `8147b8d2bd8fe67b8d186e7fb3b5804d` → 脱壳后 1,993,728 B md5 `dd0e1ff8c02327bf6408f763b8beedc9` |
| ★ TH19 版本 | 脱壳后 exe 的 VERSIONINFO 是 **ver 1.10c**（不是 1.00a）⇒ 目录与 `database_id` 记作 **`th19.v1.10c`**；见 [`games/th19.v1.10c/INDEX.md`](../th19.v1.10c/INDEX.md) §1a |
| ⚠ TH19 exe | **SteamStub 壳**（PE 段里有 `.bind`）⇒ 已过 `tooling/steamless` 脱壳（判据：`.bind` 消失、EP 回 `.text`、熵 6.54）|
| ⚠ TH19 安装 | 同目录另有**未打包的 `data/`（352 个文件）与 `ANM/`**，像是打过补丁/解包过的安装；当样本前要确认版本与 md5 |
| 一手系统文档 | 能力卡文案 **1.10c 版**（从 dat 解出，5074 B，md5 `8720e840…`）= `local/th19.v1.10c/ability.txt`；`abcard.anm`/`abmenu.anm`/`ability.anm` = 卡面/菜单/演出 |
| 官方定位（二手转述）| TH19 是**对战型弹幕游戏**，画面构成**大体基于 TH09**、但**重构了系统**（操作感更接近通常的东方作），**取消分数系统**，并**沿用 TH18/TH18.5 的卡牌系统**，且**没有 Replay** |

### 2c. 已知限制：**没有社区命名可比对**（TH20 太新）

`tooling/ghidra/build_worklist.py th20.v1.00a` **跑不了** —— 它要 `local/vendor/th-re-data/th20.v1.00a`
（ExpHP 的逐版本 funcs/statics/结构体），而**这一作还没有**（TH06–TH17 都有，TH20 没）。
⇒ 影响：① 生成不了 `games/th20.v1.00a/unexplored.md`（自动待挖清单）；② 建库时「套 ExpHP 名」那一步跳过
（`bootstrap` 已提示 `th-re-data（缺失,跳过命名）`）。
⇒ **唯一后果是「函数名全靠我们自己起」**（比 TH16/TH18 那些辛苦一点），**不影响挖**：
直接按地址读反汇编 + 用 MCP 只读工具即可（工具目录见 `tooling/ghidra/mcp-tools.md`）。
⚠ 若哪天社区出了 th20 的 re-data，放进 `local/vendor/th-re-data/` 再重跑 `bootstrap` 就能补上命名。

### 2d. 本作的**替代命名金矿**：RTTI + assert 串（✅ 已用，不靠社区）

exe 里保留着 **MSVC RTTI 类名**与 **ZUN 的 assert 字符串**（`…\src\game\weaponred.cpp:42 StoneRedReimuInf`），
二者合起来给了我们 **79 个 ZUN 自己的类名 + 125 个挂在函数上的 `file:line` 锚点**。
工具与用法（含四个已踩的坑）见 [`tooling/ghidra/README.md`](../../tooling/ghidra/README.md) 的 TH20 一节：

| 工具 | 干什么 |
| --- | --- |
| `tooling/ghidra/th20_anchors.py` | 字符串 → 引用它的函数（assert 锚点），出 `local/th20.v1.00a/th20-anchors.json` |
| `tooling/ghidra/th20_rtti.py` | RTTI 复原：类名 → COL → vftable → 逐槽函数，出 `th20-rtti.json` |
| `tooling/ghidra/th20_names.py` | 写库：清 **73 处 FID 假阳性** + 套 `Class::vftable` + 钉 assert 注释 |
| `tooling/ghidra/th20_decomp_corpus.py` | 按地址区间把**全部**反编译落成语料库（`local/th20.v1.00a/decomp/`，~7000 函数）|
| `tooling/ghidra/th20_show.py` | 从语料库取函数 / grep / 找调用方（**不占工程锁** ⇒ 可并行分工）|

> ⚠ **Ghidra 工程锁是独占的**（两个只读 driver 也会撞）：多人/多 agent 并行挖本作时，
> 一个人生成语料库，其余人读文件。

## 3. 覆盖统计（挖了多少）

| 子系统 | 状态 |
| --- | --- |
| 样本 / 工程 / 命名 | ✅ 样本 + ✅ 工程（7315 函数）+ ✅ 命名（RTTI/assert 锚点，`symbols.json` 475 条）|
| **异变石（表 / 四槽 / 30 槽接口）** | ✅ [`engine/stone/th20/01`](../../engine/stone/th20/01-stone-table-and-slots.md)（工厂表 `0x5b0ab0`、四槽 setter、槽位语义）|
| **异变攻击（状态机 / 时长）** | ✅ [`engine/stone/th20/06`](../../engine/stone/th20/06-attack-gauge-and-miss.md)（三态 + 时长公式 + 槽线 280.0）|
| **异变攻击（ECL/anm/SHT 资产）** | 🟡 [`engine/stone/th20/02`](../../engine/stone/th20/02-attack-assets.md) |
| **异变敌人（槽 / 颜色 / 等级 / 掉落）** | 🟡 [`engine/stone/th20/03`](../../engine/stone/th20/03-stone-enemy.md) |
| **异变值（倍率 / 上限）** | 🟡 [`engine/stone/th20/04`](../../engine/stone/th20/04-incident-value.md) |
| **碎片道具（掉落 / 价值 / 吸收）** | 🟡 [`engine/stone/th20/05`](../../engine/stone/th20/05-shards-and-items.md) |
| **抵消 Miss 的链路** | ❓ [`engine/stone/th20/07`](../../engine/stone/th20/07-miss-cancel-chain.md)（引擎侧链路未坐实）|
| 玩家（移动 / 射击 / 子机）| ⏳ 未挖（本阶段不需）|
| 得分 / 道具（通用部分）| ⏳ 未挖 |
| 异变石「装备表 / 数量 / 通关奖励」（菜单侧）| ⏸ **本阶段按 owner 裁决①不挖**（不做装备界面）|

## 4. 待挖图（按「卡 120 需要什么」排序）

1. **异变石四槽的表** ✅ 已挖（[`stone/th20/01`](../../engine/stone/th20/01-stone-table-and-slots.md)）：
   工厂表 `th20:0x5b0ab0`（`[角色][9]`）、四槽偏移 `+0x28/+0x2c/+0x30/+0x34`、
   四槽 id 来自根对象 `+0x1c/+0x20/+0x24/+0x28`；**剩**：石 ↔ 卡号 的对应（需 owner 拍两块）。
2. **异变攻击槽**：✅ 状态机与时长（[`stone/th20/06`](../../engine/stone/th20/06-attack-gauge-and-miss.md)）；
   🟡 碎片充能换算（[`stone/th20/05`](../../engine/stone/th20/05-shards-and-items.md)）；
   ❓ **抵消 Miss 的窗口与优先级**（[`stone/th20/07`](../../engine/stone/th20/07-miss-cancel-chain.md)）。
3. **异变敌人槽**：🟡 [`stone/th20/03`](../../engine/stone/th20/03-stone-enemy.md)
   （竖直槽的蓄积 / 满槽生成 / 颜色与等级 / 击破掉落）。
4. **异变值**：🟡 [`stone/th20/04`](../../engine/stone/th20/04-incident-value.md)（提升量 / 倍率公式 / 上限 5000）。
5. **碎片道具**：🟡 [`stone/th20/05`](../../engine/stone/th20/05-shards-and-items.md)（与 116 季节道具的对照）。
