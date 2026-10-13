> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
### 117 — 东方鬼形兽（CARD-CODE）

> 只读文档；代码基准 `mods/th18.v1.00a/card-expand/native/`。行为表 / 成本 / 数值 / 跨卡 / 未测的完整台账见 `CARD-BEHAVIOR-AUDIT.md` §117（本文只引用，不重抄）。

**一句话**：TH17 动物灵/咆哮的 TH18 卡侧复刻——敌人掉兽灵，收满 5 格自动起咆哮（就绪窗口），按 X 用成 40 帧霊撃，中弹时可用咆哮抵命（同台账 §117）。

**实现链路**：
1. 注册：`CE_CARD(117, …)`（`native/sdk.h:918`）落到 `cards/animal.c:4216-4242`；无 `.ctor`、无 `.active_recharge`/`.on_activate`（`cards/animal.c:4217-4223` 注释）；卡面 / `price_tier` 在 `patch/th18/cards.js:532-549`。
2. 杂兵掉落：`on_enemy_death`（`cards/animal.c:3671`）→ 判据 `animal_enemy_drop_measure`（`cards/animal.c:1398`，贴图 ≥64×64 或同波同类 ≤3）→ 入队 `animal_big_drop_push`（`cards/animal.c:1663`）；帧末 `animal_big_drop_drain`（`cards/animal.c:1677`）建 VM + 打掉落行。
3. Boss / 相位：`on_enemy_drop_pre`（`cards/animal.c:3775`，`dropItems` 事件）、`animal_phase_track`（`cards/animal.c:3584`）、`animal_drop_midboss_death`（`cards/animal.c:3538`）。
4. 兽灵实体：`token_spawn`（`cards/animal.c:1656`）→ `token_make_vm`（`cards/animal.c:1594`）；每帧 `tokens_tick`（`cards/animal.c:3375`）走 24 px 收集、60 px 减速、变色、离屏、DVD 弹跳。
5. 满槽 → 就绪：`token_collect`（`cards/animal.c:3353`）→ `animal_collect`（`cards/animal_core.c`）；rc==0 且 ready ⇒ `on_ready`（`cards/animal.c:3113`）建光环 / 就绪条 / SE。
6. 咆哮期效果：`on_tick_2`（`cards/animal.c:3946`）每帧 `hyper_shot_apply`（`cards/animal.c:2425`）、獭 `orb_tick`+`orb_cancel`（`cards/animal.c:3057`/`:3089`）、鹫 `eagle_beam_tick`（`cards/animal.c:2675`）/ 咲夜 `sakuya_eagle_tick`（`cards/animal.c:2898`）。
7. X 用掉：`on_tick_2`（`cards/animal.c:3983`，输入掩码 bit1 上升沿）→ `hyper_begin`（`cards/animal.c:3159`，60 帧无敌 + 脚本 `0x21`）→ 每帧 `hyper_frame`（`cards/animal.c:3195`，半径消弹 + 40 伤害源）→ `hyper_end`（`cards/animal.c:3265`）→ `release_spawn`/`release_tick`（`cards/animal.c:3218`/`:3231`，脚本 160，120 帧后爆散 2 只）。
8. 抵命：`on_death_before_deathbomb`（`cards/animal.c:3828`）→ `ce_rescue_note_claim(117)`（`cards/animal.c:3844`）+ `hyper_begin(s,1)` → `cancel_impending_death`（`cards/animal.c:699`）；真死 `on_death_frame2`（`cards/animal.c:3850`）掉 2 只。
9. HUD：`on_load`（`cards/animal.c:4188`）→ `hud_slot_claim`（`cards/animal.c:2006`）；每帧 `on_draw`（`cards/animal.c:4097`）→ `hud_sync`（`cards/animal.c:2129`）；压暗 `hud_fade_tick`（`cards/animal.c:2103`）。

