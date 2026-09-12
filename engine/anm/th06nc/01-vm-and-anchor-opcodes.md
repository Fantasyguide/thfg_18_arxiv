# TH06NC ANM VM 与新增的锚点 opcode（32–35）

> **版本**：TH06NC（`th06nc.exe`，MD5 `b0cc28ec904e9efce2439ad857c938b5`，imagebase `0x140000000`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

状态：VM 定位与 opcode 语义 ✅ 一手验证（跳表 + 指令字节 + 125 个样本文件独立解析）。
「NC 为什么加这几条」原为 🟡 推测，现已**部分证伪**，见 §6；
与 TH06 的逐项对照见 [`02-th06-baseline-comparison.md`](02-th06-baseline-comparison.md)。

TH06NC 的 ANM 仍是 **v0 容器 + TH06 式解释器**，但指令集从 **0–31 扩到 0–35**。
新增的四条全部是**无参数**的锚点设置，把 TH06 的「两个开关位」改成了一个 3 位枚举，
凑成 3×2 的锚点网格。

| opcode | 暂定名 🟡 | 锚点 | 样本中出现 |
| --- | --- | --- | --- |
| 23（TH06 已有） | `anchorTopLeft` | 左 / 上 | 425 次 |
| **32** | `anchorTopCenter` | 居中 / 上 | 81 次 |
| **33** | `anchorLeft` | 左 / 居中 | **0 次**（实现了但未使用） |
| **34** | `anchorTopRight` | 右 / 上 | 77 次 |
| **35** | `anchorRight` | 右 / 居中 | 1 次 |
| （不设） | — | 居中 / 居中 | 默认 |

---

## 1. VM 定位：`0x1400069b0` = `AnmManager::ExecuteScript`

- **① 发现**：`0x1400020b0(this, idx, "data/result00.anm", spriteOffset)` 被结果页初始化
  `0x140073d50` 用来装载 anm，签名与 TH06 `AnmManager::LoadAnm` 逐参对应；
  它所在的 `0x140001000`–`0x14000c000` 段共 73 个函数，是 AnmManager 的方法区。
  该段内 `0x1400069b0`（2788 字节）带跳表且用到 `-999` 哨兵。
- **② 推测**：`0x1400069b0` 是 `AnmManager::ExecuteScript`。
- **③ 验证**：逐 case 与社区 `AnmManager/ExecuteScript.c` 对形：
  case 0 清 `flags&~1` 后**落穿**进 case 15 把 `currentInstruction` 清零并 `return 1`；
  case 5 `Jump` 把指令指针设为 `scriptStart + arg` 并把脚本时间重设为目标指令的 `time`；
  case 7/8 翻 `flags^0x40`/`^0x80` 且把对应缩放取负；case 16 走 PRNG 选随机精灵；
  case 18/19/20 共用一个 handler、只差 flags 位。全部同构。
- **④ 结论**：`0x1400069b0` = `AnmManager__ExecuteScript(this, vm)` ✅。
- **⑤ 证据**：`0x1400069b0`、`0x1400020b0`、`0x140073d50`；
  `local/vendor/th06-re/src/th06_102h.exe/AnmManager/ExecuteScript.c`。

顺带确认的锚点：`0x140a6e9b0` = `g_AnmManager`（结果页 `0x140007530(g_AnmManager, vm, …)`
与总状态机 `0x1400479e0` 里的 `0x140002800(g_AnmManager, idx)` 两处独立佐证）。

### VM 字段（相对 vm 基址）

| 偏移 | 含义 | 依据 |
| --- | --- | --- |
| `+0x8a` | `pendingInterrupt`（s16） | `0x1400069b0` 入口判空；结果页按步长 `0x120` 批量写 |
| `+0xb8` | 脚本当前时间 | `MOV R8D,[R11+0xb8]` 与指令 `time` 比较 |
| `+0xc4` | `flags`（u32） | 本文主角；opcode 1 是 `OR [R11+0xc4],0x1` |
| `+0xf0` | 脚本起始指针 | case 5 的跳转基址 |
| `+0xf8` | `currentInstruction` | 入口 `CMP qword ptr [RDX+0xf8],0x0` |
| `+0x100` | 当前精灵指针 | 绘制路径全程解引用它 |

**VM 步长 `0x120`** ✅：结果页对 38 个 VM 写 `pendingInterrupt` 的地址是
`+0x13a / +0x25a / +0x37a …` 等差 `0x120`，且 `0x13a − 0x8a = 0xb0` 正好是首个 VM 的基址。

## 2. 分派上界：VM 只认 0–35

