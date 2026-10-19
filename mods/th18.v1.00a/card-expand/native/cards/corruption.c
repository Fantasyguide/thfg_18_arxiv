/* 腐化（id 70）—— 放炸弹不再消耗炸弹数，而是消耗**上限**。
 *
 * 拿到卡时上限顶满（原生上限 7），此后每放一发上限 −1；当前数跟着上限走，
 * 所以 HUD 上那个数字**就是剩余预算**：一次给满七发，用一发少一发，过关也不回复。
 *
 * 【为什么挂断点而不是 on_tick_2】
 * consume_bomb `0x4574d0` 全库只有 `do_bomb` `0x4203b7` 一个调用方，所以断点挂在它**刚返回**
 * 的那一条（`0x4203bc`，`a1 c0 f2 4c 00`，5 字节绝对寻址、无相对量）就不多不少地覆盖每一次
 * 炸弹消耗，而且**同一帧**改完 —— 用 on_tick_2 的边沿要等到下一帧，中间会闪一格。
 *
 * 【为什么要接管每关开场】
 * `GameThread__thread_start` 在 `0x44274a`–`0x44278f` 把当前数刷成 `min(3, 上限)`，
 * 会把预算显示压回 3。`on_stage_start`（`+0x34`，同一函数 `0x442F98` 广播、在那之后）
 * 把当前数拉回 = 上限。**它不加东西，只是不让引擎的补给把预算显示改小。**
 *
 * 【删卡】它改的是全局上限（一局内不重置）⇒ on_delete 里必须把上限还原，见文件末的 on_delete 注释。
 *
 * 【撤销扣除为什么是精确的】
 * 进 do_bomb 前 can_bomb 已保证 `CURRENT >= 1`，且不变式 `CURRENT <= MAX` 成立，
 * 所以 consume 里的两次钳位（`< 0 → 0`、`min(cur,max)`）都不会触发 —— 扣完必然正好是
 * `CURRENT-1`，`+= 1` 就是精确还原。
 *
 * ============================================================================================
 * ★★ 2026-10-19 修掉的真 bug：带本卡时**每一次死亡都免费**（残机永不减）
 *
 * 因果链（一手反汇编，th18 v1.00a，地址全核对过）：
 *   ① 原生死秽回避（`AUTOBOMB` = 原生卡 id 23）走 `Card__death_save_bomb_revive` `0x40a2a0`
 *      （唯一调用方 `0x40a534`，前置 `Bomb__can_bomb_and_deathbomb_check` `0x420420`，
 *      后者**只要求 `CURRENT_BOMBS > 0`** ⇒ 只剩 1 发也放行）。它在 `0x40a3ed` 与 `0x40a42c`
 *      各做一次「`CURRENT_BOMBS 0x4ccd58` − 1 → 钳 0 → 再钳 `MAX_BOMBS 0x4ccd64`」，
 *      ⇒ 一次死亡回避花掉 **1 或 2 发当前数**，**只写 CURRENT、从不写 MAX**
 *      （全函数对 MAX 只有读：`0x40a3d7` / `0x40a40c`）。
 *   ② 它**不经过** `consume_bomb 0x4574d0` ⇒ 本卡挂在 `0x4203bc` 的 `on_bomb_spent` **抓不到它**。
 *   ③ 本卡 `on_tick_2` 每帧把 CURRENT 钉回 MAX ⇒ 那 1–2 发下一帧被"补回" ⇒ MAX 一分不掉
 *      ⇒ **每次死亡免费** ⇒ 残机永不减。**这就是 2026-10-19 这个 bug 的根。**
 *
 * 修法（只在本卡内闭环，不动共享件；纯逻辑在 `corruption_core.{h,c}`）：
 *   · 新增 `on_death_before_deathbomb`（虚表 **`+0x10`**）：`Player__die` `0x45d3a0` 末尾
 *     `0x45d574` 对链表每张卡各 `call [vt+0x10]` 一次、**返回值被忽略**（所以这里只记账、
 *     不"救场"）；此时 `0x45d557` 刚把决死计时 `player+0x47908` 设成 8 帧、**还没扣命**
 *     ⇒ 正好在这里拍残机快照 + 开 24 帧窗口。
 *   · `on_tick_2` 改成**先结算、再钉不变式**：窗口内 `MAX − CURRENT ∈ {1,2}` ⇒ 那就是死亡回避
 *     漏记的支出 ⇒ `MAX −= spent; CURRENT = MAX`，并**自己刷 HUD**（结算后 CURRENT == MAX，
 *     原来那条"不相等才刷"的判据不会触发）。
 *   · 真死（`0x45d1a0` 扣命、`0x45d1e8` 把 CURRENT 重设成
 *     `clamp(每条命炸弹数 0x4ccd60 = 3, 0, MAX)`）⇒ 残机快照变小
 *     ⇒ **不结算**、关窗。
 *   · 按 B 自救 / 六文钱 / 113 救场 ⇒ CURRENT 仍等于 MAX ⇒ `spent == 0` ⇒ 自动不重复记（不加额外判据）。
 * ============================================================================================
 */
