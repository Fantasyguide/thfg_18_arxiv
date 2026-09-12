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

/* ---- 占位期令牌颜色的**轮转表**（放在纯逻辑层，好让主机单测能查它）------------------
 * ★❓ 自定：TH18 侧「谁产令牌、什么颜色」没有一手（spec §9 #12）⇒ 占位期由卡侧按「本局第几个令牌」
 * 在下面这张表里轮转。⚠ 表里必须全是**合法枚举值** —— 踩过一次「把 [0,1] 的散列 cast 成 int」，
 * 结果令牌颜色恒为 0（`UFO_COLOR_NONE`）⇒ 令牌对所有代码路径不可见、整张卡等于死的
 * （测量记录见 `docs/superpowers/specs/2026-09-12-ufo-112-impl.md` §2.4）。
 * ★★ **表的顺序/长度本身也是设计**（踩过第二个坑，见 impl.md §2.5）：原作四种颜色是
 * `1 红 / 2 蓝 / 3 绿 / −1 彩虹`，若就按这个顺序排成**周期 4**（`红 蓝 绿 彩虹`），
 * 那么**连续 3 个 serial 永远互不相同**（4 个互异值上的连续段）⇒ **每一次召唤都是彩虹 UFO**，
 * 红/绿的吸满奖励与击破加成**永远走不到**（死代码）。
 * ⇒ 表长 **7**、顺序如下（穷举 4^4..4^8 得到的：**周期 ≤ 6 无解**，7 是最短的能全覆盖的长度；
 *   周期 7 共有 168 张可用表，本表取其中一张）：
 *   `绿 彩虹 彩虹 蓝 红 红 红` ⇒ 6 个滑动窗口分别产出
 *   `绿 / 蓝 / 彩虹 / —(不成组) / 红 / 红 / 彩虹` —— **三条同色规则 + 三互不相同 + 不成组全都出现**。
 * 🟡 代价（❓ 自定手感）：这张表里红占 5/7 ⇒ 红 UFO（命碎片）偏多、绿/蓝各 1 次。比例是待调的。 * 🟡 比例与顺序是 ❓ 自定手感值（不改变「三种同色规则都够得着」这条），待用户/实跑调。 */
#define UFO_TOKEN_COLOR_ROTATION_N 7
extern const int32_t k_ufo_token_color_rotation[UFO_TOKEN_COLOR_ROTATION_N];
static inline int32_t ufo_token_color_for_serial(uint32_t serial)
{
    return k_ufo_token_color_rotation[serial % (uint32_t)UFO_TOKEN_COLOR_ROTATION_N];
}

/* ---- ★❓ 自定：**金令牌的出现规则**（一手没有金色；spec §2.1 #3「金 = ❓ 自定」）---------
 * 金 ≡ 蓝（`UFO_GOLD_COUNTS_AS_BLUE`）⇒ 金只可能替换**轮转表里轮到「蓝」的那一格**，
 * 且每隔 `UFO_GOLD_CYCLE` 个轮转周期才替换一次（`1` = 每个周期都替换 ⇒ 蓝色令牌再也不出现，
 * 那就等于把蓝整条换掉了，不符合「金 = 蓝的调色版」这个说法）。
 * 取 2 ⇒ 表里的蓝在周期 1/3/5… 出金、周期 0/2/4… 出蓝 ⇒ 两种都能在实机上看到。
 * ⚠ 这是 **❓ 自定手感/美术口径**，**不改变任何行为**（金进槽即归一成蓝）。
 * TODO(资产/ECL §6 步 3)：真 ECL 到位后颜色由**实体自己携带**（`$I0`），这一段整段删掉。 */
#define UFO_GOLD_CYCLE 2
int32_t ufo_token_color_for_serial_gold(uint32_t serial);

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
     * （用户口述口径），那时 `slot[2]` 就没了 ⇒ 必须在 `ufo_group_consume()` 里快照一份。
     * ⚠ 一手的槽是**留到 UFO 结束时才清**（`th12:0x44a326`–`0x44a344`）⇒ 它随时读得到；
     *   我们提前清空 ⇒ 只有这一格快照。差别与依据见 `ufo_group_consume()` 的注。 */
    int32_t last_slot2;
} ufo_group_t;

void    ufo_group_reset(ufo_group_t *g);
/* 放一个令牌进**第一个空槽**（**不过 `ufo_color_norm()`**，桶归并在 classify 里做）。
 * 无空槽 / 颜色非法 ⇒ 返回 0 且不改状态。
 * 返回 1 = 已放入；此时若 `g->filled == UFO_SLOT_COUNT` 就该调 `ufo_group_classify()`。 */
