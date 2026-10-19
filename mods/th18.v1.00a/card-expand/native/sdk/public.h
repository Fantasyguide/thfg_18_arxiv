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
    uint32_t kind;        /* CE_EDEATH_*；按**栈扫描匹配到的调用点返回地址**解码（见 sdk.c）*/
    uint32_t flags;       /* CE_EDEATH_F_* */
    uint32_t retaddr;     /* 匹配到的调用点返回地址（0 = 前 12 格都没命中）*/
    uint32_t esp;         /* BP 入口的原始 regs->esp（可证伪日志用）*/
    int32_t  ret_slot;    /* retaddr 命中第几格：0..11；未命中 = -1 */
    float    x, y;        /* enemy+0x1270 / +0x1274（世界坐标）*/
    int32_t  drop_main;   /* enemy+0x623c 当前值（**尚未消费**；改了会影响随后的撒落）*/
    void    *enemy;       /* ★ 只在本次回调内有效（同帧 sweep 会删）；禁止缓存 */
    /* ★★ 2026-09-22 夜（108 的 P-D 需要；追加在**尾部** ⇒ 既有消费者不动、ABI 安全）：
     * 「这一下是不是**自机弹**打死的」。一手：TH08 的「人間逢魔 + 自机弹击杀再掉 1 颗刻符」那一条写在
     * `Player::compute_damage_to_enemy` 体内（`th08:0x451865` / `:0x45187f`）⇒ 真信号 = 「死亡发生在
     * 那条伤害路径里」，TH18 没有等价暴露面。本字段取**我们已有的一手分解**里最近的一档：
     * `kind == CE_EDEATH_KILLED`（`retaddr 0x42f3b7`，life ≤ 0 的**主路** = 被伤害打死）⇒ 1，
     * 其余（炸弹秒杀 / ECL die / killAll / OTHER）⇒ 0。❓ 登记在 `TUNING.md`（108 的 §2.0zk #37）。*/
    int32_t  by_player_bullet;   /* 1 = 走了「被打死」主路（见上；❓ 近似，不是 TH08 原义）*/
} ce_enemy_death_t;

/* ---- 敌人**吃到伤害**（共享事件，113 的「Boss 随血量掉灵」用）----------------------
 * 断点 `ce_enemy_damage` `0x42f2cb`（`EnemyData::step_game_logic` `0x42ed40` 内）：
 *   被偷的指令 = `add [ebx+0x5008],esi`（引擎自己的**累计受伤**记账）—— handler 必须
 *   返回 `BP_EXEC_ORIGINAL`，绝不能吃掉它。站点在**两道免伤闸之后**（`0x42f2b9`
 *   `test byte [ebx+0x5130],0x10` / `0x42f2c2 cmp [ebx+0x50f4],0`）⇒ 语义 = 「HP 即将减少」。
 *   `0x42ed40` 每敌每帧恰好一次 ⇒ 这不是"每命中一次"，卡侧要按帧累计。
 * ⚠ 与 `on_enemy_death` 同一套纪律：`ev->enemy` **只在本次回调内有效**（同帧可能被删）。*/
typedef struct {
    uint32_t id;        /* enemy+0x6830（单调递增，**用它去重/查表**，别用指针）*/
    int32_t  damage;    /* 本帧最终伤害（已过自机倍率 / ECL 扩展 / 炸弹倍率）*/
    int32_t  hp_cur;    /* enemy+0x6220 */
    int32_t  hp_max;    /* enemy+0x6230 */
    uint32_t flags;     /* enemy+0x635c 原值（判 CE_EDAMAGE_F_BOSS）*/
    float    x, y;      /* enemy+0x1270 / +0x1274（世界坐标）*/
    void    *enemy;     /* zEnemy*；仅本次回调有效 */
} ce_enemy_damage_t;
#define CE_EDAMAGE_F_BOSS  0x1u   /* 已置位 = ECL 调过 setBoss（boss / 道中 boss）*/

