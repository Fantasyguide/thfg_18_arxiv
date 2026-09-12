/* 鬼形兽（id 117）—— TH17 动物灵 / 咆哮（HYPER）的 TH18 卡侧复刻（Stage 2）。
 *
 * 一手语义：`engine/animal/th17/01-animal-spirits.md`（TH17 v1.00b 全链）。
 * TH18 里**没有** GoastManager / zToken / HYPER_* 残骸（审计 `engine/animal/th18/01-animal-residuals.md`），
 * 所以全部自建，接缝走 card-expand SDK：
 *
 *   动物灵实体   自己的 VM 池（40 只，TH17 `zGoastManager` 上限 0x28），贴图 = 零售 `beast1.png`
 *               的 13 个 32×32 单元（狼/獭/鹫 各 3 帧 + 4 种鱼灵），脚本 137..143 从 `beast.anm`
 *               script0..6 生成；**DVD 式约 45° 弹跳**（版边 x±180 / y 128..384 翻符号 + 速度下限）。
 *   运动/收集   24 px 吸收（vel = (player−token)×0.25，约 10 帧）；60 px 内减速 ×0.5；
 *               远离时变色（240→180 帧循环 狼→獭→鹫），靠近冻结；长年不理 → 版边消失。
 *   5 格队列    animal_core.c（TH17 TOKENS[5]：多数 ≥3 → 暴走咆哮 + 时长 840/1140/1500；否则混合 1140）。
 *   咆哮        **满槽自动发起**（用户 2026-09-11 定案；一手 TH17 `HYPER_FLAGS` bit1 从 `0x40fb60`
 *               就置上 ⇒ `Player__do_shooting` 立刻换主炮组）：暴走（同类 ≥3）时兽灵专属效果
 *               **就绪期就生效** —— 狼 = 换主炮 shot 组（TH17 `火力档+10`）、鹫 = 卡侧矩形束
 *               （主炮 EAGLE 组留空，见 §AL39）、獭 = 3 颗旋转光球；
 *               时长 840/1140/1500（同类 == 自机凭依再 +480）；收动物灵延长 90/60/42…；
 *               停止射击时窗口只 −1（有存活子弹时 −2）；收鱼灵**立刻**换成道具。
 *   霊撃        **X（炸弹键，TH17 原位置）** 把已在跑的咆哮**用掉** = 40 帧：半径 (1−f⁴)×200 消弹
 *               + 伤害源 + 60 帧无敌（`do_hyper_cancel`）—— 玩家手感 = 「中弹但只丢咆哮」（用户 2026-09-11）。
 *               起霊撃时 TH17 会立刻删掉水獭 3 光球（`th17:0x40f7f7`）；本卡照做。
 *               C（`on_activate`）是同一个动作的 TH18 主动卡版入口；中弹抵消死亡走同一段（`by_hit=1`）。
 *   水獭 3 光球  **咆哮就绪期**（非霊撃期）每帧：VM 留在自机位置，脚本 anchorOffset/angleVel 画轨道；
 *               每颗光球按 TH17 `0x40f200`–`0x40f372` 各做一次半径 28 消弹 + 伤害 2 的源。
 *   中弹不扣命  on_death_before_deathbomb：就绪且未霊撃 → 开霊撃 + cancel_impending_death。
 *   正常结束    超时未霊撃 → 队列里的鱼灵换道具 + 自机附近放出 **2 个变色动物灵**；
 *               霊撃结束（C 或中弹）→ 鱼灵照换、**不放**那 2 个（用户 2026-09-10 定案）。
 *   Miss        真死（on_death_frame2）→ 掉 2 个**不变色**的动物灵（与自机兽灵同种）。
 *   消弹契约    本卡有 **3 处**全屏/圈消弹（鹫束 5 圆、水獭 3 光球、霊撃半径圈），三处都先
 *               `animal_cancel_claim()` 认领「本帧已消过弹」闩锁（`EVENTS-CONTRACT.md` §5.2 细则 2）：
 *               同帧 107 / 111 已先动手就让位（这一帧不消弹、消 0 颗、如实记一条日志），
 *               但状态机 / 伤害源 / 视觉 VM 一概照旧 —— **单装 117 时行为与改动前逐字节相同**。
 *   HUD         左下角 5 格灵槽（槽框 = 零售 `beastf.png` sprite34 切图 + 图标 = 槽内 token 的贴图）。
 *
 * ⚠️ 与 TH17 的已知差异（Stage 2 边界，见 CARDS.md / NEXT.md §1v）：
 *   ① ~~不做「同类 == 凭依兽灵 → +480」~~ → **2026-09-10 二次定案已接**（凭依 = 自机序号 %3）；
 *      不做稀有动物灵（8..14）；
 *   ② ~~暴走咆哮的射击强化未做~~ → **2026-09-11 已接**：狼/鹫 = **fire-time set index** 重定向
 *      （TH17 `火力档 + 10/+15`；不覆盖零售偏移槽 0..9）。数据 = `assets/sht/append_shooterset.py`
 *      追进的 TH17 WOLF set11..14；**pl01 EAGLE 主炮组留空**，视觉/伤害/消弹改由 `eagle_beam_tick()`
 *      的卡侧矩形束负责（`ability.anm` script163 = TH17 pl01.anm entry0 sprite29），不依赖
 *      火力档与 option（见 §AL39）；獭的 3 光球已建 VM（视觉）并在咆哮就绪期做消弹
 *      （霊撃起手时照 TH17 删掉）；
 *   ③ 灵槽几何（x=300+36i / y=924，表面 px）是从 TH17 脚本 pos(-360+36i, 872) 估的，🟡 待实跑调；
 *   ④ 「对话中不变色」按「照常变色、但不移动/不消失」实现（用户口径）。 */
#include <string.h>

#include "sdk.h"
#include "animal_core.h"

/* 本卡 id。⚠ `CE_CARD` 对 ID 做 `##` 拼接 ⇒ 那里必须写**字面量** `117`（照 mof.c / signal.c 的规矩），
 * 这个常量只给下面 `animal_cancel_claim` 的消弹闩锁用（EVENTS-CONTRACT §5.2 细则 2）。*/
#define ANIMAL_ID_NUM 117

/* —— 音效（一手：TH17 §4/§5 的 SoundManager 调用）—— */
#define ANIMAL_SE_SPAWN   0x3b   /* 生成：TH17 `spawn_token` `th17:0x41055d` push 0x3b（稀有动物走 `th17:0x410547` 的 0x4a）*/
#define ANIMAL_SE_COLLECT 0x3f   /* 收集（`0x410b0e`）*/
#define ANIMAL_SE_READY   0x4d   /* 就绪（`0x40fb60`）*/
#define ANIMAL_SE_HYPER   0x2c   /* 霊撃（`0x40f6d0`）*/
#define ANIMAL_SE_CHANGE  0x4e   /* 变色（`0x410c35` 段）*/
#define ANIMAL_SE_RELEASE 0x3e   /* 正常结束放灵 */

/* —— 实体参数（TH17 §4.2/§4.3 的一手常量）—— */
#define ANIMAL_TOKEN_MAX      40      /* TH17 `zGoastManager+0x10` 上限 0x28 */
#define ANIMAL_COLLECT_R2     576.0f  /* 24 px */
#define ANIMAL_TOKEN_GRACE      10    /* ★ 刚出生的灵 10 帧内不可收（用户 2026-09-10：
                                       *   真死掉落曾在中弹点被当场收走；铁律 3 第②问的「无敌时间」）*/
/* ★ 爆散基座（一手 TH17 `0x40e920`：在 **(自机.x, 自机.y − 64)** 爆散，散开靠每颗自己的随机初速）。
 *   Miss 的真死掉落与「咆哮正常结束」的释放**共用这个基座**（用户 2026-09-10：Miss 的灵是
 *   「中弹点正前方的一个位置掉落」）。*/
#define ANIMAL_BURST_DY       64.0f
/* ★ TH17 释放动画（entry12 `beastap.png` = 脚本 0x2c(44) 父 + 45/46 两个子）：
 *   `0x40f494` 在**咆哮自然结束**时建父 VM（2× 自机位置 = 本卡的表面坐标），复位 `release_timer`；
 *   `release_timer` 到 **120 帧**时 `0x40e920` 才在 (自机.x, 自机.y − 64) 爆散动物灵并播 se 0x3e
 *   —— 用户 2026-09-10：「会出一个 extra beast appeared 的贴图，好像在闪烁，消失之后才出 2 个动物灵」。*/
#define ANIMAL_RELEASE_FRAMES  120
#define ANIMAL_RELEASE_DY     (-128.0f)  /* 父脚本原写法 `pos(0,-128,0)`（位置改由 C 写，偏移照抄）*/
#define ANIMAL_SLOW_R2        3600.0f /* 60 px */
#define ANIMAL_ABSORB         0.25f   /* 吸收速度系数 */
#define ANIMAL_ABSORB_FRAMES  9       /* 吸收后 age > 9 出列 */
#define ANIMAL_SPEED_MIN      0.2f    /* 速度下限：|v| < 0.2 就重掷 ±(0.5+rand×0.3) */
#define ANIMAL_SPEED_FLOOR    0.5f
#define ANIMAL_BOUNCE_X       180.0f
#define ANIMAL_BOUNCE_Y0      128.0f
#define ANIMAL_BOUNCE_Y1      384.0f
#define ANIMAL_OFF_X          208.0f  /* 离屏判定 |x| > 208 */
#define ANIMAL_OFF_DY         240.0f  /* 离屏判定 |y − 224| > 240 */
#define ANIMAL_OFF_FRAMES     119     /* 离屏帧数 > 119 出列 */
#define ANIMAL_DIM_AGE        7800    /* 年龄 7800 → interrupt 9（变暗）*/
#define ANIMAL_BOUNCE_MAX_AGE 8400    /* 年龄 8400 起不再弹跳 → 漂出屏幕 */
#define ANIMAL_LIFE_FAR       240     /* 变色计时：远离时 240 帧 */
#define ANIMAL_LIFE_NEAR      90      /* 靠近且剩余 < 60 → 重置 90 */
#define ANIMAL_LIFE_AFTER     180     /* 变色后重置 180 */
#define ANIMAL_FLICKER_LEAD    60     /* 寿命剩 60 帧时发 interrupt 2（TH17 `0x410c8c` 的 `cmp eax,0x3c`）*/

/* —— 槽（HUD）几何：表面 px。复刻 TH17 `beast.anm` script14–18 的 `pos(-360+36i, 872)`：
 *   · 间距 36 px 照抄；
 *   · 绝对位置锚在**游玩区左下角**——最左一格的 48×48 可见框左边 = 游玩区左边缘 257（表面 px，
 *     与天空璋卡（116）季节槽的左边同一条线，`SEASON_GAUGE_BX 308 − 51 = 257`），
 *     可见框底边 = 924（游玩区底边 928 上方 4 px）⇒ 框心 x0 = 257+24 = 281、y = 924−24 = 900；
 *   · 本 mod 的槽框贴图是 48×48 居中补到 64×64（ORDER 的 WxH 必须是 2 的幂），VM 用 originMode(1)
 *     居中 ⇒ 可见框与框心差 24 px，上面的 281/900 已把它算进去。
 * ★★ 2026-09-12：**用「自带锚点」登记进 HUD 槽位表**（`HUD-LAYOUT.md` §5 早列为待办的那一支；
 *   用户 2026-09-12 裁决「**117 现在的位置是正确的左下角**」⇒ 不挪回货架第一格）。
 *   `ce_hud_slot_claim_pinned_card(117, &blk, ANIMAL_HUD_PIN_X, ANIMAL_HUD_PIN_Y, &slot)`：
 *   登记进表（`ce_hud_slot_get()` 取得到）、**货架不会动它**（别的块也不会为它让位），
 *   关卡纪元照样清 ⇒ 每关开场重新认领（`on_load`）。
 *   锚点 = 整排的**左下角**：(88, 924)。x 就是用户 2026-09-10 按 1280 截图量的 `ANIMAL_HUD_X0`
 *   （整排左移 193 px：281−193 = 88）；y 取**已量过的那条线 924**（`CE_HUD_ANCHOR_Y` 同一档），
 *   slot 的中心 y 由 `+ANIMAL_HUD_DY`（= 904−924 = −20）推回来 ⇒ 逐像素等于迁移前的 `ANIMAL_HUD_Y`。
 *   ⚠️ 实测口径：48×48 的可见框以 904 为心 ⇒ 实占 y **880..928**（比 924 还低 4 px，见 §1.4 的
 *      表）；这里按用户量过的 924 当锚点报账，只差这 4 px 的下沿（不参与任何排布计算）。
 *   块尺寸 = 5 格整排（含 24 px 的格半径余量）：x 88..449、y 880..928 ⇒ 报 **193 × 48**
 *   （与 `HUD-LAYOUT §1.4` 的「动物灵 193×48」一致）。*/
#define ANIMAL_HUD_X0          88.0f   /* ★ 用户 2026-09-10 按 1280 截图量的：整排**左移 193 px**（281−193）*/
#define ANIMAL_HUD_Y          904.0f   /* ★ 同上：**下移 4 px**（900+4）*/
#define ANIMAL_HUD_DX         36.0f
/* ★ 2026-09-12：pinned 锚点 = 整排左下角（表面 px）。x/y 一律由它推：
 *   第 i 格中心 = (pin_x + DX·i, pin_y + ANIMAL_HUD_DY)。*/
#define ANIMAL_HUD_PIN_X      88.0f        /* = ANIMAL_HUD_X0（迁移前的硬编码值，一个像素没动）*/
#define ANIMAL_HUD_PIN_Y     924.0f        /* 已量过的那条底线（= CE_HUD_ANCHOR_Y）*/
#define ANIMAL_HUD_DY        (-20.0f)      /* 格中心 y = pin_y − 20（904 − 924）*/
#define ANIMAL_HUD_BLOCK_W   193.0f        /* 5 格整排：pin_x .. pin_x+361（可见框 88..449）*/
#define ANIMAL_HUD_BLOCK_H    48.0f        /* 可见框高 48；上沿 880（= 924 − 48 + 4，见上面的实测口径注）*/
/* ★ 火焰的锚点补偿（用户 2026-09-10）：
 *   火焰贴图 = 48×48 单元放在 64×64 画布的**下沿**（脚本 156 里 `anchor(0, 2)` = 锚心 + 底边），
 *   这样 1.5 倍放大时和 TH17 一样**往上长**。C 侧要按 (32, 56) 补一下，1 倍时的画面才和原来一致。
 *   alpha：**填了没满** = 0.3 档（用户实测 alpha<0.3）；**就绪 / 咆哮中** = 拉满 255（并放大 1.5）。*/
/* ★ 2026-09-10 二次纠偏：**精灵 VM 的默认 anchor 是「锚心」**（不是左上角 —— 左上角那个结论只在
 *   `drawRect` 那类**矩形**图元上成立，就绪条那边踩的就是矩形）。所以火焰在改锚点前是「图心 = 槽心」。
 *   现在脚本 156 用 `anchor(0,2)`（锚心 + 底边）+ 贴图在画布里底对齐 ⇒ 「图底边中点」= VM 位置，
 *   要维持「图心 = 槽心」就得把 VM 位置放到 槽心 + (0, 24)（24 = 48/2）。如此 1 倍画面不变、
 *   1.5 倍时以图底边中点为心**往上长**。*/
#define ANIMAL_FLAME_ANCHOR_DX   0.0f
#define ANIMAL_FLAME_ANCHOR_DY  24.0f
/* ★ 填了但没满时的 alpha：**不是估的**，取 TH17 script24 `interruptLabel(27)` 的原值 ——
 *   那一条正是「1.0 倍」那一档：`scale(1.0f,1.0f); iset($I0,128); alpha($I0); color(255,255,255)` ⇒ **128**。
 *   （另一档 `interruptLabel(3)` 会淡到 `$I0/4` = 63 ≈ 0.25；若实机觉得 128 太亮就换 63。）*/
#define ANIMAL_FLAME_ALPHA_LOW  128
#define ANIMAL_HUD_DIM        0xc0ffffffu   /* 未就绪：半透明（alpha 192）*/
#define ANIMAL_HUD_FULL       0xffffffffu   /* 就绪：实心 */

/* —— HUD 渐隐（自机贴近左下角时把槽位淡下去）——
 * ★ 用户 2026-09-10 口径：**不对齐 TH17**，改用**天空璋卡（TH16）那一套时间序列**，好让两张系统卡一致：
 *     target = 64（dim，TH16 `alphaTime(5,0,64)`）/ 255，按 `step = 51` 每帧走 ⇒ 约 5 帧走完。
 * 触发条件**暂时照抄**天空璋卡（TH16 `th16:0x427f41` / `0x427f6c` 的闩锁 + 迟滞：`x < −64 且 y > 400`
 * 淡出；`y < 384 或 x > −64` 淡回）。
 * 🟡 **最终版要按「UI 此刻的位置」判**：多张左下角卡要竖直垒起来（`SDK.md` §17 / `HUD-LAYOUT.md`），
 *    那时这里改成「自机 vs HUD 可见矩形」的判据，只动 `animal_hud_player_near()`。*/
#define ANIMAL_HUD_FADE_DIM    64
#define ANIMAL_HUD_FADE_STEP   51
#define ANIMAL_HUD_HIDE_X     (-64.0f)   /* 照抄天空璋卡：自机 x < 它… */
#define ANIMAL_HUD_HIDE_Y     400.0f     /* …且 y > 它 → 淡出 */
#define ANIMAL_HUD_SHOW_Y     384.0f     /* 迟滞：y < 它（或 x > HIDE_X）→ 淡回 */

/* ★ 调试键（2026-09-10，用户要求）：拍**空格**时，如果本卡在卡组里，就从画面中央
 * (ECL 坐标 x=0, y=224) 召唤一只随机**动物灵**（type 1..3，长寿命/变色版）。
 * `ce_key_down` 走 `GetKeyboardState`（user32）—— 空格不在游戏配置里、也不进 replay 输入流，
 * 所以只作调试用。发布前把 ANIMAL_DEBUG_KEY 改成 0 即可关掉。*/
#define ANIMAL_DEBUG_KEY      1
/* ★ 调试键出什么（用户口径，随时改这两行）：
 *   2026-09-10 第一版：只出**资源灵**（token 4..7）→ 测鱼灵换道具；
 *   2026-09-10 第二版：只出**狼灵**（token 1）→ 稳定进狼暴走；
 *   2026-09-11 第三版（现行，用户口径）：**随机 1..3**（狼 / 獭 / 鹫，长寿命变色版）——
 *   一拍空格随机给一只兽灵，方便一次跑遍三种暴走（含獭的 3 光球、鹫的激光）。
 *   日志仍是一行 `DEBUG spawn animal type N at (0, 224) by SPACE`（N 就是这只的类型）。
 *   Shift+空格仍是资源灵 4..7 循环、Ctrl+空格仍是强制就绪。*/
