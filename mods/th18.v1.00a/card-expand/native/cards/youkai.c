/* 永夜抄（id 108）—— 妖机体 / 使魔（th08 系）。
 *
 * 设计唯一真源：`docs/superpowers/specs/2026-09-12-youkai-108-design.md`。
 * 一手：`local/_scratch/research-108-shooter-tick.md`（止射闸）、
 *       `local/_scratch/research-108-damage-transfer.md`（伤害转交）、
 *       `local/_scratch/research-death-signal-survey.md`（敌人移除点全集）、
 *       `docs/superpowers/plans/2026-09-10-ufo-card-feasibility.md` §1.1（spawn 参数表）。
 * 共享契约：`EVENTS-CONTRACT.md`（止射闸 = OR；`on_enemy_death` = 广播只读；
 * 低速子机 = 选举覆盖；1/2 转嫁 = 单一落笔）。
 *
 * **一句话**：低速（聚焦）= 整个人换成妖侧机体（外观 + 子机 + 弹幕同时切、原生弹被压掉），
 * 高速 = 原机体；低速时身边的**使魔** = 敌人发射口实体化出来的**真敌人**（`NoEnemyDebug` 惰性空壳），
 * 使魔可打、被打死 ⇒ 那个槽不再重建、并按 **1/2** 把伤害转嫁给本体。
 *
 * 纯逻辑（低速判定 / 档位 / 1:1 映射与状态差分 / 取整与上限）在 `youkai_core.c`，
 * 主机单测 `tests/test_youkai_core.c`；本文件只做「碰引擎的那部分」。
 *
 * ⚠⚠ **与 spec 的一处主动差异**（必须知道）：spec §3.5 只要求每帧钉 `enemy+0x1270/+0x1274`；
 * 本实现**同时**钉 `enemy+0x12b4/+0x12b8`（`zEnemyData.abs_pos`）—— 理由是 AbilityManager tick
 * （优先级 `0x16`）**早于**敌人 tick（`0x1b`，AUDIT V20 口径），惰性空壳虽然不跑 ECL、速度恒 0，
 * 但 `final_pos` 是否每帧由 `abs_pos` 重算**没有一手证据** ⇒ 只钉 `final_pos` 有「当帧被敌人 tick
 * 写回出生点」的风险。双写是**保守**做法，🟡 待实跑确认哪一格才是可见位置的源头。
 *
 * ★ 使魔的**画面**：惰性空壳不跑 ECL ⇒ 谁也不会给它装 anm 脚本 ⇒ 卡侧起一个**跟随 VM**
 *   （`YK_FAM_ANM_LAYER` 那一节，范式照 112 飞碟）画四色零售切图（`ability.anm` 206..209）。
 *
 * ★ 占位清单（每条都是「需要资产 / 需要用户决策」）见文件尾 —— 取值一律是**不做**这个语义。 */
#include <string.h>

#include "sdk.h"
#include "youkai_core.h"

/* ============================ 裸引擎地址 / 偏移 ============================
 * ★ 本卡**不改 sdk.h / engine.h**（硬约束）⇒ 缺的包装在这里写 `static inline` / 宏，每条带出处。
 * 地址 = TH18 v1.00a。已有 SDK 包装的**不重复造**（`ce_spawn_enemy` / `CE_SPAWN_*`）。*/

/* 发射口（spec §6.3 / IDEAS.md §1.3）—— engine.h 里没有这几条，故落在本文件。
 *
 * ★★ **2026-09-12 对抗审计订正：这三个基址原本是 zEnemyData 相对、却被当成 zEnemy 相对用** ——
 *   错 `0x122c`（= `zEnemyData` 在 `zEnemy` 里的偏移），读到的全是**敌人 ECL-VM 头**。
 *   一手证据（逐条）：
 *     ① `local/vendor/th-re-data/data/th18.v1.00a/type-structs-own.json:818`
 *        `["0x122c","data","struct zEnemyData"]` —— 即这三个 `0x598/0x4e58/0x4f18` 是
 *        **`zEnemyData` 内部的偏移**（同表 `:896` 的 `0x4ff4 life` 与已知的 `enemy+0x6220`
 *        正好差 `0x122c`，交叉印证）。
 *     ② 引擎自己的读取点全都在「已经 +0x122c 过的基址」上：ECL 派发器
 *        `430d30: 81 c1 2c 12 00 00 add ecx,0x122c`（把 zEnemy 换算成 zEnemyData 后再进 handler），
 *        `433ec0: 8d b7 98 05 00 00 lea esi,[edi+0x598]` + `433ec6: imul ecx,eax,0x488`
 *        + `433edc: b9 22 01 00 00 mov ecx,0x122` + `433ee1: f3 a5 rep movs`（0x122 dword = 0x488 字节）,
 *        `433eef: movq xmm0,[edx+esi*4+0x4e58]`、`433f46: movss xmm0,[edi+ecx*4+0x4f20]`。
 *     ③ 而 `CE_ENEMY_FOREACH` / `ce_enemy_by_id` 交出来的是 **zEnemy\***（本卡自己在用同一个 e 读
 *        `e+0x6830`(enemy_id) / `e+0x635c`(flags) / `e+0x1270` / `e+0x12b4`，这些才是 zEnemy 相对）。
 *   ⇒ 三处常量一律 **+0x122c** 换算成 zEnemy 相对（`0x598→0x17c4`、`0x4e58→0x6084`、`0x4f18→0x6144`），
 *     并在下面注明各自的 zEnemyData 相对原值。**这是会让整条「发射口 → 使魔」链失效的错误。** */
#define YK_DATA_OFF         0x122c /* `zEnemyData` 在 `zEnemy` 里的偏移（vendor `zEnemy +0x122c data`）*/
#define YK_EMIT_PROPS       (YK_DATA_OFF + 0x598)  /* `bullet_props[i]`（zEnemyData 相对 0x598；stride 0x488）
                                                    * `.type` = `+0x00`（**int32**，见 `yk_slot_type` 的注）*/
#define YK_EMIT_PROP_STRIDE 0x488
#define YK_EMIT_OFFSETS     (YK_DATA_OFF + 0x4e58) /* `bullet_mgr_offsets[i]`：相对偏移（float2 xy + float）*/
#define YK_EMIT_ORIGINS     (YK_DATA_OFF + 0x4f18) /* `bullet_mgr_origins[i]`：绝对原点；`.z > 0.9` = 绝对模式 */
#define YK_EMIT_VEC_STRIDE  0x0c
/* 敌人字段（engine.h 没有的几条）*/
#define YK_ENEMY_ABS_POS    0x12b4 /* `zEnemyData.abs_pos` = `enemy+0x122c+0x88`（= spawn `+0x00` 的去处，
                                    * engine.h `CE_SPAWN_POS` 的注释）*/
#define YK_ED_LIFE_CUR      0x6220 /* `zEnemyData.life.current`（= `CE_ENEMY_DATA + 0x4ff4`；扣血点 `0x42f314` 一手）*/
#define YK_FLAGS_DELETE_NOW 0x2000000u /* `flags_lo` 位 25 = 「本帧删」：`0x42d200` 只置它，
                                        * sweep `0x42dfc5` `test [enemy+0x635c],0x2000000` → 直接析构
                                        *（不跑死亡处理、不掉落、不跑死亡 ECL）⇒ 使魔的**静默撤退**。*/
/* 使魔皮肤（review §6.5.4 说要 `anmSelect` 的等价物）—— ❓ **未证实，别照抄这两个偏移**。
 * 2026-09-12 对抗审计（一手反查 `local/_scratch/th18_full.asm`）：
 *   ① 全反汇编里 `+0x1490` / `+0x149c` 作为内存操作数**一条引用都没有**
 *      （`grep -cE '\[[a-z0-9]+\+0x1490\]' th18_full.asm` = 0，0x149c 同为 0）；
 *   ② 同族偏移里真正有引用的是 `+0x14e8/+0x14ec/+0x14f0/+0x14f4/+0x14f8/+0x14b8/+0x14d0…`
 *      （`engine/ecl/th16/01-ecl-context-and-variables.md` §2 把它们记为 **ECL 局部变量
 *      `+0x1498..0x14a4 = I0..I3` / `+0x14a8..0x14b4 = F0..F3`**）；
 *   ③ ⇒ **`0x149c` 与引擎文档记的 ECL 局部变量 `I1` 撞址**，`0x1490` 则无名可对。
 * 当前**是死代码**（占位 `YK_FAM_ANM_INDEX = -1` ⇒ `_pin_familiar` 里那个 `if` 恒假），
 * 所以没有实害；但**谁把占位翻成真值，就会往 ECL 局部变量上写**。
 * ⇒ 结论：这两个偏移的语义 🟡 **未证实**，要坐实需要一条一手证据，二选一：
 *   (a) 在 `th18.exe` 里找到**读它**的指令（`anmSelect` opcode handler 里对 `ctx+0x1490` 的
 *       `movzx`/`mov`，或建 anm VM 时把该格当脚本号用），或
 *   (b) 找到**写它**的 spawn 参数/opcode（对照 `zEnemyData` 的字段图确认 +0x264/+0x270 两个
 *       相对偏移在 TH18 是否仍成立）。
 *   在那之前**不要**用这两个常量改任何使魔外观。 */
