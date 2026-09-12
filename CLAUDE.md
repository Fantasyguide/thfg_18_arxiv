# CLAUDE.md — renkolab

东方 Project 引擎逆向工作台。**先读 [`README.md`](README.md)**（结构与导航），环境没搭好先看 [`docs/SETUP.md`](docs/SETUP.md)，
**动手前必读 [`METHOD.md`](METHOD.md)**（逆向记录纪律）与 [`DOCSTYLE.md`](DOCSTYLE.md)（文档规范）。
本文件只讲环境和跨层规矩。

## 层次归属：动手前先判断这一改属于哪层

| 层 | 装什么 | 轴 |
| --- | --- | --- |
| `engine/` | 引擎知识：运行时语义、字段图、opcode 表 | **子系统**为主，版本为辅 |
| `games/` | 每作的样本/工程登记、覆盖统计、待挖图 | 版本 |
| `mods/` | 改造产物：cave / patch / 成品脚本资产 | **版本**，死绑 exe build |
| `tooling/` | Ghidra 工具链 | 版本无关 |
| `local/` | exe、资产、Ghidra 工程、vendor（**gitignored**） | — |

三条容易犯的错：

1. **别在 `games/` 里写引擎结论。** 它只做登记和导航，正文指回 `engine/*`。
   一旦开始在那里写语义，版本轴就复辟了，这正是拆分前的病。
2. **新作特有机制是「子系统」，不是「版本目录」。** TH18 卡牌 → `engine/card/`，
   矩阵里 th16 一列填 ❌/❓ 即可。不要再开 `th19/` 这种国中之国。
3. **mod 产物不进 `engine/`。** cave 源码、patch、审计记录属于 `mods/<版本>/<mod名>/`。

## 环境（不写死路径；缺什么问 doctor）

```bash
python3 tooling/doctor.py     # 自检：缺什么 + 怎么补
source tooling/env.sh         # 探测并导出 GHIDRA_INSTALL_DIR / JAVA_HOME
```

- **Ghidra 12.x** + **JDK 21**（Ghidra 12 不吃 17）+ 带 **pyghidra** 的 python。
  路径一律靠 `tooling/env.sh` 探测——**别在脚本或文档里写死任何一台机器的路径**，
  协作者 clone 下来要能直接跑。装法见 [`docs/SETUP.md`](docs/SETUP.md)。
- **驱动 Ghidra 两条路**：
  1. **MCP（推荐迭代用）**：`ghidra-re`（自维护 fork `Renko6626/re-mcp@thtk-patches`）。
     已在本仓注册（project-local，2026-09-01 实测 41 个 pinned 工具）；
     ⚠️ **注册/改动后要新开会话才加载**，且作用域绑目录——在别的目录开会话调不到。
     工具目录见 [`tooling/ghidra/mcp-tools.md`](tooling/ghidra/mcp-tools.md)，
     装法与自检见 [`docs/SETUP.md`](docs/SETUP.md)。
     **fork 不在本仓维护**——它有自己的仓库和 upstream，随用随取。
  2. **脚本**：`tooling/ghidra/run.sh <exe> <script.py>`（封装好 env 的 pyghidra）。
- **新作一键起库**：`tooling/ghidra/bootstrap.py`（建库 → 分析 → 补建漏掉的函数 → 套 ExpHP
  名/结构体/labels → 回放我们那层 → dump → 落盘）。幂等，随时可重跑。
- ⚠️ **Ghidra 里的成果不导出就等于没有**。工程在 `local/`（gitignored），
  干完活跑 `tooling/ghidra/symbols.py export <版本>` 存进 `games/<版本>/symbols.json`；
  `symbols.py status <版本>` 随时对账。详见 [`tooling/ghidra/README.md`](tooling/ghidra/README.md) 的「两层符号」。
- **想看库里有什么又不抢锁**：`tooling/ghidra/export_html.py <版本>` 导出 HTML
  （对齐 MCP 只读工具能看到的东西）+ `serve.sh <版本>`。工程锁是独占的——
  GUI／MCP／driver 三者同一时刻只能有一个，见 [`docs/SETUP.md`](docs/SETUP.md) 的「锁」。
- ⚠️ **坑**：Ghidra 12 移除了 Jython，`.py` 必须走 **PyGhidra（CPython 3）**，
  不能 `analyzeHeadless -postScript foo.py`；analyzeHeadless 的工程目录**必须绝对路径**。

## 样本

游戏 exe 是 ZUN 版权商业软件，由用户放进 `local/<版本>/`（已 gitignore）。
**没有样本就先问用户，不要去下载。** 现有样本与工程见 [`local/README.md`](local/README.md)
和各作的 `games/<版本>/INDEX.md`。

