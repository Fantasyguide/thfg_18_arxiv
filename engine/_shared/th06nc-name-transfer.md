# 给 TH06NC 批量迁名：四种手法的实测结果

> **版本**：跨版本。地址带前缀（`th06nc:0x1400020b0` / `th06:0x00411390`）。

TH06NC 有 2816 个函数。本文记录**四种批量迁名手法的实测**——两条否定、两条成立
（§3 与 §4 其实是同一个坐标系的两轮，第二轮把输入扩大了一个量级）——
以及一条比技术更要紧的**许可证约束**。

写下否定结果是为了**不再重复投入**：这几条都花了真金白银的测量。

---

## 0. 先说一条**被我写错过**的约束

> ⚠️ **本节原先写的是「把 TH06 的名字搬到 NC 被许可证挡死」——错的，已更正。**
> 它与 [`community-sources.md`](community-sources.md) 里早已记下的
> 「用户确认可研究/移植，正式发布前建议注明出处」直接冲突。

TH06 原版工程里有 **1267 个已命名函数（90.8%）**，NC 只有 4.4%。这个不对称是真的。
那些名字来自 ExpHP 的 `th06-re` XML（风格 `AsciiManager__on_tick_01`），无 LICENSE 文件。

**但「用一个名字」和「转发整份数据集」是两回事**：前者是短的事实性标签，
后者才是 `不入库、不转发` 那条规矩针对的汇编形态。而**迁名到 NC 不涉及后者**——
TH06 的地址一个都不搬，共享的只有词汇，地址与验证都在 NC 上自己做。

所以**批量迁名不可行的真正原因是技术性的**（§1、§2 实测），
不是许可证。仓库层面「不往 th06nc 的库里导上游名」是一条**政策选择**
（保持来源可分 + 那批名字只能算线索），理由见
[`community-sources.md`](community-sources.md) 的同名小节。

不过下面这条推论仍然成立，而且更重要：

> **要找的不是「TH06 ↔ NC 的函数对应」，而是「CC0 源码 ↔ NC 二进制」的坐标系。**

因为 CC0 的 `th06-decomp` 给的是**语义**（每个 opcode 调什么、每条消息在哪个函数里发），
而 `th06-re` 只给「叫什么」。语义才能在 NC 自己的字节上验证——
这也正是 §3 的日志消息手法能成、而 §1/§2 不能成的原因。

## 1. ✗ 常量签名（实测失败）

**想法**：两边玩法数值零偏差（已验证 13 个常量全中，见
[`../player/th06nc/01`](../player/th06nc/01-main-loop-and-collision.md) §2.14/§2.16），
那就用「函数里出现的浮点常量集合」当指纹配对。

**做法**：capstone 逐函数反汇编，收集内存操作数指向的 4 字节浮点值，
按集合相等 + 双向唯一配对。以「两边同名」的 25 对函数作 ground truth。

**结果**：

| 签名长度门限 | 双向唯一配对 | ground truth 命中 |
| --- | --- | --- |
| ≥ 2 | 5 组 | **0** |
| ≥ 3 | 5 组 | **0** |
| ≥ 4 | 3 组 | **0** |

**诊断（关键）**：直接量已知正确的 25 对，Jaccard ≥ 0.5 的**只有 4 对**。
大多数函数只有 0–3 个浮点常量——11 对里至少一侧 ≤ 1 个。
能配上的全是常量密集的 bomb 函数（6–9 个）。

**结论**：作为批量手法**不成立**。信号本身不够，不是阈值没调好。
对「常量密集的数学/特效函数」仍有零星价值，但不值得为此建工具。

## 2. ✗ 调用图自举（实测失败）

**想法**：用两边同名的函数当种子，若 NC 函数 X 与 TH06 函数 Y 的
「已配对被调集合」相同且双向唯一，就配对；迭代传播。

**结果**：34 个种子 → **只多出 1 个**，且那 1 个是 `th06nc:0x1400373b0`，
正是我们此前手工定位的 `EnemyManager__OnUpdate`。第 2 轮 0 新增，收敛。

补了 10 个新名字后重测：种子 34 → 37，**结果不变**，没有级联。

