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
 *       ② **presence**：`ce_vitem_flying() > 0` 与原生谓词**或**起来 ⇒ 续 idle
 *          —— 修「我们这些点还在飞的时候就提前结算」。
 *     ---- ★★ 2026-09-18（B1）：**两条通道的「算不算」由生产者按引擎同一谓词给出** ——
 *     `faith.c` 只报**真空飞行中被收**（本池 `state == 3` = 过回收线 / 放 B / 对话 ⇒ 朝自机飞）
 *     与**真空飞行中还在飞**的那些，飘落 / 飞散 / 70px 环吸**都不报**（`faith_core.h` 的对照表）。
 *     这正是用户 2026-09-18 的规格：**信仰点必须与 P 点 / 金钱完全一致**（开批 / 计数 +1 /
 *     飞行中组织批次结算）。⇒ 本文件这一侧**一个字没改**（它本来就只认 state 3），
 *     变的是生产者报上来的真值；旧实现「活着的绿点都算在飞 / 任何状态被收都算成员」会造成
 *     普通游玩顺手捡点就开批、绿点飘着就把批吊住 ⇒ 实跑症状「绿点一直触发收点」。
 *     **批的档位 / 倍率 / 碎片 / 批分算法一个字没改**；微档（消弹那档的半道具）不入批（§1x.3），
 *     由 SDK 层的 `CE_VITEM_HALF` 挡在生产者一侧。诊断三行日志见 `on_item_frame`。
 *   · ★ 2026-09-18（共享 PIV 池落地后的涨源接线）：**满 P 道具（TH18 item type 8）被收 ⇒
 *     往共享 PIV 池写 +100 显示**（TH14 一手 `th14:0x439220` 的已满支、`th14:0x43923b` `add edx,0x2710`；
 *     出处 `engine/faith/th14/01-point-item-value.md`）。落点 = 现成站点 `ce_item_collect`
 *     （`on_item_collect` 开头，本卡本来就订阅它）⇒ **不新增断点 / 不改字节**。
 *     ⚠ **绿点那一档（TH14 `th14:0x439d30`，+2 / +20 显示）本卡够不着 ⇒ 本轮没接**：
 *     绿点是共用件（`cards/faith.c` / `faith_core.h`，110/111/107 共用）的自建实体，收点与档位都在
 *     `faith.c:dot_collect()` 里；114 只经「虚拟收集」层拿到**颗数**（B11 契约：不报值、不带档位）
 *     ⇒ 那一档要挂在**共用件**的收点处（改 `faith.c` = 要 AUDIT §BE 登记），逐条一手见 `on_item_collect`
 *     上方那段长注。
 *   · ★★ 2026-09-21（P9 / `WORK-PACKAGES.md` §0 的 114 #1，用户：「我记得原版应该是**道具从 bonus
 *     位置生成**，而不是自机位置」）：**碎片道具的生成点 = 结算那一刻的「批位」**（TH14 一手：批位
 *     float3 在成员加入批时被写成自机坐标，结算时**弹字与 spawn 读同一份栈上坐标**，见 core.h 的
 *     `C2X_BONUS_ANCHOR_Y_OFFSET` 注 + `engine/faith/th14/01-point-item-value.md` §6(d)）。
 *     改动三件：① 新增 `note_bonus_anchor()`（成员门内 / 虚拟收集通道内各一处写点）；
 *     ② 碎片弹点从「弹片帧的**当下**自机位 − 76 px」（§Z14 的可见性权宜值）改成
 *     **结算帧快照下来的批位 − 48**；③ 初速 1.2f（自定）→ **2.2f**（原版 settlement 实参）。
 *     登记 = AUDIT §Z23；验收 = NEXT.md §1s.2。
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

