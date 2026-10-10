/* faith_hud.c —— ★★ 110（风神录）/ 111（地灵殿）**组合 UI** 的共享实现。
 *
 * 声明 / 调用点 / 布局草图在 `faith_hud.h`；**纯逻辑**（Δ 求和、10 的倍数截断、档→条宽、
 * 缺卡中性值、倍率文本）在 `faith_hud_core.{h,c}`（主机单测 `tests/test_faith_hud_core.c`）。
 * 本文件只管**画**：起 VM、每帧摆位 / 换 sprite / 上色 / 发中断 + 实况诊断。
 *
 * ## 一手出处（每个数都要能追到地址或脚本号）
 *
 *   · `engine/player/th10/02-hud-bottom-left.md` —— TH10 那块的 7 个 VM（`front.anm` script51..57，
 *     layer 15）、蓝点 sprite29 `(256,80,16,16)`、最大得点 6 位（5 动态 + 末位恒 '0'，字距 7）、
 *     以及**那条槽的真身**：`GLOBALS+0x14` 的 zTimer（满 130 帧）被 `th10:0x4158b6`–`0x415937`
 *     直画成两条实心矩形（宽 = `current × 38/130`、A 取最左数字 VM 的 `color_1.A`）。
 *   · `engine/player/th11/03-hud-bottom-left.md` —— TH11 那块的 4 个 VM（script32..35）、
 *     蓝点 sprite26、最大得点 = `AsciiManager` 文本 `"%.6d*%1.2f"`（字距 9、零填充、值截到 10 的倍数）、
 *     「×」= ASCII 星号字形、倍率 = `(min(⌊graze/100⌋,899) + min(⌊交信度/100⌋,100))/100`，
 *     ★ 信号条的**一手填充写法** = `+0x4c = 9+4×档` **且** `+0x78/+0x88 = uv0 + (9+4×档)/512`
 *     （`th11:0x41b6cf`–`0x41b711`）⇒ 是**截断**不是伸缩（本仓等价物 = `ce_anm_set_clip_x()`）。
 *   · `engine/player/th11/04-merged-hud-spec.md` —— **合并布局规格（坐标表 / 宽度预算 / 逐项判定）**。
 *     本文件的几何常量逐条对着它写；凡它标 ❓ 的，这里也标 ❓ 并给出理由。
 *
 * ## 与两块旧 HUD 的关系（退役说明）
 *
 * 110 原来的 `mof_hud_*`（9 支 VM = 图标 + 6 数字位 + 2 条）与 111 原来的 `signal_hud_*`
 * （3 支原版 VM + 3 行 `ce_hud_vm_text_*` 读数）**全部退役**：
 *   · **VM 的持有者搬到本文件**（16 支，见 `FH_N_VM`）；两张卡侧不再持有任何 HUD VM ⇒
 *     「退役的那套 VM 谁负责回收」这个问题的答案是：**没有留下任何需要回收的旧 VM**
 *     （旧代码路径不再被调用 ⇒ 不会起 VM；两卡本来起过的 VM 在同一个 `on_delete` 里由
 *     `faith_hud_departed()` 统一收 —— 见那里的注释）。
 *   · 旧脚本**复用**（211 数字位 / 212 槽底 / 213 增长条 / 214 图标 / 104 画矩形）⇒ 一个都没删，
 *     只是改由本文件驱动。**210（TH10 的图标脚本）也不再被引用**（TH11 的 214 与它同锚点同美术，
 *     取一支即可）—— 它留在 `ability.anm` 里不占运行时成本（脚本是数据，没人起就不跑）。
 *
 * ## 层与绘制顺序（`HUD-LAYOUT.md` §4.1 的硬规则）
 *
 * 全部 **layer 25** ⇒ 折到绘制组 38 ⇒ 被暂停模糊罩（同组、后起）盖住 + 进得了 P 键截图。
 * 同一层内**后起的画在上面** ⇒ 起用顺序 = 槽底 → 增长条 → 图标 → 6 个数字位 → 「×」→ 倍率
 * → 连击条两条（= `04-merged-hud-spec.md` §1 的「顺序是行为，不许重排」）。
 *
 * ## 缺卡中性态（用户 ③）
 *
 * | 在场 | 行 1（信号槽 + 数值）| 行 2（Δ + × + 倍率）| 行 3（连击槽）|
 * | --- | --- | --- | --- |
 * | 110+111 | 111 的真实信号 | `Δ_10+Δ_11` ×（擦弹项 + 信号项）| 110 的真实连击槽 |
 * | **只有 110** | **1.00 / 满格 49 px / 不闪**（中性）| `Δ_10` ×（擦弹项 + **1.00**）| 真实连击槽 |
 * | **只有 111** | 111 的真实信号 | `Δ_11` ×（擦弹项 + 信号项）| **填满（满宽）** |
 * | 两张都不在 | 整块不画（VM 全收）| — | — |
 *
 * ⚠ 判据是 `faith_registered_mask()`（注册表），**不是**「槽值恰好为 0」—— 用户明令（`NEXT.md` §2A ③）。
 */
#include <stddef.h>
#include <string.h>
#include "sdk.h"
#include "faith.h"
#include "faith_hud.h"
#include "mof_core.h"          /* `mof_core_graze_mult()` —— 行 2 倍率的**擦弹项**（TH11 一手同式）*/
#include "hud_dim.h"           /* ★ 2026-09-15h：压暗**改走跨卡统一件**（唯一实现；本文件不再自持一份）*/

/* ============================ 几何常量（640×480 空间；块左下角 = 原点，y 向上）============================
 *
 * 块内坐标一律「相对块左下角、y 向上」；写进 VM 时换成**表面 px**：
 *     表面 x = (块左下角 x640 + 块内 x) × 2
 *     表面 y = (块左下角 y640 − 块内 y) × 2        ← y 反向：块内 y 向上、屏面 y 向下
 * （块左下角的 640 点 = `slot.x * CE_HUD_SURFACE_TO_640`，同 `signal.c` 的算法 ——
 *  `ce_hud_slot_get()` **不填** x640/y640，别直接读那两个字段。）*/

#define FH_U640_TO_SURF      2.0f    /* 640 空间 → 表面 px（= `resolutionMode(2)` 的 DAT×0.5 折半）*/
#define FH_BLOCK_W         194.0f    /* ★★★ 2026-10-03z14（owner：「113 和 110-111 **重合**」）——
                                      *   子智能体审计量过：本块墨迹 = x **63..254**（图标 40×40 画在 63、
                                      *   连击条到 254），而声明块只有 188（68..256）⇒
                                      *   ① 左沿出块 5px、② 右沿 254 与 **113 的 248** 相交 6px（= owner 看到的重合）。
                                      *   声明放宽到 **194**（68..262）后：排布器会把它和 113 分开 ✓；
                                      *   左沿那 5px 仍在块外（要收得把 `FH_ICON_X` 从 −2.5 收到 0，属观感改动，未做）。*/
#define FH_BLOCK_H          60.0f

/* ---- 行 1：信号槽（TH11 抄）----
 * 槽底/条 = **48×16**，`anchor(1,2)` = 左下角 ⇒ VM pos 就是矩形左下角。
 * 位置：块内 (18, 14)（= 规格表 #2 的「左-底 (18,30)」—— 那里的 y 用的是「quad 上沿 30、下沿 14」
 * 的读法；本实现在 y 向上的块坐标里就是左下角 (18,14)，等价且不会自相矛盾）。
 * ⚠ 增长条的**美术缩放 = 2.0**：贴图是 TH11 的 640 原生像素 1:1 切图（64×16），而本仓 UI 层
 *   `resolutionMode(2)` 会把 texel 折半 ⇒ 只有 ×2 才与 TH11 屏幕上**同样的相对大小**
 *   （一手口径见 `sdk.c` 的 `HUD_FONTS[]` 注释：TH13 那套原尺寸美术也是 ×2；`04-merged-hud-spec.md`
 *   §1 的「640 48×16 → 表面 96×32」正是这个意思）。⚠ 旧 `signal.c` 传的是 **1.0**（= 只有一半大），
 *   本轮随合并一起订正 —— 登记 `TUNING.md`。*/
#define FH_GAUGE_X          18.0f
#define FH_GAUGE_UP         14.0f    /* 块内 y：矩形**下沿**（矩形占 y 14..30）*/
#define FH_GAUGE_ART_SCALE   2.0f
#define FH_GAUGE_RECT_PX    64.0f    /* `SIGNAL_GAUGE_BAR` 的**矩形宽**（内容 48 靠左，右边 16 透明）
                                      * —— `ce_anm_set_clip_x` 的 `f` 分母必须是它（一手：`f` 是「采样
                                      * 整张 sprite 矩形的百分之几」；见 `04-merged-hud-spec.md` §4.2）。*/

