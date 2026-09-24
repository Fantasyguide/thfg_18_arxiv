/* faith.h —— 绿点 / 信仰系统的**引擎侧**共享件：一个 VM 池 + 按档 spawn + 收集判定 + 按槽记账。
 *
 * 用户定案（2026-09-12）：110（风神录）与 111（地灵殿）的绿点「完完全全就是同一个东西」，
 * 只做**一套**系统、数值表不同；两者**共用一个池**（= 一次实现），落槽时按 `slot_mask` 分流到
 * 各自的记账槽 `Δ_110` / `Δ_111`（`faith_core.c`）。这是本模块的**核心接口约定**。
 *
 * 依据：
 *   · `docs/superpowers/specs/2026-09-10-mof-faith-design.md` §2.4（自建 VM 池，运动学照原生道具）
 *   · `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §7.1 / §7.3 / §7.9
 *     （一套系统 + 分槽记账；大 / 小信仰点一律走自建 VM 池；`type 2` 判据）
 *   · 先例 `native/cards/season.c`（TH16 自建 VM 池 + 收集实体最完整的模板）、`animal.c`（117）
 *
 * ⚠ 自建实体的已知代价（review §7.3，用户已知并接受）：**不入原生道具池** ⇒
 *   原生磁吸 / 回收线**看不到它**；所以吸引与收集判定由本模块自己做
 *   （判据照抄原生：读自机自己的 `player+0x4798c` 收集半径 / `+0x47990|94` 吸引半径 /
 *    `+0x47988` 磁速 / `+0x47998` 回收线）。
 *   ★ 2026-09-15（`NEXT.md` §1x.2）：**114 的收点批（辉针城）这一条已经接上** ——
 *   大 / 中两档被收 ⇒ 批计数 +1（**不进 SCORE / 不进批分**）、在飞期间续批 presence；
 *   微档不入批（§1x.3）。走的是 SDK 的声明式层（`sdk_core.h` 的「虚拟收集 / 虚拟在飞」，
 *   生产者只声明、消费者只读）⇒ 卡侧（`mof.c` / `signal.c`）**一行都不用改**，
 *   将来 116 季节点 / 117 动物灵要接 114 同样只加两行声明。
 *   ★★ 2026-09-18（B1，用户规格）：两条通道**都只认「真空飞行」（本池 state 3 = 过回收线 /
 *   放 B / 对话 ⇒ 朝自机飞）**，与引擎对 P 点 / 金钱的谓词逐字同一条（`type ∈ {1,2}` 且
 *   `item+0xc74 == 3`，`AUDIT.md` §Z22）⇒ 飘落 / 飞散 / 70px 环吸中顺手捡的点**不开批也不续批**
 *   （那就是「普通游玩顺手捡一颗」）。对照表与判据在 `faith_core.h` 的 `faith_state_t` 一节。
 *
 * ⚠ 美术占位（❓ 自定，见 `METHOD.md`「铁律:UI 与动画一律先从原代码找」）：
 *   TH18 **没有「绿点」这个 item type**（只有 `1..0x13`），我们也没有绿点贴图 ⇒ 现在借
 *   季节点那张 16×16 原生切图（`CE_ANM_ABILITY_SPRITE_PT_*`）+ `ce_anm_set_color` 染绿。
 *   拿到绿点贴图后只改 `faith.c` 的 `faith_sprite_for()`，接口不动。
 */
#pragma once
#ifndef FAITH_H
#define FAITH_H
#include <stdint.h>
#include "faith_core.h"

/* ---- 池（**静态分区**：大档 64 槽 / 小档 256 槽，各自独立上限 —— 大信仰点一次清场几十颗，
 * 不会把小点的位置吃掉）---- */
#define FAITH_BIG_POOL    64     /* 大信仰点 / 大绿点实体上限 */
#define FAITH_SMALL_POOL  256    /* 小信仰点 / 小绿点实体上限 */
#define FAITH_POOL_TOTAL  (FAITH_BIG_POOL + FAITH_SMALL_POOL)
#define FAITH_LAYER       13     /* 世界层 13 = 相机 3（游玩区），与原生道具同层（照 season.c）*/
/* ★★ 2026-09-15（用户定案，删掉 `FAITH_INTANGIBLE 32`）：**本池的点没有出生保护**。
 *   一手 + 实测：TH18 原生的 P 点 / 金钱**没有**出生无敌 —— 道具 tick 里 state 1 的收点检查
 *   （`th18:0x44620a` 共享尾 → `0x446227` 30px / `0x446647` 70px 环）从**出生第一帧**就跑，
 *   全 exe 里没有任何按「出生帧数」跳收点的判定；`th18:0x446981` 的 `cmp CURRENT_POWER,MAX_POWER`
 *   只拦「加火力」那一支、**不拦掉落**（用户 2026-09-15 实测：满 P 仍然掉 P 点）。
 *   ⇒ 我们原来那道 32 帧闸门（不吸 / 不收 / 不减速）是 ❓ 自定，等于「线上撒的点当帧吃不到」，
 *     连 `ce_item_collect_all()`（把活道具切 state 3）也吸不走刚撒的点 ⇒ **整条删掉**。
 *   ⚠ **出生动画（脚本 272 / 273）一个字都没改**：那是 TH10 原生出生脚本的逐条转写（贴图/帧表），
 *     它只是「画成什么样」，不是「能不能被收」。闪白/缩放照播，只是不再挡住吸与收。
 *   ⚠ 删掉的是**三条判定**：过回收线/放 B/对话 → 真空飞行、30 px 收点、90 px 环吸入。
 *     ★ 2026-09-15d（`NEXT.md` **§1x.4**）：**「减速三件套」那一条也留下并补全了** ——
 *     年龄门（`age > 31`）是**引擎自己的**（`th18:0x445a80` 同段 / `th16:0x42fad2`–`0x42fb35`），
 *     当年与出生保护**巧合地共用了 32 这个数**。三件套 = **变慢 + 变黄 + 抖动**，
 *     一手落点与出处见 `faith.c` 的「§1x.4 减速三件套」注释块。*/

