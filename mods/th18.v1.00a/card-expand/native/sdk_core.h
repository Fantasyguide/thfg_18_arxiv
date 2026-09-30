/* sdk_core.h —— 行为 SDK 里不依赖平台的部分：行为注册表、与 JSON 的对账、私有状态槽。
 *
 * 纯 C11（只 <stdint.h>），主机上 make test-host 直接测。桩 / 断点 / 引擎访问在 sdk.h / sdk.c。
 * 设计见 ../SDK.md §3–§4。
 */
#pragma once
#include <stdint.h>

#define CE_SDK_CARD_ID_MIN   58
#define CE_SDK_CARD_ID_MAX   254
/* = id 空间大小（58..254 = 197）。旧值 71 是图鉴条目数那两处 `cmp r,imm8` 的符号界；
 * 那两处 2026-09-19 已改成无符号字节比较（`sites.py` 的 `MENU_COUNT_BCMP`，上界 255），
 * 于是这里的界回到「id 空间 / 表行」本身，与 `CE_CARD_MAX_NEW` 同步。*/
#define CE_SDK_MAX_BEHAVIORS (CE_SDK_CARD_ID_MAX - CE_SDK_CARD_ID_MIN + 1)
#define CE_STATE_SLOTS       256       /* 同时存在的带状态卡对象上限（一局最多 256 张卡，0x411469）*/
#define CE_STATE_BYTES       512       /* 每张卡私有状态上限（2026-09-08 256 → 512：股市 id 80 要 430+ 字节；静态表 256 × 512 = 128 KB）*/

/* 一张卡的行为：id + 它的虚表（21 槽拷贝）+ 一句给日志看的槽清单 */
typedef struct {
    uint32_t    id;
    const void *vtable;
    const char *slots;                 /* 例如 "ctor,on_tick_2" */
    const void *hooks;                 /* ce_hooks_t*（sdk.h）；事件断点按虚表找到它 */
    uint8_t     is_active;             /* active_recharge != 0；供不依赖 ce_hooks_t 的契约检查 */
} ce_behavior_t;

enum {
    CE_SDK_REG_NONE = 0,
    CE_SDK_REG_ID_RANGE,
    CE_SDK_REG_DUPLICATE,
    CE_SDK_REG_CAPACITY,
};

/* 登记；返回 0 = id 越界、重复或容量已满。第一个错误会一直保留到测试 reset。 */
int ce_sdk_register(const ce_behavior_t *b);
int ce_sdk_registration_error(uint32_t *id, int *reason);
unsigned ce_sdk_behavior_count(void);
const ce_behavior_t *ce_sdk_behavior_at(unsigned i);
/* 按 id 找；NULL = 这张卡没有行为（保持基类虚表） */
const ce_behavior_t *ce_sdk_find(uint32_t id);
void ce_sdk_reset_for_test(void);      /* 只给单测用 */

/* 对账：json_ids[0..n) 是 cards.js 登记的 id。
 *   返回 0 且 *bad_id = 第一个「C 有行为、JSON 没登记」的 id（FAIL）；
 *   返回 1；*unbound = 「JSON 有、C 无行为」的张数（允许，开发期正常）。 */
int ce_sdk_bind_check(const uint32_t *json_ids, unsigned n, uint32_t *bad_id, unsigned *unbound);

enum {
    CE_SDK_CONTRACT_NONE = 0,
    CE_SDK_CONTRACT_ACTIVE_BEHAVIOR_REQUIRED,
    CE_SDK_CONTRACT_ACTIVE_CATEGORY_REQUIRED,
};
int ce_sdk_contract_check(const uint32_t *json_ids, const uint32_t *categories, unsigned n,
                          uint32_t *bad_id, int *reason);

#define CE_STATE_RESERVED    16        /* 块头留给 SDK（主动卡状态机 ce_active_t）；卡的私有状态从这之后起 */

/* 私有状态槽：键 = 卡对象指针。alloc 幂等（同键返回同块，首次清零）；size > CE_STATE_BYTES 或槽满返回 NULL。 */
void *ce_state_alloc(const void *key, unsigned size);
void *ce_state_get(const void *key);   /* 没分配过返回 NULL */
void  ce_state_free(const void *key);  /* 没分配过是空操作 */
unsigned ce_state_in_use(void);
/* 卡侧私有状态：同一把键、同一块，返回块头 CE_STATE_RESERVED 字节之后的地址；size 超出剩余空间返回 NULL。 */
void *ce_state_user(const void *key, unsigned size);

/* ---- 自机弹伤害倍率层（AUDIT §AA）----------------------------------------
 * 「给自机弹加伤害」这件事出现了第三次（K♠ 61、古锭刀 76、衔尾蛇 78），提炼成一层。
 * 每张参与的卡报一个千分比（CE_DMG_UNIT = ×1），这里连乘出**聚合倍率**，并指定其中一张卡
 * （id 最小的那张）当「落笔人」——只有它的 `+0x28` 桩真的写 `bullet+0x9c`，所以一颗弹只被乘一次、
 * 只取整一次，与卡在链表里的顺序无关。上限也由落笔人在 `+0x2c` 里按同一个倍率抬。
 * 纯逻辑，无引擎依赖，主机单测。*/
#define CE_DMG_UNIT 1000

void ce_dmg_reset(void);
/* 登记 / 更新一张卡的贡献。permille = 0 表示这张卡不在场（绑定时登记声明值，`+0x50` 删对象时清 0）。*/
void ce_dmg_set(uint32_t id, int permille);
int  ce_dmg_aggregate(void);      /* 千分比连乘；没有贡献者时 = CE_DMG_UNIT */
int  ce_dmg_applier(void);        /* 落笔那张卡的 id；没有贡献者时 -1 */
int  ce_dmg_scale(int damage);    /* damage × 聚合 / UNIT，单次取整；聚合 = UNIT 时原样返回 */

/* 集卡判定（纯函数）：self 在 set 里，且 set 里其余每张 owned[id] != 0 → 1。owned 是 int[255]。*/
int ce_royal_flush_ready(const int32_t *owned, uint32_t self_id, const uint32_t *set, unsigned n);
/* 神之宣告：消耗一半残机（向上取整）；残机 ≤ 0 返回 0 = 不能发动 */
int ce_judgment_cost(int lives);
/* 黄金小丑：利息 = money 的 percent％ 向下取整；money ≤ 0 返回 0 */
int ce_interest(int money, int percent);
/* 倒吊人：n 个候选里按游戏 RNG 的一个 dword 选下标；n == 0 返回 -1 */
int ce_pick_index(uint32_t rnd, unsigned n);

/* ---- 无敌帧声明层（EVENTS-CONTRACT.md §4；107 结界 0.67s / 117 霊撃 40f 都写 player+0x47774）----
 * 直接各写各的会互相**截短**（后写的短无敌盖掉前面给的长无敌）⇒ 改成「各卡声明、取最大值」。
 * 纯逻辑、无引擎依赖（写计时器在 sdk.h 的 ce_invuln_apply 里做）。*/
void ce_invuln_reset(void);
/* 登记 / 更新一张卡的本帧请求（frames <= 0 = 这张卡本帧不参与）。绑定时清 0、删对象时清 0。*/
void ce_invuln_request(uint32_t id, int frames);
int  ce_invuln_frames(void);      /* 本帧应给的无敌帧数（取最大）；无贡献者 = 0 */
int  ce_invuln_applier(void);     /* 最长的那张卡的 id（并列取 id 最小）；无贡献者 = -1 */

/* ==================== ★ PoC 高度层（= 收点线 / 「回收线」，2026-09-15）====================
 * 字段 = `player+0x47998`（float，640 空间 y，**越大越靠屏幕下方 = 线越低**）。
 * 一手见 [`engine/player/th18/05-poc-height-and-indicator.md`](../../../../../engine/player/th18/05-poc-height-and-indicator.md)：
 *   · 全 exe **只有两个写点** —— `Player__reset` `0x45ae8f` 写 **128.0f**、原生卡 **22 ITEM_LINE（CardKanako）**
 *     的 `on_load` `0x40c275` 写 **224.0f**（外加置 `Gui+0x194` bit `0x2000` = 「画那条线」）；
 *   · 开局那条指示线（`front.anm` script `0x39`）**每建一次都现读这个字段**算位置
 *     （`0x43afe1`：`y = poc × 2 − 80.0`）⇒ **字段改了显示就跟着变**，不需要动资产。
 *
 * ## 形状（「声明 → 聚合 → 落笔」；与无敌层同形，不是子机层那种「选举落笔者」）
 *   1. **声明**：卡声明自己想要的**绝对高度**（float 的位型）。**粘性**（照原生卡 22：开局写一次管到改口），
 *      删卡 / 退出走 `ce_poc_drop()` / `ce_poc_release()`。
 *   2. **聚合 = 取最大**（多条声明同时在场时取 **y 最大**的那条 = 线最低的那条）。理由：
 *      · 「过线全屏收点」的收益对 y **单调** ⇒ 取最大 = 谁的收益都不打折，与 `ce_invuln_*`「取最大、不截短别人给的」同一条纪律；
 *      · 「相加」在这里**没有语义**（两条线只有一个真相，128+160 不是任何一张卡想要的东西）；
 *      · 「取最小」会让后进场的低线卡白声明（先到的 224 把 288 顶掉）。
 *      ⚠ 反过来说：**「把线抬高」的卡（y 更小）在有人降低时会被忽略** —— 本仓没有这种卡（用户口径只有「降低」）；
 *      真要做「难度抬线」得另开一条通道，登记 `TUNING.md`。
 *   3. **夹取**：`[0, 448]`（448 = 游玩区高；一手：ECL y ∈ [0,448]，`engine/anm/th18/01-vm-instantiate.md` §3）。
 *   4. **落笔**：每帧一次写字段（`sdk.h` 的 `ce_poc_apply()`，帧界 `BP_ce_item_frame`）。
 *      ★ **没有声明时一个字都不写** —— 否则会把原生卡 22 写进去的 224 顶回 128。
 *   5. **只读访问器** = `ce_poc_height()`（`sdk.h`）：返回**字段当下的值**（= 引擎真正拿去用的那个数，含原生卡的 224）。
 *      有声明时因为每帧落笔，它就是声明值；万一别人（引擎 / 原生卡）中途改过，读出来的也是**改后的真值**
 *      —— 这正是「按相对量下移」的卡需要的（见下）。
 *   6. **基准记账（「被删掉会怎样」）**：本层在**第一次落笔之前**记下字段里的原值（= 基准），
 *      声明**全部撤掉**时把基准写回去（`ce_poc_release()` 立刻写，不等帧界 —— 删卡发生在**商店**里，
 *      那一刻 AbilityManager 与 ItemManager 都冻住，帧钩子不跑）。
 *      ⚠ 基准会被**刷新**：若发现字段的值既不是我们上次写的、也不是本次目标（= 别人改过，典型是原生卡 22
 *      或 `Player__reset`），就把新值当基准 ⇒ 删卡时写回的是**别人的**值，不是写死 128。
 *
 * 纯逻辑、无引擎依赖（float 一概走**位型** `uint32_t`：非负有限 float 的位型序 = 数值序，
 * 聚合/夹取全在整数域做；真正写字段由 `sdk.h` 做）。主机单测 `tests/test_sdk_core.c`。*/
#define CE_POC_HEIGHT_DEFAULT_BITS 0x43000000u   /* 128.0f —— 一手 `0x45ae8f`（Player__reset）*/
#define CE_POC_HEIGHT_KANAKO_BITS  0x43600000u   /* 224.0f —— 一手 `0x40c275`（原生卡 22 on_load）*/
#define CE_POC_HEIGHT_MAX_BITS     0x43e00000u   /* 448.0f —— 游玩区高（夹取上限）*/
#define CE_POC_HEIGHT_MIN_BITS     0x00000000u   /* 0.0f（夹取下限；也是「退出声明」的哨兵）*/

