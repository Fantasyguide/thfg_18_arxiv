/* sdk_core.h —— 行为 SDK 里不依赖平台的部分：行为注册表、与 JSON 的对账、私有状态槽。
 *
 * 纯 C11（只 <stdint.h>），主机上 make test-host 直接测。桩 / 断点 / 引擎访问在 sdk.h / sdk.c。
 * 设计见 ../SDK.md §3–§4。
 */
#pragma once
#include <stdint.h>

#define CE_SDK_MAX_BEHAVIORS 71        /* = CE_CARD_MAX_NEW：JSON 最多登记这么多，行为不可能更多 */
#define CE_SDK_CARD_ID_MIN   58
#define CE_SDK_CARD_ID_MAX   254
#define CE_STATE_SLOTS       256       /* 同时存在的带状态卡对象上限（一局最多 256 张卡，0x411469）*/
#define CE_STATE_BYTES       512       /* 每张卡私有状态上限（2026-09-08 256 → 512：股市 id 80 要 430+ 字节；静态表 256 × 512 = 128 KB）*/

/* 一张卡的行为：id + 它的虚表（21 槽拷贝）+ 一句给日志看的槽清单 */
typedef struct {
    uint32_t    id;
    const void *vtable;
    const char *slots;                 /* 例如 "ctor,on_tick_2" */
    const void *hooks;                 /* ce_hooks_t*（sdk.h）；事件断点按虚表找到它 */
    uint8_t     is_active;             /* active_recharge != 0；供不依赖 ce_hooks_t 的契约检查 */
} ce_behavior_t;

enum {
    CE_SDK_REG_NONE = 0,
    CE_SDK_REG_ID_RANGE,
    CE_SDK_REG_DUPLICATE,
    CE_SDK_REG_CAPACITY,
};

/* 登记；返回 0 = id 越界、重复或容量已满。第一个错误会一直保留到测试 reset。 */
int ce_sdk_register(const ce_behavior_t *b);
int ce_sdk_registration_error(uint32_t *id, int *reason);
unsigned ce_sdk_behavior_count(void);
const ce_behavior_t *ce_sdk_behavior_at(unsigned i);
/* 按 id 找；NULL = 这张卡没有行为（保持基类虚表） */
const ce_behavior_t *ce_sdk_find(uint32_t id);
void ce_sdk_reset_for_test(void);      /* 只给单测用 */

/* 对账：json_ids[0..n) 是 cards.js 登记的 id。
 *   返回 0 且 *bad_id = 第一个「C 有行为、JSON 没登记」的 id（FAIL）；
 *   返回 1；*unbound = 「JSON 有、C 无行为」的张数（允许，开发期正常）。 */
int ce_sdk_bind_check(const uint32_t *json_ids, unsigned n, uint32_t *bad_id, unsigned *unbound);

enum {
    CE_SDK_CONTRACT_NONE = 0,
    CE_SDK_CONTRACT_ACTIVE_BEHAVIOR_REQUIRED,
    CE_SDK_CONTRACT_ACTIVE_CATEGORY_REQUIRED,
};
int ce_sdk_contract_check(const uint32_t *json_ids, const uint32_t *categories, unsigned n,
                          uint32_t *bad_id, int *reason);

#define CE_STATE_RESERVED    16        /* 块头留给 SDK（主动卡状态机 ce_active_t）；卡的私有状态从这之后起 */

/* 私有状态槽：键 = 卡对象指针。alloc 幂等（同键返回同块，首次清零）；size > CE_STATE_BYTES 或槽满返回 NULL。 */
void *ce_state_alloc(const void *key, unsigned size);
void *ce_state_get(const void *key);   /* 没分配过返回 NULL */
void  ce_state_free(const void *key);  /* 没分配过是空操作 */
unsigned ce_state_in_use(void);
/* 卡侧私有状态：同一把键、同一块，返回块头 CE_STATE_RESERVED 字节之后的地址；size 超出剩余空间返回 NULL。 */
void *ce_state_user(const void *key, unsigned size);