#define ANIMAL_DEBUG_TYPE_MIN 1
#define ANIMAL_DEBUG_TYPE_MAX 3
#define ANIMAL_DEBUG_FISH_MIN 4    /* Shift+空格：资源灵 4..7 循环（别跟上面的 MIN/MAX 混用）*/
#define ANIMAL_DEBUG_FISH_MAX 7
#define ANIMAL_DEBUG_X        0.0f
#define ANIMAL_DEBUG_Y        224.0f

/* —— 咆哮**就绪窗口**条（TH17 `beast.anm` script30/31 = 0x1e/0x1f）——
 * TH17：`AnimalManager__on_ready` `th17:0x40fb60`（`0x40fc6a`/`0x40fc74`）建两条 UI VM 存
 * `manager+0x28/+0x2c`；之后每帧（`0x40f0c6` 段）把两条 VM 的坐标写成**自机坐标 ×2**，
 * 剩余时间写进 scale.x（同一族每帧刷 VM 字段）⇒ 条从**两端朝中点**收缩；
 * `do_hyper_cancel` `th17:0x40f6d0` 在霊撃起手时清掉 ⇒ 只在「槽满、还没按 C」的窗口里显示。
 * 表面 px 换算：`表面 = (640 + x×2, 32 + y×2)`（`engine/anm/th18/01-vm-instantiate.md` §3）。
 * 三条 VM 的坐标全由本卡每帧写（脚本里不写 `pos()`）。*/
#define ANIMAL_BAR_PER_FRAME   0.125f /* ★ 长度 = 帧数 × 这个常量：TH17 `th17:0x410180` 读 `th17:0x4a3af4` = 0.125 */
#define ANIMAL_BAR_H          10.0f   /* 外框高（表面 px）—— 照 TH17 `drawRectBorder(1.0f, 10.0f)` */
#define ANIMAL_BAR_PAD         1.0f   /* 边框宽（TH17 那条 1 单位描边；外框长 = 填充满长 + 2×它）*/
#define ANIMAL_BAR_DX        193.0f   /* ★ 用户 2026-09-10 按 1280 截图量的：整条**左移 193 px** */
#define ANIMAL_BAR_DY        (-54.0f) /* ★ 同上：整条**下移 94 px**（原 −40 ⇒ −40+94 = +54 ⇒ 条心在自机**下方** 54 px）*/
#define ANIMAL_BAR_GROW        1.0f   /* ★ 用户：框的**下边界再下 1 px、右边界再右 1 px**（左/上不动）*/
#define ANIMAL_BAR_FILL_DY     1.0f   /* ★ 用户：表示进度的条**再下移 1 px** */

/* —— 咆哮期间的 fire-time set index（TH17 `Player__do_shooting` `th17:0x44a710`）——
 * TH17：**狼暴走 → idx = 火力档 + 10**、**鹫暴走 → idx = 火力档 + 15**（聚焦与否都一样，獭/混合不换），
 * 这两组是 TH17 `sht` 里**本来就有的高索引**，不是往 0..9 覆盖。TH18 的 `pl0X.sht` 没有那两组
 * ⇒ 本卡把 TH17 `set 11..14`（狼）/`16..19`（鹫）转成 TH18 记录、追加进**未使用的 0x1e..0x27**
 * （`assets/sht/append_shooterset.py`），再在 fire-time 只改引擎选中的 index。零售 0..9 一个字节不动，
 * 所以咆哮前已出的弹命中时按旧 set 回读，仍解析到零售 row（不会变自机贴图）。
 *
 * ★ 引擎侧：`Player__tick_shooting_state` `th18:0x45ea00` 算完 idx（`0x45ebd1 idiv`、聚焦 `0x45ebf5`）
 *   后在 `0x45ebfb` 查 `sht[0xe0 + idx*4]`；同一 idx 还在 `0x45ebed`/`0x45ebf8` 存进 `[ebp-8]`，
 *   由 `0x45ec69` 打包成 `(set<<8)|row` 写进 `PlayerBullet+0xac`。SDK 断点 `BP_ce_shot_set_index`
 *   在 0x45ebfb 同时改写 edx 与 `[ebp-8]`，弹里也记 beast set。
 *
 * ★ 2026-09-11 九修（AUDIT §AL39）：pl01 EAGLE 的主炮 shooter 行**全部留空**。
 *   用户实跑确认 TH17 `mode=2` 在主炮路径下会变成「带激光贴图的移动 sprite-bullet」：
 *   按住 Z 进咆哮出的是会飞的贴图弹；松开 Z 进咆哮则因为 `on_shot_set_index` 只在开火时
 *   被问而根本没有弹。按用户已批准 end state，改用卡侧矩形束：
 *     · `assets/sht/append_shooterset.py` 给 pl01 的 EAGLE 0x23..0x27 写空组（只有终止符）；
 *     · `on_shot_set_index` 在 ready/hypering 时仍返回 `EAGLE_0 + power`，但目标组为空 ⇒
 *       引擎不创建 bullet，也就不会再有 sprite-bullet；
 *     · `eagle_beam_tick()` 在 Z (`CE_INPUT_HELD & 1`) 按住时每帧挂/刷新一条矩形束：
 *       视觉用本 mod `ability.anm` 的新脚本 163（TH17 `pl01.anm` entry0 sprite29 的 512×32
 *       激光条，来源贴图与 TH17 抽取物逐字节一致），伤害/消弹用 `ce_damage_rect` +
 *       沿束多点 `ce_cancel_radius`。松开 Z / 咆哮结束立即收掉。
 *   这样 (a)(b)(c) 三条保证都不依赖火力档、不依赖 option。*/
#define ANIMAL_SHT_POWER_N     5

/* —— EAGLE 卡侧矩形束（用户 2026-09-11 批准的 end state）——
 * TH17 `pl01.anm` entry0 `sprite29` = (0,288,512,32) 激光条；C 把它当成
 * anchor(1,0) + rotate(-90°) 的束（与青眼 script80 同款画法），从自机位置向上拉出 512 px。
 * 判定：宽 34（TH17 EAGLE hit_h=34），长度固定 512；伤害矩形 angle=0、中心在束中点。
 * 消弹：引擎只有圆形 `ce_cancel_radius`，所以在束轴上取 5 个重叠圆近似矩形柱。*/
#define ANIMAL_EAGLE_BEAM_LEN        512.0f
#define ANIMAL_EAGLE_BEAM_WIDTH      34.0f
#define ANIMAL_EAGLE_BEAM_SPRITE_W   512.0f
#define ANIMAL_EAGLE_BEAM_DAMAGE     20
#define ANIMAL_EAGLE_BEAM_CAP        300
#define ANIMAL_EAGLE_BEAM_CANCEL_R   20.0f
#define ANIMAL_EAGLE_BEAM_CANCEL_N   5
#define ANIMAL_EAGLE_BEAM_CANCEL_MAX 9999
#define ANIMAL_EAGLE_BEAM_CANCEL_MODE 1

/* 槽位号由 `assets/sht/append_shooterset.py` 分配并写进 `native/sht_ids.h`
 * （WOLF 0x1e..0x22 / EAGLE 0x23..0x27，紧跟在 PEA/SEASON 的 0x17..0x1d 之后）。
 * **不写兜底值**：头文件没生成就该编译不过，免得默默指到别人的组上。*/
_Static_assert(CE_SHT_SET_WOLF_4 == CE_SHT_SET_WOLF_0 + 4, "狼 5 组必须连续");
_Static_assert(CE_SHT_SET_EAGLE_4 == CE_SHT_SET_EAGLE_0 + 4, "鹫 5 组必须连续");

/* —— 鱼灵换道具（TH17 `th17:0x410640`）——
 * TH17 一次撒 **30 颗**、位置 = (自机.x, 自机.y−80) **+ (cos(a)×i, sin(a)×i/2)**，i = 0,2,…,58
 * （`th17:0x41071d`–`0x4107cd`：每轮 i += 2.0、timer += 2，`esi=10` 起、`esi<0x46` 止）
 * —— 越靠后撒得越开、角度 a = `Rng__rand_float(min=i, max=i×0.5) × π`（`th17:0x402880` + `0x4a3c5c`）。
 * 每颗的**出膛角固定 −π/2、速度 2.2**（`th17:0x410790`），只有位置随机。*/
#define ANIMAL_ITEM_R_MAX      58.0f   /* i 的上界（= 最外层椭圆的长半轴，px）*/
#define ANIMAL_P_ITEM_N        30      /* 火力鱼 / 金钱鱼一轮撒几颗（TH17 是 30；金钱卡是 +50 金/张，改小）*/
#define ANIMAL_MONEY_CARD_N     1      /* 金钱卡的张数（**备用**：当前金钱鱼走 30 颗金钱道具）*/
#define ANIMAL_ITEM_T_POWER      1     /* TH18 道具 type 1 = 火力（+1）—— 这个是真道具 */
/* ★ 碎片/金钱一律用 TH18 的**一次性卡道具**（`th18:0x4b4020`：type 0x10–0x13 行第一 dword = card_id）：
 *   type 4（命碎）/ 6（符卡碎）那类**是别的作的遗留槽位**，虹龙洞原生只用卡道具掉碎片
 *   —— 辉针城卡（id 114）2026-09-09 实跑已经踩过同一个坑并纠偏：`cards/collect2x.c:180`。
 *   这四张卡的贴图是「道具图 abitem.png」那种卡，不是「宝石 / 点」形状。*/
#define ANIMAL_ITEM_T_FISH_LIFE  0x10  /* EXTEND2 卡道具 → CardLifeFragment：残机碎片 */
#define ANIMAL_ITEM_T_FISH_BOMB  0x11  /* BOMB2 卡道具  → CardBombFragment：符卡碎片 */
#define ANIMAL_ITEM_T_MONEY       2    /* ★ TH18 type 2 = 金钱（`MONEY += 1`，金色方点）—— 用户 2026-09-10
                                        *   二次定案：金钱鱼回退成 TH17 的 **30 颗金钱道具**，不走金钱卡。*/
#define ANIMAL_ITEM_T_MONEY_CARD 0x12  /* PENDULUM 卡道具 → +50 金（**备用**，当前不再产出）*/
#define ANIMAL_ITEM_T_POWER_CARD 0x13  /* DANGO 卡道具 → +0.50 火力（本卡暂不用）*/

/* —— 出生点安全区（★ 铁律，见 CARD-CHECKLIST §0 铁律 3）——
 * TH18 的**环收**判定 `th18:0x4465da`：自机到道具的距离² 比 `player+0x47990`（聚焦）/`+0x47994`（非聚焦）²，
 * 进了就把 state 翻 4 开始吸附 ⇒ **出生在圈里 = 一出生就飞走**（TH17 的鱼灵换道具基座 y−80、
 * 环收半径 70 ⇒ 正好在圈外 10 单位；本 mod 的道具回收范围卡（21）把半径抬到 110 ⇒ 就落进圈里了）。
 * 另外 `spawn` 第 6 参 = **intro 倒计时**（`item+0xc84`，`th18:0x445b78` 的 `jg 0x4467e5` 直接把
 * state-1 整段跳过）⇒ 那段时间不动也不能收；TH17 鱼灵换道具传的是 10,12,…,68，本卡照抄。*/
#define ANIMAL_PICKUP_MARGIN   12.0f  /* 出生点至少留到「环收半径 + 这么多」之外（ECL 单位）*/
#define ANIMAL_FISH_CARD_SPEED  1.2f  /* 碎片卡的短弧（照 collect2x 实跑口径）*/
#define ANIMAL_FISH_ITEM_SPEED  2.2f  /* P / 金钱：原生 main-drop 速度*/
#define ANIMAL_PI             3.14159265358979f

#define ANIMAL_FN_CANCEL_IMPENDING_DEATH 0x45d590

typedef struct {
    int32_t id;          /* anm VM id；0 = 空槽 */
    int32_t type;        /* 1..3 基本动物 / 4..7 鱼灵 */
    int32_t long_life;   /* 1 = 变色动物灵（boss 掉的长寿命变体 15..17）*/
    float   x, y, z;
    float   vx, vy, vz;
    int32_t age;         /* 出生帧数 */
    int32_t life;        /* 变色计时（只对 long_life）*/
    int32_t off;         /* 离屏帧数 */
    int32_t collecting;  /* 1 = 已被吸收、朝自机飞 */
    int32_t near;        /* 1 = 在 60 px 内（变色计时冻结）*/
    int32_t dimmed;      /* 1 = 已发过 interrupt 9 */
    int32_t flicker_logged; /* 1 = 本次寿命周期已打过 pre-switch flicker 日志 */
} animal_token_t;

static animal_token_t s_tok[ANIMAL_TOKEN_MAX];
static int32_t s_n_tok;

typedef struct {
    ce_card_t *card;
    animal_core_t core;
    int32_t frame;
    int32_t kill_count;
    int32_t hud_soul[ANIMAL_SLOT_MAX];
    int32_t hud_flame[ANIMAL_SLOT_MAX];
    int32_t hud_soul_delay[ANIMAL_SLOT_MAX];   /* 空槽灵魂的相位错开（创建前等几帧）*/
    int32_t hud_icon[ANIMAL_SLOT_MAX];
    int32_t dbg_key_prev;            /* 调试键上一帧（边沿检测）*/
    int32_t dbg_type;                /* 调试键：Ctrl+空格强制满槽用的类型游标 1→2→3 */
    float   death_x, death_y;        /* 中弹那一刻的自机坐标（`on_death_before_deathbomb` 存，真死掉落用）*/
    int32_t death_valid;             /* 上面两个数还有效吗 */
    int32_t tok_grace_unused;        /* 占位：保持结构对齐（未用）*/
    int32_t hud_dim;                 /* 1 = 自机贴近 HUD、槽位淡出中 */
    int32_t hud_alpha;               /* HUD 当前 alpha（255 ↔ 64，每帧 ±51，照天空璋卡）*/
    int32_t hud_fade_touched;        /* 1 = 上一帧动过灵魂/火焰的 alpha（淡回时要补写一次）*/
    int32_t hud_slot_ok;             /* ★ 2026-09-12：本关已认领到 pinned HUD 槽（`s_slot` 在静态区）*/
    int32_t phase_sig;               /* boss 活动中断槽的相位签名（TH17 `BossItem(33)` 的边界信号）*/
    int32_t phase_is_spell;          /* 当前 phase_sig 对应的相位是否为符卡（SPELLCARD+0x78 bit0 在本相位期间亮过）*/
    int32_t phase_is_mid;            /* 当前 phase_sig 是否属于道中 boss（新中断槽名首字母 'M'，照 AUDIT §AL）*/
    int32_t phase_drop_seen;         /* 本相位已有 TH18 `dropItems` 事件命中；on_tick_2 边界不再重复发 */
    int32_t death_dropped;           /* 本次 boss 死亡的 2 资源灵已由 on_enemy_drop_pre 发出 */
    int32_t boss_seen;               /* 正在跟踪一个 boss（用于无 drop 事件时的死亡兜底）*/
    int32_t boss_dead_sub;           /* 当前 boss 的 ECL 子程序名含 "Dead"（BossDead / MBossDead）*/
    float   boss_x, boss_y;          /* 最后看到的 boss 坐标（死亡兜底掉落位置）*/
    int32_t bomb_gate_logged;        /* 本轮咆哮是否已打印过 bomb suppressed 日志 */
    int32_t orb_spawn_logged;        /* 本轮水獭咆哮是否已打印 3 个 orb VM id */
    int32_t orb_log_last;            /* 上次打印 orb 位置日志的 s->frame，避免刷屏 */
    int32_t aura_id;                 /* 咆哮期间的**类型**光环 VM（脚本 146/147/148，64×64 跟随自机）*/
    int32_t aura_type;               /* 本次咆哮用来上色的兽灵 1/2/3（混合时按数量选，见 pick_mixed_aura）*/
    int32_t aura_rainbow;            /* ★ 彩蛋：队列里**一个动物灵都没有**时，光环**每帧随机换色** */
    int32_t aura_anim;               /* 彩蛋用：光环动画帧计数器（脚本 146–148 是 8 张 × 每张 3 帧）*/
    int32_t self_aura_id;            /* 咆哮期间的**全屏渐变圆** VM（TH17 script32 = 脚本 155，layer 6 在弹幕区背后）*/
    int32_t strike_id;               /* 霊撃三色光圈父 VM（脚本 150，自删）*/
    int32_t bar_edge[4];             /* 咆哮就绪条的**外框** = 4 条 1px 矩形（上/下/左/右，脚本 157 = TH17 0x1e）*/
    int32_t bar_fill;                /* 咆哮就绪条的**填充** VM（脚本 158 = TH17 0x1f）*/
    int32_t release_anim;            /* 咆哮**自然结束**的释放动画父 VM（脚本 160 = TH17 script44）*/
    int32_t release_wait;            /* 释放动画起算的等待帧（TH17 `release_timer` 到 120 帧才爆散）*/
    int32_t anm_context_note;        /* 咆哮中修过 player+0x10 串位的日志只打一次（§AL37）*/
    int32_t shot_set_logged;         /* 本次咆哮已打过 `shot set -> wolf/eagle` */
    int32_t shot_index_logged;       /* 本次咆哮已打过 fire-time index 覆盖日志 */
    int32_t shot_index_req_logs;     /* 本次咆哮已打印的 fire-time 请求条数（前 5 条）*/
    int32_t shot_index_none_logged;  /* 本次咆哮已打印「30 帧内没人来问 set index」*/
    int32_t roar_start_frame;        /* 本次咆哮 on_ready 的 s->frame（用于 30 帧超时日志）*/
    int32_t beam_id;                 /* EAGLE 卡侧矩形束的可视 VM（脚本 163）*/
    int32_t beam_logged;             /* 本次激活已打印束 on 日志 */
    int32_t se_left;                 /* 撒一批道具时还要补播几声 SE（2 帧一声，模拟「每颗各响一声」）*/
    int32_t se_gap;                  /* 距下一声还有几帧 */
    int32_t se_probe_left;           /* ★ 调试：SE 试听序列还剩几个候选 */
    int32_t se_probe_idx;            /* ★ 调试：下一个候选在表里的下标 */
    int32_t se_probe_gap;            /* ★ 调试：距下一声还有几帧 */
    int32_t orb[3];                  /* 水獭暴走的 3 个旋转消弹体 VM（脚本 149）*/
    int32_t cancel_yield_logged;     /* ★ 本卡消弹「让位」日志已打过（EVENTS-CONTRACT §5.2 细则 2）：
                                      * 重新认领到闩锁就清 0 ⇒ 同一次让位只打一条，不每帧刷屏 */
    int32_t drops, hypers, saves, ends;
} animal_card_t;