/* float ↔ 位型（**只做位拷贝**，不产生任何浮点运算 ⇒ 不碰 x87）。*/
static inline uint32_t ce_f32_bits(float f)        { union { float f; uint32_t u; } v; v.f = f; return v.u; }
static inline float    ce_f32_from_bits(uint32_t b) { union { float f; uint32_t u; } v; v.u = b; return v.f; }

void ce_poc_reset(void);                     /* 全清（`ce_sdk_setup` / 单测）：连基准一起清 */
/* 声明一张卡想要的高度（float 位型）。**粘性**：设一次管到改口 / 撤声明。
 * `bits == 0`（0.0f）= 退出声明（照 `ce_dmg_set(id,0)` 的惯例）；负 / NaN / Inf = 拒收。
 * 返回 1 = 记下；0 = 拒收或退出声明。id 越界同样返回 0。*/
int  ce_poc_declare(uint32_t id, uint32_t height_bits);
void ce_poc_drop(uint32_t id);               /* 撤一张卡的声明（删卡 / 自己退出；不写引擎）*/
int  ce_poc_has_declaration(void);           /* 有人在声明吗？（= 落笔的闸门）*/
uint32_t ce_poc_target_bits(void);           /* 聚合（取最大、夹取）后该写进字段的值；无声明 = 0 */
int  ce_poc_winner(void);                    /* 最高那条声明的卡 id（并列取 id 最小）；无声明 = -1 */
/* 生效值：有声明 ⇒ 聚合值；无声明 ⇒ 引擎字段的实读值（**不写**任何东西）。纯函数，供单测与诊断。*/
uint32_t ce_poc_effective_bits(uint32_t engine_bits);

/* ---- 帧界落笔的**决策**（纯逻辑；真正写内存的是 `sdk.h` 的 `ce_poc_apply()`）----
 * `engine_bits` = 调用方读到的字段当下值。返回 1 且填 `*out_bits` = 本帧该写这个值；0 = 本帧别写。*/
int ce_poc_frame(uint32_t engine_bits, uint32_t *out_bits);
/* 删卡 / 退出路径（`ce_layer_drop_card` 调）：撤声明；若撤完**没人声明**且记过基准 ⇒ 返回 1 并把基准填进
 * `*out_bits`（调用方立刻写回）。返回 0 = 不用写。*/
int ce_poc_drop_take_base(uint32_t id, uint32_t *out_bits);
/* 基准只读（诊断 / 单测）：1 = 记着基准（`*out_bits` 填值）。*/
int ce_poc_base(uint32_t *out_bits);

/* ============ ★★ 2026-09-20 「遮」声明（覆盖位）/ 语义高度 / 线显示（106 的非接管闸）============
 * 为什么有这一节（背景与一手，逐条给地址）：
 *   · 106 的设计「**上线收点当且仅当满 P**」原本靠**接管**两条 `ja`（`th18:0x445bb9` / `0x446003`）
 *     实现 —— 实测**不可接管**（道具只减速、不变橙不抖）且**不可回放**（窗口是一条裸 `ja rel32`，
 *     回放会从跳板池按原位移跳进未写区 `00 00` ⇒ 写 `0x1` 违例 ⇒ 崩）⇒ 两条站点**永久 park**，
 *     见 [`AUDIT.md`](../AUDIT.md) §BE-EQ / §BE-ER。功能改走**动 POC 字段**这条不接管的路
 *     （[`WORK-PACKAGES.md`](../WORK-PACKAGES.md) §7 A/A2/A3/A4）。
 *   · 字段 `player+0x47998` 的**全部消费者**（一手穷举 = 2 写 5 读，见
 *     [`engine/player/th18/05-poc-height-and-indicator.md`](../../../../../engine/player/th18/05-poc-height-and-indicator.md)）：
 *     两条 `ja`（`0x445baa` / `0x445ff4`，**过线 ⇒ 道具进真空态**）、
 *     `collect_power_item` `0x44698d` 与 `collect_money_item` `0x446bf9`（**线上满额 / 线下按距离折额**）、
 *     开局指示线的位置读点 `0x43afe1`（+ 可见性位）、以及**我们自己的**几个读数处。
 *
 * ## 三个概念（与前两节同形：声明 → 聚合 → 落笔；纯逻辑、无引擎依赖）
 *   1. **「遮」声明（= 覆盖位）**：`ce_poc_mask_declare(id, on)`。它在场时**无条件生效** ——
 *      落笔值 = `min(语义高度, 自机 y)`，忽略别家声明的**高度**（现有仲裁「取最大 y 者生效」
 *      表达不了"覆盖"）。夹取到 ≥ `CE_POC_TINY_BITS`（0.0f 是"未声明"哨兵，负值位型按无符号比会翻）。
 *   2. **语义高度**：`ce_poc_semantic_bits(engine_bits)` = **不含「遮」**的生效值
 *      （别家声明取最大 → 基准 → 引擎字段实读值）。我们自己的读数处（106 / 111 / SDK 计分）一律读它
 *      ⇒ 它们的语义 = **没有 106 时**的语义（用户口径：「106 只做『过线不收点』，其它语义零变化」）。
 *   3. **线显示**：`ce_poc_line_hide_declare(id, on)`。有人声明时落笔（`sdk.h` 的 `ce_poc_apply()`）
 *      **清** `Gui+0x194` 的 bit `0x2000`（= 不画开局那条指示线）；撤完按**基准**写回。
 *      一手：那一位的**唯一写者 = 原生卡 22**（`0x40c288` 的 `or`，**只置不清**），唯一读点 `0x43af0b`
 *      ⇒ 「主动清」是本层新增的写点（登记 `AUDIT.md` §BE）。owner 口径（§7-A4）＝ **(ii) 106 在场整局不画**。
 *
 * ## ★ 为什么落笔值取 `min(语义高度, 自机 y)` 而不是「固定极小正数」（对 `WORK-PACKAGES` §7-A 原稿的订正）
 * 固定极小正数（≈ 0）确实让「线 > 自机 y」永不成立（过线不吸 ✓），但它把**另外两个引擎读点**一起改了：
 * 那两个读点看到的 `d = y − 0 = y`（最大 448px）⇒ 折扣系数 `0.9·(1 − d/450)` 掉到 0.004…0.9。
 *   · 语义上该给**满额**的那些颗（自机在**语义线**上方）会被打成 0.004 档；
 *   · 想用计分钩子按语义高度反推，倍率 = `c_语义 / c_遮` 在 y→448 时高达 **113×**
 *     ⇒ `ecx = 1000×G`（最高 ~1e8）× 113 **溢出 32 位** ⇒ 只能饱和 ⇒ 反而少给分。
 * 而 `min(语义高度, 自机 y)` 是把**语义几何**原样喂给引擎（`ce_poc_fold_ratio_permille()` 仍留作兜底）：
 *   · 自机在语义线上方 ⇒ 落笔值 `== y` ⇒ 引擎 `jae`（`线 ≥ y`）取**满额支**；两条 `ja`（`线 > y`，严格）
 *     **不成立** ⇒ 不吸 ✓ —— 一条字段同时满足「不吸」与「满额」，靠的正是两条判据的**严格/非严格之差**；
 *   · 自机在语义线下方 ⇒ 落笔值 `== 语义高度` ⇒ 引擎算出的 `d` 与「没有 106」时**同一个数** ⇒ 折额逐位相同 ✓。
 * ⇒ 计分**不需要事后补偿**就成了；`ce_poc_fold_ratio_permille()` 只在字段被第三方改过时才非 1000。
 *
 * ## ★ 载荷假设（唯一的、可复核的一条）：帧内顺序
 *   AbilityManager（优先级 `0x16`，卡 tick 在这里发声明）→ **Player `0x17`**（`th18:0x45caa0`，注册在 `0x45a8b4`）
 *   → … → **ItemManager `0x1e`**（`th18:0x446ec0`，注册在 `0x4458ac`）。
 *   落笔发生在 `BP_ce_item_frame` = `0x446ee3`（`ItemManager__on_tick` 里、body `0x445a80` **之前**），
 *   两条 `ja` 与两个折额读点**都在同一个 tick 的 body 里** ⇒ 落笔时读到的自机 y 就是本帧最终值，
 *   且中间没有任何 UpdateFunc 会动它 ⇒「`线 ≤ y`」在整个 ItemManager tick 里恒成立 ⇒ **不会漏吸**。
 *   （若哪天顺序变了：漏吸是可见的、折额由 `ce_poc_fold_ratio_permille()` 兜底。）
 */
#define CE_POC_TINY_BITS 0x00800000u   /* 最小**正规**正 float（1.17549435e-38）——❓自定：一手只给了
                                        * 「字段是一个 float」「0.0f = 未声明哨兵」「聚合按位型无符号比」三条约束
                                        * ⇒ 表达"屏幕最上沿之外"只能用**极小正数**；取最小正规数（而不是最小
                                        * 非正规 0x00000001）是为了不把 SSE 的非正规数处理扯进来。
                                        * 登记 `TUNING.md` §2.0m。*/

int  ce_poc_mask_declare(uint32_t id, int on);   /* 粘性：1 = 本卡要求「遮」（过线永不成立）。0 = 撤回 */
int  ce_poc_mask_active(void);                   /* 落笔闸门：有人在遮吗？ */
int  ce_poc_mask_owner(void);                    /* 声明者 id（最小）；无人 = -1 */
/* **语义高度**（不含「遮」的生效值）：别家声明取最大 → 记过的基准 → `engine_bits`（字段实读值）。
 * `bits != 0` 时返回的也是位型；绝不写任何内存。我们自己的读数处一律读它。*/
uint32_t ce_poc_semantic_bits(uint32_t engine_bits);

/* 帧界落笔的决策（`ce_poc_frame` 的「遮」版；真正写内存的是 `sdk.h` 的 `ce_poc_apply()`）。
 * `ceiling_bits` = 本帧的钳位上限 = **自机 y 的位型**（调用方在落笔那一刻读，见上面的载荷假设）；
 * 传 0（读不到自机）⇒ 退回 `CE_POC_TINY_BITS`（= 永远过不了线）。
 * 返回 1 且填 `*out_bits` = 本帧该写这个值；0 = 本帧别写（没有「遮」时逐字转发 `ce_poc_frame`）。*/
int ce_poc_frame_masked(uint32_t engine_bits, uint32_t ceiling_bits, uint32_t *out_bits);

/* ---- 线显示（`Gui+0x194` bit `0x2000`；唯一原生写者 = 卡 22 `0x40c288`，只置不清）---- */
void ce_poc_line_hide_declare(uint32_t id, int on);   /* 粘性：1 = 不画那条开局指示线 */
int  ce_poc_line_hidden(void);                        /* 有人在声明「不画」吗？ */
int  ce_poc_line_hide_owner(void);                    /* 声明者 id（最小）；无人 = -1 */
/* 帧界的决策（`ce_poc_apply` 调）。`bit_now` = 那一位当下的值（0/1）、`decl_exists` = 有人声明高度。
 * 返回 1 且填 `*out_bit` = 把这一位写成这个值；0 = 一个字都不写。规则：
 *   ① 有人 hide ⇒ 目标 0（**第一次**清之前把 `bit_now` 记成基准）；
 *   ② hide 撤完且记过基准 ⇒ 按基准写回一次（然后忘掉基准）；
 *   ③ 都没事、但有高度声明 ⇒ 目标 1（= 旧行为：照原生卡 22 把那条线打开）。*/
int ce_poc_line_apply(int bit_now, int decl_exists, int *out_bit);
/* 删卡 / 局末路径（`ce_poc_release` 调）：撤声明；若撤完没人 hide 且记过基准 ⇒ 返回 1 并填 `*out_bit`
 * （调用方**立刻**写回 —— 删卡发生在商店里，帧钩子不跑）。返回 0 = 不用写。*/
int ce_poc_line_release(uint32_t id, int *out_bit);
void ce_poc_line_reset(void);                         /* 全清（`ce_sdk_setup` / 单测）：连基准一起清 */

