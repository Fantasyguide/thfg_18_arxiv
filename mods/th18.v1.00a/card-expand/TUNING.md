# TUNING —— 待统一处理清单（UI 位置 / 表现 / 音频 / 手感）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **缘起（用户 2026-09-12）**：「**按照我不能打开游戏测试来算，所有 UI 的位置问题得到时候统一处理，
> 甚至包括 UI 的表现、音频之类的，先做能做的**。」
> **用途**：实现期**不纠结**任何看得见/听得见的东西 —— 一律用**具名常量 + `❓ 自定`**落地，
> 在这里记一行（在哪 / 现在是什么 / 将来怎么定），**等能开游戏时一次性过**。
> **不做**在这份清单里的：逻辑正确性、一手可核的引擎事实、replay 安全 —— 那些必须在实现期做对。

## 0. 两条规则

1. **不许用「待实跑」当借口跳过一手核查**：能从头文件/反汇编定死的（层号、字段偏移、脚本号语义、
   数值量纲）必须现在核到底；核不到的**标 `❓ 未证实`** 并写清「缺哪一条一手证据」。本清单收的是
   「**只有看着屏幕 / 听着声音才能定**」的那一类。
2. **一个旋钮只留一处**：所有待调值都是 `#define` 具名常量（或一个宏开关），改它不需要动逻辑。
   UI 位置一律走**货架锚点 + 偏移**（`HUD-LAYOUT.md`），不要写死绝对坐标。

## 1. 跨卡通用项

| # | 项 | 现状 | 统一处理时要定什么 |
| --- | --- | --- | --- |
| G1 | **暂停模糊层** | ✅ 规则已一手定：`0x4882b8` 把 layer 24–32 折到组 37–45、**其它层兜底折到 39**；模糊罩 = 层 38 ⇒ **只有 layer 24/25 会被罩住**（`HUD-LAYOUT.md` §4.1）。我们的 HUD 脚本已全在 24/25；场上实体用世界层（3–19，同样在模糊面内）| 只需**实跑确认**一次观感（罩子是否如期盖住）。**新脚本一律 `layer(25)`** |
| G2 | **`drawRect` 默认锚点两说** | `AUDIT.md` §AL20 的**两次实跑**说「左上角」；`signal.c` 旧注释引 season.c 说「图心」。⇒ 111（`SIGNAL_CELL_ANCHOR_CENTER`）与 112（`UFO_HUD_RECT_ANCHOR_CENTER`）各留**一个宏整体翻** | 看一眼格子里式：若整排偏半个格 ⇒ 翻宏。翻完把结论写回 §AL20 与两处注释 |
| G3 | **货架标定** | 左锚点 257 / 底边 924 / 间隙 8 / 左列软上限 1000 —— 前三个是实测，**X_MAX 与「块高怎么报」是拍的**（`HUD-LAYOUT.md` §6.1/§6.3）| 看 116/117 上架后有没有挤 / 行距是否合适；顺带定 `h` 的取法（视觉最高元素？含上下 4 px？）|
| G4 | **整摞淡出**（自机贴近左下角）| **未接**：`ce_hud_near_corner()` 每帧由 SDK 推进，但没有卡读它（形制未定，`HUD-LAYOUT.md` §4.5）| 定「哪些块一起淡、淡到多少（现按 64/255 写的）、迟滞是否够」（`SENRO_HUD_DIM` 是唯一现成的 alpha 占位）|
| G5 | **ASCII 文字罩不住** | 结构限制（`engine/anm/th18/03-ascii-text.md` §2f）：`ce_hud_text*` 那几趟绘制的时刻渲染目标不是装 UI 的 surface ⇒ **永远不被暂停模糊罩盖住**。受影响的：**116 的档位数字**、**80 股市面板的全部文字**、**114 的 bonus 弹字**（用户已豁免 114）| 要不要把 116 / 80 的文字也改成**贴图 VM**（先例：117 的槽与数字、107/110/111/113 的文字）|
| G6 | **HUD 与 HUD 文字的前后关系** | 我们的块**只会压在 HUD 文字上面**（`HUD-LAYOUT.md` §4.2，硬约束：块右缘 ≤ 1040）| 实跑看有没有压住该露的读数 |
| G7 | **`Gui` 字号/缩放** | `ce_hud_vm_text_*` 的 scale 与 640/表面换算（`CE_HUD_SURFACE_TO_640 = 0.5`，🟡 待实跑确认系数）| 确认 1280×960 表面下 0.5 是否正好；文字大小是否与原生一致 |

## 2. 逐卡清单（106–117）

> 「位置」一栏给的是**文件·常量前缀**；全部可用 `grep -n "❓" cards/<文件>.c` 找到行号。

