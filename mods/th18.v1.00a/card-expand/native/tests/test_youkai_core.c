/* test_youkai_core.c —— youkai_core.c 的主机单测。
 *
 * 语义来源（TH18 v1.00a）：
 *   `docs/superpowers/specs/2026-09-12-youkai-108-design.md`（§3.1 止射闸 / §3.2 本体贴图 / §3.3 子机 /
 *     §3.4 弹幕 / §3.5 使魔 1:1 映射与状态差分 / §3.6 击破信号 / §3.7 伤害转交 1/2 / §4 数值）
 *   `local/_scratch/research-108-damage-transfer.md`（life 差分、取整、上限、自伤回路）
 *   `local/_scratch/research-108-shooter-tick.md`（打包 set id、0x16 那格 fire_rate = 0）
 *   `assets/sht/append_shooterset.py` 的 `REPLACE`（妖侧 shooterset 的 9 行 = TH08 v1.00d
 *     `ply03as.sht` 满 P 档逐字段翻译；槽号/行数会与本文件的断言对账）
 *   `docs/superpowers/plans/2026-09-10-ufo-card-feasibility.md` §1.1（spawn 参数 +0x14 = life）
 *
 * 覆盖：低速判定 / 子机取得与聚焦边沿 / 档位与妖侧弹识别 / shooterset 自检（槽号·行数·fire_rate）/
 *       发射点世界坐标（相对 vs 绝对）/ 1:1 发射口映射 / 状态差分（建·撤·退休·打破·重新武装）/
 *       表满 / 使魔计数 / 全撤 / 伤害转交的取整与上限 / life 采样 / 自伤保护盒 / spawn life 硬约束。
 *
 * ⚠ 档位伤害表是**故意不填**（全 `YK_SHOT_DMG_NOT_SET` = 不覆盖；TH08 的单发伤害随 P 下降，
 *   只搬数字会让「P 越高越弱」，理由见 `cards/youkai_core.c` §3.4）
 *   ⇒ 这里只断言**档位映射**与「不覆盖」这个语义。 */
#include <stdio.h>
#include <string.h>

#include "../cards/youkai_core.h"
#ifdef __has_include
#  if __has_include("../sht_ids.h")
#    include "../sht_ids.h"     /* 生成物（make sht）：CE_SHT_SET_* */
#  endif
#endif

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static yk_action_t s_acts[YK_FAM_MAX];

