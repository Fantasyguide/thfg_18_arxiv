> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
### 116 — 东方天空璋（CARD-CODE）

> 本卡代码 = `mods/th18.v1.00a/card-expand/native/cards/season.c`（2082 行）+ `season_core.{h,c}`（纯逻辑/表）。
> 一句话 / 行为表 / 成本 / 数值 / 跨卡 / 未测 = **见 `CARD-BEHAVIOR-AUDIT.md` §116**，本文只补「代码怎么走」。
> 一手档 = [`AUDIT-116.md`](mods/th18.v1.00a/card-expand/AUDIT-116.md)（§3–§5 复刻表、§7 未定、§8 自检）。

**一句话**：TH16 季节系统复刻——擦弹/击破/解放清弹产出「季节点」，累计成 0..6 档，档 = 季节子机数，按 C 发动季节解放（清弹 + 伤害 + 无敌），双击 Z 切副季节，左下角画季节槽（同台账 §116）。

**实现链路**（`season.c` 内所有站点；编号 = 执行次序，不是代码行序）：

1. **注册**：`CE_CARD(116, …)` 挂 9 个回调（`season.c:2078`）：`on_activate`/`on_active_tick`/`on_power_level_change`/`on_tick_shooters`/`on_tick_2`/`on_enemy_drop_pre`/`on_draw`/`on_load`/`on_run_reset`/`on_delete`，外加 `.active_recharge = 1`（主动卡入场券）。状态块 `season_card_t` = `ce_state`（`sdk/public.h:20`），`_Static_assert(≤496)` 在 `season.c:380`。
2. **每关开场**：`on_load()`（`:2066`）只调 `hud_slot_claim(s)`（`:1640`）→ `ce_hud_slot_claim_pinned_card(116,…)`（`sdk.h:1334`），结果写回卡字段 `s->hud_slot_ok`（`:1675`）。**不清任何 run 级数据**——注释 `:2061` 明写「`on_load` 不是开局一次，每关结束的商店都会跑」。
3. **每帧主趟**：`on_tick_2`（`:1493`）依次 `ce_dup_publish`（`:1501`，杰拉代数闸）→ `graze_tick`（`:1504`）→ `ce_sweep_should_run`/`season_sweep_to_money`（`:1505`，92 清扫）→ `points_tick`（`:1506`）→ `burst_tick`（`:1507`）→ `season_dtap_tick`（`:1530`，双击 Z 切季）→ `release_tick`（`:1545`）→ 子机槽补申请/回收循环（`:1551-1566`）→ 每 60 帧只报异常的子机残留快照（`:1573-1599`）。
4. **产出①擦弹**：`graze_tick`（`:1469`）读原生擦弹计数 `0x4ccd0c`（`:1467`）取本帧 Δ，`graze_pick`（`:1425`）按引擎擦弹判据只读扫弹池挑「刚跨进圈」那颗，`spawn_point_graze`（`:678`）在**弹的位置**以 `atan2(弹−自机)`、速 1.9 撒点。
5. **产出②击破**：`on_enemy_drop_pre`（`:1058`）用「引擎自己的计数表已清零」（`:1081-1093`）+ `enemy+0x6838` 闩锁去重，boss 恒 200 / 小怪按 `mid_drop_count`（`:1032`）计时衰减，`spawn_point_burst`（`:662`）随机角 0.2..2.1 撒；池满进 `burst_left` 队列（`:1120-1124`）。
6. **产出③解放清弹**：`cancel_spawn_points`（`:707`）**先扫后消**——按引擎命中判据（`state∈{1,2}` ∧ `bullet+0x24==0` ∧ `dist²≤(弹半径/2+R)²`）在**每颗弹的位置**撒点（角 `−π/2±10°`、速 2.2），随后 `ce_cancel_radius(pos, R, 9999, 4)`（`:1389`）交给引擎消弹。
7. **点的运动/回收**：`points_tick`（`:805`）state0 待发射错帧 → state2 直线减速（`r−=0.03`）→ state1 飘落（重力 0.03 / 上限 2.0）→ state3/4 磁吸（初速 = 磁速/3、每帧 +0.2 至 12.0）；全部门/半径/磁速**读自机字段** `+0x47988..0x47998`（`:830-839`）。
8. **收点 → 升档 → 弹字**：`point_collect`（`:774`）`ce_anm_interrupt(id,1)` + `ce_add_score_delta(10)`；跨档时放 se `0x3f`、`alloc_options`、再 `popup_level_up`（`:767`）调**原生** `PopupManager__sub_4645f0`（`0x4645f0`，stdcall）。
9. **档 → 子机**：`alloc_options`（`:544`）按 `opts_for(level_of(s))` 申请/回收，`alloc_one`（`:466`）走 `ce_option_alloc`（脚本号按副季节）+ `set_option_offsets`（`:397`，TH16 副 sht 阵位）+ `set_option_art`（`:457`，TH16 原版切图）。`on_power_level_change`（`:568`）同函数。
10. **主动卡发动**：`on_activate`（`:1291`）三道门（无状态/档 0/正在解放 ⇒ `ce_refuse`），过门后 `release`（`:1308`）：查 `season_profile` 表 → 建两圈特效 `ring_spawn`（`:1144`）→ 写自机无敌计时器（`:1333-1338`）→ 冬置弹伤 1500（`:1340`）→ 扣槽 `season_points_after_release`（`:1344`）→ `alloc_options`（`:1350`）。释放期 `release_tick`（`:1359`）每帧 `ring_tick`（`:1174`）+ `afterimage_tick`（`:1246`，秋/冬残像）+ 步骤 6 + `ce_damage_rect`（`:1393`）+ 秋移速 `*1.5`（`:1398`）。
11. **子机开火**：`on_tick_shooters`（`:1613`）按副季节取 sht set，每颗子机 `ce_tick_shooters_for_card(o, frame, 0, set)`。
12. **HUD**：`on_draw`（`:1813`）两道门（无游戏线程/无自机 ⇒ `hud_hide`）→ 每帧 `hud_slot_sync`（`:1677`）取最新锚点 → `ce_hud_band_rect` 算压暗矩形（`:1838-1843`，带为空兜底回本块）→ `hud_ensure`（`:1703`）生成 4 条外框 + 3 条 + 季节字 + `ce_hud_vm_text_*` 档位数字，状态变了才重发 interrupt。副季节字/贴图每帧按 `subseason` 重取。
13. **收尾**：`clear_card`（`:2033`）收子机 → `point_clear_all` → `ring_hide` → `hud_hide` → `ce_hud_vm_text_clear` → `ce_hud_slot_release(116)` → `memset` → `hud_dim_init` + `season_dtap_init`。

