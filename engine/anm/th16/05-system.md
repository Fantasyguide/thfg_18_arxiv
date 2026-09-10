# ANM v8 System 组 — TH16 一手逐条确认（opcode 0–7，8 条）

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：**不经 Ghidra 渲染**——直读 PE 解出低段分发表 `0x462d8c`（索引，215 字节）+ `0x462cb8`（跳表，53 dword），
> 得到 8 条 opcode → case 地址；再从 `state/f/0045f980.html` 逐条读 `AnmVm::run` `0x45f980` 的反汇编
> （**不用** `functions.jsonl` 反编译文本——Ghidra 会把 `flags_lo` 位测试糊成指针运算）。
> 再用自写解析器跑 `local/th16.v1.00a/dat/` 的 **54 个真实 `.anm`（41339 条指令，其中本组 9391 条）**对账证伪。
> **可信度**：**8 条的签名、控制流与副作用全部 ✅**（每条断言背后是一条读/写该字段的机器指令）；
> **三处**是 🟡（见 §11）：「`flags_lo & 0x4000` 在全 exe 无读取者」、「caseReturn 无 interrupt 先导时的行为」、
> 以及「`6 wait` 参数为负时把时间往**前**拨」——最后这条代码侧证据充分，但真实数据 0 次出现、未经验证。
>
> ⚠️ **2026-09-09 审阅更正**：原写「两处」，漏了 `wait < 0` 那条。根因是 §6 用了一个**段级**的
> 「④结论（✅ TH16）」标题，把它下辖的 `wait < 0` 子断言一并盖成了 ✅——违反 `DOCSTYLE.md` §5
> 「可信度标记挂**断言**、不挂小节」。该子断言已就地单独降 🟡。
> **衔接**：指令头 8 字节与 `param_mask` 见 [`01-format.md`](01-format.md) §5；VM 主循环、时间闸门、`zAnmVm` 字段图、
> interrupt 查找、终止语义的骨架见 [`02-vm-core.md`](02-vm-core.md) §2/§3/§5/§6——本文把**这 8 条的一手细节补全**，
> 不重复骨架论证。变量号解析见 [`04-variables.md`](04-variables.md)。
> **不含版权字节**：只写偏移、地址与语义。

## 0. 一句话结论

这 8 条是 ANM VM 的**「生死 + 让出 + 标签」控制层**，全部零到一参、无算术。五件容易搞错的事，一手结论是：