## ★ 命名金矿（逆向一个新 exe 的第一件事）

`local/vendor/th-re-data`（ExpHP，gitignored）= 逐版本的 **funcs / statics / 结构体**。
`tooling/ghidra/bootstrap.py` 会自动套用（safe 模式，不覆盖已有名）。
**翻它能省掉大量从零命名**——详见 [`engine/_shared/community-sources.md`](engine/_shared/community-sources.md) 的金矿条目。

## ★ 协作纪律：**只做自己那批卡；动到别人的卡必须醒目声明**

本仓是**多人协作**，卡按批次分人：**系统卡 = id 106–117**（红魔乡…鬼形兽，各作系统复刻），
其余（id 58–80 那批：黑桃五张 / 皇家同花顺 / 青眼白龙 / 黄昏 / 加倍 / 腐化 / 损破核心 / 炎魔 /
黄金小丑 / 倒吊人 / 真银圣剑 / 古锭刀 / 豌豆射手 / 衔尾蛇 / 旋风斩 / 股市 …）属于**别人**。

1. **改别人的卡之前先问**；确实必须改（例如共享件升级逼着所有卡跟着改、或审计查出真 bug）时，
   **必须在 [`AUDIT.md` §BE](mods/th18.v1.00a/card-expand/AUDIT.md) 里逐条登记**
   （`文件 → 改了什么 → 为什么 → 谁改的 → 怎么验证的`）+ **在被改的文件里留一段注释**说明来龙去脉；
   作者会自己查这份清单（**不用逐个去问/通知**），但登记**一次都不能省**。
2. **改共享件等于改所有人的卡**：`sdk.h` / `sdk.c` / `sdk_core.*` / `sdk/public.h` / `engine.h` /
   `sites.py` / `Makefile` / `cards_def.c` / `patch/**` / `assets/ability/scripts/**`（脚本的 layer 也算！）
   ——动这些要在报告里单开一节说明「对既有卡的影响面 + 回归跑了什么」。
3. 汇总见 [`mods/th18.v1.00a/card-expand/AUDIT.md`](mods/th18.v1.00a/card-expand/AUDIT.md) 的
   **§BE「非系统卡改动清单（协作须知）」**（每次动别人的卡都往那里追加一行）。

## 落笔纪律（完整版在 METHOD.md）

- 每条结论写全**发现 → 推测 → 验证 → 结论（可信度 + 版本）→ 证据（地址/出处）**。
- 一手反汇编 > 推断 > 社区单源，且必过「领域常识」关。
- **「超过社区」的宣称要过额外闸门**：一手到底 / 对抗证伪 / 量纲常识 / 交叉对名。复核前一律 🟡。
- 结论回流：`engine/<子系统>/<版本>/`（一手过程）→ `engine/<子系统>/OVERVIEW.md`（跨版本断言，附链接）。
- **地址写 `` `0x4919a0` ``（反引号、无 `@`）；文档开头声明默认版本**，跨版本文档里每个地址
  都要带前缀 `th16:`。写完跑 `python tooling/check-docs.py`。细则见 [`DOCSTYLE.md`](DOCSTYLE.md)。
- 涉及**手写机器码 / ABI** 的产出（cave、hook），必须上对抗审计——
  见 [`mods/_template/AUDIT-checklist.md`](mods/_template/AUDIT-checklist.md)。
- **★ 铁律：UI 与动画行为一律先从原代码找。** 动手写任何 UI / 动画 / 特效前，
  必须先在原游戏的 anm 脚本 + 引擎函数里找到它**原本怎么做**（脚本号、地址、数值、触发条件），
  照抄实现；贴图走零售切图，别自己画。原版确实找不到的，标 `❓ 自定` 并登记，**不许包装成「复刻」**。
  完整版与反面教材见 [`METHOD.md`](METHOD.md) 的「铁律:UI 与动画一律先从原代码找」。
- **★ 铁律：长持续效果的卡必须交代「被删掉会怎样」。** 任何跨帧留下东西的卡（自建 VM / 子机槽 /
  HUD 数字 / 伤害源 / 计时器 / 队列 / 全局旋钮）都要在 `on_delete` 里收干净 —— 删卡的路
  （倒吊人 / **原版空白卡**「弃掉整副卡组」）**都发生在商店里**，那一刻 AbilityManager 被冻住，
  卡的 tick 回调全停，只有 `on_delete`（虚表 `+0x50`）跑得到。
  清单与一手机制见 [`METHOD.md`](METHOD.md) 的「铁律:长持续效果的卡必须交代『被删掉会怎样』」。
