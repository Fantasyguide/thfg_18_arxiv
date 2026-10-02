# anm/th18 §7 — `blendMode` 只管 RGB：**alpha 恒为 `Aa' := Sa`**（分离 alpha 混合）+ 顶点色两路寄存器

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址都属该版本；引用其他版本须写成 `th16:0x…`。
> **一手（2026-09-14）**：mod 侧卡 113「灵界」转场连续两轮实跑症状逼出来的
> （「转到自己，然后整体再消失」/「进场前 45 帧揭不开」）。全过程与判据见
> `mods/th18.v1.00a/card-expand/AUDIT.md` §CT-7。
> **可信度**：§1/§2 三组 render state + 分派 + 早退 = ✅ 一手（写入点 + 读取点两端都读过）；
> 「TH13 相反」= ✅ 一手（`th13:0x4034fa`）；§4 的推论口径 = ✅。

## 0. 一句话结论

**TH18 的 anm sprite 绘制永远开着「分离 alpha 混合」**：`blendMode(n)` 函数在**每条模式分支之前**
统一设 `SEPARATEALPHABLENDENABLE(0xce) ← 1`、`SRCBLENDALPHA(0xcf) ← ONE`、
`DESTBLENDALPHA(0xd0) ← ZERO`、`BLENDOPALPHA(0xd1) ← ADD` ⇒

```
Aa' = 1 · Sa + 0 · Da = Sa        （与 blendMode 无关、与 Da 无关）
```

**`blendMode(n)` 只决定 RGB。** ⇒ 任何"用 `blendMode` 把 alpha 做减法/乘法"的推理在 TH18 上都**不成立**；
想做"目的地 alpha 遮罩"只有一条路：**让画上去的那支 VM 自己携带想要的 `Sa`**（`Sa` 直接成为新的 `Da`），
并按**绘制顺序**决定谁最后写（后画的赢）。

## 1. 一手：`blendMode` 落地函数 `0x47d937`–`0x47d9a8`

模式分派之前统一设（`local/th18.v1.00a/th18.exe`，逐条）：

| D3DRS | 值 | 地址 | 含义 |
| --- | --- | --- | --- |
| `ZENABLE` (`0xf`) | `1` | `0x47d937` 一带 | 写/测深度 |
| **`SEPARATEALPHABLENDENABLE` (`0xce`)** | **`1`** | `0x47d94x` | **alpha 通道用单独的混合因子** |
| **`SRCBLENDALPHA` (`0xcf`)** | **`ONE`** | 同上 | 源 alpha 系数 = 1 |
| **`DESTBLENDALPHA` (`0xd0`)** | **`ZERO`** | 同上 | 目的地 alpha 系数 = 0（**Da 不参与**）|
| **`BLENDOPALPHA` (`0xd1`)** | **`ADD`** | 同上 | 只加 |

⇒ `Aa' = ONE·Sa + ZERO·Da = Sa` ✓（每条模式分支之后只改 `SRCBLEND`/`DESTBLEND`/`BLENDOP` = RGB 那三个）。

**对照 TH13**（同一件事它在**别处**做法相反）：画那 4 台"幕布"时它把分离 alpha 混合**关掉**
（`th13:0x4034fa` 的 `0xce ← 0`）⇒ 那时才是 `Aa' = Sa² + Da(1−Sa)`；
只有那块"清零四边形"临时开 ⇒ `Aa := Sa = 0`。
**⇒ 本仓前八轮把 TH13 的 alpha 式子套到 TH18 上，是错的**（这也是 §CT-7 的总根因）。

## 2. 一手：顶点色有**两路**寄存器，脚本指令只写其中一路

| 件 | 一手 | 事实 |
| --- | --- | --- |
| `colorMode` 选哪一路 | `0x47e202` | mode 0 → `+0x524`（color_1）、mode 1 → `+0x528`（color_2）；渲染器把**选中那一路**写进 `+0x52c` |
| `color(r,g,b)` 处理器 | `0x479fb9` 一带 | 只写 color_1 的 **RGB** |
| `alpha(a)` 处理器 | 同上 | 只写 **color_2 的 A** |
| 早退 | `0x4812e8` | `color_1.A == 0 && color_2.A == 0` ⇒ **整台 VM 不画** |