/* 三档（type 号 ↔ 两作别名 ↔ 各个来源的硬约束见 `faith_core.h` 的 §D/§E-1 大表；这里只说外观）。
 * ⚠ 三档**外观各不相同**（2026-09-15b 起）：大 = `FAITH_DOT_BIG.png` / 小 = `FAITH_DOT.png`
 *   / 微 = `FAITH_DOT_MICRO.png`，且微档用**另一条出生脚本**（273，加算 + alpha16 + scale 0→1）
 *   —— 见 `faith.c` 的 `faith_sprite_for()` / `faith_script_for()` 与 `TUNING.md` §2.6。
 * ⚠ 但**微档不建实体**（见下），⇒ 今天在实跑里还看不见微档的那张图。
 *   ⇒ `FAITH_TIER_MICRO` 今天只作「消弹入账」的档位名（`faith_cancel_account()` 直接进账，
 *     按 §2.2 的四个来源我们现在只接得到「自己放灵击」那一条，`faith_spawn()` 对微档返回 0）。
 * `faith_tier_t` 本体在 `faith_core.h`（纯逻辑层要按来源给它定档，见 `faith_core_tier_for_source()`）。*/

/* ---- 卡侧回调 ----
 * 收集到一颗绿点 / 信仰点时，本模块回调**所有**在场卡；卡自己按 `slot_mask` 认领，
 * 再把值加进自己的记账槽并 `ce_score_declare(ce_card_id(c), 聚合值)`。
 * ⚠ 卡之间**不读对方状态**：本模块只发「谁掉了多少」，聚合由 SDK 的计分层做（EVENTS-CONTRACT §3）。*/
typedef void (*faith_pickup_fn)(void *user, int slot_mask, int tier, int32_t value);

/* ★ 2026-09-15 新增：**带位置**的收集回调（`on_pickup_at`）。
 *
 * 为什么加：110 要在「吃到绿点」的位置弹一个绿色 `+N`（用户要求；一手 TH10 的弹窗画在
 * **道具自己的坐标**上 —— `th10:0x41b360 lea edi,[ebp-0x4]`，全 type 共用），而旧签名只带
 * `(tier, value)` ⇒ 卡侧只能拿**自机坐标**近似（收集半径内 ≤30 px 的偏差）。
 *
 * 兼容性（共享件纪律）：这是 `faith_user_t` 的**尾部新增字段**，且 `faith.c` 的派发是 **二选一**
 *   （设了 `on_pickup_at` 就调它，否则调 `on_pickup`）⇒
 *   · 既有注册者（111 的 `cards/signal.c`）**不设**这个字段 ⇒ 走的还是 `on_pickup`，行为逐字节不变；
 *   · 110 两个都设（`on_pickup_at` 生效、`on_pickup` 作兜底）⇒ **不会双计**。
 * 影响面与回归见报告与 `TUNING.md` §2.5。*/
typedef void (*faith_pickup_at_fn)(void *user, int slot_mask, int tier, int32_t value,
                                   float x, float y);

/* 消弹时回调（本模块**不持有任何槽**：槽状态归卡自己的 ce_state —— 这样 110 与 111
 * 各写各的、互不可见，天然满足「不跨卡耦合」）。*/
typedef void (*faith_cancel_fn)(void *user, int32_t bullets_cancelled);

