> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
### 110 — 东方风神录（CARD-CODE）

**一句话**：装备卡——吃绿点养「信仰」（= 最大得点与计分基），击破指定宿主妖精撒大信仰点；炸弹库存为 0 时按 B（或主动卡 C）发动「灵击」消弹 + 伤害，决死窗口内可救命。（同台账 `CARD-BEHAVIOR-AUDIT.md` §110 ✓）

**实现链路**：
1. 卡本体 = `mof.c`；本卡只是「一具身体」，纯逻辑在 `mof_core.c` / `faith_core.c` / `faith.c` / `bomb10.c` / `faith_hud.c`（分工表 `mof.c:33-40`）。
2. 入场与账本：`CE_CARD(110,…)` 注册 12 个钩子（`mof.c:1760-1779`）→ `ctor()`（`mof.c:1632-1662`）按「关卡号变小」判新局（`mof.c:1643`）：新局 `faith_slot_init_110()`，否则 `s->slot = s_slot_keep` 跨关保留（`mof.c:1643-1651`）。
3. 注册共享层：`on_load()`（`mof.c:1618-1630`）→ `mof_ensure_registered()`（`mof.c:1586-1616`）：`bomb10_register_host()`（`:1596`）→ `faith_register()`（`:1610`）→ `faith_hud_on_load()`（`:1615`）；中盘拿卡不调 `on_load` ⇒ `on_tick_2` 每帧补调一次（`mof.c:1232`）。
4. 每帧主链 `on_tick_2()`（`mof.c:1223-1477`）：112 等价折算（`:1230`）→ 绿妖精载体 `faith_green_fairy_tick()`（`:1240`）→ 擦弹倍率（`:1253-1258`）→ 三条门 + `mof_core_faith_tick()`（`:1280-1300`，门③帧源 = `mof_stage_frames()` `mof.c:354-367`）→ miss 命数差分（`:1302-1311`）→ 符卡监视（`:1334-1340`）。
5. 灵击触发（X）：`CE_CURRENT_BOMBS()<=0` 且 X 上升沿（`mof.c:1355`）→ `mof_core_deathbomb_window()` 定形态（`:1358`）→ `bomb10_start()`（`:1361`）；随后每帧 `bomb10_tick(1)`（`:1363`，110 恒推）。
6. 灵击的卡私有四项经 `s_bomb_host` 交给共用机器（`mof.c:1171-1178`）：对话门 `mof_bomb_gate()`（`:1066-1070`）、整档扣火力 `mof_bomb_power_spend()`（`:1075-1102`）、`mof_bomb_on_start()`（`:1122-1153`）、消弹记账 `mof_bomb_on_cancel()`（`:1161-1168`）。
7. 主动卡 C 入口 `on_activate()`（`mof.c:1728-1745`）：同一窗口判据 → **同一个** `bomb10_start()`；返回 0 = 瞬发。
8. 绿点入账：`faith_pool_tick()`（`mof.c:1374`）回调 `mof_on_pickup()`（`:460-510`：`faith_slot_add` / `faith_combo_set` / `mof_bonus_add`）与带位置支 `mof_on_pickup_at()`（`:517-528`）→ `mof_popup_number()`（`:411-453`，写原生弹字环数字槽、绿 `0xff00ff00`）。
9. 计分声明：`mof_declare_score()`（`mof.c:375-389`）→ `ce_score_declare_coupled(110, Δ)`（`:382`）+ `ce_score_family_declare()`（`:387`）+ `ce_score_family10_faith()`（`:388`）；入口 = `on_item_score()`（`:939-946`）及每次数值变动。
10. 击破撒点：`on_enemy_death()`（`mof.c:679-779`）→ `mof_core_death_is_real()` 门 → `mof_is_boss()`（`:309-313`）+ `mof_enemy_anm_identity()`（`:578-582`）→ `mof_core_wants_big_host()`（`:715`）→ `faith_scatter_from_enemy()`（`:755`）→ `faith_spawn_big()`（`:766`）→ 命中且变绿时 `mof_drop_wipe()`（`:628-635`）。
11. P 点火力：`on_item_collect()`（`mof.c:829-866`）闩 `s_p_item`（`:838`）+ 满 P 同帧竞态网 `faith_spawn()`（`:858`）；`on_item_commit()`（`:891-917`）→ `mof_core_p_owner()`（`:900`）→ `faith_p_topup_take()`（`:903`）→ 写 `CE_CURRENT_POWER()`（`:908`）+ 跨档 `ce_repopulate_options()`（`:912`）。
12. 绘制与收尾：`on_draw()` 只转发 `faith_hud_draw()`（`mof.c:1504-1519`），读数由 `faith_hud_declare()` 声明（`:1473`）；`on_run_reset`/`on_delete`（`:1664-1665`）都走 `mof_clear()`（`:1522-1580`）：退计分声明（`:1527`）→ `faith_unregister`（`:1528`）→ `faith_clear_entities_mask`（`:1533`）→ `faith_hud_departed`（`:1542`）→ `faith_green_restore_all`（`:1550`）→ `ce_fairy_claim_clear`（`:1551`）→ `faith_green_fairy_depart_all`（`:1558`）→ `bomb10_reset`（`:1565`）→ `memset`（`:1573`）。

