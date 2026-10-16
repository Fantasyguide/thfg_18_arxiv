# TH17 点道具身价（最大得点 / PIV）—— 载体在、量纲 ×100、运行期只有两条涨点、线上/线下两档

> **版本**：TH17 v1.00b（`local/th17.v1.00b/th17.exe`，md5 `7a5b5bc1c231792098a28d8367928aaf`，747,520 B，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本一律写成 `th13:0x4be7dc` 这种形状。
> **方法**：不用 Ghidra（工程锁独占）—— 小 python 解 PE 节表取 `.text`/`.rdata` 原始字节 →
> `objdump -D -b binary -m i386 -M intel --adjust-vma=0x401000 /tmp/_text.bin > /tmp/th17_full.asm`；
> 全局地址按多种绝对寻址形态穷举（`mov eax,ds:` / `mov ds:,eax` / `imul DWORD PTR ds:` / `inc` / `cmovg`）；
> 分派跳转表 `0x4341c0` 与两张难度表按 VA→文件偏移直读原字节；常量按 IEEE754 直读；
> 逐指令手写仿真 + 闭式对照（同源 scratch：`local/_scratch/th17/th17_item_value_emulator.py`，结论一致）。
> ExpHP 名取自 `local/vendor/th-re-data/data/th17.v1.00b/`（`statics.json` / `type-structs-own.json`）。
> **可信度**：载体 / 字段表 / 量纲 / 初值与上限 / 运行期两条涨点 / 点道具两档公式 / 「无第二项」= ✅ 一手（附地址）；
> `LINE` 归哪个自机、item `state==3` 的设计意图、type 8 道具的产出者 = 🟡/❓（§8）。
> 相关：[`../th13/01-point-item-value.md`](../th13/01-point-item-value.md) · [`../th10/01-faith-and-points.md`](../th10/01-faith-and-points.md) ·
> [`../../animal/th17/01-animal-spirits.md`](../../animal/th17/01-animal-spirits.md) · [`../../bullet/th17/01-cancel-to-item.md`](../../bullet/th17/01-cancel-to-item.md) ·
> [`../../player/th16/06-resource-economy.md`](../../player/th16/06-resource-economy.md)（TH16 同构对照）。

## 0. 一句话结论

**TH17 有 PIV。** 载体 `CURRENT_PIV` = `th17:0x4b5a24`（= `zGlobalsInner` 基址 `0x4b59dc` + `0x48`，ExpHP 名 `CURRENT_PIV`），
上限 `MAXIMUM_PIV` = `0x4b5a2c`，初值来源 `MINIMUM_PIV` = `0x4b5a28`（二者同族字段，初始化按**难度**查表 ×100）。

- **量纲**：**存储 = 显示 × 100**（HUD 与点道具都先 `/100`；power 满时一次加值 `+10000` 存储、弹窗写 `100`）。
- **运行期涨点全 exe 只有 2 条指令**：道具 type 8（`0x433b74`）与道具 type 9..14（`0x433f3a`）；**没有任何跌点**（PIV 单调不减）。
- **点道具（type 2）分值** = `trunc10(PIV/100)`（线上，黄字弹窗）或 `≈ 0.75·v·(1 − (y−LINE)/450)`（线下，白字），
  `v = PIV/100`、`LINE = 128`（`CHARACTER != 1`）/ `148`（`CHARACTER == 1`）、`y` = **自机** `pos.y`；
  弹窗值就是玩家看到的分值（HUD 分数按内部值 ×10 画，见 §5.4），内部记账是 `CURRENT_SCORE += 弹窗值/10`。
- **没有第二项**：公式的输入只有 `CURRENT_PIV`、自机 `y`、`CHARACTER` 三项；擦弹 / 兽灵 / 咆哮 / miss / 霊撃 / 阶段奖**一律不参与**。

## 1. 载体与字段（✅ 一手 + ExpHP 对名）

`zGlobals` 在 `0x4b59c0`，其中 `inner`（`zGlobalsInner`）在 `+0x1c` ⇒ **inner 基址 `0x4b59dc`**。
下面每一项都有一手用途对得上：

