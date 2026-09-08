/* 股市（id 80）—— 主动卡：选中它时 C = 买入 50 金钱，Shift + C = 卖出 50 金钱等值。
 * 股价由占位随机过程驱动（stock_core.c，每 15 帧一步、4 秒一根 K 线），HUD 右下常驻面板：16 根 K 线 + 三行读数。
 * 持仓跨关保留、商店只看现金、局末清零。设计 docs/superpowers/specs/2026-09-08-stock-market-design.md，审计 AUDIT §AF。
 *
 * 为什么只能是这两个键：replay 只重放 INPUT_HELD 的位（射击 / 炸弹 / 低速 / 方向 / 用卡 / 切卡），别的键回放会失同步。
 * 用卡位的分派引擎已做好（选中的主动卡才收到 c_press），我们只在 on_activate 里读低速位区分买卖。
 * 随机数走 ce_rand()（REPLAY_SAFE_RNG），与弹幕 / 零售卡同源 ⇒ replay 天然同步。
 *
 * 画图：33 个 UI 层 VM（1 底板 + 16 × {实体, 影线}），脚本 stock_rect（drawRect 1×1，C 写 pos / scale / color 就地改）。
 * UI 层 VM 的坐标空间是 2× 的（HUD 图标根 VM pos(1056, 537) 对应 640×480 空间的 (528, 268)，AUDIT X5），
 * ce_hud_text 用的是 640×480 字面量空间（AUDIT AC6）—— 两套常量分开写。⚠️ 全部「待实跑微调」。 */
#include <string.h>

#include "sdk.h"
#include "stock_core.h"

#define STOCK_SE_BUY    0x0f            /* se_cardget */
#define STOCK_SE_SELL   0x2f            /* se_bonus */

#define STOCK_SPAWN_RETRY_FRAMES 60     /* 面板 VM 持续起不来时的重试间隔（M1，同 PS_RETRY_FRAMES 量级）*/

/* —— 面板几何：UI 层 2× 空间（1280×960）——
 * 2026-09-08 挪到**右上角**（原来在侧栏下部，压着卡组图标）。那块地方零售画的是
 * 总分 `(620, 42)` 与最高分 `(620, 64)`（右对齐，`FUN_0043cd00` 里的字面量，AUDIT §AI），
 * 本 mod 里得分没有意义，盖掉正好。
 *
 * ★ 为什么面板不能垫在文字底下：Gui 的绘制注册在优先级 **0x31**（`Gui__operator_new` `0x43bb4e`），
 *   而 UI 列表的 ANM 层最早也要到 **0x3c**（层 24 的桩）。所以 HUD 文字**永远**先画、
 *   我们的图元永远后画 —— 文字压面板做不到，只能让两者不重叠。读数因此放在面板**下方**
 *   那条空隙里（面板底 96，残机行 120，中间的 104 是空的）。*/
#define UI_CANDLE_X0    856             /* 640 空间 428 */
#define UI_CANDLE_Y0    44              /* 640 空间 22 */
#define UI_CANDLE_W     400             /* 16 根 × pitch 25，实体宽 13 */
#define UI_CANDLE_H     112             /* 640 空间 56 */
/* 底板中心 (528, 50)、尺寸 212×68 ⇒ 顶边落在 640 空间的 y = 16，
 * 正好与弹幕区上边框对齐（相机 0 的视口是 (32, 16, 384, 448)，AUDIT §AI）。*/
#define UI_BG_CX        1056
#define UI_BG_CY        100
#define UI_BG_W         424
#define UI_BG_H         136
/* —— 文字：640×480 字面量空间（AUDIT AC6）—— */
#define TEXT_TICKER_X   528.0f          /* 侧栏中线 */
#define TEXT_TICKER_Y   96.0f           /* 面板底 84 与残机行 120 之间 */
/* 持仓净值：与金钱**同一个右边界**、就在它下面一行，字体也照抄金钱那行（font 10 / group 3），
 * 默认的 font 0 比金钱小一圈，而且贴在金钱左边会撞上「金」的图标。*/
#define TEXT_HOLD_X     620.0f
#define TEXT_HOLD_Y     218.0f
#define TEXT_COLOR      CE_HUD_POWER_COLOR
#define TEXT_HOLD_COLOR CE_HUD_MONEY_COLOR

#define COLOR_BG        0xdc000000u     /* AARRGGBB：黑，alpha 220 —— 要盖住底下的总分 / 最高分，不能太透 */
#define COLOR_UP        0xffff4040u     /* 红涨 */
#define COLOR_DOWN      0xff40ff40u     /* 绿跌 */

