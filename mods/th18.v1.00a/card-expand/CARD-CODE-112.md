> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
### 112 — 东方星莲船（CARD-CODE）

**一句话**：宿主小怪/Boss 击破掉令牌 → 自机碰到收入左下角三格槽 → 三枚成组即召唤一只 ECL 真敌机 UFO → 它吸走全场 P / 金钱道具与 110/111 信仰点 → 击破后按吸收量开奖（压缩包 + 返还令牌 + 加成道具）；超时飞走无奖。（同台账；行为表 / 成本 / 完整跨卡与未测见 `CARD-BEHAVIOR-AUDIT.md` §112）

卡本体 = `mods/th18.v1.00a/card-expand/native/cards/ufo.c`；纯逻辑（凑组状态机 / 奖励表 / 磁场几何）在 `ufo_core.{h,c}`。文件尾 `ufo.c:6452` 起是「占位 / TODO：取值一律 = 不做」清单。

**实现链路**：从钩子到落笔（`文件:行号`）

1. 注册：`CE_CARD(112, …)` = `ufo.c:6388`–`6397`，按实读 = 9 项（台账 §112 记 8）：`on_load` / `on_stage_start` / `on_item_frame` / `on_tick_2` / `on_hud_anm`（只打日志）/ `on_draw` / `on_enemy_death` / `on_run_reset` / `on_delete`。
2. 建 / 复位：`on_load`（`ufo.c:5625`）调 `on_ctor`（`ufo.c:5581`）+ HUD 认领（`ufo.c:5636`）；`on_ctor` 按 `s_stage_seen` 判换关，换关时**保留令牌池 + 槽**（`ufo.c:5594`–`5605`）。
3. 令牌来源：`on_enemy_death`（`ufo.c:5734`）只登记落点 —— 命中宿主表 / boss 才填 `pend_drop*`（`ufo.c:5788`–`5807`）；帧末 `on_tick_2`（`ufo.c:6245`–`6268`）才建 VM（`ufo_tok_spawn` `ufo.c:1884`）。
4. 收取入槽：`on_item_frame`（`ufo.c:5866`）→ `ufo_tokens_tick`（`ufo.c:3885`）：半径读 `player+0x4798c`（`ufo.c:3684`）、接触判定 `ufo.c:3949`；入槽 `ufo_token_to_slot`（`ufo.c:3715`）→ 纯逻辑 `ufo_group_put`（`ufo_core.h:214`）。
5. 召唤：`ufo_try_summon`（`ufo.c:3765`）取走成组结果（`ufo.c:3779`）→ 一手 HP 档（`ufo.c:3785`）→ `ufo_body_spawn`（`ufo.c:1935`）内 `ce_spawn_enemy`（`ufo.c:1954`）→ 跟随 VM（`ufo.c:3827`）+ 吸收环 / 血条（`ufo.c:3830`）。
6. 吸收：`on_item_frame`（`ufo.c:6080`）→ `ufo_absorb_tick`（`ufo.c:3542`，扫 `CE_ITEM_SLOTS` `ufo.c:3558`）→ `ufo_try_item`（`ufo.c:3454`）；信仰点同拍 `ufo_absorb_faith_tick`（`ufo.c:3602`）；吸收满 ⇒ `ufo_fill_complete`（`ufo.c:3390`）撒奖励（`ufo.c:3414`）。
7. 击破 / 飞离：死亡回调记 `pending_break`（`ufo.c:5743`），`on_tick_2` 调 `ufo_break`（`ufo.c:6238` → `ufo.c:5080`：赏金 / ×N / 大扩圈 / 返还令牌 / 压缩包）；本体 id 消失而不报死亡 ⇒ 只收尾不发奖（`ufo.c:6272`–`6275`）。
8. 开包：`ufo_drops_tick`（`ufo.c:4709`）→ 碰到即 `ufo_drop_collect`（`ufo.c:4768` → `ufo.c:4622`）：`ce_add_score_delta`（`ufo.c:4654`）+ 等价计数广播（`ufo.c:4680`）。

