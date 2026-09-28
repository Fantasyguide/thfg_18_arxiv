# tooling/ghidra — Ghidra 工具链

版本无关的逆向工具。目标是把「拿到一个新 exe → 有一个可用的、已命名的 Ghidra 库」
这件事从半天压到一条命令。

## 一键起库

```bash
source tooling/env.sh                                  # 探测环境，不写死路径
"$JAVA_HOME/bin/python" tooling/ghidra/bootstrap.py th18
```

[`bootstrap.py`](bootstrap.py) 串起九步，末尾打印一段可直接粘进
`games/<版本>/INDEX.md` 的登记文本：

```
0  漂移拦截：有未导出的成果就中止          （仅 --reanalyze，见下节）
1  建库 + headless 分析
2  补建 ExpHP 标了、而 Ghidra 没建函数的地址   create_missing_funcs.py
3  套 ExpHP 函数名 / 静态符号                  import_th_re_data.py
4  套 ExpHP 结构体 / 枚举 / 位域 / statics 类型 import_th_re_data_structs.py
5  套 ExpHP labels（VM opcode case）           import_th_re_data_labels.py
6  认 Shift-JIS 字符串                         import_sjis_strings.py
7  回放我们自己那层（覆盖，压过 ExpHP）        symbols.py apply
8  dump 函数清单                               dump_funcs.py
9  汇总
```

顺序不是随意的：第 2 步必须在第 3 步前，否则只经 vtable 进入的回调没有函数对象，
名字落不上（th18 实测漏 63 个，含 `Player::on_tick`）；第 5 步要求函数已存在；
第 7 步必须垫底——我们那层的存在意义就是订正 ExpHP。

第 6 步单说：**Ghidra 的字符串分析器对 Shift-JIS 基本无能为力**，th18 里 22 条日文串
（标题、字体名、卡牌/实绩文案模板、音乐室剧透警告）一条都没被认出来，还有几条被当成
float 显示成 `5.16662e+30` 那种荒唐数值。判据是「解码后按 Unicode 区段判 + 前一字节必须是
NUL + 至少 6 字节」——前两条排掉 .rdata 浮点表里碰巧解得通的巧合，第三条排掉最后 3 个
两字符的假阳性。落库后 HTML 和 MCP 的 `get_strings` 同时可见。

路径按约定推导（见 [`_driver.py`](_driver.py) 的 `resolve`），不用逐个指定。
工程已存在时默认跳过分析（幂等），要重来加 `--reanalyze`；`--dry-run` 只预览计数、不写库。

TH18 上的实测基线（2026-09-01，全量重建后）：`existed=728`、`skipped=874 missing=1`、
`types=157 enums=2 bitfields=2 statics=151 failed=0`、`labels skipped=492`、
`sjis found=22 failed=0`、2333 个函数。
`created=0 applied=0` 正是幂等的证据。

## 两层符号：一层重放，一层往返

Ghidra 工程在 `local/`，而 `local/*` 是 gitignored 的（仓库不留版权字节）。
所以**你在 Ghidra 里干的活默认存不住**——换台机器就没了，`--reanalyze` 一跑也没了。
[`symbols.py`](symbols.py) 就是补这个洞的。

| | ExpHP 层 | 我们那层 |
| --- | --- | --- |
| 来源 | `local/vendor/th-re-data/`（gitignored） | 你在 Ghidra 里干的活 |
| 方向 | 只进不出 | 双向 |
| 落盘 | 不入库 | `games/<版本>/symbols.json`，**入库** |

```bash
tooling/ghidra/symbols.py status th18   # DB ⇄ 仓库对 diff，两个方向都报
tooling/ghidra/symbols.py export th18   # DB → 仓库（干完活记得跑）
tooling/ghidra/symbols.py apply  th18   # 仓库 → DB（bootstrap 第 7 步自动跑）
```