/* ================= ★★ bonus 锚点（批位）：2026-09-21（P9 / 114 #1）=================
 *
 * **一手（TH14 v1.00b，`local/_scratch/th14_full.asm`）**：批位 = 全局 float3
 * `th14:0x4f5888`（x/y）+ `th14:0x4f5890`（z），在**成员加入批那一刻**被写成**自机坐标** ——
 * 三个 P / 钱 handler 内联同形（`th14:0x4391f5`/`th14:0x4391fd`/`th14:0x439205`/`th14:0x43920b`，
 * 另两处 `th14:0x4393e7`+`th14:0x4393f5` / `th14:0x4395e9`+`th14:0x4395f7`）：
 *     `movq xmm0,[ebx+0x5e0]` / `movq [0x4f5888],xmm0` / `mov eax,[ebx+0x5e8]` / `mov [0x4f5890],eax`
 * 其中 `ebx = [0x4db67c]` = 自机对象、`+0x5e0/+0x5e4/+0x5e8` = 自机 float3（同一 handler 里判
 * 「线上/线下」用的就是 `[ebx+0x5e4]` 对线 128/148：`th14:0x4390d3` 取、`th14:0x4391da` 比）。
 * ⇒ 批位语义 = **最后一次成员加入时的自机坐标**（最后写者胜）；结算时它 −48 同时给弹字与道具。
 * 我们这一侧因此不需要新断点：本卡本来就在 `ce_item_collect`（成员门）与「虚拟收集」通道上，
 * 两处都是「成员加入」事件 ⇒ 就地读自机坐标（同一帧、同一个值）。
 *
 * **与 B10 的关系**：B10（2026-09-18）发现弹字漏了 `y − 48`，当时写的是「我们直接读自机坐标 ✅
 * 等价」。等价只在「最后一次成员加入 = 结算帧」时成立；批被 presence 续命（§Z21）时两者能差出
 * 自机在 idle 窗里的位移 ⇒ 现在**真的存批位**，弹字 / 声像 / 碎片三个消费者共用它。*/
static void note_bonus_anchor(c2x_state_t *s)
{
    uint8_t *p = CE_PLAYER();
    if (!p) return;                     /* 取不到自机（异常）：保留上一个锚，`anchor_valid` 兜底 */
    s->anchor_x = *(float *)(p + CE_PLAYER_X);
    s->anchor_y = *(float *)(p + CE_PLAYER_Y);
    s->anchor_valid = 1;
}

/* 取 bonus 锚点。批开 ⇒ 成员门内必写过（`anchor_valid == 1`）；没写过只可能是异常路径 ⇒
 * 退回当下自机位并留一行日志（**不是**静默改语义）。弹字 / 声像 / 碎片快照共用这一个值。*/
static void bonus_anchor_pos(const c2x_state_t *s, float *x, float *y)
{
    if (s->anchor_valid) { *x = s->anchor_x; *y = s->anchor_y; return; }
    uint8_t *p = CE_PLAYER();
    *x = p ? *(float *)(p + CE_PLAYER_X) : 0.0f;
    *y = p ? *(float *)(p + CE_PLAYER_Y) : 0.0f;
    ce_log("collect2x: bonus anchor missing (batch open without a member write) — fall back to player pos");
}