/* ---- 行 2：6 位最大得点 + 「×」+ 倍率（墨迹上沿 y 14）----
 * · 数字：TH10 的 `front.anm` sprite30..39（本仓 `MOF_FAITH_DIGIT_*`，8×16 画布、字形在最上 9 行）、
 *   脚本 211 的 `anchor(1,1)` = 左上 ⇒ **VM pos = 墨迹上沿**；字距 **7 px**（一手 script52..57 的 pos）。
 *   ⚠ 美术缩放 2.0（同上：TH10 原生像素 1:1 切图）。旧 `mof.c` 传 1.0 ⇒ 只有一半大，本轮订正。
 * · 「×」：TH18 **font 2 的 `'*'` 字形**（`ascii_1280.png` 的 sprite209 = 16×20 单元，
 *   本仓 `SIGNAL_MUL` = 16×32 画布、居中补边，与 `FONT2_*` 同一套约定）⇒ 脚本 275 `anchor(1,1)`，
 *   缩放 **1.0**（表面像素），**与倍率字形同一套定位算式** ⇒ 三个元素（数字 / × / 倍率）共墨迹上沿。
 *   ★★ **2026-09-15f 换过这一格**：第一版借的是 TH11 `ascii.anm` sprite10（14×14，形状是**四角菱形**），
 *   用户实跑原话「乘号是叉子不是 × 号」⇒ 改用 TH18 自己那套字形里 `'*'` 的形状（它就是 ×，
 *   而且与同一行的倍率数字**同一套 font 2**）。一手的三条依据（字符→字形号 / 那一排逐格量像素 /
 *   TH11 的格式串用的就是星号字符）写在 `assets/ability/make_faith_hud_art.py` 的文件头。
 * · 倍率 `M.MM`：TH18 font 2 的字形（脚本 276，`anchor(1,1)`），**步进 7 / `.` 半步进 4**（与 TH10
 *   数字位同值 ⇒ 混排无接缝）；美术缩放 **1.0**（FONT2 的切图来自 TH18 的 `ascii_1280.png`，
 *   **已经是表面像素**，再乘 2 会大一倍 —— 见 `sdk.c` 的 HUD_FONTS 注释）。*/
#define FH_ROW_INK_TOP      14.0f
#define FH_DIGIT_X0         18.0f
#define FH_DIGIT_DX          7.0f
#define FH_DIGIT_SCALE       2.0f
#define FH_DIGITS            6        /* `%.6d` 的位宽（TH11 一手）*/
/* 「×」与倍率都是 **16×32 画布、`anchor(1,1)`、缩放 1.0** 的字形（同一个 font 2 家族）⇒ 共用一条
 * 定位算式：**VM y = 墨迹上沿 + 3 px（640）**，因为字形本体（16×20 单元）在 32 高的画布里
 * **上下各留 6 texel**、而 1 texel（缩放 1.0）在 640 空间是 0.5 px ⇒ 墨迹上沿在画布顶下方 3 px
 * ⇒ 想让墨迹落在 `FH_ROW_INK_TOP`，画布顶就得再高 3 px。
 * ★★ **2026-09-15f 修的方向错**：第一版写成「− 3」，于是 × 与倍率整体**低了 6 px（表面 12 px）**，
 * 用户实跑看到的正是「信号强度（那个 1.00）掉下去了，和最大得点不在同一行」。
 * （数字位不受影响：TH10 的字形贴着画布顶、`VM y = 墨迹上沿` 本来就是对的。）*/
#define FH_GLYPH_TOP_UP      3.0f     /* = 画布内的上留白（6 texel × 0.5 px/texel）*/
#define FH_MUL_STAR_X       61.0f     /* 规格表 §3.1 的「× 从 x61 起」（16×32 画布左沿 = 笔位）*/
#define FH_MUL_STAR_SCALE    1.0f
#define FH_MULT_X           69.0f     /* = 61 + 7（笔位步进）+ 1（规格表的间隔）*/
#define FH_MULT_N            4        /* `M.MM` 恒 4 字符（值 ≤ 9.99）*/
#define FH_MULT_SCALE        1.0f

/* ---- 蓝点图标（TH11 sprite26 / 脚本 214；与 TH10 的 sprite29 是同一张美术）----
 * 框 16×16，`anchor(1,2)` = 左下。定位：让**墨迹**（png bbox (2,1)-(16,16)）中心落在数字行
 * 墨迹的中心（块内 y 9.5）、右沿离信号槽墨迹留 0.5 px 的缝 ⇒ 框左下角 = (−2.5, 0)。
 * ❓ **自定（用户 ② 的「稍大」）**：缩放 **2.5** = 一手相对尺寸（2.0）× **1.25**（16 px → 18 px 框）。
 *   取 1.25 而不是 1.5 的理由：图标槽位只有 18 px 宽（信号槽从 x18 起）⇒ 再大就会压到槽的墨迹上。*/
#define FH_ICON_X          (-2.5f)
#define FH_ICON_UP           0.0f
#define FH_ICON_SCALE        2.5f

/* ---- 行 3：连击槽（TH10 抄的两个实心矩形，脚本 104 画）----
 * 一手：亮条 `(50,460)-(50+v·k−1,462)` 白、影 `(51,461)-(51+v·k,463)` 黑（RGB 0，A 同亮条）、
 * `k = 38/130`、A **借最左数字 VM 的 `color_1.A`**（`th10:0x4158b6` 段，`Gui+0x8393`）。
 * 本块：块内亮条 (18,1)-(93,3)、影 (19,0)-(94,2)（= 规格表 #7 的「连击条 y0..3」footprint，
 * 影相对亮条「右下各 +1」——两块都在块内，右沿 94 与数字行墨迹右界齐平）。
 * ❓ **自定**：满槽宽 **75 px**（原版 38 px）—— 用户 ②「连击槽长一些以托住最大得点×信号强度数值」，
 *   而那一行的整宽正好 75 px（6×7 + 1 + 7 + 25）。填充比例仍是 TH10 的线性（`current × k`）。
 * 载体：原版是引擎直画的 D3D 四边形（`0x43bda0`，全 exe 无对应脚本）⇒ 我们用 `drawRect` VM 近似
 *   （同旧 `mof.c` 的取舍，登记为「载体差异」；**颜色按一手给足**：亮条白、影黑、
 *   A 照抄「借数字位的 alpha」——旧的 `mof.c` 两条都是白的，本轮顺手订正）。
 *
 * ★★★ 2026-09-15f **订正一处被写错的锚点口径**（用户实跑「击破条相对偏左 / 不是左端固定」）：
 *   脚本 **104（`stock_rect`）没写 `anchor`** ⇒ 矩形走**默认锚点 = 锚心（中心）**，
 *   **不是**旧 `mof.c` 注释与 `AUDIT.md` §AL20 的「矩形默认左上角」。两条独立的一手：
 *     · 卡 116（季节槽，用户逐轮微调过的那根条）：`season.c` 的进度条为了「左端固定」
 *       每帧把**中心**左移 `−(1−ratio)×50`，注释写着「条宽 100 表面 px ⇒ 左端固定、宽度按 ratio
 *       收缩（中心随之左移）」；它的常量 `SEASON_HUD_BAR_DX` 也自注「长条**中心** x」。
 *     · 卡 80（股市，同一支脚本 104）：`stock.c` 的矩形字段名就叫 `cx/cy`（中心），
 *       底板 `cx=1056, w=424` 若按左上算会跑到 1480（屏外）——按中心算 x 844..1268 ✓ 正是右上角面板。
 *   ⇒ 本块按**中心**摆位：矩形中心 = 左沿 + 宽/2（下面 `fh_place_combo()` 的算式），于是
 *     ①**左端固定在块内 x18**（= 行 2 数字的墨迹左沿）、②随槽长向右增长、③槽空时不可见。
 *   ⚠ **绘制顺序也是行为**：一手是「先影（黑）后亮（白）」⇒ 亮条画在影之上。VM 同层**后起的画在上面**
 *     ⇒ 起用顺序必须是 **影 → 亮**（第一版反了 ⇒ 用户看到的是一条黑条 + 一圈白边）。
 *
 * ★★★ 2026-09-15f 第二次修：**「影在右下」的两个坐标约定**（用户 2026-09-15f 第六条：「阴影得是右下角」）
 *   一手（TH10 屏面坐标，**y 向下**）：亮条 `(50,460)–(50+k·v−1,462)`、影 `(51,461)–(51+k·v,463)`
 *   ⇒ 影 = 亮条 **+1 x（右）、+1 y（下）** = 右下 ✓（`th10:0x4158da/e2/f0` vs `0x41591b/923/92f`）。
 *   引擎侧的方向**实测钉死**（不是推理；两条都在仓里、都在实跑里调过）：
 *     · 卡 116：`season.c` 把进度条中心写成 `s_slot.y − 8`（常量自注「长条**中心** y = 916 − 924」）⇒
 *       **VM pos 的 y 变小 = 屏面往上** ✓（屏面 y 向下增长，与 SDK 对 `slot.y` 的定义「块的下边缘」一致）。
 *     · 卡 80：底板注释「中心 (528, 50)、尺寸 212×68 ⇒ **顶边**落在 640 空间的 y = 16」⇒ 顶边 = 中心 − 34
 *       ⇒ 同样是「y 小 = 上」✓。
 *   ⇒ **块内坐标（y 向上）↔ 屏面 y 的关系**：`屏面 y = (块左下角 y640 − 块内 y) × 2`（= `fh_sy()`），
 *     所以「块内 y 每 −1」=「屏面 y 每 +2」= **往下 1 px（640 空间）** ⇒ 影的**中心**要比亮条低 1 px。
 *   ⚠ 本条**升级为 ✅**（依据 = 上面两条仓内一手 + TH10 的绝对坐标，三处一致）；
 *     上一版是 🟡 且**真的写错了**：`FH_SHADOW_UP = 0` 是「左上角」时代的「影的下沿」，
 *     换成中心摆位后**它被当成了中心偏移** ⇒ 两条矩形的中心算出来**完全相同**（都是 y 2）
 *     ⇒ 影被亮条整条盖住 ⇒ 用户看到的**根本没有影**（他说「阴影得是右下角」= 现在没有/不在右下）。
 *     现在改成**相对亮条中心的偏移**（下面 `FH_SHADOW_DX/DY`），语义只有一个、不可能再串。*/
