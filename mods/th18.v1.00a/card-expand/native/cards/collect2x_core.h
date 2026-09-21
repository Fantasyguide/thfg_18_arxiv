/* collect2x_core.h —— 收点系统（辉针城，id 114）纯逻辑：批计数 / 档位结算 / 碎片 4:1 循环 / 掉落节流。
 * 纯 C11、不进引擎地址，主机单测 tests/test_collect2x_core.c（make test-host）。
 *
 * 语义（spec §3 + plan Task 5；批闭门演进 §Z18「池空」→ §Z20「timer 30」→ §Z21「presence 续批 + 8」
 * → §Z22「成员门 = 真空飞行体」）：
 *  - 批 = 自第一枚「真空飞行收集」起的连续收点窗口（§Z22：批成员与开批事件 = type∈{1,2} 且
 *    分发时 state==3 —— V 门（上线/B/对话）放飞的收点道具；触收 state 1 / 70px 环收 state 4 = 普通
 *    游玩收集，由接线层挡在门外）；批闭判据（§Z21，TH14 v1.00b 一手定案，见常量注）=
 *    **每帧有批集道具在真空飞行（state 3）就续 idle（c2x_batch_presence）**；
 *    批闭 = 无批集道具在真空飞行、且无新收后 C2X_BATCH_SETTLE_IDLE 游玩帧 —— 等价「收点流真停 ~8 帧即结」。
 *    曾用「池空即闭」实跑三症状（音急 / NO BONUS 偶缺 / 躲弹幕时突然冒过期 ×0.5）→ §Z18 改 timer 门
 *    （idle 只认收点事件）；§Z20 因 TH18 波内吸收空洞 8–25 帧把窗口放大 8→30（0.5 s 尾延迟，
 *    第七跑「体感慢了」）→ §Z21 追完 TH14 0x43898e 原始语义 = presence 重载（DDC 无空洞问题，
 *    无需大窗；TH18 空洞 = 在飞散布 → 由续批盖掉），窗口回 8、结构照原义。
 *    计数集 = 引擎道具 type ∈ {1(P), 2(钱)}（§Z19 订正：TH14 官方文本「红点与蓝点」→ TH18
 *    无蓝点生态位，玩家收点道具 = P 与钱。type 9–0xe 是引擎分数转换点族 —— 分派映射表 0x44685c
 *    六 type 共指加分 handler 0x446547，只由强制收集（击破转换）产生；DDC 原生转换蓝点不计收点
 *    批（0x43898e type 9/0xa 不重载批计时）→ 不入。卡道具 0x10–0x13 等不入、不回喂）。
 *    ★ 2026-09-15 追加（`NEXT.md` §1x.2）：**自建实体**（110/111 的大 / 中绿点，`cards/faith.c`
 *    的 VM 池）**也入批**——它们是自建 VM、不入引擎道具池，成员门与 presence 门都扫不到
 *    （见 `sdk_core.h` 的「虚拟收集 / 虚拟在飞」层）。它们在 core 这一侧的入口 =
 *    `c2x_collect_virtual()`：**只加 count、不碰 batch_score**（绿点走信仰、不进 SCORE）；
 *    微档（消弹那档的半道具）不入批（§1x.3），由 SDK 层的 `CE_VITEM_HALF` 挡在门外。
 *  - 批闭（c2x_maybe_settle：批开且 idle 满足时才关）按 batch_count 走档位：
 *    <20 无奖；20/30/40/50/60 → ×0.5/0.8/1.2/1.6/2.0（mult_permille）。
 *    奖 1 枚碎片：<60 走 4:1 循环（cycle 0..3 → 符卡碎，cycle=4 → 命碎并归零）；≥60 命碎（不动循环）。
 *  - 分数补差 = batch_score×(mult_permille−1000)/1000 由接线层（cards/collect2x.c）经 ce_add_score_delta
 *    应用——core 在结算时把 batch_score 快照进 out 并清零自身（out.mult_permille == 0 时接线层忽略一切）。
 *  - 掉落节流（被动二，2026-09-08 设计变更：掉钱 → 掉 P）：间隔 = C2X_DROP_INTERVAL_BASE
 *    − power_x100×C2X_DROP_INTERVAL_PER/100（截断向零），钳 ≥1；frame ≥ next_drop_at 才发
 *    （frame 单调推进，由接线层注入；引擎 RNG 全程不碰 → replay 安全）。掉落物 type 由接线层定。 */