/* ---- 「一发自机弹 × 一个敌机」的命中（断点 `ce_bullet_hit` `0x45f466`；卡 108 的「人逢魔」用）------
 * ★★★ 2026-09-27（owner：「3 可以贴一手，关键在站点写法」）。一手 TH08 的那条规定写在**玩家侧**的
 * 伤害函数里（`Player::compute_damage_to_enemy` `th08:0x451860`–`:0x451892`）：**極端人間** 期间，
 * **每一发自机弹碰到敌机**就在**那颗弹的位置**掉 1 颗刻符（type 7 ⇒ 被强制成 state 3）。
 * TH18 的伤害在**敌人侧**算（`enm_compute_damage_sources` `0x45f0f0` 遍历 1024 槽伤害源）
 * ⇒ 本站点钉在「**某个伤害源第一次命中某个敌人**」那一瞬（= 一发弹命中一次；机制全文 = `sdk.c` 的
 * `BP_ce_bullet_hit`）。`x/y` 就是**那颗弹的当前位置**（弹的 tick 每帧把 `bullet+0x48` 同步进源槽）。
 * ⚠ 大部分伤害源不是弹（原生卡 25/54 与各卡的 `ce_damage_rect` 都会往同一个池里放源）
 *   ⇒ **先看 `is_player_bullet`** 再决定要不要当成「自机弹命中」。*/
typedef struct {
    int32_t  index;            /* 伤害源槽下标（1-based；= `ce_damage_rect` 的返回值口径）*/
    int32_t  is_player_bullet; /* 1 = 有**活动弹**把自己的伤害源下标（`bullet+0xb0`）指向这个槽 */
    int32_t  bullet_i;         /* 命中它的弹在弹池里的槽号（诊断；-1 = 不是弹）*/
    int32_t  damage;           /* 这个源写着的「每次结算伤害」（源槽 +0x74；诊断用，别当伤害读数）*/
    uint32_t flags;            /* 源槽 +0x00 原值（bit0 active、bit1 圆形）*/
    int32_t  special;          /* 源槽 +0x98 特殊 handler 索引（**诊断用**：弹写 1，但两个 create
                                *   都不清它 ⇒ 槽回收后会残留 ⇒ **不要**拿它当「是不是弹」的判据）*/
    float    x, y;             /* 源槽中心 = **那个伤害源的当前位置**（源槽 +0x1c/+0x20；弹的 tick 每帧
                                *   把 `bullet+0x48` 同步进来 ⇒ 普通弹这里就是**弹的位置**）*/
    /* ★ 2026-09-27（第三十九批 · owner 实跑：「**魔理沙人激光的永点生成位置明显偏离 hit 位置**」）：
     *   对**长光源**（`mode=2` 激光 / 束）来说源槽中心 = **膛口**，而命中发生在**束打到敌机**那一点
     *   ⇒ 卡侧要能拿到**敌机位置**，才能把落点从膛口换成真正的命中处（判据 = 两者距离 > 阈值，
     *   见 `cards/youkai_core.{h,c}` 的 `yk_hit_orb_use_enemy_pos()`）。*/
    float    ex, ey;           /* 被命中敌机的世界坐标 —— 取的是**站点的 arg1**（`[ebx+8]` 指向的 {x,y}，
                                *   与 `on_shot_damage` 的 pos 同一个源）；拿不到时 = (0,0)（卡侧会退化成
                                *   用源槽中心 = 弹的位置）*/
    /* ★★★★ 2026-09-27（**上线当天崩溃后的订正**）：这个站点**没有敌指针**，只有一个 **id** ——
     *   调用方 `th18:0x42f137 push eax`（`eax = [[enemy+0x5588]+0x6830]`）压的第 7 个实参就是
     *   那个单调递增的 id（= `CE_ENEMY_ID` 那一格的值），`esi == 0` = 没有 id。
     *   ⚠ **绝不能把它当指针解引用**（上一版就这么干了 ⇒ 实跑 `0xC0000005` read 0x1274）。
     *   要拿敌对象请用别的站点（`on_enemy_death` / `on_enemy_damage` 那两个才是真指针）。*/
    uint32_t enemy_id;         /* 被命中敌机的 id（0 = 没有 id）；**不是指针** */
    int32_t  has_enemy_pos;    /* 1 = `ex/ey` 是本帧真实读到的敌机坐标；0 = 没拿到（**别用 (0,0) 当落点**）*/
    void    *enemy;            /* ★ 恒为 0（本站点拿不到指针）；留着只为结构体稳定，别用 */
} ce_bullet_hit_t;

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
     * 只在卡自己承认咆哮 active 时返回非负值；SDK 同时改写 edx 与 [ebp-8]（弹里存 set 的那格）。
     * ★ 2026-09-13：同一帧多张卡都想覆盖时**不再先到先得**（卡链表顺序不可预期）——
     *   改按 `shot_set_index_priority` 取最大者（相同则取链表里靠前的，即旧行为）。*/
    int  (*on_shot_set_index)(ce_card_t *, int power_level);
    /* 敌人死亡 / 击破（断点 0x430240，共享给 109/110/112/113）。
     * ⚠ 回调里**禁止析构任何敌人**（UAF：断点必在链表遍历栈里）；新建敌人安全；别缓存 `ev->enemy`。
     * ⚠ 回调里改 `ev->enemy` 的掉落表（`+0x623c`）是**允许**的，而且此刻引擎还没撒落 ⇒ 能改种类。*/
    void (*on_enemy_death)(ce_card_t *, const ce_enemy_death_t *);
    /* 敌人吃到伤害（断点 0x42f2cb；**每敌每帧一次**、在免伤闸之后）。
     * ⚠ 只在回调里记标量；**不要**在这里建 VM/敌人（帧末再做，§AL55 死亡栈纪律）。*/
    void (*on_enemy_damage)(ce_card_t *, const ce_enemy_damage_t *);
    /* 本帧是否跳过「原生主炮行循环」（= 本帧不发原生弹）。108（低速妖机体）与 109（蓄力拦 Z）用。
     * 与 on_shot_set_index 同一个断点（0x45ebfb）：**先问 set index；有卡覆盖了（= 117 咆哮）本帧就不抑制**。
     * ⚠ 只压「原生行循环」，`0x45eca0` 起的**卡广播照跑**（77 / 116 的卡子机不受影响）。*/
