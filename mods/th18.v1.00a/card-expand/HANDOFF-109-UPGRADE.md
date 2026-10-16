# HANDOFF-109-UPGRADE —— 《109 如何升级》指示（P19②）

> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P19**（owner 2026-10-14 新增 ✓）：**只读**通读 `cards/pofv.c` ✓ + 交付本指示 ✓ + 跨卡内容可搬迁
> （搬迁须逐条登记 `AUDIT.md` §BE 的「我动了什么」✓）。**本指示本身不改 109 的任何行为** ✓（R3 ✓）。
>
> **读者**：正在改 109 的那个会话 + owner ✓。**用法**：先看 §1 的步骤表（每步都有**判据**与**风险** ✓），
> 需要细节时按 §2–§4 的三份只读素材查（分别是**现状链路 / 缺口与风险 / 跨卡搬迁候选** ✓）。
>
> ⚠ 三条纪律：① **109 允许只读** ✓ 但**不得擅自改它的行为** ✗；② 它**仍被另一会话编辑** ⇒ 动 `pofv.c`
> 一律 **worktree + 精确 hunk + PR** ✓；③ 搬迁（搬出去）**只列候选、不擅自搬** ✓。

## 1. 升级步骤表（建议顺序；每步 = 一条独立工单 ✓）

| # | 目标 | 一手 / 依据 | 判据（怎么算做完 ✓）| 风险 |
| :-: | --- | --- | --- | --- |
| **S1** | **7 台 HUD VM 的收尾** ✗：`s_hud_chg_label/cur/slash/max_vm`（`n/m` 四颗字）与 `s_hud_lv_gf/label/digit_vm`（档位弹出三台）目前**只在显示条件转假时**被删 ⇒ 换面/retry 靠下一帧自愈，**商店删卡（tick 停）时无路可删** | 见 §3 的缺口清单（行号在 `pofv.c` 内 ✓）| 删卡 / 退菜单后**不留残影** ✓；`on_delete` 里能数出 7 台都被 `ce_anm_delete` ✓ | 动的是**删卡路径**（商店里 AbilityManager 冻住 ⇒ 只有 `+0x50` 跑得到）⇒ 必须放在 `on_delete` 一侧 ✓ |
| **S2** | **`s_c4s` 没有 clear 函数** ✗：`clear_card()` 只清它的弹池 `s_c4_b` ⇒ `active/timer/aim/tgt` 跨 retry 残留（早苗 C4 会从旧相位续 ✗）| §3 缺口清单 ✓ | retry 后立刻放 C4 ⇒ **从相位 0 起** ✓（日志 / 观感 ✓）| 低（新加一个 clear + 在 `clear_card` 调 ✓）；⚠ 早苗机体才可达 ✓ |
| **S3** | **灵本体的"真敌机"两个钩子都不收** ✗：`ce_spawn_enemy("CESpirit109")` 造的实体只靠它自己 ECL 走到 `$I0=999` 离场 ⇒ **商店删卡那一刻**场上会留一只无主灵 ✗ | §3 缺口清单 ✓ | 删卡后场上**无残留灵** ✓ | **中高**：可能影响正在跑 109 的那个会话的进度 ✗ ⇒ 先与他们对齐再动 ✓ |
| **S4** | **咲夜专用 `s_aura_sec_deg` 只增不清** ✗（全文件只递增到 90 封顶、**没有任何归零点**）⇒ 跨面/跨局永久残留 | §3 缺口清单 ✓ | 换面 / retry 后张角从初值起 ✓ | 低（加一个归零点 ✓）；仅咲夜可达 ✓ |
| **S5** | **跨面语义的"文档措辞"对齐** ⓘ：`MODEL-109-SPIRIT-MIRROR.md` 写「**跨面 ⇒ 全清**」✓，而 owner 的总口径是「换面保持数据，**除 109**」✓ ⇒ **两者其实一致**（109 被豁免 ✓）⇒ **无需改代码** ✓，只需在文档里把"为什么 109 例外"写清 ✓ | owner 口径（本 goal 的目标原文 ✓）+ 该 MODEL 文档 ✓ | 两份文档**不再互相矛盾** ✓ | 无（纯文档 ✓）|
| **S6** | **跨卡/共享内容搬迁**：把 `pofv.c` 里"概念上属于共享"的东西搬出去（候选见 §4 ✓）| §4 清单 ✓（每条都给了站点与建议去向 ✓）| 搬迁**逐条**进 `AUDIT.md` §BE 的「我动了什么」✓；搬迁前后 109 的**行为逐字不变** ✓ | 中：`pofv.c` 被并发编辑 ⇒ 一律 worktree + 精确 hunk ✓；搬迁会**动共享件** ⇒ 需报影响面 ✓ |
| **S7** | **与已修共享件的关系**（供对账 ✓）：`bomb10` 的提供者注销（P20 已修 ✓）、子弹扫描宏 `CE_BULLET_LIVE_FOR_EACH`（P26 已修 ✓ —— 109 的 `pofv.c` 也吃这个宏 ✓）| `AUDIT.md` §BE-BOMB10-UNREGISTER / §BE-BULLET-LIVE-MACRO ✓ | 109 的结界染色在 P26 之后应**恢复** ✓ | 无（只是对账 ✓）|

> ⚠ **不要**在本指示范围内做的事 ✗：改 109 的数值 / 改它的行为语义 / 把 §4 的候选**直接搬走**（先开工单 ✓）。

## 2. 素材 A —— 109 的现状与实现链路（只读报告原文）

# 109 现状与实现链路（只读整理）

> 范围：只读 `mods/th18.v1.00a/card-expand/` 下的 109 实现与文档；未改任何仓库文件。
> **路径缩写**：`CARD` = `mods/th18.v1.00a/card-expand/native/cards/pofv.c`；`CORE` = 同目录 `pofv_core.h`；
> `DOC` = `mods/th18.v1.00a/card-expand/`。行号一律写 `文件:行号`。
> 一手出处默认 **TH19 v1.10c**（imagebase `0x400000`），TH09 写 `th09:0x…`，TH18 写 `th18:0x…`。

---

## 1. 109 到底是什么卡

**一句话**：`109 = 花映塚（TH09）× 獣王園（TH19）合成卡` —— 自机按住 `C` **蓄力**并同时展开
**固灵结界 + 消弹场**，松手按蓄力档位放 **C1–C4**；击破敌人产生**暴风（連爆）**与**灵力道具**，
灵走「**真敌人**」、对面侧走「**镜像模型**」。（`DOC/CARDS.md:33`、`DOC/CARDS.md:405`、`CARD:1`、`DOC/MECHANICS-109.md:15`）

**复刻来源（三套并用，均以 owner 口径裁定）**：

| 来源 | 复刻了什么 |
| --- | --- |
| **TH09 花映塚** | 消弹场「节点律」= 圆心 + `r0`/`dr`/寿命、逐帧扩张（`th09:0x41fe93`、`th09:0x403c30` 族）；CA 消弹环；TH09 左上角 HUD（SP 行 / 連擊槽 / `Hit` 红数字）；**咲夜**（TH19 无咲夜，`DOC/HANDOFF-109-C1-C4.md:94`） |
| **TH19 獣王園** | 除霊オーラ = **固灵结界**及其四效果（`DOC/MECHANICS-109.md:113`）；灵力道具；灵的种类（TH19 四族 + TH09 幽灵）；連撃 hits；C1–C4 攻击配方（`DOC/HANDOFF-109-C1-C4.md`）；罩子（オートパリア，`th19:0x40b420`） |
| **混合（本卡自定）** | 单体场口径：原作「两场地互送」⇒ 本卡「我生成在我方场内、打我方场内的 E」；「送到对面」= 生成额外己方攻击 / 灵的暂存（`DOC/HANDOFF-109.md:18-40`、`CARD:6826-6838` 的降级 C4→C3 一例） |