**用到的 SDK**（每项一句用途 + 定义位置；`sdk.h` = `native/sdk.h`，其余同目录）：

- `ce_state`（`sdk/public.h:20`）取卡状态块；`ce_card_id`（`sdk.h:21`）本卡 id。
- `ce_anm_spawn`（`sdk.h:86`）/`ce_anm_spawn_ui`（`:248`）建世界层/UI 层 anm VM；`ce_anm_get_vm`（`:240`）判 VM 是否还在（漏删检测）。
- `ce_anm_set_pos`（`sdk.h:265`）/`set_sprite`（`:257`）/`set_scale`（`:336`）/`set_rotation`（`:287`）/`set_color`（`:273`）/`set_color2`（`:297`）每帧推点位/贴图/尺寸/自转/双色；`ce_anm_interrupt`（`:391`）发脚本 label；`ce_anm_delete`（`:393`）删 VM。
- `ce_option_alloc`（`sdk.h:649`）/`ce_option_claim`（`:648`）/`ce_option_retry`（`:650`）/`ce_option_drop`（`:654`）子机槽的生命周期。
- `ce_cancel_radius`（`sdk.h:150`）半径消弹（mode 4 = 引擎顺带撒钱）；`ce_damage_rect`（`:172`）释放伤害源；`ce_raise_damage_cap`（`:194`）抬每敌每帧上限。
- `ce_set_bullet_damage_permille`（`sdk/public.h:263`，实现在 `sdk.c:428`）冬的弹伤 ×1.5 层。
- `ce_polar_to_vel`（`sdk.h:449`）/`ce_rand`（`:464`）/`ce_player_pos`（`:486`）/`ce_player_x`（`:492`）/`ce_add_score_delta`（`:455`）运动学、随机、取位、加分。
- `ce_spawn_item`（`sdk.h:413`）/`ce_money_pocket`（`:883`）92 清扫把季节点换金钱；`ce_dup_publish`（`:1085`）与 `ce_dup_should_run`（`sdk_core.h:1049`）、`ce_sweep_should_run`（`sdk_core.h:1076`）两个跨卡闸。
- `ce_hud_slot_claim_pinned_card`（`sdk.h:1334`）/`ce_hud_slot_get`（`sdk_core.h:849`）/`ce_hud_slot_release`（`:850`）/`ce_hud_slot_claimed`（`:851`）/`ce_hud_band_rect`（`sdk_core.h:825`）左下角「一块 HUD」的认领与取坐标。
- `ce_hud_vm_text_set`（`sdk.h:798`）/`ce_hud_vm_text_clear`（`:823`）/`ce_hud_text_ex`（`:851`）档位数字（ASCII，两组号说明见 `:1980-1990`）；`hud_dim_tick`/`hud_dim_init`（`cards/hud_dim.h:184`/`:170`）跨卡压暗。
- `ce_refuse`（`sdk.h:633`）主动卡退充能；`ce_play_sound`（`:50`）四个 se（`0x25/0x2c/0x3e/0x3f/0x4e`，`season.c:84-88`）；`ce_log`（`sdk/public.h:268`）。