#pragma once
#include <stdint.h>

/* —— 常量（C2X_TABLE_*；主机测试可覆写 = 编译期改宏即可）—— */
#define C2X_TABLE_T1  20   /* 档位下界：count ≥ T1 起有奖 */
#define C2X_TABLE_T2  30
#define C2X_TABLE_T3  40
#define C2X_TABLE_T4  50
#define C2X_TABLE_T5  60   /* ≥ T5：×2.0 + 命碎（不计循环）*/

#define C2X_TABLE_M1  500  /* mult_permille：T1 档（×0.5）*/
#define C2X_TABLE_M2  800
#define C2X_TABLE_M3  1200
#define C2X_TABLE_M4  1600
#define C2X_TABLE_M5  2000

#define C2X_FRAGMENT_CYCLE  4  /* 4 符卡碎 : 1 命碎：cycle 0..3 → 符卡碎；cycle==4 → 命碎并归 0 */
/* ★ 碎片奖励呈现（2026-09-08 实跑纠偏）：type 4/6 残机碎/符卡碎宝石是旧引擎血统残留——
 * TH18 自己的资源卡掉落 = 道具 type 0x10–0x13 卡道具（05-shop-and-money §8 一手 dump：
 * 0x4b4020 表 type 0x10–0x13 行第一 dword = card_id 3/4/5/6）。收集分派 0x445A80 case 0x10–0x13
 * → allocate_new_card(table[type].card_id, 0)（mode 0 只调 ctor → ctor 型即时卡当场生效）。
 * spawn 卡道具本身 = 「原生碎片」：虹龙洞卡外观 + 原生收集链喂库存/HUD。*/
#define C2X_FRAG_BOMB  0x11    /* BOMB2 卡道具 → CardBombFragment(id 4)：符卡碎（碎片进位链）*/
#define C2X_FRAG_LIFE  0x10    /* EXTEND2 卡道具 → CardLifeFragment(id 3)：命碎（LIVES 链）*/

/* ★★ 结算弹字的 **y 偏移 = −48**（2026-09-18 一手补上；用户报「整个 bonus 的位置比正常的靠下了」）。
 *
 * ## TH14 原版逐指令（材料 `local/_scratch/th14_100b.exe`，地址即 VA）
 *   · **结算侧**（`ItemManager::on_tick__body` 结算尾）：
 *       `0x438dc6 movq xmm0,[0x4f5888]` + `0x438dce mov eax,[0x4f5890]` ⇒ 抄「批位」float3 进栈；
 *       `0x438de5 subss xmm0,[0x4c1b24]` ⇒ **y −= 48.0f**（该常量的值 = `0x42400000` = 48.0f）；
 *       同一份批位的 **x** 当声像：`0x438e03 movss xmm2,[esp+0x28]` → `push 0x39` →
 *       `call 0x46e470`（`play_sound_at_position`）⇒ 钟声声像 = 批位 x。
 *   · **批位是谁写的**：三个 P / 钱收集 handler 里**内联**了「加入批」三件事
 *     （`0x4391e9`/`0x4393d3`/`0x4395d5` 三处同形，紧随其后就是本条的写点）：
 *       `inc dword [0x4f5880]`（批计数 +1）、`add dword [0x4f5878], 身价/10`（批分累加，cap 1e9）、
 *       `movq xmm0,[reg+0x5e0]` + `movq [0x4f5888],xmm0` + `mov [0x4f5890],z`
 *       ⇒ ★ **批位 ← 自机坐标**（`reg+0x5e0/0x5e4/0x5e8` = TH14 自机的 float3 —— 同一个函数里
 *         「道具到自机距离」的减数就是这一对字段；三个站点的 reg 分别是自机，道具在另一个寄存器）。
 *   · **绘制侧没有第二处差异**：TH14 `0x457dc0` 与 TH18 `0x4644a0` 的倍率槽绘制**逐指令同形**，
 *     连常量都逐个相同 —— x `+224.0`（`[0x4c1b80]` / `[0x4b9348]`）、BONUS 行 y `+16.0`
 *     （`[0x4c1af4]` / `[0x4b9288]`）、数值行再 `+11.0`（`[0x4c1ae4]` / `[0x4b9278]`）。
 *
 * ## ⇒ 结论 / 我们的落差
 * 原版弹字两行落在 **自机 y − 32**（`−48+16`）与 **自机 y − 37**（`−48+11`）—— 即**自机上方**；
 * 我们从 2026-09-08 起只写自机坐标、**漏了这 −48** ⇒ 两行落在自机 y + 16 / +27 = **自机下方**，
 * 整体比原版低 48 px（用户 2026-09-18 实跑报的就是这一条）。
 * ⚠ 位置写的是**世界坐标 float3**（`CE_POPUP_POS`），偏移只作用在 y 上、**不要**动绘制常量。*/