**诊断**：两个原因叠加。
① **种子太少**：TH06 侧是 `th06-re` 命名风格、NC 侧是 CC0 风格，
「同名」只在少数场合成立，133 个 NC 名字只换来 37 个种子。
② **NC 大量内联**，被调集合结构性地对不上。

**值得记的一点**：它在不知情的情况下复现了 `EnemyManager__OnUpdate`——
**手法本身是对的，只是缺种子**。若哪天有了大量可靠种子，可以再试。

## 3. ✓ 日志消息交叉命名（成立，本轮产出 10 个名字）

**想法**：NC 以 **Shift-JIS** 原样保留了 ZUN 原版的日志消息；CC0 把它们抽在
`i18n.tpl` 的 `TH_*` 宏里，而每个宏的使用点带着函数名。
于是**消息文本就是 CC0 源码与 NC 二进制之间的共同坐标系**。

**关键优势：不需要任何前置锚点。** 这正是前两条卡住的地方。

**做法**：
1. `i18n.tpl` → 宏到原文的表（85 条）。
2. 扫 CC0 的 `.cpp`，得到每个宏的使用函数。
3. 在 NC 里按 **cp932** 搜原文（UTF-16 零命中，UTF-8 只命中 1 条——**编码选错就全盘皆空**）。
4. 扫 `.text` 里指向该字符串的 RIP 相对 `LEA`，定位引用它的函数。
5. 要求 **CC0 侧唯一 且 NC 侧唯一**。

**结果**：24 条消息存活，配出 **11 个 NC 函数**（其中 1 个是自证，1 个人工纠正）：

| NC 地址 | 名字 | 支撑消息 |
| --- | --- | :---: |
| `th06nc:0x1400020b0` | `AnmManager__LoadAnm` | 2（**自证**，见下） |
| `th06nc:0x140007620` | `AnmManager__LoadSurface` | 1 |
| `th06nc:0x14003a0d0` | `GameErrorContext__Flush` | 1 |
| `th06nc:0x14003a9c0` | `GameManager__AddedCallback` | 8（含内联，见下） |
| `th06nc:0x14003eb80` | `Gui__LoadMsg` | 1 |
| `th06nc:0x1400479e0` | `MainMenu__OnUpdate` | 1 |
| `th06nc:0x1400767d0` | `SoundPlayer__InitSoundBuffers` | 1 |
| `th06nc:0x1400785c0` | `Stage__LoadStageData` | 1 |
| `th06nc:0x14007a6d0` | `Supervisor__AddedCallback` | 1 |
| `th06nc:0x14007b5d0` | `Supervisor__LoadConfig` | 1 |
| `th06nc:0x1400454f0` | `WinMain` | **人工纠正**，见下 |

### 自证

`th06nc:0x1400020b0` 早在 ANM 那一轮就被独立命名为 `AnmManager__LoadAnm`。
匹配器只看消息文本、不知道这件事，**复现了正确答案** ✅。
（与 [`../ecl/th06nc/03`](../ecl/th06nc/03-opcode-crossmatch.md) §2 的
`EnemyManager__SpawnEnemy` 是同一种验证。）

### 内联把判据骗过一次

`th06nc:0x14003a9c0` 同时命中 `GameManager::AddedCallback`（7 条）与
`EclManager::Load`（1 条）。查 CC0 `GameManager.cpp:427`——`AddedCallback` 里就调
`g_EclManager.Load(...)`，NC 把它内联了。按 7:1 判为 `GameManager__AddedCallback` ✅。

`th06nc:0x1400454f0` 更凶险：它带着 `SoundPlayer::InitializeDSound` 的成功消息，
匹配器给出那个名字，**且判据认为唯一**。但看调用图——调用者是 CRT 启动代码，
自身调 `SteamAPI_Shutdown` / `SetDllDirectoryA` / `_beginthreadex`，80 个 callee——
**这是程序入口 `WinMain`**，`InitializeDSound` 被内联进去了。