⇒ **只靠脚本指令定不住最终顶点色**（哪一路生效取决于 `colorMode`，而 `color()` 与 `alpha()` 分开写在两路上）。
要"确定"就只能**两路都写**（我们的做法：卡侧 C 每帧把两路写成同一个 ARGB）。

## 3. 于是"目的地 alpha 遮罩"在 TH18 上怎么写

以 TH13 灵界转场（`th13:0x4032d0`）的语义为准，逐件翻译到 TH18：

| 一手件（TH13）| TH18 侧等价写法 |
| --- | --- |
| 游戏区"清零四边形"（`Da := 0`、RGB 保留）| 一支**全屏 `drawRect`**，两路顶点色 A ≈ 0（取 `1/255` 而不是 0，**避开 §2 的早退**），RGB 保持不变 |
| 4 台斜置幕布（把笔触 alpha **写进 `Da`**）| 幕布 VM 顶点色 **A = 255** ⇒ `Sa` = 贴图自身的笔触 alpha ⇒ `Da := 笔触` ✓（不需要任何减算 mode）|
| script3 的 t=45 常驻 `drawRect`（把 `Da` **填满**）| 另一支全屏 `drawRect`，前 45 帧 A ≈ 0、第 45 帧起 A = 255 |
| astral 背景读 `Da`（`blendMode(7)` = `DESTALPHA/INVDESTALPHA`）| 同：`blendMode(7)` 让 `rgb = astral·Da + 下层·(1−Da)` ✓ |
| **生成顺序**：清零 → 幕布 → 填充 → astral | **必须照抄**：`Da := Sa` 是**覆盖**语义，后画的全屏矩形会把前面写的 `Da` 抹掉 ⇒ 顺序错了就是"永远揭不开/擦不掉" |

⚠ 已知差（数据侧消不掉）：一手幕布的权重是 `Sa²`（它当时关着分离 alpha 混合），TH18 恒开 ⇒ 我们只能拿到 `Sa`
⇒ **遮罩边缘略宽**。这是引擎级差异，不是脚本写法问题。

## 4. 给后来人的两条判据

1. **别再外推 TH13 的 alpha 公式**到 TH18：先看目标版本的 `blendMode` 落地函数有没有开 `0xce`。
2. **"用 blendMode 做 alpha 减法" 一律先怀疑**：TH18 上 alpha 通道与 `blendMode` 无关；
   真正决定 `Da` 的只有"谁最后画 + 它的 `Sa`"。

## ★ 订正（2026-09-15）：`alpha()` 写的是 **color_1** 的 A，不是 color_2

**本文 §2（及 `AUDIT.md` §CT-7）原把「`alpha()` 写 color_2 的 A」挂在 handler `0x479fb9` 上 —— 归属错了。**
一手逐条复核（`th18.exe` 指令分派表 `0x47ba4c` / `0x47bb98`）：

| 脚本指令 | opcode | handler | 写的字段 |
| --- | --- | --- | --- |
| `alpha` | 403 | `0x479efd` | **`0x479f15: mov %al,0x527(%ebx)` = `color_1.A`** |
| `color` | 404 | `0x479f44` | `0x524/0x525/0x526` = `color_1` 的 B/G/R |
| **`alpha2`** | **405** | `0x479fa1` | **`0x479fb9: → +0x52b` = `color_2.A`**（本文原先误挂的那条）|
| `color2` | 406 | `0x479fe8` | `+0x528` 一族 |
| `alphaTime` | 409 | `0x47aafd` | 每帧提交例程 `0x47c7f2: mov %al,0x527(%esi)` = **`color_1.A`** |

绘制取哪一路由**脚本的 `colorMode`** 决定（423 → handler `0x47b69b`，写 `flags_lo(+0x534)` bits17–19）：
`0x47e202` 处 **mode 0 → `+0x524`（color_1）**、**mode ≠ 0 → `+0x528`（color_2）**，取中的写进 `+0x52c`（真正画出去的顶点色）。

⇒ **推论（重要）**：不写 `colorMode` 的脚本 ⇒ mode 0 ⇒ 走 `color_1` ⇒ `color_2` **结构性恒 0**。
所以「诊断里 `c2=0`」**不是判决字段**（连一直可见的连击条也是 `c2=0`）。
当年 `trance.c` / `faith.c` 采取的「**两路都写**」修法**依然正确**（与 colorMode 无关），只是**原因**不是本文原来写的那个。

