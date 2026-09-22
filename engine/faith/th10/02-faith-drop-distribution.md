# TH10 信仰点掉落分布 —— 大信仰点（type 3）的逐关 / 逐波 / 逐敌全量清单

> **版本**：TH10 v1.00a（`th10.exe`，487,936 B，md5 `7dc488d82c81dd4aee4ba098b8804d83`，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本写成 `th18:0x…`。ECL 内的「`0x8dec`」这类 4 位十六进制是**文件内偏移**，不是地址。
> **方法**：一手平坦反汇编（`objdump -d -M intel`，产物 `local/_scratch/th10_full.asm`）
> + 一手读 exe 的 opcode 分派表 / 参数槽跳表；
> + ECL 侧用 `thecl -d 10 -x` 全量反编译（**不加 `-m`**：eclmap 里没有 `th10.eclm`），
> 再用自写的 ECL 迷你解释器把 `main` / `MainSub*` 时间轴展开成「波」。
> **可信度**：① opcode 号、掉落字段、死亡掉落路径、`enmCreate` 参数序 ✅一手（指令原文 + 跳表原字节）；
> ② 四色妖精 ↔ `dropMain` 对应、逐关声明点与创建站点 ✅一手（ECL 文本可复算）；
> ③ **只数**（每站点实际刷几只）🟡 —— 由自写解释器展开循环/参数得到，口径见 §3.4，
> 其中「一波内嵌套子波」的**帧号**仅为累加近似（只数的可信度高于帧号）。
> **未做**：没有开 Ghidra / MCP（工程锁）。所有结论只到「地址 + 反汇编指令 + ECL 文本」一级。

---

## 0. 一句话结论（先答用户那个问题）

1. **TH10 里「纯信仰点掉落」的敌人只有一族：四色妖精里的绿妖精（`G*` / `Ecl_G*`）**，
   它体内恒写 `dropMain(3)`（大信仰点）。红 = `dropMain(1)`（P 点）、蓝 = `dropMain(2)`（点道具/蓝点）、
   黄 = `dropMain(0)`（**什么都不掉**）—— 7 关 7 次全对（§2.1）。
2. **全作（6 面 + EX）掉大信仰点的敌人 = 177 只**：一面 14 / 二面 23 / 三面 28 / 四面 25 / 五面 37 / 六面 14 / EX 36。
3. **这 177 只全是「一次刷 1 只」**：177 次 `enmCreate` 的执行里 **n=1 占 177/177（100%）**，
   ZUN 没有任何一处批量刷绿妖精。⇒ **一波改一两只**这个口径与原作一致。
4. 逐波密度（§3.2，共 54 波）：**33 个波一只没有**；非零的 21 个波里 **9 个是 1–2 只**；
   最大的三个波是 EX 面 `MainSub1` 36 只、五面 boss 段 21 只、四面 boss 段 18 只。
   **boss 段占 46/177 只（26%），道中 131 只。**
5. **稀有度（数字回答）**：type3 站点占全部敌人站点的 **9.9%（175 / 1774）**；
   一关累计 **14–37 只**（平均 25.3）；每波**中位 0 只**，出现时 **1–2 只**（§3.5）。
6. **敌人 ECL 里除 type 3 之外没有任何信仰点声明**：全作 `dropMain` 的取值只有 `0/1/2/3/7`，
   没有 `8`（微信仰点）/`9`（小信仰点）；`dropExtra` 只用 `1/2/4/7`（§4.1）。
   ⇒ 实战里小信仰点（type 9）**不是敌人掉的**，而是**满火力时 P 点被改写**成的（§4.2）。

---

## 1. 机制（全部一手；这一节撑着后面所有计数）

### 1.1 opcode 号：从 exe 的分派表读，不靠名字猜

发现：ECL 解释器的派发是「两级表」——

```
0x40e79c:  mov  ax,[esi+0x4]           ; 指令第 1 个 word = opcode
0x40e7a0:  movsx ecx,ax
0x40e7a3:  add  ecx,0xffffff00         ; ecx = opcode − 0x100
0x40e7a9:  cmp  ecx,0xb4               ; > 0xb4 ⇒ 默认 handler
0x40e7bc:  ja   0x411d18
0x40e7c2:  movzx ecx,BYTE PTR [ecx+0x411ee0]   ; ★ 索引表（1 字节/opcode）
0x40e7c9:  jmp  DWORD PTR [ecx*4+0x411d30]     ; ★ 跳表（handler 地址）
```

验证：读 exe 字节得索引表 `0x411ee0`、跳表 `0x411d30`（两张表都在 `.text`，RVA 换算用 `文件偏移 = VMA − 0x400000 − 0xc00`）；
再用 ExpHP `th-re-data` 的 `labels.json`（`ecl` 段 = `[handler 地址, "opcode__名字"]`）把地址翻成名字，两路互证。

结论（✅一手，v1.00a）：本文用到的 opcode 号如下，**后续所有计数都以「号」为准**（thecl 无 eclmap 时输出的 `ins_N` 就是号）。

| 号 | 名字 | handler | 号 | 名字 | handler |
| ---: | --- | --- | ---: | --- | --- |
| 256 | `enmCreate` | `0x40e7e1` | 326 | `dropClear` | `0x40fdd8` |
| 257 | `enmCreateA` | `0x40e985` | 327 | `dropExtra` | `0x40fdfc` |
| 260 | `enmCreateM` | `0x40eb16` | 328 | `dropArea` | `0x40fe56` |
| 261 | `enmCreateAM` | `0x40ec35` | 329 | `dropItems` | `0x40fe9e` |
| 265/266/267/268 | `enmCreate?F` 四变体 | `0x40e7d0` / `0x40e974` / `0x40eb06` / `0x40ec24` | 330 | `dropMain` | `0x40fec3` |
| 270 | `enmCreate270`（**3 坐标**） | `0x40e9e7` | 331 | `lifeSet` | `0x40ff25` |

> ⚠ opcode **329 `dropItems` 不是「声明」而是「立刻撒」**（它 `call 0x40c9a0` 直接生成道具，然后把主掉落槽清零），
> 所以统计「谁会掉信仰」时**不能**把它算成声明点。本文的清单只取 330 与 `enmCreate*`。

### 1.2 掉落表就在敌人对象里：`enemy+0x2408`

发现：ECL 的上下文基址 = `enemy+0x103c`（`0x40e760`：`add ecx,0x103c` 后跳进解释器）。

```
330 dropMain  (0x40fec3): 读第 1 实参 eax → 0x40ff08: mov [ebx+0x13cc],eax
                         ⇒ [ctx+0x13cc] = [enemy+0x2408] = 主掉落 type
327 dropExtra (0x40fdfc): 0x40fe38: mov [ebx+esi*4+0x13cc],eax   ; esi = type, eax = 个数
                         ⇒ [enemy+0x2408+type*4] = 「额外掉落」计数表（type 1..12）
326 dropClear (0x40fdd8): rep stos 12 个 dword 于 [ctx+0x13d0]，即清空 type 1..12 的计数
328 dropArea  (0x40fe56): 写 [ctx+0x1400] / [ctx+0x1404]（额外掉落的散布半径）
```

验证（死亡路径，`0x40e63e` 起）：

```
0x40e63e: mov eax,[ebp+0x2408]        ; 主掉落 type
0x40e650: test eax,eax / jle 0x40e66d ; <=0 ⇒ 不掉
0x40e654: push 0x400ccccd (2.6f)      ; 速度
0x40e659: push 0xbfc90fdb (−π/2)      ; 角度
0x40e65e: push 0xffffffff (−1)
0x40e660: push eax                    ; type
0x40e668: call 0x41bb00               ; ItemManager::spawn_item
0x40e66d: push ebp (=enemy+0x2408) / call 0x40c9d0   ; 再按计数表补撒额外掉落
```

结论（✅一手，v1.00a）：**主掉落 type > 0 ⇒ 该敌死亡时恰好掉 1 颗该 type 的道具**（角度 −π/2 = 朝下、速度 2.6）。
`0x40c9d0` 内层循环 `cmp eax,0xb`（11 项）对 `[ebp+4*type]` 逐项按个数撒（`0x40cb94 cmp ebx,eax / jl`）。

### 1.3 `enmCreate*` 的实参序：**第 4 位是 life、第 5 位是 score、第 6 位才是 item**

这条是全文最吃重的一条（决定「谁掉信仰」能不能从创建点读出来），逐位坐实：