---

## 2. 实现链路（按键 → 落笔）

### 2.1 蓄力（按住 `C`，`Shift` 只开结界）
- 键读取（只认 `C`）：`CARD:1219` `pofv_charge_key()`；结界键 = `C ∨ Shift`：`CARD:1224` `pofv_aura_key()`。
- 逐帧累加 + 被灵力封顶：`CARD:6653` 起 → `CARD:6689-6695`（`rate = clamp(s_reiryoku,0,200)`、
  涨率常数 `CARD:1301`）；一手 = `th19:0x4e7cec`–`0x4e7d29`，满 2500 用 250 帧（owner 实测 4.167 s，`CARD:1268-1300`）。
- 松手沿定档 + 扣灵力：`CARD:6720-6723`（`tier_rel` 1–4）、`CARD:6826-6838`。
- 蓄力音：`CARD:6639`（`POFV_SE_CHARGE = 0x1e` = `se_ch00.wav`，`CARD:1296-1300`）。

### 2.2 固灵结界（除霊オーラ）
- 半径律 `R = clamp(charge,0,200) × K / 100`、按角色取 `K`：`CARD:1227-1237`、`CARD:1387` `pofv_aura_kind()`。
- 可视 VM 起/写缩放/收束（脚本 402/403-405/406）：`CARD:7273-7305`、`CARD:7375-7376`、`CARD:7425-7431`；
  删卡收干净 `CARD:5548`。
- 结界效果：形状判定 `CARD:1470` `pofv_aura_hit_here()`；结界扫弹 `CARD:1751` `aura_scan_bullets()`；
  灵力道具掉落 `CARD:1422` `pofv_graze_scan_drop()`；碰弹染色 `CARD:1903` + `on_draw` `CARD:8494`。

### 2.3 灵击（= `C2` 霊撃）
- 发动 / 每帧：`CARD:2436` `pofv_c2_fire()`、`CARD:2448` `pofv_c2_tick()`（105 帧；一手 `EXReigekiInf`
  槽 1 `th19:0x53b920` / 槽 2 `th19:0x53b3a0`，配方注在 `CARD:2189-2226`）。
- 消弹 + 撒道具：`CARD:2405` `c2_cancel_drop()`、`CARD:2410`。
- 按角色分派：`CARD:6738-6740`（魔理沙 `CARD:4872` / 早苗 `CARD:5285` / 灵梦 `CARD:2436`）。

### 2.4 暴风（連爆 / 消弹场）
- 触发（敌人死亡）：`CARD:6035`、`CARD:6121`（`pofv_storm_spawn_tier` / `pofv_storm_spawn`）。
- 每帧执行：`CARD:8326-8331` → `CARD:2979` `do_storm_frame()`（半径消弹 + 連撃 `+1` `CARD:2994`
  + `dmg=1` 半径命中 `CARD:2997` + 送幽灵 `CARD:3014-3018`）。
- 画面：`CARD:2004` `storm_vm_sync()`（脚本 396，半径写入 VM scale ⇒ 判定=绘制）；半径律现取
  TH19 `ins_1028`（小妖精 50 / 灵 80，`CORE:41`/`CORE:72`）。

### 2.5 灵力道具
- 生成口① 击破掉落：`CARD:5783`（`pofv_power_spawn_dir`，角 −π/2、速 2.2、面值 20）。
- 生成口② 结界碰弹**必定掉 1 颗**：`CARD:8338-8355`（`pofv_power_spawn_packed_at`，`POFV_POWER_PTS_AURA`）。
- 生成口③ 消弹环撒道具：`CARD:1782`、`CARD:2410`、`pofv_ca_ring_tick()` `CARD:2813`。
- 推进 / 收取 / 入账：`CARD:8374` `pofv_power_tick()` → `CARD:1327` `pofv_rei_credit()` → `s_reiryoku`；
  可视 VM `CARD:3085` `power_vm_sync()`。

### 2.6 对面灵镜像（opponent slots）
- 落地门（收件方口径）：`CARD:360` `pofv_foe_spawn_gated()` → `CARD:365` `pofv_spirit_cap_for()`、
  `CARD:377` `pofv_spirit_room()`、满 ⇒ 折算计数不丢包 `CARD:394`。
- 镜像击破（最近邻）：`CARD:5979` `pofv_foe_spirit_mirror_kill()` → 回程 `CARD:6006`/`CARD:6015`
  `pofv_foe_incoming_push()`。
- 到达落回本侧：`CARD:8047` `pofv_foe_incoming_tick()`；模型每帧推进 `CARD:6463` `pofv_foe_spirit_tick()`。
- 口径全文：`DOC/MODEL-109-SPIRIT-MIRROR.md`（§0.5 五条事件源、§1 容量、§2 镜像击破）。

### 2.7 HUD
- 货架槽认领 + UI 列表起 VM：`CARD:900` `pofv_hud_chg_claim()`、`CARD:737-756` `hud_vm_spawn()`
  （`ce_anm_spawn_ui`，表面 px）。
- 蓄力槽（脚本 422：底槽 + 蓝=灵力 + 白=蓄力 + 4 枚档位标）：`CARD:959` `pofv_hud_charge_bar()`，调用点 `CARD:7874`。
- TH09 SP 行（框 = `front.png` sprite7、灰数字 8–17、`Hit` = 18）：`CARD:7878-7975`。
- TH09 連擊槽（原图 sprite42）：`CARD:7760-7800`。
- **已撤下**（注释写明原因）：TH19 `hits` 大字 `CARD:7782`、TH19 攻击槽 `CARD:7790`、TH09 `Bonus!` `CARD:7799`。

---

## 3. 入口与钩子：`CE_CARD(109, …)`

块本身：`CARD:8679-8690`。

| 回调 | 定义处 | 作用 |
| --- | --- | --- |
| `.on_active_tick = pofv_active_tick` | `CARD:8534` | 只给「按住 C」打时间戳，不做事 |
| `.active_recharge = POFV_ACTIVE_RECHARGE` | `CARD:74` (=60) | 主动卡冷却 |
| `.on_activate = on_activate` | `CARD:8511` | 恒 `CE_ACTIVATE_REFUSED`（占位） |
| `.ctor = on_ctor` | `CARD:5683` | 初始状态 / 池初始化 |
| `.on_load = on_load` | `CARD:8461` | 每关重置 C3/C4 等级 + 重认领 HUD 槽 |
| `.on_native_shot_suppress` | `CARD:5725` | 压制原生射击 |
| `.on_shot_set_index` | `CARD:8569` | 认领原生弹表（**现恒返 −1**，见 §4） |
| `.on_enemy_death` | `CARD:5731` | 暴风 / 連撃 / 掉落 / 镜像击破 / 灵离场 |
| `.on_death_before_deathbomb` | `CARD:8639` | 罩子 = 一次撞弹豁免 |
| `.on_tick_2` | `CARD:6134` | 主流程（A 路出队生成灵 → 蓄力/结界 → C1–C4 → 各系统 tick → HUD） |
| `.on_draw` | `CARD:8494` | 结界碰弹染色（虚表 `+0x48`） |
| `.on_run_reset` | `CARD:8477` | → `clear_card()` |
| `.on_delete` | `CARD:8478` | → `clear_card()`（铁律：长持续效果收干净） |

---

## 4. 已完成 vs 未完成 / 占位

**判据**：标 ✅ 的取文档原文；「注释里有一手地址」的按节点内一手出处计。**其余一律算未坐实。**

### 4.1 明确「已完成」（文档 ✅ 或注释给出的具体一手地址）