/* ---- 卡注册（每张用绿点的卡调一次；tick 时再由模块做本帧记账）----*/
#define FAITH_MAX_USERS 8
typedef struct {
    uint32_t       card_id;     /* 声明 score 的那张卡的 id（= ce_card_id(c)）*/
    int            slot_mask;   /* 本卡消费哪些槽：1 << FAITH_SLOT_110 / 1 << FAITH_SLOT_111 */
    faith_pickup_fn on_pickup;  /* 收到一颗点（可 NULL）*/
    faith_cancel_fn on_cancel;  /* 消弹入账（可 NULL）*/
    void          *user;        /* 回传给回调的卡指针 */
    /* ★ 2026-09-15 尾部新增（旧卡不设 ⇒ 行为不变）：带位置的收集回调。
     * 语义 = **二选一**：设了它就不再调 `on_pickup`（两者都不设 = 这颗点没人认领，只记账不回调）。*/
    faith_pickup_at_fn on_pickup_at;
    /* ★★ 2026-09-15 尾部新增：**满 P 转换通道**的单颗值（原始单位，与 `faith_spawn` 的 `value` 同口径）。
     * 语义：满 P 时原生 P 道具（`FAITH_TYPE18_P_SMALL` / `_BIG`）不该存在 —— 见
     * `faith_convert_full_p_items()`；被替换掉的那颗原生道具，在本槽补一颗 `FAITH_TIER_SMALL` 的点，
     * 值就是这个字段。**0 = 本卡不参与转换**（老注册者不设 ⇒ 完全不参与，行为逐字节不变）。
     * ⚠ 两作的这个值不同（110 = `FAITH_MOF_SMALL` 100、111 = `SIGNAL_GREEN_MID` 10），
     *   但**档位相同**（type 9 = E-1 的硬约束）⇒ 池只需要值，不需要知道是谁。*/
    int32_t conv_value;
    /* ★★ 2026-09-18 尾部新增：**消费者自带外观**（0 = 用点池默认的「按档」脚本/贴图）。
     * 语义：本卡槽里撒出来的点用这两个 id 起 VM —— 脚本走 `ce_anm_spawn(…, art_script)`、
     * 贴图走 `ce_anm_set_sprite(…, art_sprite)`；顶点色仍按档（大/小档 = 白 = 不染）。
     * 用处 = **107 的樱点道具实体**（TH18 没有樱点道具类型 ⇒ 外观借 TH07 原版切图
     * `SAKURA_POINT` = sprite 287 / 脚本 186，见 `SENRO-107-CHERRY-ENTITY.md`）。
     * ⚠ **老注册者不设 ⇒ 这两个字段是 0 ⇒ 行为逐字节不变**（`dot_spawn_vm()` 里 0 走原路）。
     *
     * ★★ 2026-09-18（配套）**「两种点不许重合」**：满 P 转换是「一颗原生道具 → 每个消费者各一颗」，
     *   位置与初速全同 ⇒ 会**完全重叠**。用户口径：「**由于转换成两种点，不要让两种点重合；有角度的上抛？**」
     *   ⇒ `faith_convert_full_p_items()` 按**上面这两个字段（外观）分组**给上抛角：
     *     · 第 0 组 = 一手角 **−π/2**（⇒ 单独装任何一张时行为逐字节不变）；
     *     · 之后每组 ±10°（`FAITH_CONV_SPREAD_STEP` = `0.1745329`，正是 `th18:0x429168` 那枚
     *       —— TH18 自己消弹撒点用的抖动角，一手见 `engine/bullet/th18/01-cancel-to-item.md` §2.1）。
     *   ⚠ **同外观的消费者仍共用同一个角**（⇒ 110+111 那两颗同款点依旧完全重合、视觉一颗 ——
     *     这是用户 2026-09-18 追问「不会出现两个中信仰点吧」时期望的样子）。*/
    uint32_t art_sprite;
    uint32_t art_script;
} faith_user_t;


/* ★★ 2026-09-18 新增：**显式给上抛角**的撒点入口（其余语义与 `faith_spawn()` 逐字相同）。
 * 唯一使用者 = 满 P 转换通道：一颗原生道具同时给每个消费者各撒一颗 ⇒ 按**外观**分组给角，
 * 免得 107 的樱点藏在 110/111 的信仰点底下（用户口径「不要让两种点重合；有角度的上抛？」）。
 * 详细理由与「第 0 组仍是一手 −π/2」那条不变式，写在 `faith_user_t.art_sprite` 的注里。*/
int faith_spawn_angled(float x, float y, faith_tier_t tier, int slot_mask, int32_t value,
                       float angle);

/* ★★ 2026-09-18 再加：**这一颗单独指定外观**（`sprite` / `script` 为 0 时退回注册者的
 * `art_sprite/art_script`、再退回按档默认）。用处 = 107 的**小樱点**：同一张卡要两种外观
 * （大樱点 = `SAKURA_POINT` 287、小樱点 = `SAKURA_SMALL` 288），而槽只有一个。*/
int faith_spawn_art(float x, float y, faith_tier_t tier, int slot_mask, int32_t value,
                    float angle, int32_t sprite, int32_t script);

/* ---- 生命周期 ---- */
/* ⚠ `faith_module_reset()` **没有调用者，也谁都不该调它**（2026-09-12 审计订正；旧注释写「局末与
 * 删卡都调」是错的）：它内部是 `faith_clear_entities()`（清**整个池**）+ `memset` 掉**整张注册表**
 * ⇒ 110 / 111 共用池时，它会把**另一张卡**同屏还飘着的绿点删掉、把另一张卡的注册也抹掉。
 * 两张卡各自在 `on_run_reset` / `on_delete` 里用 `faith_unregister(自己的 id)` +
 * `faith_clear_entities_mask(自己那一槽)`（= 下面这两支），不经过这里。
 * ⚠ 本函数**保留不删**（可能有外部引用），但本仓里它是**死代码**：建议后续直接删掉。*/
void faith_module_reset(void);
int  faith_register(const faith_user_t *u);   /* 0 = 失败（表满 / 参数非法）；重复注册同 id = 更新 */
void faith_unregister(uint32_t card_id);
void faith_clear_entities(void);    /* 收掉池里**所有** VM（模块复位用；卡侧别用它，见下）*/
/* ★ 收掉池里**只认领了 `slot_mask` 那些槽**的 VM —— **删卡路径必须用这一支**（`on_delete` 与
 * `on_run_reset`）。理由（2026-09-12 审计补）：池是 110/111 **共用**的，`faith_clear_entities()`
 * 会把**另一张卡**同行还飘着的绿点一起删掉（「删掉其中一张、另一张的实体凭空消失」）。
 * 每一颗点出生时就带 `slot_mask`（`faith_spawn` 的第 4 参）⇒ 按它筛就只收自己家的。
 * 传 `~0` = 所有槽（`faith_clear_entities()` 就是这么实现的）。*/
