# 青眼白龙（card-expand id 67）—— 设计

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 日期：2026-09-04。状态：用户已审阅（2026-09-04，追加「不抬伤害上限」；**2026-09-06 用户改定：抬**——见 §1.3 末尾）。归属：`mods/th18.v1.00a/card-expand/`（行为 SDK 第二批）。

## 0. 一句话

主动卡：按 C 献祭一条残机，召唤一条跟着自机的青眼白龙。龙有 2500 点生命，替玩家挡子弹，每挡一发 −1；
每 5 秒向上喷一道「毁灭的喷射白光」，路径上的敌人共吃 3000 点伤害；生命归零即死。过关龙消失。

用户已定的四个选择（2026-09-04）：**跟随玩家悬在上方** / **光束打路径上的敌人** / **主动卡、过关消失** /
**挡下的弹照炸弹消弹变点道具**。追加：**光束不消弹**；美术**先全用占位图**。

## 1. 原生原型与一手事实

三块机制各有原生原型，本节全部来自本会话 headless 反编译（`tooling/ghidra/scripts/decompile_funcs.py`，新加），
**一律 🟡，进 AUDIT 前不算成立**。

| 机制 | 原生原型 | 详见 |
| --- | --- | --- |
| 献祭残机 | 神之宣告（66） | `CURRENT_LIVES` −1 + `ce_gui_update_lives()`，已实装 |
| 跟随 | Tenshi 要石（43）`0x40e8c0` | §1.1 |
| 挡弹 + 计数 | 同上 | §1.2 |
| 光束伤害 | Remilia 脉冲（46）`0x40f3a0` state 2 | §1.3 |
| 伤害结算 | `enm_compute_damage_sources` `0x45f0f0` | §1.4 |
| 收场 | Tenshi | `AnmManager__interrupt_tree(id, 1)` `0x488be0` 触发脚本里的中断分支（收场动画）；`0x488cf0` 直接标记删除 |

### 1.1 跟随（Tenshi）

目标 = 玩家 `(x, y − 80)`，每帧 `pos += (target − pos) × 0.04`（`0x4b90b8`）。
`AnmManager__get_vm_with_id(ANM_MANAGER_PTR, id)` `0x488b40` 找到 VM 后写 `vm+0x5f0/+0x5f4/+0x5f8`。召唤点 `(x, y − 100)`。

### 1.2 挡弹 + 计数（Tenshi）

每帧 `mgr.counter = 0; BulletManager__cancel_radius_as_bomb(pos*, 0, 99999, 0)`，**半径走 XMM2**（要石 18.0 `0x4b9290`）。
函数 `0x429370` stdcall `ret 0x10`：命中条件 `state ∈ {1,2}` 且 `bullet+0x24 == 0`，`dist² ≤ (弹半径(+0x658)×0.5 + R)²`；
每消一发 `Bullet__cancel(b, mode)` `0x428e90` 并 `mgr+0x7a41e8`（`__some_cancel_related_counter`）`++`；**消满 `max_count` 立即返回**。
有命中那帧 Tenshi 把 VM `color_1`（`vm+0x524`）写成 `0xff0080ff`，否则 `0xffffffff`。

### 1.3 光束伤害（Remilia）

`Player__create_damage_source_rect`（`FUN_0045dfa0`，本仓命名）`(center*, angle, 寿命帧, 每次伤害)` stdcall `ret 0x10`，
**XMM2 = 宽、XMM3 = 高**；Remilia 每帧 `(玩家上方, 0.0, 2, 200)`、宽 32、高 ≤ 90。
池 `player+0x20574` 起 1024 槽、stride 0x9c；矩形模式 flags `&~6|1`，`+0xc` 角度、`+0x14/+0x18` 宽高、`+0x7c` 累计上限 9999999、`+0x80` 命中间隔 1。

### 1.4 伤害结算

`enm_compute_damage_sources` `0x45f0f0`：敌人侧每帧遍历池、矩形按 OBB 判；**本帧总伤害钳 `player+0x47984`**，
`GameThread__on_tick` `0x443d3b` 每帧从 `sht+0x28`（max_dmg）复位它；Remilia 脉冲期间每帧写 300（`0x40f48e`）——本卡**不写**。
结算后 `SCORE += (dmg/10+10)/10`。

同类但不用的：`LaserManager__cancel_in_radius` `0x449010`（激光不挡）、`BulletManager__cancel_bullets_in_rectangle_as_bomb`
`0x4294b0`（光束不消弹）、`Player__create_damage_source_45de40`（圆形伤害源，Miko 用）。

## 2. 行为

