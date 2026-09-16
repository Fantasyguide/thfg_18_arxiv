# TH10 左下角那块 HUD —— 蓝点图标 / 最大得点 6 位 / 下面那条槽

> **版本**：TH10 v1.00a（`th10.exe`，487,936 B，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th11:0x…`。
> **方法**：一手平坦反汇编（`objdump -d -M intel --no-show-raw-insn`）+ 常量按 PE 节表 VA→文件偏移直读
> + `thanm -l 10 -m local/vendor/eclmap/anmmap/v4.anmm local/th10.v1.00a/dat/front.anm`（v4 map 自注「for version 4: StB, Mof」）
> + 贴图像素直读（`front/front00.png`）。
> **可信度**：块内 7 个 VM 的脚本/sprite/坐标/锚点/层 ✅；「连击槽」的载体、每帧增减写点、门限、
> 填充方式 ✅（指令原文 + 常量原字节）；数字位数与前导零 ✅；**数字的 RGB 颜色 🟡**（见 §5）。
> **为什么放在 `player/`**：这块是 `zGui`（玩家侧运行时 UI），由道具分派与自机位置驱动；`engine/` 里
> 没有 `hud/` 层，而 `engine/player/th10/` 已经是 TH10 玩家侧一手结论的家（见 `01-bomb-full-chain.md`）。
> 数值侧（信仰的增减）在 [`../../faith/th10/01-faith-and-points.md`](../../faith/th10/01-faith-and-points.md)，
> 本文只解**HUD 那一块的画面**。

## 0. 一句话结论

左下角那一块 = `zGui`（全局指针 `th10:0x47770c`）里**连续 7 个 VM**（`Gui+0x8094`，stride `0x3ac`，
装载循环 `th10:0x413d5c`–`th10:0x413d7e` 一次给它们装 `front.anm` **script 51..57**，全部 `layer(15)`）：

| VM | 脚本 | sprite | pos（640×480）| 是什么 |
| --- | --- | --- | --- | --- |
| 0（`Gui+0x8094`）| 51 | **sprite29**（16×16）| (32, 448) | **蓝点图标** |
| 1..5（`Gui+0x8440`+`i·0x3ac`）| 52..56 | sprite30→引擎换 `数字+30` | (48/55/62/69/76, 450) | **最大得点** 5 个动态位 |
| 6（`Gui+0x969c`）| 57 | sprite30（**恒 '0'**）| (83, 450) | 最大得点的**末位**，恒 0 |

用户口径逐条核实：**一个蓝点 ✓**（VM0，在数字左边）；**右侧两行**——上面一行是最大得点（6 位）、
**下面那条槽就是 VM0..VM6 之外的另一个东西**：它是 `GLOBALS+0x14` 的一个 `zTimer`（信仰/连击计时器），
**不占 VM**，由引擎直接画成**两条实心矩形**（`th10:0x4158b6`）。

## 1. 发现 → 验证：块由哪 7 个 VM 组成

### 1.1 组与脚本号（一手）

`Gui::initialize` 之后有一段 VM 装配（`th10:0x413bc0`–`th10:0x413d7e`）：六组 VM 分别拿到脚本号，
用 `AnmVm::set_script`（`th10:0x43e710`，签名 `eax=VM, ecx=zAnmLoaded*, ebx=script`）逐支装入：

| 组基址 | 支数 | 脚本号 | 一手 |
| --- | --- | --- | --- |
| `Gui+0x10` | 10 | 10..19 | `th10:0x413cb9` |
| `Gui+0x24c8` | 10 | 20..29 | `th10:0x413cca` |
| `Gui+0x4980` | 9 | 30..38 | `th10:0x413cf6` |
| `Gui+0x6a8c` | 4 | 47..50 | `th10:0x413d1a` |
| `Gui+0x793c` | 2 | 80..81 | `th10:0x413d46` |
| **`Gui+0x8094`** | **7** | **51..57** | `th10:0x413d6a`（`lea ebx,[esi+0x33]`，esi=0..6）|

⇒ 左下角那块 = 最后一组。`front.anm` script51..57 的正文（`thanm` 导出，`-o` 带偏移）：

| script | 正文（逐字）| pos | sprite |
| --- | --- | --- | --- |
| 51 | 见下方正文 | (32, 448) | sprite29 |
| 52..56 | 同上**逐字相同**，只有 `pos` 的 x 不同：48 / 55 / 62 / 69 / 76；sprite 都是 sprite30 | (48+7i, 450) | sprite30 |
| 57 | 同上，`pos(83,450,0)` | (83, 450) | sprite30 |

- script51 的正文（`-o` 导出的逐条原文，其余六条把 `sprite29`/`pos` 换掉即可）：

```
sprite(sprite29); ins_85(1); randMode(1); pos(32,448,0); resampleMode(1); anchor(1,1);
layer(15); alpha(0);
+20: alphaTime(20,0,255); stop();
     case(3): alphaTime(8,1,32);  stop();
     case(2): alphaTime(8,1,255); stop();