/* ---- 引擎折额的**兜底补偿**（倍率千分点；1000 = 不动）----
 * 一手（`th18:0x446c17`–`0x446cf3`，与 `0x4469a7`–`0x446a41` 同形）：折额系数 = `0.9·(1 − d/450)`，
 * `d = ⌊y − 字段⌋`（`cvttss2si` = 截断）；`d ≤ 0`（或道具 `state == 3` 真空飞行）走**满额支**。
 * 本函数返回 `c_语义 / c_实际`（都在同一族里 ⇒ `0.9` 约掉，只剩 `(450−d_语义)/(450−d_实际)`）：
 *   · 落笔值就是本层「遮」的 `min(语义高度, y)` 时，两个 `d` 相等（或都走满额支）⇒ **1000**；
 *   · 只有第三方改过字段 / 帧内时序被破坏时才 ≠ 1000，这时按语义高度把值掰回来；
 *   · 倍率夹在 `[1, 4000]`（越界 = 数值不可信 ⇒ 回 1000，宁可少给也不放大 32 倍）。
 * 输入都是 **Q16 定点**（与计分层其余部分同单位；`vacuum` = 道具 `state == 3`）。*/
int ce_poc_fold_ratio_permille(int32_t y_q16, int32_t poc_field_q16, int32_t poc_sem_q16, int vacuum);

/* ============================ ★ 计分层（2026-09-13 按用户公式重做）============================
 * 规格（用户 2026-09-13；逐条推导与一手证据见 `../SCORE-LAYER-REWORK.md`）：
 *
 *   吃一颗点道具（TH18 = 金钱）：
 *       SCORE += G × coeff_06 × rest ;  G = G_0 + Σ G_i
 *
 * `G_0` = TH18 原生「身价」换算成的**点值**（= 引擎自己给这一颗的 SCORE 增量）。一手：引擎在
 * `th18:0x446b30`/`0x446b86` 算 `ecx = (1 + 资金力) × 难度修正 × 100`（EASY 修正 = 500，其余 = 1000）
 * 并钳到 `[1,000,000, 难度表值 × 100]`（`0x4ccd28`/`0x4ccd2c`），满额支随后 `SCORE += floor(ecx/1000)`
 * ⇒ **`G_0` = ecx/1000**；它在本层里的取值范围 `[1000, 难度表值/10]`。
 * ★ 2026-09-13 订正：用户给的「下限 10000 / 上限 E 200000…」是**身价**（= 点 × 10 = 引擎弹窗值）
 * 单位下的同一批界（引擎自己钳的就是它们），换算成点要 ÷10 —— 层里两处单位**别混**（见下）。
 * `G_i` = **各作「额外得点」，加法族**（110 风神录的信仰、111 地灵殿的绿点、107 妖妖梦的樱点…），单位 = 点。
 *
 * **合成规则（本层的全部内容）**：
 *   `G = G_0 + Σ_i uncoupled_i + coupling_permille × Σ_j coupled_j / 1000`
 *
 *   · **耦合只作用在它自己那一族上**（用户规格里唯一带乘数的一项）——
 *     声明 `ce_score_declare_coupled()` 的加数会被 `ce_score_coupling_permille()` 缩放；
 *     未声明的（`ce_score_declare_delta()`）**不缩放**。110/111 是同一个「信仰」量纲 ⇒ 走耦合通道；
 *     107 的「最大得点」不是 ⇒ 走未耦合通道。
 *   · **落笔点 = 站点 `th18:0x446bf9` 的 `ecx`**（`sdk.c` 的 `BP_ce_item_score_in`）：写
 *     `ecx = 1000 × G` ⇒ 引擎满额支 `SCORE += floor(ecx/1000) = G` **精确**，且坐标必须是 1000 的倍数
 *     （是 ⇒ 引擎「量化到 10 的倍数」那一步是恒等，不引入舍入）。没有卡声明时 SDK **一个字都不写**。
 *     `ce_score_apply()`（= `G × 10`）保留给「引擎单位」的旧读法 / 单测，**不再是落笔路径**。
 *   · **coeff_06 不在本层** —— 引擎自己按收取高度对身价打折（一手 `th18:0x446bf9`–`0x446cf3`，
 *     判据 = 回收线 `player+0x47998` 与 **`player+0x624`（自机的 y）** 比较），本层只**读出来**
 *     （`ce_score_coeff_permille()`）做诊断 / HUD。★ 因为落笔点搬到了 `0x446bf9`（那两段之前），
 *     引擎的系数现在乘在**整笔 `G_0 + ΣG_i`** 上。⇒ **不新建、也不重复施加**
 *     （用户规格：它「实际上是已经在运作的机制本身」）。
 *
 * **旧接口**：`ce_score_declare` / `ce_score_mul_declare` 保留为 **legacy** 语义（见本节末），
 * 单卡时与新接口逐字节等价（有单测），多卡时不再「id 最小者独占落笔」。
 * 全部纯逻辑、无引擎依赖（桥值由 `sdk.c` 填），主机单测 `tests/test_sdk_core.c`。*/

/* ---- 常量（一手；出处逐条写在工程里）----
 * ★★ 2026-09-13 单位订正（一手：`th18:0x442840`–`0x4428cd` 写界、`0x446b53`–`0x446bf1` 用界）：
 *   下面这六个「上下限」是**身价**单位（身价 = 引擎的弹窗值 = 点 × 10 = 站点 `ecx` ÷ 100）：
 *     · 下限 10000 = `0x4ccd28`（= 表 `0x4b666c` 值 10000 ×100 = 1,000,000）÷ 100；
 *     · 上限 = `0x4ccd2c`（= 表 `0x4b6654` 值 ×100）÷ 100 = 表值本身。
 *   引擎自己就把身价钳在这个区间里（钳的是 `ecx`，±100 倍），所以**在站点现场，层这次钳是恒等**；
 *   它只在「桥值来自别处 / 单测直接喂界外值」时有意义 ⇒ `ce_score_base_points()` 必须**先钳身价再 ÷10**。*/
#define CE_SCORE_BASE_MIN_PTS     10000      /* 身价下限（= 0x4ccd28 / 100；开局 0x4ccd24 的初值就是它）*/
#define CE_SCORE_BASE_MAX_EASY    200000     /* 身价上限（= 0x4ccd2c / 100，逐难度；表 th18:0x4b6654）*/
#define CE_SCORE_BASE_MAX_NORMAL  500000
#define CE_SCORE_BASE_MAX_HARD    700000
#define CE_SCORE_BASE_MAX_LUNATIC 1000000
#define CE_SCORE_BASE_MAX_EXTRA   500000
#define CE_SCORE_EASY_K           500        /* 难度修正：EASY 分支 th18:0x446b30（DAT 0x4b93ac = 500.0f）*/
#define CE_SCORE_NORMAL_K         1000       /* 其余难度 th18:0x446b86（DAT 0x4b93d4 = 1000.0f）*/
#define CE_SCORE_ENGINE_UNIT      10         /* 「点」→「身价/引擎单位」的倍数：身价 = 点 × 10（`SCORE += esi/10`）*/
#define CE_SCORE_GRAZE_STEP       100        /* ★ 擦弹倍率是**整数阶梯**：每 100 擦弹 +0.01（th11 一手 ⌊graze/100⌋）*/
#define CE_SCORE_GRAZE_STEP_MAX   899        /* 步数上限 = 8.99（一手 `th11:0x41f4e7` 的 `0x383`）*/
#define CE_SCORE_GRAZE_MULT_MAX   8990       /* 千分点上限（**不是 graze 阈值**；阈值 = 89900 擦弹）*/
#define CE_SCORE_COUPLING_DEFAULT 1000       /* 耦合基准 = 1000‰（「有 111 才乘」里的 ×1）*/

/* ---- 难度判据（本层自带；EASY 修正表与上下限都按它选）---- */
enum { CE_SCORE_DIFF_EASY = 0, CE_SCORE_DIFF_NORMAL = 1, CE_SCORE_DIFF_HARD = 2,
       CE_SCORE_DIFF_LUNATIC = 3, CE_SCORE_DIFF_EXTRA = 4 };

/* ---- 桥（每颗道具、在卡回调之前填；sdk.c 的 `BP_ce_item_score_in` 调）---- */
void ce_score_reset(void);                 /* 全清（SDK 初始化 / 单测）*/
/* `raw_value` = **身价**（单位 = 点 × 10 = 引擎的弹窗值）：`sdk.c` 在新站点 `th18:0x446bf9` 用
 * `ecx / 100` 填（`ecx` 是引擎钳位后的身价、单位 1/1000 点）⇒ `ce_score_base_points() == ecx/1000`。
 * ★ 2026-09-13 订正：老站点（`0x446cf6`）填的是**引擎算完的最终值**（= 10 × 点，满额支同值、
 * 线下支已被打折），所以那时这个入参的含义虽然同量纲、却已经被系数污染过。
 * `difficulty` = `th18:0x4ccd00`（0 EASY … 3 LUNATIC、4 EXTRA）。两个都由 sdk.c 从引擎读。*/
/* ============================ ★★ 计分层换版：每项走自己原作的曲线（用户 2026-09-18 定案）============================
 * 总公式：`G = G_0 × coeff_0 + Σ_i ΔG_i × coeff_i`（口径与逐作 `coeff_i` 表见
 * `engine/faith/OVERVIEW.md` 的「★★ coeff_i 到底有几种？」一节）。
 *
 * **落地路线 = A（写值时就归一）**，理由是用户 2026-09-18 那句「引擎显示不能照着 G 写吗？」——
 * 一手：`th18:0x446cfc` 把 `esi` 交给显示 `call th18:0x4645f0`，紧接着 `th18:0x446d0d` 的魔数除法拿**同一个 `esi`**
 * 做 `SCORE += esi/10` ⇒ **显示与分数同源**；归一之后**两者都照 G 走**（路线 B 只修分数，显示仍是引擎自己那个数）。
 *
 * 本层写出去的 = **归一后的值** `G_norm = G_0 + Σ_i ΔG_i × (coeff_i / coeff_0)`（点），
 * 引擎随后乘 `coeff_0` ⇒ 实际到手 = `G_0·coeff_0 + Σ ΔG_i·coeff_i` ✓。
 *
 * ★ **全程整数**（用户 2026-09-18 追问「中间的过程支持浮点吗？」）：`d` 用 Q16 定点（`sdk.c` 现成），
 *   各作曲线本来就是整数魔数除法（`k_mille·(450−d)/450`），比值 0.75 族恒 `5/6` ⇒ **取整顺序照抄原作**才逐位一致。
 *   钩子路径**不碰 xmm0–7**（thcrap 只保证 GPR），也不碰 x87（`make dllx87` 门）。
 *
 * 系数族（卡的「这一份 ΔG_i 属于谁」；**不声明 = 老行为逐字节不变**）：
 *   `NONE`         → `coeff_i = coeff_0`（比值 1）＝ 今天所有卡的默认
 *   `FAMILY_075`   → th12–th17 ⇒ 比值恒 **5/6**（同锚同 `d`；这条已在 OVERVIEW 里证过）
 *   `FAMILY_08`    → th08 ⇒ `coeff_i = 500 − d`（千分点，`d` ≤ 500 段）⇒ 比值 `(500−d)/coeff_0`
 *   `FAMILY_1`     → th07 / th11 ⇒ **不吃折扣** ⇒ 比值 `1000 / coeff_0`（线下会反向放大）
 *   `FAMILY_10`    → th10 ⇒ 🟡 还缺输入（`F = 显示信仰`），暂按 `NONE` 走并在日志里报一次（TODO）。
 */
typedef enum {
    CE_COEFF_FAMILY_NONE = 0,
    CE_COEFF_FAMILY_075  = 1,
    CE_COEFF_FAMILY_08   = 2,
    CE_COEFF_FAMILY_1    = 3,
    CE_COEFF_FAMILY_10   = 4
} ce_coeff_family_t;

