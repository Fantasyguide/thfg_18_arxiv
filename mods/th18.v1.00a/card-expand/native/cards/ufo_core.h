/* ufo_core.h —— 112「星莲船 / UFO」的**纯逻辑**层。
 *
 * 只 <stdint.h>，不含任何引擎地址、不 include sdk.h ⇒ 主机 `cc -std=c11` 直接编直接测
 * （tests/test_ufo_core.c）。碰引擎的那一半在 cards/ufo.c。
 *
 * 设计唯一真源：`docs/superpowers/specs/2026-09-12-ufo-112-design.md`（下文简写 spec §n）。
 * 一手依据：`docs/superpowers/plans/2026-09-10-ufo-card-feasibility.md`（spawn 结构 / 磁吸模型）、
 *           `engine/ufo/th12/01-ufo-system.md`（TH12 原版 §3 令牌槽 / §7 吸收链 / §8 击破奖励，
 *            本文件里写作 `UFO:行号`）、
 *           `docs/superpowers/specs/2026-09-12-shared-enemy-death-event-design.md`（死亡事件，下文 `DEATH`）。
 * 版本：TH18 v1.00a（裸地址默认属该版本）。可信度：✅ 一手 · 🟡 单源/待实跑 · ★ 占位/TODO。
 *
 * ★ 纪律：本层**一个数值都不自己发明**。凡是 spec / 一手没有的取值，一律是**具名占位常量** +
 *   `❓ 自定` + TODO，并且取「**不做**」这个语义（0 / 不设 / 不抢写），这样机制完整、单测可覆盖，
 *   而实机上不会替用户拍板。清单见 ufo.c 文件尾。
 *
 * 本文件按 spec 的小节组织：
 *   §3.1/§3.2 令牌（颜色编码 / 3 槽 / 载体参数）
 *   §3.3       管理器 = 卡本身（UFO 生命周期 / 计数 / 填充率）
 *   §3.4       吸收 = 伪磁场（几何判定 / 目标合法性栅栏三件 / 捕获半径）
 *   §3.5       击破 / 飞离 / 共用 finish
 *   §4         数值（全部一手可抄的都抄；抄不了的留占位）
 *   凑组状态机（spec 定案 1：三同色 或 三互不相同）
 */
#pragma once
#include <stdint.h>
#include <float.h>   /* FLT_MIN —— 只给下面几个 `static inline float` 用 */

/* ============================================================ §3.3 颜色编码（一手可抄）
 * 一手：TH12 `zUfo+0x30` = `1 红 / 2 蓝 / 3 绿 / −1 彩虹`（`th12:0x44a985`；`UFO:132-133`）；
 * 名字表 `th12:0x4b33e0` = {`UFO_Red`, `UFO_Blue`, `UFO_Green`, `UFO_Rainbow`}。
 * ⇒ **照抄编码**（spec §3.3 表格「−1 = 彩虹，照抄编码」）。 */
#define UFO_COLOR_RED     1
#define UFO_COLOR_BLUE    2
#define UFO_COLOR_GREEN   3
#define UFO_COLOR_RAINBOW (-1)
#define UFO_COLOR_NONE    0      /* 「没有颜色」：空槽 / 未定 */

/* ---- 金（★ 用户 2026-09-12 裁决 ①）--------------------------------------------------
 * 一手只到 `1 红 / 2 蓝 / 3 绿 / −1 彩虹`（`th12:0x44a985`；名字表 `th12:0x4b33e0` 也只有 4 个）
 * ⇒ **金没有一手来源**（spec §2.1 #3 已记「金 = ❓ 自定」）。
 * 用户裁决：**金 == 原本的蓝 UFO** —— 吸收链 / 吸满不吐奖励 / 击破产出 / HP / 飘行参数**全部照蓝的来**；
 * 贴图**暂时用蓝 UFO 的那一套**（★ 具名常量见 `ufo_color_art_index()` 与卡侧 `UFO_ECL_BODY_*` 表），
 * 切图/调色由资产线以后做 ⇒ 一句话：**金 = 蓝的调色版，等 P 图**。
 * 数值 `4` 是 ❓ 自定（枚举里 `0` = 无、`−1` = 彩虹、`1/2/3` 已占）。 */
#define UFO_COLOR_GOLD 4

/* ★❓ 自定 + TODO（用户裁决 ① 里那句「spec 没写清的别编」指的就是这一格）：
 * **金在凑组状态机里算不算独立一色？** 一手 `th12:0x422e80` 的颜色域只有 `{1,2,3,−1}`，对金完全沉默。
 *   `1`（本卡取值）= **金 ≡ 蓝**：进槽/判色前先过 `ufo_color_norm()` 把金归一成蓝 ⇒
 *       · `金 蓝 红` ≡ `蓝 蓝 红` = **「两同一异」⇒ 丢最旧**（不是「三色各一」）
 *       · `金 金 金` ≡ `蓝 蓝 蓝` ⇒ 出**蓝** UFO（行为与贴图都照蓝 —— 正是用户要的「金 = 蓝」）
 *   `0` = 金当**第 4 色**：`金 蓝 红` = 三色各一 ⇒ 彩虹；`金 金 金` ⇒ 金 UFO。
 *   ⏳ **待用户拍板**。改这一个宏就够 —— `ufo_group_classify()` 与取值无关
 *   （它只判「三格是否两两不同 / 是否同一个桶」，桶的归并全在 `ufo_color_norm()` 一处）。 */
#define UFO_GOLD_COUNTS_AS_BLUE 1

/* 颜色归一（**唯一的桶归并点**）：`UFO_GOLD_COUNTS_AS_BLUE` = 1 时金 → 蓝，其余原样。
 * 入槽 / `ufo_group_classify()` 都先过它 ⇒ 「金 = 蓝」这件事只有一处定义。 */
static inline int32_t ufo_color_norm(int32_t c)
{
#if UFO_GOLD_COUNTS_AS_BLUE
    return c == UFO_COLOR_GOLD ? UFO_COLOR_BLUE : c;
#else
    return c;
#endif
}

/* 颜色 → **美术 / ECL 子程序表的索引**。一手（`th12:0x44a909`）：`idx = (color == -1) ? 3 : color - 1`，
 * 名字表 `th12:0x4b33e0` = {`UFO_Red`, `UFO_Blue`, `UFO_Green`, `UFO_Rainbow`}。
 * ★ 用户裁决 ①：**金 = 蓝的调色版** ⇒ **金返回蓝那一格（1）**、贴图先用蓝的整套（等 P 图）。
 * 返回 −1 = 非法色（卡侧自己兜底到占位壳）。 */
int     ufo_color_art_index(int32_t color);
/* 是不是金。★ 只给「等 P 图」的资产线当判据 —— 行为上金与蓝**没有任何**区别（除贴图外一律归一）。 */
int     ufo_color_is_gold(int32_t c);

/* ---- ★★ 令牌的**场上颜色域**（用户 2026-09-18 裁决「新增 2」）--------------------------------
 * 用户原话：「**没有彩虹色，还是那句话，没有彩虹色 token，彩虹色当且仅当，收了三色 token == 飞碟，
 * 开了一个彩色 UFO（敌人）**」⇒ 一句话把颜色分成两个域：
 *   · **令牌域** = `红 / 蓝 / 绿`（**金 ≡ 蓝**，裁决「新增 3」）—— 场上飞的和进槽的都只可能是这三种；
 *   · **UFO 域** = `红 / 蓝 / 绿 / 彩虹`——彩虹**只作为召唤结果**存在（= 三色各一的合成色），
 *     由 `ufo_group_classify()` 产出，**不由任何令牌携带**。
 * 这与一手 `th12:0x426c3f`/`0x426c4d`/`0x426c5e` 一致：令牌的 type 只有
 * `10/11/12`（红/蓝/绿）与 `13/14/15`（**同一个变色令牌的三种起始显示态**，不是第四种颜色）——
 * 收进槽时按**当时显示的那一色**记账（红/蓝/绿）。
 * ⚠ 本层**仍然保留** `ufo_group_classify()` 里对「彩虹进槽」的防御支（见那里的注）：令牌域里
 *   不存在彩虹，但状态机不许被坏数据锁死。 */
/* 变色令牌的态数（一手：type 13/14/15 三种起始态）。卡侧 `UFO_TOK_RB_STATES` 与它
 * 由 `ufo.c` 的 `_Static_assert` 对账 —— 本层不 include 卡侧头文件，所以这里自己钉一个。 */
#define UFO_TOK_RB_STATES_REF 3

#define UFO_TOKEN_FIELD_COLORS_N 3
extern const int32_t k_ufo_token_field_colors[UFO_TOKEN_FIELD_COLORS_N];   /* {红, 蓝, 绿} */
static inline int ufo_token_color_is_field(int32_t c)
{
    return c == UFO_COLOR_RED || c == UFO_COLOR_BLUE || c == UFO_COLOR_GREEN || c == UFO_COLOR_GOLD;
}

/* ---- ★★ 令牌的**显示色** = 它会往槽里记的那一色（一手：变色令牌按「当时显示的那一色」入账）。
 * `is_cycler` = 这只令牌是不是一手的 `13/14/15`（**每 150 帧换色的那一只**）；
 * `rb_state` = 它当前处在三种起始态的第几态（0/1/2，对应红/蓝/绿 —— 一手 type 13/14/15）。 */
extern const int32_t k_ufo_cycler_colors[UFO_TOK_RB_STATES_REF];   /* {红, 蓝, 绿} */
static inline int32_t ufo_token_display_color(int32_t color, int is_cycler, int rb_state)
{
    if (!is_cycler) return ufo_color_norm(color);              /* 金 → 蓝（唯一的桶归并点）*/
    if (rb_state < 0 || rb_state >= UFO_TOK_RB_STATES_REF) rb_state = 0;
    return k_ufo_cycler_colors[rb_state];
}

/* ---- ★★ （P2）令牌颜色**改随机**（用户 2026-09-18 裁决：「**P2 随机吧**」）--------------------
 * 为什么不能用一手那条：一手是「哪只敌人掉、掉什么颜色都写在**敌人的 ECL 掉落表**里，没有随机、
 * 也没有轮转」（`dropExtra(10/11/12,n)`，`th12:0x4179cc`）——而 **TH18 的敌人没有这张表**
 * （spec §9 #12）⇒ 那条一手不可移植。用户裁决取**随机**。
 * ⚠ **不许调引擎的 RNG**（`0x402810` / `0x4027d0`）：那两支走 x87，会踩 `make dllx87` 的
 *   「0 条 x87」硬门（110 的 `mof.c` 也是这个理由自建 LCG）。⇒ 本层自带一个 LCG（纯整数）。
 * ⚠ **不取低位**：LCG 的低位周期短、图案明显（同余列）⇒ 用 `>> 16` 那一段再取模。 */
static inline uint32_t ufo_rand_next(uint32_t seed)
{
    return seed * 1664525u + 1013904223u;              /* Numerical Recipes LCG（与 110 同一支）*/
}
static inline int32_t ufo_token_color_for_roll(uint32_t roll)
{
    return k_ufo_token_field_colors[(roll >> 16) % (uint32_t)UFO_TOKEN_FIELD_COLORS_N];
}
/* ---- ★❓ 自定：**金令牌的出现规则**（一手没有金色；裁决「新增 3」= 金 ≡ 蓝，等用户 P 图）--------
 * 金只替换**蓝**，且每 `UFO_GOLD_CYCLE` 次里出一次（= 2 ⇒ 蓝与金都能在实机上看到）。
 * ⚠ 这是 **❓ 自定美术口径**，**不改变任何行为**（金进槽/进判据前一律 `ufo_color_norm()` 成蓝）。 */
#define UFO_GOLD_CYCLE 2
static inline int32_t ufo_token_color_gold_variant(int32_t color, uint32_t roll)
{
    if (color != UFO_COLOR_BLUE || UFO_GOLD_CYCLE <= 0) return color;
    return ((roll >> 24) % (uint32_t)UFO_GOLD_CYCLE) == 0 ? UFO_COLOR_GOLD : color;
}

