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
#define YK_EMIT_OFFSETS     (YK_DATA_OFF + 0x4e58)
#define YK_EMIT_OFFSETS_ABS (0x122c + 0x4e58)
#define YK_EMIT_ORIGINS_ABS (0x122c + 0x4f18)  /* ★ = enemy+0x6144：绝对 origins（stride 0xc）*/  /* ★ = enemy+0x6084：**绝对**活相对偏移地址（旧版 good 公式的量）*/ /* `bullet_mgr_offsets[i]`：相对偏移（float2 xy + float）*/
#define YK_EMIT_ORIGINS     (YK_DATA_OFF + 0x4f18) /* `bullet_mgr_origins[i]`：绝对原点；`.z > 0.9` = 绝对模式 */
#define YK_EMIT_VEC_STRIDE  0x0c
/* 敌人字段（engine.h 没有的几条）*/
#define YK_ENEMY_ABS_POS    0x12b4 /* `zEnemyData.abs_pos` = `enemy+0x122c+0x88`（= spawn `+0x00` 的去处，
                                    * engine.h `CE_SPAWN_POS` 的注释）*/
#define YK_ED_LIFE_CUR      0x6220 /* `zEnemyData.life.current`（= `CE_ENEMY_DATA + 0x4ff4`；扣血点 `0x42f314` 一手）*/
#define YK_FLAGS_DELETE_NOW 0x2000000u /* `flags_lo` 位 25 = 「本帧删」：`0x42d200` 只置它，
                                        * sweep `0x42dfc5` `test [enemy+0x635c],0x2000000` → 直接析构
                                        *（不跑死亡处理、不掉落、不跑死亡 ECL）⇒ 使魔的**静默撤退**。*/
/* ★★★ 2026-10-01（**P0 根因 · 修「死亡 +N / 消弹在两侧都不出现」**）：**zEnemy 上的父字段**。
 *
 * ⚠⚠ 这一格与 `engine.h` 的 `CE_SPAWN_PARENT`（= **0x50**）**不是同一个偏移**，二者只共享语义：
 *   · `CE_SPAWN_PARENT` = **spawn 参数结构**（`CE_SPAWN_SIZE` = 0x54 那一张表）里的最后一格；
 *     引擎 `EnemyManager::allocate_new_enemy` 把它**拷到敌人对象**上：
 *     一手 `th18:0x42d8e4 mov eax,[edx+0x50]` / `0x42d8e7 mov [esi+0x6834],eax`（`edx` = spawn 参数、
 *     `esi` = 新 zEnemy）—— 写点与字段图见 `engine/enemy/th18/08-parent-enemy-id-and-sub-enemies.md` §1，
 *     ExpHP（`local/vendor/th-re-data/data/th18.v1.00a/type-structs-own.json`）：
 *     `zEnemy[+0x6830]=enemy_id` / `zEnemy[+0x6834]=parent_enemy_id`。
 *   · 本卡另外三张卡（`lolk.c` / `ufo.c` / `faith.c` / `animal.c`）读的都是 **0x6834**，可交叉对账。
 *
 * ⇒ 在**敌人对象**上问「这是谁的孩子」必须读 **0x6834**。旧代码在敌人对象上写 `e + CE_SPAWN_PARENT`
 *   （= `e+0x50`）—— 那一格是 **`zEclVm.context`（+0xc）内 `zEclStack.data[]` 的一个 int32**，
 *   也就是**敌人自己 ECL 脚本的一个局部变量**（`zEclVm` / `zEclRunContextHolder` / `zEclStack` 见
 *   同一份 ExpHP 表）。后果（实跑 2026-10-01 已证）：
 *     · 「数这只母机的活子敌人」永远数不到（随机撞上某个 ECL 变量才可能相等）⇒ `n == 0`
 *       ⇒ 击破批整块被 `if (!n2)` 跳过 ⇒ **两侧都没有 +N 浮字、也没有死亡点吸收区**；
 *     · 「谁的子敌人 / 谁的链根」那几处判据全部退化成读 ECL 变量（见各自注里的 ⚠）。
 *   ⚠ **只改「敌人对象上问父字段」的读点**；写 spawn 参数那一处（`yk_spawn_familiar_parent`）
 *     照旧用 `CE_SPAWN_PARENT` —— 那是 0x50，正确。*/
#define YK_ENEMY_PARENT_ID 0x6834   /* zEnemy.parent_enemy_id（= spawn params +0x50 的去处）*/
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
    int32_t       score_seen;          /* ★ 2026-09-27（第四十一批）：上一行台账时的引擎分数（算 Δ 用）——
                                        *   owner 报「什么都没干，得分一直在涨」⇒ 台账给出**实测涨速**。*/
    uint16_t      orb_peak;            /* ★ 2026-09-27（第三十八批）：本局**同时在飞的刻符峰值**（诊断）——
                                        *   符卡螺旋那一段应当打到 ~100（`YK_ORB_MAX` 就是按它定的）。*/
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
    uint8_t       laser_probe;        /* ★ 2026-10-01（工单②：「我们到底有没有在人侧杀自机激光」）：
                                       *   `yk_kill_retail` 上下文的诊断行计数（只打前几次）*/
    uint8_t       laser_probe_human;  /* ★ 同上：**人侧**那行「NOT called」的计数（证明没碰）*/
    uint8_t       pose_probe;         /* ★ 2026-10-01（工单③）：人侧「我们到底写了什么」探针计数 */
    /* ★★★ 2026-09-22（#25 击破符卡的螺旋，一手参数见 `youkai_core.h` §3.11b）：
     * `spiral_quota` = 还剩几颗没吐（TH08 的 `spellcard[+0x10]`）；`spiral_t` = 计时器帧数（从 10 起算）；
     * `spell_was` = 上一帧在不在符卡里（**符卡结束的下降沿**是触发器）；`spiral_logged` = 只记一次日志。*/
    uint8_t       flash_pending;      /* ★★★ 2026-09-22（#22）：本帧是「人→妖」那一帧 ⇒ 第一个建出来的
                                       *   使魔身上补一支爆闪（一手：爆闪起在使魔位置上）*/
    uint32_t      spiral_quota;
    uint16_t      spiral_t;
    uint8_t       spell_was;
    uint8_t       spiral_logged;
    uint32_t      gap_releases_seen;   /* ★★★ 2026-10-01（owner 实跑 ②）：上一帧读到的
                                        *   `yk_map_gap_releases`（`etOn` 间隔判出的波次边沿次数）*/
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

/* ================== ★★★ 2026-10-01（**owner 口径 ④：锁形态**） ==================
 * owner 逐字：「当以**人形态放 B** 的时候，**B 的持续时间内应当固定为人形态**；**妖形态同理**」
 *          ＋「**灵击不算 B**」
 *          ＋「当处于**灵界状态** or **暴走咆哮状态**的时候，**可以改主机贴图，但是 shift 别把
 *             妖子机放出来**」。
 * ⇒ 本卡只做一件事：**这三个条件任一成立时，把「本卡眼中的侧」钉在锁定那一刻的值**
 *   （`yk_focused()` 代替所有 `CE_PLAYER_FOCUSED()` 的**行为判据**）——按 Shift 不再放/收妖子机、
 *   不再换妖侧出弹组、不再认领妖侧 shooterset。
 * ⚠ **主机贴图那条一律不动**（那是引擎 / 113 / 117 自己的事；owner 明确说「可以改主机贴图」）。
 * ⚠ 灵界（113）与暴走咆哮（117）是**别的卡的私有状态**，本卡读不到 ⇒ 见下面两个 TODO 桩：
 *   在跨卡只读查询落地前它们恒 0 ⇒ 总门现在只对「真 B」生效（登记 `AUDIT.md` §BE #201）。*/
static int8_t   s_side_lock = -1;        /* -1 = 不锁（跟真实 Shift）；0 = 锁人侧；1 = 锁妖侧 */
static int8_t   s_bomb_prev;             /* 上一帧 `bomb_mgr+0x30`（用来抓「B 起手那一帧」）*/
static int8_t   s_bomb_is_deathbomb;     /* 当前这次 B 是**灵击**（起手帧落在决死窗口里）⇒ 不锁 */
static uint32_t s_deathbomb_frame;       /* 最近一次见到决死窗口（自机状态 4）的帧号 */
static int8_t   s_deathbomb_seen;

/* 真实聚焦（`player+0x476cc`，引擎每帧从输入位 3 写）。
 * ★★★ P0 事故（2026-10-01 18:20 dump，`code=0xc00000fd` STACK_OVERFLOW，`dll+0x6555c`）：
 *   这一支原来是**唯一**读真实聚焦的地方，而我在批量把行为判据换成 `yk_focused()` 时
 *   **连带把本函数自己的 `CE_PLAYER_FOCUSED()` 也换掉了** ⇒ 变成 `return yk_focused() ? 1 : 0;`
 *   = **自递归**（`s_side_lock < 0` 的**正常游玩**态每帧都进）⇒ 装 108 一进关卡就爆栈。
 *   ⇒ 教训写在这里：**读引擎原值的那一支必须是独立函数、且只此一处**（下面 `yk_real_focused`），
 *     `yk_focused()` 只允许调它，**永远不许调自己**。*/
static int yk_real_focused(void)
{
    return CE_PLAYER_FOCUSED() ? 1 : 0;
}

/* 「本卡眼中的侧」：锁定期间恒为锁定值，否则 = 真实聚焦。
 * ★ P0 防线（owner 要求「结构上仍可能深层递归就加重入守卫」）：静态重入标志 —— 一旦重入
 *   （今后有人再往这里塞一层间接调用）立刻**返回真实聚焦**并打一行（只打一次），绝不递归下去。*/
static int8_t s_focus_reentry_logged;
static int yk_focused(void)
{
    static int8_t in_focus;
    int v;
    if (s_side_lock >= 0) return (int)s_side_lock;
    if (in_focus) {
        if (!s_focus_reentry_logged) {
            s_focus_reentry_logged = 1;
            ce_log("youkai: SIDE-FOCUS REENTRY guard tripped in yk_focused() - returning the real "
                   "focus (this is a BUG: yk_focused must never call itself; owner 108 P0)");
        }
        return yk_real_focused();
    }
    in_focus = 1;
    v = yk_real_focused();
    in_focus = 0;
    return v;
}

/* TODO(owner 口径 ④)：**灵界（113 Trance）/ 暴走咆哮（117 Animal）**的状态查询。
 * 这两张卡的「在进行中」是**卡内私有状态**（113：`trance.c` 的 `p->state`；117：`animal.c` 的
 * `s->core.ready || s->core.hypering`）⇒ 本卡**没有**合法的只读路径。
 * 方案（二选一，需 owner/作者拍；**本轮没动别人的卡、也没动共享件**）：
 *   ① 共享件加一个**跨卡只读标志位**（`sdk.h`：`ce_card_flag_set(card_id,bit)` / `ce_card_flag_get(card_id)`），
 *      113/117 在进入/退出状态时各置/清一位；
 *   ② 由那两张卡各导出一个 `int <card>_state_query(void)`（同一层的 `cards_def` 弱符号导出面）。
 * ⇒ 任一路落地后，把下面两支改成读那一位即可（门本身已经是纯函数 + 单测覆盖）。*/
static int yk_trance_active(void) { return ce_card_flag_any(CE_CARD_FLAG_TRANCE); }
static int yk_roar_active(void)  { return ce_card_flag_any(CE_CARD_FLAG_ROAR); }

/* ★★ 2026-10-15 **只读诊断探针**（为 owner 的「同开」需求：leveledit 米斯蒂亚那场要接管他们的使魔 ✓）。
 *
 * 目的 = **把"那场"的判别信号实测出来** ✓，而不是照 README 猜 ✗ —— 已实测证伪一条：
 * 他们改的 `st02enm.anm` 脚本号**仍是 0–18** ✓（`thanm -l 18` ✓ ⇒ README 说的「追加 scripts 28/29」
 * 指的是 **TH08 源侧 `anm8/enemy.anm`** ✗，不是 TH18 运行时的号 ✓）。
 * 另一条（ECL sub 索引）本机拿不到：他们编译要的 `th08.dat` 修订（sha256 `9d7edf43…`）不在这台机器上 ✗
 * ⇒ 所以**改成实测**：拿零售局与 leveledit 局的日志一比，哪个字段能区分一目了然 ✓。
 *
 * 做法：**只在有 boss 在场时**、每 30 帧一行 ✓；字段**全部只读** ✓：
 *   boss：id / flags / `anm_main` / `anm_slot_0_script` / `+0x274` / `own_boss_id` / `time_in_ecl.cur`
 *         / **`ecl_int_vars[4]`** / **`ecl_float_vars[]`** ✓
 *   其它敌机（最多 8 只 ✓）：id / `anm_main` / `slot0` / `+0x274` / `time_in_ecl.cur` ✓
 *   —— 他们的使魔是「移植为 TH18 **普通敌人**」✓ ⇒ 会出现在这一列 ✓。
 * 之后把判别常量钉进 `youkai_core` ✓，**再回来把本探针关掉** ✓（`YK_LE_PROBE` 改 0 ✓）。
 *
 * 偏移出处 = `local/vendor/th-re-data/data/th18.v1.00a/type-structs-own.json` 的 `zEnemyData` ✓
 * （`anm_slot_0_script` +0x26c / `anm_main` +0x270 / `__probably_cur_subscript_for_anm_main` +0x274 /
 *  `ecl_int_vars` +0x28c / `ecl_float_vars` +0x29c / `time_in_ecl` +0x2bc（`zTimer`：cur = +4）/ `own_boss_id` +0x5140 ✓）。
 *
 * ★★★ 2026-10-18（**基址订正** ✗→✓，一手）：上面这些偏移都是 **`zEnemyData` 相对**的，而本探针与
 *   下面 `yk_le_boss_sub_index()` 手里的 `e` **是 `zEnemy*`** —— 证据（三条互相独立、都一手）：
 *     ① `ce_enemy_by_id()` / `CE_ENEMY_FOREACH` 返回的是链表结点里的 `entry`（`sdk.h:667`），
 *        而它们用 `*(int32_t *)(e + CE_ENEMY_ID 0x6830)` 认人，`CE_ENEMY_ID` = **`zEnemy.enemy_id`**
 *        （`engine.h:195` / `engine.h:687`，一手 `Enemy__constructor` `0x42e3f0`）；
 *     ② `CE_ENEMY_DATA 0x122c` = **`zEnemyData` 在 `zEnemy` 里的偏移**（`engine.h:200`，步进
 *        `0x42ed40` 三处交叉核对）⇒ `zEnemyData = zEnemy + 0x122c`（vendor `zEnemy` 表：
 *        `+0x0 = struct zEclVm`、`+0x122c = struct zEnemyData` ✓）；
 *     ③ 同一族偏移在 `engine.h` 里早就有 zEnemy 绝对版：`CE_ENEMY_ECL_I0 = 0x14b8`
 *        （= `0x122c + 0x28c`）、`CE_ENEMY_ANM_SCRIPT = 0x1498`（= `0x122c + 0x26c`）✓。
 *   ⇒ 原先这些字段**少加了 0x122c** ⇒ 读到的是 `zEnemy` 头部（ECL VM 区）而不是 `zEnemyData`；
 *     这既解释了旧日志里「anm 全是 0」（那两格在 VM 的栈区），也是下面 `+0x270` / `+0x28c`
 *     「实测没有区分度」的**真实原因**（读错格了 ✗）。下面把偏移一律写成 `CE_ENEMY_DATA + …`。*/
/* ★★ B1/B2（2026-10-18）：**leveledit 使魔标记** `$I3` = `ecl_int_vars[3]` ⇒ `zEnemyData + 0x298`
 *   （= `zEnemy + 0x14c4` = `CE_ENEMY_ECL_I0 + 12`）。一手（对方仓库，这机器上就有）：
 *   `/home/nipporita/th18-leveledit/mods/stage02-mystia/translate.py:207-217` —— 每个
 *   `MystiaChild<n>()` 的第一行就是 `$I3 = 997;`，注释逐字写着
 *   「marks every generated familiar so the 108 card can recognise it from
 *    zEnemyData+ecl_int_vars[3] (+0x14c4, i.e. EnemyData+0x298)」✓；
 *   同一段还说明 TH18 的敌人只有 `$I0–$I3`、且没有任何可达的 sub 读写 `$I3` ⇒ 标记活到敌人死亡 ✓。
 *   ⇒ 探针打 `iv3=`（**正确基址**）与 `iv3z=`（老口径：直接把 0x298 加在 `zEnemy*` 上）两格，
 *     实跑一看两个数就能证明基址（997 只会出现在 `iv3` 上 ✓）。
 *   ⚠ 这两条宏在 `#if YK_LE_PROBE` **外面**：`yk_le_iv3()` 在 B2 的接管线里也要用（探针关掉后照样要判 ✓）。*/
#define YK_E_ECL_IVARS3       (CE_ENEMY_DATA + 0x298)  /* = ecl_int_vars[3] = `$I3` ✓ */
#define YK_E_ECL_IVARS3_RAW   0x298                    /* 对照：直接加在 zEnemy* 上的老口径 ✗ */
#define YK_LE_PROBE 1
#if YK_LE_PROBE
#define YK_E_ANM_SLOT0_SCRIPT (CE_ENEMY_DATA + 0x26c)
#define YK_E_ANM_MAIN         (CE_ENEMY_DATA + 0x270)
#define YK_E_SUB274           (CE_ENEMY_DATA + 0x274)
#define YK_E_TIN_ECL_CUR      (CE_ENEMY_DATA + 0x2c0)  /* zTimer @+0x2bc：prev(+0)/current(+4)/current_f(+8) ✓ */
#define YK_E_ECL_IVARS        (CE_ENEMY_DATA + 0x28c)
#define YK_E_ECL_FVARS        (CE_ENEMY_DATA + 0x29c)
#define YK_E_OWN_BOSS         (CE_ENEMY_DATA + 0x5140)
static void __attribute__((noinline)) yk_le_probe(void)
{
    static uint32_t s_probe_n;
    uint8_t *em, *e;
    int32_t i, shown = 0;
    if (s_probe_n++ % 30u) return;                        /* 30 帧一行 ✓ */
    em = CE_ENEMY_MGR();
    if (!em) return;
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        int32_t bid = *(int32_t *)(em + CE_EM_BOSS_IDS + (size_t)i * 4);
        if (!bid) continue;
        e = ce_enemy_by_id(em, bid);
        if (!e) continue;
        ce_log("le-probe(boss%d): id=%d fl=%08x anmMain=%d slot0=%d sub274=%d own=%d tin=%d iv=%d,%d,%d,%d fv=%.2f,%.2f,%.2f",
               (int)i, (int)bid, (unsigned)*(const uint32_t *)(e + CE_ENEMY_FLAGS),
               (int)*(const int32_t *)(e + YK_E_ANM_MAIN),
               (int)*(const int32_t *)(e + YK_E_ANM_SLOT0_SCRIPT),
               (int)*(const int32_t *)(e + YK_E_SUB274),
               (int)*(const int32_t *)(e + YK_E_OWN_BOSS), (int)*(const int32_t *)(e + YK_E_TIN_ECL_CUR),
               (int)*(const int32_t *)(e + YK_E_ECL_IVARS),
               (int)*(const int32_t *)(e + YK_E_ECL_IVARS + 4),
               (int)*(const int32_t *)(e + YK_E_ECL_IVARS + 8),
               (int)*(const int32_t *)(e + YK_E_ECL_IVARS + 12),
               (double)*(const float *)(e + YK_E_ECL_FVARS),
               (double)*(const float *)(e + YK_E_ECL_FVARS + 4),
               (double)*(const float *)(e + YK_E_ECL_FVARS + 8));
    }
    CE_ENEMY_FOREACH(em, e) {
        if (shown >= 8) break;
        if (!ce_enemy_lockable(e)) continue;
        ce_log("le-probe(enemy): id=%d anmMain=%d slot0=%d sub274=%d tin=%d iv3=%d iv3z=%d",
               (int)*(const int32_t *)(e + CE_ENEMY_ID),
               (int)*(const int32_t *)(e + YK_E_ANM_MAIN),
               (int)*(const int32_t *)(e + YK_E_ANM_SLOT0_SCRIPT),
               (int)*(const int32_t *)(e + YK_E_SUB274),
               (int)*(const int32_t *)(e + YK_E_TIN_ECL_CUR),
               (int)*(const int32_t *)(e + YK_E_ECL_IVARS3),
               (int)*(const int32_t *)(e + YK_E_ECL_IVARS3_RAW));
        ++shown;
    }
    ce_log("le-probe: live=%d", (int)*(const int32_t *)(em + CE_EM_ENEMY_COUNT));
}
#endif /* YK_LE_PROBE */

/* 每帧一次（在 `on_tick_2` 最前面调用）：更新「锁形态」状态机。
 * 「真 B」判据 = `bomb_mgr+0x30`（`do_bomb`，`engine.h`）**且起手那一帧不在决死窗口里**：
 *   灵击 = 决死窗口（自机状态 4，`player+0x476ac`，`Player__die` 写、持续 8 帧
 *   `player+0x47908`，见 `engine/player/th18/03-player-hit-gate.md`）里按的 B
 *   ⇒ 起手帧见到状态 4（或刚刚见过）⇒ `yk_is_real_bomb()` 判 0 ⇒ **不锁**（owner：「灵击不算 B」）。
 *   ⚠ 判据只在**起手那一帧**取一次并记住整个持续期：灵击的动画比决死窗口长得多，
 *     只看瞬时状态会把它的后半段误判成真 B。*/
static void yk_side_lock_tick(void)
{
#if YK_LE_PROBE
    yk_le_probe();          /* ★ 只读诊断：把"那场"的判别信号实测出来（常量钉死后关掉 ✓）*/
#endif
    uint8_t *m = CE_BOMB_MGR();
    int act = (m && *(const int32_t *)(m + CE_BOMB_MGR_ACTIVE) != 0) ? 1 : 0;
    int state4 = yk_deathbomb_window(*(const int32_t *)(CE_PLAYER() + CE_PLAYER_STATE));
    unsigned now = (unsigned)ce_sdk_frame();
    int real_bomb;
    if (state4) { s_deathbomb_seen = 1; s_deathbomb_frame = now; }
    if (act && !s_bomb_prev) {
        int recent = s_deathbomb_seen && (now - s_deathbomb_frame) <= 8u;   /* 决死窗口 8 帧 */
        s_bomb_is_deathbomb = (state4 || recent) ? 1 : 0;
        if (s_bomb_is_deathbomb)
            ce_log("youkai: side lock - this B is a DEATHBOMB (player state 4) => do NOT lock the form "
                   "(owner 2026-10-01: 'a deathbomb is not a B')");
    }
    if (!act) s_bomb_is_deathbomb = 0;
    s_bomb_prev = (int8_t)act;
    real_bomb = yk_is_real_bomb(act, s_bomb_is_deathbomb);
    if (yk_side_switch_allowed(yk_trance_active(), yk_roar_active(), real_bomb)) {
        if (s_side_lock >= 0) {
            ce_log("youkai: side lock OFF - Shift switches again (was locked to the %s side)",
                   s_side_lock ? "youkai/focused" : "human/unfocused");
            s_side_lock = -1;
        }
    } else if (s_side_lock < 0) {
        /* ⚠ **必须**读真实聚焦（不是 `yk_focused()`）：锁的那一刻 `s_side_lock` 还是 -1，
         *   若走 `yk_focused()` 就会在「决定锁谁」这一步先递归（P0 的第二条自递归链）。*/
        s_side_lock = (int8_t)yk_real_focused();
        ce_log("youkai: side lock ON for this B - the form stays %s until the B ends "
               "(owner 2026-10-01; deathbomb excluded)",
               s_side_lock ? "youkai/focused" : "human/unfocused");
    }
}

/* ================== ★★★ 2026-10-01（owner 工单④：**ANM VM 池压力**） ==================
 * owner 的新头号嫌疑：**人侧激光的分段是 anm VM**，而 anm VM 是全局的；本卡每帧起一大堆
 * （拖尾/浮字/环/束条/光环）⇒ 若把池占满或踢掉别人的 VM，激光就会「一节一节长出 / 命中处断一节」。
 *
 * **一手事实（先摆出来，免得修错方向）**：
 *   · 分配器 = `AnmManager__allocate_new_vm` **`0x489320`**，而 `engine.h:738` 记着它的行为：
 *     **freelist 空则 `malloc(0x60c)`** ⇒ 它是**按需增长**的，不是固定小池 ⇒ 「池满 ⇒ 起不出 VM」
 *     只在 `malloc` 失败时发生（那时连引擎自己都完了）。
 *   · id 空间 = **`id & 0x7fff`**（`AnmManager__get_vm_with_id` `0x488b40`，engine/anm/th18/01 §0）
 *     ⇒ 理论上存在**32768 个 id 回绕**后别名到别人的 VM 的风险（实跑里我们远到不了）。
 *   ⇒ 所以「池压力」这条要用**证据**判：下面这组计数 + 探针行（try/fail/skip）就是证据。
 *     若 owner 的 `YK_FX_VMS=0` 版仍然异常、而 fail 计数一直是 0 ⇒ 这条线也可以干净排除。
 *
 * **二分开关**（第二把，随下一版一起发）：
 *   `1` = 现状（照常起我们的特效 VM）；
 *   `0` = **完全不生成**我们的拖尾/浮字/环/束条/光环 VM（`yk_vm_spawn()` 直接返回 0，
 *         各调用点的既有失败分支照常跑）—— 其它逻辑（伤害/消弹/转嫁/子机/使魔实体）一行不变。*/
/* ★★★★ 2026-10-01（owner 工单⑤ · **按卡入口逐项二分**，一把开关一项、默认 1 = 现状）：
 *   owner 的新证据推翻了「VM 池压力」（触发时 4 面开头、**完全没有**拖尾/浮字/加法环）
 *   ⇒ 改成逐入口二分：**每次只关一项**，人形态打 4 面开头，看哪一项让激光恢复。
 *   ⚠ **绝不耦合**：每个开关只管自己那一条路径；关掉后其它逻辑一行不变。
 *   日志里有一行汇总（`youkai: bisect switches …`，`on_tick_2` 首次运行时打）。*/
#define YK_ENABLE_SET_INDEX       1  /* ① `on_shot_set_index`：认领妖侧 shooterset / 退选举 */
#define YK_ENABLE_NATIVE_SUPPRESS 1  /* ② `on_native_shot_suppress`：压不压原生行循环 */
#define YK_ENABLE_OPTION_POSE     1  /* ③ `on_option_pose_apply` + declare + hide/keep_records */
#define YK_ENABLE_HUD_VM          1  /* ④ HUD 数字/槽 VM（`ce_anm_spawn_ui` 那一族）*/
#define YK_ENABLE_FX_VMS          1  /* ⑤ 拖尾/浮字/环/束条/光环 VM（`yk_vm_spawn` 唯一出口）*/
#define YK_ENABLE_TRANSFER        1  /* ⑥ `ce_damage_rect` + `ce_raise_damage_cap`（伤害转嫁）*/
#define YK_ENABLE_ZONE            1  /* ⑦ `ce_cancel_radius`（死亡点消弹）*/
/* ★★★★ 2026-10-01（owner 工单⑥ · 第 102 版「7 项全关仍异常」⇒ 转「动敌人链表」这条线）：
 *   两条都是**会往敌人链表插/删对象**（或每帧重写发射口表）的路径。⚠ 默认 1 = 现状。*/
/* ★★★★ 2026-10-01（owner 工单⑧ · **整卡 no-op 总开关**，最后一道最干净的判据）：
 *   `YK_ENABLE_CARD = 0` ⇒ **本卡每一个回调入口就 return**（连声明/注册/VM 都不做；卡对象与它在卡组里的
 *   位置照旧存在）。用途：把「凶手在**我们的回调代码**里」与「凶手是**卡对象存在本身**
 *   （共享表登记 / deck 组合 / 计分层 / 加载期副作用）」这两类一次分开。
 *   ⚠ 只给二分用；=1（默认）时连编译产物都应当与现状等价。*/
#define YK_ENABLE_CARD            1
/* ⑩ 计分层声明（`ce_score_family_declare` + `ce_score_declare_delta`）*/
#define YK_ENABLE_SCORE_DECL      1
/* ⑪ 逢魔的 `CE_SCORE() += 100/帧` */
#define YK_ENABLE_FENGMA_SCORE    1
/* ⑫ 每帧那条独立的 `ce_option_pose_declare`（只关声明，不动 `on_option_pose_apply`）*/
#define YK_ENABLE_POSE_DECLARE    1
/* ⑬ HUD 的 **初始**（非每帧）`ce_anm_spawn_ui` */
#define YK_ENABLE_HUD_INIT        1
/* ★★★★ 2026-10-01（owner 工单⑨ · **按回调族排除法**；`YK_ENABLE_CARD=0` 时下面全部照旧 return）：
 *   每族一行；默认 1 = 现状。owner 按「只关一族」出包二分。*/
#define YK_ENABLE_CB_LIFECYCLE     1  /* ⑭ on_ctor / on_load / on_power_level_change / on_run_reset / on_delete */
#define YK_ENABLE_CB_STAGE         1  /* ⑮ on_stage_start（本卡当前**没有**这个回调，占位备用）*/
#define YK_ENABLE_CB_TICK_SHOOTERS 1  /* ⑯ on_tick_shooters（★ 头号嫌疑：每帧跑在 shooter 行上）*/
#define YK_ENABLE_CB_TICK2         1  /* ⑰ on_tick_2 */
#define YK_ENABLE_CB_BULLET 1  /* ⑱ on_bullet_created / on_bullet_hit */
/* ★★★★ 2026-10-01（owner 工单⑩：#110 二分把凶手锁在 bullet 回调 ⇒ 再拆两个子项做最后确认）*/
#define YK_ENABLE_CB_BULLET_CREATED 1  /* ⑱a `on_bullet_created`（只对我们自己的妖侧 set 改伤害）*/
#define YK_ENABLE_CB_BULLET_HIT     1  /* ⑱b `on_bullet_hit`（人逢魔：命中 ⇒ 掉刻符）*/
#define YK_BULLET_HIT_QUEUE         8  /* 命中回调里**只排队**；真正撒符挪到 `on_tick_2`（见修法 B）*/
/* ★★★★ 2026-10-01（owner 工单⑪：**把 `yk_orb_spawn()` 这条路径逐段开关**）：
 *   #111（只把撒符推迟 1 帧、照样撒）仍异常 ⇒ 凶手是 **`yk_orb_spawn()` 这个动作本身**。
 *   ① `ORB_VM`      = 0：不建刻符 VM（`ce_anm_spawn` 那支跳过 ⇒ 这颗直接放弃）；
 *   ② `ORB_LIST`    = 0：**不占我们自己的 `s_orb` 表槽**（只留「建 VM」那一半 ⇒ 与本卡逻辑脱钩）；
 *   ③ `ORB_ZONE`    = 0：占位（一手核对：撒符**不开区、不调 `ce_cancel_radius`** —— 消弹在 `yk_zone_*`）；
 *   ④ `ORB_DRECT`   = 0：占位（一手核对：撒符**不建 `ce_damage_rect`**；本卡只有转嫁 `:2675`、卡侧束 `:6438`）；
 *   ⑤ `ORB_PLAYERW` = 0：**不消耗引擎 RNG**（`ce_rand()` —— 本路径唯一的全局态交互）⇒ 用固定偏移。*/
#define YK_ENABLE_ORB_VM           1
#define YK_ENABLE_ORB_LIST         1
#define YK_ENABLE_ORB_ZONE         1
#define YK_ENABLE_ORB_DRECT        1
#define YK_ENABLE_ORB_PLAYERW      1
#define YK_ENABLE_CB_ITEM          1  /* ⑲ on_item_frame */
#define YK_ENABLE_CB_DRAW          1  /* ⑳ on_draw */
#define YK_ENABLE_CB_ENEMY_DEATH   1  /* ㉑ on_enemy_death */
#define YK_ENABLE_CB_DEATHBOMB     1  /* ㉒ on_death_before_deathbomb */
#define YK_ENABLE_ORBIT           1  /* ⑧ 绕转使魔整体：扫描 / `ce_spawn_enemy` 建壳 / 撤退 / VM / 喂转嫁 */
#define YK_ENABLE_MAP_LAYER       1  /* ⑨ 映射（发射口）层：note_slot / 建壳 / 撤壳 / 闸 */
#define YK_FX_VMS YK_ENABLE_FX_VMS   /* 旧名保留（同一格；`yk_vm_spawn` 里用）*/

static unsigned s_fx_try, s_fx_fail, s_fx_skip;        /* 累计（只增）*/
static unsigned s_fx_try_win, s_fx_fail_win;           /* 本「观察窗」内（每 120 帧清）*/
static uint32_t s_fx_win_start;

/* 本卡所有**特效类** VM 的唯一出口（`YK_FX_VMS=0` 时一律返回 0 = 「起不出来」）。
 * ⚠ 只包特效：**实体**（使魔跟随 VM / 刻符 / HUD 数字）仍走 `ce_anm_spawn` 本体，
 *   否则二分版会让 owner 连使魔/刻符都看不见，判据就不干净了。*/
static uint32_t yk_vm_spawn(void *anm, int script, int layer)
{
    uint32_t id;
    if (!YK_FX_VMS) { ++s_fx_skip; return 0; }
    ++s_fx_try; ++s_fx_try_win;
    id = ce_anm_spawn(anm, script, layer);
    if (!id) { ++s_fx_fail; ++s_fx_fail_win; }
    return id;
}

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

/* ★★★★ 2026-10-01（owner 工单 · 普查 ①：**壳要读「发弹时定格的 offset_final」，不是活偏移**）：
 *   现象：TH08 常见写法 `etOffset(非零) → et* → etOffset(0,0)`（138 组、`mathCirclePos` 110 处）
 *   ⇒ 活偏移字段在**同一帧内**就被清回 (0,0)，而我们是**每帧采样** ⇒ 壳只在那一帧闪一下（一闪一缩）。
 *   修法（**0 新站点**）：在**已在用**的站点 `th18:0x433ffa`（`yk_on_shot_slot`）里，把 `etOn` 刚写好的
 *   `props[i]+0x08/+0x0c/+0x10`（**offset_final**）抄一份到卡侧**每槽小缓存**；壳的目标位置优先用它。
 *   缓存按 `(enemy_id, slot)` 存（`s_emit_latch[]`，8 组环形；同一个 `(enemy,槽)` 只保留最新一份）。
 * ★★★★ 2026-10-11（**纯静态核实**，全文 `HANDOFF-108-EMITTER-STATIC-VERIFY.md` §1.2/§2）：
 *   缓存里**必须同时记模式**。一手 `th18:0x433f34` 的 `etOn` 有两条写路径：
 *     · 相对路（`origins[i].z <= 0.9f`，判据 `th18:0x433f52`）：`props = 敌人位置(此刻) + offsets[i]`；
 *     · 绝对路（`origins[i].z > 0.9f`）：`props.xy = origins[i].xy + offsets[i].xy`、`z = 0`（**不加敌人位置**）。
 *   ⇒ 绝对模式**不能**存成「props − 敌人位置」（那样消费端 `敌人位置 + 它` 会把壳拖着走、离锚点越来越远）。*/
typedef struct { uint32_t enemy_id; int16_t slot; uint8_t used; uint8_t abs_mode; uint32_t frame; float x, y, z; } yk_emit_latch_t;
#define YK_EMIT_LATCH_N 64   /* ★ 8 → 16 → 32 → **64**：键现在带「同帧第几次」（多原点）⇒
                              *   条目数最多 ×`YK_EMIT_ORIGIN_MAX`（`st03 GirlD01_at` 一只敌人就 5 条）。*/
static yk_emit_latch_t s_emit_latch[YK_EMIT_LATCH_N];
static unsigned s_emit_latch_head;
static unsigned s_emit_latch_logged;      /* 判据：打过几行 `latched=` */
static unsigned s_emit_dist0;            /* ★ 硬约束回滚后：恒 0（保留格子只为日志口径稳定；我们**从不**写 props）*/

static void yk_emit_latch_put(uint32_t enemy_id, int slot, float x, float y, float z, int abs_mode)
{
    unsigned k;
    for (k = 0; k < YK_EMIT_LATCH_N; ++k) {
        yk_emit_latch_t *L = &s_emit_latch[k];
        if (L->used && L->enemy_id == enemy_id && L->slot == (int16_t)slot) {
            L->x = x; L->y = y; L->z = z;             /* 同一槽 ⇒ 覆盖成最新 */
            L->abs_mode = (uint8_t)(abs_mode ? 1 : 0);
            L->frame = ce_sdk_frame();                /* ★ phantom 修复：记帧号 = 「本波开过火」的时间戳 */
            return;
        }
    }
    {   yk_emit_latch_t *L = &s_emit_latch[s_emit_latch_head];
        s_emit_latch_head = (s_emit_latch_head + 1u) % YK_EMIT_LATCH_N;
        L->enemy_id = enemy_id; L->slot = (int16_t)slot; L->used = 1;
        L->abs_mode = (uint8_t)(abs_mode ? 1 : 0);
        L->x = x; L->y = y; L->z = z; L->frame = ce_sdk_frame();
    }
}

/* ★★★★ 2026-10-01（owner 实跑：一面道中 boss 出现「不是发弹点的使魔」）：
 *   **该槽最近有没有真的 `etOn` 过**（判据 = 站点 `0x433ffa` 写入的 latch 时间戳）。
 *   `window` 帧内算「开过火」——卡 tick（优先级 `0x16`）跑在敌人 ECL（`0x1b`）**之前**，
 *   所以首帧开火要下一帧我们才看得见 ⇒ 取一个小窗口（`YK_EMIT_FIRED_WINDOW`）。*/
#define YK_EMIT_FIRED_WINDOW 3
static int yk_emit_fired_recently(uint32_t enemy_id, int slot)
{
    unsigned k;
    uint32_t now = ce_sdk_frame();
    for (k = 0; k < YK_EMIT_LATCH_N; ++k) {
        const yk_emit_latch_t *L = &s_emit_latch[k];
        if (L->used && L->enemy_id == enemy_id && L->slot == (int16_t)slot &&
            (uint32_t)(now - L->frame) <= (uint32_t)YK_EMIT_FIRED_WINDOW)
            return 1;
    }
    return 0;
}

static uint8_t  g_fam_nobody_state;   /* ★ 无体术位只记一次日志 */
/* ★ 运行期验证环（owner：「似乎也还有体术」）：写后 / 下一帧各核一次。*/
#define YK_BODYCHK_N 4
static uint32_t s_bodychk_fam[YK_BODYCHK_N];
static uint32_t s_bodychk_after[YK_BODYCHK_N];
static uint8_t  s_bodychk_done[YK_BODYCHK_N];
static unsigned s_bodychk_head;
static unsigned s_bodywrite_logged;
/* ★ 距该槽最近一次 `etOn` 过了多少帧（诊断用；没记录过 ⇒ 返回 0xffff）。*/
static unsigned yk_emit_fired_age(uint32_t enemy_id, int slot)
{
    unsigned k;
    uint32_t now = ce_sdk_frame();
    for (k = 0; k < YK_EMIT_LATCH_N; ++k) {
        const yk_emit_latch_t *L = &s_emit_latch[k];
        if (L->used && L->enemy_id == enemy_id && L->slot == (int16_t)slot)
            return (unsigned)(now - L->frame);
    }
    return 0xffffu;
}

/* ★★★★★ 2026-10-11（owner：「血量太多」+「没有随波次重建」两条共用）——
 *   **该真实槽最近一次 `etOn` 的出点数**（本帧最大 `ord` + 1）。
 *   站点每次 `etOn` 都写它（同一帧内按 `%B-90` → `%B+90` 的顺序写，最后写 = 最大 ord）；
 *   带帧号 ⇒ 过期（`YK_EMIT_FIRED_WINDOW`）回落 1 ⇒ **单出点场景除数恒 1**。
 *   用途：① `YK_EMIT_HP_SPLIT_MODE` 的血量均分除数；② 诊断。*/
#define YK_ORGN_N 32
typedef struct { uint32_t enemy; uint8_t real; uint8_t n; uint32_t frame; } yk_orgn_t;
static yk_orgn_t s_orgn[YK_ORGN_N];
/* 表满 ⇒ 抢「最久没更新」的那一行（**LRU**：保证正在开火的敌人一定拿得到行 ——
 * 否则本局早先那些敌人的陈旧行会把表占死、5 面中 boss 永远读不到自己的出点数）。*/
static unsigned yk_orgn_lru(void)
{
    unsigned i, best = 0;
    for (i = 1; i < (unsigned)YK_ORGN_N; ++i)
        if (s_orgn[i].frame < s_orgn[best].frame) best = i;
    return best;
}
static void yk_origin_count_note(uint32_t enemy_id, int real, int ord)
{
    unsigned i;
    uint32_t fr = ce_sdk_frame();
    uint8_t n = (uint8_t)(ord + 1);
    for (i = 0; i < (unsigned)YK_ORGN_N; ++i)
        if (s_orgn[i].enemy == enemy_id && s_orgn[i].real == (uint8_t)real) {
            s_orgn[i].n = n; s_orgn[i].frame = fr; return;
        }
    for (i = 0; i < (unsigned)YK_ORGN_N; ++i)
        if (!s_orgn[i].enemy) {
            s_orgn[i].enemy = enemy_id; s_orgn[i].real = (uint8_t)real;
            s_orgn[i].n = n; s_orgn[i].frame = fr; return;
        }
    i = yk_orgn_lru();
    s_orgn[i].enemy = enemy_id; s_orgn[i].real = (uint8_t)real;
    s_orgn[i].n = n; s_orgn[i].frame = fr;
}
static int yk_slot_origin_count(uint32_t enemy_id, int key)
{
    unsigned i;
    int real = YK_EMIT_REAL_SLOT(key);
    uint32_t now = ce_sdk_frame();
    for (i = 0; i < (unsigned)YK_ORGN_N; ++i)
        if (s_orgn[i].enemy == enemy_id && s_orgn[i].real == (uint8_t)real)
            return ((uint32_t)(now - s_orgn[i].frame) <= (uint32_t)YK_EMIT_FIRED_WINDOW && s_orgn[i].n > 0u)
                   ? (int)s_orgn[i].n : 1;
    return 1;
}

/* ★★★★ 2026-10-11（owner：中 boss「**没有随波次重建**」；第五个放行口的**调参数据**）——
 *   每个 `(敌人,键)` 最近 8 次 `etOn` 的**间隔序列**。用于从间隙模式里找「波」的边界
 *   （`MBoss1` 系列：波内 12、波间 42；`st07mbs` 10 / 40）⇒ 决定 `YK_EMIT_WAVE_RATIO`。
 *   日志：每个键**前 8 次**必打（看得见 12,12,12,…），之后只在「本次间隔 ≥ `YK_EMIT_WAVE_GAP`」
 *   时打（= 疑似波边界，一局几十行、有上限）。*/
#define YK_GAPS_N 24
#define YK_GAPS_RING 8
typedef struct { uint32_t enemy; int16_t key; uint32_t last_frame; uint8_t n; uint8_t g[YK_GAPS_RING]; } yk_gaps_t;
static yk_gaps_t s_gaps[YK_GAPS_N];
static unsigned s_gaps_logged;
static void yk_gap_note(uint32_t enemy_id, int key, uint32_t fr)
{
    unsigned i;
    yk_gaps_t *e = 0;
    for (i = 0; i < (unsigned)YK_GAPS_N; ++i)
        if (s_gaps[i].enemy == enemy_id && s_gaps[i].key == (int16_t)key) { e = &s_gaps[i]; break; }
    if (!e)
        for (i = 0; i < (unsigned)YK_GAPS_N; ++i)
            if (!s_gaps[i].enemy) {
                e = &s_gaps[i]; e->enemy = enemy_id; e->key = (int16_t)key;
                e->n = 0; e->last_frame = 0; break;
            }
    if (!e) {
        /* 表满 ⇒ **LRU** 抢最久没更新的那一行（否则 boss 拿不到行、这一局的 GAPS 数据就没了）。*/
        unsigned best = 0;
        for (i = 1; i < (unsigned)YK_GAPS_N; ++i)
            if (s_gaps[i].last_frame < s_gaps[best].last_frame) best = i;
        e = &s_gaps[best];
        e->enemy = enemy_id; e->key = (int16_t)key; e->n = 0; e->last_frame = 0;
    }
    if (e->last_frame && fr > e->last_frame) {
        uint32_t g = fr - e->last_frame;
        uint8_t idx = (uint8_t)(e->n % YK_GAPS_RING);
        e->g[idx] = (uint8_t)(g > 255u ? 255u : g);
        if (e->n < 255u) ++e->n;
    }
    e->last_frame = fr;
}
static void yk_gap_log(uint32_t enemy_id, int key, int real, int ord, uint32_t fr)
{
    unsigned i;
    const yk_gaps_t *e = 0;
    unsigned n, base;
    uint8_t last;
    if (s_gaps_logged >= 64u) return;
    for (i = 0; i < (unsigned)YK_GAPS_N; ++i)
        if (s_gaps[i].enemy == enemy_id && s_gaps[i].key == (int16_t)key) { e = &s_gaps[i]; break; }
    if (!e || !e->n) return;
    n = (e->n > YK_GAPS_RING) ? (unsigned)YK_GAPS_RING : (unsigned)e->n;
    base = (unsigned)e->n - n;
    last = e->g[(unsigned)(e->n - 1u) % YK_GAPS_RING];
    /* 只打「前 8 次」或「疑似波边界」，避免刷屏 */
    if (!(e->n <= (uint8_t)YK_GAPS_RING || last >= (uint8_t)YK_EMIT_WAVE_GAP)) return;
    ++s_gaps_logged;
    /* ⚠ 不用 snprintf（本 TU 没链 stdio）⇒ 固定 8 个字段，未用的补 0 */
    {
        unsigned g[YK_GAPS_RING];
        for (i = 0; i < (unsigned)YK_GAPS_RING; ++i)
            g[i] = (i < n) ? (unsigned)e->g[(base + i) % YK_GAPS_RING] : 0u;
        ce_log("youkai/emit GAPS: enemy %u slot %d key %d ord %d gaps(last %u of %u)=%u,%u,%u,%u,%u,%u,%u,%u "
               "wave_thresh=%d (new wave = gap >= max(%d, %dx this key's own min gap)) frame=%u",
               enemy_id, real, key, ord, n, (unsigned)e->n,
               g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7],
               (int)YK_EMIT_WAVE_GAP, (int)YK_EMIT_WAVE_GAP, (int)YK_EMIT_WAVE_RATIO, (unsigned)fr);
    }
}

static unsigned s_coord_logged;    /* ★ 坐标审计已打几行 */
#define YK_COORD_LOG_N 8
static uint32_t s_coord_enemy[YK_COORD_LOG_N];
static int16_t  s_coord_slot[YK_COORD_LOG_N];
static unsigned s_coord_head;
/* ★ 前置声明（SLOTSTATE 打点在扫描里、helper 定义在文件后段）。*/
static int  yk_fired_sticky_get(uint32_t enemy_id, int slot);
static void yk_fired_sticky_clear(uint32_t enemy_id, int slot);
static int  yk_slotstate_seen(uint32_t enemy_id, int slot);
static int  yk_raw_seen(uint32_t enemy_id, int slot);

static int yk_emit_coord_seen(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_COORD_LOG_N; ++k)
        if (s_coord_enemy[k] == enemy_id && s_coord_slot[k] == (int16_t)slot) return 1;
    {   unsigned idx = s_coord_head % YK_COORD_LOG_N;
        s_coord_head = (s_coord_head + 1u) % YK_COORD_LOG_N;
        s_coord_enemy[idx] = enemy_id; s_coord_slot[idx] = (int16_t)slot;
    }
    return 0;
}

/* ★ 调试键（owner：「**一直以来的空格键都是调试**」）：开关 + 电平边沿状态。
 *   触发 = `(CE_DEBUG_SPACE_KEYS && ce_key_down(CE_VK_SPACE))`（`sdk.h:832`/`sdk.c:397`，`GetKeyboardState`）；
 *   ⛔ 旧的 `0x4ca428` 掩码方案（`YK_DEBUG_KEY_MASK/…_MASK2`）**已退役**（该掩码没有空格位）。*/
#define YK_DEBUG_SPACE_KILL 1
static int      s_dbg_space_prev;       /* 空格电平边沿 */
/* ★★★★ 2026-10-01 晚（owner：**「帮我写一个带108=无敌的 debug 功能」**）：
 *   自机无敌 —— 一手：`CE_PLAYER_INVULN_TIMER` = **`player+0x47774`**（`zTimer{prev,cur,float}`；
 *   复活 `0x45c35e` 置 `{0x117,0x118,280.0}`、决死救回 60、炸弹各自置 —— `engine.h:448/509/510`）。
 *   落笔 = **每帧把 `cur` 刷新到 ≥ 60**（计时器一读完就补，最稳；不动标志位、不动敌人、不动别的卡）。
 *   键 = **`I`（`0x49`）** —— 不在游戏操作里（`sdk.h` 的 `CE_VK_A/S/C/D/SPACE/CONTROL/SHIFT` 都不是它）；
 *   **按下切换 ON/OFF**（边沿检测、长按不连发）；日志每次一行。**调试功能，发布前可关**。*/
#define YK_DEBUG_INVINCIBLE 1
#define YK_DEBUG_INVINCIBLE_VK 0x49   /* 'I' */
#define YK_DEBUG_INVINCIBLE_MIN 60    /* 每帧把 cur 刷到至少这么多帧 */
static int      s_dbg_inv_on;
static int      s_dbg_inv_prev;
static int      s_dbg_inv_logged;
static unsigned s_dbg_ids_logged;       /* ★ 前 3 次按键打 ids 清单 */
static uint32_t s_dbg_kill_ids[8];      /* ★ 待验证的被杀 id */
static unsigned s_dbg_kill_n;
static uint32_t s_dbg_kill_frame;
static unsigned s_shared_logged;        /* ★ 「同出点共用壳」日志条数 */
static unsigned s_hp_logged;            /* ★ hp 日志条数 */
/* ★★★★ 2026-10-01 晚（owner：「它依旧是一个**离体的**使魔，**明明那里根本不是发弹口**」）：
 *   「真的出过弹」= 建壳的**前提**。观测 = **敌弹管理器计数** `CE_BM_BULLET_COUNT`（`0x7d0`，
 *   `engine.h:744`）的**帧间增长**：某槽最近一次 `etOn`（定格帧）落在「计数增长」的那一帧/前后一帧
 *   ⇒ 记「该槽出过弹」（粘性，本波内有效）。只 `etOn` 过而 **0 子弹** ⇒ 不建壳。*/
#define YK_PROD_N 64
static uint32_t s_prod_enemy[YK_PROD_N];
static int16_t  s_prod_slot[YK_PROD_N];
static unsigned s_prod_head;
static unsigned s_emit_nobullet;      /* REJECT(no-bullets) 计数 */
static int32_t  s_prev_bc;            /* 上一帧的敌弹总数 */

static void yk_prod_set(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_PROD_N; ++k)
        if (s_prod_enemy[k] == enemy_id && s_prod_slot[k] == (int16_t)slot) return;
    {   unsigned idx = s_prod_head % YK_PROD_N;
        s_prod_head = (s_prod_head + 1u) % YK_PROD_N;
        s_prod_enemy[idx] = enemy_id; s_prod_slot[idx] = (int16_t)slot;
    }
}
static int yk_prod_get(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_PROD_N; ++k)
        if (s_prod_enemy[k] == enemy_id && s_prod_slot[k] == (int16_t)slot) return 1;
    return 0;
}
static void yk_prod_clear(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_PROD_N; ++k)
        if (s_prod_enemy[k] == enemy_id && s_prod_slot[k] == (int16_t)slot) { s_prod_enemy[k] = 0u; s_prod_slot[k] = -1; return; }
}
/* ★ 临时调试旋钮（owner）：「发射口使魔血量暂时 1」；**0 = 关闭覆盖**（恢复正式口径）。*/
#define YK_EMIT_FAM_HP_OVERRIDE 0   /* ★ C0：1 → 0（Q2「壳只活几帧」立刻消失）*/
/* ★★★★★ 2026-10-11（**重新打开**，理由与 2026-10-01 那次关闭不同）：
 *   旧口径（逐字保留在此）：owner 看到「两个槽 ⇒ 一只壳」就否掉了它 —— 那时的现场是
 *   **slot 0 与 slot 1 的 `props+0x08` 被后写覆盖成同一点**（我们只抄到每槽最后一次 `etOn`），
 *   于是「同点共壳」把本该有的两只并成了一只。
 *   现在：键带「同帧第几次真发」（`YK_EMIT_ORIGIN_KEY`）⇒ 每槽的两个出点各成一条键。
 *   `st05/06/07mbs` 的 slot 0 与 slot 1 的**两个出点两两重合**（`etOffsetRad(1, …)` 与 slot 0
 *   逐字相同）⇒ 4 次 `etOn` 落在 **2 个点**上。同点共壳正好给出 owner 要的**每侧一只**（2 只），
 *   而不是 4 只两两重叠；`st06bs Boss1_984` 的 slot 1 更是 4 次 `etOn` 全在同一点（只有 `etDist`
 *   沿角度推进弹的**出生距离**，`props+0x08` 不变）⇒ 共壳给出 **1 只**（正确：那是一个发射口）。
 *   ⇒ 语义 = 「**一个出点 = 一只使魔**」；共壳的几条键由 `yk_fam_broken` 的**全表扫**一起关掉
 *     （打死那个出点的使魔 ⇒ 落在这个点上的**每一条** `etOn` 都停）。*/
#define YK_EMIT_SHARE_SAME_POS 1   /* ★ 2026-10-11：0 → 1（多原点；见上）*/
/* ★★★★★ 2026-10-11（owner 实跑：5 面中 boss「**使魔的血量有点太多了**」）——**血量预算按出点数均分**。
 *
 *   现象：每只壳都拿「母机 HP ÷3 × 机体系数」（5 面中 boss = 13000/3 ≈ 4333）；多原点之后
 *   同一槽有 2 个出点 ⇒ **2 只 × 4333** ⇒ 该槽的总血量**翻倍**。
 *   口径（owner 拍板）：**每槽的总血量预算不变**，按**该槽本帧的活跃出点数**均分
 *   （2 个出点 ⇒ 每只 4333/2；`st03 GirlD01_at` 5 个出点 ⇒ 每只 4333/5）。
 *   实现：除数 = 站点最近一次 `etOn` 写下的「该真实槽本帧最大 `ord` + 1」（`yk_origin_count_note`），
 *   带 3 帧时效（`YK_EMIT_FIRED_WINDOW`）⇒ **单出点场景除数恒 1、数值逐位不变**；
 *   每次重建 / 新波重新算（因为除数由**最近一次**发射事实给出）。
 *   ⚠ 同点共壳（`YK_EMIT_SHARE_SAME_POS`）下只有**第一只**真的 spawn，所以「一个出点一只」的
 *     总预算正好落在那一只上，不会重复扣。
 *   `0` = 旧口径（每只都拿满血）；`1` = 均分（默认）。两口径的数值见 `TUNING.md` §2.1ccc-89。*/
#define YK_EMIT_HP_SPLIT_MODE 1
static unsigned s_hpsplit_logged;       /* 均分日志行数 */
static uint32_t s_dbg_prev_input;       /* 仅用于排障日志 */
static unsigned s_dbg_mask_logged;

static unsigned s_post_logged;        /* ★ POST 打了几行 */
/* ★ 观测未证实前：**不把 produced 当硬性前提**（默认 0 = 关 ⇒ 距离闸是唯一闸）。*/
#define YK_EMIT_REQUIRE_BULLETS 0
/* ★★★ C1（只观测）：「同一帧内**该 `(enemy_id, slot)`** 的第 k 次 fire」——身份 = `(enemy, slot, k)`
 *   （C2 的正解：5 面中 boss 同帧两发、偏移 ±90°、每波旋转 ⇒ 不能用绝对位置当身份）。
 *   ⚠ 订正：k **必须按 (敌人,槽) 独立计数**（owner log：不同敌人也拿到 k=0/1 ⇒ 之前是全帧统一计数）。*/
typedef struct { uint32_t enemy; int16_t slot; uint32_t frame; uint8_t k; } yk_fk_t;
#define YK_FK_N 32
static yk_fk_t  s_fk[YK_FK_N];
static yk_fk_t  s_fkline[YK_FK_N];      /* 每敌 FIRE 行数 */
static yk_fk_t  s_fkline2[YK_FK_N];     /* 每敌 ORIGIN-GATE 行数 */
static unsigned s_fire_logged_2;        /* FIRE 总行数 */
static unsigned s_fire_seen;            /* ★ 无条件计数：站点上见过的 etOn 总次数（区分「没发生」与「块跑不到」）*/
static unsigned s_firesum_logged;       /* ★ FIRESUM 行数 */
static unsigned s_fcout_logged;         /* ★ FIRECOUNT 行数 */
static unsigned s_dh_body, s_dh_near, s_dh_far, s_dh_logged;   /* ★ DISTHIST 计数 */
static unsigned s_dh_nolatch, s_dh_nolatch_logged;   /* ★ 无定格值（props 直读）计数 */
static unsigned s_dh_near_logged;                    /* ★ NEARBAND 行数 */
static uint32_t s_pin_last_fam; static uint32_t s_pin_last_frame;   /* ★ TRACK：最近被钉的壳/帧 */
static float s_pin_last_x, s_pin_last_y;   /* ★ 实际喂给 yk_pin_familiar 的值 */
static uint32_t s_trk_enemy; static int s_trk_slot = -1; static int s_trk_left;   /* ★ TRACK 目标与剩余帧 */
static int s_trk_moving;   /* ★ 只在本帧母机位移 > 0.5px 时打 TRACK（owner：一局就能看出壳有没有跟着动）*/
static unsigned s_trk_noshell_logged;   /* ★ (no shell yet) 只打一次 */
static unsigned s_site_logged;          /* ★ SITE(call#>=2) 行数 */
static unsigned s_mo_logged;            /* ★ 2026-10-11 MULTI(多原点第 2..N 次) 行数 */
static unsigned s_og_logged;            /* ORIGIN-GATE 总行数 */

static int yk_fk_bump(uint32_t enemy, int slot, uint32_t fr)
{
    unsigned i;
    for (i = 0; i < YK_FK_N; ++i)
        if (s_fk[i].enemy == enemy && s_fk[i].slot == (int16_t)slot) {
            if (s_fk[i].frame != fr) { s_fk[i].frame = fr; s_fk[i].k = 0; }
            else if (s_fk[i].k < 8u) ++s_fk[i].k;
            return (int)s_fk[i].k;
        }
    for (i = 0; i < YK_FK_N; ++i)
        if (!s_fk[i].enemy) { s_fk[i].enemy = enemy; s_fk[i].slot = (int16_t)slot; s_fk[i].frame = fr; s_fk[i].k = 0; return 0; }
    /* ★ 2026-10-11（实跑反证：整局 `maxk_by_slot` 恒 0 —— 旧版在这里直接 `return 0`，而 32 格
     *   会被**本局早先**见过的 (敌人,槽) 永久占满 ⇒ 5 面中 boss 的第 2 次 `etOn` 永远拿不到 k=1）。
     *   ⇒ 表满时**抢占一个不属于本帧的旧格**（它已经没用了：k 只在同帧内有意义）。*/
    for (i = 0; i < YK_FK_N; ++i)
        if (s_fk[i].frame != fr) {
            s_fk[i].enemy = enemy; s_fk[i].slot = (int16_t)slot; s_fk[i].frame = fr; s_fk[i].k = 0;
            return 0;
        }
    return 0;
}
static unsigned yk_fk_lines(yk_fk_t *t, uint32_t enemy)
{
    unsigned i;
    for (i = 0; i < YK_FK_N; ++i) if (t[i].enemy == enemy) return (unsigned)t[i].k;
    for (i = 0; i < YK_FK_N; ++i) if (!t[i].enemy) { t[i].enemy = enemy; t[i].k = 0; return 0u; }
    return 99u;
}
static void yk_fk_inc(yk_fk_t *t, uint32_t enemy)
{ unsigned i; for (i = 0; i < YK_FK_N; ++i) if (t[i].enemy == enemy) { ++t[i].k; return; } }
static unsigned yk_fk_lines2(yk_fk_t *t, uint32_t enemy)
{ unsigned i; for (i = 0; i < YK_FK_N; ++i) if (t[i].enemy == enemy) return (unsigned)t[i].k; return 0u; }

/* ★★★★ 2026-10-12z66（**订正**）：敌弹池的**基址与 state 偏移**以**命名金矿 + 我方实测对账**为准 ——
 *   · 命名金矿（`th-re-data/.../type-structs-own.json`）：`zBulletManager.bullets[0x7d0]` = **mgr+0x108c**、
 *     每张 **0xfa0**、`zBullet.state` = **+0xf68（uint16）**、`zBullet` 整好 **0xfa0** 字节 ✓；
 *   · 与 `engine.h` 的 `CE_BM_BULLETS(0xec)` 完全自洽：`0xec + 0xfa0 = 0x108c` ✓（0xec = list_0 的哨兵弹）；
 *   · 我方只读对账：按这套读出来的非零槽数 **≡ 活弹链表数 + 1 个哨兵**，逐帧吻合 ✓✓。
 *   ⛔ **旧注释写的 `bm + 0x5a8`（state `+0xaac`）是错的** ✗（两者基址差 0xAE4、state 差 0x4BC，
 *      对不上任何自洽口径）⇒ 照它扫会读到**垃圾**（本卡旧诊断 `yk_bm_live_bullets()` 就是如此 ✗）。
 *   ⇒ 需要"活弹"语义时**一律用 `CE_BULLET_LIVE_FOR_EACH`**（`engine.h`）✓。*/
static unsigned s_bm2_logged;
static int32_t  s_bm_prev_live = -1;

static unsigned yk_bm_live_bullets(void)
{
    unsigned n = 0;
    /* ★ z66：改走**活弹链表**（旧实现按错的基址 0x5a8 扫 ⇒ 数是垃圾 ✗，且判据含哨兵 ✗）。*/
    CE_BULLET_LIVE_FOR_EACH(b) {
        (void)b;
        ++n;
    }
    return n;
}

static unsigned s_bmprobe_logged;     /* ★ BM 原始探针已打几帧 */
static unsigned s_og_logged;          /* ★ ORIGIN-GATE 行数 */
static int32_t  s_cur_bc;             /* 本帧敌弹总数 */
static int      s_cur_bc_grew;        /* 本帧是否增长 */
static unsigned s_possrc_logged;      /* ★ POSSRC 打了几行 */
#define YK_POSSRC_N 128
static uint32_t s_possrc_enemy[YK_POSSRC_N];
static int16_t  s_possrc_slot[YK_POSSRC_N];
static unsigned s_possrc_head;
static int yk_possrc_seen(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_POSSRC_N; ++k)
        if (s_possrc_enemy[k] == enemy_id && s_possrc_slot[k] == (int16_t)slot) return 1;
    {   unsigned idx = s_possrc_head % YK_POSSRC_N;
        s_possrc_head = (s_possrc_head + 1u) % YK_POSSRC_N;
        s_possrc_enemy[idx] = enemy_id; s_possrc_slot[idx] = (int16_t)slot;
    }
    return 0;
}

static unsigned s_raw_logged;         /* ★ RAW 打了几行 */
#define YK_RAW_N 128
static uint32_t s_raw_enemy[YK_RAW_N];
static int16_t  s_raw_slot[YK_RAW_N];
static unsigned s_raw_head;
static int yk_raw_seen(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_RAW_N; ++k)
        if (s_raw_enemy[k] == enemy_id && s_raw_slot[k] == (int16_t)slot) return 1;
    {   unsigned idx = s_raw_head % YK_RAW_N;
        s_raw_head = (s_raw_head + 1u) % YK_RAW_N;
        s_raw_enemy[idx] = enemy_id; s_raw_slot[idx] = (int16_t)slot;
    }
    return 0;
}

static unsigned s_slotstate_logged;   /* ★ SLOTSTATE 打了几行 */
#define YK_SLOTSTATE_N 256
static uint32_t s_slotstate_enemy[YK_SLOTSTATE_N];
static int16_t  s_slotstate_slot[YK_SLOTSTATE_N];
static unsigned s_slotstate_head;
static int yk_slotstate_seen(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_SLOTSTATE_N; ++k)
        if (s_slotstate_enemy[k] == enemy_id && s_slotstate_slot[k] == (int16_t)slot) return 1;
    {   unsigned idx = s_slotstate_head % YK_SLOTSTATE_N;
        s_slotstate_head = (s_slotstate_head + 1u) % YK_SLOTSTATE_N;
        s_slotstate_enemy[idx] = enemy_id; s_slotstate_slot[idx] = (int16_t)slot;
    }
    return 0;
}

static unsigned s_gatecheck_logged;  /* ★ gate check 打了几行 */
static unsigned s_emit_badtype;    /* ★ type 在黑名单 ⇒ 不建壳的槽数 */
static unsigned s_emit_noorigin;   /* ★ 出点 (0,0)/无定格 ⇒ 不建壳的槽数 */
static int      s_cur_have_latch;  /* 本槽本帧有没有定格值（循环内临时量）*/
/* ★ 2026-10-11（静态核实后）：本槽本帧的位置来源，四选一 —— `latch-abs` / `latch-rel` / `live-abs` / `live-rel`。
 *   `latch-*` = 站点 `0x433ffa` 记下的「上一发 `etOn` 用的点」（绝对模式存世界坐标、相对模式存相对量）；
 *   `live-*`  = 没有定格值时按引擎闭式每帧重算（`origins[z]>0.9` 判绝对）。只用于日志判读。*/
static const char *s_cur_pos_src = "?";
static unsigned s_sample_logged;   /* ★ SAMPLE 数据行已打几条 */
#define YK_SAMPLE_LOG_N 48u   /* 12 → 48：5 面那类多槽场景 12 行不够看 */
static unsigned s_emit_notfam;     /* （保留：旧「大怪闸」的计数，闸已撤 ⇒ 恒 0）*/
#define YK_REJECT_LOG_N 8
static uint32_t s_reject_enemy[YK_REJECT_LOG_N];
static int16_t  s_reject_slot[YK_REJECT_LOG_N];
static unsigned s_reject_head;
static int yk_emit_reject_seen(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_REJECT_LOG_N; ++k)
        if (s_reject_enemy[k] == enemy_id && s_reject_slot[k] == (int16_t)slot) return 1;
    {   unsigned idx = s_reject_head % YK_REJECT_LOG_N;
        s_reject_head = (s_reject_head + 1u) % YK_REJECT_LOG_N;
        s_reject_enemy[idx] = enemy_id; s_reject_slot[idx] = (int16_t)slot;
    }
    return 0;
}

static unsigned s_emit_bodyport;   /* ★ 本体口（距离 < 阈值）⇒ 不建壳的槽数 */
#define YK_EMIT_BODYPORT_LOG_N 8
static uint32_t s_bodyport_enemy[YK_EMIT_BODYPORT_LOG_N];
static int16_t  s_bodyport_slot[YK_EMIT_BODYPORT_LOG_N];
static unsigned s_bodyport_head;

/* 距离（诊断/日志用；无浮点返回值 ⇒ 返回 float 会过 x87，这里只给「平方」辅助）。*/
static float yk_emit_dist(float dx, float dy) { return dx * dx + dy * dy; }  /* 平方（BM 探针用）*/
/* ★ C1 顺手：真实距离（**无 sqrt 调用** —— i686 的 sqrt 走 x87；这里 3 次牛顿迭代足够打印用）。*/
static float yk_emit_len(float dx, float dy)
{
    float a = dx < 0.0f ? -dx : dx, b = dy < 0.0f ? -dy : dy, g, h;
    if (a < b) { g = b; h = a; } else { g = a; h = b; }
    if (g <= 0.0f) return 0.0f;
    h = h / g;
    {   float x = 0.5f * (1.0f + h), y = 0.5f * (h / x), z = 0.5f * (x + y), w = 0.5f * (z + h / z);
        return g * w;
    }
}

static int yk_emit_bodyport_seen(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_EMIT_BODYPORT_LOG_N; ++k)
        if (s_bodyport_enemy[k] == enemy_id && s_bodyport_slot[k] == (int16_t)slot) return 1;
    {   unsigned idx = s_bodyport_head % YK_EMIT_BODYPORT_LOG_N;
        s_bodyport_head = (s_bodyport_head + 1u) % YK_EMIT_BODYPORT_LOG_N;
        s_bodyport_enemy[idx] = enemy_id; s_bodyport_slot[idx] = (int16_t)slot;
    }
    return 0;
}

/* ★★★★ 2026-10-01 晚（owner：「**5 面道中目前没有发射点使魔**」）：
 *   「开过火」原来是**窄窗口**（`YK_EMIT_FIRED_WINDOW = 3` 帧）⇒ 对 `etOn` 间隔 **12/30 帧**的 ECL
 *   （5 面道中那类非符）经常抓不到 ⇒ 建壳只在"恰好抓到"的帧发生 ⇒ 观感 = **没有使魔**。
 *   ⇒ 改成**本波内粘性**：该 `(enemy_id,slot)` **`etOn` 过一次**就记住，直到
 *     ① 该槽变不活跃（`!active`）② 换场/换波（整表 reset）才清 —— 与 core 里 `yk_slot_t.fired`
 *     的清理口径一致（`!active` / `yk_fam_wave_clear`）。*/
#define YK_FIRED_STICKY_N 64   /* 32 → 64：5 面多槽 + 多敌机时防挤出 */
static uint32_t s_fired_enemy[YK_FIRED_STICKY_N];
static int16_t  s_fired_slot[YK_FIRED_STICKY_N];
static unsigned s_fired_head;

static void yk_fired_sticky_set(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_FIRED_STICKY_N; ++k)
        if (s_fired_enemy[k] == enemy_id && s_fired_slot[k] == (int16_t)slot) return;   /* 已有 ⇒ 保持 */
    {   unsigned idx = s_fired_head % YK_FIRED_STICKY_N;
        s_fired_head = (s_fired_head + 1u) % YK_FIRED_STICKY_N;
        s_fired_enemy[idx] = enemy_id; s_fired_slot[idx] = (int16_t)slot;
    }
}

static int yk_fired_sticky_get(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_FIRED_STICKY_N; ++k)
        if (s_fired_enemy[k] == enemy_id && s_fired_slot[k] == (int16_t)slot) return 1;
    return 0;
}

static void yk_fired_sticky_clear(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_FIRED_STICKY_N; ++k)
        if (s_fired_enemy[k] == enemy_id && s_fired_slot[k] == (int16_t)slot) {
            s_fired_enemy[k] = 0u; s_fired_slot[k] = -1; return;
        }
}

static unsigned s_emit_phantom;   /* ★ phantom 修复判据：**配了但从没开火 ⇒ 不建壳** 的槽数（去重后）*/
static unsigned s_shell_logged;   /* ★ 建壳诊断已打几行 */
#define YK_EMIT_PHANTOM_LOG_N 8
static uint32_t s_phantom_enemy[YK_EMIT_PHANTOM_LOG_N];
static int16_t  s_phantom_slot[YK_EMIT_PHANTOM_LOG_N];
static unsigned s_phantom_head;

/* 同一个 `(敌人, 槽)` 的 phantom 诊断只打一次。*/
static int yk_emit_phantom_seen(uint32_t enemy_id, int slot)
{
    unsigned k;
    for (k = 0; k < YK_EMIT_PHANTOM_LOG_N; ++k)
        if (s_phantom_enemy[k] == enemy_id && s_phantom_slot[k] == (int16_t)slot) return 1;
    {   unsigned idx = s_phantom_head % YK_EMIT_PHANTOM_LOG_N;
        s_phantom_head = (s_phantom_head + 1u) % YK_EMIT_PHANTOM_LOG_N;
        s_phantom_enemy[idx] = enemy_id; s_phantom_slot[idx] = (int16_t)slot;
    }
    return 0;
}

/* 取该 `(敌人, 槽)` 的定格值；返回 0 = 还没发过弹（调用方回落到**活偏移/原点重算**）。
 * ★ `out_abs` 回填模式：1 = 存的是**绝对世界坐标**（`etOffsetAbs`），0 = 存的是**相对量**（要加当前敌人位置）。*/
static int yk_emit_latch_get(uint32_t enemy_id, int slot, float out3[3], int *out_abs)
{
    unsigned k;
    for (k = 0; k < YK_EMIT_LATCH_N; ++k) {
        const yk_emit_latch_t *L = &s_emit_latch[k];
        if (L->used && L->enemy_id == enemy_id && L->slot == (int16_t)slot) {
            out3[0] = L->x; out3[1] = L->y; out3[2] = L->z;
            if (out_abs) *out_abs = (int)L->abs_mode;
            return 1;
        }
    }
    if (out_abs) *out_abs = 0;
    return 0;
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

/* ★★ 2026-10-15 **leveledit（第三方米斯蒂亚补丁）「同开」门** —— owner 口径见
 * [`HANDOFF-108-LEVELEDIT-FAMILIARS.md`](../../HANDOFF-108-LEVELEDIT-FAMILIARS.md) §1 ✓：
 *   ① 那场 boss 战时**不建我们自建的使魔壳** ✓；
 *   ② 把**他们的**使魔当我们的使魔接管 ✓（下一批：喂进使魔台账 ⇒ **1/2 转伤** + 参与**人妖切换** ✓）；
 *   ③ **没装 108 时，绝不对它们做形态切换** ✗ —— 由"形态切换全部挂在本卡上"天然保证 ✓。
 *
 * ★★ 2026-10-17（老 goal 卡住的那半**收口** ✓）：判据已由**两份 dump 实测**钉死 ✓ ⇒ `YK_LE_TRUSTED = 1` ✓
 *   本门**不再恒惰性** ✗ —— 命中条件 = **boss ECL VM 的 `subroutine_index >= 24`** ✓
 *   （零售 `st02bs.ecl` 只有 **24 个 sub**、最大索引 23 ✓；leveledit 那份 **62 个** ✓ —— 详见头文件那段 ✓）。
 *   ⚠ `+0x270` / `+0x28c` 实测**没有区分度** ✗ ⇒ 仍读出来只为留探针旁证 ✓。
 *     ★★ 2026-10-18 **订正**：那两个「没有区分度」的结论建立在**基址读错**之上 ✗ —— 它们是
 *     `zEnemyData` 相对偏移，当时却直接加在 `zEnemy*` 上（少 0x122c，见上面探针那段的一手证据 ✓）。
 *     现在改成 `CE_ENEMY_DATA + …` ⇒ 下一份实跑日志才是这两格的**真值**（仍**不作判据** ✓）。
 * 读的都是 boss 敌机上的**只读字段** ✓，偏移出处 = vendor `zEnemyData` / `zEnemy`（与探针同源 ✓）。*/
#define YK_LE_B_SUB274        (CE_ENEMY_DATA + 0x274)   /* __probably_cur_subscript_for_anm_main（★ 不作判据 ✗）*/
#define YK_LE_B_ANM_MAIN      (CE_ENEMY_DATA + 0x270)
#define YK_LE_B_IV0           (CE_ENEMY_DATA + 0x28c)   /* ecl_int_vars[0] ✓ */
/* ★ ECL VM 链（**全在 th18 的 vendor 里有一手** ✓）：
 *   `zEnemy+0x0 = struct zEclVm`（内嵌 ✓）→ `+0xc = zEclRunContextHolder`（内嵌 ✓）
 *   → `holder+0x0 = current_context`（`zEclRunContext*` ✓）→ `+0x4 = cur_location`（内嵌 `zEclLocation` ✓）
 *   → `+0x0 = subroutine_index`（int32 ✓）
 *   ⚠ 起点订正（2026-10-18）：vendor 的 `zEnemy` 表写着 `+0x0 = struct zEclVm`、`+0x122c = struct
 *     zEnemyData` ⇒ **`zEnemyData = zEnemy + CE_ENEMY_DATA(0x122c)`**。本函数的实参 `b` 来自
 *     `yk_boss()` = `ce_enemy_by_id()` ⇒ **是 `zEnemy*`** ⇒ `zEclVm` 就在 `b` 上，
 *     **不要再减 0x122c**（旧代码 `vm = b - 0x122c` 是反的 ✗：它把 `b` 当成 `zEnemyData*`，
 *     结果读到堆上 `zEnemy` 之前 0x122c 字节的随机内存 —— 那正是「判据看似有值、实为噪声」的来源）。
 *     这条与上面探针段的一手证据同一套（`CE_ENEMY_ID` = `zEnemy.enemy_id` / `CE_ENEMY_DATA` =
 *     `zEnemyData` 在 `zEnemy` 里的偏移 / `CE_ENEMY_ECL_I0 0x14b8 = 0x122c+0x28c` 的三重旁证 ✓）。*/
#define YK_LE_ENEMY_DATA_OFF  0x122c  /* = CE_ENEMY_DATA ✓（只用于想要 zEnemyData 时**加**它 ✓）*/
#define YK_LE_VM_CTX_HOLDER   0x0c    /* zEclVm.context ✓ */
#define YK_LE_HOLDER_CUR      0x00    /* zEclRunContextHolder.current_context ✓ */
#define YK_LE_CTX_CURLOC      0x04    /* zEclRunContext.cur_location ✓ */
#define YK_LE_LOC_SUBIDX      0x00    /* zEclLocation.subroutine_index ✓ */
static int yk_le_boss_sub_index(const uint8_t *b)
{
    const uint8_t *vm, *holder;
    const uint8_t *ctx;
    if (!b) return -1;
    vm     = b;                                            /* `b` 已是 zEnemy*（zEclVm 内嵌在 +0x0）✓ */
    holder = vm + YK_LE_VM_CTX_HOLDER;                     /* zEclRunContextHolder（内嵌）✓ */
    ctx    = *(const uint8_t *const *)holder;              /* current_context ✓ */
    if (!ctx) return -1;                                   /* 没在跑 ⇒ 不命中 ✓ */
    return *(const int32_t *)(ctx + YK_LE_CTX_CURLOC + YK_LE_LOC_SUBIDX);
}
static int yk_le_here(void)
{
    uint8_t *b = yk_boss();
    if (!b) return 0;
    return yk_le_mode(1 /* 本卡在场 ✓ */,
                      yk_le_boss_sub_index(b),             /* ★ 判据 = ECL subroutine_index ✓ */
                      *(const int32_t *)(b + YK_LE_B_ANM_MAIN),
                      *(const int32_t *)(b + YK_LE_B_IV0));
}

/* ★★ B2（2026-10-18）：读一只敌机的 **`$I3`**（= `zEnemyData.ecl_int_vars[3]` = `zEnemy+0x14c4`）。
 *   只读 ✓；偏移/一手出处见 `YK_E_ECL_IVARS3` 那段（leveledit `translate.py:207-217` ✓）。
 *   唯一消费点 = `yk_transfer_tick()` 的接管判据（`yk_le_is_their_familiar(yk_le_iv3(e))` ✓）。*/
static int32_t yk_le_iv3(const uint8_t *e)
{
    return e ? *(const int32_t *)(e + YK_E_ECL_IVARS3) : 0;
}

/* ★★★ 2026-10-19c（owner：「接管没有生效 / 人侧没有消失 / 切换没有音效」+「最简单的做法应当是
 *   把我们这套使魔驱动放进那个包」）：**改成按他们那个包自己的口径认**，不再只靠 `$I3` 标记。
 *
 * 一手（他们仓库，本机就有 `/home/nipporita/th18-leveledit/mods/stage02-mystia/`）：
 *   · `familiar_visuals.py:39` 的 `CHILDREN = {28:28, 30:28, 37:28, 43:28, 47:29, 49:29, 55:29, 59:28}`
 *     ⇒ 他们那 8 个 `MystiaChild*` 的**主 anm 脚本只有两支**：**28 / 29**（两种使魔图像）；
 *     `README` 也写「enemy.anm 追加 scripts 28/29 —— 移植两种使魔图像」✓。
 *   · 运行时读法 = `zEnemyData + 0x26c`（`CE_ENEMY_ANM_SCRIPT` = `zEnemy+0x1498`）；这是**主脚本**
 *     （老板用它读出来是 21，正是他们 `anmSetMain(0, 21)` 写的值 ✓）。
 *   · **实跑 02:09 那局反证 `$I3` 那条不可靠**：`le-probe(enemy): id=3/4/6/7 … slot0=29`（= 他们的
 *     使魔确实在场上）而 `iv3=0`，整局 **0 行 MARK** ⇒ 只看 `$I3` 永远接管不到 ✗。
 *   ⇒ 判据改成 **`$I3 == 997`（他们 ECL 的标记，留作兜底）‖ 主脚本 ∈ {28,29}**，
 *     并且**必须**在「他们那场」里（脚本 28/29 是他们**追加**的，零售局同号脚本可能是别的用途 ⇒
 *     不做场地门会误伤零售敌机 ✗）。`le_here` 用**粘性**（同一面里确认过一次就算，见 `s_le_seen_stage`）：
 *     他们的 boss 在 `Boss()`/`BossDead()` 那些 sub<24 的时刻 `le_here` 会掉回 0，
 *     粘性保证「这一面是他们的」这件事不会因为一帧读不到 boss 就丢 ✓。*/
#define YK_LE_FAM_ANM_A  28
#define YK_LE_FAM_ANM_B  29
static int32_t s_le_seen_stage = -1;   /* 哪一面确认过「这是 leveledit 那场」（-1 = 还没确认）*/

static int yk_le_fam_by_anm(const uint8_t *e, int le_stage)
{
    int32_t sl0;
    if (!e || !le_stage) return 0;
    sl0 = *(const int32_t *)(e + CE_ENEMY_DATA + 0x26c);
    return (sl0 == YK_LE_FAM_ANM_A || sl0 == YK_LE_FAM_ANM_B) ? 1 : 0;
}


/* ★★★ 2026-09-22（P3 第二批）：「射击中」的判据（TH18 侧）。
 * TH08 是 `Player::shotActive`（一个 999 帧的射击周期标志，`th08:0x17d6ed4`）；TH18 没有同一个
 * 状态量（每帧按 rate 发弹，没有周期）⇒ 本卡用**射击键按住**（`CE_INPUT_HELD` 的 bit0）。
 * ❓ 口径，登记 `TUNING.md` §2.0y。*/
/* ★ 2026-10-11 **桥**（owner 定案 ✓）：「留一个不会被改的 Z 位置」⇒ ★ 本卡在 `on_tick_2` 最开头
 *   把键盘 Z 抄进这里 ✓（★ 卡按 id 跑 ⇒ **108 先于 109** ✓ ⇒ 109 之后清输入位**影响不到本卡** ✓）。*/
static uint32_t s_z_bridge;

static int yk_shooting(void)
{
    /* ★★★★★ 2026-10-11 **owner 定的"搭桥"**（比走闸简单 ✓）：
     *   ★ owner：「有没有可能**两边搭桥**？相当于**留一个不会被改的 Z 位置**？」
     *   ★ 背景：★ 卡 109 蓄力期间要**阻断发弹** ✓ ⇒ 它会**清掉输入块 `0x4ca428` 的 bit0** ✓；
     *     ★ 而本卡（108 灵梦妖子机）的"按 Z ⇒ 子机本体追踪靠近怪"要读**键盘 Z** ✗ ⇒ 会被误伤 ✗。
     *   ★ 桥 = ★ **本卡在自己 `on_tick_2` 的最开头把键盘 Z 抄一份**（`s_z_bridge` ✓）⇒ ★ 之后 109 怎么清
     *     原始位都与本卡无关 ✓。★ 时序依据：★ **卡按 id 顺序跑** ✓（本仓已定论「卡按 id 顺序跑 ⇒ 111 > 109」✓）
     *     ⇒ ★ **108 先于 109** ✓ ⇒ ★ 抄到的就是**本帧真实键盘 Z** ✓。
     *   ★ 为什么这样最省：★ 不用新增共享层 API ✓、不用改引擎射击路径 ✓、★ 109 那边可以继续用"清位"这个
     *     **覆盖一切子机**的暴力解 ✓（owner：茫茫多的子机 ✓）。*/
    return (s_z_bridge & 0x1u) != 0;
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
/* ★★★ 2026-09-28（第四十九批）：加一个 `parent_id` 实参 ——
 *   `0` = 老语义（1:1 映射使魔，链根）；`!= 0` = **惰性绕转使魔**（父敌 id，见下面那一节）。
 *   ⚠ 唯一不同的一格就是 `CE_SPAWN_PARENT`：它同时决定
 *     「`yk_kill_burst` 数活子敌人时把这一台算进去」（颗数！）与「父敌没了它就该撤」。
 *     ★ 2026-10-01：这里是**写 spawn 参数**（0x50），引擎会把它拷进 `zEnemy+0x6834`
 *       （见 `YK_ENEMY_PARENT_ID` 那条长注）—— **读敌人对象时要用 `YK_ENEMY_PARENT_ID`**。*/
static uint8_t *yk_spawn_familiar_parent(float x, float y, uint32_t parent_id, int32_t life)
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
    *(int32_t *)(spawn + CE_SPAWN_PARENT) = (int32_t)parent_id;
    /* `CE_SPAWN_FLAG_A/B` 一律不设：`flags_lo & 0x31` 绝不能置（置位 = 伤害免疫，spec §3.9 一手），
     * 用户定案 4 也是「不设」。life 钳在 < 1000（`CE_SPAWN_LIFE` 的 ⚠）。*/
    /* 血量：`life` = **真血量**（调用方给，**可以 >999**）。写成两步（照 `cards/ufo.c:3518` 的先例）：
     *   ① spawn 参数 `+0x14` 只写 **`yk_clamp_spawn_life(life)`**（≤999）—— 因为
     *      `EnemyManager__allocate_new_enemy` 在 **spawn 参数** life ≥ 1000（`0x3e8`）时会置
     *      `flags_lo |= 0x40000000`（`th18:0x42d8ed`/`0x42d8f6`，不可撤销的「大怪」标记，见
     *      `YK_LIFE_LIMIT`）；
     *   ② 建出来之后，若真血量 >999，**回写三格 life**（`enemy+0x6220/0x6224/0x6228`）——
     *      这条路**不置位**（一手：`0x42d815`–`0x42d82a` 引擎自己就是这么写三格的）。
     * ★★★ 2026-10-01（owner 定案口径「母机 HP / 圈数」+ parent 查证）：本卡**现在真的会**走第 ② 步
     *   （`yk_orbit_fam_hp()` 不再封顶 999：3200 血 3 圈 ⇒ 1066 > 999）。开关 = `life > spawn_life`。*/
    *(int32_t *)(spawn + CE_SPAWN_LIFE) = yk_clamp_spawn_life(life);
    /* 惰性空壳：`NoEnemyDebug` 的 `wait(1000)` 死循环 ⇒ 不跑 ECL、不动、不开火、不掉落、不自删。
     * ★ 2026-10-11（纯静态核实；`engine/enemy/th18/09-enemy-offscreen-removal.md`）：
     *   用 **`ce_spawn_enemy_keep_offscreen()`** —— 否则壳一「出框」就被引擎按精灵盒判据删掉
     *   （`th18:0x42e5a0` 返回 -1 ⇒ `0x42fe80` ⇒ 管理器 `call [vtable+0x14](1)` = `th18:0x42d220` 摘链释放）。
     *   使魔壳**本来就该待在发射口上**，而发射口（`母机位置 + offsets`）经常在画面外 ⇒ 必须免删。
     *   掩码 `0xC` = `flags_lo` bit2|bit3，也是 ZUN 原生 ECL 在会飞出画面的敌人上用的 `flagSet(12)`。*/
    {
        /* ★★ 2026-10-15 leveledit「同开」门（口径 ①）：那场里**不建**自建壳 ✓。
         * 门现在是**惰性**的 ✓（`yk_le_here()` 恒 0 —— 判别常量还没实测 ✓），所以这一行今天
         * 与原来完全等价 ✓；常量钉死后它会直接跳过建壳 ✓，而下面所有 `if (e …)` 分支自然都不跑 ✓。*/
        uint8_t *e = yk_le_here() ? 0 : (uint8_t *)ce_spawn_enemy_keep_offscreen("NoEnemyDebug", spawn);
        /* ★ 第二步：真血量 >999 ⇒ 出生后回写三格 life（**只有这条路不置位 30**）。
         *   `yk_clamp_spawn_life(life)` 是幂等的纯函数，这里再算一次只为拿到「spawn 值」。*/
        if (e && life > 0 && life > yk_clamp_spawn_life(life)) {
            *(int32_t *)(e + YK_ED_LIFE_CUR)     = life;   /* current   （0x6220）*/
            *(int32_t *)(e + YK_ED_LIFE_CUR + 4) = life;   /* maximum   （0x6224）*/
            *(int32_t *)(e + YK_ED_LIFE_CUR + 8) = life;   /* remaining （0x6228）*/
        }
        return e;
    }
}

/* 1:1 映射（发射口）使魔：**链根**（`parent_id = 0`）。
 * ★ 2026-10-01（owner 口径 ②）：`life` 从常数 100 改成**调用方按母机 HP 算**（`yk_map_fam_hp()`）；
 *   调用方读不到母机 HP 时回落 `YK_FAM_LIFE`（= 100，兜底保留）。*/
static uint8_t *yk_spawn_familiar(float x, float y, int32_t life)
{
    return yk_spawn_familiar_parent(x, y, 0u, life > 0 ? life : (int32_t)YK_FAM_LIFE);
}

/* 按 `enemy_id` 取敌人的 `life.maximum`（`CE_ENEMY_HP_MAX` = `enemy+0x6224`）；0 = 查不到 / 读不到。
 * ⚠ 只在**建壳那一刻**读（映射使魔用；母机 HP 是常数，重建时重算同值）。*/
static int32_t yk_enemy_hp_max_by_id(uint32_t id)
{
    uint8_t *e = id ? ce_enemy_by_id(CE_ENEMY_MGR(), (int32_t)id) : 0;
    return e ? *(const int32_t *)(e + CE_ENEMY_HP_MAX) : 0;
}

/* 每帧钉位置（spec §3.5）+ 皮肤（资产未到时不动）。*/
static uint8_t g_fam_nodmg_state = 0xff;   /* ★ 批次 2：上一次记过的免伤位状态（0/1；0xff = 还没记过）*/

/* ★★★★ 2026-10-01（owner 拍板 **D2 = 插值跟随**）：映射壳的**位置状态**（每个槽一份，按 `s_map.e[]`
 *   的**下标**存 —— 下标在条目存活期间稳定；`fam_id` 用来识别「这一格换了一只新壳」⇒ 重新初始化）。
 *   为什么另开一个平行数组而不塞进 `yk_slot_t`：那张表已有 `sizeof` 上限度量（`tests` 里盯着），
 *   这里 32 × 28 B = 896 B 是文件级静态，不进卡的 496 B 预算。
 *   语义见 `youkai_core.h` 的 `yk_follow_step()`：目标 = 该槽**当前世界坐标**（`sl->x/sl->y`，
 *   它由 `enemy+0x4e58+i*0x0c` 的偏移向量派生 ⇒ 偏移一变、目标就变，这正是 owner 要的
 *   「用偏移向量的帧间变化当换地方信号」），起点 = 变化那一刻壳在哪 ⇒ 60 帧滑过去。*/
/* ★★★★ 2026-10-01（owner：「至少得确定瞬移不会重建血量吧」）——**实跑可核**的三个计数（只加日志）：
 *   `emit move` = 「目标（= 槽世界坐标，由 `enemy+0x4e58+i*0x0c` 的偏移向量派生）变化过几次」；
 *   `create` / `withdraw` = 我们的壳建立/撤销次数（复用既有 `s->spawns` / `s->lost + s->silent`）；
 *   `emit hp_reset` = **壳还活着却又走了一次建壳（= 会给它重设 life）的次数** —— 结构上不可能，正常恒 0。
 *   判据：六面开头小怪 / 四面道中 boss 跑一段 ⇒ **`move` 很大而 `create/withdraw/hp_reset ≈ 0`**。*/
static unsigned s_emit_move;
static unsigned s_emit_hp_reset;

typedef struct { uint32_t fam_id; float cx, cy, sx, sy, tx, ty; uint16_t age; } yk_pin_follow_t;
static yk_pin_follow_t s_pin_follow[YK_FAM_MAX];

/* 每帧把一只壳朝它的槽原点插值推进（D2）；返回要写进敌人的位置。*/
static void yk_pin_follow_step(yk_pin_follow_t *pf, uint32_t fam_id, float tx, float ty)
{
    if (!pf) return;
    if (pf->fam_id != fam_id) {                        /* 新壳（或这一格换了壳）⇒ 就地起步，不跳 */
        pf->fam_id = fam_id;
        pf->cx = tx; pf->cy = ty;
        pf->sx = tx; pf->sy = ty;
        pf->tx = tx; pf->ty = ty;
        pf->age = 0;
        return;
    }
    if (tx != pf->tx || ty != pf->ty) {
        /* 目标变了（= 槽偏移向量变了）⇒ 捕获起点、重新计时。★ **不变量：这里只动位置**——
         *   绝不 CREATE/WITHDRAW、绝不写 life/flags（重建判据见 `yk_shell_needs_rebuild()`，
         *   它的入参里**没有**位置；`WAVE_GAP`/`broken`/`rearm` 只由 `etOn` 事件与死亡事件驱动，
         *   与偏移无关）⇒ 「瞬移不重建血量」。*/
        ++s_emit_move;
        pf->sx = pf->cx; pf->sy = pf->cy;
        pf->tx = tx; pf->ty = ty;
        pf->age = 0;
    }
    /* ⚠ `&pf->cx` 当 `float[2]` 用：`cx, cy` 在结构体里**相邻**（上面定义保证）。*/
    yk_follow_step(&pf->cx, pf->sx, pf->sy, pf->tx, pf->ty,
                   (int)pf->age, (int)YK_EMIT_FOLLOW_FRAMES);
    if (pf->age < 0xffffu) ++pf->age;
}

/* ★★★★ 2026-10-01 晚（**109 线一手定名 ⇒ 无体术正解**）：`zEnemyData + 0x5108` =
 *   **`no_hitbox_timer.current`**（`engine/ecl/th18/01-boss-interrupts-and-spellcard.md:45` 定名）
 *   ⇒ 绝对 = `zEnemy + 0x122c + 0x5108` = **`e + 0x6334`**。
 *   接触致伤判定在 `EnemyData__step_game_logic 0x42ed40`，**前置闸 `0x42f51c`：`+0x5108 > 0`
 *   ⇒ 引擎自己跳过接触致伤判定**（纯读）⇒ 每帧写 `= 2` 即可 ⇒ 壳**仍会被自机弹打中 / 可被锁定 /
 *   计数照常**，只是撞上去不掉血。**不碰 `flags_lo`**。
 *   ⛔ 作废：先前按 109 ECL 注释采用的 `setHitbox(0,0)`（`enemy+0x1344`）—— 体术走三个**通用几何判定**
 *   （圆 `0x45cd90` / OBB `0x45cea0` / AABB `0x45cc20`，判据是**双方几何体**）⇒ 把敌机盒设 0
 *   只能让「敌机那半」为 0，**永远得不到「没有体术判定」**（除非能让 `hitbox_2 < 0`）⇒ **已撤**。*/
#define YK_ENEMY_NO_HITBOX_TIMER  (0x122c + 0x5108)   /* = e + 0x6334 */

/* ★★★★ 2026-10-01 晚（owner：「**那个环绕的额外使魔也得无碰撞**」）——**统一入口**
 *   `yk_no_body_attack(f)`：对我们建的**任何**敌人壳写引擎自带闸
 *   `e+0x6334`（`zEnemyData+0x5108` = `no_hitbox_timer.current`）= 2
 *   ⇒ 引擎前置闸 `0x42f51c`（`>0`）跳过接触致伤；**不碰 `flags_lo`** ⇒ 仍可被打中/锁定。
 *   **出生时 + 每帧**都调（映射壳见 `yk_spawn_familiar`/`yk_pin_familiar`；
 *   绕转层见 `yk_orbit_spawn_one`/`yk_orbit_pin`）⇒ 以后新增 spawn 路径只要调它就不会漏。*/
static void yk_no_body_attack(uint8_t *f)
{
    if (f) *(int32_t *)(f + YK_ENEMY_NO_HITBOX_TIMER) = 2;
}

/* ★★★★★ 2026-10-01 晚（owner：空格杀到 **93..104** 那批「过期 registry id」⇒ 引擎 **id 回收** ⇒ 杀到 boss）：
 *   **活体校验**：`(母机, 壳)` 必须**父子匹配** —— 壳的 `zEnemy+0x6834`（`YK_ENEMY_PARENT_ID`）
 *   必须等于**该注册槽的母机 id**；回收来的 id 父链不同 ⇒ **不算我们的壳**。
 *   ⚠ 这是「这是我们的壳吗」的**统一判据**：空格键已走它；**pin / 站点闸 / 转嫁 / 绕转 / orphan 清理
 *     下一批统一改走它**（登记 `TUNING §2.1ccc-68` 待办）。*/
static int yk_shell_parent_ok(uint8_t *f, uint32_t expected_parent)
{
    uint32_t pid;
    if (!f || !expected_parent) return 0;
    pid = *(const uint32_t *)(f + YK_ENEMY_PARENT_ID);
    return (pid == expected_parent) ? 1 : 0;
}

static void yk_pin_familiar(uint8_t *f, float x, float y)
{
    /* ★ TRACK 用：记「这一帧钉过哪只壳」（owner 要看 pinned_this_frame）。*/
    s_pin_last_fam = *(const uint32_t *)(f + CE_ENEMY_ID);
    s_pin_last_frame = ce_sdk_frame();
    s_pin_last_x = x; s_pin_last_y = y;   /* ★ TRACK2 用：实际喂进 pin 的值 */
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
        int want = yk_focused() ? 1 : 0;
        /* ★★★★ 2026-10-01 晚（**owner 给了参照实现 ⇒ 照抄 109 的幽灵**）：
         *   owner：「无体术这块你查一下 **109 的幽灵**和其他灵，**它们是无体术的**」。
         *   109 的做法（一手，`assets/ecl/spirit109.ecl.txt:76–81`，注释就是同一件事）：
         *     `setHurtbox(20.0f, 20.0f);`   ← 受击盒保留 = 自机弹仍能打中它 ✓
         *     `setHitbox(0.0f, 0.0f);`      ← ★ **体术/接触盒归零 = 无体术** ✓
         *   字段（109 注释里的原话）：**`zEnemyData + 0x118` = `hitbox_size`**（两个 float）
         *   ⇒ 绝对 = `enemy + 0x122c + 0x118` = **`enemy + 0x1344`**（`hurtbox_size` 在 `+0x110`、我们**不动**）。
         *   我们**只对自己的壳**写（本函数只作用于 `yk_spawn_familiar()` 建的敌人），**每帧重钉都带**，
         *   所以「出生 → 死亡 / 重建」全程无体术。**不再写 `enemy+0x635c` 的 bit1**
         *   （那条是静态推的、owner 实跑证明**没有生效** ⇒ 换成 109 这条已被实跑验证过的做法）。*/
        {
            int32_t before = *(int32_t *)(f + YK_ENEMY_NO_HITBOX_TIMER);
            yk_no_body_attack(f);                     /* ★ 统一入口（映射层）*/
            if (s_bodywrite_logged < 8u) {
                ++s_bodywrite_logged;
                ce_log("youkai: no_hitbox fam_id=%u (e+0x6334) before=%d after=%d ; engine gate 0x42f51c "
                       "(>0 => skip contact damage); flags_lo untouched (still shootable/targetable)",
                       *(const uint32_t *)(f + CE_ENEMY_ID), (int)before,
                       (int)*(int32_t *)(f + YK_ENEMY_NO_HITBOX_TIMER));
            }
        }
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
/* ★★★ 2026-09-30（**owner 口径**）：四色的语义表在 `youkai_core.h`（`YK_FAM_KIND_*`），
 * 这里只负责「kind → 一支脚本」——**用 `anm_ids.h` 的宏名**，不写死数字。
 * ⚠❓ **配色本身 = 自定**（2×2 语义 ↔ 53..56 的对应关系没有一手依据；见 core 那一节的长注）。
 *   当前取法：**额外使魔**（不会体术）留在实跑已验证的 **S53（红）**，**发弹口使魔**改到 **S54**，
 *   两个留位 kind 占 S55 / S56 ⇒ 四者互不相同（下面有编译期断言）。**整表可置换**。*/
static const int32_t YK_FAM_KIND_SCRIPT[YK_FAM_KIND_COUNT] = {
    CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S54,   /* MELEE_SHOT_SAME     发弹口使魔（会体术 + 弹一样）*/
    CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S55,   /* MELEE_SHOT_DIFF     留位（会体术 + 弹不一样）*/
    CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53,   /* NO_MELEE_SHOT_SAME  额外使魔（不会体术 + 弹一样）*/
    CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S56    /* NO_MELEE_SHOT_DIFF  留位（不会体术 + 弹不一样）*/
};
/* 编译期：四支必须互不相同（「4 种颜色」这句话的机器保证 —— 抄错一格就编不过）。*/
_Static_assert(CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53 != CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S54 &&
               CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53 != CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S55 &&
               CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53 != CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S56 &&
               CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S54 != CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S55 &&
               CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S54 != CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S56 &&
               CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S55 != CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S56,
               "the four youkai familiar colours must be four distinct scripts (YK_FAM_KIND_SCRIPT)");

static int yk_fam_kind_script(int kind)
{
    if (!yk_fam_kind_valid(kind)) kind = YK_FAM_KIND_MELEE_SHOT_SAME;   /* 越界 = 最保守那一档 */
    return (int)YK_FAM_KIND_SCRIPT[kind];
}

/* 映射使魔（`s_map` 里那批 1:1 发射口使魔）用哪个 kind：
 *   新口径（**owner 2026-09-30**）= **恒 `MELEE_SHOT_SAME`**（会体术 + 弹一样）⇒ **它们全部同色**。
 *   ⚠ 这是一处**可见变化**（原来按发射口槽 `yk_fam_variant(slot)` 取色，`AUDIT.md` §BE #150 第八轮登记）。
 *   **要改回按槽取色** = 把下面 `return` 那一行换成 `return yk_fam_variant(slot);`（那个函数还在 core，
 *   槽→变体/kind 同序 ⇒ 一行）。`slot` 在旧口径下是判据，所以这里保留实参。*/
static int yk_fam_mapped_kind(int slot)
{
    (void)slot;
    return YK_FAM_KIND_MELEE_SHOT_SAME;
}

/* 使魔 ↔ 跟随 VM 的对照表（**文件级静态**，照 117 动物灵 `s_tok[40]` / 112 的做法；
 * 不进卡的 496 B 私有状态预算）。32 × 12 B = **384 B**。
 * 键 = `fam_id`（单调递增、不复用 ⇒ 不会撞键）；`fam_id == 0` = 空槽。
 * ★ 为什么不塞进 `yk_slot_t`：那是**纯逻辑**层的结构（进主机单测、有 sizeof 断言，头文件只 <stdint.h>），
 *   anm 句柄是引擎侧的东西 ⇒ 分开放（也顺手让 `sizeof(yk_card_t)` 一个字节都不变）。*/
typedef struct {
    uint32_t fam_id;   /* 0 = 空槽 */
    uint32_t anm_id;   /* 0 = 没有 VM（起失败 / 已被引擎销毁）*/
    int32_t  kind;     /* `YK_FAM_KIND_*`：自愈重起时要按**同一个 kind**（同一支脚本）再起一个 */
    /* ★★★ 2026-09-22 夜（owner：**妖侧只剩魔法阵**）：**常驻魔法阵环**的句柄（只在妖侧存在）。
     * 与 `yk_ring_blink` 那支**切换短命环**是两件事：这支是「妖侧使魔本体不可见时**一直**看得见的那圈」。*/
    uint32_t circle_id;/* ★ 常驻**加法空心环**（一手 `th08:0x425b70` 内联分配 / 句柄 `enemy+0x53c8`）
                        *   0 = 这一帧没有环（起失败 / 还没起）*/
    int8_t   circle_side;  /* 上一次发的中断对应的侧：0 = 人（interrupt 1 紫）/ 1 = 妖（2 蓝）/ -1 = 还没发
                            *   —— 只在**换侧**时发一次（每帧发会把这支 8 帧的生长动画一直重播）*/
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
    /* ★ 环：**发 interrupt 3 淡出**（一手：脚本 `interruptLabel(3)` = `scaleTime(10, 0, 0, 0)` ⇒
     *   10 帧后 `delete()` **脚本自删**）⇒ 我们**不跟句柄**（置 0 即可，也不留游荡 VM）。*/
    if (v->circle_id) { ce_anm_interrupt(v->circle_id, 3); v->circle_id = 0; }
    v->circle_side = -1;
    v->fam_id = 0;
    v->kind = 0;
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
 * ⇒ 不先摆会闪一帧）。失败只报日志（画不出来 ≠ 卡坏，`ufo_body_vm_spawn` 同款）。*/static void yk_fam_vm_spawn(uint32_t fam_id, int kind, float x, float y)
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
    script = yk_fam_kind_script(kind);
    id = ce_anm_spawn(CE_ABILITY_ANM(), script, YK_FAM_ANM_LAYER);
    if (!id) {
        ce_log("youkai: familiar follow-VM spawn FAILED (fam %u, script %d, layer %d) - the "
               "familiar entity is alive but invisible (`NoEnemyDebug` runs no ECL, so nothing "
               "else can paint it)", fam_id, script, YK_FAM_ANM_LAYER);
        return;
    }
    v->fam_id = fam_id;
    v->anm_id = id;
    v->kind = kind;
    ce_anm_set_pos(id, x, y, 0.0f);
    /* ★★★ 2026-09-22 夜（owner 二次订正）：**使魔身上不再挂常驻环** —— 环改成「切换那一下的
     * 短命特效」（`yk_ring_blink`，见上面那一段的长注）。使魔本体这一层只剩一个跟随 VM。*/
}

/* 每帧跟随：位置 = 使魔**实体**的 `final_pos`（`CE_ENEMY_POS_X/Y` = spec §3.5 钉的那两格；
 * 世界层 VM 的实体坐标与 ECL 是同一套 640 空间 ⇒ 不需要换算，`ufo_body_vm_follow` 的注）。
 * ⚠ 句柄被引擎销毁过（过场重建 ANM 管理器 / VM 列表被清）⇒ **自愈**：丢掉句柄按同一变体重起。
 * ⚠ 脚本自己 `delete()` 了 ⇒ `ce_anm_set_pos` 返回 0 ⇒ 清句柄，下一帧自愈。*/
static void yk_fam_vm_follow(uint32_t fam_id, int kind, const uint8_t *f)
{
    yk_fam_vm_t *v;
    if (!f || !fam_id) return;
    v = yk_fam_vm_find(fam_id);
    if (!v || !v->anm_id || !ce_anm_get_vm(v->anm_id)) {
        /* 记录还在 ⇒ `spawn` 按 `fam_id` 找回**同一条**（旧句柄早就没了，直接覆盖不泄漏）；
         * 记录也没了 ⇒ 它占一条空槽。两种都按**同一个变体**重起。*/
        yk_fam_vm_spawn(fam_id, kind, *(const float *)(f + CE_ENEMY_POS_X),
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
        int side = yk_focused() ? 1 : 0;
        if (v->anm_id) ce_anm_set_color(v->anm_id, side ? 0x00ffffffu : 0xffffffffu);
        /* ★★★ 2026-09-30（**owner 口径**：「得画那个加法空心环……**你只碰使魔加那个环**」）：
         *   **两侧都常驻**（一手：环由 `familiarCreate*` 内联分配 `th08:0x425b70`、句柄 `enemy+0x53c8`、
         *   每帧跟随 `th08:0x42cac9..0x42caf6` 把 `enemy+0x2d88` 拷进去 ⇒ **换侧只换中断、环不消失**）。
         *   人侧 `interrupt 1` = (192,128,255) 紫、妖侧 `interrupt 2` = (96,96,255) 蓝；尺寸也分侧
         *   （脚本 `scaleTime(8,0,0.5f/0.8f)`）—— 这正是 owner 说的「**大小形成质感**」。
         *   ⚠ 只在**换侧**时发中断：每帧发会把这支 8 帧的生长动画一直重播。*/
        if (!v->circle_id || !ce_anm_get_vm(v->circle_id)) {
            v->circle_id = yk_vm_spawn(CE_ABILITY_ANM(),
                                        CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING,
                                        YK_FAM_ANM_LAYER);
            v->circle_side = -1;
        }
        if (v->circle_id) {
            if (v->circle_side != (int8_t)side) {
                ce_anm_interrupt(v->circle_id, side ? 2 : 1);
                v->circle_side = (int8_t)side;
            }
            if (!ce_anm_set_pos(v->circle_id, *(const float *)(f + CE_ENEMY_POS_X),
                                               *(const float *)(f + CE_ENEMY_POS_Y), 0.0f)) {
                v->circle_id = 0;                          /* 句柄作废 ⇒ 下一帧自愈重起 */
                v->circle_side = -1;
            }
        }
    }
}



/* ============================================================================================
 * ★★★ 2026-09-28（第四十九批 · **owner 口径**）：**惰性绕转使魔** —— 给「没有任何发射口的大怪」补使魔
 * ============================================================================================
 * owner 逐字：「然后得加使魔了。你参考 117 掉动物灵的那个『大怪』判定，给这些大怪加上使魔，
 *   如何呢？**这些使魔是不发弹，没有体术判定的**。」
 *（动机来自更早那句：「为了增大爆永点的数量，应该给**没有使魔的怪物加使魔**了 ——
 *   **没有体术、不发弹、只绕转**的使魔」）
 *
 * 纯逻辑（「大怪」判据 / 相位推进 / 查表）在 `youkai_core.{h,c}`（有主机单测）；
 * 这一节只管**场景侧**：扫场 → 建壳 → 钉位 → 收尾。
 * 机制链（为什么这么做能增大爆永点数量）、参数理由、三个连带项：`HANDOFF-108.md` §4 + `AUDIT.md` §BE #150。
 *
 * 壳 = 照抄 `yk_spawn_familiar_parent()` 的 `NoEnemyDebug` 惰性空壳
 *（`wait(1000)` 死循环 ⇒ 不跑 ECL、不动、**不开火**、不掉落、不给分、不设免伤位），
 * **只多写一格**：`CE_SPAWN_PARENT = 父敌 id`。那一格就是「让它们被算进爆永点颗数」的关键 ——
 * `yk_kill_burst` 那边数「`CE_SPAWN_PARENT == 被杀者 id` 的**活子敌人**」时会把它们自动算进去
 *（而且一手是**每个使魔各撒一盘**：`th08:0x42aff0`）⇒ **颗数那一侧不需要任何改动**
 *（也正因此，那边**不许**再加「排除绕转使魔」的过滤）。
 *
 * ⚠ **「有没有发射口」不是筛选条件**（2026-09-28 第二轮 · owner：「我觉得可以不考虑发射口的问题，
 *   这个是**别的种类**的使魔」）—— 已经有 1:1 发射口映射的大怪**照样**补绕转使魔。
 *   这与「不进 `s_map`」**不矛盾**：那张表管的是「**发射口** ↔ 使魔」的 1:1 映射与精准闸，
 *   绕转使魔是**另一种使魔**（只绕转、不发弹），既不该被当成谁的发射口、也不该占那个池。
 * ⚠ 台数 = **父敌最大血量的函数**（owner：「数量我倾向于和血量挂钩」）⇒ `yk_orbit_count_for_hp()`；
 *   阈值 / 台数那两张表在 `youkai_core.{h,c}`，标着 **❓自定（待 th08 一手统计回来即替换）**。
 *
 * **四条硬约束**（每条都落成结构，而不是靠自觉）：
 *   ① **不进 `s_map`** ⇒ 不参与发射口映射 / 精准闸（`broken` 位）/ 使魔环爆闪 / 切换音计数；
 *   ② 由 ① 顺带 ⇒ **不参与 `yk_fam_collide_tick`**（那是本卡唯一的人侧「撞使魔 = MISS」判据）
 *      ⇒ owner 要的「**没有体术**」是**结构上**成立的，不是靠一个标志位；
 *   ③ **打得掉，血量 = 一手值 × 缩放旋钮**（第四轮 **owner 口径**：「那就 **hp 切成一手**」）：
 *      建壳时 `CE_SPAWN_LIFE = yk_clamp_spawn_life(yk_orbit_familiar_hp(台数))`
 *      = 一手值（TH08 `familiarCreate` 第 4 实参，按台数档 100/100/300/600）× `YK_ORBIT_HP_SCALE`
 *      ⇒ **当前 SCALE = 1，就是一手值 `{100, 100, 300, 600}`**（顶上还有引擎硬上限 999：
 *      `life >= 1000` 会置锁定黑名单 `flags_lo 0x40000000`，见 `YK_LIFE_LIMIT`）；
 *      并且每帧 `yk_orbit_pin()` **清掉** `CE_ENEMY_FLAG_NO_DAMAGE`（不置上、也不按聚焦分）。
 *      ⚠ 旋钮留着是因为一手道中恒 100 血在 TH18 的 DPS 下可能偏脆（owner：「一碰就碎主要是形容杂鱼」）；
 *      TH18 vs TH08 的 DPS 核查另行进行。⚠ 父敌 HP **只**用来定台数；
 *   ④ **VM 表独立**于 `s_fam_vm`（句柄放在 `s_orbit[]` 自己身上）：`s_fam_vm` 的自愈回收
 *      （`yk_fam_vm_prune`）会把「不在 `s_map` 里的 fam_id」当成**泄漏记录、连实体一起撤掉** ——
 *      而绕转使魔**恰恰故意**不在 `s_map` 里 ⇒ 混用会在下一帧就被自愈机制清光；
 *   ⑤ **被玩家击破时爆不爆永点**（把现状读清之后的定案，owner 问过）：
 *      · **盘（burst）不爆**：结构上就爆不了 —— `yk_kill_burst` 数的是「`CE_SPAWN_PARENT == 被杀者
 *        id` 的**活子敌人**」+「声明过的发射口槽」，而绕转使魔自己**没有**子敌人、也不进 `s_map`
 *        ⇒ `n == 0` ⇒ 一手那道门（`th08:0x42adc4 je`）拦下 = **一颗盘都不给** ✓ 与 owner 的倾向一致
 *        （一手颗数公式吃的是「这个敌人的使魔数」，一台被打掉的使魔在 TH08 里少的就是**它那一盘**）；
 *      · **单颗会给**：`yk_kill_burst` 尾巴那一支（一手 `th08:0x42b1e5` 起：被杀者**自己有主**
 *        ⇒ `spawn_item(type 7, …)` 一颗）—— **有意保留**：一手那一支的判据正是「有主」，
 *        而绕转使魔按定义就有主（`CE_SPAWN_PARENT`）；
 *      · 如实记录：击破它也会照常吃「击破 ⇒ 人妖槽 ±200」（一手**击破**的通用尾，与死的是不是使魔无关）；
 *   ⑥ **「样子」与属性的另一组一手对照**（都支持 owner 定的两条口径）：
 *      · 使魔**有** `hitboxSet(24.0f, 24.0f)`（opcode 77，handler `th08:0x41a840`；实测 24 ECL 里每只
 *        使魔都有）、多数还有 `setInvuln(30~80)` 的出生保护；而**没有任何使魔设 `hurtboxSet`**
 *        ⇒ 「**打得掉、但没有体术**」与一手一致（我们的壳同样不设 hurtbox，`yk_fam_collide_tick`
 *        那套 MISS 判据也只扫 `s_map`）；
 *      · `drop_main`（`familiarCreate` 第 5 实参，落 `enemy+0x3304`，消费点 `th08:0x42bea0`）
 *        **80% 是 `-2`（不掉道具）**；但**被自机击破固定掉 1 刻符**（`th08:0x42b29b`）——
 *        这正是 `yk_kill_burst` 尾巴那颗「有主单颗」的一手对照（见那一处的注）。
 *
 * 生命周期（**铁律：长持续效果必须交代「被删掉会怎样」**）：壳与跟随 VM 的**每一条死路**都过
 *   `yk_orbit_retire()`；并且 `yk_drop_all()`（删卡 / 换关 / 局末 / 自机没了 / 世界没了）里有
 *   `yk_orbit_kill_all()` 兜底 —— 删卡那一刻卡的 tick 全停，那是**唯一**的机会。
 * ⚠ 这些使魔**在人侧也活着、也绕转**（它们不是「妖侧才有的东西」）⇒ 与映射使魔同一口径：
 *   松 Shift（`yk_leave_youkai`）**不撤**它们，只有 `yk_drop_all` 撤。*/
typedef struct {
    uint32_t fam_id;      /* 0 = 空槽。键 = 使魔的 `enemy_id`（单调递增、不复用 ⇒ 不会撞键）*/
    uint32_t parent_id;   /* 父敌 `enemy_id`（`CE_SPAWN_PARENT` 那一格，颗数就靠它）*/
    float    cx, cy;      /* **出生点**（= 建壳那一刻的父敌坐标；只作记账/日志用）。
                           * ★★★★ 2026-09-30 **订正（owner 实跑：中间大蝴蝶往下走、使魔留在高处错位）**：
                           *   绕转**跟着父敌走** —— 每帧圆心 = **父敌当前位置**（见 `yk_orbit_tick`）。
                           *   一手把两条合起来看才完整：
                           *     · `moveCircle`（opcode 73，handler `th08:0x41a4b7`）只把**圆心设成那一刻
                           *       使魔所在的位置**（`0x41a50b`–`0x41a52a`：`+0x2dd0 := +0x2d34` 三个 dword
                           *       拷贝）—— 这条没错，但它**只管「圆心钉在当场」**；
                           *     · **跟不跟父敌取决于用哪个 create 变体**：`familiarCreateF`（opcode **92**，
                           *       一面妖精用的就是它）spawn 时 `moveVec = enemy->pos`、
                           *       `movePos = moveVec + pos` 并 **`flags |= TRACK_POS (0x200)`**
                           *       ⇒ **整圈跟着父敌走**；`familiarCreate`（opcode **91**）**不加**
                           *       TRACK_POS ⇒ 生成点定死；`familiarCreateA`（opcode **90**）用绝对坐标。
                           *       （三者的生成点都经 `0x41f2ba`–`0x41f2d3` 的 `Float3::operator+=`
                           *       `parent->final_pos`，脚本全写 `(0,0)` ⇒ 出生贴在本体身上。）
                           *   ⇒ 我们上一版按 91 的形状做（定圆心）⇒ owner 看到一面妖精的使魔不跟。
                           *   ⚠ `yk_orbit_tick` 的孤儿宽限（父敌查不到时原地钉住 N 帧）保留。*/
    unsigned age;         /* ★★★★ 帧龄（建壳起 +1）：绕转半径由它算（见下面 `radius`）*/
    unsigned trail_t;     /* ★★★ 拖尾节拍（每台**自己**一份 —— 一手那个开关挂在 `flags_hi` bit1 上，
                           *   即每台使魔各自计时）：数到 `YK_ORBIT_TRAIL_INTERVAL` 就在本位撒一颗火花*/
    float    radius;      /* ★ 当前绕转半径 = `yk_orbit_radius_at(age)` —— 一手形状：
                           *   出生**贴在本体身上（0）**，以 `YK_ORBIT_RADIAL_SPEED`(0.6 px/帧)
                           *   外扩 `YK_ORBIT_EXPAND_FRAMES`(100 ≈ 1.7 s) 帧到 `YK_ORBIT_FINAL_RADIUS`(60)
                           *   然后**冻结**（一手 `moveCircle`：`+0x2db0 radius = 0`、`+0x2db4 = arg3`、
                           *   `+0x2de8 = arg0` 时长；之后 `moveCircleChange(6000, ω, 0)` 冻结）。*/
    float    ang_deg;    /* ★ 2026-09-30：**浮点角**（度）—— 分圈后各圈 ω 不同（6/4.5/3°），
                          *   整数格索引会把它抹掉 ⇒ 浮点累加、再量化（4.5 自然交替 1/2 格）*/
    float    omega_deg;  /* 该台角速度（度/帧，含符号 = 转向）—— `yk_orbit_layout()` 给 */
    float    radius_final;/* 该台**最终**半径（px）= 30/60/90（分圈）或 60（单圈）*/
    int8_t   ring;       /* 第几圈（0 基）；日志用 */
    float    hurt_x, hurt_y;  /* ★ 2026-10-01：妖侧被我们**临时置 0** 的碰撞盒原值（人侧还原用）*/
    uint8_t  hurt_saved;      /* 1 = 上面那对值有效（该使魔正被「隐去碰撞」）*/
    unsigned substep;     /* ★ 绕转相位（**半步累加器**，第七轮）：每帧 +1，一圈 = `YK_ORBIT_SUBSTEPS`(240)
                           *   个 substep；查表格号 = `yk_orbit_step_of_substep(substep)`
                           *   ⇒ 实际转速 = 3°/格 ÷ `YK_ORBIT_STEP_DIV`(2) = **1.5°/帧**（240 帧 ≈ 4 s 一圈）。*/
    unsigned orphan;      /* 父敌**连续**查不到的帧数（满 `YK_ORBIT_ORPHAN_GRACE` 才撤）*/
    uint32_t anm_id;      /* 跟随 VM 句柄（画本体）*/
    uint32_t circle_id;   /* ★ 常驻**加法空心环**句柄（两侧都有；与映射使魔同一套口径）*/
    int8_t   circle_side; /* 上一次发的中断对应的侧（0 人 / 1 妖 / -1 还没发）——只在换侧时发 */
    int32_t  kind;        /* `YK_FAM_KIND_*`（VM 被引擎清过时按同一支脚本自愈重起）。
                           *   ★★★ 额外使魔恒 = `YK_FAM_KIND_NO_MELEE_SHOT_SAME`
                           *   （不会体术 + 弹一样，owner 2026-09-30 口径；见 `youkai_core.h` 那张表）。*/
} yk_orbit_t;

static yk_orbit_t s_orbit[YK_ORBIT_MAX];

/* ★★★ 2026-09-30（一手形状 · **生成延时**）：父敌**首次被扫到**之后再等 `YK_ORBIT_DELAY_FRAMES` 帧
 * 才建使魔（一手 `ecldata1.txt`：阶段一妖精**进场后飞约 1 秒**才在 **t = 50 / 60 / 70 帧**建；
 * 且 2/4 台**同一帧连发、零帧间隔、无循环**）。这里只记住「首次命中」的帧号，够用就好：
 *   · 一父敌一项（`YK_ORBIT_SEEN_MAX` 条；满了只是那一只补不上，日志里有 `scans`/`pool_full` 可判）；
 *   · 到点后**立刻把这一项删掉**（同一只敌人不会被补第二次 —— 一敌一对的判据是 `s_orbit`，不是这张表）；
 *   · 父敌没了/登记过期 ⇒ 扫场时顺手清（见 `yk_orbit_seen_prune`）。*/
typedef struct {
    uint32_t id;      /* 0 = 空槽 */
    uint32_t frame;   /* 首次命中那一刻的帧号 */
} yk_orbit_seen_t;
static yk_orbit_seen_t s_orbit_seen[YK_ORBIT_SEEN_MAX];

/* ★★★ 2026-09-30（**owner 实跑 bug**：「我看一面 3 只大蝴蝶生成了两次两个使魔」）：
 *   **「已给过使魔」的账**（与生成延时那张 `s_orbit_seen[]` **分开** —— 延时那支是**一次性**的
 *   （到点就 `id = 0` 把自己抹掉），放不下这条长期状态；**生成延时 60 帧一个字没动**）。
 * 语义（一手）：TH08 的使魔是脚本一次性连发，撒完就没了、死了也不会再生成 ⇒ 我们的扫场（每 30 帧）
 *   必须自己记住「这只父敌已经给过」，判据绝不能用「它现在名下有没有活着的使魔」。
 * 清记录**只**因父敌消失（连续 `YK_ORBIT_GIFTED_GRACE` 次扫场找不到；敌机 id 不复用）；
 *   **不**因使魔全死。表满 ⇒ **宁可不补**（`yk_orbit_gift_verdict` 的 fail-closed 分支）。
 * ⚠ boss 那条「每段攻击重建」的路与本表**无关**：`yk_orbit_scan` 本来就把 boss 排除在绕转使魔之外
 *   （`yk_enemy_is_boss(e)` ⇒ `continue`）⇒ 绕转使魔从不长在 boss 身上，不存在「boss 换段要不要重新武装」。*/
typedef struct {
    uint32_t id;              /* 0 = 空条目 */
    uint32_t frame;           /* 给出去的那一刻（日志用）*/
    unsigned gifted;          /* ★★★ 2026-10-01：**那一刻真建出来几台**（`yk_orbit_gift_mark` 的 `made`）
                               *   —— 「记忆值」：人侧玩家**先把使魔打光、再打死母机**时活子敌数 = 0，
                               *   死亡 +N 与爆点公式改用 `max(活子敌数, 这一格)`（见
                               *   `yk_kill_display_count`）。清记录的时机与 `absent` 老化一致。*/
    unsigned absent;          /* 连续几次扫场找不到这个父敌（`yk_orbit_gift_prune` 用）*/
    uint8_t  refused_logged;  /* 「拦下重复补」只记一次 */
} yk_orbit_gifted_t;
static yk_orbit_gifted_t s_orbit_gifted[YK_ORBIT_GIFTED_MAX];
static unsigned s_orbit_gift_full;    /* 表满拒绝过几次（只增，诊断用）*/
static unsigned s_orbit_gift_refused; /* 拦下过几次重复补（只增，诊断用）*/

/* ★★★ P82（owner 2026-10-15）：**映射（发射口）使魔的「当初发过几台」台账** ——
 *   修 owner 报的「**时序问题**：击破含使魔的敌人时被判定成没有使魔 ⇒ 没有 +N」✗。
 *   绕转使魔本来就有同款台账（s_orbit_gifted ✓），映射使魔没有 ✗ ⇒ 使魔先被清掉、母机同帧死亡时
 *   n = 0 ⇒ n_mem = 0 ⇒ `if (!n2)` 整块跳过 ⇒ 连 +N 都没有 ✗。
 *   这里按**母机 id** 记「哪些发射口键建过壳」（位图 ✓）⇒ 死亡那一帧仍取得出台数 ✓。
 *   文件级静态、不进卡的 496 B 预算 ✓。*/
#define YK_MAP_GIFTED_MAX 64
typedef struct {
    uint32_t id;
    uint64_t seen[2];      /* 发射口键（slot + 16*ord，≤ 16*6）位图 ✓ */
    uint32_t frame;
} yk_map_gifted_t;
static yk_map_gifted_t s_map_gifted[YK_MAP_GIFTED_MAX];

static int yk_map_gifted_find(uint32_t id)
{
    unsigned i;
    for (i = 0; i < YK_MAP_GIFTED_MAX; ++i) if (s_map_gifted[i].id == id) return (int)i;
    return -1;
}

static void yk_map_gift_mark(uint32_t id, int key, uint32_t frame)
{
    unsigned i;
    int idx;
    if (!id || key < 0 || key >= 128) return;
    idx = yk_map_gifted_find(id);
    if (idx < 0) {
        for (i = 0; i < YK_MAP_GIFTED_MAX; ++i) {
            if (!s_map_gifted[i].id) {
                s_map_gifted[i].id = id;
                s_map_gifted[i].seen[0] = 0;
                s_map_gifted[i].seen[1] = 0;
                idx = (int)i;
                break;
            }
        }
    }
    if (idx < 0) return;                      /* 表满 ⇒ 宁可不记（只影响 +N 的值 ✓）*/
    if (key < 64) s_map_gifted[idx].seen[0] |= (uint64_t)1 << key;
    else          s_map_gifted[idx].seen[1] |= (uint64_t)1 << (key - 64);
    s_map_gifted[idx].frame = frame;
}

static unsigned yk_map_gifted_count(uint32_t id)
{
    int idx;
    if (!id) return 0u;
    idx = yk_map_gifted_find(id);
    if (idx < 0) return 0u;
    return (unsigned)__builtin_popcountll(s_map_gifted[idx].seen[0])
         + (unsigned)__builtin_popcountll(s_map_gifted[idx].seen[1]);
}

static int yk_orbit_gifted_find(uint32_t id)
{
    unsigned i;
    for (i = 0; i < YK_ORBIT_GIFTED_MAX; ++i) if (s_orbit_gifted[i].id == id) return (int)i;
    return -1;
}

/* ★★★ 2026-10-01：查「这只母机当初发了几台」—— 0 = 没发过 / 记录已被「父敌消失」老化清掉。
 * 只在死亡路径读；表是文件级静态、不进卡的 496 B 预算。*/
static unsigned yk_orbit_gifted_count(uint32_t id)
{
    int idx;
    if (!id) return 0u;
    idx = yk_orbit_gifted_find(id);
    return (idx >= 0) ? s_orbit_gifted[idx].gifted : 0u;
}

/* 扫场判据（有副作用：写「拦下」日志 / 表满计数）。→ 1 = 可以补，0 = 不补。*/
static int yk_orbit_gift_maybe(uint32_t id, uint32_t frame)
{
    unsigned i;
    int idx = yk_orbit_gifted_find(id);
    int table_full = 1;
    int v;
    for (i = 0; i < YK_ORBIT_GIFTED_MAX; ++i) if (!s_orbit_gifted[i].id) { table_full = 0; break; }
    v = yk_orbit_gift_verdict(idx >= 0, table_full);
    if (v == YK_ORBIT_GIFT_DONE) {
        yk_orbit_gifted_t *g = &s_orbit_gifted[idx];
        ++s_orbit_gift_refused;
        if (!g->refused_logged) {
            g->refused_logged = 1;
            ce_log("youkai/orbit: refused re-gift to enemy #%u (already gave at frame %u) - TH08's "
                   "familiars are spawned ONCE by the script and never re-created when killed "
                   "(owner 2026-09-30: \"3 butterflies gave two batches\")", id, g->frame);
        }
        return 0;
    }
    if (v == YK_ORBIT_GIFT_FULL) {
        if (++s_orbit_gift_full == 1u)
            ce_log("youkai/orbit: gift ledger FULL (%u) - refusing to gift NEW parents rather than "
                   "risk re-gifting an old one (fail closed)", (unsigned)YK_ORBIT_GIFTED_MAX);
        return 0;
    }
    return 1;
}

/* 真给出去了才记账（`made > 0` 才调）。
 * ★★★ 2026-10-01：`made` = **那一刻真建出来几台**（不是 `want`）—— 台数被池/环上限截过
 *   （实跑日志 `=> 11 familiar(s) ... made 3`），所以记 `made` 才是「发出去几台」。
 *   ⚠ 一条敌人只记一次（`yk_orbit_gift_maybe` 已经拦重复补）⇒ 这里若已有条目就**只刷新台数**。*/
static void yk_orbit_gift_mark(uint32_t id, uint32_t frame, unsigned made)
{
    unsigned i;
    int idx;
    if (!id) return;
    idx = yk_orbit_gifted_find(id);
    if (idx >= 0) { s_orbit_gifted[idx].gifted = made; return; }
    for (i = 0; i < YK_ORBIT_GIFTED_MAX; ++i) {
        if (!s_orbit_gifted[i].id) {
            s_orbit_gifted[i].id = id;
            s_orbit_gifted[i].frame = frame;
            s_orbit_gifted[i].gifted = made;
            s_orbit_gifted[i].absent = 0;
            s_orbit_gifted[i].refused_logged = 0;
            return;
        }
    }
}

static void yk_orbit_gift_clear(void)
{
    unsigned i;
    for (i = 0; i < YK_ORBIT_GIFTED_MAX; ++i) {
        s_orbit_gifted[i].id = 0;
        s_orbit_gifted[i].gifted = 0;      /* ★ 记忆值随记录一起清（时机 = 父敌消失 / 卡重来）*/
    }
}

static unsigned yk_orbit_live(void);   /* 前向声明：阀门要用它数活着的使魔（定义在下面）*/
static void yk_zone_clear_all(void);   /* 前向声明：`yk_drop_all` / `on_ctor` 的收口要用它 */

/* ★★★ 2026-09-30：**拖尾密度自适应阀门**（❓ **我们的安全阀、不是一手**，见 `YK_ORBIT_TRAIL_VM_CAP`）。
 * 我们不跟火花 VM 的句柄（脚本自删）⇒ 数不到真实存活数，只能用**可算出的峰值估计**：
 *   `est = 台数 × ⌈YK_ORBIT_TRAIL_LIFE / 有效间隔⌉`
 * 超 `CAP` 就把**有效间隔**翻倍（2→4→8→…，上限 64），恢复时逐级回落（要求 `est × 2 <= CAP` 的迟滞）。
 * 有效间隔变化时**只记一行**日志（带 `est`/`cap`/间隔）——不刷屏。*/
static unsigned s_orbit_trail_eff = YK_ORBIT_TRAIL_INTERVAL;   /* 当前**有效**间隔（帧）*/
static uint8_t  s_orbit_trail_valve_logged;                    /* 阀门日志：状态变化才打 */

static void yk_orbit_trail_valve(void)
{
    unsigned live = yk_orbit_live();
    unsigned est, want = YK_ORBIT_TRAIL_INTERVAL;
    uint8_t changed = 0;
    if (!live) { s_orbit_trail_eff = YK_ORBIT_TRAIL_INTERVAL; return; }
    for (;;) {
        est = live * ((YK_ORBIT_TRAIL_LIFE + want - 1u) / want);   /* ceil(寿命/间隔) */
        if (est <= YK_ORBIT_TRAIL_VM_CAP || want >= 64u) break;
        want *= 2u;                                                /* 太密 ⇒ 有效间隔翻倍 */
    }
    if (want != s_orbit_trail_eff) {
        changed = 1;
        s_orbit_trail_eff = want;
    }
    /* 迟滞：只有 **还有一倍余量** 时才允许回落（否则会在边界来回跳）*/
    if (!changed && s_orbit_trail_eff > YK_ORBIT_TRAIL_INTERVAL) {
        unsigned half = s_orbit_trail_eff / 2u;
        unsigned est2;
        if (half < YK_ORBIT_TRAIL_INTERVAL) half = YK_ORBIT_TRAIL_INTERVAL;
        est2 = live * ((YK_ORBIT_TRAIL_LIFE + half - 1u) / half);
        if (est2 * 2u <= YK_ORBIT_TRAIL_VM_CAP) s_orbit_trail_eff = half;
    }
    if (est > YK_ORBIT_TRAIL_VM_CAP && !s_orbit_trail_valve_logged) {
        s_orbit_trail_valve_logged = 1;
        ce_log("youkai/orbit: trail density valve ENGAGED - %u live familiar(s) x ceil(%u/%u) = %u "
               "estimated spark VM(s) > cap %u => effective trail interval raised to %u frame(s) "
               "(the ONE-HAND interval is %u; this valve is OUR safety rail, see YK_ORBIT_TRAIL_VM_CAP)",
               live, (unsigned)YK_ORBIT_TRAIL_LIFE, s_orbit_trail_eff, est,
               (unsigned)YK_ORBIT_TRAIL_VM_CAP, s_orbit_trail_eff, (unsigned)YK_ORBIT_TRAIL_INTERVAL);
    }
}
static unsigned s_orbit_scan_timer;      /* 扫场节拍（帧）*/
static unsigned s_orbit_scans;           /* 统计：扫了几次（台账，全程累计不清）*/
static unsigned s_orbit_targets;         /* 统计：选中了几只大怪 */
static unsigned s_orbit_spawned;         /* 统计：一共造了几台 */
static unsigned s_orbit_killed;          /* 统计：一共撤了几台（外力/孤儿/收口）*/
static unsigned s_orbit_lost_orphan;     /* 统计：因为父敌没了而撤的台数 */
static uint8_t  s_orbit_pool_full;       /* 池满过（**只记一次**日志 —— 不刷屏）*/
static uint8_t  s_orbit_first_scan;      /* 首次扫场的自述打过没 */
static uint8_t  s_orbit_vm_failed;       /* VM 起失败过（只记一次）*/
/* ★ 2026-09-30（owner：「一堆使魔但是**只有一个环**」）—— 环的**诊断计数**（只在 target 日志里打）：
 *   一眼看出「建环请求 try 几次 / 成功 made 几次 / 发中断 int 几次」。*/
static unsigned s_orbit_ring_try, s_orbit_ring_made, s_orbit_ring_int;
static unsigned s_orbit_ring_fail;   /* ★ 2026-10-01：环 VM 建失败（`ce_anm_spawn` 返回 0）的次数 */
static uint8_t  s_orbit_spawn_failed;    /* 壳建失败过（只记一次）*/

/* 一只敌人**至多一对**（一敌一对，不重复补）⇒ 按父敌 id 查。*/
static yk_orbit_t *yk_orbit_find_parent(uint32_t parent_id)
{
    unsigned i;
    if (!parent_id) return 0;
    for (i = 0; i < YK_ORBIT_MAX; ++i) if (s_orbit[i].fam_id && s_orbit[i].parent_id == parent_id) return &s_orbit[i];
    return 0;
}

static unsigned yk_orbit_live(void)
{
    unsigned i, n = 0;
    for (i = 0; i < YK_ORBIT_MAX; ++i)
        if (s_orbit[i].fam_id && yk_familiar_by_id(s_orbit[i].fam_id)) ++n;
    return n;
}

static unsigned yk_orbit_free(void)
{
    unsigned i, n = 0;
    for (i = 0; i < YK_ORBIT_MAX; ++i) if (!s_orbit[i].fam_id) ++n;
    return n;
}

/* 收 VM（只动句柄 ⇒ 幂等；`ce_anm_delete` 内部有 ANM 管理器门槛）。**不清 `fam_id`**。*/
static void yk_orbit_vm_kill(yk_orbit_t *o)
{
    if (!o) return;
    if (o->anm_id) { ce_anm_delete(o->anm_id); o->anm_id = 0; }
    /* ★ 同映射使魔：环**发 interrupt 3 淡出**（10 帧后脚本自删，我们不跟句柄）。*/
    if (o->circle_id) { ce_anm_interrupt(o->circle_id, 3); o->circle_id = 0; }
    o->circle_side = -1;
}

/* 撤一台：收 VM + 撤壳 + 清槽。`why != 0` 才记数/打日志（`yk_drop_all` 那条收口是静默批量）。*/
static void yk_orbit_retire(yk_orbit_t *o, const char *why)
{
    uint8_t *f;
    uint32_t parent;
    if (!o || !o->fam_id) return;
    parent = o->parent_id;
    f = yk_familiar_by_id(o->fam_id);
    yk_orbit_vm_kill(o);                  /* ① 收 VM（幂等）*/
    if (f) yk_kill_familiar(f);           /* ② 撤壳（置删除位 ⇒ 同帧 EnemyManager sweep 静默析构）*/
    o->fam_id = 0;
    o->parent_id = 0;
    o->substep = 0;
    o->age = 0;
    o->trail_t = 0;
    o->orphan = 0;
    o->kind = 0;
    if (why) {
        ++s_orbit_killed;
        ce_log("youkai/orbit: retired familiar of enemy #%u (%s)", parent, why);
    }
}

/* 「父敌没了」那一档单独一个口：多记一个 `orphan` 统计（台账里要分开看）。*/
static void yk_orbit_retire_orphan(yk_orbit_t *o)
{
    if (!o || !o->fam_id) return;
    ++s_orbit_lost_orphan;
    yk_orbit_retire(o, "parent gone");
}

static void yk_orbit_retire_all(void)
{
    unsigned i;
    for (i = 0; i < YK_ORBIT_MAX; ++i)
        if (s_orbit[i].fam_id) yk_orbit_retire(&s_orbit[i], 0);
    for (i = 0; i < YK_ORBIT_SEEN_MAX; ++i) s_orbit_seen[i].id = 0;   /* 延时登记也清 */
    yk_orbit_gift_clear();                           /* ★ 「已给过」的账随卡重来一起清 */
    /* 兜底（句柄幂等）：`fam_id` 已经被清掉、VM 还挂着的残局 */
    for (i = 0; i < YK_ORBIT_MAX; ++i) yk_orbit_vm_kill(&s_orbit[i]);
    s_orbit_scan_timer = 0;
}

/* 全撤。**唯一收口**：`yk_drop_all`（删卡 / 换关 / 局末 / 自机没了 / 世界没了）与 `on_ctor`
 *（每关重建）都过这里。⚠ 刻意**不清**上面那几个累计统计（台账要看全程）。*/
static void yk_orbit_kill_all(void)
{
    yk_orbit_retire_all();
}

/* 每帧钉位（照 `yk_pin_familiar()` 的写法）。
 * ★★★ 2026-09-28 第二轮（**owner 口径**：「**可以不免伤，血量 = 本体血量**」）：
 *   **免伤位一律清掉**（不是置上、也不按聚焦分）—— 它们是**打得掉**的真敌人，血量在建壳时
 *   就设成父敌最大血量（`CE_SPAWN_LIFE`，见 `yk_orbit_spawn_one`）。*/
static void yk_orbit_pin(uint8_t *f, float x, float y)
{
    *(float *)(f + CE_ENEMY_POS_X) = x;                   /* final_pos（spec §3.5 的那两格）*/
    *(float *)(f + CE_ENEMY_POS_Y) = y;
    *(float *)(f + YK_ENEMY_ABS_POS) = x;                 /* + abs_pos（同 `yk_pin_familiar`）*/
    *(float *)(f + YK_ENEMY_ABS_POS + 4) = y;
    /* ★★★ 2026-10-01（**owner 口径**，真 bug）：「**妖状态下应当使魔是无敌的，也不会被索敌也不会被伤害，
     *   也不会有体术**，目前是妖状态下它只是**样子隐去了**」⇒ 这里按**侧**给免伤位：
     *   **妖侧（聚焦）⇒ 置 `CE_ENEMY_FLAG_NO_DAMAGE`**（自机弹打不进、也进不了索敌/伤害结算）、
     *   人侧 ⇒ 清掉（保持可打 —— 这正是「打死使魔 ⇒ 1/2 转嫁」与一手一致的前提）。
     *   ⚠ 这是 **owner 口径**（`engine/enemy/th08/04-…md` §9 那条「按 shift 使魔变魔法阵、不碰撞也打不到」），
     *     不是一手复刻（一手是引擎自己的侧门）。体术那一路由 `yk_fam_collide_tick` 的聚焦早退负责。*/
    if (yk_focused()) *(uint32_t *)(f + CE_ENEMY_FLAGS) |= (uint32_t)CE_ENEMY_FLAG_NO_DAMAGE;
    else *(uint32_t *)(f + CE_ENEMY_FLAGS) &= ~(uint32_t)CE_ENEMY_FLAG_NO_DAMAGE;
    /* ★ 2026-10-01 晚（owner：「环绕的额外使魔也得无碰撞」）：绕转层也走**同一个**无体术入口。*/
    yk_no_body_attack(f);
}

/* float → 像素（四舍五入；117 `animal.c` 的 `animal_round_px` 同款，那边是 static ⇒ 这里自己一份）。
 * ⚠ 不含浮点库调用（只有比较与 `+0.5f` 取整）—— 本仓 .c 禁 x86 浮点库。*/
static int32_t yk_round_px(float v)
{
    if (!(v > 0.0f)) return 0;
    if (v > 100000.0f) v = 100000.0f;
    return (int32_t)(v + 0.5f);
}

/* 逐敌机取**贴图原始尺寸**（未乘 scale）。与 117 `cards/animal.c:1043` 的
 * `animal_enemy_sprite_size()` **逐条同源**（那边是同一份判据的第一处出处 ⇒ 两处可交叉对账）：
 *   ① 槽 0 当前 VM 的 `sprite_size`（`vm + CE_ANM_VM_SPRITE_SIZE`，float2）；
 *      VM id 取自 `enemy + CE_ENEMY_ANM_VM0`（= `zEnemyData.anm_vm_ids[0]`）；
 *   ② 退路：引擎记的 `final_sprite_size`（`enemy + CE_ENEMY_FINAL_SPRITE_W/H`，float2）。
 * 返回 1 = 拿到（两边都 > 0）；0 = 两条来路都读不到（≈ 还没登场 / VM 还没建）。*/
static int yk_enemy_sprite_size(const uint8_t *e, int *out_w, int *out_h)
{
    uint8_t *vm;
    int32_t id, w, h;
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!e) return 0;
    id = *(const int32_t *)(e + CE_ENEMY_ANM_VM0);
    vm = id ? (uint8_t *)ce_anm_get_vm((uint32_t)id) : 0;
    if (vm) {
        w = yk_round_px(*(const float *)(vm + CE_ANM_VM_SPRITE_SIZE));
        h = yk_round_px(*(const float *)(vm + CE_ANM_VM_SPRITE_SIZE + 4));
        if (w > 0 && h > 0) { if (out_w) *out_w = w; if (out_h) *out_h = h; return 1; }
    }
    w = yk_round_px(*(const float *)(e + CE_ENEMY_FINAL_SPRITE_W));
    h = yk_round_px(*(const float *)(e + CE_ENEMY_FINAL_SPRITE_H));
    if (w > 0 && h > 0) { if (out_w) *out_w = w; if (out_h) *out_h = h; return 1; }
    return 0;
}

/* 「是 boss 吗」= 这个 id 在敌人管理器的 `CE_EM_BOSS_IDS[0..3]` 里。
 * 判据与数据源与 117 `cards/animal.c:724` 的 `enemy_is_boss()` **完全相同**（那边是 static ⇒
 * 不共享函数，但**数据源只有这一处**：管理器那 4 格）。*/
static int yk_enemy_is_boss(const uint8_t *e)
{
    uint8_t *em = CE_ENEMY_MGR();
    int32_t id;
    int i;
    if (!e || !em) return 0;
    id = *(const int32_t *)(e + CE_ENEMY_ID);
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i)
        if (*(const int32_t *)(em + CE_EM_BOSS_IDS + (size_t)i * 4) == id) return 1;
    return 0;
}

/* ★★★ 2026-10-01（**owner 实跑崩溃，dump 已定位到这一处**）：
 *   `crashdumps/th18.exe.15640.dmp`：EIP = `mod_th18_card_expand.dll+0x17017`，崩点字节 **`8b 40 24`**
 *   = `mov eax,[eax+0x24]`（就是**读 VM+0x24 的 sprite 号**），而 **`eax = 0x08f1ff3f` 是垃圾值**
 *   ⇒ 访问 `0x08f1ff3f + 0x24 = 0x08f1ff63`，**与异常参数一致** ✓
 *   ⇒ **`vm ? … : -1` 的 NULL 保护不够**：`vm` 是**悬垂/垃圾指针**（引擎撤了 VM，或被复用的那一格不是 VM）。
 * 修法（两层）：① **地址有效性**：`IsBadReadPtr(vm+0x24, 4)` ⇒ 不可读就当无效；
 *   ② **只读一次并缓存**（键 = `enemy_id`，32 格）：悬垂窗口从「每帧」缩到「一次」；
 *   无效 ⇒ `spid = -1`（**退回纯尺寸门**）并计 `badfp`（日志可见）。
 * ⚠ 缓存条目在敌人消失后由「表满时覆盖最旧」自然淘汰（32 格 ≫ 同屏大怪数）。*/
/* ★ raw Win32（`kernel32`）：`IsBadReadPtr` —— 本项目没有 windows.h（见 `sdk.h` 的做法），
 *   这里按 ABI 自己声明（`__stdcall`、返回 BOOL、两参数）。用于**崩溃后**的地址有效性检查（§BE #177）。*/
__declspec(dllimport) int __stdcall IsBadReadPtr(const void *lp, unsigned cb);

#define YK_YSP_MAX 32u
static uint32_t s_ysp_id[YK_YSP_MAX];
static int32_t  s_ysp_val[YK_YSP_MAX];
static unsigned s_ysp_badfp;      /* ★ 读到无效/悬垂 VM 指针的次数（诊断）*/

static int yk_sprite_id_for(const uint8_t *e, uint32_t id)
{
    unsigned i, free_i = YK_YSP_MAX;
    uint8_t *vm;
    int32_t vid;
    int v;
    if (!e || !id) return -1;
    for (i = 0; i < YK_YSP_MAX; ++i) {
        if (s_ysp_id[i] == id) return (int)s_ysp_val[i];          /* ★ 命中缓存：**不再碰 VM** */
        if (!s_ysp_id[i] && free_i == YK_YSP_MAX) free_i = i;
    }
    /* ★★★ 2026-10-01（**订正 id/指针混用**，见 `AUDIT.md` §BE #186）：`CE_ENEMY_ANM_VM0`
     *   （`enemy+0x1350` = `zEnemyData.anm_vm_ids[0]`）是 **VM id**、**不是指针** ——
     *   `engine.h` 这一格自己的注就这么写、117 `cards/animal.c` 的 `animal_enemy_sprite_size()`
     *   也是 `id → ce_anm_get_vm()`。这里原来按 `*(uint8_t **)` 解引用 ⇒ 把 **id 当地址**读
     *   ⇒ ① 几乎恒 `IsBadReadPtr` ⇒ `spid` 恒 `-1` ⇒ **白名单（48×48 的 72..87/292..307）
     *      永远不命中**，48×48 大妖精拿不到绕转使魔（= owner 报的「4 面道中那只没使魔」）；
     *   ② #177 那个崩溃值 `0x08f1ff3f` 的形状 = 日志里那种 VM id（`15f0ff8f`/`18397e18`…）。
     *   修法 = 与 `yk_enemy_sprite_size()` 逐字同一条读法；`IsBadReadPtr` **保留**作第二层
     *   （id 解析出来的 VM 仍可能是刚撤的那一支）。若万一原读法才对：`ce_anm_get_vm(不是 id)`
     *   只会返回 NULL ⇒ `spid = -1` = 今天的行为，**不会更差**。*/
    vid = *(const int32_t *)(e + CE_ENEMY_ANM_VM0);
    vm = vid ? (uint8_t *)ce_anm_get_vm((uint32_t)vid) : 0;
    if (!vm) v = -1;
    else if (IsBadReadPtr((const void *)(vm + CE_ANM_VM_SPRITE_ID), 4)) { v = -1; ++s_ysp_badfp; }
    else v = *(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID);
    if (free_i < YK_YSP_MAX) { s_ysp_id[free_i] = id; s_ysp_val[free_i] = v; }
    return v;
}

static int yk_current_character(void);   /* 前向声明：弹色那一支要用它（定义在下面）*/

/* ★★★ 2026-09-30（**owner 口径**）：「把**早苗妖侧的红色弹幕变成那个秋天的黄色** —— 到时候**逢魔再变成
 *   红色**，否则**红色变红一点不明显**」。
 * 两个色都是**一手值**（TH16 `pl03.anm` 同族的两支脚本各写一支）：
 *   · **秋黄 `(224,192,64)`** = TH16 `pl03.anm` **script10** 写的那个色（⛔ 我们**默认**用它；
 *     构建期把源脚本的 `color(255,48,48)` 换掉 —— 见 `build_player_anm.py` 的 `_yk_bullet_block`）；
 *   · **红 `(255,48,48)`** = TH16 `pl03.anm` **script6..9**（= 我们移植的那四支的**原件色**）⇒ 逢魔态用它。
 * ⚠ 为什么**不是移植偏差**：四支子机弹脚本本来就是红（逐字照抄 TH16），红是**原作色**；owner 要的是
 *   「默认换秋黄 + 逢魔回红」这条**口径**（登记 `TUNING.md` + `AUDIT.md`）。
 * ⚠ 色格式 = 引擎的 `0xAARRGGBB`，**只换 RGB、alpha 原样**（与 `YK_FENGMA_TINT_RGB` / `yk_tint_apply`
 *   同一条规矩：alpha 由弹脚本的 `alpha(170)` 管）。*/
#define YK_AYA_BULLET_RGB_YELLOW 0x00e0c040u   /* 秋黄（TH16 pl03.anm script10 一手色）*/
#define YK_AYA_BULLET_RGB_RED    0x00ff3030u   /* 红（TH16 pl03.anm script6 一手色 = 我们移植的原件色）*/

/* 每帧：**只在早苗（机体 3）+ 聚焦（妖侧）**时，把我们那批子机弹（判据 = 当前 RGB 正好是这两个色之一）
 * 刷成「逢魔 ⇒ 红 / 否则 ⇒ 秋黄」。**同帧先判逢魔再落色** ⇒ 切换帧不会闪回默认色；退出逢魔立刻回黄。
 * 只换 RGB、不动 alpha/弹道/伤害/anm 帧序。成本 = 一次 512 格的弹池扫描（与决死变红那套同量级）。*/
static void yk_aya_bullet_color_tick(yk_card_t *s)
{
    uint8_t *pl;
    unsigned i;
    if (!s) return;
    if (yk_current_character() != YK_CHAR_SANAE) return;      /* 只早苗 */
    if (!yk_focused()) return;                         /* 只妖侧（低速）*/
    pl = CE_PLAYER();
    if (!pl) return;
    for (i = 0; i < CE_PB_COUNT; ++i) {
        uint8_t *b = pl + CE_PB_POOL + (size_t)i * CE_PB_STRIDE;
        volatile uint32_t *c;
        uint32_t v;
        if (*(const int32_t *)(b + CE_PB_STATE) != 1) continue;            /* 1 = 活着的弹 */
        c = (volatile uint32_t *)(b + YK_BULLET_VM + CE_ANM_VM_COLOR1);
        v = *c;
        if (v == ((v & 0xff000000u) | YK_AYA_BULLET_RGB_RED)) {            /* 已是红 ⇒ 非逢魔则回黄 */
            if (!s->fengma) *c = (v & 0xff000000u) | YK_AYA_BULLET_RGB_YELLOW;
        } else if (v == ((v & 0xff000000u) | YK_AYA_BULLET_RGB_YELLOW)) {  /* 已是黄 ⇒ 逢魔则转红 */
            if (s->fengma) *c = (v & 0xff000000u) | YK_AYA_BULLET_RGB_RED;
        }
    }
}

/* ★★★ 2026-09-30（owner：「忠于原作就得加拖尾」）：**使魔拖尾**的一颗火花。
 * 一手：`trailFamiliarSet`（opcode 83，handler `th08:0x41e532`）置 `enemy+0x3328`（`flags_hi`）**bit1**
 *   之后，引擎**周期性**在**使魔当前位置**撒效果 `0x26` = `etama.anm` **script71**（我们的 `` `370` ``，
 *   见 `assets/ability/scripts/370_youkai_familiar_trail_s71.anm.txt`：`sprite(@YOUKAI_FAM_TRAIL)` 16×16、
 *   `blendMode(1)` 加法、`scale = 1.1 + rand(1.0)` → `scaleTime(20,0,0,0)`、`alpha = 48 + rand(64)`、
 *   `color = 128 + rand(64)` ×3）。
 * ⚠ **故意不记句柄**：这支脚本正文最后是 `+20: delete()` —— **它自己 20 帧后删掉自己**
 *   （照 `yk_flash_spawn` 的范式）；卡侧既不需要跟踪、也不需要在 `on_delete`/`retire` 里回收
 *   —— 那不是「漏收」，是**脚本的生命周期本来就是自管**的（删卡后已经在飞的最多再飘 20 帧）。
 * ⚠ 它**不占我们自己的 VM 池**（那些池是 `s_orbit[]`/`s_fam_vm[]` 里我们跟句柄的那些）。*/
static void yk_orbit_trail_spawn(float x, float y)
{
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(),
                               CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_TRAIL_S71, YK_FAM_ANM_LAYER);
    if (!id) return;                                      /* 起不出来只少观感（与 `yk_flash_spawn` 同）*/
    ce_anm_set_pos(id, x, y, 0.0f);
    /* 基线 scale/alpha/color 由脚本自己 `rand` 出来 ⇒ 这里一个字都不写（照一手）。 */
}

/* 起跟随 VM 并每帧跟随（照 `yk_fam_vm_follow()` 的写法：句柄被引擎清过 ⇒ 按同一变体自愈重起）。
 * 形态口径与映射使魔**同一套**（owner 的 standing spec：「不按 shift 看得见使魔 / 按 shift 变成魔法阵」）：
 *   · 人侧：画本体（四色原生切图，`yk_fam_kind_script()` —— 颜色由 `YK_FAM_KIND_*` 语义表定）；
 *   · 妖侧：本体透明（alpha 置 0，VM 不删 ⇒ 切回来一帧就恢复）+ 每台一支常驻魔法阵环。*/
static void yk_orbit_vm_sync(yk_orbit_t *o)
{
    uint8_t *f = yk_familiar_by_id(o->fam_id);
    const float *pp;
    int youkai_side;
    if (!f) return;
    pp = ce_enemy_pos(f);
    if (!o->anm_id || !ce_anm_get_vm(o->anm_id)) {
        o->anm_id = ce_anm_spawn(CE_ABILITY_ANM(), yk_fam_kind_script((int)o->kind), YK_FAM_ANM_LAYER);
        if (!o->anm_id) {
            if (!s_orbit_vm_failed) {                     /* 只记一次（不刷屏）*/
                s_orbit_vm_failed = 1;
                ce_log("youkai/orbit: follow-VM spawn FAILED (familiar #%u, script %d, layer %d) - "
                       "the shells stay alive (they still feed the kill-burst count) but invisible",
                       o->fam_id, yk_fam_kind_script((int)o->kind), YK_FAM_ANM_LAYER);
            }
            return;
        }
        if (pp) ce_anm_set_pos(o->anm_id, pp[0], pp[1], 0.0f);
    } else if (pp && !ce_anm_set_pos(o->anm_id, pp[0], pp[1], 0.0f)) {
        o->anm_id = 0;                                    /* 句柄作废 ⇒ 下一帧自愈重起 */
    }
    youkai_side = yk_focused() ? 1 : 0;
    if (o->anm_id) ce_anm_set_color(o->anm_id, youkai_side ? 0x00ffffffu : 0xffffffffu);
    /* ★★★ 2026-10-01（**owner 新报的真 bug**：「对于妖侧，**使魔会挡住弹幕**，也就是妖侧打不进去」）——
     *   我们原来只置了 `CE_ENEMY_FLAG_NO_DAMAGE`（**不可伤害**），但**碰撞盒还在** ⇒ 自机弹会被它吃掉
     *   （打不出伤害、也穿不过去）= 「挡住弹幕」。一手/owner 口径：妖侧使魔 = **魔法阵**
     *   ⇒ **不被自机打到、也不碰撞**（自机弹**穿过去**）。
     *   修法：**妖侧把碰撞盒置 0**（`zEnemyData.hurtbox_size` = `enemy+0x133c`/`+0x1340` 两个 float，
     *   与 117 读的是同一对），**原值存进 `o->hurt_x/hurt_y`**（只存一次）；**人侧还原**。
     *   ⚠ 与 NO_DAMAGE 是**两件事**：那个管「打到了也不掉血」，这个管「根本不吃弹」。*/
    if (youkai_side) {
        float *hb = (float *)(f + 0x133c);
        if (!o->hurt_saved && hb[0] > 0.0f) { o->hurt_x = hb[0]; o->hurt_y = hb[1]; o->hurt_saved = 1; }
        if (o->hurt_saved) { hb[0] = 0.0f; hb[1] = 0.0f; }
    } else if (o->hurt_saved) {
        float *hb = (float *)(f + 0x133c);
        hb[0] = o->hurt_x; hb[1] = o->hurt_y;
        o->hurt_saved = 0;
    }
    /* ★ 与映射使魔**同一条口径**（owner 2026-09-30）：环**两侧都常驻**，换侧只换中断
     *   （人 1 紫 / 妖 2 蓝），尺寸由脚本分侧（0.5 / 0.8）。*/
    if (!o->circle_id || !ce_anm_get_vm(o->circle_id)) {
        ++s_orbit_ring_try;
        o->circle_id = yk_vm_spawn(CE_ABILITY_ANM(),
                                    CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING, YK_FAM_ANM_LAYER);
        o->circle_side = -1;
        if (o->circle_id) ++s_orbit_ring_made;
        else if ((++s_orbit_ring_fail == 1u) || (s_orbit_ring_fail % 120u) == 0u) {
            /* ★★★ 2026-10-01（owner 实跑 `target enemy #152 ... ring vms try 13 made 3`）：
             *   同一帧一次建 16 台使魔 ⇒ 同帧要 32 支 **ability.anm** VM（跟随 + 环），
             *   而 `ce_anm_spawn` 池满时返回 0 ⇒ 环这一支**当场补不上**（下一帧还有重试：
             *   `circle_id` 保持 0 就会再进这里）。这一行 = 「环建不出来」的判据：
             *   若它**成规模地反复出现** ⇒ 是池/脚本的**长期**问题（不是同帧瞬时压力）。*/
            ce_log("youkai/orbit: ring VM spawn FAILED %u time(s) (familiar #%u, script %d, layer %d; "
                   "cumulative try %u made %u) - `ce_anm_spawn` returned 0 (ability.anm VM pool full?) "
                   "and this familiar's ring retries next frame",
                   s_orbit_ring_fail, o->fam_id, (int)CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING,
                   (int)YK_FAM_ANM_LAYER, s_orbit_ring_try, s_orbit_ring_made);
        }
    }
    if (o->circle_id) {
        if (o->circle_side != (int8_t)youkai_side) {
            ce_anm_interrupt(o->circle_id, youkai_side ? 2 : 1);
            o->circle_side = (int8_t)youkai_side;
            ++s_orbit_ring_int;
        }
        if (pp && !ce_anm_set_pos(o->circle_id, pp[0], pp[1], 0.0f)) {
            o->circle_id = 0;
            o->circle_side = -1;
        }
    }
}

/* 找空槽建一台绕转使魔：位置 = **圆心**（= 建壳那一刻的父敌坐标）+ 半径 × (cosθ, sinθ)。
 * 建壳那一刻就**清免伤位**（owner 口径：打得掉）并起**跟随 VM**（少等一帧 = 少一帧「看不见」的窗口）；
 * 半径由 `yk_orbit_radius_at(age)` 给（随帧龄外扩）、相位由 `yk_orbit_phase_*` 给。
 *
 * **血量 = 一手值 × 缩放旋钮**（第四轮 owner 口径：「那就 **hp 切成一手**」；那句
 *   「一碰就碎主要是形容**杂鱼**的」是**留旋钮**的依据）：
 *   · 一手：TH08 使魔 HP = `familiarCreate` 的**第 4 实参**（落点 `enemy+0x2dfc`；`EnemyManager::create_enemy`
 *     `th08:0x42a680`：`cmp [ebp+0x10],0` / `jl`（负数 = 用默认）/ `mov [e+0x2dfc],…`）；
 *     实测（24 ECL 全量）**50 / 100 / 180 / 200 / 300 / 400 / 600 / 2400**、**道中几乎恒 100**，
 *     218 个使魔子程序里**没有一个**用 `lifeSet` 覆盖 ⇒ 那个实参就是它的血；
 *   · 我们：**2026-10-01 起 = owner 新口径**「档 0（母机 HP ≤ 200）恒 **100**、其它 **`min(999, HP/2)`**」
 *     （纯函数 `yk_orbit_fam_hp()`）+ `YK_ORBIT_HP_SCALE`（**当前 = 1**）+ 钳 999。
 *     ⚠ **旧口径**（第 4 轮）「按台数档取一手表 `{100,100,300,600}`」保留在
 *     `YK_ORBIT_HP_ONEHAND` / `yk_orbit_familiar_hp()` 里**作对照，卡侧不再调用**。
 *     ⚠ 父敌最大血量现在**同时**决定**台数**（`yk_orbit_count_for_hp`）与**每台血量**
 *     （`yk_orbit_fam_hp`）。*/
static uint8_t *yk_orbit_spawn_one(uint32_t parent_id, int count, int k, int32_t life,
                                   float cx, float cy)
{
    float dx = 0.0f, dy = 0.0f, radius, ang = 0.0f;
    yk_orbit_layout_t L;
    uint8_t *f;
    yk_orbit_t *o;
    unsigned step;
    unsigned i;

    if (yk_orbit_free() == 0u) return 0;
    for (i = 0; i < YK_ORBIT_MAX; ++i) if (!s_orbit[i].fam_id) break;
    if (i >= YK_ORBIT_MAX) return 0;
    o = &s_orbit[i];
    /* ★★★ 2026-09-30（owner 口径 + 一手）：**按台数分圈**（`yk_orbit_layout` 表驱动）——
     *   8 台 = 3+5（30/60、ω 6/4.5、内外反向）、13 台 = 3+5+5（30/60/90、ω 6/4.5/3、内反向、
     *   中/外同向且共用初角）、其余（含 14–18 台与 ≤4 台）回落**单圈**（半径 60、ω 6、同向）。*/
    if (!yk_orbit_layout(count, k, &L)) return 0;
    radius = 0.0f;                                        /* 出生贴在本体身上（见 struct 里的注）*/
    ang = L.phase_deg + (float)yk_orbit_phase_id(parent_id);   /* 初角 + 每敌错相 */
    ang = ang - 360.0f * (float)((int)(ang / 360.0f));         /* 归一到 [0,360) */
    step = (unsigned)((int)(ang / (float)YK_ORBIT_STEP_DEG)) % YK_ORBIT_STEPS;
    if (!yk_orbit_offset(step, radius, &dx, &dy)) return 0;
    f = yk_spawn_familiar_parent(cx + dx, cy + dy, parent_id, life);   /* life = 父敌最大血量（口径）*/
    if (!f) return 0;
    o->fam_id = *(const uint32_t *)(f + CE_ENEMY_ID);     /* 键 = 使魔自己的 enemy_id */
    o->parent_id = parent_id;
    o->cx = cx;                                           /* ★ 定圆心（一手，见 struct 里的注）*/
    o->cy = cy;
    o->radius = radius;
    o->substep = step * YK_ORBIT_STEP_DIV;                /* 初相（格号 → substep，别慢半拍）*/
    o->age = 0;                                           /* 帧龄从 0 起 ⇒ 半径 0（外扩的起点）*/
    o->ang_deg = ang;                                     /* ★ 初角（layout 的初角 + 每敌错相）*/
    o->omega_deg = L.omega_deg;                           /* ★ 该台角速度（含转向符号）*/
    o->radius_final = L.radius;                           /* ★ 该台最终半径（分圈 30/60/90）*/
    o->ring = (int8_t)L.ring;
    o->trail_t = 0;                                       /* 拖尾节拍：从 0 起，攒满 interval 才撒第一颗*/
    o->orphan = 0;
    /* ★★★ 2026-09-30（owner 四色口径）：额外使魔恒 = `YK_FAM_KIND_NO_MELEE_SHOT_SAME`
     *   （**不会体术** + 弹一样）⇒ 这一批**全部同色**，不随生成顺序轮、也不按发射口槽取。*/
    o->kind = YK_FAM_KIND_NO_MELEE_SHOT_SAME;
    o->anm_id = 0;
    o->circle_id = 0;
    if (!o->fam_id) { o->parent_id = 0; return 0; }       /* 极端：刚建出来就没 id ⇒ 当失败 */
    yk_orbit_pin(f, cx + dx, cy + dy);
    yk_orbit_vm_sync(o);
    ++s_orbit_spawned;
    return f;
}

/* 首次命中登记 / 到点判定：返回 1 = **可以建了**（一到点就把登记删掉）；0 = 还没到（含刚登记）。
 * ⚠ 扫场是每 `YK_ORBIT_SCAN_INTERVAL`(30) 帧一次 ⇒ 实际延时会被量化到 30 帧的整数倍。*/
static int yk_orbit_delay_ok(uint32_t id, uint32_t frame)
{
    unsigned i, free_i = YK_ORBIT_SEEN_MAX;
    if (!id) return 0;
    for (i = 0; i < YK_ORBIT_SEEN_MAX; ++i) {
        if (s_orbit_seen[i].id == id) {
            if (frame - s_orbit_seen[i].frame >= YK_ORBIT_DELAY_FRAMES) { s_orbit_seen[i].id = 0; return 1; }
            return 0;                                    /* 还没到点（下一次扫场再看）*/
        }
        if (!s_orbit_seen[i].id && free_i == YK_ORBIT_SEEN_MAX) free_i = i;
    }
    if (free_i < YK_ORBIT_SEEN_MAX) { s_orbit_seen[free_i].id = id; s_orbit_seen[free_i].frame = frame; }
    return 0;                                            /* 刚登记（或表满）⇒ 这一帧不建 */
}

/* 清掉「父敌已经没了」的登记（每 30 帧的扫场里顺手做；`em` 为空时整表清）。*/
static void yk_orbit_seen_prune(uint8_t *em)
{
    unsigned i;
    for (i = 0; i < YK_ORBIT_SEEN_MAX; ++i) {
        if (!s_orbit_seen[i].id) continue;
        if (!em || !ce_enemy_by_id(em, (int32_t)s_orbit_seen[i].id)) s_orbit_seen[i].id = 0;
    }
    /* ★★★ 2026-09-30（owner bug）：**「已给过」的账**按「父敌还在不在」老化 ——
     *   还在 ⇒ 清零 `absent`（记录留着，**与使魔死活无关**）；连续 `YK_ORBIT_GIFTED_GRACE`
     *   次扫场找不到 ⇒ 清掉（敌机 id 不复用，所以同名 id 再出现就是新敌人、可以重新给）。*/
    for (i = 0; i < YK_ORBIT_GIFTED_MAX; ++i) {
        yk_orbit_gifted_t *g = &s_orbit_gifted[i];
        int present;
        if (!g->id) continue;
        present = em && ce_enemy_by_id(em, (int32_t)g->id) != 0;
        if (yk_orbit_gift_prune(&g->absent, present)) {
            g->id = 0;
            g->gifted = 0;   /* ★ 2026-10-01：记忆值随记录一起清（别让复用格漏出上一只的台数）*/
        }
    }
}

/* ★★★ 2026-10-01（#89 的**无条件探针**，实现在这里）：**每只敌人**一行（**同一 `enemy_id` 只打一次**、
 *   全局上限 `128` 行，够看一面）。
 *   ⚠ **位置 = `yk_orbit_scan` 逐敌循环的**最前面**（读完 id 就调）—— 它要回答的正是「这一只
 *     **为什么**没走补使魔那条路」：子敌人 / 不可锁定 / boss / 已给过 / 延时门 / 尺寸读不出来…
 *     **全都在下面**，所以必须在**所有筛选之前**读尺寸。旧版（#180/#182）挂在筛选**之后**
 *     ⇒ 它**永远看不到** boss 与被链根门排掉的子敌人 ⇒ owner 报的「4 面道中那只 `probe` 0 行」
 *     完全可能只是**它根本没走到那一步**，而不是尺寸读值失败。本版按「无条件」重摆。
 *   · `readfail` = VM 取不到 / `vm+0x54` 不可读 / `rect` 全 0；
 *   · `final` = `+0x6204/+0x6208`（`final_sprite_size` = `sprite_size × scale`，**float2**）
 *     与 VM 的 `sprite_size`（`+0x74/+0x78`）**交叉**，用来判断该信哪一条来源；
 *   · `sprite` = `vm+0x24`（`CE_ANM_VM_SPRITE_ID`）；VM 由 `CE_ENEMY_ANM_VM0`（**VM id**）
 *     经 `ce_anm_get_vm()` 解析 —— 与 `yk_enemy_sprite_size()` / `yk_sprite_id_for()` 同一条读法。
 *   ⚠ **只加日志**：不改任何判据、不动池、不碰 VM。*/
static void yk_spriteprobe_one(const uint8_t *e, uint32_t id)
{
    static uint32_t s_probe_ids[128];
    static unsigned s_probe_n, s_probe_fail, s_probe_seen;
    uint8_t *vm;
    int32_t vid;
    int w = 0, h = 0, spid, big, readfail = 0, fw, fh;
    float sx = 0.0f, sy = 0.0f, vw, vh;
    unsigned pi;

    if (!e || !id) return;
    ++s_probe_seen;
    for (pi = 0; pi < s_probe_n; ++pi) if (s_probe_ids[pi] == id) return;   /* 这只打过（同 id 只一行）*/
    if (s_probe_n >= 128u) return;                                          /* 上限 128 行 */
    s_probe_ids[s_probe_n++] = id;

    (void)yk_enemy_sprite_size(e, &w, &h);        /* 判据用的**同一个**读法（VM id → VM → +0x74）*/
    spid = yk_sprite_id_for(e, id);
    big = yk_big_by_sprite_id(w, h, spid);
    vid = *(const int32_t *)(e + CE_ENEMY_ANM_VM0);                  /* zEnemyData.anm_vm_ids[0] = VM **id** */
    vm = vid ? (uint8_t *)ce_anm_get_vm((uint32_t)vid) : 0;
    if (!vm || IsBadReadPtr((const void *)(vm + CE_ANM_VM_SCALE), 12)) readfail = 1;
    else {
        sx = *(const float *)(vm + CE_ANM_VM_SCALE);
        sy = *(const float *)(vm + CE_ANM_VM_SCALE + 4);
    }
    if (!w && !h) readfail = 1;
    vw = (float)w * sx; vh = (float)h * sy;
    fw = yk_round_px(*(const float *)(e + CE_ENEMY_FINAL_SPRITE_W));  /* ★ float2，不是 int32 */
    fh = yk_round_px(*(const float *)(e + CE_ENEMY_FINAL_SPRITE_H));
    if (readfail) ++s_probe_fail;                                     /* 计数（只增；当前不进台账行）*/
    ce_log("youkai/spriteprobe: enemy #%u sprite %d rect %ux%u scale %.2f vis %.0fx%.0f "
           "final %dx%d big %d%s",
           (unsigned)id, spid, w, h, (double)sx, (double)vw, (double)vh,
           fw, fh, big, readfail ? " readfail" : "");
    (void)s_probe_seen;
}

/* 扫全场：给「大怪」补 `yk_orbit_count_for_hp(父敌最大血量)` 台惰性绕转使魔。
 * 五条筛选（owner 调的 117 判据 + 本卡三条）逐条见下面注释；一敌一对（补过就不再补）。
 * ⚠ **「有没有发射口」不是筛选条件**（第二轮 owner 口径）—— 见下面 ④ 那段注。*/
static void yk_orbit_scan(uint32_t frame)
{
    if (!YK_ENABLE_ORBIT) return;                         /* ★ ⑧ */
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    unsigned seen = 0, roots = 0, bosses = 0, have_emit = 0, small = 0, big = 0, picked = 0;
    if (!em) return;
    ++s_orbit_scans;
    yk_orbit_seen_prune(em);
    CE_ENEMY_FOREACH(em, e) {
        int32_t id, hp_now, life;
        int w = 0, h = 0, want, k, made = 0, rings = 1;   /* rings = `s_orbit_tab[want].nrings`（血量的分母）*/
        const float *pp;
        float cx, cy;
        if (!e) continue;
        ++seen;
        id = *(const int32_t *)(e + CE_ENEMY_ID);
        if (!id) continue;
        /* ★★★ 2026-10-01（#89）：**无条件探针** —— 放在这里（所有筛选之前）的理由见它的定义处。*/
        yk_spriteprobe_one(e, (uint32_t)id);
        /* **链根**：`CE_SPAWN_PARENT == 0` —— 排除 boss 部件 / 我们自己的使魔（它们都是子敌人），
         *   也排除「别的敌人造出来的东西」（一手使魔数数的是**这个敌人自己的**使魔）。
         *   ⚠⚠ **2026-10-01 重要（别顺手「修」这一行）**：这里的 `CE_SPAWN_PARENT`（= 0x50）在
         *   `zEnemy*` 上其实是 **ECL 栈里的一个 int32（脚本局部变量）**，不是父字段（见
         *   `YK_ENEMY_PARENT_ID` 的长注）。**但也不能改成 `YK_ENEMY_PARENT_ID`**：TH18 的关卡时间轴
         *   自己是一只敌人 ⇒ 时间轴刷出来的**每一只**敌机父字段都非 0（`engine/enemy/th18/08`）
         *   ⇒ 换成真字段之后这一门会把**所有**大怪排掉 ⇒ 「惰性绕转使魔」整条链直接死掉。
         *   正确做法是另立判据（父敌是不是 boss + 排掉我们自己的实体），**另开一批**再做；
         *   现状：这一门读 ECL 变量 ⇒ 等效于「ECL 变量恰好非 0 才跳过」，实跑下大怪都过得来
         *   （`youkai/orbit: target ... chain root, not boss`）。登记 `AUDIT.md` §BE（本批）。*/
        if (*(const int32_t *)(e + CE_SPAWN_PARENT) != 0) continue;
        ++roots;
        /* **活着的真敌人**：`ce_enemy_lockable` 排掉死亡 / 未登场 / 不可锁定的那一档。*/
        if (!ce_enemy_lockable(e)) continue;
        /* **不是 boss**（117 的同一判据：id 在管理器那 4 格里）。*/
        if (yk_enemy_is_boss(e)) { ++bosses; continue; }
        /* **「有没有发射口」不再是筛选条件**（★★★ 2026-09-28 第二轮 · owner 口径：
         *   「我觉得可以不考虑发射口的问题，这个是**别的种类**的使魔」）——
         *   已经有 1:1 发射口映射的大怪**照样**补绕转使魔。这里只**数一下**（首次扫场那行日志用）。
         *   ⚠ 与「**不进 `s_map`**」不矛盾：那张表管的是「**发射口** ↔ 使魔」的 1:1 映射与精准闸，
         *     绕转使魔是**另一种使魔**（只绕转、不发弹），它既不该被当成谁的发射口、也不该占那个池；
         *     它要的只是**一颗真敌人实体 + `CE_SPAWN_PARENT`**（颗数用）。两件事互不冲突。*/
        if (yk_map_enemy_slots(&s_map, (uint32_t)id) != 0) ++have_emit;
        /* 一敌一对：已经补过的不再补。*/
        if (yk_orbit_find_parent((uint32_t)id)) continue;
        /* ★★★ 另一回事：**这只父敌以前给过没有** —— 给过就永远不再补（即使使魔全被打死）。
         *   一手：TH08 的使魔是脚本一次性连发、撒完就没了；`yk_orbit_find_parent` 只看「活着」，
         *   拿它当唯一判据就会在使魔死后又补一批（owner 实跑看到的「两批」）。*/
        if (!yk_orbit_gift_maybe((uint32_t)id, frame)) continue;
        /* ★★★ 生成延时（一手形状，见 `YK_ORBIT_DELAY_FRAMES`）：首次命中先只登记，到点才建。*/
        if (!yk_orbit_delay_ok((uint32_t)id, frame)) continue;
        /* **大怪**：贴图**原始**尺寸两边都 >= 64（117 的判据，见 `yk_big_by_sprite`）。*/
        if (!yk_enemy_sprite_size(e, &w, &h)) continue;
        /* ★ 2026-10-01：判据带 **sprite 号**（白名单）—— VM 的当前 sprite 在 `vm+0x24`（`engine.h`）。
         * ⚠ 探针 `yk_spriteprobe_one()`（#89）已经挪到**逐敌循环的最前面**（读完 id 就调），
         *   这里**只剩判据**，不再夹带日志：
         *   ① 尺寸读不出来 ⇒ 上面那条 `continue`（`small++` 都不算）；
         *   ② `yk_big_by_sprite_id()` 不过 ⇒ 这一只**不补**（`small++`）。
         * ⚠⚠ **2026-10-01 回归订正**：`96610aef`（#89）把「链根 / 可锁定 / 非 boss / 已给过 / 延时门 /
         *   尺寸」这**一整套筛选**又插了一遍（当时是套在 `{ int spid = …` 里的 16 行副本），
         *   后果 = `yk_orbit_delay_ok()` **同一帧被调两次**：第一次「到点 ⇒ 清登记、返回 1」，
         *   第二次「重新登记 + 返回 0」⇒ **永远 `continue`，任何大怪都补不出使魔**（延时门自锁）。
         *   这里把副本整段删掉、恢复「筛一遍 ⇒ 判一次」；探针另挪到循环首（见上）。*/
        if (!yk_big_by_sprite_id(w, h, yk_sprite_id_for(e, (uint32_t)id))) { ++small; continue; }
        /* 池：这台敌人要 `want` 个空槽；一个都没有就**这一帧到此为止**（后面同样不够），
         * 并且只记一次日志 —— 本卡有过 1.5 MB/min 的日志洪水事故（`AUDIT.md` §BE #143）。*/
        if (yk_orbit_free() == 0u) {
            if (!s_orbit_pool_full) {
                s_orbit_pool_full = 1;
                ce_log("youkai/orbit: pool full (%d) - no room for more orbit familiars this frame; "
                       "the pool frees up when a parent dies (each big enemy wants up to %d, "
                       "by its max HP)", (int)YK_ORBIT_MAX, (int)YK_ORBIT_MAX_PER_ENEMY);
            }
            break;
        }
        /* ★★★ 台数 = f(父敌**最大血量**)：**TH08 一手统计**的分档表（`yk_orbit_count_for_hp`，
         * 见 `youkai_core.c` 的表头注 + `engine/enemy/th08/03-familiar-count-vs-hp.md`）。
         * ⚠ **父敌 HP 只用来定台数**；使魔**自己的血量**是另一条口径（见下一段）。*/
        hp_now = *(const int32_t *)(e + CE_ENEMY_HP_MAX);
        /* ★★★ 2026-09-28 第六轮（**owner 口径**：「没过一面，同样血量的使魔数 +1」）：
         *   台数 = `HP 档 + 面加成`（面号取 `CE_CURRENT_STAGE()`，1 基；加成 = `min(面−1, 5)`），
         *   钳进 [1, `YK_ORBIT_MAX_PER_ENEMY`] ⇒ 上限 18。纯函数 = `yk_orbit_count_total`。*/
        want = yk_orbit_count_total(hp_now, (int)CE_CURRENT_STAGE());
        /* ★★★ 2026-10-01（**owner 定案口径 · 第 56 批第二轮**）：**使魔血量 = 母机 HP / 圈数**。
         *   （先给过 `min(999, 1/2 HP)`，owner 随后改成「母机 HP / 圈数」⇒ 本卡实现后者。）
         *     · 档 0（`hp_now <= YK_FAM_HP_TIER0_MAX` = 200）⇒ **恒 100**（owner 保留）；
         *     · 其它 ⇒ **`hp_now / 圈数`**（整数除法；圈数 = `s_orbit_tab[want].nrings`，1/2/3）。
         *   `rings` 走 core 的 `yk_orbit_rings_for_count(want)`（**与分圈表同一个出口**，别在卡侧另抄）。
         *   ⚠ **不再封顶 999**：`life` 是**真血量**（可能 >999，例：3200 血 3 圈 ⇒ 1066）⇒
         *     `yk_spawn_familiar_parent()` 里走「spawn 写 ≤999 + 建后回写三格 life」两步
         *     （照 `cards/ufo.c:3518`，躲 `th18:0x42d8ed`/`0x42d8f6` 那条 `flags_lo` 位 30 的门）。
         *   ⚠ 再乘 `YK_ORBIT_HP_SCALE`（默认 1）。
         *   ⚠ 一手 `familiarCreate` 第 4 实参的 `{100,100,300,600}` **保留在注释/旧函数里作对照**，
         *     但**卡侧不再使用**（`yk_orbit_familiar_hp` 现在只被单测引用）。*/
        /* ⚠ 血量只与**母机最大血量 + 圈数**有关（面加成只加台数 ⇒ 它经由 `want` 影响圈数）。
         * 半径则按**最终台数**（`want`）—— 圈大小是视觉量（见 `yk_orbit_spawn_one`）。*/
        rings = yk_orbit_rings_for_count(want);
        life = yk_orbit_hp_for_enemy(hp_now, rings, yk_current_character());   /* ★ 真血量（含机体倍率，可 >999）*/
        ++big;
        /* ★★★ 圆心 = **这一刻的父敌坐标**（一手：见 `yk_orbit_t` 里 `cx/cy` 的注）。之后不再重取。*/
        pp = ce_enemy_pos(e);
        cx = pp ? pp[0] : 0.0f;
        cy = pp ? pp[1] : 0.0f;
        for (k = 0; k < want; ++k) {
            /* 相位：`k * 120 / want` 均匀铺开 + 每敌错相（`(id*17)%120`）—— 见 core 的两个纯函数。*/
            if (yk_orbit_spawn_one((uint32_t)id, want, k, life, cx, cy)) {
                ++made;
                /* ★★★ 2026-09-30（一手 SE）：建使魔那一下的 `se_option.wav`（TH08 id `0x24`，
                 * 三条 opcode 尾巴都 `push 0x24` + `call 0x45d660` = `PlaySoundPositionedByIdx`，
                 * **声像 = 生成者 x**）⇒ 每建成功一台各播一次，x 传**父敌**的 x（= 圆心 x）。
                 * ⚠ 同一帧可能连播 N 次 —— 与一手同形（一手也是同一帧连发 2/4 台）。*/
                ce_play_voice(YOUKAI_FAM_SPAWN, cx);
            }
            if (yk_orbit_free() == 0u) break;                 /* 池正好用光 ⇒ 这台就这些 */
        }
        if (made) {
            yk_orbit_gift_mark((uint32_t)id, frame, (unsigned)made);   /* ★ 真给出去了才记账（含台数 = 记忆值）*/
            ++s_orbit_targets; ++picked;
            /* 每条目标**只打一次**（不刷屏）：这一行就是「哪只大怪被补了、补了几台、凭什么」的判据。*/
            ce_log("youkai/orbit: target enemy #%u sprite %dx%d hp %d stage %d => %d familiar(s) "
                   "ring vms try %u made %u int %u; "
                   "(HP tier %d + stage bonus %d = %d; spawn delay %u f; chain root, not boss; rings %d "
                   "(radius 0->30/60/90 in %u f), per-ring %.1f/4.5/3 deg/frame; fam life %d = "
                   "fam_hp(%d, rings %d, tier0<=%u => 100 / else HP/rings) x %d; colour kind %d "
                   "(melee %d shot_same %d) = script %d; centre pinned at spawn (%.0f,%.0f))",
                   /* ★★★ 2026-10-01 勘误：这一串**参数顺序原来与格式串错位**（全是 int ⇒ 编译器不报），
                    *   实跑读出来是「=> 17 familiar(s) ring vms try 13 made 4 int 17; (HP tier 82 +
                    *   stage bonus 82 = 87 …)」这种**标签与数值对不上**的行（owner 拿它诊断会被误导）。
                    *   现在按格式串的顺序给：`=> familiar(s)` = 实际建成数、`ring vms try/made/int` =
                    *   环 VM 三个计数、`HP tier + stage bonus = 台数`。*/
                   (unsigned)id, w, h, hp_now, (int)CE_CURRENT_STAGE(), made,
                   s_orbit_ring_try, s_orbit_ring_made, s_orbit_ring_int,
                   yk_orbit_count_for_hp(hp_now), yk_orbit_stage_bonus((int)CE_CURRENT_STAGE()), want,
                   (unsigned)YK_ORBIT_DELAY_FRAMES,
                   rings,                                            /* ★ rings（分圈数，走表）*/
                   (unsigned)YK_ORBIT_EXPAND_FRAMES,
                   (double)YK_ORBIT_FINAL_RADIUS / (double)YK_ORBIT_EXPAND_FRAMES,
                   (int)life, yk_orbit_fam_hp(hp_now, rings), rings,
                   (unsigned)YK_FAM_HP_TIER0_MAX,
                   (int)YK_ORBIT_HP_SCALE,
                   YK_FAM_KIND_NO_MELEE_SHOT_SAME,
                   yk_fam_kind_melee(YK_FAM_KIND_NO_MELEE_SHOT_SAME),
                   yk_fam_kind_shot_same(YK_FAM_KIND_NO_MELEE_SHOT_SAME),
                   yk_fam_kind_script(YK_FAM_KIND_NO_MELEE_SHOT_SAME),
                   (double)cx, (double)cy);
        } else if (!s_orbit_spawn_failed) {
            s_orbit_spawn_failed = 1;
            ce_log("youkai/orbit: shell spawn FAILED for enemy #%u (sprite %dx%d hp %d) - "
                   "`ce_spawn_enemy` returned 0 (ECL/manager not ready?)",
                   (unsigned)id, w, h, hp_now);
        }
    }
    if (!s_orbit_first_scan) {          /* 首次扫场自述一行：判「扫了没 / 场上有几只大怪」*/
        s_orbit_first_scan = 1;
        ce_log("youkai/orbit: first scan - %u enemy(ies) seen, %u chain root(s), %u boss, "
               "%u already has emitter slot(s) (they get orbit familiars too now), "
               "%u root(s) with a small sprite, %u big candidate(s), %u spawned",
               seen, roots, bosses, have_emit, small, big, picked);
    }
}

/* 每帧一次（`on_tick_2` 调；两个形态都跑 —— 这些使魔不是「妖侧才有的东西」）。
 * 三件事：① 核生命周期（壳还在吗 / 父敌还在吗）② 推进相位 + 钉位 + 跟随 VM ③ 到点扫场 + 台账。
 * ⚠ `noinline`：`on_tick_2` 里内联着 `ce_damage_rect` 的内联汇编，往它身上再加活值会让 i686
 *   报「inline assembly requires more registers than available」（2026-09-22 实测，同 `yk_fam_vm_check`）。*/
static void __attribute__((noinline)) yk_orbit_tick(uint32_t frame)
{
    if (!YK_ENABLE_ORBIT) return;                         /* ★ ⑧ */
    uint8_t *em = CE_ENEMY_MGR();
    unsigned i;
    if (!em) return;                                  /* 世界没了：什么都不做（撤由 `yk_drop_all` 负责）*/
    for (i = 0; i < YK_ORBIT_MAX; ++i) {
        yk_orbit_t *o = &s_orbit[i];
        uint8_t *f, *pe;
        const float *pp;
        float px, py, dx = 0.0f, dy = 0.0f;
        if (!o->fam_id) continue;
        f = yk_familiar_by_id(o->fam_id);
        if (!f) { yk_orbit_retire(o, "shell gone"); continue; }   /* 被外力清了 ⇒ 清槽 + 收 VM */
        pe = ce_enemy_by_id(em, (int32_t)o->parent_id);
        if (!pe) {
            /* 父敌查不到 ⇒ **等 `YK_ORBIT_ORPHAN_GRACE` 帧**再撤（理由见 `youkai_core.h`：
             * 一手颗数在 EnemyManager 的死亡块里算，可能排在卡 tick **之后** ⇒ 早撤会把颗数打回 0）。*/
            if (++o->orphan > YK_ORBIT_ORPHAN_GRACE) { yk_orbit_retire_orphan(o); continue; }
            yk_orbit_pin(f, *(const float *)(f + CE_ENEMY_POS_X),
                            *(const float *)(f + CE_ENEMY_POS_Y));   /* 原地钉住（免伤位照常清）*/
            yk_orbit_vm_sync(o);
            continue;
        }
        o->orphan = 0;
        /* ★ 2026-09-30：转速 = **该台的 ω**（分圈后 6/4.5/3°/帧，含转向符号）—— 浮点累加后量化到 3° 格。*/
        o->ang_deg += o->omega_deg;
        o->ang_deg = o->ang_deg - 360.0f * (float)((int)(o->ang_deg / 360.0f));
        if (o->ang_deg < 0.0f) o->ang_deg += 360.0f;
        o->substep = (unsigned)((int)(o->ang_deg / (float)YK_ORBIT_STEP_DEG)) % YK_ORBIT_STEPS;
        /* ★★★★ 订正：半径**随帧龄外扩**（出生 0 → 1.7 s 后 60 → 冻结），不再一进场就钉住。*/
        if (o->age < 0xffffffffu) ++o->age;
        yk_orbit_radius_at_r(o->radius_final, (int)o->age, &o->radius);   /* ★ 该台自己的最终半径 */
        /* ★★★★ 2026-09-30 **订正**：圆心 = **父敌当前位置**（一面妖精是 `familiarCreateF` = opcode 92，
         *   带 `TRACK_POS (0x200)` ⇒ 整圈跟着父敌走；只有 opcode 91 才是定圆心 —— 见 struct 里的注）。
         *   `cx/cy` 仍保留为**出生点**（记账/日志），不再参与定位；孤儿宽限那条分支照旧原地钉住。*/
        pp = ce_enemy_pos(pe);
        px = pp ? pp[0] : *(const float *)(f + CE_ENEMY_POS_X);
        py = pp ? pp[1] : *(const float *)(f + CE_ENEMY_POS_Y);
        if (!yk_orbit_offset(yk_orbit_step_of_substep(o->substep), o->radius, &dx, &dy)) continue;
        yk_orbit_pin(f, px + dx, py + dy);
        yk_orbit_vm_sync(o);
        /* ★ 拖尾（每台自己一份节拍；父敌活着才撒 —— 孤儿那 2 帧与收口之后都**不再撒新的**，
         *   已撒的会在 20 帧内自删）。位置 = **使魔当前位置**（一手：撒在使魔身上）。*/
        /* ★★★ 2026-09-30（**一手 + owner 确认**）：一手里这一整块只在 `flags_lo` **bit11 == 0**
         *   那一支里跑（`th08:0x42c43a jne 0x42c4e1`），而 owner 确认**这一 bit 就是高低速/聚焦态**
         *   （原话：「bit == 0 就相当于我们的高低速啊……**低速敌方使魔呈现为魔法阵**」）
         *   ⇒ **只在非聚焦（高速 / 人侧）撒火花**；聚焦（低速 / 妖侧）不撒 —— 那一侧本来就由
         *   每台的**环 VM**（`ce_anm_interrupt(id, 2)`）承担「魔法阵」的外观。
         *   ⚠ 切换那一帧：已撒的火花会在 20 帧内自灭，我们**不追**（也**不续撒**）。*/
        if (!yk_focused() && ++o->trail_t >= s_orbit_trail_eff) {
            o->trail_t = 0;                        /* 有效间隔（阀门可能把它抬高）*/
            yk_orbit_trail_spawn(*(const float *)(f + CE_ENEMY_POS_X),
                                 *(const float *)(f + CE_ENEMY_POS_Y));
        }
    }
    yk_orbit_trail_valve();                                    /* 每帧算一次密度（阀门）*/
    if ((++s_orbit_scan_timer % YK_ORBIT_SCAN_INTERVAL) == 0u) yk_orbit_scan(frame);
    /* 台账（每 120 帧一行，且只在「这件事真的发生过」之后）——
     * 判据行见 `HANDOFF-108.md` §3 第 51 行；累计数跨关不清（看全程）。*/
    if (s_orbit_targets && (frame % 120u) == 0u)
        ce_log("youkai/orbit: targets %u live %u spawned %u killed %u orphan %u pool_full %u "
               "scans %u ring vms %u/%u (inert orbit familiars: no shots, no body hitbox, damageable with "
               "HP = the one-hand value (`familiarCreate` arg4) x YK_ORBIT_HP_SCALE, NOT the "
               "parent's HP; their real job is to be counted by the kill burst through "
               "`YK_ENEMY_PARENT_ID`)",
               s_orbit_targets, yk_orbit_live(), s_orbit_spawned, s_orbit_killed,
               s_orbit_lost_orphan, (unsigned)s_orbit_pool_full, s_orbit_scans,
               s_orbit_ring_made, s_orbit_ring_try);
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
    id = yk_vm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING,
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
        s->aura_id = yk_vm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_AURA,
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
        /* ★★★ 2026-10-01（**owner 口径 ②**）：映射（发射口）使魔的血 = **母机 HP / 2**
         *   （`yk_map_fam_hp()`；档 0 ≤200 ⇒ 100）。口径与动机原文见 `youkai_core.h` 的同名小节；
         *   ⚠ 母机查不到 / `life.maximum` 读不到（≤0）⇒ 回落 `YK_FAM_LIFE = 100`（兜底保留）。*/
        int32_t hp_now = yk_enemy_hp_max_by_id(act->enemy_id);
        int32_t life = (hp_now > 0) ? (int32_t)yk_map_fam_hp((int)hp_now, yk_current_character())
                                    : (int32_t)YK_FAM_LIFE;
        /* ★★★★ 2026-10-01 晚（owner 临时要求）：「把**发射口使魔血量暂时弄成 1**」——
         *   `YK_EMIT_FAM_HP_OVERRIDE > 0` ⇒ **覆盖**上面的正式口径（`yk_map_fam_hp()` 原逻辑**保留**）；
         *   `0` = 关闭覆盖（恢复正式口径）。**这是临时调试值，测完改回 0**（登记 `TUNING §2.1ccc-72`）。*/
        if (YK_EMIT_FAM_HP_OVERRIDE > 0) life = (int32_t)YK_EMIT_FAM_HP_OVERRIDE;
        /* ★★★★★ 2026-10-11（owner：「5 面中 boss **使魔的血量有点太多了**」）——
         *   **每槽的总血量预算不变，按该槽本帧的活跃出点数均分**（多原点 ⇒ 2 只 × 满血 = 双倍）。
         *   除数 = 站点最近一次 `etOn` 写下的「该真实槽本帧最大 ord + 1」（`yk_slot_origin_count`），
         *   带 3 帧时效 ⇒ **单出点场景除数 = 1、`life` 逐位不变**；每次重建/新波重新算。
         *   ⚠ 同点共壳时只有第一只真 spawn ⇒ 总预算正好落在那一只上（不重复扣）。*/
        if (YK_EMIT_HP_SPLIT_MODE) {
            int norg = yk_slot_origin_count(act->enemy_id, act->slot);
            if (norg > 1) {
                int32_t before = life;
                life = life / (int32_t)norg;
                if (life < 1) life = 1;
                if (s_hpsplit_logged < 8u) {
                    ++s_hpsplit_logged;
                    ce_log("youkai/emit HP-SPLIT: enemy_id %u slot %d key %d has %d active origins "
                           "=> life %d / %d = %d (per-slot budget unchanged; single-origin scenes are "
                           "bit-identical)", act->enemy_id, YK_EMIT_REAL_SLOT(act->slot), act->slot,
                           norg, (int)before, norg, (int)life);
                }
            }
        }
        if (s_hp_logged < 8u) {
            ++s_hp_logged;
            ce_log("youkai/emit: CREATE hp=%d%s (enemy_id %u slot %d) - one shot must kill it; then the "
                   "slot is vetoed until a wave/rearm edge or the TTL",
                   (int)life, (YK_EMIT_FAM_HP_OVERRIDE > 0) ? " (override)" : "", act->enemy_id, act->slot);
        }
        /* ★ owner：「瞬移不会重建血量吧」—— 若走到这里时那一格**还有活壳**，说明有路径在重建
         *   （那才会重设 life）⇒ 记账（`emit hp_reset`，正常恒 0，是实跑判据）。*/
        {
            yk_slot_t *slc = yk_map_find(&s_map, act->enemy_id, act->slot);
            if (slc && slc->fam_id) ++s_emit_hp_reset;
        }
        /* ★★★★ 2026-10-01 晚（owner：5 面中 boss「两个槽出点完全相同」）——**同出点去重**：
         *   新槽的出点若与**同一母机已有壳**的位置重合（ε 内）⇒ **不再 spawn 第二只**，
         *   而是把该槽**绑到那只壳上**（`yk_map_bind_fam`）⇒ 观感 = 一只（与「一槽一壳 = 视觉代理」一致），
         *   且打死那只壳会让**两个槽一起停火**（`yk_fam_broken` 逐槽扫同 id —— 见 core）。*/
        if (YK_EMIT_SHARE_SAME_POS) {
            unsigned q;
            uint32_t shared = 0;
            for (q = 0; q < YK_FAM_MAX; ++q) {
                yk_slot_t *o = &s_map.e[q];
                uint8_t *of;
                if (!o->fam_id || o->enemy_id != act->enemy_id) continue;
                if (o->slot == act->slot) continue;
                of = yk_familiar_by_id(o->fam_id);
                if (!of) continue;
                {   float dx = o->x - act->x, dy = o->y - act->y;
                    if (dx < 0.0f) dx = -dx;
                    if (dy < 0.0f) dy = -dy;
                    if (dx <= 0.5f && dy <= 0.5f) { shared = o->fam_id; break; }   /* ε = 0.5 px */
                }
            }
            if (shared) {
                yk_map_bind_fam(&s_map, act->enemy_id, act->slot, shared);
                if (s_shared_logged < 8u) {
                    ++s_shared_logged;
                    ce_log("youkai/emit: slot %d shares the shell of an existing slot (fam_id %u, pos "
                           "(%.1f,%.1f)) => no second entity (killing it closes BOTH slots)",
                           act->slot, shared, (double)act->x, (double)act->y);
                }
                return;
            }
        }
        uint8_t *f = yk_spawn_familiar(act->x, act->y, life);
        uint32_t fid;
        if (!f) {
            if (s) ++s->fam_full;
            if (s) ce_log("youkai/emit: fam_full=%u (map table refused a slot) - enemy_id %u slot %d",
                          s->fam_full, act->enemy_id, act->slot);
            if (s && !s->logged_full) {
                s->logged_full = 1;
                ce_log("youkai: familiar spawn FAILED at (%.1f, %.1f) - one emitter stays "
                       "without a familiar (table full / heap refused)", act->x, act->y);
            }
            return;
        }
        fid = *(uint32_t *)(f + CE_ENEMY_ID);
        /* ★ 2026-10-01 晚（owner：空格「啥用没有」= 我们的壳**父链是 0** ⇒ 活体校验全拒）：
         *   spawn 后把父 id 写成**该槽的母机 id** ⇒ ① 空格可用；② 这条父子关系本来就该有。*/
        *(uint32_t *)(f + YK_ENEMY_PARENT_ID) = act->enemy_id;
        yk_map_gift_mark(act->enemy_id, act->slot, ce_sdk_frame());   /* ★ P82：记账（修 +N 时序 bug ✓）*/
        ce_log("youkai/emit SPAWN: slot %d key %d fam_id=%u ret=%d pos=(%.1f,%.1f) enemy_id %u",
               YK_EMIT_REAL_SLOT(act->slot), act->slot, fid, f ? 1 : 0, (double)act->x, (double)act->y, act->enemy_id);
        /* ★★★ 2026-10-01（owner：「依旧有一系列不该出现的发弹口使魔」）——**建壳诊断**（前 24 只）：
         *   逐只给出 `(enemy_id, slot)`、`type/color`、`count_1/count_2`、`fired_age`（距该槽最近一次
         *   `etOn` 过了多少帧）、`stage`、以及建壳用的坐标 ⇒ 用来判「配了没发出 / 激光槽 / `etCopy` / 别类」。
         *   ⚠ 计数用静态值，跨局累加（只作定位用）。*/
        if (s_shell_logged < 24u) {
            /* ⚠ 2026-10-01 订正：`type/color/count` 必须读**源敌机**（以前误读了我们自己那只惰性壳 ⇒ 全 0）。*/
            const uint8_t *srcprops = 0;
            {   uint8_t *se = ce_enemy_by_id(CE_ENEMY_MGR(), act->enemy_id);
                /* ★ 2026-10-11：`act->slot` 现在是**发射口键**（可能 = 真实槽 + 16*ord）
                 *   ⇒ 读引擎字段必须先折回真实槽，否则会读到 `props[16..]`（越界）。*/
                if (se) srcprops = se + YK_EMIT_PROPS + (size_t)YK_EMIT_REAL_SLOT(act->slot) * YK_EMIT_PROP_STRIDE;
            }
            ++s_shell_logged;
            ce_log("youkai/emit: CREATE shell #%u for (enemy_id %u, slot %d key %d) type=%d color=%d "
                   "count_1=%d count_2=%d fired_age=%u stage=%d pos=(%.1f,%.1f) life=%d",
                   s_shell_logged, act->enemy_id, YK_EMIT_REAL_SLOT(act->slot), act->slot,
                   *(const int32_t *)(srcprops + 0x00), *(const int32_t *)(srcprops + 0x04),
                   /* ★ 2026-10-11 静态核实订正：引擎的**两层发射计数**是两个 i16 ——
                    *   `props+0x46c` / `props+0x46e`（一手 `th18:0x42789e`/`0x4278b4` 读它当循环上限；
                    *   `etNew` 把它们设成 `0x00010001`，`th18:0x433e2c`）。原读 `+0x14/+0x18` 是错的标签。*/
                   (int)*(const int16_t *)(srcprops + 0x46c), (int)*(const int16_t *)(srcprops + 0x46e),
                   (unsigned)yk_emit_fired_age(act->enemy_id, act->slot),
                   (int)CE_CURRENT_STAGE(), (double)act->x, (double)act->y, (int)life);
        {   /* ★ 坐标审计（owner：「先给数据」）：spawn **传进去**的坐标 vs 引擎**实际落点**。*/
            const float *ap = ce_enemy_pos(f);
            ce_log("youkai/emit COORD-SPAWN: (enemy_id %u, slot %d) requested=(%.1f,%.1f) "
                   "engine_actual=(%.1f,%.1f) delta=(%.1f,%.1f) | source_props type=%d color=%d "
                   "count_1=%d count_2=%d",
                   act->enemy_id, act->slot, (double)act->x, (double)act->y,
                   ap ? (double)ap[0] : 0.0, ap ? (double)ap[1] : 0.0,
                   ap ? (double)(ap[0] - act->x) : 0.0, ap ? (double)(ap[1] - act->y) : 0.0,
                   /* ★ 2026-10-11 订正：这里原来读的是**我们自己那只壳**的 props（标签却写 `source_props`）
                    *   ⇒ 恒 0。源 = 上面已经取好的 `srcprops`（母机那一格）。*/
                   *(const int32_t *)(srcprops + 0x00),
                   *(const int32_t *)(srcprops + 0x04),
                   (int)*(const int16_t *)(srcprops + 0x46c), (int)*(const int16_t *)(srcprops + 0x46e));
        }
        }
        yk_fam_created(&s_map, act->enemy_id, act->slot, fid);
        yk_pin_familiar(f, act->x, act->y);
        /* ★ 壳建好的**那一刻**起跟随 VM（脚本按发射口槽选：槽 i → s53+i 那一套四色之一）。
         * ⚠ 建 VM 只能在 tick / 帧末（要分配），绝不在死亡栈上 —— 这条路径是 `on_tick_2`，合法。*/
        yk_fam_vm_spawn(fid, yk_fam_mapped_kind(act->slot), act->x, act->y);
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

/* ★ 2026-10-18：外部使魔（leveledit）的妖侧外观表——定义在 §3.7b（文件后半）。
 *   撤使魔的五条路都过 `yk_drop_all()` ⇒ 这里必须把本体原色还回去（否则删卡后那批使魔永远隐形）。*/
static void yk_xfam_vm_kill_all(void);

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
    /* ★★★ 2026-09-28（第四十九批）：**惰性绕转使魔**也在这里收（它不在 `s_map` 里 ⇒
     *   `yk_withdraw_all` 撤不到它）—— 删卡 / 换关 / 局末 / 自机没了 / 世界没了五条路**都过这里**
     *   ⇒ 这一句就是「删卡后一只不留」的落点（`METHOD.md` 铁律二）。*/
    yk_orbit_kill_all();
    /* ★ 2026-10-18（leveledit 接管）：他们那批使魔是**别人的敌人**，我们只是把本体藏了 + 起了环
     *   ⇒ 撤的时候必须**把本体原色还回去**（否则删卡 / 换关之后它们永远隐形）。
     *   五条退休路都过这里（见上面那段长注）；句柄幂等、拿不到就 no-op。*/
    yk_xfam_vm_kill_all();
    yk_zone_clear_all();                  /* ★ 死亡点吸收区：所有退休路都过这里 ⇒ 不留游荡的区 */
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
    if (yk_focused()) { s->fam_miss_latch = 0; return; }   /* 妖侧：可穿梭 ⇒ 不判 */
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

static uint32_t s_ledger_frame;   /* 前向声明（定义在后面；这里只用它做诊断的时间戳）*/
static void yk_emitkey_note(uint32_t enemy_id, int slot, int active, uint32_t child, uint32_t frame);

static void yk_familiars_tick(yk_card_t *s)
{
    if (!YK_ENABLE_MAP_LAYER) return;                     /* ★ ⑨ */
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    yk_action_t act;
    yk_action_t tail[YK_FAM_MAX];
    float off3[3], wxy[2];
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
            /* ★★★★ 2026-10-01 晚（owner：「**它是对称的两个使魔应当是**」）——**位置来源并列**（前 8 次）：
             *   `live` = `data+0x4e58 + i*0xc`（活相对偏移，字节步长 `0xc`；一手 `[edi+esi*4+0x4e58]`
             *   里 `esi` 是**浮点下标** ⇒ x 的字节偏移正是 `i*0xc`）；`props` = `props[i]+0x08/0x0c/0x10`
             *   （**绝对世界坐标**）；`world_from_live` = `epos + live`；`world_from_props` = props 原样。
             *   ⇒ 若 props 两槽不同而 live 相同 ⇒ **live 读法/下标错** ⇒ 位置改用 props（**不再叠加**）；
             *     若两边都相同 ⇒ 再核下标/步长。*/
            if (s_possrc_logged < 64u && yk_slot_type(e, i) && !yk_possrc_seen(id, i)) {
                const float *rl = (const float *)(e + YK_EMIT_OFFSETS + (size_t)i * YK_EMIT_VEC_STRIDE);
                const uint8_t *pp = e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE;
                float px = *(const float *)(pp + 0x08), py = *(const float *)(pp + 0x0c);
                ++s_possrc_logged;
                ce_log("youkai/emit POSSRC: enemy_id %u slot %d | live=(%.2f,%.2f,%.2f) | "
                       "props08/0c/10=(%.2f,%.2f,%.2f) | enemy_pos=(%.2f,%.2f) | world_from_live=(%.2f,%.2f) | "
                       "world_from_props=(%.2f,%.2f)",
                       id, i, (double)rl[0], (double)rl[1], (double)rl[2],
                       (double)px, (double)py, (double)*(const float *)(pp + 0x10),
                       (double)epos[0], (double)epos[1],
                       (double)(epos[0] + rl[0]), (double)(epos[1] + rl[1]), (double)px, (double)py);
            }
            int active = yk_slot_type(e, i);
            /* 不活跃、又不是我们认识的槽 ⇒ 连表都不用碰（扫得快、条目也少）。*/
            if (!active && !yk_map_find(&s_map, id, i)) continue;
            /* ★★★★ 2026-10-01 晚（owner：「**你使魔建在了敌人的正下方一个位置**，但是**是本体发弹**」）
             *   ——**坐标口径订正（一手 `th18:0x433f9e`）**：`props[i]+0x08/0x0c/0x10` 是**世界坐标**
             *   （`lea ecx,[edi+0x44]` → `call 0x413280` 取敌人位置 → `addss …, [edi+esi*4+0x4e58]`
             *    加相对偏移 → `movq [esi+0x5a0]` / `mov [esi+0x5a8]` = `props+0x08/0x0c/0x10`）。*/
            /* ★★★★★ 2026-10-11（**纯静态核实**后的位置定案；全文 `HANDOFF-108-EMITTER-STATIC-VERIFY.md`）：
             *   一手 `th18:0x433f34`（`etOn`）只有两条写发弹点的路，且两条都能**每帧重算**：
             *     · 相对路（`origins[i].z <= 0.9f`，判据 `th18:0x433f52`）= `敌人位置 + offsets[i]`（`th18:0x433fb4`）；
             *     · 绝对路（`origins[i].z > 0.9f`）        = `origins[i].xy + offsets[i].xy`（`th18:0x433f6d/0x433f76`）。
             *   ⛔ **不许再把 `props[i]+0x08/0x0c` 当世界坐标回落**：那三格是**两义共用**的一格 ——
             *     `etOn` 时是弹出生点，`laserNew`（`th18:0x434b5b/0x434b6d/0x434b7f`）时是**激光的长度三元组**
             *     （当前长/目标长/上限，消费者 `th18:0x449438`/`0x44a676`/`0x44a6f7`，**根本不是坐标**），
             *     且 `etCopy`（`th18:0x433ee1`）会整块拷贝进别的槽 ⇒ 拿它当位置 = 壳冻在旧快照 / 落在激光参数上。
             *   ★ 缓存（latch）仍然必要（同帧 `etOffset→etOn→etOffset(0)` 那一族，138 组），但它现在**带模式**：
             *     绝对模式存的是**世界坐标**，相对模式存的是**相对量**（见站点 `yk_on_shot_slot`）。
             *   ★ 没有缓存时的回落 = **`yk_emit_world_pos()`**（`youkai_core.c:363`，带单测；语义与引擎**逐条同形**：
             *     `org3[2] > 0.9` ⇒ `origins + offsets`，否则 `敌人位置 + offsets`）—— 上一版把它**停用**了，
             *     理由是「`data+0x4e58` 恒 0」；**那条观察已证伪**（`etOffsetRad` `th18:0x434107/0x434118`
             *     就写这个数组、`etOn` 相对路 `th18:0x433fb4` 就加它）⇒ 现在恢复使用。*/
            {   float lo[3], lof[3], orgf[3];
                int lo_abs = 0;
                s_cur_have_latch = yk_emit_latch_get(id, i, lo, &lo_abs) ? 1 : 0;
                if (s_cur_have_latch && !(lo[0] == 0.0f && lo[1] == 0.0f)) {
                    /* 定格值 = 站点 `0x433ffa` 抄回的「上一发 `etOn` 用的点」，**带模式** ⇔
                     * 绝对槽存世界坐标、相对槽存相对量（纯逻辑在 `youkai_core.c`，带单测）。*/
                    yk_emit_latch_reconstruct(epos, lo, lo_abs, wxy);
                    s_cur_pos_src = lo_abs ? "latch-abs" : "latch-rel";
                } else {
                    /* 没缓存（从没 `etOn` 过 / 被 32 格环挤出 / 中途装上卡）⇒ **按引擎的闭式重算**：
                     * 这正是「此刻若 `etOn(i)` 引擎会算出的点」，不是陈旧的 props 快照。
                     * 同一个闭式也是 `laserOn`（`th18:0x435104`–`0x435196`）算激光起点用的 ⇒ 激光槽同点。*/
                    __builtin_memcpy(lof, e + YK_EMIT_OFFSETS + (size_t)i * YK_EMIT_VEC_STRIDE, sizeof(float) * 3);
                    __builtin_memcpy(orgf, e + YK_EMIT_ORIGINS + (size_t)i * YK_EMIT_VEC_STRIDE, sizeof(float) * 3);
                    yk_emit_world_pos(epos, lof, orgf, wxy);
                    s_cur_pos_src = (orgf[2] > YK_ORIGIN_ABS_THRESHOLD) ? "live-abs" : "live-rel";
                }
                off3[0] = wxy[0]; off3[1] = wxy[1]; off3[2] = 0.0f;   /* 仅供调试打印 */
            }
            /* ★★★★ 2026-10-11（owner：那一堆**发射口使魔**冻住不跟母机动）——**20 帧连续 TRACK**：
             *   选一个「有壳」的 (敌人,槽)，连续 20 帧把 6 个量并列打出来，一次判死三种可能。*/
            {
                static float s_trk_px, s_trk_py; static uint32_t s_trk_pf;
                float mvx = epos[0] - s_trk_px, mvy = epos[1] - s_trk_py;
                if (ce_sdk_frame() != s_trk_pf) { s_trk_pf = ce_sdk_frame(); s_trk_px = epos[0]; s_trk_py = epos[1]; }
                s_trk_moving = ((mvx < 0 ? -mvx : mvx) + (mvy < 0 ? -mvy : mvy)) > 0.5f ? 1 : 0;
            }
            if (s_trk_left > 0 && s_trk_enemy == id && s_trk_slot == i && s_trk_moving) {
                uint8_t *tf = yk_familiar_by_id(0);   /* 占位避免未用告警 */
                uint32_t fid = 0;
                const float *fc = 0;
                yk_slot_t *tse = yk_map_find(&s_map, id, i);
                (void)tf;
                if (tse && tse->fam_id) { fid = tse->fam_id; fc = yk_familiar_by_id(fid) ? ce_enemy_pos(yk_familiar_by_id(fid)) : 0; }
                const uint8_t *ppx = e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE;
                ce_log("youkai/emit TRACK: frame=%u enemy_id %u slot %d | enemy_pos=(%.1f,%.1f) | "
                       "props08/0c=(%.1f,%.1f) | src=%s | target=(%.1f,%.1f) | "
                       "fam=%u fam_cur=(%.1f,%.1f) | pinned_this_frame=%d",
                       (unsigned)ce_sdk_frame(), id, i, (double)epos[0], (double)epos[1],
                       (double)*(const float *)(ppx + 0x08), (double)*(const float *)(ppx + 0x0c),
                       s_cur_pos_src, (double)wxy[0], (double)wxy[1],
                       fid, fc ? (double)fc[0] : 0.0, fc ? (double)fc[1] : 0.0,
                       (s_pin_last_fam == fid && s_pin_last_frame == ce_sdk_frame()) ? 1 : 0);
                ce_log("youkai/emit TRACK2: src=%s target=(%.1f,%.1f) pin_in=(%.1f,%.1f) frame=%u",
                       s_cur_pos_src, (double)wxy[0], (double)wxy[1],
                       (double)s_pin_last_x, (double)s_pin_last_y, (unsigned)ce_sdk_frame());
                --s_trk_left;
            } else if (s_trk_left == 0 && s_trk_slot < 0) {
                yk_slot_t *te = yk_map_find(&s_map, id, i);
                if (active && te && te->fam_id) {                    /* ★ 只锁 fam != 0 的真壳 */
                    s_trk_enemy = id; s_trk_slot = i; s_trk_left = 20;
                } else if (s_trk_noshell_logged < 1u) {
                    ++s_trk_noshell_logged;
                    ce_log("youkai/emit TRACK: (no shell yet) - waiting for a slot with fam_id != 0");
                }
            }
            /* ★ phantom 修复：把「站点上最近真开过火」翻成表里的 `fired` 位（`yk_note_slot` 用它决定建壳）。
             *   `type != 0` 只说明**被配置过**；配了却整波不发弹的槽（`count=0` / 被 `etDelay` 压制 /
             *   只在某分支 `etOn`）⇒ **不建壳**（owner 报的「不是发弹点的使魔」）。*/
            /* ★ 粘性口径（见上面 `yk_fired_sticky_*` 的注）：开过火就记住，只在槽不活跃时清。*/
            /* ★ 2026-10-01 晚（`bullets` 恒 0 ⇒ 我们读错字段）：**BM 原始 dword 探针**（前 8 帧），
             *   逐格打出来，让"真正在涨的那个计数"一眼可见（下一版用它换掉 `0x7d0`）。*/
            if (s_bmprobe_logged < 8u) {
                uint8_t *bm0 = CE_BULLET_MGR();
                if (bm0) {
                    unsigned q;
                    ++s_bmprobe_logged;
                    for (q = 0; q + 4 <= 0x30u; q += 4)
                        ce_log("youkai/emit BMPROBE: +0x%03x = %d (0x%08x)", 0x7d0 + q,
                               *(const int32_t *)(bm0 + 0x7d0 + q),
                               (unsigned)*(const uint32_t *)(bm0 + 0x7d0 + q));
                }
            }
            /* ★ 敌弹总数（帧间增长 = 「这一帧真的产了弹」）*/
            {   int32_t bc = 0;   /* ⛔ 订正（engine.h:757/758）：0x7d0 是弹数组**格数**不是活跃弹数 ⇒ 读它恒 0（produced 永假的真因）；真值要扫 bm+0xec 起的数组 */
                s_cur_bc_grew = (bc > s_prev_bc) ? 1 : 0;
                s_cur_bc = bc;
            }
            if (active && s_cur_bc_grew && yk_emit_fired_recently(id, i)) yk_prod_set(id, i);
            if (active) yk_map_set_fired(&s_map, id, i,
                                         yk_fired_sticky_get(id, i) || yk_emit_fired_recently(id, i));
            else { yk_fired_sticky_clear(id, i); yk_prod_clear(id, i); }
            /* ★ 2026-10-01 晚（owner：「两张壳 `emitter_world` 完全相同 ⇒ 叠在同一格」）——**原始偏移**：
             *   直接读 `data+0x4e58 + i*0xc`（活相对偏移，步长 `0xc` = float2 + float），
             *   用来判「ECL 真给同位置」还是「我们的下标/步长读错」。每个 (敌人,槽) 一次。*/
            if (s_raw_logged < 64u && yk_slot_type(e, i) && !yk_raw_seen(id, i)) {
                int k;
                ++s_raw_logged;
                for (k = 0; k < 4; ++k) {
                    const float *rr = (const float *)(e + YK_EMIT_OFFSETS + (size_t)k * YK_EMIT_VEC_STRIDE);
                    ce_log("youkai/emit RAW: enemy_id %u slot %d => off=(%.3f,%.3f,%.3f)", id, k,
                           (double)rr[0], (double)rr[1], (double)rr[2]);
                }
            }
            /* ★★★ 2026-10-01 晚（owner：5 面「一直有一个发弹点没建使魔」）——**槽级状态**（每槽一行，cap 64）：
             *   `SLOTSTATE: enemy_id N slot i active=… fired=… sticky=… have_fam_id=… fam_seen=… latch=… => shell=…`
             *   ⇒ 一眼看出某个槽是**没被看见 active** / **fired|sticky 没置上** / **被别处跳过**（含 ring 挤出 ⇒ `latch=0`）。*/
            if (s_slotstate_logged < 64u && active && !yk_slotstate_seen(id, i)) {
                yk_slot_t *e2 = yk_map_find(&s_map, id, i);
                ++s_slotstate_logged;
                ce_log("youkai/emit SLOTSTATE: enemy_id %u slot %d active=%d fired=%d sticky=%d "
                       "have_fam_id=%d fam_seen=%d latch=%d => shell=%d",
                       id, i, active, (int)(yk_fired_sticky_get(id, i) || yk_emit_fired_recently(id, i)),
                       (int)yk_fired_sticky_get(id, i), (e2 && e2->fam_id) ? 1 : 0,
                       e2 ? (int)e2->fam_seen : -1, (int)s_cur_have_latch,
                       (e2 && e2->fam_id) ? 1 : 0);
            }
            /* ★★★ 2026-09-26（owner #37，两次实跑同一条：「**发射口与敌人重合不应该建立使魔**」/
             *   「依旧能看见敌人的身上有魔法阵」）：**体上炮口不建使魔**。
             * ★★★★★ 2026-10-11（**静态核实**后发现这里原来是坏代码，已删）：
             *   旧写法 `yk_emit_on_body(off3, org3)` ①`off3` 那时已被赋成**世界发弹点**（不是相对偏移）、
             *   ②`org3` **从未赋值**（只有 `(void)org3;`）⇒ 绝对模式那一支读的是栈上野值（UB），
             *   而「相对偏移恰好 (0,0)」这个**真判据从没被求值过**。
             *   一手（`HANDOFF-108-EMITTER-STATIC-VERIFY.md`）：`etOn` 相对路 `props = 敌人位置 + offsets[i]`
             *   ⇒ `offsets == (0,0) ⇔ 发弹点 == 敌人位置`，而下面那道 `YK_EMIT_SHELL_MIN_DIST`（48 px）距离闸
             *   算的正是「目标点与母机的距离」⇒ **体上炮口 = 距离 0 < 48 已被它拒掉**（计数 `s_emit_bodyport`、
             *   日志 `is a BODY PORT`）。两处判据重复只会漂移，所以这里只留距离闸那一处。
             *   ⚠ 与 TH08 的关系：原作**从不**把发射口翻译成使魔（`etOn` handler `th08:0x41b883` 只有
             *     两条指令 + 跳尾；全 exe 5 处 `create_enemy` 没一处在 `et*` 家族里），使魔只由 ECL 的
             *     `familiarCreate*` 显式产生，而「体上发射口」在原作是**常态**（`ecldata8` 的 `Sub14`
             *     整族一只使魔都不建）⇒ 这条跳过是**复刻原作的默认行为**，不是我们发明的门。
             *   `yk_emit_on_body()` 本身（`youkai_core.c`，带单测）保留为**纯逻辑复刻**，不再从 tick 调。*/
            /* ★★★ 2026-10-01（owner 实跑：一面道中 boss 出现「不是发弹点的使魔」）——**诊断**：
             *   配了但本波**从没 `etOn`** 的槽 ⇒ 不建壳（`yk_note_slot` 的 `fired` 门）。
             *   第一条判据日志给出 `(enemy_id, slot)`、`type`、`color`、`count_1/2`（`props+0x14/+0x18`；
             *   ❓ 偏移来自 `etNew` 默认 `+0x14=0`，见 `engine/enemy/th18/07`），
             *   用来区分「配了没发」/「激光槽」/「`etCopy` 的槽」。同一 `(敌人,槽)` 只打一次。*/
            /* ★★★★ 2026-10-01（owner 硬口径：「一面到关底之前一点使魔不该有」）：
             *   **只有「发射点离本体 ≥ `YK_EMIT_SHELL_MIN_DIST`」的槽才算 TH08 式独立使魔**；
             *   本体口（距离 ≈ 0，小怪身上那种）⇒ **不建壳**。**只读**（硬约束不变）。*/
            /* ★ 坐标审计（owner：「先给数据、别猜」）：敌人位置 / `props+0x08/0x0c/0x10` 原样 /
             *   我们算出的发射点 / 壳将要 spawn 的位置 —— 四个量并列（前 8 个槽各一次）。*/
            /* ★ POST（前 24 次）：位置来源 + 定格年龄 + 计数 + 帧 + 敌弹总数 ⇒ 判「那里是不是发弹口」。*/
            if (active && s_post_logged < 24u && !yk_emit_coord_seen(id, i)) {
                const uint8_t *pp2 = e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE;
                ++s_post_logged;
                ce_log("youkai/emit POST: enemy_id %u slot %d src=%s latch_age=%u count_1=%d count_2=%d "
                       "fired_age=%u bullets=%d produced=%d pos=(%.1f,%.1f)",
                       id, i, s_cur_pos_src,
                       (unsigned)yk_emit_fired_age(id, i),
                       /* ★ 真计数 = `props+0x46c/+0x46e` 两个 i16（一手 `th18:0x42789e`/`0x4278b4`）*/
                       (int)*(const int16_t *)(pp2 + 0x46c), (int)*(const int16_t *)(pp2 + 0x46e),
                       (unsigned)yk_emit_fired_age(id, i), (int)s_cur_bc, (int)yk_prod_get(id, i),
                       (double)wxy[0], (double)wxy[1]);
            }
            if (active && s_coord_logged < 8u && !yk_emit_coord_seen(id, i)) {
                const uint8_t *pr1 = e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE;
                ++s_coord_logged;
                ce_log("youkai/emit COORD: enemy %u slot %d | enemy_pos=(%.1f,%.1f) | "
                       "props08/0c/10=(%.1f,%.1f,%.1f) | emitter_world=(%.1f,%.1f) spawn=(%.1f,%.1f) | "
                       "d2=%.1f min=%.0f | active=%d",
                       id, i, (double)epos[0], (double)epos[1],
                       (double)*(const float *)(pr1 + 0x08), (double)*(const float *)(pr1 + 0x0c),
                       (double)*(const float *)(pr1 + 0x10), (double)wxy[0], (double)wxy[1],
                       (double)wxy[0], (double)wxy[1],
                       (double)((wxy[0] - epos[0]) * (wxy[0] - epos[0]) + (wxy[1] - epos[1]) * (wxy[1] - epos[1])),
                       (double)YK_EMIT_SHELL_MIN_DIST, active);
            }
            /* ★ B（owner 推断：`type=4/7/8` 可能根本**不是弹口**）：类型白名单旋钮。
             *   `YK_EMIT_SHELL_TYPES` = 允许建壳的 `props+0x00` 值（**0 = 全接受 = 现状**；
             *   要「只对弹口建壳」时把它设成经一手确认的那几个值）。**语义表尚未一手确认** ⇒
             *   本批**不猜**：先靠 `COORD`/`COORD-SPAWN`/台账把 `type` 分布打出来，再填这张表。*/
            /* ★★★★★ 2026-10-01 晚（**owner 拍板：临时黑名单**，不是白名单）：
             *   只拒掉**观察到确定不对**的 `type ∈ {4,7,8}`（见 `YK_EMIT_SHELL_TYPE_DENY` 的注）；
             *   不用白名单的理由：`type` 语义还没挖到，白名单可能**误杀**「该有」的场景
             *   （4 面大妖精 / 米斯蒂亚 / 5–6 面中 boss 非符）。**0 = 关闭**（回退现状）。*/
            if (active) {
                int ty = *(const int32_t *)((const uint8_t *)e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE);
                if (yk_emit_type_denied(ty, YK_EMIT_SHELL_TYPE_DENY)) {
                    if (!yk_emit_reject_seen(id, i)) {
                        ++s_emit_badtype;
                        ce_log("youkai/emit: REJECT(bad-type) enemy_id %u slot %d type=%d "
                               "(deny mask 0x%x) => NO familiar (owner 2026-10-01: type 4/7/8 are the "
                               "'odd shells' observed; temporary blacklist, 0 = off)",
                               id, i, ty, (unsigned)YK_EMIT_SHELL_TYPE_DENY);
                    }
                    ++i; continue;
                }
            }
            /* ⛔ 2026-10-01 晚（**owner 已否掉**）：「大怪/尺寸白名单」那道闸**已撤** ——
             *   owner：「**不是只有大怪才有可能有发射口使魔；6 面道中的妖精也有发射口使魔**」。
             *   ⇒ 判据类改动**先出 SAMPLE 数据 + owner 指认**，再写闸（见上面的 SAMPLE 行）。*/
            /* ★★★★ 2026-10-01 晚（owner：**不是只有大怪才有发射口使魔，6 面道中的妖精也有**）：
             *   ⇒ **撤掉**「大怪/尺寸白名单」那道闸（判据类改动**先出 SAMPLE 数据 + owner 指认**，再写闸）。
             *   本行 = **数据收集版**：对每个活跃槽打**全量字段**（前 `YK_SAMPLE_LOG_N` 行），
             *   让 owner 一眼指认「该有 / 不该有」，再据此推判据（当前最可疑 = `slot type`，
             *   但**先不猜**）。`is_body_port` = 距离 < `YK_EMIT_SHELL_MIN_DIST`。*/
            if (active && s_sample_logged < YK_SAMPLE_LOG_N) {
                int w = 0, h = 0, spid = yk_sprite_id_for(e, id);
                int bodyport;
                (void)yk_enemy_sprite_size(e, &w, &h);
                bodyport = !yk_emit_far_enough(wxy[0] - epos[0], wxy[1] - epos[1], YK_EMIT_SHELL_MIN_DIST);
                ++s_sample_logged;
                ce_log("youkai/emit SAMPLE: stage=%d enemy_id=%u sprite=%d w=%d h=%d slot=%d type=%d color=%d "
                       "count_1=%d count_2=%d dist2=%.1f is_body_port=%d fired_age=%u life=%d "
                       "enemy_pos=(%.1f,%.1f) emitter_world=(%.1f,%.1f) src=%s => %s",
                       (int)CE_CURRENT_STAGE(), id, spid, w, h, i,
                       *(const int32_t *)((const uint8_t *)e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE + 0x00),
                       *(const int32_t *)((const uint8_t *)e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE + 0x04),
                       /* ★ 真计数（两个 i16）在 `props+0x46c/+0x46e`，不是 `+0x14/+0x18`（2026-10-11 订正）*/
                       (int)*(const int16_t *)(e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE + 0x46c),
                       (int)*(const int16_t *)(e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE + 0x46e),
                       (double)((wxy[0] - epos[0]) * (wxy[0] - epos[0]) + (wxy[1] - epos[1]) * (wxy[1] - epos[1])),
                       bodyport, (unsigned)yk_emit_fired_age(id, i),
                       (int)*(const int32_t *)(e + YK_ED_LIFE_CUR),
                       (double)epos[0], (double)epos[1], (double)wxy[0], (double)wxy[1],
                       s_cur_pos_src,
                       bodyport ? "REJECT(off-body)" : "CREATE");
            }
            /* ★★★★★ 2026-10-01 晚（owner：「**一瞬间**出现的、在**版顶正中间**的使魔」）：
             *   TH18 场地**居中**（`x=0` 版中、`y=0` 版顶）⇒「版顶正中间」= **(0,0)**。
             *   一手证据（实跑 log）：`COORD: enemy 26 slot 0 | enemy_pos=(-93.8,148.9) |
             *   props08/0c/10=(0.0,0.0,0.0) | emitter_world=(0.0,0.0) spawn=(0.0,0.0)` —— 就是我们
             *   **在 `etOn` 还没写出点（= 0）时就建了壳**。
             *   ⇒ 守卫（**只读**，两条都要满足才建）：
             *     ① **本帧/近几帧有定格值**（`fired_recently` ⇒ 真 `etOn` 过）
             *     ② **出点不是 (0,0)**（= props 已写出点）
             *   ⇒ 不满足者 `REJECT(no-origin)`（台账 `noorigin=N`），**不再用活偏移兜底去建壳**。*/
            if (active && wxy[0] == 0.0f && wxy[1] == 0.0f) {
                if (!yk_emit_reject_seen(id, i)) {
                    ++s_emit_noorigin;
                    ce_log("youkai/emit: REJECT(no-origin) enemy_id %u slot %d have_latch=%d "
                           "props08/0c=(%.1f,%.1f) => no shell this frame (owner: no (0,0) 'top-centre' "
                           "flicker; we wait for the real etOn origin)",
                           id, i, (int)s_cur_have_latch, (double)wxy[0], (double)wxy[1]);
                }
                ++i; continue;
            }
            /* ★ 建壳前提：**该槽真的出过弹** —— ⚠ **观测未证实前默认关闭**（owner：`bullets` 恒 0 是
             *   我们读错字段 ⇒ 把壳全拒了）。`YK_EMIT_REQUIRE_BULLETS = 1` 才启用。*/
            if (YK_EMIT_REQUIRE_BULLETS && active && !yk_prod_get(id, i)) {
                if (!yk_emit_reject_seen(id, i)) {
                    ++s_emit_nobullet;
                    ce_log("youkai/emit: REJECT(no-bullets) enemy_id %u slot %d (etOn seen but the enemy "
                           "bullet count never grew for this slot) => NO familiar (owner: that spot is "
                           "not a fire point at all)", id, i);
                }
                ++i; continue;
            }
            /* ★ 2026-10-11（owner）：**距离分布自适应观测** —— 每 60 帧一行，看有没有「真口落在 48–64」
             *   被误拒（有 ⇒ 按那一个场景加白名单条目，或把阈值下调到 gap 内）。按**被检槽**计数。*/
            {   float ddx = wxy[0] - epos[0], ddy = wxy[1] - epos[1];
                float dd2 = ddx * ddx + ddy * ddy;
                if (dd2 < 48.0f * 48.0f) ++s_dh_body;
                else if (dd2 < 64.0f * 64.0f) {
                    const uint8_t *ppn = e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE;
                    ++s_dh_near;
                    if (s_dh_near_logged < 32u && !yk_emit_reject_seen(id, i)) {
                        ++s_dh_near_logged;
                        ce_log("youkai/emit NEARBAND: enemy_id %u slot %d dist2=%.1f (dist=%.1f) props08/0c=(%.1f,%.1f) "
                               "enemy_pos=(%.1f,%.1f) => rejected by 48px gate (possible false reject)",
                               id, i, (double)dd2, (double)yk_emit_len(ddx, ddy),
                               (double)*(const float *)(ppn + 0x08), (double)*(const float *)(ppn + 0x0c),
                               (double)epos[0], (double)epos[1]);
                    }
                }
                else ++s_dh_far;
                if ((ce_sdk_frame() % 60u) == 0u && s_dh_logged < 64u) {
                    ++s_dh_logged;
                    ce_log("youkai/emit DISTHIST: stage=%d bodies(<48)=%u near(48..64)=%u far(>=64)=%u "
                           "(min_dist=%.0f)", (int)CE_CURRENT_STAGE(), s_dh_body, s_dh_near, s_dh_far,
                           (double)YK_EMIT_SHELL_MIN_DIST);
                    s_dh_body = s_dh_near = s_dh_far = 0u;
                }
            }
            if (active && !s_cur_have_latch) {
                /* ★ 诊断（2026-10-11 静态核实后改写）：**无定格值**的槽走 `live-*`（按引擎闭式每帧重算，
                 *   见上面位置块的注）—— 那**不再等于「冻结」**（旧版读 props 快照才会冻）。这里只计数，
                 *   用来判「定格环（32 格）到底有没有被挤出」。*/
                ++s_dh_nolatch;
                if ((ce_sdk_frame() % 60u) == 0u && s_dh_nolatch_logged < 32u) {
                    ++s_dh_nolatch_logged;
                    ce_log("youkai/emit NOLATCH: enemy_id %u slot %d -> live recompute (%s) count=%u "
                           "(no latch entry: evicted from the 32-slot ring / never fired since load)",
                           id, i, s_cur_pos_src, s_dh_nolatch);
                }
            }
            if (active && !yk_emit_far_enough(wxy[0] - epos[0], wxy[1] - epos[1], YK_EMIT_SHELL_MIN_DIST)) {
                /* ★ 2026-10-11 静态核实顺手修：这里原来是**双层同函数调用**
                 *   （`if (!seen()) { if (!seen()) ++counter; log }`）⇒ 第一次调用已把条目写进表，
                 *   第二次必然返回 1 ⇒ `s_emit_bodyport` **永远不涨**（日志倒还是每 (敌人,槽) 一行）。
                 *   现在去重一次、计数与日志都只做一遍。*/
                if (!yk_emit_bodyport_seen(id, i)) {
                    ++s_emit_bodyport;   /* ★ 按 (敌人,槽) 去重 */
                    ce_log("youkai/emit: slot %d of enemy_id %u is a BODY PORT (distance %.1f px < %.0f) "
                           "=> NO familiar (owner 2026-10-01: nothing before the stage boss; TH08 counts "
                           "only `familiarCreate*` entities as familiars)",
                           i, id, (double)yk_emit_len(wxy[0] - epos[0], wxy[1] - epos[1]),
                           (double)YK_EMIT_SHELL_MIN_DIST);
                }
                /* ★ 「体上炮口」= 发弹点与母机**重合**（`etOn` 相对路里 `offsets == (0,0)`）。
                 *   它与上面那道 48 px 闸不是一回事（闸更宽）⇒ 单独喂台账那一格（`#37` 口径）。*/
                if (wxy[0] == epos[0] && wxy[1] == epos[1] && s->emit_body_skipped < 0xffffu)
                    ++s->emit_body_skipped;
                ++i; continue;
            }
            if (active && !yk_emit_fired_recently(id, i) && !yk_fired_sticky_get(id, i) &&
                !yk_emit_phantom_seen(id, i)) {
                const uint8_t *pr0 = e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE;
                ++s_emit_phantom;
                ce_log("youkai/emit: slot %d of enemy_id %u is CONFIGURED but has NOT fired (type=%d "
                       "color=%d count_1=%d count_2=%d) => NO familiar (this is owner's "
                       "'not-a-fire-point' ghost; shell is created only after a real etOn)",
                       i, id, *(const int32_t *)(pr0 + 0x00), *(const int32_t *)(pr0 + 0x04),
                       (int)*(const int16_t *)(pr0 + 0x46c), (int)*(const int16_t *)(pr0 + 0x46e));
            }
            if (yk_note_slot(&s_map, id, i, active, wxy, &act)) yk_run_action(s, &act);
            yk_emitkey_note(id, i, active, act.fam_id, s_ledger_frame);   /* ★ 诊断：序号稳不稳 */
        }
        /* ★★★★★ 2026-10-11（**多原点 · 第 2..N 次真发**）——同一 `(敌人,槽)` 同帧的第 2..N 次
         *   `etOn`。上面那个 `for (i…)` 只认「第 1 次」（键 = 裸槽号）⇒ 这一段补上其余档：
         *   每档一条**独立条目**（键 = `slot + 16*ord`），因此
         *     · 亮出各自出点的使魔（`st05/06mbs` 的 ±90° 两侧、`st03` 的 5 个 72° 出点）；
         *     · 打死哪一只只关**那一个出点**（站点按 `key` 裁决，见 `yk_on_shot_slot`）。
         *
         *   生命周期与第 1 次那一支**逐条同口径**（绝不是新发明）：
         *     · 判「这一档开过火」= **粘性** `yk_fired_sticky_get(key)`（站点上真看到第 2..N 次 `etOn`
         *       时置位）或近 3 帧的定格值（`yk_emit_fired_recently`，窗口 `YK_EMIT_FIRED_WINDOW`）；
         *     · 槽不活跃（`props[i].type == 0`）⇒ 清粘性 + 撤壳（与第 1 次同一处置）；
         *     · 位置优先用**这一档自己的**定格值（相对模式重建 = `敌人位置(每帧) + 该出点偏移` ⇒
         *       跟着母机动）；定格值被挤出环时才按引擎闭式用**真实槽**的活字段重算（与第 1 次同）；
         *     · 三道门同口径：① 槽活跃、② 出点不是 (0,0)、③ 出点离母机 ≥ `YK_EMIT_SHELL_MIN_DIST`
         *       （48 px；`st07 GirlC01_at_360` 两次 `etOn` 都落在本体上、`offsets` 全 0 ⇒ 正确地
         *       一只都不建）。
         *
         *   ★ 为什么不能只认「近 3 帧」：`st03 GirlD01_at` 一波 5 发、**波间 `wait(120)`**
         *     ⇒ 只认近帧会把壳在波内第 4 帧就撤掉、下一波再建（抖 + 计数膨胀）。
         *   ★ 单次发弹的槽（绝大多数）在这里**什么都不做**：键 `slot+16` 从没被站点写过
         *     ⇒ `yk_emit_latch_get` 命中不了、粘性也没置 ⇒ 旧场景逐帧行为不变。
         *   ★ 同点共壳：`st03` 的 5 条键各在不同出点（5 只）；`st06bs Boss1_984` 的 4 条键
         *     全在同一点 ⇒ `YK_EMIT_SHARE_SAME_POS` 收成 **1 只**；`st05/06/07mbs` 的 slot 0/1
         *     两点两两重合 ⇒ **每侧 1 只（共 2 只）**，而不是 4 只两两重叠。
         *   ⚠ 本段的诊断额度独立（`s_mo_logged`），**不动**第 1 次那一支的任何计数口径。*/
        for (i = 0; i < YK_EMIT_SLOTS; ++i) {
            int o;
            for (o = 1; o < YK_EMIT_ORIGIN_MAX; ++o) {
                int oi = YK_EMIT_ORIGIN_KEY(i, o);
                int oreal = yk_slot_type(e, i);
                yk_slot_t *ex = yk_map_find(&s_map, id, oi);
                float lo2[3], w2[2];
                int abs2 = 0, have2 = 0, fired2 = 0;
                if (!oreal) {
                    /* 槽不活跃（`etNew` 清零 type）= 主复位口之一：清粘性 + 撤壳（与第 1 次同一处置）。*/
                    if (ex && (ex->fam_id || ex->fam_seen)) {
                        yk_map_set_fired(&s_map, id, oi, 0);
                        yk_fired_sticky_clear(id, oi);
                        yk_prod_clear(id, oi);
                        if (yk_note_slot(&s_map, id, oi, 0, 0, &act)) yk_run_action(s, &act);
                    }
                    continue;
                }
                have2 = yk_emit_latch_get(id, oi, lo2, &abs2);
                /* 「这一档开过火」判据与第 1 次同源，再加一条**条目自身的存在**当兜底：
                 *   粘性环只有 64 格、会被本局早先的 (敌人,键) 占满 ⇒ 只认 `sticky` 会让
                 *   5–7 面那些**晚出现**的多原点槽每波撤一次壳（抖）。条目还在 + 定格值还在 =
                 *   这一档确实开过火（定格值环 64 格，`st03` 那种 `wait(120)` 也留得住）。*/
                fired2 = yk_fired_sticky_get(id, oi) || yk_emit_fired_recently(id, oi) ||
                         (have2 && ex != 0);
                if (!fired2) {
                    /* 从没真发过：有旧条目 ⇒ 撤壳。从不曾建过壳、也没有夹持记忆的条目不置 `seen`
                     *   ⇒ 让帧末 `yk_frame_end` 直接退休它（否则多原点会给曾多发一帧的槽永久占格）。*/
                    if (ex && (ex->fam_id || ex->fam_seen)) {
                        yk_map_set_fired(&s_map, id, oi, 0);
                        yk_fired_sticky_clear(id, oi);
                        yk_prod_clear(id, oi);
                        if (yk_note_slot(&s_map, id, oi, 0, 0, &act)) yk_run_action(s, &act);
                    }
                    continue;
                }
                if (have2) {
                    yk_emit_latch_reconstruct(epos, lo2, abs2, w2);
                } else {
                    /* 定格值被挤出环（64 格）⇒ 与第 1 次同一闭式：用**真实槽**的活字段现算。*/
                    float lof[3], orgf[3];
                    __builtin_memcpy(lof, e + YK_EMIT_OFFSETS + (size_t)i * YK_EMIT_VEC_STRIDE, sizeof(float) * 3);
                    __builtin_memcpy(orgf, e + YK_EMIT_ORIGINS + (size_t)i * YK_EMIT_VEC_STRIDE, sizeof(float) * 3);
                    yk_emit_world_pos(epos, lof, orgf, w2);
                }
                if (w2[0] == 0.0f && w2[1] == 0.0f) continue;      /* 门 ②：出点还没写出来 */
                if (!yk_emit_far_enough(w2[0] - epos[0], w2[1] - epos[1], YK_EMIT_SHELL_MIN_DIST)) {
                    if (s_mo_logged < 16u) {
                        ++s_mo_logged;
                        ce_log("youkai/emit MULTI(origin %d): enemy_id %u slot %d key %d at (%.1f,%.1f) is a "
                               "BODY PORT (dist %.1f px < %.0f) => no familiar for this origin",
                               o, id, i, oi, (double)w2[0], (double)w2[1],
                               (double)yk_emit_len(w2[0] - epos[0], w2[1] - epos[1]),
                               (double)YK_EMIT_SHELL_MIN_DIST);
                    }
                    continue;
                }
                {   int ty = *(const int32_t *)((const uint8_t *)e + YK_EMIT_PROPS + (size_t)i * YK_EMIT_PROP_STRIDE);
                    if (yk_emit_type_denied(ty, YK_EMIT_SHELL_TYPE_DENY)) continue;
                }
                if (s_mo_logged < 16u && ex == 0) {
                    ++s_mo_logged;
                    ce_log("youkai/emit MULTI: enemy_id %u slot %d origin %d => key %d at (%.1f,%.1f) "
                           "(same-frame fire #%d; its own shell, its own veto)",
                           id, i, o, oi, (double)w2[0], (double)w2[1], o + 1);
                }
                yk_map_set_fired(&s_map, id, oi, 1);
                yk_fired_sticky_set(id, oi);
                if (yk_note_slot(&s_map, id, oi, 1, w2, &act)) yk_run_action(s, &act);
                yk_emitkey_note(id, i, 1, act.fam_id, s_ledger_frame);
            }
        }
    }
    s_prev_bc = s_cur_bc;   /* ★ 帧间比较基准（敌弹总数）*/
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
static uint32_t s_ledger_frame;   /* 前向声明（定义在后面；这里只用它做「每 2 帧」的节拍）*/

/* ★★★ 2026-09-30（owner：「**至少得再加一个 50% 血量转伤吧**」，路线 A 的**覆盖面补齐**）：
 *   现有转嫁只喂 `s_map`（我们自己建的**映射/发射口**使魔）⇒ 缺口 = ①**绕转使魔**（`s_orbit[]`，
 *   不进 `s_map`）②**外部子敌人**（leveledit/Mystia 那批 `MystiaChild*`：普通敌机 + `parent_enemy_id`
 *   = 母机）。两者都是「使魔」，一手 **1/2 转嫁**对它们同样成立。
 *   本表 = 「外部/绕转使魔的上帧 `life.current`」采样（enemy_id 键，**单调递增不复用**）：
 *     · 上限 `YK_XFAM_MAX`(32)；**表满只放弃这一帧的「新」条目**（宁可不转、不要乱转）；
 *     · 条目查不到（子敌/父敌消失）⇒ 删；**不因「血量没变」删**；
 *     · 喂进**同一个** `s->tr` 累加器（1/2 比例在 `yk_transfer_feed` 里），落点仍是母机（路线 A）。
 *   ⚠ 自激守卫：我们打在母机上的源若擦到子敌人，会让它掉血又被当成新伤害转一次
 *     ⇒ 照 `youkai.c` 既有的 `yk_in_transfer_zone(...)` 做法把「落在转嫁盒里」的条目**只采样不喂**。*/
#define YK_XFAM_MAX 32
/* ★★★ 2026-10-18（owner：「**1/2 转伤搞不清楚，你得上 log**」+「使魔切换还得有音效」）：
 *   `le` = 这一条是**接管的 leveledit 使魔**（`$I3 == 997`）⇒ 只有它们走妖侧外观切换 + 详细日志。
 *   绕转使魔也进这张表（见下面那条支），但它们的人妖外观由 `s_orbit[]` 自己那套管 ⇒ `le = 0`。
 *   日志节流字段就放这里（表只有 32 格，多开一张平行表不值）。*/
typedef struct {
    uint32_t id;
    int32_t  life;
    uint8_t  seen;
    uint8_t  le;               /* 1 = leveledit 接管来的外部使魔 */
    uint16_t _pad;
    uint32_t last_log_frame;   /* 上一次 hit 日志的帧（节流 `YK_XFAM_LOG_EVERY`）*/
    uint16_t log_hits;         /* 这只使魔已经打过几条 hit 日志（上限 `YK_XFAM_LOG_MAX`）*/
    uint16_t _pad2;
} yk_xfam_t;
static yk_xfam_t s_xfam[YK_XFAM_MAX];
static unsigned  s_xfam_fed;      /* 本局喂过多少次外部/绕转使魔（日志用）*/
/* 日志节流（owner：日志已 8 MB+ ⇒ 别刷爆）：
 *   · `YK_XFAM_LOG_EVERY` = **每只使魔每 30 帧最多一条** hit / side 日志；
 *   · `YK_XFAM_LOG_MAX`   = 每只使魔整局的 hit 日志上限（一局长 boss 战 ≈ 16 × 64 行 ≈ 140 KB，可接受）；
 *   · `YK_XFAM_SIDE_LOG_MAX` = 切侧日志上限（玩家狂按 Shift 也不会刷屏）。*/
#define YK_XFAM_LOG_EVERY    30u
#define YK_XFAM_LOG_MAX      64u
#define YK_XFAM_SIDE_LOG_MAX 32u
/* ★★★ 2026-10-01（owner：「体感 1/2 伤害转移没有生效…敌人本身没死」+ 日志 `0 transfer damage`）：
 *   **转嫁链路的逐段诊断**（只加计数，行为零变化）—— 把「喂料 → 累加器 → 落点」每一段都数出来：
 *   `s_xfer_mapped_feed/_delta`（映射使魔那一支）、`s_xfer_orbit_feed/_delta`（绕转）、
 *   `s_xfer_ext_feed/_delta`（外部子敌人 = leveledit 那批）、`s_xfer_rect`（**真的调了几次
 *   `ce_damage_rect`**）、`s_xfer_noboss`（因为**没有 boss** 而把转嫁量丢进 `dropped` 的次数）。*/
static unsigned s_xfer_mapped_feed, s_xfer_orbit_feed, s_xfer_ext_feed;
static int32_t  s_xfer_mapped_delta, s_xfer_orbit_delta, s_xfer_ext_delta;
static int32_t  s_xfer_ext_amount;   /* ★ 2026-10-18：本 tick **外部使魔**真正加进 pending 的量（日志用）*/
static int32_t  s_xfer_ext_le_amount;/* ★ 同上，但只算**接管的 leveledit 使魔**那一部分
                                      *   —— `transfer applied` 那行只用它做门，免得在普通关卡
                                      *   （boss 的普通子敌人也走 ext 支）里刷出 `leveledit:` 日志 */
static unsigned s_xfer_rect, s_xfer_rect_fail, s_xfer_noboss, s_xfer_noparent;
/* ★★★ 2026-10-01（owner 实跑：`xfer[tick 5706 map 0/0 orb 538/11391 ext 0/0 rect 0 … noboss 275]`
 *   ⇒ **11391 点伤害全被 `noboss` 丢掉**）—— **落点改成「这只使魔自己的母机（parent）」**：
 *   绕转/映射/外部三条支在喂料时把该使魔的 **`CE_SPAWN_PARENT`（母机 id）** 记到这里，
 *   转嫁就打在**那台母机**身上（一手语义 = 「打使魔 ⇒ 转给它的主人」，**不再要求它在 boss 表里**）；
 *   母机查不到时才退回原来的全局 boss，再不行才 `noboss` 丢。*/
static uint32_t s_xfer_parent;
static unsigned s_xfer_tick_calls;   /* ★ `yk_transfer_tick` 本局被调了几次（判断这一支有没有在跑）*/
/* ★★★ 2026-10-19（owner 实跑：01:26 那局 `youkai/leveledit:` **仍然 0 行**，而 01:15 的
 *   `le-probe(enemy)` 已经证明 `$I3 == 997` 读得到 ✓）—— **无条件 MARK 诊断**的节流表。
 *   语义：`s_le_mark_id[k] != 0` ⇒ 这一格记的是哪个敌机 id；同一 id 每 60 帧最多打一条 ✓。
 *   ⇒ 下一局一次分清三种失败（这正是本表存在的唯一理由，根因查清后整段删掉 ✓）：
 *     ① 完全没有 MARK 行 ⇒ 扫描没跑到，或那局场上根本没有 997 敌机；
 *     ② MARK 有、`le_boss=0` ⇒ boss 指针取不到（`yk_boss()` 的 lockable 门 + `CE_EM_BOSS_IDS`
 *        兜底都没成 ⇒ 接管门 `le_boss &&` 直接短路，一行日志都不会有 —— **静默失败**）；
 *     ③ MARK 有、`le_boss>0`、`parent=0` 却仍无 TAKEOVER ⇒ 是后面的门在挡
 *        （`in_map` / `in_orbit` 在 TAKEOVER 日志**之前** `continue` ✓）。*/
static uint32_t s_le_mark_id[16];
static uint32_t s_le_mark_frame[16];
static unsigned  s_xfam_used;     /* 本帧表用了几格（日志用）*/

/* 这个 id 是否已在映射层（`s_map`）—— 那边已经在喂转嫁，别重复计。*/
static int yk_map_has_id(const yk_map_t *m, uint32_t id)
{
    unsigned i;
    if (!m || !id) return 0;
    for (i = 0; i < YK_FAM_MAX; ++i) if (m->e[i].fam_id == id) return 1;
    return 0;
}

/* 这个 id 是不是我们的绕转使魔（`s_orbit[]`，不进 `s_map`）—— 那条支单独喂。*/
static int yk_orbit_find_id(uint32_t id)
{
    unsigned i;
    if (!id) return 0;
    for (i = 0; i < YK_ORBIT_MAX; ++i) if (s_orbit[i].fam_id == id) return 1;
    return 0;
}

/* 母机是不是「boss 系」：① id 在管理器那 4 格 `CE_EM_BOSS_IDS` 里；**或** ② 母机自己的
 * `CE_ENEMY_OWN_BOSS_ID != 0`（覆盖道中 boss）。两者取或 —— 一手转嫁的判据是「有主」，
 * 我们再加「主是 boss」只是为了不把**普通子敌人**（例如关卡时间轴建的普通妖精）也算进来。*/
static int yk_parent_is_boss(uint8_t *em, uint8_t *pe)
{
    int32_t pid;
    unsigned i;
    if (!em || !pe) return 0;
    pid = *(const int32_t *)(pe + CE_ENEMY_ID);
    if (!pid) return 0;
    for (i = 0; i < 4u; ++i)
        if (*(const int32_t *)(em + CE_EM_BOSS_IDS + (size_t)i * 4u) == pid) return 1;
    return *(const int32_t *)(pe + CE_ENEMY_OWN_BOSS_ID) != 0;
}

static yk_xfam_t *yk_xfam_find(uint32_t id)
{
    unsigned i;
    for (i = 0; i < YK_XFAM_MAX; ++i) if (s_xfam[i].id == id) return &s_xfam[i];
    return 0;
}

/* ======================= §3.7b 外部使魔（leveledit）的**妖侧外观** =======================
 * ★★★ 2026-10-18 owner 口径：「**除了人侧外观，其他都得跟我们的使魔一致**」。
 *
 * 我们**自建**使魔的切侧（`yk_fam_vm_follow()` / `yk_orbit_vm_sync()`，同一套口径）：
 *   · **人侧**：画本体（跟随 VM，四色原生切图）；
 *   · **妖侧**：本体**不可见** = `ce_anm_set_color(id, 0x00ffffff)`（alpha 置 0，VM 不删 ⇒
 *     切回来一帧就恢复）+ 一支**常驻魔法阵环**（脚本 `CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING`
 *     = 309，layer `YK_FAM_ANM_LAYER` = 7，`interrupt 2` = 妖侧蓝）。
 *
 * 对外部敌机要**等价**地做，唯一的差别是：它们的本体 VM 不是我们起的，句柄在
 * `zEnemyData.anm_vm_ids[0]` = **`zEnemy + CE_ENEMY_DATA(0x122c) + 0x124` = `zEnemy + 0x1350`**
 * （= `CE_ENEMY_ANM_VM0` ✓；一手读法同 117 `cards/animal.c` 的 `animal_enemy_sprite_size()` /
 * `ufo.c` 换皮的「每帧写宿主 `anm_vm_ids[k]` 的 color_1」✓）。
 *
 * ⚠ **安全兜底（做错会让妖侧使魔整批消失）**：拿不到本体 VM 句柄（还没建 / 引擎刚撤）⇒
 *   **什么都不做**（不写颜色、不起环、不留半状态），只按节流打一行 WARN ⇒ 下一帧重试。
 * ⚠ **绝不改它们的 ECL / 碰撞盒 / 血量**：这里只碰 `color_1`（可见性）与我们自己起的环 VM。
 * ⚠ 删卡 / 换关 / 那批使魔消失 ⇒ `yk_xfam_vm_kill()` 把本体原色还回去（不留隐形的敌人）。*/
typedef struct {
    uint32_t fam_id;       /* 0 = 空槽 */
    uint32_t body_vm;      /* 本体 VM **id**（`anm_vm_ids[0]`）—— 0 = 还没拿到 */
    uint32_t body_color;   /* 本体 `color_1` 原值（只存一次；人侧 / 收尾还回去）*/
    uint32_t body_flags;   /* 本体 `flags_lo`(+0x534) 原值（2026-10-19c：藏本体时清 visible bit1，还原时按原值还）*/
    uint32_t ring_vm;      /* 我们自己起的常驻环 VM（只在妖侧存在）*/
    uint32_t last_side_log;
    uint32_t last_warn;
    uint16_t side_logs;
    uint16_t warn_n;
    int8_t   side;         /* 上一次同步的侧：0 人 / 1 妖 / -1 = 还没同步过 */
    int8_t   body_saved;   /* 1 = `body_color` 有效 */
    int8_t   body_hidden;  /* 1 = 本体现在被我们置了 alpha 0（人侧只还原**一次**，别每帧钉色）*/
} yk_xfam_vm_t;
static yk_xfam_vm_t s_xfam_vm[YK_XFAM_MAX];
static int8_t s_xfam_side = -1;      /* 上一帧的全局侧（切换音只在**边沿**响一次）*/
/* ★ 2026-10-19c：`s_xfam_ring_fail` 随「接管使魔不再画我们的环」一起删掉（owner 口径：只藏本体）*/
static unsigned s_xfam_vm_full;      /* 外观表满的次数（= 台账满；日志只打第一次）*/

static yk_xfam_vm_t *yk_xfam_vm_find(uint32_t fam_id)
{
    unsigned i;
    if (!fam_id) return 0;
    for (i = 0; i < YK_XFAM_MAX; ++i) if (s_xfam_vm[i].fam_id == fam_id) return &s_xfam_vm[i];
    return 0;
}

/* 收一条：本体原色还回去（若句柄还有效）+ 环发 `interrupt 3` 淡出（脚本自删，我们不等它）。
 * 幂等；删卡 / 换关 / 那只使魔消失三条路都过这里。*/
static void yk_xfam_vm_kill(yk_xfam_vm_t *v)
{
    if (!v) return;
    if (v->body_vm && v->body_saved && v->body_hidden)
        ce_anm_set_color(v->body_vm, v->body_color);
    if (v->ring_vm) { ce_anm_interrupt(v->ring_vm, 3); v->ring_vm = 0; }
    __builtin_memset(v, 0, sizeof *v);
    v->side = -1;
}

static void yk_xfam_vm_kill_id(uint32_t fam_id) { yk_xfam_vm_kill(yk_xfam_vm_find(fam_id)); }

static void yk_xfam_vm_kill_all(void)
{
    unsigned i;
    for (i = 0; i < YK_XFAM_MAX; ++i) if (s_xfam_vm[i].fam_id) yk_xfam_vm_kill(&s_xfam_vm[i]);
}

/* ★★★ 2026-10-19g（owner：「他使魔血量太低了，得修改到我们 108 的水平」）：
 *   他们 ECL `enmCreate(..., life, 0, 0)` 给的 life 是 **TH08 原值**（实跑 `CENSUS-DET` 实测 = **20**），
 *   而本卡自建使魔的规则 = `yk_map_fam_hp(母机最大 HP, 机体)`（母机 HP/3 × 机体千分比；见 `youkai_core.h` §2.1ccc-30）。
 *   他们**自己改不上去**：ECL 的 life 实参 ≥ 1000 会被 `EnemyManager__allocate_new_enemy` 置
 *   「大怪」位 `0x40000000`（不可撤销）—— 他们 2026-10-18 就是因此被卡在 999（他们的 notes.md 记着）。
 *   ⇒ 反过来由**我们**在接管那一刻**回写三格 life**（`enemy+0x6220/6224/6228`）：这条路**不置位** ✓
 *     （本卡自建使魔正是这么做的，见 `yk_spawn_familiar_parent()` 的「999 出生 + 事后回写」）。
 *   ⚠ 只在「**上限还不够**」时写一次（`max < want`）：否则每次掉血都重写会把伤害差分吞掉；
 *     写后调用方重读 `life.current` 再进台账 ✓。
 *   ⚠ 母机 HP 读不到（≤0）⇒ `yk_map_fam_hp()` 自己会落到 100 档，仍比 20 厚 ✓。*/
static unsigned s_le_hp_logged;
/* ★ 2026-10-19h（owner：「TH08 不可能是这种一碰就碎的血量吧」）—— **倍数旋钮**（🟡 owner 给数就一行改）：
 *   1000 = 1.0×（= 本卡自建使魔的规则：母机最大 HP/3 × 机体千分比）；
 *   ⚠ 这不是「复刻 TH08 的数字」而是「按 TH18 的伤害尺度重定」：TH08 的 `familiarCreate` 实参里
 *     本来就有 600/100/50/20 四档（他们 `notes.md` 记着），20 在 TH08 的伤害尺度下不是秒杀，
 *     但 TH18 自机弹伤害高一个量级 ⇒ 同一个 20 到 TH18 就是一碰就碎 ⇒ 必须重定，不能照抄。
 *   登记 `TUNING.md` §2.0zz。*/
#define YK_LE_FAM_HP_SCALE 1000

static void yk_le_fam_raise_hp(uint8_t *f, const uint8_t *boss)
{
    int32_t boss_hp, want, cur_max;
    if (!f || !boss) return;
    boss_hp = *(const int32_t *)(boss + CE_ENEMY_HP_MAX);
    want = (int32_t)yk_map_fam_hp((int)boss_hp, yk_current_character());
    if (want <= 0) want = (int32_t)YK_FAM_LIFE;
    want = want * YK_LE_FAM_HP_SCALE / 1000;            /* ★ 旋钮（默认 1.0×）*/
    if (want < 1) want = 1;
    cur_max = *(const int32_t *)(f + YK_ED_LIFE_CUR + 4);
    if (cur_max >= want) return;                    /* 已经够厚 ⇒ 一个字节都不写 ✓ */
    *(int32_t *)(f + YK_ED_LIFE_CUR)     = want;    /* current   */
    *(int32_t *)(f + YK_ED_LIFE_CUR + 4) = want;    /* maximum   */
    *(int32_t *)(f + YK_ED_LIFE_CUR + 8) = want;    /* remaining */
    if (s_le_hp_logged < 8u) {
        ++s_le_hp_logged;
        ce_log("youkai/leveledit: HP raised id=%u %d -> %d (our rule: boss_hp %d / 3 x char permille)",
               (unsigned)*(const uint32_t *)(f + CE_ENEMY_ID), (int)cur_max, (int)want, (int)boss_hp);
    }
}

/* ★★★ 2026-10-19e（owner 一眼定位：「我知道了，是**拖尾光**没关掉」）：
 *   他们的使魔**本体脚本**每 2 tick 用 `scriptNewRoot(script30)` 撒一颗拖尾火花
 *   （一手 `assets.py:132-142`：`script script28/29 { layer(7); loop: sprite(...); +2: scriptNewRoot(script30); … jmp(loop,0); }`；
 *     script30 = 加法 16×16、20 tick 自删、layer 7）。`scriptNewRoot` = **独立 root VM**
 *   （一手 `AnmLoaded__instantiate_orphan_vm_to_world_list_back 0x488a40`，正是 `engine/anm/th18/01` §3 那条）
 *   ⇒ **不在 `anm_vm_ids[]` 里** ⇒ 藏本体（slot 0）藏不到它，它照样每 2 tick 冒新的、20 tick 才自己消。
 *   ⇒ 妖侧藏本体时，**顺手把这颗 VM 删掉**（它本来就是短命粒子，删它不影响任何人；他们的 ECL 也不引用它）。
 *   定位靠 **VM 列表**（一手 `AnmManager__get_vm_with_id 0x488b40` 的走法：
 *     `mgr+0x6f0` = world 表头 / `mgr+0x6f8` = ui 表头；结点 `{+0 = VM*, +4 = next}`；
 *      VM 的 id 在 `+0x544`（`CE_ANM_VM_SUFFIX_ID`）✓）+ 两个过滤器：
 *     · `VM+0x28`（当前脚本号，`CE_ANM_VM_SCRIPT_ID`）== **30** —— 就是他们那颗火花；
 *     · 实体坐标（`VM+0x5f0`，`CE_ANM_VM_POS`）离这只使魔 < `YK_LE_TRAIL_R`(32 px)。
 *   ⇒ 只删「贴在这只使魔身上 + script 30」的 VM，别的一律不碰 ✓；整局最多 8 行日志。*/
#define YK_LE_TRAIL_SCRIPT 30
#define YK_LE_TRAIL_R      32.0f
static unsigned s_le_trail_killed;

static void yk_xfam_kill_trails(const float *pp)
{
    uint8_t *mgr = CE_ANM_MANAGER();
    int li;
    if (!mgr || !pp) return;
    for (li = 0; li < 2; ++li) {
        uint8_t *node = *(uint8_t **)(mgr + (li ? 0x6f8u : 0x6f0u));
        unsigned guard = 0;
        while (node && guard++ < 4096u) {
            uint8_t *vm = *(uint8_t **)node;
            uint8_t *next = *(uint8_t **)(node + 4);   /* ★ 先取 next：删除会把本结点从链表摘掉 */
            if (vm && *(const int32_t *)(vm + CE_ANM_VM_SCRIPT_ID) == YK_LE_TRAIL_SCRIPT) {
                const float *vp = (const float *)(vm + CE_ANM_VM_POS);
                const float dx = vp[0] - pp[0], dy = vp[1] - pp[1];
                if (dx * dx + dy * dy < YK_LE_TRAIL_R * YK_LE_TRAIL_R) {
                    const uint32_t vid = *(const uint32_t *)(vm + CE_ANM_VM_SUFFIX_ID);
                    if (vid) {
                        if (s_le_trail_killed < 8u) {
                            ++s_le_trail_killed;
                            ce_log("youkai/leveledit: trail VM deleted id=%u (script %d within %.0f px) "
                                   "- owner: 拖尾光没关掉",
                                   vid, YK_LE_TRAIL_SCRIPT, (double)YK_LE_TRAIL_R);
                        }
                        ce_anm_delete(vid);
                    }
                }
            }
            node = next;
        }
    }
}

/* 每帧：把**接管的 leveledit 使魔**按当前侧切外观（人：本体可见；妖：本体 alpha=0 + 环）。
 * 调用点 = `yk_transfer_tick()` 最前面（那一支每帧都跑，且不在任何 `return` 之后）。
 * `noinline` 的理由同 `yk_fam_vm_check()`：别把活值塞进内联着 `ce_damage_rect` 汇编的函数。*/
static void __attribute__((noinline)) yk_xfam_vm_tick(void)
{
    unsigned i, live = 0;
    int side = yk_focused() ? 1 : 0;

    for (i = 0; i < YK_XFAM_MAX; ++i) {
        yk_xfam_t *x = &s_xfam[i];
        yk_xfam_vm_t *v;
        uint8_t *f, *vm;
        uint32_t vid;
        const float *pp;

        if (!x->id || !x->le) continue;                    /* 只看接管的 leveledit 使魔 */
        f = yk_familiar_by_id(x->id);
        if (!f) continue;                                  /* 实体没了 ⇒ 台账那一支会删条目 → kill */
        ++live;

        v = yk_xfam_vm_find(x->id);
        if (!v) {
            unsigned k;
            for (k = 0; k < YK_XFAM_MAX; ++k) if (!s_xfam_vm[k].fam_id) break;
            if (k >= YK_XFAM_MAX) {                        /* 表满（= 台账满）⇒ 本帧跳过，下次再试 */
                if (s_xfam_vm_full++ == 0u)
                    ce_log("youkai/leveledit: WARN appearance table FULL (%d) - external familiars keep "
                           "their human-side look (no half state)", (int)YK_XFAM_MAX);
                continue;
            }
            v = &s_xfam_vm[k];
            __builtin_memset(v, 0, sizeof *v);
            v->fam_id = x->id;
            v->side = -1;
        }

        pp = ce_enemy_pos(f);
        vid = *(const uint32_t *)(f + CE_ENEMY_ANM_VM0);   /* `zEnemyData.anm_vm_ids[0]` ✓ */
        vm = vid ? (uint8_t *)ce_anm_get_vm(vid) : 0;
        if (!vm || IsBadReadPtr((const void *)(vm + CE_ANM_VM_COLOR1), 4)) {
            /* ★ 兜底：拿不到本体 VM ⇒ **什么都不做**（只记一句），绝不留半状态。*/
            v->body_vm = 0; v->body_color = 0; v->body_saved = 0; v->body_hidden = 0;
            if (v->warn_n < 3u && (v->last_warn == 0u || s_ledger_frame - v->last_warn >= 300u)) {
                v->last_warn = s_ledger_frame;
                ++v->warn_n;
                ce_log("youkai/leveledit: WARN familiar id=%u has no usable body anm VM "
                       "(anm_vm_ids[0]=%u) - side switch SKIPPED this frame, nothing touched",
                       x->id, vid);
            }
            continue;
        }

        if (vid != v->body_vm) {                           /* 第一次 / 引擎重建了本体 VM */
            v->body_vm = vid;
            v->body_color = *(const uint32_t *)(vm + CE_ANM_VM_COLOR1);
            v->body_flags = *(const uint32_t *)(vm + CE_ANM_VM_FLAGS_LO);
            v->body_saved = 1;
            v->body_hidden = 0;                            /* 新 VM 还没被我们藏过 */
        }

        if (side) {
            /* 妖侧：**只藏本体，不画我们的魔法阵** —— ★ owner 2026-10-19 口径逐字：
             *   「那就只隐藏本体，不花（画）我们的魔法阵」。
             *   一手（他们仓库 `familiar_visuals.py:overlay_ecl`）：他们的使魔在
             *   `TSub<child>` 里挂了**两格**——`anmSetSprite(0, 28/29)` 本体 + `anmSetSprite(1, 31)`
             *   魔法阵（script31 = TH08 etama script48 case2，tex `mystia/fam-circle.png` 62×62）⇒
             *   **他们自带魔法阵**，我们再起环就是两层 ⇒ 这里**不再 spawn 环**（`ring_vm` 字段只为
             *   兼容旧状态保留，恒 0）。
             *   藏本体的两手（原来只有 ①，实跑「他的 VM 没消失」⇒ 补 ②，两只一起上）：
             *     ① `color_1` 的 alpha 置 0（`ce_anm_set_color`，照抄自建那条路径的写法）；
             *     ② 清 `flags_lo`（`+0x534`）**bit1 = visible**（绘制前 `0x481256` 读它）——
             *        他们的本体脚本带 `colorTime/alphaTime`，只写颜色可能被脚本每帧写回。
             *   ⚠ 两样都**只写本体的那格 VM**（`anm_vm_ids[0]`），**不碰** slot 1（他们的魔法阵）✓。*/
            ce_anm_set_color(vid, 0x00ffffffu);
            *(uint32_t *)(vm + CE_ANM_VM_FLAGS_LO) &= ~2u;      /* visible = 0 */
            v->body_hidden = 1;
            /* ★ 2026-10-19e：他们的**拖尾火花**是独立 root VM（`scriptNewRoot(script30)`），
             *   藏本体藏不到 ⇒ 这里把贴在这只使魔身上的那颗删掉（见 `yk_xfam_kill_trails()` 的一手）。*/
            yk_xfam_kill_trails(pp);
        } else {
            /* 人侧：本体原样可见（颜色 + visible 位都还回去）。
             * ⚠ 原色**只在从妖侧切回来那一次**写回（`body_hidden` 门）—— 每帧写会把他们的
             *   anm 脚本自己的受击闪白 / 淡入淡出钉死在这个快照色上。
             *   visible 位同理：只按**存下来的原值**恢复（原值 bit1=0 就别硬点亮）。*/
            if (v->body_saved && v->body_hidden) {
                ce_anm_set_color(vid, v->body_color);
                if (v->body_flags & 2u)
                    *(uint32_t *)(vm + CE_ANM_VM_FLAGS_LO) |= 2u;
                v->body_hidden = 0;
            }
            if (v->ring_vm) { ce_anm_interrupt(v->ring_vm, 3); v->ring_vm = 0; }
        }

        if (v->side != (int8_t)side) {
            if (v->side >= 0 &&
                v->side_logs < YK_XFAM_SIDE_LOG_MAX &&
                (v->last_side_log == 0u || s_ledger_frame - v->last_side_log >= YK_XFAM_LOG_EVERY)) {
                v->last_side_log = s_ledger_frame;
                ++v->side_logs;
                ce_log("youkai/leveledit: side -> %s (fam id=%u)", side ? "YOUKAI" : "HUMAN", x->id);
            }
            v->side = (int8_t)side;
        }
    }

    /* ★ 切换音：**只在全局侧的边沿**、且场上真的有接管的使魔时响**一次**（跟自建使魔同一枚
     * `YOUKAI_FAM_SHIFT` = 扩表 0x5d；资源 `assets/voice/YOUKAI_FAM_SHIFT.wav`）。
     * 两个方向都响（owner 2026-10-18：「使魔切换还得有音效」）；自建那条路（`se_fam_press`）
     * 一字未改 ⇒ 两个方向的差异不影响现有卡的行为。*/
    if (s_xfam_side >= 0 && side != (int)s_xfam_side && live)
        ce_play_voice(YOUKAI_FAM_SHIFT, ce_player_x());
    s_xfam_side = (int8_t)side;
}

/* ★★★ 2026-10-18（owner：「**1/2 转伤搞不清楚**」）：**每一次命中的可读日志**。
 *   格式逐字（owner 给的）：
 *     `youkai/leveledit: xfam hit id=<fam> dmg=<实际打出的伤害> -> boss id=<boss> transfer=<转给 boss 的伤害> (1/2)`
 *   `dmg` = 这只使魔这一笔**真实掉的血**（`life.current` 差分；扫描每 2 帧 ⇒ 是这两帧的合计）；
 *   `transfer` = 真正加进累加器 `s->tr.pending` 的量（`yk_half_with_carry()` 的半整数进位，
 *     不是简单的 `dmg/2` ⇒ 老实读 `pending` 的前后差）。
 *   节流：每只使魔每 `YK_XFAM_LOG_EVERY`(30) 帧最多一条、整局最多 `YK_XFAM_LOG_MAX`(64) 条。
 *   `in_box` = 落在转嫁盒里 ⇒ 自激守卫**只采样不喂**（日志里标出来，免得 owner 以为吞了）。*/
static void __attribute__((noinline)) yk_xfam_hit_log(uint32_t fam_id, int32_t dmg,
                                                      uint32_t boss_id, int32_t fed, int in_box)
{
    yk_xfam_t *x = yk_xfam_find(fam_id);
    if (!x || !x->le) return;
    if (x->log_hits >= YK_XFAM_LOG_MAX) return;
    if (x->last_log_frame && s_ledger_frame - x->last_log_frame < YK_XFAM_LOG_EVERY) return;
    x->last_log_frame = s_ledger_frame;
    ++x->log_hits;
    ce_log("youkai/leveledit: xfam hit id=%u dmg=%d -> boss id=%u transfer=%d (1/2)%s",
           fam_id, dmg, boss_id, fed,
           in_box ? " [in transfer box: sampled only, NOT fed]" : "");
}

/* ★★★ 2026-10-18（owner：「1/2 转伤搞不清楚」）：**转伤落地**的收尾日志。
 *   `ce_damage_rect` 建的是**伤害源**（敌人侧 `enm_compute_damage_sources` 0x45f0f0 下一次结算，
 *   `+0x74` = 每次结算的伤害）⇒ **建源那一帧读目标 HP 是读不到变化的** ⇒
 *   这里记下 (目标 id, 建源前 HP, 那一笔 amount)，**下一个 tick** 再读一次 HP 打一行：
 *     `youkai/leveledit: transfer applied boss=<id> hp_before=<x> hp_after=<y> …`
 *   `hp_after - hp_before` 里也含同一帧自机弹的伤害 ⇒ 它是「这段窗口里 boss 掉了多少」，
 *   不是「只有我们这一笔」；把 amount / ext 一起打出来就能对账。
 *   只有「本帧有**接管的 leveledit 使魔**喂过料」才记（`s_xfer_ext_le_amount > 0`）
 *   ⇒ 不把自建那条路刷爆，也不在普通关卡（boss 的普通子敌也走 ext 支）里刷 `leveledit:` 行。
 *   HP 拿不到（目标已死/查不到）⇒ `hp_after=?`，只打转伤值（照 owner 口径）。*/
static uint32_t s_xfer_obs_id;
static int32_t  s_xfer_obs_amount, s_xfer_obs_ext, s_xfer_obs_hp;
static uint8_t  s_xfer_obs_wait;
static uint32_t s_xfer_obs_log_frame;
static unsigned s_xfer_obs_skipped;

static void __attribute__((noinline)) yk_xfer_applied_tick(void)
{
    uint8_t *te;
    int32_t hp_after;
    if (!s_xfer_obs_wait) return;
    s_xfer_obs_wait = 0;
    te = s_xfer_obs_id ? ce_enemy_by_id(CE_ENEMY_MGR(), (int32_t)s_xfer_obs_id) : 0;
    hp_after = te ? *(const int32_t *)(te + YK_ED_LIFE_CUR) : -1;
    if (s_xfer_obs_log_frame && s_ledger_frame - s_xfer_obs_log_frame < YK_XFAM_LOG_EVERY) {
        ++s_xfer_obs_skipped;                          /* 节流：< 30 帧就攒着，下一行带 skipped */
        return;
    }
    s_xfer_obs_log_frame = s_ledger_frame;
    if (hp_after >= 0)
        ce_log("youkai/leveledit: transfer applied boss=%u hp_before=%d hp_after=%d (hp_delta=%d; "
               "amount=%d, ext=%d from their familiars, 1/2; skipped %u under %u-frame throttle)",
               s_xfer_obs_id, (int)s_xfer_obs_hp, (int)hp_after, (int)(s_xfer_obs_hp - hp_after),
               (int)s_xfer_obs_amount, (int)s_xfer_obs_ext, s_xfer_obs_skipped, YK_XFAM_LOG_EVERY);
    else
        ce_log("youkai/leveledit: transfer applied boss=%u hp_before=%d hp_after=? (target gone; "
               "amount=%d, ext=%d from their familiars, 1/2; skipped %u)",
               s_xfer_obs_id, (int)s_xfer_obs_hp, (int)s_xfer_obs_amount, (int)s_xfer_obs_ext,
               s_xfer_obs_skipped);
    s_xfer_obs_skipped = 0;
}

/* ★★★★ 2026-10-01（**owner 硬约束 ②**）：**时符期间，使魔受到的伤害不得转嫁**。
 *   owner 逐字：「另外，**对于时符**，应该程序内可能有定义，**这个使魔的伤害肯定不能返回到敌人身上**」。
 *   判据（一手）：符卡进行中（`zSpellcard+0x78` bit0，`0x409b10`）+ 符卡名（`+0x34`，`Spellcard__start`
 *   `0x42a370`–`0x42a37b` 逐字节拷入）是「時符 / 时符」（`yk_spell_name_is_time()`，三种编码都认）。
 *   ⇒ 命中即**不转嫁**：量进 `dropped` 账、`transferred` 不涨；进入时打一行（含 `spell id` 与名字前
 *     8 字节的十六进制 ⇒ 实跑可反查真实编码）。⚠ 实测 TH18 的 `*.ecl` 里没有这些序列（0 命中）⇒
 *     若实跑从不触发，用那行十六进制补判据（登记 `AUDIT §BE #212`）。*/
#define YK_ENEMY_FLAG_NO_DAMAGE24 0x01000000u

/* ★★★★ 2026-10-01 晚（owner 拍板「**要的**」）：**抑制转嫁的统一判据**（返回值 = 位掩码，0 = 正常转嫁）。
 *   来源三种：① `zSpellcard+0x78` bit3（耐久符卡 = 超时不算 bonus failed，**优先**）
 *             ② 符卡名以「時符/时符」开头（兜底）③ 母机 `enemy+0x635c` bit24（无敌/免伤）。
 *   命中 ⇒ 整笔 pending 进 `dropped`（**不建** `ce_damage_rect`、**不抬**每敌每帧上限）。*/
static int yk_xfer_suppress_reason(void)
{
    uint8_t *sc = CE_SPELLCARD();
    uint8_t *boss = yk_boss();
    int flags = 0, name_hit = 0, invuln = 0;
    if (sc) {
        flags = *(const int32_t *)(sc + CE_SPELL_FLAGS);
        if (flags & CE_SPELL_FLAG_ACTIVE) name_hit = yk_spell_name_is_time((const uint8_t *)(sc + 0x34), 64);
        else flags = 0;                       /* 不在符卡里 ⇒ 只可能按无敌抑制 */
    }
    if (boss) {
        uint32_t fl = *(const uint32_t *)(boss + CE_ENEMY_FLAGS);
        invuln = (fl & YK_ENEMY_FLAG_NO_DAMAGE24) ? 1 : 0;
    }
    return yk_spell_xfer_block(flags, name_hit, invuln);
}

/* 判据行（**同一敌人连续抑制只打一次**；抑制解除后再进入会再打一次）。*/
static void yk_xfer_suppress_log(int reason)
{
    static uint32_t logged_enemy;
    static int      logged_reason;
    static int32_t  logged_spell = -1;
    uint8_t *sc = CE_SPELLCARD();
    uint8_t *boss = yk_boss();
    uint32_t eid = boss ? *(const uint32_t *)(boss + CE_ENEMY_ID) : 0u;
    int32_t  sid = sc ? *(const int32_t *)(sc + CE_SPELL_ID) : -1;
    int fl = sc ? *(const int32_t *)(sc + CE_SPELL_FLAGS) : 0;
    const uint8_t *nm = sc ? (const uint8_t *)(sc + 0x34) : (const uint8_t *)"";
    if (reason == 0) { logged_enemy = 0u; logged_reason = 0; logged_spell = -1; return; }
    if (eid == logged_enemy && reason == logged_reason && sid == logged_spell) return;   /* 不刷屏 */
    logged_enemy = eid; logged_reason = reason; logged_spell = sid;
    ce_log("youkai/xfer: spell %d => transfer SUPPRESSED (by=%s%s%s; flags=0x%x survival_bit=%d "
           "boss_no_damage_bit=%d; owner 2026-10-01: familiar damage must NOT go back to the enemy "
           "on time spells / while it is invulnerable) name[0..7]=%02x %02x %02x %02x %02x %02x %02x %02x",
           (int)sid,
           (reason & YK_XFER_BLOCK_FLAG) ? "flag" : "",
           (reason & YK_XFER_BLOCK_NAME) ? ((reason & YK_XFER_BLOCK_FLAG) ? "+name" : "name") : "",
           (reason & YK_XFER_BLOCK_INVULN) ? (((reason & (YK_XFER_BLOCK_FLAG | YK_XFER_BLOCK_NAME))) ? "+invuln" : "invuln") : "",
           (unsigned)fl, (fl & CE_SPELL_FLAG_SURVIVAL) ? 1 : 0,
           (reason & YK_XFER_BLOCK_INVULN) ? 1 : 0,
           nm[0], nm[1], nm[2], nm[3], nm[4], nm[5], nm[6], nm[7]);
}

static void __attribute__((noinline)) yk_transfer_tick(yk_card_t *s)
{
    ++s_xfer_tick_calls;                   /* ★ 诊断：这一支本局跑了几次 */
    /* ★ 2026-10-18（owner：转伤要可观测 + 妖侧外观要切换）：
     *   ① `yk_xfer_applied_tick()` 收上一 tick 那笔转伤的 HP 尾巴（**必须在所有 return 之前**）；
     *   ② `yk_xfam_vm_tick()` 每帧同步接管使魔的外观（同上 —— 不能放在本函数中段的早退之后）；
     *   ③ 本 tick 的「外部使魔喂料量」清零后重新累计。*/
    yk_xfer_applied_tick();
    yk_xfam_vm_tick();
    s_xfer_ext_amount = 0;
    s_xfer_ext_le_amount = 0;
    unsigned i;
    uint8_t *boss = yk_boss();                      /* 每帧只解析一次（链表走 4 个 id）*/
    const float *bp = boss ? ce_enemy_pos(boss) : 0;
    uint8_t *tgt = 0;                               /* 这一笔转伤的落点（母机 / boss 兜底）*/
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
                   "slot %d (key %d) of enemy_id %u (silent total %u); VM withdrawn too",
                   sl->fam_id, YK_EMIT_REAL_SLOT((int)sl->slot), (int)sl->slot, sl->enemy_id, s->silent + 1u);
            yk_fam_vm_kill_id(sl->fam_id);
            yk_fam_gone(&s_map, sl->fam_id);
            ++s->silent;
            continue;
        }
        /* ★ D2（owner 拍板）：**插值跟随**槽原点，不再硬钉 —— 槽偏移每 2/5 帧随机一次时壳是滑过去，
         *   不是瞬移。`YK_EMIT_FOLLOW_FRAMES = 0` 时 `yk_follow_step()` 退化成硬钉（旧行为，便于对照）。
         *   ⚠ 目标变化**不** CREATE/WITHDRAW 壳（身份 = 槽，不变）。*/
        yk_pin_follow_step(&s_pin_follow[i], sl->fam_id, sl->x, sl->y);
        yk_pin_familiar(f, s_pin_follow[i].cx, s_pin_follow[i].cy);
        /* ★ 每帧跟随 VM：位置与上面那次钉的是**同一个** `final_pos`（`CE_ENEMY_POS_X/Y`）
         * ⇒ VM 与实体永远重合（脚本是 8 帧循环、不自删；句柄自愈见 `yk_fam_vm_follow`）。*/
        yk_fam_vm_follow(sl->fam_id, yk_fam_mapped_kind((int)sl->slot), f);
        if (bp && yk_in_transfer_zone(sl->x, sl->y, bp[0], bp[1],
                                      YK_TRANSFER_BOX_W, YK_TRANSFER_BOX_H)) {
            /* 使魔就压在转嫁盒里（boss 自己的发射口常在本体上）⇒ 它这一帧掉的血可能是**我们自己的源**
             * 打的：计入转嫁会每帧自激（research §3②）。只更新采样、不喂转嫁。*/
            yk_sample_life(sl, sl->fam_id, *(int32_t *)(f + YK_ED_LIFE_CUR));
            continue;
        }
        {   int32_t d = yk_sample_life(sl, sl->fam_id, *(int32_t *)(f + YK_ED_LIFE_CUR));
            if (d > 0) {
                ++s_xfer_mapped_feed; s_xfer_mapped_delta += d;
                s_xfer_parent = sl->enemy_id;                /* ★ 落点 = 映射槽的母机 */
            }
            yk_transfer_feed(&s->tr, d, YK_TRANSFER_PCT_MAP); }   /* ★ P59：映射（发弹口）使魔 ⇒ 1/2 ✓ */
    }

    /* ===== ★ 第二遍：**绕转 + 外部子敌人 + leveledit 使魔**的同一套 1/2 转嫁 =====
     * （owner 2026-09-30 口径；★ 2026-10-18 B2 加「他们的使魔」那一类 ✓）
     * 每 2 帧扫一次（代价减半；一手的转嫁本身就是每帧结算，2 帧的量化误差 ≤1 帧掉血）。*/
    {
        uint8_t *em = CE_ENEMY_MGR();
        uint8_t *e;
        unsigned i;
        /* ★★ 2026-10-18（B2 接管）：**那场**判据只在扫描那一帧算一次 ✓（`yk_le_here()` = boss ECL
         *   `subroutine_index ≥ 24`，见上面那段）。没装 108 ⇒ 本 tick 根本不跑 ⇒ 恒 0 ✓。
         *   他们那批使魔（`MystiaChild*`）是 `enmCreate(..., 0, 0)` 建的 ⇒ `parent_enemy_id` = 0
         *   ⇒ 原来那条「外部子敌人」判定**够不到它们** ✗；接管 = 让它们进**同一张**外部使魔台账
         *   `s_xfam`（落点 = boss）⇒ 1/2 转伤走的就是现成那条 ext 支 ✓，不新建第二套台账 ✓。*/
        int      le_here = 0;
        uint8_t *le_boss = 0;
        if (em && (s_ledger_frame % 2u) == 0u) {
            le_here = yk_le_here();
            /* ★★★ 2026-10-19 实跑订正（owner 上机日志）：`iv3 == 997` 本身就是**不会误伤的一手判据** ✓
             *   实跑证据：他们的使魔 id=338/339/340/341… 全是 `iv3=997` ✓（111 行），
             *   boss 297 与其它敌机全是 `iv3=0` ✓ ⇒ 标记可靠 ✓。
             *   而 `subroutine_index ≥ 24` **只在他们后半段阶段成立** ✗ —— 第一管 Boss1 从 sub 0 起，
             *   整场 `youkai/leveledit:` 一行都没有 ✗ ⇒ 接管一次都没触发 ✓。
             *   ⇒ boss 指针**无条件**取（`yk_boss()` 很便宜 ✓），"那场"判据只留给「不建自建壳」那半 ✓。*/
            le_boss = yk_boss();
            /* ★★★ 2026-10-19 兜底（owner：「符卡练习有使魔啊！使魔是符卡创建的」✓）：
             *   `yk_boss()` 要求 boss `ce_enemy_lockable` ✗ —— **符卡练习**里这个条件可能不成立
             *   ⇒ `le_boss` 取不到 ⇒ 接管门 `le_boss &&` 直接失败 ✗（表现就是 0 行 `youkai/leveledit:`）。
             *   ⇒ 兜底：直接按 `CE_EM_BOSS_IDS` 的槽位找**非零 boss id** ✓（不要求 lockable ✓）*/
            if (!le_boss && em) {
                unsigned bi;
                for (bi = 0; bi < CE_EM_BOSS_SLOTS; ++bi) {
                    int32_t bid_fb = *(const int32_t *)(em + CE_EM_BOSS_IDS + bi * 4);
                    if (!bid_fb) continue;
                    le_boss = ce_enemy_by_id(em, (uint32_t)bid_fb);
                    if (le_boss) break;
                }
            }
            /* ★★★ 2026-10-19：**`CE_EM_BOSS_IDS` 槽位只读体检**（任务 A 第 5 步）——
             *   `le_boss` 是接管门的第一个合取项，它取不到就**一行日志都不会有**（静默 ✗）。
             *   符卡练习里 boss 槽位到底是不是空的，用这一行钉死；每 600 帧一条（几乎零成本 ✓）。
             *   读法 = 一手 `get_boss_enemy_full` `0x4237f0`：`em+0x48` 起 4 个 int32 boss id ✓。*/
            if ((s_ledger_frame % 600u) == 0u)
                ce_log("youkai/leveledit: CTX le_boss=%u le_here=%d boss_slots=%d,%d,%d,%d "
                       "(boss slot dump; all zero => yk_boss() has nothing to resolve in this mode)",
                       le_boss ? *(const uint32_t *)(le_boss + CE_ENEMY_ID) : 0u, le_here,
                       (int)*(const int32_t *)(em + CE_EM_BOSS_IDS + 0),
                       (int)*(const int32_t *)(em + CE_EM_BOSS_IDS + 4),
                       (int)*(const int32_t *)(em + CE_EM_BOSS_IDS + 8),
                       (int)*(const int32_t *)(em + CE_EM_BOSS_IDS + 12));
            /* ★ 2026-10-19c：「他们那场」用**粘性**判据（同一面确认过一次就成立）——
             *   他们的 boss 在 `Boss()` / `BossDead()` 那种 sub<24 的时刻 `le_here` 会掉回 0，
             *   而他们的使魔可以活过那些时刻 ⇒ 不能每帧只认当帧的 `le_here` ✓。
             *   `s_le_seen_stage` 记「哪一面确认过」，换面/新局自动失效（`CURRENT_STAGE` 变了就不算）✓。*/
            if (le_here) s_le_seen_stage = CE_CURRENT_STAGE();
            {
                const int le_stage = (le_here || (s_le_seen_stage == CE_CURRENT_STAGE())) ? 1 : 0;
            /* ★★★ 2026-10-19c（owner：「接管没有生效 / 人侧没有消失 / 切换没有音效」；
             *   02:09 那局日志里 **0 行 MARK** ⇒ 那一局场上一只 `$I3 == 997` 的敌机都没有）——
             *   **只读普查**（每 120 帧：1 行汇总 + ≤4 行明细）。它一次分清两种可能，且**顺带把我们
             *   真正该用的判据量出来**：
             *     · `slot28_29 > 0`（README 的两种使魔图像脚本 28/29）却 `iv997 == 0`
             *       ⇒ 使魔**在场上**、标记没落在我们读的那一格；
             *     · `slot28_29 == 0` 且 `iv997 == 0` ⇒ 那一局**根本没有他们的使魔**（符卡捷径 / 这张符不建子机）。
             *   ★ 明细里的 `sub=` = **该敌机当前 ECL `subroutine_index`**（读法同 `yk_le_boss_sub_index()`
             *     —— 那一套已经用来判「leveledit 那场」，是**实测可读**的 ✓）。他们的使魔是
             *     `MystiaChild<28|30|37|43|47|49|55|59>`（`translate.py:15-16` 的 `CHILDREN`），
             *     在编译后的 `st02bs.ecl` 里占一段**固定下标** ⇒ 拿到这一段就能**不依赖任何标记**
             *     认出他们的使魔（owner 口径：把驱动做成「认得他们那个包」的）。
             *   `life` 顺带区分「他们的使魔（666/999）」与普通妖精。*/
            if ((s_ledger_frame % 120u) == 0u) {
                int live = (int)*(const int32_t *)(em + CE_EM_ENEMY_COUNT);
                int scanned = 0, n997 = 0, n2829 = 0, npar = 0, det = 0;
                CE_ENEMY_FOREACH(em, e) {
                    const int32_t *iv;
                    int32_t par, sl0;
                    if (!e) continue;
                    ++scanned;
                    iv = (const int32_t *)(e + CE_ENEMY_DATA + 0x28c);
                    par = *(const int32_t *)(e + YK_ENEMY_PARENT_ID);
                    sl0 = *(const int32_t *)(e + CE_ENEMY_DATA + 0x26c);
                    if (iv[3] == YK_LE_FAM_MARK) ++n997;
                    if (sl0 == 28 || sl0 == 29) ++n2829;
                    if (par) ++npar;
                    if (det < 4 && (iv[3] == YK_LE_FAM_MARK || sl0 == 28 || sl0 == 29 || par)) {
                        ++det;
                        ce_log("youkai/le: CENSUS-DET id=%u sub=%d parent=%d life=%d slot0=%d "
                               "iv=%d,%d,%d,%d,%d lockable=%d",
                               (unsigned)*(const uint32_t *)(e + CE_ENEMY_ID),
                               yk_le_boss_sub_index(e), (int)par,
                               (int)*(const int32_t *)(e + YK_ED_LIFE_CUR), (int)sl0,
                               (int)iv[0], (int)iv[1], (int)iv[2], (int)iv[3], (int)iv[4],
                               ce_enemy_lockable(e) ? 1 : 0);
                    }
                }
                ce_log("youkai/le: CENSUS frame=%u live=%d scanned=%d iv997=%d slot28_29=%d "
                       "parent=%d (slot28_29>0 && iv997==0 => marker lost; both 0 => their "
                       "familiars are not on the field / see CENSUS-DET sub=)",
                       (unsigned)s_ledger_frame, live, scanned, n997, n2829, npar);
            }
            for (i = 0; i < YK_XFAM_MAX; ++i) s_xfam[i].seen = 0;
            CE_ENEMY_FOREACH(em, e) {
                uint32_t id, parent;
                int32_t life;
                int le_fam = 0;
                int le_via = 0;          /* 0 = 不是；1 = `$I3==997`；2 = 图像脚本 28/29（2026-10-19c）*/
                const float *cp;
                uint8_t *pe;
                yk_xfam_t *x;
                if (!e) continue;
                id = *(const uint32_t *)(e + CE_ENEMY_ID);
                if (!id) continue;
                /* ★★★ 2026-10-01：读 **zEnemy+0x6834**（`YK_ENEMY_PARENT_ID`）—— 旧代码读
                 *   `CE_SPAWN_PARENT`（= +0x50 = ECL 栈变量）⇒ 这一支几乎永不成立（只有 ECL
                 *   变量恰好等于某个 boss id 时才喂一次）⇒ 「外部子敌人」那条转嫁源实际是死的
                 *   （实跑台账 `xfer[... ext 0/0 ...]`）。修对之后 = 真的按「谁是 boss 的孩子」喂。*/
                parent = (uint32_t)*(const int32_t *)(e + YK_ENEMY_PARENT_ID);
                /* ★★★ 2026-10-19（任务 A 第 4 步）：**无条件 MARK** —— 只要 `$I3 == 997` 就打一行，
                 *   **不依赖 `le_boss` / `le_here` / `pending` / 任何门** ✓（放的位置在**所有**
                 *   `continue` 之前 ⇒ 这一行是"我们到底看没看见这只敌机"的唯一凭据）。
                 *   为什么必须有它：接管块现在挂在 `!parent && le_boss && …` 三个合取项上，任一不成立
                 *   **都是静默** ✗ —— 01:26 那局 `le-probe` 明明读得到 997 ✓，`xfer[tick 1065 … ext 150/6169]`
                 *   也说明扫描在跑 ✓，却 0 行 `youkai/leveledit:` ⇒ 只能靠这条 MARK 分辨是哪一项。
                 *   三种读法（下一局看这一行就够）：
                 *     · **完全没有 MARK 行** ⇒ 那局场上没有 997 敌机 / 扫描没跑到（先看 `xfer[tick …]`）；
                 *     · `le_boss=0` ⇒ boss 取不到（看同帧 `CTX … boss_slots=`）⇒ 接管门第一项就短路 ✗；
                 *     · `le_boss>0 且 parent=0` 却仍无 TAKEOVER ⇒ 后面的门在挡（`in_map=1` / `in_orbit=1`）。
                 *   节流：同 id 每 60 帧最多一条（16 格表，满了就不打 —— 使魔种类很少 ✓）。*/
                {   int32_t iv3m = yk_le_iv3(e);
                    /* ★ 2026-10-19c：`$I3` 那条在实跑里可能读不到（02:09 那局 0 行）⇒ **图像脚本**
                     *   命中也打 MARK（`via=2`），否则下一局又会「一行日志都没有」地静默。*/
                    const int via_anm = yk_le_fam_by_anm(e, le_stage);
                    if (iv3m == YK_LE_FAM_MARK || via_anm) {
                        unsigned mi, slot = 16u;
                        for (mi = 0; mi < 16u; ++mi) {
                            if (s_le_mark_id[mi] == id) { slot = mi; break; }
                            if (!s_le_mark_id[mi] && slot == 16u) slot = mi;
                        }
                        if (slot < 16u && (s_le_mark_id[slot] != id ||
                                           (uint32_t)(s_ledger_frame - s_le_mark_frame[slot]) >= 60u)) {
                            s_le_mark_id[slot] = id;
                            s_le_mark_frame[slot] = s_ledger_frame;
                            ce_log("youkai/leveledit: MARK id=%u via=%d iv3=%d anm0=%d le_boss=%u "
                                   "le_here=%d le_stage=%d parent=%u in_map=%d in_orbit=%d in_xfam=%d life=%d",
                                   id, (iv3m == YK_LE_FAM_MARK) ? 1 : 2, (int)iv3m,
                                   (int)*(const int32_t *)(e + CE_ENEMY_DATA + 0x26c),
                                   le_boss ? *(const uint32_t *)(le_boss + CE_ENEMY_ID) : 0u,
                                   le_here, le_stage, parent,
                                   yk_map_has_id(&s_map, id) ? 1 : 0,
                                   yk_orbit_find_id(id) ? 1 : 0,
                                   yk_xfam_find(id) ? 1 : 0,
                                   (int)*(const int32_t *)(e + YK_ED_LIFE_CUR));
                        }
                    }
                }
                /* ★★ B2：**他们的使魔** —— 2026-10-19c 起是**两条判据取或**：
                 *   ① `$I3 == 997`（他们 ECL 的标记，`yk_le_is_their_familiar`）；**或**
                 *   ② 主 anm 脚本 ∈ {28,29} 且在这一面确认过「是他们的那场」（`yk_le_fam_by_anm`）。
                 *   ⇒ 加 ② 的原因：02:09 那局 `slot0=29` 的使魔在场上、`$I3` 却是 0（0 行 MARK）
                 *     ⇒ 只看 ① 永远接管不到 ✗（现象 = 人侧不消失 / 没切换音 ✓ 与 owner 报告一致）。
                 *   没有 parent 时把落点设成**当前 boss**（一手语义 = 「打使魔 ⇒ 转给它的主人」✓）。*/
                if (le_boss) {
                    if (yk_le_is_their_familiar((int)yk_le_iv3(e))) {
                        le_fam = 1;
                        le_via = 1;                                /* ① ECL 标记 */
                    } else if (yk_le_fam_by_anm(e, le_stage)) {
                        le_fam = 1;
                        le_via = 2;                                /* ② 使魔图像脚本 28/29 */
                    }
                }
                if (le_fam) parent = *(const uint32_t *)(le_boss + CE_ENEMY_ID);
                if (!parent) continue;                             /* 只看子敌人 / 接管的使魔 */
                if (yk_map_has_id(&s_map, id)) continue;            /* 已在映射层 ⇒ 那边已喂，别重复 */
                if (yk_orbit_find_id(id)) continue;                 /* 绕转使魔走下面那条支 */
                pe = ce_enemy_by_id(em, (int32_t)parent);
                if (!pe) continue;
                /* 判据：母机是 boss 系（`CE_EM_BOSS_IDS[4]` **或** 母机自己的 `own_boss_id != 0`）。
                 * ★ 接管的使魔例外：落点就是 `yk_boss()` 本人 ⇒ 天然满足，不必再走这条（也不该
                 *   因为它们没有 parent 而被它挡掉 ✓）。*/
                if (!le_fam && !yk_parent_is_boss(em, pe)) continue;
                /* ★ 2026-10-19g：接管的使魔血量提到**我们自建使魔的规则**（写三格，不置位）*/
                if (le_fam && le_boss) yk_le_fam_raise_hp(e, le_boss);
                life = *(const int32_t *)(e + YK_ED_LIFE_CUR);
                cp = ce_enemy_pos(e);
                x = yk_xfam_find(id);
                if (!x) {                                          /* 新条目：表满就**放弃**（不转）*/
                    unsigned k;
                    if (s_xfam_used >= YK_XFAM_MAX) continue;
                    for (k = 0; k < YK_XFAM_MAX; ++k) if (!s_xfam[k].id) break;
                    if (k >= YK_XFAM_MAX) continue;
                    x = &s_xfam[k];
                    __builtin_memset(x, 0, sizeof *x);
                    x->id = id;
                    x->le = (uint8_t)le_fam;
                    /* 第一次掉血就要能打出来 ⇒ 把「上次日志帧」预置成 30 帧之前。*/
                    x->last_log_frame = s_ledger_frame - YK_XFAM_LOG_EVERY;
                    ++s_xfam_used;
                    if (le_fam)                                    /* ★ 一次一条：实跑里能数出接管了几只 */
                        ce_log("youkai/leveledit: TAKEOVER familiar id=%u via=%d ($I3=%d, "
                               "anm0=%d) -> external-familiar ledger (1/2 transfer to boss id=%u)",
                               id, le_via, (int)yk_le_iv3(e),
                               (int)*(const int32_t *)(e + CE_ENEMY_DATA + 0x26c), parent);
                } else if (x->life > life) {
                    /* 掉血 ⇒ 转 1/2（`yk_transfer_feed` 内部按比例）；自激盒内只采样不喂。
                     * ★ 2026-10-18：`fed` = **真正加进累加器**的量（半整数进位器 ⇒ 不是死板的 d/2），
                     *   它就是 `xfam hit … transfer=<fed>` 那个数。*/
                    {   int32_t d = x->life - life;
                        int in_box = (!cp || !bp) ? 0 :
                                     yk_in_transfer_zone(cp[0], cp[1], bp[0], bp[1],
                                                         YK_TRANSFER_BOX_W, YK_TRANSFER_BOX_H);
                        int32_t fed = 0;
                        if (!in_box) {
                            int32_t before = s->tr.pending;
                            ++s_xfer_ext_feed; s_xfer_ext_delta += d;
                            s_xfer_parent = parent;              /* ★ 落点 = 外部子敌人的母机 */
                            yk_transfer_feed(&s->tr, d, YK_TRANSFER_PCT_MAP);   /* ★ P59：1/2 ✓ */
                            fed = s->tr.pending - before;
                            if (fed < 0) fed = 0;                /* pending 上限钳回时可能变小 */
                            s_xfer_ext_amount += fed;            /* ★ 本 tick 外部使魔的贡献 */
                            if (le_fam) s_xfer_ext_le_amount += fed;   /* 其中「接管的」那一部分 */
                            ++s_xfam_fed;
                        }
                        if (le_fam) yk_xfam_hit_log(id, d, parent, fed, in_box);
                    }
                }
                /* ★ 2026-10-18：接管的标记按**本帧判据**维护；判据掉了（`le_here` 变 0）就把外观还回去。*/
                if (!le_fam && x->le) { yk_xfam_vm_kill_id(id); x->le = 0; }
                else if (le_fam) x->le = 1;
                x->life = life;
                x->seen = 1;
            }
            for (i = 0; i < YK_XFAM_MAX; ++i) {                    /* 查不到 ⇒ 删（不因血量没变删）*/
                if (s_xfam[i].id && !s_xfam[i].seen) {
                    if (s_xfam[i].le) yk_xfam_vm_kill_id(s_xfam[i].id);   /* 妖侧外观先还回去 */
                    __builtin_memset(&s_xfam[i], 0, sizeof s_xfam[i]);
                    --s_xfam_used;
                }
            }
            }   /* ← `le_stage` 作用域结束（2026-10-19c）*/
        }
        /* ★ 绕转使魔（`s_orbit[]`）：它们不进 `s_map` 但也必须转（一手：1/2 对使魔成立）。
         * 采样与映射使魔同一形状（`life.current` 差分），直接喂同一个累加器。*/
        for (i = 0; i < YK_ORBIT_MAX; ++i) {
            yk_orbit_t *o = &s_orbit[i];
            uint8_t *f;
            if (!o->fam_id) continue;
            f = ce_enemy_by_id(em, (int32_t)o->fam_id);
            if (!f) continue;
            {
                int32_t lc = *(const int32_t *)(f + YK_ED_LIFE_CUR);
                yk_xfam_t *x = yk_xfam_find(o->fam_id);
                if (!x) {
                    unsigned k;
                    if (s_xfam_used < YK_XFAM_MAX)
                        for (k = 0; k < YK_XFAM_MAX; ++k) if (!s_xfam[k].id) {
                            __builtin_memset(&s_xfam[k], 0, sizeof s_xfam[k]);  /* le = 0：不是接管的 */
                            s_xfam[k].id = o->fam_id; s_xfam[k].life = lc; s_xfam[k].seen = 1;
                            ++s_xfam_used; break;
                        }
                    continue;
                }
                if (x->life > lc) {
                    int32_t d = x->life - lc;
                    ++s_xfer_orbit_feed; s_xfer_orbit_delta += d;
                    s_xfer_parent = o->parent_id;            /* ★ 落点 = 这只绕转使魔的母机 */
                    yk_transfer_feed(&s->tr, d, YK_TRANSFER_PCT);       /* ★ P59：绕转（附加）使魔 ⇒ 全额 ✓ */
                    ++s_xfam_fed;
                }
                x->life = lc;
                x->seen = 1;
            }
        }
    }
    if (s->tr.pending <= 0) return;
    /* ★ 硬约束 ② + owner「要的」：时符**或母机无敌**期间 ⇒ 整笔待转嫁量丢进 `dropped`。*/
    {   int why = yk_xfer_suppress_reason();
        yk_xfer_suppress_log(why);
        if (why) {
            s->dropped += (uint32_t)s->tr.pending;
            s->tr.pending = 0;
            return;
        }
    }
    /* ★★★ 2026-10-01：**落点 = 使魔自己的母机**（`s_xfer_parent`）——一手语义就是「转给它的主人」，
     *   不再要求母机在 boss 表里（道中大妖精 / 道中 boss / leveledit 米斯蒂亚那批都能落地）。
     *   母机查不到（已死/已撤）才退回**全局 boss**；两者都没有才 `noboss` 丢（账仍不攒）。*/
    {
        uint8_t *em2 = CE_ENEMY_MGR();
        uint8_t *pe = (em2 && s_xfer_parent) ? ce_enemy_by_id(em2, (int32_t)s_xfer_parent) : 0;
        const float *pp2 = pe ? ce_enemy_pos(pe) : 0;
        if (pp2) { bp = pp2; tgt = pe; }
        else {
            if (s_xfer_parent) ++s_xfer_noparent;             /* ★ 诊断：母机查不到 */
            if (!bp) {
                ++s_xfer_noboss;                              /* 连 boss 兜底也没有 ⇒ 丢 */
                s->dropped += (uint32_t)s->tr.pending;
                s->tr.pending = 0;
                return;
            }
            tgt = boss;
        }
    }
    amount = yk_transfer_clamp(s->tr.pending, YK_TRANSFER_CAP_PER_FRAME);
    s->tr.pending = 0;
    if (amount <= 0) return;
    /* ★ 每帧**新建**一条源（一源一敌一辈子只结算一次，tag 守卫 `0x45f45a` ⇒ 复用等于只打一次）。
     * 同帧抬每敌每帧上限：自机弹贴脸打 boss 时配额已被吃满，不抬就被整份吃掉（AUDIT V20）。*/
    if (!YK_ENABLE_TRANSFER) {                                /* ★ 二分开关 ⑥：不建伤害源、不抬上限 */
        s->dropped += (uint32_t)amount;
        return;
    }
    /* ★ 2026-10-18（owner：转伤要可观测）：建源**之前**读一次目标 HP，下一个 tick 再读一次
     *   ⇒ `transfer applied boss=… hp_before=… hp_after=…` 那行。
     *   门 = 本 tick 有**接管的 leveledit 使魔**喂过料（`s_xfer_ext_le_amount`）—— 别用更宽的
     *   `s_xfer_ext_amount`：普通关卡 boss 的普通子敌人也走 ext 支，那会在零售局里刷 `leveledit:` 日志。*/
    if (s_xfer_ext_le_amount > 0 && tgt) {
        s_xfer_obs_id     = *(const uint32_t *)(tgt + CE_ENEMY_ID);
        s_xfer_obs_hp     = *(const int32_t *)(tgt + YK_ED_LIFE_CUR);
        s_xfer_obs_amount = amount;
        s_xfer_obs_ext    = s_xfer_ext_le_amount;
        s_xfer_obs_wait   = 1;
    }
    ++s_xfer_rect;                                           /* ★ 诊断：真的调了几次伤害源 */
    ce_raise_damage_cap(yk_base_damage_cap() + amount);
    {   int slot = ce_damage_rect(bp, 0.0f, YK_TRANSFER_SRC_LIFE, (int)amount,
                                  YK_TRANSFER_BOX_W, YK_TRANSFER_BOX_H);
        if (slot <= 0) ++s_xfer_rect_fail;                    /* ★ 伤害源槽没开出来（>`0` = 成功）*/ }
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

/* ★★★ 2026-09-30（owner 问：「那些**间断发弹的发弹口**，**发弹口序号本身是稳定的吗**？」）
 *   —— **纯诊断**（**零行为变化**：不改 `yk_note_slot` / `yk_map_*` / 任何生命周期逻辑）。
 *   只做一件事：给每个 `(父敌 enemy_id, 槽序号 slot)` 记一个「上帧 active 与否」，并且**只在
 *   `active → inactive → active` 这条转换上**打一行；同一对最多打 `YK_EMITKEY_MAX_LOG` 次（防刷屏）。
 *   判读：拿一次 4 面道中 boss（或 6 面开场阵列）的实跑日志看那几行 ——
 *     · 同一 `(enemy, slot)` **off 之后又出现** ⇒ **序号稳** = (a)（③ 可以按现有身份做）；
 *     · 每次出现的 `slot` 都不同（旧的不再回来）⇒ **序号会漂** = (b)（要换「父敌 id + 槽位置分桶」的键）；
 *     · `child #` 每次都换 ⇒ 引擎确实每次都建新实体（那正常，**关键只是别在 off 期间撤壳**）。*/
#define YK_EMITKEY_MAX      32u
#define YK_EMITKEY_MAX_LOG  3u
typedef struct {
    uint32_t enemy_id;
    uint8_t  slot;
    uint8_t  active_prev;      /* 上一帧这个 (enemy,slot) 是不是 active */
    uint8_t  went_off;         /* 见过一次 active→inactive */
    uint8_t  logged;           /* 这一对已经打了几行 */
} yk_emitkey_t;
static yk_emitkey_t s_emitkey[YK_EMITKEY_MAX];
/* ★ 2026-10-01（owner 要「一次实跑必定出结果」）：**收尾汇总**用的三个计数。
 *   · `pairs`           = 见过的 `(enemy, slot)` 对总数；
 *   · `reappeared`      = 该对 **OFF 后又出现**的对数（>0 ⇒ **序号稳** STABLE）；
 *   · `new_after_off`   = 「**父敌我们已经认识**、但又看到一个**没见过的 slot 号**」的次数
 *                         （`reappeared == 0 && new_after_off > 0` ⇒ **序号漂** DRIFTED）。*/
static unsigned s_emitkey_pairs, s_emitkey_reappeared, s_emitkey_new_after_off;
static unsigned s_emitkey_acts;   /* ★ 本局「看到过 active 槽」的次数（>0 = 我们**确实扫到了槽**）*/

static void yk_emitkey_note(uint32_t enemy_id, int slot, int active, uint32_t child,
                            uint32_t frame)
{
    unsigned i, free_i = YK_EMITKEY_MAX;
    yk_emitkey_t *k;
    if (!enemy_id || slot < 0) return;
    for (i = 0; i < YK_EMITKEY_MAX; ++i) {
        if (s_emitkey[i].enemy_id == enemy_id && s_emitkey[i].slot == (uint8_t)slot) break;
        if (!s_emitkey[i].enemy_id && free_i == YK_EMITKEY_MAX) free_i = i;
    }
    if (i >= YK_EMITKEY_MAX) {                                 /* 没记过 ⇒ 找空格登记 */
        unsigned j;
        if (free_i >= YK_EMITKEY_MAX) return;                  /* 表满：**静默放弃**（只是诊断）*/
        i = free_i;
        /* ★ 漂的判据：**这个敌人我们已经认识**（表里有它的别的 slot），却又冒出一个新 slot 号 */
        for (j = 0; j < YK_EMITKEY_MAX; ++j)
            if (s_emitkey[j].enemy_id == enemy_id) { ++s_emitkey_new_after_off; break; }
        ++s_emitkey_pairs;
        s_emitkey[i].enemy_id = enemy_id;
        s_emitkey[i].slot = (uint8_t)slot;
        s_emitkey[i].active_prev = 0;
        s_emitkey[i].went_off = 0;
        s_emitkey[i].logged = 0;
    }
    k = &s_emitkey[i];
    if (active) ++s_emitkey_acts;                              /* ★ 诊断：扫到过活跃槽 */
    if (k->active_prev && !active) k->went_off = 1;            /* active → inactive */
    else if (!k->active_prev && active && k->went_off) {       /* inactive → active（**转换**）*/
        ++s_emitkey_reappeared;                                 /* ★ 汇总：OFF 后又出现 */
        if (k->logged < YK_EMITKEY_MAX_LOG) {
            ++k->logged;
            ce_log("youkai/emitkey: enemy #%u slot %d child #%u frame %u active %d "
                   "(re-appeared after an OFF - same (enemy,slot) pair = ordinal is STABLE; "
                   "if you never see a repeated slot, it DRIFTED)",
                   enemy_id, slot, child, frame, active);
        }
        k->went_off = 0;                                        /* 重置，等下一次 off→on */
    }
    k->active_prev = (uint8_t)(active ? 1 : 0);
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
            if (!ce_enemy_lockable(e)) continue;                            /* 只挡 NO_LOCK；妖侧只置 NO_DAMAGE */
            id = *(const uint32_t *)(e + CE_ENEMY_ID);
            if (!id) continue;
            /* ★★★ 2026-10-01（owner）：「**妖侧**使魔的追踪也不能追踪到」—— owner 随后澄清
             *   「这个关掉**不是**说关掉追踪，是关掉**对妖侧使魔可能的追踪**」。
             *   ⇒ 我们自己的**绕转使魔**（`s_orbit[]`，**不进 `s_map`**）**不进候选**。
             *   ⚠ **本函数只在妖侧被调**（唯一调用点 = `on_tick_2` 的 `if (focused)` 分支；
             *     非聚焦那一支直接把 `chase_have = 0`）⇒ 这一条 = owner 要的「只关妖侧」；
             *     **人侧根本不经过这里**（那时子机走引擎原生跟随、发弹角写 −π/2）⇒ **人侧照旧**：
             *     使魔仍是「真敌人」（可打、走 100% 转嫁）。
             *   ⚠ 这一条**今天在行为上是冗余的**：下一行的「链根」门本来就想排掉我们自建的使魔
             *      —— 但**它其实排不掉**（见下面那一行的 ⚠⚠：那一格在 `zEnemy*` 上是 ECL 栈变量，
             *      不是父字段）⇒ 真正把绕转使魔排掉的是**上面这一行**（`yk_orbit_find_id`）。
             *     写它的理由 = 把 owner 的口径从**隐式**变成**显式**：
             *     将来若有人为了**人侧**放宽链根门（人侧确实希望使魔可被索敌），妖侧这条不会跟着破。
             *   ⚠ 「按母机 id 查绕转表」那一条（父字段非 0 && `yk_orbit_find_parent(pid)`）
             *     写在这里是**死代码**（下一行已 `continue`）⇒ 不写；认使魔**自己的 id** 就够。*/
            if (yk_orbit_find_id(id)) continue;                             /* 妖侧：我们的绕转使魔不当目标 */
            /* ⚠⚠ 2026-10-01：这一格与「链根」门同款历史遗留 —— 在 `zEnemy*` 上读到的其实是
             *   **ECL 栈变量**（见 `YK_ENEMY_PARENT_ID` 的长注），**不能**改成真父字段：
             *   TH18 时间轴自己是一只敌人 ⇒ 真父字段几乎恒非 0 ⇒ 改对之后**所有**敌机都被这一行
             *   排掉 ⇒ 追踪目标永远为空。绕转使魔已由上一行显式排除 ⇒ 这一行的实际作用只是
             *   「ECL 变量恰好非 0 的敌机不当目标」。要修 = 另立判据（父敌是不是 boss），另开一批。*/
            if (*(const int32_t *)(e + CE_SPAWN_PARENT) != 0) continue;     /* 链根：不是别人生成的子敌人 */
            if (yk_map_is_fam(&s_map, id)) continue;                        /* 映射使魔（这一档是链根）不当目标 */
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
/* ★★★ 2026-10-01（owner 工单③ · **人侧写入二分开关**，调试用，随下一版发给 owner 做实验）：
 *   `1`（**默认 = 现状**）：人侧照常走本卡的子机声明/落笔（下面 `on_option_pose_apply` 会
 *        写**我们自己**那 12 格卡池对象的阵型；原生 4 格的**膛口**自本批起**只在妖侧**写，见 mirror 段）。
 *   `0`（**二分版**）：**人侧完全不碰引擎的子机层** —— 不 `hide_declare` / 不 `keep_records` /
 *        不 `yk_option_declare` / 不落笔（`on_option_pose_apply` 人侧直接早退并收干净）。
 *   ⇒ owner 用「0 版」打一次人形态：**激光恢复正常 ⇒ 凶手在本卡的人侧子机写入**；
 *     仍然异常 ⇒ 与本卡无关（回去查引擎侧，需要 dump/时序）。
 *   ⚠ 只影响**人侧**；妖侧一行不变（妖侧的隐藏/收束/膛口镜像都是功能本体）。*/
#define YK_HUMAN_OPT_WRITE 1   /* ★ 二分实验（#98/#99）已做完 ⇒ 恢复推荐值 1；=0 那版已排除「人侧子机写入」 */

static int on_option_pose_apply(ce_card_t *c)
{
    if (!YK_ENABLE_CARD) return 0;                    /* ★ 总开关：整卡 no-op */
    yk_card_t *s = ce_state(c, yk_card_t);
    const ce_option_pose_t *pose = ce_option_pose_pose();
    const float *pp = ce_player_pos();                    /* 弹簧的目标基准（th08 #1）*/
    const int character = yk_current_character();
    int want, i;

    if (!s) return 0;
    /* ★ 二分开关 ③：整个子机声明/落笔层关掉（收干净再退）。*/
    if (!YK_ENABLE_OPTION_POSE) {
        if (s->opt_script >= 0 || s->opt[0].ptr) yk_option_release(c, s);
        return 0;
    }
    /* ★ 2026-10-01（工单③ 二分开关 + 人侧探针）：`YK_HUMAN_OPT_WRITE == 0` ⇒ 人侧完全不落笔。*/
    if (!YK_HUMAN_OPT_WRITE && !yk_focused()) {
        if (s->opt_script >= 0 || s->opt[0].ptr) yk_option_release(c, s);   /* 收干净再退 */
        if (s->pose_probe < 6) {
            ++s->pose_probe;
            ce_log("youkai/pose: human side HANDS-OFF (YK_HUMAN_OPT_WRITE=0) - no option declare/"
                   "apply, no native slot writes");
        }
        return 0;
    }
    if (ce_option_pose_winner() != (int)ce_card_id(c) || !pose || pose->count <= 0) {
        if (s->opt_script >= 0 || s->opt[0].ptr) yk_option_release(c, s);   /* 退位：收干净 */
        return 0;
    }
    /* ★ 2026-10-01（工单③ 判据行）：人侧**前几次**把「我们声明了什么」打出来（格数 + 非聚焦阵型）。*/
    if (!yk_focused() && s->pose_probe < 6 && pose->count > 0) {
        ++s->pose_probe;
        ce_log("youkai/pose: human write decl off=(%d,%d) slots=%d script=%d (these are the native "
               "option objects' offsets/positions we may write; owner laser report)",
               (int)pose->cell[0].ux, (int)pose->cell[0].uy, pose->count, pose->anm_script);
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
    /* ★★★ 2026-10-01（**本批的真修法**，owner 工单③ 头号嫌疑）：**mirror 只在妖侧跑**。
     *   本段原来是**无条件**执行的 —— 它写的是**原生那 4 格**（`player+0x670+i*0xf0`）的
     *   **膛口 `+0x5c/+0x60`**（`CE_OPT_POS_X/POS_Y`）与聚焦偏移 `+0x6c/+0x70`。
     *   而它自己的注释写着「⚠ 只写**聚焦**那一对……高速（人侧）用的是非聚焦对，**一个字节都不碰**
     *   ⇒「人侧和不装 th08 完全一样」」—— 事实与注释不符：**膛口那两格在人侧也照写**，
     *   而且人侧 `have = 1`（魔理沙）时槽 1..3 还会被写成「与最后一台同点」。
     *   一手（`sdk.h` 的 mode=2 说明）：**束的位置每帧从持有者读** ⇒ 人侧每帧改写原生格的膛口
     *   = 我们主动喂束的原点（形状：束分段/起点抖、命中处像「断了一节」）。
     *   ⇒ 现在起：`yk_focused()` 为假时**整段跳过**（人侧一个字节都不写）；`YK_HUMAN_OPT_WRITE=0`
     *     时连我们自己卡池那支也不写（上面的早退）。*/
    if (yk_focused()) {
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
 * ★★★ 2026-09-27（第三十八批 · owner：「**得扩永点池到 ~1e2 量级，至少你得把击破 boss 阶段的
 *   那个螺旋形能给「渲染出来」**」）：**64 → 128**。这个数不是拍的 —— 由螺旋的**喷点速率 × 在飞时长**
 *   反推（Little 定律；算式落在 `youkai_core.{h,c}` 的 `yk_spiral_live_peak()`，单测钉着）：
 *     · 一手喷点：每 **8** 帧一批，一批 = `min(额度,7)` **同角** + **固定 6 颗 +π** = 至多 **13 颗/批**
 *       （`th08:0x4175cc` / `:0x4176be`，见 `youkai_core.h` §3.11b）；
 *     · 一颗刻符的在飞时长（TH08 运动方程，我们的实现同一套）：上抛 `vy = −2.0−rand(0.2)`、
 *       每帧 `+0.05` ⇒ **40–44 帧**才转归航，再按 `[.sht+0x14]` = **10** px/帧 归航到自机
 *       （螺旋撒在**半径 128** 的环上 ⇒ 典型再走 ~20 帧）⇒ **≈60–65 帧**；
 *     · ⇒ 稳态在飞 ≈ `13 × 65 / 8` ≈ **106 颗**（+ 最新一批的余量）⇒ 池 **128** 留了余量。
 *   ⚠ 64 那版会把螺旋**削掉约四成**（多出来的颗数静默丢进 `orb_full`）—— 那正是 owner 说的
 *     「渲染不出来」。现在实跑用台账的 `peak=` 复验：螺旋那一段应当打到 ~100，`full=` 不该明显涨。
 * ★★★ 2026-09-27（同一天 · owner 追加：「**池子改成 256 吧 —— 会增加负担吗？**」）：**128 → 256**
 *   —— 下午又按订正后的螺旋节奏再抬到 **1024**：螺旋是**每帧 13 颗**（不是每 8 帧一批，见 core 的
 *   `YK_SPIRAL_START_AGE` 订正）、一颗在飞 ≈75 帧 ⇒ 稳态 `13×75 + 13 = 988` ⇒ 池取 **1024**
 *   （256 那版会把螺旋削掉七成以上）。负担仍与「同时在飞颗数」成正比（见下面的分析），
 *   且只在螺旋那 1.2 s 里逼近上限。
 *   **负担分析**（答案：**几乎不增加**；代价只跟「**同时在飞的颗数**」走，不跟池常量走）：
 *     · **静态内存**：`yk_orb_t` = 24 B ⇒ 64/128/256 格 = 1.5 / 3 / **6 KB**（模块级 static，
 *       不进卡状态 `CE_STATE_BYTES`）。
 *     · **每帧**：两条 O(池) 的扫描（损命边沿那一遍 + 主 tick 那一遍）各 256 次「读 used 就跳过」
 *       —— 几百个指令，噪声级；真正的每颗代价是 **O(在飞颗数)**：状态分支几条浮点 + 一次
 *       `ce_anm_set_pos` + 引擎自己的 `AnmVm::run` + 一个 quad。
 *     · **VM 内存**：引擎的 VM 不是定长池 —— `AnmManager::allocate_new_vm` 在 freelist 空时
 *       **`malloc(0x60c)`**（=`th18:0x489320`，见 `engine.h` 的 `CE_FN_DO_GRAZE` 注 ⑤）
 *       ⇒ 256 支 VM ≈ **0.4 MB** 堆；收下/删掉时随引擎自己的 freelist 回收。
 *     · **对比**：TH18 自己也每帧画成百上千个弹/道具 quad；256 个额外 sprite ≈ 噪声。
 *   ⇒ 256 是**上限**，只有「螺旋（~119）+ 击破批 + 人逢魔每发命中」三路**同时**在飞才可能逼近它。
 * ❓ 池容量本身仍是**自定**（TH08 那边是 `ItemManager` 自己的道具池，我们不共享那个池）。*/
#define YK_ORB_MAX            1024
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
    /* ★★★ 2026-09-27（第三十九批 · owner：「**人逢魔生成的永点应该没有吸收过程**，或者吸收过程被
     *   透明化，总之我目前能看见自动吸收过程」）：出生那一刻槽在**逢魔**档的刻符 ⇒ 上抛结束
     *   （`vy > 0`）**直接吃掉**，不走「飞向自机」的那段路程（= owner 2026-09-27 更早的同一句口径
     *   「下落一小段之后**直接消失**（= 直接吃掉）」）。⚠ 与一手的差别登记见 `TUNING.md`。*/
    uint8_t  eat_at_toss;
    uint8_t  _pad;
} yk_orb_t;

static uint8_t s_kill_slots_logged;                    /* ★ 第四十六批：只记一次（颗数改用「声明过的发射口槽数」）*/
static unsigned s_spiral_orbs;                       /* ★ 2026-09-27（第四十四批 · owner：「打 boss 的螺旋
                                                       *   和逢魔状态没关系，是**固定撒的**」）：螺旋这一路
                                                       *   累计撒了几颗 —— 判据 = 一轮满额应当 ≈935，
                                                       *   且**与 `gauge` 无关**（下面那行日志同时打出槽值）。*/
static unsigned s_fengma_eaten;                      /* ★ 2026-09-27（第三十九批）：**逢魔档的刻符在上抛
                                                       *   结束那一帧直接入账**（不走「飞过来」那段）的次数
                                                       *   —— owner 口径「自动吸收过程是指飞过来，生成部分
                                                       *   原版还是能看见的」。*/
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

/* ★★ 2026-10-03b（92 伟大之作的**清扫闸**；统筹接 `HANDOFF-86-92-AND-SWEEP.md` §3 的 T2）：
 *   92 发动时 `ce_sweep_bump()` ++代数 ⇒ 有私有池的卡在自己 tick 里看代数变了，就把自家那批**清成金钱**。
 *   本卡这一批 = **刻符（永点 / `yk_orb_t`）** —— 它不在引擎道具池里（本卡私有池）⇒ 只能本卡自己清。
 *   ⚠ 消失必须走本卡自己的 `yk_orb_kill()`（删 VM 句柄 + 清 `used`），否则场上留幽灵 VM（铁律二）。
 *   ❓ 面值自定（1 颗刻符 = `YK_SWEEP_MONEY_PER_ORB` 枚金钱；与 112/117 的「每 token 10 枚」不是同一口径
 *      —— 刻符在场上成批出现，量级比 token 大得多）—— 见 `TUNING.md` 的 108 段。*/
#define YK_SWEEP_MONEY_PER_ORB 1
static uint32_t s_sweep_seen;

static void yk_sweep_to_money(void)
{
    unsigned i;
    int n = 0, k;
    for (i = 0; i < YK_ORB_MAX; ++i) {
        yk_orb_t *o = &s_orb[i];
        float p[3];
        if (!o->used) continue;
        p[0] = o->x;
        p[1] = o->y;
        p[2] = 0.0f;
        for (k = 0; k < YK_SWEEP_MONEY_PER_ORB; ++k) {
            if (!ce_spawn_item(2u, p, -1.5707964f, 1.7f)) ce_money_pocket(1);
        }
        yk_orb_kill(o);                     /* ⚠ 走本卡自己的收尾（见上）*/
        ++n;
    }
    if (n) {
        s_orb_live = 0;
        ce_log("youkai: 92 清扫 —— %d 颗刻符换成金钱（每颗 %d 枚）", n, YK_SWEEP_MONEY_PER_ORB);
    }
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
static void yk_orb_spawn_ex(yk_card_t *s, float x, float y, int state, int eat_at_toss)
{
    yk_orb_t *o = 0;
    unsigned i;
    uint32_t id;
    /* ★ ⑪② `ORB_LIST=0`：**只走「建 VM」那一半**（不占我们的表、不进任何本卡逻辑）。*/
    if (!YK_ENABLE_ORB_LIST) {
        if (YK_ENABLE_ORB_VM) {
            int ch0 = yk_current_character();
            uint32_t vid;
            if (ch0 < 0 || ch0 > 3) ch0 = 0;
            vid = ce_anm_spawn(yk_player_anm(), YK_ORB_SCRIPT[ch0], YK_ORB_ANM_LAYER);
            if (vid) ce_anm_set_pos(vid, x, y, 0.0f);
        }
        return;
    }
    for (i = 0; i < YK_ORB_MAX; ++i) if (!s_orb[i].used) { o = &s_orb[i]; break; }
    if (!o) {
        if (s) ++s->orb_full;
        return;
    }
    {
        int ch = yk_current_character();
        if (ch < 0 || ch > 3) ch = 0;
        id = YK_ENABLE_ORB_VM
             ? ce_anm_spawn(yk_player_anm(), YK_ORB_SCRIPT[ch], YK_ORB_ANM_LAYER)
             : 0u;                                    /* ★ ⑪① 不建 VM ⇒ 下面 `!id` 放弃这颗 */
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
    /* ★ 2026-09-27（第三十九批 / 第四十三批收窄）：出生时槽在**極人**（人逢魔）⇒ 上抛结束直接吃掉
     * （owner ④）；螺旋/擦弹那几路传 `eat_at_toss = 0` ⇒ **保留可见的吸收过程**（owner 第四十三批）。*/
    o->eat_at_toss = (uint8_t)(eat_at_toss ? 1 : 0);
    o->x = x;
    o->y = y;
    /* 一手初速（`th08:0x4402cb`–`0x4402e9`）：`vx = rand(±0.6)`、`vy = −2.0 − rand(0.2)`。
     * ⚠ 这两条**只在 state 3** 写（`spawn_item` 的 state 3 分支）；state 1 的初速是 `cancel_autocollect`
     * 那一档 `(0, −0.5, 0)`（`th08:0x441407`–`0x44142c`），而**当帧就会被归航重设成满速**
     * （`th08:0x4407f6` → `:0x4408a5`）⇒ 初始值对可见行为无影响，这里照样给一组合理初值。
     * RNG 走 `ce_rand()`（SDK 的 replay-safe RNG）——**不引 libm、不进 x87**。*/
    o->vx = ((YK_ENABLE_ORB_PLAYERW ? (float)(ce_rand() % 1201u) : 600.0f) / 1000.0f - 0.6f);                    /* ❓ 用整数取模近似 randf */
    o->vy = (state == YK_ORB_STATE_HOMING) ? -0.5f   /* `th08:0x441407` 那一档的归航初速 */
                                           : (YK_ORB_VY0 - ((YK_ENABLE_ORB_PLAYERW ? (float)(ce_rand() % 1001u) : 500.0f) / 1000.0f) * YK_ORB_VY0_RAND);
    ce_anm_set_pos(id, x, y, 0.0f);
    ++s_orb_live;
    /* ★ 2026-09-27（第三十八批）：同时在飞的**峰值** —— 符卡螺旋那一段应当打到 ~100
     * （`YK_ORB_MAX` = 128 就是按 `yk_spiral_live_peak()` 反推的；实跑用台账 `peak=` 复验）。*/
    if (s && s_orb_live > s->orb_peak) s->orb_peak = (uint16_t)s_orb_live;
}

/* 默认包装：`eat_at_toss` 由**当前槽值**决定（`yk_orb_eat_at_toss` = 只认極人）。*/
static void yk_orb_spawn(yk_card_t *s, float x, float y, int state)
{
    yk_orb_spawn_ex(s, x, y, state, yk_orb_eat_at_toss((int)s->gauge));
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
#define YK_POPUP_MAX        128  /* ★★ 2026-10-01：24 → **128**（实跑 `dropped 4740` ⇒ 一局几千个
                                  *   收刻符「分值」浮字把 24 格池挤爆，**死亡 `+N` 永远没格子**）。
                                  *   ⇒ 总量抬到 128 + **分层**：`kind == 0`（收刻符分值，装饰性）可以被
                                  *   `kind != 0`（死亡 `+N` / `+1`，判据性）**挤掉**（见下面的分配逻辑）。*/   /* ★ 2026-09-30：3 → 8（一手打死一只母机可能一次冒
                                 *   1（链根 2n）+ 子机数（≤5）个浮字 ⇒ 3 会丢；池满只丢浮字、
                                 *   不影响道具/计分，见 `yk_popup_spawn` 的降级日志）*/
#define YK_POPUP_GLYPHS     5
#define YK_POPUP_ANM_LAYER  19   /* owner 2026-10-15: 会爆出一大堆永点, +N 的 layer 得在上面 => 取世界层最高档 19 (永点 7 / 道具 12 / 弹幕 14-16 都在下面) */
/* ★★★ 2026-10-01（owner 第三轮：「依旧没有发红字」；实跑日志已证**字建出来了**）：
 *   日志判据（`th18_card_expand.log`，build 17:54:12）：`youkai/popup spawn: tag root/part/orbit
 *   value 16 …` **86 行**、`youkai/kill:` **64 行**、`youkai/zone: opened 41 … bullets 104`
 *   ⇒ **入口 / 建字 / 消弹三条路都在跑** ⇒ 属「**建了但看不见**」那一类 ⇒ 唯一剩下的结构差异 = **层**：
 *     · 引擎渲染阶段表（`engine/anm/th18/02-render-stages.md`）：**层 11 属「敌机等」那一组**
 *       （优先级 0x18），而 **道具 12 / 激光·自机 13 / 子弹 14–16** 都在它**之后**（0x1b 起）
 *       ⇒ 浮字被自机弹/敌机盖住（实跑里敌人一死正是弹幕最密的那一帧）。
 *     · 原生「场地中央展示卡」`abcard.anm script14` 用的就是 **`layer(16)`**（脚本注：子弹之上）
 *       ⇒ 16 是**一手先例**，不是自定。
 *   ⚠ 仍与刻符（层 7）**同一参照系**（世界层 3–19 + 每帧回写 `resolutionMode(2)`，见 `yk_popup_tick`）
 *     ⇒ 这一改**不动坐标**、只动先后。*/


typedef struct {
    uint8_t  used;
    uint8_t  kind;
    uint32_t color;      /* ★ 2026-09-30（§BE #163）：一手 `CreateTimePopup` 的 arg3 = D3DCOLOR
                          *   （`0xAARRGGBB`，与我们引擎/本文件别处同序）—— 链根黄 `0xfff0f00f`、
                          *   子机按 n 分四档、击破使魔白。*/
    float    scale;      /* ★ 一手：字号在**创建那一刻**从 AsciiManager 的缩放拷贝（`th08:0x4035c5`）——
                          *   链根 `SetScale 2.03125`（`0x42b0ef`–`0x42b0fe`）、其余 1.0。*/
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

/* ★★★ 2026-10-01（owner：「目前看不到 +N」，并给两条假设 = **图层顺序** / **死亡顺序导致判定时 +0**）：
 *   死亡那几笔浮字的**播报**（`kind != 0` = 判据性 UI，`tag` 说明是哪一笔：`root` / `part` / `orbit` / `fam+1`）。
 *   定额 256 行后收口（`AUDIT.md` §BE #143 的 1.5 MB/min 日志洪水事故）。
 *   判据（下一轮实跑）：
 *     · 行里有 `value` ⇒ 直接看出「链根那笔的值是不是 0」；
 *     · **一行都没有**（连 `root` 都没有）⇒ 那一只**根本没进爆点支**（`n == 0` + `yk_kill_burst_count(0) == 0`
 *       ⇒ 整块被 `if (!n2) … else { … }` 跳过，见 `yk_on_enemy_killed`）—— 与「值 = 0」是**两种**失败，别混。
 *   ⚠ 只加日志：不改池、不改值、不改位置。*/
static void yk_popup_spawn_log(const char *tag, int32_t value, float x, float y)
{
    static unsigned s_popup_logged;
    if (s_popup_logged < 256u) {
        ++s_popup_logged;
        ce_log("youkai/popup spawn: tag %s value %d at (%.0f,%.0f) frame %u",
               tag ? tag : "?", (int)value, (double)x, (double)y, (unsigned)ce_sdk_frame());
    } else if (s_popup_logged == 256u) {
        ++s_popup_logged;                                   /* 只打一次「收口」提示 */
        ce_log("youkai/popup spawn: (further popup spawn lines suppressed after 256)");
    }
}

/* ★ 2026-09-30（§BE #163）：扩参 —— 颜色/字号照一手 `CreateTimePopup`（`th08:0x403460`，4 栈参：
 *   `pos / arg1 数字主体 / arg2 附加段 / arg3 颜色`）。alpha 由 `yk_popup_tick` 按**到自机的距离**每帧算。*/
static void yk_popup_spawn(float x, float y, int32_t value, int kind, uint32_t color, float scale,
                           const char *tag)
{
    yk_popup_t *p = 0;
    uint8_t dg[YK_POPUP_GLYPHS];
    int n, i, ch = yk_current_character();
    unsigned k;
    void *anm;
    if (kind != 0) yk_popup_spawn_log(tag, value, x, y);     /* ★ 死亡三笔的播报（kind != 0）*/
    for (k = 0; k < YK_POPUP_MAX; ++k) if (!s_popup[k].used) { p = &s_popup[k]; break; }
    if (!p && kind != 0) {
        /* ★★★ 2026-10-01（owner 实跑 `popup pool full, dropped 4740`）：**分层** —— 死亡 `+N`/`+1`
         *   是**判据性** UI，池满时**先挤掉最旧的 `kind == 0`（收刻符分值，一局几千个、装饰性）**。
         *   ⇒ 这样 `pf`（丢弃计数）**不再因为收刻符的字而涨**，死亡 `+N` 一定打得出来。*/
        unsigned oldest = YK_POPUP_MAX;
        uint8_t oldest_age = 0;
        for (k = 0; k < YK_POPUP_MAX; ++k) {
            if (!s_popup[k].used || s_popup[k].kind != 0) continue;
            if (oldest == YK_POPUP_MAX || s_popup[k].age > oldest_age) {
                oldest = k; oldest_age = s_popup[k].age;              /* 最旧的那个分值浮字 */
            }
        }
        if (oldest < YK_POPUP_MAX) { yk_popup_kill(&s_popup[oldest]); p = &s_popup[oldest]; }
    }
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
        uint32_t id = yk_vm_spawn(anm, YK_GAUGE_SCRIPT_FONT[ch], YK_POPUP_ANM_LAYER);
        if (!id) { yk_popup_kill(p); return; }
        p->vm[i] = id;
        ce_anm_set_sprite(anm, id, YK_GAUGE_FONT_BASE[ch] + (int)p->g[i]);
    }
    p->kind = (uint8_t)kind;
    p->color = color;
    p->scale = scale;
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
            /* ★★★ 2026-09-30（**P0：浮字 UI「十分偏移」**，见 `AUDIT.md` §BE #162）：
             *   浮字 VM 用的 `YK_GAUGE_SCRIPT_FONT[]` 是**HUD 字形脚本**，它正文自带
             *   `layer(25); resolutionMode(2);` ⇒ 脚本一跑就**覆盖**我们创建时给的层，
             *   于是我们喂的**世界/640 空间**坐标被当成 HUD（表面）参照系 ⇒ 整块偏到别处。
             *   ⇒ 每帧**再写回**：层 = `YK_POPUP_ANM_LAYER`(19，世界层最高档)，
             *     `flags_hi` 的 resolutionMode(bits22–24) = **2**（= `1×`，即「脚本没写
             *     `resolutionMode` 时」的默认档，见 `build_player_anm.py` 的一手理由）
             *   ⇒ 浮字与同批刻符（`yk_orb_spawn`，世界层 7）**同一参照系**。
             *   ⚠ 这样**不动 HUD 那几支脚本**（它们同时被 HUD 用，不能就地改 `layer(25)`）。*/
            {
                uint8_t *vm = (uint8_t *)ce_anm_get_vm(id);
                if (vm) {
                    *(volatile uint32_t *)(vm + 0x18) = (uint32_t)YK_POPUP_ANM_LAYER;   /* vm+0x18 = layer
                                                        *   （`engine.h:305`：`set_layer 0x4066b0` 写它）*/
                    *(volatile uint32_t *)(vm + CE_ANM_VM_FLAGS_HI) =
                        (*(const uint32_t *)(vm + CE_ANM_VM_FLAGS_HI) & ~0x1c00000u) | 0x800000u;
                }
            }
            dx0 = pp ? (int)(gx - pp[0]) : 0;
            dy0 = pp ? (int)(p->y - pp[1]) : 0;
            a = 0x50;                                  /* 先给最低档，下面按一手三档覆盖 */
            /* ★ 2026-09-30（§BE #163）：**颜色/字号照一手**，并且 **alpha 按到自机的距离**衰减
             *   （一手 `th08:0x4059b4`–`0x4059f3` 三档：`d > 0x1000 ⇒ 0xd0`、
             *   `d > 0x400 ⇒ 0x50 + d·0x80/0xc00`、否则 `0x50`）。
             *   ⚠ 距离口径 = **世界/640 空间的像素**（`ce_player_pos()` 与浮字同一套）；`d` 用
             *   `max(|dx|,|dy|) + 0.4·min(|dx|,|dy|)` 近似（省一次 sqrt，量级误差 <5%，已注明）。
             *   🟡 一手那三个阈值（0x400/0x1000）的单位我没有逐条确认 —— 若它其实是 TH08 的
             *   定点子像素，则这两档几乎不触发（只走 0x50）；实跑若发现浮字**整体偏暗**，
             *   第一件事就是回来核这个单位。*/
            {
                int ax = dx0 < 0 ? -dx0 : dx0, ay = dy0 < 0 ? -dy0 : dy0;
                int d = (ax > ay ? ax : ay) + (int)(0.4f * (float)(ax < ay ? ax : ay));
                if (d > 0x1000) a = 0xd0;
                else if (d > 0x400) a = 0x50 + d * 0x80 / 0xc00;
                else a = 0x50;
            }
            (void)ce_anm_set_color(id, ((uint32_t)a << 24) | (p->color & 0x00ffffffu));
            (void)ce_anm_set_scale(id, p->scale, p->scale);
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
        yk_gauge_limits_solo(yk_focused() ? 1 : 0, &hl, &yl);
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
            s->gauge = yk_gauge_add2(s->gauge, yk_gauge_orbs_delta(yk_focused()), hl, yl);
    }
    ++s->orb_taken;
    /* ★ #27 支 2：**收刻符的分值弹字**（一手 `Item::collect_time_orb th08:0x441348`
     * `CreatePlayerPointPopup(&item+0x2a4, value, color)`）—— 值 = `yk_orb_score()`（下限 100 / 上限 10000 /
     * 阶段门 2000），落点 = 那颗刻符的位置（一手就是道具位置，不换算）。*/
    /* ★★★ 2026-10-01（**owner 纠错**：「**收刻符应该没有分值浮字来的**」）—— **删掉这一笔**：
     *   一手 `CreatePlayerPointPopup`（`th08:0x441348`）是**点数道具（point item）**那条路；
     *   **刻符收集本身不出浮字**。我们这一笔是**自己加的、多余的**，而且正是把浮字池挤爆的元凶
     *   （实跑 `popup pool full, dropped 4740`）。⇒ 若将来要挂「点数道具出字」，**挂到那条路**上，
     *   **不要**挂在刻符上（`orb_taken` 仍照旧累加/计分，只是不再打浮字）。*/
    (void)x; (void)y;
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
            o->anm_id = YK_ENABLE_ORB_VM
                        ? ce_anm_spawn(yk_player_anm(), YK_ORB_SCRIPT[ch2], YK_ORB_ANM_LAYER) : 0u;  /* ★ ⑪① */
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
            if (o->vy > 0.0f) {
                /* ★★★ 2026-09-27（第三十九批）：**逢魔档的刻符「下落一小段之后直接消失」** ——
                 *   owner：「人逢魔生成的永点应该没有吸收过程……我目前能看见自动吸收过程」＋更早的
                 *   同一句口径「下落一小段之后**直接消失**（= 直接吃掉），而不是现在还走一个自动吸收
                 *   的中间路程」。⇒ 上抛结束这一帧**当场入账**（PIV / 槽 / 弹字全按正常收下那一套走，
                 *   见 `yk_orb_collect()`），跳过归航那一段。
                 * ⚠ 只对**出生时槽在逢魔档**的刻符生效（`eat_at_toss`）—— 普通击破批/螺旋/擦弹照旧
                 *   上抛 → 归航 → 被收（那几路的「看得见的吸收」是 owner 要的观感）。*/
                if (o->eat_at_toss) {
                    /* ★ owner 同一句的澄清：「**自动吸收过程是指飞过来，生成部分原版还是能看见的**」
                     *   ⇒ 我们要的正是这个：**上抛那一段照画**（VM 一直在画），**飞向自机那一段不画**
                     *   （上抛一结束就当场入账、VM 直接收掉）。下面这个计数就是实跑判据。*/
                    ++s_fengma_eaten;
                    yk_orb_collect(s, o->x, o->y);
                    yk_orb_kill(o);
                    if (s_orb_live) --s_orb_live;
                    ++taken;
                    continue;
                }
                o->state = YK_ORB_STATE_HOMING;
            }
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
    int k, k2, i;
    float th, cs, sn;
    if (!s || !s->spiral_quota) return;
    /* ★★★ 2026-09-27（第三十九批 · 逐指令复核后的订正）：**每帧都喷**，`t` = effect 的**帧龄**。
     *   原来这里写的是「每 8 帧一批」（把 `0x41751a push 8` 当成周期）—— 那是 `Timer::current_is_gt`
     *   的实参（一次性起跑门槛「> 8 帧」），于是我们把它做成了「每 8 帧在**一个角度**上叠一簇」
     *   ⇒ owner 看到的就是「**一股一股**」而不是螺旋。订正全文 = `youkai_core.h` 的
     *   `YK_SPIRAL_START_AGE` 那一段（含 `0x4066c0` 的 8 条指令）。*/
    ++s->spiral_t;                                   /* 帧龄（一手：effect Timer 每帧 +1）*/
    if (s->spiral_t <= YK_SPIRAL_START_AGE) return;  /* 一手 `0x41751a`/`:0x41752e`：> 8 才开喷 */
    /* ★★★ 2026-09-27（第四十三批 · owner「**螺旋改成画面中心**呢？」）：**不再挂 boss** ——
     *   锚点 = 游玩区正中 `(0, 224)`（ECL 坐标，与 boss 坐标同一套）。原来那句「没有 boss ⇒ 整轮
     *   作废」也随之删掉：实测就是它把 owner 那次螺旋整轮吃掉了
     *   （`spiral CANCELLED at batch 9 - no boss on the field (1000 orbs of quota left)`）。
     *   一手那条绕的是符卡圈 effect（`0x4175b0 add ecx,0x2e0`）—— 我们既没复刻它的轨迹，
     *   就按 owner 口径取画面中心。*/
    (void)boss;
    {
        static const float k_center[2] = { YK_SPIRAL_CENTER_X, YK_SPIRAL_CENTER_Y };
        bp = k_center;
    }
    yk_spiral_angle((int)s->spiral_t, &th);          /* `t` = **帧龄**（一手 `0x417565` 取的就是它）*/
    /* ⚠ 本函数**刻意不读 `s->gauge` / `focused` / `s->fengma`**（owner 第四十四批：「打 boss 的螺旋
     *   和逢魔状态没关系，是**固定撒的**」）—— 唯一读槽的地方是上面那行开喷日志（只为留现场）。*/
    /* 一帧的吐出与账面 = 一手 `0x4175cc`–`0x4176e1`（纯函数 + 单测在 core）：
     * 第一段 `min(额度,7)` 同角 + 第二段**固定 6** 角 +π；账面扣 `min(额度,7)` 两次（最多 14/帧）。*/
    s->spiral_quota = (uint32_t)yk_spiral_quota_step((int)s->spiral_quota, &k, &k2);
    cs = yk_cos_approx(th);
    sn = yk_sin_approx(th);
    for (i = 0; i < k; ++i)                          /* 第一段：同一角、k 颗 */
        yk_orb_spawn_ex(s, bp[0] + cs * YK_SPIRAL_RADIUS, bp[1] + sn * YK_SPIRAL_RADIUS,
                        YK_ORB_STATE_SPAWN5, 0);   /* ★ state 5：上抛但**可收**；`eat = 0` ⇒ **吸收可见** */
    /* 第二段：**固定 6 颗**（与额度无关）、角 = 第一段 **+π**（一手 `0x417639 push 0x40490fdb`）。*/
    cs = yk_cos_approx(th + 3.14159265f);
    sn = yk_sin_approx(th + 3.14159265f);
    for (i = 0; i < k2; ++i)
        yk_orb_spawn_ex(s, bp[0] + cs * YK_SPIRAL_RADIUS, bp[1] + sn * YK_SPIRAL_RADIUS,
                        YK_ORB_STATE_SPAWN5, 0);   /* ★ 同上：螺旋那一路**看得见飞向自机** */
    s_spiral_orbs += (unsigned)(k + k2);         /* ★ 第四十四批：判据用（与槽值无关）*/
    if (!s->spiral_logged) {
        s->spiral_logged = 1;
        /* ★★★ 2026-09-27（第四十四批 · owner：「打 boss 的螺旋和**逢魔状态没关系**，是**固定撒的**」）：
         *   这一路**不读 `s->gauge` / `focused` / `s->fengma`**（本函数里零引用，可 grep 自证）——
         *   撒点与吸收都是**固定**的：每帧 `min(额度,7)` 同角 + 固定 6 个 +π、锚点 `(0,224)`、
         *   半径 128、9°/帧、额度 1000 ⇒ 72 帧 ≈935 颗；吸收 = `eat_at_toss 0`（看得见飞向自机）。
         *   下面这行把**当时的槽值**也打出来 —— 只为留下「它与结果无关」的现场（值为多少都不影响）。*/
        ce_log("youkai: spell-card break spiral ON (TH08 `Spellcard::on_tick` 0x416b90) - FIXED pattern, "
               "independent of the fengma state (gauge=%d at start): EVERY FRAME min(quota,7) same-angle "
               "+ 6 at +pi, 9 deg/frame (40 frames per turn), radius 128 around the PLAYFIELD CENTRE "
               "(0,224); quota 1000 = 72 frames / ~935 orbs; orbs spawn in state 5 = toss and their "
               "absorption is VISIBLE (eat_at_toss hard-coded 0)",
               (int)s->gauge);
    }
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
#define YK_HUD_BLOCK_H  53.0f     /* ★★★ 2026-10-03z18（owner：「108 高度可以往里收几px
                                   *   （最终高度大概是现在的 107 108 平均即可）」）——107 高 42、108 原 64
                                   *   ⇒ 取平均 **53**。⚠ 只改**声明**：画法锚在槽的下沿 ⇒ 画面不动。*/

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
static int s_kill_mem_logged;       /* ★ 2026-10-01：首次「用了记忆值」打一行（同上）—— 判据：
                                     *   这一行出现 = 母机死时活子敌数 < 台账记录（人侧先清光使魔）。*/
static unsigned s_kill_entry_logged; /* ★ 2026-10-01：`yk_on_enemy_killed` 的入口判据行（定额 64 行）*/
/* ============================================================================================
 * ★★★ 2026-09-30（**owner 拍板**）：**死亡点吸收区**（TH08 `Enemy::FUN_0042adb0` 的那一圈）
 * ============================================================================================
 * 一手逐条见 `youkai_core.h` 的常量区 + `engine/item/th08/02-cancel-and-graze-to-time-orbs.md` §1 #5。
 * 设计（按 owner 工单）：**一套「区」的最小实现**，结构 = `{x, y, radius, life, hit_type}` + **一个池**；
 *   · 死亡点那一圈只是**这张池里的一个区**（`hit_type = 7`）；将来「炸弹片 / 自机吸收区」往
 *     **同一张 `hit_type → 道具` 表**里加即可（本批**只接 type 7**）。
 *   · **池满口径 = 一手**：TH08 的 type 7 软闸是「只试环状游标当前格、撞占用就**整批停**」
 *     （`th08:0x4401aa`）⇒ 我们照「**池满就停止本帧吐点**」写（**不是**拍一个 `min(N)`）。
 *   · 吐点 = **我们现有的刻符 VM**（`yk_orb_spawn` / `YK_ORB_*`，与击破撒的那一套**同一个池**）
 *     —— **不是** TH18 原生 item ⇒ PIV / 人妖槽 / HUD / 收集全通；一手也确认二者同种（只差 state）。
 *   · **仲裁**：本帧若**别人**（109/112/113/116/117…）已经拿了全屏消弹那一笔（`ce_cancel_latch_owner()`
 *     != -1 且不是我们），这一帧**我们不吃弹**；我们**不认领**那个闩锁（认领有「谁执行」的语义，
 *     认了会顶掉别人的消弹）⇒ 只**观察**，避免同一批弹在同帧被两方各算一次。
 *   · **收口**（铁律）：`yk_zone_clear_all()` 挂在 `yk_drop_all`（删卡 / 换关 / 局末 / 自机没了 /
 *     世界没了——所有退休路都过它）与 `on_ctor`；**已吐的刻符**在既有的刻符池里，`on_delete` 的
 *     `yk_orb_kill_all()` 已经管（本批不需要新增收尾）。*/
typedef struct {
    float    x, y;        /* 圆心（死亡点那一支 = 敌人死亡时的位置）*/
    float    radius;      /* 半径（死亡点那一支 = `YK_ZONE_DEATH_RADIUS` 32.5f）*/
    unsigned life;        /* 剩余帧（0 = 空槽）；死亡点那一支 = `YK_ZONE_DEATH_LIFE` 16u */
    int      hit_type;    /* `YK_ZONE_HIT_ORB`(7) = 每颗被吃的弹吐 1 颗刻符（**只接这一种**）*/
} yk_zone_t;
static yk_zone_t s_zone[YK_ZONE_MAX];
static unsigned s_zone_opened, s_zone_closed, s_zone_orbs, s_zone_pool_full;
static unsigned s_zone_bullets;                 /* 累计吃掉的敌弹数（台账用）*/
static unsigned s_zone_latch_skip;              /* ★ 因**别人**拿了本帧全屏消弹那一笔而整帧没吃弹的次数
                                                 *   （owner 2026-09-30「似乎没有消弹」的排查口径：
                                                 *   这条涨得快 ⇒ 我们的区被仲裁挡得多）*/

static void yk_zone_clear_all(void)
{
    unsigned i;
    for (i = 0; i < YK_ZONE_MAX; ++i) { s_zone[i].life = 0; s_zone[i].hit_type = 0; }
}

/* 开一个区。**池满 ⇒ 返回 0（不吐、不开）** —— 一手口径「满了就不吐」（不给 min(N) 那种截断）。*/
static int yk_zone_open(float x, float y, float radius, unsigned life, int hit_type)
{
    unsigned i;
    for (i = 0; i < YK_ZONE_MAX; ++i) {
        if (s_zone[i].life) continue;
        s_zone[i].x = x; s_zone[i].y = y;
        s_zone[i].radius = radius; s_zone[i].life = life; s_zone[i].hit_type = hit_type;
        ++s_zone_opened;
        return 1;
    }
    ++s_zone_pool_full;
    return 0;
}

/* 每帧：① 扫**在场敌弹**判「进没进区」② 进区 ⇒ 弹消失 + 吐 1 颗刻符 ③ 区寿命到就撤。
 * 开销：**只有当帧有活区时才扫池**（否则一次函数调用就返回 ⇒ 零开销）；
 *       池 2000 格 × 活区数（≤ `YK_ZONE_MAX`）的平方距离比较，只在开区后的 16 帧里发生。*/
/* ⚠ **`noinline` 是必需的**（2026-09-22 实测口径，见本文件别的注）：本函数内联着
 *   `ce_cancel_radius` 的内联汇编（`sdk.h:156`），i686 的寄存器不够 ⇒ 一旦被并进更大的函数
 *   就报 `inline assembly requires more registers than available`。*/
static void __attribute__((noinline)) yk_zone_tick(yk_card_t *s, uint32_t card_id, uint32_t frame)
{
    uint8_t *mgr = CE_BULLET_MGR();
    unsigned i, live = 0;
    int owner;
    if (!s) return;
    for (i = 0; i < YK_ZONE_MAX; ++i) if (s_zone[i].life) ++live;
    if (live && mgr) {
        /* ★ 仲裁：本帧别人拿了全屏消弹那一笔 ⇒ 我们**这一帧不吃弹**（只观察、不认领）。*/
        owner = ce_cancel_latch_owner();
        if (owner != -1 && owner != (int)card_id) ++s_zone_latch_skip;
        if (owner == -1 || owner == (int)card_id) {
            for (i = 0; i < YK_ZONE_MAX; ++i) {
                float cpos[3];
                int got;
                if (!s_zone[i].life) continue;
                /* ★★★ 2026-09-30（**P0 实跑 bug**，owner：「发射口使魔放出的弹幕视觉上不会被消弹，
                 *   但之后也不会再有任何判定」）—— **改走引擎自己的消弹入口**，不再手写 `state = 3`：
                 *   实测（实跑）写 3 只是**不再判定**（引擎认为它「已在消去中」，`cancel_all` 都跳过它）
                 *   而**不播消去视觉** ⇒ 观感 =「弹还在、但打不到」。
                 *   `ce_cancel_radius` = `th18:0x429370 cancel_radius_as_bomb`（`sdk.h` 的封装：
                 *   调前清 / 调后读 `CE_BM_CANCEL_COUNTER`，并自动登记进本帧消弹记录）；
                 *   **mode 0 = 消去但不吐原生道具**（一手 mode 表：0/3 不吐道具）⇒ 我们照旧吐
                 *   **自己的刻符**，一手口径不变；`max_count` 给满池尺寸 = 圈内不限颗数。
                 *   ⚠ 形状 = 「每帧一小圈、连续 `YK_ZONE_DEATH_LIFE` 帧」= 一手「16 帧内经过死亡点
                 *     的弹逐颗被吃」的同一形状；每颗被消的弹在**区心**吐一颗刻符。*/
                cpos[0] = s_zone[i].x; cpos[1] = s_zone[i].y; cpos[2] = 0.0f;
                got = YK_ENABLE_ZONE
                    ? ce_cancel_radius(cpos, s_zone[i].radius, (int)YK_ZONE_BULLET_SLOTS, 0)
                    : 0;                                  /* ★ 二分开关 ⑦：完全不清弹 */
                if (got > 0) {
                    /* ★ 一手：`hit_type == 9` ⇒ 进区的弹**每颗变 2 颗**刻符
                     *   （`th08:0x431634 cmpl $0x9,0x18b8988` 连发两次）；7 ⇒ 1 颗。*/
                    int mult = (s_zone[i].hit_type == YK_ZONE_HIT_ORB_DOUBLE) ? 2 : 1;
                    int oi;
                    s_zone_bullets += (unsigned)got;
                    s_zone_orbs += (unsigned)(got * mult);
                    for (oi = 0; oi < got * mult; ++oi)
                        yk_orb_spawn(s, s_zone[i].x, s_zone[i].y, YK_ORB_STATE_SPAWN);
                }
            }
        }
    }
    for (i = 0; i < YK_ZONE_MAX; ++i)
        if (s_zone[i].life && --s_zone[i].life == 0u) ++s_zone_closed;
    /* 台账：每 120 帧一行，且只在「这件事真的发生过」之后（不刷屏）。*/
    if ((s_zone_opened || s_zone_bullets) && (frame % 120u) == 0u)
        ce_log("youkai/zone: opened %u closed %u orbs %u bullets %u pool_full %u latch_skip %u (16-frame "
               "death-point absorb zones via `ce_cancel_radius` mode 0 => 1 own time orb per cancelled bullet; "
               "opened only when the dying enemy's familiar count != 0, `th08:0x42adc4`)",
               s_zone_opened, s_zone_closed, s_zone_orbs, s_zone_bullets, s_zone_pool_full,
               s_zone_latch_skip);
}

/* ★★★ 2026-09-30（**owner 口径**）：「我们 108 的 **boss 的发射口使魔原则上也应当是每段攻击
 *   killall 的**」——一手：TH08 的 boss 使魔**不常驻**，每段攻击开头 `enmKillAll()`（op95）清场、
 *   本波再 `familiarCreate`（`engine/enemy/th08/04-…md` §1）。
 * 我们**不改 boss 的符卡/ECL** ⇒ 由卡侧在**相位分界**模拟：把该 boss（含它的子敌人链）的
 *   映射使魔**撤壳 + 收跟随 VM + 清槽**（走既有 `yk_run_actions` / `YK_ACT_WITHDRAW` 那条路），
 *   然后**让既有映射逻辑在下一波自然重建**（**不手工建**）。
 * ⚠ 不碰绕转使魔（我们补的，与 boss 波次无关）；不碰非 boss 的映射使魔（只按 id 过滤）。*/
static void yk_boss_fam_rebuild(yk_card_t *s)
{
    uint8_t *em = CE_ENEMY_MGR();
    yk_action_t acts[YK_FAM_MAX];
    unsigned i;
    int total = 0, nb = 0;
    if (!s || !em) return;
    for (i = 0; i < 4u; ++i) {                                  /* `CE_EM_BOSS_IDS` = int[4] */
        int32_t boss = *(const int32_t *)(em + CE_EM_BOSS_IDS + (size_t)i * 4u);
        uint8_t *e;
        int n;
        if (!boss) continue;
        ++nb;
        n = yk_withdraw_enemy(&s_map, (uint32_t)boss, acts, (int)YK_FAM_MAX);
        if (n > 0) { yk_run_actions(s, acts, n); total += n; }
        /* 子敌人链（TH18 侧 = `parent_enemy_id`）：发射口实体常常是 boss 的子敌人 ⇒ 一并收。
         * ★ 2026-10-01：读 **zEnemy+0x6834**（`YK_ENEMY_PARENT_ID`）—— 旧代码读 `CE_SPAWN_PARENT`
         *   （= +0x50 = ECL 栈变量）⇒ 这一段几乎永不命中，boss 换段时那些子敌人的映射使魔收不掉。*/
        CE_ENEMY_FOREACH(em, e) {
            int32_t id, parent;
            if (!e) continue;
            id = *(const int32_t *)(e + CE_ENEMY_ID);
            if (!id) continue;
            parent = *(const int32_t *)(e + YK_ENEMY_PARENT_ID);
            if (parent != boss) continue;
            n = yk_withdraw_enemy(&s_map, (uint32_t)id, acts, (int)YK_FAM_MAX);
            if (n > 0) { yk_run_actions(s, acts, n); total += n; }
        }
    }
    if (total)
        ce_log("youkai: boss emitter familiars REBUILT for the new attack - withdrew %d familiar(s) "
               "across %d boss(es); the 1:1 map will re-create them in the next wave "
               "(TH08 `enmKillAll` op95 semantics, owner 2026-09-30; we do NOT modify boss ECL)",
               total, nb);
}

static void yk_on_enemy_killed(yk_card_t *s, const ce_enemy_death_t *ev)
{
    int focused, i, n2;
    int32_t hl, yl;                                     /* ★ P-E：当前形态的同一对 limit */
    if (!s || !ev) return;
    focused = yk_focused();
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
    /* ★★★ 2026-09-27（第三十七批 · owner：「**2 可以删**」）：逢魔击破那一颗的算式**回到一手**
     *   （`yk_fengma_kill_orbs()` 现在恒 1）—— 原来那个「人间逢魔 + 自机弹击杀 ⇒ 2 颗」是**近似**，
     *   一手那条「極人期间**每一发自机弹碰到敌机**就掉 1 颗」已经由 `on_bullet_hit`（新站点
     *   `ce_bullet_hit`）按**真颗粒度**实现了 ⇒ 再留这一颗就是**重复计数**。
     *   一手出处：`th08:0x42da2a`/`:0x42da38` 两问取或（極人或極妖）⇒ `:0x42da41 push 1` /
     *   `:0x42da54 spawn_item(type 7,...)` **一颗**；那条 +1 是**另一条来源**（`th08:0x45187f`）。*/
    {
        int fm = yk_fengma(s->gauge, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS);
        int norbs = yk_fengma_kill_orbs(fm, (s->gauge <= YK_GAUGE_HUMAN_EFFECTS) ? 1 : 0, 0);
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
    /* ★★★ 2026-09-27（第三十七批 · **owner 2026-09-27 拍「2 可以删」**）：**删掉了这里那句
     *   `if (!focused) return;`**（「妖逢魔才撒刻符」）。它是一道**一手没有的形态门**：
     *   · 一手死亡路径 `th08:0x42d625` → `:0x42d62f je 0x42d652` → **`:0x42d652 push 1` /
     *     `:0x42d657 call 0x42adb0`**（两者之间**没有任何指令** ⇒ 无条件）→ `Enemy::sub_42adb0`
     *     体内的刻符落点 `:0x42b03d push 7` / `:0x42b048`、`:0x42b1a6`/`:0x42b1b1`、`:0x42b1b8`；
     *     那几处的门是**机体 / boss / 符卡**（`:0x42aece call 0x42f270` solo_youkai、
     *     `:0x42aefd call 0x42f230` solo_human、`:0x42b054 call 0x42f1f0` is_boss_active、
     *     `:0x42b062 call 0x4178a0` 符卡中）—— 而 `player+3` 的 **8 个读点**（`0x42c6d7`/`0x42d197`/
     *     `:0x42d1c5`/`:0x42d217`/`:0x42d65c`/`:0x44054d`/`:0x440838`/`:0x4413a5`）**没有一个落在这段里**
     *     ⇒ **掉这几颗刻符与形态无关**。
     *   · 后果（删之前）：人侧（松 Shift / 高速）击破**一颗都不撒** —— 而 owner 的 §6 口径
     *     「人侧和不装 108 完全一样」说的是**自机发弹**，不是「本卡自己的永点系统在人侧消失」。
     *   ⇒ 删掉之后：人侧击破也会撒（一手行为）。判据 = 台账里的永点计数在人侧也涨。
     * 逐条取证与影响面：`AUDIT.md` §BE #138。*/
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
        /* ★★★ 2026-10-01（**三本数分开** —— 修上一个实跑 bug 的核心）：
         *   · `n_child` = **这一帧还有几条活着的子敌人**（有位置的那几台）。**位置循环只用它**
         *     （`cx[]/cy[]` 只有这么多格被写过 ⇒ 用它当上界才不会读未初始化栈）；
         *   · `n` = `max(n_child, 声明过的发射口槽)` —— 「这一刻已知的使魔数」（旧版的 `n`）；
         *   · `n_mem` = **记忆值** = `max(n, 台账里记的「当初真建了几台」)` —— **浮字的值与
         *     爆点颗数公式的 `n`**（owner 口径 2026-10-01；纯函数 `yk_kill_display_count`）。
         * ⚠ 旧版把 `n` 同时当「值」与「位置上界」用 —— 上面那句 `n = slots` 之后 `nc = min(n,…)`
         *   会把只写过 `n_child` 格的坐标栈按 `slots` 读 ⇒ **未初始化栈**（本批一并修掉）。*/
        unsigned n_child = 0, n = 0, n_mem = 0, k;
        uint8_t *em = CE_ENEMY_MGR();
        uint8_t *e = 0;
        if (em) {
            CE_ENEMY_FOREACH(em, e) {
                /* ⚠ **不过 `ce_enemy_lockable`**：一手数的是「链上还在的结点」，与能不能锁定无关
                 *   （TH18 的 boss 部件常常不可锁定；用可锁定当筛子会漏数、颗数公式就跟着偏）。*/
                if (!e) continue;
                /* ★★★ 2026-10-01（**P0 根因**）：读的是 **zEnemy+0x6834**（`YK_ENEMY_PARENT_ID`）——
                 *   旧代码写 `e + CE_SPAWN_PARENT`（= `e+0x50` = ECL 栈里的一个局部变量）⇒ 这一支
                 *   永远数不到子敌人 ⇒ `n == 0` ⇒ 整块被门跳过 ⇒ **两侧都没有 +N / 消弹**。
                 *   实跑证据（2026-10-01）：母机带 11 台使魔、`popup spawn` 仍是 0 行。*/
                if (*(const int32_t *)(e + YK_ENEMY_PARENT_ID) != (int32_t)ev->id) continue;
                /* ★★★ 2026-09-28（第四十九批）：**惰性绕转使魔也是这里的合法子敌人**
                 *   （它们的父字段就是父敌 id，见 `yk_orbit_*` 那一节）。
                 *   ⇒ **不许**在这一支里按「是不是我们造的使魔」过滤掉它们 —— 那正好会把这一批
                 *   换来的颗数抹掉（一手：颗数 = 使魔数的函数，`th08:0x41fd40` / `:0x42adc4` 的门）。*/
                ++n_child;                                  /* 计数不封顶（颗数公式要真个数）*/
                if (n_child <= YK_KILL_CHILD_MAX) {
                    const float *cp = ce_enemy_pos(e);
                    cx[n_child - 1] = cp ? cp[0] : ev->x;
                    cy[n_child - 1] = cp ? cp[1] : ev->y;
                    /* ★★★ 2026-09-30（一手 `Enemy::FUN_0042adb0` 的循环体）：**每个子机位置也开一个
                     *   吸收区** —— `FUN_0044df00(child_final_pos, R=32.5f, 2.03f, **寿命 8**,
                     *   hitType = (被杀者 `flags_lo` bit1 ? 7 : 9))`。区结构复用同一个池
                     *   （`yk_zone_open`）；**hit type 9 = 进区的弹每颗变 2 颗刻符**
                     *   （`th08:0x431634 cmpl $0x9,0x18b8988` 连发两次）⇒ 已在 `yk_zone_tick` 里实现。*/
                    yk_zone_open(cx[n_child - 1], cy[n_child - 1], YK_ZONE_DEATH_RADIUS, YK_ZONE_CHILD_LIFE,
                                 (*(const uint32_t *)((const uint8_t *)ev->enemy + 0x3324) & 0x2u)
                                     ? YK_ZONE_HIT_ORB : YK_ZONE_HIT_ORB_DOUBLE);
                }
            }
        }
        /* ★★★ 2026-09-27（第四十六批 · owner：「不同小怪爆的永点数量似乎应当是不一样的」）：
         *   颗数公式吃的是「**这个敌人的使魔数**」（一手 `th08:0x41fd40` = ExpHP 的
         *   `__possibly__get_familiar_count`）。我们原来只用「自己真建出来的活子敌人个数」
         *   ⇒ 与敌人重合、按规则不建使魔的那些发射口**一颗都不算** ⇒ 不同小怪之间的差别被压平。
         *   现在取 **max(活子敌人, 声明过的发射口槽)**：后者才是「这个敌人本来有几台使魔」。*/
        n = n_child;
        {
            unsigned slots = yk_map_enemy_slots(&s_map, ev->id);
            if (slots > n) {
                if (!s_kill_slots_logged) {
                    s_kill_slots_logged = 1;
                    ce_log("youkai: kill burst count uses the enemy's DECLARED emitter slots "
                           "(enemy #%u: %u slot(s) vs %u live child part(s)) - TH08's count formula "
                           "reads `Enemy::sub_41fd40` = the familiar count (`th08:0x42adc4` gate + "
                           "`2n+10` cap 26), so different enemies give different amounts",
                           ev->id, slots, n);
                }
                n = slots;
            }
        }
        /* ★★★ 2026-10-01（**记忆值**，owner 口径）：人侧玩家**先把使魔打光、再打死母机**时
         *   `n == 0` ⇒ 下面整块被 `if (!n2)` 跳过 ⇒ 连 `+N` 都没有（owner 实跑 `popup spawn` 0 行）。
         *   台账（`yk_orbit_gift_mark` 记的 `made`）知道「这台母机当初发了几台」⇒ 拿它兜底。
         *   ⚠ 只影响**值 / 公式 / 开区**；位置循环仍按 `n_child`（有几个位置画几个位置）。*/
        {
            /* ★ P82：记忆值取**绕转台账与映射台账的较大者** —— owner 2026-10-15 报的
             *   「**时序问题**：被杀那一刻被判成没有使魔 ⇒ 没有 +N」正是映射那一支缺台账 ✗。*/
            unsigned mem_o = yk_orbit_gifted_count(ev->id);
            unsigned mem_m = yk_map_gifted_count(ev->id);
            n_mem = yk_kill_display_count(n, (mem_m > mem_o) ? mem_m : mem_o);
        }
        if (n_mem != n && !s_kill_mem_logged) {
            s_kill_mem_logged = 1;
            ce_log("youkai: kill burst uses the REMEMBERED familiar count (enemy #%u: %u live / "
                   "declared, %u remembered => n = %u) - the familiars were already dead when the "
                   "mother died, so the +N value and the orb formulas take max(live, remembered); "
                   "positions still come from the live children only (owner ruling 2026-10-01)",
                   (unsigned)ev->id, n, (unsigned)yk_orbit_gifted_count(ev->id), n_mem);
        }        /* ★★★ 2026-09-30（**owner 拍板**）：**死亡点吸收区** —— 一手 `th08:0x42adc4 je 0x42b1e5`
         * 的门就是这里这个 `n`（使魔数）⇒ **`n > 0` 才开**（没使魔的小怪死了什么都不开）；
         * 开的同时给 **1 颗刻符**（`th08:0x42b1a4 push $1 / push $7`，位置 = 死亡点），
         * 再开 **R=32.5f / 16 帧 / hit type 7** 的圆型区（`th08:0x42b1b8` + `th08:0x42b1d4 call 0x44df00`）。
         * ★ 2026-10-01：门改用 `n_mem`（母机死亡与**侧**无关：妖侧使魔无敌 ⇒ 活着 ⇒ `n > 0`；
         *   人侧先清光使魔 ⇒ 靠记忆值仍 `> 0`）—— 这条正是 owner「妖侧打死带使魔的敌人也要有消弹」。*/
        if (n_mem > 0) {
            /* ★ 2026-09-30 订正：这里**不再**额外给 1 颗 —— 一手链根那一支是
             *   「popup + **`2 × subEnemyCount`** 颗（下面那一环）+ **在链根位置开区**」，
             *   没有「单独的 1 颗」；那 1 颗**属于使魔/有主那一支**（`th08:0x42b1a4 push $1`，
             *   在 `0x42adc4 je 0x42b1e5` 的**目标**里），我们已经在 `!n2 && parent != 0` 那支实现。
             *   ⇒ 这里只开区（R=32.5 / 16 帧 / hit type 7 = 一手链根那一个）。*/
            yk_zone_open(ev->x, ev->y, YK_ZONE_DEATH_RADIUS, YK_ZONE_DEATH_LIFE, YK_ZONE_HIT_ORB);
        }
        n2 = (int)yk_kill_burst_count(n_mem);
        /* ★★★ 2026-10-01（**入口判据**，parent 工单）：这一行就是「死亡处理到底看到了什么」——
         *   `side` = 本帧聚不聚焦（妖侧 = 1）、`live` = 扫到的活子敌数、`slots` = 声明槽、
         *   `mem` = 台账记忆值、`n` = 门与公式用的数、`burst` = 颗数（0 = 门拦下）。
         *   判据（2026-10-01 owner 实跑 `popup spawn` 0 行）：
         *     · 没有这一行 ⇒ `on_enemy_death` 根本没走到这只敌人（kind / 使魔表 / 早退）；
         *     · 有这一行且 `live > 0` 但 `burst == 0` ⇒ **子敌识别**出错（父字段读错/表不对）；
         *     · 有这一行且 `burst > 0` ⇒ 浮字/消弹**必然**发生（下面 else 支）。
         *   ⚠ 定额 64 行后收口（本卡有过 1.5 MB/min 日志洪水事故，`AUDIT.md` §BE #143）。*/
        if (s_kill_entry_logged < 64u) {
            ++s_kill_entry_logged;
            ce_log("youkai/kill: enemy #%u kind %u side %d (1 = youkai/focused) live %u child part(s), "
                   "%u declared slot(s), %u remembered => n %u, burst %d%s",
                   (unsigned)ev->id, (unsigned)ev->kind, focused, n_child, n,
                   (unsigned)yk_orbit_gifted_count(ev->id), n_mem, n2,
                   n2 ? " (popup + death zone below)" : " (gated - no popup, no zone)");
        }
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
            /* ★ 2026-10-01：**位置上界 = 活子敌**（`n_child`）—— 有几个位置就画几个位置；
             *   值/公式用 `n_mem`（见上面那三本数的注）。母亲死时使魔已被打光 ⇒ `nc == 0`
             *   ⇒ 没有子机白字、也没有每盘那一批；只有链根黄字 + 链根自己那一环（`2 × n_mem`）。*/
            unsigned nc = (n_child > YK_KILL_CHILD_MAX) ? YK_KILL_CHILD_MAX : n_child;
            if (!s_kill_burst_logged) {
                s_kill_burst_logged = 1;
                ce_log("youkai: kill burst ON - enemy #%u: %u live child part(s) / %u declared slot(s), "
                       "value n = %u (max of live and REMEMBERED) => "
                       "%d orbs per part (TH08 `Enemy::FUN_0042adb0`: shottype branch n>=%u ? %u : "
                       "2n+10), scattered in a disc of radius 2N around EACH part; plus %u ring orbs "
                       "(state 1, radius %.0f) around the enemy itself",
                       (unsigned)ev->id, n_child, n, n_mem, n2,
                       (unsigned)YK_KILL_BURST_CAP_N, (unsigned)YK_KILL_BURST_CAP,
                       (unsigned)yk_kill_ring_count(n_mem), (double)YK_KILL_RING_RADIUS);
            }
            for (k = 0; k < nc; ++k) {                      /* 一手：每个链结点各撒 N 颗（`0x42aff0`）*/
                /* ★★★ 2026-09-30 **复刻一手**：`CreateTimePopup`（`th08:0x403460`）在**每个子机**那一步
                 *   各冒一个数字（一手调用点 `th08:0x42af78`，位置 = **该子机的 final_pos**）。
                 *   数值 = **与该子机同一次撒的颗数**（`n2`，与道具用同一个数，不另算）。
                 *   🟡 一手该 popup 的实参是不是「颗数」我没有逐条读出来（push 序列未读到）⇒
                 *      暂按「同一次撒的颗数」记，等一手复核再改。*/
                /* ★ 一手（§BE #163）：子机那笔显示的是 **`n` = 那一窝使魔总数**（`GetSubEnemyChainCount(链根)`、
                 *   **不是**每盘撒的颗数 `n2`！）⇒ 颜色按 n 四档（白 → 微黄），字号 1.0。
                 * ★ 2026-10-01：这里的值改用 `n_mem`（max(活子敌, 记忆值)，owner 口径）。*/
                yk_popup_spawn(cx[k], cy[k], (int32_t)n_mem, 1,
                               (n_mem < 2u) ? 0xffffffffu : (n_mem <= 5u) ? 0xffffffd0u
                                            : (n_mem <= 9u) ? 0xffffffb0u : 0xffffff80u, 1.0f, "part");
                for (i = 0; i < n2; ++i) {
                    float ang, rad;
                    /* ⚠ 两个随机数**先取到局部**（一手顺序是「先半径 `0x40d390`、后角度 `0x4143c0`」；
                     *   写成函数实参求值顺序在 C 里未定义 ⇒ 会破坏回放确定性）。*/
                    /* ★★★ 2026-09-30 **纠错**（`AUDIT.md` §BE #161）：一手是「角度 `rand(0..2N)`、
                     *   幅度 **±3.17 px**」= **堆在一点**（「爆一堆」的密堆感）；我们原来把**角度范围
                     *   当成了半径**（`yk_orb_burst_polar(n2,…)` ⇒ 圆盘半径 2N）。⇒ 幅度固定 3.17。
                     *   ⚠ 量纲：TH08 与 TH18 世界层同一套 ECL 单位 ⇒ 3.17 **直接照抄**。*/
                    float rr = (float)(ce_rand() % 1000u) / 1000.0f;   /* → 半径 ∈ [0, 3.17) */
                    float ra = (float)(ce_rand() % 1000u) / 1000.0f;   /* → 角 ∈ [−π, π) */
                    yk_orb_burst_polar_range(YK_KILL_BURST_SPREAD, ra, rr, &ang, &rad);
                    yk_orb_spawn(s, cx[k] + yk_cos_approx(ang) * rad,
                                    cy[k] + yk_sin_approx(ang) * rad,
                                 YK_ORB_STATE_SPAWN);   /* 一手 `0x42b048`：state 3（上抛，期间收不到）*/
                }
            }
            /* 被杀者自己那一环（一手 `0x42b13b`–`0x42b1b1`）：`subEnemyCount * 2` 颗、**state 1**、
             * 半径上界 **128**（`0x42b15d` —— ⚠ 同样走 `GetRandomF32InRange` ⇒ **圆盘**，不是固定环）。*/
            /* ★★★ 2026-09-30 **复刻一手**：链根那一步也冒一个数字（一手调用点 `th08:0x42b122`，
             *   位置 = **链根死亡点**）。数值 = **`2 × subEnemyCount`**（= 一手这一支撒的颗数
             *   `2 × subEnemyCount`，我们这里就是 `yk_kill_ring_count(n)`，同一个数不另算）。
             *   🟡 一手 `0x42b122` 的实参同样未逐条读出（push 序列未读到）⇒ 暂按「这一支撒的颗数」记。*/
            /* ★ 一手（§BE #163）：链根那笔 = **黄 `0xfff0f00f` + 字号 2.03125**（`0x42b0ef` 先 SetScale、
             *   `0x42b127` 复位），值 = **`+0x3380` = subEnemyCount**（还剩几台使魔）。
             * ⚠ 取值时机：一手在**子机循环之后**读 `+0x3380`（`th08:0x42b10d`）；我把全 `.text` 的
             *   `+0x3380` 写点过了一遍 —— **没有任何 `--`**（写点只有三处 opcode 生成时的 `+1` 与一处拷贝），
             *   且死亡函数 `0x42ad*`–`0x42b3*` 区间里**没有减** ⇒ 循环后 = 循环前 = 我们的 `n` ✓
             *   （我们的 `n` 也是「扫活子敌人」得来、循环里不递减）。
             * ★ 2026-10-01：值改用 `n_mem`（含记忆值）—— 母机死时使魔已被打光也照出这一笔。*/
            if (n_mem > 0) yk_popup_spawn(ev->x, ev->y, (int32_t)n_mem, 1, 0xfff0f00fu, 2.03125f, "root");
            /* ★★★ 2026-10-01（owner：「**缺少打死 +N 的 UI**」）：上面那笔 = **链根母机**的大黄字；
             *   这里补 **每台「绕转使魔」位置的小白字**（一手 `th08:0x42af78` 的对应物，色随 n 分档）
             *   —— 原来浮字只挂在**映射使魔**（ECL 子敌人）那条循环上，**绕转使魔一笔都没有**。*/
            {   unsigned oi;
                for (oi = 0; oi < YK_ORBIT_MAX; ++oi) {
                    yk_orbit_t *ob = &s_orbit[oi];
                    uint8_t *of;
                    const float *op;
                    if (!ob->fam_id || ob->parent_id != ev->id) continue;
                    of = ce_enemy_by_id(CE_ENEMY_MGR(), (int32_t)ob->fam_id);
                    op = of ? ce_enemy_pos(of) : 0;
                    if (!op) continue;
                    yk_popup_spawn(op[0], op[1], (int32_t)n_mem, 1,
                                   (n_mem < 2u) ? 0xffffffffu : (n_mem <= 5u) ? 0xffffffd0u
                                                : (n_mem <= 9u) ? 0xffffffb0u : 0xffffff80u, 1.0f, "orbit");
                }
            }
            for (k = 0; k < yk_kill_ring_count(n_mem); ++k) {
                float ang, rad;
                /* ★ 2026-09-30 同上纠错：一手链根那一环也是**幅度 ±3.17**（不是半径 128）。*/
                float rr = (float)(ce_rand() % 1000u) / 1000.0f;       /* → 半径 ∈ [0, 3.17) */
                float ra = (float)(ce_rand() % 1000u) / 1000.0f;       /* → 角 ∈ [−π, π) */
                yk_orb_burst_polar_range(YK_KILL_BURST_SPREAD, ra, rr, &ang, &rad);
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
         *   的差别已登记 `TUNING.md` / `AUDIT.md` §BE #114。
         * ★★★ 2026-10-01：这一格读的是 **`YK_ENEMY_PARENT_ID`（zEnemy+0x6834）** —— 旧代码在这里写
         *   `ev->enemy + CE_SPAWN_PARENT`（= +0x50，ECL 栈变量）⇒ 这一支**时灵时不灵**（撞上 ECL
         *   变量的随机值才成立）。修对之后：**时间轴建的普通敌机一律拿到这 1 颗**（上面那条口径
         *   第一次真正生效）。
         * ★★★ 2026-09-28 第三轮补的一手对照（使魔那一类的口径）：TH08 的使魔 `drop_main`
         *   **80% 是 `-2`（不掉道具）**，但**被自机击破时固定掉 1 刻符**（`th08:0x42b29b` = 这一支）
         *   ⇒ 我们补的**惰性绕转使魔**被打死时走的正是这一支（`AUDIT.md` §BE #150 §6：
         *   **盘不爆、单颗给**）。*/
        if (!n2 && ev->enemy &&
            *(const int32_t *)((const uint8_t *)ev->enemy + YK_ENEMY_PARENT_ID) != 0) {
            /* ★ 2026-09-30：**A 的旋钮**（`YK_ORBIT_KILL_ORBS`，默认 1 = 一手）。
             *   一手 = 「被杀者自己有主 ⇒ `spawn_item(type 7)` 一颗」（`th08:0x42b1e5` 起 /
             *   `th08:0x42b29b`：使魔被自机击破固定 1 颗）+ 「槽 -= 槽/12」（我们没做）。
             *   owner 若觉得「不够多」，**只改这一格**（❓口径，不是复刻）。*/
            unsigned ko;
            for (ko = 0; ko < YK_ORBIT_KILL_ORBS; ++ko)
                yk_orb_spawn(s, ev->x, ev->y, YK_ORB_STATE_SPAWN);   /* ★ §BE #117：type 7 ⇒ state 3 */
        }
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
        id = YK_ENABLE_HUD_VM ? ce_anm_spawn_ui(yk_player_anm(), YK_GAUGE_SCRIPT[ch][i], 0) : 0;
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
        id = YK_ENABLE_HUD_INIT ? ce_anm_spawn_ui(yk_player_anm(), YK_GAUGE_SCRIPT_FONT[ch], 0) : 0;  /* ⑬ */
        s->hud_digit[i] = id;
    }
    /* ★★★ 2026-09-22 夜（#27/#28）：**夜点数字的 8 格**（一手里它与百分比共用**同一个 VM**、逐位换 sprite；
     * TH18 一个 VM 一帧只画一次 ⇒ 照样开 8 格，见 `yk_night_text_sync`）。整块的门照抄：
     * `hud_ready` 为 0 时这一整批（条/图标/游标/百分比/夜点）**一起不建**。*/
    for (i = 0; i < YK_NIGHT_TEXT_MAX; ++i) {
        uint32_t id;
        if (s->hud_night[i] && ce_anm_get_vm(s->hud_night[i])) continue;
        id = YK_ENABLE_HUD_INIT ? ce_anm_spawn_ui(yk_player_anm(), YK_GAUGE_SCRIPT_FONT[ch], 0) : 0;  /* ⑬ */
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
        /* ★★★ 2026-10-03z12（owner：「**就差人妖槽的数字百分比以及最大得点没有压暗**」）——
         *   原来这里**每帧硬写 alpha = 0xff** ⇒ 我在 `yk_gauge_hud_update()` 里加的压暗
         *   会被这一行**覆盖**。⇒ alpha 改用**共享带**（`ce_hud_band_dim_alpha()`），
         *   RGB 仍走那 5 档染色（`col`）。*/ce_anm_set_color(id, ((uint32_t)ce_hud_band_dim_alpha() & 0xffu) << 24 | (col & 0x00ffffffu));           /* 一手：数字同吃那 5 档染色 */
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
    /* ★★★ 2026-09-27（第四十批 · owner：「这个值应该**只考虑 th08 所导致的**额外最大得点」）：
     *   画**本卡自己的额外量** = `piv − piv_base`（难度初值那一格是基准）——
     *   · 这同时解决了 retry 残留那条（`piv`/`piv_base` 都跟着卡状态复位 ⇒ 开局必然 `000000`，
     *     不再依赖共享池的状态）；共享池那一份改由 SDK 在 retry/退场时清（§BE #139 §1）。*/
    n = yk_night_text_glyphs(s->piv - s->piv_base, g);
    for (i = 0; i < YK_NIGHT_TEXT_MAX; ++i) {
        uint32_t id = s->hud_night[i];
        if (!id) continue;
        if (i >= n) { ce_anm_set_color(id, 0x00ffffffu); continue; }     /* 这一格不用 ⇒ 透明 */
        if (anm) ce_anm_set_sprite(anm, id, YK_GAUGE_FONT_BASE[ch] + (int)g[i].glyph);
        ce_anm_set_pos(id, h->night_x + g[i].dx * h->px, h->night_y + g[i].dy * h->px, 0.0f);
        ce_anm_set_scale(id, g[i].scale * h->px, g[i].scale * h->px);
        /* ★ z12：同款 —— 夜点/最大得点的 alpha 也走共享带（原来硬写 0xff 覆盖压暗）*/ce_anm_set_color(id, ((uint32_t)ce_hud_band_dim_alpha() & 0xffu) << 24 | (col & 0x00ffffffu));
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
     * ★★★ 2026-09-26（owner #39）**曾经**取「游玩区左下角」`(257, 928)`（= 一手换算：TH08 槽条
     *   x = 32 = TH08 游玩区左沿 ⇒ TH18 表面 257；y 基准 = 游玩区下沿 464 ⇒ 928）。
     * ★★★ 2026-09-30（**owner 口径 · 第九轮**：「**左下角 HUD 位置不对，得再往左**，参考 **117、112、
     *   113 的左下角位置**，那个才是**真正的画面左下角**。**你别管**你觉得可能什么会进入黑屏区域还是
     *   什么的，**你就按照那个调就是了**。」）⇒ **x 改成 68**（那三张卡的**左边缘**那条竖线），
     *   见下面 `YK_GAUGE_BASE_X` 的逐条算法；**y 保持 928**（本来就在同一条底线上，见 `…_Y`）。
     *   这是**owner 口径覆盖**了 #39 那条「游玩区左下角」的推导（#39 的三条依据仍留在 `yk_gauge_hud()`
     *   里解释 y 的基准订正：`928 − (464−449)×2 = 898` 那段与本次无关，继续有效）。*/
#define YK_GAUGE_BASE_X    68.0f        /* ★★★ 第九轮：整块**左边缘**对齐到 117/112/113 那条竖线。
                                         *   三个参考点（都取**左边缘**，不是数字的字形 x）：
                                         *     · **112**（`ufo.c`）：`UFO_HUD_PIN_X = 248 + UFO_HUD_OWNER_DX_SURF
                                         *       (−180) = 68`，块 132×32 ⇒ 左沿 **68**；
                                         *     · **117**（`animal.c`）：pin = `ANIMAL_HUD_PIN_X = 88`、声明块
                                         *       193×48（左沿 88）；但那条 pin 是**首格格心**
                                         *       （`ANIMAL_HUD_HALF = 24`）⇒ **可见左沿 = 64**；
                                         *     · **113**（`trance.c`）：`TRANCE_HUD_BASE_X = 248`
                                         *       （一手 TH13 根 x = −196 ⇒ 640 + (−196)×2 = 248 = **场地左沿
                                         *       256 外 8 px**），块 144×96 ⇒ 左沿 **248**。
                                         *   ⚠ 三者不是严格同一条线（64 / 68 / 248）⇒ 取 **68**：
                                         *     = 112 的块左沿 = **原生「当前卡」UI 的左沿**（112 的注释写着
                                         *       「原生当前卡 UI = 表面 x **68..116**」⇒ 系统槽本来就该放这一带），
                                         *     距 117 的可见左沿 4 px、在 113 左侧 180 px。
                                         *   **一行可回退**：改回 `257.0f` = 上一版（= 游玩区左沿）。
                                         *   ⚠ 本卡**定位不吃货架值**（`(void)s->hud`）⇒ 改锚点不动认领；
                                         *     但可见范围会横跨 112/117/113 三块（owner：「别管」）——
                                         *     重叠明细登记在 `AUDIT.md` §BE #150 第九轮。*/
/* ★★★★ 2026-10-03z56（owner：「我所谓的 108 往上是**都往上，包括声明块**」）——
 *   上一版我只抬了**画面** ✗，因为块归分配器管。根因：本卡 pin = (68, 928) 的块
 *   （68..324 × 875..928）与**原生卡面**（68..116 × 864..924）**y 上重叠** ⇒ pin 恒被拒 ⇒
 *   块走货架（924）。⇒ 现在把 **pin 挪到卡面右沿之外**（x = **124** = 卡面右沿 116 + 间隙 8 ✓）
 *   并把 y 取 **920**（= 下沿 928 − 8 ✓）⇒ **pin 被接受** ⇒ **块与画面一起上移 8px** ✓✓。
 *   （间隙 8 = `CE_HUD_GAP`；x 取 124 也与"货架在卡面在场时的起点"一致 ✓ ⇒ 观感不变、只是抬高。）*/
#define YK_HUD_PIN_X      124.0f        /* ★ z56：卡面右沿(116) + 间隙(8) ⇒ pin 不再压卡面 ✓ */
#define YK_HUD_PIN_Y      920.0f        /* ★ z56：owner 的"整体上移 8px"（旧货架位 924）*/
#define YK_GAUGE_BASE_Y   928.0f        /* = **游玩区下沿**（= TH08 640 空间的 464）。
                                         * ★ 第九轮 **y 不动**：它已经与 **112**（`UFO_HUD_PIN_Y = 928`）
                                         *   和 **113**（`TRANCE_HUD_BASE_Y = 928`）**同一条底线**；
                                         *   117 的 `ANIMAL_HUD_PIN_Y = 924` 是**格心** pin
                                         *   （`ANIMAL_HUD_DY = −20` ⇒ 格心 y = 904、块 y 876..924），折算到
                                         *   「块左下角」也在 928 一带。owner 这单只说「往左」⇒ y 保持。*/
    /* ★★★ 2026-10-03y：落点 = **分配器给的槽**（`s->hud`）。
     *   因为本卡现在是 **PINNED 在 (68,928)**（= owner 第九轮明令的那个位置），
     *   所以**单开时 `s->hud` 恒等于 (68,928) ⇒ 画的位置一个像素不变**；
     *   只有**撞上别人/原生卡面**、分配器把它回退到货架时，整块才跟着让开。
     *   （空槽兜底也在认领处：拿不到时 `s->hud` = 声明锚点。）*/
    /* ★★★ 2026-10-03z54（owner：「**确实 108 那还是往上来吧**」）——
     *   108 的墨迹底部溢 8px（子智能体审计：数字画到 900、块底 892）⇒ **整体上移 8px**。
     *   ⚠ 为什么不用 pin 的 y：本卡 pin 块（68..324）与**原生卡面**（68..116 × 864..924）
     *     在 y 上重叠 ⇒ **pin 永远被拒、回退货架**（台账长期是 `(124,924)`）⇒ 改 pin 的 y
     *     **不生效** ✗。这里改成**画面锚点偏移**：pin 路径与货架路径都上移 ✓。
     *   微调口径：**−1 = 上移 1px**（就下面这一个数）。旧值留档：0（贴那条底线）。*/
#define YK_HUD_ART_DY  (0.0f)          /* ★ z56：**归零** —— owner 要的是"**连声明块一起上移**"，
                                        *   而块归分配器管 ⇒ 改成让 **pin 本身被接受**（见 `YK_HUD_PIN_X/Y`），
                                        *   这样块与画面一起动 ✓；这里就不需要"只抬画面"的临时手段了。*/
    if (s->hud_ready && s->hud.w > 0.0f) {
        anchor[0] = s->hud.x;
        anchor[1] = s->hud.y + YK_HUD_ART_DY;      /* ★ z54：画面专用上移 */
    } else {
        anchor[0] = YK_GAUGE_BASE_X;
        anchor[1] = YK_GAUGE_BASE_Y + YK_HUD_ART_DY;
    }
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
    /* ★ z15（owner：「108 声明的矩形可能比 UI 本身**靠左**」）：墨迹探针（节流：槽位变化时一行）*/
    {
        static float s_px = -1.0f, s_py = -1.0f;
        if (s_px != s->hud.x || s_py != s->hud.y) {
            int q3;
            s_px = s->hud.x; s_py = s->hud.y;
            ce_log("youkai: hud slot (%.0f,%.0f) w=%.0f h=%.0f — 逐 VM 墨迹：",
                   (double)s->hud.x, (double)s->hud.y, (double)s->hud.w, (double)s->hud.h);
            ce_hud_vm_probe("youkai.bar", s->hud_bar);
            ce_hud_vm_probe("youkai.human", s->hud_human);
            ce_hud_vm_probe("youkai.youkai", s->hud_youkai);
            ce_hud_vm_probe("youkai.cursor", s->hud_cursor);
            for (q3 = 0; q3 < YK_GAUGE_TEXT_MAX; ++q3)
                if (s->hud_digit[q3]) ce_hud_vm_probe("youkai.digit", s->hud_digit[q3]);
        }
    }
    /* ★★★ 2026-10-03z9（owner 实跑：「**108 和 109 以及原生目前没有进入同时黑暗**」；
     *   审计也指出本卡「无压暗消费者」）——本卡的四支 VM 是靠 `interrupt(1)` **显形**、
     *   颜色里没有 alpha 通道可写 ⇒ 现在每帧把**共享带**的 alpha 写进颜色
     *   （`0x00ffffff` = 白色 = **不动贴图本身的 RGB**，只改 A）。*/
    {
        const uint32_t a = (uint32_t)ce_hud_band_dim_alpha() & 0xffu;
        uint32_t *ids[4];
        int q;
        ids[0] = &s->hud_bar;  ids[1] = &s->hud_human;
        ids[2] = &s->hud_youkai; ids[3] = &s->hud_cursor;
        for (q = 0; q < 4; ++q)
            if (*ids[q] && ce_anm_get_vm(*ids[q])) ce_anm_set_color(*ids[q], (a << 24) | 0x00ffffffu);
        /* ★ 2026-10-03z11（owner：「**108 人妖槽的数字部分也没有接入**」）——
         *   百分比/夜点的**字形 VM**（`s->hud_digit[]`）与上面四支同款处理。*/
        for (q = 0; q < YK_GAUGE_TEXT_MAX; ++q)
            if (s->hud_digit[q] && ce_anm_get_vm(s->hud_digit[q]))
                ce_anm_set_color(s->hud_digit[q], (a << 24) | 0x00ffffffu);
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

static void yk_hud_claim(ce_card_t *c, yk_card_t *s);   /* ★ z7：定义在本文件后面（on_load 之后）*/
/* ★★★ 2026-10-14（P31a，owner 口径逐字：「所有东西**除了一个之外**换面不清空：**单面的夜点计数** …
 *   **总夜点计数和刻符没有理由清空**」✓）：**run 级字段在换面时原样带回** ✓。
 *   做法（最稳的一种，不靠"猜哪一行清的" ✗）：`on_ctor` 在**换面**这一路上把 run 级字段先存进
 *   局部、跑完原有初始化、再写回 ✓ —— 于是无论中间哪一支抹了它们，出来都是上一关的值 ✓；
 *   新局照旧（`s_run_fresh` = 1 ✓）。
 *   ⚠ **不**恢复的东西：HUD 字形 VM 句柄（`hud_digit[]` / `hud_night[]` ✓）—— 引擎换关会拆掉
 *     UI VM ✓，恢复了就是死句柄 ✗ ⇒ 让 `yk_gauge_hud_kill()` 与重建那一支照旧跑 ✓。
 *   ⚠ owner 说的「**单面的夜点计数**」= 每关那一格（TH08 里给 LS 奖励符卡用 ✓，本 mod 没有它 ✓）
 *     ⇒ 本批**不特意保留**（它本来就是每关一份 ✓）；其余（总夜点 / 刻符 / 人妖槽 / 各项计数）全部带回 ✓。*/
typedef struct {
    int32_t  piv, piv_base, orbs, gauge;
    uint16_t gauge_focus_run, gauge_hold, orb_peak, gauge_ram, gauge_idle, orb_silence;
    uint32_t orb_taken, orb_full;
} yk_run_keep_t;
static yk_run_keep_t s_run_keep108;
static int           s_run_fresh = 1;   /* 1 = 下一次 on_ctor 属于「新局」（on_run_reset / on_delete 置回 1 ✓）*/

static void yk_run_save108(const yk_card_t *s)
{
    s_run_keep108.piv = s->piv;               s_run_keep108.piv_base = s->piv_base;
    s_run_keep108.orbs = s->orbs;             s_run_keep108.gauge = s->gauge;
    s_run_keep108.gauge_focus_run = s->gauge_focus_run; s_run_keep108.gauge_hold = s->gauge_hold;
    s_run_keep108.orb_peak = s->orb_peak;     s_run_keep108.gauge_ram = s->gauge_ram;
    s_run_keep108.gauge_idle = s->gauge_idle; s_run_keep108.orb_silence = s->orb_silence;
    s_run_keep108.orb_taken = s->orb_taken;   s_run_keep108.orb_full = s->orb_full;
}

static void yk_run_restore108(yk_card_t *s)
{
    s->piv = s_run_keep108.piv;               s->piv_base = s_run_keep108.piv_base;
    s->orbs = s_run_keep108.orbs;             s->gauge = s_run_keep108.gauge;
    s->gauge_focus_run = s_run_keep108.gauge_focus_run; s->gauge_hold = s_run_keep108.gauge_hold;
    s->orb_peak = s_run_keep108.orb_peak;     s->gauge_ram = s_run_keep108.gauge_ram;
    s->gauge_idle = s_run_keep108.gauge_idle; s->orb_silence = s_run_keep108.orb_silence;
    s->orb_taken = s_run_keep108.orb_taken;   s->orb_full = s_run_keep108.orb_full;
}

static int on_ctor(ce_card_t *c)
{
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_LIFECYCLE) return 0;   /* ★ 总开关 + 回调族 ⑨ */
    yk_card_t *s = ce_state(c, yk_card_t);
    {   /* ★ P31a：换面 ⇒ 出来时把 run 级字段写回上一关的值 ✓ */
        yk_run_keep_t keep108;
        const int was_fresh108 = s_run_fresh;
        s_run_fresh = 0;
        if (!was_fresh108) keep108 = s_run_keep108;
    if (!s) return 0;
    /* ★ 先收跟随 VM 再复位静态表：本函数**在局中被重建时也会跑**（每关开始引擎对卡组每张卡
     * 再调一次 ctor）—— 那时 `yk_map_reset` 会把使魔一并忘掉，它们身上的 VM 若不复位就永久留下
     * （`s_fam_vm` 是文件级静态，不是卡状态）。句柄为空时是纯 no-op。*/
    yk_fam_vm_kill_all();
    /* ★★★ 2026-09-28（第四十九批）：绕转使魔同理（文件级静态表；`on_run_reset` 那条路已经过
     *   `yk_drop_all`，这里是**再兜一次** —— ctor 在局中每关都会被调，且它是真敌人，漏一只就是游荡空壳）。*/
    yk_orbit_kill_all();
    yk_zone_clear_all();                  /* ★ 同上的区池（每关 ctor 会重跑）*/
    /* ★ 2026-09-27 第二十六批：卡侧束的句柄也是**文件级静态**（每关 ctor 会重跑）⇒ 一并收干净，
     * 否则换关后那支 VM 会永远钉在旧位置（`METHOD.md` 铁律二：长持续效果必须交代被删掉会怎样）。*/
    yk_beam_kill(0);
    yk_map_reset(&s_map);
    __builtin_memset(s_fired_enemy, 0, sizeof s_fired_enemy);
    __builtin_memset(s_fired_slot, 0, sizeof s_fired_slot);
    s_fired_head = 0;
    yk_transfer_reset(&s->tr);
    {   /* ★ 2026-09-30：外部/绕转使魔的转嫁采样表也随卡重来一起清（文件级静态）。*/
        unsigned xi;
        /* ★ 2026-10-18：清台账之前先把接管来的使魔**还原**（本体原色 + 收环）—— 否则换关后
         *   它们永久隐形（它们不是我们的实体，`yk_orbit_kill_all` 管不到）。*/
        yk_xfam_vm_kill_all();
        for (xi = 0; xi < YK_XFAM_MAX; ++xi) __builtin_memset(&s_xfam[xi], 0, sizeof s_xfam[xi]);
        s_xfam_used = 0;
    }
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
    s->gap_releases_seen = 0;           /* ★ owner ②：同上（只增不清 ⇒ 快照回 0）*/
    s_side_lock = -1;                   /* ★ owner ④：reset ⇒ 解锁（不再钉住形态）*/
    s_bomb_prev = 0; s_bomb_is_deathbomb = 0;
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
    /* ★★★★ 2026-09-27（第四十五批 · **owner 实跑：「刚一进游戏怎么撒了螺旋」**）：**极性写反了** ——
     *   触发器是 `if (s->spell_was && !sp)`（「上一帧在符卡里、这一帧不在」= **下降沿**），
     *   而这里原来初始化成 **1**（注释还写着「开场当作已经在符卡外 ⇒ 不会误触发一次」—— 意图对、
     *   取值刚好相反）⇒ **开场第一帧**（`sp == 0`）就被当成「符卡刚结束」⇒ 立刻开一轮螺旋。
     *   以前看不出来是因为那条「没有 boss ⇒ 整轮作废」把它咽掉了（`spiral CANCELLED … no boss …`），
     *   而第四十三批为 owner 的口径把作废删了、锚点改成画面中心 ⇒ 这个假触发就露出来了
     *   （实测日志：`spiral ARMED (spell ended)` 紧跟着 `spell probe end - flags 0x0 id 0 …`，
     *   即**没有任何符卡**却报了「结束」）。
     *   ⇒ 正确初值 = **0**（「上一帧不在符卡里」）⇒ 只有真正的符卡→非符卡边沿才会开螺旋。*/
    s->spell_was = 0;
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
    s->gauge_focus_prev = (uint8_t)(yk_focused() ? 1 : 0);
    s->graze_seen = 0;
    s->orb_taken = 0;
    s->orb_full = 0;
    s->orb_peak = 0;
    /* 人妖槽 HUD：认领一块左下角货架（每关开场重排；认领幂等）。抢不到就不画（0 = 没抢到）。*/
    {
        ce_hud_block_t blk;
        __builtin_memset(&blk, 0, sizeof blk);
        blk.w = YK_HUD_BLOCK_W;
        blk.h = YK_HUD_BLOCK_H;
        /* ★★★ 2026-10-03y（goal「所有左下角 HUD 进同一个分配器」）——
         *   本卡原来走**货架**（`blk.side = 0`）却**不拿货架值定位**（`yk_gauge_hud_update()` 里
         *   写死 `YK_GAUGE_BASE_X/Y = 68/928`，那是 owner 2026-09-30 第九轮明令的位置）
         *   ⇒ 结果是"认领归认领、画归画"：分配器把块排到 257+，画仍在 68 ⇒ 看着"堆在左下角、
         *   不听分配器"（owner 十二合一实跑）。
         *   ⇒ 改成 **PINNED 在 owner 那个位置 (68, 928)**：单开时槽 = pin ⇒ 画的位置**一个像素不变**；
         *     与别人/原生卡面**撞了**才回退货架，那时 `s->hud` 变了、画也跟着走（见 `…_update()`）。*/
        blk.side = CE_HUD_SIDE_PINNED;
        yk_hud_claim(c, s);
    }
        if (!was_fresh108) yk_run_restore108(s);
    }
    if (s->hud_ready) yk_gauge_hud_spawn(c, s);
    return 0;
}

/* ★★★ 2026-10-03z7（子智能体审计 P1-1）——**认领抽成一支**，`on_load` 与每帧自愈共用。
 *   本卡原来只在 `on_load`（每关一次）认领；而**纪元清表**（十二合一中盘发卡那批 / 换关竞争）
 *   之后表里就没有我们了 ⇒ 原来会整关停在兜底常量、不进系统（117 的实跑日志
 *   `wba_wc: hud geom … path=fallback` 就是同一现象）。*/
static int s_hud_sel_seen = -1;   /* ★ 2026-10-15：货架诊断限频（结论变化才打一行 ✓）*/
static void yk_hud_claim(ce_card_t *c, yk_card_t *s)
{
    ce_hud_block_t blk;
    if (!s) return;
    __builtin_memset(&blk, 0, sizeof blk);
    blk.w = YK_HUD_BLOCK_W;
    blk.h = YK_HUD_BLOCK_H;
    blk.side = CE_HUD_SIDE_PINNED;                 /* 非 pinned 会被 pinned 入口拒掉 */
    s->hud_ready = (uint8_t)(ce_hud_slot_claim_pinned_card(
        ce_card_id(c), &blk, YK_HUD_PIN_X, YK_HUD_PIN_Y, &s->hud) ? 1 : 0);
    /* ★ 2026-10-15（owner 报「108 + 77 + 71 同装 ⇒ 左下角**留出了主动卡的位置**」✗ 的**只读**诊断 ✓）：
     *   打一行「引擎判据 `mgr+0x38` vs 货架剩余宽」——有卡面 ⇒ 起点 124 ⇒ 宽 **700** ✓；
     *   没有 ⇒ 起点 68 ⇒ 宽 **756** ✓（底边右端 `CE_HUD_X_MAX` = 824 ✓，算式见 `sdk_core.h` 的让位段 ✓）。
     *   A/B 一眼分清：
     *     · `+0x38 == NULL` 而宽是 700 ⇒ **陈旧** ✓（已由 `sdk.c` 帧初的每帧 `ce_hud_card_refresh()` 修掉 ✓）；
     *     · `+0x38 != NULL` ⇒ **判据语义**问题（引擎对"非主动卡"也留了对象 ✗）⇒ 那就要换成
     *       读当前卡的 `category == 0` ✓（`cards_def.c:93` 把 category 写在表行 `+0x0c` ✓，
     *       卡对象 `+0x4c` 指向表行 ✓ —— 一手 `engine/card/th18/15-selected-card-hud.md:74` ✓）。
     *   ⚠ 本段只读 ✗、不写引擎 ✓；定位完成后可整段删掉 ✗（它不进卡面文案 ✓）。*/
    {
        void *sel = CE_PLAYER()
            ? *(void *const *)(CE_ABILITY_MGR() + CE_MGR_SELECTED_ACTIVE) : 0;
        float rw = 0.0f;
        int present = hud_card_present(sel) ? 1 : 0;
        ce_hud_row_remaining_w(&rw);
        if (present != s_hud_sel_seen) {
            s_hud_sel_seen = present;
            ce_log("youkai: 货架诊断 · mgr+0x38=%p present=%d 剩余宽=%.0f（700=让位 / 756=不让位）",
                   sel, present, rw);
        }
    }
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
    unsigned n = 0;                     /* ★ z59：`i` 随全池扫描一起去掉（改走活弹链表）*/
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
    /* ★★★ 2026-10-12z59（goal「扫描全部敌弹抽离+加速」第 2 步 · 第一张卡）——
     *   老写法：**扫 2000 槽静态池**（`mgr + CE_BM_BULLETS + i*CE_BULLET_STRIDE`，
     *   每槽 0xfa0 步长 ⇒ 一次全扫摸 ~8MB ✗），且判据 `state == 0` 只跳空槽
     *   ⇒ **`state == 5` 的池尾哨兵（假弹，下标 1997–1999）也会被当成弹** ✗
     *   （实跑对账已坐实：`链表数 ≡ 直方图[1] 数`，差的恰好是 `[5]` 那一个 ✓）。
     *   新写法：走 **`mgr+0xbc` 活弹链表**（`CE_BULLET_LIVE_FOR_EACH`）⇒
     *     · 只摸**活弹**（O(活弹) 而不是 O(2000)）✓；
     *     · 结构上**不可能碰到哨兵** ✓（链表里只有真弹）。
     *   ⚠ **行为微变**（已登记待 owner 过目）：不再给哨兵染色 ✓（它本来就不是弹）。*/
    CE_BULLET_LIVE_FOR_EACH(b) {
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

/* ★★★ 2026-10-01（owner 反复出现「游戏到底载入的是哪版 DLL」的疑问）：**编译期构建戳**。
 *   `__DATE__` / `__TIME__` 是**编译期常量**（不用运行时算 md5），每次编出来的 DLL 都不同 ⇒
 *   实跑日志里搜 `youkai: build` 就能确认版本。判据见 `HANDOFF-108.md`。*/
#define YK_BUILD_STAMP __DATE__ " " __TIME__

/* ★ 2026-10-19（owner 拍板 B）：**符卡练习里自动装上 108** ✓
 *
 * 为什么需要它（一手机制，见 `engine/card/th18/02-lifecycle.md` §6）：
 *   练习模式开局同样走 `AbilityManager__reset_cards 0x407DA0`，而它从**存档里那个槽**重建卡组
 *   （`SCOREFILE+0x5f678+slot*4` / `SCOREFILE+0x5f608+slot*0x10+i`）——owner 的**练习槽只有 113/118**
 *   ⇒ 108 根本不在卡链表里 ⇒ 本卡的 `ctor/on_tick_2` **一次都不会被引擎调到** ✗ ⇒ 使魔接管没法测。
 *   ⇒ 只能在「**与卡组无关**」的帧点上把 108 发到手上（调用点 = `sdk.c` 的 `BP_ce_item_frame`）。
 *
 * 走的是**商店同一条路**：`ce_give_card(id, CE_MODE_SHOP, 0)` = `AbilityManager__allocate_new_card`
 *   `0x411460` 的 mode 2（`cards/pot.c` / `cards/give12.c` 已有先例 ✓）⇒ 只往**本局**的
 *   AbilityManager 加一张卡，**不写存档卡组** ✓、不动卡组槽 ✓、不碰主游玩路径 ✓
 *   （判据 = 符卡练习 + 关卡里有自机 ✓，见下方 2026-10-19b 订正）。
 *
 * ★★★ 2026-10-19b 判据订正（一手；owner 实跑 01:57 那局**一行 `practice auto-equip` 都没有** ⇒ 旧判据错）：
 *   ❌ 旧判据 `CE_PRACTICE_STAGE() >= 0`（`0x4c5f8c`）**不是**练习判据 —— 那个全局是 **Extra 单关局标记**
 *      （唯一正写点 `th18:0x46d9cb` 写 1，全二进制只有 4 处清 -1，其中一处正是**关卡练习开局** `0x46769f`；
 *       ECL 侧只有 `st07`/`st07bs` 读系统变量 -9899 = 本地址）⇒ 符卡练习里它恒为 -1 ⇒ 永不触发。
 *   ✅ 真判据（两条都要）：
 *      ① `0x4cf438 == 5` —— 本局启动方式 = 符卡练习（唯一写点 `th18:0x46cfce`；4 = 关卡练习 `0x4676ac`，
 *         2 = 回放 `0x46803f`，6 = Extra `0x46d9a6`）；
 *      ② `0x4ccd14 >= 0` —— 本局要练的**符卡 id**（唯一非负写点 `th18:0x46cffe`；普通/回放/Extra 都写 -1）。
 *         这是 ECL 自己用的量：`st01bs…st07bs.ecl` 逐关读系统变量 **-9907** = 本地址，用来跳到要练的那张符。
 *      🟡 兜底一条（只为防「启动方式被改写 ⇒ 功能静默失效」重演）：`(GAME_FLAGS 0x4cccc8 & 0x30) == 0x20`
 *         且 `0x4ccd14 >= 0` 且不在回放中（`GT+0xd0 == 0`）。`0x20` 是引擎自己的练习位
 *         （`ReplayManager__initialize th18:0x461a0d..0x461a2f` 按 ② 置/清；商店 `0x417cf5`、MSG `0x43e423` 都读它）。
 *   📋 每次启动方式变化打一行只读探针 `youkai: practiceprobe cand0=…`，下一局日志即可核对三个候选值。
 *
 * 为什么不在 `on_ctor` / `on_tick_2` 里调：那两个回调**本身就属于这张卡** —— 108 不在练习卡组里时
 *   它们不存在（这正是本条要绕开的循环）⇒ 必须由帧点从外面叫进来。
 *
 * 幂等 / 只试一次（owner 口径「失败不重试」✓）：
 *   · `s_practice_added` = 本局已经成功发到手上（首发 / 被 `reset_cards` 抹掉后的**一次**补发）；
 *   · `s_practice_failed` = 发过一次且失败 ⇒ 本局**不再试** ✓（日志里 `r=0` 即判据）；
 *   · **离开符卡练习**（判据不再成立）⇒ 全部复位，下次进来可以再试；
 *   · 练习里**重开同一关 / 重选关**：`CE_TIME_IN_STAGE()` 会倒退（`th18:0x44442f` 那个清零点，
 *     与 `cards/mof.c` 的练习重开判据同一出处）⇒ 倒退就复位 ⇒ 第二局照样装得上 ✓；
 *   · `ce_owned(108)` 已经是 1（练习槽本来就带了 108）⇒ 不重复发 ✓；
 *   · ⚠ **补发一次的理由**：卡组是在关卡开场由 `reset_cards` 建的；若那一帧的时序是「我们先发、
 *     reset 后跑」，108 会被它整副删掉（它清 `owned[]` 再从存档重建）⇒ 只在「我们**成功**发过、
 *     现在 `owned[108]` 又变回 0」时补发 **一次**（`2` = 已补过，之后不再追）。这是同一局里唯一
 *     会把 108 弄丢的路径（练习里商店 30 帧自动退、没有成交 ⇒ 没有删卡路径）。 */
static int      s_practice_added;      /* 0=没发过 / 1=首发成功 / 2=被 reset 抹掉后已补发 */
static int      s_practice_failed;     /* 本局发过一次但失败 ⇒ 不再试 */
static int32_t  s_practice_last_t;     /* 上一次看到的 stage 帧号（倒退 = 新一局 / 重选关）*/

/* ---- 只读探针：启动方式一变就打一行，把三个候选值一起落日志（一手候选表见函数上方的订正段）----
 * 为什么需要它：上一版判据错得很安静 —— 符卡练习里它恒为 -1 ⇒ `yk_practice_equip_once` 逐字 no-op、
 * **一行日志都没有** ⇒ 实跑日志无法区分「没进练习」和「判据错」。这行探针就是那条缺失的证据。*/
static void yk_practice_probe(int32_t sid)
{
    static int32_t s_cand0_prev = 0x7fffffff;   /* 上一帧的 `0x4cf438`（启动方式）*/
    static int     s_is_practice_prev;
    const int32_t  cand0 = CE_START_KIND();               /* 0x4cf438：5 = 符卡练习 */
    const uint32_t cand1 = CE_PRACTICE_MODE();            /* 0x4cccc8 & 0x30：0x20 = 符卡练习 */
    const int32_t  cand2 = sid;                           /* 0x4ccd14：本局练的符卡 id，-1 = 不是 */
    const int32_t  cand3 = CE_EXTRA_RUN();                /* 0x4c5f8c：1 = Extra 单关局（旧判据用的就是它）*/
    const int is_practice = (cand0 == CE_START_KIND_SPELL_PRACTICE) && (cand2 >= 0);

    if (cand0 == s_cand0_prev && is_practice == s_is_practice_prev) return;
    s_cand0_prev = cand0;
    s_is_practice_prev = is_practice;
    ce_log("youkai: practiceprobe cand0=0x4cf438=%d cand1=0x4cccc8&0x30=0x%x cand2=0x4ccd14=%d "
           "cand3=0x4c5f8c=%d stage=%d practice=%d",
           (int)cand0, (unsigned)cand1, (int)cand2, (int)cand3,
           (int)CE_CURRENT_STAGE(), is_practice);
}

/* ✅ 符卡练习判据（一手见上方订正段）：
 *   ① `0x4cf438 == 5`（本局由符卡练习菜单启动）且 ② `0x4ccd14 >= 0`（真有选中的符）+ 关卡里有自机；
 *   🟡 兜底：`GAME_FLAGS & 0x30 == 0x20` 且 `0x4ccd14 >= 0` 且不在回放中 —— 只在 ① 被改写时救急用。*/
static int yk_in_spell_practice(int32_t *sid_out)
{
    const int32_t sid = CE_SPELL_PRACTICE_ID();
    uint8_t *gt;
    *sid_out = sid;
    if (sid < 0) return 0;                              /* 没选符 ⇒ 一定不是符卡练习（② 是必要条件）*/
    if (CE_START_KIND() == CE_START_KIND_SPELL_PRACTICE) return 1;   /* ① 主判据 */
    gt = CE_GAME_THREAD();
    if (gt && *(int32_t *)(gt + CE_GT_REPLAY_PLAYING) != 0) return 0; /* 回放里不发卡（会改回放轨迹）*/
    return (CE_PRACTICE_MODE() == CE_PRACTICE_MODE_SPELL) ? 1 : 0;    /* 🟡 兜底 */
}

/* ★★★ 2026-10-19f（owner：「只要加载了我们的 DLL，使魔就开始转嫁血量，这个可能吗？」）——**可能**。
 *
 * 为什么可能：本 DLL 里有一个**与卡组无关**的每帧帧点 = `BP_ce_item_frame`（`sdk.c`），
 *   它同时调 `yk_practice_equip_once()`（那一支在 108 不在卡组时也照跑，已实跑证过 ✓）
 *   ⇒ 只要 **DLL 在场**，就能在没有 108 的情况下替他们那批使魔做 1/2 转伤。
 *   （**没有 DLL** 当然做不到 —— 转伤是用 `ce_damage_rect` 建的伤害源，那是 DLL 侧的东西。）
 *
 * 口径：**只做转伤**。外观切换 / 藏本体 / 拖尾 / NOFIRE 仍归卡内（那些是「装上 108」才有的表现）。
 *   判据 / 喂料 / 落点与卡内那条**逐条同源**：
 *     · 他们那场 = boss 的 ECL `subroutine_index >= 24`（`yk_le_here()`，按面粘性）；
 *     · 他们使魔 = 主 anm 脚本 ∈ {28,29}（`familiar_visuals.py:39` 的 CHILDREN 口径）或 `$I3 == 997`；
 *     · 1/2 转给**母机**（`parent_enemy_id`），母机查不到退回全局 boss，都没有才丢；
 *     · 时符 / 母机无敌期 ⇒ 整笔丢进 `dropped`（与卡内同一条硬约束）。
 *   ⚠ **幂等**：`ce_owned(108)` 为真 ⇒ 整段 no-op（卡内那条已经在跑，绝不双计）。
 *   ⚠ 台账复用 `s_xfam`（模块级，不是卡状态）⇒ 同一条使魔在两条路里是同一个条目。
 *   ⚠ 节拍用 `ce_sdk_frame()`（卡不在时 `s_ledger_frame` 根本不涨）。
 *   ⚠ 调用点必须在「自机 + AbilityManager 都在」之后（`ce_owned()` 不解空指针）。*/
static yk_transfer_t s_le_tr;
static unsigned s_le_dropped, s_le_rect_ok, s_le_noboss;
static uint32_t s_le_parent;

static void __attribute__((noinline)) yk_le_transfer_standalone(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e, *boss;
    int le_stage;
    uint32_t fr, i;

    if (!em) return;
    if (ce_owned(108u)) return;                  /* 卡在手上 ⇒ 卡内那条在跑，绝不双计 ✓ */
    fr = ce_sdk_frame();
    if ((fr & 1u) != 0u) return;                 /* 与卡内同节拍：每 2 帧扫一次 */
    boss = yk_boss();
    if (boss && yk_le_here()) s_le_seen_stage = CE_CURRENT_STAGE();
    le_stage = (yk_le_here() || (s_le_seen_stage == CE_CURRENT_STAGE())) ? 1 : 0;
    if (!le_stage) return;                       /* 不是他们那场 ⇒ 一个字节都不碰 ✓ */

    for (i = 0; i < YK_XFAM_MAX; ++i) s_xfam[i].seen = 0;
    CE_ENEMY_FOREACH(em, e) {
        uint32_t id, parent;
        int32_t life, anm0, iv3;
        yk_xfam_t *x;
        if (!e) continue;
        id = *(const uint32_t *)(e + CE_ENEMY_ID);
        if (!id) continue;
        anm0 = *(const int32_t *)(e + CE_ENEMY_DATA + 0x26c);
        iv3  = yk_le_iv3(e);
        if (!(anm0 == YK_LE_FAM_ANM_A || anm0 == YK_LE_FAM_ANM_B || iv3 == YK_LE_FAM_MARK))
            continue;                            /* 只认他们的使魔（两根判据，与卡内同源）*/
        parent = (uint32_t)*(const int32_t *)(e + YK_ENEMY_PARENT_ID);
        if (!parent) {                            /* 没有母机 ⇒ 落点 = 当前 boss（一手语义）*/
            if (!boss) continue;
            parent = *(const uint32_t *)(boss + CE_ENEMY_ID);
        }
        if (boss) yk_le_fam_raise_hp(e, boss);   /* ★ 2026-10-19g：血量提到我们 108 的规则 */
        life = *(const int32_t *)(e + YK_ED_LIFE_CUR);
        x = yk_xfam_find(id);
        if (!x) {                                 /* 新条目 ⇒ 只登记，不喂（第一帧没有差分）*/
            unsigned k;
            if (s_xfam_used >= YK_XFAM_MAX) continue;
            for (k = 0; k < YK_XFAM_MAX; ++k) if (!s_xfam[k].id) break;
            if (k >= YK_XFAM_MAX) continue;
            x = &s_xfam[k];
            __builtin_memset(x, 0, sizeof *x);
            x->id = id;
            x->le = 1;
            x->last_log_frame = fr - YK_XFAM_LOG_EVERY;
            ++s_xfam_used;
            ce_log("youkai/leveledit: TAKEOVER (no-108 path) familiar id=%u anm0=%d iv3=%d -> "
                   "boss id=%u (1/2 transfer; card 108 not owned)", id, (int)anm0, (int)iv3,
                   (unsigned)parent);
        } else if (x->life > life) {
            int32_t d = x->life - life;
            s_le_parent = parent;
            yk_transfer_feed(&s_le_tr, d, YK_TRANSFER_PCT_MAP);    /* 1/2（与卡内同一条 ✓）*/
        }
        x->life = life;
        x->seen = 1;
    }
    for (i = 0; i < YK_XFAM_MAX; ++i) {           /* 查不到 ⇒ 删条目（不因血量没变删）*/
        if (s_xfam[i].id && !s_xfam[i].seen) {
            __builtin_memset(&s_xfam[i], 0, sizeof s_xfam[i]);
            --s_xfam_used;
        }
    }
    if (s_le_tr.pending <= 0) return;
    {   int why = yk_xfer_suppress_reason();      /* 时符 / 无敌期 ⇒ 整笔丢（与卡内同一条）*/
        if (why) { s_le_dropped += (unsigned)s_le_tr.pending; s_le_tr.pending = 0; return; } }
    {
        uint8_t *pe = s_le_parent ? ce_enemy_by_id(em, (int32_t)s_le_parent) : 0;
        const float *pp2 = pe ? ce_enemy_pos(pe) : 0;
        uint8_t *tgt = pe;
        const float *bp2 = pp2;
        if (!bp2) {                                /* 母机查不到 ⇒ 退回全局 boss */
            uint8_t *b2 = yk_boss();
            bp2 = b2 ? ce_enemy_pos(b2) : 0;
            tgt = b2;
        }
        if (!tgt || !bp2) {
            ++s_le_noboss;
            s_le_dropped += (unsigned)s_le_tr.pending;
            s_le_tr.pending = 0;
            return;
        }
        {
            int amount = yk_transfer_clamp(s_le_tr.pending, YK_TRANSFER_CAP_PER_FRAME);
            s_le_tr.pending = 0;
            if (amount <= 0) return;
            ce_raise_damage_cap(yk_base_damage_cap() + amount);
            if (ce_damage_rect(bp2, 0.0f, YK_TRANSFER_SRC_LIFE, (int)amount,
                               YK_TRANSFER_BOX_W, YK_TRANSFER_BOX_H) > 0) {
                if (++s_le_rect_ok <= 12u)
                    ce_log("youkai/leveledit: transfer applied (no-108 path) boss id=%u amount=%d "
                           "(rects=%u dropped=%u noboss=%u)",
                           (unsigned)*(const uint32_t *)(tgt + CE_ENEMY_ID), amount,
                           s_le_rect_ok, s_le_dropped, s_le_noboss);
            }
        }
    }
}

void yk_practice_equip_once(void)
{
    int32_t t, sid = -1;
    int r;

    /* 菜单 / 结算 / 未开局：没有自机或没有 AbilityManager ⇒ 什么都不做（也不算失败）*/
    if (!CE_PLAYER() || !CE_ABILITY_MGR()) {
        s_practice_added = 0;
        s_practice_failed = 0;
        return;
    }
    /* ★ 2026-10-19f：**没 108 时的 leveledit 使魔转伤**（与卡组无关；108 在手上时它整段 no-op）*/
    yk_le_transfer_standalone();
    yk_practice_probe(CE_SPELL_PRACTICE_ID());
    if (!yk_in_spell_practice(&sid)) {  /* 主游玩 / 关卡练习 / 回放 ⇒ 逐字 no-op */
        s_practice_added = 0;
        s_practice_failed = 0;
        return;
    }
    t = CE_TIME_IN_STAGE();
    if (t < s_practice_last_t) {       /* 计时倒退 = 重开 / 重选关 ⇒ 新的一局 */
        s_practice_added = 0;
        s_practice_failed = 0;
    }
    s_practice_last_t = t;
    if (ce_owned(108u)) {              /* 已在手上 ⇒ 不重复发 ✓（只记一次日志）*/
        if (!s_practice_added) {
            s_practice_added = 1;
            ce_log("youkai: practice auto-equip 108 skipped (already owned, spell=%d)", (int)sid);
        }
        return;
    }
    if (s_practice_failed || s_practice_added >= 2) return;   /* 失败不重试 / 补发过不再追 */
    {
        const int regive = (s_practice_added == 1);   /* 成功发过、现在又没了 = reset_cards 抹的 */
        if (regive) {
            s_practice_added = 0;
            ce_log("youkai: practice auto-equip 108 was rebuilt away by reset_cards - re-giving once");
        }
        r = ce_give_card(108u, CE_MODE_SHOP, 0);
        if (r) s_practice_added = regive ? 2 : 1;
        else   s_practice_failed = 1;
        ce_log("youkai: practice auto-equip 108 (ce_owned=%d, r=%d, spell=%d)",
               (int)ce_owned(108u), r, (int)sid);
    }
}

static int on_load(ce_card_t *c)
{
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_LIFECYCLE) return 0;   /* ★ 总开关 + 回调族 ⑨ */
    static int s_stamp_logged;
    if (!s_stamp_logged) {
        s_stamp_logged = 1;
        ce_log("youkai: build " YK_BUILD_STAMP " (card 108; compile-time stamp - check this line "
               "FIRST in any field report)");
    }
    return on_ctor(c);
}

static void on_run_reset(ce_card_t *c)
{
    s_run_fresh = 1;   /* ★ P31a：新局 ✓ */
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_LIFECYCLE) return;   /* ★ 总开关 + 回调族 ⑨ */
    yk_card_t *s = ce_state(c, yk_card_t);
    /* ★ 2026-10-01（owner：这次发版就为拿这个数）—— **emitkey 收尾汇总**：
     *   `reappeared > 0` ⇒ 同一 `(enemy,slot)` 关掉后又出现 ⇒ **序号稳**（③ 可按现有身份做）；
     *   `reappeared == 0 && new_after_off > 0` ⇒ 我们只看到「新 slot 号」 ⇒ **序号漂**（③ 要换键）。*/
    /* ★ 2026-10-01：**无条件打**（`pairs 0` 也打）—— 并带上 `acts`（本局扫到过多少次活跃槽）：
     *   这样即使一次转换都没有，也能区分「**根本没扫到槽**」与「扫到了但没有 关→开 转换」。*/
    ce_log("youkai/emitkey: pairs %u seen %u reappeared %u new_after_off %u acts %u "
           "(acts==0 => we never even SAW an active emitter slot; reappeared>0 => ordinal STABLE; "
           "reappeared==0 && new_after_off>0 => DRIFTED)",
           s_emitkey_pairs, s_emitkey_pairs, s_emitkey_reappeared, s_emitkey_new_after_off,
           s_emitkey_acts);
    /* ★ 2026-09-30：台账加 `xfam`（本局喂过几次外部/绕转使魔）与 `xtab`（表用了几格）。*/
    if (s && (s->spawns || s->transferred || s_xfam_fed))
        ce_log("youkai: run done - %u familiars spawned, %u killed, %u withdrew, %u silent, "
               "%u transfer damage, %u dropped (no boss), xfam %u xtab %u, "
               "xfer[tick %u map %u/%d orb %u/%d ext %u/%d rect %u fail %u noboss %u noparent %u rej %u clamp %u badfp %u pf %u]",
               s->spawns, s->broken, s->lost, s->silent, s->transferred, s->dropped,
               s_xfam_fed, s_xfam_used,
               s_xfer_tick_calls,
               s_xfer_mapped_feed, s_xfer_mapped_delta, s_xfer_orbit_feed, s_xfer_orbit_delta,
               s_xfer_ext_feed, s_xfer_ext_delta, s_xfer_rect, s_xfer_rect_fail, s_xfer_noboss,
               s_xfer_noparent, s ? s->tr.rejected : 0, s ? s->tr.clamped : 0, s_ysp_badfp,
               s_popup_dropped);
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
    s_run_fresh = 1;   /* ★ P31a：删卡 ⇒ 下次拿到是新局 ✓ */
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_LIFECYCLE) return;   /* ★ 总开关 + 回调族 ⑨ */
    yk_card_t *s = ce_state(c, yk_card_t);
    s_side_lock = -1;                     /* ★ owner ④：删卡 ⇒ 解锁（形态不再钉住）*/
    ce_log("youkai: card deleted - withdrawing familiars "
           "(%u spawned, %u killed, %u transfer damage this run)",
           s ? s->spawns : 0, s ? s->broken : 0, s ? s->transferred : 0);
    /* ★★★ 2026-10-01（owner #78 ③：这两行原来挂在 `on_run_reset`，**那一路这一局没跑**）⇒
     *   **挪到删卡/撤离这条一定会跑的路径**上，保证每局必出。*/
    ce_log("youkai: run done - %u familiars spawned, %u killed, %u withdrew, %u silent, "
           "%u transfer damage, %u dropped (no boss), xfam %u xtab %u, "
           "xfer[tick %u map %u/%d orb %u/%d ext %u/%d rect %u fail %u noboss %u]",
           s ? s->spawns : 0, s ? s->broken : 0, s ? s->lost : 0, s ? s->silent : 0,
           s ? s->transferred : 0, s ? s->dropped : 0, s_xfam_fed, s_xfam_used,
           s_xfer_tick_calls,
           s_xfer_mapped_feed, s_xfer_mapped_delta, s_xfer_orbit_feed, s_xfer_orbit_delta,
           s_xfer_ext_feed, s_xfer_ext_delta, s_xfer_rect, s_xfer_rect_fail, s_xfer_noboss);
    ce_log("youkai/emitkey: pairs %u seen %u reappeared %u new_after_off %u acts %u "
           "(acts==0 => we never even SAW an active emitter slot; reappeared>0 => ordinal STABLE; "
           "reappeared==0 && new_after_off>0 => DRIFTED)",
           s_emitkey_pairs, s_emitkey_pairs, s_emitkey_reappeared, s_emitkey_new_after_off,
           s_emitkey_acts);
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
    focused = yk_focused() ? 1 : 0;
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
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_ITEM) return;   /* ★ 总开关 + 回调族 ⑨ */
    yk_body_sync(ce_state(c, yk_card_t));
}

static int on_draw(ce_card_t *c)
{
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_DRAW) return 0;   /* ★ 总开关 + 回调族 ⑨ */
    /* ★ 2026-09-22（P6）：本体换脚本**不在这里**了（绘制相位太晚 ⇒ 会闪一帧），见 `on_item_frame`。
     * 这个钩子留着（本卡以后若要在绘制相位做纯视觉的东西再放这里）。*/
    (void)c;
    return 0;
}

/* +0x18：开局 / 火力档变 / 关店后广播 —— 池刚被重建过（`Player__repopulate_options` `0x45d5e0`）
 * ⇒ 必须在这里**再申请一次**（原生装备卡同款；spec §3.3 的「火力档变化时重申请」）。*/
static int on_power_level_change(ce_card_t *c)
{
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_LIFECYCLE) return 0;   /* ★ 总开关 + 回调族 ⑨ */
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
    yk_option_refresh(c, s, yk_focused());          /* 声明；落笔在帧界（SDK 调被选举者）*/
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
    if (!YK_ENABLE_CARD) return -1;                    /* ★ 总开关：整卡 no-op */
    yk_card_t *s = ce_state(c, yk_card_t);
    if (!YK_ENABLE_SET_INDEX) return -1;                  /* ★ 二分开关 ①：不认领 ⇒ 全走原生 set */
    if (!s) return -1;
    if (!yk_focused()) return -1;                      /* 人侧：原生 set */
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
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_TICK_SHOOTERS) return 0;   /* ★ 总开关 + 回调族 ⑨ */
    yk_card_t *s = ce_state(c, yk_card_t);
    uint8_t *o;
    int set;
    (void)short_timer;
    (void)long_timer;
    if (!s || !CE_PLAYER()) return 0;
    /* 高速 = 原机体（原生弹照发）；早苗的档 0 = 0 个子机 ⇒ 没有膛口，也不发妖侧弹。
     * ★ 这一条 = 止射闸第三条门的**同一张表、同一个函数**（`yk_option_wanted`）：两边同帧同源
     *   ⇒ 「不压原生弹」的那一帧，这里也恰好不出妖侧弹（不会出现两边都发或两边都不发）。*/
    if (!yk_option_wanted(yk_focused(), yk_current_character(), yk_current_power_level())) return 0;
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
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_BULLET || !YK_ENABLE_CB_BULLET_CREATED) return 0;
    uint32_t packed;
    int32_t out = 0;
    (void)c;
    if (!bullet) return 0;
    /* ★★★ 2026-10-01（**根因修法 A · 缺侧门**）：本回调只服务**妖侧弹**（`yk_bullet_is_youkai`
     *   只认 `0x16` 与逐档组 `0x54..0x67`）⇒ 加侧门后人形态下连「读+写玩家弹结构 `+0x9c`」都不做，
     *   人侧弹（含魔理沙人形态子机激光）与我们**零接触**。
     *   ⚠ 我方弹只可能在妖侧存在（`on_shot_set_index` 只在聚焦时认领我们的 set）⇒ **无功能损失**。*/
    if (!yk_focused()) return 0;
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
    if (!YK_ENABLE_NATIVE_SUPPRESS) return 0;             /* ★ 二分开关 ②：不压原生行 */
    yk_card_t *s = ce_state(c, yk_card_t);
    const int focused = yk_focused() ? 1 : 0;
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

/* ★★★★★ 2026-10-11（**多原点**：同帧同一 `(敌人,槽)` 的第 N 次真发）——
 *   身份键的第二维（`YK_EMIT_ORIGIN_KEY`，见 `youkai_core.h` §3.5）。返回 **0 起**的序号。
 *
 *   为什么是「序号」而不是「出点坐标」：出点每波都在转（`st05/06/07mbs` 的 `%B`/`%C` 每
 *   `wait(10..12)` 就 ±`%D`、外层再来 ±10°）⇒ 拿坐标当身份每波都会认成「新出点」、壳跟着重建；
 *   而 ECL 每波**发弹顺序**是固定的（先 slot 0 的 `%B−90°`、再 `%B+90°`…）⇒ 序号跨波稳定。
 *   ⚠ 键在「哪一侧」上的含义 = 每槽第 1 次 / 第 2 次真发；`st03 GirlD01_at` 的 5 次是 5 个
 *     均匀 72° 的 `etOffsetRad` ⇒ 5 个序号 = 五边形上的 5 个出点。
 *
 *   表按**帧**清空、按 `(enemy_id, slot)` 独立计数（旧的 `s_site_*` 诊断表升级成它 —— 一处口径）。
 *   表满（同帧 >32 个 `(敌人,槽)` 开火）⇒ 退化成 0（= 旧行为，不多建壳、不崩）。
 *   ★ 必须在站点**最顶端无条件调一次**（在任何否决 / 早退之前）：漏掉一次会让这一帧剩下的
 *     序号整体错位（第二次被当成第一次 ⇒ 又合并回一侧）。*/
#define YK_FIRE_ORD_N 32
static int yk_emit_fire_ordinal(uint32_t enemy_id, int slot)
{
    static uint32_t s_ord_frame;
    static uint32_t s_ord_e[YK_FIRE_ORD_N];
    static int16_t  s_ord_s[YK_FIRE_ORD_N];
    static uint8_t  s_ord_c[YK_FIRE_ORD_N];
    uint32_t fr = ce_sdk_frame();
    unsigned i;
    if (fr != s_ord_frame) {
        s_ord_frame = fr;
        __builtin_memset(s_ord_e, 0, sizeof s_ord_e);
        __builtin_memset(s_ord_c, 0, sizeof s_ord_c);
    }
    for (i = 0; i < (unsigned)YK_FIRE_ORD_N; ++i)
        if (s_ord_e[i] == enemy_id && s_ord_s[i] == (int16_t)slot) {
            if (s_ord_c[i] < 250u) ++s_ord_c[i];
            return (int)s_ord_c[i] - 1;
        }
    for (i = 0; i < (unsigned)YK_FIRE_ORD_N; ++i)
        if (!s_ord_e[i]) { s_ord_e[i] = enemy_id; s_ord_s[i] = (int16_t)slot; s_ord_c[i] = 1; return 0; }
    return 0;
}

/* ★★★ 2026-10-19d（owner 逐字：「妖侧的使魔在自机附近不能发弹，这是基本机制」）：
 *   接管的 leveledit 使魔在**妖侧**、且离自机 < `YK_LE_FAM_NOFIRE_DIST` 时，**掐掉它这一次 `etOn`**。
 *   · 「妖侧」判据与外观切换同一源 = `yk_focused()`（一帧只读一次）；
 *   · 只碰**接管的**那些：`yk_xfam_find(id)` 命中且 `x->le` ⇒ 零售敌机 / 自建壳 / 绕转使魔一律不动 ✓；
 *   · 实现 = 复用**已在用**的站点 `th18:0x433ffa`（`on_enemy_shot_slot`，返回非 0 = 本帧这一槽不发射）
 *     ⇒ **0 新站点**、不碰引擎弹幕路径；
 *   · 半径 = `YK_LE_FAM_NOFIRE_DIST`：🟡 **owner 只给了机制没给数值**，先取 64 px（≈ 自机判定圈的十几倍），
 *     登记 `TUNING.md`；日志里有实测距离，owner 说改就一行改。
 *   · 日志：整局最多 8 行（`NOFIRE`），带实测距离 ⇒ 一眼看出这条闸有没有在动。*/
#define YK_LE_FAM_NOFIRE_DIST 64.0f
static unsigned s_le_nofire_logged;

static int yk_on_shot_slot(ce_card_t *c, const ce_enemy_shot_t *ev)
{
    if (!YK_ENABLE_CARD) return 0;                    /* ★ 总开关：整卡 no-op */
    if (!YK_ENABLE_MAP_LAYER) return 0;                  /* ★ ⑨：闸也停（不掐任何 etOn）*/
    /* ★ 2026-10-19d：**妖侧近身掐弹**（owner 口径，见上面那段）。放在所有记账之前 ——
     *   这一发**不会发生**，就不该进 latch / 序号 / FIRECOUNT 的任何一笔。*/
    if (ev && ev->enemy && yk_focused()) {
        yk_xfam_t *lx = yk_xfam_find(ev->enemy_id);
        if (lx && lx->le) {
            const float *pp0 = ce_player_pos();
            const float *ep0 = ce_enemy_pos((uint8_t *)ev->enemy);
            if (pp0 && ep0) {
                const float dx = ep0[0] - pp0[0], dy = ep0[1] - pp0[1];
                const float d2 = dx * dx + dy * dy;
                if (d2 < YK_LE_FAM_NOFIRE_DIST * YK_LE_FAM_NOFIRE_DIST) {
                    if (s_le_nofire_logged < 8u) {
                        ++s_le_nofire_logged;
                        ce_log("youkai/leveledit: NOFIRE familiar id=%u d=%.0f (< %.0f px, youkai side) "
                               "- this etOn is suppressed (owner: 妖侧使魔近身不发弹)",
                               ev->enemy_id, (double)(d2 > 0.0f ? __builtin_sqrtf(d2) : 0.0f),
                               (double)YK_LE_FAM_NOFIRE_DIST);
                    }
                    return 1;                            /* ≠ 0 ⇒ 本帧这一槽不发射 */
                }
            }
        }
    }
    /* ★★★★★ 2026-10-11（**多原点 · 第 0 步**）：本帧这一条 `etOn` 是**该 `(敌人,槽)` 的第几次**
     *   —— 无条件、在任何否决/早退之前调一次（口径唯一，见 `yk_emit_fire_ordinal`）。
     *   `key` = 发射口键（`0..95`）= 本卡对「**这一个出点**」的身份；出点每波旋转也不影响它。
     *   ⚠ 真实槽号（读引擎字段用）从此一律走 `real`，**键**只用于缓存/表/闸。*/
    int real = (ev ? ev->slot : 0);
    int ord  = 0;
    int key  = real;
    if (ev && ev->enemy && real >= 0 && real < YK_EMIT_SLOTS) {
        ord = yk_emit_fire_ordinal(ev->enemy_id, real);
        if (ord >= YK_EMIT_ORIGIN_MAX) ord = YK_EMIT_ORIGIN_MAX - 1;   /* 溢出并进最后一档（不丢壳）*/
        key = YK_EMIT_ORIGIN_KEY(real, ord);
        /* ★ 2026-10-11：① 记「该真实槽本帧几个出点」（血量均分的除数）；② 记该键的 `etOn` 间隔序列。*/
        yk_origin_count_note(ev->enemy_id, real, ord);
        yk_gap_note(ev->enemy_id, key, ce_sdk_frame());
        yk_gap_log(ev->enemy_id, key, real, ord, ce_sdk_frame());
    }
    /* ★★★ 普查 ①（0 新站点）：`etOn` 已经把发弹点写进 `props[i]+0x08/+0x0c/+0x10`
     *   （就在本断点之前那几步）⇒ 立刻抄回卡侧缓存，供壳的**目标位置**使用。
     *   `ev->enemy` 只在本次回调内有效 ⇒ 只读、只抄，不缓存指针。
     * ★★★★★ 2026-10-11（**纯静态核实**后的定案；全文 `HANDOFF-108-EMITTER-STATIC-VERIFY.md` §1.2/§2）：
     *   一手 `th18:0x433f34`：`etOn` 有**两条**写路径，缓存必须按同一条判据分流，否则位置必错：
     *     · 相对路（`origins[i].z <= 0.9f`，判据 `th18:0x433f52`）：`props.xy = 敌人位置(此刻) + offsets[i]`
     *       ⇒ 缓存**相对量** `props − 敌人位置(此刻)`；消费端 `敌人位置(每帧) + 它` ⇒ 壳跟着母机走。
     *     · 绝对路（`origins[i].z > 0.9f`，`etOffsetAbs`）：`props.xy = origins[i].xy + offsets[i].xy`
     *       （**不含敌人位置**）⇒ 缓存**世界坐标本身**；若这里还减敌人位置、消费端再加回来，
     *       壳就会被母机**拖着走**、离锚点越来越远（`etOffsetAbs` 全库 22 处，含锚在自机身上那种）。
     *   `0.9f` 与引擎同一个常量（`0x4b916c`，直读 `.rdata`）。
     * ★ 缓存**按 `key` 分开写**：`st05/06/07mbs` 的两次 `etOn` 因此在缓存里各占一格、互不覆盖
     *   （这就是「只看见一侧」的根因修法：以前两次写同一个 `(enemy_id, slot)` 格，后写覆盖先写）。*/
    if (ev && ev->enemy && real >= 0 && real < YK_EMIT_SLOTS) {
        const uint8_t *prr = (const uint8_t *)ev->enemy + YK_EMIT_PROPS
                            + (size_t)real * YK_EMIT_PROP_STRIDE;
        const float *org_abs = (const float *)((const uint8_t *)ev->enemy + YK_EMIT_ORIGINS
                                              + (size_t)real * YK_EMIT_VEC_STRIDE);
        const float *ep_now = ce_enemy_pos((uint8_t *)ev->enemy);
        int lat_abs = (org_abs[2] > YK_ORIGIN_ABS_THRESHOLD) ? 1 : 0;
        float lx, ly, lz;
        if (lat_abs) {                       /* 绝对锚：存世界坐标（消费端不再加敌人位置）*/
            lx = *(const float *)(prr + 0x08);
            ly = *(const float *)(prr + 0x0c);
        } else {                             /* 相对：存 `props − 敌人位置(此刻)` = `offsets[i]` */
            lx = *(const float *)(prr + 0x08) - (ep_now ? ep_now[0] : 0.0f);
            ly = *(const float *)(prr + 0x0c) - (ep_now ? ep_now[1] : 0.0f);
        }
        lz = *(const float *)(prr + 0x10);
        yk_emit_latch_put(ev->enemy_id, key, lx, ly, lz, lat_abs);
        {
            uint32_t fr = ce_sdk_frame();
            int k = yk_fk_bump(ev->enemy_id, real, fr);   /* ★ k = 序号 `ord`（诊断口径，按真实槽）*/
            ++s_fire_seen;   /* ★ 无条件（在 k 门槛之前）*/
            /* ★ 每帧：活跃弹数（扫数组）+ FIRECOUNT（本帧每 (敌人,槽) 的 etOn 次数 vs 弹数增量）*/
            {   unsigned live = yk_bm_live_bullets();
                int32_t dl = (s_bm_prev_live < 0) ? 0 : (int32_t)live - s_bm_prev_live;
                s_bm_prev_live = (int32_t)live;
                if ((fr % 60u) == 0u && s_bm2_logged < 64u) {
                    ++s_bm2_logged;
                    ce_log("youkai/emit BMPROBE2: live_bullets=%u frame=%u (走 mgr+0xbc 活弹链表, state+0xaac != 0/3)",
                           live, (unsigned)fr);
                }
                if (k <= 0 && s_fcout_logged < 64u) {
                    int calls = (int)s_fk[0].k;   /* 占位：下面按当前帧的 s_fk 项逐条打 */
                    (void)calls;
                }
                if (s_fcout_logged < 64u) {
                    unsigned q;
                    for (q = 0; q < YK_FK_N && s_fcout_logged < 64u; ++q) {
                        if (!s_fk[q].enemy || s_fk[q].frame != fr) continue;
                        ++s_fcout_logged;
                        ce_log("youkai/emit FIRECOUNT: enemy_id %u slot %d eton_calls=%u live_delta=%d "
                               "est_volleys=%d frame=%u (eton_calls from SITE/FIRE path; live_delta from the "
                               "enemy bullet array)",
                               s_fk[q].enemy, (int)s_fk[q].slot, (unsigned)s_fk[q].k + 1u, (int)dl, (int)dl,
                               (unsigned)fr);
                    }
                }
            }
            /* ★ FIRESUM（owner）：每帧汇总「本帧 fire 次数 / 该帧内某槽的最大 k」⇒ 即使没到 boss
             *   也能看出 k 到底能不能 >0。每 60 帧一行（避免刷屏）。*/
            {   unsigned i2, mx = 0;
                for (i2 = 0; i2 < YK_FK_N; ++i2)
                    if (s_fk[i2].enemy && s_fk[i2].frame == fr && s_fk[i2].k > mx) mx = s_fk[i2].k;
                if ((fr % 60u) == 0u && s_firesum_logged < 64u) {
                    ++s_firesum_logged;
                    ce_log("youkai/emit FIRESUM: frame=%u fires=%u maxk_by_slot=%u (k>0 means the same "
                           "(enemy,slot) fired twice in one frame)", (unsigned)fr, s_fire_seen, mx);
                }
            }
            const uint8_t *prf = (const uint8_t *)ev->enemy + YK_EMIT_PROPS
                               + (size_t)real * YK_EMIT_PROP_STRIDE;
            const float *orf = (const float *)((const uint8_t *)ev->enemy + YK_EMIT_ORIGINS_ABS
                                              + (size_t)real * YK_EMIT_VEC_STRIDE);
            const float *olf = (const float *)((const uint8_t *)ev->enemy + YK_EMIT_OFFSETS_ABS
                                              + (size_t)real * YK_EMIT_VEC_STRIDE);
            yk_slot_t *g0 = yk_map_find(&s_map, ev->enemy_id, key);   /* ★ 查**这一条键**（多原点）*/
            /* 额度（owner）：**k>=1 一律打**（关键样本）+ 每敌 ≤8 + 总 ≤256 ⇒ 中 boss 必现。
             * ★ 2026-10-11：k>=1（= 同帧第 2..N 次）那一支**再放宽**（总 256 → 512、每敌 8 → 24）
             *   —— 9 处多原点里最密的 `st03 GirlD01_at` 一局能到 5 次/波、头 256 行早就被前几面用光了。*/
            if ((k >= 1 || s_fire_logged_2 < 16u) && s_fire_logged_2 < 512u &&
                yk_fk_lines(s_fkline, ev->enemy_id) < (k >= 1 ? 24u : 8u)) {
                ++s_fire_logged_2;
                yk_fk_inc(s_fkline, ev->enemy_id);
                ce_log("youkai/emit FIRE: enemy_id %u slot %d k=%d key=%d type=%d color=%d props08/0c/10=(%.1f,%.1f,%.1f) "
                       "offsets[slot]=(%.1f,%.1f,%.1f) origins[slot]=(%.1f,%.1f,%.1f) frame=%u have_shell=%d "
                       "(key = slot + 16*k: the identity this fire is mapped to)",
                       ev->enemy_id, real, k, key, *(const int32_t *)(prf + 0x00),
                       *(const int32_t *)(prf + 0x04), (double)lx, (double)ly, (double)lz,
                       (double)olf[0], (double)olf[1], (double)olf[2],
                       (double)orf[0], (double)orf[1], (double)orf[2], (unsigned)fr,
                       (g0 && g0->fam_id) ? 1 : 0);
            }
            if ((k >= 1 || (g0 && g0->fam_id)) && s_og_logged < 512u && yk_fk_lines2(s_fkline2, ev->enemy_id) < 24u) {
                ++s_og_logged;
                yk_fk_inc(s_fkline2, ev->enemy_id);
                ce_log("youkai/emit ORIGIN-GATE: enemy %u slot %d k=%d key=%d closed=%d veto=%d "
                       "(closed = this origin's shell was killed => THIS etOn is vetoed)",
                       ev->enemy_id, real, k, key,
                       (g0 && g0->fam_seen && g0->broken) ? 1 : 0,
                       (g0 && g0->fam_seen && !g0->fam_id) ? 1 : 0);
            }
        }
        /* ⛔ 2026-10-01 晚（owner log 反证：`k` 跨敌人递增 ⇒ 运行的是**这一份旧的"每槽全局计数"块**）：
         *   本块**已删**；**唯一保留**的是上面那份按 `(enemy_id, slot)` 独立计数的 `s_fk[]` 版本
         *   （否则同一槽一帧内两次 fire 拿不到 k=0/k=1，C2 无从做起）。*/
        yk_fired_sticky_set(ev->enemy_id, key);   /* ★ 粘性「本波开过火」（按**键**，多原点各记各的）*/
        /* ★★★★★ 2026-10-01（**owner 硬约束，回滚**）：owner 逐字「**但是无论如何，你只能不发弹，
         *   **不能改发弹位置**」⇒ 本卡**永远不写 `props` 的任何一格**（`+0x24` 的 `etDist` 归零
         *   已回滚 —— 一手 `th18:0x4251f9`/`0x425219` 确实能「把生成点推 dist px」，但那**不许碰**）。
         *   ⇒ 本站点对我们的槽**只做两件事**：① 读事实（上面那段 latch：`props+0x08/+0x0c/+0x10`
         *     = 世界坐标，只读）；② 通过返回值**否决这条 `etOn`**（`yk_shot_slot_gate()` 返回 1）。
         *   **不变量（硬）**：禁止修改 `props` 的 offsets / origins / dist / aim / angle / type 任何一格。*/
        if (s_emit_latch_logged < 8u) {                   /* 判据：前 8 次定格值各打一行 */
            ++s_emit_latch_logged;
            ce_log("youkai/emit: latched offset_final for slot %d of enemy_id %u = (%.1f, %.1f, %.1f) "
                   "at the etOn site (0x433ffa) - the shell targets this FROZEN point, not the live "
                   "offset (survey item 1: 138 ECL groups clear it back to 0 in the same frame)",
                   real, ev->enemy_id, (double)lx, (double)ly, (double)lz);
        }
    }
    yk_card_t *s = ce_state(c, yk_card_t);
    uint32_t have = 0;
    uint32_t rearms_before;

    /* ★★★★ 2026-10-11（owner：整局 `maxk_by_slot` 恒 0；`SITE` 一眼定生死）——**站点最顶端无条件计数**
     *   （任何 return/否决之前；序号由 `yk_emit_fire_ordinal` 统一给出，与键同源）：
     *   `ord` = 本帧内该 `(敌人,槽)` 第几次进站点（0 起）；**只打 ord>=1**（= 同帧多发，避免刷屏）。
     *   实跑判据（build `Oct 11 2026 03:28:24` 已见 64 行）：`call#=2` 出现 ⇒ **引擎真发两次**
     *   （不是合并）⇒「只建一侧」是我们这边的键少了一维。*/
    if (ev && ev->enemy_id && ord >= 1 && s_site_logged < 256u) {
        ++s_site_logged;
        ce_log("youkai/emit SITE: enemy_id %u slot %d call#=%d key=%d frame=%u (unconditional, before "
               "any early return; call#>=2 = the engine really fired this slot twice in one frame)",
               ev->enemy_id, real, ord + 1, key, (unsigned)ce_sdk_frame());
    }
    if (!s || !ev || !ev->enemy_id) return 0;
    if (real < 0 || real >= YK_EMIT_SLOTS) return 0;
    /* ★★★ 2026-10-01（**owner 报的真 bug**：「**只有人侧才能打死使魔啊**」＋「TH08 的关键是在于，
     *   **使魔是符卡的有机组成部分，由它来向你发弹**」）—— **原 P8-1 的侧门已删**。
     * 旧代码：`if (!CE_PLAYER_FOCUSED()) return 0;`（未聚焦 ⇒ 本卡不表态）。它建立在一条**早已不成立**
     * 的前提上（P8-1 那条注写「1:1 映射整体只在低速活着」）；事实（2026-09-22 owner 口径订正之后）：
     *   · **松 Shift 不撤使魔**（`on_tick_2` 的 `!focused` 支只收妖侧专有的东西 —— 光环 / 染色 /
     *     切换环；`yk_leave_youkai()` 一个字都不碰 `s_map`）；映射使魔与闸的夹持态**跨形态保持**；
     *   · 而**妖侧使魔无敌**（`yk_orbit_pin` 那一套：`NO_DAMAGE` + 碰撞盒置 0）⇒ 它**只可能在人侧被打死**。
     * ⇒ 旧门把闸**唯一该起作用的那一侧关掉了** = 等于没做（owner 的判据：打死发射口使魔 ⇒ 那个口
     *   应当停火；TH08 里那一发弹就是使魔自己发的）。
     * 新的语义（与侧无关）：**「这一槽的使魔建过、现在没了」⇒ 掐掉这一刻那条 `etOn`** —— 由
     *   `yk_shot_slot_suppressed()` 判（`fam_seen && broken`）。它在妖侧只会被 `enmKillAll` 波次清场
     *   触发，而那一刻槽马上会被 ECL 重新武装 / 由映射重建（route A/A′ 立刻放开）⇒ 不会夹死 boss。
     * ⚠ 旧门当年是给「击破一个阶段后 boss 不再发弹」止血的；那个症状的真根因（缺少复位口）已由
     *   route A（卡 tick 消费 `rearm_pending`）+ route A′（该槽第二次 `etOn` 当场放开）解决，
     *   不再需要拿「不表态」当兜底。*/
    /* 按 id 回查**当前活着的**使魔（0 = 没了）。★ 不碰 `ev->enemy`：那是敌人本体、不是使魔，
     * 而且只在本次回调内有效（同帧 sweep 可能删它）。
     * ★★★★★ 2026-10-11（**多原点 = 逐出点关火**）：回查/裁决一律用**这一条 `etOn` 的键**
     *   （`key`）—— `st05/06/07mbs` 的第 1 次真发（`%B−90°` 那侧）与第 2 次（`+90°` 那侧）
     *   从此是两条独立条目 ⇒ 打死一侧的使魔只掐那一次 `etOn`，另一侧照常发弹。 */
    have = yk_slot_fam_id(&s_map, ev->enemy_id, key);
    rearms_before = yk_map_rearms(&s_map);
    {   /* ★ 打点 ③/④：本站点读到的三条件与最终裁定（owner 要的 `gate check` 行；前 8 次）。*/
        yk_slot_t *g = yk_map_find(&s_map, ev->enemy_id, key);
        int veto = yk_shot_slot_gate(&s_map, ev->enemy_id, key, have);
        /* ⚠ 只对**我们真管过的槽**打（表里有条目且 `fam_seen`/`broken` 有值）——
         *   原来只打前 8 条全局 ⇒ 全是 stage 1 的 enemy 3/4/5，永远看不到 158。
         *   `fam_seen=-1 / broken=-1` = **查不到条目**的哨兵（这槽我们从没建过壳）。*/
        if (g && (g->fam_seen || g->broken) && s_gatecheck_logged < 48u) {
            ++s_gatecheck_logged;
            ce_log("youkai/emit: gate check (enemy_id %u, slot %d key %d ord %d) fam_seen=%d have_fam_id=%d broken=%d "
                   "=> veto=%d (veto=1 means this etOn is CLAMPED)",
                   ev->enemy_id, real, key, ord, g ? (int)g->fam_seen : -1, (int)(have ? 1 : 0),
                   g ? (int)g->broken : -1, veto);
        }
        if (!veto) return 0;
    }
    /* ★★★ 2026-09-22（P8-5）：**每一次「夹持期开始」记一行完整的上下文**（含 boss 与活跃槽数）。
     * 记账判据 = `rearms` 涨（= 闸刚置上 `rearm_pending`，一个夹持期只记一次）。
     * ⚠ 不逐帧逐槽打（16 槽 × 60 fps = 每秒上千行，会把日志淹掉）——「每次」按**夹持期**计。*/
    if (s && yk_map_rearms(&s_map) != rearms_before) {
        yk_slot_t *sl = yk_map_find(&s_map, ev->enemy_id, key);
        ce_log("youkai: emitter gate CLAMPED slot %d (key %d ord %d) of enemy_id %u (focused=%d fam_seen=%d "
               "broken=%d no_fam_frames=%d) - P1: this slot stays silent for the REST of this wave "
               "until a main release port (ECL re-arm / wave edge / phase edge / reset) or the "
               "%d-frame rebuild TTL fires (core: `yk_shot_slot_gate` / `YK_EMIT_REBUILD_TTL`)",
               real, key, ord, ev->enemy_id, yk_focused() ? 1 : 0,
               sl ? (int)sl->fam_seen : -1, sl ? (int)sl->broken : -1,
               sl ? (int)sl->no_fam_frames : -1, (int)YK_EMIT_REBUILD_TTL);
    }
    if (s && !s->logged_gate) {
        s->logged_gate = 1;
        ce_log("youkai: emitter gate ACTIVE - slot %d (key %d) of enemy_id %u stays silent for the rest of "
               "this wave (P1: releases only on ECL re-arm / wave edge / phase edge / reset) or until "
               "the %d-frame rebuild TTL fires",
               real, key, ev->enemy_id, (int)YK_EMIT_REBUILD_TTL);
    }
    return 1;
}

/* ★★★ 2026-09-27（第三十七批 · **owner「3 可以贴一手，关键在站点写法」**）：
 *   **人逢魔（槽 ≤ −8000）+ **一发自机弹命中敌机** ⇒ 在**那颗弹的位置**掉 1 颗刻符**。
 *
 *   一手（`Player::compute_damage_to_enemy`，`th08:0x451860`–`:0x451892`，逐指令）：
 *     `451860 mov ecx,0x160f508 / 451865 call 0x406d10` ⇒ **極端人間**问（假 ⇒ 整段跳过）
 *     `451871 mov eax,[bullet+0x480]` / `451877 movsx ecx,WORD [eax+0x1e]` / `45187d jge` ⇒ ≥0 跳过
 *     `45187f push 3` / `451881 push 7` / `451886 add edx,0x2a4`（**弹的位置**）/ `451892 call 0x4400a0`
 *   ⇒ 颗粒度是「**每一发命中**」（不是「每次击破」）；出生态 type 7 ⇒ `0x440124` 强制 state 3（先上抛）。
 *
 *   TH18 没有等价函数（伤害在敌人侧算）⇒ SDK 新站点 `ce_bullet_hit`（`0x45f466`）把
 *   「某个伤害源**第一次**命中某个敌」那一刻交出来，并已判定 `ev->is_player_bullet`
 *   （扫弹池找 `bullet+0xb0 == 本槽下标`）。机制全文 = `sdk.c` 的 `BP_ce_bullet_hit`。
 *
 *   ⚠ 回调里**只记标量 + 建自己的永点**（`yk_orb_spawn` 建的是 anm VM —— 与卡自己的 tick 同级，
 *     不在死亡栈上）；**不缓存 `ev->enemy`**（同帧可能被删）。
 *   ⚠ 本卡原来那条「人间逢魔 + 自机弹击杀 ⇒ 逢魔击破那笔 +1」的**近似已撤**（`yk_fengma_kill_orbs`
 *     恒 1）—— 否则同一件事记两遍（一手那 +1 与死亡路径那 1 颗本来就是两笔，而「击破 + 命中」
 *     在极人时自然会同时发生 ⇒ 那一瞬**确实是 2 颗**，只是账要记在各自的来源上）。*/
static unsigned s_bullet_hit_orbs;                     /* 台账：本批新来源撒了几颗（实跑判据）*/
static unsigned s_bullet_hit_pos_swap;                /* ★ 2026-09-27（第三十九批）：人逢魔「每发命中」那条
                                                       *   里，落点从「源槽中心（膛口）」换成「敌机位置」的次数
                                                       *   —— 激光类应当明显 >0，普通弹应当 0。*/
static unsigned s_graze_pos_from_bullet;              /* ★ 2026-09-27（第三十九批）：擦弹刻符有几批的落点
                                                       *   拿到了「被擦到的那颗弹」的坐标（站点 `ce_graze`）。
                                                       *   这个数不涨 ⇒ 站点没挂上（或 `ce_graze_take` 恒 0）。*/
/* ★★★★ 2026-10-01（**根因修法 C · owner #113 二分：`CB_BULLET_HIT=0` ⇒ 正常**）：
 *   #111 已只排队、#112 又证明不是「建 VM/链表」⇒ 逐行审计后结论：**只要我们的 C 代码在引擎的命中断点里
 *   跑，就有扰动风险** ⇒ 本回调现在压成**纯记录器**：
 *     · **不取 `ce_state`**、**不调任何 `ce_*`**、**不调任何 `yk_*` 判据**（连纯函数 `yk_human_fengma_bullet_orbs`
 *       也不调）—— 断点里只做「读事件结构 + 写本文件静态环」；
 *     · **判据 / 落点换算 / 撒符**全部挪到 `on_tick_2` 的 `yk_bullet_hit_flush()`（普通帧上下文）。
 *   功能语义不变（人逢魔命中掉刻符，≤1 帧延迟；长光源落点规则不变）。*/
static float    s_bhit_x[YK_BULLET_HIT_QUEUE];
static float    s_bhit_y[YK_BULLET_HIT_QUEUE];
static float    s_bhit_ex[YK_BULLET_HIT_QUEUE];
static float    s_bhit_ey[YK_BULLET_HIT_QUEUE];
static uint8_t  s_bhit_flags[YK_BULLET_HIT_QUEUE];   /* bit0 = has_enemy_pos, bit1 = is_player_bullet */
static uint8_t  s_bhit_used[YK_BULLET_HIT_QUEUE];
static unsigned s_bhit_head;
static unsigned s_bhit_dropped;

static void on_bullet_hit(ce_card_t *c, const ce_bullet_hit_t *ev)
{
    unsigned k;
    (void)c;                                          /* ★ 连 `ce_state` 都不取（少一次 SDK 调用）*/
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_BULLET || !YK_ENABLE_CB_BULLET_HIT) return;
    if (!ev) return;
    /* ▼▼ 本函数**只做这一件事**：把原始事件字段抄进静态环（纯内存写）。▼▼ */
    for (k = 0; k < (unsigned)YK_BULLET_HIT_QUEUE; ++k) {
        unsigned idx = (s_bhit_head + k) % (unsigned)YK_BULLET_HIT_QUEUE;
        if (s_bhit_used[idx]) continue;
        s_bhit_used[idx] = 1;
        s_bhit_x[idx] = ev->x;   s_bhit_y[idx] = ev->y;
        s_bhit_ex[idx] = ev->ex; s_bhit_ey[idx] = ev->ey;
        s_bhit_flags[idx] = (uint8_t)((ev->has_enemy_pos ? 1u : 0u) | (ev->is_player_bullet ? 2u : 0u));
        return;
    }
    ++s_bhit_dropped;                                 /* 环满（极小概率）：丢一颗，账上可见 */
}

/* `on_tick_2` 里调用（**普通帧上下文**）：判据、落点换算、撒符全在这里做。*/
static void yk_bullet_hit_flush(yk_card_t *s)
{
    unsigned k;
    if (!s) return;
    for (k = 0; k < (unsigned)YK_BULLET_HIT_QUEUE; ++k) {
        int has_enemy_pos, is_player_bullet;
        float ox, oy;
        if (!s_bhit_used[k]) continue;
        s_bhit_used[k] = 0;
        has_enemy_pos    = (s_bhit_flags[k] & 1u) ? 1 : 0;
        is_player_bullet = (s_bhit_flags[k] & 2u) ? 1 : 0;
        if (!yk_human_fengma_bullet_orbs(s->gauge, is_player_bullet)) continue;   /* 判据挪到这里 */
        ox = s_bhit_x[k]; oy = s_bhit_y[k];
        if (has_enemy_pos && yk_hit_orb_use_enemy_pos(s_bhit_x[k] - s_bhit_ex[k],
                                                     s_bhit_y[k] - s_bhit_ey[k])) {
            ox = s_bhit_ex[k]; oy = s_bhit_ey[k];
            ++s_bullet_hit_pos_swap;
        }
        yk_orb_spawn(s, ox, oy, YK_ORB_STATE_SPAWN);   /* state 3 = 先上抛（一手 `push 3`）*/
        ++s_bullet_hit_orbs;                           /* 台账：真撒出去的算一颗 */
    }
    s_bhit_head = 0;
}

/* ★ §3.6 击破信号：共享死亡事件是**广播只读**（EVENTS-CONTRACT §1）——
 * 这里只做一件事：认出「这只被打死的是我的使魔」⇒ 把那个发射口槽标成「被打破」。
 * ⚠ 回调里禁止析构任何敌人（UAF），也不缓存 `ev->enemy`（同帧 sweep 会删）。*/
static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_ENEMY_DEATH) return;   /* ★ 总开关 + 回调族 ⑨ */
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
     *     · **纯状态判断**（本卡：这一槽的使魔没了）⇒ **把 `KILL_ALL` 算进来是对的**：清场时使魔确实
     *       从场上消失了，卡必须知道（否则表里留着已死使魔的 id）。
     *   ★★★ 2026-10-01（**P1 口径改动**）：**`KILL_ALL` 的处理方式变了** —— 它不再走 `yk_fam_broken()`
     *     （那是「**玩家打死**」的语义 = 夹闸），而是走 `yk_fam_wave_clear()`：`enmKillAll` 是 **ECL 的
     *     「新一波开始」**（op95，110 处、集中在段首），一手语义就是「每波重新 `familiarCreate`」
     *     （`engine/enemy/th08/01-familiars-and-anm.md` §3）⇒ 新波该口应当**照常发弹**（放行 + 重建）。
     *     这也是 owner 要的「主复位口 = 波次/相位边沿」里的**波次边沿**。
     *   炸弹秒杀（`BOMB_KILL`）与普通击破（`KILLED`）仍走 `yk_fam_broken()`（使魔被玩家打掉 ⇒ 夹闸）。
     *   ECL_DIE / OTHER 对我们自造的惰性空壳（无 ECL、无符卡）不可能发生，故不列。 */
    if (ev->kind != CE_EDEATH_KILLED && ev->kind != CE_EDEATH_BOMB_KILL &&
        ev->kind != CE_EDEATH_KILL_ALL)
        return;
    /* ① 波次边沿（`KILL_ALL` = `enmKillAll`）：**放行 + 让映射重建**，不夹闸（P1 主复位口 ②）。*/
    if (ev->kind == CE_EDEATH_KILL_ALL) {
        yk_fam_wave_clear(&s_map, ev->id);       /* 不是我们的使魔 ⇒ 返回 0，什么都不做 */
        return;                                  /* 清场不算击破（本函数开头那段约定）⇒ 不进 +N 那条路 */
    }
    {   /* ★ 2026-10-01 晚（owner：5 面道中「使魔被打死了，但**没重建、也没否决发弹**」）：
         *   打点 ①/②：`broken` 到底有没有真打上（`on_enemy_death` ⇒ `KILLED/BOMB_KILL` 广播）。*/
        yk_slot_t *slt = yk_map_find_fam(&s_map, ev->id);
        if (slt) {
            /* ★ 2026-10-11（多原点）：这只壳可能被**多条键**引用（同点共壳）⇒ 打印它自己的那条
             *   + 提示下面的 `yk_fam_broken` 会把**所有**引用它的键一起标 broken（逐出点关火）。*/
            ce_log("youkai/emit: BROKEN set for (enemy_id %u, slot %d key %d) from fam %u (kind=%u) "
                   "=> EVERY key bound to this shell goes silent until a wave/rearm edge or the "
                   "%d-frame TTL",
                   slt->enemy_id, YK_EMIT_REAL_SLOT((int)slt->slot), (int)slt->slot, ev->id,
                   (unsigned)ev->kind, (int)YK_EMIT_REBUILD_TTL);
        }
    }
    if (!yk_fam_broken(&s_map, ev->id)) {
        /* ★★ 2026-09-22：**不是我们的使魔** ⇒ 这是一次普通击破 —— 人妖槽与刻符的涨源就在这里。*/
        yk_on_enemy_killed(s, ev);
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
        yk_gauge_limits_solo(yk_focused() ? 1 : 0, &hl3, &yl3);
        s->gauge = yk_gauge_add2(s->gauge, yk_gauge_fam_kill_delta(s->gauge), hl3, yl3);
        yk_orb_spawn(s, ev->x, ev->y, YK_ORB_STATE_SPAWN);   /* ★ §BE #117：type 7 ⇒ 强制 state 3 */
        s->orb_silence = YK_ORB_SILENCE_FRAMES;
    }
    /* ★ #27 支 1：**「+1」弹字**（一手 `Enemy::sub_42adb0 th08:0x42b296 CreateTimePopup(&enemy+0x2d88, 1, 0, 0xFFFFFFFF)`
     * —— 一手是「击破魔使」那一刻；我们卡里没有 op90-92 那种魔使，**我们的使魔 = 同一位置的对应物** ⇒ 挂这里）。*/
    /* 击破使魔那一支（一手 `th08:0x42b296`）：**常数 1** + 白 `0xffffffff` + 字号 1.0（`kind 1` = 带 '+'）。*/
    /* ★ 这一笔在「被杀者**自己有主**」分支里 ⇒ **绕转使魔被击破也有 `+1`** ✓（妖侧无敌 ⇒ 只在人侧发生）。*/
    yk_popup_spawn(ev->x, ev->y, 1, 1, 0xffffffffu, 1.0f, "fam+1");
    ce_log("youkai: familiar enemy_id %u killed (kind %u, real=%u) at (%.0f, %.0f) - "
           "P1: that emitter slot now stays silent for the rest of this wave (main release ports: "
           "ECL re-arm / wave edge / phase edge / reset) or up to %d frames until the rebuild TTL "
           "fires (gate = on_enemy_shot_slot 0x433ffa)",
           ev->id, ev->kind, (ev->flags & CE_EDEATH_F_REAL) ? 1u : 0u, ev->x, ev->y,
           (int)YK_EMIT_REBUILD_TTL);
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
    s->gauge = yk_gauge_add(s->gauge, yk_focused() ? (int)(base / 100) : -(int)(base / 100));
}
#endif

/* ★★ 2026-09-22（人妖槽：死亡清零）：一手 `th08:0x44ac09`（`Player::begin_death` 里
 * `set_youkai_gauge(0)`）。走 `on_death_before_deathbomb`（决死窗口**之前**那一帧，
 * 与 TH08 的 `begin_death` 同一时刻）。*/
/* ★★★★ 2026-10-11（owner 整蛊协同：「当有 106 的时候，决死 B 消耗两个」）——
 *   **106 判据（D 路线，不改共享件）**：**只读** AbilityManager 的卡链表（SDK 自己用的那份：
 *   `CE_MGR_CARD_LIST_FIRST` → node → `CE_NODE_CARD` → `ce_card_id(card)`），查有没有 id=106。
 *   这就是「在卡组里（已加载的卡）」的读法；不需要新增 sdk 接口 ⇒ 无共享件登记成本。*/
static int yk_has_card(uint32_t want)
{
    uint8_t *mgr = CE_ABILITY_MGR();
    const uint8_t *node;
    unsigned g = 0;
    if (!mgr) return 0;
    for (node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST); node && g < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++g) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (card && ce_card_id((ce_card_t *)card) == want) return 1;
    }
    return 0;
}

#define YK_DEATHBOMB_DOUBLE_WITH_106 1   /* ★ owner 整蛊协同：有 106 ⇒ 决死多扣 1 个 B（总消耗 2）；0 = 关 */
static unsigned s_db106_logged;

static int on_death_before_deathbomb(ce_card_t *c)
{
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_DEATHBOMB) return 0;   /* ★ 总开关 + 回调族 ⑨ */
    /* ★★★ 2026-10-11（owner：「**当有 106 的时候，决死 B 消耗两个**」；「**不会扣到负数**」、
     *   「**最后一个 B 的时候也可以决死**」）—— 本回调在**决死窗口之前**那一帧跑（`youkai.c` 本函数），
     *   在这里**多扣 1 个**：B 数 = `CE_CURRENT_BOMBS()`（`0x4ccd58`）；`fragments/max` = `0x4ccd5c/0x4ccd64`。
     *   · `bombs >= 1` 才扣（`bombs=1` ⇒ 扣成 **0**，**触发判定不动** ⇒ 照旧允许决死）；
     *   · **clamp ≥ 0**：`bombs=0` 时不动（绝不出现负数）；
     *   · **只在有 106 时生效**；无 106 / 开关关 ⇒ 一个字节都不写（行为逐位不变）。*/
    if (YK_DEATHBOMB_DOUBLE_WITH_106 && yk_has_card(106u)) {
        int32_t before = CE_CURRENT_BOMBS();
        if (before > 0) {
            CE_CURRENT_BOMBS() = before - 1;
            if (s_db106_logged < 32u) {
                ++s_db106_logged;
                ce_log("youkai/debug: deathbomb cost = 2 (card 106 present); bombs %d -> %d%s",
                       (int)before, (int)CE_CURRENT_BOMBS(),
                       (CE_CURRENT_BOMBS() == 0) ? " (clamped, still allowed)" : "");
            }
        } else if (s_db106_logged < 32u) {
            ++s_db106_logged;
            ce_log("youkai/debug: deathbomb cost = 2 (card 106 present) but bombs already 0 (clamped, no negative)");
        }
    }
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
 * ## 视觉（每台我们自己那台子机一条**膛口轨迹** × 逐帧 `rate` 条残影束）
 *   · 脚本 = `pl01.anm` 追加的 `62..66`（`YK_BEAM_SCRIPT[tier]`，形状见
 *     `assets/ability/make_youkai_bullet_art.py` 的 `gen_beam`）；**从 `pl01.anm` 起**
 *     （`yk_player_anm()` = `player+0x0c`）—— 贴图 `YKB_M_OPT.png` 就长在那个文件的图集里。
 *   · 每帧写：位置 = **该条残影对应的历史膛口**（一手 `trail[k]`；★ **第三十四批起 `k = 0` 就是
 *     「本帧」的膛口** —— 一手那里写的是 `vm+0x2a4` = 上一帧的膛口，见下面「跟随」一节）、
 *     方向 = **−π/2**（一手 `ply01as.sht` 那一行的 `+0x14`；脚本**烘了** `rotate(-90°)` 当初值、
 *     卡侧每帧用真实角度覆盖它 —— 第三十一批起与 117 `script163` 逐行同形；仍**不写** `rotateAuto`）、
 *     长度 = **`YK_LASER_LEN`（2048）× 展开系数** ⇒ `scale.x = (2048·grow)/768 = 2.6667·grow`
 *     （★ 第三十三批：分母从 512 改成 **768** = 贴图宽 = **整条 entry 行宽** ——
 *      为什么必须同宽见 `youkai_core.h` 的 `YK_M_LASER_BEAM_SPRITE_W` 长注：
 *      `scrollX` 的绕回按 **entry 宽**取模，贴图窄于 entry 就会把邻居的像素扫进束里）；
 *     （★ `anchor(1,0)` = **左端锚**，见 `engine/anm/th16/10` §2
 *     ⇒ 近端钉在膛口、**远端往外伸**，与 117 的 `eagle_beam_tick` 逐行同形）；
 *     宽度 = **逐档**（`YK_M_LASER_BEAM_W_PX100 / 32`）× 展开系数。
 *   · **展开**照 117 的细节「如收回般（倒放）展开」：束脚本 `interruptLabel(1)` 的收束段是
 *     `scaleTime(8, 1, 1.0, 0.0)`（8 帧里 `scale.y → 0`）⇒ 展开就是它的倒放
 *     （`YK_M_LASER_BEAM_GROW_FRAMES` = 8 帧）。★★ 第二十七批起**两轴同涨** —— 上一版只涨宽度
 *     ⇒ 束**一出生就是全长**、只是变粗，owner 报的「一节一节长出」观感就是这里（加上贴图滚动的频闪，
 *     见 `YK_M_LASER_BEAM_SCROLL` 的长注）。
 *
 * ## 残影多束（★ 第二十七批，owner ③；一手逐指令见 `youkai_core.h` 的同一节）
 *   · 每条轨迹 = **32 格**的膛口历史（一手 `vm+0x2b0`）；每帧 `yk_m_laser_beam_trail_step()` 推一格
 *     （`trail[i] = trail[i-1]` 且 `y -= 1`），未填的格是 **−999** 哨兵。
 *   · 每帧出 **`YK_M_LASER_BEAM_RATE[tier]`** 条（2/4/6/8/12）：`k = 0` 是本帧膛口、越 `k` 越旧越淡
 *     （alpha 递减照一手 `0x450ad0`，见 `yk_m_laser_beam_bar_alpha`）。**玩家不动时它们几乎重合**、
 *     **横向移动时散成好几条** —— 这就是 owner 要的那条机制。
 *   · ★★★ **第三十四批：整档 `rate` 支 VM 在这条轨迹的「第一个活点那一帧」一起起出来**
 *     （`yk_m_laser_beam_bars_resident()`），还没轮到条数的那几支写 `alpha 0`。
 *     上一版是「历史填到第几条就起几条」（一手也是这个节奏），但**我们的副本各是一支独立 VM、
 *     各自从出生帧开始积分 `scrollX`** ⇒ 按需长大时相邻副本的 UV 相位差 16 texel = ¼ 纹样周期，
 *     12 份互相平均 ⇒ 纹样对比从 0.384 掉到 0.085（owner 第三十四批第一条「看不清纹理」的**另一半**成因，
 *     另一半是 `YK_M_LASER_BEAM_ALPHA` 的 176 过曝）。一手是**同一支 VM 画 rate 次**（UV 不重算）
 *     ⇒ 必须同帧起才同形。**伤害与 `bars` 完全不受本改动影响**（`bars` 仍只由轨迹决定）。
 *   · **每台子机一条轨迹**（一手只用 `options[0]`；本卡 M 恒 1 台 ⇒ 逐字同形），
 *     一台都没有时回退本体 1 条（117 的同款兜底）。
 *
 * ## ★★★ 跟随（第三十四批，owner 第 4 条）：「**本体（以及伤害源）应当完全跟着子机定死**」
 *   · **一手**（`th08:0x450840`，逐指令）：先 `trail[i] = trail[i-1]; trail[i].y -= 1`，**再**把
 *     `vm+0x2a4`（= 上一帧存下的膛口）写进 `trail[0]`，**最后**把本帧膛口存回 `vm+0x2a4`
 *     ⇒ 一手那条束的**本体慢一帧**，横移时与子机错位（owner 报的正是这个）。
 *   · **本批**：第 0 格写**本帧**膛口（历史点的推进顺序与一手一致，只是整条历史不再延迟一帧）；
 *     **伤害源全部落在本帧膛口**（`yk_beam_bar_hit(mux, muy, d)`）。
 *     ⓘ 静止时两者**逐像素等价**（12 个轨迹点只差 1 px、判定盒 2048 长本来就互相重叠）⇒
 *     这只改「移动时判定跟着残影往后拖」那半，**逐条份额 / Σ = dmg / 一源一敌一次结算都不变**。
 *   · **判据**（可判、进 `HANDOFF-108.md` §3）：探针那一行 `trail0 (…) muzzle (…)` 的**两个数必须相等**，
 *     且每条残影行的 `pos`（`k = 0` 那条）读出来 = 同一个数；改前 `trail0` 会比 `muzzle` 落后一帧。
 *
 * ## 伤害（矩形伤害源，每 2 帧一笔 × 当帧那几条残影，**位置 = 本帧膛口**）
 *   · `ce_damage_rect(center, 0.0f, 2, dmg_k, 16, YK_LASER_LEN)`（`th18:0x45dfa0`）：`h = YK_LASER_LEN`
 *     沿 y、`w = 16` 沿 x，angle 0 ⇒ 竖直的长条（判定轴约定见 `AUDIT.md` §U10）。
 *     宽 16 照一手每条残影的 `SpawnShot(…, 16.0, 448.0, …)`；**长跟着视觉走**（`YK_LASER_LEN` = 2048，
 *     第三十一批起不再是硬写的 448）；`center = 本帧膛口 − YK_LASER_LEN/2·ŷ`（居中的盒心）。
 *   · 节奏 = **每 2 帧新建一笔**（一手 type 4/5/6 的 `vm[0x454+8] % 2`，`th08:0x4517b5`）；
 *     而一个源对**每个敌人一辈子只结算一次**（`0x45f0f0` 的 `+0x84` tag，一手
 *     `engine/enemy/th18/06-enemy-inert-flag.md` §③ 的 `0x45f45a`）⇒ 平均 = 表值 / 2 每帧。
 *   · **总额不变**：`dmg_k` 由 `yk_m_laser_beam_bar_damage(dmg, bars, k)` 分摊，`Σ_k = dmg`
 *     ⇒ 从「一条吃满」改成「几条分摊」，每帧平均与上一版**同额**（残影**不是** N 倍伤害）。
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
 *     商店里 tick 全停，只有 `on_delete` 跑得到 ⇒ 收尾**不依赖 tick**。
 *
 * ## ★★★ 2026-09-27 第二十八批（owner ⑥ 遗留 ❓ 收口）：**束根光斑 + 束端命中特效 + 极妖染红**
 * 三条一手的完整链条（脚本全文、逐指令、以及「TH08 查不到什么 + 试过哪些手段」）写在
 * `youkai_core.h` 的同一节，这里只说**落点**：
 *   · **束根光斑** = 原生 `ability.anm` script4（`YK_BEAM_CORE_SCRIPT`）：每台我们自己的子机**一支**，
 *     挂在**本帧的束近端**（轨迹第 0 格 —— 与束本体同一个锚点）。位置**每帧写**，
 *     **旋转与 scale 一律不碰**：自转（`angleVel`）与脉动（`scaleTime`）都写在脚本里，
 *     我们一写就会把积分器/插值器打掉。撤走/松 Z 时发 `interrupt 1`（script4 的收束段同样 8 帧）。
 *   · **束端命中特效** = 原生 `ability.anm` script5（`YK_BEAM_HIT_FX_SCRIPT`）：**一次性**（脚本自己
 *     `+20: delete()`）⇒ **不记 id**（本仓既有的「脚本自己 delete() 的一次性特效不用记 id」）。
 *     位置 = **(束轴 x, 命中点 y)** —— 一手 TH08 `0x450ee0` 逐指令就是「x 取束轴、y 取命中点」。
 *     ⚠ 它**只在真的有敌人挨着束时**才出（我们的束**不截断**，所以不能照 TH18 那样无条件放在
 *     几何束端 —— 那会是一颗飘在画面顶端的火花，TH18 那条之所以看不见是因为它的束长 512 > 448
 *     把端点顶出了游玩区）。判据 = 沿束轴那条带找**最近**（y 最大 = 先碰到）的一只可打敌人。
 *   · **极妖染红** = 复用 owner ④ 那套（`YK_FENGMA_TINT_RGB` + `yk_fengma_tint_dword`）：
 *     `s->gauge >= YK_GAUGE_YOUKAI_EFFECTS` 时每条残影束的低 24 位染红、**alpha 不动**
 *     （一手 `0x450bf2` 只写 B/G/R 三个字节）。切换当帧就变色（每帧都重写颜色，天然幂等）。*/
#define YK_BEAM_ANM_LAYER 14    /* 世界层 3..19（一手 `engine/anm/th18/02-render-stages.md` §1 的优先级表）。
                                 * 层号小 = 先画 = 被后画的盖住（同层内才看生成顺序）。
                                 *
                                 * ★★★ 2026-09-27 第二十七批（owner 一句话解开了死结）：
                                 *   「**子机 VM 层一定得是 14 吗？相当于你不用通常意义的子机 VM 层呢？
                                 *     反正这个子机也没动画**」——
                                 *   一手：**子机的 VM 层由 `Player__allocate_option` 写 14**
                                 *   （`th18:0x40a8bc mov DWORD PTR [esi+0x18],0xe`，另两处 `0x40ce78`/`0x40d012` 同），
                                 *   **自机弹的 VM 层 = 13**（零售 `pl01.anm` 弹体脚本逐条 `layer(13)`）。
                                 *   13/14 相邻、没有整数层可插 ⇒ 原来只能二选一（owner 两条反馈互相打架）。
                                 *   ⇒ 按 owner 的提法：**我们三支妖侧子机脚本（TH08 那三支，静态、本来无动画）
                                 *     显式写 `layer(15)`** 把 14 让出来 —— 脚本里的 `layer()` 在脚本执行时写
                                 *   `vm+0x18`，晚于 allocate 时的写入 ⇒ 生效（生成器
                                 *   `assets/ability/make_youkai_option_art.py` 的「层」一节）。
                                 *   ⇒ 最终顺序：**自机弹 13 < 束 14 < 妖侧子机 15** ✓
                                 *     「束在自机弹之上」+「束被子机盖住」两条**同时**成立。
                                 *   ⚠ 早苗那支（TH16 文的鸦）**原版自带 `layer(13)`** ⇒ 照抄不动（它有姿态动画、
                                 *     也没有束）⇒ 她的子机仍在 13：与她自己的弹同层（历史行为，未变）。*/
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
 * 不放 `yk_card_t`：那个结构已经 468 / 496 字节（`_Static_assert` 就在文件尾），只剩 28 字节。
 * ★ 第二十七批起是**二维**的：`[unit]` = 一条膛口轨迹（`YK_OPTION_CELLS` 台我们自己的子机
 * + 最后那一格 = 「一台都没有」时回退本体的兜底轨迹）、`[k]` = 这条轨迹上的第 k 条残影。
 * 本卡 M 恒 1 台（`YK_OPTION_BY_CHAR[1] = {1,1,1,1,1}`）⇒ 实际同时活着的只有 1×rate ≤ 12 支。*/
static float    s_beam_muz[YK_M_LASER_BEAM_UNITS][2];   /* 本帧的膛口（每帧现取一次）*/
static float    s_beam_trail[YK_M_LASER_BEAM_UNITS][YK_M_LASER_BEAM_TRAIL_MAX][2];  /* 膛口历史（x,y）*/
static uint32_t s_beam_vm[YK_M_LASER_BEAM_UNITS][YK_M_LASER_BEAM_BARS_MAX];        /* 每条残影的 VM id */
/* ★ 第二十八批：**每台子机一支**束根光斑（原生 script4）。它是有生命周期的常驻 VM（跟着束起落），
 * 所以必须记 id、必须在 `yk_beam_kill` 里一起收 —— 与下面那些一次性火花不同。*/
static uint32_t s_beam_core[YK_M_LASER_BEAM_UNITS];
static uint16_t s_beam_age;                       /* 本次激活的帧数（展开系数 + 结算节拍都用它）*/
static uint8_t  s_beam_live;                      /* 本帧起/刷了几条（诊断 + 日志判据）*/
static uint8_t  s_beam_logged;                    /* 「束起来」只记一行（几何自述）*/
static int32_t  s_beam_tier;                      /* 当前这几条束是按哪一档起的（-1 = 没起）*/
/* ★★ 第二十八批（A 类第 3 条：**池压力要可测**）：本帧「想要几支 / 真拿到几支」。
 * `_vm` = 常驻（残影束 + 束根光斑），`_fx` = 本帧的一次性命中特效。探针每 120 帧打这两个比值，
 * `dropped` 是**累计**起失败数（进 600 帧的 ledger 一行，与 `s_ring_failed` 同一个形状）。
 * ⚠ 判据的含义：**`got < want` 只影响观感** —— 伤害是按 `bars`（轨迹）分摊的、与 VM 起没起来无关
 * （见下面第 4 步的注），所以池满时不会丢 DPS。*/
static uint8_t  s_beam_vm_want, s_beam_vm_got;
static uint8_t  s_beam_fx_want, s_beam_fx_got;
static uint32_t s_beam_dropped;

/* 这一档的残影条数上限必须 ≥ 表里的最大值（`YK_M_LASER_BEAM_BARS_MAX` 是数组第二维）——
 * 单位测试里还有一条「max(表) == BARS_MAX」的断言，两处一起兜。*/
_Static_assert(YK_M_LASER_BEAM_BARS_MAX <= YK_M_LASER_BEAM_TRAIL_MAX,
               "bars per trail can never exceed the trail length");

/* 收干净（`fade=1` = 走脚本的收束段；`fade=0` = 当场删）。返回收掉几条。
 * ★ 第二十八批：**束根光斑一起收**（它是常驻 VM，漏了就是永久钉在膛口的一颗光斑）——
 *   两支脚本的收束段都是 `interrupt 1` = `scaleTime(8,1,0,0)` → `delete()`，同一支 8 帧，同拍淡出。*/
static int yk_beam_kill(int fade)
{
    int u, i, n = 0;
    for (u = 0; u < (int)YK_M_LASER_BEAM_UNITS; ++u) {
        for (i = 0; i < (int)YK_M_LASER_BEAM_BARS_MAX; ++i) {
            if (!s_beam_vm[u][i]) continue;
            ++n;
            if (fade) ce_anm_interrupt(s_beam_vm[u][i], 1);  /* 束脚本 interruptLabel(1)：scaleTime(8,1,1,0) → delete */
            else      ce_anm_delete(s_beam_vm[u][i]);
            s_beam_vm[u][i] = 0;
        }
        if (s_beam_core[u]) {                            /* ★ 第二十八批：束根那颗光斑（原生 script4）*/
            ++n;
            if (fade) ce_anm_interrupt(s_beam_core[u], 1);   /* script4 的 interruptLabel(1)：scaleTime(8,1,0,0) */
            else      ce_anm_delete(s_beam_core[u]);
            s_beam_core[u] = 0;
        }
    }
    if (n)
        ce_log("youkai/beam: off (%d VM(s) = afterimage beam(s) + root glow, fade %d, age %u, tier %d) - "
               "mode=2 engine beams are gone for good, so this is the only place they are cleaned",
               n, fade, (unsigned)s_beam_age, (int)s_beam_tier);
    s_beam_age = 0;
    s_beam_live = 0;
    s_beam_logged = 0;
    s_beam_tier = -1;
    s_beam_vm_want = s_beam_vm_got = 0;
    s_beam_fx_want = s_beam_fx_got = 0;
    return n;
}

/* 把一条残影摆到 `(x,y)`：位置 / −π/2 / 两轴 scale（**都乘展开系数**）/ 逐条 alpha / 极妖染红。
 * 拆成独立函数只为**压寄存器压力**（`ce_anm_set_*` 是内联函数，每个都要在 i686 上凑寄存器）。
 * `tint` = 极妖（`s->gauge >= YK_GAUGE_YOUKAI_EFFECTS`）⇒ 低 24 位染红、alpha 不动（一手 `th08:0x450bf2`）。
 *
 * ★★★ 2026-09-27 第三十二批：owner 报「**看起来不是从发射端长出**」（「117 的贴图是正确地从发射端长出的」）
 * 之后，本函数与 117 的 `eagle_beam_tick()` **逐行对照过一遍**（表在 `AUDIT.md` §BE #133 §5）：
 *   · **VM 位置 = 近端**（传进来的 `(x,y)` 是**轨迹点** = 上一帧的膛口；束心只出现在**判定盒**的
 *     `center = y − BAR_H/2`，那是 `ce_damage_rect`、不是 VM） —— 与 117 的「束建在本体位置」同形 ✓
 *   · **`scale.x` = 长度 ÷ 贴图逻辑宽**（`YK_LASER_LEN / SPRITE_W`，满长 2.6667；第三十三批前是 4.0）
 *     —— 与 117 的 `vis / SPRITE_W` 同形 ✓
 *   · **每帧 rotation = −π/2**，与脚本烘的 `rotate(0.0f, 0.0f, -1.5707964f)` 同向（脚本初值被覆盖）✓
 *   · 锚点在**图左端**（脚本 `anchor(1, 0)`）⇒ 近端钉在膛口、远端往外伸；`grow` 两轴同乘 ⇒ **从膛口往外长** ✓
 * ⇒ 「不像从发射端长出」的成因**不在这三处**，而在**贴图那一层**：每条残影各拿一个**随机 UV 相位**
 *   （12 份纹样互相平均 ⇒ 一条光滑亮条）+ 流速照抄 163 时超奈奎斯特（0.57 周期/帧的频闪）。
 *   两条都在**束脚本 + 卡侧常量**里改掉了（`YK_M_LASER_BEAM_SCROLL` / 生成器），**本函数因此一个字节没动**。
 *   ⚠ 下次若要再动本函数，先读 §BE #133 §5 —— 那里已经证明几何是 117 同形的，别再从几何猜病因。*/
static void __attribute__((noinline)) yk_beam_bar_place(uint32_t id, float x, float y,
                                                       float grow, float w_px, int alpha, int tint)
{
    (void)ce_anm_set_pos(id, x, y, 0.0f);
    (void)ce_anm_set_rotation(id, 0.0f, 0.0f, -YK_ATAN_HALF_PI);
    /* ★★ 117 的形状（`eagle_beam_tick`）：**两轴同乘展开系数**、`anchor(1,0)` 在近端
     * ⇒ 束从膛口**连续**往外伸到满长（上一版只乘 `scale.y` ⇒ 一出生就全长、只变粗）。
     * 束长 = `YK_LASER_LEN`（2048，第三十一批按 owner 口径不再照抄一手的 448）
     * ⇒ 系数里的 `YK_LASER_LEN / SPRITE_W` = **2.6667** 就是满长那一端
     *   （第三十三批：`SPRITE_W` 512 → 768 = 贴图铺满整条 entry 行宽；117 那边是它自己的 512×4.0）。*/
    (void)ce_anm_set_scale(id, YK_LASER_LEN / YK_M_LASER_BEAM_SPRITE_W * grow,
                           w_px / YK_M_LASER_BEAM_SPRITE_H * grow);
    /* 一手 `0x450ad0` 每画一条就改一次 `vm+0x1f3`（越旧越淡），画完再还原 ⇒ 这里等价于
     * 「每条 VM 常驻自己的 alpha」。RGB 默认全白（脚本里没有 color）；**极妖时整条染红**
     * （一手 `0x450bf2` 的 `k` 循环里写 RGB(255,64,64)，alpha 不动）—— 每帧重写 ⇒ 中途变妖也当帧生效。*/
    (void)ce_anm_set_color(id, yk_m_laser_beam_bar_color(alpha, tint));
}

/* 在轨迹点上挂一笔矩形伤害源（`life = SETTLE` ⇒ 一源一敌一辈子只结算一次，见块首的长注）。*/
static void __attribute__((noinline)) yk_beam_bar_hit(float x, float y, int dmg)
{
    float center[3];
    if (dmg <= 0) return;                       /* 分摊到 0 的条不占伤害源池 */
    center[0] = x;
    center[1] = y - YK_M_LASER_BEAM_BAR_H * 0.5f;   /* 盒心 = 轨迹点 − BAR_H/2（束向上伸）*/
    center[2] = 0.0f;
    (void)ce_damage_rect(center, 0.0f, YK_M_LASER_BEAM_SETTLE, dmg,
                         YK_M_LASER_BEAM_BAR_W, YK_M_LASER_BEAM_BAR_H);
}

/* ★★★ 第二十八批：**束端命中特效**（一手见 `youkai_core.h` 的 ②）。
 * 沿束轴那条带找**最近**（y 最大 = 束向上射时先碰到）的一只可打敌人；找到就在
 * **(束轴 x, 那个敌人的 y)** 起一支**一次性**的原生 `ability.anm` script5（脚本自己 `+20: delete()`）。
 * 返回 1 = 这一帧算「有命中」（起到了或起失败但本该有），0 = 没有目标。
 *
 * ⚠ 为什么不用「几何束端」：TH18 那条束命中时会被 `hit2` **截到接触点**，所以它的「束端」就是命中点；
 *   我们的束**不截断**（`YK_LASER_LEN` = 2048 ⇒ 整列贯通）⇒ 几何束端在画面顶端，照它摆会是一颗**飘在空中的火花**。
 *   TH08 那一手（`th08:0x450ee0`）正是把特效摆在**命中点**上（x 取束轴）⇒ 这里两个一手口径都指向
 *   「命中点」，故用敌人位置建判据。
 * ❓ 判据的两个近似（登记 `TUNING.md` §2.1ccc-8）：① 横向带宽 = 束半宽 8 + 特效半宽 16（TH18 的敌人
 *   判定盒字段图本批没解出来）；② 纵向用**敌人中心**的 y（引擎用的是接触点，差一个敌人的半高）。
 *   **两处都只影响火花画在哪，不参与伤害** —— 伤害照旧由上面那支 `ce_damage_rect` 的几何求交决定。
 * `noinline`：调用者 `yk_beam_tick` 里已经有内联汇编（`ce_damage_rect`），本函数又带一个链表遍历。*/
static int __attribute__((noinline)) yk_beam_hit_fx_try(float bx, float by)
{
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    uint32_t id;
    float best_y = 0.0f;
    int found = 0;

    if (!em) return 0;
    CE_ENEMY_FOREACH(em, e) {
        const float *p;
        if (!ce_enemy_lockable(e)) continue;
        if (*(const uint32_t *)(e + CE_ENEMY_FLAGS) & CE_ENEMY_FLAG_NO_DAMAGE) continue;  /* 免伤位 ⇒ 打不掉 */
        p = ce_enemy_pos(e);
        if (!yk_m_laser_beam_fx_on_beam(p[0], p[1], bx, by)) continue;
        if (!found || p[1] > best_y) { best_y = p[1]; found = 1; }   /* 最近 = y 最大（先碰到）*/
    }
    if (!found) return 0;
    ++s_beam_fx_want;
    id = yk_vm_spawn(CE_ABILITY_ANM(), YK_BEAM_HIT_FX_SCRIPT, YK_BEAM_HIT_FX_LAYER);
    if (!id) { ++s_beam_dropped; return 1; }        /* 池满 ⇒ 这一帧没有火花（纯观感，伤害照旧）*/
    ++s_beam_fx_got;
    /* script5 的层号由它自己写 `layer(15)`（脚本的 `layer()` 晚于 instantiate 的层参数 ⇒ 参数只作记录）。
     * 位置这里现写：`ce_anm_spawn` 建出来是 (0,0,0)，我们在同一帧（绘制之前）把它挪到命中点上。*/
    (void)ce_anm_set_pos(id, bx, best_y, 0.0f);
    return 1;
}

/* 每帧：妖侧（低速）+ 魔理沙 + 按住 Z ⇒ 对**每台我们自己的子机**推一格膛口轨迹，并在轨迹点上
 * 摆出 `YK_M_LASER_BEAM_RATE[tier]` 条残影束（越旧越淡）、每 `SETTLE` 帧把这一档的伤害分摊到它们头上；
 * 一台都没有（池满 / 声明没落笔）⇒ 回退本体 1 条轨迹（117 的同款兜底：「否则看着像死掉」）。
 * 其余情况全收（含松 Z）。
 * ⚠ `noinline` 是**必须**的（本仓踩过两次）：本函数内联着 `ce_damage_rect`（`sdk.h:167` 的内联汇编），
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
    const int rate = yk_m_laser_beam_rate(tier);
    const float w_px = (float)w100 * 0.01f;
    /* ★ 第二十八批：极妖 ⇒ 残影束整条染红（一手 `th08:0x450bf2`）。判据与 owner ④ 的自机弹染色同源
     * （`yk_gauge_is_extremely_youkai` = 一手 `th08:0x406d70` 的 `≥`）。每帧重算 ⇒ 中途进/出极妖当帧变色。*/
    const int tint = yk_gauge_is_extremely_youkai((int)s->gauge);
    float grow;                      /* 展开系数（每次激活从 0 到 1）——**每帧重算**：见下面的 spawn 分支 */
    int units[YK_M_LASER_BEAM_UNITS];/* 本帧要推哪几条轨迹（下标 = 子机格号；兜底 = YK_OPTION_CELLS）*/
    float mux[YK_M_LASER_BEAM_UNITS];/* 本帧的膛口 x（下一帧才进轨迹 —— 一手 `vm+0x2a4` 的时序）*/
    float muy[YK_M_LASER_BEAM_UNITS];/* 同上 y */
    int nunit = 0, i, n = 0;
    /* ★ 第二十八批（A 类第 3 条「池压力要可测」）：本帧想要的常驻 VM 数 = 整档条数 + 每 trail 一支光斑；
     *   `n` = 真摆出来的**看得见**的（含复用，含光斑），`nres` = 摆出来的**常驻残影**（含 alpha 0 的那几支，
     *   见 `yk_m_laser_beam_bars_resident()`）。`ncore` 从中拆出来，只为让 on 那一行把「残影 / 光斑」分开报。*/
    int want = 0, ncore = 0, nres = 0;

    if (!s || !CE_PLAYER() || !yk_focused() || character != YK_CHAR_MARISA || !yk_shooting()) {
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
    /* 换档 ⇒ 脚本号与残影条数都变了 ⇒ 先硬收，本帧重起（历史一并从哨兵重来）。*/
    if (s_beam_tier >= 0 && s_beam_tier != tier) yk_beam_kill(0);
    /* ⚠ `grow` 必须在这里、**每帧**从 `s_beam_age` 现算（不能提到函数开头）：上面那几支
     *   「收掉 / 重起」都会把 age 归零 ⇒ 提前算出来的系数会是**上一段激活**的残留
     *   （第一版就是这么写的：重起那一帧画的是旧 age 的宽度）。117 的 `eagle_beam_grow` 也是
     *   在 spawn 之后才取。*/
    grow = (float)yk_m_laser_beam_grow_permille(s_beam_age) * 0.001f;

    /* ---- 1. 本帧的膛口（每条轨迹一个）---------------------------------------------------
     * 一手只有 `options[0]` 一个源（`th08:0x450a21 add edx,0x6b0` 硬取 options[0]）；我们按
     * 「每台我们自己的子机各一条轨迹」展开（本卡 M 恒 1 台 ⇒ 与一手逐字同形）。
     * ⚠ 取到的是**本帧**的膛口；★ 第三十四批起它**当帧**就进 `trail[0]`（一手 `vm+0x2a4` 的写法是
     *   `trail[0] = vm+0x2a4` 在前、`vm+0x2a4 = 本帧膛口` 在后 ⇒ 轨迹整体慢一帧 —— owner 要的是
     *   「定死」，见本节「跟随」那一段）。本帧的值先进 `mux/muy`，第 3 步同时落到 `trail[0]` 与
     *   `s_beam_muz`（后者只是探针那一行的 `muzzle` 读数）。*/
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
        mux[i] = x;
        muy[i] = y;
        units[nunit++] = i;
    }
    if (nunit == 0) {
        /* 回退：本体正上方（117：「子机数为 0（火力 <1.00）时才回退成自机本体的一条」）*/
        mux[YK_OPTION_CELLS] = pp[0];
        muy[YK_OPTION_CELLS] = pp[1];
        units[nunit++] = (int)YK_OPTION_CELLS;
    }

    /* ---- 2. 激活那一帧：整条轨迹清成哨兵（一手 `0x44fe20` 把 32 格的 x 全写 −999，
     *         并把 `vm+0x2a4.x` 也清成 −999 ⇒ **第一帧推不出活点**、一条残影都不出）------------
     * （`s_beam_muz` 不再是「留给下一帧的膛口」，它是**本帧膛口**的记录 = 探针那一行的 `muzzle`；
     *   第 0 帧也照写本帧真值 ⇒ 与 `trail[0]` 的对比在激活帧就有意义。）*/
    if (s_beam_age == 0u) {
        int u, k;
        for (u = 0; u < (int)YK_M_LASER_BEAM_UNITS; ++u)
            for (k = 0; k < (int)YK_M_LASER_BEAM_TRAIL_MAX; ++k)
                s_beam_trail[u][k][0] = YK_M_LASER_BEAM_TRAIL_NONE;
    }

    /* ---- 3. 每条轨迹：推一格 → 摆残影 + 束根光斑 → （按节拍）分摊伤害 + 束端命中特效 ---------*/
    for (i = 0; i < nunit; ++i) {
        const int u = units[i];
        float *trail = &s_beam_trail[u][0][0];
        int bars, resident, k;

        /* ★★★ 第三十四批（owner 第 4 条「**本体以及伤害源完全跟着子机定死**」）：
         * 一手 `0x450840` 的顺序是「先整体上移一格、再把 `vm+0x2a4`（= **上一帧**存下的膛口）写进第 0 格」，
         * 之后才把本帧膛口存回 `vm+0x2a4` ⇒ 一手那条束的**本体慢一帧**（横移时与子机错位，
         * owner 报的就是这个）。本批把第 0 格改成写**本帧**膛口：历史点的推进仍是
         * 「先 `trail[i] = trail[i-1]` 再写本帧」（与一手同序），只是整条历史**不再延迟一帧**。
         * ⇒ 不变量：同一帧里 `trail[0] == s_beam_muz[u] ==` 本帧膛口（探针行 `trail0 … muzzle …` 必须相等）。
         * 第 0 帧不推（轨迹全是哨兵 ⇒ 一条都不出，与一手「激活帧不出」同形）。*/
        if (s_beam_age != 0u)
            yk_m_laser_beam_trail_step(trail, (int)YK_M_LASER_BEAM_TRAIL_MAX, mux[u], muy[u]);
        s_beam_muz[u][0] = mux[u];        /* 本帧膛口（探针的 `muzzle`；历史已在上面落进 trail[0]）*/
        s_beam_muz[u][1] = muy[u];
        /* 本帧出几条：从第 0 格起数**连续**的活点（一手 draw 遇哨兵就 break），上限 = 本档 rate。*/
        bars = yk_m_laser_beam_bar_count(trail, (int)YK_M_LASER_BEAM_TRAIL_MAX, rate);
        if (bars > (int)YK_M_LASER_BEAM_BARS_MAX) bars = (int)YK_M_LASER_BEAM_BARS_MAX;

        /* ★★★ 第三十四批：**本帧要常驻几支 VM** = 整档 `rate`（不是「历史填到几条」，`bars` 只管
         * 「哪几条看得见」）。理由（一手 + 逐像素模拟，两条独立证据）写在
         * `yk_m_laser_beam_bars_resident()` 的长注里：一手 `0x450ad0` 把**同一支 VM** 画 `rate` 次、
         * **UV 从不重算** ⇒ 一手的副本共享相位；我们各支各自积分 ⇒ 若按需长大，相邻副本相位差
         * 16 texel = ¼ 纹样周期、12 份互相平均 ⇒ 纹样对比从 0.384 掉到 0.085（owner 的「看不清纹理」）。
         * 还没轮到条数的那几支：位置照写轨迹（哨兵 = 屏外）、**alpha 写 0** ⇒ 加算下贡献恒 0。*/
        resident = yk_m_laser_beam_bars_resident(bars, rate);

        /* ★ 第二十八批（池压力可测）：本帧「想要几支常驻 VM」= 整档条数 + （下面）一支光斑。
         * ⚠ **只用来记账**，绝不参与 `bars` 的决定 —— `bars` 同时是伤害的分摊基数，
         *   拿计数器去钳它会**悄悄改伤害**（那是本仓最不该犯的错）。溢出在收尾统一钳。*/
        want += resident;
        for (k = 0; k < resident; ++k) {
            const float x = trail[k * 2 + 0];
            const float y = trail[k * 2 + 1];
            const int vis = (k < bars);
            if (!s_beam_vm[u][k] || !ce_anm_get_vm(s_beam_vm[u][k])) {
                s_beam_vm[u][k] = yk_vm_spawn(yk_player_anm(), YK_BEAM_SCRIPT[tier], YK_BEAM_ANM_LAYER);
                if (!s_beam_vm[u][k]) {               /* 池满 ⇒ 这一帧到此为止 */
                    ++s_beam_dropped;                 /* ★ 记账：**只少一条观感**，见下面伤害那一段 */
                    break;
                }
            }
            yk_beam_bar_place(s_beam_vm[u][k], x, y, grow, w_px,
                              vis ? yk_m_laser_beam_bar_alpha(YK_M_LASER_BEAM_ALPHA, k, rate) : 0, tint);
            ++nres;
            if (vis) ++n;
        }
        /* 收掉这一档用不到的尾巴（换档变小 / 激活重来时会留几支）*/
        for (k = resident; k < (int)YK_M_LASER_BEAM_BARS_MAX; ++k) {
            if (!s_beam_vm[u][k]) continue;
            ce_anm_delete(s_beam_vm[u][k]);
            s_beam_vm[u][k] = 0;
        }
        /* ---- 束根那颗自转光斑（★ 第二十八批；原生 `ability.anm` script4）----------------------
         * 每台我们自己的子机**一支**，钉在**本帧的束近端**（轨迹第 0 格 = 束本体那个锚点 = 本帧膛口）。
         * 一手的 script4 自带 `angleVel`（自转）与 `scaleTime` 脉动 ⇒ **我们只写位置**，
         * 旋转 / scale 一律不碰（一写就把积分器与插值器打掉，光斑就不转了）。*/
        if (bars > 0) {
            ++want;                                   /* 光斑也是本帧想要的常驻 VM 之一 */
            if (!s_beam_core[u] || !ce_anm_get_vm(s_beam_core[u])) {
                s_beam_core[u] = yk_vm_spawn(CE_ABILITY_ANM(), YK_BEAM_CORE_SCRIPT,
                                             YK_BEAM_CORE_ANM_LAYER);
                if (!s_beam_core[u]) ++s_beam_dropped;    /* 池满 ⇒ 少一颗光斑（观感）*/
            }
            if (s_beam_core[u]) {
                (void)ce_anm_set_pos(s_beam_core[u], trail[0], trail[1], 0.0f);
                ++n;
                ++ncore;
            }
        } else if (s_beam_core[u]) {                  /* 第 0 帧（还没有轨迹点）⇒ 不该有光斑 */
            ce_anm_delete(s_beam_core[u]);
            s_beam_core[u] = 0;
        }
        /* 伤害：每 `SETTLE` 帧把这一档的 `dmg` 点**摊到 bars 条**上（Σ = dmg ⇒ 总额与上一版同额）。
         * ★★★ **伤害源的位置：全部落在「本帧膛口」**（owner 第 4 条「以及伤害源……跟着子机定死」）——
         *   改前是逐条挂在各自的轨迹点上（= 一手 `0x450840` 那个「每条残影各发一发 `SpawnShot`」），
         *   横移时判定跟着残影往后拖。ⓘ **这与「站着不动」时的判定完全等价**：静止时 12 个轨迹点
         *   本来就只差 1 px（`TRAIL_DY`），12 个盒互相重叠 ⇒ 本批只是把「移动时也跟着漂」那半去掉。
         *   ⇒ 逐条份额、`Σ = dmg`、每条对每个敌人一辈子结算一次（`0x45f0f0` 的 `+0x84` tag）**都不变**。
         * ★★ 池压力的判据就在这一段的「与 VM 无关」上：`bars` 来自**轨迹**（还没到的历史点不算），
         *   而 `ce_anm_spawn` 起不起得来**不参与**这个循环 ⇒ 池满时（探针里 `got < want`）
         *   **伤害一个点都不少**，少的只是画面上的残影。*/
        if (bars > 0 && yk_m_laser_beam_should_settle(s_beam_age)) {
            for (k = 0; k < bars; ++k) {
                const int d = yk_m_laser_beam_bar_damage(dmg, bars, k);
                if (d <= 0) continue;
                yk_beam_bar_hit(mux[u], muy[u], d);       /* ★ 本帧膛口（不再用 trail[k]）*/
            }
            /* 束端命中特效（★ 第二十八批；原生 `ability.anm` script5）：同一支节拍、同一处命中点。
             * 用**判定盒的近端**（轨迹第 0 格 = 本帧膛口）而不是视觉束端 —— 判定长恒 = `YK_LASER_LEN`
             * （TUNING §2.1ccc-6 第 10 行）。*/
            if (yk_m_laser_beam_fx_due(s_beam_age))
                (void)yk_beam_hit_fx_try(trail[0], trail[1]);
        }
    }

    s_beam_live = (uint8_t)(n > 255 ? 255 : n);
    s_beam_vm_want = (uint8_t)(want > 255 ? 255 : want);
    s_beam_vm_got = (uint8_t)(nres + ncore > 255 ? 255 : nres + ncore);
    s_beam_tier = tier;
    ++s_beam_age;
    /* ⚠ 日志**每次激活只打一行**，而且打在「第一条残影真的出来」的那一帧上（`n > 0`）——
     *   第 0 帧（激活帧）按一手一条都不出（轨迹全是哨兵）；若照 `spawned` 打，条数逐帧长大
     *   ⇒ 一次按 Z 会连打 `rate`（最多 12）行。判据行仍是 `youkai/beam: on (` … `)` 与 `off (` … `)` 成对。*/
    if (!s_beam_logged && n > 0) {
        s_beam_logged = 1;
        ce_log("youkai/beam: on (%d afterimage beam(s) + %d root glow now, %d resident bar VM(s), "
               "up to %d visible per trail over %d trail(s) "
               "at tier %d, script %d, layer %d; hit fx = ability.anm script %d every %d frame(s), tint %d) - "
               "visual %.0f x %.1f px (scale %.3f x %.3f), bar box %.0f x %.0f px, "
               "%d dmg spread over the bar(s) every %d frame(s), no cancel; trail %d points, dy -%.0f "
               "(TH08 `th08:0x450840`/`0x450ad0`/`0x450ee0`)",
               n - ncore, ncore, nres, rate, nunit, tier, (int)YK_BEAM_SCRIPT[tier], (int)YK_BEAM_ANM_LAYER,
               (int)YK_BEAM_HIT_FX_SCRIPT, (int)YK_M_LASER_BEAM_FX_EVERY, tint,
               (double)YK_LASER_LEN, (double)w_px,
               (double)(YK_LASER_LEN / YK_M_LASER_BEAM_SPRITE_W),
               (double)(w_px / YK_M_LASER_BEAM_SPRITE_H),
               (double)YK_M_LASER_BEAM_BAR_W, (double)YK_M_LASER_BEAM_BAR_H,
               dmg, (int)YK_M_LASER_BEAM_SETTLE,
               (int)YK_M_LASER_BEAM_TRAIL_MAX, (double)YK_M_LASER_BEAM_TRAIL_DY);
    }
}


/* ★★★ 第三十四批：**「本体与判定钉在本帧膛口」的判据量** = `max(|Δx|, |Δy|)`（单位 = 1/100 px）。
 * 返回值是**整数**：本仓 `make dllx87` 要求我们自己的目标文件里零 x87，而 i386 上 float 返回必然过 x87
 * （`yk_m_laser_beam_fx_on_beam()` 是同一个形状：float 入参、整数返回）。
 * **0 = 完全钉死**（探针那一行打出来的 `d(trail0,muzzle) 0.00`）。*/
static int yk_beam_pin_delta100(float ax, float ay, float bx, float by)
{
    float dx = ax - bx, dy = ay - by, m;
    if (dx < 0.0f) dx = -dx;
    if (dy < 0.0f) dy = -dy;
    m = (dx > dy) ? dx : dy;
    return (int)(m * 100.0f + 0.5f);
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
        /* ★ 2026-09-27（第二十六批起，第二十七批改成残影多束）：束走卡侧 ⇒ 探针读**新路**：
         *   ① 每条残影 VM 的实况（轨迹下标 / 位置 / 旋转 / scale / alpha）；
         *   ② 本档的**残影条数**与轨迹读数（`trail[k]`，期望：`k = 0` 与本帧膛口一致、
         *      相邻两点 y 差 ≥ 1，x 相同则说明自机没横向移动）；
         *   ③ **载体行有没有被引擎发**：数一遍弹池里 packed set/row = 我们那条激光行的弹。
         *      必须恒 **0**（行里 `start_delay = 15` 不可达，`youkai_core.h` 那段有一手的充要证明）
         *      —— 这一行就是「引擎不再产生任何东西」的实跑判据；一旦非 0 说明行被改回可发了。
         *   ★★★ **第三十四批的三条判据行**（`HANDOFF-108.md` §3 第 36/37 行）：
         *   · **跟随**：汇总行的 `trail0 (x,y)` 与 `muzzle (x,y)` **必须逐字相等**（同一帧同一对），
         *     且 `bar 0` 那行的 `pos` 也等于它 —— 改前 `trail0` 恰好落后一帧的膛口；
         *   · **亮度**：`c1` 的高字节 = 该条残影的 alpha ⇒ `bar 0` = `46ffffff`（= 70 = 一手 `alpha(70)`）、
         *     `bar 1..rate-1` 逐条递减（档 0/1 = `46/1aff`）、**没轮到条数的那几支 = `00ffffff`**（alpha 0）；
         *   · **相位**：同一档的所有副本必须**同一帧起**（⇒ 共享 `scrollX` 相位，与一手「同一支 VM
         *     画 rate 次」同形）。判据 = 汇总行的 `resident` 在**束刚亮起那一帧就已经 = 整档**
         *     （`on (…)` 那行的 `%d resident bar VM(s)`；本仓没有可读的 VM 年龄字段，
         *     所以这条判据落在「起满的时机」上）。*/
        const int tier = yk_current_power_level();
        const int w100 = yk_m_laser_beam_w_px100(tier);
        const int dmg = yk_m_laser_beam_damage(tier);
        const int rate = yk_m_laser_beam_rate(tier);
        /* ★ 跟随判据量（第三十四批）：同一帧里 `trail[0]`（本体锚）与 `muzzle`（子机膛口）的距离。*/
        const int pin100 = yk_beam_pin_delta100(s_beam_trail[0][0][0], s_beam_trail[0][0][1],
                                                s_beam_muz[0][0], s_beam_muz[0][1]);
        uint8_t *pl = CE_PLAYER();
        int u, i, k, nrow = 0;

        for (u = 0; u < (int)YK_M_LASER_BEAM_UNITS; ++u) {
            uint32_t cid = s_beam_core[u];
            uint8_t *cv = cid ? (uint8_t *)ce_anm_get_vm(cid) : 0;
            if (cid)
                ce_log("youkai/probe beam unit %d core: vm %08x live %d pos(%.1f,%.1f) rz %.3f wz %.4f "
                       "(native ability.anm script %d, layer %d; rotation/scale are the SCRIPT's)",
                       u, (unsigned)cid, cv ? 1 : 0,
                       cv ? (double)*(const float *)(cv + CE_ANM_VM_POS) : 0.0,
                       cv ? (double)*(const float *)(cv + CE_ANM_VM_POS + 4) : 0.0,
                       cv ? (double)*(const float *)(cv + CE_ANM_VM_ROTATION + 8) : 0.0,
                       cv ? (double)*(const float *)(cv + CE_ANM_VM_ANGVEL + 8) : 0.0,
                       (int)YK_BEAM_CORE_SCRIPT, (int)YK_BEAM_CORE_ANM_LAYER);
            for (i = 0; i < (int)YK_M_LASER_BEAM_BARS_MAX; ++i) {
                uint32_t id = s_beam_vm[u][i];
                uint8_t *v = id ? (uint8_t *)ce_anm_get_vm(id) : 0;
                if (!id) continue;
                ce_log("youkai/probe beam unit %d bar %d: vm %08x live %d pos(%.1f,%.1f) "
                       "rot(%.3f,%.3f,%.3f) scale(%.3f,%.3f) c1 %08x trail(%.1f,%.1f) age %u via=%s",
                       u, i, (unsigned)id, v ? 1 : 0,
                       v ? (double)*(const float *)(v + CE_ANM_VM_POS) : 0.0,
                       v ? (double)*(const float *)(v + CE_ANM_VM_POS + 4) : 0.0,
                       v ? (double)*(const float *)(v + CE_ANM_VM_ROTATION) : 0.0,
                       v ? (double)*(const float *)(v + CE_ANM_VM_ROTATION + 4) : 0.0,
                       v ? (double)*(const float *)(v + CE_ANM_VM_ROTATION + 8) : 0.0,
                       v ? (double)*(const float *)(v + CE_ANM_VM_SCALE) : 0.0,
                       v ? (double)*(const float *)(v + CE_ANM_VM_SCALE + 4) : 0.0,
                       v ? (unsigned)*(const uint32_t *)(v + CE_ANM_VM_COLOR1) : 0u,
                       (double)s_beam_trail[u][i][0], (double)s_beam_trail[u][i][1],
                       (unsigned)s_beam_age, (u == (int)YK_OPTION_CELLS) ? "body" : "option");
            }
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
        ce_log("youkai/probe beam: %d afterimage(s) live, %d resident bar VM(s), tier %d "
               "(script %d, layer %d, w %.2f px, "
               "%d dmg / %d frame spread over up to %d bar(s) at the muzzle), trail0 (%.1f,%.1f) "
               "muzzle (%.1f,%.1f) d(trail0,muzzle) %d.%02d px [body + damage are pinned to THIS frame's "
               "muzzle => d MUST be 0.00], engine bullets from our laser row = %d (MUST be 0)",
               (int)s_beam_live, (int)s_beam_vm_want, tier, (int)YK_BEAM_SCRIPT[tier], (int)YK_BEAM_ANM_LAYER,
               (double)w100 * 0.01, dmg, (int)YK_M_LASER_BEAM_SETTLE, rate,
               (double)s_beam_trail[0][0][0], (double)s_beam_trail[0][0][1],
               (double)s_beam_muz[0][0], (double)s_beam_muz[0][1],
               pin100 / 100, pin100 % 100, nrow);
        /* ★★ 第二十八批（A 类第 3 条）：**池压力的可测判据** —— 「本帧想要几支 / 真拿到几支」。
         *   `resident` = 残影束 + 束根光斑（常驻，池满时 `got < want` ⇒ 画面少东西、**伤害不变**）；
         *   `fx` = 本帧的一次性束端命中特效（自删，20 帧）；`dropped` = **累计**起失败数。
         *   `tint 1` = 极妖（每条残影的 `c1` 应当读成 `aaff4040`；`0` 时是 `aaffffff`）。*/
        ce_log("youkai/probe beam spawn: resident %u/%u (want/got this frame), fx %u/%u, dropped %u total, "
               "tint %d (extreme youkai), core script %d, hit fx script %d",
               (unsigned)s_beam_vm_want, (unsigned)s_beam_vm_got,
               (unsigned)s_beam_fx_want, (unsigned)s_beam_fx_got, (unsigned)s_beam_dropped,
               yk_gauge_is_extremely_youkai((int)s->gauge),
               (int)YK_BEAM_CORE_SCRIPT, (int)YK_BEAM_HIT_FX_SCRIPT);
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

/* ★★ 2026-10-02（Jera 88「杰拉」）：本卡场上的**刻符（永点）**交给 SDK 的「复制代数闸」——
 * 每帧上报可见数，代数一变就把活着的刻符按**同坐标 / 同出生态 / 同「逢魔直接吃掉」标志**各重撒一颗
 * （`yk_orb_spawn_ex` 是它唯一的出生口 ⇒ 复制品与原件逐字同源）。
 * 语义见 `sdk_core.h` 末节与 `cards/jera.c` 文件头。
 * ⚠ 先存下标集合：`yk_orb_spawn_ex` 会找**空槽**（不一定是池尾）⇒ 遍历中新增的槽可能被下一轮扫到。*/
static uint32_t s_dup_seen;

static void yk_dup_run(yk_card_t *s)
{
    unsigned i, n = 0;
    uint8_t was[YK_ORB_MAX];
    for (i = 0; i < YK_ORB_MAX; ++i) was[i] = s_orb[i].used;         /* 快照：只看复制前就活着的那批 */
    for (i = 0; i < YK_ORB_MAX; ++i) {
        const yk_orb_t *o = &s_orb[i];
        if (!was[i] || !o->used) continue;
        yk_orb_spawn_ex(s, o->x, o->y, (int)o->state, (int)o->eat_at_toss);
        ++n;
    }
    ce_log("youkai: jera dup %u orbs (live before %u)", n, s_orb_live);
}

static int on_tick_2(ce_card_t *c)
{
    /* ★★ 桥 —— 必须**第一件事**：★ 109（id 更大 ⇒ 本卡之后跑）会清输入块的 Z 位 ✓，
     *   ★ 所以本卡要在这里先把**键盘 Z** 抄下来 ✓（见 `yk_shooting` 那段长注 ✓）。*/
    s_z_bridge = ce_input_z_frozen;   /* ★ 读共享槽 ✓（109 先存后清 ✓）—— 不再自己抄键盘 Z ✗ */
    if (!YK_ENABLE_CARD || !YK_ENABLE_CB_TICK2) return 0;   /* ★ 总开关 + 回调族 ⑨ */
    yk_card_t *s = ce_state(c, yk_card_t);
    int focused;
    static int8_t s_optmode_logged;
    if (!s) return 0;
    ce_dup_publish((uint32_t)ce_card_id(c), (int)s_orb_live);
    if (ce_dup_should_run(&s_dup_seen)) yk_dup_run(s);
    if (!s_optmode_logged) {                              /* ★ 工单③：日志里要能看出是哪个二分版 */
        s_optmode_logged = 1;
        ce_log("youkai: bisect switches set_index=%d native_suppress=%d option_pose=%d hud_vm=%d "
               "fx_vms=%d transfer=%d zone=%d orbit=%d map_layer=%d human_opt_write=%d "
               "card=%d score_decl=%d fengma_score=%d pose_declare=%d hud_init=%d "
               "cb_life=%d cb_stage=%d cb_shooters=%d cb_tick2=%d cb_bullet=%d cb_item=%d cb_draw=%d "
               "cb_edeath=%d cb_dbomb=%d orb_vm=%d orb_list=%d orb_zone=%d orb_drect=%d "
               "orb_playerw=%d (1 = 现状)",
               (int)YK_ENABLE_SET_INDEX, (int)YK_ENABLE_NATIVE_SUPPRESS, (int)YK_ENABLE_OPTION_POSE,
               (int)YK_ENABLE_HUD_VM, (int)YK_ENABLE_FX_VMS, (int)YK_ENABLE_TRANSFER,
               (int)YK_ENABLE_ZONE, (int)YK_ENABLE_ORBIT, (int)YK_ENABLE_MAP_LAYER,
               (int)YK_HUMAN_OPT_WRITE, (int)YK_ENABLE_CARD, (int)YK_ENABLE_SCORE_DECL,
               (int)YK_ENABLE_FENGMA_SCORE, (int)YK_ENABLE_POSE_DECLARE, (int)YK_ENABLE_HUD_INIT,
               (int)YK_ENABLE_CB_LIFECYCLE, (int)YK_ENABLE_CB_STAGE, (int)YK_ENABLE_CB_TICK_SHOOTERS,
               (int)YK_ENABLE_CB_TICK2, (int)YK_ENABLE_CB_BULLET, (int)YK_ENABLE_CB_ITEM,
               (int)YK_ENABLE_CB_DRAW, (int)YK_ENABLE_CB_ENEMY_DEATH, (int)YK_ENABLE_CB_DEATHBOMB,
               (int)YK_ENABLE_ORB_VM, (int)YK_ENABLE_ORB_LIST, (int)YK_ENABLE_ORB_ZONE,
               (int)YK_ENABLE_ORB_DRECT, (int)YK_ENABLE_ORB_PLAYERW);
        ce_log("youkai: human option-write mode = %s (%s) - owner laser bisect switch",
               YK_HUMAN_OPT_WRITE ? "ON (current)" : "OFF (human side fully hands-off)",
               "YK_HUMAN_OPT_WRITE");
    }
    if ((++s_ledger_frame % 600u) == 0u && (s->spawns || s->broken || s->lost || s->silent))
        ce_log("youkai: ledger - %u spawned, %u killed, %u withdrew, %u silent, %u dropped, "
               "%u body-emitter slot(s) skipped (#37: no familiar when the emitter is the enemy itself)",
               s->spawns, s->broken, s->lost, s->silent, s->dropped, s->emit_body_skipped);
    /* ★★★ 2026-10-01（工单④探针）：**我们的特效 VM 压力**（每 120 帧一行，只在有活动时打）。
     * `fail` 是判据：一直 0 ⇒ 「池满/起不出 VM」这条线可以排除（`0x489320` 本来就是 freelist 空
     * 就 malloc 的按需增长分配器，见 `YK_FX_VMS` 那一段）。`skip` = 二分版跳过的次数。*/
    {
        uint32_t now = (uint32_t)ce_sdk_frame();
        if (!s_fx_win_start) s_fx_win_start = now;
        if (now - s_fx_win_start >= 120u) {
            if (s_fx_try_win || s_fx_fail_win || s_fx_skip) {
                ce_log("youkai/fx: our FX VM spawns in the last %u frame(s): try %u fail %u "
                       "(cumulative try %u fail %u skipped %u) YK_FX_VMS=%d",
                       now - s_fx_win_start, s_fx_try_win, s_fx_fail_win, s_fx_try, s_fx_fail,
                       s_fx_skip, (int)YK_FX_VMS);
            }
            s_fx_try_win = 0; s_fx_fail_win = 0; s_fx_win_start = now;
        }
    }
    if (s_ring_failed && (s_ledger_frame % 600u) == 0u)
        ce_log("youkai: switch ring VM spawn FAILED %u time(s) total (cosmetic only - the Shift "
               "magic circle was missing)", s_ring_failed);
    /* ★★ 第二十八批（A 类第 3 条）：**池压力的降级路径有账** —— 束那一族（残影束 / 束根光斑 / 束端命中特效）
     * 起不出 VM 时只少观感：伤害是按**轨迹点**分摊后由 `ce_damage_rect` 交的，与 VM 起没起来无关
     * （见 `yk_beam_tick` 里那段注 + 探针那行 `resident want/got`）。这一行就是「真的发生过降级」的判据。*/
    if (s_beam_dropped && (s_ledger_frame % 600u) == 0u)
        ce_log("youkai/beam: %u VM spawn FAILED so far (anm pool full?) - cosmetic only: the damage is "
               "still spread over every trail point and the bar box is unchanged", s_beam_dropped);
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
    if ((s_ledger_frame % 120u) == 0u && yk_focused()) {
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
            ce_log("youkai: gauge=%d piv=%d orbs=%d taken=%u live=%u peak=%u full=%u hud=%u "
                   "speed=%.0f pick=%.0f shoot=%d fengma=%u fscore=%d "
                   "score=%d dscore=%d fengma=%d spiral_orbs=%u piv_pool=%d piv_show=%d pivw=%d bullet_hit_orbs=%u hitpos_swap=%u graze_pos_bullet=%u fengma_eaten=%u "
                   "graze_n 1/2/3=%u/%u/%u graze_gate ok/boss/gauge/shottype/off=%u/%u/%u/%u/%u clamped=%u",
                   (int)s->gauge, (int)s->piv, (int)s->orbs, s->orb_taken, s_orb_live,
                   (unsigned)s->orb_peak, s->orb_full, (unsigned)s->hud_ready, sp, rr, yk_shooting(),
                   (int)CE_SCORE(), (int)(CE_SCORE() - s->score_seen), (unsigned)s->fengma,
                   s_spiral_orbs,
                   (int)s->fengma_score,
                   /* ★ 2026-09-27（§BE #137）：这两个数 = owner 报的那条能不能过的**判据**
                    * （`piv_pool` = 左下角那个数画的东西、`piv_wrote` = 本卡写进去的那一份）。*/
                   (int)ce_piv_pool(), (int)(s->piv - s->piv_base),
                   (int)ce_piv_written(ce_card_id(c)),   /* 本卡往池里的写账：本批起应当恒 0 */
                   /* ★ 2026-09-27（第三十七批）：`bullet_hit_orbs` = 「人逢魔 + 自机弹命中」这条新来源
                    * 一共撒了几颗（一手 `th08:0x45187f`）。人侧击破那批的判据另看 `orbs`/`taken`。*/
                   s_bullet_hit_orbs, s_bullet_hit_pos_swap, s_graze_pos_from_bullet, s_fengma_eaten,
                   s_graze_n[1], s_graze_n[2], s_graze_n[3],
                   s_graze_gate[YK_GRAZE_GATE_OK], s_graze_gate[YK_GRAZE_GATE_BOSS],
                   s_graze_gate[YK_GRAZE_GATE_GAUGE], s_graze_gate[YK_GRAZE_GATE_SHOTTYPE],
                   s_graze_gate[YK_GRAZE_GATE_OFF], s_graze_clamped);
            s->score_seen = (int32_t)CE_SCORE();      /* ★ 第四十一批：本行台账的分数基线 */
        }
    if (!CE_PLAYER()) {
        yk_drop_all(s, "no player");
        return 0;
    }
    /* ★★★★ 2026-10-01 晚（owner：**调试功能**「空格 = 击杀所有使魔」）：
     *   · **边沿**检测（不是长按连发）：`CE_INPUT_HELD()`（`engine.h:122` = `*(u32*)0x4ca428`）与上一帧异或；
     *   · 命中 ⇒ **走正常击杀路径**（把壳的 `life.current` 置 0 ⇒ 引擎下一帧走 `Enemy__die` ⇒
     *     **死亡广播 ⇒ 我们的 `BROKEN set` ⇒ 站点否决**）—— 不是把实体直接删掉；
     *   · 覆盖**映射壳 + 绕转使魔**；日志每次一行；
     *   · ⚠ **调试功能**（`YK_DEBUG_SPACE_KILL`，默认 1；发布前可关）。
     *   ❓ 空格位 = `YK_DEBUG_KEY_MASK`（= `0x001`，TH 系 Z/射击位）**待一手核**：本块会打前 8 次
     *     输入掩码变化（`held=0x…`）⇒ 若按空格不出 `SPACE` 行，用那行掩码即可定出正确位。*/
    if (YK_DEBUG_SPACE_KILL) {
        /* ★★★★ 2026-10-01 晚（owner：「**一直以来的空格键都是调试**」）——改用 SDK 现成接口：
         *   **`(CE_DEBUG_SPACE_KEYS && ce_key_down(CE_VK_SPACE))`**（`sdk.h:832` / `sdk.c:397` = `GetKeyboardState`，
         *   **user32 原始键盘状态**，不是游戏输入掩码、**不进 replay 输入流**；只给**电平** ⇒
         *   **边沿自己记**（先例：`animal.c:3988` `(CE_DEBUG_SPACE_KEYS && ce_key_down(CE_VK_SPACE))`；`input_synth.c:162`）。
         *   为什么不用 `0x4ca428`：它的掩码里**没有空格位**（log 只见 `0x10/0x20/0x40/0x80`+`0x008`），
         *   且 `sdk.h:822` 说明那批键**不进 replay 输入流**。
         *   ⇒ 按下那一刻触发（长按不连发）；**只读电平、不吞键、不改键**。*/
        /* ★ 写后验证（下一帧）：`life_after` 又变非 0 / `alive=1` ⇒ 有人在复活它（我们的 pin/重建）。*/
        if (s_dbg_kill_n && ce_sdk_frame() != s_dbg_kill_frame) {
            unsigned q;
            for (q = 0; q < s_dbg_kill_n; ++q) {
                uint32_t kid = s_dbg_kill_ids[q];
                uint8_t *kf = yk_familiar_by_id(kid);
                ce_log("youkai/debug: SPACE VERIFY fam=%u life_after=%d alive=%d flags=0x%08x", kid,
                       kf ? (int)*(const int32_t *)(kf + YK_ED_LIFE_CUR) : -999,
                       kf ? 1 : 0, kf ? (unsigned)*(const uint32_t *)(kf + CE_ENEMY_FLAGS) : 0u);
            }
            s_dbg_kill_n = 0;
        }
        {   /* ★ 「自机真的挨了一下」观测（owner）：状态字 `player+0x476ac` 变化就打一行
             *   （1 = 正常；4 = 决死窗口 —— 一手 `engine/player/th18/03-player-hit-gate.md:177+`）。*/
            static int32_t s_pl_state_prev = -999;
            int32_t st = *(const int32_t *)(CE_PLAYER() + 0x476ac);
            if (st != s_pl_state_prev) {
                if (s_pl_state_prev != -999)
                    ce_log("youkai/debug: PLAYER HIT/STATE %d -> %d (4 = deathbomb window; invincible=%d)",
                           (int)s_pl_state_prev, (int)st, s_dbg_inv_on);
                s_pl_state_prev = st;
            }
        }
        int down = (CE_DEBUG_SPACE_KEYS && ce_key_down(CE_VK_SPACE)) ? 1 : 0;
        if (down && !s_dbg_space_prev) {
            unsigned m = 0, k = 0, j;
            /* ★★★★★ 2026-10-01 晚（owner：「我空格**直接带走道中 boss** 了」）——**只杀我们注册过的壳**：
             *   · **注册表** = `s_map.e[].fam_id`（映射壳）∪ `s_orbit[].fam_id`（绕转使魔），
             *     且必须**实体存在**；
             *   · **母机 id 一律不碰**：先把所有 `s_map.e[].enemy_id`（= 我们认过的母机）收进 `own_parents[]`，
             *     任何 id 命中它就 **skip**（这条专治「把母机当壳」）；
             *   · 顺带 skip：`fam_id == 所在槽的 enemy_id`（自指异常）、实体查不到、id 为 0。
             *   · 前 3 次按键打出 `ids/own/skipped` 三个清单 ⇒ 一眼看出谁被误杀。*/
            uint32_t own_parents[YK_FAM_MAX];
            uint32_t ids[YK_FAM_MAX + YK_ORBIT_MAX];
            uint32_t skipped[YK_FAM_MAX + YK_ORBIT_MAX];
            unsigned np = 0, ni = 0, ns = 0;
            for (j = 0; j < YK_FAM_MAX; ++j)
                if (s_map.e[j].enemy_id) own_parents[np++] = s_map.e[j].enemy_id;
            for (j = 0; j < YK_FAM_MAX; ++j) {
                uint32_t id = s_map.e[j].fam_id;
                uint8_t *f;
                int is_parent = 0, q;
                if (!id) continue;
                /* ★ 2026-10-11（多原点）：同点共壳会让**多条键引用同一只壳** ⇒ 去重，否则
                 *   `killed N` 会把一只壳数成 N 只（`life=0` 是幂等的，但台账/清单会骗人）。*/
                {   int dup = 0; unsigned q2;
                    for (q2 = 0; q2 < ni; ++q2) if (ids[q2] == id) { dup = 1; break; }
                    if (dup) continue;
                }
                if (id == s_map.e[j].enemy_id) { skipped[ns++] = id; continue; }
                for (q = 0; q < (int)np; ++q) if (own_parents[q] == id) { is_parent = 1; break; }
                if (is_parent) { skipped[ns++] = id; continue; }
                f = yk_familiar_by_id(id);
                if (!f) { skipped[ns++] = id; continue; }
                if (!yk_shell_parent_ok(f, s_map.e[j].enemy_id)) { skipped[ns++] = id; continue; }
                /* ★ 2026-10-01 晚（owner：41 次按键、41 次 killed 2，但壳**没死**）：
                 *   `life = 0` 之外还要**清掉会挡死的标志**（我们在壳上按侧写过的
                 *   `CE_ENEMY_FLAG_NO_DAMAGE`）—— 否则伤害段被跳过、死亡流程不进。*/
                *(uint32_t *)(f + CE_ENEMY_FLAGS) &= ~(uint32_t)CE_ENEMY_FLAG_NO_DAMAGE;
                *(int32_t *)(f + YK_ED_LIFE_CUR) = 0;
                ids[ni++] = id;
                ++m;
            }
            for (j = 0; j < YK_ORBIT_MAX; ++j) {
                uint32_t id = s_orbit[j].fam_id;
                uint8_t *f;
                int is_parent = 0, q;
                if (!id) continue;
                for (q = 0; q < (int)np; ++q) if (own_parents[q] == id) { is_parent = 1; break; }
                if (is_parent || id == s_orbit[j].parent_id) { skipped[ns++] = id; continue; }
                f = yk_familiar_by_id(id);
                if (!f) { skipped[ns++] = id; continue; }
                if (!yk_shell_parent_ok(f, s_orbit[j].parent_id)) { skipped[ns++] = id; continue; }
                *(uint32_t *)(f + CE_ENEMY_FLAGS) &= ~(uint32_t)CE_ENEMY_FLAG_NO_DAMAGE;
                *(int32_t *)(f + YK_ED_LIFE_CUR) = 0;
                ids[ni++] = id;
                ++k;
            }
            ce_log("youkai/debug: SPACE(ce_key_down) => killed %u familiars (mapping %u / orbit %u) - "
                   "normal death path (they die next frame => BROKEN set => the emitter slot is vetoed "
                   "until a wave/rearm edge or the TTL)", m + k, m, k);
            for (j = 0; j < ni && j < 8u; ++j) { s_dbg_kill_ids[j] = ids[j]; }
            s_dbg_kill_n = (ni < 8u) ? ni : 8u;
            s_dbg_kill_frame = ce_sdk_frame();
            if (s_dbg_ids_logged < 3u) {
                unsigned q;
                ++s_dbg_ids_logged;
                for (q = 0; q < ni; ++q)
                    ce_log("youkai/debug: SPACE KILL id=%u (registered shell)", ids[q]);
                for (q = 0; q < ns; ++q)
                    ce_log("youkai/debug: SPACE SKIP id=%u (parent/unregistered => NOT killed)", skipped[q]);
                for (q = 0; q < np; ++q)
                    ce_log("youkai/debug: SPACE PARENT id=%u (mothership; never touched)", own_parents[q]);
            }
        }
        s_dbg_space_prev = down;
    }
    if (YK_DEBUG_INVINCIBLE) {
        int d = ce_key_down(YK_DEBUG_INVINCIBLE_VK) ? 1 : 0;
        if (d && !s_dbg_inv_prev) {
            s_dbg_inv_on = !s_dbg_inv_on;
            s_dbg_inv_logged = 0;   /* 每次 ON 重新打前 16 帧 */
            ce_log("youkai/debug: INVINCIBLE %s (player+0x47774 cur>=%d per frame) - debug only, card 108",
                   s_dbg_inv_on ? "ON" : "OFF", (int)YK_DEBUG_INVINCIBLE_MIN);
        }
        s_dbg_inv_prev = d;
        if (s_dbg_inv_on) {
            ce_timer_t *iv = CE_PLAYER_INVULN();
            if (iv) {
                /* ★ 运行期验证（owner：ON 了照死）：**写后读回** —— 前 16 帧各一行，
                 *   看 `cur` 有没有真的保持 ≥60（被引擎每帧覆盖 ⇒ 时机太晚 ⇒ 要更早的钩子）。*/
                /* 一手（engine/player/th18/03-player-hit-gate.md:239）：判定侧读的是
                 * **player+0x47778（cur）> 0 即无敌**（zTimer{prev=+0x47774, cur=+0x47778, cur_f=+0x4777c}）。
                 * 为消除结构体布局疑问，这里**按绝对偏移直写三个格**。*/
                {   uint8_t *pp = CE_PLAYER();
                    if (*(const int32_t *)(pp + 0x47778) < YK_DEBUG_INVINCIBLE_MIN) *(int32_t *)(pp + 0x47778) = YK_DEBUG_INVINCIBLE_MIN;
                    if (*(const int32_t *)(pp + 0x47774) < YK_DEBUG_INVINCIBLE_MIN) *(int32_t *)(pp + 0x47774) = YK_DEBUG_INVINCIBLE_MIN;
                    *(float *)(pp + 0x4777c) = (float)YK_DEBUG_INVINCIBLE_MIN;
                }
                if (s_dbg_inv_logged < 16) {
                    ++s_dbg_inv_logged;
                    ce_log("youkai/debug: INVULN abs[0x47774]=%d abs[0x47778]=%d abs[0x4777c]=%.1f frame=%u "
                           "(gate reads 0x47778 > 0) min=%d",
                           (int)*(const int32_t *)(CE_PLAYER() + 0x47774),
                           (int)*(const int32_t *)(CE_PLAYER() + 0x47778),
                           (double)*(const float *)(CE_PLAYER() + 0x4777c), (unsigned)ce_sdk_frame(),
                           (int)YK_DEBUG_INVINCIBLE_MIN);
                }
            }
        }
    }
    /* ★ 排障用（无害）：前 24 次**游戏输入掩码**变化各打一行（`0x4ca428`；空格不在其中）。*/
    if (s_dbg_mask_logged < 24u) {
        uint32_t held = CE_INPUT_HELD();
        if (held != s_dbg_prev_input) {
            ++s_dbg_mask_logged;
            ce_log("youkai/debug: input mask changed held=0x%08x (prev=0x%08x) [0x4ca428; no space bit]",
                   (unsigned)held, (unsigned)s_dbg_prev_input);
            s_dbg_prev_input = held;
        }
    }
    yk_side_lock_tick();                                  /* ★ owner ④：锁形态（B / 灵界 / 咆哮）*/
    yk_bullet_hit_flush(s);                               /* ★ 修法 B：命中回调排队的刻符在这里落地 */
    /* ★ 2026-10-01（owner：「瞬移不会重建血量吧」）实跑判据行（每 120 帧、只在有活动时打）：
     *   `move` 涨 = 槽在换位置（瞬移或插值）；`create/withdraw/hp_reset` 不涨 = **没有重建、没有重置血量**。*/
    if ((s_ledger_frame % 120u) == 0u && (s_emit_move || s->spawns || s->lost || s->silent)) {
        {
            /* ★ 2026-10-11 静态核实：这里原来有个**丢弃返回值**的 `yk_emit_latch_get(0,0,lz)` 占位调用
             *   （只为了「打一行日志」）—— 已删（签名带模式后它是死代码；日志本就不需要它）。*/
            ce_log("youkai/emit: move %u create %u withdraw %u hp_reset %u latch_logs=%u dist0=%u phantom=%u bodyport=%u notfam=%u noorigin=%u badtype=%u nobullet=%u fire_seen=%u "
                   "(position-only pinning: offset changes never rebuild the shell nor reset its HP; "
                   "shell target = the engine's own fire-point formula, recomputed per frame; "
                   "AUDIT §BE #208/#210; static verify: HANDOFF-108-EMITTER-STATIC-VERIFY.md)",
                   s_emit_move, s->spawns, s->lost + s->silent, s_emit_hp_reset, s_emit_latch_logged,
                   s_emit_dist0, s_emit_phantom, s_emit_bodyport, s_emit_notfam, s_emit_noorigin, s_emit_badtype, s_emit_nobullet, s_fire_seen);
        }
    }
    focused = yk_focused();                               /* ★ owner ④：本卡眼中的侧（锁期间钉住）*/
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
    /* ★ 2026-10-03z7（审计 P1-1）：自愈 —— `claimed` 为假（纪元清表）或取不到槽时**补认领**，
     *   然后再取。⚠ 局中删卡的**保留位**在 SDK 里 `claimed` 仍为真 ⇒ 不会被误判（§4.4）。*/
    if (!s->hud_ready || !ce_hud_slot_claimed(ce_card_id(c)) ||
        !ce_hud_slot_get(ce_card_id(c), &s->hud)) {
        yk_hud_claim(c, s);
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
    if (ce_sweep_should_run(&s_sweep_seen)) yk_sweep_to_money();   /* ★ 92 清扫：刻符 → 金钱 */
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
            /* ★★★ 2026-09-27（§BE #136 / owner 实跑反馈「**妖逢魔现在擦弹似乎不掉永点**」）：
             *   ★ **本轮删掉了包在这里的那道 `if (focused)`** —— 它是「聚焦即撒」时代的遗留，
             *   **一手没有这道门**，而且它正好把 owner 报的那种情形整段吞掉：
             *
             *   · **一手（逐指令）**：擦弹奖励函数 `Player::sub_44a930`（`th08:0x44a930`）里那道
             *     `IsYoukai()` 闸（`th08:0x44aa6c call 0x40bc40` / `:0x44aa74 test eax,eax` /
             *     `:0x44aa76 je 0x44aa86`）**只包住 +100 那一笔**；人侧／妖侧**在 `th08:0x44aa86`
             *     会合**，而三处刻符生成（`th08:0x44aac9` / `:0x44aaef` / `:0x44ab0f`）全在会合点
             *     **之后** ⇒ **刻符与人/妖形态无关**。真正决定给不给的是三道门：
             *     `:0x44aa9e`（机体 · 本卡恒过）→ `:0x44aaac`（`is_boss_active`）→ `:0x44aaba`
             *     （`GaugeIsExtremelyYoukai` = 槽 **≥ +8000**）。
             *   · **社区交叉（独立第二源，スコアアタックまとめ wiki 的「刻符出現条件」）**：
             *     「妖怪逢魔が時に敵弾にかする（**ボス戦のみ**）」＋小ネタ「カスリ時に刻符が出るのは
             *     **妖怪逢魔が時であることが条件で、低速であることは関係ない**」
             *     ⇒ 与上面逐指令**逐字同义**：要**极妖 + 有 boss**，**不要求低速**。
             *   ⚠ 因此「低速」（本卡的 `focused`）在这一支里**不是**判据 —— 玩家的常规技巧
             *     「人妖カスリ」（在弹列里**瞬间切高速**擦、再切回低速收）正是靠这一条成立的。
             *   ⇒ 判据全在 `yk_graze_field_gate()` / `yk_graze_orbs_per_bullet()`（纯逻辑 + 单测），
             *     卡侧这一支**不再自己加任何形态门**。*/
            /* ★★★ 2026-09-27（第三十九批 · owner 实跑：「**妖逢魔即使在 boss 也不生成永点——或者
             *   生成永点瞬间吃了？**」）：**落点从「自机」改成「被擦到的那颗弹」**。
             *   · 真因（本轮定位）：我们原来撒在 `(px, py)` = **自机身上** ⇒ 距离 0 < 抓取盒 20/24
             *     ⇒ 而 state 5 **上抛期间就能被收** ⇒ **当帧就被吃掉**、屏幕上什么都看不见。
             *   · 一手正好相反：擦弹刻符撒在**被擦到的那颗弹**的位置上（`Player::sub_44a930`
             *     `th08:0x44a930` 的第 1 个实参 = pos，由调用方传弹的坐标）—— 那颗弹在**擦弹环**
             *     上（离自机 20–30 px），撒在那里就**看得见**（上抛一段、`vy > 0` 才转归航）。
             *   · 拿法：SDK 的 `ce_graze_take(&gx, &gy)`（站点 `ce_graze` `0x42497f`，本轮**放回发布面**；
             *     机制与放回论证见 `sites.py` 那一段）。取不到（无站点/本帧没记到）时退回自机位置。
             *   · 弹/激光那条 `flag` 分不出来 ⇒ 用「本帧擦弹差分」当颗数（既有口径，未变）。*/
            {
                float gx = px, gy = py;
                /* ⚠ 用 **peek**（只读）而不是 `take`：116（季节卡）在同一帧也要读这个事件，
                 *   `take` 会把待处理数据清掉 ⇒ 谁先调谁偷走。见 `sdk.h` 的 `ce_graze_peek` 注释。*/
                if (ce_graze_peek(&gx, &gy) > 0) ++s_graze_pos_from_bullet;
                for (i = 0; i < d; ++i)
                    for (k = 0; k < orbs; ++k)
                        yk_orb_spawn_ex(s, gx, gy, YK_ORB_STATE_SPAWN5, 0);  /* ★ 擦弹也**可见吸收** */
            }
            /* ★ 实跑判据（统筹要的那一条）：`gate=` 一眼看出**这次擦弹为什么没给**
             * （`ok` 全过 / `boss` 场上没 boss / `gauge` 槽没到 +8000 / `shottype` 人単独非妖夢 /
             * `off` 开关关掉），`n=` 是**过了门之后每颗弹给几颗**（0 = 被门挡）。
             * ★ 2026-09-27（§BE #136）：这两本账**不再只在聚焦时记** —— 擦弹事件的账要能覆盖
             *   「高速擦弹」那一路（否则 owner 报的那种情形日志里一行都没有，判不出来）。
             * 限频：`gate` 或 `n` 一变就打一行；都不变时最多每 60 帧一行（免得密擦时刷屏）。*/
            if (orbs <= YK_GRAZE_ORBS_MAX) ++s_graze_n[orbs];
            if (gate >= 0 && gate <= 4) ++s_graze_gate[gate];
            if (gate != s_graze_log_gate || orbs != (unsigned)s_graze_log_n
                || (uint32_t)(s_ledger_frame - s_graze_log_frame) >= 60u) {
                s_graze_log_gate = gate;
                s_graze_log_n = (int)orbs;
                s_graze_log_frame = s_ledger_frame;
                ce_log("youkai: graze orbs n=%u x%u bullet(s) gate=%s (spell=%d shottype=%d gauge=%d "
                       "boss=%d focus=%d) tally 1/2/3=%u/%u/%u clamped=%u - TH08 th08:0x44aac9+aaef+ab0f",
                       orbs, d, yk_graze_gate_name(gate), spell, st, (int)s->gauge,
                       boss, focused, s_graze_n[1], s_graze_n[2], s_graze_n[3],
                       s_graze_clamped);
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
                if (YK_ENABLE_FENGMA_SCORE) CE_SCORE() += YK_FENGMA_SCORE_PER_FRAME;  /* ⑪ */
                s->fengma_score += YK_FENGMA_SCORE_PER_FRAME;
            }
        }
    }
    /* ★ #26 风味选项：决死窗口里全屏敌弹变红（判据 = 既有原语 `player+0x476ac == 4`）。*/
    (void)yk_deathbomb_tint();
    yk_gauge_tick(s, focused);
    if (YK_ENABLE_SCORE_DECL) ce_score_family_declare(ce_card_id(c), CE_COEFF_FAMILY_08);  /* ⑩ */
    /* ★★★ 2026-09-27（§BE #122，owner：「人妖槽的那个 100000 东西要**按照额外最大得点的方法 plot**，
     *   也就是说**初始得 plot 000000**」＋「我们**有一系列的额外最大得点公式**」）：
     *   **声明给计分层的必须是「额外」量** —— `sdk_core.h` 的池一节写着用户逐字
     *   「初值是 0，因为只有**加进去的**才是『额外』最大得点」⇒ 这里从「整个夜点」改成
     *   **`piv − piv_base`**（难度初值那一格是基准、不进池）⇒ 池初值 = 0 ✓
     *   （显示侧同步改成读池：见 `yk_night_text_sync()`）。*/
    if (YK_ENABLE_SCORE_DECL) ce_score_declare_delta(ce_card_id(c), s->piv - s->piv_base);  /* ⑩ */
    /* ⚠ 历史（§BE #137，2026-09-27 第三十六批）：这里曾经加过一支「本卡往共享 PIV 池里写」的代码，
     *   好让屏上那个数（当时画的是池）跟着永点涨。**第四十批已整体撤掉** —— owner 改了口径
     *   （「这个值应该**只考虑 th08** 所导致的额外最大得点」），而且那一支在计分里与本卡的
     *   `ΔG_08` 声明**重复计了一遍**（池 = `ce_piv_family_points()` ⇒ `ce_score_total()` ×5/6）。*/
    /* ★★★ 2026-09-27（第四十批 · **owner 口径变更**：「这个值应该**只考虑 th08 所导致的**额外最大得点」）：
     *   **本卡不再往共享 PIV 池里写**（第三十六批加的那一支整体撤掉），原因两条：
     *     · **显示**：屏上那个数改成画**本卡自己的额外量** `piv − piv_base`（= TH08 的时点带来的量），
     *       不再画共享池 ⇒ 与 110/111/112–117 的贡献解耦（owner 就是要求这个）；
     *     · **计分**：池是 `ce_piv_family_points()` 的族项、进 `ce_score_total()`（`×5/6`），而本卡的
     *       PIV 早就按 §BE #43 的口径以 **`ΔG_08`** 声明进计分层了 ⇒ 两边都写 = **同一笔 PIV 在
     *       计分里算两遍**（本轮发现并撤掉；一手口径见 `youkai_core.h` 的 `YK_PIV_POOL_CAP` 那一节）。
     *   ⇒ 池里从此**只有** th12–th17 族与擦弹那一项 —— 那也正是它原本的语义。*/
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
    /* ★ 2026-10-01（工单③ 第 3 条核对）：本调用**值恒 0**（= 本卡从不声明「原生子机不许出现」），
     *   而声明层是**粘性**的（`sdk_core.c:645`：写一次就一直有效）⇒ 人侧再喊一遍**没有任何行为**。
     *   为「人侧少碰引擎层」起见，现在只在**妖侧**（或二分版）喊；人侧不喊不可能改变任何东西。*/
    if (YK_HUMAN_OPT_WRITE || focused)
        ce_native_option_hide_declare((uint32_t)ce_card_id(c), 0, yk_current_character());
    /* ★ 修法第 3 步（2026-09-23）：**隐藏但请求保留记录** —— 这样即使 111（魔理沙侧）同时声明了隐藏，
     *   构建断点也不会把 `lvl` 压 0 ⇒ 那 4 格的记录/坐标照建，行循环有活膛口（我们的 `slot >= 1` 行）。
     *   显示仍由下面那支 keep_state sweep 每帧收掉 ⇒ 111 的「原生不可见」不受影响。*/
    /* ★ 2026-10-01（工单③ 第 3 条核对）：这一支**人侧必须继续调、且必须传 0** ——
     *   声明层同样是**粘性**的 ⇒ 人侧不调 = 妖侧那次「保留记录」一直挂着（会削弱 111 那侧的隐藏语义）。
     *   ⇒ 只有二分版把它压成 0（人侧完全不申请）。*/
    ce_native_option_keep_records((uint32_t)ce_card_id(c),
                                  (YK_HUMAN_OPT_WRITE ? focused : 0));
    if (focused) {
        int n = ce_native_option_sweep_keep_state();
        /* ★★★ 2026-10-01（owner 定性「就是 th08 的问题」+ 工单②的假设 1，**收紧判据 + 加诊断**）：
         *   **收紧**：只在「妖侧 **且** 我们真的认领了妖侧 shooterset」时才收原生 `mode=2` 激光。
         *   为什么：`ce_kill_retail_option_lasers()` 打的是 **packed set ≤ 0x15 = 原生/零售那一族**
         *   （`sdk.c:639`；魔理沙的「ミニ八卦炉」人侧束 = **set 0x0b**）—— 它是为「我们占了妖侧 set、
         *   原生子机被藏、可它们的束寿命只跟 Z 走会钉在原地」这一件事服务的。认领不了的时候
         *   （早苗档 0 等表里 0 台子机的档）原生行**照发**，那些束**不是**我们要替换的东西
         *   ⇒ 杀了就是我们**主动打断自机激光**（形状 = 断层 + 命中蓝闪断续）。
         *   判据与 `on_shot_set_index` / `yk_should_suppress_native` **同表同帧**（`yk_option_wanted`）。
         *   **诊断**（owner 要的判据）：妖侧这一支打上下文行；人侧另打一行「NOT called」⇒
         *   人形态打一次就能证明 108 有没有碰自机激光。各自只打前几次（不刷屏）。*/
        if (s && s->laser_probe < 6) {
            ++s->laser_probe;
            ce_log("youkai/laser: kill_retail context focused=%d real_focused=%d claimed_set=0x%02x "
                   "want=%d (human side must print focused=0 and never call the kill)",
                   focused, yk_real_focused(), (unsigned)s->claimed_set,
                   yk_option_wanted(1, yk_current_character(), yk_current_power_level()));
        }
        if (yk_option_wanted(1, yk_current_character(), yk_current_power_level())) {
            int kn = ce_kill_retail_option_lasers();
            if (kn > 0 && s && s->laser_probe < 16) {
                ++s->laser_probe;
                ce_log("youkai/laser: kill_retail KILLED %d native mode=2 laser(s) while we own the "
                       "youkai shooterset (these are the hidden human-side option beams, set <= 0x15)",
                       kn);
            }
        }
        if (n && s && !s->logged_natopt) {
            s->logged_natopt = 1;
            ce_log("youkai: native (human-side) options hidden in low speed - swept %d stale VM(s); "
                   "they come back the frame Shift is released (declaration is per-character)", n);
        }
    } else if (s && s->laser_probe_human < 4) {
        ++s->laser_probe_human;
        ce_log("youkai/laser: kill_retail NOT called on the human side (real_focused=%d) - 108 does not "
               "touch native option lasers here (owner's laser report: this line is the proof)",
               yk_real_focused());
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
    /* ★ 2026-10-01（工单③ 二分开关）：`YK_HUMAN_OPT_WRITE == 0` ⇒ 人侧**退出声明**（不参选、不落笔）。*/
    if (!YK_ENABLE_OPTION_POSE || !YK_ENABLE_POSE_DECLARE) ce_option_pose_declare(c, NULL);  /* ★ ③/⑫ */
    else if (YK_HUMAN_OPT_WRITE || focused) yk_option_refresh(c, s, focused);
    else ce_option_pose_declare(c, NULL);
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
        /* ★★★ 2026-09-27（第四十二批 · **日志洪水 + 白干活**）：这一行原来无条件跑 —— 它坐在
         *   **高速（人侧）的每帧**路径里 ⇒ `s_natopt_probe_done` 每帧被清 0 ⇒ 上面那个
         *   「native option slots at HIGH speed」探针**每帧**都重新探、并打 **5 行**日志
         *   （实测 owner 那一局：几分钟 1.5 MB、日志里 90% 是它；`ce_log` 每行都 open/write/close）。
         *   原意是「**每进一次妖侧**打一行（**离开时**复位）」⇒ 改成只在**妖→人的边沿**复位。*/
        {
            static uint8_t s_natopt_focus_prev;
            if (s_natopt_focus_prev && !focused) s_natopt_probe_done = 0;   /* 只在这一帧复位 */
            s_natopt_focus_prev = (uint8_t)(focused ? 1 : 0);
        }
    }
    yk_familiars_tick(s);
    /* ★★★ 2026-09-28（第四十九批 · owner：「给这些大怪加上使魔」）：**惰性绕转使魔**每帧一次 ——
     *   扫场（每 `YK_ORBIT_SCAN_INTERVAL` 帧）+ 生命周期 + 相位/钉位/跟随 VM。
     *   ⚠ 刻意**不放在 `focused` 分支里**：它们不是「妖侧才有的东西」，人侧也活着、也绕转
     *   （与 1:1 映射使魔同一口径：松 Shift 不撤，只有 `yk_drop_all` 撤）。*/
    yk_orbit_tick(s_ledger_frame);
    /* ★ 早苗妖侧弹色（owner 口径）：默认秋黄、逢魔红 —— 每帧刷一次（只动 RGB）。*/
    yk_aya_bullet_color_tick(s);
    /* ★ 死亡点吸收区：每帧核寿命 + 吃进区的敌弹（无活区时零开销）。*/
    yk_zone_tick(s, ce_card_id(c), s_ledger_frame);
    /* ★★ 2026-09-26（owner：那声切换音「**在有使魔的情况下才有**」）：人→妖那一下的 `se_ophide`
     *   **延到这里才播** —— 门要读**本帧**使魔表（`yk_familiars_tick()` 刚在上面建/撤完）。
     *   一手语义也是这个：那一声是**敌人的方法**（`FUN_0042c420`）在「形态变了」时发的，
     *   所以只有当场真的有那些使魔实体时才听得见（详见上面 SE 那一段的长注释）。*/
    if (s->se_fam_press) {
        /* ★★★ 2026-09-30（owner：「**进妖侧有敌方使魔时候**的音效缺失了」）—— 原来这里只数
         *   **映射（发射口）**使魔 ⇒ 场上只有**绕转**使魔时 `fam_now == 0`，SE 不播（owner 看到的
         *   正是 4/6 面那种「大怪 + 绕转使魔」的场面）。一手语义：这一声是**使魔实体自己**发的
         *   （`FUN_0042c420`，见 ctx 段的长注释）⇒ **绕转使魔同样是「敌方使魔」**，必须一起数。*/
        const int fam_now = (int)yk_map_fam_live_count(&s_map) + (int)yk_orbit_live();
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
            /* ★ 2026-09-27（第三十九批）：发额度这一行 —— owner 报「螺旋并没有生成」时，
             *   有这一行 = 触发对了（问题在喷点或观感）、没有这一行 = **符卡结束的下降沿没抓到**。*/
            ce_log("youkai: spiral ARMED (spell ended) - quota %u, 8-frame batches around the boss",
                   (unsigned)YK_SPIRAL_QUOTA_FULL);
            /* ★★★ 2026-09-30（owner 口径）：**boss 的发射口使魔按每段攻击重建**（相位分界 = 就是这条
             *   符卡结束的下降沿，不新造判据）⇒ 收掉该 boss 的映射使魔，让映射逻辑在下一波重建。*/
            yk_boss_fam_rebuild(s);
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
            ce_log("youkai: emitter gate released %u slot(s) on the WAVE edge (enmKillAll broadcast, "
                   "total %u) - new wave: clamp flags cleared, the 1:1 mapping rebuilds those slots",
                   rel - s->releases_seen, rel);
            ce_log("youkai/emit: slot %d of enemy_id %u released by WAVE-EDGE(enmKillAll) after "
                   "vetoing %u etOn(s) (owner's main feature: kill the familiar => the emitter is "
                   "closed until a wave/phase edge or the %d-frame TTL)",
                   (int)yk_map_last_release_slot(&s_map), yk_map_last_release_enemy(&s_map),
                   (unsigned)yk_map_last_release_vetoed(&s_map), (int)YK_EMIT_REBUILD_TTL);
            s->releases_seen = rel;
        }
    }
    /* ★★★ 2026-10-01（owner 实跑 ②「没有识别到波次、下一波没重建」）：**`etOn` 间隔**那条波次边沿
     *   的记账（与上面 `enmKillAll` 那条**分开**）—— 涨了 = 有一波是靠「两次 `etOn` 间隔 ≥
     *   `YK_EMIT_WAVE_GAP` 帧」认出来的（一手 `MBoss1`：波内 12 帧 / 波间 ≥60 帧）。
     *   ⚠ 它涨得太频 ⇒ 阈值相对该 ECL 的波内间隔偏小（表现 = 使魔比预期早重建），调大它
     *   （`TUNING.md` §2.1ccc-33）。*/
    {
        uint32_t gap = yk_map_gap_releases(&s_map);
        if (gap != s->gap_releases_seen) {
            ce_log("youkai: emitter gate released %u slot(s) on an etOn GAP (adaptive: >= %d frames "
                   "AND >= %dx this slot's own in-wave gap; total %u) - those slots fire again and "
                   "rebuild next tick", gap - s->gap_releases_seen, (int)YK_EMIT_WAVE_GAP,
                   (int)YK_EMIT_WAVE_RATIO, gap);
            ce_log("youkai/emit: slot %d of enemy_id %u released by WAVE-GAP(adaptive) after vetoing "
                   "%u etOn(s)", (int)yk_map_last_release_slot(&s_map),
                   yk_map_last_release_enemy(&s_map), (unsigned)yk_map_last_release_vetoed(&s_map));
            s->gap_releases_seen = gap;
        }
    }
    /* ★★★ 2026-10-01（P1，owner：「终符卡持续发弹、高速把发弹口全清掉 ⇒ 终符变成奶奶来也能过」）：
     *   **长期自动重建**记账 —— 夹持超过 `YK_EMIT_REBUILD_TTL`（600 帧 ≈ 10 s）的槽会被无条件放开 +
     *   重建使魔。判据：这一行在涨 = 「有口被堵到 TTL 才放开」（owner 那个场景正在被兜住）；
     *   它**不涨**而 `CLAMPED` 在涨 = 那些口在主复位口（重新武装/波次/相位）之前就恢复了。*/
    {
        uint32_t heals = yk_map_heals(&s_map);
        if (heals != s->heals_seen) {
            ce_log("youkai: emitter gate REBUILT %u slot(s) after the %d-frame rebuild TTL "
                   "(P1 auto-rebuild, owner 2026-10-01; total %u) - those emitters fire again",
                   heals - s->heals_seen, (int)YK_EMIT_REBUILD_TTL, heals);
            s->heals_seen = heals;
                    ce_log("youkai/emit: slot %d of enemy_id %u released by TTL(%d frames) after vetoing "
                   "%u etOn(s)", (int)yk_map_last_release_slot(&s_map),
                   yk_map_last_release_enemy(&s_map), (int)YK_EMIT_REBUILD_TTL,
                   (unsigned)yk_map_last_release_vetoed(&s_map));
}
    }
    return 0;
}

CE_CARD(108, .ctor = on_ctor, .on_load = on_load, .on_power_level_change = on_power_level_change,
             .on_tick_shooters = on_tick_shooters, .on_tick_2 = on_tick_2,
             .on_bullet_created = on_bullet_created, .on_enemy_death = on_enemy_death,
             /* ★ 2026-09-27（第三十七批）：**人逢魔 + 一发自机弹命中敌机 ⇒ 在弹的位置掉 1 颗**
              *   （一手 `th08:0x451860`–`:0x451892`；站点 = `ce_bullet_hit` `0x45f466`）。*/
             .on_bullet_hit = on_bullet_hit,
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
/* ★ 2026-09-30（owner：使魔两侧都常驻**加法空心环**）后记录 = 20 B：环句柄 `circle_id` 之外多了
 * 「上一次发的中断对应的侧」`circle_side`（1 字节，补齐到 4 字节对齐）—— 表 = 32×20 B = 640 B。*/
_Static_assert(sizeof(yk_fam_vm_t) == 20, "familiar follow-VM record changed size");

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
