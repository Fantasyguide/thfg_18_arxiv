# AUDIT —— th18_bgwarp 对抗审计

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`，md5 `9969cac756098c1da05a81de45437a70`）。本文裸地址默认属该版本。
>
> 姿态按 [`../../_template/AUDIT-checklist.md`](../../_template/AUDIT-checklist.md)：**默认我们写错了，去二进制找证伪证据**。
> 两轮：**① 自查**（作者按 A–F 逐条走，证据在下面）；**② 独立对抗审计**（另起一个 agent，只给它源码与 exe，让它证伪）。
>
> **结论（2026-10-15）**：`make check` / `make verify` 全绿；**未实跑** —— 静态审计通过 ≠ 能跑。
> 未实跑的产出在 [`README.md`](README.md) 顶部与 §4 都明写了。

## A. ABI / 栈平衡

本 DLL **不调用任何引擎函数**（唯一的引擎交互是两处 hook 点的寄存器与内存）。
所以 checklist A 的「被调函数 `ret N` / 谁清栈」那一族**不适用** —— 但**跳板自己的栈平衡**适用，逐字节核过：

| 步 | 指令 | ESP 变化 | 证据 |
| --- | --- | --- | --- |
| 入口 | （thcrap 的 `call` 落在本 hook） | — | 跳板由 `VirtualAlloc` 得到，地址只存在 DLL 内 |
| 1 | `60` pushad | −32 | capstone 反汇编复核（见 E） |
| 2 | `9c` pushfd | −4 | 与 thcrap 的 `x86_reg_t` 同布局（`thcrap/src/expression.h`） |
| 3 | `54` push esp | −4 | 参数 = `&x86_reg_t` |
| 4 | `b8 imm32` / `ff d0` call eax | −4（call 压返回地址） | 用 `mov eax,imm32; call eax` 而不是 `call rel32`，避免跳板离 DLL 超过 ±2GB |
| 5 | `83 c4 04` add esp,4 | +4 | cdecl，caller 清参 |
| 6 | `9d` popfd / `61` popad | +36 | 回到入口 ESP |
| 7 | 被搬走的原字节 | 0 | 这时 ESP/EBP **与原生入口完全一致**，所以 `push ebp; mov ebp,esp` 得到的栈帧与不打 hook 时相同 |
| 8 | `e9 rel32` jmp target+len | 0 | 直接续在原流程上 |

- **FPU 栈**：跳板不碰 x87；C 处理函数是 cdecl 函数，返回时 x87 栈为空（进出等深）⇒ ✅。
- **XMM**：`pushad` 不保存 xmm0–7，本 DLL 也没有保存。两处 hook **都在「调用者不再依赖 xmm」的位置**：
  - hook① 是 `FUN_0042b540` 的**函数入口** ⇒ 按 Win32 约定 xmm 本来就是 caller-saved，被调方任意破坏；本题里 `0x42b529` 的调用点之后（`0x42b52e` 起）是一条 `pop esi` + 收尾，**没有任何 xmm 读**（一手反汇编）。
  - hook②（`0x41ee38`）之后第一条用到 xmm 的指令是 `0x41ee3e movss xmm1,[edi+0x3434]`（**重新装载**）；循环体内的 xmm 值到这里已经死。
- **EFLAGS**：`pushfd/popfd` 原样带过；C 处理函数里对 flags 的改动不泄漏 ✅。

## B. 写入点

| # | 位置 | 类型 | 原字节（`make check` 逐字节验） | 结论 |
| --- | --- | --- | --- | --- |
| ① | `0x42b540` 前 7 字节 | **代码**（入口补丁 `e9` + 2×`0x90`） | `55 8b ec 51 57 8b f9` | CONFIRMED |
| ② | `0x41ee38` 前 6 字节 | **代码**（入口补丁 `e9` + 1×`0x90`） | `8b 7c 24 1c 33 c9` | CONFIRMED |
| ③ | `mesh+0x14` 的顶点数组 | **数据**（堆，`malloc` 出来的） | 无（运行时指针） | 见 C |

- `0x42b540` 在 `.text`（可执行、非只读）⇒ `VirtualProtect(PAGE_EXECUTE_READWRITE)` 后写入 ✅。
- ③ **只写堆上的顶点数组**，不写引擎的代码、不写对象头、不动 VM 指针 ✅。
- 「被重指的槽在原生流程中确实不被用到」不适用：我们没有重指任何东西，只是**在两个既有函数的前几条指令上加了跳板**。

## C. 偏移与内存安全

| 偏移 | 一手出处 | 结论 |
| --- | --- | --- |
| `mesh+0x00/0x04/0x10/0x14` | ctor `0x42b1f0`（`0x42b22d`／`0x42b233`／`0x42b24e`／`0x42b260`） | CONFIRMED |
| `mesh+0x14` 步长 28 | 建网格写点 `0x42b480`–`0x42b4fc`（`add eax,0x1c`） | CONFIRMED |
| 顶点字段 `{x,y,z,w,color,u,v}` | `0x42b480`–`0x42b4eb` 逐条；`+0x13` 是 alpha（`0x41e894`） | CONFIRMED |
| `vm+0x5c8` | 上传读点 `0x42b56b`；绘制 `0x481643` | CONFIRMED |
| `stage+0x3410/0x3414/0x3418/0x3434/0x3438/0x343c` | `0x41dace`／`0x41da0d`／`0x41eadd`／`0x41e78a`／`0x41e797`／`0x41e7ab` | CONFIRMED |
| `on_tick_part_2` 把 stage 存在 `[esp+0x1c]` | `0x41e778` 写、`0x41ee38` 自己读；函数体内 `push ecx`+`call 0x42b3e0`（`ret 4`）与 D3DX（stdcall）都**不改 esp 净额** | CONFIRMED |

内存安全措施（都在 `th18_bgwarp.c` 里）：

- 所有指针**先 `readable()` 再解引用**（`VirtualQuery` 逐段走到 `end`，拒 `PAGE_NOACCESS`/`PAGE_GUARD`/未提交，
  且 `p < 0x10000` 直接拒）。
- `n`/`m` 一律夹在 `1..64`；`readable(v, n*m*28)` 不通过就**整段放弃**。
- 模式 1 的写回按 6 个 float 字段逐个 memcpy，**跳过 `+0x10` 的 D3DCOLOR**（避免把颜色当 float 乘 1.0）。
- 注入写在 `scan_mesh()` 之前，`scan_mesh` 也做同样的夹取 ⇒ 日志里不会读到半截。

**未证伪 / 仍待实跑**：`stage` 与 `mesh` 若在关卡切换瞬间是野值，`readable()` 会放行一些「恰好可读」的
垃圾地址 ⇒ 最坏是**打出几行垃圾日志**（只读，不会写坏东西）；本 DLL 没有对「这确实是 stage」做签名判据。

## D. 量纲 / 常识关

| 量 | 值 | 判据 |
| --- | --- | --- |
| 半径阈值 | `112.0f` | 一手 `0x41da0d` 写 `0x42e00000`；`0x41eae5` 比较 |
| 圆盘中心 | `(224.0, 240.0)` | 一手常量 `0x4b9348` / `0x4b934c`；网格跨度 `[0x56acbc]-r …`，与「格心 224/240」自洽 |
| 正弦项系数 | `×8.0f` | 一手 `0x4b9268` |
| 相位步长 | `π/2` / `0.6981317` | 一手 `0x4b91dc` / `0x4b9150` |
| 圆内/圆外 alpha | `0x60` / `0x00` | 一手 `0x41ebfe` / `0x41ed5a` |
| ★ 幅度 `amp` | ❓ **未定** | 一手是运行时值 `stage+0x3438`；本 DLL 在 hook② 把它打出来，实跑后回填默认值 |
| D3D 设备 | `[0x4ccdf8]` | 每个绘制点的 `mov eax,[0x4ccdf8]`；`DrawPrimitiveUP` = vtable `+0x14c`（IDirect3DDevice9 第 83 项） |

与可观察行为冲突时**先怀疑我们的映射** —— 判读表（[`README.md`](README.md) §4）就是按这条写的：
若「重算后」的 alpha 全是 `ff`，我们**不会**说文档错了，而会先假设是那一场走了另一支。

## E. 收尾

- ✅ **汇编后比对反汇编**：`make_stub()` 的字节序列用 capstone 反汇编复刻过，逐条对上设计（`pushad`/`pushfd`/`push esp`/
  `mov eax,imm32`/`call eax`/`add esp,4`/`popfd`/`popad`/原 7 字节/`e9`），跳板总长 = 20+7 = 27 字节，
  尾部 `jmp` 落在 `0x42b547`（= `0x42b540+7`）✅；入口补丁 = `e9` + (len−5) 个 `0x90` ✅。
- ✅ **可一键关掉**：删掉 `<thcrap>/bin/th18_bgwarp.dll` 即恢复原状（进程内零持久化：不写注册表、不改文件、
  代码页只在进程存活期间被改）。
- ✅ `make check`：4 组签名逐字节对上真 exe；两个 hook 窗口经 capstone 验「整数条指令 + 无相对寻址」。
- ✅ `make verify`：`PE32 … Intel 80386`；导出表是无装饰的 `thcrap_plugin_init`（`@1`）与 `bgwarp_mod_exit`（`@2`）。
- ❌ **游戏内实跑**：**没做**。README 顶部与 §4 明写「未实跑」，判读表就是交给人的那一步。

## F. UI / 动画复刻（铁律：先从原代码找）

- ✅ 原**脚本/代码**找到了：舞台网格 + `[mode==2]` 的逐顶点重算（`0x41ead4`–`0x41ed51`），不是我们编的。
- ✅ 原**数值**照抄：半径 112、圆心 224/240、`×8`、相位步长、alpha 60/00（见 D 表，逐条给地址）。
- ⚠️ 两处**明确的差异**，不许包装成「复刻」：
  1. 一手的归一化 `n = normalize(dx, dy, dz)` 里 `dz = v.z - R²`（`0x41ebbf`）—— 本 DLL 照抄了这一项，
     并**顺带指出**：`dz` 量级 ≫ `dx/dy`，所以 `t*n.x` 实际上小到看不见，位移主要来自 sin 项。这是**事实陈述**，不是设计选择。
  2. 相位的**初值**（一手 = `stage+0x3434/0x3438` 的当帧值）本 DLL 用「自由相位 + `phase_drift`」代替 ⇒ **❓ 自定**，
     登记在此，不进 `TUNING.md`（那是 card-expand 的账）。
- ✅ 不动任何贴图、不新增 anm 资产：本 DLL 只改顶点位置与 alpha。
- ❌ 未做「照抄视觉验收」：需要人跑一局对比原版非符画面（README §4 判读表第 4/5 条）。

## 独立对抗审计（第二轮）

> 由另一个 agent 只拿源码 + `TARGET.md` + exe 独立跑，姿态是「默认写错了」。
> 结果与逐条处置见下一节（**审计意见 → 我方核对 → 处置**）。

（待填 —— 第二轮正在进行）

## 结论格式约定

每条 claim 标 **CONFIRMED（一手，附地址）/ REFUTED（附反证）/ OPEN（说清缺什么）**。
本 DLL 只解开了「**顶点这一层能不能被我们驱动**」这一个分支；**109 要的效果本身**（网格从哪来、跟不跟 boss）
仍是 OPEN。

## Q. 对另一支（`native/th18_bgwarp.c`）的唯一改动：`retire` 开关（2026-10-22）

**为什么**：C 方案（owner 从一开始要的）要求"**引擎那份 aura 退休**，由 route_b 一个 pass 画两处透镜"
⇒ 否则两份扭曲叠在同一批像素上，后画的会采到先画的 ⇒ **透明度叠影**（owner 实测现象）。

**为什么不能复用现有旋钮**：`amp_mul = 0` **做不到** —— `inject_swirl()` 里
径向项 `x += t*nx`（`& y += t*ny`）**不受 `amp_mul` 缩放**（只有 `sin(...)*amp*8` 那项乘了 `mul`）✗

**改动（最小、默认不生效）**：
- 新增 ini `retire`（默认 **0**）+ 静态 `g_retire_logged`；
- `hook_upload()` 里，若 `retire=1` 且是舞台网格/允许碰敌方网格 ⇒ 把该网格**所有顶点 alpha 置 0** 后 `return`
  （既不再扭、也看不见）；同时打一行 `[退休] retire=1 ⇒ 网格 %dx%d 全部 alpha=0`；
- `retire=0` 时**行为与改动前逐字一致**（只在 ini 读到 1 时才走新分支）✓

**怎么验证**：`game/th18_bgwarp.ini` 里 `retire = 1` ⇒ 进 boss 战 ⇒ 日志出现 `[退休] …`
且**画面里 boss 那个扭曲消失**；再配合 `game/th18_routeb.ini` 的 `two = 1`（我们画两份）⇒ 两边都在且无叠影 ✓