```
0x40e90d: mov eax,[edi+esi*4+0x18]  ; 第 3 个 int 实参 → push 3 → call 0x44ff00
0x40e91c: mov [esp+0x25c],eax       ; → 局部结构 +0x14
0x40e929: mov eax,[edi+esi*4+0x1c]  ; 第 4 个 int → push 4
0x40e938: mov [esp+0x254],eax       ; → 局部结构 +0x0c
0x40e93f: mov esi,[edi+esi*4+0x20]  ; 第 5 个 int → push 5
0x40e966: mov [esp+0x258],eax       ; → 局部结构 +0x10
0x40ed11: call 0x40cfb0             ; Enemy::create(manager, ctx, &结构)
 0x40cff4: mov [ebp+0x23f8],ecx     ; ← 结构 +0x0c
 0x40cffd: mov [ebp+0x23fc],edx     ; ← 结构 +0x14
 0x40d00c: mov [ebp+0x2408],eax     ; ← 结构 +0x10   ★ 主掉落
```

三条独立对名：

| 位 | 落点 | 独立证据 |
| --- | --- | --- |
| 第 4 实参 | `enemy+0x23fc` | `331 lifeSet`（`0x40ff30`）写 `[ctx+0x13c0]` = `enemy+0x23fc`，同一格 ⇒ **life** |
| 第 5 实参 | `enemy+0x23f8` | 击破计分 `0x40e231: mov eax,[ebp+0x13bc]` → `0x409d90 add_to_score` ⇒ **score** |
| 第 6 实参 | `enemy+0x2408` | 与 §1.2 的 `dropMain` 同格、与死亡掉落读取同格 ⇒ **item（主掉落 type）** |

⇒ **「体内 `dropMain` 与创建点第 6 参，谁后写谁生效」**（出生时写一次、敌人自己的 ECL 跑到 `dropMain` 再写一次，
`0x40d0c7`–`0x40d0ec` 还有一处出生后改写，见 §4.2 的 🟡）。
实测例：一面 `RGirl00` 的创建点是第 6 参 `2`，但它体内写 `dropMain(1)` ⇒ **实际掉 1**（本文一律取「体内优先」）。

### 1.4 opcode 270 是「3 坐标」变体，item 在**第 7 个**实参

一手：`ins_270` 的格式表（`thecl/thecl10.c:238`）写 `"mffSSSS"`，但 hexdump 一面 `stage04.ecl` 偏移 `0xdf30` 的实参区得到
`(−128.0f, 160.0f, −2.0f, 60, 1000, 1)`：第 3 个值是 **float z**（handler `0x40ea39` 用浮点转换函数 `0x412aa0`），
其余 3 个 int 才是 life/score/item。⇒ **270 / 271 的 item = 第 7 实参**（本文按此处理，否则会把 life 当 item）。

### 1.5 8 个「参数槽」与 `$I0`：刷出来的敌人会**继承**刷出者的这 8 格

发现：数值 0..3 的「颜色选择」在 ECL 里写成 `$I0`（thecl 无 eclmap 时输出 `[-9985]`），
而它的真身是一手可查的：

```
敌机特变量 getter（敌人 vtable 槽 +4 = 0x411fc0）：
0x411fc4: add eax,0x2710        ; 索引 +10000
0x411fc9: cmp eax,0x32          ; ≤ 0x32（51 项）
0x411fd2: jmp [eax*4+0x412228]  ; 跳表 0x412228
  -9985 → 0x412048: mov eax,[ecx+0x1138]
  -9984 → 0x412051: mov eax,[ecx+0x113c]
  -9983 / -9982 → +0x1140 / +0x1144
  -9981 → 0x41206c: fld [ecx+0x1148]   （float）
  -9980 / -9979 / -9978 → +0x114c / +0x1150 / +0x1154 （float）
```

而 `enmCreate*` 在出生时把**刷出者的 `ctx+0xfc..0x11b`（8 个 dword）整块复制给新敌人**：

```
0x40e954: lea esi,[ebx+0xfc] / rep movs 8 dwords → 局部结构 +0x20
0x40d034: lea esi,[ebx+0x20] / lea edi,[ebp+0x1138] / rep movs 8 dwords
（ctx+0xfc = enemy+0x1138 ✓）
```

结论（✅一手，v1.00a）：`$I0`/`$I1`/`$I2`/`$I3` = `enemy+0x1138..0x1147`（4 个 int），
`%F0..%F3` = `+0x1148..0x1157`（4 个 float）；**创建时被继承**。
⇒ 读法：`enmCreate` 之前最近的 `$I0 = k` 就是那只敌人的 `$I0`；这也是 ZUN 在四面用「一只运输机带一只妖精」的手法（§3.3）。

### 1.6 「波次」的口径（可复算，且不靠猜）

| 口径 | 定义 | 依据 |
| --- | --- | --- |
| **时间轴 routine** | 关卡 ECL 里被引擎当主线程跑的是 `main()`；它用 `@MainSubNN() async;` 逐个派发道中组 | thecl 反编译文本里 `main` 内的 `@X() async` 调用；`main` 是 ECL 的入口子程序 |
| **波 = 一个时间轴 routine** | `main` 自己直接写的那批 = `main#k`；每个 `@MainSubNN() async` = 一波 | 同上 |
| `main#k` 的切块 | `main` 内联刷敌按帧号聚类，相邻两次刷敌间隔 **≥180 帧（3 秒）** 就切块 | 阈值是**人为选的**，文中给出每块的帧区间，可自行重切 |

依据补强：TH10 的关卡 ECL 里只有 `main`（68–140 个子程序中唯一被引擎启动的那个），
`MainSub*` 全部由 `main`（以及更深一层的 `MainSub*`）用 `async` 派发；例如一面 `main` 在 `+1680` 帧处 `@MainSub() async;`。

---

## 2. 大信仰点（type 3）的站点全量清单

### 2.1 先钉死「谁掉大信仰点」：四色妖精 ↔ `dropMain` 一一对应

`thecl` 的 `Ecl_*Girl00` 包装子程序把「颜色」写成一个参数（一面原文）：

```asm
; dat/stage01.ecl
void Ecl_BGirl00() { anmSelect(1); anmSetSprite(1, 45); @Ecl_Girl00(0);  delete(); }   ; 蓝
void Ecl_RGirl00() { anmSelect(1); anmSetSprite(1, 46); @Ecl_Girl00(5);  delete(); }   ; 红
void Ecl_GGirl00() { anmSelect(1); anmSetSprite(1, 47); @Ecl_Girl00(10); delete(); }   ; 绿
void Ecl_YGirl00() { anmSelect(1); anmSetSprite(1, 48); @Ecl_Girl00(15); delete(); }   ; 黄
void Ecl_Girl00(var A) { anmSelect(1); anmSetMain(0, $A); … }
```

即 **颜色 = `anmSetMain(0, 5k)`：0 蓝 / 5 红 / 10 绿 / 15 黄**（sprite 45/46/47/48）。
把这套字母族（`B*` / `R*` / `G*` / `Y*`）与它们体内的 `dropMain` 交叉，**7 关 7 次零例外**：

| 关 | 蓝 `B*` | 红 `R*` | **绿 `G*`** | 黄 `Y*` |
| --- | --- | --- | --- | --- |
| 一面 | `dropMain(2)` ×5 | `dropMain(1)` ×5 | **`dropMain(3)` ×5** | （无黄族） |
| 二面 | `dropMain(2)` ×6 | `dropMain(1)` ×6 | **`dropMain(3)` ×6** | （无黄族） |
| 三面 | `dropMain(2)` ×6 | `dropMain(1)` ×6 | **`dropMain(3)` ×6** | `dropMain(0)` ×4 |
| 四面 | `dropMain(2)` ×4 | `dropMain(1)` ×4 | **`dropMain(3)` ×4** | `dropMain(0)` ×4 |
| 五面 | `dropMain(2)` ×7 | `dropMain(1)` ×7 | **`dropMain(3)` ×7** | `dropMain(0)` ×7 |
| 六面 | `dropMain(2)` ×4 | `dropMain(1)` ×4 | **`dropMain(3)` ×4** | `dropMain(0)` ×4 |
| EX | `dropMain(2)` ×7 | `dropMain(1)` ×7 | **`dropMain(3)` ×7** | `dropMain(0)` ×5 |

