> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
# 113 — 东方神灵庙（CARD-CODE）

> 版本：TH18 v1.00a（mod 侧 `th18:0x400000`）；一手出处一律带 `thNN:` 前缀，地址一律加反引号。
> 本文只写**代码端**；行为表 / 成本 / 完整数值 / 未测点见 [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) §113（本文直接引用，不重抄）。

**一句话**：收神灵充灵界槽；槽满按 C 开灵界（强化机体/主炮）；中弹时取消这次死亡先进灵界，窗口结束再补死。（同台账 §113 ✓）

**实现链路**：

1. 注册：`trance.c:4925` 的 `CE_CARD(113, …)` 挂 `.ctor`(`:4927`) / `.on_load`(`:4929`) / `.on_item_frame`(`:4930`) / `.on_item_collect`(`:4931`) / `.on_death_after_deathbomb`(`:4932`) / `.on_death_frame2`(`:4933`) / `.on_enemy_death`(`:4934`) / `.on_enemy_damage`(`:4941`) / `.on_enemy_drop_pre`(`:4945`) / `.on_tick_2`(`:4946`) / `.on_shot_set_index`(`:4954`) / `.on_option_shot_gate`(`:4956`) / `.on_stage_start`(`:4958`) / `.on_draw`(`:4959`) / `.on_run_reset`(`:4960`) / `.on_delete`(`:4961`)。
2. 帧点入口 `on_item_frame`(`trance.c:2944`)：共享 PIV / 擦弹族声明(`:2950`、`:2951`) → 关卡号检测(`:2968`，新局写一格 / 跨面保留) → `trance_tick`(`:2991`，实现 `trance_core.c:240`) → 跨卡标志 `CE_CARD_FLAG_TRANCE`(`:2998`) → 对话强制退出(`:3007` → `trance_end` core `:210`)。
3. 手动开灵界 `on_activate`(`:2401`)：`trance_fill_cond`(`:2408`，定义 `:956`) → `trance_can_activate`(`:2409`，core `:153`) → `trance_begin`(`:2414`，core `:177`) → 当场 `ce_no_hit_declare(113,1)`(`:2418`) → `ce_play_sound(0x34)`(`:2422`)。
4. 中弹救援 `on_death_after_deathbomb`(`:2583`)：`acc` 门(`:2593`) → 原生让位 `trance_native_rescue_would_fire`(`:2603`，定义 `:2555`) → owner 门(`:2620`) → `trance_force_on_death`(`:2629`，core `:320`) → `ce_rescue_note_claim`(`:2638`) → `Player__cancel_impending_death`(`:2639`，定义 `:2511`，`th18:0x45d590`) → `return 1`(`:2653`)。
5. 补死（延时非免死）：帧点挂账 `redie_pending`(`:3046`) → 玩家帧 `on_tick_2`(`:2309`) → `trance_player_die`(`:2520`)：先撤闸门(`:2530`) → `ce_forced_death_begin/end`(`:2535`/`:2537`) → `Player__die`(`:2536`，`th18:0x45d3a0`)。
6. 掉灵三条通道：击破 `on_enemy_death`(`:4655`) 按敌人 (T,N,alive) 算颗数(`:4688`) → `trance_kill_note`(core `:605`)；boss 伤害 `on_enemy_damage`(`:4851`)；boss 相位批次 `on_enemy_drop_pre`(`:4733`，只读 `counts[]`)。
7. 撒实体：帧末 `trance_kill_take`(`:3562`) → `trance_spirit_spawn`(`:3570`，定义 `:1697`)；相位批次 `trance_phase_burst_tick`(`:2920`/`:3638`)；通道 E 白灵(`:3609`，源 `on_death_frame2:3754`)。
8. 收实体：`trance_spirits_tick`(`:2016`/`:3643`) 距离判定(`:2111`) → `trance_on_spirit_collected`(`:3807`) → `trance_collect`(core `:429`，槽 +2) → 奖励 `ce_piv_add`(`:3836`)。
9. 落笔层：`trance_declare_score`(`:986`/`:3647`，仅本地账 + 声明)、HUD `trance_hud_tick`(`:3980`/`:3689`)、主炮 `on_shot_set_index`(`:2877`)。

