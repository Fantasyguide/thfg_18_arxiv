> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
### 109 — 东方花映塚（CARD-CODE）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）；正文裸地址默认属它，跨作写 `th09:0x…` / `th19:0x…` / `th11:0x…`。
> **代码**：`native/cards/pofv.c`（8691 行）＋池/数学 `native/cards/pofv_core.c`（1166 行）。
> **行为账**：见 `CARD-BEHAVIOR-AUDIT.md` §109（本文只写「钩子 → 落笔」的链路，不重抄行为表）。

**一句话**：花映塚 × 獣王園 合成卡——按住 `C` 蓄力并展开固灵结界，松手按档放 C1–C4 攻击；击破/消弹掉灵力道具攒槽，场上循环生成自建灵。（同台账 §109）

**实现链路**：
1. 注册：`CE_CARD(109, …)` `pofv.c:8679`–`:8691`（12 个回调＋`.active_recharge` 值，见「生命周期」）。
2. 键电平：`pofv_charge_key()` `:1219`（只认 C）、`pofv_aura_key()` `:1224`（C ∨ Shift）；每帧取 `:6635`/`:6636`。
3. 蓄力累加：`rate` `:6689`、`s_aura_charge +=` `:6692`、`cap` `:6680`、clamp `:6694`；`s_aura_stamp` 由 `.on_active_tick` `:8537` 打戳。
4. 结界半径：按角色取 K `:6599`–`:6602`；张开追赶写 `s_aura_r` `:7144`；咲夜固定 `144.0f` `:7163`；`s_aura_size<=0` 归零 `:7267`。
5. 松手放档：只在松手沿 `:6720`；档位判定 `:6721`–`:6723`；角色分派 `:6737`–`:6772`；C1 补发 `:6822`–`:6825`；扣灵力 `:6828`–`:6833`。
6. C1–C4 落点：`pofv_c1_fire()` `:3358`、`pofv_c2_fire()` `:2436`、`pofv_c3_fire()` `:3941`、`pofv_c4_fire()` `:4516`；魔理沙 `:4872`/`:5018`/`:5117`；早苗 `:5285`/`:5405`/`:4627`；伤害源 `ce_damage_rect()`。
7. TH09 消弹环：`pofv_ca_ring_spawn()` `:2785`、`pofv_ca_ring_tick()` `:2813`；每帧 `:8331`。
8. 暴风：`on_enemy_death` 入池 `:6035`/`:6121`/`:6126`；`do_storm_frame()` `:2979` 半径消弹 `:2989`＋命中 `:2997`；VM `storm_vm_sync()` `:2004`（每帧 `:8332`）。
9. 灵力道具：击破撒 `pofv_power_spawn_dir()` `:5783`（落点 `faith_scatter_from_enemy()` `:5782`）；入账 `pofv_rei_credit()` `:1327`；VM `power_vm_sync()` `:3085`（每帧 `:8449`）。
10. 灵：周期生成 `:6493`（1800 帧，boss 在场跳过 `:6504`）；结界罩到写 `$I0=1` `:6207`、沈静化 `:6242`；离场回收 VM `:5745`；真敌机 `ce_spawn_enemy("CESpirit109", …)` `:8206`。
11. 罩子：上升沿给盾 `pofv_barrier_fire()` `:4293`；真撞弹 `pofv_on_death_before_deathbomb()` `:8639` ⇒ 撤销中弹 `:8652`、无敌 `:8653`、三层破罩 `:8616`/`:8667`、反向位移 `:8668`；每帧 `:7054`。
12. 染色：`on_draw()` `:8494`（`ce_sdk_frame()` 同帧去重 `:8497`）⇒ `pofv_aura_tint_tick()` `:1903`，写 `bullet+0x28+CE_ANM_VM_COLOR1` `:1926`。
13. Z 阻断：`.on_native_shot_suppress` `:5728`；`on_tick_2` 先存 `ce_input_z_frozen` 再清 Z 位 `:7042`/`:7043`。