/* ================================================= §3.2/§4 令牌槽（3 格，一手可抄）
 * 一手：`COLLECTED_UFOS[0..2]`（`th12:0x4b0c4c` 起）+ 已填槽数（`UFO:61-69`）。 */
#define UFO_SLOT_COUNT 3

/* ================================================= 凑组规则（spec §0 定案 1 + 用户 2026-09-12 裁决 ②）
 * 「集齐 **3 个同色** 或 **3 个互不相同**」⇒ 召唤；**「两同一异」⇒ 丢最旧**（照一手 `cnt1c = 4` 那条路）。
 * 决策只有一处：`ufo_group_put()`（下面那个注释块就是完整规则）。 */
typedef struct {
    int32_t slot[UFO_SLOT_COUNT];   /* UFO_COLOR_*；UFO_COLOR_NONE = 空槽 */
    int32_t filled;                 /* 已填槽数 0..3（单调 == 非空槽数） */
    int32_t last_group;             /* 上一次合成出的颜色；UFO_COLOR_NONE = 还没合成过 */
    /* ★ 2026-09-12（本轮一手对账）新增：**上一组第 3 格的颜色**（`COLLECTED_UFOS[2]`）。
     * 为什么需要它：一手在**彩虹 UFO 的掉落**上要读 `COLLECTED_UFOS[2]` 决定掉哪一种彩虹令牌
     * （`th12:0x4b0c54`，写点见 `ufo_break_token_state()` 的注）；而我们的槽在**召唤那一刻就清空**
     * （★ 2026-09-18 P1 已改成一手口径：槽留到 UFO 结束时才清），快照在 `ufo_group_take_ready()` 里做。
     * ⚠ 一手的槽是**留到 UFO 结束时才清**（`th12:0x44a326`–`0x44a344`）⇒ 它随时读得到；
     *   ★ P1 起我们与一手一致（结束时才清），快照仍然留着 —— 它是「召唤那一刻的第 3 枚颜色」，
     *   而结束清槽之后那个信息就没了。依据见 `ufo_group_take_ready()` 的注。 */
    int32_t last_slot2;
} ufo_group_t;

void    ufo_group_reset(ufo_group_t *g);
/* 放一个令牌进**第一个空槽**（**不过 `ufo_color_norm()`**，桶归并在 classify 里做）。
 * 无空槽 / 颜色非法 ⇒ 返回 0 且不改状态。
 * 返回 1 = 已放入；此时若 `g->filled == UFO_SLOT_COUNT` 就该调 `ufo_group_classify()`。 */
int     ufo_slot_set(ufo_group_t *g, int32_t color);

/* ================================================= ★★ P1（用户 2026-09-18「按一手」）：槽的**生命周期**
 * 一手（一手逐条，地址在 `AUDIT.md` §DQ）：**槽在 UFO 结束时才清**，不是召唤那一刻清：
 *   · 收满第 3 枚 ⇒ `0x422e80` 写 `[0x4b0c5c] = 3`（= 「该召唤了」），**槽计数 `[0x4b0c58]` 仍是 3**；
 *   · 召唤只读颜色 + 置自己的 `+0x2c` 闩（`0x44a4b2` / `0x44a8a0`），**不碰槽**；
 *   · UFO 真的从敌机表里消失那一刻（`0x44a326`–`0x44a344`）才把 `COLLECTED_UFOS[0..2]` 与两个计数清 0。
 * ⇒ 语义后果（**这就是用户要的那条**）：「UFO 在场期间槽是满的」⇒ **收不进新令牌**；
 *   此时再碰到令牌 ⇒ 令牌**被消耗**，但给的是那个「满槽补偿」（见下面 `UFO_SLOT_FULL_*`），不进槽。
 * （我们以前照 2026-09-12 的早期口述「召唤即清」做 —— 那条口径作废。） */
/* 取走「已成组」这个结果：返回合成色（UFO_COLOR_NONE = 没成组），并记录 `last_group` / `last_slot2`。
 * **不清槽**（= 一手：槽留到 UFO 结束时才清）⇒ 调用方要清时显式调 `ufo_group_clear()`。 */
int32_t ufo_group_take_ready(ufo_group_t *g);
/* 清空三格（+ 清 `last_group` / `last_slot2`）。**调用点只有一处**：UFO 结束（击破 / 飞走 / 撤场 / 删卡
 * 都汇到卡侧的 `ufo_teardown()`）—— 一手 `0x44a326`–`0x44a344`。 */
void    ufo_group_clear(ufo_group_t *g);

/* ---- 满槽补偿（一手：`th12:0x4270b0` 开头那一段，逐条）------------------------------------
 *   判据：**已填槽数 >= 3**（`0x4270b5 cmp [0x4b0c58],3`，不是「三格各自非零」）；
 *   动作：`[0x4b0c78] += 100000`（`0x4270d1 add eax,0x186a0`）并**夹到上限 `[0x4b0cf4]`**；
 *   表现：**飘一个 `1000` 的弹字**（`0x4270f1 mov eax,0x3e8`）+ 颜色 `0xff80ff80`（`0x4270e5 push`）；
 *   令牌：**被消耗**（`0x426da4` 那条共同尾：state=0 + 音效 0x26 + 给 VM 发 interrupt 1）。
 * ⚠ `[0x4b0c78]` 是 **TH12 的 PIV（= 点道具的价值基数，×100 定点）**；**TH18 没有这个状态量**
 *   （TH18 的点道具身价 = `(MONEY+1) × K × 100`，`engine.h` 的 `CE_ADDR_MONEY_BASE`）
 *   ⇒ 「+PIV」在 TH18 只能**映射**。落点与三种映射见卡侧 `ufo_slot_full_bonus()` 的注（默认取哪一种、
 *   为什么，都写在那里 + `TUNING.md`）。**本层只提供一手的两个原始数值。** */
#define UFO_SLOT_FULL_PIV_ADD       100000   /* ✅ 一手：`add eax,0x186a0`（= +1000 显示值，×100 定点）*/
#define UFO_SLOT_FULL_POPUP_VALUE   1000     /* ✅ 一手：弹字的值（`0x4270f1 mov eax,0x3e8`）*/
#define UFO_SLOT_FULL_POPUP_COLOR   0xff80ff80u /* ✅ 一手：弹字颜色（`0x4270e5 push 0xff80ff80`）*/

/* ---- 入槽的**唯一推荐入口**（把「槽满怎么办」这条决策收在纯逻辑层，可单测）----
 * `ufo_group_put` 的返回码（刻意与卡侧同名，卡侧直接 `#define` 转发）。 */
#define UFO_GROUP_PUT_NONE  0   /* 没收下（颜色非法 / 写不进去）*/
#define UFO_GROUP_PUT_HELD  1   /* 收下了，但**没成组**（槽没满，或满了不成组⇒已当场丢掉最旧）*/
#define UFO_GROUP_PUT_READY 2   /* 收下了且**成组**⇒ 调用方去 `ufo_group_take_ready()` + 召唤 */

/* 落一个令牌进槽，返回上面的码。
 *
 * ★★ 规则（**2026-09-12 按用户裁决 ② 改成照抄一手**，见下；改之前是「只在死锁时丢最旧」）：
 *   ① **槽满且已经成组**（上一帧被「场上已有 UFO」挡下的那个组合）⇒ 原样报 `READY`：
 *      **不收、不丢、不替换**（丢掉等于让玩家白攒一个组）。
 *   ② **第三格刚被填上就判合成**（一手 `th12:0x422e80` 的判定点就是这里）：
 *        · 三同色 ⇒ 该色（`READY`）
 *        · 三色各一 ⇒ 彩虹（`READY`）
 *        · **两同一异 ⇒ 丢掉最旧的一格（`slot[0]`）、保留最近两个**（一手 KEEP2 `cnt1c = 4`：
 *          `slots[0] = slots[1]; slots[2] = 0; cnt18 = 2; slots[1] = color`）⇒ 返回 `HELD`，
 *          槽里**只剩 2 枚**（= 用户 2026-09-12 确认的「th12 会删掉第一个 token」）。
 *   ③ **丢完要重判合成**（用户明确要求）：判出来理论上不可能是组（2 格判不出组），
 *      但这条重判让「丢完之后的槽状态」与一手逐字一致，也顺手挡住未来的规则变动。
 *   ④ 槽满且**不成组**时**先丢最旧再收新的**：新规则下这条路在②里就被消掉了（满格不成组当场变 2 格）
 *      ⇒ 这一支是**防御性**的（坏状态 / 外部直接改 struct），与②同一个「丢最旧」口径。
 *   ⚠ 结果：**槽里永远不存在「满 3 格却不成组」的稳态**（一手也是 —— 这才是它不会锁死的原因）。
 *   ⏳ 用户对 spec §9 #2 拍板（= 本裁决）已落地；再改就只改这一个函数。
 * `*out_dropped`（可 NULL）= **丢掉的令牌个数**（0 或 1；防御支可能 2），卡侧累加进 `dropped_tokens`。 */
int     ufo_group_put(ufo_group_t *g, int32_t color, int32_t *out_dropped);

/* ★ **字面一手的 KEEP2 条件**（纯函数，只判不改）—— ★ 2026-09-12 本轮**订正**：
 * 一手的条件其实有**三条**（`th12:0x422ed4`–`0x422ee9` 逐条）：
 *     `0x422ed4 cmp ecx,edi`（ecx = `slots[0]`、edi = 新颜色）/ `0x422ed6 jne 0x422ee3`
 *       · 相等 ⇒ `0x422ed8 cmp eax,edi`（eax = `slots[1]`）
 *           `0x422eda jne 0x422ef3`   ← **`slots[0] == color` 且 `slots[1] != color` 也走 KEEP2**
 *           两边都相等 ⇒ `0x422edc mov [esi+0x18],edi`（三同色 ⇒ 出该色）
 *     `0x422ee3 cmp eax,edi` / `0x422ee5 je 0x422ef3`（`slots[1] == color`）
 *     `0x422ee7 cmp ecx,eax` / `0x422ee9 je 0x422ef3`（`slots[0] == slots[1]`）
 *     `0x422eeb or eax,0xffffffff`（三条都不成立）⇒ **彩虹**
 * ⇒ 三条不成立 = `slots[0] != color && slots[1] != color && slots[0] != slots[1]`
 *   = **三色互不相同** ⇒ 彩虹。**「两同一异」的三种排列（含首尾同色 `{A,B,A}`）全部走 KEEP2。**
 * ⚠ 本节此前写的是「`{A,B,A}` 不满足条件 ⇒ 字面一手出彩虹」—— **那是漏读了上面第一条分支**
 *   （只抄了后两条），本轮对着完整反汇编订正。⇒ 结论：**我们的实现与字面一手在 6 种排列上完全一致**，
 *   不存在「有意偏离」，`AUDIT.md` §BF4 那条「与字面一手的差异」应当作废（报告里已列）。 */
int     ufo_group_keep2_literal_th12(const ufo_group_t *g, int32_t color);

/* ---- ★ 用户 2026-09-12 裁决 ③ 的**触发条件**（纯逻辑）----
 * ★ 2026-09-18 订正：框**已经实装**（卡侧 `ufo_pentagon_hint_update()` 真建/跟随/收，
 *   脚本 = `ability.anm` 223 = TH12 `bullet.anm` script211 的逐条移植）——本行原写「美术待做、
 *   只有预留钩子」是过期口径。本层只负责**判据**（给卡侧问「该不该亮框」）。
 * 「当玩家手上**有两个令牌**（= 再来一枚就能成组）时，屏幕上这类令牌外面会有一圈**五边形框**」。
 * ① `ufo_group_pentagon_hint(g)`：槽里此刻**是不是「差一枚就成组」**（= 存在某个合法颜色能补上）。
 *    ⚠ 在当前规则下它**等价于 `filled == 2`**（4 个颜色桶里任意两枚都补得出三色各一或三同色）
 *    —— 仍然按 `∃c` 写：规则一变（例如金不再归一成蓝）它就自动跟着变，不用回来改。
 * ② `ufo_pentagon_hint_for_color(g, c)`：**场上某一枚令牌**（颜色 `c`）自己够不够格被画框
 *    —— 收了它就能成组。这才是「框画在哪些令牌外面」的判据（卡侧预留钩子用它）。
 * ⚠ 本卡**不画**这个框（不用 `drawRect` 硬凑一个假的）：缺 `assets/ability/scripts/` 下的
 *   五边形框 anm 脚本（`layer(25)`），见 spec §5 资产清单那一条。 */