_Static_assert(sizeof(animal_card_t) <= 496, "ce_state 上限 496 字节");

static animal_card_t *st(ce_card_t *c) { animal_card_t *s = ce_state(c, animal_card_t); if (s) s->card = c; return s; }

/* ============================ ★ 全屏消弹的「本帧第一家动手」闩锁 ============================
 * 契约：`EVENTS-CONTRACT.md` §5.2 细则 2 —— 同帧多张卡都要全屏消弹时**只有第一家动手**
 * （107 / 111 已按此认领），第二家**让位**：跳过消弹，但自己的状态机（计时器 / 标志 / 伤害源 /
 * 视觉 VM）一概照旧，并且**不许**改成小半径或别的替代动作。
 *
 * 返回 1 = 本帧这几处消弹照旧打出去；0 = 让位（这一处整帧不消弹 ⇒ 本帧消 0 颗）。
 * ⚠ **本卡同帧本来就有多处消弹点**：type 3（鹫）的霊撃期 `hyper_frame`（自机位置那个圈）与
 *   `eagle_beam_tick`（沿束 5 个圆）会在**同一帧**都跑 —— 那是同一张卡的一件事，
 *   不是「两家抢账」。所以「本帧的认领者已经是 **117 自己**」也算照旧消弹
 *   ⇒ 单装 117 时行为与改动前**逐字节相同**（只有别的卡先认领了才会走到让位分支）。*/
static int animal_cancel_claim(animal_card_t *s)
{
    if (ce_cancel_latch_claim((uint32_t)ANIMAL_ID_NUM)) { s->cancel_yield_logged = 0; return 1; }
    return ce_cancel_latch_owner() == (int)ANIMAL_ID_NUM;
}

/* 让位时如实记一笔「本处这一帧消 0 颗」（别把别人的颗数算到自己头上、也别让日志说谎）。
 * 本卡三处消弹**都没有把颗数接进任何账**（`ce_cancel_radius` 的返回值原本就是 `(void)` 丢弃）
 * ⇒ 让位不改任何计数器 / 日志数值，只是不在这一帧动手。同一次让位只打一条。*/
static void animal_cancel_yield_log(animal_card_t *s, const char *where)
{
    if (s->cancel_yield_logged) return;
    s->cancel_yield_logged = 1;
    ce_log("wba: %s 全屏消弹让位（本帧已由卡 %d 认领，本处消 0 颗）—— EVENTS-CONTRACT §5.2 细则 2",
           where, ce_cancel_latch_owner());
}

/* —— 小工具 —— */

/* ★ 主凭依灵（TH17 每个自机固定绑定的兽灵，如霊夢A = オオカミ霊）：
 *   TH18 **没有**凭依选择 ⇒ 用户 2026-09-11 定案按「主凭依灵 ≠ 本凭依灵」做（任何暴走咆哮都不吃 +480）。
 *   彩蛋：**卡组里带着**「弱肉强食之理」(id 25, 早鬼) 或「鬼杰组长的恐吓」(id 30, 袿姬) 时，
 *   视为主凭依灵**恒存在** ⇒ 触发的任何暴走咆哮都按「本凭依灵的暴走咆哮」算（吃 +480）。
 *   「带着」= 在本局卡组链表里（`mgr+0x18` 哨兵 / `+0x1c` 首结点，读法照 sdk.c 的 card_in_list）。*/
static int32_t goast_for_deck(void)
{
    uint8_t *mgr = CE_ABILITY_MGR();
    const uint8_t *head, *node;
    unsigned guard = 0;

    if (!mgr) return ANIMAL_GOAST_NONE;
    head = mgr + CE_MGR_CARD_LIST_HEAD;
    node = *(const uint8_t *const *)(head + CE_NODE_NEXT);
    for (; node && node != head && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        const uint8_t *card = *(const uint8_t *const *)(node + CE_NODE_CARD);
        int32_t id;
        if (!card) continue;
        id = *(const int32_t *)(card + CE_CARD_ID);
        if (id == 25 || id == 30) return ANIMAL_GOAST_ANY;
    }
    return ANIMAL_GOAST_NONE;
}

/* ★ 「咆哮中」= **满槽就绪**（`ready`）或 40 帧霊撃（`hypering`）。
 * 一手 TH17：兽灵专属效果（狼/鹫换主炮组、獭 3 光球）绑的是 `HYPER_FLAGS` bit1 = **咆哮中**，
 * 而 bit1 在 `FUN_0040fb60`（满 5 格**就绪那一刻**）就置上（`th17:0x40fc8a or 2`；`Player__do_shooting`
 * 的索引公式读的就是它，见 `engine/sht/th17/01` §4）—— 即**咆哮是满槽自动生效的**，不等玩家按键。
 * 用户 2026-09-11 口径与此一致：「满槽自动发动咆哮，三动物灵额外效果」；
 * 40 帧霊撃（炸弹键 / 中弹取消）只是「把咆哮用掉」，是另一件事。*/
static int roaring(animal_card_t *s) { return s->core.ready || s->core.hypering; }

static int32_t script_for_type(int32_t type)
{
    switch (type) {
    case 1: return CE_ANM_ABILITY_SCRIPT_ANIMAL_WOLF;
    case 2: return CE_ANM_ABILITY_SCRIPT_ANIMAL_OTTER;
    case 3: return CE_ANM_ABILITY_SCRIPT_ANIMAL_EAGLE;
    case 4: return CE_ANM_ABILITY_SCRIPT_FISH_BOMB;
    case 5: return CE_ANM_ABILITY_SCRIPT_FISH_LIFE;
    case 6: return CE_ANM_ABILITY_SCRIPT_FISH_POWER;
    default: return CE_ANM_ABILITY_SCRIPT_FISH_POINT;
    }
}

/* 槽内图标用的 sprite（用户 2026-09-10：鱼灵槽内要改成**本作**的道具样子）：
 *   动物灵 = 本卡的 32×32 token 图（狼/獭/鹫）；
 *   鱼灵 = TH18 自己的道具字体：4(B碎)→ITEM_BOMB、5(残碎)→ITEM_LIFE、6(P)→ITEM_P、7(蓝点)→ITEM_MONEY。
 *   `ITEM_*` 来自 `ITEM_TYPE_TABLE_sprite_and_card` `th18:0x4b4020`（stride 8）的 sprite 号
 *   （type1=112 / type2=113 / type4=115 / type6=117），切自 `bullet1.png` y=32 一行。*/
static int32_t sprite_for_type(int32_t type)
{
    switch (type) {
    case 1: return CE_ANM_ABILITY_SPRITE_ICON_WOLF;
    case 2: return CE_ANM_ABILITY_SPRITE_ICON_OTTER;
    case 3: return CE_ANM_ABILITY_SPRITE_ICON_EAGLE;
    /* ★ 2026-09-10 二次定案（用户）：槽里**回退**成 TH17 的鱼形槽位图（`beastf.png` 的 `ICON_FISH_*`）——
     *   之前那版「槽里显示咆哮结束后拿到的资源卡卡面」作废（卡面贴图留在 ORDER 里备用，不再引用）。
     *   金钱鱼自己那张槽内图等用户 P 好金版再换（`ICON_FISH_MONEY`）。*/
    case 4: return CE_ANM_ABILITY_SPRITE_ICON_FISH_BOMB;
    case 5: return CE_ANM_ABILITY_SPRITE_ICON_FISH_LIFE;
    case 6: return CE_ANM_ABILITY_SPRITE_ICON_FISH_POWER;
    default: return CE_ANM_ABILITY_SPRITE_ICON_FISH_MONEY;   /* ★ 用户 2026-09-10 手绘的金版（蓝点鱼改金）*/
    }
}

static void cancel_impending_death(void)
{
    void *p = CE_PLAYER();
    if (p) ((void (__attribute__((thiscall)) *)(void *))ANIMAL_FN_CANCEL_IMPENDING_DEATH)(p);
}

static void set_invuln(int32_t frames)
{
    ce_timer_t *t = CE_PLAYER() ? CE_PLAYER_INVULN() : 0;
    if (!t) return;
    if (t->cur < frames) { t->prev = t->cur; t->cur = frames; t->cur_f = (float)frames; }
}

static int dialogue_open(void)
{
    uint8_t *gui = CE_GUI();
    return gui && *(int32_t *)(gui + CE_GUI_MSG) != 0;
}

/* 自机弹池里还有活弹吗（TH17 `HYPER_FLAGS` bit10；有 → 咆哮窗口每帧多 −1）。 */
static int player_has_live_bullets(void)
{
    uint8_t *p = CE_PLAYER();
    int i;
    if (!p) return 0;
    for (i = 0; i < CE_PB_COUNT; ++i) {
        if (*(int32_t *)(p + CE_PB_POOL + i * CE_PB_STRIDE + CE_PB_STATE) == 1) return 1;
    }
    return 0;
}

static int enemy_is_boss(const uint8_t *e)
{
    uint8_t *em = CE_ENEMY_MGR();
    int32_t id;
    int i;
    if (!e || !em) return 0;
    id = *(const int32_t *)(e + CE_ENEMY_ID);
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i)
        if (*(const int32_t *)(em + CE_EM_BOSS_IDS + i * 4) == id) return 1;
    return 0;
}

/* —— TH17 `BossItem` 的自然掉落对齐（2026-09-10）——
 * 一手：`thecl -d 17` 的 `default.ecl` `BossItem` 子程序 + 全 stage ECL 调用普查：
 *   `@BossItem(33,0,0)` ×39 → item33 → rand%3+30 → token 15..17（随机变色动物灵）
 *   `@BossItem(30/32,0,0)` ×10 → 指定狼 / 鹫（长寿命）   `@BossItem(22,0,0)` ×7 → token 7（点鱼灵）
 *   `@BossItem(19,…)` ×21 / `(20,…)` ×15 → token 4 / 5（炸弹碎片灵 / 残机碎片灵）
 *   稀有动物 23..29 在道中 boss 的条件分支里（本卡不做）
 * TH18 侧能对齐的信号：
 *   · 敌人死亡 → `on_enemy_drop_pre` + boss id 表（BossItem 19/20）
 *   · boss 相位 / 符卡边界 → 活动中断槽的 `phase_sig` 变化（照 lolk.c 的一手 `phase_read`；33/30/32/22）
 *   · 杂兵不掉（TH17 杂兵只有 item 1/2/3）  · 道中 boss 的相位边界不掉（槽名首字母 'M' 过滤）*/
#define ANIMAL_ENEMY_LIFE_CUR   0x6220   /* zEnemyData.life.current（lolk.c 同址）*/
#define ANIMAL_SLOT_SUB_LIFE    0x08     /* zEnemyInterrupt.sub_life[0x40]（子程序名）*/
/* ECL 当前子程序名（照 lolk.c 的一手常量）：用来区分真正的 BossDead / MBossDead 死亡
 * 与普通相位结束的 `BossItem` 事件；这样 on_enemy_drop_pre 不会把中间相位当成死亡重复发。*/
#define ANIMAL_ENEMY_ECL_CTX        0x0c     /* zEnemy.ecl.context.current_context */
#define ANIMAL_ECL_CTX_SUB          0x04     /* zEclRunContext.cur_location.subroutine_index */
#define ANIMAL_ENEMY_ECL_FILEMGR    0x1218   /* zEnemy.ecl.file_manager */
#define ANIMAL_ECL_FM_SUB_TABLE     0x8c     /* zEclFileManager.subroutines */
#define ANIMAL_ECL_FM_SUB_COUNT     0x08     /* zEclFileManager 子程序个数 */

static uint8_t *animal_boss_enemy(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    unsigned i;
    if (!em) return 0;
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        int32_t id = *(int32_t *)(em + CE_EM_BOSS_IDS + i * 4);
        uint8_t *e;
        if (!id) continue;
        e = ce_enemy_by_id(em, id);
        if (e) return e;
    }
    return 0;
}

/* 活动中断槽（第一个 hp > -1 && time > 0）→ phase_sig + 是否道中 boss。
 * 返回 0 = 当前没有活动槽（相位之间的空档，调用方保留旧 sig 以免漏掉下一次变化）。*/
static int animal_phase_sig(const uint8_t *e, int32_t *sig, int *is_mid)
{
    const uint8_t *base = e + CE_ENEMY_DATA + CE_ED_INTERRUPTS;
    unsigned i;
    for (i = 0; i < CE_ED_INTERRUPT_SLOTS; ++i) {
        const uint8_t *slot = base + i * CE_ED_INTERRUPT_STRIDE;
        int32_t hp = *(const volatile int32_t *)slot;
        int32_t tm = *(const volatile int32_t *)(slot + 4);
        if (hp > -1 && tm > 0) {
            const char *name = (const char *)(slot + ANIMAL_SLOT_SUB_LIFE);
            *sig = (int32_t)((i + 1) * 1000003u ^ (uint32_t)hp * 31u ^ (uint32_t)tm * 131u ^
                             *(const volatile uint32_t *)(slot + ANIMAL_SLOT_SUB_LIFE));
            *is_mid = (name[0] == 'M') ? 1 : 0;
            return 1;
        }
    }
    return 0;
}

/* 取敌人的 ECL 主上下文当前子程序名；失败返回 0。读法/偏移照 `native/cards/lolk.c` 的
 * `enemy_ecl_sub`（一手：zEnemy+0xc 主上下文、file_manager+0x8c 子程序表每项 8 字节）。 */
static const char *animal_enemy_sub_name(const uint8_t *e, int32_t *out_sub)
{
    uint8_t *ctx, *fm, **subs;
    int32_t sub;

    if (out_sub) *out_sub = -1;
    if (!e) return 0;
    ctx = *(uint8_t **)(e + ANIMAL_ENEMY_ECL_CTX);
    fm  = *(uint8_t **)(e + ANIMAL_ENEMY_ECL_FILEMGR);
    if (!ctx || !fm) return 0;
    sub = *(int32_t *)(ctx + ANIMAL_ECL_CTX_SUB);
    if (sub < 0 || sub >= *(int32_t *)(fm + ANIMAL_ECL_FM_SUB_COUNT)) return 0;
    subs = *(uint8_t ***)(fm + ANIMAL_ECL_FM_SUB_TABLE);
    if (!subs) return 0;
    if (out_sub) *out_sub = sub;
    return (const char *)subs[sub * 2];
}

/* —— 实体池 —— */

static void token_remove(int i)
{
    if (s_tok[i].id) ce_anm_delete((uint32_t)s_tok[i].id);
    s_tok[i] = s_tok[--s_n_tok];
}

static void token_clear_all(void)
{
    int i;
    for (i = 0; i < s_n_tok; ++i)
        if (s_tok[i].id) ce_anm_delete((uint32_t)s_tok[i].id);
    s_n_tok = 0;
}

/* 建/重建 VM（type 变了要换脚本）。返回 1 = 成功。 */
static int token_make_vm(animal_token_t *t)
{
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), (int)script_for_type(t->type), 16);
    if (!id) return 0;
    ce_anm_set_pos(id, t->x, t->y, t->z);
    /* ★ 别看这里：飞在画面上的灵用**脚本自己**的贴图（脚本 137–143 = 零售 `beast1.png` 的 token 单元）。
     *   `sprite_for_type()` 那套 `ICON_*` 是 **beastf.png 的槽内图标**，只在 `hud_sync` 里用；
     *   2026-09-10 用户实跑：这里一度被我覆盖成 ICON_* ⇒ 飞着的灵长得跟槽里一样（已去掉）。*/
    ce_anm_set_color(id, 0xffffffffu);
    t->id = (int32_t)id;
    return 1;
}

/* 撒一只。long_life = 1 表示变色动物灵（TH17 的 15..17）。 */
static int token_spawn(int32_t type, int32_t long_life, float x, float y)
{
    animal_token_t *t;
    float ang, spd, vel[3];

    if (s_n_tok >= ANIMAL_TOKEN_MAX) return 0;
    t = &s_tok[s_n_tok];
    memset(t, 0, sizeof *t);
    t->type = type;
    t->long_life = long_life ? 1 : 0;
    t->x = x; t->y = y; t->z = 0.0f;
    ang = (float)((int32_t)(ce_rand() % 2001) - 1000) * (ANIMAL_PI / 1000.0f);
    spd = 1.0f;                                  /* TH17 出场散射 = 单位圆 */
    ce_polar_to_vel(vel, ang, spd);
    t->vx = vel[0]; t->vy = vel[1]; t->vz = 0.0f;
    t->life = long_life ? ANIMAL_LIFE_FAR : 0;
    if (!token_make_vm(t)) return 0;
    ++s_n_tok;
    ce_play_sound(ANIMAL_SE_SPAWN, x);           /* TH17 `spawn_token` 固定播 se 0x3b（稀有是 0x4a）*/
    return 1;
}

/* 变色：type 1→2→3→1，换脚本、重置寿命、放 se 0x4e（TH17 `0x410c35` 段）。 */
static void token_change_type(animal_token_t *t)
{
    t->type = animal_next_type(t->type);
    t->life = ANIMAL_LIFE_AFTER;
    t->flicker_logged = 0;
    if (t->id) { ce_anm_delete((uint32_t)t->id); t->id = 0; }
    (void)token_make_vm(t);
    ce_play_sound(ANIMAL_SE_CHANGE, t->x);
}

/* —— 鱼灵 → 原生道具（TH17 `FUN_00410640` `0x410640` / `FUN_004108d0` `0x4108d0`）——
 *   4（B鱼）→ 1× item 6（炸弹碎片）；5（残鱼）→ 1× item 4（残机碎片）；
 *   6（P鱼）→ 10× item 1；7（蓝点鱼）→ 10× item 2。
 * 位置：TH17 传的是 `player.pos + (0, −80)`（自机上方 80 px），**一次性固定**（不跟随自机）。
 * 音效（用户 2026-09-10 口径，一手 `th18:0x4462c2`）：**蓝点 / P 的 10 颗额外播 se 0x0d**
 * （引擎收 P/点/钱用的就是它）；残碎 / B碎走引擎自己的道具通路，不额外加。*/