#define YK_ENEMY_ANM_INDEX  0x1490 /* ❓ 未证实（全 asm 无引用）—— 声称是 `selected_anm_index`（= +0x264）*/
#define YK_ENEMY_ANM_MAIN   0x149c /* ❓ 未证实（全 asm 无引用，且与 ECL 局部变量 I1 撞址）—— 声称是 `anm_main`（= +0x270）*/
/* 自机弹的打包 set id（`engine/sht/th18/02` §2：`(set<<8)|组内行号`，`0x45ec69` 打包 / `0x45ee10` 回读）*/
#define YK_PB_PACKED_ID     0xac
/* 妖侧 shooterset 的行指针（`sht + 0xe0 + idx*4`，与 `0x40a9c0` / `0x45ebfb` 同一张表）*/
#define YK_SHT_SET_ROW      0xe0

/* ---- ★ 资产占位（每条都要资产/用户才能定案；取值一律是「不做」这个语义）---- */
#define YK_OPTION_SCRIPT_YOUKAI  CE_ANM_ABILITY_SCRIPT_PEASHOOTER_OPT
                                     /* ★ TODO(资产 §5)：妖侧子机的 anm 脚本 —— **占位 = 借**豌豆射手的
                                      * 子机脚本 102（先例做法：116 借零售贴图）。真值要往 `ability.anm`
                                      * 追加妖侧子机脚本（`assets/build_ability.py` 的 ORDER）。
                                      * 注意：`yk_option_count()` 现在是 0 ⇒ 这一项**还不会**被声明出去。*/
#define YK_OPTION_RETRY      60     /* 没拿到子机时的重试间隔（槽满 / 关卡中途买到），照 77 */
#define YK_OPTION_MAX        2      /* 一次最多管几个子机槽（12 格公共池还要留给 71/77/116）*/
/* ★ TODO(资产/决策 §3.3)：低速子机阵型 —— 声明层收的偏移（`+0x6c/+0x70` 聚焦、`+0x64/+0x68` 非聚焦）。
 * 占位 = 全 0（= 自机中心）：不自作主张摆阵型。单位约定（**🟡 待实跑核**）：声明层的 float 按**像素**，
 * 原生字段是 1/128 定点 ⇒ 落笔时 × `YK_SUBPIXEL`。`ce_option_alloc` 的 off 参数是 x 还是 y
 * 两处文档说法不一致（71 写 0x18 / 77 写 0x28）⇒ 落笔时四个字段都自己写，不依赖它。*/
#define YK_OPTION_OFFSET_FX  0.0f
#define YK_OPTION_OFFSET_FY  0.0f
#define YK_OPTION_OFFSET_UX  0.0f
#define YK_OPTION_OFFSET_UY  0.0f
#define YK_OPTION_OFFSET_ALLOC 0    /* 传给 `ce_option_alloc` 的那个 off（随后被上面四个字段覆盖）。
                                     * 落笔走 `ce_option_get`：它第一次调用就是 `ce_option_alloc`
                                     *（认领失败 ⇒ 立即申请；之后才按 `YK_OPTION_RETRY` 补申请）。*/
#define YK_SUBPIXEL          (1.0f / CE_SUBPIXEL_TO_PIXEL)   /* 128.0f：像素 → 原生定点 */
#define YK_FAM_ANM_INDEX  (-1)      /* ★ TODO(资产 §5)：使魔贴图。`NoEnemyDebug` 不跑 ECL ⇒ 不写这两格
                                     * 时会画「当前关卡敌人 anm 的 slot 0」（audit-open-items-106-109 §②）。
                                     * 占位 = 不写（< 0）；填真值时两格一起给。
                                     * ★★ 2026-09-12：**画面不再走这条路** —— 四色零售切图已在 `ability.anm`
                                     * 206..209，由卡侧的**跟随 VM** 画（见下面 `YK_FAM_ANM_LAYER` 那一节）。
                                     * 这两格保持占位（偏移本身仍 ❓ 未证实，别翻成真值）。*/
#define YK_FAM_ANM_SCRIPT (-1)
#define YK_TRANSFER_BOX_W 16.0f     /* 转嫁源盒子。研究 §4 的建议值（放 boss 坐标的小盒，
                                     * 避免顺带打到部件 / 杂鱼）；🟡 待实跑微调。*/
#define YK_TRANSFER_BOX_H 16.0f
#define YK_TRANSFER_SRC_LIFE 2      /* 源寿命 2 帧 = 建源帧 + 下一帧各结算一次，之后引擎自己回收
                                     *（`0x45c402` 循环，research §1.3a）；先例 71/72/75 同款。*/

/* ------------------------------------------------------------------ 卡状态
 * 私有状态预算 = `CE_STATE_BYTES`(512) − `CE_STATE_RESERVED`(16) = 496 B（sdk_core.h）。
 * **使魔表不进这里**：32 × 24 B = 768 B，放文件级静态（照 117 动物灵 `s_tok[40]` 的做法）。
 * 卡是单例（一个 id 只可能有一个对象），静态表在 ctor / on_load / on_run_reset 里复位。*/
typedef struct {
    ce_option_t   opt[YK_OPTION_MAX];  /* 子机槽（SDK：认领 / 申请 / 收起）*/
    yk_transfer_t tr;                  /* 1/2 转嫁累加器 */
    int32_t       opt_script;          /* 手里这几个子机是用哪个 anm 脚本申请的（-1 = 没有）*/
    int32_t       declared;            /* 已经发出去的子机声明数量（-1 = 还没声明过，需要重发）*/
    uint32_t      shot_timer;          /* 我们自己的「射击帧」计数（引擎那个松开 Z 会跳，相位不可控）*/
    uint32_t      spawns;              /* 本局建了几只使魔 */
    uint32_t      broken;              /* 本局被打死的使魔数（= 被打死的发射口槽数）*/
    uint32_t      lost;                /* 状态差分撤掉的使魔数 */
    uint32_t      transferred;         /* 本局累计转嫁的伤害 */
    uint32_t      dropped;             /* 取不到 boss 而丢掉的转嫁量（研究 §4：本帧不转嫁）*/
    uint32_t      fam_full;            /* 使魔表满 / 堆分配失败而放弃的槽数 */
    uint8_t       logged_opt;          /* 「子机声明停用（资产未到）」只记一次 */
    uint8_t       logged_sht;          /* 「妖侧 shooterset 没装」只记一次 */
    uint8_t       logged_noshot;       /* 「压了原生弹但自己也没弹」只记一次 */
    uint8_t       logged_full;         /* 「使魔表满 / 堆拒绝」只记一次 */
    uint8_t       logged_gate;         /* 「发射口精确闸生效（使魔没了 ⇒ 该槽哑火）」只记一次 */
    uint8_t       _pad[3];             /* 对齐（保持结构布局稳定）*/
} yk_card_t;

static yk_map_t s_map;              /* 使魔表（(enemy_id, 发射口槽) → 使魔）*/

/* ============================== 小工具 ============================== */
static int yk_base_damage_cap(void)                       /* 照 76 guding.c：sht 头的每敌每帧上限基值 */
{
    uint8_t *p = CE_PLAYER();
    uint8_t *sht;
    if (!p) return 0;
    sht = *(uint8_t **)(p + CE_PLAYER_SHT_PTR);
    return sht ? *(int32_t *)(sht + CE_SHT_MAX_DMG) : 0;
}

/* 发射口槽是否活跃：`props[i].type(+0x00) != 0`（`etNew` 清零、只有 `etSprite` 写它，spec §6.3）。
 * ★ 2026-09-12 审计：`.type` 是 **int32**，不是 u16 —— 一手 `43404e: 89 84 3e 98 05 00 00
 *   mov DWORD PTR [esi+edi*1+0x598],eax`（4 字节存 ECL 的 int 参数），vendor
 *   `zEnemyBulletShooter 0x0 type int32_t`。原来读 u16 在「非 0 / 0」这个判据上结果相同（小端），
 *   但语义是错的 ⇒ 按 int32 读。 */
static int yk_slot_type(uint8_t *e, int i)
{
    return *(const int32_t *)(e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE) != 0;
}

static void yk_slot_offsets(uint8_t *e, int i, float off3[3], float org3[3])
{
    __builtin_memcpy(off3, e + YK_EMIT_OFFSETS + (size_t)i * YK_EMIT_VEC_STRIDE, sizeof(float) * 3);
    __builtin_memcpy(org3, e + YK_EMIT_ORIGINS + (size_t)i * YK_EMIT_VEC_STRIDE, sizeof(float) * 3);
}

/* 当前 boss（`mgr+0x48` 的 4 个 id，照 `get_boss_enemy_full` `0x4237f0` 的读法；76 guding.c 同款）。
 * 取不到（道中 / 已死 / 不可锁定）⇒ 本帧不转嫁（研究 §4）。*/
static uint8_t *yk_boss(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    unsigned i;
    if (!em) return 0;
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        int32_t id = *(int32_t *)(em + CE_EM_BOSS_IDS + i * 4);
        uint8_t *e;
        if (!id) continue;
        e = ce_enemy_by_id(em, id);
        if (ce_enemy_lockable(e)) return e;
    }
    return 0;
}