**只导出「我们的」**：`th-re-data` 上游无 LICENSE，不擅自转发。导出时现场与 ExpHP 数据
diff，逐字相同的一律剔掉；Ghidra 各分析器自产的东西（`switchdataD_*` 标签、
`Library Function -` 注释、RTTI/vftable 符号）靠 `SourceType != USER_DEFINED` 挡掉。
th18 实测：DB 里 3724 个候选筛到 **200 条真·我们的**。

**函数原型 / 调用约定 / 全局类型绑定一律留下**，因为 ExpHP 根本不给这些——
而正是绑定决定了反编译好不好读：`*(int *)((int)param_1 + 0x3c)` 绑上类型之后是
`(self->__id_3c).id`。

⚠️ **导出是手动的**，唯一的安全网是 `bootstrap.py --reanalyze` 前的漂移拦截：
有没导出的东西就中止。平时自己记得跑 `status`。

## 类型绑定：让反编译从 `param_1 + 0x54` 变成 `self->flags`

ExpHP 给名字和结构体布局，**不给绑定**——哪个函数的哪个参数是哪个类型。没绑之前，
`zCardBaseClass` 躺在库里没人用，反编译全是裸偏移。[`bind_types.py`](bind_types.py) 补这个洞。

```bash
tooling/ghidra/bind_types.py th18                # 出计划，不写库
tooling/ghidra/bind_types.py th18 --sample 6     # 再附 6 个函数绑定前后的反编译对照
tooling/ghidra/bind_types.py th18 --apply        # 应用 tier 1
tooling/ghidra/bind_types.py th18 --revert       # 撤销我们下过的蛋
```

**规则是数据**，在 [`bindings/<版本目录名>.json`](bindings/th18.v1.00a.json)，入库、可 diff、可 review：

| 字段 | 干什么 |
| --- | --- |
| `vtable_rules` | tier 1。签名**逐字取自 ExpHP 的 vtable 成员注释**，函数名后缀 == 槽名 |
| `slot_param_overrides` | 我们的一手结论**压过** ExpHP 的注释（它不是处处可信，见下） |
| `subclass_structs` | 给比基类大的子类建「基类字段 + 尾部填充」的结构体 |
| `this_rules` | tier 2。只绑 this，其余参数保持 Ghidra 推断 |
| `ambiguous` | **故意不绑**的族 + 理由 |

### 覆盖面与局限（th18 实测，2026-09-01）

| | 数量 | 占比 |
| --- | --- | --- |
| 已绑函数（tier 1） | **268** | 全库 2333 个函数的 **11.5%** |
| 涉及类 | 58 个 `Card*` 类（含基类） | — |
| 覆盖 vtable 槽 | **21 / 21**（每槽都有实例） | — |
| 新建子类结构体 | **31**（含复用 ExpHP 已有的 `zCardMomoyo`） | — |
| tier 2 已就绪未开 | 再 318 个 | 开了合计 586 ≈ 25% |

槽分布的大头是样板方法（`operator_delete` 58 / `method_4C` 52 / `__on_load__2` 43），
真正有肉的只有 `__on_tick_2` 16 / `c_press` 13 / `on_tick_shooters` 12。**别把 268 当成"读懂了 268 个函数"。**

**知道自己没做什么**：

1. **只覆盖卡牌，88.5% 没动**。因为 ExpHP 只给了 `zVTableCard` 一个 vtable 的逐槽签名，
   另外 4 个（`zVTableBomb`/`Delete`/`Ecl`/`Laser`）注释全是 `void*`，取不到——**这套方法天生只能吃卡牌**。
2. **`Player__*` 故意不绑**，tier 2 那 318 个也没开：它们的 this 类型是**按函数名前缀猜的**，
   没有逐槽签名兜底，得每族抽验才敢开。
3. **子类字段仍是 `self->field_0x58`**：子类结构体只有「基类字段 + 尾部填充」，`0x54` 以上没命名。
4. **绑定不会让上游的名字变对**：`+0x34` 现在显示成 `__timer_2__prolly_bomb_time`，
   而一手结论是它才是充能倒计时（`engine/card/th18/01-object-model.md` §3）。**更好读 ≠ 更正确。**
