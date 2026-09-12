/* youkai_core.h —— 108「永夜抄」妖机体 / 使魔的**纯逻辑**层。
 *
 * 只 <stdint.h>，不碰引擎、不进 DLL 的断点路径 ⇒ 主机 `make test-host` 口径直接测
 * （tests/test_youkai_core.c）。引擎侧在 youkai.c。
 *
 * 设计唯一真源：`docs/superpowers/specs/2026-09-12-youkai-108-design.md`。
 * 一手：`local/_scratch/research-108-shooter-tick.md`（止射闸）、
 *       `local/_scratch/research-108-damage-transfer.md`（伤害转交）、
 *       `local/_scratch/research-death-signal-survey.md`（敌人移除点全集）。
 * 版本：TH18 v1.00a（裸地址默认属该版本）。可信度：✅ 一手 · 🟡 待实跑 · ★ 占位/TODO。
 *
 * 本文件按 spec 的小节组织：§3.1 止射闸 / §3.2 本体贴图 / §3.3 子机 / §3.4 弹幕 /
 * §3.5 发射口→使魔（1:1 映射与状态差分）/ §3.6 击破信号 / §3.7 伤害转交 1/2 / §4 数值。
 *
 * ★ 所有「需要资产 / 需要用户决策」的点都是**具名常量 + TODO**，一个数值都不自己发明 ——
 *   缺资产的那几项一律取「**不做**」这个语义（占位值 = 不覆盖 / 不抢写 / 不持有），
 *   这样机制是完整的、单测能覆盖，而实机上不会替用户拍板。清单见 youkai.c 文件尾。
 */
#pragma once
#include <stdint.h>

/* ==================================================================== §3.1 止射闸
 * 判据（spec §3.1）：108 active && 聚焦（`player+0x476cc != 0`）。
 * 「卡 active」由 SDK 保证（这条询问只对**挂在卡链表上**的行为调，见 sdk.c `BP_ce_shot_set_index`）——
 * 被动卡一旦拥有就一直在链上，所以判据只剩「有自机 + 聚焦」。
 * ⚠ 与 117 咆哮的次序由 SDK 定：**先问 set index，有卡覆盖了（咆哮）就不抑制**（已实装在 sdk.c）。
 * 返回非 0 = 请求 SDK 本帧跳过原生行循环（`retaddr = 0x45eca0`，卡广播照跑）。 */
int yk_suppress_native_shot(int player_present, int focused);

/* ================================================================= §3.2 本体贴图
 * 本体 VM = `player+0x14`，换贴图 = 写 `vm+0x24`（sprite id）；脚本每帧写 sprite
 * ⇒ 必须 `on_draw`（帧末）每帧抢写。返回 **< 0 = 本帧不抢写**。
 *   高速 = 原机体 ⇒ 恒 -1（这是**设计**，不是占位：spec §0「高速 = 原机体」）。
 *   低速 = 妖侧（早苗 → 射命丸文，TH16 `pl03.anm`）⇒ ★ TODO(资产)：见下方常量。 */
#define YK_BODY_SPRITE_UNFOCUSED (-1)   /* 高速：原机体（设计，永久）*/
#define YK_BODY_SPRITE_FOCUSED   (-1)   /* ★ TODO(资产 §5)：妖侧本体 sprite 号。资产未接进 pl0X.anm ⇒ 不抢写 */
int yk_body_sprite(int focused);

/* ==================================================================== §3.3 子机
 * 换位（低速阵型）是**原生双偏移** `+0x64/+0x68` vs `+0x6c/+0x70`，引擎自己做，卡侧零成本。
 * 换图 / 数量走 **SDK 的低速子机声明层**（spec §3.3.1，`ce_optpose_declare` /
 * `ce_optpose_winner` / `ce_optpose_pose`，sdk_core.h）：各卡只**声明**，层选举 **id 最小者落笔**
 * ⇒ **108(108) < 111(111)** ⇒ 108 胜、且只有它写字段（没有「谁后写谁赢」的竞态）。
 * 本卡只回答两个问题：**低速声明什么**、**高速退出声明**（用户定案 5「高速互不影响」= 高速时不占位，
 * 111 在高速自然不受 108 影响）。 */