int main(void)
{
    yk_map_t m;
    yk_slot_t *sl;
    yk_action_t a;
    yk_transfer_t t;
    yk_slot_t e;
    float p1[2];
    float ep[2], off[3], org[3], out[2];
    int32_t carry, out_dmg;
    unsigned i;

    p1[0] = 10.0f; p1[1] = 20.0f;

    /* ============================== §3.1 止射闸 */
    CHECK(yk_suppress_native_shot(1, 1) == 1);        /* 有自机 + 聚焦 = 妖机体 ⇒ 压原生弹 */
    CHECK(yk_suppress_native_shot(1, 0) == 0);        /* 高速 = 原机体 ⇒ 不压 */
    CHECK(yk_suppress_native_shot(0, 1) == 0);        /* 标题 / 过场（没有自机）⇒ 不压 */
    CHECK(yk_suppress_native_shot(0, 0) == 0);

    /* ============================== §3.2 本体贴图 */
    CHECK(yk_body_sprite(1) == YK_BODY_SPRITE_FOCUSED);
    CHECK(yk_body_sprite(0) == YK_BODY_SPRITE_UNFOCUSED);
    CHECK(yk_body_sprite(0) < 0);                     /* 高速不抢写（设计：高速 = 原机体）*/
    CHECK(yk_body_sprite(1) < 0);                     /* ★ 占位：妖侧 sprite 资产未到 ⇒ 现在也不抢写 */

    /* ============================== §3.3 子机（SDK 的低速子机声明层，spec §3.3.1）*/
    CHECK(yk_option_count(1) == YK_OPTION_COUNT_FOCUSED);
    CHECK(yk_option_count(0) == YK_OPTION_COUNT_UNFOCUSED);
    CHECK(yk_option_count(0) == 0);                           /* 高速：一格槽都不占（用户定案 5）*/
    CHECK(!yk_option_wanted(0));
    CHECK(yk_option_wanted(1) == (YK_OPTION_COUNT_FOCUSED > 0));   /* wanted = count > 0（与占位值无关）*/
    CHECK(YK_OPTION_COUNT_UNFOCUSED == 0);                    /* 高速永不声明（用户定案 5）*/
    CHECK(yk_option_script_changed(50, 50) == 0);             /* 一致 ⇒ 不用重申请 */
    CHECK(yk_option_script_changed(50, 51) == 1);             /* 声明的脚本和手里那个不一样 ⇒ 重申请 */
    CHECK(yk_option_script_changed(50, -1) == 0);             /* 手里还没有子机 ⇒ 不算「变了」 */
    CHECK(yk_option_script_changed(50, 0) == 1);

    /* ============================== §3.4 弹幕（档位 / 妖侧弹 / shooterset 自检）*/
    CHECK(yk_shot_tier(0) == 0);
    CHECK(yk_shot_tier(99) == 0);
    CHECK(yk_shot_tier(100) == 1);
    CHECK(yk_shot_tier(250) == 2);
    CHECK(yk_shot_tier(399) == 3);
    CHECK(yk_shot_tier(400) == 4);
    CHECK(yk_shot_tier(99999) == YK_SHOT_TIERS - 1);          /* 溢出钳最高档 */
    CHECK(yk_shot_tier_damage(-1) == yk_shot_tier_damage(0)); /* 越界钳位 */
    CHECK(yk_shot_tier_damage(99) == yk_shot_tier_damage(YK_SHOT_TIERS - 1));
    CHECK(yk_shot_tier_damage(0) == YK_SHOT_DMG_NOT_SET);     /* ★ 故意不填（见文件头注释 / youkai_core.c §3.4）*/
    CHECK(yk_shot_tier_damage(4) == YK_SHOT_DMG_NOT_SET);

    /* 打包 id = (set << 8) | 组内行号（`0x45ec69` 打包 / `0x45ee10` 回读）*/
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 3));
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 0));
    CHECK(!yk_bullet_is_youkai(((uint32_t)(YK_SHOT_SET_YOUKAI + 1) << 8) | 3));
    CHECK(!yk_bullet_is_youkai((uint32_t)YK_SHOT_SET_YOUKAI));             /* 只带 row 不带 set */
    CHECK(!yk_bullet_is_youkai(0x0503u));                                  /* 人侧 0x05 组 */
    /* 占位（表 = 全 0 = 不覆盖）⇒ 妖侧弹也原样保留 shooter 记录的 damage */
    out_dmg = -1;
    CHECK(yk_bullet_damage_apply(50, ((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 1, 400, &out_dmg) == 0);
    CHECK(out_dmg == -1);
    CHECK(yk_bullet_damage_apply(50, 0x0501u, 400, &out_dmg) == 0);         /* 不是妖侧弹 ⇒ 不碰 */
    CHECK(yk_bullet_damage_apply(50, ((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 1, 400, 0) == 0); /* out=NULL 安全 */

    /* shooterset 自检：那一格必须是**我们写的合法记录**才敢调 `ce_tick_shooters_for_card`
     *（零售原值是 `fire_rate = 0` ⇒ 装备卡路径整数除零，`0x40aa4f` 没有零分支）。
     * 零售那格长什么样、我们写进去的 9 行长什么样、逐字段出处 —— 全在
     * `assets/sht/append_shooterset.py` 的 `REPLACE` 注释里（一手：TH08 v1.00d `ply03as.sht` 满 P 档）。*/
    CHECK(!yk_shotset_usable(0));        /* ★ 零售那一格的值 = 自检必须拒绝（这条就是除零护栏本身）*/
    CHECK(!yk_shotset_usable(-1));       /* 负 = 终止符 */
    CHECK(!yk_shotset_usable(128));
    CHECK(yk_shotset_usable(1));
    CHECK(yk_shotset_usable(127));
    /* 我们那 9 行的 fire_rate 逐行 = TH08 `rec[+0x00]`（10/5/3/5/10/6/5/5/6）⇒ 每一行都必须判「可用」。
     * ⚠ 这一组数改了要**同时**改 `append_shooterset.py` 的 `REPLACE`（生成器会核对 header 常量）。*/
    CHECK(yk_shotset_usable(3));
    CHECK(yk_shotset_usable(5));
    CHECK(yk_shotset_usable(6));
    CHECK(yk_shotset_usable(10));

    /* 槽号与行数：生成器（`make sht`）会回来核对这两个常量，对不上就失败（防静默漂移）*/
    CHECK(YK_SHOT_SET_YOUKAI == 0x16);          /* 零售唯一无卡映射的那一格（见 youkai_core.h 的出处）*/
    CHECK(YK_SHOT_SET_YOUKAI < 0x17);           /* ★ 必须落在**零售区**：0x17–0x27 是本 mod 追加区，
                                                 *   动了会让别的卡（77/116/117）的索引漂移 */
    CHECK(YK_SHOT_SET_YOUKAI_ROWS == 9);        /* 9 行 = `REPLACE` 里的行数 */
    CHECK(YK_SHOT_SET_YOUKAI_ROWS <= 256);      /* 行号进 `packed_id` 低 8 位 */
    /* 打包 id 往返：9 行都在我们的 set 字节下；别的 set 一律不认（人侧 0x05 / 追加区 0x17）*/
    for (i = 0; i < (unsigned)YK_SHOT_SET_YOUKAI_ROWS; ++i)
        CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | i));
    CHECK(!yk_bullet_is_youkai(((uint32_t)(YK_SHOT_SET_YOUKAI - 1) << 8) | 0));
    CHECK(!yk_bullet_is_youkai(((uint32_t)(YK_SHOT_SET_YOUKAI + 1) << 8) | 0));
    CHECK(!yk_bullet_is_youkai(0x1700u));       /* 追加区第一格（77 的豌豆）不是妖侧弹 */
    /* ★ 判据**只看 set 字节**（不夹行号）：行号越界的打包 id 也算「我们的」——
     * 因为引擎不会造出这种弹（`0x40a9c0` 的行循环在终止符处就停，`0x40aa8f`）。
     * 钉住这条语义，免得以后误以为这里有行号闸。*/
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | YK_SHOT_SET_YOUKAI_ROWS));
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 0xffu));
#ifdef CE_SHT_SET_YOUKAI
    /* `native/sht_ids.h` 是生成物：跑过 `make sht` 之后它就会有这一条 ⇒ 两边必须相等。
     * （没跑过 `make sht` 时这个宏不存在，上面那条硬编码 0x16 仍然钉着槽号。）*/
    CHECK(CE_SHT_SET_YOUKAI == YK_SHOT_SET_YOUKAI);