int     ufo_group_pentagon_hint(const ufo_group_t *g);
int     ufo_pentagon_hint_for_color(const ufo_group_t *g, int32_t color);

/* 凑组判定（**纯函数，不改状态**）：三同色 → 该色；三色互不相同 → 彩虹；否则 UFO_COLOR_NONE。
 * 先过 `ufo_color_norm()`（金 → 蓝）⇒ 「金 蓝 红」= 两同一异、「金 金 金」= 蓝。
 * 槽未满 ⇒ UFO_COLOR_NONE。 */
int32_t ufo_group_classify(const ufo_group_t *g);
int     ufo_group_has_match(const ufo_group_t *g);
/* 预览：若此刻合成会得到什么颜色（= classify，命名给人看）。 */
int32_t ufo_group_peek_color(const ufo_group_t *g);
/* 消费掉当前组：判色 → 记 last_group → 清空 3 槽与 filled。返回合成色（NONE = 没得合、状态不变）。 */
int32_t ufo_group_take_ready(ufo_group_t *g);
int     ufo_slots_filled(const ufo_group_t *g);
/* 单色校验：`c` 是 UFO_COLOR_* 之一则返回非 0（★ **含金** —— 金是合法颜色，只是行为归一成蓝）。
 * 抓「彩虹令牌」时用它把令牌实体的颜色当**占位**校验。 */
int     ufo_color_valid(int32_t c);

/* ============================================== §3.3 UFO 生命周期（管理器 = 卡本身）
 * 一手字段分工（`UFO` §4）：`zUfo+0x14` 存活帧数 / `+0x28` 存活上限 **720** /
 * `+0x48` `+0x4c` 两个吸收计数 / `+0x50` 填充率 0..1。本层照分工，但**不抄对象本体**（spec 定案 4）。
 * ★ 存活上限 720 是**一手可抄**的（`UFO:180`）；「实际离场 420 帧」**不抄**（spec §4：TH18 节奏不同 ⇒
 *   正式离场由 ECL 自己飞走，卡侧只认「id 不在了」这个信号，spec §9 #10）。 */
#define UFO_LIFE_MAX   720      /* ✅ 一手：`zUfo+0x28`（`th12:0x44a8a0`，`UFO:180`） */
/* ✅ 一手：吸收门 = 「UFO 存活帧数 **> 59**」才能吸（`th12:0x4271c0`，`UFO:204`）。
 * ⚠ 常量存的是**那个比较的另一边**（59），吸第一颗的帧号 = 60 —— 命名容易读错，所以两件事分开放：
 *   `ufo_can_absorb()` 判 `frames > UFO_ABSORB_PAUSE`；`UFO_ABSORB_FIRST_FRAME` = 它 +1。
 * （实装时先写成 60 把「frames > 60」变成「第 62 帧才吸」，单测当场红 ⇒ 这里按一手字面值钉死。） */
#define UFO_ABSORB_PAUSE       59
#define UFO_ABSORB_FIRST_FRAME 60   /* = 59 + 1：存活到第 60 帧起（含）能吸 */

typedef struct {
    uint32_t ufo_id;        /* 场上 UFO 的 `enemy_id`；0 = 场上没有（⚠ 只留 id，禁缓存指针：DEATH §2.4） */
    uint32_t frames;        /* UFO 已存活帧数（`zUfo+0x14` 的语义） */
    int32_t  color;         /* UFO 颜色 UFO_COLOR_*（合成色） */
    int32_t  cnt_p;         /* 吸收的 type 1（火力道具）计数 —— `zUfo+0x48/+0x4c` 那个分工 */
    int32_t  cnt_money;     /* 吸收的 type 2（金钱道具）计数 */
    /* 填充率 0..UFO_FILL_UNIT（**百万分比整数**，不是 float）。
     * ★ 为什么不用 float：i686 Windows ABI 下 `float`/`double` ↔ `int` 的转换会生成 x87 指令
     *   （`fldl`/`fistpll`/`fldcw`），而 `make dllx87` 断言我们的 object 里 **0 条 x87**。
     *   本卡实装时先写 `float fill` 就踩了这道门（`ufo_absorb_item` 里一次 `(double)` 就带出 6 条）。
     *   ⇒ 本层**完全不用浮点**；要报给人看的 0..1 比率由卡侧 `ufo_fill_ratio()` 转一次（那只在
     *   `static inline` 里、且是纯 SSE 代码）。 */
    int32_t  fill_millionths;
    /* —— 下两格是「供 checker 记账」的观测值，不参与判定（本层无指针 / 无回调查询）—— */
    uint32_t absorbed;      /* 已被吸收（置空槽）的道具总数 */
    uint32_t skipped_guard; /* 因为「目标点合法性栅栏」而本帧没被拉的道具数（记账，不是错误） */
    /* ★★ 2026-09-18（P4）本轮召唤这只 UFO 时**写进 spawn 的 life**（= 血条的分母）。
     * 为什么要存：一手 `{1600..2500}` **一律用真值**（★ owner 2026-09-26 拍定：「肯定是真值」）；血条/HUD 用的是
     * `life.current / life.maximum` ⇒ 分母必须与**当初真的写进去的那个数**一致，不能在显示侧
     * 另算一次（两处各算一次 = 迟早漂移）。0 = 还没召唤（或旧状态）。 */
    int32_t  hp_max;
} ufo_ufo_t;

void     ufo_reset(ufo_ufo_t *u);
/* 召唤一个 UFO（花色 → 清零两个计数与 fill）。返回 1 = 成功（有 ufo_id）。 */
int      ufo_spawn(ufo_ufo_t *u, uint32_t enemy_id, int32_t color, int32_t hp_max);
int      ufo_present(const ufo_ufo_t *u);
int      ufo_can_spawn(const ufo_ufo_t *u);     /* !present 才允许再召唤（一手：单例管理器） */
/* 存活帧 +1。返回非 0 = **本帧达到存活上限**（一手 720）⇒ 卡侧只记一条「飞走」收尾的候选，
 * 真正的飞走信号仍是「id 不在了」（spec §9 #10：TH18 的飞走由 ECL `delete()` 驱动，不经过 `Enemy__die`）。 */
int      ufo_tick_advance(ufo_ufo_t *u);
void     ufo_finish(ufo_ufo_t *u);              /* 击破 / 飞离 **共用**收尾（spec §3.5） */

/* 现在能不能吸（一手：存活帧数 > 59；`th12:0x4271c0`）。 */
int      ufo_can_absorb(const ufo_ufo_t *u);

/* ================================================== §3.4 吸收 = 伪磁场（几何判定）
 * 一手过滤（`th12:0x4271c0`，`UFO:204-207`）：**只吸类型 1 与 2**。
 * 两侧 type 语义都已一手：TH18 `1` = 火力道具、`2` = **金钱**（`SHOP:301` / `SHOP:20`）
 * ⇒ 与用户定案 ⑦「蓝点 == 金钱」自洽。 */
#define UFO_ITEM_TYPE_P     1
#define UFO_ITEM_TYPE_MONEY 2

int  ufo_absorbable(uint32_t type, int32_t state, uint32_t ufo_id, int32_t item_type_ok);
/* 到 UFO 中心的距离² < 324（= 18 px）⇒ 吸收（✅ 一手 `th12:0x426879`；spec §4「18 px 可作候选照抄」） */
#define UFO_CAPTURE_DIST      18.0f
#define UFO_CAPTURE_DIST_SQ   324.0f
int  ufo_within_capture(float dx, float dy);
int  ufo_within_capture_sq(float d2);

/* ★❓ 自定（spec §4：「与 TH18 原生磁吸手感对齐前先跑」）：本卡自己的**拉拽步长**。
 * 一手是 TH12 的 `item+0x9bc += (UFO.timer/600)*0.05 + 0.02`、上限 6.0（`th12:0x426879`，spec §4）——
 * 但那是 TH12 的「飞向 UFO」专用状态，TH18 没有这个状态（`PLAN` §2.3：`zItem` 无目标点字段），
 * 我们只能自己直写 `pos` ⇒ 步长必须**重新标定**。占位 = 一手公式逐字搬过来（照抄不算发明），
 * 标 🟡 待实跑调。 */
#define UFO_DRAG_SPEED_REF_FRAMES  600.0f   /* ✅ 一手：TH12 公式里的 600 */
#define UFO_DRAG_SPEED_BASE        0.02f    /* ✅ 一手：TH12 公式里的 +0.02 */
#define UFO_DRAG_SPEED_SLOPE       0.05f    /* ✅ 一手：TH12 公式里的 ×0.05 */
#define UFO_DRAG_SPEED_MAX         6.0f     /* ✅ 一手：TH12 上限（`th12:0x4a4250` = 6.0） */
/* ⚠⚠ i686 **Windows ABI 的 `float` 返回值走 x87 `st(0)`** ⇒ 任何**非 `static inline` 的
 * `float` 函数**都会让 `make dllx87` 的「0 条 x87 指令」断言变红（本卡实装时踩过）。
 * 所以本层**所有返回 `float` 的函数一律 `static inline` 定义在头文件里**（`-O2` 下内联、函数边界消失），
 * `.c` 里不留实现。`static inline` 在 C11 里是「一个定义规则」安全的（非 `extern inline` 语义）。*/
static inline float ufo_drag_speed(uint32_t ufo_frames)
{
    float t = (float)ufo_frames / UFO_DRAG_SPEED_REF_FRAMES;   /* 一手：UFO.timer / 600 */
    float s;
    if (t > 1.0f) t = 1.0f;                                   /* 一手公式没有比值上限项，但速度有上限项；
                                                               * 夹比值不改变速度上限的行为 */
    s = t * UFO_DRAG_SPEED_SLOPE + UFO_DRAG_SPEED_BASE;
    if (s > UFO_DRAG_SPEED_MAX) s = UFO_DRAG_SPEED_MAX;
    if (s < UFO_DRAG_SPEED_BASE) s = UFO_DRAG_SPEED_BASE;
    return s;
}
/* ★★ 2026-09-18（实跑反馈修）：一手拉拽速度是**累计量**，不是"每帧位移" —— 见 `ufo.c` 的长注。
 *   `ufo_drag_speed()` = 一手的**单帧增量**（`(timer/600)×0.05 + 0.02`，上限 6.0 是加在**累计值**上）；
 *   `ufo_drag_speed_accum()` = 从第 1 帧加到第 N 帧的**和**（= 每帧真正的位移），封顶 6.0：
 *       Σ_{k=1..N}(k/600×0.05 + 0.02) = 0.02N + (0.05/1200)·N(N+1)
 *   ⇒ N=100 时 ≈ **2.42 单位/帧**（把它当 0.028 用就慢了几十倍 —— 那就是实跑报的"定住"）。
 * 道具那边用**逐颗的增量表**（`ufo.c` 的 `s_pull[]`，更忠实：中途开始被拉的道具不会白得历史）；
 * 信仰点是自建实体、没有道具槽地址可做键 ⇒ 这边用**闭式累计**（❓ 自定：晚进场的点会继承当前速度）。 */