## 5. ★ 十个模式的完整表（2026-10-03 补，一手 = 跳表 + 逐 case 读 push）

分派：`movzx eax,[esi+0x3120e08]; cmp eax,9; ja 默认; jmp dword ptr [eax*4 + 0x47dcac]`
（`0x47d9ae`–`0x47d9be`）⇒ **模式 0..9**，跳表 `0x47dcac` 十项。
每条的 `SRCBLEND`(`0x13`) / `DESTBLEND`(`0x14`) / `BLENDOP`(`0x15`) 直接读各 case 的 push 序列：

| mode | 目标地址 | `SRCBLEND` | `DESTBLEND` | `BLENDOP` | 效果 |
| :-: | --- | --- | --- | --- | --- |
| 0 | `0x47d9c5` | `SRCALPHA` | `INVSRCALPHA` | `ADD` | 普通 alpha 混合 |
| 1 | `0x47d9f0` | `SRCALPHA` | `ONE` | `ADD` | **加算**（发光/闪光）|
| 2 | `0x47da1b` | `SRCALPHA` | `ONE` | `REVSUBTRACT` | 反向减算（`dst − src·a`，压暗；118 擦弹圈用）|
| 3 | `0x47da46` | `ONE` | `ZERO` | `ADD` | 覆盖；**附带 `ZENABLE(0xf) ← 0`**（不写深度）|
| 4 | `0x47da83` | `INVDESTCOLOR` | `INVSRCCOLOR` | `ADD` | screen（`1−(1−s)(1−d)`）|
| **5** | `0x47dad9` | **`DESTCOLOR`** | **`ZERO`** | `ADD` | **乘法**（`rgb′ = src × dst`）——**滤波/染色**用这个 |
| 6 | `0x47daae` | `INVSRCCOLOR` | `INVSRCALPHA` | `ADD` | — |
| 7 | `0x47db01` | `DESTALPHA` | `INVDESTALPHA` | `ADD` | 把**下层 alpha** 当遮罩（astral 背景那套）|
| 8 | `0x47db29` | `SRCALPHA` | `ONE` | `MIN` | — |
| 9 | `0x47db51` | `SRCALPHA` | `ONE` | `MAX` | — |

**两条使用要点**（本仓 2026-10-03 实装 92 的"金色闪光"时踩到）：

1. **乘法层（mode 5）不能用 `alpha()` 淡入淡出**：本文 §2 已证 RGB 只由 `blendMode` 决定、alpha 走分离混合
  （`Aa′ = Sa`）⇒ 乘法系数就是 VM 的**颜色**，要淡入就得用 **`colorTime()` 把颜色从白插到目标色**
   （白 = 乘 1 = 不变，这是天然的"无效果"端点）。
2. **`alpha` 别写 0**：`color_1.A == 0 && color_2.A == 0` 时整台 VM **不画**（`0x4812e8`，见 §2）——
   乘法层想"完全透明"是做不到的（它本来就不吃 alpha），要收就 `colorTime` 回白再 `delete()`。

### 5.1 推论：**有形状的乘法/滤波只能靠几何遮罩**

既然 RGB 因子由 `blendMode` 决定、alpha 只写 `Aa′`（§1/§2），那么：

- **贴图 alpha 不能给乘法"开洞"**：`blendMode(5)`（`DESTCOLOR/ZERO`）的因子是**源色 × 底色**，
  源色 = 顶点色 × **贴图 RGB** ⇒ 贴图透明处的 RGB 一样参与乘法（0 值还会把画面乘黑）。想做
  "只在圈内染色"，靠贴图 alpha 是**做不到**的。
- **正解 = 几何遮罩**：`drawPoly(1.0f, 32)`（实心盘）/ `drawRing(...)` / `texCircle(...)` 这些**图元**只
  栅格化形状内的像素 ⇒ 乘法天然只作用在形状内。半径就用 `scale`（`drawPoly(1.0f,…)` 的半径 = 1.0
  ⇒ 缩放值 = 半径），扩张用 `scaleTime`。
- 同理，**淡化**也不能用 `alpha`（不进 RGB）⇒ 用 `colorTime` 把颜色插回**白**（乘 1 = 不变）。
  实测用例：本仓卡 92 的"圆形扩张金色滤镜"（`assets/ability/scripts/398_great_work_tint.anm.txt`）。