```
140006a3e  MOVSX EAX,word ptr [R10]          ; time  = instr[+0]  (s16)
140006a42  CMP   EAX,R8D                     ; 与 vm->scriptTime 比较
140006a45  JG    0x140006e0a                 ; 还没到点
140006a4b  MOVZX EDX,byte ptr [R10 + 0x2]    ; opcode = instr[+2] (u8)
140006a50  CMP   DL,0x23                     ; 35
140006a53  JA    0x1400070e3                 ; > 35 → 直接走推进尾部
140006a59  MOV   EAX,dword ptr [RSI + RDX*0x4 + 0x7494]   ; RSI = imagebase
140006a60  ADD   RAX,RSI
140006a63  JMP   RAX
```

- **① 发现**：跳表在 `0x140007494`，36 条 RVA；上界 `CMP DL,0x23` / `JA`。
- **② 推测**：指令集恰好是 0–35，没有稀疏高位 opcode。
- **③ 验证**：用 [`tooling/anm0_scan.py`](../../../tooling/anm0_scan.py) 独立解析全部
  **125 个 `.anm` 文件**（不使用 thanm 的格式表，避免循环论证），
  出现过的 opcode 是 `0–5, 7, 9–31, 32, 34, 35`，**没有任何 ≥36 的值**。
- **④ 结论**：TH06NC ANM 指令集 = 0–35，比 TH06 的 0–31 多四条 ✅。
- **⑤ 证据**：`0x140006a4b`–`0x140006a63`、跳表 `0x140007494`；扫描器输出见 §5。

推进尾部 `0x1400070e3` 是 `MOVZX ECX,byte ptr [R10+0x3]` / `ADD RCX,0x4`，
即 **下一条 = 当前 + 4 + argsSize**，与 v0 指令头 `{s16 time; u8 opcode; u8 argsSize}` 一致 ✅。

跳表还顺带给出两条：

- **opcode 6 与 22 直接指向 `0x1400070e3`** —— 执行期是纯 no-op ✅。
  22 是 `interruptLabel`，只在 `0x1400068f0` 的中断扫描里被读，不在执行路径上生效。
- opcode 18/19/20 共用 handler `0x140006d44` ✅。

## 3. 四条新 opcode 的指令字节

| opcode | handler | 指令 |
| --- | --- | --- |
| 23 | `0x140006fbe` | `AND [R11+0xc4],0xfffffcff` / `OR [R11+0xc4],0x400` |
| 32 | `0x140007085` | `AND [R11+0xc4],0xfffffbff` / `OR [R11+0xc4],0x300` |
| 33 | `0x14000709d` | `AND [R11+0xc4],0xfffff9ff` / `OR [R11+0xc4],0x100` |
| 34 | `0x1400070cd` | `AND [R11+0xc4],0xfffffdff` / `OR [R11+0xc4],0x500` |
| 35 | `0x1400070b5` | `AND [R11+0xc4],0xfffffaff` / `OR [R11+0xc4],0x200` |

每条都只有两条指令，随后跳到推进尾部；**不读任何参数**。

关键性质：这五条里，bit 8/9/10 **每一位要么被 `AND` 强制清零、要么被 `OR` 强制置一**，
没有一位保留原值。所以它们是**绝对赋值而非切换**，幂等、与先前状态无关 ✅。
把 `flags` 的 bit 8–10 当作一个 3 位字段 `F` 来看：

| opcode | 23 | 32 | 33 | 34 | 35 |
| --- | --- | --- | --- | --- | --- |
| `F` | 4 | 3 | 1 | 5 | 2 |

**参数长度的独立佐证** ✅：扫描 125 个文件，opcode 32/34/35 的 `argsSize` **恒为 0**，
与「handler 不读参数」完全一致——代码侧与数据侧各自独立得出同一结论。

## 4. `F` 的语义：3×2 锚点网格

- **① 发现**：五个绘制函数 `0x140002a80` / `0x1400036c0` / `0x140004310` /
  `0x140004dc0` / `0x140005a60` 都读 `*(uint *)(vm + 0xc4) & 0x700`，且**按值相等比较**
  （`== 0x100 || == 0x400` 之类），不是按位测试。
- **② 推测**：TH06 的 `bit8 = 左对齐 / bit9 = 上对齐` 两个开关位，在 NC 里被换成了枚举。
- **③ 验证**：在 `0x140002a80` 里，设 `cx` 为换算后的 `pos.x`、`W` 为缩放后精灵宽度，
  三个分支给出的横向跨度分别是：

  | `F` | 跨度 | 含义 |
  | --- | --- | --- |
  | 1, 4 | `[cx, cx+W]` | `pos` 是**左**边缘 |
  | 0, 3 | `[cx−W/2, cx+W/2]` | **居中** |
  | 其余 | `[cx−W, cx]` | `pos` 是**右**边缘 |

  纵向条件 `((F' − 0x300) & 0xfffffcff) == 0 && F' != 0x600` 化开就是 `F ∈ {3,4,5}` →
  `pos` 是**上**边缘；否则垂直居中。
  `0x1400036c0` 写成「相对居中基线加减半宽/半高」、`0x140004dc0` 把纵向拆成两次测试，
  **集合完全相同**，五处一致。