/* ★★★ 2026-09-24（owner 第三轮实跑）：「UFO 吸收的速度很奇怪，**先是让道具定那了**，然后再是
 *   缓慢**从 0 开始匀加速**吸收；之前不是这样的你怎么又改错了，是**匀速缓慢吸收**」
 *   ⇒ 拉拽速度改成**常量**：`UFO_DRAG_SPEED_CONST_MILLI` = **1500**（= 1.5 单位/帧）。
 *
 *   一手依据（**TH18 自己那条匀速吸附**）：`th18:0x446647` 把 `item+0xc80` 写成
 *     `[player+0x47988] / 3.0` = 5.0 / 3 = **1.667 单位/帧**（state 4 = 70 px 环吸，每帧重查）
 *     ⇒ 引擎那一支是**匀速**的；本卡取 **1.5**（比它再缓一点，就是 owner 说的"缓慢"）。
 *
 *   ⚠ **TH12 的 state 7（被 UFO 吸）那一支确实是累计加速**（`th12:0x426926 fadd [edi+0x9bc]`，
 *     增量 = `timer/600×0.05 + 0.02`、上限 6.0；2026-09-18 那版照抄了它，也是 owner 当时
 *     「吸收速度极慢、道具定住了一样」促成的）⇒ **本行是 owner 口径、不是复刻**：
 *     一手的曲线在头 1 秒里只有 0.02→0.04 单位/帧（≈0.08 表面 px/帧）⇒ 肉眼就是"定住"。
 *   回退 = 把 `UFO_DRAG_ACCEL_ENABLE` 改回 **1**（下面那条闭式与 `ufo.c` 的逐颗增量表都还在）。*/
#define UFO_DRAG_ACCEL_ENABLE      0
/* ★★★ 2026-09-24 第二轮（owner：「**th10-th11 的绿点似乎没有被 UFO 吸收**…理应被吸收的绿点结果
 *   还能慢慢悠悠往下飘」）—— 常量从 1.5 提到 **3.0 单位/帧**：
 *   **1.5 压不过落体末速**：道具与信仰点的下落都是 `vy += 0.03`、**封顶 2.0**（一手 `0x4b90a4`；
 *   信仰点同表，见 `faith.c` 的注）⇒ 匀速 1.5 < 2.0 时**永远追不上**，看着就是"慢慢悠悠往下飘"。
 *   一手那条累计曲线在第 **~1.2 s** 越过 2.0 ⇒ 3.0 与它的量级一致，且对速度下限留了一档余量。*/
#define UFO_DRAG_SPEED_CONST_MILLI 3000     /* ❓owner 口径：3.0 单位/帧（= 6 表面 px/帧）*/
static inline float ufo_drag_speed_const(void)
{
    return (float)UFO_DRAG_SPEED_CONST_MILLI / 1000.0f;
}

static inline float ufo_drag_speed_accum(uint32_t frames)
{
    float n;
    float s;
    if (!UFO_DRAG_ACCEL_ENABLE) return ufo_drag_speed_const();
    n = (float)frames;
    s = 0.02f * n + (0.05f / 1200.0f) * n * (n + 1.0f);
    if (s > UFO_DRAG_SPEED_MAX) s = UFO_DRAG_SPEED_MAX;
    if (s < UFO_DRAG_SPEED_BASE) s = UFO_DRAG_SPEED_BASE;
    return s;
}

/* 从道具指向 UFO 的**未归一化**位移分量。返回 0 = 重合/非正距离（没有方向）。 */
int   ufo_drag_delta(float ix, float iy, float ux, float uy, float *out_dx, float *out_dy);
/* 往 UFO 走一步：**按距离的比例**逼近（`d > speed` 时每帧正好走 `speed`；`d <= speed` 时这一帧
 * 正好压在 UFO 上，绝不越过）。
 * ★ 为什么不归一化：`speed` 是从 TH12 的「速度」公式抄来的（spec §4），但 TH12 那套是引擎给道具
 * 单独一条「飞向 UFO」状态、由引擎自己做 `pos += vel × GAME_SPEED`（`th12:0x426879`）；
 * TH18 没有这个状态（`PLAN` §2.3），我们只能直写 `pos` ⇒ 必须自己保证**不过冲**，否则道具会在
 * UFO 两侧来回跳。比例步进天然满足这一点，且不需要 sqrt（本层不链 libm）。 */
void  ufo_drag_step(float *px, float *py, float ux, float uy, float speed);

/* ---- ★★ 围栏（spec §3.4；★ 2026-09-21 P2 #4 砍掉「离自机 30 px」那一条）-------------
 * ① **目标点必须留在屏内**：引擎当帧就删。一手（本次实装新增一手复核，见 ufo.c 头）：
 *    `0x445e38` 的删除判定读 `0x4b93a4` = **472.0f**（`item+0xc30 > 472` ⇒ 删）与
 *    `0x4b9340` = **200.0f**（`|item+0xc2c| >= 200` ⇒ 删；`0x445e28` 的 `andps` 是取绝对值）。
 * ② ~~目标点离自机必须 > 30 px~~ —— **已从「拉拽」这一路上撤掉**（保留函数本身，单测还在用）：
 *    一手 TH12 的 state 7（飞向 UFO）**每帧走完位移之后会落回共同的尾巴 `0x4260af`**，
 *    而那就是**自机收点矩形**的判定（`0x426100`–`0x426164` 四条 `fcomp [player+0xc444..]`）
 *    ⇒ **一手允许自机在半途把被吸的道具收走**（收走也合理：玩家照样拿到那颗道具）。
 *    我们原来照抄了「别让自机收走」这条**并不存在于一手**的栅栏 ⇒ 后果是：
 *    道具的下一步只要会踏进自机的 30 px 收点圈，我们就**整帧不动**它；下一帧速度更大、
 *    目标点还是落在圈里 ⇒ **永久卡在圈边上**（自机不挪窝它就不动）。
 *    ⚠ 这正是实跑口径「吸收速度似乎还是慢了 / 像定住了一样」的第二个来源（第一个是
 *      2026-09-18 已修的「累计速度当单帧位移」）。
 * ③ **跳过 state 3 / 4**：那是引擎自己的真空飞行 / 70 px 环吸，别抢。 */
int ufo_target_on_screen(float x, float y);
/* ⚠ 仍然导出（单测 / 文档口径），但**拉拽那一路上不再使用它**（理由见上面 ②）。 */
int ufo_target_clear_of_player(float x, float y, float px, float py);

/* 这一帧往 (nx,ny) 走一步合不合法。**只判屏内**（②③ 不在这里，见上面）。
 * ★ 2026-09-21：签名从 `(nx, ny, px, py)` 收成 `(nx, ny)` —— 自机坐标不再参与判据，
 *   留着形参只会让人以为「离自机太近就不能走」。 */
int ufo_step_allowed(float nx, float ny);

/* ---- 吸收的位移**是直写 pos**（不是写 vel）------------------------------------------
 * 一手时序：`on_item_frame`（`0x446ee3`）在道具池 body（`0x445a80`）**之前** ⇒ 本帧我们写的
 * pos 之后，引擎还会拿 state 1（抛物/坠落）的物理再叠一点（`REVIEW:1002`，`PLAN` §2.4）
 * ⇒ **写 pos 的同时必须把 vel 清零**，否则残余 vel 会被 body 用掉、把道具甩回原处。
 * 一手位移式（本次实装新增复核，`objdump` `0x445bf5`–`0x445c55`）：
 *     pos.x += vel.x × GAME_SPEED × slowdown ；vel.y += GAME_SPEED × 0.03 ；vel.z 同 x
 * ⇒ 清零 vel 之后每帧只剩 vy 的 +0.03 重力（`PLAN` §2.4 已记「可忽略」）。 */
#define UFO_ITEM_VEL_DAMP 0.0f

/* ============================== §4 填充率与吸满开奖（一手公式 + 一手表）
 * 一手（`th12:0x44aca0`，`UFO:213-222`）：**type 1 与 type 2 同权**，每次
 *     fill += 0.032 − 关卡号 × 0.002
 * 且判定是**严格** `> 1.0`（不是 `>=`），满了夹回 1.0。
 * ⚠ spec §4 明确：逐关阈值（34/36/39/42/46/51/56）是**为 TH12 道具密度**算的 ⇒ TH18 要重估。
 *   本实装**照抄公式**（照抄不算发明），并额外提供 `ufo_fill_increment` 走一个**具名占位缩放**，
 *   默认 1000 ‰ = 完全不缩放 ⇒ 「不替用户拍板」。 */
/* 一手公式（`th12:0x44aca0`）的**逐字**形式，只供文档/对账用 —— 运行时一切走下面的整数版
 * `ufo_inc_millionths`（千分比 × 1000 = 百万分比；段名里的数字是百万分比）。 */
#define UFO_FILL_PER_ITEM_MILLIONTHS   32000   /* ✅ 一手 0.032 ⇒ 32000/1000000 */
#define UFO_FILL_PER_STAGE_MILLIONTHS   2000   /* ✅ 一手 0.002 ⇒  2000/1000000 */
#define UFO_FILL_PER_TOKEN_MILLIONTHS 100000   /* ✅ 一手 0.1   ⇒ 100000/1000000 */
#define UFO_FILL_UNIT       1000000 /* 1.0 的百万分比 */
#define UFO_FILL_SPIT_AT    UFO_FILL_UNIT /* ✅ 一手：严格 `> 1.0` 才吐 */
#define UFO_FILL_SCALE_PERMILLE 1000 /* 🟡 占位：1000 = 不缩放（TH12 原值）；调手感就改这一个数 */

/* 单颗道具的填充增量（**百万分比整数**）。⚠ 整数是**必须**的，不是风格问题：
 *   · `float` 返回值/转换 → i686 x87 指令 → `make dllx87` 红（见 `ufo_ufo_t.fill_millionths` 的注）；
 *   · 而且 float 只有 24 位尾数，而这一族增量在 1e-5 量级、1.0 在 1e0 → 用 float 累加会偏
 *     （实装时先按 float 写，单测立刻红：stage 1 该 34 颗却算成 32）。 */
static inline int32_t ufo_inc_millionths(int32_t stage, int32_t scale_permille)
{
    int64_t inc = (int64_t)(32 - 2 * stage) * 1000;   /* 一手 0.032 − stage×0.002 ⇒ ×1000000/1000 */
    if (inc <= 0 || scale_permille <= 0) return 0;
    inc = inc * scale_permille / 1000;
    return inc > 0 ? (int32_t)inc : 0;
}
/* 单颗增量的「每 1.0 的千分比」预览（给人看 / 卡侧日志用；纯整数除法，不碰浮点）。 */
static inline int32_t ufo_inc_permille(int32_t stage, int32_t scale_permille)
{
    return ufo_inc_millionths(stage, scale_permille) / 1000;
}
/* 0..1 的比率，只给 HUD / 日志用。**唯一**的浮点出口，而且是 `static inline`（x87 门）。 */
static inline float ufo_fill_ratio(const ufo_ufo_t *u)
{
    return u ? (float)u->fill_millionths / (float)UFO_FILL_UNIT : 0.0f;
}
/* 把当前 fill 推到「吸满」所需的最少道具数 = floor(1/inc) + 1。
 * inc <= 0 ⇒ −1（不可达）。spec §4 那张关卡表就是本函数的输出（34/36/39/42/46/51/56）。
 * ⚠ 一手判定是 `1.0 >= fill`（`0x44acd8 fcom [esi+0x50]; test ah,0x41; jnp` 跳到收工 —— jnp 跳
 *   的条件是 `fill >= 1.0`），**不是**严格 `> 1.0`；本函数按 `floor(1/inc)+1` 是为了与 spec §4
 *   那张**实测表**（34/36/39/42/46/51/56）逐格对上（浮点累加的漂移在整数版本里不存在）。 */