> 可信度：✅一手（ECL 文本，逐子程序取体内最后一次 `dropMain` 的字面值）。
> 🟡 颜色本身（"G 是绿的"）只由 **名字 + anm 脚本号 + sprite 号**三者自洽推出，本文**没有做贴图像素取样**。
> 但「哪一族掉 type 3」不依赖颜色命名 —— 那是 ECL 里的字面值。

### 2.2 逐关：`dropMain(3)` 声明点（体内自己写）

| 关 | 子程序（定义行） | `dropMain(3)` 行 | ECL 偏移 |
| --- | --- | ---: | --- |
| 一面 | `GGirl00`(929) `GGirl00B`(938) `GGirl00C`(948) `GGirl00D`(958) `GGirl00E`(968) | 932 / 943 / 951 / 961 / 971 | `0x5800` `0x58b0` `0x591c` `0x59b4` `0x5a4c` |
| 二面 | `GGirl00`(991) `GGirl00B`(999) `GGirl00C`(1007) `GGirl00D`(1015) `GSpCir00`(1023) `GSpCir01`(1031) | 994 / 1002 / 1010 / 1018 / 1026 / 1034 | `0x61bc` `0x6230` `0x62a4` `0x6318` `0x638c` `0x6408` |
| 三面 | `GGirl00`(1047) `GGirl00B`(1055) `GGirl00C`(1063) `GGirl00D`(1071) `GSpCir00`(1079) `GSpCir01`(1087) | 1050 / 1058 / 1066 / 1074 / 1082 / 1090 | `0x6a58` `0x6acc` `0x6b40` `0x6bb4` `0x6c28` `0x6ca4` |
| 四面 | `GGirl00`(1495) `GGirl01`(1503) `GGirl03`(1511) `GGirl04`(1519) `Girl02`(1602) | 1498 / 1506 / 1514 / 1522 / 1610 | `0xad40` `0xadb4` `0xae28` `0xae9c` `0xb5c0` |
| 五面 | `GCir00`(1427) `GCir01`(1435) `GCir02`(1443) `GGirl00`(1451) `GGirl00L`(1459) `GGirl02`(1467) `GGirl04`(1475) | 1430 / 1438 / 1446 / 1454 / 1462 / 1470 / 1478 | `0x9410` `0x9484` `0x94f8` `0x956c` `0x95e0` `0x9654` `0x96c8` |
| 六面 | `GCir00`(1971) `GCir01`(1979) `GCir01b`(1987) `GGirl00`(1995) | 1974 / 1982 / 1990 / 1998 | `0xd0d8` `0xd14c` `0xd1c0` `0xd234` |
| EX | `GCir00`(2126) `GCir00b`(2134) `GCir00c`(2142) `GGirl00`(2150) `GGirl05`(2158) `GGirl06`(2166) `GGirl06b`(2174) | 2129 / 2137 / 2145 / 2153 / 2161 / 2169 / 2177 | `0xdd30` `0xdda4` `0xde18` `0xde8c` `0xdf00` `0xdf74` `0xdfe8` |

合计 **40 个声明点**（行号 = `local/_scratch/th10/decomp/stageNN_m.txt` 与 `thecl` 裸 dump 一致，两者逐字节相同，见附录）。

> 🟡 关卡映射：`dat/` 下只有 `default.ecl` + `stage01..stage07.ecl` 共 8 个 ECL；TH10 是「6 面 + Extra」，
> 故本文把 `stage01..stage06` 记作一面~六面、**`stage07` 记作 EX**。该映射未逐字核对台词/角色（`st07_*.msg` 未读）。

### 2.3 逐关：**创建站点**（谁被刷出来时 item=3 或刷出的子程序体内写 3）

下表由静态扫描全 ECL 得到（每个 `enmCreate*` 站点一行；「依据」列说明 type 3 是体内写的还是创建点第 6 参给的）。
一/二/三面与 EX 的绿妖精**多数是直接刷出来的**；四面/五面/六面则是「运输机 → 妖精」两层（§3.3）。

（下表每行 = 一个 `enmCreate*` 站点；`行` = thecl dump 的行号，`ECL 偏移` = 该指令在 `.ecl` 里的偏移）

##### 一面（`dat/stage01.ecl`）