**用到的 SDK**：

| 调用 | 定义 | 用途 |
| --- | --- | --- |
| `ce_key_down` | `sdk.h:841` | 读 C / Shift / 空格键电平（蓄力、结界、调试）。 |
| `ce_player_pos` | `sdk.h:486` | 自机坐标：结界中心、掉落点、破罩原点。 |
| `ce_anm_spawn` / `ce_anm_delete` / `ce_anm_get_vm` | `sdk.h:86`/`:393`/`:240` | 卡自建 VM 的起 / 删 / 探活。 |
| `ce_anm_set_scale` / `_pos` / `_rotation` / `_sprite` | `sdk.h:336`/`:265`/`:287`/`:257` | 每帧写结界 / 消弹场 VM 的几何。 |
| `ce_anm_set_color` | `sdk.h:273` | 结界碰弹染色（整条 dword 覆盖）。 |
| `ce_anm_interrupt` | `sdk.h:391` | 触发脚本收束 / 破罩「anm 事件 1」。 |
| `ce_damage_rect` | `sdk.h:172` | C1–C4 与暴风的半径伤害源。 |
| `ce_invuln_request` | `sdk_core.h:95` | 罩子撞后 100 帧无敌（声明层）。 |
| `ce_rescue_note_claim` | `sdk_core.h:503` | 罩子救援闩锁（照 107/117 约定）。 |
| `ce_forced_death_active` | `sdk_core.h:520` | 区分「卡造的死」与真撞弹。 |
| `ce_spawn_enemy` | `sdk.h:1197` | 生成灵本体真敌机（ECL `CESpirit109`）。 |
| `ce_enemy_by_id` | `sdk.h:619` | 按 id 查灵本体存活。 |
| `ce_hud_slot_claim_card_ex` | `sdk.h:1301` | 认领「灵力=蓄力槽」HUD 块（`:942`）。 |
| `ce_hud_number_clear` / `ce_hud_vm_text_set` | `sdk.h:774`/`:798` | HUD 数字 / 文字 VM 擦除与写值。 |
| `ce_play_sound` | `sdk.h:50` | 蓄力音 `0x1e`（`pofv.c:1280`）、道具音。 |
| `ce_sdk_frame` | `sdk_core.h:618` | `on_draw` 同帧去重。 |
| `ce_rand` | `sdk.h:464` | 灵的种类、掉落散布随机。 |
| `ce_log` | `sdk/public.h:268` | 诊断日志。 |

`sdk_actions.h`：**无**（其唯一 API `ce_give_card_transaction` 本卡未调用）。

**数值**（同上台账 §109 的成本/数值段；此处只列代码落点）：

| 项 | 值 | 单位 | 站点 | 一手出处 |
| --- | --- | --- | --- | --- |
| `.active_recharge` | 60 | 帧 | `pofv.c:74` | ❓自定（只满足主动卡契约） |
| 灵力上限 | 2500 | 灵力（存储） | `pofv.c:1317` | 一手（`0x9c4` = 2500） |
| 每颗灵力道具 | +15 | 灵力 | `pofv.c:1320` | 一手 `FUN_00520d40` |
| C1/C2/C3/C4 阈值 | 500/1000/1700/2500 | 灵力 | `pofv.c:4144`–`:4151` | 一手 `pl00.sht` 偏移 ×10 |
| 结界半径 | 蓄力 × K/100，K=64/14/64/128 | px | `pofv.c:1238`–`:1241` | 一手 §7.2（咲夜 64 ❓） |
| 暴风（灵档） | r=80 / 40 帧 / dmg 10 | px, 帧, 伤害 | `pofv_core.h:72`–`:74` | 一手 `ins_1028/1031/1029` |
| 灵循环周期 | 1800 | 帧 | `pofv.c:479` | ❓自定（owner 未定数） |
| 罩子破罩 / 无敌 | 40 / 100 | 帧 | `pofv.c:4165`/`:4166` | 一手 |