#endif

    /* ============================== §3.5 发射点世界坐标（etOn 的算法）*/
    ep[0] = 5.0f; ep[1] = 6.0f;
    off[0] = 10.0f; off[1] = -4.0f; off[2] = 0.0f;
    org[0] = 100.0f; org[1] = 200.0f; org[2] = 0.0f;
    yk_emit_world_pos(ep, off, org, out);            /* 相对模式：敌人位置 + 偏移 */
    CHECK(out[0] == 15.0f && out[1] == 2.0f);
    org[2] = 1.0f;                                   /* 绝对模式（etOffsetAbs 把 z 置 1.0）*/
    yk_emit_world_pos(ep, off, org, out);
    CHECK(out[0] == 110.0f && out[1] == 196.0f);
    org[2] = YK_ORIGIN_ABS_THRESHOLD;                /* 阈值本身不算绝对（判据是 > 0.9）*/
    yk_emit_world_pos(ep, off, org, out);
    CHECK(out[0] == 15.0f && out[1] == 2.0f);
    yk_emit_world_pos(0, off, org, out);             /* 敌人坐标指针为空也安全 */
    CHECK(out[0] == 10.0f && out[1] == -4.0f);
    CHECK(yk_emit_active(0) == 0);
    CHECK(yk_emit_active(1) == 1);
    CHECK(yk_emit_active(0x1234) == 1);              /* `props[i].type != 0` = 活跃 */

    /* ============================== §3.5 1:1 映射 + 状态差分 */
    yk_map_reset(&m);
    CHECK(yk_map_fam_count(&m) == 0);

    /* 一个敌人一个活跃槽 ⇒ 恰好 1 只使魔（实测以 1 为主）*/
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 100, 0, 1, p1, &a) == 1);
    CHECK(a.kind == YK_ACT_CREATE && a.enemy_id == 100 && a.slot == 0 && a.fam_id == 0);
    CHECK(a.x == 10.0f && a.y == 20.0f);             /* 动作里带发射点坐标 */
    yk_fam_created(&m, 100, 0, 7001);
    CHECK(yk_map_fam_count(&m) == 1);
    CHECK(yk_note_slot(&m, 100, 0, 1, p1, &a) == 0); /* 已有使魔 ⇒ 不重复建（1:1）*/
    CHECK(yk_note_slot(&m, 100, 5, 0, p1, &a) == 0); /* 不活跃的槽连条目都不建 */
    CHECK(yk_map_find(&m, 100, 5) == 0);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);   /* 本帧扫到了 ⇒ 谁也不撤 */

    /* boss 4 个槽 ⇒ 4 只使魔（实测上限，review §6.3）；敌人 100 这一帧还在 ⇒ 它的使魔留着 */
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 100, 0, 1, p1, &a) == 0);  /* 已有使魔 ⇒ 保持 */
    for (i = 0; i < 4; ++i) {
        CHECK(yk_note_slot(&m, 200, (int)i, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
        yk_fam_created(&m, 200, (int)i, 8000u + i);
    }
    CHECK(yk_map_fam_count(&m) == 5);                /* 4 + 前面那只 */
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);

    /* 状态差分①：发射口变不活跃且有使魔 ⇒ 撤（动作带回 fam_id）*/
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 100, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 1, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 2, 0, p1, &a) == 1);
    CHECK(a.kind == YK_ACT_WITHDRAW && a.fam_id == 8002u && a.enemy_id == 200 && a.slot == 2);
    CHECK(yk_map_fam_count(&m) == 4);
    CHECK(yk_note_slot(&m, 200, 3, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 2, 0, p1, &a) == 0); /* 已撤 ⇒ 不再重复撤 */
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);

    /* 状态差分②：那个槽又变活跃（ECL 重新武装）⇒ 建一只新的 */
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 100, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 1, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 3, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 2, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 200, 2, 8009);
    CHECK(yk_map_fam_count(&m) == 5);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);

    /* 状态差分③：敌人 100 没了（本帧一个槽都没扫到）⇒ 撤 + 退休条目 */
    yk_frame_begin(&m);
    for (i = 0; i < 4; ++i) CHECK(yk_note_slot(&m, 200, (int)i, 1, p1, &a) == 0);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 1);
    CHECK(s_acts[0].kind == YK_ACT_WITHDRAW && s_acts[0].fam_id == 7001u && s_acts[0].enemy_id == 100);
    CHECK(yk_map_find(&m, 100, 0) == 0);             /* 条目退休（`enemy_id` 单调不复用）*/
    CHECK(yk_map_fam_count(&m) == 4);                /* 200 的 8000 / 8001 / 8009 / 8003 */

    /* §3.6 击破信号：使魔被打死 ⇒ 那个槽「被打破」*/
    CHECK(yk_fam_broken(&m, 8001u) == 1);
    CHECK(yk_fam_broken(&m, 999999u) == 0);          /* 不是我们的使魔 */
    CHECK(yk_map_fam_count(&m) == 3);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 200, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 1, 1, p1, &a) == 0); /* 活跃、但「被打破」⇒ 不重建 */
    CHECK(yk_note_slot(&m, 200, 2, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 3, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 1, 0, p1, &a) == 0); /* 不活跃、没有使魔 ⇒ 无动作 */
    CHECK(yk_note_slot(&m, 200, 1, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);  /* inactive→active = 下一波 */
    yk_fam_created(&m, 200, 1, 8010);
    CHECK(yk_map_fam_count(&m) == 4);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);
    CHECK(yk_fam_broken(&m, 8010u) == 1);            /* 再打死一次照样打破 */
    CHECK(yk_map_fam_count(&m) == 3);

    /* 使魔查不到（过场 destroy_all 静默移除，不走死亡事件）⇒ 松开，但**不算打破** ⇒ 下一帧照 1:1 补回来 */
    sl = yk_map_find(&m, 200, 3);
    CHECK(sl != 0 && sl->fam_id == 8003u);
    yk_fam_gone(&m, 8003u);
    CHECK(yk_map_fam_count(&m) == 2);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 200, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 1, 1, p1, &a) == 0); /* 槽 1 仍「被打破」：还是那只 8010 刚被打死 */
    CHECK(yk_note_slot(&m, 200, 2, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 3, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 200, 3, 8011);
    CHECK(yk_map_fam_count(&m) == 3);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);

    /* 表满：一个敌人的 16 槽 + 另一个敌人的 16 槽 = YK_FAM_MAX ⇒ 第 33 个槽放弃（不崩、不排队）*/
    yk_map_reset(&m);
    yk_frame_begin(&m);
    for (i = 0; i < YK_EMIT_SLOTS; ++i) {
        CHECK(yk_note_slot(&m, 300, (int)i, 1, p1, &a) == 1);
        yk_fam_created(&m, 300, (int)i, 30000u + i);
        CHECK(yk_note_slot(&m, 301, (int)i, 1, p1, &a) == 1);
        yk_fam_created(&m, 301, (int)i, 31000u + i);
    }
    CHECK(yk_map_fam_count(&m) == YK_FAM_MAX);
    CHECK(yk_note_slot(&m, 302, 0, 1, p1, &a) == 0);  /* 表满 ⇒ 放弃这个槽 */
    CHECK(yk_map_find(&m, 302, 0) == 0);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);

    /* 全撤（高速 / miss / 局末 / 删卡）*/
    CHECK(yk_withdraw_all(&m, s_acts, (int)YK_FAM_MAX) == (int)YK_FAM_MAX);
    CHECK(yk_map_fam_count(&m) == 0);
    CHECK(s_acts[0].kind == YK_ACT_WITHDRAW);
    CHECK(yk_map_find_fam(&m, 30000u) == 0);
    CHECK(yk_withdraw_all(&m, s_acts, (int)YK_FAM_MAX) == 0);   /* 空表再撤是空操作 */

    /* ★ 缓冲区不够时**不许丢条目**（原来无条件 yk_map_reset ⇒ 剩下的敌人变孤儿）：
     *   表里放 2 个条目、只给 1 格缓冲 ⇒ 撤 1 个、**表里还剩 1 个**（下一帧接着撤）。*/
    CHECK(yk_note_slot(&m, 400, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 400, 0, 9100);
    CHECK(yk_note_slot(&m, 401, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 401, 0, 9101);
    CHECK(yk_map_fam_count(&m) == 2);
    CHECK(yk_withdraw_all(&m, s_acts, 1) == 1);          /* 只撤得下 1 个 */
    CHECK(yk_map_fam_count(&m) == 1);                    /* ⇒ 另一个**还在表里**（不变式不破） */
    CHECK(s_acts[0].kind == YK_ACT_WITHDRAW);
    CHECK(yk_withdraw_all(&m, s_acts, (int)YK_FAM_MAX) == 1);
    CHECK(yk_map_fam_count(&m) == 0);                    /* 处理完 ⇒ 表清空 */
    CHECK(yk_withdraw_all(&m, s_acts, 1) == 0);
    CHECK(sizeof(yk_map_t) <= 1024u);                 /* 静态表预算（现 32 × 28 = 896 B；加 fam_seen 后 24→28）*/

    /* ============================== §3.7 伤害转交 1/2：取整 */
    yk_transfer_reset(&t);
    CHECK(t.carry == 0 && t.pending == 0);
    carry = 0;
    CHECK(yk_half_with_carry(5, &carry) == 2 && carry == 1);      /* 5 / 2 = 2 余 1 */
    CHECK(yk_half_with_carry(3, &carry) == 2 && carry == 0);      /* 5+3 = 8 ⇒ 合计 4 = 8/2，半分不丢 */
    CHECK(yk_half_with_carry(0, &carry) == 0 && carry == 0);
    CHECK(yk_half_with_carry(-7, &carry) == 0 && carry == 0);
    CHECK(yk_half_with_carry(7, 0) == 3);                         /* carry = NULL 安全 */
    CHECK(yk_half_with_carry(1, &carry) == 0 && carry == 1);
    CHECK(yk_half_with_carry(1, &carry) == 1 && carry == 0);      /* 两次 1 点 ⇒ 转 1 */

    yk_transfer_reset(&t);
    yk_transfer_feed(&t, 5);
    yk_transfer_feed(&t, 3);
    CHECK(t.pending == 4 && t.carry == 0);
    yk_transfer_feed(&t, 0);                                      /* 没有伤害就不加 */
    yk_transfer_feed(&t, -9);                                     /* 负数（life 变大）也不加 */
    CHECK(t.pending == 4);
    yk_transfer_reset(&t);
    yk_transfer_feed(&t, 1);
    CHECK(t.pending == 0 && t.carry == 1);
    yk_transfer_feed(&t, 1);
    CHECK(t.pending == 1 && t.carry == 0);

    /* 单帧上限：cap <= 0 = 无上限（占位默认，TODO 待定）*/
    CHECK(yk_transfer_clamp(4, 0) == 4);
    CHECK(yk_transfer_clamp(4, -1) == 4);
    CHECK(yk_transfer_clamp(4, 60) == 4);
    CHECK(yk_transfer_clamp(100, 60) == 60);
    CHECK(yk_transfer_clamp(0, 60) == 0);
    CHECK(yk_transfer_clamp(-5, 60) == 0);
    CHECK(YK_TRANSFER_DEN == 2);                                  /* 用户 2026-09-12 定案 1/2 */
    CHECK(YK_TRANSFER_CAP_PER_FRAME == 0);                        /* ★ 占位：还没封顶 */

    /* life 采样（逐帧差分的边界）*/
    memset(&e, 0, sizeof e);
    e.fam_id = 7;
    e.prev_life = YK_LIFE_UNSAMPLED;
    CHECK(yk_sample_life(&e, 7, 100) == 0);                       /* 第一次采样不算伤害 */
    CHECK(yk_sample_life(&e, 7, 90) == 10);
    CHECK(yk_sample_life(&e, 7, 90) == 0);                        /* 没掉血 */
    CHECK(yk_sample_life(&e, 7, 95) == 0);                        /* life 变大 = 引擎自己重算 */
    CHECK(yk_sample_life(&e, 7, 90) == 5);
    CHECK(yk_sample_life(&e, 8, 10) == 0);                        /* 换了使魔 ⇒ 重新采样 */
    CHECK(yk_sample_life(&e, 8, 4) == 6);
    CHECK(yk_sample_life(0, 1, 1) == 0);                          /* 空条目安全 */

    /* 自伤保护盒（转嫁源打 boss，别把压在 boss 身上的使魔算进来 ⇒ 防每帧自激）*/
    CHECK(yk_in_transfer_zone(0.0f, 0.0f, 0.0f, 0.0f, 16.0f, 16.0f));
    CHECK(yk_in_transfer_zone(20.0f, 0.0f, 0.0f, 0.0f, 16.0f, 16.0f));       /* 20 = 8 + 12 */
    CHECK(!yk_in_transfer_zone(20.1f, 0.0f, 0.0f, 0.0f, 16.0f, 16.0f));
    CHECK(yk_in_transfer_zone(-120.0f, 5.0f, -100.0f, 0.0f, 16.0f, 16.0f));  /* 相对中心 */
    CHECK(!yk_in_transfer_zone(-200.0f, 0.0f, -100.0f, 0.0f, 16.0f, 16.0f));

    /* ============================== §3.5 硬约束：spawn 的 life < 1000 */
    CHECK(YK_LIFE_MAX_ALLOWED < YK_LIFE_LIMIT);
    CHECK(yk_clamp_spawn_life(1) == 1);
    CHECK(yk_clamp_spawn_life(999) == 999);
    CHECK(yk_clamp_spawn_life(YK_LIFE_LIMIT) == YK_LIFE_MAX_ALLOWED);        /* 1000 ⇒ 999 */
    CHECK(yk_clamp_spawn_life(4000) == YK_LIFE_MAX_ALLOWED);                /* 零售 Lily 那种 4000 会被钳 */
    CHECK(yk_clamp_spawn_life(0) == 1);
    CHECK(yk_clamp_spawn_life(-5) == 1);
    CHECK(YK_FAM_LIFE < YK_LIFE_LIMIT);   /* ★ 占位（1，一发即破）；换成 >= 1000 的值这条会红 */

    /* ============ §3.5.1 发射口精确闸（on_enemy_shot_slot → yk_shot_slot_suppressed）============
     * 断点 0x433ffa；返回 1 ⇒ SDK 置 retaddr = 0x43402d ⇒ 本帧这一槽不发射。
     * 判据三分：① 从没管过 ⇒ 放行；② 建过又没了 ⇒ 掐；③ 表里有但还没建 ⇒ 放行。 */
    yk_map_reset(&m);
    /* ① 表里没有这个槽（我们从没管过）⇒ 一律放行 —— 否则第一帧就把敌人的发射口掐死 */
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 0);
    CHECK(yk_shot_slot_suppressed(&m, 500, 3, 0) == 0);
    /* 越界 / 空入参：绝不掐（这是「不吞正常发射」的第一道保险）*/
    CHECK(yk_shot_slot_suppressed(&m, 0, 0, 0) == 0);            /* enemy_id = 0 不是敌人 */
    CHECK(yk_shot_slot_suppressed(&m, 500, -1, 0) == 0);
    CHECK(yk_shot_slot_suppressed(&m, 500, YK_EMIT_SLOTS, 0) == 0);
    CHECK(yk_shot_slot_suppressed(0, 500, 0, 0) == 0);
    /* ③ 活跃、条目已建、但使魔还没建出来（broken == 0）⇒ 放行（建被排到下一个 on_tick_2）*/
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 500, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 0);          /* ★ 关键：不能掐，否则永远建不出来 */
    /* 建好了、活着 ⇒ 放行 */
    yk_fam_created(&m, 500, 0, 9001);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 9001) == 0);
    /* ② 被打死 ⇒ **掐**（= spec §9.6「打掉使魔 ⇒ 那个发射口停火」）*/
    CHECK(yk_fam_broken(&m, 9001) == 1);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 1);
    /* 建完又活着 ⇒ 放行（确认上面那条不是恒 1）*/
    yk_fam_created(&m, 500, 0, 9001);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 9001) == 0);
    CHECK(yk_fam_broken(&m, 9001) == 1);
    /* 别的槽不受影响（同敌人的另一槽 / 别的敌人）⇒ 不误伤 */
    CHECK(yk_shot_slot_suppressed(&m, 500, 1, 0) == 0);
    CHECK(yk_shot_slot_suppressed(&m, 501, 0, 0) == 0);
    /* ECL 重新武装（inactive → active 边沿）复位 broken ⇒ 闸放开 */
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 500, 0, 0, p1, &a) == 0);             /* 变不活跃：已打破 ⇒ 不撤也不建 */
    CHECK(yk_note_slot(&m, 500, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 0);          /* ★ 边沿复位 ⇒ 放行，准备重建 */
    yk_fam_created(&m, 500, 0, 9002);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 9002) == 0);
    /* 静默消失（过场 destroy_all，不走死亡事件）：`yk_fam_gone` 清 `fam_id` ⇒ 闸**当帧就夹上**
     *   （比走死亡事件更快），而下一帧的 1:1 映射会重建它、闸随之放开
     *   —— 这一段与既有的「槽 3 静默移除 ⇒ 下一帧重建」那条断言是同一个口径。 */
    yk_fam_gone(&m, 9002);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 1);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 500, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE); /* 1:1 补建 */
    yk_fam_created(&m, 500, 0, 9003);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 9003) == 0);       /* ★ 补建回来 ⇒ 闸放开 */
    /* ★ 删卡 / 松开低速 / 局末 / 断链的**唯一复位口**：yk_drop_all → yk_withdraw_all → yk_map_reset
     *   ⇒ 整表清空后闸必须**全部放开**（否则钩子还返回 1，敌人就永远哑火了）。 */
    CHECK(yk_withdraw_all(&m, s_acts, (int)YK_FAM_MAX) == 1);    /* 撤掉 9003 那一条 */
    CHECK(s_acts[0].kind == YK_ACT_WITHDRAW);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 0);          /* ★ 闸放开 */
    CHECK(yk_map_fam_count(&m) == 0);

    printf("youkai_core: %d checks, %d failed\n", s_pass + s_fail, s_fail);
    return s_fail ? 1 : 0;
}