| id | 位置 / 表现 | 音频 | 其它待调 |
| --- | --- | --- | --- |
| **106** 红魔乡 | 判定点 alpha=0（不是位置，是**观感**：脚本若每帧重设颜色会压不住）；无自建 HUD | — | rank 数值手感（12 条增减量级）—— 实跑打两关看档位变化快不快 |
| **107** 妖妖梦 | `SENRO_HUD_W/H/ROW_DY`（块 200×40、行距 12，❓）；三行读数落点；结界特效 VM **未做**（现用 SE + 震屏占位）| `SENRO_SE_BORDER_OPEN/END`（都借 `0x2f`）、`SENRO_SE_LIBYOU`（借 `0x09`）| 中弹灵击的消弹延后 1 帧（用户说「手感几乎没影响」）；点道具分值基线 50000（用户说「粗暴缝合没关系」）|
| G1 | **暂停模糊层** | ✅ 一手已定：layer 24/25 会被罩住（其余兜底折到组 39、在罩子之后）。我们的 HUD 已全在 24/25；场上实体用世界层（3–19，也在模糊面内）| 实跑确认观感一次 |
| G2 | **`drawRect` 默认锚点两说**：§AL20 两次实跑 = 左上角；旧注释 = 图心 ⇒ 111/112 各留一个**整体翻的宏** | 看一眼格子/条：整排偏半格就翻宏，并把结论写回 §AL20 |
| **110** 风神录 | `MOF_HUD_FAITH_X/Y`（640 空间 52/366/378）、`MOF_HUD_COLOR_*`；**绿点实体**借季节点贴图 + `ce_anm_set_color` 染色（TH18 没有绿点 item type）| — | 灵击伤害/震屏（`MOF_BOMB_DMG/DMG_LIFE/SHAKE_*` 全是 ❓）；boss 判据占位；蓝点高度系数曲线（已按 spec 线性，端点值待看）|
| **111** 地灵殿 | `SIGNAL_HUD_BLOCK_W/H`（96×20）、`SIGNAL_CELL_*`（12/12/7/6）、字号（scale 0.75）、4 个颜色；**5 格是 `drawRect` 拼的**（要更像原版得切 TH11 的信号槽图）| — | 灵击伤害/震屏（`SIGNAL_BOMB_*` ❓）；河童 bomb 的区分未做 |
| **112** 星莲船 | `UFO_HUD_*`（块 112×30、格 20×20、血条 112×6、7 个颜色，全 ❓）；**贴图与 anm 全缺** ⇒ 用 `drawRect` + 贴图 VM 占位；五边形提示框只有判据+钩子；**令牌/UFO 没有 anm 脚本**（TH12 script135–138 未移植）⇒ 真 ECL 已装但看不见 | `UFO_SE_*` 借现成 id；`UFO_SE_ITEM_BREAK` 占位 | TTL 2400、飘行框、生成间隔、金是否独立一色（现归一成蓝）|
| **113** 神灵庙 | `TRANCE_HUD_X/Y`（两行手摆、未走货架）；神灵实体借 script129 + 原版 16×16 魂贴图 + 一手 TH13 染色值 | `TRANCE_SE_*` 5 个**全 ❓自定** | 池 128 / 吃取半径 24 px / bob；中弹无伤=每帧声明 30 帧无敌（近似）；计分载体搁置 |
| **114** 辉针城 | bonus 弹字走引擎自己的 `ce_popup_mult` ⇒ **暂停时清晰**（用户已豁免：「复用老代码但是 ZUN 动层了没办法」）| — | — |
| **115** 绀珠传 | 面板 6 条 VM + 数值行在层 24/25（会被罩住 ✓）；中左面板不参与货架（`HUD-LAYOUT.md` §5 建议用 `CE_HUD_SIDE_PINNED` 登记，**未做**）| — | 建议统一处理时把它登记进表（不挪位置）|
| **116** 天空璋 | **已迁货架**（块 104×36、锚点 = 第一格 257/924；所有元素 = 锚点 + 偏移，迁移前后逐像素相同）；档位数字是 **ASCII**（罩不住 ⇒ 见 G5）| — | 见 G3（它的块是第一个有实跑数据的 h）；与 117 的**透明边**重叠（把块宽 104→112 即可消除）|
| **117** 鬼形兽 | **已用「自带锚点」登记**（193×48 @ (88,924)，5 格坐标逐像素不变）；消弹圈只剩 2 处（霊撃认领 / 水獭 3 光球，鹫束消弹已删）；魔理沙鹫束视觉 **2048×128 px**（两轴同一 `SCALE`）× 判定宽 34 · 外流 `scrollX(-1/21)`；换面清实体挂 `on_stage_start`（商店不清）| — | 与 116 同上；`animal_card_t` 大小见静态断言；手感旋钮 = `ANIMAL_EAGLE_BEAM_SCALE`（整体放大）/ `ANIMAL_EAGLE_HOMING_EXTRA`（灵梦鹫转角）/ 鹫束外扫占空 ⇒ 见 §2.1 |

### 2.1 117 魔理沙鹫束 / 灵梦鹫追尾 / 放灵：哪些是一手、哪些是用户口径 / 手感旋钮

- **一手（不许动）**：TH17 `pl01.anm` script16 只把贴图**宽**拉到 4×（32→128 px）；长度引擎写
  `vm+0x68=+0xa0/512`（`th17:0x44bfa3`），**一手上限 512**；判定宽 34 = TH17 `pl01a.sht` EAGLE `hit_h`；
  外流 = script16 的 `scrollX(-1/21)`；渐变圆收回 = script32 的 interrupt 1（30 帧）/ interrupt 2（50 帧）。
  灵梦鹫追尾 = TH17 tick1 ≡ TH18 tick1（半径 256 / 转角 0.08 / 60 帧窗口 / 速度三档，逐指令对账，§AL49）。
- **用户口径（本轮覆盖一手，改前先读 §AL49）**：
  - `ANIMAL_EAGLE_BEAM_SCALE = 4.0`：**唯一缩放旋钮**，两轴共用 ⇒ 长 512→**2048 px**、宽 32→128 px
    （用户口述「整体 3 倍」，沿用他已认可的 4×；**想切 3× 只改这一个数** = 1536 px）。远端必须顶出画面。
  - 暴走咆哮正常结束的 2 只 = **本次咆哮那一只**（`animal_end_burst` 的 `roar_type` 1..3）；
    混合 4 / 未知 0 = 回退一手随机 `1+rand()%3`（TH17 `0x40ee03` 本来就是随机，见 §AL49）——
    改这条要动 `animal_core.c`，不是 TUNING 旋钮。
- **手感旋钮**：`ANIMAL_EAGLE_HOMING_EXTRA`（0.08 = 灵梦鹫每帧额外转角，转弯半径减半；**改 0.0 回纯一手**）·
  `ANIMAL_EAGLE_BEAM_SHOT_FRAMES`（每发 15 帧 = TH17 `rate`）·
  `ANIMAL_EAGLE_BEAM_LOOP`（1 = 循环；嫌脉冲改 0 = 拉满一次保持）·
  `ANIMAL_EAGLE_BEAM_MIN_FRAC`（0.5 = 每发起始半程；因满长已 2048，抬到 1.0 也仍出屏）。