#define FH_BAR_X            18.0f     /* 亮条**左沿**（块内）*/
#define FH_BAR_UP            1.0f     /* 块内 y：亮条**下沿**（亮条占 y 1..3 ⇒ **中心** y 2）*/
#define FH_BAR_H              2.0f
/* 影 ≡ 亮条**中心** + (DX, DY)**（块内坐标，y 向上 ⇒ 屏面「下」= DY 取负）**：
 * `+1 右 / −1（块内）= 屏面下 1 px` ⇒ **右下** ✓ 一手。⚠ 符号别只看名字：这两条是**中心**偏移。*/
#define FH_SHADOW_DX         1.0f     /* 右 +1（屏面）*/
#define FH_SHADOW_DY        (-1.0f)   /* 块内 y −1 = 屏面 y +2 表面 px = **下** 1 px（640 空间）*/

/* ---- 整块淡出 / 淡回：★❓ **用户口径（2026-09-15f 起，UI 统一）——不是复刻** ----
 *
 * ## 本质（用户点醒的口径）
 *
 * **压暗 = 「别让 HUD 挡住自机」**：自机**靠近那块的覆盖范围**就淡下去，**离开**（滞回带外）就淡回来。
 * ⇒ 各作的两个沿**不是「不统一」，而是各自那块 HUD 的矩形不同**：TH11 的 `x < −64 / y > 416` 就是
 * **它自己那块**的沿（再往外放一个边距），TH10 的 `x < −128 / y > 416` 是**它自己那块**（更宽更靠左）的沿。
 * ⚠ 所以本块的两个沿**必须从我们自己的块矩形推**，**不许**照抄某一作的数（第一版照抄了
 * `y > 400 / y < 384`，那既不是我们块的沿、也不是任何一作的沿）。
 *
 * ## ★★ 2026-09-15h：本块的压暗**并进跨卡统一件 `cards/hud_dim.{h,c}`**（不再各有一份）
 *
 * 上一轮（2026-09-15g）本文件自己写了一份 `fh_dim_limit()` + `FH_DIM_HYST/ALPHA/STEP`，与共享件
 * 「逐条一致、只有 band 差 16」⇒ 父 agent 2026-09-15h 明令**合并成一份实现**。现在：
 *   · **本文件只提供两样东西**：① 本块的矩形（表面 px，`FH_BLOCK_W/H` + 本帧槽位左下角
 *     `sh.x/sh.y`）交给 `hud_dim_rect_surface()`；② 把 `hud_dim_tick()` 返回的 alpha 写进各元素
 *     的**两路**顶点色（`fh_set_alpha()`，见下）。判据 / 边距 / 滞回 / 斜坡全在共享件里。
 *   · 原先那三个本地常量（`FH_DIM_HYST 16` / `FH_DIM_ALPHA 32` / `FH_DIM_STEP 51`）**已删** ——
 *     它们的值现在分别是 `HUD_DIM_BAND` / `HUD_DIM_ALPHA_DIM` / `HUD_DIM_FRAMES`，**别再在本文件里重定义**。
 *
 * ## ★ 换算订正：`224` → `320`（本文件原先那一份的门限因此整体偏了 96 自机 px）
 *
 * 上一轮本文件把「世界 x → 640 空间」写成 `x640 = 224 + 世界x`（引自 `cards/stock.c` 的常量注
 * 与 `AUDIT.md` 里相机 0 视口 `(32,16,384,448)` 那条**旧**记法）。**父 agent 2026-09-15h 裁定 320 成立**，
 * 一手依据：`engine/anm/th18/01-vm-instantiate.md` §3b「世界层 `表面 = (640 + x×2, 32 + y×2)`，
 * **游玩区左边缘 257** ⇐ `640 − 192×2`、底边 928 ⇐ `32 + 448×2`」⇒ 游玩区左沿在 640 空间 = **128**
 * ⇒ 世界 x −192 ↔ 640 空间的 128 ⇒ **`x640 = 世界x + 320`**（y 侧两说一致：`y640 = 世界y + 16`）。
 * `224`（= 旧视口 32..416 的中线）会把游玩区放到 640 空间的 32..416 = 表面 64..832，与「257 是左沿」矛盾。
 * ⇒ 本块的 x 门**从 `−95.5..−1.5` 订正为 `−191.5..−97.5`**（y 门不变：矩形 416..446）。
 *   ⚠ `cards/stock.c` 里还有一处同样的 `224`（`PLAYER_TEXT_CX`，**别人的卡**）——
 *     本会话**只读判断、没动**，已登记 `NEXT.md` §2D 第 5 条 / `AUDIT.md` §DD-7。
 *
 * ## 判据（共享件 `hud_dim.h` 的口径）：**进 = 块矩形 + 28 / 出 = 块矩形 + 44**（滞回 16）
 *
 * ⚠ 2026-09-15g 那版「**进 = 自机落在矩形之内**」**已作废**（父 agent 2026-09-15h 裁错订正）：
 *   本块的**下沿在 446**，而自机 y 被钳在 **432** ⇒ 「在矩形之内」只剩 **16 px 高**的一条
 *   （y ∈ [416,432]），左下角那几块更是只剩 4–10 px —— 一手的语义本来就是**「块矩形 + 一个边距」**
 *   （一手各作的门都落在它们自己那块上沿之上 24–32 px：TH11 的 416 = 它那块沿 + 24，TH10 约 32，
 *   TH16 的 400 在它那块沿之上 28）⇒ 统一取 **28**（边距）+ **16**（滞回）= **出的一侧 44**。
 *
 * 本块（默认货架锚点 = 表面 (257, 924)、块 188×60 ⇒ 自机坐标 x ∈ [−191.5, −97.5] / y ∈ [416, 446]）：
 *   · **进** = 自机落在「矩形 + 28」内：`x ∈ [−219.5, −69.5] ∧ y ∈ [388, 474]`
 *     （自机钳位 x ∈ [−184, 184]、y ∈ [32, 432] ⇒ 实际可触发区 x ∈ [−184, −69.5] / y ∈ [388, 432]）
 *   · **出** = 离开「矩形 + 44」：`x < −235.5 ∨ x > −53.5 ∨ y < 372 ∨ y > 490`
 *   ⚠ 矩形每帧从当前槽位算（块会随货架移动），所以这四个数**随锚点变**，不是常量。
 *
 * ## 统一的那几个数（用户批准；与 116/117/107/113/115 同口径）
 *
 * | 项 | 值 |
 * | --- | --- |
 * | 进压暗 | 自机在**本块矩形外扩 28 px**（`HUD_DIM_MARGIN`）之内 |
 * | 淡回 | 自机在**本块矩形外扩 44 px**（= 28 + `HUD_DIM_BAND` 16）之外（滞回带 16 px）|
 * | 终点 alpha | **64**（25%；★ 2026-09-22 起 = TH18 原生一手，见 `engine/card/th18/15-selected-card-hud.md` §4.2）|
 * | 起始 alpha | 255 |
 * | 过渡 | **C 侧 8 帧线性**（`HUD_DIM_FRAMES`；从当前值线性走到目标，第 8 帧精确到位）|
 *
 * ## 原一手值（留档，谁也不删；脚本里那几条 `alphaTime` 就是这个记录）
 *
 *   · TH11：触发 `x < −64 ∧ y > 416`、淡回 `y < 400 ∨ x > −64`、压暗 `alphaTime(10, 0, 64)`
 *     （`th11:0x41b4a1`–`0x41b55a` + `front.anm` script32/33/35）；
 *   · TH10：触发 `x < −128 ∧ y > 416`、淡回 `y < 400 ∨ x > −112`、压暗 `alphaTime(8, 1, 32)`
 *     （`th10:0x41499f`–`0x414a5a` + `front.anm` script51..57）。
 *   ⇒ **性质**：边距 / 滞回 / C 驱动 = ❓ 用户口径；**终点 64 与 8 帧 = 复刻**（TH18 原生那块「当前卡」UI 的一手值）
 *     （TH10 一手是 `alphaTime(8, 1, 32)`、TH11 是 `alphaTime(10, 0, 64)`，都已退役）⇒ 别写成「复刻 TH11 / TH10」。
 *
 * ## 实现方式 = C 驱动（不再发脚本的中断 2/3）
 *
 *   · 脚本里那几条 `alphaTime(...)` **保留但不再被触发**（它们同时是上面那两个一手值的记录）；
 *   · 淡入仍然归脚本（`alpha(0)` + `+20: alphaTime(20,0,255)`）⇒ C 在**前 `FH_WARMUP_FRAMES` 帧
 *     一帧都不写 alpha**，之后每帧把 `s_hud_dim.alpha` 写进各元素的 `color_1`/`color_2`（两路同值）。
 *     ⚠ 这样不会有「脚本插值 vs C 硬写」互相打架：20 帧淡入结束时脚本的目标值正好是 255，
 *     之后脚本不再动那一格 ⇒ C 是唯一作者。**别改成「乘一个系数再写回」**（每帧复合会衰减到 0）。*/

/* 档位闪色（一手 `th11:0x41b593`–`0x41b66e`）：档 0 → 中断 7（常态白），档 k → 中断 7+k。
 * `FH_WARMUP_FRAMES` = 一手的前 20 帧门（`th11:0x41b593 cmp [edi+0x43cc],0x14`）：块刚起用的
 * 20 帧内不发，免得把脚本自己的 20 帧淡入闪断。★ 它同时是「C 什么时候可以开始写 alpha」的门
 * （见上面的压暗块）——两个用途共用一个常量，语义都是「脚本的 20 帧淡入还没跑完」。*/
#define FH_INT_IDLE          7
#define FH_WARMUP_FRAMES    20

/* 自机擦弹计数（`GlobalsInner+0x30`，与 `mof.c` 的 `MOF_GRAZE_ADDR` / `signal.c` 同址，**只读**）。*/
#define FH_GRAZE_ADDR        0x4ccd0c