typedef struct {
    stock_state_t st;
    uint32_t bg_id;
    uint32_t body_id[STOCK_CANDLES];
    uint32_t wick_id[STOCK_CANDLES];
    uint8_t  frame;                      /* 距上一步的帧数 */
    uint8_t  inited;
    uint8_t  log_spawn_fail;             /* 失败日志只打一次（M2：跟成功日志分开旗标，否则先失败一次后面成功就打不出坐标了）*/
    uint8_t  log_spawn_ok;               /* 成功日志只打一次 */
    uint8_t  spawn_retry;                /* 持续失败时的重试倒计时（M1，见 vms_retry）*/
} stock_card_t;

_Static_assert(sizeof(stock_card_t) <= CE_STATE_BYTES - CE_STATE_RESERVED, "stock_card_t must fit ce_state");

static uint32_t spawn_rect(void)
{
    static const float origin[3] = { 0.0f, 0.0f, 0.0f };
    return ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_STOCK_RECT, origin);
}

static void kill_vms(stock_card_t *s)
{
    ce_anm_delete(s->bg_id); s->bg_id = 0;
    for (int i = 0; i < STOCK_CANDLES; ++i) {
        ce_anm_delete(s->body_id[i]); s->body_id[i] = 0;
        ce_anm_delete(s->wick_id[i]); s->wick_id[i] = 0;
    }
}

/* 33 次查表每帧都跑，不是漏优化：这是故意的自愈检查（VM 可能被引擎在别处摧毁），只在该重绘时才查会晚一帧发现。 */
static int vms_alive(const stock_card_t *s)
{
    if (!s->bg_id || !ce_anm_get_vm(s->bg_id)) return 0;
    for (int i = 0; i < STOCK_CANDLES; ++i)
        if (!s->body_id[i] || !ce_anm_get_vm(s->body_id[i]) || !s->wick_id[i] || !ce_anm_get_vm(s->wick_id[i])) return 0;
    return 1;
}

/* 底板先起（UI 列表尾插 = 后画在上），再 16 组蜡烛。任何一个没了就整组重来，保证叠放顺序。 */
static int spawn_vms(stock_card_t *s)
{
    kill_vms(s);
    s->bg_id = spawn_rect();
    for (int i = 0; i < STOCK_CANDLES; ++i) { s->wick_id[i] = spawn_rect(); s->body_id[i] = spawn_rect(); }
    if (!vms_alive(s)) {
        kill_vms(s);
        if (!s->log_spawn_fail) { ce_log("stock: FAILED to spawn panel VMs"); s->log_spawn_fail = 1; }
        return 0;
    }
    ce_anm_set_pos(s->bg_id, (float)UI_BG_CX, (float)UI_BG_CY, 0.0f);
    ce_anm_set_scale(s->bg_id, (float)UI_BG_W, (float)UI_BG_H);
    ce_anm_set_color(s->bg_id, COLOR_BG);
    if (!s->log_spawn_ok) {
        ce_log("stock: panel spawned at top-right (bg id %u at %d,%d size %dx%d; candles x %d..%d y %d..%d; 2x UI space, /2 = 640 space)",
               s->bg_id, UI_BG_CX, UI_BG_CY, UI_BG_W, UI_BG_H, UI_CANDLE_X0, UI_CANDLE_X0 + UI_CANDLE_W, UI_CANDLE_Y0, UI_CANDLE_Y0 + UI_CANDLE_H);
        s->log_spawn_ok = 1;
    }
    return 1;
}

/* 持续失败时的退避（M1）：跟 sdk.h 的 ce_option_retry 一个思路 —— 倒计时到 0 才真的再试一次整组 33 个 VM，
 * 免得 UI 池长期满着时每帧都白跑一遍查表 + spawn。 */
static int vms_retry(stock_card_t *s)
{
    if (s->spawn_retry) { s->spawn_retry--; return 0; }
    s->spawn_retry = STOCK_SPAWN_RETRY_FRAMES;
    return spawn_vms(s);
}

static void apply_rect(uint32_t id, const stock_rect_t *r)
{
    ce_anm_set_pos(id, (float)r->cx, (float)r->cy, 0.0f);
    ce_anm_set_scale(id, (float)r->w, (float)r->h);        /* w = h = 0 的空槽就是看不见 */
    ce_anm_set_color(id, r->up ? COLOR_UP : COLOR_DOWN);
}

static void draw_candles(stock_card_t *s)
{
    stock_rect_t body[STOCK_CANDLES], wick[STOCK_CANDLES];
    stock_layout(&s->st, UI_CANDLE_X0, UI_CANDLE_Y0, UI_CANDLE_W, UI_CANDLE_H, body, wick);
    for (int i = 0; i < STOCK_CANDLES; ++i) { apply_rect(s->wick_id[i], &wick[i]); apply_rect(s->body_id[i], &body[i]); }
}