/* 几何桥（每颗道具、在卡回调之前填）：`y_q16` = 自机 y、`poc_q16` = 回收线高度，都是 Q16 定点。
 * ★ 有了它本层就能**自己算 `coeff_0`**（引擎那段也是整数骨架）：线上（y ≥ 线）= 1000‰、
 *   线下 = `900·(450−d)/450`（`d` = 线 − y，px；`d ≥ 450` ⇒ 0‰ = 引擎只给「下限 +1 分」那一档）。*/
void ce_score_bridge_geom(int32_t y_q16, int32_t poc_q16);
/* ★★ 2026-09-18 补：**各作曲线吃谁的 y 不一样**（一手：th06/th07/th08/th10 判**道具 y**、
 * th12/th13/th15/th18 判**自机 y**）⇒ 这个三参版本同时带上两个 y（都是 Q16）。`coeff_0` 用自机 y
 * （引擎读的就是 `PLAYER+0x624`），族 10 / 08 用道具 y（`item+0xc2c`，`CE_ITEM_POS`）。
 * 两参版本保留 = `y_item := y_player`（旧调用 / 单测）。*/
void ce_score_bridge_geom2(int32_t y_player_q16, int32_t y_item_q16, int32_t poc_q16);
/* th10（110 风神录）那一族的曲线要 `F` = **显示信仰**：`coeff = 0.5 − d×(F−100000)/(6080·F)`（一手见
 * `engine/faith/th10/01-faith-and-points.md`）。卡每帧/每颗填一次；`F ≤ 0` ⇒ 比值回 1000（不归一）。*/
void ce_score_family10_faith(int32_t faith_display);
int  ce_score_coeff0_permille(void);              /* 本颗的 `coeff_0`（千分点）；无几何信息时回落到标记位 */
int  ce_score_ratio_permille(int family);         /* `coeff_i / coeff_0`（千分点）；`coeff_0 ≤ 0` ⇒ 1000（不归一）*/
/* 卡声明「我这一份 ΔG_i 属于哪个族」（粘性；`ce_layer_drop_card` 会清）。0 = 不声明（= NONE）。*/
void ce_score_family_declare(uint32_t id, int family);
int  ce_score_family_of(uint32_t id);

void ce_score_bridge(uint32_t raw_value, int difficulty);
/* ★★ `coeff_06`（收取高度系数）= **引擎自己的机制，本层刻意不重实现**（用户规格：「不要新建，
 * 它实际上是已经在运作的机制本身」）。一手（`th18:0x446bf9`–`0x446cf3`，**落笔点就在这段的开头**）：
 *   · 判据 = `player+0x47998`（回收线高度）与 **`player+0x624`（自机的 y）** 比较，
 *     **线上**（或道具 `state == 3` 真空飞行）⇒ 系数 = 1.0（`0x446cb4` 那一支）；
 *   · 线下 ⇒ 按到回收线的距离做一次折扣（`0x446c17`–`0x446cf3` 的整数骨架；🟡 形状见下）；
 *   · 系数乘在**身价**上（`0x446cfe` 之后的 `mul esi` / `SCORE(0x4cccfc) += …`）。
 * ⇒ 因为落笔点搬到了这段**之前**（写 `ecx`），它现在作用在**整笔 `G_0 + ΣG_i`** 上，
 *   **对所有声明者一视同仁**：我们既不该、也不需要自己再乘一次（自己乘 = 双乘）。
 * ★ **2026-09-18 订正（一手，三作互证）**：那条折扣曲线的精确形状 = **线性**的
 *   `≈ 0.9 × (1 − d/450)`，`d` = 自机 y − 回收线，单位 px；`0.9` 来自 `9F/10` 那一项，
 *   除数来自 `0x446c5c` 的魔数 **`0x91a2b3c5` = `ceil(2^40/450)`**（逐位相等；`imul` + `add edx,ecx`
 *   + `sar edx,8` 就是有符号 **÷450**）。旧稿写的 `163.18` 是把「负魔数补正」误读成「分子 +2^32」，
 *   `d ≥ 450` 时系数为负、被引擎顶到下限（**+1 分**）。
 *   TH14 / TH16 / TH17 的同类例程用同一个魔数 = 同一个 ÷450（系数 0.75 那种），是旁证。
 *   一手全链：`engine/faith/th18/01-point-item-value.md` §6、`engine/faith/th14/01-point-item-value.md` §5。
 *   要读当下这一颗的系数（诊断 / HUD）就用下面这个值 —— 由 sdk.c 从站点现场判「线上 / 线下」写。*/
void ce_score_set_coeff(uint32_t permille);
int  ce_score_coeff_permille(void);

/* ---- 加数通道（粘性：设一次管到改口为止；`ce_layer_drop_card()` 会清）---- */
/* 未耦合加数（点）：**原样进 G**（107 的「最大得点」走这条）。0 = 这张卡不参与。*/
void ce_score_declare_delta(uint32_t id, int32_t points);
/* 耦合加数（点）：进 G 之前会被 `ce_score_coupling_permille()` 缩放（110/111 的信仰走这条）。*/
void ce_score_declare_coupled(uint32_t id, int32_t points);

/* ---- 耦合通道（用户规格：只有这一项带乘数）---- */
/* 擦弹倍率（千分点）= min(⌊graze/100⌋, 899) × 10（th11 一手整数阶梯）。graze 为负按 0 夹。*/
int  ce_score_graze_mult(uint32_t graze);
/* 111 的「信号强度」（千分点，0..1000；`ce_score_mul_declare` 声明，粘性）。*/
void ce_score_mul_declare(uint32_t id, int permille);
int  ce_score_signal_strength(void);        /* Σ 各卡声明的信号强度（千分点）*/
/* ★ 合成用的耦合系数 = 擦弹倍率 + 信号强度；**两边都没有贡献时 = 1000**（×1.0），
 * 所以未耦合族的卡不会因为「没人声明信号」而被缩到 0。*/
int  ce_score_coupling_permille(void);

/* ============================ ★★ 共享 PIV 池 + 擦弹族（用户 2026-09-18 定案）============================
 * 口径（逐条有出处；推导与跨作证据见 `engine/faith/OVERVIEW.md` 的
 * 「结论：th12–th17 可以共用一个 PIV」与「跨作『擦弹 ⇒ ？』全表」两节）：
 *
 *   · **池里装的是「额外」最大得点**（点），**初值 0** —— 只有加进去的才算「额外」
 *     （用户逐字：「初值是 0，因为只有加进去的才是『额外』最大得点」）。
 *   · **th12–th17 六作的 `CURRENT_PIV` 是同一个量** ⇒ 实现上**共用一块池**（不必六套）。
 *   · ★ **写入闸在「写」这一侧**（用户 2026-09-18：「**大家都往 PIV 里面写相当于自动相加了**，
 *     只是写的时候**自己这边有一道最大写入的闸**」）：每个写入者有自己的**可写上限 = `cap_i − init_i`**，
 *     它累计写满就再也写不进去；池本身**不做读值钳位**。
 *     ⇒ 同一件事的另一种说法（用户举例）：th13 EASY 初值 5000、上限 100000
 *       ⇒「th13 路径就只能最多往里面写入 `100000 − 5000` 点 PIV」= **95000**。
 *   · ★ **池只有一份、自动相加**：多卡同装 = 多一个**来源**涨（用户：「多加了一套系统」），
 *     不是把池数 N 遍 ⇒ 族项 = **池的当前值**（`ce_piv_family_points()`）。
 *   · 六作的 `coeff_i` 恰好完全相同（0.75 族，锚与 `d` 已统一 ⇒ 相对 `coeff_0` 恒为 5/6）
 *     —— 所以「各报一份」与「先求和再乘」在这里等价（那一步在计分层换版时落地）。
 *
 * **擦弹 ⇒ 最大得点**是这一族里**唯一**「按状态算、不按卡算」的东西（用户口径：**别加一张卡擦弹数的作用就 +1 次**）：
 * 一手四作（th12–th17）同形「每 +1 擦弹 ⇒ 最大得点 +1 显示」，所以本层把它做成**一个**写入者
 * （`ce_graze_points()`，写进同一块池），只在族内有卡在场时非 0；`graze` 由 `sdk.c` 在落笔点现场从 `0x4ccd0c`
 * 读进来（普通内存读，**不新增站点**）。*/
void ce_piv_reset(void);                                  /* 新局 / run reset：池归 0（在场声明保留）*/
int  ce_piv_claim(uint32_t id, int32_t writable_cap);      /* 声明在场 + 本卡可写上限（= cap_i − init_i，点）*/
/* ★ 便利入口：112–117（th12–th17）的 `cap_i − init_i` 已按各自难度表内置（一手出处逐条在实现里），
 *   卡侧只要 `ce_piv_claim_auto(ce_card_id(c), ce_difficulty())` 一行。
 *   返回 1 = 认出了这张卡并 claim 成功；0 = 不在表里（id 不是 112–117 / 难度越界）。*/
int  ce_piv_claim_auto(uint32_t id, int difficulty);
int32_t ce_piv_cap_for(uint32_t id, int difficulty);       /* 查表：本卡该难度的可写上限（-1 = 查不到）*/
void ce_piv_release(uint32_t id);                          /* 退场（`ce_layer_drop_card` 自动调）*/
void ce_piv_add(uint32_t id, int32_t points);              /* 往池里加（**加值不夹**）；未 claim 的 id 不加*/
int32_t ce_piv_pool(void);                                 /* 池的当前值（额外量，点）*/
int32_t ce_piv_reading(uint32_t id);                       /* 本卡读数 = 池的值（一份；不在场 ⇒ 0）*/
int32_t ce_piv_written(uint32_t id);                       /* 本卡已写进池的量（≤ 它的可写上限）*/
int32_t ce_piv_room(uint32_t id);                          /* 本卡还剩多少可写（= cap_i − 已写）*/
int32_t ce_piv_family_points(void);                        /* ★ 族项 = **池的当前值**（一份、自动相加）*/
int  ce_piv_present_count(void);
int32_t ce_piv_cap_of(uint32_t id);                        /* 诊断：本卡的可写上限（< 0 = 不在场）*/

void ce_graze_bridge(uint32_t graze_total);                /* sdk.c 在落笔点读一次 `0x4ccd0c` 塞进来 */
int  ce_graze_family_claim(uint32_t id);                   /* 112/113/114/115 声明「我是这一族」*/
void ce_graze_family_release(uint32_t id);
int  ce_graze_family_present(void);
int32_t ce_graze_points(void);                             /* = 本局擦弹 × 1（点）；族内无卡 = 0（**只算一次**）*/
uint32_t ce_graze_total(void);                             /* 诊断：桥进来的擦弹总数 */

/* ---- 读值 ---- */
/* 原生基础值 G_0（**点**）= clamp(桥值, 身价下限, 按难度身价上限) / 10
 *   ⚠ 钳在**身价**上、除在最后（2026-09-13 订正：钳在「点」上会严 10 倍）。
 *   引擎在站点之前已经钳过同一个界（钳 `ecx`）⇒ 现场这次钳**恒等**，`G_0 ≡ 桥值/10 ≡ ecx/1000`。*/
int32_t ce_score_base_points(void);
/* 最终 G（点）= G_0 + Σ未耦合 + coupling × Σ耦合 / 1000（**明确舍弃的东西：不做归一化**）。*/
int32_t ce_score_total(void);
/* 「引擎单位」的读法（= G × 10 = 身价 = 弹窗值）。★ 2026-09-13 起**不再是落笔路径**：
 * 落笔是 `sdk.c` 在站点 `0x446bf9` 写 `ecx = 1000 × ce_score_total()`；本函数留给单测与旧的读法。*/