/* 每帧用 id 回查使魔（**绝不缓存敌人指针**：敌人是堆对象，过场 `destroy_all` 会静默释放）。*/
static uint8_t *yk_familiar_by_id(uint32_t fam_id)
{
    uint8_t *em = CE_ENEMY_MGR();
    return em && fam_id ? ce_enemy_by_id(em, (int32_t)fam_id) : 0;
}

/* 按 `(enemy_id, 发射口槽)` 取**本卡记的**使魔 id（0 = 这一槽没有活着的使魔）。
 * ★ 只读自己的槽表 —— 关闸判据、位置重钉、life 差分三条路都用它，**不碰 `ev->enemy`**
 *   （那是敌人本体，且只在回调内有效）。同时核一遍「按 id 还查得到」⇒ 表里记的 id 死了也当 0。*/
static uint32_t yk_slot_fam_id(const yk_map_t *m, uint32_t enemy_id, int slot)
{
    const yk_slot_t *e = yk_map_find((yk_map_t *)m, enemy_id, slot);
    if (!e || !e->fam_id) return 0;
    return yk_familiar_by_id(e->fam_id) ? e->fam_id : 0;
}

/* ========================= §3.5 使魔的建 / 钉 / 撤 ========================= */
static uint8_t *yk_spawn_familiar(float x, float y)
{
    uint8_t spawn[CE_SPAWN_SIZE];
    float *pos;

    if (!CE_ENEMY_MGR()) return 0;
    __builtin_memset(spawn, 0, sizeof spawn);
    pos = (float *)(spawn + CE_SPAWN_POS);
    pos[0] = x;
    pos[1] = y;
    pos[2] = 0.0f;
    *(int32_t *)(spawn + CE_SPAWN_SCORE) = 0;             /* 打死不给分（spec §6.1 的无害化）*/
    *(int32_t *)(spawn + CE_SPAWN_MAIN_DROP) = 0;         /* 不掉落 */
    *(int32_t *)(spawn + CE_SPAWN_PARENT) = 0;
    /* `CE_SPAWN_FLAG_A/B` 一律不设：`flags_lo & 0x31` 绝不能置（置位 = 伤害免疫，spec §3.9 一手），
     * 用户定案 4 也是「不设」。life 钳在 < 1000（`CE_SPAWN_LIFE` 的 ⚠）。*/
    *(int32_t *)(spawn + CE_SPAWN_LIFE) = yk_clamp_spawn_life(YK_FAM_LIFE);
    /* 惰性空壳：`NoEnemyDebug` 的 `wait(1000)` 死循环 ⇒ 不跑 ECL、不动、不开火、不掉落、不自删。*/
    return (uint8_t *)ce_spawn_enemy("NoEnemyDebug", spawn);
}

/* 每帧钉位置（spec §3.5）+ 皮肤（资产未到时不动）。*/
static void yk_pin_familiar(uint8_t *f, float x, float y)
{
    *(float *)(f + CE_ENEMY_POS_X) = x;                   /* final_pos（spec §3.5 指定的那两格）*/
    *(float *)(f + CE_ENEMY_POS_Y) = y;
    *(float *)(f + YK_ENEMY_ABS_POS) = x;                 /* + abs_pos（见文件头的 ⚠ 差异说明）*/
    *(float *)(f + YK_ENEMY_ABS_POS + 4) = y;
    if (YK_FAM_ANM_INDEX >= 0 && YK_FAM_ANM_SCRIPT >= 0) { /* 占位 -1 ⇒ 现在不写 */
        *(int32_t *)(f + YK_ENEMY_ANM_INDEX) = YK_FAM_ANM_INDEX;
        *(int32_t *)(f + YK_ENEMY_ANM_MAIN) = YK_FAM_ANM_SCRIPT;
    }
}

/* 撤一只使魔：置 `flags_lo` 位 25 ⇒ 同一帧的 EnemyManager sweep 静默析构它
 *（`0x42dfc5` → `0x42dff2 call [vt+0x14]`；不做死亡处理 / 不掉落 / 不跑死亡 ECL —— 正是「撤」的语义）。
 * ⚠ 不自己调析构（那是 UAF：我们可能正处在别人的链表遍历里，research-death-signal-survey §5）。*/
static void yk_kill_familiar(uint8_t *f)
{
    *(uint32_t *)(f + CE_ENEMY_FLAGS) |= YK_FLAGS_DELETE_NOW;
}

/* ====================== ★ 使魔的跟随 VM（画面）======================
 * 为什么需要：使魔 = `NoEnemyDebug` 的**惰性空壳**（`wait(1000)` 死循环）—— 它不跑 ECL，
 * 也就**没有任何人**给它装 anm 脚本 ⇒ 引擎画不出东西（`YK_FAM_ANM_*` 那两个偏移 ❓ 未证实，
 * 而且 `etSprite` 只在 ECL 里跑）。四色零售切图已经进了 `ability.anm`
 * （script 206..209 = TH08 `enemy.anm` script53..56；见
 * `assets/ability/scripts/206_youkai_familiar_s53.anm.txt` 的注）⇒ 由卡侧起 VM 画。
 * **范式逐条照 112 飞碟的跟随 VM**：`ufo.c` 的 `ufo_body_vm_spawn` / `ufo_body_vm_follow` /
 * `ufo_body_vm_kill`（建壳那一刻起、每帧钉位置、壳没了就删）。
 *
 * 生命周期（**铁律：长持续效果必须交代「被删掉会怎样」**）：使魔消失的**每一条路**都要过
 * `yk_fam_vm_*`，尤其 `on_delete`（删卡那一刻 AbilityManager 冻着、卡的 tick 回调全停，
 * 只有 `on_delete` 跑得到）⇒ `yk_drop_all` 里兜一次 `yk_fam_vm_kill_all`。*/

/* 跟随 VM 的层 = **世界层 7**。
 *   一手：`engine/anm/th18/02-render-stages.md` §1 —— 层 6/7/8/9/10/11 = 「敌机等」，
 *         且按 §2 的相机表全在**相机 3 = 游玩区**；使魔是**场上实体**，就该落在这一档。
 *   先例：`ufo.c` 的 `UFO_BODY_ANM_LAYER 7`（同一用途：卡侧替一个场上实体起 VM）。
 *   ⚠ 使魔的脚本**没有** `layer()`（零售 enemy.anm script53..56 都没写，那几份 anm.txt 注明）
 *     ⇒ 这个参数是**唯一**的层来源（飞碟本体能说「脚本自己写 layer(7)」，这里不能照抄那句话）。
 *   ❌ 不用 HUD 的 24/25：那是**相机 2 的外围 UI 列表**（§1 的 `0x3c` / `0x39` 两行专门提醒
 *     「世界列表 24」与「UI 列表 24」是两码事），与场上实体的语义无关。
 *   🟡 层号本身未实跑核（ufo.c 的 7 也标着未验）。实跑若整只看不见，第一个该动的就是这一个数。*/
#define YK_FAM_ANM_LAYER 7

/* 变体（0..3）→ `ability.anm` 里的脚本号。★ 用 `anm_ids.h` 的**宏名**（`make anm` 生成），
 * 不写死数字；四条逐一列（不假设号连续 —— 万一资产重排，这里会**编译期**就对不上而不是静默画错）。*/
static int yk_fam_anm_script(int variant)
{
    switch (variant) {
    case 0:  return CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53;   /* 红 */
    case 1:  return CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S54;   /* 绿 */
    case 2:  return CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S55;   /* 蓝白 */
    default: return CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S56;   /* 黄 */
    }
}

/* 使魔 ↔ 跟随 VM 的对照表（**文件级静态**，照 117 动物灵 `s_tok[40]` / 112 的做法；
 * 不进卡的 496 B 私有状态预算）。32 × 12 B = **384 B**。
 * 键 = `fam_id`（单调递增、不复用 ⇒ 不会撞键）；`fam_id == 0` = 空槽。
 * ★ 为什么不塞进 `yk_slot_t`：那是**纯逻辑**层的结构（进主机单测、有 sizeof 断言，头文件只 <stdint.h>），
 *   anm 句柄是引擎侧的东西 ⇒ 分开放（也顺手让 `sizeof(yk_card_t)` 一个字节都不变）。*/
typedef struct {
    uint32_t fam_id;   /* 0 = 空槽 */
    uint32_t anm_id;   /* 0 = 没有 VM（起失败 / 已被引擎销毁）*/
    int32_t  variant;  /* 0..3：自愈重起时要按同一套脚本再起一个 */
} yk_fam_vm_t;
static yk_fam_vm_t s_fam_vm[YK_FAM_MAX];

static yk_fam_vm_t *yk_fam_vm_find(uint32_t fam_id)
{
    unsigned i;
    if (!fam_id) return 0;
    for (i = 0; i < YK_FAM_MAX; ++i) if (s_fam_vm[i].fam_id == fam_id) return &s_fam_vm[i];
    return 0;
}

/* 收掉一条记录（句柄清零 ⇒ 幂等；`ce_anm_delete` 内部有 ANM 管理器门槛）。*/
static void yk_fam_vm_kill(yk_fam_vm_t *v)
{
    if (!v) return;
    if (v->anm_id) { ce_anm_delete(v->anm_id); v->anm_id = 0; }
    v->fam_id = 0;
    v->variant = 0;
}

