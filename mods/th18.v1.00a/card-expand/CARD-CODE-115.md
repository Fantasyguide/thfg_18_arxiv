> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；本文跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P15a**（W4 前半）：**代码端文档** —— 素材 = [`CARD-BEHAVIOR-AUDIT.md`](CARD-BEHAVIOR-AUDIT.md) 的本卡节（P18 只读审计 ✓）
> + [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md) / [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md) / [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) 的相关段 + 卡本体代码 ✓。
> **本文只描述现状** ✓（不改代码 ✓）；玩家视角的说明另见 P15b 的 `PLAYER-GUIDE.md` ✓。
>
### 115 — 东方绀珠传（CARD-CODE）

**一句话**：把 TH15 的章节系统搬到 TH18——擦弹判定区里有敌弹时该弹染黄、自机周围起擦弹圈、场上掉落物减速 ×0.3；boss 战按「一段攻击 = 一波」切波，波末按擦弹数 × 击破率结算章节奖，左侧一块 TH15 风格面板显示实时读数（同 `CARD-BEHAVIOR-AUDIT.md` §115 ✓）。

**实现链路**：以下行号均为 `cards/lolk.c` / `cards/lolk_core.c` 现读值。
1. 注册：`lolk.c:1132-1138` 的 `CE_CARD(115, …)` 挂 `on_tick_2` / `on_draw` / `on_stage_start` / `on_run_reset` / `on_delete` / `on_item_collect`。
2. 每帧入口 `on_tick_2`（`lolk.c:908-998`）：① 擦弹差分 `:927-932`（`graze_read()` `:47-50` 读 `0x4ccd0c`）；② 击破/刷出差分 `:934-943`（`enemy_count_read()` `:84-88` 读 `zEnemyManager+0x198`）；②b 新建敌人数 `:945-952`（`spawn_id_read()` `:254-258` 读 `+0x90`）。
3. 减速 + 擦弹圈 `:955-958`：`bullet_in_graze_zone()` `:460-517`（`CE_BULLET_LIVE_FOR_EACH` = `sdk.h:1260`；圆/矩形两模式判据 `:502-510`；命中 `tint_mark()` `:451-458`）→ `lolk_slowdown_step()`（`lolk_core.c:321-336`）→ `slow_apply()` `:326-337` 每帧把 `ITEM_MANAGER+0xe6bb14` 写 0.3 → `ring_sync()` `:526-541` 起/钉/收 TH15 擦弹圈脚本。
4. 波信号：`phase_read()` `:292-322`（boss 槽 `CE_EM_BOSS_IDS`、`SPELLCARD+0x78` 三位、中断槽 `zEnemy+0x6374` 八格、超时持续位 `+0x635c` bit24）；`stage_ecl_read()` `:224-252`（关卡脚本 `main` 敌人的主上下文子程序名 → `parse_mainsub_number()` `:203-217`）。
5. 波状态机 `lolk_tick()`（`lolk_core.c:177-289`）：旧波相位结果采样 `:190-206`、boss 消失结算 `:220-225`、开波判据 `mid_boundary()` `:84-96` / 相位指纹 `:244-248`、`open_wave()` `:159-175`、`close_wave()` `:122-157`。
6. 结算落笔 `apply_settle()` `:616-670`：`ce_add_score_delta()` `:622`；`ce_piv_add(id, (bonus/50000)×10)` `:647-650`；`bonus ≥ 1e6` 发命碎（`lolk_core.c:145-153`）经 `spawn_item_at()` `:609-614` 生成；面板滚动参数 `:661-664`。
7. 绘制 `on_draw()` `:1048-1082`：`panel_show()` `:696-726` 起六条贴图 VM；`panel_alpha_sync()` `:845-872` 走共享压暗件；`panel_rows_draw()` `:1011-1046` 五行数值走 `ce_hud_vm_text_set`；退场淡出 `:1060-1076`。
8. 收道具 `on_item_collect()` `:382-393`：TH18 type `9`–`0x0e` ⇒ `ce_piv_add(id, 2)`。

**用到的 SDK**：