int     ufo_slot_set(ufo_group_t *g, int32_t color);

/* ---- 入槽的**唯一推荐入口**（把「槽满怎么办」这条决策收在纯逻辑层，可单测）----
 * `ufo_group_put` 的返回码（刻意与卡侧同名，卡侧直接 `#define` 转发）。 */
#define UFO_GROUP_PUT_NONE  0   /* 没收下（颜色非法 / 写不进去）*/
#define UFO_GROUP_PUT_HELD  1   /* 收下了，但**没成组**（槽没满，或满了不成组⇒已当场丢掉最旧）*/
#define UFO_GROUP_PUT_READY 2   /* 收下了且**成组**⇒ 调用方去 `ufo_group_consume()` + 召唤 */

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

/* ---- ★ 用户 2026-09-12 裁决 ③ 的**触发条件**（纯逻辑；美术待做，卡侧只有预留钩子）----
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
int32_t ufo_group_consume(ufo_group_t *g);
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
} ufo_ufo_t;

void     ufo_reset(ufo_ufo_t *u);
/* 召唤一个 UFO（花色 → 清零两个计数与 fill）。返回 1 = 成功（有 ufo_id）。 */
int      ufo_spawn(ufo_ufo_t *u, uint32_t enemy_id, int32_t color);
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
/* 从道具指向 UFO 的**未归一化**位移分量。返回 0 = 重合/非正距离（没有方向）。 */
int   ufo_drag_delta(float ix, float iy, float ux, float uy, float *out_dx, float *out_dy);
/* 往 UFO 走一步：**按距离的比例**逼近（`d > speed` 时每帧正好走 `speed`；`d <= speed` 时这一帧
 * 正好压在 UFO 上，绝不越过）。
 * ★ 为什么不归一化：`speed` 是从 TH12 的「速度」公式抄来的（spec §4），但 TH12 那套是引擎给道具
 * 单独一条「飞向 UFO」状态、由引擎自己做 `pos += vel × GAME_SPEED`（`th12:0x426879`）；
 * TH18 没有这个状态（`PLAN` §2.3），我们只能直写 `pos` ⇒ 必须自己保证**不过冲**，否则道具会在
 * UFO 两侧来回跳。比例步进天然满足这一点，且不需要 sqrt（本层不链 libm）。 */
void  ufo_drag_step(float *px, float *py, float ux, float uy, float speed);

/* ---- ★★ 围栏三件（spec §3.4「少一样就会出问题」）----------------------------------
 * ① **目标点必须留在屏内**：引擎当帧就删。一手（本次实装新增一手复核，见 ufo.c 头）：
 *    `0x445e38` 的删除判定读 `0x4b93a4` = **472.0f**（`item+0xc30 > 472` ⇒ 删）与
 *    `0x4b9340` = **200.0f**（`|item+0xc2c| >= 200` ⇒ 删；`0x445e28` 的 `andps` 是取绝对值）。
 * ② **目标点离自机必须 > 30 px**：否则引擎的收点检查（`0x44625f`，半径 `player+0x4798c` = 30.0f）
 *    当帧把它收掉，我们白吸。
 * ③ **跳过 state 3 / 4**：那是引擎自己的真空飞行 / 70 px 环吸，别抢。 */
int ufo_target_on_screen(float x, float y);
int ufo_target_clear_of_player(float x, float y, float px, float py);

/* 给定「UFO 在 (ux,uy)、自机在 (px,py)」这一帧，往目标点走一步是否合法（把三件栅栏一次问完）。
 * `want_absorb` = 本次是不是已经到捕获半径（那就不再看栅栏，直接收）。 */
int ufo_step_allowed(float nx, float ny, float px, float py);

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

/* ---- 吸满吐奖励（表抄，type 号映射到 TH18）------------------------------------------
 * 一手（`th12:0x44aca0`，`UFO:213-222`）：红 → 道具 4；绿 → 道具 5；彩虹 → 13/14/15；
 * **蓝 → switch 无 case 2 ⇒ 什么都不吐**（spec §9 #3 未拍板「复刻还是修正」⇒ 本实装**复刻**）。
 * TH18 的 type 号**不能照抄**（上限 0x13，0x10–0x13 是卡道具；spec §2.2 末两行）⇒ 映射表：
 *   红 4 → **TH18 type 4**（命碎片；`0x4463c6` 的 case 4 调 `0x457570` = 命碎累加，一手）
 *   绿 5 → **TH18 type 6**（符卡碎片：`BOMB_FRAGMENTS += 1`，`SHOP:304`；跳表一手）
 *   彩虹 13/14/15 → **TH18 没有对应原生 type**（令牌已是 ECL 实体，spec §4 末行）⇒ 不吐道具，
 *                    改为在下一次召唤时沿用同一个合成色（`last_group` 已经这么记）。
 * ★ 以上「红→4 / 绿→6」是 **spec §4 的候选建议（⏳ 待拍）**，不是复刻。 */