void faith_clear_entities_mask(int slot_mask);

/* ★★ 2026-09-15（组合 UI）：「**哪些槽有卡在场**」的只读查询 —— 返回**所有注册者 `slot_mask` 的并集**。
 *
 * 为什么需要它（用户 2026-09-15 口径 ③ 的实现判据）：组合 UI（110/111 共用那一块）在**缺卡**时要走
 * 中性显示（只装 110 ⇒ 信号强度 1.00、槽满格不闪；只装 111 ⇒ 连击条填满），所以那块 UI 必须能判
 * 「110 在不在 / 111 在不在」。
 * ⚠ **不许**用「槽值恰好为 0」当判据（用户明令）—— 槽值 0 是完全正常的游玩状态（一局开头、
 *   miss 之后都会是 0），用它判「卡不在场」会当场把中性显示画错。这里的判据是
 *   **注册表**（`faith_register()` 那张表，由卡的 `on_load` 填、`on_delete`/`on_run_reset` 清）——
 *   与「谁在消费这个槽」是同一份事实，不是它的影子。
 * 返回值：`(1 << FAITH_SLOT_110) | (1 << FAITH_SLOT_111)` 之类的位掩码（0 = 没有任何卡在场）。*/
int faith_registered_mask(void);

/* ======================================================================================
 * ★★ 2026-09-15h：P 点补值的**共享策略**（运行时入口；纯逻辑在 `faith_core.h`）
 * ======================================================================================
 * 「一颗小 P 补到多少火力」原来两张卡各接一次线（110 的 `mof.c` + 111 的 `signal.c`），
 * 而落笔只有一处（`mof_core_p_owner()` 选举，id 最小者胜 ⇒ **110 在场恒 110 落笔**）
 * ⇒ 111 的「爱丽丝把 P 点收益减半」在双卡同装时被绕过。本函数就是那个**唯一**的答案来源。
 *
 * ## 判据（全是现成件，**不许用槽值**）
 *   · **111 在场** = `faith_registered_mask()` 的 111 位（本文件上面的注册表）；
 *   · **机体门** = `CE_ADDR_CHARACTER == FAITH_CHAR_MARISA`（用户拍板的 Σ 映射，不是复刻）。
 *   ⇒ 两者都成立 ⇒ 返回 **2 / 3 交替**（平均 2.5 单位 = **+0.025 档**，🟡 用户口径）；否则返回 **5**（常态）。
 *
 * ## ⚠ 调用纪律（**唯一的不变式，破了就会双计**）
 *   1. **只有落笔者可以调**：110 的 `on_item_commit`（`cards/mof.c`）与 111 的（`cards/signal.c`）
 *      都必须在**自己那一侧的 owner 检查之后**才调它 —— 非落笔者必须早退、一个字节都不写。
 *   2. 它**会推进共享相位**（`s_p_alt`，静态区）⇒ 一颗道具调用**恰好一次**；而「每颗道具全局只落笔
 *      一次」是既有选举保证的 ⇒ 相位也就**每颗只推进一次**。
 *   3. ⚠ **两张卡各自持一份相位是错的**（两个 `alt` 会分叉成两条序列）—— 相位只有这里这一份。
 *
 * ## 「被删掉会怎样」（`METHOD.md` 铁律二）
 *   相位/计数是**纯记账**、在世界里不留任何东西（不建实体、不写引擎字段、不占槽位）：
 *   最后一张卡走时 `faith_unregister()` 把它归零 ⇒「删卡后又装回来」不会带着上一局的相位。
 * ==================================================================================== */
/* ★★★ 2026-09-24：「改妖精」抽离到共享件 —— owner 口径「**是 110 或 111 只要装一张就改**」。
 *   驱动 = `faith_pool_tick()` 每帧调一次 `faith_green_swap_tick()`（那张池子两张卡都会调）。*/
void     faith_green_swap_tick(void);
void     faith_green_restore_all(void);
uint32_t faith_green_applied(void);          /* 此刻真的改上的行位掩码（= s_fgreen_applied）*/
int      faith_green_row_green(int i);       /* 第 i 行此刻既改上、表里也仍是绿 */

int32_t faith_p_topup_take(void);
/* 只读诊断（**任何卡都可以调**，不推进相位）：相位 / 已推进颗数 / LO、HI 两侧计数；任一 `out` 可为 NULL。
 * ⚠ 读的是**共享**相位（不是本卡私有的）⇒ 两张卡打出来的是同一组数。*/
void faith_p_topup_stats(int32_t *alt, int32_t *items, int32_t *lo, int32_t *hi);
/* 归零共享相位（`faith_module_reset()` 与「最后一张卡退注册」时调；**卡侧不需要**调它）。*/
void faith_p_topup_reset(void);

/* ---- 撒点 ---- */
/* 在 (x,y) 撒一颗点；`slot_mask` 决定它落进谁的槽、`tier` 决定 entity 池与半径。
 * 池满返回 0（丢弃：本模块**不做待撒队列** —— 大信仰点的量级一次十几颗，不会爆）。
 * 运动学 = **复刻**（E-2 一手）：角恒 `FAITH_THROW_ANGLE`（−π/2，直上）、速 = `faith_core_throw_speed(tier)`
 * （击破/满 P 转换族 2.2、消弹族 0.6）；出生当帧即可被收（没有出生保护，见文件头）。*/
