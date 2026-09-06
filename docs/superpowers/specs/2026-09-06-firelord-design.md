# 炎魔之王拉格纳罗斯（card-expand id 72）—— 设计

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 日期：2026-09-06。状态：用户已定（§0）。归属：`mods/th18.v1.00a/card-expand/`。
> 参照：[`2026-09-04-blue-eyes-design.md`](2026-09-04-blue-eyes-design.md)（召唤物 / 挡弹 / 血条）、
> [`2026-09-05-broken-core-design.md`](2026-09-05-broken-core-design.md)（定点伤害源 / 敌人链表 / 确定性算术）。
> 引擎一手：`CardTsukasa__c_press` `0x410e60`（扣火力三步）、`GlobalsInner__spend_power` `0x457480`、
> `Player__repopulate_options_and_notify_cards` `0x45d5e0`、`Rng__rand_dword` `0x402740`。

## 0. 一句话

**主动卡**：按 C 消耗 2.00 火力，召唤一尊**不跟随自机**的炎魔之王。800 点生命替玩家挡子弹；每 8 秒滑到一个随机落点，
到位那帧向场上**随机一个**敌人投一颗火球（飞行物，落地爆炸 400）。生命归零死亡，过关消失。

用户已定（2026-09-06）：设计参照青眼白龙；**火球本身要有特效渲染**；卡图与本体是用户提供的美术
（`cards/_src/FIRELORD.png`、`ability/firelord/_src/FIRELORD_TOPDOWN.png`）；召唤 / 攻击各一条用户语音
（`voice/_src/FIRELORD_*.ogg`），火球爆炸仍用零售 `0x2c`。

## 1. 与青眼的差别

| | 青眼白龙（67） | 炎魔之王（72） |
| --- | --- | --- |
| 代价 | 1 残机 | **2.00 火力**（门槛 3.00，§2.1）|
| 位置 | 跟随自机上方 80 px | **不跟随**：每 420 帧（7 s）在弹幕区**下 1/3** 抽一个随机落点，60 帧 quintic ease-in-out 滑过去；待命时上下呼吸浮动（±4 px、96 帧）|
| 攻击 | 每 5 s 向上一道 45 帧光束（矩形伤害源每帧一个）| 到位那帧向**随机一个**敌人投火球：直线飞向开火时记下的坐标（4 px/帧，飞行中撒橙白粒子），落地连续 12 帧、每帧一个 96×96 伤害源 × 50，加火星 + 小震屏 |
| 生命 / 挡弹 | 1500 / 半径 48 | 800 / 半径 33.6（本体约 64 px 高；2026-09-06 用户：先减半到 28 再 +20％）|
| 随机 | 无 | 游戏 `REPLAY_SAFE_RNG`（落点 + 目标）|
| 语音 | 无 | 召唤 `FIRELORD_SUMMON`（叠在 `0x4d` 上）、投球 `FIRELORD_ATTACK` |

## 2. 机制

### 2.1 扣火力（照 Tsukasa）

`CardTsukasa__c_press` `0x410e60` 是零售唯一一张「按 C 扣火力」的卡，三步照抄：

1. **门槛**：`CURRENT_POWER >= 成本 + 一档`。Tsukasa 成本 1 档、门槛 2 档（`0x410e6d`：`eax = [0x4ccd40] * 2; cmp [0x4ccd38], eax; jl → 0x10 无效音`）。
   我们成本 200、门槛 300。理由：`spend_power` **永远保留 1.00**（`0x45749f`：扣完 `< 一档` 就钳回一档；`<= 一档` 时直接拒绝不扣），
   只要求 ≥ 2.00 的话 2.50 时实际只扣 1.50，「消耗 2.00」就不老实。
2. **扣**：`GlobalsInner__spend_power(&GlobalsInner, 200)`（thiscall + 1 栈参 `ret 4`，返回档数是否变了）。
3. **重建子机**：`Player__repopulate_options_and_notify_cards`（this = `PLAYER+0x620`，一个哑栈参，`ret 4`）——Tsukasa 扣完**无条件**调，我们照做
   （扣 2 档时档数必变）。它顺手把 `on_power_level_change` 广播给所有卡（装备卡在此重建自己的子机，与零售一致）。

**登场序列**（2026-09-06 用户）：按 C 那帧扣火力、起召唤语音，本体**还不出现**；接下来 60 帧在目标位置每帧起 6 颗 script98 聚气粒子
（±64 px 内随机起点、20 帧加速汇聚到中心、橙红变白热）；第 60 帧本体出现 + 血条 + 第一圈爆炸（`impact_fx`：光环 + 10 颗火星 + 震屏 14 帧）+ 发动音 `0x4d`；
之后每 8 帧再一圈光环 + 轻震（7 帧、3 px），共 3 圈。聚气期间不挡弹、不计帧、不移动；过关 / 局末在聚气中也照常撤销。
火力不足 → `0x10` 无效音、`CE_ACTIVATE_REFUSED`（充能退回），与青眼「残机 0 拒绝」同一条路。