#define UFO_REWARD_LIFE_FRAG  4    /* ★⏳ spec §4 候选：红 UFO 吸满 → 命碎片（TH18 type 4，一手语义）*/
#define UFO_REWARD_BOMB_FRAG  6    /* ★⏳ spec §4 候选：绿 UFO 吸满 → 符卡碎（TH18 type 6，一手语义）*/
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

/* ================================================== §4 HP（硬约束）
 * 硬约束：`spawn+0x14 >= 1000` ⇒ `0x42d8f6` 置 `flags_lo |= 0x40000000`（锁定黑名单，
 * `engine.h` 的 `CE_SPAWN_LIFE` 注）⇒ 自建载体一律 < 1000。
 * 原作 HP 是 `{1600,1900,2100,2300,2500}`（按火力档，`th12:0x4b33f0`）—— **照抄会进黑名单**（spec §9 #4）。
 * ⇒ 本实装给**具名占位** + 钳制帮手，绝不自己发明「缩几倍」。 */
#define UFO_LIFE_LIMIT       1000   /* ⚠ 硬界：>= 1000 进锁定黑名单 */
#define UFO_LIFE_MAX_ALLOWED 999    /* 钳制上限（= LIMIT − 1，照 108 `yk_clamp_spawn_life` 同一口径） */
/* `ufo_clamp_spawn_life` 的**输出上界**（单独一个常量，好让卡侧写一条
 * `_Static_assert(UFO_CLAMP_OUTPUT_MAX < UFO_LIFE_LIMIT)` —— 否则只断言输入常量的话，
 * 把钳制函数改成返回 >= 1000 那几条断言照样过）。 */
#define UFO_CLAMP_OUTPUT_MAX UFO_LIFE_MAX_ALLOWED
#define UFO_HP_PLACEHOLDER   100    /* ★❓ 自定（spec §9 #4「HP 缩法」未拍板）：占位 HP。
                                     * 一手值 1600..2500 照抄会进黑名单；缩法没有一手 ⇒ 取一个小值占位。
                                     * ⚠ 代价必须知道：< 1000 ⇒ **没有原生血条**（一手血条的触发位是
                                     * `flags_lo |= 0x40000000`，正是我们躲开的那一位，spec §4）。 */
int32_t ufo_clamp_spawn_life(int32_t life);
/* 血条用：`cur / max` 的**千分比**（0..1000），纯整数（本层不用浮点，理由见 `fill_millionths` 的注）。
 * `max <= 0` ⇒ 0（不给除零）；`cur < 0` ⇒ 0；`cur > max` ⇒ 1000（引擎在别处可能超上限）。
 * 卡侧 HUD 的第一版血条（用户 2026-09-12 裁决 ④）就用它把本体 HP 画成进度条。 */
int32_t ufo_hp_permille(int32_t cur, int32_t max);

/* ================================================= §3.2 令牌载体（ECL 真敌机）
 * 载体 = ECL 真敌机（spec 用户定案 ②，先例 = 零售 `CardLilyWhite` `0x40fe70` 的 `ItemCarryLily`）。
 * ⚠ 真 ECL 子程序**还没写**（spec §6 步 3，要资产管理线）⇒ 用具名占位常量：
 *   原版自带的惰性壳 `"NoEnemyDebug"`（108 已用同一招；`wait(1000)` 死循环 ⇒ 不动、不开火、不掉落）。 */
#define UFO_ECL_TOKEN_NAME  "NoEnemyDebug"  /* ★ TODO(资产/ECL §6 步 3)：令牌自己的 ECL 子程序名待写 */
/* UFO **本体**的子程序名不在这里 —— 它按颜色分 4 件（一手名字表 `th12:0x4b33e0` =
 * {`UFO_Red`, `UFO_Blue`, `UFO_Green`, `UFO_Rainbow`}，索引 = 上面的 `ufo_color_art_index()`），
 * 具名表放卡侧 `ufo.c`（`UFO_ECL_BODY_RED/BLUE/GREEN/RAINBOW`，★ **金借蓝那一件** = 用户裁决 ①）。 */
