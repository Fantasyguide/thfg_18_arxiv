# TH11 信仰点掉落分布 —— 逐关 / 逐波 / 逐敌机全量清单（大绿点 type 3）

> **版本**：TH11 v1.00a（`th11.exe`，688,128 B，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **数据来源**：`local/th11.v1.00a/th11.dat` 解包出的 21 个 `.ecl` + `local/_scratch/th11_full.asm` 平坦反汇编。
> **方法**：① thtk `thdat -x 11` 解档 → `thecl -d 11 -r -x -j -m eclmap/th11.eclm` 反编译成带地址的指令流；
> ② 脚本按「生成站点 → 被生成子程序自身脚本的有效掉落」归因（§4）；③ 引擎侧语义逐条一手反汇编核对（§2/§3）。
> **可信度**：item type 语义、四个 ECL 掉落指令的语义、`enmCreate` 第 6 参 = 主掉落、引擎侧 type-3 来源穷举 = ✅ 一手；
> 每波敌机数 = ✅（除 `deci` do-while 的 ±1，见 §4.3 🟡）。

## 0. 一句话结论

TH11 的「信仰」就是 HUD 的**最大得点**（`0x4a56f0`，见 [01-faith-and-points.md](01-faith-and-points.md)）；
能给它加值的道具只有三种道具 type：**3（大绿点 / 本文的「大信仰点」，+100 显示）、9（中信仰点，+10）、8（小信仰点，+0.1）**。

**「掉大信仰点的敌人」在 TH11 里一点也不稀有**：全作道中 1970 只敌机里有 **471 只（24%）**掉大信仰点。
但它的分布是**双峰**的 —— 86 个波次里 62 个波**一只都没有**，其余 24 个波里 20 个是**整批**（≥5 只，最大 80 只），
另外 4 个是「本来就只有 1~2 只敌机」的单只波。
**在真正的杂兵波（≥3 只敌机，57 个）里，没有任何一个波是「洒一两只绿妖精」** —— 这与「一波改一两只」的直觉正好相反（§6）。

## 1. 先钉死 item type：哪几个是「信仰点」

### 1.1 道具类型分派表（一手）

```
th11:0x423a75: mov eax,[edi+0x468]      ; item->type
0x423a7f: dec eax                       ; 索引 = type-1
0x423a84: cmp eax,0xa / ja 0x423afe     ; type 12 及以上 → 无效果
0x423a89: jmp DWORD PTR [eax*4+0x423f00]
```

表 `th11:0x423f00`（11 项 = type 1..11，实测原值）：

| type | handler | 效果（一手） | 信仰？ |
| --- | --- | --- | --- |
| 1 | `0x423b34` | `collect_power(1)`（`0x420a00`） | ❌ 火力 |
| 2 | `0x423b7c` | 蓝点：得分 = `trunc10((交信度+擦弹) × 最大得点 / 100)` | ❌ 给分 |
| **3** | `0x423c82` | 难度跳表 `0x423f2c`（5 项全指 `0x423c98`）→ `mov ebx,0x2710` → `0x423cba call 0x424640` = **`add_to_piv(+10000)`** | ✅ **大信仰点（+100 显示）** |
| 4 | `0x423cf6` | `collect_power(ds:0x4a574c)` = 按「每一级火力」的量加（`0x423cf6 mov eax,ds:0x4a574c`） | ❌ 火力 |
| 5 | `0x423c3e` | `call 0x4209b0` = **命碎 +1**（5 碎 = 1 残，见 [research-th11-fragments-and-type8](../../../local/_scratch/research-th11-fragments-and-type8.md)） | ❌ 命碎 |
| 6 | `0x423a90` | `collect_power(MAXIMUM_POWER)` | ❌ 火力 |
| 7 | `0x423d97` | `call 0x420950`（extend/残机） | ❌ 残机 |
| **8** | `0x423cc4` | `push 0xa; call 0x424640`（+10 存储）+ `0x40ceb0(10)`（分数 +10） | ✅ **小信仰点（+0.1）** |
| **9** | `0x423cd7` | `push 0x3e8; call 0x424640` | ✅ **中信仰点（+10）** |
| 10 | `0x423b34` | 同 type 1（视觉变体：`0x4243af mov ebx,0x61`） | ❌ 火力 |
| 11 | `0x423cf6` | 同 type 4（`0x4243d5 mov ebx,0x64`） | ❌ 火力 |

⇒ **「信仰点」= type 3 / 8 / 9**；其中**「大信仰点 / 大绿点」= type 3**（`add_to_piv` 的唯一三个调用者
`0x423cba`(3) / `0x423cc6`(8) / `0x423cec`(9) 与表一致）。

> 🟡→✅ **对姊妹篇的一处读法订正**：[01-faith-and-points.md §4](01-faith-and-points.md) 写
> 「字节表 `0x424578 = [0,1,1,0,1,1,1,1,1,0,0]`，下标 = type−1，**≠0 ⇒ 改写**」。
> 实测 `0x42434a: jmp DWORD PTR [eax*4+0x424570]`，跳表 `0x424570 = {0x424351, 0x424358}`，
> 其中 `0x424351: mov ebx,9`（改写成 9）、`0x424358`（不改写）。**字节值 0 ⇒ 改写**，读法反了；
> **但结论（满火力时 type 1/4/10/11 → 9）不变**，因为 `[0,1,1,0,1,1,1,1,1,0,0]` 的 0 恰好落在 type 1/4/10/11 上。

### 1.2 满火力 / 满信仰的转化规则（一手）

| 情形 | 规则 | 证据 |
| --- | --- | --- |
| **满火力**（`0x4a56e8 >= 0x4a5748`）时**生成** P 类道具 | type **1/4/10/11 → 9（中信仰点）**。改写发生在**生成时**（`0x424230` 内）而不是吃掉时 | `0x42432d`–`0x42434a`；`0x424381: mov [ebp+0x468],ebx` |
| 满火力时生成 type 3/8/9 | **不改写**（字节表这三位都是 1） | 同上 |
| **满信仰**（`0x4a56f0` 到 `0x4a576c` 上限） | **没有任何转化**：绿点照掉照吃，只是 `add_to_piv` 把值钳在上限 | `0x424640`–`0x424658` |

⇒ 复刻含义：**在 TH11 里「满火力」本身就是一个信仰收入来源**（所有 P 点变中信仰点），但**「满信仰」不改变掉落**。

### 1.3 引擎侧（非 ECL）type 3 来源 = 零（否定结论，穷举）

