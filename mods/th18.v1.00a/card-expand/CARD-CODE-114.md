> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
### 114 — 东方辉针城（CARD-CODE）

**一句话**：TH14 式收点批 —— 把「真空飞行收点」的 P / 钱（外加 110/111 的自建绿点）攒成一批，批闭时按批大小走档位倍率给分数补差、每批弹 1 枚碎片卡；另有一座被动：非聚焦命中敌人掉 P（同 CARD-BEHAVIOR-AUDIT.md §114）。

**实现链路**
1. 注册：`collect2x.c:516-518` 的 `CE_CARD(114, …)`（宏体 `sdk.h:918`）只挂 6 个钩子 —— `on_item_frame` / `on_item_collect` / `on_item_commit` / `on_shot_damage` / `on_stage_start` / `on_run_reset`；**无 ctor / on_load / on_delete**。
2. 每游玩帧入口：站点 `ce_item_frame` `0x446ee3`（`sites.py:1097`）→ `sdk.c:2673` 沿卡链回调 → `collect2x.c:228` `on_item_frame`。
3. 帧首：`collect2x.c:238` `++s_play_frame`（自计数帧源，见 `collect2x.c:91-102`）；`:234` `ce_piv_claim_auto` + `:235` `ce_graze_family_claim` 声明共享池；`:240-241` 到点弹欠账碎片（`spawn_pending_frags` `:197`）。
4. 虚拟成员通道：`collect2x.c:256` `ce_vitem_take_collect()` 抽干 → `:258` `c2x_collect_virtual()`（`collect2x_core.c:29`，只加 count）→ `:264` `note_bonus_anchor()` 记批位。
5. presence 续批门：`collect2x.c:284` `ce_item_pool_any_attracting()`（`sdk.c:3216`）/ `:285` `ce_vitem_flying()` → `:294` `c2x_batch_presence()`（`collect2x_core.c:22`）。
6. 批闭结算：`collect2x.c:300` `c2x_maybe_settle()`（`collect2x_core.c:42`）判 idle 与档位 → `:340-341` 差量 `ce_add_score_delta` → `:349-360` 登记碎片 pending（帧内快照批位）→ `:366` 倍率弹字 `ce_popup_mult`（本文件 `:165-191`）→ `:377` `ce_play_sound(CE_SE_PIN00)`。
7. 成员收集：站点 `ce_item_collect` `0x44625f`（`sites.py:1100`）→ `sdk.c:2972` → `collect2x.c:422` `on_item_collect`：`type==8` 满 P → `:436` `ce_piv_add(114,100)`；`type∈{1,2}` 且 `state==3` → `:453` `c2x_collect_batch`、`:458` 记批位、`:459-460` stash `CE_SCORE()`。
8. 分值提交：站点 `ce_item_commit` `0x44658a`（`sites.py:1104`）→ `sdk.c:2995` → `collect2x.c:463` `on_item_commit`：`delta = CE_SCORE() − s_stash` 累进 `batch_score`（`:467-473`）。
9. 掉 P：站点 `ce_shot_damage` `0x45f2a2`（`sites.py:1108`）→ `sdk.c:3016` → `collect2x.c:476` `on_shot_damage`：`:480` 节流（`c2x_maybe_drop`）→ `:487` `spawn_item_at(1, pos, 2.2f)`。
10. 纯逻辑与单测：`collect2x_core.c:13-97` 承载批计数 / presence / 结算 / 碎片 4:1 / 掉落节流；主机单测 `tests/test_collect2x_core.c`（`make test-host`）。

**用到的 SDK**
- 钩子契约：`ce_hooks_t` 的四个道具/命中回调（`sdk/public.h:141-144`）与 `on_stage_start` / `on_run_reset`；注册宏 `CE_CARD`（`sdk.h:918`）—— 本卡不注册 ctor / on_load / on_delete。
- `ce_state`（`sdk/public.h:20`）取每卡状态块；`ce_card_id`（`sdk.h:21`）/ `ce_difficulty`（`sdk.h:1076`）作池声明键与难度；`ce_log`（`sdk/public.h:268`）诊断日志。
- `ce_spawn_item`（`sdk.h:413`）落 P / 碎片道具；`ce_play_sound`（`sdk.h:50`）+ `CE_SE_PIN00`（`engine.h:537`）结算钟声（声像 = 批位 x）。
- `ce_add_score_delta`（`sdk.h:455`）施加倍率补差（可负，内部钳制）。
- `ce_item_pool_any_attracting`（`sdk.h:877`，实现 `sdk.c:3216`）判原生 state-3 在飞。
- 共享 PIV 池：`ce_piv_claim_auto`（`sdk_core.h:426`）/ `ce_piv_add`（`:429`）/ `ce_piv_pool`（`:430`）/ `ce_piv_written`（`:432`）；擦弹族 `ce_graze_family_claim`（`:439`）。
- 自建绿点声明层：`ce_vitem_take_collect`（`sdk_core.h:978`）/ `ce_vitem_flying`（`:982`）。
- 引擎只读宏：`CE_SCORE`（`engine.h:354`）、`CE_CURRENT_POWER`（`:359`）、`CE_PLAYER`（`:373`）、`CE_PLAYER_X`/`CE_PLAYER_Y`（`:452`/`:454`）、`CE_ADDR_MAX_POWER`（`:38`）、`CE_ITEM_STATE`（`:878`）、`CE_ITEM_STATE_VACUUM`（`:898`）、`CE_POPUP_*`（`:942-970`，供本地 `ce_popup_mult`）。