**用到的 SDK**：
- `ce_state`（`sdk/public.h:20`）：取/建本卡 `mof_card_t`（≤496 字节，`mof.c:253`）。
- `ce_log`（`sdk/public.h:268`）：全部诊断行（本文件 19 处）。
- `ce_player_pos`（`sdk.h:486`）：收集那刻的自机 y ⇒ 蓝点高度系数（`mof.c:502`）。
- `ce_rand`（`sdk.h:464`）：引擎回放安全 RNG；st06 四色妖精 1/4 掷（`mof.c:739`）。
- `ce_owned`（`sdk.h:472`）：查 111 是否在场（P 点落笔者选举，`mof.c:900`）。
- `ce_repopulate_options`（`sdk.h:402`）：补差跨档后重建子机（`mof.c:912`）。
- `ce_add_life`（`sdk.h:98`）：卡内奖残（`mof.c:1380`）。
- `ce_spell_active`（`sdk.h:605`）/ `ce_spell_flags`（`sdk.h:600`）：符卡进行中 / 超时位（`mof.c:1125`、`:1334`、`:1336`）。
- `ce_fairy_claim_clear`（`sdk.h:1150`）：退绿妖精护栏位（`mof.c:1551`）。
- `ce_score_declare_coupled`（`sdk_core.h:388`，实现 `sdk_core.c:1079`）：交 Δ_110（`mof.c:382`）。
- `ce_score_family_declare`（`sdk_core.h:361`，实现 `sdk_core.c:1054`）：声明 th10 系数族（`mof.c:387`）。
- `ce_score_family10_faith`（`sdk_core.h:357`，实现 `sdk_core.c:1061`）：交显示信仰 F（`mof.c:388`）。
- `CE_CARD`（`sdk.h:918`）：展开 21 槽虚表与 12 个钩子桩（`mof.c:1760`）。
- 引擎宏（`engine.h`）：`CE_ADDR_INPUT_PRESSED:119`、`CE_ADDR_MAX_POWER:38`、`CE_PLAYER_STATE:455`、`CE_ADDR_CHARACTER:72`、`CE_ENEMY_ANM_SRC/SCRIPT:688-689`、`CE_POPUP_*:937-966`。
- `sdk_actions.h`：无调用（该头只有 `ce_give_card_transaction`，本卡不直接用）。

**数值**（单位：显示值 / 存储 / 帧；超 8 行详表见 `CARD-BEHAVIOR-AUDIT.md` §110）：
| 值 | 单位 | 站点 | 一手出处 |
| --- | --- | --- | --- |
| 信仰基础 50000 | 显示值 | `faith_core.h:37` | 一手 th10 存储钳 5000 的显示形态 |
| 大信仰点 5000/8000/10000 | 信仰（E/N·H·L/EX）| `mof_core.h:119` | 一手 `th10:0x41b57f` |
| 小信仰点 100、微档（消弹）10 | 信仰 | `faith_core.h:259`、`:258` | 一手 `th10:0x41b602`、`th10:0x4081e5` |
| 连击槽满 130；大点 +120f、小点 +60f、击杀非 boss +10f、微档 +3f | 帧 | `faith_core.h:308`、`mof_core.h:533`、`:558` | 一手 `th10:0x413035`、`th10:0x40e681` |
| 灵击持续 296 | 帧 | `mof_core.h:683` | 🟡 脚本 `time` 累加（非逐条）|
| 灵击 −3000（决死 0）；miss −(信仰−50000)/3；全避 −30000 | 信仰 | `mof.c:1140`、`:1306`、`:1337` | `faith_core.h:497-499` |
| 灵击门槛 200 档（半档 150）、扣 100（半档 50）、下限 105 | 火力单位（1 档 = 100）| `mof_core.h:746`、`mof.c:1084` | owner 2026-09-24；`mof.c:1091` 文案写 1.55 与常量不符 |
| P 点补差 4 单位（共享策略常态 5 / 爱丽丝 2、3 交替）| 火力单位 | `mof.c:903-908` | owner 2026-09-15；策略在 `faith_core.h` |