`0x424230` 是唯一的道具生成函数：`ecx` = item type（`0x424233 mov ebx,ecx`），最终由 `0x424381 mov [ebp+0x468],ebx` 写进 `item+0x468`。全 exe 只有 **21 处** `call 0x424230`，下表按类别列全：

| 站点 | 传入的 type | 说明 |
| --- | --- | --- |
| `0x40b0ac`、`0x40b1d2`、`0x40b36b`、`0x40b508` | 8 | 消弹 → 小信仰点（函数 `0x40af90`，`0x40b0a2: mov ecx,0x8`） |
| `0x412862` | 掉落数组 `[esi+0x2544]` | **敌人死亡主掉落的内联版**（`0x41282c mov ecx,[esi+0x2544]` → `0x412862 call 0x424230` → `0x412867 call 0x4101d0`） |
| `0x412aec` / `0x412c2c` / `0x412df0` | 8 | 同上（另一批消弹调用点） |
| `0x424166` / `0x424190` / `0x4241ba` / `0x4241fd` | 9 | 把场上已有的 type **1/4/10/11** 道具改写成 type 9（`0x424147 cmp eax,1`、`0x424171 cmp eax,4`、`0x42419b cmp eax,0xa`、`0x4241de cmp eax,0xb`）。触发条件（道具格满？）🟡 未钉死 |
| `0x4101b7` | 掉落数组 `[esi+0]` | **敌人死亡主掉落**（值来自 ECL，见 §3） |
| `0x4102f1` | 掉落数组 `[type*4+0x1508]` | **敌人死亡附加掉落**（同上） |
| `0x4313d0` | 栈数组 `[esp+edi*4+0x3c]`，取值 **0 / 4 / 6** | 玩家死亡掉道具（`0x431340 mov [esp+0x40],0x6`、`0x431360 mov eax,4`） |
| `0x426458`、`0x4269da`、`0x426f5e`、`0x427936`、`0x427e37`、`0x42844e` | 8 | 弹幕/激光消去 → 小信仰点 |

⇒ **引擎侧永远只撒 8 / 9（以及 0/4/6），撒 type 3 的唯一途径是 ECL 的掉落数组。**
所以「大信仰点掉落站点」可以**完全**由 ECL 静态穷举（§5）。

## 2. ECL 侧四个掉落指令的语义（一手）

ECL 指令头 = `{u32 time; u16 id; u16 size; ...}`（`0x412e7e: movzx eax,WORD PTR [edi+4]`）。
扩展 opcode 分派：`0x412e85 add ecx,0xffffff00` → `0x412ea3 movzx ecx,BYTE [ecx+0x416df0]` → `0x412eaa jmp [ecx*4+0x416bd4]`。
敌人 ECL 上下文 `ctx = enemy + 0x103c`（thunk `0x412e20: add ecx,0x103c`），掉落数组在 **`ctx+0x1508`**：

| opcode | 指令 | handler | 一手语义 |
| --- | --- | --- | --- |
| 326 | `dropClear()` | `0x414d14` | `memset(ctx+0x150c, 0, 0x30)` —— **只清「附加掉落」12 格，不清主掉落 `ctx+0x1508`** |
| 327 | `dropExtra(type,n)` | `0x414d2c` | `ctx[type*4+0x1508] = n`（`0x414d68`） |
| 328 | `dropArea(a,b)` | `0x414d74` | 掉落散布半径（`ctx+0x153c/0x1540`） |
| 329 | `dropItems()` | `0x414daa` | 立即结算（`call 0x410190(ctx+0x34, ctx+0x1508)`） |
| **330** | **`dropMain(type)`** | `0x414dbd` | **`ctx[0x1508] = type`（`0x414df2`）** |
| 331 | `lifeSet(hp)` | `0x414dfd` | HP |
| 332 | `setBoss(n)` | `0x414e8f` | 置 `ctx+0x1580` 的 `0x80000` 位（`0x414ed6`）并把 ctx 登记进 `[0x4a8d7c]+0x1c+n*4` |

**死亡结算**（两处等价实现）：`0x410190` = `if (drop[0] > 0) spawn_item(type=drop[0])`（`0x4101b7`）→ `call 0x4101d0`
遍历 `drop[1..12]` 各撒 `drop[t]` 个 type-t 道具；敌人死亡路径里同一段代码内联在 `0x41282c`–`0x412867`。

**`enmCreate*` 的第 6 参 = 被生成的敌人的主掉落**（一手链）：

```
0x412ec1 (op 256 enmCreate) 里 push 5; call 0x417df0 → [esp+0x2a4] = 第6参
0x413015: mov [esp+0x2a4],eax            ; 参数结构体 base = esp+0x294 ⇒ 第6参落在 struct+0x10
0x41303f: call 0x4108f0                  ; Enemy 生成
0x410955: mov edx,[ebx+0x10]             ; struct+0x10
0x410958: mov [ebp+0x2544],edx           ; 新敌人 enemy+0x2544 == ctx+0x1508 == 主掉落  ★
```

⇒ **enmCreate 第 6 参与 `dropMain` 写的是同一个字段**；子程序自己再 `dropMain(...)` 会覆盖它。

**`0x80000`（`setBoss`）对掉落的影响**：`dropMain` / `dropExtra` / 生成时的 type 只在
`1 → 10`、`4 → 11` 上被改写（`0x414dd3`–`0x414ded`、`0x414d44`–`0x414d53`、`0x410a32`–`0x410a4d`）——
**全是 P 点，永远不动 type 3**。所以本文的统计与 Boss 标记无关。

### 2.1 TH11 ECL 里 `dropMain(N)` 的 N 取值表

| N | 含义 | 出现次数（全 21 个 ecl） |
| --- | --- | --- |
| 0 | 不掉主道具 | 30 |
| 1 | P 点（小）；满火力时 → type 9 | 41 |
| 2 | 蓝点（点道具） | 72 |
| **3** | **大信仰点（大绿点）** | 38 |
| 4 | P 点（大）；满火力时 → type 9 | 0（本作未用；`dropExtra(4,·)` 有 4 处） |
| 5 | 命碎道具（每个 = 1/5 残机） | 82 |
| — | `dropExtra` 只用过 type 1 / 2 / 3 / 4 | — |

`enmCreate*` 第 6 参的实际分布：`2`×487、`0`×200、`1`×71、**`3`×12**。

## 3. 掉落站点全量清单

口径：**一个「站点」= 一条会写掉落数组的指令**：`dropMain(N)`、`dropExtra(T,n)`、`enmCreate*(..., 第6参)`。
「实际撒出 type 3」= 被生成子程序自己的脚本（含同步 `call` 链）里有 `dropMain(3)`/`dropExtra(3,n)`，
或该子程序**完全没写掉落**时回落到 `enmCreate` 第 6 参 = 3。
（`dropClear` 不清主掉落，所以它**不会**取消 `enmCreate` 第 6 参 —— 这一条对 §5 的计数影响很大。）