int32_t ce_score_apply(void);
/* 声明加数的卡有几张（两族相加）；**0 = 没有卡参与 ⇒ SDK 一个字都不写**（原生路径逐字节不变）。*/
int  ce_score_participants(void);
/* Σ 各卡声明的加数（**原始和**，点；两族都算进来，不乘耦合）—— 诊断 / 日志 / 旧断言用。*/
int32_t ce_score_aggregate(void);
/* 落笔者 id（声明者里最小的那张）—— 只用于日志 / HUD 归属，**不再决定谁写值**。*/
int  ce_score_applier(void);

/* ---- 旧接口（legacy：单卡逐字节等价；多卡时求和，不再互相吞）----
 * `ce_score_declare(id, v)` 的 `v` 仍是**旧语义**（该卡自己的「得分值」，非增量），
 * 层把它按 `max(v, 10)` 折成加数（= 老落笔公式 `*value = max(v,10)*10` 的 G 那一半）。*/
void ce_score_declare(uint32_t id, int32_t value);   /* 兼容入口（= ce_score_declare_legacy）*/
void ce_score_declare_legacy(uint32_t id, int32_t value);
int  ce_score_legacy_count(void);                    /* 走了 legacy 通道的卡数 */
int32_t ce_score_legacy_sum(void);                   /* Σ legacy 值（点，已按 max(v,10) 折）*/
int  ce_score_mul_aggregate(void);                   /* = ce_score_signal_strength()（旧名）*/

/* ---- 「本帧已消过弹」闩锁 + 本帧消弹记录（EVENTS-CONTRACT.md §5.2 细则 2）----
 * 107 的结界灵击与 117 的霊撃都会全屏消弹；同一帧两者都触发时，弹只能被消一次（否则第二家扫到空池、
 * 或重复记账）。每帧初 reset，各卡 claim：第一个拿到的负责消弹，其余让位。
 *
 * ★★ 2026-09-13（原生卡冲突审计 G 通道）把「仲裁」与「事实」拆开 —— 别再用 owner 当「本帧消过弹」的判据：
 *   · **闩锁**（`ce_cancel_latch_*`）= 谁拿全屏消弹这一笔的**仲裁**（用户 2026-09-12 定案：只有一家做）。
 *   · **记录**（`ce_cancel_frame_*`）= 本帧**本层实际消掉了几颗**，由 SDK 的消弹 helper
 *     （`ce_cancel_radius` / `ce_cancel_all_bullets`）**自动**登记 —— 卡不必（也不能）自己记得调。
 *     ⇒ 原生的清弹路径（47 SUN 每帧全屏清、19/20/51/30 的 option 清…）**不在**这条记录里（我们看不到），
 *       所以正确读法是：**count == 0 才代表"真的一颗都没消"**；owner 不是你不代表没人消过弹，
 *       owner 是你也不代表真消到了弹（`ce_cancel_latch_release()` 就是为这一档准备的）。*/
void ce_cancel_latch_reset(void);
int  ce_cancel_latch_claim(uint32_t id);   /* 本帧第一个认领的返回 1（并记住 id）；已被别人认领返回 0 */
int  ce_cancel_latch_owner(void);          /* 本帧认领者 id；没人 = -1 */
int  ce_cancel_latch_release(uint32_t id); /* ★ 认领者发现一颗都没消（n <= 0）时交回闩锁；交回成功返回 1 */
void ce_cancel_frame_note(int n);          /* 记录一次消弹（n = 颗数；< 0 = 数不出来）。helper 内自动调 */
int  ce_cancel_frame_count(void);          /* 本帧本层消掉的弹数合计（未知颗数的不计进来）*/
int  ce_cancel_frame_seen(void);           /* 本帧本层有没有发生过消弹（含数不出颗数的）*/
int  ce_cancel_frame_unknown(void);        /* 本帧有没有「数不出颗数」的消弹（诊断用）*/

/* ---- 「这次死亡由谁救」闩锁（EVENTS-CONTRACT.md §5.2；用户 2026-09-12 定案）----
 * 救援层顺序被钉死为 **107 → 117 → 决死 B → 113**。113 的语义是**延时**（取消死亡 → 进灵界 →
 * 出灵界再死一次），所以它**必须知道前面三家有没有人已经救过**，否则会出现
 * 「107/117 花钱救了你、113 的窗口结束时又把你杀掉」。
 * 每帧初 reset（与 ce_cancel_latch_reset 同一处：`ce_item_frame` 断点），各救援者 claim 报到：
 *   107（`cards/senro.c`）/ 117（`cards/animal.c`）**无条件** claim，且**必须丢弃返回值** ——
 *   用户 2026-09-12 重申「**107 和 117 同时触发的时候，得要 107 和 117 同时破除**」
 *   ⇒ 这两家谁也不能靠「先 claim 到账」把另一家挡掉（把返回值当门 = 让先到的独占救援 = 违规）；
 *   113（`cards/trance.c`）**查 owner**：没人认领才轮到它动手（它是最后一家）。
 * ⚠️ 为什么「每帧」够用：一次死亡横跨多帧（`Player__die` 置 state 4 之后还有决死窗口 / 死亡动画），
 *   同帧不可能发生两次死亡 ⇒ 每帧闩锁 == 每次死亡闩锁。*/
void ce_rescue_note_reset(void);
int  ce_rescue_note_claim(uint32_t id);    /* 记下「这次死亡由 id 救了」；第一个返回 1，已被别人记返回 0 */
int  ce_rescue_note_owner(void);           /* 这次死亡的救援者 id；没人 = -1 */

/* ---- ★★ 「这次死亡是**卡自己造的**」通报位（2026-09-16u；第一个消费者 = 111 的河童罩子）----
 * 语义（用户口径逐字）：「这个罩子原则上是**撞弹**的保护，所以**不应当能抵消中弹灵界的结束灵界击杀**」。
 * 背景：113（神灵庙）的「中弹灵界」是**延时**——中弹时由 113 救下、灵界窗口结束时**补一次死亡**
 * （`cards/trance.c` 的 `trance_player_die()` 直接调 `Player__die` `th18:0x45d3a0`）。
 * 那次补死会照常广播卡虚表 `+0x10`，于是「中弹瞬间」类钩子（107/117 的救援、111 的罩子）会看见它，
 * 但它是**卡造的死**、不是撞弹 ⇒ 罩子不该替玩家把它吃掉。
 * 用法（成对、同帧）：
 *   · 造死的卡：`ce_forced_death_begin()` → 调 `Player__die` → `ce_forced_death_end()`；
 *   · 救援层的卡：在 `+0x10` 里查 `ce_forced_death_active()`，非 0 ⇒ **不接这一下**。
 * ⚠ **不粘性、不跨帧**：它描述的是「这一次调用」，用完必须 `end()`（异常路径请在调用前后各写一次）。
 * ⚠ 引擎自己的三处撞弹调用点（`th18:0x45cd27` / `0x45ce81` / `0x45d060`）不经过这里
 *   ⇒ 真中弹一定读到 0。`ce_sdk_setup()` 会复位。*/
void ce_forced_death_begin(void);
void ce_forced_death_end(void);
int  ce_forced_death_active(void);
void ce_forced_death_reset(void);

/* ---- ★ 「中弹无伤」闸门声明层（`EVENTS-CONTRACT.md` §4.1 的第 5 层；2026-09-13）----
 * 一手依据（`engine/player/th18/03-player-hit-gate.md`）：
 *   TH13 的「灵界中弹无伤」= `Player::on_hit` **第一行**读灵界标志直接 return（`th13:0x444ca6`）
 *   —— **不扣命、不记 miss、不播中弹音、弹照样被吃掉**。
 *   TH18 在那个语义槽位上**是空的**（`Player__die` `th18:0x45d3a0` 入口没有任何前导判断），
 *   所以本仓在入口插了一个断点（`BP_ce_player_die`）：**闸门开着 ⇒ 跳过整个 die 体**。
 * 与 `ce_invuln_*` 的分工（两者**不互相替代**）：
 *   · `ce_invuln_*` 写的是无敌计时器 `player+0x47774` ⇒ 子弹**根本不判定**、不吞弹；
 *   · 本层跳过 `Player__die` ⇒ 判定照跑、**弹被吃掉**（= 一手 TH13 的行为）。
 * **粘性声明**（照 `ce_score_declare`，不是照每帧闩锁）：断点可能在任何一帧的中途落下，
 *   而声明写在卡 tick 里 ⇒ 必须"声明一次管到改口为止"，否则两个 tick 之间的那一枪会穿过去。
 * 仲裁：**任一声明即生效**（`ce_no_hit_active()`）；owner = id 最小者（诊断用）。
 * ⚠ 卡被删时必须显式声明 0（`on_delete` 那一路；SDK 也会在 `ce_sdk_setup` 里 reset 一次）。*/
void ce_no_hit_declare(uint32_t id, int on);   /* 粘性：1 = 本卡要求「中弹不算数」，0 = 撤回 */
int  ce_no_hit_active(void);                   /* 断点 handler 读它：非 0 ⇒ 跳过 Player__die 体 */
int  ce_no_hit_owner(void);                    /* 声明者 id（最小）；无人 = -1 */
void ce_no_hit_reset(void);                    /* 全清（SDK 初始化用）*/

/* ---- ★★ 2026-09-16za **「未满 P」声明层**（卡 106 红魔乡的两条新设计共用一个来源）----
 * 用户口径（逐字）：「红魔乡卡再加一条设计：**上线收点当且仅当满P**」
 *               「**协同效应：除非满P，不加信号强度**」。
 * 一手（TH06 反编译 `ItemManager.cpp` 的道具 tick，本机 `/mnt/d/项目/幻想指南/18/th06/src/`）：
 *     `if (curItem->state == 1 || (128 <= g_GameManager.currentPower && g_Player.positionCenter.y < 128.0f))`
 *     → 进吸引态（`state = 1`，速度 `sincosmul(…, 8.0f)`）
 *   ⇒ 一手意义：**「满 P（128 = TH06 的满火力）+ 自机在回收线（y = 128.0）以上」是「过线收点」的充要条件**
 *     （`state == 1` 那条是「已经在吸引态」= 结果，不是入口）。TH06 里**没有**任何「不满 P 也收」的路径。
 * 声明者 = **106**：每帧声明 `power < MAX_POWER`（`CE_ADDR_MAX_POWER` 0x4ccd3c）。
 * 消费者 = 两处，各不相同（用户选的口径逐条见下）：
 *   ① **过线收点**：`BP_ce_poc_gate_a/b`（`th18:0x445bb9` / `th18:0x446003`，两条 `ja 0x445f63`
 *      = `ItemManager__on_tick__body` 的「过线 ⇒ 道具无条件真空」）⇒ 声明开着时那两条 `ja` **不执行**
 *      = 道具照常下落（= TH06 里不满 P 时过线什么都不发生）✅ 精确门，**回收线本身照旧可见**。
 *   ② **信号强度**：111 的信号推进（`faith_signal_step_gated(..., allow_line_topup)`）——用户口径
 *      「**只停「线上补满」那一条**」：不满 P 时**不给**「在线上未满 ⇒ 补满并钳到 100.00」那一支
 *      （擦弹独立通道 / 已满档的小额加值 / 线下衰减 **都不受影响**）。
 * 粘性（照 `ce_no_hit_declare`）：断点可能落在任意帧，而声明写在卡 tick 里 ⇒ 声明一次管到改口为止。
 * 仲裁：**任一声明即生效**（`ce_full_p_gate_active()`）；owner = id 最小者（诊断用）。
 * ⚠ 卡被删时必须显式声明 0（`on_delete` 那一路；SDK 的 `ce_layer_drop_card()` 也会撤，见 `sdk.h`）。*/
void ce_full_p_gate_declare(uint32_t id, int on);   /* 粘性：1 = 「现在不是满 P」 */
int  ce_full_p_gate_active(void);                   /* 两个断点 handler / 111 的信号推进读它 */
int  ce_full_p_gate_owner(void);                    /* 声明者 id（最小）；无人 = -1 */
void ce_full_p_gate_reset(void);                    /* 全清（SDK 初始化用）*/