| 宿主子程序 | 行 | ECL 偏移 | 目标敌人 | 形式 | 有效 type | 依据 |
| --- | ---: | --- | --- | --- | ---: | --- |
| `MainSub` | 1571 | `0x8dec` | `GGirl00B` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub` | 1573 | `0x8e20` | `GGirl00B` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `main` | 1800 | `0x9f0c` | `Ecl_GGirl00` | `enmCreateA` | 3 | 创建点第 6 参=3 |
| `main` | 1819 | `0xa0d0` | `GGirl00` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 1831 | `0xa1f0` | `GGirl00` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 1841 | `0xa2e0` | `GGirl00` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `main` | 1843 | `0xa310` | `Girl01` | `enmCreateAM` | 3 | 创建点第 6 参=3 |
| `main` | 1863 | `0xa584` | `Ecl_GGirl00` | `enmCreateA` | 3 | 创建点第 6 参=3 |
| `main` | 1865 | `0xa5b8` | `GGirl00` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 1867 | `0xa5e8` | `GGirl00` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 1906 | `0xa9a8` | `GGirl00` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `main` | 1916 | `0xaa98` | `GGirl00` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `main` | 1973 | `0xb1f0` | `Girl01` | `enmCreateA` | 3 | 创建点第 6 参=3 |
| `main` | 1974 | `0xb220` | `Girl01` | `enmCreateAM` | 3 | 创建点第 6 参=3 |

##### 二面（`dat/stage02.ecl`）

| 宿主子程序 | 行 | ECL 偏移 | 目标敌人 | 形式 | 有效 type | 依据 |
| --- | ---: | --- | --- | --- | ---: | --- |
| `MBossDummy` | 1538 | `0x9050` | `GSpCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MBossDummy` | 1540 | `0x9084` | `GSpCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MBossDummy` | 1550 | `0x9188` | `GSpCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MBossDummy` | 1552 | `0x91bc` | `GSpCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MBossDummy` | 1570 | `0x9390` | `GSpCir01` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MBossDummy` | 1572 | `0x93c4` | `GSpCir01` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MBossDummy` | 1584 | `0x94fc` | `GSpCir01` | `enmCreate` | 3 | 体内 dropMain(3) |
| `main` | 2058 | `0xbae4` | `GGirl00C` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 2059 | `0xbb18` | `GGirl00C` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `main` | 2073 | `0xbcec` | `GGirl00C` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 2074 | `0xbd20` | `GGirl00C` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `main` | 2103 | `0xc028` | `GGirl00D` | `enmCreateAF` | 3 | 体内 dropMain(3) |
| `main` | 2113 | `0xc12c` | `GGirl00D` | `enmCreateAMF` | 3 | 体内 dropMain(3) |
| `main` | 2123 | `0xc230` | `GGirl00D` | `enmCreateAF` | 3 | 体内 dropMain(3) |
| `main` | 2133 | `0xc334` | `GGirl00D` | `enmCreateAMF` | 3 | 体内 dropMain(3) |
| `main` | 2143 | `0xc438` | `GGirl00D` | `enmCreateAF` | 3 | 体内 dropMain(3) |
| `main` | 2153 | `0xc53c` | `GGirl00D` | `enmCreateAMF` | 3 | 体内 dropMain(3) |
| `main` | 2163 | `0xc640` | `GGirl00D` | `enmCreateAF` | 3 | 体内 dropMain(3) |
| `main` | 2173 | `0xc744` | `GGirl00D` | `enmCreateAMF` | 3 | 体内 dropMain(3) |
| `main` | 2183 | `0xc848` | `GGirl00D` | `enmCreateAF` | 3 | 体内 dropMain(3) |
| `main` | 2193 | `0xc94c` | `GGirl00D` | `enmCreateAMF` | 3 | 体内 dropMain(3) |
| `main` | 2203 | `0xca50` | `GGirl00D` | `enmCreateAF` | 3 | 体内 dropMain(3) |
| `main` | 2265 | `0xd014` | `GGirl00B` | `enmCreateAM` | 3 | 体内 dropMain(3) |

##### 三面（`dat/stage03.ecl`）

| 宿主子程序 | 行 | ECL 偏移 | 目标敌人 | 形式 | 有效 type | 依据 |
| --- | ---: | --- | --- | --- | ---: | --- |
| `MainSub01` | 1634 | `0x9a24` | `GGirl00B` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub01` | 1650 | `0x9bc4` | `GGirl00B` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub01` | 1666 | `0x9d64` | `GGirl00B` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub01` | 1674 | `0x9e34` | `GGirl00B` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub01` | 1676 | `0x9e68` | `GGirl00B` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub01` | 1678 | `0x9e9c` | `GGirl00B` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub01` | 1680 | `0x9ed0` | `GGirl00B` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub01` | 1682 | `0x9f04` | `GGirl00B` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub02` | 1703 | `0xa0d8` | `GGirl00B` | `enmCreateAF` | 3 | 体内 dropMain(3) |
| `MainSub02r` | 1724 | `0xa2ac` | `GGirl00B` | `enmCreateAMF` | 3 | 体内 dropMain(3) |
| `MainSub03` | 1749 | `0xa564` | `GGirl00C` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub03` | 1752 | `0xa5c0` | `GGirl00C` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub03b` | 1803 | `0xabb4` | `GGirl00C` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `MainSub03b` | 1823 | `0xae38` | `GGirl00C` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `MainSub03b` | 1826 | `0xae94` | `GGirl00C` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 2228 | `0xcf84` | `GGirl00` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 2230 | `0xcfb4` | `GGirl00` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 2274 | `0xd3d4` | `Girl02` | `enmCreateAM` | 3 | 创建点第 6 参=3 |
| `main` | 2283 | `0xd494` | `Girl02` | `enmCreateAM` | 3 | 创建点第 6 参=3 |
| `main` | 2292 | `0xd554` | `Girl02` | `enmCreateAM` | 3 | 创建点第 6 参=3 |
| `main` | 2301 | `0xd614` | `Girl02` | `enmCreateAM` | 3 | 创建点第 6 参=3 |
| `main` | 2318 | `0xd7cc` | `GSpCir00` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 2332 | `0xd938` | `GSpCir00` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `main` | 2346 | `0xdaa4` | `GSpCir00` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `main` | 2360 | `0xdc10` | `GSpCir00` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 2374 | `0xdd7c` | `GSpCir00` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `main` | 2388 | `0xdee8` | `GSpCir00` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 2402 | `0xe054` | `GSpCir00` | `enmCreateA` | 3 | 体内 dropMain(3) |
| `main` | 2416 | `0xe1c0` | `GSpCir00` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `main` | 2419 | `0xe21c` | `Girl02` | `enmCreateA` | 3 | 创建点第 6 参=3 |
| `main` | 2421 | `0xe274` | `Girl02` | `enmCreateAM` | 3 | 创建点第 6 参=3 |
| `main` | 2424 | `0xe2cc` | `Girl02` | `enmCreateA` | 3 | 创建点第 6 参=3 |
| `main` | 2426 | `0xe324` | `Girl02` | `enmCreateAM` | 3 | 创建点第 6 参=3 |

##### 四面（`dat/stage04.ecl`）

| 宿主子程序 | 行 | ECL 偏移 | 目标敌人 | 形式 | 有效 type | 依据 |
| --- | ---: | --- | --- | --- | ---: | --- |
| `DiveEnemy00` | 1222 | `0x9320` | `GGirl00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `DiveEnemy00B` | 1263 | `0x9750` | `GGirl00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `DiveEnemy01` | 1304 | `0x9b80` | `GGirl01` | `enmCreate` | 3 | 体内 dropMain(3) |
| `DiveEnemy01B` | 1345 | `0x9fe8` | `GGirl01` | `enmCreate` | 3 | 体内 dropMain(3) |
| `DiveEnemy02` | 1377 | `0xa29c` | `Girl02` | `enmCreate` | 3 | 体内 dropMain(3) |
| `DiveEnemy02B` | 1404 | `0xa4b8` | `Girl02` | `enmCreate` | 3 | 体内 dropMain(3) |
| `DiveEnemy04` | 1440 | `0xa908` | `GGirl04` | `enmCreate` | 3 | 体内 dropMain(3) |

##### 五面（`dat/stage05.ecl`）

| 宿主子程序 | 行 | ECL 偏移 | 目标敌人 | 形式 | 有效 type | 依据 |
| --- | ---: | --- | --- | --- | ---: | --- |
| `MainSub1` | 2248 | `0xe5ec` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub1` | 2250 | `0xe61c` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub1` | 2309 | `0xeb7c` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub1` | 2311 | `0xebac` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub1` | 2341 | `0xee5c` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub1` | 2343 | `0xee8c` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub10` | 2388 | `0xf530` | `GCir02` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub10` | 2391 | `0xf5ac` | `GCir02` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub10` | 2478 | `0x10314` | `GCir02` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub10` | 2481 | `0x10390` | `GCir02` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub10` | 2525 | `0x10a44` | `GCir02` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub10` | 2528 | `0x10ac0` | `GCir02` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub10` | 2568 | `0x11110` | `GCir02` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub10` | 2571 | `0x1118c` | `GCir02` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub11` | 2637 | `0x11820` | `GGirl04` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub11` | 2639 | `0x11850` | `GGirl04` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub11` | 2646 | `0x11910` | `GGirl04` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub11` | 2648 | `0x11940` | `GGirl04` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub2` | 2694 | `0x11dc4` | `GGirl00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2` | 2696 | `0x11df4` | `GGirl00L` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub3` | 2707 | `0x11edc` | `Girl01` | `enmCreate` | 3 | 创建点第 6 参=3 |
| `MainSub3` | 2711 | `0x11f5c` | `Girl01` | `enmCreate` | 3 | 创建点第 6 参=3 |
| `MainSub5` | 2827 | `0x12b20` | `GGirl02` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub5` | 2829 | `0x12b50` | `GGirl02` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub5` | 2836 | `0x12c10` | `GGirl02` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub5` | 2838 | `0x12c40` | `GGirl02` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub7` | 2892 | `0x131d4` | `Girl01` | `enmCreate` | 3 | 创建点第 6 参=3 |
| `MainSub7` | 2896 | `0x13254` | `Girl01` | `enmCreate` | 3 | 创建点第 6 参=3 |
| `MainSub8` | 2931 | `0x13638` | `Girl01` | `enmCreate` | 3 | 创建点第 6 参=3 |
| `MainSub8` | 2935 | `0x136b8` | `Girl01` | `enmCreate` | 3 | 创建点第 6 参=3 |
| `MainSub8` | 2937 | `0x136e8` | `Girl03` | `enmCreate` | 3 | 创建点第 6 参=3 |
| `MainSub9` | 2974 | `0x13a14` | `GCir01` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub9` | 2976 | `0x13a44` | `GCir01` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub9` | 3035 | `0x13fa4` | `GCir01` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub9` | 3037 | `0x13fd4` | `GCir01` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub9` | 3067 | `0x14284` | `GCir01` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub9` | 3069 | `0x142b4` | `GCir01` | `enmCreateM` | 3 | 体内 dropMain(3) |

##### 六面（`dat/stage06.ecl`）

| 宿主子程序 | 行 | ECL 偏移 | 目标敌人 | 形式 | 有效 type | 依据 |
| --- | ---: | --- | --- | --- | ---: | --- |
| `MainSub1a` | 2244 | `0xeb88` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub1b` | 2263 | `0xed0c` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub2a` | 2308 | `0xf0ac` | `GCir01` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2b` | 2327 | `0xf230` | `GCir01b` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2c` | 2348 | `0xf404` | `GCir01` | `enmCreateAM` | 3 | 体内 dropMain(3) |
| `MainSub2d` | 2370 | `0xf5d8` | `GCir01b` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub5` | 2434 | `0xfaa4` | `Girl02` | `enmCreate` | 3 | 创建点第 6 参=3 |

##### EX（`dat/stage07.ecl`）

