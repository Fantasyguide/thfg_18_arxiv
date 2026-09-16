# TH18 收点线高度（`poc_height`）与开局那条指示线 —— 字段、两个写点、原生卡 22 ITEM_LINE

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th11:0x…`。
> **方法**：一手反汇编 —— 全 `.text` 扫 `[reg+0x47998]` 的 disp32 字节模式（`98 79 04 00`，7 处命中，逐处反查指令边界）+
> 卡工厂跳表（`0x412dac`）解 id→虚表 + ExpHP 名（[th-re-data](../../../local/vendor/th-re-data/) 的
> `CardKanako__on_load` / `CardKanako__on_load__2`）+ `engine/card/th18/03-hooks.md` 的虚表槽调用点。
> **可信度**：指令级 ✅（字段、写值、写点、卡身份、指示线的读点与公式）；「指示线何时重建 / 中途改了高度线会不会动」= 🟡（§6）。
> **归属**：引擎知识。想「改收点线高度」的卡见 §5；改造侧共用件见
> [`mods/th18.v1.00a/card-expand`](../../../mods/th18.v1.00a/card-expand/SDK.md) 的 PoC 高度层。

## 0. 一句话结论

- **字段** = `player+0x47998`（float，640 空间 y，**越大越靠屏幕下方 = 收点线越低**）。
- **全 exe 只有两个写点**：`Player__reset` 写 **128.0**（`0x45ae8f`）、原生卡 **22 ITEM_LINE（CardKanako）的 `on_load`** 写 **224.0**（`0x40c275`）。
  **没有每帧写点** —— 这张原生卡是**开局一次性**把线降下去。
- **指示线不是贴图恒定物，也不是卡自己画的**：它是引擎 GUI 自己建的 VM（`front.anm` **script 57 = `0x39`**），
  位置**每帧从字段现算**（`y640 = poc_height × 2 − 80.0`），可见性由 **`Gui+0x194` 的 bit `0x2000`** 把门，
  而这一位的**唯一写入者就是那张原生卡**。
- ⇒ **改了字段，线的位置自动跟着变**（引擎读字段，不读常量）；要做的只是「**在开局之前把字段写好** + **把 `0x2000` 置上**」。

## 1. 字段与两个写点（穷举）

穷举法：`PatternScan('98 79 04 00')` 扫全 exe → 7 处 disp32 命中，其中 `.text` 内 7 处逐处反查指令边界，
得到 **2 个写、5 个读**（无第三类）。计算出来的地址（`lea`/指针运算）在这套代码里没有出现。

| 站点 | 机器码 | 指令 | 角色 |
| --- | --- | --- | --- |
| `0x45ae8f` | `c78698790400 00000043` | `mov dword [esi+0x47998], 0x43000000` | **写 128.0f**（`Player__reset`，同一段还写 `+0x4798c`=30.0 / `+0x47990`=`+0x47994`=70.0 / `+0x4799c`=`+0x479a0`=3.0）|
| `0x40c275` | `c78098790400 0000006043` | `mov dword [eax+0x47998], 0x43600000` | **写 224.0f**（`CardKanako__on_load`，`eax = [0x4cf410]` = 自机指针）|
| `0x43afe1` | `f30f108098790400` | `movss xmm0, [eax+0x47998]` | 读（**开局指示线**，见 §4）|
| `0x445baa` / `0x445ff4` | `f30f108698790400` | `movss xmm0, [esi+0x47998]` | 读（`ItemManager__on_tick__body` 两条支路：线上 ⇒ 道具无条件真空）|
| `0x44698d` | `f30f108a98790400` | `movss xmm1, [edx+0x47998]` | 读（`ItemManager__collect_power_item`：线上满额 / 线下按高度折）|
| `0x446bf9` | `f30f108698790400` | `movss xmm0, [esi+0x47998]` | 读（`ItemManager__collect_money_item`，同上；本仓 `sdk.c` 的 `BP_ce_item_score_in` 就挂在这里）|

四条引擎读点的形状**完全同形**：`comiss poc, [player+0x624]`（自机 y）+ 条件跳转 ⇒ **唯一语义 = 「自机在线上还是线下」**。
⇒ 改这个字段 = 一次性改「全屏收点 + 绿点/P 点满额回收」的**高度门**，所有读它的机制一起变（这正是要抽层的原因）。

## 2. 那张原生卡是谁（卡 22 = ITEM_LINE = CardKanako）

**定位链（机器码级）**：

1. `0x411482`：`mov ebx,[ebp+8]`（卡 id）→ `cmp ebx,0x38` → `jmp dword [ebx*4 + 0x412dac]`
   = 卡工厂 `AbilityManager__allocate_new_card` `0x411460` 的**跳表**（57 项，id 0–56）。
2. 跳表第 **22** 项 = 分支 `0x411b32`；该分支体内 `0x411b7f`：`mov dword [esi], 0x4b5618` ⇒ **id 22 的虚表 = `0x4b5618`**。
3. 虚表 `0x4b5618` 的槽位（`+0x00` 起 21 槽）里 **`+0x20` = `0x40c270`**，其余多是基类桩 ⇒
   **`0x40c270` = 卡 22 的 `on_load` 覆写**（槽语义见 [`../../card/th18/03-hooks.md`](../../card/th18/03-hooks.md) §1：
   `+0x20 on_load` ← `AbilityManager__notify_cards_on_load` `0x408ae5`）。
4. ExpHP 命名交叉印证：`CardKanako__on_load` = `0x40c270`（size 37）、`CardKanako__on_load__2` = `0x40c250`、
   `CardKanako__method_4C` = `0x40c260`、`CardKanako__operator_delete` = `0x40c2a0`。
   ⇒ 「卡 22 = 八坂神奈子」与 [`../../card/th18/08-catalog.md`](../../card/th18/08-catalog.md) 的 ITEM_LINE 行一致。

**`on_load` 全文（`0x40c270`，37 字节，逐条）**：

```asm
0x40c270  a1 10f44c00          mov  eax, ds:0x4cf410        ; eax = 自机（PLAYER_PTR）
0x40c275  c780 98790400 00006043  mov  dword [eax+0x47998], 0x43600000   ; ★ 收点线 = 224.0f
0x40c27f  a1 e0f24c00          mov  eax, ds:0x4cf2e0        ; eax = Gui（GUI_PTR）
0x40c284  85c0                 test eax, eax
0x40c286  740a                 je   0x40c292               ; Gui 不在 ⇒ 跳过（空指针安全）
0x40c288  8188 94010000 00200000  or dword [eax+0x194], 0x2000   ; ★ 置「画那条线」的位
0x40c292  33c0                 xor  eax, eax
0x40c294  c3                   ret
```

**结论**：这张卡**只做两件事**（都是开局一次）：把 `poc_height` 从 128 改成 224、把 Gui 的显示位打开。
**没有每帧 tick、没有写别的字段、没有自己画线**（`+0x2c on_tick_2` 还是基类桩）。
⇒ 「降低 PoC 高度」这件事在 TH18 里的原生形状 = **开局写一个 float 常量 + 置一个 UI 位**。

**什么时候被调**：`on_load` 广播 = `AbilityManager__notify_cards_on_load` `0x408ae5`（对卡链表逐张 `call [vt+0x20]`），
唯一调用者 `AbilityShop__sub_417880` `0x417880`（`0x41791a` 处），而 `0x417880` 的调用者里有
`GameThread__thread_start` `0x442f98` 的 `0x443725`（每关 setup）+ 商店侧的四处（改卡组后重广播）。
⇒ **每关开场 / 改卡组时**各来一次，正好覆盖「开局那条线」的时点。

## 3. 开局那条指示线（谁画的）

- **宿主**：`FUN_0043a8b0` `0x43a8b0`（2711 字节，本仓未命名，读 `edi = [0x4cf2e0]` = Gui）。它就是引擎自己的
  **前层 GUI 构建/tick**：`AnmManager__allocate_new_vm` `0x489320` → `0x407420(this=Gui->anm, vm, script)` 起脚本 →
  `AnmManager__insert_in_world_list_back` `0x488350` → 回填 `entity_pos`。
- **脚本号 = `0x39`（57）**：`0x43af50` `push 0x39` / `0x43af53 call 0x407420`。
  资产侧真身 = `front.anm` 的 `script57`（`local/th18.v1.00a/anm/front/front.anm.txt:1802`）：
  `blendMode(1)` / `layer(22)` / `scriptNew(script55)` / `scriptNew(script56)` / `sprite(sprite44)` / `pos(0,48,0)` /
  `alpha(0)` → +60 帧后 `scaleTime(10,4,1,1)` + `alphaTime(10,4,192)` → **每 8 帧 `color` 在 `(255,128,0)`/`(255,255,255)` 之间闪**
  → +198 帧 `alphaTime(10,0,0)` + `delete()`。⇒ 它是**开局出现、闪烁约 3.3 秒后自删**的一条线（不是常驻 HUD）。
- **可见性门**：`0x43af0b` `test dword [edi+0x194], 0x2000` / `je 0x43b09a`（不置位 ⇒ 整段跳过，线根本不建）。
  全 exe **唯一写这一位的点 = `0x40c288`**（上面那张原生卡）；唯一读点 = `0x43af0b`。
- **位置 = 读字段现算**（`0x43afc6`–`0x43b08a`）：

```asm
0x43afc6  a1 10f44c00      mov   eax, ds:0x4cf410       ; 自机
0x43afe1  f30f10 8098790400 movss xmm0, [eax+0x47998]     ; ★ 收点线高度（字段！不是常量）
0x43afe9  f30f58c0         addss xmm0, xmm0              ; ×2
0x43afed  f30f5c05 ec924b00 subss xmm0, [0x4b92ec]       ; − 80.0f（该 DAT = 0000a042）
0x43aff5  f30f11442438     movss [esp+0x38], xmm0        ; 局部 vec3 的 y（x/z 都是 0）
...
0x43b078  660fd681 f0050000 movq [ecx+0x5f0], xmm0       ; 写 VM 的 entity_pos（+0x5f0/+0x5f8）
```

  （`ecx` = 从 world/ui 链表里按 id 找回的那个 VM；`+0x5f0` 是 `entity_pos`，
  绘制位置 = `pos + entity_pos`，见 [`../../anm/th18/01-vm-instantiate.md`](../../anm/th18/01-vm-instantiate.md) §3a。）
- **调用时点**：`FUN_0043a8b0` 全 exe 只有两个调用点 —— `0x44422d`（`FUN_00443e60`，`GameThread__on_tick` 的
  **帧 0** 分支：`[thread+0x10] == 0`，里面先 `AnmManager` 清树 / 清场 / `Player__reset` `0x4441bf`，
  再调本函数）与 `0x444476`（`FUN_004443c0`，**帧 30** 分支）。**没有每帧调用点**（也没有函数指针注册）。

## 4. 对改造（卡 / 共用件）的三条结论

1. **位置不用我们画、也不许改资产**：引擎每建一次线都**现读字段**。改高度 = 写字段 ⇒ 线自动到新位置。
   ⚠ 但**必须**同时把 `Gui+0x194` 的 bit `0x2000` 置上（否则线不出现 —— 原生只有卡 22 会置它）。
   照抄原生那句的**空指针检查**（`0x40c284`）与 `or`（**只置不清**，`0x40c288`）。
2. **时点 = 开局之前**：唯一的读点在**帧 0 / 帧 30** 的那两次 GUI 构建里（§3），
   所以高度必须在**关卡开局那一刻就已经生效**（原生卡正是用 `on_load` 做的）。
   中途改高度**可能不会**让已经画出来的那条线动（没有每帧读点）—— 用户口径里强调「**开局**」就是这个原因。
3. **消费者只有一个真值来源**：字段本身。任何「读回收线」的代码都该读**生效值**（= 字段），
   别自己写死 128 / 224（TH18 里 128 只在 `Player__reset` 里出现，224 只在卡 22 里出现，都不是常量语义）。

## 5. 🟡 未钉死（诚实清单）

- **帧 0 那支里的顺序**：`Player__reset`（写 128，`0x4441bf`）在前、`FUN_0043a8b0`（建线读字段，`0x44422d`）在后 ——
  若 `on_load`（写 224）排在这一帧**之前**，那 224 会被 `Player__reset` 抹掉；而原生卡在实机上确实生效
  ⇒ 实际时序必然是「`Player__reset` 先、`on_load` 后」（或 `FUN_00443e60` 不是每关都跑）。
  **本仓没有实跑探针钉这一条** ⇒ 🟡。对改造的影响很小：照原生卡**在 `on_load` 里写一次**，就与原生同形。
- **帧 30 那支到底重不重建这条线**（`FUN_0043a8b0` 有 `Gui+0x4c` 链表非空/空两条路，`0x43a90d`），
  以及线的自删（script57 的 `delete()`）与重建谁先谁后 ⇒ 🟡。**不影响「开局显示正确高度」这个目标**。
- 线的**最终屏幕像素**（`pos(0,48,0)` + `entity_pos` 的和再乘 `resolutionMode(2)` 的缩放）没有逐像素对过 ⇒ 🟡。
  本文只断言「**变量部分**只来自字段」，公式 `2×poc − 80` 是一手的。

## 6. 复现命令

```bash
# ① 字段的所有访问点（disp32 字节模式）
python3 - <<'EOF'
d=open('local/th18.v1.00a/th18.exe','rb').read()
i=0
while (j:=d.find(bytes.fromhex('98790400'),i))>=0:
    print(hex(j), d[j-8:j+8].hex()); i=j+1
EOF
# ② 卡工厂跳表：第 22 项 -> 分支 -> 虚表（0x412dac + 22*4）
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x412dac --dwords 57
# ③ 原生卡 on_load 全文 / 指示线公式 / 可见性门
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x40c270 --bytes 37
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x43afe1 --bytes 24
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x4b92ec --dwords 1   # 80.0f
```

## 7. 交叉链接

- 卡 22 的目录条目（值、价格档）：[`../../card/th18/08-catalog.md`](../../card/th18/08-catalog.md) §A 行 `22 ITEM_LINE`。
- 虚表 21 槽与调用点：[`../../card/th18/03-hooks.md`](../../card/th18/03-hooks.md)。
- 道具收集 / 身价与收取高度系数：[`../../card/th18/05-shop-and-money.md`](../../card/th18/05-shop-and-money.md)、
  [`../../item/OVERVIEW.md`](../../item/OVERVIEW.md)。
- 改造侧共用件（声明 → 聚合 → 落笔 + 只读访问器）：
  [`mods/th18.v1.00a/card-expand/SDK.md`](../../../mods/th18.v1.00a/card-expand/SDK.md) 的「PoC 高度层」。
