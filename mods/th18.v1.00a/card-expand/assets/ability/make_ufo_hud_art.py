#!/usr/bin/env python3
"""星莲船（系统卡 112）**飞碟槽 HUD** 的贴图 + 动画脚本 —— TH12 v1.00b 原版 1:1 切图。

## 为什么有这一批

用户 2026-09-18 实跑：「**飞碟槽 HUD 你没按照原版做**」—— 之前那版是 `drawRect` 拼的占位
（`HUD-LAYOUT.md` §4.1 的第一版，卡里自己标了 ❓ 自定）。这一批把**原版那几件**搬进来：
令牌槽（`front.anm` script80）、收集闪光（script81）、吸收数记录面板（script134/135）。

## ★ 一手订正（2026-09-18 第二轮）：槽的 **sprite 语义**与第一轮猜的相反

第一轮把 sprite59/60/61/62 当成「红/蓝/绿/彩虹」四色槽，**错了**。逐像素量了原图
（`front/front00.png` 的 8 张 24×16，统计 alpha>32 像素的均值）：

| sprite | 可见像素 | 平均 RGB | 是什么 |
| --- | --- | --- | --- |
| **59** | 68 | (0, 0, 0) | **空槽 = 纯黑框**（不是红色！） |
| **60** | 198 | (151, 62, 62) | 红槽 |
| **61** | 198 | (65, 72, 112) | 蓝槽 |
| **62** | 198 | (64, 112, 71) | 绿槽 |
| **63** | 68 | (0, 0, 0) | 空槽·亮版（与 59 同像素） |
| **64** | 198 | (235, 123, 123) | 红槽·**亮版**（暗版的 1.55 倍亮度） |
| **65** | 198 | (149, 134, 173) | 蓝槽·亮版 |
| **66** | 198 | (148, 173, 132) | 绿槽·亮版 |

⇒ **槽只有「空 / 红 / 蓝 / 绿」四态，原版根本没有彩虹色的槽**（与用户裁决「新增 2：没有彩虹色
token」完全吻合 ✓）。三处一手互相印证：

1. `FUN_004214b0` `th12:0x4214b0` 给每个槽写 `pending_interrupt = COLLECTED_UFOS[i] + 10`
   ⇒ 中断 **10 = 空**（sprite59）、**11 = 红**（60）、**12 = 蓝**（61）、**13 = 绿**（62）。
   （`COLLECTED_UFOS[i]` 用 1/2/3 记红/蓝/绿，0 = 空 —— 与 `FUN_004270b0` 的颜色 0/1/2 差 1。）
2. `FUN_00421690` `th12:0x421690` 每帧**直接写 VM 的 sprite 字段**（`0x53f80`）：
   值 = `COLLECTED_UFOS[i] + 59 + (闪烁 ? 4 : 0)` ⇒ **+4 = 亮版那一组**（59..62 / 63..66）✓
   ⇒ 这就是旧记录里那句「UFO 在场时令牌槽闪烁」的**真身**（原版是**换 sprite**，不是改 alpha）。
3. 槽位（哪个 VM 站哪一列）由 `FUN_004214b0` / GUI 构建时的中断 7/8/9 决定 = x 52/73/94 ✓

## 一手依据（`/tmp/front12.txt` = `thanm -l 12 -m v4.anmm th12.dat` 展开的 front.anm 文本）

| 件 | 原版脚本 | 层 | 一手要点 |
| --- | --- | --- | --- |
| **令牌槽** | `front.anm` **script80** | 20 | `anchor(0,0)`；`pos(52,456)`；**三个槽位 = 中断 7/8/9 → `posTime(8,0, 52/73/94, 456)`**（间距 21）；**四态 = 中断 10/11/12/13 → `sprite59/60/61/62`**；`alpha(0)` → `+60: alphaTime(20,0,255)`（出现）；**中断 17 = 入槽出现动画**（`alpha(0)+scale(0,0)` → 30 帧到 1）；中断 2 = 亮到 255、中断 3 = 压到 **alpha 64**；中断 14 = 槽回收（滑到 x=31 淡出 → `pos(82,464)` 淡入） |
| **收集闪光** | `front.anm` **script81** | 21 | 同一个 `sprite59`；15 帧后 `alpha(255)` + `scale(0.5)` → `scaleTime(8,4,1.5)` → `scaleTime(8,1,1.0)` → 31 帧 `delete()`；中断 7/8/9 = 槽位、10..13 = 四态 |
| **吸收数记录面板** | `front.anm` **script134**（+ 子 **135**） | 23 / 24 | 固定屏幕位 **`pos(528,288)`**；`sprite102 = front_ufo.png (0,0,96×96)`；`scale(0,0)` → `scaleTime(20,0,1.0)`；中断 1 = 缩回 0（134 不 delete，135 在 +20 帧 `delete()`）|

### 一手矩形（都是 1:1 切图，不缩放）

| 输出名 | 源 | 矩形 | 是什么 |
| --- | --- | --- | --- |
| `SLOT_EMPTY` | `front/front00.png` | sprite59 = (336,336,24×16) | 空槽（纯黑框） |
| `SLOT_RED` | 同 | sprite60 = (360,336,24×16) | 红槽 |
| `SLOT_BLUE` | 同 | sprite61 = (384,336,24×16) | 蓝槽 |
| `SLOT_GREEN` | 同 | sprite62 = (408,336,24×16) | 绿槽 |
| `SLOT_EMPTY_B` … `SLOT_GREEN_B` | 同 | sprite63..66 = (336/360/384/408, **352**, 24×16) | 四态的**亮版**（UFO 在场时闪烁的另一组） |
| `PANEL_FRAME` | `front/front_ufo.png` | sprite102 = (0,0,96×96) | 记录面板（外框那一张）|
| `PANEL_INNER` | 同 | sprite103 = (96,0,96×96) | 记录面板（内容那一张）|

## 移植改动（逐条；生成出来的脚本头里也引这一份）

1. **Δ1 层号**：原版 20/21/23/24（TH12 的 HUD 相机）。TH18 里 24/25 才是「会被暂停模糊罩住」的 HUD 组
   （`HUD-LAYOUT.md` §4.1 / `AUDIT` §BB）⇒ 本批统一改成 **25**。
2. **Δ2 删 `ins_85(1)`**：v4 助记符表里它是 `# 85: unknown flag`，TH18 的 opcode 表里没有这一条
   （`thanm` 直接报 `opcode 85 is not known to exist in version 8`）⇒ 不猜、不搬。
   （`randMode` / `resampleMode` 在 v8 里同名，逐条保留。）
3. **Δ3 坐标 ×2**：TH12 的 `front.anm` 是 640×480 空间；TH18 的 **UI 层 VM 坐标就是表面 px**
   （一手：TH18 自己的 HUD 图标根脚本 `pos(1056, 537)` = 640 空间 528×2；`HUD-LAYOUT.md` §1.1 的两列）。
   ⇒ 所有 `pos` / `posTime` 的 x、y **×2**（z 不动）。
4. **Δ4 尺寸 ×2**：同一件事的另一半 —— TH18 的 UI 层 `resolutionMode(2)` 画出来是 **1 texel = 1 表面 px**
   （一手 `AnmManager__draw_vm` `th18:0x481bcc`：`flags_hi` 位 20-22 = 2 ⇒ 尺寸 × `DAT_0056aca0 × 0.5` = ×1），
   而 TH12 的 1 texel = 1 个 640 单位 = **2 表面 px** ⇒ 所有 `scale` / `scaleTime` 的 x、y **×2**
   （先例：`faith_hud.c` 的 `ce_anm_set_scale(条, w640 * 2, h640 * 2)` 同一套口径）。
   ⚠ 不这么做的话槽/面板只有原版的一半大（同款踩坑记录：`AUDIT.md` 12447「resolutionMode(2) 会把
   texel 折半 ⇒ 画出来只有原版相对尺寸的一半」）。
4b. **Δ4b 静止 scale ×2（★ 2026-09-23 补，P3 漏掉的那一半）**：Δ4 只改了脚本里**写出来的**
   `scale` / `scaleTime`，而 TH12 的 `front.anm` script80/81 **通篇没有 `scale()`** ——
   槽的静止尺寸吃的是 **VM 初值**。那个初值是 **1.0**（一手 `AnmVm::wipe`：整块 `memset 0` 之后
   `scale = scale_2 = uv_scale = (1,1)`，见 `engine/anm/th16/02-vm-core.md` §3.3），
   在 TH12 的 640 空间 = 24×16 **屏幕** px ⇒ 到 TH18 的 1280×960 表面必须是 **48×32** ⇒ 静止值要写 **2.0**。
   **同一条脚本自证**：script80 的 `interruptLabel(17)`（新令牌入槽）目标 = `scaleTime(30,4,1.0,1.0)`
   —— 那是把槽**放大回静止尺寸**；Δ4 已把它改成 `2.0`。⇒ 若静止值不是同一个 2.0，
   令牌一入槽那一格就会突然**变成两倍大**（原版三格恒等大）。
   漏了这一条的实测后果（用户 2026-09-22 实跑「星莲船缺少 hud」）：三个槽按 **24×16 表面 px**
   （= 原版的一半线性尺寸、1/4 面积）画，而空槽贴图 sprite59 是**纯黑轮廓**（逐像素量过：
   68 个可见像素、均值 RGB (0,0,0)，alpha 150/255）⇒ 1280×960 的画面上等于看不见；
   且与 C 侧认领的块（132×32 = 三格 48 宽 + 两缝，见 `ufo.c` 的 `UFO_HUD_BLOCK_W`）**差一倍**。
   ⇒ 只有 302/303（吃 VM 初值的那两支）需要补这一句；304/305 自己第一句就 `scale(0,0)`，不涉及初值。
5. **Δ5 槽/闪光脚本的 x 位移**（面板不加）：`X_SHIFT_640` 现在 = **96**（★★ 2026-09-22 订正，
   见下）。这条位移**不是**"TH18 适配"，而是**原版自己的坐标空间换算**：
   TH12 的 `front.anm` / `bullet.anm` **世界空间**里**游玩区占 x ∈ [32,416]、y ∈ [16,464]**
   （两个端距都是内缩项的来源；同一份 anm 的 script82 大横幅写 `pos(224,208)` = 场地**中心**、
   script79 小横幅滑到 `x=32, y=467` = 场地左沿 / 下沿下 3 px —— 三处互证），
   而 **TH18 的 UI 层坐标是绝对表面 px**，场地在表面 px 里 = **x 256..1024 / y 32..928**
   （一手：相机 3 视口 `((ac80−408)/2, (ac84−472)/2, 408, 472)` = 居中的 408×472，
   `engine/anm/th18/02-render-stages.md` §2；用户 1280 截图实测「游玩区左边缘 257 / 底边 928」）。
   ⇒ 原版槽的**场地相对**位置 = x 52/73/94 − 32 = **20/41/62**、y 456 − 16 = **440**
   ⇒ TH18 表面坐标 = 256 + 2×20/41/62 = **296/338/380**、32 + 2×440 = **912**
   ⇒ `X_SHIFT_640` = **96**（= 128 − 32，把"场地左沿在 640 空间的 128"减掉）。
   ★ **为什么必须加这 96**（这是 2026-09-22 修 #1「缺少飞碟(token)槽位 UI」的根因）：
   不加 = 槽落在 640 空间 52/73/94 = **表面 104/146/188**，即 **TH18 场地左沿（表面 256）之外的黑边**；
   而空槽的贴图 sprite59 是**纯黑框**（逐像素量过：68 个可见像素、均值 RGB (0,0,0)）⇒
   黑底画黑框 = 用户实跑看不到。加 96 = 回到**场地左下角**（原版真正的位置）。
   ⚠ **口径冲突（已登记 `TUNING.md` §2.0v，待 owner 拍）**：116 的季节槽（表面 263..365 / 888..920）
   与 117 的动物灵格（257..449 / 876..924）也在同一条带子上 ⇒ 本块（272..404 / 896..928）
   与它们**横向重叠**。要退回"屏幕黑边"那一版就把这个数改回 **0.0**（重跑本脚本 + `make anm`）。
6. 脚本号从 **302** 起（196–301 已用）。
7. **Δ6 补 `resolutionMode(2)`（2026-09-23）—— ⚠ 它的"根因故事"已被推翻，但**写法保留**（见下）**：
   原始记录写的机制是「UI 列表脚本**不写** `resolutionMode` ⇒ 坐标与尺寸按 640×480 设计空间解释
   （×`DAT_0056aca0` = **×2**）⇒ 槽落到 y≈1824 = 屏外」。**这条是错的** —— 2026-09-23 由 108 session
   与本次（112）分别逐指令复核（`HUD-LAYOUT.md` §4.1b-订正；`AUDIT.md` §BE 详录 #57 §6）：
   `th18:0x4063d0`（绘制路径调用点 `th18:0x481770`，乘的是**位置**那三个分量）里
   `mode 1/3 → ×[0x56aca0]`、`mode 2/4 → ×[0x56aca0]×0.5`、**其余（含 mode 0 = 不写）直接跳过整段乘法**；
   而 1280×960 那一支（`th18:0x4735ce`）把 `[0x56aca0]` 写成 **2.0** ⇒ **mode 0 ≡ mode 2 ≡ ×1**。
   证据 ①（原生 `front.anm` script20..25「实测画在表面 (1048,…)」）**是按同一张错表推出来的，不是测量**
   （原脚本 `pos(524, 58)` 逐字核对：**确实没写** `resolutionMode`）⇒ 不能当证据。
   **⇒ 「从看不见变看得见」的真功臣是 Δ4b**（静止 scale 1.0 → 2.0：24×16 → 48×32 表面 px）——
   两条改动落在**同一个 commit**（`1cd64ba`），当时归因错了。
   **写法为什么保留**：`resolutionMode(2)` = 「本脚本的值以 **2× 设计空间（1280×960）** 为准」，
   在 1280×960 下是 no-op，在别的分辨率下（如 960×720 的 `DAT = [0x4b91c8]` 支）才生效 ——
   这正是本仓所有 HUD 脚本（144/145/210..214/105/136/306..308）的统一写法
   ⇒ **纪律不变**（`HUD-LAYOUT.md` §4.1b 的落笔规则仍照办：写 `resolutionMode(2)` + 表面 px 常量）。
   ⚠ **Δ4b 与 Δ6 是一对**（Δ4b 是真正生效的那一半）：1 texel = 1 表面 px 时静止 scale 必须写 2.0，
   否则槽只有原版的一半大。
8. **Δ7 摆位归 C 侧（★ 2026-09-23，用户实跑「槽位只有 1 个」的根因）—— 脚本一个字都不改**：
   一手 TH12 把三个槽位做成**中断 7/8/9 → `posTime(8, 0, 52/73/94, 456)`** 的目标点
   （一手里那条中断由 GUI 构建时发出：`th12:0x41d650` 建三只 script80 的 VM 后紧跟
   `mov edx,7/8/9; call [vm+0x494]`，逐指令读出）—— 我们原样移植了，但**在 TH18 里不落地**：
   实跑日志（`ufo/slot.vm` 1539 行）三格的脚本 `pos.x` **恒 = 296**（= 本脚本初始 `pos(296,912)`），
   即三格叠在同一个点上 = 用户看到的「只有 1 个」；而同一份日志里三格的 alpha/scale 各自按 30 帧
   爬升（= 中断 17 的配方真在跑）⇒ 中断机制本身没坏，坏的是**摆位那一条**。
   ⇒ 修法在 C 侧（`ufo.c` 的 `ufo_hud_place_at()`）：绘制点 = `pos(+0x30)` + `entity_pos(+0x5f0)` +
   `__pos_2(+0x4e4)`（一手 `th18:0x48171f`），所以每帧写 `entity_pos = 槽心 − (pos + __pos_2)`，
   让三项之和**恰为**槽心。**本脚本里的 296/338/380 与 912 因此仍是唯一的一份真相**
   （C 侧 `k_ufo_slot_x[]` 抄的就是这三个数）—— 这也是为什么这里**只**改注释、不改字节。
   ⚠ 中断 7/8/9 照旧发（配方原样保留）；将来若 TH18 侧那条路生效了，C 的差值会自动归零，画面不动。

## 输出

- `assets/ability/ufo_hud/{SLOT_*,PANEL_*,BANNER_*,SUMMON_*}.png`（12 张）
- ORDER 片段 `entries/ORDER.ufo_hud.txt`（一个图集 entry、12 个成员）
- 脚本 `assets/ability/scripts/{302..305}_ufo_hud_*.anm.txt`

跑法（幂等）：
    python3 assets/ability/make_ufo_hud_art.py
    # 把 ORDER 片段替换进 entries/ORDER.txt 末尾那一段
    make -C native anm
"""
import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC00 = REPO / "local/th12.v1.00b/anm/front/front/front00.png"
SRC_UFO = REPO / "local/th12.v1.00b/anm/front/front/front_ufo.png"
SRC02 = REPO / "local/th12.v1.00b/anm/front/front/front02.png"   # 召唤大横幅（entry1，THTX 304x32）
SRC_TXT = Path("/tmp/front12.txt")            # `thanm -l 12 -m v4.anmm th12.dat` 的 front.anm 文本
OUT = HERE / "ufo_hud"
SCRIPTS = HERE / "scripts"
ORDER_FRAG = HERE / "entries" / "ORDER.ufo_hud.txt"

