/* 永夜抄（id 108）—— 妖机体 / 使魔（th08 系）。
 *
 * 设计唯一真源：`docs/superpowers/specs/2026-09-12-youkai-108-design.md`。
 * ★ TH08 的**社区反编译**（用户 2026-09-16 提供，MIT）：<https://github.com/Patchouli-CN/th08_dec>，
 *   本地克隆 `local/vendor/th08_dec`（`src/*.cpp` + `config/reccmp-functions.csv` 的 2221 条名字↔地址）。
 *   本作此前没有源码级来源 ⇒ **本卡进一步查证先翻它**；⚠ 它记的 sha256 与我们样本不同（另一个 1.00d 构建），
 *   本轮抽样核过布局一致（vtable 6/6、函数入口 400/400），但每条结论仍要落到 `th08:0x…` + 我们自己的样本上。
 *   出处与用法见 `engine/_shared/community-sources.md` 的「TH08 专用」一节。
 * 一手：`local/_scratch/research-108-shooter-tick.md`（止射闸）、
 *       `local/_scratch/research-108-damage-transfer.md`（伤害转交）、
 *       `local/_scratch/research-death-signal-survey.md`（敌人移除点全集）、
 *       `docs/superpowers/plans/2026-09-10-ufo-card-feasibility.md` §1.1（spawn 参数表）。
 * 共享契约：`EVENTS-CONTRACT.md`（止射闸 = OR；`on_enemy_death` = 广播只读；
 * 低速子机 = 选举覆盖；1/2 转嫁 = 单一落笔）。
 *
 * **一句话**：低速（聚焦）= 整个人换成妖侧机体（外观 + 子机 + 弹幕同时切、原生弹被压掉），
 * 高速 = 原机体；**低速但没有妖侧子机时例外** —— ❓**口径（用户 2026-09-12 定）**：
 * 那一刻**不压**原生弹（否则膛口没有子机、妖侧又发不出弹 ⇒ 一发都没有），见 `on_native_shot_suppress`；低速时身边的**使魔** = 敌人发射口实体化出来的**真敌人**（`NoEnemyDebug` 惰性空壳），
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
 *   （`YK_FAM_ANM_LAYER` 那一节，范式照 112 飞碟）画四色原生切图（`ability.anm` 206..209）。
 *
 * ★ 占位清单（每条都是「需要资产 / 需要用户决策」）见文件尾 —— 取值一律是**不做**这个语义。 */
#include <string.h>

#include "sdk.h"
#include "youkai_core.h"
#include "pl_anm_ids.h"   /* ★ 生成物：pl0X.anm 追加脚本号（妖侧本体 5 支 = CE_PL_ANM_SCRIPT_YKBODY_*）*/

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

/* ---- ★ 低速子机的贴图脚本（2026-09-12 **已换成原生切图**，不再借豌豆射手的 102）----
 * 四支机体**各一支**原生子机脚本（不是「一个共享图集里的一格」，也不是机体本体图）：
 *   TH08 妖侧：子机 = `zPlayerOption`（`player+0x40c`×4、stride `0x2f4`、每格头 0x2a4 就是 AnmVm），
 *     进低速那一帧按机体把脚本装进 `option+0x0` 那个 VM ——
 *     `th08:0x44e3d7 push 0x12`（pl00 八雲紫）/ `th08:0x44ea6f push 0x1d`（pl01 アリス）/
 *     `th08:0x44eba7 push 0x18`（pl02 レミリア），经 `AnmLoaded::load_script_into_vm` `th08:0x4069f0`
 *     （`ecx = [player+0xc]` = 本机体的 anm、`arg1 = option`、`arg2 = 脚本号`）。
 *   TH16：本体子机脚本号 = `th16:0x492c00[CHARACTER]` = {8,7,11,11}（**菜单序** ⇒ `CHARACTER`=2 = 文），
 *     `th16:0x4443ad` 取值 + `AnmLoaded::copy_vm th16:0x407b20` ⇒ **pl03 script 11**。
 * 资产：`assets/ability/scripts/224..227`（由 `assets/ability/make_youkai_option_art.py` 逐条照抄原生 +
 *   `assets/ability/youkai_option/` 下的切图；ORDER 片段 `entries/ORDER.youkai_option.txt` 待主 agent 合并）。
 * ⚠ 号是 **224..227**：`make anm` 之前 `native/anm_ids.h` 里还没有这四条宏 ⇒ 下面走**字面量**；
 *   `make anm` 之后宏就存在了，`#ifdef` 会自动切到宏（**这一步不用手改**）。*/
#ifdef CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_YUKARI
#define YK_OPT_SCRIPT_YUKARI   CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_YUKARI
#else
#define YK_OPT_SCRIPT_YUKARI   224      /* `make anm` 后换宏（本行会被上面的 #ifdef 取代）*/
#endif
#ifdef CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_ALICE
#define YK_OPT_SCRIPT_ALICE    CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_ALICE
#else
#define YK_OPT_SCRIPT_ALICE    225      /* `make anm` 后换宏 */
#endif
#ifdef CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_REMILIA
#define YK_OPT_SCRIPT_REMILIA  CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_REMILIA
#else
#define YK_OPT_SCRIPT_REMILIA  226      /* `make anm` 后换宏 */
#endif
#ifdef CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_AYA
#define YK_OPT_SCRIPT_AYA      CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_AYA
#else
#define YK_OPT_SCRIPT_AYA      227      /* `make anm` 后换宏 */
#endif

/* TH18 机体号（`CE_ADDR_CHARACTER`：0 霊夢 / 1 魔理沙 / 2 咲夜 / 3 早苗；与 `youkai_core.h` 的
 * `YK_OPTION_BY_CHAR` **同一顺序**）→ 该机体的低速子机脚本。
 * 四条逐一列（不假设号连续 —— 万一资产重排，这里会**编译期/边界**就对不上而不是静默画错）。*/
static int yk_option_script(int character)
{
    switch (character) {
    case 0:  return YK_OPT_SCRIPT_YUKARI;   /* 霊夢   → TH08 pl00 妖侧 = 八雲紫   （script18）*/
    case 1:  return YK_OPT_SCRIPT_ALICE;    /* 魔理沙 → TH08 pl01 妖侧 = アリス   （script29）*/
    case 2:  return YK_OPT_SCRIPT_REMILIA;  /* 咲夜   → TH08 pl02 妖侧 = レミリア （script24）*/
    default: return YK_OPT_SCRIPT_AYA;      /* 早苗   → TH16 pl03        = 射命丸文（script11）*/
    }
}
#define YK_OPTION_RETRY      60     /* 没拿到子机时的重试间隔（槽满 / 关卡中途买到），照 77 */
#define YK_OPTION_MAX        4      /* 一次最多管几个子机槽 = 数量表的最大值（咲夜 = 4；`YK_OPTION_CELLS`）。
                                     * 12 格公共池要和 71/77/116 分：本卡满配 4 ⇒ 留 8 格给那三张
                                     *（71 一颗 / 77 一颗 / 116 季节最多 6 —— 三张同时满配就正好 12，🟡 见报告）。*/
/* ★ 落笔（写 `+0x64/+0x68` 非聚焦、`+0x6c/+0x70` 聚焦）现在**只在** `on_option_pose_apply` 里做 ——
 * 偏移值来自声明（`yk_option_offsets`，逐格），单位 = 像素 ⇒ 落笔时 × `YK_SUBPIXEL` 变原生定点。
 * `ce_option_alloc` 的 off 参数只用来给**申请那一刻**的初始 y 一个值（引擎自己会算初始 pos，
 * `th18:0x40a7f6`–`0x40a84a`），随后每帧由引擎的跟随循环按我们写的四个字段重算 ⇒ 传 0 即可。*/
#define YK_OPTION_OFFSET_ALLOC 0
#define YK_SUBPIXEL          (1.0f / CE_SUBPIXEL_TO_PIXEL)   /* 128.0f：像素 → 原生定点 */
#define YK_FAM_ANM_INDEX  (-1)      /* ★ TODO(资产 §5)：使魔贴图。`NoEnemyDebug` 不跑 ECL ⇒ 不写这两格
                                     * 时会画「当前关卡敌人 anm 的 slot 0」（audit-open-items-106-109 §②）。
                                     * 占位 = 不写（< 0）；填真值时两格一起给。
                                     * ★★ 2026-09-12：**画面不再走这条路** —— 四色原生切图已在 `ability.anm`
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
    /* ★★ 2026-09-22（th08 #1）：机体 0（霊夢・紫）的**子机弹簧**状态（TH08 `th08:0x44e770`）。
     * 只有机体 0 需要它 —— 另两个机体的妖侧子机在原作里是**每帧直接吸附**（无速度），
     * 静态偏移就够（`yk_option_uses_spring` 的注）。单位 = **像素（屏幕空间、+y = 下）**。*/
    float         opt_pos[YK_OPTION_MAX][2];
    float         opt_vel[YK_OPTION_MAX][2];
    yk_transfer_t tr;                  /* 1/2 转嫁累加器 */
    int32_t       opt_script;          /* 手里这几个子机是用哪个 anm 脚本申请的（-1 = 没有）*/
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
    uint8_t       logged_pass0;        /* ★ 2026-09-12：「子机 0 ⇒ 放行原生弹（用户口径）」只记一次 */
    uint8_t       logged_hudgeo;       /* ★★ 2026-09-22（P2）：人妖槽 HUD 的**几何自述**只记一次
                                        *   （锚点 + 四个位置 + 倍数，实跑里用它与屏上所见对照，
                                        *   定「写进去的数生效的是 640 空间还是表面 px」这一争）*/
    uint8_t       logged_full;         /* 「使魔表满 / 堆拒绝」只记一次 */
    uint8_t       logged_gate;         /* 「发射口精确闸生效（使魔没了 ⇒ 该槽哑火）」只记一次 */
    uint8_t       opt_spring_live;     /* ★ th08 #1：弹簧状态是否已经按「自机位置」起过步。
                                        *   0 = 下一帧从自机位置出发（= TH08 state 1 的初值）。*/
    /* ★★ 2026-09-22（路 A）：本体脚本的跟踪状态（`yk_body_sync`）。
     * `body_focused` = 上一帧在不在低速（换形那一帧要**显式**设脚本，见 `yk_body_sync`）；
     * `body_state` = 引擎那条移动动画状态机的复刻结果（0..4）；
     * `body_script` = 我们**当前装着**的脚本号；`-1` = 「现在跑的不是我们装的」（高速 / 还没装过）
     *   —— 用它挡「状态没变还重装」（重装 = 动画从第 0 帧起，会看出来一顿一顿的）。
     * `body_prev_dx` = 上一次看到的 `player+0x47934`（引擎拿它当「上一帧输入速度」）。*/
    /* ★★ 2026-09-22（人妖槽 + 永点）：见文件里那一节的长注释。 */
    int32_t       piv;                 /* 永点：PIV 累加器（= 额外最大得点，点）—— 每帧声明给计分层 */
    int32_t       orbs;                /* 累计刻符数（一手公式里的 `total_time_orbs`，`th08:0x41825c`）*/
    int32_t       gauge;               /* 人妖槽（i16 语义；负 = 人侧、正 = 妖侧，量程 ±10000）*/
    uint16_t      gauge_focus_run;     /* 一手 `unkE2ad0`：聚焦状态**保持不变**的帧数（TH08 的 4 / 30 两档门）*/
    uint16_t      gauge_hold;          /* 一手 `unkE2ae8`：按住聚焦的帧数（爬升速率 = min(帧数/15, 21)）*/
    uint8_t       gauge_focus_prev;    /* 上一帧的聚焦位（漂移状态机的边沿检测）*/
    uint32_t      graze_seen;          /* 上一帧的累计擦弹（`CE_ADDR_GRAZE` 0x4ccd0c）快照 */
    uint32_t      orb_taken;           /* 本局收了几颗刻符 */
    uint32_t      orb_full;            /* 池满 / VM 建不出来而放弃的颗数（诊断）*/
    uint32_t      hud_bar, hud_human, hud_youkai, hud_cursor;  /* 人妖槽 HUD 的 4 支 VM */
    uint32_t      hud_digit[YK_GAUGE_TEXT_MAX];  /* ★ 百分比数字的**字形 VM**（每格一个；TH08 那边是
                                                  *   同一个 VM 反复换字，TH18 一个 VM 一帧只画一次
                                                  *   ⇒ 卡侧开 7 格，逐格换 sprite，见 `yk_gauge_text_sync`）*/
    ce_hud_slot_t hud;                 /* HUD 货架槽（`ce_hud_slot_claim_card`，左下角）*/
    uint8_t       hud_ready;           /* 货架认领成功（0 = 这一关没抢到块，HUD 不画）*/
    uint8_t       body_focused;        /* 0/1 */
    uint8_t       body_state;          /* 0..YK_BODY_STATES-1 */
    int32_t       body_script;         /* 追加脚本号 或 -1 */
    int32_t       body_prev_dx;        /* player+0x47934 的上一帧快照 */
    /* ★★ 2026-09-22 晚（owner 第 2 条：妖子机追敌 + 旋转）：**机体 0（霊夢・八雲藍）**那一支的状态。
     * 一手（子状态机 + 弹簧 + 旋转标签）逐条见 `youkai_core.h` 的「妖侧子机的追敌 + 旋转」一节。
     *   `opt_age`   = 手里这颗子机出场了多少帧（TH08 `Timer(&option+0x2e0)` 的对应物；
     *                 进场门 = 满 `YK_OPT_CHASE_MIN_AGE`(10) 帧）；
     *   `opt_chase` = 现在是不是「追敌」态（= 已发过旋转标签、家位置指向那只敌人）；
     *   `chase_*`   = 本帧的追踪目标（`on_tick_2` 每帧解析一次，帧界的 `on_option_pose_apply` 读它）。
     * ★ 目标从哪来 = ❓ **自定**（TH08 那一格 `player+0xe2abc` 在我们的样本里够不到，见 core 的长注释）：
     *   先 boss（`mgr+0x48` 可锁定那只）、没有就取**最近的敌人**（排除我们自己的使魔）。*/
    uint16_t      opt_age;             /* 手里那颗子机出场帧数（≥ 10 才准进追踪）*/
    uint8_t       opt_chase;           /* 1 = 追敌态（已发旋转标签）*/
    uint8_t       chase_have;          /* 本帧有没有目标 */
    uint32_t      chase_id;            /* 目标 enemy_id（0 = 没有；诊断日志用）*/
    float         chase_x, chase_y;    /* 目标坐标（640 空间 = `final_pos`）*/
    uint32_t      chase_enters;        /* 本局进过几次追踪态（诊断）*/
    uint8_t       logged_chase;        /* 「追踪生效」只记一次 */
    uint8_t       logged_natopt;       /* 「隐藏原生子机」只记一次 */
    uint8_t       _pad[2];             /* 对齐（保持结构布局稳定；新增位一律从 `_pad` 里拿）*/
    uint32_t      heals_seen;          /* ★ 2026-09-22（#7）：上一帧读到的闸自愈计数（`yk_map_heals`）
                                        *   的快照。差值 > 0 ⇒ 打一行日志 —— 实跑里「boss 不发弹」
                                        *   到底是闸夹死（这一行在涨）还是别的原因，一眼可判。*/
    uint32_t      rearms_seen;         /* ★★ 2026-09-22（工单 E / route A）：上一帧读到的「下一波复位」
                                        *   计数（`yk_map_rearms`）的快照。它在涨 = 波次级复位生效
                                        *   （被夹住的槽在下一次 `etOn` 就被放开、使魔同帧重建）。*/

    int32_t       claimed_set;        /* ★★★ 2026-09-22（P7）：最近一次认领/打日志的主炮 set（-1 = 还没认领过）*/
    /* ★★★ 2026-09-22（#25 击破符卡的螺旋，一手参数见 `youkai_core.h` §3.11b）：
     * `spiral_quota` = 还剩几颗没吐（TH08 的 `spellcard[+0x10]`）；`spiral_t` = 计时器帧数（从 10 起算）；
     * `spell_was` = 上一帧在不在符卡里（**符卡结束的下降沿**是触发器）；`spiral_logged` = 只记一次日志。*/
    uint8_t       flash_pending;      /* ★★★ 2026-09-22（#22）：本帧是「人→妖」那一帧 ⇒ 第一个建出来的
                                       *   使魔身上补一支爆闪（一手：爆闪起在使魔位置上）*/
    uint32_t      spiral_quota;
    uint16_t      spiral_t;
    uint8_t       spell_was;
    uint8_t       spiral_logged;
    uint32_t      releases_seen;       /* ★★★ 2026-09-22（P8-3 / route A′）：上一帧读到的
                                        *   `yk_map_releases`（第二问主动放行的次数，只增不清）*/
    uint32_t      silent;              /* ★★★ 2026-09-22（P8-6）：使魔**查不到**（过场 destroy_all /
                                        *   静默移除）而被松开的次数 —— 原来账上无痕（只有 VM 侧自愈）*/
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

/* ★★★ 2026-09-22（P3 第二批）：「射击中」的判据（TH18 侧）。
 * TH08 是 `Player::shotActive`（一个 999 帧的射击周期标志，`th08:0x17d6ed4`）；TH18 没有同一个
 * 状态量（每帧按 rate 发弹，没有周期）⇒ 本卡用**射击键按住**（`CE_INPUT_HELD` 的 bit0）。
 * ❓ 口径，登记 `TUNING.md` §2.0y。*/