#define C2X_POPUP_Y_OFFSET  48.0f   /* ★ 一手：TH14 `[0x4c1b24]` = 48.0f（`subss`，弹字 y 减去它）*/

/* ★★ 2026-09-21（P9 / `WORK-PACKAGES.md` §0 的 114 #1，用户：「原版应该是道具从 bonus 位置生成，
 * 而不是自机位置」）：**同一个 48.0f 也作用在碎片道具的生成点上** —— 「bonus 位置」是**一个点、
 * 两个消费者**。TH14 结算尾逐指令（材料 `local/_scratch/th14_full.asm`）：
 *   · 批位抄进栈上 float3：`th14:0x438dc6 movq xmm0,[0x4f5888]` + `th14:0x438dce mov eax,[0x4f5890]`
 *     → `th14:0x438dd9 movq [esp+0x28],xmm0` / `th14:0x438ded mov [esp+0x30],eax`；
 *   · 只改 y：`th14:0x438ddf movss xmm0,[esp+0x2c]` + `th14:0x438de5 subss xmm0,[0x4c1b24]`（= 48.0f）
 *     + `th14:0x438df8 movss [esp+0x2c],xmm0`；
 *   · **弹字**（五档 `call 0x458000`：`th14:0x438e20`/`0x438e67`/`0x438eb9`/`0x438f0b`/`th14:0x438f55`）
 *     与**碎片道具 spawn**（`th14:0x438fbc lea eax,[esp+0x28]` → `th14:0x438fda call 0x4397b0`，
 *     同形另三处 `th14:0x438ffc`/`th14:0x439028`/`th14:0x439047`）读的是**同一份指针**；
 *     连声像也取它（`th14:0x438e03 movss xmm2,[esp+0x28]`）。
 * ⇒ 原版弹字与道具**同一个锚**（批位 − 48），我们照做：这里做成 POPUP 常量的**别名**
 *   （一处定义、两处引用），别各写一个字面量。*/
#define C2X_BONUS_ANCHOR_Y_OFFSET  C2X_POPUP_Y_OFFSET

/* ★ 批闭 idle（游玩帧；§Z21 = TH14 presence 语义，值回原版 8）。
 * TH14 v1.00b 一手（th14_full.asm，AUDIT §Z18/§Z21）：批计时 `[0x4f5884]`；**重载点在 per-item 在飞体
 * 内、距离/吸收判定之前**（0x43898e：批集 kind ≠9/0xa/0xb → 每帧重载 `edx=0x8`；accessor `0x439d60`
 * = `mov [0x4f5884],0x8`），池循环帧尾 `0x438dad` 递减、归零即走结算 → **TH14 原义 = presence 续批**：
 * 还有批集道具在飞（吸附中）就续 idle，收点流真停后 ~8 游玩帧结；DDC 满火力收点线让到达紧密无空洞，
 * 该窗口只是尾延迟，不是批拆粒度。
 * ★ §Z20（第六跑「一次收点被拆两次结算」）把窗口放大 8→30 是**没追到 presence 语义时的替代品**：TH18
 * POC 线在但过线只吸引、不批结算（`player+0x47998`），道具经 70px swoosh 环/回收线真空以 ~5px/帧 磁速入 → 波内吸收空洞 8–25 帧常见，纯 timer 窗
 * 必须 ≥ 空洞上界才不拆批，代价 = 流停后最多 0.5 s 才发奖（第七跑「体感慢了」）。§Z21 追完一手：TH18
 * 侧同样有 presence 谓词 —— 批集道具 state == 3 = 真空飞行（V 门翻 0x445f6f；state 4 = 70px 环 swoosh
 * 是普通游玩磁吸，§Z22 收窄出不续批，置位 0x446647；模型见 engine.h CE_ITEM_STATE 注）→ 每帧接线层
 * 扫描、有则 c2x_batch_presence 续 idle → 窗口只需盖「真空空 + 无新收」的真停尾段 → 回原生 8（0.13 s）。
 * §Z22（第九跑）另加**成员门**（接线层 on_item_collect）：批成员/开批事件只认分发时 state==3 的
 * {1,2} 收集 —— 普通游玩触收/环收既不开批也不续批，根治非全屏收点模式的误 NO BONUS；窗口兜底
 * （§Z21 尾）随成员门落地作废：若仍有体感拆批，先提窗口到 16、再查谓词。*/