/* ★ 2026-09-13 十七修：**装备子机路径**（`Player__tick_shooters_for_ability_card`）会拿
 * `sht set` 去开火，而 set 不只原生装备子机组 —— 咆哮期间它会是**咆哮组**：
 *   0x1e..0x22 = 117 的 WOLF 组（非空 ⇒ 看得见子弹；用户实跑「按 Z 进魔理沙狼咆哮，子机还发射激光」）
 *   0x23..0x27 = EAGLE 组（空组 ⇒ 看不出问题，所以之前只测鹫时以为修好了）。
 * 数值必须与 `assets/sht/append_shooterset.py` 的分配 / `native/sht_ids.h` 一致
 * （`CE_SHT_SET_WOLF_0` / `CE_SHT_SET_EAGLE_4`）。*/
#define CE_SHT_SET_OPTION_MIN   0x0a    /* 原生装备子机组下限 */
#define CE_SHT_SET_OPTION_MAX   0x15    /* 原生装备子机组上限 */
#define CE_SHT_SET_BEAST_MIN    0x1e    /* = CE_SHT_SET_WOLF_0  */
#define CE_SHT_SET_BEAST_MAX    0x27    /* = CE_SHT_SET_EAGLE_4 */

    int  (*on_native_shot_suppress)(ce_card_t *);
    /* BP_ce_option_shot_gate 0x40a9c3：Player__tick_shooters_for_ability_card 0x40a9c0 的
     * 序言后 `mov edx,[ebp+8]; sub esp,0xc`（6 字节，两条完整指令、无相对寻址）。只对
     * **原生装备子机组** sht set 0x0a..0x15 询问。返回 1 = 本帧这一组装备子机弹不发；
     * SDK 跳到该函数自己的收尾 0x40aa9e（mov esp,ebp / pop ebp / ret 0x10）。
     * ★ 2026-09-12 十三修：上一版站点取 3 字节被 thcrap 拒绝装载，门回读失败后触发降级；
     * 现在站点长度修到 6 字节，但功能仍由 `CE_OPTION_SHOT_GATE_ENABLE` **默认关**（sdk.c）。
     * 117 若需要拦装备子机普通弹，等实机验证后再打开；不开时此回调不会被调用。*/
    int  (*on_option_shot_gate)(ce_card_t *, int sht_set);
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
    /* ★ 2026-09-13（卡 113 灵界）：`on_shot_set_index` 的**优先级**（仲裁用，取最大者胜）。
     * ★★ 2026-09-16z（**用户口径**「改子机和改主体发弹的覆盖原则」）——数值越大越先赢：
     *   原则：① **特殊大于常态** ② **专用大于通用**；阶梯：**117 > 113 > 108 > 111**（末两条见 §8）。
     *   0  = 默认（不覆盖的卡）
     *   10 = 灵界（113）（`CE_SHT_PRIO_113`）
     *   20 = 咆哮（117）（`CE_SHT_PRIO_117`）—— ★ **人为规定 117 > 113**（用户 2026-09-16z 逐字）
     * ⚠ 只有在 `on_shot_set_index` 返回 >= 0 时才参与仲裁；相同优先级时取链表里靠前的（旧行为）。
     * ⚠ 加在结构体**最后**：`ce_hooks_t` 是各卡的静态常量（全用指定初始化器），尾部加字段不影响旧卡。*/
    int  shot_set_index_priority;
    /* ★★★ 2026-09-27（卡 108 · owner「3 可以贴一手，关键在站点写法」）：**一发自机弹 × 一个敌机**的命中
     *   （断点 `ce_bullet_hit` `0x45f466`）。一手 = TH08「極人期间每一发自机弹碰到敌机 ⇒ 在弹的位置掉
     *   1 颗刻符」（`th08:0x451860`–`:0x451892`）；TH18 没有等价函数 ⇒ 取「某个伤害源**第一次**命中某个敌」。
     *   ⚠ `ev->enemy` 只在回调内有效；回调里**只记标量**（建 VM / 建道具请放到自己的 tick 里做）。
     *   ⚠ 先判 `ev->is_player_bullet`：同一个池里还有原生卡与各卡 `ce_damage_rect` 放的源。
     *   ⚠ 加在结构体**最后**（`ce_hooks_t` 全是静态常量的指定初始化器 ⇒ 尾部加字段不影响旧卡）。*/
    void (*on_bullet_hit)(ce_card_t *, const ce_bullet_hit_t *);
    /* ★★★ 2026-10-19（腐化 70 × 原生 23 死秽回避之药）：**死亡消耗了 B**（`spent` = 已经扣掉的发数 1–2）。
     *   两个来源，语义相同：
     *     ① 站点 `ce_death_save_spent` `0x40a44b`：原生死亡回避 `0x40a2a0` 扣完 CURRENT（`0x40a3ed`/`0x40a42c`）
     *        之后的汇合点 ⇒ `spent = MAX_BOMBS − CURRENT_BOMBS`（此刻 MAX 还没动）；
     *     ② 卡侧 `ce_death_save_spent_note(n)`：别的卡在 `+0x10` 里自己扣了 B（108 的「有 106 多扣 1」）。
     *   ⚠ 只做记账：**不要**在这里扣 CURRENT（扣已经发生过了）；要「把这次消耗记进自己的预算」就在这里改自己的量。
     *   ⚠ 加在结构体**最后**（`ce_hooks_t` 全是静态常量的指定初始化器 ⇒ 尾部加字段不影响旧卡）。*/
    void (*on_death_save_spent)(ce_card_t *, int32_t spent);
} ce_hooks_t;