static int yk_shooting(void)
{
    return (CE_INPUT_HELD() & 0x1u) != 0;
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
 * 而且 `etSprite` 只在 ECL 里跑）。四色原生切图已经进了 `ability.anm`
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
 *   ⚠ 使魔的脚本**没有** `layer()`（原生 enemy.anm script53..56 都没写，那几份 anm.txt 注明）
 *     ⇒ 这个参数是**唯一**的层来源（飞碟本体能说「脚本自己写 layer(7)」，这里不能照抄那句话）。
 *   ❌ 不用 HUD 的 24/25：那是**相机 2 的外围 UI 列表**（§1 的 `0x3c` / `0x39` 两行专门提醒
 *     「世界列表 24」与「UI 列表 24」是两码事），与场上实体的语义无关。
 *   🟡 层号本身未实跑核（ufo.c 的 7 也标着未验）。实跑若整只看不见，第一个该动的就是这一个数。*/
#define YK_FAM_ANM_LAYER 7

/* 弹壳里的 anm VM（颜色在它 `+CE_ANM_VM_COLOR1`）。
 * 一手/先例：`lolk.c:73` 的 `CE_BULLET_VM 0x28`（115 拿它逐颗染擦弹圈里的弹）、
 * `signal.c:1795` 的 `SIGNAL_BULLET_VM_OFF 0x28`（`zBullet.vm`）⇒ 两个卡各自本地定义同一格，
 * `engine.h` 没有这一条（它只钉了 `CE_BULLET_FLAGS/POS/STATE`）⇒ 本卡照同一写法**本地**定义。*/
#define YK_BULLET_VM 0x28

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
    /* ★★★ 2026-09-22（P4）：**魔法阵环**（TH08 `etama.anm` script48 那条链）的句柄与状态。
     * 环与本体**同生同死**（spawn/follow/kill/prune 四条路一起走），所以放在同一条记录里。
     * `ring_state` = 上一次发过的 interrupt（0 = 还没发过 ⇒ 出生时补发 2 = 妖侧蓝环）。*/
    uint32_t ring_id;  /* 0 = 没有环 VM */
    int32_t  ring_state;/* 最近一次发的环 interrupt（1 = 人侧紫 / 2 = 妖侧蓝）*/
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
    if (v->ring_id) { ce_anm_delete(v->ring_id); v->ring_id = 0; }   /* ★ P4：环与本体同生同死 */
    v->fam_id = 0;
    v->variant = 0;
    v->ring_state = 0;
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

/* ★★★ 2026-09-22（P1，实跑日志直证）：跟随 VM 表的**泄漏回收**（自愈）。
 *
 * **不变式**：`s_fam_vm` 里每条 `fam_id != 0` 的记录 ⇔ `s_map` 里那个 `fam_id` 还活着（活使魔）。
 * 违反不变式的记录 = **泄漏**：格子被一条没人认领的记录占着，而它对应的那只使魔实体也没人管
 * （不再钉位置、松 Shift 也不撤 = owner 报的「敌方使魔滞留」）。
 *
 * **实跑证据**（`game/th18_card_expand.log`，2026-09-22 那次）：
 *   12 行 `youkai: familiar follow-VM table full (32) - familiar 193/194/… stays invisible`
 *   —— 32 格全被泄漏记录占满，之后造的使魔**根本没有 VM ⇒ 画不出来**（`NoEnemyDebug` 空壳不跑 ECL，
 *   没有任何别人会画它）。根因在 `yk_chase_target_resolve`（见那里的注）：判据误用了有副作用的
 *   `yk_fam_broken` ⇒ 地图侧记录被清、VM 侧记录没人收。**根因已修**（判据改 `yk_map_is_fam`）；
 *   本函数是**第二道保险**：即使以后又出现一条新路径漏收（或旧泄漏已经在场），表也不该满。
 *
 * 做两件事（幂等、可在 tick 里安全调用 —— 只标记删除，不析构）：
 *   ① `ce_anm_delete` 收掉那条孤儿 VM；
 *   ② 那只**孤儿使魔实体**也撤掉（`yk_kill_familiar` 只置 `flags_lo` 位 25 ⇒ 同帧 EnemyManager
 *      sweep 静默析构它，不跑死亡处理 = 「撤」的语义）；实体早没了就跳过。
 * 返回回收了几条（卡侧据此打一行日志 —— 实跑里这一行**应当不再出现**；出现了就说明还有漏收的路径）。*/
static unsigned yk_fam_vm_prune(void)
{
    unsigned i, n = 0;
    for (i = 0; i < YK_FAM_MAX; ++i) {
        yk_fam_vm_t *v = &s_fam_vm[i];
        uint8_t *f;
        if (!v->fam_id) continue;
        if (yk_map_is_fam(&s_map, v->fam_id)) continue;     /* 活使魔：不变式成立 */
        f = yk_familiar_by_id(v->fam_id);                   /* 孤儿：先拿下实体（可能是 0） */
        yk_fam_vm_kill(v);                                  /* ① 收 VM（幂等）*/
        if (f) yk_kill_familiar(f);                         /* ② 撤那只没人管的真敌人 */
        ++n;
    }
    return n;
}

/* 每帧的不变式检查 + 记账（`on_tick_2` 调；`noinline` 的理由见那里的注 —— 别并进 `on_tick_2`）。*/
static void __attribute__((noinline)) yk_fam_vm_check(void)
{
    unsigned pruned = yk_fam_vm_prune();
    if (pruned)
        ce_log("youkai: familiar follow-VM table self-healed - reclaimed %u leaked record(s) "
               "this frame (those familiars were orphans: the slot map no longer knew their "
               "`fam_id`; entity + VM both withdrawn)", pruned);
}

/* 起一个跟随 VM 并**立刻**摆到实体坐标（世界层 VM 的实体坐标 (0,0,0) = 弹幕区上边框中点
 * ⇒ 不先摆会闪一帧）。失败只报日志（画不出来 ≠ 卡坏，`ufo_body_vm_spawn` 同款）。*/static void yk_fam_vm_spawn(uint32_t fam_id, int variant, float x, float y)
{
    yk_fam_vm_t *v = yk_fam_vm_find(fam_id);
    uint32_t id;
    int script;
    unsigned i;

    if (!fam_id) return;
    if (!v) {
        for (i = 0; i < YK_FAM_MAX; ++i) if (!s_fam_vm[i].fam_id) { v = &s_fam_vm[i]; break; }
    }
    if (!v) {
        /* ★ 2026-09-22（P1）：表满 ⇒ **先自愈回收**（把已经不在 `s_map` 里的泄漏记录收掉）再重试一次。
         * 回收后仍然没有空格 = 32 条记录逐条都对应一只活使魔 —— 那与 `yk_map_t` 那 32 格一一对应，
         * 到不了（除非 `yk_map_alloc` 侧先满）。真到不了这一行，日志就是唯一线索。*/
        unsigned n = yk_fam_vm_prune();
        if (n) {
            ce_log("youkai: familiar follow-VM table self-healed - reclaimed %u leaked record(s) "
                   "before spawning familiar %u (P1: records outlive their `fam_id` in the slot map)",
                   n, fam_id);
            for (i = 0; i < YK_FAM_MAX; ++i) if (!s_fam_vm[i].fam_id) { v = &s_fam_vm[i]; break; }
        }
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
    /* ★★★ 2026-09-22（P4）：**每一个使魔都带一个魔法阵环**（TH08 的行为：环在使魔的一生里
     * 一直存在，Shift 只换它的 interrupt）。环脚本出生是 `scale(0,0); alpha(0)`（不可见）
     * ⇒ 必须**立刻发一次 interrupt**，否则什么都看不到：TH08 那边就是
     * `AllocEffectSlot(0x20,…)` 紧接着 `SetInterrupt(环, IsYoukai() ? 2 : 1)`（`th08:0x41b003`）。
     * 我们的使魔只在低速（= 妖侧）存在 ⇒ 出生那一刻恒发 **2 = 蓝 (96,96,255) α200 λ0.8**；
     * 人侧那支（1 = 紫 α255 λ0.5）留给「使魔在人侧也活着」的情形 —— `yk_fam_vm_follow` 每帧
     * 按聚焦位比对，变了才补发（见那里的注）。*/
    /* ⚠ 自愈重起时可能还攥着一支旧的环句柄（本体 VM 死了但环还活着）⇒ 先收掉，否则那条环永久留在场上。*/
    if (v->ring_id) { ce_anm_delete(v->ring_id); v->ring_id = 0; }
    v->ring_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING,
                              YK_FAM_ANM_LAYER);
    v->ring_state = 0;
    if (v->ring_id) {
        ce_anm_set_pos(v->ring_id, x, y, 0.0f);
        ce_anm_interrupt(v->ring_id, 2);
        v->ring_state = 2;
    } else {
        /* 环起不出来只会「少一圈魔法阵」，不影响本体与机制 ⇒ 记一行就够（实跑判据）。*/
        ce_log("youkai: familiar ring VM spawn FAILED (fam %u, script %d) - the familiar body is "
               "there but its TH08 magic circle (etama.anm script48) is missing",
               fam_id, (int)CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING);
    }
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
    /* ★★★ 2026-09-22（P4）：环 —— ① 同帧钉到同一坐标；② **聚焦位一变就换 interrupt**
     * （TH08：`SetInterrupt(环, IsYoukai() ? 2 : 1)`，一手见 `youkai.c` 的环那一节与生成器文件头）。
     * 环句柄没了就清 0（`ring_state` 也归 0）⇒ 下一帧按同一个 `fam_id` 走 spawn 的环分支重建。*/
    if (v->ring_id && !ce_anm_get_vm(v->ring_id)) { v->ring_id = 0; v->ring_state = 0; }
    if (v->ring_id) {
        int want = CE_PLAYER_FOCUSED() ? 2 : 1;
        if (v->ring_state != want) {
            ce_anm_interrupt(v->ring_id, want);
            v->ring_state = want;
        }
        if (!ce_anm_set_pos(v->ring_id, *(const float *)(f + CE_ENEMY_POS_X),
                                        *(const float *)(f + CE_ENEMY_POS_Y), 0.0f)) {
            v->ring_id = 0;
            v->ring_state = 0;
        }
    }
}

/* ============ ★★★ #22 的另一半：Shift 那一下的**爆闪**（一手 `th08:0x42c420` 两支）=================
 * 一手：`FUN_0042c420` 里两支各起一支特效（都在**使魔身上** `&enemy+0x2d88`）：
 *   · **人→妖**：`0x42c44e SpawnEffect(0x1f, …, 颜色 0x80303080)` ⇒ 模板 `0x4c6d30[0x1f]` = `etama.anm`
 *     **script59**（`alpha(96)→0`、`scale(0.5→5.0)` over 10 帧）—— 我们的脚本 **310**；
 *   · **妖→人**：`0x42c4ef SpawnEffect(0x1e, …, 0x80803030)` ⇒ **script60**（`alpha(0)→96`、
 *     `scale(5.0→0.4)`）—— 我们的脚本 **311**。
 *   两支共用 sprite193（`etama2.png (65,209,30,30)`）⇒ 我们切了一张 `YOUKAI_FAM_FLASH.png`（32×32）。
 * ⚠ 两支脚本**自己 `delete()`**（`+10:` 那一步）⇒ **卡侧不跟踪句柄**，起完就不管（10 帧后自灭）；
 *   也因此**不需要**在 `on_delete` 里收尾（最坏情况是删卡后还飘 ≤10 帧的一闪）。
 * ⚠ 位置 = 使魔的位置（一手就是 `enemy+0x2d88`）；我们只在**那一帧真的有使魔**时起
 *   （人→妖 = 重建出来的那些使魔；妖→人 = 撤走前最后记下的坐标），免得在没使魔时凭空闪一下。*/
static void yk_flash_spawn(int script, float x, float y)
{
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), script, YK_FAM_ANM_LAYER);
    if (id) ce_anm_set_pos(id, x, y, 0.0f);
    /* 起不出来只影响观感（10 帧的一闪）⇒ 不记日志（每关都有若干次，会刷屏）。*/
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
        /* ★ #22：人→妖那一帧的爆闪（310）—— 起在**第一只**建出来的使魔身上（一手 script59）。*/
        if (s && s->flash_pending) {
            s->flash_pending = 0;
            yk_flash_spawn(CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_FLASH59, act->x, act->y);
        }
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

/* ============================ §3.7 伤害转交 1/2 ============================
 * ⚠ **`noinline` 是必需的，别删**（2026-09-22 实测）：本函数是 `on_tick_2` 里**唯一**内联着
 *   `ce_damage_rect`（`sdk.h:172` 那段内联汇编 —— 4 个栈参 + XMM2/XMM3）的路径。i686 只有 8 个
 *   通用寄存器，而 `on_tick_2` 身上挂的东西已经很多（子机声明 / 人妖槽 / 刻符 / 擦弹 / 使魔表 /
 *   PIV 上报…）⇒ 它一旦被内联进 `on_tick_2`，编译器就报
 *   `inline assembly requires more registers than available`（整个 TU 编不过，不是警告）。
 *   代价：每帧多一次 call（可忽略）；收益：加任何一行业务代码都不会再把整棵树编红。*/
static void __attribute__((noinline)) yk_transfer_tick(yk_card_t *s)
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
             * 并且**同时收掉跟随 VM** —— 实体没了还留 VM = 场上一个打不到的幽灵使魔。
             * ★★★ 2026-09-22（P8-6）：**这一步也进账 + 打一行**（原来账上无痕 —— 实跑里
             *   「self-healed 前 120 帧找不到对应死亡行」就是因为静默消失没有记录）。
             *   `silent` 与 `killed`（死亡事件）/`lost`（我们的状态差分撤）三笔分开记，
             *   三者相加才是「这一局使魔消失的完整账」。*/
            ce_log("youkai: familiar %u vanished silently (no death event) - released its emitter "
                   "slot %d of enemy_id %u (silent total %u); VM withdrawn too",
                   sl->fam_id, (int)sl->slot, sl->enemy_id, s->silent + 1u);
            yk_fam_vm_kill_id(sl->fam_id);
            yk_fam_gone(&s_map, sl->fam_id);
            ++s->silent;
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
 * 走 **SDK 的低速子机声明层**（spec §3.3.1）：本卡只**声明**（每帧、低速才算），
 * **只有被选举者落笔**（id 最小者胜，108 < 111）—— 落笔 = 申请子机 + **逐格**写原生双偏移
 * （`+0x64/+0x68` 非聚焦、`+0x6c/+0x70` 聚焦，单位 = 原生 1/128 定点）。
 * ★ 2026-09-12：落笔**只**发生在 `on_option_pose_apply` 里（SDK 每帧调一次被选举的那张卡；
 *   被顶掉时再补叫一次「退位通知」，那时 `ce_option_pose_winner() != 我们` ⇒ 只收子机、不写字段）。
 * ★ 逐格偏移：`count >= 2` 时每格必须摆在不同位置（否则会叠在同一点）——
 *   表与一手依据在 `youkai_core.h` 的 §3.3（位置 = 本作原生 `pl0X.sht` 同档阵型）。*/

/* 本帧读引擎的「机体号 + 火力档」：档位算式与 `th18:0x45d604`/`0x45d60a` 同款
 * （`CURRENT_POWER / POWER_PER_LEVEL`；`yk_shot_tier()` 负责钳到 0..4）。*/
static int yk_current_character(void)
{
    return *(const int32_t *)CE_ADDR_CHARACTER;
}

static int yk_current_power_level(void)
{
    return yk_shot_tier((int)CE_CURRENT_POWER());
}

/* ---- ★★ 2026-09-22 晚（owner 第 2 条）：本帧的**追踪目标**（`on_tick_2` 每帧解析一次）---------
 * 一手：TH08 的目标是 `player+0xe2abc` 里的**一只敌人的指针**（`th08:0x44e8f1` 读它、取
 * `final_pos` = `zEnemy+0x2d88`）。那个指针在本 build 里没有任何非 0 写点（见 `youkai_core.h` 的长注释）
 * ⇒ 目标选择这一格 ❓ **自定**，取「先 boss、没有就最近的敌人」：
 *   ① boss = `mgr+0x48` 的 4 个 id 里**可锁定**的那只（与伤害转嫁用同一个 `yk_boss()`，零新增读法）；
 *   ② 没有 boss ⇒ 遍历敌人链表取**离自机最近**的一只（`ce_enemy_lockable` 过滤），
 *      并**排除我们自己的使魔**（它们是真敌人，会把目标吸到自己身上）。
 * 目标只在本帧有效（`on_tick_2` 写进卡状态、帧界的 `on_option_pose_apply` 读）——**绝不缓存指针**。*/
static void yk_chase_target_resolve(yk_card_t *s)
{
    uint8_t *e;
    float px = 0.0f, py = 0.0f;
    float best = 0.0f;
    uint32_t best_id = 0;
    const float *bp;

    if (!s) return;
    s->chase_have = 0;
    s->chase_id = 0;
    e = yk_boss();
    if (e && (bp = ce_enemy_pos(e)) != 0) {
        uint32_t id = *(const uint32_t *)(e + CE_ENEMY_ID);
        if (id) {
            s->chase_have = 1;
            s->chase_id = id;
            s->chase_x = bp[0];
            s->chase_y = bp[1];
            return;
        }
    }
    /* 没有 boss：最近的敌人。`best == 0` 表示还没选到（距离 0 也可能命中 ⇒ 用 `best_id` 判）。*/
    if (CE_PLAYER()) {
        const float *pp = ce_player_pos();
        if (pp) { px = pp[0]; py = pp[1]; }
    }
    CE_ENEMY_FOREACH(CE_ENEMY_MGR(), e) {
        const float *ep;
        float dx, dy, d;
        uint32_t id;
        if (!ce_enemy_lockable(e)) continue;
        id = *(const uint32_t *)(e + CE_ENEMY_ID);
        if (!id) continue;
        /* ★★★ 2026-09-22（P1 / 实跑日志直证）：这里原来调的是 `yk_fam_broken` —— 那是**事件**
         * （清 `fam_id` + 置 `broken`），**不是判据** ⇒ 每帧对每一只活使魔都走了一次「被打死」：
         *   ① 表条目被清 ⇒ 那只真敌人变**孤儿**（不再钉位置、松 Shift 也不撤 = owner「敌方使魔滞留」）；
         *   ② 跟随 VM 记录再没人收 ⇒ 32 格 `s_fam_vm` 泄漏 ⇒ 之后 `ce_anm_spawn` 拿不到格子
         *      （日志 `familiar follow-VM table full (32) - familiar 193/… stays invisible`）；
         *   ③ 槽被标 `broken` ⇒ 发射口闸夹上 ⇒ route A 重建一只**新**使魔（新 `fam_id`）⇒ ①逐波累积。
         * 修法 = 判据换成**只读**的 `yk_map_is_fam`；事件那两条路（`on_enemy_death` / 静默消失）
         * 仍走 `yk_fam_broken` / `yk_fam_gone`。全文见 `youkai_core.h` 的 `yk_map_is_fam`。*/
        if (yk_map_is_fam(&s_map, id)) continue;          /* 我们自己的使魔不当目标 */
        ep = ce_enemy_pos(e);
        if (!ep) continue;
        dx = ep[0] - px;
        dy = ep[1] - py;
        d = dx * dx + dy * dy;
        if (!best_id || d < best) { best = d; best_id = id; s->chase_x = ep[0]; s->chase_y = ep[1]; }
    }
    if (best_id) { s->chase_have = 1; s->chase_id = best_id; }
}

static void yk_option_declare(ce_card_t *c, yk_card_t *s, int focused)
{
    ce_option_pose_t pose;
    yk_opt_off_t off;
    const int character = yk_current_character();
    const int level = yk_current_power_level();
    int count = yk_option_count(focused, character, level);
    int i;

    if (count <= 0) {
        /* 「退出声明」= 不进选举（也**不挡住 111** —— 否则「赢了选举却不落笔」会把 111 的子机一起闷掉）。
         * 来路：高速（用户定案 5）、早苗的低火力档（TH16 档 0 = 0 个）、池被重建的那一帧。
         * ★ 光退出还不够：手里若还攥着子机，被顶掉那一刻的「退位通知」会再叫我们一次
         *   `on_option_pose_apply`，由那里收干净。*/
        ce_option_pose_declare(c, NULL);
        return;
    }
    if (count > YK_OPTION_CELLS) count = YK_OPTION_CELLS;   /* 表最大 4；层自己也会再钳一次 */
    __builtin_memset(&pose, 0, sizeof pose);
    pose.count = count;
    /* ★ 子机贴图脚本 = **按机体**选（2026-09-12 换掉占位的 102）：四支机体各一支原生子机脚本，
     * 号在 `YK_OPT_SCRIPT_*` 那一节（224..227，出处见那里的注释）。脚本是**申请时定死的**
     * （`Player__allocate_option` `0x40a790` 第 5 个栈参）⇒ `on_option_pose_apply` 里换了号会先收再要。*/
    pose.anm_script = yk_option_script(character);
    for (i = 0; i < count; ++i) {
        if (!yk_option_offsets(character, count, i, &off)) continue;
        pose.cell[i].fx = off.fx;
        pose.cell[i].fy = off.fy;
        pose.cell[i].ux = off.nfx;
        pose.cell[i].uy = off.nfy;
    }
    if (ce_option_pose_declare(c, &pose) < count && s && !s->logged_opt) {
        s->logged_opt = 1;                                  /* 不该发生（上面按同一个上限钳过）——留一行日志 */
        ce_log("youkai: option pose declare clamped below %d cells (spec §3.3.1 layer cap)", count);
    }
}

/* 每帧刷新：**每帧都声明**（层在 `BP_ce_item_frame` 每帧清集合 ⇒ 不声明 = 本帧退出）。
 * 落笔不在这里 —— SDK 在帧界调被选举者的 `on_option_pose_apply`（见下）。*/
static void yk_option_refresh(ce_card_t *c, yk_card_t *s, int focused)
{
    yk_option_declare(c, s, focused);
}

/* 收掉本卡手里所有子机。★ `ce_option_drop` 自己会**把池槽松开**（`option+0x00 = 0`，一手：
 * sdk.c 的注释与 `0x40a790` 的空位判据）⇒ 不需要再调 `ce_repopulate_options` 去回收。*/