**用到的 SDK**（`ce_*` 调用清单；定义位置 = 头文件行）：

- `ce_log`(`sdk/public.h:268`) — 全卡日志；`ce_refuse`(`sdk.h:633`) — 主动发动失败回执。
- `ce_state`(`sdk/public.h:20`)/`ce_state_user`(`sdk_core.h:64`) — 取卡私有状态；`ce_card_id`(`sdk.h:21`) — 读卡 id。
- `ce_anm_spawn`(`sdk.h:86`)/`ce_anm_spawn_ui`(`:248`)/`ce_anm_delete`(`:393`)/`ce_anm_get_vm`(`:240`) — 起 / 删 / 查神灵、HUD、幕布 VM。
- `ce_anm_set_pos`(`sdk.h:265`)/`ce_anm_set_color`(`:273`)/`ce_anm_set_color2`(`:297`)/`ce_anm_set_scale`(`:336`)/`ce_anm_set_rotation`(`:287`)/`ce_anm_set_sprite`(`:257`)/`ce_anm_interrupt`(`:391`) — 外观与消失段。
- `ce_hud_slot_get`(`sdk_core.h:849`)/`ce_hud_slot_claimed`(`:851`)/`ce_hud_slot_claim_pinned_card`(`sdk.h:1334`)/`ce_hud_vm_text_clear`(`sdk.h:823`)/`ce_hud_vm_text_set_font`(`sdk.h:820`)/`ce_hud_band_dim_alpha`(`sdk_core.h:829`)/`ce_hud_vm_probe`(`sdk.h:1233`) — 左下角指示槽 HUD。
- `ce_no_hit_declare`(`sdk_core.h:536`)/`ce_invuln_request`(`:95`)/`ce_rescue_note_owner`(`:504`)/`ce_rescue_note_claim`(`:503`)/`ce_forced_death_begin`(`:518`)/`ce_forced_death_end`(`:519`)/`ce_card_flag_set`(`:1018`) — 共享声明层（无伤 / 无敌 / 救援 / 跨卡标志）。
- `ce_piv_claim_auto`(`sdk_core.h:426`)/`ce_piv_add`(`:429`)/`ce_graze_family_claim`(`:439`)/`ce_score_declare_delta`(`:386`) — 共享 PIV 池与计分层。
- `ce_set_bullet_damage_permille`(`sdk/public.h:263`)/`ce_add_life_fragment`(`sdk.h:101`)/`ce_add_bomb_fragment`(`sdk.h:112`) — 伤害层与碎片奖励。
- `ce_play_sound`(`sdk.h:50`)/`ce_player_pos`(`:486`)/`ce_player_x`(`:492`)/`ce_enemy_pos`(`:569`)/`ce_rand`(`:464`)/`ce_key_down`(`:841`)/`ce_difficulty`(`:1076`)/`ce_sdk_frame`(`sdk_core.h:618`)/`ce_dup_publish`(`sdk.h:1085`)/`ce_dup_should_run`(`sdk_core.h:1049`) — 音效、坐标、随机、调试键、帧号、复制发布。

**数值**（关键值；完整表见台账 §113）：