**跨卡协同**：
- 106（红魔乡）：`pofv_card_present(106)` ⇒ 开 `s_z_th09_mode` `:7017`（`:294` 的注释同述）。
- 108（红魔乡）：共用 `on_native_shot_suppress` 断点 `th18:0x45ebfb`（`:5710`）；本卡先存 `ce_input_z_frozen` 再清 Z 位 `:7042`（108 < 109，108 先跑）。
- 111（地灵殿）：同写 `bullet+0x28+CE_ANM_VM_COLOR1`；卡按 id 顺序跑 ⇒ 109 先、111 后，111 的红压住本卡 `:8486`–`:8488`（❓未解决）。
- 共享件：`faith_scatter_from_enemy()` `:5782`（照 110/111，已在 `AUDIT.md` §BE 登记 `:5778`）、`bomb10_cancel_impending_death()` `:8652`（111 同款）、`ce_hud_slot_claim_card_ex()` `:942`（116/117 同款 `:8455`）。
- PIV / 额外最大得点：**无**（`pofv.c` 无 PIV 调用点）⇒ 与 `MAXPOINT-AUDIT.md` 无交集。

**生命周期**：
- 覆盖：`.ctor` `:8682`→`:5683`、`.on_load` `:8683`→`:8461`、`.on_run_reset` `:8690`→`:8477`、`.on_delete` `:8691`→`:8478`。
- 其余 8 个为游玩期回调：`.on_activate` `:8681`、`.on_active_tick` `:8679`、`.on_native_shot_suppress` `:8684`、`.on_shot_set_index` `:8685`、`.on_enemy_death` `:8686`、`.on_death_before_deathbomb` `:8687`、`.on_tick_2` `:8688`、`.on_draw` `:8689`。
- `on_run_reset` 与 `on_delete` 逐字相同（都只调 `clear_card`）⇒ 无「只靠 `on_delete`」的状态。
- **无 `.on_stage_start`**：`pofv.c` 里不存在该符号。
- 跨面：`.ctor` 每关重跑且清 run 级数据 ⇒ 换面全清；owner 已豁免（`LIFECYCLE-AUDIT.md` §2 的 109 行、§2.1 判据表）。
- `clear_card()` `:5483` 收：暴风池＋VM `:5498`、灵力道具池/VM `:5516`、消弹登记 `:5530`、结界 VM/分片 `:5548`/`:5549`、染色彩还原 `:5556`、各档 clear `:5564`–`:5573`（含罩子 `:5567`）、火花 `:5574`。
- 已知缺口（`LIFECYCLE-AUDIT.md` F7e）：7 台 HUD VM、`s_c4s` 无 clear 函数、灵本体真敌机两钩子都不收。

**已知未测 / 风险**（台账 §109＋本卡注释）：
- 暴风半径律仍借 TH09 节点值；一手 TH19 Bomb tween 的键值未取到（`pofv.c:21`–`:24`）。
- 一手「enemy flags bit27 ⇒ 额外道具 n = 1」的例外未接（`:5798`）。
- 破罩音效 `0x1b` 是 TH19 SE 号 ⇒ 未落地（`:8658`）。
- `on_activate` 的 `return 0` 曾实跑卡死 ⇒ 现退回 `CE_ACTIVATE_REFUSED`，引擎拒绝音效未去掉（`:8523`–`:8526`）。
- 角色读点 `0x4cccf4` 的「1 = 魔理沙」只由日志核实、未逐值核（`:6649`）。
- 咲夜结界半径先用固定 144 px、K=64，一手换算未定（`:7163`）。
- 早苗 C1/C2 原生弹表认领已改恒 `-1`（不覆盖主炮），改走卡侧 `pofv_emit_group()`（`:8578`–`:8581`）。
- CA 环 kind 4 的 `+0x28` 标量语义未读到 ⇒ 不实现（`:6808`）。
