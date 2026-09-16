/* 辉针城（id 114，内部 COLLECT_2X）—— 被动卡，双被动（spec §3–4，逻辑 collect2x_core.h，接线本文件）：
 * id 从 254 改为 127（2026-09-08 实跑）：编成 deck 槽对 id > 0x7f 当负值剔除（原生符号扩展界），
 * 新卡上限 → 127，后卡 126、125… 降序（AUDIT §Z11）。
 *
 *   · 收点系统（TH14 1:1 复刻）：一次性收点 = 自第一枚「飞过来的批集道具」（state 3 真空体）被收起的
 *     一批（§Z22 成员门：批成员/开批事件只认真空飞行收集 —— 触收 state 1 / 70px 环收 state 4 是普通
 *     游玩收集，不入批不开批：run-9 = 非全屏收点模式下误触发 NO BONUS）；御币式持续击杀 / 上线 /
 *     bomb / 对话自动收的连续收点会拉长同一批。批闭 = **无批集道具在真空飞行（presence）且无新收后
 *     8 游玩帧**（窗口值 = `C2X_BATCH_SETTLE_IDLE`；TH14 原版 timer `[0x4f5884]` presence 语义
 *     一手见 AUDIT §Z18/§Z21：0x43898e per-item 在飞体对批集 kind 每帧重载 0x8，帧尾递减归零即结）。
 *     on_item_frame 每帧扫池：有 type∈{1,2} 的道具 state==3（真空飞行；4 = 70px 环 swoosh 是普通
 *     游玩磁吸、不续批，§Z22 收窄）→ `ce_item_pool_any_attracting` → `c2x_batch_presence` 续 idle ——
 *     TH18 波内吸收空洞 8–25 帧（§Z20 六跑拆批根因）= 真空在飞散布 → presence 盖掉，批只在收点流
 *     真停 ~0.13 s 后结（§Z21 第七跑「体感慢了」：30 帧窗的 0.5 s 尾延迟回原生 8；8→30 史见常量注）。
 *     落地休止/抛物中的场道具不续批（TH14 原义）：迟到残余仍拆下一批。曾用「池空即闭」实跑三症状
 *     （音偏急 / NO BONUS 偶缺 / 躲弹幕时突然冒过期 ×0.5 弹字）→ §Z18 改 timer 门、§Z21 门上加
 *     presence。批闭按 batch_count 走档位 20/30/40/50/60 → ×0.5/0.8/1.2/1.6/2.0 + 碎片（<60 走 4
 *     符卡碎 : 1 命碎循环，≥60 命碎、不入循环）。倍率只作用分数：SCORE 补 batch_score×(mult−1000)/1000
 *     （×0.5/×0.8 = 负补差，1:1 原义）。
 *   · 高速射击掉 P（2026-09-08 设计变更：原「掉金钱」实跑未出现，用户拍板改掉 P）：非聚焦射击
 *     命中敌人（断点 ce_shot_damage 已按聚焦位 / bomb / param_6 门控）→ 按当前火力节流掉火力道具
 *     type 1（命中点）：间隔 = 96 − power×16/100 帧（core 节流，帧源见 frame_now）。
 *   · ★★ 2026-09-15（`NEXT.md` §1x.2）：**自建实体（110/111 的大 / 中绿点）也入批**（用户口径
 *     「大、中绿点会计入 th14 收卡的计数」+「绿点也得当作正在飞行的道具」）。它们是自建 VM
 *     （`cards/faith.c` 的池），成员门（`on_item_collect` 的 type{1,2}+state 3）与 presence 谓词
 *     （`ce_item_pool_any_attracting()` 扫原生池）**都碰不到** ⇒ 批会偏小、而且可能在绿点还在飞时
 *     就结算。两条通道都接在 SDK 的声明式层上（`sdk_core.h` 的「虚拟收集 / 虚拟在飞」，
 *     生产者只声明、本卡只读）：
 *       ① **成员**：`on_item_frame` 开头抽干 `ce_vitem_take_collect()` → `c2x_collect_virtual()`
 *          —— **在 `batch_open` 门之前**（第一颗绿点就能开批）、**只加 count、不碰 batch_score**
 *          （绿点走信仰、不进 SCORE ⇒ 也不进批分，结算补差对纯绿点批恒 0。用户：「只 +1」）；
 *       ② **presence**：`ce_vitem_flying() > 0`（绿点还在飘/被吸）与原生谓词**或**起来 ⇒ 续 idle
 *          —— 修「我们这些点还在飞的时候就提前结算」。
 *     **批的档位 / 倍率 / 碎片 / 批分算法一个字没改**；微档（消弹那档的半道具）不入批（§1x.3），
 *     由 SDK 层的 `CE_VITEM_HALF` 挡在生产者一侧（`faith.c`）。诊断两行新日志见 `on_item_frame`。
 *
 * 断点（sdk.c 头表字节一手 + AUDIT §Z）：ce_item_frame 0x446ee3（每帧、池 body 前）/
 * ce_item_collect 0x44625f（type 分派前，分值副作用未发生）/ ce_item_commit 0x44658a（分值已入 SCORE、
 * 公共收尾前）/ ce_shot_damage 0x45f2a2。
 *
 * 批次分值捕获（stash）：收集断点在 type 分派**前** stash 当时的 SCORE，提交断点累加 (SCORE−stash)
 * —— 两断点夹住引擎的 type 分值加法器，差量即本道具的分值增量（P 道具无分值 → 0）。stash 只对批集
 * 收集置位、任意提交后失效：批集道具按池槽顺序「收集→提交」背靠背走完才轮到下一道具，中间夹的
 * 碎片等非批集提交见不到活 stash，不会把分值混进批（AUDIT §Z「stash 属接线层」）。
 * P/钱**都**自带小分（§Z19 尾段订正，推翻早先「P/钱不动 SCORE」推断 —— 早先只看 handler stub 没读
 * 例程体）：P 例程 0x446870 尾 0x446ac0 与钱例程 0x446b00 尾 0x446d03 都无条件 `SCORE += 身价/10`
 * （cap 1e9）→ stash 差量 = 该道具小分，批分 = 批内 Σ小分、**非零**，倍率补差有真实来源
 * （TH14 的「收点分×档」源 = 蓝点面值；TH18 无蓝点，对应物 = P/钱小分）。
 *
 * 早结敞口（Task 9 实跑看）：批闭判定只挡 state∈{1,3,4}；若存在「收集了、分值提交滞后到批闭之后」
 * 的道具（跨帧延迟路径），那一枚的分值会漏出本批 —— 观察日志里的 settle/commit 滞后条目，扩判定集即可。 */