int   ufo_items_to_full(int32_t stage, int32_t scale_permille);
/* 记一颗被吸收的道具：两个计数 + fill。返回非 0 = **这一颗把它吸满了**（卡侧据此吐奖励）。
 * ★★ 2026-09-12 本轮按一手**订正语义**（旧版是「满着期间每颗都返回非 0」，会把奖励刷成无限）：
 *   一手 `th12:0x44aca0` 的结构是**两段检查**（`0x44acd8` / `0x44ad63`，逐条见下）：
 *     ① 计数**先做**（`0x44acc8`/`0x44acd5` 的 `add [esi+0x48],edi`）—— 满着也照数；
 *     ② `0x44acd8 fld1; fcom fill; test ah,0x41; jnp 0x44ae98` ⇒ **fill 已经 >= 1.0 就整个收工**
 *        （**不加 fill、不开奖**）；
 *     ③ 没满才按色加 fill（`0x44acf5` 的 `0.032 − 关卡×0.002`；令牌 type 10/11/12 走 `0x44ad57` 的 `+0.1`）；
 *     ④ 加完之后再看一眼（`0x44ad63 fcom fill; test ah,0x41; jp 0x44ae98`）⇒ `fill >= 1.0` 才
 *        夹回 1.0 + 播音 + 发中断 + 按色吐**一件**奖励（`0x44ad71`–`0x44ae97`）。
 *   ⇒ 可观测语义：**一次吸满只开一次奖**；满着期间再吸只是计数（不再产奖励）。
 *   卡侧现在就是 `if (ufo_absorb_item(...)) 开奖` ⇒ 本条订正**直接**修掉了「吸满后每颗再来一件」的漏洞。 */
int   ufo_absorb_item(ufo_ufo_t *u, uint32_t type, int32_t stage, int32_t scale_permille);
/* ★★★ 2026-09-26（owner 的跨卡口径）：吸收的**通道**从"道具类型"提升为显式 kind ——
 *   因为中信仰点、以及**满 P 时的樱点**都要按 **P** 计（进彩色转换 / 红 UFO 的倍率通道），
 *   而**大信仰点"原封不动"**（只加 fill、不进任何计数）。三种 kind = 一手那两种 type 的语义 + 中性：
 *     `UFO_ABSORB_P`       = 一手 type 1：非彩虹 → `cnt_p`、彩虹 → `cnt_money`（即"转换时变金钱"）
 *     `UFO_ABSORB_MONEY`   = 一手 type 2：非彩虹 → `cnt_money`、彩虹 → `cnt_p`（"金钱转成 P"）
 *     `UFO_ABSORB_NEUTRAL` = **只加 fill、不计数**（大信仰点：不参与转换、原封不动）
 *   `ufo_absorb_item()` 保留为 `type` 包装（行为与以前逐字相同 ⇒ 原生道具那条路不受影响）。*/
enum { UFO_ABSORB_P = 0, UFO_ABSORB_MONEY = 1, UFO_ABSORB_NEUTRAL = 2 };
int   ufo_absorb_item_ex(ufo_ufo_t *u, int kind, int32_t stage, int32_t scale_permille);

/* ---- 吸满吐奖励（表抄，type 号映射到 TH18）------------------------------------------
 * 一手（`th12:0x44aca0`，`UFO:213-222`）：红 → 道具 4；绿 → 道具 5；彩虹 → 13/14/15；
 * **蓝 → switch 无 case 2 ⇒ 什么都不吐**（spec §9 #3 未拍板「复刻还是修正」⇒ 本实装**复刻**）。
 * TH18 的 type 号**不能照抄**（上限 0x13，0x10–0x13 是卡道具；spec §2.2 末两行）⇒ 映射表：
 *   红 4 → **TH18 type `0x10`**（EXTEND2 卡道具 = 残碎卡 → `CardLifeFragment`，一手三条见 `signal_core.h`）
 *   绿 5 → **TH18 type `0x11`**（BOMB2 卡道具 = 符卡碎片卡 → `CardBombFragment`，同源）
 *          ⚠ 但绿 UFO **吸满**那一路**不吐它** —— 用户裁决 P6 = 掉**整颗 Bomb**（自建实体），
 *            只有**击破**的颜色加成（`ufo_break_color_bonus`）才吐 `0x11`。
 *   彩虹 13/14/15 → **TH18 没有对应原生 type**（令牌已是 ECL 实体，spec §4 末行）⇒ 不吐道具，
 *                    改为在下一次召唤时沿用同一个合成色（`last_group` 已经这么记）。
 * ★★ 2026-09-21（P2 #5「UFO 应该爆**残碎卡**，而非残碎」）：**载体改成 TH18 自己的卡道具**
 *   `0x10` EXTEND2 / `0x11` BOMB2 —— 原来写的 `4` / `6` 是**别作的遗留道具槽**（TH18 的道具分派
 *   switch 里连 case 都没有，见 `AUDIT.md` §AL12d 与 `collect2x.c:342` 的「实跑纠偏」）。
 *   一手对照（TH12 `th12:0x44af00` 的颜色跳表 + `0x44aca0` 的 switch）：
 *     · 红 → 道具 **4**（命碎）/ 绿 → 道具 **5**（B 碎）—— 一手**只给了"碎片"这个语义**，
 *       TH18 侧承载「命碎 / B 碎」的原生 type 是 `0x10` / `0x11`（三条一手见 `signal_core.h`
 *       的「命碎的载体」段：dump 表 `th12` 同款的 `th18:0x4b4020` type `0x10/0x11` → card_id 3/4；
 *       113 `signal.c:581` 与 114 `collect2x_core.h:54-55` 已经这么落地并实跑过）。
 *   ⇒ 这**不是**「换个像的」：`0x10` 走引擎的卡道具链（`allocate_new_card` → `CardLifeFragment`），
 *     捡到就是**残碎卡**（3 个 = 1 残）；`4` 在 TH18 里根本不进道具分派。 */
#define UFO_REWARD_LIFE_FRAG  0x10 /* ✅ 一手语义「命碎」→ TH18 载体 = EXTEND2 卡道具（残碎卡）*/
#define UFO_REWARD_BOMB_FRAG  0x11 /* ✅ 一手语义「B 碎」→ TH18 载体 = BOMB2 卡道具（符卡碎片卡）*/
#define UFO_REWARD_NONE       0    /* 不掉 */
/* ★ 用户裁决 ①：**金与蓝的返回值必须逐格相同**（金 = 蓝的调色版）——
 * `ufo_spit_reward` / `ufo_break_color_bonus` 里金与蓝走**同一个 case**，单测把这条钉成对照表。 */
int32_t ufo_spit_reward(int32_t color);
/* 击破的颜色加成（`th12:0x44af00`，`UFO:234-268`）：红 → 额外一件命碎片；绿 → 额外一件 Bomb 碎片；
 * 蓝 / 彩虹 / 金 → 无额外。TH18 type 号同上面的映射候选。 */
int32_t ufo_break_color_bonus(int32_t color);

/* ============================ ★★ 掉落表：**每次击破/吸满都会返还一枚令牌**（本轮新增，一手）
 * 一手 = TH12 `Ufo::break` `th12:0x44af00` 的颜色跳表 `th12:0x44b590`（索引 = `color + 1`，
 * 见 `0x44b12d`–`0x44b13a`：`mov eax,[esi+0x30]; inc eax; cmp eax,4; ja …; jmp [eax*4+0x44b590]`）：
 *   | 索引 | 颜色 | 分支 | 赠送的**令牌**道具 |
 *   | --- | --- | --- | --- |
 *   | 0 | −1 彩虹 | `0x44b416` | `COLLECTED_UFOS[2]`（`0x4b0c54`）**减 1 后**跳表 ⇒ 1→13、2→14、3→15 |
 *   | 1 | 0（不可达）| `0x44b584` | 无 |
 *   | 2 | 1 红 | `0x44b141` | `0x44b23a push 0xd`  ⇒ **道具 13** |
 *   | 3 | 2 蓝 | `0x44b245` | `0x44b32a push 0xe`  ⇒ **道具 14** |
 *   | 4 | 3 绿 | `0x44b337` | `0x44b409 push 0xf`  ⇒ **道具 15** |
 *   ⚠ **每色恰好 1 枚（没有「掉两个飞碟」这回事）**：红/绿那两条各只有 **一次** 令牌 spawn
 *     （红的 `0x44b23a`、绿的 `0x44b409`），另一件是「加成道具」（红=4 残机碎片 / 绿=5 B 碎）。
 *   ⚠ 同时还会生成一件**赏金道具** 16/17/18（`0x44b0c3`/`0x44b0e2`/`0x44b0ff`）——那是**分数**道具，
 *     不是令牌；TH18 侧没有可用 type（`UFO_BOUNTY_TYPES_USABLE == 0`），本层只留判据。
 *   ⚠ 投出速度/角度：`0x44af00` 里传的是角度 `0x4a4060`(−π/2)、速度 `0x4a42c0`(2.2)，
 *     **但 `ItemManager__spawn_item` `th12:0x4273f0` 对 type 10..15 会无视这两个实参**
 *     （`0x4274b1`–`0x4274d1` 按「本关第几个令牌」的奇偶自己选角度、速度取 `0x4a4270`=1.5）
 *     ⇒ 令牌的实际初速见下面 `ufo_tok_motion_init()`（±45°、1.5）。
 * 吸满那条同表（`th12:0x44aca0` 的 `0x44ae03`：彩虹才吐令牌，红/绿吐 4/7、蓝什么都不吐）。
 * 返回值：**0/1/2 = 该令牌的「彩虹态」相位**（TH12 道具 13/14/15 三种显示态，脚本 178/179/180
 * ↔ 我们的 `ability.anm` 219/220/221）；**−1 = 不掉令牌**。 */
#define UFO_TOK_DROP_NONE (-1)
int32_t ufo_break_token_state(int32_t ufo_color, int32_t slot2_color);
/* 吸满（fill 到顶）时吐的令牌相位：**只有彩虹**有（`th12:0x44ada1`–`0x44ae03` 的 case −1）；
 * 红/绿走道具（`ufo_spit_reward`）、蓝什么都不吐（一手 switch 无 case 2）⇒ 其余一律 −1。 */
int32_t ufo_spit_token_state(int32_t ufo_color, int32_t slot2_color);

/* 击破**赏金**（`UFO:246-254`）：两个吸收计数决定生成哪一类「赏金道具」——
 * 一手是 16/17/18 三个 type（TH18 里 16 = type 0x10 = EXTEND2 **卡道具**、17/18 超出 0x13 上限，
 * 三格**全部不能用**）⇒ TH18 侧没有对应物，本实装**不生成赏金道具**，改为把
 * `cnt_p`/`cnt_money` 折算成**分数**（`ce_add_score_delta`）。
 * ★ 分值公式：**仓里没有一手**（spec §9 #5 明确「起手：`th12:0x44af00` 里写 `+0x68` 的那一段」）
 *   ⇒ 本实装**不编公式**，改用一个**具名占位权重**，默认 0 ⇒ 加 0 分（= 不做这个语义）。
 *   ⏳ 待一手查证后填 `UFO_BOUNTY_SCORE_PER_ITEM`。 */
#define UFO_BOUNTY_SCORE_PER_P     0   /* ★ TODO(一手 §9 #5)：每个吸收的 P 道具折算多少分。0 = 不折算 */
#define UFO_BOUNTY_SCORE_PER_MONEY 0   /* ★ TODO(一手 §9 #5)：每个吸收的金钱道具折算多少分。0 = 不折算 */
int64_t ufo_break_bounty_score(const ufo_ufo_t *u);
/* 「赏金道具该不该生成」这条一手规则本身（两个计数都 0 ⇒ 一件都不生成）——
 * 保留成纯逻辑，等 TH18 侧有可用 type 后接线（现在恒返回 0，因为上面三个 type 都不可用）。 */