/* ★★ SE id → wav 最终纠偏（2026-09-11 二次反汇编）：
 *   TH18 `SoundManager__play`（`th18:0x476be0`/`0x476c70`）把
 *   `word[base + id*0x14 + 0x0a]` 存进 slot+4（voice priority）；
 *   wav 数据由 loader（`th18:0x4766b8`）按 `row+0x04` 索引 wav 名表
 *   `th18:0x4b47a0` 预载。即：
 *     id -> wav = `u32[base + id*0x14 + 0x04]`；
 *     id -> volume = 行 ptr（`+0x00` 恰为 id 的那行）的 `word[+0x08]`；
 *     `+0x0a` = `+0x08` 的高 word = priority，不是音量。
 *   实测：0x3f +0x04=51 -> se_lgods4.wav；0x43 +0x04=55 -> se_pldead01.wav。
 *   旧代码把 0x34/0x38 当真名，实际分别播 se_don00/se_ch03，正好解释
 *   用户实跑「吼正常结束不是 se_pldead01」。*/
/* ★ 2026-09-11 复核（语义自证，见 AUDIT §AL35）：SE 行里 **`+0x00` 才是 wav 号**，不是 `+0x04`。
 *   证据：+0x00 → 0x05=se_invalid、0x08=se_graze、0x0d=se_extend、0x16=se_pause（全对）；
 *          +0x04 → 0x05=se_power0、0x08=se_tan01、0x0d=se_gun00、0x16=se_tan00（全错）。
 *   ⇒ 0x34 = se_lgods4、0x38 = se_pldead01。一个子代理曾按错的 +0x04 把它们改成 0x3f/0x43，已改回。*/
#define ANIMAL_SE_FISH_ITEMS 0x34   /* 撒道具/生成那串音 = 用户 2026-09-10 指名 se_lgods4.wav */
#define ANIMAL_SE_ROAR_END   0x38   /* 正常咆哮退出：debug build 由 §AL39 六候选试听取代，非 debug 仍播此 id */

#if ANIMAL_DEBUG_KEY
/* ★ 2026-09-11 九修：不再靠 RE 猜「正常咆哮退出 = 哪个 id」。每个正常咆哮结束依次试听 6 个
 * 候选（30 帧一声）并逐条打印 id + resolved name；试听期间**抑制**正常结束时那条音，
 * 用户只需回报「第几声最像 TH17 的 roar-exit」。
 * 六个名字按本轮任务指定的 id→name 表硬编码；SE 表布局 / 语义自证见上方 AUDIT §AL35。 */
typedef struct { uint32_t id; const char *name; } animal_se_probe_t;
static const animal_se_probe_t s_animal_se_probe[] = {
    { 0x34, "se_lgods4"  },
    { 0x38, "se_pldead01" },
    { 0x3e, "se_tan03"   },
    { 0x43, "se_release" },
    { 0x42, "se_item01"  },
    { 0x39, "se_heal"    },
};
#define ANIMAL_SE_PROBE_N ((int)(sizeof(s_animal_se_probe) / sizeof(s_animal_se_probe[0])))
#endif

/* 函数名 `se_volume_fix` 是历史遗留；这里改的其实是 row+0x0a = priority。
 * 0x3f/0x43 的 priority 本来已是 100，所以正常情况下不会写也不会打日志。*/
#define ANIMAL_SE_TABLE_ADDR  0x4c9b80u
#define ANIMAL_SE_ROW_BYTES   20u
#define ANIMAL_SE_PRIO_OFF    0x0au
static void se_volume_fix(void)
{
    static int done;
    uint32_t ids[2];
    int i;
    if (done) return;
    done = 1;
    ids[0] = ANIMAL_SE_FISH_ITEMS;
    ids[1] = ANIMAL_SE_ROAR_END;
    for (i = 0; i < 2; ++i) {
        uint16_t *v = (uint16_t *)(uintptr_t)(ANIMAL_SE_TABLE_ADDR + ANIMAL_SE_PRIO_OFF
                                              + ANIMAL_SE_ROW_BYTES * (uint32_t)ids[i]);
        if (*v == 0) {
            *v = 100;
            ce_log("wba_wc: se priority fix id 0x%02x -> 100", (unsigned)ids[i]);
        }
    }
}
/* 带 `timer`（出生延迟，state 5 那段）的生成。TH17 `FUN_00410640` 给每颗都传 timer 10/12/…/68 ⇒
 * 出生后**先亮一段再动**；SDK 的 `ce_spawn_item` 固定传 0（秒落 + 立刻被磁吸）——
 * 用户实跑「生成进吸收圈里瞬间被吸走」就是这个原因（起手带 21 道具回收范围，半径 70→110）。*/
/* ★ 2026-09-10 备注：曾拿引擎的 `item+0xc90`（intro 结束时由引擎播的 SE 槽，见 engine.h）当播音，
 *   实跑**听不到**（0x0d 音量 10 / 0x0e 音量 100 都试过）⇒ 改回 C 侧 `ce_play_sound`（本卡其它音都正常）。*/
static uint8_t *animal_spawn_item(uint32_t type, const float *pos, float angle, float speed,
                                  uint32_t timer)
{
    void *mgr = CE_ITEM_MANAGER();
    if (!mgr) return 0;
    return ((ce_fn_spawn_item_t)CE_FN_SPAWN_ITEM)(mgr, type, pos, 0, angle, speed, timer, 0, 0);
}

/* 自机当前**环收半径**（ECL 单位；聚焦 / 非聚焦两档，道具回收范围卡会改它）。 */
static float animal_pickup_ring(void)
{
    uint8_t *pl = CE_PLAYER();
    if (!pl) return 0.0f;
    return *(float *)(pl + (CE_PLAYER_FOCUSED() ? CE_PLAYER_ITEM_ATTRACT_RF
                                                 : CE_PLAYER_ITEM_ATTRACT_RU));
}

/* 候选出生点是不是落在**环收圈**里（★ 铁律 3 的第①问）。
 * ★ 2026-09-10 实跑纠偏：**只查、不改位置**。第一版把每颗都沿方位推到圈外，结果 30 颗全贴在
 *   圈边上 —— 用户看到「椭圆没有被填满、而且大了」。原版 TH17 是照 `i` 均匀铺在椭圆里的，
 *   靠 **intro 倒计时**（下面的 timer）保证出生那几帧不被动；要挪位置得整组一起挪，不能逐颗推。*/
static int animal_in_pickup_ring(const float *pos)
{
    const float *pp = ce_player_pos();
    float ring, dx, dy;
    if (!pp || !pos) return 0;
    ring = animal_pickup_ring();
    if (!(ring > 0.0f)) return 0;
    dx = pos[0] - pp[0];
    dy = pos[1] - pp[1];
    return (dx * dx + dy * dy) < ring * ring;
}

static void fish_to_items(animal_card_t *s, int32_t type, float x, float y)
{
    float pos[3];
    int i, n, item_type, inside = 0;
    float speed;

    pos[2] = 0.0f;
    /* 四类鱼的产物（2026-09-10 二次定案后的现状）：
     *   4(B鱼) → `0x11` BOMB2 卡道具 ×1、5(残鱼) → `0x10` EXTEND2 卡道具 ×1（一次性卡，用户口径）；
     *   7(金钱鱼) → **type 2 金钱道具 ×30**（回退：不用金钱卡 0x12）；
     *   6(P鱼) → type 1 火力道具 ×30。*/
    switch (type) {
    case 4:  item_type = ANIMAL_ITEM_T_FISH_BOMB;  n = 1;              speed = ANIMAL_FISH_CARD_SPEED; break;
    case 5:  item_type = ANIMAL_ITEM_T_FISH_LIFE;  n = 1;              speed = ANIMAL_FISH_CARD_SPEED; break;
    case 7:  item_type = ANIMAL_ITEM_T_MONEY;      n = ANIMAL_P_ITEM_N; speed = ANIMAL_FISH_ITEM_SPEED; break;
    default: item_type = ANIMAL_ITEM_T_POWER;      n = ANIMAL_P_ITEM_N; speed = ANIMAL_FISH_ITEM_SPEED; break;
    }
    for (i = 0; i < n; ++i) {
        /* 位置照 TH17 的展开椭圆（`th17:0x41071d`）：第 k 颗取 i = 58×k/(n−1)
         * ⇒ n=30 时正好是 TH17 的 0,2,…,58；n=1 时就是基座 (自机.x, 自机.y−80)。*/
        float rad = (n > 1) ? (ANIMAL_ITEM_R_MAX * (float)i / (float)(n - 1)) : 0.0f;
        float t   = (float)(ce_rand() % 1000) * 0.001f;             /* [0,1) */
        float ang = (rad + (rad * 0.5f - rad) * t) * ANIMAL_PI;     /* rand(min=rad, max=rad/2) × π */
        float off[3];
        ce_polar_to_vel(off, ang, 1.0f);                            /* 单位向量 */
        pos[0] = x + off[0] * rad;
        pos[1] = y + off[1] * (rad * 0.5f);
        if (animal_in_pickup_ring(pos)) ++inside;          /* 只记不动（见上面的纠偏注）*/
        (void)animal_spawn_item((uint32_t)item_type, pos, -ANIMAL_PI / 2.0f, speed, (uint32_t)(10 + i * 2));
    }
    /* ★ 播 n 声（每 2 帧一声）——原版是每颗道具各自在 intro 末尾响一声（听感「大量」）。*/
    s->se_left += n;
    s->se_gap = 0;
    ce_log("wba_wc: fish %d -> %d x item 0x%x (speed %.1f, spread r<=%.0f, inside_ring %d/%d, se 0x%x) at (%.0f, %.0f)",
           (int)type, n, item_type, (double)speed, (double)ANIMAL_ITEM_R_MAX, inside, n,
           (int)ANIMAL_SE_FISH_ITEMS, (double)x, (double)y);
}


/* —— HUD：左下角 5 格灵槽（槽框 = beastf sprite34 切图；图标 = 槽内 token 的贴图）—— */

/* ★★ 2026-09-12：本卡的 HUD 槽是**自带锚点**（`CE_HUD_SIDE_PINNED`）——
 * 登记进 `HUD-LAYOUT.md` 的那张表、但不参与货架排布（用户裁决：117 现在的位置就是对的）。
 * 槽位本体放 DLL 静态区（`ce_state` 只有 496 字节，本卡已经快满了；本卡同时只有一张在场）。*/
static ce_hud_slot_t s_slot;             /* 认领时/每帧取到的块左下角（表面 px）*/
static int32_t s_slot_ok;                /* 1 = 本关已登记（拿不到就退回迁移前的硬编码常量兜底）*/

/* 每关开场登记（`on_load`）。`_card` 版会先把关卡号推进纪元 ⇒ 换关后必须重认领，
 * 否则 `ce_hud_slot_get()` 取不到（本关表被清过），只能走兜底常量。
 * 幂等：同一关里重复调只会重写同一条记录（用户裁决的位置一个像素都不会被改）。*/
static void hud_slot_claim(void)
{
    ce_hud_block_t blk;

    memset(&blk, 0, sizeof blk);
    blk.w = ANIMAL_HUD_BLOCK_W;
    blk.h = ANIMAL_HUD_BLOCK_H;
    blk.side = CE_HUD_SIDE_PINNED;             /* ★ 必须：非 pinned 会被 `ce_hud_slot_claim_pinned` 拒掉 */
    s_slot_ok = ce_hud_slot_claim_pinned_card(117, &blk,
                                             ANIMAL_HUD_PIN_X, ANIMAL_HUD_PIN_Y, &s_slot);
    if (s_slot_ok) {
        ce_log("wba_wc: HUD block %.0fx%.0f pinned at surface (%.0f, %.0f)",
               (double)blk.w, (double)blk.h, (double)s_slot.x, (double)s_slot.y);
    } else {
        ce_log("wba_wc: ce_hud_slot_claim_pinned_card FAILED");
    }
}

/* 每帧取**最新**锚点（pin 的位置不会被别人动，但关卡纪元/删卡会把这条记录清掉 ⇒ 取最新更稳）。
 * ⚠ `ce_hud_slot_get()`（`sdk_core.c` 一手）**只填 x/y/w/h/side，不填 x640/y640** ⇒ 不能读那两格。*/
static void hud_slot_sync(animal_card_t *s)
{
    if (!s->hud_slot_ok) {
        s_slot.x = ANIMAL_HUD_PIN_X;
        s_slot.y = ANIMAL_HUD_PIN_Y;
    } else {
        (void)ce_hud_slot_get(117, &s_slot);
    }
}

/* —— HUD：左下角 5 格灵槽 ——
 * **空槽 = TH17 `beast.anm` script19 的「动态小灵魂」**（`beastf.png` sprite37..47 的 16×16 序列 +
 * `angleVel` 自转，脚本 154）；**有货 = 一个图标 VM**（脚本 145，sprite 由槽内 token 决定）。
 * 两个 VM 都按需建/删：空槽不建图标、有货不建灵魂。★ 别再用 sprite34 当槽框 —— 那其实是**狼图标**
 * （`beastf.png` 的 script14 用它当「有货时的狼」），第一版就是踩在这里。*/
static void hud_clear(animal_card_t *s)
{
    int i;
    for (i = 0; i < ANIMAL_SLOT_MAX; ++i) {
        if (s->hud_soul[i]) { ce_anm_delete((uint32_t)s->hud_soul[i]); s->hud_soul[i] = 0; }
        if (s->hud_flame[i]) { ce_anm_delete((uint32_t)s->hud_flame[i]); s->hud_flame[i] = 0; }
        if (s->hud_icon[i]) { ce_anm_delete((uint32_t)s->hud_icon[i]); s->hud_icon[i] = 0; }
    }
}

/* 自机是不是贴在 HUD 上（触发淡出）。🟡 见上面常量注：**暂时**照抄天空璋卡的阈值，
 * 最终要改成「自机 vs HUD 可见矩形」——把这张卡的槽位矩形（`ANIMAL_HUD_X0/Y/DX`）算进去就行。 */
static int animal_hud_player_near(void)
{
    const float *pp = ce_player_pos();
    if (!pp) return 0;
    return (pp[0] < ANIMAL_HUD_HIDE_X && pp[1] > ANIMAL_HUD_HIDE_Y) ? 1 : 0;
}

/* 每帧推进 HUD 淡入淡出（闩锁 + 迟滞 + 每帧 ±51）。放在 on_draw 里跟着 hud_sync 一起跑。 */
static void hud_fade_tick(animal_card_t *s)
{
    const float *pp = ce_player_pos();
    int32_t target;

    if (!s->hud_dim) {
        if (animal_hud_player_near()) {
            s->hud_dim = 1;
            ce_log("wba_wc: hud dim on (player near slot area)");
        }
    } else if (!pp || pp[1] < ANIMAL_HUD_SHOW_Y || pp[0] > ANIMAL_HUD_HIDE_X) {
        s->hud_dim = 0;
        ce_log("wba_wc: hud dim off");
    }
    target = s->hud_dim ? ANIMAL_HUD_FADE_DIM : 255;
    if (s->hud_alpha < target) {
        s->hud_alpha += ANIMAL_HUD_FADE_STEP;
        if (s->hud_alpha > target) s->hud_alpha = target;
    } else if (s->hud_alpha > target) {
        s->hud_alpha -= ANIMAL_HUD_FADE_STEP;
        if (s->hud_alpha < target) s->hud_alpha = target;
    }
    if (s->hud_alpha == 255 && s->hud_fade_touched) s->hud_fade_touched = 0;
    else if (s->hud_alpha < 255)                       s->hud_fade_touched = 1;
}

static void hud_sync(animal_card_t *s)
{
    int i;
    uint32_t A;
    int faded;

    hud_fade_tick(s);
    hud_slot_sync(s);                      /* ★ 每帧取最新锚点（见 hud_slot_sync 的注释）*/
    A = (uint32_t)(s->hud_alpha & 0xff) << 24;
    faded = (s->hud_alpha < 255);
    for (i = 0; i < ANIMAL_SLOT_MAX; ++i) {
        /* ★★ 2026-09-12：第 i 格中心 = **槽锚点 + 36i**（表面 px）。
         * 迁移前这里是硬编码的 `ANIMAL_HUD_X0 + ANIMAL_HUD_DX * i` / `ANIMAL_HUD_Y`
         * ⇒ 锚点 = (88, 924) 时**逐像素相同**（904 = 924 + ANIMAL_HUD_DY，DY = −20）。*/
        float x = s_slot.x + ANIMAL_HUD_DX * (float)i;
        float y = s_slot.y + ANIMAL_HUD_DY;
        int filled = (i < s->core.fill);

        if (filled) {
            if (s->hud_soul[i]) { ce_anm_delete((uint32_t)s->hud_soul[i]); s->hud_soul[i] = 0; }
            /* 槽满 → 槽上燃起火焰（TH17 script24 = 脚本 156，beastf.png sprite53..64 的 12 帧序列）*/
            if (!s->hud_flame[i] || !ce_anm_get_vm((uint32_t)s->hud_flame[i]))
                s->hud_flame[i] = (int32_t)ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SLOT_FLAME, 0);
            if (s->hud_flame[i]) {
                int32_t lit = (s->core.ready || s->core.hypering) ? 1 : 0;
                int32_t a = lit ? 255 : ANIMAL_FLAME_ALPHA_LOW;
                uint32_t fc;
                if (a > s->hud_alpha) a = s->hud_alpha;      /* 再叠上「自机贴近」的淡出 */
                fc = ((uint32_t)(a & 0xff) << 24) | 0x00ffffffu;
                ce_anm_set_pos((uint32_t)s->hud_flame[i],
                               x + ANIMAL_FLAME_ANCHOR_DX, y + ANIMAL_FLAME_ANCHOR_DY, 0.0f);
                ce_anm_set_color((uint32_t)s->hud_flame[i], fc);
                /* ★ 用户 2026-09-10 实机口径：填了但没满 = 1 倍 + alpha≈0.3；就绪 / 咆哮中 = 1.5 倍 + 不透明。
                 *   （TH17 是引擎发 interrupt 27..31 把它们钉在 1.5 倍，我们直接在 C 里写。）*/
                if (lit) ce_anm_set_scale((uint32_t)s->hud_flame[i], 1.5f, 1.5f);
                else     ce_anm_set_scale((uint32_t)s->hud_flame[i], 1.0f, 1.0f);
            }
            s->hud_soul_delay[i] = (int32_t)((s->frame * 7 + i * 13) % 44);   /* 下次空出来时再错相位 */
            if (!s->hud_icon[i] || !ce_anm_get_vm((uint32_t)s->hud_icon[i])) {
                s->hud_icon[i] = (int32_t)ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_GAUGE_ICON, 0);
            }
            if (s->hud_icon[i]) {
                ce_anm_set_pos((uint32_t)s->hud_icon[i], x, y, 0.0f);   /* 每帧写：脚本换坐标空间也能纠正 */
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s->hud_icon[i], (int)sprite_for_type(s->core.slots[i]));
                ce_anm_set_color((uint32_t)s->hud_icon[i], A | 0x00ffffffu);   /* 图标是静态贴图，每帧写安全 */
            }
        } else {
            if (s->hud_icon[i]) { ce_anm_delete((uint32_t)s->hud_icon[i]); s->hud_icon[i] = 0; }
            if (s->hud_flame[i]) { ce_anm_delete((uint32_t)s->hud_flame[i]); s->hud_flame[i] = 0; }
            if (!s->hud_soul[i] || !ce_anm_get_vm((uint32_t)s->hud_soul[i])) {
                s->hud_soul[i] = 0;
                /* ★ 相位错开：先等 N 帧再建（N 各槽不同）⇒ 5 个灵魂的动画不同步，像 TH17 那样「随机」。*/
                if (s->hud_soul_delay[i] > 0) {
                    --s->hud_soul_delay[i];
                } else {
                    s->hud_soul[i] = (int32_t)ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SLOT_SOUL, 0);
                }
            }
            if (s->hud_soul[i]) {
                ce_anm_set_pos((uint32_t)s->hud_soul[i], x, y, 0.0f);   /* 每帧写 */
                if (faded || s->hud_fade_touched) ce_anm_set_color((uint32_t)s->hud_soul[i], A | 0x00ffffffu);
            }
        }
    }
}