| 符号 | 用途 | 定义 |
| --- | --- | --- |
| `ce_state` | 取 496B 私有状态（本卡 `lolk_card_t`） | `sdk/public.h:20` |
| `ce_card_id` / `ce_log` | 卡号 / 落日志 | `sdk.h:21` / `sdk/public.h:268` |
| `ce_piv_claim_auto` | 声明 PIV 在场 + 本卡可写上限 | `sdk_core.h:426` |
| `ce_piv_add` | 往共享 PIV 池加值（层侧累计夹） | `sdk_core.h:429` |
| `ce_graze_family_claim` | 声明「我是擦弹族」（只算一次） | `sdk_core.h:439` |
| `ce_difficulty` | 难度索引（查 PIV 表用） | `sdk.h:1076` |
| `ce_add_score_delta` | 章节奖进总分 | `sdk.h:455` |
| `ce_spawn_item` | 生成碎片道具（type 0x10） | `sdk.h:413` |
| `ce_play_sound` / `ce_player_x` | 跳动 SE / SE 定位 | `sdk.h:50` / `sdk.h:492` |
| `ce_player_pos` | 自机坐标（擦弹圈/碎片落点） | `sdk.h:486` |
| `ce_spell_flags` | `SPELLCARD+0x78` 三位（进行/耐久/超时） | `sdk.h:600` |
| `ce_enemy_by_id` | `boss_ids` → 活 boss 指针 | `sdk.h:619` |
| `ce_anm_spawn` / `ce_anm_spawn_ui` | 起擦弹圈（世界层）/ 起面板（UI 层） | `sdk.h:86` / `sdk.h:248` |
| `ce_anm_get_vm` | 句柄存活校验 | `sdk.h:240` |
| `ce_anm_set_pos` | 擦弹圈每帧钉自机 / 面板摆位 | `sdk.h:265` |
| `ce_anm_set_color` | 写面板压暗 alpha | `sdk.h:273` |
| `ce_anm_interrupt` / `ce_anm_delete` | 发淡出中断 / 换关收 VM | `sdk.h:391` / `sdk.h:393` |
| `ce_hud_vm_text_set` / `ce_hud_vm_text_clear` | 五行数值贴图 VM | `sdk.h:798` / `sdk.h:823` |
| `hud_dim_tick` / `hud_dim_init` / `hud_dim_rect_surface` | 共享压暗件（见跨卡） | `cards/hud_dim.h:184` / `:170` / `:174` |
| `CE_CARD` / `CE_ABILITY_ANM` | 卡注册表 / ability.anm 句柄 | `sdk.h:918` / `sdk.h:84` |

引擎字段宏全在 `engine.h`：`CE_ITEM_SLOWDOWN`(`:924`)、`CE_SPELL_FLAGS`/`CE_SPELL_FLAG_*`(`:135-139`)、`CE_EM_BOSS_IDS`/`CE_EM_BOSS_SLOTS`(`:149-150`)、`CE_ENEMY_ID`(`:186`)、`CE_BULLET_FLAGS`/`CE_BULLET_POS`(`:791-792`)、`CE_ANM_VM_COLOR1`(`:307`)、`CE_PLAYER()`(`:373`)、`CE_ADDR_GAME_THREAD_PTR`(`:27`)；遍历宏 `CE_ENEMY_FOREACH` `sdk.h:593`、`CE_BULLET_LIVE_FOR_EACH` `sdk.h:1260`。

**数值**：

| 项 | 值 | 单位 | 站点 | 一手出处 |
| --- | --- | --- | --- | --- |
| 掉落物减速因子 | 0.3 写 / 引擎 +0.1 每帧回 1.0 | float | `lolk.c:336`、`lolk_core.h:74` | TH15 `0x3e99999a`；`engine/player/th18/02` |
| 章节奖 bonus | `⌊擦弹×击破率％×50 / 10⌋×10` | 分 | `lolk_core.c:30-38` | TH15 `0x43ac70` 起（`0x4cffc0`=50.0） |
| score 增量 | `bonus / 10` | 分 | `lolk_core.c:144`、`lolk.c:622` | TH15 `0x43ad19` |
| PIV 增量（章节奖） | `(bonus/50000)×10` 两步取整 | 显示 | `lolk.c:647` | TH15 `0x43ad58` |
| 碎片阈值 | 1e6 | bonus | `lolk_core.h:68`、`lolk_core.c:145` | TH15 `0x43ae42` |
| 收转换点道具 | +2 | 显示 | `lolk.c:388`、`lolk.c:381` | TH15 `0x43ff4b`（`2.0f` @`th15:0x4cff10`） |
| 面板保留 / 跳动起始 | 300 / 90 | 帧 | `lolk.c:607` / `:604` | 原生 `gui+0x1b4`=290（差 10 未改，`TUNING.md` §7.5） |
| 擦弹圈半径系数 | 3.6 | ×判定半径 | `lolk.c:68`、`:481` | TH15 变大态 `th15:0x457510`（≤8 行，其余见台账） |

**跨卡协同**：