#define UFO_TOKEN_LIFE      1      /* ★❓ 自定：令牌 HP。硬约束 < 1000；1 = 一发即破（能收） */
#define UFO_TOKEN_MAX       3      /* ★❓ 自定（spec §3.2 ⏳「令牌实体的出现规则」待拍）：同时最多几个令牌
                                    * 实体。⚠ 占位 1 会让「凑齐 3 个」几乎不可能（场上只有一个、且容易被
                                    * 子弹误伤或超时撤掉）⇒ 取 3；令牌表另有 `UFO_TOKEN_SLOTS` 分配护栏 */
#define UFO_TOKEN_TTL_FRAMES 2400  /* ★❓ 自定：令牌实体的存活帧上限（到点自己静默撤）。换关时随 run 复位。
                                    * ⚠ 与一手的 1200 帧门（`UFO_TOK_NO_BOUNCE_FRAMES`）**不是一回事**：
                                    *   一手 1200 帧之后只是「不反弹 + 出界即删」，令牌自己飞出去就没了；
                                    *   我们的惰性壳不会自己飞出去 ⇒ 留一个更长的 TTL 兜底（防漏实体）。 */
/* 令牌实体的出生点（自定）：横行在活动框内、纵在框上沿附近 —— 框一手 = TH12 `moveLimit(0,128,280,128)`
 * ⇒ x ∈ [−140,140]、y ∈ [64,192]（spec §1 表末行「照抄成自写 ECL 的飘行框」）。
 * 占位期由卡侧按 `enemy_id` 确定性散列落点（**不调 `ce_rand()`**：死亡回调里不引入随机流）。 */
#define UFO_FRAME_HALF_W    140.0f  /* ✅ 一手：280×128 的活动框半宽（`th12:0x4174fd`，spec §1） */
#define UFO_FRAME_Y_TOP      64.0f  /* ✅ 一手：框上沿 y */
#define UFO_FRAME_Y_BOTTOM  192.0f  /* ✅ 一手：框下沿 y */

/* ================== ★★ 令牌的**运动**（本轮新增；用户口述「飞碟在场上…飞行」= 这一条）
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
 * ④ **1200 帧之后不再反弹**（`0x4263d1 cmp [edi+0x98c],0x4b0; jge 0x42650f` ⇒ `0x42650f` 只判出界不判反弹），
 *    出界即删：外框 = `x ≤ [0x4a4400]`(−208) / `x ≥ [0x4a43a0]`(208) / `y ≤ [0x4a43f8]`(−16) /
 *    `y ≥ [0x4a3e90]`(464) ⇒ `0x42655b mov [edi+0x9b0],0`（state 0 = 回收）。
 *    ★ 社区说的「变色碎片变色 8 次后不再反弹 / 固定碎片 20 s 后不再反弹」= **同一个 1200 帧门**
 *      （1200 ÷ 150 帧换色 = 8 次）。两种说法同源 ✅。
 * ⑤ 6/7 状态的取舍见 `ufo_step_allowed` 一族；令牌的**收取**（自机碰到）在 TH12 是
 *    「道具位置落进自机的收点矩形 `player+0xc444..`」（`0x426100`/`0x426131`/`0x426144`/`0x426157` 四次比较），
 *    该矩形的半尺寸来自玩家表 `0x4b31b4` = {60,70,60}（按机体，× 0.5 × 0.5 ⇒ 半宽 **15 单位**）——
 *    ⚠ 我们的令牌不是道具、走不到那条引擎通路 ⇒ 卡侧只能自算距离（见 ufo.c 的 `UFO_TOKEN_PICK_DIST`）。 */
#define UFO_TOK_FLY_SPEED   1.5f         /* ✅ 一手：`0x4a4270` */
#define UFO_TOK_ANGLE_DOWN_RIGHT 0.785398185f  /* ✅ 一手：`0x4a4274` = π/4（右下方）*/
#define UFO_TOK_ANGLE_DOWN_LEFT  2.3561945f    /* ✅ 一手：`0x4a4278` = 3π/4（左下方）*/
/* cos/sin(±45°) × 1.5 —— 不用 libm（本层不链数学库，见 `ufo_drag_step` 那段注的同一理由）。 */
#define UFO_TOK_VEL_COMP    (UFO_TOK_FLY_SPEED * 0.70710678f)   /* = 1.060660124f */
#define UFO_TOK_BOUNCE_X    176.0f       /* ✅ 一手：`0x4a4198` / 左边界 `0x4a440c` = −176 */
#define UFO_TOK_BOUNCE_Y_TOP    160.0f   /* ✅ 一手：`0x4a3f4c` */
#define UFO_TOK_BOUNCE_Y_BOTTOM 370.0f   /* ✅ 一手：`0x4a4408` */
#define UFO_TOK_OUT_X       208.0f       /* ✅ 一手：`0x4a43a0` / `0x4a4400` = −208（外框，出界即删）*/
#define UFO_TOK_OUT_Y_TOP   (-16.0f)     /* ✅ 一手：`0x4a43f8` */
#define UFO_TOK_OUT_Y_BOTTOM 464.0f      /* ✅ 一手：`0x4a3e90` */
/* 出生点夹取（一手 `th12:0x4273f0` 的 `0x42747d`–`0x4274b1`：x 夹 ±192（`0x4a3d88`/`0x4a3d98`）、
 * y 不小于 0（`fldz; fcom …`））—— 令牌从敌人死亡点掉出来，别落在版外。 */