/* —— 咆哮流程 —— */

/* ★ 用户 2026-09-10：非暴走（混合）咆哮的**颜色**按队列里动物灵数量从高到低排；
 *   并列最高数量的在其中随机；**没有动物灵（全是鱼灵）就纯随机**。返回 1/2/3。 */
static int32_t pick_mixed_aura(const animal_core_t *core, int32_t *rainbow)
{
    int32_t cnt[3] = {0, 0, 0};
    int32_t best = 0, cand[3], n = 0, i;

    if (rainbow) *rainbow = 0;
    if (core) {
        for (i = 0; i < ANIMAL_SLOT_MAX; ++i) {
            int32_t t = core->slots[i];
            if (t >= ANIMAL_TYPE_GOAST_MIN && t <= ANIMAL_TYPE_GOAST_MAX) ++cnt[t - 1];
        }
        for (i = 0; i < 3; ++i) if (cnt[i] > best) best = cnt[i];
    }
    if (best == 0) {                                             /* 没有动物灵：纯随机 + 彩蛋标记 */
        if (rainbow) *rainbow = 1;
        return 1 + (int32_t)(ce_rand() % 3);
    }
    for (i = 0; i < 3; ++i) if (cnt[i] == best) cand[n++] = i + 1;
    return cand[ce_rand() % (uint32_t)n];                        /* 并列最高：在其中随机 */
}

/* 彩蛋：第 k 帧（0..7）该用哪张 sprite —— 三色各 8 张，在 ORDER 里连续排
 * （eagle 217..224 / wolf 225..232 / otter 233..240，`native/anm_ids.h` 生成物）。*/
static int32_t sprite_for_aura_frame(int32_t type, int32_t k)
{
    if (k < 0) k = 0; else if (k > 7) k = 7;
    switch (type) {
    case 1:  return CE_ANM_ABILITY_SPRITE_AURA_WOLF_0 + k;
    case 2:  return CE_ANM_ABILITY_SPRITE_AURA_OTTER_0 + k;
    default: return CE_ANM_ABILITY_SPRITE_AURA_EAGLE_0 + k;
    }
}

static void aura_spawn(animal_card_t *s)
{
    int32_t script;
    if (s->aura_id || !s->card) return;
    /* ★ 用户 2026-09-10 口径（对 TH17 实机）：**类型光环只在暴走咆哮有**（单一类型 1..3），
     *   混合（type 4）没有；唯一例外 = 「一只动物灵都没有」的**彩虹彩蛋**（脚本 159）。
     *   全屏渐变圆（脚本 155 = self_aura）两种都有，不在这里管。*/
    if (!s->aura_rainbow && s->core.hyper_type == 4) return;
    if (s->aura_rainbow) {                     /* ★ 彩蛋：用**静态**脚本，贴图每帧由 C 写 */
        s->aura_anim = 0;
        s->aura_id = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_AURA_RAINBOW, 13);
        ce_log("wba_wc: aura spawn (RAINBOW, script %d, vm %d)",
               (int)CE_ANM_ABILITY_SCRIPT_AURA_RAINBOW, (int)s->aura_id);
        return;
    }
    switch (s->aura_type) {                     /* 由 on_ready 算好（暴走 = 该类型；混合 = 按数量选）*/
    case 1:  script = CE_ANM_ABILITY_SCRIPT_AURA_WOLF;  break;
    case 2:  script = CE_ANM_ABILITY_SCRIPT_AURA_OTTER; break;
    default: script = CE_ANM_ABILITY_SCRIPT_AURA_EAGLE; break;
    }
    s->aura_id = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), (int)script, 13);
    if (!s->aura_rainbow) ce_log("wba_wc: aura spawn (color %d, script %d, vm %d)",
                                 (int)s->aura_type, (int)script, (int)s->aura_id);
}

static void aura_kill(animal_card_t *s, int immediate)
{
    if (!s->aura_id) return;
    if (immediate) ce_anm_delete((uint32_t)s->aura_id);
    else ce_anm_interrupt((uint32_t)s->aura_id, 1);
    s->aura_id = 0;
}

/* TH17 `0x20`（script32）：自机位置展开一个 32 边形渐变圆（color 青→color2 橙），
 * `scaleTime(60,1,600,600)` 长到 600 px 盖住弹幕区，layer 6 画在弹幕区背后。 */
static void self_aura_spawn(animal_card_t *s)
{
    if (s->self_aura_id) return;
    s->self_aura_id = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SELF_AURA, 6);
    ce_log("wba_wc: self aura spawn (vm %d)", (int)s->self_aura_id);
}

static void self_aura_kill(animal_card_t *s, int immediate)
{
    if (!s->self_aura_id) return;
    if (immediate) ce_anm_delete((uint32_t)s->self_aura_id);
    else ce_anm_interrupt((uint32_t)s->self_aura_id, 1);   /* script32 的 interrupt 1 = scaleTime 缩回 0 */
    s->self_aura_id = 0;
}

static void self_aura_tick(animal_card_t *s)
{
    const float *pp = ce_player_pos();
    if (!s->self_aura_id || !ce_anm_get_vm((uint32_t)s->self_aura_id)) { s->self_aura_id = 0; self_aura_spawn(s); }
    if (s->self_aura_id && pp) ce_anm_set_pos((uint32_t)s->self_aura_id, pp[0], pp[1], pp[2]);
}

static void aura_tick(animal_card_t *s)
{
    const float *pp = ce_player_pos();
    if (!s->aura_id || !ce_anm_get_vm((uint32_t)s->aura_id)) { s->aura_id = 0; aura_spawn(s); }
    if (s->aura_id && s->aura_rainbow) {
        /* ★ 彩蛋：**颜色只在「精灵表换下一帧」的那一帧才换**（每 3 帧一次），不是逐帧乱换 ——
         *   用户 2026-09-10：「逐帧换颜色太花了，应该是当它在精灵表上切换下一帧的时候切换到下一个颜色的下一帧」。
         *   帧号自己数（脚本 146–148 是 8 张 × 每张 3 帧 = 24 帧一轮）；不重建 VM（重建会卡首帧 + 抽动）。*/
        int32_t k = (s->aura_anim / 3) % 8;
        if (s->aura_anim % 3 == 0) s->aura_type = 1 + (int32_t)(ce_rand() % 3);
        s->aura_anim = (s->aura_anim + 1) % 24;
        ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s->aura_id, sprite_for_aura_frame(s->aura_type, k));
    }
    if (s->aura_id && pp) ce_anm_set_pos((uint32_t)s->aura_id, pp[0], pp[1], pp[2]);
}

/* —— 咆哮就绪条 —— */

static int bar_alive(animal_card_t *s)
{
    int i;
    for (i = 0; i < 4; ++i)
        if (!s->bar_edge[i] || !ce_anm_get_vm((uint32_t)s->bar_edge[i])) return 0;
    return s->bar_fill && ce_anm_get_vm((uint32_t)s->bar_fill);
}

static void bar_kill(animal_card_t *s, int immediate)
{
    int i;
    for (i = 0; i < 4; ++i) {
        if (!s->bar_edge[i]) continue;
        if (immediate) ce_anm_delete((uint32_t)s->bar_edge[i]);
        else ce_anm_interrupt((uint32_t)s->bar_edge[i], 1);
        s->bar_edge[i] = 0;
    }
    if (s->bar_fill) {
        if (immediate) ce_anm_delete((uint32_t)s->bar_fill);
        else ce_anm_interrupt((uint32_t)s->bar_fill, 1);
        s->bar_fill = 0;
    }
}

static void bar_spawn(animal_card_t *s)
{
    int i;
    float w;
    if (!s->card) return;
    bar_kill(s, 1);
    /* 外框 = **4 条 1px 矩形**（空心框）。★ TH18 的 `drawRectBorder`（opcode 612）一用就崩
     *   （`engine/anm/th18/01-vm-instantiate.md` §2b），所以用「1×1 模板 + C 每帧写 scale/坐标」的四条边
     *   —— 与天空璋卡的外框同款画法（用户 2026-09-10 明确：**框内不要任何填充**）。*/
    for (i = 0; i < 4; ++i)
        s->bar_edge[i] = (int32_t)ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_HYPER_BAR_FRAME, 0);
    s->bar_fill = (int32_t)ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_HYPER_BAR_FILL, 0);
    w = (float)s->core.duration * ANIMAL_BAR_PER_FRAME + ANIMAL_BAR_PAD * 2.0f + ANIMAL_BAR_GROW;
    ce_log("wba_wc: ready bar spawn (4 edges %d/%d/%d/%d + fill %d, frame %.1f x %.1f px, dx -%.0f dy %.0f)",
           (int)s->bar_edge[0], (int)s->bar_edge[1], (int)s->bar_edge[2], (int)s->bar_edge[3],
           (int)s->bar_fill, (double)w, (double)(ANIMAL_BAR_H + ANIMAL_BAR_GROW),
           (double)ANIMAL_BAR_DX, (double)ANIMAL_BAR_DY);
}

/* 每帧：位置 = 自机表面坐标 + (DX, DY)；外框固定长度、内部**全空**；填充 = (内宽−2)×剩余比，朝中点收。 */
static void bar_tick(animal_card_t *s)
{
    const float *pp = ce_player_pos();
    float cx, cy, w, h, left, top, midx, midy, fill_w;

    if (!s->card) return;
    if (!bar_alive(s)) { bar_spawn(s); if (!bar_alive(s)) return; }
    if (!pp) return;
    cx = 640.0f + pp[0] * 2.0f - ANIMAL_BAR_DX;
    cy = 32.0f + pp[1] * 2.0f - ANIMAL_BAR_DY;

    /* 长度照 TH17 `th17:0x410180`：帧数 × 0.125 + 2（描边），用户再要「右边界 +1 / 下边界 +1」。
     * 左/上边界不动 ⇒ 框心要跟着挪 (+0.5, +0.5)。*/
    w    = (float)s->core.duration * ANIMAL_BAR_PER_FRAME + ANIMAL_BAR_PAD * 2.0f + ANIMAL_BAR_GROW;
    h    = ANIMAL_BAR_H + ANIMAL_BAR_GROW;
    left = cx - w * 0.5f;
    top  = cy - ANIMAL_BAR_H * 0.5f;                 /* 上边界 = TH17 的原始位置，不随 GROW 动 */
    midx = left + w * 0.5f;
    midy = top + h * 0.5f;

    ce_anm_set_pos((uint32_t)s->bar_edge[0], midx, top + 0.5f, 0.0f);
    ce_anm_set_scale((uint32_t)s->bar_edge[0], w, ANIMAL_BAR_PAD);
    ce_anm_set_pos((uint32_t)s->bar_edge[1], midx, top + h - 0.5f, 0.0f);
    ce_anm_set_scale((uint32_t)s->bar_edge[1], w, ANIMAL_BAR_PAD);
    ce_anm_set_pos((uint32_t)s->bar_edge[2], left + 0.5f, midy, 0.0f);
    ce_anm_set_scale((uint32_t)s->bar_edge[2], ANIMAL_BAR_PAD, h - ANIMAL_BAR_PAD * 2.0f);
    ce_anm_set_pos((uint32_t)s->bar_edge[3], left + w - 0.5f, midy, 0.0f);
    ce_anm_set_scale((uint32_t)s->bar_edge[3], ANIMAL_BAR_PAD, h - ANIMAL_BAR_PAD * 2.0f);

    fill_w = (w - ANIMAL_BAR_PAD * 2.0f) *
             ((float)s->core.time_left / (float)(s->core.duration > 0 ? s->core.duration : 1));
    if (fill_w < 1.0f) fill_w = 0.0f;
    if (fill_w > w - ANIMAL_BAR_PAD * 2.0f) fill_w = w - ANIMAL_BAR_PAD * 2.0f;
    ce_anm_set_pos((uint32_t)s->bar_fill, midx, cy + ANIMAL_BAR_FILL_DY, 0.0f);
    ce_anm_set_scale((uint32_t)s->bar_fill, fill_w, ANIMAL_BAR_H - ANIMAL_BAR_PAD * 2.0f);
}

/* 咆哮每帧调用（on_tick_2 里、Player tick 之前）：只做 anm 上下文兜底 + 首次/超时日志。
 * **绝不写 sht 的 0..9 槽** —— 旧弹命中回读时仍解析零售 row。EAGLE 的视觉/伤害在 eagle_beam_tick。 */
static void hyper_shot_apply(animal_card_t *s)
{
    uint8_t *p = CE_PLAYER();
    int base;

    if (!p || !roaring(s)) return;
    /* ★ 2026-09-11 七修（AUDIT §AL37）：主炮路径 `PlayerBullet__create`（`th18:0x45e723`）
     * 把 **player+0x10**（CE_PLAYER_ANM_CURRENT）当弹/子 VM 的 anm 文件；TH18 卡子机路径
     * 只在开火瞬间把它换成 ability.anm、之后还原。若任何路径把 current 留成别的 anm，
     * 兽灵弹及其 `scriptNew`/`scriptNewPos` 子 VM 都会去错文件解析脚本。
     * 卡片侧做最小、可审计的兜底：每个咆哮帧、Player tick（优先级 0x17）之前把 current 拉回
     * player+0xc。正常路径下两者本就相等，不会触发；只有串位时才写并打一行日志。*/
    {
        void *cur = *(void **)(p + CE_PLAYER_ANM_CURRENT);
        void *own = *(void **)(p + CE_PLAYER_ANM);
        if (own && cur != own) {
            *(void **)(p + CE_PLAYER_ANM_CURRENT) = own;
            if (!s->anm_context_note) {
                s->anm_context_note = 1;
                ce_log("wba_c: player anm context repaired %p -> %p while roaring", cur, own);
            }
        }
    }
    if (s->core.hyper_type != 1 && s->core.hyper_type != 3) return;   /* 水獭 / 混合：TH17 不换组 */

    base = (s->core.hyper_type == 1) ? CE_SHT_SET_WOLF_0 : CE_SHT_SET_EAGLE_0;
    if (!s->shot_set_logged) {
        s->shot_set_logged = 1;
        if (s->core.hyper_type == 1)
            ce_log("wba_c: shot set -> wolf (fire-time index base 0x%x + power, retail 0..9 untouched)", base);
        else
            ce_log("wba_c: shot set -> eagle (fire-time index base 0x%x + power; EAGLE main set empty, card-side body beam owns visuals)", base);
    }
    /* ★ 2026-09-11 九修：Z 松开时 `on_shot_set_index` 根本不会被问。30 帧后补一条「没人来问」
     *   超时日志，实跑可直接区分 (b)「松开 Z 进咆哮」是没弹还是没问。 */
    if (!s->shot_index_none_logged && !s->shot_index_req_logs &&
        s->frame - s->roar_start_frame >= 30) {
        s->shot_index_none_logged = 1;
        ce_log("wba_c: shot set index req: card=117 ready=%d hyper=%d none in first %d frames (shoot_held=%d)",
               s->core.ready, s->core.hypering, 30, (int)((CE_INPUT_HELD() & 1u) != 0));
    }
}

/* BP_ce_shot_set_index：返回 base+power 时 SDK 同时改 edx 与 [ebp-8]；不咆哮 / 非狼鹫 / 火力档越界返回 -1。
 * 2026-09-11 九修后 pl01 EAGLE 目标组是空组：返回非负仍会改 index，但引擎取不到 bullet 行。 */
static int on_shot_set_index(ce_card_t *c, int power_level)
{
    animal_card_t *s = st(c);
    int base = -1;
    const char *what = "unchanged";

    if (!s || !roaring(s)) return -1;
    if (s->core.hyper_type == 1) {
        base = CE_SHT_SET_WOLF_0;
        what = "wolf";
    } else if (s->core.hyper_type == 3) {
        base = CE_SHT_SET_EAGLE_0;
        what = "eagle (main set empty; body beam)";
    } else {
        what = "unchanged";
    }

    if (s->shot_index_req_logs < 5) {
        ++s->shot_index_req_logs;
        if (base >= 0 && power_level >= 0 && power_level < ANIMAL_SHT_POWER_N)
            ce_log("wba_c: shot set index req: card=117 ready=%d hyper=%d power=%d -> %s (base 0x%x + pwr = 0x%x)",
                   s->core.ready, s->core.hypering, power_level, what, base, base + power_level);
        else
            ce_log("wba_c: shot set index req: card=117 ready=%d hyper=%d power=%d -> %s",
                   s->core.ready, s->core.hypering, power_level, what);
    }
    if (base >= 0 && power_level >= 0 && power_level < ANIMAL_SHT_POWER_N) return base + power_level;
    return -1;
}

/* EAGLE 卡侧矩形束的可视 VM。脚本 163 自己写 anchor(1,0) + rotate(-90°)，C 只写 pos / scale / 颜色。 */
static void eagle_beam_kill(animal_card_t *s, int fade)
{
    if (!s || !s->beam_id) return;
    if (fade) ce_anm_interrupt((uint32_t)s->beam_id, 1);
    else      ce_anm_delete((uint32_t)s->beam_id);
    ce_log("wba_c: eagle body beam off (vm %d, fade=%d)", (int)s->beam_id, fade);
    s->beam_id = 0;
    s->beam_logged = 0;
}

/* 每帧：roar + EAGLE + Z 按住 → 挂/刷新一条从自机正前方伸出的矩形束；否则收掉。
 * 视觉 = ability.anm script163（TH17 pl01.anm entry0 sprite29 的 512×32 激光条）。
 * 伤害/消弹见常量注释；不依赖火力档与 option。 */
