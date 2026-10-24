> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
### 106 — 东方红魔乡（CARD-CODE）

**一句话**：红魔乡（EOSD_PLAYRANK）是被动整蛊卡：把 th06 的 PlayRank 十二路增减照 1:1 记账（用户定案：**不写任何引擎字段、不产生任何游戏效果**）、聚焦时不显示自机判定点、不满 P 时过收点线不吸道具且整局不画那条收点指示线，另在 DLL 自己的 rodata 里留一处零影响的「冴月麟」字符串彩蛋。（同台账：见 CARD-BEHAVIOR-AUDIT.md §106）

**实现链路**：
1. 注册：`native/cards/th06rank.c:534` 的 `CE_CARD(106, …)` 只挂回调、无 `on_activate`/充能 ⇒ 被动常在；宏展开与虚表槽位见 `native/sdk.h:918`。
2. `on_load`（`th06rank.c:502`）→ `init_all`（`th06rank.c:160`）：`memset` 私有状态、`t06_state_init` 按难度建 min/max、申报「不画线」、冴月麟同值写入、打一行日志。
3. `on_stage_start`（`th06rank.c:451`）：`t06_state_stage_reset`（subRank=0 / 帧计数=0 / 撤库存基准）→ `seed` 重设差分基准 → 按 `ce_owned(36/37)` 各补 +0.70 / +0.02。
4. 逐游玩帧 `on_item_frame`（`th06rank.c:201`）← `ce_item_frame` 断点 `0x446ee3`（`native/sites.py:1097`）：对话门 `CE_GUI_MSG_ACTIVE()` → `t06_tick_frame` → 帧初清碎片窗口 → 帧数条件 +1.00（`th06rank.c:213`）→ 扫 4696 槽道具池记出屏 −0.03（`th06rank.c:229-249`）。
5. 逐颗道具 `on_item_collect`（`th06rank.c:255`）← 断点 `0x44625f`（`sites.py:1100`），type 分派前：type→动作表在 `th06rank_core.c:124`；蓝点分档读 `ce_poc_height_semantic()` 对**自机 y**（`th06rank.c:288-291`）。
6. 管理器帧 `on_tick_2`（`th06rank.c:329`，虚表 `+0x2c`，tick 优先级 `0x16`）：难度重同步（`th06rank.c:332`）→ 调试键空格=满 P（`th06rank.c:344-355`）→ 满 P 闸申报（`th06rank.c:373-388`）→ 擦弹差分 +0.06（`th06rank.c:393-406`）→ `t06_commit_frame` 收口整命/整炸（`th06rank.c:411`）。
7. `on_bomb_spent`（`th06rank.c:419`）← `0x4203bc`：−2.00；`on_death_after_deathbomb`（`th06rank.c:430`，虚表 `+0x0c`）：`acc != 0` 让位，否则 −16.00。
8. **判定点隐藏（`t06_hide_hitbox_dot()`）—— 两个相位都写**：
   · **`on_item_frame`（优先级 0x1e）**= **主笔**：晚于所有卡的更新回调（`on_tick_2` = 0x16）与玩家 tick（0x17，判定点 VM 在那里建）、早于**全部**绘制阶段（层 18 = 0x28）⇒ 本帧最后一笔，谁都盖不掉；
   · `on_draw`（虚表 `+0x48`，优先级 0x33）= 兜底（那一笔晚于本帧绘制，只对下一帧有效）；
   写什么：`color_1 = 0x00FFFFFF`（alpha 0）**＋ `color_2` 的 A 字节 = 0**（绘制早退 `0x4812e8` 判两路 A 都 0，与 `colorMode` 无关）**＋ alpha 插值器三格 `initial/goal/current = 0`**（`vm+0x140`；引擎每帧用 `current` 覆盖 `color_1.A`）；
   四道门 = 游戏线程 / 自机 / 聚焦 / `player+0x476b0` 句柄（VM 没了就跳过）。
   ★ 2026-10-25 修 owner 报的「106 的隐藏被 108 覆盖」：根因是**相位**（原来只在 0x33 写 ⇒ 落在绘制之后），详见 `AUDIT.md` §BE-106-HITBOX-108-2026-10-25；探针 `T06R-DOT:` 行（`CE_DEV_DUMP≥1`）给实跑判据。
   ★★ 2026-10-25 追加（owner 一手「没有拿掉逆时针转的那一个」）：判定点是**两颗** VM —— 正转那颗 = `player+0x476b0`（effect.anm script `0x1a`），**反转那颗 = 它的子 VM**（script26 第 0 帧 `scriptNew(script25)` = script `0x19`，`angleVel(-0.10471976)`）。子 VM 由 `AnmLoaded__instantiate_child_vm` `0x4888f0` 建、挂在 `父+0x594`（`list_of_children`）⇒ `t06_hide_hitbox_dot()` 沿 `父+0x598` 走一层、对每颗子 VM 同样压（`t06_hide_one_vm()`）；探针多一行 `T06R-DOT2:`（每个子 VM 一行，`script` 应为 `25`）。一手与改动清单见 `AUDIT.md` §BE-106-HITBOX-108-2026-10-25 §6。