| 项 | 一手判据 | 落点 |
| --- | --- | --- |
| 蓄力速率 = 10/帧、满 2500 用 250 帧 | owner 实测 4.167 s（`CARD:1268-1300`） | `CARD:6689-6695` |
| 蓄力音 = `se_ch00.wav`（序号 30 = `0x1e`） | `th18:0x4b76e0` 串 / `0x4b7504` 起 71 项名单 | `CARD:1296-1300` |
| 结界半径律 + 各角色 K（灵梦 64 / 魔理沙 14 / 咲夜 64 / 早苗 128） | `engine/versus/th19/06-aura.md` §7.2；`AuraCtrlInf` 槽 3 写缩放 | `CARD:1227-1237` |
| C1 灵梦 4 发弹表（`pl00.sht` 第 10 项、`pl00.anm` script10） | `DOC/HANDOFF-109-C1-C4.md:33-40` | `CARD:2041-2098` |
| C1 四发「转不起来」已修（脚本补 `type(1)` + `angleVel` 27°/帧） | `th18:0x47ed50` 有 `fsincos`、`0x47e8f0` 无 | `DOC/OWNER-FEEDBACK-109.md` §C2；脚本 427 |
| C2 灵梦 105 帧 = 通用 `Ecl_EtBreakLv2`（8/12/16/20，4 帧）+ 槽 4 环绕点 + 45–105 扩张环 128→192 | `th19:0x53b920` / `0x53b3a0` | `CARD:2189-2226` |
| TH09 CA 消弹环（L1/L2/L3 = 192/256/448） | `th09:0x403c30` / `0x403c90` / `0x403cf0` | `CARD:2724-2870`、`CARD:6795` |
| 罩子回罩 + 破罩三件事（40 帧禁移 / 100 帧无敌 / 反位移） | `th19:0x40b420`（唯二调用点 `0x4e7b8d`/`0x4e8176`）、`0x544c90`、`0x544d39` | `CARD:4144-4312`、`CARD:8639` |
| 暴风半径律 = TH19 `ins_1028`（小妖精 50 / 灵 80） | `DOC/HANDOFF-109-STORM.md` §0 | `CORE:41`/`CORE:72`、`CARD:6035` |
| 灵力道具 = TH16 季节道具动力（`script129`）+ TH18 `bullet/item.png` sprite519 | `DOC/MECHANICS-109.md:156` | `CORE` §E、`CARD:3085` |
| TH09 HUD 切图（框 sprite7 / 灰数字 8–17 / `Hit` 18 / 条 42 / 红数字 25–34） | `DOC/HANDOFF-109-TH09-FRONT.md` | `CARD:7878-7975` |
| 灵 = 自建 ECL 真敌人（`CESpirit109`，四族共用） | `DOC/SPEC-109-FUSION.md:120-121`、`:259` | `assets/ecl/spirit109.ecl.txt` |
| `on_delete`/`on_run_reset` 收干净所有自建 VM/池 | 铁律（`CLAUDE.md`） | `CARD:5483-5680` |

### 4.2 明确未完成 / 占位（❓ / TODO / 注释明说「占位」）

- **暴风画面仍不是一手**：脚本 **396** 是 `drawPoly` 程序化近似（`CARD:42-55` 明写「只能用观感近似…不许写成复刻」）。
  TH19 对象（`ghost.anm` script16→17）已核出但**未接**（`DOC/HANDOFF-109-STORM.md` §0/§1）。
- **C1/C2/C3/C4 观感全是脚本 396 占位**：`CARD:2061`、`CARD:2212`、`CARD:3686`、
  `CARD:3130`（`CARD:3129` 明写路线 = 纯伤害源 `ce_damage_rect` + VM）。
- **C1 触发非一手**：用「结界张开上升沿」代替一手「蓄力 ≥ C1 阈值」（`CARD:2051-2056`；
  `DOC/TUNING.md` C1 节）。`POFV_REI_C1` 值一手未解 ⇒ ❓（`CARD:1316`、`CARD:278`）。
- **C1 数据表只有灵梦**；魔理沙 / 早苗组标「待补 ❓」（`CARD:2068-2070`）。
- **C3/C4 多处 ❓ 自定**：档位固定 1、伤害借 C1 的 14、`%B = [-9888.0f]` 未定位取 0、
  寿命 600 兜底、每波奇偶贴图用尺寸近似、`ins_611` type 8 未解码不落地（`CARD:3684-3700`、`CARD:5081`）。
- **早苗 C4 的「三个 async 发射器」正解未做**（`CARD:6746-6752` 注释明说正解待做）。
- **罩子触发偏离一手**：一手是「C3/C4 才给盾」，本卡改挂结界上升沿（`CARD:4139`）；
  位移方向因回调无参数 ⇒ 用最近敌弹反推 ❓（`CARD:4186`、`CARD:8639-8650`）。
- **HUD 三件已撤下**（不是完成）：TH19 `hits` 大字 `CARD:7782`、TH19 攻击槽 `CARD:7790`、
  TH09 `Bonus!` `CARD:7799`；TH09 红数字 / 图标「已切入库、尚未接线」（`DOC/HANDOFF-109-HUD-STATE.md` §1）。
- **灵力道具原贴图未切**：现用 TH18 季节道具图，TH19 原图待切（`DOC/TUNING.md:2278`）。
- **`on_activate` 恒 `CE_ACTIVATE_REFUSED` 占位**（`CARD:8526`）；「去掉引擎拒绝音」的 `return 0`
  方案因实跑卡死**已回滚**（`CARD:8511-8528`）。
- **`on_shot_set_index` 现恒返 −1**（不再认领主炮 set；早苗改由 `pofv_emit_group()` 直接发弹），
  `POFV109_NATIVE_SETS` 开关保留（`CARD:8569-8593`、`CARD:8554`）。
- **EX 敌人本体（`ExAttack*`）尚未实现**，缺敌机名 / ECL ❓（`CARD:247`）；
  对侧 EX 攻击 = `pofv_foe_spirit_loop_exit()` **只留接口**，未调用（`CARD:334-340`、`CARD:6003`）。
- **TH09 续命模型 `s_z_th09_mode` 默认关、待接**（`CARD:319`）。
- **灵力道具「先留口搁置」处仍在**（`CARD:6218`）。
- **文件头 `CARD:1-7` 的进度注释已过期**：仍写「其余…一行都还没接」，与现状不符。
- **`DOC/CARDS.md:505-517`（2026-10-01）的「C3/C4 走 ECL 版底伪 boss 载体 `CEPofvBoss109`」已作废**：
  `assets/ecl/` 下只剩 `spirit109.ecl.txt`，无 `pofv109_boss.ecl.txt`；现实现 = 纯 C 伤害源 + VM。

### 4.3 不确定（不猜）

- 一手「结界被固灵之后的后果」两作不同（TH09 幽灵变火焰上飘→三发自机狙；TH19 四种灵到点自然成佛），
  本卡**两种都要做**，但「自机狙最终会去掉」⇒ 只是方向（`DOC/MECHANICS-109.md:103-111`）。
- 灵的同屏量上限、`animal_spirits.png` 第 4/5 行切法、暴风第三台 VM 的作用，
  文档均标 ❓（`DOC/MECHANICS-109.md:231-236`）。
- 「10 = 落地/寄存」该数哪个计数器：文档未给 ⇒ `CARD:374-377` 已**撤回**该门，只留在场数门。


## 3. 素材 B —— 109 的已知缺口与风险（只读报告原文）

# 109（花映塚，系统卡 id 109）已知缺口与风险清单 —— 只读盘点