# (输出名, 源图, entry 名, TH12 sprite 号, 期望矩形, 输出画布)
SPRITES = [
    ("SLOT_EMPTY",   SRC00,   "front/front00.png",    59, (336, 336, 24, 16), (32, 16)),
    ("SLOT_RED",     SRC00,   "front/front00.png",    60, (360, 336, 24, 16), (32, 16)),
    ("SLOT_BLUE",    SRC00,   "front/front00.png",    61, (384, 336, 24, 16), (32, 16)),
    ("SLOT_GREEN",   SRC00,   "front/front00.png",    62, (408, 336, 24, 16), (32, 16)),
    ("SLOT_EMPTY_B", SRC00,   "front/front00.png",    63, (336, 352, 24, 16), (32, 16)),
    ("SLOT_RED_B",   SRC00,   "front/front00.png",    64, (360, 352, 24, 16), (32, 16)),
    ("SLOT_BLUE_B",  SRC00,   "front/front00.png",    65, (384, 352, 24, 16), (32, 16)),
    ("SLOT_GREEN_B", SRC00,   "front/front00.png",    66, (408, 352, 24, 16), (32, 16)),
    # ★★ 2026-09-23（owner #17「飞碟槽**底下**还有一抹**毛笔**艺术化贴图，我们没有」）：
    #   一手 = TH12 `front.anm` **script79** 用的 `sprite67` = `front00.png` (336, 368, 96×16)
    #   —— **与我们已切过的 8 张槽图同一张 sheet**（59..66 在 y=336/352，67 就在它们下面的 y=368）。
    #   脚本原文（`local/th12.v1.00b/anm/front/front.anm.txt`，`thanm` 反出的文本）：
    #     `sprite(sprite67); resampleMode(1); anchor(1, 2); layer(19); alpha(128);
    #      pos(-96, 467); posTime(60, 4, 32, 467);` → `+30: alphaTime(20, 0, 255)`（淡入到位）
    #   ⇒ 位置 = 该 anm 空间 (32, 467)：x 是**场地左沿**、y 比场地下沿(464)低 3 ⇒ 换算到表面
    #     (32+96)×2 = **256**、(467)×2 = **934** ⇒ 正好压在我们那排槽（248..380 / 896..928）**下面** ✓
    #     （owner 原话「槽底下」）⇒ 它是**槽的底衬**，画在槽之前。
    ("BANNER_APPEAR", SRC00,  "front/front00.png",    67, (336, 368, 96, 16), (96, 16)),
    ("PANEL_FRAME",  SRC_UFO, "front/front_ufo.png", 102, (0, 0, 96, 96),     (128, 128)),
    ("PANEL_INNER",  SRC_UFO, "front/front_ufo.png", 103, (96, 0, 96, 96),    (128, 128)),
    # ★★ 2026-09-23（owner 实跑第 ③ 条「召唤动画是紫色三角（不对）」→ 回一手）：
    #   **TH12 召唤 UFO 时的真实表现**在 `th12:0x44a8a0` 里写得很清楚 —— 它建的是
    #   `front.anm` **script 0x52(82)** 的 VM，并写 `pending_interrupt = (色==-1?3:色-1) + 7`
    #   （`engine/ufo/th12/01-ufo-system.md` §5）。script82 = **大横幅**：
    #     `sprite68`（entry1 `front/front02.png` 的 (0,0,304,32)，THTX 就是 304x32）、`layer(6)`、
    #     `type(2)`、`pos(224,208)`（= 场地中心、场地上半）、`blendMode(1)`（加算）、
    #     `alpha(0) → alphaTime(20,0,128)`；`+30: scaleTime(30,2,2,2) + alphaTime(30,0,0)`；
    #     `+35/+40/+45: scriptNew(84)`（三只同样的横幅残影）；`+75: delete()`；
    #     中断 7/8/9/10 = 红/蓝/绿/彩虹（`color(255,128,128)` 一族）—— 与 §5 的 `+7` 逐字吻合 ✓。
    #   子脚本 83 = `sprite69`（(0,0,32,32) 的**32 宽窗口**）从 `pos(88,208)` `posTime(30,0,360,208)`
    #     + `scrollX(0.0296875)` ⇒ 一道**扫过横幅的擦除**（32 宽的窗口在 304 宽的贴图上滑 + 卷动 uv）。
    #   ⇒ 本图集加这两个成员（304x32 + 32x32），卡侧脚本 = 324（82）/ 325（83）/ 326（84）。
    #   ⚠ 它与我们此前"候选"的紫环（enemy.anm 141+139/140）/ 青六边形（103+102/104）**无关**：
    #     那两个是 owner #14 之前按"全 enemy.anm 里只有 4 支用 drawPoly"猜的，owner 已判**不对**。
    ("SUMMON_BANNER", SRC02,  "front/front02.png",     68, (0, 0, 304, 32),     (304, 32)),
    ("SUMMON_STRIP",  SRC02,  "front/front02.png",     69, (0, 0, 32, 32),      (32, 32)),
]

