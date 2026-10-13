> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
### 111 — 东方地灵殿（CARD-CODE）

**一句话**：把 TH11 的通信强度槽 + 信仰/蓝点分 + 灵击/命碎 + 12 伙伴支援特技整层搬进 TH18：擦弹与站位养一条 0–130 的信号槽，满槽全收点，槽值当计分乘数（同台账 §111）。

**实现链路**（一手反汇编见 `signal.c` 文件头与 `signal_core.h`；本文只报站点）：

1. **绑定**：`CE_CARD(111, …)` `signal.c:4089` 挂 ctor / on_load / on_tick_2 / on_draw / 三个道具钩子 / on_bomb_spent / 两个死亡钩子 / on_stage_start / on_run_reset / on_delete / on_tick_shooters / on_shot_set_index / on_option_pose_apply，并声明 `.active_recharge = 1` + `.on_activate`（`signal.c:4095`）。
2. **开局 / 注册**：`ctor` `signal.c:3874` 从静态账本 `s_slot_keep111`（`signal.c:106`）恢复槽，只在「关卡号变小」时 `faith_slot_init_111()`；`on_load` `signal.c:3854` → `signal_ensure_registered()` `signal.c:3821`（`bomb10_register_host()` `:3830`、`faith_register()` 注册 `1 << FAITH_SLOT_111` `:3840`、`faith_hud_on_load()`）。
3. **计分声明**：`signal_declare_score()` `signal.c:446` → `ce_score_declare_coupled()` / `ce_score_family_declare(CE_COEFF_FAMILY_1)` / `ce_score_mul_declare()`（`signal.c:455`、`:458`、`:459`）；`on_tick_2` ⑨ 每帧重发（`:3503`），粘性、删卡由层清。
4. **主 tick**：`on_tick_2` `signal.c:3311` 逐段：⓪ 咲夜擦弹扫描 `signal_graze_scan()` `:3348`；① 擦弹帧差 + 倍率 `mof_core_graze_mult()` `:3353-3357`；② 回收线档 `signal_core_band()` `:3372`；③ 槽推进 `signal_core_apply_step_mod()` `:3390`；④ 命数差分兜底 miss `signal_core_miss_penalty()` `:3404-3407`；④b 命碎段机 `signal_seg_tick()` `:3428`；⑤ 满槽全收 `signal_sweep_items()` `:3433`；⑥ X 上升沿灵击 `bomb10_start()` `:3449`；⑥b 十二伙伴按机体门分派 `:3463-3467`；⑦ 满 P 转换 `faith_convert_full_p_items()` `:3477`；⑧ 绿点池 `faith_pool_tick()` `:3480`、得分奖残 `signal_lifefrag_tick()` `:3483`、灵击逐帧 `bomb10_tick()` `:3492`、河童罩子 `signal_shield_tick()` `:3496`、残像 `signal_trail_tick()` `:3500`。
5. **HUD**：`on_draw` `signal.c:3730` 只转发共享件 `faith_hud_draw()` `:3743`，再由 `signal_tint_tick()` `:3761` 收放咲夜的红染与擦弹圈（帧守卫在 `ce_sdk_frame()`）。
6. **灵击两入口共用一份实现**：`s_bomb_host` `signal.c:1616`（`power_spend` = `signal_bomb_power_spend()` `:1560` → `signal_core_alice_power_spend()` `signal_core.h:648`；`on_start` `:1584` 起河童罩子 `signal_shield_arm()`）；C 入口 `on_activate()` `signal.c:4069` → 同一 `bomb10_start()` `:4085`。
7. **绿点三源 → 一槽**：击破大绿点 `signal_on_enemy_drop_pre()` `signal.c:640`（`ce_drop_main()` `:656`）、满 P 中绿点 `signal_on_item_collect()` `:731` + `on_item_commit()` `:795`、消弹小绿点 `signal_on_cancel()` `:493`；收集回调 `signal_on_pickup()` `:470` → `signal_core_pickup()` `signal_core.h:83`。
8. **收尾**：`signal_clear()` `signal.c:3777`（退声明 → `faith_unregister` → `faith_clear_entities_mask(1 << FAITH_SLOT_111)` → `faith_hud_departed()` → `faith_green_fairy_depart_all()` → `signal_support_reset()` → `memset`）。

