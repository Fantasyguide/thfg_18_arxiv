/* 杰拉（Jera，id 88）—— **主动卡**，致敬《以撒的结合》的符文「Jera」（Pickup ID 33，拾取语 "Abundance"）。
 *
 * 规则（owner 2026-10-02 逐字）：「使用后复制所有掉落物，包括但不限于永点、P点、金钱、季节道具、
 * 灵力道具、残碎B碎、112压缩包等等（后者涉及到卡间协同作用）」。
 * 原版对位：以撒的 Jera 符文 = 「Duplicates all pickups and chests in the room」（一手：官方 wiki）。
 *
 * ## 复制面（三段，各走各的通道）
 *
 * ① **引擎道具池**（本体能直接够到）：P 点 / 金钱 / 命碎 / 符卡碎 / 卡道具（0x10–0x13）/
 *    消弹产生的小分道具（type 9–0xe）/ 别的卡用 `ce_spawn_item()` 撒的一切（112 的整颗 B 与奖励、
 *    111 的命碎、114 的碎片卡…）。做法 = 扫全池（**两条 freelist 连续排布，共 4696 槽**，
 *    `CE_ITEM_POOL_OFF` + `CE_ITEM_SLOTS` 一次扫完），先给活槽打一个标记，再逐个 `spawn` 复制品 ——
 *    **先打标后生成**才不会把自己刚撒的复制品再复制一遍（`+0xc8c` 是引擎全 exe 零引用的字段，
 *    `spawn` 会把它清 0，天然不递归）。
 * ② **共享信仰点池**（107 樱点 / 110-111 信仰点·绿点）：走 `faith.h` 的公开 API ——
 *    `faith_absorb_list()` 快照 + `faith_spawn()` 原档重撒（tier / slot_mask / value 一起带回去，
 *    外观由 slot_mask 决定 ⇒ 樱点复制出来还是樱点）。
 * ③ **各卡私有池**（108 刻符 / 109 灵力道具 / 112 压缩包 / 116 季节道具 / …）：走 SDK 的
 *    **复制代数闸**（`ce_dup_request()` + `ce_dup_generation()`，见 `sdk_core.h` 末节）——
 *    那些池是各卡 `.c` 里的 static，只有那张卡自己够得着；它们在**自己的 tick** 里看到代数变了，
 *    就把自己场上那批各复制一份，并每帧 `ce_dup_publish()` 上报可见数（Jera 用它判「值不值得发动」）。
 *
 * ## 语义细节（一手 / 实跑口径）
 * · **同帧稍后就生效**：`on_activate` 在 Player tick（优先级 0x17），ItemManager tick 在 0x1e
 *   ⇒ 复制品当帧就会被 tick（过回收线 / V 门 / 30px 直收）——「复制出来就立即全屏吸收」正是这条。
 * · **源在真空飞行（state 3）⇒ 复制品照抄 state + 磁速**（type 9–0xe 除外：它们靠 state 5→2→3 的
 *   原生链，强写 3 会让 VM 没建出来）。
 * · **一次性**：绝不做「每帧复制」（池会几何翻倍）。
 * · 池满：`spawn_item` 静默返回 0 ⇒ 我们计数并在日志里报 `made/failed`。
 * · 不建任何跨帧资源（无 VM / 池 / HUD）⇒ `on_delete` 不需要收东西（SDK 自动退代数闸台账）。
 *
 * 数值（充能 / 上限 / 是否散开）全是 ❓ 自定，登记在 `TUNING.md` §2.10。 */
#include <stddef.h>

#include "sdk.h"
#include "faith.h"

#define JERA_RECHARGE     1800u      /* ❓自定：30 s（对位「一次性消耗品」，与反转牌/神之宣告同族但更短）*/
#define JERA_MAX_COPY      512       /* ❓自定：单次复制的上限（防一帧几百颗的帧耗；池 600+4096）*/
#define JERA_FAITH_MAX     320       /* = FAITH_POOL_TOTAL（共享点池的实体上限）*/
#define JERA_SCATTER       12.0f     /* ❓自定：复制品在原位附近的散开半径（px）；0 = 原位重叠 */
#define JERA_MARK          0x4a4552u /* "JER"：写进 `zItem+0xc8c`（spawn 清零、全 exe 零引用）*/
#define JERA_ITEM_MARK     0xc8c     /* 上面那个标记位（engine.h 没有宏；一手见 AUDIT §BE 详录 #212）*/

/* 静态快照：ce_state 只有 496 字节，装不下 320 条信仰点快照 ⇒ 文件级 static（本卡只会在主线程 tick）。*/
static faith_absorb_t s_jera_pts[JERA_FAITH_MAX];