### 3.1 `dropMain(3)` / `dropExtra(3,n)` 站点（46 处，一手地址）

| ECL 文件 | 子程序 | 地址 | 行号 | 指令 | 本文件内被 `enmCreate` 引用次数 |
| --- | --- | --- | --- | --- | --- |
| `stage01.ecl` | `GGirl00` | `0x4efc` | 940 | `dropMain(3)` | ❌ 0（死代码） |
| `stage01.ecl` | `GGirl02` | `0x4fb0` | 951 | `dropMain(3)` | ✅ 21 |
| `stage01.ecl` | `GGirl03` | `0x5050` | 961 | `dropMain(3)` | ❌ 0（死代码） |
| `stage01.ecl` | `GGirl05` | `0x50e8` | 971 | `dropMain(3)` | ✅ 14 |
| `stage02.ecl` | `GCir00` | `0x720c` | 1431 | `dropMain(3)` | ❌ 0（死代码） |
| `stage02.ecl` | `GCir01` | `0x72ac` | 1441 | `dropMain(3)` | ❌ 0（死代码） |
| `stage02.ecl` | `GCir01B` | `0x734c` | 1451 | `dropMain(3)` | ❌ 0（死代码） |
| `stage02.ecl` | `GCir02` | `0x73ec` | 1461 | `dropMain(3)` | ❌ 0（死代码） |
| `stage02.ecl` | `GCir02B` | `0x748c` | 1471 | `dropMain(3)` | ❌ 0（死代码） |
| `stage02.ecl` | `GGirl00` | `0x752c` | 1481 | `dropMain(3)` | ❌ 0（死代码） |
| `stage02.ecl` | `GGirl01` | `0x75cc` | 1491 | `dropMain(3)` | ❌ 0（死代码） |
| `stage02.ecl` | `GGirl01B` | `0x766c` | 1501 | `dropMain(3)` | ❌ 0（死代码） |
| `stage02.ecl` | `GGirl02` | `0x770c` | 1511 | `dropMain(3)` | ❌ 0（死代码） |
| `stage02.ecl` | `GGirl03` | `0x77ac` | 1521 | `dropMain(3)` | ✅ 4 |
| `stage03.ecl` | `GCir00` | `0xa928` | 2143 | `dropMain(3)` | ❌ 0（死代码） |
| `stage03.ecl` | `GCir01` | `0xa9b8` | 2152 | `dropMain(3)` | ❌ 0（死代码） |
| `stage03.ecl` | `GCir02` | `0xaf0c` | 2233 | `dropMain(3)` | ❌ 0（死代码） |
| `stage03.ecl` | `GCir03` | `0xaf88` | 2241 | `dropMain(3)` | ❌ 0（死代码） |
| `stage03.ecl` | `GGirl00` | `0xb018` | 2250 | `dropMain(3)` | ❌ 0（死代码） |
| `stage04.ecl` | `GCir00` | `0x6718` | 1282 | `dropMain(3)` | ❌ 0（死代码） |
| `stage04.ecl` | `GCir01` | `0x67a8` | 1291 | `dropMain(3)` | ❌ 0（死代码） |
| `stage04.ecl` | `GCir01b` | `0x6848` | 1301 | `dropMain(3)` | ❌ 0（死代码） |
| `stage04.ecl` | `GCir02` | `0x68e8` | 1311 | `dropMain(3)` | ✅ 30 |
| `stage04.ecl` | `GCir03` | `0x6974` | 1320 | `dropMain(3)` | ❌ 0（死代码） |
| `stage04.ecl` | `GGirl00` | `0x6a04` | 1329 | `dropMain(3)` | ❌ 0（死代码） |
| `stage04.ecl` | `GGirl00b` | `0x6aa4` | 1339 | `dropMain(3)` | ❌ 0（死代码） |
| `stage04.ecl` | `GGirl01` | `0x6b44` | 1349 | `dropMain(3)` | ❌ 0（死代码） |
| `stage04.ecl` | `GGirl01b` | `0x6be4` | 1359 | `dropMain(3)` | ❌ 0（死代码） |
| `stage04.ecl` | `GGirl02` | `0x6c84` | 1369 | `dropMain(3)` | ❌ 0（死代码） |
| `stage04.ecl` | `GGirl03` | `0x6d24` | 1379 | `dropMain(3)` | ❌ 0（死代码） |
| `stage05.ecl` | `GCir00` | `0xec8` | 162 | `dropMain(3)` | ❌ 0（死代码） |
| `stage05.ecl` | `GCir01` | `0xf44` | 170 | `dropMain(3)` | ❌ 0（死代码） |
| `stage05.ecl` | `GCir02` | `0xfc0` | 178 | `dropMain(3)` | ❌ 0（死代码） |
| `stage05.ecl` | `GPhantom02` | `0x1198` | 207 | `dropMain(3)` | ✅ 10 |
| `stage05boss.ecl` | `GBoss1Girl` | `0xa1b4` | 2083 | `dropMain(3)` | ❌ 0（死代码） |
| `stage06.ecl` | `Crow01` | `0x8f8` | 95 | `dropExtra(3, 2)` | ✅ 2 |
| `stage06.ecl` | `Crow02` | `0xd88` | 156 | `dropExtra(3, 8)` | ✅ 2 |
| `stage07.ecl` | `GGirl00` | `0x4ac` | 37 | `dropMain(3)` | ❌ 0（死代码） |
| `stage07.ecl` | `GGirl03` | `0x54c` | 47 | `dropMain(3)` | ❌ 0（死代码） |
| `stage07.ecl` | `GGirl05` | `0x5e4` | 57 | `dropMain(3)` | ❌ 0（死代码） |
| `stage07.ecl` | `Girl01` | `0x930` | 106 | `dropExtra(3, 10)` | ✅ 19 |
| `stage07.ecl` | `Girl01b` | `0xa8c` | 126 | `dropExtra(3, 10)` | ✅ 2 |
| `stage07.ecl` | `Girl02` | `0xbe8` | 146 | `dropExtra(3, 10)` | ✅ 1 |
| `stage07.ecl` | `Girl04` | `0xebc` | 189 | `dropExtra(3, 6)` | ✅ 10 |
| `stage07.ecl` | `Girl06` | `0x1190` | 232 | `dropExtra(3, 6)` | ✅ 9 |
| `stage07.ecl` | `Girl07` | `0x1344` | 257 | `dropExtra(3, 10)` | ❌ 0（死代码） |

