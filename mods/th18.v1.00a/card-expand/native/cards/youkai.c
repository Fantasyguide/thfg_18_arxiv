/* 永夜抄（id 108）—— 妖机体 / 使魔（th08 系）。
 *
 * 设计唯一真源：`docs/superpowers/specs/2026-09-12-youkai-108-design.md`。
 * ★ TH08 的**社区反编译**（用户 2026-09-16 提供，MIT）：<https://github.com/Patchouli-CN/th08_dec>，
 *   本地克隆 `local/vendor/th08_dec`（`src/` 下的 `.cpp` 源码 + `config/reccmp-functions.csv` 的 2221 条名字↔地址）。
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
    /* ★★★ 2026-09-27（owner ①②）：咲夜（机体 2）的妖侧子机是**放置型** —— 位置在创建那一刻冻结
     * （`opt_pos` 存的就是那个**绝对**坐标，`opt_frozen` = 这一格已经冻好了），朝向每帧自转。
     * 一手：`th08:0x44eb70` 的 state 2 整段不碰位置 + 共尾 `face = norm(face + step)`（见 `youkai_core.h`）。*/
    float         opt_face[YK_OPTION_MAX];   /* 公转相位（弧度，TH08 `option+0x2d8` 的对应物）*/
    /* ★ 2026-09-27（owner「4 个小魔法阵得移动绕转」）：公转向量（单位向量；每帧按本格的角度增量
     * 用旋转矩阵转一点，**不引 libm**）。VM 的绘制位置 = 冻结锚点 + `YK_OPT_ORBIT_R` × 这个向量。*/
    float         opt_orbit[YK_OPTION_MAX][2];
    uint8_t       opt_frozen[YK_OPTION_MAX]; /* 1 = 这一格的位置已按创建那一刻冻好 */
    uint8_t       logged_aim;                /* ★ owner ⑤：朝敌角那一行只记一次 */
    yk_transfer_t tr;                  /* 1/2 转嫁累加器 */
    int32_t       opt_script;          /* 手里这几个子机是用哪个 anm 脚本申请的（-1 = 没有）*/
    uint32_t      shot_timer;          /* 我们自己的「射击帧」计数（引擎那个松开 Z 会跳，相位不可控）*/
    uint32_t      spawns;              /* 本局建了几只使魔 */
    uint32_t      broken;              /* 本局被打死的使魔数（= 被打死的发射口槽数）*/
    uint32_t      lost;                /* 状态差分撤掉的使魔数 */
    uint32_t      transferred;         /* 本局累计转嫁的伤害 */
    uint32_t      dropped;             /* 取不到 boss 而丢掉的转嫁量（研究 §4：本帧不转嫁）*/
    uint32_t      fam_full;            /* 使魔表满 / 堆分配失败而放弃的槽数 */
    uint32_t      emit_body_skipped;   /* ★★★ 2026-09-26（owner #37）：**体上炮口**被跳过的槽次数
                                        *   （发射口与敌人本体重合 ⇒ 不建使魔；判据见 core 的
                                        *   `yk_emit_on_body()`）。进 ledger 一行，实跑能看总量。*/
    uint8_t       se_fam_press;        /* ★★★ 2026-09-26：本帧是「人→妖」边沿 ⇒ 待播那声 `se_ophide`。
                                        *   延到 `yk_familiars_tick()` 之后再消费（门要读本帧的使魔表）。*/
    float         se_fam_press_x;      /* 上面那一发 SE 的自机 x（一手那声是定位音）*/
    uint8_t       logged_emit_body;    /* 「这一格是体上炮口」日志最多 8 行（避免刷屏）*/
    uint8_t       logged_opt;          /* 「子机声明停用（资产未到）」只记一次 */
    uint8_t       logged_sht;          /* 「妖侧 shooterset 没装」只记一次 */
    uint8_t       logged_noshot;       /* 「压了原生弹但自己也没弹」只记一次 */
    uint8_t       logged_pass0;        /* ★ 2026-09-12：「子机 0 ⇒ 放行原生弹（用户口径）」只记一次 */
    uint8_t       logged_hudgeo;       /* ★★ 2026-09-22（P2）：人妖槽 HUD 的**几何自述**只记一次
                                        *   （锚点 + 四个位置 + 倍数，实跑里用它与屏上所见对照，
                                        *   定「写进去的数生效的是 640 空间还是表面 px」这一争）*/
    uint8_t       hud_vm_up_logged;    /* ★ 2026-09-23：HUD 起来时**必打一次**逐 VM 实况的许可位
                                        *   （owner 那两局太短 ⇒ 60 秒那支一行都没打，位置判不出）*/
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
    int32_t       piv;                 /* 永点：PIV 累加器（TH08 的 `pointItemValue`，含难度初值）*/
    int32_t       piv_base;            /* ★ §BE #122：难度初值（`yk_night_point_init`）—— 声明给计分层的是
                                        *   **`piv − piv_base`**（「额外」），池初值才是 0（owner 口径）*/
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
    uint32_t      hud_night[YK_NIGHT_TEXT_MAX];  /* ★ #27/#28：**夜点数字**的字形 VM（左对齐 7 px/字、前导零不占位）*/
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
    uint32_t      opt_spin_vm;         /* ★★★ 2026-09-26：**上一次把旋转/待机标签发给哪一支 VM**。
                                        *   引擎会把这格的 VM 删掉重建（实跑日志：同一个 `ptr` 配不同的
                                        *   anm id），重建后的 VM 从脚本第 0 条开始 ⇒ 只发一次的旋转会丢。*/
    uint8_t       logged_chase;        /* 「追踪生效」只记一次 */
    uint8_t       logged_spin_resend;  /* 「VM 换了、补发旋转」只记一次（实跑判据）*/
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
    /* ★★★ 2026-09-22 夜（owner #23）：**本体**的 Shift 光环（一手 `Player::move` `th08:0x44aec0`）。
     *   `aura_id` = 光环 VM（我们的脚本 312 = TH08 `etama.anm` script54；0 = 没起来 / 已经淡完）；
     *   `aura_on` = 「现在应当在」（低速 = 妖侧）；`aura_logged` = 「起来过」只记一行；
     *   `focus_age` = **上一次形态切换之后过了几帧** —— 对应一手 `player+8`（人妖切换的进度计时器 0..7，
     *   `>= 7` 才置 `player+5 = is_youkai`），两下本体爆闪的门是 `>= 4`（`th08:0x44b14f` / `:0x44b3a9`）。*/
    uint8_t       night_logged;         /* ★ P-A：夜点初值只记一行 */
    uint8_t       solo_cap_logged;      /* ★ P-E：单人反向封顶「真的夹住了」只记一行 */
    uint16_t      gauge_ram;            /* ★ P-C：连续推进帧数（一手 `player+0xe2ae8`）*/
    uint16_t      gauge_idle;           /* ★ P-C：连停火帧数（一手 `player+0xe2ad0`，上限 30）*/
    uint8_t       gauge_shoot_logged;   /* ★ P-C：推进口径只记一行（1 = 斜坡 / 2 = 放符卡）*/
    uint16_t      orb_silence;          /* ★ P-D：击破魔使后的「收刻符不给槽」帧数（一手 50 帧）*/
    uint8_t       fam_nodmg_logged;     /* ★ 批次 2：免伤位「按形态切过」只记一次（=1 妖侧 / =2 人侧）*/
    uint32_t      fam_miss_latch;       /* ★ 批次 3：已经撞过的那只使魔的 fam_id（接触期不重复触发）*/
    uint8_t       fengma;               /* ★ P-B：本帧在不在逢魔（极人 OR 极妖）*/
    int32_t       fengma_score;         /* ★ P-B：本局逢魔自动加分的累计（诊断）*/
    uint32_t      aura_id;
    uint16_t      focus_age;
    uint8_t       aura_on;
    uint8_t       aura_logged;
} yk_card_t;

static yk_map_t s_map;

/* ★ 批次 3：表里有没有活着的使魔（撞判用的零成本门）。*/
static int s_map_has_fam(void)
{
    unsigned i;
    for (i = 0; i < YK_FAM_MAX; ++i) if (s_map.e[i].fam_id) return 1;
    return 0;
}              /* 使魔表（(enemy_id, 发射口槽) → 使魔）*/

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

/* ★★★ 2026-09-23（擦弹第 1 颗的场上门）：TH08 `EnemyManager::is_boss_active`（`th08:0x42f1f0`）
 * 的 TH18 对应物 = **boss id 表里任一格非 0**（一手扫 `ENEMY_MANAGER.bosses[0..7]` 那 8 个指针，
 * 任一非 0 即真；TH18 的同位表 = `zEnemyManager+0x48` 的 **4** 格 `int32` enemy_id）。
 * SDK 侧**没有**现成的「boss 活否」封装（`sdk.h:582` 只给了读法本身：「boss 用
 * `em + CE_EM_BOSS_IDS + i*4` 取 id 再问它」）⇒ 这里用**已有的常量**（`engine.h` 的
 * `CE_EM_BOSS_IDS` / `CE_EM_BOSS_SLOTS`）+ 本卡 `yk_boss()` 的**同一条读法**
 * ⇒ **零新增站点、零新增断点**。
 * ⚠ 与 `yk_boss()` 的两点差别（都照一手）：① 那个服务「追击目标」，要求 `ce_enemy_lockable`；
 * 这里「槽位非 0」即真（一手不看状态位）；② 槽数取 TH18 的 4（TH08 是 8）。*/
static int yk_boss_active(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    unsigned i;
    if (!em) return 0;
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i)
        if (*(const int32_t *)(em + CE_EM_BOSS_IDS + (size_t)i * 4) != 0) return 1;
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

/* ★★★ 2026-09-23（owner #37/#36「没有使魔也出魔法阵 / 也响切换音」）：
 * `yk_map_fam_count()` 数的是**映射表里的发射口**，而**与敌人重合的发射口按规则不建使魔**
 * ⇒ 那些格子照样留在映射里 ⇒ 计数 > 0 ⇒ 一按 shift 就响一声 + 在敌人身上画一圈魔法阵
 *（owner 逐字：「**那一声切换是绑定在使魔形态转换上的**…我一按 shift 发现敌人身上都有一个魔法阵」）。
 * ⇒ 判据改成**真正活着的使魔实体**（`yk_familiar_by_id()` 解得到）。环 / 切换音 / 爆闪三处全用它。*/