static void spawn_item_at(uint32_t type, float x, float y, float speed)
{
    float p3[3] = { x, y, 0.0f };
    if (!ce_spawn_item(type, p3, -1.5707964f, speed))   /* 角度 −π/2（向上）；速率按场景：P/钱 与碎片卡 = 2.2 原生 main-drop 实参 */
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
    /* ★★ 2026-09-21（P9 / 114 #1）**弹点 = 结算那一刻的 bonus 锚点**（`pending_frag_*` 是 settle
     * 登记 pending 时对批位做的快照，见 `on_item_frame`）—— 不再取「弹片这一刻的当下自机位」：
     *   · **原版同形**：TH14 结算尾 `th14:0x438fbc lea eax,[esp+0x28]`（栈上那份**批位 y − 48**）
     *     → `th14:0x438fda call 0x4397b0`（spawn，`th14:0x4397b0` 头 `movq xmm0,[eax]` 写 item+0xbac）
     *     —— 与弹字 `call 0x458000` 是**同一个指针**；同形另三处 `th14:0x438ffc`/`0x439028`/`0x439047`。
     *   · **角度 / 速度也是原版实参**：四处的 `push 0x28`(arg6) + `sub esp,0xc` + `[esp+8] = 0x400ccccd`
     *     （= **2.2f**）+ `[esp+4] = 0xbfc90fdb`（= **−π/2**）⇒ 与 main-drop 包装
     *     `th14:0x4381f0` 的 `0x4381ff`/`0x438207` 逐字节相同（本仓 §Z17 早已按这两条实参在掉 P 上用过）。
     *     旧值 **1.2f 是 ❓自定**（2026-09-08 的「短弧」体感值）⇒ 本轮回到一手。
     *   · **y 不夹**：原版 spawn `th14:0x4397b0` 只钳 **x** 到 ±192（`th14:0x43981b` 对 `th14:0x4c1c9c`
     *     = −192.0f、`th14:0x43982c` 对 `th14:0x4c1b74` = 192.0f），y 一字不动；TH18 对位
     *     `th18:0x446f40` 同样只钳 x（`th18:0x446fcc`/`th18:0x446fdd`，`th18:0x4b946c`/`th18:0x4b933c`）。
     *     ⇒ 旧实现的「贴顶保底 40」（`py − 76 >= 40 ? … : 40`）是自定，本轮去掉（锚点 −48 一般落
     *     在 POC 线附近：线上收 ⇒ 批位 y ≈ 线高 128/224 ⇒ 弹点在 80/176）。*/
    const float ax = s->pending_frag_x;
    const float sy = s->pending_frag_y - C2X_BONUS_ANCHOR_Y_OFFSET;
    /* 横向散开（±12 px）只在 n > 1 时才有量 —— core 目前恒 n = 1（`out.fragment_count = 1`），
     * 原版也是一次结算**一颗**（`th14:0x438fda` 单调用）⇒ 这一项对 1 颗恒等于锚点自己。*/
    for (int i = 0; i < n; ++i)
        spawn_item_at((uint32_t)s->pending_frag_type,
                      ax + ((float)i - (n - 1) * 0.5f) * 12.0f, sy, 2.2f);
    ce_log("collect2x: frag card type 0x%x x%d spawn @(%d,%d) bonus(%d,%d) t=%d", s->pending_frag_type, n,
           (int)ax, (int)sy, (int)s->pending_frag_x, (int)s->pending_frag_y, s_play_frame);
    s->pending_frag_type = 0;
    s->pending_frag_count = 0;
}