**用到的 SDK**（定义位置 = `native/` 下 `sdk.h`/`sdk_core.h`/`sdk/public.h`；`sdk_actions.h` 的：无）：
| API | 用途 / 定义 |
| --- | --- |
| `ce_state` | 取卡私有状态块（`sdk/public.h:20`）；`ce_card_id`（`sdk.h:21`）读卡 id |
| `ce_hud_slot_claim_pinned_card` | 认领 pinned HUD 槽（`sdk.h:1334`） |
| `ce_hud_slot_get` / `ce_hud_slot_release` / `ce_hud_stage_begin` | 读 / 释放槽位、推关卡纪元（`sdk_core.h:849`/`:850`/`:793`） |
| `ce_hud_band_rect` / `ce_hud_block_t` / `ce_hud_slot_t` | HUD 空带矩形 + 类型（`sdk_core.h:825`/`:709`/`:733`） |
| `ce_layer_drop_card` | 删卡收口，自动撤 flag / 槽位（`sdk.h:1096`） |
| `ce_card_flag_set` | 置/撤跨卡只读标志 `CE_CARD_FLAG_ROAR`（`sdk_core.h:1018`） |
| `ce_cancel_latch_claim` | 同帧消弹闩锁认领（`sdk_core.h:483`）；`_release`（`:485`）、`_owner`（`:484`） |
| `ce_rescue_note_claim` | 记「这次死亡由 117 救了」（`sdk_core.h:503`） |
| `ce_dup_publish` | Jera 复制上报可见数（`sdk.h:1085`）；`ce_dup_should_run`（`sdk_core.h:1049`）响应 |
| `ce_sweep_should_run` | 92 清扫响应，token→金钱（`sdk_core.h:1076`） |
| `ce_cancel_radius` | 霊撃圆消弹（`sdk.h:150`）；`ce_damage_rect`（`sdk.h:172`）矩形伤害源；`ce_raise_damage_cap`（`sdk.h:194`）伤害上限 |
| `ce_anm_spawn` / `ce_anm_spawn_ui` | 建 VM（`sdk.h:86`/`sdk.h:248`） |
| `ce_anm_delete` / `ce_anm_get_vm` / `ce_anm_interrupt` | 删 VM / 取 VM / 发 interrupt（`sdk.h:393`/`:240`/`:391`） |
| `ce_anm_set_pos`/`_set_sprite`/`_set_scale`/`_set_color`/`_set_rotation` | VM 驱动（`sdk.h:265`/`:257`/`:336`/`:273`/`:287`） |
| `ce_player_pos` / `ce_player_x` | 自机坐标（`sdk.h:486`/`:492`） |
| `ce_enemy_pos` / `ce_enemy_by_id` / `ce_enemy_lockable` / `ce_spell_active` | 敌机读取 / 锁定 / 符卡判据（`sdk.h:569`/`:619`/`:584`/`:605`） |
| `ce_spawn_item`/`ce_money_pocket` | 鱼灵换道具 / 进金钱（`sdk.h:413`/`sdk.h:883`） |
| `ce_play_sound`/`ce_rand`/`ce_polar_to_vel`/`ce_log` | 音效 / RNG / 极坐标 / 日志（`sdk.h:50`/`:464`/`:449`，`sdk/public.h:268`） |
| `ce_key_down` | 调试键（`sdk.h:841`，仅 `ANIMAL_DEBUG_KEY`） |
| `ce_sdk_c_press`/`ce_sdk_active_tick`/`ce_sdk_active_reset` | 主动卡 SDK（`sdk.h:855-857`）；本卡未声明 `active_recharge` ⇒ 恒返回 0；契约 `ce_sdk_contract_check`（`sdk_core.h:53`） |
| `ce_kill_retail_option_lasers` | 咆哮期清原版子机激光（`sdk.h:673`） |

