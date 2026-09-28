# th09.v1.50a —— 東方花映塚 ～ Phantasmagoria of Flower View.（样本登记 + 导航）

> **版本**：TH09 v1.50a（`th09.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。

> **本文只做「登记与导航」**。**引擎结论一律写回 `engine/<子系统>/th09/`**。

## 1. 样本与工程

| 项 | 值 |
| --- | --- |
| 版本 | `th09.v1.50a`（Steam 版编号；本作 2005 年的原始版是 1.50a）|
| 来源（本机）| `D:\117\桌面\迫真车万\[th09] 东方花映塚 (汉化版+日文版)\`（另有 Steam）|
| 本仓副本 | `local/th09.v1.50a/th09.exe`（**685,056 B**，md5 `cf634df46e05552e104fa97a971aaac0`）+ `th09.dat` 88,180,006 B |
| 壳 | ✅ **无壳**（PE 段 = `.text/.rdata/.data/.data1/.rsrc`，**不含** `.bind`）|
| 数据 | `th09.dat`（88 MB）|
| Ghidra 工程 | ✅ **已建**：`local/th09.v1.50a/ghidra_projects/th09.exe.gpr` |
| MCP `database_id` | `th09.v1.50a` |
| exe 内函数总数 | **2090**（已命名 **618** / 🔬 真·待挖 **1472**）|
| dump | `local/th09.v1.50a/th09.v1.50a-funcs.json` |
| 命名金矿 | ✅ **有**：`local/vendor/th-re-data/data/th09.v1.50a/`（ExpHP）⇒ `applied=272 skipped=3 missing=0`、types 63 / statics 28 / labels 262 |
| SJIS 字符串导入 | `[sjis] found=83 applied=83 already=0 occupied=0 failed=0` |
| 符号导出 | ⏳ 未做（干完活：`tooling/ghidra/symbols.py export th09.v1.50a`）|

> ⚠ 样本是 ZUN 版权商业软件：**只放 `local/`，永不入库**。

## 2. 本作是什么（导航用）

**全系列第一个对战型弹幕游戏**（第二个 = TH19 獣王園，2023）。与单人作的差别是系统级的：

| 特征 | 一句话 |
| --- | --- |
| 分屏对战 | 两侧各一个场地，双方弹幕/敌人会**互相送入**对面场地 |
| 花（Flower）流程 | 击破敌人推进关卡进度，进度满时**把 Boss 攻击送进对手场地** |
| 霊 / 幽灵 | 场上的霊（幽灵）是资源与攻击的载体 |
| 无通常意义的残机/Bomb 循环 | 对战用「被弹 → 精神条/生命」的另一套判定（待一手核）|

> 以上为**二手**（社区常识层）。一手结论见 `engine/` 的对应文档（本作尚未开挖）。

## 3. 覆盖统计（挖了多少）

| 子系统 | 状态 |
| --- | --- |
| 样本 / 工程 / 命名 | ✅ 三项齐（th-re-data 命名已套）|
| 对战循环（花 / 送敌人 / 场地）| ⏳ 未挖（119 工单相关）|
| 霊（幽灵）系统 | ⏳ 未挖 |
| 其余 | ⏳ 未挖 |

## 4. 待挖图（按「119 合成卡需要什么」排序）

1. **对战骨架**：两侧场地对象 / 每帧更新 / 胜负判定函数在哪里（th-re-data 有名字，先按名字定位）。
2. **花与进度**：progress 字段、阈值、满进度时「送攻击」的入口（对应 TH19 的「送動物霊」）。
3. **霊（幽灵）**：生成、移动、被击破的效果。
4. **自机侧**：与单人作差异在哪（shooter 表 / 判定 / 决死）。

## 关联

- 119 合成卡工单：`mods/th18.v1.00a/card-expand/SINGLE-CARD-TEST.md` 的 `§th20` 第 6/8 条。
- 姊妹作（TH19 獣王園）：[`games/th19.v1.00a/INDEX.md`](../th19.v1.00a/INDEX.md)。
- 命名金矿用法：[`tooling/ghidra/th-re-data.md`](../../tooling/ghidra/th-re-data.md)。
