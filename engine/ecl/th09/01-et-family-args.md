# 01 — TH09 的 `et*` 发射器族（opcode **96–104**）的**实参顺序**（转写用）

> **版本**：TH09 v1.50a（`local/th09.v1.50a/th09.exe`，imagebase `0x400000`）。
> **方法**：一手 —— 派发表（文件偏移 `0xb4a4` = VA `0xc0a4`，见 §1）+ 反汇编共用实作 `0x408040`。
> **可信度**：实参顺序与落点 ✅（逐指令）；**每个字段的「游戏内含义」** 🟡（下一步要跟
> `0x4131c0` 或与 TH16/18 的 `etNew` 族对名）。2026-10-01。

## 1. 这一族共用一个 handler

TH09 的 ECL 派发表 = **文件偏移 `0xb4a4`（VA `0xc0a4`）起 186 个 handler 地址**，按
`opcode - 2` 索引（拿 ExpHP `th-re-data` 的 `ecl` label 对齐，**146 条一致命中**）。

| opcode | 名字（从**同代** `th08.eclm` 借）| handler |
| :-: | --- | --- |
| 96 | `etFanAimed` | `0x40a168` |
| 97 | `etFan` | `0x40a168` |
| 98 | `etRingAimed` | `0x40a168` |
| **99** | **`etRing`** | `0x40a168` |
| 100 | `etRingAimedAway` | `0x40a168` |
| **101** | **`etRingAway`** | `0x40a168` |
| 102 / 103 / 104 | `etRandAng` / `etRandSpd` / `etRand` | `0x40a168` |

⇒ **96–104 共用一个入口**（`0x40a168` 是「共用取参序言」：`call 0x408040` 后各支 `jmp 0x408750`
回 VM 主循环）。`0x408040` 是**真正的实作**，节拍见 §2。

## 2. 实参顺序（`0x408040` 逐指令读出）

Eclipse 指令流里，`esi` 指向这条指令的描述块：

- `[esi + 4]` = **opcode** ⇒ ★ **`mode = opcode − 0x60`**（96 → mode 0）写进 `emitter+0x1f8`（word）
- `[esi + 0xa]` = **实参类型位掩码**（`byte`）：**第 i 位 = 「第 i 个实参是表达式/变量」**；
  置位时 `call 0x405760`（求值），否则直接取立即数。
  用的位：`0x01`(arg1)、`0x02`(arg2)、`0x04`(arg3)、`0x08`(arg4)、`0x10`(arg5)、`0x20`(arg6)、
  `0x40`(arg7)、`0x80`(arg8)。
- 实参槽（**注意 arg1/arg2 是同一个 dword 的高低 16 位**）：

| 实参 | 字节码偏移 | 类型 | 落点 |
| :-: | :-: | :-: | --- |
| arg1 | `+0xc`（**低 16**）| int | `emitter + 0x00`（word）|
| arg2 | `+0xe`（**高 16**）| int | `emitter + 0x02`（word）|
| arg3 | `+0x10` | int | `emitter + 0x1f4`（word）|
| arg4 | `+0x14` | int | `emitter + 0x1f6`（word）|
| arg5 | `+0x18` | float | `emitter + 0x18` |
| arg6 | `+0x1c` | float | `emitter + 0x1c` |
| arg7 | `+0x20` | float | `emitter + 0x10` |
| arg8 | `+0x24` | float | `emitter + 0x14` |
| arg9 | `+0x28` | int | `emitter + 0x1fc` |

另外：
- `emitter + 0x1fa`（word）恒写 0；
- 发射器的**初始位置**来自 `call 0x401100`（对 `enemy+0x2dd4` / `enemy+0x2e04` 做的 Float3 运算），
  结果拷进 `emitter + 4`（float3）；
- 最后 `call 0x4131c0` = **`BulletManager::shoot_rival_bullets_array`**（ExpHP 命名）—— 真正发弹。

## 3. 与 TH16/TH18 `etNew` 族的对应（翻译目标）

TH09 是「**一条指令带全部参数**」的老式风格；TH16/TH18 拆成了
`etNew`(600) ＋ 一串 setter（`etOn`/`etSprite`/`etOffset`/`etAngle`/`etSpeed`/`etCount`/`etAim`/…）。
两边的**模式表**能对上（`engine/ecl/th16/ECL-info.md` 的 `etAim` mode 表）：