> 仓库根 `/home/nipporita/renkolab`；实现文件 `mods/th18.v1.00a/card-expand/native/cards/pofv.c`（8691 行）。
> 材料：`pofv.c` 的注释与 `TODO`/`❓` 标记（`grep -c` 命中 275 行）+ 109 相关文档（`HANDOFF-109-*.md` / `MODEL-109-SPIRIT-MIRROR.md` / `AUDIT.md` §AO、§BE-109* / `PLAN-109-SPIRITS.md` / `SPEC-109-FUSION.md` / `TUNING.md`）+ 姊妹审计 `LIFECYCLE-AUDIT.md`。
> ⚠ **材料可得性**：`LIFECYCLE-AUDIT.md` 在**本工作区不存在**（`ls mods/th18.v1.00a/card-expand/` 无此文件；`grep -rln LIFECYCLE --include=*.md` 只命中 `TUNING.md`/engine 文件）。其 §2 的 109 行与 §4 的 F7e，本清单取自 P17 批次的同内容副本 `/tmp/p17-rows.md:1`、`/tmp/p17-fixlist.md:51`，并另行用代码一手逐条复核（复核结果见各条）。
> 行号写 `文件:行号`；一手出处写 `thNN:0x……`；不确定处写「不确定」。只读：未改仓库任何文件。
> 下面只写「缺口/风险 → 出处 → 后果 → 判定依据 → 处置方向（只写方向）」。

## 1. 指定四条（已逐条代码复核）

### 1.1 7 台 HUD VM 没有钩子收
- **缺口**：`n/m` 四字（标签/当前档/斜杠/上限档）+ 档位弹出三台（底图/标签/数字）共 **7 台卡自建 UI VM**，不在 `clear_card` 的清理面内。
- **站点**：句柄声明 `pofv.c:782`、`pofv.c:803`；生成点 `pofv.c:7509-7516`、`pofv.c:7625-7630`；唯一删点在「显示条件转假」分支 `pofv.c:7575-7578`、`pofv.c:7655-7657`；每帧句柄体检 `pofv.c:7814-7822`（`ce_anm_get_vm` 探活后**只清 0、不删 VM**，且依赖 tick）。
- **`clear_card` 覆盖**：`pofv.c:5483-5679` 逐台真删了 storm / power / aura / c1–c4 / 火花 / 蓄力槽等 VM，**没有这 7 个**（grep 7 个句柄的全部引用点确认）。
- **文档出处**：`LIFECYCLE-AUDIT.md` §2 109 行（副本 `/tmp/p17-rows.md:1`）、§4 F7e（`/tmp/p17-fixlist.md:51`）；本卡自己的立规 `AUDIT.md:4503`（AO8：「加 VM 时必须补进这里」）。
- **后果**：删卡两条路（倒吊人 / 原版空白卡「弃整副卡组」）都发生在商店里，那一刻 tick 全停 ⇒ 这 7 台 VM 无人删 ⇒ 「n/m」「Lv n」残影留在屏幕上直到本关结束；跨面时若引擎未回收它们，体检也只清句柄、VM 仍在。
- **判定依据**：一手（代码读证，7 个句柄引用点全部核过）+ 推断（残影能否被其它系统回收，未实跑）。
- **处置方向**：把 7 个句柄并入 `clear_card` 的真删组（照 `pofv.c:5616-5618` 同款），或把 `pofv.c:7814` 的体检表抽成「硬删全部 HUD VM」函数供两处共用。

### 1.2 `s_c4s` 没有 clear
- **缺口**：早苗 C4「扫动风扇」的状态结构 `s_c4s` **没有清理函数**，`clear_card` 也不碰它。
- **站点**：表与状态 `pofv.c:4596-4598`；fire `pofv.c:4627-4634`；tick `pofv.c:4636-4661`（仅 `timer > 3600` 时自停，`pofv.c:4661`）；`pofv_c4_clear()`（`pofv.c:4503-4517`）只清 `s_c4` / `s_c4_b[]`；`clear_card` 只调 `pofv_c4_clear()`（`pofv.c:5568`）⇒ `s_c4s.active/timer/wait/aim/tgt` 全部保留。
- **文档出处**：`LIFECYCLE-AUDIT.md` §2 109 行 / §4 F7e（副本 `/tmp/p17-rows.md:1`、`/tmp/p17-fixlist.md:51`）。
- **后果**：删卡/跨面后 `s_c4s.active` 仍为 1、相位保留；重新拿到卡后 `pofv_c4s_tick()`（调用点 `pofv.c:7068`）每帧无条件跑 ⇒ 在玩家再次按 C4（`pofv.c:6751`）之前，会从旧相位继续发射这只「幽灵风扇」。跨面亦违反 `MODEL-109-SPIRIT-MIRROR.md:111-114`「各类冷却 / 队列 / 待生成项一并归零」。
- **判定依据**：一手（代码读证）。
- **处置方向**：新增 `pofv_c4s_clear()` 并在 `clear_card` 调，或把 `s_c4s` 纳入 `pofv_c4_clear()`。

### 1.3 灵本体的真敌机两个钩子都不收
- **缺口**：灵 = 引擎真敌机（`ce_spawn_enemy`）。删卡 / 跨面时 `clear_card` **只清账与 VM，不喊走真敌机**。
- **站点**：生成 `pofv.c:8206` `ce_spawn_enemy("CESpirit109", sp)`；账 `pofv.c:1135-1137`；`clear_card` 的灵清理只删 VM + 清 `s_spirit_id/kind/...`（`pofv.c:5671-5677`）；写 `$I0=999` 的只有 tick 内两处 `pofv.c:6264`（沈静化成佛）与 `pofv.c:6320`（幽灵满 5000 帧）——tick 在删卡时已停。
- **文档出处**：ECL 侧注释 `assets/ecl/spirit109.ecl.txt:44` 明写「`$I0 = 999`（卡侧在**沈静化成佛 / 删卡 / 一波结束**时喊）」⇒ **「删卡」这半边在实现里没有落**（一手文档与代码不一致）。`LIFECYCLE-AUDIT.md` §2 109 行 / §4 F7e（副本同上）。
- **一手佐证（ECL 无自删）**：`assets/ecl/spirit109.ecl.txt:132`（kind 分支循环等 `$I0`）、`:189`/`:194`/`:198`（幽灵三条回边都只查 `$I0`）、`:203` 附近的 `delete()` 只在 `$I0=999` 之后；生成物同形见 `native/build/ecl/_ufo_tmp/merged.back.txt:88-140`。⇒ 卡侧不喊走，敌机不会自己退场。
- **后果**：删卡/跨面后，真敌机成为「无户口」实体：`s_spirit_id` 已清 ⇒ 卡侧再认不回它、不再结算、不再有跟随 VM（看不见），但它是否仍留在场上取决于引擎是否在该路径清敌人管理器 —— **不确定**（未核、未实跑）。
- **判定依据**：一手 ECL + 一手代码（写点缺失）+ 推断（引擎换关/重开是否清场未核）。
- **处置方向**：在 `clear_card` 里对每个 live `s_spirit_id` 写 `$I0=999`（走现成离场支），或调用 SDK 的敌人删除口；并给出实跑判据（删卡后 grep 账 / 敌机数）。

