> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
### 108 — 东方永夜抄（CARD-CODE）

> 代码 = `mods/th18.v1.00a/card-expand/native/cards/youkai.c`（下文简写 `youkai.c`；纯逻辑在 `youkai_core.c` / `youkai_core.h`，单测 `tests/test_youkai_core.c`）。只读文档，未改任何文件。
> 台账出处：`CARD-BEHAVIOR-AUDIT.md` §108（在 `docs/p18-card-behavior-audit` 分支）；`LIFECYCLE-AUDIT.md` = `docs/p17-lifecycle`、`VM-IDIOM-MAP.md` = `docs/p14-vm-idiom-map`、`MAXPOINT-AUDIT.md` = `docs/p16-maxpoint`（四份均未落在 main 工作树）。

**一句话**：低速（按 Shift 聚焦）时整机切成 TH08 妖侧 —— 本体外观、低速子机、逐（机体 × 火力档）的妖侧出弹组一起换；高速回原机体；同时把敌人的每个发射口按 1:1 长成可打的「使魔」真敌人，打死它那个出点就哑火，使魔挨的伤害转嫁给母机，另带 TH08 的人妖槽（含逢魔）、永点（刻符）与夜点系统。（同台账 ✓）

**实现链路**：
1. **注册**：`youkai.c:9689` 的 `CE_CARD(108, …)` 挂 16 个回调 —— `.ctor` / `.on_load` / `.on_power_level_change` / `.on_tick_shooters` / `.on_tick_2` / `.on_bullet_created` / `.on_enemy_death` / `.on_bullet_hit` / `.on_enemy_shot_slot` / `.on_shot_set_index` / `.on_draw` / `.on_item_frame` / `.on_death_before_deathbomb` / `.on_option_pose_apply` / `.on_run_reset` / `.on_delete`。
2. **侧别**：读引擎原值只此一处 `yk_real_focused()`（`youkai.c:377`）；`yk_focused()`（`youkai.c:386`）在 `s_side_lock >= 0` 时返回锁定侧（`s_side_lock` 定义 `youkai.c:364`）；每帧状态机 `yk_side_lock_tick()`（定义 `youkai.c:423`，`on_tick_2` 首调 `youkai.c:9003`）。P0 自递归事故留档 `youkai.c:371`。
3. **本体换装**：`on_item_frame`（`youkai.c:7185`）→ `yk_body_sync`（调用 `youkai.c:7188`，定义 `youkai.c:7127`）→ `yk_body_load_script`（定义 `youkai.c:7104`）；进/出低速的换形分支 `youkai.c:7145` / `youkai.c:7148`，相位理由 `youkai.c:7170`。`on_draw`（`youkai.c:7191`）已是空壳。
4. **低速子机**：`on_power_level_change`（`youkai.c:7202`）逐槽 `ce_option_claim`（`youkai.c:7220`）→ `yk_option_refresh`（`youkai.c:7226`，定义 `youkai.c:4210`）→ `yk_option_declare`（`youkai.c:4171`）→ `ce_option_pose_declare`（`youkai.c:4202`）；帧界落笔 `on_option_pose_apply`（`youkai.c:4351`）：选举门 `youkai.c:4376`、回收多余槽 `youkai.c:4417`、`ce_option_get` `youkai.c:4433`。
5. **妖侧出弹**：`on_shot_set_index`（`youkai.c:7251`）—— 0 子机放行 `youkai.c:7264`、选组 `youkai.c:7281`；`on_tick_shooters`（`youkai.c:7296`）**不再自发弹**（`youkai.c:7321`），改由引擎行循环发；逐发数值 `on_bullet_created`（`youkai.c:7334`）→ `yk_bullet_damage_apply`（`youkai.c:7347`）。
6. **发射口 → 使魔**：站点 `on_enemy_shot_slot` = `yk_on_shot_slot`（`youkai.c:7484`）—— 序号/键 `youkai.c:7496`、props 抄回 `youkai.c:7533`、闸 `yk_shot_slot_gate`（调用 `youkai.c:7667`，定义 `youkai_core.c`）、否决返回 `youkai.c:7700`；建壳 `yk_spawn_familiar_parent`（`youkai.c:1247`）→ `ce_spawn_enemy_keep_offscreen`（`youkai.c:1280`），life > 999 回写三格（`youkai.c:1284`）；每帧钉位 `yk_pin_familiar`（`youkai.c:1392`）。
7. **伤害转嫁**：`yk_transfer_tick`（`youkai.c:3805`）→ `ce_raise_damage_cap`（`youkai.c:3984`）+ `ce_damage_rect`（`youkai.c:3985`）；抑制/丢账理由 `yk_xfer_suppress_reason`（`youkai.c:3761`）。
8. **打死使魔 → 关火**：`on_enemy_death`（`youkai.c:7791`）—— 波次边沿（KILL_ALL）`youkai.c:7817` / `youkai.c:7818`；KILLED / BOMB_KILL 走 `yk_fam_broken`（`youkai.c:7834`）⇒ 收跟随 VM `youkai.c:7843`、−gauge/12 `youkai.c:7852`、掉 1 刻符 `youkai.c:7853`、「+1」弹字 `youkai.c:7860`。
9. **人妖槽 / 刻符 / 吸收区**：`yk_gauge_tick`（调用 `youkai.c:9404`，定义 `youkai.c:5797`）；`yk_orbs_tick`（调用 `youkai.c:9121`，定义 `youkai.c:5460`）与 `yk_orb_spawn`（`youkai.c:5080`）；`yk_zone_tick`（调用 `youkai.c:9568`，定义 `youkai.c:5967`）—— 仲裁 `ce_cancel_latch_owner` `youkai.c:5976` + `ce_cancel_radius` `youkai.c:5995`。
10. **人侧撞使魔 = 一次普通 MISS**：`yk_fam_collide_tick`（调用 `youkai.c:9590`，定义 `youkai.c:3150`）；判定半径读 `youkai.c:3163`，`ce_forced_death_begin/end` 门在 `youkai.c:3179` / `youkai.c:3183`。
11. **计分声明**：族 `ce_score_family_declare`（`youkai.c:9405`）+ 加数 `ce_score_declare_delta`（`youkai.c:9412`）。
12. **收尾**：`yk_drop_all`（定义 `youkai.c:3109`）在 `on_run_reset`（`youkai.c:6998`）与 `on_delete`（`youkai.c:7029`）各调一次。

