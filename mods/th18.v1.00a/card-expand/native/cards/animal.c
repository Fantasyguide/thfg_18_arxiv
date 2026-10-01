/* 鬼形兽（id 117）—— TH17 动物灵 / 咆哮（HYPER）的 TH18 卡侧复刻（Stage 2）。
 *
 * 一手语义：`engine/animal/th17/01-animal-spirits.md`（TH17 v1.00b 全链）。
 * TH18 里**没有** GoastManager / zToken / HYPER_* 残骸（审计 `engine/animal/th18/01-animal-residuals.md`），
 * 所以全部自建，接缝走 card-expand SDK：
 *
 *   动物灵实体   自己的 VM 池（40 只，TH17 `zGoastManager` 上限 0x28），贴图 = 原版 `beast1.png`
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
 *               ★ 2026-09-21 owner 口径（`WORK-PACKAGES.md` §1b：**117 改为非主动卡**）：本卡**不声明**
 *               `.active_recharge` / `.on_activate`，**不占 C 键**；触发只剩两条，全部来自游戏状态
 *               与**引擎掩码内**的键（无新键 ⇒ replay 安全）：满槽自动就绪（`on_ready`）+ **X** 用掉。
 *               口径、五段链与 replay 依据见 `engine/input/th18/01-key-mapping.md` §2/§4a。
 *               （中弹抵消死亡走的是同一段 `hyper_begin`，只是 `by_hit=1`。）
 *   水獭 3 光球  **咆哮就绪期**（非霊撃期）每帧：VM 留在自机位置，脚本 anchorOffset/angleVel 画轨道；
 *               每颗光球按 TH17 `0x40f200`–`0x40f372` 各做一次半径 28 消弹 + 伤害 2 的源。
 *   中弹不扣命  on_death_before_deathbomb：就绪且未霊撃 → 开霊撃 + cancel_impending_death。
 *   正常结束    超时未霊撃 → 队列里的鱼灵换道具 + 自机附近放出 **2 个变色动物灵**；
 *               霊撃结束（X 或中弹）→ 鱼灵照换、**不放**那 2 个（用户 2026-09-10 定案）。
 *   Miss        真死（on_death_frame2）→ 掉 2 个**不变色**的动物灵（与自机兽灵同种）。
 *   消弹契约    ★ 2026-09-13 bug-2(b) 后本卡有 **2 处**圈消弹（水獭 3 光球、霊撃半径圈）：
 *               霊撃半径圈先 `animal_cancel_claim()` 认领「本帧已消过弹」闩锁
 *               （`EVENTS-CONTRACT.md` §5.2 细则 2）：同帧 107 / 111 已先动手就让位
 *               （这一帧不消弹、消 0 颗、如实记一条日志），但状态机 / 伤害源 / 视觉 VM 照旧。
 *               **鹫束原来的 5 圆消弹已按用户要求删除**（TH17 鹫激光不消弹）。
 *   HUD         左下角 5 格灵槽（槽框 = 原生 `beastf.png` sprite34 切图 + 图标 = 槽内 token 的贴图）。
 *
 * ⚠️ 与 TH17 的已知差异（Stage 2 边界，见 CARDS.md / NEXT.md §1v）：
 *   ① ~~不做「同类 == 凭依兽灵 → +480」~~ → **2026-09-10 二次定案已接**（凭依 = 自机序号 %3）；
 *      不做稀有动物灵（8..14）——AL56 的中 boss 稀有主掉落只记日志、不发错种类；
 *   ② ~~暴走咆哮的射击强化未做~~ → **2026-09-11 已接 / 2026-09-12 十二修补全**：狼/鹫 = **fire-time
 *      set index** 重定向（TH17 `火力档 + 10/+15`；不覆盖原版偏移槽 0..9）。数据 =
 *      `assets/sht/append_shooterset.py` 追进的 TH17 WOLF set11..14；**pl01 EAGLE 主炮组留空**，
 *      视觉/伤害改由 `eagle_beam_tick()` 的**卡侧 5 条扇形粗激光**负责（`ability.anm`
 *      script163 = TH17 pl01.anm entry0 sprite29）：**从本体位置**按 TH17 5 行角度
 *      -90°/-75°/-105°/-60°/-120° 各拉一条 **2048 px** 束（用户口径整张 sprite ×4；一手 sprite 长 512）。
 *      **卡侧束只服务魔理沙（pl01）**：灵梦 /
 *      咲夜（pl00/pl02 映射）的鹫是 5 颗 body 追尾弹，数据路径已画对（`ANIMAL_CHAR_MARISA`）。
 *      **装备 option 卡**走的是另一条 `Player__tick_shooters_for_ability_card` 路径，咆哮期间本应由
 *      `on_option_shot_gate`（断点 `BP_ce_option_shot_gate` 0x40a9c3）拦掉；但 2026-09-12 十三修把
 *      该功能默认关了（上一版 3 字节站点被 thcrap 拒绝 → 门回读失败 → 降级路径腐化卡对象），
 *      等实机验证后再开。不开时装备子机仍会打普通弹（已知、非崩坏）；獭的 3 光球已建 VM（视觉）
 *      并在咆哮就绪期做消弹（霊撃起手时照 TH17 删掉）；
 *   ③ 灵槽几何（x=300+36i / y=924，表面 px）是从 TH17 脚本 pos(-360+36i, 872) 估的，🟡 待实跑调；
 *   ④ 「对话中不变色」按「照常变色、但不移动/不消失」实现（用户口径）。 */
#include <string.h>

#include "sdk.h"
#include "animal_core.h"
#include "animal_wave_table.h"  /* AL58c：各关各波各组的静态动态总数（展开 $A-- 循环）*/
#include "broken_core_core.h"   /* bc_atan2f：纯 SSE 的确定性 atan2（bug-1 的 EAGLE 追尾补正用）*/
#include "hud_dim.h"            /* ★ 跨卡统一件：左下角 HUD 压暗（❓ 用户口径，见文件头）*/

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
 *   锚点报账为 (88, 924)。x 就是用户 2026-09-10 按 1280 截图量的 `ANIMAL_HUD_X0`
 *   （整排左移 193 px：281−193 = 88）；y 取**已量过的那条线 924**（`CE_HUD_ANCHOR_Y` 同一档），
 *   slot 的中心 y 由 `+ANIMAL_HUD_DY`（= 904−924 = −20）推回来 ⇒ 逐像素等于迁移前的 `ANIMAL_HUD_Y`。
 *   ⚠️ 实测口径：48×48 的可见框以 904 为心 ⇒ 实占 y **880..928**（比 924 还低 4 px，见 §1.4 的
 *      表）；这里按用户量过的 924 当锚点报账，只差这 4 px 的下沿（不参与任何排布计算）。
 *   ★★ 2026-09-15i 订正（**只改注释，值一个字节没动**）：这个 (88, 924) 在**绘制**里是
 *      **首格的格心**（`hud_sync` 的 `x = s_slot.x + 36·i`、`y = s_slot.y − 20`），**不是**整排的左下角 ⇒
 *      可见整排 = 64..256（见下面 `ANIMAL_HUD_VIS_*` 那张表）。
 *      报给槽位表的块尺寸 193×48 仍是「5 格整排」那一档（= 48 + 4×36 + 1，与 `HUD-LAYOUT §1.4` 的
 *      「动物灵 193×48」对齐）—— 它跟着**声明块**走，不跟着可见整排改（换它 = 改货架/重叠账）。*/
#define ANIMAL_HUD_X0          88.0f   /* ★ 用户 2026-09-10 按 1280 截图量的：整排**左移 193 px**（281−193）*/
#define ANIMAL_HUD_Y          904.0f   /* ★ 同上：**下移 4 px**（900+4）*/
#define ANIMAL_HUD_DX         36.0f
/* ★ 2026-09-12：pinned 锚点 = 整排左下角（表面 px）。x/y 一律由它推：
 *   第 i 格中心 = (pin_x + DX·i, pin_y + ANIMAL_HUD_DY)。*/
#define ANIMAL_HUD_PIN_X      88.0f        /* = ANIMAL_HUD_X0（迁移前的硬编码值，一个像素没动）*/
#define ANIMAL_HUD_PIN_Y     924.0f        /* 已量过的那条底线（= CE_HUD_ANCHOR_Y）*/
#define ANIMAL_HUD_DY        (-20.0f)      /* 格中心 y = pin_y − 20（904 − 924）*/
#define ANIMAL_HUD_BLOCK_W   193.0f        /* **声明给槽位的块宽**：pin_x .. pin_x+193 ⇒ 88..281（值不动）*/
#define ANIMAL_HUD_BLOCK_H    48.0f        /* 同上，块高 48；上沿 876（= 924 − 48）*/
/* ★★ 2026-09-15i：**「声明块」≠「可见整排」** —— 压暗的判据用后者（见 `hud_fade_tick`），
 *   这里先把两份几何各自钉下来（只读推导：`hud_sync` 的 `x = s_slot.x + ANIMAL_HUD_DX·i` + 各 VM 的 sprite 盒）：
 *
 *   | | 表面 px | 来源 |
 *   | --- | --- | --- |
 *   | **声明块**（喂 `ce_hud_slot_claim_pinned_card`）| (88, 924) + 193×48 ⇒ x 88..281、y 876..924 | `ANIMAL_HUD_PIN_*` + `ANIMAL_HUD_BLOCK_W/H`；照 HUD-LAYOUT §1.4 的「动物灵 193×48」 |
 *   | **可见整排**（5 格真的画在哪）| **(64, 928) + 192×48 ⇒ x 64..256、y 880..928** | 见下 |
 *
 *   ⚠ **两者相差 24 px，根子是「88 是首格的格心、不是整排的左沿」**——
 *   `hud_sync` 里 `第 i 格中心 = (s_slot.x + 36·i, s_slot.y − 20) = (88+36i, 904)`：
 *   锚点的 x 落成**首格格心**（`ANIMAL_HUD_PIN_X` 的注释写「整排左下角」，与代码实际用法差**半个格半径**）。
 *   ⇒ 可见整排 = 首格盒 88±24 .. 末格盒 232±24 = **64..256**（宽 = 48 + 4×36 = 192），
 *     下沿 = 格心 904 + 24 = **928**（不是声明块的 924）。
 *   各 VM 的实测内容（`assets/ability/entries/ORDER.txt` 的贴图 + 脚本 145/154/156 的 `originMode(1)`，
 *   中心锚 ⇒ 可见范围 = 格心 ± 图元半宽）：格内图标 `ICON_*` = 64×64 画布、**art bbox 40×40 居中**
 *   ⇒ 单格 68..108（首格）.. 212..252（末格）；空槽灵魂 `SOUL_0` = 16×16（art 仅 6×9）；
 *   满槽火焰 `FLAME_0` = 64×64 画布、`anchor(0,2)`（底边中）且就绪/咆哮时 ×1.5
 *   ⇒ 单格 x ±24（首格 64..112、末格 208..256）、顶边最高到 y 862。
 *   **⇒ 整排内容的实际外包 = x 62..255、y 862..928**（火焰 ×1.5 时比格盒往左 / 往上多出约 1.5 / 18 px），
 *   取**格盒**那份（64..256 / 880..928）当「可见整排」的规范矩形 —— 那 1.5 px 落在自机够不到的一侧
 *   （门的左沿由自机钳位决定），18 px 的顶边也不影响 y 侧的门。见 `hud_fade_tick` 的注。
 *
 *   ⚠ **用户 2026-09-15 口径**：「117 的显示是在游玩区的左下角没问题，**全部落在了游玩区**。它的**左边界**的位置
 *   和 113 116 是基本一致的」—— 父 agent 由此读成「可见整排的左沿 ≈ 表面 248..257」。**本轮实测对不上**：
 *   按上面这套代码/常量/贴图推，可见整排左沿 = **64**（= 88 − 24），右沿 = **256**。
 *   · 与 113（声明块左沿 248）/116（257）**基本一致的其实是本排的「右沿 256」**（= 游玩区左边缘 256/257 那条线）
 *     —— 这是「117 的整排紧贴着 113/116 那一排的左边」的几何，很可能就是那句话看的东西；
 *   · 若用户的「左边界 = 248..257」字面成立，则本排得整个画在 256..449（= 2026-09-10「左移 193」**之前**的
 *     名义位置 `x = 281+36i`）—— 但常量（源码 + 已编译 DLL 的立即数 88/36/−20/193/48 全查过）与绘制代码
 *     都不支持这个读法，且 DLL 里**根本没有 281**。⇒ 已报父 agent（见 `AUDIT.md` §DD-8 与 `NEXT.md` §2D）。*/
/* 可见整排的**尺寸常量**（左沿 / 下沿**刻意不写死** —— 它们每帧从**运行时的槽位值** `s_slot` 推，
 *   见 `hud_fade_tick`；为什么不能从 `ANIMAL_HUD_PIN_X` 直接推，见 `hud_log_geometry` / `hud_slot_sync` 的注）*/
#define ANIMAL_HUD_HALF       24.0f   /* 格半径 = 48/2：格心 − 它 = 格盒左沿（可见左沿）*/
#define ANIMAL_HUD_VIS_W     192.0f   /* 可见整排宽 = 48 + 4×36（5 格 48×48、间距 36）（= 256−64）*/
#define ANIMAL_HUD_VIS_H      48.0f   /* 可见整排高 = 格盒 48（= 928−880）*/
/* ✏️ 对账用的**参考值**（pin = (88,924) 时，仅供文档/人眼比对 —— **不许当判据**）：
 *   可见整排 = (64, 928) + 192×48；等价的右沿 256 / 上沿 880。
 *   真值看日志 `wba_wc: hud geom …` 的 `vis` 那一列（2026-09-15j 起：认领当场一行 + 每 300 帧一行）。*/
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

/* —— HUD 压暗（自机的体量碰到本卡的槽位排 → 整排淡下去）——
 * ★★ 2026-09-15g：**改走跨卡统一件 `cards/hud_dim.{h,c}`**（唯一实现）。
 * 性质 = ❓ **用户口径（UI 统一），不是复刻**（用户 2026-09-15：「压暗这个涉及到 UI 统一，
 * 这个不能复刻。是那个靠近透明吧」）⇒ **动态**统一：终点 alpha **32**（原 64）、起始 255、
 * 过渡 **8 帧线性**、终点 **64**（★ 2026-09-22 换成 TH18 原生一手；历史 255↔32 = ±56/帧、4 帧 → 128 / 20 帧）、滞回带 **16 px**；
 * **区域**按**本卡自己那块 HUD 的矩形**推（见下）。
 *
 * ★★ 2026-09-15h：**进/出的口径订正**（父 agent：15g 那版「进 = 矩形**之内**」裁错了）——
 *   **进 = 自机落在「块矩形外扩 `HUD_DIM_MARGIN`(28)」之内**、**出 = 离开「矩形 + 44」（28 + 滞回 16）之外**。
 *   理由：本块的**下沿 446 在自机 y 钳位 432 之下** ⇒ 「站进矩形」只剩 **10 px 高**；一手各作的门本来就落在
 *   它们自己那块上沿之上 24–32 px（TH16 的 `400` 在它那块沿之上 28）⇒ 边距是**原本就有的语义**。
 *
 * 本卡的**压暗矩形** → 门限（**自机坐标**；换算 `自机 = 表面/2 − (320, 16)`，见 `hud_dim.h`）：
 *   ★★ 2026-09-15i 起 = **可见整排**那份（**不是**声明给槽位的块；两份为什么差 24 px 见上面 `ANIMAL_HUD_VIS_*`）：
 *   · 矩形 = (64, 928) + **192 × 48**（`ANIMAL_HUD_VIS_*`）⇒ 表面 x 64..256、y 880..928
 *   · ⇒ 自机矩形 = x ∈ [−288, −192]、y ∈ [424, 448]（上沿 424 / 下沿 448）
 *   · ⇒ **进** = 自机落在「矩形 + 28」之内：x ∈ [−316, −164] ∧ y ∈ [396, 476]
 *        ⚠ 自机钳位 x ∈ [−184, 184]、y ∈ [32, **432**] ⇒ 实际可触发区 = x ∈ [−184, −164]、y ∈ [396, 432]
 *          （≈ **20 × 36 px**）
 *   · ⇒ **出** = 离开「矩形 + 44」：x < −332 ∨ x > −148 ∨ y < 380 ∨ y > 492（**16 px 滞回带**）
 *   ⚠ **几何事实（要拍板的）**：本排**整排画在游玩区左边缘（256/257）之外**（可见 64..256，自机中心最左只能到 272）
 *     ⇒ 自机**碰不到**它，压暗只在「贴左墙」的那 20 px 里成立。旧口径用声明块（88..281）时是 32.5 px
 *     —— 那个宽度有一半来自「声明块比可见整排右移 24 px」这个记账偏移，**不是**本排真的能被碰到。
 *     要它更早亮：`HUD_DIM_MARGIN`（一个宏、全卡）或**挪本排**（摆位口径，见 `AUDIT.md` §DD-6 第 2 条）
 *     —— 后者不归本轮（父 agent 明令「只换压暗用的矩形」）。见 `AUDIT.md` §DD-8、`TUNING.md` §7.2/§7.4。
 *   ⚠ 旧值 `x < −64 ∧ y > 400` / 淡回 `y < 384` 是**照抄天空璋卡**（本卡注释原话），是 TH16 自己那块
 *     槽的几何；统一后按本卡几何重推，数字因此不同 —— **几何差异，不是新的不统一**。原一手值留档：
 *       `ANIMAL_HUD_FADE_DIM   64`   —— 用户 2026-09-10 口径 = 照抄天空璋卡（TH16 `alphaTime(5,0,64)`）
 *       `ANIMAL_HUD_FADE_STEP  51`   —— 步长（照 TH16 那一套时间序列）
 *       `ANIMAL_HUD_HIDE_X (-64.0f) / HIDE_Y 400.0f / SHOW_Y 384.0f` —— `th16:0x427f41` / `th16:0x427f6c`
 *       `animal_hud_player_near()`   —— 本卡自己那份门函数（已删，改由共享件算）*/

/* ★ 调试键（2026-09-10，用户要求）：拍**空格**时，如果本卡在卡组里，就从画面中央
 * (ECL 坐标 x=0, y=224) 召唤一只随机**动物灵**（type 1..3，长寿命/变色版）。
 * `ce_key_down` 走 `GetKeyboardState`（user32）—— 空格不在游戏配置里、也不进 replay 输入流，
 * 所以只作调试用。发布前把 ANIMAL_DEBUG_KEY 改成 0 即可关掉。*/
#define ANIMAL_DEBUG_KEY      0   /* ★ 2026-09-13 用户口径：发布版关闭调试键（Space/Shift+Space/Ctrl+Space 全部停用）*/
/* ★ 2026-09-12 十一修：正常咆哮退出 SE 试听探针已停用（用户已从 6 候选中回报
 * **第 5 声 = id 0x42**；见下方 ANIMAL_SE_ROAR_END）。代码保留，但默认 0：
 * 不设置试听序列、不打印任何探针日志，正常咆哮退出照常播 ANIMAL_SE_ROAR_END。*/
#define ANIMAL_SE_PROBE       0
/* ★ 调试键出什么（用户口径，随时改这两行）：
 *   2026-09-10 第一版：只出**资源灵**（token 4..7）→ 测鱼灵换道具；
 *   2026-09-10 第二版：只出**狼灵**（token 1）→ 稳定进狼暴走；
 *   2026-09-11 第三版：**随机 1..3**（狼 / 獭 / 鹫）→ 一次跑遍三种暴走；
 *   2026-09-12 第四版（现行，用户口径）：**随机 1..7**（动物灵 1..3 + 鱼灵 4..7）——
 *   一拍空格动物 / 鱼都可能出，鱼灵直接走「收鱼灵 → 换道具」那条路，方便测撒道具音。
 *   日志一行 `DEBUG spawn spirit type N at (0, 224) by SPACE`（N 就是这只的类型）。
 *   Shift+空格仍是资源灵 4..7 循环、Ctrl+空格仍是强制就绪。*/
#define ANIMAL_DEBUG_TYPE_MIN 1
#define ANIMAL_DEBUG_TYPE_MAX 3
/* ★ 2026-09-13 用户口径（**第六版，现行**）：**空格只发动物灵**（type 1..3 = 狼 / 獭 / 鹫）——
 *   钱鱼诊断已完成（第五版曾只发钱鱼 type 7）。Shift+空格（资源鱼 4..7 循环）/ Ctrl+空格（强制就绪）不变。*/
#define ANIMAL_DEBUG_FISH_MIN 4    /* Shift+空格：资源灵 4..7 循环（别跟上面的 MIN/MAX 混用）*/
#define ANIMAL_DEBUG_FISH_MAX 7
#define ANIMAL_DEBUG_X        0.0f
#define ANIMAL_DEBUG_Y        224.0f

/* —— 咆哮**就绪窗口**条（TH17 `beast.anm` script30/31 = 0x1e/0x1f）——
 * TH17：`AnimalManager__on_ready` `th17:0x40fb60`（`0x40fc6a`/`0x40fc74`）建两条 UI VM 存
 * `manager+0x28/+0x2c`；之后每帧（`0x40f0c6` 段）把两条 VM 的坐标写成**自机坐标 ×2**，
 * 剩余时间写进 scale.x（同一族每帧刷 VM 字段）⇒ 条从**两端朝中点**收缩；
 * `do_hyper_cancel` `th17:0x40f6d0` 在霊撃起手时清掉 ⇒ 只在「槽满、还没按 X」的窗口里显示。
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
 * （`assets/sht/append_shooterset.py`），再在 fire-time 只改引擎选中的 index。原生 0..9 一个字节不动，
 * 所以咆哮前已出的弹命中时按旧 set 回读，仍解析到原生 row（不会变自机贴图）。
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
 *     · `eagle_beam_tick()` 在 Z (`CE_INPUT_HELD & 1`) 按住时每帧挂/刷新矩形束：
 *       视觉用本 mod `ability.anm` 的新脚本 163（TH17 `pl01.anm` entry0 sprite29 的 512×32
 *       激光条，来源贴图与 TH17 抽取物逐字节一致），伤害用 `ce_damage_rect`
 *       （**不消弹**：TH17 的鹫激光不消弹，见 2026-09-13 bug-2(b)）。松开 Z / 咆哮结束立即收掉。
 *       ★ 2026-09-12 用户口径：「魔理沙应该是普通子机激光，不是从机体射出来」⇒
 *       **每个自机原生子机（`inner+0x50` / `player+0x670`，4 格，stride 0xf0）各一条**；
 *       子机数为 0（火力 <1.00）时才回退成自机本体的一条（否则看着像死掉）。
 *       原生子机列表/坐标见 `sdk_core.h` 的注释与 `th18:0x45bc90`（引擎每帧把
 *       `player_pos + 聚焦/非聚焦偏移` 写回 `option+0x5c/+0x60`）。*/
#define ANIMAL_SHT_POWER_N     5

/* —— EAGLE 卡侧 5 条扇形粗激光（用户 2026-09-12 定案；2026-09-13 bug-2 修正）——
 * TH17 `pl01.anm` entry0 `sprite29` = (0,288,512,32) 激光条（512 沿束方向、32 宽）。
 * 一手 `pl01.anm` script16（本作脚本 163 逐条照抄语义）：
 *   · `anchor(1,0)` —— `engine/anm/th16/10` §2 的锚点表：h=1 = 左端锚；`rotate(-90°)` 后本地 +x
 *     → 屏幕上。⇒ 留在 VM 的是束的**近端**（自机 / 子机），远端朝外 —— 与青眼 script80 同款画法。
 *   · `scale(4,0) → scaleTime(20,1,4,4)`：TH17 一手只把贴图**宽**（本地 y）拉到 4×（32→128 px）；
 *     长度由引擎每帧写 `+0xa0/512`（`th17:0x44bfa3` 读 `0x4a3aa0`=1/512 写 `vm+0x68`），一手上限 512。
 * ★★ 2026-09-13 bug-2：**视觉缩放 = 整张 sprite 同一个倍数**（用户口径）。唯一旋钮 =
 *    `ANIMAL_EAGLE_BEAM_SCALE`；两个轴共用它 ⇒ 视觉 **512×4 = 2048 px 长**、32×4 = 128 px 宽，
 *    远端顶出画面（用户：旧版 512 的远端结束在画面里，看着短）。
 *    · **用户口径、非 TH17 一手**：用户口述「整体 3 倍」，沿用他已用眼睛认可「直接对了」的宽 4×
 *      ⇒ 取 4.0（长度随之 ×4 = 2048）。想切 3× 只改这一个数（3.0 ⇒ 1536 px）。
 *    · 判定**宽**仍是一手 `pl01a.sht` EAGLE `hit_h = 34`，**不跟着放大**；
 *      判定**长**由 `eagle_beam_tick` 跟当前视觉长度一起走（看得见的束 = 打得到的地方）。
 *    · `MIN_FRAC`/`LOOP` 的「每发外扫」只让 scale.x 从 `SCALE×MIN_FRAC` 涨到 `SCALE`（宽恒 SCALE）；
 *      MIN_FRAC=0.5 ⇒ 最短 1024 px，远端仍出屏。
 * ★ 2026-09-13 bug-2(b)：**删掉消弹**。TH17 的鹫激光不消弹（用户明确要求）；原 `ce_cancel_radius`
 *   的 5 圆近似段连同 `ANIMAL_EAGLE_BEAM_CANCEL_*` 常量一起删除。*/