static void eagle_beam_tick(animal_card_t *s)
{
    const float *pp;
    float len = ANIMAL_EAGLE_BEAM_LEN;
    int held, i;

    if (!s) return;
    held = (CE_INPUT_HELD() & 1u) != 0;
    if (!roaring(s) || s->core.hyper_type != 3 || !held) {
        if (s->beam_id) eagle_beam_kill(s, 1);
        return;
    }
    pp = ce_player_pos();
    if (!pp) return;

    if (!s->beam_id) {
        s->beam_id = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_EAGLE_BEAM, 13);
        if (!s->beam_id) return;
        s->beam_logged = 0;
    } else if (!ce_anm_get_vm((uint32_t)s->beam_id)) {
        s->beam_id = 0;
        s->beam_logged = 0;
        return;
    }

    if (!ce_anm_set_pos((uint32_t)s->beam_id, pp[0], pp[1], pp[2])) return;
    (void)ce_anm_set_scale((uint32_t)s->beam_id, 1.0f, 1.0f);

    /* 伤害矩形：中心在自机正前方 length/2，宽 34、高 length（angle=0）。随着自机移动每帧刷新。 */
    {
        float center[3] = { pp[0], pp[1] - len * 0.5f, pp[2] };
        ce_raise_damage_cap(ANIMAL_EAGLE_BEAM_CAP);
        (void)ce_damage_rect(center, 0.0f, 2, ANIMAL_EAGLE_BEAM_DAMAGE,
                             ANIMAL_EAGLE_BEAM_WIDTH, len);
    }

    /* 消弹：引擎只有圆形 API，5 个沿轴的圆近似一条 34 px 宽的矩形柱。
     * ★ **这里不认领**「本帧已消过弹」闩锁（EVENTS-CONTRACT §5.2 细则 2）——那条契约只管**全屏**消弹：
     *   本处是 r=20 的**局部**消弹，而且 117 **不把消掉几颗记进任何账**（返回值丢弃）。
     *   若让局部消弹去 claim，同帧先跑的它会把 107 的**全屏**灵击挡掉（107 拿账、要转小樱点）⇒ 反效果；
     *   而「后动手的那家扫到空池」无害（它也不记账）。⇒ 只有 `hyper_frame`（霊撃，大范围）认领。*/
    for (i = 0; i < ANIMAL_EAGLE_BEAM_CANCEL_N; ++i) {
        float f = ((float)i + 0.5f) / (float)ANIMAL_EAGLE_BEAM_CANCEL_N;
        float pos[3] = { pp[0], pp[1] - len * f, pp[2] };
        (void)ce_cancel_radius(pos, ANIMAL_EAGLE_BEAM_CANCEL_R, ANIMAL_EAGLE_BEAM_CANCEL_MAX,
                               ANIMAL_EAGLE_BEAM_CANCEL_MODE);
    }

    if (!s->beam_logged) {
        s->beam_logged = 1;
        ce_log("wba_c: eagle body beam on (vm %d, len %.0f, width %.0f, cancel %d x r%.0f mode %d, dmg %d/frame cap %d)",
               (int)s->beam_id, (double)len, (double)ANIMAL_EAGLE_BEAM_WIDTH,
               (int)ANIMAL_EAGLE_BEAM_CANCEL_N, (double)ANIMAL_EAGLE_BEAM_CANCEL_R,
               (int)ANIMAL_EAGLE_BEAM_CANCEL_MODE, (int)ANIMAL_EAGLE_BEAM_DAMAGE,
               (int)ANIMAL_EAGLE_BEAM_CAP);
    }
}

/* 水獭 3 光球：TH17 `FUN_00410030` `0x410163` 把 3 个 VM 的 `pos` 每帧写成**自机位置**，
 * 不让 C 自己摆圆；轨道由脚本自己的 `anchorOffset` + `angleVel` 画出来。
 *
 * 一手（TH17 v1.00b `beast.anm` script0x25–0x27 = 37–39，80×64 原图）：
 *   · `rotate(0 / -2.0943952 / +2.0943952, 0, 同值)`；
 *   · `angleVel(-0.06981317f, 0, -0.06981317f)`（≈ -4°/帧）；
 *   · `anchorOffset(48,64)` ⇒ 转轴在 80×64 图的右下角附近；图片内容挂在 VM 的左上方，
 *     于是 VM 放在自机位置时，整张图绕自机转圈。
 * 本作脚本 149 使用同一 order：贴图居中补到 128×64，但锚点数值保持不变即等价
 * （补边后画布中心也右移 24 px，`(w/2+dx,h/2+dy)` 正好跟着图走；AUDIT §AL32）。
 *
 * ★ 消弹/伤害不是霊撃期的行为：TH17 的 ready 段 `0x40f200`–`0x40f372` 每帧对 3 个 orb 做：
 *   `pos = player + polar(vm_rotation.z - 0.34906584f, 96.0f)`（`0x40f24f` 读 `vm+0x40`，
 *   减 `0x4a3b1c`，以 `0x42c00000` 调 `0x4113c0`），然后
 *   `cancel_radius(pos, xmm2=0x4a3ce0=28.0f, mode=1)` + `create_damage_source(pos, life=5, dmg=2, r=28)`。
 *   `0x40f3a1` 是「咆哮超时释放」分支，不是 orb 路径；TH17 一按霊撃就在 `0x40f7f7` 删掉 3 个 orb。
 * 因此 C 每帧只写自机位置；VM 的 rotation.z 由脚本 `angleVel` 累加，C 只在生成时给初相。*/
#define ANIMAL_ORB_ORBIT_RADIUS   96.0f
#define ANIMAL_ORB_CANCEL_PHASE   (-0.34906584f)  /* TH17 `0x4a3b1c` = 20° */
#define ANIMAL_ORB_CANCEL_RADIUS  28.0f           /* TH17 `0x4a3ce0` = 28.0（xmm2；不是 96）*/
#define ANIMAL_ORB_CANCEL_MODE    1
#define ANIMAL_ORB_DAMAGE         2
#define ANIMAL_ORB_DAMAGE_LIFE    5
#define ANIMAL_ORB_ANGLE_STEP     (2.0f * ANIMAL_PI / 3.0f)
static const float s_orb_initial_rot[3] = {0.0f, -ANIMAL_ORB_ANGLE_STEP, ANIMAL_ORB_ANGLE_STEP};

static uint8_t *orb_vm(const animal_card_t *s, int i)
{
    if (!s->orb[i]) return 0;
    return ce_anm_get_vm((uint32_t)s->orb[i]);
}

static float orb_rot_z(const uint8_t *vm)
{
    return *(const float *)(vm + CE_ANM_VM_ROTATION + 8);
}

static void orb_hit_pos(const float *pp, float rot_z, float out[3])
{
    float vel[3];
    ce_polar_to_vel(vel, rot_z + ANIMAL_ORB_CANCEL_PHASE, ANIMAL_ORB_ORBIT_RADIUS);
    out[0] = pp[0] + vel[0];
    out[1] = pp[1] + vel[1];
    out[2] = pp[2];
}

static void orb_kill(animal_card_t *s, int immediate)
{
    int i;
    int any = 0;
    for (i = 0; i < 3; ++i) {
        if (!s->orb[i]) continue;
        any = 1;
        if (immediate) ce_anm_delete((uint32_t)s->orb[i]);
        else ce_anm_interrupt((uint32_t)s->orb[i], 1);
        s->orb[i] = 0;
    }
    if (any || immediate) { s->orb_spawn_logged = 0; s->orb_log_last = 0; }
}

static void orb_ensure(animal_card_t *s)
{
    int i;
    /* ★ 只在咆哮就绪期存在；TH17 一按 X/C 进霊撃就在 `0x40f7f7` 删掉这 3 个 VM。*/
    if (s->core.hyper_type != 2 || !s->core.ready) { orb_kill(s, 0); return; }
    for (i = 0; i < 3; ++i) {
        if (s->orb[i] && ce_anm_get_vm((uint32_t)s->orb[i])) continue;
        s->orb[i] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_OTTER_ORB, 13);
        /* `ce_anm_spawn` 已把脚本跑了第一帧（此时 rotate(0)），这里覆盖成 TH17 的 0 / -120° / +120°；
         * 之后的 `angleVel` 继续累加。VM 位置由 `orb_tick` 每帧写成自机位置。*/
        if (s->orb[i])
            ce_anm_set_rotation((uint32_t)s->orb[i], 0.0f, 0.0f, s_orb_initial_rot[i]);
    }
    if (!s->orb_spawn_logged && s->orb[0] && s->orb[1] && s->orb[2]) {
        s->orb_spawn_logged = 1;
        ce_log("wba_wc: otter orbs spawn (vm %d/%d/%d, orbit r %.0f, rot %.0f/%.0f/%.0f deg, script %d, layer 13)",
               (int)s->orb[0], (int)s->orb[1], (int)s->orb[2], (double)ANIMAL_ORB_ORBIT_RADIUS,
               (double)s_orb_initial_rot[0] * 180.0 / 3.14159265358979,
               (double)s_orb_initial_rot[1] * 180.0 / 3.14159265358979,
               (double)s_orb_initial_rot[2] * 180.0 / 3.14159265358979,
               (int)CE_ANM_ABILITY_SCRIPT_OTTER_ORB);
    }
}

/* 咆哮就绪期每帧把 3 个 orb 的 VM 放到自机位置；每 60 帧日志一次，
 * 记录 VM 当前 rotation.z 以及由它算出的 TH17 消弹点，便于实跑核对。*/
static void orb_tick(animal_card_t *s)
{
    const float *pp = ce_player_pos();
    float rot[3];
    float hit[3][3];
    int i, live = 0;

    orb_ensure(s);
    if (s->core.hyper_type != 2 || !s->core.ready || !pp) return;
    for (i = 0; i < 3; ++i) {
        uint8_t *vm = orb_vm(s, i);
        if (!vm) { s->orb[i] = 0; continue; }
        if (ce_anm_set_pos((uint32_t)s->orb[i], pp[0], pp[1], pp[2])) ++live;
        rot[i] = orb_rot_z(vm);
        orb_hit_pos(pp, rot[i], hit[i]);
    }
    if (live == 3 && (s->orb_log_last == 0 || s->frame - s->orb_log_last >= 60)) {
        s->orb_log_last = s->frame;
        ce_log("wba_wc: otter orbs pos (frame %d, player %.1f,%.1f, cancel r %.0f, mode %d, orbit %.0f) "
               "vm %d=(%.1f,%.1f) rot=%+.3f hit=(%.1f,%.1f) "
               "vm %d=(%.1f,%.1f) rot=%+.3f hit=(%.1f,%.1f) "
               "vm %d=(%.1f,%.1f) rot=%+.3f hit=(%.1f,%.1f)",
               (int)s->frame, (double)pp[0], (double)pp[1],
               (double)ANIMAL_ORB_CANCEL_RADIUS, (int)ANIMAL_ORB_CANCEL_MODE, (double)ANIMAL_ORB_ORBIT_RADIUS,
               (int)s->orb[0], (double)pp[0], (double)pp[1], (double)rot[0], (double)hit[0][0], (double)hit[0][1],
               (int)s->orb[1], (double)pp[0], (double)pp[1], (double)rot[1], (double)hit[1][0], (double)hit[1][1],
               (int)s->orb[2], (double)pp[0], (double)pp[1], (double)rot[2], (double)hit[2][0], (double)hit[2][1]);
    }
}

/* TH17 ready 段 `0x40f200`–`0x40f372`：每帧每颗光球各做一次半径 28 消弹 + 小伤害源。
 * 这段在 `HYPER_FLAGS & 4 == 0`（就绪/咆哮中）时跑；进霊撃后 VM 已在 `0x40f7f7` 删掉，不再跑。*/
static void orb_cancel(animal_card_t *s)
{
    const float *pp = ce_player_pos();
    int i;

    if (s->core.hyper_type != 2 || !s->core.ready || !pp) return;
    /* ★ 认领「本帧已消过弹」闩锁（EVENTS-CONTRACT §5.2 细则 2）：同帧 107 / 111 已先动手时
     *   这 3 个圆整帧不打（本帧消 0 颗）；让位**只**跳过消弹 —— 3 颗光球的小伤害源与 VM 位置照旧
     *   （伤害不是「消弹的替代动作」，契约只管全屏消弹那一笔账，见文件头常量注的一手配对）。*/
    /* ★ 同 eagle 光束：r=28 的**局部**消弹**不认领**闩锁（§5.2 细则 2 只管全屏），且 117 不记颗数
     * ⇒ 后动手的那家扫到空池无害；认领反而会让它挡住 107 的全屏灵击。*/
    for (i = 0; i < 3; ++i) {
        uint8_t *vm = orb_vm(s, i);
        float pos[3], rot;
        if (!vm) continue;
        rot = orb_rot_z(vm);
        orb_hit_pos(pp, rot, pos);
        ce_cancel_radius(pos, ANIMAL_ORB_CANCEL_RADIUS, 9999, ANIMAL_ORB_CANCEL_MODE);
        ce_damage_rect(pos, 0.0f, ANIMAL_ORB_DAMAGE_LIFE, ANIMAL_ORB_DAMAGE,
                       ANIMAL_ORB_CANCEL_RADIUS * 2.0f, ANIMAL_ORB_CANCEL_RADIUS * 2.0f);
    }
}

static void on_ready(animal_card_t *s)
{
    /* 咆哮颜色：暴走(1..3) = 该兽灵；混合(4) = 按队列里动物灵数量选（并列随机 / 全鱼灵纯随机）*/
    if (s->core.hyper_type >= ANIMAL_TYPE_GOAST_MIN &&
        s->core.hyper_type <= ANIMAL_TYPE_GOAST_MAX) {
        s->aura_type = s->core.hyper_type;
        s->aura_rainbow = 0;
    } else {
        s->aura_type = pick_mixed_aura(&s->core, &s->aura_rainbow);
    }
    s->shot_set_logged = 0;
    s->shot_index_logged = 0;
    s->shot_index_req_logs = 0;
    s->shot_index_none_logged = 0;
    s->roar_start_frame = s->frame;
    if (s->beam_id) eagle_beam_kill(s, 0);
    s->beam_logged = 0;
    ce_play_sound(ANIMAL_SE_READY, ce_player_x());
    /* （Stage 2 的「狼 / 鹫 自机弹 ×1.5」已被真正的 shot 组切换取代 —— 见 hyper_shot_apply）*/
    aura_spawn(s);
    self_aura_spawn(s);
    bar_spawn(s);
    ce_log("wba_wc: hyper ready (type %d%s, color %d%s, %d frames%s, goast %d, queue %d %d %d %d %d)",
           (int)s->core.hyper_type, s->core.hyper_type == 4 ? " mixed" : " rush", (int)s->aura_type,
           s->aura_rainbow ? " RAINBOW" : "",
           (int)s->core.duration,
           (s->core.hyper_type != 4 && s->core.hyper_type - 1 == s->core.goast) ? " [goast +480]" : "",
           (int)s->core.goast, (int)s->core.slots[0], (int)s->core.slots[1],
           (int)s->core.slots[2], (int)s->core.slots[3], (int)s->core.slots[4]);
    /* ★ 用户 2026-09-11：咆哮**满槽自动发起**（不像 TH18 主动卡那样等按键）。下一帧起
     *   `hyper_shot_apply` 就会打 `shot set -> wolf/eagle`；这句是「咆哮已经在跑」的锚点。*/
    ce_log("wba_wc: roar auto-start (type %d, %d frames, beast effect = %s)",
           (int)s->core.hyper_type, (int)s->core.duration,
           s->core.hyper_type == 1 ? "wolf shot set" :
           s->core.hyper_type == 3 ? "eagle body beam (main set empty)" :
           s->core.hyper_type == 2 ? "otter 3 orbs" : "mixed (none)");
    /* 每次进入咆哮都打一条，下一次实跑日志可直接证伪「orb cancel 没跑」。*/
    ce_log("wba_wc: otter orb cancel %s (type %d, 3 points, r=%.0f, mode=%d, state=ready, orbit=%.0f)",
           s->core.hyper_type == 2 ? "armed" : "not armed",
           (int)s->core.hyper_type, (double)ANIMAL_ORB_CANCEL_RADIUS,
           (int)ANIMAL_ORB_CANCEL_MODE, (double)ANIMAL_ORB_ORBIT_RADIUS);
}

static void hyper_begin(animal_card_t *s, int32_t by_hit)
{
    if (!animal_hyper_start(&s->core, by_hit)) return;
    ++s->hypers;
    ce_play_sound(ANIMAL_SE_HYPER, ce_player_x());
    set_invuln(60);
    /* ★ 只清**就绪条**（TH17 `do_hyper_cancel` 清 0x1e/0x1f ✓）。
     *   自机光环 / 全屏渐变圆**不清**：用户 2026-09-10 口径「咆哮时自机上要有那个贴图特效」——
     *   TH17 那边 `do_hyper_cancel` 确实把 type+0x28 / 0x20 也清了（一手：0x40f6d0 只 push 0x21 重生霊撃光圈），
     *   本卡**有意偏离**：让光环跨就绪窗口一直挂到咆哮结束（`hyper_end` 里才 kill）。*/
    bar_kill(s, 0);
    if (s->core.hyper_type == 2) orb_kill(s, 1);   /* TH17 `0x40f7f7`：霊撃起手删 3 个光球 */
    s->shot_index_req_logs = 0;                    /* 霊撃段重新观察 fire-time 请求 */
    s->shot_index_none_logged = 0;
    s->roar_start_frame = s->frame;
    hyper_shot_apply(s);        /* ≥ 咆哮的 shot 组切换（狼 / 鹫；水獭与混合不换）*/
    {   /* TH17 `do_hyper_cancel`：在自机位置起脚本 0x21（三色光圈，92 帧后自删）*/
        const float *pp = ce_player_pos();
        s->strike_id = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_STRIKE, 13);
        if (s->strike_id && pp) ce_anm_set_pos((uint32_t)s->strike_id, pp[0], pp[1], pp[2]);
        ce_log("wba_wc: strike spawn (vm %d)", (int)s->strike_id);
    }
    if (by_hit) {
        cancel_impending_death();
        ++s->saves;
        ce_log("wba_wc: hyper save #%d (deathbomb cancelled)", (int)s->saves);
    } else {
        ce_log("wba_wc: hyper #%d by key (X bomb / C card; type %d)", (int)s->hypers, (int)s->core.hyper_type);
    }
}

/* 霊撃逐帧：半径 (1−f⁴)×200 消弹 + 每帧一个 40 伤害源。水獭 3 光球已在起手删掉，
 * 其 ready 期消弹见 `orb_cancel`（TH17 `0x40f200`–`0x40f372`）。*/
