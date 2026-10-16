# TH12 点道具身价（PIV / 最大得点）—— 载体、量纲、难度两表、两个涨入口、蓝点线上/线下两档

> **版本**：TH12 v1.00b（`local/th12.v1.00b/th12.exe`，imagebase `0x400000`）。本文地址一律带 `th12:` 前缀。
> **方法**：一手平坦反汇编（小 python 解 PE 节区 → `objdump -D -b binary -m i386 -M intel
> --adjust-vma=`0x401000`，产物 `/tmp/th12work/full.asm`）+ 常量/表按 PE 节区 VMA 换算直读原始字节
> + 全 exe 地址穷举（`th12:0x4b0c78` 共 **16 处**，逐处在指令边界复核过）。
> **可信度**：载体 / 死字段辨析 / 量纲 / 难度两表 / 写点穷举 / 蓝点两档公式 / 道具类型↔贴图（type 1..0x10）= **✅**；
> UFO 链的**事件语义**（飞碟计数、两个乘数槽的归属）= **🟡**；线下公式的**设计常量命名** = 🟡；
> 飞碟奖励的 type `0x11`/`0x12`（贴图空白）= 🟡。

## 0. 一句话结论

TH12 的「最大得点」**就是点道具身价本身**，**活载体是 `th12:0x4b0c78`**（ExpHP 名 = `zGlobals`+`0x38` `__PIV_RELATED`）。
**存储 = 显示 × 100**，HUD 上是 `trunc10(PIV / 100)`。

⚠ **ExpHP 的名字有坑**：`statics.json` 把 `th12:0x4b0cf0` 命名为 `CURRENT_PIV`、`th12:0x4b0cf4` 命名为 `MAXIMUM_PIV`，
但 `th12:0x4b0cf0` **全 exe 只有 run 初始化写一次、之后再也没有任何读**（死字段）；真正被读写的是
`__PIV_RELATED`（`th12:0x4b0c78`）。本作**没有** TH13 那种「最大得点 = 显示 ×100 的独立上限字段被复用」的情形。

全 exe 一共只有 **7 条**写点，其中**玩法中改值的只有两个**：**擦弹 `+100`（存储）**
与 **UFO 收集计数满 3 时 `+100000`（存储）**；另外 3 条是初始化与录像恢复/重置
（`th12:0x421e74`、`th12:0x43b29e`、`th12:0x43c828`）。
**miss / 灵击 / 阶段奖 / 收点道具本身都没有写点**，而且**没有任何衰减**——TH12 的「最大得点」只涨不跌。

## 1. 载体与穷举证据（✅）

### 1.1 ExpHP 名与绝对地址

| ExpHP 静态 | 字段 | 绝对地址 | 用途（本文一手结论） |
| --- | --- | --- | --- |
| `GLOBALS` `th12:0x4b0c40` | +`0x38` `__PIV_RELATED` | **`th12:0x4b0c78`** | **活载体**（点道具身价 / HUD 最大得点） |
| 同上 | +`0xb4` `MAXIMUM_PIV` | `th12:0x4b0cf4` | 每次加值后的**夹取上限**（只被读 2 次） |
| 同上 | +`0xb0` `CURRENT_PIV` | `th12:0x4b0cf0` | **死字段**（只写 1 次、0 读，见 §1.3） |

> ExpHP 在 `type-structs-own.json` 的 `zGlobals` 里给的正是 `0x38 __PIV_RELATED` / `0xb0 CURRENT_PIV` /
> `0xb4 MAXIMUM_PIV` 三个字段名——`__PIV_RELATED` 这个名字本身就是「**真正有关的那个 PIV**」的暗示，
> 与本次一手结论一致。**但名字只是词汇，结论由下面的指令与字节给出。**

### 1.2 `th12:0x4b0c78` 全部 16 处访问（穷举口径）

穷举口径：① 反汇编全文 `grep -n "4b0c78"`（覆盖 `mov eax,ds:` / `mov ds:,eax` / `fild DWORD PTR ds:` 三种
绝对寻址形态；本作**没有** `imul DWORD PTR ds:0x4b0c78` 形态）；② 在整个 exe 原始字节里搜该地址的
4 字节小端 `78 0c 4b 00`（用于抓函数指针表 / 数据引用）。**两法给出同一组 16 处，且全部落在 `.text` 的指令边界上。**

| # | 地址 | 所在函数（ExpHP） | 读/写 | 一手指令 |
| --- | --- | --- | --- | --- |
| 1 | `th12:0x421e74` | `GameThread::thread_start`（run/关卡初始化段） | **写** | `mov ds:0x4b0c78,eax` |
| 2 | `th12:0x4270c6` | `ItemManager::on_tick__body` | 读 | `mov eax,ds:0x4b0c78` |
| 3 | `th12:0x4270d8` | 同上 | **写** | `mov ds:0x4b0c78,eax`（+100000） |
| 4 | `th12:0x4270df` | 同上 | **写** | `mov DWORD PTR ds:0x4b0c78,ecx`（夹 MAX） |
| 5 | `th12:0x4391e2` | `Player::sub_4390f0` 之后的擦弹处理 `th12:0x4391c0` | 读 | `mov eax,ds:0x4b0c78` |
| 6 | `th12:0x4391f2` | 同上 | **写** | `mov ds:0x4b0c78,eax`（+100） |
| 7 | `th12:0x4391f9` | 同上 | **写** | `mov DWORD PTR ds:0x4b0c78,ecx`（夹 MAX） |
| 8 | `th12:0x41f6f0` | `Gui::on_draw_2c` | 读 | `mov ecx,DWORD PTR ds:0x4b0c78`（HUD 内联取显示值） |
| 9 | `th12:0x43b00a` | `ReplayManager::initialize` | 读 | `mov edx,DWORD PTR ds:0x4b0c78`（写录像） |
| 10 | `th12:0x43b29e` | 同上 | **写** | `mov DWORD PTR ds:0x4b0c78,edx`（读录像恢复） |
| 11 | `th12:0x43c60a` | `ReplayManager::on_draw` | 读 | `mov edx,DWORD PTR ds:0x4b0c78` |
| 12 | `th12:0x43c828` | 同上 | **写** | `mov DWORD PTR ds:0x4b0c78,edx` |
| 13 | `th12:0x44b1b7` | UFO 奖励例程 `th12:0x44af00`（color 1 分支） | 读 | `mov ecx,DWORD PTR ds:0x4b0c78` |
| 14 | `th12:0x44b2db` | 同上（color 2 分支） | 读 | 同上 |
| 15 | `th12:0x44b394` | 同上（color 3 分支） | 读 | 同上 |
| 16 | `th12:0x44b549` | 同上（color −1 分支） | 读 | 同上 |

**写点合计 7 条**，其中 3 条是初始化与录像恢复/重置（`th12:0x421e74`、`th12:0x43b29e`、`th12:0x43c828`），
真正在玩法中改值的只有 §4 的两条（各带一次夹取）。

### 1.3 为什么 `th12:0x4b0cf0`（`CURRENT_PIV`）是死字段（✅ 否定结论，已过指令边界复核）

`th12:0x4b0cf0` 全 exe **只有 2 处**引用，且都在同一段初始化里：

```
th12:0x421e0d  mov edx,[ecx*4+0x4b30e8]   ; ecx = DIFFICULTY，A[diff]
th12:0x421e14  imul edx,edx,0x64          ; ×100
th12:0x421e17  mov ds:0x4b0cf0,edx        ; ← 唯一写
th12:0x421e1d  mov eax,[ecx*4+0x4b30fc]   ; B[diff]
th12:0x421e24  fild DWORD PTR ds:0x4b0cf0 ; ← 唯一读（就地读回来算）
th12:0x421e2a  imul eax,eax,0x64          ; ×100
th12:0x421e2d  fld QWORD PTR ds:0x4a3ce8  ; 100.0
th12:0x421e33  fdiv st(1),st              ; st1 = (A×100)/100 = A
th12:0x421e59  mov ds:0x4b0cf4,eax        ; MAXIMUM_PIV = B[diff]×100
th12:0x421e5e  fxch st(1) / fstp [esp+0x14]   ; 存 A
th12:0x421e64  fmul DWORD PTR [esp+0x14]      ; 100.0 × A
th12:0x421e68  call th12:0x4931e0             ; 浮点→int（截断）
th12:0x421e74  mov ds:0x4b0c78,eax        ; ★ PIV = A[diff]×100（与 0x4b0cf0 同值）
```

⇒ `th12:0x4b0c78` 与 `th12:0x4b0cf0` 在初始化时**恒等**，但此后 6 条写点全部写 `th12:0x4b0c78`，
`th12:0x4b0cf0` 再无人问津。**引用它的字段名会直接把下游结论带错**（会以为夹取上限是走 `CURRENT_PIV`）。

> 复核（`METHOD.md` 硬规矩 4）：这两处都逐条打开到指令边界确认过是真指令（不是立即数/操作数误命中）；
> 原始字节里搜 `f0 0c 4b 00` 也**只有** `th12:0x421e19` / `th12:0x421e26` 两个文件偏移，与指令边界吻合。

## 2. 量纲：显示 ↔ 存储（✅ 两侧各有独立指令证据）

### 2.1 PIV：存储 = 显示 × 100

取值函数 `th12:0x421880`（ExpHP 名 `Globals::sub_421880`，约定 **eax = &GLOBALS**，`ret` 无栈参）：

```
th12:0x421880  mov ecx,[eax+0x38]      ; = PIV（GLOBALS+0x38）
th12:0x421883  mov eax,0x51eb851f
th12:0x421888  imul ecx                ; 魔数 0x51eb851f + sar edx,5 ⇒ ecx/100
th12:0x42188a  sar edx,5
th12:0x42188d  ..0x421892 符号修正
th12:0x421894  mov eax,ecx  (cdq / idiv esi=10)
th12:0x42189d  idiv esi
th12:0x4218a2  sub eax,edx             ; eax = ecx − (ecx % 10) = trunc10(ecx)
th12:0x4218a4  ret
```

⇒ **返回值 = `trunc10(PIV / 100)`**，即「最大得点」的显示值，且**永远是 10 的倍数**。

同一个算式在 HUD 绘制处还有一份**内联副本**（`Gui::on_draw_2c`）：

```
th12:0x41f6f0  mov ecx,ds:0x4b0c78    ; PIV（存储）
th12:0x41f706  mov eax,0x51eb851f ; imul ecx ; sar edx,5 ; 符号修正
th12:0x41f71a  idiv esi(=10)
th12:0x41f71c  sub ecx,edx
th12:0x41f71e  call th12:0x401610     ; AsciiManager 画数字
```

⇒ HUD 上那个数 = `trunc10(PIV/100)`，**两处实现（函数与内联）逐指令同形**，互为独立证据。

### 2.2 分数：存储 = 显示 ÷ 10（**与 PIV 相反，别混**）

`Globals::cap_score` `th12:0x40e730`（`ecx = &GLOBALS`，栈上 1 个参数 = **显示值**）：

```
th12:0x40e730  mov eax,0x66666667 ; imul [esp+4] ; sar edx,2 ; 符号修正   ; = 参数/10
th12:0x40e743  add DWORD PTR [ecx+0x4],eax        ; CURRENT_SCORE += 参数/10
th12:0x40e746  cmp [ecx+0x4],0x3b9aca00 ; jl ; mov [ecx+0x4],0x3b9ac9ff   ; 夹 999,999,999
```

而 HUD 画分数时**再乘 10**（`Gui::on_draw_2c`，`th12:0x41f514`）：

```
th12:0x41f4e7  mov edx,[esi+0x6cdc]    ; GUI 里滚动的显示计数（与 CURRENT_SCORE 同单位）
th12:0x41f514  lea ecx,[eax+eax*4]     ; eax × 5
th12:0x41f517  lea ecx,[edx+ecx*2]     ; × 2 ⇒ × 10
th12:0x41f51e  call th12:0x401610      ; 画出来
```

⇒ **分数：显示 = 存储 × 10**；PIV：**显示 = 存储 ÷ 100**。两者方向相反，跨作搬运时最容易在这里翻车。

### 2.3 蓝点的飘分数字 = 显示值

飘分弹窗 `th12:0x43e250(pos_ptr, color)`，**要显示的数字放在 eax**，函数内部用 ÷10 循环把它逐位拆成十进制字节
（`th12:0x43e290` 的 `imul ebx` + `mov BYTE PTR [eax+esi*1],bl`，位数写在弹窗对象 `+0x39`）。
⇒ 弹窗里那个数就是**显示值**，与 `cap_score` 收到的参数同源（§5 的 `esi`）。

## 3. 初值与上下限：按难度的两张 5 元表（✅ 一手 + 原始字节直读）

初始化在 `th12:0x421e0d`–`th12:0x421e74`（`ecx = GLOBALS+0x68 DIFFICULTY`），索引规则：

```
th12:0x421d7d  cmp DWORD PTR ds:0x4b0cb0,0x7    ; CUR_STAGE_NUM == 7（Extra）
th12:0x421d86  mov ecx,0x4            ; ⇒ DIFFICULTY 被强写成 4（并写回 th12:0x4b0ca8）
th12:0x421d93  mov ecx,DWORD PTR ds:0x4b0ca8   ; 否则从 GLOBALS+0x68 取难度索引
```

| 难度索引 | `A` = PIV 初值 **（显示）** | 表地址 | `B` = MAXIMUM_PIV **（显示）** | 表地址 |
| --- | --- | --- | --- | --- |
| 0（Easy） | 5000 | `th12:0x4b30e8` | 100000 | `th12:0x4b30fc` |
| 1（Normal） | 10000 | `th12:0x4b30ec` | 200000 | `th12:0x4b3100` |
| 2（Hard） | 15000 | `th12:0x4b30f0` | 300000 | `th12:0x4b3104` |
| 3（Lunatic） | 20000 | `th12:0x4b30f4` | 400000 | `th12:0x4b3108` |
| 4（Extra） | 20000 | `th12:0x4b30f8` | 500000 | `th12:0x4b310c` |

- 两表**各恰好 5 项**：表 A 之后紧跟表 B（`0x4b30fc − 0x4b30e8 = 0x14 = 5×4`），表 B 之后是 `0x49cd0c`（指针）+ `0x00000000`。
- 原始字节（`.data`，VA→文件偏移换算后直读）：`th12:0x4b30e8` = `88130000 10270000 983a0000 204e0000 204e0000 | a0860100 400d0300 e0930400 801a0600 20a10700`。
- **存储值是这两张表 ×100**（`th12:0x421e14` 与 `th12:0x421e2a` 的 `imul eax,eax,0x64`）：
  Easy 起始 PIV = `500000`、上限 = `10000000`；Extra 起始 = `2000000`、上限 = `50000000`（均为存储）。
- ⇒ **Extra 的初值 = Lunatic，但上限高一级**（TH13 的 Extra 是「初值 20000 / 上限 500000」，同形）。
- ⚠ 本段还顺带把 `CURRENT_BOMB_FRAGMENTS`(`+0x64`)、`COLLECTED_UFOS[0..2]`(`+0xc..+0x14`)、
  `__UFO_COUNT__COPY_18/1C`(`+0x18/+0x1c`) 清零（`th12:0x421e35`–`th12:0x421e53`）——**即每局/每关重置飞碟链**。

## 4. 涨 / 跌入口穷举（✅ 写点；🟡 事件身份）

| 入口 | 写点 | 一手效果 | 可信度 |
| --- | --- | --- | --- |
| run / 关卡初始化 | `th12:0x421e74` | `PIV = A[diff] × 100`（并写 `MAXIMUM_PIV = B[diff] × 100` 于 `th12:0x421e59`） | ✅ |
| **擦弹** | `th12:0x4391f2` + `th12:0x4391f9` | `PIV += 0x64 (100 存储 = +1 显示)`，再夹到 `MAXIMUM_PIV` | ✅（擦弹计数器即 `GLOBALS+0x9c`） |
| **UFO 收集计数满 3 时再收飞碟道具** | `th12:0x4270d8` + `th12:0x4270df` | `PIV += 0x186a0 (100000 存储 = +1000 显示)`，再夹 `MAXIMUM_PIV`；同帧飘窗 `1000` | ✅ 指令 / 🟡 事件 |
| 录像读档（播放时恢复现场） | `th12:0x43b29e`、`th12:0x43c828` | 从录像数据恢复 PIV | ✅ |
| 上限夹取 | 只读 `th12:0x4b0cf4`（读点 `th12:0x4270cb`、`th12:0x4391e7`） | 加法完成后 `if (PIV > MAX) PIV = MAX` | ✅ |
| **miss（被弹）** | **无写点** | —— | ✅ 否定结论 |
| **灵击（bomb）** | **无写点** | —— | ✅ 否定结论 |
| **阶段奖 / 通关结算** | **无写点**（`th12:0x420aec`、`th12:0x420c31` 只**读**显示值参与加分，门是 `th12:0x420acd` 的 `cmp CUR_STAGE_NUM,6` 与 `th12:0x420c16` 的 `cmp …,7`） | —— | ✅ 否定结论 |
| **收点道具本身** | **无写点**（收蓝点只改 `CURRENT_SCORE`） | —— | ✅ 否定结论 |
| **衰减（任何形式的跌）** | **不存在**：16 处访问里没有任何负增量写 | —— | ✅ 否定结论 |

### 4.1 擦弹入口的完整算式（`th12:0x4391c0`）

```
th12:0x4391c3  cmp DWORD PTR ds:0x4b0cdc,0x5f5e0ff   ; GRAZE 上限 99,999,999
th12:0x4391da  jge 跳过
th12:0x4391dc  add DWORD PTR ds:0x4b0cdc,ebx(=1)     ; ★ GRAZE++
th12:0x4391e2  mov eax,ds:0x4b0c78                   ; PIV
th12:0x4391e7  mov ecx,DWORD PTR ds:0x4b0cf4         ; MAXIMUM_PIV
th12:0x4391ed  add eax,0x64                          ; ★ +100 存储 = +1 显示
th12:0x4391f2  mov ds:0x4b0c78,eax
th12:0x4391f7  jle 0x4391ff
th12:0x4391f9  mov DWORD PTR ds:0x4b0c78,ecx         ; ★ 夹 MAX
th12:0x4391ff  ...（rank 抖动量 / 擦弹特效 / 音效）
```

⇒ **一次擦弹 = 最大得点 +1（显示）**，与 TH10/TH11/TH13 的「擦弹喂身价」同形。调用者 8 处
（`th12:0x409c78` / `th12:0x409f65` 在 `Bullet`/`BulletManager` 内、`th12:0x414043`、`th12:0x41b579` /
`th12:0x41b5da`、`th12:0x429a71`、`th12:0x42b084`、`th12:0x42cc13`），**身份 = 🟡**（弹/敌/激光等碰触判定的分支）。

### 4.2 UFO 入口的完整算式（`th12:0x4270b0`，`ItemManager::on_tick__body` 内的子函数）

```
th12:0x4270b5  mov ebp,ds:0x4b0c58        ; GLOBALS+0x18（飞碟收集计数）
th12:0x4270bb  cmp ebp,3
th12:0x4270c4  jl 0x427105                ; 不足 3 → 走「只记数」那一支
th12:0x4270c6  mov eax,ds:0x4b0c78        ; PIV
th12:0x4270cb  mov ecx,DWORD PTR ds:0x4b0cf4
th12:0x4270d1  add eax,0x186a0            ; ★ +100000 存储 = +1000 显示
th12:0x4270d8  mov ds:0x4b0c78,eax
th12:0x4270dd  jle 0x4270e5
th12:0x4270df  mov DWORD PTR ds:0x4b0c78,ecx   ; ★ 夹 MAX
th12:0x4270e5  push 0xff80ff80 ; push &item.pos ; mov eax,0x3e8(1000)
th12:0x4270f6  call th12:0x43e250         ; 飘窗「1000」
th12:0x4270fb  xor eax,eax ; ret           ; ★ 提前返回：不再走登记流程
```

调用者只有 3 处，都是「收到飞碟道具」分支：`th12:0x426c3f`（类型 `0x0a`、传 color 0）、
`th12:0x426c4d`（类型 `0x0b`、color 1）、`th12:0x426c5e`（类型 `0x0c`、color 2）。
「不足 3」那一支 `th12:0x427105` 才做登记：`th12:0x42714d` `lea edi,[esi+1]`（esi = color 0/1/2）、
`th12:0x427150` `mov esi,0x4b0c4c`（= `&COLLECTED_UFOS[0]`）、`th12:0x427155 call th12:0x422e80`
**把这个颜色追加进 `COLLECTED_UFOS` 三格数组**（`th12:0x422e80` 逐格填 `[esi]`/`[esi+4]`/`[esi+8]`，
并把 `GLOBALS+0x18`/`+0x1c` 两个计数写成 1/2/3）。

## 5. 蓝点（点道具）被收时的分值公式（✅）

### 5.1 先钉「哪个 type 是蓝点」（✅ 三重证据）

`ItemManager::spawn_item` `th12:0x4273f0` 用 `type + 0xa5` 当 **anm 脚本号**，纹理取
`[BULLET_MANAGER+0x4debdc]`（= `bullet_anm`）：

```
th12:0x42759b  mov edx,ds:0x4b43c8    ; BULLET_MANAGER_PTR
th12:0x4275a1  mov [esi+0x9b4],ebx    ; 存 type
th12:0x4275a7  mov edi,[edx+0x4debdc] ; bullet_anm
th12:0x4275b2  add ebx,0xa5           ; 脚本号 = type + 0xa5
th12:0x4275ca  call th12:0x454d10     ; AnmVm::set_script(bullet_anm, vm, type+0xa5)
```

「判定」列 = 该 type 在状态机里的**收得处理器**（§5.2 的分派表 `th12:0x426f7c`）所调用的 `Globals::collect_*`，
外加贴图主色；两路互相独立。

| item type | 脚本 | sprite | 贴图主色（去白后均值，`etama8.png` 实测） | 收得处理器 → 判定 |
| --- | --- | --- | --- | --- |
| 1 | 166 | 426 | (205,44,44) 红 | `th12:0x426a64` → `th12:0x426ff0`：小 power（满 power 时改给分） |
| **2** | **167** | **427** | **(73,76,168) 蓝** | **`th12:0x426b10`：★ 蓝点（点道具）** |
| 3 | 168 | 428 | (216,76,76) 红，233px | `th12:0x426a70`：大 power |
| 4 | 169 | 429 | (176,68,167) 紫红 | `th12:0x426bad` → `collect_life_fragments` |
| 5 | 170 | 430 | (68,176,77) 绿 | `th12:0x426bf2` → `collect_bomb_fragment` |
| 6 | 171 | 431 | (166,44,167) 紫红，170px | `th12:0x426bd9` → `collect_extend`（命） |
| 7 | 172 | 432 | (53,173,54) 绿，170px | `th12:0x426c0b` → `collect_bomb`（灵击） |
| 8 | 173 | 433 | (206,178,79) 金，221px | `th12:0x426997`：满 power 型大 power |
| 9 | 174 | 434 | (201,24,24) 红，57px | `th12:0x426c24`：固定 10 分小道具 |
| 0x0a..0x0c | 175..177 | 435..437 | 蓝 (91,94,184) / 红 / 紫红，41–75px | `th12:0x426c3f`/`0x426c4d`/`0x426c5e` → `th12:0x4270b0`：飞碟道具 |
| 0x0d..0x0f | 178..180 | 438..440 | 绿 / 紫红 / 绿 | 与 `0x0a..0x0c` **同一组处理器**（分派表重复项） |
| 0x10 | 181 | 441 | (170,144,53)，「P」形 | `th12:0x426c6f`：飞碟奖励道具 |
| 0x11 | 182 | 442 | (174,172,105)，圆团 | 同上（另起一行的贴图） |
| 0x12 | 183 | 443 | **该格全透明（空贴图）** | 同上（🟡 疑为遗留未用，见 §8） |

证据链：① 引擎侧 `type + 0xa5` → 脚本号（指令，见上）；② `anm/bullet/bullet.anm.txt` 里
`script166`–`script183` 正好 18 支道具脚本（`script165` 是「出现/消失」的通用动画），
脚本 166→`sprite426`、167→`sprite427`…（数据）；③ `anm/bullet/bullet/etama8.png` 逐格取色
（sprite426..443 **只**在 `bullet.anm` entry5 里定义，entry5 的纹理正是 `etama8.png`）。
⇒ **type 1 是红 power、type 2 是蓝点**，与领域常识一致（`METHOD.md` 的常识关）。

### 5.2 逐指令算式

蓝点在道具状态机的类型分派里走 `th12:0x426b10`（分派表 `th12:0x426f7c`，索引 `[item+0x9b4] − 1`，
即 `type 2` → 表项 1）：

```
th12:0x42616a  mov eax,[edi+0x9b4]      ; edi = zItem（ItemManager+0x14 起的 normal_items 数组）
th12:0x426174  dec eax ; cmp eax,0x11 ; ja 0x426da4
th12:0x426182  jmp DWORD PTR [eax*4+0x426f7c]     ; type 1→0x426a64, type 2→0x426b10, …
```

**判据（线上 / 线下）**：

```
th12:0x426b10  mov edx,ds:0x4b4514        ; PLAYER_PTR
th12:0x426b16  fld DWORD PTR [edx+0x980]  ; 自机 y（zPlayer.position = 0x97c/0x980/0x984）
th12:0x426b1c  fstp DWORD PTR [esp+0xc]
th12:0x426b20  fld DWORD PTR ds:0x4a3db8  ; 128.0f ← 道具回收线
th12:0x426b26  fcomp DWORD PTR [esp+0xc]
th12:0x426b2a  fnstsw ax ; test ah,1 ; je 0x426b9e
```

`test ah,1` 测的是 C0，`je`（C0==0）在 `128.0 ≥ y` 时成立 ⇒ **自机 y ≤ 128（线上）走「满值」支**，
**y > 128（线下）走「按高度打折」支**。

**线上（`y ≤ 128.0`，`th12:0x426b9e`）**：

```
th12:0x426b9e  mov eax,0x4b0c40 ; call th12:0x421880   ; eax = trunc10(PIV/100) = 显示值
th12:0x426ba3  jmp 0x426d65
th12:0x426d65  mov ecx,eax ; mov eax,0x66666667 ; imul ecx ; sar edx,2 ; 符号修正  ; = eax/10
th12:0x426d78  lea esi,[eax+eax*4] ; add esi,esi      ; esi = (eax/10)×10 = trunc10(显示值)
th12:0x426d7d  test esi,esi ; jg ; mov esi,0xa        ; 下限 10
th12:0x426d86  push 0xffffff00 ; lea eax,[edi+0x96c] ; push eax ; mov eax,esi
th12:0x426d94  call th12:0x43e250                     ; ★ 飘窗 = esi
th12:0x426d99  push esi ; mov ecx,0x4b0c40
th12:0x426d9f  call th12:0x40e730                     ; ★ CURRENT_SCORE += esi/10
```

⇒ **线上蓝点：显示得分 = `trunc10(PIV / 100)`（就是 HUD 上那个「最大得点」，无任何乘数）；
入账分数 = 该值 / 10（存储），即屏幕分数 +该值。** 因 `trunc10` 幂等，`esi` 直接就是 HUD 值。

**线下（`y > 128.0`，`th12:0x426b3a`）**：

```
th12:0x426b31  cmp DWORD PTR [edi+0x9b0],3 ; je 0x426b9e   ; 某状态下强制走满值支
th12:0x426b3a  mov eax,0x4b0c40 ; call th12:0x421880       ; eax = PIV_disp
th12:0x426b48  lea eax,[eax+eax*2] ; cdq ; and edx,3 ; add eax,edx ; mov esi,eax ; sar esi,2
th12:0x426b53                                              ; ★ esi0 = 3 × PIV_disp / 4
th12:0x426b56  call th12:0x4931e0                          ; st0 = 自机 y（浮点）→ eax = (int)y
th12:0x426b5b  mov ecx,eax ; add ecx,0xffffff80            ; ★ t = (int)y − 128
th12:0x426b60  imul ecx,esi                                ; ecx = t × esi0
th12:0x426b63  mov eax,0x91a2b3c5 ; imul ecx ; add edx,ecx ; sar edx,8 ; 符号修正
th12:0x426b76  sub esi,eax                                 ; ★ esi = esi0 − (t×esi0)/450
th12:0x426b78  ..0x426b8c  魔数 0x66666667 ⇒ trunc10
th12:0x426b8e  test esi,esi ; jg ; mov esi,0xa              ; 下限 10
th12:0x426b97  push 0xffffffff ; jmp 0x426d8b               ; 同一套飘窗 + cap_score
```

> **魔数换算（我第一遍读错了，记在这里防复现）**：`mov eax,0x91a2b3c5; imul ecx` 是**有符号**乘法，
> 立即数按 int32 解释是 **−1851034171**，于是 `((M×x)>>32 + x) >> 8 = x×(1 − 0.431111)/256 = x/450`。
> 若误按无符号算会得到「除 163.17」这个**非整数**的怪常量（且穷举不出任何整数除数）——正是这个矛盾把符号问题逼了出来。
> **已验证**（本文写作时的独立复核）：按 int32 有符号乘 + `sar` 语义实现这段指令，在
> `x ∈ [0, 2×10⁹)` 上与 `trunc(x / 450)` 逐值比对，**0 处不符**。

⇒ **线下蓝点：`显示得分 = trunc10( 0.75 × PIV_disp × (1 − ((int)y − 128)/450) )`，下限 10。**
即线上/线下**不是两个加数，而是同一项的两档**：线下从满值的 **0.75** 起随高度线性衰减，
到 `y = 128 + 450 = 578`（在 640×480 画面之外）才衰减到 0；自机 y 最深处（≈448）仍有约 **21.7%**。
结构上与 TH10（`engine/faith/th10/01-faith-and-points.md` §蓝点：线上 = 信仰、线下 = 高度公式）**同形**，
但常量一个都不能搬（TH10 的线在 `144.0f`）。

## 6. 有没有「第二项」（基础值之外的额外加数）？

**分两层答，别混：**

1. **全局载体层：没有第二项。** 「最大得点」是**一个 int**（`th12:0x4b0c78`），它的显示值是
   `trunc10(PIV/100)`。`th12:0x4b0cf4`（`MAXIMUM_PIV`）是**夹取上限**、不是「在基准之上再加一份」的额外字段；
   `th12:0x4b0cf0` 是死字段。⇒ 与 `engine/faith/OVERVIEW.md` 的跨版本断言一致：
   「PIV 与 HUD 上的最大得点是同一个量……不是『在别的基准之上再加一份』的额外项」。
   **本作没有** TH11 那种「乘 (交信度 + 擦弹倍率)」的第二因子（线上蓝点纯等于 PIV）。

2. **单个飘分对象层：飞碟奖励点道具（type `0x10`/`0x11`/`0x12`）**确实是**两项之和**，
   每颗道具自带两个乘数槽 `[item+0x9cc]`、`[item+0x9d0]` 与一个浮点倍率 `[item+0x9d4]`：

```
; ① 飞碟奖励例程把「吸收计数」塞进这颗道具的乘数槽（th12:0x44af00 内）
th12:0x44b112  mov eax,[esi+0x48] ; th12:0x44b115  mov [edi+0x9cc],eax   ; 槽 A = 飞碟的 <某类道具吸收计数>
th12:0x44b11b  mov ecx,[esi+0x4c] ; th12:0x44b11e  mov [edi+0x9d0],ecx   ; 槽 B = 飞碟的 <另一类吸收计数>
th12:0x44b124  mov edx,[esi+0x30] ; th12:0x44b127  mov [edi+0x9c8],edx   ; 选择子 = 飞碟颜色(0..3)