```

- `sprite29` = `(256, 80, 16, 16)`；`sprite30..39` = `(272+8i, 80, 8, 9)` —— **十个数字字形**
  （`local/th10.v1.00a/anm/front/front.anm.txt` 的 sprite 表；实测像素：字形是**白笔画**，笔画外是黑描边）。
- `anchor(1,1)` = **（左，顶）**（取值表 0=中 / 1=左 / 2=右、0=中 / 1=顶 / 2=底，
  见 [`../../anm/OVERVIEW.md`](../../anm/OVERVIEW.md) 的「锚点的跨版本演化」）。**本文自己复核过**：
  `front.anm` script2..5（sprite0/1/2/3 = `(0,0,32,480)`/`(32,0,224,480)`/`(0,480,384,16)`/`(0,496,384,16)`）
  在 `pos(0,0)/(416,0)/(32,0)/(32,464)` 下**正好铺满 640×480 的四条边** ⇒ 只有「pos = 矩形左上角」成立。
  本仓的 `210/211_mof_faith_*.anm.txt` 注释与 `mof.c` 的常量都已经按这个口径写，**无需订正**。
  ⇒ 下面所有矩形都按**左上角锚**算（这一点决定了「数字末位右沿 89 与槽右端 89 对齐」那句交叉验证）。

### 1.2 蓝点图标（用户口径「一个蓝点」）

| 项 | 值 | 一手 |
| --- | --- | --- |
| anm / 脚本 / sprite | `front.anm` script51 → sprite29（`(256,80,16,16)`）| 上表 |
| 640×480 空间 | 左上 (32, 448)，占 16×16 ⇒ **x 32..48 / y 448..464** | `pos(32,448)` + `anchor(1,1)` |
| 表面 px（×2）| **x 64..96 / y 896..928** | `DAT_0056aca0` = 分辨率倍率，640×480 下 ×2 |
| 可见墨迹 | x 34..46 / y 449..462（png 的 alpha bbox 相对精灵框 = (2,1)-(14,14)）| `front00.png` 直读 |
| 颜色 | 画里自带：主色 `(51,51,204)` 蓝 + `(34,34,119)` 暗蓝 + 黑描边 | 同左 |
| 层 | `layer(15)` | script51 |

### 1.3 最大得点：**6 位**、`front.anm` sprite30..39、前导零照画

绘制循环在 `Gui::on_tick`（`th10:0x414a5c`–`th10:0x414abd`）：

```
414a60: mov ebp,ds:0x474c4c        ; ★ 值 = GLOBALS+0xc = 信仰(存储)
414a66: lea ecx,[edi+0x8094] ; push ecx ; call 0x43ee30   ; run VM0（图标）
414a6d: mov esi,0x2710             ; 首位除数 10000
414a77: add edi,0x8440             ; 从 VM1 起
414a7d: mov ebx,0x5                ; ★ 只跑 5 次
414a82: mov eax,ebp / cdq / idiv esi         ; 商 = 该位数字，余数回写 ebp
414a93: lea edx,[eax+0x1e]                   ; ★ sprite 号 = 数字 + 30
414a98: call 0x43e5a0                        ; AnmVm::set_sprite（sprite 号落 VM+0x384）
414aa2: imul esi (魔数 0x66666667, sar 2)     ; esi /= 10 → 1000 / 100 / 10 / 1
414ab1: call 0x43ee30                        ; run VM
414abf/414ac3: lea ecx,[edi+0x969c] ; call 0x43ee30   ; ★ 第 7 支只 run、不换 sprite
```

| 断言 | 一手 |
| --- | --- |
| 位数 = **5 动态 + 1 恒 '0' = 6 位** | 循环 5 次（`th10:0x414a7d`）+ script57 的 sprite 全程没人改（全 exe 无写 `Gui+0x969c` 的 sprite 字段）|
| 末位为什么恒 '0' | 显示值 = 存储 × 10（[`../../faith/th10/01`](../../faith/th10/01-faith-and-points.md) §1 的三重证据）⇒ 十进制个位**恒为 0** |
| 字体 = **`front.anm` sprite30..39**（不是 ascii）| 引擎写的是 **sprite 号**（`+0x384`），`th10:0x43e5a0` 只在 `zAnmLoaded` 的 sprite 表里取条目 |
| 前导零 | ✅ **照画**：5 次循环里没有任何「为零就跳过」的分支（`th10:0x414a82`–`th10:0x414abd`），值 5000 → `050000` |
| 字距 | 7 px（pos 48→55→62→69→76→83），字形 8 px 宽 ⇒ **相邻字形差 1 px 重叠** |
| 颜色 | 🟡 见 §5 |

**位置上的一处交叉验证**：`pos` 的 x = **48 + 7i**（i=0..5）、字形 8 px、`anchor(1,1)` = 左上
⇒ 六格的墨迹 = **x 48..89**（末格 83..89）；下面那条槽满槽时的最右 = **89**（§2.3 的 `51+38`）。
两者**右对齐在同一条线上** ⇒ 槽确实是「数字那两行里下面那一行」，不是另有出处的装饰。

## 2. 下面那条槽到底是什么 —— 用代码证明

**结论（✅ 一手）：它不是贴图、不是分段、也不是 UV 裁切；它是 `GLOBALS+0x14` 那个 `zTimer` 的值，
被画成两条按比例填的实心矩形。** 语义 = **信仰/连击计时器**：计时器没走完时**信仰不衰减**，
走完（= 0）时信仰开始按帧掉 —— 这就是社区叫它「連撃タイマー / 连击槽」的那条机制。

### 2.1 载体与量纲

| 项 | 一手 |
| --- | --- |
| `GLOBALS` | `th10:0x474c40`（`mov edi,0x474c40` 出现在 8 个站点，见 `th10:0x40e675`/`th10:0x41b46e`/…，每次喂给 `th10:0x412ff0`）|
| zTimer 结构 | `+0x14` previous = **`0x474c54`**、`+0x18` current = **`0x474c58`**、`+0x1c` current_f = `0x474c5c`、`+0x20` game_speed 指针 = `0x474c60`、`+0x24` flags = `0x474c64` |
| 单位 | **帧**。上限 `0x82` = **130 帧** |
| 初始化 | `th10:0x413022` 把 previous 写成哨兵 `0xfff0bdc1`、`th10:0x41302b` 把 game_speed 指针写 `0x476f78`(= 1.0f)、flags `\|= 1` |

`AnmVm` 里没有它 —— **槽没有 VM、没有 sprite、没有脚本**（这就是为什么逐 VM 数块是数不出来的）。

### 2.2 每帧增减的写点（一手穷举）

| 方向 | 站点 | 值 | 一手 |
| --- | --- | --- | --- |
| **+** | `th10:0x412ff0`（`Timer::addTime(&GLOBALS+0x14, frames)`，内部 `th10:0x44bf40`）| 按道具 | 9 个调用点 |
| **上限** | `th10:0x413035` | `current = 0x82`、`current_f = 130.0f`（`0x43020000`）| 已满时不再加 |
| **−** | `th10:0x418a10` | `Timer::addTime(-1.0f)` = **每帧 −1 帧** | 每帧函数 `th10:0x418a00`，由 `GameThread::on_tick` 在 `th10:0x418764` 调用 |
| 归零时的**副作用** | `th10:0x418a26` | 计时器 ≤ 0 时 `信仰 -= GLOBALS+0x10`（速率，下限 18）| 见下 |

充能帧数（每个调用点前的 `push` 立即数，`th10:0x412ff0` 的 9 个调用点）：

| 站点 | 帧 | 归属（道具分派表 `th10:0x41b8a0`，下标 = type−1）|
| --- | --- | --- |
| `th10:0x41b473` | +60（`0x3c`）| type 1/10（P 点）|
| `th10:0x41b4a0` | +60 | type 1/10 的另一条支线 |
| `th10:0x41b575` | +100（`0x64`）| type 2（蓝点）/ type 5 的合流 |
| `th10:0x41b5cc` | +120（`0x78`）| type 3（大信仰点）|
| `th10:0x41b5e9` | +3 | type 8（微信仰点）|
| `th10:0x41b624` | +60 | type 9（小信仰点）|
| `th10:0x41b6cc` / `th10:0x41b6f6` | +20（`0x14`）| type 4/11（大 P）|
| `th10:0x40e681` | +10（`0xa`）| 敌人侧（`EnemyData::on_tick` 命中结算）|

⇒ 道具类型与帧数的对应表和 [`../../faith/th10/01`](../../faith/th10/01-faith-and-points.md) §3.1 的「连击计时器」列一致。

**它保护什么（这条是本节的重点）**：`th10:0x418a00` 是 `GameThread::on_tick` 每帧调一次的
`Globals` 维护函数（调用点 `th10:0x41875f`–`th10:0x418764`，`eax = 0x474c40`）：

```
418a00: mov ecx,[eax+0x18]        ; current
418a06: jle 0x418a17              ; 槽没走完 ⇒ 什么都不做（信仰不掉）
418a08: push 0xbf800000 (-1.0f) / lea esi,[eax+0x14] / call 0x44bf40   ; ★ 每帧 −1 帧
418a17: mov ecx,[eax+0xc]         ; 槽 == 0 ⇒ 这里才是信仰衰减
418a1a: mov esi,0x1388            ; 地板 = 存储 5000（显示 50000）
418a23: mov edx,[eax+0x10]        ; ★ 速率 = GLOBALS+0x10
418a26: sub ecx,edx ; cmp edx,0x12 / mov [eax+0x10],0x12  ; 速率下限 18
418a3c: 钳到 0x1388
```

**⚠️ 这个每帧函数是带门的**（调用点 `th10:0x418740`–`th10:0x41875d` 三个前置条件，任一不满足就整段跳过）：

| 门 | 地址 | 含义 |
| --- | --- | --- |
| `[0x477704]+0x10 == 0` | `th10:0x418746` | **场上没有 Boss** 才跑（有 Boss 时槽不倒数、信仰不掉）|
| `[Gui+0x9eb8] == 0` | `th10:0x418751` | Gui 没有「对话 VM」才跑 |
| `[edi+0x14] >= 0x5a` | `th10:0x418759` | 开场 90 帧之后才跑 |

⇒ **一帧槽满 → 信仰不掉；槽空 → 信仰每帧 −(≥18) 存储**（= 显示 −180/帧 = **−10800/秒**），
但只在「无 Boss、无对话、非开场 90 帧」时发生。
社区那句「无敌人 −10800/s」在这里有落点（速率下限 18 × 10 × 60）；速率初值 `1`（`th10:0x40e26b`）
与 `9`（`th10:0x417a00`）在第一次衰减时就被抬到 18（`th10:0x418a30`）。
⚠ 与 [`../../faith/th10/01`](../../faith/th10/01-faith-and-points.md) §3.4 的「找不到任何对应」相抵，
本文以指令原文为准（那条结论应订正为「衰减存在，但由**槽**门控、速率 = `GLOBALS+0x10`」）。

### 2.3 画法与填充方式（★ 一手）

`Gui::on_draw`（`th10:0x415800`）里唯一读 `0x474c58` 的地方，`th10:0x4158b6`–`th10:0x415937`：

```
4158b6: mov eax,ds:0x474c58 ; test eax,eax ; je 0x41593c   ; ★ current == 0 ⇒ 整条不画（不是画空框）
4158c6: movzx ebx,BYTE PTR [ebp+0x8393] ; shl ebx,0x18     ; 颜色 = Gui 的 UI alpha（低 24 位 = 0）
4158c0: fild DWORD PTR ds:0x474c58                         ; v = current（整数！不是 current_f）
4158d0: fmul DWORD PTR ds:0x470cb8                         ; v × 0.2923077
4158da: mov [esp+0x10],0x424c0000 (51.0f)                  ; 矩形 1：x1 = 51
4158e2: mov [esp+0x14],0x43e68000 (461.0f)                 ;           y1 = 461
4158ea: fadd DWORD PTR ds:0x470d2c (51.0f)                 ;           x2 = 51 + v×k
4158f0: mov [esp+0x1c],0x43e78000 (463.0f)                 ;           y2 = 463
4158fc: call 0x43bda0                                      ; 画实心矩形（D3D 顶点扇，无贴图）
41590c: fsub DWORD PTR ds:0x470afc (1.0f)                  ; 矩形 2 = 矩形 1 的 x2 − 1
41591b: mov [esp+0x10],0x42480000 (50.0f)                  ; 矩形 2：x1 = 50 / y1 = 460
415915: or ebx,0xffffff                                    ; 颜色 = alpha<<24 | 白
415937: call 0x43bda0
```

| 项 | 真值 |
| --- | --- |
| 贴图 | **没有**（`th10:0x43bda0` 是引擎自己的实心矩形绘制：`AnmManager::flush_sprites` + IDirect3DDevice 直接提交 4 顶点）|
| 分段 | **不是**（没有分段 sprite、没有「一格一格点亮」的逻辑）|
| 裁切 / UV | **不是**（不改任何 VM 的 UV / scale）|
| 实际做法 | **一条按比例填的实心矩形**：x2 = 51 + v×k，y 恒 461..463 |
| k | `th10:0x470cb8` = **0.2923077**（= 38/130）⇒ 满槽宽 **38 px**；色值不是整数倍关系，按原字节写 |
| 阴影 / 亮条 | 阴影 = `(51,461)-(51+v·k,463)` 纯 alpha（黑）；亮条 = `(50,460)-(50+v·k−1,462)` `alpha<<24\|白` |
| **alpha 借位** | 两个矩形的 A = **最左那位数字 VM 的 `color_1.A`**（`Gui+0x8393` = `Gui+0x8094+0x3ac+0x2ff`）⇒ 槽与数字**同一条淡入曲线**（`alpha(0)` + `alphaTime(20,0,255)`）|
| 轨道（槽底）| **无**：不满时不画框，只画到哪儿算哪儿 ⇒ 观感是「白线随计时器变短」 |
| alpha 来源 | **`Gui+0x8393` = VM1（最左那一位数字）的 `color_1` 的 A 字节**（= `Gui+0x8094+0x3ac+0x2ff`）|

**同一条值的第二个消费者**（也是它「有存在感」的地方）：`th10:0x426434` 画的**自机周边方框**
（`th10:0x426439: mov eax,ds:0x474c58 ; test eax,eax ; je` 早退；尺寸 = `current × 同一个 k`
+ 常量 225.0f/17.0f/16.0f/24.0f/2.0f）⇒ 计时器越长、自机周围那圈越大。
**它不参与计分**：`0x474c58` 的读者只有三处 —— 本节的两处 + 回放存读（`th10:0x42a4cb`/`th10:0x42a7b9`）。

## 3. 这一块的锚点与占用

| 元素 | 640×480 空间 | 表面 px（×2）| 尺寸（640 空间）|
| --- | --- | --- | --- |
| 蓝点图标 | 框 x 32..48 / y 448..464（墨迹 ≈ x 34..46 / y 449..462）| 框 x 64..96 / y 896..928 | 16×16 |
| 最大得点 6 位 | 六格框 x 48..91 / y 450..459（墨迹 x 48..89）| 框 x 96..182 / y 900..918 | 6×8 宽 × 9 高，字距 7 |
| 连击槽（满槽时）| x 50..89 / y 460..463（亮条 50..88，影 51..89）| x 100..178 / y 920..926 | 39×3 |
| **整块外接** | **x 32..89 / y 448..464** | **x 64..178 / y 896..928** | **57×16** |

与我们的货架锚点（[`HUD-LAYOUT.md`](../../../mods/th18.v1.00a/card-expand/HUD-LAYOUT.md) §1.2：
表面 `(257, 924)` = 640 空间 `(128.5, 462)`）的关系：原版这一块的**左沿在 32**、
**底沿在 464**（比我们的锚点线低 2 px）⇒ 原版坐标**不能直接搬进我们的槽**，
块内几何只能按「相对块左下角」的偏移抄（见 [`../th11/04-merged-hud-spec.md`](../th11/04-merged-hud-spec.md) §2）。

## 4. 与 TH11 的差异（合并前必读）

| 项 | TH10 | TH11 |
| --- | --- | --- |
| 数字来源 | `GLOBALS+0xc`（**存储**，显示 = ×10）| `GLOBALS+0x10`（显示 = ⌊存储/100⌋ 截到 10 的倍数）|
| 画法 | **6 个 VM**、sprite30..39（8×9）| `AsciiManager` 文本 `"%.6d*%1.2f"` |
| 槽 | zTimer，**矩形**填充 38 px | 交信度，**贴图裁切** 9+4×档 px |
| 槽位置 | 在数字**下面**（460..463）| 在数字**上面**（内容 446..454）|

数字侧的 TH11 细节见 [`../th11/03-hud-bottom-left.md`](../th11/03-hud-bottom-left.md)，
合并规格见 [`../th11/04-merged-hud-spec.md`](../th11/04-merged-hud-spec.md)。

## 5. 🟡 未定：数字的 RGB 颜色

- **事实**：script51..57 **一次都没调 `color()`**，只调 `alpha(0)` + `alphaTime(20,0,255)`
  （`th10:0x43f104` = `alpha` 写 `VM+0x2ff`；`alphaTime` 只动 A 通道）。
- **绘制读的是** `VM+0x2fc`（`th10:0x442e91: mov eax,[edi+0x2fc]`，随后四个顶点的 diffuse 都用它；
  `th10:0x442e7f` 按 flags bit15 决定读 `+0x2fc` 还是 `+0x300`）。
- **默认值**：`AnmVm::constructor`（`th10:0x402050`）`rep stosd` 把整个 `0x3ac` 清零 ⇒ 构造后 color = `0x00000000`；
  唯一写白的地方是**冷**版 `AnmVm::set_script`（`th10:0x43e913: mov DWORD PTR [eax+0x2fc],0xffffffff`），
  而 `Gui` 的装配走的是**温**版（`th10:0x43e710`，不碰 color）。
- ⇒ 静态上我**没能**把「Digit 的 color = `0xffffffff`」钉死；但有三条旁证指向**白**：
  ① 字形贴图本身就是白笔画（实测 `(255,255,255,255)` 与 `(0,0,0,153/255)` 两类像素）；
  ② 顶部那组**分数**数字（scripts 10..29）**同样不写 `color()`**，而分数在原版是白字 —— 同一套写法不可能一处白一处黑；
  ③ sprite 路径的 COLOROP **不是硬编码的**：`th10:0x4429d7` 取的是**每个 sprite 自己的** `[sprite+0x4]`
  （`mov edx,[edi+0x394] / mov eax,[edx+0x4]`）⇒ 只要那批字形的 sprite 记的是「只取贴图色」，
  RGB 就来自贴图、顶点色只管 A —— 这正好解释「ZUN 只写 `alpha()` 不写 `color()`」。
  🟡 该 per-sprite 字段的来源没追（`.anm` 文件里的 sprite 记录只有 `{id,x,y,w,h}` 20 字节，
  见 §6 的字节转储 ⇒ 是装载期算出来的）。
- **对我们的要求（与结论无关也成立）**：本仓的 HUD VM 起手必须**显式** `color(255,255,255)`
  （th18 侧的默认同样不可依赖），所以这条 🟡 不影响落地。

## 6. 复现命令

```bash
# ① 脚本 / sprite（v4 map 是 TH10 用的那张：mapfile 自注「for version 4: StB, Mof」）
local/vendor/thtk/build/thanm/thanm -l 10 -m local/vendor/eclmap/anmmap/v4.anmm \
    local/th10.v1.00a/dat/front.anm | sed -n '/^script script51 {/,/^script script53/p'