#include "sdk.h"
#include "collect2x_core.h"
#include <string.h>

static int32_t s_stash;             /* 批集收集断点存的 SCORE（提交断点差量用）*/
static int     s_stash_valid;       /* stash 只对「批集收集后、下一次提交前」有效（见文件头）*/

/* ★★ 2026-09-15（`NEXT.md` §1x.2）：**自建实体**（110/111 的大 / 中绿点）的两个诊断计数。
 * 都与批的算法无关（批的档位 / 倍率 / 碎片一个字没改）—— 只为实跑时能在日志里看见两件事：
 *   ① 批里**有几个成员是自建实体**（成员通道在动）；
 *   ② 批有没有被**自建实体续过命**（presence 通道在动；原生池空的时候才记，避免与原生重复计数）。*/
static int32_t s_batch_virtual;     /* 本批累计的自建实体成员数（批闭时清零）*/
static int32_t s_batch_vhold;       /* 本批有没有「只靠自建实体在飞」续过 idle（批闭时清零）*/
static int32_t s_virtual_total;     /* 本 run 累计（`on_run_reset` 清零）*/

/* 帧源（2026-09-08 §Z14 订正）：card+0x20 的 zTimer.cur 只在**主动卡**释放模板的 __on_tick_2 末尾递增
 * （engine/card/th18/01-object-model.md §3 一手证据全在主动卡路径；状态切换还清零）——本卡 0x54 纯被动，
 * 那钟不可靠。实跑佐证：碎片改挂它延迟 20 帧后就完全不出。改用接线层自计数：on_item_frame 分派点
 * 0x446ee3 是 ItemManager tick 0x446ec0 包装的门卫 CMP——包装先查 [0x4cf2e4]+0xb0 状态位和开店
 * [0x4cf2a4]，冻结帧连 CMP 都到不了（sites.py 711 一手 disasm），所以每次分派 = 一个**正常游玩帧**。
 * s_play_frame = 本 run 游玩帧计数：单调、随游玩走、暂停/对话/商店帧不计（节流/延迟要的正是这个）。 */
static int32_t s_play_frame;