9. 收尾：`on_run_reset`（`th06rank.c:525`）与 `on_delete`（`th06rank.c:516`）撤三层声明（满 P 闸 / 「遮」/ 不画线）并 `memset` 私有状态。
10. 纯逻辑（难度表 `th06rank_core.c:14`、增减 `th06rank_core.c:49`/`:61`、帧条件 `th06rank_core.c:89`、分档 `th06rank_core.c:98`/`:105`、窗口 `th06rank_core.c:176`）全在 core；本文件只接线（`th06rank.c:12-13` 自述）。

**用到的 SDK**：
- `ce_log` — 单行日志（`sdk/public.h:268`）；`ce_log_key` — **关键行**（`CE_DEV_DUMP≥1` 才进二进制，`sdk/public.h:310`，106 的 `T06R-DOT:` 用它）；`ce_card_id` — 取卡 id（`sdk.h:21`）；`ce_state`/`ce_state_user` — 卡私有状态 496B（`sdk/public.h:20` / `sdk_core.c:131`）；`ce_owned` — 查卡组存在性（`sdk.h:472`）。
- `ce_poc_declare_mask`（`sdk.h:537` → `sdk_core.c:399`）— 对收点线声明「遮」；`ce_poc_line_hide`（`sdk.h:542` → `sdk_core.c:459`）— 声明不画开局指示线；`ce_poc_height_semantic`（`sdk.h:554`）— 读**不含「遮」**的语义高度。
- `ce_full_p_gate_declare`（`sdk_core.h:560` / `sdk_core.c:615`）— 「未满 P」粘性声明，供 111 消费。
- `ce_anm_set_color`（`sdk.h:273`）— 整条 dword 写 VM `color_1`（本卡 alpha=0、RGB 全白）。
- `ce_key_down`（`sdk.h:841` / `sdk.c:399`）+ `CE_VK_SPACE` — 测试键电平读 + 边沿检测；`CE_CARD`（`sdk.h:918`）— 卡与虚表注册。
- 引擎读数宏（`native/engine.h`）：`CE_ADDR_CURRENT_LIVES:22`、`CE_ADDR_LIFE_FRAGMENTS:130`、`CE_ADDR_CURRENT_BOMBS:24`、`CE_ADDR_BOMB_FRAGMENTS:25`、`CE_ADDR_PLAYER_PTR:71`、`CE_ADDR_MAX_POWER:38`、`CE_ADDR_GAME_THREAD_PTR:27`、`CE_CURRENT_POWER():359`、`CE_GUI_MSG_ACTIVE():386`、`CE_PLAYER_FOCUSED():456`、`CE_PLAYER_Y:454`、`CE_PLAYER_POC_HEIGHT:474`、`CE_ITEM_*`（`:872-929`）。

**数值**（完整表见 CARD-BEHAVIOR-AUDIT.md §106）：

| 值 | 单位 | 站点 | 一手出处 |
| --- | --- | --- | --- |
| 难度表 rank 初值/min/max：EASY 16/12/20、N/H/L 16/10/32、EXTRA 16/14/18 | 存储整数 | `th06rank_core.c:14` | th06 `GameManager.cpp:70-76` |
| 帧数间隔 = 2400 − 残机×240（且帧 ≠ 0） | 游玩帧 | `th06rank_core.h:59-60` + `th06rank_core.c:89` | th06 `EnemyManager.cpp:162-170` |
| 擦弹 +0.06 | 存 6000（1/100000 rank） | `th06rank.c:402` | th06 `Player.cpp:1394` |
| 蓝点线上 +0.06 / 线下 +0.006 | 存 30000÷5 / 3000÷5 | `th06rank_core.h:78-81` | th06 `ItemManager.cpp:234/238` |
| 小 P +0.002（110/111 在场 ×5 ⇒ +0.01） | 存 1000÷5 | `th06rank.c:263` | th06 `ItemManager.cpp:205` |
| 命碎 +0.70 / B 碎 +0.02 | 定值，存 70000 / 2000 | `th06rank_core.h:75` / `th06rank_core.h:77` | spec §3（TH18 自有通道） |
| 整命 +2.00 / 整炸 +0.05 / 出屏 −0.03 / Bomb −2.00 / Miss −16.00 | 存 200000/5000/3000/200000/1600000 | `th06rank.c:411` / `th06rank.c:244` / `th06rank.c:422` / `th06rank.c:442` | th06 `ItemManager.cpp:299/291/145`、`Player.cpp:185/225` |
| 出屏判据 `\|x\| ≥ 200.0` 或 `y > 472.0` | 世界坐标 float | `th06rank.c:71-72`, `:238-240` | th18 `0x445e13` / `0x445e20` / `0x445e2b` |