（`stage4c00a/b/c`、`stage4c01a/b/c`、`default.ecl`、全部 boss/mboss 文件里**没有任何 type-3 掉落指令**。）

### 3.2 `enmCreate*(..., 3)` 站点（12 处，第 6 参 = 3）

| ECL 文件 | 生成者 | 地址 | 行号 | 目标子程序 | 说明 |
| --- | --- | --- | --- | --- | --- |
| `stage01.ecl` | `MainSub03` | `0xadd0` | 2184 | `RockS` | 小岩石，1 只/次 |
| `stage01.ecl` | `MainSub04` | `0xae88` / `0xaecc` | 2196/2198 | `Girl04` | 同时 `dropExtra(2,4)` |
| `stage01.ecl` | `MainSub07` | `0xc454` / `0xc484` | 2451/2452 | `Girl06` | 同时 `dropExtra(2,4)` |
| `stage01.ecl` | `MainSub08` | `0xc7c8` | 2497 | `RockS` | **`MainSub08` 从没被 `main` 调用（死代码）** |
| `stage01.ecl` | `MainSub09` | `0xd0e4` | 2614 | `Girl06` | |
| `stage01.ecl` | `MainSub10` | `0xdfd8` | 2806 | `Girl06` | |
| `stage02.ecl` | `BossCard2` | `0x407c` | 789 | `BossCard2Dummy` | 首领战中，非道中 |
| `stage03.ecl` | `BossCard2` | `0x471c` / `0x48c0` / `0x4924` | 828/850/853 | `BossCard2Dummy` / `BossCard2Dummy2` | 首领战中 |

### 3.3 ECL 侧「小信仰点（type 8）」来源

ECL 撒 type 8 只有一条路：VM 的 4 个「消弹」op 调 `0x40af90`（弹幕区域内消弹 → 每个弹自身位置撒 type 8）。
调用点在 `0x416656` / `0x416690` / `0x4166ca` / `0x416703`（都在扩展 VM 体内，属脚本级 `laserCancel`/消弹 op）。
`type 9` 在 ECL 里**完全没有**直掉落站点（只由 §1.2 的满火力改写产生）。

## 4. 复算方法（口径写死）

### 4.1 解档 + 反编译

```bash
# ① 看归档里有什么（TH11 的 thdat 版本号 = 11）
local/vendor/thtk/build/thdat/thdat -l 11 local/th11.v1.00a/th11.dat

# ② 解档（thdat -x 把归档内所有文件解到当前目录，共 185 个条目）
mkdir -p /tmp/th11ecl && cd /tmp/th11ecl
/home/nipporita/renkolab/local/vendor/thtk/build/thdat/thdat \
    -x 11 /home/nipporita/renkolab/local/th11.v1.00a/th11.dat

# ③ 反编译（两遍：人类可读 + 每条指令带地址的 raw）
ECLMAP=/home/nipporita/renkolab/local/vendor/eclmap/eclmap/th11.eclm
THECL=/home/nipporita/renkolab/local/vendor/thtk/build/thecl/thecl
mkdir -p txt raw
for f in *.ecl; do
  $THECL -d 11    -x -j -m $ECLMAP "$f" "txt/${f%.ecl}.txt"   # 可读（-j: SJIS→UTF-8，否则会 UnicodeDecodeError）
  $THECL -d 11 -r -x -j -m $ECLMAP "$f" "raw/${f%.ecl}.txt"   # ★ 统计用这份：call/goto 也带地址
done
```

`th11.dat` 里的 ECL 一共 **21 个**：`stage01..stage04`（道中 + 首领同文件）、
`stage05/06/07 + 各自的 boss/mboss`（`stage05boss.ecl` 等）、
`stage4c00a/b/c` + `stage4c01a/b/c`（面 4 首领（さとり）复制的 6 组符卡帮手）、`default.ecl`（通用模板）。
依赖关系写在每个 stage 文件的 `ecli { ... }` 头里（例：`stage05.ecl` → `default.ecl`, `stage05mboss.ecl`, `stage05boss.ecl`）。
**TH11 没有 `stage04boss.ecl`** —— 面 4 首领在 `stage04.ecl` 内。

### 4.2 「波次」的定义

**波次 = `main()` 时间线上的一次 `@MainSubNN()` 调用**（`callAsync` / `call` 各算一次），
外加 `main` 里直接 `enmCreate` 的敌机（`MapleEnemy` / `MBoss` / `Boss` 各算一个波）。
依据：ZUN 的道中节奏全部由 `main()` 里的 `wait(N)` + `@MainSubNN() async` 串起来，
`MainSubNN` 内部再 `call`/`callAsync` 出 `MainSubNNL/R/b` 等子波（例：`MainSub05` → `MainSub05L`,`MainSub05R` 各 2 次）。
一个波内**被该波子程序（含同步/异步 call 链）自己 `enmCreate` 出来的敌机**都算这个波的敌人；
**敌人脚本再生成的东西（例：面 1 中 BOSS `MBoss` 自己变出的幻影）不算**（否则一个首领就会翻出几十只）。

### 4.3 计数口径与已知误差

| 项 | 口径 |
| --- | --- |
| 站点执行次数 | 控制流权重 × 各层循环次数。控制流按「条件跳转两出口各 1/2（数据相关分支取期望）」；回边不传播，循环次数单独乘；条件回边（循环尾）的出口权重 = 循环入口权重 |
| 循环识别 | `pushi(0); seti(A); … if (A < N) goto L` 型 → N 次；`pushi(N); seti(A); … deci(A); jmpNeq(L,0)` 型 → **N 次**（对应 thtk 的 `if ($A--)` 后自减模型） |
| 🟡 `deci` 型循环的 ±1 | 若 VM 的 `deci` 实为「先自减再判零」，则这类循环只跑 **N−1** 次。**本仓未钉死 `deci` 的语义**（TH11 的 ECL VM 里只找到扩展 opcode 分派 `0x412eaa`，基表 op 的执行路径没定位到）。两种读法的总计见 §5 表下注 |
| 「纯」大信仰点敌人 | 有效主掉落 = `{3}` 且没有任何附加掉落（例：`GGirl02` 是纯的；`Girl04` = type 3 + `dropExtra(2,4)` 不是） |
| 🟡 跨文件子程序名 | 脚本按「同文件内解析」处理 `call`/`enmCreate` 目标。`ecli { ... }` 让多个 `.ecl` 共享命名空间，实际存在跨文件引用（见下） |