> ⚠️ **判据的漏洞**：这个函数还带着 `main.cpp` 的 `TH_ERR_OPTION_CHANGED_RESTART`，
> 但那条在 CC0 侧属于**自由函数**（不在任何 `Class::method` 里），所以没被算作冲突。
> **CC0 侧落在自由函数里的消息也应当算作冲突证据**——目前靠人工复核补，未自动化。
> 假阳性记在 `th06nc_logname_spec.py` 的 `REJECTED` 里，防止以后又被"发现"一次。


## 4. ✓✓ 非日志字符串：同一坐标系，把输入扩到全部字面量

§3 只用了 `i18n.tpl` 的 85 条日志消息，那是 §5 当时留的第 2 条待办。本轮兑现：
把输入换成 **CC0 全部 `.cpp` 里的字符串字面量**——文件名、格式串、菜单文本。
命中面大一个量级。

### 4.1 规模

| 量 | 数 |
| --- | ---: |
| CC0 源码里 ≥5 字节的字面量 | 530 |
| **原样存在于 NC 的 exe 里**（cp932，且前后都是 NUL，即完整 C 串） | 186 |
| 被 `.text` 里 RIP 相对 `LEA` 指到 | 119 |
| 沾到至少一条字面量的 NC 函数 | 36 |

自动判据判决：**接受 22、拒绝 14**。账要这么算：

| | 条 |
| --- | ---: |
| 自动接受 | 22 |
| ├ 落在**已入库地址**上且名字一致（自证，见 4.4） | 9 |
| ├ 新名字，直接入库 | 10 |
| ├ **人工否决**（判据看不见的内联者 / 与 CC0 调用关系矛盾，见 4.5） | 2 |
| └ 落在已入库地址上但**两轮都判错了名字**（见 4.5 第三条） | 1 |
| 自动拒绝 14 里，靠判据 (d) 拆开平票后入库 | +3 |
| **合计新入库** | **13** |

### 4.2 判据：三条自动的 + 一条人工的

**(a) CC0 侧唯一**——该字面量在 CC0 里只出现在一个函数体内。

**(b) 自由函数算冲突**——这条就是 §3 留下的判据漏洞。§3 放跑 `WinMain`，
是因为它只看 `Class::method` 归属，没把**自由函数**里的字面量算进冲突。
本轮把 `<free>` 归属计入后，`th06nc:0x1400454f0` **被判据自动拒绝了**
（它同时带着 `SoundPlayer::InitializeDSound` 的消息和 `main.cpp` 一条自由函数的消息）。
上一轮靠人工才拦下的东西，这一轮机器自己拦住了 ✅。

**(c) caller-closure 挑「内联者」而不是「被内联者」**——这是本轮发现的新坑。
NC 内联得凶，一个 NC 函数常同时带着好几个 CC0 方法的字面量；
**按条数投票会选中被内联进来的那个**。改成：若这些 owner 同属一个类、
且其中恰有一个在 CC0 里（传递地）调用其余全部，就选它。

`th06nc:0x1400479e0` 是活证据：

| 规则 | 判给谁 |
| --- | --- |
| 票数（§3 用的） | `MainMenu::LoadDiffCharSelect`（9 条） |
| caller-closure | `MainMenu::OnUpdate`（**1 条**） |

CC0 里 `OnUpdate` → `ReplayHandling` / `LoadDiffCharSelect` / `BeginStartup` → `LoadTitleAnm`
（`MainMenu.cpp:102/145/279/1014`），闭包盖住全部五个 owner。
而 **§3 用一条完全不同的日志消息，独立地也判成了 `OnUpdate`** ——
两条互不相干的证据链撞到同一个答案。

> ⚠️ **§3 的 `GameManager__AddedCallback` 是靠 7:1 票数判对的，那是运气。**
> 同样的形状换个数字就会判错。规则本身有缺陷，不是阈值问题。

**(d) chain 回调的结构签名**（人工复核，**不在自动判据里**）——
TH06 的 `Chain` 回调是被存成函数指针的，所以在 NC 里表现为
「**被 `LEA`、从不被 `CALL`**」。这条拆开了两组自动判据认输的平票，
也否掉了一个自动判据接受了的候选（4.5）。

### 4.3 结果：13 个新名字