#define C2X_BATCH_SETTLE_IDLE   8

#define C2X_DROP_INTERVAL_BASE  96  /* 掉落间隔基数（帧）：power 0.00 时 */
#define C2X_DROP_INTERVAL_PER   16  /* 每 power 1.00（power_x100 = 100）减 16 帧 → 4.00 时 32 */
#define C2X_DROP_INTERVAL_MIN    1

/* 收点状态（每张卡一份，ce_state 惯例存卡上下文）。batch_score 由接线层在提交断点累计。*/
typedef struct {
    int64_t  batch_score;   /* 本批计数集道具的分值累计（倍率乘在它上面；结算时清零）*/
    int32_t  batch_count;   /* 本批已收计数集道具数 */
    int32_t  fragment_cycle;/* 4:1 循环游标 0..4 */
    int32_t  batch_open;    /* 批开（自首个计数集收集事件起）*/
    int32_t  last_event_at; /* 批事件帧：最近一次收集或吸附中 presence（c2x_collect_batch /
                               c2x_batch_presence 记录；idle 批闭判据）*/
    int32_t  next_drop_at;  /* 被动二：下一枚掉落最早帧（0 = 立即可发；现掉 P）*/
    int32_t  pending_frag_type;   /* 0 = 无待弹碎片；结算帧只登记不 spawn（TH14 节奏：先结算后弹卡）*/
    int32_t  pending_frag_count;
    int32_t  pending_frag_at;     /* frame_now ≥ 此帧才弹（接线层在 on_item_frame 检查）*/
    /* —— ★★ 2026-09-21（P9 / 114 #1）：**bonus 锚点**（TH14 的「批位」float3 `th14:0x4f5888`）——
     * 由**接线层**写（`cards/collect2x.c` 的 `note_bonus_anchor()`）：成员加入批那一刻记**当下自机坐标**，
     * 与原版三个 P/钱 handler 内联写点**同一事件、同一值**（一手见 core.h 上方的
     * `C2X_BONUS_ANCHOR_Y_OFFSET` 注）。core 这侧只当数据带着、不解释。
     *   · `anchor_*` = 本批**最后一次**成员加入时的自机位（最后写者胜，原版同）；
     *   · `pending_frag_*` = **结算那一刻**的锚点快照 —— 碎片是**延迟 20 帧**才弹的，
     *     这 20 帧里下一批的成员加入会改写 `anchor_*`，快照保证弹点仍是本批的 bonus 位置。
     * ⚠ 两个 **不预减 48**：`C2X_BONUS_ANCHOR_Y_OFFSET` 在**用时**加（弹字与道具共用，原版同）。*/
    float    anchor_x;           /* 本批批位 x（= 最后一次成员加入时的自机 x）*/
    float    anchor_y;           /* 本批批位 y（同上）*/
    int32_t  anchor_valid;       /* 批位写过没有（批开 ⇒ 必为 1；防御用）*/
    float    pending_frag_x;     /* 结算帧的批位快照 x（延迟弹碎片用）*/
    float    pending_frag_y;     /* 结算帧的批位快照 y（同上）*/
} c2x_state_t;