### 1.4 `MODEL-109` 的「跨面 ⇒ 全清」与 ctor 每关重跑的关系
- **模型口径**：`MODEL-109-SPIRIT-MIRROR.md:111-114` —— 归零点 = **跨面事件**（明写「不是 `clear_card`」，删卡与跨面是两件事）；清的是**值**，「只有引擎侧的句柄 / 注册不重建」。
- **实现口径**：`on_ctor`（`pofv.c:5683-5690`）无条件调 `clear_card`；`on_run_reset` / `on_delete`（`pofv.c:8477-8478`）与它**同一个函数**；虚表 `pofv.c:8679-8691` **没有 `on_stage_start`**。⇒ 「跨面全清」实际是靠「引擎每关重调 `+0x00 ctor`」这个**实跑事实**达成的（P17 实跑日志 `/tmp/p17-pr.md:13`、`th18_card_expand.log:509-513`）。
- **关系与风险**：① 跨面语义**没有显式站点**，证据链是实跑经验而非契约；若某条重开路径（退主菜单重进 / retry）只跑 `on_run_reset`（也调 `clear_card`，多半仍成立）或两者都不跑，语义即断。② 与 MODEL 措辞不一致：MODEL 说「句柄 / 注册不重建、只清值」，代码把句柄清零并**真删 VM**（如 `pofv.c:5616`、`pofv.c:5671`）⇒ 跨面时结界 / HUD 的 VM 相位从头重建（可见 1 帧闪烁或动画重启的风险），与「不重建」不同。③ MODEL 明说删卡 ≠ 跨面，代码里两者共用 `clear_card`。④ `on_ctor` 的日志仍写 "nothing else yet"（`pofv.c:5687`），与现实不符。
- **判定依据**：实跑（ctor 每关重跑）+ 一手文档（MODEL）+ 代码读证；②③④ 的可见后果为推断（未实跑）。
- **处置方向**：或补 `on_stage_start` 显式做跨面归零、把 ctor 的清理面收窄到「新拿到卡」，或更新 MODEL 措辞以反映「句柄清零 + 下一帧重建」的实现；并补「退主菜单重进 / retry」的实跑判据。

## 2. P17 清单里同族的第 4 条（补齐）

| 缺口 / 风险 | 出处 | 现在会有什么后果 | 判定依据 | 处置方向 |
| --- | --- | --- | --- | --- |
| 咲夜 `s_aura_sec_deg` **只增不清** | 声明 `pofv.c:222`；每帧 `+3`、上限 90 `pofv.c:6621-6623`；`clear_card` 无它 | 一旦张到 90° 就**永远** 90°，重新拿卡首帧即为满张角（半张角用点 `pofv.c:1483`） | 一手（代码读证） | 加进 `clear_card`（与 MODEL「全清」一致） |

## 3. 其它读到的 `❓` / `TODO`（按风险排序，非穷举；`pofv.c` 内共 275 行标记，`❓自定` 大多已登记 `TUNING.md`）

| 缺口 / 风险 | 站点或文档出处 | 现在会有什么后果 | 判定依据 | 处置方向 |
| --- | --- | --- | --- | --- |
| **EX 敌人本体未实现**（`ExAttack` / `ExAttackW/O/E`，HP 200/2400/4000） | `pofv.c:245-247` | EX 只涨计数 / 起光环，**不出实体** ⇒ 「EX 攻击」无实弹；`s_ex_pending` 之后无消费端 | 一手（文档自述缺敌机名/ECL） | 找到 EX 敌机名 / ECL 再落地，或明确留口 |
| `s_ex_level` 恒 0、难度→索引(0..7) 未定 | `pofv.c:245-246`、`pofv.c:263-264` | EX 阈值恒按等级 0（75），不随难度；局内不升 | 一手（引擎全局 `DAT_0062b250`）+ 自定 | 查 `DAT_0062b250` 的初值写点与难度映射 |
| `POFV_SP_RATE`：一手常量 `0x4a7e44` 未解出 | `pofv.c:490-494` | SP 系数暂取 1.0 ⇒ SP / `Bonus!` 计分口径可能偏 | 一手常量未解 | dump `0x4a7e44` 常量值后替换 |
| `Bonus!` 的 `!` 画不出、位置为近似 | `pofv.c:486-490`、`pofv.c:8020-8023` | 一手满值 999990 的提示只做到数字 | 一手（部分） | `th09` `ascii.anm` `sprite107` 已定位（`HANDOFF-109-HUD-STATE.md` §4），接线即可 |
| Bomb tween 的**键值与时长**未取到 | `pofv.c:21-27` | 暴风半径律仍借 TH09（r0 0 / dr 1.6 / 寿命 70 ⇒ 112，`th09:0x41fe93`）⇒ 形状与 TH19 一手可能不同 | 一手（部分）+ 推断 | 补 Bomb 模板参数后替换 |
| 暴风**第三台 VM（启动器）**语义未明 | `HANDOFF-109.md:107` 与 §6 | 三台 VM 中第三台到底启动什么不明 | 一手未核 | 继续 RE，或明确不做 |
| 卷环回调 `0x40dc70` 的**径向映射**没读出来 | `HANDOFF-109.md:107` | 卷环厚度只能近似 | 一手未核 | 补反汇编（`th09:0x40dc70`） |
| C1 数据表只做了灵梦一组 | `pofv.c:2065-2070` | 魔理沙 / 早苗的 C1 数值待补 | 一手未取 | 按 `pl01` / `pl02` 补表 |
| C1/C2 伤害源形状 / 寿命自定 | `pofv.c:2061`、`pofv.c:2146` | 伤害判定与一手可能不同 | 自定（已登记 TUNING） | owner 裁定 / 实跑校 |
| 早苗 C1/C2 改走**原生弹表**（新增 shooterset `0x68`/`0x69`） | `AUDIT.md:4874-4882` | 依赖共享生成器 `assets/sht/append_shooterset.py` ⇒ 共享件变更会波及所有卡 | 一手（owner 口径）+ 协作风险 | 共享件回归已跑；后续改生成器需再回归 |
| 早苗 C4S 为简化（只取一个目标角、±10° 窗语义不同） | `pofv.c:4645-4650` | 转向律与一手不同（已按审计 R15 如实登记） | 一手（审计登记） | 按一手那 8 档分支补齐 |
| `ins_618` / `ins_620` / `ins_316` 未解码；8 档 rank ↔ 难度未定 | `pofv.c:4591` | C4S 部分参数悬空 | 未解码 | 继续 RE |
| 咲夜领域 **anm 脚本未解出** ⇒ 暂用圆 | `pofv.c:1384-1391` | 外观非一手；张开律仍 🟡 | 一手（部分） | 解 `local/th09.v1.50a/dat/pl02.anm` script88 |
| 咲夜张开**时长 / 量纲**不明（一手 3.0 的语义） | `pofv.c:7134-7161` | 用 `3.0 × POFV_AURA_SAKUYA_UNIT(20)` 近似 | 一手未核 | 等一手核完再定，或 owner 拍 |
| HUD 蓄力槽「Lv1 标签」与「按 C 的飘散粒子」两处留口 | `pofv.c:870-881` | 规格 ②③ 未做 | 一手（规格）+ 缺贴图 | 补字形 / 粒子资产后再上 |
| TH19 `hits` 大字 / 攻击槽**已撤下**，scale 量纲未核 | `pofv.c:7794-7806`、`pofv.c:7796` | 攻击槽不显示（不发明） | 一手未核 | 核清原作 `scale(8,112)` 量纲后再上 |
| `zBullet +0x48` / `+0x4c` 语义未定位 | `pofv.c:1808-1830`、`pofv.c:1878` | 结界抖动默认关（`POFV_AURA_BULLET_SHAKE 0`） | 未定位 | RE 这两个字段 |
| 与 115/111/108 的**弹染色门字段冲突**（同一 `bullet+0x28`+color1） | `pofv.c:1811-1820`、`pofv.c:8486-8490` | 卡按 id 顺序跑 ⇒ 109 的 `on_draw` 先、111（id 更大）后 ⇒ 同弹同帧 111 的红压住 109 | 一手（顺序）+ 代码读证 | 登记已如实；与 111 作者协调或改引擎遍历（不在本卡权限内） |
| `s_aura_stamp` 复位值曾错过（0 → 判「按住 C」假阳） | `pofv.c:5556-5560` | 已修为 `-1` | 一手（代码） | 保持 |
| 安全阀阈值 20 为自定 | `pofv.c:234`、`pofv.c:330-345` | 观感旋钮；日志 `【安全阀】` 会打 | 自定（待核） | 实跑校 |
| 「本侧发 EX ⇒ 暂时不给对面灵」的 TODO 未接 | `pofv.c:339-343` | `pofv_foe_spirit_loop_exit_any` 未调用（`s_foe` 声明顺序挡住） | TODO（代码） | 前移 `s_foe` 声明或加访问口后接上 |
| 灵功能本体 `TODO(灵)` | `pofv.c:1128`、`pofv.c:5834` | 召唤处的灵本体行为未落 | TODO | 落地灵本体行为 |
| W6 消费侧 / W4 固化 / W7 EX / 比赛时长→cap 未做 | `PLAN-109-SPIRITS.md:316-325`、`:178-182` | 灵 / 结界链条不完整（现恒按 60 s 档） | 一手文档（计划） | 按 PLAN 继续 |
| 灵的种类 4/5 与 `animal_spirits.png` 第 4/5 行切图待核 | `HANDOFF-109.md:150-152` | 旧切法看着像**碎片** | 待核 | 重切并对照 |
| TH09 幽灵火焰形 / 超时三发自机狙 / 一手 5000 帧寿命 | `PLAN-109-SPIRITS.md:543`、`:552` | 外观与寿命口径与一手有别 | 一手（部分） | 切图 / ECL 侧落地（owner 已定自机狙最终去掉） |
| C4 的 **4 次反弹 + 左右墙**未做（只做一次版顶反弹） | `pofv.c:4747-4749`（一手 `etEx(type=64)`） | 运动为简化 | 一手（简化，已登记） | 补齐 4 次与左右墙 |
| 魔理沙 C3M/C4M 的 `len = 600` **无一手出处**、归属为 owner 定 | `pofv.c:4839`、`pofv.c:4950`、`pofv.c:5101` | 命中盒可能偏大；命中归属为自定 | 一手无 | owner 裁定后再改 |
| C3/C4 **发动瞬间的消弹**（`Ecl_EtBreakLv3`）未做 | `pofv.c:3696` | 一手发动瞬间的消弹圈缺失 | 一手（部分） | 补 `EtBreakLv3/4` |
| C3 弹寿命 / 加速时长 / `%B` 取 0 | `pofv.c:3716-3728` | 弹寿命与自转为近似 | 一手（部分） | 补一手参数 |
| C1 的 `class 5` 子机槽出膛暂用自机坐标 | `pofv.c:2078` | 出膛点与一手不同 | 自定（已登记） | 接子机槽口 |
| 结界减速系数 `POFV_AURA_SLOW 0.5` 一手只说「变慢」 | `pofv.c:1487` | 现值仅存档；owner 已裁定**去掉减速**（`$I2` 恒 0） | 一手（部分）+ owner | 已按 owner 处理，保持 |
| 调试日志总开关 `POFV_DBG = 0` | `pofv.c:82-93` | 发布态无消弹圈 / 灵日志 ⇒ 实跑定位需临时开宏重编 | 代码 | 保留可开关（现状即如此） |
| 文件头进度声明过期：`pofv.c:5` 仍写「其余（消弹场 CA / 罩子 / 结界 / 灵 / 道具 / HUD）**一行都还没接**」 | `pofv.c:5` | 与现状（这些均已接）矛盾，会误导读者 | 一手（代码读证） | 更新头部注释 |
| `on_ctor` 日志仍写 "nothing else yet" | `pofv.c:5687` | 与 `clear_card` 实做的大量清理不符 | 一手（代码读证） | 改文案 |
| `AUDIT.md §BE-109-BBALL`：`sprite(N)` 是全局号还是 entry 内局部号**未定** | `AUDIT.md:32330-32332` | C3/C4 阴阳玉的帧序 / `scale` 待定 | 待独立复核（agent `a84cf536`） | 等复核定案后按它改 |
| `AUDIT.md §BE-109-HUD20`：跟人字 layer 25→20，**暂停模糊表现会变** | `AUDIT.md:32340-32343` | 观感变化（代价已登记） | 一手（owner 定「照能正确显示的卡」） | owner 验收；不接受则回退这一笔 |
| 109 敌弹扫描收口（z65）**只提交、未发版** | `AUDIT.md:5107` | `pofv.c` 里有另一 agent 的在途改动 ⇒ 发版会连带打包 | 实跑 / AUDIT 登记 | 发版前从干净 worktree 构建（已有先例 `AUDIT.md:5109`） |
| 16 个原版未逆到的量：具名占位 + `❓自定` + `TODO(th09)` | `AUDIT.md:4504`（AO9） | 未包装成「复刻」（合规） | 一手（审计登记） | 逐条按 `SPEC-109-FUSION.md` §5.2 补一手 |