| 绝对地址 | inner 偏移 | ExpHP 名 | 本文一手用途 |
| --- | --- | --- | --- |
| `0x4b5a24` | `+0x48` | `CURRENT_PIV` | 点道具/power 道具的**分值基数**；两条涨点写它；HUD 读它 |
| `0x4b5a2c` | `+0x50` | `MAXIMUM_PIV` | 两条涨点的 `cmovg` 夹取对象；运行期**只读** |
| `0x4b5a28` | `+0x4c` | `MINIMUM_PIV` | 只在初始化写一次、**全 exe 无人读**（= 初值存档） |
| `0x4b5a20` | `+0x44` | `NUM_POINT_ITEMS_COLLECTED` | 在点道具分支 `inc`（`0x43455d`）——**只增不读** |
| `0x4b59fc` | `+0x20` | `CURRENT_SCORE` | 收点入账；钳 `0x3b9ac9ff`（整数 999,999,999） |
| `0x4b59f4` | `+0x18` | `CHARACTER` | 决定回收线 `LINE = 128 / 148`（§5.3） |
| `0x4b5a00` | `+0x24` | `DIFFICULTY` | PIV 初值/上限两张表的索引（§3） |
| `0x4b5a30` / `0x4b5a34` / `0x4b5a38` | `+0x54/+0x58/+0x5c` | `CURRENT_POWER` / `MAXIMUM_POWER` / `POWER_PER_LEVEL` | power 分支用到（满 power 才涨 PIV） |

**穷举口径（`CURRENT_PIV` `0x4b5a24`）**：全反汇编里 `grep -c 4b5a24` = **10 处**，逐条看过，覆盖
`mov eax,ds:` / `imul DWORD PTR ds:` / `mov ds:,eax` 三种绝对寻址形态；**没有** `lea`/寄存器相对形态。
另做「间接访问扫描」：全 exe 里把 inner 基址 `0x4b59dc` 装进寄存器的指令（100 处）之后 40 条内，
按 `+0x48 / +0x4c / +0x50` 位移访问的 —— **0 处**（所以不像有函数拿着 `&inner` 偷偷读写）。
10 处清单见 §4.1 / §5；`0x4b5a2c` 全 exe 5 处（1 写 4 读）、`0x4b5a28` 全 exe **1 处**（初始化）。

## 2. 量纲：存储 = 显示 × 100（✅ 三条独立证据）

1. **弹窗与加值成对**：type 8 分支 `0x433b59` `add ecx,0x2710`（+10000 存储）而同一条链的弹窗 `0x433b71` `push 0x64`（显示 **100**）。
2. **所有消费点都先 `/100`**：点道具（`0x4344eb` / `0x434458` / `0x43427c` / `0x4342ff`）、HUD（`0x42cfd9`）用的都是同一段魔数
   `0x51eb851f` + `sar edx,5`（÷100）。TH13 同款量纲（存储 = 显示 ×100），TH11 是 ÷100 —— **逐作不同，不许对搬**。
3. **初始化也是 ×100**：`0x43038c` / `0x430394` 两条 `imul …,0x64`（§3）。

⇒ 显示值恒为 100 的倍数（初值 ×100，增量 500/1000/…/10000 也是 100 的倍数）。

## 3. 初值与上下限：按难度的两张表（✅ 一手 + 表值直读）

初始化在 `GameThread::thread_start` `0x430080` 里（`0x4302fc` 起 `edi = ds:0x4b5a00` = `DIFFICULTY`）：

```
4302fc  edi = ds:0x4b5a00                     ; ★ 难度索引
43038c  ecx = [edi*4 + 0x4a17a8] * 0x64       ; 表 A × 100
430394  eax = [edi*4 + 0x4a1790] * 0x64       ; 表 B × 100
4303c0  ds:0x4b5a2c = eax                     ; ★ MAXIMUM_PIV = B[diff] × 100
4303e7  ds:0x4b5a28 = ecx                     ; ★ MINIMUM_PIV = A[diff] × 100
4303ff  cvttss2si eax, (float)(ecx / 100.0f * 100.0f)   ; 恒等（只做取整）
430404  ds:0x4b5a24 = eax                     ; ★ CURRENT_PIV 初值 = MINIMUM_PIV
```

表值（`.rdata` 原字节直读，单位 = **显示**）：