**用到的 SDK**：本卡调用的 `ce_*`（`sdk_actions` 无调用 = 无）

| API | 用途 | 定义 |
| --- | --- | --- |
| `ce_spawn_enemy` | 建 UFO 本体（真敌机，指定 ECL 子程序） | `sdk.h:1197` |
| `ce_spawn_item` | 撒奖励 / 金钱 / 满 B 原生道具 type 7 | `sdk.h:413` |
| `ce_add_bomb` / `ce_money_pocket` / `ce_add_score_delta` | 整颗 B / 池满折钱 / 加分 | `sdk.h:99` / `883` / `455` |
| `ce_piv_claim_auto` / `ce_piv_add` / `ce_graze_family_claim` | 共享 PIV：报可写上限 / 满槽 +1000 / 擦弹族 | `sdk_core.h:426` / `429` / `439` |
| `ce_dup_claim` / `ce_dup_publish` / `ce_dup_should_run` | 杰拉复制：认领满 B / 报可见数 / 本帧跑 | `sdk.h:1088` / `1085`；`sdk_core.h:1049` |
| `ce_sweep_should_run` | 92 清扫发动时把场上令牌变金钱 | `sdk_core.h:1076` |
| `ce_fairy_group_claimed` | 110/111 妖精护栏（只读，跳过被认领宿主） | `sdk.h:1147` |
| `ce_anm_spawn` / `ce_anm_spawn_ui` / `ce_anm_delete` / `ce_anm_get_vm` | 自建 VM 全链（本体 / 令牌 / 环 / HUD） | `sdk.h:86` / `248` / `393` / `240` |
| `ce_hud_slot_claim_pinned_card` / `ce_layer_drop_card` | 左下三格槽认领 / 退声明 | `sdk.h:1334` / `1096` |
| `ce_state` / `ce_state_free` | 卡私有状态 `ufo_card_t`（预算由 `ufo.c:6400` 断言守） | `sdk/public.h:20` / `sdk_core.h:61` |
| `ce_difficulty` / `ce_sdk_frame` / `ce_key_down` | 难度查 PIV 两表 / 帧钟播种 / 调试键 | `sdk.h:1076` / `sdk_core.h:618` / `sdk.h:841` |
| `ce_assume_fail` | 吸收时道具 state 越文档域 ⇒ 跳过 + 记账 | `sdk.h:389` |

**数值**：

| 项 | 值 | 单位 | 站点 | 一手出处 |
| --- | --- | --- | --- | --- |
| 凑组槽位 | 3 | 格 | `ufo_core.h:137` | 一手三同色 / 三色各一 `th12:0x422e80` |
| 令牌同时上限 | 6 | 枚 | `ufo_core.h:759` | ❓自定（一手是敌人掉落表产物） |
| 令牌 TTL | 2400 | 帧 | `ufo_core.h:764` | ❓自定（一手 1200 帧只停反弹 `ufo_core.h:823`） |
| UFO 存活上限 | 720 | 帧 | `ufo_core.h:266` | 一手 `zUfo+0x28` |
| 首吸帧 | 60（门 = 59） | 帧 | `ufo_core.h:272` / `271` | 一手 `th12:0x4271c0` |
| 真血量（五档） | 1600 / 1900 / 2100 / 2300 / 2500 | HP | `ufo_core.c:719` | 一手 `th12:0x4b33f0` |
| 吸收半径 | 18 | px | `ufo_core.h:319` | 一手 |
| 满槽补偿 | +1000 显示（池写 100000 存储） | 点 | `ufo_core.h:186`、`ufo.c:5019` | 一手 `th12:0x4270d8` |

**跨卡协同**：