**数值**（值 / 单位（显示 | 存储 | 帧）/ 站点 / 一手出处；超 8 行见台账 §116）：

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| 档位阈值 | 100/230/390/590/840/1140 | 季节点（显示） | `season_core.c:5` | `th16:0x43deb0` 一手 |
| 季节点池上限 | 512 | 个 | `season.c:117`（`SEASON_POINT_POOL`） | ❓自定（TH16 池 4696 槽）|
| 擦弹点初速 / 消弹点 | 1.9 / 角 `−π/2±10°`、速 2.2 | px每帧 / rad+px每帧 | `season.c:695`, `:728-729` | `th16:0x444e03`、`th16:0x416bbb` 一手 |
| 飘落重力 / 上限 / 磁吸 | 0.03 / 2.0 / 每帧 +0.2 至 12.0 | px每帧（状态量） | `season.c:120-124` | `th16:0x42f4e0` 一手 |
| 双击窗口 | 8 | 帧 | `season_core.h:100` | ❓自定（照 TH11 `th11:0x430e50`）|
| HUD 块 / PIN | 134×36 @ (68,924) | 表面 px | `season.c:177`/`:180`/`:229-230` | ❓自定（owner 口径，`AUDIT-116` §10.4）|
| 释放表（时长/半径/每帧伤） | 春 10..20 帧/60..300/100 … | 帧 / px / 点 | `season_core.c:10-28` | `.rdata` `th16:0x491e70`..`th16:0x491fd4` 一手 |
| 无敌帧 | 春 = 时长+60、夏 9、秋 30、冬 30、土用 10 | 帧 | `season_core.c:32`（`k_invuln`）| 🟡 攻略口径，非一手 |
| PIV（额外最大得点） | init 10000 / cap 500000（难度 E..EX）| 显示点 | 本卡**零** `ce_piv_*` 调用；表在 `sdk_core.c:874`/`:882` | 见 `MAXPOINT-AUDIT.md:27`（0 差）|

**跨卡协同**（站点 = `season.c`）：

- **88 杰拉**：`ce_dup_publish`（`:1501`）上报场上点数，`ce_dup_should_run`（`:1502`）为真时**照原档同角度/同初速**再撒一颗（`:1503-1505`）。
- **92 伟大之作**：`ce_sweep_should_run`（`:1505`）→ `season_sweep_to_money`（`:594`）倒序把每颗季节点在原位换 1 枚金钱（`ce_spawn_item(2,…)`），走 `point_remove` + 删 VM 两条收尾（`:612-613`）。
- **115 绀珠传**：引擎「道具减速因子」`CE_ITEM_SLOWDOWN`（`:823-829`）只作用于飘落态 + 染色/抖 ±1 px（`:944-967`）；收益侧跟着变慢（见 `CARD-NUMERIC-INTERACTION-AUDIT.md` §3）。
- **原生 21/22（ITEM_CATCH / ITEM_LINE）**：本卡**只读**自机 `+0x47988..0x47998`（`:830-839`），不写 ⇒ 与原生不冲突（`CARD-NATIVE-CONFLICT-AUDIT.md` §E）。
- **113 / 115 的判定点**：`graze_pick` 读 `0x479a4/a8/9c`（`:1429-1431`）⇒ 113 缩小判定点会连带缩小本卡擦弹圈（未测）。
- **共享件**：子机池 12 格与 108/71/77 共分（预算刚好 12/12，`CARD-RESOURCE-INTERACTION-AUDIT.md:141`）；移速 `*=` 与 59/62/113 可交换（同文 §182）；弹伤千分比与 61/76/78/113 连乘（同文 §73/§80）。

