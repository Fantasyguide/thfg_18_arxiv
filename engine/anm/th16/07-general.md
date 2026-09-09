# ANM v8 General 组 — TH16 一手逐条确认（opcode 400–438，39 条）

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：不经 Ghidra 渲染——解 PE 节表把 `.text` 字节喂 capstone，逐条反汇编 39 个 case 体、12 个插值器 step/set、
> 缓动曲线分派 `interp_common_methods` `0x4033f0` 及其 31 项跳表 `0x403a10`；「opcode → case 地址」由高段分发表
> （`0x462fa8` 索引表 + `0x462e64` 跳表）逐字节解出；再用自写解析器跑 `local/th16.v1.00a/dat/` 的 **54 个真实 `.anm`
> （1929 script / 41339 指令，其中本组 17453 条）**做对账与证伪。
> **可信度**：39 条的签名、目标字段、flag 位全部 ✅（每条断言背后是一条读/写该字段的机器指令）；
> **缓动曲线编号与公式 ✅**（`0x4033f0` 分派表 + 常量池逐字节解出）；`411 rotateTime2D` 的 mode 8 角度 Hermite 精确公式 🟡
> （依赖 `0x4475f0`/`0x447650` 两个角度 helper，本文未逐条解码其数学）；`422 posAdopt`、`431 v8-flag-431` 两条例外见 [§8](#8-社区标-wip2-的两条)。
> **衔接**：指令头 8 字节与 `param_mask` 见 [`01-format.md`](01-format.md) §5；VM 主循环、帧尾、`zAnmVm` 字段图见
> [`02-vm-core.md`](02-vm-core.md)；变量参数一律引 [`04-variables.md`](04-variables.md) 的 36 号表，本文不重推。
> **不含版权字节**：只写偏移、地址与语义。

## 0. 一句话结论

这 39 条是 ANM VM 的**变换与外观层**：16 条直接写字段（`pos`/`rotate`/`scale`/颜色/速度/锚点…），
1 条逐帧速度积分（`415 angleVel`/`416 scaleGrowth`/`425 uVel`/`426 vVel` 置 `flags_hi & 0x1000000`），
1 条 UV 重算（`418`），6 条写 flag 位，**13 条是插值指令（`*Time` 家族）**——它们把「起点 = 当前值、
终点 = 参数值、时长 = t 帧、曲线 = mode 号」写进 12 组帧尾插值器之一。四件最容易搞错的事：

1. **缓动曲线编号是一张 32 项的表，编号顺序不按「难度」排**：`0=linear`、`1/2/3=t²/t³/t⁴`、
   `4/5/6=1-(1-t)²/³/⁴`、`7/8/17=linear`（但 step 层特判成别的）、`9–14=in-out/out-in`、`15/16=常数 0/1`、
   `18–21=正弦族`、`22–31=高斯凹凸族`。公式见 [§4](#4-专项缓动曲线编号与公式)。
2. **`mode 8` 对标准 `*Time` 指令 = smoothstep**（`3u²−2u³`），因为它们的两个控制点（Hermite 切线）恒从全局
   `0x4d9dc4` 读，而该全局是 `.data` BSS 零段、全文无一处写 ⇒ 恒 `(0,0,0)`；只有 `420 posBezier` 显式传入切线，
   才得到真正的三次 Hermite。见 [§5](#5-专项-mode-78-与-17-在-step-层的特判)。
3. **`433 posTime2D` 不是「2D 版 posTime」**，而是**极坐标目标**：`目标 = (cos(x)·y, sin(x)·y, 0)`——
   arg2 是角度、arg3 是半径，且 z 分量会一路插值到 0。社区标 `wip:1`，本文一手钉死 ✅。见 [§6.2](#62-433-posTime2D-是极坐标目标不是-2D-版-posTime)。
4. **`mode` 参数恒为字面量**（13 条 `*Time` 指令的 `arg1` 从不查 `param_mask`）；`t` 与各分量参数可以查。
   `421 anchor` 是**一个 u32 参数**（低 16 位 h、高 16 位 v），不是两个 2 字节参数。见 [§2](#2-39-条指令表) 与 [§6.1](#61-421-anchor-是一个-u32-不是两个-s16)。

---

## 1. 怎么核的：opcode → case 地址是自己解跳表得到的

**① 发现**：高段分派序言在 `0x460c46`：`SUB EAX,0x12d; CMP EAX,0x138; JA 0x4629c8`（默认），
`MOVZX EAX,byte [EAX+0x462fa8]; JMP [EAX*4+0x462e64]`。

**② 推测**：opcode 400–438 落在高段（`301 ≤ op ≤ 613`），index = opcode − 301 = 99…137。

**③ 验证**：直读 PE 解出 `0x462fa8`（313 字节索引表，`0x50`=80 = 默认槽）与 `0x462e64`（81 项跳表），
得到 opcode → case 地址映射。**本组 39 条全部落在 `AnmVm::run` 内**（`0x460c64`–`0x46299c`），与 thpages
的 opcode 号逐条吻合，0 处错位。相邻的 300 系 / 600 系 case（`0x460c9d`、`0x462608` 等）是**别的组**，不在本文。

**④ 结论**：本文全部 39 个 case 地址是一手的，不依赖社区 label ✅（TH16）。

**⑤ 证据**：`0x460c46`–`0x460c5d`；表 `0x462fa8`（`u8[313]`）与 `0x462e64`（`void*[81]`）；case 地址见 §2 表。

### 1.1 每条 case 的公共骨架

与 [`03-math-jumps.md`](03-math-jumps.md) §1.1 相同：`EBX = zAnmVm*`，`EDI = 当前指令指针`，
`ins.args[i] = *(u32*)(EDI + 8 + 4*i)`，`param_mask` 的 `bit i` 管 `arg i`。右值 = mask 置位查变量、否则字面量；
左值 = 写 vm 字段。收尾 `ADD [EBX+0x28], ins.size` 后 `JMP 0x45fb9a`。变量参数编码宽度跟指令类型走
（整型写 int32、浮点写 float32），见 [`03-math-jumps.md`](03-math-jumps.md) §3。

**本组两个与 100 系不同的骨架细节**：

- **flag 写法的两种收尾**：写 `flags_lo`（`+0x530`）的 case 就地 `XOR [EBX+0x530], EAX` 再推进指针；
  写 `flags_hi`（`+0x534`）的 case 跳到共享尾巴 `0x4629c2`（`XOR [EBX+0x534], EAX`）再落进默认分支 `0x4629c8`。
- **`mode` 参数恒为字面量**：13 条 `*Time` 的 `arg1` 只做 `MOV EAX/MOVZX,[EDI+0xC]`，从不 `TEST [EDI+6]`。
  `420 posBezier` 无 mode（硬编码 `8`）。

---

## 2. ★ 39 条指令表

**记号**：`$` = int32 右值（`get_int_var`）· `f` = float32 右值（`get_float_var`）· 目标字段的偏移见
[`02-vm-core.md`](02-vm-core.md) §3.3。`*Time` 家族的 `t` 恒为 arg0（可变量）、`mode` 恒为 arg1（字面量）。
签名列从左到右就是 `arg0, arg1, …`。

| opcode | 助记 | 签名 | 语义 | case 地址 | 可信度 |
| :---: | --- | --- | --- | --- | :---: |
| 400 | `pos` | `fff` | `pos = (x,y,z)`；`flags_lo & 0x400` 时写 `+0x4e0`（`pos_2`）而非 `+0x2c` | `0x46159c` | ✅ |
| 401 | `rotate` | `fff` | `rotation = (rx,ry,rz)`；`flags_lo \|= 4` | `0x4613a6` | ✅ |
| 402 | `scale` | `ff` | `scale = (sx,sy)`；`flags_lo \|= 8` | `0x461006` | ✅ |
| 403 | `alpha` | `$` | `color_1.A = (u8)arg0` | `0x461165` | ✅ |
| 404 | `rgb` | `$$$` | `color_1 = (r,g,b)`（低字节写 `+0x522/+0x521/+0x520`） | `0x4611ac` | ✅ |
| 405 | `alpha2` | `$` | `color_2.A = (u8)arg0` | `0x461229` | ✅ |
| 406 | `rgb2` | `$$$` | `color_2 = (r,g,b)`（`+0x526/+0x525/+0x524`） | `0x461270` | ✅ |
| 407 | `posTime` | `$$fff` | 插值 `pos`，t 帧，mode 曲线；控制点 = `0x4d9dc4` | `0x4618d4` | ✅ |
| 408 | `rgbTime` | `$$$$$` | 插值 `color_1` RGB，t 帧，mode 曲线 | `0x461d09` | ✅ |
| 409 | `alphaTime` | `$$$` | 插值 `color_1` A，t 帧，mode 曲线 | `0x461dc4` | ✅ |
| 410 | `rotateTime` | `$$fff` | 插值 `rotation`，t 帧，mode 曲线；控制点 = `0x4d9dc4` | `0x461f2f` | ✅ |
| 411 | `rotateTime2D` | `$$f` | 插值 `rotation.z`（角度感知，取最短路径），t 帧 | `0x46204b` | ✅ |
| 412 | `scaleTime` | `$$ff` | 插值 `scale`，t 帧，mode 曲线 | `0x462102` | ✅ |
| 413 | `rgb2Time` | `$$$$$` | 插值 `color_2` RGB，t 帧，mode 曲线 | `0x461e1c` | ✅ |
| 414 | `alpha2Time` | `$$$` | 插值 `color_2` A，t 帧，mode 曲线 | `0x461ed7` | ✅ |
| 415 | `angleVel` | `fff` | `angular_velocity = (ωx,ωy,ωz)`；`flags_hi \|= 0x1000000` | `0x461439` | ✅ |
| 416 | `scaleGrowth` | `ff` | `scale_growth = (gx,gy)`；`flags_hi \|= 0x1000000` | `0x4614c9` | ✅ |
| 417 | `alphaTimeLinear` | `$$` | `alphaTime` 的线性特例：`(alpha, t)`，mode 硬编码 0 | `0x46152f` | ✅ |
| 418 | `v8-418` | — | 重算 sprite UV 四角（见 [§6.3](#63-418419-重算-uv-四角)） | `0x462564` | ✅ |
| 419 | `v8-flag-419` | `$` | `flags_hi` bit13 = `0x2000`（每帧重算 UV） | `0x4625cf` | ✅ |
| 420 | `posBezier` | `$fffffffff` | 三次 Hermite 移到位（切线 = 参数），见 [§6.4](#64-420-posBezier-是-hermite-不是-bezier) | `0x461b25` | ✅ |
| 421 | `anchor` | `$`(1 个 u32) | h = 低 16 位、v = 高 16 位；见 [§6.1](#61-421-anchor-是一个-u32-不是两个-s16) | `0x4617a1` | ✅ |
| 422 | `posAdopt` | — | `pos = entity_pos; entity_pos = 0` | `0x462424` | ✅ 机制 / 🟡 语义 |
| 423 | `colorMode` | `$`(byte) | `flags_lo` bits17–18（2 位，0/1/2/3） | `0x462970` | ✅ |
| 424 | `rotateAuto` | `$`(byte) | `flags_hi` bit7 = `0x80` | `0x46299c` | ✅ |
| 425 | `uVel` | `f` | `uv_scroll_vel.x = vel`；`flags_hi \|= 0x1000000` | `0x4617ea` | ✅ |
| 426 | `vVel` | `f` | `uv_scroll_vel.y = vel`；`flags_hi \|= 0x1000000` | `0x461829` | ✅ |
| 427 | `uVelTime` | `$$f` | 插值 `uv_scroll_vel.x`，t 帧 | `0x4622c4` | ✅ |
| 428 | `vVelTime` | `$$f` | 插值 `uv_scroll_vel.y`，t 帧 | `0x462350` | ✅ |
| 429 | `uvScale` | `ff` | `uv_scale = (u,v)`；`flags_lo \|= 0x10` | `0x4610f0` | ✅ |
| 430 | `uvScaleTime` | `$$ff` | 插值 `uv_scale`，t 帧 | `0x462229` | ✅ |
| 431 | `v8-flag-431` | `$`(byte) | `flags_hi` bit8 = `0x100`（语义 🟡，见 [§8](#8-社区标-wip2-的两条)） | `0x46293c` | ✅ 机制 / 🟡 语义 |
| 432 | `slowdown-immune` | `$` | `flags_hi` bit9 = `0x200`（= `02` §3.6 的「强制 GAME_SPEED=1.0」） | `0x460c64` | ✅ |
| 433 | `posTime2D` | `$$ff` | **极坐标目标**，见 [§6.2](#62-433-posTime2D-是极坐标目标不是-2D-版-posTime) | `0x4619fe` | ✅ |
| 434 | `scale2` | `ff` | `scale_2 = (sx,sy)`；`flags_lo \|= 8` | `0x46107b` | ✅ |
| 435 | `scale2Time` | `$$ff` | 插值 `scale_2`，t 帧 | `0x462194` | ✅ |
| 436 | `anchorOffset` | `ff` | `anchor_offset = (dx,dy)` | `0x4616d6` | ✅ |
| 437 | `rotationSystem` | `$` | `flags_hi` bits2–4（3 位，旋转次序约定） | `0x461746` | ✅ |
| 438 | `originMode` | `$`(byte) | `flags_hi` bits18–19（2 位，原点模式） | `0x462925` | ✅ |

### 2.1 flag 位总表（本组坐实的）

| 位（`+0x530` = `flags_lo` / `+0x534` = `flags_hi`） | 谁写 | 语义（本组视角） |
| --- | --- | --- |
| `lo` 21–22（`0x600000`） | 421 h | 水平锚点 0=中 1=左 2=右 |
| `lo` 23–24（`0x1800000`） | 421 v | 垂直锚点 0=中 1=顶 2=底 |
| `lo` 17–18（`0x60000`） | 423 | 颜色模式（0/1/2/3） |
| `hi` 2–4（`0x1c`） | 437 | 3D 旋转次序（0–7） |
| `hi` 7（`0x80`） | 424 | 自动转向（朝运动方向） |
| `hi` 8（`0x100`） | 431 | 🟡 未知 bitflag |
| `hi` 9（`0x200`） | 432 | 本 VM 强制 `GAME_SPEED=1.0`（= `02` §3.6） |
| `hi` 13（`0x2000`） | 419 | 帧尾重算 sprite UV 四角（= `02` §3.6） |
| `hi` 18–19（`0xc0000`） | 438 | 原点模式（0/1/2） |

> **可信度分层**：上表「谁写」与「位位置/位宽」是 ✅（每条对到 `SHL/AND/XOR` 的立即数）；「0=中 1=左 2=右」这类
> **取值含义**来自社区（thpages `anchor`/`colorMode`/`originMode`/`rotationSystem`，`originMode` 还标 `wip:1`），🟡。
> `anchor` 的 2 位取值范围是 0–3，社区只记了 0/1/2，值 3 无记录。

**求值顺序**：本组写字段的指令大多「右值在前、写字段在后」；`415 angleVel`/`416 scaleGrowth` 是
「后两个参数先读、arg0 最后」（与 `03-math-jumps.md` §9.1 的形态 C 同款）。flag 指令只读 arg0。
完整判据（`TEST byte [EDI+6], 1<<i` 的执行序）与 `03-math-jumps.md` §9.1 相同，这里不再逐条列。

---

## 3. 插值器架构（12 组，`*Time` 指令写它们）

**① 发现**：`AnmVm::step_interpolators` `0x463b30` 逐组检查「剩余帧闸门」dword，非 0 才调用该组的 step 函数，
结果写回目标字段。**② 推测**：`*Time` 指令 = 往这些结构里填 (起点, 终点, 切线, 闸门, mode)。**③ 验证**：逐条反汇编
`0x463b30` + 各 step/set 函数。**④ 结论**（✅ TH16）：12 组与 [`02-vm-core.md`](02-vm-core.md) §3.4 的表一致，本文补上
**每组的结构布局与 step/set 函数**：

| 目标字段 | 基址 | 闸门 | step 函数 | set 函数（哪条指令用） |
| --- | --- | --- | --- | --- |
| `pos` `+0x2c` | `+0x8c` | `+0xdc` | `InterpFloat3::step` `0x406e10` | 内联（407/420/433） |
| `color_1` RGB | `+0xe4` | `+0x134` | `InterpInt3::step` `0x464590` | `0x464c60`（408） |
| `color_1` A | `+0x13c` | `+0x164` | `InterpInt::step` `0x4643b0` | `0x406ce0`（409/417） |
| `rotation` `+0x38` | `+0x16c` | `+0x1bc` | `0x406e10` | 内联（410） |
| `rotation.z`（2D） | `+0x1c4` | `+0x1ec` | `InterpAngle::step` `0x464080` | 内联（411） |
| `scale` `+0x50` | `+0x1f4` | `+0x230` | `InterpFloat2::step` `0x463d40` | `0x447550`（412） |
| `scale_2` `+0x58` | `+0x238` | `+0x274` | `0x463d40` | `0x464a00`（435） |
| `uv_scale` `+0x68` | `+0x27c` | `+0x2b8` | `0x463d40` | `0x464960`（430） |
| `color_2` RGB | `+0x2c0` | `+0x310` | `0x464590` | `0x464b40`（413） |
| `color_2` A | `+0x318` | `+0x340` | `0x4643b0` | `0x464aa0`（414） |
| `uv_scroll_vel.x` | `+0x348` | `+0x370` | `InterpFloat::step` `0x4171c0` | 内联（427） |
| `uv_scroll_vel.y` | `+0x378` | `+0x3a0` | `0x4171c0` | 内联（428） |

**结构布局**（`start`/`target`/`tangent1`/`tangent2`/`output` 均为值；`timer` = 内嵌 zTimer，速度源 = `0x490eb0[0]` = `GAME_SPEED`）：

| 类型 | start | target | tangent1 | tangent2 | output | timer | 闸门 | mode |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `InterpFloat3` | `+0x00`×3 | `+0x0c`×3 | `+0x18`×3 | `+0x24`×3 | `+0x30`×3 | `+0x3c` | `+0x50` | `+0x54` |
| `InterpFloat2` | `+0x00`×2 | `+0x08`×2 | `+0x10`×2 | `+0x18`×2 | `+0x20`×2 | `+0x28` | `+0x3c` | `+0x40` |
| `InterpInt3` | `+0x00`×3 | `+0x0c`×3 | `+0x18`×3 | `+0x24`×3 | `+0x30`×3 | `+0x3c` | `+0x50` | `+0x54` |
| `InterpInt` | `+0x00` | `+0x04` | `+0x08` | `+0x0c` | `+0x10` | `+0x14` | `+0x28` | `+0x2c` |
| `InterpAngle` | `+0x00` | `+0x04` | `+0x08` | `+0x0c` | `+0x10` | `+0x14` | `+0x28` | `+0x2c` |
| `InterpFloat` | `+0x00` | `+0x04` | `+0x08` | `+0x0c` | `+0x10` | `+0x14` | `+0x28` | `+0x2c` |

**step 的公共骨架**（每种类型一致，✅）：`if (闸门 <= 0) return 当前 output`；否则内嵌 zTimer `increment` 一步
（`current += GAME_SPEED`）；`if (current < 闸门)` 算中间值（`start + (target−start)·ease(mode, current/闸门)`，或 mode 7/8/17 特判）；
否则 `闸门 = 0`，output = target（mode 7/17 时 = 累计后的 start）。**闸门 = `t` 帧**（`GAME_SPEED=1` 时正好 t 帧）。

---

## 4. ★ 专项：缓动曲线编号与公式

**① 发现**：`interp_common_methods` `0x4033f0` 入口 `LEA EAX,[ECX−1]; DIVSS XMM3,XMM2; CMP EAX,0x1e; JA 线性;
JMP [EAX*4+`0x403a10`]`——`u = current_f / gate`，mode 1–31 各落一条跳表项，mode 0 / 负数 / >31 走默认 = 线性。
**② 推测**：这是一张 32 模式的缓动表。**③ 验证**：直读跳表 `0x403a10`（31 项）与常量池
（`0x4944d8`=1.0、`0x49449c`=0.5、`0x494558`=2.0、`0x494584`=3.0、`0x494588`=π、`0x494534`=π/2、
`0x4945b8`=2π、`0x494734`=−π），逐条解码每个 case。**④ 结论**（✅ TH16）：公式如下。

**⑤ 证据**：`0x4033f0`–`0x403a10`；跳表 `0x403a10`（`void*[31]`）；常量地址见上。

### 4.1 mode 表（u = 已过帧数 / t，`ease(mode, u)`）

| mode | 公式 | 通俗名 |
| :---: | --- | --- |
| 0 | `u` | linear |
| 1 | `u²` | ease-in 二次 |
| 2 | `u³` | ease-in 三次 |
| 3 | `u⁴` | ease-in 四次 |
| 4 | `1−(1−u)²` | ease-out 二次 |
| 5 | `1−(1−u)³` | ease-out 三次 |
| 6 | `1−(1−u)⁴` | ease-out 四次 |
| 7 | `u`（step 层特判为「逐帧累加」，见 [§5](#5-专项-mode-78-与-17-在-step-层的特判)） | — |
| 8 | `u`（step 层特判为「Hermite」，见 [§5](#5-专项-mode-78-与-17-在-step-层的特判)） | — |
| 9 | `u<½ ? 2u² : 1−2(1−u)²` | in-out 二次 |
| 10 | `u<½ ? 4u³ : 1−4(1−u)³` | in-out 三次 |
| 11 | `u<½ ? 8u⁴ : 1−8(1−u)⁴` | in-out 四次 |
| 12 | `u<½ ? ½−½(1−2u)² : ½+½(2u−1)²` | out-in 二次 |
| 13 | `u<½ ? ½−½(1−2u)³ : ½+½(2u−1)³` | out-in 三次 |
| 14 | `u<½ ? ½−½(1−2u)⁴ : ½+½(2u−1)⁴` | out-in 四次 |
| 15 | `0` | 常数 0 |
| 16 | `1` | 常数 1 |
| 17 | `u`（step 层特判为「加速累加」） | — |
| 18 | `sin(π/2 · u)` | ease-in 正弦 |
| 19 | `1−cos(π/2 · u)` | ease-out 正弦 |
| 20 | `u<½ ? ½·sin(πu) : 1−½·sin(πu)` | in-out 正弦 |
| 21 | `u<½ ? ½(1−cos(πu)) : ½+½·sin((2u−1)·π/2)` | out-in 正弦 |
| 22 | `g_c(u)`，c=0.25 | 下凹（anticipation） |
| 23 | `g_c(u)`，c=0.30 | 下凹 |
| 24 | `g_c(u)`，c=0.35 | 下凹 |
| 25 | `g_c(u)`，c=0.38 | 下凹 |
| 26 | `g_c(u)`，c=0.40 | 下凹 |
| 27–31 | `1−g_c(u)`，c 同 22–26 顺序 | 上凸（overshoot） |
| >31 / 负 | `u` | linear |

### 4.2 高斯族 `g_c(u)` 的闭合式

**① 发现**：mode 22–26 的 case 是「减常数 → 平方 → 除常数 → 减常数 → 除常数」五连击，常量对是
`(0.25, 0.5625, 0.111111, 0.888888)`…`(0.40, 0.36, 0.444444, 0.555556)`。**② 推测**：这是「以 c 为中心的下凹抛物线」。
**③ 验证**：对五组常量，`w=(1−c)²`、`s=c²/w`、`n=1−s` 全部精确成立（例如 c=0.25：`w=0.5625`、`s=1/9`、`n=8/9`）。
**④ 结论**（✅ TH16）：

```
g_c(u) = [ (u−c)² / (1−c)² − c²/(1−c)² ] / [ 1 − c²/(1−c)² ]
        = 0 (u=0)，在 u=c 取到最小值 −c²/((1−c)²−c²)，1 (u=1)
```

c 依次 = `0.25, 0.30, 0.35, 0.38, 0.40`（mode 22→26），凹底深度依次 = `−0.125, −0.225, −0.408, −0.602, −0.800`。
mode 27–31 = `1−g_c(u)`（峰值 = `1.125, 1.225, 1.408, 1.602, 1.800`，上凸到目标之外再回落）。
**播放器注意**：22–26 会在过程中**越过起点往反方向**（ease < 0），27–31 会**越过终点**（ease > 1）——不是「夹在 0 和 1 之间」。
**⑤ 证据**：常量直读 `0x494460/0x49446c/0x494474/0x494480/0x49448c`（c）、`0x4944a4/0x494498/0x494490/0x494484/0x494478`（w）、
`0x494444/0x494450/0x494468/0x49447c/0x494494`（s）、`0x4944c8/0x4944c4/0x4944b8/0x4944a8/0x4944a0`（n）。

### 4.3 浮点精度注意

`interp_common_methods` 全程 float32（`divss/mulss/addss`），只有正弦族在调 `sin`（`0x4884c0`）时
`cvtss2sd`→双精度→`cvtsd2ss` 回 float。播放器用 JS `Math.sin/cos` + `Math.fround` 等价（与
[`03-math-jumps.md`](03-math-jumps.md) §4.2 的精度结论一致）。`t`/`current` 是整数帧计数，`u = current_f/t` 是 float。

---

## 5. 专项：mode 7 / 8 / 17 在 step 层的特判

**① 发现**：`0x4033f0` 对 mode 7/8/17 返回线性 `u`，但各 step 函数**在调用它之前**先对这三个值做了特判
（`InterpFloat2::step` `0x463e3c` 起、`InterpFloat3::step` `0x406f18` 起、`InterpInt::step` `0x464486` 起、
`InterpInt3::step` 同、`InterpAngle::step` `0x464170` 起）。**② 推测**：7/8/17 的真实语义在 step 层。
**③ 验证**：逐条解码每个特判分支。**④ 结论**（✅ TH16）：

| mode | 中间帧（`current < t`） | 结束帧（`current ≥ t`） |
| :---: | --- | --- |
| 7 | `start += target`；output = start | output = start（累计后的值） |
| 17 | `start += tangent2`；`tangent2 += target`；output = start | output = start |
| 8 | Hermite：`start·h00 + target·h11 + tangent1·h10 + tangent2·h01` | output = target |

Hermite 基（`h00=(u−1)²(2u+1)`、`h10=(1−u)²u`、`h01=(u−1)u²`、`h11=u²(3−2u)`，即标准三次 Hermite）：
`output = (2u³−3u²+1)·start + (u³−2u²+u)·tangent1 + (u³−u²)·tangent2 + (3u²−2u³)·target`。

**mode 7 = 逐帧相对位移**（`target` 当每帧步长，t 帧共走 `t×target`）；**mode 17 = 逐帧加速**（`target` 当每帧加速度）；
**mode 8 = 三次 Hermite**（`tangent1`/`tangent2` 是起点/终点切线，等于「每帧速度 × t」）。

### 5.1 mode 8 对标准 `*Time` = smoothstep（关键结论）

**① 发现**：407/410 的 case 体把两组切线从全局 `0x4d9dc4`（`MOVQ XMM0,[0x4d9dc4]` + `MOV EAX,[0x4d9dcc]`）
写进 `+0x18`/`+0x24`；12 组的 set 函数（`0x447550` 等）要么同样读 `0x4d9dc4`、要么把切线写 0。
**② 推测**：`0x4d9dc4` 是共享的「默认切线」常量。**③ 验证**：`.data` 节（VA `0x9d000`，raw 只到 `0xa6400`，
之后 BSS 零填）——`0x4d9dc4` 在 BSS 内；对全 `.text` 扫描，**只有 6 处读 + 2 处 `push 0x4d9dc4`（传地址当输入），0 处写**。
**④ 结论**（✅ TH16）：`0x4d9dc4` 恒 `(0,0,0)` ⇒ **标准 `*Time` 指令的 mode 8 = smoothstep**
`output = start + (target−start)·(3u²−2u³)`。只有 `420 posBezier` 显式传非零切线，mode 8 才是完整 Hermite。
**⑤ 证据**：`0x4618ff`（407 读 `0x4d9dc4`）、`0x461fcd`（410 同）；`0x4d9dc4` 在 `.data` BSS；扫描脚本见本任务报告。

> ⚠️ 这意味着「`posTime(t, 8, x,y,z)`」与「`posTime(t, 9, x,y,z)`」（mode 9 = in-out 二次）画出来**几乎一样但不等价**：
> 8 是 `3u²−2u³`，9 是分段二次。别把 mode 8 当成「Bezier」——Bezier 只有 `420 posBezier` 那一条。

### 5.2 InterpAngle（`411 rotateTime2D`）的差异

`InterpAngle::step` `0x464080` 在 mode 7/17 里**先对角度 wrap**（`0x464199` 起的两段 ±2π 循环，同 `129 mathReduceAngle`），
默认模式走 `interp_common_methods` + `0x405390`（**最短路径角度差**，把 `target−start` 折进 `[−π,π]` 再插值）⇒ 旋转永远走短边、
不会绕 360°。mode 8 走 `0x4475f0`/`0x447650` 两个角度 helper，**其精确 Hermite 数学本文未逐条解码 ⇒ 🟡**（结构 ✅：切线 = 0 + 角度归一）。

---

## 6. 专项：几条「一眼错」的指令

### 6.1 `421 anchor` 是一个 u32，不是两个 s16

**① 发现**：case `0x4617a1` 用 `MOVZX EAX, word [EDI+8]`（低 16 位）+ `MOVZX ECX, word [EDI+0xA]`（高 16 位）。
**② 推测**：h、v 打包在 arg0 的 32 位里。**③ 验证**：真实数据 829 条 `421` **全部只有 1 个参数**（size=12），
`param_mask` 恒 0。**④ 结论**（✅ TH16）：`anchor(h, v)` 编码为**一个 u32**，`h = arg0 & 0xFFFF`、`v = arg0 >> 16`；
h→`flags_lo` bits21–22、v→bits23–24。社区 `sig 'ss'` 描述的字节相同（两个小端 s16 紧排），但**参数边界/数量不同**
——写解析器/汇编器要按「1 个 u32」数参数，否则 `size` 和 `param_mask` 位序会错。
**⑤ 证据**：`0x4617a1`–`0x4617e2`；统计见 [§9](#9-真实数据对账54-个-anm)。

### 6.2 `433 posTime2D` 是极坐标目标，不是「2D 版 posTime」

**① 发现**：case `0x4619fe` 把 arg2、arg3 交给 `0x464d60`（`fld θ; fsincos; fmul r; fstp *ECX; fmul r; fstp *ECX+4`），
结果写进 target.xy，target.z = 0。**② 推测**：`0x464d60` = 极坐标→笛卡尔（与 `130 mathCirclePos` 的 `0x464930` 同款，只是写进 float[2]）。
**③ 验证**：调用点 `MOVSS [ESP], arg2; MOVSS [ESP+4], arg3`，`0x464d60` 对 arg2 做 `fsincos` ⇒ arg2 = 角度、arg3 = 半径。
**④ 结论**（✅ TH16）：**`posTime2D(t, mode, x, y)` = 把位置插值到 `(cos(x)·y, sin(x)·y, 0)`**（z 分量一路到 0），
起点 = 当前 `pos.xyz`。真实数据 4 条全用 mode 5（ease-out 三次）。社区标 `wip:1`，本文升为 ✅。
**⑤ 证据**：`0x4619fe`–`0x461b20`；`0x464d60`（`0x464d6a` `fld`/`0x464d6d` `fsincos`）。

### 6.3 `418`/`419`：重算 UV 四角

`418`（`0x462564`）：调 `recompute_uv_quad` `0x468c00`（按 `flags_lo` bit25–29 的渲染模式算 sprite 四角），
再把 4 个角各过一遍 `0x45f940`（`x/640.0`、`y/480.0`、负值钳 0，常量 `0x4946d4`=640、`0x4946bc`=480）写进
`+0x3a8..0x3c4`。`419` 置 `flags_hi & 0x2000` ⇒ 帧尾自动做同一件事（`02` §2）。✅ 机制；「用坐标选 sprite 区域」的
高层解读 🟡（依赖渲染路径 `0x468c00` 的逐分支几何，归渲染文档）。证据：`0x462564`、`0x45f940`、`0x468c00`、`0x4625cf`。

### 6.4 `420 posBezier` 是 Hermite，不是 Bezier

**① 发现**：case `0x461b25` 把 arg1/2/3 → `+0xa4..0xac`（切线 1）、arg7/8/9 → `+0xb0..0xb8`（切线 2）、
arg4/5/6 → `+0x98..0xa0`（目标）、mode 硬编码 8。**② 推测**：与 mode 8 的 Hermite 参数一一对应。
**③ 验证**：`InterpFloat3::step` `0x406feb` 的 mode 8 分支算出 `start·(u−1)²(2u+1) + tangent1·(1−u)²u +
tangent2·(u−1)u² + target·u²(3−2u)`——正是 Hermite 基（`h00/h10/h01/h11`）。**④ 结论**（✅ TH16）：
`posBezier(t, x1,y1,z1, x2,y2,z2, x3,y3,z3)`：**终点 = (x2,y2,z2)**（arg4–6）、**初速 = (x1,y1,z1)/t**（arg1–3，切线 1）、
**末速 = (x3,y3,z3)/t**（arg7–9，切线 2）。社区的「(x2,y2,z2) 是终点 / x1/t、x3/t 是速度」**完全正确**，本文一手坐实。
**⑤ 证据**：`0x461b25`–`0x461d04`；`0x406feb`–`0x407121`。

---

## 7. 与社区不一致处的裁决

对账对象：thpages `anm.ts` 与 thtk `v8.anmm`。**签名（参数个数/类型）逐条比对全部一致** ✅，分歧只在下面几处：

| # | 社区 | 一手 | 裁决 |
| --- | --- | --- | --- |
| 1 | `433 posTime2D` = *"2D 版 posTime？"*（`wip:1`） | 极坐标目标 `(cos(x)·y, sin(x)·y, 0)` | **社区没答，本文答**；升 ✅ |
| 2 | `421 anchor` `sig 'ss'`（两个 s16） | 一个 u32（低 16=h、高 16=v） | 字节相同、参数边界不同；按 u32 计数更准确 |
| 3 | `v8.anmm` 未给 418/419/431 名字；thpages `v8-flag-419` *"effectively calls 418 every frame"* | 419 = `flags_hi & 0x2000`（帧尾重算 UV） | 社区对 ✅（补上 flag 位地址） |
| 4 | `v8.anmm` 把 `412 scaleTime`/`435 scale2Time` 都记 `sig 'SSff'` | 一致 ✅；但 `427/428 uVelTime/vVelTime` 签名 `SSf` | 一致 ✅ |
| 5 | ExpHP 命名 `0x464c60 = set_rgb2_time`、`0x464b40 = set_rgb1_time` | `0x464c60` 写 `color_1`（408 用）、`0x464b40` 写 `color_2`（413 用） | **ExpHP 名字对调了**，见 `names-R6.md` |

---

## 8. 社区标 `wip:2` 的两条

- **`422 posAdopt`**（社区：*"copies pos_3 into pos, zeros pos_3"*，`wip:2`）。一手 `0x462424`：
  `MOVQ XMM0,[EBX+0x5e0]` → `pos`（`+0x2c..0x34`），再 `+0x5e0..0x5e8 = 0`。`+0x5e0` 即 `entity_pos`（`02` §3.3）。
  ⇒ **机制 ✅**（`pos = entity_pos; entity_pos = 0`，逐字节坐实）；社区描述的语义正确。真实数据 0 处使用（无 ground truth），
  「pos_3」这个别名与 `entity_pos` 是否同物、以及「为什么要收养宿主实体坐标」的**动机**仍是 🟡。
- **`431 v8-flag-431`**（社区：*"sets the state of an unknown bitflag"*，`wip:2`）。一手 `0x46293c`：
  arg0 低字节 → `flags_hi` bit8（`0x100`）。**机制 ✅**（写哪个位是确定的）；**该位含义 ❓**（`02` §3.6 的 flag 表没有 bit8 的语义，
  全 exe 无其他测试点可交叉）。真实数据 499 次使用（全部在某个 anm 里批量出现，见 [§9](#9-真实数据对账54-个-anm)），
  但**用途不明，维持 ❓**。

---

## 9. 真实数据对账（54 个 `.anm`）

自写解析器跑 `local/th16.v1.00a/dat/*.anm`：54 文件 / 423 entry / 1929 script / 41339 指令
（+1929 条 `-1` 结束标记 = 43268，与 `01-format.md` §10 一致）。**本组 17453 条**（28 条用到 / 11 条未用）：

| 检查项 | 结果 |
| --- | --- |
| 每条 `size` = `8 + 4 × 参数个数`（按 §2 表推） | **17453 / 17453**，0 例外 |
| 参数个数与 §2 签名逐条吻合 | 全部吻合；唯一例外 `421` = 1 参数（§6.1） |
| `mode`（arg1）恒为字面量（`*Time` 家族） | 全部 `param_mask` 未标 arg1，0 例外 |
| `420 posBezier` 的 `param_mask` | 仅 `0x180`/`0x186`（arg1/2/7/8 为变量），恰是 `01-format.md` §5.2 记的 bit8 用例 |

**使用分布**（按出现次数）：`404`×3230 · `409`×2834 · `400`×1970 · `403`×1685 · `407`×1415 · `412`×1243 ·
`408`×1210 · `402`×1066 · `421`×829 · `431`×499 · `401`×423 · `410`×252 · `415`×183 · `406`×112 · `405`×94 ·
`424`×70 · `414`×67 · `437`×43 · `425`×41 · `438`×38 · `413`×32 · `423`×27 · `426`×27 · `436`×23 · `429`×16 ·
`430`×16 · `420`×4 · `433`×4。

**一次都没出现的 11 条**（实现完整、ZUN 自家没用，无 ground truth）：`411` `416` `417` `418` `419` `422`
`427` `428` `432` `434` `435`。其中 `416 scaleGrowth`/`434 scale2`/`435 scale2Time`/`417 alphaTimeLinear` 是
「社区文档里有、TH16 却没用」——mod 作者可放心用，但播放器要对这些靠本文语义而非「跑一遍」。

**mode 分布（ZUN 实际用到的曲线，分母 = 各 `*Time` 指令条数）**：
`407 posTime`（1415）：`0`×435 · `4`×604 · `9`×126 · `1`×70 · `2`×65 · `21`×44 · `5`×30 · `6`×30 · `10`×4 · `3`×7。
`409 alphaTime`（2834）：`0`×1945 · `4`×671 · `1`×163 · `21`×36 · `9`×18 · `3`×1。
`412 scaleTime`（1243）：`4`×604 · `0`×428 · `1`×134 · `9`×59 · `20`×3 · `2`×6 · `6`×4 · `3`×1 · `5`×3 · `27`×1。
`410 rotateTime`（252）：`0`×108 · `9`×71 · `4`×25 · `7`×16 · `21`×22 · `1`×8 · `3`×2。
`408 rgbTime`（1210）：`0`×895 · `9`×239 · `4`×48 · `1`×26 · `21`×2。
`433 posTime2D`（4）：`5`×4。`430 uvScaleTime`（16）：`0`×16。`413/414`：`0`。

⇒ **ZUN 用过的曲线只有 0/1/2/3/4/5/6/7/9/10/20/21/27 这 13 个**；8、11–19、22–26、28–31 是「实现完整但自家没用」。
mode 0（linear）与 4（ease-out 二次）占绝对多数。**mode 7 只在 `410 rotateTime` 出现 16 次**（逐帧加旋转角的用法）。

---

## 10. 写播放器会踩的坑

| 坑 | 说明 |
| --- | --- |
| **mode 8 ≠ bezier** | 标准 `*Time` 的 mode 8 是 smoothstep（切线恒 0）；Bezier 只有 `420`。照「mode 8 = bezier」实现会画错 |
| **mode 7/17 是「累加」不是「缓动」** | 它们每帧把 target/加速度**加进 start**，跨 t 帧累计；结束值 = `t×target`，不是 `target` |
| **22–26 会 < 0、27–31 会 > 1** | 插值因子会越出 `[0,1]`；JS 里 `Math.fround` 之外别再做 clamp |
| **`421 anchor` 按 1 参数数** | `size`、`param_mask` 位序都按 4 字节 arg 算；h/v 是 arg0 的高低 16 位 |
| **`mode`/`h`/`v` 恒字面量** | 反汇编器/汇编器不必为这些位置生成变量引用；`t` 和各分量可以 |
| **`433 posTime2D` 是极坐标** | 别当成「只插值 x,y 的 posTime」；z 会一路到 0，arg2 是角度（弧度） |
| **`400 pos` 可能写 `+0x4e0`** | `flags_lo & 0x400` 时写 `pos_2` 而非 `pos`（`02` §3.3）；播放器复刻这个分流 |
| **颜色是 8 位截断** | `403/404/405/406/409/…` 写字节前 `movzx al`/`mov byte …`——alpha 参数取低 8 位 |
| **`t` 受减速影响** | 插值器内嵌 zTimer 以 `GAME_SPEED` 步进 ⇒ 减速期实际帧数 = `t / GAME_SPEED`（`02` §4） |
| **写 `flags_hi` 的 case 走共享尾巴 `0x4629c2`** | 反汇编扫 case 末尾时别把这条 `XOR` 当成「丢失的运算」 |

---

## 11. 还没搞清楚的

| 问题 | 状态 |
| --- | :---: |
| `431 v8-flag-431`（`flags_hi` bit8）的含义（499 次真实使用，无交叉测试点） | ❓ |
| `411 rotateTime2D` mode 8 的角度 Hermite 精确公式（`0x4475f0`/`0x447650` 未逐条解码） | 🟡 |
| `418` 的高层效果（`0x468c00` 各渲染模式分支的几何） | — 归渲染文档 |
| `422 posAdopt` 的动机（`entity_pos` 何时被预置、为何收养） | 🟡 |
| `423 colorMode` 2/3（横/竖渐变）与 `406/405` `color_2` 的渲染消费 | — 归渲染文档 |
| `437 rotationSystem` 3 位取值的具体旋转次序（XYZ/ZYX…） | 🟡 |
| 本组在 TH18 的差异（本文只做 TH16） | — 阶段 3 |

---

## 关联

- 指令头 / `param_mask` / script 表：[`01-format.md`](01-format.md)
- VM 主循环、帧尾、`zAnmVm` 字段图、终止语义：[`02-vm-core.md`](02-vm-core.md)
- 100 系数学/跳转（求值顺序判据、除零、PRNG 流）：[`03-math-jumps.md`](03-math-jumps.md)
- 变量号表：[`04-variables.md`](04-variables.md)
- 运动积分器同源（`apply_velocities` 的角度归一与 `129` 同款）：[`../../_shared/math-and-prng.md`](../../_shared/math-and-prng.md)
- 子系统总览：[`../OVERVIEW.md`](../OVERVIEW.md)
- 本作入口：[`README.md`](README.md)
- 想要的 Ghidra 重命名（待 T10 统一落库）：[`naming-batch.md`](naming-batch.md)
- 纪律：[`../../../METHOD.md`](../../../METHOD.md)、[`../../../DOCSTYLE.md`](../../../DOCSTYLE.md)