#define UFO_BOUNTY_TYPE_BOTH   18   /* ✅ 一手：`cntA != 0 && cntB != 0`（`UFO:246`）—— ⚠ TH18 无此 type */
#define UFO_BOUNTY_TYPE_MONEY  17   /* ✅ 一手：只有 `cntB != 0` —— ⚠ TH18 无此 type */
#define UFO_BOUNTY_TYPE_P      16   /* ✅ 一手：只有 `cntA != 0` —— ⚠ TH18 里 0x10 = 卡道具，语义不符 */
int32_t ufo_bounty_item_type(const ufo_ufo_t *u);   /* 返回 18/17/16，两个计数都 0 ⇒ 0 */
/* TH18 侧能不能真的撒这三种（一手：`0x446f40` 开头 `type > 0x13 → return 0`；16 = 0x10 是卡道具） */
#define UFO_BOUNTY_TYPES_USABLE 0   /* ★ TH18 侧不可用 ⇒ 0（spec §2.2 末两行）*/

/* ================================================== ★★ 掉落物实体（2026-09-18 用户口径）
 * 两件东西共用同一套「自建掉落物」机制（用户问过"会不会小题大做"⇒ 答复：不，正是本卡该走的路）：
 *   · **压缩包**（`UFO_DROP_KIND_BAG`）—— 用户：「击破还会掉落一个**被自动吸收的压缩包**
 *     （注意，**应该是有实体的**，只是被自动吸收过来）」= 一手 TH12 的**赏金道具**（道具 16/17/18）；
 *   · **整颗 Bomb**（`UFO_DROP_KIND_BOMB`）—— 用户裁决 **P6**：绿 UFO 喂饱给整颗 Bomb（一手 = 道具 7）。
 * TH18 三个 type（16/17/18）全不可用（16 = 0x10 是 EXTEND2 卡道具、17/18 超 type 上限）⇒ **自建 VM**。
 * 单位一律**百万分比整数**（与 `fill_millionths` 同口径：本层不用 float 存状态，避免 x87 硬门）。 */
#define UFO_DROP_KIND_NONE 0
#define UFO_DROP_KIND_BAG  1
#define UFO_DROP_KIND_BOMB 2

/* ---- 一手运动（逐条地址见 `AUDIT.md` §DR；★ 2026-09-21 P2 #10 逐条 objdump 重读）----------
 * **压缩包 = 一手 state 8**（`th12:0x426189`–`0x426231`）：
 *   · **出生那一刻**：`ItemManager__spawn_item` `th12:0x4275e1`–`0x4275e7` 判 `type−16 <= 2`
 *     ⇒ `0x42761d mov [esi+0x9b0],8`（**state = 8**），`0x4276a4 fldz` → `0x4276f9 fstp [esi+0x984]`
 *     ⇒ **速度 = 0.0**、`0x4276ea mov [esi+0x98c],0` ⇒ **年纪 = 0**。
 *   · 每帧 **先位移、后加速**（顺序照抄）：
 *       位移 = `vel = polar(atan2(自机 − 道具), speed)`（`0x4261ad call 0x4377a0` = **朝自机**、
 *              `0x4261b7 call 0x427d00` = 极坐标→分量）、`pos += vel × GAME_SPEED`（`0x4261bc`–`0x4261f3`）；
 *       加速：`cmp [edi+0x98c],0x3c / jl 跳过`（`0x4261f6`，**年纪 < 60 不加速**）；
 *              `fld [0x4a404c]=12.0; fcomp speed; jne 跳过`（`0x4261ff`，**到 12.0 不再加**）；
 *              `speed += 0.2`（`0x426212 fadd [0x4a3fb8]`，double 0.2）。
 *   ⇒ 实现里写成「`age > 60` 才加速」（`age` 在 step 开头自增）⇒ 前 60 帧速度恒 0、原地不动，
 *     第 61 帧那一步先是"用 0 速度位移"、这一帧末才涨到 0.2 —— **与一手的先后顺序逐条相同**。
 *   · **没有 TTL、没有出界剔除**（只有「自机 state 4 ⇒ 清速度、转 state 1」那一条，本卡不采用）。
 *   ⇒ 观感 = 「**原地站 60 帧（1 s）→ 每帧 +0.2 加速到 12.0（再 60 帧到顶）**」。
 * ★★ **与 owner 记忆冲突（留档，别偷偷改）**：`SINGLE-CARD-TEST` §th12 #10 原话是
 *   「压缩包飞过来的速度太慢，**没记错的话应该是满速启动**」。**一手不是满速启动** ——
 *   speed 在 spawn 那一刻被**明确写 0**（`0x4276f9`），60 帧门（`0x4261f6`）又挡在加速前面。
 *   ⇒ 本实装**按一手**（同 `AUDIT.md` §DQ「对话期间」那条纪律：记忆与一手矛盾时按一手、
 *     差异登记、等 owner 裁决）。要改成「满速启动」= 把 `UFO_DROP_HOME_FULL_SPEED_START` 改 **1**
 *     （一行：起速直接 12.0、跳过 60 帧等待），登记在 `TUNING.md` §2.0r。
 * ⚠ 60 帧门读的是**物件自己的年纪**（`item+0x98c`）—— 同一格在状态 6 的「1200 帧后不反弹」里也读
 *   ⇒ 与社区口径「变色碎片 20 s 后不反弹」逐格对上（见本文件「令牌的运动」④）。
 * **整颗 Bomb = 普通道具**（一手 `spawn_item(type 7, angle −π/2, speed 2.2)`）：
 *   出生向上抛（vy = −2.2），之后每帧 `vy += 0.03`（`0x445c6d` 段的重力，见本文件头 ①）。 */
#define UFO_DROP_HOME_DELAY_FRAMES 60
#define UFO_DROP_HOME_ACCEL_MILLI  200      /* 0.2/帧 ×1000 */
#define UFO_DROP_HOME_SPEED_MAX_MILLI 12000 /* 12.0 ×1000 */
#define UFO_DROP_TOSS_SPEED_MILLI  2200     /* 2.2 ×1000 */
#define UFO_DROP_GRAVITY_MILLI     30       /* 0.03/帧 ×1000 */
/* ★❓ 自定开关（**默认 0 = 一手**）：1 = 「满速启动」—— 跳过 60 帧等待、起速就是 12.0。
 * 只有 owner 明确要按记忆版做时才翻（翻了要同步 `test_ufo_core` 里 `ufo_drop_step` 的用例）。 */
#define UFO_DROP_HOME_FULL_SPEED_START 0

typedef struct {
    int32_t  kind;              /* UFO_DROP_KIND_* */
    int32_t  alive;
    float    x, y;
    int32_t  vy_milli;          /* 整颗 Bomb 用（重力积分）；压缩包不用（它每帧重算方向）*/
    int32_t  speed_milli;       /* 压缩包的归航速度（一手 state 8 的那个 speed）*/
    uint32_t age;               /* 出生以来的帧数（速度门用它）*/
    /* —— 携带的账（压缩包用；一手：`item+0x9c8/0x9cc/0x9d0/0x9d4`）—— */
    int32_t  ufo_color;         /* `+0x9c8` = UFO 颜色（决定结算分支与倍率）*/
    int32_t  cnt_p;             /* `+0x9cc` = cntA（吸收的 P 道具数）*/
    int32_t  cnt_money;         /* `+0x9d0` = cntB（吸收的金钱数）*/
    int32_t  mult_permille;     /* `+0x9d4` = 倍率（由 `ufo_drop_mult_permille()` 算）*/
    /* ★ 用户 2026-09-18：被吸走的**信仰点**也要跟着回来（`NEXT.md` §1x.9）*/
    uint32_t faith_big, faith_mid;
    int32_t  faith_mask;
    /* ★★★ 2026-09-26（owner）：「压缩包的**等价计数** —— 我吃了这个等价于吃了多少个大信仰点 /
     *   中信仰点 / 樱点 / P / 金钱，其他卡要用」⇒ UFO 在场期间**原样分类**吞下的东西，随包携带。
     *   与 `faith_*` 那三格的区别：那三格是"要还回去的点"（共享点池实体），这五格是**折算给别人的账**。*/
    int32_t  eq_p, eq_money, eq_big, eq_mid, eq_cherry;
} ufo_drop_t;

/* 倍率（一手 `0x44b149` / `0x44b24d` / `0x44b33f` / `0x44b49c`，逐条）：
 *   红 = 火力满 2.0 / 否则 1.0；蓝 = fill≥1.0→8.0 / ≥0.5→6.0 / 否则 2.0+8·fill；
 *   绿 = 恒 2.0；彩虹 = fill≥1.0→4.0 / ≥0.5→3.0 / 否则 2.0+2·fill（fill 是 0..1）。
 * `power_full` = `[0x4b0c48] >= [0x4b0cd0]`（TH18 侧 = `CURRENT_POWER >= MAX_POWER`）；
 * `fill_millionths` = 0..UFO_FILL_UNIT。返回**千分点**（1000 = ×1.0）。 */
int32_t ufo_drop_mult_permille(int32_t ufo_color, int power_full, int32_t fill_millionths);

/* 结算值（一手三条式子，`0x426c9a` / `0x426cd2` / `0x426d27`）：
 *   · 红（1）        ：`trunc(cntA·PV·mult) + cntB·PV`（**跳过** /10×10 取整）
 *   · 彩虹（−1）     ：`trunc(PV·cntB·mult)` 再 `/10×10`
 *   · 蓝（2）/绿（3）：同彩虹那支
 *   下限统一 **10**（`0x426d81 mov esi,0xa`）。`pv` = 一手的 PV（= round10(PIV/100)）。
 * ⚠ 取整用**截断向零**（一手 `_ftol` 的快路径 `cvttsd2si`）。 */
int32_t ufo_drop_settle_value(int32_t ufo_color, int32_t cnt_p, int32_t cnt_money,
                              int32_t mult_permille, int32_t pv);
/* 一手 PV = `round10(PIV/100)`（`0x421880`：`/100` 再减 `%10` ⇒ **向下取到 10 的倍数**）。
 * TH18 侧的对位量 = `engine.h` 的 `CE_ADDR_MONEY_BASE`（身价，单位 = **点 × 100**）
 * ⇒ `pv = round10(money_base / 100)`（同一句"取到 10 的倍数"）⇒ 单位是**显示的点数**。 */
int32_t ufo_drop_pv_from_base(int32_t money_base);

/* 一步运动（**只改 `x/y/vy_milli/speed_milli/age`**）：
 *   · 压缩包：`age >= 60` 起加速（+0.2，上限 12.0）并**朝自机**走（比例步进，不过冲）；
 *   · 整颗 Bomb：`vy += 0.03` 重力积分（出生那一帧由调用方给 −2.2）。
 * 返回 1 = 还活着（调用方负责收集判定与回收）。 */
int  ufo_drop_step(ufo_drop_t *d, float px, float py);
/* 收集判定：到自机的距离² ≤ r²（r = 本作自己的收点半径，P5 同一口径）。 */
int  ufo_drop_within_pick(const ufo_drop_t *d, float px, float py, float r);

/* ================================================== §4 HP（硬约束 + 2026-09-21 的绕法）
 * 硬约束：`spawn+0x14 >= 1000` ⇒ `th18:0x42d8f6` 置 `flags_lo |= 0x40000000`
 *   （`test [enemy+0x635c],0xc000021` ⇒ 三处 C++ 伤害源**跳过这只敌人**：
 *    `0x40b531` / `0x40b7b1`（人偶 / 卡侧伤害）、`0x421e57`（灵梦炸弹）；见 `AUDIT.md` §BO-12 五）
 *   ⇒ **写进 spawn 的那个数**必须 < 1000。
 *
 * ★★ 2026-09-21（P2 #6「感觉飞碟太脆了」）：硬界只卡**spawn 那一刻**，不卡之后 ——
 *   一手（objdump th18.exe，本次复核）：
 *     · `0x42d815`–`0x42d82a`：`spawn+0x14` 被写进 `enemy+0x6220 / +0x6224 / +0x6228`
 *       （三格同一个数；`data.life` 的基址 = `CE_ENEMY_DATA(0x122c) + 0x4ff4`）；
 *     · `0x42d8ed`–`0x42d8fc`：`cmp [spawn+0x14],0x3e8` / `jl` 跳过 ⇒ **只有 spawn 那一刻**
 *       按「≥1000」置位 30。
 *   ⇒ 本实装：**spawn 写钳制值（≤999）**，敌机建出来后**再把真血量写回那三格**
 *     （`ufo_body_apply_real_hp()`，卡侧）。位 30 因此**不会**被置 ⇒ 人偶/炸弹伤害源照常打得中，
 *     而 UFO 拿到的是**一手原值** `{1600..2500}` —— 比原来的「缩到 999」厚 1.6–2.5 倍。
 *   ⚠ 这不是"发明数值"：数值就是一手表；绕的是**引擎那条只在 spawn 生效的黑名单闸**。 */