int faith_spawn(float x, float y, faith_tier_t tier, int slot_mask, int32_t value);


/* ============================ ★★ 声明式「外部消费者」接口（112 的飞碟；2026-09-18）============================
 * 形状 = `NEXT.md` §1x.7 定下的那一条（「自建实体声明『我在飞 / 我被收』，消费者只读」），
 * 先由 112（星莲船 UFO）落地；将来 116 季节点 / 117 动物灵要接同一条路，照这个形状加一对即可。
 *
 * 语义（**两条都很窄，故意不做成通用反射**）：
 *   ① `faith_absorb_list()` —— **只读**枚举本帧活着的大/中信仰点（微档不建实体 ⇒ 天然不在列表里）。
 *      调用方拿到的是一份**快照**（坐标 + 档位 + 槽 + 原值），可以随便存、随便比。
 *   ② `faith_absorb_take()` —— 把快照里的那一颗**取走**：从池里移除并删它的 anm VM。
 *      ⚠ **不动任何记账**（不 +Δ、不进 114 的批、不加连击）：一手的道具被 UFO 吸走也是
 *      「道具消失、不进自机的账」⇒ 「吸走后算谁的账」由**调用方**决定（112 把它记进
 *      待返还的账里，击破时由「压缩包」带回；见 `NEXT.md` §1x.9）。
 *      ⚠ 用**快照里的坐标**匹配（同一帧内坐标不变的假设与本池自己的 tick 一致）；
 *      匹配不上（那一颗已经被别人收走 / 已出界）返回 0，调用方当「这一次没吸到」处理。
 *
 * 为什么用「快照 + 按坐标取回」而不是给外部一个裸指针：本池是**静态区紧凑数组 + swap-remove**
 *   （见 `faith.c` 的 `dot_remove`）—— 任何外部持有的下标在一次移除之后都可能指到别人身上。
 *   坐标匹配让调用方可以安全地「先枚举、再逐颗吸」（两段之间别的点会被收走）。*/
typedef struct {
    float   x, y;          /* 快照那一刻的坐标（ECL 640 空间，世界层同坐标系）*/
    int32_t tier;          /* FAITH_TIER_BIG / FAITH_TIER_SMALL（faith_tier_t）*/
    int32_t slot_mask;     /* 落进谁的槽（= 它是 110 的还是 111 的）*/
    int32_t value;         /* 原始单位分值 */
    /* ★★ 2026-09-24（owner：「**似乎在 UFO 期间生成的点是没有『出生无敌』的**」）——
     *   本池对**回收线/放 B/对话**那条真空门**本来就有**出生豁免（`FAITH_TOSS_FRAMES` = 上抛期，
     *   见 `faith.c` §B12），但**消费者（112 的飞碟）此前拿不到这个信息** ⇒ 刚撒出来的点会被
     *   UFO 当场吸走（= owner 说的"没有出生无敌"）。
     *   ⇒ 快照带上这两个字段，让消费者**自己判**（生产者只声明状态、不替消费者决定扣不扣）。*/
    int32_t age;           /* 出厂帧数（`p->age`）*/
    int32_t state;         /* 本池的段号：1 飘落 / 2 飞散(上抛) / 3,4 朝自机飞 */
    int32_t in_toss;       /* 1 = 还在**上抛豁免期**（`state == 2 && age < FAITH_TOSS_FRAMES`）*/
} faith_absorb_t;

int faith_absorb_list(faith_absorb_t *out, int max);   /* 返回写进 out 的条数（0 = 没有 / 参数非法）*/
int faith_absorb_take(const faith_absorb_t *e);        /* 1 = 真的取走了一颗（VM 也删了）*/
/* ③ 把那一颗**往别处挪一步**（飞碟的伪磁场拉拽用它）：按与 `take` 同一个匹配规则找到它、
 *    写回新坐标，并把 VM 的位置同步过去。返回 1 = 真的挪到了（那一颗还在）。
 *    ⚠ 只动 `x/y` 与 VM 位置：**不碰 state / 速度 / 记账**（拉拽期它仍然是"飘落中"的点，
 *      110/111 自己的收点判定照旧能把它收走 —— 与一手「被吸走的道具仍可被自机碰触」同款）。 */
int faith_absorb_move(const faith_absorb_t *e, float nx, float ny);
/* ④ ★★ 2026-09-24（owner：「我原本所说的愣住确实是 UFO 原版吸收的状态，但是我看到的只是顿住一下，
 *     实测是顿住好长时间」）—— **被拉拽期间把这一颗自己的运动按住**：写回新坐标、同时把它的
 *     `vx/vy` 清零（`state 2` 时再把 `lr` 清零 ⇒ 下一帧落回"飘落"）。
 *   **一手依据**：TH12 的道具上钩那一刻（state 7，`0x4271ec`–`0x427204` 三条 `FLDZ/FST`）
 *     **把速度向量与吸引距离一起清零** ⇒ 后面的位移**只**由飞碟那一步决定。
 *   ⚠ 我们此前只调 `faith_absorb_move()`（只写坐标、不碰速度）⇒ 点自己的上抛/下落每帧还在叠，
 *     与飞碟那一步互相抵消 ⇒ 点**悬在半空**（owner 看到的"顿住好长时间"）。
 *   ⚠ 与 `move` 一样**不碰记账**（不进 Δ / 不进批 / 不加连击）。*/