- **④ 结论**：`F` 是锚点枚举 ✅：

  | `F` | X | Y | 由谁设置 |
  | :---: | --- | --- | --- |
  | 0 | 居中 | 居中 | 默认（不设） |
  | 1 | 左 | 居中 | **33** |
  | 2 | 右 | 居中 | **35** |
  | 3 | 居中 | 上 | **32** |
  | 4 | 左 | 上 | 23（TH06 已有） |
  | 5 | 右 | 上 | **34** |
  | 6, 7 | 右 | 居中 | 无 opcode 可达 |

- **⑤ 证据**：`0x140002a80`、`0x1400036c0`、`0x140004310`、`0x140004dc0`、`0x140005a60`；
  社区对照 `AnmManager/Draw.c:45-51`、`AnmManager/DrawNoRotation.c:22-31`（TH06 侧是
  `vm->flags >> 8 & 1` 与 `& 2` 两个独立位）。

### 编码变了，opcode 23 的语义没变

TH06 的 opcode 23 是 `vm->flags |= (0x100|0x200)`（只置位，位型 = 3 = 左+上）。
NC 的 opcode 23 改成了 `&~0x300 | 0x400`（位型 = 4）。
**但在各自的解码规则下两者都是「左上」** ✅——NC 只是把位型重排成枚举，
opcode 层面的含义没动。反过来，NC 里位型 3 现在表示「居中+上」，由新 opcode 32 设置。

> 这一点值得记牢：**不能拿 TH06 的 `flags` 位型直接套 NC 的**，
> 但可以放心沿用 TH06 的 opcode 语义。

## 5. 数据侧全库统计

`python3 tooling/anm0_scan.py local/th06nc/extracted`，125 个文件全部按 v0 干净解析：

| opcode | 次数 | argsSize |
| --- | --- | --- |
| 0 / 15 / 21 / 24 | 269 / 411 / 555 / 504 | 0 |
| 1 | 2183 | 4 |
| 17 | 1279 | 12 |
| 12 | 902 | 8 |
| 22 | 832 | 4 |
| 19 | 828 | 16 |
| **23** | **425** | **0** |
| **32** | **81** | **0** |
| **34** | **77** | **0** |
| **35** | **1** | **0** |
| **33** | **0** | — |

- 其余 opcode 的 `argsSize` 全部单值，且与 TH06 格式表吻合（17→12、18/19/20→16、30→12）✅。
- **opcode 6（`nop`）和 8（`flipY`）同样一次都没用过**——所以「33 未使用」并不孤立，
  不能据此怀疑 33 的实现有问题。

新 opcode 的分布：

| opcode | 分布 |
| --- | --- |
| 32 | **只在** `th06IN/text*.anm`，9 个语言版本**各 9 次** |
| 34 | `th06IN/text*.anm` 各 8 次 + `th06CM/front.anm` 1 次 + `th06TL/spellselect00.anm` 4 次 |
| 35 | 只有 `th06CM/front.anm` 1 次 |

## 6. 为什么加这几条：🟡 推测

**已验证的事实** ✅：`text*.anm` 的 entry 名字是 `"@"`、`hasData = 0`。
按 v0 容器的约定（thtk `anm_types.h` 注释：TH06 里 `@` 是唯一的表达方式），
这表示该 entry **没有内嵌贴图，是运行时创建的空白纹理**——52 个 2048×36 的文本槽，
游戏运行时把本地化字符串画进去（NC 侧的绘制入口 `0x140007530` → 字形渲染 `0x14007d2c0`）。
所以**每一处 `ins_32` / `ins_34` 锚定的都是运行时渲染的文本槽**。

**曾经的推测**：NC 有 11 种语言，同一句话渲染宽度不同，因此需要居中/右对齐锚点
来保持版面。这与分布高度吻合（新 opcode 只出现在本地化文本 anm 和两处标签宽度可变的菜单里）。

**❌ 但那条因果已被证伪**：拿到 TH06 exe 后两边都查了，**谁都不会按实测文本宽度
改写精灵矩形** ✅——
`th06:0x434b60`（`AnmManager__DrawVmTextFmt`）与本作的 `0x140007530` 是同一形状：
读精灵的 `startPixelInclusive` / `textureWidth` 去画字，然后只置 `flags |= 1`，**全程不写精灵**；
TH06 社区源码里 `widthPx`/`heightPx` 也只出现在读侧（唯一的写在加载器 `LoadSprite`）。
所以锚点作用的是**固定尺寸的槽**，不是实际文本宽度。
完整对照见 [`02-th06-baseline-comparison.md`](02-th06-baseline-comparison.md) §4。