**数值**：
| 值 | 单位（显示/存储/帧） | 站点 | 一手出处 |
| --- | --- | --- | --- |
| 5 | 格（槽上限） | `cards/animal_core.h:24` | TH17 `TOKENS[5]` |
| 840 / 1140 / 1500 | 帧（就绪时长，同类 3/4/5） | `cards/animal_core.c:17-19` | 一手表 `0x4a13cc`；代码注释引 `0x4a13d0`/`d4`/`d8` |
| 480 | 帧（凭依加成） | `cards/animal_core.h:37` | `th17:0x40fbe5`；本卡恒 NONE |
| 40 | 帧（霊撃本体） | `cards/animal_core.h:36` | `th17:0x40f6d0` |
| (1−f⁴)×200 | px（霊撃半径） | `cards/animal_core.c:178` | `th17:0x40f61a` |
| 90 / 60 / 42… | 帧（咆哮中收灵延长，递减） | `cards/animal_core.c:99` | 用户口径；TH17 为 120/80/53 🟡 |
| 24 / 60 | px（收集半径 / 减速半径） | `cards/animal.c:3402`/`cards/animal.c:3406` | TH17 `0x410c8c` 段 |
| 60 / 120 | 帧（霊撃无敌 / 释放动画） | `cards/animal.c:3164`/`cards/animal.c:91` | 一手 `do_hyper_cancel` |

**跨卡协同**：
- 107 / 111：同帧消弹闩锁 `ce_cancel_latch_*`，霊撃半径圈先认领、被占则让位（`cards/animal.c:3208`；`EVENTS-CONTRACT.md` §5.2）。
- 113 灵界：救援层顺序 107 → 117 → 决死 B → 113，`ce_rescue_note_claim(117)` 无条件（`cards/animal.c:3844`）；本卡 `+0x0c` 的 `acc ≠ 0` 时不按 miss 记账（`cards/signal.c:3237`）。
- 108 妖怪：`CE_CARD_FLAG_ROAR` 只在狼(1)/鷲(3)咆哮置位（`cards/animal.c:3968-3970`）；108 读它钉侧别（`cards/youkai.c:413-414`，`sdk_core.h:1011`）。
- 113 主炮组：`shot_set_index_priority = CE_SHT_PRIO_117`(20) > 113(10)（`sdk/public.h:257-258`；113 注册 `cards/trance.c:4956`、口径注释 `:4951`）；108/111 不换主炮，不参与。
- 92 清扫 / Jera：`ce_sweep_should_run` token→金钱（`cards/animal.c:3955`）、`ce_dup_*` 复制（`cards/animal.c:3953`）。
- 共享件：HUD 压暗 `hud_dim.{h,c}`（`cards/animal.c:64`/`cards/animal.c:2103`）；共享 PIV 池 init 10000×5 / cap 500000×5（`MAXPOINT-AUDIT.md:26`）；VM 直写普查中本卡为 0（`VM-IDIOM-MAP.md:25`）。

**生命周期**：钩子覆盖 = `on_load`（`cards/animal.c:4188`，幂等认领槽位，不碰状态）、`on_stage_start`（`cards/animal.c:4204`，`clear_card(c,1)` 清令牌/VM/队列，保留槽）、`on_run_reset`（`cards/animal.c:4213`）与 `on_delete`（`cards/animal.c:4214`，均 `clear_card(c,0)`）、`on_draw`（`cards/animal.c:4097`）；**无 `.ctor`**。缺口：`clear_card` 是唯一清理入口（30 个 file-static，`LIFECYCLE-AUDIT.md:69`）；「换面清令牌」一条待 owner 裁定（`LIFECYCLE-AUDIT.md:61`、`:130`）。

**已知未测 / 风险**：
- HUD 排位 (88,924) 与可见整排 (64,928) 差 24 px，代码内两说、未拍板（`cards/animal.c:141-179`，AUDIT §DD-8）。
- 稀有动物灵 8..14 只打一行日志、不发（`beast2*` 资产缺，`cards/animal.c:3563` / TUNING §2.3）。
- 咲夜 / 早苗的兽灵咆哮全为 ❓ 自定（TH17 无对应行，`cards/animal.c:360-399` / TUNING §2.1b）。
- 调试键 `ANIMAL_DEBUG_KEY=0`（`cards/animal.c:242`）：空格刷灵 / 强制就绪路径无发布版实跑证据。
- 咆哮中收灵延长 90/60/42 与 TH17 一手 120/80/53 两说未定（`CARD-BEHAVIOR-AUDIT.md` §117）。
