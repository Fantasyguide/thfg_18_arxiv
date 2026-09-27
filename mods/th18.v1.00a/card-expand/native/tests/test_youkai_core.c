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
 * 覆盖：**止射闸的三条门（含 ❓用户口径「子机 0 ⇒ 不压原生弹」）** / 低速判定 / 子机取得与聚焦边沿 /
 *       档位与妖侧弹识别 / shooterset 自检（槽号·行数·fire_rate）/
 *       发射点世界坐标（相对 vs 绝对）/ 1:1 发射口映射 / 状态差分（建·撤·退休·打破·重新武装）/
 *       表满 / 使魔计数 / 全撤 / 伤害转交的取整与上限 / life 采样 / 自伤保护盒 / spawn life 硬约束。
 *
 * ⚠ §3.1 的判据里**第三条**（子机数 > 0）是**用户口径**（2026-09-12），不是一手复刻 ——
 *   断言里那一段逐格与 `yk_option_count` 对账，钉的就是「门与数量表同源」这件事。
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
    /* ★ 2026-09-22（th08 #5）：妖侧组的「集合」描述。旧行为（只有 0x16 一格）= SINGLE_ONLY；
     * 新形态 = 0x16 + 逐角色逐档那一片。两种都测，防止把单格语义改坏。*/
    const yk_sets_t sets_single = YK_SETS_SINGLE_ONLY(YK_SHOT_SET_YOUKAI);
    const yk_sets_t sets_tiers  = { YK_SHOT_SET_YOUKAI, 0x48u, YK_YOUKAI_TIER_SETS };
    (void)sets_tiers;      /* 只在 `#ifdef CE_SHT_SET_YOUKAI_R0` 那一段里用（没跑 make sht 时用不到）*/

    p1[0] = 10.0f; p1[1] = 20.0f;

    /* ★ 2026-09-22（工单 E）：`yk_map_reset` **不**清账本计数（`heals` / `rearms` 是「只增不清」的
     *   计数器，见 `yk_map_rearms` 的注）；卡侧那份表是**文件级静态**（天然 0 初始化），
     *   而这里是栈上的局部 ⇒ 必须先自己清零一次，否则计数断言读到的是垃圾值。*/
    __builtin_memset(&m, 0, sizeof m);

    /* ============================== §3.1 止射闸 */
    CHECK(yk_suppress_native_shot_spec31_legacy(1, 1) == 1);        /* ★ legacy（§3.1 字面）：只说明「有自机 + 聚焦」，不含「0 子机放行」那条用户口径 */
    CHECK(yk_suppress_native_shot_spec31_legacy(1, 0) == 0);        /* 高速 = 原机体 ⇒ 不压 */
    CHECK(yk_suppress_native_shot_spec31_legacy(0, 1) == 0);        /* 标题 / 过场（没有自机）⇒ 不压 */
    CHECK(yk_suppress_native_shot_spec31_legacy(0, 0) == 0);

    /* --- ★★ 新门（2026-09-12）：第三条 = 「本帧妖侧子机数 > 0」-------------------------------
     * ❓**口径（用户 2026-09-12 定）：子机数为 0 ⇒ 放行原生弹**（`count == 0` 时不压）。
     *   **不是一手复刻**：TH08 妖侧不看 P、一按低速就有子机 ⇒ 原作没有「没子机」的档；
     *   本作的约束是「妖侧弹的膛口恒取子机（`0x40a9c0`）」⇒ 没有子机就一发都发不出来，
     *   此时再压原生弹 = 那一档「什么都不发」。
     * 判据顺序（短路）= ① 有自机 ② 聚焦（= spec §3.1 原文）③ 子机数 > 0（= 用户口径）。*/
    {
        int c, lv;
        /* ① 早苗（机体 3）档 0 ⇒ 数量 0 ⇒ **不压**（这条就是用户口径本身）*/
        CHECK(yk_option_count(1, 3, 0) == 0);
        CHECK(yk_should_suppress_native(1, 1, 3, 0) == 0);
        /* ①' 早苗档 1..4（数量 1..4）⇒ **压**（口径只放行「一发都出不来」的那一档）*/
        for (lv = 1; lv < 5; ++lv) {
            CHECK(yk_option_count(1, 3, lv) == lv);
            CHECK(yk_should_suppress_native(1, 1, 3, lv) == 1);
        }
        /* ③ 霊夢 / 魔理沙 / 咲夜（常数 1 / 1 / 4）⇒ **任何档都压**（含档 0；与早苗档 0 的对照）*/
        for (c = 0; c < 3; ++c) {
            for (lv = 0; lv < YK_OPTION_TIERS; ++lv) {
                CHECK(yk_option_count(1, c, lv) > 0);
                CHECK(yk_should_suppress_native(1, 1, c, lv) == 1);
            }
        }
        /* ④ 高速 ⇒ 不压（既有行为不变）：与机体 / 档无关 */
        for (c = 0; c < YK_OPTION_CHARS; ++c) {
            for (lv = 0; lv < YK_OPTION_TIERS; ++lv) {
                CHECK(yk_should_suppress_native(1, 0, c, lv) == 0);
                CHECK(yk_should_suppress_native(0, 1, c, lv) == 0);   /* 没有自机（标题 / 过场）*/
            }
        }
        /* ★ 门与数量表**同源**：判据逐格等于「那张表算出来的数量 > 0」——
         * 这一条断言是防「有人回头往门里塞第二份判断」（例如按机体号枚举）的护栏。*/
        for (c = -2; c < YK_OPTION_CHARS + 2; ++c) {
            for (lv = -2; lv < YK_OPTION_TIERS + 2; ++lv) {
                CHECK(yk_should_suppress_native(1, 1, c, lv)
                      == (yk_option_count(1, c, lv) > 0));
            }
        }
        /* 越界钳位（引擎读到的值不信任）：档 99 ⇒ 钳到档 4 ⇒ 早苗压；机体 99 ⇒ 钳到早苗 ⇒ 档 0 不压 */
        CHECK(yk_option_count(1, 99, 0) == 0);
        CHECK(yk_should_suppress_native(1, 1, 3, 99) == 1);
    }

    /* ============================== §3.2 本体外观（移动动画状态机，一手 th18:0x45b610–0x45b7a8）*/
    /* 四条判据 + 「都不成立 ⇒ 保持原状态」，逐条对着引擎的 `cmp/jcc` 写（见 youkai_core.h §3.2）。*/
    CHECK(yk_body_state(-1, 0, YK_BODY_IDLE) == YK_BODY_MOVE_L);    /* 起步向左（th18:0x45b62a）*/
    CHECK(yk_body_state(-5, 2, YK_BODY_IDLE) == YK_BODY_MOVE_L);    /* 上一帧为正也算「起步」*/
    CHECK(yk_body_state(0, -1, YK_BODY_MOVE_L) == YK_BODY_STOP_L);  /* 从左侧停下（th18:0x45b766）*/
    CHECK(yk_body_state(1, 0, YK_BODY_IDLE) == YK_BODY_MOVE_R);     /* 起步向右（th18:0x45b7a6）*/
    CHECK(yk_body_state(3, -2, YK_BODY_IDLE) == YK_BODY_MOVE_R);
    CHECK(yk_body_state(0, 1, YK_BODY_MOVE_R) == YK_BODY_STOP_R);   /* 从右侧停下（th18:0x45b66a）*/
    /* 持续同向移动 / 静止 ⇒ 引擎**不动脚本** ⇒ 我们也保持上一次的状态。*/
    CHECK(yk_body_state(-1, -1, YK_BODY_MOVE_L) == YK_BODY_MOVE_L);
    CHECK(yk_body_state(1, 1, YK_BODY_MOVE_R) == YK_BODY_MOVE_R);
    CHECK(yk_body_state(0, 0, YK_BODY_IDLE) == YK_BODY_IDLE);
    CHECK(yk_body_state(0, 0, YK_BODY_STOP_L) == YK_BODY_STOP_L);   /* 「停下」那支脚本会自己循环 */
    /* `last` 越界（引擎读到的值不信任）⇒ 钳成 idle。*/
    CHECK(yk_body_state(0, 0, 99) == YK_BODY_IDLE);
    CHECK(yk_body_state(0, 0, -7) == YK_BODY_IDLE);
    /* 状态号必须是 0..4 连续（C 侧 `YK_BODY_SCRIPT[4][YK_BODY_STATES]` 的下标 = 状态号，
     * 生成器 `_YK_STATES` 的次序也是这一套 ⇒ 两边靠这三个常量对齐）。*/
    CHECK(YK_BODY_STATES == 5);
    CHECK(YK_BODY_IDLE == 0 && YK_BODY_MOVE_L == 1 && YK_BODY_STOP_L == 2);
    CHECK(YK_BODY_MOVE_R == 3 && YK_BODY_STOP_R == 4);
    /* ============================== §3.3 子机（SDK 的低速子机声明层，spec §3.3.1 / §12.4）*/
    /* --- 数量表：用户定案「看不同机体在永夜抄与天空璋（文文）的数量」+ 一手出处见 youkai_core.h。
     *     行 = 机体 0..3（霊夢/魔理沙/咲夜/早苗），列 = 火力档 0..4（CURRENT_POWER/100 钳位）。*/
    for (i = 0; i < 5; ++i) {
        CHECK(yk_option_count(1, 0, (int)i) == 1);            /* 霊夢 → th08 pl00 妖侧（八雲紫）= 1，与 P 无关 */
        CHECK(yk_option_count(1, 1, (int)i) == 1);            /* 魔理沙 → th08 pl01 妖侧（アリス）= 1，与 P 无关 */
        CHECK(yk_option_count(1, 2, (int)i) == 4);            /* 咲夜 → th08 pl02 妖侧（レミリア）= 4，与 P 无关 */
    }
    CHECK(yk_option_count(1, 3, 0) == 0);                     /* 早苗 → th16 pl03（射命丸文）= floor(P/100) */
    CHECK(yk_option_count(1, 3, 1) == 1);
    CHECK(yk_option_count(1, 3, 2) == 2);
    CHECK(yk_option_count(1, 3, 3) == 3);
    CHECK(yk_option_count(1, 3, 4) == 4);
    CHECK(YK_OPTION_BY_CHAR[3][4] == 4);                      /* 表本体可读（youkai_core.h）*/
    CHECK(YK_OPTION_TIERS == YK_SHOT_TIERS);                  /* 列数 = 五档（同一组火力档）*/
    CHECK(YK_OPTION_CHARS == 4);
    /* 高速：一格槽都不占（用户定案 5）——与机体 / 火力无关 */
    for (i = 0; i < 4; ++i) {
        int lv;
        for (lv = 0; lv < 5; ++lv) {
            CHECK(yk_option_count(0, (int)i, lv) == 0);
            CHECK(!yk_option_wanted(0, (int)i, lv));
        }
    }
    /* wanted = count > 0（膛口 = 子机 ⇒ 它顺带就是「妖侧能不能发弹」的判据）*/
    CHECK(yk_option_wanted(1, 2, 0));                         /* 咲夜档 0 也有 4 个 */
    CHECK(!yk_option_wanted(1, 3, 0));                        /* 早苗档 0 = 0 个（th16:0x4444c1 的清格循环）*/
    CHECK(yk_option_wanted(1, 3, 1));
    /* 越界一律钳进表内（引擎读到的值不信任）：机体 < 0 / > 3、档 < 0 / > 4 */
    CHECK(yk_option_count(1, -5, 2) == yk_option_count(1, 0, 2));
    CHECK(yk_option_count(1, 99, 2) == yk_option_count(1, 3, 2));
    CHECK(yk_option_count(1, 2, -3) == yk_option_count(1, 2, 0));
    CHECK(yk_option_count(1, 3, 99) == yk_option_count(1, 3, 4));

    /* --- 逐格阵型（spec §12.4 的形状缺口）：**「咲夜 = 4」必须真的是 4 个不同的位置** ---
     * 表 = 原生 `pl0X.sht` 的位置块（一手出处见 youkai_core.h §3.3），格号 k = T[count-1] + i。*/
    {
        yk_opt_off_t o1, o2;
        int c, k, j;

        for (c = 0; c < YK_OPTION_CHARS; ++c) {
            /* ★ 2026-09-22（th08 #1）：机体 0/1 的妖侧子机在原作里**只有 1 个**（`YK_OPTION_BY_CHAR`
             *   的第 0/1 行全是 1）⇒ 它们那张表的 4 格是同一个值（同值的"备用格"）。
             *   所以「同档内两格不重合」这条只能对**真的有多格**的机体（2 咲夜 / 3 早苗）断言。*/
            const int multi = (c >= 2);
            for (k = 1; k <= YK_OPTION_CELLS; ++k) {
                for (j = 0; j < k; ++j) {
                    CHECK(yk_option_offsets(c, k, j, &o1));           /* 每格都取得到 */
                    if (!multi) continue;
                    int m;
                    for (m = j + 1; m < k; ++m) {
                        CHECK(yk_option_offsets(c, k, m, &o2));
                        CHECK(!(o1.fx == o2.fx && o1.fy == o2.fy));
                        CHECK(!(o1.nfx == o2.nfx && o1.nfy == o2.nfy));
                    }
                }
                CHECK(!yk_option_offsets(c, k, k, &o1));              /* 第 count 格起 = 无此格 */
            }
        }
        /* ★★ 2026-09-22（th08 #1）：机体 0/1/2 改用 **TH08 妖侧阵型**（一手见 youkai_core.h）。
         *   灵梦 = 1 个、(0, −96)；魔理沙 = 1 个、(0, −32)；咲夜 = 4 个 (±30,−16) / (±10,−32)。*/
        CHECK(yk_option_offsets(0, 1, 0, &o1) && o1.fx == 0.0f && o1.fy == -96.0f);
        CHECK(yk_option_offsets(1, 1, 0, &o1) && o1.fx == 0.0f && o1.fy == -32.0f);
        CHECK(yk_option_offsets(2, 4, 0, &o1) && o1.fx == -30.0f && o1.fy == -16.0f);
        CHECK(yk_option_offsets(2, 4, 1, &o1) && o1.fx == -10.0f && o1.fy == -32.0f);
        CHECK(yk_option_offsets(2, 4, 2, &o1) && o1.fx ==  10.0f && o1.fy == -32.0f);
        CHECK(yk_option_offsets(2, 4, 3, &o1) && o1.fx ==  30.0f && o1.fy == -16.0f);
        /* 早苗 4 格 = 引擎给 4 个原生子机准备的那 4 个位置（k = 6..9 的 `+0x90` 块）*/
        CHECK(yk_option_offsets(3, 4, 0, &o1) && o1.fx == -28.0f && o1.fy == -20.0f);
        CHECK(yk_option_offsets(3, 4, 1, &o1) && o1.fx == 28.0f  && o1.fy == -20.0f);
        CHECK(yk_option_offsets(3, 4, 2, &o1) && o1.fx == -8.0f  && o1.fy == -32.0f);
        CHECK(yk_option_offsets(3, 4, 3, &o1) && o1.fx == 8.0f   && o1.fy == -32.0f);
        CHECK(yk_option_offsets(3, 4, 0, &o1) && o1.nfx == -36.0f && o1.nfy == -34.0f);  /* 非聚焦块 */
        /* ★ 咲夜（机体 2）：TH08 一手 = 4 格全在自机上方（y < 0）、左右分开、且**互不相同**
         *（具体数值在上面那组 CHECK 里逐格钉住 —— 那是 `th08:0x44eb70` 的跳表 `th08:0x44ee4b`）。*/
        for (j = 0; j < 4; ++j) {
            CHECK(yk_option_offsets(2, 4, j, &o1));
            CHECK(o1.fy < 0.0f);                                   /* 4 格都在上方 */
            CHECK(o1.fx != 0.0f);                                  /* 且左右分开（不是一条竖线） */
            for (k = j + 1; k < 4; ++k) {
                CHECK(yk_option_offsets(2, 4, k, &o2));
                CHECK(o1.fx != o2.fx || o1.fy != o2.fy);
            }
        }
        /* 越界参数：机体 / count / cell / 空指针 */
        CHECK(!yk_option_offsets(4, 1, 0, &o1));
        CHECK(!yk_option_offsets(-1, 1, 0, &o1));
        CHECK(!yk_option_offsets(0, 0, 0, &o1));
        CHECK(!yk_option_offsets(0, YK_OPTION_CELLS + 1, 0, &o1));
        CHECK(!yk_option_offsets(0, 1, -1, &o1));
        CHECK(!yk_option_offsets(0, 1, 0, NULL));
    }

    CHECK(yk_option_script_changed(50, 50) == 0);             /* 一致 ⇒ 不用重申请 */
    CHECK(yk_option_script_changed(50, 51) == 1);             /* 声明的脚本和手里那个不一样 ⇒ 重申请 */
    CHECK(yk_option_script_changed(50, -1) == 0);             /* 手里还没有子机 ⇒ 不算「变了」 */
    CHECK(yk_option_script_changed(50, 0) == 1);

    /* ===== ★★ 2026-09-22（th08 #1）：妖侧子机的**家位置**与弹簧（TH08 一手，见 youkai_core.h）=====
     * owner 原话：「灵梦机体妖子机本身应当追踪上去」。一手 = TH08 的妖侧子机位置是逐机体写死的：
     *   机体 0 霊夢・紫 = (0,−96) + **弹簧跟随**（`th08:0x44e770`）；机体 1 アリス = (0,−32) 吸附；
     *   机体 2 レミリア = 4 格 (±30,−16) / (±10,−32)（`th08:0x44eb70` 的跳表 `th08:0x44ee4b`）。
     * ⚠ 同时钉住那条**一手负结论**：追敌人的分支（`th08:0x44e8d0`）在 v1.00d 够不到
     *   （`player+0xe2abc` 全 exe 只被写 0）⇒ 本卡**不做**追敌人，别把它当漏做。 */
    {
        float home[2], goal[2], pos[2], vel[2];
        const float player[2] = { 200.0f, 300.0f };
        /* 机体 0：1 格、正上方 96 px（`th08:0x4b42c4`）*/
        CHECK(yk_option_home_xy(0, 0, home) && home[0] == 0.0f && home[1] == -96.0f);
        CHECK(yk_option_home_xy(0, 1, home) && home[1] == -96.0f);       /* 只有 1 格 ⇒ 各格同值 */
        /* 机体 1：正上方 32 px（`th08:0x44ea40` 的 `y -= 32.0`）*/
        CHECK(yk_option_home_xy(1, 0, home) && home[0] == 0.0f && home[1] == -32.0f);
        /* 机体 2：4 格，逐格不同（顺序 = 子机序号 0..3）*/
        CHECK(yk_option_home_xy(2, 0, home) && home[0] == -30.0f && home[1] == -16.0f);
        CHECK(yk_option_home_xy(2, 1, home) && home[0] == -10.0f && home[1] == -32.0f);
        CHECK(yk_option_home_xy(2, 2, home) && home[0] ==  10.0f && home[1] == -32.0f);
        CHECK(yk_option_home_xy(2, 3, home) && home[0] ==  30.0f && home[1] == -16.0f);
        /* 机体 3（早苗 = TH16 文）本批**没有**一手数据 ⇒ 返回 0（调用者回落到 TH18 原生阵型）*/
        CHECK(!yk_option_home_xy(3, 0, home));
        CHECK(!yk_option_home_xy(-1, 0, home));
        CHECK(!yk_option_home_xy(0, YK_OPTION_CELLS, home));
        CHECK(!yk_option_home_xy(0, 0, 0));
        /* 只有机体 0 走弹簧 */
        CHECK(yk_option_uses_spring(0) == 1);
        CHECK(yk_option_uses_spring(1) == 0);
        CHECK(yk_option_uses_spring(2) == 0);
        CHECK(yk_option_uses_spring(3) == 0);
        /* 家位置 = 自机 + 偏移，且 `y` 夹到 `>= 32`（`th08:0x44e770` 那一步）*/
        CHECK(yk_option_home_from_player(0, 0, player, goal));
        CHECK(goal[0] == 200.0f && goal[1] == 204.0f);                    /* 300 − 96 */
        {   /* 自机贴到屏上沿（y = 100）⇒ 300−96 那一套不成立，夹到 32 */
            const float hi[2] = { 100.0f, 20.0f };
            CHECK(yk_option_home_from_player(0, 0, hi, goal));
            CHECK(goal[1] == YK_OPT_HOME_Y_MIN);                          /* 20 − 96 < 32 ⇒ 夹成 32 */
        }
        CHECK(!yk_option_home_from_player(3, 0, player, goal));           /* 没数据 ⇒ 不动手 */
        CHECK(!yk_option_home_from_player(0, 0, 0, goal));
        /* 弹簧一步（`th08:0x44e770` 逐步式）：pos/vel 都被就地更新，且**方向朝目标** */
        pos[0] = player[0]; pos[1] = player[1]; vel[0] = 0.0f; vel[1] = 0.0f;
        goal[0] = player[0]; goal[1] = player[1] - 96.0f;                 /* 不夹（300−96 > 32）*/
        yk_opt_spring_step(pos, vel, goal);
        CHECK(vel[1] < 0.0f);                                             /* 向上加速 */
        CHECK(pos[1] < player[1] && pos[1] > goal[1]);                    /* 动了一步但没到 */
        CHECK(pos[0] == player[0] && vel[0] == 0.0f);                     /* x 方向没有差 ⇒ 不动 */
        /* 收敛性：连续积分 600 帧后必须停在家位置上（±0.5 px）*/
        {
            int f;
            for (f = 0; f < 600; ++f) yk_opt_spring_step(pos, vel, goal);
            CHECK(pos[0] > goal[0] - 0.5f && pos[0] < goal[0] + 0.5f);
            CHECK(pos[1] > goal[1] - 0.5f && pos[1] < goal[1] + 0.5f);
        }
        /* 空指针安全 */
        yk_opt_spring_step(0, vel, goal);
        yk_opt_spring_step(pos, 0, goal);
        yk_opt_spring_step(pos, vel, 0);

        /* ★★★ 2026-09-27（owner ①② 咲夜）：**放置型**（`th08:0x44eb70` 的 state 2 不碰位置
         * + 共尾的 `face = norm(face + step)` 自转）。逐条断言照抄那一手的常量。*/
        {
            float face = -1.0f, spin = 0.0f, ang = 0.0f;
            CHECK(yk_option_uses_freeze(2) == 1);              /* 只有咲夜那一支 */
            CHECK(yk_option_uses_freeze(0) == 0);
            CHECK(yk_option_uses_freeze(1) == 0);
            CHECK(yk_option_uses_freeze(3) == 0);
            /* 出生朝向 `+0x2d8`：idx 0/2 = 0、idx 1/3 = π（`0x40490fdb`）*/
            CHECK(yk_option_freeze_face0(0, &face) && face == 0.0f);
            CHECK(yk_option_freeze_face0(1, &face) && face == 3.14159274101257324f);
            CHECK(yk_option_freeze_face0(2, &face) && face == 0.0f);
            CHECK(yk_option_freeze_face0(3, &face) && face == 3.14159274101257324f);
            CHECK(!yk_option_freeze_face0(0, 0));
            /* 每帧增量（跳表 `th08:0x44ee5b`）：+1.5° / −2° / +2° / −1.5°（内外圈反向）*/
            CHECK(yk_option_freeze_spin(0, &spin) && spin > 0.0261799f && spin < 0.0261800f);
            CHECK(yk_option_freeze_spin(1, &spin) && spin > -0.0349066f && spin < -0.0349065f);
            CHECK(yk_option_freeze_spin(2, &spin) && spin > 0.0349065f && spin < 0.0349066f);
            CHECK(yk_option_freeze_spin(3, &spin) && spin > -0.0261800f && spin < -0.0261799f);
            CHECK(!yk_option_freeze_spin(4, &spin) && spin == 0.0f);   /* 越界 ⇒ 不转 */
            CHECK(!yk_option_freeze_spin(0, 0));
            /* `th08:0x43edb0` = `f = angle + step` 再绕回 `[−π, π)` */
            yk_opt_angle_norm(0.0f, 0.5f, &ang);
            CHECK(ang == 0.5f);                                          /* 没越界 ⇒ 原样 */
            yk_opt_angle_norm(3.0f, 0.5f, &ang);
            CHECK(ang < 3.14159274101257324f && ang > -3.14159274101257324f);
            CHECK(ang > 3.0f - 6.2831855f + 0.49f);                      /* = 3.5 − 2π ≈ −2.783 */
            yk_opt_angle_norm(0.0f, -4.0f, &ang);                        /* 负侧只加一次 2π */
            CHECK(ang > 2.28f && ang < 2.29f);                           /* = −4 + 2π ≈ 2.2832 */
            yk_opt_angle_norm(0.0f, 100.0f, &ang);                       /* 迭代上限 16（一手 `cmp eax,0x10`）*/
            CHECK(ang < 0.0f && ang > -1.0f);
            yk_opt_angle_norm(0.0f, 0.5f, 0);                            /* 空指针安全 */
        }

        /* ★★ 2026-09-22 晚（owner 第 2 条）：**追敌**那一段（一手 `th08:0x44e8d0`）。
         * 家位置 = 目标 `final_pos` + (0, +32)，`y` 夹到 `>= YK_OPT_HOME_Y_MIN`。*/
        {
            const float tgt[2] = { 240.0f, 120.0f };
            CHECK(yk_option_chase_goal(tgt, goal));
            CHECK(goal[0] == 240.0f && goal[1] == 120.0f + YK_OPT_CHASE_GOAL_DY);
            {   /* 目标贴到屏上沿 ⇒ 夹到 32（`th08:0x44e918`–`0x44e92f` 的 fcomp/写回）*/
                const float hi2[2] = { 240.0f, -40.0f };
                CHECK(yk_option_chase_goal(hi2, goal));
                CHECK(goal[1] == YK_OPT_HOME_Y_MIN);
            }
            CHECK(!yk_option_chase_goal(0, goal));
            CHECK(!yk_option_chase_goal(tgt, 0));
            /* 进门条件（`th08:0x44e770` 尾巴）：目标存在 **且** 出场满 10 帧 */
            CHECK(yk_option_chase_want(1, YK_OPT_CHASE_MIN_AGE));
            CHECK(yk_option_chase_want(1, YK_OPT_CHASE_MIN_AGE + 1));
            CHECK(!yk_option_chase_want(1, YK_OPT_CHASE_MIN_AGE - 1));
            CHECK(!yk_option_chase_want(1, 0));
            CHECK(!yk_option_chase_want(1, -5));                          /* 越界当 0 */
            CHECK(!yk_option_chase_want(0, 1000));                        /* 没目标 ⇒ 永不进 */
            /* 旋转/待机标签号：脚本生成器（`make_youkai_option_art.py` 的 LABEL_MAP）里
             * TH08 原号 3→5（旋转）、1→2（待机循环 A）—— 两处**必须**一致，改一处就编译期红。*/
            CHECK(YK_OPT_SPIN_LABEL == 5);
            CHECK(YK_OPT_IDLE_LABEL == 2);
            /* 追踪用的弹簧与跟随自机**同一条**：从自机位出发、600 帧后收敛到目标上方 32 */
            pos[0] = 200.0f; pos[1] = 300.0f; vel[0] = 0.0f; vel[1] = 0.0f;
            {   int f; for (f = 0; f < 600; ++f) yk_opt_spring_step(pos, vel, goal); }
            CHECK(pos[0] > goal[0] - 0.5f && pos[0] < goal[0] + 0.5f);
            CHECK(pos[1] > goal[1] - 0.5f && pos[1] < goal[1] + 0.5f);
        }
    }

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
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 3, &sets_single));
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 0, &sets_single));
    CHECK(!yk_bullet_is_youkai(((uint32_t)(YK_SHOT_SET_YOUKAI + 1) << 8) | 3, &sets_single));
    CHECK(!yk_bullet_is_youkai((uint32_t)YK_SHOT_SET_YOUKAI, &sets_single));             /* 只带 row 不带 set */
    CHECK(!yk_bullet_is_youkai(0x0503u, &sets_single));                                  /* 人侧 0x05 组 */
    /* 占位（表 = 全 0 = 不覆盖）⇒ 妖侧弹也原样保留 shooter 记录的 damage */
    out_dmg = -1;
    CHECK(yk_bullet_damage_apply(50, ((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 1, 400, &sets_single, &out_dmg) == 0);
    CHECK(out_dmg == -1);
    CHECK(yk_bullet_damage_apply(50, 0x0501u, 400, &sets_single, &out_dmg) == 0);         /* 不是妖侧弹 ⇒ 不碰 */
    CHECK(yk_bullet_damage_apply(50, ((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 1, 400, &sets_single, 0) == 0); /* out=NULL 安全 */

    /* shooterset 自检：那一格必须是**我们写的合法记录**才敢调 `ce_tick_shooters_for_card`
     *（原生原值是 `fire_rate = 0` ⇒ 装备卡路径整数除零，`0x40aa4f` 没有零分支）。
     * 原生那格长什么样、我们写进去的 9 行长什么样、逐字段出处 —— 全在
     * `assets/sht/append_shooterset.py` 的 `REPLACE` 注释里（一手：TH08 v1.00d `ply03as.sht` 满 P 档）。*/
    CHECK(!yk_shotset_usable(0));        /* ★ 原生那一格的值 = 自检必须拒绝（这条就是除零护栏本身）*/
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
    CHECK(YK_SHOT_SET_YOUKAI == 0x16);          /* 原生唯一无卡映射的那一格（见 youkai_core.h 的出处）*/
    CHECK(YK_SHOT_SET_YOUKAI < 0x17);           /* ★ 必须落在**原生区**：0x17–0x27 是本 mod 追加区，
                                                 *   动了会让别的卡（77/116/117）的索引漂移 */
    CHECK(YK_SHOT_SET_YOUKAI_ROWS == 9);        /* 9 行 = `REPLACE` 里的行数 */
    CHECK(YK_SHOT_SET_YOUKAI_ROWS <= 256);      /* 行号进 `packed_id` 低 8 位 */
    /* 打包 id 往返：9 行都在我们的 set 字节下；别的 set 一律不认（人侧 0x05 / 追加区 0x17）*/
    for (i = 0; i < (unsigned)YK_SHOT_SET_YOUKAI_ROWS; ++i)
        CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | i, &sets_single));
    CHECK(!yk_bullet_is_youkai(((uint32_t)(YK_SHOT_SET_YOUKAI - 1) << 8) | 0, &sets_single));
    CHECK(!yk_bullet_is_youkai(((uint32_t)(YK_SHOT_SET_YOUKAI + 1) << 8) | 0, &sets_single));
    CHECK(!yk_bullet_is_youkai(0x1700u, &sets_single));       /* 追加区第一格（77 的豌豆）不是妖侧弹 */
    /* ★ 判据**只看 set 字节**（不夹行号）：行号越界的打包 id 也算「我们的」——
     * 因为引擎不会造出这种弹（`0x40a9c0` 的行循环在终止符处就停，`0x40aa8f`）。
     * 钉住这条语义，免得以后误以为这里有行号闸。*/
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | YK_SHOT_SET_YOUKAI_ROWS, &sets_single));
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 0xffu, &sets_single));
#ifdef CE_SHT_SET_YOUKAI
    /* `native/sht_ids.h` 是生成物：跑过 `make sht` 之后它就会有这一条 ⇒ 两边必须相等。
     * （没跑过 `make sht` 时这个宏不存在，上面那条硬编码 0x16 仍然钉着槽号。）*/
    CHECK(CE_SHT_SET_YOUKAI == YK_SHOT_SET_YOUKAI);