/* 结算输出。mult_permille == 0 → 无奖（<20）：此时 fragment_* 无意义（为 0），
 * ★ 而 batch_score **仍带关批那一刻的快照**（2026-09-18 B11 之后它可能非零 —— 自建实体也进批分）
 *   —— 接线层在 `mult == 0` 时走 ×0 分支早退、**不用**这个值（无奖 = 无补差）。*/
typedef struct {
    int32_t  mult_permille;   /* 500/800/1200/1600/2000 */
    int32_t  fragment_type;   /* C2X_FRAG_BOMB / C2X_FRAG_LIFE / 0 */
    int32_t  fragment_count;  /* 本次结算应弹碎片数（当前恒 1）*/
    int64_t  batch_score;     /* 本次被结算的批分值（快照；core 已清零自身）*/
} c2x_settle_out_t;

/* 批集判定 + 计数：type ∈ {1(P), 2(钱)} → count++、开批、记 last_event_at=frame，返回 1；其余 type → 0。*/
int c2x_collect_batch(c2x_state_t *s, uint32_t type, int32_t frame);

/* ★★ 2026-09-15（`NEXT.md` §1x.2 **成员通道**）：**自建实体**（110/111 的大 / 中绿点）被收 ⇒ 入批。
 * 语义与 `c2x_collect_batch` **只差 type 门**：自建实体没有引擎 type / state（它是我们自己的 VM），
 * 「算不算收点」由生产者自己的收集判定给出（`cards/faith.c` 的 `dot_collect()`，微档不进，
 * 见 §1x.3）⇒ 这里负责「开批 + count += n + batch_score += value + 记 last_event_at」。
 * ★★ **2026-09-18（B11）：`batch_score` 里只有原生 P/钱** —— 用户口径：「**绿点的面值不进
 *   `batch_score`，它加的不是分数**」（绿点加的是信仰，不是 SCORE）⇒ 这一支**只动 count**，
 *   不碰 `batch_score`（2026-09-18 曾加过 `value` 参数并当天回退，别再走回头路）。
 *   ⇒ 纯绿点批的补差恒 **0 是设计**：弹字的价值行会是 0、补差 0，但档位 / 碎片照出。
 *   ★ 那么「一批 54 颗纯绿点批」本身是怎么来的？—— 用户实跑那次是**死亡**（真空门的死亡分支
 *   把全场绿点一次吸走）。那条属于**另一条 bug**，已在生产者侧修掉（`cards/faith.c` 的
 *   `faith_vacuum_gate()` 去掉「自机 state 2/4」分支）⇒ 纯绿点的大批不该再出现在死亡那一刻。
 * n <= 0 / s 为空 → 0（不做事、不返回「已计数」）。 */
int c2x_collect_virtual(c2x_state_t *s, int32_t n, int32_t frame);

/* presence 续批（§Z21，TH14 0x43898e 原义）：批开时每帧由接线层在有批集道具吸附中（TH18 state 3/4）
 * 调它 → 记 last_event_at=frame，返回 1；批未开 → 0（无事可续）。只续 idle 不计数。*/
int c2x_batch_presence(c2x_state_t *s, int32_t frame);

/* 帧 tick 用：批开且 idle 满足（frame − last_event_at ≥ C2X_BATCH_SETTLE_IDLE）→ 关批、按档位填 out，
 * 返回 1；批未开 / idle 未满 → out 全零，返回 0（批保持开）。frame = 单调游玩帧（接线层注入）。*/
int c2x_maybe_settle(c2x_state_t *s, int32_t frame, c2x_settle_out_t *out);

/* 掉落节流：frame ≥ next_drop_at → 置 next_drop_at = frame + 间隔(power_x100)，返回 1（本帧该掉）；
 * 未到点 → 0（不重排）。间隔 = c2x_drop_interval(power_x100)。power_x100 = power×100（0..400）。*/
int c2x_maybe_drop(c2x_state_t *s, int32_t frame, int32_t power_x100);

/* 纯函数：掉落间隔（帧）。 */
int32_t c2x_drop_interval(int32_t power_x100);