static void yk_fam_vm_kill_id(uint32_t fam_id) { yk_fam_vm_kill(yk_fam_vm_find(fam_id)); }

/* 收掉**整张表**的 VM。⚠ 只看句柄、不看 `fam_id` 的来路：删卡那一刻 tick 全停，
 * 没有任何「顺手收」的机会（照 `ufo.c` 的 `ufo_tok_vm_kill_all`）⇒
 * `yk_drop_all`（删卡 / 高速 / 局末 / 断链）与 `on_ctor` 都要过这里。*/
static void yk_fam_vm_kill_all(void)
{
    unsigned i;
    for (i = 0; i < YK_FAM_MAX; ++i) yk_fam_vm_kill(&s_fam_vm[i]);
}

/* 起一个跟随 VM 并**立刻**摆到实体坐标（世界层 VM 的实体坐标 (0,0,0) = 弹幕区上边框中点
 * ⇒ 不先摆会闪一帧）。失败只报日志（画不出来 ≠ 卡坏，`ufo_body_vm_spawn` 同款）。*/
static void yk_fam_vm_spawn(uint32_t fam_id, int variant, float x, float y)
{
    yk_fam_vm_t *v = yk_fam_vm_find(fam_id);
    uint32_t id;
    int script;
    unsigned i;

    if (!fam_id) return;
    if (!v) {
        for (i = 0; i < YK_FAM_MAX; ++i) if (!s_fam_vm[i].fam_id) { v = &s_fam_vm[i]; break; }
    }
    if (!v) {   /* 表满：VM 表与使魔表同容量（每条使魔至多一条记录）⇒ 理论上到不了 */
        ce_log("youkai: familiar follow-VM table full (%d) - familiar %u stays invisible",
               (int)YK_FAM_MAX, fam_id);
        return;
    }
    script = yk_fam_anm_script(variant);
    id = ce_anm_spawn(CE_ABILITY_ANM(), script, YK_FAM_ANM_LAYER);
    if (!id) {
        ce_log("youkai: familiar follow-VM spawn FAILED (fam %u, script %d, layer %d) - the "
               "familiar entity is alive but invisible (`NoEnemyDebug` runs no ECL, so nothing "
               "else can paint it)", fam_id, script, YK_FAM_ANM_LAYER);
        return;
    }
    v->fam_id = fam_id;
    v->anm_id = id;
    v->variant = variant;
    ce_anm_set_pos(id, x, y, 0.0f);
}

/* 每帧跟随：位置 = 使魔**实体**的 `final_pos`（`CE_ENEMY_POS_X/Y` = spec §3.5 钉的那两格；
 * 世界层 VM 的实体坐标与 ECL 是同一套 640 空间 ⇒ 不需要换算，`ufo_body_vm_follow` 的注）。
 * ⚠ 句柄被引擎销毁过（过场重建 ANM 管理器 / VM 列表被清）⇒ **自愈**：丢掉句柄按同一变体重起。
 * ⚠ 脚本自己 `delete()` 了 ⇒ `ce_anm_set_pos` 返回 0 ⇒ 清句柄，下一帧自愈。*/
static void yk_fam_vm_follow(uint32_t fam_id, int variant, const uint8_t *f)
{
    yk_fam_vm_t *v;
    if (!f || !fam_id) return;
    v = yk_fam_vm_find(fam_id);
    if (!v || !v->anm_id || !ce_anm_get_vm(v->anm_id)) {
        /* 记录还在 ⇒ `spawn` 按 `fam_id` 找回**同一条**（旧句柄早就没了，直接覆盖不泄漏）；
         * 记录也没了 ⇒ 它占一条空槽。两种都按**同一个变体**重起。*/
        yk_fam_vm_spawn(fam_id, variant, *(const float *)(f + CE_ENEMY_POS_X),
                                         *(const float *)(f + CE_ENEMY_POS_Y));
        return;                                            /* spawn 里已经摆好位置 */
    }
    if (!ce_anm_set_pos(v->anm_id, *(const float *)(f + CE_ENEMY_POS_X),
                                    *(const float *)(f + CE_ENEMY_POS_Y), 0.0f))
        v->anm_id = 0;                                     /* 句柄作废 ⇒ 下一帧自愈重起 */
}

static void yk_run_action(yk_card_t *s, const yk_action_t *act)   /* s 可为 NULL（卡状态没了也要把敌人收干净）*/
{
    if (act->kind == YK_ACT_CREATE) {
        uint8_t *f = yk_spawn_familiar(act->x, act->y);
        uint32_t fid;
        if (!f) {
            if (s) ++s->fam_full;
            if (s && !s->logged_full) {
                s->logged_full = 1;
                ce_log("youkai: familiar spawn FAILED at (%.1f, %.1f) - one emitter stays "
                       "without a familiar (table full / heap refused)", act->x, act->y);
            }
            return;
        }
        fid = *(uint32_t *)(f + CE_ENEMY_ID);
        yk_fam_created(&s_map, act->enemy_id, act->slot, fid);
        yk_pin_familiar(f, act->x, act->y);
        /* ★ 壳建好的**那一刻**起跟随 VM（脚本按发射口槽选：槽 i → s53+i 那一套四色之一）。
         * ⚠ 建 VM 只能在 tick / 帧末（要分配），绝不在死亡栈上 —— 这条路径是 `on_tick_2`，合法。*/
        yk_fam_vm_spawn(fid, yk_fam_variant(act->slot), act->x, act->y);
        if (s) ++s->spawns;
    } else if (act->kind == YK_ACT_WITHDRAW) {
        uint8_t *f = yk_familiar_by_id(act->fam_id);
        yk_fam_vm_kill_id(act->fam_id);                    /* ★ 壳要撤了 ⇒ VM 必须同时收（不留幽灵使魔）*/
        if (f) yk_kill_familiar(f);
        if (s) ++s->lost;
    }
}

static void yk_run_actions(yk_card_t *s, const yk_action_t *acts, int n)
{
    int i;
    for (i = 0; i < n; ++i) yk_run_action(s, &acts[i]);
}

/* 全撤（高速 / miss / 局末 / 删卡）：使魔是**真敌人**，卡不再管它们时必须收干净
 *（METHOD 的铁律：长持续效果要交代「被删掉会怎样」—— 这里不留一只游荡的空壳）。
 * ★★ 2026-09-12：**这里同时是「发射口精确闸」的唯一复位口**（`0x433ffa` 那条钩子）。
 *   闸的判据是 `s_map` 里的 `broken` 位，而本函数经 `yk_withdraw_all` → `yk_map_reset()`
 *   把**整张表**清空（`enemy_id = 0` / `fam_id = 0` / `broken = 0`）⇒ 闸自然全部放开。
 *   ⚠ 这一点是硬要求：删卡（倒吊人 / 原版空白卡）/ 松开低速 / 局末 / 断链四条路都走这里
 *   （`on_delete`、`on_run_reset`、`on_tick_2` 的 !focused 分支、`yk_familiars_tick` 的 !em 分支）——
 *   任何一条漏掉都会让**敌人永远哑火**（钩子还在返回 1，但没人再清表）。 */
static void yk_drop_all(yk_card_t *s, const char *why)
{
    yk_action_t acts[YK_FAM_MAX];
    unsigned had = yk_map_fam_count(&s_map);
    int n = yk_withdraw_all(&s_map, acts, (int)YK_FAM_MAX);

    yk_run_actions(s, acts, n);
    /* ★ 兜底收 VM（句柄幂等）：`acts` 装不下的条目、以及 `fam_id` 已经被清掉但 VM 还挂着的残局，
     * 都靠这一支清干净 —— 删卡那一刻 tick 全停，这里就是**唯一**的机会（照 112 的 kill_all）。*/
    yk_fam_vm_kill_all();
    if (had && why) ce_log("youkai: withdrew %u familiar(s) (%s)", had, why);
    if (s) s->tr.pending = 0;
}

/* 扫全场敌人 → 1:1 映射 + 状态差分。*/
static void yk_familiars_tick(yk_card_t *s)
{
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    yk_action_t act;
    yk_action_t tail[YK_FAM_MAX];
    float off3[3], org3[3], wxy[2];
    int i, n;

    if (!em || !CE_PLAYER()) {
        yk_drop_all(s, "no world");
        return;
    }
    yk_frame_begin(&s_map);
    CE_ENEMY_FOREACH(em, e) {
        uint32_t id;
        const float *epos;
        if (!e) continue;
        id = *(uint32_t *)(e + CE_ENEMY_ID);
        if (!id) continue;
        epos = ce_enemy_pos(e);
        for (i = 0; i < YK_EMIT_SLOTS; ++i) {
            int active = yk_slot_type(e, i);
            /* 不活跃、又不是我们认识的槽 ⇒ 连表都不用碰（扫得快、条目也少）。*/
            if (!active && !yk_map_find(&s_map, id, i)) continue;
            yk_slot_offsets(e, i, off3, org3);
            yk_emit_world_pos(epos, off3, org3, wxy);
            if (yk_note_slot(&s_map, id, i, active, wxy, &act)) yk_run_action(s, &act);
        }
    }
    /* 帧末：本帧没扫到的条目 = 敌人没了（过场全清 / 静默移除）⇒ 有使魔就撤，然后退休。*/
    n = yk_frame_end(&s_map, tail, (int)YK_FAM_MAX);
    yk_run_actions(s, tail, n);
}