**跨卡协同**：
- **110 / 111**：小 P 通道 ×5（`th06rank.c:263`、`th06rank_core.c:105`；spec §8 用户口径）。
- **111**：`ce_full_p_gate_declare(106, gate)` 被 `native/cards/signal.c:2194` 读成 `no_line_topup` ⇒ 关掉「线上未满就补满并钳 100.00」那一支（TUNING §2.8）。
- **原生卡 22 Kanako 与任何 PoC 高度/线显示声明者**：「遮」是覆盖位、无条件压过别家高度（`sdk_core.h:174-211`）；整局清 `Gui+0x194` bit `0x2000` ⇒ 不画线（唯一原生置位 = 原生卡 22 `on_load`，一手 `th18:0x40c288`；唯一读点 `th18:0x43af0b`；WORK-PACKAGES §7-A/A4）。
- **36 Narumi / 37 Patchouli**：不经道具池 ⇒ `on_stage_start` 按持有各补 +0.70 / +0.02（`th06rank.c:459-460`）。
- **原生救援 23 Eirin / 35 ShikiEiki**：`+0x0c` 的 `acc` 契约（`th18:0x45c116` / `0x45c121` / `0x45c127`）⇒ 前面的卡已取消死亡就不记 Miss（`th06rank.c:438`）。
- **108 妖怪之山**：108 侧读卡链表，106 在场时决死 B 多消耗一个（见 CARD-BEHAVIOR-AUDIT.md §108）。
- **110 残余**：`mof.c` 经 `faith_player_poc_height()` 读到**遮值**（不是语义值），106+110 同场时其蓝色系数偏差已知（AUDIT.md §BE-ET §3）。
- 与两份共享系统普查的关系：本卡**不进** MAXPOINT-AUDIT.md 的共享 PIV 池（无 `ce_piv_*` 调用）；**不在** VM-IDIOM-MAP.md 的 43 处「VM 直写」内（判定点只经 `ce_anm_set_color` 封装整写、不裸写字段）。

**生命周期**：
- 钩子覆盖：**未注册 `.ctor`**（LIFECYCLE-AUDIT.md §2.1 判 106 换面保持 ✅）；`on_load +0x20` ✓、`on_stage_start +0x34` ✓、`on_run_reset +0x4c` ✓、`on_delete +0x50` ✓（撤声明 + `memset`，`sdk.h:10` 保证 on_delete 时状态还活着）。
- 换面：`on_stage_start` 只清关卡级（subRank / 帧计数 / 库存基准），rank 与 min/max 跨关保留 ✓（LIFECYCLE-AUDIT.md:62）。
- 全新一局走「全删重建 + `+0x50`」、不跑 `+0x4c`，本卡由 `on_delete` 覆盖 ✓（LIFECYCLE-AUDIT.md:26/33；106 不在 F 清单）。
- 已知缺口：判定点 VM 的 alpha **不还原**（`th06rank.c:510-515` 自陈；`CARD-RESOURCE-INTERACTION-AUDIT.md:314` 记为「已知且接受」，商店里不聚焦 ⇒ 实不可复现）。本卡无自建 VM / 子机 / HUD / 全局旋钮。

**已知未测 / 风险**：
- 判定点 alpha=0 的**实跑观感**未回报（AUDIT §AN6 只标 🟡；仓库无 106 实跑回报）。
- 满 P 闸改走「遮」后的四条实跑验收（不满 P 不吸 / 满 P 照吸 / 计分逐位不变 / 开局无线）交给 owner，未见回报；`SINGLE-CARD-TEST.md` §th06 #1 仍写旧的 §BE-EP 修法（状态陈旧）。
- 每帧扫 `CE_ITEM_SLOTS`=4696 槽的**帧耗**无任何实测数据。
- 「遮」的两个自定值未实跑验证：`CE_POC_TINY_BITS = 0x00800000`（`sdk_core.h:205`）与「钳位上限 = 自机 y、余量 0」（TUNING §2.0t）。
- 擦弹差分里 `if (d > 200) d = 200` 的阈值 200 **无一手来源**（`th06rank.c:401`，注释自承防溢出护栏）。
- ItemManager 的 UpdateFunc 优先级**全仓无记载** ⇒ 碎片窗口跨帧收口的正确性只有推理（`th06rank.c:319-320` 自承）。
- 碎片净减抑制的边界（3 碎 = 2.1、净 0 不抑制）只在主机单测验过，无实跑对账。
- `+0x0c` 残余：**排在 106 之后**的救援者救下时，106 仍会记一笔假 Miss（`th06rank.c:437`）。
- 106 与 110 同场时 110 的蓝色系数读到遮值（上「跨卡协同」末条）—— 归 110 作者。