| TH09（`mode = opcode − 96`）| TH16/18 写法 |
| --- | --- |
| 96 `etFanAimed`（mode 0）| `etAim 0`（自机狙）+ `etCount(路数,层)` + `etAngle(偏移, 夹角)` |
| 97 `etFan`（mode 1）| `etAim 1`（固定弹）+ `etCount` + `etAngle(起始角, 夹角)` |
| 98 `etRingAimed`（mode 2）| `etAim 2`（自机狙环形）|
| **99 `etRing`（mode 3）** | `etAim 3`（固定弹环形）+ `etCount` + `etAngle(起始角, 层间偏角)` |
| 100 `etRingAimedAway`（mode 4）| `etAim 4`（偶数狙环形）|
| **101 `etRingAway`（mode 5）** | `etAim 5` / 或 `etAim 3` + 起始角偏 π（🟡 待定）|
| 102/103/104（mode 6/7/8）| `etAim 6`/`7`/`8`（含角度/速度涨落）|

## 4. ★★ 字段级交叉映射（TH09 → TH16/18）—— **「Ctrl+C」可行的依据**

两侧的字段表放在一起就能对名。TH16/18 那侧是**已经逐字段坐实过的**
（`engine/ecl/th16/05-fire-interface.md` §3.1 写入侧 + `engine/bullet/th16/01` §6 读取侧，
双向印证；TH18 的 opcode 号与 TH16 相同）：

| TH09 实参 | 类型 | TH16/18 | 目标 desc 字段 | 依据 |
| --- | :-: | --- | --- | --- |
| `mode = opcode − 96` | — | `etAim(mode)` | `[0xda]` | 两侧的 mode 表逐个对上（§3）|
| **arg1 / arg2** | int/int | **`etSprite(arg1, arg2)`** | `[0]`=type、`[1]`=subtype | ★ **TH09 的 arg1/arg2 就是同一个 dword 的高低 16 位**（§2），而 `etSprite` 正好取一对 `(type, subtype)` ⇒ **结构完全吻合** |
| arg3 / arg4 | int/int | 疑 `etCount(路数, 层数)` | `[0xd9]` 低16=路数、高16=层数 | 位置上是一对 int（🟡 待坐实）|
| arg5 / arg6 | float/float | 疑 `etSpeed(第一层速度, 最后一层速度)` | `[7]` / `[8]` | 灵梦那四条的 arg6 = **`0.5f`**（低速度）——与「环弹慢速」以及文档里「速度实参 0.5f」的观察一致（🟡）|
| arg7 / arg8 | float/float | 疑 `etAngle(基准角, 层间偏角)` | `[5]` / `[6]` | 同四条里 arg8 = **`0.0f`** ⇒ 层间偏角 0；arg7 是变量（各支不同）⇒ 基准角（🟡）|
| arg9 | int | 🟡 **未定** | `[0x1fc]` | 灵梦那四条的 arg9 = `514`/`642`；同一轮的观察：`0x202`/`0x282` 的**高字节都是 2**（🟡 见 `engine/card/th09/02` §4.6）|

## 5. ★★★ 读取侧坐实（`0x4131c0` = `shoot_rival_bullets_array`，2026-10-01）

```c
/* th09:0x4131c0  —— 实参 = (emitter)  */
eax = (int16)*(emitter + 0x00) * 0xD48 + bullet_mgr;   /* ★ arg1 = 「弹类型记录」的索引，stride 0xD48 */
*(emitter + 0x210) = eax;                              /* 该类型记录的指针存这里 */
call 0x41bb70(emitter + 4, …); fstp [ebp-8];           /* 位置 →（基准角？）一个 float */
for (wave = 0; wave < (int16)*(emitter + 0x1F6); ++wave)        /* ★ arg4 = 波数/层数 */
    for (i   = 0; i   < (int16)*(emitter + 0x1F4); ++i)          /* ★ arg3 = 路数 */
        edi = shoot_one_bullet(emitter, i, wave, [ebp-8], 1);    /* 0x412960 */
if ((*(emitter + 0x1FC) >> 8) & 2) call 0x43e380(…);   /* ★ arg9 是**标志位**：bit9(0x200) 触发 */
```

⇒ **逐条订正/坐实**：