int faith_absorb_hold(const faith_absorb_t *e, float nx, float ny);

/* 大信仰点（110 的击破通道；§7.9 的判据由卡侧给，本函数只负责撒）。*/
int faith_spawn_big(float x, float y, int32_t value_110);

/* ============================ ★★ 2026-09-22：大点的**落点散布**（共用件）============================
 *
 * 一手 = ZUN 自己「一次爆多颗道具」的分布（附加掉落循环 `th18:0x430510`，逐条见
 * `faith_core_scatter_off()` 的头注）：`pos = 敌机坐标 + polar(rand± × π, rx, ry) × (rand01 × 0.5)`
 * —— 半径 `rx/ry` 取**这只敌人自己的掉落区**（`drop+0xbc/+0xc0`，`dropArea` handler `0x42d090` 写；
 * 为 0 时用引擎自己给敌机死亡路径的默认 **32.0f**，`0x430766`/`0x430773`）。
 *
 * **为什么搬进共用件**（原先是 `cards/mof.c` 的私有 `mof_faith_scatter()`）：
 *   用户报的现象是 th11 #1「**绿点和金钱重合问题需解决**」—— 111（`cards/signal.c`）在同一个
 *   `enemy_drop_pre` 里 `faith_spawn()` 用的是**敌机坐标原值**，而金钱由引擎按上面那条公式撒在它周围
 *   ⇒ 大绿点正好落在金钱云的中心。110 已经按这条公式撒了（2026-09-18c），111 没有。
 *   做成共用入口之后 111 侧只需要两行（`if (faith_scatter_from_enemy(e, &ox, &oy)) { x += ox; y += oy; }`），
 *   不必再抄一份公式。
 *
 * ⚠ 随机源是**我们自己的 LCG**（❓ 自定，登记 `TUNING.md` §2.0i）：引擎那两支 RNG
 *   （`0x402810` / `0x4027d0`）的返回值走 **x87**（`fld/fstp`），从 C 里调会让目标文件带上 x87
 *   ⇒ 撞 `make dllx87` 的 0 条硬线。**分布公式本身逐条照抄**，只有随机源换掉。
 * ⚠ 三角走**引擎的 FSINCOS**（`ce_polar_to_vel`）⇒ 不引 libm。
 * 返回 1 = 写好了偏移（调用方把它加到敌人坐标上）；0 = `enemy == NULL`（两个偏移都写 0）。
 * 调用者：`cards/mof.c` 的 `on_enemy_death`（击破宿主 ⇒ 大信仰点）。*/
int faith_scatter_from_enemy(const uint8_t *enemy, float *ox, float *oy);

/* ============================ ★★ 2026-09-22：**绿妖精载体**（110 / 111 共用）============================
 *
 * 「每波多刷一只（或一对）绿的」（用户 2026-09-18「有没有可能你给循环 +1？」；2026-09-18c 的
 * 「妖精应当跟随那个队列才对」）—— 实现整段从 `cards/mof.c` 搬到这里，**110 与 111 共用一份**。
 *
 * **为什么必须共用**（一手 + 现状）：111（地灵殿）的设计本来就要求「让绿妖精出现」
 * （`docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §805，用户 2026-09-12），
 * 而这段代码原来只被 110 的 `on_tick_2` 调 ⇒ 只装 111 时**一只都不会出现**
 * （owner 实跑 th11 #4「全程未见绿妖精」）。载体掉的 `(N,S) = (2,10)` 本来就是**同一张宿主表**的键。
 *
 * **调用约定**（两张卡各在自己的 `on_tick_2` 里调一次）：
 *   · 自带**帧守卫**（同 `faith_pool_tick()` 的口径，帧号 = `ce_sdk_frame()`）⇒ 两卡同装时一帧只跑一遍；
 *   · 卡不在场 ⇒ 谁都不调它 ⇒ 一次都不跑（含 107 单装、无卡、商店里）；
 *   · 位置与 110 原来的位置一致（`on_tick_2` 一开头，`mof_green_tick()` 之后）。
 * 细节（判据、队形槽位、跟随通道、日志 `faithgf:`）全在 `faith.c` 的段落头注释里。*/
void faith_green_fairy_tick(void);

/* **收尾**（`METHOD.md` 铁律二）：把场上那几只我们刷出来的绿妖精写 `$I0 = 999` 让它们飞离自删。
 * 在 `on_clear`（= `on_delete` 与 `on_run_reset` 共用的那个）里调——**110 与 111 都要调**：
 * 谁被删掉，那一侧的 tick 就没了，不收就会把它们钉在关卡里到本局结束（删卡发生在**商店**里、
 * 那一刻 `on_tick_2` 全停 ⇒ 这是唯一还跑得到的收尾点）。幂等：表被第一个调用者清空，第二个什么都不做。*/
void faith_green_fairy_depart_all(void);