**数值**

| 值 | 单位 | 站点 | 一手出处 |
| --- | --- | --- | --- |
| 售价档 10（=300 金钱）| 档 | `patch/th18/cards.js:493` | 台账 §114 引 `engine/card/th18/08-catalog.md:6` |
| 批闭 idle 8 | 游玩帧 | `collect2x_core.h:112` | 一手 TH14 presence（`th14:0x4f5884`、`th14:0x439d60`）|
| 档位阈值 20/30/40/50/60 | 批数 | `collect2x_core.h:36-40` | 复刻（TH14 计划真值，台账 §114）|
| 倍率 500/800/1200/1600/2000 | permille | `collect2x_core.h:42-46` | 复刻；只作用分数（`collect2x.c:340`）|
| 碎片循环 4 符卡碎 : 1 命碎 | 次 | `collect2x_core.h:48`、`collect2x_core.c:71-80` | 设计 |
| 掉落间隔 96 − 火力×16/100（钳 ≥1）| 帧 | `collect2x_core.h:114-116` | ❓自定（2026-09-08 设计变更）|
| 碎片弹出延迟 20 | 游玩帧 | `collect2x.c:195` | ❓自定（`TUNING.md` §2.9b #4）|
| 满 P 道具 → PIV +100 显示 | 点 | `collect2x.c:419`、`:436` | 一手 `th14:0x43923b`（+10000 存储 ÷100）|

**跨卡协同**
- 共享 PIV 池：112/113/115 同池（`sdk_core.h:424-439`）；本卡每帧 `ce_piv_claim_auto` 声明在场、满 P 收 type 8 时 `ce_piv_add(114,100)`（`collect2x.c:234`、`:425-439`）—— 闸在层内，见 MAXPOINT-AUDIT.md §2 th14 行（代码写 100 与一手一致）。
- 110/111/107 共用绿点 VM（`faith.c` / `faith_core.h`）：114 只读其「虚拟收集 / 虚拟在飞」声明（`collect2x.c:256`、`:285`）；生产者 `faith.c` 的 `dot_collect()` 只报真空飞行态（B1 口径）。
- 碎片走原生卡道具 `0x10`/`0x11` → `CardLifeFragment(3)`/`CardBombFragment(4)`（`collect2x.c:342-348`）；与 117 的碎片卡叠加会真的多发卡（`TUNING.md:2146`）。
- 倍率弹字用原生 PopupManager 倍率槽 k13–17（`collect2x.c:165-191`）；与 107 的数字槽各占各槽（`TUNING.md:3120`）。
- VM 直写普查：本文件 0 处直写、连 `CE_ANM` 都不出现（VM-IDIOM-MAP.md §1 表）。

**生命周期**
- 已挂：`on_item_frame`（trace `+0x68`）/ `on_item_collect`（`+0x6c`）/ `on_item_commit`（`+0x70`）/ `on_shot_damage`（`+0x74`）/ `on_stage_start`（`+0x34`）/ `on_run_reset`（`+0x4c`）。
- 换面（`on_stage_start`，`collect2x.c:492-501`）：只清批级/关卡级 —— `batch_*`、pending 碎片、`anchor_valid`、`s_stash_valid`、虚拟诊断计数、`next_drop_at`；run 级保留 ✓（LIFECYCLE-AUDIT.md §2 114 行）。
- 新局/retry（`on_run_reset`，`collect2x.c:503-514`）：`memset` 状态块 + 清各 file-static（含 `s_play_frame`、4:1 循环游标）✓。
- 缺口：**无 `on_delete`** ⇒ 走 `+0x50`（retry 买卡 / 全新一局 / 退菜单重进）时 6 个 file-static 残留（`s_play_frame` 续涨、`s_stash_valid` 会让新局首个非批集提交多打一行日志；无分值污染）。见 LIFECYCLE-AUDIT.md §2 114 行与 F7c（`:108`、`:217`）。SDK 侧 `ce_layer_drop_card` / `ce_dmg_set` 仍在 `+0x50` 无条件跑（`sdk.h:984` 起）。

**已知未测 / 风险**
- 无 `on_delete` 的 file-static 残留（上条；LIFECYCLE-AUDIT.md F7c）。
- 自建绿点写批位受声明层 2 帧容差影响，可能比绿点被收晚 ≤2 帧（`collect2x.c:259-264`，自定、无实跑）。
- 绿点那一档 PIV（TH14 +2/+20）够不着 ⇒ 本轮没接（`collect2x.c:385-394`）。
- 早结敞口：分值提交滞后到批闭之后的道具会漏出本批（`collect2x.c:74-75`、`:469-471`，待实跑观察）。
- 2026-09-21 改动（碎片生成点 = 批位快照 −48、初速 2.2f）状态 = 已改·待实跑（`SINGLE-CARD-TEST.md` §th14 #1；`AUDIT.md` §Z23）。
- 被动二命中门控未定案：award 路径不读「本调用开火过」标志 ⇒ 停火时敌人身上还掉不掉 P 待实跑（`NEXT.md` §1s.3）。
- 满 P 判据 `mx > 0` 是防御不是语义（`collect2x.c:427-429`）；可达状态恒等，未单独实跑。
- bonus 弹字不参与暂停模糊（`NEXT.md:39-40`）—— 已知视觉差异，未列为 bug。