#define UFO_LIFE_LIMIT       1000   /* ⚠ 硬界：**spawn+0x14** >= 1000 进 C++ 伤害源黑名单（位 30）*/
#define UFO_LIFE_MAX_ALLOWED 999    /* spawn 值上限（= LIMIT − 1，照 108 `yk_clamp_spawn_life` 同一口径） */
/* `ufo_clamp_spawn_life` 的**输出上界**（单独一个常量，好让卡侧写一条
 * `_Static_assert(UFO_CLAMP_OUTPUT_MAX < UFO_LIFE_LIMIT)` —— 否则只断言输入常量的话，
 * 把钳制函数改成返回 >= 1000 那几条断言照样过）。 */
#define UFO_CLAMP_OUTPUT_MAX UFO_LIFE_MAX_ALLOWED
int32_t ufo_clamp_spawn_life(int32_t life);

/* ---- ★★ HP = 一手表 × 火力档（用户 2026-09-18 裁决 **P4**：「**HP 一手确实是按火力**」）-------------
 * 一手（`th12:0x4b33f0`）：`{1600, 1900, 2100, 2300, 2500}`，索引 = `CURRENT_POWER / POWER_PER_LEVEL`
 * （0..4 档）⇒ **复刻**这张表与它的索引方式。**原值直接用**（不再缩放）：
 *   `ufo_hp_for_power()` = 真血量；`ufo_clamp_spawn_life()` = 写进 `spawn+0x14` 的那个值（≤999）；
 *   两者在 `ufo.c` 的 `ufo_try_summon()` 里分两步落笔（spawn 后回写 `enemy+0x6220/24/28`）。
 * ⚠ 档间比例与一手**逐档相同**（1600/2500 = 0.64 …）—— 因为它就是一手表本身。
 * ⏳ 「TH18 的每帧伤害上限（`sht+0x28`：灵梦 90 / 魔理沙 160，`engine/sht/th18/01` §5）比 TH12 大」
 *   这一层**没有做缩放**（一手只给了 TH12 的量纲，缩多少要靠 owner 拍）⇒ 真血量就是一手原值；
 *   若实跑仍嫌脆，`UFO_HP_ONEHAND_SCALE_PERMILLE` 是**唯一的旋钮**（默认 1000 = 不缩放，
 *   已登记 `TUNING.md`）。 */
#define UFO_HP_TABLE_N        5      /* 一手：五档（0..4 = CURRENT_POWER / POWER_PER_LEVEL）*/
#define UFO_HP_MAX_ONEHAND 2500      /* ✅ 一手：表里最高档（`th12:0x4b33f0`）*/
#define UFO_HP_ONEHAND_SCALE_PERMILLE 1000  /* 🟡 旋钮：1000 = 一手原值（唯一的缩放点）*/
extern const int32_t k_ufo_hp_by_power[UFO_HP_TABLE_N];
/* 火力档 → **真血量**（一手表 × `UFO_HP_ONEHAND_SCALE_PERMILLE`；默认即一手原值）。
 * `per_level <= 0`（引擎还没初始化）⇒ 取最低档。档位夹到 0..4。**纯整数**（本层零浮点）。*/
int32_t ufo_hp_for_power(int32_t power, int32_t per_level);
/* 火力 → 档位（0..4，夹取）；日志与单测共用，免得两处各写一次除法。 */
static inline int32_t ufo_hp_tier(int32_t power, int32_t per_level)
{
    int32_t level = (per_level > 0) ? power / per_level : 0;
    if (level < 0) level = 0;
    if (level >= UFO_HP_TABLE_N) level = UFO_HP_TABLE_N - 1;
    return level;
}
/* 该档的**一手原值**（未缩放；日志/对账用）。 */
static inline int32_t ufo_hp_onehand_for_tier(int32_t tier)
{
    if (tier < 0) tier = 0;
    if (tier >= UFO_HP_TABLE_N) tier = UFO_HP_TABLE_N - 1;
    return k_ufo_hp_by_power[tier];
}
/* 血条用：`cur / max` 的**千分比**（0..1000），纯整数（本层不用浮点，理由见 `fill_millionths` 的注）。
 * `max <= 0` ⇒ 0（不给除零）；`cur < 0` ⇒ 0；`cur > max` ⇒ 1000（引擎在别处可能超上限）。
 * 卡侧 HUD 的第一版血条（用户 2026-09-12 裁决 ④）就用它把本体 HP 画成进度条。 */
int32_t ufo_hp_permille(int32_t cur, int32_t max);

/* ================================================= §3.2 令牌载体（★ 2026-09-12：**自建 VM 池**）
 * ★★ 用户 2026-09-12 拍板：「**token 得是自建 VM 池子**」—— 令牌**不再是真敌人**。
 *
 * 为什么换（三条代价，前两条是可观测的 bug 级问题；依据见 `AUDIT.md` §BO-11 的 P3）：
 *   ① 一手 TH12 的令牌是**道具**（type 10..15，`th12:0x4121e0` 的 `EnemyDrop__spawn_ex_drops`）：
 *      **打不掉、只能接触收**。我们那会儿用「真敌人壳 + `UFO_TOKEN_LIFE` = 1」⇒ **任何一发子弹
 *      都能把令牌「收」走** ⇒ 「碰到才收」形同虚设、风险收益手感被抹掉。
 *   ② 真敌人壳会被 `enemy_count_real` 照计 ⇒ **炸弹门 `0x420453` / 决死门 `0x45c07b` 被放行**
 *      （`AUDIT.md` §BO-3：空场也能放炸弹/决死）。
 *   ③ 壳本身是多余的：贴图早就由卡侧的 anm VM 画、位置也是卡侧每帧写的
 *      ⇒ 壳只剩「占位」，却带来上面两条 + 每次 spawn 的分配成本 + 对一个占位 ECL 名的依赖。
 * 先例（同仓已有两处，照着抄）：117 `animal.c` 的 `s_tok[40]`、113 `trance.c` 的 `s_spirit[128]`
 *   —— 文件级静态句柄表 + `ce_anm_spawn` / `ce_anm_delete`，**没有实体**。
 *
 * ⇒ 本层里与「真敌人」绑在一起的东西**一条都不留**：占位 ECL 名常量 / 令牌 HP 常量 /
 *   「令牌被子弹打掉也算收」那个判据函数（`ufo_token_kill_counts`）全部删除。
 *   ⚠ **令牌的来源不动**：仍在**小怪击破时**登记、帧末生成（`ufo.c` 的 `on_enemy_death` ③）。
 *   ⚠ **UFO 本体不动**：它有血量 / 受击盒 / 要被子弹打死 ⇒ 仍然是 ECL 真敌机。
 * UFO **本体**的子程序名也不在这里 —— 它按颜色分 4 件（一手名字表 `th12:0x4b33e0` =
 * {`UFO_Red`, `UFO_Blue`, `UFO_Green`, `UFO_Rainbow`}，索引 = 上面的 `ufo_color_art_index()`），
 * 具名表放卡侧 `ufo.c`（`UFO_ECL_BODY_RED/BLUE/GREEN/RAINBOW`，★ **金借蓝那一件** = 用户裁决 ①）。 */
/* ★ 2026-09-24（owner 第 14 轮）：**3 → 6**。理由：宿主改成"每波只挑 1~2 只"之后，一次宿主击破掉 **2 枚**
 *   （owner 第 11 轮口径），一波两只宿主就是 4 枚 —— 池 3 会当场拒掉后面的（实跑就是"打了不掉"）。
 *   一手 TH12 的令牌池是 **16 格**（`th12:0x427402` 起 0x9d8 步长的 16 槽），所以我们这 3 本来就只是
 *   ❓ 自定的小上限；要回退就改回 3（一行）。 */
#define UFO_TOKEN_MAX       6      /* ★❓ 自定（spec §3.2 ⏳「令牌实体的出现规则」待拍）：同时最多几个令牌。
                                    * ⚠ 占位 1 会让「凑齐 3 个」几乎不可能 ⇒ 取 3。
                                    * （一手的道具池是 16 格 —— 见 `AUDIT.md` §BO-11；我们的池容量
                                    *  `UFO_TOKEN_CAP` 是「同时活几个 + 余量」，判据在下面那个
                                    *  `ufo_token_pool_room()`，卡侧表用的是它。）*/
#define UFO_TOKEN_TTL_FRAMES 2400  /* ★❓ 自定：令牌的存活帧上限（到点自己静默撤）。换关时随 run 复位。
                                    * ⚠ 与一手的 1200 帧门（`UFO_TOK_NO_BOUNCE_FRAMES`）**不是一回事**：
                                    *   一手 1200 帧之后只是「不反弹 + 出界即删」，令牌自己飞出去就没了；
                                    *   我们的 VM 不会自己飞出去 ⇒ 留一个更长的 TTL 兜底（防漏 VM）。
                                    * ★ 换池之后这条 TTL 的语义**没变**（仍然只撤「自己」），只是载体从
                                    *   敌机换成了 VM；它仍然是**唯一**会给令牌设寿命的那条路。 */
/* 令牌实体的出生点（自定）：横行在活动框内、纵在框上沿附近 —— 框一手 = TH12 `moveLimit(0,128,280,128)`
 * ⇒ x ∈ [−140,140]、y ∈ [64,192]（spec §1 表末行「照抄成自写 ECL 的飘行框」）。
 * 占位期由卡侧按 `enemy_id` 确定性散列落点（**不调 `ce_rand()`**：死亡回调里不引入随机流）。 */
#define UFO_FRAME_HALF_W    140.0f  /* ✅ 一手：280×128 的活动框半宽（`th12:0x4174fd`，spec §1） */
#define UFO_FRAME_Y_TOP      64.0f  /* ✅ 一手：框上沿 y */
#define UFO_FRAME_Y_BOTTOM  192.0f  /* ✅ 一手：框下沿 y */