### 2.2 移动

召唤在自机上方 64 px（钳进落点范围）。每 420 帧 `fl_step` 报 `need_move` → C 取一个 `ce_rand()` 喂 `fl_begin_move`：
低 16 位 → x ∈ [−160, 160)、高 16 位 → y ∈ [300, 410)（弹幕区**下 1/3**，区高 448；离底边留 38 px 给本体半高 + 血条。实体坐标，y 从顶边起算）。
60 帧 quintic ease-in-out（smootherstep 6t⁵ − 15t⁴ + 10t³）插值，到位那帧报 `fire`。
**呼吸浮动**（2026-09-06 用户）：画面 y = 逻辑 y + `fl_bob_dy`（三角相位过 smoothstep 折成来回，±4 px、96 帧一个来回，不引 libm、`static inline` 守零 x87）；
挡弹判定与血条跟着画面位置走，移动中也叠加。常态每帧还从本体起 2 颗 script97 身体火焰（`%RANDF` 抽身体范围内的起点、向上飘），量与火球飞行粒子相仿。

### 2.3 火球

- **选目标**：两遍走 `EnemyManager+0x18c` 链表（与破损核心同一套过滤 `+0x635c & 0xc000021`）：先数 n，`ce_rand() % n` 取下标，再取那一个的 `+0x1270/+0x1274`。
  没敌人就不投（只移动）。**不缓存 enemy 指针**。
- **飞行**：直线飞向开火时记下的坐标，`n = ⌊d / 4⌋ + 1` 帧恰好到点（每帧位移 = d / n，不会飞过头；2026-09-06 用户：速度 8 → 4）；方向角 `bc_atan2f`（确定性 SSE 多项式）。
  非追踪：敌人走开就打空，这是设计（火球是「砸向一个地方」）。飞行中每帧起 2 颗 script95 粒子钉在火球坐标，粒子在 ANM 里用 `%RANDF`（UI RNG 流）抽飘散方向，白热缩小淡成橙红——视觉随机不进 replay 流。
- **飞行开路**（2026-09-06 加强）：每帧 `ce_cancel_radius((bx, by), 24, 9999, 0)`，火球穿过的弹变点道具；不动炎魔的 hp（各自一次调用、各自读计数器）。
- **爆炸**：到点那帧先 `ce_cancel_radius((ex, ey), 96, 9999, 0)` 消弹（半径 = 杀伤框边长），再起 script94 光环（0.4 → 2.4）、10 颗 script96 火星、`ce_screen_shake(14, 5, 0)`、放 `0x2c`；
  **下一帧起连续 12 帧**每帧 `ce_raise_damage_cap(300)` + `ce_damage_rect((ex, ey), 0, 寿命 2, 100, 96×96)`。
  之所以拆成 12 个源而不是一个 1200 的源：`enm_compute_damage_sources` `0x45f0f0` 的 tag 守卫让**一个源对同一敌人只结算一次**。
  ★ **上限的真相**（2026-09-06）：自机弹也是往同一个伤害源池放记录，「每个敌人每帧」的合计钳 `player+0x47984`（Reimu 90 / Marisa 160 / Sakuya 60 / Sanae 120）——
  自机对着 boss 开火时配额已被自机弹吃满，第一版 50 × 12 名义 600 实际接近 0。解法照零售 Remilia（`0x40f48e`：`on_tick_2` 里写 300 再放源）：
  GameThread `0x443d3b`（优先级 0x10）每帧复位、AbilityManager tick（0x16）写、敌人结算（0x1b）读 ⇒ 只对当帧有效，不用还原。300 − 任何角色的上限 ≥ 140 > 100 ⇒ 火球的 100 整份落在自机弹之上，12 帧实打 1200。范围内多个敌人各吃一份。
- **震屏**：照 ECL 517 `setScreenShake(time, start, end)` 的 case `0x434d8d` 调 `ScreenEffect` 工厂 `0x476060`（fastcall：ecx = type 1、edx = time；栈 start / end / 0 / 0x5b，`ret 0x10`）。
  on_tick `0x475c70` 把强度从 start 线性降到 end，每帧用 UI RNG 抽相机偏移 —— 不进 replay 流。14 帧、5 px 起，是「小震」。
- 周期 420 ≫ 最长飞行（满场 ~600 px / 4 = 150 帧）⇒ 场上最多一颗，状态里只留一个槽。

### 2.4 生命 / 挡弹 / 血条 / 收尾

与青眼同一原语：`ce_cancel_radius(pos, 33.6, hp, 0)` 每帧（pos 含呼吸浮动），挡一发 −1，挡到那帧本体染橙 `0xffff8040`。血条复用 script86 / 87（drawRect 根 VM，
C 写 pos / scale / color），满宽 40、>50％ 橙 / >20％ 黄 / 红。归零 → `interruptLabel(1)` 放大淡出、`0x29`、`on_active_tick` 返回 0 开始充能。
过关 / 局末删 VM。本体 VM 被引擎收掉（O29h）→ 结束。