### 2.1b 117 咲夜 / 早苗兽灵咆哮（AL59–AL67，2026-09-13，**全部 ❓ 自定**）

> 一手：TH17 **没有**咲夜/早苗的兽灵行 ⇒ 唯一硬口径是「DPS 大致等于一手量级」。
> 素材/一手见 `AUDIT.md` §AL59a/§AL60b/§AL61a/§AL62/§AL65/§AL66/§AL67；**数值全部可调**。
> ★ AL62：颜色改成**构建期改像素**（源图可编辑）+ 咲夜刀 ×2 + 早苗狼出膛点再前推。**下方是 AL62 后的默认值。**
> ★ AL65（用户四条）：① 早苗狼 DPS 对齐**灵梦大鹫** 100/3 = **33.33**（5 行 rate3 × dmg20；不是灵梦狼 63.33）；
>   ② 早苗鹫 rate 6→12、dmg 48→96（DPS 仍 40、hit0 不变）；③ 咲夜狼汇聚点改「**贴板底恰好聚焦到板头**」，
>   `SAKUYA_WOLF_FOCUS_DY` 320 → **−432**（自机 y 钳位 [32,432]、弹幕区顶 y=0）；④ 咲夜鹫新增**正中正前方口**
>   （无摆动，DPS 10），旧 6 点扫荡周期 4→6（30→20），当时总 30。
> ★ AL66（用户「咲夜大鹫得加强，整体 dps 上升 1/3」）：咲夜鹫总 **30 → 40** = 扫荡 **30**（周期 6→4、dmg 20）
>   + 正面 **10**（不变）。即把 AL65 削掉的 1/3 还给扫荡；摆动公式/视觉/命中框/真发弹路径/退出清理不动。
> ★ AL67（用户「换成 dps 分布扫荡 20+正面 20」）：总 **40 不变**、火力更多挪到正面 =
>   扫荡 **20**（周期 4→6、dmg 20）+ 正面 **20**（dmg 20→**40** / 周期仍 2，弹流视觉不变）。
> ★ 诱导结案（用户 2026-09-13 裁定）：「一次性诱导的效果 ≈ 完整诱导，唯一边界情况是追踪的怪被打死了」⇒
>   早苗狼**不加逐帧追踪、行为不动**；边界情况（锁定的怪中途死亡）接受。一手见 `AUDIT.md` §AL65a。