ATLAS_NAME = "ATLAS_UFO_HUD"
# 12 个成员：8 个 32x16 槽（两行 y=0 / y=18 —— 行间留 2 px，免得线性采样竖向渗色）
#             + 2 个 128x128 面板（y=40，到 y=168）
#             + 96x16 毛笔横幅（y=170..186）
#             + ★ 召唤大横幅 304x32（y=0，x=192..496 —— 槽只占 x<128，不冲突）
#             + ★ 召唤擦除窗口 32x32（y=192..224）
#   ⇒ 画布从 256x192 长到 **512x224**（只多这一条 304 宽的横幅，其余成员坐标一个没动）。
ATLAS_W, ATLAS_H = 512, 224
ATLAS_POS = {
    "SLOT_EMPTY":   (0, 0),   "SLOT_RED":   (32, 0),   "SLOT_BLUE":   (64, 0),   "SLOT_GREEN":   (96, 0),
    "SLOT_EMPTY_B": (0, 18),  "SLOT_RED_B": (32, 18),  "SLOT_BLUE_B": (64, 18),  "SLOT_GREEN_B": (96, 18),
    "PANEL_FRAME":  (0, 40),  "PANEL_INNER": (128, 40),
    "BANNER_APPEAR": (0, 170),      # #17 毛笔横幅：面板段结束于 y=168，170+16=186 ✓
    "SUMMON_BANNER": (192, 0),      # #③ 召唤大横幅 304x32：x 192..496 ≤ 512 ✓（槽的 x<128 不撞）
    "SUMMON_STRIP":  (0, 192),      # #③ 擦除窗口 32x32：y 192..224 ≤ 224 ✓
}