| 实参 | 结论 | 可信 |
| :-: | --- | :-: |
| **arg3 = `emitter+0x1f4`** | ★ **路数**（内层循环上界）| ✅✅ 与 TH16/18 的 `etCount` **低16=路数**逐字同构 |
| **arg4 = `emitter+0x1f6`** | ★ **波数/层数**（外层循环上界）| ✅✅ 同 `etCount` **高16**（TH16/18 用一整 word 装它）|
| **arg1 = `emitter+0x00`** | ★ **弹「类型记录」索引**：`idx * 0xD48 + bullet_mgr` ⇒ 一条 **0xD48 字节**的类型记录 | ✅ 一手（不是「sprite 对」——我 §4 那行的猜测**偏了**，见下）|
| **arg9 = `emitter+0x1fc`** | ★ **标志位**（bit9 = 0x200 触发 `0x43e380(emitter+4, emitter+0x200)`）| ✅ 一手 |

**⇒ §4 的两条要订正**：

1. ~~「arg1/arg2 = `etSprite(type, subtype)`」~~ —— **arg1 是「类型记录索引（stride 0xD48）」**，
   `0xD48` 那种体量不像一个 sprite 号；arg2（`emitter+0x02`）在本函数里**没有被读**（可能给
   `shoot_one_bullet` 或其它分支用）⇒ **改成 🟡 待定**，别按 `etSprite` 硬套。
2. ~~「arg9 = 514/642 那种 id / 高字节=组」~~ —— **arg9 是标志位**：灵梦四条里的
   `514 = 0x202` / `642 = 0x282` / 咲夜的 `18946 = 0x4A02` **都带 bit1(0x2) 与 bit9(0x200)**
   ⇒ 那才是它们「看起来同族」的真正原因（不是「高字节=组」）。

## 6. ★★ 四个 float 的归属（`shoot_one_bullet` `0x412960` 读取侧，2026-10-01）

在 `0x412cb9`–`0x412cd3` 读到**环的铺法公式**（`[0x48e4c4]` 实测 = **6.2831855 = 2π**）：

```asm
0x412caf  movsx eax, word [esi+0x1f4]   ; eax = 路数
0x412cb9  fild  [ebp+0xc]               ; i
0x412cbf  fmul  [0x48e4c4]              ; ★ × 2π
0x412cc5  fidiv [ebp+0xc]               ; ÷ 路数        ⇒ (i × 2π) / 路数
0x412cc8  fild  [ebp+0x10]              ; wave
0x412ccb  fmul  [esi+0x14]              ; ★ × arg8
0x412cd0  fadd  [esi+0x10]              ; ★ + arg7
0x412cd3  fstp  [ebp+0x18]              ; = 这一颗的角
```

⇒ **arg7（`+0x10`）= 基准角**、**arg8（`+0x14`）= 层间偏角** ——
与 `engine/ecl/th16/ECL-info.md`「**对圆形弹幕，ang1 同样是角度偏移量…ang2 是相邻『层』之间的
角度偏移量**」**逐字相符** ⇒ 这一对就是 **`etAngle`**。✅✅

另一路（`0x412c98`）：
```asm
fld [esi+0x18]; fsub [esi+0x1c]; call 0x404910(this=…); fadd [esi+0x1c]
→ 0x412d59  mov [ebx+0xd70], ecx        ; ★ arg5/arg6 的结果落到 bullet+0xd70
```
⇒ `arg5/arg6`（`+0x18`/`+0x1c`）是**另一对角度**（`normalize(arg5 − arg6) + arg6` 是「以 arg6 为基准
把 arg5 归一化」的惯用写法），落到 **`bullet+0xd70`**；而这一颗的角 → `call 0x42aed0(角, 0)`
（返回 float，落 `bullet+0xd7c`，形态像 sin/cos 的方向换算）。

## 7. ★★★ `arg5/arg6` 定案 = **`etSpeed`**（`0x404910` = `random01() × x`）

跟进去读 `0x404910` 与它调的两层：

```asm
0x404910  push ebp; mov ebp,esp
0x404913  call 0x42ae70          ; ← f(ecx)，而调用点的 ecx = 常量 0x4ace0c
0x404918  fmul [ebp+8]           ; × 压入的 (arg5 - arg6)
0x40491c  ret 4

0x42ae70  push ecx; call 0x42ae50 ; rand() → int
          fild [ebp-4]            ; 转 float
          jge +; fadd [0x48e2a8]  ; ★ 负数补 2^31（0x48e2a8 = 2147483648.0）
          fmul [0x48fb7c]         ; ★ 归一（1/2^32 量级）⇒ 结果落在 [0,1)
```

⇒ **`0x404910(x)` = `random01() × x`**。所以 `0x412c98` 那一路是：

```c
[ebp-8] = random01() * (arg5 - arg6) + arg6;   /* ★ 每颗弹一个 [arg6, arg5] 的随机值 */
0x412d59  *(float *)(bullet + 0xd70) = [ebp-8];
```