| 宿主子程序 | 行 | ECL 偏移 | 目标敌人 | 形式 | 有效 type | 依据 |
| --- | ---: | --- | --- | --- | ---: | --- |
| `MainSub1` | 3131 | `0x1379c` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub1` | 3133 | `0x137cc` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub1` | 3135 | `0x137fc` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub1` | 3149 | `0x1394c` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub1` | 3151 | `0x1397c` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub1` | 3153 | `0x139ac` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub1` | 3167 | `0x13afc` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub1` | 3169 | `0x13b2c` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub1` | 3171 | `0x13b5c` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub1` | 3185 | `0x13cac` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub1` | 3187 | `0x13cdc` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub1` | 3189 | `0x13d0c` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3452 | `0x15754` | `GCir00b` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3454 | `0x15784` | `GCir00b` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3456 | `0x157b4` | `GCir00b` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3458 | `0x157e4` | `GCir00b` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3460 | `0x15814` | `GCir00b` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3462 | `0x15844` | `GCir00b` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3464 | `0x15874` | `GCir00b` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3466 | `0x158a4` | `GCir00b` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3468 | `0x158d4` | `GCir00b` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3470 | `0x15904` | `GCir00b` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3472 | `0x15934` | `GCir00b` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3474 | `0x15964` | `GCir00b` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3476 | `0x15994` | `GCir00b` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3478 | `0x159c4` | `GCir00b` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3480 | `0x159f4` | `GCir00b` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3482 | `0x15a24` | `GCir00b` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3484 | `0x15a54` | `GCir00b` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub2` | 3486 | `0x15a84` | `GCir00b` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub4` | 3577 | `0x1627c` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub4` | 3579 | `0x162ac` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub4` | 3581 | `0x162dc` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub4` | 3599 | `0x1647c` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub4` | 3601 | `0x164ac` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub4` | 3603 | `0x164dc` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3761 | `0x17240` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3763 | `0x17270` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3765 | `0x172a0` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3767 | `0x172d0` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3769 | `0x17300` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3771 | `0x17330` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3773 | `0x17360` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3775 | `0x17390` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3777 | `0x173c0` | `GCir00` | `enmCreate` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3779 | `0x173f0` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3781 | `0x17420` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3783 | `0x17450` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3785 | `0x17480` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3787 | `0x174b0` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3789 | `0x174e0` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3791 | `0x17510` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3793 | `0x17540` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |
| `MainSub8` | 3795 | `0x17570` | `GCir00` | `enmCreateM` | 3 | 体内 dropMain(3) |

---

## 3. 分布：逐关 / 逐波 / 逐敌

### 3.1 逐关合计

| 关 | `dropMain(3)` 声明点 | type3 创建站点 | 道中直接刷出敌机 | **掉 type 3 的敌人数** |
| --- | ---: | ---: | ---: | ---: |
| 一面 | 5 | 14 | 170 | **14** |
| 二面 | 6 | 23 | 163 | **23** |
| 三面 | 6 | 33 | 265 | **28** |
| 四面 | 5 | 7 | 24 | **25** |
| 五面 | 7 | 37 | 60 | **37** |
| 六面 | 4 | 7 | 44 | **14** |
| EX | 7 | 54 | 37 | **36** |
| **合计** | **40** | **175** | **763** | **177** |

- 「道中直接刷出敌机」= 时间轴 routine（`main` 内联块 + 各 `MainSub*`）**自己**的 `enmCreate` 次数（循环已展开），
  **不含**由敌人再刷出的子代（四面起有「运输机 → 妖精」两层，见 §3.3）。
**分母口径之一（静态、可复算）**：全 ECL 里 `enmCreate*` 站点的总数与其中 type 3 站点数：

| 关 | 全部 `enmCreate*` 站点（静态） | 其中 type3 站点 | 占比 |
| --- | ---: | ---: | ---: |
| 一面 | 147 | 14 | 9.5% |
| 二面 | 213 | 23 | 10.8% |
| 三面 | 208 | 33 | 15.9% |
| 四面 | 322 | 7 | 2.2% |
| 五面 | 351 | 37 | 10.5% |
| 六面 | 131 | 7 | 5.3% |
| EX | 402 | 54 | 13.4% |
| **合计** | **1774** | **175** | **9.9%** |

- 🟡 「敌机总数」一列**不给**：由敌人再刷出的子代需要逐链展开，且 boss 攻击用的辅助敌机（如 `MBossAtEnemy`）的循环次数
  依赖运行时变量（`[-9959]` 难度等），静态只能给下界。**能给的量已给**（763 = 波自己刷出的）+ §3.3 的子代明细。

### 3.2 逐波表（**这一张是用户口径的关键表**）

波 = 一个时间轴 routine（`main` 内联块按 180 帧切 / 每个 `@MainSubNN() async`）。
「type3」列含该波刷出的敌机再放出的子代（绿妖精），因为那才是屏幕上真的绿妖精。

**一面**

| 波 | 帧区间 | 本波直接刷敌 | type 3 | 绿妖精的来路 |
| --- | --- | ---: | ---: | --- |
| `main#1` | 160..340 | 10 | 1 | Ecl_GGirl00×1 |
| `main#2` | 640..1880 | 46 | 7 | Ecl_GGirl00×1, GGirl00×5, Girl01×1 |
| `main#3` | 2080..2520 | 36 | 2 | GGirl00×2 |
| `main#4` | 2920..4480 | 62 | 2 | Girl01×2 |
| `main#5` | 4880..4880 | 1 | 0 | — |
| `MainSub (main@1680)` | 20..300 | 15 | 2 | GGirl00B×2 |

**二面**

| 波 | 帧区间 | 本波直接刷敌 | type 3 | 绿妖精的来路 |
| --- | --- | ---: | ---: | --- |
| `main#1` | 0..290 | 16 | 0 | — |
| `main#2` | 92..1728 | 27 | 11 | GGirl00C×4, GSpCir00×4, GSpCir01×3 |
| `main#3` | 2974..4964 | 71 | 11 | GGirl00D×11 |
| `main#4` | 60..5864 | 19 | 1 | GGirl00B×1 |
| `MainSub01 (main@830)` | 0..90 | 10 | 0 | — |
| `MainSub02 (main@5164)` | 0..270 | 10 | 0 | — |
| `MainSub02 (main@5534)` | 0..270 | 10 | 0 | — |

**三面**

| 波 | 帧区间 | 本波直接刷敌 | type 3 | 绿妖精的来路 |
| --- | --- | ---: | ---: | --- |
| `main#1` | 0..0 | 1 | 0 | — |
| `main#2` | 220..280 | 3 | 0 | — |
| `main#3` | 1080..1080 | 2 | 0 | — |
| `main#4` | 1280..1760 | 12 | 2 | GGirl00×2 |
| `main#5` | 2480..2600 | 4 | 0 | — |
| `main#6` | 2860..2860 | 1 | 0 | — |
| `main#7` | 60..3805 | 5 | 4 | Girl02×4 |
| `main#8` | 4005..5285 | 58 | 10 | GSpCir00×8, Girl02×2 |
| `main#9` | 5485..5485 | 2 | 2 | Girl02×2 |
| `main#10` | 5885..5885 | 1 | 0 | — |
| `MainSub01 (main@160)` | 10..320 | 32 | 8 | GGirl00B×8 |
| `MainSub02r (main@1580)` | 10..80 | 8 | 1 | GGirl00B×1 |
| `MainSub02 (main@1670)` | 10..80 | 8 | 1 | GGirl00B×1 |
| ``MainSub04` / `MainSub04r` ×16` | 10..80 | 128（每个 8） | 0 | — |

**四面**

| 波 | 帧区间 | 本波直接刷敌 | type 3 | 绿妖精的来路 |
| --- | --- | ---: | ---: | --- |
| `main#1` | 0..0 | 1 | 0 | — |
| `main#2` | 60..8600 | 1 | 0 | — |
| `MainSub1 (main@0)` | 30..3350 | 21 | 7 | GGirl00×5, Girl02×2 |
| `MainSub4 (main@0)` | 3600..8328 | 1 | 18 | GGirl00×10, GGirl01×5, Girl02×3 |

**五面**

| 波 | 帧区间 | 本波直接刷敌 | type 3 | 绿妖精的来路 |
| --- | --- | ---: | ---: | --- |
| `main#1` | 0..0 | 1 | 0 | — |
| `MainSub1 (main@0)` | 160..3563 | 58 | 16 | GCir00×6, GGirl00×1, GGirl00L×1, GGirl02×4, Girl01×4 |
| `MainSubMBoss (main@0)` | 3800..6120 | 1 | 21 | GCir01×6, GCir02×8, GGirl04×4, Girl01×2, Girl03×1 |

**六面**