/* ★★ 2026-09-16：**把整池活着的点切成「朝自机飞」**（本池 state 3 = 引擎道具 state 3 的对位）。
 * 用途 = 所有「全屏收点」的调用点 —— 它们原来只调 `ce_item_collect_all()`，而**那个函数只扫原生道具池**，
 * 本池的点是**自建 VM**（TH18 没有信仰点 item type）⇒ 实跑里「信仰点纹丝不动」。
 * 一手：TH10 / TH11 的信仰点**就是道具**（type 3 / 9）⇒ 满槽全收（`th11:0x42364a` → `0x423805`）
 * 与萃香的全收（`th11:0x4245e0`）扫的是同一个池、**天然包含信仰点**。
 * `slot_mask` = 收哪些槽（`~0u` = 全收，黄金调用口径：那两条都是全屏收、与哪张卡无关）；
 * 返回被切的数量。⚠ 只动 state（不动 pos/vel）；`state == 0`（还没建 VM）与 `state == 3`（已经在飞）跳过
 * —— 与 `ce_item_collect_all()` 的 `st == 0 || st == 3` 同规则。详见 `faith.c` 的实现注释。*/
int faith_collect_all(uint32_t slot_mask);
/* ★★ 2026-09-16q：**消弹点不由本模块撒**（用户口径：「微档点可以完全认为是 **th18 消弹点**吧」）
 * ⇒ 110/111 的灵击消弹改用 **`ce_cancel_radius(..., mode 1)`**，由**引擎自己**在每颗弹的位置撒
 *   type 9（一手 `engine/bullet/th18/01-cancel-to-item.md` §2.1），我们只收那些点。
 * 上一批（2026-09-16p）在这里加过的 `faith_cancel_drop_dots()` 已撤（口径被取代）。*/

/* ==================== ★★ 满 P 全场转换（A + B 两条要求的同一台机器）====================
 *
 * **复刻**（一手两个落点，两作各一份）：
 *   · `th10:0x41bbf8`–`th10:0x41bc23`（`ItemManager::spawn_item` 内**在道具字段被填之前**）：
 *     `cmp WORD ds:0x474c48,0x64`（火力 = 100 单位 = 5.00 满）⇒ `type 1/4 → 9`（索引表 `0x41be28`
 *     + 跳表 `0x41be1c`）⇒ **满 P 时 P 道具一出生就是信仰点**（那份 P 点不存在）。
 *   · `th10:0x41ba50`（`ItemManager::on_tick__body` 尾部 `0x41b889` 调它，条件是 collect 侧
 *     `0x41b44b` / `0x41b6b3` 置的「刚满」标志 `[esp+0x14]`）：**满 P 那一帧全场转换** ——
 *     循环 150 槽（stride 0x3f0），对 `[item+0x3dc] != 0`（在用）且 `[item+0x3e0] ∈ {1,4}` 的道具：
 *     清在用标志 + `spawn_item(type=9, pos=**道具自己的位置**, 0xffffffff, angle=0xbfc90fdb (−π/2),
 *     speed=0x400ccccd (2.2))` + 逐颗起一台**特效 VM**（`0x448db0`，参 `0x189`；
 *     ⚠ 2026-09-18 订正：那是 **anm 脚本号**、不是 SE id —— `0x448db0` 用 `ANM_MANAGER_PTR`，
 *     转换循环**从不**调 SE 播放器 ⇒ 原版转换**静音**；`0x189` = `bullet.anm` script393）。
 *     （`type 10/11 → 5` 那两支是**死代码**：前面的类型过滤已把它们挡在循环外。）
 *   · **TH11 同款**：`th11:0x4240f0`（同形，stride 0x478、150 槽）—— `[esi] ∈ {1,4,10,11}` ⇒ 一律
 *     `type 9`、`pos = esi−0x34`（**道具自己的位置**）、角 `th11:0x49842c` = −π/2、速 `th11:0x498430` = 2.2。
 *
 * ⇒ 一手把「两处代码」讲的是**同一条不变式**：**满 P 期间场上不存在 P 类道具**
 *   （出生即 9 + 满的那一帧全场转换）。本模块就照这条不变式实现成**一条每帧规则**：
 *     满 P（`power >= max_power > 0`）时扫一遍原生道具池，把 `FAITH_TYPE18_P_*` 的原生道具
 *     **原位删掉**（照引擎自己的清尾，见 `faith.c` 的 `faith_item_delete()`），
 *     再在**它的位置**给每个 `conv_value > 0` 的注册者撒一颗 `FAITH_TIER_SMALL` 的点（E-1 硬约束）。
 *   ⇒ A（出生即替换，不是加撒）与 B（满的那一帧全场原位重生 + 上抛）由同一帧规则同时满足；
 *     撒在**道具自己的位置**、角 −π/2、速 2.2（一手实参；**不另加随机散**）。
 *
 * 卡侧用法：每帧 tick 里调一次（**放在 `faith_pool_tick()` 之前**，这样本帧转换出来的点
 * 当帧就会建 VM / 受物理 / 被收）。函数自带**帧守卫**（同 `faith_pool_tick()` 的口径）⇒
 * 110 与 111 同装时各调一次也只会转一遍。
 * 返回本帧被转换的原生道具**颗数**（诊断 / 单测用；0 = 没到满 P 或没得转）。
 *
 * ⚠ 「删原生道具」是**直写引擎道具池**（照引擎自己的清尾四步：state = 0 → 删 anm VM → anm id 清 0
 *   → freelist 前插）。同款先例 = `cards/ufo.c` 的 `ufo_item_relink()`（其文件头 ④ 逐指令对照）。
 * ⚠ 只碰 `type ∈ {1, 3}` 的**在用**槽；type 3 是「大 P」（TH10 的 type 4 对位，同样在转换表里）。
 * ⚠ `type 8`（TH18 的「补满火力」）**不转**：TH10 没有这一档（登记 `TUNING.md` §2.5）。*/