1. **生死是两轴，不是一维的**：`1 delete` / `2 static` / `3 stop` / `4 stopHide` 在「返回什么」「指令指针动不动」
   「帧尾跑不跑」「能不能被 interrupt 唤醒」四个维度上各不同（[§3](#3-专项-delete--static--stop--stophide-的生死语义)）。
   一句话版：`delete` = 销毁、`static` = 冻死（可被外部销毁）、`stop` = 暂停（可被 interrupt 唤醒）、
   `stopHide` = `stop` + 隐藏。
2. **`0 nop` 与 `5 case` 在执行路径上都是默认分支（只 `instr_offset += size`）**；`5` 的语义完全来自
   interrupt 扫描，运行时它只是一条 12 字节的 no-op（[§5](#5-专项-case-标签与-interrupt-扫描)）。
3. **`6 wait n` 是「把脚本时间往回拨 n」**，`previous` 先被设为旧 `current`、然后 `current -= n`；
   `n` 走 `get_int_var`，**可以是变量**（真实数据里 125/368 条用变量，见 [§6](#6-专项-wait-把时间往回拨)）。
4. **`7 caseReturn` 不是「结构体拷贝恢复」**：`Timer::copy` 只搬 `current`，小数位/`previous`/速度源全丢；
   且它跳回 **DISPATCH**（`0x45fba0`），**同帧继续跑**，不是跳去帧尾（[§7](#7-专项-casereturn-恢复快照)）。
5. **`4 stop2`（社区 `wip:2`）一手坐实 = 清可见位 + 落进 `3 stop`**，语义与 thpages 的描述完全一致，
   可升 ✅（过 `METHOD.md` 四闸门，见 [§8.1](#81-4-stop2社区-wip2--一手坐实为「清可见位--落进-stop」)）。

---

## 1. 怎么核的：opcode → case 地址是自己解跳表得到的

**① 发现**：`th-re-data` 的 `labels.json` 里 `anm` 键对本组给了 6 条 label，其中 3 条是裸数字
（`0x4629dc 4`、`0x45fc65 6`、`0x45fcc1 7`）、2 条名不副实（`0x4629e3 3__run`、`0x462bf0 1__end`）、
1 条正常（`0x462c08 2__static`）。`0`（nop）与 `5`（case）没有 label（它们落在默认分支 `0x4629c8` 上）。

**② 推测**：这些 label 的「数字前缀」就是它服务的 opcode。**这是待验假设**——label 是人打的，可能错位。

**③ 验证**：不看 label，直读 PE 把低段分发表的字节解出来。派发序言在 `0x45fbce`：

```
0045fbce  movsx eax, word ptr [edi]        ; ins.opcode（s16）
0045fbd1  cmp   eax, 0x12c                 ; 300
0045fbe2  inc   eax                        ; idx = opcode + 1
0045fbe3  cmp   eax, 0xd6                  ; 214（无符号）
0045fbe8  ja    0x4629c8                   ; 越界 → 默认分支（= nop）
0045fbee  movzx eax, byte ptr [eax + 0x462d8c]   ; 索引表（215 项）
0045fbf5  jmp   dword ptr [eax*4 + 0x462cb8]     ; 跳表（53 项）
```

索引表 `0x462d8c` 的头 9 字节直读 PE 为（下标 = opcode + 1）：

```
0x462d8c: 00 34 00 01 02 03 34 04 05 ...
          -1  0  1  2  3  4  5  6  7     ← 下标对应的 opcode
```

跳表 `0x462cb8` 的头 6 项 + 末项直读为：

| 槽 | 地址 | 服务 opcode |
| :---: | --- | --- |
| 0 | `0x462bf0` | −1（脚本末）与 1（delete）**共用** |
| 1 | `0x462c08` | 2 static |
| 2 | `0x4629e3` | 3 stop |
| 3 | `0x4629dc` | 4 stopHide |
| 4 | `0x45fc65` | 6 wait |
| 5 | `0x45fcc1` | 7 caseReturn |
| 52 | `0x4629c8` | **默认分支**（= nop）：0 nop、5 case、以及一切无 handler 的 opcode |

**④ 结论**：本文全部 case 地址是一手的，不依赖社区 label ✅（TH16）。两处与社区 label 有出入：
`0x4629e3` 的社区名 `3__run` 是 stop 而不是「run」、`0x462bf0` 的社区名 `1__end` 是 delete 的「销毁」路径
（名不副实），命名请求见 [`naming-batch.md`](naming-batch.md)。

**⑤ 证据**：`0x45fbce`–`0x45fbf5`；索引表 `0x462d8c`（`u8[215]`）、跳表 `0x462cb8`（`void*[53]`）。

---

## 2. ★ 8 条指令表

**记号**：`S` = 整型参数（`param_mask` 置位则查变量，否则字面量）· `—` = 无参。
`arg i` = `*(u32*)(ins + 8 + 4*i)`，`param_mask` 的 `bit i` 管 `arg i`（[`01-format.md`](01-format.md) §5.2）。
除 `6 wait` 外，本组其余 7 条都**不查 `param_mask`**（没有可作变量的参数）。

| opcode | 助记（thpages / thanm） | 签名 | 语义 | case 地址 | 可信度 |
| :---: | --- | --- | --- | --- | :---: |
| 0 | `nop` | — | 只 `instr_offset += size` | `0x4629c8`（默认分支） | ✅ |
| 1 | `delete` | — | 清可见位、`instr_offset = -1`、**返回 1**（请销毁） | `0x462bf0` | ✅ |
| 2 | `static` | — | `instr_offset = -1`、返回 0（冻死但存活） | `0x462c08` | ✅ |
| 3 | `stop` | — | 置 `0x4000`、`Timer::decrement`、跑帧尾（时间冻结，可被 interrupt 唤醒） | `0x4629e3` | ✅ |
| 4 | `stopHide` / `stop2` | — | 清可见位，落进 `3 stop` | `0x4629dc` | ✅ |
| 5 | `case` | `S n`（**恒字面量**） | 运行时 no-op；interrupt 扫描时当标签 | `0x4629c8`（默认分支） | ✅ |
| 6 | `wait` | `S t`（**可变量**） | `previous = current`；`current_f -= (float)t`；`current = (int)current_f` | `0x45fc65` | ✅ |
| 7 | `caseReturn` | — | `Timer::copy(script_time, saved_time)`；`instr_offset = saved_instr_offset` | `0x45fcc1` | ✅ |

> `-1`（脚本结束标记）**不算**本组的 8 条，但它的 case 体与 `1 delete` 逐字节相同（索引表下标 0 与 2 都指向
> 槽 0 = `0x462bf0`），语义差异只在「它是解析器层面的结构性终止符，`size` 恒 0、占 8 字节」
> （[`01-format.md`](01-format.md) §5.1、[`02-vm-core.md`](02-vm-core.md) §6.1）。本节表里不单列它。

### 2.1 五个共用落点

本组 case 体互相共享尾巴，判据 = 「case 体末尾 `jmp`/直落到一个比自己小、且落在别的 case 体内部的地址」：

| 落点 | 地址 | 干什么 | 谁用（穷举） |
| --- | --- | --- | --- |
| 默认分支（= nop） | `0x4629c8` | `instr_offset += size`，回 `0x45fb9a` | `0 nop`、`5 case`、一切无 handler 的 opcode |
| 「stop 主体」 | `0x4629e3` | 查 pending_interrupt → 置 `0x4000` → `Timer::decrement` → 帧尾 | `3 stop` 入口；`4 stopHide` 清可见位后直落这里 |
| 「返回 0」 | `0x462c90` | `XOR EAX,EAX` 后落进 `0x462c92`（恢复 `GAME_SPEED` + RET） | `2 static`（`0x462c08` → `JMP 0x462c90`） |
| 「返回（EAX 已就绪）」 | `0x462c92` | 恢复 `GAME_SPEED`、`__security_check_cookie`、RET | `1 delete`（`0x462c03`）、`static` 经 `0x462c90` |
| 帧尾 | `0x462a06` | 速度积分/滚动/UV/插值器/几何/on_wait，末了 `Timer::increment` | `3/4 stop`（`0x462a01` 之后直落）、时间闸门被挡（`0x45fbc8`） |

**两条返回路径的差别是「EAX 谁设」** ✅：`delete` 在 `0x462bf7` 先 `MOV EAX,1` 再 `JMP 0x462c92`；
`static` 跳 `0x462c90` 做 `XOR EAX,EAX`（=0）。`0x462c92` 之后是共享的还原 + RET（`0x462c99` 读 `saved_speed`、
`0x462ca4` 写回 `0x4a5788` = `GAME_SPEED`，`0x462cac` 调 `__security_check_cookie`）。

---

## 3. 专项① `delete` / `static` / `stop` / `stopHide` 的生死语义

**① 发现**：四条 case 体直读反汇编（见 §2 表地址），各自做的事如下：

```
0x462bf0  delete:    AND [EBX+0x530], 0xfffffffe   ; flags_lo &= ~1（隐藏）
                     MOV EAX, 1                    ; 返回「请销毁」
                     MOV [EBX+0x28], 0xffffffff    ; instr_offset = -1
                     JMP 0x462c92
0x462c08  static:    MOV [EBX+0x28], 0xffffffff    ; instr_offset = -1
                     JMP 0x462c90                  ; EAX=0 返回（存活）
0x4629dc  stopHide:  AND [EBX+0x530], 0xfffffffe   ; flags_lo &= ~1（隐藏）
0x4629e3  stop:      CMP [EBX+0x490], 0            ; pending_interrupt ?
                     JNZ 0x45fac6                  ;   有 → DO_INTERRUPT
                     OR  [EBX+0x530], 0x4000       ; flags_lo |= 0x4000（已 stop）
                     LEA ECX, [EBX+0x540]
                     CALL 0x40d490                 ; Timer::decrement
                     ... 直落帧尾 0x462a06 ...
```

**② 推测**：四条的区别可以用四个布尔量正交地描述：清可见位？写 `instr_offset=-1`？返回 1？跑帧尾？

**③ 验证**：把四条的每个副作用对到机器指令（上表），并与 `run` 的早退闸门顺序（`0x45fa55`
`if (instr_offset < 0) return 0` 在 interrupt 服务点 `0x45fa7b` **之前**）交叉核对，得到：

| 维度 | `1 delete` | `2 static` | `3 stop` | `4 stopHide` |
| --- | :---: | :---: | :---: | :---: |
| 清可见位 `flags_lo & ~1` | ✅ `0x462bf0` | — | — | ✅ `0x4629dc` |
| `instr_offset = -1` | ✅ `0x462bfc` | ✅ `0x462c08` | —（原地不动） | —（原地不动） |
| 返回值 | 1（销毁） | 0（存活） | 0（存活） | 0（存活） |
| 本帧跑帧尾 | 否（直接 RET） | 否（直接 RET） | **是** | **是** |
| 下一帧还能跑脚本 | 否（被销毁） | 否（`0x45fa55` 早退） | 是（重执行 stop） | 是 |
| 插值器/速度积分继续 | 否（销毁） | 否（早退） | **是** | **是** |
| 可被 interrupt 唤醒 | 否 | **否**（早退在 interrupt 之前） | **是** | **是** |

**④ 结论**（✅ TH16）：

1. **`delete` 与 `static` 都写 `instr_offset = -1`**，区别只在返回值：`delete` 返回 1 让管理器销毁整棵子树
   （`0x46e340` 非 0 → `0x46e660`，见 [`02-vm-core.md`](02-vm-core.md) §6.1）；`static` 返回 0，VM 留在链表里
   继续被渲染，但从下一帧起 `0x45fa55` 早退，脚本、计时器、插值器、速度积分、帧尾几何**全部不再跑**。
2. **`static` 的「interrupt 被禁用」是早退闸门造成的，不是显式判断** ✅：`0x45fa55`（`instr_offset < 0`）在
   `0x45fa7b`（pending_interrupt 服务点）**之前**，所以 static 的 VM 永远到不了 interrupt 服务。
   这正是 thpages 说的 *"interrupts are disabled"* 的一手机制。
3. **`stop` 与 `stopHide` 不写 `instr_offset`**——`instr_offset` 停在 stop 这条指令上，下一帧重执行它，
   形成自锁。区别只在 `stopHide` 多清一个可见位。
4. **`stop` 的净时间变化为 0**：`Timer::decrement`（`0x40d490`，`cur_f -= 速度`）之后帧尾末尾的
   `Timer::increment`（`0x462c11`，`cur_f += 速度`）又加回来。但**帧尾照跑**，所以插值器、速度积分
   每帧照常推进——这是「stop 暂停的是**脚本**，不是**动画**」的精确含义。

**⑤ 证据**：四条 case 地址见 §2 表；早退闸门 `0x45fa55`、interrupt 服务点 `0x45fa7b`、帧尾 `0x462a06`、
`Timer::increment` `0x462c11`、`Timer::decrement` `0x40d490`。

---

## 4. 专项② `stop` 的「自锁 + 可唤醒」与 `static` 的「冻死」

> **2026-09-09 审阅更正（重复论证）**：本节的发现→验证→结论链与 [`02-vm-core.md`](02-vm-core.md)
> §5（DO_INTERRUPT 伪代码）+ §6.1（stop 的 interrupt 免疫）**实质重复**——同样的地址
> （`th16:0x45fa7b` / `th16:0x45fb21` / `th16:0x45fb90` / `th16:0x46f0e3` / `th16:0x45fa55`）、
> 同样的论证。**02 是权威，本节只保留 opcode 视角的关联**（`4 stopHide` 与 thpages `stop2` 的对应）。
> 两份文档将来若语义漂移，以 02 为准。

**① 发现**：`stop` 的 case 体在置 `0x4000` **之前**先查 `pending_interrupt`（`0x4629e3`），命中就跳
`0x45fac6`（DO_INTERRUPT）而不是置位。interrupt 服务尾在 `0x45fb21` 清 `0x4000`、`0x45fb90` 重设可见位 `1`。

**② 推测**：`stop` 是一台「可被 interrupt 打破的自锁」，`static` 是「无法被 interrupt 打破的冻死」。

**③ 验证**（两条互相独立的证据）：

- **唤醒路径**：VM 停在 stop 上 → 外部 `interrupt_tree(n)` 写 `pending_interrupt = n`（`0x46f0e3`）→
  下一帧 `run` 顶部 `0x45fa7b` 命中 → DO_INTERRUPT 找到 `ins 5 n` 标签 → `0x45fb21` 清 `0x4000`、
  `0x45fb90` 置可见位、`0x45fb97` 把 `instr_offset` 拨到标签之后 → 脚本从标签处继续。
  即使外部 interrupt 到得晚（`run` 已经进了 stop 的 case 体），stop 自己的 `0x4629e3` 仍会再查一次，
  同样能跳去 DO_INTERRUPT ✅。
- **冻死路径**：`static` 写 `instr_offset = -1` 后，下一帧 `0x45fa55` 在 `0x45fa7b` **之前**返回，
  interrupt 服务点永远够不到。所以 static 的 VM 对 interrupt 免疫，只能被**外部销毁**（host 调
  `destroy` 路径）✅。

**④ 结论**（✅ TH16）：

- `stop`/`stopHide` 的「暂停」是**可逆**的：interrupt 命中会清 `0x4000`、重设可见位，并跳到标签。
  因此 `stopHide` 被 interrupt 唤醒后**自动重新可见**——这正是 thpages 对 `stop2` 说的
  *"Successful interrupts will automatically re-enable the visibility flag"*，一手坐实（`0x45fb90`）。
- `static` 的「冻死」**不可逆**（除非外部销毁后重建）✅。
- `stop` 与 `stopHide` 的区别**只有可见位**：被 interrupt 唤醒后二者行为完全一致（都重设可见位并继续）。

**⑤ 证据**：`0x4629e3`/`0x4629ea`/`0x4629f0`、`0x45fac6`、`0x45fb21`/`0x45fb90`/`0x45fb97`、
`0x46f0e3`（`interrupt_tree` 写 pending）、`0x45fa55`/`0x45fa7b`（早退 vs interrupt 服务点的顺序）。

---

## 5. 专项③ `case` 标签与 interrupt 扫描

**① 发现**：索引表下标 6（= opcode 5）指向槽 52 = 默认分支 `0x4629c8`，所以 `case` 运行时是 no-op。
它的语义全在 interrupt 扫描里（`0x45fac6`–`0x45fb1f`）：

```
0045faf1  MOVZX EAX, word ptr [EDI]          ; ins.opcode
0045faf4  CMP AX, 0x5                         ; opcode == 5 ?
0045faf8  JNZ 0x45faff
0045fafa  CMP ECX, [EDI + 0x8]                ; args[0] == want ?（ECX = pending_interrupt）
0045fafd  JZ  0x45fb21                        ;   命中 → 跳过去
0045faff  CMP AX, -0x1                        ; opcode == -1（脚本末）?
0045fb03  JZ  0x45fb21                        ;   到末尾 → 退出扫描
0045fb05  CMP AX, 0x5
0045fb0b  CMP [EDI + 0x8], -0x1               ; opcode==5 且 args[0]==-1 → default 标签
0045fb17  MOVZX EAX, word ptr [EDI + 0x2]     ; ins.size
0045fb1b  ADD EDX, EAX;  ADD EDI, EAX          ; 步进
0045fb1f  JMP 0x45faf1
```

**② 推测**：`case` 的 `n` 参数恒为字面量（扫描直接 `CMP [EDI+0x8]`，从不查 `param_mask`）。

**③ 验证**（两路）：

1. **代码侧**：扫描全程没有一条 `TEST [EDI+0x6]`（`param_mask`），`args[0]` 是 `CMP` 直接读 4 字节 ✅。
2. **数据侧**：54 个文件里 `case` 出现 **4009** 次，`param_mask` **4009/4009 全为 0**；`args[0]` 的取值
   落在 `{1..39}` 与 `4294967295`（= −1 = default 标签，27 次）。反例集合为空 ✅。

**④ 结论**（✅ TH16）：

- `case n` 是**无副作用标签**：运行时只 `instr_offset += size`（默认分支）；执行到它和没写它一样。
- `n` **恒为字面量 s32**，不可为变量（interrupt 扫描不做变量解析）。
- **`n == -1` 是 default 标签**：只有精确匹配失败、且扫描走到脚本末时，才回退到 `ins 5 -1`
  （`0x45fb0b` 记 fallback，`0x45fb47` 回填）。真实数据里 default 标签出现 27 次。
- **同号标签只有第一个生效**（扫描命中即停，`0x45fafd` 的 `JZ` 直接跳走），且扫描从脚本开头开始
  （[`02-vm-core.md`](02-vm-core.md) §5）。

**⑤ 证据**：索引表下标 6；`0x45faf1`–`0x45fb1f`（扫描）、`0x45fb21`/`0x45fb47`（命中/default 回填）；
真实数据统计见 [§9](#9-真实数据对账54-个-anm)。

---

## 6. 专项④ `wait` 把时间往回拨

**① 发现**：`0x45fc65`–`0x45fcbc` 直读反汇编：

```
0045fc65  TEST byte ptr [EDI + 0x6], 0x1      ; param_mask bit0 → arg0 是变量?
0045fc6b  PUSH dword ptr [EDI + 0x8]           ;   arg0（变量号）
0045fc70  CALL 0x45f610                        ;   get_int_var → EAX
0045fc79  MOV ECX, dword ptr [EDI + 0x8]       ; 否则字面量 → ECX
0045fc8a  MOVD XMM0, ECX
0045fc8e  CVTDQ2PS XMM0, XMM0                   ; (float)(s32)t
0045fc84  MOV EAX, [EBX + 0x544]                ; old current
0045fc91  MOV [EBX + 0x540], EAX                ; previous = old current
0045fc97  SUBSS XMM1, XMM0                       ; current_f -= (float)t
0045fc9b  CVTTSS2SI EAX, XMM1                    ; 截断向零
0045fc9f  MOVSS [EBX + 0x548], XMM1              ; current_f = 新值
0045fcaf  MOV [EBX + 0x544], EAX                 ; current = (int)current_f
0045fcb9  ADD [EBX + 0x28], <size>               ; instr_offset += size
0045fcbc  JMP 0x45fb9a
```

**② 推测**：`wait t` = 「把脚本时间拨回 t」，让后续 `time` 更大的指令被时间闸门挡住 t 帧。

**③ 验证**：`MOVD + CVTDQ2PS` 说明 `t` 是**有符号 int32**；`CVTTSS2SI`（截零）说明 `current` 是截断不是 floor。
真实数据穷举：`wait` 出现 **368** 次，其中 **125 次** `param_mask & 1`（变量），变量号全部落在
`{10000×111, 10002×4, 10003×10}`（= I0/I2/I3，整型寄存器）；243 次字面量，字面量 ∈ `[0, 120]`，
**0 次负值**（见 [§9](#9-真实数据对账54-个-anm)）。

**④ 结论**（✅ TH16）：

- `wait` 是本组**唯一可带变量的指令**，且走 `get_int_var`（整型解析）。它能读 `I0`/`I2`/`I3`
  （真实数据正是这 3 个）。写浮点变量属类别错误：`F0–F3`（10004–10007）会被 `cvttss2si` 截成整数；
  `pos/rot/camera/random` 等 19 个「只有浮点语义」的号会被 `get_int_var` 原样返回变量号本身
  （得到 10013 这类大整数 = 等 10013 帧），见 [`04-variables.md`](04-variables.md) §4。
- `previous = 旧 current` 在减法**之前**写入，于是 `previous > current`（`wait` 之后计时器「倒退」）。
- `wait 0` 是 no-op（真实数据里 7 次），`wait < 0` 会把时间往**前**拨（等价跳过若干帧），ZUN 数据里 0 次。
- 与「时间标签」的等价性：`wait t` ≈ 把下一条指令的 `time` 增大 t（thpages 对 `wait` 描述里的 Option 2），
  但实现是「拨时间」不是「设目标」，与 `6 wait` 在 [`02-vm-core.md`](02-vm-core.md) §4 的「往回拨」一致。

**⑤ 证据**：`0x45fc65`–`0x45fcbc`；`get_int_var` `0x45f610`；统计见 §9。

---

## 7. 专项⑤ `caseReturn` 恢复快照

**① 发现**：`0x45fcc1`–`0x45fce3` 直读反汇编：

```
0045fcc1  MOVUPS XMM0, xmmword ptr [EBX]       ; saved_time[0..15]（previous/current/current_f/速度源）
0045fcc4  MOV EAX, dword ptr [EBX + 0x10]      ; saved_time.control
0045fcca  MOV ECX, ESP                          ; 栈上 20 字节临时
0045fccc  MOVUPS xmmword ptr [ECX], XMM0
0045fccf  MOV dword ptr [ECX + 0x10], EAX
0045fcd2  LEA ECX, [EBX + 0x540]                ; &script_time
0045fcd8  CALL 0x464d80                         ; Timer::copy(&script_time, byval saved_time)
0045fcdd  MOV EAX, [EBX + 0x14]                 ; saved_instr_offset
0045fce0  MOV [EBX + 0x28], EAX                 ; instr_offset = saved_instr_offset
0045fce3  JMP 0x45fba0                          ; ★ 回 DISPATCH，不是帧尾
```

**② 推测**：`caseReturn` 把脚本恢复到「被 interrupt 打断那一刻」的计时器与指令偏移。

**③ 验证**：interrupt 服务路径在 `0x45fb6b` 用 `Timer::copy(&saved_time, byval script_time)` 存快照、
`0x45fb7d` 存 `saved_instr_offset = instr_offset`；`caseReturn` 是它的对称恢复。但 `Timer::copy`（`0x464d80`）
**不是结构体拷贝**——它与 `Timer::set_value` 逐字节相同，只取源的 `current`，算出
`previous = current - 1`、`current_f = (float)current`（[`02-vm-core.md`](02-vm-core.md) §3.2）。

**④ 结论**（✅ TH16）：

1. **恢复只保真到整数时间**：`current_f` 的小数、`previous` 的旧值、速度源下标、`control` 位全部丢弃。
   `GAME_SPEED == 1.0` 时看不出来；减速期（浮点路径）可观测。→ 一次 interrupt + 一次 caseReturn 会
   **截掉两次小数位**。
2. **恢复后同帧继续**：`0x45fce3` 跳回 `0x45fba0`（DISPATCH）而不是帧尾，所以 caseReturn 之后脚本
   **当帧继续**从 `saved_instr_offset` 执行，还要过一次时间闸门。
3. **没有 interrupt 先导时是「回卷」**：`saved_time`/`saved_instr_offset` 的初始值是 `wipe` 的 memset 0
   （`+0x00`/`+0x14` 都不在 `wipe` 保留清单里），所以裸执行 `caseReturn` 会 `script_time = 0`、
   `instr_offset = 0`——回到脚本开头时间 0。ZUN 数据里 caseReturn 恒跟在 interrupt 后，此为理论边界 🟡。
4. `caseReturn` **不是**结束 `case` 块的唯一手段——ZUN 常用 `stop` 结尾（thpages 原话，一手数据里
   `stop` ×2398 ≫ `caseReturn` ×754 佐证）。

**⑤ 证据**：`0x45fcc1`–`0x45fce3`；interrupt 存快照 `0x45fb6b`/`0x45fb7d`；`Timer::copy` `0x464d80`；
`wipe` `0x4090f0`（`+0x00`/`+0x14` 被 memset）。

---

## 8. 与社区不一致处的裁决

对账对象：thpages `anm.ts` 的 `nop/delete/static/stop/stop2/case/wait/caseReturn` 描述与
`local/vendor/thpages/static/mapfile/v8.anmm`（`0 nop / 1 delete / 2 static / 3 stop / 4 stopHide /
5 interruptLabel / 6 wait / 7 caseReturn`）。

### 8.1 `4 stop2`（社区 `wip:2`）→ 一手坐实为「清可见位 + 落进 stop」

thpages 对 `4` 标 `wip: 2`，描述是 *"like stop except it also hides the graphic by clearing the
visibility flag… Successful interrupts will automatically re-enable the visibility flag"*。
按 `METHOD.md` 的额外闸门逐条过：

- **一手到底**：`0x4629dc` 第一条就是 `AND [EBX+0x530], 0xfffffffe`（清 bit0 = 可见位），随后直落
  `0x4629e3`（stop 主体）。可见位 bit0 = 可见/可绘制是 [`02-vm-core.md`](02-vm-core.md) §3.6 一手坐实的
  （`0x45fb90` 的 `OR …,0x1` 就是 interrupt 重设可见）。✅
- **对抗证伪**：会不会是「别的 flag 位」？`0xfffffffe` 逐位 = 只清 bit0，无歧义；「stop 主体」是
  `0x4629dc` 的下一字节地址（直落，非跳转），反汇编里明摆着。✅
- **量纲常识**：「隐藏 + 暂停、等 interrupt 唤醒后重新可见」是精灵动画的标准用法（如「受击后隐藏、
  结束后淡回」），与游戏表现自洽。✅
- **交叉对名**：`v8.anmm` 叫 `stopHide`，thpages 叫 `stop2`（`wip:2`）。二者描述同一行为；`stopHide`
  更能自解释。✅

**结论**（✅ TH16）：`4 = stopHide = 清可见位 + 落进 stop`，与 thpages 的 `wip:2` 描述**完全一致**，升 ✅。
**命名裁决**：Ghidra label 建议用 `4__stopHide`（对齐 `v8.anmm`，比 `stop2` 自解释），thpages 的 `stop2`
  作为别名保留在文档表里。命名请求见 [`naming-batch.md`](naming-batch.md)（由 T10 统一落库）。

### 8.2 「v8 无 opcode 15」——一手确认

> **2026-09-09 审阅更正**：本小节与 [`02-vm-core.md`](02-vm-core.md) §6.2 近乎逐句重复，
> 且 opcode 15 **不在本文 0–7 的编号范围内**，属越界的重复劳动。保留是因为读者常从
> TH06(v0) 的 `0 delete` / `15 static` 带着错误前提进来（本项目的计划初稿就栽过这一次），
> 但结论以 02 §6.2 为准，本节不再独立主张。

本任务 brief 与 [`02-vm-core.md`](02-vm-core.md) §6.2 都说 `0` 是 nop、v8 无 opcode 15。一手复核：

- 索引表下标 16（= opcode 15）直读 = `0x34` = 槽 52 = **默认分支**（无 handler）✅。
- 真实数据 41339 条指令里 opcode 15 出现 **0 次** ✅。
- thpages `delete` 描述里的 *"TH06… stop once they encounter ins_0 or ins_15"* 是 **TH06/v0** 语境
  （旧格式里 `0` 是 delete、`15` 是另一个终止符），与 v8 无关——别把 v0 的编号搬进 v8（这正是 R2 纠正过的错）。

**结论**（✅ TH16）：v8 的 `0 = nop`、opcode 15 无 handler（默认分支 = 只推进指针）。

### 8.3 命名与签名两处小出入

| 出处 | 社区写法 | 一手 | 影响 |
| --- | --- | --- | --- |
| `v8.anmm` | `5 interruptLabel` | 运行时确实 no-op（默认分支） | 名更贴切（点明它服务于 interrupt），无分歧 |
| thpages `case` sig | `S`（暗示可变量） | 扫描不查 `param_mask`，`n` 恒字面量 | 4009/4009 真实数据 mask=0；「S」在汇编器层面仍是 4 字节字面量，不误导 |
| thpages `wait` sig | `S` | `get_int_var` 整型解析，**可变量** | 一手印证，125/368 用变量 |

---

## 9. 真实数据对账（54 个 `.anm`）

自写解析器（[`runtime/anm/src/format/anm-v8.js`](../../../runtime/anm/src/format/anm-v8.js) 的 `parseAnm`）
跑 `local/th16.v1.00a/dat/*.anm`：54 文件 / **41339** 条指令（不含每 script 一条被解析器排除的
`opcode=-1` 结束标记；加上 1929 条 = 43268，与 [`01-format.md`](01-format.md) §10 一致），其中本组 **9391** 条：

| 检查项 | 结果 |
| --- | --- |
| 本组各 opcode 出现次数 | `5 case`×4009 · `3 stop`×2398 · `1 delete`×1558 · `7 caseReturn`×754 · `6 wait`×368 · `0 nop`×221 · `4 stopHide`×56 · `2 static`×27 |
| 每条 `size` 是否 = `8 + 4 × 参数个数` | **9391 / 9391 吻合**（0/1/2/3/4/7 全 8 字节；5/6 全 12 字节） |
| `0/1/2/3/4/7` 的 `param_mask` | **5014 / 5014 全 0**（= 221+1558+27+2398+56+754；7 条无参指令，handler 也不查 mask，自洽） |
| `5 case` 的 `param_mask` | **4009 / 4009 全 0** ⇒ 标签号恒字面量 |
| `5 case` 的 `args[0]` 取值 | 值域 `{1..39}` ∪ `{−1}`；**分布极不均匀**（下方补注）；`0` 一次未出现；反例集合空 |
| `6 wait` 的 `param_mask` | 243 条 = 0（字面量）、**125 条 = 1（变量）** |
| `6 wait` 字面量 `args[0]`（s32） | ∈ `[0, 120]`；`wait 0`×7；**0 次负值** |
| `6 wait` 变量 `args[0]` | `10000`×111 · `10002`×4 · `10003`×10（= I0/I2/I3，整型寄存器） |
| opcode 15 / 132–199 / 214–299 出现次数 | **0 / 0 / 0**（穷举 41339 条，无一条落在这些洞上） |
| 各 opcode 的 `time < 0` 占比（模板预跑段） | `0`×0/221 · `1`×0/1558 · `2`×8/27 · `3`×0/2398 · `4`×0/56 · `5`×13/4009 · `6`×14/368 · `7`×0/754 |

> **`5 case` 标签号的完整分布**（2026-09-10 审阅补：原文写「`{1..39}` 各若干」，把一个跨度两个数量级的
> 分布抹平了，复核者无法据此证伪；分母 4009）：
>
> `1`×870 · `3`×469 · `2`×430 · `8`×258 · `6`×237 · `7`×187 · `9`×132 · `27`×100 · `28`×98 ·
> `29`×86 · `30`×86 · `5`×85 · `31`×85 · `10`×74 · `17`×64 · `18`×63 · `19`×58 · `14`×57 ·
> `20`×57 · `21`×53 · `15`×49 · `16`×49 · `11`×48 · `22`×44 · `25`×42 · `23`×37 · `13`×34 ·
> `4`×33 · `24`×32 · `12`×30 · **`−1`×27** · `26`×11 · `32`×6 · `33`×5 · `34`×3 ·
> `35`/`36`/`37`/`38`/`39` 各 ×2
>
> 合计 4009 ✅。**`0` 一次都没出现**——这与 [`02-vm-core.md`](02-vm-core.md) §5 的「interrupt 号 0 无法投递」
> 互为印证（没有 `case 0`，因为没有任何 interrupt 能命中它）。

**本组在 TH16 自家资源里全 8 条都被用到**（对比 Math 组 46 条里 22 条一次没用），没有「实现了但 ZUN 没用过」
的条目——**每条都有 ground truth** ✅。

---

## 10. 写播放器会踩的坑

| 坑 | 说明 |
| --- | --- |
| **`static` 与 `delete` 都写 `instr_offset = -1`，但返回值不同** | `delete` 返回 1（销毁子树）、`static` 返回 0（留着渲染）。播放器若把「`instr_offset = -1`」当成「销毁」，会把 static 的 VM 错误销毁 |
| **`stop` 之后插值器/速度积分照跑** | `stop` 只冻结脚本时间，不冻结动画。播放器别把 stop 实现成「整个 VM 不 tick」——那会把 `posTime` 家族也冻住 |
| **`static` 之后插值器/速度积分真停了** | 与上一条相反：`0x45fa55` 早退发生在帧尾**之前**。二者一字之差，行为相反 |
| **`stop` 的净时间 = 0 依赖「decrement 后帧尾又 increment」** | 播放器若只实现「时间不前进」而跳过 decrement/increment，会丢 `previous` 的中间态（好在 `previous` 的不变量「prev = cur − 1 步」最终保持，见 [§4](#4-专项stop-的「自锁--可唤醒」与-static-的「冻死」)） |
| **`wait` 的 `previous` 写在减法之前** | 于是 `wait` 后 `previous > current`。任何依赖 `previous <= current` 的逻辑（插值器跨刻度判断）会看到「时间倒退」 |
| **`wait t` 用 `CVTDQ2PS`（有符号 int→float）** | `t` 是 s32；`wait -5` 会把时间往**前**拨。JS 里要 `Math.trunc` 还原 `current`（`CVTTSS2SI` 截零），别用 `Math.floor` |
| **`caseReturn` 恢复只保真到整数时间** | `Timer::copy` 只搬 `current`，小数位丢两次（存 + 取）。减速期（`GAME_SPEED ≠ 1`）会发散。照 [`02-vm-core.md`](02-vm-core.md) §3.2 实现 `copy`，别写成 `dst = src` |
| **`caseReturn` 回 DISPATCH，不是帧尾** | 恢复后**同帧**继续执行，还要过一次时间闸门。写成「恢复后本帧结束」会差一帧 |
| **`case n` 的 `n` 是字面量，`wait t` 的 `t` 可以是变量** | 汇编器/反汇编器不能把两者统一处理。`case` 的 `n` 永不作变量解析 |
| **interrupt 扫描不查 `param_mask`** | 反汇编 `case` 时不用为 `args[0]` 生成 `$`/`%` 前缀 |
| **`stopHide` 被 interrupt 唤醒后自动重设可见位** | `0x45fb90` 无条件 `OR …,0x1`。播放器要记得：唤醒 = 重新可见，不是「恢复到 stop 之前的可见态」 |
| **`0 nop` 与 `5 case` 走默认分支** | 二者行为相同（`instr_offset += size`）。解析器/播放器遇到 opcode 0 或 5，除了步进什么都不做 |

---

## 11. 还没搞清楚的

| 问题 | 状态 |
| --- | :---: |
| `flags_lo & 0x4000`（stop 置位、interrupt 清位）在**整个 exe** 里是否有读取者——grep `[0x530]` 与 `0x4000` 同现的行只有 stop 的写 `0x4629f0` 与一处无关位（`0x466820` 测 bit 18/19）；但「无读取者」靠 grep、未做逐函数穷举 | 🟡 |
| 裸执行 `caseReturn`（无 interrupt 先导）把 `script_time = 0`、`instr_offset = 0` 的「回卷」后果——推理明确（`wipe` memset），真实数据 0 处出现，未构造样本验证 | 🟡 |
| `wait < 0` 的「往前拨时间」行为——代码侧明确（`CVTDQ2PS` + `SUBSS`），真实数据 0 处出现 | 🟡 |
| `static` 之后 `run` 顶部那段（`GAME_SPEED` 存/减速/复原、`on_tick` 回调）仍每帧照跑——这是 [`02-vm-core.md`](02-vm-core.md) §6.1 已记的，本组不复核 | — |
| 本组在 TH18 的差异（本文只做 TH16，不外推） | — 阶段 3 |

---

## 关联

- 指令头 / `param_mask` / script 表：[`01-format.md`](01-format.md)
- VM 主循环、时间闸门、`zAnmVm` 字段图、interrupt 查找、终止语义骨架：[`02-vm-core.md`](02-vm-core.md)
- 变量号解析（`wait` 走的 `get_int_var`）：[`04-variables.md`](04-variables.md)
- 本组后续的算术/跳转层：[`03-math-jumps.md`](03-math-jumps.md)
- 子系统总览：[`../OVERVIEW.md`](../OVERVIEW.md)
- 本作入口：[`README.md`](README.md)
- 想要的 Ghidra 重命名（待 T10 统一落库）：[`naming-batch.md`](naming-batch.md)
- 纪律：[`../../../METHOD.md`](../../../METHOD.md)、[`../../../DOCSTYLE.md`](../../../DOCSTYLE.md)
