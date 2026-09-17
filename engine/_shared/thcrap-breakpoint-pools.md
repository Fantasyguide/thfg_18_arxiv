# thcrap 断点池（站点）真相：**按 stage 现算现分配，没有固定容量**（一手）

> **版本**：跨版本（TH18 v1.00a 实测；机制一手来自 thcrap 2024-11-06 的源码 + 本机安装的
> `thcrap/bin/thcrap.dll` 字节 + 本 modkit 的实跑日志/dump）。
> 关联：`mods/th18.v1.00a/card-expand/GATES.md`（站点闸台账，本文修正它的模型）、
> `docs/HANDOVER-2026-09-17-site-budget-and-primitives.md`、`site-budget-and-primitives.md`（原语落地表）。

## 1. 一句话结论

**thcrap 的「站点」不是一个有固定容量、需要抢名额的资源**：每个 **stage** 在装载时按
**实际有效地址条数现算池大小**，1 条 = 1 格 cave + 1 个桩，池子按需申请。
⇒ 历史上记的「≤12 安全 / 14·16·22 崩 / 24 安全 / 26 崩」**不是容量线**，是当年那几次配置的读数；
「声明数超线 ⇒ 必崩」这条因果**不成立**（`/tmp` 之外的证据见 §2、§5）。
真正会随条数变化的是**池内布局**（每格的位置），这也正是崩点偏移会随条数平移的原因（§3、§4）。

## 2. 机制（一手：源码 + 安装版字节 + 本机实测）

**① 一条声明 = 一格 + 一桩。** `breakpoints_apply(stage.breakpoints, count, module, page_array[2])`
按 stage 调一次（`runconfig.cpp` 的 `runconfig_stage_apply`），一个 stage **只有一个池对**，
**多个归档（我们 255 / 测试 / `base_tsa`）只要在同一 stage，就吃同一个池**：

| 池 | 用途 | 每条的步长 | 申请大小 |
| --- | --- | --- | --- |
| `cave_source`（sourcecave）| 存**被偷走的原字节** + `e9` 回跳；桩把返回地址指到这里，**会被执行** | `align16(cavesize + 5)`，实测 `0x10` | `Σ 每格步长` |
| `cave_call`（callcave）| 桩（thcrap 的 `bp_entry` 实例，每桩一份）| 桩变体长度，x86 默认 `bp_entry0` = **`0x30`** | `Σ 桩长` |

`cave_source` 先 `memset(…, 0xCC)` 再逐格写入 ⇒ **没被写到的地方是 `0xCC`**；
`VirtualAlloc` 提交整页 ⇒ **池申请大小之后、整页之内是 `0`**（这一段是崩点所在，见 §3）。
两个池是相邻的 `VirtualAlloc`，**Windows 的 64KB 分配粒度** ⇒ 池页基址恒差 `0x10000`
（实测：binhack 洞 `03c00000` / sourcecave `03c10000` / callcave `03c20000`）。

**② 桩会直接 `call` 到我们的 `BP_*` handler。** 一手反汇编（本机 `thcrap.dll` 的桩模板，
与实跑日志里 `page` 落下来的桩逐字节同形）：

```asm
pushad / lahf / seto al / push eax / cld / mov ebp, esp
push <json_obj>            ; 槽 +0x0a
push ebp                   ; regs = x86_reg_t*（就是这一帧）
call <BP_xxx>              ; 槽 +0x10，装载时按 handler 地址现算 rel32
mov esp, ebp               ; 丢弃栈参
test eax, eax              ; 返回值 = cave_exec
je  +7
mov dword [ebp+0x24], <cave>   ; 槽 +0x1d：把 cave 写进返回地址槽
pop eax / cmp al,0x81 / sahf / popad / ret
```

⇒ **安装版没有 `breakpoint_process`、没有 `esp_diff` 机制**（那是 2020 版老源码的写法，
老桩末尾有 `add esp, eax`；本机桩是 `mov esp, ebp`）。由此三条**硬结论**：

1. **handler 改 `regs->esp` 是无效的**（没有任何代码消费它）；要「返回并弹掉栈参」只有两条路：
   声明里用 thcrap 原生的 **`stack_clear_size`**（走 `bp_entry?s` 变体的 `ret imm16`），
   或者自己把 `retaddr` 设成**真正的调用者返回地址**。
2. **`regs->retaddr` = 桩要跳过去的地址**（桩在 handler 返回非 0 时无条件写 `[ebp+0x24]`）；
   handler 想改落点就在返回 0 时写它。
3. **`[regs->esp]` 不是调用者返回地址**，而是 **thcrap 自己那条 `call <桩>` 的返回地址 = 站点 `+5`**
   （本仓 `native/sdk.c` 的「实跑订正②」也独立测到过：站点 `th18:0x430240` 处读到 `th18:0x430245`）。

