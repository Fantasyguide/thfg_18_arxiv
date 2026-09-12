/* Stable card-author surface. TH18 addresses and ABI wrappers stay in ../sdk.h. */
#pragma once
#include <stdint.h>

typedef struct ce_card ce_card_t;

/* ---- 敌人发射口（共享事件，108 用）------------------------------------------
 * 断点 `0x433ffa`（见 `sites.py` 的 `ce_enemy_shot_slot`）：敌人 ECL opcode `0x259`（etOn）里
 * 「发弹三步」的汇聚点。**这一帧这一槽要不要真发**由卡决定（返回非 0 = 掐掉）。
 * 一手：`edi` = `&enemy->data`（`enemy+0x122c`）、槽号 i 在 `[ebp-0x56c]`、`esi = &data + i*0x488`；
 * 掐掉 = 跳过 `et_protect_range` 的 set / `call 0x427810`（`__fire`）/ 复位三步（`retaddr = 0x43402d`）。
 * ⚠ `enemy` **只在本次回调内有效**（同帧 sweep 可能删它，DEATH §2.4）⇒ 禁止缓存、禁止在回调里析构。*/
typedef struct {
    uint32_t enemy_id;    /* enemy+0x6830（单调递增）*/
    int      slot;        /* 发射口槽号 0..15（`[ebp-0x56c]`）*/
    void    *enemy;       /* zEnemy*（= data − 0x122c）；仅本次回调有效 */
} ce_enemy_shot_t;
#define CE_ACTIVATE_REFUSED (-1)
void *ce_state_user(const void *card, unsigned size);
#define ce_state(c, T) ((T *)ce_state_user((c), sizeof(T)))

/* ---- 敌人死亡 / 击破（共享事件，109 / 110 / 112 / 113）------------------------
 * 断点 0x430240（`FUN_00430240`，SDK 内部叫 Enemy__die）入口，**在引擎撒主掉落之前**、
 * 链表仍挂着。用 `regs->retaddr` 分死因；四张卡要的「击破」= `CE_EDEATH_KILLED`。
 * 详见 `docs/superpowers/specs/2026-09-12-shared-enemy-death-event-design.md`。*/
#define CE_EDEATH_KILLED     0u   /* 被打死的主路（retaddr 0x42f3b7，life ≤ 0）*/
#define CE_EDEATH_BOMB_KILL  1u   /* 炸弹秒杀路（retaddr 0x42ee90）*/
#define CE_EDEATH_ECL_DIE    2u   /* ECL die opcode */
#define CE_EDEATH_OTHER      3u   /* 脚本 / 计时 / 标志驱动的移除 */
#define CE_EDEATH_KILL_ALL   4u   /* killAll 一族（打出来的清场）*/

#define CE_EDEATH_F_REAL     0x1u /* bit0：算作真敌人（(flags_lo & 0x31) == 0）*/
#define CE_EDEATH_F_HAS_DROP 0x2u /* bit1：掉落表非空（drop_main != 0）*/

typedef struct {
    uint32_t id;          /* enemy+0x6830（单调递增；记账 / 去重用，别缓存指针）*/
    uint32_t kind;        /* CE_EDEATH_* */
    uint32_t flags;       /* CE_EDEATH_F_* */
    float    x, y;        /* enemy+0x1270 / +0x1274（世界坐标）*/
    int32_t  drop_main;   /* enemy+0x623c 当前值（**尚未消费**；改了会影响随后的撒落）*/
    void    *enemy;       /* ★ 只在本次回调内有效（同帧 sweep 会删）；禁止缓存 */
} ce_enemy_death_t;