#define ANIMAL_EAGLE_BEAM_SCALE       4.0f    /* ★ 唯一缩放旋钮：两轴同倍数（3.0 = 用户口述「3倍」）*/
#define ANIMAL_EAGLE_BEAM_BASE_LEN    512.0f  /* sprite29 逻辑长度（一手 TH17 pl01.anm entry0）*/
#define ANIMAL_EAGLE_BEAM_BASE_W      32.0f   /* sprite29 逻辑宽度（一手）*/
#define ANIMAL_EAGLE_BEAM_LEN         (ANIMAL_EAGLE_BEAM_BASE_LEN * ANIMAL_EAGLE_BEAM_SCALE) /* 满长 2048 */
#define ANIMAL_EAGLE_BEAM_HIT_W      34.0f    /* ★ 判定宽度：TH17 pl01a.sht EAGLE hit_h=34，不跟着放大 */
#define ANIMAL_EAGLE_BEAM_SPRITE_W    ANIMAL_EAGLE_BEAM_BASE_LEN  /* anm scale.x 的分母 */
#define ANIMAL_EAGLE_BEAM_DAMAGE     20
#define ANIMAL_EAGLE_BEAM_CAP        300
/* ★ 2026-09-13 bug6(b) 保留：贴图沿束方向往外走 —— TH17 `pl01a.sht` EAGLE 5 行 `rate=15`。
 *   每 SHOT_FRAMES 帧一发，束的视觉长度从 MIN_FRAC 拉到满；LOOP=0 时只拉满一次并保持满长。
 *   贴图本体再按 TH17 script16 的 `scrollX(-1/21)` 沿束向流动（见脚本 163 / 132）。*/
#define ANIMAL_EAGLE_BEAM_TRAVEL      ANIMAL_EAGLE_BEAM_LEN  /* 行程：每发沿束方向推进的像素（= 满长）*/
#define ANIMAL_EAGLE_BEAM_SHOT_FRAMES 15       /* 每发帧数：TH17 pl01a.sht rate=15 */
#define ANIMAL_EAGLE_BEAM_LOOP        1        /* 1 = 每 SHOT_FRAMES 重开一发 */
/* ★ 2026-09-13 二十修（用户口径）：「魔理沙大鹫咆哮按 Z 时激光应当**如收回般（倒放）展开**，
 *   实测是瞬间出现、收回反而正常」⇒ 每发的**前 6 帧**两轴都从 0 涨到满。
 *   6 = 收回侧脚本 `interruptLabel(1)` 的 `scaleTime(6, 1, 1.0f, 0.0f)` 的帧数 ⇒ 展开正是它的倒放。*/
#define ANIMAL_EAGLE_BEAM_GROW_FRAMES 6
/* ★ 2026-09-12 十二修：pl01 的鹫 = 本体发出的 **5 条**散开粗激光（TH17 `pl01a.sht` EAGLE 5 行）。
 *   不再挂到原生子机 / 不再单根本体束；角度表在 `eagle_beam_tick` 上方。*/
#define ANIMAL_EAGLE_BEAM_N          5
#define ANIMAL_EAGLE_BEAM_MAX        ANIMAL_EAGLE_BEAM_N
/* ★ 2026-09-12 十二修：卡侧鹫束只服务**魔理沙（pl01）** —— 它的贴图/行为来自 TH17
 *   pl01 鹫（本体 5 连扇形激光，见 `engine/sht/th17/02` §2.4）；灵梦 / 咲夜（pl00/pl02 映射）
 *   的鹫是 5 颗 body 追尾弹，数据路径（pl00 EAGLE 组）已经画对，**不该**再叠 pl01 的激光条。
 *   `CE_ADDR_CHARACTER` 0..3 = 灵梦 / 魔理沙 / 咲夜 / 早苗（`engine.h` 一手）。*/
#define ANIMAL_CHAR_REIMU            0u
#define ANIMAL_CHAR_MARISA           1u

/* ============================ ★ 2026-09-13（AL59/AL61）咲夜（pl02）EAGLE ============================
 * 用户口径：「大鹫咆哮 = 多个发弹点在前面做 θ=θ0·sin(ωt+θᵢ) 的运动」；
 *   AL61 追加：「**要真发弹**，不要激光式远程伤害判定」。
 *
 * 一手：TH17 没有咲夜的兽灵行 ⇒ 数值全 ❓ 自定。
 * 实现：
 *   · N 个虚拟发弹点：θᵢ(t)=THETA0·sin(OMEGA t + φᵢ)，位置 = 自机 + R·(sinθ,−cosθ)。
 *   · **每个点真发一颗引擎自机弹**：直接调 `Player__shoot_one_bullet`（`th18:0x45e930`，
 *     thiscall(player; packed_id, timer, pos*, option) `ret 0x10`），
 *     packed_id = `(CE_SHT_SET_EAGLE_1 << 8) | row0`；该行在 `pl02.sht` EAGLE 组：
 *     `anm=31`（pl02.anm 追加的紫刀）、`func_on_init=5`、`opt_slot=0/mode=0`、speed=18。
 *     `func_on_init=5`（`th18:0x4612d0`）把 `bullet+0x64` 设成 `player+0x479cc` ⇒ 每发前写 θ。
 *     `PlayerBullet__create` 对 body 弹（opt_slot 低 4 位=0）直接用传入 pos（`th18:0x45e411`）；
 *     弹自带伤害源，`Player__tick_bullets` 正常结算。**这不是 hook**：与 `ce_damage_rect` 同级的
 *     直接引擎调用，无新 `BP_*` / 无 sites.py 改动。
 *   · 主炮路径 `on_shot_set_index` 对咲夜 EAGLE 返回**空的 EAGLE_0** ⇒ 引擎不会自己发这组；
 *     只有卡侧显式发。松 Z / 咆哮结束 / 对话 / 删卡都把**在飞的**这组弹收掉（state=2 + interrupt 1）。
 *   · 内层 sin/cos 走递推（不自带 libm、不出 x87）。纯 SSE、可 replay。
 *   · DPS = 扫荡 N × 每颗伤害 / PERIOD + 正面每颗伤害 / FRONT_PERIOD
 *          = 6 × 20 / 6 + 40 / 2 = 20 + 20 = 40.0（每颗伤害由 sht 行结算）。
 *   · ★ AL65 #4（用户「正面火力太差，加一个正中的口，dps 是总 1/3；原扫荡不动、只削 1/3」）：
 *     正中正面口 = 自机正前贴身 (0, −R)、θ=0（标准角 −π/2 = 正上）、**无摆动**。
 *   · ★ AL66（用户「咲夜大鹫得加强，整体 dps 上升 1/3」）：总 30 → 40。
 *   · ★ AL67（用户「换成 dps 分布扫荡 20+正面 20」）：总 40 不变，火力更多挪到正面。
 *     扫荡 PERIOD 4→6（6×20/6 = 20）；正面从「dmg 20 / 每 2 帧」改成「**dmg 40 / 每 2 帧**」
 *     （→ 20），即选用户方案 (b)：弹流视觉/节奏与 AL65 相同，靠每发伤害翻倍而不是加倍弹数。
 *     ⇒ 扫荡 20 + 正面 20 = **40**。*/
#define ANIMAL_CHAR_SAKUYA           2u
#define ANIMAL_SAKUYA_EAGLE_N         6      /* 发弹点数（扫荡；正中正面另加 1 个，不动 φ 表）*/
#define ANIMAL_SAKUYA_EAGLE_R         16.0f  /* 发弹点到自机的距离；用户口径要贴身（12~20） */
#define ANIMAL_SAKUYA_EAGLE_THETA0    0.6f   /* 方位角振幅（rad，≈34°） */
#define ANIMAL_SAKUYA_EAGLE_OMEGA     0.03f  /* 方位角角速度（rad/帧，周期 ≈ 209 帧） */
/* sin/cos(0.03) 的常数（Python 复核：sin=0.0299955002, cos=0.9995500337）*/
#define ANIMAL_SAKUYA_EAGLE_OMEGA_SIN 0.0299955002f
#define ANIMAL_SAKUYA_EAGLE_OMEGA_COS 0.9995500337f
#define ANIMAL_SAKUYA_EAGLE_PERIOD    6      /* AL67：扫荡每点每 6 帧一颗 ⇒ 6×20/6 = 20 DPS（AL66 曾 4→30）*/
#define ANIMAL_SAKUYA_EAGLE_FRONT_PERIOD 2   /* AL67：正中正面口每 2 帧一颗（节奏不变）*/
#define ANIMAL_SAKUYA_EAGLE_DAMAGE    20     /* 扫荡每颗伤害（sht EAGLE_1 row 0，引擎伤害源结算）*/
#define ANIMAL_SAKUYA_EAGLE_FRONT_DAMAGE 40  /* AL67：正面每颗伤害（sht EAGLE_1 row 1）⇒ 40/2 = 20 DPS */
/* 卡侧发弹用的组/行：EAGLE_1 行 0 = 扫荡 / 行 1 = 正中正面（`pl02.sht` 由 append_shooterset 填；EAGLE_0 空）*/
#define ANIMAL_SAKUYA_EAGLE_BULLET_SET CE_SHT_SET_EAGLE_1
#define ANIMAL_SAKUYA_EAGLE_BULLET_ROW 0
#define ANIMAL_SAKUYA_EAGLE_FRONT_ROW  1
#define ANIMAL_SAKUYA_EAGLE_DPS_TARGET 40.0f  /* AL67：总 = 扫荡 20 + 正面 20（AL66 30+10 的再分配）*/
/* `Player__shoot_one_bullet` `th18:0x45e930`；thiscall + 4 栈参（被调方 ret 0x10）*/
#define CE_FN_PLAYER_SHOOT_ONE_BULLET 0x45e930u

/* ★ AL59：四套新模式的**具名 DPS 目标**（每帧伤害，❓ 自定；算式与对照见 TUNING.md §2.1b）。
 * 三条数据路径的目标**真正的源**在 `assets/sht/append_shooterset.py` 的同名 Python 常量
 * （行参数在那里逐行写、构建期烘进 pl0X.sht）；这里列出的是**同值的 C 侧镜像**，只作文档 / 日志锚，
 * 不参与任何计算 —— 改数值时两处都要改（Python 决定实际行为，这里决定观感口径）。*/
#define ANIMAL_SAKUYA_WOLF_DPS_TARGET  63.3333f  /* = 灵梦 WOLF 一手（append_shooterset.SAKUYA_WOLF_DPS_TARGET）*/
#define ANIMAL_SANAE_WOLF_DPS_TARGET   33.3333f  /* AL65：对齐**灵梦大鹫** 5×20/3（append_shooterset.SANAE_WOLF_DPS_TARGET）*/
#define ANIMAL_SANAE_EAGLE_DPS_TARGET  40.0f     /* AL65：5×96/12（频率再降半、伤害加倍；hit0）*/

/* bug2 对账用：`PlayerBullet+0xac` 的打包 id = `(set << 8) | row`（一手 `th18:0x45ec69`；
 * 同一手语义见 `cards/youkai.c` 的 `YK_PB_PACKED_ID`）。灵梦 / 咲夜 / 早苗的鹫走
 * pl00a 的 EAGLE body 追尾弹（set `0x24..0x27`），这里只打印前 3 条生成日志，
 * 用来证伪「EAGLE 弹根本没发出去 / 没伤害」——有这行就是数据路径活着、初值 dmg = 20。*/
#define ANIMAL_BULLET_PACKED_ID      0xac

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
/* ★ TH17 出场防重叠（一手 `engine/animal/th17/01-animal-spirits.md` §4.1 / `0x410570`）：
 *   若存在已有 token 距离² < 576（24 px）且速度点积 ≥ 0.96（同向），本次角度 += 3.883222 rad，
 *   最多重试 10 次。用户 2026-09-13：早期「只走四个对角线」是**用户观感口径（🟡，非一手硬规则）**，
 *   初速仍随机取四对角线，但**去重叠重试允许离开对角线集**。*/
#define ANIMAL_OVERLAP_R2      576.0f
#define ANIMAL_OVERLAP_DOT     0.96f
#define ANIMAL_OVERLAP_RETRIES 10
#define ANIMAL_OVERLAP_TURN    3.883222f   /* ≈222.5°（TH17 `0x410570` 的 +3.883222 rad）*/
#define ANIMAL_MISS_HEADING_L  (ANIMAL_PI * 0.75f)  /* 135°：vx<0，左 */
#define ANIMAL_MISS_HEADING_R  (ANIMAL_PI * 0.25f)  /*  45°：vx>0，右 */

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
/* ★ 2026-09-13 bug-1：本轮（每关）是否已打过「追尾补正生效」日志 —— 证伪用，`clear_card` 每关清。*/
static int32_t s_homing_logged;

/* —— ★ 2026-09-13 AL55：道中「大怪」击破 → 1 动物灵的**帧内队列** ——
 * 共享死亡事件 `on_enemy_death` 跑在 `Enemy__die` `th18:0x430240` 的死亡栈上；按本仓
 * `DEATH` 的纪律（`ufo.c` / `trance.c` 同款），那里**只登记标量**，不建 ANM VM。
 * 真正的 `token_spawn()`（会 `ce_anm_spawn`）放到 `on_tick_2` 帧末。*/
#define ANIMAL_BIG_DROP_QUEUE 32
typedef struct {
    float   x, y;         /* 死亡时的敌人坐标（后续只读标量；不缓存 enemy 指针）*/
    int32_t hp_cur;       /* zEnemyData.life.current（死亡时通常 ≤ 0，照实记）*/
    int32_t hp_max;       /* zEnemyData.life.maximum */
    int32_t drop_main;    /* zEnemyDrop.main_drop（ECL enmCreate 第 6 参 / dropMain 的当前值）*/
    int32_t sub;          /* ECL 主上下文子程序号（-1 = 读不到；类型/sub id 诊断用）*/
    int32_t sprite_w;     /* ★ 贴图 slot0 原始 sprite 宽（animal_enemy_sprite_size）*/
    int32_t sprite_h;     /* ★ 贴图 slot0 原始 sprite 高 */
    int32_t big_by_size;  /* ★ 1 = sprite 尺寸 ≥ ANIMAL_BIG_SPRITE_MIN_W/H */
    int32_t wave;         /* ★ 绑定的 MainSub 数字（去掉尾字母；-1 = 拿不到）*/
    int32_t wave_src;     /* ★ ANIMAL_WAVE_SRC_*：bound/live/sticky/none */
    int32_t odd;          /* ★ 1 = 本波同族+同尺寸数量 ≤3，或 >3 时相对过半多数派的少数派 */
    int32_t census;       /* ★ 运行时实测：本波该 group 累积成员数（仅日志/兜底）*/
    int32_t wave_total;   /* ★ 判定用的同波总数（静态表或兜底 census）*/
    int32_t group_total;  /* ★ AL58c 静态表：本波该组总数；-1 = 表未命中，走 census 兜底 */
    int32_t veto;         /* ★ ANIMAL_WAVE_VETO_*：整波否决来源（覆盖 size/odd）*/
    int32_t reason;        /* ANIMAL_DROP_REASON_* */
} animal_big_drop_t;

static animal_big_drop_t s_big_drop[ANIMAL_BIG_DROP_QUEUE];
static int32_t s_big_drop_n;
static int32_t s_big_drop_lost;   /* 队列满时丢掉的击破数（诊断；clear_card 每关清）*/
static int32_t s_big_diag_left;        /* AL55 诊断：本关还剩几只「任意死亡」要打印 */
static int32_t s_big_diag_noncir_left; /* ★ 额外保证前 4 只非 Cir/ECir 族的死亡也进样本 */
static int32_t s_wave_bind_fix_logged; /* ★ AL60：sticky→静态表修正日志（每关前 8 条）*/
static int32_t s_stage_script_id;      /* 关卡脚本 enemy_id 缓存（animal_stage_mainsub 用）*/

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
    int32_t burst_type;              /* ★ 2026-09-13 bug-5：本次自然结束咆哮的兽灵类型（1..3 暴走 / 4 混合 /
                                      *   0 未知），hyper_end 在清 core 前捕获，120 帧后 release_tick 用
                                      *   （原先占位字段改名，不改结构大小）*/
    hud_dim_t hud;                   /* ★ 2026-09-15g：压暗状态（闩锁 + alpha）—— 跨卡统一件
                                      * `cards/hud_dim.{h,c}`（原来是 `hud_dim`/`hud_alpha`
                                      * 两个 int32：255 ↔ **64**、每帧 ±51 —— 那是**旧实现**的留档；
                                      * 现值 = 共享件的 64 / 8 帧线性）*/
    int32_t hud_fade_touched;        /* 1 = 上一帧动过灵魂/火焰的 alpha（淡回时要补写一次）*/
    /* ★ 2026-09-15j：原来的 `int32_t hud_slot_ok` **已删** —— 它全仓没有写点（`ce_state` 被 memset 成 0）
     * ⇒ `hud_slot_sync()` 里的 `if (!s->hud_slot_ok)` 恒真、「每帧取最新锚点」是空注释（真洞，见那里）。
     * 现在认领/同步一律用**文件级** `s_slot_ok`（槽位表那本账），卡状态里不再放这个标志。*/
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
    int32_t beam_id[ANIMAL_EAGLE_BEAM_MAX]; /* EAGLE 卡侧矩形束的可视 VM（脚本 163），每条一个槽 */
    int32_t beam_n;                  /* 上次已打印的束数（固定 5；用于首次/重建时补一条 on 日志）*/
    int32_t beam_logged;             /* 本次激活已打印束 on 日志 */
    int32_t beam_age;                /* 本次激活的帧数（展开动画用；新建/kill 时清零）*/
    /* ★ 2026-09-13（AL59/AL61）咲夜 EAGLE（见 ANIMAL_SAKUYA_EAGLE_* 常量块）。
     * AL61 改成**真引擎弹**：不再有卡侧 VM 句柄，弹归引擎池管；这里只留相位/帧数。*/
    int32_t sakuya_age;              /* 本次激活帧数（同时是内层相位步数）*/
    int32_t sakuya_logged;           /* 本次激活已打印参数 / DPS 日志 */
    float   sakuya_phase_s;          /* sin(ω·t)：每帧乘固定旋转矩阵递推（不出 x87）*/
    float   sakuya_phase_c;          /* cos(ω·t)*/
    int32_t eagle_body_logs;         /* ★ bug2 对账：已打印的「EAGLE body 弹生成」日志条数（封顶 3，避免刷屏）*/
    int32_t se_left;                 /* 撒一批道具时还要补播几声 SE（2 帧一声，模拟「每颗各响一声」）*/
    int32_t se_gap;                  /* 距下一声还有几帧 */
    int32_t se_scatter_id;           /* 本段连续撒道具序列播的 SE id（2026-09-12 定案 0x3e；段内不变）*/
    int32_t option_gate_logged;      /* ★ 十二修：本次咆哮已打过「拦掉装备子机普通弹」日志 */
    int32_t retail_laser_kill_logged;/* ★ 2026-09-13：本次咆哮已打印的 kill probe 帧数（前 8 帧，含 n=0） */
    int32_t roar_bullet_logs;        /* ★ 2026-09-13：本次咆哮已打印的 bullet set 普查条数（封顶 8）*/
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
 * ⚠ 2026-09-13 bug-2(b) 后本卡只剩 **2 处**全屏/圈消弹：霊撃期 `hyper_frame`（自机位置那个圈）
 *   与 ready 期 `orb_cancel`（水獭 3 光球）；`eagle_beam_tick` 的鹫束消弹已按用户要求删除。
 *   `animal_cancel_claim` 现在只被 `hyper_frame` 调用（`orb_cancel` 走自己的 `ce_cancel_radius`）。*/
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
 *   彩蛋：**卡组里带着**「弱肉强食之理」(id **25**，早鬼) 或「鬼杰组长的恐吓」(id **34**，吉吊八千慧) 时，
 *   ✅ 2026-09-13 用户看卡图确认：**MONEY(34) = 鬼杰组长的恐吓（吉吊）**、**OPTION_BR(30) = 偶像亲卫队（袿姬）**
 *   （卡图原名 `ability/MONEY_max.png` / `ability/OPTION_BR_max.png`）⇒ 彩蛋比的是 **25 或 34**（00 段曾错写成 30）。
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
        /* ★ 2026-09-13 用户口径：关系 = 「**或**」（两张任一即可）；
         *   成员 = 「弱肉强食之理」(25, 早鬼) 或「鬼杰组长的恐吓」(**34**, 吉吊八千慧) ——
         *   31..35 那一段是 TH17 角色卡，34 = MONEY = 吉吊（`engine/card/th18/08-catalog.md`）；
         *   **30 是袿姬的「偶像亲卫队」**（4 个固定 option 槽 = 4 个亲卫），不参与这个彩蛋。*/
        if (id == 25 || id == 34) return ANIMAL_GOAST_ANY;
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
 *   稀有动物 23..29 在道中 boss 的条件分支里（本卡仍不做；AL56 已把资源灵 4+5 与
*   st05/06 的无条件 token15 接上，稀有只记日志 / 不发错种类）
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

/* ============================ ★ AL58：贴图体型 + 「杂鱼波鹤立鸡群」掉落判据 ==========
 * 用户 2026-09-13 口径（定案原文见 AUDIT §AL57）：
 *   (a) 贴图体型 ≥ enemy.png 右下角那一档（64×64）⇒ 必掉；
 *   (b) 小于阈值的：同波（MainSub 段）内按「族名 + 贴图尺寸」归组，
 *       组内数量 ≤ ANIMAL_ODD_GROUP_MAX（=3）⇒ 判为鹤立鸡群 ⇒ 组内每只都掉；
 *       组内数量 > 3 ⇒ 只有相对同波“过半多数派”不同的少数派掉；纯杂鱼波不掉。
 *
 * 一手度量（2026-09-13）：
 *   · 用户给的原图：/mnt/d/.../data/ANM/enemy/enemy.png（512×512 贴图集）。
 *   · `thanm -l 18 local/th18.v1.00a/dat/enemy.anm` entry0 = enemy/enemy.png：
 *     右下角最大 sprite 组 = sprite96..107（x 384..448, y 320..448），全部 64×64；
 *     script40..44 逐帧引用 sprite96..107（thanm -l 输出：sprite96 行 112、script40 行 939）。
 *   · ECL `anmSetMain(0,S)` 的 S 是 enemy.anm **扁平脚本号**（engine/anm/th18/04 §3/§4）；
 *     S=40 → script40 → sprite96..99 = 64×64 ⇒ GirlD01/GirlD02 命中阈值（st01.ecl.txt:975/1111）。
 *     其余 st01 敌人：GirlB01 S=20 → 48×32；GirlC01 S=35 → 48×48；
 *     GirlRedA01/GirlRedA01n S=5、GirlBlueA01/GirlBlueA01n S=0 → 32×32；
 *     Cir* 走 `anmSetSprite(0,S)`，S=65/68/77/80/56 → 32×32。
 *
 * 运行时取尺寸（不缓存 enemy 指针）：
 *   · 首选：zEnemyData.anm_vm_ids[0]（data+0x124 = enemy+0x1350）→ ce_anm_get_vm()
 *     → zAnmVmPrefix.sprite_size（vm+0x74/+0x78）原始贴图 W×H；
 *   · 退路：zEnemyData.final_sprite_size（data+0x4fd8 = enemy+0x6204）W/H，
 *     这是引擎在 anmSetMain/anmSetSprite K==0 时写的 `vm.sprite_size × vm.scale`。
 *   · `anm_main`（enemy+0x149c = data+0x270）只是**最后一次 anmSetMain(0,S) 的脚本号 S**，
 *     不是 sprite id、不是渲染入口；Cir* 用 anmSetSprite 而 anm_main 保持 0，所以必须读 slot0 VM，
 *     不能只查 anm_main 表。映射链：anm_main(S) → enemy.anm 扁平脚本 S → 脚本内 sprite() 引用
 *     → AnmLoaded sprite 表 (anm+0x11c, 每项 0x44) → sprite w/h；或走上面的 VM 当前值。
 *
 * 波键 = 关卡脚本主上下文当前子程序名里的 MainSub 数字（去掉尾字母）：
 *   zEnemy+0xc = context.current_context；ctx+4 = cur_location.subroutine_index；
 *   zEnemy+0x1218 = file_manager；fm+0x8c = 子程序表（{name,bytecode} 每项 8B）；fm+8 = 表长。
 *   完整一手见 lolk.c 的 `parse_mainsub_number()` / engine/chapter/th18/03 §3c。
 *   MainSub01/01b/01c 同键 = 同一“波”；MainSub02/02b、06/06b 同理。
 *
 * 诊断：每只死亡诊断行增列 `sprite=WxH big_by_size=… wave=… odd=…`；掉落行带 `reason=`。*/