/* 本块服务的两张卡（**块认领**用的 id：取在场的、id 更小的那张 ⇒ 确定性。
 * 「谁的 id 小谁认领」与货架（按 card_id 升序排）是同一套口径；两张都不在时不认领）。*/
#define FH_ID_110            110u
#define FH_ID_111            111u

/* ============================ 状态 ============================ */
static faith_hud_slot_t s_decl[FAITH_SLOT_COUNT];   /* 每槽的**声明读数**（卡每帧写；本模块只读）*/
static uint32_t s_owner;                            /* 认领块的卡 id（0 = 还没认领）*/
static int32_t  s_stage = -1;                       /* 上一次 on_load 看到的关卡纪元 */
static int32_t  s_claim_cool;                       /* 认领失败后的冷却帧数（别每帧重试 + 刷日志）*/

static uint32_t s_icon;
static uint32_t s_gauge_bg, s_gauge_bar;
static uint32_t s_digit[FH_DIGITS];
static int32_t  s_digit_spr[FH_DIGITS];
static uint32_t s_star;

/* 倍率的 4 个字形 VM：脚本 276 是**模板**（绑 `FONT2_0`），sprite 逐字由这里换。*/
static uint32_t s_mult_vm[FH_MULT_N];
static uint32_t s_mult_spr[FH_MULT_N];

static uint32_t s_bar[2];                            /* 0 = 亮条 / 1 = 影 */
/* ★ 2026-09-15h：压暗状态 = 共享件 `hud_dim_t`（闩锁 + alpha）—— 原来本文件自己有两格
 * `s_dim` / `s_dim_a`（已删）。**静态初值写成满亮 255 + 稳态帧数**（= `hud_dim_init()` 的四个值；
 * ★ 2026-09-21：共享件多了 `from` / `t` 两格，静态初值必须写全，否则 `t = 0` 会让第一帧凭空起斜坡）：
 * 本文件在第一次 `hud_dim_tick()` 之前就会读它（`fh_place_combo()` 从数字位「借 alpha」）。*/
static hud_dim_t s_hud_dim = { 0, HUD_DIM_ALPHA_FULL, HUD_DIM_ALPHA_FULL, HUD_DIM_FRAMES };
static int32_t  s_flash;                             /* 上一次发的档位闪色档（-1 = 还没发过）*/
static int32_t  s_age;                               /* 块起用后的帧数（喂 20 帧门）*/
static int32_t  s_warned;                            /* 起 VM 失败只报一次*/
static int32_t  s_up_logged, s_diag_frame;
static int32_t  s_pres_last = -1;                    /* 上一次的在场掩码（变一次打一行日志）*/

/* ============================ 小工具 ============================ */
static void fh_warn_once(const char *what)
{
    if (s_warned) return;
    s_warned = 1;
    ce_log("faith_hud: VM 起不来（%s）—— 这一块会缺件（VM 池满？见 sdk.h 的 ce_anm_spawn_ui）", what);
}

/* VM 还活着吗（引擎会在别处摧毁 VM ⇒ 每帧自愈检查，旧 `mof.c` / `signal.c` 同款）。*/
static int fh_alive(uint32_t id) { return id != 0 && ce_anm_get_vm(id) != 0; }

/* 起一个 HUD VM：`pos` 传表面 px（脚本里 `resolutionMode(2)`；`pos` 字段本身一律 0 ⇒ 位置只由
 * `entity_pos` 给 —— 两个坐标字段是**相加**的，一手 `AnmManager__draw_vm` `th18:0x48171f`）。*/
static uint32_t fh_spawn(int script)
{
    float pos[3] = { 0.0f, 0.0f, 0.0f };

    return ce_anm_spawn_ui(CE_ABILITY_ANM(), script, pos);
}

/* 块内 (x, y[向上]) → 表面 px（照本文件头的算式）。
 * ⚠ **必须 `static inline`**：i686 Windows ABI 的 **float 返回值走 x87 `st(0)`**，而本仓不变式是
 *   「我们的 object 里 0 条 x87 指令」（`make dllx87`；先例与完整理由见 `faith.h` 的
 *   `faith_player_poc_height()` 注释）⇒ 这两支就地展开，不留 ABI 边界。*/
static inline float fh_sx(float bx640, float x)  { return (bx640 + x) * FH_U640_TO_SURF; }
static inline float fh_sy(float by640, float up) { return (by640 - up) * FH_U640_TO_SURF; }

/* ★★ 2026-09-18：本块的**自带锚点**（表面 px；见 `fh_claim()` 里的推导）。
 * 一手位置 = 640 空间 x **32** = **游玩区左边缘** = TH10 原生 HUD 的 `pos(32,448)` 那一列
 * （原来走货架左列，拿到的是 `CE_HUD_ANCHOR_X = 257` ⇒ 比这个位置**右 193 表面 px**）。
 *
 * ★★★ 2026-10-03m（owner 定案）：「**110-111 也改成 68 吧**」——
 *   `64`（TH10 一手：640 空间 32 ⇒ 表面 64）→ **`68`**（640 空间 **34** ⇒ 表面 68）：
 *   · 与**左下角带里其它卡**对齐（107 / 116 / 112 的真实 pin 都是 **68**；112 是
 *     `248 + UFO_HUD_OWNER_DX_SURF(−180)`，见 `HUD-LAYOUT.md` §1.4 的订正块）；
 *   · 也正是原生那块「**当前主动卡**」UI 的**左沿**（68）= 整条带的公共左边界；
 *   · ⚠ 这是**对一手的 +2 单位偏离**（表面 +4 px）：owner 口径优先于逐值复刻 ⇒ 登记 `TUNING.md`；
 *   · ✅ 由此块右沿 = `68 + 188 = 256`，**正好止步于货架锚点 257**（不越进原生卡图标列）。
 *   （owner 同时说「**117 可能最终也会回到 68**」—— 那条**本次未动**，117 现在仍是 88。）*/
#define FH_PIN_X   68.0f
/* ★★★ 2026-10-03z52（owner：「**110-111 的矩形可以往左延伸 5px**」）——
 *   子智能体审计量过：本块**墨迹左沿 = 63**（图标 40×40 画在 63），而声明块从 68 起
 *   ⇒ 左沿出块 5px ✗。owner 的处置 = **把矩形往左延 5px**（不改画面）✓
 *   ⇒ 声明锚点 `FH_PIN_X − 5`、块宽 `FH_BLOCK_W + 5`；**画法靠 `FH_BLOCK_LEFT_PAD` 补回来** ✓。*/
#define FH_BLOCK_LEFT_PAD   5.0f
#define FH_PIN_Y  924.0f

/* ============================ 认领块（每关重来一次）============================ */
static void fh_claim(void)
{
    const int mask = faith_registered_mask();
    ce_hud_block_t blk;
    ce_hud_slot_t got;
    uint32_t want;

    if (s_owner && ce_hud_slot_get(s_owner, &got)) return;   /* 已有可用占位（含被删卡的保留占位）*/
    if (s_claim_cool > 0) { --s_claim_cool; return; }

    want = (mask & (1 << FAITH_SLOT_110)) ? FH_ID_110
         : (mask & (1 << FAITH_SLOT_111)) ? FH_ID_111 : 0u;
    if (!want) { s_owner = 0; return; }                      /* 两张都不在 ⇒ 没有块 */
    if (s_owner == want) s_owner = 0;                        /* 占位没了（纪元清了表）⇒ 重新认领 */

    memset(&blk, 0, sizeof blk);
    blk.w = FH_BLOCK_W + FH_BLOCK_LEFT_PAD;   /* ★ z52：把"左沿出块 5px"的那 5px 声明进来 */
    blk.h = FH_BLOCK_H;
    /* ★★ 2026-09-18（用户实跑报）：**位置改成「自带锚点」**，钉在**原版 HUD 的左下角** ——
     *   原话：「110 111 复合 UI 的实际位置是**左下角向右了 1/3 个游玩区宽度**」。
     *   实测（日志 `faith_hud: 块@(257,924)`）：货架左列锚点 `CE_HUD_ANCHOR_X = 257`（表面 px）
     *   = 640 空间 x 128.5 —— 那是**引擎自己的卡图标列**位置，不是本 UI 该在的地方；
     *   而本块的内部布局是照 TH10 `front.anm` 的原生 HUD 摆的（蓝点在块内 x −2.5、数字 x18 起、
     *   槽 x18..82）⇒ 块原点就应当落在 **TH10 原生 HUD 的原点**：
     *   `MOF_HUD_*` 的图标 `pos(32, 448)`（640）⇒ 表面 **(64, y)**。
     *   ⚠ 只改 x（用户只报了横向）；y 沿用货架行底 **924**（表面）= 640 462（离游玩区底边 464 两像素），
     *     要更贴原版（640 446 ⇒ 表面 892）就改 `FH_PIN_Y` 一个数。
     *   ⚠ `blk.side` 必须是 `CE_HUD_SIDE_PINNED`（`ce_hud_slot_claim_pinned` 会拒掉别的）。
     *   先例 = 117（`cards/animal.c` 的 `ANIMAL_HUD_PIN_*`，用户按截图量过的固定位置）。*/
    blk.side = CE_HUD_SIDE_PINNED;
    /* ⚠ 入口必须是 `_card` 版（认领前把关卡号推进纪元 ⇒ 上一关的占位在本关开场收敛）。*/
    if (!ce_hud_slot_claim_pinned_card(want, &blk, FH_PIN_X - FH_BLOCK_LEFT_PAD, FH_PIN_Y, &got)) {
        s_claim_cool = 60;
        ce_log("faith_hud: 块 %.0fx%.0f 认领失败（卡 id %u；槽位表拒绝 —— 见 sdk_core.h 的 hud_register 门槛）",
               (double)blk.w, (double)blk.h, (unsigned)want);
        return;
    }
    s_owner = want;
    ce_log("faith_hud: 组合 UI 块 %.0fx%.0f（640 空间 %.0fx%.0f）认领于表面 (%.0f, %.0f) —— 槽位左下角；认领卡 id %u",
           (double)blk.w, (double)blk.h, (double)(blk.w * CE_HUD_SURFACE_TO_640),
           (double)(blk.h * CE_HUD_SURFACE_TO_640), (double)got.x, (double)got.y, (unsigned)want);
}

