# anm — ANM 精灵动画 VM

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th18:0x405bf0`）。

东方里几乎所有「显示」都挂在 `.anm` 上：创建一个显示对象 = 选一段 anm 脚本/模板，
由 anm VM 每帧驱动它的精灵、变换、特效。它因此是**图形侧的总接缝**——
MSG 的文本绘制走它（[`../msg/OVERVIEW.md`](../msg/OVERVIEW.md)）、弹幕的外观也走它。

## 断言 × 版本矩阵

| 断言 | th16 | th18 | 证据 |
| --- | :---: | :---: | --- |
| 存在 anm 脚本 VM，每帧驱动显示对象 | ✅ | 🟡 | [th16/02-vm-core](th16/02-vm-core.md) §1（一手反 `th16:0x45f980`） |
| VM = 单函数解释器 `th16:0x45f980`，主循环 `th16:0x45fba0`，时间闸门 `ins.time <= vm.time` | ✅ | ❓ | [th16/02-vm-core](th16/02-vm-core.md) §2 |
| 脚本时间每帧 `+= GAME_SPEED`；`6 wait n` 是把时间往回拨 n，不是设唤醒点 | ✅ | ❓ | [th16/02-vm-core](th16/02-vm-core.md) §4 |
| interrupt = 从脚本开头线性扫 `ins 5 <n>`；`ins 5 -1` 是 default；命中后时间设成标签的 `time` | ✅ | ❓ | [th16/02-vm-core](th16/02-vm-core.md) §5 |
| `opcode -1`（跑到末尾）与 `1 delete` 共用一段代码，都返回「请销毁」；`2 static` 冻结但保留 | ✅ | ❓ | [th16/02-vm-core](th16/02-vm-core.md) §6 |
| 分发是三段式跳表：`th16:0x462cb8`/`th16:0x462e64`，共 134 个已实现 opcode | ✅ | ❓ | [th16/02-vm-core](th16/02-vm-core.md) §7 |
| `AnmLoaded__instantiate_vm_to_world_list_back(anm; out_id, script, layer, out_vm)` thiscall ret 0x10，实体坐标 (0,0,0) | — | ✅ | [th18/01-vm-instantiate](th18/01-vm-instantiate.md) |
| `AnmLoaded__set_sprite(anm; vm, idx)` thiscall ret 8；按 id 删 `th18:0x488cf0` stdcall ret 4 | — | ✅ | 同上 |
| `.anm` v8 = entry 单向链表，header 固定 0x40 字节，表内偏移一律相对 entry | ✅ | 🟡 | [th16/01-format](th16/01-format.md) §3 |
| script 表每项 8 字节 `{i32 id; u32 offset}`，**id 不参与查找**，运行时按位置索引 | ✅ | 🟡 | [th16/01-format](th16/01-format.md) §5 |
| 指令头 8 字节 `{s16 op; u16 size; s16 time; u16 param_mask}`，`op=-1` 结束且实占 8 字节 | ✅ | 🟡 | [th16/01-format](th16/01-format.md) §5.1 |
| entry 的 `width`/`height` 是贴图**分配**尺寸，真实图像尺寸在 THTX 头里 | ✅ | 🟡 | [th16/01-format](th16/01-format.md) §3.2 |
| `format` 是 9 项查表下标（bpp `th16:0x491b90` / D3DFORMAT `th16:0x491bb4`）；7 = `D3DFMT_A8` | ✅ | ❓ | [th16/01-format](th16/01-format.md) §7 |
| TH16 加载器只接受 `version == 8`，没有旧版本分支 | ✅ | ❓ | [th16/01-format](th16/01-format.md) §9 |
| 脚本表在 `anm+0x10c`，模板步长 0x60c（th16 0x5fc） | 🟡 | ✅ | 同上 §2 |
| 跳转的 `dest` 是**脚本内字节偏移**（不是指令下标）；`instr_offset` 裸赋值、取指裸相加，全路径无乘法 | ✅ | ❓ | [th16/03-math-jumps](th16/03-math-jumps.md) §5（`th16:0x45fc0d`；857/857 真实跳转落在指令边界） |
| 跳转成功时 `script_time` 被 `Timer::set_value(t)` **覆盖成参数 `t`**，与目标指令的 `time` 标签无关 | ✅ | ❓ | [th16/03-math-jumps](th16/03-math-jumps.md) §6（`th16:0x406490`；857 条里 106 条 `t ≠ 目标 time`） |
| `I0–I3` = 变量号 10000–10003、`F0–F3` = 10004–10007，且**变量号的编码宽度跟指令类型走**（整型指令编 int32、浮点指令编 float32；跨类型是转换不是重解释位） | ✅ | ❓ | [th16/03-math-jumps](th16/03-math-jumps.md) §3（`th16:0x45f890` / `th16:0x45f780` 两张索引表） |
| 整数除 / 取模**没有零守卫**：`b == 0` 与 `INT_MIN / -1` 都是裸 `idiv` ⇒ x86 `#DE`；浮点除走 IEEE 不崩 | ✅ | ❓ | [th16/03-math-jumps](th16/03-math-jumps.md) §4（`th16:0x4602fa` 等四条 case） |
| `131 mathCirclePosRand` 的半径**不是** `[lo, hi]`：一手是 `lo + s·(hi − lo)`、`s ∈ [−1, 1)` ⇒ 区间宽一倍且向下越过 `lo`（社区判错） | ✅ | ❓ | [th16/03-math-jumps](th16/03-math-jumps.md) §7.2（`th16:0x46075e`–`th16:0x460775`） |
| 操作数求值次序共**六种形态**，不是「一律下标升序」；判据 = case 体里 `test byte [edi+6]` 的执行次序 | ✅ | ❓ | [th16/03-math-jumps](th16/03-math-jumps.md) §9.1（46/46 穷举） |
| 变量号 10000–10035 共 36 条；可写 22 / 只读 14；`$` int 8 个、`%` float 28 个；整型读对 19 个浮点号返回变量号本身（跨类型读非对称） | ✅ | ❓ | [th16/04-variables](th16/04-variables.md) §2/§3/§4（`th16:0x45f578` 跳表 + 两张 lvalue 表穷举） |
| 随机族：10010–10012、10022 走 Stream B `th16:0x4a6d80`；10030–10032 走 Stream A `th16:0x4a6d88`；10022 浮读 = u32→float 不取模、整读 = `raw % +0x4dc` | ✅ | ❓ | [th16/04-variables](th16/04-variables.md) §5；[th16/03-math-jumps](th16/03-math-jumps.md) §7.4 |
| 10026 = 有效 z 旋转（`th16:0x464dd0` 沿 `+0x5ac` 递归累积、`flags_hi & 0x10000` 闸门）；10016–10021 读舞台相机镜像块（非 vm 字段） | ✅ | ❓ | [th16/04-variables](th16/04-variables.md) §6 |
| 渲染阶段 × 相机 × 层：层 0–2 相机 2、3–19 相机 3、20–23 相机 1、24+ 相机 2；四台相机全透视（LookAt + PerspectiveFov，无 Ortho） | — | ✅ | [th18/02-render-stages](th18/02-render-stages.md) |
| 渲染模式 8 = D3D WORLD 矩阵 + 单位四边形；2D 路径自算顶点并加相机 2D 偏移（只有相机 2 非零） | — | ✅ | [th18/01 §3b](th18/01-vm-instantiate.md) |