| `DIFFICULTY` | `0x4a17a8` → `MINIMUM_PIV`/初值 | `0x4a1790` → `MAXIMUM_PIV` | 同段另一表 `0x4a1778` |
| :---: | ---: | ---: | ---: |
| 0 / 1 / 2 / 3 | 10000 | 500000 | 5 |
| 4 | 10000 | 500000 | 0 |
| 5 | 100000 | 1000000 | 0 |

- **上限运行期不变** ✅：`0x4b5a2c` 全 exe 只在 `0x4303c0` 写一次，其余 4 处都是两条涨点里的 `cmp` / `cmovg`。
- **没有下限**（`0x4b5a28` 无人读）；唯一的「地板」是弹窗值的 `≤0 → 10`（§5.1/§5.2）。
- 🟡 `DIFFICULTY` 索引 4 / 5 的归属（系列惯例 0..3 = Easy/Normal/Hard/Lunatic、4 = Extra）未做游戏内核对；
  已知 0..4 的初值/上限相同，只有索引 5 是两倍。

## 4. 涨/跌入口穷举（✅ 逐条；这是本作与 TH10/TH13 最大的不同）

### 4.1 写点全集：**运行期只有 2 条**（+ 1 条初始化）

| # | 地址 | 现场 | 做什么 |
| :---: | --- | --- | --- |
| 1 | `0x430404` | 关卡初始化（§3） | `CURRENT_PIV = MINIMUM_PIV` |
| 2 | `0x433b74` | 道具 **type 8** 分支 `0x433b40` | `PIV += 10000` 存储（**+100 显示**），夹 `MAXIMUM_PIV`，黄字弹窗 100 |
| 3 | `0x433f3a` | 道具 **type 9..14** 分支 `0x433f09` | `PIV += trunc(float_tbl[0x4a0d6c+type*4] × 100.0f)`，夹 `MAXIMUM_PIV` |

`0x433f09` 的表值（`0x4a0d6c` 直读 index = type）：**type 9→5.0、10→10.0、11→20.0、12→30.0、13→40.0、14→50.0**
（×`100.0f`（`0x4a3d30`）⇒ 存储 +500/+1000/+2000/+3000/+4000/+5000 = 显示 **+5/+10/+20/+30/+40/+50**），
随后 `0x433f5a` 起再按 `trunc10(PIV_new/100)/100` 折进分数。type 9/10/11/12/13 都在本作 ECL 里真的会掉
（`local/_scratch/ecltxt/` 全 stage：`dropExtra(10..13, n)` 共 142 处、`dropMain(14)` 1 处）。

### 4.2 **0 处**：以下机制一条都不碰 PIV（同一穷举口径）

| 机制 | 代表地址（一手） | 是否读写 PIV |
| --- | --- | --- |
| 擦弹 | `Player::do_graze` `0x449fa0` | ❌（TH15 是 `+100`，TH17 完全不是） |
| 兽灵收集 / 咆哮就绪 | `0x40f980` / `0x40fb60` | ❌ 见 [`animal/th17/01`](../../animal/th17/01-animal-spirits.md) §5.2 |
| 霊撃（咆哮消耗） | `do_hyper_cancel` `0x40f6d0` | ❌ |
| miss / 真死 | `FUN_004491b0` `0x4491b0`（`CURRENT_LIVES--`） | ❌ ⇒ **本作 miss 不扣 PIV**（TH10/TH11 扣，别外推） |
| 阶段清关奖 | `0x432381`–`0x4324f8`（`SCORE += (5×残机+炸弹)×100000`） | ❌（只读 `CURRENT_LIVES` `0x4b5a40` / `CURRENT_BOMBS` `0x4b5a4c`） |
| 符卡奖励 | ❓未追（不在本文范围） | ❌ 结构上不可能：PIV 全 exe 只有 §4.1 的 3 个写点 |
| 敌弹取消 | `gen_items_from_et_cancel` `0x419c30` | ❌ 直接（只间接：撒 type 9 道具，见 [`bullet/th17/01`](../../bullet/th17/01-cancel-to-item.md) §1.3） |
| 道具 type 1/2/3/4/5/6/7 | `0x434200` / `0x434400` / `0x433c49` / `0x433d75` / `0x433e4e` / `0x433ea5` / `0x433ebf` | ❌ 只**读**当基数（type 1/2），不写。type 3（大 P）满 power 时加的是**分数**：`0x433c62 add eax,0x7d0`(2000) + 弹窗 20000，之后又走 `0x433d37` 折算一次（`0x450b70` 是 `ret 0xc` 自清栈，故 esp 平衡、两笔都算）⇒ **不写 PIV** |