#define ANIMAL_ENEMY_LIFE_MAX        0x6224   /* zEnemyData.life.maximum */
#define ANIMAL_ENEMY_HURTBOX_X       0x133c   /* zEnemyData.hurtbox_size.x */
#define ANIMAL_ENEMY_ANM_MAIN        0x149c   /* zEnemyData.anm_main（data+0x270，脚本号）*/
#define ANIMAL_ENEMY_PARENT_ID       0x6834   /* zEnemy.parent_enemy_id（关卡脚本 = 0；照 lolk.c 一手）*/
#define ANIMAL_ENEMY_ANM_VM0         (CE_ENEMY_DATA + 0x124)   /* zEnemyData.anm_vm_ids[0] = slot0 VM id */
#define ANIMAL_ENEMY_FINAL_SPRITE_W  (CE_ENEMY_DATA + 0x4fd8)  /* zEnemyData.final_sprite_size.x（ExpHP）*/
#define ANIMAL_ENEMY_FINAL_SPRITE_H  (CE_ENEMY_DATA + 0x4fdc)
#define ANIMAL_ANM_VM_SPRITE_SIZE    0x74   /* zAnmVmPrefix.sprite_size（vm+0x74/+0x78）*/
#define ANIMAL_ANM_VM_SCALE          0x54   /* zAnmVmPrefix.scale（备查；本判据用原始贴图尺寸）*/
/* ★★★ 2026-10-01（**owner 要求与 108 对齐**）：阈值 **64 → 48**。
 *   一手 rect（`enemy.anm` entry0 = `enemy.png`，120 sprites，108 侧同一份实测）：
 *   小妖精 **32×32 / 48×32**（高最多 32）、**右上角 `sprite72..87` 的两种「大妖精」= 48×48**、
 *   大蝴蝶 64×64 ⇒ 「两边 ≥ 48」正好卡在「小妖精（高 32）」与「大妖精（48）」之间；
 *   原 64 会把 4 面那种大妖精漏掉（108 侧已按同一条改）。见 `AUDIT.md` §BE #170。*/
#define ANIMAL_BIG_SPRITE_MIN_W      48
#define ANIMAL_BIG_SPRITE_MIN_H      48     /* ★ 2026-10-01：与 108 对齐（见上面那条注释 + §BE #170）*/
#define ANIMAL_ODD_GROUP_MAX         3      /* 用户 2026-09-13：同波同类 ≤3 也叫鹤立鸡群（具名旋钮）*/
#define ANIMAL_ODD_MIN_SAMPLE        1      /* 用户口径「哪怕只有 1 只」；total < 它时不参与统计 */
#define ANIMAL_ODD_MAJORITY_NUM      1      /* 严格过半多数派：max/total > NUM/DEN = 1/2 */
#define ANIMAL_ODD_MAJORITY_DEN      2
#define ANIMAL_BIG_DROP_DY           (-8.0f)
#define ANIMAL_BIG_DIAG_N            16
#define ANIMAL_MAINSUB_PREFIX        "MainSub"
#define ANIMAL_MAINSUB_DIGITS_MAX    6
#define ANIMAL_FAMILY_KEY_MAX        32

#define ANIMAL_DROP_REASON_NONE        0
#define ANIMAL_DROP_REASON_SIZE        1
#define ANIMAL_DROP_REASON_ODD_ONE_OUT 2

/* ★ 整波否决（用户 2026-09-13 口径）：命中后本波所有敌机一律不掉，覆盖 size/odd。
 *   · third     = 用户口中的「第三波」；用户按 1-based 数，所以代码取 0-based wave 2。
 *   · 用户 2026-09-13 更正：此前理解的 last-wave 是误读；道中 boss 后第二波的大蝴蝶要掉，所以不再设
 *     last-wave veto。*/
#define ANIMAL_WAVE_VETO_NONE  0
#define ANIMAL_WAVE_VETO_THIRD 1
#define ANIMAL_THIRD_WAVE_KEY  2   /* 用户 1-based 第三波 = 0-based 2 */

/* ★ AL58d 波号绑定：主上下文在某些时点不是 MainSubNN（实测 wave=-1），死亡瞬间实时读会
 * 让“静态表查不到 + census 键 -1”双双失效。⇒ 每只敌机首次见到时就把当时的波号绑到 enemy_id；
 * 死亡时优先用绑定值。sticky last wave 给“首次见到时已经 -1”的敌机兜底。*/
#define ANIMAL_ENEMY_WAVE_BIND_MAX 256
#define ANIMAL_WAVE_SRC_NONE   0   /* 实时/粘滞都拿不到 */
#define ANIMAL_WAVE_SRC_LIVE   1   /* 首次绑定时实时 MainSub 有效 */
#define ANIMAL_WAVE_SRC_STICKY 2   /* 首次绑定时实时为 -1，用 sticky last wave */
#define ANIMAL_WAVE_SRC_BOUND  3   /* 查表命中已有绑定 */
typedef struct {
    uint32_t id;      /* zEnemy.enemy_id */
    int32_t  wave;    /* 绑定的 MainSub 数字 */
    int32_t  src;     /* ANIMAL_WAVE_SRC_* */
} animal_enemy_wave_bind_t;

static animal_enemy_wave_bind_t s_wave_bind[ANIMAL_ENEMY_WAVE_BIND_MAX];
static int32_t s_wave_bind_n;
static int32_t s_wave_bind_pos;          /* 满表后的环形淘汰位置 */
static int32_t s_sticky_wave = -1;       /* 最近一次有效 MainSub 数字 */

/* ★ AL58 census：按绑定的波号累积「group key → 见过的成员」；只作日志/兜底。*/
#define ANIMAL_WAVE_CENSUS_MAX        256
typedef struct {
    uint32_t id;                      /* zEnemy.enemy_id（去重键）*/
    int32_t  wave;                    /* ★ 敌机绑定的波号，不再用实时 MainSub */
    int32_t  type_id;                 /* anm_main 外观脚本号 */
    int32_t  w, h;                    /* sprite 原始尺寸 */
    char     family[ANIMAL_FAMILY_KEY_MAX]; /* 行为体名（已按 '_' 截断）*/
} animal_wave_census_member_t;

static int32_t s_wave_census_n;           /* 已见成员数 */
static int32_t s_wave_census_lost;        /* 表满后丢掉的成员数（诊断）*/
static animal_wave_census_member_t s_wave_census[ANIMAL_WAVE_CENSUS_MAX];

static const char *animal_drop_reason_name(int32_t reason)
{
    switch (reason) {
    case ANIMAL_DROP_REASON_SIZE:        return "size";
    case ANIMAL_DROP_REASON_ODD_ONE_OUT: return "odd-one-out";
    default:                             return "none";
    }
}

static const char *animal_wave_veto_name(int32_t veto)
{
    switch (veto) {
    case ANIMAL_WAVE_VETO_THIRD: return "third";
    default:                     return "none";
    }
}
static const char *animal_wave_src_name(int32_t src)
{
    switch (src) {
    case ANIMAL_WAVE_SRC_LIVE:   return "live";
    case ANIMAL_WAVE_SRC_STICKY: return "sticky";
    case ANIMAL_WAVE_SRC_BOUND:  return "bound";
    default:                     return "none";
    }
}


/* 旧版族名黑名单只保留给「栈扫描漏掉时按 KILLED 兜底」这一条死因识别用；
 * 道中掉落判据已改为贴图尺寸 + 同波统计，不再看这个前缀表。*/
static const char *const animal_fallback_noncir_prefixes[] = {
    "Cir",
    "ECir",
};

static int animal_name_is_fallback_noncir(const char *name)
{
    size_t i;
    if (!name) return 0;
    for (i = 0; i < sizeof animal_fallback_noncir_prefixes / sizeof animal_fallback_noncir_prefixes[0]; ++i) {
        size_t n = strlen(animal_fallback_noncir_prefixes[i]);
        if (strncmp(name, animal_fallback_noncir_prefixes[i], n) == 0) return 0;
    }
    return 1;
}

static int animal_name_is_stage_script(const char *n)
{
    if (!n) return 0;
    if (strncmp(n, "Main", 4) == 0) return 1;
    return strcmp(n, "main") == 0;
}

static int32_t animal_parse_mainsub_number(const char *n)
{
    int32_t v = 0;
    int i;
    if (!n) return -1;
    if (strncmp(n, ANIMAL_MAINSUB_PREFIX, 7) != 0) return -1;
    n += 7;
    if (*n < '0' || *n > '9') return -1;
    for (i = 0; i < ANIMAL_MAINSUB_DIGITS_MAX && n[i] >= '0' && n[i] <= '9'; ++i) {
        v = v * 10 + (n[i] - '0');
        if (v > 99999) { v = 99999; break; }
    }
    return v;
}

static uint8_t *animal_stage_script_find(uint8_t *em)
{
    uint8_t *e;
    if (!em) return 0;
    if (s_stage_script_id) {
        e = ce_enemy_by_id(em, s_stage_script_id);
        if (e) return e;
        s_stage_script_id = 0;
    }
    CE_ENEMY_FOREACH(em, e) {
        const char *name;
        int32_t sub;
        if (!e) continue;
        if (*(const int32_t *)(e + ANIMAL_ENEMY_PARENT_ID) != 0) continue;
        name = animal_enemy_sub_name(e, &sub);
        if (name && animal_name_is_stage_script(name)) {
            s_stage_script_id = *(const int32_t *)(e + CE_ENEMY_ID);
            return e;
        }
    }
    return 0;
}

static int32_t animal_stage_mainsub(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    const char *name;
    int32_t sub;
    if (!em) return -1;
    e = animal_stage_script_find(em);
    if (!e) return -1;
    name = animal_enemy_sub_name(e, &sub);
    return animal_parse_mainsub_number(name);
}

static int32_t animal_wave_veto(int32_t wave)
{
    if (wave == ANIMAL_THIRD_WAVE_KEY) return ANIMAL_WAVE_VETO_THIRD;  /* 用户 1-based 第三波 */
    return ANIMAL_WAVE_VETO_NONE;
}

static void animal_family_key(const char *name, char *out, size_t cap)
{
    size_t i = 0;
    if (!out || cap == 0) return;
    if (!name) { out[0] = '\0'; return; }
    while (name[i] && name[i] != '_' && i + 1 < cap) {
        out[i] = name[i];
        ++i;
    }
    out[i] = '\0';
}

static int32_t animal_round_px(float v)
{
    if (!(v > 0.0f)) return 0;
    if (v > 100000.0f) v = 100000.0f;
    return (int32_t)(v + 0.5f);
}

/* 逐敌机取贴图尺寸；成功返回 1，W/H 为原始 sprite 尺寸（未乘 scale）。*/
static int animal_enemy_sprite_size(const uint8_t *e, int32_t *out_w, int32_t *out_h)
{
    uint8_t *vm;
    int32_t id, w, h;
    float fw, fh;
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!e) return 0;

    /* ① slot0 当前 VM 的原始 sprite_size（ExpHP zAnmVmPrefix+0x74/+0x78）。*/
    id = *(const int32_t *)(e + ANIMAL_ENEMY_ANM_VM0);
    vm = id ? ce_anm_get_vm((uint32_t)id) : 0;
    if (vm) {
        w = animal_round_px(*(const float *)(vm + ANIMAL_ANM_VM_SPRITE_SIZE));
        h = animal_round_px(*(const float *)(vm + ANIMAL_ANM_VM_SPRITE_SIZE + 4));
        if (w > 0 && h > 0) {
            if (out_w) *out_w = w;
            if (out_h) *out_h = h;
            return 1;
        }
    }

    /* ② 退路：引擎记的 final_sprite_size = sprite_size × scale（anmSetMain/anmSetSprite K==0 写）。*/
    fw = *(const float *)(e + ANIMAL_ENEMY_FINAL_SPRITE_W);
    fh = *(const float *)(e + ANIMAL_ENEMY_FINAL_SPRITE_H);
    w = animal_round_px(fw);
    h = animal_round_px(fh);
    if (w > 0 && h > 0) {
        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        return 1;
    }
    return 0;
}

/* 同波计数只认「非 boss、非关卡脚本、可读到族名和贴图尺寸」的敌人。*/
static int animal_wave_enemy_eligible(const uint8_t *e)
{
    const char *name;
    int32_t sub;
    if (!e || enemy_is_boss(e)) return 0;
    name = animal_enemy_sub_name(e, &sub);
    return name && !animal_name_is_stage_script(name);
}

/* ★ 纯杂鱼波 singleton guard（不是掉落判据）：
 * 波刚开始、玩家秒杀第一只 Cir 时，当前存活表里只有 1 只；若直接套用「同波同类 ≤3 ⇒ 鹤立鸡群」，
 * 会把纯杂鱼波的第一只误判成 odd。用户口径「纯杂鱼波小体型一律不掉」优先，所以当当前敌人
 * 的族名前缀属于成波杂鱼表、且同波所有可计入敌人也都属于该表时，抑制 <=3 的 odd 判定。
 * 只是防误掉；真正决定掉落仍是 AL57 的尺寸 + 同波统计，不会再把非 Cir 一律判掉。*/
static const char *const animal_swarm_guard_prefixes[] = {
    "Cir",
    "ECir",
};

static int animal_name_is_swarm_guard(const char *name)
{
    size_t i;
    if (!name) return 0;
    for (i = 0; i < sizeof animal_swarm_guard_prefixes / sizeof animal_swarm_guard_prefixes[0]; ++i) {
        size_t n = strlen(animal_swarm_guard_prefixes[i]);
        if (strncmp(name, animal_swarm_guard_prefixes[i], n) == 0) return 1;
    }
    return 0;
}

static int animal_wave_bind_lookup(uint32_t id, int32_t *out_wave, int32_t *out_src)
{
    int32_t i;
    for (i = 0; i < s_wave_bind_n; ++i) {
        if (s_wave_bind[i].id == id) {
            if (out_wave) *out_wave = s_wave_bind[i].wave;
            if (out_src)  *out_src = ANIMAL_WAVE_SRC_BOUND;
            return 1;
        }
    }
    return 0;
}

/* ★ AL60 回归修复用的静态表助手：按 (stage, type_id, w, h) 找波。
 * 背景（一手）：st01 的 `MainSub03` 是「spawn 3 只 GirlD01 后**立刻 return**」的同步子程序
 * （`thecl -d 18 st01.ecl`：`@MainSub03()` 内无 `ins_23`；相比之下 `MainSub01/02/06` 内部有等待 ⇒
 * sub 名会保持到卡片 tick）。卡片 `on_tick_2` 在该帧稍后跑到 `animal_wave_census_update` 时，
 * 关卡脚本 sub 名已经回到 `MainFront` ⇒ `animal_stage_mainsub()` 返回 -1 ⇒ 旧代码用 `s_sticky_wave`
 * （= 上一波 2 = 被 third-wave veto 的那一波）登记 GirlD01 ⇒ `wave=2 veto=third drop=0`。
 * 修法：sticky 推断时，若 sticky 波的静态组里**没有**这只敌机的 (type,w,h)，就取 sticky 之后
 * 第一个真正含它的波（波序单调向前，所以「sticky 之后第一个」= 正确波）。*/
static int animal_static_wave_has(int32_t stage, int32_t wave,
                                  int32_t type_id, int32_t w, int32_t h)
{
    size_t i;
    if (stage <= 0 || wave < 0 || w <= 0 || h <= 0) return 0;
    for (i = 0; i < ANIMAL_WAVE_GROUP_TOTAL_COUNT; ++i) {
        const animal_wave_group_total_t *e = &animal_wave_group_totals[i];
        if (e->stage == stage && e->wave == wave && e->type_id == type_id &&
            e->w == w && e->h == h) return 1;
    }
    return 0;
}

static int32_t animal_static_next_wave(int32_t stage, int32_t after_wave,
                                       int32_t type_id, int32_t w, int32_t h)
{
    size_t i;
    int32_t best = -1;
    if (stage <= 0 || w <= 0 || h <= 0) return -1;
    for (i = 0; i < ANIMAL_WAVE_GROUP_TOTAL_COUNT; ++i) {
        const animal_wave_group_total_t *e = &animal_wave_group_totals[i];
        if (e->stage != stage || e->type_id != type_id || e->w != w || e->h != h) continue;
        if (e->wave <= after_wave) continue;
        if (best < 0 || e->wave < best) best = e->wave;
    }
    return best;
}

/* 取/绑定一只敌机的波号：优先已有绑定，其次当前 live MainSub，其次 sticky（+ AL60 静态表修正），最后 -1。
 * 绑定表按 enemy_id 去重；满了环形淘汰最旧项（只记一次日志）。
 * type_id / w / h 由调用方先算好（调用方本来就要用；避免这里再读一次 VM 且防止 VM 未就绪时乱绑）。*/
static int32_t animal_enemy_wave_bind(const uint8_t *e, int32_t live_wave,
                                      int32_t type_id, int32_t w, int32_t h, int32_t *out_src)
{
    uint32_t id;
    int32_t wave, src;
    int32_t idx;

    if (out_src) *out_src = ANIMAL_WAVE_SRC_NONE;
    if (!e) return -1;
    id = *(const uint32_t *)(e + CE_ENEMY_ID);
    if (animal_wave_bind_lookup(id, &wave, out_src)) return wave;

    if (live_wave >= 0) {
        wave = live_wave;
        src = ANIMAL_WAVE_SRC_LIVE;
    } else if (s_sticky_wave >= 0) {
        wave = s_sticky_wave;
        src = ANIMAL_WAVE_SRC_STICKY;
        /* ★ AL60：sticky 只是「上一波」的猜测；用静态组表修正到「sticky 之后第一个含它的波」。*/
        if (!animal_static_wave_has(CE_CURRENT_STAGE(), wave, type_id, w, h)) {
            int32_t nw = animal_static_next_wave(CE_CURRENT_STAGE(), wave, type_id, w, h);
            if (nw >= 0) {
                if (s_wave_bind_fix_logged < 8) {
                    int32_t sub = -1;
                    const char *nm = animal_enemy_sub_name(e, &sub);
                    ++s_wave_bind_fix_logged;
                    ce_log("wba_wc: wave bind sticky-fix: sticky=%d -> wave=%d "
                           "(type=%d sprite=%dx%d sub=%s)",
                           (int)s_sticky_wave, (int)nw, (int)type_id, (int)w, (int)h,
                           nm ? nm : "(null)");
                }
                wave = nw;
            }
        }
    } else {
        wave = -1;
        src = ANIMAL_WAVE_SRC_NONE;
    }

    if (s_wave_bind_n < ANIMAL_ENEMY_WAVE_BIND_MAX) {
        idx = s_wave_bind_n++;
    } else {
        idx = s_wave_bind_pos;
        s_wave_bind_pos = (s_wave_bind_pos + 1) % ANIMAL_ENEMY_WAVE_BIND_MAX;
        if (s_wave_census_lost == 0)
            ce_log("wba_wc: enemy wave bind table full (cap %d) — ring-evicting oldest",
                   ANIMAL_ENEMY_WAVE_BIND_MAX);
    }
    s_wave_bind[idx].id = id;
    s_wave_bind[idx].wave = wave;
    s_wave_bind[idx].src = src;
    if (out_src) *out_src = src;
    return wave;
}

static int animal_wave_census_has_id(uint32_t id)
{
    int32_t i;
    for (i = 0; i < s_wave_census_n; ++i)
        if (s_wave_census[i].id == id) return 1;
    return 0;
}

/* census 里指定波是不是清一色 Cir/ECir 杂鱼（singleton guard 用）。*/
static int animal_wave_census_all_swarm_guard(int32_t wave)
{
    int32_t i, n = 0;
    if (wave < 0) return 0;
    for (i = 0; i < s_wave_census_n; ++i) {
        if (s_wave_census[i].wave != wave) continue;
        if (!animal_name_is_swarm_guard(s_wave_census[i].family)) return 0;
        ++n;
    }
    return n > 0;
}

static void animal_wave_census_reset(void)
{
    s_wave_census_n = 0;
    s_wave_census_lost = 0;
    s_wave_bind_n = 0;
    s_wave_bind_pos = 0;
    s_sticky_wave = -1;
    s_wave_bind_fix_logged = 0;
}

/* 把当前存活且可计入的敌人计入各自绑定波的 census。只在 enemy_id 首次见时登记；
 * 主上下文读不到 MainSubNN 时不再把波号写成 -1，而是用敌机自己的绑定值。
 * 每帧 + 死亡当帧各调一次；census 只作日志/兜底，判定主路径是 AL58c 静态组表。*/
static void animal_wave_census_update(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    int32_t live;

    if (!em) return;
    live = animal_stage_mainsub();
    if (live >= 0) s_sticky_wave = live;

    CE_ENEMY_FOREACH(em, e) {
        const char *name;
        int32_t sub, w, h, type_id, wave, src;
        uint32_t id;
        animal_wave_census_member_t *m;

        if (!animal_wave_enemy_eligible(e)) continue;
        id = *(const uint32_t *)(e + CE_ENEMY_ID);
        if (animal_wave_census_has_id(id)) continue;
        name = animal_enemy_sub_name(e, &sub);
        if (!name) continue;
        if (!animal_enemy_sprite_size(e, &w, &h)) continue;
        type_id = *(const int32_t *)(e + ANIMAL_ENEMY_ANM_MAIN);
        /* ★ AL60：先拿到 (type,w,h)，绑定才能用静态表把 sticky 修正到正确波。*/
        wave = animal_enemy_wave_bind(e, live, type_id, w, h, &src);
        if (wave < 0) continue;

        if (s_wave_census_n >= ANIMAL_WAVE_CENSUS_MAX) {
            if (s_wave_census_lost++ == 0)
                ce_log("wba_wc: wave census full (cap %d) — further members ignored",
                       ANIMAL_WAVE_CENSUS_MAX);
            return;
        }
        m = &s_wave_census[s_wave_census_n++];
        m->id = id;
        m->wave = wave;
        m->type_id = type_id;
        m->w = w;
        m->h = h;
        animal_family_key(name, m->family, sizeof m->family);
    }
}

static int32_t animal_wave_census_total(int32_t wave)
{
    int32_t i, n = 0;
    if (wave < 0) return 0;
    for (i = 0; i < s_wave_census_n; ++i)
        if (s_wave_census[i].wave == wave) ++n;
    return n;
}

static int animal_wave_census_group_count(const char *name, int32_t type_id,
                                          int32_t w, int32_t h, int32_t wave)
{
    char key[ANIMAL_FAMILY_KEY_MAX];
    int32_t i, n = 0;
    if (!name || wave < 0 || w <= 0 || h <= 0) return 0;
    animal_family_key(name, key, sizeof key);
    for (i = 0; i < s_wave_census_n; ++i) {
        const animal_wave_census_member_t *m = &s_wave_census[i];
        if (m->wave != wave) continue;
        if (m->type_id == type_id && m->w == w && m->h == h &&
            strcmp(m->family, key) == 0) ++n;
    }
    return n;
}

static int animal_wave_census_max_group(int32_t wave)
{
    int32_t i, j, max = 0;
    if (wave < 0) return 0;
    for (i = 0; i < s_wave_census_n; ++i) {
        const animal_wave_census_member_t *a = &s_wave_census[i];
        int32_t c = 0;
        if (a->wave != wave) continue;
        for (j = 0; j < s_wave_census_n; ++j) {
            const animal_wave_census_member_t *b = &s_wave_census[j];
            if (b->wave != wave) continue;
            if (a->type_id == b->type_id && a->w == b->w && a->h == b->h &&
                strcmp(a->family, b->family) == 0) ++c;
        }
        if (c > max) max = c;
    }
    return max;
}

/* ★ AL58c 静态组表：按 (stage, wave, anm_main, sprite W×H) 查本波该组的动态总数。
 * 表由 `native/tools/gen_animal_wave_table.py` 从 7 关 ECL 生成，已展开 `$A--`/`deci` 循环。
 * 找不到时返回 -1；运行时退到 census（只作兜底/日志，常规 ECL 敌人都应命中）。*/