5. **ExpHP 的签名可能还有错**：21 个槽里只有 5 个的调用点被一手验过参数，抓到 2 处错就是在这 5 个里。
6. **4 个 `Card*` 函数在计划外**（真·辅助函数 + `CardMomoyo__on_bullet_init` 疑似槽 0x28 但没逐字核对）。


### 三件必须知道的事

1. **回退有三层**。①`--revert` 按 [`games/<版本>/bindings.json`](../../games/th18.v1.00a/bindings.json)
   逐条恢复成 Ghidra 自动推断，**应用之后被人改过的一律跳过并报出来**；
   ②绑定会被 `symbols.py export` 吸进 `symbols.json` 并入库，`git checkout` 它
   再 `bootstrap.py --reanalyze` 就是干净重建；③默认**不覆盖既有人工签名**，要压得显式 `--force`。
   幂等判据是「这次想要的 == 上次想要的」**且**「库里现在的 == 上次写完读回来的」，
   所以改了规则再跑会真的重绑，没改就是 no-op。

2. **ExpHP 的 vtable 签名会错**。实证两处：`zVTableCard.on_player_death_after_deathbomb`
   的第二参标成 `struct zPlayer*`，一手是**救命累加器**（`Player__on_tick__body` case 4 是
   `acc |= vtable[0x0c](acc)`）；`recharge` 槽的两个 `int32_t` 实为
   `(float *掉落坐标, int *掉落计数表)`。这就是 `slot_param_overrides` 存在的理由——
   **别把上游数据当 ground truth**。

3. **子类结构体不是可选项**。只绑 `zCardBaseClass *`(0x54) 的话，子类自有字段会被 Ghidra
   渲染成 `self[1].card_id`——那不是"读着别扭"，那是**错的**（真身是 `card+0x58`，一个浮点坐标）。
   建了同名带填充结构体之后显示成 `self->field_0x58`，偏移可回推，不骗人。
   大小取自 `AbilityManager__allocate_new_card` 每个 case 的 `operator new` 实参。

⚠️ 改了 `zCardBaseClass` 的布局要 `--apply --force` 重跑，子类结构体不会自己跟。

## 看库里有什么：HTML 导出（不抢锁的那个视图）

Ghidra 的工程锁是**工程级独占**的，只读打开程序也得先开工程。所以三条路线互斥：
**GUI 开着，driver 和 MCP 都进不去；MCP 开着库，driver 进不去。**
也就是说「你在 GUI 里翻代码」和「模型在跑分析」不能同时发生。

[`export_html.py`](export_html.py) 把两者解耦——**导出时占一次锁，之后你随便翻都不占**。

```bash
tooling/ghidra/export_html.py th18     # 全量导出 -> local/th18.v1.00a/state/（gitignored）
tooling/ghidra/serve.sh th18 [端口]     # http.server，只绑 127.0.0.1，默认 6090
# 你那边：ssh -L 6090:localhost:6090 <你>@<机器> → 浏览器开 localhost:6090
```

导出内容对齐 `ghidra-re` MCP 那批只读工具，目标是**你在 HTML 里看到的 ≈ 模型调 MCP
能看到的**：

| MCP 工具 | HTML 里对应 |
| --- | --- |
| `decompile_function` | 每函数页的反编译 C（**模型读到的就是这个**） |
| `disassemble_function` | 全量反汇编 + 入口前 6 条 |
| `get_xrefs_to` / `get_call_graph` | 调用者 / 被调用，双向可点 |
| `list_decompiler_variables` | 局部变量与参数表 |
| `read_bytes` | 入口前 16 字节（定 hook 点用） |
| `list_structures` / `get_structure` | `structs.html` 全字段图 |
| `get_strings` | `strings.html`，按引用数排序（含认出来的 22 条日文串） |
| `list_functions` / `list_names` | `index.html` 全库检索（正则，纯客户端） |