**用到的 SDK**（定义位置 = `native/sdk.h` / `sdk_core.h` / `sdk/public.h`）：
- 卡状态 / 日志：`ce_state`（取私有块，`sdk/public.h:20`）、`ce_card_id`（`sdk.h:21`）、`ce_sdk_frame`（`sdk_core.h:618`）、`ce_log`（`sdk/public.h:268`）、`ce_assume_fail`（`sdk.h:389`）。
- ANM VM：`ce_anm_spawn`（`sdk.h:86`）/ `ce_anm_spawn_ui`（`sdk.h:248`）起 VM，`ce_anm_get_vm`（`sdk.h:240`）取指针，`ce_anm_set_pos` / `set_sprite` / `set_color` / `set_color2` / `set_scale` / `set_rotation`（`sdk.h:265/257/273/297/336/287`）逐帧落笔，`ce_anm_interrupt`（`sdk.h:391`）/ `ce_anm_delete`（`sdk.h:393`）控状态与回收。
- 敌人：`ce_enemy_by_id`（`sdk.h:619`）、`ce_enemy_pos`（`sdk.h:569`）、`ce_enemy_lockable`（`sdk.h:584`）、`ce_spawn_enemy_keep_offscreen`（`sdk.h:1206`，使魔壳免「出框即删」）。
- 子机 / 阵型声明层：`ce_option_pose_declare`（`sdk.h:706`）、`ce_option_pose_winner`（`sdk.h:708`）、`ce_option_pose_pose`（`sdk.h:709`）、`ce_option_get`（`sdk.h:674`）、`ce_option_claim`（`sdk.h:648`）、`ce_option_drop`（`sdk.h:654`）、`ce_option_x`/`ce_option_y`（`sdk.h:690/691`）、`ce_native_option_hide_declare`（`sdk.h:732`）、`ce_native_option_hidden`（`sdk.h:733`）、`ce_native_option_keep_records`（`sdk_core.h:586`）、`ce_native_option_sweep_keep_state`（`sdk.h:743`）、`ce_repopulate_options`（`sdk.h:402`）、`ce_kill_retail_option_lasers`（`sdk.h:673`）。
- 伤害 / 消弹：`ce_damage_rect`（`sdk.h:172`）、`ce_raise_damage_cap`（`sdk.h:194`）、`ce_cancel_radius`（`sdk.h:150`）、`ce_cancel_latch_owner`（`sdk_core.h:484`）、`ce_forced_death_begin`/`_end`（`sdk_core.h:518/519`）。
- HUD：`ce_hud_slot_claim_pinned_card`（`sdk.h:1334`）、`ce_hud_slot_get`/`ce_hud_slot_claimed`（`sdk_core.h:849/851`）、`ce_hud_vm_probe`（`sdk.h:1233`）、`ce_hud_band_dim_alpha`（`sdk_core.h:829`，HUD 压暗共享带）。
- 计分 / 共享池：`ce_score_family_declare`（`sdk_core.h:361`）、`ce_score_declare_delta`（`sdk_core.h:386`）、`ce_score_total`（`sdk_core.h:451`）、`ce_piv_pool`/`ce_piv_written`/`ce_piv_family_points`（`sdk_core.h:430/432/434`）、`ce_money_pocket`（`sdk.h:883`）。
- 跨卡 / 共享状态：`ce_card_flag_any`（`sdk_core.h:1022`）、`ce_card_flag_set`/`_get`（`sdk_core.h:1018/1020`）、`ce_dup_publish`（`sdk.h:1085`）、`ce_dup_should_run`（`sdk_core.h:1049`）、`ce_sweep_bump`/`ce_sweep_should_run`（`sdk_core.h:1074/1076`）、`ce_graze_peek`/`ce_graze_take`（`sdk.h:444/437`）、`ce_input_z_frozen`（`sdk.h:688`，Z 桥）。
- 杂项：`ce_rand`（`sdk.h:464`）、`ce_key_down`（`sdk.h:841`）、`ce_play_voice`（`sdk.h:65`）、`ce_spell_active`（`sdk.h:605`）、`ce_spawn_item`（`sdk.h:413`）、`ce_poc_height_semantic`（`sdk.h:554`）。