#define UFO_TOK_SPAWN_CLAMP_X 192.0f     /* ✅ 一手：`0x4a3d88` / `0x4a3d98` */
#define UFO_TOK_NO_BOUNCE_FRAMES 1200    /* ✅ 一手：`0x4263d1` 的 `0x4b0`（= 20 s = 8 次换色）*/

/* 一枚令牌的**运动状态**（纯数据，卡侧每槽一份）。⚠ 卡私有状态放不下 16 份 ⇒ 卡侧放文件静态
 * （与五边形框句柄同一套做法，理由见 ufo.c 的 `s_tok_motion`）。 */
typedef struct {
    float   vx, vy;   /* 每帧位移（单位 = 道具/ECL 空间）*/
    int32_t age;      /* 出厂帧数（一手：>= `UFO_TOK_NO_BOUNCE_FRAMES` 之后不反弹）*/
} ufo_tok_motion_t;

/* 初始化（角度按「本关第几个令牌」的奇偶交替；`serial` = 0 基的令牌序号 ⇒ 第 1 枚 = 左下 45°）。 */
void ufo_tok_motion_init(ufo_tok_motion_t *m, uint32_t serial);
/* 走一帧：位移 + 反弹 + 出界判定。返回非 0 = **该令牌该被撤掉**（一手：出界即 state 0）。
 * 指针入参、整型返回 ⇒ 不产生 x87（`dllx87` 门）。 */
int  ufo_tok_motion_step(ufo_tok_motion_t *m, float *x, float *y);

/* ================================================= §3.4 伪磁场扫描（成本记账）
 * 池 = `CE_ITEM_SLOTS` 4696 槽 × stride 0xc94（`engine.h:426-427`）。本层的早停判据：
 *   ① 只在「UFO 在场且已过 60 帧暂停」时扫（`ufo_can_absorb`）；
 *   ② state 3 / 4 直接跳过（引擎自己的真空/环吸）；
 *   ③ type 不是 1 / 2 直接跳过（先读 type 再算几何 ⇒ 非 P/钱道具零几何开销）。
 * ⚠ 没有 exe 就别编帧耗 ⇒ ufo.c 里只记「扫了几槽 / 拉了几颗 / 跳了几颗」，实测帧耗**待实跑**。 */
#define UFO_ABSORB_TYPES_N 2
int ufo_absorb_type_index(uint32_t type);   /* 1 → 0、2 → 1、其它 → −1（早停用） */

/* ============================================ 死亡事件过滤（DEATH §1.3 / spec §7）
 * 四张卡要的「击破」= `CE_EDEATH_KILLED`；**必须排除 `KILL_ALL`**（否则过场清场把计数器刷满，
 * spec §7 AUDIT）。本层不 include sdk/public.h ⇒ 自己钉一遍 kind 常量（值来自 DEATH §2.1，
 * 编译期由 ufo.c 的 `_Static_assert` 对账）。 */
#define UFO_EDEATH_KILLED    0u
#define UFO_EDEATH_BOMB_KILL 1u
#define UFO_EDEATH_ECL_DIE   2u
#define UFO_EDEATH_OTHER     3u
#define UFO_EDEATH_KILL_ALL  4u
/* 令牌「入槽」认哪几种死因：被打死 / 炸弹秒杀 —— ★❓ 自定（spec §9 #13「`BOMB_KILL` 算不算击破」未拍板）。
 * 取「算」：玩家炸掉一个自己能打的令牌实体却什么都拿不到会很怪；`KILL_ALL` **一律不算**。
 * ⏳ 用户拍板后改这一个函数。 */
int ufo_token_kill_counts(uint32_t kind);
/* UFO 本体的「击破」认哪几种：同为 ★❓ 自定（同上 #13）。 */
int ufo_body_kill_counts(uint32_t kind);