⇒ **`arg5/arg6` 是「速度」那一对** —— 与 TH16/18 文档「**对随机子弹，它的两个参数代表了
子弹速度的涨落的中心和范围**」相符 ⇒ 对应 **`etSpeed(第一层速度, 最后一层速度)`**。
（我 §6 那个「不是 speed」的猜测**作废**；`arg1` 指的类型记录是**弹型/贴图**那一半，速度在参数里。）

### 7.1 ★ 九个实参全部落位（TH09 → TH16/18 最终对照表）

| TH09 实参 | 语义 | TH16/18 | 目标字段 |
| :-: | --- | --- | --- |
| — | `mode = opcode − 0x60`（96→0）| `etAim(mode)` | `desc[0xda]` |
| **arg1** | 弹「类型记录」索引（`idx × 0xD48 + bullet_mgr`）| 类型/贴图那一半（`etSprite` 的 type 侧 🟡）| `emitter+0x210` |
| arg2 | 🟡 本族未读（可能给 `shoot_one_bullet` 的其它分支）| — | `emitter+0x02` |
| **arg3** | **路数**（内层循环上界）| `etCount` 低 16 | `desc[0xd9]` 低 |
| **arg4** | **波数/层数**（外层循环上界）| `etCount` 高 16 | `desc[0xd9]` 高 |
| **arg5 / arg6** | **速度**（每颗取 `rnd×(a−b)+b` ⇒ 随机涨落区间）| **`etSpeed`** | `desc[7]/[8]` |
| **arg7 / arg8** | **基准角 / 层间偏角**（`(i×2π)/路数 + wave×a8 + a7`）| **`etAngle`** | `desc[5]/[6]` |
| **arg9** | **标志位**（bit9 = `0x200` → `call 0x43e380`）| 🟡 | `emitter+0x1fc` |

⇒ **「Ctrl+C」的两侧规格到这一节就算齐了**：模式、类型、路数/波数、速度、角度**全部对名**。

⚠ **还没做**：`0x43e380`（arg9 bit9 那支）是什么；`arg2` 的用途；以及**开始逐条转写**
`pl00/pl01/pl02.ecl`（下一步就是它）。

## 8. ★★ `arg9`（标志位）的语义：**bit9 = 发射时播音效** ⇒ 对应 TH18 的 `etSound`

一手（反汇编 `0x43e380`，即 `0x4131c0` 里 `arg9 & 0x200` 触发的那一支）：

```asm
0x43e380  fld  [ebp+0xc]                    ; 一个 float 参数（音量/声像？）
0x43e38a  fmul [0x4914ac]                   ; 乘一个常量
0x43e390  movsx eax, word [ebx*8 + 0x4a1f86]  ; ★ 按 `ebx`（= arg9 的另一路参数）查表，stride 8
0x43e39f  call 0x47b1d4                     ; ← 交给声音系统
0x43e3b0  mov edi, [edx]                    ; ★ 在 `this+0x620` 起的 **12 个槽**里找目标
0x43e3be  cmp ecx, 0xc                      ;    （12 = 12 个声道）
0x43e3ca  mov edx, [esi + ecx*4 + 0x650]
0x43e3d1  cmp edx, 0x80                     ; ★ 每声道一个计数器与 0x80（128）比较（冷却/优先级）
```

⇒ **结构 = 「12 个声道 + 每声道一个计数器 + 按 id 查表 + 一个 float 参数」= 典型的 ZUN SE 播放**。
⇒ **`arg9` 的 `bit9`（`0x200`）= 「这一轮发弹时顺便播一个音效」**。

**交叉印证**：灵梦那四条的 `514 = 0x202` / `642 = 0x282` / 咲夜的 `18946 = 0x4A02`
**都带 bit1(0x2) 与 bit9(0x200)** ⇒ 它们「看着同族」的真因就在这里（bit1 🟡 未定 —— 可能也是渲染/命中
相关的开关；bit9 = 播音）。

⇒ **转写时**：TH09 的 `arg9` bit9 ≈ TH18 的 **`etSound(id, sfx)`**（TH16/18 把它独立成一条）。★ 我们
现在**没有转写它**（TH18 需要的是 sfx id，而一手只给了「要不要播」这一位）⇒ 登记为「已知差异」，
不算漏（若将来要补：从 TH09 那张表 `0x4a1f86` 读出 id，再映射到 TH18 的 wav）。
