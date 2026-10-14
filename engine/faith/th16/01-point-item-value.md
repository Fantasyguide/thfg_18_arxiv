# faith — TH16 点道具身价（PIV / 最大得点）：载体 `0x4a57d8`、量纲 ×100、难度两表、涨入口全穷举、蓝点两分支公式

> **版本**：TH16 v1.00a（`local/th16.v1.00a/th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；
> 引用其他版本写作 `th18:0x…`。
> **方法**：一手平坦反汇编（python 解 PE 节区 → `objdump -D -b binary -m i386 -M intel --adjust-vma=0x401000`，
> 全 `.text` 172k 行；按区间取行的辅助脚本 `/tmp/th16work/range.py`）+ 常量/表/浮点按节区 VMA 直读原始字节
> + **全 exe 地址穷举**（4 字节小端搜 `0x4a57d8/0x4a57dc/0x4a57e0/0x4a586c/0x4a5790`，命中处逐条回**指令边界**复核）
> + ExpHP `th-re-data`（`local/vendor/th-re-data/data/th16.v1.00a/`）命名交叉 + 逐指令整数仿真对拍（`/tmp/th16work/fit.py`）。
> **可信度**：载体 / 量纲 / HUD 取值 / 难度两表 / clamp / **三个写点** / 蓝点两分支公式 / 「无第二项」= ✅ 一手；
> 「季节解放消弹掉 PIV 道具」的调用者身份 = 🟡；`0x4a586c` 用途 = ❓。

## 0. 一句话结论

TH16 的「最大得点」= **点道具身价**，载体 `CURRENT_PIV @0x4a57d8`（全局块 `0x4a5790` 的 +0x48），
**存储 = 显示 × 100**（`collect_piv` 里 `mulss xmm1,[0x494638]=100.0f`），HUD 取 `trunc10(⌊PIV/100⌋)`。

- **初值/上限按难度两表**（`0x4922a8` / `0x492290`，各自 ×100 存）；**只夹上界**，全 exe **没有任何减法/重设 PIV 的写点**。
- **涨 PIV 的通道只有两条道具**：`ITEM_F`（type 8，满 P 道具）`+10000 存储 = +100 显示`；
  `ITEM_PIV_5..50`（type 9–14）`+{5,10,20,30,40,50} 显示`。**擦弹 / miss / 灵击 / Boss 奖 / 季节解放本体都不写 PIV**。
- **蓝点（type 2）分值 = f(PIV)**：线上就**正好等于 HUD 上的最大得点**；线下一档衰减（0.75 倍再按 `Δy/450` 线性掉、十位取整、下限 10）。
- **季节不参与乘数**：`subseason` 只通过「解放消弹掉哪种 type（`8 + season_level`）」间接决定单颗 PIV 增量。
- **没有「第二项」**：分值公式里除了 PIV 与坐标没有任何加数；两个看着像的字段（`zBomb.__field_dc`、`0x4a5860/0x4a5868`）分别是**纯显示用**与**只写不读**。

## 1. 载体、量纲与 HUD 取值（Q1 / Q2）

### 1.1 载体（✅ 一手 + ExpHP 名）

全局块基址 `0x4a5790`（= ExpHP `zReplaySavedGlobals`，`0x4a57d8 - 0x48 = 0x4a5790` 对得上）：

| 项 | 一手地址 | ExpHP 名 | 本次一手事实 |
| --- | --- | --- | --- |
| 当前身价 | `0x4a57d8` | `CURRENT_PIV` | 存储 = 显示 ×100；**全 exe 3 个写点**，见 §3 |
| 初值 | `0x4a57dc` | `INITIAL_PIV` | **只写不读**：全 exe 唯一访问是初始化 `0x42cd76` |
| 上限 | `0x4a57e0` | `MAXIMUM_PIV` | 初始化写 `0x42cd7c` + 两处 `cmp/cmovg` 夹取 |
| 难度索引 | `0x4a57b4` | `DIFFICULTY` | 两表都按它索引（`0x42cd60`） |

> **发现**：`0x4a57d8` 的 4 字节小端在全 exe 命中 11 处，全部落在 `.text`，逐条回指令边界后是
> **3 写 8 读**，没有一处是别的指令的操作数（避开了 [`METHOD.md`](../../../METHOD.md) 里那条
> 「字节序列穷举会把立即数当命中」的坑）。
> **验证**：`objdump --start-address/--stop-address` 逐条打开 11 处；另检查寄存器相对形态
> （先 `mov reg,0x4a5790` 再用 `[reg+0x48]`）：全 exe 共 **14** 处把 `0x4a5790` 装进寄存器，
> 其中 9 处是「把全局块基址当 this/参数」——被调函数是 `0x42e590`（清计数）、`0x43ddf0`（`Globals::add_power`）、
> `0x449190`（全局标志位设置器，改 `globals+0x45c`）、其余同族，**无一碰 +0x48**；
> 另 5 处在存档/回放一族（见 §3.1 的 `rep movs`）。
> **结论**：TH16 的 PIV **只有 3 个逐指令写点**（✅ 高，版本 TH16 v1.00a）；
> 另有一条**整块搬运**的写路径（回放/状态恢复，§3.1 尾注），它不按偏移写、按 0x228 字节搬。
> **证据**：穷举表见 §3.1。

### 1.2 量纲：**存储 = 显示 × 100**（✅）

```
430dc0: mulss  xmm1,DWORD PTR ds:0x494638     ; 0x494638 = 100.0f（直读原始字节）
430dc8: mov    ecx,DWORD PTR ds:0x4a57d8      ; PIV（存储）
430dce: cvttss2si eax,xmm1                    ; 参数（显示点，浮点）→ 整数
430dd2: add    ecx,eax
430dd4: cmp    ecx,DWORD PTR ds:0x4a57e0      ; MAXIMUM_PIV
430dda: cmovg  ecx,DWORD PTR ds:0x4a57e0
430de1: mov    DWORD PTR ds:0x4a57d8,ecx
```

⇒ 唯一加 PIV 的函数 `Globals::collect_piv @0x430dc0` 明确把**显示量 ×100 后加进存储**。

### 1.3 HUD 取值：`Gui::on_draw_2` 里那条 ÷100 + 取十位（✅）

`Gui::on_draw_2 @0x428e70`（`Gui::initialize @0x426b00` 载入 **`"front.anm"`**（串在 `0x4921f4`），
`0x426b46` `push 0x429af0` 注册 on_tick、**`0x426bd6` `push 0x429b10`** 注册 on_draw，
而 `0x429b10` = `jmp 0x428e70` 的 thunk ⇒ 这就是**游戏内 HUD 的绘制函数**）：

```
42978c: mov    eax,0x51eb851f
429791: imul   DWORD PTR ds:0x4a57d8      ; ★ HUD 取 PIV 的那条指令
429797: sar    edx,0x5                    ; edx = ⌊PIV/100⌋
42979a: mov    ecx,edx ; shr ecx,0x1f ; add ecx,edx   ; 符号修正（负数向零）
4297a1: mov    eax,ecx ; cdq ; idiv esi   ; esi=10（0x42971b 设，区间内无改写）
4297aa: sub    ecx,edx                    ; ecx -= 余数 ⇒ trunc10(⌊PIV/100⌋)
4297ac: push   ecx ; lea eax,[esp+0x34] ; push eax
4297ae: call   0x4082b0                   ; AsciiManager 的千分位整数打印
```

`0x4082b0` 内部用 `0x491c7c` / `0x491c80` / `0x491c88` / `0x491c90` = `"%d"` / `"%d,%.3d"` / `"%d,%.3d,%.3d"` 一族格式串
（直读 .rdata）⇒ HUD 上那个数字是**带千分位的最大得点**，不是内部定点。

> **量纲换算**：**1 显示点 = 100 存储**。HUD 显示 **= `trunc10(⌊存储/100⌋)`**（10 的倍数向下取整）。
> **同一条算式的第二份独立副本**：`item_collect_piv_part_2 @0x42c860`（`imul 0x51eb851f`+`sar 5`+`idiv 10`+`sub`，返回同一值），
> **全 exe 只有 1 个调用者**：道具收取 `0x42fe33` ⇒ HUD **不**经这个函数，是内联复制。
> **量纲互证**（✅）：`ITEM_F`（type 8）满 P 时加 `0x2710 = 10000` 存储，而它的弹窗推的数是 `0x64 = 100`（`0x4303f2`）
> ⇒ 100 显示 = 10000 存储，与 ×100 完全吻合。

> **HUD 元素身份**（✅）：同一段里按顺序画的是**得点**（`Gui.current_score`，`[edi+0x170]` + `0x4a57b8`，`0x42941a`）、
> **下一档续命阈值**（`0x4a57fc` × 表 `0x491880`/`0x4917c4`，`0x4294b1`）、**残机/碎片**（`0x4a57f4`/`0x4a57f8`）、
> **灵击/碎片**（`0x4a5800`/`0x4a5804`）、**电源**（`0x4a57e4`/`0x4a57ec`/`0x4a57e8`）、**本项（PIV）**、**擦弹**（`0x4a57c0`）。
> 与 PIV 并列的每一项都能对上公认的 HUD 元素，剩下这一项又是「点道具身价」⇒ 它就是 HUD 的「最大得点」。
> 标签（「最大得点」四个字）是 `front.anm` 的贴图，未逐条追（❓ 只是标签）。

## 2. 初值与上下限：按难度的两张表（Q3，✅ 一手 + 表值直读）

初始化在同一条「新游戏/全局重置」链上（`FUN_42cb60`，由 `zStage` 注册的 thunk `0x42d1f0` = `jmp 0x42cb60` 进入）：

```
42cd60: mov    edx,DWORD PTR ds:0x4a57b4      ; DIFFICULTY
42cd66: imul   ecx,DWORD PTR [edx*4+0x4922a8],0x64   ; 初值表 ×100
42cd6e: imul   eax,DWORD PTR [edx*4+0x492290],0x64   ; 上限表 ×100
42cd76: mov    DWORD PTR ds:0x4a57dc,ecx      ; INITIAL_PIV = 初值[diff] ×100
42cd7c: mov    ds:0x4a57e0,eax                ; MAXIMUM_PIV = 上限[diff] ×100
42cd81: movd/cvtdq2ps/divss 100.0f/mulss 100.0f/cvttss2si
42cd9c: mov    ds:0x4a57d8,eax                ; CURRENT_PIV = INITIAL_PIV（÷100×100 是空转）
```

| 难度索引 | 初值表 `0x4922a8`（**显示**） | 上限表 `0x492290`（**显示**） |
| --- | --- | --- |
| 0 / 1 / 2 / 3 | 10000 | 500000 |
| 4 | 10000 | 500000 |
| 5 | 100000 | 1000000 |

- 索引 = `DIFFICULTY @0x4a57b4`：0–3 = Easy/Normal/Hard/Lunatic、4 = Extra（`0x4294bf` / `0x43ddd7` 都用 `cmp …,4` 判 Extra）。
- ❓ 第 6 项（索引 5）：本作是否有难度取到 5 未验（表里有这一项，值同 TH13 的 PH 档）。
- **clamp 点**（只有上界，两处）：`collect_piv` 的 `0x430dd4`/`0x430dda`；`collect_furu_powah` 的 `0x4303e0`/`0x4303eb`。
- **没有下界**：PIV 不会变小（无减法/无重设写点，见 §3）。
- **触发时机**：同一段里还把 `CURRENT_SCORE`（`0x42cd4b`）、`GRAZE`（`0x42cd45`）清 0、按模式/难度设残机（`0x42cdea`–`0x42ce10`）
  ⇒ 这是**每局开头**（新游戏）的那条初始化；`INITIAL_PIV` 本身无人读，所以局中**没有**「回到初值」的机制（✅ 负结论）。

## 3. 涨 / 跌入口穷举（Q4：全 exe 写点闭合）

### 3.1 写点（✅ 全 exe 只有 3 个）

| # | 地址 | 所在函数 | 动作 |
| --- | --- | --- | --- |
| 1 | `0x42cd9c` | `FUN_42cb60`（新局初始化） | `CURRENT_PIV = INITIAL_PIV`（§2） |
| 2 | `0x4303f5` | `Globals::collect_furu_powah @0x4303a0` | `PIV += 0x2710(10000 存储 = +100 显示)`，`cmovg` 夹上界 |
| 3 | `0x430de1` | `Globals::collect_piv @0x430dc0` | `PIV += xmm1*100`（xmm1 = 道具的显示增量），夹上界 |

**读点 8 个**（§1.3 的 HUD + `0x42c865` + `0x430181`/`0x43022c`（P 道具满转分两分支）
+ `0x4303ce`（满 P 道具）+ `0x43067a`/`0x43071d`（蓝点两分支）+ `0x430dc8`）。

> **尾注：唯一的「非逐指令」写路径 = 整块搬运**（✅ 一手，🟡 模块身份）。存档/回放一族把整个
> `zReplaySavedGlobals`（`0x4a5790` 起 `0x8a` 个 dword = `0x228` 字节，**含 +0x48/+0x4c/+0x50**）用 `rep movs` 搬：
> 存方向 2 处 `0x447918`（globals → 缓冲；同函数 `0x44793e` 还把 `0x4a57b8` 写进记录）、`0x448f45`；
> **取方向 2 处 `0x447b7d`、`0x44910b`（缓冲 → `edi=0x4a5790`）**——这是正常游玩之外唯一会改 `CURRENT_PIV` 的地方
> （replay 回放 / 状态恢复，值与录像帧一致）。它们不经过任何 `[reg+0x48]` 偏移访问，所以 §1.1 的「未发现寄存器相对写」
> 与本节「3 个写点」都要读成「**逐指令写点 3 个；另加 4 处 0x228 字节整块搬运（其中 2 处向 globals）**」。

### 3.2 道具 → PIV（✅）

| type | ExpHP 枚举 | 调用 | PIV 增量（**显示**） | 分值 |
| --- | --- | --- | --- | --- |
| 1 | `ITEM_POWER` | `collect_power_item @0x430100` | **0** | 未满 P：power+1、`add_to_score(100)`（`0x43032f`）；已满：走 §4 的 f(PIV)（`0x430177` / `0x430222`） |
| 2 | `ITEM_POINT` | `collect_point_item @0x430620` | **0** | f(PIV)（§4） |
| 3 | `ITEM_BIG_POWER` | `collect_big_power @0x4304a0` | **0** | 未满：power += `POWER_PER_LEVEL`、`add_to_score(100)`；已满：`add_to_score(20000)`（`0x4304f1`）**再** `add_to_score(20000)`（`0x43051e`） |
| 8 | `ITEM_F` | `collect_furu_powah @0x4303a0` | **+100**（`+0x2710` 存储） | 未满：power 直接拉到上限；已满：只加 PIV |
| 9–14 | `ITEM_PIV_5..50` | `collect_piv @0x430dc0`（经 `0x42fe0d`） | **+5 / +10 / +20 / +30 / +40 / +50** | `D/10`（D = 加完之后的显示最大得点） |
| 16 | `ITEM_SEASON` | `0x42fe58` → `0x43de50` | **0** | `add_to_score(10)`；季节槽 +1，跨档时弹窗+SFX 0x3f+`Gui__update_season_gauge` |

- **增量表**（浮点，直读 `0x4917e0`）：`{5.0, 10.0, 20.0, 30.0, 40.0, 50.0}`。
  ExpHP 名叫 `ITEM_PIV_AMOUNTS__ITEMS_8_TO_D`，但**实际索引基址是 `0x4917bc` + `type*4`**（`0x42fe0d`），
  type 9..14 才落在 `0x4917e0`..`0x4917f4` ⇒ 名字里的 8 是**错 1**（勘误见 §7）。
- **switch 门**：`ItemManager__on_tick_1d__body @0x42f4e0` 内 `jmp DWORD PTR [eax*4+0x4300bc]`（`0x42fd96`，
  `eax = item_type - 1`；表值 9–14 全指向 `0x42fe0d`，8 → `0x42fd9d`，2 → `0x42fdc1`）。
  收取门 = 道具坐标落在 `player.item_collect_box`（`[player+0x2c7e8]`/`+0x2c7ec`，`0x42fd6a`–`0x42fd84`）。

### 3.3 逐事件：谁碰 PIV、谁不碰（✅，负结论过指令边界）

| 事件 | 一手地址 | 是否写 PIV |
| --- | --- | --- |
| P 类道具（type 1） | `0x430100` | **否**（满 P 时转分，不动 PIV） |
| 大 P（type 3） | `0x4304a0` | **否** |
| 满 P 道具（type 8） | `0x4303a0` | **是**（+100 显示） |
| 点类道具（type 2） | `0x430620` | **否** |
| PIV 道具（type 9–14） | `0x42fe0d` → `0x430dc0` | **是**（+5..+50 显示） |
| 擦弹 | `0x444cf0` | **否**：只 `GRAZE 0x4a57c0`+1（夹 `0x5f5e0ff`）与 `0x4a57c4`+1 |
| miss / 掉命 | `0x442628`（`Player::on_tick` 内的状态计时分支，🟡 状态身份未钉） | **否**：半径消弹传 **kind=0**，而 `gen_items_from_cancel` 对 kind 0 直接 `return`（`0x416a0c`）⇒ 一颗道具都不掉，更无 PIV 写点 |
| 灵击 / 季节解放（消弹本体） | `0x416d20` / `0x416e20`（kind=4） | **否**（本体不写；但消弹**掉 PIV 道具**，见 §5） |
| Boss 奖 / ECL 加分 | `0x41de08`（ECL 加分指令），`0x42c139`（过关 `阶段号×1000000`），`0x42e2ab`/`0x42e386`（`(5×残机+灵击)×1000000`） | **否**，都只 `add_to_score` |

## 4. 蓝点（type 2）被收时的分值公式（Q5，✅ 逐指令）

`Globals::collect_point_item @0x430620`，先算 POC 线 `line = (CHARACTER==3) ? 148 : 128`（`0x4a57a4` = `CHARACTER`，
魔理沙 = 3；`0x430626`–`0x43063a`），再看两个门：

```
43065a: comiss xmm0(line),xmm1(player.y) ; jae 0x430713   ; 线上（y ≤ line）
430663: cmp    [item+0xc50],4            ; je  0x430713   ; 或 state==4（正被吸走）也算满额
430670: ...线下一档
```

**线上（`0x430713`）**：

```
ecx = ⌊PIV/100⌋ ; edx = ecx mod 10 ; ecx -= edx      ; D = trunc10(⌊PIV/100⌋)
eax = ecx/10（idiv 10 魔数 0x66666667 + sar 2）; esi = eax*10   ; = D（D 本来就是 10 的倍数）
if (esi <= 0) esi = 10
popup(item.pos, esi) ; add_to_score(esi) ; NUM_POINT_ITEMS_COLLECTED(0x4a57d4)++
[0x4a5868]++ ; [0x4a5860] += esi ; LAST_POINT_OR_POWER_COLLECTED_POS(0x4a5870) = player.pos
```

⇒ **线上蓝点分 = HUD 上的最大得点本身**（`0x430756` 的下限 10 只是防空值）。

**线下（`0x430670`）**（逐指令）：

```
Q  = ⌊PIV/100⌋（0x51eb851f + sar 5 + 符号修正）        ; 0x43067a-0x43068a
R  = Q mod 10（idiv 10）                               ; 0x43068f
DY = (int)player.y - line                              ; 0x430691 / 0x4306a0
a  = trunc(3*(R-Q)/4)                                  ; 0x430697-0x4306af（cdq/and 3/add/sar 2）
x  = a * DY                                            ; 0x4306b2
ecx = trunc( x * 0x91a2b3c5 >> 32 + x >> 8 )           ; 0x4306b5-0x4306c4 ★= trunc(x/450)
ecx += trunc(3*(Q-R)/4)                                ; 0x4306b7-0x4306cf
value = max(10, 10 * trunc(ecx/10))                    ; 0x4306d1-0x4306ee
popup(item.pos, value) ; add_to_score(value) ; 0x4a57d4++
```

- 魔数 `0x91a2b3c5` + `add edx,esi` + `sar edx,8` 经逐指令仿真核实 **= 有符号除以 450**（`ceil(2^40/450)` 正好是该魔数）；
  所以闭式是 **`value ≈ trunc10( 0.75·(Q−R)·(1 − DY/450) )`，下限 10**（`fit.py` 对 (PIV, DY) 网格逐点与仿真一致）。
- 例（PIV 显示 10000，即初值）：`DY=1 → 7480`、`DY=100 → 5830`、`DY=160 → 4830`；线上恒为 `10000`。
- **同一套公式的独立第二份**：P 道具在满 P 时走 `collect_power_item` 的 `0x430177`（线下）/ `0x430222`（线上），
  指令序列逐条同形 ⇒ 互为交叉验证（✅）。
- **线上 / 线下确实分档**（✅ 一手指令），且 `item.state == 4`（已被吸走）按线上满额算。
- 🟡 未解：「0.75 倍 + 450」是 ZUN 的原式写法（如 `3*d10/4 − 3*d10*dy/1800`）还是别的等价形，代码只能给出这一串整数运算。

## 5. 季节 / 子季节是否参与（专项，✅ 除标注处）

**结论：不做乘数、不进分值公式；只通过「解放消弹掉哪种 type」间接决定单颗 PIV 增量。**

1. **公式侧不参与**（✅ 负结论）：`collect_point_item`（`0x430620`）、`collect_power_item`（`0x430100`）、
   `collect_piv`（`0x430dc0`）三条链里**没有任何** `SUBSEASON(0x4a57ac)` / 季节槽 / `season_level` 的读数，
   只有 PIV、坐标、常量。
2. **参与方式 = 选 type**（✅）：`gen_items_from_cancel @0x416a00` 的 **kind 4** 分支：

```
416b7e: cmp edx,4 ; jne 落空
416bbb: push 0x10（ITEM_SEASON）→ call ItemManager::spawn(0x430960)     ; 消弹必掉一颗季节道具
416bc2: eax = SUBSEASON_BOMB_PTR(0x4a6da4)
416bc7: cmp [eax+0x30],1 ; jne 结束                                     ; ★ 子季节解放进行中才继续
416be7: eax = [eax+0xd8]        ; season_level（0..6）
416bf5: add eax,8               ; ★ type = 8 + season_level
416c10: call ItemManager::spawn  ; 掉 type 8..14 的道具
```

   ⇒ `season_level` 0 → `ITEM_F`（满 P 道具，满 P 时 +100 显示）；1..6 → `ITEM_PIV_5 / 10 / 20 / 30 / 40 / 50`。
   **等级越高，消弹掉的 PIV 道具单价越高**——机制上是换 type，不是乘法。
   `season_level` 的来源：`0x40dc68`–`0x40dc84` 扫表 `SEASON_POWER_LEVEL_REQUIREMENTS @0x4a583c`（6 项）数出等级写 `[bomb+0xd8]`。
3. **谁调消弹**（🟡 事件身份）：`BombSub*/BombMain*` 各 `method_10`/`on_tick` 都 `push 4` 当 kind 再调
   `BulletManager::cancel_radius_as_bomb @0x416d20` / `cancel_rectangle_as_bomb @0x416e20`
   （`0x40e411`/`0x40eb98`/`0x40ec41`/`0x40eff1`/`0x40f518`/`0x40f8b1`/`0x410049`/`0x4104d1`/`0x410b14`/`0x410bf0`/`0x4112b9`/`0x4117e1`）；
   激光侧的同名取消函数在 `LaserLine/LaserInfinite/LaserCurve::cancel_as_bomb_{circle,rectangle}` 里也调 `0x416a00`。
   🟡 未逐个钉「哪位调用者是解放、哪位是灵击」——本作两者都是「季节解放」（主炸弹 + 子季节炸弹两个对象）。
4. **解放中还额外做了两件事**（都是「更容易收到 PIV 道具」，不是加算）：
   - **强制吸附**：`0x42f88f`–`0x42f8a1`，`Bomb::subseason_condition_42f090 @0x42f090` 判 `[bomb+0x30]==1 && [bomb+0x38] < 0x2710`
     ⇒ 道具进「飞向自机」态（`0x42fbc1` 段置 `state=4`）。
   - **累计显示**：每收一颗 PIV 道具，`0x42fe1e`–`0x42fe2b` 把本次显示增量累加进
     `SUBSEASON_BOMB->__field_dc`（`zBomb+0xdc`，float）；解放收尾 `0x40dc28`–`0x40dc8a` 把它搬到 `__field_e0`、
     起 `release_piv_gain_display_timer(+0xe4)`（`call 0x406490`）、把 `__field_dc` 清 0；
     `Bomb::on_draw @0x40de30` 用 ASCII **`"+%d"`**（格式串 `0x491d54`）画在 `__field_fc`，颜色按 `[bomb+0xf8]`（= season_level）
     取 7 色表（`0x40decd`–`0x40df02`，`0x60606060`→`0xffffff30`）。
     **这个累计值只用来显示**：全 exe 没有第二处读 `zBomb+0xdc`（`addss/movss` 各一处，都在 `0x42fe2x`），
     `add_to_score` 的 13 个调用点也没有一个用到 `+0xdc`/`+0xe0`。

## 6. 有没有「第二项」：TH16 **没有**（Q6，✅）

- 蓝点分 = **f(PIV) 单变量**（§4），PIV 道具分 = `D/10`（`0x42fe33`–`0x42fe51`），
  P 满转分 = 同一套 f(PIV)（§4）⇒ 全 exe **不存在「基础身价 + 额外加数」的两项结构**，
  也没有 TH10「信仰」那种第二条计分行。
- 三个「长得像第二项」的字段，逐条否掉：
  - `zBomb` 对象 +0xdc（指针取自 `SUBSEASON_BOMB_PTR @0x4a6da4`）：**纯显示**（§5.4），不进分。
  - `0x4a5860`（累计值）/ `0x4a5868`（计数）：写入点 `0x430278`/`0x43027e`、`0x430369`/`0x43036f`、
    `0x430552`/`0x430558`、`0x430766`/`0x43076c`；**全 exe 无任何读取点**（只写不读）。
    新局初始化会把这一族一起清 0（`0x42e5e1`→`0x4a5860`、`0x42e5eb`→`0x4a5864`、`0x42e5f5`→`0x4a5868`，另有 `0x42e599`→`0x4a57d4`）。
  - `0x4a586c`：`0x42fbf9` 读 / `0x42fc09` 写（type 9..0xd 完全不碰它，其它 type 设成 8，type 0xe 保留旧值）；
    新局清 0（`0x42e5ff`，即全局块 +0xdc）。用途 ❓。
- 相关的计数只有 `NUM_POINT_ITEMS_COLLECTED`：**实际地址是 `0x4a57d4`**（`0x430706`/`0x430793` inc、`0x42e590` 清 0；
  ExpHP 标的 `0x4a57d0` 全 exe 无引用）。

## 7. 与既有文档 / ExpHP 命名的出入

1. [`engine/player/th16/06-resource-economy.md`](../../player/th16/06-resource-economy.md) §5 写「type8 满则 **+10000 PIV**」——
   **量纲要补**：10000 是**存储**，HUD 上是 **+100**（§1.3 与 `0x4303f2` 的弹窗数互证）。
2. 同文 §1/§5 的「type 9–0xe 走 `collect_piv` + `add_to_score`，并把值累加进 `SUBSEASON_BOMB->__field_dc`（下游未追）」
   —— **下游已追**：该累加只做「解放结束时的 `+N` 最大得点浮字」（§5.4），不参与任何分数；`type 9–0xe` 的
   `add_to_score` 实为 `D/10`（D = 加完之后的显示最大得点，`0x42fe33`–`0x42fe51`）。
3. 同文 §1 表把 type 3 记作「满则 +20000 分」——**代码里 `add_to_score(20000)` 出现两次**（`0x4304f1` 与 `0x43051e`），
   合 40000；🟡 只报事实（弹窗仍显示 20000），是否 ZUN 有意未证。
4. ExpHP 名勘误三处：`ITEM_PIV_AMOUNTS__ITEMS_8_TO_D @0x4917e0` 实际索引从 **type 9** 起（基址 `0x4917bc`）；
   `NUM_POINT_ITEMS_COLLECTED` 实际在 **`0x4a57d4`**（`0x4a57d0` 无引用）；
   `CONTINUES_USED @0x4a57b8` 被 HUD 当**第二个数字**与 `Gui.current_score` 一起打印（`0x42941a`）——🟡 存疑，本次未追。

## 8. 复核入口与开放项

**复核入口**：`CURRENT_PIV@0x4a57d8`、`INITIAL_PIV@0x4a57dc`、`MAXIMUM_PIV@0x4a57e0`、`DIFFICULTY@0x4a57b4`、
`CHARACTER@0x4a57a4`、`SUBSEASON@0x4a57ac`、`SUBSEASON_BOMB_PTR@0x4a6da4`、`MAIN_BOMB_PTR@0x4a6da8`、
`GUI_PTR@0x4a6dcc`、`ASCII_MANAGER_PTR@0x4a6d98`；表 `0x492290`/`0x4922a8`（难度两表）、`0x4917e0`（PIV 增量浮点）、
`0x494638`（100.0f）；函数 `0x42cb60`（新局初始化）、`0x430dc0`（collect_piv）、`0x4303a0`（ITEM_F）、
`0x430620`（蓝点）、`0x430100`（P 道具）、`0x4304a0`（大 P）、`0x42c860`（取值副本）、`0x428e70`（HUD 绘制）、
`0x4082b0`（千分位打印）、`0x416a00`（消弹掉道具）、`0x416d20`/`0x416e20`（消弹）。

**开放**：

- ❓ `0x4a586c` 的用途（PIV 道具相关，只在这个 item tick 里读写）。
- 🟡 「线下 0.75 倍 + 450 衰减」的原式写法。
- 🟡 难度索引 5 是否可达（表里有第 6 项）。
- 🟡 kind 4 消弹调用的「解放 vs 灵击」身份；本作两者同属「季节解放」的两个对象。
- ❓ HUD 上「最大得点」四字标签所在 `front.anm` script（本文只钉到数字本身）。

## 按 mod 的「额外机制」口径重述（2026-09-18；含线高条件的**角色**属性）

### ★ mod 里的线高口径（用户 2026-09-18）

> 「**14 15 16 17 的线就按照实际 POC 来吧**」

本节记的**本作原生线**（上表的 128 / 148 与它的角色条件）**只作文档**：
mod 侧一律用 **TH18 的实际 POC**（`player+0x47998`，live 128 ↔ 224）作锚，
即 `d = y_src − POC_TH18(t)` —— 与 `coeff_0` 同一个锚、同一个 `d`。
（由此 th12–th17 的 `coeff_i / coeff_0` 是常数：线上 1、线下 **5/6**，见 [`OVERVIEW`](../OVERVIEW.md)。）


参照系：`G = G_0 × coeff_0 + Σ ΔG_i × coeff_i`（`G_0 = G_18`）。本节一手 = 复核 `th16:0x430620`–`th16:0x4306e0`（蓝点支）、
三个 PIV 写点、`th16:0x42fe0d`（type 9–14 入口）与三张表/常量的原字节。

| 项 | 一手 | 结论 |
| --- | --- | --- |
| 线高 | `th16:0x430626` `cmp ds:0x4a57a4,3` → `eax=0x80` / `ecx=0x94` / **`cmove eax,ecx`** ⇒ 线 = **128，`CHARACTER == 3` 时 148** | ★ `ds:0x4a57a4` = vendor 名 **`CHARACTER`**（不是季节！） |
| 判谁 | `th16:0x430642` `edi = [ds:0x4a6ef8]`（= vendor **`PLAYER_PTR`**）、`xmm1 = [edi+0x614]` ⇒ **自机 y** | 自机 y |
| 例外位 | `th16:0x430663` `cmp [item+0xc50],4` ⇒ 无视高度满值 | ★ 例外值是 **4**（th13/14/15 是 3） |
| 线上 | 线 ≥ 自机 y ⇒ 值 = `trunc10(PIV/100)` | = HUD 的「最大得点」本身，无乘数 |
| 线下 | `(3/4)·trunc10(PIV/100) × (1 − t/450)`，`t = (int)自机 y − 线` | ⇒ **0.75 族**（与 th12–th15 逐指令同形） |
| 季节 | `CURRENT_SEASON_POWER` = `th16:0x4a5808`、`MAX_SEASON_POWER` = `th16:0x4a580c` 在蓝点支里**一次都不出现** | ✅ 旧结论「季节不做乘数」复核成立 |

**PIV 的全部写点（一手复核，**只有三条、全是加**）**

| # | 站点 | 增量 |
| --- | --- | --- |
| 1 | `th16:0x42cd76` / `:0x42cd7c` / `:0x42cd9c` | 初始化：`INITIAL_PIV`(`th16:0x4a57dc`) / `MAXIMUM_PIV`(`th16:0x4a57e0`) / `CURRENT_PIV`(`th16:0x4a57d8`) |
| 2 | `th16:0x4303ce`–`th16:0x4303f5` | `add edx,0x2710` = **+10000 存储 = +100 显示**，`cmovg MAXIMUM_PIV`（满 P 支） |
| 3 | `th16:0x430dc0`–`th16:0x430de1` | `PIV += (int)(farg × [ds:0x494638])`，`th16:0x494638` = **100.0** ⇒ farg 以**显示**为单位；夹 `MAXIMUM_PIV` |

写点 3 的唯一调用方 = `th16:0x42fe19`，实参取自**按 type 索引的浮点表** `th16:0x4917bc + type*4`：
表 `th16:0x4917e0` 起的 6 项原字节 = **{5, 10, 20, 30, 40, 50}**（type 9..14）⇒ **+5/+10/+20/+30/+40/+50 显示** ✅ 与旧记录一致。

⇒ **`ΔG_16` = PIV 本身；`coeff_16` = 线上 `1` / 线下 `0.75×(1−t/450)`（线 128、`CHARACTER==3` 时 148；判自机 y）** ⇒ 进 `coeff_16`。
**擦弹不涨 PIV**（三条写点里没有擦弹站点）⇒ 不含 `ΔG_擦弹`；**本作没有额外项**。

### ★ 顺带一条跨作发现：那条「148 线」是**角色属性**

| 作 | 条件常量 | vendor 名 | 判据 |
| --- | --- | --- | --- |
| th14 | `th14:0x4f5828` | **`CHARACTER`**（vendor 直名） | `== 1` ⇒ 148 |
| th15 | `th15:0x4e7404` = `GLOBALS+0x14` | `CHARACTER`（同版 `zGlobalsInner` 布局） | `== 1` ⇒ 148 |
| th16 | `th16:0x4a57a4` | **`CHARACTER`**（vendor 直名） | `== 3` ⇒ 148 |

⇒ 三作的「线 148」都是**某个自机/机体档**的固有属性，不是模式开关。
⚠ **mod 侧后果**：按已定口径 A（`d` 一律相对 **TH18 的 live 线**），**各作自己的这条角色条件在 mod 里不生效** ——
TH18 那边本来就有自己的 live POC（128 ↔ 224）。要连「那一档角色在 TH18 上也该 148」都复刻，就得另开一条口径（本文不建议）。