/* ============================ §3.7 伤害转交 1/2 ============================ */
static void yk_transfer_tick(yk_card_t *s)
{
    unsigned i;
    uint8_t *boss = yk_boss();                      /* 每帧只解析一次（链表走 4 个 id）*/
    const float *bp = boss ? ce_enemy_pos(boss) : 0;
    int32_t amount;

    for (i = 0; i < YK_FAM_MAX; ++i) {
        yk_slot_t *sl = &s_map.e[i];
        uint8_t *f;
        if (!sl->enemy_id || !sl->fam_id) continue;
        f = yk_familiar_by_id(sl->fam_id);
        if (!f) {
            /* 不走死亡事件就没了（过场 `destroy_all`）⇒ 双保险：自己松开（spec §9 实跑清单 6），
             * 并且**同时收掉跟随 VM** —— 实体没了还留 VM = 场上一个打不到的幽灵使魔。*/
            yk_fam_vm_kill_id(sl->fam_id);
            yk_fam_gone(&s_map, sl->fam_id);
            continue;
        }
        yk_pin_familiar(f, sl->x, sl->y);                              /* 位置每帧重钉 */
        /* ★ 每帧跟随 VM：位置与上面那次钉的是**同一个** `final_pos`（`CE_ENEMY_POS_X/Y`）
         * ⇒ VM 与实体永远重合（脚本是 8 帧循环、不自删；句柄自愈见 `yk_fam_vm_follow`）。*/
        yk_fam_vm_follow(sl->fam_id, yk_fam_variant((int)sl->slot), f);
        if (bp && yk_in_transfer_zone(sl->x, sl->y, bp[0], bp[1],
                                      YK_TRANSFER_BOX_W, YK_TRANSFER_BOX_H)) {
            /* 使魔就压在转嫁盒里（boss 自己的发射口常在本体上）⇒ 它这一帧掉的血可能是**我们自己的源**
             * 打的：计入转嫁会每帧自激（research §3②）。只更新采样、不喂转嫁。*/
            yk_sample_life(sl, sl->fam_id, *(int32_t *)(f + YK_ED_LIFE_CUR));
            continue;
        }
        yk_transfer_feed(&s->tr, yk_sample_life(sl, sl->fam_id, *(int32_t *)(f + YK_ED_LIFE_CUR)));
    }

    if (s->tr.pending <= 0) return;
    if (!boss) {
        /* 没有 boss（道中空场 / boss 已死）：本帧不转嫁（研究 §4）。**丢掉的量记账**，不攒着 ——
         * 攒着会在 boss 出现那一帧一次性砸下去（顶掉配额、放大计分）。*/
        s->dropped += (uint32_t)s->tr.pending;
        s->tr.pending = 0;
        return;
    }
    amount = yk_transfer_clamp(s->tr.pending, YK_TRANSFER_CAP_PER_FRAME);
    s->tr.pending = 0;
    if (amount <= 0) return;
    /* ★ 每帧**新建**一条源（一源一敌一辈子只结算一次，tag 守卫 `0x45f45a` ⇒ 复用等于只打一次）。
     * 同帧抬每敌每帧上限：自机弹贴脸打 boss 时配额已被吃满，不抬就被整份吃掉（AUDIT V20）。*/
    ce_raise_damage_cap(yk_base_damage_cap() + amount);
    ce_damage_rect(bp, 0.0f, YK_TRANSFER_SRC_LIFE, (int)amount,
                   YK_TRANSFER_BOX_W, YK_TRANSFER_BOX_H);
    s->transferred += (uint32_t)amount;
}

/* ============================== §3.3 子机 ==============================
 * 走 **SDK 的低速子机声明层**（spec §3.3.1）：本卡只声明，**只有被选举者落笔**
 *（id 最小者胜，108 < 111）。落笔 = 申请子机 + 写原生双偏移（`+0x64/+0x68` 非聚焦、`+0x6c/+0x70` 聚焦，
 * 单位 = 原生 1/128 定点）。*/