还多给一样 MCP 没有的：**每个名字是谁给的**——`exphp` / `ours`（进了 symbols.json）/
`auto`（Ghidra 的 FunctionID、demangler 猜的，别当结论）/ `none`。这决定了你该信它几分。

附带 `state/data/functions.jsonl`（一行一个函数，含反编译文本）给模型冷启动读——
没有 MCP 时也能先知道库里有什么。

## 环境（无 sudo，用户空间，已验证）

装法与自检见 [`docs/SETUP.md`](../../docs/SETUP.md)；`python3 tooling/doctor.py` 会逐条
告诉你缺什么。要点：**Ghidra 12.x + JDK 21**（不吃 17）+ 带 pyghidra 的 python，
路径一律由 [`../env.sh`](../env.sh) 探测。

⚠️ **两个坑**：

1. **Ghidra 12 移除了 Jython**，`.py` 必须走 PyGhidra（CPython 3）。
   `analyzeHeadless -postScript foo.py` 会报 "Ghidra was not started with PyGhidra"。
2. **analyzeHeadless 的工程目录必须是绝对路径**，不接受以 `.` 开头的相对路径。

## 目录

| 文件 | 用途 |
| --- | --- |
| [`bootstrap.py`](bootstrap.py) | ★ 一键起库（上文） |
| [`symbols.py`](symbols.py) | ★ 我们那层的往返：`export` / `apply` / `status`（下节） |
| [`export_html.py`](export_html.py) | ★ 把库状态导成可离线翻的 HTML（不抢锁的那个视图） |
| `serve.sh` | 端出上面的产物，只绑 `127.0.0.1` |
| [`_driver.py`](_driver.py) | 所有 driver 共用的开库/落盘骨架 + 路径推导 |
| [`th-re-data.md`](th-re-data.md) | ★★ ExpHP 符号金矿的用法与待挖地图说明 |
| [`create_missing_funcs.py`](create_missing_funcs.py) | 在 ExpHP 标了函数、Ghidra 没建函数的地址上补建 |
| [`import_th_re_data.py`](import_th_re_data.py) | 套 funcs/statics 名字 + 注释（safe，不覆盖已有名） |
| [`import_th_re_data_structs.py`](import_th_re_data_structs.py) | 套结构体/枚举/位域/typedef + statics 类型 |
| [`import_th_re_data_labels.py`](import_th_re_data_labels.py) | 套 labels.json：VM dispatch 里的 opcode case 标签 |
| [`import_sjis_strings.py`](import_sjis_strings.py) | 认 Shift-JIS 字符串（Ghidra 的分析器认不出日文） |
| [`dump_funcs.py`](dump_funcs.py) | 导出当前命名状态到 JSON |
| [`build_worklist.py`](build_worklist.py) | 对比 ExpHP 与本地命名，算出「谁都没命名的处女地」 |
| [`apply_th16_thredata_bulk_names.py`](apply_th16_thredata_bulk_names.py) | TH16 批量导名（历史脚本） |
| [`th20_anchors.py`](th20_anchors.py) | TH20：字符串 → 引用它的函数（assert 锚点），出 JSON |
| [`th20_rtti.py`](th20_rtti.py) | TH20：MSVC RTTI 复原（类名 → COL → vftable → 逐槽） |
| [`th20_names.py`](th20_names.py) | TH20：清 FID 假阳性 + 套 `Class::vftable` + 钉 assert 注释 |
| [`th20_decomp_corpus.py`](th20_decomp_corpus.py) | TH20：按地址区间把反编译落成语料库（**不占锁地并行分析**） |
| [`th20_show.py`](th20_show.py) | TH20：从语料库里取函数 / grep / 找调用方（**不占锁**） |
| [`th20_read.py`](th20_read.py) | TH20：开库做反编译/反汇编/xref/读字节（**占锁，一次一个**） |
| [`mcp-tools.md`](mcp-tools.md) | `ghidra-re` MCP 工具目录（pinned / hidden） |
| `run.sh` | 对单个二进制跑一个 pyghidra 脚本 |
| `scripts/` | 逐子系统的命名固化脚本（headless 可复现） |
| `patches/` | 我们给 MCP fork 打的补丁（见下） |