/* ctor：每关开场引擎会再调一次；只在还没初始化时铺初值（存档 / replay 装载的 mode 1 不调 ctor，所以不用 owned[] 判「刚获得」）。 */
static int ctor(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    if (s && !s->inited) {
        stock_init(&s->st, ce_rand());          /* 只在这里向引擎那只回放安全 RNG 取一次；之后走自带 LCG（stock_core.h ★）*/
        s->inited = 1;
        ce_log("stock: opened at %d.%02d (seed %08x)", s->st.price / 100, s->st.price % 100, s->st.rng);
    }
    return 0;
}

/* C 键（引擎已门控：对话中 / 无敌人时不会来）。低速位按住 = 卖，否则买。返回 0 = 瞬发。 */
static int on_activate(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    if (!s || !s->inited) return ce_refuse("stock", "no state");
    int sell = (CE_INPUT_HELD() & CE_INPUT_FOCUS) != 0;
    int32_t money = CE_MONEY();
    int ok = sell ? stock_sell(&s->st, &money) : stock_buy(&s->st, &money);
    if (!ok) return ce_refuse("stock", sell ? "no holding" : "no cash");
    CE_MONEY() = money;
    ce_play_sound(sell ? STOCK_SE_SELL : STOCK_SE_BUY, ce_player_x());
    ce_log("stock: %s %d @ %d.%02d -> hold %d.%04d (money %d, cost %d, value %d, pnl %d)",
           sell ? "sell" : "buy", STOCK_LOT, s->st.price / 100, s->st.price % 100,
           s->st.hold / 10000, s->st.hold % 10000, money, s->st.cost, stock_value(&s->st), stock_pnl(&s->st));
    return 0;
}

/* +0x2c：管理器侧每帧（菜单 / 商店里不跑 ⇒ 行情冻结）。每 15 帧走一步；VM 丢了就整组重建。 */
static int on_tick_2(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    int redraw = 0;
    if (!s || !s->inited) return 0;
    if (++s->frame >= STOCK_STEP_FRAMES) {
        s->frame = 0;
        stock_step(&s->st);
        redraw = 1;
    }
    if (!vms_alive(s)) { if (!vms_retry(s)) return 0; redraw = 1; }
    if (redraw) draw_candles(s);
    return 0;
}

/* +0x48：HUD 文字，只能在这里画（AUDIT AC）。 */
static int on_draw(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    if (!s || !s->inited) return 0;
    const stock_state_t *st = &s->st;
    char arrow = st->price > st->cur.o ? '^' : st->price < st->cur.o ? 'v' : '-';
    int32_t pnl = stock_pnl(st), value = stock_value(st);
    /* 面板下方一行：现价 + 涨跌箭头 + 本局盈亏 */
    ce_hud_text(TEXT_TICKER_X, TEXT_TICKER_Y, TEXT_COLOR, "%d.%02d %c %c%d",
                st->price / 100, st->price % 100, arrow, pnl < 0 ? '-' : '+', pnl < 0 ? -pnl : pnl);
    /* 金钱正下方：持仓净值，右边界与金钱对齐、字体也一样大。没持仓就一个字都不画。 */
    if (st->hold > 0)
        ce_hud_text_font(TEXT_HOLD_X, TEXT_HOLD_Y, TEXT_HOLD_COLOR,
                         CE_ASCII_FONT_MONEY, CE_ASCII_GROUP_MONEY, CE_ASCII_ALIGN_RIGHT, "+%d", value);
    return 0;
}

/* +0x4c：局末。持仓 / 行情清零，面板删掉。 */
static void on_run_reset(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    if (!s) return;
    if (s->inited) ce_log("stock: run reset (price %d.%02d, hold %d.%04d, pnl %d, %u steps)",
                          s->st.price / 100, s->st.price % 100, s->st.hold / 10000, s->st.hold % 10000, stock_pnl(&s->st), s->st.steps);
    kill_vms(s);
    memset(s, 0, sizeof *s);
}

/* +0x50：删对象（局末 recount / 倒吊人摧毁）。私有状态还在，只需把 VM 删掉。 */
static void on_delete(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    if (s) kill_vms(s);
}

CE_CARD(80, .active_recharge = 30, .ctor = ctor, .on_activate = on_activate, .on_tick_2 = on_tick_2,
            .on_draw = on_draw, .on_run_reset = on_run_reset, .on_delete = on_delete);
