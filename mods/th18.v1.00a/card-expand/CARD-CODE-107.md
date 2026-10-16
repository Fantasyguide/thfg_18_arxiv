> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
### 107 — 东方妖妖梦（CARD-CODE）

> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本卡文件内**裸地址 = TH18**，引 th07 一律带 `th07:` 前缀。
> 行为 / 成本 / 数值总表见 `CARD-BEHAVIOR-AUDIT.md` §107（P18 只读审计）——本节只写代码端链路，不重抄其表。

**一句话**：收道具（原生消弹小分道具 = ★）、结界中擦弹与自机弹命中一起攒**樱点**与 **+C**；+C 满 50000 自动开**森罗结界 540 帧**（结界中每帧全屏收点、背景压暗、自机红白闪），中弹或按 B 打**灵击**（崩结界 + 全屏消弹由引擎撒原生小分道具 + 40 帧无敌，中弹时取消本次死亡），自然到期则樱点/最大樱点各 +10000 并结算 Border Bonus；樱点本身是该卡的「最大得点」（吃一颗点道具的得分 = 樱点），HUD 为原版两行计数器。（同台账 §107 ✓）

**实现链路**：

1. 注册：`senro.c:1984`–`:1999` 的 `CE_CARD(107, …)` 列 15 个钩子；宏展开 = `sdk.h:918`，虚表槽数组 = `sdk.h:986` 起。
2. ctor `+0x00`：`senro.c:1716` → `senro_clear` `senro.c:1679`（退两个声明层 + 收本卡 UI VM + `faith_unregister` 与清自己槽）→ `senro_init` `senro_core.c:88` → `senro_declare_score` `senro.c:1121`。
3. `on_load +0x20`：`senro.c:1922` → `senro_claim_hud` `senro.c:1748`（`ce_hud_slot_claim_pinned_card` `senro.c:1755`，钉 (68,928)）→ `senro_faith_claim` `senro.c:1887`。
4. `on_stage_start +0x34`：`senro.c:1936` → `senro_stage_reset` `senro_core.c:89`，并收世界层特效 / 压暗 / 公告 / 自己槽的樱点实体。
5. `on_tick_2 +0x2c`（主链）：`senro.c:1307` → ⓿ `senro_hit_cancel_flush` `senro.c:1327` ｜ ① 无敌声明 `senro.c:1330` ｜ ② `senro_tick` `senro.c:1341`（结界计时 / +C 比例 / 自然结束）+ ③ `senro_border_collect` `senro.c:1342` ｜ ④ 擦弹差分 `senro.c:1348` ｜ ⑤ 按 B 灵击 `senro.c:1361` ｜ ⑥ 符卡奖励与小樱点 `senro.c:1367` ｜ ⑦ 计分声明 `senro.c:1386` ｜ ⑨ `senro_fx_tick` / `senro_fx_flush` `senro.c:1408` ｜ ⑩ `senro_dim_tick` `senro.c:1412`。
6. 入账四通道：`on_item_collect` `senro.c:1417` → `senro_item_cherry` `senro_core.c:214`；`on_shot_damage_any` `senro.c:1438` → `senro_hit` `senro_core.c:286`；`on_bomb_spent` `senro.c:1455` → `senro_penalty_bomb` `senro_core.c:369`；`on_death_frame2` `senro.c:1488` → `senro_penalty_miss` `senro_core.c:353`。
7. 计分：`on_item_score` `senro.c:1132` → `senro_declare_score` `senro.c:1121`（`ce_score_declare_delta` `senro.c:1124`，值 = `senro_point_item_value` `senro_core.c:306`；`ce_score_family_declare` `senro.c:1129` 归一 `CE_COEFF_FAMILY_1` `sdk_core.h:342`）；自然结束那一笔分数走 `ce_add_score_delta` `senro.c:1083`。
8. 灵击 / 救援：`on_death_before_deathbomb +0x10` `senro.c:1470` → `senro_do_libyou` `senro.c:1178`（中弹路只置 `hit_cancel_pending` `senro.c:1191` + `senro_cancel_impending_death` `senro.c:1224`，全屏消弹下一帧补 `senro.c:1231`）；按 B 路在 `senro.c:1195` 当场 `ce_cancel_radius`（mode 1）。
9. 表现：`on_item_frame`（优先级 `0x1e`）`senro.c:1979` → `senro_player_flash` `senro.c:967`；`on_draw +0x48` `senro.c:1531` → 标签 `senro.c:1590` / 樱点 `senro.c:1606` + 最大樱点 `senro.c:1620` / +C `senro.c:1633` / 脉动花 `senro.c:1644`，公告 `senro_text_draw` `senro.c:1009`（`ce_hud_text_ex`）。
10. 掉落：`on_enemy_death` `senro.c:1840` → `senro_rot_drop_next` `senro_core.c:246` → `senro_spawn_rot_cherry` `senro.c:1829`；满 P 转换 `senro_faith_tick` `senro.c:1908` → `faith_convert_full_p_items` `faith.c:998` → 收下回调 `senro_on_cherry_pickup` `senro.c:1853`。
11. 清理：`on_run_reset +0x4c` `senro.c:1955` / `on_delete +0x50` `senro.c:1969` → `senro_clear` `senro.c:1679`；112 的折算 `senro_bounty_eq_fold` `senro.c:1286` 由 `senro.c:1314` 每帧读。