static void yk_option_declare(ce_card_t *c, yk_card_t *s, int count)
{
    if (count <= 0) {
        /* ★ 现在走的就是这条（占位）。「退出声明」= 不进选举、也**不挡住 111**
         *（否则「赢了选举却不落笔」会把 111 的子机一起闷掉）。*/
        ce_optpose_declare(ce_card_id(c), 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
        if (s && !s->logged_opt) {
            s->logged_opt = 1;
            ce_log("youkai: option pose declaration parked (count=0): youkai option anm script / "
                   "count are placeholders - the low-speed submarine chain stays out of the "
                   "election layer (spec §3.3.1) until assets land");
        }
        return;
    }
    ce_optpose_declare(ce_card_id(c), count, YK_OPTION_SCRIPT_YOUKAI,
                       YK_OPTION_OFFSET_FX, YK_OPTION_OFFSET_FY,
                       YK_OPTION_OFFSET_UX, YK_OPTION_OFFSET_UY);
}

static void yk_option_apply(ce_card_t *c, yk_card_t *s);

/* 每帧刷新声明层：只有「想声明的数量」变了才真的重发（`ce_optpose_declare` 内部要重选举，
 * 扫 255 个 id）；`force` = 刚复位过 / 池被引擎重建过 ⇒ 强制重发一次。然后交给落笔逻辑。*/
static void yk_option_refresh(ce_card_t *c, yk_card_t *s, int focused, int force)
{
    int count = yk_option_count(focused);
    if (force || s->declared != count) {
        s->declared = count;
        yk_option_declare(c, s, count);
    }
    yk_option_apply(c, s);
}

/* 收掉本卡手里所有子机。★ `ce_option_drop` 自己会**把池槽松开**（`option+0x00 = 0`，一手：
 * sdk.c 的注释与 `0x40a790` 的空位判据）⇒ 不需要再调 `ce_repopulate_options` 去回收。*/
static void yk_option_release(ce_card_t *c, yk_card_t *s)
{
    int i;
    for (i = 0; i < YK_OPTION_MAX; ++i) {
        /* ★ 删卡路径的漏洞补在这里（2026-09-12 审计）：`ce_option_drop(c, o)` 在 `o->ptr == 0`
         * 时走的是**零售单子机**那一支（`sdk.c:426`）—— 它无条件删 `card+0x1c` 那个 VM。而多子机卡里
         * 那一格只记得**最后分配**的那个（引擎每次 `Player__allocate_option` 都覆盖它，`0x40a906`）
         * ⇒ 对**空槽**调用就等于删掉**另一格**的 VM：`opt[0]` 空、`opt[1]` 在手里的组合下，
         * opt[1] 的 VM 会先被这一支删一次、再被下面 `opt[1]` 自己的那一支删一次
         *（`0x488cf0` 按 id 查不到就干净返回 ⇒ 今天只是白删一次，语义已经错了；先例是
         * 季节卡（116，同样是多子机）`cards/season.c:1497` 的 `if (s->opt[i].ptr)` 起手）。*/
        if (s->opt[i].ptr) ce_option_drop(c, &s->opt[i]);
        s->opt[i].ptr = 0;
        s->opt[i].retry = 0;
    }
    /* 兜底：所有槽都不在我们手里、但 `card+0x1c` 还挂着 VM（引擎分配过而认领失败 / 池刚被重建）
     * ⇒ 补一次零售的单子机收尾；上面已经清过那一格时它是 no-op。*/
    ce_option_drop(c, 0);
    s->opt_script = -1;
}

/* 被选举者落笔（每帧调；没赢选举就什么都不做 —— 单一写者 ⇒ 与 111 没有竞态）。*/
static void yk_option_apply(ce_card_t *c, yk_card_t *s)
{
    const ce_option_pose_t *pose = ce_optpose_pose();
    int want, i;
    uint8_t *o;

    if (ce_optpose_winner() != (int)ce_card_id(c) || !pose || pose->count <= 0) {
        if (s->opt_script >= 0) yk_option_release(c, s);   /* 手里还攥着子机（高速 / 被别人覆盖）⇒ 还回去 */
        return;
    }
    if (yk_option_script_changed(pose->anm_script, (int)s->opt_script)) {
        /* 子机的 anm 脚本是**申请时**定死的（`Player__allocate_option` `0x40a790` 第 5 个栈参）
         * ⇒ 换套必须重申请（先收再要；槽由 `ce_option_drop` 松开）。
         * ⚠ 未实跑（只在资产到位、脚本真的会变时才会走到）。*/
        yk_option_release(c, s);
    }
    want = pose->count;
    if (want > YK_OPTION_MAX) want = YK_OPTION_MAX;
    for (i = 0; i < want; ++i) {
        o = ce_option_get(c, &s->opt[i], YK_OPTION_OFFSET_ALLOC, pose->anm_script,
                          YK_OPTION_RETRY, "youkai");
        if (!o) continue;
        s->opt_script = pose->anm_script;
        /* 写阵型（零售 `0x40b8c1` 也这么自己摆；单位 = 原生 1/128 定点）*/
        *(int32_t *)(o + CE_OPT_OFF_NF_X) = (int32_t)(pose->ux * YK_SUBPIXEL);
        *(int32_t *)(o + CE_OPT_OFF_NF_Y) = (int32_t)(pose->uy * YK_SUBPIXEL);
        *(int32_t *)(o + CE_OPT_OFF_F_X)  = (int32_t)(pose->fx * YK_SUBPIXEL);
        *(int32_t *)(o + CE_OPT_OFF_F_Y)  = (int32_t)(pose->fy * YK_SUBPIXEL);
    }
}

/* 妖侧 shooterset 装上没有：读那一格记录的首字节 `fire_rate`（i8，`engine/sht/th18/02` §1）。
 * 我们的记录 = TH08 `ply03as.sht` 满 P 档 9 行，`fire_rate ∈ {3,5,6,10}`（`assets/sht/append_shooterset.py`
 * 的 `REPLACE` 覆盖零售那格 0x16）；零售原值 / build/sht 没重跑时那格还是 `0`
 * ⇒ `yk_shotset_usable` 返回 0（装备卡路径 `0x40aa4f` 的 `idiv` 没有零分支）。
 * `< 0` = 终止符 ⇒ 也不可用。**运行期自检**，不靠一个手写的「资产已装」开关。*/
static int yk_youkai_shotset_installed(void)
{
    uint8_t *p = CE_PLAYER();
    uint8_t *sht;
    uint8_t *row;
    if (!p) return 0;
    sht = *(uint8_t **)(p + CE_PLAYER_SHT_PTR);
    if (!sht) return 0;
    row = *(uint8_t **)(sht + YK_SHT_SET_ROW + (size_t)YK_SHOT_SET_YOUKAI * 4);
    return row && yk_shotset_usable((int)*(const int8_t *)row);
}

/* ============================== 回调 ============================== */
/* +0x00：卡对象可能在局中被重建（每关开始引擎对卡组每张卡再调一次 ctor）⇒ 静态表在这里复位。*/
static int on_ctor(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    if (!s) return 0;
    /* ★ 先收跟随 VM 再复位静态表：本函数**在局中被重建时也会跑**（每关开始引擎对卡组每张卡
     * 再调一次 ctor）—— 那时 `yk_map_reset` 会把使魔一并忘掉，它们身上的 VM 若不复位就永久留下
     * （`s_fam_vm` 是文件级静态，不是卡状态）。句柄为空时是纯 no-op。*/
    yk_fam_vm_kill_all();
    yk_map_reset(&s_map);
    yk_transfer_reset(&s->tr);
    __builtin_memset(s->opt, 0, sizeof s->opt);
    s->opt_script = -1;
    s->declared = -1;                                  /* 强制下一帧重发一次声明 */
    s->shot_timer = 0;
    s->spawns = s->broken = s->lost = s->transferred = s->dropped = s->fam_full = 0;
    s->logged_opt = s->logged_sht = s->logged_noshot = s->logged_full = s->logged_gate = 0;
    return 0;
}

static int on_load(ce_card_t *c) { return on_ctor(c); }

static void on_run_reset(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    if (s && (s->spawns || s->transferred))
        ce_log("youkai: run done - %u familiars spawned, %u killed, %u withdrew, "
               "%u transfer damage, %u dropped (no boss)",
               s->spawns, s->broken, s->lost, s->transferred, s->dropped);
    yk_drop_all(s, 0);                                   /* 局末：不留使魔 */
    if (s) yk_option_release(c, s);
    on_ctor(c);
    /* 子机声明层的「退出」由 SDK 在 `+0x4c` 桩里自动做（`ce_layer_drop_card`），卡不用管。*/
}

/* +0x50：局中被删（倒吊人 / 原版空白卡「弃掉整副卡组」，商店里 AbilityManager 冻着、
 * tick 回调全停）—— 只有这里收得到。使魔是**真敌人**，必须在这里交代清楚。*/
static void on_delete(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    ce_log("youkai: card deleted - withdrawing familiars "
           "(%u spawned, %u killed, %u transfer damage this run)",
           s ? s->spawns : 0, s ? s->broken : 0, s ? s->transferred : 0);
    yk_drop_all(s, 0);
    if (s) yk_option_release(c, s);                      /* 私有状态随后由 SDK 释放 */
}

/* +0x48（帧末）：抢写本体 sprite。★ 全仓无先例（114–117 都没改过自机本体贴图）⇒ AUDIT 单列一条。
 * 本体脚本每帧写 `vm+0x24`，所以必须**每帧**在绘制前抢写；写的是 VM 字段（不是脚本），
 * 不动 `+0x28 instr_offset`（换脚本 = 重启、丢状态，spec §3.2 明确不做）。
 *
 * ★★ 2026-09-12 一手订正：**不能直写 `p + CE_PLAYER_VM + CE_ANM_VM_SPRITE_ID`**。正确形状 =
 *   `AnmLoaded__set_sprite`（`0x477b00`；thiscall(anm; vm, sprite_idx) ret 8，engine.h 的
 *   `CE_FN_ANM_SET_SPRITE`），一手逐条：
 *     ① `477b24: mov [edx+0x24],eax` **只是第一句**：它接着用 `esi+0x11c`（anm 的 sprite 表，
 *         stride 0x44）把 UV（`vm+0x3ac/0x3b0/0x3b4/0x3b8` 与 `+0x3c0..+0x3c8` 两组）、
 *        尺寸基准（`vm+0x74/+0x78`）与 scale 反比（`vm+0x454/+0x468`）一起算出来并写回
 *        （`477b2e`–`477d1a`）⇒ **直写会让这些缓存停在上一张图上**。
 *     ② 而**画的时候**贴图取的不是 `vm+0x24` 之外的东西，是 **VM 自己那一页的 anm**：
 *        `0x4810d0`（AnmVm 的绘制）`481117: mov eax,[esi+0x20]`（页号）→
 *        `481129: mov edx,[0x51f65c+eax*4+0x312072c]`（该页 = AnmLoaded*）→
 *        `481130: mov eax,[edx+0x11c]`（它的 sprite 表）→ `481136: mov ecx,[eax+ecx*4+0x8]`
 *        （texture id）→ SetTexture（`481171 call [esi+0x104]`）。
 *   ⇒ 传的 `anm` 必须是 **VM 的页所指向的那一份**：自机本体就是 `player+0xc`（pl0X.anm）。
 *     ⚠ **不是** `player+0x10`：那一格开火期间被引擎换成 ability.anm（engine.h 的注）⇒
 *       传错 = UV 从一个 anm 取、贴图从另一个 anm 取，画出来是错的。
 *   先例：`season.c:996-1003`（起残影 VM 后读本体 sprite 号，再 `ce_anm_set_sprite(anm, id, sprite)`，
 *   传的 anm 就是 `player+0xc`）。
 *   ⚠ sdk.h 的 `ce_anm_set_sprite(anm, id, sprite)` 收的是**世界列表的 anm id**（内部走
 *     `ce_anm_get_vm(id)`），而本体 VM **没有 id**（它是 `player+0x14` 里的内嵌 zAnmVm）
 *     ⇒ 这里直接调 `0x477b00`，复用 sdk.h 的 `ce_fn_anm_set_sprite_t` 签名（不改共享件）。
 *   🟡 今天这一支**一次都不会执行**：`YK_BODY_SPRITE_FOCUSED = -1`（妖侧本体资产未接，占位）
 *     ⇒ 连旧的直写也从来没生效过（`if (sprite >= 0)` 恒假）。见报告。*/
static int on_draw(ce_card_t *c)
{
    uint8_t *p = CE_PLAYER();
    void *anm;
    int sprite;
    (void)c;
    if (!p) return 0;
    sprite = yk_body_sprite(CE_PLAYER_FOCUSED());
    if (sprite < 0) return 0;                             /* 占位期走这条（高速恒走这条，是设计）*/
    anm = *(void **)(p + CE_PLAYER_ANM);
    if (!anm) return 0;
    ((ce_fn_anm_set_sprite_t)CE_FN_ANM_SET_SPRITE)(anm, p + CE_PLAYER_VM, sprite);
    return 0;
}

/* +0x18：开局 / 火力档变 / 关店后广播 —— 池刚被重建过（`Player__repopulate_options` `0x45d5e0`）
 * ⇒ 必须在这里**再申请一次**（零售装备卡同款；spec §3.3 的「火力档变化时重申请」）。*/
static int on_power_level_change(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    int i;
    if (!s) return 0;
    /* 池刚被 `Player__repopulate_options_and_notify_cards` `0x45d5e0` 整池重建：
     * 手里的槽指针全作废、重试倒计时也要清掉（否则要等 60 帧才补申请）。
     * 照零售 / 77：不 drop（引擎已经重建过），直接清指针再去要一个。*/
    for (i = 0; i < YK_OPTION_MAX; ++i) { s->opt[i].ptr = 0; s->opt[i].retry = 0; }
    s->opt_script = -1;
    yk_option_refresh(c, s, CE_PLAYER_FOCUSED(), 1);
    return 0;
}

/* +0x1c：只在射击帧广播（`0x45ecb8` 卡广播）。低速出妖侧那一套 shooterset。
 * ⚠ 两条前置（都是一手）：
 *   ① 膛口 = 子机：`0x40a9c0` **不读记录里的 `opt_slot`**，膛口恒取 `option+0x5c/0x60`
 *      （`0x40aa0c`／`0x40aa21`，整组只算一次）；`option == NULL` 时它 `test edx,edx / je`
 *      直接返回 ⇒ 没有子机就不发弹 —— 所以本卡现在**还发不出妖侧弹**：
 *      `YK_OPTION_COUNT_FOCUSED`（占位 0）没定之前 `yk_option_wanted()` 恒 0。
 *   ② 那一格必须是**我们写的合法记录**：零售 0x16 是 `fire_rate = 0`，装备卡路径会整数除零
 *      （`engine/sht/th18/02` §2）⇒ 自检记录（`yk_shotset_usable`），没覆盖成功就不调。
 *      覆盖由 `assets/sht/append_shooterset.py` 的 `REPLACE` 做（`make sht`）。*/
static int on_tick_shooters(ce_card_t *c, uint32_t short_timer, uint32_t long_timer)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    uint8_t *o;
    (void)short_timer;
    (void)long_timer;
    if (!s || !CE_PLAYER()) return 0;
    if (!yk_option_wanted(CE_PLAYER_FOCUSED())) return 0;     /* 高速 = 原机体（原生弹照发）*/
    if (!yk_youkai_shotset_installed()) {
        if (!s->logged_sht) {
            s->logged_sht = 1;
            ce_log("youkai: youkai shooterset 0x%02x not installed (record fire_rate is 0) - no "
                   "youkai bullets until `make sht` covers that retail slot "
                   "(calling it would divide by zero, engine/sht/th18/02 §2)",
                   YK_SHOT_SET_YOUKAI);
        }
        return 0;
    }
    o = ce_option_claim(c, &s->opt[0]);                       /* 膛口 = 子机（0x40a9c0 一手）*/
    if (!o) return 0;
    ++s->shot_timer;                                          /* 射击帧计数自己数（引擎那个松开 Z 会跳）*/
    ce_tick_shooters_for_card(o, (int)s->shot_timer, 0, YK_SHOT_SET_YOUKAI);
    return 0;
}