static void yk_option_release(ce_card_t *c, yk_card_t *s)
{
    int i;
    for (i = 0; i < YK_OPTION_MAX; ++i) {
        /* ★ 删卡路径的漏洞补在这里（2026-09-12 审计）：`ce_option_drop(c, o)` 在 `o->ptr == 0`
         * 时走的是**原生单子机**那一支（`sdk.c:426`）—— 它无条件删 `card+0x1c` 那个 VM。而多子机卡里
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
     * ⇒ 补一次原生的单子机收尾；上面已经清过那一格时它是 no-op。*/
    ce_option_drop(c, 0);
    s->opt_script = -1;
    s->opt_spring_live = 0;                /* ★ th08 #1：松手 ⇒ 下次再要子机时重新从自机位置起 */
    s->opt_age = 0;                        /* ★ 2026-09-22 晚：出场计时从 0 起（TH08 的 `Timer(&option+0x2e0)`）*/
    s->opt_chase = 0;                      /* 手里没子机 ⇒ 追踪态自然结束（旋转标签由新 VM 自己的脚本起手）*/
}

/* ★ 被选举者落笔（SDK 每帧调**恰好一次**；spec §3.3.1 的 `on_option_pose_apply`）。
 * 两种来路都在这里处理：
 *   ① `ce_option_pose_winner() == 我们` ⇒ 按本帧声明申请 / 逐格摆位；
 *   ② **退位通知**（上一帧我们是落笔者、这一帧不是：松了 Shift / 被别人盖过 / 池被重建）
 *      ⇒ 此时 winner 不是我们 ⇒ 把手里所有子机还回去（否则高速还看得见妖侧子机 = 用户定案 5 的反面）。*/
static int on_option_pose_apply(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    const ce_option_pose_t *pose = ce_option_pose_pose();
    const float *pp = ce_player_pos();                    /* 弹簧的目标基准（th08 #1）*/
    const int character = yk_current_character();
    int want, i;

    if (!s) return 0;
    if (ce_option_pose_winner() != (int)ce_card_id(c) || !pose || pose->count <= 0) {
        if (s->opt_script >= 0 || s->opt[0].ptr) yk_option_release(c, s);   /* 退位：收干净 */
        return 0;
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
        uint8_t *o = ce_option_get(c, &s->opt[i], YK_OPTION_OFFSET_ALLOC, pose->anm_script,
                                   YK_OPTION_RETRY, "youkai");
        float fx = pose->cell[i].fx;
        float fy = pose->cell[i].fy;
        if (!o) continue;
        s->opt_script = pose->anm_script;
        /* ★★ 2026-09-22（th08 #1）：机体 0（霊夢・紫）的妖侧子机在**原作里是弹簧跟随**
         * （`th08:0x44e3a0` → `th08:0x44e770`，逐步式与常量见 `youkai_core.h`）。
         * 引擎只认「自机位 + 偏移」这一个形状（`th18:0x45bce8`–`0x45bd08`）⇒ 我们**自己积分**
         * 那颗子机的位置（`s->opt_pos`）、把「位置 − 自机坐标」当偏移写下去 ⇒ 观感逐帧等于原作的滞后跟随。
         * ⚠ 引擎每帧都会用我们写的这四个字段重算子机位置 ⇒ **必须每帧重写**（不能只在申请那一帧写）。*/
        if (yk_option_uses_spring(character) && pp) {
            float goal[2];
            int chasing = 0;
            /* ★★ 2026-09-22 晚（owner 第 2 条）：**追敌 + 旋转**（只有机体 0 = 霊夢・八雲藍那一支，
             * 一手 `th08:0x44e3a0` 的子状态 3 → `th08:0x44e8d0`；见 `youkai_core.h` 的长注释）。
             * 判据 = `yk_option_chase_want(有目标, 出场帧数)`（TH08：目标存在 + `Timer(&option+0x2e0)` 满 10 帧）。
             * 进/出的**边沿**各发一次 `AnmVm::SetInterrupt`（TH08 原版：进 = 标签 3 旋转、出 = 标签 1 待机循环；
             * 我们的 TH18 号 = `YK_OPT_SPIN_LABEL`(5) / `YK_OPT_IDLE_LABEL`(2)，映射见生成器文件头）。
             * ⚠ 只对 `i == 0` 做（TH08 只有 1 颗子机；机体 1/2 根本没有这一段）。*/
            if (character == 0 && i == 0) {
                uint32_t vm = *(const uint32_t *)(o + CE_OPT_ANM_ID);
                int want_chase = yk_option_chase_want((int)s->chase_have, (int)s->opt_age);
                chasing = want_chase;
                if (want_chase && !s->opt_chase) {
                    if (vm) ce_anm_interrupt(vm, YK_OPT_SPIN_LABEL);      /* TH08 `push 3`（`0x44e8a6`）*/
                    s->opt_chase = 1;
                    ++s->chase_enters;
                    if (!s->logged_chase) {
                        s->logged_chase = 1;
                        ce_log("youkai: youkai option CHASING enemy_id %u (spin label %d, goal = target "
                               "+ %.0f y) - TH08 shape: `th08:0x44e8d0` spring toward the target's "
                               "`final_pos`, spin = script label from `th08:0x44e8a6 push 3`",
                               s->chase_id, (int)YK_OPT_SPIN_LABEL, (double)YK_OPT_CHASE_GOAL_DY);
                    }
                } else if (!want_chase && s->opt_chase) {
                    if (vm) ce_anm_interrupt(vm, YK_OPT_IDLE_LABEL);      /* TH08 `push 1`（`0x44e6dd`）*/
                    s->opt_chase = 0;
                }
            }
            if (chasing && s->chase_have) {
                const float tgt[2] = { s->chase_x, s->chase_y };
                /* 追敌：家位置 = 目标 `final_pos` + (0, +32)，弹簧与跟随自机**逐条同式**。*/
                if (!yk_option_chase_goal(tgt, goal)) { goal[0] = pp[0]; goal[1] = pp[1]; }
            } else if (!yk_option_home_from_player(character, i, pp, goal)) {
                goal[0] = pp[0]; goal[1] = pp[1];   /* 跟随自机（一手 `th08:0x44e770`）*/
            }
            if (!s->opt_spring_live) {            /* TH08 state 1：从**自机位置**出发 */
                s->opt_pos[i][0] = pp[0];
                s->opt_pos[i][1] = pp[1];
                s->opt_vel[i][0] = 0.0f;
                s->opt_vel[i][1] = 0.0f;
            }
            yk_opt_spring_step(s->opt_pos[i], s->opt_vel[i], goal);
            fx = s->opt_pos[i][0] - pp[0];
            fy = s->opt_pos[i][1] - pp[1];
        }
        /* 写**这一格**的阵型（原生 `0x40b8c1` 也这么自己摆；单位 = 原生 1/128 定点）。
         * 引擎每帧自己按聚焦位取用：`pos = 自机位 + (聚焦 ? [+0x6c/+0x70] : [+0x64/+0x68])`
         * （一手 `th18:0x45bce8`–`0x45bd08`，且**卡池 12 格与原生 4 格走同一个循环**
         * —— `th18:0x45b8ea`/`0x45b8fa` 两处调用）⇒ 卡侧只写这四个字段就够。*/
        *(int32_t *)(o + CE_OPT_OFF_NF_X) = (int32_t)(pose->cell[i].ux * YK_SUBPIXEL);
        *(int32_t *)(o + CE_OPT_OFF_NF_Y) = (int32_t)(pose->cell[i].uy * YK_SUBPIXEL);
        *(int32_t *)(o + CE_OPT_OFF_F_X)  = (int32_t)(fx * YK_SUBPIXEL);
        *(int32_t *)(o + CE_OPT_OFF_F_Y)  = (int32_t)(fy * YK_SUBPIXEL);
    }
    s->opt_spring_live = 1;                /* 本帧起过步 ⇒ 下一帧不再回到自机位置 */
    return 0;
}

/* ================= ★★ 2026-09-22（th08 #5）妖侧 shooterset：逐角色 × 逐火力档 =================
 *
 * **现象（owner 单卡实跑）**：「妖侧子机目前似乎发射的都是同一个弹幕，火力大概 4P」——
 *   一句里有两条事实，各有一个根因：
 *   ① 「都是同一个弹幕」：第一版只做了**一格** `YK_SHOT_SET_YOUKAI = 0x16`，内容 = TH08
 *      `ply03as.sht` 的 slot 4（**妖夢＆幽々子**的妖侧、满 P 档）⇒ 四个机体**共用**这一套，
 *      连角色都不分。
 *   ② 「火力大概 4P」：TH08 的档就是「4.00」（P=128）那一档（见下），所以读数完全正确 ——
 *      它确实一直是满 P 档的妖夢弹。
 *
 * **一手**（全文 `engine/sht/th08/01-youkai-shootersets.md`）：TH08 的 `.sht` 里
 * **槽 = 火力档**（生成器 `th08:0x450f60` 的档位循环 `th08:0x451005`–`0x451020`：
 * `get_current_power() >= [slot+4]` 就换下一档），阈值实测 `8/24/48/80/128/999`；
 * 两作 P 的显示单位都是 x.xx、上限 4.00（TH08 `P/32`、TH18 `P/100`）
 * ⇒ TH18 档 `k`（P=100k）折算成 TH08 的 P=32k ⇒ **正好落在 slot k** ⇒ **`档 k ↔ slot k`**。
 *
 * ⇒ 本卡按 **(角色, 火力档)** 取组：`pl0X.sht` **本来就是逐角色的文件**，
 *   所以「同一槽号在不同文件里放不同角色的行」就够 —— 不需要更多槽位。
 *   表由 `assets/sht/append_shooterset.py` 生成（`CE_SHT_SET_YOUKAI_R0..S4`，名字里的 R/M/S
 *   = TH08 的角色 0/1/2 = 霊夢・八雲紫 / 魔理沙・アリス / 咲夜・レミリア）。
 *
 * ⚠ **早苗（角色 3）不在这 15 格里**：她的妖侧是 TH16 的射命丸文，而本批没有 TH16 的行数据
 *   ⇒ 她**回落**到 `YK_SHOT_SET_YOUKAI`（0x16，那一格保留着；内容仍是 `ply03as` 满 P 档）。
 *   这是**已知缺口**，见报告的 🔴（要补需要一轮 TH16 `pl03*.sht` 的抽取）。*/
static const yk_sets_t s_yk_sets = {
    YK_SHOT_SET_YOUKAI,            /* 0x16：早苗的回落格（也是「生成物没到」时的唯一格）*/
    CE_SHT_SET_YOUKAI_R0,          /* 15 格的第一格（逐档组）*/
    YK_YOUKAI_TIER_SETS            /* = 3 角色 × YK_SHOT_TIERS */
};
/* 槽号布局是**生成物**给的（`sht_ids.h`）⇒ 用编译期断言把「下标 = 槽号」钉死：
 * 卡侧按 `first + 角色*档数 + 档` 算，生成序列一旦重排（插在中间）这里当场编译失败，
 * 而不是静默拿错组（照 111/117 那两处 `_Static_assert` 的先例）。*/
_Static_assert(CE_SHT_SET_YOUKAI_M0 == CE_SHT_SET_YOUKAI_R0 + YK_SHOT_TIERS,
               "youkai tier sets must be laid out R0..R4, M0..M4, S0..S4 in this exact order");
_Static_assert(CE_SHT_SET_YOUKAI_S0 == CE_SHT_SET_YOUKAI_M0 + YK_SHOT_TIERS,
               "youkai tier sets must be laid out R0..R4, M0..M4, S0..S4 in this exact order");
_Static_assert(YK_YOUKAI_TIER_SETS == 3 * YK_SHOT_TIERS,
               "YK_YOUKAI_TIER_SETS must cover 3 characters x YK_SHOT_TIERS tiers");

/* 本帧该用哪一格。**只读表，不做运行期探测**（探测在 `yk_youkai_shotset_installed` 里）。*/
static int yk_shot_set_for(int character, int tier)
{
    if (character < 0 || character >= 3) return YK_SHOT_SET_YOUKAI;   /* 早苗 / 越界 ⇒ 回落格 */
    if (tier < 0) tier = 0;
    if (tier >= YK_SHOT_TIERS) tier = YK_SHOT_TIERS - 1;
    return (int)s_yk_sets.tier_first + character * YK_SHOT_TIERS + tier;
}

/* 妖侧 shooterset 装上没有：读那一格记录的首字节 `fire_rate`（i8，`engine/sht/th18/02` §1）。
 * 我们的记录 = TH08 `ply03as.sht` 满 P 档 9 行，`fire_rate ∈ {3,5,6,10}`（`assets/sht/append_shooterset.py`
 * 的 `REPLACE` 覆盖原生那格 0x16）；原生原值 / build/sht 没重跑时那格还是 `0`
 * ⇒ `yk_shotset_usable` 返回 0（装备卡路径 `0x40aa4f` 的 `idiv` 没有零分支）。
 * `< 0` = 终止符 ⇒ 也不可用。**运行期自检**，不靠一个手写的「资产已装」开关。*/
static int yk_youkai_shotset_installed(int set)
{
    uint8_t *p = CE_PLAYER();
    uint8_t *sht;
    uint8_t *row;
    if (!p) return 0;
    sht = *(uint8_t **)(p + CE_PLAYER_SHT_PTR);
    if (!sht) return 0;
    row = *(uint8_t **)(sht + YK_SHT_SET_ROW + (size_t)set * 4);
    /* 空组（只有终止符，如 pl03 上那 15 格）⇒ 首字节 = 0xFF ⇒ `-1` ⇒ 不可用 ⇒ 回落。
     * 这一条就是「早苗自动走 0x16」的机制：不需要按角色写死分支。*/
    return row && yk_shotset_usable((int)*(const int8_t *)row);
}

/* 本帧真正要用的那一格：先按 (角色, 档) 取；那一格不可用（没装 / 是空组）就回落到 `0x16`。*/
static int yk_youkai_shotset_pick(int character, int tier)
{
    int set = yk_shot_set_for(character, tier);
    if (yk_youkai_shotset_installed(set)) return set;
    return YK_SHOT_SET_YOUKAI;
}

/* ============================== 回调 ============================== */
/* +0x00：卡对象可能在局中被重建（每关开始引擎对卡组每张卡再调一次 ctor）⇒ 静态表在这里复位。*/
/* =====================================================================================
 * ★★ 2026-09-22（owner：「那就先做人妖槽以及永点VM池」）：**人妖槽 + 永点（刻符）**
 *
 * 一手全文：`engine/player/th08/03-youkai-body-and-gauge.md` §2（人妖槽）与
 *           `engine/item/th08/01-time-orbs.md`（刻符：生成点 / 运动 / 收集 / 资产）。
 * 纯逻辑（限值 / 判定 / 几何 / PIV 公式）在 `youkai_core.{h,c}` §3.10/§3.11，这里是引擎侧。
 *
 * 三件事：
 *   ① **人妖槽**：TH18 **没有**这套字段与写点（三条独立负证据见该文 §2.5）⇒ 卡侧自建一个 i16 +
 *      6 个限值（数值抄一手写点 `th08:0x44d9ee`–`0x44da8f`）+ 4 个判定 + TH08 那 7 个写入者里
 *      **能在 TH18 上接到的那些**（逐条见下方各自注释）。
 *   ② **永点（刻符）VM 池**：TH08 的刻符是**真道具实体**（type 7，一生成就是 state 3 的「必然归航」），
 *      本卡用**一个 VM 池**当载体（owner 口径：「刻符要建一个相对更大的 VM 池子；默认自动吸收；
 *      中途暴毙 ⇒ 变成类 P 点」）。贴图/脚本逐条照抄 TH08（`etama.anm` sprite179 + script68）。
 *   ③ **永点 → PIV**：`PIV += ((delta + (total&1))/2)×10`（`th08:0x418220`）⇒ 按 §BE 详录 #43 的口径
 *      作为 `ΔG_08` **并进 `ecx` 声明**（`ce_score_family_declare(FAMILY_08)` + `ce_score_declare_delta`）。
 *
 * ★ 铁律：长持续效果要交代「被删掉会怎样」—— 刻符 VM 与 HUD 四件 VM 的生命周期全部经过
 *   `yk_orb_kill_all` / `yk_gauge_hud_kill`，`on_delete`（虚表 +0x50，商店里唯一跑得到的收尾点）
 *   与 `on_run_reset` 都调；HUD 槽位由 `ce_layer_drop_card` 自动释放。
 * ===================================================================================== */

/* 追加进 `pl0X.anm` 的**刻符 / 人妖槽 HUD**脚本号（生成物 `pl_anm_ids.h`）。
 * ⚠ 四个文件的号互不相同（各从自己的原生脚本数往后接）⇒ 必须按机体查表，不能写死一个数。*/
static const int32_t YK_ORB_SCRIPT[4] = {
    CE_PL_ANM_SCRIPT_YKORB_PL00, CE_PL_ANM_SCRIPT_YKORB_PL01,
    CE_PL_ANM_SCRIPT_YKORB_PL02, CE_PL_ANM_SCRIPT_YKORB_PL03,
};
static const int32_t YK_GAUGE_SCRIPT[4][4] = {   /* [机体][0 槽条 / 1 人间图标 / 2 妖怪图标 / 3 游标] */
    { CE_PL_ANM_SCRIPT_YKGAUGE_BAR_PL00, CE_PL_ANM_SCRIPT_YKGAUGE_HUMAN_PL00,
      CE_PL_ANM_SCRIPT_YKGAUGE_YOUKAI_PL00, CE_PL_ANM_SCRIPT_YKGAUGE_CURSOR_PL00 },
    { CE_PL_ANM_SCRIPT_YKGAUGE_BAR_PL01, CE_PL_ANM_SCRIPT_YKGAUGE_HUMAN_PL01,
      CE_PL_ANM_SCRIPT_YKGAUGE_YOUKAI_PL01, CE_PL_ANM_SCRIPT_YKGAUGE_CURSOR_PL01 },
    { CE_PL_ANM_SCRIPT_YKGAUGE_BAR_PL02, CE_PL_ANM_SCRIPT_YKGAUGE_HUMAN_PL02,
      CE_PL_ANM_SCRIPT_YKGAUGE_YOUKAI_PL02, CE_PL_ANM_SCRIPT_YKGAUGE_CURSOR_PL02 },
    { CE_PL_ANM_SCRIPT_YKGAUGE_BAR_PL03, CE_PL_ANM_SCRIPT_YKGAUGE_HUMAN_PL03,
      CE_PL_ANM_SCRIPT_YKGAUGE_YOUKAI_PL03, CE_PL_ANM_SCRIPT_YKGAUGE_CURSOR_PL03 },
};

/* 字体脚本号按机体查表（生成物宏名带机体号；四个文件的号互不相同）*/
static const int32_t YK_GAUGE_SCRIPT_FONT[4] = {
    CE_PL_ANM_SCRIPT_YKGAUGEFONT_PL00, CE_PL_ANM_SCRIPT_YKGAUGEFONT_PL01,
    CE_PL_ANM_SCRIPT_YKGAUGEFONT_PL02, CE_PL_ANM_SCRIPT_YKGAUGEFONT_PL03,
};
/* 字形 sprite 基号按机体查表（0..9 = '0'..'9'、10 = '%'、11 = '.'、12 = '-'）*/
static const int32_t YK_GAUGE_FONT_BASE[4] = {
    CE_PL_ANM_YKGAUGEFONT_BASE_PL00, CE_PL_ANM_YKGAUGEFONT_BASE_PL01,
    CE_PL_ANM_YKGAUGEFONT_BASE_PL02, CE_PL_ANM_YKGAUGEFONT_BASE_PL03,
};

/* 自机自己的 anm（`player+0xc`）。⚠**不是** `player+0x10`：那一格开火期间被引擎换成 ability.anm
 * （engine.h 的注）⇒ 刻符 / HUD 的 VM 必须挂在 pl0X.anm 那一页上（贴图按 VM 自己那一页取）。*/
static void *yk_player_anm(void)
{
    uint8_t *p = CE_PLAYER();
    return p ? *(void **)(p + CE_PLAYER_ANM) : 0;
}

/* ---- 永点（刻符）VM 池 -------------------------------------------------------------------
 * ❓ **自定：池容量**（owner 只说「相对更大」）：64。TH08 那边是道具池（`ItemManager` 自己的数组，
 *    实参 `push 8` 一类），本卡不共享那个池 ⇒ 自己定一个。满 ⇒ **放弃这一颗**（不排队、不崩），
 *    并在 `s->orb_full` 上记账（诊断日志用）。*/
#define YK_ORB_MAX            64
/* ❓ **自定：世界层**。TH08 的刻符是道具实体；TH18 里同档的是「场上实体」那一层 =
 *    6..11（`engine/anm/th18/02-render-stages.md`），使魔跟随 VM 用的就是 7（`YK_FAM_ANM_LAYER`）
 *    ⇒ 刻符同层。脚本自己没有 `layer()` ⇒ 这个参数是唯一的层来源。*/
#define YK_ORB_ANM_LAYER      7
/* ★★ 一手（owner 第 4 条，2026-09-22 晚）—— 见 `youkai_core.h` §3.11 的常量块：
 *   · **归航速度**：`[ds:0x18b896c]+0x14` = 当前 `.sht` 头的 `itemCollectSpeed`
 *     = **10.0**（pl00/pl02/pl03）/ **12.0**（pl01）⇒ 取代原来 ❓自定的 3.0
 *     （owner 的原话「很慢」就是它：3.0 vs 10.0）。
 *   · **「刚开始就满速」**：擦弹那一路在 TH08 里**直接就生 state 1**（`th08:0x431650` 一族
 *     `spawn_item(type 7, state 1)`）⇒ 一生成就满速归航、没有加速段。原来我们把它也按 state 3 生，
 *     于是先飘 ~40 帧（`vy = −2.0 + 0.05/帧`）再归航 —— 那正是 owner 说的「像是有一个很小的加速度」。
 *     「击破敌机」那一路一手是 state 3（`th08:0x42b048`）⇒ 保持不变。
 *   · **拾取**：一手是**接触玩家的抓取盒**（`Player::CalcItemBoxCollision`，`ItemManager.cpp:288`），
 *     盒半宽 = `[.sht+0x18]/2`（12 / 16）+ 刻符半宽 8 ⇒ 半径口径 **20 / 24**。
 *   · **下落（state 0）**：一手 `vx = 0`、`vy` 夹 `>= −2.2`、`vy += 0.03`（终速 3.0）；
 *     **掉到游玩区下沿 + 16（= 464）就整颗删掉、不给分** —— ⚠ 原来我们写成「`y <= 自机 y + 16` 就收」，
 *     那是把 `arcadeRegionSize.y + 16` 那条**损失线**读成了自机身上的回收线（方向也反了）。*/
#define YK_ORB_ANM_LAYER      7       /* ❓自定：世界层（脚本自己没有 `layer()`；同使魔跟随 VM）*/

/* 一手：出生初速 `vy = −2.0 − rand(0.2)`、`vx = rand(±0.6)`（`th08:0x4402cb`–`0x4402e9`）； */
#define YK_ORB_VY0           (-2.0f)
#define YK_ORB_VY0_RAND       0.2f
#define YK_ORB_VX_RAND        0.6f
/* 一手：`vy += 0.05`（`th08:0x4406ac`，常量 `ds:0x4b5b38`），`vy > 0` 那一刻转归航（`th08:0x4406e4`）。*/
#define YK_ORB_GRAVITY        0.05f
/* 一手：中弹/暴毙那一刻把**归航**的刻符打回下落态：`vx = 0`、`vy = −0.9`、state = 0
 *（`th08:0x441567`/`:0x441574` 的批量收尾 与 `th08:0x4408e0`/`:0x4408f0` 的每帧判定，两条同结果）。*/
#define YK_ORB_VY_DEAD       (-0.9f)

typedef struct {
    uint32_t anm_id;      /* 0 = 空槽 */
    float    x, y;        /* 640 空间（世界层 VM 的实体坐标与 ECL 同一套，`ufo_body_vm_follow` 的注）*/
    float    vx, vy;
    uint8_t  state;       /* 一手：3 = 出生上浮（必转 1）、1 = 归航、0 = 下落（类 P 点）*/
    uint8_t  used;
    uint16_t _pad;
} yk_orb_t;

static yk_orb_t s_orb[YK_ORB_MAX];
static unsigned s_orb_live;                 /* 诊断用：当前活着几颗 */

static void yk_orb_kill(yk_orb_t *o)
{
    if (!o || !o->used) return;
    if (o->anm_id) ce_anm_delete(o->anm_id);  /* 按 id 标删（`0x488cf0`）；VM 自己也会 static() 不走 delete */
    o->anm_id = 0;
    o->used = 0;
}

static void yk_orb_kill_all(void)
{
    unsigned i;
    for (i = 0; i < YK_ORB_MAX; ++i) yk_orb_kill(&s_orb[i]);
    s_orb_live = 0;
}

/* 生成一颗刻符（TH08 的 `ItemManager::spawn_item(pos, 7, state)`，`th08:0x4400a0` 的 type 7 那一档）。
 * `state` = **一手的出生状态**，按来源取（见 `youkai_core.h` §3.11 的常量块）：
 *   · `YK_ORB_STATE_HOMING`(1) = **擦弹**那一路（`th08:0x431650` 一族 `spawn_item(type 7, 1)`
 *     ⇒ 一生成就满速归航、没有加速段，owner 要的「刚开始就满速」）；
 *   · `YK_ORB_STATE_SPAWN`(3) = **击破敌机**那一路（`th08:0x42b048` ⇒ 先上浮、`vy > 0` 再归航）。*/
/* ★★★ 2026-09-22（#25 / 一手）：**三条来源 × state × 何时可被收**（逐站点读 push，全文
 * `engine/item/th08/01-time-orbs.md` §5.4）。这张表就是 owner 那三句话的编码：
 *
 * | 来源 | 出生态（我们） | 上抛/「无敌」 | 何时可被收 |
 * | --- | --- | --- | --- |
 * | 通用击破（`th08:0x42b03b push 3`）| `YK_ORB_STATE_SPAWN` | **≈40–44 帧**（`vy = −2.0 − rand(0.2)`、`+0.05/帧`）| `vy > 0` 转归航之后 |
 * | **極端人/妖那一颗**（`0x42da41 push 1`）| `YK_ORB_STATE_HOMING` | **0 帧** | **生成当帧**（若已在抓取盒内）|
 * | 击破符卡螺旋 / 擦弹（type 0xa → type 7）| `YK_ORB_STATE_SPAWN` | ≈40–44 帧 | `vy > 0` 之后 |
 * | 消弹 / `cancel_autocollect` | `YK_ORB_STATE_HOMING` | 0 帧 | 生成当帧 |
 *
 * ⚠ **「无敌」的正确含义（别写成无敌帧）**：state 3/5 被**收集分派显式排除**
 *   （`th08:0x4409f2`–`0x4409ff`：`movsx ecx,[item+0x2d7]; cmp ecx,3; je 0x440c37`）——
 *   上抛期间**吸收判定根本不会跑**，但道具本身仍是普通道具（会掉出下沿被丢失线吃掉、也会被清屏）。
 * ⚠ 我们的 `YK_ORB_STATE_SPAWN`(3) 同时覆盖 TH08 的 **state 3 与 state 5**：两者逐指令同形
 *   （`0x42b03b` 的 state 3 与 `0x440124` 改写出的 state 5 都是「vy 上抛 → `vy > 0` 转归航」，
 *   且都被收集分派排除）⇒ 一个常量就够，不必再造一个 5。*/
static void yk_orb_spawn(yk_card_t *s, float x, float y, int state)
{
    yk_orb_t *o = 0;
    unsigned i;
    uint32_t id;
    for (i = 0; i < YK_ORB_MAX; ++i) if (!s_orb[i].used) { o = &s_orb[i]; break; }
    if (!o) {
        if (s) ++s->orb_full;
        return;
    }
    {
        int ch = yk_current_character();
        if (ch < 0 || ch > 3) ch = 0;
        id = ce_anm_spawn(yk_player_anm(), YK_ORB_SCRIPT[ch], YK_ORB_ANM_LAYER);
    }
    if (!id) {                                  /* VM 建不出来 ⇒ 这颗不要（不崩、不排队）*/
        if (s) ++s->orb_full;
        return;
    }
    o->anm_id = id;
    o->used = 1;
    o->state = (uint8_t)((state == YK_ORB_STATE_HOMING) ? YK_ORB_STATE_HOMING : YK_ORB_STATE_SPAWN);
    o->x = x;
    o->y = y;
    /* 一手初速（`th08:0x4402cb`–`0x4402e9`）：`vx = rand(±0.6)`、`vy = −2.0 − rand(0.2)`。
     * ⚠ 这两条**只在 state 3** 写（`spawn_item` 的 state 3 分支）；state 1 的初速是 `cancel_autocollect`
     * 那一档 `(0, −0.5, 0)`（`th08:0x441407`–`0x44142c`），而**当帧就会被归航重设成满速**
     * （`th08:0x4407f6` → `:0x4408a5`）⇒ 初始值对可见行为无影响，这里照样给一组合理初值。
     * RNG 走 `ce_rand()`（SDK 的 replay-safe RNG）——**不引 libm、不进 x87**。*/
    o->vx = ((float)(ce_rand() % 1201u) / 1000.0f - 0.6f);                    /* ❓ 用整数取模近似 randf */
    o->vy = (state == YK_ORB_STATE_HOMING) ? -0.5f   /* `th08:0x441407` 那一档的归航初速 */
                                           : (YK_ORB_VY0 - ((float)(ce_rand() % 1001u) / 1000.0f) * YK_ORB_VY0_RAND);
    ce_anm_set_pos(id, x, y, 0.0f);
    ++s_orb_live;
}

static int yk_orb_collect(yk_card_t *s)
{
    /* 收集结算（一手 `Item::collect_time_orb` `th08:0x4412b0`）：
     *   ① `AddTimeOrbs(1)`（`th08:0x44137a`）⇒ 刻符 +1、**PIV += ((1+(total&1))/2)×10**（见 core §3.11）；
     *   ② 人妖槽 ±111（`th08:0x44139e`/`:0x4413b0` → `th08:0x4413cb`，聚焦 = +111）。
     * ⚠ 另外两笔本轮**不接**（分数那一路与符卡 +8000），原因见下面那段「本轮的取舍」。*/
    if (!s) return 0;
    yk_piv_add(&s->piv, &s->orbs, 1);
    /* ★★★ 2026-09-22（P3 第二批）：`AddToYoukaiGauge` 的**第一道闸** —— 射击中（TH08 的
     * `shotActive != 0`）带 `param_2 == 0` 的加数**整条丢掉**（一手 `th08:0x43c0c2`/`:0x43c0cb`，
     * `0x17d6ed4` = `g_Player.shotActive`）⇒ 本卡这里照抄：射击中不收槽（PIV 那笔照记）。*/
    if (!yk_gauge_gate_blocks(yk_shooting()))
        s->gauge = yk_gauge_add(s->gauge, yk_gauge_orbs_delta(CE_PLAYER_FOCUSED()));
    ++s->orb_taken;
    return 1;
}

/* ❓ 自定：**多项式近似 sin / cos**（不引 libm、不进 x87）。只用于「刻符撒在半径 128 的环上」
 *   这个**视觉**位置 —— 不参与任何判定与计分 ⇒ 精度足够。`ang` 归一到 [−π, π] 后用
 *   Bhaskara 一类的三点近似（误差 < 2%），乘 128 的落点偏差 < 3 px。*/
static float yk_cos_approx(float a)
{
    float s;
    const float PI = 3.14159265f, TWO_PI = 6.28318531f;
    while (a > PI) a -= TWO_PI;
    while (a < -PI) a += TWO_PI;
    /* cos(a) ≈ 1 − a²/2 + a⁴/24 − a⁶/720（泰勒到 6 阶，|a| ≤ π 时误差 < 0.5%）*/
    s = a * a;
    return 1.0f - s * 0.5f + s * s * (1.0f / 24.0f) - s * s * s * (1.0f / 720.0f);
}
static float yk_sin_approx(float a)
{
    float s;
    const float PI = 3.14159265f, TWO_PI = 6.28318531f;
    while (a > PI) a -= TWO_PI;
    while (a < -PI) a += TWO_PI;
    /* sin(a) ≈ a − a³/6 + a⁵/120 − a⁷/5040 */
    s = a * a;
    return a * (1.0f - s * (1.0f / 6.0f) + s * s * (1.0f / 120.0f) - s * s * s * (1.0f / 5040.0f));
}

/* ❓ 自定：**Newton 迭代开方**（6 次）。为什么要它：归航要把速度归一化，而本仓 `.c` 不引 libm
 *   （108 的子机弹簧那一节也是同一条纪律）。纯 `* / +` ⇒ `-mfpmath=sse` 下全是 SSE 指令，
 *   `make dllx87` 门不受影响；迭代次数固定 ⇒ **回放确定**。输入 `v = dx²+dy²`，量级 < 1e6 时 6 次足够。*/
static float yk_fsqrt(float v)
{
    float x;
    int i;
    if (!(v > 0.0f)) return 0.0f;
    x = (v > 1.0f) ? v : 1.0f;
    for (i = 0; i < 6; ++i) x = 0.5f * (x + v / x);
    return x;
}

/*
 * ❓ **本轮的取舍（写在这里免得别人以为漏了）**：
 *   · 刻符入账的**分数**那一路先不接 —— `yk_orb_score()` 需要「本关蓝点数 / 本局蓝点数」两个计数，
 *     TH18 侧只有 `CE_ADDR_MONEY_ITEMS`（吃到的金钱道具数）这一个近似物；两者语义不同，
 *     硬套会把「分数」这条线变成自定。⇒ **只做 PIV 那条线**（owner 要的正是它），分数留待 owner 拍。
 *   · 符卡加成 +8000 同理（TH18 的符卡分与 TH08 不是一套）。
 *   · `Timer::current_is_eq_(&0x18b89d4, 0)` 那道门（TH08 里「收刻符什么时候动人妖槽」的条件）
 *     ❓ 语义未钉 ⇒ 本卡**无条件**动（登记 `TUNING.md`）。
 */

/* 每帧：刻符的运动（一手 `ItemManager::on_tick__` 的三个 state 分支 + 卡自己的池遍历）。
 * 返回本帧收了几颗。*/
static int yk_orbs_tick(yk_card_t *s)
{
    uint8_t *p = CE_PLAYER();
    float px, py;
    float home_speed, pick_r;
    int taken = 0;
    unsigned i;
    int dead, ch;
    if (!p) return 0;
    px = *(float *)(p + CE_PLAYER_X);
    py = *(float *)(p + CE_PLAYER_Y);
    /* ★★ 一手（owner 第 4 条）：归航速度 / 拾取半径**按机体**取当前 `.sht` 头的值
     * （见 `youkai_core.h` §3.11）：10（pl00/02/03）/ 12（pl01）；拾取 20 / 24。*/
    ch = yk_current_character();
    if (ch < 0 || ch > 3) ch = 0;
    yk_orb_motion_params(ch, &home_speed, &pick_r);
    /* 一手（`th08:0x440866`/`:0x4408f0`）：玩家 dead 段（TH08 是 state 1/2）⇒ 归航的刻符立刻回到
     * 下落态（「中途暴毙 ⇒ 变成类 P 点」）。TH18 的玩家状态机在 `player+0x476ac`，**state 2 = 死亡段**
     * （`Player__commit_death_and_enter_state2` `th18:0x45d090`）。*/
    dead = (*(const int32_t *)(p + CE_PLAYER_STATE) == 2);   /* TH18 的 state 2 = 死亡/复活段 */
    for (i = 0; i < YK_ORB_MAX; ++i) {
        yk_orb_t *o = &s_orb[i];
        float dx, dy, dist;
        if (!o->used) continue;
        if (!o->anm_id || !ce_anm_get_vm(o->anm_id)) {
            /* ★★★ 2026-09-22（P5 / th08 #19「人形态击中得到的永点……上抛之后下落直接消失，
             * 动画上不吸收到角色这里来」）：**这里原来直接 `yk_orb_collect()` 记账收下**、
             * 顺手把那一颗停掉 —— 观感就是「飞向角色的途中凭空消失、没进角色身上」，而
             * `orb_taken` 还在涨 ⇒ 正是 owner 描述的那一幕。
             * 一手复核：TH08 里**没有**「刻符自己消失」的路径（丢失线只对 state 0 生效、而且要走
             * 464 下沿）⇒ 这条 ⚠ 只能是我们自己的句柄死亡路径。
             * 修法 = **VM 没了 ≠ 这一颗没了**：清句柄，然后在**原位重起一支**（脚本/层与出生时同一套）
             * —— 飞行坐标与 state 都在我们自己的池里，跟 VM 无关；收下与否只由抓取盒决定。*/
            int ch2 = yk_current_character();
            if (ch2 < 0 || ch2 > 3) ch2 = 0;
            o->anm_id = ce_anm_spawn(yk_player_anm(), YK_ORB_SCRIPT[ch2], YK_ORB_ANM_LAYER);
            if (o->anm_id) ce_anm_set_pos(o->anm_id, o->x, o->y, 0.0f);
            else if (s) ++s->orb_full;              /* 重起也失败 ⇒ 这颗只当「看不见但仍在飞」*/
        }
        if (dead && o->state == YK_ORB_STATE_HOMING) {
            o->state = YK_ORB_STATE_FALL;
            o->vx = 0.0f;
            o->vy = YK_ORB_VY_DEAD;                      /* `th08:0x441574` 的 −0.9（每帧版是 −0.7）*/
        }
        if (o->state == YK_ORB_STATE_SPAWN) {
            /* 出生上浮：`vy += 0.05`；`vy > 0` 那一刻转归航（一手 `th08:0x4406ac` / `:0x4406e4`）。*/
            o->vy += YK_ORB_GRAVITY;
            if (o->vy > 0.0f) o->state = YK_ORB_STATE_HOMING;
        }
        if (o->state == YK_ORB_STATE_FALL) {
            /* 一手（`ItemManager.cpp:268-276` / `th08:0x44093e` 族）：`vx = 0`、`vy` 夹 `>= −2.2`、
             * `vy += 0.03`（终速 3.0）。**掉出游玩区下沿 + 16 就整颗删掉、不给分**（= 损失）。*/
            o->vx = 0.0f;
            if (o->vy < YK_ORB_FALL_VY_MIN) o->vy = YK_ORB_FALL_VY_MIN;
            o->vy += YK_ORB_FALL_GRAVITY;
            if (o->vy > YK_ORB_FALL_TERMINAL) o->vy = YK_ORB_FALL_TERMINAL;
        }
        /* 收集 = **接触玩家的抓取盒**（一手 `Player::CalcItemBoxCollision`，`ItemManager.cpp:288`）：
         * 只对 state 0/1 生效（TH08 把 state 3 从这条判据里摘出去 ⇒ 上浮期间收不到）。*/
        if (o->state != YK_ORB_STATE_SPAWN) {
            dx = px - o->x;
            dy = py - o->y;
            dist = dx * dx + dy * dy;
            if (dist <= pick_r * pick_r) {
                yk_orb_collect(s);
                yk_orb_kill(o);
                if (s_orb_live) --s_orb_live;
                ++taken;
                continue;
            }
        }
        if (o->state == YK_ORB_STATE_HOMING) {
            /* 归航：每帧把速度**重设**成指向自机、模长 = 一手速度（`th08:0x4407f6` →
             * `:0x440886`(方向) / `:0x4408a5`(造速度)）⇒ **没有加速段**，当帧就是满速。*/
            dx = px - o->x; dy = py - o->y;
            dist = dx * dx + dy * dy;
            dist = yk_fsqrt(dist);              /* ❓ 自定：Newton 近似开方（见上）*/
            if (dist > 0.0001f) { o->vx = dx / dist * home_speed; o->vy = dy / dist * home_speed; }
        }
        o->x += o->vx;
        o->y += o->vy;
        if (o->state == YK_ORB_STATE_FALL && o->y >= YK_ORB_LOST_Y) {
            /* 一手：下沿那一颗**不给分**直接消失（`ItemManager.cpp:271` 的 `Delete()`；
             * ⚠ 那条线是 `arcadeRegionSize.y (448) + 16`，**不是**自机身上的回收线）。*/
            yk_orb_kill(o);
            if (s_orb_live) --s_orb_live;
            continue;
        }
        ce_anm_set_pos(o->anm_id, o->x, o->y, 0.0f);
    }
    return taken;
}

/* ============ ★★★ #25 击破符卡的「螺旋撒点」（一手参数逐条照抄，见 `youkai_core.h` §3.11b）================
 *
 * owner 口径：「击破符卡会有一个**螺旋式**的撒永点，无敌一段时间后自动满速吸收」。
 * 一手（`th08:0x416b10` 的喷点主体 `0x417533`–`0x4176e1`，逐指令）：每 **8** 帧一批；
 * `θ(t) = ((t − 10) × 2π/40) − π/2`（**9°/帧、40 帧一圈**、`t = 10` 正上方）；半径 **128**；
 * 绕**符卡所有者（boss）的位置**；一批 = `min(额度, 7)` 同角 + **固定 6 颗 +π**；
 * 额度两段扣减；出生态 = `type 0xa → type 7 + state 5`（我们的 `YK_ORB_STATE_SPAWN`）。
 *
 * ⚠ **「无敌」这个词的正确含义**（别写成无敌帧）：state 5/3 只是被**收集分派显式排除**
 *   （`th08:0x4409f2`–`0x4409ff`：`cmp [item+0x2d7],3 / je 跳过`）⇒ 上抛期间**收不到**它，
 *   但它照样是普通道具（会被丢失线吃掉、也会被清屏）。所谓「之后自动满速吸收」= `vy > 0` 转
 *   state 1 之后每帧把速度重设成满速归航（`th08:0x44088e` = `[.sht+0x14]` = 10/12）。
 *
 * ❓ **一处口径**（登记 `TUNING.md`）：TH08 的额度 `spellcard[+0x10]` 由它自己的
 *   `Spellcard::ecl_spell_end`（`th08:0x4161b0`）按**符卡计时器**算出 100 / 700 / 1000
 *   （用时越短越高；「不算击破」那一位给 700）；TH18 的符卡对象在我们的 SDK 里没有对应的
 *   计时器读法 ⇒ 本卡取**满额 1000**（= 用时最短那一档）。要按 TH18 符卡用时折算只改这一个常量。*/
#define YK_SPIRAL_QUOTA_FULL 1000u   /* ❓ 口径：TH08 满额档（`th08:0x4162ef` 写 0x3e8）*/

static void yk_spiral_tick(yk_card_t *s)
{
    uint8_t *boss;
    const float *bp;
    int k, i;
    float th, cs, sn;
    if (!s || !s->spiral_quota) return;
    if (s->spiral_t % YK_SPIRAL_BATCH_FRAMES) { ++s->spiral_t; return; }   /* 一手：每 8 帧一批 */
    boss = yk_boss();
    bp = boss ? ce_enemy_pos(boss) : 0;
    if (!bp) { s->spiral_quota = 0; return; }        /* boss 没了 ⇒ 这一轮作废（下一次符卡重来）*/
    yk_spiral_angle((int)s->spiral_t + 10, &th);     /* `t` 从 10 起算 ⇒ 第一批 = 正上方 */
    k = yk_spiral_first_count((int)s->spiral_quota);
    cs = yk_cos_approx(th);
    sn = yk_sin_approx(th);
    for (i = 0; i < k; ++i)                          /* 第一段：同一角、k 颗 */
        yk_orb_spawn(s, bp[0] + cs * YK_SPIRAL_RADIUS, bp[1] + sn * YK_SPIRAL_RADIUS,
                     YK_ORB_STATE_SPAWN);
    if (k) s->spiral_quota = (uint32_t)((int)s->spiral_quota - k);
    /* 第二段：**固定 6 颗**、角 = 第一段 **+π**（一手 `0x417639 push 0x40490fdb` = π；
     * ⚠ 不是 π/2 —— `engine/item/th08/01-time-orbs.md` 的旧值已订正）。*/
    cs = yk_cos_approx(th + 3.14159265f);
    sn = yk_sin_approx(th + 3.14159265f);
    for (i = 0; i < YK_SPIRAL_SECOND_N; ++i)
        yk_orb_spawn(s, bp[0] + cs * YK_SPIRAL_RADIUS, bp[1] + sn * YK_SPIRAL_RADIUS,
                     YK_ORB_STATE_SPAWN);
    s->spiral_quota = (s->spiral_quota > (uint32_t)YK_SPIRAL_SECOND_N)
                        ? (s->spiral_quota - (uint32_t)YK_SPIRAL_SECOND_N) : 0u;   /* 夹到 0 */
    if (!s->spiral_logged) {
        s->spiral_logged = 1;
        ce_log("youkai: spell-card break spiral ON (TH08 `Spellcard::sub_416b10`) - 8-frame batches, "
               "9 deg/frame (40 frames per turn), radius 128 around the boss, %d same-angle orbs + "
               "%d at +pi per batch; orbs spawn in state 5 = toss (NOT collectible until vy > 0 - "
               "that is the 'invincible' in the owner report, not an invulnerability window)",
               YK_SPIRAL_FIRST_MAX, YK_SPIRAL_SECOND_N);
    }
    ++s->spiral_t;
}

/* ---- 人妖槽 HUD（TH08 `ascii.anm` 的 4 支脚本 + 百分比数字）--------------------------------
 * 一手（`AsciiManager::InitializeVms` + `AsciiManager::on_draw`）：
 *   槽条 = script5（sprite155，128×16，`anchorTopLeft`）、人间图标 = script6（sprite153，16×16）、
 *   妖怪图标 = script7（sprite154，16×16）、游标 = script8（sprite152，8×12，**锚点 = 心**）；
 *   百分比数字 = `DrawPercentage`（本卡用共用件的 `ce_hud_number_set` 画，见 ❓ 注）。
 * 四支脚本与贴图**逐条抄进 pl0X.anm**（`build_player_anm.py` 的 `YK_EXTRA_PLAN`；脚本里的
 * `anchorTopLeft()` 按 v2→v8 的 opcode 表翻成 `anchor(1, 1)`）。
 *
 * ⚠ TH08 的脚本自带 `pos()` / `posTime()`（TH08 的 640×480 坐标）：本卡**每帧覆盖位置**
 *   （`ce_gauge_hud()` 按 HUD 货架锚点算），所以那段滑入动画**被覆盖掉**了 —— ❓ 自定，登记 `TUNING`。
 * ⚠ 四支脚本出生时是**隐藏**的（`stopHide()` + `interruptLabel(1)` 停着），要 `ce_anm_interrupt(id, 1)`
 *   才显形（TH08 那边由 `SetGaugeInterrupt` 发）—— 本卡生成后立刻发一次。
 * ⚠ 图标的 x 由**限值**决定（一手 `88 + limit·56/10000`），游标/数字按 gauge 走（core §3.10 的几何）。*/

/* ❓ 自定：HUD 块尺寸（表面 px，喂货架分配器）。宽 = 128×2（槽条 128 个 640 空间 px）、
 *   高 = 视觉 70（数字顶 446 → 槽条底 481 = 35 个 640 空间 px ×2）+ 上下各 4 px pad（照 §6.1 口径）。*/
#define YK_HUD_BLOCK_W 256.0f
#define YK_HUD_BLOCK_H  78.0f

/* ---- 人妖槽的每帧漂移（TH08 `Player::move` 的聚焦状态机，一手）--------------------------------
 * 一手形状（`th08_dec/src/Player.cpp` 那段，逐条地址见 `engine/player/th08/03-...md` §2.2 的
 * 「Player::UpdateMovement 聚焦漂移」一行）：
 *   · **按住低速**：状态保持 ≥ 4 帧后开始爬升，速率 = `min(按住帧数 / 15, 21)`，方向**朝妖**（+）；
 *   · **松开低速**：状态保持 ≥ 30 帧后开始回落，速率按当前档取 **−5 / −3 / −2 / +2 / +3 / +5**
 *     （极妖 −5、中妖 −3、正 −2、未到中人 +2、未到极人 +3、极人 +5）；`|g| <= 9` ⇒ 直接归零。
 * ❓ **自定**：TH08 那两条算式末尾都乘 `g_ShotSpeed`（本作聚焦的射击减速因子，≤ 1）；
 *   TH18 没有同一个全局 ⇒ 本卡取 1（登记 `TUNING.md`）。
 * ❓ **自定**：两个计时器的**精确**语义（`unkE2ad0` / `unkE2ae8` 的重置时机）按上式简化：
 *   换状态时两个都清。方向与速率表是一手的。*/
static void yk_gauge_tick(yk_card_t *s, int focused)
{
    int d, g2;
    int shooting;
    if (!s) return;
    shooting = yk_shooting();
    if (focused != (int)s->gauge_focus_prev) {          /* 换状态 ⇒ 两个计时器清零（❓ 简化）*/
        s->gauge_focus_prev = (uint8_t)(focused ? 1 : 0);
        s->gauge_focus_run = 0;
        s->gauge_hold = 0;
    }
    if (s->gauge_focus_run < 0xffffu) ++s->gauge_focus_run;
    /* ★★★ 2026-09-22（P3 第二批）：**射击中每帧 ±26**（一手 `Player.cpp:445-452`：
     * `AddToYoukaiGauge(0x6590 / shotInterval(999) = 26, param_2 = 1)`，方向 = `shotType & 1`
     * = `isYoukaiMode` ⇒ 我们的口径里 = 低速位）。
     * ⚠ **两条 TH08 语义一起照抄**（都在 `FUN` / `Player` 的一手门里）：
     *   ① 射击中**只**有这一项在动 —— 下面的漂移整段被 `shotActive == 0` 挡住（`Player.cpp:1083`
     *      的 `this->shotActive == 0` 前置）；② 击破 ±200 / 收刻符 ±111 被 `AddToYoukaiGauge`
     *      的第一道闸丢掉（`th08:0x43c0c2`；见 `yk_orb_collect` / `yk_on_enemy_killed` 的注）。*/
    if (shooting) {
        s->gauge = yk_gauge_add(s->gauge, yk_gauge_shoot_delta(focused));
        return;                                         /* ★ 漂移在射击中不跑（一手的前置门）*/
    }
    if (focused) {
        if (s->gauge_focus_run >= 4) {                  /* 一手：`unkE2ad0 >= 4` 才开始动 */
            d = (s->gauge_hold < 300) ? (int)(s->gauge_hold / 15) : 21;
            s->gauge = yk_gauge_add(s->gauge, d);
            if (s->gauge_hold < 300) ++s->gauge_hold;
        }
        return;
    }
    s->gauge_hold = 0;
    if (s->gauge_focus_run < 30) return;                /* 一手：`unkE2ad0 >= 0x1e` 才开始回落 */
    g2 = s->gauge;
    if (g2 > YK_GAUGE_DEAD_ZONE || g2 < -YK_GAUGE_DEAD_ZONE) {
        if (yk_gauge_is_extremely_youkai(g2))        d = -5;
        else if (yk_gauge_is_moderately_youkai(g2))  d = -3;
        else if (g2 > 0)                             d = -2;
        else if (!yk_gauge_is_moderately_human(g2))  d = 2;
        else if (!yk_gauge_is_extremely_human(g2))   d = 3;
        else                                        d = 5;
        s->gauge = yk_gauge_add(s->gauge, d);
    } else {
        s->gauge = 0;                                   /* 一手 `th08:0x44bf61`：死区归零 */
    }
}

/* 涨源①：**击破敌机** ⇒ 人妖槽 ±200（一手 `th08:0x42da2a` 一带：`isYoukaiMode ? +200 : -200`）；
 * 同时（仅在妖侧）撒刻符 —— 一手 `Enemy::sub_42adb0` `th08:0x42b048`（type 7 / state 3）。
 * 数量：一手有**三条按 `shottype` 分的公式**（§4），共用变量 `n` = 该敌的**连锁段数**（`th08:0x41fd40`
 * 沿 `enemy+0x8` 数）。TH18 侧我们读不到那个连锁计数 ⇒ ❓ **自定：取 n = 1**，
 * 走「双人组」那一支 `N = n*2 + 10 = 12`（TH08 的默认档；妖単独同式但常数是 +6）。登记 `TUNING.md`。*/
#define YK_ORB_PER_KILL_CHAIN 1
static void yk_on_enemy_killed(yk_card_t *s, const ce_enemy_death_t *ev)
{
    int focused, i, n;
    float ang;
    if (!s || !ev) return;
    focused = CE_PLAYER_FOCUSED();
    /* ★★★ 2026-09-22（P3 第二批）：同刻符那一笔 —— 射击中 ±200 也被 `AddToYoukaiGauge`
     * 的第一道闸丢掉（一手 `th08:0x43c0c2`；decomp `EnemyManager.cpp:670-677` 传的正是 `param_2 = 0`）。*/
    if (!yk_gauge_gate_blocks(yk_shooting()))
        s->gauge = yk_gauge_add(s->gauge, focused ? 200 : -200);
    if (!focused) return;                               /* 妖逢魔才撒刻符 */
    /* ★★★ 2026-09-22 深夜（owner 口径细化）：**两条一手来源分开实现**（各带自己的出处）。
     *
     * ① **通用击破批**（一手 `th08:0x42b048` = `Enemy::sub_42adb0`，push **state 3**）：
     *    撒在敌人位置 + **半径 128** 的环上（`th08:0x42b03b`，角度 = `randf(2π)`，常量 `0x43000000`）。
     *    **state 3 = 先上抛** —— 而且**上抛期间收不到它**（一手 `th08:0x4409f2`–`0x4409ff`：
     *    收集分派第一句就是 `cmp [item+0x2d7],3 / je 跳过`）⇒ 这正是 owner 记忆里的
     *    「**『无敌』的上抛过程**」，上抛结束（`vy > 0`）才转归航。
     * ② **極端人/妖那一颗**（一手 `th08:0x42da54` = `EnemyManager::on_tick` 的死亡块，
     *    push **state 1**）：那一档是**生成即满速归航**（= owner 说的「**自动吸收**」），
     *    原样**一颗**、不发生在上抛环上（一手就是一次 `call 0x4400a0`）。
     *    ⚠ 一手里这条是「極端人間 **或** 極端妖怪」（`0x42da2a`/`0x42da38` 两问取或）——
     *      判据 = core 的 `yk_orb_instant_absorb(gauge)`。
     * ⚠ 一句口径：**「自动」≠「免半径」** —— 一手唯一吸收判据仍是抓取盒相交
     *   （`th08:0x440a17 call 0x44a5a0`，盒 = `[.sht+0x18]/2` = 20/24）；「自动」的意思是
     *   道具自己飞过来（state 1 当帧满速），玩家不用去够。*/
    n = YK_ORB_PER_KILL_CHAIN * 2 + 10;
    for (i = 0; i < n; ++i) {
        ang = (float)(ce_rand() % 62832u) / 10000.0f;   /* 0..2π 的近似 */
        yk_orb_spawn(s, ev->x + yk_cos_approx(ang) * 128.0f, ev->y + yk_sin_approx(ang) * 128.0f,
                     YK_ORB_STATE_SPAWN);   /* 一手 `th08:0x42b048`：state 3（上抛，期间收不到）*/
    }
    /* ② 極端人/妖的那一颗：state 1（生成即满速归航 = 自动吸收）。位置 = 敌人处（一手 push 的是
     * `enemy+0x2d88` 那份坐标，没有 128 环）。*/
    if (yk_orb_instant_absorb(s->gauge))
        yk_orb_spawn(s, ev->x, ev->y, YK_ORB_STATE_HOMING);
}

static void yk_gauge_hud_kill(yk_card_t *s)
{
    int i;
    if (!s) return;
    if (s->hud_bar)    ce_anm_delete(s->hud_bar);
    if (s->hud_human)  ce_anm_delete(s->hud_human);
    if (s->hud_youkai) ce_anm_delete(s->hud_youkai);
    if (s->hud_cursor) ce_anm_delete(s->hud_cursor);
    s->hud_bar = s->hud_human = s->hud_youkai = s->hud_cursor = 0;
    for (i = 0; i < YK_GAUGE_TEXT_MAX; ++i) {
        if (s->hud_digit[i]) ce_anm_delete(s->hud_digit[i]);
        s->hud_digit[i] = 0;
    }
}

/* 生成四支 VM（UI 列表；`ce_anm_spawn_ui` 的 `pos` 传 NULL ⇒ 由脚本自己的 `pos()` 起手，
 * 下面第一帧就被我们的几何覆盖）。句柄存在卡的私有状态里 ⇒ 自愈：找不到就重建。*/
static void yk_gauge_hud_spawn(ce_card_t *c, yk_card_t *s)
{
    uint32_t *slot[4];
    int i, ch;
    float px;
    (void)c;
    if (!s) return;
    slot[0] = &s->hud_bar; slot[1] = &s->hud_human;
    slot[2] = &s->hud_youkai; slot[3] = &s->hud_cursor;
    ch = yk_current_character();
    if (ch < 0 || ch > 3) ch = 0;
    /* 表面 px / 640 空间 px（= `1/CE_HUD_SURFACE_TO_640`，TH18 = 2）—— 见下面那次 `ce_anm_set_scale`。*/
    px = (CE_HUD_SURFACE_TO_640 > 0.0f) ? (1.0f / CE_HUD_SURFACE_TO_640) : 2.0f;
    for (i = 0; i < 4; ++i) {
        uint32_t id;
        if (*slot[i] && ce_anm_get_vm(*slot[i])) continue;
        id = ce_anm_spawn_ui(yk_player_anm(), YK_GAUGE_SCRIPT[ch][i], 0);
        if (!id) { *slot[i] = 0; continue; }
        *slot[i] = id;
        ce_anm_interrupt(id, 1);          /* 一手：脚本出生是 stopHide 状态，interrupt 1 = 「显形」*/
        /* ★★★ 2026-09-22（P2）：**贴图要 ×2**。四件贴图是从 TH08 `ascii.anm` 原生**原尺寸**切来的
         * （游标 8×12 / 两图标 16×16 / 条 128×16），而 TH18 的表面是 1280×960（TH08 是 640×480）
         * ⇒ 这四支脚本又没有 `resolutionMode`（`th18:0x4063d0` 的 mode 0 支 = **尺寸也 ×1**），
         * 不乘 2 就只有原版一半的线性尺寸 —— owner 实跑 th08 #15「人妖整体的 hud 似乎比原版缩小了 50%」。
         * 一手依据（mode 0 ⇒ 尺寸直通）见 `youkai_core.h` 的 `yk_gauge_hud_t`。*/
        ce_anm_set_scale(id, px, px);
    }
    /* ★★ 2026-09-22 晚（owner：「th08 的贴图搞过来」）：**百分比数字的 7 格字形 VM**。
     * 载体脚本 = TH08 `ascii.anm` script4（`sprite(sprite136); zWriteDisable(1); static();`）
     * 逐条照抄（生成器 `YK_EXTRA_SCRIPTS` 的 `gaugefont` 那一行）⇒ 这里只申请、不摆位置；
     * 每帧由 `yk_gauge_text_sync` 换 sprite / 位置 / 缩放（TH08 那边是同一个 VM 反复换字）。*/
    for (i = 0; i < YK_GAUGE_TEXT_MAX; ++i) {
        uint32_t id;
        if (s->hud_digit[i] && ce_anm_get_vm(s->hud_digit[i])) continue;
        id = ce_anm_spawn_ui(yk_player_anm(), YK_GAUGE_SCRIPT_FONT[ch], 0);
        s->hud_digit[i] = id;
    }
}

/* ★★ 人妖槽的百分比数字：按一手布局逐格写 7 个字形 VM 的 sprite / 位置 / 缩放 / 颜色。
 * 布局（`AsciiManager::DrawPercentage` `th08:0x405e10`）在纯逻辑 `yk_gauge_text_glyphs` 里。
 * ⚠ 不用的格：`alpha = 0`（同 `color(argb)` 的 alpha 通道）⇒ 不留残字。
 * ★★★ 2026-09-22（P2）：**基准点、逐字形的步进（7 / 5 / 2）、以及字形 scale 全部 × `h->px`**：
 *   一手那一套步进是 **640 空间**的量（TH08 的屏幕宽 640），而这批 VM 的脚本没有 `resolutionMode`
 *   ⇒ 位置与尺寸都按 1 单位 = 1 表面 px 走（`th18:0x4063d0` 的 mode 0 直通支，一手见
 *   `youkai_core.h` 的 `yk_gauge_hud_t`）⇒ 不乘 2 就等于「字距只有一半、字也只有一半大」
 *   （owner 实跑 th08 #14「百分比位置完全错位」/#15「整体缩小 50%」）。*/
static void yk_gauge_text_sync(yk_card_t *s, const yk_gauge_hud_t *h, uint32_t col)
{
    yk_gauge_glyph_t g[YK_GAUGE_TEXT_MAX];
    void *anm;
    int n, i, ch;
    if (!s || !h) return;
    ch = yk_current_character();
    if (ch < 0 || ch > 3) ch = 0;
    anm = yk_player_anm();
    n = yk_gauge_text_glyphs(s->gauge, g);
    for (i = 0; i < YK_GAUGE_TEXT_MAX; ++i) {
        uint32_t id = s->hud_digit[i];
        if (!id) continue;
        if (i >= n) {                                     /* 这一格本帧不用 ⇒ 透明掉 */
            ce_anm_set_color(id, 0x00ffffffu);
            continue;
        }
        if (anm) ce_anm_set_sprite(anm, id, YK_GAUGE_FONT_BASE[ch] + (int)g[i].glyph);
        ce_anm_set_pos(id, h->text_x + g[i].dx * h->px, h->text_y + g[i].dy * h->px, 0.0f);
        ce_anm_set_scale(id, g[i].scale * h->px, g[i].scale * h->px);  /* 小数两位 = 0.8×2 = 1.6（一手 + P2 的 ×2）*/
        ce_anm_set_color(id, 0xff000000u | col);           /* 一手：数字同吃那 5 档染色 */
    }
}

static void yk_gauge_hud_update(yk_card_t *s)
{
    yk_gauge_hud_t h;
    float anchor[2];
    uint32_t col;
    if (!s) return;
    anchor[0] = s->hud.x;
    anchor[1] = s->hud.y;
    yk_gauge_hud(s->gauge, anchor, CE_HUD_SURFACE_TO_640, &h);
    col = yk_gauge_color(s->gauge);
    /* ★★★ 2026-09-22（P2）：**四件也每帧重写 scale**（`h.px` = 2）。
     * 为什么每帧写而不是只在 spawn 时写一次：贴图是 TH08 的原生尺寸（游标 8×12 / 图标 16×16 /
     * 条 128×16），要 ×2 才与原版同观感；而 VM 的 scale 是会被别的路径动到的（引擎重建 VM、
     * 脚本自己写、自愈重申请）⇒ 每帧钉死最省心，成本 = 4 次写。理由全文见 `yk_gauge_hud_t` 的注。*/
    if (s->hud_bar) {
        ce_anm_set_pos(s->hud_bar, h.bar_x, h.bar_y, 0.0f);
        ce_anm_set_scale(s->hud_bar, h.px, h.px);
        ce_anm_set_color(s->hud_bar, 0xff000000u | col);   /* 一手：槽条用**同一个**颜色（AsciiManager.cpp:1060）*/
    }
    if (s->hud_human) {
        ce_anm_set_pos(s->hud_human, h.human_x, h.human_y, 0.0f);
        ce_anm_set_scale(s->hud_human, h.px, h.px);
    }
    if (s->hud_youkai) {
        ce_anm_set_pos(s->hud_youkai, h.youkai_x, h.youkai_y, 0.0f);
        ce_anm_set_scale(s->hud_youkai, h.px, h.px);
    }
    if (s->hud_cursor) {
        ce_anm_set_pos(s->hud_cursor, h.cursor_x, h.cursor_y, 0.0f);
        ce_anm_set_scale(s->hud_cursor, h.px, h.px);
    }
    /* ★ 只记一次的**几何自述**（实跑判据）：P2 这一版的分辨率空间之争（640 空间 vs 表面 px）
     * 靠的就是把「我们写进去的那几个数」打在日志里 —— 与屏上看到的位置一比就知道生效的是哪一种。
     * 锚点 = 货架给的那一格左下角（表面 px）。⚠ 参数刻意压到 6 个：`ce_log` 是变参函数，
     * 一次压十几路浮点会把 i686 的寄存器用光，连带 `sdk.h` 里那些内联汇编的函数报
     * 「inline assembly requires more registers than available」（2026-09-22 当场踩到过一次）。*/
    if (!s->logged_hudgeo) {
        s->logged_hudgeo = 1;
        ce_log("youkai: gauge HUD geometry (surface px; these scripts carry no resolutionMode => "
               "`th18:0x4063d0` mode 0 multiplies by 1) - anchor (%.0f,%.0f) bar (%.0f,%.0f) "
               "text (%.0f,%.0f) scale x%.1f",
               anchor[0], anchor[1], h.bar_x, h.bar_y, h.text_x, h.text_y, h.px);
    }
    /* ★★ 百分比数字：**TH08 的字形**（`ascii.anm` sprite 0x88..0x94，生成器搬进 pl0X.anm）
     * 逐格换 sprite / 位置 / 缩放，布局 = `AsciiManager::DrawPercentage` `th08:0x405e10`
     * （纯逻辑在 `yk_gauge_text_glyphs`）。一手：数字同样吃那 5 档染色（`AsciiManager.cpp:1030-1058`）。*/
    yk_gauge_text_sync(s, &h, col);
}

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
    s->shot_timer = 0;
    s->spawns = s->broken = s->lost = s->transferred = s->dropped = s->fam_full = 0;
    s->logged_opt = s->logged_sht = s->logged_noshot = s->logged_full = s->logged_gate = 0;
    s->logged_pass0 = 0;
    s->logged_hudgeo = 0;               /* ★ P2：换关重开 ⇒ 几何自述再打一次（锚点可能变了）*/
    s->heals_seen = 0;
    s->rearms_seen = 0;                 /* ★ route A：整表 reset 也清条目，但计数只增不清 ⇒ 快照回 0 */
    s->releases_seen = 0;               /* ★ P8-3：同上（route A′ 计数只增不清）*/
    s->claimed_set = -1;                /* ★ P7：认领日志的「变化才打」闩 */
    s->flash_pending = 0;
    s->spiral_quota = 0;                /* ★ #25：换关不开螺旋（下降沿才开）*/
    s->spiral_t = 0;
    s->spell_was = 1;                   /* 开场当作「已经在符卡外」⇒ 不会误触发一次 */
    s->spiral_logged = 0;
    s->silent = 0;                      /* ★ P8-6：静默消失的账，每关重开归零 */
    s->opt_spring_live = 0;
    /* ★ 2026-09-22 晚（owner 第 2 条）：追踪态 + 目标 + 出场计时（子机在 ctor 里已经全收）
     * ★ 2026-09-22 晚（owner 第 1 条）：原生子机隐藏声明的「只记一次」闩 */
    s->opt_age = 0;
    s->opt_chase = 0;
    s->chase_have = 0;
    s->chase_id = 0;
    s->chase_x = 0.0f;
    s->chase_y = 0.0f;
    s->chase_enters = 0;
    s->logged_chase = 0;
    s->logged_natopt = 0;
    __builtin_memset(s->opt_pos, 0, sizeof s->opt_pos);
    __builtin_memset(s->opt_vel, 0, sizeof s->opt_vel);
    /* ★★ 本体外观（路 A）：本函数在**属性点变化 / 局中重建**时也会跑 ⇒ 本体脚本的跟踪也要复位。
     * `body_focused = 0` + `body_script = -1` 的组合语义 =「现在跑的不是我们装的脚本、上一帧不在低速」
     * ⇒ 下一次进低速那一帧**必定**走一次「换形」分支装载妖侧 idle（与 TH08 进妖侧时显式设脚本 5 同效）。
     * 高速且从未进过低速时，`yk_body_sync` 什么都不做（人侧交回引擎）。*/
    s->body_focused = 0;
    s->body_script = -1;
    s->body_state = YK_BODY_IDLE;
    s->body_prev_dx = 0;
    /* ★★ 2026-09-22（人妖槽 + 永点）：
     *   · 刻符池 / HUD VM **先收干净**（本函数在局中重建时也会跑）；
     *   · PIV / 刻符计数 / 人妖槽清零 —— 一手：人妖槽在新局初始化块里被清（`th08:0x43ad9f`）、
     *     但**换关不清**（`engine/player/th08/03-...md` §4.2：关卡开始块只写显示副本），
     *     所以这里**只在「新局」语义下调**：`on_run_reset` → `on_ctor` 是本卡唯一的局末收尾；
     *     每关的 `on_load` 也走这里 ⇒ 人妖槽会在换关时归零（⚠ **与一手不同**，登记 `TUNING.md`）。*/
    yk_orb_kill_all();
    yk_gauge_hud_kill(s);
    yk_piv_reset(&s->piv, &s->orbs);
    s->gauge = 0;
    s->gauge_focus_run = 0;
    s->gauge_hold = 0;
    s->gauge_focus_prev = (uint8_t)(CE_PLAYER_FOCUSED() ? 1 : 0);
    s->graze_seen = 0;
    s->orb_taken = 0;
    s->orb_full = 0;
    /* 人妖槽 HUD：认领一块左下角货架（每关开场重排；认领幂等）。抢不到就不画（0 = 没抢到）。*/
    {
        ce_hud_block_t blk;
        __builtin_memset(&blk, 0, sizeof blk);
        blk.w = YK_HUD_BLOCK_W;
        blk.h = YK_HUD_BLOCK_H;
        blk.side = 0;
        s->hud_ready = (uint8_t)(ce_hud_slot_claim_card(ce_card_id(c), &blk, &s->hud) ? 1 : 0);
    }
    if (s->hud_ready) yk_gauge_hud_spawn(c, s);
    return 0;
}