**③ 本机实测对账（2026-09-17 23:16 那局，日志一手）。** 从 `game/th18_card_expand.log` 的
整页 hex 离线解出：**42 格**（`0x10` 步长，最后一格 `+0x2a0`，填充止于 `+0x2b0`）+ **42 桩**（`0x30` 步长，止于 `0x7e0`），
首个全零格 `@03c102b0` 与 DLL 自己打的 `pool:` 行**完全一致**；逐格解出的「站点 / `cavesize` / `e9` 回跳目标」
与三份声明（我们 20 + 测试 2 + `base_tsa` 20）**逐条对上，零多余零缺失**。
⇒ 「42 条声明配 42 格」是**尺寸吻合**，不是「刚好没越线」。

## 3. 崩溃签名：**跳进池页从没写过的那一段**

一手（`local/crashdumps` 的 `th18.exe.73348.dmp`，2026-09-17 01:35）：

| 项 | 值 | 说明 |
| --- | --- | --- |
| 异常 | 异常码 C0000005（Windows 访问违例）**写** 地址 `0x1` | `params = [1, 1]` |
| `Eip` | `th18:0x03c2064a` | 不在任何模块；那一页 dump 捕到的 `0x3c205ca..0x3c206ca` **全 0** |
| `Eax` | `1` | `00 00` = `add byte ptr [eax], al` ⇒ 正好写 `0x1` |
| 帧 | `esp=0x1ade40`、`ebp=0x1ade74`、`[ebp+4]=0x401357`（更新链派发）| 崩在某条 `UpdateFunc` 的 `on_tick` 里 |
| 栈 | 全栈段里**没有** thcrap / 我们 DLL 的返回地址；`[esp-4] = 0` | 不是刚 `call`/`ret` 到这里的现场（`call` 会压返回地址、`ret` 会留弹掉的槽）|

**共同点（四次崩点）：** 崩点**偏移恒 ≡ `0xa` (mod `0x10`)**（`+0x62a` / `+0x64a` / `+0x65a`），
**换配置时按 `0x10/条` 平移**（25 条 → `+0x65a`；22 条 → `+0x62a`）；
崩的**页会换**（`03c1` / `03c2` / `03d2`），但**页内偏移几乎不变**。

⇒ 这些量只能由「**池布局**」解释：转移到的是一个**相对于池页的地址**，而不是某个固定模块地址。

## 4. 触发原理（推测 → 当前最站得住的读法，可信度 🟡）

1. **`00 00` + `eax=1` + 写 `0x1`** 只说明一件事：**控制流来到了池页里没写过的那一段**
   （`0xCC` 填充之外、整页之内的 `0`）。⇒ 有人把**一个池地址**当了控制流目标（`jmp` / `ret` 落点）。
2. 那个池地址**落在本局池子的「已写区」之外**，而它的**页内偏移跟着声明条数走** ⇒ 它是
   **按池布局算出来的**（cave 格 / 桩的地址，或从它们派生），**不是** 某个固定地址。
3. 于是「条数 → 崩」的真正链条是：**条数只改变池内布局** ⇒ 布局一改，某个**陈旧的 / 被错算的池地址**
   就从「落在已写格上」变成「落在零尾巴里」。⇒ **条数从来不是判据，布局才是**；
   这也解释了同一份 40 条配置「崩过也跑通过」（§2 的第三条 + `GATES.md` §8）。
4. 反过来说：**「多挂几条站点」不会因为"名额"崩**，只会因为「布局变了 / 有别的 bug 在拿池地址当落点」崩。

## 5. 已经排除的（都有脚本/一手证据，别再回头查）

| 假设 | 判据 | 结果 |
| --- | --- | --- |
| 站点窗口 × binhack / 站点 × 站点 重叠（把已改过的字节偷进 cave）| 三份声明（我们/测试/`base_tsa`）302 项两两求交 | **零重叠** |
| 窗口不是整指令（cave 尾部把 `e9` 当操作数）| 42 条全量 capstone 逐条 | **42/42 对齐** |
| 窗口里含分支（相对跳转在 cave 里基址变）| 同上，扫 `call`/`jcc`/`jmp` | 只有 3 条**起始** `e8`（`cave_fix` 会修），**非起始分支 0 条** |
| thcrap `esp_diff` 补偿算错（窗口动 esp）| 2024-11 版桩里根本没有 `esp_diff`（§2）| 假设不成立 |
| 更新链节点被写坏（`ce_chain_node_guard`）| 实跑 24 条 `chain node`、0 条 `CHANGED`、0 条 `guard` | 排除 |
| 崩点页是「未提交」的 `RESERVE` | dump 捕到该页字节（全 0）⇒ 已提交、可执行 | 之前那条读法作废 |

## 6. 我们这边的**真 bug**：handler 里的 `regs->esp` / `retaddr = sp[0]` 写法

`native/sdk.c` 的 `BP_ce_native_option_muzzle`（站点 `th18:0x45e930` = `Player__shoot_one_bullet` 入口）
按「thcrap 支持 `esp_diff`」的老假设写成：

```c
muzzle = (uint8_t *)(uintptr_t)sp[4];   /* 注释说是第 5 个栈参=膛口 */
regs->retaddr = sp[0];                  /* 注释说是"调用者返回地址" */
regs->esp = regs->esp + 0x14;           /* 想等价 `ret 0x10` */
```