**用到的 SDK**（本卡**不用** `sdk_actions.h` —— 它只声明 `ce_give_card_transaction` ⇒ 该模块：无）：

- **VM / ANM**：`ce_anm_spawn` (`sdk.h:86`)、`ce_anm_spawn_ui` (`sdk.h:248`)、`ce_anm_delete` (`sdk.h:393`)、`ce_anm_get_vm` (`sdk.h:240`)、`ce_anm_set_pos` (`sdk.h:265`)、`ce_anm_set_color` (`sdk.h:273`) —— 起 / 收 / 句柄自愈 / 摆位 / 上色；`ce_anm_set_scale` (`sdk.h:336`)、`ce_anm_set_rotation` (`sdk.h:287`) —— 长命环的缩放与自转；`ce_anm_set_sprite` (`sdk.h:257`) —— 数字位换字形。
- **HUD**：`ce_hud_slot_claim_pinned_card` (`sdk.h:1334`)、`ce_hud_slot_get` (`sdk_core.h:849`)、`ce_hud_slot_claimed` (`sdk_core.h:851`)、`ce_hud_band_rect` (`sdk_core.h:825`)、`ce_hud_text_ex` (`sdk.h:851`) —— 钉住槽 / 每帧取最新排布 / 纪元清表检测 / 压暗带矩形 / 公告文字（**仅 on_draw 可调**）。
- **声明层**：`ce_invuln_request` (`sdk_core.h:95`)、`ce_invuln_apply` (`sdk.h:1060`)、`ce_score_declare_delta` (`sdk_core.h:386`)、`ce_score_family_declare` (`sdk_core.h:361`) —— 无敌层声明剩余帧并补落笔 / 计分加数与族归一（合成为 `ce_score_apply` `sdk_core.h:454`）。
- **消弹 / 道具**：`ce_cancel_radius` (`sdk.h:150`)、`ce_cancel_latch_claim` (`sdk_core.h:483`)、`ce_cancel_latch_release` (`sdk_core.h:485`)、`ce_cancel_latch_owner` (`sdk_core.h:484`)、`ce_cancel_frame_count` (`sdk_core.h:487`)、`ce_item_collect_all` (`sdk.h:1035`) —— 全屏消弹（mode 1）/ 闩锁与空认领交回 / 认领者与帧内颗数 / 结界全屏收原生道具。
- **玩家 / 状态**：`ce_rescue_note_claim` (`sdk_core.h:503`)、`ce_player_pos` (`sdk.h:486`)、`ce_player_x` (`sdk.h:492`)、`ce_state` (`sdk/public.h:19`) —— 救援层报到（返回值必须丢弃）/ 取自机坐标 / 卡私有状态。
- **计分 / 表现 / 杂**：`ce_add_score_delta` (`sdk.h:455`)、`ce_play_sound` (`sdk.h:50`)、`ce_screen_shake` (`sdk.h:468`)、`ce_rand` (`sdk.h:464`)、`ce_spell_active` (`sdk.h:605`)、`ce_spell_flags` (`sdk.h:600`)、`ce_log` (`card_expand.h:25`) —— 自然结束加分 / 7 处 SE / 震屏 / 相位随机 / 符卡下降沿 / 日志。

（`ce_layer_drop_card` 与 `ce_score_apply` 由 `CE_CARD` 宏在 SDK 侧调，本卡不直接调 —— 见 `sdk.h:977` 与 `sdk.h:982`。）

**数值**：