/* ---- 自机弹伤害倍率层（AUDIT §AA）----------------------------------------
 * 「给自机弹加伤害」这件事出现了第三次（K♠ 61、古锭刀 76、衔尾蛇 78），提炼成一层。
 * 每张参与的卡报一个千分比（CE_DMG_UNIT = ×1），这里连乘出**聚合倍率**，并指定其中一张卡
 * （id 最小的那张）当「落笔人」——只有它的 `+0x28` 桩真的写 `bullet+0x9c`，所以一颗弹只被乘一次、
 * 只取整一次，与卡在链表里的顺序无关。上限也由落笔人在 `+0x2c` 里按同一个倍率抬。
 * 纯逻辑，无引擎依赖，主机单测。*/
#define CE_DMG_UNIT 1000

void ce_dmg_reset(void);
/* 登记 / 更新一张卡的贡献。permille = 0 表示这张卡不在场（绑定时登记声明值，`+0x50` 删对象时清 0）。*/
void ce_dmg_set(uint32_t id, int permille);
int  ce_dmg_aggregate(void);      /* 千分比连乘；没有贡献者时 = CE_DMG_UNIT */
int  ce_dmg_applier(void);        /* 落笔那张卡的 id；没有贡献者时 -1 */
int  ce_dmg_scale(int damage);    /* damage × 聚合 / UNIT，单次取整；聚合 = UNIT 时原样返回 */

/* 集卡判定（纯函数）：self 在 set 里，且 set 里其余每张 owned[id] != 0 → 1。owned 是 int[255]。*/
int ce_royal_flush_ready(const int32_t *owned, uint32_t self_id, const uint32_t *set, unsigned n);
/* 神之宣告：消耗一半残机（向上取整）；残机 ≤ 0 返回 0 = 不能发动 */
int ce_judgment_cost(int lives);
/* 黄金小丑：利息 = money 的 percent％ 向下取整；money ≤ 0 返回 0 */
int ce_interest(int money, int percent);
/* 倒吊人：n 个候选里按游戏 RNG 的一个 dword 选下标；n == 0 返回 -1 */
int ce_pick_index(uint32_t rnd, unsigned n);

/* ---- 无敌帧声明层（EVENTS-CONTRACT.md §4；107 结界 0.67s / 117 霊撃 40f 都写 player+0x47774）----
 * 直接各写各的会互相**截短**（后写的短无敌盖掉前面给的长无敌）⇒ 改成「各卡声明、取最大值」。
 * 纯逻辑、无引擎依赖（写计时器在 sdk.h 的 ce_invuln_apply 里做）。*/
void ce_invuln_reset(void);
/* 登记 / 更新一张卡的本帧请求（frames <= 0 = 这张卡本帧不参与）。绑定时清 0、删对象时清 0。*/
void ce_invuln_request(uint32_t id, int frames);
int  ce_invuln_frames(void);      /* 本帧应给的无敌帧数（取最大）；无贡献者 = 0 */
int  ce_invuln_applier(void);     /* 最长的那张卡的 id（并列取 id 最小）；无贡献者 = -1 */

/* ---- 改值类事件的计分层（EVENTS-CONTRACT.md §3：声明 + 聚合 + 单一落笔）----
 * 起因：110 / 111 / 113 都要改 on_item_score 传出来的同一个 esi，而原来「逐个调、谁后写谁赢」。
 * 现在各卡只**声明**自己的贡献（粘性，与 ce_dmg_set 同款），SDK 求和，并由 **id 最小的那张卡落笔**
 * （= 它按 PV 公式算出最终值写 regs->esi）。卡片之间互不读对方状态。*/
void ce_score_reset(void);
void ce_score_declare(uint32_t id, int32_t value);   /* 粘性：设为 0 = 这张卡不参与 */
int32_t ce_score_aggregate(void);                    /* Σ 各卡贡献（信仰总量）*/
int  ce_score_applier(void);                         /* 落笔者 id（最小）；无贡献者 = -1 */