⇒ **PIV 单调不减**：全 exe 没有一条负向写点（也没有 `sub`/`dec` 形态的写）。

## 5. 点道具（type 2）被收时的分值：逐指令（✅）

### 5.1 入口与「不重复计分」（✅）

`ItemManager::on_tick__body` `0x433410` 是道具主循环；`0x433b27` 取 `[item+0xc5c]`（= item type）后
`0x433b39` `jmp DWORD PTR [eax*4+0x4341c0]`（14 项跳转表，index = type−1）。表值（原字节直读）：

| type | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9..14 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 目标 | `0x433c33` | `0x433d69` | `0x433c49` | `0x433d75` | `0x433e4e` | `0x433ea5` | `0x433ebf` | `0x433b40` | `0x433f09` |

type 2 → `0x433d69 mov ecx,edi` / `0x433d6b call 0x434400`（`Globals::collect_point_item`，ExpHP 名），
返回后 `0x433d70 jmp 0x433f86`（= 收尾：`interrupt` 动画 + 状态清 0 + 挂回空闲链表）——
**跳过 `0x433d37` 那条「`score += [esp+0x18]/10` 尾巴」**（该尾巴只有 **type 3** 的两条支路进：`0x433c9b` / `0x433cfe`）
⇒ 点道具不会重复计分 ✅。
旁证：`0x43455d inc DWORD PTR ds:0x4b5a20` 是**点道具分支独有**的计数器，与 ExpHP 的
`NUM_POINT_ITEMS_COLLECTED` 同名同位 ⇒ type 2 = 点道具 ✅（另：各 stage ECL 里 `dropMain(2)` 共 33 处，
与 TH10 文档「点道具 = type 2」的系列惯例一致）。

### 5.2 线上/线下两档（✅ 现场）

```
434400  eax = ds:0x4b77d0 (= PLAYER) ; xmm1 = [eax+0x614] = player.pos.y   ★ 比的是「自机 y」
43440b  cmp ds:0x4b59f4,0x1                                    ; CHARACTER
43441e  eax = 0x80(128) ; ecx = 0x94(148) ; cmove eax,ecx     ; LINE = 148 if CHARACTER==1 else 128
434438  comiss xmm0(=(float)LINE), xmm1(player.y)
43443b  jae 0x4344e1                 ; ★ 自机在线上（y ≤ LINE）→ 全值档（黄）
434441  cmp DWORD PTR [edi+0xc58],0x3 / 0x434448 je 0x4344e1  ; ★ item.state==3 → 也走全值档
43444e  ...                          ; 线下档（白）
```

**A. 线上档**（`0x4344e1`–`0x43456c`，弹窗色 `push 0xffffff00` 黄）：

```
4344e1  eax = 0x51eb851f ; esi = 10
4344eb  imul DWORD PTR ds:0x4b5a24      ; ★ PIV
4344f6  sar edx,5 ; +符号修正            ; v = PIV / 100
434500  cdq ; idiv esi(10)              ; eax = v/10, edx = v%10
43450a  sub ecx,edx                     ; w = v − v%10 = trunc10(v)
43450c  imul 0x66666667 ; sar edx,2     ; w/10
43451e  lea esi,[eax+eax*4] ; add esi,esi   ; esi = trunc10(v)（×5×2）
434528  test esi,esi ; cmovle esi,10     ; 下限 10
43452e  push 位置(item+0xc10) ; call 0x450b70   ; ★ 弹窗（= 玩家看到的分值）
434534  CURRENT_SCORE += esi/10（0x66666667+sar 2），钳 0x3b9ac9ff
43455d  inc NUM_POINT_ITEMS_COLLECTED
```

⇒ **线上分值 = `trunc10(PIV / 100)`**（10 的倍数，`v ≤ 0` 时取 10）。

**B. 线下档**（`0x43444e`–`0x4344df`，弹窗色 `push 0xffffffff` 白）：