/* ============ ★★★ 2026-09-22（owner #26 · **风味选项**）决死窗口里全屏**敌弹**变红 =============
 * owner 原话：「有 th08 的时候，决死状态会导致画面上所有弹幕变红」；这是**风味**（要一个开关）。
 * 实现照 115（`lolk.c`）的现成做法**逐条**抄（owner 指定）；owner 同日裁：**不抽共享件**
 *   ⇒ 留在 108 卡内，115 一个字不动（`AUDIT.md` §BE 无需跨卡登记）。
 *
 * 抄过来的三件事（`lolk.c:429`–`516`）：
 *   · 染色字段 = 弹的 anm VM 颜色 `b + YK_BULLET_VM + CE_ANM_VM_COLOR1`；
 *   · `mark` 进池前**先记原色**，**每帧开头**无条件把上一帧染过的还原 —— 只换 RGB、**绝不碰 alpha**
 *     （115 的第三十三轮实跑教训：写回含 alpha 的快照会把弹钉死在低 alpha 上 ⇒ 越擦越透明）；
 *   · 池 `YK_TINT_MAX 256`，满了就不再染（只影响观感）。
 *
 * owner 的三条硬要求，逐条对应到下面的代码：
 *   ① **只染敌弹** —— 扫的是**敌弹管理器**的池（`CE_BULLET_MGR()`；115 拿同一个池做擦弹圈，
 *      语义就是「敌弹」）。自机弹在 `zPlayer` 里另一个数组（`player+0xbe838`）、道具在 `ItemManager`
 *      ⇒ 结构上碰不到；
 *   ② **退出 / 死亡 / 换关必须还原** —— 每帧开头无条件还原（退出窗口、中弹死亡都在下一帧还原；引擎
 *      每帧按脚本重设弹的颜色，所以「还原」= 交还给引擎）+ `on_delete` / `on_run_reset` 各兜一次
 *      （那两条路上卡的 tick 会停）；
 *   ③ **开关关掉一帧都不许染** —— 整段（含两张静态表）包在 `#if YK_DEATHBOMB_RED_ENABLE` 里：
 *      关掉 = 连代码都不存在，零指令零副作用。
 *
 * ⚠ 表放在**卡外**（文件级静态数组）而不是 `yk_card_t` 里：`ce_state` 的上限是 496 字节，
 *   超了它返回 NULL、每个回调都在 `if (!s) return 0;` 退掉 —— 卡看着绑上了却什么都不做
 *   （115 2026-09-09 正是这么踩的，`lolk.c:420` 留了编译期闸）。*/