#define YK_OPTION_COUNT_FOCUSED   0   /* ★ TODO(资产 §5 / 决策 §3.3)：占位 0 = **本帧不参与声明**。
                                       *   两个原因都是「不能自己发明」：
                                       *   ① 妖侧子机贴图（`ability.anm` 里的脚本）还不存在 —— 申请了也只能
                                       *      借豌豆射手的图（`anm_ids.h` 没有 youkai 条目）；
                                       *   ② **数量**：spec §4 没给。TH08 妖侧是「两只使魔」，但 spec 的
                                       *      `ce_option_pose_t` 只有**一套**聚焦/非聚焦偏移 ⇒ 「左右对称两只」
                                       *      那种形态表达不出来 ⇒ 要么 count=1（引擎的原生双偏移管左右），
                                       *      要么等 §10 决策。翻成 > 0 时本卡才会进选举、才会真去申请子机
                                       *      （落笔逻辑已经在 youkai.c 里写好）。*/
#define YK_OPTION_COUNT_UNFOCUSED 0   /* 高速：不声明（一格槽都不占）*/
int yk_option_count(int focused);
int yk_option_wanted(int focused);                    /* = count > 0：这张卡本帧参不参与选举 */
/* 声明的脚本和手里那个子机的脚本不一样 ⇒ 必须重申请（脚本是申请时定死的）。
 * `have < 0` = 手里还没有子机 ⇒ 不算「变了」。 */
int yk_option_script_changed(int want, int have);

/* =================================================================== §3.4 弹幕
 * 形态一档固定（用户 2026-09-12 拍板；★ 相对 TH08 是**主动简化** ⇒ ❓ 自定）、
 * **数值随 P 变**（这半条与原作一致）：`on_bullet_created` 里按 `bullet+0xac` 的打包 set id
 * 认出妖侧弹 ⇒ 改 `bullet+0x9c`。
 *   档 = `CURRENT_POWER / POWER_PER_LEVEL(100)` 钳到 [0,4]（TH18 五档；TH08 是六档阈值
 *   `8/24/48/80/128/999`，人妖两表共用 —— 映射到 TH18 五档是**本卡的决定**，见报告）。
 *
 * ★ 2026-09-12 一手补齐：TH08 妖侧每档伤害**已经抽出来了**（`rec[+0x1c]`，读点
 *   `th08:0x451805`；来源 `local/th08.v1.00d/dat/ply03as.sht`，解析脚本见报告）。
 *   但**不能照抄进下面那张表**：TH08 的「每发伤害」是配着「路数」一起设计的 ——
 *   满 P 档 9 发里单发 22–30，最低档 3 发里单发 36–48，**单发伤害随 P 下降**、靠路数变多
 *   把总 DPS 顶上去。我们的形态是**一档固定**（见下），把 TH08 的单发值搬进五档表
 *   会让「P 越高越弱」。⇒ 五档保持 `YK_SHOT_DMG_NOT_SET`（不覆盖），
 *   **这是决定不是占位遗漏**：要不要另设计一条随 P 上升的曲线由用户定（报告 §4 ❓）。*/