**因此本节现在只剩一条 ❓**：既然槽是定宽的，锚点对文本槽究竟起什么作用？
候选是 `0x14007d2c0`（字形渲染）自己在槽内做对齐——**未验**。
「NC 为什么加这四条」在找到这一环之前不算解释清楚。

## 6.5 和后续作品的关系

**后续作品确实有直接的锚点指令，而且比 NC 更强。** 社区 mapfile 给出的路径是
`v0/v2` 的无参 `anchorTopLeft` → **`v4`（TH09.5 起）改名成带参的 `65 anchor`** →
`v8`（TH13 起）的 `421 anchor(h, v)` 加上 `436 anchorOffset(dx, dy)`。

TH16 那条已在本仓一手反完（[`../th16/07-general.md`](../th16/07-general.md) §6.1）：
`421` 是**一个 u32**，`h = 低 16 位` → `th16:flags_lo` bits 21–22、`v = 高 16 位` → bits 23–24，
h 取 0 中 / 1 左 / 2 右，v 取 0 中 / 1 顶 / **2 底**。

所以 TH06NC 是把现代的锚点概念**用 v0 的老风格（一个组合一条无参 opcode）补了回来，但没补全**：
它有右对齐，**没有下对齐**——3×2 而不是 3×3。

⚠️ `local/vendor/truth/map/v0.anmm` 只到 opcode 31，**社区对 32–35 没有记录**；
本文的结论因此走的是 [`METHOD.md`](../../../METHOD.md) 的「超过社区」闸门（一手到底 + 字节级 + 数据侧独立复算）。

跨版本对照表见 [`../OVERVIEW.md`](../OVERVIEW.md) 的「锚点的跨版本演化」。

## 7. 下一步

1. ❓ 查 `0x14007d2c0` 是否在定宽槽内做对齐——这是 §6 剩下的唯一一环
   （「有没有精灵矩形写回」已收口，答案是没有）。
2. ❓ `F = 6/7` 无 opcode 可达，确认是否真的死码（也可能被别处直接写 `flags`）。
3. — 其余 0–31 的 opcode 只做了对形，没有逐条一手复核；本文不对它们的细节下断言。
4. — ANM 加载器 `0x1400020b0` 的边界与 `AnmManager` 结构体字段尚未展开。

## 8. 相关地址一览

| 地址 | 含义 |
| --- | --- |
| `0x1400069b0` | `AnmManager__ExecuteScript` |
| `0x140007494` | opcode 跳表（36 条 RVA） |
| `0x1400070e3` | 推进尾部；也是 opcode 6/22 与越界 opcode 的落点 |
| `0x140006fbe` | opcode 23 handler |
| `0x140007085` | opcode 32 handler |
| `0x14000709d` | opcode 33 handler |
| `0x1400070cd` | opcode 34 handler |
| `0x1400070b5` | opcode 35 handler |
| `0x140006d44` | opcode 18/19/20 共用 handler |
| `0x1400068f0` | 中断标签扫描 |
| `0x140002980` | `AnmManager__SetActiveSprite` |
| `0x1400020b0` | `AnmManager__LoadAnm` |
| `0x140002a80` | 绘制（主路径） |
| `0x1400036c0` · `0x140004310` · `0x140004dc0` · `0x140005a60` | 绘制变体 |
| `0x140007530` | 把字符串画进 VM 的文本槽 |
| `0x14007d2c0` | 字形渲染（带缓存） |
| `0x140a6e9b0` | `g_AnmManager` |

## 9. 复现

```sh
# 数据侧：不依赖 thanm 格式表的独立统计
python3 tooling/anm0_scan.py local/th06nc/extracted

# 代码侧：把符号打进 Ghidra
source tooling/env.sh
python tooling/ghidra/apply_th06nc_anm.py --dry-run
```

跳表和五条 handler 也可以完全脱离 Ghidra 复核：

```sh
python3 - <<'EOF'
import struct
d = open('local/th06nc/th06nc.exe', 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]
optsz = struct.unpack_from('<H', d, pe + 20)[0]
secs = [struct.unpack_from('<IIII', d, pe + 24 + optsz + i * 40 + 8) for i in range(nsec)]
def v2f(va):
    rva = va - 0x140000000
    for vs, sva, rs, ptr in secs:
        if sva <= rva < sva + max(vs, rs):
            return ptr + (rva - sva)
for op in (23, 32, 33, 34, 35):
    rva = struct.unpack_from('<I', d, v2f(0x140007494 + op * 4))[0]
    va = 0x140000000 + rva
    print(op, hex(va), d[v2f(va):v2f(va) + 22].hex(' '))
EOF
```