/* ---- ★★ 「不要让自机长出**原生**那几格子机」声明层（2026-09-16；本层为卡 111 的魔理沙侧新增）----
 * 一手：TH18 的自机子机（option）有**两个互不相干的数组**（`engine.h` 的 `CE_NATIVE_OPT_*` 一节）：
 *   · **原生子机数组** `inner+0x50`（4 格，stride 0xf0）—— 引擎按**火力档**重建
 *     （`Player__repopulate_options_and_notify_cards` `0x45d5e0`：`lvl = CURRENT_POWER/100` ⇒ 0..4 格），
 *     VM 贴图脚本按机体查表（`0x4b7020` 普通形 / `0x4b7030` 满火力形）；
 *   · **卡申请的子机池** `inner+0x410`（12 格）—— `Player__allocate_option` `0x40a790` 管，数量由卡声明。
 * 本层管的是**前者**（后者由 `ce_optpose_*` 选举层管）。声明「隐藏」时 SDK 做两件事：
 *   ① 断点 `BP_ce_repopulate_native_option`（站点 `0x45d610`，`repopulate` 算完 `lvl` 那条 mov 上）：
 *      把本次的 `lvl` 压成 0 + 把该数组里两支 VM（`+0xb0`/`+0xb4`，4 格共 8 支）interrupt 掉
 *      ⇒ **原生子机不建、已建的立刻收**（画面与「满火力形」一起消失；`lvl == 0` 也让引擎走它自己的
 *      「清空 4 格」那条路）；
 *   ② 断点 `BP_ce_native_option_muzzle`（站点 `0x45e930`，`Player__shoot_one_bullet` 入口）：
 *      膛口指针落在**原生数组区间** `[PLAYER+0x670, PLAYER+0xa30)` 的那一发**整发跳过**
 *      （主炮行循环是按行号取组的，`lvl == 0` 压不住它；不跳过就会出现「子弹从看不见的子机位置飞出来」）。
 * ⚠ **机体门在层里**：声明要带一个机体号，只有「当前机体 == 那个机体」时②才拦、①才压
 *   （换机体后**立刻**恢复原生行为，不依赖卡再跑一次 tick —— 换机体发生在商店里，那一刻卡 tick 冻着）。
 * ⚠ **粘性**（照 `ce_no_hit_declare`）：声明 1 = 生效到改口为止。理由与它同：`repopulate` 可能落在
 *   两帧之间的任意时刻，而声明写在卡 tick 里。删卡/退场时由 `ce_layer_drop_card()` 撤（见 `sdk.h`）。*/
void ce_native_option_hide_declare(uint32_t id, int on, int character);  /* 粘性；character = 生效的机体号 */
/* ★ 2026-09-23：**隐藏但请求保留记录**（108 用）。有卡这么声明时，构建断点不动手（记录/坐标照建），
 *   显示由该卡自己的 `ce_native_option_sweep_keep_state()` 每帧收掉。 */
void ce_native_option_keep_records(uint32_t id, int on);
int  ce_native_option_records_wanted(void);
int  ce_native_option_hidden(int character);   /* 断点 handler 读它：非 0 ⇒ 隐藏（character = 当前机体）*/
int  ce_native_option_hide_owner(void);        /* 声明者 id（最小）；无人 = -1 */
void ce_native_option_hide_reset(void);        /* 全清（SDK 初始化用）*/

/* ---- ★ 「这次死亡有没有被取消」结果位（2026-09-13；由**另一个会话**在 sdk_core.c 里实装，
 *      本块是给它补的**声明**（它的 .c 引用了 `CE_DEATH_RESULT_UNKNOWN` 而头里没有 ⇒ 全仓编不过）。
 *      语义按 .c 的用法：`-1` 未知 / `0` 照常死 / `1` 被取消；`_seq` 每次 note 自增（跨越一次死亡用）。
 *      ⚠ 归属那一侧（谁调 note、什么时候 reset）以那边的会话为准；这里只补最小声明，不改行为。
 *
 *      ★ 2026-09-13 补一手依据（BP30-IMPLEMENTATION.md §2 / CARD-NATIVE-CONFLICT-AUDIT.md §A-2）：
 *      引擎在 `Player__on_tick__body` 的 case 4 里把五家救援者（卡虚表 `+0x0c`）**全跑完**才判一次 ——
 *      `th18:0x45c112 xor edi,edi`（acc=0）→ `0x45c116 push edi` → `0x45c11c call [eax+0xc]` →
 *      `0x45c121 or edi,eax` → 循环 → **`0x45c127 test edi,edi`** → `0x45c12d jne 0x45c3d1`（取消死亡）
 *      否则 `0x45c135 call 0x45d090`（扣命）。**新断点 `BP_ce_death_result` 就落在这条 `0x45c127` 上**
 *      （6 字节 `85 ff 8b 7c 24 14`），handler 读 `EDI = acc` 后 `note(acc != 0)`；
 *      复位点是**已有**的 `BP_ce_player_die`（`0x45d3a0` 入口 = 一次死亡开始）与 `ce_sdk_setup`。
 *      ⇒ 读到 `-1` = 这次死亡还没判（**别当成 0 用**）；读到 `0` = 会/已经扣命；读出 `1` = 已被取消。---- */
enum { CE_DEATH_RESULT_UNKNOWN = -1 };
void     ce_death_result_reset(void);
void     ce_death_result_note(int cancelled);
int      ce_death_result(void);
int      ce_death_cancelled(void);
unsigned ce_death_result_seq(void);

/* ---- ★ SDK 帧号（2026-09-13 加；跨卡互作审计发现 #3 的修法）----
 * 由**每帧一次**的帧点 `BP_ce_item_frame`（`sdk.c` 的 handler，站点 `th18:0x446ee3` = ItemManager tick 门卫）递增。
 * 用途：**共用模块的「每帧只推进一次」守卫** —— 例 `faith_pool_tick()` 被 110 与 111 各调一次（共用池），
 * 没有守卫时同一帧推进两次（闪白不可收期 32 帧变 16、下落/磁吸翻倍、吸引与收集判定各多跑一遍）。
 * ⚠ 只有帧点会 bump ⇒ 别在任何卡回调里手动 bump。*/
void     ce_sdk_frame_bump(void);              /* 帧点专用（`sdk.c` 的 `BP_ce_item_frame` 里调）*/
uint32_t ce_sdk_frame(void);                   /* 只读当前帧号 */

/* ★★ 2026-09-13 用户要的调试件入口：**逐帧合成方向输入**（调试键 = 空格）。
 * ⚠ 必须在**卡 tick（AbilityManager 节点 0x16）**里调 —— 见 `DEV-INPUT-SYNTH.md`「落点」一节：
 *   ItemManager 节点（`ce_item_frame` 那个断点 = 0x1e）在 **Player（0x17）之后**，
 *   在那里写 INPUT_HELD 玩家已经读过了（实跑症状：日志有 START/STOP 但 dx 恒 0）。
 * 定义在 `cards/input_synth.c`；`CE_INPUT_SYNTH_ENABLED = 0` 时是空壳。
 * 同帧只跑一次（内部用 `ce_sdk_frame()` 去重，多张卡各调一次也只推进一格）。*/
void     ce_input_synth_tick(void);
/* 同上的**绘制**半边：在 `on_draw`（虚表 `+0x48`）里画一行状态字（生效时才画）。
 * ⚠ `ce_hud_text` 只能在 `on_draw` 里调（SDK 契约）；同样用 `ce_sdk_frame()` 同帧去重。*/
void     ce_input_synth_draw(void);

/* ---- 低速子机声明层（EVENTS-CONTRACT.md §1「选举覆盖」；108 spec §3.3.1）----
 * 108（妖机体）与 111（SA 子机层）都要定义「低速时子机长什么样」，而最终落在同一批 zPlayerOption
 * （原生聚焦偏移 +0x6c/+0x70、非聚焦 +0x64/+0x68、贴图脚本、数量）⇒ 各自写 = 谁后写谁赢。
 * 规则：**各卡只声明，SDK 选举 id 最小者落笔**（与伤害层/计分层同一套选举）。
 * 纯逻辑、无引擎依赖（真正写引擎字段由被选举的卡在 `on_option_pose_apply` 里做，分派见 sdk.c）。
 *
 * ★★ 2026-09-12 **逐格偏移**（spec §12.4 点名的形状缺口；一手依据在下面 `CE_OPTION_POSE_CELLS`）：
 *   声明的是**每一格**自己的偏移，不是一套偏移给所有格 —— 否则 `count >= 2` 会全部叠在同一个点上。
 *   引擎侧为什么必须逐格、且为什么「不写偏移、留给原生阵型」这条路走不通（一手）：
 *     · 原生阵型（`sht[0x40/0x90 + (TRI[档−1]+i)*8]`）由 `Player__repopulate_options` `0x45d5e0`
 *       写进**原生子机数组** `inner+0x50`（4 格，`th18:0x45b8ea push 4` + `0x45d81e mov edi,4`，
 *       逐格写下标见 `th18:0x45d8fc`）；
 *     · 卡申请的装备卡子机在**另一个数组** `inner+0x410`（12 格，`Player__allocate_option` `0x40a790`
 *       扫的就是它，`th18:0x40a7a4 add edi,0xa30`）；
 *     · 两个数组共用一个**每帧**的跟随更新 `FUN_0045bc90`（`th18:0x45b8ea`/`0x45b8fa` 两处调用：
 *       原生 count=4、卡池 count=12），它对每一格算
 *       `pos = player_pos + (focused ? [+0x6c/+0x70] : [+0x64/+0x68])`（`th18:0x45bce8`–`0x45bd08`）
 *       并写回该格的 `+0x5c/+0x60`（出膛点）与两个 VM 的位置。
 *   ⇒ **原生阵型永远不会落到卡池的格子上** ⇒ (b)「不写偏移、留给引擎」对卡申请的格子**不成立**，
 *     要 `count >= 2` 不叠就必须逐格给偏移（(a)）。
 *   单位 = **像素**（引擎自己在 `0x45d902` 把 sht 的 float 乘 128 变成定点写进 `+0x64`；
 *   `0x4b9310` = 128.0f 一手）⇒ 卡侧落笔时 ×128 取整即可。*/

/* 逐格上限 = **原生上限 4**（一手：原生数组 4 格 `th18:0x45b8ea push 0x4` / `0x45d81e mov edi,0x4`；
 * 卡池 12 格 `th18:0x45b8fa push 0xc`）。要支持更多格的卡（如 111 将来「携带子机上限 8」）
 * 就是把这个常量改大一行 —— 存储代价 = 256 × (16·N + 8) 字节（这张表是静态的）。*/
#define CE_OPTION_POSE_CELLS 4

typedef struct {
    float fx, fy;       /* 聚焦（低速）偏移 —— 对应原生 option +0x6c/+0x70，单位 = 像素 */
    float ux, uy;       /* 非聚焦偏移 —— 对应 +0x64/+0x68；卡只吃低速时给同值即可 */
} ce_option_cell_t;

typedef struct {
    int   count;        /* 想要几个低速子机（<= 0 = 这张卡本帧不声明；> CELLS ⇒ 钳到 CELLS）*/
    int   anm_script;   /* 子机贴图脚本号（= ce_option_alloc 第 4 参）*/
    ce_option_cell_t cell[CE_OPTION_POSE_CELLS];   /* 逐格偏移；只有 [0, count) 有效 */
} ce_option_pose_t;

void ce_optpose_reset(void);          /* 全清（含落笔权交接记账）：setup / 单测用 */
/* ★ **每帧**声明（`BP_ce_item_frame` 每帧清集合 ⇒ 不声明 = 本帧退出）：
 *   p == NULL 或 p->count <= 0 = 退出声明；返回真正记下的格数（clamp 后）。*/