**跨文件引用的实际清单**（对本文结论无影响，但口径要写清）：`stage05/06/07 main → Boss`/`MBoss`（定义在各自的 boss/mboss 文件）、
`stage04 → BossCard2_00X`/`BossCardItem`（定义在 `stage4c0*.ecl`）、全部文件 → `default.ecl` 的 `Ecl_EtBreak*`。
**这些子程序里一个 type-3 掉落指令都没有**（§3.1 是全 21 个 ecl 的穷举），所以 type-3 计数不受影响；
受影响的是它们的主掉落会回落到 `enmCreate` 第 6 参（`Boss`/`MBoss` 都是 1 = P 点，本来也不进 type-3 统计）。

## 5. 逐关汇总

> **道中** = `main()` 里在首领登场之前的波次（含中 BOSS）。“首领/BOSS 自己生成的敌机”不计入。
> 掉大信仰点敌机数、type3 道具数均为**期望值**（口径见 §4.3）。

| 关 | ECL 文件 | 道中波次数 | 道中敌机总数 | **掉大信仰点敌机** | 其中「纯」 | **大信仰点道具数** | 每波大信仰点敌机(均值) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| st01 | `stage01.ecl` | 16 | 303 | **170** | 160 | 170 | 10.62 |
| st02 | `stage02.ecl` | 9 | 151 | **40** | 40 | 40 | 4.44 |
| st03 | `stage03.ecl` | 10 | 74 | **0** | 0 | 0 | 0.00 |
| st04 | `stage04.ecl` (+`stage4c0*`) | 19 | 354 | **100** | 100 | 100 | 5.26 |
| st05 | `stage05.ecl` (+mboss/boss) | 10 | 386 | **88** | 88 | 88 | 8.80 |
| st06 | `stage06.ecl` (+mboss/boss) | 9 | 277 | **32** | 0 | 184 | 3.56 |
| st07(EX) | `stage07.ecl` (+mboss/boss) | 13 | 425 | **41** | 0 | 334 | 3.15 |
| **合计** | | **86** | **1970** | **471** | **388** | **916** | **5.48** |

🟡 若 `deci` 是「先自减」：上表变 敌机 1846 / 掉大信仰点 419 / 纯 340 / 道具 848（逐关：st01 301/168/158/168、
st02 143/36/36/36、st03 74/0/0/0、**st04 298/60/60/60**、**st05 348/86/86/86**、**st06 257/28/0/164**、st07 425/41/0/334）。
受影响的只有 st04/05/06，**结论不变**。

### 5.1 逐波次明细

#### st01（`stage01.ecl`）

| 波 | main 内调用 | 地址 | 敌机数 | 掉大信仰点敌机 | 其中「纯」 | 大信仰点个数 | 敌机构成 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| W00 | `@MainSub00` | `0x10aa0` | 1 | 0 | 0 | 0 | Girl01×1 |
| W01 | `@MainSub01` | `0x10ac0` | 20 | 0 | 0 | 0 | RGirl00×20 |
| W02 | `@MainSub02` | `0x10af4` | 19 | **5** | 5 | 5 | BGirl02×6, GGirl02×5, RGirl02×8 |
| W03 | `@MainSub03` | `0x10b28` | 21 | **20** | 20 | 20 | RockB×1, RockS×20 |
| W04 | `@MainSub04` | `0x10b48` | 2 | **2** | 0 | 2 | Girl04×2 |
| W05 | `@MainSub05` | `0x10b7c` | 28 | **8** | 8 | 8 | BGirl05×8, GGirl05×8, RGirl05×12 |
| W06 | `enmCreate(MBoss)` | `0x10bb0` | 1 | 0 | 0 | 0 | MBoss×1 |
| W07 | `@MainSub06` | `0x10bf4` | 105 | **75** | 75 | 75 | BGirl05×15, GGirl05×75, RGirl05×15 |
| W08 | `@MainSub07` | `0x10c24` | 2 | **2** | 0 | 2 | Girl06×2 |
| W09 | `@MainSub03` | `0x10c58` | 21 | **20** | 20 | 20 | RockB×1, RockS×20 |
| W10 | `@MainSub07` | `0x10c8c` | 2 | **2** | 0 | 2 | Girl06×2 |
| W11 | `@MainSub09` | `0x10cc0` | 20 | **9** | 8 | 9 | BGirl02×7, GGirl02×8, Girl06×1, RGirl02×4 |
| W12 | `@MainSub10` | `0x10ce0` | 20 | **9** | 8 | 9 | BGirl02×7, GGirl02×8, Girl06×1, RGirl02×4 |
| W13 | `@MainSub09` | `0x10d14` | 20 | **9** | 8 | 9 | 同 W11 |
| W14 | `@MainSub10` | `0x10d34` | 20 | **9** | 8 | 9 | 同 W12 |
| W15 | `enmCreate(Boss)` | `0x10d8c` | 1 | 0 | 0 | 0 | Boss×1 |

> W07 是「绿妖精大潮」：`MainSub06` 15 次 `callAsync` 出 `MainSub06L/R`，每组 7 只里 **5 只是绿的**（`anmSetMain(1,47)` / `dropMain(3)`）。

#### st02（`stage02.ecl`）

| 波 | main 内调用 | 地址 | 敌机数 | 掉大信仰点敌机 | 其中「纯」 | 大信仰点个数 | 敌机构成 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| W00 | `enmCreate(MapleEnemy)` | `0xf180` | 1 | 0 | 0 | 0 | MapleEnemy×1 |
| W01 | `@MainSub00` | `0xf1c4` | 10 | 0 | 0 | 0 | BGirl00×4, RGirl00×6 |
| W02 | `@MainSub01` | `0xf1e4` | 24 | 0 | 0 | 0 | BGirl01×4, BGirl02×4, RGirl01×8, RGirl02×8 |
| W03 | `@MainSub03` | `0xf218` | 6 | 0 | 0 | 0 | BCir00×6 |
| W04 | `@MainSub04` | `0xf24c` | 3 | 0 | 0 | 0 | PCir01×2, PCir01B×1 |
| W05 | `enmCreate(MBoss)` | `0xf280` | 1 | 0 | 0 | 0 | MBoss×1 |
| W06 | `@MainSub05` | `0xf2c4` | 80 | **40** | 40 | 40 | BGirl03×40, GGirl03×40 |
| W07 | `@MainSub06` | `0xf308` | 25 | 0 | 0 | 0 | BCir02×9, BCir02B×6, RCir02×6, RCir02B×4 |
| W08 | `enmCreate(Boss)` | `0xf360` | 1 | 0 | 0 | 0 | Boss×1 |