#define YK_DEATHBOMB_RED_ENABLE 1       /* ★ 风味总开关（owner 说要一个开关；默认选 1 的理由见 TUNING.md）*/
#define YK_TINT_MAX 256                 /* 照 115 `LOLK_TINT_MAX`：满了就不再染（只影响观感）*/

#if YK_DEATHBOMB_RED_ENABLE
static uint8_t *s_tint_bullet[YK_TINT_MAX];
static uint32_t s_tint_orig[YK_TINT_MAX];
static unsigned s_tint_n;               /* 本帧染了几颗（= 下一帧要还原几颗）*/
static uint8_t  s_tint_logged;          /* 「本次决死窗口已记过一行」*/

/* 还原上一帧染过的（无条件、只换 RGB）。返回还原了几颗（诊断）。*/
static unsigned yk_tint_restore_all(void)
{
    unsigned i;
    for (i = 0; i < s_tint_n; ++i) {
        volatile uint32_t *c = (volatile uint32_t *)(s_tint_bullet[i] + YK_BULLET_VM + CE_ANM_VM_COLOR1);
        *c = yk_tint_restore(*c, s_tint_orig[i]);
    }
    i = s_tint_n;
    s_tint_n = 0;
    return i;
}

static void yk_tint_mark(uint8_t *b, uint32_t *col)
{
    if (s_tint_n >= YK_TINT_MAX) return;         /* 表满就不再染（只影响观感）*/
    s_tint_bullet[s_tint_n] = b;
    s_tint_orig[s_tint_n] = *col;
    s_tint_n++;
    *(volatile uint32_t *)col = yk_tint_apply(*col);   /* ★ 只换 RGB */
}

/* 帧级：还原上一帧 + （若在决死窗口）把本帧池里所有活着的敌弹染红。
 * ★ 闲时零成本：不在窗口且上一帧没染过任何东西 ⇒ 连池都不扫（`#if` 关掉时整个函数不存在）。*/
static unsigned yk_deathbomb_tint(void)
{
    uint8_t *mgr;
    uint8_t *pl = CE_PLAYER();
    unsigned i, n = 0;
    int want;

    if (!pl) {
        if (s_tint_n) yk_tint_restore_all();     /* 自机没了（重生间隙）⇒ 也要把颜色还回去 */
        s_tint_logged = 0;
        return 0;
    }
    want = yk_deathbomb_window(*(const int32_t *)(pl + CE_PLAYER_STATE));
    if (!want && s_tint_n == 0) { s_tint_logged = 0; return 0; }
    (void)yk_tint_restore_all();                 /* ★ 每帧开头无条件还原上一帧 */
    if (!want) { s_tint_logged = 0; return 0; }
    mgr = CE_BULLET_MGR();
    if (!mgr) return 0;
    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint8_t *b = mgr + CE_BM_BULLETS + (size_t)i * CE_BULLET_STRIDE;
        if (*(const uint16_t *)(b + CE_BULLET_STATE) == 0) continue;      /* 空槽（一手语义）*/
        yk_tint_mark(b, (uint32_t *)(b + YK_BULLET_VM + CE_ANM_VM_COLOR1));
        ++n;
    }
    if (n && !s_tint_logged) {
        s_tint_logged = 1;
        ce_log("youkai: deathbomb window - tinted %u live enemy bullet(s) red "
               "(TH08 flavour option, YK_DEATHBOMB_RED_ENABLE=1; restored on the frame the window ends)",
               n);
    }
    return n;
}
#else
static unsigned yk_deathbomb_tint(void) { return 0; }
#endif

static int on_load(ce_card_t *c) { return on_ctor(c); }

static void on_run_reset(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    if (s && (s->spawns || s->transferred))
        ce_log("youkai: run done - %u familiars spawned, %u killed, %u withdrew, %u silent, "
               "%u transfer damage, %u dropped (no boss)",
               s->spawns, s->broken, s->lost, s->silent, s->transferred, s->dropped);
    (void)yk_deathbomb_tint();                           /* ★ #26：换关也要把染色还回去（先还原）*/
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
#if YK_DEATHBOMB_RED_ENABLE
    /* ★ #26：删卡那一刻 tick 全停 ⇒ 这里兜一次「把颜色还回去」（否则商店里那些弹会一直红）。*/
    if (s_tint_n) {
        unsigned n = yk_tint_restore_all();
        ce_log("youkai: card deleted during the deathbomb window - restored %u tinted bullet(s)", n);
    }
#endif
    yk_orb_kill_all();                                   /* ★ 刻符 VM 池：删卡时一并收（商店里 tick 停着）*/
    yk_gauge_hud_kill(s);                                /* ★ 人妖槽 HUD 四件 + 数字 */
    if (s) yk_option_release(c, s);                      /* 私有状态随后由 SDK 释放 */
}