int  ce_optpose_declare(uint32_t id, const ce_option_pose_t *p);
/* 删卡路径 = 立刻退出声明（不等下一帧的清集合）。*/
void ce_optpose_drop(uint32_t id);
/* 帧初清声明集合（SDK 在 `BP_ce_item_frame` 里与 `ce_cancel_latch_reset()` 同处调）。*/
void ce_optpose_frame_reset(void);
int  ce_optpose_winner(void);                       /* 落笔者 id（最小）；无人声明 = -1 */
const ce_option_pose_t *ce_optpose_pose(void);      /* 落笔者的声明；无人 = NULL */
/* ★ 落笔权交接（**每帧落笔点调一次**）：
 *   · 返回本帧落笔者 id（-1 = 无人声明）；
 *   · `*out_prev` = **上一帧的落笔者、而这一帧没赢**的那张卡（-1 = 没有）——
 *     SDK 要拿它去调一次对方的 `on_option_pose_apply`（**退位通知**）：那张卡此刻
 *     `ce_optpose_winner() != 自己` ⇒ 只许把子机收干净，不许写字段。
 *   没有这条通知，「松开 Shift 的 108」会把自己的子机永远留在池里（它不再被选举 ⇒ 没人再调它）。*/
int  ce_optpose_frame(int *out_prev);

/* ---- HUD 槽位分配器（HUD-LAYOUT.md §3/§4；110/111/113 + 58–80 的 5 张卡都要）----
 * 目的：多张卡都要占「左下角」时不再各自硬编码坐标而重叠。规则（§4.4）：
 *   · 认领集合**按 card_id 排序** ⇒ 同样的卡组永远得到同样的排布（replay / 读档安全）；
 *   · 认领**幂等**（on_load 每关都会再调，重复认领返回同一个槽）；
 *   · 局中删卡**不当场重排**（§4.4：避免整摞 UI 跳动）⇒ release 只标记「保留」，
 *     真正的清空发生在 `ce_hud_slots_reset()`（关卡开场调）。
 * 纯逻辑、无引擎依赖；坐标单位 = **表面 px**（调用方要 640 空间的点自己乘系数，见 sdk.h 的包装）。*/
typedef struct {
    float w, h;      /* 声明块尺寸（表面 px）*/
    int   side;      /* 0 = 左下（默认）／1 = 右下 */
} ce_hud_block_t;

#define CE_HUD_SIDE_LEFT  0
#define CE_HUD_SIDE_RIGHT 1
#define CE_HUD_SIDE_PINNED 2   /* ★ **自带锚点**：用户按截图量过的固定位置（117）。登记进表、
                                *   但**不参与货架排布**（`x/y` 由认领时给的锚点决定，不随别人重排）。
                                *   HUD-LAYOUT §5 早列为待办；2026-09-12 用户裁决「117 现在的位置
                                *   （左下角、整排左移 193 px 后）是正确的」⇒ 用这一支保位置。*/

typedef struct {
    float x, y;          /* 块**左下角**（表面 px）：y 是块的**下边缘**（TH 坐标 y 向下增长）*/
    float x640, y640;    /* 同一个点，640×480 空间 —— 直接喂 ce_hud_vm_text_* / ce_hud_number_* */
    float w, h;          /* 回显声明值，卡自己按它算内部布局 */
    int   side;
} ce_hud_slot_t;

/* 底部货架的常量（表面 px，HUD-LAYOUT.md §2）。117 动物灵 / 116 季节槽实测推出的同一条线：
 * 左锚点 257、底边 924；行内间隙 8；左列软上限 1000（§6.3 🟡 待实跑标定），
 * 硬上限 1040 来自 §4.2（HUD 文字永远画在图元之前 ⇒ 不许压到卡组图标行）。*/
#define CE_HUD_ANCHOR_X        257.0f
#define CE_HUD_ANCHOR_X_RIGHT 1040.0f   /* ❓ 右侧镜像锚点（§6.2 无先例卡验证过）*/
#define CE_HUD_ANCHOR_Y        924.0f   /* 行底边 */
#define CE_HUD_GAP               8.0f   /* 行内/行间间隙 */
#define CE_HUD_X_MAX          1000.0f   /* 左列软上限（排不下就上抬一行）*/
#define CE_HUD_X_HARD_MAX     1040.0f   /* 硬约束：块右边缘不许越过（§4.2）*/
#define CE_HUD_X_MIN           257.0f   /* 右列向左排的软下限 */

/* 旧名（兼容）：基底 = 锚点 */
#define CE_HUD_BASE_LEFT_X   CE_HUD_ANCHOR_X
#define CE_HUD_BASE_RIGHT_X  CE_HUD_ANCHOR_X_RIGHT
#define CE_HUD_BASE_BOTTOM_Y CE_HUD_ANCHOR_Y
#define CE_HUD_STACK_PAD     CE_HUD_GAP

/* 表面 px → 640×480 字面量空间（引擎内部再 × `DAT_0056aca0`；TH18 表面 1280×960 ⇒ ÷2）。
 * 🟡 待实跑确认一次；纯数学常数，故放这里（sdk_core 层不依赖引擎头，见 sdk.h 的同名别名）。*/
#define CE_HUD_640_SCALE      0.5f

/* 排布算法（HUD-LAYOUT.md §2，底部货架）：认领集合按 card_id **升序**，同一侧各自算 ——
 * 左列从左锚点沿底边向右排（`x + w > CE_HUD_X_MAX` ⇒ 整体上抬一行，行高 = 该行最高块）；
 * 右列从右锚点向左排。⇒ 完全确定性：同样的卡组永远同样的排布（replay / 读档安全）。*/

void ce_hud_slots_reset(void);                                  /* 清空全部认领与保留（测试 / 手动用）*/
/* ★ 关卡纪元：**每关开场重排**（HUD-LAYOUT.md §4.4 的「下关开场重算」）。
 * 由 `sdk.h` 的 `ce_hud_slot_claim_card()` 在**每次认领前**把 `CE_CURRENT_STAGE()` 推进来：
 * 关卡号变了 ⇒ 先 `ce_hud_slots_reset()` 再认领 ⇒ 上一关被删卡留下的占位自然收敛，
 * 而**同一关内**（局中删卡）一笔都不重排（§4.4：避免整摞 UI 突然跳动）。
 * ⚠ 不能拿 `on_run_reset`（`+0x4c`）当关卡信号：它由 `recount 0x4080e0` 调。
 * ★ 2026-09-27 订正（一手反汇编 + 实跑日志）：`recount 0x4080e0` 的**唯一**调用点是 `0x4433e2`
 *   （`GameThread__teardown_and_recount_cards` 里 gamemode **10/11** 那一支）⇒ 它跑在
 *   **「重开本局」那一刻**，不是「局中删卡」；局中删卡走 `+0x50`（`operator_delete`）。
 *   本注释旧稿写的「局中删卡后也会跑」与一手不符（实跑 162k 行日志里 `+0x4c` 只出现 1 次）。*/
void ce_hud_stage_begin(int stage);
int  ce_hud_stage_current(void);                                /* 记住的关卡号（-1 = 还没认领过）*/
/* 认领（幂等）。成功返回 1 并填 *out；卡 id 非法 / blk 非法返回 0。
 * `blk->side == CE_HUD_SIDE_PINNED` 时本函数返回 0（要用下面那个带锚点的入口）。*/
int  ce_hud_slot_claim(uint32_t card_id, const ce_hud_block_t *blk, ce_hud_slot_t *out);
/* ★ **自带锚点认领**（`blk->side == CE_HUD_SIDE_PINNED`）：按用户按截图量过的固定位置登记。
 *   `anchor_x/anchor_y` = 块的**左下角**（表面 px，y 是块的下边缘）。登记后 `ce_hud_slot_get()` 取得到、
 *   关卡纪元照样会清（下一关要重新认领），但**货架不会去动它**（别的块也不会为它让位 ⇒ 它自己负责不压别人）。
 *   ⚠ `blk->side` 必须 == `CE_HUD_SIDE_PINNED`，否则返回 0（反之 `ce_hud_slot_claim()` 也拒 pinned）。*/
int  ce_hud_slot_claim_pinned(uint32_t card_id, const ce_hud_block_t *blk,
                              float anchor_x, float anchor_y, ce_hud_slot_t *out);
/* ⚠ 认领时填给 `out` 的坐标是**那一刻**的；同一开场批次里后来的认领会按 id 顺序重排
 * ⇒ 卡片应在开场批次结束后（第一帧 tick）用本函数取最新值，或每帧取一次（开销可忽略）。*/
int  ce_hud_slot_get(uint32_t card_id, ce_hud_slot_t *out);
void ce_hud_slot_release(uint32_t card_id);                     /* 释放：标记保留（不当场重排）*/
int  ce_hud_slot_claimed(uint32_t card_id);                     /* 本关是否已认领（含保留）*/

/* 「玩家贴近左下角 → 整摞淡出」+ 迟滞（TH16 `th16:0x427f41/0x427f6c` 同款栅格）。
 * 栅格：x < −64 && y > 400 淡出；y < 384 || x > −64 淡回（迟滞状态内部保持）。
 * ★ **每帧只推进一次**：由 SDK 在 `BP_ce_item_frame` 里用自机坐标调 `ce_hud_near_corner_update()`；
 *   卡片**只读** `ce_hud_near_corner()`，别自己在 tick 里各调一遍（多张卡同帧各调 = 状态被反复推进，
 *   甚至一帧内先淡出又淡回）。`ce_hud_player_near_corner(x,y)` 是「更新 + 读取」的兼容入口。*/
void ce_hud_near_corner_update(float player_x, float player_y);   /* 推进迟滞（每帧一次）*/
int  ce_hud_near_corner(void);                                    /* 读当前状态：1 = 该淡出 */
int  ce_hud_player_near_corner(float player_x, float player_y);    /* 兼容：更新后返回状态 */

/* ---- 掉落层（EVENTS-CONTRACT.md §2，用户 2026-09-12 拍板：「加倍的意图是 ×2」）----
 * 引擎的 `zEnemyDrop`（`enemy+0x623c`）：`+0x00` main_drop（一颗，**在事件触发前就撒了**）、
 * `+0x04..+0x53` = ex_drop[20] 计数（type 1..0x13；引擎只撒前 19 格）、
 * `+0x54..+0xa3` = timed_ex_drop[20]（按出场计时折算的加成表，`n = ex[i] + timed[i]×(T−timer)/T`，
 * 见 `engine.h` 的 `CE_ENEMY_DROP_TIMED_*`）—— 两张表**都对最终颗数线性** ⇒ 乘数两张都要乘。
 * 规则：**加法来自各卡（照现状直接改 counts），乘法由层在最后一次性作用在「最终总数」上**：
 *     final[t] = ( base[t] + Σ add[t] ) × Π mult           （base[main] = ex[main] + 1，因为已撒一颗）
 *     写回 ex[t] = final[t] − (1 if t == main else 0)；timed[t] 只乘、不做「已撒一颗」补偿
 * ⇒ 「加倍」不再依赖卡链顺序，且**别的卡加的那颗也会被翻倍**（`(1+1)×2 = 4`，不是 `1×2+1 = 3`）。*/
#define CE_DROP_MULT_UNIT   1000
#define CE_DROP_TYPES       20      /* counts 格数（type 1..0x13 + 1 个空位）*/
#define CE_DROP_MAX_MAIN    19      /* 主掉落补偿只认到 type 19（引擎的循环只撒 1..19）*/
#define CE_DROP_TIMED_TYPES 20      /* +0x54 timed_ex_drop 的格数（与 ex_drop 同）*/

void ce_drop_reset(void);
/* 粘性乘数（千分比；1000 = ×1、0 = 不参与）。绑定时登记、opdelete 时清。*/
void ce_drop_mult_set(uint32_t id, int permille);
int  ce_drop_mult_aggregate(void);                 /* Π 各卡乘数；无贡献者 = CE_DROP_MULT_UNIT */
/* 引擎侧调：传 **zEnemyDrop 基址**（不是 counts！）—— z[0] = main_drop、z[1..20] = ex_drop、
 * z[21..40] = timed_ex_drop；就地写回「最终 − 已撒」（timed 表不做补偿）。*/