#endif
#ifdef CE_SHT_SET_YOUKAI_R0
    /* ===== ★★ 2026-09-22（th08 #5）：妖侧不再只有一格 =====
     * 逐角色 × 逐火力档那 15 格**必须**是连续的、且顺序 = R0..R4, M0..M4, S0..S4
     *（`youkai.c` 的 `_Static_assert` 钉同一件事；这里再钉一次是为了让**纯逻辑单测**也能跑）。 */
    /* ★ 2026-09-23：**不再钉绝对值** —— 111 的组数会随数值口径变化（DPS 对齐 TH18 原生后
     *   31 → 43 格），硬编码会把这条纯逻辑单测被**无关改动**打红。只钉意图：
     *   ① 排在 111 那一段之后；② **20 格**（★ 2026-09-26 由 15 扩到 20：早苗那 5 格是 TH16
     *   `pl03sub`（秋/文），见 `YOUKAI_TIERS_A`）都在「宣布可用」的范围内。*/
    CHECK(CE_SHT_SET_YOUKAI_R0 > CE_SHT_SET_MARISA_MAIN_00);
    CHECK(CE_SHT_SET_YOUKAI_R0 + YK_SHOT_TIERS * 4 <= CE_SHT_SLOTS);
    CHECK(CE_SHT_SET_YOUKAI_M0 == CE_SHT_SET_YOUKAI_R0 + YK_SHOT_TIERS);
    CHECK(CE_SHT_SET_YOUKAI_S0 == CE_SHT_SET_YOUKAI_M0 + YK_SHOT_TIERS);
    CHECK(CE_SHT_SET_YOUKAI_A0 == CE_SHT_SET_YOUKAI_S0 + YK_SHOT_TIERS);
    CHECK(YK_YOUKAI_TIER_SETS == 20);
    {
        const yk_sets_t real = { YK_SHOT_SET_YOUKAI, CE_SHT_SET_YOUKAI_R0, YK_YOUKAI_TIER_SETS };
        unsigned k;
        /* ① 保留格 + 15 格逐档格 ⇒ 全部认（判据**只看 set 字节**，与行号无关）*/
        CHECK(yk_set_is_youkai(YK_SHOT_SET_YOUKAI, &real));
        for (k = 0; k < YK_YOUKAI_TIER_SETS; ++k)
            CHECK(yk_set_is_youkai(CE_SHT_SET_YOUKAI_R0 + k, &real));
        /* ② **紧邻的两格都不认** —— 这是"必须精确、不能写宽区间"的关键断言：
         *    0x47 = 111 的主炮组最后一格、0x57 = 下一张卡的组。写成宽区间会把它们的弹套上
         *    TH08 的伤害表（静默改别人的卡）。*/
        CHECK(!yk_set_is_youkai(CE_SHT_SET_YOUKAI_R0 - 1u, &real));
        CHECK(!yk_set_is_youkai(CE_SHT_SET_YOUKAI_R0 + YK_YOUKAI_TIER_SETS, &real));
        CHECK(!yk_set_is_youkai(0x17u, &real));                     /* 77 豌豆 */
        CHECK(!yk_set_is_youkai(0x28u, &real));                     /* 113 灵界 */
        CHECK(!yk_set_is_youkai(0x05u, &real));                     /* 人侧主炮 */
        /* ③ 单格形态（旧行为）：逐档格**不**认、保留格认 */
        CHECK(yk_set_is_youkai(YK_SHOT_SET_YOUKAI, &sets_single));
        CHECK(!yk_set_is_youkai(CE_SHT_SET_YOUKAI_R0, &sets_single));
        /* ④ NULL = 什么都不认（安全侧：宁可不改也不误改别人的弹）*/
        CHECK(!yk_set_is_youkai(YK_SHOT_SET_YOUKAI, 0));
        CHECK(!yk_set_is_youkai(CE_SHT_SET_YOUKAI_R0, 0));
        /* ⑤ 打包 id 往返（各角色各档各取一行）*/
        for (k = 0; k < YK_YOUKAI_TIER_SETS; ++k)
            CHECK(yk_bullet_is_youkai(((CE_SHT_SET_YOUKAI_R0 + k) << 8) | 4u, &real));
        CHECK(!yk_bullet_is_youkai((0x4700u), &real));
        /* ⑥ 伤害层：档位表还是占位 ⇒ 对逐档格也**不覆盖**（与单格同口径）*/
        out_dmg = -1;
        CHECK(yk_bullet_damage_apply(50, ((CE_SHT_SET_YOUKAI_R0 + 3) << 8) | 1, 400, &real, &out_dmg) == 0);
        CHECK(out_dmg == -1);
    }
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

    /* ★★★ 2026-09-26（owner #37「发射口与敌人重合不应该建立使魔」）：体上炮口的判据。
     *   相对模式 + 偏移恰好 (0,0) ⇒ 1（跳过这一格、不建使魔）；绝对模式（org.z > 阈值）恒 0
     *   （位置是脚本 `etOffsetAbs` 给的，与本体无关）；任何非 0 偏移 ⇒ 0（那是**有意摆远**的发射口，
     *   原生语料里最小是 16 px）。*/
    off[0] = 0.0f; off[1] = 0.0f; off[2] = 0.0f;
    org[0] = 100.0f; org[1] = 200.0f; org[2] = 0.0f;
    CHECK(yk_emit_on_body(off, org) == 1);           /* 相对 + (0,0) = 本体 */
    org[2] = 1.0f;                                   /* etOffsetAbs ⇒ 脚本给的位置，不算本体 */
    CHECK(yk_emit_on_body(off, org) == 0);
    org[2] = YK_ORIGIN_ABS_THRESHOLD;                /* 阈值本身仍算相对（判据是 > 0.9）*/
    CHECK(yk_emit_on_body(off, org) == 1);
    off[1] = -16.0f;                                 /* 原生最小显式偏移 = 16 px ⇒ 保留使魔 */
    CHECK(yk_emit_on_body(off, org) == 0);
    off[1] = 0.0f; off[0] = 32.0f;
    CHECK(yk_emit_on_body(off, org) == 0);
    off[0] = 0.0f;
    CHECK(yk_emit_on_body(off, 0) == 1);             /* 没有 origins 段（全 0）也按相对处理 */
    CHECK(yk_emit_on_body(0, org) == 0);             /* 空指针安全 */

    /* 槽 → 皮肤变体（四色一套一槽，用户定案；脚本宏在 cards/youkai.c，这里只钉映射）：
     * 槽 0..3 ⇒ 变体 0..3（= S53..S56，红/绿/蓝白/黄）；槽 >= 4 走 `& 3` 兜底（规格说走不到）。*/
    CHECK(yk_fam_variant(0) == 0);
    CHECK(yk_fam_variant(1) == 1);
    CHECK(yk_fam_variant(2) == 2);
    CHECK(yk_fam_variant(3) == 3);
    CHECK(yk_fam_variant(4) == 0);                   /* 兜底：四色循环（❓ 自定，非第 5 色）*/
    CHECK(yk_fam_variant(5) == 1);
    CHECK(yk_fam_variant(YK_EMIT_SLOTS - 1) < 4);    /* 变体恒在四色域内 ⇒ 卡侧查表不会越界 */
    CHECK(yk_fam_variant(-1) == 0);                  /* 越界入参也安全 */
    for (i = 0; i < (unsigned)YK_EMIT_SLOTS; ++i) CHECK(yk_fam_variant((int)i) >= 0 && yk_fam_variant((int)i) < 4);

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

    /* ★★★ 2026-09-22（P1，实跑日志直证）：**判据必须是只读的**。
     * 这是那次「跟随 VM 表满 32 格 ⇒ 之后造的使魔 `stays invisible` + 敌方使魔滞留」的根因：
     * 卡侧每帧的「排除我们自己的使魔」原来调的是**事件** `yk_fam_broken`（它会清 `fam_id` + 置
     * `broken`），于是地图侧记录被清（那只真敌人变孤儿）、VM 侧记录再没人收。
     * 这里把两件事钉死：① `yk_map_is_fam` 命中时 `yk_map_t` **一个字节都不变**（含 `broken` /
     * `no_fam_frames` / 位置）；② 连问两次结果一致（幂等）；③ 对照 `yk_fam_broken` 确实会改表
     * （两者的语义差别是**故意的**，不是可以互相替换的）。*/
    {
        yk_map_t snapshot;
        yk_slot_t *cur;
        float p2[2];
        p2[0] = 1.0f; p2[1] = 2.0f;
        yk_map_reset(&m);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 500, 0, 1, p2, &a) == 1 && a.kind == YK_ACT_CREATE);
        yk_fam_created(&m, 500, 0, 5555u);
        yk_frame_end(&m, s_acts, (int)YK_FAM_MAX);
        __builtin_memcpy(&snapshot, &m, sizeof m);
        cur = yk_map_find(&m, 500, 0);
        CHECK(cur != 0 && cur->fam_id == 5555u && cur->fam_seen == 1 && cur->broken == 0);
        CHECK(yk_map_is_fam(&m, 5555u) == 1);                    /* 命中 */
        CHECK(yk_map_is_fam(&m, 5555u) == 1);                    /* 幂等（连问两次）*/
        CHECK(yk_map_is_fam(&m, 0u) == 0);                       /* fam_id 0 = 空 ⇒ 恒 0 */
        CHECK(yk_map_is_fam(&m, 12345u) == 0);                   /* 不是我们的使魔 */
        CHECK(yk_map_is_fam(0, 5555u) == 0);                     /* NULL 表不崩 */
        CHECK(__builtin_memcmp(&snapshot, &m, sizeof m) == 0);   /* ★ 纯读：整张表逐字节不变 */
        CHECK(yk_fam_broken(&m, 5555u) == 1);                    /* 对照：事件版**会**改表 */
        CHECK(__builtin_memcmp(&snapshot, &m, sizeof m) != 0);
        CHECK(yk_map_is_fam(&m, 5555u) == 0);                    /* 事件之后它不再是我的使魔 */
        CHECK(yk_map_find(&m, 500, 0) != 0 && yk_map_find(&m, 500, 0)->broken == 1);
    }

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
    CHECK(sizeof(yk_map_t) <= 1024u);                 /* 静态表预算（现 32 × 28 + 8 = 904 B）*/
    CHECK(sizeof(yk_slot_t) == 28u);                  /* ★ route A 的 rearm_pending 塞进 padding，不许撑大 */

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
    CHECK(yk_clamp_spawn_life(4000) == YK_LIFE_MAX_ALLOWED);                /* 原生 Lily 那种 4000 会被钳 */
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

    /* ===== ★★ 2026-09-22（th08 #7）闸的**到期自愈**：ECL 不再重新武装时不许永久哑火 =====
     * 现象（owner 单卡实跑）：「击破 boss 一个阶段后 boss 几乎不再发弹」——boss 的发射口在
     * 整个阶段里**持续活跃**（反复 `etOn`、中间不调 `etNew`），而 `broken` 的唯一复位口是
     * inactive→active 边沿 ⇒ 闸夹死 + 使魔不再重建 ⇒ 那个槽**永远**不发弹。
     * 修法 = 连续 `YK_EMIT_BROKEN_TTL` 帧「记忆里有使魔、当前没有」⇒ 无条件放开。 */
    CHECK(YK_EMIT_BROKEN_TTL > 0);
    yk_map_reset(&m);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 700, 2, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 700, 2, 7001);
    CHECK(yk_fam_broken(&m, 7001) == 1);                         /* 使魔被打死 */
    CHECK(yk_shot_slot_suppressed(&m, 700, 2, 0) == 1);          /* 闸夹上（设计如此）*/
    {
        int f;
        unsigned base = yk_map_heals(&m);
        /* TTL−1 帧：仍然夹着（发射口**一直活跃**，没有边沿）*/
        for (f = 1; f < YK_EMIT_BROKEN_TTL; ++f) {
            yk_frame_begin(&m);
            CHECK(yk_note_slot(&m, 700, 2, 1, p1, &a) == 0);     /* broken ⇒ 不重建、不撤 */
            CHECK(yk_shot_slot_suppressed(&m, 700, 2, 0) == 1);
        }
        CHECK(yk_map_heals(&m) == base);                         /* 还没到期 ⇒ 没自愈 */
        /* 第 TTL 帧：到期 ⇒ 闸放开 + 本帧就重建（CREATE 动作）*/
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 700, 2, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
        CHECK(yk_shot_slot_suppressed(&m, 700, 2, 0) == 0);      /* ★ 闸放开（关键断言）*/
        CHECK(yk_map_heals(&m) == base + 1);                     /* ★ 记了一次账 */
        yk_fam_created(&m, 700, 2, 7002);
        CHECK(yk_shot_slot_suppressed(&m, 700, 2, 7002) == 0);   /* 使魔回来了 ⇒ 照旧放行 */
        /* 重建之后再打死 ⇒ 年龄**重新**从 0 起算（不会立刻又自愈）*/
        CHECK(yk_fam_broken(&m, 7002) == 1);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 700, 2, 1, p1, &a) == 0);
        CHECK(yk_map_heals(&m) == base + 1);                     /* 只过了 1 帧 ⇒ 没自愈 */
        CHECK(yk_shot_slot_suppressed(&m, 700, 2, 0) == 1);
    }
    /* 到期之前 ECL 若重新武装过 ⇒ 立刻复位、**不计**自愈账（走的是边沿那条路）*/
    yk_map_reset(&m);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 701, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 701, 0, 7101);
    CHECK(yk_fam_broken(&m, 7101) == 1);
    {
        unsigned base = yk_map_heals(&m);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 701, 0, 0, p1, &a) == 0);         /* etNew：清零 type */
        CHECK(yk_note_slot(&m, 701, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE); /* etSprite：写回 */
        CHECK(yk_shot_slot_suppressed(&m, 701, 0, 0) == 0);
        CHECK(yk_map_heals(&m) == base);                         /* 边沿复位**不**记账 */
    }

    /* ============ ★★ 2026-09-22（工单 E）route A = 闸的复位口改成「下一波 etOn」============
     * 一手根因：`engine/enemy/th18/07-emitter-slots-waves-and-phase-transitions.md`
     * 原来的复位口是 `props[i].type` 的 inactive→active 边沿，而 `etNew` 与同槽 `etSprite` 在原生 ECL 里
     * **永远同帧**（22 份 ECL、282/282 处）⇒ 卡每帧只采样一次，永远看不到那一瞬 ⇒ 边沿基本不发生；
     * 而 boss 段首的 `enmKillAll()` 必然把使魔清光 ⇒ 原来只能等 `YK_EMIT_BROKEN_TTL` 才放开。
     * route A：夹住的槽**被 `etOn` 问到**（= `yk_shot_slot_gate`）时置 `rearm_pending`，
     * 卡 tick 的 `yk_note_slot` 同帧消费 ⇒ 清闸 + 重建（每个使魔死亡最多丢 1 条 etOn）。 */
    CHECK(sizeof(yk_slot_t) == 28u);                  /* 新字段塞进 padding（上面那条是同一件事）*/
    yk_map_reset(&m);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 800, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 800, 0, 8001);
    CHECK(yk_shot_slot_gate(&m, 800, 0, 8001) == 0);            /* 使魔活着 ⇒ 放行，不记账 */
    CHECK(yk_map_rearms(&m) == 0);
    CHECK(yk_fam_broken(&m, 8001) == 1);                        /* 被打死（例如段首 enmKillAll） */
    {
        unsigned base = yk_map_rearms(&m);
        unsigned hbase = yk_map_heals(&m);
        unsigned lbase = yk_map_releases(&m);
        CHECK(yk_shot_slot_gate(&m, 800, 0, 0) == 1);           /* 第一次 etOn：掐 + 记「下一波」 */
        CHECK(yk_map_rearms(&m) == base + 1);
        /* ★★★ 2026-09-22（P8-3 / **route A′**）：**第二次**被问到时当场放行并清夹持态 ——
         * 「恢复」不再依赖卡 tick（卡 tick 在未聚焦时根本不跑），也不用等 120 帧 TTL。*/
        CHECK(yk_shot_slot_gate(&m, 800, 0, 0) == 0);
        CHECK(yk_map_releases(&m) == lbase + 1);                /* ★ 记一笔「第二问放行」 */
        CHECK(yk_map_rearms(&m) == base + 1);                   /* ★ 不重复记「掐」的账 */
        CHECK(yk_shot_slot_suppressed(&m, 800, 0, 0) == 0);     /* ★ 闸已放开（关键断言） */
        CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 800, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);  /* 1:1 映射重建 */
        CHECK(yk_map_heals(&m) == hbase);                       /* ★ 没走 TTL 兜底 */
        yk_fam_created(&m, 800, 0, 8002);
        CHECK(yk_shot_slot_gate(&m, 800, 0, 8002) == 0);        /* 下一次 etOn：使魔已回来 ⇒ 放行 */
        /* 再来一轮：证明不是一次性的（掐 → 放 → 重建）*/
        CHECK(yk_fam_broken(&m, 8002) == 1);
        CHECK(yk_shot_slot_gate(&m, 800, 0, 0) == 1);
        CHECK(yk_map_rearms(&m) == base + 2);
        CHECK(yk_shot_slot_gate(&m, 800, 0, 0) == 0);           /* 第二问：route A′ 再放一次 */
        CHECK(yk_map_releases(&m) == lbase + 2);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 800, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
        yk_fam_created(&m, 800, 0, 8003);
    }
    /* 没被夹住的槽：闸恒放行、一次都不记账（route A 绝不误伤）。
     * ⚠ `rearms`/`heals` 都是**只增不清**的（`yk_map_reset` 不动它们，卡侧靠快照比对打日志）
     *   ⇒ 这里一律与快照比，别写绝对值。*/
    {
    unsigned rbase = yk_map_rearms(&m);
    yk_map_reset(&m);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 801, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    CHECK(yk_shot_slot_gate(&m, 801, 0, 0) == 0);               /* 还没建完：放行（第一道保险） */
    CHECK(yk_shot_slot_gate(&m, 801, 1, 0) == 0);               /* 别的槽 / 从没管过 */
    CHECK(yk_shot_slot_gate(&m, 0, 0, 0) == 0);
    CHECK(yk_shot_slot_gate(0, 801, 0, 0) == 0);
    CHECK(yk_map_rearms(&m) == rbase);
    /* 整表复位（撤卡 / 松低速 / 局末）⇒ 待处理的「下一波」也一起清掉，不留悬案 */
    yk_fam_created(&m, 801, 0, 8101);
    CHECK(yk_fam_broken(&m, 8101) == 1);
    CHECK(yk_shot_slot_gate(&m, 801, 0, 0) == 1);
    yk_map_reset(&m);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 801, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);  /* 新条目、无残留 */
    CHECK(yk_shot_slot_suppressed(&m, 801, 0, 0) == 0);
    }

    /* ============ §3.10 人妖槽（TH08 `youkaiGauge`；限值/判定/几何全部一手）============ */
    /* 夹紧：`th08:0x43c0fa`（下限）/`th08:0x43c128`（上限），两条都**取等号**。*/
    CHECK(yk_gauge_clamp(0) == 0);
    CHECK(yk_gauge_clamp(9999) == 9999);
    CHECK(yk_gauge_clamp(10000) == YK_GAUGE_YOUKAI_LIMIT);
    CHECK(yk_gauge_clamp(12345) == YK_GAUGE_YOUKAI_LIMIT);
    CHECK(yk_gauge_clamp(-12345) == YK_GAUGE_HUMAN_LIMIT);
    /* 加法先按 i16 截断再夹（`th08:0x43c0e9` 的 `mov WORD`）——小增量下与直接加等价。*/
    CHECK(yk_gauge_add(9000, 500) == 9500);
    CHECK(yk_gauge_add(9900, 500) == YK_GAUGE_YOUKAI_LIMIT);
    CHECK(yk_gauge_add(-9900, -500) == YK_GAUGE_HUMAN_LIMIT);
    CHECK(yk_gauge_add(0, 200) == 200);
    /* 死区：`|g| <= 9 ⇒ 0`（`th08:0x44bf4d` 的 fcomp 9.0 → `:0x44bf61`）。*/
    CHECK(yk_gauge_dead_zone(9) == 0);
    CHECK(yk_gauge_dead_zone(-9) == 0);
    CHECK(yk_gauge_dead_zone(0) == 0);
    CHECK(yk_gauge_dead_zone(10) == 10);
    CHECK(yk_gauge_dead_zone(-10) == -10);
    /* 四档判定（`th08:0x406d10/40/70/a0`；极人/中人 ≤、极妖/中妖 ≥，**取等号**）。*/
    CHECK(yk_gauge_is_extremely_human(-8000) == 1);
    CHECK(yk_gauge_is_extremely_human(-7999) == 0);
    CHECK(yk_gauge_is_moderately_human(-2000) == 1);
    CHECK(yk_gauge_is_moderately_human(-1999) == 0);
    CHECK(yk_gauge_is_extremely_youkai(8000) == 1);
    CHECK(yk_gauge_is_extremely_youkai(7999) == 0);
    CHECK(yk_gauge_is_moderately_youkai(2000) == 1);
    CHECK(yk_gauge_is_moderately_youkai(1999) == 0);
    CHECK(yk_gauge_is_extremely_human(0) == 0 && yk_gauge_is_extremely_youkai(0) == 0);
    /* 收刻符的增量：聚焦 = +111、否则 −111（`th08:0x44139e`/`:0x4413b0`）。*/
    CHECK(yk_gauge_orbs_delta(1) == 111 && yk_gauge_orbs_delta(0) == -111);
    /* ★★★ 2026-09-22（P3 第二批）：射击中每帧 ±26（一手 `Player.cpp:445-452` + `:541`
     * `0x6590 / 0x3e7` = 26000/999 = 26，方向 = `shotType & 1` = isYoukaiMode ⇒ 本卡口径的低速位）。
     * ★ 2026-09-27 订正（§BE #110）：`yk_gauge_gate_blocks` 现在**只**给推进支用（参数 = `shotActive`
     *   = TH08 `player+0xfdc`）；击破 ±200 / 收刻符 ±111 / 擦弹 +100 三笔**不再过闸** ——
     *   它们对应的是 `AddToYoukaiGauge` 的第一道闸 `th08:0x43c0c2`，而它读的 `ds:0x17d6ed4`
     *   全 exe 无写点又是 BSS ⇒ 恒为 0 ⇒ 那道闸永不生效。*/
    CHECK(YK_GAUGE_SHOOT_DELTA == 26);
    CHECK(yk_gauge_shoot_delta(1) == 26 && yk_gauge_shoot_delta(0) == -26);
    CHECK(yk_gauge_gate_blocks(1) == 1 && yk_gauge_gate_blocks(0) == 0);
    /* ★★★ 2026-09-22（永点三来源之一）：一手 `th08:0x42da54` 的**或门** ——
     * 「極端人間 **或** 極端妖怪」时那一颗刻符是 **state 1**（生成即满速归航 = 自动吸收）。*/
    CHECK(yk_orb_instant_absorb(-8000) == 1);      /* 極端人間（`<= −8000`）*/
    CHECK(yk_orb_instant_absorb(8000) == 1);       /* 極端妖怪（`>= +8000`）*/
    CHECK(yk_orb_instant_absorb(-3000) == 0);      /* 中人：不成立 */
    CHECK(yk_orb_instant_absorb(0) == 0);
    /* ★★★ 2026-09-22（P3 第二批）：射击中每帧 ±26（一手 `Player.cpp:445-452` + `:541`
     * `0x6590 / 0x3e7` = 26000/999 = 26，方向 = `shotType & 1` = isYoukaiMode ⇒ 本卡口径的低速位）。
     * ★ 2026-09-27 订正（§BE #110）：`yk_gauge_gate_blocks` 现在**只**给推进支用（参数 = `shotActive`
     *   = TH08 `player+0xfdc`）；击破 ±200 / 收刻符 ±111 / 擦弹 +100 三笔**不再过闸** ——
     *   它们对应的是 `AddToYoukaiGauge` 的第一道闸 `th08:0x43c0c2`，而它读的 `ds:0x17d6ed4`
     *   全 exe 无写点又是 BSS ⇒ 恒为 0 ⇒ 那道闸永不生效。*/
    CHECK(YK_GAUGE_SHOOT_DELTA == 26);
    CHECK(yk_gauge_shoot_delta(1) == 26 && yk_gauge_shoot_delta(0) == -26);
    CHECK(yk_gauge_gate_blocks(1) == 1 && yk_gauge_gate_blocks(0) == 0);
    /* 染色五档（`AsciiManager.cpp:1035-1058`，顺序即优先级）。*/
    CHECK(yk_gauge_color(-9000) == 0x7070ffu);
    CHECK(yk_gauge_color(-3000) == 0xb0b0ffu);
    CHECK(yk_gauge_color(9000) == 0xff7070u);
    CHECK(yk_gauge_color(3000) == 0xffb0b0u);
    CHECK(yk_gauge_color(0) == 0xffffffu);
    CHECK(yk_gauge_color(-8000) == 0x7070ffu);      /* 极人优先于中人 */
    CHECK(yk_gauge_color(8000) == 0xff7070u);       /* 极妖优先于中妖 */
    /* HUD 几何（一手 `AsciiManager`，**640 空间的字面量**）：**输出一律表面 px** ——
     * `px = 1/to640 = 2`（TH18）。2026-09-22（P2）实跑定案：那批 VM 的脚本**显式写 `resolutionMode(2)`**
     * ⇒ `th18:0x4063d0` 的 mode 2 支 = `DAT × 0.5` = **×1**（= 1 单位 = 1 表面 px），写 640 空间的数会
     * 「位置高半个屏幕 + 整体缩小 50%」（owner th08 #14/#15）。全文见 `youkai_core.h` 的注。
     * 锚点 = **游玩区左下角**（表面 px）——2026-09-26（#39）改成一手换算的 `(257,928)`，
     * 所有 y = `ay − (464 − y640)·k`（464 = TH08 游玩区的下沿，见 `YK_GAUGE_640_FLOOR`）。*/
    {
        yk_gauge_hud_t h;
        float a[2];
        const float ax = 257.0f, ay = 928.0f;                       /* 表面 px（锚点直接用）*/
        const float k  = 2.0f;                                      /* 640 空间 → 表面 px */
        a[0] = 257.0f; a[1] = 928.0f;
        yk_gauge_hud(0, a, 0.5f, &h);
        CHECK(h.px == k);                                           /* ★ P2：倍数出口 = 2 */
        /* ★ 2026-09-22 晚订正：槽条的停稳 y = **449**（`ascii.anm` script5 的 `posTime` 终点），
         * 465 只是滑入动画的起点 ⇒ 与两个图标同高（原来写 465 ⇒ 条比图标低 16 px）。
         * ★ 2026-09-26（#39）：基准从 481 改成 464（TH08 游玩区下沿）⇒ 整块下移 34 px。*/
        CHECK(h.bar_x == ax && h.bar_y == ay - 15.0f * k);          /* 槽条 128×16，左上 ⇒ 表面 (257,898) */
        CHECK(h.bar_y == h.human_y);                                /* ★ 条与图标同高（一手 449）*/
        CHECK(h.human_x == ax && h.human_y == ay - 15.0f * k);      /* 人间图标在条左端（88−56−32=0） */
        CHECK(h.youkai_x == ax + 112.0f * k);                       /* 妖怪图标在条右端（88+56−32=112） */
        CHECK(h.cursor_x == ax + 64.0f * k);                        /* g=0 ⇒ 96−32=64 */
        CHECK(h.cursor_y == ay - 11.0f * k);
        CHECK(h.text_x == ax + 64.0f * k);
        CHECK(h.text_y == ay - 18.0f * k);
        CHECK(h.night_x == ax + 48.0f * k);                         /* 一手 x = 80 ⇒ 80 − 32 = 48 */
        CHECK(h.night_y == ay - 4.0f * k);
        /* 满妖侧：游标走到条右端（96+56−32 = 120）。*/
        yk_gauge_hud(10000, a, 0.5f, &h);
        CHECK(h.cursor_x == ax + 120.0f * k);
        CHECK(h.text_x == ax + 64.0f * k + 40.0f * k);
        /* 满人侧：游标走到条左端。*/
        yk_gauge_hud(-10000, a, 0.5f, &h);
        CHECK(h.cursor_x == ax + 8.0f * k);                         /* 96−56−32 = 8 */
        /* 越界输入按限值夹（引擎/卡状态给脏值时不跑到条外）。*/
        yk_gauge_hud(999999, a, 0.5f, &h);
        CHECK(h.cursor_x == ax + 120.0f * k);
        /* 锚点 NULL / to640 非法 ⇒ 锚点当 0、比例回默认 2（不崩）。*/
        yk_gauge_hud(0, 0, 0.5f, &h);
        CHECK(h.bar_x == 0.0f && h.bar_y == -(464.0f - 449.0f) * k);
        yk_gauge_hud(0, a, 0.0f, &h);
        CHECK(h.px == 2.0f && h.bar_x == 257.0f);                   /* to640 = 0 ⇒ 倍数回 2，锚点仍是表面 px */
        yk_gauge_hud(0, a, 0.5f, 0);                                /* out = NULL：什么都不做 */
    }

    /* ============ ★★ 人妖槽百分比数字的字形布局（一手 `th08:0x405e10`）============ */
    {
        yk_gauge_glyph_t g[YK_GAUGE_TEXT_MAX];
        int n, i;

        /* 0 ⇒ "0.00%"：5 个字形（1 整数 + '.' + 2 小号 + '%'）。*/
        n = yk_gauge_text_glyphs(0, g);
        CHECK(n == 5);
        CHECK(g[0].glyph == 0 && g[1].glyph == YK_GAUGE_GLYPH_DOT);
        CHECK(g[4].glyph == YK_GAUGE_GLYPH_PERCENT);
        CHECK(g[2].glyph == 0 && g[3].glyph == 0);
        CHECK(g[2].scale == 0.8f && g[3].scale == 0.8f);            /* 小数两位 = 小号 */
        CHECK(g[2].dy == 2.0f && g[3].dy == 2.0f);                  /* 且 y += 2（一手）*/
        CHECK(g[0].scale == 1.0f && g[4].scale == 1.0f && g[4].dy == 0.0f);
        /* 位数只影响居中：n = 4 + 1（<1000）⇒ xPos = 5*3.5 − 3.5 − 4 = 10 ⇒ dx0 = −10。*/
        CHECK(g[0].dx == -10.0f);
        /* 步进：整数 7、'.' 之后 5、小号 5、收尾 7（逐条抄 `DrawPercentage`）。*/
        CHECK(g[1].dx == g[0].dx + 7.0f);
        CHECK(g[2].dx == g[1].dx + 5.0f);
        CHECK(g[3].dx == g[2].dx + 5.0f);
        CHECK(g[4].dx == g[3].dx + 7.0f);

        /* 1234（= 12.34%）⇒ "12.34%"：6 个字形，整数两位。*/
        n = yk_gauge_text_glyphs(1234, g);
        CHECK(n == 6);
        CHECK(g[0].glyph == 1 && g[1].glyph == 2 && g[2].glyph == YK_GAUGE_GLYPH_DOT);
        CHECK(g[3].glyph == 3 && g[4].glyph == 4 && g[5].glyph == YK_GAUGE_GLYPH_PERCENT);
        CHECK(g[3].scale == 0.8f && g[4].scale == 0.8f);
        CHECK(g[0].dx == -((6 * 3.5f - 3.5f - 4.0f)));              /* n = 4 + 2 */

        /* 负值 ⇒ 前面多一个 '-'（步进 7），位数 +1。*/
        n = yk_gauge_text_glyphs(-1234, g);
        CHECK(n == 7);
        CHECK(g[0].glyph == YK_GAUGE_GLYPH_MINUS && g[0].scale == 1.0f && g[0].dy == 0.0f);
        CHECK(g[1].glyph == 1 && g[1].dx == g[0].dx + 7.0f);
        CHECK(g[6].glyph == YK_GAUGE_GLYPH_PERCENT);
        CHECK(g[0].dx == -((7 * 3.5f - 3.5f - 4.0f)));

        /* ±10000 ⇒ 固定 "100.00" / "-100.00"（一手：≥10000 那一支）。*/
        n = yk_gauge_text_glyphs(10000, g);
        CHECK(n == 7);                                              /* "100.00" + '%' */
        CHECK(g[0].glyph == 1 && g[1].glyph == 0 && g[2].glyph == 0);
        CHECK(g[3].glyph == YK_GAUGE_GLYPH_DOT);
        CHECK(g[4].glyph == 0 && g[5].glyph == 0);
        CHECK(g[4].scale == 0.8f && g[5].scale == 0.8f && g[5].dy == 2.0f);
        CHECK(g[6].glyph == YK_GAUGE_GLYPH_PERCENT);
        n = yk_gauge_text_glyphs(-10000, g);
        CHECK(n == YK_GAUGE_TEXT_MAX);                              /* 8 = '-' + "100.00" + '%' */
        CHECK(g[0].glyph == YK_GAUGE_GLYPH_MINUS && g[7].glyph == YK_GAUGE_GLYPH_PERCENT);
        /* 越界值按 ±10000 夹（引擎给脏值时不会算到数组外）。*/
        CHECK(yk_gauge_text_glyphs(999999, g) == 7);
        CHECK(yk_gauge_text_glyphs(-999999, g) == YK_GAUGE_TEXT_MAX);
        /* NULL 出口：只算位数、不崩。*/
        CHECK(yk_gauge_text_glyphs(0, 0) == 5);
        CHECK(yk_gauge_text_glyphs(12345, 0) == 7);
        /* 全定义域：返回 1..MAX 且 dx 单调、字形号在域内。*/
        for (i = -11000; i <= 11000; i += 137) {
            int k = yk_gauge_text_glyphs(i, g);
            int j;
            CHECK(k >= 1 && k <= YK_GAUGE_TEXT_MAX);
            for (j = 0; j < k; ++j) {
                CHECK(g[j].glyph <= YK_GAUGE_GLYPH_MINUS);
                CHECK(g[j].scale == 1.0f || g[j].scale == 0.8f);
                if (j) CHECK(g[j].dx > g[j - 1].dx);
            }
            CHECK(g[k - 1].glyph == YK_GAUGE_GLYPH_PERCENT);
        }
    }

    /* ============ §3.11 永点（刻符）→ PIV（一手 `th08:0x418220`）============ */
    {
        int32_t piv, orbs;
        /* 逐颗：+10 / 0 / +10 / 0 …（总数为奇数那颗 +10）⇒ 平均 +5/颗。*/
        yk_piv_reset(&piv, &orbs);
        CHECK(piv == 0 && orbs == 0);
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == 1 && piv == 10);
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == 2 && piv == 10);
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == 3 && piv == 20);
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == 4 && piv == 20);
        /* 一次给 2 颗：total = 6（偶）⇒ (2+0)/2×10 = 10。*/
        yk_piv_add(&piv, &orbs, 2);  CHECK(orbs == 6 && piv == 30);
        /* **负数不动 PIV**（`th08:0x418288`/`:0x41828c jle`），只动计数。*/
        yk_piv_add(&piv, &orbs, -15);
        CHECK(orbs == -9 && piv == 30);
        /* 再收一颗：total = −8（偶）⇒ (1+0)/2 = 0 ⇒ 不加。*/
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == -8 && piv == 30);
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == -7 && piv == 40);  /* total 奇 ⇒ +10 */
        /* SC 捕获那份额外账：`count × 10`，独立加（`th08:0x41732c`）。*/
        yk_piv_spell_bonus(&piv, 7); CHECK(piv == 110);
        yk_piv_spell_bonus(&piv, 0); CHECK(piv == 110);               /* 非正不加 */
        yk_piv_spell_bonus(&piv, -3);CHECK(piv == 110);
        /* 刻符的道具分值（`th08:0x4412c3`–`0x4412ee`）：本关蓝点 ≥ 2000 ⇒ 满值 10000。*/
        CHECK(yk_orb_score(2000, 0) == 10000);
        CHECK(yk_orb_score(5000, 10) == 10000);
        CHECK(yk_orb_score(0, 0) == 100);            /* (0/2)*10 = 0 ⇒ 下限 100 */
        CHECK(yk_orb_score(0, 100) == 500);          /* (100/2)*10 */
        CHECK(yk_orb_score(1999, 4) == 100);         /* 没到门槛 + 值太小 ⇒ 下限 */
        CHECK(yk_orb_score(0, 20000) == 100000);     /* 门槛按**本关**算，本局计数照给 */

        /* ★★ 2026-09-22 晚（owner 第 4 条）+ ★★★ 2026-09-22（P5 #16b）：刻符的**运动常量**。
         * 归航速度 = `[.sht+0x14]`（TH08 一手 10/12，**未改**）；
         * 拾取半径 = **TH18 自己那一档 70**（`th18:0x45ae7b`/`:0x45ae85`；原生 22 号卡 ITEM_LINE = 110）
         * —— owner 2026-09-22 拍的 **TH18 适配**（要「全屏吸收那种速度感」），
         * **不是** TH08 一手值（一手 = `[.sht+0x18]/2 + 8` = 20/24，见 `youkai_core.c` 的注）。*/
        {
            float sp = 0.0f, r = 0.0f;
            int c;
            yk_orb_motion_params(0, &sp, &r);
            CHECK(sp == 10.0f && r == 20.0f);                     /* pl00as：10.0 / 24/2+8（一手）*/
            yk_orb_motion_params(1, &sp, &r);
            CHECK(sp == 12.0f && r == 24.0f);                     /* pl01as：12.0 / 32/2+8（一手）*/
            yk_orb_motion_params(2, &sp, &r);
            CHECK(sp == 10.0f && r == 20.0f);                     /* pl02as */
            yk_orb_motion_params(3, &sp, &r);
            CHECK(sp == 10.0f && r == 20.0f);                     /* pl03as */
            yk_orb_motion_params(-1, &sp, &r);                    /* 越界 ⇒ 0 号那一档 */
            CHECK(sp == 10.0f && r == 20.0f);
            for (c = -2; c <= 6; ++c) {                           /* 全定义域不崩、值恒在 {10,12}/{70} */
                yk_orb_motion_params(c, &sp, &r);
                CHECK((sp == 10.0f || sp == 12.0f) && (r == 20.0f || r == 24.0f));
            }
            sp = 7.0f; r = 7.0f;
            yk_orb_motion_params(0, &sp, 0);                      /* 单出口 */
            CHECK(sp == 10.0f && r == 7.0f);
            yk_orb_motion_params(0, 0, &r);
            CHECK(r == 20.0f);
            yk_orb_motion_params(0, 0, 0);                        /* 双 NULL：什么都不做、不崩 */
            /* ★★★ 2026-09-26（#41「从敌机铺出去的范围太大」）：**击破批的落点算式**
             * 一手（`th08:0x42aff0`–`0x42b048`）= 圆盘内均匀撒：
             *   `angle = rand_signed(π) ∈ [−π, π)`、`radius = rand(2·count) ∈ [0, 2·count)`。
             * 旧实现是**固定半径 128 的环** ⇒ 12 颗时是圆盘上界（24）的 5.3 倍。*/
            {
                float a = 9.0f, rad = 9.0f;
                const float PI = 3.14159265f;
                yk_orb_burst_polar(12, 0.0f, 0.0f, &a, &rad);     /* r=0 ⇒ 角 = −π、半径 0 */
                CHECK(a == -PI && rad == 0.0f);
                yk_orb_burst_polar(12, 0.5f, 0.5f, &a, &rad);     /* 中值 ⇒ 角 0、半径 = count */
                CHECK(a == 0.0f && rad == 12.0f);
                yk_orb_burst_polar(12, 1.0f, 1.0f, &a, &rad);     /* 上界 ⇒ 角 π、半径 2·count */
                CHECK(a == PI && rad == 24.0f);
                yk_orb_burst_polar(12, 0.25f, 0.1f, &a, &rad);    /* 角 = −π/2（正下方）*/
                CHECK(rad == 0.1f * 24.0f);
                CHECK(a < 0.0f && a > -PI);                       /* ★ 恒在 [−π, π) 内 */
                yk_orb_burst_polar(0, 0.9f, 1.0f, &a, &rad);      /* count = 0 ⇒ 半径 0（不崩）*/
                CHECK(rad == 0.0f);
                yk_orb_burst_polar(-3, 0.9f, 1.0f, &a, &rad);     /* 负 count 也夹到 0 */
                CHECK(rad == 0.0f);
                yk_orb_burst_polar(12, 0.3f, 0.7f, &a, 0);        /* 单出口 */
                CHECK(a < 0.0f && a > -PI);
                yk_orb_burst_polar(12, 0.3f, 0.7f, 0, &rad);
                CHECK(rad > 0.0f);
                yk_orb_burst_polar(12, 0.3f, 0.7f, 0, 0);         /* 双 NULL：不崩 */
                for (a = 0.0f; a < 1.0f; a += 0.125f) {           /* 半径上界恒 = 2·count */
                    float t = 0.0f;
                    yk_orb_burst_polar(26, 0.5f, a, 0, &t);
                    CHECK(t >= 0.0f && t < 52.0f);
                }
            }
            /* ★★★ 2026-09-22（#25 击破符卡的螺旋）：一手参数（`engine/item/th08/01-time-orbs.md` §5.3）
             * 角度闭式 `θ(t) = ((t − 10) × 2π/40) − π/2`：`t = 10` 正上方（−π/2）、每 +1 帧 +9°（2π/40）。*/
            {
                float a = 0.0f;
                const float D = 0.15707963268f;            /* 2π/40 = 9° */
                yk_spiral_angle(10, &a);
                CHECK(a > -1.5708f - 1e-4f && a < -1.5708f + 1e-4f);      /* t = 10 ⇒ 正上方 */
                yk_spiral_angle(11, &a);
                CHECK(a > (-1.5708f + D) - 1e-3f && a < (-1.5708f + D) + 1e-3f);
                yk_spiral_angle(50, &a);                                   /* 40 帧一圈 ⇒ 回到正上方 */
                CHECK(a > -1.5708f - 1e-3f && a < -1.5708f + 1e-3f);
                yk_spiral_angle(0, 0);                                     /* NULL 出口不崩 */
                CHECK(YK_SPIRAL_BATCH_FRAMES == 8 && YK_SPIRAL_FIRST_MAX == 7 &&
                      YK_SPIRAL_SECOND_N == 6 && YK_SPIRAL_RADIUS == 128.0f);
                CHECK(yk_spiral_first_count(1000) == 7);                   /* min(额度, 7) */
                CHECK(yk_spiral_first_count(7) == 7);
                CHECK(yk_spiral_first_count(3) == 3);
                CHECK(yk_spiral_first_count(0) == 0 && yk_spiral_first_count(-5) == 0);
            }
            /* 一手的状态号与常量（改了要在注释里同步改一手出处）*/
            CHECK(YK_ORB_STATE_FALL == 0 && YK_ORB_STATE_HOMING == 1 && YK_ORB_STATE_SPAWN == 3);
            CHECK(YK_ORB_FALL_GRAVITY == 0.03f);                  /* `ds:0x4b44c0` */
            CHECK(YK_ORB_FALL_TERMINAL == 3.0f);                  /* `0x40400000` */
            CHECK(YK_ORB_FALL_VY_MIN == -2.2f);                   /* `ds:0x4b5b2c` */
            CHECK(YK_ORB_LOST_Y == 464.0f);                       /* `arcadeRegionSize.y (448) + 16` */
            /* ★★★ 2026-09-22（owner #26 · 风味选项）：决死窗口 + 染色的两条不变量。
             * 判据与一手见 `youkai_core.h` 的同名小节（窗口 = `player+0x476ac == 4`，
             * 一手 `th18:0x45d4e4` 全 exe 唯一写点）。*/
            CHECK(YK_DEATHBOMB_STATE == 4);
            CHECK(yk_deathbomb_window(4) == 1);
            CHECK(yk_deathbomb_window(0) == 0 && yk_deathbomb_window(1) == 0 &&
                  yk_deathbomb_window(3) == 0 && yk_deathbomb_window(5) == 0);
            CHECK(yk_deathbomb_window(-1) == 0);
            /* 染色的两条不变量：只换 RGB + alpha 原样；还原 = 当前 alpha + 快照 RGB。*/
            CHECK(YK_TINT_RED == 0xffff0000u);
            CHECK(yk_tint_apply(0xffffffffu) == 0xffff0000u);   /* 纯红 */
            CHECK(yk_tint_apply(0x80aabbccu) == 0x80ff0000u);        /* alpha 0x80 原样保留 */
            CHECK(yk_tint_apply(0x00112233u) == 0x00ff0000u);
            CHECK(yk_tint_restore(0x40ff0000u, 0x80aabbccu) == 0x40aabbccu);  /* 取当前 alpha */
            CHECK(yk_tint_restore(0xffffffffu, 0x80aabbccu) == 0xffaabbccu);
            /* 染了再还原 = 回到原色（除 alpha 用的是「当前」那一份 —— 引擎每帧会重设颜色）。*/
            CHECK(yk_tint_restore(yk_tint_apply(0xffffffffu), 0xffffffffu) == 0xffffffffu);
            /* ★★★ 2026-09-22 夜（owner 工单 P-A · 夜点）：初值四档 + 蓝点分（TH08 一手算式）。
             * 一手：`th08:0x43af1b` 的 switch（60000/100000/200000/300000/300000）；
             *       `Item::CollectPoint`（PoC 线以上 = 满值；以下 = 夜点/2 − 距离×(夜点/1000)，十位取整）。*/
            CHECK(YK_NIGHT_POINT_GRADES == 5);
            CHECK(yk_night_point_init(0) == 60000);
            CHECK(yk_night_point_init(1) == 100000);
            CHECK(yk_night_point_init(2) == 200000);
            CHECK(yk_night_point_init(3) == 300000);
            CHECK(yk_night_point_init(4) == 300000);          /* Extra（`th08:0x43af85`）*/
            CHECK(yk_night_point_init(-1) == 300000 && yk_night_point_init(99) == 300000);  /* 越界取最高档 */
            /* 蓝点分：线以上 / isMaxValue ⇒ 满值（十位取整不改它，因为四档初值都是 10 的倍数）*/
            CHECK(yk_blue_point_score(100000, 0, 0) == 100000);
            CHECK(yk_blue_point_score(100000, -5, 0) == 100000);
            CHECK(yk_blue_point_score(100000, 123, 1) == 100000);
            /* 线以下：100000/2 − 123×(100000/1000) = 50000 − 12300 = 37700（十位取整后同值）*/
            CHECK(yk_blue_point_score(100000, 123, 0) == 37700);
            /* 距离大到把值压成负 ⇒ 我们夹 0（一手没有夹零，登记 ❓ 自定）*/
            CHECK(yk_blue_point_score(100000, 100000, 0) == 0);
            CHECK(yk_blue_point_score(0, 10, 0) == 0);
            /* 十位取整：60000/2 − 7×(60000/1000) = 30000 − 420 = 29580（已是 10 的倍数）*/
            CHECK(yk_blue_point_score(60000, 7, 0) == 29580);
            /* ★★★ 2026-09-22 夜（P-B 逢魔）：门（两条都含等号、取或）+ 击破掉几颗刻符
             * 一手：`th08:0x406d10`（极人 `g <= −8000`，`setle`）/ `th08:0x406d70`（极妖 `g >= +8000`，`setge`）；
             *       `th08:0x42da41`（逢魔 ⇒ 1 颗）；`th08:0x45187f`（人间逢魔 + 自机弹击杀 ⇒ 再 1 颗）。*/
            CHECK(YK_FENGMA_SCORE_PER_FRAME == 100);
            CHECK(yk_fengma(-8000, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 1);   /* 含等号 */
            CHECK(yk_fengma(8000, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 1);
            CHECK(yk_fengma(-8001, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 1);   /* 更极端也算 */
            CHECK(yk_fengma(-7999, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 0);
            CHECK(yk_fengma(7999, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 0);
            CHECK(yk_fengma(0, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 0);
            /* 妖梦人型那种「人侧只要 −3000」的阈值也走同一个函数（阈值是参数）*/
            CHECK(yk_fengma(-3000, -3000, 8000) == 1 && yk_fengma(-2999, -3000, 8000) == 0);
            /* 击破掉几颗：非逢魔 0；逢魔 1；人间逢魔 + 自机弹 2（妖怪逢魔恒 1）*/
            CHECK(yk_fengma_kill_orbs(0, 1, 1) == 0);
            CHECK(yk_fengma_kill_orbs(1, 0, 0) == 1);
            CHECK(yk_fengma_kill_orbs(1, 0, 1) == 1);          /* 妖怪逢魔：自机弹也只 1 颗 */
            CHECK(yk_fengma_kill_orbs(1, 1, 0) == 1);
            CHECK(yk_fengma_kill_orbs(1, 1, 1) == 2);
            /* ★★★ 2026-09-22 夜（P-E 单人反向封顶）：一手 `Player::on_registration`
             * —— 单人（`th08:0x44da99` / `:0x44dac4` 的两个门）把**反侧 limit** 收成 ±2000
             * （`th08:0x44daa2` = +0x7d0 / `th08:0x44dacd` = −0xf830），effects 不动 ⇒ 反侧逢魔不可达。*/
            {
                int32_t hl = 0, yl = 0;
                yk_gauge_limits_solo(1, &hl, &yl);            /* 妖侧（低速）⇒ 人侧封在 −2000 */
                CHECK(hl == -YK_GAUGE_SOLO_OTHER_LIMIT && hl == -2000);
                CHECK(yl == YK_GAUGE_YOUKAI_LIMIT);
                yk_gauge_limits_solo(0, &hl, &yl);            /* 人侧 ⇒ 妖侧封在 +2000 */
                CHECK(hl == YK_GAUGE_HUMAN_LIMIT);
                CHECK(yl == YK_GAUGE_SOLO_OTHER_LIMIT && yl == 2000);
                yk_gauge_limits_solo(1, 0, 0);                /* NULL 安全 */
                /* 反侧真的到不了逢魔线：妖侧形态下，往人侧推到底也只能到 −2000 */
                yk_gauge_limits_solo(1, &hl, &yl);
                CHECK(yk_gauge_add2(-1900, -200, hl, yl) == -2000);       /* 夹在反侧 limit */
                CHECK(yk_gauge_add2(-2000, -100, hl, yl) == -2000);
                CHECK(yk_fengma(yk_gauge_add2(-2000, -100, hl, yl),
                                YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 0);  /* 反侧逢魔不可达 */
                CHECK(yk_gauge_add2(9900, 500, hl, yl) == 10000);         /* 正侧仍到 100% */
                CHECK(yk_fengma(yk_gauge_add2(7900, 200, hl, yl),
                                YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 1);  /* 正侧可逢魔 */
                /* `clamp2` 与旧的 `clamp` 在满量程时逐点一致（回归保护）*/
                CHECK(yk_gauge_clamp2(-99999, YK_GAUGE_HUMAN_LIMIT, YK_GAUGE_YOUKAI_LIMIT) ==
                      yk_gauge_clamp(-99999));
                CHECK(yk_gauge_clamp2(99999, YK_GAUGE_HUMAN_LIMIT, YK_GAUGE_YOUKAI_LIMIT) ==
                      yk_gauge_clamp(99999));
                CHECK(yk_gauge_clamp2(1234, YK_GAUGE_HUMAN_LIMIT, YK_GAUGE_YOUKAI_LIMIT) == 1234);
                /* ★★★ 2026-09-27（owner ③「按/松 shift 不得让人妖值瞬间变化」）：
                 * **夹取不许回收已经在区间外的值** —— 换形那一帧限值本身换了，若照旧直接夹，
                 * 停在 −5000 的槽会被当帧拉成 −2000（+30% 的瞬跳）；一手没有任何一处这么做。*/
                yk_gauge_limits_solo(1, &hl, &yl);        /* 妖侧：区间 = [−2000, +10000] */
                CHECK(yk_gauge_add2(-5000, 0, hl, yl) == -5000);          /* ★ 换形当帧：一点不动 */
                CHECK(yk_gauge_add2(-5000, +20, hl, yl) == -4980);        /* 只按 delta 走，不弹回 −2000 */
                CHECK(yk_gauge_add2(-1900, -200, hl, yl) == -2000);       /* 区间内照旧夹（回归）*/
                CHECK(yk_gauge_add2(-2100, -200, hl, yl) == -2300);       /* 区间外 ⇒ 不回收（同上）*/
                yk_gauge_limits_solo(0, &hl, &yl);        /* 人侧：区间 = [−10000, +2000] */
                CHECK(yk_gauge_add2(5000, 0, hl, yl) == 5000);            /* ★ 松 Shift 也不瞬跳 */
                CHECK(yk_gauge_add2(5000, -20, hl, yl) == 4980);
                CHECK(yk_gauge_add2(1900, 200, hl, yl) == 2000);          /* 区间内照旧夹 */
                /* 换形空窗常量（一手 `th08:0x44be05` 的 `cmp …,0x1e`）*/
                CHECK(YK_GAUGE_SHIFT_BLANK == 30);
            }
            /* ★★★ 2026-09-27（owner ⑥）：**我们自己那条激光行**的判据。
             * ★ 第二十六批起它**不再用于收束**（引擎不发这一行了，见下）—— 留着当**运行期判据**：
             *   探针用它数「这一行的引擎弹有几发」，必须恒 0。*/
            CHECK(yk_marisa_laser_row(0x59, 0) == 1);
            CHECK(yk_marisa_laser_row(0x5d, 0) == 1);
            CHECK(yk_marisa_laser_row(0x59, 1) == 0);      /* 组内第 1 行 = 本体行（不是激光）*/
            CHECK(yk_marisa_laser_row(0x58, 0) == 0);      /* 霊夢的组：不是我们的激光 */
            CHECK(yk_marisa_laser_row(0x5e, 0) == 0);      /* 咲夜 */
            CHECK(yk_marisa_laser_row(0x63, 0) == 0);      /* 早苗 */
            CHECK(yk_marisa_laser_row(0x16, 0) == 0);      /* 兜底组 */
            CHECK(YK_LASER_LEN == 448.0f);                 /* 一手 TH08 的恒定束长 */
            CHECK(YK_CHAR_MARISA == 1);                    /* 机体表（`CE_ADDR_CHARACTER` 一手）*/
            /* ★★★ 2026-09-27 第二十六批：**卡侧矩形束**的四个纯函数（一手见 `youkai_core.h`）。
             * 宽度 = 一手 TH08 `player01.anm` script24..28 的 `scale.y` × 贴图高 32
             *   （16 / 16 / 19.2 / 22.4 / 25.6 px；×100 存整数避免 float 表）。*/
            CHECK(yk_m_laser_beam_w_px100(0) == 1600);
            CHECK(yk_m_laser_beam_w_px100(1) == 1600);
            CHECK(yk_m_laser_beam_w_px100(2) == 1920);
            CHECK(yk_m_laser_beam_w_px100(3) == 2240);
            CHECK(yk_m_laser_beam_w_px100(4) == 2560);
            CHECK(yk_m_laser_beam_w_px100(-1) == 1600);           /* 越界钳进表内（不读表外）*/
            CHECK(yk_m_laser_beam_w_px100(99) == 2560);
            /* 伤害 = `round(2 × 该档对齐后的激光行伤害 / 该行的 rate)`（= 本卡既有 DPS 模型里那一行的份额 × 2）
             * ⇒ 平均每帧 = 7.0 / 4.5 / 3.5 / 3.0 / 2.5（生成器逐档算 + 回读对账，见 `append_shooterset.py`）。*/
            CHECK(yk_m_laser_beam_damage(0) == 14);
            CHECK(yk_m_laser_beam_damage(1) == 9);
            CHECK(yk_m_laser_beam_damage(2) == 7);
            CHECK(yk_m_laser_beam_damage(3) == 6);
            CHECK(yk_m_laser_beam_damage(4) == 5);
            CHECK(yk_m_laser_beam_damage(-5) == 14);
            CHECK(yk_m_laser_beam_damage(5) == 5);
            /* 结算节拍 = 每 2 帧一笔（一手 TH08 `th08:0x4517a1` 每 2 帧结算一次）*/
            CHECK(YK_M_LASER_BEAM_SETTLE == 2);
            CHECK(yk_m_laser_beam_should_settle(0) == 1);         /* 出生那一帧就结算（age 从 0 起）*/
            CHECK(yk_m_laser_beam_should_settle(1) == 0);
            CHECK(yk_m_laser_beam_should_settle(2) == 1);
            CHECK(yk_m_laser_beam_should_settle(3) == 0);
            CHECK(yk_m_laser_beam_should_settle(120) == 1);
            /* 展开系数：头 8 帧 0→1000 线性，之后恒 1000（117 的「如收回般倒放展开」，
             * 帧数 = 束脚本 `interruptLabel(1)` 的 `scaleTime(8, 1, 1.0, 0.0)`）。*/
            CHECK(YK_M_LASER_BEAM_GROW_FRAMES == 8);
            CHECK(yk_m_laser_beam_grow_permille(0) == 0);
            CHECK(yk_m_laser_beam_grow_permille(1) == 125);
            CHECK(yk_m_laser_beam_grow_permille(4) == 500);
            CHECK(yk_m_laser_beam_grow_permille(7) == 875);
            CHECK(yk_m_laser_beam_grow_permille(8) == 1000);
            CHECK(yk_m_laser_beam_grow_permille(9999) == 1000);   /* 不溢出、不回头 */
            /* 束的条数上限 = 每台我们的子机一条 + 回退本体的那一条（117 的同款形状）*/
            CHECK(YK_M_LASER_BEAM_MAX == YK_OPTION_CELLS + 1);
            /* 贴图逻辑尺寸（`YKB_M_OPT.png` = 256×32；`scale.x = 448/256`、`scale.y = w/32`）*/
            CHECK(YK_M_LASER_BEAM_SPRITE_W == 256.0f);
            CHECK(YK_M_LASER_BEAM_SPRITE_H == 32.0f);
            /* ★★★ 2026-09-27（owner ④）：**妖逢魔染色**的两个纯函数。
             * 一手：只有霊夢（`ply00as`）与魔理沙（`ply01as`）的行 `+0x1e > 0` ⇒ 只有这两位的妖侧组染红。*/
            CHECK(yk_fengma_tint_set(0x54) == 1);            /* 霊夢 A0..A4 */
            CHECK(yk_fengma_tint_set(0x58) == 1);
            CHECK(yk_fengma_tint_set(0x59) == 1);            /* 魔理沙 M0..M4 */
            CHECK(yk_fengma_tint_set(0x5d) == 1);
            CHECK(yk_fengma_tint_set(0x5e) == 0);            /* 咲夜：一手全是 0 ⇒ 不染 */
            CHECK(yk_fengma_tint_set(0x62) == 0);
            CHECK(yk_fengma_tint_set(0x63) == 0);            /* 早苗（TH16 数据）⇒ 一手口径下不染 */
            CHECK(yk_fengma_tint_set(0x67) == 0);
            CHECK(yk_fengma_tint_set(0x16) == 0);            /* 兜底组（幽々子）⇒ 不染 */
            /* 染色 dword：只换低 24 位（BGR），**alpha 必须原样**（一手只写 `+0x1f0/1/2` 三个字节）*/
            CHECK(yk_fengma_tint_dword(0x80000000u) == 0x80ff4040u);
            CHECK(yk_fengma_tint_dword(0x00abcdefu) == 0x00ff4040u);
            CHECK(yk_fengma_tint_dword(0xff123456u) == 0xffff4040u);
            /* TH18 的 dword 是 `0xAARRGGBB` ⇒ 低字节 B=0x40、中 G=0x40、高 R=0xff = RGB(255,64,64) 红 */
            CHECK((YK_FENGMA_TINT_RGB & 0xffu) == 0x40u);
            CHECK(((YK_FENGMA_TINT_RGB >> 8) & 0xffu) == 0x40u);
            CHECK(((YK_FENGMA_TINT_RGB >> 16) & 0xffu) == 0xffu);
            /* ★★★ 2026-09-22 夜（#27 弹字）：距离 alpha 三段 + 十进制字形。
             * 一手：`th08:0x4054e3`-`0x405545` / `:0x405983`-`0x4059f1`（`d = dx²+dy²` 整数）。*/
            CHECK(YK_POPUP_SCORE_LIFE == 60 && YK_POPUP_TIME_LIFE == 90);
            CHECK(YK_POPUP_GLYPH_PLUS == 13);
            CHECK(yk_popup_alpha(0, 0) == 0x50);                  /* d = 0 ⇒ 0x50 */
            CHECK(yk_popup_alpha(32, 0) == 0x50);                 /* d = 0x400 ⇒ 仍 0x50（不 > 0x400）*/
            CHECK(yk_popup_alpha(33, 0) == 0x50 + (0x41 * 128) / 0xc00);   /* 刚过 0x400 */
            CHECK(yk_popup_alpha(64, 0) == 0xD0);                 /* d = 0x1000 ⇒ > 0x400 支；0x1000 不 > 0x1000 */
            CHECK(yk_popup_alpha(65, 0) == 0xD0 || yk_popup_alpha(65, 0) > 0x50);
            CHECK(yk_popup_alpha(1000, 1000) == 0xD0);            /* 远处 ⇒ 满 alpha 上限 */
            {
                uint8_t g[6];
                CHECK(yk_popup_digits(1, g, 6) == 1 && g[0] == 1);
                CHECK(yk_popup_digits(10000, g, 6) == 5 && g[0] == 1 && g[4] == 0);
                CHECK(yk_popup_digits(12345, g, 3) == 3 && g[0] == 1 && g[1] == 2 && g[2] == 3);
                CHECK(yk_popup_digits(0, g, 6) == 1 && g[0] == 0);
                CHECK(yk_popup_digits(-5, g, 6) == 1 && g[0] == 0);
                CHECK(yk_popup_digits(7, 0, 6) == 0);              /* NULL 出口：不写、不崩 */
                CHECK(yk_popup_digits(7, g, 0) == 0);              /* 容量 0：不写 */
            /* ★★★ 2026-09-22 夜（P-C 订正）：**时间斜坡**（一手 `th08:0x44be67`–`0x44bea8`）
             * 与**回中三档**（一手 `th08:0x44bf70`–`0x44bfde`）。*/
            CHECK(YK_GAUGE_RAMP_DIV == 15 && YK_GAUGE_RAMP_CAP == 21 && YK_GAUGE_RAMP_FRAMES == 300);
            CHECK(YK_GAUGE_IDLE_MAX == 30 && YK_GAUGE_STOP_CLEAR == 4);
            CHECK(yk_gauge_ramp_delta(0) == 0);
            CHECK(yk_gauge_ramp_delta(14) == 0);
            CHECK(yk_gauge_ramp_delta(15) == 1);
            CHECK(yk_gauge_ramp_delta(29) == 1);
            CHECK(yk_gauge_ramp_delta(30) == 2);
            CHECK(yk_gauge_ramp_delta(300) == 20);
            CHECK(yk_gauge_ramp_delta(301) == 20);            /* 一手：先夹到 300 再 /15 */
            CHECK(yk_gauge_ramp_delta(315) == 20);          /* 帧数先夹 300 ⇒ 最大 20（21 那个 imm 是
                                                              * float→int 的转换上界，实达不到）*/
            CHECK(yk_gauge_ramp_delta(99999) == 20);          /* 帧数上限 300 ⇒ 实际峰值 20 */
            CHECK(yk_gauge_ramp_delta(-5) == 0);              /* 负帧数按 0 */
            /* 回中三档（带符号）：极妖 ⇒ −5、中妖 ⇒ −3、正侧 ⇒ −2、负侧 ⇒ +2/+3/+5 */
            CHECK(yk_gauge_center_delta(9000) == -5);
            CHECK(yk_gauge_center_delta(8000) == -5);         /* 含等号（setge）*/
            CHECK(yk_gauge_center_delta(2500) == -3);
            CHECK(yk_gauge_center_delta(2000) == -3);
            CHECK(yk_gauge_center_delta(500) == -2);
            CHECK(yk_gauge_center_delta(-500) == 2);
            CHECK(yk_gauge_center_delta(-2500) == 3);
            CHECK(yk_gauge_center_delta(-9000) == 5);
            /* ★★★ 2026-09-22 夜（P-D 缩水版）：击破魔使 = −gauge/12（一手 `th08:0x42b216 mov ecx,0xc`）
             * 与 50 帧静默常量（一手 `th08:0x42b240`）。*/
            CHECK(YK_FAM_KILL_GAUGE_DIV == 12 && YK_ORB_SILENCE_FRAMES == 50);
            CHECK(yk_gauge_fam_kill_delta(12000) == -1000);   /* 夹紧后的量程内：−12000/12 */
            CHECK(yk_gauge_fam_kill_delta(10000) == -833);    /* 一手 `idiv` 向零截断（不是 −834）*/
            CHECK(yk_gauge_fam_kill_delta(8000) == -666);
            CHECK(yk_gauge_fam_kill_delta(2000) == -166);
            CHECK(yk_gauge_fam_kill_delta(12) == -1);
            CHECK(yk_gauge_fam_kill_delta(11) == 0);          /* 向零截断 ⇒ 11/12 = 0 */
            CHECK(yk_gauge_fam_kill_delta(-10000) == 833);    /* 人侧反向同式 */
            CHECK(yk_gauge_fam_kill_delta(0) == 0);
            /* ★★★ 2026-09-22 夜（#27/#28）：**夜点数字的字形**（一手 `th08:0x405d11`–`0x405e01`：
             * 左对齐、7 px/字、**前导零不画也不占位**）。*/
            {
                yk_gauge_glyph_t ng[YK_NIGHT_TEXT_MAX];
                int n2;
                CHECK(YK_NIGHT_TEXT_MAX == 8);
                n2 = yk_night_text_glyphs(60000, ng);
                /* ★ 2026-09-27（§BE #121）：补前导零到 6 位 ⇒ 60000 变成 `060000`（旧断言「5 位」作废）*/
                CHECK(n2 == (int)YK_NIGHT_TEXT_DIGITS);
                CHECK(ng[0].glyph == 0 && ng[1].glyph == 6 && ng[5].glyph == 0);
                CHECK(ng[1].dx == 7.0f && ng[5].dx == 35.0f);             /* 每字 +7（一手 `pos.x += 7.0`）*/
                CHECK(ng[0].dy == 0.0f && ng[0].scale == 1.0f);
                n2 = yk_night_text_glyphs(300000, ng);
                CHECK(n2 == 6 && ng[0].glyph == 3);
                /* ★ 2026-09-27（§BE #121，owner 口径）：**改成补前导零到 6 位**（按「额外最大得点」
                 *   的画法）⇒ 下面三条旧断言（前导零不占位 / 0 只画一位）已作废并改掉。*/
                n2 = yk_night_text_glyphs(123456789, ng);                 /* 超过 8 位 ⇒ 留高位、截 8 位 */
                CHECK(n2 == 8 && ng[0].glyph == 1 && ng[7].glyph == 8);
                n2 = yk_night_text_glyphs(0, ng);
                CHECK(n2 == (int)YK_NIGHT_TEXT_DIGITS && ng[0].glyph == 0 && ng[5].glyph == 0);
                n2 = yk_night_text_glyphs(-5, ng);
                CHECK(n2 == (int)YK_NIGHT_TEXT_DIGITS && ng[0].glyph == 0 && ng[5].glyph == 0);
                CHECK(yk_night_text_glyphs(60000, 0) >= 1);               /* NULL 出口不崩 */
            /* ★★★ 2026-09-22 夜（批次 3）：人侧撞使魔 = 圆 vs 圆
             * （一手：自机判定半径 3.0 @`player+0x4799c`；使魔 hurtbox 半宽 12.0 = 默认 24 的一半）。*/
            CHECK(YK_FAM_HIT_REACH == YK_FAM_HURTBOX_HALF && YK_FAM_HIT_REACH == 12.0f);
            CHECK(yk_fam_hit_player(0.0f, 0.0f, 3.0f) == 1);           /* 正中 */
            CHECK(yk_fam_hit_player(14.9f, 0.0f, 3.0f) == 1);          /* 3.0 + 12.0 = 15.0 内 */
            CHECK(yk_fam_hit_player(15.1f, 0.0f, 3.0f) == 0);          /* 刚好出界 */
            CHECK(yk_fam_hit_player(10.0f, 10.0f, 3.0f) == 1);         /* √200 ≈ 14.1 < 15 */
            CHECK(yk_fam_hit_player(11.0f, 11.0f, 3.0f) == 0);         /* √242 ≈ 15.6 > 15 */
            CHECK(yk_fam_hit_player(1.0f, 1.0f, 0.0f) == 1);           /* 自机半径 0 也按 hurtbox 判 */
            CHECK(yk_fam_hit_player(20.0f, 0.0f, 0.0f) == 0);
            CHECK(yk_fam_hit_player(1.0f, 0.0f, -5.0f) == 1);          /* 负半径：仍按 hurtbox（r>0）*/
            /* ★★★ 2026-09-22 夜（Time 第三道门）：抓取盒的玩家状态白名单。
             * 一手 TH08 = {0 ALIVE, 3 INVULNERABLE, 4 BORDER} 放行（`th08:0x44a5c1`/`:0x44a5c9`/`:0x44a5d4`）；
             * TH18 等位映射 = {1 存活, 3 无敌/复活, 4 决死窗口}（移动只在 case 1 `th18:0x45c0cc`）。*/
            CHECK(YK_ITEM_BOX_STATE_INIT == 0 && YK_ITEM_BOX_STATE_ALIVE == 1 &&
                  YK_ITEM_BOX_STATE_DEAD == 2 && YK_ITEM_BOX_STATE_INVULN == 3 &&
                  YK_ITEM_BOX_STATE_BORDER == 4);
            CHECK(yk_item_box_open(1) == 1);                      /* 存活 */
            CHECK(yk_item_box_open(3) == 1);                      /* 无敌/复活 */
            CHECK(yk_item_box_open(4) == 1);                      /* 决死窗口 */
            CHECK(yk_item_box_open(0) == 0);                      /* 初生/未就位 ⇒ 关 */
            CHECK(yk_item_box_open(2) == 0);                      /* 死亡段 ⇒ 关 */
            CHECK(yk_item_box_open(-1) == 0 && yk_item_box_open(99) == 0);   /* 野值一律关 */
            /* ★★★ 2026-09-22 夜（P-F · 擦弹的**颗数**）：一手 `Player::sub_44a930`（`th08:0x44a930`）
             * 三处 `spawn_item(pos, type 0xa, state 1)`（`th08:0x44aac9` / `:0x44aaef` / `:0x44ab0f`），
             * 各道门**嵌套** ⇒ 每擦到一颗弹 **0 / 1 / 2 / 3** 颗。逐指令与门语义全文 = `youkai_core.h` §3.11c。
             * 这里**每道门各测正 / 反两个方向**（入参正交地把门打开 / 关上）。*/
            /* 两条 shottype 谓词（一手 `th08:0x42f230` / `:0x42f270`：`>= 4` 才算单人，再取奇偶）。*/
            CHECK(yk_shottype_is_solo_human(0) == 0 && yk_shottype_is_solo_human(3) == 0);
            CHECK(yk_shottype_is_solo_human(4) == 1 && yk_shottype_is_solo_human(10) == 1);
            CHECK(yk_shottype_is_solo_human(5) == 0 && yk_shottype_is_solo_human(11) == 0);
            CHECK(yk_shottype_is_solo_youkai(5) == 1 && yk_shottype_is_solo_youkai(11) == 1);
            CHECK(yk_shottype_is_solo_youkai(4) == 0 && yk_shottype_is_solo_youkai(10) == 0);
            CHECK(yk_shottype_is_solo_youkai(0) == 0 && yk_shottype_is_solo_youkai(3) == 0);
            CHECK(yk_shottype_is_solo_human(4) != yk_shottype_is_solo_youkai(4));   /* 互斥 */
            /* ---- 第 1 颗前面那三条**场上门**（★ 2026-09-23 统筹拍：按一手接上，具名开关可回退）----
             * 一手分支顺序 = 机体（`th08:0x44aa9e`）→ boss（`th08:0x44aaac`）→ 槽极妖（`th08:0x44aaba`）。*/
            CHECK(YK_GRAZE_GATE_OK == 0 && YK_GRAZE_GATE_SHOTTYPE == 1 && YK_GRAZE_GATE_BOSS == 2 &&
                  YK_GRAZE_GATE_GAUGE == 3 && YK_GRAZE_GATE_OFF == 4);
            /* 门「机体」（`th08:0x44aa94` / `:0x44aa9e`）：**人単独且不是妖夢（0xa）**。
             *   正（挡）：4 / 6 / 8；反（放）：3（组）、5（妖単独）、0xa（妖夢例外）。*/
            CHECK(yk_graze_field_gate(1, 1, 8000, 4) == YK_GRAZE_GATE_SHOTTYPE);
            CHECK(yk_graze_field_gate(1, 1, 8000, 8) == YK_GRAZE_GATE_SHOTTYPE);
            CHECK(yk_graze_field_gate(1, 1, 8000, 3) == YK_GRAZE_GATE_OK);
            CHECK(yk_graze_field_gate(1, 1, 8000, 5) == YK_GRAZE_GATE_OK);
            CHECK(yk_graze_field_gate(1, 1, 8000, 10) == YK_GRAZE_GATE_OK);   /* 妖夢例外 */
            /* 门「boss」（`th08:0x44aaac`）：正（放）= 场上有 boss；反（挡）= 没有。
             *   挡住时**先于**槽门被报（一手顺序：boss 在槽之前判）。*/
            CHECK(yk_graze_field_gate(1, 1, 8000, 0) == YK_GRAZE_GATE_OK);
            CHECK(yk_graze_field_gate(1, 0, 8000, 0) == YK_GRAZE_GATE_BOSS);
            CHECK(yk_graze_field_gate(1, 0, 0, 0) == YK_GRAZE_GATE_BOSS);     /* 两条都不过 ⇒ 报 boss */
            /* 门「槽极妖」（`th08:0x44aaba`，阈值 +8000 = 80%）：正（放）= 8000 及以上；反（挡）= 7999 及以下。*/
            CHECK(yk_graze_field_gate(1, 1, 8000, 0) == YK_GRAZE_GATE_OK);
            CHECK(yk_graze_field_gate(1, 1, 7999, 0) == YK_GRAZE_GATE_GAUGE);
            CHECK(yk_graze_field_gate(1, 1, 0, 0) == YK_GRAZE_GATE_GAUGE);
            CHECK(yk_graze_field_gate(1, 1, 10000, 0) == YK_GRAZE_GATE_OK);   /* 满槽 */
            CHECK(yk_graze_field_gate(1, 1, -10000, 0) == YK_GRAZE_GATE_GAUGE); /* 极人侧：也不算极妖 */
            /* ❓ 开关关掉（`YK_GRAZE_FIELD_GATES = 0`）：三条门整段不评 ⇒ 恒 `OFF`
             *   （与三个入参无关）⇒ 这就是「聚焦即撒」老行为。*/
            CHECK(yk_graze_field_gate(0, 0, 0, 4) == YK_GRAZE_GATE_OFF);
            CHECK(yk_graze_field_gate(0, 1, 8000, 0) == YK_GRAZE_GATE_OFF);
            /* ---- 颗数：门没过 ⇒ **0 颗**（一手三条 `je 0x44ab14` 都跳到函数尾）---- */
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_BOSS, 1, 1, 0) == 0);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_GAUGE, 1, 1, 0) == 0);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_SHOTTYPE, 1, 1, 4) == 0);
            /* 门 A = flag（`th08:0x44aace`）**且** 符卡进行中（`th08:0x44aad9` = `SPELLCARD.flags & 1`）：
             *   正：弹路径 + 符卡中 ⇒ 第 2/3 颗；反：符卡不在 ⇒ 停在 1 颗；激光路径（flag=1）⇒ 也停在 1 颗。*/
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 1, 0) == 3);   /* 门开 */
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 0, 0) == 1);   /* 门关（没在打符卡）*/
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 0, 1, 0) == 1);   /* 门关（激光擦弹，flag != 0）*/
            /* 门 B（`th08:0x44ab00`）：**妖単独**机体停在第 2 颗。
             *   正：`!solo_youkai`（组 0..3）⇒ 3 颗；反：`solo_youkai`（5 / 7 / 9 / 11）⇒ 2 颗。*/
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 1, 0) == 3);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 1, 5) == 2);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 1, 11) == 2);
            /* 门 B 只在前两颗都给出来的前提下才被求值（嵌套）：妖単独 + 没在打符卡 ⇒ 仍是 1 颗。*/
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 0, 5) == 1);
            /* 本卡的映射（TH18 机体号 0..3 = TH08 的「组」）下能取到的两档：平擦 1 / 符卡中 3。*/
            for (i = 0; i < 4u; ++i) {
                CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 0, (int)i) == 1);
                CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 1, (int)i) == 3);
            }
            /* ★ 开关关掉（`OFF`）⇒ **回到老行为**：连「人単独非妖夢」也不再拦（只会被门 A/B 影响）。*/
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OFF, 1, 1, 4) == 3);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OFF, 1, 0, 4) == 1);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OFF, 1, 1, 10) == 3);
            /* 野值（不该出现的判据码）⇒ 按「没过」处理（防御性；不崩、不给）。*/
            CHECK(yk_graze_orbs_per_bullet(7, 1, 1, 0) == 0);
            CHECK(yk_graze_orbs_per_bullet(-1, 1, 1, 0) == 0);
            }
            }
        }
    }

    /* ★★★ 2026-09-27（§BE #121）：夜点数字**补前导零到 6 位**（owner：按额外最大得点的画法，
     *   初始要看得见 `000000`）。`glyph 0` = '0'、dx = i×7（一手：每位 `pos.x += 7.0`）。*/
    {
        yk_gauge_glyph_t gg[YK_NIGHT_TEXT_MAX];
        int n = yk_night_text_glyphs(0, gg);
        CHECK(n == (int)YK_NIGHT_TEXT_DIGITS);
        CHECK(gg[0].glyph == 0 && gg[5].glyph == 0);
        CHECK(gg[5].dx > gg[0].dx);                       /* 低位在右 */
        n = yk_night_text_glyphs(1234, gg);
        CHECK(n == (int)YK_NIGHT_TEXT_DIGITS);
        CHECK(gg[0].glyph == 0 && gg[1].glyph == 0);       /* 前导零补满 */
        CHECK(gg[2].glyph == 1 && gg[3].glyph == 2 && gg[4].glyph == 3 && gg[5].glyph == 4);
        n = yk_night_text_glyphs(-5, gg);
        CHECK(n == (int)YK_NIGHT_TEXT_DIGITS && gg[5].glyph == 0);   /* 负数按 0 */
        n = yk_night_text_glyphs(999999, gg);
        CHECK(n == (int)YK_NIGHT_TEXT_DIGITS && gg[0].glyph == 9);
        n = yk_night_text_glyphs(1234567, gg);             /* 7 位（补零只在不足 6 位时）*/
        CHECK(n == 7 && gg[0].glyph == 1 && gg[6].glyph == 7);
    }

    /* ★★★ 2026-09-27（§BE #114）：击破批的**门**与**颗数**（一手 `Enemy::FUN_0042adb0`）。
     * 门的地址：`th08:0x42adbc call 0x41fd40`（数连锁）→ `0x42adc4 cmp [ebp-0x4],0 je 0x42b1e5`
     * （连锁为空 ⇒ **整段撒点跳过**）⇒ `n == 0` 必须给 **0 颗**（不是公式在 n=0 时的 10）。*/
    {
        unsigned k;
        yk_map_t t;
        memset(&t, 0, sizeof t);
        CHECK(yk_kill_burst_count(0) == 0);
        CHECK(yk_kill_ring_count(0) == 0);
        /* 双人组那一支（owner 2026-09-27 拍板）：`n >= 8 ? 26 : 2n+10`。
         * 一手边界两条：`0x42af29 cmp [ebp-0x4],0x8` / `0x42af2f mov [ebp-0x68],0x1a`。*/
        CHECK(yk_kill_burst_count(1) == 12);       /* ← 旧版写死的 12（这一档正好与一手同值）*/
        CHECK(yk_kill_burst_count(2) == 14);
        CHECK(yk_kill_burst_count(7) == 24);
        CHECK(yk_kill_burst_count(8) == 26);       /* 封顶那一档 */
        CHECK(yk_kill_burst_count(9) == 26);
        CHECK(yk_kill_burst_count(9999u) == 26);
        /* 被杀者自己那一环：`0x42b150 shl eax,1` ⇒ `2 · subEnemyCount`。*/
        CHECK(yk_kill_ring_count(1) == 2);
        CHECK(yk_kill_ring_count(4) == 8);
        CHECK(yk_kill_ring_count(9999u) == 19998u);
        /* 单调不减（防将来把常数写成 +6 / +16 那两支时手滑）。*/
        for (k = 1u; k < 40u; ++k) {
            CHECK(yk_kill_burst_count(k) <= yk_kill_burst_count(k + 1u));
            CHECK(yk_kill_burst_count(k) >= 12u);
        }
        /* ★ 半径上界可换（一手尾巴那一支把 `2N` 换成常量 128 的实参）：
         *   `yk_orb_burst_polar_range(range, …)` ≡ `yk_orb_burst_polar(range/2, …)`；
         *   `0x42b15d push 0x43000000` 是 `GetRandomF32InRange(128.0f)` 的**实参** ⇒ 圆盘上界。*/
        {
            float a1, r1, a2, r2;
            yk_orb_burst_polar(64, 0.25f, 0.5f, &a1, &r1);
            yk_orb_burst_polar_range(128.0f, 0.25f, 0.5f, &a2, &r2);
            CHECK(a1 == a2);
            CHECK(r1 == 64.0f);                    /* 0.5 × 128 */
            CHECK(r2 == 64.0f);
            yk_orb_burst_polar_range(128.0f, 0.0f, 1.0f, &a2, &r2);
            CHECK(r2 == 128.0f);                   /* 上界（真实 RNG 的 r2 < 1 ⇒ 取不到，这里喂 1.0）*/
            yk_orb_burst_polar_range(128.0f, 1.0f, 0.0f, &a2, &r2);
            CHECK(a2 > 3.14159f - 1e-4f && a2 < 3.14159f + 1e-4f);   /* 角上界 = π（≈ 满一圈）*/
            CHECK(r2 == 0.0f);
            yk_orb_burst_polar_range(-5.0f, 0.5f, 1.0f, &a2, &r2);    /* 负上界夹到 0 */
            CHECK(r2 == 0.0f);
            yk_orb_burst_polar_range(128.0f, 0.5f, 0.5f, 0, 0);       /* 双 NULL：不崩 */
        }
        (void)t;
    }

    printf("youkai_core: %d checks, %d failed\n", s_pass + s_fail, s_fail);
    return s_fail ? 1 : 0;
}