static int32_t animal_static_group_total(int32_t stage, int32_t wave,
                                         int32_t type_id, int32_t w, int32_t h)
{
    size_t i;
    if (stage <= 0 || wave < 0 || w <= 0 || h <= 0) return -1;
    for (i = 0; i < ANIMAL_WAVE_GROUP_TOTAL_COUNT; ++i) {
        const animal_wave_group_total_t *e = &animal_wave_group_totals[i];
        if (e->stage == stage && e->wave == wave && e->type_id == type_id &&
            e->w == w && e->h == h) return e->total;
    }
    return -1;
}

/* 同波静态总人数 / 最大组人数；用于“严格过半多数派”。*/
static void animal_static_wave_stats(int32_t stage, int32_t wave,
                                     int32_t *out_total, int32_t *out_max)
{
    size_t i;
    int32_t total = 0, max = 0;
    if (out_total) *out_total = 0;
    if (out_max) *out_max = 0;
    if (stage <= 0 || wave < 0) return;
    for (i = 0; i < ANIMAL_WAVE_GROUP_TOTAL_COUNT; ++i) {
        const animal_wave_group_total_t *e = &animal_wave_group_totals[i];
        if (e->stage != stage || e->wave != wave) continue;
        total += e->total;
        if (e->total > max) max = e->total;
    }
    if (out_total) *out_total = total;
    if (out_max) *out_max = max;
}

/* 鹤立鸡群（same/total/max 由调用方取静态组表；取不到时退 census）：
 *   · 本波该 group 总数 ≤ ANIMAL_ODD_GROUP_MAX（3）⇒ 该组全部掉（用户 2026-09-13 口径）；
 *   · >3 时，只要本波存在“过半多数派”（max*NUM/DEN）且本组不是它 ⇒ 本组掉。
 * 否则不掉。无多数派的纯杂鱼波（10/10、9/9）不会误伤。*/
static int animal_odd_one_out(const char *name, int32_t w, int32_t h,
                              int32_t wave, int32_t same, int32_t total, int32_t max)
{
    if (wave < 0 || !name || w <= 0 || h <= 0) return 0;
    if (total < ANIMAL_ODD_MIN_SAMPLE) return 0;
    if (same <= ANIMAL_ODD_GROUP_MAX) {
        /* 纯杂鱼波的首只/前几只不被 singleton 误判；混合波仍按 odd 掉。*/
        if (animal_name_is_swarm_guard(name) && animal_wave_census_all_swarm_guard(wave)) return 0;
        return 1;
    }
    if (same <= 0) return 0;
    if (max * ANIMAL_ODD_MAJORITY_DEN > total * ANIMAL_ODD_MAJORITY_NUM && same < max) return 1;
    return 0;
}

/* 汇总一只敌人的死亡掉落判据：填 diag/queue 需要的所有标量，返回 1 = 掉。*/
static int animal_enemy_drop_measure(const uint8_t *e, const char *name,
                                     int32_t *hp_cur, int32_t *hp_max, int32_t *drop_main,
                                     int32_t *anm_main, float *hurtbox_x,
                                     int32_t *sprite_w, int32_t *sprite_h,
                                     int32_t *big_by_size, int32_t *wave, int32_t *wave_src,
                                     int32_t *odd, int32_t *census, int32_t *wave_total,
                                     int32_t *group_total, int32_t *veto, int32_t *reason)
{
    int32_t w = 0, h = 0, big = 0, wave_key = -1, is_odd = 0, why = ANIMAL_DROP_REASON_NONE;
    int32_t type_id = 0, live_wave = -1, wave_src_v = ANIMAL_WAVE_SRC_NONE;
    int32_t same = 0, total = 0, max = 0, veto_src = ANIMAL_WAVE_VETO_NONE;
    int32_t census_same = 0, census_total = 0, static_total = -1;

    if (hp_cur)    *hp_cur = 0;
    if (hp_max)    *hp_max = 0;
    if (drop_main) *drop_main = 0;
    if (anm_main)  *anm_main = 0;
    if (hurtbox_x) *hurtbox_x = 0.0f;
    if (sprite_w)  *sprite_w = 0;
    if (sprite_h)  *sprite_h = 0;
    if (big_by_size)*big_by_size = 0;
    if (wave)      *wave = -1;
    if (wave_src)  *wave_src = ANIMAL_WAVE_SRC_NONE;
    if (odd)       *odd = 0;
    if (census)    *census = 0;
    if (wave_total)*wave_total = 0;
    if (group_total)*group_total = -1;
    if (veto)      *veto = ANIMAL_WAVE_VETO_NONE;
    if (reason)    *reason = ANIMAL_DROP_REASON_NONE;

    if (!e) return 0;
    if (hp_cur)    *hp_cur = *(const int32_t *)(e + ANIMAL_ENEMY_LIFE_CUR);
    if (hp_max)    *hp_max = *(const int32_t *)(e + ANIMAL_ENEMY_LIFE_MAX);
    if (drop_main) *drop_main = *(const int32_t *)(e + CE_ENEMY_DROP);
    if (anm_main)  *anm_main = *(const int32_t *)(e + ANIMAL_ENEMY_ANM_MAIN);
    type_id = anm_main ? *anm_main : *(const int32_t *)(e + ANIMAL_ENEMY_ANM_MAIN);
    if (hurtbox_x) *hurtbox_x = *(const float *)(e + ANIMAL_ENEMY_HURTBOX_X);

    if (!animal_enemy_sprite_size(e, &w, &h)) return 0;
    if (sprite_w) *sprite_w = w;
    if (sprite_h) *sprite_h = h;
    big = (w >= ANIMAL_BIG_SPRITE_MIN_W && h >= ANIMAL_BIG_SPRITE_MIN_H);
    if (big_by_size) *big_by_size = big;

    /* ★ 死亡瞬间“本敌机绑定波”优先；live 只负责更新 sticky + 首次登记。*/
    live_wave = animal_stage_mainsub();
    if (live_wave >= 0) s_sticky_wave = live_wave;
    wave_key = animal_enemy_wave_bind(e, live_wave, type_id, w, h, &wave_src_v);
    if (wave)      *wave = wave_key;
    if (wave_src)  *wave_src = wave_src_v;
    veto_src = animal_wave_veto(wave_key);
    if (veto) *veto = veto_src;

    if (name && wave_key >= 0) {
        int32_t stage = CE_CURRENT_STAGE();
        census_same = animal_wave_census_group_count(name, type_id, w, h, wave_key);
        census_total = animal_wave_census_total(wave_key);
        static_total = animal_static_group_total(stage, wave_key, type_id, w, h);
        if (static_total >= 0) {
            same = static_total;
            animal_static_wave_stats(stage, wave_key, &total, &max);
        } else {
            /* 表里没有（非 ECL 敌 / 未收录的 stage）：退运行时 census 兜底，日志会写 group_total=-1。*/
            same = census_same;
            total = census_total;
            max = animal_wave_census_max_group(wave_key);
        }
    }
    if (census)     *census = census_same;
    if (wave_total) *wave_total = total;
    if (group_total)*group_total = static_total;

    if (!big && name && wave_key >= 0)
        is_odd = animal_odd_one_out(name, w, h, wave_key, same, total, max);
    if (odd) *odd = is_odd;

    if (veto_src != ANIMAL_WAVE_VETO_NONE) why = ANIMAL_DROP_REASON_NONE;
    else if (big) why = ANIMAL_DROP_REASON_SIZE;
    else if (is_odd) why = ANIMAL_DROP_REASON_ODD_ONE_OUT;
    if (reason) *reason = why;
    return why != ANIMAL_DROP_REASON_NONE;
}

static const char *animal_death_kind_name(uint32_t kind)
{
    switch (kind) {
    case CE_EDEATH_KILLED:    return "KILLED";
    case CE_EDEATH_BOMB_KILL: return "BOMB_KILL";
    case CE_EDEATH_ECL_DIE:   return "ECL_DIE";
    case CE_EDEATH_KILL_ALL:  return "KILL_ALL";
    default:                  return "OTHER";
    }
}

/* 死因 / REAL 位不够用时，用「KILLED/BOMB_KILL + 可读的具名 ECL 敌人」兜底。
 * 只排除关卡脚本（Main 前缀 / main）和 `Ecl_` 特效敌；这比旧版的 `sub >= 0 && HP >= 500`
 * 稳，且不会把 HP 100 的队长漏掉。*/
static int animal_big_real_ok(const ce_enemy_death_t *ev, const char *name,
                              int32_t anm_main, float hurtbox_x, int32_t drop_main)
{
    if (ev->flags & CE_EDEATH_F_REAL) return 1;
    if (name && name[0]) {
        if (strncmp(name, "Main", 4) == 0 || strcmp(name, "main") == 0) return 0;
        if (strncmp(name, "Ecl_", 4) == 0) return 0;
        return 1;
    }
    /* 名读不到但正判字段像 Girl*：仍算真敌人（三只大蝴蝶的兜底）。*/
    if (anm_main != 0) return 1;
    if (hurtbox_x > 0.0f && hurtbox_x < 28.0f) return 1;
    if (drop_main >= 16) return 1;
    return 0;
}

/* 只认「这只怪被玩家打死」的死亡事件；KILL_ALL（过场清场）/ ECL_DIE / OTHER 不算。
 * `BOMB_KILL` 是炸弹秒杀，也是玩家击破，照收（与 `ufo.c` 的 `ufo_body_kill_counts` 同口径）。*/
static int animal_big_death_counts(uint32_t kind)
{
    return kind == CE_EDEATH_KILLED || kind == CE_EDEATH_BOMB_KILL;
}

/* —— 实体池 —— */

static void token_remove(int i)
{
    if (s_tok[i].id) ce_anm_delete((uint32_t)s_tok[i].id);
    s_tok[i] = s_tok[--s_n_tok];
}

/* ★ 2026-09-13 bug-4：整只清空 = 删所有 VM + 每只字段归零 + `s_n_tok = 0`。
 *   旧版只扫 `i < s_n_tok` 并清计数 —— 换面时若引擎已经删了绘制而 `s_n_tok` 还 >0，
 *   残留实体在下次 `token_change_type` 会被 `token_make_vm` 重建（用户：「下一次切换种类又突然出来了」）。
 *   现在扫满 `ANIMAL_TOKEN_MAX` 并 `memset`，任何残留一位都不留。*/
static void token_clear_all(void)
{
    int i;
    for (i = 0; i < ANIMAL_TOKEN_MAX; ++i)
        if (s_tok[i].id) { ce_anm_delete((uint32_t)s_tok[i].id); s_tok[i].id = 0; }
    memset(s_tok, 0, sizeof s_tok);
    s_n_tok = 0;
}

/* 建/重建 VM（type 变了要换脚本）。返回 1 = 成功。 */
static int token_make_vm(animal_token_t *t)
{
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), (int)script_for_type(t->type), 16);
    if (!id) return 0;
    ce_anm_set_pos(id, t->x, t->y, t->z);
    /* ★ 别看这里：飞在画面上的灵用**脚本自己**的贴图（脚本 137–143 = 原生 `beast1.png` 的 token 单元）。
     *   `sprite_for_type()` 那套 `ICON_*` 是 **beastf.png 的槽内图标**，只在 `hud_sync` 里用；
     *   2026-09-10 用户实跑：这里一度被我覆盖成 ICON_* ⇒ 飞着的灵长得跟槽里一样（已去掉）。*/
    ce_anm_set_color(id, 0xffffffffu);
    t->id = (int32_t)id;
    return 1;
}

/* 撒一只（显式初速角版）。初速单位圆；随后按 TH17 `0x410570` 的防重叠重试。
 * `log_dir` = 1 时无论是否重试都打一行方向日志（用于中弹/真死的左右对）。*/
static int token_spawn_heading(int32_t type, int32_t long_life, float x, float y,
                               float start_ang, int log_dir)
{
    animal_token_t *t;
    float ang = start_ang, vel[3] = { 0.0f, 0.0f, 0.0f };
    int retry, bad = 1;

    if (s_n_tok >= ANIMAL_TOKEN_MAX) return 0;
    /* ★ 用户 2026-09-13 口径：「鱼灵从一开始就不应该换种类」。
     *   一手 TH17 `spawn_token` `0x410380`：只有动物分支写 `[token+0x6c] |= 2` + life 240；
     *   鱼灵不换种类 / 不变色。*/
    if (animal_type_is_fish(type)) long_life = 0;

    for (retry = 0; retry <= ANIMAL_OVERLAP_RETRIES; ++retry) {
        int j;
        ce_polar_to_vel(vel, ang, 1.0f);
        bad = 0;
        for (j = 0; j < s_n_tok; ++j) {
            float dx = x - s_tok[j].x;
            float dy = y - s_tok[j].y;
            float d2 = dx * dx + dy * dy;
            float dot = vel[0] * s_tok[j].vx + vel[1] * s_tok[j].vy;
            if (d2 < ANIMAL_OVERLAP_R2 && dot >= ANIMAL_OVERLAP_DOT) { bad = 1; break; }
        }
        if (!bad) break;
        if (retry < ANIMAL_OVERLAP_RETRIES) ang += ANIMAL_OVERLAP_TURN;
    }

    t = &s_tok[s_n_tok];
    memset(t, 0, sizeof *t);
    t->type = type;
    t->long_life = long_life ? 1 : 0;
    t->x = x; t->y = y; t->z = 0.0f;
    t->vx = vel[0]; t->vy = vel[1]; t->vz = 0.0f;
    t->life = long_life ? ANIMAL_LIFE_FAR : 0;
    if (!token_make_vm(t)) return 0;
    ++s_n_tok;
    if (log_dir || bad || retry > 0) {
        ce_log("wba_wc: token spawn type %d dir %.1f deg (retry %d%s) at (%.0f, %.0f)",
               (int)type, (double)(ang * 180.0f / ANIMAL_PI), retry,
               bad ? ", still-overlapping" : "", (double)x, (double)y);
    }
    ce_play_sound(ANIMAL_SE_SPAWN, x);           /* TH17 `spawn_token` 固定播 se 0x3b（稀有是 0x4a）*/
    return 1;
}

/* 撒一只。初速随机取四个对角线（用户观感口径）；防重叠重试按 TH17 一手。 */
static int token_spawn(int32_t type, int32_t long_life, float x, float y)
{
    float ang = (float)((int32_t)(ce_rand() % 4) * 2 + 1) * (ANIMAL_PI * 0.25f);
    return token_spawn_heading(type, long_life, x, y, ang, 0);
}

/* 大怪击破登记（死亡栈上的 O(1) 路径：只抄标量，不建 VM / 不缓存 enemy）。*/
static int animal_big_drop_push(const animal_big_drop_t *d)
{
    if (s_big_drop_n >= ANIMAL_BIG_DROP_QUEUE) {
        if (s_big_drop_lost++ == 0)
            ce_log("wba_wc: big-enemy drop queue full (cap %d) - further kills will not drop",
                   ANIMAL_BIG_DROP_QUEUE);
        return 0;
    }
    s_big_drop[s_big_drop_n++] = *d;
    return 1;
}

/* 帧末兑现：建 VM 的那一刻才打印「可证伪」的掉落行。
 * ⚠ 必须在 `on_tick_2` 的非死亡栈上调用（`token_spawn` 会 `ce_anm_spawn`）。*/
static void animal_big_drop_drain(animal_card_t *s)
{
    while (s_big_drop_n > 0) {
        animal_big_drop_t d = s_big_drop[0];
        int32_t type = 1 + (int32_t)(ce_rand() % 3);
        float y = d.y + ANIMAL_BIG_DROP_DY;
        int ok = token_spawn(type, 1, d.x, y);

        if (ok) {
            ++s->drops;
            ce_log("wba_wc: big-enemy drop -> 1 animal spirit (reason=%s, veto=%s, type %d, "
                   "sprite=%dx%d, big_by_size=%d, wave=%d wave_src=%s, odd=%d, "
                   "group_total=%d census=%d total=%d, hp=%d/%d, drop_id %d, sub %d) "
                   "at (%.0f, %.0f)",
                   animal_drop_reason_name(d.reason), animal_wave_veto_name(d.veto),
                   (int)type, (int)d.sprite_w, (int)d.sprite_h,
                   (int)d.big_by_size, (int)d.wave, animal_wave_src_name(d.wave_src),
                   (int)d.odd, (int)d.group_total, (int)d.census, (int)d.wave_total,
                   (int)d.hp_cur, (int)d.hp_max, (int)d.drop_main, (int)d.sub,
                   (double)d.x, (double)y);
        } else {
            ce_log("wba_wc: big-enemy drop blocked by token pool full (reason=%s, veto=%s, cap %d, "
                   "sprite=%dx%d, wave=%d wave_src=%s, odd=%d, group_total=%d census=%d total=%d, "
                   "hp=%d/%d, drop_id %d, sub %d) at (%.0f, %.0f)",
                   animal_drop_reason_name(d.reason), animal_wave_veto_name(d.veto),
                   ANIMAL_TOKEN_MAX, (int)d.sprite_w, (int)d.sprite_h,
                   (int)d.wave, animal_wave_src_name(d.wave_src), (int)d.odd,
                   (int)d.group_total, (int)d.census, (int)d.wave_total,
                   (int)d.hp_cur, (int)d.hp_max, (int)d.drop_main, (int)d.sub, (double)d.x, (double)y);
        }

        --s_big_drop_n;
        if (s_big_drop_n > 0)
            memmove(&s_big_drop[0], &s_big_drop[1],
                    (size_t)s_big_drop_n * sizeof s_big_drop[0]);
    }
}

static void animal_big_drop_clear(void)
{
    s_big_drop_n = 0;
    s_big_drop_lost = 0;
    s_big_diag_left = ANIMAL_BIG_DIAG_N;
    s_big_diag_noncir_left = 4;
    s_stage_script_id = 0;
    animal_wave_census_reset();
    memset(s_big_drop, 0, sizeof s_big_drop);
}

/* 变色：type 1→2→3→1，换脚本、重置寿命、放 se 0x4e（TH17 `0x410c35` 段）。 */
static void token_change_type(animal_token_t *t)
{
    t->type = animal_next_type(t->type);
    t->life = ANIMAL_LIFE_AFTER;
    t->flicker_logged = 0;
    if (t->id) { ce_anm_delete((uint32_t)t->id); t->id = 0; }
    (void)token_make_vm(t);
    /* ★ 2026-09-13 用户口径（原话「鱼灵从一开始就不应该换种类」）+ 一手 TH17 `spawn_token`
     *   `th17:0x410380`：只有**稀有动物**分支（type 15..17）写 `[token+0x6c] |= 2`（= 变色闩锁），
     *   类型 4..7 的鱼灵永远不带这个闩锁 ⇒ tokens_tick 的 `if (t->long_life)` 整块（含 flicker
     *   与换种类）根本不会进。⇒ `token_spawn()` 已对鱼灵无条件清零 long_life，**鱼灵到不了这里**：
     *   这一声 `ANIMAL_SE_CHANGE 0x4e` 只可能是动物灵。不要再加「只对 1..3 播」的特例。*/
    ce_play_sound(ANIMAL_SE_CHANGE, t->x);
}

/* —— 鱼灵 → 原版道具（TH17 `FUN_00410640` `0x410640` / `FUN_004108d0` `0x4108d0`）——
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
/* ★ 2026-09-12 十三修：撒道具/生成那串音由**用户耳朵定案** = A/B/C 三候选里的 C。
 *   用户口径（原话）：「应该是这个里面的最后一个音效是对的…也就是 C」⇒ C = `0x3e`（试听表名 se_tan03）。
 *   被否掉的候选：A `0x34`（se_lgods4）、B `0x43`（se_release）。试听轮换已删除（见 `scatter_se_select`），
 *   从此 debug/release 都播这一个 id。
 *   ⚠ `0x3e` 与 `ANIMAL_SE_RELEASE`（咆哮自然结束放灵那一声，一手 TH17 `th17:0x40e920`）**同 id** ——
 *     与本卡「放灵 / 撒道具都是『释放』那一下」的听感一致（不是一条 RE 结论，只记观察）。*/
#define ANIMAL_SE_FISH_ITEMS 0x3e   /* 撒道具/生成那串音 = 用户 2026-09-12 耳听定案（C 候选）*/
/* ★ 2026-09-12 十一修实跑锁定：正常咆哮退出 = **0x42**。
 *   证据 ①（用户耳朵）：6 候选以 180 帧一档播放，用户回报**第 5 声**像 TH17 的 roar-exit；
 *     第 5 个候选 = `0x42`（试听表名 `se_item01`）。
 *   证据 ②（一手 TH17）：原版 TH17 `0x40f4fc` 在 roar end 同一条路径 `push 0x42`
 *     （AUDIT §AL26/§AL39/§AL40）。
 *   旧值 `0x38` 是 2026-09-11 早期候选探查阶段的（错误）读数；本轮用户耳朵 + 一手 TH17
 *   双重证据把它推翻。保留备注以防回退。*/
#define ANIMAL_SE_ROAR_END   0x42
/* ★ 2026-09-12 十四修：撒道具声序列**总长封顶**（用户压力测试 bug 4）。
 *   单颗鱼一次 = `ANIMAL_P_ITEM_N`(30) 颗道具 ⇒ 30 声；多颗鱼同帧被吃时不再往上累加。*/
#define ANIMAL_SE_SEQ_MAX     30
/* ★ 2026-09-13 用户口径：撒道具的出生点必须 **clamp 进版内** —— 展开椭圆以自机为中心（半径最大 58），
 *   自机贴左右版边时外侧那几颗会出生在版外（版边 ±192），初速 2.2、3 秒窗口内回不来 ⇒ 直接丢。
 *   184 = 192 − 8（留一点余量，避免正好压在版边判定上）。*/
#define ANIMAL_ITEM_X_LIMIT   184.0f

#if ANIMAL_DEBUG_KEY && ANIMAL_SE_PROBE
/* ★ 2026-09-11 九修 / 2026-09-12 十修 / 十一修：不再靠 RE 猜「正常咆哮退出 = 哪个 id」。每个正常
 * 咆哮结束依次试听 6 个候选，**间距 180 帧（3 秒）**并逐条打印 id + resolved name；试听期间
 * **抑制**正常结束时那条音，用户只需回报「第几声最像 TH17 的 roar-exit」。
 * 六个名字按本轮任务指定的 id→name 表硬编码；SE 表布局 / 语义自证见上方 AUDIT §AL35。
 * ⚠ 用户 2026-09-12 反馈 30 帧太密听不清 ⇒ 只把 gap 改 180；候选表 / 抑制逻辑 / 日志一字未动。
 * ⚠ 2026-09-12 十一修：用户已回报第 5 声 = 0x42，且 roar-exit 已写死上方；本试听代码保留，
 *   但整个块默认不编译（`ANIMAL_SE_PROBE 0`）⇒ 不设序列、不打任何探针日志。*/
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
/* ★ 2026-09-12 十修：候选间距 30 → **180 帧（3 秒）**。30 帧时 6 个候选几乎连成一串听不清
 *（用户实跑反馈）；候选顺序 / 抑制 / 每条日志格式都不动。*/
#define ANIMAL_SE_PROBE_GAP 180
#endif

/* 函数名 `se_volume_fix` 是历史遗留；这里改的其实是 row+0x0a = priority。
 * 只查撒道具 / 正常咆哮退出这两行；priority 本来已是 100 时不会写也不会打日志。*/
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

/* ★ 2026-09-12 十三修：A/B/C 试听**结束**，id 由用户耳朵定案 = `0x3e`（= `ANIMAL_SE_FISH_ITEMS`）。
 *   轮换表与轮换状态（`s_scatter_se[]` / `se_scatter_abc`）**已删除**：debug 与 release 现在播同一个
 *   id，用户继续用 debug build 复跑也不会再听到被否掉的 A / B。
 *   一次 scatter event = 一段**连续播出的 se_left 序列**（多颗鱼 / 多次 `fish_to_items()` 合并成一段）；
 *   只在段首认一次 id 并打一行日志，段内不再换 id（否则日志说两个 id 都播了，而耳朵只听到一段）。*/
static void scatter_se_select(animal_card_t *s)
{
    if (s->se_left > 0) return;                      /* 段内：沿用本段已认的 id */
    s->se_scatter_id = (int32_t)ANIMAL_SE_FISH_ITEMS;
    ce_log("wba_wc: scatter SE -> id 0x%02x (locked by user ear 2026-09-12; "
           "candidates were A 0x34 se_lgods4 / B 0x43 se_release / C 0x3e se_tan03)",
           (unsigned)s->se_scatter_id);
}

