# anm — ANM 精灵动画 VM

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th18:0x405bf0`）。

东方里几乎所有「显示」都挂在 `.anm` 上：创建一个显示对象 = 选一段 anm 脚本/模板，
由 anm VM 每帧驱动它的精灵、变换、特效。它因此是**图形侧的总接缝**——
MSG 的文本绘制走它（[`../msg/OVERVIEW.md`](../msg/OVERVIEW.md)）、弹幕的外观也走它。

## TH06NC 快照

TH06NC 不是这里描述的 v8 架构：它是 **ANM v0 容器 + TH06 式单函数解释器**
（`th06nc:0x1400069b0`），指令集 **0–35**，跳表 `th06nc:0x140007494` 共 36 条。
比 TH06 多出的四条（32/33/34/35）全是无参数的锚点设置，把 TH06 的
「bit8=左 / bit9=上」两个开关位改成了 flags bit 8–10 的 3 位枚举，凑成 3×2 锚点网格；
opcode 23 的位型跟着变了但**含义仍是左上**。证据见
[engine/anm/th06nc](th06nc/01-vm-and-anchor-opcodes.md)。

⚠️ 因此 **TH06 的 `flags` 位型不能直接套 NC**——能沿用的只有 opcode 层面的语义。

## 锚点（anchor）的跨版本演化

同一个概念在 ANM 各版本里的表达方式变了三次，**记号和能力都不通用**：

| ANM 版本 | 作品 | 指令 | 横向 | 纵向 | 像素微调 | 可信度 |
| --- | --- | --- | --- | --- | --- | :---: |
| v0 | TH06 | `23 anchorTopLeft`（无参） | 中 / 左 | 中 / 上 | 无 | 🟡 社区 mapfile |
| v0 | **TH06NC** | `23` + 新增 `32/33/34/35`（5 条无参） | 中 / 左 / 右 | 中 / 上 | 无 | ✅ 一手 |
| v2 | TH07 | `22 anchorTopLeft`（无参） | 中 / 左 | 中 / 上 | 无 | 🟡 社区 mapfile |
| v4 | TH09.5 起 | `65 anchor`（**带参**） | ❓ | ❓ | ❓ | 🟡 仅凭改名推断 |
| v8 | TH13 起 | `421 anchor(h, v)` + `436 anchorOffset(dx, dy)` | 0 中 / 1 左 / 2 右 | 0 中 / 1 顶 / **2 底** | 有 | ✅ 一手（取值 🟡） |

三条值得记住的差异：

1. **只有 v4 以后是「一条带参指令」**；v0/v2 和 TH06NC 都是「一个组合一条无参 opcode」。
   TH06NC 选择沿用后者，代价是补一格就要加一条 opcode，好处是 v0 容器与工具链形状不变。
2. **TH06NC 没有「下对齐」**：它是 3×2 = 6 格，TH16 是 3×3 = 9 格。
   想让精灵贴底边对齐，NC 的 ANM 做不到。
3. **位型不通用**：TH16 的 h/v 是 `flags_lo` 的两个独立 2 位字段（`th16:0x4617a1`，
   bits 21–22 / 23–24）；TH06NC 是 `flags` bit 8–10 的**单个 3 位枚举**；
   TH06 又是 bit8/bit9 两个独立开关位。三种编码互不兼容，**只有 opcode 层面的语义能沿用**。

两边都留了没用满的编码空间：TH16 的 2 位字段取值范围 0–3 而社区只记了 0/1/2；
TH06NC 的 3 位字段里 6/7 无 opcode 可达。

细节见 [th16/07-general.md §6.1](th16/07-general.md#61-421-anchor-是一个-u32不是两个-s16)
与 [th06nc/01-vm-and-anchor-opcodes.md §4](th06nc/01-vm-and-anchor-opcodes.md)。
社区 mapfile：`local/vendor/truth/map/v0.anmm`、`v2.anmm`、`v4.anmm`、`v8.anmm`
——**其中 v0 表只到 opcode 31，TH06NC 新增的 32–35 社区无记录**。

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
| System 组终止四态：`0 nop` / `1 delete` / `2 static`（冻结但 `alive` 不变）/ `3 stop`（自锁但可被 interrupt 唤醒）/ `4 stopHide`（清可见位后**直落 `stop` 主体**） | ✅ | ❓ | [th16/05-system](th16/05-system.md) §3/§8.1（`th16:0x4629dc`） |
| `5 case` / `7 caseReturn` 的 `caseReturn` 跳回 **DISPATCH** `th16:0x45fba0` 而非帧尾 | ✅ | ❓ | [th16/05-system](th16/05-system.md) §3（`th16:0x45fcc1`） |
| `300 sprite` 的 `id < 0` 走「系统贴图」分支（ascii.anm 的 sprite `0x102`），**是热路径**：4683 条里 80 条 `arg0 == -1` | ✅ | ❓ | [th16/06-settings](th16/06-settings.md) §2.1（`th16:0x460bf8`） |
| Settings 组的 flag 位归属：`306`→`flags_lo` bit15、`316/317`→bit1（`draw_vm` 双闸门）、`307`→`flags_hi` bit10（**无任何读者，死位**） | ✅ | ❓ | [th16/06-settings](th16/06-settings.md) §3（全 `.text` 扫描 + negative set） |
| 插值曲线共 31 个 mode（跳表 `th16:0x403a10`），mode 0 / 负 / >31 落 linear；22–26 = `g_c(u)` 下凹会 < 0，**27–31 = `1−g_c(1−u)`** 上凸会 > 1 | ✅ | ❓ | [th16/07-general](th16/07-general.md) §4（`th16:0x4038e7`；峰值 1.125–1.800 数值复算吻合） |
| `433` 是极坐标 `(cos θ·r, sin θ·r)`（`fsincos` 后 ST0 = cos），社区未记 | ✅ | ❓ | [th16/07-general](th16/07-general.md) §6.2（`th16:0x464d60`） |
| child 出生时**不拷父的变换字段**，只继承 layer / root / `flags_hi` bit25；而 `createRoot` 把源的 `pos +0x2c..0x34` 拷进新 VM 的 **`pos_2 +0x4e0..0x4e8`**（非同偏移） | ✅ | ❓ | [th16/08-child](th16/08-child.md) §3；[th16/02-vm-core](th16/02-vm-core.md) §8.1 |
| ANM 绘制路径的 render state：全局重置 8 项（`ALPHAREF=1`/`ALPHAFUNC=7`/`ZFUNC=8`/alpha test 与 blend 均 enable）；**`COLORARG1`/`ALPHAARG1` 从不设 ⇒ 恒 `D3DTA_TEXTURE`**；blend mode 共 10 种，唯 mode 3 关 `ALPHATESTENABLE` | ✅ | 🟡 | [th16/09-drawing](th16/09-drawing.md) §4（`th16:0x45ba80` / `th16:0x465254`） |
| Drawing 组只**置状态**不发 draw call；真正的绘制在 `draw_vm` 按 `render_mode` 分派（28 个 mode，索引表 `th16:0x468bac` / 跳表 `th16:0x468b70`） | ✅ | 🟡 | [th16/09-drawing](th16/09-drawing.md) §1/§3 |
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