## 4. 材料与文档漂移（不影响玩法，但影响后续判读）

- `LIFECYCLE-AUDIT.md` **未落盘**：P17 的 PR 说明（`/tmp/p17-pr.md:6`）称「新增 `LIFECYCLE-AUDIT.md`」，但工作区与磁盘上均无此文件 ⇒ 只有其内容副本（`/tmp/p17-*.md`）可引；引用时勿当一手文档。
- `pofv.c:5` 的「一行都还没接」与 `pofv.c:5687` 的 "nothing else yet" 均为过期文案（见 §3）。
- `SPEC-109-FUSION.md:5` 仍写「第 2 步（灵力道具 / 固灵结界 / 灵）尚未开工」——与现状不符（疑似未随实现更新）。
- `pofv.c` 内 `❓自定` 大多已按纪律登记 `TUNING.md`；本清单不重复其判据，只指出与生命周期 / 一手不一致的项。


## 4. 素材 C —— 109 的跨卡 / 共享内容搬迁候选（只读报告原文）

# 卡 109（`mods/th18.v1.00a/card-expand/native/cards/pofv.c`）跨卡 / 共享内容清单（只读备料）

- 口径：只列「搬家」候选，不改 109 行为，不实际搬迁。判定三选一 —— ①别处已有各自实现 ②操作引擎/共享池的公共语义 ③109 只是第一个用户。
- 行号取自当前工作副本（`pofv.c` 共 8691 行）；一手出处写 `thNN:0x……` 或文档章节；不确定处显式标注。
- 不列入（已共享、无需搬）：`faith_scatter_from_enemy()`（`faith.h:310`，109 已在用 `pofv.c:5782/5816`）；`on_native_shot_suppress` / `on_shot_set_index` 钩子（`pofv.c:5724/8569`，走 `sdk.c` 的 `BP_ce_shot_set_index`）。

## 1. 跨卡在场查询
- 内容：`pofv_card_present(int want_id)`（`pofv.c:295`）+ 常量 `POFV_CARD_ID_106`（`pofv.c:2228`）。
- 为什么共享：①另一处实现 `faith.c:1883 faith_green_owner_present()`（同样"某卡在不在场"，走本卡注册表）；`sdk.c` 里同一段卡链表遍历另有 ~20 份（`sdk.c:1628`、`1753`、`2958`…）。②操作的是引擎卡链表公共语义（`CE_ABILITY_MGR()+CE_MGR_CARD_LIST_FIRST`，`engine.h:391`）。
- 建议去向：`sdk.h`/`sdk.c` 增公共 `ce_card_present(id)`（可加 `ce_card_any_present(ids,n)`）；`faith` 的内存表版本逐步退役。
- 搬迁风险：`sdk.c` 是共享件（多人同改）；109 现仅用于"有 106 ⇒ 开 TH09 Z 续命"（`pofv.c:317`），搬时必须一字不变；卡链表语义 ≠ faith 的注册表语义，不可并成一个函数。