**源图（RGB/尺寸都在这）—— 用户可自己改（AL63 语义）**
- Windows 侧：`D:\项目\幻想指南\18\art-src\` **根目录 = 零售原图**（13 张，未调色，供你上色）；
  子目录 `_auto_default/` = 构建缺图兜底的已上色默认图（**不是原图**）。
- 仓库规范输入：`assets/player_art_src/<key>.png`。**有图 = 原样使用（不套色）**；**缺图 = 零售原图 + 默认色兜底（内存，不落盘）**。
- 三步：改 art-src 原图 → `bash assets/sync_player_art_src.sh` → `cd native && make anm`。
- 默认色（只在缺图时兜底）：早苗 青蛙/爆炸/闪光 = mono→紫 H275、蛇 = hue→红 H0、狼命中光斑 = mono→浅红 H0/S0.65；
  咲夜 红刀 = 原样、蓝刀 = hue→紫 H275（没有零售紫刀）。
  （`player_art_lib.py` 的 `SPECS`：`mode` = keep/hue/mono，`hue` = 目标色相。）
- 想把 art-src 恢复成零售原图：`python3 assets/player_art_src/make_player_art.py --refresh-originals`。

**早苗（pl03）—— 数据路径**

| 旋钮 | 默认 | 在哪 | 怎么调 / 影响 |
| --- | --- | --- | --- |
| `PL03_SCRIPT_FROG` / `PL03_SCRIPT_SNAKE` | 29 / 30 | `append_shooterset.py` | 追加脚本号（与 `build_player_anm.py` 一致；`pl_anm_ids.h` 核对）|
| 视觉放大 `k` | `2.0` | `build_player_anm.py` `APPEND_PLAN[].k` | 乘脚本里所有 scale x/y；**改完 `make anm sht`** |
| `SANAE_EAGLE_SPREAD` | `(-0.6,-0.3,0,0.3,0.6)` | `append_shooterset.py` | 5 发青蛙扇面（rad）|
| `SANAE_EAGLE_DMG` / `RATE` | ★AL65：**`96 / 12`** | 同上 | 频率再降半、伤害加倍；DPS = 5×96/12 = **40.0** |
| 青蛙 `func_on_hit` | **0** | `_sanae_eagle_rows` | hit0 播爆炸、**不建爆炸伤害源**（改 4 = 多判）|
| `SANAE_WOLF_FORWARD_1` / `_2` | **`-28.0` / `-42.0`** | 同上 | ★AL62：内圈/中圈纵向偏移（负=前方）；x 固定 ±20/±40 |
| `SANAE_WOLF_DMG` / `RATE` / 5 行 | ★AL65：**`20 / 3`** | 同上 | 对齐**灵梦大鹫** 100/3 = **33.33**（偏差 0%）；`tick=4` 90° 折线不变；**诱导已结案**（一次性 ≈ 完整诱导，不加逐帧追踪）|
| 蛇/青蛙 hit/speed | 蛇 16×72/8；青蛙 48×48/7 | 同上 | 判定/速度 |

**咲夜 WOLF（pl02）—— 数据路径**

| 旋钮 | 默认 | 含义 |
| --- | --- | --- |
| `SAKUYA_WOLF_FOCUS_DY` | ★AL65：**`-432.0`** px | 汇聚点相对自机本体的 y 偏移（**负 = 正前方/上**）；含义 = 贴板底恰好聚焦到板头 |
| `SAKUYA_WOLF_DMG/SPAWN/rate` | 灵梦一手值 / 3 | DPS = 190/3 = **63.33** |
| `SAKUYA_KNIFE_SCALE` | **`2.0`** | ★AL62：刀视觉倍数（`build_player_anm.py` 的 `KNIFE_SCALE` 注入脚本 `scale(2,2)`）|
| `SAKUYA_WOLF_HIT_W/H` | **128 / 32** | ★AL62：跟视觉放大的命中框 |
| `anm` | `30` | pl02.anm 追加红刀副本（alpha 255、scale×2）|

> **★ AL65 #3 的汇聚距离算式（一手 y 值）**：
> ① 板底（自机可达最低位置）= **432.0 px**；② 板头（弹幕区顶端）= **0.0 px**。
> 来源：`Player__sub_45b170` `0x45B170` 把自机 y 钳在 `[0x1000, 0xd800]`（定点 1/128 px）⇒ `[32.0, 432.0]`；
> 弹幕区 384×448、`originMode(1)` 的 (0,0) = 上边框中点、场地正中 (0,224)
> （`engine/player/th18/01-position-and-state-timers.md` §2；`engine/anm/th18/01-vm-instantiate.md` §3；
> `engine/anm/th18/02-render-stages.md` 相机 0 = `(32,16,384,448)`）。
> **汇聚点 = 0 − 432 = −432.0**（自机本地 y，负 = 前方）；`atan2(FOCUS_DY − dy, −dx)` 逐行算角。
> 聚焦/低速只改移动速度与子机偏移（`player+0x476cc` / sht `+0x90` 块），**不改本体 y 钳位**；
> WOLF 组 `opt_slot=0`（body）⇒ 两档弹道基准都是本体 y，**无像素差**（若以后改成子机行，再按子机 y 重算）。

**咲夜 EAGLE（pl02）—— 卡侧真发弹**

| 旋钮 | 默认 | 含义 / 怎么调 |
| --- | --- | --- |
| `ANIMAL_SAKUYA_EAGLE_N` | `6` | **扫荡**发弹点数（φ 表按 6 硬编码；正中正面口另加 1 个，不动 φ）|
| `ANIMAL_SAKUYA_EAGLE_R` | `16.0f` px | 发弹点离自机多远（贴身，扫荡/正面共用）|
| `_THETA0` / `_OMEGA` | `0.6f` / `0.03f` | 摆动幅度/快慢；改 OMEGA 必须同步 `OMEGA_SIN/COS` |
| `_PERIOD` / `_DAMAGE` | ★AL67：**`6`** / `20` | 扫荡每点每 PERIOD 帧发一颗；DPS = 6×20/6 = **20.0**（AL66 曾 4→30）|
| `_FRONT_PERIOD` / `_FRONT_DAMAGE` | ★AL67：**`2`** / **`40`** | 正中正面口（θ=0、无摆动）每 2 帧一颗、每颗 dmg 40 ⇒ 40/2 = **20.0**（弹流节奏与 AL65 相同）|
| `_BULLET_SET` / `_ROW` / `_FRONT_ROW` | `EAGLE_1` / `0` / `1` | 卡侧 `shoot_one_bullet` 用；row0 = 扫荡 dmg20 / row1 = 正面 dmg40（主炮路径钉到空 EAGLE_0）|
| sht 行 `anm/speed/hit` | `31 / 18 / (96,16)` | ★AL62 命中框 ×2（`SAKUYA_EAGLE_HIT_W/H`）；两行逐字相同、只差 dmg |
| `KNIFE_SCALE` | `2.0` | 同上（`build_player_anm.py`；刀视觉 ×2）|
| `CE_FN_PLAYER_SHOOT_ONE_BULLET` | `0x45e930` | 引擎真发弹原语（thiscall，`ret 0x10`）|

> **★ AL65 #4 / AL66 / AL67 的 DPS 拆账**：AL65 旧总 30；AL66 总 30 → **40**；AL67 把 40 重新分配为
> **扫荡 20（6 点，周期 4→6、dmg 20 不变）** + **正中正面 20（1 点，dmg 20→40、周期仍 2、
> θ=0、无摆动、位置 = 自机正前 `(0,−R)`）** = **40**。用户给的方案 (b)：正面不靠加倍弹数，
> 而靠每发伤害翻倍 ⇒ 弹流视觉/节奏与 AL65 完全相同、不增加弹数。旧 6 点摆动公式/视觉/弹速/命中框**一字不动**。

> 改完 `animal.c`/`pl0X.sht`/`pl0X.anm` 必须 `make anm sht && make check`（含 `dllx87=0`）。
> 改源图只需 `bash assets/sync_player_art_src.sh && cd native && make anm`。

### 2.2 117 道中掉落：贴图体型 + 同波鹤立鸡群（AL58，2026-09-13 用户定案）

**判据（原话见 `AUDIT.md` §AL58）**：
1. 贴图 slot0 原始尺寸 ≥ **64×64** ⇒ 必掉（reason=`size`）。
2. 小于阈值的，按「同波（MainSub 数字）+ 族名/外观变体 + 贴图尺寸」归组：
   - 组内数量 **≤ `ANIMAL_ODD_GROUP_MAX`（3）** ⇒ 该组算「鹤立鸡群」⇒ 组内每只都掉
     （用户原话「<=3 也可以叫鹤立鸡群」）；
   - 组内数量 > 3：若同波存在 **严格过半多数派**（`max*2 > total`）且本组不是它 ⇒ 本组掉；
     否则（例如 `Cir00p=10`+`Cir00m=10`、`Cir01a=9`+`Cir02a=9`）不掉。
3. 纯杂鱼波 singleton guard：若本组族名前缀在 `Cir`/`ECir` 表中，且当前同波可计入敌人全部属于该表，
   则抑制 `≤3` 的 odd 误判——防止玩家秒杀纯杂鱼波第一只时把它当「唯一一只」掉。
   这只是 guard，不是掉落白名单；非 Cir 一律仍走尺寸 + 统计，不再被族名黑名单整片放行。

**可调常量（`native/cards/animal.c`）**：

```c
#define ANIMAL_BIG_SPRITE_MIN_W    64   /* enemy.png 右下角 sprite96..107 = 64×64（thanm 实测）*/
#define ANIMAL_BIG_SPRITE_MIN_H    64
#define ANIMAL_ODD_GROUP_MAX        3   /* 用户 2026-09-13：同波同类 ≤3 也叫鹤立鸡群 */
#define ANIMAL_ODD_MIN_SAMPLE       1   /* 最少样本总数；1 = 用户「哪怕只有 1 只」*/
#define ANIMAL_ODD_MAJORITY_NUM     1   /* 严格过半多数派 = max/total > 1/2 */
#define ANIMAL_ODD_MAJORITY_DEN     2
#define ANIMAL_WAVE_CENSUS_MAX      256  /* 每个 MainSub 波累计最多记录的不同 enemy_id 数 */
static const char *const animal_swarm_guard_prefixes[] = { "Cir", "ECir" };  /* 仅防纯杂鱼 singleton 误掉 */
```

- **体型阈值来源**：`/mnt/d/.../data/ANM/enemy/enemy.png` + 本地 `enemy.anm` 的 `thanm -l` sprite 表；
  右下角最大类 = sprite96..107（x 384..448, y 320..448）= 64×64；`script40..44` 逐个引用它们，
  `GirlD01/GirlD02` ECL 写 `anmSetMain(0,40)`。
- **运行时取尺寸**：slot0 VM id = `zEnemyData+0x124`（enemy+0x1350）→ `ce_anm_get_vm()` →
  `zAnmVmPrefix.sprite_size`（vm+0x74/+0x78）。退路 = `zEnemyData.final_sprite_size`
  （data+0x4fd8 = enemy+0x6204）。`anm_main`（enemy+0x149c）只是脚本号，不是 sprite id。
- **波键**：关卡脚本主上下文当前子程序名 → `MainSub` 数字（去尾字母）；`MainSub01/01b/01c`
  同键，`02/02b`、`06/06b` 同理。读法同 `lolk.c` / `engine/chapter/th18/03`。
- **统计口径（AL58 最终版）**：**按 MainSub 波累积 census**——波内每见到一只 `(行为体名, anm_main, W×H)`
  相同的敌人，就按 `enemy_id` 去重记入本波；`on_tick_2` 每帧 + `on_enemy_death` 死亡当帧更新；
  MainSub 数字变化时整表清零。`same/total/max` 全取自 census。
  **不用同屏数**：玩家手快/清屏时同屏只剩 1~2 只，会把 `Cir/GirlB` 波误判成 `same≤3` 而掉；
  census 与击杀节奏解耦。未见过就离场的成员仍会漏计，日志 `total` 可核对。
- **census 表**：`ANIMAL_WAVE_CENSUS_MAX=256`（波内不同 enemy_id 上限；满了打 `wave census full`）。
- **验收用例**：`GirlD01`×3 必掉（体型 + 组数≤3，非特例）；`GirlB01`×8 不掉（组数 8 >3、无少数派）；
  纯 `Cir00p/m`、`Cir00bp/m`、`Cir01a/Cir02a` 不掉；`GirlC01`、`GirlRedA01+GirlBlueA01`、
  `GirlRedA01n+GirlBlueA01n` 掉（组数 ≤3）；`GirlD02`×3 掉（体型 + 组数≤3）。
- **死因解码不变**：`animal_big_death_counts()`、`edeath_call_site()` 12-dword 扫描和
  `real_bit=1 + hp<=0 + 非 Cir/ECir` 兜底继续用（AL55ter）。
- Boss/中 Boss 仍在 `on_enemy_drop_pre` 结账；本路径只 skip，避免重复。

### 2.2b 117 整波否决（AL58b，用户 2026-09-13 更正版）

- 用户波名（1-based）：第一波 `MainSub00`；第二波 `MainSub01(+01b+01c)`；第三波 `MainSub02(+02b)`；
  第四波 `MainSub03`；第五波 `MainSub04`；道中 boss 后第一波 `MainSub05`；道中 boss 后第二波
  `MainSub06(+06b)`。
- 最终规则：第二波 `GirlC01` 掉；第三波**整波 veto**；第四波 `GirlD01`×3 掉；道中 boss 后第二波
  `GirlD02`×3 掉、`GirlB01`×8 不掉。
- `last-wave` veto **已作废**：此前误把“本关最大编号 MainSub”当 boss 前最后一波，实际那是道中 boss
  后第二波，大蝴蝶要掉。代码只保留 `ANIMAL_WAVE_VETO_THIRD`（`ANIMAL_THIRD_WAVE_KEY=2`）。
- `animal_wave_veto(wave)`：只有 `wave==2` ⇒ `third`；日志 `veto=third|none`。
- `death diag` / 掉落行带 `veto=`、`census=/total=`。

### 2.2c 117 静态组表（AL58c，取代运行时 census 判定）

- 实机三处误掉（第二波第一只杂鱼、道中 boss 前随机、道中 boss 后 GirlB01）根因 = 运行时 census
  在首杀时只有 1~2，命中 `same≤3`。**判定必须用“这一波这个组本来总共几只”。**
- 方案 A（已落地）：`native/tools/gen_animal_wave_table.py` 从 `st01..st07.ecl` 生成
  `native/cards/animal_wave_table.h`：
  - 用 `thecl -j -d 18` 反编译 ECL，解析 `MainSubNN`，**展开 `$A--`/`deci`+`jmpNeq` 循环**；
  - 用 `thanm -l 18 enemy.anm` 把 `anmSetMain/anmSetSprite(0,S)` 映射到 sprite W×H；
  - 得到 `(stage, wave, anm_main, w, h) -> total` 表。
- 运行时 `animal_enemy_drop_measure()` 优先查表取 `same/total/max`；`census=` 只作实测日志/兜底；
  表未命中时 `group_total=-1`，退 census 兜底。
- 1 面动态数（循环展开，一手）：wave0=20；wave1=红16+蓝16+GirlC1；wave2=GirlB8+红n12+蓝n12；
  wave3=GirlD3；wave4=Cir01a9+Cir02a18；wave5=Cir00b 20+20；wave6=GirlD2+GirlB8。
  只有 `GirlC01`(1)、`GirlD01`(64×64)、`GirlD02`(64×64) 掉；其余 `group_total>3` 不掉。
- `veto=third`（MainSub02）保留；`last-wave` veto 已作废；日志 `group_total=/census=/total=`。

### 2.2d 117 波号绑定到敌机（AL58d，2026-09-13 用户 2 面实机反馈）

- 现象：2 面第二波（3 只 `anm_main 214` / 48×48 中妖精）没掉。日志里 `wave=-1` 出现，导致静态表
  查不到 + census 键 -1 双失效。
- 原因：`animal_stage_mainsub()` 实时读在主上下文不是 `MainSubNN` 时会返回 -1。
- 修法：`enemy_id → wave` 绑定表（每帧首次见到时绑定，sticky last wave 兜底，表满环淘汰）；
  死亡优先用绑定波号；census 也按绑定波号分组；日志 `wave_src=bound|live|sticky|none`。
- 验收：2 面 `wave 1` 三只 48×48 中妖精各掉 1；2 面 `wave 0` 红蓝杂鱼不掉；1 面验收不变。

### 2.4 117 出场防重叠（AL57，无新旋钮）

- 初速四对角线是**用户观感口径（🟡，非一手硬规则）**；TH17 一手去重叠 = 距离² < 576 且点积 ≥ 0.96
  时 `+3.883222 rad`（≈222.5°）重试、最多 10 次，允许离开对角线。
- 唯一旋钮在 `native/cards/animal.c`：`ANIMAL_OVERLAP_R2 / _DOT / _RETRIES / _TURN`；
  中弹/真死 2 只的方向常量 `ANIMAL_MISS_HEADING_L/R`（135°/45°）只为保证一左一右，可改。
- 日志：`token spawn type N dir X deg (retry K...)`；若实机仍看到重叠，先看 `retry` 是否到 10 仍
  `still-overlapping`（那说明同向 token 太密，属于池子/出生密度问题，不是判据没跑）。

### 2.3 117 TH17 中 Boss 掉落（AL56，无新旋钮）

`on_enemy_drop_pre` 以 `sub` 名分叉：`MBoss*` 走中 Boss 口径，其余 `*Dead` 仍关底口径。
一手表见 `AUDIT.md` §AL56；可调项只有：

- `animal_drop_midboss_stage()` 的关卡号 1..7 映射（`CE_CURRENT_STAGE()` 一手是 1-based；
  万一实机读到 0-based，改这一个函数）。
- st05/st06 无条件 `item30 -> token15`（长寿命狼）已接；主线 1–4 的中 Boss 恒为 token4+token5。
- 相位边界（Extra `MBoss2`/`MBoss3`）= 1 随机长寿命动物 + token4+token5，已接。
- **资产缺口**：稀有主掉落 item23..29→token8..14（クラゲ/牛/ひよこ/亀/埴輪/埴輪马/三匹ひよこ）
  没有 `beast2*` 贴图与脚本，卡侧只打一行 `midboss rare conditional ... not spawned`，不发错种类。

## 3. 资产缺口（要别的手段，不是调参）

| # | 缺什么 | 影响 | 已有线索 |
| --- | --- | --- | --- |
| A1 | ~~TH12 `enemy.anm` script135–138 + `enemy/ufo_anim.png`~~ **✅ 已落地**：16 张切图 + `ability.anm` script 164–167；112 用**卡侧跟随 VM** 画（ECL 侧改 `anmSetSprite(0,-1)` 当隐形载体）⇒ 见 §3.1 | 实跑未验（❓ 跟随可能晚一帧）| `make_ufo_art.py`；第 4 组贴图**是黄的**（不是彩虹色），彩虹脚本 138 = 205/210/215/220 各取一帧 |
| A2 | 108 的资产：妖侧 shooterset `0x16` / 使魔四色 / 子机数量表 / 妖侧子机贴图 **全部 ✅**（详见 `AUDIT` §BO-8 与 §BO-9）；**仅剩「妖机体本体 sprite」的路由待用户拍板** | 108 低速弹幕的整条链已通（止射闸 + shooterset + 子机层） | TH08 提取已工具化：`tooling/thtk/extract_th08.py`（`ply*.sht` 要绕过 thdat bug）+ TH16 `pl03` 切图 |
| A3 | 111 信号槽 / 110 绿点 / 109 活化灵 / 113 神灵槽的**原生切图** | 现在都是 `drawRect` 或借图染色 | 各自的 spec §5；112 那一半**已落地**（见 A1/A5） |
| A4 | ~~107 结界特效 VM~~ **✅ 已落地**（脚本 **204** 甲 / **205** 乙×32，层 `19` ❓自定）—— 做法与 ❓ 见 `AUDIT.md` §BO-6；★ 持续期**另有**两块原版视觉（全屏变暗 + 自机红白闪）本期未搬 ⇒ `NEXT.md` §9z #1 | ✅ | th07 **已解包**：`local/th07.v1.00b/dat/` + `anm/<stem>/`，平坦反汇编 `local/_scratch/th07_full.asm` |
| A5 | ~~令牌贴图~~ **✅ 已落地**：12 张 + `ability.anm` script 168–173，同样由卡侧跟随 VM 画；令牌**继续用 `NoEnemyDebug`**（原版死循环空壳 = 正好是我们要的惰性载体，**不需要新 ECL**）| 实跑未验；彩虹三段与换色**已落地**（见 A8）| 一手 = TH12 `bullet.anm` script175..180 / sprite 444..449·453..458 |
| A6 | 112 的**扩圈消弹**已随 ECL 一起移植（`UFO_EtBreak`）| — | ✅ 不用另做 |
| A7 | ~~五边形提示框~~ **✅ 已落地**（2026-09-12，**112** 的框：脚本 **223**，`ufo.c` 真驱动建/跟随/删）。⚠ 原写「113」是串行笔误 | ✅ 一手：原版 TH12 `bullet.anm` script211 `drawPolyBorder(1.0f, 5)` | spec §5.9 |
| A8 | ~~令牌的彩虹三段 + 每 150 帧换色~~ **✅ 已落地**（2026-09-12）：三段一段不缺地补齐（脚本 **219–221**，`SCRIPT_FULL_PORTS` 生成并断言标签序列）；换色走 `ce_anm_delete` + `ce_anm_spawn(新脚本)` | 送中断用**已存在**的 `ce_anm_interrupt(id, n)`（**零新 SDK 面**）；150 帧是一手 `0x426483 cmp eax,0x96` + `0x426497` 清零 | TH12 `bullet.anm` script178/179/180；触发条件一手 = **距离 + 计时器**（60 px 圈 / 90 帧），**不是**「被打掉 / 状态转换」 |

### 3.1 112 的「anm 落点」问题（A1 / A5 / A8 共同卡点）—— **映射已复核，方案已定**

`ability.anm` 的脚本**不在 ECL 的 anm 索引表里** ⇒ 把 TH12 的飞碟/令牌脚本放进 `ability.anm` 之后，
ECL 敌机的 `anmSetSprite(0, S)` 指不到它们；而**原样保留** `anmSelect(1); anmSetSprite(0, 135..138)`
会去播 **`effect.anm` 的 135..138 号脚本**（那是 30 帧就 `delete()` 的真特效 ⇒ 看得见的杂图）。

**N（`anmSelect`）→ 文件**（一手复核完毕，全文见
[`engine/anm/th18/04-anm-slot-selection.md`](../../../engine/anm/th18/04-anm-slot-selection.md)；选择器 `th18:0x438d40`）：

| N | 文件 | 一手 |
| --- | --- | --- |
| 0 | `bullet.anm` | `th18:0x4cf2bc`(=BULLET_MANAGER)+`0x7a41ec` ← `preload_anm(7,"bullet.anm")`，写入 `th18:0x423b7a`–`0x423b86` |
| 1 | `effect.anm` | `th18:0x4cf2c8`(=EffectManager)+`0xc` ← `preload_anm(8,"effect.anm")`，写入 `th18:0x42aceb`–`0x42acf7` |
| **2** | **`enemy.anm`**（= 七关 ECL 头 `anim{}` 的第 0 条）| `ENEMY_MANAGER+0x170` ← 装载器 `th18:0x42da90` 写 `+0x170+i*4` |
| 3 | 当前关 `st0Xenm.anm`（`anim{}` 第 1 条）| `+0x174` |
| 4 / 5 | st05·st06·st07 = `st05enm2.anm`；仅 st07 = `st04enm` | `+0x178` / `+0x17c` |
| 7 | 当前关 `st0Xwl.anm` | `th18:0x4cf2b4`(=Stage)+`0x3464` ← `th18:0x41b907` |
| ≥8（除 7）| `anim{}` 第 `N-2` 条；**超条数 = NULL 解引用崩** | `+0x168+N*4` |

`K` = 本敌机自己的 anm 槽号（容量 16，`zEnemyData.anm_vm_ids[0x10]` = `zEnemyData+0x124`）；
`S` = 该 anm 文件的**扁平脚本号**（跨 entry 连续、**无越界校验**）。
**`anmSetSprite(K, -1)` = 拆掉该槽、不建 VM**（`th18:0x436bf6`；原生 14 处全用在 boss 死亡 / 符卡收尾）
⇒ 这就是原版「让这个槽不画东西」的官方手法，与 `anmSelect` 选到哪个文件**无关**。

三条出路（**本卡走第 2 条**，其余两条记在这里备选）：

1. **加一条 anm 构建管线**（照 `build_ability.py` 写 `build_enemy_anm.py`）：4+6 条脚本追加进 TH18 的
   **`enemy.anm`**（新脚本从 **261** 号起）⇒ ECL 写 `anmSelect(2); anmSetSprite(0, 261..264)`、
   令牌写 `anmSetSprite(0, 265..270)` + 一条真令牌子程序。**最忠实**（TH12 原版 UFO 就住在 `enemy.anm`）、
   C 侧零改动、引擎自己管位置/层级；代价 = patch 里多一个被替换的原生 `.anm` + 新管线 + 一条新 ECL 子程序。
   （等价写法：追加进 `effect.anm`（195 起）⇒ `anmSelect(1); anmSetSprite(0, 195..)`。）
2. **卡侧跟随 VM（当前实装）**：ECL 侧把本体刷成隐形载体 —— **`anmSetSprite(0, -1)`**；
   卡侧 `ce_anm_spawn(CE_ABILITY_ANM(), script164.., layer)` 起 VM，每帧把位置抄成敌机 `abs_pos`，
   敌机没了 / 收了 / 卡删了就 `ce_anm_delete()`。令牌同样（它继续用 `NoEnemyDebug` 当惰性载体，
   不需要新 ECL）。零新管线、零新替换的原生文件；代价 = C 侧多一段生命周期管理。
   ⚠️ 风险点 = **孤儿 VM**：`ufo_withdraw_all` / 静默消失 / 卡删 四条路都要把 VM 收干净（`on_delete` 也收）。
3. 给本关 SCPT 头 `anim{}` 加一条自己的 anm ⇒ `anmSelect(3); anmSetSprite(0, S)`。**不可行**：要改七关的
   `.ecl` 头（ECL 已经在我们的 patch 里，但那是改关卡数据，不是本卡该动的东西）。




### 3.2 A8 现在缺的到底是什么（2026-09-12 更正 → **同日已落地**）

原文写「需要『给 anm VM 送中断』这条通路（仓里没有）」—— **这句已经不成立**：
`sdk.h` 早就有 `ce_anm_interrupt(id, n)`（`CE_FN_ANM_INTERRUPT_TREE`）。当时记下的两件「真正缺的」现在都做完了：

1. ~~**拿到「敌机自己的」anm VM id**~~ ✅ 已解决：令牌是 `NoEnemyDebug` 惰性载体，它的 anm 槽在
   `zEnemyData.anm_vm_ids[0]`（= `zEnemyData+0x124`），`ufo.c` 就用这个槽的 VM id 去 `ce_anm_interrupt`。
2. ~~**「给 VM 重指脚本」那一支**~~ ✅ 已解决（等价达成）：SDK 确实**没有**换脚本通路，
   所以换色（13→14→15）用 **`ce_anm_delete` + `ce_anm_spawn(新脚本)`** 实现；
   一手 TH12 是**同一个 VM 原地换脚本**（`0x4061e0` = `AnmVm__wipe` + `AnmLoaded__load_script_into_vm`），
   差别只可能在换色那一帧有一帧叠影/空档 ⇒ 实跑对账时专门看一眼（登记在 `ufo.c`）。

⚠ 送中断的**时机**也与本文件早先的猜测不同：不是「被打掉 / 状态转换 / 收集爆环」，而是
**玩家距离 + 换色计时器**（60 px 圈 `0x4a4410` = `3600.0`；标签 `3` 离开圈且 `<90` 帧 / `2` 计时器 `==90`
或离开圈时 `>=90` / `7` 进入圈那一帧）；「收集爆环」是**另一条 VM**（脚本 `type+0xc9` = 211..216，
标签 7/8/9/10 = 红/蓝/绿/彩虹染色）。


## 4. 音频（全部借现成 SE，统一处理时按 `engine/_shared/th18-sound-table.md` 对一遍）

| 卡 | 常量 | 现值 | 说明 |
| --- | --- | --- | --- |
| 107 | `SENRO_SE_BORDER_OPEN` / `_END` | `0x2f` `se_bonus` | th07 原版是 SE 0x21（自然结束）/ 0x2F（开门），TH18 表不同源 ⇒ 借 |
| 107 | `SENRO_SE_LIBYOU` | `0x09` `se_cancel00` | 灵击音 |
| 109 | `POFV_SE_CHARGE_DING` | `CE_SE_EXTEND` | 升级「叮」 |
| 112 | `UFO_SE_ABSORB/FULL/BREAK/TOKEN` | `0x0d/0x2f/0x38/0x39` | 都是现成 id（注释里写了对应 wav）|
| 112 | `UFO_SE_ITEM_BREAK` | `0x11` | **占位**：TH12 的赏金道具落地有专用音，未查 |
| 113 | `TRANCE_SE_PICK/LEVELUP/ENTER/END/FORCED` | `0x3f/0x44/0x34/0x46/0x2c` | 注释写了数值取自 TH13，但**两作表不同源** ⇒ 只当占位 |
| 106/108/110/111/114–117 | 无自建音（用引擎原生音效）| — | — |
| **117** | `ANIMAL_SE_SPAWN` / `_COLLECT` / `_READY` / `_HYPER` / `_CHANGE` / `_RELEASE` / `_ROAR_END` / `_FISH_ITEMS` | `0x3b` / `0x3f` / `0x4d` / `0x2c` / `0x4e` / `0x3e` / `0x42` / **`0x3e`** | 前 7 个全是一手 TH17 地址（§AL26/§AL41）；**`ROAR_END 0x42` 与 `FISH_ITEMS 0x3e` 由用户耳朵定案**（§AL41 / §AL44，撒道具与放灵同 id）；撒道具试听候选 A `0x34` / B `0x43` 已否、轮换已删 |

## 5. 与本文的配合

- 实装期：**新加的每一个「只有看了才知道」的值** ⇒ 具名常量 + `❓ 自定` + 在本文对应卡那一行补一句。
- 统一处理时：按 G1–G7 → 逐卡 → §3 资产 → §4 音频的顺序过；每改一处，把结论写回对应
  spec / `AUDIT.md`（层与锚点这类**一手结论**尤其要回流，别只留在代码注释里）。

## 6. 117 钱鱼灵撒道具：150 短少的诊断口径（2026-09-13）

- **原生金钱计数没有上限**（`0x446d2e inc [MONEY]` 无钳位），**也不是 150 颗道具池硬上限**
  （普通道具空链表 = `mgr+0x14` 起 600 槽，stride `0xc94`；5 只钱鱼同帧 150 颗 << 600）。
- 已在 `fish_to_items()` 日志里加 **`spawn_ok k/n`**：同一行现有 `inside_ring x/n` 只表示出生瞬间
  在环收圈内的颗数，不是最终收获数。
- 统一实机处理时要定的口径：
  1. 原地吃 5 只钱鱼、等道具全部落回，`spawn_ok` 是否 `30/30`×5、MONEY 是否 `+150`；
  2. 若 `spawn_ok k<n` ⇒ 普通道具池真被其他掉落挤满；改「跨帧分批撒」或「失败队列补撒」，总量仍 30/鱼；
  3. 若 `spawn_ok=n` 但仍 `<150` ⇒ 是下落窗口内自机移动导致的漏收；可调项只有出膛速度 / timer / 基座，
     任何位置挪动都必须整组一起挪（不能逐颗推圈外），以保留 TH17 的椭圆观感。

### §2.4 稀有动物灵（token8..14）：**按不存在处理**（用户 2026-09-13 口径）

> ★ **2026-09-13 用户口径：稀有动物灵（token8..14 / item23–29）「当不存在」** —— 不实现、不做资产、
> 条目也不进列表；中boss 的条件稀有主掉落分支**命中时什么都不生成**（当前只留一行
> `midboss rare conditional … not spawned` 作可证伪痕迹）。⇒ 中boss 实得掉落 =
> **token4 + token5**（st05/06 再 +1 长寿命狼）；Extra 相位边界 = 1 随机长寿命动物 + 2 资源灵。