int faith_convert_full_p_items(void);

/* ---- 每帧 ---- */
/* 推进池（飘落 / 吸引 / 收集 / 出界）+ 收集回调。卡在 `on_tick_2` 里调一次。
 * 返回本帧被收走的颗数（诊断 / 测试用）。*/
int faith_pool_tick(void);

/* 消弹计数入账（**直接吃 `ce_cancel_radius()` 的返回值**，§5.5 订正 3 ——
 * 不要用 `ce_cancel_all_bullets()`，它是 void 数不出来）。
 * 已注册的卡各自从自己的槽拿：110 侧 = 微信仰点每颗 +10；111 侧 = 小绿点每 100 个 +10
 * （余数走 `faith_slot_t.small_rem`，跨关保留）。**不做跨卡耦合**：各槽自己算自己的。*/
void faith_cancel_account(int32_t bullets_cancelled);

/* ---- 便利：读自机几何 / 判定 ---- */
/* 回收线（PoC 高度）。★ 2026-09-15 **已迁移到共享件 `ce_poc_height()`**（`sdk.h` 的 PoC 高度层）：
 * 本支现在只是一层薄包装，留给 110/111 的既有调用点（`faith.c` / `mof.c` / `signal.c`）零 churn。
 * ⚠ 仍然**必须** `static inline`：i686 Windows ABI 的 **float 返回值走 x87 st(0)**
 * ⇒ 独立函数会让 `make dllx87` 门变红（本仓不变式「我们的 object 里 0 条 x87 指令」）。*/
static inline float faith_player_poc_height(void)
{
    return ce_poc_height();
}
int   faith_player_above_line(void);     /* 自机在回收线上方？*/
int   faith_enemies_present(void);       /* 场上有敌人？（连击槽空时的衰减档位用）*/

/* ---- ★★ 2026-09-15d（`NEXT.md` §1x.4）：绀珠传（115）擦弹减速期的「三件套」共用件 ----
 *
 * 用户口径：「th15 擦弹时候**大中绿点也得变黄、变慢、抖动**」。三件套的**一手**都在引擎自己
 * 的道具 tick 里（`th18:0x445a80`，逐指令见 `faith.c` 的注释块）：引擎替**原生道具**做，
 * 我们自建的 VM 得自己复刻。本文件的这两支就是那一套的**对外入口** ——
 *   · `faith_slowdown_permille()` = 读减速因子（**变慢**那一层），返回千分比（1000 = 没减速）；
 *   · `faith_dot_slow_style()`    = 按那一套条件写**变黄 + 抖动**（`slowed` 传 1 即可，也可以在
 *     卡侧自己用 `faith_slowdown_permille() < 1000` 算出来传进来）。
 * ⚠ 本池的**收点 / 吸入 / 物理**（`faith_pool_tick`）内部用的也是同一支 slowdown 读法
 *   （`faith.c` 里的 `static inline faith_item_slowdown_permille()`，**本头不暴露它**）。
 * ⚠ **为什么 slowdown 那支对外是「返回 `int` 千分比」而不是 `float`**：本仓不变式 `make dllx87`
 *   要求我们的 object 里 **0 条 x87 指令**，而 i686 Windows ABI 的 **float 返回值走 x87 `st(0)`**
 *   ⇒ 任何跨编译单元的 `float` 返回都会在边界上插一条 `flds`（本轮实测踩到：早一版
 *   `faith_item_slowdown()` 起手就是一条 `flds`）。⇒ 对外只给 `int`；内部读法写 `static inline`
 *   就地展开（纯 SSE）。要 `float` 就自己 `× 0.001f`（一条 `mulss`，不碰 x87）。
 *   ⚠ 同理：**别把这两支改成返回/按值传 `float` 的形态**（`faith_dot_slow_style()` 的 `float` 入参
 *   是安全的 —— i686 的**栈传参**不走 x87，`make dllx87` 实测 0 条）。
 * ⚠ 颜色是**用户口径**、不是复刻：减速期的黄**统一成 TH15 给弹的那支 `0xffffd080`**
 *   （用户 2026-09-15：「**肯定得统一成 th15，包括到时候 th07 的樱点**」）—— 与 116 季节点
 *   （`season.c` 的 `LOLK_GRAZED_COLOR`）同色，**将来 107（妖妖梦）的樱点也要复用同一常量**
 *   （待办登记在 `TUNING.md` §2.5d 末尾）。⚠ 引擎自己给**原生道具**写的是 `0xffa0ffff`
 *   （一手 `th18:0x445d12`）⇒ 同屏时我们的点与原生 P 点 / 金钱会差一点点色，这是**刻意**的。
 * ⚠ 陈账：本仓 `cards/season.c`（116 季节点）里**还有一份同源代码的第二次出现**
 *   （`points_tick` 里那段逐字同形的「slow + tint + jitter」）。本仓惯例是「出现第二次就提炼」，
 *   但 116 不在本卡的文件所有权内 ⇒ **本轮不动它**，改造它的人可以直接改调下面这两支
 *   （条件 / 颜色 / 抖动分布**现在已经完全一致**，接过去观感零变化，见 `faith.c` 的注释对照表）。*/
int   faith_slowdown_permille(void);     /* 减速因子 ×1000（1000 = 没减速）；供卡侧读 */
void  faith_dot_slow_style(uint32_t anm_id, uint8_t age, int slowed, float x, float y);

#endif /* FAITH_H */