**跨卡协同**：
- 111（地灵殿）：共用绿点实体池 `faith.{h,c}`（`mof.c:90`、`:1528`）；耦合计分（`mof.c:375-389`）；灵击唯一实现 `bomb10.{h,c}`，两卡同装「id 小者驱动 / 落笔」（`mof.c:900`、`:1361-1363`、`:1171-1178`）；组合 HUD `faith_hud.{h,c}`（`mof.c:1473`、`:1517`）；P 点共享策略 `faith_p_topup_take()`（`mof.c:903`）。
- 112（星莲船）：压缩包等价计数经 `ufo_bounty.h` 广播，本卡按自己槽折算入账（`mof.c:1202-1221`、`:1214`）。
- 绿妖精换皮 / 载体 / 投放收尾在共享件 `faith.c`：110 或 111 单装都生效（`mof.c:1240`、`:1550`、`:1558`）。
- 计分层 `sdk_core`：110 只交 Δ_110，耦合系数 = 擦弹倍率 + 111 声明的信号强度，由层合成（`mof.c:369-389`；详见 `SCORE-LAYER-REWORK.md`）。
- 共享 PIV 池（`MAXPOINT-AUDIT.md`）：**无参与**——本卡明确「不写 max point 全局」（`mof.c:62`），PIV 表里 110 不在列。
- 生命周期：`LIFECYCLE-AUDIT.md` 把本卡列为「跨关保持 run 级账本」的**范本**（`mof.c:1643-1651`、`:1475`）。

**生命周期**：
- 覆盖：`ctor`（`mof.c:1632`）、`on_load`（`:1618`）、`on_stage_start`（`:1675-1684`，只 `bomb10_reset()` + 清 `spell_watch`）、`on_run_reset` / `on_delete`（`:1664-1665` → `mof_clear()` `:1522-1580`）。`on_delete` = 虚表 `+0x50`；删卡发生在商店、tick 全停 ⇒ 收尾只能靠它（文件头自查 `mof.c:64-77`）。
- 跨帧遗留物与收点：绿点实体池（按槽筛 `:1533`）、组合 UI 的 16 支 VM（`:1542`）、绿妖精换皮 / 载体（`:1550`、`:1558`）、灵击那一发与两支环 VM（`:1565`）；伤害源是引擎池、寿命 1–2 帧，故意不清（`:1566-1572`）。
- 钩子缺口（`LIFECYCLE-AUDIT.md` §4）：F1 `bomb10` 提供者无注销 ⇒ 删卡后悬垂 user（`bomb10.c:170`）；F4 `mof_bonus` 被换面清（`mof.c:1653`）；F5 绿皮收尾只挂 110；F6 `faith_hud_departed` 在 retry 路早退；F7g `s_slot_keep` 哨兵让「删卡后同 id 新卡」继承旧 Δ。
- F2 / F3 台账标「已修（P21 批次，分支 `fix/p17-f2f3-newrun-and-firstinit`）」；本工作树（main）`mof.c:1643` 仍是台账描述的旧判据 ⇒ 两处口径不一致，按实跑复核。

**已知未测 / 风险**（逐条短句）：
- 灵击伤害用 `ce_damage_rect`（矩形独立源）近似一手「伤害累加器」，量纲未与 TH10 逐条对账 🟡（`mof.c:1012-1032`）。
- 门③「关卡帧 ≥ 90」用卡自数帧（TH18 无 `zGameThread` 字段图），待实跑 `sf=` 栏钉 🟡（`mof.c:114-138`）。
- 灵击持续 296 帧来自脚本文本累加 🟡；尾部另有 80×30 帧计数循环，真寿命可能更长（`mof.c:1038-1039`）。
- 灵击无视觉、不消激光；微档信仰点不建实体（`ce_cancel_radius()` 不给位置）——均登记「未做」（`mof.c:80-84`、`:544-549`）。
- 蓝点高度系数仍用**自机 y**，一手用的是**道具自身 y** ⇒ 已知偏差（`mof.c:491-503`）。
- 决死形态「消弹入不入账」🟡/❓ 自定，开关 = `MOF_BOMB_DEATHBOMB_ACCOUNT`（`mof.c:970`）。
- 擦弹是否续连击「口径待核」（`mof.c:1258`）；RANK 写入在 TH18 是死字段、写了无实际效果（`mof.c:999-1000`）。
- 绿妖精：`anmColor` 运行时染色测不出、`enemy.anm` 重载时机未验、`Cir*`/`Phantom*` 不在覆盖内（`REPORT-110-GREEN-FAIRY.md` §8）。
- 实跑金丝雀（`LIFECYCLE-AUDIT.md` §3）：一面→二面后 PIV / 信仰 Δ **不应**被清；换面后 `sf=` 应从 0 重爬、`gate` 第 3 位 90 帧内为 0。