void faith_hud_on_load(void)
{
    const int32_t stage = (int32_t)CE_CURRENT_STAGE();

    if (stage != s_stage) {          /* 新关卡：货架会在本关第一次认领时清表 ⇒ 强制重认领 */
        s_stage = stage;
        s_owner = 0;
        s_age = 0;                   /* 淡入重来（脚本的 20 帧 alphaTime）*/
    }
    fh_claim();
}

void faith_hud_declare(int slot, const faith_hud_slot_t *v)
{
    if (slot < 0 || slot >= FAITH_SLOT_COUNT) return;
    if (v) s_decl[slot] = *v;
    else   memset(&s_decl[slot], 0, sizeof s_decl[slot]);
}

/* ============================ 收 VM ============================ */
static void fh_kill(void)
{
    int i;

    if (s_icon)      { ce_anm_delete(s_icon);      s_icon = 0; }
    if (s_gauge_bg)  { ce_anm_delete(s_gauge_bg);  s_gauge_bg = 0; }
    if (s_gauge_bar) { ce_anm_delete(s_gauge_bar); s_gauge_bar = 0; }
    if (s_star)      { ce_anm_delete(s_star);      s_star = 0; }
    for (i = 0; i < FH_DIGITS; ++i) { ce_anm_delete(s_digit[i]); s_digit[i] = 0; s_digit_spr[i] = -1; }
    for (i = 0; i < FH_MULT_N; ++i) { ce_anm_delete(s_mult_vm[i]); s_mult_vm[i] = 0; s_mult_spr[i] = 0; }
    for (i = 0; i < 2; ++i) { ce_anm_delete(s_bar[i]); s_bar[i] = 0; }
    hud_dim_init(&s_hud_dim);            /* 压暗闩锁 + alpha 复位（= **满亮 255**）：块被收掉之后
                                          * 下一次起用时不该从半暗开始（`s_age` 也归 0 ⇒ 前 20 帧本来
                                          * 也强制 255，这里只是把状态本身也写正）*/
    s_flash = -1;
    s_age = 0;
    s_warned = 0;
    s_up_logged = 0;
    s_diag_frame = 0;
}

void faith_hud_departed(void)
{
    const int mask = faith_registered_mask();
    const int ours = (1 << FAITH_SLOT_110) | (1 << FAITH_SLOT_111);

    if (mask & ours) {
        /* 还有一张在场 ⇒ 块与占位都不动（用户 ③：只有一张也照常显示），
         * 只把**退场那一槽**的声明清掉（免得它的残值继续画在行 2 上）。*/
        if (!(mask & (1 << FAITH_SLOT_110))) memset(&s_decl[FAITH_SLOT_110], 0, sizeof s_decl[0]);
        if (!(mask & (1 << FAITH_SLOT_111))) memset(&s_decl[FAITH_SLOT_111], 0, sizeof s_decl[0]);
        return;
    }
    /* 两张都走了（或从没来过）：**这一支就是「删卡路径」的收尾** —— 删卡发生在商店里，
     * 那一刻 `on_draw` 不会再来，只有 `on_delete`（虚表 `+0x50`）跑得到 ⇒ 16 支 VM 必须在这里收干净。*/
    fh_kill();
    if (s_owner) { ce_hud_slot_release(s_owner); }
    s_owner = 0;
    memset(s_decl, 0, sizeof s_decl);
    s_stage = -1;                      /* 下次 on_load 当作「新关卡」重认领 */
}

/* ============================ 每帧：起 VM + 摆位 + 上色 ============================ */

/* 槽底 / 增长条（TH11 脚本 212 / 213，`anchor(1,2)` = 左下角）。*/
static void fh_place_gauge(float bx, float by, int32_t signal)
{
    const float x = fh_sx(bx, FH_GAUGE_X), y = fh_sy(by, FH_GAUGE_UP);

    if (!fh_alive(s_gauge_bg)) {
        s_gauge_bg = fh_spawn(CE_ANM_ABILITY_SCRIPT_SIGNAL_GAUGE_BG);
        if (!s_gauge_bg) fh_warn_once("gauge bg");
    }
    if (s_gauge_bg) {
        ce_anm_set_pos(s_gauge_bg, x, y, 0.0f);
        ce_anm_set_scale(s_gauge_bg, FH_GAUGE_ART_SCALE, FH_GAUGE_ART_SCALE);
        (void)ce_anm_mirror_color(s_gauge_bg);      /* 顶点色两路都写（见 sdk.h 的长注）*/
    }

    if (!fh_alive(s_gauge_bar)) {
        s_gauge_bar = fh_spawn(CE_ANM_ABILITY_SCRIPT_SIGNAL_GAUGE_RETICLE);
        if (!s_gauge_bar) fh_warn_once("gauge bar");
    }
    if (s_gauge_bar) {
        /* ★ **截断**填充（一手 `th11:0x41b6cf`–`0x41b711`：宽与 UV 一起写）——
         * `f = (9 + 4×档) / sprite 矩形宽(64)`，美术缩放 2.0（TH11 原生像素 1:1 切图）。*/
        ce_anm_set_pos(s_gauge_bar, x, y, 0.0f);
        (void)ce_anm_set_clip_x(s_gauge_bar,
                                (float)faith_hud_signal_width_px(signal) / FH_GAUGE_RECT_PX,
                                FH_GAUGE_ART_SCALE, FH_GAUGE_ART_SCALE);
        (void)ce_anm_mirror_color(s_gauge_bar);
    }
}

/* 蓝点（TH11 脚本 214 / sprite26）。*/
static void fh_place_icon(float bx, float by)
{
    if (!fh_alive(s_icon)) {
        s_icon = fh_spawn(CE_ANM_ABILITY_SCRIPT_SIGNAL_GAUGE_ICON);
        if (!s_icon) fh_warn_once("icon");
    }
    if (!s_icon) return;
    ce_anm_set_pos(s_icon, fh_sx(bx, FH_ICON_X), fh_sy(by, FH_ICON_UP), 0.0f);
    ce_anm_set_scale(s_icon, FH_ICON_SCALE, FH_ICON_SCALE);
    (void)ce_anm_mirror_color(s_icon);
}

/* 数字 → sprite 号。**按名字逐个列**（不写 `…_DIGIT_0 + d` 的算术）：`MOF_FAITH_DIGIT_0..9` 现在
 * 确实是连续的（`anm_ids.h` 345..354），但「连续」是 ORDER.txt 行序的副产品 ⇒ 显式表把这条隐含
 * 假设去掉（旧 `mof.c` 的 `s_hud_digit_tbl` 就是这个理由，本表是它的搬家）。*/
static const int32_t s_digit_tbl[10] = {
    CE_ANM_ABILITY_SPRITE_MOF_FAITH_DIGIT_0, CE_ANM_ABILITY_SPRITE_MOF_FAITH_DIGIT_1,
    CE_ANM_ABILITY_SPRITE_MOF_FAITH_DIGIT_2, CE_ANM_ABILITY_SPRITE_MOF_FAITH_DIGIT_3,
    CE_ANM_ABILITY_SPRITE_MOF_FAITH_DIGIT_4, CE_ANM_ABILITY_SPRITE_MOF_FAITH_DIGIT_5,
    CE_ANM_ABILITY_SPRITE_MOF_FAITH_DIGIT_6, CE_ANM_ABILITY_SPRITE_MOF_FAITH_DIGIT_7,
    CE_ANM_ABILITY_SPRITE_MOF_FAITH_DIGIT_8, CE_ANM_ABILITY_SPRITE_MOF_FAITH_DIGIT_9,
};

/* 6 位最大得点（TH10 脚本 211 / sprite30..39，`anchor(1,1)`，字距 7）。*/
static void fh_place_digits(float bx, float by, int32_t maxpoint)
{
    int d[FH_DIGITS], i;

    faith_hud_digits6(maxpoint, d);
    for (i = 0; i < FH_DIGITS; ++i) {
        int32_t spr = (int32_t)s_digit_tbl[d[i]];

        if (!fh_alive(s_digit[i])) {
            s_digit[i] = fh_spawn(CE_ANM_ABILITY_SCRIPT_MOF_FAITH_DIGIT);
            s_digit_spr[i] = -1;
            if (!s_digit[i]) { fh_warn_once("digit"); continue; }
        }
        ce_anm_set_pos(s_digit[i], fh_sx(bx, FH_DIGIT_X0 + FH_DIGIT_DX * (float)i),
                       fh_sy(by, FH_ROW_INK_TOP), 0.0f);
        ce_anm_set_scale(s_digit[i], FH_DIGIT_SCALE, FH_DIGIT_SCALE);
        if (s_digit_spr[i] != spr && ce_anm_set_sprite(CE_ABILITY_ANM(), s_digit[i], (int)spr))
            s_digit_spr[i] = spr;
        (void)ce_anm_mirror_color(s_digit[i]);
    }
}