static int32_t frame_now(void)
{
    return s_play_frame;
}

static void spawn_item_at(uint32_t type, float x, float y, float speed)
{
    float p3[3] = { x, y, 0.0f };
    if (!ce_spawn_item(type, p3, -1.5707964f, speed))   /* 角度 −π/2（向上）；速率按场景：P/钱 = 2.2 原生 main-drop，碎片卡 = 1.2 短弧 */
        ce_log("collect2x: spawn type %u failed (pool full?)", type);
}

/* settle 档位色 = TH14 原版地面真值（plan Task 8；mult_permille 与档位一一对应，灰蓝→青→绿→黄绿→黄）*/
static uint32_t tier_color(int mult_permille)
{
    switch (mult_permille) {
    case 2000: return 0xffffff00;   /* ≥60  ×2.0 */
    case 1600: return 0xe0ffff60;   /* 50–59 ×1.6 */
    case 1200: return 0xd0c0ff80;   /* 40–49 ×1.2 */
    case 800:  return 0xc0a0ffa0;   /* 30–39 ×0.8 */
    default:   return 0xb040ffff;   /* 20–29 ×0.5 */
    }
}

/* 倍率弹字：喂 TH18 原生 ×倍率环（PopupManager [0x4cf420] 槽 k13–17 @ +0x9cc）。写入后
 * tick 0x464020 驱动位移动画、age>60 起每帧 α−4 淡出（≤0 清 busy），draw 0x464180 以文本管理器
 * [0x4cf2ac] 渲染 —— 生命周期全原生，这里只在 settle 时写一次、busy 最后置（防同帧 BP 再入
 * 读到半初始化槽）。★ anim 字段（+0x20/+0x24/+0x28）必须照 adder 0x4645f0 复位：原生回收只清 busy，
 * age（+0x24）不复零的话 tick 看到残留 >60 会当帧淡出回收、位移 float（+0x28）残留会瞬移。
 * 值 int 恒 ≥0（结算分）→ draw 走 "BONUS %.1f" 分支，不会出 "NO BONUS"。AUDIT §Z。 */
static int ce_popup_mult(const float *pos, int value, uint32_t color, float mult)
{
    uint8_t *mgr = CE_POPUP_MGR();
    if (!mgr) return 0;
    for (int k = 0; k < CE_POPUP_RATE_SLOTS; ++k) {
        uint8_t *s = mgr + CE_POPUP_RING_RATE + (size_t)k * CE_POPUP_STRIDE;
        if (*(uint8_t *)(s + CE_POPUP_BUSY)) continue;              /* 占着：原生会自动回收，下个 settle 再看 */
        *(int32_t *)(s + CE_POPUP_AGE - 4) = -1;                    /* +0x20 上一帧 age（adder 初值）*/
        *(int32_t *)(s + CE_POPUP_AGE) = 0;                         /* age 归零（见上）*/
        *(int32_t *)(s + CE_POPUP_AGE + 4) = 0;                     /* +0x28 位移动画 float */
        if (!(*(uint32_t *)(s + 0x30) & 1)) {                       /* 动画类标志 bit0（adder 语义）*/
            *(uint32_t *)(s + 0x30) |= 1;
            *(int32_t *)(s + 0x2c) = 0;
        }
        *(float *)(s + CE_POPUP_POS) = pos[0];
        *(float *)(s + CE_POPUP_POS + 4) = pos[1];
        *(float *)(s + CE_POPUP_POS + 8) = pos[2];
        *(float *)(s + 0x18) = 1.0f;                                /* scale（tick 弹出动画的起点）*/
        *(uint32_t *)(s + CE_POPUP_COLOR) = color;
        *(int32_t *)(s + CE_POPUP_VALUE) = value;                   /* draw 的 "%d" 行（值）*/
        *(float *)(s + CE_POPUP_MULT) = mult;                       /* draw 的 "BONUS %.1f" 行（倍率）*/
        *(uint8_t *)(s + CE_POPUP_BUSY) = 1;
        return 1;
    }
    ce_log("collect2x: popup rate ring full (5 busy in <60 frames?) — drop visual");   /* 只可能连刷 5+ 批 */
    return 0;
}

/* 碎片从结算帧推迟 N 帧弹（TH14 节奏 1:1：结算瞬间只有弹字/分值，音效与碎片稍后才来；
 * 2026-09-08 用户实跑：「现在是出现 bonus 的同时瞬间生成碎片」→ 改延迟。体感值，可调。）*/