**生命周期**（钩子覆盖 + 已知缺口，判据见 `LIFECYCLE-AUDIT.md` §0.1）：

- 注册的钩子：`on_load`（`:2066`）/`on_run_reset`（`:2075`）/`on_delete`（`:2076`，两者都 = `clear_card`）/`on_activate`/`on_active_tick`/`on_tick_2`/`on_tick_shooters`/`on_enemy_drop_pre`/`on_draw`/`on_power_level_change`。
- **不注册 `ctor`、不声明 `on_stage_start`** ⇒ 换面不会被误清（① 满足，`LIFECYCLE-AUDIT.md` §0.1/§2.1 判「✅」）。
- retry/重进②：两条清理路（`+0x4c` 与 `+0x50`）都接 `clear_card` ✓；审计留一条「待核：`clear_card` 是否覆盖全部 run 级 static（10 个）」（同文档 §1）。
- `on_delete` 收的东西：6 颗子机、点池全部 VM、两圈特效 VM、5 个 HUD VM + 数字 VM、HUD 槽位；压暗/双击状态复位（`:2038-2051`）。✓ 铁律「被删掉会怎样」有交代。
- 缺口（审计已登记、本卡未修）：`clear_card` 的 `if (!s) return;`（`:2037`）排在 `point_clear_all` **之前** ⇒ `ce_state` 拿不到时文件级 VM/池会留守（`CARD-RESOURCE-INTERACTION-AUDIT.md` §9，同 112/115/117 形状）。
- 已知设计缺口：`s_pt`/`s_slot`/`s_slot_ok`/`s_hud_digit`/`s_stat_*`/`s_dup_seen`/`s_sweep_seen` 全在 DLL 静态区，**只能靠 `clear_card` 清**（卡对象里没有它们的账）。

**已知未测 / 风险**（台账 §116 + `AUDIT-116` §7/§11 逐条）：

- 无敌帧非一手（攻略口径），且**春「每帧刷到 50」未复刻**（`season_core.c:32`；`AUDIT-116` §7-3）。
- 升档弹字字形 = `ascii.anm` sprite299 `(80,248,48,8)`，未肉眼确认（`AUDIT-116` §7-1）。
- `cancel_spawn_points` 的命中判据按 `engine.h` 对 `0x429370` 的描述镜像，**函数体未反汇编复核**（`:718-725`）。
- TH16 的 **45 帧释放冷却未复刻**；夏 `type(3)` / 冬 `type(2)` / 秋 `type(0)+resampleMode(1)` 在 TH18 的渲染等价未实跑（`AUDIT-116` §7-4/§7-5）。
- 无敌走**直写计时器**（`:1333-1338`，raise-only），未迁 `ce_invuln_request`（`CARD-NUMERIC-INTERACTION-AUDIT.md:40/§124`）。
- 与 `sq.c`（60）同写道具四参数：本卡只读、不改；但 60 一旦用 `=` 覆盖，`:830-839` 读到的值当帧即变（`CARD-NATIVE-CONFLICT-AUDIT.md` §285）。
- 掉落去重把闩锁写在 `enemy+0x6838`（`CE_ENEMY_DDC_UNUSED`，`engine.h` 标为死字段）⇒ 别人读/写该格、或链序在前的卡改 `counts`，都会改本卡去重判断（`CARD-RESOURCE-INTERACTION-AUDIT.md:391`）。
- 子机池刚好 12/12 零余量，缺「换套/中途买卡时 `option allocated FAILED`」的实跑日志（同文档 §13）。
- HUD：墨迹右沿曾短 5px（已改 134 宽，`:177`）；`layer(24)→(25)` 真因见 `AUDIT-116` §10.3；107/108 同场仍重叠（owner 已裁「先不动」§10.4）。
- 季节点「自动回收」里 `s->rel_frames > 0` 那条门 + 出生 32 帧不吸不收是 ❓自定桥接（`:896-905`、`season.c:140`），非一手。