static void fish_to_items(animal_card_t *s, int32_t type, float x, float y)
{
    float pos[3];
    int i, n, item_type, inside = 0, spawned = 0, clamped = 0;
    float speed, xmin = 1e9f, xmax = -1e9f;

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
        /* 位置照 TH17 的展开椭圆（`th17:0x41071d`–`0x4107cd`）：第 k 颗取半径 i = 58×k/(n−1)
         * ⇒ n=30 时正好是 TH17 的 0,2,…,58；`a = Rng__rand_float(min=i, max=i×0.5) × π`
         *（`th17:0x402880` + `0x4a3c5c` = π），x/y 半径 = i / i×0.5。*/
        float rad = (n > 1) ? (ANIMAL_ITEM_R_MAX * (float)i / (float)(n - 1)) : 0.0f;
        float t   = (float)(ce_rand() % 1000) * 0.001f;             /* [0,1) */
        float ang = (rad + (rad * 0.5f - rad) * t) * ANIMAL_PI;     /* rand(min=rad, max=rad/2) × π */
        float off[3];
        uint8_t *it;
        ce_polar_to_vel(off, ang, 1.0f);                            /* 单位向量 */
        pos[0] = x + off[0] * rad;
        /* ★ 2026-09-13 用户口径：出生点 clamp 进版内（只动 x；y 是从上方落下来的，不动）。*/
        if (pos[0] >  ANIMAL_ITEM_X_LIMIT) { pos[0] =  ANIMAL_ITEM_X_LIMIT; ++clamped; }
        if (pos[0] < -ANIMAL_ITEM_X_LIMIT) { pos[0] = -ANIMAL_ITEM_X_LIMIT; ++clamped; }
        if (pos[0] < xmin) xmin = pos[0];
        if (pos[0] > xmax) xmax = pos[0];
        pos[1] = y + off[1] * (rad * 0.5f);
        if (animal_in_pickup_ring(pos)) ++inside;          /* 只记不动（见上面的纠偏注）*/
        it = animal_spawn_item((uint32_t)item_type, pos, -ANIMAL_PI / 2.0f, speed, (uint32_t)(10 + i * 2));
        if (it) ++spawned;                                  /* 道具池满时 spawn 返回 0；用于证伪截断 */
    }
    /* ★ 播 n 声（每 2 帧一声）——原版是每颗道具各自在 intro 末尾响一声（听感「大量」）。
     * ★ 2026-09-12 十四修（用户 bug 3 / 4）：
     *   ① **残卡 / B 卡不播**（fish 4 → 1× `0x11` BOMB2、5 → 1× `0x10` EXTEND2）：引擎对卡道具
     *      自带生成音（用户口径「那个用的是生成残卡、B 卡的系统自带声音，无需额外干任何事情」）
     *      ⇒ 只有 `n > 1`（P 鱼 / 金钱鱼各 30 颗）才走撒道具声序列。
     *   ② **整段封顶** `ANIMAL_SE_SEQ_MAX`：用户压力测试（同帧吃很多鱼）时 `se_left += n`
     *      会把序列越拖越长（「道具很快生成完毕，但音效持续很长很长时间」）⇒ 多颗鱼不再延长总时长。*/
    /* ★ 2026-09-13 bug-3：对话中**不排**撒道具声序列 —— 用户实跑「对话期吃多道具鱼灵，
     *   音效被推迟到对话结束才一次补播」。根因是 se_left 在 `on_tick_2` 的 `if (!paused)`
     *   里才推进（对话期冻结、攒着）。这里在**入队那一刻**就按 `dialogue_open()` 拦掉；
     *   `on_tick_2` 的 paused 分支再兜底清一次（覆盖「对话开始前已排一半」的情况）。
     *   道具本身照撒（与音效无关）；残卡/B 卡本来就不进这条（n==1）。*/
    if (n > 1 && !dialogue_open()) {
        scatter_se_select(s);   /* 段首认一次 id（定案 = ANIMAL_SE_FISH_ITEMS）*/
        s->se_left += n;
        if (s->se_left > ANIMAL_SE_SEQ_MAX) s->se_left = ANIMAL_SE_SEQ_MAX;
        s->se_gap = 0;
    }
    ce_log("wba_wc: fish %d -> %d x item 0x%x (speed %.1f, spread r<=%.0f, inside_ring %d/%d, spawn_ok %d/%d, se %s) at (%.0f, %.0f)",
           (int)type, n, item_type, (double)speed, (double)ANIMAL_ITEM_R_MAX, inside, n, spawned, n,
           (n > 1) ? (!dialogue_open() ? "0x3e x seq" : "discarded (dialogue)") : "none (engine card sfx)",
           (double)x, (double)y);
    ce_log("wba_wc:   spawn x[%.0f..%.0f] clamped %d/%d (limit ±%.0f)",
           (double)xmin, (double)xmax, clamped, n, (double)ANIMAL_ITEM_X_LIMIT);
}


/* —— HUD：左下角 5 格灵槽（槽框 = beastf sprite34 切图；图标 = 槽内 token 的贴图）—— */

/* ★★ 2026-09-12：本卡的 HUD 槽是**自带锚点**（`CE_HUD_SIDE_PINNED`）——
 * 登记进 `HUD-LAYOUT.md` 的那张表、但不参与货架排布（用户裁决：117 现在的位置就是对的）。
 * 槽位本体放 DLL 静态区（`ce_state` 只有 496 字节，本卡已经快满了；本卡同时只有一张在场）。*/
static ce_hud_slot_t s_slot;             /* 认领时/每帧取到的块左下角（表面 px）*/
static int32_t s_slot_ok;                /* 1 = 本关已登记（拿不到就退回迁移前的硬编码常量兜底）*/
static int32_t s_last_path_table;        /* 1 = 本帧的 s_slot 来自槽位表（`ce_hud_slot_get`）、0 = 兜底常量
                                          * —— 只给 `hud_log_geometry()` 的诊断行用（见那里）*/
static uint32_t s_geom_log_tick;         /* 几何诊断行的节流计数（每 300 帧一行）*/

/* ★★ 2026-09-15j：**运行时几何诊断**（父 agent 指令：压暗矩形**不许再靠纸面推导**）。
 * 一行给全「可见整排到底在哪」需要的所有量（**以这一行为准**，不是以注释/常量推）：
 *   `claim_ok`  = `ce_hud_slot_claim_pinned_card()` 的返回值（文件级 `s_slot_ok`）。
 *                 ⚠ 卡状态里原来那个 `hud_slot_ok` **已删**（它从没被写过 ⇒ 每帧都走兜底常量，
 *                 「每帧取最新锚点」那句注释当时是空的；2026-09-15j 改成文件级 `s_slot_ok`）
 *   `path`      = 本帧 x/y 到底从哪来：`tbl` = `ce_hud_slot_get(117)` 的返回值、`fallback` = 兜底常量
 *   `slot`      = 认领到的槽位 (x, y) + w×h（表面 px，槽位表里的账）
 *   `cells`     = 逐格真的画出去的 x（`s_slot.x + 36·i`，i = 0..4）与 min..max
 *   `vis`       = 由 cells 推的**可见整排**盒（左沿 = min − 24 格半径、右沿 = max + 24、下沿 = y + 24）
 *   `dim`       = 本帧喂 `hud_dim_rect_surface()` 的矩形（= `ANIMAL_HUD_VIS_*`）
 * 想让用户对账时：让他看游戏里 117 那 5 格的左右两端，与 `vis` 的两个数对一下（差 190 px 量级就说明
 * 「用户看到的」与「代码画的」不是同一个东西 —— 那必须查实机/DLL，别再改矩形）。*/
static void hud_log_geometry(const char *tag, int path_table)
{
    float c[ANIMAL_SLOT_MAX];
    float cmin, cmax, y;
    int i;

    y = s_slot.y + ANIMAL_HUD_DY;
    cmin = cmax = s_slot.x;
    for (i = 0; i < ANIMAL_SLOT_MAX; ++i) {
        c[i] = s_slot.x + ANIMAL_HUD_DX * (float)i;
        if (c[i] < cmin) cmin = c[i];
        if (c[i] > cmax) cmax = c[i];
    }
    ce_log("wba_wc: hud geom [%s] claim_ok=%d path=%s | slot (%.0f, %.0f) + %.0fx%.0f | "
           "cells %.0f %.0f %.0f %.0f %.0f (x %.0f..%.0f) | vis(dim) (%.0f, %.0f) + %.0fx%.0f | "
           "consts (%.0f, %.0f) + %.0fx%.0f drift=%d",
           tag, (int)s_slot_ok, path_table ? "tbl" : "fallback",
           (double)s_slot.x, (double)s_slot.y, (double)s_slot.w, (double)s_slot.h,
           (double)c[0], (double)c[1], (double)c[2], (double)c[3], (double)c[4],
           (double)cmin, (double)cmax,
           (double)(cmin - ANIMAL_HUD_HALF), (double)(y + ANIMAL_HUD_HALF),
           (double)(cmax - cmin + 2.0f * ANIMAL_HUD_HALF), (double)ANIMAL_HUD_VIS_H,
           (double)(ANIMAL_HUD_PIN_X - ANIMAL_HUD_HALF),
           (double)(ANIMAL_HUD_PIN_Y + ANIMAL_HUD_DY + ANIMAL_HUD_HALF),
           (double)ANIMAL_HUD_VIS_W, (double)ANIMAL_HUD_VIS_H,
           /* drift = 「按运行时槽位推出来的矩形」≠「按 PIN 常量推的矩形」——正常应当 0；
            * 出现 1 就说明槽位表里的 x/y 与 pin 不再一致（那就是要查的事了，别再改注释）*/
           (cmin - ANIMAL_HUD_HALF != ANIMAL_HUD_PIN_X - ANIMAL_HUD_HALF) ||
           (y + ANIMAL_HUD_HALF != ANIMAL_HUD_PIN_Y + ANIMAL_HUD_DY + ANIMAL_HUD_HALF) ? 1 : 0);
}

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
        /* 认领失败 ⇒ 立刻把**兜底常量**填进 `s_slot`（SDK 失败时不会写 `out`）
         * ⇒ 这一行诊断与后续每帧的 `hud_slot_sync()` 都不会看到垃圾值 */
        s_slot.x = ANIMAL_HUD_PIN_X;
        s_slot.y = ANIMAL_HUD_PIN_Y;
        s_slot.w = ANIMAL_HUD_BLOCK_W;
        s_slot.h = ANIMAL_HUD_BLOCK_H;
        s_slot.side = CE_HUD_SIDE_PINNED;
    }
    hud_log_geometry("claim", s_slot_ok);     /* 认领当场再打一遍完整几何（含逐格 x）*/
}

/* 每帧取**最新**锚点（pin 的位置不会被别人动，但关卡纪元/删卡会把这条记录清掉 ⇒ 取最新更稳）。
 * ⚠ `ce_hud_slot_get()`（`sdk_core.c` 一手）**只填 x/y/w/h/side，不填 x640/y640** ⇒ 不能读那两格。
 * ★★ 2026-09-15j **修了一个洞**：这里原来读 `s->hud_slot_ok`，而认领写的是**文件级 `s_slot_ok`**
 *   —— 卡状态那个字段（`animal_card_t.hud_slot_ok`）**全仓没有任何写点**（`ce_state` 被 memset 成 0）
 *   ⇒ 这个 if 恒真、**`ce_hud_slot_get()` 从来没被调用过**，「每帧取最新锚点」是一句空注释。
 *   对 117 没有可见后果（pinned 的账就是 `ANIMAL_HUD_PIN_X/Y`，兜底常量同值），但它是**真洞**：
 *   ① 槽位表里若真有别的值（将来 SDK 改 pinned 的语义 / 有人换 pin），卡侧看不见；
 *   ② 压暗矩形要「按**运行时槽位值**推」（父 agent 2026-09-15j 口径）⇒ 必须真的去读那张表。
 *   现在读文件级 `s_slot_ok`，并且 `ce_hud_slot_get()` 失败时**显式**退回兜底常量（不留陈旧值）。*/