#define C2X_FRAG_SPAWN_DELAY_FRAMES 20

static void spawn_pending_frags(ce_card_t *c, c2x_state_t *s)
{
    (void)c;
    const int n = s->pending_frag_count;
    uint8_t *p = CE_PLAYER();
    const float px = p ? *(float *)(p + CE_PLAYER_X) : 0.0f;
    const float py = p ? *(float *)(p + CE_PLAYER_Y) : 0.0f;
    const float sy = py - 76.0f >= 40.0f ? py - 76.0f : 40.0f;   /* 贴顶保底（近顶时无条件真空，无碍）*/
    /* 弹点锚定自机上方 76px（★ 半径一手钉死：0x45ae67 起初始化 [player+0x47988]=5.0 磁速、
     * [+0x4798c]=0x41f00000=30.0 直接收集闸（0x44624a：dist²<30² → 0x44625f 分派）、
     * [+0x47990/0x47994]=0x428c0000=70.0 swoosh 闸（0x4465fb/0x44660e，type 9–0xe 豁免 0x44661d 系）、
     * [+0x47998]=128/224 回收线（0x45ae8f/0x40c275；player.y ≤ 线 → 无条件真空 0x445ffc 系）。
     * spawn ≤70px → 当帧 swoosh ≈「瞬间回收」；76 = 70 + 6 余量，初速 1.2（apex ≈ +24px：上抛 ~40f
     * → 落回 70 被吸 ~45f，全程 ~1.4s 短弧）。延迟期间自机可移动 → 弹点取**当下**自机位。 */
    for (int i = 0; i < n; ++i)
        spawn_item_at((uint32_t)s->pending_frag_type,
                      px + ((float)i - (n - 1) * 0.5f) * 12.0f, sy, 1.2f);
    ce_log("collect2x: frag card type 0x%x x%d spawn @(%d,%d) t=%d", s->pending_frag_type, n,
           (int)px, (int)sy, s_play_frame);
    s->pending_frag_type = 0;
    s->pending_frag_count = 0;
}