| 值 | 单位 | 站点 | 一手出处 |
| :-: | --- | --- | --- |
| 灵界槽上限 600（3 格） | 点（存储） | `trance_core.h:95` | `th13:0x405815` |
| 每颗神灵入槽 +2（type3 再 +1） | 点 | `trance_core.h:98-99` | `th13:0x438b3d` / `th13:0x438c3d` |
| 灵界中扣槽 1 | 点/帧 | `trance_core.h:100` | `th13:0x43d4f0` |
| 手动结束再扣 200 | 点 | `trance_core.h:101` | `th13:0x405cdd` |
| 进灵界硬门 200 | 点 | `trance_core.h:102` | `th13:0x405a40` |
| 进关槽初值 200（一格） | 点 | `trance_core.h:113` / `trance.c:2977` | `FUN_0042d3c0`（调用点 `th13:0x42bb6b`） |
| 点道具入共享 PIV +100 | 点（显示） | `trance.c:622` / `:3804` | `th13:0x42e8ca`（争议见 `MAXPOINT-AUDIT.md` §▲1） |
| 击破兜底档 (T,N) = (60,1) / (120,4) | 帧/颗 | `trance_core.h:642-645` | ❓自定（档位取 TH13 普查） |

**跨卡协同**：

- **救援层 107 → 117 → 决死 B → 113**：113 落卡虚表 `+0x0c`，三道让位门站点 `trance.c:2593`/`:2603`/`:2620`；原生 23/35 判据在 `trance.c:2555`。
- **111 河童罩子**：补死时用 `ce_forced_death_begin/end`(`trance.c:2535-2537`) 通报「卡自己造的死」，罩子让位。
- **117 咆哮主炮**：`CE_SHT_PRIO_113` = 10 < `CE_SHT_PRIO_117` = 20（`sdk/public.h:257`，站点 `trance.c:4955`）。
- **108 永夜抄**：113 在 `trance.c:2998` 声明 `CE_CARD_FLAG_TRANCE`（`sdk_core.h:1011`，见 `HANDOFF-108-CROSSCARD-SHIFT.md` §2.2）。
- **共享 PIV 池 / 计分层**：`ce_piv_claim_auto`(`trance.c:2950`)、`ce_piv_add`(`:3804`/`:3836`)，与 110/111/112/114 同池（见 `MAXPOINT-AUDIT.md`）。
- **boss 相位断点 `th18:0x430510`**：与 110/111/116/117 共用；113 只读 `counts[]`、不写 116 的闩锁(`trance.c:4733`)。

**生命周期**（详见 `LIFECYCLE-AUDIT.md` 行 113）：

- 钩子覆盖：ctor(`trance.c:4480`) 只清私有状态、**刻意不清神灵池**；on_load(`:4499`) 只打自检日志；on_stage_start(`:4572`) 先存槽 → `trance_clear` → 写回；on_run_reset(`:4617`) 与 on_delete(`:4628`) 都走 `trance_clear`。
- `trance_clear`(`:4386`) 收：声明层(`:4407`/`:4408`/`:4412`)、灵界形态(`:4420`)、HUD / 文字 / BGM / 背景 / 幕布 / 弹窗 / `trance_spirit_clear_all`(`:4470`)；删卡发生在商店、tick 全停，只有这条路收得掉。
- 已知缺口：`max_point_bonus` 只靠「关卡号变小」判新局(`:2970-2976`) ⇒ **同一关 retry 可能不复位**（`LIFECYCLE-AUDIT.md` §2 行 113 ✗）；无 on_delete-only 项 ✓。

**已知未测 / 风险**：

- 「从帧点调 `Player__die` 无额外时序副作用」= 代码自评 🟡 推断（`trance.c:236`）。
- 通道 E 头注 `:171` 写「仍未接线」与实现（`:3775` 置 `death_echo`、`:3609` 消费）矛盾，需实跑对账。
- st02 终符用 `0x11` 卡道具、与普通相位同型，只靠「`+60` 帧 > 符卡门 3 帧」挡（`:396-397`）🟡。
- `MAXPOINT-AUDIT.md` §▲1：`:3803`/`:3804` 挂在 type 2 蓝点上却引满 P 地址 `th13:0x42e8ca`，与 114/112 不同构，待裁决。
- VM 直写：`:4005-4006` 写 `entity_pos`、`:3974-3977` 写裁切三元组（`VM-IDIOM-MAP.md` §I4/I5）。
- 神灵 480 帧寿命 + 脚本尾段、池 128 槽等手感值见 `TUNING.md`，未实跑验收。