static unsigned yk_map_fam_live_count(const yk_map_t *m)
{
    unsigned i, n = 0;
    if (!m) return 0;
    for (i = 0; i < YK_FAM_MAX; ++i)
        if (m->e[i].enemy_id && m->e[i].fam_id && yk_familiar_by_id(m->e[i].fam_id)) ++n;
    return n;
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
static uint8_t g_fam_nodmg_state = 0xff;   /* ★ 批次 2：上一次记过的免伤位状态（0/1；0xff = 还没记过）*/

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
    /* ★★★ 2026-09-22 夜（owner 口径 **批次 2**）：**人侧可打、妖侧不可打**。
     * 一手：引擎自己的免伤闸 = `enemy+0x635c` 的 **bit4**（`th18:0x42f2b9 test BYTE [ebx+0x5130],0x10`，
     * `ebx` = `zEnemyData` ⇒ 绝对 `0x122c+0x5130 = 0x635c` = `CE_ENEMY_FLAGS`）——**置上就扣不了血**；
     * 另一道是 `enemy+0x6320` 的 invuln 计时器（> 0 也免伤），我们只用位这一道（干净、可逆、无计时）。
     * ⇒ **妖侧（低速/聚焦）置位**：自机弹打不出伤害 ⇒ 打不掉 ⇒ P-D 那套（掉刻符 / `−gauge/12` / 50 帧静默）
     *   **结构上不可能触发**；**人侧清位**：照旧可打、可击破。*/
    {
        uint32_t *fl = (uint32_t *)(f + CE_ENEMY_FLAGS);
        int want = CE_PLAYER_FOCUSED() ? 1 : 0;
        if (want) *fl |= CE_ENEMY_FLAG_NO_DAMAGE;
        else      *fl &= ~(uint32_t)CE_ENEMY_FLAG_NO_DAMAGE;
        if (g_fam_nodmg_state != want) {           /* 两态各记一次（实跑判据）*/
            g_fam_nodmg_state = (uint8_t)want;
            ce_log("youkai: familiar damage gate = %s (TH18 `enemy+0x635c` bit4, engine's own gate "
                   "`th18:0x42f2b9`) - hit-able only on the HUMAN side, so the kill payout "
                   "(1 orb + -gauge/12 + 50f silence) can only happen there",
                   want ? "NO-DAMAGE (youkai side)" : "hittable (human side)");
        }
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
    /* ★★★ 2026-09-22 夜（owner：**妖侧只剩魔法阵**）：**常驻魔法阵环**的句柄（只在妖侧存在）。
     * 与 `yk_ring_blink` 那支**切换短命环**是两件事：这支是「妖侧使魔本体不可见时**一直**看得见的那圈」。*/
    uint32_t circle_id;/* 0 = 这一帧没有常驻魔法阵（人侧 / 起失败 / 还没起）*/
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
    if (v->circle_id) { ce_anm_delete(v->circle_id); v->circle_id = 0; }
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
    /* ★★★ 2026-09-22 夜（owner 二次订正）：**使魔身上不再挂常驻环** —— 环改成「切换那一下的
     * 短命特效」（`yk_ring_blink`，见上面那一段的长注）。使魔本体这一层只剩一个跟随 VM。*/
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
    /* ★★★ 2026-09-22 夜（owner 口径：**使魔两种形态都存在**）—— 按形态切外观：
     *   · **人侧（高速 / 未聚焦）**：画**本体**（现有跟随 VM，四色原生切图）；
     *   · **妖侧（低速 / 聚焦）**：**本体不可见**（alpha 置 0，VM 不删 ⇒ 切回来一帧就恢复），
     *     改画**每只一支常驻魔法阵环**（TH08 `etama.anm` script48 / 我们的脚本 309 / sprite 479，
     *     `SetInterrupt(2)` = 妖侧蓝 —— 一手 `th08:0x41b04e SetInterrupt(环, IsYoukai()?2:1)`）。
     * ⚠ 与 `yk_ring_blink` 的**切换短命环**是两支 VM、两个用途（那支是「切换那一下闪一圈」）。*/
    {
        int youkai_side = CE_PLAYER_FOCUSED();
        if (v->anm_id) ce_anm_set_color(v->anm_id, youkai_side ? 0x00ffffffu : 0xffffffffu);
        if (youkai_side) {
            if (!v->circle_id || !ce_anm_get_vm(v->circle_id)) {
                v->circle_id = ce_anm_spawn(CE_ABILITY_ANM(),
                                            CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING,
                                            YK_FAM_ANM_LAYER);
                if (v->circle_id) ce_anm_interrupt(v->circle_id, 2);   /* 2 = 妖侧蓝（一手）*/
            }
            if (v->circle_id && !ce_anm_set_pos(v->circle_id, *(const float *)(f + CE_ENEMY_POS_X),
                                                               *(const float *)(f + CE_ENEMY_POS_Y), 0.0f))
                v->circle_id = 0;                          /* 句柄作废 ⇒ 下一帧自愈重起 */
        } else if (v->circle_id) {
            ce_anm_delete(v->circle_id);                   /* 人侧不留魔法阵 */
            v->circle_id = 0;
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
 *
 * ★★★ 2026-09-27（owner ⑦：「**切换机体的时候，人->妖是红色圆特效，妖->人是蓝色，现在都是白色**」）：
 *   **颜色是 `SpawnEffect` 的栈参**，不在 anm 脚本里 —— 两支 ported 脚本都没有 `color()`（零翻译照抄），
 *   所以画面用的是贴图原色 ⇒ 白 ✗。一手把两个色号给了：
 *     · 使魔身上：人→妖 `0x80303080`、妖→人 `0x80803030`（`th08:0x42c45f` / `:0x42c4ff`）；
 *     · 本体那两下（`0x44b155` / `0x44b3af`，见下面 #23 那一段）：人→妖 `0x80ff8080`、妖→人 `0x808080ff`。
 *   一手那两个立即数按**小端内存序 = R,G,B,A** 解释（`0x80303080` → R=0x80 G=0x30 B=0x30 A=0x80 = **红**，
 *   与 owner 的「人→妖红 / 妖→人蓝」逐条吻合 ⇒ 这就是那条口径的一手证据）。
 *   TH18 的 `ce_anm_set_color` 收的是 **`0xAARRGGBB`**（本卡别处用 `0x00ffffff` = 全透明白 ⇒ A 在高字节）
 *   ⇒ 传参时**逐通道搬一次**：GRED = `0x80803030`、GBLUE = `0x80303080`（使魔那一对）、
 *   BGRE = `0x808080ff`、BBLUE = `0x80ff8080`（本体那一对）。
 * ⚠ 两支脚本**自己 `delete()`**（`+10:` 那一步）⇒ **卡侧不跟踪句柄**，起完就不管（10 帧后自灭）；
 *   也因此**不需要**在 `on_delete` 里收尾（最坏情况是删卡后还飘 ≤10 帧的一闪）。
 * ⚠ 位置 = 使魔的位置（一手就是 `enemy+0x2d88`）；我们只在**那一帧真的有使魔**时起
 *   （人→妖 = 重建出来的那些使魔；妖→人 = 撤走前最后记下的坐标），免得在没使魔时凭空闪一下。*/
#define YK_FLASH_FAM_RED   0x80803030u   /* 使魔·人→妖（一手色 `0x80303080` 按 RGBA 内存序重排）*/
#define YK_FLASH_FAM_BLUE  0x80303080u   /* 使魔·妖→人（一手色 `0x80803030`）*/
#define YK_FLASH_BODY_BLUE 0x808080ffu   /* 本体·人→妖（一手色 `0x80ff8080`）*/
#define YK_FLASH_BODY_RED  0x80ff8080u   /* 本体·妖→人（一手色 `0x808080ff`）*/

static void yk_flash_spawn(int script, float x, float y, uint32_t color)
{
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), script, YK_FAM_ANM_LAYER);
    if (!id) return;                    /* 起不出来只影响观感（10 帧的一闪）⇒ 不记日志 */
    ce_anm_set_pos(id, x, y, 0.0f);
    /* ★ 2026-09-27（owner ⑦）：颜色 = 一手 `SpawnEffect` 的栈参（脚本里没有 color()）*/
    ce_anm_set_color(id, color);
}

/* ============ ★★★ 2026-09-22 夜（owner **二次订正**）：切换那一下的**短命魔法阵环** =============
 * owner 原话：「**它不是一个固定的环，而是在切换时候的一个特效环，只会存在一小段时间 / 你可能扫错了**」。
 *
 * **一手（先说清旧实现站在什么上，免得后人以为我们瞎改）**：TH08 的使魔环（`etama.anm` script48，
 *   sprite197 = `etama2.png (193,113,62,62)` 的**空心环**）确实是**挂在使魔身上、与使魔同生**的：
 *   · 建使魔时 `th08:0x41b003` 门 `enemy+0x53c8 == 0` → `0x41b01d push 0x20` + `call 0x425b70`（第三个分配器）
 *     → 句柄存 `enemy+0x53c8`（`0x41b02c`），紧接着 `0x41b04e SetInterrupt(环, Player::is_youkai()?2:1)`；
 *   · Shift 时 `FUN_0042c420` 只**重发** interrupt（人→妖 `0x42c482` 发 2、妖→人 `0x42c522` 发 1）+ 爆闪 + 音效；
 *   · 脚本**主路径不自删**：`interruptLabel(1/2)` 淡入 8 帧 → `stop()`；只有 **interrupt 3**
 *     是「淡出 10 帧 + `delete()`」（那是使魔死亡/回收那条路）。
 *   ⇒ 一手里它是**常驻环**（= 我们 P4 那一版做的形态）。
 *
 * ⇒ **本实现 = owner 口径覆盖一手**（登记 `TUNING.md` §2.0ze 的 `❓ 自定`）：按 owner 要的**形态**做 ——
 *   **切换那一下**在**每只当时在场的使魔**位置**起一支新的环**（人→妖 = interrupt 2 蓝 / 妖→人 = 1 紫，
 *   沿用一手 `0x41b04e` 的取色法），保持 `YK_RING_LIFE_FRAMES` 帧后发 **interrupt 3**
 *   ⇒ 脚本自己淡出 10 帧再 `delete()` ⇒ **「闪一圈、很快消失」**，场上**不再有常驻环**。
 *   ⚠ 环与爆闪一样是**纯观感**：起不出来只记一行，不动任何机制。*/
#define YK_RING_MAX         4      /* 一次切换最多起 4 支（一闪而过；多出来的丢弃）*/
#define YK_RING_HOLD_FRAMES 6      /* 淡入 8 帧（一手 script48 的 `alphaTime(8,0,…)`）+ 保持 6 帧 */
#define YK_RING_LIFE_FRAMES (8 + YK_RING_HOLD_FRAMES)

typedef struct { uint32_t id; uint8_t age; } yk_ring_t;
static yk_ring_t s_ring[YK_RING_MAX];
static unsigned  s_ring_failed;     /* 起失败的支数（诊断；进 ledger 那一行 —— 不单独刷屏）*/
static uint8_t   s_ring_logged;     /* 「起失败」只记一行 */

/* 起一支短命环（`variant`：1 = 人侧紫 / 2 = 妖侧蓝 —— 一手 `0x41b04e` 的 `IsYoukai()?2:1`）。*/
static void yk_ring_blink(float x, float y, int variant)
{
    unsigned i;
    uint32_t id;
    for (i = 0; i < YK_RING_MAX; ++i) if (!s_ring[i].id) break;
    if (i == YK_RING_MAX) return;                      /* 池满 ⇒ 这一次少闪几圈（纯观感）*/
    id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING,
                      YK_FAM_ANM_LAYER);
    if (!id) {
        ++s_ring_failed;
        if (!s_ring_logged) {
            s_ring_logged = 1;
            ce_log("youkai: switch ring VM spawn FAILED (script %d) - the Shift magic circle is "
                   "missing (cosmetic only)", (int)CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING);
        }
        return;
    }
    s_ring[i].id = id;
    s_ring[i].age = 0;
    ce_anm_set_pos(id, x, y, 0.0f);
    ce_anm_interrupt(id, variant);                     /* 出生是 `scale(0) alpha(0)` ⇒ 必须立刻发一次 */
}

/* 每帧：计时；到点发 interrupt 3（脚本淡出 10 帧 + `delete()`）⇒ 之后不跟踪（自删）。*/
static void yk_ring_tick(void)
{
    unsigned i;
    for (i = 0; i < YK_RING_MAX; ++i) {
        if (!s_ring[i].id) continue;
        if (!ce_anm_get_vm(s_ring[i].id)) { s_ring[i].id = 0; continue; }   /* 引擎清过 ⇒ 丢弃 */
        if (++s_ring[i].age >= YK_RING_LIFE_FRAMES) {
            ce_anm_interrupt(s_ring[i].id, 3);
            s_ring[i].id = 0;                          /* 剩下 10 帧淡出由脚本自己走完 */
        }
    }
}

/* 收干净：**只有「卡的 tick 会停」的两条路**需要它（`on_delete` 商店里删卡 / `on_run_reset` 换关）。
 * ⚠ **不要**放进 `yk_drop_all` —— 松 Shift 时 drop_all 就跑在「刚起环」之后，会把刚闪的那一圈立刻收掉。*/
static void yk_ring_kill_all(void)
{
    unsigned i;
    for (i = 0; i < YK_RING_MAX; ++i) {
        if (s_ring[i].id) { ce_anm_delete(s_ring[i].id); s_ring[i].id = 0; }
    }
}

/* ============ ★★★ 2026-09-22 夜（owner #23）：**本体**的 Shift 光环 + 两下**本体**爆闪 ==============
 * owner 原话：「人妖切换的时候**在本体会有一个机体印象色的光环**」。
 *
 * 一手（`Player::move` `th08:0x44aec0`，逐指令；**进妖侧**那支 `0x44b11f` 起）：
 *   · `0x44b171`–`0x44b19c`：若 `player+0xbe834 == 0`（光环句柄）⇒
 *     `push -1 / 1 / 2 / &player+0x2b4 / 0x16` → `0x44b194 call 0x425870`
 *     （`EffectManager::sub_425870_creates_indexed_effect`；re-data 名）⇒ 模板表 `0x4c6d30[0x16]`
 *     = **`etama.anm` script54**，句柄存 `player+0xbe834`（`zPlayer.__effect_be834`）。
 *     它的 init（`0x426c40`）每帧把位置钉到**自机**（读全局 `0x17d61ac` 一族 = `player+0x2b4`）⇒ 我们每帧 `set_pos`。
 *   · **妖→人**那支 `0x44b3cc`–`0x44b3f0`：`0x44b3e3 call 0x407120` = `AnmVm::SetInterrupt(光环, 1)`
 *     ⇒ 脚本走 `interruptLabel(1)`：**30 帧淡出后自己 `delete()`**；随后卡侧清句柄。
 *   · 两下**本体**爆闪（一次性，各自 `+10: delete()`；**与使魔那对 59/60 不是同一对**）：
 *       **人→妖** `0x44b155 SpawnEffect(0x1d, &player+0x2b4, 1, 0x80ff8080)`（我们的 **313** = TH08 script58）；
 *       **妖→人** `0x44b3af SpawnEffect(0x1c, …, 0x808080ff)`（我们的 **314** = TH08 script57）。
 *     两支都起在**自机**位置，且与光环同受 `player+8 >= 4` 的门（= 上一次形态至少持续了 4 帧；
 *     我们这里就是 `s->focus_age`，在**这次**切换的边沿上读「上一次持续了多少帧」）。
 *   · ⚠ **颜色**：一手的光环颜色参是 `-1` = `zAnmVm.color_1` **中性** ⇒ **TH08 里不逐机体变色**
 *     （同一条函数范围内的数据引用全扫过，没有颜色表；见 `TUNING.md` §2.0zd 的 `❓ 自定` 那一行）。
 *     本批按一手实现（固定色的淡红火花云）；「逐机体印象色」要 owner 拍。
 *
 * 生命周期（铁律：长持续效果必须交代「被删掉会怎样」）：
 *   光环只在**妖侧**存在 ⇒ 每一条**离开妖侧**的路都要过 `yk_aura_kill()`，而它们**全都**经过
 *   `yk_drop_all()`（松 Shift / miss / 局末 / 删卡 / 换关 / 敌人管理器没了）⇒ 把它挂在 `yk_drop_all` 的
 *   最前面（与「发射口闸」同一个唯一复位口的思路）。kill = 发 `interrupt 1`（30 帧淡出后自删）+ 清句柄。*/
#define YK_AURA_FLASH_MIN_AGE 4           /* 一手 `player+8 >= 4`（爆闪门；见上）*/

static void yk_aura_kill(yk_card_t *s)
{
    if (!s) return;
    if (s->aura_id) {
        ce_anm_interrupt(s->aura_id, 1);   /* 脚本 interruptLabel(1)：30 帧淡出 + delete() */
        s->aura_id = 0;
    }
    s->aura_on = 0;
}

/* 每帧跟随（一手就是每帧钉在自机）+ 自愈（句柄被引擎清过 ⇒ 重起；脚本自己 delete 了 ⇒ 清 0）。*/
static uint32_t s_orb_log_tick;
static uint32_t s_orb_vm_logged;    /* ★ #42：出生即记的计数（前 5 颗）*/
static uint32_t s_orb_log_pending;   /* ★ #42：下一帧要打的那颗 VM id（0 = 不用打）*/     /* ★ 2026-09-23（#42）：永点逐 VM 实况的节拍 */
static uint8_t s_aura_edge_spawn;   /* ★ #32/#29：光环「每个形态边沿只起一次」的许可位 */
static void yk_aura_tick(yk_card_t *s, float x, float y)
{
    if (!s || !s->aura_on) return;
    if (s->aura_id && !ce_anm_get_vm(s->aura_id)) s->aura_id = 0;   /* 引擎清过 VM 列表 ⇒ 句柄作废 */
    /* ★★★ 2026-09-23（owner #32/#29：「th08 按 shift **光环动画持续触发**」，判据 = **切换那一下的边沿**）：
     *   原来这里「只要 `aura_id == 0` 就起一支」= **每帧自愈重起** —— 一旦外壳的 VM 被引擎清掉
     *   （或 `ce_anm_set_pos` 当帧失败把句柄清零），下一帧又会起一支**从头播**的动画 ⇒
     *   观感就是「光环一直重新触发」。
     *   ⇒ 改成**每个边沿只起一次**：`s_aura_edge_spawn` 只在「进低速」那一支置 1，
     *     这里消费掉（起成功就清 0）⇒ VM 中途没了**不补**（等下一次切换），动画只播一遍。
     *   ⚠ 与 #33/#38 的「高速完全回归原生」一致：离开低速走 `yk_aura_kill()`（interrupt 1 → 30 帧淡出自删）。*/
    if (!s->aura_id && s_aura_edge_spawn) {
        s->aura_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_AURA,
                                  YK_FAM_ANM_LAYER);
        if (!s->aura_id) {
            ce_log("youkai: body Shift aura VM spawn FAILED (script %d) - the TH08 body halo "
                   "(etama.anm script54) is missing this frame",
                   (int)CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_AURA);
            return;                                          /* 下一帧再试（脚本没起来不影响机制）*/
        }
        s_aura_edge_spawn = 0;                               /* ★ #32/#29：本边沿只起一次 */
        if (!s->aura_logged) {
            s->aura_logged = 1;
            ce_log("youkai: body Shift aura ON (TH08 `Player::move` 0x44b194 -> etama.anm script54) - "
                   "fixed light-red spark cloud, mix colour -1 = neutral, follows the player; "
                   "leaving low speed sends interrupt 1 (30-frame fade then delete)");
        }
    }
    if (!ce_anm_set_pos(s->aura_id, x, y, 0.0f)) s->aura_id = 0;    /* 句柄作废 ⇒ 下一帧自愈重起 */
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
        /* ★ #22：人→妖那一帧的爆闪（310）—— 起在**第一只**建出来的使魔身上（一手 script59）。
         * ★ owner 二次订正：同一下还要起一支**短命环**（一手里环是常驻的，owner 要切换那一下的形态）。*/
        if (s && s->flash_pending) {
            s->flash_pending = 0;
            yk_flash_spawn(CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_FLASH59, act->x, act->y,
                           YK_FLASH_FAM_RED);
            yk_ring_blink(act->x, act->y, 2);          /* 2 = 妖侧蓝（一手 `0x41b04e`）*/
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
/* ★★★ 2026-09-22 夜（owner 口径：**使魔两种形态都存在**）：把「离开妖侧的收尾」与「撤使魔」
 * **拆成两支**（原来挤在 `yk_drop_all` 里，于是松 Shift 会把使魔一起撤掉 —— 那正是 owner 说错的形态）。
 *
 *   · `yk_leave_youkai(s)`：**只收「妖侧才有的东西」** —— 光环（#23）、切换环（#27 的短命环）、
 *     染色（#26 的决死红）、以及 `tr.pending`；
 *   · `yk_drop_all(s, why)`：**撤使魔**（撤回表 + 收跟随 VM）—— 只在**真正要撤**的场合调：
 *     删卡（`on_delete`）、换关（`on_run_reset`）、自机没了（`on_tick_2` 的 `!CE_PLAYER()`）、
 *     世界没了（`yk_chase_target_resolve` 的 `!em` 分支）。
 * ⚠ **松 Shift（`!focused`）不再走 `yk_drop_all`** —— 使魔在人侧也活着（owner 口径），
 *   1:1 映射与闸（`fam_seen` / `broken` / `rearm_pending`）跨形态保持。*/
static void yk_leave_youkai(yk_card_t *s)
{
    yk_aura_kill(s);                     /* ★ #23：光环只在妖侧存在 */
    if (s) s->tr.pending = 0;
}

static void yk_drop_all(yk_card_t *s, const char *why)
{
    yk_action_t acts[YK_FAM_MAX];
    unsigned had = yk_map_fam_count(&s_map);
    int n;
    yk_aura_kill(s);                     /* 删卡 / 换关 / 自机没了：光环也一起收（幂等）*/
    n = yk_withdraw_all(&s_map, acts, (int)YK_FAM_MAX);

    yk_run_actions(s, acts, n);
    /* ★ 兜底收 VM（句柄幂等）：`acts` 装不下的条目、以及 `fam_id` 已经被清掉但 VM 还挂着的残局，
     * 都靠这一支清干净 —— 删卡那一刻 tick 全停，这里就是**唯一**的机会（照 112 的 kill_all）。*/
    yk_fam_vm_kill_all();
    if (had && why) ce_log("youkai: withdrew %u familiar(s) (%s)", had, why);
    if (s) s->tr.pending = 0;
}

/* 扫全场敌人 → 1:1 映射 + 状态差分。*/
/* ★★★ 2026-09-22 夜（owner **批次 3**）：**人侧撞到使魔 ⇒ MISS**（妖侧「不可见可穿梭」⇒ 不判）。
 *
 * 一手/口径：
 *   · owner：「人间操术下撞击会 MISS；妖怪操术下不可见但可穿梭其间」（`TH08-ITEMS-AND-SC.md` §2b）；
 *   · 自机判定半径 = `player+0x4799c`（非聚焦，原生 **3.0**；`engine.h` 的 `CE_PLAYER_HITBOX_R_NF`）；
 *   · 使魔 hurtbox 半宽 = **12.0**（惰性真敌人默认 hurtbox 24 的一半，`YK_FAM_HURTBOX_HALF`，spec §6.1）；
 *   · 判据 = **圆 vs 圆**：`dx²+dy² <= (3.0 + 12.0)²`（`yk_fam_hit_player`，纯逻辑在 core，有单测）。
 *   ⚠ 用 `ce_forced_death_begin/end` **包不包**这一调 = 一个开关（`YK_FAM_HIT_FORCED_DEATH`）：
 *     包上 ⇒ 111 的河童罩子会**让位**（`sdk_core.h` 的通报位语义：那是给「卡造的死」用的，例如 113 的补死）；
 *     不包 ⇒ 这次死亡与**真中弹**完全同形，**所有救援层（107/113/117 + 111 罩子）照旧生效**。
 *     本批取 **0（不包）**：owner/工单要的是「撞魔使 = 一次**普通 MISS**」+「救援层该照旧生效」。
 *     ⇒ 要 111 让位就把这一个数改成 1（一行、无其它影响）。
 *   · **只在人侧判**（妖侧与批次 2 的免伤位分工一致：妖侧**打不到 + 撞不到**）；
 *   · 自机不在存活态（`CE_PLAYER_STATE != 1`）⇒ 不判（死了/决死窗口里不该再撞）；
 *   · 同一只使魔**只触发一次**（`fam_miss_latch` 记住那只 `fam_id`，离开接触范围后解锁）。*/
#define YK_FAM_HIT_FORCED_DEATH 0     /* ❓ 口径（见上）；1 = 用通报位包住这一调（111 罩子让位）*/
#define YK_FN_PLAYER_DIE 0x45d3a0     /* TH18 `Player__die`（一手；先例 `trance.c` 的 `TRANCE_FN_PLAYER_DIE`）*/
typedef void (__attribute__((thiscall)) *yk_fn_player_die_t)(void *player);

static void yk_fam_collide_tick(yk_card_t *s)
{
    uint8_t *pl;
    const float *pp;
    float hit_r;
    unsigned i;
    if (!s || !s_map_has_fam()) return;
    if (CE_PLAYER_FOCUSED()) { s->fam_miss_latch = 0; return; }   /* 妖侧：可穿梭 ⇒ 不判 */
    pl = CE_PLAYER();
    if (!pl) return;
    if (*(const int32_t *)(pl + CE_PLAYER_STATE) != 1) return;    /* 只在存活态判（1 = 存活）*/
    pp = ce_player_pos();
    if (!pp) return;
    hit_r = *(const float *)(pl + CE_PLAYER_HITBOX_R_NF);         /* 一手 3.0 */
    for (i = 0; i < YK_FAM_MAX; ++i) {
        uint8_t *f;
        uint32_t fid = s_map.e[i].fam_id;
        float dx, dy;
        if (!fid) continue;
        if (fid == s->fam_miss_latch) continue;                   /* 这一只已经撞过（接触期不重复）*/
        f = yk_familiar_by_id(fid);
        if (!f) continue;
        dx = pp[0] - *(const float *)(f + CE_ENEMY_POS_X);
        dy = pp[1] - *(const float *)(f + CE_ENEMY_POS_Y);
        if (!yk_fam_hit_player(dx, dy, hit_r)) continue;
        s->fam_miss_latch = fid;                                  /* 先落闩，异常路径也不会连撞 */
        ce_log("youkai: MISS by touching familiar enemy_id %u (human side; TH08 人型撞魔使 = 一次普通 MISS, "
               "hit_r %.1f + hurtbox %.1f)", fid, (double)hit_r, (double)YK_FAM_HIT_REACH);
#if YK_FAM_HIT_FORCED_DEATH
        ce_forced_death_begin();
#endif
        ((yk_fn_player_die_t)(uintptr_t)YK_FN_PLAYER_DIE)(pl);
#if YK_FAM_HIT_FORCED_DEATH
        ce_forced_death_end();
#endif
        return;                                                   /* 一帧最多一次 */
    }
    s->fam_miss_latch = 0;                                        /* 没在接触 ⇒ 解锁 */
}

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
            /* ★★★ 2026-09-26（owner #37，两次实跑同一条：「**发射口与敌人重合不应该建立使魔**」/
             *   「依旧能看见敌人的身上有魔法阵」）：**体上炮口直接跳过整格**。
             *   为什么是 `continue`（跳过）而不是「建的时候拒掉」：
             *     ① 若在 `yk_note_slot` **之后**拒建 ⇒ 表里留下 `active=1 / fam_id=0 / broken=0` 的条目
             *        ⇒ 同一个 `CREATE` 分支**每帧**再命中一次（每帧一次失败 + 日志刷屏）；
             *     ② 若在 `yk_note_slot` 里「不建但不清」⇒ 这一格一旦曾经建过壳（`fam_seen=1`），
             *        壳撤掉后闸会**夹住敌人本体炮**（`yk_shot_slot_suppressed` 只看 `fam_seen`），
             *        要等 `YK_EMIT_BROKEN_TTL`（120 帧）才自愈；
             *     ③ 跳过 ⇒ 本帧「没扫到这一格」⇒ `yk_frame_end` 当场退休该条目（原来有壳就发一次
             *        `WITHDRAW`，壳与跟随 VM 一起收掉），`fam_seen` 归零、闸恒放行。
             *   判据 = `yk_emit_on_body()`（相对模式 + 偏移恰好 (0,0)；一手与数据统计见 core 的声明段）。
             *   ⚠ 与 TH08 的关系：原作**从不**把发射口翻译成使魔（`etOn` handler `th08:0x41b883` 只有
             *     两条指令 + 跳尾；全 exe 5 处 `create_enemy` 没一处在 `et*` 家族里），使魔只由 ECL 的
             *     `familiarCreate*` 显式产生，而「体上发射口」在原作是**常态**（`ecldata8` 的 `Sub14`
             *     整族一只使魔都不建）⇒ 这条跳过是**复刻原作的默认行为**，不是我们发明的门。*/
            if (yk_emit_on_body(off3, org3)) {
                if (s->emit_body_skipped < 0xffffu) ++s->emit_body_skipped;
                if (s->logged_emit_body < 8) {
                    ++s->logged_emit_body;
                    ce_log("youkai: emitter slot %d of enemy_id %u IS the enemy's own body "
                           "(relative offsets 0,0) - NO familiar built (owner #37); TH08 never "
                           "turns body emitters into familiars either", i, (unsigned)id);
                }
                continue;
            }
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
 * 一手：TH08 的目标 = **全局 `g_BossEnemy`**（= `g_Player+0xe2abc`，绝对地址 `th08:0x18b89b4`；
 * 定向侧 `th08:0x44e8f1` / `th08:0x450279` 都读它、取 `final_pos` = `zEnemy+0x2d88`）。
 * ⚠ 2026-09-27 订正（owner：「原版灵梦妖子机的索敌你核一下，可能不是全屏索敌」）：它**不是**任何形式
 * 的范围搜索 —— 写点是 `th08:0x42d4d9`，门见下面 `yk_chase_target_resolve` 尾部的长注
 *（±64 px 窄带 + 链根）。我们此前那一支「没有 boss ⇒ 最近的敌人」是**本卡自定** ⇒ 已按一手收掉。
 * 现在：**只有 boss**（`mgr+0x48` 的 4 个 id 里可锁定的那只，与伤害转嫁共用 `yk_boss()`）；没有 boss
 * ⇒ `chase_have = 0`（= 一手「`g_BossEnemy == 0`」）⇒ 子机不追、定向不写、子机行照 `−π/2` 直上。
 * 目标只在本帧有效（`on_tick_2` 写进卡状态、帧界的 `on_option_pose_apply` 读）——**绝不缓存指针**。*/
static void yk_chase_target_resolve(yk_card_t *s)
{
    uint8_t *e;
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
    /* ★★★ 2026-09-27 第二十二批（owner 两次追问后的**订正**）：「**灵梦妖子机没有 boss 的时候也应当追踪，
     *   你一手代码肯定读错了**」—— owner 是对的，我上一版把 `g_EclExitLeftBound` 读成了「ECL 里的一个固定 x」，
     *   于是把门理解成「boss 附近的敌人」。**一手订正**（社区反编译 `local/vendor/th08_dec/src/`）：
     *
     *   · 门逐字（`EnemyManager.cpp` 的逐敌循环，二进制 `th08:0x42d481`–`0x42d4d9`）：
     *     `if (fabsf(enemy->movePos.x − g_EclExitLeftBound) < 64.0f && enemy->HasOwnerEnemy() == 0 &&
     *          (g_BossEnemy == NULL || g_BossEnemy->pos.y <= enemy->movePos.y)) g_BossEnemy = enemy;`
     *   · **`g_EclExitLeftBound`（`0x17d61ac`）就是「玩家 x」**，不是固定边界：同一个反编译里
     *     `AsciiManager.cpp:854` 原文注「距玩家（**g_EclExitLeftBound 即玩家 x**）越远 alpha 越高」、
     *     `:958` 把边界指示 VM 直接摆在 `pos.x = g_EclExitLeftBound`（= 玩家那一列）。
     *   · ⇒ 真语义 = **「无主的（链根）敌人里，横向离自机 64 px 以内的那一个，取 y 最大（= 离自机最近）的」**，
     *     **和 boss 无关** —— boss 只是「通常正好落在你头上那一列」而已。
     *   · 一个候选都没有 ⇒ `g_BossEnemy == 0` ⇒ 定向那一支（`th08:0x450240`）第一句
     *     `cmp DWORD PTR [eax+0xe2abc],0x0 / je 0x450310` 就跳过 ⇒ 子机行照 `−π/2` 直上；**追敌/旋转也停**。
     *
     * ⚠ 上一版那支「只有 boss」是**错的**（owner 实跑：「灵梦妖子机完全不索敌了」）⇒ 本版按上面那条订正的门重写。
     * ⚠ TH18 侧的两处对位（登记 `TUNING.md` 2.1ddd-1）：① 「链根」= `CE_SPAWN_PARENT(enemy+0x6834) == 0`
     *   （TH18 的 `parent_enemy_id`，§BE #114 用过同一格）；② TH08 的 y 是「屏幕向下为正」，
     *   取**最大 y** = 离自机最近的那一只（与一手同一套坐标）。*/
    if (CE_PLAYER()) {
        const float *pp = ce_player_pos();
        float best_y = 0.0f;
        uint32_t best_id = 0;
        float best_x = 0.0f;
        const float px = pp ? pp[0] : 0.0f;
        CE_ENEMY_FOREACH(CE_ENEMY_MGR(), e) {
            const float *ep;
            uint32_t id;
            if (!ce_enemy_lockable(e)) continue;
            if (*(const int32_t *)(e + CE_SPAWN_PARENT) != 0) continue;   /* 链根：不是别人生成的子敌人 */
            id = *(const uint32_t *)(e + CE_ENEMY_ID);
            if (!id) continue;
            if (yk_map_is_fam(&s_map, id)) continue;                       /* 我们自己的使魔不当目标 */
            ep = ce_enemy_pos(e);
            if (!ep) continue;
            if (ep[0] - px >= 64.0f || px - ep[0] >= 64.0f) continue;      /* 一手：|x − 玩家 x| < 64 */
            if (!best_id || ep[1] >= best_y) {                             /* 一手：y 最大者胜 */
                best_id = id; best_y = ep[1]; best_x = ep[0];
            }
        }
        if (best_id) { s->chase_have = 1; s->chase_id = best_id; s->chase_x = best_x; s->chase_y = best_y; }
    }
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
static uint32_t s_opt_released_vm[YK_OPTION_MAX];   /* ★ §BE #118：交出时点名删掉的 VM id */
static uint32_t s_opt_released_frame;              /* 交出那一帧（隔 30 帧再复查）*/

/* 交出子机 **30 帧后**回头看那几支 VM 是否真的被回收了（一手：`0x488cf0` 是标记删除，
 * 同帧一定还查得到 ⇒ 必须延迟判）。只在真没回收时打一行。*/
static void yk_opt_released_check(void)
{
    uint32_t f = ce_sdk_frame();
    int i;
    if (!s_opt_released_frame || f < s_opt_released_frame + 30u) return;
    for (i = 0; i < YK_OPTION_MAX; ++i) {
        if (!s_opt_released_vm[i]) continue;
        if (ce_anm_get_vm(s_opt_released_vm[i]))
            ce_log("youkai: option VM %08x still alive 30 frames after release (slot %d) - the drop did "
                   "not free it", (unsigned)s_opt_released_vm[i], i);
        s_opt_released_vm[i] = 0;
    }
    s_opt_released_frame = 0u;
}

/* ★ 2026-09-27 第二十六批：束改由卡侧画/结算 ⇒ 定义在下面（`yk_beam_tick` 那一段），
 * 这里先声明 —— `yk_option_release`（子机撤走 / 删卡 / 换关）要用它把在飞的束**硬删**掉。*/
static int yk_beam_kill(int fade);

static void yk_option_release(ce_card_t *c, yk_card_t *s)
{
    int i;
    uint32_t vmids[YK_OPTION_MAX];              /* ★ §BE #118：交出前逐个记下 VM id，交出后验尸 */
    for (i = 0; i < YK_OPTION_MAX; ++i) vmids[i] = 0;
    /* ★★ 2026-09-23（owner：「依旧子机滞留」）：**交出子机那一刻记一行** ——
     *   owner 那局日志里 `youkai: option allocated (ptr 183eb908, anm id …)` 反复出现、**ptr 恒定而 anm id
     *   每次都是新的** ⇒ 两种可能必须分开：① 我们在反复重申请同一格而**旧 VM 没被删**（= 滞留 VMs 越积越多）；
     *   ② 那些是**引擎原生**子机（被我们重建后停在原地）。这一行给的是「我们交出去了几格、哪些 VM 被点名删」，
     *   与下面那支「引擎原生 4 格」的探针合起来就能一眼分流。*/
    if (s) {
        int n = 0;
        uint32_t vm0 = 0, vm1 = 0, vm2 = 0, vm3 = 0;
        for (i = 0; i < YK_OPTION_MAX; ++i) {
            if (!s->opt[i].ptr) continue;
            uint32_t vm = *(const uint32_t *)(s->opt[i].ptr + CE_OPT_ANM_ID);
            vmids[i] = vm;                          /* ★ §BE #118：留到删完复查 */
            if (n == 0) vm0 = vm; else if (n == 1) vm1 = vm;
            else if (n == 2) vm2 = vm; else vm3 = vm;
            ++n;
        }
        if (n) ce_log("youkai: option release - %d slot(s) held, VMs to delete: %08x %08x %08x %08x "
                      "(if the same slot keeps coming back with a NEW anm id, the old VM did not die)",
                      n, (unsigned)vm0, (unsigned)vm1, (unsigned)vm2, (unsigned)vm3);
    }
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
    /* ★★★ 2026-09-27（§BE #118 → 同日订正）：**这一支原来是假阳性，已作废**。
     *   一手 `0x488cf0` 证明 `ce_anm_delete` 只是**标记删除**（置 `vm+0x538` bit7，随后由 anm 管理器回收）
     *   ⇒ 调用返回后 `ce_anm_get_vm(id)` 当然还查得到 ⇒ 「STILL ALIVE」这条判据不成立。
     *   （owner 那局日志里它报了 7 次，全是同一格 —— 那是**预期行为**，不是证据。）
     *   ⇒ 现在只把 id 记进 `s_opt_released_vm[]`，交给 `yk_opt_released_check()` 在**隔 30 帧**时复查：
     *     那时还活着才算「VM 真的没被回收」。*/
    if (s) {
        for (i = 0; i < YK_OPTION_MAX; ++i) {
            if (!vmids[i]) continue;
            s_opt_released_vm[i] = vmids[i];
            s_opt_released_frame = ce_sdk_frame() ? ce_sdk_frame() : 1u;
        }
    }
    /* 兜底：所有槽都不在我们手里、但 `card+0x1c` 还挂着 VM（引擎分配过而认领失败 / 池刚被重建）
     * ⇒ 补一次原生的单子机收尾；上面已经清过那一格时它是 no-op。
     *
     * ★★★ 2026-09-27（owner：「**灵梦从妖机体退回来，子机本体消失，发弹正常**」）：**这一支要收窄**。
     *   `ce_option_drop(c, 0)`（= `sdk.c` 的 `o == NULL` 支）是**无条件**删 `card+0x1c` 那个 VM
     *   （`sdk.c:665-667`），它假设那一格一定是我们自己的子机。但对**人侧**（松 Shift 之后）来说，
     *   引擎原生那 4 格子机（`player+0x670 + i*0xf0`，`CE_NATIVE_OPT_*`）的 VM 也可能正挂在
     *   `card+0x1c` 上（`Player__allocate_option` 每次分配都写它）—— 删掉之后：**子机的记录还在**
     *   （`in_use` 仍 1 ⇒ 行循环照发弹、膛口照镜像），但**画不出东西了** ⇒ 正是 owner 看到的那一幕。
     *   ⇒ 现在先**排除「这一格 VM 属于原生 4 格」**再删；命中就只打一行日志、不动手。*/
    {
        uint32_t *card_vm = (uint32_t *)((uint8_t *)c + CE_CARD_OPTION_ANM_ID);
        uint8_t *pl = CE_PLAYER();
        int native_uses = 0;
        if (*card_vm && pl) {
            int k;
            for (k = 0; k < CE_NATIVE_OPT_COUNT; ++k) {
                const uint8_t *ns = pl + CE_NATIVE_OPT_PLAYER_MUZ0 + (size_t)k * CE_NATIVE_OPT_STRIDE;
                if (*(const uint32_t *)(ns + CE_OPT_ANM_ID) == *card_vm) { native_uses = 1; break; }
            }
        }
        if (*card_vm && native_uses) {
            ce_log("youkai: option release fallback - card+0x1c VM %08x belongs to a NATIVE option slot "
                   "(human side) => NOT deleting it (that is the 'option body vanishes but keeps firing' "
                   "case); the field is left alone", (unsigned)*card_vm);
        } else {
            ce_option_drop(c, 0);
        }
    }
    s->opt_script = -1;
    s->opt_spring_live = 0;                /* ★ th08 #1：松手 ⇒ 下次再要子机时重新从自机位置起 */
    s->opt_age = 0;                        /* ★ 2026-09-22 晚：出场计时从 0 起（TH08 的 `Timer(&option+0x2e0)`）*/
    s->opt_chase = 0;                      /* 手里没子机 ⇒ 追踪态自然结束（旋转标签由新 VM 自己的脚本起手）*/
    s->opt_spin_vm = 0;                    /* ★ 2026-09-26：VM 没了 ⇒ 忘了「发给谁」*/
    yk_beam_kill(0);                       /* ★ 2026-09-27 第二十六批：子机撤走 ⇒ 束**硬删**
                                            *（膛口没了，束不能留在原地；`on_delete`/`on_run_reset`
                                            * 都走这一支 ⇒ 商店里删卡也收得到）*/
    /* ★ 2026-09-27（owner ②）：**松 Shift ⇒ 放置型的那四个魔法阵解除冻结** —— 下一次进妖侧
     * 要按「创建那一刻」重新定型（一手：TH08 整个 option 对象在 `th08:0x44b0ac` 一带被
     * `memset` 重建，state 回到 1 ⇒ 位置重新写一遍）。*/
    __builtin_memset(s->opt_frozen, 0, sizeof s->opt_frozen);
    __builtin_memset(s->opt_face, 0, sizeof s->opt_face);
    __builtin_memset(s->opt_orbit, 0, sizeof s->opt_orbit);
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
    /* ★★ 2026-09-25：**多出来的槽必须自己收掉**（`HANDOVER-108.md` §3 点名的缺口，此前只建不收）。
     *   现象（owner 2026-09-23 逐字）：「魔理沙用 C 放灵击之后，有一个我们改出来的子机就定在原地了…
     *   放灵击的几帧里面，三个子机都被定在原地，但是只有两个子机一段时间后被重建了，
     *   然后留下那个子机没有被回收」。
     *   根因：本函数只在 `i < want` 里申请，**从来没有回收 `i >= want` 的槽** —— 110 的 C 灵击扣火力后调
     *   `ce_repopulate_options()`（`th18:0x45d5e0`）⇒ 档数 3 → 2 ⇒ 下一帧 `want == 2` ⇒ 第 3 格的
     *   `ptr` 还在、它的 ANM VM 也没人删 ⇒ 那台子机**冻在最后被摆的位置**上再也不动（`ptr` 永不归还池）。
     *   一手对位（2026-09-25 本人在 `th18.exe` 上逐指令读的）：引擎自己就是「**按档重建 + 收掉多出来的**」成对做 ——
     *   ① 拆：`0x45d818 lea esi,[ebx+0xb4]` + `0x45d81e mov edi,4`（★ 恒 **4** 次，与 `lvl` 无关）⇒ 循环体
     *      `0x45d827 call 0x488be0` 删该格 VM、`0x45d82c mov [esi],0x0` 清字段，`0x45d832 lea esi,[esi+0xf0]` 下一格；
     *   ② 收**多出来的那几格**：`0x45dbe0 mov edi,4` + `0x45dbeb sub edi,edx`（`edx` = 档数）⇒ 循环 **`4 − lvl` 次**，
     *      每次删 `+0xb0` 那支 VM（`0x45dbf2 push [esi]` + `0x45dc05 call 0x488be0`）并清 `+0x00` 状态
     *      （`0x45dbf4 mov [esi-0xb0],0x0`）+ `+0xdc`（`0x45dc00`）；
     *   ③ 重建只建 `i < lvl` 的格。
     *   ⇒ 档数掉下去时，**引擎会自己收掉多出来的格**；卡侧自己申请子机时必须补齐同一半。
     *   形状照 116 `cards/season.c:alloc_options()` 与 111 `cards/signal.c:signal_option_align()`（两条都已实跑）。
     *   `ce_option_drop()` 删 VM（按**该槽自己**的 `option+0xb0`）**并真的松开池槽**（`option+0x00 = 0`）——
     *   只写 `o->ptr = 0` 是**漏槽**（12 格用完后新子机再也申请不到，日志现 `option allocated FAILED (pool full)`）。
     *   ⚠ 这一支**每帧都会跑**（本函数被声明层每帧调一次），所以档数掉下去的那一帧就收干净，
     *     不需要 116 那种「每帧兜底再收一次」的第二条路。*/
    for (i = want; i < YK_OPTION_MAX; ++i) {
        if (!s->opt[i].ptr) { s->opt[i].retry = 0; continue; }
        ce_option_drop(c, &s->opt[i]);                  /* 删 VM + 收激光 + 真正松开池槽 */
        s->opt[i].ptr = 0;
        s->opt[i].retry = 0;
        /* 弹簧状态也清掉：这一格下次被重新申请时按「新槽」从**自机位置**起步（TH08 state 1），
         * 而不是从这台子机冻住的旧坐标飞出来。*/
        s->opt_pos[i][0] = 0.0f; s->opt_pos[i][1] = 0.0f;
        s->opt_vel[i][0] = 0.0f; s->opt_vel[i][1] = 0.0f;
        s->opt_frozen[i] = 0;              /* ★ 咲夜那支：这一格下次重申请要重新定型 */
        s->opt_face[i] = 0.0f;
        s->opt_orbit[i][0] = 0.0f; s->opt_orbit[i][1] = 0.0f;
        ce_log("youkai: option #%d reclaimed (only %d wanted now; frame %u) - "
               "the slot is handed back to the pool, its VM deleted",
               i, want, (unsigned)ce_sdk_frame());
    }
    for (i = 0; i < want; ++i) {
        const int was_held = (s->opt[i].ptr != 0);      /* 本帧是「认领旧槽」还是「新申请」 */
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
                /* ★★★ 2026-09-26（owner：「灵梦的妖子机**应当旋转**」报了多次 —— 真因在这一格）：
                 *   **判据不能只看 `opt_chase` 这个边沿，必须盯 VM 身份**。引擎会在火力变化等时机
                 *   把这一格的 VM **删掉重建**（实跑日志原文：**同一个 `ptr`** 配**不同**的 anm id ——
                 *   `youkai: option allocated (ptr 22bbcfc0, anm id 15f0ff8f)` → 后来 `18397e18` →
                 *   `1ed7fe69`），而重建出来的 VM 从脚本第 0 条开始 = **待机循环**。
                 *   我们的旋转/待机只在**边沿**发一次（`opt_chase` 0→1 / 1→0）⇒ VM 一换，
                 *   旋转就再也没人发 ⇒ 子机在敌人面前**不转**（观感 = 「说了多少次了」）。
                 *   修法：记下 `opt_spin_vm`（上一次把标签发给哪一支 VM），**只要当前 VM 不是它**，
                 *   就按当前应有的状态补发一次（幂等：同一支 VM 每帧只发一次、只发一次标签）。*/
                if (vm && vm != s->opt_spin_vm) {
                    const int resend = (s->opt_spin_vm != 0);
                    ce_anm_interrupt(vm, want_chase ? YK_OPT_SPIN_LABEL : YK_OPT_IDLE_LABEL);
                    s->opt_spin_vm = vm;
                    if (resend && !s->logged_spin_resend) {
                        s->logged_spin_resend = 1;
                        ce_log("youkai: youkai option VM was REBUILT (same slot, new anm id %08x) - "
                               "re-sent label %d (spin %d / idle %d) so the rotation is not lost; "
                               "this is why the option used to sit still",
                               (unsigned)vm, want_chase ? (int)YK_OPT_SPIN_LABEL : (int)YK_OPT_IDLE_LABEL,
                               (int)YK_OPT_SPIN_LABEL, (int)YK_OPT_IDLE_LABEL);
                    }
                }
                /* ★★ 2026-09-26：**追踪态里周期性重申旋转标签**。为什么需要（一手）：引擎有一次性广播
                 *   `th18:0x416d50`（给原生 4 格 + **卡池 12 格**逐格发 `interrupt 2`；调用点 3 处：
                 *   复活/恢复 `0x45ad3c`·`0x4179e7`、贴墙子状态结束 `0x45b2af`）⇒ 我们的 VM 会被打回
                 *   标签 2（`rotate(0,0,0)+angleVel(0,0,0)`）而 **VM id 不变** ⇒ 上面那条「盯身份」的补发
                 *   救不了（旋转从此永久丢失）。重申是**幂等**的：标签 5 段里**没有 `rotate()`**，
                 *   只有 `sprite(S30)` + `angleVel(0,0,0.62831855)` + `stop()` ⇒ 重进不会让相位跳变
                 *   （每 32 帧一次，肉眼无感；也顺手覆盖「别的原因把标签顶掉」这类没查到的路径）。*/
                if (want_chase && vm && (ce_sdk_frame() % 32u) == 0u)
                    ce_anm_interrupt(vm, YK_OPT_SPIN_LABEL);
                if (want_chase && !s->opt_chase) {
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
            if (!s->opt_spring_live || !was_held) {   /* TH08 state 1：从**自机位置**出发
                                                       * （`!was_held` = 本帧新申请的这一格 —— 见上面
                                                       * 「多出来的槽必须自己收掉」那段：收掉再重建的槽
                                                       * 不能从停住的旧坐标起步）*/
                s->opt_pos[i][0] = pp[0];
                s->opt_pos[i][1] = pp[1];
                s->opt_vel[i][0] = 0.0f;
                s->opt_vel[i][1] = 0.0f;
            }
            yk_opt_spring_step(s->opt_pos[i], s->opt_vel[i], goal);
            fx = s->opt_pos[i][0] - pp[0];
            fy = s->opt_pos[i][1] - pp[1];
        } else if (yk_option_uses_freeze(character) && pp) {
            /* ★★★ 2026-09-27（owner ①②）：「**原版咲夜妖自机会固定在创建它的位置，直到松开 shift**」
             *   + 「原版咲夜妖自机的**四个小魔法阵是在动的，发弹点不动，只动 vm**」。
             *
             * 一手（`th08:0x44eb70`，逐指令；全文见 `youkai_core.h` 的同一段）：
             *   · state 1（出生）：`position = 自机位置 + 本机体阵型` + 朝向写 0/π；
             *   · **state 2（活动）整段不碰位置**（`0x44eb92 je 0x44ed03`）⇒ 停在创建那一刻的绝对坐标；
             *   · 共尾：满 12 帧后 `face = norm(face + step)`，再把 VM 的旋转推向 face
             *     ⇒ 转的是**贴图**（那张魔法阵），膛口（`+0x2b0`）一动不动 = 「只动 vm」。
             *
             * TH18 侧：引擎每帧算 `pos = 自机 + 偏移`（`th18:0x45bce8`）⇒ 我们把**冻结的绝对坐标
             * 减自机坐标**当偏移写下去，位置就冻住了（和机体 0 的弹簧同一个手法）；旋转直接写 VM。*/
            uint32_t vm;
            if (!was_held || !s->opt_frozen[i]) {          /* TH08 state 1：只在这一帧定型 */
                if (!yk_option_home_from_player(character, i, pp, s->opt_pos[i])) {
                    s->opt_pos[i][0] = pp[0];
                    s->opt_pos[i][1] = pp[1];
                }
                yk_option_freeze_face0(i, &s->opt_face[i]);
                /* 出生位形照一手：偏移 = `(cos(face0), sin(face0)) × r` ⇒ face0 = 0 的格子起在 **+x**、
                 * face0 = π 的起在 **−x** ⇒ 左右两边的格子从一开始就**镜像对称** ✓（owner 的口径）。*/
                s->opt_orbit[i][0] = (s->opt_face[i] == 0.0f) ? 1.0f : -1.0f;
                s->opt_orbit[i][1] = 0.0f;
                s->opt_frozen[i] = 1;
            }
            fx = s->opt_pos[i][0] - pp[0];
            fy = s->opt_pos[i][1] - pp[1];
            /* ★★★ 2026-09-27（owner ③④：「**在切换移动方向的时候，咲夜妖子机会动一帧**」）：
             * 光写偏移**不够** —— 引擎每帧自己算 `pos = 自机位 + 偏移`（`th18:0x45bce8`），
             * 而我们的 `pp`（`ce_player_pos()`）与引擎那一刻读到的自机位**可能差一帧** ⇒ 冻结的绝对坐标
             * 会被 `(pp_引擎 − pp_我们)` 顶一下；**换向那一帧**这个差最大（速度反向）⇒「动一帧」。
             * ⇒ 再把**这一格自己的位置字段**也写死（`CE_OPT_POS_X/Y`，定点 1/128），谁后写谁赢
             * ⇒ 观感上钉住（槽 1..4 的膛口镜像本来就是这个手法，见下面「原生膛口镜像」那段）。*/
            *(int32_t *)(o + CE_OPT_POS_X) = (int32_t)(s->opt_pos[i][0] * YK_SUBPIXEL);
            *(int32_t *)(o + CE_OPT_POS_Y) = (int32_t)(s->opt_pos[i][1] * YK_SUBPIXEL);
            vm = *(const uint32_t *)(o + CE_OPT_ANM_ID);
            if (vm) {
                /* ★★★ 2026-09-27（owner：「子机魔法阵不是说不转，而是说它 **4 个小魔法阵得移动绕转**」）：
                 * 一手的 `face` 是**公转相位**不是自转角（见 `youkai_core.h` 的 `YK_OPT_ORBIT_R` 段：
                 * `0x4286e0(&+0x2a4, face, 8.0)` = 极坐标 → 向量、`0x410a70` 再用它算 VM 的位置）
                 * ⇒ 每帧把单位向量按本格的增量转一点，VM 就绕**冻结锚点**画半径 8 px 的圆；
                 *   锚点（= 发弹点、`CE_OPT_POS_X/Y`）一个字节都不动 ✓。*/
                float c, sn;
                if (s->opt_age >= YK_OPT_FREEZE_SPIN_START) {   /* `th08:0x44ed03 push 0xc` */
                    float step = 0.0f;
                    float nx;
                    yk_option_freeze_spin(i, &step);               /* 跳表 `th08:0x44ee5b` 的符号与档 */
                    /* ★★★ 2026-09-27 第二十五批（owner：「**相位不对，它两边的魔法阵是对称的**」）：
                     * **订正上一版的 `fsincos` 次序** —— x86 的 `fsincos` 是 **cos 落在 ST(0)、sin 落在 ST(1)**
                     * ⇒ `0x4286e0` 的两条 `fstp` 存出来是 `(cos(face), sin(face)) × r`（不是我上一版写的
                     * `(sin, cos)`）。用 `(cos, sin)` 时，`face0 = 0 / π` 的两格偏移互为**相反数**
                     * ⇒ 左右两边的格子**恒为镜像**（owner 看到的就是这个对称）；上一版那样写会把对称破掉。
                     * ⇒ 方向也回到标准旋转矩阵（+x → +y，与 face 增大同向）。*/
                    if (step > 0.030f)        { c = YK_OPT_ORBIT_COS_INNER; sn = YK_OPT_ORBIT_SIN_INNER; }
                    else if (step > 0.0f)     { c = YK_OPT_ORBIT_COS_OUTER; sn = YK_OPT_ORBIT_SIN_OUTER; }
                    else if (step < -0.030f)  { c = YK_OPT_ORBIT_COS_INNER; sn = -YK_OPT_ORBIT_SIN_INNER; }
                    else if (step < 0.0f)     { c = YK_OPT_ORBIT_COS_OUTER; sn = -YK_OPT_ORBIT_SIN_OUTER; }
                    else                      { c = 1.0f; sn = 0.0f; }
                    nx = s->opt_orbit[i][0] * c - s->opt_orbit[i][1] * sn;
                    s->opt_orbit[i][1] = s->opt_orbit[i][0] * sn + s->opt_orbit[i][1] * c;
                    s->opt_orbit[i][0] = nx;
                    yk_opt_angle_norm(s->opt_face[i], step, &s->opt_face[i]);
                }
                ce_anm_set_pos(vm,
                               s->opt_pos[i][0] + YK_OPT_ORBIT_R * s->opt_orbit[i][0],
                               s->opt_pos[i][1] + YK_OPT_ORBIT_R * s->opt_orbit[i][1], 0.0f);
            }
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
    /* ★★★ 2026-09-26（owner 实跑：「妖侧子机不发弹 / 只有一行 / 形式不对」）：**原生膛口镜像** ----
     *
     * **谁在什么时候用这几格**：妖侧弹由**引擎的行循环**发（本卡 `on_shot_set_index` 认领 set，
     * `th18:0x45ebfb`–`0x45ec9f`），行里的 `+0x20` 选膛口（一手 `th18:0x45ec41 mov al,[edi+0x20]` →
     * `0` = 自机本体；非 0 ⇒ `0x45ec4f and eax,0xf` → `imul 0xf0` → `+0x580` → `+ esi`(= player)）。
     * 我们的行照抄 TH08 的 `rec[+0x20]`（`assets/sht/append_shooterset.py` 的 `YOUKAI_TIER_ORIGINS`）
     * ⇒ **非 0 的那些行读的是原生槽 1..4**（`player+0x670 + (n-1)*0xf0`）。
     *
     * **为什么必须由我们喂**（两条，都是这一轮实跑/一手定案的）：
     *   ① 原生那 4 格的**记录数只有 `lvl` = `CURRENT_POWER/100` 格**（`repopulate` 只建 `i < lvl`），
     *      档 1 时槽 2/3/4 是 `state 0`、膛口停在 `(0,-400)`（实跑日志原文：`native opt slot 2/3/4:
     *      state 0 vm 00000000 muzzle (0.0, -400.0)`）⇒ 那些行**发到屏幕外** = owner 的「子机不发弹」
     *      （咲夜档 0/1 的行正是原生槽 2/3 ⇒ 整列消失；魔理沙/灵梦档 1 的行只有槽 1 活着 ⇒
     *      弹从**引擎那台看不见的子机**（原生阵型、贴着自机）出来 ⇒ owner 的「只有一行」）。
     *   ② 就算记录活着，它的位置也是**原生阵型**（`sht[0x90 + k*8]`，贴着自机），
     *      而不是 TH08 的子机位（我们那台**看得见的**子机所在处）。
     * ⇒ 卡侧每帧把这 4 格的**聚焦偏移与膛口**都写成我们那台子机的当前坐标：
     *    · 偏移（`+0x6c/+0x70`）⇒ 引擎那支跟随更新（`0x45bce8`–`0x45bd08`）算出的 target 与我们一致；
     *    · 膛口（`+0x5c/+0x60`）⇒ **直接写死**。为什么不能只写偏移：`state == 0` 的格子被跟随更新
     *      **整格跳过**（一手 `th18:0x45bcc0 cmp DWORD PTR [esi-0x60],0x0; je`），而那几格正是
     *      低档要用的 ⇒ 只写偏移的话它们的膛口永远停在 `(0,-400)`。
     * ⚠ 只写**聚焦**那一对（`+0x6c/+0x70`）：高速（人侧）用的是非聚焦对（`+0x64/+0x68`），
     *   一个字节都不碰 ⇒ 「人侧和不装 th08 完全一样」不受影响；下次火力变化时 `repopulate` 会把
     *   两份都按 `pl0X.sht` 重写，我们下一帧再镜像一次即可（幂等）。
     * ⚠ 机位数不够时（例：早苗档 1 只有 1 台，而她的回落组有原点 2 的行）**沿用最后一台的坐标**
     *   ⇒ 那些行与已有的那台同点出膛，而不是从 `(0,-400)` 或屏幕外冒出。*/
    {
        uint8_t *pl = CE_PLAYER();
        if (pl) {
            const int have = (want > 0) ? want : 0;
            int32_t mx = 0, my = 0;                     /* 最后一台的**绝对**膛口（定点）*/
            for (i = 0; i < CE_NATIVE_OPT_COUNT; ++i) {
                uint8_t *ns;
                int cell = (i < have) ? i : (have > 0 ? have - 1 : -1);
                if (cell < 0) break;                     /* 手里一台都没有 ⇒ 这几格不碰 */
                ns = pl + CE_NATIVE_OPT_PLAYER_MUZ0 + (size_t)i * CE_NATIVE_OPT_STRIDE;
                if (i < have) {
                    /* 这一格有对应的子机 ⇒ 偏移照抄我们刚写进它自己那对字段的值（引擎算出的 target
                     * 因此与我们一致），膛口优先取**引擎已经 lerp 过的当前位置**（`+0x5c/+0x60`）
                     * —— 弹从「看得见的那台子机**此刻**在哪」出膛，而不是从它的目标点提前冒出。*/
                    uint8_t *o = s->opt[i].ptr;
                    if (!o) continue;
                    *(int32_t *)(ns + CE_OPT_OFF_F_X) = *(const int32_t *)(o + CE_OPT_OFF_F_X);
                    *(int32_t *)(ns + CE_OPT_OFF_F_Y) = *(const int32_t *)(o + CE_OPT_OFF_F_Y);
                    mx = *(const int32_t *)(o + CE_OPT_POS_X);
                    my = *(const int32_t *)(o + CE_OPT_POS_Y);
                    if (!mx && !my) {                     /* 刚申请、引擎还没算过 ⇒ 用目标点兜底 */
                        mx = (int32_t)((pp ? pp[0] : 0.0f) * YK_SUBPIXEL)
                           + *(const int32_t *)(o + CE_OPT_OFF_F_X);
                        my = (int32_t)((pp ? pp[1] : 0.0f) * YK_SUBPIXEL)
                           + *(const int32_t *)(o + CE_OPT_OFF_F_Y);
                    }
                }
                /* 多余的原生格（没有对应子机）⇒ 与最后一台同点（见上 ⚠）*/
                *(int32_t *)(ns + CE_OPT_POS_X) = mx;
                *(int32_t *)(ns + CE_OPT_POS_Y) = my;
            }
        }
    }
    /* ★★★ 2026-09-27（owner ⑤：「**灵梦妖子机的发弹方向实际上不是固定竖直朝上的，应该是向敌人的方向**」）：
     * 把「朝敌的角」写进 TH18 的**原生瞄准格** `player+0x479cc`（`CE_PLAYER_AIM_ANGLE`）。
     *
     * 一手（`th08:0x450240` 逐指令，= `g_PlayerShotTable1[1]`；只有**子机行**的 `+0x28 = 1` 会走它）：
     *   · 目标 = `g_BossEnemy`（`th08:0x18b89b4`，写点 `th08:0x42d4d9`）—— 为 0 时**整段跳过**（行原样发）；
     *   · 否则 `angle = wrap(atan2(boss.y − bullet.y, boss.x − bullet.x) + row.angle + π/2)`
     *     （`ds:0x4b4468` = +π/2）、`speed = row.speed × 1.5`（`ds:0x4b4438`）⇒ 子机行对着 boss 飞。
     *   ⇒ 本卡：**每帧**把「从我们这台子机指向目标」的角写到那一格（原生的 `init5` `th18:0x4612d0`
     *     在**发射那一刻**读它 ⇒ 效果 = 发射瞬间朝敌，不是每帧追踪 ✓ 与一手同形）。
     *   · 我们的行角度基准就是 −π/2（竖直向上）= 一手里 `row.angle + π/2` 的抵消项 ⇒ 直接写 `atan2`
     *     （那两个 ±2° 散角行在 init5 下会与其它子机行同向，登记 `TUNING.md` ❓）。
     *   · 目标来源沿用本卡既有的解析（先 boss、否则最近的敌人 —— `yk_chase_target_resolve`，
     *     就是 TH08 `g_BossEnemy` 那一格的 ❓ 对位）；**没有目标 ⇒ 写 −π/2**（= 一手「行原样」）。
     *   · 只对**机体 0（霊夢・紫）**写：一手只有 `ply00as` 的子机行挂了那支回调（魔理沙是 init4/tick5、
     *     咲夜是 init7，都不会朝敌定向）。*/
    if (character == 0 && pp) {
        float ax = pp[0], ay = pp[1] + YK_OPT_HOME_Y_REIMU;   /* 子机名义位置（没积分时用它）*/
        float ang = -YK_ATAN_HALF_PI;                              /* 一手：目标为 0 ⇒ 行原样（竖直向上）*/
        if (s->opt_spring_live && (s->opt_pos[0][0] != 0.0f || s->opt_pos[0][1] != 0.0f)) {
            ax = s->opt_pos[0][0]; ay = s->opt_pos[0][1];     /* 弹簧积分过的**绝对**位置（更接近一手）*/
        }
        if (s->chase_have) ang = yk_atan2f(s->chase_y - ay, s->chase_x - ax);
        *(float *)(CE_PLAYER() + CE_PLAYER_AIM_ANGLE) = ang;
        if (!s->logged_aim) {
            s->logged_aim = 1;
            ce_log("youkai: aim angle written to PLAYER+0x%x = %.4f rad (target %s, TH08 "
                   "`th08:0x450240` = option rows aim at `g_BossEnemy` at FIRE time; body rows stay -pi/2)",
                   (unsigned)CE_PLAYER_AIM_ANGLE, (double)ang,
                   s->chase_have ? "found" : "none => straight up");
        }
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
 * ★ **早苗（角色 3）现在是 `A0..A4`（2026-09-26 第四批补上；本节旧文说「她不在这些格里、
 *   恒回落 0x16」已过期）**：她的妖侧 = TH16 的射命丸文（秋），行数据取自 `local/th16.v1.00a`
 *   的 `pl03sub.sht` set 1..4 ⇒ `A1..A4`；**`A0` 是空组**（她档 0 = 0 台子机 ⇒ 认领闸不放行，
 *   由原生弹照发 —— 与「空组回落到 0x16」不是一回事：回落只在**越界**或**该格没数据却仍要发弹**
 *   时兜底）。`0x16`（满 P 的 `ply03as` = TH08 妖夢那一组）仍留着当最后一层兜底，不再是她的常规路径。*/
static const yk_sets_t s_yk_sets = {
    YK_SHOT_SET_YOUKAI,            /* 0x16：**最后一层兜底**（生成物没到 / 逐档组是空组时用）*/
    CE_SHT_SET_YOUKAI_R0,          /* 20 格的第一格（逐档组）*/
    YK_YOUKAI_TIER_SETS            /* = 4 角色 × YK_SHOT_TIERS */
};
/* 槽号布局是**生成物**给的（`sht_ids.h`）⇒ 用编译期断言把「下标 = 槽号」钉死：
 * 卡侧按 `first + 角色*档数 + 档` 算，生成序列一旦重排（插在中间）这里当场编译失败，
 * 而不是静默拿错组（照 111/117 那两处 `_Static_assert` 的先例）。*/
_Static_assert(CE_SHT_SET_YOUKAI_M0 == CE_SHT_SET_YOUKAI_R0 + YK_SHOT_TIERS,
               "youkai tier sets must be laid out R0..R4, M0..M4, S0..S4 in this exact order");
_Static_assert(CE_SHT_SET_YOUKAI_S0 == CE_SHT_SET_YOUKAI_M0 + YK_SHOT_TIERS,
               "youkai tier sets must be laid out R0..R4, M0..M4, S0..S4 in this exact order");
_Static_assert(CE_SHT_SET_YOUKAI_A0 == CE_SHT_SET_YOUKAI_S0 + YK_SHOT_TIERS,
               "youkai tier sets must be laid out R0..R4, M0..M4, S0..S4, A0..A4 in this exact order");
_Static_assert(YK_YOUKAI_TIER_SETS == 4 * YK_SHOT_TIERS,
               "YK_YOUKAI_TIER_SETS must cover 4 characters x YK_SHOT_TIERS tiers");

/* 本帧该用哪一格。**只读表，不做运行期探测**（探测在 `yk_youkai_shotset_installed` 里）。
 * 四个角色一律走逐档组：0/1/2 = TH08 的三份 `plyNNas.sht`，**3 = 早苗 = TH16 `pl03sub`（秋/文）**
 * （2026-09-26 补上；此前她恒回落到 `0x16` = TH08 妖夢那一组 —— 那就是 owner 报的
 * 「早苗自机发 5 条扇形、子机发一条」）。只有**越界**（不该发生）与「那一格是空组」才回落。*/
static int yk_shot_set_for(int character, int tier)
{
    if (character < 0 || character >= YK_OPTION_CHARS) return YK_SHOT_SET_YOUKAI;  /* 越界 ⇒ 回落格 */
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
/* 字形 sprite 基号按机体查表（0..9 = '0'..'9'、10 = '%'、11 = '.'、12 = '-'、**13 = '+'** —— 第 14 格是 #27 追加的）*/
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
 *   · **「刚开始就满速」**（★ 出处**已订正**，行为不变）：当年据 `th08:0x431650` 一族
 *     `spawn_item(type 7, state 1)` 认定「擦弹那一路生 state 1」，**那一族的出处认错了** ——
 *     `th08:0x431650`/`:0x431668` 是 `BulletManager::on_tick` 里「判定返回 **`eax == 2`**」那一支
 *     （`th08:0x43160f cmp eax,2 / jne`；2 = `th08:0x44a4a1 call 0x449ff0`
 *     `Player::check_bomb_cancel_areas` 命中**自机的炸弹消弹区** ⇒ `th08:0x44a4aa mov eax,2`），
 *     按 `ds:0x18b8988`（== 9 ⇒ 2 颗，否则 1 颗）出 `type 7, state 1`，
 *     **与擦弹无关**。擦弹（判定 `eax == 1`）在 `th08:0x431603` 只置 `bullet+0xdbd` 闩；
 *     出货在 `Player::sub_44a930` 的 `th08:0x44aac9`/`:0x44aaef`/`:0x44ab0f`（`type 0xa, state 1`
 *     ⇒ 被 `th08:0x440124` 改写成 **type 7 + state 5**）。
 *     ⇒ 擦弹那一路**不是**「生成即满速」，我们按 state 5 生（= `YK_ORB_STATE_SPAWN5`：先上抛、**且可收**）。
 *     「击破敌机」那一路一手是 state 3（`th08:0x42b048`）⇒ 与之同形，保持不变。
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
/* ★★★ 2026-09-22 夜（一手复核订正）：**两个不同的常量，别混** ——
 *   · **每帧**路径（`ItemManager::on_tick`，玩家非存活时把 state 1/3 打回下落）用 **−0.7**
 *     （`th08:0x4408e0` / `:0x440711`，float 位型 `0xbf333333`）；
 *   · **损命当帧的批量收尾**（`ItemManager::sub_441530`）用 **−0.9**（`th08:0x441574`，位型 `0xbf666666`）。
 *   我们原来只有一个 `YK_ORB_VY_DEAD = −0.9f` 被两条路共用 ⇒ 本批拆成两个。*/
#define YK_ORB_VY_DEAD       (-0.9f) /* 损命当帧的批量值（`th08:0x441574` 0xbf666666）*/
#define YK_ORB_VY_DEAD_FRAME (-0.7f) /* 每帧路径的值（`th08:0x4408e0` 0xbf333333）*/

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
static uint8_t  s_orb_dying_edge;           /* 「损命当帧」的边沿闩（`§BE #112`，见 `yk_orbs_tick`）*/

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
 *   · `YK_ORB_STATE_HOMING`(1) = **消弹 / `cancel_autocollect`** 那一支
 *     （`th08:0x4308e5` 一族 + `ItemManager::cancel_autocollect th08:0x4413e0` ⇒ 生成即满速归航）；
 *   · `YK_ORB_STATE_SPAWN`(3) = **击破敌机 / 符卡螺旋 / 擦弹**那几路（`th08:0x42b048` /
 *     `th08:0x41761d` / `th08:0x44aac9` 一族 ⇒ 先上浮、`vy > 0` 再归航）。
 *     ⚠ 擦弹那一路的一手出处是 **`th08:0x44aac9` 一族**（`Player::sub_44a930`），**不是**
 *     `th08:0x431650`（那是炸弹消弹区那一支，见 `YK_ORB_ANM_LAYER` 前的注）。*/
/* ★★★ 2026-09-22（#25 / 一手）：**三条来源 × state × 何时可被收**（逐站点读 push，全文
 * `engine/item/th08/01-time-orbs.md` §5.4）。这张表就是 owner 那三句话的编码：
 *
 * | 来源 | 出生态（我们） | 上抛/「无敌」 | 何时可被收 |
 * | --- | --- | --- | --- |
 * | 通用击破（`th08:0x42b03b push 3`）| `YK_ORB_STATE_SPAWN`(**不可收**) | **≈40–44 帧**（`vy = −2.0 − rand(0.2)`、`+0.05/帧`）| `vy > 0` 转归航之后 |
 * | **極端人/妖那一颗**（`0x42da41 push 1`）| `YK_ORB_STATE_HOMING` | **0 帧** | **生成当帧**（若已在抓取盒内）|
 * | 击破符卡螺旋 / 擦弹（type 0xa → type 7）| `YK_ORB_STATE_SPAWN5`(**可收**) | ≈40–44 帧 | **上抛期间也能收** |
 * | 消弹 / `cancel_autocollect` | `YK_ORB_STATE_HOMING` | 0 帧 | 生成当帧 |
 *
 * ⚠ **「无敌」的正确含义（别写成无敌帧）**：**只有 state 3** 被收集分派排除
 *   （`th08:0x4409f2`–`0x4409ff`：`movsx ecx,[item+0x2d7]; cmp ecx,3; je 0x440c37` —— **只比 3**）——
 *   上抛期间**吸收判定根本不会跑**，但道具本身仍是普通道具（会掉出下沿被丢失线吃掉、也会被清屏）。
 * ★★★ 2026-09-27 订正（§BE #111）：**state 5 与 state 3 不是同一档** —— 两者运动同形（都上抛、
 *   `vy > 0` 转归航），但 **state 5 在上抛期间就能被收**（上面那条 `cmp ecx,3` 只排除 3）。
 *   本卡原来把两者并成一个 `YK_ORB_STATE_SPAWN` ⇒ **螺旋 / 擦弹那两路白丢了一段可收时间**。
 *   现在分开：击破批 = `YK_ORB_STATE_SPAWN`(3，不可收)、螺旋/擦弹 = `YK_ORB_STATE_SPAWN5`(5，可收)。*/
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
        /* ★ 2026-09-23（#42）：**出生即记**（前 5 颗）—— 上一版只在「60 秒采样点那一刻有活着的永点」才打，
         *   而永点常在几帧内就被收走 ⇒ 一次实跑 0 行、拿不到证据（实测 owner 15:17 那局的日志就是 0 行）。
         *   出生即记保证必有一手证据：VM 在不在、sprite/alpha/layer/draw 是什么，一眼可判。*/
        if (id && s_orb_vm_logged < 5u) {
            ++s_orb_vm_logged;
            s_orb_log_pending = id;                    /* 下一帧的 tick 里打（VM 已建好、字段已写）*/
        }
    }
    if (!id) {                                  /* VM 建不出来 ⇒ 这颗不要（不崩、不排队）*/
        if (s) ++s->orb_full;
        return;
    }
    o->anm_id = id;
    o->used = 1;
    /* 三种出生态：1 = 生成即满速归航、3 = 上抛**不可收**（击破批）、5 = 上抛**可收**（螺旋/擦弹）。*/
    o->state = (uint8_t)((state == YK_ORB_STATE_HOMING || state == YK_ORB_STATE_SPAWN5)
                         ? state : YK_ORB_STATE_SPAWN);
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

/* ★★★ 2026-09-22 夜（P-C 订正）：TH08 `shotActive`（`player+0xfdc`）的对应读数。
 * 一手订正：它**不是「开火」**，而是「**正在放符卡/炸弹**」——`th08:0x44be0f` 用它跳过开火推进支，
 *   而放符卡期间只有 `±26/帧` 那一项在动（`th08:0x44c820` 一族，`shotInterval = 999`）。
 * TH18 的对应物 = `[bomb_mgr+0x30]`（`engine.h`：`do_bomb` 置 1、`Bomb__can_bomb_and_deathbomb_check
 *   th18:0x420420` 查它）⇒ 两个 0x43c0bb 闸（击破 ±200 / 收刻符 ±111）与推进支都用它。*/
static int yk_bomb_active(void)
{
    uint8_t *m = CE_BOMB_MGR();
    return m && *(const int32_t *)(m + CE_BOMB_MGR_ACTIVE) != 0;
}

/* ============ ★★★ 2026-09-22 夜（owner #27）：**刻符的两支弹字**（+N 提示）====================
 * 一手与两支的分工见 `youkai_core.h` 的同名小节（`th08:0x403460` / `th08:0x403330`）。
 * ⚠ **我们卡里没有 TH08 op90-92 那种「魔使」**：一手弹「+1」的是「击破魔使」那一刻
 *   （`th08:0x42b296`，紧跟掉 1 颗刻符）⇒ 我们的对应物 = **击破我们自己的使魔**（`yk_fam_*` 那套），
 *   所以「+1」挂在**使魔被击破**那一刻（依据写在这里，免得后来人以为是随手挂的）。
 * 贴图 = `pl0X.anm` 里已有的字形（`CE_PL_ANM_YKGAUGEFONT_BASE_PLxx`：0..9 = '0'..'9'、
 *   10 = '%'、11 = '.'、12 = '-'、**13 = '+'**（本批追加的第 14 格，TH08 sprite149））；
 *   VM 必须挂**自机那一页**（`yk_player_anm()`，贴图按 VM 自己那一页取）。
 * ❓ 自定（owner 2026-09-22 已批，登记 `TUNING.md`）：**层 = 世界层 11**、**池 = 3 支 x 5 字形**。*/
#define YK_POPUP_MAX        3
#define YK_POPUP_GLYPHS     5
#define YK_POPUP_ANM_LAYER  11

typedef struct {
    uint8_t  used;
    uint8_t  kind;
    uint8_t  n;
    uint8_t  age;
    float    x, y;
    uint32_t vm[YK_POPUP_GLYPHS];
    uint8_t  g[YK_POPUP_GLYPHS];
} yk_popup_t;
static yk_popup_t s_popup[YK_POPUP_MAX];
static unsigned s_popup_dropped;
static uint32_t s_hud_vm_log_tick;   /* ★ 2026-09-23（#30）：逐 VM 实况行的节拍 */
static uint8_t s_natopt_probe_done;   /* ★ 上一条诊断：每进一次妖侧打一行（离开时复位）*/
static uint8_t  s_aura_edge_spawn;   /* ★ 2026-09-23（#32/#29）：光环**每个形态边沿只起一次** */   /* ★ 2026-09-23（#30）：逐 VM 实况行的节拍 */

static void yk_popup_kill(yk_popup_t *p)
{
    unsigned i;
    if (!p) return;
    for (i = 0; i < YK_POPUP_GLYPHS; ++i) {
        if (p->vm[i]) { ce_anm_delete(p->vm[i]); p->vm[i] = 0; }
    }
    p->used = 0; p->n = 0; p->age = 0;
}

static void yk_popup_kill_all(void)
{
    unsigned i;
    for (i = 0; i < YK_POPUP_MAX; ++i) yk_popup_kill(&s_popup[i]);
}

static void yk_popup_spawn(float x, float y, int32_t value, int kind)
{
    yk_popup_t *p = 0;
    uint8_t dg[YK_POPUP_GLYPHS];
    int n, i, ch = yk_current_character();
    unsigned k;
    void *anm;
    for (k = 0; k < YK_POPUP_MAX; ++k) if (!s_popup[k].used) { p = &s_popup[k]; break; }
    if (!p) {
        /* ★ 2026-09-25（卫生 + 诊断）：这个计数原来**只加不读**（`-Wunused-but-set-variable`）。
         *   改成**首次 + 每 60 次**各打一行 —— 池满（`YK_POPUP_MAX` = 3 是 ❓ 自定值，见 `TUNING.md`）
         *   在实跑里是「弹字被吃掉」的唯一可观测信号，据此决定要不要抬这个自定上限。*/
        ++s_popup_dropped;
        if (s_popup_dropped == 1 || (s_popup_dropped % 60) == 0)
            ce_log("youkai: popup pool full, dropped %u popup(s) so far (YK_POPUP_MAX %d)",
                   s_popup_dropped, (int)YK_POPUP_MAX);
        return;
    }
    if (ch < 0 || ch > 3) ch = 0;
    n = yk_popup_digits(value, dg, (int)YK_POPUP_GLYPHS);
    p->n = 0;
    if (kind == 1) p->g[p->n++] = YK_POPUP_GLYPH_PLUS;
    for (i = 0; i < n && p->n < (int)YK_POPUP_GLYPHS; ++i) p->g[p->n++] = dg[i];
    anm = yk_player_anm();
    if (!anm) { p->n = 0; return; }
    for (i = 0; i < (int)p->n; ++i) {
        uint32_t id = ce_anm_spawn(anm, YK_GAUGE_SCRIPT_FONT[ch], YK_POPUP_ANM_LAYER);
        if (!id) { yk_popup_kill(p); return; }
        p->vm[i] = id;
        ce_anm_set_sprite(anm, id, YK_GAUGE_FONT_BASE[ch] + (int)p->g[i]);
    }
    p->kind = (uint8_t)kind;
    p->age = 0;
    p->x = x; p->y = y;
    p->used = 1;
}

static void yk_popup_tick(void)
{
    const float *pp = ce_player_pos();
    unsigned k, i;
    for (k = 0; k < YK_POPUP_MAX; ++k) {
        yk_popup_t *p = &s_popup[k];
        int life;
        float base_x, step;
        if (!p->used) continue;
        life = (p->kind == 1) ? (int)YK_POPUP_TIME_LIFE : (int)YK_POPUP_SCORE_LIFE;
        if (++p->age >= life) { yk_popup_kill(p); continue; }
        if (p->kind == 0) p->y -= 0.5f;
        if (p->kind == 0) { step = 8.0f; base_x = p->x - (float)p->n * 4.0f; }
        else              { step = 7.0f; base_x = p->x - (float)p->n * 3.5f; }
        for (i = 0; i < (unsigned)p->n; ++i) {
            uint32_t id = p->vm[i];
            float gx = base_x + (float)i * step;
            int a, dx0, dy0;
            if (!id || !ce_anm_get_vm(id)) { p->vm[i] = 0; continue; }
            if (!ce_anm_set_pos(id, gx, p->y, 0.0f)) { p->vm[i] = 0; continue; }
            dx0 = pp ? (int)(gx - pp[0]) : 0;
            dy0 = pp ? (int)(p->y - pp[1]) : 0;
            a = yk_popup_alpha(dx0, dy0);
            (void)ce_anm_set_color(id, ((uint32_t)a << 24) | 0x00ffffffu);
        }
    }
}

static int yk_orb_collect(yk_card_t *s, float x, float y)
{
    /* 收集结算（一手 `Item::collect_time_orb` `th08:0x4412b0`）：
     *   ① `AddTimeOrbs(1)`（`th08:0x44137a`）⇒ 刻符 +1、**PIV += ((1+(total&1))/2)×10**（见 core §3.11）；
     *   ② 人妖槽 ±111（`th08:0x44139e`/`:0x4413b0` → `th08:0x4413cb`，聚焦 = +111）。
     * ⚠ 另外两笔本轮**不接**（分数那一路与符卡 +8000），原因见下面那段「本轮的取舍」。*/
    if (!s) return 0;
    {
        int32_t hl, yl;                                     /* ★ P-E：单人反向封顶（见 core 的同名小节）*/
        yk_gauge_limits_solo(CE_PLAYER_FOCUSED() ? 1 : 0, &hl, &yl);
        yk_piv_add(&s->piv, &s->orbs, 1);
    /* ★★★ 2026-09-27 订正（第十六批，一手）：**这道闸不存在** —— `AddToYoukaiGauge` 的第一道闸
     * （`th08:0x43c0c2`：`cmp ds:0x17d6ed4,0 / je 加数` + `cmp [ebp+0xc],0`）读的是一个
     * **全 exe 没有任何写点的全局**：`0x17d6ed4` 在同一页（`0x17d6e00..0x17d6eff`）里只有 **11 处引用、
     * 全是读**（`cmp` ×10 + `mov eax` ×1），而且它落在 PE 的 **BSS 空档**（`.data` 结束于 `0x4cce00`、
     * `.data1` 起于 `0x18da000`）⇒ 加载器零填充 ⇒ **运行期恒为 0** ⇒ 那道「整形中丢掉加数」的分支
     * **永不成立**。⚠ 与 `shotActive`（`player+0xfdc`，`0x44be0f` 那道**推进支**的闸）是**两个东西**：
     * 后者有写点、是活的（本卡 `yk_bomb_active()` 映射的是它，保留）。⇒ 这三笔加数**不做闸**。*/

        /* ★ P-D：击破魔使后 **50 帧内收刻符不给槽**（一手 `th08:0x42b240 Timer(0x18b89d4).set(50)`，
         * 门住 `Item::collect_time_orb` 的 ±111：`th08:0x44138e`–`0x44139c`）—— **这一道是真的**（有写点）。*/
        if (s->orb_silence == 0)
            s->gauge = yk_gauge_add2(s->gauge, yk_gauge_orbs_delta(CE_PLAYER_FOCUSED()), hl, yl);
    }
    ++s->orb_taken;
    /* ★ #27 支 2：**收刻符的分值弹字**（一手 `Item::collect_time_orb th08:0x441348`
     * `CreatePlayerPointPopup(&item+0x2a4, value, color)`）—— 值 = `yk_orb_score()`（下限 100 / 上限 10000 /
     * 阶段门 2000），落点 = 那颗刻符的位置（一手就是道具位置，不换算）。*/
    yk_popup_spawn(x, y, yk_orb_score((int32_t)s->orb_taken, (int32_t)s->orb_taken), 0);
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
    /* ★★★ 2026-09-23（owner：「爆出来时点吸收速度不够快，**它应当是瞬间加速到满速**」）：
     *   **根因 = 这个开方本身**，不是抓取盒、也不是加速段（归航那支本来就「当帧重设成满速」）。
     *   旧写法「初值 = v，6 次 Newton」**在 v > 64² 时根本没收敛**：从 x0 = v 出发每步大约减半，
     *   6 步只把 x 降到 v/64 = d²/64，而返回值该是 d ⇒ **越远误差越大**（实测，float32 逐点算的）：

     *   | 距离 d | 旧返回值÷真值 | 该颗的每帧位移（满速 = 10）|
     *   | ---: | ---: | ---: |
     *   | 64 | 1.31× | 7.62 |
     *   | **128**（符卡爆点那圈半径）| **2.16×** | **4.62** |
     *   | 256 | 4.08× | 2.45 |
     *   | 512 | 8.04× | 1.24 |
     *   | 768 | 12.03× | 0.83 |

     *   ⇒ 「爆出来的那一片」恰好在 128~512 这一段 ⇒ 观感就是「飞得慢、还不加速」。
     *   改成 **指数减半给初值（IEEE754 bit trick，纯整数运算 ⇒ 不进 x87）+ 2 次 Newton**：
     *   全程相对误差 = 0（float32 精度），迭代次数固定 ⇒ 回放确定。 */
    union { float f; uint32_t u; } c;
    float x;
    if (!(v > 0.0f)) return 0.0f;
    c.f = v;
    c.u = (c.u >> 1) + 0x1fbb67ebu;   /* 经典「指数减半」初值：已准到 ~3% 以内 */
    x = c.f;
    x = 0.5f * (x + v / x);
    x = 0.5f * (x + v / x);
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


/* ★ 2026-09-23（#42「永点没看见」）：**永点 VM 的逐 VM 实况** —— 与 `yk_hud_vm_log` / 112 的
 * `ufo/slot.vm` 同一套读法（spr/scale/alpha/layer/draw）。每 60 帧给前两颗**在飞**的刻符各打一行，
 * 一次实跑即可判定「没建 / alpha=0 / 画错层 / 画在屏外」——这是 #42 收口缺的最后一块证据。*/

static const char *yk_resmode_note(uint32_t flags_hi);

/* ★★★ 2026-09-26（#39 的**诊断缺口补课**）：`resolutionMode`（`flags_hi` bits22–24）必须打出来。
 * 为什么：人妖槽「完全看不见」那一轮，`spr/scale/alpha/layer/draw` **五项全对**，唯独
 * `layer(25)` 顺手把 `resolutionMode` 置成了 1（= 坐标 ×DAT ⇒ 画到屏外）—— 那一项当时没打，
 * 于是花了四轮（改脚本 pos、改锚点、照抄 113、再挪位置）才定位。这个函数现在把模式值与
 * 「×1 / ×DAT」的判读一起打出来，**下一次一眼就能定案**。判读表（一手见 `engine.h` 的注）：
 *   0 = ×1 · 1 = **×DAT（危险）** · 2 = ×1 · 3 = ×DAT · 4 = ×1；模式 1 直接标 `★×`。*/
static const char *yk_resmode_note(uint32_t flags_hi)
{
    switch ((flags_hi & 0x1c00000u) >> 22) {
    case 0: return "0=x1";
    case 1: return "1=xDAT(★画到屏外)";
    case 2: return "2=x1";
    case 3: return "3=xDAT(★)";
    case 4: return "4=x1";
    default: return "?";
    }
}

static void yk_orb_vm_log(yk_card_t *s)
{
    unsigned i, n = 0;
    (void)s;
    for (i = 0; i < YK_ORB_MAX && n < 2; ++i) {
        yk_orb_t *o = &s_orb[i];
        const uint8_t *vm;
        const float *sp, *p2, *ep;
        if (!o->used || !o->anm_id) continue;
        vm = (const uint8_t *)ce_anm_get_vm(o->anm_id);
        if (!vm) { ce_log("youkai/orb.vm #%u: (no VM; id %u, state %d, live=%.1f)", i, o->anm_id, (int)o->state, (double)o->y); ++n; continue; }
        sp = (const float *)(vm + CE_ANM_VM_SCRIPT_POS);
        p2 = (const float *)(vm + CE_ANM_VM_POS_2);
        ep = (const float *)(vm + CE_ANM_VM_POS);
        ce_log("youkai/orb.vm #%u id %u script %d state %d: draw = (%.0f,%.0f) [= pos+pos2+ent]; "
               "sprite %d scale %.2f alpha %u layer %d flags_lo %08x resMode %s",
               i, o->anm_id, (int)*(const int32_t *)(vm + CE_ANM_VM_SCRIPT_ID), (int)o->state,
               (double)(sp[0] + p2[0] + ep[0]), (double)(sp[1] + p2[1] + ep[1]),
               (int)*(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID),
               (double)*(const float *)(vm + CE_ANM_VM_SCALE),
               (unsigned)((*(const uint32_t *)(vm + CE_ANM_VM_COLOR1) >> 24) & 0xffu),
               (int)*(const int32_t *)(vm + CE_ANM_VM_LAYER),
               (unsigned)*(const uint32_t *)(vm + CE_ANM_VM_FLAGS_LO),
               yk_resmode_note(*(const uint32_t *)(vm + CE_ANM_VM_FLAGS_HI)));
        ++n;
    }
}

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
    /* ★★★ 2026-09-27（§BE #112）：**损命当帧的那一次批量收尾**（一手 `th08:0x441530`，逐条）：
     *   遍历道具链表，**只动 `state == 1`**（归航）的那些 —— `state = 0`、`vx = 0`、**`vy = −0.9`**
     *   （`0x441574` 的字面量 `0xbf666666`）、`vz = 0`；而**每帧**路径（玩家非存活时）是 −0.7
     *   （`0x4408e0`/`:0x440711`，跑在 item tick 里）。⇒ 两条路的常量不同，本函数里**先**跑这一支
     *   （把 state 1 变成 state 0 之后，下面那条每帧判定就只剩 state 3 可打）✓ 与一手同序。
     *   ⚠ 这一支原来只定义了常量没接上（`YK_ORB_VY_DEAD` 是死代码）——`§BE #112` 补上。*/
    if (dead && !s_orb_dying_edge) {
        for (i = 0; i < YK_ORB_MAX; ++i) {
            yk_orb_t *o = &s_orb[i];
            if (o->used && o->state == YK_ORB_STATE_HOMING) {
                o->state = YK_ORB_STATE_FALL;
                o->vx = 0.0f;
                o->vy = YK_ORB_VY_DEAD;                 /* −0.9（损命当帧的批量值）*/
            }
        }
    }
    s_orb_dying_edge = (uint8_t)(dead ? 1 : 0);
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
        /* ★★★ 2026-09-22 夜（Time 的一手订正）：**损命窗口内的两颗都要打回下落**，不只是归航的：
         *   · 一手每帧路径 = `ItemManager::on_tick`（一个按 state 分叉的 if/else 链，**每支各自**在末尾
         *     查一次 `player_state == 2`）：
         *     – state 1（归航）⇒ `vy = −0.7`（`th08:0x4408e3`）然后 `state = 0`（`:0x4408f0`）；
         *     – state 3（上抛中）⇒ 整支照抄（`th08:0x4406f7`–`:0x440725`：`state=0`、`vx=0`、`vy=−0.7`、`vz=0`）；
         *     – state 5（符卡螺旋 / 擦弹）⇒ **上抛期间整支跳过**（`th08:0x440797` `jne 0x4407a5` ⇒ `jmp 0x440c37`，
         *       连 `vy` 都刚加完就返回）；**但 `vy > 0` 当帧转 state 1 之后，同一个分支里紧接着又查一次**，
         *       `vy = −0.7`（`th08:0x4407d0`）⇒ 所以 5 的判据是「**这一帧结束时会不会落到归航**」。
         *   ⇒ 我们原来是「只打 `YK_ORB_STATE_HOMING`」= 漏了 state 3 那一支（我们的 `YK_ORB_STATE_SPAWN`），
         *     而 state 5 那一支要**带上 `vy > 0` 的条件**（否则会把还在上抛的也打下去，与一手不符）。
         *   ⚠ 损命**当帧**的批量收尾（`th08:0x441530`，只动 state 1）用 **−0.9**，与每帧的 −0.7 是两个常量；
         *     那里的顺序是「先 `vy` 后 `state`」（`th08:0x441574` / `:0x44155d`），本支只求结果同值。*/
        if (dead && (o->state == YK_ORB_STATE_HOMING || o->state == YK_ORB_STATE_SPAWN ||
                     (o->state == YK_ORB_STATE_SPAWN5 && o->vy > 0.0f))) {
            o->state = YK_ORB_STATE_FALL;
            o->vx = 0.0f;
            o->vy = YK_ORB_VY_DEAD_FRAME;                /* 每帧路径：一手 `th08:0x4408e3` / `:0x440711` / `:0x4407d0` = −0.7 */
        }
        if (o->state == YK_ORB_STATE_SPAWN || o->state == YK_ORB_STATE_SPAWN5) {
            /* 出生上浮：`vy += 0.05`；`vy > 0` 那一刻转归航（一手 `th08:0x4406ac` / `:0x4406e4`）。
             * ⚠ 3 与 5 在这一支**逐指令同形**，差别只在下面那道「能不能收」的闸（§BE #111）。*/
            o->vy += YK_ORB_GRAVITY;
            if (o->vy > 0.0f) o->state = YK_ORB_STATE_HOMING;
        }
        /* ★★★ 2026-09-27（§BE #123，owner「1 2 继续开工」的第 2 条）：**PoC 线以上自动收**。
         *   一手（`ItemManager::on_tick` 的 `else` 支，decomp 原文 —— 只对 **state 0/1** 生效）：
         *   ```
         *   if (item->state == AUTOCOLLECT ||
         *       (g_Player.positionCenter.y < g_PlayerShtFile->pocY &&
         *        (g_GameManager.GetPower() >= 128.0 || g_Player.isYoukaiMode ||
         *         shotType == 1 || shotType == 6)))
         *       ⇒ 转 AUTOCOLLECT（= 满速飞向自机）
         *   ```
         *   `g_PlayerShtFile->pocY` = **128.0**（TH08 表头 `+0x1c`）；TH18 的同名量 =
         *   `player+0x47998`（`CE_PLAYER_POC_HEIGHT`，`Player__reset` 写 **128.0**，`th18:0x45ae8f`）
         *   ⇒ 两作**同一把尺子**（都是「游玩区 448 高中的 128」）。
         *   ⚠ 本卡的永点**只在妖侧（聚焦）生成** ⇒ `isYoukaiMode` 恒真 ⇒ 那三个括号项退化成
         *     「只看 PoC 线」，与一手同构（另外两项是给人侧的其它 shotType 留的）。
         *   ⚠ 为什么需要它：`SpawnItem` 对 type 7 会强制 state 3（§BE #117），而 state 3 之后若被
         *     损命那一批打回 state 0（§BE #112），原来会一路掉出下沿**被丢掉**；TH08 在自机位于
         *     PoC 线以上时会把它**吸走**（这条就是那一路）。*/
        if (o->state == YK_ORB_STATE_FALL && !dead) {
            const float *pp2 = ce_player_pos();
            if (pp2 && pp2[1] < ce_poc_height_semantic()) {
                o->state = YK_ORB_STATE_HOMING;                 /* 转归航（= TH08 的 AUTOCOLLECT）*/
            }
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
        /* ★★★ 2026-09-22 夜（Time 的**第三道门**）：**损命窗口内抓取盒整段关闭** —— 一手
         * `Player::CalcItemBoxCollision th08:0x44a5a0` 的白名单是 `{0 ALIVE, 3 INVULNERABLE, 4 BORDER}`
         *（`th08:0x44a5c1`/`:0x44a5c9`/`:0x44a5d4` 三个 `je`，其余 `th08:0x44a5d9 xor eax,eax`）。
         * TH18 的等位映射 = `{1 存活, 3 无敌/复活, 4 决死窗口}`（一手：移动只在 case 1 `th18:0x45c0cc`；
         * `Player__die th18:0x45d3a0` 置 4；`th18:0x45d090` → 2 = 死亡段）⇒ 被挡的是 `{0, 2}`。
         * ⚠ 与「每帧把刻符打回下落」（上一批那两处订正）**是两条独立规矩**，一起才等于 owner 那条
         *   「恰好在损命时出现的那颗不能自动吸收」。*/
        /* ★ 2026-09-27（§BE #111）：一手**只排除 state 3**（`th08:0x4409fc cmp ecx,3 / je`）
         * ⇒ state 5（螺旋/擦弹）**上抛期间照收**。*/
        if (o->state != YK_ORB_STATE_SPAWN && yk_item_box_open(*(const int32_t *)(CE_PLAYER() + CE_PLAYER_STATE))) {
            dx = px - o->x;
            dy = py - o->y;
            dist = dx * dx + dy * dy;
            if (dist <= pick_r * pick_r) {
                float ox = o->x, oy = o->y;
                yk_orb_collect(s, ox, oy);
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
    if (s_orb_log_pending) { s_orb_log_pending = 0; yk_orb_vm_log(s); }        /* ★ #42：出生即记 */
    if (++s_orb_log_tick >= 3600u) { s_orb_log_tick = 0; yk_orb_vm_log(s); }   /* #42：另有每 60 秒一次 */
    return taken;
}

/* ============ ★★★ #25 击破符卡的「螺旋撒点」（一手参数逐条照抄，见 `youkai_core.h` §3.11b）================
 *
 * owner 口径：「击破符卡会有一个**螺旋式**的撒永点，无敌一段时间后自动满速吸收」。
 * 一手（`th08:0x416b10` 的喷点主体 `0x417533`–`0x4176e1`，逐指令）：每 **8** 帧一批；
 * `θ(t) = ((t − 10) × 2π/40) − π/2`（**9°/帧、40 帧一圈**、`t = 10` 正上方）；半径 **128**；
 * 绕**符卡所有者（boss）的位置**；一批 = `min(额度, 7)` 同角 + **固定 6 颗 +π**；
 * 额度两段扣减；出生态 = `type 0xa → type 7 + state 5`（我们的 `YK_ORB_STATE_SPAWN5`，**上抛但可收**）。
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
                     YK_ORB_STATE_SPAWN5);   /* ★ state 5：上抛但**可收**（一手见 core 的常量注）*/
    if (k) s->spiral_quota = (uint32_t)((int)s->spiral_quota - k);
    /* 第二段：**固定 6 颗**、角 = 第一段 **+π**（一手 `0x417639 push 0x40490fdb` = π；
     * ⚠ 不是 π/2 —— `engine/item/th08/01-time-orbs.md` 的旧值已订正）。*/
    cs = yk_cos_approx(th + 3.14159265f);
    sn = yk_sin_approx(th + 3.14159265f);
    for (i = 0; i < YK_SPIRAL_SECOND_N; ++i)
        yk_orb_spawn(s, bp[0] + cs * YK_SPIRAL_RADIUS, bp[1] + sn * YK_SPIRAL_RADIUS,
                     YK_ORB_STATE_SPAWN5);
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

/* ★★★ 2026-09-27（§BE #113）：**符卡口径的探针**（`TUNING.md` §2.0zy-ter 那道额度公式的前置作业）。
 *
 * 目的：TH08 的额度是 `1000 / 700 / 100 / 按比例`（`th08:0x4162ae`–`0x416347`，两个计时器
 * `spellcard[+0x114]`（`t`）与 `spellcard[+0x108]`（`e`）），本卡现在**恒取满额 1000**。
 * 要照搬需要 TH18 侧的**两个量**：① `zSpellcard+0x24` = 符卡经过帧（已升 ✅，见 `engine.h`
 * 的 `CE_SPELL_ELAPSED`）；② 该符卡的**限时** —— 候选 = boss 的第一个活动中断槽的 `time`。
 * 「哪个槽属于当前符卡」**静态定不下来**（符卡是在某个攻击段里宣言的，而收尾那一刻槽可能已经
 * 被下一段 `setInterrupt` 换掉）⇒ 先在实跑里打一次两行日志对出来，**本函数不改任何行为**。
 *
 * 读法（owner 实跑）：
 *   · `start` 行：`elapsed` 应为 0；`slots` 里 `hp > -1 && time > 0` 的**第一个**就是候选限时；
 *   · `end` 行：`elapsed` 应 ≈ 这张符卡**实际打了多少帧**（拿它反查 `+0x24` 是不是经过帧）；
 *     若这次是**超时**（`flags` 带 `0x80`）⇒ `elapsed ≈ 该槽 time` ⇒ 槽与限时的对应关系成立。
 * ⚠ `noinline` 的理由同 `yk_fam_vm_check`（`on_tick_2` 里内联着 `ce_damage_rect` 那段汇编，
 *   别再往它身上加活值）。*/
static void __attribute__((noinline)) yk_spell_probe(const char *tag)
{
    uint8_t *sc = CE_SPELLCARD();
    uint8_t *boss = yk_boss();
    uint8_t *ed = boss ? (boss + CE_ENEMY_DATA) : 0;
    uint32_t flags = sc ? *(uint32_t *)(sc + CE_SPELL_FLAGS) : 0u;
    int i, hp[CE_ED_INTERRUPT_SLOTS], tm[CE_ED_INTERRUPT_SLOTS];
    for (i = 0; i < CE_ED_INTERRUPT_SLOTS; ++i) {
        const uint8_t *sl = ed ? (ed + CE_ED_INTERRUPTS + (unsigned)i * CE_ED_INTERRUPT_STRIDE) : 0;
        hp[i] = sl ? *(const int32_t *)sl : -2;
        tm[i] = sl ? *(const int32_t *)(sl + 4) : 0;
    }
    ce_log("youkai: spell probe %s - flags 0x%x id %d elapsed(+0x24) %d (+0x28 int %d / f %.1f) | "
           "boss %d time_in_ecl %d alive %d | slot hp [%d %d %d %d %d %d %d %d] "
           "slot time [%d %d %d %d %d %d %d %d]",
           tag, (unsigned)flags, sc ? *(const int32_t *)(sc + CE_SPELL_ID) : 0,
           sc ? *(const int32_t *)(sc + CE_SPELL_ELAPSED) : -1,
           sc ? *(const int32_t *)(sc + (CE_SPELL_ELAPSED + 4)) : -1,
           sc ? (double)*(const float *)(sc + (CE_SPELL_ELAPSED + 4)) : 0.0,
           ed ? *(const int32_t *)(boss + CE_ENEMY_ID) : 0,
           ed ? *(const int32_t *)(ed + CE_ED_TIME_IN_ECL + 4) : -1,
           ed ? *(const int32_t *)(ed + CE_ED_TIME_ALIVE) : -1,
           hp[0], hp[1], hp[2], hp[3], hp[4], hp[5], hp[6], hp[7],
           tm[0], tm[1], tm[2], tm[3], tm[4], tm[5], tm[6], tm[7]);
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

/* ❓ 自定：HUD 块尺寸（表面 px，喂货架分配器）。宽 = 128×2（槽条 128 个 640 空间 px）；
 *   高 = 视觉 (468 − 440) × 2 = 56（640 空间：百分比字形顶 = 446 − 6 = 440、夜点字形底 = 460 + 6
 *   = 466，小号位再 +2 = 468）+ 上下各 4 px pad（照 §6.1 口径）。
 *   ⚠ 认领值**不参与定位**（`yk_gauge_hud_update()` 里锚点是常量）—— 它只为「这一格归我」的记账。*/
#define YK_HUD_BLOCK_W 256.0f
#define YK_HUD_BLOCK_H  64.0f

/* ---- 人妖槽的每帧漂移（TH08 `Player::move` 的聚焦状态机，一手）--------------------------------
 * 一手形状（`th08_dec/src/Player.cpp` 那段，逐条地址见 `engine/player/th08/03-...md` §2.2 的
 * 「Player::UpdateMovement 聚焦漂移」一行）：
 *   · **门（三道，缺一不过 ⇒ 整块跳过）**：`!Gui::IsMsgActive()` + **`player+8 >= 30`** +
 *     `shotActive == 0`（`th08:0x44bdf0`–`0x44be19`）。`player+8` = 上一次换形之后的帧数。
 *   · **按住射击键 ⇒ 推进支**：先看 `player+0xe2ad0`（连停火帧数，> 0 只减不加）；否则
 *     `delta = (i32)(min(player+0xe2ae8 的帧数, 300) / 15)`（0..20）向**当前形态**推。
 *   · **松开射击键 ⇒ 回中支**：`>= 4 帧` ⇒ `player+0xe2ae8 = 0`；`>= 30 帧` 才按档
 *     **−5 / −3 / −2 / +2 / +3 / +5** 回中（极妖 −5、中妖 −3、正 −2、未到中人 +2、未到极人 +3、
 *     极人 +5）；`|g| <= 9` ⇒ 直接归零。
 *   ⚠ **订正两处旧注释（2026-09-27，逐指令复核 `th08:0x44bf04`）**：①「按住低速保持 ≥ 4 帧后开始
 *     爬升」**错** —— `push 4` 属于**回中支**（停火 ≥ 4 帧 ⇒ 清推进计时器）；②「换状态时两个计时器
 *     都清」**错** —— 一手的边沿只清 `player+0xe2ae8`（= 我们的 `gauge_ram`），**不清** `+0xe2ad0`。
 * ❓ **自定**：TH08 那两条算式末尾都乘 `g_ShotSpeed`（聚焦的射击减速因子，≤ 1）；
 *   TH18 没有同一个全局 ⇒ 本卡取 1（登记 `TUNING.md`）。*/
static void yk_gauge_tick(yk_card_t *s, int focused)
{
    int d, g2;
    int shooting;
    int32_t hl, yl;                                     /* ★ P-E：当前形态的同一对 limit */
    if (!s) return;
    yk_gauge_limits_solo(focused, &hl, &yl);
    shooting = yk_shooting();
    if (focused != (int)s->gauge_focus_prev) {          /* 换状态 ⇒ 清推进计时器（一手）*/
        s->gauge_focus_prev = (uint8_t)(focused ? 1 : 0);
        s->gauge_focus_run = 0;
        s->gauge_hold = 0;
    }
    if (s->gauge_focus_run < 0xffffu) ++s->gauge_focus_run;
    /* ★★★ 2026-09-22 夜（P-E）：**单人反向封顶真的夹住了**就记一行（实跑判据）。
     * 一手：单人的反侧 limit = ±2000（`th08:0x44daa2` / `th08:0x44dacd`），而逢魔线仍是 ±8000
     * ⇒ 反侧**永远到不了逢魔**（owner §1.8 的「只能到 20%」）。*/
    if (!s->solo_cap_logged) {
        int capped = focused ? (int)hl : (int)yl;
        if ((focused && s->gauge <= capped) || (!focused && s->gauge >= capped)) {
            s->solo_cap_logged = 1;
            ce_log("youkai: solo reverse cap engaged - the opposite side stops at %d "
                   "(TH08 `Player::on_registration` th08:0x44daa2/th08:0x44dacd: solo caps the reverse "
                   "limit at 2000 = 20%%, while the fengma line stays at +/-8000)",
                   capped);
        }
    }
    /* ★★★ 2026-09-22 夜（P-C 订正，owner 裁「按一手」）：三条互斥的分支**照 `Player::move` 重写**。
     *
     * 一手（`th08:0x44bdf0`–`0x44bffb`，逐条）：
     *   ① 门：`!IsMsgActive()` + `player+8 >= 30` + **`shotActive == 0`**（不在放符卡/炸弹）；
     *   ② **`shotTimer2 >= 0`（射击键按住）⇒ 推进支**：先看 `player+0xe2ad0`（连停火帧数）
     *      —— **> 0 时只减不加**（预热，`th08:0x44be5d`）；否则 `delta = floor(min(player+0xe2ae8, 300)/15)`
     *      （封顶 21）向当前形态推，并把 `player+0xe2ae8` +1；
     *   ③ **`shotTimer2 < 0`（键松开）⇒ 回中支**：`player+0xe2ad0 >= 4` ⇒ `player+0xe2ae8 = 0`；
     *      `player+0xe2ad0 < 30` ⇒ **只是等（延迟 30 帧）**；到 30 才按三档 `±2/±3/±5` 回中，
     *      `|g| <= 9` ⇒ **直接置 0**（`th08:0x44bf5a`）。
     *   ④ **放符卡/炸弹期间**（`shotActive != 0`）：上面两条**都被跳过**，只有 `±26/帧` 那一项在动。*/
    if (yk_bomb_active()) {                             /* ① 放符卡/炸弹：一手 `th08:0x44c820` 一族 */
        int bomb_delta = (focused ? 1 : -1) * YK_GAUGE_SHOOT_DELTA;
        s->gauge = yk_gauge_add2(s->gauge, bomb_delta, hl, yl);
        if (s->gauge_shoot_logged != 2) {
            s->gauge_shoot_logged = 2;
            ce_log("youkai: gauge advance = BOMB/spell period at %+d/frame (TH08 `shotActive` = casting a "
                   "spell or bomb, NOT ordinary shooting; th08:0x44c820 family)", bomb_delta);
        }
        return;
    }
    /* ★★★ 2026-09-27（owner ③）：**换形后 30 帧内，漂移那一块整块不进**。
     * 一手 `th08:0x44be05 cmp DWORD PTR [ecx+0x8],0x1e` + `0x44be09 jl 0x44c012`
     * （`ecx+0x8` = `player+8` = 上一次换形之后的帧数 = 我们的 `focus_age`，换形当帧刚被清 0）。
     * ⇒ 「按一下马上松 ⇒ 槽一点不变」在一手里有**三条独立原因**，这是第二条（第一条 = 边沿当帧
     *   就被这道门挡住；第三条 = 换形时 `gauge_ram` 清零后 `rate = frames/15` 还要 15 帧才 ≥ 1）。
     * ⚠ **只挡漂移支**：一手这道门只包住 `Player::move` 那一块；`yk_on_enemy_killed` / 收刻符 /
     *   擦弹 / 击破使魔都不受影响（它们的写点在自己的函数里），**别**把这道门搬过去。
     * ⚠ 放符卡/炸弹那一支（上面）**不受这道门管**：它在 TH08 里属射击路径（`th08:0x44c820` 一族），
     *   不在这道门的范围里。*/
    if (s->focus_age < YK_GAUGE_SHIFT_BLANK) return;
    if (shooting) {                                     /* ② 推进支（射击键按住）*/
        if (s->gauge_idle > 0) {                        /* 预热：连停火帧数先减到 0（`th08:0x44be5d`）*/
            --s->gauge_idle;
            return;
        }
        d = yk_gauge_ramp_delta((int)s->gauge_ram);
        s->gauge = yk_gauge_add2(s->gauge, focused ? d : -d, hl, yl);   /* 方向 = `player+3` */
        if (s->gauge_ram < 0xffffu) ++s->gauge_ram;                     /* `th08:0x44bee9` */
        if (!s->gauge_shoot_logged) {
            s->gauge_shoot_logged = 1;
            ce_log("youkai: gauge advance = continuous-fire time ramp, delta = floor(min(frames,300)/15) "
                   "capped at %d (TH08 `Player::move` th08:0x44be67 - NOT the 10%%/25%% tiers the owner "
                   "remembered; those do not exist in v1.00d)", (int)YK_GAUGE_RAMP_CAP);
        }
        return;
    }
    /* ③ 回中支（射击键松开）*/
    if (s->gauge_idle >= YK_GAUGE_STOP_CLEAR) s->gauge_ram = 0;   /* `th08:0x44bf04 push 4`：>= 4 帧清零 */
    if (s->gauge_idle < YK_GAUGE_IDLE_MAX) ++s->gauge_idle;       /* 上限 30（`th08:0x44c007`）*/
    if (s->gauge_idle < YK_GAUGE_IDLE_MAX) return;                /* 延迟 30 帧：`th08:0x44bf1d` */
    g2 = s->gauge;
    if (g2 > YK_GAUGE_DEAD_ZONE || g2 < -YK_GAUGE_DEAD_ZONE) {
        s->gauge = yk_gauge_add2(s->gauge, yk_gauge_center_delta(g2), hl, yl);
    } else {
        s->gauge = 0;                                   /* 一手 `th08:0x44bf5a`：死区归零 */
    }
}

/* 涨源①：**击破敌机** ⇒ 人妖槽 ±200（一手 `th08:0x42da2a` 一带：`isYoukaiMode ? +200 : -200`）；
 * 同时（仅在妖侧）撒刻符 —— 一手 `Enemy::sub_42adb0`（`th08:0x42adb0`，这一轮读全，见 §BE #114）。
 *
 * ★★★ 2026-09-27（owner 拍板 **B：照一手加门**）：**结构整个订正**了三处：
 *   ① **门**：一手 `n = GetSubEnemyChainCount(this)`（`th08:0x42adbc`），`n == 0` ⇒ `0x42adc4 je`
 *      **跳过整段撒点** ⇒ **单只、无兄弟的敌机一颗都不给**（旧版对每次击破都撒 12 颗 = 与一手不同）。
 *      TH18 侧的对位：**「这个敌人自己建出来的活子敌人」**——ECL 的 `enemyCreate` 会把
 *      当前敌人的 `enemy_id` 写进子敌人的 `parent_enemy_id`（一手 `th18:0x436b64`–`0x436b82`：
 *      `eax = [esi+0x5588]`（= zEnemyData 反查所属 zEnemy）→ `[eax+0x6830]` = 施放者的 enemy_id
 *      → `allocate_new_enemy` 里写进 `+0x6834`（`th18:0x42d8e7`）⇒ 就是 TH18 的「子敌人链」）。
 *   ② **撒点位置**：一手是撒在**每个子敌人的位置**上（`0x42af6c add ecx,0x2d88` = 该链结点的
 *      `final_pos`），**不是**被杀者身上（旧版撒在被杀者位置 = 与一手不同）。颗数 = 同一算式。
 *   ③ **被杀者自己那一环**：链走完之后（`0x42b0e5`）才是 `subEnemyCount * 2` 颗、
 *      **state 1**（生成即归航）、**固定半径 128 的环**（`0x42b15d push 0x43000000`）——
 *      这一支才是我早先记的那条「128 环」，且它与上面那批是**两套常量**（旧版混成了一套）。
 *   ⚠ `param != 0`（= 死亡路径）才撒；`0x42b1e5` 起还有一支：**被杀者自己有主**（它是别人的子敌人）
 *     ⇒ 槽 `-= 槽/12`（`0x42b223`）＋**单颗** state 1 刻符（`0x42b29b` 的 `spawn_item`）。
 *   ⚠ 弹消圈（`Player::make_bomb_cancel_area_circle` `th08:0x44df00`，每个子敌人一处 radius 32
 *     ×8 颗 / 被杀者一处 radius 32 ×16 颗）**本期未做**：TH18 的对应面是
 *     `engine.h` 的 `CE_FN_BULLET_CANCEL_RADIUS_AS_BOMB`，参数语义与 `0x44df00` 不是一一对应
 *     ⇒ 按纪律记 ❓ 待验（`TUNING.md` §2.0zy-bis 的续），不硬凑。*/
#define YK_KILL_CHILD_MAX 16        /* ❓ 自定：子敌人位置缓存的容量（一对位，一手无上限）*/
static int s_kill_burst_logged;     /* 首次「带子敌人」的击破打一行（自定诊断，不参与逻辑）*/
static int s_kill_gated_logged;     /* 首次「被门拦下」的击破打一行（同上）*/
static void yk_on_enemy_killed(yk_card_t *s, const ce_enemy_death_t *ev)
{
    int focused, i, n2;
    int32_t hl, yl;                                     /* ★ P-E：当前形态的同一对 limit */
    if (!s || !ev) return;
    focused = CE_PLAYER_FOCUSED();
    yk_gauge_limits_solo(focused, &hl, &yl);
    /* ★★★ 2026-09-27 订正（同 §BE #110）：`AddToYoukaiGauge` 的第一道闸（`th08:0x43c0c2`）读的
     * `ds:0x17d6ed4` **全 exe 没有写点、又是 BSS**（⇒ 恒为 0）⇒ 「整形中丢掉 ±200」那条分支永不成立
     * ⇒ 击破 ±200 **无条件照加**（decomp `EnemyManager.cpp:670-677` 传的 `param_2 = 0` 只对那条死分支有意义）。*/
    s->gauge = yk_gauge_add2(s->gauge, focused ? 200 : -200, hl, yl);
    /* ★★★ 2026-09-22 夜（P-B）：**逢魔那一颗刻符与聚焦无关** —— 一手它在死亡公共尾
     *（`EnemyManager::on_tick` 的 `th08:0x42da2a`/`th08:0x42da38` ⇒ `th08:0x42da41 push 1` /
     * `th08:0x42da54 spawn_item(type 7, state 1)`），人侧（未聚焦）一样掉。
     * ⚠ 原来这段写在下面 `!focused` 之后 ⇒ 人侧逢魔不掉 —— 本批挪到前面（一手修正）。
     * 出生态 state 1 = 生成即满速归航（我们的 `YK_ORB_STATE_HOMING`）。*/
    /* ★★★ 2026-09-22 夜（P-B + P-D）：逢魔击破掉几颗 = 一手 `yk_fengma_kill_orbs()` 的算式
     *（逢魔 ⇒ 1；**人间逢魔 + 自机弹击杀** ⇒ 2）。`by_player_bullet` 是**新追加**的事件字段
     *（`ce_enemy_death_t` 尾部，见 `sdk/public.h` 的注与 `AUDIT.md` §BE #66 的共享件登记）。*/
    {
        int fm = yk_fengma(s->gauge, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS);
        int human_fm = (s->gauge <= YK_GAUGE_HUMAN_EFFECTS) ? 1 : 0;
        int norbs = yk_fengma_kill_orbs(fm, human_fm, ev->by_player_bullet ? 1 : 0);
        int oi;
        /* ★★★ 2026-09-27（§BE #117，owner：「人妖逢魔的永点……应当是下落一小段之后直接消失
         *   （= 直接吃掉），而不是现在还走一个自动吸收的中间路程」）：**改成上抛态（state 3）**。
         *   一手（`ItemManager::SpawnItem` 正文）：`if (itemType == ITEM_TIME) state = ITEM_STATE_UNK3;`
         *   `else if (itemType == ITEM_TIME2) { state = ITEM_STATE_UNK5; itemType = ITEM_TIME; }`
         *   ⇒ **type 7（时点）的实参 state 被强制覆写成 3**（type 0xa ⇒ 强制 5）——
         *   调用点写的 `push 1`（`th08:0x42da41`）**根本到不了 item**；真正会落在 state 1 的
         *   只有「消弹 / `cancel_autocollect`」那一支（`th08:0x4308e5` / `0x4413e0`）。
         *   ⇒ 出生态 3：初速 `vy = −2.0 − rand(0.2)`、`vx = rand(±0.6)`、每帧 `vy += 0.05`，
         *     `vy > 0` 才转归航 ⇒ 画面上是**先上抛一小段、再落下来被收**。*/
        for (oi = 0; oi < norbs; ++oi)
            yk_orb_spawn(s, ev->x, ev->y, YK_ORB_STATE_SPAWN);
    }
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
    /* ★★★ 2026-09-27（§BE #114）：**照一手的结构撒** —— 门 + 每个子敌人一处 + 被杀者自己那一环。
     *   `child[]` 先扫出「自己建出来的活子敌人」（`parent_enemy_id == ev->id`，见函数头的注 ①），
     *   `n` = 个数；`n == 0` ⇒ `yk_kill_burst_count` 给 0 ⇒ **一颗都不撒**（一手 `0x42adc4` 的门）。*/
    {
        float cx[YK_KILL_CHILD_MAX], cy[YK_KILL_CHILD_MAX];
        unsigned n = 0, k;
        uint8_t *em = CE_ENEMY_MGR();
        uint8_t *e = 0;
        if (em) {
            CE_ENEMY_FOREACH(em, e) {
                /* ⚠ **不过 `ce_enemy_lockable`**：一手数的是「链上还在的结点」，与能不能锁定无关
                 *   （TH18 的 boss 部件常常不可锁定；用可锁定当筛子会漏数、颗数公式就跟着偏）。*/
                if (!e) continue;
                if (*(const int32_t *)(e + CE_SPAWN_PARENT) != (int32_t)ev->id) continue;
                ++n;                                        /* 计数不封顶（颗数公式要真个数）*/
                if (n <= YK_KILL_CHILD_MAX) {
                    const float *cp = ce_enemy_pos(e);
                    cx[n - 1] = cp ? cp[0] : ev->x;
                    cy[n - 1] = cp ? cp[1] : ev->y;
                }
            }
        }
        n2 = (int)yk_kill_burst_count(n);
        if (!n2) {
            /* 门拦下（TH08 的常态：绝大多数道中敌机没有子敌人）—— 只在**第一次**打一行，
             * 让实跑日志能一眼看出「门在生效」而不是「刻符没了」。*/
            if (!s_kill_gated_logged) {
                s_kill_gated_logged = 1;
                ce_log("youkai: kill burst GATED OUT (enemy #%u has no ECL child part) - TH08 "
                       "`th08:0x42adc4 je` skips the whole burst, so no disc; a plain enemy still "
                       "gets the SINGLE HasOwnerEnemy orb (`th08:0x42b29b`) because TH18's stage "
                       "timeline is itself an enemy (`th18:0x444228` \"main\") - see "
                       "engine/enemy/th18/08 (further gated kills stay silent)",
                       (unsigned)ev->id);
            }
        } else {
            unsigned nc = (n > YK_KILL_CHILD_MAX) ? YK_KILL_CHILD_MAX : n;
            if (!s_kill_burst_logged) {
                s_kill_burst_logged = 1;
                ce_log("youkai: kill burst ON - enemy #%u has %u live ECL child part(s) => "
                       "%d orbs per part (TH08 `Enemy::FUN_0042adb0`: shottype branch n>=%u ? %u : "
                       "2n+10), scattered in a disc of radius 2N around EACH part; plus %u ring orbs "
                       "(state 1, radius %.0f) around the enemy itself",
                       (unsigned)ev->id, n, n2, (unsigned)YK_KILL_BURST_CAP_N, (unsigned)YK_KILL_BURST_CAP,
                       (unsigned)yk_kill_ring_count(n), (double)YK_KILL_RING_RADIUS);
            }
            for (k = 0; k < nc; ++k) {                      /* 一手：每个链结点各撒 N 颗（`0x42aff0`）*/
                for (i = 0; i < n2; ++i) {
                    float ang, rad;
                    /* ⚠ 两个随机数**先取到局部**（一手顺序是「先半径 `0x40d390`、后角度 `0x4143c0`」；
                     *   写成函数实参求值顺序在 C 里未定义 ⇒ 会破坏回放确定性）。*/
                    float rr = (float)(ce_rand() % 1000u) / 1000.0f;   /* → 半径 ∈ [0, 2N) */
                    float ra = (float)(ce_rand() % 1000u) / 1000.0f;   /* → 角 ∈ [−π, π) */
                    yk_orb_burst_polar(n2, ra, rr, &ang, &rad);
                    yk_orb_spawn(s, cx[k] + yk_cos_approx(ang) * rad,
                                    cy[k] + yk_sin_approx(ang) * rad,
                                 YK_ORB_STATE_SPAWN);   /* 一手 `0x42b048`：state 3（上抛，期间收不到）*/
                }
            }
            /* 被杀者自己那一环（一手 `0x42b13b`–`0x42b1b1`）：`subEnemyCount * 2` 颗、**state 1**、
             * 半径上界 **128**（`0x42b15d` —— ⚠ 同样走 `GetRandomF32InRange` ⇒ **圆盘**，不是固定环）。*/
            for (k = 0; k < yk_kill_ring_count(n); ++k) {
                float ang, rad;
                float rr = (float)(ce_rand() % 1000u) / 1000.0f;       /* → 半径 ∈ [0, 128) */
                float ra = (float)(ce_rand() % 1000u) / 1000.0f;       /* → 角 ∈ [−π, π) */
                yk_orb_burst_polar_range(YK_KILL_RING_RADIUS, ra, rr, &ang, &rad);
                yk_orb_spawn(s, ev->x + yk_cos_approx(ang) * rad,
                                ev->y + yk_sin_approx(ang) * rad,
                             YK_ORB_STATE_SPAWN);       /* ★ §BE #117：type 7 ⇒ 实参被覆写成 state 3 */
            }
        }
        /* 一手 `0x42b1e5` 起：**被杀者自己有主**（它是别人的子敌人）⇒ 单颗 state 1（`0x42b29b`）。
         * ⚠ 一手它**只在自己不是链根时**才轮到 —— 链根按定义 `ownerEnemy == 0`（`0x41f000`），
         *   所以「有链条的敌机」不会同时拿这一颗；用 `N == 0`（= 没有子敌人）当判据与此同形。
         * ⚠ 一手这一支还带「槽 -= 槽/12」（`0x42b223`）；本卡**没做**那一笔（登记 `TUNING.md`）。
         * ★★★ 2026-09-27 本轮新验（**TH18 架构差异，必须知道**）：TH18 的**关卡时间轴本身就是一个敌人**
         *   （`GameThread::on_tick` `th18:0x444228` 用 ECL 名 `"main"` 调 `allocate_new_enemy`）⇒
         *   时间轴建的每一只敌机，其 `parent_enemy_id` **都是那个载体的 enemy_id（非 0）**；
         *   而 `enmCreate` 写父字段是显式的（`th18:0x436b82` 把施放者的 `enemy_id` 存进 params+0x50，
         *   `0x42d8e7` 落到 `+0x6834`）。⇒ **普通妖精在这一支下会掉 1 颗**（不是 0 颗）——
         *   这正是 owner 选 B 时说的「普通妖精只剩 1 颗」；与 TH08（普通敌机 `ownerEnemy == 0` ⇒ 0 颗）
         *   的差别已登记 `TUNING.md` / `AUDIT.md` §BE #114。*/
        if (!n2 && ev->enemy &&
            *(const int32_t *)((const uint8_t *)ev->enemy + CE_SPAWN_PARENT) != 0)
            yk_orb_spawn(s, ev->x, ev->y, YK_ORB_STATE_SPAWN);   /* ★ §BE #117：type 7 ⇒ 强制 state 3 */
    }
    /* ② 極端人/妖的那一颗**已挪到上面**（`!focused` 之前）—— 一手它与聚焦无关，见那里的注。*/
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
    for (i = 0; i < YK_NIGHT_TEXT_MAX; ++i) {          /* ★ #27/#28：夜点数字的 8 格也一起收 */
        if (s->hud_night[i]) ce_anm_delete(s->hud_night[i]);
        s->hud_night[i] = 0;
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
    /* ★★★ 2026-09-22 夜（#27/#28）：**夜点数字的 8 格**（一手里它与百分比共用**同一个 VM**、逐位换 sprite；
     * TH18 一个 VM 一帧只画一次 ⇒ 照样开 8 格，见 `yk_night_text_sync`）。整块的门照抄：
     * `hud_ready` 为 0 时这一整批（条/图标/游标/百分比/夜点）**一起不建**。*/
    for (i = 0; i < YK_NIGHT_TEXT_MAX; ++i) {
        uint32_t id;
        if (s->hud_night[i] && ce_anm_get_vm(s->hud_night[i])) continue;
        id = ce_anm_spawn_ui(yk_player_anm(), YK_GAUGE_SCRIPT_FONT[ch], 0);
        s->hud_night[i] = id;
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

/* ★★★ 2026-09-23（owner 第 #30 条「又看不见人妖槽」）：**逐 VM 实况行** —— 与 112 那条
 * `ufo/slot.vm`（正是靠它一次定案的）同款。只读、每 60 帧一次 ⇒ 常驻零成本。
 * 五项判据：`spr`(+0x24) / `scale`(+0x54) / `a`(color_1 `+0x524` 的 A 字节) / `L`(layer `+0x18`) /
 * `draw` = 脚本 pos(+0x30) + `__pos_2`(+0x4e4) + 实体坐标(+0x5f0)（C 侧摆位写的就是最后那项）。
 * 判据一句话：**`L` 必须是 24/25（否则落在兜底绘制组 39 = 暂停模糊罩之后）、`a` 必须非 0、
 * `draw` 必须落在 HUD 块里**。三者缺哪个，一眼就看得出是「没画 / 透明 / 画错层 / 画错地方」。*/
static void yk_hud_vm_log(const char *tag, uint32_t id)
{
    const uint8_t *vm = id ? (const uint8_t *)ce_anm_get_vm(id) : 0;
    const float *sp, *p2, *ep;
    if (!vm) { ce_log("youkai/hud.vm %s: (no VM; id %u)", tag, id); return; }
    sp = (const float *)(vm + CE_ANM_VM_SCRIPT_POS);
    p2 = (const float *)(vm + CE_ANM_VM_POS_2);
    ep = (const float *)(vm + CE_ANM_VM_POS);
    ce_log("youkai/hud.vm %s id %u script %d instr %d: draw = pos(0x30)(%.0f,%.0f) + pos2(0x4e4)(%.0f,%.0f) "
           "+ ent(0x5f0)(%.0f,%.0f) = 表面 (%.0f,%.0f); sprite %d scale %.2f alpha %u layer %d flags_lo %08x "
           "resMode %s flags_hi %08x",
           tag, id, (int)*(const int32_t *)(vm + CE_ANM_VM_SCRIPT_ID),
           (int)*(const int32_t *)(vm + 0x2c),
           (double)sp[0], (double)sp[1], (double)p2[0], (double)p2[1], (double)ep[0], (double)ep[1],
           (double)(sp[0] + p2[0] + ep[0]), (double)(sp[1] + p2[1] + ep[1]),
           (int)*(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID),
           (double)*(const float *)(vm + CE_ANM_VM_SCALE),
           (unsigned)((*(const uint32_t *)(vm + CE_ANM_VM_COLOR1) >> 24) & 0xffu),
           (int)*(const int32_t *)(vm + CE_ANM_VM_LAYER),
           (unsigned)*(const uint32_t *)(vm + CE_ANM_VM_FLAGS_LO),
           yk_resmode_note(*(const uint32_t *)(vm + CE_ANM_VM_FLAGS_HI)),
           (unsigned)*(const uint32_t *)(vm + CE_ANM_VM_FLAGS_HI));
}

/* ★★★ 2026-09-22 夜（#27/#28）：**夜点数字**（一手几何：y=460、x 从 80 起、左对齐 7 px/字、
 * **前导零不画也不占位**、五档染色与槽条/百分比共用；`th08:0x405d11`–`0x405e01`）。
 * 值 = 我们卡里的 `piv`（= TH08 的 `pointItemValue` / 夜点，P-A 已把初值按难度落好）。*/
static void yk_night_text_sync(yk_card_t *s, const yk_gauge_hud_t *h, uint32_t col)
{
    yk_gauge_glyph_t g[YK_NIGHT_TEXT_MAX];
    void *anm;
    int n, i, ch;
    if (!s || !h) return;
    ch = yk_current_character();
    if (ch < 0 || ch > 3) ch = 0;
    anm = yk_player_anm();
    /* ★ §BE #122：显示「额外最大得点」池（`ce_piv_pool()`，**初值 0** ⇒ 初始画 `000000`），
     *   而不是 TH08 那个带难度初值（NORMAL 100000）的夜点本体 —— 后者正是 owner 说的「那个 100000 东西」。
     *   池 = Σ 各卡的 `ce_score_declare_delta()`（本卡那笔见 `on_tick_2` 里的声明）⇒ 与 110/111
     *   那一族「最大得点」**同一份数**（`cards/faith_hud.c` 的 `faith_hud_maxpoint()` 也是读它）。*/
    n = yk_night_text_glyphs(ce_piv_pool(), g);
    for (i = 0; i < YK_NIGHT_TEXT_MAX; ++i) {
        uint32_t id = s->hud_night[i];
        if (!id) continue;
        if (i >= n) { ce_anm_set_color(id, 0x00ffffffu); continue; }     /* 这一格不用 ⇒ 透明 */
        if (anm) ce_anm_set_sprite(anm, id, YK_GAUGE_FONT_BASE[ch] + (int)g[i].glyph);
        ce_anm_set_pos(id, h->night_x + g[i].dx * h->px, h->night_y + g[i].dy * h->px, 0.0f);
        ce_anm_set_scale(id, g[i].scale * h->px, g[i].scale * h->px);
        ce_anm_set_color(id, 0xff000000u | col);
    }
}

static void yk_gauge_hud_update(yk_card_t *s)
{
    yk_gauge_hud_t h;
    float anchor[2];
    uint32_t col;
    if (!s) return;
    /* ★★★ 2026-09-23（owner：「人妖槽好像在整个画面的**上方**被挡住了」+「**照着 113 定位**」）：
     *   根因 = **货架（shelf）会把块上抬**：`HUD-LAYOUT.md` §2 的算法在一行放不下时
     *   `row_bottom -= row_h + GAP` ⇒ 装了几张系统卡（116/117/110/111 …）之后，我们这块可能被排到
     *   **第二行**（= 画面上方），还会和游玩区叠着 ⇒ 观感正是「跑到上面、还被挡住」。
     *   而 **113（`trance.c`）根本不用货架** —— 它写死表面坐标（`TRANCE_HUD_BASE_X/Y`），
     *   owner 2026-09-23 明确要「照着 113 117 定位」⇒ 本卡按同一口径：**锚点改成常量**，
     *   不再吃 `s->hud`（认领仍保留：只为「这块归我」的记账与日志，不参与定位）。
     * ★★★ 2026-09-26（owner #39：「参考 th12/13/16/17/18 **原生的左下角框**看一下人妖槽的左下角
     *   位置应该是哪样的坐标」）：锚点**不再抄 113 的数**，改成**一手换算的游玩区左下角**
     *   `(257, 928)` —— 依据三条：
     *     · 用户量过的 TH18 游玩区 = `x 257..1023 / y 32..928`（`trance.c:3884` 的注释里就写着）；
     *     · TH08 的槽条 x = **32 = TH08 游玩区左沿**（相机 0 视口 `scale×(32,16,384,448)`，
     *       `engine/anm/th18/02-render-stages.md` §2）⇒ 折算到 TH18 = 257（不是 113 的 248：
     *       113 是 **TH13 灵界槽**的槽根，与本卡不是同一个东西）；
     *     · TH08 的槽条 y = 449 而 TH08 游玩区下沿 = 464 ⇒ 条"骑"在场地下沿上；折算到 TH18
     *       就是 `928 − (464−449)×2 = 898`（`yk_gauge_hud()` 里那段基准订正）。
     *   ⇒ 人妖槽整块的**左下角 = 游玩区左下角**，与原版 TH08 逐条同构（并顺带对齐了
     *     116/117 用的那条「左下角内缩锚点 257」= `HUD-LAYOUT.md` §1.2）。*/
#define YK_GAUGE_BASE_X   257.0f        /* = **游玩区左沿**（用户量过：游玩区 = x 257..1023 / y 32..928）
                                         *   —— 一手同源：TH08 的槽条 x = 32 = **TH08 游玩区左沿**（相机 0
                                         *   视口 `scale×(32,16,384,448)`）⇒ 折算到 TH18 表面就是 257。
                                         *   ⚠ 旧值 248 是抄 113 的（113 是 TH13 的灵界槽，不是 TH08 的槽条）。*/
#define YK_GAUGE_BASE_Y   928.0f        /* = **游玩区下沿**（同一份用户量值）= TH08 640 空间的 464 */
    anchor[0] = YK_GAUGE_BASE_X;
    anchor[1] = YK_GAUGE_BASE_Y;
    (void)s->hud;                       /* 认领值不再参与定位（见上）；日志里保留显示 */
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
    yk_night_text_sync(s, &h, col);                        /* ★ #27/#28：夜点数字（左对齐 7 px/字）*/
}

static int on_ctor(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    if (!s) return 0;
    /* ★ 先收跟随 VM 再复位静态表：本函数**在局中被重建时也会跑**（每关开始引擎对卡组每张卡
     * 再调一次 ctor）—— 那时 `yk_map_reset` 会把使魔一并忘掉，它们身上的 VM 若不复位就永久留下
     * （`s_fam_vm` 是文件级静态，不是卡状态）。句柄为空时是纯 no-op。*/
    yk_fam_vm_kill_all();
    /* ★ 2026-09-27 第二十六批：卡侧束的句柄也是**文件级静态**（每关 ctor 会重跑）⇒ 一并收干净，
     * 否则换关后那支 VM 会永远钉在旧位置（`METHOD.md` 铁律二：长持续效果必须交代被删掉会怎样）。*/
    yk_beam_kill(0);
    yk_map_reset(&s_map);
    yk_transfer_reset(&s->tr);
    __builtin_memset(s->opt, 0, sizeof s->opt);
    s->opt_script = -1;
    s->shot_timer = 0;
    s->spawns = s->broken = s->lost = s->transferred = s->dropped = s->fam_full = 0;
    s->logged_opt = s->logged_sht = s->logged_noshot = s->logged_full = s->logged_gate = 0;
    s->hud_vm_up_logged = 0;
    s->logged_pass0 = 0;
    s->logged_hudgeo = 0;               /* ★ P2：换关重开 ⇒ 几何自述再打一次（锚点可能变了）*/
    s->heals_seen = 0;
    s->rearms_seen = 0;                 /* ★ route A：整表 reset 也清条目，但计数只增不清 ⇒ 快照回 0 */
    s->releases_seen = 0;               /* ★ P8-3：同上（route A′ 计数只增不清）*/
    s->night_logged = 0;
    s->solo_cap_logged = 0;
    s->gauge_ram = 0;
    s->gauge_idle = 0;
    s->gauge_shoot_logged = 0;
    s->orb_silence = 0;
    s->fam_nodmg_logged = 0;
    s->fam_miss_latch = 0;
    s->fengma = 0;
    s->fengma_score = 0;
    s->claimed_set = -1;                /* ★ P7：认领日志的「变化才打」闩 */
    s->flash_pending = 0;
    s->aura_id = 0;                     /* ★ #23：本体 Shift 光环（低速 = 妖侧才存在）*/
    s->aura_on = 0;
    s->aura_logged = 0;
    s->focus_age = 0;
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
    s->opt_spin_vm = 0;
    s->chase_have = 0;
    s->chase_id = 0;
    s->chase_x = 0.0f;
    s->chase_y = 0.0f;
    s->chase_enters = 0;
    s->logged_chase = 0;
    s->logged_spin_resend = 0;
    s->opt_spin_vm = 0;
    s->logged_natopt = 0;
    __builtin_memset(s->opt_pos, 0, sizeof s->opt_pos);
    __builtin_memset(s->opt_vel, 0, sizeof s->opt_vel);
    __builtin_memset(s->opt_face, 0, sizeof s->opt_face);
    __builtin_memset(s->opt_frozen, 0, sizeof s->opt_frozen);
    __builtin_memset(s->opt_orbit, 0, sizeof s->opt_orbit);
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
    /* ★★★ 2026-09-22 夜（owner 工单 P-A）：**夜点初值四档**按难度起（一手 `th08:0x43af1b` 的 switch）——
     * 我们此前从 0 起，那是一手没有的状态。夜点 = TH08 的 `pointItemValue`（`globals+0x24`），
     * 也就是我们这套 `piv`（永点/最大得点）：屏幕中间下方那个数 + 「蓝点的最大分」。
     * 难度一手编码 = `CE_ADDR_DIFFICULTY`（0 EASY/1 NORMAL/2 HARD/3 LUNATIC/4 EXTRA），与 TH08 同序。*/
    s->piv = yk_night_point_init(*(const int32_t *)(uintptr_t)CE_ADDR_DIFFICULTY);
    s->piv_base = s->piv;              /* ★ §BE #122：这一格是**基准**，不进「额外最大得点」池 */
    if (!s->night_logged) {
        s->night_logged = 1;
        ce_log("youkai: night point init = %d (difficulty %d; TH08 `pointItemValue` globals+0x24, "
               "`th08:0x43af1b` switch) - blue points give at most this much, +10 per 2 time orbs",
               (int)s->piv, *(const int32_t *)(uintptr_t)CE_ADDR_DIFFICULTY);
    }
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
    yk_ring_kill_all();                                  /* ★ 切换环：换关时 tick 会停 ⇒ 这里直接收 */
    yk_popup_kill_all();                                 /* ★ #27：弹字同样要收干净 */
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
    yk_ring_kill_all();                                  /* ★ 切换环：删卡那一刻 tick 停着 ⇒ 直接收 */
    yk_popup_kill_all();                                 /* ★ #27：弹字（删卡那一刻 tick 停着）*/
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
    /* ★★★ 2026-09-26（owner：「咲夜妖机体的子机一直在重建」）：**逐槽核实，而不是无脑清空**。
     *   上面那条「引擎整池重建 ⇒ 我们的指针全作废」只在**真的**重建过时成立；而引擎的
     *   `repopulate` 一手（`0x45d5e0`）只拆**原生那 4 格**（`lea ebx,[ecx+0x50]`；清 `in_use` 的那一圈
     *   `0x45dbe0` 用的也是同一个基址）——**它根本不碰卡池**（`inner+0x410`）。于是无脑清指针会让我们
     *   把还活着的子机当成丢了、去重新申请（同一个池格、新 VM）⇒ 每次广播都白重建一次。
     *   ⇒ 改成用 `ce_option_claim()` 逐槽问一句「这一格还是我的吗」：是我的就留着（VM 照跑），
     *     被回收/被别的卡拿走了它自己会清（`sdk.c` 的 `ce_option_claim`）。 */
    for (i = 0; i < YK_OPTION_MAX; ++i) {
        s->opt[i].retry = 0;
        (void)ce_option_claim(c, &s->opt[i]);   /* 还是我们的 ⇒ 原样留着；丢了 ⇒ 它自己清 ptr */
    }
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
    /* ★ 2026-09-26（ownner 实跑「灵梦/魔理沙/咲夜 妖侧不出弹」那一批时一并查出来的矛盾）：
     *   **本帧一台妖侧子机都没有时不许认领 set**。理由：SDK 的止射闸（`on_native_shot_suppress`）
     *   只在**没有卡认领 set** 时才会被问到（`sdk.c`：先问 `on_shot_set_index`，有卡覆盖了就不抑制）
     *   ⇒ 今天只要聚焦就认领 ⇒ 用户 2026-09-12 那条口径「**0 个子机时宁可打原生弹、也别一发都没有**」
     *   在聚焦态**从来没生效过**（早苗档 0 = 0 台：认领了我们的行，行里的膛口一台都没有 ⇒ 一发不发）。
     *   现在两处用**同一个判据**（`yk_option_wanted`，与子机声明层同表同帧）⇒ 认领 ⟺ 有膛口 ⟺ 不抑制。*/
    if (!yk_option_wanted(1, yk_current_character(), yk_current_power_level())) {
        if (!s->logged_pass0) {
            s->logged_pass0 = 1;                              /* 只记一次（否则每帧一行）*/
            ce_log("youkai: focused with 0 youkai options at tier %d (char %d) - NOT claiming our "
                   "shot set either, so the NATIVE rows keep firing (user ruling 2026-09-12); "
                   "our option rows would have no muzzle at all", power_level, yk_current_character());
        }
        return -1;
    }
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
 *     逼出来的，用户 2026-09-12 拍了「宁可打原生弹，也别一发都没有」。
 *
 * ★ 2026-09-25（卫生）：本函数**已退订**（2026-09-22 P7 妖侧主炮改成「认领 set + 逐行」之后就不再挂进
 *   `.on_native_shot_suppress`，见本文件尾「占位清单」）⇒ 保留为**历史参考**，并按 `trance_bgm.c:234`
 *   的先例显式标 `unused`，让仓库的「0 警告」卫生要求过。**不要再挂回去**：现在妖侧靠 set 认领。*/
static int on_native_shot_suppress(ce_card_t *c) __attribute__((unused));
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
    /* ★★★ 2026-09-22 夜（owner **P-D 缩水版**）：击破魔使的三条一手（逐条见 `youkai_core.h`）：
     *   ① **扣槽 = −gauge/12**（`th08:0x42b213`–`0x42b223`，除数立即数 `0xc`；**不是 10%，也没有妖梦 5%**
     *      —— owner 已裁「按一手」）；② **掉 1 颗刻符（state 1 = 生成即满速归航）**（`th08:0x42b29b`）；
     *   ③ **50 帧内收刻符不给槽**（`th08:0x42b240 Timer(0x18b89d4).set(50)`）。*/
    {
        int32_t hl3, yl3;
        yk_gauge_limits_solo(CE_PLAYER_FOCUSED() ? 1 : 0, &hl3, &yl3);
        s->gauge = yk_gauge_add2(s->gauge, yk_gauge_fam_kill_delta(s->gauge), hl3, yl3);
        yk_orb_spawn(s, ev->x, ev->y, YK_ORB_STATE_SPAWN);   /* ★ §BE #117：type 7 ⇒ 强制 state 3 */
        s->orb_silence = YK_ORB_SILENCE_FRAMES;
    }
    /* ★ #27 支 1：**「+1」弹字**（一手 `Enemy::sub_42adb0 th08:0x42b296 CreateTimePopup(&enemy+0x2d88, 1, 0, 0xFFFFFFFF)`
     * —— 一手是「击破魔使」那一刻；我们卡里没有 op90-92 那种魔使，**我们的使魔 = 同一位置的对应物** ⇒ 挂这里）。*/
    yk_popup_spawn(ev->x, ev->y, 1, 1);
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

/* ★★★ 2026-09-22 夜（P-F 工单 · 擦弹颗数的保真缺口）：**擦弹那一路给几颗**的几本账。
 * 全部只进日志（实跑判据），不参与任何判定；`n` 的值域与那几道门见 `youkai_core.h` §3.11c。*/
static unsigned s_graze_n[4];        /* [1]/[2]/[3] = 各档各给过几次 */
static unsigned s_graze_gate[5];     /* 按判据码（`YK_GRAZE_GATE_*`，0..4）各遇到过几次 */
static unsigned s_graze_clamped;     /* 被对账护栏夹掉的弹数（> 0 = 那一帧确实跳帧了） */
static int      s_graze_log_gate = -1;   /* 上一次打日志时的判据码（值一变就打一行）*/
static int      s_graze_log_n = -1;      /* 上一次打日志时的颗数 */
static uint32_t s_graze_log_frame;       /* 上一次打日志的帧号（值不变时最多每 60 帧一行） */

/* ★★★ 2026-09-23（统筹拍「三条场上门按一手接上，但必须一行可回退」）：**擦弹第 1 颗的场上门开关**。
 *   1 = **一手**（默认）：按 `Player::sub_44a930` 的三条分支评 —— 机体 `th08:0x44aa9e` /
 *       `EnemyManager::is_boss_active` `th08:0x44aaac` / `GaugeIsExtremelyYoukai` `th08:0x44aaba`；
 *       ⚠ 后果：**不在 boss 战 / 槽没到 +8000 时，擦弹一颗永点都不给**（这正是原作的行为）。
 *   0 = **回退态**：三条门整段不评 = 本批交付前的「**聚焦即撒**」（门 A 的符卡门与门 B 的机体门照算）。
 * 手感不对时改这一行即可（判据码会打成 `gate=off`，日志一眼能看出开关是关的）。
 * 一手逐指令与判据全文：`youkai_core.h` §3.11c；登记 `TUNING.md` §2.0zs / `AUDIT.md` §BE 详录 #73。*/
#define YK_GRAZE_FIELD_GATES 1

/* ❓ **自定（不是一手规则）**：一手对「一帧擦到几颗弹」**没有上限** —— `BulletManager::on_tick`
 * 逐弹调一次 `Player::sub_44a930`，一帧 N 颗就是 N 次（`engine/_shared/rosters/th06-th09.md` §3.2
 * 与 `HUMAN-YOUKAI-GAUGE.md` §3bis.6 都写了「没有单帧上限」）。这一格是**对账护栏**，理由只有一条：
 * 我们读的是 TH18 的**累计**擦弹计数 `CE_ADDR_GRAZE`（`0x4ccd0c`），而本卡的 tick 在商店 / 暂停等
 * 场合不跑（AbilityManager 冻结）⇒ 恢复后那一帧的差分可能是「一段被跳过的时间」而不是
 * 「一次擦弹爆发」，**差分本身分不出这两种情形**。上限取 = 刻符池容量（一帧最多填满一次池）；
 * 夹掉的部分进 `s_graze_clamped` 并**当场打一行日志**（实跑见到那行 = 那一帧确实跳帧了）。
 * （上一版这里是「单帧上限 8 颗，防一段大擦弹一次性灌满池」，既没有一手依据、也没考虑跳帧。）*/
#define YK_GRAZE_RESYNC_MAX 64u

/* 判据码 → 日志里的名字（`gate=…`）：一眼看出**这次擦弹为什么没给**。*/
static const char *yk_graze_gate_name(int g)
{
    switch (g) {
    case YK_GRAZE_GATE_OK:       return "ok";
    case YK_GRAZE_GATE_SHOTTYPE: return "shottype";
    case YK_GRAZE_GATE_BOSS:     return "boss";
    case YK_GRAZE_GATE_GAUGE:    return "gauge";
    default:                     return "off";
    }
}

/* ============ ★★★ 2026-09-27 第二十六批（owner ⑥ 第二轮）：**魔理沙妖侧子机的真激光 = 卡侧矩形束**
 *
 * ## 为什么把引擎那条路整条换掉（一手 + 实跑，两条独立证据）
 *   · **实跑**（owner 探针日志，已定案）：`youkai/probe beam: state 2 len(+0xa0) 18.0 width(+0xa4) 16.0`
 *     ⇒ 引擎那条 `mode=2` 束一出生就被判完成（`+0x8c = 2`）、只长了一帧（18 px）=「十分短小的袖箭」。
 *   · **一手**：束的生死全在 `tick2`（`0x45fc80`）与**命中路径**里 —— 行里没写命中回调时引擎走
 *     `0x45f6a0` → `0x45f6f0`（给弹的 VM 发 `interrupt 1` + `+0x8c = 2`）⇒ 一碰敌人就死；
 *     补上 `func_on_hit = 2` 又会被 `hit2`（`0x460320`）**截到首个接触点**（= 不贯通）。
 *   · 117 当年在 TH17 上有同一结论（`cards/animal.c:283`–`300`）⇒ 照它的做法：**引擎不发、卡侧发**。
 *     ⇒ 行表里那一条现在是**不可达的载体**（`start_delay = 15`，见 `append_shooterset.py` 的 M 段与
 *     `youkai_core.h` 的同一段），本文件负责**视觉 + 伤害**两件事。
 *
 * ## 视觉（每条我们自己那台子机一条束 VM）
 *   · 脚本 = `pl01.anm` 追加的 `62..66`（`YK_BEAM_SCRIPT[tier]`，形状见
 *     `assets/ability/make_youkai_bullet_art.py` 的 `gen_beam`）；**从 `pl01.anm` 起**
 *     （`yk_player_anm()` = `player+0x0c`）—— 贴图 `YKB_M_OPT.png` 就长在那个文件的图集里。
 *   · 每帧写：位置 = **子机膛口**（`ce_option_x/y` = `option+0x5c/+0x60` 定点 → 像素）、
 *     方向 = **−π/2**（一手 `ply01as.sht` 那一行的 `+0x14`；脚本里不 bake 角度、也不写 `rotateAuto`）、
 *     长度 = **恒 448**（一手 `th08:0x450a4f`）⇒ `scale.x = 448/256`；
 *     宽度 = **逐档**（`YK_M_LASER_BEAM_W_PX100`）× 展开系数。
 *   · **展开**照 117 的细节「如收回般（倒放）展开」：束脚本 `interruptLabel(1)` 的收束段是
 *     `scaleTime(8, 1, 1.0, 0.0)`（8 帧里 `scale.y → 0`）⇒ 展开就是它的倒放（`YK_M_LASER_BEAM_GROW_FRAMES`
 *     = 8 帧、**只作用在宽度**上 —— 一手的束长是常量，不像 117 那样两轴同涨）。
 *
 * ## 伤害（矩形伤害源，每 2 帧一笔）
 *   · `ce_damage_rect(center, 0.0f, 2, dmg, w, 448)`（`th18:0x45dfa0`）：`h = 448` 沿 y、`w` 沿 x，
 *     angle 0 ⇒ 竖直的长条（判定轴约定见 `AUDIT.md` §U10）。
 *   · 节奏 = **每 2 帧新建一笔**（一手 TH08 每 2 帧结算一次，`th08:0x4517a1`）；
 *     而一个源对**每个敌人一辈子只结算一次**（`0x45f0f0` 的 `+0x84` tag，一手
 *     `engine/enemy/th18/06-enemy-inert-flag.md` §③ 的 `0x45f45a`）⇒ 平均 = 表值 / 2 每帧。
 *   · **不消弹**（一手：TH08 那条 `type=5` 在碰撞 pass 里被跳过销毁；同 117 的 bug-2(b)）。
 *   · **不抬每帧伤害上限**：一手 `pl01.sht+0x28` = **160/帧/敌**，我们这一档的束 ≤ 14 点/2 帧
 *     + 本体弹 ≤ 44 点/5 帧 ⇒ 远在额度内。117 抬到 300 是因为它 5 条 × 20/帧；
 *     这里**故意不抬**（`ce_raise_damage_cap` 改的是全局额度、会影响别人的卡）。
 *
 * ## 生命周期（`METHOD.md` 铁律二：被删掉会怎样）
 *   · 松 Z ⇒ `yk_beam_tick` 自己看到（`yk_shooting()` false）⇒ `interrupt 1`（收束段）后**忘掉句柄**
 *     （忘掉才轮到脚本的 `scaleTime` 讲话 —— 我们每帧写 scale 会覆盖它）；
 *   · 松 Shift（人侧）/ 换机体 / 换档 ⇒ 同一支 tick 收；
 *   · **子机撤走**（`yk_option_release`）⇒ **硬删**（`ce_anm_delete`）：膛口没了，束不能留在原地；
 *   · **删卡 / 换关**（`on_delete` / `on_run_reset` 都调 `yk_option_release`）⇒ 硬删 ——
 *     商店里 tick 全停，只有 `on_delete` 跑得到 ⇒ 收尾**不依赖 tick**。*/
#define YK_BEAM_ANM_LAYER 15    /* 世界层 3..19（一手 `engine/anm/th18/02-render-stages.md`）。
                                 * 13 = 与 117 的鹫束（`ability.anm` script163）同层，那条已实跑验收过 */
/* 逐档束脚本（`pl01.anm` 追加 62..66；宏名由 `make anm` 写进 `pl_anm_ids.h`）。
 * 下标 = 火力档 0..4，与 `append_shooterset.py` 的 `M_BEAM_ANM` 同序（不是 `tier-1` —— 那条约定是早苗的）。*/
static const int32_t YK_BEAM_SCRIPT[YK_M_LASER_BEAM_TIERS] = {
    CE_PL_ANM_SCRIPT_YKBULLET_M_BEAM0_PL01, CE_PL_ANM_SCRIPT_YKBULLET_M_BEAM1_PL01,
    CE_PL_ANM_SCRIPT_YKBULLET_M_BEAM2_PL01, CE_PL_ANM_SCRIPT_YKBULLET_M_BEAM3_PL01,
    CE_PL_ANM_SCRIPT_YKBULLET_M_BEAM4_PL01,
};
/* 这张表按**火力档**索引（`yk_current_power_level()` = `yk_shot_tier()`，它把 0..4 之外的钳进表内）
 * ⇒ 档数与表长必须相等，否则取脚本会越界（生成物重排时当场编译失败，照本仓的 `_Static_assert` 先例）。*/
_Static_assert(YK_M_LASER_BEAM_TIERS == YK_SHOT_TIERS,
               "beam script table is indexed by the shot tier (0..YK_SHOT_TIERS-1)");
/* 束的状态：文件级静态（照本卡 `s_ring` / `s_orb` / `yk_fam_vm_t` 那一族 —— 卡是单例）。
 * 不放 `yk_card_t`：那个结构已经 468 / 496 字节（`_Static_assert` 就在文件尾），只剩 28 字节。*/
static uint32_t s_beam_vm[YK_M_LASER_BEAM_MAX];   /* 每条束的 VM id（0 = 没起来）*/
static uint16_t s_beam_age;                       /* 本次激活的帧数（展开系数 + 结算节拍都用它）*/
static uint8_t  s_beam_live;                      /* 本帧起/刷了几条（诊断 + 日志判据）*/
static uint8_t  s_beam_logged;                    /* 「束起来」只记一行（几何自述）*/
static int32_t  s_beam_tier;                      /* 当前这几条束是按哪一档起的（-1 = 没起）*/

/* 收干净（`fade=1` = 走脚本的收束段；`fade=0` = 当场删）。返回收掉几条。*/
static int yk_beam_kill(int fade)
{
    int i, n = 0;
    for (i = 0; i < (int)YK_M_LASER_BEAM_MAX; ++i) {
        if (!s_beam_vm[i]) continue;
        ++n;
        if (fade) ce_anm_interrupt(s_beam_vm[i], 1);   /* 束脚本 interruptLabel(1)：scaleTime(8,1,1,0) → delete */
        else      ce_anm_delete(s_beam_vm[i]);
        s_beam_vm[i] = 0;
    }
    if (n)
        ce_log("youkai/beam: off (%d beam(s), fade %d, age %u, tier %d) - "
               "mode=2 engine beams are gone for good, so this is the only place they are cleaned",
               n, fade, (unsigned)s_beam_age, (int)s_beam_tier);
    s_beam_age = 0;
    s_beam_live = 0;
    s_beam_logged = 0;
    s_beam_tier = -1;
    return n;
}

/* 每帧：妖侧（低速）+ 魔理沙 + 按住 Z ⇒ 在**每台我们自己的子机**的位置挂/刷新一条竖直束；
 * 一台都没有（池满 / 声明没落笔）⇒ 回退本体 1 条（117 的同款兜底：「否则看着像死掉」）。
 * 其余情况全收（含松 Z）。
 * ⚠ `noinline` 是**必须**的（本仓踩过两次）：本函数内联着 `ce_damage_rect`（`sdk.h:172` 的内联汇编），
 *   而唯一调用者 `on_tick_2` 已经很大 ⇒ 一内联就报
 *   「inline assembly requires more registers than available」（i686 寄存器不够）。
 *   先例 = 同文件的 `yk_fam_vm_check` / `yk_spiral_tick`。*/
static void __attribute__((noinline)) yk_beam_tick(yk_card_t *s)
{
    const int character = yk_current_character();
    const int tier = yk_current_power_level();
    const float *pp;
    const int w100 = yk_m_laser_beam_w_px100(tier);
    const int dmg = yk_m_laser_beam_damage(tier);
    const float w_px = (float)w100 * 0.01f;
    float grow;                      /* 展开系数（每次激活从 0 到 1）——**每帧重算**：见下面的 spawn 分支 */
    int i, n = 0, spawned = 0;

    if (!s || !CE_PLAYER() || !CE_PLAYER_FOCUSED() || character != YK_CHAR_MARISA || !yk_shooting()) {
        yk_beam_kill(1);
        return;
    }
    /* 妖侧 shooterset 没装（`make sht` 没跑 / 那一格是空组）⇒ 我们这一行的**数据**都不在，
     * 别画一条没有对应弹的束（判据与 `on_tick_shooters` 的装载自检同源）。*/
    if (!yk_youkai_shotset_installed(yk_youkai_shotset_pick(character, tier))) {
        yk_beam_kill(1);
        return;
    }
    pp = ce_player_pos();
    if (!pp) { yk_beam_kill(1); return; }
    /* 换档 ⇒ 脚本号变了（脚本内容一样，但别让「行里记的档」与卡侧悄悄错开）⇒ 先硬收，本帧重起。*/
    if (s_beam_tier >= 0 && s_beam_tier != tier) yk_beam_kill(0);
    /* ⚠ `grow` 必须在这里、**每帧**从 `s_beam_age` 现算（不能提到函数开头）：上面那几支
     *   「收掉 / 重起」都会把 age 归零 ⇒ 提前算出来的系数会是**上一段激活**的残留
     *   （第一版就是这么写的：重起那一帧画的是旧 age 的宽度）。117 的 `eagle_beam_grow` 也是
     *   在 spawn 之后才取。*/
    grow = (float)yk_m_laser_beam_grow_permille(s_beam_age) * 0.001f;

    {
        int have = 0;
        for (i = 0; i < YK_OPTION_MAX; ++i) if (s->opt[i].ptr) ++have;
        if (have == 0) {
            /* 回退：本体正上方（117：「子机数为 0（火力 <1.00）时才回退成自机本体的一条」）*/
            uint32_t *id = &s_beam_vm[YK_M_LASER_BEAM_MAX - 1];
            if (!*id || !ce_anm_get_vm(*id)) {
                *id = ce_anm_spawn(yk_player_anm(), YK_BEAM_SCRIPT[tier], YK_BEAM_ANM_LAYER);
                if (!*id) { s_beam_live = 0; s_beam_tier = tier; return; }   /* 起不出来 ⇒ 本帧没束 */
                spawned = 1;
                s_beam_age = 0;
                grow = 0.0f;      /* 这一帧 = 激活的第 0 帧 ⇒ 展开系数 0（与 117 同款：出生帧不可见）*/
            }
            (void)ce_anm_set_pos(*id, pp[0], pp[1], 0.0f);
            (void)ce_anm_set_rotation(*id, 0.0f, 0.0f, -YK_ATAN_HALF_PI);
            (void)ce_anm_set_scale(*id, YK_LASER_LEN / YK_M_LASER_BEAM_SPRITE_W,
                                   w_px / YK_M_LASER_BEAM_SPRITE_H * grow);
            if (yk_m_laser_beam_should_settle(s_beam_age)) {
                float center[3] = { pp[0], pp[1] - YK_LASER_LEN * 0.5f, 0.0f };
                (void)ce_damage_rect(center, 0.0f, YK_M_LASER_BEAM_SETTLE, dmg, w_px, YK_LASER_LEN);
            }
            n = 1;
        } else {
            for (i = 0; i < YK_OPTION_MAX; ++i) {
                const uint8_t *o = s->opt[i].ptr;
                float x, y;
                if (!o) continue;
                /* 膛口 = 引擎每帧算好的那一对（一手：行循环按行里 `+0x20` 取的就是它，`th18:0x45ec41`）。
                 * 还没算过（两格都是 0）⇒ 用「自机位 + 本格的聚焦偏移」兜底（与膛口镜像那一段同款）。*/
                x = ce_option_x(o);
                y = ce_option_y(o);
                if (x == 0.0f && y == 0.0f) {
                    x = pp[0] + (float)*(const int32_t *)(o + CE_OPT_OFF_F_X) * CE_SUBPIXEL_TO_PIXEL;
                    y = pp[1] + (float)*(const int32_t *)(o + CE_OPT_OFF_F_Y) * CE_SUBPIXEL_TO_PIXEL;
                }
                if (!s_beam_vm[i] || !ce_anm_get_vm(s_beam_vm[i])) {
                    s_beam_vm[i] = ce_anm_spawn(yk_player_anm(), YK_BEAM_SCRIPT[tier], YK_BEAM_ANM_LAYER);
                    if (!s_beam_vm[i]) continue;
                    spawned = 1;
                    s_beam_age = 0;
                    grow = 0.0f;  /* 同上：这一帧就是激活的第 0 帧 */
                }
                (void)ce_anm_set_pos(s_beam_vm[i], x, y, 0.0f);
                (void)ce_anm_set_rotation(s_beam_vm[i], 0.0f, 0.0f, -YK_ATAN_HALF_PI);
                (void)ce_anm_set_scale(s_beam_vm[i], YK_LASER_LEN / YK_M_LASER_BEAM_SPRITE_W,
                                       w_px / YK_M_LASER_BEAM_SPRITE_H * grow);
                if (yk_m_laser_beam_should_settle(s_beam_age)) {
                    float center[3] = { x, y - YK_LASER_LEN * 0.5f, 0.0f };
                    (void)ce_damage_rect(center, 0.0f, YK_M_LASER_BEAM_SETTLE, dmg, w_px,
                                         YK_LASER_LEN);
                }
                ++n;
            }
        }
    }
    s_beam_live = (uint8_t)n;
    s_beam_tier = tier;
    ++s_beam_age;
    if (!s_beam_logged || spawned) {
        s_beam_logged = 1;
        ce_log("youkai/beam: on (%d beam(s) at tier %d, script %d, layer %d) - visual 448 x %.1f px "
               "(scale %.3f x %.3f), hit rect 448 x %.1f px, %d dmg every %d frame(s), no cancel; "
               "one per our own option (TH08 shape: `th08:0x450840` spawns 16x448 bars every frame)",
               n, tier, (int)YK_BEAM_SCRIPT[tier], (int)YK_BEAM_ANM_LAYER, (double)w_px,
               (double)(YK_LASER_LEN / YK_M_LASER_BEAM_SPRITE_W),
               (double)(w_px / YK_M_LASER_BEAM_SPRITE_H), (double)w_px, dmg,
               (int)YK_M_LASER_BEAM_SETTLE);
    }
}

/* ★★★ 2026-09-27（owner ②③ 实跑：「**魔理沙妖子机激光依旧没有射出去只有一小节就截断了**」+
 * 「**咲夜妖子机的魔法阵没有动**」）：这两条都在引擎内部（束的几何由 tick2 的 `+0xa0` 驱动；
 * 子机的旋转到底走不走 VM 的 rotation 字段）—— 静态读码到头了 ⇒ 按本仓的探针纪律打**读数**，
 * 让 owner 那一局把数字带回来（一次跑就能定案）。每 120 帧一行、只在妖侧。*/
static void yk_probe_tick(yk_card_t *s, int character)
{
    uint32_t fr = ce_sdk_frame();
    if (!s || (fr % 120u) || fr < 240u) return;
    if (s->opt[0].ptr) {
        uint8_t *o = s->opt[0].ptr;
        uint32_t vm = *(const uint32_t *)(o + CE_OPT_ANM_ID);
        uint8_t *v = vm ? (uint8_t *)ce_anm_get_vm(vm) : 0;
        ce_log("youkai/probe char %d age %u: opt0 vm %08x live %d rot(%.3f,%.3f,%.3f) scale(%.2f,%.2f) "
               "face %.3f orbit(%.2f,%.2f) vmPos(%.1f,%.1f) anchor(%d,%d) frozen %d",
               character, (unsigned)s->opt_age, (unsigned)vm, v ? 1 : 0,
               v ? (double)*(const float *)(v + CE_ANM_VM_ROTATION) : 0.0,
               v ? (double)*(const float *)(v + CE_ANM_VM_ROTATION + 4) : 0.0,
               v ? (double)*(const float *)(v + CE_ANM_VM_ROTATION + 8) : 0.0,
               v ? (double)*(const float *)(v + CE_ANM_VM_SCALE) : 0.0,
               v ? (double)*(const float *)(v + CE_ANM_VM_SCALE + 4) : 0.0,
               (double)s->opt_face[0], (double)s->opt_orbit[0][0], (double)s->opt_orbit[0][1],
               v ? (double)*(const float *)(v + CE_ANM_VM_POS) : 0.0,
               v ? (double)*(const float *)(v + CE_ANM_VM_POS + 4) : 0.0,
               (int)*(const int32_t *)(o + CE_OPT_POS_X), (int)*(const int32_t *)(o + CE_OPT_POS_Y),
               (int)s->opt_frozen[0]);
    }
    if (character == YK_CHAR_MARISA) {
        /* ★ 2026-09-27 第二十六批：束改走卡侧 ⇒ 探针也换成**新路的读数**（两条一起看）：
         *   ① 每条束 VM 的实况（id 在不在、位置、旋转、scale —— 期望 rz = −π/2、scale = 448/256 × 宽/32）；
         *   ② **载体行有没有被引擎发**：数一遍弹池里 packed set/row = 我们那条激光行的弹。
         *      必须恒 **0**（行里 `start_delay = 15` 不可达，`youkai_core.h` 那段有一手的充要证明）
         *      —— 这一行就是「引擎不再产生任何东西」的实跑判据；一旦非 0 说明行被改回可发了。*/
        const int tier = yk_current_power_level();
        const int w100 = yk_m_laser_beam_w_px100(tier);
        const int dmg = yk_m_laser_beam_damage(tier);
        uint8_t *pl = CE_PLAYER();
        int i, k, nrow = 0;

        for (i = 0; i < (int)YK_M_LASER_BEAM_MAX; ++i) {
            uint32_t id = s_beam_vm[i];
            uint8_t *v = id ? (uint8_t *)ce_anm_get_vm(id) : 0;
            if (!id) continue;
            ce_log("youkai/probe beam #%d: vm %08x live %d pos(%.1f,%.1f) rot(%.3f,%.3f,%.3f) "
                   "scale(%.3f,%.3f) age %u via=%s",
                   i, (unsigned)id, v ? 1 : 0,
                   v ? (double)*(const float *)(v + CE_ANM_VM_POS) : 0.0,
                   v ? (double)*(const float *)(v + CE_ANM_VM_POS + 4) : 0.0,
                   v ? (double)*(const float *)(v + CE_ANM_VM_ROTATION) : 0.0,
                   v ? (double)*(const float *)(v + CE_ANM_VM_ROTATION + 4) : 0.0,
                   v ? (double)*(const float *)(v + CE_ANM_VM_ROTATION + 8) : 0.0,
                   v ? (double)*(const float *)(v + CE_ANM_VM_SCALE) : 0.0,
                   v ? (double)*(const float *)(v + CE_ANM_VM_SCALE + 4) : 0.0,
                   (unsigned)s_beam_age, (i == (int)YK_M_LASER_BEAM_MAX - 1) ? "body" : "option");
        }
        if (pl) {
            for (k = 0; k < CE_PB_COUNT; ++k) {
                const uint8_t *b = pl + CE_PB_POOL + (size_t)k * CE_PB_STRIDE;
                uint32_t packed;
                if (*(const int32_t *)(b + CE_PB_STATE) == 0) continue;
                packed = *(const uint32_t *)(b + CE_PB_PACKED_ID);
                if (!yk_marisa_laser_row((int)(packed >> 8), (int)(packed & 0xffu))) continue;
                ++nrow;
            }
        }
        ce_log("youkai/probe beam: %d live / %d slot(s), tier %d (script %d, w %.2f px, %d dmg / %d frame), "
               "engine bullets from our laser row = %d (MUST be 0)",
               (int)s_beam_live, (int)YK_M_LASER_BEAM_MAX, tier,
               (int)YK_BEAM_SCRIPT[tier], (double)w100 * 0.01, dmg, (int)YK_M_LASER_BEAM_SETTLE,
               nrow);
    }
}

/* ============ ★★★ 2026-09-27（owner ④）：**妖逢魔 ⇒ 自机弹染红**（一手见 `youkai_core.h` 的同名小节）
 *
 * 一手：TH08 是「造弹那一刻按『极妖 + 该行 `+0x1e > 0`』置 `bullet+0x470`，然后**每次绘制前**
 * 把弹 VM 的 `color_1` 的 B/G/R 三字节写成 `0x40/0x40/0xff`（红），alpha 不动」——
 * 所以**在飞的弹也会当帧变色**（不是只染后发的）。
 * TH18 侧：行表里的 `+0x1e` 带不过来（`+0x1c..+0x1f` 是死字段）⇒ 用**认领的组号**判
 * （`yk_fengma_tint_set`：只有霊夢 `0x54..0x58` 与魔理沙 `0x59..0x5d` 染，与一手「只有这两位的行
 * `+0x1e > 0`」一致），然后**每帧扫弹池**写色 —— 形状照 116 `signal.c` 的同一段扫描。
 * ⚠ 只写低 24 位（保 alpha）：否则会把脚本自己的 `alphaTime` 淡出打坏。*/
static void yk_fengma_tint_bullets(void)
{
    uint8_t *pl = CE_PLAYER();
    uint8_t *pool;
    int i, n = 0;
    if (!pl) return;
    pool = pl + CE_PB_POOL;
    for (i = 0; i < CE_PB_COUNT; ++i) {
        uint8_t *b = pool + (size_t)i * CE_PB_STRIDE;
        uint32_t packed, vmid, want;
        uint8_t *vm;
        if (*(const int32_t *)(b + CE_PB_STATE) == 0) continue;         /* 空槽 */
        packed = *(const uint32_t *)(b + CE_PB_PACKED_ID);
        if (!yk_fengma_tint_set((int)((packed >> 8) & 0xffu))) continue; /* 不是那两位的妖侧组 */
        vmid = *(const uint32_t *)(b + CE_PB_VM_ID);
        vm = vmid ? (uint8_t *)ce_anm_get_vm(vmid) : 0;
        if (!vm) continue;
        want = yk_fengma_tint_dword(*(const uint32_t *)(vm + CE_ANM_VM_COLOR1));
        if (*(const uint32_t *)(vm + CE_ANM_VM_COLOR1) == want) continue;   /* 已经染过（幂等）*/
        ce_anm_set_color(vmid, want);
        ce_anm_set_color2(vmid, want);                                   /* 两路同写（本仓纪律）*/
        ++n;
    }
    (void)n;
}

static int on_tick_2(ce_card_t *c)
{
    yk_card_t *s = ce_state(c, yk_card_t);
    int focused;
    if (!s) return 0;
    if ((++s_ledger_frame % 600u) == 0u && (s->spawns || s->broken || s->lost || s->silent))
        ce_log("youkai: ledger - %u spawned, %u killed, %u withdrew, %u silent, %u dropped, "
               "%u body-emitter slot(s) skipped (#37: no familiar when the emitter is the enemy itself)",
               s->spawns, s->broken, s->lost, s->silent, s->dropped, s->emit_body_skipped);
    if (s_ring_failed && (s_ledger_frame % 600u) == 0u)
        ce_log("youkai: switch ring VM spawn FAILED %u time(s) total (cosmetic only - the Shift "
               "magic circle was missing)", s_ring_failed);
    /* ★★★ 2026-09-26（owner 实跑「妖侧子机不发弹 / 只有一行 / 灵梦子机不旋转」那一批）：
     *   **逐格实况行**（每 120 帧一行、只在低速且手里有子机时打）—— 这一批的判据全在这一行里：
     *     · `want/have`：本帧该有几台 / 真拿到几台（**行里的膛口就靠这两半**）；
     *     · 每格的 `opt`（我们那台子机**看得见的**位置，`option+0x5c/+0x60`）与
     *       `muz`（**原生槽 n 的膛口**，= 行循环 `th18:0x45ec41` 真正会用的那一对）
     *       ⇒ 两者应当**逐帧几乎相等**（原生槽是每帧镜像过来的，差几 px 是引擎那支 lerp 的滞后）；
     *     · `state`：原生槽 n 的 `+0x00`（`0` = 引擎没给这一档建记录 ⇒ 全靠我们写的膛口）；
     *     · `chase`/`age`/`enters`：灵梦那支的追敌/旋转门（`age >= 10` 才准进）；
     *     · `set`：本帧认领的妖侧 shooterset 槽号（认领没生效 ⇒ 打的是原生 set）。
     *   ⚠ 判「子机不发弹」时看 `muz`：仍是 `(0,-400)` 或与 `opt` 差很远 ⇒ 镜像没生效；
     *     `want>0` 而 `have==0` ⇒ 池满（日志会有 `option allocated FAILED (pool full)`）。*/
    if ((s_ledger_frame % 120u) == 0u && CE_PLAYER_FOCUSED()) {
        int nopt = 0, k;
        uint8_t *pl = CE_PLAYER();
        for (k = 0; k < YK_OPTION_MAX; ++k) if (s->opt[k].ptr) ++nopt;
        if (nopt) {
            ce_log("youkai/opt: char %d want %d have %d age %u chase %d/%d(id %u) enters %u set 0x%02x",
                   yk_current_character(),
                   yk_option_count(1, yk_current_character(), yk_current_power_level()), nopt,
                   (unsigned)s->opt_age,
                   (int)s->opt_chase, (int)s->chase_have, (unsigned)s->chase_id,
                   (unsigned)s->chase_enters, (unsigned)s->claimed_set);
            if (pl) {
                for (k = 0; k < nopt && k < CE_NATIVE_OPT_COUNT; ++k) {
                    const uint8_t *o = s->opt[k].ptr;
                    const uint8_t *ns = pl + CE_NATIVE_OPT_PLAYER_MUZ0
                                      + (size_t)k * CE_NATIVE_OPT_STRIDE;
                    /* ★ 子机 VM 的**当前旋转角**（`zAnmVmPrefix` 的 `rz` —— 一手 `engine.h` 的
                     *   `CE_ANM_VM_ROTATION 0x3c`，自机弹那条写 `+0x44`）。灵梦那支「应当旋转」的
                     *   判据就是**这一格在不在涨**：旋转段 `angleVel(0,0,0.62831855)` ⇒ 每帧 +0.628。
                     * ★★ 2026-09-26 **追加「渲染模式」**（`mode`）—— 这才是「rz 在涨但画面上不转」的判据：
                     *   模式 = `flags_lo(+0x534)` 的 bit26-30（只有 `type()` opcode 写它，
                     *   一手 `th18:0x47b12c`）；`draw_vm` `th18:0x481210` 按它分派 —— **模式 0** 走
                     *   `0x47e8f0`（整段没有旋转读取、没有 fsincos ⇒ 永远轴对齐），**模式 1/3** 走
                     *   `0x47ed50`（读 rz + fsincos ⇒ 真的转）。我们的脚本是从 TH08（v3 没有渲染模式）
                     *   照抄的 ⇒ 必须显式 `type(1)`（见 `assets/ability/make_youkai_option_art.py` 的文件头）。*/
                    /* ★★ 2026-09-26：**`wz` 才是「旋转有没有在跑」的判据，`rz` 不是** —— 引擎每帧
                     *   把 ωz 积分进 rz（`0x47bdad`→`0x47bdc5`→`0x47bdfe`）并按 ±π wrap 回绕，
                     *   而我们每 120 帧才打一行：灵梦那支 ωz = 0.6283/帧 = **10 帧一圈** ⇒ 120 帧正好
                     *   **12 圈整** ⇒ `rz` 逐次读数几乎不变（不是说没转）。判据是两条一起看：
                     *     · `wz ≈ 0.628` ⇒ 旋转段（`interruptLabel(5)` 那段 `angleVel`）**在跑**；
                     *     · `mode == 1` ⇒ 绘制走的是**会读 rz 的那条路**（模式 0 完全忽略旋转，见 §BE #95）。
                     *   两条同时成立 = 画面上确实在转（36°/帧）。*/
                    const uint8_t *vm = ce_anm_get_vm(*(const uint32_t *)(o + CE_OPT_ANM_ID));
                    ce_log("youkai/opt #%d state %d opt(%d,%d) muz(%d,%d) rz %.3f wz %.4f mode %d spr %d vm %08x",
                           k, *(const int32_t *)(ns + CE_OPT_IN_USE),
                           *(const int32_t *)(o + CE_OPT_POS_X), *(const int32_t *)(o + CE_OPT_POS_Y),
                           *(const int32_t *)(ns + CE_OPT_POS_X), *(const int32_t *)(ns + CE_OPT_POS_Y),
                           vm ? (double)*(const float *)(vm + CE_ANM_VM_ROTATION + 8) : 0.0,
                           vm ? (double)*(const float *)(vm + CE_ANM_VM_ANGVEL + 8) : 0.0,
                           vm ? (int)((*(const uint32_t *)(vm + CE_ANM_VM_FLAGS_LO) >> 26) & 0x1fu) : -1,
                           vm ? *(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID) : -1,
                           (unsigned)*(const uint32_t *)(o + CE_OPT_ANM_ID));
                }
            }
        }
    }
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
                   "speed=%.0f pick=%.0f shoot=%d fengma=%u fscore=%d "
                   "graze_n 1/2/3=%u/%u/%u graze_gate ok/boss/gauge/shottype/off=%u/%u/%u/%u/%u clamped=%u",
                   (int)s->gauge, (int)s->piv, (int)s->orbs, s->orb_taken, s_orb_live,
                   s->orb_full, (unsigned)s->hud_ready, sp, rr, yk_shooting(),
                   (unsigned)s->fengma, (int)s->fengma_score,
                   s_graze_n[1], s_graze_n[2], s_graze_n[3],
                   s_graze_gate[YK_GRAZE_GATE_OK], s_graze_gate[YK_GRAZE_GATE_BOSS],
                   s_graze_gate[YK_GRAZE_GATE_GAUGE], s_graze_gate[YK_GRAZE_GATE_SHOTTYPE],
                   s_graze_gate[YK_GRAZE_GATE_OFF], s_graze_clamped);
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
    /* ★★★ 2026-09-23（owner：妖侧「只有本体在发弹、只有一列（正常两列）」，四机体都是「子机问题」）：
     *   **机制诊断**（每次进妖侧打一行）：TH18 主炮行循环按行里的 `+0x20` 选膛口 —— `slot 0` = 自机本体，
     *   `slot >= 1` = **`PLAYER + 0x580 + slot*0xf0` = 引擎自己的 4 格原生子机**（一手 `0x45ec4c`–`0x45ec5d`；
     *   `engine.h` 的 `CE_NATIVE_OPT_*` 一节），**不是**卡申请的池（`inner+0x410`）。而我们这套 TH08 的行
     *   正是按 TH08 的子机编号写的（`+0x20` = 0..4）⇒ 那些行要的就是**原生那 4 格**。
     *   可我们**恰恰在低速把它们藏了**（`ce_native_option_hide_declare` + 每帧 `sweep` + 隐藏断点把 `lvl`
     *   压 0 ⇒ 连记录都不建）⇒ 若这一行打出「4 格全是 0/状态 0」，机制就当场坐实，修法见下面那条待办。
     *   ⚠ 只读、每进一次妖侧一行（不刷屏）。*/
    if (!focused && !s_natopt_probe_done) {
        /* ★ 2026-09-23：**高速**也探一次（`focused` 那一支探的是「藏起来了没」；这一支探的是
         *   「松 Shift 后它们回没回来、位置动不动」）—— 判「滞留」到底是原生那 4 格还是我们的。*/
        uint8_t *pl = CE_PLAYER();
        s_natopt_probe_done = 1;
        if (pl) {
            int k, live = 0;
            for (k = 0; k < CE_NATIVE_OPT_COUNT; ++k) {
                uint8_t *o = pl + CE_NATIVE_OPT_PLAYER_MUZ0 + (size_t)k * CE_NATIVE_OPT_STRIDE;
                if (*(const int32_t *)(o + CE_NATOPT_STATE) == 2 ||
                    *(const uint32_t *)(o + CE_NATOPT_VM_MAIN))
                    ++live;
            }
            ce_log("youkai: native option slots at HIGH speed - %d/4 live (these are what the human "
                   "side shows and what the row loop's slot>=1 fires from)", live);
            for (k = 0; k < CE_NATIVE_OPT_COUNT; ++k) {
                uint8_t *o = pl + CE_NATIVE_OPT_PLAYER_MUZ0 + (size_t)k * CE_NATIVE_OPT_STRIDE;
                ce_log("youkai:   native opt slot %d: state %d vm %08x muzzle (%.1f, %.1f)", k + 1,
                       *(const int32_t *)(o + CE_NATOPT_STATE),
                       (unsigned)*(const uint32_t *)(o + CE_NATOPT_VM_MAIN),
                       (double)(*(const int32_t *)(o + CE_NATOPT_POS_X) * CE_SUBPIXEL_TO_PIXEL),
                       (double)(*(const int32_t *)(o + CE_NATOPT_POS_Y) * CE_SUBPIXEL_TO_PIXEL));
            }
        }
    }
    if (focused && !s_natopt_probe_done) {
        uint8_t *pl = CE_PLAYER();
        s_natopt_probe_done = 1;
        if (pl) {
            int k, live = 0;
            for (k = 0; k < CE_NATIVE_OPT_COUNT; ++k) {
                uint8_t *o = pl + CE_NATIVE_OPT_PLAYER_MUZ0 + (size_t)k * CE_NATIVE_OPT_STRIDE;
                if (*(const int32_t *)(o + CE_NATOPT_STATE) == 2 ||
                    *(const uint32_t *)(o + CE_NATOPT_VM_MAIN))
                    ++live;
            }
            ce_log("youkai: native option slots at low speed - %d/4 live (state==2 or a VM). Our TH08 "
                   "rows fire their slot>=1 rows from THIS array (0x45ec4c), NOT from the card pool; "
                   "if this is 0/4 the option half of the set cannot fire = 'only one column'", live);
            for (k = 0; k < CE_NATIVE_OPT_COUNT; ++k) {
                uint8_t *o = pl + CE_NATIVE_OPT_PLAYER_MUZ0 + (size_t)k * CE_NATIVE_OPT_STRIDE;
                ce_log("youkai:   native opt slot %d: state %d vm %08x muzzle (%.1f, %.1f)", k + 1,
                       *(const int32_t *)(o + CE_NATOPT_STATE),
                       (unsigned)*(const uint32_t *)(o + CE_NATOPT_VM_MAIN),
                       (double)(*(const int32_t *)(o + CE_NATOPT_POS_X) * CE_SUBPIXEL_TO_PIXEL),
                       (double)(*(const int32_t *)(o + CE_NATOPT_POS_Y) * CE_SUBPIXEL_TO_PIXEL));
            }
        }
    }
    if (s->hud_ready) {
        ce_hud_slot_get(ce_card_id(c), &s->hud);           /* 每帧取最新锚点（同批后来的认领会重排）*/
        yk_gauge_hud_spawn(c, s);                          /* 自愈：VM 被引擎清过就重建 */
        yk_gauge_hud_update(s);
        /* ★ 2026-09-23（#30「又看不见人妖槽」）：每 60 帧一次逐 VM 实况（与 112 那条同款）。
         *   静态计数（不用 play_frame —— 后者在关卡开头若干帧是 0，会连打几十行）。 */
        /* ★★ 2026-09-23（owner：「人妖槽好像在整个画面的**上方**被挡住了」）：
         *   上一版这一族只挂在 **60 秒**节拍上（下面那支）⇒ owner 那两局**一行都没打**（局太短），
         *   于是「我们写进去的数」有证据（几何自述：anchor (257,924) bar (257,860) ✓ 左下角），
         *   而「**实际画在哪**」没有证据 ⇒ 判不出是「写错」还是「画到别的 pass 去了」。
         *   ⇒ 补一支 **HUD 起来时必打一次**（`s->hud_vm_up_logged`，每关/每次重建各一次）。
         *   判据：`draw` 那一列若 = 我们写的 (257,860) ⇒ 数值生效、问题在绘制组/pass；
         *         若 = (0,0) 之类 ⇒ 写没落到 VM 上（句柄陈旧 / VM 被引擎重建过）。*/
        if (s->hud_bar && !s->hud_vm_up_logged) {
            s->hud_vm_up_logged = 1;
            yk_hud_vm_log("bar", s->hud_bar);
            yk_hud_vm_log("human", s->hud_human);
            yk_hud_vm_log("youkai", s->hud_youkai);
            yk_hud_vm_log("cursor", s->hud_cursor);
        }
        if (++s_hud_vm_log_tick >= 3600u) {
            int q;
            s_hud_vm_log_tick = 0;
            yk_hud_vm_log("bar", s->hud_bar);
            yk_hud_vm_log("human", s->hud_human);
            yk_hud_vm_log("youkai", s->hud_youkai);
            yk_hud_vm_log("cursor", s->hud_cursor);
            for (q = 0; q < YK_GAUGE_TEXT_MAX; ++q) yk_hud_vm_log("digit", s->hud_digit[q]);
        }
    }
    yk_orbs_tick(s);
    {
        uint32_t graze = *(const uint32_t *)(uintptr_t)CE_ADDR_GRAZE;
        if (graze > s->graze_seen) {
            uint32_t d = graze - s->graze_seen, i;
            unsigned k, orbs;
            int spell, st, gate, boss;
            float px = *(const float *)(CE_PLAYER() + CE_PLAYER_X);
            float py = *(const float *)(CE_PLAYER() + CE_PLAYER_Y);
            if (d > YK_GRAZE_RESYNC_MAX) {                 /* ❓ 自定：**对账护栏**，不是一手规则
                                                            *   （一手「没有单帧上限」的证据与理由见
                                                            *   `YK_GRAZE_RESYNC_MAX` 的注释）*/
                s_graze_clamped += d - YK_GRAZE_RESYNC_MAX;
                ce_log("youkai: graze resync clamped %u bullet(s) (cumulative %u) - the cumulative "
                       "TH18 graze counter jumped, i.e. our tick did not run for a while; "
                       "first-hand TH08 has NO per-frame cap",
                       d - YK_GRAZE_RESYNC_MAX, s_graze_clamped);
                d = YK_GRAZE_RESYNC_MAX;
            }
            /* ★★★ 2026-09-22（P5 / th08 #18「妖形态是擦弹得自动吸收的永点……总之是有一个『无敌』的
             * 上抛过程，然后直接吸收」）：**擦弹那一路是 state 5（上抛），不是 state 1** —— 上一版
             * 认错了对象。一手（逐指令，本样本）：
             *   · 擦弹真正的落点 = `Player::sub_44a930`（`th08:0x44a930`）的
             *     `th08:0x44aac9 / :0x44aaef / :0x44ab0f` ⇒ `spawn_item(pos, **type 0xa**, state 1)`；
             *     ⚠ **不是** `Player::sub_44a6a0`：那个是 `0x44a6a0`–`0x44a928`（`ret 0x14` = 五参）的
             *     **激光判定**，它在 `th08:0x44a8d1` 以 `flag = 1` 调进 `sub_44a930`（`ret 0x8` = 两参）；
             *     vendor `funcs.json` 把这两个函数并成了一条，别照它读边界（core §3.11c 有全文）。
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
             * ⚠ 仍**未复刻**的两点（登记 TUNING §2.0y）：① 落点 —— TH08 撒在**被擦到的那颗弹**上，
             *   我们拿不到那颗弹（`ce_graze_take` 的断点 `0x42497f` 在 `sites.py` 里被注释掉 ⇒ 恒返回 0）
             *   ⇒ 落点 = 自机（上抛之后回来才被收，至少看得见动作）；
             *   ② 弹 / 激光那条 `flag`（`[ebp+0xc]`）分不出来（TH18 只有一个累计计数）。
             *
             * ★★★ 2026-09-22 夜（P-F · **颗数**的保真缺口）：一手是
             * **每擦到一颗弹给 1 / 2 / 3 颗**（几道门**嵌套**，`je 0x44ab14` 直接跳到函数尾）：
             *   · `th08:0x44aac9` = 第 1 颗（前面还有三条场上门 —— 见下面 ★ 2026-09-23 那一段）；
             *   · `th08:0x44aaef` = 第 2 颗：门 = `[ebp+0xc] == 0`（**弹**路径；激光那条传 1）
             *     **且** `SPELLCARD.flags & 1`（`th08:0x44aad9 call 0x4178a0`）= **符卡进行中**；
             *   · `th08:0x44ab0f` = 第 3 颗：门 = `!shottype_is_solo_youkai()`（`th08:0x44aaf9`）。
             * 判据 / 出处全文 = `youkai_core.h` §3.11c（含两条 shottype 谓词、`SPELLCARD.flags` bit0 的
             * 置位与清位指令、以及「一帧可以擦 N 颗」这条没有上限的一手）。TH18 侧我们这样接：
             * `spell_active = ce_spell_active()`（`zSpellcard+0x78` bit0，与 TH08 的 bit0 同义）、
             * `shottype` = TH18 机体号 0..3（owner §4 的映射把每个 TH18 机体都映到 TH08 的一个**组**
             * ⇒ 两条 solo 谓词恒假 ⇒ 本卡只会给 **1 颗**（没在打符卡）/ **3 颗**（符卡进行中）；
             * 第 2 颗那一档 = 「妖単独」机体，TH18 没有对应机体）、`bullet_path` 恒 1。
             *
             * ★★★ 2026-09-23（统筹拍「三条场上门按一手接上」）：第 1 颗前面那三条门由
             * `yk_graze_field_gate()` 评，顺序照一手 `th08:0x44aa9e`（机体）→ `:0x44aaac`（boss）
             * → `:0x44aaba`（槽极妖 >= +8000）；没过 ⇒ 这一颗弹**一颗都不给**。
             * 开关 = `YK_GRAZE_FIELD_GATES`（默认 1 = 一手；0 = 整段不评 = 「聚焦即撒」老行为）。
             * 日志 `gate=` 直接给出**是哪道门挡的**（`ok` / `boss` / `gauge` / `shottype` / `off`）。*/
            spell = ce_spell_active();
            st = yk_current_character();
            if (st < 0 || st > 3) st = 0;
            boss = yk_boss_active();          /* 本帧读一次，保证「判据用的」与「日志打的」是同一个值 */
            gate = yk_graze_field_gate(YK_GRAZE_FIELD_GATES, boss, (int)s->gauge, st);
            orbs = (unsigned)yk_graze_orbs_per_bullet(gate, 1 /* 弹路径（激光分不出来，见上面 ②）*/,
                                                      spell, st);
            if (focused) {
                for (i = 0; i < d; ++i)
                    for (k = 0; k < orbs; ++k)
                        yk_orb_spawn(s, px, py, YK_ORB_STATE_SPAWN5);   /* ★ state 5：可收 */
                /* ★ 实跑判据（统筹要的那一条）：`gate=` 一眼看出**这次擦弹为什么没给**
                 * （`ok` 全过 / `boss` 场上没 boss / `gauge` 槽没到 +8000 / `shottype` 人単独非妖夢 /
                 * `off` 开关关掉），`n=` 是**过了门之后每颗弹给几颗**（0 = 被门挡）。
                 * 限频：`gate` 或 `n` 一变就打一行；都不变时最多每 60 帧一行（免得密擦时刷屏）。*/
                if (orbs <= 3u) ++s_graze_n[orbs];
                if (gate >= 0 && gate <= 4) ++s_graze_gate[gate];
                if (gate != s_graze_log_gate || orbs != (unsigned)s_graze_log_n
                    || (uint32_t)(s_ledger_frame - s_graze_log_frame) >= 60u) {
                    s_graze_log_gate = gate;
                    s_graze_log_n = (int)orbs;
                    s_graze_log_frame = s_ledger_frame;
                    ce_log("youkai: graze orbs n=%u x%u bullet(s) gate=%s (spell=%d shottype=%d gauge=%d "
                           "boss=%d) tally 1/2/3=%u/%u/%u clamped=%u - TH08 th08:0x44aac9+aaef+ab0f",
                           orbs, d, yk_graze_gate_name(gate), spell, st, (int)s->gauge,
                           boss, s_graze_n[1], s_graze_n[2], s_graze_n[3],
                           s_graze_clamped);
                }
            }
            /* ★★★ 2026-09-22 夜（P-C ③）：**擦弹 +100 只给妖侧**（一手 `Player::sub_44a930 th08:0x44a930`：
             * `th08:0x44aa6c call 0x40bc40`（`IsYoukai()` = **`player+5`**）为真才
             * `th08:0x44aa78 push 0 / th08:0x44aa7a push 0x64 / th08:0x44aa81 call 0x43c0bb`）。
             * ⚠ 两条必须照抄的差别：① **`IsYoukai()`（`player+5`）要等换形 7 帧才置位**
             *   （`th08:0x44b1d0 cmp [player+8],7` / `:0x44b1d9 mov BYTE [player+5],1`）⇒ 我们读 **`focus_age >= 7`**；
             *   ② **刻符那笔用的是 `player+3`（`isYoukaiMode`，立即生效）** ⇒ 两笔**不同步**，别合成一条。
             * 一手恒 **+100（= 1%）**、永不取负；`param_2 = 0` 对应的是 `0x43c0c2` 那道闸 ——
             * ★ 2026-09-27 订正：那道闸读的全局**恒为 0**（见 §BE #110）⇒ 擦弹这一笔也**不做闸**。*/
            {
                int32_t hl2, yl2;
                yk_gauge_limits_solo(focused ? 1 : 0, &hl2, &yl2);
                if (focused && s->focus_age >= 7)
                    s->gauge = yk_gauge_add2(s->gauge, 100, hl2, yl2);
            }
        }
        s->graze_seen = graze;
    }
    /* ★★★ 2026-09-22 建、**2026-09-26 订正**（owner：「音效应当是**从高速进入低速**，**在有使魔的情况下**
     *   才有的，现在正好是**低速进高速**有」）：**Shift 那一下的两枚 SE**。
     *
     * 一手（`FUN_0042c420` = `th08:0x42c420`–`0x42c58e`，**这是敌人的方法**：`this` 用 `+0x2d88` 位置 /
     *   `+0x3324` flags / `+0x332f` enemyType / `+0x53c8` linkedEffect；调用点 `th08:0x42c98a`
     *   在 `EnemyManager::OnUpdate` 的逐敌循环里，门 = `enemy->flags` bit8）：
     *   · **人→妖**（`0x42c440 call 0x40bc40` 判当前已是妖）：
     *     `0x42c45f SpawnEffect(0x1f, 色 0x80303080)` + `0x42c477 push 2` → `0x42c482 call 0x407120`
     *     （= `AnmVm::SetInterrupt`，作用在该敌人的 `linkedEffect` 上）+ **`0x42c489 push 0x28`** →
     *     `0x42c490 call 0x45d550`；尾部 `0x42c498 mov BYTE [this+0x332f],0`（enemyType = 0 = 妖）。
     *   · **妖→人**（`0x42c4e1` 判当前不是妖）：`0x42c4ff SpawnEffect(0x1e, 色 0x80803030)` +
     *     `0x42c517 push 1` → SetInterrupt(linkedEffect, 1) + **`0x42c529 push 0x27`** → `0x42c530 call`；
     *     尾部 `0x42c538 mov BYTE [this+0x332f],2`（enemyType = 2 = 人）。
     *
     * ★★ **声号 → wav 名的表基准本轮订正**（这是当年把两向接反的**总根因**）：
     *   TH08 的声音表 = **`th08:0x4c8040` + id*8**，行内 `+0x00` = wav 号（`0x45d4ce mov ecx,[eax*8+0x4c8040]`）、
     *   `+0x04` = 音量（`0x45d512 movsx eax,word [edx*8+0x4c8044]`）、`+0x06` = 第三格
     *   （播放入口 `0x45d550` 读的是它：`0x45d55c movsx ecx,word [eax*8+0x4c8046]`）。
     *   **本仓原来把 `+0x06` 那张表头当成了「第二格的 wav 号」**（写 `0x4c8046`）⇒ 每一行都读成**下一行**的
     *   wav ⇒ `se_opshow` / `se_ophide` 整体错位一格。本轮自证（`python` 直读 `local/th08.v1.00d/th08.exe`）：
     *   `id 0x27 → wav 31 → 名表 0x4c81b0[31] = se_opshow.wav`、`id 0x28 → wav 32 → [32] = se_ophide.wav`、
     *   `id 0x29 → wav 33 = se_invalid.wav`（名表起点也自证：46 行 × 8 = `0x170`，`0x4c8040+0x170 = 0x4c81b0`）。
     *   ⇒ **人→妖 = id `0x28` = `se_ophide`**（TH18 没有这一枚 ⇒ 扩表 `CE_VOICE_YOUKAI_FAM_SHIFT` = `0x5d`）、
     *     **妖→人 = id `0x27` = `se_opshow`**（TH18 也没有）。
     *   ⇒ **owner 的记忆是对的**（「人转妖时敌方使魔有一枚 wav，这枚不存在于后续」），
     *     本文档此前记的「口径出入：一手是妖→人那枚才独有」是**建在错位表上的假登记**，作废。
     *
     * ⚠ 旧版还有第二处错：人→妖播的 TH18 `0x0c` **不是** `se_invalid`（本轮直读 TH18 自己的表
     *   `th18:0x4c9b80` + stride `0x14` + 名表 `th18:0x4b47a0`：`0x0c` → wav 10 = `se_select00.wav`；
     *   TH18 的 `se_invalid` 是 id `0x05`）⇒ 那句「字节与 TH18 一致」也不成立，已删。
     *
     * **播法**（本轮）：那声 `se_ophide` 由**在场的形态响应敌人**发出（一手：它是敌人的方法、门 = 使魔实体
     *   存在）⇒ 只播**人→妖**，并且门的读数必须在 `yk_familiars_tick()`（本帧真的建/撤使魔）**之后** ——
     *   旧版放在边沿分支里，那一刻读到的是上一帧（人侧）的表 ⇒ 该响的被吞、不该响的（妖→人）反倒响。
     *   妖→人一侧 TH08 播 `se_opshow`（id 0x27），TH18 没有这一枚 ⇒ 按 owner 口径**先不播**（要补就得再开一个
     *   扩表槽，登记为 🟡 待 owner 拍板；见 `TUNING.md`）。*/
    if (focused != (int)s->gauge_focus_prev) {
        float px = *(const float *)(CE_PLAYER() + CE_PLAYER_X);
        float py = *(const float *)(CE_PLAYER() + CE_PLAYER_Y);
        /* ★★★ 2026-09-22 夜（owner #23）：两下**本体**爆闪的门 = 一手 `player+8 >= 4`
         *（= 上一次形态至少持续了 4 帧，防连按；`player+8` 是切换进度计时器 0..7，见 `yk_aura_*` 的注）。
         * 读的是**这次边沿之前**的 `focus_age` ⇒ 语义与一手一致；读完这一帧归零。*/
        int flash_ok = (s->focus_age >= YK_AURA_FLASH_MIN_AGE);
        if (focused) {
            /* ★ 2026-09-26：**这一声延到本帧 `yk_familiars_tick()` 之后再播**（门要读本帧的表）。
             *   只置一个待播标记 + 记下当时的自机坐标。*/
            s->se_fam_press = 1;
            s->se_fam_press_x = px;
            s->flash_pending = 1;                 /* ★ #22：使魔重建时在它身上补一支爆闪（310）*/
            /* ★ #23：本体那一下（一手 `0x44b155 SpawnEffect(0x1d, …, 0x80ff8080)` = 我们的 313）
             * —— 一手起在**自机**坐标上（`&player+0x2b4`），不是使魔身上。*/
            if (flash_ok) yk_flash_spawn(CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_BODYFLASH58, px, py,
                                        YK_FLASH_BODY_BLUE);
            s->aura_on = 1;                       /* ★ #23：妖侧 ⇒ 光环应当在 */
            s_aura_edge_spawn = 1;                /* ★ #32/#29：**只在这个边沿**允许起一支（下一帧由 tick 消费）*/
        } else {
            /* ★ #23：妖→人那一下（一手 `0x44b3af SpawnEffect(0x1c, …, 0x808080ff)` = 我们的 314）。*/
            if (flash_ok) yk_flash_spawn(CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_BODYFLASH57, px, py,
                                        YK_FLASH_BODY_RED);
            /* ★ owner 二次订正：使魔**撤走那一刻**在它们各自的位置上闪一圈魔法阵
             *（=`「随着 shift 消失变成魔法阵」那条 #12 的形态）；环短命、自删，不留常驻环。*/
            {
                unsigned fi, nring = 0;
                for (fi = 0; fi < YK_FAM_MAX && nring < YK_RING_MAX; ++fi)
                    if (s_map.e[fi].enemy_id && s_map.e[fi].fam_id &&
                        yk_familiar_by_id(s_map.e[fi].fam_id)) {   /* #37：只给活着的使魔画环 */
                        yk_ring_blink(s_map.e[fi].x, s_map.e[fi].y, 1);   /* 1 = 人侧紫 */
                        ++nring;
                    }
            }
            yk_aura_kill(s);                      /* 光环收到 interrupt 1（30 帧淡出后自删）*/
            /* ★★ 2026-09-26（订正 #36）：**妖→人这一侧不播**。一手那一枚是 `se_opshow`（id 0x27），
             *   TH18 里没有 ⇒ 按 owner 口径（「音效应当是从高速进入低速，在有使魔的情况下才有的」）
             *   这一侧保持安静；日志照打，**判据从「两向各一行」改成「人→妖一行（有使魔时）」**。*/
            ce_log("youkai: shift SE 妖→人 - silent by design (TH08 would play id 0x27 = se_opshow, "
                   "which TH18 does not ship); owner ruling 2026-09-26: only the 人→妖 one is wanted; "
                   "familiar live count = %d", (int)yk_map_fam_live_count(&s_map));
            if (yk_map_fam_live_count(&s_map)) {
                unsigned fi;
                /* ★ #22：妖→人那一支的爆闪（311）起在**撤走前最后记下的使魔坐标**上
                 *（一手就是起在使魔身上；使魔这一帧正在被撤）。*/
                for (fi = 0; fi < YK_FAM_MAX; ++fi)
                    if (s_map.e[fi].enemy_id && s_map.e[fi].fam_id &&
                        yk_familiar_by_id(s_map.e[fi].fam_id)) {
                        yk_flash_spawn(CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_FLASH60,
                                       s_map.e[fi].x, s_map.e[fi].y, YK_FLASH_FAM_BLUE);
                        break;                     /* 一手是每只使魔各一支；我们只起一支（画面上一闪）*/
                    }
            }
        }
        s->focus_age = 0;                     /* ★ #23：一手 `player+8` 在这一帧被清零（0x44b1a5 / 0x44b3f8）*/
        /* ★★★ 2026-09-27（owner ③「松开/按下 shift 不会导致人妖值瞬间变化，甚至可能瞬间按下松开
         *   or 松开按下 人妖值可能都不变」）：换形这一帧还要**清推进速率计时器**。
         *   一手：两条边沿各有一句 `player+0xe2ae8.SetCurrent(0)`（人→妖 `th08:0x44b1b7`、
         *   妖→人 `th08:0x44b40a`，`call 0x4065f0`）—— `player+0xe2ae8` 就是本卡的 `gauge_ram`。
         *   不清的话：玩家一直按着 Z（推进支在跑、`gauge_ram` 早涨到 300 ⇒ rate = 20）时一换形，
         *   下一帧就以 ±20/帧 反向推 ⇒ 手感上正是「一按 Shift 数值立刻动起来」。
         *   ⚠ **不要**顺手清 `gauge_idle`：一手边沿**不清** `player+0xe2ad0`（它是回中支的自己那套）。*/
        s->gauge_ram = 0;
    }
    /* ★★★ 2026-09-22 夜（owner #23）：本体的 Shift 光环 —— 每帧钉在自机（一手的 init `0x426c40`
     * 就是每帧读自机坐标）+ 自愈。`focus_age` = 上一次切换之后过了几帧（一手 `player+8` 的口径）。*/
    if (s->focus_age < 60000) ++s->focus_age;
    yk_aura_tick(s, *(const float *)(CE_PLAYER() + CE_PLAYER_X), *(const float *)(CE_PLAYER() + CE_PLAYER_Y));
    yk_ring_tick();                       /* ★ 切换环的计时（到点自己发 interrupt 3 淡出）*/
    if (s->orb_silence) --s->orb_silence;   /* ★ P-D：击破魔使后的 50 帧静默倒数（一手 Timer(0x18b89d4)）*/
    yk_popup_tick();                      /* ★ #27：刻符弹字（寿命 / 上浮 / 排版 / 距离 alpha）*/
    /* ★★★ 2026-09-22 夜（P-C ②）：**死亡时人妖槽直接置 0**（一手 `Player::begin_death th08:0x44ac01 push 0` /
     * `th08:0x44ac09 call 0x44e140` = `set_youkai_gauge(0)`）。TH18 侧读 `CE_PLAYER_STATE`：进死亡段那一帧清零。*/
    {
        int32_t pst = *(const int32_t *)(CE_PLAYER() + CE_PLAYER_STATE);
        if (pst == 2 && s->gauge != 0) {
            s->gauge = 0;
            s->gauge_ram = 0;
            s->gauge_idle = 0;
            ce_log("youkai: gauge reset to 0 on death (TH08 `Player::begin_death` th08:0x44ac09 "
                   "set_youkai_gauge(0))");
        }
    }
    /* ★★★ 2026-09-22 夜（owner 工单 **P-B 逢魔**）：**逢魔时分数自动加 100/帧**（一手一档）。
     * 一手：`Player::on_tick th08:0x44c390` 的 `th08:0x44c554 push 0x64` → `th08:0x44c55b add_to_score`
     *   （人侧）/ `th08:0x44c58c`+`:0x44c593`（妖侧）；门 = 极人 OR 极妖 **且 `Gui::IsMsgActive() == 0`**
     *   （`th08:0x44c504 call 0x4358bb`）⇒ **对话中不加分**。⚠ owner 的「≥90% 最快档」一手不存在
     *   （owner 2026-09-22 裁：按一手）。落笔 = 直接加 `CE_SCORE()`（TH18 的显示分那一格）。*/
    {
        uint8_t *pl2 = CE_PLAYER();
        if (pl2) {
            int fm = yk_fengma(s->gauge, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS);
            s->fengma = (uint8_t)fm;
            /* ★★★ 2026-09-27（owner ④）：**妖逢魔（`gauge >= +8000`）⇒ 自机弹染红**。
             * 一手判据就是「极妖」（`th08:0x406d70` 比 `+0x3dfde` = **+8000**）—— 与 `yk_fengma`
             * 那个「极人 OR 极妖」的**取或**不同 ⇒ 这里必须单独判 `>= YK_GAUGE_YOUKAI_EFFECTS`
             * （人逢魔那一路一手**不改色**，改的是「命中掉一颗刻符」，见 `youkai_core.h`）。*/
            if (s->gauge >= YK_GAUGE_YOUKAI_EFFECTS) yk_fengma_tint_bullets();
            if (fm && !CE_GUI_MSG_ACTIVE()) {
                CE_SCORE() += YK_FENGMA_SCORE_PER_FRAME;       /* 一手 100 显示分/帧、一档 */
                s->fengma_score += YK_FENGMA_SCORE_PER_FRAME;
            }
        }
    }
    /* ★ #26 风味选项：决死窗口里全屏敌弹变红（判据 = 既有原语 `player+0x476ac == 4`）。*/
    (void)yk_deathbomb_tint();
    yk_gauge_tick(s, focused);
    ce_score_family_declare(ce_card_id(c), CE_COEFF_FAMILY_08);
    /* ★★★ 2026-09-27（§BE #122，owner：「人妖槽的那个 100000 东西要**按照额外最大得点的方法 plot**，
     *   也就是说**初始得 plot 000000**」＋「我们**有一系列的额外最大得点公式**」）：
     *   **声明给计分层的必须是「额外」量** —— `sdk_core.h` 的池一节写着用户逐字
     *   「初值是 0，因为只有**加进去的**才是『额外』最大得点」⇒ 这里从「整个夜点」改成
     *   **`piv − piv_base`**（难度初值那一格是基准、不进池）⇒ 池初值 = 0 ✓
     *   （显示侧同步改成读池：见 `yk_night_text_sync()`）。*/
    ce_score_declare_delta(ce_card_id(c), s->piv - s->piv_base);
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
    /* ★★★ 2026-09-23（修法 **B-1**，依据 = owner 19:02 那局的实跑证据 `native option slots at low speed
     *   - 0/4 live`，四格全 `state 0`）：**本卡不再声明「原生子机不许出现」**。
     *   原因：本卡的 TH08 行里 `slot >= 1` 的膛口就是**引擎那 4 格原生子机**（一手 `0x45ec4c`），
     *   而我们原来既声明隐藏（隐藏断点把 `lvl` 压 0 ⇒ **记录都不建**）又每帧 sweep（`state` 写 0）
     *   ⇒ 那些行**没有活膛口** ⇒ 只有本体一列发得出来（owner：「本体只发一列、子机不发」）。
     *   ⇒ 现在：不声明隐藏（让引擎照常建记录、跟随位置），只**杀显示**（新 sweep 保留 `state`）。
     *   ⚠ 已知剩下的一处：**111（魔理沙侧）仍然声明隐藏** ⇒ 魔理沙机体下隐藏断点仍会把 `lvl` 压 0，
     *     那一路的 `slot >= 1` 行仍无膛口（登记在 `HANDOFF-108.md` §4）。*/
    ce_native_option_hide_declare((uint32_t)ce_card_id(c), 0, yk_current_character());
    /* ★ 修法第 3 步（2026-09-23）：**隐藏但请求保留记录** —— 这样即使 111（魔理沙侧）同时声明了隐藏，
     *   构建断点也不会把 `lvl` 压 0 ⇒ 那 4 格的记录/坐标照建，行循环有活膛口（我们的 `slot >= 1` 行）。
     *   显示仍由下面那支 keep_state sweep 每帧收掉 ⇒ 111 的「原生不可见」不受影响。*/
    ce_native_option_keep_records((uint32_t)ce_card_id(c), focused);
    if (focused) {
        int n = ce_native_option_sweep_keep_state();
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
        else { s->opt_age = 0; s->opt_chase = 0; s->opt_spin_vm = 0; }
    } else {
        s->chase_have = 0;
        s->chase_id = 0;
        s->opt_age = 0;
        s->opt_chase = 0;
        s->opt_spin_vm = 0;
    }
    /* ★ 2026-09-27 第二十六批：**卡侧矩形束**（每台我们自己的子机一条；一台都没有时回退本体）。
     * 每帧无条件调一次 —— 它自己判「低速 + 魔理沙 + 按住 Z」，任一条不成立就**当场收**
     *（松 Z 也走这里；松 Shift / 换机体 / 换档 另有 `yk_option_release` 的硬删兜底）。
     * ⚠ 只在这里调**一次**：`s_beam_age`（展开系数 + 每 2 帧的结算节拍）由它自己 ++。*/
    yk_beam_tick(s);
    /* ★ owner ②③ 的读数探针（每 120 帧一行）—— 排在束 tick **之后**：它读的是 `s_beam_*`，
     * 放在前面会慢一帧。*/
    if (focused) yk_probe_tick(s, yk_current_character());
    yk_option_refresh(c, s, focused);                     /* 每帧声明（低速才有货；落笔由 SDK 在帧界叫）*/
    if (!focused) {
        /* 高速 = 原机体：使魔全部撤（spec §9 实跑清单 1「松开全部还原」）。
         * ★★★ 2026-09-22（P8-2）：**无条件**走这一支（原来写成 `if (yk_map_fam_count(...))`）——
         *   `yk_map_fam_count` 只数 `fam_id != 0` 的条目，而**夹持态**的条目（`fam_seen == 1` /
         *   `broken == 1` / `rearm_pending == 1`，`fam_id == 0`）**不在计数里** ⇒ 松 Shift 清不掉它，
         *   下一次聚焦时那条夹持态还在（闸还会掐）。`yk_drop_all` 里的 `yk_withdraw_all`
         *   **整表 reset**，把夹持态一起清干净；`had == 0` 时它只是一个 32 次的循环 + 不做任何日志。*/
        /* ★★★ 2026-09-22 夜（owner 口径订正）：**松 Shift 不撤使魔** —— 使魔在人侧也活着
         *（人侧可见可打；妖侧不可见只留魔法阵）。这里只收「妖侧才有的东西」（光环 / 切换环 / 染色）。
         * ⇒ 1:1 映射与闸的夹持态**跨形态保持**（闸的基线行为回归见 §BE 详录 #68）。*/
        yk_leave_youkai(s);
        /* ★★★ 2026-09-23（owner：「**依旧子机滞留**」）—— **高速安全带**：
         *   正常路径是 SDK 在帧界叫 `on_option_pose_apply`，那里看到「这一帧我们没声明」就把手里的
         *   子机交回（`yk_option_release`）。可 owner 反复看到「子机滞留」⇒ 万一那条路没走到
         *   （退位通知没来 / 池被重建 / 我们上一帧的声明还被当成赢家），子机就会**一直挂在场上**。
         *   这里在**高速**（= 人侧）每帧做一次兜底：只要我们还攥着子机，就当场交回。
         *   ⚠ 是**幂等**的：没攥着时 `opt_script < 0 && !opt[0].ptr` ⇒ 一次函数调用 + 两个比较；
         *     真攥着才会走释放（并打一行 —— 那一行本身也是诊断：若它反复出现，说明帧界那条路有问题）。*/
        if (s->opt_script >= 0 || s->opt[0].ptr) {
            static uint32_t s_hs_release_n;
            ++s_hs_release_n;
            ce_log("youkai: HIGH-SPEED safety release #%u - we still held options while unfocused "
                   "(the frame-boundary pose hand-back did not run; this is the belt-and-braces path)",
                   (unsigned)s_hs_release_n);
            yk_option_release(c, s);
        }
        s_natopt_probe_done = 0;      /* ★ 诊断：下次再进妖侧再打一行 */
    }
    yk_familiars_tick(s);
    /* ★★ 2026-09-26（owner：那声切换音「**在有使魔的情况下才有**」）：人→妖那一下的 `se_ophide`
     *   **延到这里才播** —— 门要读**本帧**使魔表（`yk_familiars_tick()` 刚在上面建/撤完）。
     *   一手语义也是这个：那一声是**敌人的方法**（`FUN_0042c420`）在「形态变了」时发的，
     *   所以只有当场真的有那些使魔实体时才听得见（详见上面 SE 那一段的长注释）。*/
    if (s->se_fam_press) {
        const int fam_now = (int)yk_map_fam_live_count(&s_map);
        s->se_fam_press = 0;
        if (fam_now) {
            ce_play_voice(YOUKAI_FAM_SHIFT, s->se_fam_press_x);   /* TH08 id 0x28 = se_ophide */
            ce_log("youkai: shift SE 人→妖 played (se_ophide = TH08 id 0x28, ext 0x%02x); "
                   "familiar live count = %d (read AFTER this frame's familiar tick)",
                   (unsigned)CE_VOICE_YOUKAI_FAM_SHIFT, fam_now);
        } else {
            ce_log("youkai: shift SE 人→妖 NOT played - no familiar live this frame "
                   "(TH08 shape: the sound comes from the familiars themselves)");
        }
    }
    yk_fam_collide_tick(s);              /* ★ 批次 3：**人侧**撞到使魔 ⇒ MISS（妖侧可穿梭）*/
    yk_transfer_tick(s);
    /* ★★★ 2026-09-22（P1）：跟随 VM 表的**每帧不变式检查**（32 次比较，成本可忽略）。
     * 正常情况回收 0 条（判据已修，见 `yk_fam_vm_prune` 的注）；回收 >0 就是实跑判据里的
     * 「还有别的漏收路径」信号 ⇒ 打一行日志（含条数）让人一眼看见。
     * ⚠ 这一步**刻意包成一个 `noinline` 的小函数**：`on_tick_2` 已经很大，而它里面内联着
     *   `ce_damage_rect`（`sdk.h:172` 那段内联汇编）—— 往它身上再加几路活值，i686 的寄存器就不够，
     *   编译器会报「inline assembly requires more registers than available」（2026-09-22 实测）。*/
    yk_fam_vm_check();
    yk_opt_released_check();      /* ★ §BE #118：交出子机 30 帧后复查 VM 是否真的被回收 */
    /* ★★★ #25：**符卡结束的下降沿**开一轮螺旋（一手：TH08 在符卡结算时给额度、之后分批吐）。*/
    {
        int sp = ce_spell_active();
        if (s->spell_was && !sp) {
            s->spiral_quota = YK_SPIRAL_QUOTA_FULL;
            s->spiral_t = 0;
        }
        if (s->spell_was != (uint8_t)(sp ? 1 : 0))      /* ★ §BE #113：两个边沿各打一行探针 */
            yk_spell_probe(sp ? "start" : "end");
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
/* ★ 2026-09-22 夜（owner 二次订正）后记录 = 12 B（`fam_id`/`anm_id`/`variant`）：常驻环的两个字段
 * 随「环改成切换那一下的短命特效」一起去掉了 ⇒ 表从 32×20 B 降到 32×12 B。*/
/* ★ 2026-09-22 夜（owner：妖侧只剩魔法阵）后记录 = 16 B：加回一个**常驻魔法阵**句柄
 * （`circle_id`）；表 = 32×16 B = 512 B，仍是文件级静态、不进卡的私有状态预算。*/
_Static_assert(sizeof(yk_fam_vm_t) == 16, "familiar follow-VM record changed size");

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
 *       · ~~擦弹 ⇒ 每点擦弹 1 颗（落在自机位置）、单帧上限 8 颗~~ —— **2026-09-22 晚已作废**：
 *         一手是**每擦到一颗弹 1 / 2 / 3 颗**（`th08:0x44aac9` 必给 + `:0x44aaef` 的符卡门 +
 *         `:0x44ab0f` 的机体门；逐指令全文在 `youkai_core.h` §3.11c），落点仍 = 自机
 *         （拿不到被擦的那颗弹）；同时那个「单帧上限 8」也撤了，换成 `YK_GRAZE_RESYNC_MAX`
 *         （❓ 对账护栏，**不是**一手规则 —— 一手没有单帧上限）；
 *       · 漂移算式末尾的 `g_ShotSpeed` 取 1，两个计时器的重置时机做了简化；
 *       · HUD 块 256×78 与「每帧覆盖位置 ⇒ TH08 的 `posTime` 滑入被覆盖」；
 *       · 人妖槽**换关会归零**（一手是**不**归零，只有新局清）—— 因为本卡的 `on_load` 复用 `on_ctor`。
 *     ⏳ 本轮**不接**（都在报告「未决」里）：
 *       · 刻符的**分数**那一路（`yk_orb_score` 需要「本关/本局蓝点数」，TH18 只有 `CE_ADDR_MONEY_ITEMS` 这个近似物）
 *         与**符卡 +8000**（TH18 的符卡分不是 TH08 那套）；
 *       · 「收刻符动人妖槽」那道 `Timer::current_is_eq_(&0x18b89d4, 0)` 门 ❓ 语义未钉 ⇒ 本卡无条件动；
 *       · 人妖槽四档判定在 TH18 侧的**后果**（TH08 的蓝点 ×2 / 掉落分档 / 使魔弹染色 / 射击间隔）
 *         —— 判定与 HUD 染色已接，四档的玩法后果待 owner 拍（见 `AUDIT.md` §BE-FG §4）。*/