/* 「×」（TH18 font 2 的 `'*'` 字形 = × 形；脚本 275 `anchor(1,1)`）。
 * 与倍率**共用一条定位算式**（`FH_ROW_INK_TOP + FH_GLYPH_TOP_UP`）⇒ 与 6 位数字的墨迹上沿对齐。*/
static void fh_place_star(float bx, float by)
{
    if (!fh_alive(s_star)) {
        s_star = fh_spawn(CE_ANM_ABILITY_SCRIPT_FAITH_HUD_MUL);
        if (!s_star) { fh_warn_once("mul"); return; }
    }
    ce_anm_set_pos(s_star, fh_sx(bx, FH_MUL_STAR_X), fh_sy(by, FH_ROW_INK_TOP + FH_GLYPH_TOP_UP), 0.0f);
    ce_anm_set_scale(s_star, FH_MUL_STAR_SCALE, FH_MUL_STAR_SCALE);
    (void)ce_anm_mirror_color(s_star);
}

/* 倍率 `M.MM`（TH18 font 2 字形；脚本 276 的模板 VM + 逐字换 sprite）。
 * 步进 7、`.` 半步进 4（= `sdk.c` 的 `HUD_FONTS[0]`；与 TH10 数字位的 7 px 同值 ⇒ 混排无接缝）。
 * ⚠ VM y **必须比墨迹上沿高**（`+ FH_GLYPH_TOP_UP`，2026-09-15f 修的方向错，见常量块的注释）。*/
static void fh_place_mult(float bx, float by, int32_t mult_permille)
{
    char txt[FH_MULT_N + 1];
    float pen = FH_MULT_X;
    int i;

    faith_hud_mult_text(mult_permille, txt);
    for (i = 0; i < FH_MULT_N; ++i) {
        uint32_t spr = (txt[i] == '.') ? (uint32_t)CE_ANM_ABILITY_SPRITE_FONT2_DOT
                                       : (uint32_t)(CE_ANM_ABILITY_SPRITE_FONT2_0 + (uint32_t)(txt[i] - '0'));

        if (!fh_alive(s_mult_vm[i])) {
            s_mult_vm[i] = fh_spawn(CE_ANM_ABILITY_SCRIPT_FAITH_HUD_MULT);
            s_mult_spr[i] = 0;
            if (!s_mult_vm[i]) { fh_warn_once("mult glyph"); continue; }
        }
        ce_anm_set_pos(s_mult_vm[i], fh_sx(bx, pen), fh_sy(by, FH_ROW_INK_TOP + FH_GLYPH_TOP_UP), 0.0f);
        ce_anm_set_scale(s_mult_vm[i], FH_MULT_SCALE, FH_MULT_SCALE);
        if (s_mult_spr[i] != spr && ce_anm_set_sprite(CE_ABILITY_ANM(), s_mult_vm[i], (int)spr))
            s_mult_spr[i] = spr;
        (void)ce_anm_mirror_color(s_mult_vm[i]);
        pen += (txt[i] == '.') ? 4.0f : 7.0f;
    }
}

/* 连击槽两条矩形（TH10 一手几何；脚本 104 的 `drawRect`）。
 * ★★ **`pos` = 矩形的【中心】**（脚本 104 没写 `anchor` ⇒ 默认锚心；两条独立一手见常量块）——
 *   所以这里把「左沿 + 宽/2」当中心写 ⇒ 左端钉死在块内 `FH_BAR_X`、随槽长向右增长（用户 ③④）。
 * ★ **绘制顺序 = 影先、亮后**（一手就是先画影再画亮）⇒ VM 同层后起的在上面 ⇒ 亮条盖在影上。
 * ⚠ `alpha` **借最左数字 VM 的 `color_1.A`**（一手 `Gui+0x8393`）⇒ 与数字同一条淡入/压暗曲线。*/
static void fh_place_combo(float bx, float by, int32_t combo)
{
    const float w640 = (float)faith_hud_combo_width_milli(combo) * 0.001f;
    const uint8_t *dvm = fh_alive(s_digit[0]) ? ce_anm_get_vm(s_digit[0]) : 0;
    const uint32_t alpha = dvm ? (*(const uint32_t *)(dvm + CE_ANM_VM_COLOR1) & 0xff000000u)
                               : 0xff000000u;
    /* 亮条（块内）：左沿 `FH_BAR_X`、下沿 `FH_BAR_UP`、高 `FH_BAR_H` ⇒ **中心**在
     * `(FH_BAR_X + w/2, FH_BAR_UP + FH_BAR_H/2)`；影 = 亮条中心 + `(FH_SHADOW_DX, FH_SHADOW_DY)`。
     * ⚠ 两个中心**分别算**（上一版把「影的下沿」当中心加进去 ⇒ 两者中心相同 ⇒ 影被盖住，
     *   用户报「阴影得是右下角」）；别再把两个偏移混在一个算式里。*/
    const float bright_cx = FH_BAR_X + w640 * 0.5f;
    const float bright_cy = FH_BAR_UP + FH_BAR_H * 0.5f;
    int k;

    /* k = 0 先摆**影**（黑）、k = 1 再摆**亮条**（白）—— **起用顺序 = 绘制顺序**（后起的在上面），
     * 一手就是「先影后亮」⇒ 亮条盖住影的左/上各 1 px，影只在**右下**露出来 ✓ 不许调换。*/
    for (k = 0; k < 2; ++k) {
        const uint32_t id_slot = (uint32_t)k;                  /* 0 = 影 / 1 = 亮 */
        const float cx = bright_cx + (k == 0 ? FH_SHADOW_DX : 0.0f);
        const float cy = bright_cy + (k == 0 ? FH_SHADOW_DY : 0.0f);
        const uint32_t col = (k == 0 ? 0x00000000u : 0x00ffffffu) | alpha;

        if (!fh_alive(s_bar[id_slot])) {
            s_bar[id_slot] = fh_spawn(CE_ANM_ABILITY_SCRIPT_STOCK_RECT);
            if (!s_bar[id_slot]) { fh_warn_once("combo bar"); continue; }
        }
        ce_anm_set_pos(s_bar[id_slot], fh_sx(bx, cx), fh_sy(by, cy), 0.0f);
        /* 一手：`current == 0 ⇒ 整条不画`（`th10:0x4158be`）—— 我们的等价物 = 宽度写 0（不可见）。*/
        ce_anm_set_scale(s_bar[id_slot], w640 * FH_U640_TO_SURF, FH_BAR_H * FH_U640_TO_SURF);
        ce_anm_set_color(s_bar[id_slot], col);
        (void)ce_anm_mirror_color(s_bar[id_slot]);
    }
}

/* 整块压暗 / 淡回（TH11 的门限，边沿触发 —— 每帧重发会把脚本的 alphaTime 反复重头开始）。*/
/* 把「当前压暗淡入值」`s_hud_dim.alpha` 写进一支 VM 的**两路**顶点色：**只改 A**（RGB 原样保留 ——
 * 脚本的基色 `(32,32,64)`、档位闪色、连击条的白都在 RGB 里，动它就是覆盖既有配色）。
 * 两路同值 ⇒ 与脚本 `colorMode` 选哪一路无关（口径同 `sdk.h` 的 `ce_anm_mirror_color()`）。*/
static void fh_set_alpha(uint32_t id)
{
    uint8_t *vm;
    uint32_t c1;

    if (!id) return;
    vm = ce_anm_get_vm(id);
    if (!vm) return;
    c1 = (*(const uint32_t *)(vm + CE_ANM_VM_COLOR1) & 0x00ffffffu) | ((uint32_t)s_hud_dim.alpha << 24);
    *(uint32_t *)(vm + CE_ANM_VM_COLOR1) = c1;
    *(uint32_t *)(vm + CE_ANM_VM_COLOR2) = c1;
}

/* ★★ 2026-09-15h：本块的门限**不再自己推** —— 交给跨卡统一件 `cards/hud_dim.{h,c}`（唯一实现）。
 * 这里只做两件事：① 把本块的**表面矩形**交给它；② 把它给的 alpha 写进各元素。
 * 矩形（表面 px）= 本帧槽位左下角 `(sx, sy)` + `FH_BLOCK_W×FH_BLOCK_H`（188×60 = 640 空间的 94×30）；
 * 换算 `自机 = 表面/2 − (320, 16)` 在共享件里（一手依据与 `224` 的订正见本文件上部的长注）。
 * ★ 与一手的差异（都是用户 2026-09-15f 拍板的「UI 统一」，**不是复刻**）：
 *   ① 两个沿**由本块矩形推**（一手两作各用它们自己块的沿 ⇒ 数值不同，那不是「不统一」）；
 *   ② 过渡由 **C 每帧写 alpha** 驱动（一手是发中断 2/3 让脚本 `alphaTime` 插值）。
 * ⚠ `s_hud_dim.alpha` 是**唯一**的 alpha 作者（前 20 帧除外 ⇒ 脚本的淡入）⇒ 不许再往 VM 发中断 2/3，
 *   也**别**改成「读回当前 alpha 再乘系数」（每帧复合会一路衰减到 0）。*/