```
v = PIV/100 ; rem = v%10
A = trunc( trunc(3*(rem−v)/4) × (y − LINE) / 450 )   ; 0x434494 负魔数 0x91a2b3c5 + sar edx,8 ≡ ÷450
B = trunc( 3*(v−rem)/4 )                             ; 0x4344a5 = 0.75 × trunc10(v)
esi = 10 × trunc( (A+B)/10 )                         ; 0x4344ba(/10) → 0x4344c6(×5×2)
esi ≤ 0 → 10
```

闭式（与逐指令仿真逐点一致，见 `local/_scratch/th17/th17_item_value_emulator.py`）：

```
线下分值 ≈ 0.75 · v · (1 − (y − LINE)/450)      v = PIV/100，d = y − LINE ≥ 1
```

⇒ 线下**随自机高度连续衰减**：`y = LINE` 处 = `0.75·v`，`y = LINE + 450` 处 = 0（截到 10 的倍数、下限 10）。

| PIV 存储（显示） | 线上 | `y=LINE+1` | `y=LINE+50` | `y=LINE+100` | `y=LINE+300` |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1,000,000（10000） | 10000 | 7480 | 6660 | 5830 | 2500 |
| 5,000,000（50000） | 50000 | 37410 | 33330 | 29160 | 12500 |
| 50,000,000（500000） | 500000 | 374160 | 333330 | 291660 | 125000 |

### 5.3 `LINE` 同时决定「什么时候道具自动飞向自机」（✅ 两处独立现场）

同一组常量在**主循环**里也出现一次，用于把道具切成 `state 3`（自动回收）：

```
43355b  eax = [player+0x18db0] ; if eax==2 || eax==4 → 0x433590（无条件回收）
43356b  cmp ds:0x4b59f4,0x1 ; eax = 0x80(128) ; cmove eax,[esp+0x1c](148)
433583  comiss (float)LINE, player.pos.y ; 0x43358a ja 0x43380a
43380a  ... 0x433819  mov [edi+0xc58],3        ; ★ state = 3（飞向自机）
```

⇒ **线上 = 「自机 y ≤ 128（CHARACTER==1 时 148）」这一条线**，与 TH10「比道具自己的 y」是**两种写法**（别对搬）。
`state 3` 另有两个非位置入口：`0x433599`（炸弹中途）/ `0x4335b1`（另一管理器状态）——**语义未钉**，见 §8。

### 5.4 玩家看到的分值怎么记账（✅）

- 弹窗值 = `esi`；内部 `CURRENT_SCORE += esi/10`（`0x434534`–`0x434563`）。
- **HUD 分数按内部值 ×10 画**（一手两处：`0x4323ca` / `0x4324f8`，清关奖 `SCORE += X` 的同时
  `[GUI+0x2d4] += 10X`）⇒ **玩家看到的加分 = 弹窗值 = `trunc10(PIV/100)`**。
- 与 TH13 完全同形（`值 = trunc10(PIV/100)`、入账 `值/10`），但 TH13 的 `PIV` 只由擦弹 `+100` 供养，TH17 不是。

## 6. 有没有「第二项」（额外加数）—— ❌ 没有（用户最关心的那条）

| 问题 | 一手答案 |
| --- | --- |
| 基础值之外是否还有加数？ | ❌ **没有**。公式的输入只有 `CURRENT_PIV`、`player.pos.y`、`CHARACTER`（定 `LINE`）三项，`esi` 全部由它们算出 |
| 有没有兽灵 / 咆哮 / 擦弹 / 道具数 / 难度 / 残机作为乘数或加数？ | ❌ 一个都没有（§4.2 逐条 + §1 的 10 处穷举：点道具链里除 PIV 外不读任何全局） |
| `NUM_POINT_ITEMS_COLLECTED`（`0x4b5a20`）参与吗？ | ❌ 只 `inc`（`0x43455d`）、全 exe **无人读** ⇒ 死计数器 |
| 难度影响分值吗？ | ❌ 难度只在**关卡初始化**决定 PIV 的表值（§3），公式里不读 `DIFFICULTY` |
| 那「总最大得点」怎么涨？ | 纯加性累加器 + 上限：`PIV = clamp(PIV + Δ, −, MAXIMUM_PIV)`，`Δ ∈ {10000(满 power 时的 type 8), 500/1000/2000/3000/4000/5000(type 9..14)}` 存储（= 显示 `100 / 5…50`） |