**用到的 SDK**（`sdk_actions.h` 本卡不 include，未使用）：

- `ce_state(c, T)` — 卡私有 496B 状态区（本卡 `st()` `signal.c:425`）；宏 `sdk/public.h:20`。
- `ce_score_declare_coupled(id, points)` — 声明耦合族 Δ 得点（不含基础）；`sdk_core.h:388`（`signal.c:455`）。
- `ce_score_mul_declare(id, permille)` — 声明信号强度（0..1000‰ 乘数）；`sdk_core.h:394`（`signal.c:459`）。
- `ce_item_collect_all()` — 把活道具切 state 3（满槽全收 / 萃香全收共用）；`sdk.h:1035`（`signal.c:854`）。
- `ce_drop_main(counts)` — 取击破主掉落类型计数（判大绿点）；宏 `sdk.h:25`（`signal.c:656`）。
- `ce_spawn_item(type,pos,angle,speed)` — 撒命碎道具（type `0x10`）；`sdk.h:413`（`signal.c:619`）。
- `ce_add_life()` — 得分跨阈值奖残（走引擎加法）；`sdk.h:98`（`signal.c:3265`）。
- `ce_poc_height_semantic()` / `ce_poc_declare_px_id()` — 读语义线高 / 神奈子降线声明；`sdk.h:554`、`:528`（`signal.c:3372`、`:3050`）。
- `ce_native_option_hide_declare()` / `ce_option_pose_declare()` — 隐藏原生子机 / 低速子机逐格偏移；`sdk_core.h:583`、`sdk.h:706`（`signal.c:3899`、`:2469`）。
- `ce_cancel_latch_claim()` / `ce_forced_death_active()` — 同帧消弹闩锁 / 救人通报位（罩子让位）；`sdk_core.h:483`、`:520`（`signal.c:2992`、`:1237`）。
- `ce_hud_slot_get()` — 取共享 HUD 块最新矩形（由共享件调用）；`sdk_core.h:849`。
- `ce_full_p_gate_active()` / `ce_owned()` — 读 106「未满 P」声明关掉线上补满 / 灵击驱动选举；`sdk_core.h:561`、`sdk.h:472`（`signal.c:2194`、`:3492`）。

**数值**（≤8 行；完整表见 CARD-BEHAVIOR-AUDIT.md §111）：

| 数值 | 值 | 单位 | 站点 | 一手出处 |
| --- | --- | --- | --- | --- |
| 信号槽上限 | 130.00（存 130000） | 千分点 | `faith_core.h:371` | `th11:0x40bab5` |
| 满槽阈值 | 100.00（100000） | 千分点 | `faith_core.h:372` | `th11:0x42364a` |
| 擦弹入槽 | E +8.00 / N·H·L·EX +5.00 | 显示点/颗 | `faith_core.h:398-402` | `th11:0x4a7950` |
| 线下每帧 | −0.30（存 −300，下限 0） | 显示点/帧 | `faith_core.h:392` | `th11:0x420a85` |
| 绿点档值 | 大 +100 / 中 +10 | 显示点/颗 | `faith_core.h:260-261` | 一手 th11 item type 3 / 9 |
| 灵击无敌 | 270（普通）/ 200（决死） | 帧 | `bomb10.h:138-139` | `th10:0x4259be` / `th10:0x425c2d` |
| 灵击火力 | 门槛 ≥2.00 / 扣 1 档 / 下限 1.05；魔理沙 = 0.5 档 | 档 | `faith_core.h:325-328` | ❓ 用户映射（一手 `th11:0x4311d3`/`0x4311f1` 为 per-shot 20、无下限） |