#define YK_SHOT_TIERS            5      /* TH18 火力档 0..4（CURRENT_POWER 0..400 / 100）*/
#define YK_SHOT_POWER_PER_LEVEL  100    /* = `CE_ADDR_POWER_PER_LEVEL`（0x4ccd40）*/
#define YK_SHOT_DMG_NOT_SET      0      /* 档位表里这个值 = 「不覆盖 `bullet+0x9c`」*/
/* 妖侧 shooterset 槽 = **覆盖**零售那格空槽（不是追加 —— 偏移数组 40 格已经全满）。
 * 一手：四份零售 `pl0X.sht` 的 `+0x02` 都是 40、`0x00`–`0x16` 逐项非 0、`0x17`–`0x27` 是本 mod
 * 其余四张卡（`native/sht_ids.h`）⇒ 没有「新索引」可加；而 `0x16` 是零售**唯一没有卡映射**的一格
 * （扫全 exe，调 `Player__tick_shooters_for_ability_card` `0x40a9c0` 的 11 处压栈立即数 =
 * `{0x0a,0x0b,0x0c/0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15}`，没有 `0x16`；
 * 主炮 `0x45ea00` 的公式只给 `0x00`–`0x09`）。
 * 分配／写入它的是 `assets/sht/append_shooterset.py` 的 `REPLACE` 表；那份脚本会回头核对
 * 本文件里的这两个常量，对不上就让 `make sht` 失败（防两边静默漂移）。
 * 内容 = TH08 v1.00d `ply03as.sht`（妖夢＆幽々子 的**妖侧**）第 5 档（`thr = 128` = 满 P）
 * 的 9 行逐字段翻译，`fire_rate = 3/5/10/6` ⇒ **除零风险随覆盖一起消失**（零售那格是 0，
 * 而 `0x40a9c0` 的 `idiv` 在 `0x40aa4f` 没有零分支）。逐字段出处见该脚本的 `REPLACE` 注释。*/
#define YK_SHOT_SET_YOUKAI       0x16
#define YK_SHOT_SET_YOUKAI_ROWS  9      /* 那组有几行（行号进 `packed_id` 低 8 位；认出妖侧弹只看 set 字节）*/
int     yk_shot_tier(int power);
int32_t yk_shot_tier_damage(int tier);
/* 打包 id = `(set << 8) | 组内行号`（`0x45ec69` 打包 / `0x45ee10` 回读）。 */
int     yk_bullet_is_youkai(uint32_t packed_id);
/* 1 = 要覆盖；*out = 覆盖后的值（档位表是占位 0 时恒返回 0）。 */
int     yk_bullet_damage_apply(int32_t current, uint32_t packed_id, int power, int32_t *out);
/* 妖侧 shooterset 能不能用：记录首字节 = `fire_rate`（i8）。`0` ⇒ 资产没装（会除零）、`< 0` ⇒ 终止符。 */
int     yk_shotset_usable(int fire_rate);

/* ============================================================ §3.5 发射口 → 使魔
 * 1:1 映射：该敌当前 `props[i].type(+0x00, u16) != 0` 的发射口槽数（实测以 1 为主、boss 最多 4）。
 * 每帧**状态差分**：活跃且无使魔 → 建；不活跃且有使魔 → 撤；本帧没扫到（敌人没了）→ 撤 + 退休。
 * 地图键 = `(enemy_id, 发射口槽)`（**不是指针**：敌人是堆对象，`allocate_new_enemy` = `new`）。 */
#define YK_EMIT_SLOTS     16     /* `zEnemyData.bullet_props[0x10]`（IDEAS.md §1.3）*/
#define YK_FAM_MAX        32     /* 工程护栏（**不是**游戏性数值）。spec §4 没给上限 ⇒ TODO(决策)。
                                  * 32 条 × **28 B** = 896 B 的**文件级静态**表（不进卡的 512 B 私有状态预算，
                                  * 照 117 动物灵 `s_tok[40]` 的做法；单测断言 ≤ 1024 B）；每只使魔 = 一次
                                  * 0x683c 字节堆 `new` + 一个敌人 tick ⇒ 表满时**放弃这一只**（不崩、不排队）。*/
#define YK_LIFE_LIMIT     1000   /* ★ 硬约束（spec §3.5 / §6.1）：spawn 参数 `+0x14` 必须 < 这个数，
                                  * 否则 `flags_lo` 位 30 被置 ⇒ 进锁定黑名单 ⇒ 诱导打不到使魔。*/