⇒ 与 TH10（信仰 = 独立计分行、miss/灵击/阶段超时回落）、TH13（擦弹 `+100` 唯一通道）、TH16
（满 power 的 P 道具 `+10000 PIV`，本作同款常量但挂在 **type 8**）都不同：**TH17 的 PIV 只由「道具」这一条腿供养，且只上不下**。

## 7. HUD 上显示的是什么（✅ 值 / 🟡 标签名）

- `Gui::on_tick` `0x42b5f0`（`0x42cfd9`）逐帧算 `v = PIV/100` → `trunc10` → `0x408300` 出数字：
  ```
  42cfd9  imul DWORD PTR ds:0x4b5a24 ; 42cfdf sar edx,5 ; 42cfec idiv esi(10)
  42cff2  sub ecx,edx                ; trunc10(PIV/100)
  42cff6  call 0x408300              ; 数字 → HUD 文本
  42d02e  call 0x408300 (push ds:0x4b5a0c = GRAZE)   ; 紧接着同一栏位画「擦弹数」
  ```
  同一函数里先画 Score（`0x42c7aa`）⇒ 顺序 = **Score → 本值 → グレイズ**。
- **exe 里没有任何 SJIS HUD 文本**（`最大得点`/`得点`/`グレイズ`/`スコア` 全 0 命中，TH10/13/16/18 同样 0）：
  HUD 的量只是浮点/整数经 `%3d`/`%6d` 这类 ASCII 格式串（`0x4a16f4`–`0x4a1740`）画成数字，
  **汉字标签是 `front.anm` 的贴图**（串 `front.anm` 在 `0x4a16e8`）。
- 结论：**HUD 上那一栏显示的就是 `trunc10(CURRENT_PIV/100)`**（✅ 值 + 位置在 Score 与 グレイズ 之间）；
  把它读作系列同款的「**最大得点**」是 🟡 —— 依据是 ExpHP 字段名 `CURRENT_PIV`、量纲/显示公式与
  TH10/TH11/TH13 的「最大得点」逐条同形、且在 HUD 上紧邻 Score 与擦弹数；**未做人眼比对贴图**。

## 8. 开放 / 未解 ❓

| 事项 | 状态 |
| --- | --- |
| **type 8 道具（满 power 时 `+10000 PIV`）在本作怎么产出** | ❓ 反向穷举：`spawn_item` `0x434740` 全 exe 17 个调用点里没有 `push 8`；本地解包 ECL（`local/_scratch/ecltxt/`：st01..st07 + 各 bs/mbs + default）里**没有** `dropExtra(8)`/`dro…（详见本节正文） |
| `LINE = 148` 对应哪个自机 | 🟡 `CHARACTER == 1`（ExpHP 结构体顺序 0/1/2）；一手只坐实「索引 1」，自机名未核 |
| item `state == 3` 的完整语义 | 🟡 一手：`0x433819` 由「自机在线上」`0x43358a` 置位；另有 `0x433599`（炸弹中）/ `0x4335b1` 两个入口未解；TH11 的 `state 3` 是「满槽全屏回收」，**未在 TH17 复核** |
| `DIFFICULTY` 索引 4 / 5 的归属 | 🟡 见 §3 |
| 符卡 / 阶段奖励的完整公式 | ❓ 不在本文范围（结构上不含 PIV，见 §4.2） |
| `[player+0x18db0] ∈ {2,4}` 的字段名 | ❓ 见 `0x43355b`（ExpHP 结构体里没有它） |

## 9. 证据与复现

```bash
# 1) 取 .text 并平坦反汇编（本机口径，未用 Ghidra）
python3 - <<'PY'
import struct
d=open('local/th17.v1.00b/th17.exe','rb').read(); pe=struct.unpack_from('<I',d,0x3c)[0]
n=struct.unpack_from('<H',d,pe+6)[0]; o=struct.unpack_from('<H',d,pe+20)[0]
so=pe+24+o
for i in range(n):
    s=so+i*40; nm=d[s:s+8].rstrip(b'\0').decode()
    vsz,va,rsz,ro=struct.unpack_from('<IIII',d,s+8)
    if nm=='.text': open('/tmp/_text.bin','wb').write(d[ro:ro+vsz])