| 数值 | 值 | 单位 | 站点 | 一手出处 |
| --- | --- | --- | --- | --- |
| +C 满槽阈值（开结界门）| 50000 | 存储（+C 绝对量）| `senro_core.h:106`、`senro_core.c:120` | `th07:0x42f623` 的 `0xc350` |
| 森罗结界时长 | 540 | 帧（9 s @60fps）| `senro_core.h:107`、`senro_core.c:98` | `th07:0x4419fa` 写 `0x21c` |
| 灵击后的无敌 | 40 | 帧（≈0.67 s）| `senro_core.h:120`、`senro_core.c:342` | `th07:0x441d8e` 写 `0x28` |
| 结界自然结束加值 | 10000（樱点与最大樱点各一次）| 存储 | `senro_core.h:121`、`senro_core.c:180` | `th07:0x44167a` / `th07:0x441689` |
| ★（消弹小分道具）加值 | 20（樱点与 +C 各 +20）| 存储 | `senro_core.h:127`、`senro_core.c:222` | `th07:0x43370b`→`th07:0x433712` |
| 大樱点（type 7）加值 | 1000 + 100×符卡奖励次数 | 存储 | `senro_core.h:125`、`senro.c:1878` | `th07:0x433886`、`th07:0x433895` |
| 结界中擦弹加值 | 80（高速）/ 30（低速）| 存储（同时加到樱点与最大樱点）| `senro_core.h:130`、`senro_core.c:205` | `th07:0x43ed96` / `th07:0x43ed7c` |
| 点道具分值（= 本卡「最大得点」）| 50000 + max(0, 樱点−50000)/5，向下取整 10 | 显示 / 得分 | `senro_core.h:134`、`senro_core.c:315` | `th07:0x43310a`、`th07:0x4331c8`、`th07:0x4331db` |

其余（Miss 与 Bomb 的扣减比例、小樱点赞与封顶、HUD 版式常数、SE 号、震屏）见 `CARD-BEHAVIOR-AUDIT.md` §107 的表，不重抄。

**跨卡协同**：

- **110 风神录 / 111 地灵殿（共享信仰点池，`FAITH_SLOT_107 = 3` `faith_core.h:29`）**：`senro_faith_claim` `senro.c:1887` 注册、`senro_faith_tick` `senro.c:1908` 每帧调 `faith_convert_full_p_items` `faith.c:998`（满 P 时一颗 P 道具给各 `conv_value>0` 注册者同帧各撒一颗点）、`senro.c:1151` 用 `faith_collect_all` 顺手收绿点；删卡顺序 `faith_unregister` + `faith_clear_entities_mask(1 << FAITH_SLOT_107)` `senro.c:1697`–`:1698`。见 `SENRO-107-CHERRY-ENTITY.md` §2.3 / §3。
- **112 星莲船（等价计数广播）**：`ufo_bounty_eq_peek`（`ufo_bounty.h:43`）、`ufo_bounty_eq_log_n`（`ufo_bounty.h:61`）、`ufo_bounty_eq_log`（`ufo_bounty.h:62`）；107 在 `senro.c:1314` 每帧折自己槽（`senro_bounty_eq_fold` `senro.c:1286`，按 `slot_mask` 过滤、走本卡自己的收下回调）。见 `HANDOFF-CROSSCARD-STATE.md` §13。
- **117 动物灵 / 113 灵界（救援层）**：中弹在 `senro.c:1470` 无条件烧结界并报到 `ce_rescue_note_claim`（返回值丢弃 `senro.c:1481`），与 117 同时、顺序 107 → 117；全屏消弹只给第一家、无敌走声明层取 max。见 `EVENTS-CONTRACT.md` §5.2。
- **同帧消弹的其他卡**：共用「本帧已消过弹」闩锁 —— `senro.c:1194`–`:1203`（空认领 `release` 交回）与 `senro.c:1241`–`:1251`（让位但仍推进状态机）；见 `CARD-NATIVE-CONFLICT-AUDIT.md` §G。
- **计分层（SDK 合成）**：本卡只声明 `G_07`（`senro.c:1124`）与族（`senro.c:1129`），不再与 110/111 争落笔权。见 `SCORE-LAYER-REWORK.md`、`CARD-NUMERIC-INTERACTION-AUDIT.md` §1.6 / §6。
- **HUD 共享件**：`hud_dim_init`（`hud_dim.h:170`）、`hud_dim_tick`（`hud_dim.h:184`）压暗、`ce_hud_band_rect` 给「带」矩形；钉住位与 112 / 113 / 117 同一条底线（`senro.c:323`–`:332`）。
- **共享 PIV 池（`MAXPOINT-AUDIT.md`）**：该文**无本卡条目** —— 107 不调 `ce_piv_add`，其「额外最大得点」是自算的樱点值，只经 `ce_score_declare_delta` 进分。