/* ★ 第二通道：**乘数附加项**（EVENTS-CONTRACT §3.1 的两卡同装公式要用）。
 * 起因（2026-09-12 审计）：110 与 111 同装时公式是 `(基础均值 + Δ110 + Δ111) × (擦弹倍率 + 信号强度)`，
 * 而**落笔者永远是 110**（id 更小）⇒ 110 拿不到 111 的信号强度；契约又禁止**读别的卡的状态**。
 * ⇒ 让 111 只**声明**「我这份信号强度（千分点）」，落笔者读**聚合值**即可，谁都不读卡状态。
 * 粘性，语义与 `ce_score_declare` 同款；`ce_layer_drop_card()` 会一并清。（擦弹倍率不在这里 —— 它来自
 * 全局擦弹计数，由落笔者自己算，两边都算会重复计入。）*/
void ce_score_mul_declare(uint32_t id, int permille); /* 0 = 这张卡不参与 */
int  ce_score_mul_aggregate(void);                    /* Σ 各卡的乘数附加项（千分点）*/

/* ---- 「本帧已消过弹」闩锁（EVENTS-CONTRACT.md §5.2 细则 2）----
 * 107 的结界灵击与 117 的霊撃都会全屏消弹；同一帧两者都触发时，弹只能被消一次（否则第二家扫到空池、
 * 或重复记账）。每帧初 reset，各卡 claim：第一个拿到的负责消弹，其余让位。*/
void ce_cancel_latch_reset(void);
int  ce_cancel_latch_claim(uint32_t id);   /* 本帧第一个认领的返回 1（并记住 id）；已被别人认领返回 0 */
int  ce_cancel_latch_owner(void);          /* 本帧认领者 id；没人 = -1 */

/* ---- 「这次死亡由谁救」闩锁（EVENTS-CONTRACT.md §5.2；用户 2026-09-12 定案）----
 * 救援层顺序被钉死为 **107 → 117 → 决死 B → 113**。113 的语义是**延时**（取消死亡 → 进灵界 →
 * 出灵界再死一次），所以它**必须知道前面三家有没有人已经救过**，否则会出现
 * 「107/117 花钱救了你、113 的窗口结束时又把你杀掉」。
 * 每帧初 reset（与 ce_cancel_latch_reset 同一处：`ce_item_frame` 断点），各救援者 claim 报到：
 *   107（`cards/senro.c`）/ 117（`cards/animal.c`）**无条件** claim，且**必须丢弃返回值** ——
 *   用户 2026-09-12 重申「**107 和 117 同时触发的时候，得要 107 和 117 同时破除**」
 *   ⇒ 这两家谁也不能靠「先 claim 到账」把另一家挡掉（把返回值当门 = 让先到的独占救援 = 违规）；
 *   113（`cards/trance.c`）**查 owner**：没人认领才轮到它动手（它是最后一家）。
 * ⚠️ 为什么「每帧」够用：一次死亡横跨多帧（`Player__die` 置 state 4 之后还有决死窗口 / 死亡动画），
 *   同帧不可能发生两次死亡 ⇒ 每帧闩锁 == 每次死亡闩锁。*/
void ce_rescue_note_reset(void);
int  ce_rescue_note_claim(uint32_t id);    /* 记下「这次死亡由 id 救了」；第一个返回 1，已被别人记返回 0 */
int  ce_rescue_note_owner(void);           /* 这次死亡的救援者 id；没人 = -1 */

/* ---- 低速子机声明层（EVENTS-CONTRACT.md §1「选举覆盖」；108 spec §3.3.1）----
 * 108（妖机体）与 111（SA 子机层）都要定义「低速时子机长什么样」，而最终落在同一批 zPlayerOption
 * （原生聚焦偏移 +0x6c/+0x70、非聚焦 +0x64/+0x68、贴图脚本、数量）⇒ 各自写 = 谁后写谁赢。
 * 规则：**各卡只声明，SDK 选举 id 最小者落笔**（与伤害层/计分层同一套选举），`only_focused` 由卡自己填。
 * 纯逻辑、无引擎依赖（真正写引擎字段由被选举的卡在自己的 tick 里做）。*/