/* 改这张卡当前的自机弹伤害倍率（千分比）。只在值真的变了的时候调（古锭刀在终符沿、衔尾蛇在叠层时）。*/
/* ======================================================================================
 * ★★ 2026-09-16z **子机与主炮的「覆盖原则」阶梯**（用户口径逐字）
 * ======================================================================================
 * > 「改子机和改主体发弹的覆盖原则：1、**特殊大于常态** 2、**专用大于通用**
 * >   108，**仅低速妖机体 > 111 魔理沙**；113 灵界 or 117 咆哮 > 108 & 111；**人为规定 117 > 113**」
 *
 * 四个**互不相同的**争用面（阶梯在每一面上的落点）：
 *   | 争用面 | 谁在争 | 落点（谁赢） | 在哪实现 |
 *   | --- | --- | --- | --- |
 *   | 主炮 shooterset（fire-time set index）| 117 咆哮 / 113 灵界 / **111 魔理沙B** | **117 > 113 > 111** | 本文件的 `shot_set_index_priority` + `sdk.c` 的 `BP_ce_shot_set_index`（取最大）|
 *   | 原生主炮 set **认领** | 117 咆哮(20) / 113 灵界(10) / 108 妖机体（默认 0）/ 111 魔理沙B(−1) | 取优先级最大者 | `sdk.c` 的 `BP_ce_shot_set_index` |
 *   | 原生主炮「本帧不发」| **只剩 109**（108 于 2026-09-22 改成认领，见下） | 108 原来也在这条上（只在没人覆盖 set 时才被问到）| `sdk.c` 的同一条断点（先覆盖、后抑制）|
 *   ⚠ **2026-09-22 口径变更（用户 #13「妖形态主机也发弹」）**：108 **从「只抑制」改成「认领 set」**。
 *     为什么：一手里 TH08 的妖侧 set **含本体行**（`ply00as.sht` 满档 3 行 `+0x20` = 0 = 本体），
 *     而 108 原来把整条原生行循环压掉 ⇒ ① 妖形态本体一发都没有 ② **连带把 113/117 的覆盖也压死**
 *     （它们同样靠那条行循环发）。改后：108 低速返回自己那组（默认优先级 0）、113(10)/117(20) 仍赢它、
 *     111(−1) 输给它 —— 而 111 的组在 108 聚焦时本来就被压着（净损失为零）。
 *     ⚠ 同时作废用户 2026-09-12 的「无子机 ⇒ 放行原生弹」（认领之后本体行照发），见 `TUNING.md` §2.0y。
 *   | 子机**外观落笔**（pose 层）| 108（低速）/ 111（魔理沙）| **108 > 111** —— 且 108 只在**低速且妖机体在场**时声明 | `ce_option_pose_declare` + `sdk.c` 的选举（**id 最小者**落笔：108 < 111）|
 *   | 子机**开火闸**（native option path）| 113 灵界 / 117 咆哮 | 两家都「一律停火」⇒ 结果一致（无先后可言）| `on_option_shot_gate` |
 * ⚠ 「117 > 113」是**人为规定**（用户定），不是从某个一手推出来的；「108 仅低速 > 111」的两个条件
 *   （低速、妖机体在场）由 108 自己的声明条件保证（`yk_should_suppress_native()` 与 pose 的 count>0）。
 * ⚠ 111 是**最底**（`CE_SHT_PRIO_111` = −1，比"不覆盖"的默认 0 还低）—— 所以「特殊/专用」的东西
 *   一出现就该盖住它。
 * ★★ 2026-09-16zb（用户口径「尽可能完全对齐」+「主炮别换弹幕了」）：**111 现在也覆盖主炮**了 ——
 *   TH11 的主炮每发伤害**逐元素逐档**不同（16→8..16，`MARISA_TH11_MAIN_DMG`）⇒ 不覆盖就没法照抄。
 *   覆盖的那一组只含 2 行 `slot 0` 的主炮行（弹/音效仍是本作原生）；因为 111 优先级是全场最低，
 *   108（默认 0）/113（10）/117（20）任一生效都能盖住它 ⇒ 阶梯 **117 > 113 > 108 > 111** 仍然成立。*/