- 共享 PIV 池：`ce_piv_claim_auto` `lolk.c:913`、`ce_piv_add` `:647-650`（章节奖）/ `:388`（消弹小道具 +2）——与 110/112/113/114/116/117 同池；init/cap 逐作值见 `MAXPOINT-AUDIT.md` §1（th15 行）。
- 减速旋钮 `ITEM_MANAGER+0xe6bb14` **唯一写者 = 本卡**（`lolk.c:336`）⇒ 110/111/116 的自建实体（绿点/季节点/令牌）一起变慢、收益侧同步下降（`CARD-NUMERIC-INTERACTION-AUDIT.md` §1.9、`:286`）。
- 判定点只读：`lolk.c:474-481` 读 `0x479a4`/`0x479a8`/`0x4799c`；113 灵梦缩判定点会连带缩小本卡擦弹圈（`CARD-RESOURCE-INTERACTION-AUDIT.md:275`、`CARD-NUMERIC-INTERACTION-AUDIT.md:590`）。
- 擦弹族声明 `ce_graze_family_claim` `lolk.c:914`，与 106/107/110/111/112/113/114/116 同族（`MAXPOINT-AUDIT.md` §4）。
- 共享压暗件：调用 `lolk.c:859`、复位 `:903`；与 116/117/107/113/110+111 同档（终点 64 / 8 帧线性），矩形按本卡几何每帧现算 `:824-840`；触发行为 = ❓ 用户口径（非复刻，`TUNING.md` §7.5）。
- 共享站点 `ce_item_collect`（`th18:0x44625f`，`sites.py:1100`）：本卡订阅它，与 110/114 同站点，不新增断点、不改字节。

**生命周期**：

- ctor：**未注册**（无 `.ctor`）⇒ 换关不误清 run 级数据（符合 `LIFECYCLE-AUDIT.md` §0.1 判据 ①）。
- `on_load`：无。`on_stage_start`：`lolk.c:1084-1098` —— `lolk_reset` + 三 sentinel + 压暗复位 + 收圈/面板 + `s_stage_id=0`。
- `on_run_reset`：`:1112-1130` —— 收圈/面板 + `memset` + 压暗复位 + sentinel 补写。
- `on_delete`：`:1100-1110` —— 收圈 + `panel_hide()`（六条贴图 + 五行数值），补上 2026-09-10 漏掉的删卡路。
- 已知缺口：文件级 static `s_tint_bullet[256]` / `s_tint_orig[256]` / `s_tint_n`（`:430-432`）持引擎弹指针 + 旧 RGB，唯一清点 = 下一帧扫描开头 `tint_restore_all()` `:434-449` ⇒ 换面/retry/删卡那一刻不 restore（`LIFECYCLE-AUDIT.md` F7d、`CARD-RESOURCE-INTERACTION-AUDIT.md` §3.1）。
- 同一缺口：`s_stage_id`（`:127`）在 `on_delete` 里不碰（`on_stage_start` `:1095` 与 `on_run_reset` `:1129` 各自归零）；三处 static 清理都排在 `if (!s) return;` 之后（`:1087`/`:1103`/`:1115`），对照形状见 `CARD-RESOURCE-INTERACTION-AUDIT.md:332`/`:501`。

**已知未测 / 风险**：

- 击破率分母 = `enemy_count_real` 差分近似（`lolk.c:640-644`、`lolk_core.h:57-62`）：TH18 无 TH15 章内 SPAWNED/DESTROYED ⇒ 非一手、无实跑校准。
- 碎片口径自相矛盾：`LOLK_FRAG_CYCLE=5` 注释仍写「每 5 波给命碎」，实现恒发 `LOLK_FRAG_LIFE`（`lolk_core.h:70`、`lolk_core.c:145-153`），`LOLK_FRAG_BOMB` 全仓未被使用。
- 「道具变黄」那半没接：TH15 的黄来自擦弹道具 type 13，TH18 无该生成链 ⇒ 待 owner 拍（`SINGLE-CARD-TEST.md` §th15-P9b）。
- 面板几何/淡出（终点 64 / 8 帧 / 五行 20 帧）与擦弹圈 3.6 系数均标「已改 · 待实跑」，无验收记录。
- 满 P 收 type 8 ⇒ +100 那支已按 owner 口径**撤掉**（`lolk.c:341-347`、`:391-392`），实现已删、只留一手记录。
- `CE_BULLET_FLAG_GRAZED`（`:74`）定义后未参与判据 —— 擦弹一生只算一次，故本卡每帧自扫（`:52-62` 注释）。