static void on_item_frame(ce_card_t *c)
{
    c2x_state_t *s = ce_state(c, c2x_state_t);
    ++s_play_frame;                                     /* 游玩帧计数（帧源注释：分派 = 游玩帧）*/
    const int32_t now = s_play_frame;
    if (s->pending_frag_count && now >= s->pending_frag_at)
        spawn_pending_frags(c, s);                      /* 延迟到点：从当下自机位弹卡（批闭与否无关）*/

    /* ★★ 2026-09-15（`NEXT.md` §1x.2 **成员通道**）：自建实体（110/111 的大 / 中绿点）被收 ⇒ 入批计一员。
     * 来源 = SDK 的「虚拟收集」声明层（`sdk_core.h`）：`cards/faith.c` 在它**自己的收集判定**
     * 命中的那一刻报一颗（`ce_vitem_note_collect`），这里抽干、按颗数计入本批。
     *   · **在 `batch_open` 门之前**：第一颗绿点自己就能开批（§Z22 的成员门只针对**引擎原生**道具的
     *     真空谓词 —— 自建实体没有 type / state，「算不算收点」由生产者的收集判定给出）；
     *   · **不进批分**：`c2x_collect_virtual()` 只动 count、不碰 batch_score ⇒ 结算补差对纯绿点批
     *     恒 0（绿点走信仰槽，不进 SCORE —— 用户口径「只 +1」）；
     *   · 微档不进（§1x.3 的半道具）：由 SDK 层的 `CE_VITEM_HALF` 挡在生产者那一侧。
     * 顺序：与卡 tick（生产者）同帧谁先谁后都读得到（层带 2 帧容差，见 `sdk_core.h` 契约 ④）。*/
    {
        const int vn = ce_vitem_take_collect();
        if (vn > 0) {
            c2x_collect_virtual(s, vn, now);
            s_batch_virtual += vn;
            s_virtual_total += vn;
        }
    }
    if (!s->batch_open) return;

    /* §Z21/§Z22 presence 续批：本帧有批集道具在真空飞行（state == 3）→ 续 idle。TH18 波内吸收空洞
     * （8–25 帧，§Z20）= 真空在飞散布 → 空洞被续批盖住，idle 窗（C2X_BATCH_SETTLE_IDLE=8，原生值）
     * 只兜「真空空 + 无新收」的真停尾段（用户 run-9 语义：下收点线后残余在飞的 P 继续算、收完即结）。
     * §Z22 收窄 state 4：70px 环 swoosh 是普通游玩磁吸（任何时刻都发生），不属全屏收点 → 不续批
     * （run-9 症状根因之一）。扫池成本：每游玩帧 ≤4696 槽 × 2 dword，批开期才扫；早停。
     * ★★ 2026-09-15（§1x.2）：**自建实体在飞也算 presence** —— `ce_vitem_flying() > 0`（110/111 的
     *   大 / 中绿点还飘着/被吸着）⇒ 同样续 idle。没有它，一批会在我们这些点**还在飞的时候**被判
     *   「收点流停了」而提前结算（用户点名的毛病；批偏小 + 边界错）。这一路只**续命**、不计数
     *   （计数走上面的成员通道）⇒ 与原生谓词是**或**关系，不改变原生那一支的语义。*/
    const int native_flying  = ce_item_pool_any_attracting();
    const int virtual_flying = native_flying ? 0 : ce_vitem_flying();   /* 原生那一路命中就不必再问自建 */
    if (native_flying || virtual_flying > 0) {
        if (!native_flying && !s_batch_vhold) {
            /* 只靠自建实体续着（原生池里一颗真空在飞的都没有）—— 记一次，实跑能看见这条路在动。
             * 每批只记一次（`s_batch_vhold` 在批闭 / 开新关时清零）。*/
            s_batch_vhold = 1;
            ce_log("collect2x: batch held by virtual items in flight (%d flying, count %d, t=%d)",
                   virtual_flying, s->batch_count, now);
        }
        c2x_batch_presence(s, now);
        return;                                         /* 有真空在飞 / 自建在飞：本帧必然不结（idle 刚续）*/
    }

    const int32_t count = s->batch_count;               /* 关批前留档（settle 后 count 已清零，日志用）*/
    c2x_settle_out_t out;
    if (!c2x_maybe_settle(s, now, &out)) return;        /* 批开但 idle 未满：等（presence+timer 批闭门）*/

    /* ★ 2026-09-15（§1x.2）：本批的自建实体成员数 / 有没有被自建实体续过命 —— 批闭即清零，
     * 并在批里真的含自建实体时补一行日志（**不改 settle 那一行的格式**：它是 NEXT/AUDIT 里的对账标记）。
     * 判据（实跑读法）：`virtual 0` = 110/111 没在场或没收到绿点；`virtual N` 且 `batch score` 为 0
     * ⇒ 纯绿点批（不进批分、补差 0）的签名。*/
    {
        const int32_t vmembers = s_batch_virtual;
        const int32_t vheld = s_batch_vhold;
        s_batch_virtual = 0;
        s_batch_vhold = 0;
        if (vmembers > 0)
            ce_log("collect2x: batch had %d virtual (green-dot) members of %d, virtual-presence=%d (t=%d)",
                   vmembers, count, vheld, now);
    }

    uint8_t *p = CE_PLAYER();
    const float px = p ? *(float *)(p + CE_PLAYER_X) : 0.0f;
    const float py = p ? *(float *)(p + CE_PLAYER_Y) : 0.0f;
    float pz[3] = { px, py, 0.0f };

    if (!out.mult_permille) {
        /* 5–19 批 ×0 档：TH14 原版真值（0x438dfe 系）= 灰弹字、无碎片无分；<5 连弹字都没有。
         * value −1（int）→ draw 0x464520 cmpl 走 jl → 0x464572 只弹 "NO BONUS"（fmt 0x4b7204），
         * 不画 %d 行也不画 mult 行（×0 时 TH14 压入的正是 −1 + 0x80a0a0a0 + mult 0.0）。 */
        if (count >= 5) {
            if (!ce_popup_mult(pz, -1, 0x80a0a0a0, 0.0f))
                ce_log("collect2x: ×0 NO BONUS popup dropped (rate ring full)");
        }
        ce_log("collect2x: batch of %d closed below tier 20 (×0, no award)", count);
        return;
    }
    int64_t delta = out.batch_score * (int64_t)(out.mult_permille - 1000) / 1000;
    if (delta) ce_add_score_delta(delta);               /* 钳制在帮手内（可为负）*/
    /* 碎片奖励 = TH18 原生卡道具 0x10 EXTEND2 / 0x11 BOMB2（实跑纠偏：type 4/6 残碎/符卡碎宝石是
     * 旧引擎血统残留，虹龙洞自己的碎片 = 卡掉落——卡外观 + 收集走原生分配链喂库存；常量注见 core.h）。
     * ★ 2026-09-08 节奏变更：结算帧只登记 pending（TH14 = 结算音效先、碎片隔一会才弹），
     * C2X_FRAG_SPAWN_DELAY_FRAMES 帧后在 on_item_frame 从当下自机位 spawn。0x10/0x11 不进
     * 池空判定（批集 {1,2}，§Z19），延迟在飞不挡批闭。连续两次结算间隔 < 延迟窗口 → 后者覆盖前者（日志见）。 */
    if (out.fragment_count) {
        if (s->pending_frag_count)
            ce_log("collect2x: frag pending overwritten (new settle before spawn window)");
        s->pending_frag_type = out.fragment_type;
        s->pending_frag_count = out.fragment_count;
        s->pending_frag_at = now + C2X_FRAG_SPAWN_DELAY_FRAMES;
    }
    ce_log("collect2x: settle %d items, batch score %lld -> x%d.%03d, +%lld score, frag type 0x%x x%d (t=%d)",
           count, (long long)out.batch_score, out.mult_permille / 1000, out.mult_permille % 1000,
           (long long)delta, out.fragment_type, (int)out.fragment_count, now);
    /* 倍率弹字（原生环；纯视觉）。值 = 结算分（≥0 → draw 走 "BONUS" 分支）；位置取自机位同碎片。 */
    ce_popup_mult(pz, (int)out.batch_score, tier_color(out.mult_permille), out.mult_permille / 1000.0f);
    /* ★ 收点完毕音（2026-09-08 §Z17，三跑缺音 → §Z16 曾拟 0x2e 后被一手推翻）：
     * TH14 v1.00b 原版结算真值 = se id 0x39：on_tick__body 结算尾 0x438dfe 起，≥20 五档各自在弹字
     * 前 `push 0x39 → SoundManager__play_sound_at_position 0x46e470`（声像 xmm2 = 批位 x，438e03-09
     * 起系）。id 0x39 → cfg 行（0x4d61b8+0x39*0x14）+4 = wav 下标 0x2e → 名表 0x4d60b0 → se_pin00.wav，
     * 调校 −2dB/prio 100；TH18 的 0x39 行（sound-table §7）= 同 wav 同调校 → 直用 0x39。
     * 全 exe `push 0x39` 播音仅此 5 处（0x47b78b/0x49f6c5 系是对象构造参数）—— pin00 在 DDC 就是
     * 收点结算的钟声，0x2e se_cardget（商店买符卡）语义不符已弃。5–19 灰字无音（×0 早 return 在其上）
     * 与原生一致；碎片弹出帧原生无声（收集时另有原生道具声）。时机体感由批闭 idle 门决定
     * （§Z20：最后一收后 30 游玩帧 = 0.5 s，TH18 滴入物理适配值；原生 TH14 8 帧见 §Z18）；
     * 仍偏早/偏晚就调 C2X_BATCH_SETTLE_IDLE。 */
    ce_play_sound(CE_SE_PIN00, pz[0]);
}