/* ================== ★★ 令牌的**运动**（用户口述「飞碟在场上…飞行」= 这一条）
 * 一手全在 TH12 `ItemManager__on_tick__body`（`th12:0x425c20`）的 **state 6**（令牌道具专用状态）里：
 *
 * ① **初速**（`th12:0x4273f0` 的 `0x4274b1`–`0x4274ef`，type 10..15 专用，**无视调用方传的角度/速度**）：
 *      `mov edi,[ItemManager+0x666fe0]`（= 本关第几个令牌，`0x427411` 处 `inc`）
 *      `and edi,1` ⇒ 奇偶选角度：偶 → `fld [0x4a4274]` = **0.785398185f = π/4（右下 45°）**
 *                               奇 → `fld [0x4a4278]` = **2.3561945f = 3π/4（左下 45°）**
 *      速度 = `fld [0x4a4270]` = **1.5f**（常量，与角度一起喂 `0x427d00` 算 `vel`）
 *    ⚠ 因为 `inc` 在奇偶判断**之前** ⇒ 本关第 1 枚令牌拿到的是**奇** = 3π/4 = 左下 45°（与社区表一致）。
 * ② **每帧位移**：`pos += vel`（`0x4263a1`–`0x4263cb`；`0x42637b` 的 `ds:0x4b2ed0` 是 GAME_SPEED 标量指针）。
 * ③ **反弹**（`0x4263e1`–`0x42650d`，四条各带「速度朝向正确才翻」的守卫，反射因子 = `[0x4a4168]` = **double −1.0**）：
 *      `x <= [0x4a440c]`(−176) 且 `vel.x < 0` ⇒ `vel.x *= −1`（`0x426423`）
 *      `x >= [0x4a4198]`(176)  且 `vel.x > 0` ⇒ `vel.x *= −1`
 *      `y <= [0x4a3f4c]`(160)  且 `vel.y < 0` ⇒ `vel.y *= −1`（`0x4264fb`）
 *      `y >= [0x4a4408]`(370)  且 `vel.y > 0` ⇒ `vel.y *= −1`
 *    ⚠ 单位 = TH12 的道具/ECL 空间（活动区 x ∈ [−192,192]、y ∈ [0,448]）⇒ 反弹发生在**离左右版边 16、
 *      上 160、下 370** 处（社区描述「左右版边 / 板中偏上 / 自机复活线上方」✅）。
 *      ★★ 2026-09-12 **换池复核**：这套常量**就直接写在我们自己的坐标字段上**，不做任何换算 ——
 *      TH12 的道具坐标与 TH18 的**世界层 VM 实体坐标**是同一个 ECL 640 空间
 *      （一手：`vm+0x5f0` = ECL 坐标、1 单位 = 2 表面 px、游玩区 x ∈ [−192,192] y ∈ [0,448]，
 *       见 `engine/anm/th18/01-vm-instantiate.md` §3）⇒ 量纲一致。
 *      ⚠ 一手的**活动框**在 TH12 由道具管理器管；我们的令牌是**自建 VM**（§3.2）⇒ 位置/速度**只**存在
 *      我们自己的结构里（`ufo_tok_motion_t` 的 `x/y/vx/vy`），**不碰任何敌人字段**。
 * ④ **1200 帧之后不再反弹**（`0x4263d1 cmp [edi+0x98c],0x4b0; jge 0x42650f` ⇒ `0x42650f` 只判出界不判反弹），
 *    出界即撤：外框 = `x ≤ [0x4a4400]`(−208) / `x ≥ [0x4a43a0]`(208) / `y ≤ [0x4a43f8]`(−16) /
 *    `y ≥ [0x4a3e90]`(464) ⇒ `0x42655b mov [edi+0x9b0],0`（state 0 = 回收）。
 *    ★ 社区说的「变色碎片变色 8 次后不再反弹 / 固定碎片 20 s 后不再反弹」= **同一个 1200 帧门**
 *      （1200 ÷ 150 帧换色 = 8 次）。两种说法同源 ✅。
 * ⑤ 6/7 状态的取舍见 `ufo_step_allowed` 一族；令牌的**收取**（自机碰到）在 TH12 是
 *    「道具位置落进自机的收点矩形 `player+0xc444..`」（`0x426100`/`0x426131`/`0x426144`/`0x426157` 四次比较），
 *    该矩形的半尺寸来自玩家表 `0x4b31b4` = {60,70,60}（按机体，× 0.5 × 0.5 ⇒ 半宽 **15 单位**）——
 *    ⚠ 我们的令牌不走引擎那条通路 ⇒ 卡侧只能自算距离（见 ufo.c 的 `UFO_TOKEN_PICK_DIST`）。 */
#define UFO_TOK_FLY_SPEED   1.5f         /* ✅ 一手：`0x4a4270` */
#define UFO_TOK_ANGLE_DOWN_RIGHT 0.785398185f  /* ✅ 一手：`0x4a4274` = π/4（右下方）*/
#define UFO_TOK_ANGLE_DOWN_LEFT  2.3561945f    /* ✅ 一手：`0x4a4278` = 3π/4（左下方）*/
/* cos/sin(±45°) × 1.5 —— 不用 libm（本层不链数学库，见 `ufo_drag_step` 那段注的同一理由）。 */
#define UFO_TOK_VEL_COMP    (UFO_TOK_FLY_SPEED * 0.70710678f)   /* = 1.060660124f */
#define UFO_TOK_BOUNCE_X    176.0f       /* ✅ 一手：`0x4a4198` / 左边界 `0x4a440c` = −176 */
#define UFO_TOK_BOUNCE_Y_TOP    160.0f   /* ✅ 一手：`0x4a3f4c` */
#define UFO_TOK_BOUNCE_Y_BOTTOM 370.0f   /* ✅ 一手：`0x4a4408` */
#define UFO_TOK_OUT_X       208.0f       /* ✅ 一手：`0x4a43a0` / `0x4a4400` = −208（外框，出界即撤）*/
#define UFO_TOK_OUT_Y_TOP   (-16.0f)     /* ✅ 一手：`0x4a43f8` */
#define UFO_TOK_OUT_Y_BOTTOM 464.0f      /* ✅ 一手：`0x4a3e90` */
/* 出生点夹取（一手 `th12:0x4273f0` 的 `0x42747d`–`0x4274b1`：x 夹 ±192（`0x4a3d88`/`0x4a3d98`）、
 * y 不小于 0（`fldz; fcom …`））—— 令牌从敌人死亡点掉出来，别落在版外。 */
#define UFO_TOK_SPAWN_CLAMP_X 192.0f     /* ✅ 一手：`0x4a3d88` / `0x4a3d98` */
#define UFO_TOK_NO_BOUNCE_FRAMES 1200    /* ✅ 一手：`0x4263d1` 的 `0x4b0`（= 20 s = 8 次换色）*/

/* 一枚令牌的**运动状态**（= 它的**位置与速度**，纯数据）。
 * ★★ 2026-09-12 换池之后**这两个坐标就是我们自己的字段**（以前是每帧读/写敌机的
 *   `CE_ENEMY_POS_X/Y` 与 `enemy+0x12b4` 的 `abs_pos`）⇒ 本结构体是那一枚令牌**唯一的**坐标真相。
 *   具体怎么接到 VM 上（`ce_anm_set_pos`）由卡侧 `ufo.c` 的令牌池负责；本层只管算。
 * ⚠ 池深 × 这个结构体（实测 `sizeof` = 20 B × 池 5 = 100 B，再加卡侧那一层
 *   `ufo_tok_t` 的其它字段）⇒ 卡侧把**整张令牌池**放**文件静态区**（`ufo.c` 的 `s_tokpool`，
 *   理由与先例 `trance.c` 的 `s_spirit[128]` / `animal.c` 的 `s_tok[40]` 同一套），**不进卡私状态**
 *   （`CE_STATE_BYTES − CE_STATE_RESERVED` = 496 B 的预算）。 */
typedef struct {
    float   x, y;     /* 当前位置（ECL 640 空间 = 世界层 VM 的实体坐标；1 单位 = 2 表面 px）*/
    float   vx, vy;   /* 每帧位移（单位同上）*/
    int32_t age;      /* 出厂帧数（一手：>= `UFO_TOK_NO_BOUNCE_FRAMES` 之后不反弹）*/
} ufo_tok_motion_t;

/* 令牌池的容量判据（**上限只有这一处定义**：纯逻辑层给判据、卡侧表用同一个宏）。
 * `UFO_TOKEN_MAX` 是「同时活几个」（玩家手感上限），容量判据要再留一点余量给同帧的边界状态
 * ⇒ 池 = `UFO_TOKEN_MAX` + `UFO_TOKEN_POOL_SLACK`。⚠ 池是文件静态的**定长数组** ⇒ 判据必须
 * 在写表之前问（`ufo.c` 的 `ufo_tok_free()` 与它对账，越界就是内存写坏）。 */
#define UFO_TOKEN_POOL_SLACK 2
#define UFO_TOKEN_CAP (UFO_TOKEN_MAX + UFO_TOKEN_POOL_SLACK)
static inline int ufo_token_pool_room(int live)
{
    return live < 0 ? 0 : (live < UFO_TOKEN_CAP ? UFO_TOKEN_CAP - live : 0);
}

/* 初始化（角度按「本关第几个令牌」的奇偶交替；`serial` = 0 基的令牌序号 ⇒ 第 1 枚 = 左下 45°）。
 * `x/y` = 出生点（一手：那只敌人的**死亡坐标**，已在卡侧夹取过）。全字段都写（不留半初始化状态）。 */
void ufo_tok_motion_init_at(ufo_tok_motion_t *m, uint32_t serial, float x, float y);
/* 走一帧：位移 + 反弹 + 出界判定。返回非 0 = **该令牌该被撤掉**（一手：出界即 state 0）。
 * 指针入参、整型返回 ⇒ 不产生 x87（`dllx87` 门）。 */
int  ufo_tok_motion_step(ufo_tok_motion_t *m);

/* ★★ 2026-09-23（owner 实跑第 9 条）：「**进入对话后，场上的 token 应当不再反弹，直接飞走**」。
 * 一手没有「对话」这条支（TH12 的令牌是普通道具，只按 1200 帧那条规则停止反弹）；
 * ⇒ 本函数把「停止反弹」这件事**提前**：`age` 直接顶到 `UFO_TOK_NO_BOUNCE_FRAMES`（⇒ 不再反弹，
 * 出界判定照旧把它回收），并把速度**放大 `UFO_TOK_LEAVE_SPEED_MUL` 倍**（❓自定：owner 要的是"直接飞走"，
 * 不是"慢慢飘出去"）。幂等（已经进入离场态的令牌不再加速）。 */
#define UFO_TOK_LEAVE_SPEED_MUL 2.0f
void ufo_tok_motion_leave(ufo_tok_motion_t *m);

/* ================================================= §3.4 伪磁场扫描（成本记账）
 * 池 = `CE_ITEM_SLOTS` 4696 槽 × stride 0xc94（`engine.h:426-427`）。本层的早停判据：
 *   ① 只在「UFO 在场且已过 60 帧暂停」时扫（`ufo_can_absorb`）；
 *   ② state 3 / 4 直接跳过（引擎自己的真空/环吸）；
 *   ③ type 不是 1 / 2 直接跳过（先读 type 再算几何 ⇒ 非 P/钱道具零几何开销）。
 * ⚠ 没有 exe 就别编帧耗 ⇒ ufo.c 里只记「扫了几槽 / 拉了几颗 / 跳了几颗」，实测帧耗**待实跑**。 */
#define UFO_ABSORB_TYPES_N 2
int ufo_absorb_type_index(uint32_t type);   /* 1 → 0、2 → 1、其它 → −1（早停用） */

/* ============================================ 死亡事件过滤（DEATH §1.3 / spec §7）
 * 本层不 include sdk/public.h ⇒ 自己钉一遍 kind 常量（值来自 DEATH §2.1，
 * 编译期由 ufo.c 的 `_Static_assert` 对账）。 */
#define UFO_EDEATH_KILLED    0u
#define UFO_EDEATH_BOMB_KILL 1u
#define UFO_EDEATH_ECL_DIE   2u
#define UFO_EDEATH_OTHER     3u
#define UFO_EDEATH_KILL_ALL  4u
/* 哪里用：★ 2026-09-12 **换池之后只剩一处** —— 令牌的**来源**门（小怪击破时登记，`ufo.c` 的
 * `on_enemy_death` ③；一手 `dropExtra(10..12,n)`，op 407 handler `th12:0x4179cc`）。
 * 认哪几种死因：被打死 / 炸弹秒杀 —— ★❓ 自定（spec §9 #13「`BOMB_KILL` 算不算击破」未拍板）。
 * 取「算」；`KILL_ALL` **一律不算**（过场清场会把计数器刷满，spec §7 AUDIT 硬要求）。
 * ⚠ 这里**没有** `ufo_token_kill_counts()` 了：那条路是「**令牌自己被子弹打掉也算收**」——
 *   它是**真敌人载体**的产物（令牌有 HP 才会死），换池之后令牌根本没有死亡事件
 *   ⇒ 用户拍板的「token 得是自建 VM 池子」把整条路删掉了（`AUDIT.md` §BO-11 的 P3 代价①）。 */
int ufo_body_kill_counts(uint32_t kind);