灵击伤害（32.0/帧、75 帧、20×30 框）与命中碎阈值等见台账 §111 与 `signal.c:142-145` 的表。

**跨卡协同**：

- **110（风神录）**：共用 `faith.c` 实体池 / `faith_core` / 组合 UI `faith_hud`（188×60）/ `bomb10` 唯一一份灵击状态；同装时 111 一律让位（撒点 `signal.c:694`、P 点落笔 `:804`、灵击驱动 `:3492`）。
- **112（星莲船）**：`ufo_bounty` 广播等价计数 → `signal_bounty_eq_fold()` `signal.c:3290`，按本卡槽折算入账，不与实体重复（见 HANDOFF-CROSSCARD-STATE.md §13）。
- **108（妖妖梦）**：低速子机声明层按 id 序（108 < 111）⇒ 同装时 108 落笔，111 只收子机（`signal.c:3899`、`:2745`）。
- **106（红魔乡）**：读 `ce_full_p_gate_active()` `signal.c:2194` 关掉「线上补满」；读 `ce_poc_height_semantic()` `:3372` 避开「遮」。
- **救援类（107/113/117）**：`+0x0c` 的 acc ≠ 0 ⇒ 不按 miss 记账 `signal.c:3237`。**共享 PIV 池：无**（本卡无任何 `ce_piv_*` 调用，MAXPOINT-AUDIT 无 111 行）。

**生命周期**（LIFECYCLE-AUDIT.md:56、:83 判本卡为正例）：

- `ctor` `signal.c:3874`（恢复跨关账本 / 新局判据 `s_slot_stage111` / 隐藏原生子机声明 / 段机 rebase）；`on_load` `:3854`（注册 + 声明 + 早苗收点线）。
- `on_stage_start` `signal.c:3938`（支援状态每关归零 + 段机 rebase + 信号槽归 0 = 复刻一手 `th11:0x41fb24`；**保留** Δ / `small_rem` / `frag_idx`）。
- `on_run_reset` `signal.c:3962`（信号归 0 + `frag_idx` 清 + 段机整台 init + HUD 退场）；与 `on_delete` `:3987` 都走 `faith_hud_departed()`。
- `on_delete` `signal.c:3987` → `signal_clear()` `:3777`：退计分声明 / 退注册 / 只收自己槽 `1 << FAITH_SLOT_111` / 共享 HUD / 绿妖精载体 / 支援复位 / `memset`。**缺口：无**。

**已知未测 / 风险**：

- 得分奖残阈值表只取到第 0 项（`0x3b9aca00` = 10⁹）⇒ TH18 分数上限打不到该路（`signal.c:3255-3257`、TUNING 111-1）。
- 命碎段机边角：相邻相位指纹逐位相同认不出、间隔 <5 帧的第二边沿被吞、道中妖精波不计段（TUNING 111-10/11/12）。
- 过场 `killAll` 清场时宿主妖精会多掉一颗大绿点（该钩子不带 kind）—— `signal.c:669-674`。
- 原版「每颗小绿点另给 +10 分」未做；对白中 / 一局前 90 帧不推进槽的两个窗口未实现（`signal.c:491-492`、TUNING 111-5）。
- 激光擦弹档已具名但没人调用（只用子弹表 ⇒ 激光收益略高，TUNING 111-3）；河童 bomb 那种不扣命中弹归零信号槽未做（TUNING 111-9）。
- C 入口有一帧相位差（起手第一次 tick 落在下一帧；总帧数/伤害/半径全同）—— `signal.c` 的 `on_activate` 注释，🟡。
- 魔理沙爱丽丝的 0.5 档：比例 ✅一手、档位门槛/下限为 ❓ 映射（TUNING 111-15/16）。
- 三行文字读数为 ❓ 自定（原生 TH11 那块没有数字），块因此 208×56 把 116/117 往右推（TUNING 111-7）。