static void hud_slot_sync(animal_card_t *s)
{
    if (!s_slot_ok || !ce_hud_slot_get(117, &s_slot)) {
        s_slot.x = ANIMAL_HUD_PIN_X;
        s_slot.y = ANIMAL_HUD_PIN_Y;
        s_slot.w = ANIMAL_HUD_BLOCK_W;
        s_slot.h = ANIMAL_HUD_BLOCK_H;
        s_slot.side = CE_HUD_SIDE_PINNED;
        s_last_path_table = 0;
    } else {
        s_last_path_table = 1;
    }
    (void)s;
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

/* 每帧推进 HUD 淡入淡出。★ 2026-09-15g：**判据与动态**全部由跨卡统一件 `cards/hud_dim.{h,c}`
 * 说了算（本卡原来那份 `animal_hud_player_near()` + 五个本地常量已删，原一手值见上面常量区的留档注）
 * ⇒ 这里只剩「每帧现算本卡的块矩形 → 调一次 + 边沿打日志 + 维护 hud_fade_touched」。
 * 放在 on_draw 里跟着 hud_sync 一起跑（`CE_PLAYER() && CE_GAME_THREAD()` 门之后 ⇒ 每帧正好一次，
 * 且 `hud_slot_sync()` 刚取过最新锚点 ⇒ 矩形是本帧的）。
 *
 * ★★ 2026-09-15i：**喂进去的是「可见整排」那份矩形，不是「声明给槽位的块」**（父 agent 裁定）——
 *   两份几何为什么不同、各自的来源，见上面 `ANIMAL_HUD_VIS_*` 那段（根子：`ANIMAL_HUD_PIN_X = 88`
 *   被代码当成**首格格心**用，于是声明块 88..281 比可见整排 64..256 整体**右移 24 px**）。
 *   压暗的口径是「**HUD 画在哪，判据就跟着哪**」（用户 2026-09-15）⇒ 判据必须用**可见**那份：
 *     · 声明块 (88, 924) 193×48 仍然照旧喂 `ce_hud_slot_claim_pinned_card()`（货架/重叠账要它，
 *       且它是「本排的左移前基准」的记账值）—— **一个字节没动**；
 *     · 压暗 (64, 928) 192×48 = 5 格盒（`ANIMAL_HUD_VIS_*`）。
 *   ⚠ **行为后果（要报的）**：本排右沿 256 与「自机中心能到的最左」272 只差 16 px
 *   （自机 x 钳位 ±184 ⇒ 表面 272..1008）⇒ 自机**永远碰不到**这一排，压暗只能靠共享件那条
 *   28 px 边距在「贴着左墙」时成立（进 `x ∈ [−184, −164]`、出 `x < −196`，即 20 px 宽的一条）。
 *   这不是算错，是本排的几何（它整排画在游玩区左边缘之外）；要它更早亮 = 改 `HUD_DIM_MARGIN`
 *   （一个宏、影响所有卡）或**挪本排**（摆位口径，不归本轮）。
 *   ⚠ 满槽火焰（`FLAME_0`，×1.5、`anchor(0,2)`）顶边最高到表面 y 862 —— 比本矩形上沿 880 高 18 px；
 *   它对**门的 y 侧**没有影响（`y0 = 424` 再外扩 28 = 396 已经在自机 y 钳位上沿之外，进区恒含自机底边 432），
 *   所以这里不为了它把矩形加高（要加就把 `ANIMAL_HUD_VIS_H` 改 66、下沿不动）。*/
static void hud_fade_tick(animal_card_t *s)
{
    const int32_t was_dim = s->hud.dim;
    hud_dim_rect_t r;

    /* ★★ 2026-09-15j：矩形**每帧从运行时槽位值推**（`s_slot`），左沿/下沿**不写死**：
     *   画在哪 = `hud_sync` 的 `x = s_slot.x + 36·i` / `y = s_slot.y − 20`
     *   ⇒ 可见整排 = x ∈ [s_slot.x − 24, s_slot.x + 4×36 + 24]、y ∈ [(s_slot.y−20) − 24, (s_slot.y−20) + 24]
     *   ⇒ 左下角 = `(s_slot.x − ANIMAL_HUD_HALF, s_slot.y + ANIMAL_HUD_DY + ANIMAL_HUD_HALF)`、尺寸 192×48。
     *   **为什么不许从 `ANIMAL_HUD_PIN_X` 推**：① PIN_X 是**首格的格心**、不是整排左沿（差半个格半径）；
     *   ② 它只是**认领时交给槽位表的请求值** —— 卡真正画的 x 是槽位表还回来的 `s_slot.x`
     *   （`path=tbl` 时 = `ce_hud_slot_get(117)`，`path=fallback` 时才是常量）。pin 的注释在历史上
     *   写错过一次（写成「整排左下角」），所以判据只认**运行时**那一个数。
     *   ⇒ 若哪天表里的 x 不是 88（日志 `wba_wc: hud geom … slot (…)` 会直接显示），压暗**自动跟着走**。*/
    hud_dim_rect_surface(&r, s_slot.x - ANIMAL_HUD_HALF,
                         s_slot.y + ANIMAL_HUD_DY + ANIMAL_HUD_HALF,
                         ANIMAL_HUD_VIS_W, ANIMAL_HUD_VIS_H);
    (void)hud_dim_tick(&s->hud, &r, ce_player_pos());
    if (s->hud.dim != was_dim) {
        ce_log(s->hud.dim ? "wba_wc: hud dim on (player body touches the slot row)"
                          : "wba_wc: hud dim off");
    }
    if (s->hud.alpha == 255 && s->hud_fade_touched) s->hud_fade_touched = 0;
    else if (s->hud.alpha < 255)                    s->hud_fade_touched = 1;
}

static void hud_sync(animal_card_t *s)
{
    int i;
    uint32_t A;
    int faded;

    /* ★ 2026-09-15g：**先取锚点再推进压暗** —— 压暗的门限由本帧的块矩形推（见 `hud_fade_tick`），
     * 顺序反了就会用上一帧的锚点（块被货架重排的那一帧会错一格）。*/
    hud_slot_sync(s);                      /* ★ 每帧取最新锚点（见 hud_slot_sync 的注释）*/
    hud_fade_tick(s);
    /* ★★ 2026-09-15j：每 300 帧（= 5 s）打一行**运行时几何**（父 agent 指令）——
     *   压暗矩形要用「运行时的槽位值」，而这件事只能从这一行看出来（纸面推导错过一次）。
     *   一行含：认领结果 claim_ok、x/y 来自槽位表还是兜底（path）、槽位 (x,y,w,h)、
     *   逐格 x 与 min..max、可见整排盒、以及本帧真的喂进 `hud_dim_rect_surface()` 的矩形。*/
    if (++s_geom_log_tick >= 300u) {
        s_geom_log_tick = 0;
        hud_log_geometry("tick", s_last_path_table);
    }
    A = (uint32_t)(s->hud.alpha & 0xff) << 24;      /* ★ 2026-09-15g：读跨卡统一件 */
    faded = (s->hud.alpha < 255);
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
                if (a > s->hud.alpha) a = s->hud.alpha;      /* 再叠上「自机贴近」的淡出（统一件 alpha）*/
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

/* —— 渐变圆（self aura，脚本 155 = TH17 beast.anm script32）的收回：三条退出路径一手对照 ——
 * TH17 一手（`engine/animal/th17/01` §5.3/§5.4；`do_hyper_cancel` `th17:0x40f6d0`、
 * 超时结束段 `th17:0x40f394`–`0x40f553`）：
 *   · 自然结束（没用霊撃，`HYPER_FLAGS & 4 == 0`）→ `th17:0x40f508` 对 `manager+0x30` 发 interrupt **1**；
 *   · 霊撃起手（X / 中弹，`do_hyper_cancel` by_hit 0/1）→ `th17:0x40f7bf` 对 `manager+0x30` 发 interrupt **2**
 *     （句柄**不清零**）；
 *   · 霊撃跑完 40 帧后仍走 `th17:0x40f508` 再补一次 interrupt **1**。
 * 脚本 155 / TH17 script32 的两个 label（逐字节）：
 *   interrupt 1 = `scaleTime(30,4,0,0)`                    → 30 帧缩回 0（自然结束）
 *   interrupt 2 = `scaleTime(20,4,200,200)` + frame40 `scaleTime(10,1,0,0)`
 *                                                           → 先缩到 200、停 20 帧、再 10 帧收 0（共 50 帧）
 * ⇒ 用户 bug7「按 X / 中弹收回更快」= 我们过去只在 hyper_end 发 interrupt 1（30 帧直线收），
 *   而 TH17 在**按键那一刻**就发 interrupt 2（两段曲线收）。现在按路径分别发。*/
#define ANIMAL_SELF_AURA_FADE_NATURAL   1   /* interrupt 1：30 帧缩回（自然结束；TH17 0x40f508）*/
#define ANIMAL_SELF_AURA_FADE_BY_HYPER  2   /* interrupt 2：50 帧两段缩放（X / 中弹；TH17 0x40f7bf）*/

/* TH17 `0x20`（script32）：自机位置展开一个 32 边形渐变圆（color 青→color2 橙），
 * `scaleTime(60,1,600,600)` 长到 600 px 盖住弹幕区，layer 6 画在弹幕区背后。 */
static void self_aura_spawn(animal_card_t *s)
{
    if (s->self_aura_id) return;
    s->self_aura_id = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SELF_AURA, 6);
    ce_log("wba_wc: self aura spawn (vm %d)", (int)s->self_aura_id);
}

/* 只发 interrupt、**不动句柄**：TH17 `0x40f7bf` 发完 interrupt 2 后不清零 `manager+0x30`，
 * 40 帧后 `0x40f508` 还要再发 interrupt 1（见上面常量注）。*/
static void self_aura_interrupt(animal_card_t *s, int label)
{
    if (s->self_aura_id) ce_anm_interrupt((uint32_t)s->self_aura_id, label);
}

static void self_aura_kill(animal_card_t *s, int immediate)
{
    if (!s->self_aura_id) return;
    if (immediate) ce_anm_delete((uint32_t)s->self_aura_id);
    else ce_anm_interrupt((uint32_t)s->self_aura_id, ANIMAL_SELF_AURA_FADE_NATURAL);
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
 * **绝不写 sht 的 0..9 槽** —— 旧弹命中回读时仍解析原生 row。EAGLE 的视觉/伤害在 eagle_beam_tick。 */
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

    /* ★ 2026-09-13 十八修（真身）：set 重定向只挡**新**弹；`mode=2` 激光的寿命只跟 Z 走
     *   （`tick2` `0x4601e0` 的 `[player+0x476d4] < 0` 才自杀），所以按住 Z 进咆哮时
     *   **咆哮前已经打出来的普通子机激光会一直钉在原处**。每帧把它们收掉：
     *   `ce_kill_retail_option_lasers()` 只碰 packed set ≤ 0x15 的 mode=2 弹
     *   （0..9 主炮 + 0x0a..0x15 原生装备子机组），不动 108/77/116（0x16..0x1d）与
     *   本卡 WOLF/EAGLE（0x1e..0x27）。*/
    {
        int n = ce_kill_retail_option_lasers();
        /* 前 8 个咆哮帧各打一条 n（包括 n=0），便于看到「旧束只在第一帧被收掉」；set 明细由
         * sdk.c 在 n>0 时追加。字段复用 = 本次咆哮已打过的 probe 帧数，on_ready/on_load 清零。*/
        if (s->retail_laser_kill_logged < 8) {
            ++s->retail_laser_kill_logged;
            ce_log("wba_c: retail option laser kill probe #%d: n=%d (mode=2 set<=0x15 old beams)",
                   (int)s->retail_laser_kill_logged, n);
        }
    }

    base = (s->core.hyper_type == 1) ? CE_SHT_SET_WOLF_0 : CE_SHT_SET_EAGLE_0;
    if (!s->shot_set_logged) {
        s->shot_set_logged = 1;
        if (s->core.hyper_type == 1)
            ce_log("wba_c: shot set -> wolf (fire-time index base 0x%x + power, retail 0..9 untouched)", base);
        else {
            /* ★ AL59：EAGLE 形态现在按自机分岔（灵梦 = data-path 追尾、魔理沙/咲夜 = 卡侧、早苗 = 数据路径大青蛙）*/
            unsigned ch = *(const uint32_t *)CE_ADDR_CHARACTER;
            const char *how = (ch == ANIMAL_CHAR_MARISA) ? "card-side beams, main set empty"
                            : (ch == ANIMAL_CHAR_SAKUYA) ? "card-side REAL bullets (shoot_one_bullet), main path pinned to empty EAGLE_0"
                            : (ch == 3u) ? "big frog scatter (main set, anm=29, hit0 explosion)"
                            : "data path (main set, homing body bullets)";
            ce_log("wba_c: shot set -> eagle (fire-time index base 0x%x + power; %s)", base, how);
        }
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
    int pin_base = 0;               /* ★ AL61：把主炮路径钉在 base 组本身上（不 +power）*/
    const char *what = "unchanged";

    if (!s || !roaring(s)) return -1;
    if (s->core.hyper_type == 1) {
        base = CE_SHT_SET_WOLF_0;
        what = "wolf";
    } else if (s->core.hyper_type == 3) {
        unsigned ch = *(const uint32_t *)CE_ADDR_CHARACTER;
        base = CE_SHT_SET_EAGLE_0;
        if (ch == ANIMAL_CHAR_SAKUYA) {
            /* ★ AL61：咲夜鹫 = 卡侧 `Player__shoot_one_bullet` 真发弹（set EAGLE_1）。
             *   主炮路径钉到**空的 EAGLE_0** ⇒ 引擎不会自动发这组（否则会双份）。*/
            pin_base = 1;
            what = "eagle (card-side REAL bullets; main path pinned to empty EAGLE_0)";
        } else if (ch == 3u) {
            what = "eagle (big frog scatter; data path)";
        } else {
            what = "eagle (main set or card-side beams)";
        }
    } else {
        what = "unchanged";
    }

    if (s->shot_index_req_logs < 5) {
        ++s->shot_index_req_logs;
        if (base >= 0 && power_level >= 0 && power_level < ANIMAL_SHT_POWER_N)
            ce_log("wba_c: shot set index req: card=117 ready=%d hyper=%d power=%d -> %s (base 0x%x + pwr = 0x%x%s)",
                   s->core.ready, s->core.hypering, power_level, what, base,
                   pin_base ? base : base + power_level, pin_base ? ", pinned" : "");
        else
            ce_log("wba_c: shot set index req: card=117 ready=%d hyper=%d power=%d -> %s",
                   s->core.ready, s->core.hypering, power_level, what);
    }
    if (base >= 0 && power_level >= 0 && power_level < ANIMAL_SHT_POWER_N)
        return pin_base ? base : base + power_level;
    return -1;
}

/* ★ 2026-09-12 十二修（用户实跑）：咆哮换掉的是**主炮组**，但 TH18 的「装备子机」走的是
 *   另一条路 —— 每张装备 option 卡的 `__on_shoot` 调 `Player__tick_shooters_for_ability_card`
 *   `0x40a9c0`（set 0x0a..0x15），按自己那组原生 row 在 option 位置逐帧发弹。咆哮期间
 *   `on_shot_set_index` 管不到它们 ⇒ 用户看到「灵梦 / 魔理沙的子机照旧发射普通激光」。
 *   一手 TH17 `Player__do_shooting`（`th17:0x44a710`）换的是**整组**主炮 + 子机：
 *   pl00 狼行全是 body、pl01 鹫行是 option 行（由我们的卡侧束接管）⇒ TH18 这些装备 option
 *   卡在咆哮里应当停火。
 *
 *   本回调只被 SDK 在**原生组** 0x0a..0x15 上问到（`BP_ce_option_shot_gate` 0x40a9c3）；
 *   返回 1 = 本帧这一组不发。`roaring()` = ready || hypering，与主炮组一致。
 *   ★ 2026-09-12 十四修：SDK 的 `CE_OPTION_SHOT_GATE_ENABLE` 已开（见 sdk.c 的 0x40a9c3 站点），
 *   本回调现在就生效。*/
static int on_option_shot_gate(ce_card_t *c, int sht_set)
{
    animal_card_t *s = st(c);

    if (!s || !roaring(s)) return 0;
    /* ★ 2026-09-13 十七修：二次校验与 SDK 同步 —— 咆哮期间这条路径会用**咆哮组**当 set
     *   （WOLF 0x1e..0x22 非空 ⇒ 用户看到子机照旧开火；EAGLE 空组 ⇒ 看不出）。*/
    if (!((sht_set >= CE_SHT_SET_OPTION_MIN && sht_set <= CE_SHT_SET_OPTION_MAX) ||
          (sht_set >= CE_SHT_SET_BEAST_MIN   && sht_set <= CE_SHT_SET_BEAST_MAX)))
        return 0;
    if (!s->option_gate_logged) {
        s->option_gate_logged = 1;
        ce_log("wba_c: option shot suppressed while roaring (retail equip option set 0x%x, type %d, char %u)",
               (unsigned)sht_set, (int)s->core.hyper_type,
               (unsigned)*(const uint32_t *)CE_ADDR_CHARACTER);
    }
    return 1;
}

/* +0x28：bug2 对账日志（低风险；只读 `bullet+0xac` / `bullet+0x9c`，不改弹）。
 * 灵梦 / 咲夜 / 早苗的鹫 = pl00a EAGLE 5 行 body 追尾弹（set 0x24..0x27；`engine/sht/th17/02` §2.2）。
 * 命中伤害由引擎的伤害源管线自动结算（`engine/sht/th16/08` 一手：`func_on_hit` 只管特效、
 * 不管扣血），所以这行日志是「弹真的生成了、初值 dmg 正确」的**可证伪证据**：
 *   · 有 `eagle body bullet created ... dmg 20` ⇒ 数据路径活着，打不中只可能是追踪/命中观感；
 *   · 一条都没有 ⇒ 咆哮没换到 EAGLE 组 / set index 没生效（去看 `shot set index req` 日志）。*/
static int on_bullet_created(ce_card_t *c, void *bullet)
{
    animal_card_t *s = st(c);
    uint32_t packed;
    int set;

    if (!s || !bullet) return 0;
    packed = *(uint32_t *)((uint8_t *)bullet + ANIMAL_BULLET_PACKED_ID);
    set = (int)(packed >> 8);
    /* ★ 2026-09-13 十八修【可证伪普查】：咆哮期间前 8 颗自机弹各属哪一组 set。
     * 期望 = 0x1f..0x22（WOLF，重定向生效）；出现 0x00..0x09 ⇒ 主炮仍走零售组（重定向失效）；
     * 出现 0x0a..0x15 ⇒ 装备子机路径还在发（`0x40a9c0` 没被门拦住）。只读、不改弹。*/
    if (roaring(s) && s->roar_bullet_logs < 8) {
        ++s->roar_bullet_logs;
        ce_log("wba_c: roar bullet set census #%d: set 0x%x row %d (char %u)",
               (int)s->roar_bullet_logs, (unsigned)set, (int)(packed & 0xffu),
               (unsigned)*(const uint32_t *)CE_ADDR_CHARACTER);
    }
    if (set < CE_SHT_SET_EAGLE_1 || set > CE_SHT_SET_EAGLE_4) return 0;
    if (s->eagle_body_logs >= 3) return 0;
    ++s->eagle_body_logs;
    ce_log("wba_c: eagle body bullet created (char %u set 0x%x row %d, dmg %d; hit data-path auto)",
           (unsigned)*(const uint32_t *)CE_ADDR_CHARACTER, (unsigned)set, (int)(packed & 0xffu),
           *(const int32_t *)((uint8_t *)bullet + CE_BULLET_DAMAGE));
    return 0;
}

/* EAGLE 卡侧矩形束的可视 VM。脚本 163 自己写 anchor(1,0) + rotate(-90°)（= 向上）；
 * C 每帧写 pos / scale，并用 `ce_anm_set_rotation` 把「向上」换成该条的散射角。
 * ★ 2026-09-12 十二修（用户口径，AUDIT §AL42）：pl01 的鹫 = **本体发出的 5 条散开粗激光**
 *   （TH17 `pl01a.sht` EAGLE 5 行：伤害 80/60/60/50/50、角度 -90°/-75°/-105°/-60°/-120°；
 *   `opt_slot` 低 4 位全是 0 = 本体）。不再挂到原生子机 / 单根本体束。
 *   本函数把数组里的 5 条束全部收掉（fade=1 走 interrupt，fade=0 直接 delete）。 */
static void eagle_beam_kill(animal_card_t *s, int fade)
{
    int i, n = 0;
    if (!s) return;
    for (i = 0; i < ANIMAL_EAGLE_BEAM_MAX; ++i) {
        if (!s->beam_id[i]) continue;
        ++n;
        if (fade) ce_anm_interrupt((uint32_t)s->beam_id[i], 1);
        else      ce_anm_delete((uint32_t)s->beam_id[i]);
        s->beam_id[i] = 0;
    }
    if (n) {
        ce_log("wba_c: eagle beam off (%d beams, len %.0f (sprite %.0f x scale %.1f), hit_len %.0f, hit_w %.0f, "
               "vis_w %.0f, dmg %d/frame cap %d, fade=%d)",
               n, (double)ANIMAL_EAGLE_BEAM_LEN, (double)ANIMAL_EAGLE_BEAM_BASE_LEN,
               (double)ANIMAL_EAGLE_BEAM_SCALE, (double)ANIMAL_EAGLE_BEAM_LEN,
               (double)ANIMAL_EAGLE_BEAM_HIT_W,
               (double)(ANIMAL_EAGLE_BEAM_BASE_W * ANIMAL_EAGLE_BEAM_SCALE),
               (int)ANIMAL_EAGLE_BEAM_DAMAGE, (int)ANIMAL_EAGLE_BEAM_CAP, fade);
    }
    s->beam_n = 0;
    s->beam_logged = 0;
    s->beam_age = 0;
}

/* TH17 `pl01a.sht` EAGLE 5 行的角度（弧度，-π/2 = 正上；`engine/sht/th17/02` §2.4）：
 * 行序 0..4 = -90° / -75° / -105° / -60° / -120°，扇形散开、越外侧伤害越低。
 * 这是「本体发射的 5 连扇形粗激光」的几何来源；卡侧只实现视觉 + 伤害（不消弹，bug-2(b)）。*/
static const float s_eagle_beam_angle[ANIMAL_EAGLE_BEAM_N] = {
    -1.5707964f,   /* row0  -90° (dmg 80) */
    -1.3089969f,   /* row1  -75° (dmg 60) */
    -1.8325957f,   /* row2 -105° (dmg 60) */
    -1.0471976f,   /* row3  -60° (dmg 50) */
    -2.0943952f,   /* row4 -120° (dmg 50) */
};

/* bug6(b)：每发相位 → 视觉长度（px）。
 * 一手：TH17 `pl01.anm` script16 的长度由引擎写 `vm+0x68 = +0xa0/512`（`th17:0x44bfa3`，
 * `0x4a3aa0` = 1/512），`+0xa0` 每帧 +18（`0x4a3ccc`）到 512（`0x4a3dac`）。本卡侧把
 * 「每 SHOT_FRAMES 帧一发」的行程做成从 MIN_FRAC×TRAVEL 拉到 TRAVEL 的锯齿；LOOP=0 则一次拉满保持。*/
/* **本次激活**的展开系数 0..1：束建好后的前 `ANIMAL_EAGLE_BEAM_GROW_FRAMES` 帧 0→1，之后恒 1。
 * ★ 2026-09-13 二十二修（用户）：「展开」必须是**一次性的** —— 二十修把它挂在「每发相位」上
 *   （`frame % SHOT_FRAMES`），于是按住 Z 时每 15 帧就 0→满 重来一次，用户看到的正是
 *   「持续地缩小展开缩小展开」的蹦迪感。现在改用**激活帧数** `s->beam_age`：按 Z 展开一次，
 *   保持满尺寸（"往外飞"的观感由脚本 `scrollX` 的贴图外流给）。*/
static float eagle_beam_grow(const animal_card_t *s)
{
    float g;
    if (!s) return 1.0f;
    g = (float)s->beam_age / (float)ANIMAL_EAGLE_BEAM_GROW_FRAMES;
    if (g < 0.0f) g = 0.0f;
    if (g > 1.0f) g = 1.0f;
    return g;
}

static float eagle_beam_vis_len(animal_card_t *s)
{
    return ANIMAL_EAGLE_BEAM_TRAVEL * eagle_beam_grow(s);
}

/* 每帧：roar + EAGLE + 魔理沙(pl01) + Z 按住 → 在**本体位置**按 5 个角度各挂/刷新一条
 * 满长 2048 px 的矩形束（脚本 163 = TH17 pl01.anm entry0 sprite29 的 512×32 激光条 ×scale）；否则全收。
 * 视觉：scale.y 恒 = `ANIMAL_EAGLE_BEAM_SCALE`（宽 32×4=128），
 *       scale.x = 当前视觉长 / sprite 逻辑长（满长时也 = SCALE；每发外扫时从 SCALE×MIN_FRAC 涨到 SCALE）；
 *       贴图流动由脚本 163 的 `scrollX` 负责（TH17 script16 同款）。
 * 伤害：矩形长 = 当前视觉长（跟视觉一起走」，避免看得见的束和打得到的地方脱节），宽 = 一手 hit_h 34。
 * **不再消弹**（2026-09-13 bug-2(b)：TH17 的鹫激光不消弹）。
 * 灵梦 / 咲夜 的鹫是数据路径的 5 颗 body 追尾弹，卡侧束只给 pl01。 */
static void eagle_beam_tick(animal_card_t *s)
{
    const float *pp;
    int held, i, spawned = 0;

    if (!s) return;
    held = (CE_INPUT_HELD() & 1u) != 0;
    if (!roaring(s) || s->core.hyper_type != 3 || !held ||
        *(const uint32_t *)CE_ADDR_CHARACTER != ANIMAL_CHAR_MARISA) {
        eagle_beam_kill(s, 1);
        return;
    }
    pp = ce_player_pos();
    if (!pp) return;

    for (i = 0; i < ANIMAL_EAGLE_BEAM_N; ++i) {
        float a = s_eagle_beam_angle[i];
        float vis = eagle_beam_vis_len(s);   /* 当前视觉/判定长（1024..2048）*/
        float dir[3];

        if (!s->beam_id[i] || !ce_anm_get_vm((uint32_t)s->beam_id[i])) {
            s->beam_id[i] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_EAGLE_BEAM, 13);
            if (!s->beam_id[i]) continue;
            spawned = 1;
            s->beam_age = 0;                 /* 这一次激活从 0 开始展开 */
        }
        if (!ce_anm_set_pos((uint32_t)s->beam_id[i], pp[0], pp[1], pp[2])) continue;
        /* 脚本 163 预设 rotate(-90°)（正上）；用角度 a 覆盖它，5 条就散开。 */
        (void)ce_anm_set_rotation((uint32_t)s->beam_id[i], 0.0f, 0.0f, a);
        /* bug6/ bug-2：脚本里**不跑** scaleTime，否则插值器会覆盖这里的每帧写入。
         * 两轴同一个旋钮 `ANIMAL_EAGLE_BEAM_SCALE`：满长时 scale = (SCALE, SCALE)；
         * 每发外扫只让 x 从 SCALE×MIN_FRAC 涨到 SCALE（宽度恒 SCALE）。
         * anchor 在近端 ⇒ 远端往外伸。*/
        /* ★ 二十修：宽度也吃展开系数（原来恒 SCALE ⇒ 一上来就满宽、看着是「瞬间出现」）。 */
        (void)ce_anm_set_scale((uint32_t)s->beam_id[i],
                               vis / ANIMAL_EAGLE_BEAM_SPRITE_W,
                               ANIMAL_EAGLE_BEAM_SCALE * eagle_beam_grow(s));

        ce_polar_to_vel(dir, a, vis);   /* dir = (cos a, sin a, 0) × vis：束尖相对本体的偏移 */
        {
            float center[3] = { pp[0] + dir[0] * 0.5f, pp[1] + dir[1] * 0.5f, pp[2] };
            ce_raise_damage_cap(ANIMAL_EAGLE_BEAM_CAP);
            (void)ce_damage_rect(center, a + ANIMAL_PI * 0.5f, 2, ANIMAL_EAGLE_BEAM_DAMAGE,
                                 ANIMAL_EAGLE_BEAM_HIT_W, vis);
        }
        /* ★ 2026-09-13 bug-2(b)：这里原来有 5 个 `ce_cancel_radius` 圆近似矩形柱消弹 —— 已删。
         *   TH17 的鹫激光不消弹（用户明确要求），删掉后本函数只剩视觉 + 伤害。*/
    }

    ++s->beam_age;                           /* 本次激活的帧数（展开用；一次性）*/

    if (!s->beam_logged || s->beam_n != ANIMAL_EAGLE_BEAM_N || spawned) {
        s->beam_logged = 1;
        s->beam_n = ANIMAL_EAGLE_BEAM_N;
        ce_log("wba_c: eagle beam on (%d body fan lasers, visual %.0f x %.0f px = %.0f x %.0f, "
               "scale %.1f, grow %d frames one-shot, hit_len %.0f hit_w %.0f, "
               "dmg %d/frame cap %d, no-cancel)",
               ANIMAL_EAGLE_BEAM_N,
               (double)ANIMAL_EAGLE_BEAM_LEN, (double)(ANIMAL_EAGLE_BEAM_BASE_W * ANIMAL_EAGLE_BEAM_SCALE),
               (double)ANIMAL_EAGLE_BEAM_LEN, (double)(ANIMAL_EAGLE_BEAM_BASE_W * ANIMAL_EAGLE_BEAM_SCALE),
               (double)ANIMAL_EAGLE_BEAM_SCALE, (int)ANIMAL_EAGLE_BEAM_GROW_FRAMES,
               (double)eagle_beam_vis_len(s), (double)ANIMAL_EAGLE_BEAM_HIT_W,
               (int)ANIMAL_EAGLE_BEAM_DAMAGE, (int)ANIMAL_EAGLE_BEAM_CAP);
    }
}

/* ★ 2026-09-13 bug-1：pl00 / pl02 / pl03 EAGLE body 追尾弹的「收敛补正」。
 *
 * 一手对账（本次）：TH17 tick1 `th17:0x44b7d0` 与 TH18 tick1 `th18:0x45f7c0` **逐指令等价** ——
 *   索敌 `find_nearest_enemy_id` 半径都 = **256**（`th17:0x4a3d6c` / `th18:0x4b9354`；旧文档
 *   `engine/sht/th17/02` §3 写「TH18 是 512」是**错的**，本次实测 256）、
 *   转角都 = `diff × 0.08`（`th17:0x4a3ae4` / `th18:0x4b90d0`）、
 *   都有 `cmp [bullet+0x10], 0x3c` 的 **60 帧转向窗口**（60 帧后只 `speed += 0.2`、不再转）、
 *   `|diff|<15°` +0.2 cap16 / `≥45°` −0.2 floor4（`0x4a3b10`/`0x4a3c84`/`0x4a3cc8`
 *   ↔ `0x4b90fc`/`0x4b9248`/`0x4b9288`）、无目标 +0.1 cap16（`0x4a3aec`/`0x4b90d8`）。
 *   ⇒ `append_shooterset.py` 的 `HYPER_FUNC_MAP["tick"][1] = 1` **就是** TH18 唯一对应「追尾」的一档，
 *     没有别的索引可换（TH18 tick 表只有 1=homing、2=laser、3=accel、4=lock-dash、5=bounce）。
 *   TH17 行的 tail（+0x38..+0x57）在 pl00a EAGLE 5 行**全 0**（本次逐字节回读），且两个 tick1
 *   都**不读 shooter 记录** ⇒ 没有「半径/转速」参数可读错（`HYPER_TEMPLATE_PL00A_EAGLE` 的 tail 全 0 正确）。
 *
 * 「绕半圈然后直飞」是这套 dynamics 的固有结果：speed=10 + 0.08/帧 ⇒ 转弯半径 125 px，
 * 侧翼行（±60/±90/±120/±150）初始角误差大时进极限环，60 帧窗口一关就直飞（用户实跑复现）。
 * ⇒ **卡侧最小补正**：每帧给仍在 60 帧窗口内的 EAGLE body 弹**再叠一份 0.08 的转角**
 *   （总 ~0.16/帧 ⇒ 转弯半径减半、20 帧内收敛），只写 `bullet+0x64`；
 *   speed / dmg / 初始角 / 60 帧窗口 / 索敌全交给引擎的 tick1（TH17 数值一个不改）。
 *   ⚠ **这是卡侧补正、非 TH17 一手**（一手只证明「映射对、缺口是转角太小」）。
 * 可证伪判据：日志里 EAGLE body 弹仍在，但观感上侧翼行不再绕半圈直飞、命中显著改善；
 *   若想关掉：把 `ANIMAL_EAGLE_HOMING_EXTRA` 改 0.0f 即回到纯引擎 tick1（数据零改动）。*/
#define ANIMAL_PB_AGE        0x10   /* zPlayerBullet：存活帧（`tick_bullets` 每帧 +1；tick1 的 60 帧窗口用它）*/
#define ANIMAL_PB_POS_X      0x48   /* 位置 x/y（与 tick1 的 find_nearest / atan2 同源）*/
#define ANIMAL_PB_POS_Y      0x4c
#define ANIMAL_PB_ANGLE      0x64   /* 当前航向角（tick1 每帧 `+= diff × 0.08`）*/
#define ANIMAL_PB_TARGET_ID  0x90   /* tick1 锁定的 enemy_id（0 = 还没锁到）*/
#define ANIMAL_EAGLE_HOMING_EXTRA 0.08f

static void eagle_homing_assist(void)
{
    uint8_t *p = CE_PLAYER();
    uint8_t *em = CE_ENEMY_MGR();
    int i, n = 0;
    if (!p || !em) return;
    /* ★ 2026-09-13（AL59）：EAGLE 组现在只有**灵梦（pl00）**是 data-path 的 5 发追尾 body 弹了
     *   —— pl01 = 卡侧束、pl02 = 卡侧摆动点（主炮组空）、pl03 = 大青蛙（func tick0 直飞）。
     *   若不加这道门，本函数会给 Sanae 的青蛙每帧多转 0.08 rad（虽然当前靠 tid==0 侥幸跳过，
     *   但弹对象若复用到旧 homing 槽的 +0x90 就会误转）。只服务灵梦，行为最小。*/
    if (*(const uint32_t *)CE_ADDR_CHARACTER != ANIMAL_CHAR_REIMU) return;
    for (i = 0; i < CE_PB_COUNT; ++i) {
        uint8_t *b = p + CE_PB_POOL + (size_t)i * CE_PB_STRIDE;
        int32_t set, age, tid;
        uint8_t *e;
        const float *ep;
        float *pa, a, diff;

        if (*(const int32_t *)(b + CE_PB_STATE) != 1) continue;        /* 只处理活动弹 */
        set = (int)(*(const uint32_t *)(b + ANIMAL_BULLET_PACKED_ID) >> 8);
        if (set < CE_SHT_SET_EAGLE_1 || set > CE_SHT_SET_EAGLE_4) continue;
        age = *(const int32_t *)(b + ANIMAL_PB_AGE);
        if (age >= 60) continue;   /* 引擎的 60 帧转向窗口已关（TH17/TH18 tick1 同款）——别再转 */
        tid = *(const int32_t *)(b + ANIMAL_PB_TARGET_ID);
        if (!tid) continue;        /* 还没锁到目标：让引擎 tick1 先索敌 */
        e = ce_enemy_by_id(em, tid);
        if (!e || !ce_enemy_lockable(e)) continue;                     /* 目标没了：也让引擎重锁 */
        ep = ce_enemy_pos(e);
        pa = (float *)(b + ANIMAL_PB_ANGLE);
        a = *pa;
        diff = bc_atan2f(ep[1] - *(const float *)(b + ANIMAL_PB_POS_Y),
                         ep[0] - *(const float *)(b + ANIMAL_PB_POS_X)) - a;
        while (diff >  ANIMAL_PI) diff -= 2.0f * ANIMAL_PI;
        while (diff < -ANIMAL_PI) diff += 2.0f * ANIMAL_PI;
        a += diff * ANIMAL_EAGLE_HOMING_EXTRA;
        while (a >  ANIMAL_PI) a -= 2.0f * ANIMAL_PI;
        while (a < -ANIMAL_PI) a += 2.0f * ANIMAL_PI;
        *pa = a;
        ++n;
    }
    if (n > 0 && !s_homing_logged) {
        s_homing_logged = 1;
        ce_log("wba_c: eagle homing assist active (%d body bullets, +%.2f rad/frame extra; TH17 data untouched)",
               n, (double)ANIMAL_EAGLE_HOMING_EXTRA);
    }
}


/* ========================= 咲夜（pl02）EAGLE 卡侧摆动发弹点 =========================
 * 设计 / 数值全部见 `ANIMAL_SAKUYA_EAGLE_*` 常量块（❓ 自定，TH17 无咲夜兽灵行）。
 * 这里只实现：相位递推（无 libm / 无 x87）、每点 VM + 短束、按 PERIOD 出伤害源。*/

/* φ_i = i·2π/N（N=6 ⇒ i·60°）的 sin/cos —— 硬编码常量表（和角公式用）*/
static const float s_sakuya_phi_sin[ANIMAL_SAKUYA_EAGLE_N] = {
    0.0f, 0.8660254f, 0.8660254f, 0.0f, -0.8660254f, -0.8660254f,
};
static const float s_sakuya_phi_cos[ANIMAL_SAKUYA_EAGLE_N] = {
    1.0f, 0.5f, -0.5f, -1.0f, -0.5f, 0.5f,
};

/* 小角 Taylor（|x| ≤ THETA0 = 0.6；sin 误差 < 6e-6、cos 误差 < 1e-4）——纯 SSE、确定性。*/
static float sakuya_sin_small(float x)
{
    float x2 = x * x;
    return x - x * x2 * (1.0f / 6.0f - x2 * (1.0f / 120.0f));
}

static float sakuya_cos_small(float x)
{
    float x2 = x * x;
    return 1.0f - x2 * (0.5f - x2 * (1.0f / 24.0f - x2 * (1.0f / 720.0f)));
}

/* ——— AL61：咲夜 EAGLE **真发弹** ———
 * 直接调引擎 `Player__shoot_one_bullet` `th18:0x45e930`（thiscall(player; packed_id, timer, pos*, option)
 * `ret 0x10`）。与 `ce_damage_rect` 同级：**不是 hook、无新 BP_* / 无 sites.py 改动**。
 * 行参数在 pl02.sht 的 EAGLE 组（`anm=31` 紫刀 / `func_on_init=5` / `opt_slot=0 mode=0`）。
 * ★ AL67：EAGLE_1 现在有 2 行 —— row0 = 扫荡（dmg 20）/ row1 = 正中正面（dmg 40），故加 `row` 参数。*/
typedef int (__attribute__((thiscall)) *sakuya_shoot_one_fn_t)(void *player, int packed_id, int timer,
                                                               const float *pos, void *option);

static int sakuya_eagle_shoot(const float *pos, float dir, int row)
{
    uint8_t *p = CE_PLAYER();
    if (!p) return 0;
    *(float *)(p + CE_PLAYER_AIM_ANGLE) = dir;   /* func_on_init=5（0x4612d0）读它当 bullet.angle */
    return ((sakuya_shoot_one_fn_t)CE_FN_PLAYER_SHOOT_ONE_BULLET)(
        p, (int)((uint32_t)ANIMAL_SAKUYA_EAGLE_BULLET_SET << 8 | (uint32_t)row),
        0, pos, 0);
}

/* 收掉**在飞的**卡侧 EAGLE 弹：扫引擎自机弹池，命中该组的 state=2 + 给弹 VM 发 interrupt 1。
 * ⚠ 只对**咲夜**扫 —— sets 0x24..0x27 在灵梦那里是她的 EAGLE 追尾弹，别误杀。*/
static int sakuya_eagle_kill_bullets(void)
{
    uint8_t *pl = CE_PLAYER();
    int i, n = 0;
    if (!pl || *(const uint32_t *)CE_ADDR_CHARACTER != ANIMAL_CHAR_SAKUYA) return 0;
    for (i = 0; i < CE_PB_COUNT; ++i) {
        uint8_t *b = pl + CE_PB_POOL + (size_t)i * CE_PB_STRIDE;
        uint32_t packed, set;
        if (*(const int32_t *)(b + CE_PB_STATE) != 1) continue;
        packed = *(const uint32_t *)(b + CE_PB_PACKED_ID);
        set = packed >> 8;
        if (set < (uint32_t)CE_SHT_SET_EAGLE_1 || set > (uint32_t)CE_SHT_SET_EAGLE_4) continue;
        *(int32_t *)(b + CE_PB_STATE) = 2;
        ce_anm_interrupt(*(const uint32_t *)(b + CE_PB_VM_ID), 1);
        ++n;
    }
    return n;
}

/* 所有退出路径都调它（松 Z / 咆哮结束 / 对话 / 删卡 / 换面）；path 只进日志。*/
static void sakuya_eagle_kill(animal_card_t *s, const char *path)
{
    int n = 0;
    if (!s) return;
    /* 只在「本次确实跑过」时才扫弹池（否则非咆哮期每帧扫 512 弹太浪费）；过渡帧扫一次就够。*/
    if (s->sakuya_age || s->sakuya_logged) n = sakuya_eagle_kill_bullets();
    if (s->sakuya_age || s->sakuya_logged || n) {
        ce_log("wba_c: sakuya eagle kill (path=%s, fired_age=%d, in-flight_bullets=%d)",
               path ? path : "?", (int)s->sakuya_age, n);
    }
    s->sakuya_age = 0;
    s->sakuya_logged = 0;
}

/* 每帧：咆哮 + EAGLE + 咲夜 + Z 按住 → N 个摆动点上各发一颗**真引擎弹**；否则全收。*/
static void sakuya_eagle_tick(animal_card_t *s)
{
    const float *pp;
    int held, i;

    if (!s) return;
    held = (CE_INPUT_HELD() & 1u) != 0;
    if (!roaring(s) || s->core.hyper_type != 3 || !held ||
        *(const uint32_t *)CE_ADDR_CHARACTER != ANIMAL_CHAR_SAKUYA) {
        sakuya_eagle_kill(s, "tick-lost(not-roaring/no-Z/wrong-char)");
        return;
    }
    pp = ce_player_pos();
    if (!pp) { sakuya_eagle_kill(s, "no-player"); return; }

    if (s->sakuya_age == 0) {          /* 本次激活起点：ω·t = 0 ⇒ sin=0, cos=1 */
        s->sakuya_phase_s = 0.0f;
        s->sakuya_phase_c = 1.0f;
    }
    for (i = 0; i < ANIMAL_SAKUYA_EAGLE_N; ++i) {
        float th, sa, ca, px, py, dir;
        if ((s->sakuya_age + i) % ANIMAL_SAKUYA_EAGLE_PERIOD != 0) continue;
        /* theta_i = THETA0 · sin(ωt + φ_i) = THETA0·(sin·cosφ + cos·sinφ) */
        th = ANIMAL_SAKUYA_EAGLE_THETA0 *
             (s->sakuya_phase_s * s_sakuya_phi_cos[i] + s->sakuya_phase_c * s_sakuya_phi_sin[i]);
        sa = sakuya_sin_small(th);
        ca = sakuya_cos_small(th);
        px = pp[0] + ANIMAL_SAKUYA_EAGLE_R * sa;   /* theta 从正前方向右量 */
        py = pp[1] - ANIMAL_SAKUYA_EAGLE_R * ca;
        dir = -ANIMAL_PI * 0.5f + th;              /* 标准角：-π/2 = 正上 */
        {
            float pos[3] = { px, py, pp[2] };
            (void)sakuya_eagle_shoot(pos, dir, ANIMAL_SAKUYA_EAGLE_BULLET_ROW);
        }
    }
    /* ★ AL65 #4 / AL67：正中正面口 —— 自机正前贴身 (0, −R)、θ=0（标准角 −π/2 = 正上）、**无摆动**。
     * ★ AL67 起走 **EAGLE_1 row 1（dmg 40）**，每 FRONT_PERIOD 帧一颗 ⇒ 40/2 = 20 DPS；
     *   弹流节奏/视觉与 AL65 相同，只是每发伤害翻倍（不增加弹数）。*/
    if (s->sakuya_age % ANIMAL_SAKUYA_EAGLE_FRONT_PERIOD == 0) {
        float pos[3] = { pp[0], pp[1] - ANIMAL_SAKUYA_EAGLE_R, pp[2] };
        (void)sakuya_eagle_shoot(pos, -ANIMAL_PI * 0.5f, ANIMAL_SAKUYA_EAGLE_FRONT_ROW);
    }
    if (!s->sakuya_logged) {
        s->sakuya_logged = 1;
        ce_log("wba_c: sakuya eagle on (%d oscillating points @ every %d frames + 1 front-center "
               "@ every %d frames -> REAL bullets, set 0x%x rows %d/%d, theta0=%.2f omega=%.3f R=%.0f, "
               "dmg %d/%d, sweep %.0f + front %.0f = DPS target %.1f)",
               (int)ANIMAL_SAKUYA_EAGLE_N, (int)ANIMAL_SAKUYA_EAGLE_PERIOD,
               (int)ANIMAL_SAKUYA_EAGLE_FRONT_PERIOD,
               (unsigned)ANIMAL_SAKUYA_EAGLE_BULLET_SET, (int)ANIMAL_SAKUYA_EAGLE_BULLET_ROW,
               (int)ANIMAL_SAKUYA_EAGLE_FRONT_ROW,
               (double)ANIMAL_SAKUYA_EAGLE_THETA0, (double)ANIMAL_SAKUYA_EAGLE_OMEGA,
               (double)ANIMAL_SAKUYA_EAGLE_R, (int)ANIMAL_SAKUYA_EAGLE_DAMAGE,
               (int)ANIMAL_SAKUYA_EAGLE_FRONT_DAMAGE,
               (double)((ANIMAL_SAKUYA_EAGLE_N * (float)ANIMAL_SAKUYA_EAGLE_DAMAGE) /
                        (float)ANIMAL_SAKUYA_EAGLE_PERIOD),
               (double)((float)ANIMAL_SAKUYA_EAGLE_FRONT_DAMAGE /
                        (float)ANIMAL_SAKUYA_EAGLE_FRONT_PERIOD),
               (double)ANIMAL_SAKUYA_EAGLE_DPS_TARGET);
    }
    /* 推进内层相位：乘固定旋转矩阵（纯 SSE）。超时（~1500 帧）累积漂移 ~1e-5，无需重归一化。*/
    {
        float ns = s->sakuya_phase_s * ANIMAL_SAKUYA_EAGLE_OMEGA_COS +
                   s->sakuya_phase_c * ANIMAL_SAKUYA_EAGLE_OMEGA_SIN;
        float nc = s->sakuya_phase_c * ANIMAL_SAKUYA_EAGLE_OMEGA_COS -
                   s->sakuya_phase_s * ANIMAL_SAKUYA_EAGLE_OMEGA_SIN;
        s->sakuya_phase_s = ns;
        s->sakuya_phase_c = nc;
    }
    ++s->sakuya_age;
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
    /* ★ r=28 的**局部**消弹**不认领**闩锁（§5.2 细则 2 只管全屏），且 117 不记颗数
     * ⇒ 后动手的那家扫到空池无害；认领反而会让它挡住 107 的全屏灵击。
     * （鹫束的同类局部消弹已在 2026-09-13 bug-2(b) 整段删除。）*/
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
    s->option_gate_logged = 0;
    s->retail_laser_kill_logged = 0;
    s->roar_bullet_logs = 0;
    s->roar_start_frame = s->frame;
    eagle_beam_kill(s, 0);
    sakuya_eagle_kill(s, "on_ready");  /* AL61：新旧咆哮交替，先清旧弹 */
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
           s->core.hyper_type == 3 ? "eagle option beams (main set empty)" :
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
     *   **类型光环**（自机上的 64×64 `aura_*`）本卡有意留着挂到咆哮结束（用户 2026-09-10）；
     *   **全屏渐变圆**（self_aura）这里按 TH17 `th17:0x40f7bf` 发 interrupt 2（不删句柄），
     *   40 帧后 `hyper_end` 再补 interrupt 1（`th17:0x40f508`）—— 这正是用户 bug7 的一条。*/
    bar_kill(s, 0);
    self_aura_interrupt(s, ANIMAL_SELF_AURA_FADE_BY_HYPER);   /* TH17 `0x40f7bf`：X / 中弹起手 */
    if (s->core.hyper_type == 2) orb_kill(s, 1);   /* TH17 `0x40f7f7`：霊撃起手删 3 个光球 */
    s->shot_index_req_logs = 0;                    /* 霊撃段重新观察 fire-time 请求 */
    s->shot_index_none_logged = 0;
    s->retail_laser_kill_logged = 0;
    s->roar_bullet_logs = 0;
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
     *   下面那个每帧 40 伤害源照打（它是霊撃本体的输出，不是消弹的替代动作）。
     * ★ 2026-09-13（原生卡冲突审计 G 通道）：**消到 0 颗就把闩锁交回** —— 闩锁代表「本帧有人真的
     *   消过弹」，原生 47/19/20/51 之类先清了池时我们数出来的是 0，占着闩锁会把后面的卡白挡一帧。*/
    if (animal_cancel_claim(s)) {
        const int cn = ce_cancel_radius(pp, r, 9999, s->core.by_hit ? 0 : 1);
        if (cn <= 0) (void)ce_cancel_latch_release((uint32_t)ANIMAL_ID_NUM);
    } else {
        animal_cancel_yield_log(s, "wba_wc: hyper radius");
    }
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
        /* ★ 2026-09-13 bug-5：类型由 `hyper_end` 捕获（core 已被清），2 只都带 long_life=1
         * = TH17 的「会变」那版（type 15..17 / `+0x6c |= 2`）—— 与「鱼灵永不换种类」不冲突：
         * 动物灵变色/换种类是 TH17 行为，鱼灵不是。*/
        nburst = animal_end_burst(burst, 2, s->burst_type, ce_rand);
        for (i = 0; i < nburst; ++i) {
            /* 同一基座放 2 只：也强制一左一右；防重叠重试仍按 TH17 一手。*/
            float dir = (i == 0) ? ANIMAL_MISS_HEADING_L : ANIMAL_MISS_HEADING_R;
            (void)token_spawn_heading(burst[i], 1, px, py - ANIMAL_BURST_DY, dir, 1);
        }
        ce_play_sound(ANIMAL_SE_RELEASE, ce_player_x());
        ce_log("wba_wc: release burst (%d animals: type %d %d, roar_type %d%s, se 0x3e)",
               (int)nburst, (int)burst[0], (int)(nburst > 1 ? burst[1] : 0), (int)s->burst_type,
               (s->burst_type >= ANIMAL_TYPE_GOAST_MIN && s->burst_type <= ANIMAL_TYPE_GOAST_MAX)
                   ? " (matches roar type)" : " (mixed/unknown -> TH17 random fallback)");
    }
}