# ★ 槽的四态在 ORDER 里必须**连续四个**、且亮版那一组紧接其后（正好 +4）
#   （一手：`FUN_00421690` 写的值是 `COLLECTED_UFOS[i] + 59 + (闪烁 ? 4 : 0)`）
SLOT_ORDER = ["SLOT_EMPTY", "SLOT_RED", "SLOT_BLUE", "SLOT_GREEN"]
SLOT_ORDER_B = ["SLOT_EMPTY_B", "SLOT_RED_B", "SLOT_BLUE_B", "SLOT_GREEN_B"]

# (输出脚本号, 输出名, 源脚本号, x 是否要 +96 单位, 是否要补 Δ4b 的静止 scale)
#   `rest_scale` = True 只给**通篇没有 `scale()`** 的两支（TH12 的 script80/81）——
#   它们吃 VM 初值 (1,1)，必须显式补成 (2,2)，理由见文件头 Δ4b。
SCRIPT_PORTS = [
    (302, "ufo_hud_slot",        80,  True, True),    # 令牌槽（3 个 VM 共用这一支，靠中断选槽位/四态）
    (303, "ufo_hud_flash",       81,  True, True),    # 收集闪光（一次性，31 帧后自删）  ⚠ True = 受 X_SHIFT_640 影响
    (304, "ufo_hud_panel",      134,  False, False),  # 吸收数记录面板（父；子 = 305）—— 自己第一句就 scale(0,0)
    (305, "ufo_hud_panel_inner", 135, False, False),  # 面板内容（134 的 scriptNew）
    (315, "ufo_hud_banner",       79, True,  True),   # ★ #17 毛笔横幅（owner：槽底下那一抹）
                                                      #   ⚠ 编号由构建侧按文件名顺序定（脚本号被占则顺延，`make anm` 会报「编号应为 N」）
                                                      #   x_shift=True：它也在那个 +96 的 anm 空间里
                                                      #   rest_scale=True：script79 通篇**没有** `scale()`
                                                      #     ⇒ 吃 VM 初值 1.0（同 script80/81 的 Δ4b 坑）
]
LAYER_FROM, LAYER_TO = (19, 20, 21, 23, 24), 25   # Δ1（19 = script79 毛笔横幅那一层；TH12 里它也是 HUD 层，
                                                    #   搬到 TH18 必须进 25 那一组，否则按「不在 37..45 ⇒ 组 39」
                                                    #   落到暂停模糊罩之后，与 slot 的 25 不同批）