| 波 | 帧区间 | 本波直接刷敌 | type 3 | 绿妖精的来路 |
| --- | --- | ---: | ---: | --- |
| `main#1` | 0..0 | 1 | 0 | — |
| `MainSub1 (main@0)` | 160..1350 | 42 | 14 | GCir00×6, GCir01×4, GCir01b×4 |
| `MainSubBoss (main@0)` | 3669..3669 | 1 | 0 | — |

**EX**

| 波 | 帧区间 | 本波直接刷敌 | type 3 | 绿妖精的来路 |
| --- | --- | ---: | ---: | --- |
| `main#1` | 0..0 | 1 | 0 | — |
| `MainSub1 (main@0)` | 300..2880 | 36 | 36 | GCir00×18, GCir00b×18 |

波数 54
每波 type3 只数分布 {0: 33, 1: 4, 2: 5, 4: 1, 7: 2, 8: 1, 10: 1, 11: 2, 14: 1, 16: 1, 18: 1, 21: 1, 36: 1}
type3 总只数 177；其中 boss 段 46

**「一波几只」的分布**（共 **54** 个波，含 `main` 内联块）：

| 每波 type 3 只数 | 0 | 1 | 2 | 4 | 7 | 8 | 10 | 11 | 14 | 16 | 18 | 21 | 36 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 波数 | 33 | 4 | 5 | 1 | 2 | 1 | 1 | 2 | 1 | 1 | 1 | 1 | 1 |

⇒ 61% 的波**一只都没有**；非零的 21 个波里 **9 个是 1–2 只**，其余 12 个是「连续放一小串」的波
（最大的三个：EX 的 `MainSub1` 36 只、五面 boss 段 21 只、四面 boss 段 18 只）。
**boss 段合计吃掉了 46/177 只（26%）**，其余 131 只在道中。

### 3.3 逐敌：四面起的「运输机 → 妖精」两层结构（🟡 只数与帧号可信度不同）

四/五/六/EX 面的绿妖精**大多不是波直接刷的**，而是由一只「运输机」敌机在自己脚本里再 `enmCreate` 出来的：

| 关 | 运输机子程序 | 它体内的站点（行） | `$I0` 分支 → 颜色 | 该关绿妖精来源 |
| --- | --- | --- | --- | --- |
| 四面 | `DiveEnemy00` | 1222 | `$I0==2` → `GGirl00` | `MainSub2` 的 `$I0 = 2` 段（5 只） |
| 四面 | `DiveEnemy00B` | 1263 | 同上 | `MainSub5`（10 只） |
| 四面 | `DiveEnemy01B` | 1345 | → `GGirl01` | `MainSub7`（5 只） |
| 四面 | `DiveEnemy02(B)` | 1377 / 1404 | → `Girl02`（体内写 3） | 各 1 / 3 只 |
| 五/六/EX | `GCir0*` / `GGirl0*` | 见 §2.3 表 | 波直接刷 | — |

一手原文（四面 `stage04.ecl`，`$I0 = 2` 那段）：

```asm
; MainSub2，行 2156 起：5 只 DiveEnemy00 → 5 只绿妖精
[-9985] = 2;                                                    ; $I0 = 2 ⇒ 绿
ins_270("DiveEnemy00", 128.0f, -512.0f, -2.0f, 60, 1000, 1);    ; ×5（行 2158/2160/2162/2164/2166）
[-9985] = 1;                                                    ; 接下来 5 只是蓝
```

⇒ **ZUN 的写法就是「一波里挑 1–2 只把 `$I0` 改成 2」**，这与用户的预期一致。

### 3.4 只数是**怎么算出来的**（口径写全，便于复算/反驳）

1. 取 `thecl -d 10 -x` 的裸 dump（`ins_N` 形式，**不带 eclmap**）。
2. 用一条「只认整数/浮点赋值 + `if/unless … goto` + 标签」的具体解释器跑每个时间轴 routine：
   - 循环由 `$C = 0 … $C = $C + 1 … if ($C < 9) goto L` 与 `if ($A--) goto L` 两种惯用法自然展开；
   - `+N: //M` 记帧；`@Sub(a,b)` 把实参写进被调者局部 `$A/$B`；
   - `enmCreate*` 记一次刷敌，并把**当前 8 个参数槽**（`[-9985]`…`[-9978]`）传给新敌人（§1.5）。
3. 对被刷出的敌人，若它体内还有 `enmCreate*`，按上面同法再展开一层（最多 4 层，带记忆化与防环）。
4. 「有效主掉落」= 体内最后一次 `dropMain` 的值；体内没有 ⇒ 创建点第 6（或 270/271 的第 7）实参。
5. `!E/!N/!HL` 这类 rank 块也解析，但本文数字是**全 rank 合并**（实测：全部 type3 站点都在全 rank 分支上，
   所以 E/N/H/L 四档的绿妖精只数相同）。

**已知误差（🟡，如实登记）**：

| # | 误差 | 方向 | 影响 |
| --- | --- | --- | --- |
| ① | 条件里出现运行时量（`[-9959]` 难度、`[-9987.0f]` 随机）时按「该分支不成立」处理 | 可能**少算** | 多出现在 boss 的弹幕辅助敌机，不影响 type3 站点（已逐条人工核对，§3.3 表） |
| ② | 嵌套子波的**帧号**是「父创建帧 + 子相对帧」累加 | 帧号偏大/偏小 | 帧号仅供参考；**只数不受影响** |
| ③ | `main` 内联块的切块阈值 180 帧 | 只影响「块」这一层归类 | 每个 type3 站点的绝对帧号已在 §3.2 留出（可重切） |
| ④ | 未做贴图像素取样 | 只影响「颜色」这一层 | 「哪一族掉 type 3」是 ECL 字面值，不受影响 |

### 3.5 「纯信仰点掉落」的敌人在 TH10 里**稀有还是普遍**？（只给数字）

| 口径 | 数字 |
| --- | ---: |
| 全作掉大信仰点的敌人数（6 面 + EX） | **177 只** |
| 每关平均 | **25.3 只/关**（6 面 141 只，EX 36 只） |
| 不含 boss 段的道中合计 | **131 只**（占 74%） |
| 每关 type3 站点 ÷ 该关全部敌人站点 | **9.9%**（175 / 1774） |
| 每波的 type3 只数（54 波） | **中位 0 只**；61% 的波是 0；非零波的常见值 **1–2 只** |
| 单次 `enmCreate` 刷出的绿妖精只数 | **1 只（177/177 = 100%）** |
| 单只绿妖精给的信仰（E/N 难度） | **+5000 显示单位**（一面绿妖精 ≈ 满火力 50 颗小信仰点） |

⇒ **结论：「纯信仰点掉落」的敌人是「稀有但不罕见」**：同一波里通常 **0 只，出现时 1–2 只**；
一关累计 **14–37 只**（EX 最密）；相对全部敌人只占 **约 10% 的站点**。
**用户的预期（「一波改一两只」）与原作实测完全一致**；若要照搬密度，
应选**道中波里 1–2 个站点**改成绿妖精，而不是整波改。

### 3.6 ★★ 绿妖精在队形里的**位置与运动**（一手；对卡 110 的「补刷一只」直接可用）

这一节回答 110 的实测现象「**绿色妖精队列错误**」（`SINGLE-CARD-TEST.md` §th10 #2）：
原作里那只绿的**排在队列的哪一格**、**怎么动**、**跟谁走**。

**① 位置 = 循环的「下一格」（ZUN 自己就是「给循环 +1」）** —— 一面道中的两处同形写法：

```asm
; dat/stage01.ecl `main`（裸 dump 的 ECL 偏移）
% B = _f(-144) ; $C = 0 ; goto main_572
main_176:
   unless (($C % 2) == 0) goto main_368
   ins_257("Ecl_BGirl00", %B, -16.0f, 20, 1000, 2)   ; 0x9da4  偶数只 = 蓝
   goto main_420
main_368:
   ins_257("Ecl_RGirl00", %B, -16.0f, 20, 1000, 1)   ; 0x9df0  奇数只 = 红
main_420:
   %B ; +20 ; 32 ; ins_51() ; ins_45(%B)             ; 0x9e4c/0x9e5c  %B += 32
   $C = $C + 1
main_572:
   if ($C < 9) goto main_176                          ; 9 只：x = -144, -112, … , +112
   ins_257("Ecl_GGirl00", %B, -16.0f, 20, 1000, 3)    ; ★ 0x9f0c 第 10 只 = **绿的**，x = %B = +144
```