static void on_item_frame(ce_card_t *c)
{
    /* ★ 2026-09-18（用户口径）：向**共享 PIV 池**声明本卡在场 + 本卡的可写上限（`cap_i − init_i`，按难度查表）。
     *   一行、幂等（每帧重申，难度一变立刻跟上）；退场由 `ce_layer_drop_card()` 兜底。
     *   擦弹那**一项**是**共用写入者**（用户口径：别加一张卡擦弹数的作用就 +1 次）⇒ 这里只声明「我在这一族」。
     *   口径与表的一手出处：`engine/faith/OVERVIEW.md`「结论：th12–th17 可以共用一个 PIV」+ `sdk_core.h` 同名注释块。*/
    ce_piv_claim_auto(ce_card_id(c), ce_difficulty());
    ce_graze_family_claim(ce_card_id(c));

    c2x_state_t *s = ce_state(c, c2x_state_t);
    ++s_play_frame;                                     /* 游玩帧计数（帧源注释：分派 = 游玩帧）*/
    const int32_t now = s_play_frame;
    if (s->pending_frag_count && now >= s->pending_frag_at)
        spawn_pending_frags(c, s);                      /* 延迟到点：从**结算帧快照的 bonus 锚点**弹卡（批闭与否无关）*/

    /* ★★ 2026-09-15（`NEXT.md` §1x.2 **成员通道**）：自建实体（110/111 的大 / 中绿点）被收 ⇒ 入批计一员。
     * 来源 = SDK 的「虚拟收集」声明层（`sdk_core.h`）：`cards/faith.c` 在它**自己的收集判定**
     * 命中的那一刻报一颗（`ce_vitem_note_collect`），这里抽干、按颗数计入本批。
     *   · **在 `batch_open` 门之前**：第一颗绿点自己就能开批（§Z22 的成员门只针对**引擎原生**道具的
     *     真空谓词 —— 自建实体没有 type / state，「算不算收点」由生产者的收集判定给出）；
     *   · **不进批分**：`c2x_collect_virtual()` 只动 count、不碰 batch_score ⇒ 结算补差对纯绿点批
     *     恒 0（绿点走信仰槽，不进 SCORE —— 用户口径「只 +1」）；
     *   · 微档不进（§1x.3 的半道具）：由 SDK 层的 `CE_VITEM_HALF` 挡在生产者那一侧。
     *   · ★★ 2026-09-18（B1）：**生产者那边只报「真空飞行中被收」的**（`faith.c` 的
     *     `dot_collect()` 过 `faith_core_state_is_vacuum_flight()`）⇒ 本卡收到的真值与原生成员门
     *     同一个谓词，用户规格「与 P 点 / 金钱完全一致」在**接线两端**都成立。
     * 顺序：与卡 tick（生产者）同帧谁先谁后都读得到（层带 2 帧容差，见 `sdk_core.h` 契约 ④）。*/
    {
        const int vn = ce_vitem_take_collect();
        if (vn > 0) {
            c2x_collect_virtual(s, vn, now);
            /* ★ 2026-09-21（P9 / 114 #1）：自建实体入批**也写批位** —— 原版没有对应物（DDC 的绿点
             * 根本不进批），但用户 2026-09-18 的 B1 口径是「信仰点与 P 点 / 金钱**完全一致**」
             * （开批 / 计数 +1 / 飞行中组织结算）⇒ 成员加入的**写点也照同一条**（❓自定，登记的
             * 是「这条通道跟原生同形」这件事本身）。⚠ 容差：层的声明带 2 帧窗（`sdk_core.h` 契约 ④）
             * ⇒ 这里读到的自机位可能比绿点**被收那一刻**晚 ≤2 帧（TH14 那三处是当场写）。*/
            note_bonus_anchor(s);
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
     *   （计数走上面的成员通道）⇒ 与原生谓词是**或**关系，不改变原生那一支的语义。
     *   ★★ 2026-09-18（B1）：生产者报的「在飞」也**收窄到真空飞行**（`faith.c` 只数 `state == 3`）
     *   ⇒ 这一路与原生谓词（`ce_item_pool_any_attracting`：state 3）逐字同一条，用户规格
     *   「与 P 点 / 金钱完全一致」成立；飘落 / 飞散中的绿点**不再把批吊住**（那正是实跑症状
     *   「绿点一直触发收点」的一半）。*/
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

    /* ★★ 2026-09-18（B10）+ ★ 2026-09-21（P9 / 114 #1）：**弹字锚点 = (批位 x, 批位 y − 48)** ——
     *   TH14 原版逐指令：批位 float3 在**成员加入批那一刻**被写成**自机坐标**（三个 P/钱 handler 内联：
     *   `movq xmm0,[reg+0x5e0]` + `movq [0x4f5888],xmm0`），结算时 `subss xmm0,[0x4c1b24]`
     *   （= 48.0f）再进弹字；绘制侧两作同形同常量 ⇒ 原版两行落在**自机上方**（y−32 / y−37）。
     *   B10 时我们只写自机坐标、漏了 −48 ⇒ 弹字整体低 48 px（用户报「比正常的靠下了」）；
     *   本轮再把「取**当下**自机位」换成**真的批位**（`note_bonus_anchor` 的存值 —— 原版的批位就是
     *   最后一次成员加入时的自机位），于是弹字 / 声像 / 碎片道具三者共用**同一个** bonus 锚点，
     *   与原版读同一份 `[esp+0x28]` 同形。完整出处（含批位写点与绘制常量逐个对照）见
     *   `collect2x_core.h` 的 `C2X_POPUP_Y_OFFSET` / `C2X_BONUS_ANCHOR_Y_OFFSET` 长注。*/
    float ax, ay;
    bonus_anchor_pos(s, &ax, &ay);
    float pz[3] = { ax, ay - C2X_POPUP_Y_OFFSET, 0.0f };

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
     * C2X_FRAG_SPAWN_DELAY_FRAMES 帧后在 on_item_frame 从**结算帧快照的 bonus 锚点** spawn
     * （★ 2026-09-21 P9/114 #1：原版是结算帧**当场** spawn，延迟是本 mod 的 ❓自定节奏 —— 见下）。
     * 0x10/0x11 不进
     * 池空判定（批集 {1,2}，§Z19），延迟在飞不挡批闭。连续两次结算间隔 < 延迟窗口 → 后者覆盖前者（日志见）。 */
    if (out.fragment_count) {
        if (s->pending_frag_count)
            ce_log("collect2x: frag pending overwritten (new settle before spawn window)");
        s->pending_frag_type = out.fragment_type;
        s->pending_frag_count = out.fragment_count;
        s->pending_frag_at = now + C2X_FRAG_SPAWN_DELAY_FRAMES;
        /* ★ 2026-09-21（P9 / 114 #1）：**把批位快照进 pending** —— 碎片是这之后 20 帧才弹的，
         * 这窗口里若又开新批（成员加入会改写 `anchor_*`），弹点必须是**本批**的 bonus 位置。
         * 原版没有这个问题（结算帧当场 spawn），是我们的延迟引入的 ⇒ 快照是这一侧的必需品。*/
        s->pending_frag_x = ax;
        s->pending_frag_y = ay;
    }
    ce_log("collect2x: settle %d items, batch score %lld -> x%d.%03d, +%lld score, frag type 0x%x x%d (t=%d)",
           count, (long long)out.batch_score, out.mult_permille / 1000, out.mult_permille % 1000,
           (long long)delta, out.fragment_type, (int)out.fragment_count, now);
    /* 倍率弹字（原生环；纯视觉）。值 = 结算分（≥0 → draw 走 "BONUS" 分支）；
     * 位置 = 同一个 bonus 锚点（原版弹字与碎片读同一份栈坐标）。 */
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

/* ================= ★ 满 P 涨源：往**共享 PIV 池**写（TH14 一手 + TH18 接缝）=================
 *
 * TH14 往最大得点（PIV）里加的道具侧涨源有**两档**（一手 `engine/faith/th14/01-point-item-value.md`
 * 的「按 mod 的额外机制口径重述」表 + §4「涨/跌入口穷举」——全 exe 只有四条写点，三条是涨）：
 *
 *   · **绿点 type 9/10/11**：`th14:0x439d30`（`PIV += (int)(farg × 100.0f)`，常量 `th14:0x4c1b54` = 100.0f；
 *     **唯一调用点** `th14:0x438ba7`，两支实参 = `th14:0x4c1a78` = 2.0f ⇒ **+2 显示** /
 *     `th14:0x4c1afc` = 20.0f ⇒ **+20 显示**）。
 *     ⚠ **本卡够不着（本轮没接）**：本 mod 的绿点是**共用件的自建实体池**（`cards/faith.c` /
 *       `cards/faith_core.h`，110 / 111 / 107 共用一套），**收点判定与档位（大/中/小）都在
 *       `faith.c` 的 `dot_collect()` 里**（那里同时是 `ce_vitem_note_collect()` 的报点处）。
 *       114 这一侧只经 SDK 的「虚拟收集」层拿**颗数**（`ce_vitem_take_collect()`，`sdk_core.h` 的
 *       「虚拟收集 / 虚拟在飞」块），而按那里的契约 ★★B11 **只报「几颗」、不报值、也不带档位**
 *       ⇒ 想按「每颗 +2 / +20」写池，只能在**共用件**的收点处挂（= 改 `faith.c`，要 AUDIT §BE 登记），
 *       **本轮没改**（本任务口径：够不着就只接满 P）。
 *
 *   · **满 P 道具（item type 8）= 下面接的这一档**：`th14:0x439220` 就是 type 8 的 handler
 *     （分派一手 `th14:0x438b3e` 跳表 `th14:0x439088` 第 8 项 → `th14:0x438b51` `call 0x439220`），
 *     `th14:0x439231` `cmp 当前火力, 火力上限` ⇒ **未满**走「补满火力」那一支（`th14:0x439282`）；
 *     **已满**这一支才 `th14:0x439235` `edx = PIV` → `th14:0x43923b` `add edx,0x2710`
 *     （+10000 存储 = **+100 显示**；存储 = 显示 ×100，§2 量纲）→ `th14:0x43925c` 写回（现场夹上限）。
 *
 * TH18 的**对位事件 = 收 type 8 道具（补满火力）**：一手 `th18:0x44625f` 的 type 分派（字节表
 * `th18:0x44685c` + 跳表 `th18:0x446830`）把 type 8 交给 `th18:0x44627f` —— 它 `push [0x4ccd3c]`
 * （MAX_POWER）调 `th18:0x4573f0`（= `add_power`；`th18:0x457401 cmp 当前火力,上限` + `jl` ⇒
 * **火力已满时直接返回 0**）⇒ `th18:0x446291 je 0x446584` **当帧静默消失**。
 * 即：TH18 原版在「已满」这一支**什么都不给** —— 因为 **TH18 根本没有 PIV 全局**
 * （一手 `engine/faith/th18/01-point-item-value.md` §0：身价是 MONEY 的派生量，不是独立累加器）
 * ⇒ 我们在这一支补上 TH14 的那 **+100 显示**，写进跨作共享的那块池。
 * 满 P 判据照 **TH18 引擎自己**的那一条（`th18:0x446981` `cmp eax,[0x4ccd3c]` 的同款谓词）；
 * `engine.h` 只给了 `CE_CURRENT_POWER()`，max power 的读取宏**就地补一个**（同址 `0x4ccd3c` =
 * `GlobalsInner+0x60`；与 `cards/mof.c` 的 `CE_MAX_POWER_VAL()` / `cards/lolk.c` 的同名宏同源，**只读**）。
 *
 * **接缝 = 现成的站点 `ce_item_collect`（`th18:0x44625f`）** —— 本卡本来就在订阅它（批的成员门），
 * 它落在**本颗道具的副作用之前**（`sites.py` 原话：type 分派与分值都还没发生）⇒ 这里读到的火力
 * 正是「收这颗之前」的值，与 TH14 在 handler 里判的是**同一个时点**（`th14:0x439231` 也读在补火力之前）。
 * 一颗道具分发一次 ⇒ **一次事件加一笔**；同一帧多颗就多笔（不合并、不漏）。
 * ⚠ 本卡不新增断点 / 不改字节：新增的只是这里的一个回调分支（虚表注册见文件末 `CE_CARD`）。*/
#define C2X_ITEM_TYPE18_FULL_POWER  8u    /* TH18 item type 8 = 补满火力（一手 th18:0x44627f）*/
#define C2X_PIV_FULLP_ITEM          100   /* TH14 满 P 时收一颗 type 8 = +100 显示（一手 th14:0x43923b）*/
#define C2X_MAX_POWER()  (*(volatile int32_t *)CE_ADDR_MAX_POWER)   /* 0x4ccd3c = GlobalsInner+0x60 */

static void on_item_collect(ce_card_t *c, uint32_t type, const void *item)
{
    /* ★ 满 P 涨源（共享 PIV 池）——逐条一手见函数上方那一段长注。 */
    if (type == C2X_ITEM_TYPE18_FULL_POWER) {
        const int32_t mx = C2X_MAX_POWER();
        /* ⚠ `mx > 0` 是**防御**不是语义：`max` 还是 0（引擎尚未初始化火力）时 `cur >= max` 恒真，
         *   会白写一笔（`faith.c` 的满 P 门同款守卫）。真值只可能在游玩帧到达 ⇒ 可达状态里恒等。*/
        if (mx > 0 && CE_CURRENT_POWER() >= mx) {
            /* ★ TH14 路径往**共享 PIV 池**写：**+100 显示**
             *   （一手 `th14:0x43923b` `add edx,0x2710` = +10000 存储；出处 =
             *   `engine/faith/th14/01-point-item-value.md` 的涨源表 + §4 写点穷举）；
             *   池的权威口径 = `sdk_core.h` 的「★ 共享 PIV 池 + 擦弹族」块（在场声明见 `on_item_frame`）。
             *   ⚠ **闸由层管，本卡不夹** —— 可写上限 = `cap_i − init_i`（本卡按 th14 难度两表，
             *   `ce_piv_claim_auto()` 每帧在 `on_item_frame` 里重申），`ce_piv_add` 自己按已写账截断。*/
            ce_piv_add(ce_card_id(c), C2X_PIV_FULLP_ITEM);
            ce_log("collect2x: full-P item -> PIV +%d (pool %d, wrote %d, t=%d)",
                   C2X_PIV_FULLP_ITEM, ce_piv_pool(), ce_piv_written(ce_card_id(c)), s_play_frame);
        }
        return;                                         /* type 8 不是批集 {1,2}：不入批、不进批分 */
    }

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
    /* ★ 2026-09-21（P9 / 114 #1）：**批位 = 成员加入那一刻的自机坐标** —— TH14 三个 P/钱 handler
     * 在「加入批」三件事（count++ / 批分累加 / 写批位）里内联的就是这个值（逐指令见
     * `note_bonus_anchor` 上方）：本断点落在 type 分派**之前**、与收集同帧 ⇒ 与 `[reg+0x5e0]`
     * 同一帧同一个值。结算时的弹字 / 声像 / 碎片生成点都从这里取。*/
    note_bonus_anchor(s);
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
    s->anchor_valid = 0;                                /* ★ P9/114 #1：批位随关作废（新关成员加入时重写）*/
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
