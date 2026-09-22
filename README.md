# renkolab

东方 Project 的**引擎逆向工作台**：拆游戏 exe 搞懂运行时语义，再拿这些语义去做 mod。

四层分工，一句话版：

> `engine/` 回答**引擎怎么工作** · `runtime/` 回答**同一份知识的可执行形式** ·
> `mods/` 回答**我改了什么** · `tooling/` 回答**怎么再来一遍**

这个划分不是审美，是因为四种东西的**寿命完全不同**：引擎知识跨版本可蒸馏、只增不改，
寿命最长；`runtime/` 跟着**格式版本**走而不是跟着某一作走——比如一个 ANM v8 播放器要
覆盖 TH13～TH185 一整代，比单个 mod 产物活得久得多，但格式总会翻代（v9…），所以比
`engine/` 短；mod 产物死绑某个 exe build，换个版本就全废，寿命最短；工具链版本无关、
随 Ghidra 自己的版本演进，跟游戏版本不是同一根轴。
它们塞进同一棵树就会互相拖累——这正是本仓库从 `THTK-Studio/research/` 拆出来的原因。

## 目录

```
docs/SETUP.md  ★ clone 下来怎么跑起来(协作者从这开始)
tooling/doctor.py  自检:缺什么 + 怎么补 · tooling/env.sh 探测环境
METHOD.md      ★ 逆向记录纪律 —— 一条结论怎么才算成立(动手前必读)
DOCSTYLE.md    ★ 文档规范 —— 成立之后怎么写下来(地址/路径/版本/表格)
CLAUDE.md        agent 说明:环境、路径、纪律

engine/        知识层 —— 子系统为主，版本为辅
  _shared/       跨子系统:主循环 / 引擎数学+PRNG / THA1 归档 / 社区来源
  player/        自机运行时(生命·季节释放·开火·option·字段图·资源经济)
  bullet/        弹幕引擎(核心 / 运动 VM / 激光)
  ecl/           ECL 敌机脚本 VM(核心已基本反完)
  sht/           SHT 自机配置(TH16 运行时语义已攻下)
  msg/           MSG 对话系统(stage + 结局/staff 两套指令集)
  menu/          MainMenu 状态机
  card/          卡牌/能力系统(TH18 特有)
  anm/           ANM 精灵动画 VM(未系统开工)

games/         每作一页薄导航:样本、Ghidra 工程、覆盖统计、待挖图
runtime/       引擎知识的可执行形式 —— ★ ANM v8 播放器已拆成独立仓库 live-anm，见 runtime/README.md
mods/          产物层 —— 版本为主，死绑 build
tooling/       工具层 —— Ghidra 工具链
local/         gitignored:exe / 资产 / Ghidra 工程 / vendor 克隆
```

## 从哪开始

| 你想干什么 | 先读 |
| --- | --- |
| **刚 clone 下来** | `python3 tooling/doctor.py` → [`docs/SETUP.md`](docs/SETUP.md) |
| 接着逆向 | [`METHOD.md`](METHOD.md) → 对应 `engine/<子系统>/OVERVIEW.md` → 该版本的一手文档 |
| 开一个新作 | [`games/`](games/) 看已有两作的登记格式 → [`tooling/ghidra/README.md`](tooling/ghidra/README.md) 建库 |
| 做 mod | [`mods/README.md`](mods/README.md) → [`mods/_template/`](mods/_template/) |
| 查某个格式/社区结论 | [`engine/_shared/community-sources.md`](engine/_shared/community-sources.md) |
| **问「这一作原版有没有 X」之前** | ★ [`engine/_shared/original-common-sense.md`](engine/_shared/original-common-sense.md)（原版常识层 + 「三查」硬规矩；**答案在里面就不许问 owner**）|

## 每个子系统怎么读

`engine/<子系统>/OVERVIEW.md` 是门面：一段散文讲跨版本模型，加一张**断言 × 版本矩阵**，
标出哪条断言在哪个版本**一手验过**、哪条只是从别的版本借来的**待验假设**。
矩阵每一格都链回确立它的那份一手文档。

`engine/<子系统>/<版本>/` 下面才是一手证据：地址、偏移、反编译依据、可信度分级。

**OVERVIEW 里不许出现没有出处的断言。** 从 th16 借到 th18 的判断一律标 🟡，
在该版本 exe 上验过才能改 ✅——这条纪律的完整版在 [`METHOD.md`](METHOD.md)。

## 纪律

- **仓库不留任何版权字节。** 游戏 exe、解包资产、Ghidra 工程、大段反编译原文一律只在 `local/`。
- 结论按 `METHOD.md` 的**发现 → 推测 → 验证 → 结论(可信度+版本) → 证据(地址/出处)** 五段链记。
- 一手反汇编 > 推断 > 社区单源；「超过社区」的宣称要过额外闸门。
- 不确定标 🟡/❓。**宁可少宣称，不可假宣称。**
- 写完跑 `python tooling/check-docs.py`（地址格式 / 版本声明 / 链接 / 表格四项）。
  规范见 [`DOCSTYLE.md`](DOCSTYLE.md)——**一条不被脚本检查的规范，半年后就是考古材料**。

仓库当前私有，但按可公开的纪律写——随时可以切公开。
