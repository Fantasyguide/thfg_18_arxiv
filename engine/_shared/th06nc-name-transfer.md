# 给 TH06NC 批量迁名：三种手法的实测结果

> **版本**：跨版本。地址带前缀（`th06nc:0x1400020b0` / `th06:0x00411390`）。

TH06NC 有 2816 个函数，我们这层只命名了 100 出头。本文记录**三种批量迁名手法的实测**——
两条否定、一条成立——以及一条比技术更要紧的**许可证约束**。

写下否定结果是为了**不再重复投入**：这三条都花了真金白银的测量。

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

## 4. 下一步

1. ❓ 把「自由函数消息也算冲突」写进匹配逻辑，重跑一遍看有没有别的假阳性。
2. ❓ 同样的坐标系还能用在**非日志字符串**上：文件名、格式串、窗口标题。
   本文只用了 `i18n.tpl` 的 85 条。
3. ❓ 新增的 10 个名字给调用图自举带来的种子太少（+3）。
   若从 **CC0 源码**直接建调用图（而不是从 TH06 二进制），种子问题也许能绕开——
   毕竟 CC0 才是许可证上唯一能用的一侧。

## 5. 复现

```sh
python3 -c "import sys; sys.path.insert(0,'tooling/ghidra'); \
  import th06nc_logname_spec as s; print(s.validate())"
python3 -m unittest discover -s tooling/ghidra/tests   # 含出处守门
source tooling/env.sh && python3 tooling/ghidra/apply_th06nc_lognames.py --dry-run
```