**数值**：

| 值 | 单位（显示 / 存储 / 帧） | 站点 | 一手出处 |
| --- | --- | --- | --- |
| 低速子机数量 霊夢{1,1,1,1,1} / 魔理沙{1,1,1,1,1} / 咲夜{4,4,4,4,4} / 早苗{0,1,1,1,1} | 台（存储） | `youkai_core.h:145`–`youkai_core.h:148` | 各机体妖侧原作数量；台账 §108 |
| 人妖槽量程 ±10000、逢魔门 ±8000、中人/中妖 ±2000、死区 9 | 槽值（存储） | `youkai_core.h:1521`–`youkai_core.h:1528` | `th08:0x44d9ee` / `th08:0x44da09` / `th08:0x44d9f7` / `th08:0x44da12` |
| 击破敌机 ±200（聚焦 + / 非聚焦 −） | 槽值（每只） | `youkai.c:6074` | `th08:0x42da2a` 一族；台账 §108 |
| 收一颗刻符 ±111；击破魔使 −gauge/12；之后 50 帧静默 | 槽值（每颗 / 每台 / 帧） | `youkai_core.h:1529`、`youkai.c:7852`、`youkai_core.h:2218`–`youkai_core.h:2219` | `th08:0x4413cb`、`th08:0x42b216`（`mov ecx,0xc`）、`th08:0x42b240` |
| 伤害转嫁 100%（`YK_TRANSFER_PCT`；一手分母 `YK_TRANSFER_DEN`=2 保留可切 50） | %（存储） | `youkai_core.h:1467`、`youkai_core.h:1456` | owner 2026-10-01 定案；一手 1/2 见台账 §108 |
| 使魔 life 100 / hurtbox 半宽 12.0 / 人侧撞判半径 3.0 | 点 / 像素（存储） | `youkai_core.h:1000`、`youkai_core.h:1502`、`youkai.c:3163` | `th08:0x41a840`；`youkai_core.h:989`–`youkai_core.h:999` |
| 发射口闸自动重建 TTL 600；波次间隔 24 帧 + 3× 自适应 | 帧（存储） | `youkai_core.h:1046`、`youkai_core.h:1072`、`youkai_core.h:1087` | ❓自定（owner 2026-10-01 / 2026-10-11） |
| 刻符池 1024 / 映射使魔 32 / 绕转使魔 48 / 外部采样 32 | 格（存储） | `youkai.c:4873`、`youkai_core.h:947`、`youkai_core.h:2402`、`youkai.c:3687` | 工程护栏（❓自定，非游戏性数值） |