## 3. 数值

| 项 | 值 |
| --- | --- |
| id / `internal_name` / `category` / `price_tier` / `weight` | 72 / `FIRELORD` / 0（主动）/ 14（500）/ 2 |
| 成本 / 门槛 / 充能 | 200 / 300 / 600 帧 |
| HP / 挡弹半径 | 800 / 33.6 |
| 周期 / 滑动 / 落点范围 / 呼吸 | 420 帧 / 60 帧 quintic / x [−160, 160) × y [300, 410) / ±4 px、96 帧 |
| 火球速度 / 消弹 / 爆炸 / 震屏 | 4 px/帧 / 飞行 r 24、落地 r 96 / 12 帧 × 100 × 96×96 + 上限抬 300（实打 1200）/ 14 帧、5 → 0 |
| 登场 | 语音 → 60 帧 × 6 颗聚气粒子 → 本体 + 3 圈爆炸（间隔 8 帧）|
| 音效 | 召唤 `0x4d` + 语音 `0x55`；投球语音 `0x56`；爆炸 `0x2c`；死亡 `0x29` |

## 4. 美术

| 资源 | 来源 / 处理 | 在哪 |
| --- | --- | --- |
| 卡图 sprite 146/147 | 用户提供立绘 500×654 → `fit_card.py FIRELORD … --no-detect --trim 0 --margin 0 --fill`（横向拉 4％ 到 4:5，套零售框）| `cards/FIRELORD_*.png` |
| 本体 `FIRELORD_BODY`（entry21 / sprite123）| 用户提供正面像 1189×1323 白底 → `make_firelord_art.py` 抠白底（四角泛洪 + 边缘斜坡）→ 256×256；script91 缩放 0.25（约 64 px 高）| `ability/firelord/RAGNAROS.png` |
| 火球 `FIRELORD_FIREBALL`（sprite124）| 程序生成 64×64：白芯 → 黄 → 橙 → 红，边缘按角度噪声抖出火舌（固定种子）| `FIREBALL.png` |
| 爆炸 `FIRELORD_BLAST`（sprite125）| 程序生成 128×128：内圈闪光 + 外圈光环 | `BLAST.png` |

脚本：91 本体（出场放大、待命、`interruptLabel(1)` 死亡；**不碰 pos**，pos 全归 C）；92 火球（加色、沿 +x 拉成彗星、脉动缩放；
**不碰 rotate**：方向由 C 写 `rotation.z`）；93 拖尾（每 2 帧一个，缩小淡出染红）；94 爆炸（0.4 → 2.4 放大淡出）；95 飞行粒子（每帧 2 颗，`%RANDF` 飘散 ±16 px，27 帧白 → 橙红）；96 落地火星（10 颗，飘 ±60 px 先快后慢，33 帧）；97 常态身体火焰（每帧 2 颗，在身体范围内随机起点、向上飘 30 px，27 帧）；98 登场聚气（每帧 6 颗，±64 px 起点 20 帧汇聚到中心）。2026-09-06 用户：粒子略大、寿命 +50％，常态也要有与火球相仿的粒子。

语音：`_src/*.ogg` → `convert_voice.py`（ffmpeg，+6 dB + 软限幅，44.1 kHz 16-bit 单声道）→ RMS −10.7 / −11.2（基准 −10 ± 4）。

## 5. 实现

- `native/cards/firelord.c`：`on_activate`（门槛 → 起 VM → spend_power → repopulate → 音效 / 语音）、`on_active_tick`（挡弹 → `fl_step` → 移动 / 开火 / 火球位姿 / 拖尾 / 爆炸 / 死亡）、
  `on_stage_start` / `on_run_reset` 删 VM。
- `native/cards/firelord_core.c/.h`：状态机（不产随机数，随机 dword 由调用方喂）。主机单测 `tests/test_firelord_core.c`。
- 新 SDK 包装（`sdk.h`）：`ce_spend_power`、`ce_repopulate_options`、`ce_rand`、`ce_screen_shake`；`engine.h` 新增 `CE_FN_SPEND_POWER` / `CE_FN_PLAYER_REPOPULATE_OPTIONS` /
  `CE_FN_RNG_RAND_DWORD` / `CE_ADDR_REPLAY_SAFE_RNG` / `CE_ADDR_POWER_PER_LEVEL` / `CE_PLAYER_INNER` / `CE_FN_SCREEN_EFFECT_NEW`。

## 6. 审计

[`AUDIT.md`](../../../mods/th18.v1.00a/card-expand/AUDIT.md) §V。核心：V1/V3 两个调用约定、V5 RNG 选对实例、V7 爆炸的 8 源结算。