```bash
# run.sh <binary> <script.py> [args...]
tooling/ghidra/run.sh local/th18.v1.00a/th18.exe tooling/ghidra/scripts/list_functions.py
```

`scripts/` 里的 `apply_th16_*_names.py` 是**唯一能给数据符号真改名**的途径
（MCP 的 rename 对数据符号需经 driver 落盘），且 headless 可复现。

## 没有社区金矿的新作（TH20 / TH19）：RTTI + assert 锚点 + 反编译语料库

> ⚠️ 这一节的脚本文件名叫 `th20_*`，但**全都是版本参数化的**（`--version th19` / `--version th09.v1.50a` …），
> 名字只是历史遗留（先给 TH20 写的）。TH19 用的是同一套链子。

TH20《錦上京》2025-08 才出，`local/vendor/th-re-data/th20.v1.00a` **不存在**（TH06–TH18.5 都有），
所以 `bootstrap.py` 的第 2–5 步全空转，库里 7315 个函数只剩 C++ 库的 FID 名
（`games/th20.v1.00a/INDEX.md` §2c 记了这条限制）。但 ZUN 把**两样命名金矿**编进了 exe：

1. **MSVC RTTI**：`.?AVStoneRedReimuInf@@` 这类 type descriptor ⇒ 类名；
   顺着 `_TypeDescriptor(+8=名字) → RTTICompleteObjectLocator(+0xc 指向它) → vftable[-1]` 走，
   拿得到 **79 个 ZUN 自己的类**（`StoneRedReimuInf` / `WeaponStoneInf` / `StoneEnemyCtrlInf` …）。
2. **assert 字符串**：`D:\cygwin\home\zun\prog\th20\src\game\weaponred.cpp:42 StoneRedReimuInf`
   —— 文件 + 行号 + token，每条只被一个函数引用 ⇒ 那是它的归属（156 条，其中 125 条落在函数上）。
   它同时是一张**源码文件地图**：`src\game\stoneenemy.cpp` / `weaponstone.cpp` / `effect\efflinestone.cpp` …

```bash
# 1) 三类锚点：字符串引用表 + 符号表   → local/th20.v1.00a/th20-anchors.json
python3 tooling/ghidra/th20_anchors.py
# 2) RTTI 复原（类名 → vftable → 逐槽函数） → local/th20.v1.00a/th20-rtti.json
python3 tooling/ghidra/th20_rtti.py --grep '.' --json local/th20.v1.00a/th20-rtti.json
# 3) 写库：清 FID 假阳性 + 类 vftable 标签 + assert 板注释
python3 tooling/ghidra/th20_names.py --dry-run     # 先看计划
python3 tooling/ghidra/th20_names.py --apply
```

**★ 并行分工的唯一正确姿势**：**Ghidra 工程锁是独占的**（实测两个只读 driver 也会撞锁），
所以「几个 agent 同时挖同一个 exe」不能各开一个库。做成两步：

```bash
# 一个人（占锁）：把要挖的地址区间反编译落盘 → local/th20.v1.00a/decomp/*.c + INDEX.tsv
python3 tooling/ghidra/th20_decomp_corpus.py
python3 tooling/ghidra/th20_decomp_corpus.py --range "weapon:0x52f000-0x539000" --list
# 其余人（不占锁）：读语料库 —— 取函数 / grep / 找调用方
python3 tooling/ghidra/th20_show.py 0x5120d0
python3 tooling/ghidra/th20_show.py --grep "StoneAttack"
python3 tooling/ghidra/th20_show.py --list --range 0x511000-0x515000
```