## 2. 弹的几何字段 + "可消弹"判据 + 自扫消弹
- 内容：`POFV_BULLET_FLAG_COLLIDE/SCALED/SIZE_SCALE/RADIUS_OFF`（`pofv.c:113`）、`POFV_FN_BULLET_CANCEL 0x428e90` + 原型（`pofv.c:103`）、`cancel_white_in_circle()`（`pofv.c:1708`）、`pofv_is_white_bullet()`（`pofv_core.c:814`，声明 `pofv_core.h:623`）。
- 为什么共享：①同一组偏移各卡各写一份：`lolk.c:72-81`（`CE_BULLET_RADIUS/SIZE_SCALE/FLAG_*`）、`signal.c:1896-1901`（`SIGNAL_BULLET_*`）、`season.c:724/1454`、`mof.c:1008`；自扫池 + 调消弹也有第二处 `great_work.c:90`/`great_work.c:206`（`gw_fn_bullet_cancel_t` + `CE_BULLET_LIVE_FOR_EACH`）。②消弹是引擎公共语义（`th18:0x428e90 Bullet__cancel`；半径消弹 `th18:0x429370`，`engine.h:213`）。
- 建议去向：`engine.h` 统一 `CE_BULLET_*`；新增共享 `ce_bullet_cancel_where(pred)`（或给 `ce_cancel_radius` 加判据参数），`pofv_is_white_bullet` 随同搬。
- 搬迁风险：`engine.h`/`sdk.h` 是共享面，常量值必须不变（`0x2/0x40/0xf38/0x658`）；`cancel_white_in_circle` 里还捆着卡私有副作用（`s_aura_bullet_done[i]` 闩锁 + etbreak 火花，`pofv.c:1726`），只搬"扫+筛"、别搬副作用。

## 3. 弹 VM 颜色染色（多方争用同一字段）
- 内容：`POFV_BULLET_VM_OFF 0x28`（`pofv.c:1825`）、`s_aura_tint[]` 与 `pofv_aura_tint_g/color/restore_all/tick`（`pofv.c:1834`–`pofv.c:1976`）。
- 为什么共享：①第二、三处实现：`lolk.c:425-518`（`tint_mark`/`tint_restore_all`，115）、`signal.c:2033-2062`（`SIGNAL_REMILIA_TINT_*`，111）；`signal.c:2020` 明文"不做共享的弹染色层"。②写的是引擎 VM 公共字段（`vm+0x524` = `CE_ANM_VM_COLOR1`，`engine.h:307`；载体 `th19:0x4f5130` 那条）。
- 建议去向：新共享件（`cards/btint.{h,c}` 或 sdk 层）做"逐弹颜色认领 + 删卡即还原"的仲裁；115/111/109 改调。
- 搬迁风险：高——要动 115/111（别人的卡 ⇒ `AUDIT §BE` 登记）；落笔时机（`on_draw` vs `on_tick_2`、卡 id 顺序）已是被记录的未解决问题（`pofv.c:8480`–`pofv.c:8493`），搬动会改三家时序；109 的"A 字节是否一起写"取舍（`pofv.c:1820`）须保留。注：`pofv.c:1812` 称"108 也写这一格"，本次 grep 未见 `animal.c` 直写 `+0x524` ⇒ **不确定**。

## 4. 卡属敌机台账（id → 用途/等级/VM）+ 死亡栈延后生成队列
- 内容：`s_spirit_id/kind/level/vm/body`、`pofv_note_spirit/slot_free/take/is_ours/live_count_fam`（`pofv.c:1515`、`pofv.c:1584`–`pofv.c:1651`）；入队侧 `s_spirit_q*`（`pofv.c:183`）。
- 为什么共享：①同类能力另一处 `faith.c:1445 gf_is_ours()`；SDK 已有 `ce_fairy_claim_instance`/`ce_fairy_instance_claimed`（`sdk.h:1148`）但属**逐帧重申**语义，109 要的是**持久到死亡**的账。②"死亡回调只入队、帧末才 `ce_spawn_enemy`"这条纪律被各卡各实现：`ufo.c:1306-1329`（`pend_drop`）、`trance_core.h:141`、`star_cannon.c:182`、`signal.c:639/947`。
- 建议去向：`sdk_core.h`/`sdk.h` 收"卡属敌机台账"（id → 卡私有 tag，死亡自动摘除）+ "死亡事件延后队列"（有界环、溢出丢尾照 `trance_core.h:146`）。
- 搬迁风险：共享面；109 台账还兼存本体/附属 VM 句柄（`pofv.c:1617`、`pofv.c:5846`），迁移时 VM 生命周期须一并带走，否则重演"常驻 VM 残留"（`pofv.c:5842`）。

## 5. 卡侧直发原生自机弹
- 内容：`POFV_FN_PLAYER_SHOOT_ONE_BULLET 0x45e930` + `pofv_shoot_one_fn_t` + `pofv_emit_row/emit_group`（`pofv.c:3464`–`pofv.c:3484`）。
- 为什么共享：①第二处 `animal.c:400-402`（`CE_FN_PLAYER_SHOOT_ONE_BULLET`）+ `animal.c:2845 sakuya_eagle_shoot()`——同一地址、同一 thiscall/`ret 0x10` 原型各写一遍；`signal.c:2701` 也依赖它。②走引擎公共发射路径（`th18:0x45e930`），SDK 相邻设施只有 `ce_tick_shooters_for_card`（`sdk.h:224`）。
- 建议去向：`sdk.h` 内联 `ce_shoot_one_bullet(packed_id,timer,pos,option)`（或 `ce_emit_row(set,row,pos)`），`engine.h` 收 `CE_FN_PLAYER_SHOOT_ONE_BULLET`。
- 搬迁风险：纯新增内联、无行为变化；但 `animal.c` 也在用同一原型，改成共享即动别人的卡（`AUDIT §BE`）；`packed_id=(set<<8)|row` 口径要写进共享件注释。

## 6. 输入块（Z 位）的合成 / 阻断
- 内容：`*(uint32_t*)CE_ADDR_INPUT_HELD |= 1` 与 `&= ~1`（`pofv.c:7026`、`pofv.c:7043`）、`s_z_playhead`（`pofv.c:318`）。
- 为什么共享：①别的站点 `animal.c:2681`/`2904` 读同一位；`season.c:76` 明文"同仓先例 `cards/pofv.c` 的 `POFV_INPUT_SHOT`"另写一份；`youkai.c:1196-1213` 因 109 清位而**被迫自建 `s_z_bridge`**（跨卡互作已发生）；且仓里本有共享件 `cards/input_synth.{h,c}`（`input_synth.h:1`，现处"已拆线"态）。②输入块 `th18:0x4ca428` 是引擎/回放的公共语义（`sdk.h:831` 注：这些键不进 replay 流）。
- 建议去向：`sdk.h` 共享"读/写/屏蔽输入位"口（或复活 `input_synth.c`），集中登记谁在屏蔽。
- 搬迁风险：写全局输入位影响所有卡与子机（`youkai.c:1199` 即为证据），并牵到录像录制（`pofv.c:5722` 记录过副作用）；搬迁不得改"仅蓄力期间清 bit0"的现有语义，否则 108 的桥失效。