#### st03（`stage03.ecl`）—— **道中一只大信仰点都没有**

| 波 | main 内调用 | 地址 | 敌机数 | 掉大信仰点敌机 | 大信仰点个数 | 敌机构成 |
| --- | --- | --- | --- | --- | --- | --- |
| W00 | `enmCreate(MapleEnemy)` | `0x14a80` | 1 | 0 | 0 | MapleEnemy×1 |
| W01 | `enmCreate(MapleEnemy2)` | `0x14ab4` | 1 | 0 | 0 | MapleEnemy2×1 |
| W02 | `@MainSub00` | `0x14af8` | 13 | 0 | 0 | RCir00×13 |
| W03 | `@MainSub01` | `0x14b18` | 10 | 0 | 0 | BGirl00×5, RGirl00×5 |
| W04 | `@MainSub02` | `0x14b4c` | 10 | 0 | 0 | PCir01×10 |
| W05 | `@MainSub03` | `0x14b6c` | 8 | 0 | 0 | BCir02×8 |
| W06 | `enmCreate(MBoss)` | `0x14ba0` | 1 | 0 | 0 | MBoss×1 |
| W07 | `@MainSub04` | `0x14bf4` | 10 | 0 | 0 | BGirl00×5, RGirl00×5 |
| W08 | `@MainSub05` | `0x14c14` | 6 | 0 | 0 | BCir02×6 |
| W09 | `@MainSub06` | `0x14c34` | 14 | 0 | 0 | PCir01×14 |

> stage03 里 `GCir00/01/02/03`、`GGirl00` 五个绿 wrapper **定义了但一个都没被生成** —— 面 3 的道中确实不给大信仰点。

#### st04（`stage04.ecl` + `stage4c0*.ecl`）

| 波 | main 内调用 | 地址 | 敌机数 | 掉大信仰点敌机 | 其中「纯」 | 大信仰点个数 | 敌机构成 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| W00 | `@MainSub00` | `0xf7a0` | 12 | 0 | 0 | 0 | RCir00×12 |
| W01 | `@MainSub01` | `0xf7c0` | 1 | 0 | 0 | 0 | MBoss×1 |
| W02 | `@MainSub03` | `0xf7f4` | 13 | 0 | 0 | 0 | BGirl01×13 |
| W03 | `@MainSub02` | `0xf814` | 8 | 0 | 0 | 0 | RGirl00×8 |
| W04 | `@MainSub04` | `0xf848` | 18 | 0 | 0 | 0 | BCir01×6, BCir01b×3, RCir01×6, RCir01b×3 |
| W05 | `@MainSub00b` | `0xf890` | 12 | 0 | 0 | 0 | RCir00×12 |
| W06 | `@MainSub01b` | `0xf8b0` | 1 | 0 | 0 | 0 | MBoss2×1 |
| W07 | `@MainSub05` | `0xf8d0` | 50 | **50** | 50 | 50 | GCir02×50 |
| W08 | `@MainSub06` | `0xf8f0` | 2 | 0 | 0 | 0 | RGirl02×2 |
| W09 | `@MainSub03b` | `0xf924` | 108 | 0 | 0 | 0 | BGirl01b×72, RGirl01b×36 |
| W10 | `@MainSub02b` | `0xf944` | 12 | 0 | 0 | 0 | RGirl00b×12 |
| W11 | `@MainSub04` | `0xf978` | 18 | 0 | 0 | 0 | 同 W04 |
| W12 | `@MainSub06b` | `0xf9ac` | 7 | 0 | 0 | 0 | RGirl02×6, RGirl03×1 |
| W13 | `enmCreate(Boss)` | `0xfa04` | 1 | 0 | 0 | 0 | Boss×1 |
| W14 | `@MainSub02` | `0xfb2c` | 8 | 0 | 0 | 0 | 同 W03 |
| W15 | `@MainSub03` | `0xfb4c` | 13 | 0 | 0 | 0 | 同 W02 |
| W16 | `@MainSub04` | `0xfb6c` | 18 | 0 | 0 | 0 | 同 W04 |
| W17 | `@MainSub05` | `0xfb8c` | 50 | **50** | 50 | 50 | 同 W07 |
| W18 | `@MainSub06` | `0xfbac` | 2 | 0 | 0 | 0 | 同 W08 |

#### st05（`stage05.ecl` + mboss/boss）

| 波 | main 内调用 | 地址 | 敌机数 | 掉大信仰点敌机 | 其中「纯」 | 大信仰点个数 | 敌机构成 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| W00 | `enmCreate(MapleEnemy)` | `0x674c` | 1 | 0 | 0 | 0 | MapleEnemy×1 |
| W01 | `@MainSub00` | `0x67a8` | 48 | 0 | 0 | 0 | BPhantom00×24, RPhantom00×24 |
| W02 | `@MainSub01` | `0x67dc` | 10 | 0 | 0 | 0 | BCir00×10 |
| W03 | `@MainSub02` | `0x6810` | 61 | 0 | 0 | 0 | BCir01×1, BPhantom01×36, GPhantom01×12, RPhantom01×12 |
| W04 | `@MainSub03` | `0x6844` | 1 | 0 | 0 | 0 | MBoss×1 |
| W05 | `@MainSub04` | `0x6878` | 80 | **80** | 80 | 80 | GPhantom02×80 |
| W06 | `@MainSub05` | `0x68ac` | 64 | **8** | 8 | 8 | BCir02×56, GPhantom02×8 |
| W07 | `@MainSub06` | `0x68e0` | 72 | 0 | 0 | 0 | BCir01×2, BPhantom01×42, GPhantom01×14, RPhantom01×14 |
| W08 | `@MainSub00` | `0x6914` | 48 | 0 | 0 | 0 | 同 W01 |
| W09 | `enmCreate(Boss)` | `0x696c` | 1 | 0 | 0 | 0 | Boss×1 |

#### st06（`stage06.ecl` + mboss/boss）—— 大信仰点走的是 `dropExtra(3,n)`（一只敌人撒一堆）