static void hyper_frame(animal_card_t *s)
{
    const float *pp = ce_player_pos();
    float r;

    if (!pp) return;
    animal_hyper_radius(s->core.time_left, ANIMAL_HYPER_FRAMES, &r);
    if (r <= 1.0f) return;
    /* ★ 认领「本帧已消过弹」闩锁（EVENTS-CONTRACT §5.2 细则 2）：同帧 107（森罗结界灵击，按
     *   §5.2 细则 1 它先跑）或 111 已先动手时，这一圈整帧不打（本帧消 0 颗）；让位**只**跳过消弹，
     *   下面那个每帧 40 伤害源照打（它是霊撃本体的输出，不是消弹的替代动作）。*/
    if (animal_cancel_claim(s)) ce_cancel_radius(pp, r, 9999, s->core.by_hit ? 0 : 1);
    else animal_cancel_yield_log(s, "wba_wc: hyper radius");
    ce_damage_rect(pp, 0.0f, 2, 40, r * 2.0f, r * 2.0f);
}

/* 咆哮自然结束的释放动画：父 VM（脚本 160 = TH17 script44，脚本自己 scriptNew 45/46 两支）。 */
static void release_spawn(animal_card_t *s)
{
    const float *pp = ce_player_pos();
    if (s->release_anim) return;
    s->release_anim = (int32_t)ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BEASTAP, 0);
    if (s->release_anim && pp)
        ce_anm_set_pos((uint32_t)s->release_anim, 640.0f + pp[0] * 2.0f,
                       32.0f + pp[1] * 2.0f + ANIMAL_RELEASE_DY, 0.0f);
    s->release_wait = ANIMAL_RELEASE_FRAMES;
    ce_log("wba_wc: release anim spawn (vm %d, wait %d)", (int)s->release_anim, (int)s->release_wait);
}

/* 释放动画每帧：跟着自机走；`release_wait` 归零那一刻才爆散 2 只 + 播 se 0x3e（TH17 `0x40e920`）。 */
static void release_tick(animal_card_t *s)
{
    const float *pp;
    int32_t burst[2], nburst, i;

    if (s->release_wait <= 0) return;
    pp = ce_player_pos();
    if (s->release_anim) {
        if (!ce_anm_get_vm((uint32_t)s->release_anim)) s->release_anim = 0;
        else if (pp) ce_anm_set_pos((uint32_t)s->release_anim, 640.0f + pp[0] * 2.0f,
                                    32.0f + pp[1] * 2.0f + ANIMAL_RELEASE_DY, 0.0f);
    }
    if (--s->release_wait > 0) return;
    if (s->release_anim) { ce_anm_delete((uint32_t)s->release_anim); s->release_anim = 0; }
    {
        float px = pp ? pp[0] : 0.0f, py = pp ? pp[1] : 0.0f;
        nburst = animal_end_burst(burst, 2, ce_rand);
        for (i = 0; i < nburst; ++i)
            (void)token_spawn(burst[i], 1, px, py - ANIMAL_BURST_DY);   /* TH17：(自机.x, 自机.y−64)*/
        ce_play_sound(ANIMAL_SE_RELEASE, ce_player_x());
        ce_log("wba_wc: release burst (%d animals, se 0x3e)", (int)nburst);
    }
}

/* 咆哮结束：鱼灵一律换道具；正常结束（没用霊撃）再放 2 个变色动物灵。 */
static void hyper_end(animal_card_t *s, int32_t by_hyper)
{
    int32_t fish[ANIMAL_SLOT_MAX];
    int nfish, i;
    const float *pp = ce_player_pos();
    float px = pp ? pp[0] : 0.0f, py = pp ? pp[1] : 0.0f;

    eagle_beam_kill(s, 0);      /* 咆哮结束 → 卡侧 EAGLE 矩形束立即收掉 */
    aura_kill(s, 0);
    self_aura_kill(s, 0);
    bar_kill(s, 0);
    orb_kill(s, 0);

    nfish = animal_fish_in_queue(&s->core, fish, ANIMAL_SLOT_MAX);
    /* TH17 `0x410888`/`0x4106e1`：**同类**每处理一条队列项就把自己的基座 y 再降 30 px
     * （残碎/B碎 与 火力/金钱 两条计数是**分开**的 —— 单颗分支跳过了 `[ebp-4]` 那次减法）。*/
    {
        int single = 0, burst = 0;
        for (i = 0; i < nfish; ++i) {
            int32_t f = fish[i];
            if (f == 4 || f == 5) fish_to_items(s, f, px, py - 80.0f - 30.0f * (float)single++);
            else                  fish_to_items(s, f, px, py - 80.0f - 30.0f * (float)burst++);
        }
    }

    if (!by_hyper) {
        /* ★ 自然结束：先出**释放动画**（`beastap`），2 只灵等 `ANIMAL_RELEASE_FRAMES` 帧后（release_tick）才爆散
         *   —— 一手 TH17 `0x40f494` 建脚本 0x2c + `release_timer` 到 120 帧才调 `0x40e920`。
         *   用户 2026-09-11：停止用 RE 猜这条音，改为 debug build 的 6 候选试听（§AL39）。*/
#if ANIMAL_DEBUG_KEY
        s->se_probe_idx = 0;
        s->se_probe_left = ANIMAL_SE_PROBE_N;
        s->se_probe_gap = 0;
        ce_log("wba_wc: SE probe active (%d candidates, 30 frames apart); normal roar-end SE 0x%02x suppressed",
               ANIMAL_SE_PROBE_N, (unsigned)ANIMAL_SE_ROAR_END);
#else
        ce_play_sound(ANIMAL_SE_ROAR_END, px);
#endif
        release_spawn(s);
        ce_log("wba_wc: hyper end #%d (normal; %d fish -> items; release anim %d, burst in %d frames)",
               ++s->ends, nfish, (int)s->release_anim, (int)ANIMAL_RELEASE_FRAMES);
    } else {
        ce_log("wba_wc: hyper end #%d (by hyper; %d fish -> items, no release)", ++s->ends, nfish);
    }

    if (animal_clear_and_restore(&s->core)) on_ready(s);
}

#if ANIMAL_DEBUG_KEY
/* 正常咆哮退出后的 6 候选试听。第一条在结束当帧播，之后每 30 帧一条；对话暂停时冻结。 */
static void se_probe_tick(animal_card_t *s, int32_t paused)
{
    if (!s || s->se_probe_left <= 0 || paused) return;
    if (s->se_probe_gap > 0) { --s->se_probe_gap; return; }
    {
        int idx = s->se_probe_idx;
        if (idx < 0 || idx >= ANIMAL_SE_PROBE_N) {
            s->se_probe_left = 0;
            return;
        }
        ce_play_sound(s_animal_se_probe[idx].id, ce_player_x());
        ce_log("wba_wc: SE probe %d/%d id 0x%02x (%s)",
               idx + 1, ANIMAL_SE_PROBE_N,
               (unsigned)s_animal_se_probe[idx].id, s_animal_se_probe[idx].name);
        ++s->se_probe_idx;
        --s->se_probe_left;
        s->se_probe_gap = 30;
    }
}
#endif

/* —— 收集 —— */

static void token_collect(animal_card_t *s, animal_token_t *t)
{
    int rc;
    ce_play_sound(ANIMAL_SE_COLLECT, t->x);
    ce_anm_interrupt((uint32_t)t->id, 1);          /* TH17：interrupt 1 = 吸收动画 */
    t->collecting = 1;
    t->age = 0;

    rc = animal_collect(&s->core, t->type);
    if (rc == 0 && s->core.ready) {
        on_ready(s);
    } else if (rc == 1) {
        ce_log("wba_wc: hyper extended (%d/%d frames left)", (int)s->core.time_left, (int)s->core.duration);
    } else if (rc == 3) {
        const float *pp = ce_player_pos();
        fish_to_items(s, t->type, pp ? pp[0] : t->x, pp ? (pp[1] - 80.0f) : t->y);
        ce_log("wba_wc: fish %d -> items immediately (hyper active)", (int)t->type);
    }
}

/* —— 每帧推进所有动物灵 —— */

static void tokens_tick(animal_card_t *s, int32_t paused)
{
    const float *pp = ce_player_pos();
    float px = pp ? pp[0] : 0.0f, py = pp ? pp[1] : 0.0f, pz = pp ? pp[2] : 0.0f;
    int i = 0;

    while (i < s_n_tok) {
        animal_token_t *t = &s_tok[i];
        float dx, dy, d2, slow = 1.0f;

        if (t->collecting) {                        /* 吸收：朝自机飞，约 10 帧后出列 */
            t->vx = (px - t->x) * ANIMAL_ABSORB;
            t->vy = (py - t->y) * ANIMAL_ABSORB;
            t->vz = (pz - t->z) * ANIMAL_ABSORB;
            t->x += t->vx; t->y += t->vy; t->z += t->vz;
            ++t->age;
            if (t->id) ce_anm_set_pos((uint32_t)t->id, t->x, t->y, t->z);
            if (t->age > ANIMAL_ABSORB_FRAMES) { token_remove(i); continue; }
            ++i; continue;
        }

        dx = t->x - px; dy = t->y - py;
        d2 = dx * dx + dy * dy;

        /* ★ 用户 2026-09-10（wiki 同口径）：对话中动物灵**照常运动 / 变色 / 可被收集**，
         *   只是**不会消失**（下面的离屏出列在 paused 时跳过）。原来整只冻结是错的。*/

        if (d2 < ANIMAL_COLLECT_R2 && t->age >= ANIMAL_TOKEN_GRACE) {   /* 24 px 且过了出生宽限：收 */
            token_collect(s, t);
            ++i; continue;
        }
        if (d2 <= ANIMAL_SLOW_R2) slow = 0.5f;      /* 60 px：减速 */

        if (t->long_life) {                         /* 变色：远离时倒计时，靠近冻结 */
            if (d2 <= ANIMAL_SLOW_R2) {
                if (!t->near && t->id) ce_anm_interrupt((uint32_t)t->id, 8);
                t->near = 1;
                if (t->life < ANIMAL_FLICKER_LEAD) t->life = ANIMAL_LIFE_NEAR;
            } else {
                /* TH17 `0x410c25`：离开 60px 时按剩余寿命发 2（<=60，闪烁）或 3（>60，离开动画）。 */
                if (t->near && t->id)
                    ce_anm_interrupt((uint32_t)t->id,
                                     t->life <= ANIMAL_FLICKER_LEAD ? 2 : 3);
                t->near = 0;
                if (t->life > 0) --t->life;
            }
            /* ★ TH17 `0x410c8c`：寿命正好剩 60 帧时，**每个** token 都发 interrupt 2（闪烁）——
             * 当前 bug 只在离开 60px 的瞬间发，导致没靠近过的 token 切换前不闪。
             * 靠近冻结在 60 帧时每帧重复发；TH17 原样，日志只记第一次以便证伪。 */
            if (t->life == ANIMAL_FLICKER_LEAD && t->id) {
                ce_anm_interrupt((uint32_t)t->id, 2);
                if (!t->flicker_logged) {
                    t->flicker_logged = 1;
                    ce_log("wba_wc: token flicker (was near=%d)", t->near);
                }
            }
            if (t->life <= 0) token_change_type(t);
        }

        /* 离屏（TH17 `0x410c8c` 段）：|x| > 208 或 |y−224| > 240 → 计时，> 119 帧出列。
         * 对话中不出列（wiki：「在对话状态时动物灵道具不会消失」）。*/
        if (t->x > ANIMAL_OFF_X || t->x < -ANIMAL_OFF_X ||
            t->y > 224.0f + ANIMAL_OFF_DY || t->y < 224.0f - ANIMAL_OFF_DY) {
            if (!paused && ++t->off > ANIMAL_OFF_FRAMES) { token_remove(i); continue; }
        } else {
            t->off = 0;
        }

        /* DVD 式弹跳（TH17 `0x410c8c` 段）：版边翻符号 + 速度下限，保证斜向运动 */
        if (t->age < ANIMAL_BOUNCE_MAX_AGE) {
            if (t->x <= -ANIMAL_BOUNCE_X && t->vx < 0.0f) t->vx = -t->vx;
            if (t->x >= ANIMAL_BOUNCE_X && t->vx > 0.0f) t->vx = -t->vx;
            if (t->y <= ANIMAL_BOUNCE_Y0 && t->vy < 0.0f) t->vy = -t->vy;
            if (t->y >= ANIMAL_BOUNCE_Y1 && t->vy > 0.0f) t->vy = -t->vy;
            if (t->vx > -ANIMAL_SPEED_MIN && t->vx < ANIMAL_SPEED_MIN) {
                float m = ANIMAL_SPEED_FLOOR + (float)(ce_rand() % 301) * 0.001f;
                t->vx = (t->vx < 0.0f) ? -m : m;
            }
            if (t->vy > -ANIMAL_SPEED_MIN && t->vy < ANIMAL_SPEED_MIN) {
                float m = ANIMAL_SPEED_FLOOR + (float)(ce_rand() % 301) * 0.001f;
                t->vy = (t->vy < 0.0f) ? -m : m;
            }
        }

        t->x += t->vx * slow;
        t->y += t->vy * slow;
        t->z += t->vz * slow;

        ++t->age;
        if (t->age == ANIMAL_DIM_AGE && t->id) ce_anm_interrupt((uint32_t)t->id, 9);   /* 变暗 */
        if (t->id) ce_anm_set_pos((uint32_t)t->id, t->x, t->y, t->z);
        ++i;
    }
}


/* —— 相位 / 死亡掉落（TH17 `BossItem` 对齐）——
 * 一手：TH17 `default.ecl` 的 `BossItem(A,B,C)` = `dropClear; dropExtra(1,$B); dropExtra(2,$C);
 * dropArea; dropMain($A); dropItems`，**一次调用只发一颗主掉落**。
 *   · 符卡相位（`BossCard1` 等）以 `@BossItem(33,0,0)` 结束 → item33 = 随机 token 1..3（1 只动物灵）；
 *   · 普通 / 血条相位（`Boss2` 等）以 `@BossItem(20,30,10); @BossItem(19,30,10)` 结束 →
 *     item20 = token5（残机碎片灵）、item19 = token4（炸弹碎片灵），共 **2 只资源灵**。
 * TH18 的相位 `BossItem` 事件由 BP_ce_enemy_drop（on_enemy_drop_pre）先到；若某相位没有
 * dropItems（例如 st07bs 的 BossDead 只有 delete），则由 phase_sig 边界 / boss 消失兜底补发。
 * 判“符卡”用引擎的 SPELLCARD+0x78 bit0（在本相位期间亮过就锁存；`ce_spell_active`），
 * 判“死亡”用 ECL 当前子程序名含 "Dead"（BossDead / MBossDead；一手上下文偏移见
 * `animal_enemy_sub_name`，与 lolk.c 的 `enemy_ecl_sub` 同源）。*/

static void animal_drop_resource_pair(animal_card_t *s, float x, float y,
                                      const char *tag, int old_sig, int new_sig)
{
    (void)token_spawn(4, 0, x, y);
    (void)token_spawn(5, 0, x, y);
    ++s->drops;
    if (old_sig || new_sig)
        ce_log("wba_wc: %s -> 2 resource spirits (4+5) (sig %d -> %d)", tag, old_sig, new_sig);
    else
        ce_log("wba_wc: %s -> 2 resource spirits (4+5)", tag);
}

static void animal_drop_phase_animal(animal_card_t *s, float x, float y, int old_sig, int new_sig)
{
    int32_t t = 1 + (int32_t)(ce_rand() % 3);
    (void)token_spawn(t, 1, x, y);
    ++s->drops;
    if (old_sig || new_sig)
        ce_log("wba_wc: phase drop spell -> 1 animal spirit (sig %d -> %d, type %d)",
               old_sig, new_sig, (int)t);
    else
        ce_log("wba_wc: phase drop spell -> 1 animal spirit (type %d)", (int)t);
}

/* 每帧跟踪 boss 相位（phase_sig 只在有活动中断槽时更新；旧相位 kind 锁存到 s->phase_is_*）。
 * 返回 1 = 本帧处理了一个相位边界。掉落发在边界那帧，位置取新相位 boss 坐标。 */
static int animal_phase_track(animal_card_t *s)
{
    uint8_t *be = animal_boss_enemy();
    int32_t sig = 0;
    int is_mid = 0;

    if (!be) {
        /* 无 dropItems 的死亡（例如 st07bs BossDead 直接 delete）：兜底补 2 资源灵。
         * 只在最后看到的 ECL 子程序名含 "Dead" 且本相位没有 drop 事件时发，避免 BossEscape /
         * 关卡转场时误掉。 */
        if (s->boss_seen && s->boss_dead_sub && !s->death_dropped && !s->phase_drop_seen) {
            animal_drop_resource_pair(s, s->boss_x, s->boss_y, "boss death", 0, 0);
            s->death_dropped = 1;
        }
        s->boss_seen = 0;
        s->phase_sig = 0;
        s->phase_is_spell = 0;
        s->phase_is_mid = 0;
        s->phase_drop_seen = 0;
        s->death_dropped = 0;
        s->boss_dead_sub = 0;
        return 0;
    }

    s->boss_seen = 1;
    {
        const float *bp = ce_enemy_pos(be);
        if (bp) { s->boss_x = bp[0]; s->boss_y = bp[1]; }
    }
    {
        int32_t sub = -1;
        const char *name = animal_enemy_sub_name(be, &sub);
        s->boss_dead_sub = (name && strstr(name, "Dead")) ? 1 : 0;
    }

    if (!animal_phase_sig(be, &sig, &is_mid)) return 0;   /* 空档：保留旧 sig / kind */

    if (!s->phase_sig) {
        /* 第一次看到活动槽：只登记，不在 boss 出场瞬间掉东西。 */
        s->phase_sig = sig;
        s->phase_is_spell = ce_spell_active() ? 1 : 0;
        s->phase_is_mid = is_mid;
        s->phase_drop_seen = 0;
        return 0;
    }

    if (sig != s->phase_sig) {
        int32_t old_sig = s->phase_sig;
        int old_spell = s->phase_is_spell;
        int old_mid = s->phase_is_mid;
        const float *bp = ce_enemy_pos(be);

        if (!s->phase_drop_seen && !old_mid && bp) {
            if (old_spell)
                animal_drop_phase_animal(s, bp[0], bp[1], old_sig, sig);
            else
                animal_drop_resource_pair(s, bp[0], bp[1], "phase drop normal", old_sig, sig);
        }

        s->phase_sig = sig;
        s->phase_is_spell = ce_spell_active() ? 1 : 0;
        s->phase_is_mid = is_mid;
        s->phase_drop_seen = 0;
        return 1;
    }

    if (ce_spell_active()) s->phase_is_spell = 1;   /* 符卡位在相位中途才亮：锁存 */
    return 0;
}