local/vendor/thtk/build/thanm/thanm -l 10 -o -m local/vendor/eclmap/anmmap/v4.anmm \
    local/th10.v1.00a/dat/front.anm | sed -n '/^script script51 {/,/^script script53/p'   # 带偏移

# ② VM 组 → 脚本号（6 个组）
objdump -d -M intel --start-address=0x413cb0 --stop-address=0x413d86 local/th10.v1.00a/th10.exe
# ③ 数字绘制循环（5 次 + sprite = 数字+30 + 第 7 支只 run）
objdump -d -M intel --start-address=0x414a5c --stop-address=0x414ad0 local/th10.v1.00a/th10.exe
# ④ 槽的两条矩形
objdump -d -M intel --start-address=0x4158b6 --stop-address=0x41593c local/th10.v1.00a/th10.exe
# ⑤ 每帧 −1 / 信仰衰减
objdump -d -M intel --start-address=0x418a00 --stop-address=0x418a41 local/th10.v1.00a/th10.exe
objdump -d -M intel --start-address=0x412ff0 --stop-address=0x41304d local/th10.v1.00a/th10.exe
# ⑥ 常量真值（.rdata 段 RVA 0x66000 / 文件偏移 0x65000）
python3 -c "
import struct;d=open('local/th10.v1.00a/th10.exe','rb').read()
f=lambda v: struct.unpack('<f',d[0x65000+(v-0x400000-0x66000):][:4])[0]
print('k=',f(0x470cb8),'51=',f(0x470d2c))"
# ⑦ .anm 文件里的 sprite 记录（只有 id,x,y,w,h 五个字段，20 字节；没有 colorop）
python3 -c "
import struct;d=open('local/th10.v1.00a/dat/front.anm','rb').read()
i=d.find(struct.pack('<4f',272.0,80.0,8.0,9.0))   # sprite30
print(hex(i), ' '.join(f'{b:02x}' for b in d[i-24:i+20]))"
```

## 7. 结论表 / 还缺什么

| # | 断言 | 可信度 | 版本 |
| --- | --- | --- | --- |
| 1 | 左下角块 = `Gui+0x8094` 起 7 个 VM，脚本 `front.anm` 51..57，全 `layer(15)` | ✅ 一手 | v1.00a |
| 2 | 蓝点图标 = sprite29（16×16），pos (32,448)，`anchor(1,1)` | ✅ 一手 | v1.00a |
| 3 | 最大得点 = **6 位**（5 动态 sprite30+digit + 1 恒 '0'），字距 7 px，前导零照画 | ✅ 一手 | v1.00a |
| 4 | 数字是 **vm 组**（`front.anm` sprite30..39），不是 ascii | ✅ 一手 | v1.00a |
| 5 | 「连击槽」= `GLOBALS+0x14` zTimer（current `0x474c58`），上限 130 帧 | ✅ 一手 | v1.00a |
| 6 | 槽每帧 −1（`th10:0x418a10`）；槽空 ⇒ 信仰每帧 −(≥18)（`th10:0x418a26`）；**两者都受「无 Boss / 无对话 / 开场 90 帧后」三道门约束**（`th10:0x418746`/`0x418751`/`0x418759`）| ✅ 一手 | v1.00a |
| 7 | 槽是**两条实心矩形**，宽 = `current × 0.2923077`（满 38 px），无贴图 / 无分段 / 无裁切 | ✅ 一手 | v1.00a |
| 8 | 槽的 alpha = `Gui+0x8393` | 🟡 用法一手，写点未穷举 | v1.00a |
| 9 | 数字的 RGB 颜色 = 白 | 🟡 静态没能钉死（脚本不写 RGB，见 §5）| v1.00a |

**订正（本文对既有文档的一处）**

[`../../faith/th10/01-faith-and-points.md`](../../faith/th10/01-faith-and-points.md) §3.4「槽空不扣信仰」
与「−10800/s 找不到对应」应订正为：**槽就是信仰衰减的门** —— 槽 > 0 时每帧只 −1 帧、信仰不动；
槽 == 0 时信仰每帧 −(≥18 存储)，速率字段是 `GLOBALS+0x10`。
写点漏掉的原因与 TH11 那篇遇到的是同一类：那处写的是 `mov [eax+0xc],ecx`（`eax = 0x474c40`），
**按绝对地址 grep `0x474c4c` 抓不到**（[`../../faith/th11/02`](../../faith/th11/02-koushin-signal.md) §1 也记过同一个坑）。

**还缺什么**

- 🟡 `Gui+0x8393` 的写点（那两个矩形的 alpha 由谁维护：贴到左下角的淡出？对话时的压暗？）。
- 🟡 `0x474c60`（game_speed 指针）除 `1.0f` 外有没有被改成别的（影响每帧 −1 的实际速率）。
- ❓ 原版实机上数字的颜色（§5）—— 需要一张截图或实跑取色。
- ❓ `ins_85(1)`（v4 的 85 = 无名 flag，落 flags bit `0x10000000`）的实际效果：本仓两个脚本都没搬（沿用既有取舍）。