#define YK_LIFE_MAX_ALLOWED 999
#define YK_LIFE_UNSAMPLED INT32_MIN   /* `prev_life` 未采样（第一次看到这只使魔）*/
/* ★ TODO(数值设计 §10⑥)：使魔 HP **待用户定**（spec §4 表里是 ⏳）。占位 = 1：
 *   「一发即破」—— 它**不是**数值设计结果，只满足两条：① 必须 < `YK_LIFE_LIMIT`（硬约束，上面那条）；
 *   ② 让「使魔可打 → 击破 → 关那个槽 → 1/2 转嫁」这条链在实跑里**一发就能验完**。
 *   定案后改这一行即可（`yk_clamp_spawn_life` 会拦住 >= 1000 的误填）。*/
#define YK_FAM_LIFE       1

/* 槽位 → 使魔皮肤**变体**（0..3）。卡侧把变体映射到 `anm_ids.h` 的脚本宏（**不写死数字**）：
 * `CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53 + 变体`。
 * 语义（用户 2026-09-12 定案，spec §3.5）：**使魔数量 = 该敌当前活跃发射口槽数（1–4）
 * ⇒ 四色正好一槽一套，槽 i 用 `s53+i` 那一套**。四色零售切图 = TH08 `enemy.anm` script53..56
 * （红 / 绿 / 蓝白 / 黄），已切进 `assets/ability/scripts/206_youkai_familiar_s53.anm.txt` … `209_…_s56`。
 * 槽域是 0..`YK_EMIT_SLOTS`-1 而四色只有 4 套 ⇒ 槽 >= 4 取 `slot & 3`
 * （❓ **自定兜底**：规格说实测最多 4 个活跃槽，这条理论上走不到；不是一个「第 5 种颜色」的承诺）。*/
int     yk_fam_variant(int slot);

/* 发射点世界坐标（`etOn` 的算法，IDEAS.md §1.3）：`origins[i].z > 0.9` = 绝对模式。 */
#define YK_ORIGIN_ABS_THRESHOLD 0.9f
void yk_emit_world_pos(const float enemy_xy[2], const float off3[3], const float org3[3], float out_xy[2]);
/* 活跃判据：`props[i].type != 0`（`etNew` 清零、只有 `etSprite` 写它，spec §6.3）。 */
int  yk_emit_active(uint16_t prop_type);

enum { YK_ACT_NONE = 0, YK_ACT_CREATE = 1, YK_ACT_WITHDRAW = 2 };

typedef struct {
    int      kind;       /* YK_ACT_* */
    uint32_t enemy_id;   /* 发射口所属敌人（建的时候要它的指针 / 撤的时候只是记账）*/
    uint32_t fam_id;     /* 使魔的 `enemy_id`（WITHDRAW 时有意义；每帧用 id 回查，不缓存指针）*/
    int      slot;       /* 发射口槽 0..15 */
    float    x, y;       /* 发射点世界坐标（本帧）*/
} yk_action_t;

typedef struct {
    uint32_t enemy_id;   /* 0 = 空条目（`enemy_id` 单调递增、不复用，见 engine.h `CE_ENEMY_ID`）*/
    uint32_t fam_id;     /* 0 = 这个槽没有活着的使魔 */
    int32_t  prev_life;  /* 使魔 `life.current` 的上一帧值（YK_LIFE_UNSAMPLED = 还没采过）*/
    float    x, y;       /* 发射点世界坐标（每帧重算，卡侧按它钉使魔位置）*/
    uint8_t  slot;
    uint8_t  seen;       /* 本帧扫到过这个 (敌人, 槽)*/
    uint8_t  active;     /* 发射口本帧活跃（`props[i].type != 0`）*/
    uint8_t  broken;     /* 使魔被打死 ⇒ 该槽「被打破」：不再重建，等 ECL 重新武装（inactive→active）*/
    uint8_t  fam_seen;   /* ★ 2026-09-12：这一槽**曾经有过我们的使魔**（关闸「曾经有过」的记忆）。
                          *   与 `fam_id` 分开：`yk_fam_broken` / `yk_fam_gone` 都会清 `fam_id`，
                          *   但闸要一直夹到 ECL 重新武装（inactive→active 边沿）为止。
                          *   塞进原有 padding ⇒ `yk_slot_t` 仍是 24 B（单测有 sizeof 断言）。*/
} yk_slot_t;             /* 28 B × YK_FAM_MAX（2026-09-12 加 fam_seen 后 24→28，见该字段注）*/