/* +0x28：按 `bullet+0xac` 的打包 set id 认出妖侧弹 ⇒ 数值随 P（spec §3.4）。
 * ⚠ 走这里而不是伤害层（`bullet_damage_permille`）：那一层作用于**所有**自机弹（会连带人侧）。*/
static int on_bullet_created(ce_card_t *c, void *bullet)
{
    uint32_t packed;
    int32_t out = 0;
    (void)c;
    if (!bullet) return 0;
    packed = *(uint32_t *)((uint8_t *)bullet + YK_PB_PACKED_ID);
    if (yk_bullet_damage_apply(*(int32_t *)((uint8_t *)bullet + CE_BULLET_DAMAGE), packed,
                               CE_CURRENT_POWER(), &out))
        *(int32_t *)((uint8_t *)bullet + CE_BULLET_DAMAGE) = out;
    return 0;
}

/* ★ 核心：止射闸（spec §3.1）。返回非 0 ⇒ SDK 让本帧跳过原生行循环（`retaddr = 0x45eca0`），
 * `0x45eca0` 起的**卡广播照跑**（77 / 116 的卡子机不受影响）；117 咆哮生效时 SDK 根本不问这一条
 *（sdk.c：先问 `on_shot_set_index`，有卡覆盖了就不抑制）。*/
static int on_native_shot_suppress(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    const char *why;
    if (!yk_suppress_native_shot(CE_PLAYER() != 0, CE_PLAYER_FOCUSED())) return 0;
    /* ⚠ 这对是**一体**的：闸把原生弹压掉了，而妖侧弹要**先有子机**（`0x40a9c0` 的膛口，
     * 见 `on_tick_shooters` 前置①）⇒ 下面两种情况下低速**一发弹都没有**。
     * 第一次命中时把原因记下来（实跑诊断用），但不改行为
     *（行为 = spec §3.1 的原文：只看「有自机 + 聚焦」）。*/
    if (!s || s->logged_noshot) return 1;
    if (!yk_youkai_shotset_installed()) {
        why = "youkai shooterset is NOT installed (run `make sht`)";
    } else if (!yk_option_wanted(1)) {
        why = "there is no option to fire from (YK_OPTION_COUNT_FOCUSED is still 0, spec §3.3)";
    } else {
        return 1;
    }
    s->logged_noshot = 1;
    ce_log("youkai: native shots suppressed at focus but %s -> NO bullets at all "
           "(spec §3.1 + §3.4.1)", why);
    return 1;
}

/* ★★ §3.5.1 发射口**精确关闸**（2026-09-12 新 SDK 面；见 `youkai_core.h` 与 `sdk.c` 的 `0x433ffa`）。
 *
 * 断点 `0x433ffa` = `etOn` 里「发弹三步」的汇聚点（一手：`433ffa movss xmm1,[edi+0x5144]` →
 * `434008/434028 call 0x42cca0` → `434013 lea eax,[esi+0x598]` → `434019 push eax` →
 * `43401a call 0x427810`(fire) → `43402d jmp 0x436417`(opcode 收尾)）。
 * SDK 把 `slot` 从 `[ebp-0x56c]` 取出（`433fcf: imul esi,[ebp-0x56c],0x488` 一手核过），
 * 并用 `regs->esi == &data + slot*0x488` 自检。返回非 0 ⇒ `retaddr = 0x43402d` ⇒ 本帧这一槽不发射。
 *
 * **本卡判据**（`yk_shot_slot_suppressed`，纯逻辑在 core、单测覆盖）：这个 `(enemy_id, slot)`
 * **曾经有过我们的使魔、现在没了** ⇒ 掐掉本帧发射。判据只用**我们自己记的槽表**，不读 `props[].type`
 * （⇒ 不受 `etOn` / `etSprite` 谁先写 type 这个未定量的影响），也不按指针找使魔
 * （`ev->enemy` 是**敌人本体**，使魔是我们自建的另一个敌人 ⇒ 用 `ev->enemy_id` 核对）。
 *
 * **不会吞正常发射**的三道保险（对应 core 注释里的三分）：
 *   ① 槽表里没有这个 `(enemy_id, slot)` ⇒ 放行（我们从没管过这个发射口）；
 *   ② 表里有、`fam_id` 已回填且活着 ⇒ 放行；
 *   ③ 表里有、还没建使魔（`broken == 0`，建被排到下一个 `on_tick_2`）⇒ 放行。
 * 只有「建过又没了」（`broken == 1`）才掐 —— 那一刻正是 spec §9.6「打掉使魔 ⇒ 那个发射口停火」的语义。
 *
 * ⚠ 与旧近似的分工：**新闸是精确的**（使魔一没，那一槽**当帧**就不发）；旧的
 *   「不重建 + 转嫁归零」（`yk_fam_broken` 里的注释）保留为**兜底** —— 即使本钩子因为
 *   表满 / 条目退休等原因没掐到，也不会重建使魔、也不会继续转嫁。
 *
 * ⚠ 一个 `enemy_id` 可能被**多张卡**同时挂钩（SDK 是 OR：任一返回非 0 即掐）——本卡只对
 *   自己使魔表里的槽表态，别的槽一律放行。 */
static int yk_on_shot_slot(ce_card_t *c, const ce_enemy_shot_t *ev)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    uint32_t have = 0;

    if (!s || !ev || !ev->enemy_id) return 0;
    if (ev->slot < 0 || ev->slot >= YK_EMIT_SLOTS) return 0;
    /* 按 id 回查**当前活着的**使魔（0 = 没了）。★ 不碰 `ev->enemy`：那是敌人本体、不是使魔，
     * 而且只在本次回调内有效（同帧 sweep 可能删它）。 */
    have = yk_slot_fam_id(&s_map, ev->enemy_id, ev->slot);
    if (!yk_shot_slot_suppressed(&s_map, ev->enemy_id, ev->slot, have)) return 0;
    if (s && !s->logged_gate) {
        s->logged_gate = 1;
        ce_log("youkai: emitter gate ACTIVE - slot %d of enemy_id %u stays silent until ECL re-arms it",
               ev->slot, ev->enemy_id);
    }
    return 1;
}

/* ★ §3.6 击破信号：共享死亡事件是**广播只读**（EVENTS-CONTRACT §1）——
 * 这里只做一件事：认出「这只被打死的是我的使魔」⇒ 把那个发射口槽标成「被打破」。
 * ⚠ 回调里禁止析构任何敌人（UAF），也不缓存 `ev->enemy`（同帧 sweep 会删）。*/