/* 咆哮结束：鱼灵一律换道具；正常结束（没用霊撃）再放 2 个变色动物灵。 */
static void hyper_end(animal_card_t *s, int32_t by_hyper)
{
    int32_t fish[ANIMAL_SLOT_MAX];
    int nfish, i;
    const float *pp = ce_player_pos();
    float px = pp ? pp[0] : 0.0f, py = pp ? pp[1] : 0.0f;

    /* ★ 2026-09-13 bug-5：在 `animal_clear_and_restore()` 把 core 清空**之前**把本次咆哮类型
     *   抄进卡状态 —— 释放动画要 120 帧后才爆散，那时 core.hyper_type 已经是 0。
     *   只在没有待播释放动画时才写：避免「上一发释放还没爆散，新一发又结束」把类型改掉（边缘情况）。*/
    if (s->release_wait <= 0) s->burst_type = s->core.hyper_type;

    eagle_beam_kill(s, 0);      /* 咆哮结束 → 卡侧 EAGLE 矩形束立即收掉 */
    sakuya_eagle_kill(s, by_hyper ? "hyper_end(X/cancel)" : "hyper_end(normal)");
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

    /* ★ bug7（2026-09-13）：TH17 `th17:0x40f4fc` 在**自然结束与霊撃结束**的公共尾部都
     * `push 0x42`（`0x40f4d0` 起两条路汇合）⇒ **每一条退出路径恰好播一次**
     * ANIMAL_SE_ROAR_END。旧代码只在 `!by_hyper` 分支里播，X / 中弹路径漏播 —— 提到 if 之前统一播。
     * 试听探针（debug）仍只在自然结束时启动并抑制那一声。*/
#if ANIMAL_DEBUG_KEY && ANIMAL_SE_PROBE
    if (!by_hyper) {
        s->se_probe_idx = 0;
        s->se_probe_left = ANIMAL_SE_PROBE_N;
        s->se_probe_gap = 0;
        ce_log("wba_wc: SE probe active (%d candidates, %d frames apart); normal roar-end SE 0x%02x suppressed",
               ANIMAL_SE_PROBE_N, ANIMAL_SE_PROBE_GAP, (unsigned)ANIMAL_SE_ROAR_END);
    } else {
        ce_play_sound(ANIMAL_SE_ROAR_END, px);
    }
#else
    ce_play_sound(ANIMAL_SE_ROAR_END, px);
#endif

    if (!by_hyper) {
        /* ★ 自然结束：先出**释放动画**（`beastap`），2 只灵等 `ANIMAL_RELEASE_FRAMES` 帧后（release_tick）才爆散
         *   —— 一手 TH17 `0x40f494` 建脚本 0x2c + `release_timer` 到 120 帧才调 `0x40e920`。*/
        release_spawn(s);
        ce_log("wba_wc: hyper end #%d (normal; %d fish -> items; release anim %d, burst in %d frames)",
               ++s->ends, nfish, (int)s->release_anim, (int)ANIMAL_RELEASE_FRAMES);
    } else {
        ce_log("wba_wc: hyper end #%d (by hyper; %d fish -> items, no release)", ++s->ends, nfish);
    }

    if (animal_clear_and_restore(&s->core)) on_ready(s);
}

#if ANIMAL_DEBUG_KEY && ANIMAL_SE_PROBE
/* 正常咆哮退出后的 6 候选试听。第一条在结束当帧播，之后每 ANIMAL_SE_PROBE_GAP（180 帧 = 3 s）
 * 一条；对话暂停时冻结。★ 默认 ANIMAL_SE_PROBE 0：整段不编译，无任何日志。*/
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
        s->se_probe_gap = ANIMAL_SE_PROBE_GAP;
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

/* —— ★ 中 Boss（MBoss*）的 TH17 口径 ——
 * 一手（TH17 v1.00b，`thecl -d 17`；文件/子程序/行号见 AUDIT §AL56）：
 *   · `st0[1-6]mbs` 只有单相（`setInterrupt(1,0,t,"MBossDead")`），**没有**符卡边界；
 *     `MBossDead` 里 `spellEnd(); BossItemPhase(...); dropMain(19); dropItems(); dropMain(20); dropItems();`
 *     ⇒ 动物灵固定是 **item19→token4 + item20→token5** 各 1。
 *   · `st05mbs` / `st06mbs` 在 19/20 之后还有**无条件** `dropMain(30); dropItems();`
 *     （item30 → token15 = 长寿命狼灵），本卡用 `token_spawn(1, long_life=1)` 表达。
 *   · `st01..st04` / `st07` 的 `BossItemPhase` 主掉落是**条件稀有动物**（item23..29 → token8..14）；
 *     稀有动物贴图 / 脚本不在本卡 asset 范围（见 AUDIT §AL56 的 🟡），这里只按一手表记日志、不发 VM，
 *     以免用错种类冒充。**不改变**「资源灵 4+5」这条硬口径。
 *   · `st07mbs`（Extra）另有两次相位边界 `MBoss2` / `MBoss3`：`BossItem(33,50,50); BossItem(20,0,0);
 *     BossItem(19,0,0);` ⇒ 1 只随机长寿命动物灵 + 资源灵 4+5。卡侧相位边界走 `animal_drop_midboss_phase()`。*/
static const char *animal_midboss_stage_name(int stage)
{
    static const char *const names[8] = {
        "st??mbs", "st01mbs", "st02mbs", "st03mbs", "st04mbs", "st05mbs", "st06mbs", "st07mbs"
    };
    if (stage < 0 || stage > 7) stage = 0;
    return names[stage];
}

/* `CURRENT_STAGE` 是 1-based（engine/card/th18/05：dmode 1–5 与 CURRENT_STAGE 直接比较）。
 * 读不到 0 / 越界时夹到 1..7，只影响日志与 st05/06 的无条件 tok15 分支；不会误判成别的面。*/
static int animal_midboss_stage(void)
{
    int st = CE_CURRENT_STAGE();
    if (st <= 0) return 1;
    if (st > 7) return 7;
    return st;
}

static void animal_drop_midboss_death(animal_card_t *s, float x, float y, const char *sub)
{
    int st = animal_midboss_stage();
    const char *fn = animal_midboss_stage_name(st);
    (void)s;

    /* 固定 19 + 20：各一张（TH17 `dropMain(19)` / `dropMain(20)` 各一条 `dropItems`）。*/
    (void)token_spawn(4, 0, x, y);   /* item19 -> token4：炸弹碎片灵 */
    (void)token_spawn(5, 0, x, y);   /* item20 -> token5：残机碎片灵 */
    if (st == 5 || st == 6) {
        /* TH17 st05/st06 的无条件 `dropMain(30)`：item30 -> token15（狼灵的长寿命变体）。*/
        (void)token_spawn(1, 1, x, y);
        ce_log("wba_wc: midboss drop -> 2 resource spirits (4+5) + 1 long-life wolf (item30/token15) "
               "(TH17 %s: BossItemPhase(...) + dropMain(19) + dropMain(20) + dropMain(30)) at (%.0f, %.0f)"
               " [sub=%s]",
               fn, (double)x, (double)y, sub ? sub : "(null)");
    } else {
        ce_log("wba_wc: midboss drop -> 2 resource spirits (4+5) "
               "(TH17 %s: BossItemPhase(...) + dropMain(19) + dropMain(20)) at (%.0f, %.0f) [sub=%s]",
               fn, (double)x, (double)y, sub ? sub : "(null)");
    }
    /* 条件主掉落（st01 item23 / st02 item24 / st03 item25 / st04 item26 / st05 item27 /
     * st06 item28 / st07 item29）是稀有动物 token8..14：本卡无对应贴图/脚本。
     * 这里单独记一行，实机可据此区分「资源灵对了，但稀有没出」而不是「整个中boss没掉」。*/
    ce_log("wba_wc: midboss rare conditional (TH17 %s: BossItemPhase main 23..29 -> token8..14) "
           "not spawned: rare animal asset not in card-117 scope (AUDIT §AL56 / TUNING §2.3)", fn);
}