#include "sdk.h"
#include "corruption_core.h"

#define CORRUPTION_MAX_BOMBS 7          /* 原生上限（0x4428ed 等三处 mov [0x4ccd64],7）*/

static void sync_hud(void) { ce_gui_update_bombs(); }

/* 中弹窗口：`+0x10` 拍快照（corruption_death_window_open）、`on_tick_2` 消费。
 * 纯逻辑与一手对位见 `corruption_core.h`；为什么需要它见文件头 2026-10-19 那一段。*/
static corruption_death_window_t s_death_window;

static int ctor(ce_card_t *c)
{
    if (!ce_fresh_acquire(c)) return 0;             /* 每关开场引擎会再调一次 ctor，那次不算新获得 */
    CE_MAX_BOMBS() = CORRUPTION_MAX_BOMBS;
    CE_CURRENT_BOMBS() = CE_MAX_BOMBS();
    sync_hud();
    ce_log("corruption: acquired — bombs %d/%d (from now on a bomb spends the MAX, not the count)",
           CE_CURRENT_BOMBS(), CE_MAX_BOMBS());
    return 0;
}

static void on_bomb_spent(ce_card_t *c)
{
    (void)c;
    CE_CURRENT_BOMBS() += 1;                        /* 精确还原刚才那一下 */
    if (CE_MAX_BOMBS() > 0) CE_MAX_BOMBS() -= 1;    /* 改成扣上限，下限 0 */
    if (CE_CURRENT_BOMBS() > CE_MAX_BOMBS()) CE_CURRENT_BOMBS() = CE_MAX_BOMBS();
    sync_hud();
    ce_log("corruption: bomb spent the cap — bombs %d/%d left",
           CE_CURRENT_BOMBS(), CE_MAX_BOMBS());
}

/* 每关开场：不让引擎的 min(3, 上限) 补给把预算显示压小 */
static void on_stage_start(ce_card_t *c)
{
    (void)c;
    if (CE_CURRENT_BOMBS() != CE_MAX_BOMBS()) {
        CE_CURRENT_BOMBS() = CE_MAX_BOMBS();
        sync_hud();
    }
}

/* ---- 中弹瞬间（虚表 `+0x10`，`Player__die` 末尾 `0x45d574`）----
 * 这里只**记账**，不"救场"（返回值本来就被引擎忽略）：
 *   拍下中弹那一刻的残机 + 开一个 24 帧的窗口 ⇒ 给下面 `on_tick_2` 判断「这一下是死亡回避
 *   花的 1–2 发、还是真死（残机 −1）」用。见文件头 2026-10-19 那一段。*/
static int on_death_before_deathbomb(ce_card_t *c)
{
    (void)c;
    corruption_death_window_open(&s_death_window, CE_CURRENT_LIVES(),
                                 CORRUPTION_DEATH_WINDOW_FRAMES);
    return 0;
}

/* 每帧：**先结算、再钉不变式**。
 * ⚠ 2026-10-19 订正：这里原来写「`0x40a3d0` 一带的**碎片进位**」——**错**。`0x40a3d0` 是
 * 死亡回避 `0x40a2a0` 的**第 1 次扣炸弹**（`a1 58 cd 4c 00` = `mov eax,[0x4ccd58]` …
 * `0x40a3ed mov [0x4ccd58],edx`），第 2 次在 `0x40a418`–`0x40a42c`；碎片是另一个全局
 * `0x4ccd5c`（复活那条 `0x45d1aa` 读它刷 HUD），跟这里无关。
 * 真正会打破不变式「CURRENT == MAX = 剩余预算」的：① 死亡回避扣 CURRENT（`0x40a3ed` / `0x40a42c`，
 * 由上面的窗口结算记账）；② 每关 `CURRENT = min(3, MAX)` 补给（`0x44278f`，由 on_stage_start 抵消）。
 * ⇒ 每帧仍然要把不变式拉回来，但**必须排在结算之后**。*/