常量与数据表**不必开库**：`tooling/pe_read.py local/th20.v1.00a/th20.exe 0x573ff0 --dwords 20`
（注意 TH20 的 `.rdata`/`.data` 指针**在文件里是 0、靠重定位填**，指针表要用
`th20_read.py --bytes` 经 Ghidra 读，标量常量可以直接读文件）。

三个已踩过的坑（都写进脚本注释了）：

- **jpype 会把 `bytearray` 拷成 Java 数组**：`mem.getBytes(addr, buf)` 回填读不到，
  必须自己 `JArray(JByte)(n)` 再造回来，否则**静默全零**。
- **别信 `MemoryBlock.isInitialized()`**：TH20 的 `.rdata`/`.data` 在这工程里报 `False`，
  信它就会把 vftable/COL 全扫掉，只剩 `.text`。
- **`vftable[-1]` 才是 COL 指针**：找到「存 COL 的那一格」后起点在它**后面** 4 字节。
- Ghidra 的 FID 分析器在 ZUN 代码上打了 **73 处假阳性**（名字里带 `FID_conflict:`），
  不但错，还会让反编译把真函数体前面糊上 ~7 KB 的库函数说明 —— `th20_names.py` 负责清掉。

## MCP：让 Claude Code 直接驱动 Ghidra（推荐用于迭代）

`ghidra-re` 把反编译/反汇编/xref/struct/搜索暴露成 MCP 工具，免去
「写脚本 → 跑 → 解析 stdout」。用的是**自维护 fork**
`Renko6626/re-mcp@thtk-patches`（上游 `jtsylve/ida-mcp` 基本不维护）。

```bash
uv tool install --force \
  "git+https://github.com/Renko6626/re-mcp@thtk-patches#subdirectory=packages/re-mcp-ghidra"

source tooling/env.sh
claude mcp add ghidra-re \
  -e GHIDRA_INSTALL_DIR="$GHIDRA_INSTALL_DIR" -e JAVA_HOME="$JAVA_HOME" \
  -- "$(command -v re-mcp-ghidra)" stdio
```

- **fork 不在本仓维护**——它有自己的仓库和 upstream remote，吸收进来会打断跟上游 rebase、
  那条安装 URL、以及同仓的 `re-mcp-ida`/`re-mcp-core` 两个包。随用随取。
  [`patches/`](patches/README.md) 留着我们打过的补丁存档。
- ✅ **已在本仓注册**（project-local，2026-09-01 握手实测 41 个 pinned 工具）。
  装法、自检与三个坑见 [`docs/SETUP.md`](../../docs/SETUP.md)。
- ⚠️ **新增/改动 MCP 注册后要新开一个会话才会加载**；当前会话注册后仍调不到。
  且**作用域绑目录**——在别的目录开会话调不到（拆库后它一度挂在旧仓库名下）。
- ⚠️ **跑 `dump_funcs.py` 等 driver 前先 `close_database`**，否则撞工程锁。
- 这是**我们 dev 期逆向用的 MCP**，与 THTK-Studio 自己要 ship 的 MCP 服务器无关
  （local scope，不写进项目 `.mcp.json`）。

## 样本

游戏 exe 是 ZUN 版权商业软件，**不在仓库、不要下载**。放进 `local/<版本>/`，
见 [`local/README.md`](../../local/README.md)。exe 本体无壳（加密在 `.dat`），Ghidra 可直接吃。

## 产出去向

| 产物 | 去处 |
| --- | --- |
| 反编译 dump、跳转表导出等大文件 | `local/`（gitignore） |
| 结论（索引→行为表、字段图） | `engine/<子系统>/<版本>/`，见 [`METHOD.md`](../../METHOD.md) |
| 可复用脚本 | 本目录（入库） |

仅对**用户自有**游戏做互操作性逆向；产出是格式语义文档与工具支持，不分发游戏代码或资产。