按 §2 的安装版语义，这里**三处都不成立**：`esp` 写了无效；`sp[0]` 是**站点 `+5`**（thcrap 蹦床返回地址，
我们自己另一处注释已经实测到过）；`sp[4]` 因此也**错位一格**。
⇒ 一旦这条路径真的命中，就是「不重放序言 + 落回函数体内 + 栈参不弹」⇒ 帧错位 ⇒ 之后某个 `ret`
弹到栈上的陈旧值（而栈上的陈旧值里**就有池地址**）⇒ 正是 §3 的崩法。**这属于同类隐患的样板。**

⚠ 实跑证据：`df1631a` 那版日志里**一次都没有** `原生子机弹已拦`（0 行）⇒ 它当时走在「放行」分支
（`sp[4]` 错位使它几乎不可能过区间的判据）⇒ **它不是 09-17 那几次崩的直接原因**（那几次的 build = `fa9949a`，
`native/sdk.c` 里连这段代码都还没有），但**它是一颗已经上膛的子弹**：`9631528` 把
`ce_native_option_muzzle` 重新放进了发布面（用户实跑「还有小激光」的那次修复）⇒ **这条路径现在会真跑**。

**正确写法（本文 §2 的一手语义推出来的，可直接落地）：**

| 位置 | 现在 | 应改成 | 依据 |
| --- | --- | --- | --- |
| 站点声明（`native/sites.py`）| 无 `stack_clear_size` | `"stack_clear_size": 0x10`（⇒ 走 `bp_entry0s` 变体）| 该函数是 **stdcall**（调用方 `th18:0x45ec78` 之后**没有** `add esp,0x10`，清理在函数自己 `ret 0x10`）|
| handler 读膛口 | `sp[4]` | `sp[5]` | `sp[0]` = 站点 `+5`（我们自己实测过）、`sp[1]` = 调用者返回地址、`sp[2..5]` = 4 个栈参 |
| handler 落点 | `retaddr = sp[0]` | `retaddr = sp[1]` | 同上；`sp[1]` 才是「回到调用者」的地址 |
| handler 栈清理 | `regs->esp += 0x14` | **删掉** | 安装版没有 `esp_diff`；清理交给 `stack_clear_size` 的 `ret imm16` |
| 声明里的 `stack_clear_size` | —— | **`4 + 函数自身的清理量`**（本例 `ret 0x10` ⇒ 写 `0x14`）| 桩的 `ret imm16` 弹的是**桩自己**那条 `call <桩>` 压的返回地址，比调用者压的返回地址低一格；漏掉那 4 字节 = 调用链整体偏低 4（2026-09-18 实跑一手：一按 Z 就崩，`th18:0x4013ba` 读 `[ebx+0x50]`、`ebx=3`；旁证 = thcrap 自己的站点带 `c2 08 00` = 4 + 一栈参）|

⇒ 返回 0 + `retaddr = sp[1]` + 声明 `stack_clear_size: 0x10` = **桩自己 `ret 0x10`**：
回到调用者、弹掉 4 个栈参、不重放序言 —— 与注释里想要的「等价 `ret 0x10`」逐字一致，
而且**全程不依赖任何被忽略的 `regs` 字段**。另外建议给所有 `regs->retaddr` 赋值加一道
「目标必须在某个模块的可执行段里」的自检（不合就记一行并保持原值），把这一类整体堵掉。

## 7. 下一步（一次实跑就能收口）

thcrap **自己也装了异常处理器**（`exception_init()`），崩溃时会把地址归类写成
`<池名> + 0x偏移（Stage N）` 并附一段执行流分析，落在 `thcrap/logs/thcrap_log.txt`（滚动保留 6 份）。
⇒ 现在只要**在完整日志还在的时候复现一次**，就能直接点名「谁把池地址当落点」：

1. 实跑（打枪 + 上线收点那套）；崩了就把 `thcrap/logs/thcrap_log.txt`、
   `game/th18_card_expand.log`（`pool:` 行 + `site:` 轨迹）与 `crashdumps/*.dmp` 三样一起看；
2. 用 `tooling/crashdump/dmp.py` + `tooling/poolmap.py` 交叉核：崩点偏移 ↔ 本局池里的格；
3. 若那一局**不崩**，就按 §6 把 handler 的落点写法换掉（并加自检门），再看是否还会复现 ——
   这也是把「拿池地址当落点」这一类整体堵掉的办法。

## 关联

- 站点闸与配额台账：`mods/th18.v1.00a/card-expand/GATES.md`（本文修正它的容量模型；
  ⚠ 该文件的 §1/§2 还写着「超线 ⇒ 构建失败」当容量线读，建议开头补一段指向本文 §1/§4 的订正）；
- 交接说明：`docs/HANDOVER-2026-09-17-site-budget-and-primitives.md`；
- 崩溃取证工具：`tooling/crashdump/dmp.py`、`tooling/poolmap.py`、`tooling/xref.py`；
- 本机运行期读数（DLL 自己打的）：`pool: 可执行页 N、跳板格 M、首个全零格 @…`
  由 `mods/th18.v1.00a/card-expand/native/sdk.c` 的 `ce_mem_map_dump()` 输出。