**跨卡协同**：
- **113 灵界 / 117 暴走咆哮**：本卡读共享只读标志位 `ce_card_flag_any`（`youkai.c:413` = `CE_CARD_FLAG_TRANCE`、`youkai.c:414` = `CE_CARD_FLAG_ROAR`，定义 `sdk_core.h:1011/1012`），喂进 `yk_side_switch_allowed`（`youkai.c:441`）⇒ 进行中时侧别被钉住（放/收子机与换出弹组一并停）。落地过程见 `HANDOFF-108-CROSSCARD-SHIFT.md`。
- **111（魔理沙机体卡）**：共享「原生低速子机」声明层 —— `ce_native_option_hide_declare`（`youkai.c:9449`）、`ce_native_option_keep_records`（`youkai.c:9456`）、`ce_native_option_sweep_keep_state`（`youkai.c:9459`）；两边都声明时隐藏按 OR 生效，选举阶梯 108 默认 0 / 111 −1（见台账 §108）。
- **106（红魔乡）**：卡组里有 106 ⇒ 决死 B 多扣 1（总消耗 2，clamp ≥ 0）；判据只读 AbilityManager 卡链表 `yk_has_card`（`youkai.c:7914`），落笔 `youkai.c:7940`–`youkai.c:7943`；无 106 时一个字节都不写。
- **全屏消弹仲裁（109 / 112 / 113 / 116 / 117…）**：吸收区吃弹前先问 `ce_cancel_latch_owner()`（`youkai.c:5976`）—— 本帧别人已认领 ⇒ 108 整帧不吃弹、也不认领闩锁（只观察），说明写在 `youkai.c:5921`。
- **88（Jera 杰拉）刻符复制代数闸**：每帧 `ce_dup_publish`（`youkai.c:8697`）上报活刻符数，`ce_dup_should_run`（`youkai.c:8698`）为真 ⇒ `yk_dup_run`（`youkai.c:8673`）按同坐标/同生态重撒一颗。
- **共享 12 格子机池（71 / 77 / 116）**：本卡满配最多占 4 格，余 8 格让给别人（`youkai.c:169`–`youkai.c:171`）。
- **共享 PIV 池**：本卡只用 `ce_piv_pool()` 做显示（`youkai.c:6517`），不直接进分；见 `MAXPOINT-AUDIT.md` §4。

**生命周期**：
- 钩子覆盖：`.ctor`（`youkai.c:6688`）/ `.on_load`（`youkai.c:6957`，**无条件转调** `on_ctor`，`youkai.c:6966`）/ `.on_run_reset`（`youkai.c:6969`）/ `.on_delete`（`youkai.c:7006`，虚表 `+0x50`，商店删卡唯一跑得到的收尾点）；**没有 `on_stage_start`**。
- `on_delete` 收的东西：`yk_drop_all`（`youkai.c:7029`）、`yk_ring_kill_all`（`7030`）、`yk_popup_kill_all`（`7031`）、决死染色还原（`7034`–`7037`）、`yk_orb_kill_all`（`7039`）、`yk_gauge_hud_kill`（`7040`）、`yk_option_release`（`7041`）；子机声明层的退出由 SDK 的 `+0x4c` 桩自动做（`youkai.c:7001`）。
- 已知缺口：`on_delete` **不调** `on_ctor`；本文件有 207 个文件级 static、约 40 组可能跨局残留（`LIFECYCLE-AUDIT.md:53`）。
- 已知违反：`on_load` 无条件 `on_ctor` ⇒ 夜点 / 人妖槽 / 逢魔 / 刻符 / 子机 / 使魔**每关复位**，是 12 张系统卡里「换面保持」的唯一实锤违反（`LIFECYCLE-AUDIT.md:104`、`:121`、`:132`）；代码自认在 `youkai.c:6788`–`youkai.c:6791`，并登记 `TUNING.md`。

**已知未测 / 风险**：
- 使魔皮肤两个偏移 `enemy+0x1490` / `enemy+0x149c` 全反汇编零内存引用，且 `0x149c` 与 ECL 局部变量 I1 撞址 ⇒ 语义未证实；当前是死代码占位（`youkai.c:99`–`youkai.c:116`）。
- `abs_pos` 双写（每帧同时钉 `enemy+0x1270`/`+0x1274` 与 `+0x12b4`/`+0x12b8`）：哪一格才是可见位置源头没有一手证据，🟡 待实跑（`youkai.c:24`–`youkai.c:28`、`youkai.c:1392` 起）。
- 使魔跟随 VM 的层号 7 与「VM 位置 = 实体 final_pos」只做过代码层对账，未实跑（占位清单 `youkai.c:9758`–`youkai.c:9762`）。
- 绕转使魔的台数阈值表与 HP 缩放旋钮是 ❓自定（`youkai_core.h:2420`–`youkai_core.h:2512`）。
- 发射口闸两条自定时间参数未实跑：600 帧 TTL 与波次间隔 24 帧 / 3×（`youkai_core.h:1046`、`youkai_core.h:1072`、`youkai_core.h:1087`）。
- 时符抑制转嫁的名字闸（符卡名以「時符/时符」开头）在 TH18 的 `*.ecl` 里 0 命中 ⇒ 该路径从未触发，判据未验（`youkai.c:3748`–`youkai.c:3754`，台账登记 `AUDIT.md` §BE #212）。
- 伤害转嫁现行 100% 属 owner 定案（非一手）；一手 1/2 的旋钮保留即可切回（`youkai_core.h:1467`、`youkai_core.h:1456`）。
- 映射使魔 life = 100 是 TH08 `familiarCreate` 的**众数**、不是逐站点一手值（`youkai_core.h:989`–`youkai_core.h:999`）；人妖槽四档判定的玩法后果未接（`youkai.c` 文件尾清单）。