| NC 地址 | 名字 | 支撑字面量 | 第二重证据 |
| --- | --- | :---: | --- |
| `th06nc:0x140009700` | `AsciiManager__AddedCallback` | 3 | 被 `0x140009400` 取址、从不被调用 |
| `th06nc:0x140011f80` | `BulletManager__AddedCallback` | 2 | 被 `GameManager__AddedCallback` 调用 |
| `th06nc:0x14002b390` | `EffectManager__AddedCallback` | 5 | 被 `0x14002b490` 取址、从不被调用 |
| `th06nc:0x14002b7a0` | `Ending__LoadEnding` | 1 | 被 `0x14002bac0` 调用，对应 `Ending.cpp:223` |
| `th06nc:0x14002c8d0` | `Ending__AddedCallback` | 9 | 被 `0x14002b840` 取址、从不被调用 |
| `th06nc:0x14003cd20` | `Gui__OnDraw` | 5 | 被 `0x140041ca0` 取址；**调用 `Gui__DrawStageElements`** |
| `th06nc:0x14003d500` | `Gui__ActualAddedCallback` | 28 | 被 `0x140041ca0` 调用（同一个注册点） |
| `th06nc:0x14003ece0` | `GuiImpl__MsgRead` | 3 | 被 `EnemyManager__RunEclTimeline` 调用 |
| `th06nc:0x140041820` | `Gui__DrawStageElements` | 3 | **被 `Gui__OnDraw` 调用**，对应 `Gui.cpp:95` |
| `th06nc:0x140058cc0` | `MusicRoom__AddedCallback` | 4 | 被 `0x1400581b0` 取址、从不被调用 |
| `th06nc:0x140073d50` | `ResultScreen__AddedCallback` | 4 | 取址者同时取址已知的 `ResultScreen__OnUpdate` |
| `th06nc:0x14006d310` | `ResultScreen__HandleReplaySaveKeyboard` | 1 | 被已知的 `ResultScreen__OnUpdate` 调用，对应 `ResultScreen.cpp:1699` |
| `th06nc:0x14007b360` | `Supervisor__LoadPbg3` | 1 | 被五处调用（每个 `.dat` 一次），是归档加载器的调用形状 |

**每一条的第二重证据都是人工复核出来的调用图/结构事实**，不是匹配器给的——
METHOD 的「光有匹配不够」写在 `th06nc_strname_spec.py` 的 `validate()` 里，
备注短于 40 字符就测试失败。

两条**互为佐证**的最漂亮：`Gui__OnDraw` 与 `Gui__DrawStageElements` 各自由自己的
字面量独立命名，而 NC 里前者 `CALL` 后者，正是 CC0 `Gui.cpp:95` 写的那条边。

### 4.4 自证：不知情地复现了上一轮 9 个名字

匹配器只看字面量，不读 `symbols.json`。它独立判出的 22 条里，有 **9 条落在已入库的地址上，
且名字完全一致**：`AnmManager__LoadAnm` / `LoadSurface`、`GameErrorContext__Flush`、
`GameManager__AddedCallback`、`Gui__LoadMsg`、`MainMenu__OnUpdate`、
`Player__AddedCallback`、`SoundPlayer__InitSoundBuffers`、`Supervisor__AddedCallback`。
（第 10 条落在同一个地址上但**两轮判的名字都是错的**，见 4.5。）

`apply_th06nc_strnames.py` 每次跑都会重新核对这 10 条（输出里的 `reproduced=10`），
名字一旦对不上就说明有人动过库。

### 4.5 人工复核拦下的两条

自动判据接受、**人工否决**：

- `th06nc:0x140054870` → `MainMenu__DrawReplayMenu`。两条格式串双向唯一，判据满意。
  但它**被 `0x140055a30` 取址且从不被调用**——chain 回调的签名。而 CC0 里
  `DrawReplayMenu` 从来不当函数指针用，`MainMenu::OnDraw` 才是，且
  `MainMenu.cpp:80` 正是 `OnDraw` 调 `DrawReplayMenu`。**更可能是 `OnDraw` 内联了它。**
  这是 §3 `WinMain` 那个坑的同一类：**内联者自己不贡献字面量时，判据看不见它。**
