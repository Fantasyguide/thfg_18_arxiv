# SETUP — clone 下来怎么跑起来

给**第一次上手的人**（包括未来的你自己、协作者、新会话的 agent）。

本仓不写死任何一台机器的路径：环境靠 `tooling/env.sh` 探测，缺什么由
`tooling/doctor.py` 逐条告诉你。**先跑自检，再照它说的补**，别照着文档一条条手配。

```bash
git clone <repo> renkolab && cd renkolab
python3 tooling/doctor.py
```

它会打一张表，全绿就能开工；有 ❌ 就往下看对应那节。

## 装什么

### Ghidra 12.x

从 [releases](https://github.com/NationalSecurityAgency/ghidra/releases) 下载解压即可，
**不需要 sudo**。放进 `~/opt/` 下探测得到（`~/opt/ghidra_*_PUBLIC`），
其他位置就自己 `export GHIDRA_INSTALL_DIR=<解压路径>`。

⚠️ **必须是 12.x**。本仓脚本按 12 写——12 移除了 Jython，`.py` 只能走 PyGhidra（CPython 3），
`analyzeHeadless -postScript foo.py` 会报 "Ghidra was not started with PyGhidra"。

### JDK 21 + python + pyghidra

一个 conda 环境同时解决三样：

```bash
conda create -n ghidra -c conda-forge openjdk=21 python=3.11
conda activate ghidra && pip install pyghidra
```

⚠️ **必须 JDK 21+**，Ghidra 12 不吃 17。`tooling/env.sh` 会**逐个候选校验版本号**——
本机就踩过：shell 里 export 着一个 JDK 17，无条件信 `$JAVA_HOME` 就会起不来，
而报错信息完全看不出是版本问题。所以探测会跳过版本不够的，继续往下找。

### 样本（要你自己准备）

游戏 exe 是 ZUN 版权商业软件。**用你自己合法持有的副本，不要去下载，也不要问仓库要。**
按 [`../local/README.md`](../local/README.md) 的哈希表放进 `local/<版本>/`。
`doctor.py` 会算 md5 给你核对。

### ExpHP 的 th-re-data（强烈建议）

逆向新 exe 时最值钱的东西——逐版本的 funcs / statics / 结构体 / labels。
没有它 `bootstrap.py` 只能建个裸库。

```bash
git clone https://github.com/exphp-share/th-re-data local/vendor/th-re-data
```

⚠️ 该仓库**无 LICENSE**：本地逆向随便用，但**不擅自转发**。所以它在 `local/` 下不入库，
`symbols.py export` 也会把与它逐字相同的条目全部剔掉。

### thtk（解 dat / 反 anm；做资产 mod 必备）

thtk 发布页只有 Windows exe，Linux 上自己编。没 sudo 也行——缺 bison/flex/m4 时脚本用
`apt-get download` 拉 .deb 解到 `local/vendor/bisonflex/` 直接用：

```bash
bash tooling/thtk/build.sh                   # → local/vendor/thtk/build/{thanm,thdat}/
python3 tooling/thtk/unpack.py th18.v1.00a   # 需要你放好 local/th18.v1.00a/th18.dat
```

指令助记符靠 anmmap：clone thpages（`local/vendor/thpages`，`static/mapfile/v8.anmm`）。
细节与坑见 [`../tooling/thtk/README.md`](../tooling/thtk/README.md)。

### ghidra-re MCP（可选，但强烈建议）

让 agent 直接驱动 Ghidra，免去「写脚本 → 跑 → 解析 stdout」。用的是自维护 fork
（上游 `jtsylve/ida-mcp` 基本不维护）。

```bash
uv tool install --force \
  "git+https://github.com/Renko6626/re-mcp@thtk-patches#subdirectory=packages/re-mcp-ghidra"

cd <仓库根>                                  # ★ 作用域绑目录，必须在本仓下注册
source tooling/env.sh
claude mcp add ghidra-re \
  -e GHIDRA_INSTALL_DIR="$GHIDRA_INSTALL_DIR" -e JAVA_HOME="$JAVA_HOME" \
  -- "$(command -v re-mcp-ghidra)" stdio
```

**三个坑，每个都真踩过：**

1. ⚠️ **注册完当前会话仍然调不到，必须新开会话**才加载。
2. ⚠️ **作用域是 project-local**，注册在哪个目录就只在那个目录的会话里可用。
   本仓从 THTK-Studio 拆出来后，它一直挂在旧仓库名下，导致「文档说有、实际没有」——
   而文档骗人比没文档更糟。
3. ⚠️ **fork 不在本仓维护**，它有自己的 remote 和 upstream。
   [`../tooling/ghidra/patches/`](../tooling/ghidra/patches/README.md) 是补丁**存档**，
   不是要你手动打——修复已经烘进 fork 分支了。

该 MCP 有 ~90 个工具但只 pin 一小撮给客户端，其余是 hidden：不在工具列表里 ≠ 不存在，
用 `search_tools(关键词)` 发现、`call`/`batch`/`execute` 按名调。
目录见 [`../tooling/ghidra/mcp-tools.md`](../tooling/ghidra/mcp-tools.md)。

## 起库

```bash
source tooling/env.sh
"$JAVA_HOME/bin/python" tooling/ghidra/bootstrap.py th18
```

幂等，随时可重跑。九步做了什么、为什么是这个顺序，见
[`../tooling/ghidra/README.md`](../tooling/ghidra/README.md)。

## 日常

```bash
# 看库里有什么（不抢锁）
"$JAVA_HOME/bin/python" tooling/ghidra/export_html.py th18
tooling/ghidra/serve.sh th18            # 只绑 127.0.0.1；远程用 ssh -L 转发

# 按 VA 直读 exe 字节（跳表 / 常量池），不占工程锁
python3 tooling/pe_read.py local/th16.v1.00a/th16.exe 0x45f578 --dwords 36 --base 10000

# 干完活把成果存回仓库 —— 不导出就等于没有
tooling/ghidra/symbols.py status th18
tooling/ghidra/symbols.py export th18

# 写完文档
python tooling/check-docs.py
```

## ★ 锁：三条路线互斥，同一时刻只能有一个

Ghidra 的工程锁**工程级独占**，只读打开程序也得先开工程。

| 谁占着锁 | 后果 |
| --- | --- |
| MCP 开着库（`open_database` 之后） | 所有 driver 脚本开不了库 |
| Ghidra GUI 开着工程 | MCP 和 driver 都开不了 |
| driver 正在跑 | MCP 那边会失败 |

所以**「人在 GUI 里翻代码」和「模型在跑分析」不能同时发生**。协调办法：

- 跑 driver / bootstrap 前，先在 MCP 里 `close_database`。
- 想看库里有什么又不抢锁 → 用 HTML 导出（导出时占一次锁，之后随便翻都不占）。
- driver 撞锁时会给人话提示，不会甩一句 Java 异常。
- 上次 driver 被 kill 留下残锁：删 `local/<版本>/ghidra_projects/<工程>.rep/*.lock`。

## 两条流程，成熟度不一样

**逆向流程：可用。** `bootstrap` 起库 → MCP 或 HTML 看 → 在 `engine/<子系统>/<版本>/`
按 [`../METHOD.md`](../METHOD.md) 的五段证据链落结论 → `symbols.py export` 把命名/类型
存回仓库。每一环都有工具和检查。

**mod 流程：只有纪律，缺工具。** [`../mods/README.md`](../mods/README.md) 定了
`TARGET.md`（死绑登记）+ 对抗审计的规矩，`th16.v1.00a/tracking-laser` 也有完整的
cave 源码与 thcrap patch 文档——但**状态是「静态审计通过，未实跑」**。缺的是：

| 缺什么 | 后果 |
| --- | --- |
| 汇编/编译 cave 的构建步骤 | `.asm`/`.c` 到机器码这步得手工，没有可复现脚本 |
| `expected` 字节的自动核对 | `TARGET.md` 里的原字节靠手抄，没工具对着 exe 验 |
| 实跑环境 | Windows 游戏，Linux 上要 Wine；本仓没验证过 |

所以现在能**设计并静态审计**一个 mod，不能**一键构建并验证**它。
接新坑前先知道这条边界在哪。

## 附录：本机实测值（zhustation，2026-09-01）

**你的机器不会长这样**，这里只是给「跑通之后应该是什么样」一个参照：

| 项 | 值 |
| --- | --- |
| Ghidra | 12.1.2，`~/opt/ghidra_12.1.2_PUBLIC/` |
| JDK / python | conda 环境 `ghidra`：openjdk 21 + python 3.11 + pyghidra 3.1.0 |
| MCP | fork `re-mcp 3.0.1`，握手实测 41 个 pinned 工具 |
| th18 库 | 函数 2333 / 已命名 1366；ExpHP `skipped=874 missing=1`；labels 492；结构体 157 |
| 我们那层 | `games/th18.v1.00a/symbols.json` 200 条 |