typedef struct { yk_slot_t e[YK_FAM_MAX]; } yk_map_t;

void       yk_map_reset(yk_map_t *m);
unsigned   yk_map_fam_count(const yk_map_t *m);
yk_slot_t *yk_map_find(yk_map_t *m, uint32_t enemy_id, int slot);
yk_slot_t *yk_map_find_fam(yk_map_t *m, uint32_t fam_id);
void       yk_frame_begin(yk_map_t *m);
/* 扫到一个发射口槽。返回 1 = `*act` 里有一条要执行的动作（建 / 撤）。 */
int        yk_note_slot(yk_map_t *m, uint32_t enemy_id, int slot, int active, const float xy[2],
                        yk_action_t *act);
/* 帧末：本帧没扫到的条目 = 敌人没了 ⇒ 有使魔就撤，然后退休该条目。返回动作数。 */
int        yk_frame_end(yk_map_t *m, yk_action_t *out, int max);
/* 卡侧建完使魔后回填（记 fam_id、把 life 采样清成「未采样」）。 */
void       yk_fam_created(yk_map_t *m, uint32_t enemy_id, int slot, uint32_t fam_id);
/* §3.6 击破信号：使魔被打死 ⇒ 那个槽「被打破」。返回 1 = 这只使魔是本卡的。 */
int        yk_fam_broken(yk_map_t *m, uint32_t fam_id);
/* 使魔查不到了（过场 `destroy_all` / 静默移除 / 我们自己刚标了删除还没被 sweep）⇒ 松开。
 * ★ 2026-09-12：**不置 `broken`**（唯一复位口是 inactive→active 边沿，置了会把「下一帧补建」堵死；
 *   既有单测 `槽 3 静默移除 ⇒ 下一帧重建` 钉的就是这条）。关闸用的「曾经有过」记忆改由
 *   `fam_id` 承担 —— 清 0 那一刻闸夹上，重建回填后自动放开。 */
void       yk_fam_gone(yk_map_t *m, uint32_t fam_id);
/* 全部撤（高速 / miss / 局末 / 删卡）。返回动作数。 */
int        yk_withdraw_all(yk_map_t *m, yk_action_t *out, int max);

/* ==================================================== §3.5.1 发射口关闸（精确闸，2026-09-12）
 * 新 SDK 面 `on_enemy_shot_slot`（断点 `0x433ffa` = etOn 的「发弹三步」汇聚点，一手见 sdk.c）：
 * 返回非 0 ⇒ SDK 置 `retaddr = 0x43402d` ⇒ **本帧这一槽不发射**（零内存写、每帧每槽问一次、可逆）。
 *
 * **本卡判据**：`(enemy_id, slot)` 这个发射口**曾经有过我们的使魔、现在没了** ⇒ 掐掉本帧发射。
 *   `have_fam_id` = 卡侧按 id 回查当前活着的使魔（0 = 查不到）。
 *   返回 1 = 抑制。
 *
 * ⚠ **为什么不能只看「没有使魔就掐」**：`have_fam_id == 0` 有三种来路，只有②该掐 ——
 *   ① 我们还没为它建过（建被排到下一个 `on_tick_2`）⇒ **必须放行**，否则第一帧就把槽掐死、
 *      永远建不出使魔；
 *   ② 有过、现在没了 ⇒ 掐；
 *   ③ 表里没有这个 `(enemy_id, slot)` 条目（表满 / 已被 `yk_frame_end` 退休）= 我们**从没管过** ⇒ 放行。
 * ⇒ 「曾经有过」这一位记忆必须**单独存**（`yk_slot_t.fam_seen`）：`fam_id` 会被 `yk_fam_broken` /
 *   `yk_fam_gone` 清掉、`broken` 会被重新武装清掉，两者单独用都会漏（实测：用 `fam_id` 时
 *   「静默移除后那一帧」判不出来，用 `broken` 时「被打死后又武装过」判不出来）。
 *   复位口 = `yk_note_slot` 的 inactive→active 边沿（ECL 重新武装）+ `yk_map_reset`（删卡等）。
 *   **该函数不读 `props[].type`** ⇒ 不受「`etOn` 与 `etSprite` 谁先写 type」这个未定量的影响。 */