static int on_tick_2(ce_card_t *c)
{
    corruption_budget_t b;
    int32_t spent = 0;

    (void)c;
    /* ① 结算（窗口内、残机没变小）：把死亡回避漏记的 1–2 发记进预算 */
    b.max_bombs = CE_MAX_BOMBS();
    b.current_bombs = CE_CURRENT_BOMBS();
    b.lives = CE_CURRENT_LIVES();
    if (corruption_death_window_tick(&b, &s_death_window, &spent) == CORRUPTION_SETTLED) {
        CE_MAX_BOMBS() = b.max_bombs;
        CE_CURRENT_BOMBS() = b.current_bombs;
        /* ★ 必须自己刷 HUD：结算后 CURRENT == MAX，下面那条"不相等才刷"的判据不会触发。*/
        sync_hud();
        ce_log("corruption: death save cost %d of the cap — bombs %d/%d left",
               spent, CE_CURRENT_BOMBS(), CE_MAX_BOMBS());
    }

    /* ② 再钉不变式（这张卡的模型就是 CURRENT 恒等于 MAX）*/
    if (CE_CURRENT_BOMBS() != CE_MAX_BOMBS()) {
        CE_CURRENT_BOMBS() = CE_MAX_BOMBS();
        sync_hud();
    }
    return 0;
}

/* ★ 2026-09-12（删卡收尾审计，铁律 METHOD.md「长持续效果的卡必须交代『被删掉会怎样』」）：**本卡留下的
 * 全局旋钮必须在 on_delete（虚表 +0x50）里还原**。它是那七步清单里的第 7 类（全局旋钮）唯一漏网的一张 ——
 * 既不每帧重写、也没走 SDK 的按卡聚合，删卡路径上就没人收。
 *
 * 【为什么删卡路径会漏】`MAX_BOMBS`（`0x4ccd64`）是**一局内持续累积**的量：三处 `mov [0x4ccd64],7`
 * （`0x4428ed` / `0x442920` / `0x442957`，AUDIT §T8）只在**开局初始化**跑，每关开场 `0x442745`–`0x44278f`
 * 写的是 `CURRENT = min(3, MAX)`、**不碰 MAX**。而本卡把「放一发炸弹」记成 `MAX −= 1`。
 * 倒吊人（74）/ 原版空白卡那次删卡都发生在**商店里**，那一刻 `AbilityManager__on_tick` 被
 * `ABILITY_SHOP_PTR` 冻住 ⇒ `on_bomb_spent` / `on_tick_2` 当场停摆，`on_run_reset` 要等局末，
 * **只有这里跑得到**。不收的后果：扣掉的那几格上限**再也没人还**，整局剩下的关卡上限一直偏低
 * （例：用掉 3 发后删卡 ⇒ 后面每关都是 `3/4` 而不是 `3/7`），HUD 也一直是错的。
 *
 * 【为什么写回 7 就是精确撤销】本卡 ctor 在**取得那一刻**把 MAX 写成 7（`CORRUPTION_MAX_BOMBS`），
 * 此后同一局内只有本卡会减它 —— 引擎侧只有 `CardBomb__destructor` `0x409C20` 的 +1 与那三处开局 =7，
 * 皇家同花顺（58–62）的 +1/+2 也只是往上加且被 `< 7` 钳住 ⇒ MAX 恒 ≤ 7，写回 7 不会超过任何别的卡
 * 给过的上限，也不需要额外记账。**当前数不动**：那是玩家已经用掉的预算，不退还；删卡时不变式
 * `CURRENT == MAX` 仍成立（商店里没人动炸弹），下一个关卡开场引擎自己会把它压到 `min(3, 7)`。
 * `on_run_reset` 不补：局末之后引擎在每局开头的初始化里自己把 MAX 写回 7，这里再写一次没有意义。*/
static void on_delete(ce_card_t *c)
{
    (void)c;
    if (CE_MAX_BOMBS() == CORRUPTION_MAX_BOMBS) return;
    ce_log("corruption: dismissed (card destroyed) — bomb cap %d -> %d (engine default), count %d kept",
           CE_MAX_BOMBS(), CORRUPTION_MAX_BOMBS, CE_CURRENT_BOMBS());
    CE_MAX_BOMBS() = CORRUPTION_MAX_BOMBS;
    if (CE_CURRENT_BOMBS() > CE_MAX_BOMBS()) CE_CURRENT_BOMBS() = CE_MAX_BOMBS();
    sync_hud();
}

CE_CARD(70, .ctor = ctor, .on_bomb_spent = on_bomb_spent, .on_stage_start = on_stage_start,
            .on_death_before_deathbomb = on_death_before_deathbomb,
            .on_tick_2 = on_tick_2, .on_delete = on_delete);