static void fh_dim_tick(float sx, float sy)
{
    hud_dim_rect_t r;
    const float *pp = ce_player_pos();
    int i;

    /* ① 每帧现算本块矩形（块会随货架移动）→ ② 共享件：闩锁（边距 28 + 滞回 16）+ alpha 斜坡（8 帧线性 → 64）
     * ★★★ 2026-10-03q（owner：「**所有左下角 HUD 应当是"一块 HUD"**」）：矩形改用**整条带**
     *   （`ce_hud_band_rect()` = 所有已认领块 ∪ 原生卡面）⇒ 自机压到带里**任意一块**时，
     *   带里的每一块**一起淡**（不再"只淡被压到的那块"）。空带兜底回本块矩形。*/
    {
        float bx, by, bw, bh;
        ce_hud_band_rect(&bx, &by, &bw, &bh);
        if (bw <= 0.0f || bh <= 0.0f) { bx = sx; by = sy; bw = FH_BLOCK_W; bh = FH_BLOCK_H; }
        hud_dim_rect_surface(&r, bx, by, bw, bh);
    }
    (void)hud_dim_tick(&s_hud_dim, &r, pp);

    /* ③ 前 `FH_WARMUP_FRAMES` 帧**一帧都不写**：把 alpha 让给脚本自己的 20 帧淡入
     *    （`alpha(0)` + `+20: alphaTime(20,0,255)`）。第 20 帧结束时脚本的目标正好是 255
     *    ⇒ 之后脚本不再动那一格 ⇒ 从这一帧起 C 是唯一作者。*/
    if (s_age < FH_WARMUP_FRAMES) { s_hud_dim.alpha = HUD_DIM_ALPHA_FULL; return; }

    /* ④ 写进各元素的**两路**顶点色（只改 A，RGB 原样保留 —— 脚本的基色 / 闪色 / 白条都在 RGB 里）
     *    ⚠ 两条连击条不在这个表里：它们的 color_1 由 `fh_place_combo()` 从**数字位**的 alpha
     *    现取现拼（一手 `Gui+0x8393` 的「借位」）⇒ 数字位被写成什么，它们就跟着是什么 ✓。*/
    fh_set_alpha(s_icon);
    fh_set_alpha(s_gauge_bg);
    fh_set_alpha(s_gauge_bar);
    fh_set_alpha(s_star);
    for (i = 0; i < FH_DIGITS; ++i) fh_set_alpha(s_digit[i]);
    for (i = 0; i < FH_MULT_N; ++i) fh_set_alpha(s_mult_vm[i]);
}

/* 档位闪色（一手 `th11:0x41b593`–`0x41b66e`；**只在边沿发**）。
 * ⚠ 110 单装（111 不在场）时**一帧都不发**：用户 ③ 要「信号满格（**不闪烁**）」。*/
static void fh_flash_tick(int have111, int32_t signal)
{
    int32_t lvl;

    if (s_age < FH_WARMUP_FRAMES) return;                 /* 一手的前 20 帧门 */
    if (!have111) {
        if (s_flash > 0 && s_gauge_bar) {                 /* 111 中途被删 ⇒ 收回常态白 */
            ce_anm_interrupt(s_gauge_bar, FH_INT_IDLE);
            s_flash = 0;
        }
        return;
    }
    lvl = (int32_t)faith_hud_signal_flash(signal);
    if (lvl == s_flash) return;
    if (s_gauge_bar) ce_anm_interrupt(s_gauge_bar, (int)(FH_INT_IDLE + lvl));
    s_flash = lvl;
}

/* ============================ 诊断（只读；实跑对账用）============================
 * 「HUD 起全了」那一次逐支打一行（旧 `mof.c` / `signal.c` 各有一套，合并后一套就够）；
 * 此后每 `FH_LOG_EVERY` 帧一行汇总。字段与偏移沿用两块旧 HUD 的口径（那里有完整出处）。*/
#define FH_LOG_EVERY      600
#define FH_VM_LAYER       0x18      /* zAnmVm.layer（`HUD-LAYOUT.md` §4.1 的判据字段）*/
#define FH_VM_POS_SCRIPT  0x2c      /* 脚本 `pos()`（与 `CE_ANM_VM_POS`(+0x5f0) 相加才画到屏幕）*/
#define FH_VM_FLAGS_LO    0x534     /* flags_lo：bit0/1 绘制门槛、bits17–19 colorMode、bits26–30 渲染模式*/
#define FH_VM_MIXED       0x52c     /* mixed_inherited_color：真正画出去的顶点色*/

static void fh_diag_row(const char *tag, uint32_t id, int32_t want)
{
    const uint8_t *vm = ce_anm_get_vm(id);
    const float *ent, *scr;

    if (!vm) {
        ce_log("faith_hud: %-8s id=%u %s", tag, (unsigned)id,
               id ? "★ VM 追不回（id 有、VM 没了）" : "（这一支没起来 / 起失败）");
        return;
    }
    ent = (const float *)(vm + CE_ANM_VM_POS);
    scr = (const float *)(vm + FH_VM_POS_SCRIPT);
    ce_log("faith_hud: %-8s id=%d sprite=%d%s layer=%d flags=%08x c1=%08x c2=%08x eff=%08x 画在 (%.0f,%.0f)",
           tag, (int)id, (int)*(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID),
           (want >= 0 && *(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID) != want) ? " ★不符" : "",
           (int)*(const int32_t *)(vm + FH_VM_LAYER),
           (unsigned)*(const uint32_t *)(vm + FH_VM_FLAGS_LO),
           (unsigned)*(const uint32_t *)(vm + CE_ANM_VM_COLOR1),
           (unsigned)*(const uint32_t *)(vm + CE_ANM_VM_COLOR2),
           (unsigned)*(const uint32_t *)(vm + FH_VM_MIXED),
           (double)(ent[0] + scr[0]), (double)(ent[1] + scr[1]));
}

static const char *fh_pres_name(int mask)
{
    const int a = (mask & (1 << FAITH_SLOT_110)) ? 1 : 0;
    const int b = (mask & (1 << FAITH_SLOT_111)) ? 1 : 0;

    if (a && b) return "110+111";
    if (a)      return "只有 110（信号中性 1.00 / 满格不闪）";
    if (b)      return "只有 111（连击条填满）";
    return "无（整块不画）";
}