typedef struct {
    int  (*ctor)(ce_card_t *);
    int  (*dtor)(ce_card_t *);
    int  (*on_death_after_deathbomb)(ce_card_t *, uint32_t);
    int  (*on_death_before_deathbomb)(ce_card_t *);
    int  (*on_death_frame2)(ce_card_t *);
    int  (*on_power_level_change)(ce_card_t *);
    int  (*on_tick_shooters)(ce_card_t *, uint32_t, uint32_t);
    int  (*on_load)(ce_card_t *);
    int  (*on_tick)(ce_card_t *);
    int  (*on_bullet_created)(ce_card_t *, void *);
    int  (*on_tick_2)(ce_card_t *);
    int  (*on_enemy_drop)(ce_card_t *, uint32_t, uint32_t);
    void (*on_stage_start)(ce_card_t *);
    int  (*on_hud_anm)(ce_card_t *, uint32_t);
    int  (*on_draw)(ce_card_t *);
    void (*on_run_reset)(ce_card_t *);
    void (*on_item_score)(ce_card_t *, int32_t *);
    void (*on_item_money)(ce_card_t *, int32_t *);
    void (*on_enemy_drop_pre)(ce_card_t *, int32_t *);
    void (*on_bomb_spent)(ce_card_t *);
    /* BP_ce_bomb_gate 0x45c048：原版 `Player__on_tick__body` 的普通炸弹分支
     *   `test BYTE PTR ds:0x4ca434,0x2`。返回非 0 = 本卡要求拦下这一发炸弹，
     *   SDK 会让执行跳过 `call do_bomb`（0x45c051）继续到 0x45c056；不咆哮时必须返回 0。*/
    int  (*on_bomb_gate)(ce_card_t *);
    void (*on_shop_open)(ce_card_t *);
    /* 删对象（局末 recount / 倒吊人 ce_destroy_card）时、SDK 释放私有状态之前调：卡在这里删自己起的 VM 等外部资源。
     * on_run_reset 只覆盖局末那条路，局中被摧毁只走这里。2026-09-08 股市（80）第一个用户。 */
    void (*on_delete)(ce_card_t *);
    uint32_t active_recharge;
    int  (*on_activate)(ce_card_t *);
    int  (*on_active_tick)(ce_card_t *, uint32_t);
    /* collect-2x 四断点（名/址/窗字节一手见 ../sdk.c 头表；AUDIT §Z）*/
    void (*on_item_frame)(ce_card_t *);                    /* BP_ce_item_frame   0x446ee3：每个正常游玩帧、道具池 body 前一次（借门卫位次，不读 CMP）*/
    void (*on_item_collect)(ce_card_t *, uint32_t, const void *); /* BP_ce_item_collect 0x44625f：type 分派前；type = *(uint32_t*)(item+0xc78)，副作用未发生 */
    void (*on_item_commit)(ce_card_t *);                   /* BP_ce_item_commit  0x44658a：本道具分值已入 SCORE、公共收尾前 */
    void (*on_shot_damage)(ce_card_t *, int32_t, const float *);  /* BP_ce_shot_damage 0x45f2a2：本帧本敌可计分命中；(dmg 钳后 ≥1, pos = 敌世界坐标 x,y) */
    /* 自机弹伤害倍率（千分比，1000 = ×1）。非 0 = 这张卡参与伤害层：SDK 在绑定时登记这个初值，
     * 之后卡可以随时 ce_set_bullet_damage_permille() 改。所有参与者的值连乘成聚合倍率，
     * 由其中一张（id 最小的）在 +0x28 里对 bullet+0x9c 就地乘一次、并在 +0x2c 里抬每帧伤害上限。
     * ★ 别再自己写 bullet+0x9c：那样会各自取整、还得各自记得抬上限。边界：只管自机弹，
     * 炸弹与 ce_damage_rect 的伤害源不在内。AUDIT §AA。*/
    int  bullet_damage_permille;
    /* BP_ce_shot_set_index 0x45ebfb：Player__tick_shooting_state 查主炮 shooterset 前。
     * 返回 >= 0 = 覆盖本次 fire-time set index（TH17 咆哮的 `power_level + base`）；-1 = 不覆盖。
     * 只在卡自己承认咆哮 active 时返回非负值；SDK 同时改写 edx 与 [ebp-8]（弹里存 set 的那格）。*/
    int  (*on_shot_set_index)(ce_card_t *, int power_level);
    /* 敌人死亡 / 击破（断点 0x430240，共享给 109/110/112/113）。
     * ⚠ 回调里**禁止析构任何敌人**（UAF：断点必在链表遍历栈里）；新建敌人安全；别缓存 `ev->enemy`。
     * ⚠ 回调里改 `ev->enemy` 的掉落表（`+0x623c`）是**允许**的，而且此刻引擎还没撒落 ⇒ 能改种类。*/
    void (*on_enemy_death)(ce_card_t *, const ce_enemy_death_t *);
    /* 本帧是否跳过「原生主炮行循环」（= 本帧不发原生弹）。108（低速妖机体）与 109（蓄力拦 Z）用。
     * 与 on_shot_set_index 同一个断点（0x45ebfb）：**先问 set index；有卡覆盖了（= 117 咆哮）本帧就不抑制**。
     * ⚠ 只压「原生行循环」，`0x45eca0` 起的**卡广播照跑**（77 / 116 的卡子机不受影响）。*/
    int  (*on_native_shot_suppress)(ce_card_t *);
    /* ★ BP_ce_shot_damage 0x45f2a2 的**同一个断点、同一份实参**，但**不按聚焦过滤**：
     *   on_shot_damage 的策略门是「非聚焦 + 非 bomb」（114 辉针城「非聚焦命中掉 P」的原义），
     *   而 107 森罗结界需要**高速 / 低速两条通道都要**（`REVIEW` §9 / `IDEAS` 的「高速约 3 倍」）。
     *   ⇒ 这里把「非 bomb 帧的全部自机弹命中」原样交出来，`focused` 由 SDK 传（= 本帧是否聚焦）。
     * 语义边界（与 on_shot_damage 一致）：`param_6 != 0` 的路径到不了这个断点；**bomb 生效帧不发**；
     *   同一帧同一敌只发一次；`damage` 已钳 ≥1；`pos` = 该敌世界坐标 {x,y}。
     * ⚠ 与 `on_shot_damage` **可以同时声明**（会各收一次）—— 若要「高速走一条、低速走另一条」，
     *   请在回调里按 `focused` 自己分流，别让两个钩子重复记账。*/
    void (*on_shot_damage_any)(ce_card_t *, int32_t damage, const float *pos, int focused);
    /* 敌人这一槽**本帧要不要真发弹**（断点 `0x433ffa`，ECL etOn）。返回非 0 = 掐掉（不发）。
     * 108 妖机体用它做「使魔被打死 ⇒ 关掉对应的发射口」；不声明 = 完全不受影响（默认放行）。*/
    int  (*on_enemy_shot_slot)(ce_card_t *, const ce_enemy_shot_t *);
    /* ★ 低速子机声明层的**落笔回调**（spec §3.3.1；层 = sdk_core 的 `ce_optpose_*`，分派在 sdk.c）。
     * 每帧在 `BP_ce_item_frame` 里被调**恰好一次**，且**只有被选举的那张卡**（id 最小者胜）：
     *   ① 它是本帧落笔者 ⇒ 申请 / 摆位 / 换图都归它（单一写者 ⇒ 与别的卡没有竞态）；
     *   ② 或者它是**上一帧的落笔者、这一帧没赢**（自己退出声明 / 被 id 更小的卡盖过 / 池被重建）
     *      ⇒ SDK 补叫它一次去收尾（**退位通知**）—— 此时 `ce_option_pose_winner()` 不是自己。
     * 所以回调里一律先判：
     *     if (ce_option_pose_winner() == (int)ce_card_id(c)) { 声明里的 count 格：ce_option_get + 写偏移 }
     *     else                                              { 把手里所有子机 ce_option_drop 收干净 }
     * ⚠ 不声明这个槽 = 永远进不了选举（`ce_option_pose_declare()` 每帧都要调；`count <= 0` = 本帧退出）。
     * ⚠ 回调里可以申请子机（= 建 anm VM），它与卡自己的 tick 同一安全级（**不在死亡栈上**）。*/
    int  (*on_option_pose_apply)(ce_card_t *);
} ce_hooks_t;

/* 改这张卡当前的自机弹伤害倍率（千分比）。只在值真的变了的时候调（古锭刀在终符沿、衔尾蛇在叠层时）。*/
void ce_set_bullet_damage_permille(ce_card_t *c, int permille);

int ce_give_card(uint32_t id, uint32_t mode, uint32_t notify);
int ce_destroy_card(ce_card_t *victim);
ce_card_t *ce_pick_random_card(ce_card_t *exclude);
void ce_log(const char *fmt, ...);