PY
objdump -D -b binary -m i386 -M intel --adjust-vma=0x401000 /tmp/_text.bin > /tmp/th17_full.asm
grep -c 4b5a24 /tmp/th17_full.asm       # = 10（本文 §1 穷举口径）
# 2) 逐指令仿真 + 闭式对照（同源 scratch，本次复跑一致）
python3 local/_scratch/th17/th17_item_value_emulator.py
```

- 关键地址：`0x434400`（点道具）/ `0x434200`（P 道具）/ `0x433410`（道具主循环 + type 分派）/
  `0x433b40`（type 8）/ `0x433f09`（type 9..14）/ `0x4341c0`（14 项跳转表）/ `0x430404`（PIV 初始化）/
  `0x42cfd9`（HUD）/ `0x450b70`（弹窗）/ `0x4b5a24` `0x4b5a2c` `0x4b5a28`（PIV 三兄弟）。
- 常量/表：`0x4a0d6c`（type→分值 float 表）、`0x4a3d30`（100.0f）、`0x4a1790` / `0x4a17a8` / `0x4a1778`（难度表）、
  `0x4a16e8`（`front.anm` 串）、`0x4a16f4`–`0x4a1740`（HUD ASCII 格式串）。

## 按 mod 的「额外机制」口径重述（2026-09-18）

### ★ mod 里的线高口径（用户 2026-09-18）

> 「**14 15 16 17 的线就按照实际 POC 来吧**」

本节记的**本作原生线**（上表的 128 / 148 与它的角色条件）**只作文档**：
mod 侧一律用 **TH18 的实际 POC**（`player+0x47998`，live 128 ↔ 224）作锚，
即 `d = y_src − POC_TH18(t)` —— 与 `coeff_0` 同一个锚、同一个 `d`。
（由此 th12–th17 的 `coeff_i / coeff_0` 是常数：线上 1、线下 **5/6**，见 [`OVERVIEW`](../OVERVIEW.md)。）


参照系：`G = G_0 × coeff_0 + Σ ΔG_i × coeff_i`（`G_0 = G_18`）。本节一手 = 复核 `th17:0x434200`–`0x4342a0`（蓝点支）
与两个写点、以及三个全局的 vendor 名。

| 项 | 一手 | 结论 |
| --- | --- | --- |
| 载体 / 量纲 | `GLOBALS = th17:0x4b59c0`（vendor），`inner` 在 `+0x1c` ⇒ `CURRENT_PIV` = `inner+0x48` = **`th17:0x4b5a24`** ✅、`MINIMUM_PIV` = `+0x4c` = `th17:0x4b5a28`（只…（详见本节正文） | 与旧记录一致 |
| 线高 | `th17:0x43423e` `cmp ds:0x4b59f4,1` → `eax=0x80` / `edx=0x94` / `cmove` ⇒ 线 **128，条件为 1 时 148**；`ds:0x4b59f4` = `GLOBALS+0x34` = `inner+0x18` = vendor **`CHARACTER`** | ★ 与 th14 / th15 同：**按角色**切换 |
| 判谁 | `th17:0x434219` `eax = [ds:0x4b77d0]`（vendor = **`PLAYER_PTR`**）、`xmm1 = [eax+0x614]` ⇒ **自机 y** | 自机 y |
| 线上 / 线下 | 线 ≥ 自机 y ⇒ 满值（`trunc10(PIV/100)`）；否则 `(3/4)·trunc10(PIV/100) × (1 − t/450)`，`t = (int)自机 y − 线` | ⇒ **0.75 族** |
| 例外位 | `th17:0x434265` `cmp [item+0xc58],3` ⇒ 无视高度满值 | 同族字节（th16 是 `+0xc50 == 4`） |
| 写点 | `th17:0x433b74`（满 P，`+0x2710` = +100 显示）、`th17:0x433f3a`（type 9–14 查表 +5…+50 显示）、初始化 `th17:0x4303c0`/`:0x4303e7`/`:0x430404` | **只有两个加值写点**、**无减法**、**无擦弹站点** |
| `ΔG_17` / `coeff_17` | `ΔG_17` = PIV 本身；`coeff_17` = 线上 `1` / 线下 `0.75×(1−t/450)` | ⇒ **进 `coeff_17`**；**无额外项** |