- `th06nc:0x1400678a0` → `MainMenu__ReplayHandling`。它引用了 CC0 里只属于
  `ReplayHandling` 的 `g_GameErrorContext.Log("japanese")`（`MainMenu.cpp:1280`），
  但**唯一调用者是 `WinMain`**，而 CC0 里 `ReplayHandling` 只被 `MainMenu::OnUpdate` 调用。
  调用关系与 CC0 矛盾 —— 矛盾即拒绝。

还有**第三条，是后来 STD 那轮才抓出来的**，也是最值得记的一条：

- `th06nc:0x1400785c0` → `Stage__LoadStageData`。§3 用日志消息判成它，§4 用字符串
  **又独立判了一遍，还是同一个错名**——两轮、两条互不相干的证据链，一起错到同一处。
  真身是 `Stage::AddedCallback`（`LoadStageData` 被内联了进去）。
  拆穿它的不是字符串，是**结构**：它被装进 calc 链的 `addedCallback` 槽，
  而且是 `g_StageFiles` 的唯一引用者——CC0 的 `LoadStageData` 拿路径当参数，
  根本不碰那张表。详见
  [`../std/th06nc/01`](../std/th06nc/01-stage-runtime-and-nc-loop.md) §9。

> **教训**：`WinMain`、`0x140054870`、`0x1400785c0` 三例都是同一个盲点——
> **内联者自己不贡献字面量时，判据只看得见被内联进去的那个**。
> 判据 (c) 的 caller-closure 只在**内联者也贡献了至少一条字面量**时才起作用；
> 一条都不贡献就彻底失明。**这个盲点还没有自动解法**，目前全靠人工看调用图与取址点。

三条连同 §3 的 `WinMain`、两条平票的输家，都登记在 `th06nc_strname_spec.REJECTED` 里，
有测试钉住，防止以后又被"发现"一次。

## 5. 下一步

1. ✅ ~~把「自由函数消息也算冲突」写进匹配逻辑~~ —— §4.2 (b) 做了，
   而且它**自动拒绝了 `WinMain`** 那条假阳性。
2. ✅ ~~同样的坐标系用在非日志字符串上~~ —— §4 做了，+13 个名字。
3. ❓ **把「chain 回调结构签名」自动化**（§4.2 (d) 目前是人工）。判据是
   「被 `LEA`、从不被 `CALL`」，两样本文都已经算出来了，接进匹配器即可。
   它能救回 §4.5 那两条被否的，也能顺手命名一批 `*::RegisterChain`。
4. ❓ **内联者不贡献字面量时判据看不见它**（§4.5 第一条）。可能的补法：
   若候选 owner 在 CC0 里被同类的某个 chain 回调调用、而该回调在 NC 侧未命名，
   就把它降级为「候选是回调本身」。
5. ❓ 现在有 74 个函数名了，再跑一次 §2 的调用图自举看看种子够不够。
   §2 的诊断是「手法对、缺种子」——种子从 37 涨到了多少值得一测。
6. ❓ 若从 **CC0 源码**直接建调用图（而不是从 TH06 二进制），种子问题也许能绕开——
   毕竟 CC0 才是许可证上唯一能用的一侧。

## 6. 复现

```sh
# 日志消息那轮（§3）
python3 -c "import sys; sys.path.insert(0,'tooling/ghidra'); \
  import th06nc_logname_spec as s; print(s.validate())"
source tooling/env.sh && python3 tooling/ghidra/apply_th06nc_lognames.py --dry-run

# 非日志字符串那轮（§4）—— 匹配器是**离线**的，不抢 Ghidra 工程锁
python3 tooling/ghidra/th06nc_strname_match.py            # 打印 22 接受 / 14 拒绝
python3 tooling/ghidra/th06nc_strname_match.py --check    # 与 spec 对账，不一致非零退出
source tooling/env.sh && python3 tooling/ghidra/apply_th06nc_strnames.py --dry-run

python3 -m unittest discover -s tooling/ghidra/tests      # 含出处守门（122 条）
```

`--check` 会核对三件事：匹配器接受的每个地址要么在 spec 里、要么在 `REPRODUCED` /
`REJECTED` 里（不许有"匹配器认了但没人写下来"的）；`TIEBREAKS` 里登记的地址
必须确实是匹配器判不了的（否则说明注释过期了）。