> **图例**：✅ 该版本一手验过（证据列给地址/出处） · 🟡 待验（从别的版本借来的假设，或单源） · ❌ 已知不同/不存在 · ❓ 存疑 · — 未看
>
> **本页不许出现没有出处的断言。** 从某作借到另一作的判断一律 🟡，在该版本 exe 上验过才能改 ✅（[`METHOD.md`](../../METHOD.md)）。

## 工具与样本（2026-09-04 就位）

- 标准 thtk（thanm / thdat，release 12）本机编好：[`tooling/thtk/`](../../tooling/thtk/README.md)；`unpack.py` 把 th18 的
  56 个 anm 全解成「一目录 = spec + 贴图」（`local/th18.v1.00a/anm/`，不入库）。`-l` → `-c` 往返字节一致。
- anmmap：thpages `v8.anmm`（th18 在用，150 条指令名）——这是 opcode 语义研究时的社区参照，运行时实现仍须在 exe 上自证。
- 第一个产物线：card-expand 的卡图追加（`mods/th18.v1.00a/card-expand/assets/`）。它只动格式层，没有引擎结论；
  引擎侧待答的第一问是 **运行时 sprite 数上限**（`AnmManager__preload_anm` → `AnmLoaded`）。

## 开工前必读

[th16/README.md](th16/README.md) 里的「起步锚点」全部是做 [`../bullet/`](../bullet/OVERVIEW.md)
时**顺带掀开的口子**，多为 🟡（call-site / 二手）。**动手时务必自己反编译复核，别当定论。**

ExpHP `th-re-data` 对 `zAnmVm` / `AnmManager` 已有大量命名（TH16 AnmVm 45 / AnmManager 38），
起步先套（[`../../tooling/ghidra/bootstrap.py`](../../tooling/ghidra/bootstrap.py)）。