typedef struct {
    int   count;        /* 想要几个低速子机（0 = 这张卡本帧不声明）*/
    int   anm_script;   /* 子机贴图脚本号（= ce_option_alloc 第 4 参）*/
    float fx, fy;       /* 聚焦（低速）偏移，对应原生 option +0x6c/+0x70 */
    float ux, uy;       /* 非聚焦偏移，对应 +0x64/+0x68 */
} ce_option_pose_t;

void ce_optpose_reset(void);
/* 粘性声明：count <= 0 = 这张卡退出声明（绑定时清 0、删对象时清 0）。*/
void ce_optpose_declare(uint32_t id, int count, int anm_script, float fx, float fy, float ux, float uy);
int  ce_optpose_winner(void);                       /* 落笔者 id（最小）；无人声明 = -1 */
const ce_option_pose_t *ce_optpose_pose(void);      /* 落笔者的声明；无人 = NULL */

/* ---- HUD 槽位分配器（HUD-LAYOUT.md §3/§4；110/111/113 + 58–80 的 5 张卡都要）----
 * 目的：多张卡都要占「左下角」时不再各自硬编码坐标而重叠。规则（§4.4）：
 *   · 认领集合**按 card_id 排序** ⇒ 同样的卡组永远得到同样的排布（replay / 读档安全）；
 *   · 认领**幂等**（on_load 每关都会再调，重复认领返回同一个槽）；
 *   · 局中删卡**不当场重排**（§4.4：避免整摞 UI 跳动）⇒ release 只标记「保留」，
 *     真正的清空发生在 `ce_hud_slots_reset()`（关卡开场调）。
 * 纯逻辑、无引擎依赖；坐标单位 = **表面 px**（调用方要 640 空间的点自己乘系数，见 sdk.h 的包装）。*/
typedef struct {
    float w, h;      /* 声明块尺寸（表面 px）*/
    int   side;      /* 0 = 左下（默认）／1 = 右下 */
} ce_hud_block_t;

#define CE_HUD_SIDE_LEFT  0
#define CE_HUD_SIDE_RIGHT 1
#define CE_HUD_SIDE_PINNED 2   /* ★ **自带锚点**：用户按截图量过的固定位置（117）。登记进表、
                                *   但**不参与货架排布**（`x/y` 由认领时给的锚点决定，不随别人重排）。
                                *   HUD-LAYOUT §5 早列为待办；2026-09-12 用户裁决「117 现在的位置
                                *   （左下角、整排左移 193 px 后）是正确的」⇒ 用这一支保位置。*/

typedef struct {
    float x, y;          /* 块**左下角**（表面 px）：y 是块的**下边缘**（TH 坐标 y 向下增长）*/
    float x640, y640;    /* 同一个点，640×480 空间 —— 直接喂 ce_hud_vm_text_* / ce_hud_number_* */
    float w, h;          /* 回显声明值，卡自己按它算内部布局 */
    int   side;
} ce_hud_slot_t;

/* 底部货架的常量（表面 px，HUD-LAYOUT.md §2）。117 动物灵 / 116 季节槽实测推出的同一条线：
 * 左锚点 257、底边 924；行内间隙 8；左列软上限 1000（§6.3 🟡 待实跑标定），
 * 硬上限 1040 来自 §4.2（HUD 文字永远画在图元之前 ⇒ 不许压到卡组图标行）。*/