| 波 | main 内调用 | 地址 | 敌机数 | 掉大信仰点敌机 | 大信仰点个数 | 敌机构成 |
| --- | --- | --- | --- | --- | --- | --- |
| W00 | `enmCreate(MapleEnemy)` | `0x4084` | 1 | 0 | 0 | MapleEnemy×1 |
| W01 | `@MainSub00` | `0x40e0` | 32 | 0 | 0 | Crow00×32 |
| W02 | `@MainSub01` | `0x4114` | 12 | **12** | 24 | Crow01×12（每只 `dropExtra(3,2)`） |
| W03 | `@MainSub02` | `0x4148` | 144 | 0 | 0 | BPhantom00×144 |
| W04 | `enmCreate(MBoss)` | `0x41a4` | 1 | 0 | 0 | MBoss×1 |
| W05 | `@MainSub03` | `0x41f8` | 20 | **20** | 160 | Crow02×20（每只 `dropExtra(3,8)`） |
| W06 | `@MainSub04` | `0x422c` | 64 | 0 | 0 | Crow03×64 |
| W07 | `@MainSub05` | `0x4260` | 2 | 0 | 0 | RPhantom01×2 |
| W08 | `enmCreate(Boss)` | `0x42b8` | 1 | 0 | 0 | Boss×1 |

#### st07 = Extra（`stage07.ecl` + mboss/boss）

| 波 | main 内调用 | 地址 | 敌机数 | 掉大信仰点敌机 | 大信仰点个数 | 敌机构成 |
| --- | --- | --- | --- | --- | --- | --- |
| W00 | `enmCreate(MapleEnemy2)` | `0x4bcc` | 1 | 0 | 0 | MapleEnemy2×1 |
| W01 | `@MainSub00` | `0x4c14` | 20 | 0 | 0 | RGirl00×20 |
| W02 | `@MainSub01` | `0x4c48` | 7 | **7** | 70 | Girl01×7（每只 `dropExtra(3,10)`） |
| W03 | `@MainSub02` | `0x4c7c` | 1 | **1** | 10 | Girl02×1 |
| W04 | `@MainSub03` | `0x4cb0` | 120 | 0 | 0 | BGirl03×120 |
| W05 | `@MainSub04` | `0x4ce4` | 10 | **10** | 60 | Girl04×10（`dropExtra(3,6)`） |
| W06 | `enmCreate(MBoss)` | `0x4d2c` | 1 | 0 | 0 | MBoss×1 |
| W07 | `@MainSub05` | `0x4d7c` | 120 | 0 | 0 | BGirl05×120 |
| W08 | `@MainSub06` | `0x4db0` | 9 | **9** | 54 | Girl06×9（`dropExtra(3,6)`） |
| W09 | `@MainSub03` | `0x4de4` | 120 | 0 | 0 | 同 W04 |
| W10 | `@MainSub07` | `0x4e04` | 14 | **14** | 140 | Girl01×12, Girl01b×2（`dropExtra(3,10)`） |
| W11 | `enmCreate(Boss)` | `0x4e5c` | 1 | 0 | 0 | Boss×1 |
| W12 | `enmCreate(MBoss)` | `0x4f54` | 1 | 0 | 0 | MBoss×1 |

## 6. 密度的真实形状（回答「一波几只」）

把 86 个波按「掉大信仰点的敌机数」分类：

| 每波大信仰点敌机 | 波数 |
| --- | --- |
| **0 只** | **62** |
| 1–2 只 | 4（全都是 `MapleEnemy`/`MBoss`/`Boss`/`Girl04×2` 这类单只波） |
| 3–10 只 | 10 |
| 11–30 只 | 5 |
| 31 只以上 | 5（40 / 50 / 50 / 75 / 80） |

只看**真正的杂兵波（≥3 只敌机，共 57 个）**：**37 个波一只都没有，其余 20 个波全部 ≥5 只**
（5, 7, 8, 8, 9, 9, 9, 9, 9, 10, 12, 14, 20, 20, 20, 40, 50, 50, 75, 80）。
**一个 1–4 只的波都没有。**

**明确回答**

1. **「纯信仰点掉落」的敌人在 TH11 稀有吗？—— 不稀有，而且量很大。**
   全作道中 1970 只敌机里 **471 只（23.9%）**掉大信仰点；其中 **388 只（19.7%）掉的是「纯」大信仰点**（不带任何别的道具）。
   逐关：面 1 170/303（56%）、面 2 40/151（26%）、**面 3 0/74（0%）**、面 4 100/354（28%）、面 5 88/386（23%）、
   面 6 32/277（12%）、EX 41/425（9.6%）。
2. **一关几只？** 道中合计 41–170 只/关（中位数约 88）；大信仰点**道具**数 0–334 个/关。
3. **一波几只？** **不是「一两只」**，而是 **0 或 ≥5** 的两极：
   57 个杂兵波里 37 个是 0，另外 20 个是 5–80 只（中位数 9，均值 19.6）。
4. 所以如果 TH18 的复刻目标要贴合原作手感，**「一波改一两只绿妖精」在原作里没有对应物**；
   原作的语法是「**这一波专门放一群绿妖精（5~20 只），下一波一只不放**」。
   与之最接近的「少量绿妖精混编」样本只有面 1 的 W02（19 只里 5 只绿）——**下限就是 5 只**。

## 7. 顺带结论：一堆「绿 wrapper」是死代码

`GGirl00`/`GCir00` 这类**定义了 `dropMain(3)` 但没有任何 `enmCreate` 引用**的子程序
（按文件内引用统计，共 **33 个**，占 46 个 type-3 掉落站点的 72%）：

| ECL 文件 | 死掉的绿 wrapper |
| --- | --- |
| `stage01.ecl` | `GGirl00` `GGirl03` |
| `stage02.ecl` | `GCir00` `GCir01` `GCir01B` `GCir02` `GCir02B` `GGirl00` `GGirl01` `GGirl01B` `GGirl02` |
| `stage03.ecl` | `GCir00` `GCir01` `GCir02` `GCir03` `GGirl00`（**整关 5 个全死**） |
| `stage04.ecl` | `GCir00` `GCir01` `GCir01b` `GCir03` `GGirl00` `GGirl00b` `GGirl01` `GGirl01b` `GGirl02` `GGirl03` |
| `stage05.ecl` | `GCir00` `GCir01` `GCir02` |
| `stage05boss.ecl` | `GBoss1Girl`（0xa1b4；同族的 `BBoss1Girl` 蓝 / `RBoss1Girl` 红里只有红的那只在 `0x153c` 被生成） |
| `stage07.ecl` | `GGirl00` `GGirl03` `GGirl05` |

⇒ **统计「原作的绿妖精有哪些」时必须按「被生成」过滤，光看 `dropMain(3)` 的定义会高估一倍以上。**
另：`stage01 MainSub08`（含 `0xc7c8 enmCreateA("RockS", …, 3)`）也没被 `main` 调用 —— 同样是死代码。