static void on_item_collect(ce_card_t *c, uint32_t type, const void *item)
{
    /* §Z22 成员门：批 = 全屏收点（上线 / 放 B / 对话真空）**飞过来**的收点道具（run-9：普通游玩
     * 收集也会把批撑过 5 → 误弹灰 NO BONUS）。type {1,2} 进入 state 3 的唯一入口 = V 门翻 0x445f63
     * （0x445b9a–0x445be0：上线 / 放 B 前 60 帧 / 对话管理器；state-2 顶点翻 3 只覆盖 state-5 出生的
     * 9–0xe 分数转换点族，P/钱不经过 state 2/5 —— state 机模型 engine.h CE_ITEM_STATE 注）。
     * 触收（state 1）与 70px 环收（state 4）不开批、不计数。引擎 handler 0x44699e/0x446c0a 分发时
     * 也读 state==3 给全额 —— 同一谓词。type 9–0xe 转换点高频、先挡。 */
    if (type != 1 && type != 2) return;
    uint32_t st = item ? *(const uint32_t *)((const uint8_t *)item + CE_ITEM_STATE) : 0;
    if (st != CE_ITEM_STATE_VACUUM) return;             /* 非真空收集（触收/环收/异常槽）：不入批 */
    c2x_state_t *s = ce_state(c, c2x_state_t);
    c2x_collect_batch(s, type, frame_now());            /* type 已收窄进批集：必然开批/计数 */
    s_stash = CE_SCORE();                               /* type 分派前：引擎的分值加法器还没跑 */
    s_stash_valid = 1;
}