/* ---- 本体动画：追加进 pl0X.anm 的 5 支脚本号（生成物 `pl_anm_ids.h`）----------------------
 * 下标 = 引擎自己的状态号（`yk_body_state()` 的 0..4 = idle / 起步左 / 从左停 / 起步右 / 从右停），
 * 与 `assets/build_player_anm.py` 的 `_YK_STATES`、宏后缀、以及 `PINNED_NEW_SIDS` **三处同名同序**
 * （生成器那条 `_assert_pinned_numbers()` 会在构建期先红，不会等到实跑发现「低速放的是另一支动画」）。 */
static const int32_t YK_BODY_SCRIPT[4][YK_BODY_STATES] = {
    /* 0 霊夢   → 八雲紫   （TH08 player00.anm script5..9）*/
    { CE_PL_ANM_SCRIPT_YKBODY_PL00_IDLE,   CE_PL_ANM_SCRIPT_YKBODY_PL00_MOVE_L,
      CE_PL_ANM_SCRIPT_YKBODY_PL00_STOP_L, CE_PL_ANM_SCRIPT_YKBODY_PL00_MOVE_R,
      CE_PL_ANM_SCRIPT_YKBODY_PL00_STOP_R },
    /* 1 魔理沙 → アリス   （TH08 player01.anm script5..9）*/
    { CE_PL_ANM_SCRIPT_YKBODY_PL01_IDLE,   CE_PL_ANM_SCRIPT_YKBODY_PL01_MOVE_L,
      CE_PL_ANM_SCRIPT_YKBODY_PL01_STOP_L, CE_PL_ANM_SCRIPT_YKBODY_PL01_MOVE_R,
      CE_PL_ANM_SCRIPT_YKBODY_PL01_STOP_R },
    /* 2 咲夜   → レミリア （TH08 player02.anm script5..9；9 帧、46×46）*/
    { CE_PL_ANM_SCRIPT_YKBODY_PL02_IDLE,   CE_PL_ANM_SCRIPT_YKBODY_PL02_MOVE_L,
      CE_PL_ANM_SCRIPT_YKBODY_PL02_STOP_L, CE_PL_ANM_SCRIPT_YKBODY_PL02_MOVE_R,
      CE_PL_ANM_SCRIPT_YKBODY_PL02_STOP_R },
    /* 3 早苗   → 射命丸文 （**TH16** pl03.anm script0..4；24 帧）*/
    { CE_PL_ANM_SCRIPT_YKBODY_PL03_IDLE,   CE_PL_ANM_SCRIPT_YKBODY_PL03_MOVE_L,
      CE_PL_ANM_SCRIPT_YKBODY_PL03_STOP_L, CE_PL_ANM_SCRIPT_YKBODY_PL03_MOVE_R,
      CE_PL_ANM_SCRIPT_YKBODY_PL03_STOP_R },
};

/* 妖侧本体帧的 sprite 区间（生成物）—— 自愈闸用：我们装的那支脚本只可能写这个区间里的号，
 * 一旦在低速下读到区间**外**的号，就说明引擎把本体脚本换回去了（死亡重生走 `Player__initialize`
 * 会装回人侧 script0）⇒ 立刻重装，不等下一次移动状态变化。引擎自己那套用的是 0..23 ⇒ 不重叠。*/
static const int32_t YK_BODY_SPRITE_FIRST[4] = {
    CE_PL_ANM_YKBODY_FIRST_PL00, CE_PL_ANM_YKBODY_FIRST_PL01,
    CE_PL_ANM_YKBODY_FIRST_PL02, CE_PL_ANM_YKBODY_FIRST_PL03,
};
static const int32_t YK_BODY_SPRITE_LAST[4] = {
    CE_PL_ANM_YKBODY_LAST_PL00, CE_PL_ANM_YKBODY_LAST_PL01,
    CE_PL_ANM_YKBODY_LAST_PL02, CE_PL_ANM_YKBODY_LAST_PL03,
};

/* 两条调用约定的本地签名（照 sdk.h 里 `ce_fn_anm_set_sprite_t` 的写法；**不动共享件 sdk.h**）。
 * 一手：`0x407420` 两个出口都 `ret 8`（`0x407580`）、`0x478580` 是裸 `ret`（`0x47b89d`）。*/
typedef void (__attribute__((thiscall)) *ce_fn_anm_load_script_t)(void *anm, void *vm, int script);
typedef void (__attribute__((thiscall)) *ce_fn_anm_vm_run_t)(void *vm);

/* +0x48（帧末）：本体外观。
 *
 * ★★ 2026-09-22（P4 · **路 A**，owner 裁决）：「抢写本体 sprite」这条路**已废弃**，改成
 *   「**整支换脚本**」——低速 = 妖侧：把 TH08 那五支妖侧本体脚本（已追加进 `pl0X.anm`，
 *   见 `assets/build_player_anm.py` 的 `YOUKAI_BODY` / `_youkai_plan`）按引擎自己的状态号装上；
 *   高速 = 原机体：交回引擎那条状态机（我们只在**出低速那一帧**把它显式设回人侧 idle）。
 *
 * 为什么必须是「换脚本」而不是「换 sprite 号」（两条一手，都逐条看过）：
 *   ① TH08 的妖侧本体就是**同一台 VM 换脚本号**（人 0/1..4 ↔ 妖 5/6..9）——帧时序与朝向
 *      （script8/9 的 `flipX()`）全在脚本正文里 ⇒ 只改 `vm+0x24` 会把两者一起丢掉。
 *   ② 贴图按 **VM 自己那一页的 anm** 取：`0x4810d0`（AnmVm 的绘制）`481117: mov eax,[esi+0x20]`
 *      （页号）→ `481129: mov edx,[0x51f65c+eax*4+0x312072c]`（AnmLoaded*）→
 *      `481130: mov eax,[edx+0x11c]`（它的 sprite 表）→ `481136: mov ecx,[eax+ecx*4+0x8]`
 *      （texture id）→ SetTexture（`481171 call [esi+0x104]`）
 *      ⇒ 传的 `anm` 必须是 **VM 的页所指向的那一份**：自机本体 = `player+0xc`（pl0X.anm）。
 *      ⚠ **不是** `player+0x10`：那一格开火期间被引擎换成 ability.anm（engine.h 的注）
 *        ⇒ 传错 = UV 从一个 anm 取、贴图从另一个 anm 取，画出来是错的。
 *   先例：113 `trance.c` 的 `trance_form_set_sprite`（它只换**一张图**：形态 24 帧与本体帧号 1:1
 *   ⇒ `0x477b00` 就够）；本卡要换**整支脚本**，所以走 `0x407420`。
 *   旧路（写 `vm+0x24`）与它的全部一手订正留在 `AUDIT.md` §BE 详录 #50 里，不再执行。*/
static void yk_body_load_script(void *anm, uint8_t *vm, int32_t script)
{
    /* 三步曲**逐条照抄引擎自己的两处用法**（`Player__initialize` `th18:0x45a8dd`–`0x45a8fe`
     * 与移动动画状态机 `th18:0x45b621`–`0x45b645`，两处逐字节同形）：
     *   ① `AnmVm__load_script` `0x407420`（thiscall(anm; vm, script) ret 8）—— 把脚本模板
     *      `[anm+0x10c] + script*0x60c` 的 0x151 dword（0x544 B）整块拷进 VM；层 `vm+0x18`
     *      与 `vm+0x548` 由它自己存下来再写回；
     *   ② `vm+0x5c0 = 0`、`vm+0x5bc = 0`；③ `AnmVm__run` `0x478580`（thiscall(vm)，裸 ret）。
     * ⚠ 脚本头部必须自带 `type/layer/resampleMode` 前导（`0x407420` 会整块替换模板）：
     *   TH18 原生本体脚本自带那三行，TH08 的源脚本没有 ⇒ 由生成器补（`_inject_body_prologue`）。*/
    ((ce_fn_anm_load_script_t)CE_FN_ANM_LOAD_SCRIPT)(anm, vm, script);
    *(int32_t *)(vm + CE_ANM_VM_POST_LOAD_A) = 0;
    *(int32_t *)(vm + CE_ANM_VM_POST_LOAD_B) = 0;
    ((ce_fn_anm_vm_run_t)CE_FN_ANM_VM_RUN)(vm);
}

/* 每帧的本体外观同步。判据 / 状态号 / 换形那一帧的规则全部一手（见 youkai_core.h §3.2）：
 *   · 状态 = `yk_body_state(本帧 dx, 上一帧 dx, 上一帧状态)`，dx = `player+0x47934`
 *     （引擎在同一个函数里先把它读成「上一帧」再覆盖成本帧值 ⇒ 我们自己留一份 prev 就等价）；
 *   · **换形那一帧**（进/出低速）= TH08 的显式设脚本 + `velocityX = 0`：
 *     进 = 妖侧 idle（`th08:0x44b12b push 0x5`）、出 = 人侧 idle（`th08:0x44b385 push 0x0`），
 *     并把我们的 `prev_dx` 清 0 ⇒ 下一帧必定按方向选到移动动画（与 TH08 清 velocityX 同效）；
 *   · 高速期间**不碰本体 VM**（人侧 0..4 由引擎那条状态机自己管，我们不抢）。*/
static void yk_body_sync(yk_card_t *s)
{
    uint8_t *p = CE_PLAYER();
    uint8_t *vm;
    void *anm;
    int ch, focused, dx, st, want;
    if (!p || !s) return;
    anm = *(void **)(p + CE_PLAYER_ANM);
    if (!anm) return;                                     /* anm 未装载 ⇒ 什么都别碰 */
    vm = p + CE_PLAYER_VM;
    ch = yk_current_character();
    if (ch < 0 || ch > 3) ch = 0;
    focused = CE_PLAYER_FOCUSED() ? 1 : 0;
    if (focused != (int)s->body_focused) {                /* 换形那一帧 */
        s->body_prev_dx = 0;
        s->body_state = YK_BODY_IDLE;
        s->body_focused = (uint8_t)focused;
        if (focused) {
            yk_body_load_script(anm, vm, YK_BODY_SCRIPT[ch][YK_BODY_IDLE]);
            s->body_script = YK_BODY_SCRIPT[ch][YK_BODY_IDLE];
        } else {
            yk_body_load_script(anm, vm, YK_BODY_IDLE);   /* th08:0x44b385 push 0x0（人侧 idle）*/
            s->body_script = -1;                          /* 「现在跑的不是我们装的脚本」*/
        }
        return;
    }
    dx = *(int32_t *)(p + CE_PLAYER_SPEED_X);
    st = yk_body_state(dx, s->body_prev_dx, s->body_state);
    s->body_prev_dx = dx;
    s->body_state = (uint8_t)st;
    if (!focused) return;                                 /* 高速：引擎自己管，我们不抢 */
    want = YK_BODY_SCRIPT[ch][st];
    if (want == s->body_script) {
        /* 状态没变 ⇒ 一般不重装（重装 = 动画从第 0 帧起）。**但**先看一眼 VM 现在写着哪个 sprite：
         * 我们装的脚本只可能写 [FIRST, LAST] 里的号，落在外面就是引擎把脚本换回去了
         * （死亡重生 `Player__initialize` 会装回人侧 script0）⇒ 这一帧必须补救。*/
        int32_t sp = *(int32_t *)(vm + CE_ANM_VM_SPRITE_ID);
        if (sp >= YK_BODY_SPRITE_FIRST[ch] && sp <= YK_BODY_SPRITE_LAST[ch]) return;
    }
    yk_body_load_script(anm, vm, want);
    s->body_script = want;
}

/* ★★★ 2026-09-22（P6 / th08 #20「妖自机在开始移动的时候会闪一帧」）：`yk_body_sync` 的**相位**改了 ——
 * 原来它只从 `on_draw`（= `AbilityManager__on_draw`，**绘制优先级 0x33**）跑
 * —— 一手（`th18:0x408484`–`0x40849d`：注册时 `push 0x33`），而自机是
 * `Player__on_tick` 更新 0x17、`Player__on_draw` 绘制 **0x1d**；引擎自己的移动换脚本状态机在
 * **tick** 里（`Player__tick` → `0x45be90` → `0x45b170`，`0x45b621`–`0x45b649` 那三步：
 * `AnmVm__load_script` + `vm+0x5c0 = 0` + `vm+0x5bc = 0`），而且**只在输入边沿**（`0x45b618`
 * 读「上一帧 dx」）动手 ⇒
 *   **边沿那一帧**：tick 里装了**人侧**移动脚本、同帧 0x1d 就把它画出来（= owner 看到的「闪一帧」），
 *   我们的修正在 0x33 才落地 ⇒ 只能下一帧才纠正。
 * ⇒ 修法 = 把 `yk_body_sync` 挪到「**晚于自机 tick(0x17)、早于自机 draw(0x1d)**」的 tick 相位。
 *   TH08 也是这个相位（`th08:0x44aec0` = `Player::UpdateMovement` 里的 `push 0x5` / `push 0x0`，
 *   在自机的每帧更新里、VM 被画之前）⇒ 这是**复刻一手**，不是自定。
 *   **不新增站点**：走 SDK 既有的事件 `on_item_frame`（`sites.py` 的 `0x446ee3` = ItemManager tick，
 *   更新优先级 **0x1e**：晚于 0x17、早于 0x1d，且与自机同在**更新相位**）—— 先例 = 114 的 `on_item_frame`。
 * ⚠ 别把它挪回任何 `on_draw`（那正是「闪一帧」的成因）。*/
static void on_item_frame(ce_card_t *c)
{
    yk_body_sync(ce_state(c, yk_card_t));
}

static int on_draw(ce_card_t *c)
{
    /* ★ 2026-09-22（P6）：本体换脚本**不在这里**了（绘制相位太晚 ⇒ 会闪一帧），见 `on_item_frame`。
     * 这个钩子留着（本卡以后若要在绘制相位做纯视觉的东西再放这里）。*/
    (void)c;
    return 0;
}

/* +0x18：开局 / 火力档变 / 关店后广播 —— 池刚被重建过（`Player__repopulate_options` `0x45d5e0`）
 * ⇒ 必须在这里**再申请一次**（原生装备卡同款；spec §3.3 的「火力档变化时重申请」）。*/
static int on_power_level_change(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    int i;
    if (!s) return 0;
    /* 池刚被 `Player__repopulate_options_and_notify_cards` `0x45d5e0` 整池重建：
     * 手里的槽指针全作废、重试倒计时也要清掉（否则要等 60 帧才补申请）。
     * 照原生 / 77：不 drop（引擎已经重建过），直接清指针再去要一个。*/
    for (i = 0; i < YK_OPTION_MAX; ++i) { s->opt[i].ptr = 0; s->opt[i].retry = 0; }
    s->opt_script = -1;
    /* ★ th08 #1：整池重建 ⇒ 我们手里那几格已经作废（引擎重建的子机在新位置）⇒
     * 弹簧也从头起（否则会从**旧偏移**滑进来，看起来像"从别处飞回来"）。*/
    s->opt_spring_live = 0;
    yk_option_refresh(c, s, CE_PLAYER_FOCUSED());          /* 声明；落笔在帧界（SDK 调被选举者）*/
    return 0;
}

/* +0x1c：只在射击帧广播（`0x45ecb8` 卡广播）。低速出妖侧那一套 shooterset。
 * ⚠ 两条前置（都是一手）：
 *   ① 膛口 = 子机：`0x40a9c0` **不读记录里的 `opt_slot`**，膛口恒取 `option+0x5c/0x60`
 *      （`0x40aa0c`／`0x40aa21`，整组只算一次）；`option == NULL` 时它 `test edx,edx / je`
 *      直接返回 ⇒ **妖侧弹的膛口就是妖侧子机**（本卡手里那一格）。
 *      ⇒ 声明的数量为 0 时（高速 / 早苗档 0）这里自然不发弹。
 *   ② 那一格必须是**我们写的合法记录**：原生 0x16 是 `fire_rate = 0`，装备卡路径会整数除零
 *      （`engine/sht/th18/02` §2）⇒ 自检记录（`yk_shotset_usable`），没覆盖成功就不调。
 *      覆盖由 `assets/sht/append_shooterset.py` 的 `REPLACE` 做（`make sht`）。*/
/* ★★★ 2026-09-22（P7 / owner #13「妖形态主机也发弹的」）：**本卡主动认领主炮 shooterset**。
 *
 * 一手（TH08）：妖侧那一套 set **含本体行**（`ply00as.sht` 满档 5 行里 3 行 `+0x20` = 0 = 本体、
 * 2 行 = 1 = 子机；`th08:0x450f6f` 按 `player+0x3` = isYoukaiMode 选 `plyNNas.sht`）⇒
 * 「妖形态主机也发弹」是原作行为；我们原来把整条行循环压掉（`on_native_shot_suppress`），
 * 妖形态本体一发都没有 —— 而且**连带把 113/117 的 set 覆盖也压死**（它们同样靠这条行循环发）。
 *
 * 现在：低速（= 妖侧）时返回我们那一组（按机体 × 火力档选，早苗回落 `0x16`），
 * 让**引擎**发（它按 `+0x20` 分膛口）。优先级用默认 0 —— `sdk/public.h` 的阶梯表里
 * 「108（默认 0）」本来就是这么写的；113（10）/117（20）仍然赢我们，111（−1）输给我们
 * （111 那一组现在压根不需要发：108 聚焦时它本来就被压着，见 AUDIT §BE 详录 #57 §3c 的影响面）。
 * ⚠ 高速一律返回 −1（人侧交回引擎的原生 set）。*/
static int on_shot_set_index(ce_card_t *c, int power_level)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    if (!s) return -1;
    if (!CE_PLAYER_FOCUSED()) return -1;                      /* 人侧：原生 set */
    /* ⚠ `power_level` 这个形参是 SDK 传进来的**主炮档**（`BP_ce_shot_set_index` 的 eax），
     *   与 `yk_current_power_level()` 同源（`CURRENT_POWER / 100`）⇒ 直接用引擎给的那个值，
     *   不做二次读法（少一条可能与引擎不同步的路径）。越界一律钳进表内。*/
    {
        int tier = power_level;
        int set;
        if (tier < 0) tier = 0;
        if (tier >= YK_SHOT_TIERS) tier = YK_SHOT_TIERS - 1;
        set = yk_youkai_shotset_pick(yk_current_character(), tier);
        /* ★ 认领的**一行证据**（实跑判据 + 归属证据）：只在「(机体, 档, set) 变了」时打一次。
         * 它同时把 P7 影响面里那两条写进日志：① 我们赢了谁（111 的 −1 恒输给我们）；
         * ② 113(10)/117(20) 若同时在认领，SDK 会另打一条 trace（槽 0x98）。*/
        if (set != (int)s->claimed_set) {
            s->claimed_set = set;
            ce_log("youkai: claiming main-cannon shot set 0x%02x (char %d tier %d) - the ENGINE "
                   "fires our rows now (slot 0 from the player, slot>=1 from our options); "
                   "113/117 still win by priority, 111 loses (its set never fired while we "
                   "suppressed the whole row loop)", set, yk_current_character(), tier);
        }
        return set;
    }
}

static int on_tick_shooters(ce_card_t *c, uint32_t short_timer, uint32_t long_timer)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    uint8_t *o;
    int set;
    (void)short_timer;
    (void)long_timer;
    if (!s || !CE_PLAYER()) return 0;
    /* 高速 = 原机体（原生弹照发）；早苗的档 0 = 0 个子机 ⇒ 没有膛口，也不发妖侧弹。
     * ★ 这一条 = 止射闸第三条门的**同一张表、同一个函数**（`yk_option_wanted`）：两边同帧同源
     *   ⇒ 「不压原生弹」的那一帧，这里也恰好不出妖侧弹（不会出现两边都发或两边都不发）。*/
    if (!yk_option_wanted(CE_PLAYER_FOCUSED(), yk_current_character(), yk_current_power_level())) return 0;
    set = yk_youkai_shotset_pick(yk_current_character(), yk_current_power_level());
    if (!yk_youkai_shotset_installed(set)) {
        if (!s->logged_sht) {
            s->logged_sht = 1;
            ce_log("youkai: no usable youkai shooterset for this card (tried 0x%02x, fallback "
                   "0x%02x) - record fire_rate is 0/terminator; run `make sht` "
                   "(calling it would divide by zero, engine/sht/th18/02 §2)",
                   yk_shot_set_for(yk_current_character(), yk_current_power_level()),
                   YK_SHOT_SET_YOUKAI);
        }
        return 0;
    }
    /* ★★★ 2026-09-22（P7 / owner #13）：**这里不再自己发弹** —— 妖侧的主炮组改成由
     * **引擎自己的行循环**发（`th18:0x45ebfb`–`0x45ec9f`，本卡经 `on_shot_set_index` 认领 set）。
     * 为什么必须换：`Player__tick_shooters_for_ability_card`（`0x40a9c0`）**完全不读行里的 `+0x20`**
     * （膛口恒取传进来的那颗子机，`0x40aa0c`/`0x40aa21` 只算一次）⇒ 妖侧本体主炮**一发都发不出来**；
     * 而引擎那条行循环按 `0x45ec41 mov al,[edi+0x20]` 分「本体（0）/ 第 n 架子机（≥1）」，
     * 正是 TH08 的语义（两作的编号法一致，见 `assets/sht/append_shooterset.py` 的 origin 段）。
     * ⚠ 与 `on_shot_set_index` 是**一对**：那边认领、这边闭嘴；少任何一半都会「双发」或「都不发」。*/
    (void)o; (void)set;
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
                               CE_CURRENT_POWER(), &s_yk_sets, &out))
        *(int32_t *)((uint8_t *)bullet + CE_BULLET_DAMAGE) = out;
    return 0;
}

/* ★ 核心：止射闸（spec §3.1 + 一条**用户口径**）。返回非 0 ⇒ SDK 让本帧跳过原生行循环
 * （`retaddr = 0x45eca0`），`0x45eca0` 起的**卡广播照跑**（77 / 116 的卡子机不受影响）；117 咆哮生效时
 * SDK 根本不问这一条（sdk.c：先问 `on_shot_set_index`，有卡覆盖了就不抑制）。
 *
 * 判据 = `yk_should_suppress_native(有自机, 聚焦, 机体号, 火力档)`（纯逻辑在 core、单测覆盖）：
 *   ① 有自机 ② 聚焦 —— 这两条 = **spec §3.1 原文**；
 *   ③ **本帧妖侧子机数 > 0** —— ❓**口径：`count == 0` 放行原生弹（用户 2026-09-12 定）**。
 *   取数与子机声明层**同源同帧**（`yk_option_declare`：`CE_ADDR_CHARACTER` + `yk_shot_tier(CE_CURRENT_POWER())`
 *   → `yk_option_count` → `YK_OPTION_BY_CHAR`）—— 这里调的是**同一个** `yk_option_wanted`，没有第二份判断。
 *   ⚠ 这一条**不是一手复刻**：原版永夜抄的妖侧不看火力、一按低速就有子机（TH08 那张固定表），
 *     原作里不存在「低火力 ⇒ 没子机 ⇒ 一发都没有」这个档；这条门是本实现的约束（膛口恒取子机）
 *     逼出来的，用户 2026-09-12 拍了「宁可打原生弹，也别一发都没有」。 */