命名规律（一手）：`BGirlXX`/`BCirXX` = 蓝（`dropMain(2)` 蓝点、`anmSetMain(1,45)`、`anmSetSprite(0,63)`），
`RGirlXX` = 红（`dropMain(1)` P 点、anm 46），**`GGirlXX`/`GCirXX`/`GPhantomXX` = 绿（`dropMain(3)` 大信仰点、anm 47）**，
`YGirlXX` = 黄（anm 48，**19 个子程序全部既没有 drop 调用也从未被生成** —— 整族死代码）。
例：`stage01 GGirl02` = `0x4fb0 dropMain(3)` + `0x4fc4 anmSetMain(1,47)`。

## 8. 可信度表 / 还缺什么

| # | 断言 | 可信度 |
| --- | --- | --- |
| 1 | 大信仰点 = item type 3（`add_to_piv(+10000)` = 显示 +100）；8/9 = 小/中 | ✅ 一手（分派表 `0x423f00` + 三个 `add_to_piv` 调用点） |
| 2 | 满火力时 P 点（1/4/10/11）在**生成时**改写成 type 9；满信仰无改写 | ✅ 一手（`0x424339`–`0x424356`） |
| 3 | 引擎侧永远不撒 type 3（21 处 `0x424230` 全查） | ✅ 一手（穷举） |
| 4 | `dropMain(N)` 写 `ctx+0x1508`；`dropExtra(T,n)` 写 `ctx[T*4+0x1508]`；`dropClear` 只清附加格 | ✅ 一手（`0x414dbd` / `0x414d2c` / `0x414d14`，死亡结算 `0x410190`+`0x4101d0`） |
| 5 | `enmCreate*` 第 6 参 = 新敌人的主掉落（= `ctx+0x1508`） | ✅ 一手（`0x413015` → `0x410955` → `enemy+0x2544`，且 `ctx = enemy+0x103c`） |
| 6 | 逐关/逐波/逐敌机计数（§5/§6） | ✅ 口径可复算；🟡 `deci` 型循环 ±1（§4.3） |
| 7 | 一族 `GGirl00` 等是死代码 | ✅ 一手（全 21 个 ecl 的 `enmCreate` 目标穷举） |
| 8 | `deci` 是「后自减判旧值」还是「先自减判新值」 | ❓ **未钉死**。TH11 只定位到扩展 opcode 分派（`0x412eaa`）；基表 op 的执行路径没找到（exe 里只有一处读 opcode：`0x412e7e`），`deci` 的 ±1 因此悬着 |
| 9 | 练习模式/其它难度下 `0x4a5720` 是否影响掉落 | ❓ 未查（掉落与难度无关的是 type 3，但敌人配置可能按难度分支） |
| 10 | 敌人「被消弹打死 / 走到屏幕外」时不掉主道具 | ❓ 未逐条验证（本文按「死亡即结算」计） |

## 9. 复现命令与脚本

统计脚本放在 `/tmp`（按仓规不进库，内容见下）：

```bash
# 站点级清单（770 行：所有 ecl 的所有 enmCreate 站点 + 其有效掉落）
python3 /tmp/th11_faith_drops.py all        > /tmp/th11_faith_report.txt
python3 /tmp/th11_faith_drops.py sites      # → /tmp/th11_faith_sites.tsv
python3 /tmp/th11_faith_drops.py dropops    # 含 type3 掉落指令的子程序（一手地址）
python3 /tmp/th11_faith_drops.py waves      # 逐波次表
python3 /tmp/th11_faith_drops.py notes      # 循环识别备注（UNKNOWN 会标出来）
python3 /tmp/th11_faith_report.py stage     # 逐关汇总（本文 §5 表）
python3 /tmp/th11_faith_report.py waves     # 逐波次 markdown 表（本文 §5.1）
DECI_MODE=pre python3 /tmp/th11_faith_report.py stage   # 🟡 deci 另一种读法的下限
```

脚本实现要点（便于重写）：输入 `raw/*.txt`，用
`^(enmCreate\w*)\((.*)\)$` / `^dropMain\((\d+)\)$` / `^dropExtra\((\d+),\s*(\d+)\)$` / `^call(Async)?\("(\w+)"\)$`
正则抓站点；`void (\w+)\(` 切子程序；`jmpNeq\((\w+),\s*\d+\)` + 往回 5 条找 `pushi(N)`/`pushi([slot])` 认循环；
「有效掉落」= 沿同步 `call` 链合并被生成子程序自己的 `dropMain`/`dropExtra`，为空才回落到 `enmCreate` 第 6 参。

引擎侧核对用：

```bash
# 道具类型分派 + 三个 add_to_piv 调用点 + 满火力改写
objdump -d -M intel --start-address=0x423a70 --stop-address=0x423f40 local/th11.v1.00a/th11.exe
# 四个 ECL 掉落 handler
objdump -d -M intel --start-address=0x414d14 --stop-address=0x414e00 local/th11.v1.00a/th11.exe
# 扩展 opcode 分派（330 = dropMain）
objdump -d -M intel --start-address=0x412e70 --stop-address=0x412eb0 local/th11.v1.00a/th11.exe
# 死亡结算（主掉落 + 附加掉落）
objdump -d -M intel --start-address=0x410190 --stop-address=0x4101d0 local/th11.v1.00a/th11.exe
# 所有道具生成调用点（查 type）
grep -n "call   0x424230" local/_scratch/th11_full.asm
python3 -c "
import struct
d=open('local/th11.v1.00a/th11.exe','rb').read()
sec=lambda v: (0x400+(v-0x400000-0x1000)) if v<0x48b000 else (0x8a400+(v-0x400000-0x8b000))
print('type table 0x423f00:', [hex(struct.unpack('<I',d[sec(0x423f00)+4*i:sec(0x423f00)+4*i+4])[0]) for i in range(11)])
print('rewrite bytes 0x424578:', [hex(d[sec(0x424578)+i]) for i in range(11)])
print('rewrite jmptbl 0x424570:', [hex(struct.unpack('<I',d[sec(0x424570)+4*i:sec(0x424570)+4*i+4])[0]) for i in range(2)])
"
```

## 10. 与姊妹篇的关系

* [01-faith-and-points.md](01-faith-and-points.md)：信仰 = 最大得点、`add_to_piv` `0x424640`、三档绿点数值、蓝点公式 —— 本文不重复，只补 §1.1 的完整 type 表与 §1.2 的**生成时机**细节。
* [02-koushin-signal.md](02-koushin-signal.md)：交信度 —— 与掉落无关（交信度只乘蓝点分数）。
* 本文新增：**掉落站点的完整分布**（原作「绿妖精」到底有多少、在哪几波），这是做 TH18 卡 110/111 复刻时「改哪些妖精」的对照基准。