/* 复制一条引擎道具。`it` = 源槽；返回 1 = 撒成功。 */
static int jera_dup_one(uint8_t *it)
{
    uint8_t *cp;
    uint8_t *pl = CE_PLAYER();
    float pos[3];
    int32_t type = *(int32_t *)(it + CE_ITEM_TYPE);
    int32_t st = *(int32_t *)(it + CE_ITEM_STATE);
    float mag = *(float *)(it + CE_ITEM_MAG_SPEED);
    uint32_t r = ce_rand();

    pos[0] = *(float *)(it + CE_ITEM_POS);
    pos[1] = *(float *)(it + CE_ITEM_POS + 4);
    pos[2] = *(float *)(it + CE_ITEM_POS + 8);
    if (JERA_SCATTER > 0.0f) {                       /* 散开一点，别整整齐齐叠成一摞（replay-safe RNG）*/
        pos[0] += ((float)(int32_t)(r & 0xffu) - 128.0f) * (JERA_SCATTER / 128.0f);
        pos[1] += ((float)(int32_t)((r >> 8) & 0xffu) - 128.0f) * (JERA_SCATTER / 128.0f);
    }
    /* 角度 −π/2（向上）、速度 2.2 = 原生 main-drop 实参；timer 0 = 「出生即受物理」（见文件头）*/
    cp = ((ce_fn_spawn_item_t)CE_FN_SPAWN_ITEM)(CE_ITEM_MANAGER(), (uint32_t)type, pos, 0,
                                                -1.5707964f, 2.2f, 0, 0, 0);
    if (!cp) return 0;
    if (st == CE_ITEM_STATE_VACUUM && type < 9) {     /* 源已在飞向自机 ⇒ 复制品照抄（9–0xe 例外，见文件头）*/
        *(int32_t *)(cp + CE_ITEM_STATE) = CE_ITEM_STATE_VACUUM;
        *(float *)(cp + CE_ITEM_MAG_SPEED) =
            (mag > 0.0f) ? mag
                         : (pl ? *(float *)(pl + CE_PLAYER_ITEM_ATTRACT_SPD) : 0.0f);
    }
    return 1;
}

static int on_activate(ce_card_t *c)
{
    uint8_t *mgr = CE_ITEM_MANAGER();
    unsigned i, n_src = 0, made = 0, failed = 0, skipped = 0;
    int n_pts, n_pts_made = 0;
    int vis;

    (void)c;
    if (!mgr) return ce_refuse("jera", "no item manager");

    /* ① 先只打标（不生成）：所有 refuse 路径都必须在任何副作用之前。*/
    for (i = 0; i < CE_ITEM_SLOTS; ++i) {
        uint8_t *it = mgr + CE_ITEM_POOL_OFF + (size_t)i * CE_ITEM_STRIDE;
        if (*(int32_t *)(it + CE_ITEM_STATE) == 0) continue;
        *(uint32_t *)(it + JERA_ITEM_MARK) = JERA_MARK;
        ++n_src;
    }
    /* ② 共享信仰点池的快照（必须在生成之前取，否则会枚举到自己的复制品）。*/
    n_pts = faith_absorb_list(s_jera_pts, JERA_FAITH_MAX);

    vis = ce_dup_vis_total();
    if (!n_src && n_pts <= 0 && vis <= 0 && ce_dup_producers() <= 0)
        return ce_refuse("jera", "nothing to duplicate");

    /* ③ 复制引擎道具池里打过标的那批（复制品的标记被 spawn 清 0 ⇒ 不会递归）。
     * ⚠ **清标那一步不受上限约束**：到 `JERA_MAX_COPY` 之后就不再复制，但**必须继续把整池的标清完** ——
     *   留着残标 = 下一次发动会把「那时候早已不在的槽」当源（type/坐标都是垃圾）。*/
    for (i = 0; i < CE_ITEM_SLOTS; ++i) {
        uint8_t *it = mgr + CE_ITEM_POOL_OFF + (size_t)i * CE_ITEM_STRIDE;
        if (*(uint32_t *)(it + JERA_ITEM_MARK) != JERA_MARK) continue;
        if (made + failed < JERA_MAX_COPY) {
            if (jera_dup_one(it)) ++made; else ++failed;
        } else {
            ++skipped;
        }
        *(uint32_t *)(it + JERA_ITEM_MARK) = 0;        /* 每一条路径都要清标（失败 / 超上限也清）*/
    }
    /* ④ 共享信仰点：原档 / 原槽 / 原值重撒。*/
    for (i = 0; i < (unsigned)n_pts; ++i)
        if (faith_spawn(s_jera_pts[i].x, s_jera_pts[i].y, (faith_tier_t)s_jera_pts[i].tier,
                        s_jera_pts[i].slot_mask, s_jera_pts[i].value))
            ++n_pts_made;
    /* ⑤ 把代数闸推一格：有私有池的卡下一次 tick 会把自己那批也复制一份（1 帧内到）。*/
    if (ce_dup_producers() > 0) ce_dup_request();
    ce_play_sound(CE_SE_RELEASE, ce_player_x());       /* 0x4d 发动音（与青眼/炎魔同一枚）*/
    ce_log("jera: dup retail %u/%u (failed %u, over-cap %u) + shared points %d/%d + \"%d producers asked\" "
           "(visible total %d)", made, n_src, failed, skipped, n_pts_made, n_pts, ce_dup_producers(), vis);
    return 0;                                          /* 瞬发 */
}

CE_CARD(88, .active_recharge = JERA_RECHARGE, .on_activate = on_activate);