**生命周期**：

- 钩子覆盖：ctor `senro.c:1716`（注册 `senro.c:1985`）、`on_load` `senro.c:1922`、`on_stage_start` `senro.c:1936`、`on_run_reset` `senro.c:1955`、`on_delete` `senro.c:1969` 五条**都在**；清理集中 `senro_clear` `senro.c:1679`。
- 跨帧物与清点站点：无敌声明 / 计分声明 `senro.c:1684`–`:1685`（`CE_CARD` 宏另做一次，`sdk.h:977` / `sdk.h:982`）、HUD 槽 + 本卡全部 UI VM `senro.c:1686`（`senro_hud_clear` `senro.c:632`）、世界层 1 环 + 32 颗 = 33 个句柄 `senro.c:1691`（`senro_fx_clear` `senro.c:884`）、压暗罩 / 自机本体色 / 公告 `senro.c:1694`（`senro_period_clear` `senro.c:991`）、共享池自己槽 `senro.c:1697`–`:1698`。
- **已知违反（`LIFECYCLE-AUDIT.md` §2.1）**：本卡注册了 `.ctor`（`senro.c:1985`）且 ctor 无条件 `senro_clear`（`senro.c:1679`）整块 memset；引擎**每关开场**会对每张卡再调一次 ctor ⇒ `spell_bonuses` / `drop_rot` / 统计被连坐清，**换面保持 ❌**（樱点 / +C 每关重设是复刻一手，不计）。同文 §2 把 107 与 108 并列为实锤违反。
- 删卡路：发生在**商店里**（倒吊人 / 原版空白卡），那一刻 AbilityManager 冻着、卡的 tick 全停 ⇒ 只能靠 `on_delete`（虚表 `+0x50`），不能等下一帧（`senro.c:1688`–`:1690` 注）。

**已知未测 / 风险**：

- 「结界中每帧全屏自动收点」只引到区间 `th07:0x4414xx` + 用户口径，**没有逐指令地址** ⇒ 一手性未核（`senro.c:1141`）。
- 注释与代码 / 文档漂移，未对账：头注「7 s」（`senro.c:20`）vs `senro_core.h:107` 的 540 帧；HUD 块宽注释仍写 200 / 286（`senro.c:303`–`:306`、`:1746`）vs 代码 `SENRO_HUD_W 280`（`senro.c:308`）；`SENRO_HUD_PIN_Y` 注释史与 `AUDIT.md` §BE #241 记的 932 对不上代码的 928（`senro.c:324`）。
- `SENRO_DIM_ENABLE` 代码为 1（`senro.c:198`），而同文件 `senro.c:902`–`:905` 注释仍写「默认关」；且压暗一手曲线触发值 `ds:0x4d44e0` 全 exe 找不到写点、原文自标 🟡（`engine/spirit/th07/01-barrier-visual.md` §4.3）⇒ 遮罩是否等于原版未实跑验证。
- ❓自定数值无一手：小樱点触发的 ≥8 颗门与 24 颗封顶（`senro.c:108`–`:109`）、樱点满值公告 30 帧节流（`senro.c:424`）、震屏 20 / 12 / 0（`senro.c:151`–`:153`）、Miss 1/2 与 Bomb 1/4（`senro_core.h:150`–`:157`）。
- `senro.c`（1999 行）**没有卡侧单测**：`tests/test_senro_core.c:1`–`tests/test_senro_core.c:35` 只覆盖纯逻辑 `senro_core.c`；HUD VM 池、faith 池、世界层特效、公告通道的引擎侧行为无主机证据。
- 中弹灵击的消弹推迟一帧、且 claim 失败只记日志（`senro.c:1230`–`:1250`）—— 当前靠「117 只在后续帧消弹」成立；`AUDIT.md` §AX3 已登记为跨卡缺口，未实跑验证将来有卡在 `+0x10` 就消弹的情形。
- `VM-IDIOM-MAP.md` I2：本卡 `senro.c:981`、`senro.c:982`、`senro.c:984`、`senro.c:985`、`senro.c:998`、`senro.c:999` 成对直写自机内嵌 VM 的 `color_1` + `color_2`（3 个写点 × 2 条 store）—— 现有 `ce_anm_*` 无「一步成对」封装。
- 112 压缩包吃掉本卡樱点时，小樱点的「打包值」会被破坏（`HANDOFF-112.md` §①c）：归属靠 `slot_mask` 保住，数值对不上 107 的账。