- 110/111 信仰点 + 107 樱点（共享点池 `faith.h`）：`ufo_absorb_faith_tick`（`ufo.c:3602`）按池枚举（`faith_absorb_list` `faith.h:270`）、`faith_absorb_take`（`faith.h:271`）取走；开包广播等价计数（`ufo.c:4675`–`4682`，协议 `ufo_bounty.h:31`–`62`），107/110/111 各自按 `mask` 折回本卡账（见 `CARD-BEHAVIOR-AUDIT.md` §112 跨卡）。
- 杰拉 / 复制系统：`ce_dup_claim`（`ufo.c:6228`）认领满 B 原生道具，`ce_dup_publish`（`ufo.c:6229`、`6233`）报可见数，`ce_dup_should_run`（`ufo.c:6231`）触发 `ufo_dup_run` / `ufo_dup_tokens`（`ufo.c:6203`）。
- 92 清扫系统：`ce_sweep_should_run`（`ufo.c:6232`）⇒ `ufo_sweep_to_money`（`ufo.c:1509`）把每只令牌变 10 枚金钱。
- 110/111 妖精护栏：`ce_fairy_group_claimed` 每帧只读（`ufo.c:2824`、`2855`、`5778`），被认领宿主让开（不换皮、不掉令牌）。
- 共享 PIV 池：`ce_piv_claim_auto` + `ce_graze_family_claim`（`ufo.c:5872`–`5873`），满 3 令牌再收一枚 ⇒ `ce_piv_add(112, 1000)`（`MAXPOINT-AUDIT.md` §2：th12 蓝点只读、UFO 满 3 = +1000）。

**生命周期**：

- 钩子覆盖：`on_load` → `on_ctor`（`ufo.c:5625` / `5581`）；`on_stage_start`（`ufo.c:5673`）只收本体 / 环 / 血条 / 召唤特效 / 宿主皮，**显式保留令牌池 + 槽**（`ufo.c:5693`、`5699`）；`on_run_reset`（`ufo.c:6314`）与 `on_delete`（`ufo.c:6358`）都汇到 `ufo_withdraw_all`（`ufo.c:5644`）。
- 未注册 `.ctor` ⇒ 换关不被 ctor 清；按 `LIFECYCLE-AUDIT.md` §2.1 = 「没注册 ctor + 换面显式保留」的正解，且 `on_stage_start` 那条是 2026-09-23 已修的 bug。
- 已知残留（`LIFECYCLE-AUDIT.md` F7f / §2 的 112 行）：`s_bounty_eq*`（旧 `take()` 路径隐患，新 `peek` 读法免疫）、`s_eq` / `s_faith_log_*`（只在下一次召唤 / 结包时清）、`s_bolt[]` + Lcg（视觉 scratch）。无「只靠 `on_delete`」项。

**已知未测 / 风险**：

- 金在凑组里算不算独立一色：`UFO_GOLD_COUNTS_AS_BLUE` 默认 1 = 金 ≡ 蓝（`ufo_core.h:56`）；一手颜色域无金 ⇒ 待拍。
- 令牌「谁掉 / 什么颜色」无 TH18 一手：宿主表（`ufo_carrier_table.h:41`）与随机起始色是 ❓ 自定占位。
- 五边形框 / 令牌的层号（12 / 13）在 TH12↔TH18 层轴是否逐档等价未实测（`ufo.c:613`、`ufo.c:429`）。
- 击破得分权重 `UFO_BOUNTY_SCORE_PER_*` = 0（`ufo_core.h:563`）⇒ 击破本体加 0 分；分值实际走压缩包结算（`ufo.c:4654`）。
- 「飞走」当前做不到：占位 ECL 不移动，存活到 720 帧只打一条日志（`ufo.c:5950`–`5952`）；无停循环音 API（`ufo.c:5062`）。
- 击破 ×N 弹字间距 owner 仍判偏宽，已登记待办（`HANDOFF-112.md` §3 ①b）；观感项（槽位 / 满 B / 大横幅 / 闪电）仍待眼判（同上 §3 ①）。