#define CE_HUD_ANCHOR_X        257.0f
#define CE_HUD_ANCHOR_X_RIGHT 1040.0f   /* ❓ 右侧镜像锚点（§6.2 无先例卡验证过）*/
#define CE_HUD_ANCHOR_Y        924.0f   /* 行底边 */
#define CE_HUD_GAP               8.0f   /* 行内/行间间隙 */
#define CE_HUD_X_MAX          1000.0f   /* 左列软上限（排不下就上抬一行）*/
#define CE_HUD_X_HARD_MAX     1040.0f   /* 硬约束：块右边缘不许越过（§4.2）*/
#define CE_HUD_X_MIN           257.0f   /* 右列向左排的软下限 */

/* 旧名（兼容）：基底 = 锚点 */
#define CE_HUD_BASE_LEFT_X   CE_HUD_ANCHOR_X
#define CE_HUD_BASE_RIGHT_X  CE_HUD_ANCHOR_X_RIGHT
#define CE_HUD_BASE_BOTTOM_Y CE_HUD_ANCHOR_Y
#define CE_HUD_STACK_PAD     CE_HUD_GAP

/* 表面 px → 640×480 字面量空间（引擎内部再 × `DAT_0056aca0`；TH18 表面 1280×960 ⇒ ÷2）。
 * 🟡 待实跑确认一次；纯数学常数，故放这里（sdk_core 层不依赖引擎头，见 sdk.h 的同名别名）。*/
#define CE_HUD_640_SCALE      0.5f

/* 排布算法（HUD-LAYOUT.md §2，底部货架）：认领集合按 card_id **升序**，同一侧各自算 ——
 * 左列从左锚点沿底边向右排（`x + w > CE_HUD_X_MAX` ⇒ 整体上抬一行，行高 = 该行最高块）；
 * 右列从右锚点向左排。⇒ 完全确定性：同样的卡组永远同样的排布（replay / 读档安全）。*/

void ce_hud_slots_reset(void);                                  /* 清空全部认领与保留（测试 / 手动用）*/
/* ★ 关卡纪元：**每关开场重排**（HUD-LAYOUT.md §4.4 的「下关开场重算」）。
 * 由 `sdk.h` 的 `ce_hud_slot_claim_card()` 在**每次认领前**把 `CE_CURRENT_STAGE()` 推进来：
 * 关卡号变了 ⇒ 先 `ce_hud_slots_reset()` 再认领 ⇒ 上一关被删卡留下的占位自然收敛，
 * 而**同一关内**（局中删卡）一笔都不重排（§4.4：避免整摞 UI 突然跳动）。
 * ⚠ 不能拿 `on_run_reset`（`+0x4c`）当关卡信号：它由 `recount 0x4080e0` 调，**局中删卡后也会跑**。*/
void ce_hud_stage_begin(int stage);
int  ce_hud_stage_current(void);                                /* 记住的关卡号（-1 = 还没认领过）*/
/* 认领（幂等）。成功返回 1 并填 *out；卡 id 非法 / blk 非法返回 0。
 * `blk->side == CE_HUD_SIDE_PINNED` 时本函数返回 0（要用下面那个带锚点的入口）。*/
int  ce_hud_slot_claim(uint32_t card_id, const ce_hud_block_t *blk, ce_hud_slot_t *out);
/* ★ **自带锚点认领**（`blk->side == CE_HUD_SIDE_PINNED`）：按用户按截图量过的固定位置登记。
 *   `anchor_x/anchor_y` = 块的**左下角**（表面 px，y 是块的下边缘）。登记后 `ce_hud_slot_get()` 取得到、
 *   关卡纪元照样会清（下一关要重新认领），但**货架不会去动它**（别的块也不会为它让位 ⇒ 它自己负责不压别人）。
 *   ⚠ `blk->side` 必须 == `CE_HUD_SIDE_PINNED`，否则返回 0（反之 `ce_hud_slot_claim()` 也拒 pinned）。*/
int  ce_hud_slot_claim_pinned(uint32_t card_id, const ce_hud_block_t *blk,
                              float anchor_x, float anchor_y, ce_hud_slot_t *out);
/* ⚠ 认领时填给 `out` 的坐标是**那一刻**的；同一开场批次里后来的认领会按 id 顺序重排
 * ⇒ 卡片应在开场批次结束后（第一帧 tick）用本函数取最新值，或每帧取一次（开销可忽略）。*/