void ce_drop_apply(int32_t *z_enemy_drop);

/* ==================== ★★ 自建实体 → 114（辉针城）收点批：虚拟收集 / 虚拟在飞（2026-09-15）====================
 *
 * ## 要解决的问题（`NEXT.md` §1x.2，用户 2026-09-15 口径）
 *
 * 114 的批**只看引擎原生道具**，两个门都碰不到我们自建的实体：
 *   · **成员门** = `cards/collect2x.c` 的 `on_item_collect`：`type ∈ {1,2}`（P / 钱）**且**
 *     `item+CE_ITEM_STATE == 3`（真空飞行收，§Z22）才入批；
 *   · **续命门（presence）** = 同文件 `on_item_frame` 每帧扫**原生道具池**找 state 3 的道具
 *     （`ce_item_pool_any_attracting()`）→ `c2x_batch_presence()` 续 idle。
 * 110 / 111 的绿点是**自建 VM 池**（`cards/faith.c`，不入原生道具池，见 `faith.h` 文件头）⇒
 * 天然不计入、也续不了批 ⇒ 批会**偏小**，而且**可能在这些点还在飞的时候就提前结算**。
 *
 * ## 本层给的正是那两条通道（生产者只声明，消费者只读；卡之间**不互相调用**）
 *
 * | 通道 | 生产者调 | 消费者（114）调 | 语义 |
 * | --- | --- | --- | --- |
 * | **成员** | `ce_vitem_note_collect(kind, n)` | `ce_vitem_take_collect()` | 自建实体**被收的那一刻** +n ⇒ 批计数 +1 |
 * | **在飞** | `ce_vitem_declare_flying(src, kind, n)` | `ce_vitem_flying()` | 本帧**还有 n 个在飞** ⇒ presence 续命 |
 *
 * ★★ 2026-09-18（B11）：**本层只报「几颗」，不报「值多少」** —— 用户口径：「**绿点的面值不进
 *   `batch_score`，它加的不是分数**」（绿点加的是信仰，不是 SCORE）⇒ 生产者/消费者都不带面值
 *   （2026-09-18 曾加过一支 `value` 通道并当天回退：那会让倍率把信仰当分数放出来，语义错）。
 *   ⇒ 纯绿点批的结算补差恒 **0 是设计**，不是漏算；它出现在「死亡 / 全屏收点一次收掉几十颗」这类
 *   场景里本来是**另一条 bug**（真空门的死亡分支，见 `cards/faith.c` 的 `faith_vacuum_gate()`）。
 *
 * ★★ 2026-09-18（B1，用户规格「信仰点与 P 点 / 金钱**完全一致**：开批 / 计数 +1 / 飞行中组织批次结算」）：
 *   **生产者必须按 114 的同一谓词报** —— 即只报「**引擎道具 `item+0xc74 == 3`（真空飞行）**」的
 *   对应物（110/111 的绿点 = 本池 `state 3` = 过回收线 / 放 B / 对话 ⇒ 朝自机飞；对照表在
 *   `cards/faith_core.h` 的 `faith_state_t` 一节）。**不计**飘落 / 飞散（对应引擎 state 1 的坠落体）
 *   与 70px 环吸（引擎 state 4）—— 引擎对 P / 钱**正是这样**（坠落中的道具不计数也不续批，
 *   §Z22 的收窄），而「普通游玩顺手捡一颗」在引擎那边同样不开批。
 *   ⇒ 生产者的 `state` 语义若与引擎不同（例：116 季节点 / 117 动物灵），**由生产者自己映射**
 *   到「此刻是否等于引擎 state 3 的语义」，别把「任何活着的实体」都报上来（那会让批被凭空吊住）。
 *
 * ★ **为什么不走「卡间直连」**（§1x.2 的两条候选路，本条 = 候选 1）：直连（faith.c 调 114 暴露的函数）
 *   面窄，但要额外处理「114 不在场」「114 被删卡」两种情形，而且**下一对耦合**（116 季节点 / 117 动物灵
 *   —— 它们同样是自建实体、同样要入批）得再写一遍；本层是**一次收口**：生产者只报事实，谁消费、
 *   消费成什么由卡自己决定。⇒ 116 / 117 将来**只加两行声明**就能接进 114 的批，不必认识 114。
 *   （用户 2026-09-15 的口径也点了这个意图：「别的自建实体卡也能复用」。）
 *
 * ## 契约（写进这里当唯一出处）
 *
 *   1. **半道具不入批**（§1x.3 用户定案）：`CE_VITEM_HALF` 的两条通道**都**返回 0 / 视为 0 ——
 *      「实体只为动画」的那一档（110/111 的**微档** = 消弹每颗那一档）不算收点道具。
 *   2. **不进 SCORE、不进批分**：本层**只写自己的静态区**（不动 SCORE / MONEY / 引擎字段）；
 *      114 侧对应的 core 入口（`c2x_collect_virtual()`）**只动 `batch_count`**、
 *      不碰 `batch_score` ⇒ 结算补差 `batch_score×(mult−1000)/1000` 恒 0（绿点走信仰，不走引擎分）。
 *      档位 / 倍率 / 碎片逻辑一个字都不改（用户口径「如果不进 score 那无所谓，只 +1」）。
 *   3. **114 不在场 / 被删卡 = 零副作用**：两条通道都是**自过期**的（下面 `CE_VITEM_HOLD_FRAMES`），
 *      没有消费者时什么都不留下；生产者被删时用 `ce_vitem_drop(src)` 立刻撤掉自己的在飞声明。
 *      本层**从不**写引擎内存、从不注册回调 ⇒ 没有 114 的运行与加这一层之前逐字节等价。
 *   4. **帧内顺序无关**：消费者与生产者可能落在同一帧的不同断点上（生产者 = 卡的 `on_tick_2`，
 *      消费者 = `ce_item_frame`）⇒ 两侧都留 `CE_VITEM_HOLD_FRAMES` 帧的容差窗口，
 *      谁先谁后都读得到（在飞的点会连续飞很多帧，1–2 帧容差不改变批的边界）。
 *   5. **去重归生产者**：同一颗实体被 110 与 111 共用池（`slot_mask` 分账）认领时，只许报**一次**
 *      （`cards/faith.c` 在**收点那一次**上报，不是在各卡的回调里 ⇒ 天然一颗一次）。
 *
 * ⚠ 消费者必须**每帧**抽干 `ce_vitem_take_collect()`（114 在 `on_item_frame` 里、`batch_open` 门**之前**）：
 *   否则累加器只能靠过期丢掉（那会漏计），并且自建实体**开不了批**。
 * ⚠ 生产者必须**每帧**声明在飞数（含 0）：声明是**本帧**的语义（照 `ce_optpose_declare` 的形状），
 *   不声明 = 该源本帧没有东西在飞（容差窗口过后即失效）。*/
#define CE_VITEM_SRC_SHARED 0u      /* 共享模块的源号（110/111 共用的绿点池 = `cards/faith.c`）。
                                     * 卡自己拥有的池请用**自己的 card_id** 当源号（如将来的 116 / 117）。*/
#define CE_VITEM_HOLD_FRAMES 2      /* 两条通道的容差窗口（SDK 帧）：覆盖「生产者与消费者同帧不同序」
                                     * 与「消费者少跑一帧」；超窗 = 生产者不再声明 / 没有消费者 ⇒ 丢弃。
                                     * ★ 窗口是**读**的容差，不是维持时间：真值永远是「本帧声明」。*/
#define CE_VITEM_NOTE_MAX 1024      /* 成员累加器的上限（正常每帧被抽干 ⇒ 永远看不到这个钳；
                                     * 只有「没有消费者」时才会逼近它 —— 用来兜住 int 溢出）*/

typedef enum {
    CE_VITEM_FULL = 0,      /* **完整道具**：算批成员、算在飞（例：110/111 的**大 / 中**绿点）*/
    CE_VITEM_HALF = 1       /* **半道具**：实体只为动画 ⇒ 两条通道都不认（例：微档，§1x.3）*/
} ce_vitem_kind_t;

void ce_vitem_reset(void);                                  /* 全清（单测 / 手动用；运行时**不需要**：自过期）*/

/* ---- 生产者（自建实体池）---- */
/* 成员通道：**被收的那一刻**报 n 颗（n <= 0 = 不做事；`kind == CE_VITEM_HALF` = 直接丢弃）。
 * ★★ 2026-09-18（B1）：**只报「真空飞行中被收」的那些**（= 引擎道具 `item+0xc74 == 3` 的同一谓词，
 *   见上面那段）—— 普通游玩顺手捡（对应引擎 state 1 / 4）**别报**，那是引擎侧 §Z22 的收窄。
 * ★★ 2026-09-18（B11）：**不带值**（用户口径「绿点的面值不进 `batch_score`，它加的不是分数」）
 *   ⇒ 114 只把它算作**批成员（计数 +1）**，不进 `batch_score`（详见本段开头的说明）。
 * 累加跨帧，等消费者 `ce_vitem_take_collect()` 抽干；`CE_VITEM_HOLD_FRAMES` 内没人抽就整段作废。*/
void ce_vitem_note_collect(int kind, int n);
/* 在飞通道：**每帧**声明该源在飞的完整实体数（n <= 0 = 退出本帧声明；`kind == CE_VITEM_HALF` ⇒ 记 0）。
 * ★★ 2026-09-18（B1）：数字里**只算「此刻等于引擎 state 3 语义」的那些**（110/111 绿点 = 本池
 *   `state == 3`）—— 坠落 / 飘落 / 70px 环吸**不算**（引擎对 P / 钱同样不续批）。
 * 同一源同一帧重复声明 = 后一次覆盖前一次（别把半道具混进来）。*/
void ce_vitem_declare_flying(uint32_t source, int kind, int n);
/* 生产者退出（删卡 / 池被清空）：立刻撤掉该源的在飞声明（成员累加器不用管 —— 它会自过期）。*/
void ce_vitem_drop(uint32_t source);

/* ---- 消费者（114；也可用于诊断 / HUD）---- */
/* 抽干成员累加器：返回「自上次抽干以来被收的自建实体数」（0 = 没有；过期则丢弃并返回 0）。*/
int ce_vitem_take_collect(void);
/* 只读当前待抽干的成员数（**不**清零；过期时为 0）——诊断 / 单测用。*/
int ce_vitem_collect_pending(void);
/* 在飞数合计（Σ 各源本帧声明；超窗的源不计）。0 = 本帧没有任何自建实体在飞。*/
int ce_vitem_flying(void);
int ce_vitem_flying_from(uint32_t source);                  /* 单个源（诊断 / 单测用）*/

/* ★★ 2026-09-22：左下角那块**原生「当前（选中）主动卡」UI** 在不在 —— 货架排布要用它。
 *
 * 一手（`engine/card/th18/15-selected-card-hud.md`）：那块 UI = `abcard.anm` script0 的 VM 树，
 * 显隐的唯一判据是 `ABILITY_MANAGER->+0x38`（选中的主动卡对象）为 0 与否 —— 为 0 时引擎把整棵树的
 * `visible`（`vm+0x534` bit1）清掉 ⇒ **一个像素都不画**（不是画 0 长度序列）。
 *
 * 于是「排布」这一侧的契约很简单：**有它 ⇒ 货架起点不许压到它；没有它 ⇒ 那块不存在、不占地方**
 * （后者就是 owner 说的「整个 UI 是不存在的，这会影响 hud 排布」）。几何与让位算式在
 * `cards/hud_dim.{h,c}`（`hud_card_shelf_x0()`），本函数只是把「在不在」传进来。
 *
 * ⚠ 唯一调用方是 `sdk.h` 的两个 `ce_hud_slot_claim_*_card()` 包装（那里能读引擎全局）；
 *   **认领之外不重排**（HUD-LAYOUT §4.4）⇒ 只在这里设一次就够了，别做每帧刷新。*/
void ce_hud_card_set(int present);