镜像的那一处（`%B = _f(144)`、每轮 `ins_53()` = `subf` 32、9 只从 +144 递减到 −112）：

```asm
   if ($C < 9) goto main_1832
   ins_257("Ecl_GGirl00", %B, -16.0f, 20, 1000, 3)    ; ★ 0xa584 绿的排在 **−144**（下一格）
```

⇒ **两条硬结论**（✅ 一手，ECL 文本 + 逐个偏移可复算）：

| # | 结论 |
| --- | --- |
| ① | 绿妖精**就排在循环的下一格**：`%B` 在循环结束后正好是「还没被用掉的那一格」（间距 **32**、y 与全队同为 **−16**）⇒ **它就是「循环 +1」的那一只**，不是另找地方塞进去的 |
| ② | 它用**与同队成员完全相同的入场与运动**（同一个 `Ecl_Girl00` 体的兄弟子程序 —— 见下），**没有任何跟随/追人逻辑**：跟随这件事在 TH10 里不需要，因为它本来就是队形成员 |

**② 运动 = 同族子程序**（一手：`stage01.ecl` 的 `Ecl_*Girl00` 族，`thecl -d 10` 裸 dump）：

| 子程序 | ECL 偏移 | 外观 | 运动 |
| --- | --- | --- | --- |
| `Ecl_BGirl00` | `0x4f74` | `anmSelect(1); anmSetSprite(1, 45)` | `@Ecl_Girl00(0)` |
| `Ecl_RGirl00` | —— | `anmSetSprite(1, 46)` | `@Ecl_Girl00(5)` |
| **`Ecl_GGirl00`** | **`0x4ffc`** | **`anmSetSprite(1, 47)`** | **`@Ecl_Girl00(10)`** |
| `Ecl_Girl00(A)` | `0x5084` | `anmSetMain(0, $A)`（颜色就是那个参数） | `flagSet(32)` → `setHurtbox(24,24)` / `setHitbox(16,16)` → **`moveVel(%ANGLE_PLAYER, 7.0f)`** → `moveVelTime(60, 4, %ANGLE_PLAYER, 0.0f)` → `nop()` → `@Ecl_Girl00_at()`（发弹） |

（同一关里另一支 `Girl00(A)`（`GGirl00` 一族用）写的是 `moveVel(π/2, 2.7f)` 再 `moveVelTime(60,0,0,−999999)` ⇒ 直着往下、60 帧后停 —— 两族是「朝自机冲」与「直落」两种，别混。）

⇒ **对卡 110 的意义**：我们的载体**不在原生的 `for` 循环里**（原生 ECL 是零售字节，改不得），
所以「复刻」只能落在**语义等价**上：**补在队形外侧的下一格、与同波妖精同轨迹**。
2026-09-18c 选的「跟随同波那一只（目标坐标 + 出生时队形偏移）」正是这条语义的实现；
**它必须在补刷之后每帧都写**，否则那只绿妖精会钉在出生点上（= 用户报的「队列错误」，
根因见 `mods/th18.v1.00a/card-expand/native/cards/mof.c` 的 `mof_ge_follow_tick()` 长注）。

---

## 4. 其它把信仰推上去的来源（同样给分布，不只列机制）

### 4.1 敌人 ECL 侧：**除 type 3 之外没有任何信仰点声明**（一手穷举）

| 关 | `dropMain(1)` P点 | `dropMain(2)` 蓝点 | `dropMain(4)` 大P | `dropMain(0)` 无掉落 | `dropMain(3)` 大信仰 | `dropMain(7)` 1UP |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 一面 | 5 | 5 | 0 | 1 | 5 | 0 |
| 二面 | 6 | 6 | 0 | 1 | 6 | 0 |
| 三面 | 6 | 6 | 0 | 5 | 6 | 0 |
| 四面 | 4 | 4 | 0 | 5 | 5 | 0 |
| 五面 | 7 | 7 | 0 | 8 | 7 | 1 |
| 六面 | 4 | 4 | 0 | 4 | 4 | 0 |
| EX | 7 | 7 | 0 | 6 | 7 | 1 |
| **合计** | **39** | **39** | **0** | **30** | **40** | **2** |

**全作 `dropMain` 只出现 `0/1/2/3/7`**：没有 `8`（微信仰点 +10）、没有 `9`（小信仰点 +100）、没有 `5`。
`dropExtra` 的穷举（46 个 type-1 站点共 482 颗 P 点、49 个 type-2 站点共 392 颗蓝点）：

| 关 | `dropExtra(1,n)` 站点 | Σn | `dropExtra(2,n)` 站点 | Σn | `dropExtra(4,n)` | `dropExtra(7,n)` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 一面 | 4 | 13 | 4 | 15 | 1 | 0 |
| 二面 | 5 | 20 | 5 | 18 | 1 | 0 |
| 三面 | 5 | 32 | 5 | 19 | 0 | 1 |
| 四面 | 4 | 36 | 4 | 28 | 2 | 0 |
| 五面 | 5 | 49 | 7 | 39 | 0 | 0 |
| 六面 | 6 | 106 | 7 | 60 | 1 | 0 |
| EX | 17 | 226 | 17 | 213 | 1 | 0 |
| **合计** | **46** | **482** | **49** | **392** | — | — |

⇒ **`dropExtra` 也没有一处 type 3/8/9**（只有 1/2/4/7）。extra 掉落集中在 boss / 中 boss 身上
（EX 面 17+17 个站点、Σn 226+213），是「boss 死时撒一大把道具」的写法。

### 4.2 引擎侧：满火力时 P 点被改写成**小信仰点**（这是实战里最大的信仰水龙头）

一手（`ItemManager::spawn_item` `0x41bb00`，与 [`01-faith-and-points.md`](01-faith-and-points.md) §3.6 同源）：

```
0x41bbf7: cmp WORD PTR ds:0x474c48,0x64   ; 火力 = 100 单位 = 5.00（满）
0x41bc09: movzx ecx,BYTE PTR [eax+0x41be28]  ; 索引表，下标 = type−1
0x41bc10: jmp DWORD PTR [ecx*4+0x41be1c]
索引表 0x41be28 = [00,02,02,00,02,02,02,02,02,01,01]
  0x41bc1e ⇒ type = 9（小信仰点，+100 显示信仰）
  0x41bc17 ⇒ type = 5（点道具）
```

读表：**满火力时 type 1 / 4（P 点、大 P）→ type 9（小信仰点，+100 显示信仰）**；
type 10 / 11 → type 5（点道具）。⇒ **39 个声明 `dropMain(1)` 的敌人 + 46 个 `dropExtra(1,n)` 站点（482 颗 P 点）
在满火力时就变成 482+ 颗小信仰点**，这才是 TH10 信仰的主要来源，而**大信仰点只有 177 颗**。

🟡 一处未闭环：`dropMain` / `dropExtra` 的 handler（`0x40fed7`–`0x40ff08`）与出生后改写（`0x40d0c7`）
都有一条「若 `enemy+0x2480` 的 **bit 15** 置位，则 1→10、4→11」的分支；但 `flagSet`（`0x40fd44`）是**按位或**，
而 7 个关卡 ECL 里 `flagSet` 的实参最大是 **148（0x94）**，没有任何一处设 bit 15；
`enemy+0x2480` 的全部写入点（`0x40d024` / `0x40d0bc` / `0x40d782` / `0x40e6f4` / `0x41286b` / `0x412928`）
也没有一处置 bit 15。⇒ **该分支在实战中疑似死代码**，不影响 type 3（3 从不被改写）。

### 4.3 其它信仰来源（一律给一手地址，分布则无「敌人」维度）