static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    if (!s || !ev) return;
    /* 「被打死」= KILLED（`retaddr 0x42f3b7`，life ≤ 0 主路 —— SDK 已按一手研究实现成
     * `CE_EDEATH_KILLED`；⚠ spec §3.6 正文里写的 `0x42ee90` 是**炸弹秒杀**路，研究 §2.4 已勘误）。
     *
     * ★★ **为什么本卡把 `KILL_ALL` 也算进来（与 109/110/112 的做法相反）—— 2026-09-12 口径裁决**：
     *   契约的分界线是「**这个 kind 会不会被换成玩家的收益**」，不是「算不算死」：
     *     · 给奖励 / 加计数 / 得分 / 掉落（109 爆风、110 大信仰点、112 计数）⇒ **必须排除 `KILL_ALL`**
     *       （清场会把计数刷满；而且 `0x4307f1` / `0x430a00` / `0x430c08` 那三路在调 `0x430240`
     *        **之前**已经 `memset(enemy+0x6240)` 并清了主掉落 ⇒ 本来就不该算击破）。
     *     · **纯状态判断**（本卡：这一槽的使魔没了 ⇒ 标 `broken`、不再重建，等 ECL 下一波
     *       `etNew(i)`+`etOn(i)` 重新武装时自然复位）⇒ **把 `KILL_ALL` 算进去是对的**：
     *       清场时使魔确实从场上消失了；若不算，`broken` 不置 ⇒ 下一帧又凭 1:1 映射把它建回来，
     *       卡的状态就与场上矛盾了。
     *   炸弹秒杀（`BOMB_KILL`）同理：使魔真的没了 ⇒ 一并打破。
     *   ECL_DIE / OTHER 对我们自造的惰性空壳（无 ECL、无符卡）不可能发生，故不列。 */
    if (ev->kind != CE_EDEATH_KILLED && ev->kind != CE_EDEATH_BOMB_KILL &&
        ev->kind != CE_EDEATH_KILL_ALL)
        return;
    if (!yk_fam_broken(&s_map, ev->id)) return;           /* 不是我们的使魔（道中妖精 / boss 死了）*/
    /* ★ 壳体已经被引擎析构了 ⇒ 跟随 VM 也必须同时收，否则会留下一个**画着已死使魔的幽灵**
     *（打不到、不消失）。这里是**共享死亡断点的死亡栈**上：删 VM 只是给它标删
     *（`0x488cf0` 置 `vm+0x538` 位 7，真正的析构由 AnmManager 自己的 update 走）⇒ 与
     * `ufo.c` 的 `ufo_tok_kill` 同一条纪律、同一个 🟡（「从死亡断点直接进 AnmManager」未实跑验证）。*/
    yk_fam_vm_kill_id(ev->id);
    ++s->broken;
    ce_log("youkai: familiar enemy_id %u killed (kind %u, real=%u) at (%.0f, %.0f) - "
           "that emitter slot stays without a familiar until ECL re-arms it "
           "(real emitter shutdown needs hook 0x434019, TODO)",
           ev->id, ev->kind, (ev->flags & CE_EDEATH_F_REAL) ? 1u : 0u, ev->x, ev->y);
}

/* +0x2c：AbilityManager tick（优先级 0x16），早于敌人结算（0x1b）⇒ 上限只对当帧有效、不用还原。*/
static int on_tick_2(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    int focused;
    if (!s) return 0;
    if (!CE_PLAYER()) {
        yk_drop_all(s, "no player");
        return 0;
    }
    focused = CE_PLAYER_FOCUSED();
    yk_option_refresh(c, s, focused, 0);                  /* 声明（变了才发）+ 落笔（只有被选举者动手）*/
    if (!focused) {
        /* 高速 = 原机体：使魔全部撤（spec §9 实跑清单 1「松开全部还原」）。
         * 常态下这条只是两次比较。*/
        if (yk_map_fam_count(&s_map)) yk_drop_all(s, "unfocused");
        return 0;
    }
    yk_familiars_tick(s);
    yk_transfer_tick(s);
    return 0;
}

CE_CARD(108, .ctor = on_ctor, .on_load = on_load, .on_power_level_change = on_power_level_change,
             .on_tick_shooters = on_tick_shooters, .on_tick_2 = on_tick_2,
             .on_bullet_created = on_bullet_created, .on_enemy_death = on_enemy_death,
             .on_enemy_shot_slot = yk_on_shot_slot,           /* §3.5.1 发射口精确闸（0x433ffa）*/
             .on_native_shot_suppress = on_native_shot_suppress, .on_draw = on_draw,
             .on_run_reset = on_run_reset, .on_delete = on_delete);

/* ============================== 编译期对账 ============================== */
/* 卡的私有状态必须塞进 `CE_STATE_BYTES`(512) − `CE_STATE_RESERVED`(16)（sdk_core.h）。
 * 使魔表在文件级静态数组里，所以这里只查状态结构。*/
_Static_assert(sizeof(yk_card_t) <= CE_STATE_BYTES - CE_STATE_RESERVED,
               "youkai card private state exceeds the SDK budget");
/* 跟随 VM 的静态表：32 × 12 B = 384 B（文件级静态，不进卡的 496 B 预算）。
 * 钉住记录的大小是为了让「加字段 = 表变大」这件事**在编译期**被看见（12 = 3 × u32/i32，无 padding）。*/
_Static_assert(sizeof(yk_fam_vm_t) == 12, "familiar follow-VM record grew (table is 32 of these)");

/* ============================== ★ 占位 / TODO 清单 ==============================
 * 每一条都「不自己发明数值」，取值一律是**不做**这个语义：
 *   §5 资产
 *     1. `YK_BODY_SPRITE_FOCUSED` = -1        —— 妖侧本体 sprite 号（射命丸文 / TH16 pl03.anm 未接进 pl0X.anm）
 *     2. ~~`YK_SHOT_SET_YOUKAI` = 0x16 + 档位表~~ —— ✅ 2026-09-12 **已接真数据**：
 *        shooterset = TH08 `ply03as.sht` 满 P 档 9 行（`assets/sht/append_shooterset.py` 的 `REPLACE`
 *        覆盖零售那格，`fire_rate ∈ {3,5,6,10}`，除零风险随之消失）；**档位伤害表仍不填**
 *        （原因见 `youkai_core.c` §3.4 的注释：TH08 单发伤害随 P 下降，是配着路数设计的）。
 *        ⚠ 但它现在**还发不出来**：`0x40a9c0` 的膛口恒取子机 ⇒ 要等第 3 条的子机数量定案。
 *     3. `YK_OPTION_SCRIPT_YOUKAI` + `YK_OPTION_COUNT_FOCUSED` = 0 —— 妖侧子机脚本（借 102 占位）与数量
 *        （数量没给、且声明层只有一套偏移 ⇒ 现在整条子机链**不参与声明**）
 *     4. `YK_FAM_ANM_INDEX` / `YK_FAM_ANM_SCRIPT` = -1 —— 使魔贴图（借零售 or 复用 116/117 的做法）
 *        → ✅ 2026-09-12 **资产已到**：四色零售切图 = `ability.anm` script 206..209
 *          （TH08 `enemy.anm` script53..56，红/绿/蓝白/黄），由**跟随 VM** 画
 *          （`YK_FAM_ANM_LAYER` 那一节；槽 i → s53+i）。上面那两个偏移保持占位（仍 ❓ 未证实）。
 *        🟡 未实跑：层号 7 与「VM 位置 = 实体 final_pos」都只在代码层对账过。
 *   §10 决策
 *     5. `YK_FAM_LIFE` = 1（使魔 HP；硬约束 < 1000）
 *     6. `YK_TRANSFER_CAP_PER_FRAME` = 0（单帧转嫁上限：0 = 无上限；研究 §7.5 提醒会顶掉 boss 配额）
 *     7. `YK_FAM_MAX` = 32（分配护栏；spec 没给上限）
 *     8. `YK_OPTION_OFFSET_*` = 0（妖侧子机阵型；`ce_option_alloc` 的 off 参数是 x 还是 y 两处文档不一致）
 *   §3.5 缺断点
 *     9. `0x434019`（`etOn` 里 `push eax` 前）没装 —— 「打死使魔 ⇒ 真的关掉那个发射口」只能近似
 *        （不重建 + 转嫁归零），发射口**本身**还在开火
 *   🟡 待实跑
 *    10. 文件头的 `abs_pos` 双写（spec §3.5 只写 final_pos）
 *    11. `YK_TRANSFER_BOX_W/H` = 16×16、`YK_FAM_HURTBOX_HALF` = 12（默认 hurtbox 24.0 的一半）
 *    12. 止射闸（A 路线 = `retaddr` 改控制流）本身未实跑；与 117 咆哮 / 77 / 116 同装的验收见 spec §9
 *    13. 「聚焦态变了 ⇒ 先收子机再重申请」（脚本是申请时定死的）只在资产到位、脚本真的会变时
 *        才走到 —— 未实跑。★ 附带一手：`ce_option_drop` 现在**自己会把池槽松开**（`option+0x00 = 0`，
 *        见 sdk.c 的实现注释 + `0x40a790` 的空位判据）⇒ **不需要** `ce_repopulate_options()` 回收
 *   ❌ 已否决（写进代码是为了不再走一遍）
 *    14. `enemy+0x6320`（spec §3.9 的「第二杠杆」）= `invuln_timer.current`（研究 §2.1 一手）
 *        ⇒ 设正值 = 使魔无敌，与「使魔必须能被打」直接冲突 ⇒ **不采用**
 *    15. 直接写 `enemy+0x6220`（life）转嫁 —— 跳过 12 件事（符卡 ×7 记账 / 每帧上限 / 死亡判定本身），
 *        且会打乱死亡事件的 retaddr 分类（研究 §2.2）⇒ **不采用**
 *    16. `on_shot_damage` 当转嫁触发器 —— 它有 `CE_PLAYER_FOCUSED()` 门，妖机体期间一次都不来（研究 §0）*/