### 2.1 数值（放 `blue_eyes.c` 顶部一处常量，实跑后调）

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `BE_HP` | 1500 | 生命 = 能挡的子弹数（初版 2500，2026-09-04 平衡下调）|
| `BE_RADIUS` | 48.0 | 挡弹半径（要石 18；龙大） |
| `BE_FOLLOW_DY` | −80.0 | 跟随目标相对玩家的 y 偏移（要石同款） |
| `BE_FOLLOW_LERP` | 0.04 | 跟随插值系数（要石同款） |
| `BE_WAVE_PERIOD` | 300 | 发波周期（帧）= 5 s；第一波在召唤后 `BE_WAVE_PERIOD` 帧 |
| `BE_WAVE_FRAMES` | 45 | 一波持续帧数（初版 30，2026-09-04 平衡 ×1.5）|
| `BE_WAVE_DMG_PER_FRAME` | 100 | 每帧请求伤害；30 × 100 = **3000** 是名义值。~~不改 `player+0x47984`~~ **2026-09-06 改定：光束帧抬到 300**（`ce_raise_damage_cap`，炎魔 / Remilia 同款）——自机弹与卡的源共用「每敌每帧」配额，不抬时自机开火光束几乎白打；抬后一波实打 45 × 100 = 4500 |
| `BE_BEAM_WIDTH` | 32.0 | 光束宽（Remilia 同款） |
| `BE_RECHARGE` | 600 | 充能帧数（10 s）；真正的成本是残机 |

### 2.2 状态（私有状态 `be_state_t`）

`{ uint32_t anm_id; int32_t hp; uint32_t wave_timer; uint32_t wave_left; float x, y, z; uint32_t hit_flash; }`

### 2.3 事件

**`on_activate`**：`CURRENT_LIVES < 1` → `CE_ACTIVATE_REFUSED`（无效音 0x10；**不允许献祭最后一条命**）。
否则 `CURRENT_LIVES−−`、`ce_gui_update_lives()`；`pos = (玩家 x, 玩家 y − 100)`；起龙 ANM（`ability.anm` 追加脚本
`BLUE_EYES_DRAGON`，层 13 照要石）、写坐标；`hp = BE_HP`、`wave_timer = 0`；发动音 0x4d；返回 1 进持续态。

**`on_active_tick`**，每帧四步：

1. 跟随：目标 `(px, py + BE_FOLLOW_DY)`，lerp，写 `vm+0x5f0`。VM 没了 → 视为死亡，返回 0。
2. 挡弹：`counter = 0; cancel_radius(pos, BE_RADIUS, max = hp, mode 0)`；`hp −= counter`；`counter > 0` 时染色一帧。
3. 发波：`wave_timer++`；到 `BE_WAVE_PERIOD` 时 `wave_left = BE_WAVE_FRAMES`、起光束 ANM（`BLUE_EYES_BEAM`，随龙的 x、从龙口到区域顶边）、放音。
   `wave_left > 0` 时每帧 `damage_rect(center = (x, y/2), angle 0, life 2, dmg BE_WAVE_DMG_PER_FRAME, w BE_BEAM_WIDTH, h = y)`，`wave_left−−`。
   ~~不写伤害上限~~ 2026-09-06 起每个光束帧先 `ce_raise_damage_cap(300)`（GameThread 每帧复位，只对当帧有效；理由与时序见炎魔 spec §2.3、AUDIT V20）。
4. `hp ≤ 0` → `interrupt_tree(anm_id, 1)`（死亡动画）、放音、返回 0。

**`on_stage_start`（+0x34）/ `on_run_reset`（+0x4c）**：龙在 → 删 VM（`0x488cf0`）、状态清零（用户选「过关消失」；SDK 已把状态机置 0）。

龙活着期间 SDK 状态机在持续态：C 键天然无效、充能不走，不需要「已有一条龙」的额外判断。
死亡后进收尾 → 空闲，充能走完可再召（再花一条命）。

坐标系：伤害源与消弹用的是**玩家坐标系**（`player+0x620` 那套，Tenshi/Remilia 直接拿它喂两个函数），
ANM 实体坐标 `vm+0x5f0` 也是它（三张原生卡都直接拷）。区域顶边 y 的取法与「光束高 = 龙 y」的假设在实跑时核对（🟡：玩家坐标 y 是否从区域顶部起算——
`engine/anm/th18/01-vm-instantiate.md` §3 说 ECL 坐标 (0,0) 是上边框中点；若玩家坐标同源则龙 y 就是到顶边的距离）。

### 2.4 replay / 确定性

无自带随机；消弹、伤害、计时全走引擎与帧计数。与原生主动卡一致。

### 2.5 不做的

激光不挡、光束不消弹；龙不跨关；不做「同时多条龙」；不动 `bullet+0x24 != 0`（不可消）的弹——它们穿过龙，与原生炸弹一致。

## 3. SDK / 引擎层改动