static int on_native_shot_suppress(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    const int focused = CE_PLAYER_FOCUSED() ? 1 : 0;
    const int character = yk_current_character();
    const int level = yk_current_power_level();

    /* ▼ 站点自检门 ④（2026-09-18，**只记账**）：本卡的「使魔形态随 Shift 变」整个建立在
     *   `CE_PLAYER_FOCUSED()`（= `player+0x476cc`，引擎每帧从输入位 3 写）这一个读法上。
     *   这里拿**第二个独立来源**（原始输入块 `0x4ca428` 的 bit 3 = `CE_INPUT_FOCUS`）交叉核对：
     *   两者应当**每帧一致**（一手：engine.h 第 338–339 行的写点 `0x45b414 mov [edi+0x476cc],edx`）。
     *   不一致 = 我们读错了位 / 引擎改了写法 ⇒ 记一行（只记一次），行为不变。*/
    if (CE_PLAYER() && (int)((CE_INPUT_HELD() & CE_INPUT_FOCUS) != 0) != focused)
        ce_assume_fail("youkai: 聚焦位两个来源不一致（player+0x476cc vs 输入位 3）");

    if (!yk_should_suppress_native(CE_PLAYER() != 0, focused, character, level)) {
        /* 走到这里的三条来路，只有第三条要记日志（前两条每关都会出现，记了就是噪音）：
         *   ① 没有自机（标题 / 过场）；② 高速（= 原机体，设计）；
         *   ③ ❓口径生效：低速 + **本帧子机数 0**（今天只有「早苗 + P < 100」这一档）⇒ 放行原生弹。
         *      ⚠ 这一行的用途：实跑时一眼看出**那一刻是原生弹在打**，而不是「什么都没打」。*/
        if (!s || s->logged_pass0 || !focused) return 0;
        s->logged_pass0 = 1;
        ce_log("youkai: focused with 0 youkai options at this power level "
               "(char %d, tier %d) - NOT suppressing native shots (user ruling 2026-09-12): "
               "the native shot line is what fires now; the youkai side has no muzzle because "
               "0x40a9c0 fires from an option slot only",
               character, level);
        return 0;
    }
    /* 压了原生弹。❓口径改掉之后，还剩一种情况会让本帧真的**一发都没有** = 妖侧 shooterset 没装
     * （子机 0 那条已经在上面放行了）⇒ 第一次命中时记下原因（实跑诊断用），但不改行为。*/
    if (!s || s->logged_noshot ||
        yk_youkai_shotset_installed(yk_youkai_shotset_pick(character, level))) return 1;
    s->logged_noshot = 1;
    ce_log("youkai: native shots suppressed at focus but no usable youkai shooterset "
           "(char %d tier %d; tried 0x%02x, fallback 0x%02x; record fire_rate is 0; run `make sht`) "
           "-> NO bullets at all (spec §3.1 + §3.4.1)",
           character, level, yk_shot_set_for(character, level), YK_SHOT_SET_YOUKAI);
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
 *   自己使魔表里的槽表态，别的槽一律放行。
 *
 * ★★ 2026-09-22（工单 E / **route A**）：改调 `yk_shot_slot_gate`（= 上面那条判据 + **副作用**）。
 *   副作用 = 夹住的槽被问到时置 `rearm_pending`：**这个槽的下一次 `etOn` 就是「下一波发弹」**，
 *   卡 tick 会消费它 ⇒ 清闸 + 同帧重建使魔。这一条取代了原来的「等 ECL 重新武装」——
 *   一手（22 份原生 ECL、282/282 处）证明那条边沿在实战里观测不到（`etNew` 与同槽 `etSprite` 永远同帧），
 *   而 boss 段首的 `enmKillAll()` 必然把使魔清光 ⇒ 原来会落到 120 帧 TTL 才放开
 *   （= owner 报的「击破 boss 第一阶段后再不发弹」）。全文见
 *   `engine/enemy/th18/07-emitter-slots-waves-and-phase-transitions.md` 与 `youkai_core.h` §3.5.2。
 * ⚠ 代价：每次使魔死亡**最多丢 1 条 `etOn`**（那一帧照旧掐，下一帧使魔已回来）。 */
static int yk_on_shot_slot(ce_card_t *c, const ce_enemy_shot_t *ev)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    uint32_t have = 0;
    uint32_t rearms_before;

    if (!s || !ev || !ev->enemy_id) return 0;
    if (ev->slot < 0 || ev->slot >= YK_EMIT_SLOTS) return 0;
    /* ★★★ 2026-09-22（P8-1）：**未聚焦 ⇒ 本卡一律不表态**。
     * 本卡的 1:1 映射整体只在低速（= 妖侧）活着（`on_tick_2` 的 `!focused` 分支把使魔全撤 + 清表）；
     * 高速时场上的使魔与我们无关 ⇒ 闸在这里没有任何语义，却**可能认下 boss 自己的发射口**
     * （槽表对每一只敌人、每个 `props[i].type != 0` 的槽都建条目，没有任何 boss / 可锁定过滤）。
     * 这条闸就是 owner 早先「击破一个阶段后 boss 不再发弹」的机制（闸的归属判据确实会认 boss）——
     * 加上这条门之后，「未聚焦的那一半时间」不再有任何夹持的可能。*/
    if (!CE_PLAYER_FOCUSED()) return 0;
    /* 按 id 回查**当前活着的**使魔（0 = 没了）。★ 不碰 `ev->enemy`：那是敌人本体、不是使魔，
     * 而且只在本次回调内有效（同帧 sweep 可能删它）。 */
    have = yk_slot_fam_id(&s_map, ev->enemy_id, ev->slot);
    rearms_before = yk_map_rearms(&s_map);
    if (!yk_shot_slot_gate(&s_map, ev->enemy_id, ev->slot, have)) return 0;
    /* ★★★ 2026-09-22（P8-5）：**每一次「夹持期开始」记一行完整的上下文**（含 boss 与活跃槽数）。
     * 记账判据 = `rearms` 涨（= 闸刚置上 `rearm_pending`，一个夹持期只记一次）。
     * ⚠ 不逐帧逐槽打（16 槽 × 60 fps = 每秒上千行，会把日志淹掉）——「每次」按**夹持期**计。*/
    if (s && yk_map_rearms(&s_map) != rearms_before) {
        yk_slot_t *sl = yk_map_find(&s_map, ev->enemy_id, ev->slot);
        ce_log("youkai: emitter gate CLAMPED slot %d of enemy_id %u (focused=%d fam_seen=%d "
               "broken=%d no_fam_frames=%d) - this etOn is dropped; the NEXT etOn of this slot is "
               "released by route A' (core: `yk_shot_slot_gate`)",
               ev->slot, ev->enemy_id, CE_PLAYER_FOCUSED() ? 1 : 0,
               sl ? (int)sl->fam_seen : -1, sl ? (int)sl->broken : -1,
               sl ? (int)sl->no_fam_frames : -1);
    }
    if (s && !s->logged_gate) {
        s->logged_gate = 1;
        ce_log("youkai: emitter gate ACTIVE - slot %d of enemy_id %u stays silent until the next "
               "etOn of that slot (route A) or the %d-frame self-heal expires",
               ev->slot, ev->enemy_id, (int)YK_EMIT_BROKEN_TTL);
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
    if (!yk_fam_broken(&s_map, ev->id)) {
        /* ★★ 2026-09-22：**不是我们的使魔** ⇒ 这是一次普通击破 —— 人妖槽与刻符的涨源就在这里。
         * ⚠ `KILL_ALL`（段首清场 `enmKillAll`）**不算击破**（本函数开头那段约定：给奖励的东西必须
         *   排除它，否则清场会把计数刷满）。*/
        if (ev->kind != CE_EDEATH_KILL_ALL) yk_on_enemy_killed(s, ev);
        return;
    }
    /* ★ 壳体已经被引擎析构了 ⇒ 跟随 VM 也必须同时收，否则会留下一个**画着已死使魔的幽灵**
     *（打不到、不消失）。这里是**共享死亡断点的死亡栈**上：删 VM 只是给它标删
     *（`0x488cf0` 置 `vm+0x538` 位 7，真正的析构由 AnmManager 自己的 update 走）⇒ 与
     * `ufo.c` 的 `ufo_tok_kill` 同一条纪律、同一个 🟡（「从死亡断点直接进 AnmManager」未实跑验证）。*/
    yk_fam_vm_kill_id(ev->id);
    ++s->broken;
    ce_log("youkai: familiar enemy_id %u killed (kind %u, real=%u) at (%.0f, %.0f) - "
           "that emitter slot stays silent until the NEXT etOn of that slot (route A) or the "
           "%d-frame fallback self-heal expires (gate = on_enemy_shot_slot 0x433ffa)",
           ev->id, ev->kind, (ev->flags & CE_EDEATH_F_REAL) ? 1u : 0u, ev->x, ev->y,
           (int)YK_EMIT_BROKEN_TTL);
}

/* ★★★ 2026-09-22（P3 / th08 #17「人妖槽在 +99 和 -99 点跳」）：**原来的「收道具 ⇒ 人妖槽 ±身价/100」
 * 这一源已被删除** —— 它在原作里**不存在**，是 `ItemManager.cpp:597` 那一行的**误读**。
 *
 * **一手（逐条，2026-09-22 复核）**：
 *   · `ItemManager.cpp:597` 在 `Item::CollectTimeOrb`（本样本 `th08:0x4412b0`）体内，而且
 *     `0x44139e mov DWORD PTR [ebp-0x4],0x6f` 把 `itemScore` **当场改写成字面量 111**
 *     再 `0x4413cb call 0x43c0bb`（= `AddToYoukaiGauge`）⇒ 那一行说的是**收刻符 ±111**，
 *     与「普通道具身价」无关（刻符那一源已在 `yk_orb_collect` 里实现 ✅）。
 *   · 全 exe 里 `call 0x43c0bb` 只有 **8 处**（`0x42b223` / `0x42d682` / `0x4413cb` / `0x44aa81` /
 *     `0x44bedf` / `0x44bffb` / `0x44c832` / `0x44c850`）—— **没有一处**在「普通道具被收下」的路径上。
 *   · 更致命的是**量纲**：TH18 的 `CE_ADDR_MONEY_BASE`（`0x4ccd24`）单位是「点 × 100」且下限
 *     **1,000,000**（`engine.h` 的注）⇒ `base/100` ≥ **10,000** = **一整条槽**。owner 那一跑的
 *     日志里 `score-in: base=1000000 …` 逐颗递增，于是每收一颗道具就把槽**顶到 ±10000（夹紧）**，
 *     数值长期停在 ±99.xx% 那一带、且符号跟着「收下那一刻聚不聚焦」翻 —— 正是 owner 报的
 *     「在 +99 和 -99 点跳」。日志合证：`youkai: gauge=-9855 … taken=0` = −10000 + 5×29（只有漂移与
 *     3 次击破 −200 在动，没有任何刻符），只有这一源能给出那个量级。
 *
 * ⇒ 结论：**删掉这一源**（不是调小，是它不该存在）。本卡的四个涨源与一手一一对应：
 *   ① 击破敌机 ±200（`th08:0x42da2a` 一族 / decomp `EnemyManager.cpp:670`）、
 *   ② 收刻符 ±111（`th08:0x4413cb`，且过「自动收集期间不加」那道门 —— 见 `yk_orb_collect` 的注）、
 *   ③ 聚焦漂移（decomp `Player.cpp:1088-1127`）、④ 死亡清零（`th08:0x44ac09`）。
 *   ⚠ 尚缺的两条（一手已定位，见 `AUDIT.md` §BE 详录 #57）：**射击中每帧 ±26**（`Player.cpp:445-452`，
 *     `AddToYoukaiGauge(0x6590/0x3e7, 1)`）与 `AddToYoukaiGauge` 的**第一道闸**
 *     （`th08:0x43c0c2`：`ds:0x17d6ed4` = `g_Player+0xfdc` = **`shotActive`**，非 0 且 `param_2 == 0`
 *     ⇒ 整条丢掉）——本批**未接**（登记 `TUNING.md`），本批只做「去掉不存在的源」这一半。
 * ⚠ `on_item_commit` 这个钩子**不再订阅**（下方 `CE_CARD` 表里已删）：它在本卡里没有别的用途。*/
#if 0  /* 保留原文以便对照（见上面那段订正）—— 这一源已作废，别再打开 */
static void on_item_commit(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    int32_t base;
    if (!s) return;
    base = *(const int32_t *)(uintptr_t)CE_ADDR_MONEY_BASE;
    if (base <= 0) return;
    s->gauge = yk_gauge_add(s->gauge, CE_PLAYER_FOCUSED() ? (int)(base / 100) : -(int)(base / 100));
}
#endif

/* ★★ 2026-09-22（人妖槽：死亡清零）：一手 `th08:0x44ac09`（`Player::begin_death` 里
 * `set_youkai_gauge(0)`）。走 `on_death_before_deathbomb`（决死窗口**之前**那一帧，
 * 与 TH08 的 `begin_death` 同一时刻）。*/
static int on_death_before_deathbomb(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    if (s) s->gauge = 0;
    return 0;
}

/* +0x2c：AbilityManager tick（优先级 0x16），早于敌人结算（0x1b）⇒ 上限只对当帧有效、不用还原。*/
/* ★ 自动测试用「定期使魔账」（2026-09-17）：`run done` 只在收尾（过关重挂 / 重开 / 删卡）才打
 * ⇒ 代打场景经常判「未触发」。有值时每 ~600 帧打一行同样的计数，
 * `verify.py` 的 familiar 判据已经同时接受 `run done` 与 `ledger` 两种形态。*/
static uint32_t s_ledger_frame;

static int on_tick_2(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    int focused;
    if (!s) return 0;
    if ((++s_ledger_frame % 600u) == 0u && (s->spawns || s->broken || s->lost || s->silent))
        ce_log("youkai: ledger - %u spawned, %u killed, %u withdrew, %u silent, %u dropped",
               s->spawns, s->broken, s->lost, s->silent, s->dropped);
    /* ★ 人妖槽 / 永点的账（每 ~600 帧一行）：实跑要看见的四件 —— 槽值、刻符收了几颗、
     * PIV 当前值、池里还剩几颗活着。VERIFY 的判据就取这一行（见 `SINGLE-CARD-TEST.md` §th08）。*/
    if ((s_ledger_frame % 600u) == 0u && (s->orb_taken || s->gauge || s->piv))
        /* ★★★ 2026-09-22（P5 #16b / P3 第二批）：账里带上**本帧生效的运动参数与射击位** ——
         * 实跑一眼可判「拾取半径是不是 TH18 那档 70」（一手 20/24 只在注释里，见 `youkai_core.c`）
         * 与「射击中每帧 ±26 那一支有没有在跑」（`shoot=1` 时槽应当**稳步**走向当前形态，
         * 而击破/收刻符那两笔在射击中不计）。*/
        {
            float sp = 0.0f, rr = 0.0f;
            int ch = yk_current_character();
            if (ch < 0 || ch > 3) ch = 0;
            yk_orb_motion_params(ch, &sp, &rr);
            ce_log("youkai: gauge=%d piv=%d orbs=%d taken=%u live=%u full=%u hud=%u "
                   "speed=%.0f pick=%.0f shoot=%d",
                   (int)s->gauge, (int)s->piv, (int)s->orbs, s->orb_taken, s_orb_live,
                   s->orb_full, (unsigned)s->hud_ready, sp, rr, yk_shooting());
        }
    if (!CE_PLAYER()) {
        yk_drop_all(s, "no player");
        return 0;
    }
    focused = CE_PLAYER_FOCUSED();
    /* ★★ 2026-09-22（人妖槽 + 永点）：每帧三件事 + 一件声明，全部是**普通内存读写**（不新增站点）。
     *   ① 刻符运动 / 收集（`yk_orbs_tick`，一手运动方程见 core §3.11）；
     *   ② 擦弹涨源：TH18 的累计擦弹在 `CE_ADDR_GRAZE`（0x4ccd0c），本卡自己差分
     *      （SDK 的 `ce_graze_bridge` 读的是同一个全局 ⇒ 不新增站点、不重复计数）；
     *   ③ 人妖槽漂移（一手 TH08 的聚焦状态机）；
     *   ④ PIV 声明给计分层：**族 08**（`CE_COEFF_FAMILY_08`）+ 未耦合加数 = PIV
     *      —— 这正是 §BE 详录 #43 的「并进 `ecx` 声明」口径（`ΔG_08` 随 `coeff_18` 一起被折扣）。*/
    if (s->hud_ready) {
        ce_hud_slot_get(ce_card_id(c), &s->hud);           /* 每帧取最新锚点（同批后来的认领会重排）*/
        yk_gauge_hud_spawn(c, s);                          /* 自愈：VM 被引擎清过就重建 */
        yk_gauge_hud_update(s);
    }
    yk_orbs_tick(s);
    {
        uint32_t graze = *(const uint32_t *)(uintptr_t)CE_ADDR_GRAZE;
        if (graze > s->graze_seen) {
            uint32_t d = graze - s->graze_seen, i;
            float px = *(const float *)(CE_PLAYER() + CE_PLAYER_X);
            float py = *(const float *)(CE_PLAYER() + CE_PLAYER_Y);
            if (d > 8u) d = 8u;                            /* ❓ 自定：单帧上限（防一段大擦弹一次性灌满池）*/
            /* ★★★ 2026-09-22（P5 / th08 #18「妖形态是擦弹得自动吸收的永点……总之是有一个『无敌』的
             * 上抛过程，然后直接吸收」）：**擦弹那一路是 state 5（上抛），不是 state 1** —— 上一版
             * 认错了对象。一手（逐指令，本样本）：
             *   · 擦弹真正的落点 = `Player::sub_44a6a0` 的 `th08:0x44aac9 / :0x44aaef / :0x44ab0f`
             *     ⇒ `spawn_item(pos, **type 0xa**, state 1)`；
             *   · 而 `spawn_item` 把 type 10 **当场改写**成 `type 7 + state 5`
             *     （`th08:0x440124 cmp [ebp+0xc],0xa / mov [ebp+0x10],5 / mov [ebp+0xc],7`）⇒
             *     那个 `state 1` 实参**被丢弃**；
             *   · state 5 的初速与 state 3 逐指令同形（`th08:0x4402cb`–`:0x4403b6`：`vy = −2.0 − rand(0.2)`、
             *     `vx = rand(±0.6)`），每帧 `vy += 0.05`（`th08:0x440740`），`vy > 0` 才转归航
             *     （`th08:0x44079c`），且**上抛期间跳过抓取盒判定**（`0x4407a5`）⇒ 看得见那一段上抛。
             *   · `state 1`（生成即满速）只属于「消弹 / `cancel_autocollect`」那一支
             *     （`th08:0x4308e5` 一族 + `ItemManager::cancel_autocollect` `th08:0x4413e0`）。
             * ⇒ 我们那句 `YK_ORB_STATE_HOMING` 改成 `YK_ORB_STATE_SPAWN`（= 我们的 state 3，
             *   运动方程与 TH08 的 state 5 同一套：上抛 → `vy > 0` 转归航，且上抛期间不收）。
             * ⚠ 仍**未复刻**的两点（登记 TUNING）：① 落点 —— TH08 撒在**被擦到的那颗弹**上，
             *   我们拿不到那颗弹（`ce_graze_take` 的断点 `0x42497f` 在 `sites.py` 里被注释掉 ⇒ 恒返回 0）
             *   ⇒ 落点 = 自机（上抛之后回来才被收，至少看得见动作）；② TH08 那支的门
             *   （極端妖怪 + `is_boss_active` + 符卡/机体分支）我们没接，用的是「聚焦即撒」。*/
            if (focused) for (i = 0; i < d; ++i) yk_orb_spawn(s, px, py, YK_ORB_STATE_SPAWN);
        }
        s->graze_seen = graze;
    }
    /* ★★★ 2026-09-22（P4 / owner：「人转妖的时候，敌方使魔会有一个 wav，这个不存在于后续音效当中」）：
     * **Shift 那一下的两枚 SE** —— 一手 `FUN_0042c420`（`th08:0x42c420`）两支各播一枚（逐指令）：
     *   · **人→妖**（`0x42c43a` 起：`flags` bit11 == 0 且玩家已是妖，同支把环换成 interrupt 2）：
     *     `0x42c489 push 0x28` → id 0x28 → 声音表第二格 wav **33** = **`se_invalid.wav`**；
     *     而这一枚的字节**与 TH18 自己的 `se_invalid.wav` 完全相同**（md5 `464ff702…`）
     *     ⇒ 直接播 TH18 原生那一格（`0x0c`），**不占新槽**。
     *   · **妖→人**（`0x42c4e1` 起：bit11 != 0 且玩家已不是妖，同支把环换成 interrupt 1）：
     *     `0x42c529 push 0x27` → wav **32** = **`se_ophide.wav`**（= option hide「子机收起来」那一声）；
     *     TH18 没有这一枚 ⇒ 走扩表 `CE_VOICE_YOUKAI_FAM_SHIFT`（`0x5d`，构建期从 TH08 现取）。
     * ⚠ 口径出入照实登记：owner 记的是「**人→妖**那一枚不存在于后续」，一手是「**妖→人**那枚
     *   （se_ophide）才是 TH08 独有」（人→妖那枚 = se_invalid，TH18 一直有）⇒ 两向都播，各用一手那一枚。
     * ⚠ 我们的使魔只在低速（妖）存在 ⇒ 妖→人那一下正是「把它们收起」= 与 se_ophide 的语义完全对上；
     *   人→妖那一下（TH18 里 se_invalid 同时是**符卡超时**音）只在**当时场上有我们的使魔**（= 一次形态
     *   切换真的发生）时才播，免得每次按 Shift 都像超时。*/
    if (focused != (int)s->gauge_focus_prev) {
        float px = *(const float *)(CE_PLAYER() + CE_PLAYER_X);
        if (focused) {
            if (yk_map_fam_count(&s_map)) ce_play_sound(0x0cu, px);        /* 人→妖：TH08 id 0x28 = se_invalid */
            s->flash_pending = 1;                 /* ★ #22：使魔重建时在它身上补一支爆闪（310）*/
        } else {
            if (yk_map_fam_count(&s_map)) {
                unsigned fi;
                ce_play_voice(YOUKAI_FAM_SHIFT, px);   /* 妖→人：TH08 id 0x27 = se_ophide */
                /* ★ #22：妖→人那一支的爆闪（311）起在**撤走前最后记下的使魔坐标**上
                 *（一手就是起在使魔身上；使魔这一帧正在被撤）。*/
                for (fi = 0; fi < YK_FAM_MAX; ++fi)
                    if (s_map.e[fi].enemy_id && s_map.e[fi].fam_id) {
                        yk_flash_spawn(CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_FLASH60,
                                       s_map.e[fi].x, s_map.e[fi].y);
                        break;                     /* 一手是每只使魔各一支；我们只起一支（画面上一闪）*/
                    }
            }
        }
    }
    /* ★ #26 风味选项：决死窗口里全屏敌弹变红（判据 = 既有原语 `player+0x476ac == 4`）。*/
    (void)yk_deathbomb_tint();
    yk_gauge_tick(s, focused);
    ce_score_family_declare(ce_card_id(c), CE_COEFF_FAMILY_08);
    ce_score_declare_delta(ce_card_id(c), s->piv);
    /* ★★ 2026-09-22 晚（owner 第 1 条）：**低速隐藏原生（人侧）子机**。
     * 一手：TH08 进低速那一帧把同一个子机对象的脚本换成妖侧那一支（`th08:0x44b12b push 0x5` 是本体、
     *   子机是各机体回调里的 `SetAndExecuteScriptIdx(option, 0x12/0x1d/0x18)`）⇒ **人侧那套子机不该同时存在**。
     *   TH18 的原生 4 格子机是另一套数组（`player+0x670`，`engine.h` 的 `CE_NATIVE_OPT_*`）⇒ 走
     *   **既有的** `ce_native_option_hide_*` 声明层（111 的同款，出处 `sdk.h` 那一整段 + AUDIT §BE #32）：
     *     · **声明**（粘性；机体门在层里）：低速 = 1、高速 = 0 ⇒ 松开 Shift **当帧**就恢复原生行为；
     *     · **每帧再 `sweep` 一遍**：卡刚装上 / power up 之后引擎 `repopulate` 的档位短路缓存会让
     *       「不重建 = 也不清」⇒ 只靠断点会留下幽灵子机；
     *     · **再收一次它已经在打的激光**：`mode=2` 激光的寿命只跟射击键走（`0x4601e0`），
     *       持有者被清掉之后束会**钉在原地**直到松开 Z（2026-09-18 111 的同类实跑症状）。
     *   ⚠ 与 111 的交互：两边都声明时语义 = **按机体各自生效**（`ce_native_option_hidden(char)` 是 OR）
     *     ⇒ 魔理沙同时带 108+111 时仍然隐藏（两边都要隐藏，没有冲突）；`owner` = id 最小者（诊断）。*/
    ce_native_option_hide_declare((uint32_t)ce_card_id(c), focused, yk_current_character());
    if (focused) {
        int n = ce_native_option_sweep();
        (void)ce_kill_retail_option_lasers();
        if (n && s && !s->logged_natopt) {
            s->logged_natopt = 1;
            ce_log("youkai: native (human-side) options hidden in low speed - swept %d stale VM(s); "
                   "they come back the frame Shift is released (declaration is per-character)", n);
        }
    }
    /* ★ 追踪目标（机体 0 那一支用）：每帧解析一次，帧界的 `on_option_pose_apply` 读卡状态。
     * ⚠ 只在低速解析（高速没有我们的子机、使魔也撤了，解析出来也没人用）。*/
    if (focused) {
        yk_chase_target_resolve(s);
        if (s->opt_script >= 0 && s->opt[0].ptr) { if (s->opt_age < 0xffffu) ++s->opt_age; }
        else { s->opt_age = 0; s->opt_chase = 0; }
    } else {
        s->chase_have = 0;
        s->chase_id = 0;
        s->opt_age = 0;
        s->opt_chase = 0;
    }
    yk_option_refresh(c, s, focused);                     /* 每帧声明（低速才有货；落笔由 SDK 在帧界叫）*/
    if (!focused) {
        /* 高速 = 原机体：使魔全部撤（spec §9 实跑清单 1「松开全部还原」）。
         * ★★★ 2026-09-22（P8-2）：**无条件**走这一支（原来写成 `if (yk_map_fam_count(...))`）——
         *   `yk_map_fam_count` 只数 `fam_id != 0` 的条目，而**夹持态**的条目（`fam_seen == 1` /
         *   `broken == 1` / `rearm_pending == 1`，`fam_id == 0`）**不在计数里** ⇒ 松 Shift 清不掉它，
         *   下一次聚焦时那条夹持态还在（闸还会掐）。`yk_drop_all` 里的 `yk_withdraw_all`
         *   **整表 reset**，把夹持态一起清干净；`had == 0` 时它只是一个 32 次的循环 + 不做任何日志。*/
        yk_drop_all(s, "unfocused");
        return 0;
    }
    yk_familiars_tick(s);
    yk_transfer_tick(s);
    /* ★★★ 2026-09-22（P1）：跟随 VM 表的**每帧不变式检查**（32 次比较，成本可忽略）。
     * 正常情况回收 0 条（判据已修，见 `yk_fam_vm_prune` 的注）；回收 >0 就是实跑判据里的
     * 「还有别的漏收路径」信号 ⇒ 打一行日志（含条数）让人一眼看见。
     * ⚠ 这一步**刻意包成一个 `noinline` 的小函数**：`on_tick_2` 已经很大，而它里面内联着
     *   `ce_damage_rect`（`sdk.h:172` 那段内联汇编）—— 往它身上再加几路活值，i686 的寄存器就不够，
     *   编译器会报「inline assembly requires more registers than available」（2026-09-22 实测）。*/
    yk_fam_vm_check();
    /* ★★★ #25：**符卡结束的下降沿**开一轮螺旋（一手：TH08 在符卡结算时给额度、之后分批吐）。*/
    {
        int sp = ce_spell_active();
        if (s->spell_was && !sp) {
            s->spiral_quota = YK_SPIRAL_QUOTA_FULL;
            s->spiral_t = 0;
        }
        s->spell_was = (uint8_t)(sp ? 1 : 0);
    }
    yk_spiral_tick(s);
    /* ★★ 2026-09-22（工单 E / route A）：**波次级复位的记账**。夹住的槽在下一次 `etOn`
     * （= 引擎自己的发弹事件）就被放开，卡 tick 的 `yk_note_slot` 同帧消费并重建使魔
     * （语义与一手见 `youkai_core.h` §3.5.2）。这里只**记账**：
     *   这一行在涨 = route A 生效（= 原来会掉进 120 帧哑火的那种局面被一帧内解决）；
     *   它**不涨而 `killed` 在涨** ⇒ 那些槽的 ECL 再也不 `etOn` 它们（只发生在「这一波本来就不发弹」的
     *   时候，无害）或者闸被更前面的卡掐在前面 —— 后者要看 `gate ACTIVE` 那行。 */
    {
        uint32_t rearms = yk_map_rearms(&s_map);
        if (rearms != s->rearms_seen) {
            ce_log("youkai: emitter gate re-armed %u slot(s) on the next etOn (route A, total %u) - "
                   "those emitters fire again and their familiar is rebuilt in the same tick",
                   rearms - s->rearms_seen, rearms);
            s->rearms_seen = rearms;
        }
    }
    /* ★★★ 2026-09-22（P8-3 / **route A′**）：`yk_shot_slot_gate` 在**第二次**被问到时当场放行并清夹持态
     * （不再等卡 tick 消费、也不再等 120 帧 TTL）⇒ 这一行在涨就是「恢复已经与卡 tick 解耦」的证据。
     * 与上面 `re-armed` 的分工：`re-armed` = 掐掉过几发（卡 tick 侧的消费），`released` = 断点侧放行了几次。*/
    {
        uint32_t rel = yk_map_releases(&s_map);
        if (rel != s->releases_seen) {
            ce_log("youkai: emitter gate released %u slot(s) on the second etOn (route A', total %u) "
                   "- clamp flags cleared at the breakpoint, the 1:1 mapping rebuilds next tick",
                   rel - s->releases_seen, rel);
            s->releases_seen = rel;
        }
    }
    /* ★ 2026-09-22（#7）：闸的**兜底**自愈记账。`YK_EMIT_BROKEN_TTL` 帧没再被 `etOn` 问到的
     * 发射口会被无条件放开。route A 落地后这一条**正常应当基本不涨**（只有「ECL 再也不 `etOn`
     * 这个槽」时才轮到它）；它在涨 ⇒ 那个槽是靠 TTL 才放开的，值得看一眼。*/
    {
        uint32_t heals = yk_map_heals(&s_map);
        if (heals != s->heals_seen) {
            ce_log("youkai: emitter gate self-healed %u slot(s) after %d frames without any etOn "
                   "(fallback path; total %u) - those emitters fire again", heals - s->heals_seen,
                   (int)YK_EMIT_BROKEN_TTL, heals);
            s->heals_seen = heals;
        }
    }
    return 0;
}

CE_CARD(108, .ctor = on_ctor, .on_load = on_load, .on_power_level_change = on_power_level_change,
             .on_tick_shooters = on_tick_shooters, .on_tick_2 = on_tick_2,
             .on_bullet_created = on_bullet_created, .on_enemy_death = on_enemy_death,
             .on_enemy_shot_slot = yk_on_shot_slot,           /* §3.5.1 发射口精确闸（0x433ffa）*/
             /* ★ 2026-09-22（P7）：`on_native_shot_suppress` **已退订** —— 妖侧主炮改成「认领 set +
              *   让引擎的行循环发」（见 `on_shot_set_index` 的注）。站点本身留着（109 还在用同一站点）。*/
             .on_shot_set_index = on_shot_set_index, .on_draw = on_draw,
             /* ★ 2026-09-22（P6）：本体脚本的换装改在**更新相位**（晚于自机 tick 0x17、早于自机绘制 0x1d）
              *   —— 原来挂在 `on_draw`（绘制 0x33）⇒ 移动起步那一帧会先画出引擎装的人侧脚本（闪一帧）。*/
             .on_item_frame = on_item_frame,
             /* ★ 2026-09-22（P3）：`on_item_commit` **已退订** —— 它原来那一源（收道具 ±身价/100）
              *   在原作里不存在（逐条一手见上面那段订正：`ItemManager.cpp:597` 是**收刻符 ±111**）。*/
             .on_death_before_deathbomb = on_death_before_deathbomb,   /* 人妖槽：死亡清零 */
             .on_option_pose_apply = on_option_pose_apply,    /* §3.3.1 低速子机声明层的落笔 */
             .on_run_reset = on_run_reset, .on_delete = on_delete);

/* ============================== 编译期对账 ============================== */
/* 卡的私有状态必须塞进 `CE_STATE_BYTES`(512) − `CE_STATE_RESERVED`(16)（sdk_core.h）。
 * 使魔表在文件级静态数组里，所以这里只查状态结构。*/
_Static_assert(sizeof(yk_card_t) <= CE_STATE_BYTES - CE_STATE_RESERVED,
               "youkai card private state exceeds the SDK budget");
/* ★ 逐格上限必须和声明层的上限一致（youkai_core.h 的 `YK_OPTION_CELLS` ↔ sdk_core.h 的
 * `CE_OPTION_POSE_CELLS`，两边都是「原生子机数组 4 格」那个一手上限）。*/
_Static_assert(YK_OPTION_CELLS == CE_OPTION_POSE_CELLS,
               "youkai_core formation cells must match the SDK option-pose cell cap");
/* 数量表的最大值必须塞得进本卡管的槽数（`YK_OPTION_MAX` = 本卡子机槽数组长度）。*/
_Static_assert(YK_OPTION_MAX >= 4, "YK_OPTION_MAX must cover the biggest row of YK_OPTION_BY_CHAR");
/* 跟随 VM 的静态表：32 × 12 B = 384 B（文件级静态，不进卡的 496 B 预算）。
 * 钉住记录的大小是为了让「加字段 = 表变大」这件事**在编译期**被看见（12 = 3 × u32/i32，无 padding）。*/
/* P4 让记录从 12 B 长到 20 B（+ 环句柄 + 环 interrupt 状态）⇒ 32 × 20 = 640 B 的文件级静态表。
 * 钉住大小是为了让「加字段 = 表变大」这件事在编译期被看见。*/
_Static_assert(sizeof(yk_fam_vm_t) == 20, "familiar follow-VM record grew (table is 32 of these)");

/* ============================== ★ 占位 / TODO 清单 ==============================
 * 每一条都「不自己发明数值」，取值一律是**不做**这个语义：
 *   §5 资产
 *     1. ~~`YK_BODY_SPRITE_FOCUSED` = -1~~ —— ✅ 2026-09-22 **已接真资产（路 A，owner 裁决）**：
 *        55 张妖侧本体图经 `assets/build_player_anm.py` 的 `YOUKAI_BODY` / `_youkai_plan`
 *        **追加**进 pl00/pl01/pl02/pl03.anm（4 个 entry + 20 支脚本），本体画法改成**整支换脚本**
 *        （`yk_body_sync` / `yk_body_load_script`，一手见 `youkai_core.h` §3.2）。旧路（写 `vm+0x24`）
 *        与它的全部一手订正留在 `AUDIT.md` §BE 详录 #50，**已不执行**。
 *        🟡 未实跑：装载与状态机只在代码层对过账（`make check` / 单测）。
 *     2. ~~`YK_SHOT_SET_YOUKAI` = 0x16 + 档位表~~ —— ✅ 2026-09-12 **已接真数据**：
 *        shooterset = TH08 `ply03as.sht` 满 P 档 9 行（`assets/sht/append_shooterset.py` 的 `REPLACE`
 *        覆盖原生那格，`fire_rate ∈ {3,5,6,10}`，除零风险随之消失）；**档位伤害表仍不填**
 *        （原因见 `youkai_core.c` §3.4 的注释：TH08 单发伤害随 P 下降，是配着路数设计的）。
 *        ⚠ 膛口 = 子机（`0x40a9c0` 一手）⇒ 它能不能发弹取决于第 3 条的子机数量（现已接上）。
 *     3. ~~`YK_OPTION_SCRIPT_YOUKAI`（借豌豆射手 102）~~ —— ✅ 2026-09-12 **已换成原生切图**：
 *        四支机体各一支子机脚本（`YK_OPT_SCRIPT_{YUKARI,ALICE,REMILIA,AYA}` = `ability.anm` 224..227，
 *        由 `assets/ability/make_youkai_option_art.py` 逐条照抄原生；一手落点写在那一节的注释里）。
 *        数量 / 阵型 / 落笔早在同一天接上（`YK_OPTION_BY_CHAR` 4×5 表 + `yk_option_offsets` 逐格阵型 +
 *        声明层 `on_option_pose_apply`，spec §3.3.1/§12.4）⇒ 这一项**不再是占位**。
 *        ⚠ 两个已知缺口，实跑要盯：① ~~`count = 0` 的档（早苗 P<100）妖侧**一发弹都没有**，
 *        而原生弹被 `on_native_shot_suppress` 压掉 ⇒ 那一档「什么都不发」~~ —— ✅ 2026-09-12 **按用户口径关掉**：
 *        止射闸加了第三条「本帧子机数 > 0」⇒ `count == 0` 时**放行原生弹**（见 `on_native_shot_suppress`
 *        与 `yk_should_suppress_native`；❓ 口径、非复刻），实跑会看到一行 `NOT suppressing native shots`；
 *        ② 12 格公共池与 71/77/116 共享，本卡满配 4 格。
 *        ❓ ③ TH08 那三支脚本的**中断标签**是 TH08 的约定（1/2 = 循环、3/4 = 起爆、5 = 散场），
 *        与 TH18 引擎发给子机 VM 的 2 出场 / 3 收起 / 1 销毁**不同** —— 逐条照抄原生的必然结果，
 *        要不要改标签是设计决定（见生成器文件头与报告 ❓）。
 *     4. `YK_FAM_ANM_INDEX` / `YK_FAM_ANM_SCRIPT` = -1 —— 使魔贴图（借原生 or 复用 116/117 的做法）
 *        → ✅ 2026-09-12 **资产已到**：四色原生切图 = `ability.anm` script 206..209
 *          （TH08 `enemy.anm` script53..56，红/绿/蓝白/黄），由**跟随 VM** 画
 *          （`YK_FAM_ANM_LAYER` 那一节；槽 i → s53+i）。上面那两个偏移保持占位（仍 ❓ 未证实）。
 *        🟡 未实跑：层号 7 与「VM 位置 = 实体 final_pos」都只在代码层对账过。
 *   §10 决策
 *     5. ~~`YK_FAM_LIFE` = 1~~ —— ✅ 2026-09-22（工单 E 第 2 条）**定案 = 100**，一手取自 TH08
 *        `familiarCreate` 的 life 参数众数（990 处里 100 出现 159 次；只看我们移植的那四套外观
 *        53..56 也是众数 68 次）；TH08 里更大的档（1200/13500/5000）在**本卡的 spawn 路径上不可用**
 *        （life ≥ 1000 ⇒ 引擎置锁定黑名单，`th18:0x42d8f6`）。已登记 `TUNING.md`。
 *        ★ 附带效果：`life = 1` 时 1/2 转嫁**从未生效过**（`yk_half_with_carry(1) == 0`）⇒ 现在才真正跑到。
 *     6. `YK_TRANSFER_CAP_PER_FRAME` = 0（单帧转嫁上限：0 = 无上限；研究 §7.5 提醒会顶掉 boss 配额）
 *     7. `YK_FAM_MAX` = 32（分配护栏；spec 没给上限）
 *     8. ~~`YK_OPTION_OFFSET_*` = 0（妖侧子机阵型）~~ —— ✅ 2026-09-12 **已接真数据**：
 *        逐格阵型 = 原生 `pl0X.sht` 的非聚焦/聚焦位置块（`yk_option_offsets`，`youkai_core.h` §3.3
 *        有一手出处；机体 2 的极坐标行已离线换算成像素）。单位 = 像素（引擎自己 ×128）。
 *   §3.5 缺断点
 *     9. `0x434019`（`etOn` 里 `push eax` 前）没装 —— ★ 2026-09-22：**不再需要单独装**：
 *        route A 用的 `0x433ffa`（同一 opcode 的「发弹三步」汇聚点，已在 `sdk.c` 里）就能在
 *        **发弹前**掐掉这一槽，且能顺便拿到「下一波发弹」这个事件。旧的「不重建 + 转嫁归零」兜底保留。
 *   🟡 待实跑
 *    10. 文件头的 `abs_pos` 双写（spec §3.5 只写 final_pos）
 *    11. `YK_TRANSFER_BOX_W/H` = 16×16、`YK_FAM_HURTBOX_HALF` = 12（默认 hurtbox 24.0 的一半）
 *    12. 止射闸（A 路线 = `retaddr` 改控制流）本身未实跑；与 117 咆哮 / 77 / 116 同装的验收见 spec §9。
 *        ⚠ 判据已按**用户口径**改成三条（多加「本帧子机数 > 0」⇒ `count == 0` 放行原生弹，
 *        2026-09-12）⇒ 与 spec §3.1 的字面**有意不同**，实跑要验的正是这一条：早苗 P<100 低速时
 *        屏幕上应当是**原生弹**在打（日志 `NOT suppressing native shots`），换机体 / 升到 P>=100
 *        之后立刻切回「原生弹被压 + 妖侧弹出膛」。
 *    13. 「聚焦态变了 ⇒ 先收子机再重申请」（脚本是申请时定死的）只在资产到位、脚本真的会变时
 *        才走到 —— 未实跑。★ 附带一手：`ce_option_drop` 现在**自己会把池槽松开**（`option+0x00 = 0`，
 *        见 sdk.c 的实现注释 + `0x40a790` 的空位判据）⇒ **不需要** `ce_repopulate_options()` 回收
 *   ❌ 已否决（写进代码是为了不再走一遍）
 *    14. `enemy+0x6320`（spec §3.9 的「第二杠杆」）= `invuln_timer.current`（研究 §2.1 一手）
 *        ⇒ 设正值 = 使魔无敌，与「使魔必须能被打」直接冲突 ⇒ **不采用**
 *    15. 直接写 `enemy+0x6220`（life）转嫁 —— 跳过 12 件事（符卡 ×7 记账 / 每帧上限 / 死亡判定本身），
 *        且会打乱死亡事件的 retaddr 分类（研究 §2.2）⇒ **不采用**
 *    16. `on_shot_damage` 当转嫁触发器 —— 它有 `CE_PLAYER_FOCUSED()` 门，妖机体期间一次都不来（研究 §0）
 *   ★★ 2026-09-22（人妖槽 + 永点，owner「那就先做人妖槽以及永点VM池」）—— 那一节自己的清单：
 *     已接（一手出处逐条写在代码里）：
 *       · 人妖槽：卡侧 i16 + 6 个限值（`th08:0x44d9ee`–`0x44da8f`）+ 4 个判定（`th08:0x406d10/40/70/a0`）
 *         + 夹紧（`th08:0x43c0bb`）+ 死区归零（`th08:0x44bf61`）+ HUD 四件（TH08 `ascii.anm` script5..8）
 *         + 百分比数字（共用件 HUD 数字）；
 *       · 涨源：击破敌机 ±200（`th08:0x42da2a`）、收道具 ±身价（`ItemManager.cpp:597` 的形状）、
 *         收刻符 ±111（`th08:0x4413cb`）、聚焦漂移（TH08 `Player::move` 的状态机）、死亡清零（`th08:0x44ac09`）；
 *       · 永点：刻符 VM 池（贴图/脚本逐条抄 TH08 `etama.anm` sprite179 + script68）+
 *         运动三段（出生上浮 → 归航 → 下落）+ 收集 → `PIV += ((1+(total&1))/2)×10`（`th08:0x418220`）+
 *         按 §BE 详录 #43 作为 `ΔG_08` 并进 `ecx` 声明（`ce_score_family_declare(FAMILY_08)`）。
 *     ❓ 自定（逐条登记在 `TUNING.md` §2.0x，实跑可能要动）：
 *       · `YK_ORB_MAX` = 64（池容量）、`YK_ORB_ANM_LAYER` = 7（层）、`YK_ORB_HOME_SPEED` = 3.0、
 *         `YK_ORB_PICK_R` = 20.0（TH08 那两个量在道具运动表 / 碰撞代码里，本轮没抽出一手）；
 *       · 三角 / 开方用**自己写的多项式与 Newton 近似**（不引 libm、不进 x87）；
 *       · 击破敌机撒刻符的数量取「双人组」支 + **连锁段数 n = 1** ⇒ 12 颗；
 *       · 擦弹 ⇒ 每点擦弹 1 颗（落在自机位置）、单帧上限 8 颗；
 *       · 漂移算式末尾的 `g_ShotSpeed` 取 1，两个计时器的重置时机做了简化；
 *       · HUD 块 256×78 与「每帧覆盖位置 ⇒ TH08 的 `posTime` 滑入被覆盖」；
 *       · 人妖槽**换关会归零**（一手是**不**归零，只有新局清）—— 因为本卡的 `on_load` 复用 `on_ctor`。
 *     ⏳ 本轮**不接**（都在报告「未决」里）：
 *       · 刻符的**分数**那一路（`yk_orb_score` 需要「本关/本局蓝点数」，TH18 只有 `CE_ADDR_MONEY_ITEMS` 这个近似物）
 *         与**符卡 +8000**（TH18 的符卡分不是 TH08 那套）；
 *       · 「收刻符动人妖槽」那道 `Timer::current_is_eq_(&0x18b89d4, 0)` 门 ❓ 语义未钉 ⇒ 本卡无条件动；
 *       · 人妖槽四档判定在 TH18 侧的**后果**（TH08 的蓝点 ×2 / 掉落分档 / 使魔弹染色 / 射击间隔）
 *         —— 判定与 HUD 染色已接，四档的玩法后果待 owner 拍（见 `AUDIT.md` §BE-FG §4）。*/