REST_SCALE = 2.0                              # Δ4b：VM 初值 1.0 的 TH18 等价（= Δ4 对"没写出来的那个数"的同一刀）
X_SHIFT_640 = 96.0                            # Δ5：**96 = 复刻**（2026-09-23 末轮一手定案，原 ❓自定 作废）。
                                              #     一手 TH12 `front.anm` 那个 anm 空间 = 640 屏幕空间**x 方向 +96**
                                              #     （该文件把场地中心写成 `pos(224, 240)`：场地 x 32..416 / y 16..464，
                                              #      中心 (224,240)；屏幕空间下该是 (320,240) 而**全文件没有 320**）
                                              #     ⇒ 一手槽 script80 `pos(52/73/94, 456)` = 屏幕 148/169/190
                                              #     = 表面 296/338/380 ✓；52+96 = 148 与 Δ5 是同一个数。
                                              #     ⚠ 改回 0.0 = 回到"屏幕最左黑边"104/146/188 —— **错的**，
                                              #       且正是用户实跑报「缺少槽位 UI」那一版（见 ufo.c 常量区定案注）。
                                              #     ★ 2026-09-23 owner 裁决 ②：**摆位改由 C 侧 `k_ufo_slot_x[]` 接管**
                                              #       （Δ7 自校准：`entity_pos = 槽心 − (pos + pos2)`），槽心取 272/314/356
                                              #       = 照 113 的块基准 248/928（`trance.c`）。⇒ 这里的 96 留作**一手原值**，
                                              #       脚本那两项与 C 的差值由 entity_pos 补掉 ⇒ 本文件**不必**跟着改。