; ② 收这颗道具时按 [item+0x9c8] 分派（jt th12:0x426fdc，索引 [0x9c8]+1）
th12:0x426c83  mov eax,[edi+0x9c8] ; inc eax ; cmp eax,4 ; ja 跳过
th12:0x426c93  jmp DWORD PTR [eax*4+0x426fdc]
;   [-1]→0x426c9a  [0]→0x426da4(不给分)  [1]→0x426cd2  [2]→0x426d27  [3]→0x426d27

; 两项式（[0x9c8]==1，th12:0x426cd2）
th12:0x426cf0  mov eax,0x4b0c40 ; call th12:0x421880        ; eax = PIV_disp
th12:0x426cfa  mov ecx,[edi+0x9cc] ; mov ebx,eax ; imul ecx,ebx
th12:0x426d09  fild [esp+0xc] ; fmul [edi+0x9d4]           ; 第 1 项 × 浮点倍率
th12:0x426d13  call th12:0x4931e0                           ; 截断
th12:0x426d18  mov edx,[edi+0x9d0] ; imul edx,ebx ; add esi,edx   ; ★ 第 2 项
th12:0x426d25  jmp 0x426d7d                                  ; 同一套 trunc10 / 下限10 / 飘窗 / cap_score
```

⇒ **type `0x10`/`0x11`/`0x12` 的显示得分 = `int(PIV_disp × 槽A × 倍率) + PIV_disp × 槽B`**（`th12:0x426cd2`），
另一路（`th12:0x426c9a` / `th12:0x426d27`，即选择子 −1/2/3）= **单项** `trunc10(int(PIV_disp × 槽B × 倍率))`。
**这就是本作唯一存在的「基础值 + 额外加数」结构**，而且它只在**飞碟奖励掉出来的那三种点道具**上成立；
**普通蓝点（type 2，§5）没有第二项**。若用户要写「总最大得点公式」，正确的口径是
**「HUD 那个数 = `trunc10(PIV/100)`，单值；只有飞碟奖励道具的飘分才额外乘吸收计数」**。
（另注，为什么两条链**互不干扰**：`[item+0x9c8]` 这个选择子只有飞碟奖励例程写（`th12:0x44b127`，值 = 飞碟颜色），
而**只有 type `0x10`/`0x11`/`0x12` 的收得处理器去读它**（`th12:0x426c6f` 那一支）；
type 2（普通蓝点）走的是 `th12:0x426b10`，**根本不看** `0x9c8`/`0x9cc`/`0x9d0`/`0x9d4` 这四个槽。
所以池子回收留下的残值也不会串味。）

### 6.1 两个乘数槽从哪来（🟡 事件语义）

飞碟在场时，**每收一个道具都会喂飞碟的计数器**：`ItemManager::on_tick__body` 在收道具的公共出口调
`th12:0x44aca0`，参数 `eax = [item+0x9b4]`（道具类型），函数内用**字节表** `th12:0x44aeb0` 分派：

```
th12:0x44acb1  cmp eax,0xb ; ja …
th12:0x44acba  movzx eax,BYTE PTR [eax+0x44aeb0]   ; 表 = [0,1,3,3,3,3,3,3,3,2,2,2]（按 type−1 索引）
th12:0x44acc1  jmp DWORD PTR [eax*4+0x44aea0]
;  type 1 (power)  → 0x44acc8: 颜色==−1 ? [ufo+0x4c]++ : [ufo+0x48]++
;  type 2 (蓝点)   → 0x44ad0c: 颜色==−1 ? [ufo+0x48]++ : [ufo+0x4c]++
;  type 3..9       → 0x44ad63（不计数）
;  type 0xa..0xc   → 0x44ad39
```

⇒ **蓝点喂 `[ufo+0x48]`、power 道具喂 `[ufo+0x4c]`**（在飞碟颜色 ≠ −1 时），
而奖励例程把 `[ufo+0x48]` 写进槽 A、`[ufo+0x4c]` 写进槽 B ⇒ 两项式的物理含义是
**「（吸收的蓝点数 × 浮点倍率）+ 吸收的 power 道具数」倍乘 PIV**。**🟡**：字节表与两条 `++` 是 ✅ 一手，
但「哪一个槽是点、哪一个是 power」的**命名**依赖 `th12:0x44acc8` / `th12:0x44ad0c` 与 type 的配对，
建议实跑复核（收 N 颗蓝点 + M 颗 power 后看飞碟奖励飘分值）。

各颜色的浮点倍率 `[item+0x9d4]`（`th12:0x44b141` / `0x44b245` / `0x44b337` / `0x44b416`）：

| 飞碟颜色 | 倍率写法 | 一手 |
| --- | --- | --- |
| 0 | ——（不生成计分道具，`th12:0x44b584` 直接返回） | ✅ |
| 1 | `power >= MAX_POWER ? 2.0f : 1.0f` | ✅ |
| 2 | 按 `[ufo+0x50]` 三段：`8.0` / `6.0` / `6.0 + 8.0×[ufo+0x50]`（常量 `0x4a4078`/`0x4a4088`/`0x4a43c0`/`0x4a4108`/`0x4a3d00`） | 🟡 分段阈值方向 |
| 3 | `2.0f`（常量 `0x4a4014`） | ✅ |

## 7. 与飞碟链的接口（供卡侧引用）

- 飞碟是**以敌人对象实现**的：`Ufo::on_tick` `th12:0x44a2d0` 里
  `th12:0x44a8a0` 用 `call th12:0x412990`（ECL 生成敌人）建出飞碟本体，并把
  `[enemy+0x103c] = 0x44a890`（**`zEnemy.on_death_callback`**）挂上死亡回调，
  回调 → `th12:0x44af00`（奖励例程）。
- 召唤条件：`th12:0x44a4b2` `cmp ds:0x4b0c5c,3` + `th12:0x44a4bb` `cmp ds:0x4b0c60,0`（`jl` 跳过）
  → `eax = ds:0x4b0c64`（颜色）→ `call th12:0x44a8a0`。
- 飞碟本体位置 = `[[UFO_PTR]+0x34] + 0x1074`（`UFO_PTR` = `th12:0x4b4534`；`[ufo+0x34]` 是那个敌机，
  敌机 `zEnemyData` 在 `+0x1040`，其 `final_pos` 在 `+0x34` ⇒ `+0x1074`）。
- 飞碟的召唤参数表：`[power / POWER_PER_LEVEL]` 索引 `th12:0x4b33f0` = `{1600,1900,2100,2300,2500}`
  （`th12:0x44a8f3` 读入后写进传给 `th12:0x412990` 的参数块 `+0x14`；**疑为生成敌机的 `score_reward`，🟡**），
  颜色索引（`color−1`，`color==−1` 时取 3）索引 `th12:0x4b33e0` = 4 条 ECL 路径指针（`0x4a20fc`/`0x4a20f0`/`0x4a20e4`/`0x4a20d8`）。（🟡 路径内容未解）

## 8. 未解 / 开放

- 🟡 §4.1 那 8 个擦弹调用点各自的事件身份（弹 / 敌 / 激光 / 道具），门已列全，身份未逐一钉。
- 🟡 §4 写点 3 的**事件语义**：`[0x4b0c58]` 与 `[0x4b0c5c]` 这对计数（`zGlobals+0x18/+0x1c`，
  ExpHP 叫 `__UFO_COUNT__COPY_18/1C`）的确切含义：两者总是**被一起清零**
  （`th12:0x421e4d`/`th12:0x421e53`、`th12:0x44a33e`/`th12:0x44a344`、以及录像重置），
  但只有 `[0x4b0c58]` 出现在 PIV 加法门上，`[0x4b0c5c]` 出现在召唤门上；全 exe 找不到对它们的
  `inc`/`add` 绝对写点（只有 `th12:0x422e80` 通过 `+0xc`/`+0x10` 间接写），**追加点未钉**。
- ❓ 计分道具（type 9）走 `th12:0x426c24`：`fld ds:0x4a4210`（0.1f）+ `call th12:0x427ca0` + `push 0xa` →
  `th12:0x426d9a` `cap_score(10)`，与「固定 10 分小道具」吻合（显示 +10）。但
  ① `th12:0x427ca0` **身份未解**（它读 `[ebp+0x42c]`/`[ebp+0x638]`，且被当子程序 `call`——疑为真实函数边界
  与 ExpHP 的切法不同，**未钉**）；② **它是什么东西的掉落未追**。
- 🟡 飞碟奖励的 type `0x11`/`0x12`（脚本 182/183，sprite 442/443）在 `etama8.png` 里：
  442 是一个圆团、**443 整格全透明（空贴图）**。而 `th12:0x44b0c3` 确实会 `push 0x12` 生成它 ⇒
  要么这两个 type 是**遗留未用**（ZUN 半成品），要么贴图在运行时由别的纹理补 —— **实跑可见**（看飞碟奖励时
  有没有「看不见的点道具」）。
- ⚠ **一处真实不一致（登记，未下结论）**：power 道具在满 power 时（`th12:0x426ff0` 的
  `th12:0x427002` 支）执行 `CURRENT_SCORE += 1`（+1 存储 = **显示 +10**），而飘窗值给的是
  `eax = 0x64`（**显示 100**，`th12:0x427027`）。按 §2.2 的量纲（显示 = 存储 ×10）两者**差 10 倍**，
  与蓝点链（飘窗值与显示增量恒等）不自洽。可能是原版就有的笔误，也可能我漏了一条读法 —— **标 ❓，实跑可一秒验伪**
  （满 power 收一颗红 power 道具，看屏幕分数涨 10 还是 100）。
- ❓ 分数超越（额外残机）阈值表：`Gui::on_draw_2c` 段内的分数滚动显示 `th12:0x420e70`（ExpHP 未单列名字）用 `th12:0x420f52` `cmp displayed,[extend_count*4+0x4aeeb4]`
  （Extra 难度走 `th12:0x4af0f8`），而 `th12:0x4aeeb4` 的**首项 = 1000000000**，
  大于计分上限 `999999999`（`th12:0x40e74f`）⇒ **v1.00b 的分数超越实际上触发不了**（残机改为靠「命之欠片」）。
  这两张表的**结构**（首项之外紧跟调色板/函数指针）像是与相邻数组共用，未逐项解 —— 与本文主题（PIV）无关，仅登记。

## 9. 假阳性登记（`METHOD.md` 硬规矩 4）

- **`th12:0x4ce8d0` / `th12:0x4ce8d4` / `th12:0x4ce8d8`**：按「同一段代码里连续读 3 个 dword 再写进对象」
  的形状，一度被当成「道具的三个分值参数」。**实际是弹幕 `ex_steer` 插值的零向量**：
  写点是 `th12:0x40b32f`–`th12:0x40b38f`，那里的 `ebp` 是 **`zBullet`**
  （`[ebp+0x4bc]` = `zBullet.pos`、`[ebp+0x8d4]` = `ex_steer_timer`、`[ebp+0x9a8]` = `__ex_steer_i`），
  三个 dword 落在这个 `zInterpFloat3` 里，与 zItem 的 `0x9c8`/`0x9cc`/`0x9d0` **偏移巧合、对象不同**。
  **教训**：`0x9xx` 这一带 `zBullet` 与 `zItem` 偏移撞车，凡在此区间下结论必须先确认基址寄存器是谁。
- `/tmp/_text.bin` 是本仓多会话共用的老约定路径，**会被并发会话覆写**（本次就撞上：第一次 objdump 出来的
  是别作的代码，地址整体对不上）。本文改用私有 `/tmp/th12work/text.bin`。**建议后续一律带版本名。**

## 10. 复现步骤

```bash
# 1) 解 PE 节区（.text VA=0x401000 / raw=0x400）
python3 - <<'PY'
import struct
d=open('local/th12.v1.00b/th12.exe','rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]
n=struct.unpack_from('<H',d,pe+6)[0]; opt=pe+24
so=opt+struct.unpack_from('<H',d,pe+20)[0]
for i in range(n):
    o=so+i*40; name=d[o:o+8].rstrip(b'\0').decode()
    vsz,va,rsz,ra=struct.unpack_from('<IIII',d,o+8)
    if name=='.text': open('/tmp/th12work/text.bin','wb').write(d[ra:ra+rsz])
PY
# 2) 平坦反汇编（注意 --adjust-vma = 0x401000，不是 0x1000）
objdump -D -b binary -m i386 -M intel --adjust-vma=0x401000 /tmp/th12work/text.bin > /tmp/th12work/full.asm
# 3) PIV 穷举
grep -n "4b0c78" /tmp/th12work/full.asm        # 16 处
python3 -c "d=open('local/th12.v1.00b/th12.exe','rb').read(); print([hex(i) for i in range(len(d)) if d.startswith(bytes.fromhex('780c4b00'),i)])"
```

## 11. 与既有文档的关系

- 跨版本断言（「PIV = 最大得点，不是额外项」「量纲逐作不同」）在 `engine/faith/OVERVIEW.md`；
  **本作新增的事实是**：① ExpHP 的 `CURRENT_PIV` 是死字段、活载体是 `__PIV_RELATED`；
  ② 量纲：PIV 是 **存储 ÷ 100 取 10 的倍数**得显示（与 TH11 的「显示 = 存储 ÷ 100」同向，
  见 `engine/faith/th11/01-faith-and-points.md`），而**分数**是**存储 × 10**（与 PIV 反向，同一份文档里两把尺）；
  ③ 存在**唯一的**两项式结构，但它挂在飞碟奖励道具上而不是全局量上。
- 线下折扣分支的**结构**与 `engine/faith/th10/01-faith-and-points.md` 同形（那边线在 `144.0f`）；
  TH13 那篇留的 ❓「线上/线下是否分档」在本作给出 ✅ 答案（分档，且线下是**乘性折扣**不是加数）。

## 9. 按 mod 的「额外机制」口径重述 + 飞碟奖励道具的高度项（否定结论，2026-09-18）

参照系：`G = G_0 × coeff_0 + Σ ΔG_i × coeff_i`（`G_0 = G_18`）。本节的一手 = 平坦反汇编复核
`th12:0x426b10`（蓝点）／`th12:0x426c6f`（飞碟奖励道具）／`th12:0x426d50`–`th12:0x426da4`（两者共用的尾巴）。

| 项 | 一手 | 结论 |
| --- | --- | --- |
| 载体 / 量纲 | `PIV = th12:0x4b0c78`（存储 = 显示 **×100**）、上限 `th12:0x4b0cf4`、取值 `th12:0x421880` = `trunc10(⌊PIV/100⌋)` | 与 §1–§3 一致 |
| 涨源（在玩中只有两个） | 擦弹 `th12:0x4391f2`（`+100` 存储 = **+1 显示**）；**飞碟收集计数满 3** 收飞碟道具 `th12:0x4270d8`（`+100000` 存储 = **+1000 显示**，再夹上限） | 无衰减（§6 的否定结论不变） |
| 蓝点（type 2）线上 | `y ≤ 128.0f` ⇒ `PIV_disp`（**无乘数**） | 值是「那个量本身」 |
| 蓝点线下 | `0.75 × PIV_disp × (1 − t/450)`，`t = (int)player_y − 128`（★ 自机 y **先取整**再减线高） | ⇒ **0.75 族**（与 th13–th17 同族） |
| `ΔG_12` / `coeff_12` | `ΔG_12` = PIV 本身；`coeff_12` = 线上 `1`、线下 `0.75×(1−t/450)` | ⇒ **进 `coeff_12`**（不是 `1`） |

### ★ 飞碟奖励道具（type `0x10`–`0x12`，handler `th12:0x426c6f`）**不判高度**

- **线常量 `th12:0x4a3db8`（128.0f）在道具状态机里只被 `th12:0x426b20`（蓝点那一支）引用一次** ⇒ 其余类型（含飞碟奖励道具）没有高度项。
- 它内部还有一层**子分派**（`th12:0x426c83`：`eax = [item+0x9c8] + 1; cmp eax,4; jmp [eax*4+0x426fdc]`）：

| 子项（`[item+0x9c8]`） | handler | 形状 |
| --- | --- | --- |
| −1 | `th12:0x426c9a` | `ftol(PIV_disp × [item+0x9d0] × [item+0x9d4])` |
| 0 | `th12:0x426da4` | 不给分（另一条路） |
| **1** | **`th12:0x426cd2`** | ★ **两项之和**：`ftol(N × PIV_disp × [item+0x9d4]) + PIV_disp × [item+0x9d0]`，`N = [item+0x9cc]`（**第二项不乘 `N`**） |
| 2 / 3 | `th12:0x426d27` | 与「−1」同式 |

- 四支共用尾巴 `th12:0x426d50`–`th12:0x426da4`：`ftol` → `trunc10`（`/10` 再 `×10`）→ 下限 **10** → 飘窗（黄 `0xffffff00`）→ `th12:0x40e730`（`SCORE += v/10`）。
  ⇒ 与蓝点**共用尾巴**（蓝点的线上支 `th12:0x426ba3` 正是 `jmp 0x426d65`）。
- ⇒ **要复刻飞碟奖励那一条，`coeff` 取 `1`**（原作里它不吃高度折扣）；`ΔG` 形状按上表四支之一。

⚠ 仍未钉（🟡，不影响上面的结论）：`[item+0x9c8]`（子类型）、`[item+0x9cc]`（`N`）、`[item+0x9d0]` / `[item+0x9d4]`（两个奖励系数槽）
的**写点与语义归属**（谁是「飞碟数」、谁是脚本给的倍率）尚未逐一追；`type 0x12` 的贴图是空图（§8 的老 🟡）。