| 改动 | 内容 | AUDIT |
| --- | --- | --- |
| `ce_cancel_radius(pos, r, max, mode)` | `0x429370` stdcall + XMM2，内联汇编照 `ce_play_sound`；返回 `mgr.counter`（调用前清零） | 新条目：四个 `ret 0x10` 出口、XMM2 的确是半径、counter 偏移从 ExpHP 结构对回 |
| `ce_damage_rect(center, angle, life, dmg, w, h)` | `FUN_0045dfa0` stdcall `ret 0x10` + XMM2/XMM3 | 新条目：出口、两个 XMM 的落点 `+0x14/+0x18`、flags |
| `ce_anm_set_pos(id, x, y, z)` | `0x488b40`（调用约定待定：Tenshi `push [esi+0x1c]` 前 ecx = `ANM_MANAGER_PTR`，看着是 thiscall(mgr; id)）→ `vm+0x5f0` | 新条目：cc 与 `ret N` |
| `ce_anm_interrupt(id, n)` | `0x488be0` | 新条目 |
| `ce_anm_set_color(id, rgba)` | `vm+0x524`（`color_1`，Tenshi `0x40eb4c` 无命中写 `0xffffffff`、有命中 `0x40eb60` 写 `0xff0080ff`） | 同上 |
| `engine.h` | `CE_ADDR_ANM_MANAGER_PTR 0x51f65c`、`CE_PLAYER_DAMAGE_CAP 0x47984`（只读，日志用）、`CE_BM_CANCEL_COUNTER 0x7a41e8`（ExpHP `__some_cancel_related_counter`；Tenshi `0x40eb56` 读）、伤害源池 `0x20574 / 0x9c / 0x400` | — |
| **私有状态与主动卡状态机同槽** | `ce_active_t` 与卡的 `ce_state(c, T)` 用同一把键拿同一块 256 字节 → 互相踩。改：块头固定放 `ce_active_t`，`ce_state` 从 `sizeof(ce_active_t)` 起给卡（一把键、一次 free）；现有卡（10♠ 非主动、反转/神之宣告无状态）不受影响 | SDK §4 补一句 |
| 一手文档 | `engine/player/th18/02-damage-sources.md`：th18 伤害源管线（对齐 `engine/sht/th16/08`），含池布局、两个 create、结算与每帧上限 | `check-docs.py` |

## 4. 数据与资产

- `patch/th18/cards.js`：`"67"`，`category: 0`（主动）、`price_tier` 高档（同神之宣告）、`deck_visible: 1`、文案（`％` 不用 ASCII）。
- `abcard.anm`：卡图 `BLUE_EYES` `_max/_min` 占位（`fit_card.py`，先用程序生成的占位图）。
- `ability.anm`（`assets/ability/`）：`BLUE_EYES_DRAGON`（循环待机、中断分支 1 = 死亡淡出后 delete）、`BLUE_EYES_BEAM`（一次性，`BE_WAVE_FRAMES` 帧自灭）。贴图先用占位色块，`build_ability.py` 出脚本号到 `anm_ids.h`。
- `cards_dev.js` 起手卡组加 67；`CARDS.md` 补一行（致敬・游戏王）；`MAP.md` 追溯表补。

## 5. 验收（`_test` + devstage）

日志：`sdk: 67 bound (.active_recharge = 600, .on_activate = on_activate, .on_active_tick = on_active_tick)`。
关卡里按 C：`blue_eyes: summoned, lives 3 -> 2, hp 2500, anm id …` → 龙跟在自机上方；弹进半径变点道具，
每 60 帧一行 `blue_eyes: hp 2500 -> N`；第 300 帧 `blue_eyes: wave 1 start` → 龙口向上一道光束，boss 血条掉一截
（devstage 血量 ÷100，一波就死，正式弹幕看血条）；`hp ≤ 0` → `blue_eyes: died after F frames, W waves` + 死亡动画；
残机 0 时按 C → `blue_eyes: refused (no lives)` + 无效音、充能条不动。过关 → `blue_eyes: dismissed (stage start)`。
崩溃优先怀疑：三个新引擎调用的 `ret N` / XMM 约定、`get_vm_with_id` 的 cc、私有状态同槽改法。

## 6. 开放问题

1. ~~伤害上限~~ 已定：不抬。实跑记一下 `sht+0x28`（每机体的 max_dmg）与每波实际掉血，决定 3000 名义值要不要改。
2. 光束高度 = 龙 y 的坐标假设（§2.3）。
3. `bullet+0x24` 的语义（推测：不可消弹标记）——查 `Bullet__cancel` 与 ECL 置位处，进 AUDIT。
4. 平衡：3000 / 5 s 约等于每波一张符卡；数值全在 §2.1。

## 附记（2026-09-06）

龙加了与炎魔之王同款的**呼吸浮动**：画面 y = 跟随位置 y + `be_bob_dy`（三角相位过 smoothstep 折成来回，±5 px、96 帧一个来回，不引 libm、头里 `static inline` 守零 x87）。
挡弹判定、光束起点（龙头）、血条都用同一个浮动值，与画面一致。跟随的 lerp 目标不受影响（浮动只叠在显示 / 判定上）。