int        yk_shot_slot_suppressed(const yk_map_t *m, uint32_t enemy_id, int slot, uint32_t have_fam_id);

/* 硬约束护栏：spawn 的 life 参数 >= 1000 会进锁定黑名单 ⇒ 钳到 999（并在卡侧记一条日志）。 */
int32_t    yk_clamp_spawn_life(int32_t life);

/* =============================================================== §3.7 伤害转交 1/2
 * 触发器 = 使魔自己的 `life.current`（`enemy+0x6220`）**逐帧差分**（不是 `on_shot_damage`：
 * 那个回调第一行就有 `CE_PLAYER_FOCUSED()` 门，而妖机体 = 低速 = 聚焦 ⇒ 卡生效期间一次都不来，
 * 见 research-108-damage-transfer §0）。
 * 落点 = **每帧新建**一条 `ce_damage_rect` 打在 boss 坐标（一源一敌一辈子只结算一次，tag 守卫
 * `0x45f45a` ⇒ 不能复用）+ 同帧 `ce_raise_damage_cap(基值 + 转嫁值)`。
 * 比例 = 1/2（**用户 2026-09-12 定案**；❓ 自定 —— TH08 没有这条管线，不是复刻，spec §3.7）。*/
#define YK_TRANSFER_DEN 2                       /* 1/2 */
#define YK_TRANSFER_CAP_PER_FRAME 0             /* ★ TODO(决策 §7.5-1)：单帧转嫁上限。0 = 无上限（占位）。
                                                 *   研究提醒：使魔一刀 2000 ⇒ boss 一帧 1000，会顶掉 boss 的
                                                 *   每帧配额并放大计分 ⇒ 要不要封顶由用户定。*/
typedef struct {
    int32_t carry;      /* 奇数额外那半的进位（累计到偶数再补，见 yk_half_with_carry）*/
    int32_t pending;    /* 本帧要转嫁的量（卡侧转完清零；取不到 boss 时按研究 §4 丢掉并记账）*/
} yk_transfer_t;

void    yk_transfer_reset(yk_transfer_t *t);
/* delta 的一半（向下取整），余数进 `*carry` ⇒ 连续两帧的奇数伤害**不丢半分**。 */
int32_t yk_half_with_carry(int32_t delta, int32_t *carry);
void    yk_transfer_feed(yk_transfer_t *t, int32_t delta);
/* cap <= 0 = 无上限（占位默认）；否则钳到 cap。 */
int32_t yk_transfer_clamp(int32_t pending, int32_t cap);
/* 采样一只使魔的 life.current：返回本帧掉血量（>0）。
 * 第一次采样 / 换了一只使魔（`fam_id` 变了）/ life 变大（引擎自己重算）都返回 0。 */
int32_t yk_sample_life(yk_slot_t *e, uint32_t fam_id, int32_t life_now);
/* 自伤保护：转嫁源打在 boss 坐标的小盒上。若某只使魔**也**在这个盒子里
 * （boss 的发射口就在本体上 ⇒ 使魔被钉在本体中心，这很常见），它这一帧掉的血是我们自己的源打的，
 * 计入转嫁会**每帧自激**（research §3② 那条回路）。盒 = 以中心为心、宽 w × 高 h，
 * 四周各加 `YK_FAM_HURTBOX_HALF`（惰性真敌人的默认 hurtbox 24.0 的一半，spec §6.1）。
 * 返回非 0 = 在盒内 ⇒ 这一帧的掉血**不计入**转嫁。 */
#define YK_FAM_HURTBOX_HALF 12.0f
int     yk_in_transfer_zone(float fx, float fy, float cx, float cy, float w, float h);