#define CE_SHT_PRIO_113   10   /* 113 灵界：主炮整组换成灵界组（`CE_SHT_SET_TRANCE` 0x28）*/
#define CE_SHT_PRIO_117   20   /* 117 咆哮：主炮整组换成狼/鹫组（用户人为规定它 > 113）*/
/* 111 魔理沙B：主炮组按 TH11 的逐元素逐档伤害整组覆盖 —— **全场最低**（负值 = 会覆盖、但永远让位）。
 * 为什么不给 0：0 是「不覆盖的卡」的默认值，108 没显式设过 ⇒ 同值会走"链表靠前者赢"（不确定）。*/
#define CE_SHT_PRIO_111  (-1)

void ce_set_bullet_damage_permille(ce_card_t *c, int permille);

int ce_give_card(uint32_t id, uint32_t mode, uint32_t notify);
int ce_destroy_card(ce_card_t *victim);
ce_card_t *ce_pick_random_card(ce_card_t *exclude);
void ce_log(const char *fmt, ...);

/* ★★★ 2026-10-19（腐化 70 × 原生 23 死秽回避之药 = 简单无敌）：**卡侧「死亡消耗了 B」的上报通道**。
 *
 * 引擎自己那条死亡回避（`Card__death_save_bomb_revive 0x40a2a0`）由站点 `0x40a44b` 自动上报
 * （`BP_ce_death_save_spent`，spent = MAX − CURRENT）。但**卡侧**也有一条会在死亡时扣 B 的路，
 * 它直接写 `CURRENT_BOMBS`（不走 `consume_bomb` ⇒ `on_bomb_spent` 也抓不到）：
 *   · 108 永夜抄：`on_death_before_deathbomb` 里「有 106 ⇒ 决死多扣 1 个 B」（owner 2026-10-11 口径）。
 * 那种写入会被「腐化」每帧的 `CURRENT = MAX` 钉回去（= 白送），所以扣完必须调本函数报一次：
 * 沿卡链表广播 `on_death_save_spent(NULL 卡的 spent)`，由收方（腐化）把它记进自己的预算。
 *
 * 语义约定：
 *   · `spent` = 这一次死亡**已经真的从 CURRENT 扣掉**的发数（> 0；≤ 0 直接忽略）；
 *   · 调用**前**先完成扣 B（本函数只广播、不写引擎值）；收方回写 CURRENT 是它自己的事；
 *   · 没有收方（没带腐化）= 空转，一个字节都不写 ⇒ 旧行为逐位不变。*/
void ce_death_save_spent_note(int32_t spent);