| 来源 | 一手 | 与敌人分布的关系 |
| --- | --- | --- |
| 大信仰点本身 | `0x41b57f` + 难度表 `0x41b8cc`（{`0x41b595`,`0x41b595`,`0x41b59c`,`0x41b5a3`,`0x41b5a3`}）⇒ E/N +5000、H +8000、L/EX +10000（显示单位） | **= 本文 §3 的 177 只**，7 关合计显示信仰 +177×5000（N）≈ **88.5 万** |
| 点道具（type 2，蓝点）线上得分 = 信仰 | `0x41b4bd` 读 `0x474c4c` → `0x41b569 call 0x409d90` | **39 个声明 `dropMain(2)` 的站点 + 49 个 `dropExtra(2,n)` 站点（392 颗）**；它**不直接加信仰**，而是「分数 = 信仰」 |
| 小信仰点（type 9，+100） | `0x41b5fc` | **敌人不掉**；只由 §4.2 的满火力改写产生（482+ 颗 P 点的来源） |
| 微信仰点（type 8，+10） | `0x41b5d6` | **全作没有任何声明点**（`dropMain`/`dropExtra` 里都没有 8） |
| 符卡奖励 | `0x409874`：`(3×stage + 10) × 信仰`，上限 `0x5f5e100` | 逐关 stage 系数固定（一面 13、六面 28、EX 31），与敌人分布无关 |
| 关卡结算 | `0x416c70`：显示分 `+= 信仰 × 100` | 同上 |
| 掉信仰的「反方向」 | miss `0x4269d0`、非决死灵击 `0x425a15`（−3000）、Boss 阶段超时 `0x412932`（−30000）、连击槽空逐帧 `0x418a17` | 见 [`01-faith-and-points.md`](01-faith-and-points.md) §3.2 |

---

## 5. 结论（可信度 + 版本）

| # | 断言 | 可信度 | 版本 |
| --- | --- | --- | --- |
| 1 | ECL 主掉落 type 存在 `enemy+0x2408`；`dropMain`（opcode 330）与 `enmCreate*` 第 6 实参写同一格，**体内后写者生效** | ✅一手 | v1.00a |
| 2 | 死亡时主掉落 type > 0 ⇒ 恰好掉 **1 颗**该 type 道具（`0x40e63e`，角度 −π/2、速度 2.6） | ✅一手 | v1.00a |
| 3 | opcode 270/271 是 3 坐标变体，item = **第 7** 实参（否则把 life 当 item） | ✅一手（hexdump + handler） | v1.00a |
| 4 | `$I0..$I3`/`%F0..%F3` = `enemy+0x1138..0x1157`，**创建时从刷出者继承** | ✅一手（跳表 `0x412228` + `0x40e954`） | v1.00a |
| 5 | **TH10 掉大信仰点的敌人只有绿妖精一族**：7 关 7 次「`B*=2 / R*=1 / G*=3 / Y*=0`」零例外 | ✅一手（ECL 文本） | v1.00a |
| 6 | 全作 type3 声明点 40、创建站点 175、**实际刷出 177 只**（一面 14 / 二面 23 / 三面 28 / 四面 25 / 五面 37 / 六面 14 / EX 36） | 🟡（只数由自写解释器展开，口径见 §3.4） | v1.00a |
| 7 | **177 次出现全是 n=1**：原作**从不批量刷**绿妖精；一波非零典型值 1–2 只 | 🟡（同 6；但「n=1」是逐站点字面值，接近 ✅） | v1.00a |
| 8 | 敌人 ECL 里**没有** type 8 / 9 的任何声明；`dropExtra` 只用 1/2/4/7 | ✅一手（穷举） | v1.00a |
| 9 | 满火力时 P 点（type 1/4）→ 小信仰点（type 9，+100）：**实战信仰的主要来源** | ✅一手（跳表 + 索引表原字节） | v1.00a |

---

## 6. 还缺什么 / 🟡

1. **「颜色」没有像素级验证** —— 本文说 `G*` 是绿妖精，依据是子程序命名 + `anmSetMain(0,10)` / sprite 47。
   要坐实需按 [`research-110-green-fairy-host.md`](../../../local/_scratch/research-110-green-fairy-host.md)
   的办法对 `enemy.anm` 取样 HSV。**但这不影响 §3 的只数**（那是 ECL 字面值 `dropMain(3)`）。
2. **敌人总数**（分母）没有给全 —— 需要把「由敌人再刷出的子代」和 boss 弹幕辅助敌机的运行时循环全部展开；
   本文只给了「波自己刷出的 763 只」。要做的话建议加一条「以运行时变量为界的区间分析」，取 min/max 两界。
3. **帧号**在嵌套子波里是累加近似（§3.4 误差②）—— 只数不受影响，但「绿妖精出现在第几秒」这类结论要慎用。
4. `enemy+0x2480` bit 15 那条 drop 改写分支**未找到设置点**（§4.2 🟡）。
5. EX 面的 `GCir00` 有 18 只（`MainSub1` 直刷 12 + 子波 6），是全文最密的道中波；
   未核对这是否与 Extra 的难度设计有关（可能只是 Extra 全程「绿妖精雨」）。

---

## 附录 A · 复现命令

```bash
cd /home/nipporita/renkolab
md5sum local/th10.v1.00a/th10.exe        # 7dc488d82c81dd4aee4ba098b8804d83
objdump -d -M intel local/th10.v1.00a/th10.exe > local/_scratch/th10_full.asm

# 1) ECL 全量反编译（★ 不要加 -m：eclmap 里没有 th10.eclm）
mkdir -p /tmp/th10ecl
cd local/th10.v1.00a/dat
for f in default stage01 stage02 stage03 stage04 stage05 stage06 stage07; do
  ../../../local/vendor/thtk/build/thecl/thecl -d 10 -x $f.ecl /tmp/th10ecl/$f.txt
done
md5sum /tmp/th10ecl/stage01.txt   # 与 local/_scratch/th10/decomp/stage01_raw.txt 逐字节相同

# 2) opcode 号 → handler → 名字（一手分派表 × ExpHP labels）
python3 - <<'PY'
import struct, json
d = open('local/th10.v1.00a/th10.exe','rb').read()
rva = lambda v: d[v-0x400000-0xc00:]                 # .text：RVA 0x1000 / raw 0x400
idx, jt = rva(0x411ee0), rva(0x411d30)
lab = {int(a,16): n for a, n in json.load(open(
    'local/vendor/th-re-data/data/th10.v1.00a/labels.json'))['ecl']}
for op in (256,257,260,261,265,266,267,268,270,271,326,327,328,329,330):
    i = idx[op-0x100]
    h = struct.unpack('<I', jt[4*i:4*i+4])[0]
    print(op, hex(h), lab.get(h))
PY

# 3) 静态扫描：所有 type3 站点 + 40 个 dropMain(3) 声明点
python3 - <<'PY'
import re, glob, os
sub_re = re.compile(r'^void\s+(\w+)\s*\(')
ins_re = re.compile(r'/\*\s*[0-9a-f]+\s*\(\+\s*[0-9a-f]+\)\s*\*/\s*ins_(\d+)\((.*)\);')
CREATE = {256,257,260,261,265,266,267,268,270,271}
for fp in sorted(glob.glob('/tmp/th10ecl/*.txt')):
    cur, decl = None, []
    for ln, line in enumerate(open(fp, encoding='utf-8', errors='replace'), 1):
        m = sub_re.match(line.strip())
        if m: cur = m.group(1); continue
        im = ins_re.search(line)
        if not im: continue
        op, args = int(im.group(1)), [a.strip() for a in im.group(2).split(',')]
        if op == 330 and args and args[0] == '3':
            decl.append((cur, ln))
        if op in CREATE:
            # 只需再判一次「目标子程序体内是否有 dropMain(3)」即可得有效 type
            pass
    if decl:
        print(os.path.basename(fp), decl)
PY
```

## 附录 B · 波次与只数的复现

§3 的只数由自写 ECL 迷你解释器展开得到（临时产物，未入库）。它需要的最小语义只有四条，
按这四条重写一遍即可得到同样的数字：

1. 变量名三种写法等价：`$A` / `%B` / `[-9985]` / `[-9981.0f]`（thecl 有/无 eclmap 时输出的形式不同）。
2. 循环展开：`$X = k … $X = $X + 1 … if ($X < n) goto L`、`if ($X--) goto L`。
3. `@Sub(a,b)` 把实参写进被调者局部 `$A/$B`；`enmCreate*` 把**当前** `[-9985..-9978]`（8 格）传给新敌人（§1.5）。
4. 被刷出的敌人，如果体内还有 `enmCreate*`，按同法再展开（带记忆化 + 防环 + 深度上限 4）。

> 想彻底独立复算，最短路径是**照 §3.3 的表逐链手算**：绿色只数 = （运输机被刷的只数）×（该运输机
> 在 `$I0==2` 分支下的次数）。四面的 25 只 = `DiveEnemy00`×5 + `DiveEnemy00B`×10 + `DiveEnemy01B`×5 + `Girl02`×5。