static void on_item_commit(ce_card_t *c)
{
    c2x_state_t *s = ce_state(c, c2x_state_t);
    if (!s_stash_valid) return;                         /* 无活 stash（碎片提交 / 批外提交）：不入账 */
    int64_t delta = (int64_t)CE_SCORE() - (int64_t)s_stash;
    s_stash_valid = 0;
    if (!s->batch_open) {                               /* 早结敞口：stash 活到批闭之后才提交 → 分值漏出本批 */
        ce_log("collect2x: commit +%lld after batch closed (lagging item; see AUDIT Z7)", (long long)delta);
        return;
    }
    s->batch_score += delta;
}

static void on_shot_damage(ce_card_t *c, int32_t damage, const float *pos)
{
    (void)damage;
    c2x_state_t *s = ce_state(c, c2x_state_t);
    if (!c2x_maybe_drop(s, frame_now(), CE_CURRENT_POWER())) return;  /* 火力 1.00=100 起（×100 int）*/
    /* 被动二 2026-09-08 设计变更：掉钱(type 2) 实跑未出现 → 用户拍板改掉 P 火力道具(type 1)。
     * ★ 落点（§Z15 定案，用户要求「从敌人掉」）：pos = sdk.c BP_ce_shot_damage 解引用 [ebx+8] 得来
     * —— 0x45f0f0 本帧被结算那个敌人的中心 {x,y}（callee 以它减各源中心判重叠；圆形分支纯点测试、
     * 矩形分支 0x404440）。从此弹 = P 从敌人身上顶出后落下（引擎道具重力把它带回 swoosh/自机行）。
     * type 1 ∈ 计数批集 {1,2}（§Z19）：命中掉 P 被吃 → count++ 拉长收点批（梦A式持续击杀语义）；
     * P 满 4.00 后引擎把多余 P 转分，无碍。初速 2.2 向上小抛 = 原生 main-drop 实参。*/
    spawn_item_at(1, pos[0], pos[1], 2.2f);
    ce_log("collect2x: P drop (power %d, t=%d) @(%d,%d)", CE_CURRENT_POWER(), s_play_frame,
           (int)pos[0], (int)pos[1]);
}

static void on_stage_start(ce_card_t *c)
{
    c2x_state_t *s = ce_state(c, c2x_state_t);
    s->batch_open = 0; s->batch_count = 0; s->batch_score = 0;  /* 上一关末帧的收点批不带进新关 */
    s->pending_frag_count = 0; s->pending_frag_type = 0;        /* 未到点的待弹碎片也丢（跨关无意义）*/
    s_stash_valid = 0;
    s_batch_virtual = 0; s_batch_vhold = 0;             /* ★ §1x.2 的自建实体诊断计数：与批同生命周期 */
    s->next_drop_at = 0;                                /* 新关第一发命中即可掉（计数不归零：0 ≤ 任何游玩帧）*/
}

static void on_run_reset(ce_card_t *c)
{
    c2x_state_t *s = ce_state(c, c2x_state_t);
    if (s->batch_count || s->fragment_cycle)
        ce_log("collect2x: run reset (open batch of %d, fragment cycle %d)", s->batch_count, s->fragment_cycle);
    if (s_virtual_total)                                /* ★ §1x.2：本 run 一共收了多少自建实体成员 */
        ce_log("collect2x: run reset (virtual green-dot members this run: %d)", s_virtual_total);
    memset(s, 0, sizeof *s);                            /* 4:1 循环游标也归零：循环按 run 计 */
    s_stash_valid = 0;
    s_batch_virtual = 0; s_batch_vhold = 0; s_virtual_total = 0;
    s_play_frame = 0;                                   /* run 游玩帧计数同归零 */
}

CE_CARD(114, .on_item_frame = on_item_frame, .on_item_collect = on_item_collect,
              .on_item_commit = on_item_commit, .on_shot_damage = on_shot_damage,
              .on_stage_start = on_stage_start, .on_run_reset = on_run_reset);