/* ============================ 每帧入口 ============================ */
void faith_hud_draw(void)
{
    /* ★ 门（两张卡原来各自判一次 ⇒ 现在共用一套）：`on_draw`（虚表 `+0x48`）在**主菜单 / 标题**
     * 里也会被调，而 `on_tick_2` 不会。门关着时把 VM 收掉（别只是不画 —— VM 会留着被别处按旧坐标重画）。*/
    if (!CE_PLAYER() || !CE_GAME_THREAD()) { fh_kill(); return; }

    /* 两张卡都会调本函数 ⇒ **一帧只画一遍**（帧守卫与 `faith_pool_tick()` 同一口径）。*/
    {
        static uint32_t s_last_frame;
        static int      s_have_last;
        const uint32_t now = ce_sdk_frame();
        if (s_have_last && now == s_last_frame) return;
        s_last_frame = now;
        s_have_last = 1;
    }

    {
        const int mask = faith_registered_mask();
        const int have110 = (mask & (1 << FAITH_SLOT_110)) ? 1 : 0;
        const int have111 = (mask & (1 << FAITH_SLOT_111)) ? 1 : 0;
        ce_hud_slot_t sh;
        float bx, by;
        int32_t signal, combo, maxpoint, mult;
        char mtxt[FH_MULT_N + 1];
        int i, all;

        if (!have110 && !have111) { fh_kill(); return; }   /* 两张都不在 ⇒ 整块不画 */

        fh_claim();
        if (!s_owner || !ce_hud_slot_get(s_owner, &sh)) { fh_kill(); return; }
        /* ⚠ `ce_hud_slot_get()` **只填 x/y/w/h/side**（不填 x640/y640）⇒ 自己换（同 `signal.c`）。*/
        bx = (sh.x + FH_BLOCK_LEFT_PAD) * CE_HUD_SURFACE_TO_640;   /* ★ z52：画面不动（补回左延的 5px）*/
        by = sh.y * CE_HUD_SURFACE_TO_640;

        /* ---- 读数：在场的那一槽用真值，不在场的用**中性值**（用户 ③）---- */
        maxpoint = faith_hud_maxpoint(have110 ? s_decl[FAITH_SLOT_110].delta_fixed : 0,
                                      have111 ? s_decl[FAITH_SLOT_111].delta_fixed : 0);
        signal   = have111 ? s_decl[FAITH_SLOT_111].signal : FAITH_HUD_SIGNAL_NEUTRAL;
        combo    = have110 ? s_decl[FAITH_SLOT_110].combo  : FAITH_HUD_COMBO_NEUTRAL;
        /* ★★★ 2026-09-26（owner 追问：「**擦弹倍率是 th10 的东西吗**」）—— **不是，是 TH11（111）的**：
         *   一手 = `th11:0x41f4e7`（步数上限 `0x383` = **899** ⇒ 8.99）+ `th11:0x41f640`（信号强度），
         *   两者相加那条 `add` 也在 TH11（`th11:0x41ce1f`）⇒ 这一整项属于**地霊殿（111）的得点公式**。
         *   用户规格（`sdk_core.c` 的 `ce_score_coupling_permille()` 里逐字引着）：
         *     「`(擦弹倍率 + 信号强度) if 有 111 else 1`」⇒ **只装 110 时耦合必须恒 1.00**。
         *   ⚠ 计分层**本来就是对的**（`mof_core_pv()` 的「只装 110」支 = `PV = 信仰`，**无乘数**；
         *     `test_mof_core.c` 用 `mof_core_pv(1,0,55000,8990,130000) == 55000` 钉着）——
         *     漏的只是**这一行显示**：原来无条件加擦弹项 ⇒ 只装 110 实跑里擦弹过 100 就画 `×1.01`
         *     （owner 报的现象；日志 27/27 采样都精确等于 `1.00 + ⌊graze/100⌋×0.01`）。*/
        mult     = faith_hud_mult_permille(
                       have111 ? mof_core_graze_mult(*(const volatile int32_t *)FH_GRAZE_ADDR) : 0, /* 擦弹项：**只在 111 在场时** */
                       faith_hud_signal_strength(signal));                              /* 信号项（中性 1.00）*/
        faith_hud_mult_text(mult, mtxt);

        /* ★★ 站点自检门 ⑥（2026-09-18，**只记账**；用户口径逐字：「收的点得符合最大得点公式算出来的那个」）
         *   屏幕上这一对（6 位**最大得点** × **倍率**）与「收点实际入账的点数」必须同源同值。
         *   期望值用**与 UI 同一份**函数算（上面刚算出的 `maxpoint` / `mult`，不另写一份公式）：
         *       E = trunc10(最大得点 × 倍率/1000)          ← 一手形状 = TH11「点道具得分 = trunc10((A+B)×Y/100)」
         *   实付值取计分层当前的总数（`ce_score_aggregate()`：`G_0 + Σ非耦合 + 耦合系数×Σ耦合/1000`）。
         *   只在**两层都在场**时才比（UI 有槽位 + 计分层有参与者），否则两边本来就不是同一件事。
         *   ⚠ 不等**只记一行**（不改任何数值 —— 改数值比记一行危险得多）；本门是给「公式漂了」留现场。*/
        if ((have110 || have111) && ce_score_participants() > 0) {
            const int32_t expect_points = faith_hud_trunc10(
                (int32_t)(((int64_t)maxpoint * (int64_t)mult) / 1000));
            const int32_t paid_points = ce_score_aggregate();
            if (expect_points != paid_points)
                ce_assume_fail("faith HUD: 收点实付 ≠ 最大得点×倍率（只记一次；见 AUDIT §BE-EA）");
        }

        /* ---- 起 VM + 摆位（**顺序是行为**：后起的画在上面）---- */
        fh_place_gauge(bx, by, signal);      /* ① 槽底 → ② 增长条 */
        fh_place_icon(bx, by);               /* ③ 蓝点 */
        fh_place_digits(bx, by, maxpoint);   /* ④ 6 位数字 */
        fh_place_star(bx, by);               /* ⑤ 「×」*/
        fh_place_mult(bx, by, mult);         /* ⑥ 倍率 */
        /* ★★ 压暗淡入**摆在连击条之前**：`fh_place_combo()` 的 alpha 是**从数字位现取**的
         *   （一手「借位」）⇒ 先由 `fh_dim_tick()` 把这一帧的 dim 值写进数字位，条才会同帧跟上；
         *   反过来的话条会慢一帧（淡入淡出时看得见一帧的错位）。
         *   ⚠ 它必须**在所有 VM 都起过之后**才有东西可写（第一帧 VM 还没起 ⇒ 这一帧只是空转）。
         *   ⚠ 传的是**表面 px 的块左下角**（= 本帧槽位 `sh`），**不是** 640 空间那份（`bx/by` 只喂摆位）——
         *     共享件自己做 `表面/2 − (320,16)` 的换算（2026-09-15h 的订正：原来是 `224`）。*/
        fh_dim_tick(sh.x, sh.y);
        fh_place_combo(bx, by, combo);       /* ⑦ 连击条两条 */
        fh_flash_tick(have111, signal);
        ++s_age;

        /* ---- 诊断（只读）---- */
        if (mask != s_pres_last) {
            s_pres_last = mask;
            ce_log("faith_hud: 在场 = %s（判据 = faith_registered_mask() 0x%02x，**不是**槽值）",
                   fh_pres_name(mask), mask);
        }
        if (!s_up_logged) {
            all = fh_alive(s_icon) && fh_alive(s_gauge_bg) && fh_alive(s_gauge_bar) && fh_alive(s_star);
            for (i = 0; i < FH_DIGITS && all; ++i) if (!fh_alive(s_digit[i])) all = 0;
            for (i = 0; i < FH_MULT_N && all; ++i) if (!fh_alive(s_mult_vm[i])) all = 0;
            for (i = 0; i < 2 && all; ++i) if (!fh_alive(s_bar[i])) all = 0;
            if (all) {
                s_up_logged = 1;
                ce_log("faith_hud: 组合 UI 16 支 VM 全在（槽底/条/图标/6 数字/×/4 倍率/2 连击条；"
                       "「画在」= entity_pos + 脚本 pos，屏面 1280×960）");
                fh_diag_row("icon", s_icon, (int32_t)CE_ANM_ABILITY_SPRITE_SIGNAL_GAUGE_ICON);
                fh_diag_row("gauge_bg", s_gauge_bg, (int32_t)CE_ANM_ABILITY_SPRITE_SIGNAL_GAUGE_BG);
                fh_diag_row("gauge_bar", s_gauge_bar, (int32_t)CE_ANM_ABILITY_SPRITE_SIGNAL_GAUGE_BAR);
                for (i = 0; i < FH_DIGITS; ++i) {
                    char tag[12];
                    tag[0] = 'd'; tag[1] = (char)('0' + i); tag[2] = '\0';
                    fh_diag_row(tag, s_digit[i], -1);
                }
                fh_diag_row("mul", s_star, (int32_t)CE_ANM_ABILITY_SPRITE_SIGNAL_MUL);
                for (i = 0; i < FH_MULT_N; ++i) {
                    char tag[12];
                    tag[0] = 'm'; tag[1] = (char)('0' + i); tag[2] = '\0';
                    fh_diag_row(tag, s_mult_vm[i], -1);
                }
                fh_diag_row("bar", s_bar[0], -1);
                fh_diag_row("bar_sh", s_bar[1], -1);
            }
        }
        if (++s_diag_frame >= FH_LOG_EVERY) {
            /* ★ 2026-09-15f：连同**行 3 两条矩形实际画在哪**一起打（用户第六条「阴影得是右下角」的判据）——
             *   直接从两个 VM 的 `entity_pos` 读出表面坐标，`影−亮` 应当是 **(+2, +2) 表面 px**
             *   （= 块内 +1 右 / −1 上 ⇒ 一手 TH10 的「影 = 亮条右下各 +1」）。
             *   ⚠ 它是**机器判据**：读的是真画出去的位置，不是我们以为的常量。*/
            const float *bp = fh_alive(s_bar[1]) ? (const float *)(ce_anm_get_vm(s_bar[1]) + CE_ANM_VM_POS) : 0;
            const float *sp = fh_alive(s_bar[0]) ? (const float *)(ce_anm_get_vm(s_bar[0]) + CE_ANM_VM_POS) : 0;

            s_diag_frame = 0;
            ce_log("faith_hud: 块@(%.0f,%.0f) | %s | 行2 %d ×%s | 行1 %d.%02d（档 %d / 宽 %d px）| 行3 连击 %d",
                   (double)sh.x, (double)sh.y, fh_pres_name(mask),
                   (int)maxpoint, mtxt,
                   (int)(faith_hud_signal_strength(signal) / 1000),
                   (int)((faith_hud_signal_strength(signal) % 1000) / 10),
                   (int)faith_hud_signal_level(signal),
                   (int)faith_hud_signal_width_px(signal), (int)combo);
            /* ★ 2026-09-15h：压暗淡入的实况（❓ 用户口径的统一值：**块矩形 + 边距 28**、
             *   ★ 2026-09-22 起 终点 **64**（25%）/ **8 帧线性** = TH18 原生一手）：`L` = 闸锁（1 = 已进入压暗），
             *   `a` = **本帧真正写进各 VM 的 alpha**（255 ↔ 128）、`t` = 本次过渡已走帧数。
             *   判据：自机**靠近块**（默认锚点 = 矩形 x −191.5..−97.5 / y 416..446，**外扩 28** 之内
             *   ⇒ 实际可触发区 x ∈ [−184, −69.5] / y ∈ [388, 432]）⇒ 20 帧内 `a` 走到 128；
             *   **出到「矩形 + 44」之外** ⇒ 20 帧内回到 255。这一行同时打印**本帧推出来的矩形**，
             *   所以不用去猜门限是不是变过（它随货架槽位走）。`a` 卡在 255 而人确实贴着块
             *   ⇒ 闸锁/坐标有问题（`pp` 来自 `ce_player_pos()`，自机坐标口径 = `hud_dim.h` 的 `表面/2 − (320,16)`）。*/
            {
                hud_dim_rect_t r;
                hud_dim_rect_surface(&r, sh.x, sh.y, FH_BLOCK_W, FH_BLOCK_H);
                ce_log("faith_hud: 压暗 L%d a=%d（目标 %d，过渡 %d 帧线性、已走 %d）| 本块矩形(自机坐标) x %.1f..%.1f / y %.1f..%.1f"
                       "（进 = 落在「矩形 + %.0f」内；出 = 出到「矩形 + %.0f」之外）",
                       (int)s_hud_dim.dim, (int)s_hud_dim.alpha,
                       (int)(s_hud_dim.dim ? HUD_DIM_ALPHA_DIM : HUD_DIM_ALPHA_FULL),
                       (int)HUD_DIM_FRAMES, (int)s_hud_dim.t,
                       (double)r.x0, (double)r.x1, (double)r.y0, (double)r.y1,
                       (double)HUD_DIM_MARGIN, (double)(HUD_DIM_MARGIN + HUD_DIM_BAND));
            }
            if (bp && sp)
                ce_log("faith_hud: 行3 矩形 亮@(%.0f,%.0f) 影@(%.0f,%.0f) ⇒ 影−亮 = (%+.0f,%+.0f) 表面 px "
                       "（应当 (+2,+2) = 一手「右下各 +1」；x 正 = 右、y 正 = 下）",
                       (double)bp[0], (double)bp[1], (double)sp[0], (double)sp[1],
                       (double)(sp[0] - bp[0]), (double)(sp[1] - bp[1]));
        }
    }
}