static void animal_drop_midboss_phase(animal_card_t *s, float x, float y)
{
    int st = animal_midboss_stage();
    int32_t t = 1 + (int32_t)(ce_rand() % 3);
    /* TH17 st07mbs MBoss2 / MBoss3：BossItem(33,50,50) = item33 -> rand%3+30 -> token15..17
     * （长寿命随机动物）；BossItem(20,0,0) = token5；BossItem(19,0,0) = token4。*/
    (void)token_spawn(4, 0, x, y);
    (void)token_spawn(5, 0, x, y);
    (void)token_spawn(t, 1, x, y);
    ++s->drops;
    ce_log("wba_wc: midboss phase drop -> 1 long-life animal (type %d, item33) + 2 resource spirits "
           "(4+5) (TH17 %s: BossItem(33,50,50) x1 + BossItem(20,0,0) x1 + BossItem(19,0,0) x1) "
           "at (%.0f, %.0f)",
           (int)t, animal_midboss_stage_name(st), (double)x, (double)y);
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
            if (s->phase_is_mid)
                animal_drop_midboss_death(s, s->boss_x, s->boss_y, "MBossDead(no-drop-event)");
            else
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

        if (!s->phase_drop_seen && bp) {
            if (old_mid)
                animal_drop_midboss_phase(s, bp[0], bp[1]);   /* ★ 中 Boss 相位边界照 TH17 掉 */
            else if (old_spell)
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

/* ★ AL57：道中击破 → 1 动物灵（`on_enemy_death`，断点 0x430240；**在引擎撒主掉落之前**）。
 * 死亡栈上只登记标量；真正建 VM / 打掉落行在 `on_tick_2` 的 `animal_big_drop_drain()`。
 *
 * 判据（2026-09-13 用户定案，详见上方 AL58 块）：
 *   ① 贴图尺寸 ≥ 64×64 ⇒ reason=size；
 *   ② 小体型 ⇒ 同波（MainSub 数字）内按族名+尺寸分组：
 *        组内数量 ≤ 3 ⇒ reason=odd-one-out（用户原话「<=3 也可以叫鹤立鸡群」）；
 *        >3 且存在过半多数派、当前组不是它 ⇒ reason=odd-one-out；
 *        纯杂鱼波的多数组（Cir 10/10、9/9；GirlB01×8）⇒ 不掉。*/

static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    animal_card_t *s = st(c);
    animal_big_drop_t d;
    int32_t hp_cur = 0, hp_max = 0, drop_main = 0, sub = -1, anm_main = 0;
    int32_t sprite_w = 0, sprite_h = 0, big_by_size = 0, wave = -1, wave_src = 0, odd = 0;
    int32_t census = 0, wave_total = 0, group_total = -1, veto = 0, reason = 0;
    float hurtbox_x = 0.0f;
    uint32_t flags;
    const char *name;
    int drop_decision, diag = 0;

    if (!s || !ev || !ev->enemy) return;

    /* ★ 先累积本波 census，再判据；这样 death diag 里的 census 已经包含目标。*/
    animal_wave_census_update();

    flags = *(const uint32_t *)(ev->enemy + CE_ENEMY_FLAGS);
    name = animal_enemy_sub_name((const uint8_t *)ev->enemy, &sub);
    drop_decision = animal_enemy_drop_measure((const uint8_t *)ev->enemy, name,
                                               &hp_cur, &hp_max, &drop_main,
                                               &anm_main, &hurtbox_x,
                                               &sprite_w, &sprite_h, &big_by_size,
                                               &wave, &wave_src, &odd, &census, &wave_total,
                                               &group_total, &veto, &reason);

    /* ★ 先诊断、后判据：前 16 只死亡逐只打印；**再额外保证 4 只非 Cir/ECir 族**打一行。
     *   用户实测前半面 16 只全被 Cir00 占满，三只 GirlD01 根本没进样本。
     *   ★ AL60：非 Cir 配额去掉 `drop_decision` 前置 —— 回归（GirlD01 drop=0）当年正是因为
     *   「drop=0 的不打诊断」而完全看不见；现在前 4 只非 Cir **无论掉不掉**都打。*/
    if (s_big_diag_left > 0) {
        --s_big_diag_left;
        diag = 1;
    } else if (s_big_diag_noncir_left > 0 && !animal_name_is_swarm_guard(name)) {
        --s_big_diag_noncir_left;
        diag = 1;
    }
    if (diag) {
        ce_log("wba_wc: death diag kind=%s retaddr=0x%08x ret_slot=%d esp=0x%08x raw_flags=0x%08x "
               "real_bit=%d id=%u sub=%d/%s hp=%d/%d hurtbox=%.1f main_drop=%d anm_main=%d "
               "sprite=%dx%d big_by_size=%d wave=%d wave_src=%s veto=%s odd=%d drop=%d "
               "group_total=%d census=%d total=%d pos=(%.0f,%.0f)",
               animal_death_kind_name(ev->kind), (unsigned)ev->retaddr, (int)ev->ret_slot,
               (unsigned)ev->esp, flags, (ev->flags & CE_EDEATH_F_REAL) ? 1 : 0, (unsigned)ev->id,
               (int)sub, name ? name : "(null)", (int)hp_cur, (int)hp_max, (double)hurtbox_x,
               (int)drop_main, (int)anm_main, (int)sprite_w, (int)sprite_h,
               (int)big_by_size, (int)wave, animal_wave_src_name(wave_src),
               animal_wave_veto_name(veto), (int)odd, drop_decision,
               (int)group_total, (int)census, (int)wave_total,
               (double)ev->x, (double)ev->y);
    }

    if (!animal_big_death_counts(ev->kind)) {
        /* ★ 实跑兜底：thcrap 栈上扫描也没命中任何已知 call site（retaddr=0 / ret_slot=-1）时，
         *   只要这是 real_bit=1 的具名敌机、HP 已 ≤ 0、族名不在 Cir/ECir（旧黑名单仅用于这里的死因识别）
         *   ⇒ 按 KILLED 处理。用户口径「我打死它才掉」；信息层拿不到明确清屏/自灭时按 KILLED 处理比漏掉更稳。
         *   ⚠ 掉落判据仍走 AL57 的贴图尺寸 + 同波统计，不再受这个前缀表影响。*/
        int scan_miss = (ev->retaddr == 0 && ev->ret_slot < 0);
        if (!(scan_miss && (ev->flags & CE_EDEATH_F_REAL) && hp_cur <= 0 &&
              animal_name_is_fallback_noncir(name))) return;
        ce_log("wba_wc: death kind fallback -> KILLED (call-site scan miss, retaddr=0x%08x "
               "ret_slot=%d hp=%d/%d sub=%s sprite=%dx%d big_by_size=%d wave=%d wave_src=%s "
               "veto=%s odd=%d drop=%d group_total=%d census=%d total=%d)",
               (unsigned)ev->retaddr, (int)ev->ret_slot, (int)hp_cur, (int)hp_max,
               name ? name : "(null)", (int)sprite_w, (int)sprite_h, (int)big_by_size,
               (int)wave, animal_wave_src_name(wave_src), animal_wave_veto_name(veto),
               (int)odd, drop_decision, (int)group_total, (int)census, (int)wave_total);
    }
    if (!animal_big_real_ok(ev, name, anm_main, hurtbox_x, drop_main)) return; /* REAL + 正判兜底 */
    if (!drop_decision) return;

    if (enemy_is_boss(ev->enemy)) {
        int is_mid = (name && strncmp(name, "MBoss", 5) == 0);
        ce_log("wba_wc: big-enemy death (%s) -> no extra animal spirit; %s handles it "
               "(hp=%d/%d, drop_id %d, sub %d, sprite=%dx%d, wave=%d wave_src=%s, veto=%s, "
               "odd=%d, group_total=%d census=%d total=%d) at (%.0f, %.0f)",
               is_mid ? "midboss" : "boss",
               is_mid ? "midboss drop path" : "existing *Dead drop_pre",
               (int)hp_cur, (int)hp_max, (int)drop_main, (int)sub,
               (int)sprite_w, (int)sprite_h, (int)wave, animal_wave_src_name(wave_src),
               animal_wave_veto_name(veto), (int)odd, (int)group_total, (int)census,
               (int)wave_total, (double)ev->x, (double)ev->y);
        return;
    }

    d.x = ev->x;
    d.y = ev->y;
    d.hp_cur = hp_cur;
    d.hp_max = hp_max;
    d.drop_main = drop_main;
    d.sub = sub;
    d.sprite_w = sprite_w;
    d.sprite_h = sprite_h;
    d.big_by_size = big_by_size;
    d.wave = wave;
    d.wave_src = wave_src;
    d.odd = odd;
    d.census = census;
    d.wave_total = wave_total;
    d.group_total = group_total;
    d.veto = veto;
    d.reason = reason;
    (void)animal_big_drop_push(&d);
}

static void on_enemy_drop_pre(ce_card_t *c, int32_t *counts)
{
    animal_card_t *s = st(c);
    const uint8_t *e;
    const float *ep;
    int is_boss;
    const char *sub;
    int32_t sub_idx = -1;
    int is_death;
    int is_mid;

    if (!s || !counts) return;
    e = (const uint8_t *)counts - CE_ENEMY_DROP_COUNTS - CE_ENEMY_DROP;
    is_boss = enemy_is_boss(e);
    ++s->kill_count;
    if (!is_boss) return;                        /* 杂兵不掉动物灵（TH17 杂兵只有 item 1/2/3）*/
    ep = ce_enemy_pos(e);
    if (!ep) return;

    /* TH18 `dropItems` 对 TH17 的每一条 `BossItem` 触发一次。先按 ECL 当前子程序名分**死亡**：
     * - `MBossDead`（名以 `MBoss` 开头）= 中 Boss：走 `animal_drop_midboss_death()`（TH17 stXXmbs 口径）；
     * - 其余名含 `Dead` = 关底 Boss：保持既有 `*Dead -> 2 资源灵(4+5)`（TH17 相位口径）；
     * - 非死亡（`Boss2` / `BossCard1` / `MBoss2` …）按相位 kind 发。
     * 相位结束后 `on_tick_2` 还会看到新的 phase_sig；`phase_drop_seen` 防止同一相位重发。*/
    sub = animal_enemy_sub_name(e, &sub_idx);
    is_mid = (sub && strncmp(sub, "MBoss", 5) == 0) ? 1 : 0;
    is_death = (sub && strstr(sub, "Dead")) ? 1 : 0;
    if (is_death) {
        if (s->death_dropped) return;            /* 同一死亡阶段多条 BossItem 只结一次 */
        if (is_mid)
            animal_drop_midboss_death(s, ep[0], ep[1], sub);
        else
            animal_drop_resource_pair(s, ep[0], ep[1], "boss death", 0, 0);
        s->death_dropped = 1;
        s->phase_drop_seen = 1;                  /* 最后相位由本事件结账，兜底不再补 */
        return;
    }

    if (s->phase_drop_seen) return;              /* 同一相位多条 BossItem 只结一次 */
    s->phase_drop_seen = 1;                      /* 本相位已有 drop 事件，边界不再重复发 */
    if (is_mid) {
        /* ★ TH17 中 Boss 不是「中间相位不掉」：st07mbs 的 MBoss2 / MBoss3 明确掉
         * `BossItem(33)+BossItem(20)+BossItem(19)`（AUDIT §AL56）。主线 st01-06 没有
         * 相位边界（只有计时中断到 MBossDead），所以这条只在 Extra 真正触发。*/
        animal_drop_midboss_phase(s, ep[0], ep[1]);
        return;
    }
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
    int32_t type_l = 0, type_r = 0;   /* ★ 两只**各自独立随机**（用户 2026-09-13 口径）*/

    if (!s) return 0;
    pp = ce_player_pos();
    /* ★ 2026-09-13 二十一修（用户口径）：TH17 的**凭依兽灵是可选配置**（像其它作的配置一样选），
     *   本卡设定为「她们**没有被凭依**」（`ANIMAL_GOAST_NONE`）⇒ 类型**不能**按自机序号推
     *   （魔理沙 ≠ 獭）。中弹掉落与 boss 掉落的 `1 + rand%3` 同口径。
     *   ★ 2026-09-13 用户口径（本条）：**两只各自独立随机** —— 不再"掷一次、两只共用"，
     *     所以同一拨的两只**可能**是不同种类（左/右各掷一次）。*/
    {
        /* ★ 用 `on_death_before_deathbomb` 存下的**中弹点**（不是 respawn 后的自机位），
         *   并且左右各放 32 px（> 24 px 的收取半径）—— 否则会在复活点被当场收走。*/
        float bx = s->death_valid ? s->death_x : (pp ? pp[0] : 0.0f);
        float by = s->death_valid ? s->death_y : (pp ? pp[1] : 0.0f);
        /* 基座 = 中弹点**正前方** (x, y − 64)，两颗同一基座、靠各自随机初速散开（TH17 `0x40e920` 同款）*/
        /* ★ 2026-09-13 二十修（用户 bug）：「魔理沙中弹后生成的是固定两只水獭灵、不会切换」；
         *   契约 = **从头到尾不会出现不切换的灵** ⇒ 中弹掉落的两只改成会变色/换种类的那一支
         *   （`long_life = 1`，即 TH17 type 15..17 那一路）。类型仍取本机体的凭依兽灵
         *   （`1 + 自机序号 % 3`，魔理沙 = 2 獭；换种类循环 2→3→1→2 …）。*/
        /* ★ 用户 2026-09-13：「死亡后掉的动物灵，应当保证是走向左右两个方向」。
         *   基座相同 ⇒ 强制初速一左（135°, vx<0）一右（45°, vx>0）；去重叠重试仍按 TH17 一手
         *   （两点积 0 < 0.96，所以不会触发同向重试，也不会有重叠）。*/
        type_l = 1 + (int32_t)(ce_rand() % 3);
        type_r = 1 + (int32_t)(ce_rand() % 3);
        (void)token_spawn_heading(type_l, 1, bx, by - ANIMAL_BURST_DY, ANIMAL_MISS_HEADING_L, 1);
        (void)token_spawn_heading(type_r, 1, bx, by - ANIMAL_BURST_DY, ANIMAL_MISS_HEADING_R, 1);
        s->death_valid = 0;
    }
    ce_log("wba_wc: miss -> 2 cycling animals (types %d/%d, each independently random, long_life=1, dir L135/R45)",
           (int)type_l, (int)type_r);
    return 0;
}

/* ★★ 2026-09-21（owner 口径「117 不应该是主动卡」，`WORK-PACKAGES.md` §1b）：**`on_activate` 已删除**。
 *
 *   它原来做的事只有一句 `hyper_begin(s, 0)` —— 与 `on_tick_2` 里 X 那一条
 *   （`ready && !paused && (INPUT_PRESSED & 0x002)` → `hyper_begin(s, 0)`）**逐字相同**
 *   ⇒ 删掉 C 入口**不丢任何效果**，剩下的两条触发全是游戏状态 / 引擎掩码内的键：
 *     ① 「咆哮就绪」= **满槽自动**（`animal_collect` → `on_ready`，一手 TH17 `AnimalManager__on_ready`
 *        `th17:0x40fb60`：HYPER_FLAGS bit1 当场置上，`Player__do_shooting` 立刻换主炮组）；
 *     ② 「把就绪的咆哮用掉」= **X（炸弹键，掩码 bit1）**（一手 TH17 `player_is_trying_to_bomb`
 *        `th17:0x411d40`：`HYPER_FLAGS & 6 != 0` 时返回 0 ⇒ 炸弹键在咆哮期间被占用）。
 *   ⇒ 本卡自此**没有任何主动行为**：不声明 `.active_recharge` ⇒ SDK 的 `ce_sdk_c_press` 第一句
 *     `if (!h->active_recharge) return 0;` 直接返回 0（= 基类槽 `xor eax,eax; ret` 的效果，
 *     见 `sdk.h` 顶注），绑定时也不再走 `active_init()` ⇒ **flags 里不再有 `CE_FLAG_ACTIVE`（bit3）**
 *     ⇒ `set_selected_active_card(mgr, -1)`（`th18:0x408b00`）不会选中它、HUD 主动行也不画它。
 *   ⚠ 随之而来的两条**下游**改动（不在本文件里，已在报告与 `AUDIT.md` §BE 登记）：
 *     `patch/th18/cards.js` 的 `"category": 0` 必须去掉（默认 2 = 被动能力卡）—— 契约
 *     `category == 0 ⟺ active_recharge != 0`（`native/tools/contract_check.py` 判据 1、
 *     运行时 `ce_sdk_contract_check` reason 1）否则**整局新卡禁用**。*/

/* TH17 `player_is_trying_to_bomb` `th17:0x411d40` 在 `HYPER_FLAGS & 6 != 0` 时返回 0 ⇒
 * 咆哮就绪 / 霊撃期间炸弹键只作咆哮动作。TH18 的原生门在 `Player__on_tick__body` 的
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
    s->core.goast = goast_for_deck();       /* TH18 无凭依选择：默认「主凭依灵不存在」；带 25/34 时恒存在 */
    paused = dialogue_open();
    animal_big_drop_drain(s);               /* AL55：上一帧大怪击破队列 → 本帧建 VM + 打掉落行 */
    animal_wave_census_update();            /* AL58：每帧把当前可计入敌人累积进本波 census */
    se_volume_fix();

    /* ★ 用户 2026-09-11：TH17 里「咆哮模式」满槽自动成立，**炸弹键（输入位 0x2 = X）**
     *   是把它**用掉**（`do_hyper_cancel(this,0)`：40 帧消弹 + 伤害源 + 60 帧无敌）——
     *   玩家手感上「按 X = 中弹」，区别只是不扣命（一手：`engine/animal/th17/01` §5.2 激活段）。
     *   TH17 在这个状态下会**屏蔽原版炸弹**（`player_is_trying_to_bomb` `0x411d40` 要求
     *   `HYPER_FLAGS & 6 == 0`）；本卡由 `on_bomb_gate` + SDK 断点 `BP_ce_bomb_gate`
     *   （`th18:0x45c048`）在 `ready || hypering` 时拦下原生 `do_bomb`，只让 X 走这段。*/
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
                /* ★ 用户 2026-09-13：鱼灵不换种类 ⇒ 调试键也不给它 long_life（否则 VM 每 240 帧
                 * 重建 + 播 0x4e，用户就是这么复现的）。token_spawn 里还有一道无条件门兜底。*/
                (void)token_spawn(t, 0, ANIMAL_DEBUG_X, ANIMAL_DEBUG_Y);
                ce_log("wba_wc: DEBUG spawn resource type %d at (%.0f, %.0f) by SHIFT+SPACE", (int)t,
                       (double)ANIMAL_DEBUG_X, (double)ANIMAL_DEBUG_Y);
            } else {
                (void)token_spawn(t, animal_type_is_fish(t) ? 0 : 1, ANIMAL_DEBUG_X, ANIMAL_DEBUG_Y);
                ce_log("wba_wc: DEBUG spawn spirit type %d at (%.0f, %.0f) by SPACE", (int)t,
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
        /* ★ 撒道具的「大量」播音：2 帧一声，播完 n 声。
         *   id 由 scatter_se_select() 在段首认一次，2026-09-12 用户耳听定案 = ANIMAL_SE_FISH_ITEMS
         *   （0x3e，试听候选表里的 C；A 0x34 / B 0x43 已否）。*/
        if (s->se_left > 0) {
            if (s->se_gap <= 0) { ce_play_sound((uint32_t)s->se_scatter_id, ce_player_x()); --s->se_left; s->se_gap = 2; }
            else --s->se_gap;
        }
        release_tick(s);        /* ★ 释放动画 / 延迟爆散（对话中冻结，见下）*/
    } else if (s->se_left > 0) {
        /* ★ 2026-09-13 bug-3（对话音效不推迟）：对话期**直接把待播序列丢掉**，不攒到对话结束。
         *   `fish_to_items()` 已经在入队时按 `dialogue_open()` 拦了一手；这里兜底处理
         *   「进对话前已排到一半」的序列。release_tick（释放动画）**不动** —— 那是另一件事，
         *   照旧在对话中冻结。*/
        ce_log("wba_wc: discard %d pending scatter SE (dialogue, id 0x%02x) — 不推迟到对话后",
               (int)s->se_left, (unsigned)s->se_scatter_id);
        s->se_left = 0;
        s->se_gap = 0;
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
    else eagle_beam_kill(s, 1);   /* 对话中不持续刷伤害（空数组时是 no-op）*/
    /* ★ 2026-09-13（AL59）：咲夜 EAGLE = 卡侧摆动发弹点。对话中也收掉（同鹫束的理由）。*/
    if (!paused) sakuya_eagle_tick(s);
    else sakuya_eagle_kill(s, "paused(dialogue)");
    /* ★ 2026-09-13 bug-1：pl00/pl02/pl03 的 EAGLE body 追尾弹收敛补正（对话中弹被冻住，别转）。*/
    if (!paused) eagle_homing_assist();
#if ANIMAL_DEBUG_KEY && ANIMAL_SE_PROBE
    se_probe_tick(s, paused);
#endif
    tokens_tick(s, paused);
    return 0;
}

/* HUD 全在左下角 5 格 VM 里；VM 的建/删放在 on_draw（原生 `AbilityManager__on_draw` 相位）。*/
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

/* 复位本卡所有跨帧状态（VM / 实体 / core / 队列 / 旗标）。
 * `keep_hud_slot` = 1 时**不动 HUD 槽位认领**（`ce_hud_slot_release` / `s_slot_ok`）——
 * `on_stage_start` 用（槽位归 `on_load` / `on_run_reset` 管，见 on_load 注释）。*/
static void clear_card(ce_card_t *c, int keep_hud_slot)
{
    animal_card_t *s = st(c);
    if (!s) return;
    hud_clear(s);
    /* ★ 2026-09-12：HUD **槽位**也在这里退（`CE_CARD` 的 `on_run_reset`/`opdelete` 会先调
     * `ce_layer_drop_card()` → `ce_hud_slot_release()`，这里显式再来一次是幂等的）。
     * 注意 pinned 的 release 语义与货架一致：只标记「保留」，不当场动别人的排布。*/
    if (!keep_hud_slot) {
        ce_hud_slot_release(117);
        s_slot_ok = 0;
    }
    eagle_beam_kill(s, 0);
    sakuya_eagle_kill(s, "clear_card");  /* AL61：删卡/换面收在飞弹 */
    if (s->release_anim) { ce_anm_delete((uint32_t)s->release_anim); s->release_anim = 0; }
    s->release_wait = 0;
    aura_kill(s, 1);
    self_aura_kill(s, 1);
    bar_kill(s, 1);
    orb_kill(s, 1);
    if (s->strike_id) { ce_anm_delete((uint32_t)s->strike_id); s->strike_id = 0; }
    token_clear_all();
    animal_big_drop_clear();         /* AL55：未兑现的大怪击破队列也随换关/删卡清掉 */
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
    s->option_gate_logged = 0;
    s->retail_laser_kill_logged = 0;
    s->roar_bullet_logs = 0;
    s->roar_start_frame = 0;
    memset(s->beam_id, 0, sizeof s->beam_id);
    s->beam_n = 0;
    s->beam_logged = 0;
    /* ★ AL61：咲夜 EAGLE 现在是真引擎弹，没有卡侧句柄；相位/帧数清 0 即可。*/
    s->sakuya_age = 0;
    s->sakuya_logged = 0;
    s->sakuya_phase_s = 0.0f;
    s->sakuya_phase_c = 1.0f;
    s->eagle_body_logs = 0;
    s->se_left = 0;
    s->se_gap = 0;
    s->se_scatter_id = ANIMAL_SE_FISH_ITEMS;
    s->se_probe_left = 0;
    s->se_probe_idx = 0;
    s->se_probe_gap = 0;
    s->orb_spawn_logged = 0;
    s->orb_log_last = 0;
    s->cancel_yield_logged = 0;      /* 消弹让位日志（§5.2 细则 2）*/
    s->burst_type = 0;               /* ★ bug-5：本次释放类型也一并清 */
    hud_dim_init(&s->hud);           /* ★ 2026-09-15g：压暗状态复位 = **满亮 255**（共享件）*/
    s->hud_fade_touched = 0;
    s_homing_logged = 0;             /* ★ bug-1：追尾补正日志每关重打一次 */
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

/* ★ 2026-09-13 bug-4：**换面**（1面→2面 这类关卡切换）时整只清空动物灵 + 咆哮状态。
 *   挂 `on_stage_start`（虚表 +0x34）= `GameThread__thread_start` `0x442F98` 的「每关开场」广播：
 *   只在**关卡游玩线程开场**跑一次。**进商店不经过它** —— 商店是另一个 scene，只有
 *   `on_load`（`AbilityManager__notify_cards_on_load`）会跑 ⇒ 用户明确要求「商店不清除」，
 *   这条路径一个字节都不碰。
 *   与 `clear_card(c, 0)` 的唯一差别 = `keep_hud_slot=1`：槽位认领留给 `on_load`（商店那步已认领）。
 *   清的内容 = 5 个动物灵实体（含 `s_n_tok` 与每只字段）+ HUD 绘制 + 咆哮 VM（束/光环/条/光球/
 *   释放动画/霭击光圈）+ `core` 队列计数 + 待播 SE —— 下一帧 `on_draw`/`hud_sync` 按空 core 重建。*/
static void on_stage_start(ce_card_t *c)
{
    animal_card_t *s = st(c);
    if (!s) return;
    clear_card(c, 1);
    ce_log("wba_wc: stage start -> animal tokens (%d slots) + core/HUD/roar VMs cleared; HUD slot kept "
           "(shop path = on_load is NOT touched)", ANIMAL_TOKEN_MAX);
}

static void on_run_reset(ce_card_t *c) { clear_card(c, 0); }
static void on_delete(ce_card_t *c) { clear_card(c, 0); }

CE_CARD(117,
        /* ★★ 2026-09-21（owner 口径「117 不应该是主动卡」）：**这里原来有 `.active_recharge = 1,` 与
         *   `.on_activate = on_activate,` 两行** —— 一起删掉即「改被动」的全部改动（SDK 侧判定主动卡的
         *   唯一开关就是 `active_recharge` 非 0：`sdk.c` 的 `active_init` / `ce_sdk_c_press` /
         *   `ce_sdk_active_tick` / `ce_sdk_active_reset` 四处的第一句都是它）。
         *   触发改为**游戏状态驱动**：满槽自动 `on_ready`（→ 咆哮就绪）+ X 用掉（`on_tick_2` 里那条，
         *   一手 TH17 `player_is_trying_to_bomb` `th17:0x411d40`）⇒ **不占 C 键、不引入任何新键**。
         *   完整五段链 + 下游 `cards.js` 契约改动见 `engine/input/th18/01-key-mapping.md` §2/§4a。*/
        .on_bomb_gate = on_bomb_gate,
        .on_shot_set_index = on_shot_set_index,
        /* ★★ 2026-09-16z（用户口径「改子机和改主体发弹的覆盖原则」）：「**人为规定 117 > 113**」
         *   ⇒ 本卡优先级 = `CE_SHT_PRIO_117`（20）> 113 灵界的 `CE_SHT_PRIO_113`（10）。
         *   落点 = `sdk/public.h` 的阶梯说明 + `sdk.c` 的 `BP_ce_shot_set_index`（同帧多张卡都要换 set 时取最大）。
         *   ⚠ 这条是**人为规定**，不是从一手推出来的；同一面上 108/111 都不参与（它们不换主炮 set）。*/
        .shot_set_index_priority = CE_SHT_PRIO_117,
        .on_option_shot_gate = on_option_shot_gate,
        .on_bullet_created = on_bullet_created,
        .on_tick_2 = on_tick_2,
        .on_draw = on_draw,
        .on_enemy_death = on_enemy_death,       /* ★ AL55：大怪击破 → 1 动物灵（非 boss；帧末兑现）*/
        .on_enemy_drop_pre = on_enemy_drop_pre,
        .on_death_before_deathbomb = on_death_before_deathbomb,
        .on_death_frame2 = on_death_frame2,
        .on_load = on_load,
        .on_stage_start = on_stage_start,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