## 7. 弹池按几何形状扫（碰 / 最近 / 擦弹）
- 内容：`aura_scan_bullets()`（`pofv.c:1751`）、`pofv_barrier_nearest_bullet()`（`pofv.c:4192`）、`pofv_graze_scan_drop()`（`pofv.c:1422`）——三处各自重写 state/flags/半径/取坐标。
- 为什么共享：①同一能力另一处 `lolk.c:513` 用共享迭代宏 `CE_BULLET_LIVE_FOR_EACH` 做同一件事，`great_work.c:206` 已从池扫迁到活弹链；109 因需"槽下标闩锁"仍留池扫（`pofv.c:1429/1715/1758` 注释）。②弹池/活弹链是引擎公共语义（`sdk.h` 的 `CE_BULLET_POOL_FOR_EACH_IDX` / `CE_BULLET_LIVE_FOR_EACH`）。
- 建议去向：`sdk.h` 补"带槽号/带弹指针 + 判据回调"的共享遍历（如 `ce_bullet_each(pos,r,ctx)`），统一池扫与活弹链两种口味。
- 搬迁风险：109 闩锁按下标（`s_aura_bullet_done[i]`），共享口必须能给出槽下标或稳定身份，否则等于改行为；三处扫描同帧内互相依赖次序（`pofv.c:1731`），搬动须保持调用序。

## 8. 场地矩形常量 + 裁到场地内的伤害源
- 内容：`POFV_FIELD_HALF_W 192.0f` / `POFV_FIELD_H 448.0f`（`pofv.c:159`、`pofv.c:163`）、`storm_hit_clipped()`（`pofv.c:1980`）。
- 为什么共享：①同一数值另一处 `bomb10.h:119-122`（`BOMB10_FIELD_W/H/CX/CY`，`bomb10.c:669` 也在裁）、`mof_core.h:96`（`MOF_BOARD_Y 448.0f`）、`sdk.h:1151-1156` 直接写"场地 = x∈[-192,192]、y∈[0,448]"。②是引擎游玩区几何（公共语义），伤害源是共享设施 `ce_damage_rect`（`sdk.h:172`）。
- 建议去向：`engine.h`（或 `sdk.h`）收 `CE_FIELD_HALF_W/CE_FIELD_H`；`sdk.h` 加 `ce_damage_rect_clipped(...)` / `ce_rect_clip_field()`。
- 搬迁风险：只加常量/内联，风险低；但 `bomb10`/`mof` 若跟着改名会动别人的卡（`AUDIT §BE`）；109 把"裁剪"当防误杀手段（`pofv.c:157`），语义须保住。

## 9. "探活再删"的 VM 收尾
- 内容：`power_vm_drop()`（`pofv.c:3053`）+ 本文件 27 处 `if (id && ce_anm_get_vm(id)) ce_anm_delete(id)`。
- 为什么共享：①各卡各写包装：`senro.c:626 senro_hud_vm_kill`、`ufo.c:1487/1496/1985`、`youkai.c:1552/2028`；全仓 77 处 `ce_anm_get_vm((uint32_t)`（109 占 27、trance 18、animal 14、season 12）。②处理的是引擎 anm 句柄回收（`sdk.h:240/393`；`sdk.h:240` 注明陈旧 id 直删 = 误删别人的 VM）。
- 建议去向：`sdk.h` 一个内联 `ce_anm_kill(uint32_t id)`（0 安全 + 探活 + 删），各卡 `*_vm_kill` 退化为薄壳。
- 搬迁风险：纯内联、行为等价；`sdk.h` 属共享面。注意部分站点在"探活失败"时也要清句柄，内联需覆盖两种写法。

## 10. HUD 数字拆位 + 字形 VM 摆位
- 内容：`ipow10()`（`pofv.c:768`）+ hits/SP 两个拆位循环（`pofv.c:7901`、`pofv.c:7950`）。
- 为什么共享：①同一能力另有三处实现：`faith_hud_core.c:39 faith_hud_digits6`、`youkai_core.c:1939 yk_popup_digits`、`ufo.c:4482 ufo_panel_digits`（各自手写 ÷10 数位循环）；SDK 也已给 `ce_hud_digit_t`/`ce_hud_number_t`（`sdk.h:757/759`、`sdk.c:718/751`），109 一半用它、一半自建。②HUD 通用能力，非 109 概念。
- 建议去向：`sdk_core.h` 收纯逻辑 `ce_hud_digits_split(value, slots, out[])`（可主机单测），摆位走现成 `ce_hud_vm_text_t`（`sdk.h:798`）或 `ce_hud_number_set`。
- 搬迁风险：低；须用参数表达 109 的格式怪癖（"固定 3 格 + 前导留空"、SP 6 格零填充，`pofv.c:767/7946`），共享件不可定死。

## 11. 无 x87 的方向 / 三角查表
- 内容：`s_c2_dir36[36][2]`（`pofv.c:2331`）+ `c3_dir5()`（`pofv.c:3802`）+ 内联 8 向表（`pofv.c:6613`、`pofv.c:7329`）。
- 为什么共享：①同一能力各卡各备：`animal.c:2831-2840`（`sakuya_sin_small/cos_small`）、`ufo.c:3122`"不许算三角函数 ⇒ 写成常量"、`pofv_core.c:830`（`s_power_dir[8][2]`）。②"零 x87"是所有卡共同的硬门（`pofv.c:3133/3801` 明文），属共享约束下的公共资产。
- 建议去向：`engine.h`/`sdk.h` 提供只读方向表（如 `ce_dir_unit_x100(i,n)`），各卡共用。
- 搬迁风险：低（只加只读资产）；但各家方向数与角度起点不同（36 向起点 0°、8 向语义各异），共享件要给"起点 + 旋向"参数，不能假设统一。

## 12. 保命 / 罩子的整段编排
- 内容：`pofv_on_death_before_deathbomb()`（`pofv.c:8639`）+ `pofv_barrier_*`（`pofv.c:4192`–`pofv.c:4330`）。
- 为什么共享：①同一能力的另一处 = `signal.c:911-960` / `signal.c:1236-1260` 的河童罩子（同样挂 `+0x10`、同样 `bomb10_cancel_impending_death()` + `ce_invuln_request` + 破罩演出）；107/117/113 的救援层也走同一协议（`sdk_core.h:503/514/520`，`trance_core.h:513`）。②走的是引擎"中弹瞬间广播 + 撤销中弹 + 无敌声明"公共协议（`th18:0x45d590 Player__cancel_impending_death`）。
- 建议去向：把"这一次中弹由罩子吃掉"的整段（guard → 认领 → 撤销 → 无敌 → 反方向位移）收进共享件；`bomb10.h` 已有 `bomb10_cancel_impending_death()`，可扩成 `bomb10_absorb_hit(...)`。
- 搬迁风险：`bomb10.h`/`signal.c` 是共享/别人的面；109 的破罩演出与 111 **有意不同**（`pofv.c:8611`：一手是常驻 VM 重放、本卡每 8 帧重发 ⇒ 另起三台爆发），共享化时须保留 109 的取舍，不可强并入 111 那套。

---

## 5. 复现与出处

- 素材由 **一卡一 agent 的只读 workflow** 产出（P19 批次 ✓）：三份分别落在 `/tmp/p19/{state,gaps,crosscard}.md` ✓；
- 相关的姊妹台账：`LIFECYCLE-AUDIT.md`（109 那一行 + §4 的 F 清单 ✓）、`CARD-BEHAVIOR-AUDIT.md`（§109 ✓）、
  `MAXPOINT-AUDIT.md`（共享 PIV 池 ✓）、`VM-IDIOM-MAP.md`（VM 直写普查 ✓）；
- 109 的既有设计文档：`MECHANICS-109.md` / `MODEL-109-SPIRIT-MIRROR.md` / `HANDOFF-109-*.md` / `AUDIT-109*` ✓。