/* —— SDK 回调 —— */

static void on_enemy_drop_pre(ce_card_t *c, int32_t *counts)
{
    animal_card_t *s = st(c);
    const uint8_t *e;
    const float *ep;
    int is_boss;
    const char *sub;
    int32_t sub_idx = -1;
    int is_death;

    if (!s || !counts) return;
    e = (const uint8_t *)counts - CE_ENEMY_DROP_COUNTS - CE_ENEMY_DROP;
    is_boss = enemy_is_boss(e);
    ++s->kill_count;
    if (!is_boss) return;                        /* 杂兵不掉动物灵（TH17 杂兵只有 item 1/2/3）*/
    ep = ce_enemy_pos(e);
    if (!ep) return;

    /* TH18 `dropItems` 对 TH17 的每一条 `BossItem` 触发一次。先按 ECL 当前子程序名分**死亡**：
     * `BossDead` / `MBossDead` 是真正的“boss/midboss death”，一律 2 资源灵；
     * 其余（`Boss2` / `BossCard1` …）是相位结束，按锁存的相位 kind 发（符卡 1 动物 / 普通 2 资源）。
     * 相位结束后 `on_tick_2` 还会看到新的 phase_sig；`phase_drop_seen` 防止同一相位重发。*/
    sub = animal_enemy_sub_name(e, &sub_idx);
    is_death = (sub && strstr(sub, "Dead")) ? 1 : 0;
    if (is_death) {
        if (s->death_dropped) return;            /* 同一死亡阶段多条 BossItem 只结一次 */
        animal_drop_resource_pair(s, ep[0], ep[1], "boss death", 0, 0);
        s->death_dropped = 1;
        s->phase_drop_seen = 1;                  /* 最后相位由本事件结账，兜底不再补 */
        return;
    }

    if (s->phase_drop_seen) return;              /* 同一相位多条 BossItem 只结一次 */
    s->phase_drop_seen = 1;                      /* 本相位已有 drop 事件，边界不再重复发 */
    if (s->phase_is_mid) return;                 /* 道中 boss 的中间相位不掉（与 phase_sig 的 'M' 过滤一致）*/
    if (s->phase_is_spell)
        animal_drop_phase_animal(s, ep[0], ep[1], s->phase_sig, s->phase_sig);
    else
        animal_drop_resource_pair(s, ep[0], ep[1], "phase drop normal", s->phase_sig, s->phase_sig);
}

static int on_death_before_deathbomb(ce_card_t *c)
{
    animal_card_t *s = st(c);
    if (!s) return 0;
    /* ★ 先把**中弹那一刻**的自机坐标记下来：真死掉落要用它。
     *   `on_death_frame2` 是 respawn 之后的钩子，那时 `ce_player_pos()` 已经是复活点 ⇒
     *   2026-09-10 用户：「撞弹生成的动物灵似乎是在角色 respawn 之后才生成」+「会直接收取」。*/
    {
        const float *pp = ce_player_pos();
        if (pp) { s->death_x = pp[0]; s->death_y = pp[1]; s->death_valid = 1; }
    }
    if (!s->core.ready || s->core.hypering) return 0;
    /* ★ 救援层报到（用户 2026-09-12 定案：顺序 107 → 117 → 决死 B → 113）。
     * ⚠️⚠️ **返回值必须丢弃**：用户同日重申「**107 和 117 同时触发的时候，得要 107 和 117 同时破除**」
     *   ⇒ 本卡**无条件** `animal_hyper_start()`（`st->ready = 0`），不许因为闩锁被别人拿了就不破。
     * 闩锁的唯一消费者是 113（最后一家）。把这里改成 `if (claim(...))` 就是独占救援 = 违规。*/
    (void)ce_rescue_note_claim(ANIMAL_ID_NUM);
    hyper_begin(s, 1);
    return 0;   /* 返回值被 Player__die 忽略；救命靠 cancel_impending_death 的副作用 */
}

/* 真死（+0x14，死亡结算之后）→ 掉 2 个**不变色**的动物灵，与自机兽灵同种（用户口径）。 */
static int on_death_frame2(ce_card_t *c)
{
    animal_card_t *s = st(c);
    const float *pp;
    int32_t type;

    if (!s) return 0;
    pp = ce_player_pos();
    type = 1 + (int32_t)(*(const uint32_t *)CE_ADDR_CHARACTER % 3);
    {
        /* ★ 用 `on_death_before_deathbomb` 存下的**中弹点**（不是 respawn 后的自机位），
         *   并且左右各放 32 px（> 24 px 的收取半径）—— 否则会在复活点被当场收走。*/
        float bx = s->death_valid ? s->death_x : (pp ? pp[0] : 0.0f);
        float by = s->death_valid ? s->death_y : (pp ? pp[1] : 0.0f);
        /* 基座 = 中弹点**正前方** (x, y − 64)，两颗同一基座、靠各自随机初速散开（TH17 `0x40e920` 同款）*/
        (void)token_spawn(type, 0, bx, by - ANIMAL_BURST_DY);
        (void)token_spawn(type, 0, bx, by - ANIMAL_BURST_DY);
        s->death_valid = 0;
    }
    ce_log("wba_wc: miss -> 2 non-cycling animals (type %d)", (int)type);
    return 0;
}

static int on_activate(ce_card_t *c)
{
    animal_card_t *s = st(c);
    if (!s) return ce_refuse("wba_wc", "no state slot");
    if (!s->core.ready) return ce_refuse("wba_wc", "queue not full");
    hyper_begin(s, 0);
    return 0;
}

/* TH17 `player_is_trying_to_bomb` `th17:0x411d40` 在 `HYPER_FLAGS & 6 != 0` 时返回 0 ⇒
 * 咆哮就绪 / 霊撃期间炸弹键只作咆哮动作。TH18 的原版门在 `Player__on_tick__body` 的
 * `0x45c048`（`test BYTE PTR ds:0x4ca434,0x2`）；SDK 新断点 `BP_ce_bomb_gate` 在这里
 * 询问每张卡：返回 1 = 跳过 `do_bomb`，跳到 `0x45c056`。
 * 只在本卡的 `ready || hypering` 时拦；不咆哮时返回 0，原版炸弹照常放。*/
static int on_bomb_gate(ce_card_t *c)
{
    animal_card_t *s = st(c);
    if (!s) return 0;
    if (!(*(const uint32_t *)CE_ADDR_INPUT_PRESSED & 0x002u)) return 0;   /* 只拦这一次 X 上升沿 */
    if (!roaring(s)) { s->bomb_gate_logged = 0; return 0; }
    if (!s->bomb_gate_logged) {
        s->bomb_gate_logged = 1;
        ce_log("wba_wc: bomb suppressed while roaring");
    }
    return 1;
}

static int on_tick_2(ce_card_t *c)
{
    animal_card_t *s = st(c);
    int paused;

    if (!s) return 0;
    ++s->frame;
    s->core.goast = goast_for_deck();       /* TH18 无凭依选择：默认「主凭依灵不存在」；带 25/30 时恒存在 */
    paused = dialogue_open();
    se_volume_fix();

    /* ★ 用户 2026-09-11：TH17 里「咆哮模式」满槽自动成立，**炸弹键（输入位 0x2 = X）**
     *   是把它**用掉**（`do_hyper_cancel(this,0)`：40 帧消弹 + 伤害源 + 60 帧无敌）——
     *   玩家手感上「按 X = 中弹」，区别只是不扣命（一手：`engine/animal/th17/01` §5.2 激活段）。
     *   TH17 在这个状态下会**屏蔽原版炸弹**（`player_is_trying_to_bomb` `0x411d40` 要求
     *   `HYPER_FLAGS & 6 == 0`）；本卡由 `on_bomb_gate` + SDK 断点 `BP_ce_bomb_gate`
     *   （`th18:0x45c048`）在 `ready || hypering` 时拦下原版 `do_bomb`，只让 X 走这段。*/
    if (s->core.ready && !paused && (*(const uint32_t *)CE_ADDR_INPUT_PRESSED & 0x002u))
        hyper_begin(s, 0);

#if ANIMAL_DEBUG_KEY
    {   /* 调试：拍空格 → 画面中央召唤一只随机动物灵（本卡在卡组里才会跑到这里）*/
        int down = ce_key_down(CE_VK_SPACE) ? 1 : 0;
        int ctrl = ce_key_down(CE_VK_CONTROL) ? 1 : 0;
        int shift = ce_key_down(CE_VK_SHIFT) ? 1 : 0;
        if (down && !s->dbg_key_prev) {
            int32_t t = ANIMAL_DEBUG_TYPE_MIN +
                        (int32_t)(ce_rand() % (uint32_t)(ANIMAL_DEBUG_TYPE_MAX - ANIMAL_DEBUG_TYPE_MIN + 1));
            if (ctrl) {
                /* ★ Ctrl+空格 = 立刻把 5 格塞成同一类并进入就绪（类型 1→2→3 循环）——
                 *   只为快速试「咆哮换 shot 组」（狼 / 鹫）与就绪条，不用真的收集 5 颗。*/
                int i;
                s->dbg_type = (s->dbg_type % 3) + 1;
                s->core.fill = 0;
                s->core.ready = 0;
                s->core.hypering = 0;
                memset(s->core.slots, 0, sizeof(s->core.slots));
                for (i = 0; i < ANIMAL_SLOT_MAX; ++i) (void)animal_collect(&s->core, s->dbg_type);
                if (s->core.ready) on_ready(s);
                ce_log("wba_wc: DEBUG force ready type %d (queue %d %d %d %d %d) by CTRL+SPACE",
                       (int)s->dbg_type, (int)s->core.slots[0], (int)s->core.slots[1], (int)s->core.slots[2],
                       (int)s->core.slots[3], (int)s->core.slots[4]);
            } else if (shift) {
                /* Shift+空格 = 按 4→5→6→7→4 循环指定一种资源灵（不用等 RNG 抽到想看的那种）*/
                s->dbg_type = (s->dbg_type < ANIMAL_DEBUG_FISH_MIN || s->dbg_type >= ANIMAL_DEBUG_FISH_MAX)
                              ? ANIMAL_DEBUG_FISH_MIN : s->dbg_type + 1;
                t = s->dbg_type;
                (void)token_spawn(t, 1, ANIMAL_DEBUG_X, ANIMAL_DEBUG_Y);
                ce_log("wba_wc: DEBUG spawn resource type %d at (%.0f, %.0f) by SHIFT+SPACE", (int)t,
                       (double)ANIMAL_DEBUG_X, (double)ANIMAL_DEBUG_Y);
            } else {
                (void)token_spawn(t, 1, ANIMAL_DEBUG_X, ANIMAL_DEBUG_Y);
                ce_log("wba_wc: DEBUG spawn animal type %d at (%.0f, %.0f) by SPACE", (int)t,
                       (double)ANIMAL_DEBUG_X, (double)ANIMAL_DEBUG_Y);
            }
        }
        s->dbg_key_prev = down;
    }
#endif

    /* boss 相位 / 符卡边界 —— 具体判定和掉落规则见 `animal_phase_track` 的注释。
     * 符卡相位结束 → 1 只随机动物灵；普通相位结束 → 2 只资源灵（4+5）。
     * 若 TH18 的 `dropItems` 事件已经在 on_enemy_drop_pre 结过账，这里只推进指纹、不重复掉落。*/
    (void)animal_phase_track(s);

    if (!paused) {
        /* ★ 撒道具的「大量」播音：2 帧一声，播完 n 声（用户 2026-09-10 指名 se_lgods4.wav = SE 0x3f）*/
        if (s->se_left > 0) {
            if (s->se_gap <= 0) { ce_play_sound(ANIMAL_SE_FISH_ITEMS, ce_player_x()); --s->se_left; s->se_gap = 2; }
            else --s->se_gap;
        }
        release_tick(s);        /* ★ 释放动画 / 延迟爆散（对话中冻结，见下）*/
    }

    if (s->core.hypering) {
        const float *pp = ce_player_pos();
        /* 霊撃中不再刷就绪光环（TH17 `do_hyper_cancel` 在起手时就把 0x1e/0x1f/0x20/type+0x28 清掉了）*/
        if (s->strike_id && pp) {
            if (ce_anm_get_vm((uint32_t)s->strike_id)) ce_anm_set_pos((uint32_t)s->strike_id, pp[0], pp[1], pp[2]);
            else s->strike_id = 0;
        }
        hyper_shot_apply(s);    /* 每帧只刷新 anm 兜底 / 请求日志；不改 sht 槽 */
        aura_tick(s);           /* ★ 咆哮期间继续挂自机光环（含「没有动物灵」的彩虹彩蛋）*/
        self_aura_tick(s);
        if (!paused) {          /* ★ 用户 2026-09-10：**进对话时咆哮的计时冻结**（霊撃帧数不走）*/
            hyper_frame(s);
            if (animal_hyper_tick(&s->core)) hyper_end(s, 1);
        }
    } else if (s->core.ready) {
        /* ★ 用户 2026-09-11：满槽**自动**咆哮 —— 兽灵专属效果（狼/鹫换主炮组、獭 3 光球）在就绪期
         *   就生效（TH17 `HYPER_FLAGS` bit1 从 `0x40fb60` 起就是 1；见 `roaring()` 的注释）。*/
        hyper_shot_apply(s);
        orb_tick(s);
        if (!paused) orb_cancel(s);   /* TH17 ready 段：3 光球每帧各消弹 + 小伤害源 */
        aura_tick(s);
        self_aura_tick(s);
        bar_tick(s);
        int dec = paused ? 0 : (1 + player_has_live_bullets());   /* 对话中暂停；停止射击 → 只 −1 */
        while (dec-- > 0) {
            if (animal_ready_tick(&s->core)) { hyper_end(s, 0); break; }
        }
    }

    if (!paused) eagle_beam_tick(s);
    else if (s->beam_id) eagle_beam_kill(s, 1);   /* 对话中不持续刷伤害/消弹 */
#if ANIMAL_DEBUG_KEY
    se_probe_tick(s, paused);
#endif
    tokens_tick(s, paused);
    return 0;
}

/* HUD 全在左下角 5 格 VM 里；VM 的建/删放在 on_draw（零售 `AbilityManager__on_draw` 相位）。*/
static int on_draw(ce_card_t *c)
{
    animal_card_t *s = st(c);
    /* ★ 用户在**主菜单**看到 5 个空槽灵魂（bug3）：`on_draw`（虚表 +0x48）在菜单里也会被调，
     *   而 `on_tick_2`（+0x2c）不会。门：没有自机/游戏线程就整块收掉，不画、也不留 VM。
     * ★ 2026-09-12 复核（本卡 5. 项）：这道门就是先例（115 `lolk.c:706` / 111 `signal.c:556` 同款
     *   `CE_PLAYER() && CE_GAME_THREAD()`），**不需要再改**；槽位表的账不在这里动
     *   （那归 `on_load` 认领 / `on_delete` 里 `ce_layer_drop_card()` 释放）。*/
    if (!s) return 0;
    if (!CE_PLAYER() || !CE_GAME_THREAD()) { hud_clear(s); return 0; }
    hud_sync(s);
    return 0;
}

static void clear_card(ce_card_t *c)
{
    animal_card_t *s = st(c);
    if (!s) return;
    hud_clear(s);
    /* ★ 2026-09-12：HUD **槽位**也在这里退（`CE_CARD` 的 `on_run_reset`/`opdelete` 会先调
     * `ce_layer_drop_card()` → `ce_hud_slot_release()`，这里显式再来一次是幂等的）。
     * 注意 pinned 的 release 语义与货架一致：只标记「保留」，不当场动别人的排布。*/
    ce_hud_slot_release(117);
    s_slot_ok = 0;
    eagle_beam_kill(s, 0);
    if (s->release_anim) { ce_anm_delete((uint32_t)s->release_anim); s->release_anim = 0; }
    s->release_wait = 0;
    aura_kill(s, 1);
    self_aura_kill(s, 1);
    bar_kill(s, 1);
    orb_kill(s, 1);
    if (s->strike_id) { ce_anm_delete((uint32_t)s->strike_id); s->strike_id = 0; }
    token_clear_all();
    memset(&s->core, 0, sizeof s->core);
    s->phase_sig = 0;
    s->phase_is_spell = 0;
    s->phase_is_mid = 0;
    s->phase_drop_seen = 0;
    s->death_dropped = 0;
    s->boss_seen = 0;
    s->boss_dead_sub = 0;
    s->bomb_gate_logged = 0;
    s->shot_set_logged = 0;
    s->shot_index_logged = 0;
    s->shot_index_req_logs = 0;
    s->shot_index_none_logged = 0;
    s->roar_start_frame = 0;
    s->beam_id = 0;
    s->beam_logged = 0;
    s->se_probe_left = 0;
    s->se_probe_idx = 0;
    s->se_probe_gap = 0;
    s->orb_spawn_logged = 0;
    s->orb_log_last = 0;
    s->cancel_yield_logged = 0;      /* 消弹让位日志（§5.2 细则 2）*/
}

/* ★ 2026-09-12：HUD 槽位的认领放**每关开场**（`on_load`，虚表 +0x20）。
 * 为什么必须放这里而不是只在 `ctor`：槽位带「关卡纪元」——`ce_hud_slot_claim_pinned_card()`
 * 在认领前把 `CE_CURRENT_STAGE()` 推进去，关卡号一变 SDK 就清空整张表 ⇒ 不重认领的话
 * 本关 `ce_hud_slot_get()` 取不到（只能走 `hud_slot_sync` 的兜底常量，账就不对了）。
 * ⚠ `on_load` 不是「开局一次」——它由 `AbilityManager__notify_cards_on_load` 在**每关结束的
 *   商店/结算**里都会跑（见 season.c 第二十六轮的记录）⇒ 这里只做幂等的登记，**不碰任何状态**
 *   （状态只在 `on_run_reset` / `on_delete` 清）。*/
static int on_load(ce_card_t *c)
{
    animal_card_t *s = st(c);
    if (!s) return 0;
    hud_slot_claim();
    return 0;
}

static void on_run_reset(ce_card_t *c) { clear_card(c); }
static void on_delete(ce_card_t *c) { clear_card(c); }

CE_CARD(117,
        .active_recharge = 1,
        .on_activate = on_activate,
        .on_bomb_gate = on_bomb_gate,
        .on_shot_set_index = on_shot_set_index,
        .on_tick_2 = on_tick_2,
        .on_draw = on_draw,
        .on_enemy_drop_pre = on_enemy_drop_pre,
        .on_death_before_deathbomb = on_death_before_deathbomb,
        .on_death_frame2 = on_death_frame2,
        .on_load = on_load,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