int  ce_hud_slot_get(uint32_t card_id, ce_hud_slot_t *out);
void ce_hud_slot_release(uint32_t card_id);                     /* 释放：标记保留（不当场重排）*/
int  ce_hud_slot_claimed(uint32_t card_id);                     /* 本关是否已认领（含保留）*/

/* 「玩家贴近左下角 → 整摞淡出」+ 迟滞（TH16 `th16:0x427f41/0x427f6c` 同款栅格）。
 * 栅格：x < −64 && y > 400 淡出；y < 384 || x > −64 淡回（迟滞状态内部保持）。
 * ★ **每帧只推进一次**：由 SDK 在 `BP_ce_item_frame` 里用自机坐标调 `ce_hud_near_corner_update()`；
 *   卡片**只读** `ce_hud_near_corner()`，别自己在 tick 里各调一遍（多张卡同帧各调 = 状态被反复推进，
 *   甚至一帧内先淡出又淡回）。`ce_hud_player_near_corner(x,y)` 是「更新 + 读取」的兼容入口。*/
void ce_hud_near_corner_update(float player_x, float player_y);   /* 推进迟滞（每帧一次）*/
int  ce_hud_near_corner(void);                                    /* 读当前状态：1 = 该淡出 */
int  ce_hud_player_near_corner(float player_x, float player_y);    /* 兼容：更新后返回状态 */

/* ---- 掉落层（EVENTS-CONTRACT.md §2，用户 2026-09-12 拍板：「加倍的意图是 ×2」）----
 * 引擎的 `zEnemyDrop`（`enemy+0x623c`）：`+0x00` main_drop（一颗，**在事件触发前就撒了**）、
 * `+0x04..+0x53` = ex_drop[20] 计数（type 1..0x13；引擎只撒前 19 格）、
 * `+0x54..+0xa3` = timed_ex_drop[20]（按出场计时折算的加成表，`n = ex[i] + timed[i]×(T−timer)/T`，
 * 见 `engine.h` 的 `CE_ENEMY_DROP_TIMED_*`）—— 两张表**都对最终颗数线性** ⇒ 乘数两张都要乘。
 * 规则：**加法来自各卡（照现状直接改 counts），乘法由层在最后一次性作用在「最终总数」上**：
 *     final[t] = ( base[t] + Σ add[t] ) × Π mult           （base[main] = ex[main] + 1，因为已撒一颗）
 *     写回 ex[t] = final[t] − (1 if t == main else 0)；timed[t] 只乘、不做「已撒一颗」补偿
 * ⇒ 「加倍」不再依赖卡链顺序，且**别的卡加的那颗也会被翻倍**（`(1+1)×2 = 4`，不是 `1×2+1 = 3`）。*/
#define CE_DROP_MULT_UNIT   1000
#define CE_DROP_TYPES       20      /* counts 格数（type 1..0x13 + 1 个空位）*/
#define CE_DROP_MAX_MAIN    19      /* 主掉落补偿只认到 type 19（引擎的循环只撒 1..19）*/
#define CE_DROP_TIMED_TYPES 20      /* +0x54 timed_ex_drop 的格数（与 ex_drop 同）*/

void ce_drop_reset(void);
/* 粘性乘数（千分比；1000 = ×1、0 = 不参与）。绑定时登记、opdelete 时清。*/
void ce_drop_mult_set(uint32_t id, int permille);
int  ce_drop_mult_aggregate(void);                 /* Π 各卡乘数；无贡献者 = CE_DROP_MULT_UNIT */
/* 引擎侧调：传 **zEnemyDrop 基址**（不是 counts！）—— z[0] = main_drop、z[1..20] = ex_drop、
 * z[21..40] = timed_ex_drop；就地写回「最终 − 已撒」（timed 表不做补偿）。*/
void ce_drop_apply(int32_t *z_enemy_drop);