U640_TO_SURF = 2.0                            # Δ3（坐标）/ Δ4（尺寸）


def read_sprite_rects(path: Path, entry: str) -> dict[int, tuple[int, int, int, int]]:
    if not path.is_file():
        raise SystemExit(f"缺 {path}（先跑 thanm：见文件头「跑法」）")
    txt = path.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'name: "%s".*?sprites: \{(.*?)\n    \}' % re.escape(entry), txt, re.S)
    if not m:
        raise SystemExit(f"{path}: 找不到 entry {entry}")
    out: dict[int, tuple[int, int, int, int]] = {}
    for sm in re.finditer(r"sprite(\d+): \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+) \}", m.group(1)):
        out[int(sm.group(1))] = (int(sm.group(2)), int(sm.group(3)), int(sm.group(4)), int(sm.group(5)))
    return out


def scripts_of(text: str) -> dict[int, list[str]]:
    out: dict[int, list[str]] = {}
    cur = None
    for line in text.splitlines():
        m = re.match(r"script script(\d+) \{", line)
        if m:
            cur = int(m.group(1)); out[cur] = []; continue
        if cur is not None:
            out[cur].append(line)
            if line.strip() == "}":
                cur = None
    return out


def make_sprites() -> list[str]:
    # ⚠ 矩形从**anm 文本**读（不是从 PNG 读）——两个 entry 都在同一份 front.anm 文本里
    rects = {e: read_sprite_rects(SRC_TXT, e)
             for e in ("front/front00.png", "front/front_ufo.png", "front/front02.png")}
    OUT.mkdir(parents=True, exist_ok=True)
    lines = []
    for name, src, entry, sid, want, canvas_wh in SPRITES:
        got = rects[entry].get(sid)
        if got != want:
            raise SystemExit(f"{entry} 的 sprite{sid} 矩形漂了：期望 {want}，实际 {got}")
        x, y, w, h = got
        im = Image.open(src).convert("RGBA").crop((x, y, x + w, y + h))
        vis = sum(1 for a in im.getchannel("A").tobytes() if a > 32)
        if vis < 20:
            raise SystemExit(f"sprite{sid} @({x},{y}) 可见像素只有 {vis} —— 切错位置了")
        canvas = Image.new("RGBA", canvas_wh, (0, 0, 0, 0))
        # ★★ 2026-09-22 订正：**居中**贴进 32×16 画布（不是左上角）。
        #   脚本写的是 `anchor(0, 0)` = **锚心**（`AUDIT.md` §AL10/§AL20 两次实跑钉死：显式
        #   `anchor(0,0)` 与 sprite 的默认都是"图心 = VM 位置"）。原版 sprite 是 **24×16**，
        #   它的图心就是 VM 位置；把 24 宽的图塞进 32 宽的画布，**只有居中**才能让图心仍然落在
        #   VM 位置上。左上角对齐会让整排槽（与闪光）**统一左偏 4 单位 = 8 表面 px**
        #   —— 第一轮记的"居中会多 4 px 偏移"其实是**切图矩形**取宽了 8 px（切到隔壁 sprite），
        #   不是贴法的错。 ⚠ 画布必须保持 32×16：ORDER 里那十行写死 `32x16`
        #   （"矩形 = 成员原图自己的像素尺寸"是 ORDER.txt 的约定，改它会动到冻结段校验）。
        canvas.paste(im, ((canvas_wh[0] - w) // 2, (canvas_wh[1] - h) // 2))
        canvas.save(OUT / f"{name}.png")
        lines.append(f"{name:<14} ability/ufo_hud/{name}.png   "
                     f"@{ATLAS_POS[name][0]},{ATLAS_POS[name][1]},{canvas_wh[0]}x{canvas_wh[1]}")
    return lines


def _f(v: str, mul: float = 1.0, add: float = 0.0) -> str:
    n = float(v.strip().rstrip("fF"))          # 源码字面量带 `f` 后缀，先剥掉
    return f"{(n + add) * mul:.1f}f"


def conv_line(line: str, x_shift: bool) -> str:
    """Δ3（坐标 ×2）/ Δ4（尺寸 ×2）/ Δ5（槽与闪光脚本的 x 再 +96 单位）。

    ⚠ 只动这四种 opcode 的**数值参数**，其余一字不改（sprite / 中断号 / 时机 / alpha 表照抄）。"""
    dx = X_SHIFT_640 if x_shift else 0.0
    m = re.match(r"^(\s*)pos\(\s*([-\d.f]+),\s*([-\d.f]+),\s*([-\d.f]+)\s*\);\s*$", line)
    if m:
        return (f"{m.group(1)}pos({_f(m.group(2), U640_TO_SURF, dx)}, "
                f"{_f(m.group(3), U640_TO_SURF)}, {m.group(4)});")
    m = re.match(r"^(\s*)posTime\(\s*(\d+),\s*(\d+),\s*([-\d.f]+),\s*([-\d.f]+),\s*([-\d.f]+)\s*\);\s*$", line)
    if m:
        return (f"{m.group(1)}posTime({m.group(2)}, {m.group(3)}, "
                f"{_f(m.group(4), U640_TO_SURF, dx)}, {_f(m.group(5), U640_TO_SURF)}, {m.group(6)});")
    m = re.match(r"^(\s*)scale\(\s*([-\d.f]+),\s*([-\d.f]+)\s*\);\s*$", line)
    if m:
        return f"{m.group(1)}scale({_f(m.group(2), U640_TO_SURF)}, {_f(m.group(3), U640_TO_SURF)});"
    m = re.match(r"^(\s*)scaleTime\(\s*(\d+),\s*(\d+),\s*([-\d.f]+),\s*([-\d.f]+)\s*\);\s*$", line)
    if m:
        return (f"{m.group(1)}scaleTime({m.group(2)}, {m.group(3)}, "
                f"{_f(m.group(4), U640_TO_SURF)}, {_f(m.group(5), U640_TO_SURF)});")
    return line


def port(lines: list[str], name_map: dict[int, str], x_shift: bool, rest_scale: bool,
         src_num: int) -> list[str]:
    out = []
    for line in lines:
        if line.strip() == "}":
            break
        s = line
        m = re.search(r"sprite\(sprite(\d+)\)", s)
        if m and int(m.group(1)) in name_map:
            s = s.replace(f"sprite(sprite{m.group(1)})", f"sprite(@{name_map[int(m.group(1))]}@)")
        m = re.search(r"scriptNew\(script(\d+)\)", s)
        if m:
            s = s.replace(f"scriptNew(script{m.group(1)})", "scriptNew(@SCRIPT_UFO_HUD_PANEL_INNER@)")
        m = re.match(r"\s*layer\((\d+)\);\s*$", s)
        if m and int(m.group(1)) in LAYER_FROM:            # Δ1：层号 → 25
            s = s.replace(f"layer({m.group(1)})", f"layer({LAYER_TO})")
            out.append(conv_line(s, x_shift))
            if rest_scale:                                 # Δ4b：显式补上 VM 初值的 ×2（原脚本没有 scale()）
                out.append(f"    scale({REST_SCALE:.1f}f, {REST_SCALE:.1f}f);"
                           f"   // Δ4b：静止 scale（TH12 的 VM 初值 1.0 × 2 —— 一手还把它当"
                           f" `interruptLabel(17)` 的目标值）")
            # Δ6：照本仓 HUD 的统一写法显式写 `resolutionMode(2)`（= 值以 2× 设计空间 1280×960 为准）。
            #     ⚠ 2026-09-23 订正：它在本机的 1280×960 下**是 no-op**（mode 0 ≡ mode 2 ≡ ×1，
            #     见文件头 Δ6 与 AUDIT §BE 详录 #57 §6）；真正让槽"从看不见变看得见"的是 Δ4b。
            out.append("    resolutionMode(2);   // Δ6：值以 2x 设计空间（1280x960）为准（本机 1280x960 下 = no-op；"
                       "见生成器文件头 Δ6 的 2026-09-23 订正）")
            continue
        # Δ2：删掉 `ins_85(1)` —— v4 表里它是 `unknown flag`，TH18 没这条 opcode
        if re.match(r"\s*ins_85\([^)]*\);\s*$", s):
            out.append("    // Δ2：原版的 `ins_85(1)` 已删（v4 表里它叫 `unknown flag`，TH18 无此 opcode）")
            continue
        # ★★ Δ-OWNER（2026-09-23，owner 实跑第 1 条）：**闪光脚本（← script81）不再做缩放动画**。
        #   owner 原话：「收槽的动画还是不太对，收进去之后现在是**空槽位本身会有一个放大再缩小的动作**，
        #   一手显然不是」⇒ 那支 flash VM 画的**就是槽位那一张彩色框**（同一个 sprite）⇒ 它的
        #   `scale 0.5→1.5→1.0` 看起来就是"槽自己放大缩小" ⇒ 本批**只留 alpha 闪**。
        #   ⚠ 一手 script81 的默认路径**确实有**这三段 scale（原文见本文件 README 的对照表）⇒
        #     这是 **owner 口径**、不是复刻；回退 = 把下面这个 if 删掉（原文即回来）。
        if src_num == 81 and re.match(r"\s*scale(Time)?\([^)]*\);\s*$", s):
            out.append("    // Δ-OWNER：原版的这一行 scale 已按 owner 口径去掉（`" + s.strip() + "`）")
            continue
        out.append(conv_line(s, x_shift))
    return out


def main() -> int:
    if not SRC_TXT.is_file():
        raise SystemExit(f"缺 {SRC_TXT}（先跑：thanm -l 12 -m <v4.anmm> th12.dat 展开的 front.anm 文本）")
    order = make_sprites()

    # ★ 断死槽的四态在 ORDER 里连续、亮版正好 +4（卡侧 `ce_anm_set_sprite` 直接做加法）
    names = [n for n, _s, _e, _i, _w, _c in SPRITES]
    idx = {n: i for i, n in enumerate(names)}
    for i, n in enumerate(SLOT_ORDER):
        if idx[n] != i:
            raise SystemExit(f"槽四态的 ORDER 顺序错了：{n} 排在第 {idx[n]} 位（应为 {i}）")
    for i, n in enumerate(SLOT_ORDER_B):
        if idx[n] != i + len(SLOT_ORDER):
            raise SystemExit(f"亮版组的 ORDER 顺序错了：{n} 排在第 {idx[n]} 位（应为 {i + 4}）")

    ORDER_FRAG.write_text(
        "# 卡 112 的飞碟槽 HUD 贴图（TH12 front.anm 的 1:1 切图）\n"
        "#   生成器 = assets/ability/make_ufo_hud_art.py（矩形从 anm 文本读 + 断死）\n"
        "#   ★ 槽的四态 = SLOT_EMPTY/RED/BLUE/GREEN（连续 4 个），亮版那一组紧接其后（+4）——\n"
        "#     一手 `th12:0x421690` 每帧写的 sprite 就是 `COLLECTED_UFOS[i] + 59 + (闪烁 ? 4 : 0)`。\n"
        f"ATLAS {ATLAS_NAME} {ATLAS_W}x{ATLAS_H}\n" + "\n".join(order) + "\n", encoding="utf-8")

    src_scripts = scripts_of(SRC_TXT.read_text(encoding="utf-8", errors="replace"))
    name_map = {sid: name for name, _s, _e, sid, _w, _c in SPRITES}
    made = []
    for num, name, src_num, x_shift, rest_scale in SCRIPT_PORTS:
        if src_num not in src_scripts:
            raise SystemExit(f"源脚本 script{src_num} 不在 {SRC_TXT}")
        body = port(src_scripts[src_num], name_map, x_shift, rest_scale, src_num)
        d4b_common = [
            f"script script{num} {{",
            f"    // 卡 112（星莲船）飞碟槽 HUD —— TH12 v1.00b `front.anm` **script{src_num}** 的逐条移植",
            "    //（生成器 assets/ability/make_ufo_hud_art.py；sprite / 中断号 / 时机 / alpha 表逐条照抄）。",
            "    // ★ 移植改动：Δ1 层号 20/21/23/24 → **25**（TH18 里只有 24/25 会被暂停模糊罩住）；",
            "    //   Δ2 删 `ins_85(1)`（v4 的 `unknown flag`，TH18 无此 opcode）；",
            "    //   Δ3 坐标 ×2（TH12 的 640 空间 → TH18 UI 层的**表面 px**）；",
            "    //   Δ4 尺寸 ×2（TH18 UI 层 `resolutionMode(2)` = 1 texel 1 表面 px，TH12 = 1 texel 1 个 640 单位 = 2 表面 px）；",
        ]
        if rest_scale:
            d4b = [
                "    //   Δ4b 静止 scale **2.0**（★ 本脚本适用）：原脚本通篇没有 `scale()`，静止尺寸吃"
                " **VM 初值 1.0**（一手 `AnmVm::wipe`）⇒ TH18 侧必须显式写成 2.0。",
                "    //        ★ 漏了它的实测后果：三个槽按 24×16 表面 px 画（原版的一半线性尺寸 / 1/4 面积），"
                "与 C 侧认领的块 132×32 差一倍",
                "    //        ⇒ 用户 2026-09-22 实跑「星莲船缺少 hud」（空槽贴图本身又是纯黑轮廓，见文件头 Δ4b）。",
            ]
        else:
            d4b = ["    //   Δ4b 静止 scale：**不适用**（本脚本自己第一句就写 `scale(0,0)`，不吃 VM 初值）。"]
        header = d4b_common + d4b + [
            "    //   Δ6 `resolutionMode(2)`（2026-09-23）：**值以 2× 设计空间（1280×960）为准**。",
            "    //        ★ 2026-09-23 订正：旧注写的「不写 ⇒ 坐标 ×2 ⇒ y=1824 屏外」**是错的** ——",
            "    //        逐指令复核 `th18:0x4063d0`（调用点 `th18:0x481770`）：mode 1/3 → ×DAT、mode 2/4 → ×DAT×0.5、",
            "    //        **mode 0（不写）跳过整段乘法**；1280×960 支把 DAT 写成 2.0（`th18:0x4735ce`）",
            "    //        ⇒ **mode 0 ≡ mode 2 ≡ ×1**。真正让槽出现的是 Δ4b（同一 commit）。",
            "    //        写法保留：它是本仓所有 HUD 脚本的统一口径（`HUD-LAYOUT.md` §4.1b-订正）。",
            f"    //   Δ5 x 位移 {X_SHIFT_640:.0f} 单位（= 原版槽的场地相对位置 20/41/62 折到 TH18 表面；0 = 旧版屏幕黑边）"
            f"{'—— 本脚本适用' if x_shift else '—— **本脚本不适用**（面板是屏幕右列固定位）'}。",
            "    // 逐条依据、实像素证据与中断语义见生成器文件头。",
        ]
        if x_shift:
            header += [
                "    //   Δ7 ★ 2026-09-23（用户实跑「槽位只有 1 个」）：**脚本一个字节都没改**，"
                "摆位改由 C 侧补差值。",
                "    //        一手 TH12 把三个槽位做成中断 7/8/9 → `posTime(8,0,52/73/94,456)` 的目标点"
                "（`th12:0x41d650` 建三只 VM 后紧跟 `mov edx,7/8/9; call [vm+0x494]`）；",
                "    //        在 TH18 里那条路不落地 —— 实跑日志三格的脚本 `pos.x` 恒 296（= 本脚本初始 `pos(296,912)`）"
                "⇒ 三格叠成一个。",
                "    //        绘制点 = `pos(+0x30)` + `entity_pos(+0x5f0)` + `__pos_2(+0x4e4)`（一手 `th18:0x48171f`）"
                "⇒ C 侧每帧写",
                "    //        `entity_pos = 槽心 − (pos + __pos_2)`（`ufo.c` 的 `ufo_hud_place_at()`）"
                "⇒ 本脚本里这组数字仍是唯一真相。",
                "    //        中断 7/8/9 照旧发（配方原样保留）；哪天真生效了，C 的差值自动归零、画面不动。",
            ]
        (SCRIPTS / f"{num}_{name}.anm.txt").write_text(
            "\n".join(header + body + ["}"]) + "\n", encoding="utf-8")
        made.append((num, name, src_num))
    print(f"切图 {len(SPRITES)} 张 → {OUT}")
    print(f"ORDER 片段 → {ORDER_FRAG}（图集 {ATLAS_W}x{ATLAS_H}）")
    for num, name, src_num in made:
        print(f"  script{num:<3} ← front.anm script{src_num}   {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
