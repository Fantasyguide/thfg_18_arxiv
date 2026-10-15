# TH18 卡牌扩容 —— 设计文档（**暂时**）

> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；跨作引用一律 `thNN:` 前缀 ✓；正文裸地址默认属 mod 侧 ✓。
> **工单**：**P61**（owner 2026-10-14 批准的计划 v2）。
> **"暂时"的含义** ✓：本文件是**当前权威口径**，描述**代码现在的事实**；凡尚未拍板的，**显式标 ⏳ 待裁**（汇总见 [`INTENT-AUDIT.md`](INTENT-AUDIT.md) §3），⏳ 项**不许当既定设计使用** ✗。

## 0. 这份文档给谁看、怎么用

- **读者**：改这张卡的人 / agent ✓（玩家向的说明另见 [`PLAYER-GUIDE.md`](PLAYER-GUIDE.md) ✓）。
- **用法**：动手前先读本卡小节 ⇒ 再读实际代码 ⇒ **代码与本文冲突时以代码的活常量为准** ✓，并把本文改对 ✓。
- **落笔要求**：每个元素在代码注释里必须标 **复刻（给一手出处）** 或 **❓自定（给理由 + 登记 `TUNING.md`）** ✓；**写成"自定"却包装成"复刻"是本仓明令禁止的** ✗。

## 1. 通用框架

| 项 | 口径 |
| --- | --- |
| 卡 id | **106–117 = 东方作品号 + 100**（106 红魔乡 … 117 鬼形兽）✓；**118 = 挑战卡**✓；**119 不做**✓；**58–92 那批属其它作者**✓；**201 = 调试用的删卡**✓ |
| 版本轴 | mod 死绑 TH18 的 exe build（`th18:0x400000`）✓ |
| 共享件 | `sdk.h` / `sdk.c` / `sdk_core.*` / `engine.h` / `sites.py` / `Makefile` / `cards_def.c` / `patch/**` / `assets/ability/scripts/**` ⇒ **动它们等于动所有人的卡** ✓，必须在 `AUDIT.md` §BE 登记 + 说明影响面 ✓ |
| 落笔纪律 | 复刻 ⇒ 一手出处（脚本号/地址/常量）；❓自定 ⇒ 理由 + `TUNING.md` 登记 ✓ |
| 长持续效果 | 凡跨帧留东西（自建 VM / 子机槽 / HUD 数字 / 伤害源 / 计时器 / 队列 / 全局旋钮）⇒ **必须交代"被删掉会怎样"**（删卡发生在商店，AbilityManager 冻结，只有 `on_delete` 跑得到）✓ |
| 数值冲突 | **活常量 `#define` / `_Static_assert` > 代码注释 > 文档** ✓（注释会过期 ✗）|
| 发布 | `make check` rc=0 + `make release`；DLL **四处同 md5** ✓ |

## 2. 系统卡 106–117

> 每卡一节，表格列 = `机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态`。
> **S3 逐卡对账（workflow `p61-intent-audit`）产出后回填** ✓；**109 只列跨卡耦合** ✓。

### 108 永夜抄（`cards/youkai.c`）
- **人侧打掉使魔的代价**（槽扣 1/12、掉 1 颗永点、50 帧内收永点不给槽）：**复刻**，一手 = `th08:0x42b240` 的 `Timer(0x18)`（见 `youkai.c:7894` 的注释）✓
- **使魔伤害转嫁**：附加（绕转）使魔 = **全额 100%**；发弹口（映射）使魔 = **50%** ✓ 出处 `youkai_core.h:1467–1468`（`YK_TRANSFER_PCT` / `YK_TRANSFER_PCT_MAP`）+ 调用点 `youkai.c:3853/3903/3938` ✓ ｜ 复刻（TH08 侧转嫁链）＋ owner 2026-10-14 口径 ✓ ｜ ✅（P59）｜ 其余待 S3 回填。

### 111 地灵殿（`cards/signal.c`）
- **支援特技按机体**（每个自机三条，来自 TH11 地灵殿的搭档机体）：灵梦 = 八云紫（缺口瞬移）/ 伊吹萃香（全屏收道具）/ 射命丸文（1.50 倍速 + 拖影）；魔理沙 = 帕秋莉（双击 Z 换子机）/ 河城荷取（水泡罩）/ 爱丽丝（灵击半扣）；咲夜 = 蕾米莉亚（**可持续擦弹**：同一颗弹每 12 帧再计入一次，ON 1 + OFF 11）/ 芙兰朵露（5 秒蓄力清屏）/ 红美铃（信号槽下限 + 涨速）；早苗 = 洩矢诹访子 / 八坂神奈子 / 古明地恋 ✓ 出处 `signal.c` 的 `signal_support_*` 一族 + `AUDIT.md` §DE ✓ ｜ 复刻 + owner 口径 ✓ ｜ ✅（P57）｜ 其余待 S3 回填。

### 112 星莲船（`cards/ufo.c`）
- **喂饱所需颗数**：按**当前关卡**取一手公式 `fill += 0.032 − 关卡号×0.002` ⇒ 第 1–7 关 = **34/36/39/42/46/51/56 颗** ✓ 出处 `ufo.c` 的 `ufo_fill_stage()` + `ufo_core.h` §4 ✓ ｜ 复刻（TH12 一手公式）✓ ｜ ✅（P58）｜ 其余待 S3 回填。

### 106 红魔乡（`cards/th06rank`）

- 一句话：红魔乡（EOSD_PLAYRANK）是被动整蛊卡：把 th06 的 PlayRank 十二路增减照 1:1 记账（用户定案：**不写任何引擎字段、不产生任何游戏效果**）、聚焦时不显示自机判定点、不满 P 时过收点线不吸道具且整局不画那条收点指示线，另在 DLL 自己的 rodata 里留一处零影响的「冴月麟」字符串彩蛋。（同台账：见 CARD-BEHAVIOR-AUDIT.md §106）

| 机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态 |
| --- | --- | --- | --- | :-: |
| **满 P 判据**（「满 P」= 威力满档） | `gate = (pmax > 0 && CE_CURRENT_POWER() < pmax)`；`CE_CURRENT_POWER()` = `[0x4ccd38]`、`pmax` = `[0x4ccd3c]`（`CE_ADDR_MAX_POWER`，**随 sht 变**） | `cards/th06rank.c:373-377`；`engine.h:37,38,359` | ✅复刻 | ✅ |
| **不满 P ⇒ 收点线「遮」**（上线收点当且仅当满 P） | `ce_poc_declare_mask(c, gate)`（粘性，每帧写）；落笔值 = `min(语义高度, 自机 y)`；读不到自机 y ⇒ `CE_POC_TINY_BITS = 0x00800000u`；下钳到 tiny、上钳 `CE_POC_HEIGHT_MAX_BITS` | `cards/th06rank.c:378-380`；`sdk.h:560`；`sdk_core.h:205,211-222`；`sdk_core.c:417- | ❓自定 | ✅ |
| **满 P 闸的第二个消费者：111「线上补满信号」**（协同） | 同一 `gate` → `ce_full_p_gate_declare(ce_card_id(c), gate)`；粘性、按卡 id 记账、owner = id 最小者 | `cards/th06rank.c:377`；`sdk_core.h:550-563`；`sdk_core.c:600-628` | ❓自定 | ✅ |
| **整局不画开局收点指示线** | `ce_poc_line_hide(c, 1)`；落笔 = **清** `Gui+0x194` 的 bit `0x2000`；撤完按基准写回 | `cards/th06rank.c:148-158,166,457,506`；`sdk.h:565`；`sdk_core.h:180,225-227`；`sdk | ❓自定 | ✅ |
| **低速不显示判定点** | `on_draw`：`id = *(u32*)(player+0x476b0)`（`T06R_PLAYER_HITBOX_VM`）；`ce_anm_set_color(id, 0x00FFFFFFu)`（`color_1` = `vm+0x524` 的 alpha = 0）；仅在 `CE_PLAYER_FOCUSED()`（`player | `cards/th06rank.c:46,466-499`；`engine.h:27,71,373,454,456` | ✅复刻 | ✅ |
| **难度表（rank 初值 / 上下限）** | `EASY{16,12,20}` `NORMAL{16,10,32}` `HARD{16,10,32}` `LUNATIC{16,10,32}` `EXTRA{16,14,18}`（`k_diff[5]`）；`T06_DIFF_COUNT 5`；越界按 NORMAL | `cards/th06rank_core.c:14-28`；`cards/th06rank_core.h:46-52` | ✅复刻 | ✅ |
| **rank 增减算法（进位/借位/夹取，小数不修正）** | 单位 = 1/100000 rank；`T06_SUBRANK_CARRY = 100000`（= 1.00 rank）；`T06_UNIT_PERMILLE = 1000`（1 个 th06 原值单位）；`while(sub>=CARRY){rank++;sub-=CARRY;}` 后 `if(rank>max)rank=max`（** | `cards/th06rank_core.h:43-44,93-94`；`cards/th06rank_core.c:40-71` | ✅复刻 | ✅ |
| **帧数条件 +1.00** | `T06_INC_FRAME = 100000`（+1.00）；`interval = 2400 − lives×240`（`T06_FRAME_BASE 2400` / `T06_FRAME_PER_LIFE 240`）；`lives` 钳 `[0,10]`；`stage_frame != 0 && stage_frame % inte | `cards/th06rank_core.h:59-60,71`；`cards/th06rank_core.c:82-95`；`cards/th06rank.c | ✅复刻 | ✅ |
| **帧源 / 时间轴帧** | `on_item_frame`（断点 `ce_item_frame` = `0x446ee3`，`sites.py:1097`）每 hit `++stage_frame`；门 = `if (CE_GUI_MSG_ACTIVE()) return;`（`CE_GUI()`≠0 且 `Gui+CE_GUI_MSG(0x1b0)`≠0） | `cards/th06rank.c:194-210`；`sites.py:1097,1358`；`engine.h:382,385,386` | ✅复刻 | ✅ |
| **擦弹 +0.06** | `T06_INC_GRAZE = 6000`（+0.06，= 6×1000）；每帧差分 `[0x4ccd0c]`（`T06R_ADDR_GRAZE`）；单帧 `d` 钳 `200`（`d > 200 ⇒ 200`）；`g < last` 或 `last < 0` ⇒ 只重设基准 | `cards/th06rank_core.h:72,97`；`cards/th06rank.c:55,390-406` | ✅复刻 | ✅ |
| **蓝点（金钱 item type 2）+0.06 / +0.006 分档** | `T06_INC_POINT_HIGH = 30000 ÷5 = 6000`（+0.06）；`T06_INC_POINT_LOW = 3000 ÷5 = 600`（+0.006）；判据 = **自机 y** `*(float*)(p+CE_PLAYER_Y(0x624)) <= ce_poc_height_semantic()` **或* | `cards/th06rank_core.h:78-81,84,108-109`；`cards/th06rank_core.c:98-103`；`cards/t | ✅复刻 | ✅ |
| **小 P（item type 1）+0.002，110/111 在场 ×5** | `T06_INC_POWER_SMALL = 1000 ÷5 = 200`（+0.002）；`if (ce_owned(110) |  | 待核 | ✅ |
| **命碎 +0.70 / B 碎 +0.02（逐颗）** | `T06_INC_LIFE_FRAG = 70000`（+0.70）；`T06_INC_BOMB_FRAG = 2000`（+0.02）；type `4 / 0x10` → 命碎、`6 / 0x11` → B碎；每颗**当场**加，含触发进位那一颗 | `cards/th06rank_core.h:75,77`；`cards/th06rank_core.c:124-135`；`cards/th06rank.c: | 待核 | ✅ |
| **整命 +2.00 / 整炸 +0.05（库存正向增量）+ 碎片净减抑制** | `T06_INC_LIFE_FULL = 200000`（+2.00）、`T06_INC_BOMB_FULL = 5000`（+0.05）；`d_life = CURRENT_LIVES[0x4ccd48] − last`、`d_bomb = CURRENT_BOMBS[0x4ccd58] − last`；`d` 钳 `±100`；条件  | `cards/th06rank_core.h:74,76`；`cards/th06rank_core.c:176-207`；`cards/th06rank.c: | ✅复刻 | ✅ |
| **道具出屏 −0.03（逐帧扫池预测）** | `T06_DEC_OFFSCREEN = 3000`（−0.03）；`T06R_ITEM_X_LIMIT 200.0f`、`T06R_ITEM_Y_LIMIT 472.0f`；判据 = `alive(state!=0)` 且（`\ | pos.x\ | 待核 | ✅ |
| **Miss −16.00（非决死，带 `acc` 门）** | `T06_DEC_MISS = 1600000`（−16.00）；挂虚表 `+0x0c on_death_after_deathbomb`；`if (acc != 0) return 0;`（引擎累加器 = 前序卡已救下） | `cards/th06rank_core.h:88`；`cards/th06rank.c:426-445` | ✅复刻 | ✅ |
| **放 Bomb −2.00（含决死 B）** | `T06_DEC_BOMB = 200000`（−2.00）；`on_bomb_spent`（断点 `ce_bomb_spent` = `0x4203bc`，`sites.py:1080`） | `cards/th06rank_core.h:87`；`cards/th06rank.c:418-424`；`sites.py:1080,1370` | ✅复刻 | ✅ |
| **每关开场：subRank 清零 / 帧计数清零 / 基准撤销** | `t06_state_stage_reset`：`rank.sub_rank = 0; stage_frame = 0; window_clear; seeded = 0`（**rank 整数保留**）；`on_stage_start` 里 `max_rank <= 0` 时兜底 `init_all` | `cards/th06rank_core.c:155-164`；`cards/th06rank.c:451-457` | ✅复刻 | ✅ |
| **难度中途变更 ⇒ 按新表重设** | `resync_difficulty`：`t06r_difficulty()`（`0x4ccd00`，只读）变 ⇒ `t06_init(&s->r.rank, d)`（rank 回 16、min/max 换表、sub_rank = 0） | `cards/th06rank.c:175-183,332`；`cards/th06rank.c:57-62` | ❓自定 | ✅ |
| **每关两张碎片卡的补货** | `if (ce_owned(36)) +70000`（Narumi +0.70）；`if (ce_owned(37)) +2000`（Patchouli +0.02） | `cards/th06rank.c:447-460` | 待核 | ✅ |
| **rank → 难度 影响面完全搁置（不写引擎 rank）** | **不写** `0x4ccd08`（TH18 的 rank 死代码）；不 hook `0x42aa20`；**无 HUD**、不显示 rank 数字 | `cards/th06rank.c:9-27,57-62`；`AUDIT.md:4479` §AN AN5 | ✅复刻 | ✅ |
| **冴月麟彩蛋（写字符串）** | `static char k_sakuyue[16] = "\xe5\x86\xb4\xe6\x9c\x88\xe9\xba\x9f"`；`T06R_SAKUYUE_LEN = 9`；`on_load` 里 `memcpy(k_sakuyue, src, 10)` 写自身静态区 | `cards/th06rank.c:77-92,167,504` | ❓自定 | ✅ |
| **调试键：空格 ⇒ 满 P**（**当前被活常量关死**） | 代码：`CE_DEBUG_SPACE_KEYS && ce_key_down(CE_VK_SPACE)`，边沿检测 file-local `dbg_space_prev`；写 `CE_CURRENT_POWER() = [0x4ccd3c]`。**活常量 `CE_DEBUG_SPACE_KEYS = 0`（`sdk.h:872`，2026 | `cards/th06rank.c:334-355`（宏用在 `:346`）；`sdk.h:861,866-872` | 待核 | ✅ |
| **私有状态容量闸门** | `typedef char t06r_state_fits[(sizeof(t06r_state_t) <= 496) ? 1 : -1];`；实测 `sizeof(t06r_state_t) = **456**`（宿主机复现：`t06_state_t=48` + 差分字段，含 `poc_gate`；上限 = `CE_STATE_BYTE | `cards/th06rank.c:100-113`；`sdk_core.h:16,56` | 待核 | ✅ |

### 107 妖妖梦（`cards/senro`）

- 一句话：收道具（原生消弹小分道具 = ★）、结界中擦弹与自机弹命中一起攒**樱点**与 **+C**；+C 满 50000 自动开**森罗结界 540 帧**（结界中每帧全屏收点、背景压暗、自机红白闪），中弹或按 B 打**灵击**（崩结界 + 全屏消弹由引擎撒原生小分道具 + 40 帧无敌，中弹时取消本次死亡），自然到期则樱点/最大樱点各 +10000 并结算 Border Bonus；樱点本身是该卡的

| 机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态 |
| --- | --- | --- | --- | :-: |
| **樱点加法器**（唯一「加」出口） | `cherry += delta` 夹 `≤ cherry_max`、`< 0 → 0`；`delta > 0` 才 `plus += delta`（夹 `SENRO_PLUS_FULL`）；`cherry ≥ cherry_max` ⇒ 置 `SENRO_EV_CHERRY_MAXED(0x08)` | `core.c:104-134`；`core.h:16-20` | ✅复刻 | ✅ |
| **+C 满槽自动开森罗结界** | `SENRO_PLUS_FULL = 50000`（`0xc350`）；判据 `plus >= 50000 && !active && !bomb_active` ⇒ `senro_border_open()`；`bomb_active` 时置 `queued` 并报 `SENRO_EV_BORDER_QUEUE`；排队在炸弹结束帧兑现 | `core.h:106`、`core.c:94-102`、`core.c:120-132`、`core.c:172` | 待核 | ✅ |
| **结界时长** | `SENRO_BORDER_FRAMES = 540`（开结界写 `timer = 540`）；`plus = timer × 50000 / 540` 每帧重写 | `core.h:107`、`core.c:98-99`、`core.c:191` | ✅复刻 | ✅ |
| **结界自然结束结算** | 最大樱点 `+=10000`、樱点 `+=10000`；`last_bonus = round10(cherry)`；`plus = 0`；SE `0x2f`；公告 kind 4 `Border Bonus %7d`；分数 `ce_add_score_delta(last_bonus)` | `core.h:121`、`core.c:176-188`、`senro.c:1081-1093` | 待核 | ✅ |
| **灵击（中弹 / 按 B）** | 门 `senro_border_active()`；收尾 = `active=0/queued=0/timer=0/plus=0`、`invuln = SENRO_INVULN_FRAMES = 40`；全屏消弹 `SENRO_CANCEL_R = 4000.0f`、`MAX = 9999`、`MODE = 1`；`by_hit` 路当场 | `core.h:120`、`senro.c:76-78`、`senro.c:1178-1228`、`senro.c:1231-1258` | 待核 | ✅ |
| **中弹 = 消耗结界救命（救援层）** | `+0x10 on_death_before_deathbomb`：无结界直接返回；有结界 ⇒ `(void)ce_rescue_note_claim(107)` + `senro_do_libyou(by_hit=1)` + `senro_cancel_impending_death()`；**返回值必须丢弃** | `senro.c:1470-1484` | 待核 | ✅ |
| **★（TH18 消弹点）收下 → 樱点/+C 各 +20** | TH18 item `type ∈ [9, 0xe]`（`SENRO_TH18_ITEM_SCORE_LO/HI`）⇒ `d = SENRO_STAR_CHERRY = 20`；**其他 type（含金钱 2 / P 1）返回 0** | `core.h:127`、`core.h:161-162`、`core.c:219-234` | 待核 | ✅ |
| **大樱点（TH07 type 7）** | 值 = `SENRO_ITEM_BIG_BASE(1000) + SENRO_ITEM_BIG_PER_BONUS(100) × spell_bonuses`，樱点与 +C 各加；来源 ① 满 P 转换 `faith_convert_full_p_items()` ② 敌人掉落轮转表 | `core.h:125-126`、`senro.c:1889-1898`、`senro.c:1936-1943`、`senro.c:1966-1980` | 待核 | ✅ |
| **逐敌大樱点轮转表** | `SENRO_ROT_LEN = 32`、`SENRO_ROT_CHERRY = 7`；表 `{0,0,1,0,1,0,0,7,1,1,0,0,7,1,1,0,1,0,1,0,1,0,1,0,1,0,7,1,1,1,0,2}`（**3 项 = 7**）；门 = `ev->flags & CE_EDEATH_F_REAL`；游标每次上卡用  | `core.h:250-254`、`core.c:241-255`、`senro.c:1900-1911`、`senro.c:1711` | 待核 | ✅ |
| **小樱点（TH07 type 8）爆发 / 打包** | 触发 = `!spell_now && spell_was`（符卡结束下降沿）**且** `ce_cancel_frame_count() >= SENRO_SMALL_TRIGGER_MIN(8)`（❓自定）**且** `ce_cancel_latch_owner() < 0`（无人认领）；每颗值 = `SENRO_SMALL_CHER | `senro.c:98-109`、`senro.c:1373-1377`、`senro.c:1838-1878`、`core.h:128-129` | ❓自定 | ✅ |
| **结界中擦弹 → 樱点 + 最大樱点** | 门 `s->active`（结界中）且 `n > 0`；`slow`（聚焦）⇒ 各 `+30`，非聚焦 ⇒ 各 `+80`（`SENRO_GRAZE_SLOW/FAST`）；**只加 cherry 与 cherry_max，不动 +C**；每帧用原生擦弹计数 `0x4ccd0c` 差分取 n | `core.h:130-131`、`core.c:199-210`、`senro.c:64`、`senro.c:1345-1358` | 待核 | ✅ |
| **自机弹命中 → 樱点（无聚焦项）** | `p = (stage >= 5) ? 10 : stage × 2`；`k = SENRO_HIT_DIV(10) − p/3`（`k < 1 → 1`）；`v = (damage / k) × 10`；`v == 0 → SENRO_HIT_MIN(10)`；`v > SENRO_HIT_CAP(70) → 70`；只挂 `on_sh | `core.h:147-149`、`core.c:286-304`、`senro.c:1438-1452` | ❓自定 | ✅ |
| **Miss 扣樱点** | `d = cherry × SENRO_MISS_NUM(1) / SENRO_MISS_DEN(2)`，`> SENRO_MISS_CAP(100000) → 夹`，再 `round10`；`cherry -= d`（夹 ≥ 0）；**不动 +C / 最大樱点** | `core.h:150-154`、`core.c:353-367` | ❓自定 | ✅ |
| **Bomb 扣樱点** | `d = cherry × SENRO_BOMB_NUM(1) / SENRO_BOMB_DEN(4)`，再 `round10`；`cherry -= d`；**不动 +C / 最大樱点** | `core.h:155-157`、`core.c:369-382` | ❓自定 | ✅ |
| **蓝点 / 点道具分值 = 本卡「最大得点」** | `v = SENRO_POINT_BASE(50000)`；`cherry > 50000` ⇒ `v += (cherry − 50000) / SENRO_POINT_DIV(5)`；再 `round10`；卡侧只**声明**（`ce_score_declare_delta(107, v)`）不落笔 | `core.h:134-135`、`core.c:306-318`、`senro.c:1121-1130` | 待核 | ✅ |
| **HUD 版式（原版计数器）** | 数字 VM 槽 `SENRO_HUD_VM_SLOTS = 17`；樱点字段 6 位（`SENRO_HUD_FIELD_CHERRY`）**补零**绘制、**最大樱点**字段画 `cherry_max − cherry_max_base`、6 位或 `≥1000000` 时 **7 位**（`SENRO_HUD_FIELD_MAX`）；+ | `senro.c:308`、`senro.c:314`、`senro.c:323-324`、`senro.c:351-367`、`senro.c:1601-16 | 待核 | ✅ |
| **HUD 三档配色 + 结界中的 +C 呼吸色** | 樱点 `≥ cherry_max` ⇒ `0xFFFFD080` 金黄；否则 `≥ 50000` ⇒ `0xFFFFFF80` 亮黄；否则 `0xFFFFFFFF` 白；最大樱点行 `0xFFF0D0E0`；+C 结界外 `0xFFC080B0`；结界中 `R = 0xFF`、`G = B = (plus×0xc0/50000 + w×6 | `senro.c:415-419`、`senro.c:1501-1521`、`senro.c:1601-1603` | 待核 | ✅ |
| **GUI 公告（单槽互斥）** | `SENRO_TEXT_LIFE = 180` 帧；滑入 `SENRO_TEXT_SLIDE = 30` 帧 `x: 416→104`（`y = 168`）；色 `0xFFE0B0FF`（kind 2/4）、`0xFFC0B0FF`（kind 3）；`SENRO_TEXT_SCALE = 1.4f`；三条字串 `Supernatural  | `senro.c:213-228`、`senro.c:1009-1048`、`senro.c:1667-1673` | ❓自定 | ✅ |
| **「樱点满值」公告节流** | `SENRO_MAXED_POPUP_GAP = 30` 帧；`if (frame − maxed_popup_at >= 30)` 才发 | `senro.c:424`、`senro.c:1094-1102` | ❓自定 | ✅ |
| **结界展开 / 崩结界的大环（世界层 VM 甲）** | 开结界 = 脚本 `SENRO_FIELD_LONG`（371，长命）：每帧跟自机、`scale 1.0 → 0.25 / 540`、`rotation −π/1000 per 帧`、`alpha 0 → 160 / 30 帧`；崩结界 = 脚本 `SENRO_FIELD`（204，第 33 帧自杀）；层 `SENRO_FIELD_ANM | `senro.c:481`、`senro.c:492`、`senro.c:503-511`、`senro.c:752-794`、`senro.c:854-864 | ❓自定 | ✅ |
| **灵击的 32 颗（世界层 VM 乙）** | `SENRO_RING_UNITS = 32`、`SENRO_RING_LIFE = 30`、`SENRO_RING_SPAN = 256.0f`、`SENRO_RING_DEN = 30.0f`；`pos = home + v_i × (帧 × 256 / 30)`；方向表 `s_ring_v[32][2]` 编译期常量，`ang =  | `senro.c:487-490`、`senro.c:516-535`、`senro.c:797-831`、`senro.c:865-881` | 待核 | ✅ |
| **结界持续期：全屏压暗（只压背景）** | `SENRO_DIM_ENABLE = 1`（**活值 = 开**）；层 `SENRO_DIM_LAYER = 5`（背景组最上层）；斜坡 `SENRO_DIM_RAMP = 30` 帧；`g` 满 `0x80`、稳 `0x30` ⇒ `alpha = 255 − (255·g + 64)/128`（稳态 **159**） | `senro.c:198`、`senro.c:203-212`、`senro.c:902-960` | 待核 | ✅ |
| **结界持续期：自机每 2 帧红/白闪** | 判据 `((SENRO_BORDER_FRAMES − timer) & 3) < 2` ⇒ `0xFFFF0000` 红，否则 `0xFFFFFFFF` 白；写 `player+0x14+0x524/0x528`（color_1/color_2，两个都写）；只在 `on_item_frame`（优先级 0x1e）里写，退出结界还原原始色 | `senro.c:229-230`、`senro.c:967-988`、`senro.c:2041-2044` | 待核 | ✅ |
| **震屏（灵击表现）** | `SENRO_SHAKE_TIME = 20`、`SENRO_SHAKE_START = 12`、`SENRO_SHAKE_END = 0` | `senro.c:151-153`、`senro.c:1223` | 待核 | ✅ |
| **音效（SE）** | 开结界 `CE_VOICE_PCB_BORDER`（扩表 `0x5c` = TH07 `se_border.wav`）+ `0x30`；自然结束 `0x2f`；灵击 `0x1b` + `0x2f` | `senro.c:143-147`、`senro.c:1066-1068`、`senro.c:1084`、`senro.c:1220-1222` | 待核 | ✅ |
| **樱点实体走共享点池（110/111 的 `faith.c`）** | `u.slot_mask = 1 << FAITH_SLOT_107`、`u.conv_value = 1`（满 P 转换通道开关）、`art_sprite = SAKURA_POINT(287)`、`art_script = 186`；自机为中心撒一圈，角 `−π/2 + 2π·i/k` | `senro.c:1811-1834`、`senro.c:1865-1874`、`senro.c:1946-1980` | 待核 | ✅ |
| **112 压缩包等价计数折算（跨卡）** | `SENRO_BOUNTY_EQ_FOLD = 1`；只折 `mask & (1 << FAITH_SLOT_107)` 的逐颗账，用**原值**走本卡 `senro_on_cherry_pickup()`；各自记 `s_bounty_eq_seen` 代数 | `senro.c:1282-1305`、`senro.c:1314` | 待核 | ✅ |
| **run 级账本跨关镜像** | `senro_run_keep_t {spell_bonuses, last_bonus, stat[6]}`；`on_stage_start` 存、`on_ctor` 里 `fresh = (s_run_stage < 0 |  | 待核 | ✅ |
| **难度表初值（TH07 六档）** | `cherry_max`：E/N 200000、H 250000、L 300000、EX 400000、PH 400000；`cherry`：仅 EX 200000、PH 300000（其余 0）；上限 `SENRO_CHERRY_MAX_CAP = 9999990` | `core.h:122`、`core.c:12-32`、`core.c:147-153` | 待核 | ✅ |
| **结界中每帧全屏收点** | `ce_item_collect_all()` + `faith_collect_all(~0u)`（连自建绿点一起收，n 累加） | `senro.c:1144-1159`、`senro.c:1342` | 待核 | ✅ |
| **同帧消弹闩锁（跨卡仲裁）** | `ce_cancel_latch_claim(107)`；`n <= 0` ⇒ `ce_cancel_latch_release(107)`（空认领交回）；中弹补做帧即使让位也**照常推进状态机** | `senro.c:1194-1203`、`senro.c:1241-1251` | 待核 | ✅ |
| **HUD 槽认领（钉住，非货架）** | `SENRO_HUD_PIN_X = 68.0f`、`SENRO_HUD_PIN_Y = 928.0f`、`blk.side = CE_HUD_SIDE_PINNED`、`ce_hud_slot_claim_pinned_card(107, …)`；每帧 `if (!slot_ok |  | 待核 | ✅ |
| **HUD 压暗（跨卡统一件 `hud_dim`）** | `hud_dim_init/tick`，矩形取 `ce_hud_band_rect()`（空带兜底回本块），α 与自机位置相关 | `senro.c:543-544`、`senro.c:1559-1568`、`senro.c:1712` | ✅复刻 | ✅ |
| **游玩帧门 / 非游玩帧收 VM** | `on_tick_2` 首行 `if (!s |  | 待核 | ✅ |
| **对话中冻结结界计时** | `if (!CE_GUI_MSG_ACTIVE()) senro_tick(...)`（`CE_BOMB_ACTIVE()` 作为 bomb_active） | `senro.c:1340-1343` | 待核 | ✅ |
| **`on_ctor` 参数 / 状态上限** | `_Static_assert(sizeof(senro_card_t) <= 496, "ce_state 上限 496 字节（超了会静默返回 NULL）")` | `senro.c:583` | 待核 | ✅ |

### 108 永夜抄（`cards/youkai`）

- 一句话：低速（按 Shift 聚焦）时整机切成 TH08 妖侧 —— 本体外观、低速子机、逐（机体 × 火力档）的妖侧出弹组一起换；高速回原机体；同时把敌人的每个发射口按 1:1 长成可打的「使魔」真敌人，打死它那个出点就哑火，使魔挨的伤害转嫁给母机，另带 TH08 的人妖槽（含逢魔）、永点（刻符）与夜点系统。（同台账 ✓）

| 机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态 |
| --- | --- | --- | --- | :-: |
| 低速＝妖侧机体（外观＋子机＋出弹同时切） | 行为判据 = `yk_focused()`；本体脚本号 = `YK_BODY_SCRIPT[4][5]`（追加进 `pl0X.anm` 的 5 支，下标语义 idle/move_l/stop_l/move_r/stop_r = `YK_BODY_STATES 5`） | `youkai.c:377-408`、`youkai.c:7094-7130`；`youkai_core.h:82-87` | ✅复刻 | ✅ |
| 锁形态（放 B 期间固定当前侧） | `s_side_lock ∈ {−1,0,1}`；「真 B」判据 = `bomb_mgr+0x30` 起手帧 **不在**决死窗口；决死窗口 = 自机状态 `4`、持续 `8` 帧；`yk_is_real_bomb()` / `yk_side_switch_allowed()` 三条件取或 | `youkai.c:353-462`；`youkai_core.c:1095-1098` | 待核 | ✅ |
| 灵界 / 暴走咆哮 跨卡只读标志 | 活代码 = `ce_card_flag_any(CE_CARD_FLAG_TRANCE)` / `(CE_CARD_FLAG_ROAR)`；位值 `CE_CARD_FLAG_TRANCE 0x01` / `CE_CARD_FLAG_ROAR 0x02`；写方 = 113 `trance.c:2998`、117 `animal.c:3969 | `youkai.c:413-414`；`sdk_core.h:1011-1012`、`:1022`；`sdk_core.c:2047` | 待核 | ✅ |
| 妖侧主炮 shooterset 认领（替代压原生弹） | `on_shot_set_index` 低速且 `yk_option_wanted(...)` 为真时返回 `yk_youkai_shotset_pick(char,tier)`；优先级默认 `0` ⇒ 113(10)/117(20) 仍胜、111(−1) 输；`YK_SHOT_TIERS 5`、`YK_SHOT_SET_YOUKAI 0 | `youkai.c:7297-7340`；`youkai_core.h:866,880` | ✅复刻 | ✅ |
| 原生弹压制 —— **已退订（死代码＋死开关）** | `on_native_shot_suppress` 定义带 `__attribute__((unused))`（`youkai.c:7415`）；`CE_CARD(108,…)` 里**没有** `.on_native_shot_suppress`；开关 `YK_ENABLE_NATIVE_SUPPRESS 1`（`youkai.c:48 | 定义 `youkai.c:7415-7450`；注册表 `youkai.c:9730-9747` | 待核 | ✅ |
| 本体外观换装时机 | 换在**更新相位** `on_item_frame`（`+0x1c`？见注册表 `youkai.c:9747`），不是 `on_draw`（绘制 0x33）；自愈闸 = 低速下读到 `YK_BODY_SPRITE_FIRST/LAST` 区间外的号就重装 | `youkai.c:7245-7250`、`youkai.c:7116-7126` | ✅复刻 | ✅ |
| 子机数量表（逐机体 × 火力档） | `YK_OPTION_BY_CHAR[4][5]` = 灵梦 `{1,1,1,1,1}` / 魔理沙 `{1,1,1,1,1}` / 咲夜 `{4,4,4,4,4}` / 早苗 `{0,1,1,1,1}`；`YK_OPTION_CELLS 4`（`_Static_assert` 对账 `CE_OPTION_POSE_CELLS`，`you | `youkai_core.h:135,144-148,173` | 待核 | ✅ |
| 子机阵型（逐格偏移） | 格号 `k = T[count−1] + i`（一手 `th18:0x45d8fc`），非聚焦块 `sht+0x40`、聚焦块 `sht+0x90`，单位像素；咲夜那两块是**弧度/半径**、已离线换算成像素；`YK_OPTION_OFFSET_ALLOC 0` | `youkai_core.h:173-190`；`youkai.c:174-177` | ✅复刻 | ✅ |
| 选举落笔（只有 id 最小者写） | `ce_option_pose_winner() != ce_card_id(c)` ⇒ 退位并收子机；每帧 `ce_option_pose_declare`；`want > YK_OPTION_MAX(4)` 钳；`i ≥ want` 的槽必须自己 `ce_option_drop` | `youkai.c:3991-4000`、`:4169-4210`、`:4351-4440` | 待核 | ✅ |
| 弹簧跟随 / 追敌旋转（仅机体 0 霊夢） | `YK_OPT_SPRING_K 1.0f` / `YK_OPT_SPRING_DIV 16.0f` / `YK_OPT_SPRING_DAMP 0.2f`；家位置 `YK_OPT_HOME_Y_REIMU −96.0f`、夹 `≥32.0f`；追敌门 = 有目标 ＋ 出場帧数 ≥ `YK_OPT_CHASE_MIN_AGE 10`；旋转 | `youkai_core.h:223-231,838-841`；`youkai.c:4460-4510` | ✅复刻 | ✅ |
| 子机贴图脚本（按机体原生切图） | `YK_OPT_SCRIPT_YUKARI/ALICE/REMILIA/AYA` = `224/225/226/227`（宏存在时用 `CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_*`）；脚本**申请时定死**，换号须先收再要；`YK_OPTION_RETRY 60` | `youkai.c:122-168`、`:4385-4390` | ✅复刻 | ✅ |
| 使魔＝发射口实体化出的**真敌人**（惰性空壳） | 数量 = 该敌当前活跃发射口槽数（1–4）；四色 = `ability.anm script53..56`；`YK_FAM_MAX 32` 表（32×28B 文件级静态） | `youkai.c:16-19,30-32`；`youkai_core.h:1090-1092,947` | ✅复刻 | ✅ |
| 1:1 映射键（含「同帧第几次」维） | `YK_EMIT_SLOTS 16` × `YK_EMIT_ORIGIN_MAX 6` = `YK_EMIT_KEYS 96`；`YK_EMIT_ORIGIN_KEY(slot,ord) = slot + 16*ord`；同点共壳 `YK_EMIT_SHARE_SAME_POS 1` | `youkai_core.h:937-945`、`:1102-1118`；`youkai.c:861` | ✅复刻 | ✅ |
| 建壳闸（谁才配当使魔） | 发射点离本体 ≥ `YK_EMIT_SHELL_MIN_DIST 48.0f`；`YK_EMIT_REQUIRE_BULLETS 0`；`YK_EMIT_SCENE_WHITELIST 0`；`YK_EMIT_SHELL_TYPES 0`（全接受）；`YK_EMIT_SHELL_TYPE_DENY 0`（关） | `youkai_core.h:1425-1437`；`youkai.c:874-886` | 待核 | ✅ |
| 打死使魔 ⇒ 该发射口关火（主功能 #209） | `broken` 位夹持；复位口 = ECL 重新武装边沿 / 波次边沿（`CE_EDEATH_KILL_ALL` ⇒ `yk_fam_wave_clear`）/ 相位边沿 / 整表 reset；自动重建 TTL = `YK_EMIT_REBUILD_TTL 600` 帧；新波 = `eton_gap ≥ max(YK_EMIT_WAVE | `youkai_core.h:1046,1072,1087`；`youkai.c:7837-7900`、`:6021-6045` | ❓自定 | ✅ |
| 映射使魔血量（≠ 常数 100） | 活代码 = `yk_map_fam_hp(母机HP, 机体)`：档 0（`parent_hp ≤ YK_FAM_HP_TIER0_MAX 200`，含读不到）⇒ `100`，否则 `parent_hp / 3`，再 × 机体千分比 `{1000,350,880,240}`（灵梦/魔理沙/咲夜/早苗），保底 1；`YK_EMIT_FAM_H | `youkai.c:2919-2960`；`youkai_core.c:2317-2336,2278-2301`；`youkai_core.h:2296-230 | 待核 | ✅ |
| spawn 血 <1000 硬约束（两步写血） | `YK_LIFE_LIMIT 1000` / `YK_LIFE_MAX_ALLOWED 999`；spawn 参数 ≥1000 会在建敌时置 `flags_lo | = 0x40000000`（`th18:0x42d8ed cmp …,0x3e8` / `0x42d8f6`）；故先 999 出生、返回后回写 `enemy+0 | 待核 | ✅ |
| 发射口精确关闸（SDK 站点） | `on_enemy_shot_slot = yk_on_shot_slot`；判据 = `(enemy_id, slot)` 曾经有过我们的使魔、现在没了 ⇒ 掐本帧 `etOn`；站点 = `th18:0x433ffa`（`retaddr 0x43402d`） | `youkai.c:9744`、`:7450-7530` | ✅复刻 | ✅ |
| 惰性绕转（附加）使魔 | 生成延时 `YK_ORBIT_DELAY_FRAMES 60`；扫场间隔 `YK_ORBIT_SCAN_INTERVAL 30`；池 `YK_ORBIT_MAX 48`；台数 = `yk_orbit_count_for_hp` 分档 `{200→2, 1000→4, 2000→8, >2000→13}` + 面加成 ≤`YK_ORBIT_ | `youkai_core.h:2306,2403,2421-2430,2454,2479`；`youkai_core.c:2116-2125,2265-2301 | 待核 | ✅ |
| 绕转使魔血量（≠ 一手表） | 活码 `yk_orbit_fam_hp(母机HP, 圈数)`：档 0（≤200）⇒ 100，否则 `parent_hp / rings`（rings 钳 `[1,5]`），**不封顶 999**；旧入口 `yk_orbit_familiar_hp` = 一手表 `{100,100,300,600}` × `YK_ORBIT_HP_SCAL | `youkai_core.c:2265-2276,2330-2340`；`youkai_core.h:2500-2513` | 待核 | ✅ |
| 伤害转交（**按使魔类别分比例**） | `YK_TRANSFER_PCT 100`（绕转/附加使魔）；`YK_TRANSFER_PCT_MAP 50`（发弹口映射 ＋ 外部子敌人）；50% 走半整数进位器 `yk_half_with_carry`（`YK_TRANSFER_DEN 2`）；100% = 逐字节 delta、`carry=0`；防御 `YK_TRANSFER_DE | `youkai_core.h:1456,1467-1468,1479-1480`；`youkai_core.c:1114-1150`；调用点 `youkai.c | ❓自定 | ✅ |
| 转嫁落点 = 使魔自己的母机 | `s_xfer_parent`（＝该使魔的 `parent_enemy_id`，读 `zEnemy+0x6834 = YK_ENEMY_PARENT_ID`）；查不到退全局 boss，再不行 `noboss` 丢；源 = `ce_damage_rect(bp, 0.0f, YK_TRANSFER_SRC_LIFE 2, amount, Y | `youkai.c:3699-3710,3956-3990`；`youkai_core.h:185-190` | 待核 | ✅ |
| 转嫁自激守卫 | 16×16 转嫁盒（`YK_TRANSFER_BOX_W/H 16.0f`）＋四周各加 `YK_FAM_HURTBOX_HALF 12.0f`；盒内的使魔这一帧只采样不喂 | `youkai_core.h:1495-1503`；`youkai.c:3843-3852`；`youkai_core.c:1990-2000` | 待核 | ✅ |
| 转嫁抑制（时符 / 耐久符卡 / 母机无敌） | 位掩码 `YK_XFER_BLOCK_FLAG 1`（`zSpellcard+0x78` bit3 = `YK_SPELL_FLAG_SURVIVAL 8`）/ `YK_XFER_BLOCK_NAME 2`（名字起「時符/时符」）/ `YK_XFER_BLOCK_INVULN 4`（母机 `enemy+0x635c` bit24）；命中  | `youkai_core.h:1407-1416`；`youkai.c:3755-3805` | 待核 | ✅ |
| 死亡点吸收区（TH08 那圈消弹） | 门 = `n_mem > 0`（`n = max(活子敌, 声明槽, 台账记忆)`）；`YK_ZONE_DEATH_RADIUS 32.5f`、`YK_ZONE_DEATH_LIFE 16u`、`YK_ZONE_HIT_ORB 7`；池 `YK_ZONE_MAX 12u`，**池满＝本帧不吐（不 min(N)）**；消弹走 `ce_can | `youkai.c:5910-6025`、`:6160-6225`；`youkai_core.h:2324-2333` | ❓自定 | ✅ |
| 击破撒符（+N 浮字 / 刻符 / 螺旋） | `n = max(活子敌, 声明槽, 记忆值)`；螺旋：`YK_SPIRAL_START_AGE 8`（>8 才开喷）、每帧第一段 `min(quota,7)` 同角 ＋ 第二段固定 `YK_SPIRAL_SECOND_N 6` 颗（+π）、`YK_SPIRAL_RADIUS 128.0f`、9°/帧（40 帧一圈）、锚点 `YK_SPI | `youkai.c:5640-5700`、`:6160-6225`；`youkai_core.h:1915-1929`；`youkai.c:5657` | ❓自定 | ✅ |
| 擦弹 ⇒ 刻符（三道场上门） | `YK_GRAZE_FIELD_GATES 1`；门序 = 机体（本卡映射下恒过）→ `is_boss_active` → `gauge ≥ YK_GAUGE_YOUKAI_EFFECTS(+8000)`；颗数 = 1（非符卡）或 3（符卡中，`ce_spell_active()` = bit0）；**不要求低速**（无 `focused | `youkai.c:8014-8045`、`:9169-9260`；`youkai_core.h:2080-2090` | ❓自定 | ✅ |
| 人妖槽（TH08 `youkaiGauge`，卡侧自建 i16） | 量程 `YK_GAUGE_SPAN 10000`（limits ±10000）；判定 `−8000/−2000/+2000/+8000`；死区 ` | g | 待核 | ✅ |
| 夜点 / PIV（最大得点） | 初值按难度 = `60000/100000/200000/300000`；`PIV += ((delta + (total&1))/2) × 10`（每 2 颗刻符 +10）；池上限 `YK_PIV_POOL_CAP 0x7ffffffe`；显示值 = `piv − piv_base`；计分族 `CE_COEFF_FAMILY_08` | `youkai_core.h:1768,2147-2150`；`youkai.c:6839-6855`、`:9451-9458` | 待核 | ✅ |
| 逢魔（极人/极妖） | `yk_fengma(gauge, −8000, +8000)`；`YK_FENGMA_SCORE_PER_FRAME 100`（`CE_SCORE() += 100/帧`）；极妖 ⇒ 敌弹/残影束低 24 位染红 `YK_FENGMA_TINT_RGB 0x00ff4040`（alpha 不动）；人间逢魔 ＋ 自机弹命中 ⇒ 每发 1  | `youkai_core.h:2170-2173,755-759`；`youkai.c:7760-7835`、`:9440-9450`、`:8686-8700` | 待核 | ✅ |
| 决死窗口全屏敌弹变纯红（风味） | `YK_DEATHBOMB_STATE 4`；颜色 `YK_TINT_RED 0xffff0000`（**只换 RGB、alpha 原样**）；每帧开头无条件还原上一帧；表 `YK_TINT_MAX 256`（满了不再染）；开关 `YK_DEATHBOMB_RED_ENABLE 1`；`on_delete`/`on_run_reset`  | `youkai_core.h:2114-2120`；`youkai.c:6920-6921`、`:6955-6990`、`:7078-7084` | 待核 | ✅ |
| 决死 B 双倍消耗（与 106 协同） | `YK_DEATHBOMB_DOUBLE_WITH_106 1`；`yk_has_card(106)` ⇒ `bombs>0` 时 `CE_CURRENT_BOMBS() −= 1`（clamp ≥0）；`bombs=1 ⇒ 0` 仍允许决死 | `youkai.c:7974-7995`、`:7956-7972` | 待核 | ✅ |
| 人侧撞使魔 ⇒ MISS | 圆 vs 圆：`dx²+dy² ≤ (自机半径 3.0 + YK_FAM_HIT_REACH 12.0)²` = 15.0；只在人侧判；同一只只触发一次（`fam_miss_latch`）；`YK_FAM_HIT_FORCED_DEATH 0`（不用通报位包） | `youkai.c:3135-3180`；`youkai_core.h:2235`；`youkai_core.c:1998-2003` | ✅复刻 | ✅ |
| 魔理沙妖侧子机真激光（卡侧矩形束） | 每台子机一条 32 格膛口轨迹（`YK_M_LASER_BEAM_TRAIL_MAX 32`）＋逐档 `YK_M_LASER_BEAM_RATE {2,4,6,8,12}` 条残影；伤害 `ce_damage_rect(center,0.0f,YK_M_LASER_BEAM_SETTLE 2,dmg_k,16.0f,YK_LASER_LE | `youkai.c:8052-8130`、`:8151-8210`、`:8280-8290`；`youkai_core.h:437-560` | 待核 | ✅ |
| 人妖槽 HUD（4 件 ＋ 百分比 ＋ 夜点） | `YK_HUD_BLOCK_W 256.0f` × `YK_HUD_BLOCK_H 53.0f`；PINNED 槽 `YK_HUD_PIN_X/Y = 124.0f/920.0f`；块基址 `YK_GAUGE_BASE_X/Y = 68.0f/928.0f`；百分比数字 ≤`YK_GAUGE_TEXT_MAX 8` 格、夜点 `YK_NI | `youkai.c:5777-5784`、`:6582-6601`、`:6363-6385`；`youkai_core.h:1697-1712` | 待核 | ✅ |
| Shift 那一下的爆闪 ＋ 短命魔法阵环 ＋ 本体光环 | 爆闪色 `YK_FLASH_FAM_RED 0x80803030` / `YK_FLASH_FAM_BLUE 0x80303080` / `YK_FLASH_BODY_BLUE 0x808080ff` / `YK_FLASH_BODY_RED 0x80ff8080`；环 `YK_RING_MAX 4`、`YK_RING_HOLD_FRAM | `youkai.c:2742-2800`、`:2852-2910`；`youkai.c:1478,2876` | 待核 | ✅ |
| 刻符 VM 池（永点载体） | `YK_ORB_MAX 1024`、层 `YK_ORB_ANM_LAYER 7`；初速 `YK_ORB_VY0 −2.0f` ±`YK_ORB_VY0_RAND 0.2f`、`YK_ORB_VX_RAND 0.6f`、重力 `YK_ORB_GRAVITY 0.05f`；state `0 落/1 归航/3 出生上浮不可收/5 上抛可收`；分 | `youkai.c:4844-4980`；`youkai_core.h:1774-1776,1804-1818` | 待核 | ✅ |
| 刻符提示弹字（+N / 分值） | 池 `YK_POPUP_MAX 128`、`YK_POPUP_GLYPHS 5`、层 `YK_POPUP_ANM_LAYER 16`；寿命 `YK_POPUP_SCORE_LIFE 60` / `YK_POPUP_TIME_LIFE 90`；字形 `YK_POPUP_GLYPH_PLUS 13`（'+'） | `youkai.c:5096-5160`；`youkai_core.h:2185-2192` | 待核 | ✅ |
| 调试功能（空格自杀 ＋ I 无敌） | `YK_DEBUG_SPACE_KILL 1`；`YK_DEBUG_INVINCIBLE 1`、键 `I = 0x49`、每帧把 `player+0x47774` 的 `cur` 刷到 ≥ `YK_DEBUG_INVINCIBLE_MIN 60`；边沿检测（长按不连发） | `youkai.c:790-810`、`:9000-9020` | 待核 | ✅ |

### 110 风神录（`cards/mof`）

- 一句话：装备卡——吃绿点养「信仰」（= 最大得点与计分基），击破指定宿主妖精撒大信仰点；炸弹库存为 0 时按 B（或主动卡 C）发动「灵击」消弹 + 伤害，决死窗口内可救命。（同台账 `CARD-BEHAVIOR-AUDIT.md` §110 ✓）

| 机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态 |
| --- | --- | --- | --- | :-: |
| **击破 → 大信仰点（宿主表口径）** | 宿主键 = `(enemy+CE_ENEMY_ANM_SRC, +CE_ENEMY_ANM_SCRIPT)`（一手 `enemy+0x1494/+0x1498`，写点 `th18:0x436c85/0x436c91`、`0x431659/0x43165f`）；表 = 6 个视觉身份 `(2,10)(2,40)(2,56)(2,68)(2, | 表 `mof_big_faith_table.h:183-196`（`MOF_BIG_FAITH_HOSTS[]`、`mof_big_faith_host_lo | ✅复刻 | ✅ |
| **绿妖精（换精灵 + 换掉落）** | 每行 = `{src_sprite,dst_sprite,src_script,dst_script,enabled,stage_mask}`；一手尺度：`MOF_GREEN_SPRITES 12` / `MOF_GREEN_POSE_MAX 5` / `MOF_GREEN_ENTRY 0x44`；**活表里只有 2 行 `enabled | 表 `mof_green_core.c:48-76`；常量 `mof_green_core.h:48-58`；阶段判据 `mof_green_core.h:98 | 待核 | ✅ |
| **「变绿 ⇒ 掉绿点」的判据（实例级）** | 走 `faith_green_node_owns((int32_t)ev->id)`（共用件按**实例 id** 记账：`faith_gf_node_find(enemy_id)`）；命中的走 `mof_core_wants_big_green()`（= 只查非 boss + 真敌人，**不查宿主表**）；旧路 `mof_green_lo | 卡侧 `mof.c:758-764`；`faith.c:2490-2493`（`faith_green_node_owns`）；`faith.c:2171-21 | 待核 | ✅ |
| **落地「替换 vs 追加」两个口径** | 开关 `MOF_GREEN_WIPE_HOSTS`，活值 **0**（`#ifndef` 兜底 = 0）⇒ 只有 `is_green` 才清原生掉落；清掉落 = `memset(drop, 0, CE_ENEMY_DROP_TIMED_TIME)` + 窗口 `+0xa4` 置 0；第 6 关四色妖精另走**追加**（`anm_src== | 开关 `mof.c:774-776`、用点 `mof.c:781`；清法 `mof.c:639-646`；st06 支 `mof.c:738-757` | 待核 | ✅ |
| **逐帧信仰衰减（门 + 两档速率）** | 三条门**全满足**才推进：① `!mof_boss_alive()`（表首格 `ENEMY_MGR+CE_EM_BOSS_IDS` 非 0）② `!CE_GUI_MSG_ACTIVE()` ③ `stage_frames >= MOF_STAGE_FRAME_GATE = 90`；速率两档：**有击破 `MOF_FAITH_DRAIN_ | 卡侧 `mof.c:1291-1311`；常量+断言 `mof_core.h:482-489`；实现 `mof_core.c:328-360`；卡常量 `mof | ✅复刻 | ✅ |
| **门③「关卡帧」的来源** | **不用引擎计数**：`s->stage_frames` 自增（`on_tick_2` 每帧调一次 `mof_stage_frames()`），重置判据 = `CE_TIME_IN_STAGE()`（`0x4ccce8`）**回退** ∨ `CE_CURRENT_STAGE()`（`0x4cccdc`）**变化**；返回**自增后**的值 | 函数 `mof.c:365-378`；调用 `mof.c:1295`；常量 `mof.c:139` | 待核 | ✅ |
| **大/小信仰点的连击充能** | 大 `MOF_COMBO_BIG = 120`（一手 `0x41b5c5 push 0x78`）/ 小 `MOF_COMBO_SMALL_FAITH = 60`（`0x41b61d push 0x3c`）；**不走** `faith_combo_add(ev)`（共用表把「大」写 130）；`faith_combo_set()` 自夹 ` | 值 `mof_core.h:531-533`、`mof_core.c:383-390`；卡侧 `mof.c:491-495` | 待核 | ✅ |
| **引擎道具 / 击破的连击充能** | `mof_core_item_combo(type18)`：**1 小 P → 60f**（`0x41b46c push 0x3c`）/ **2 金钱 → 100f**（`0x41b56e push 0x64`）/ **3 大 P → 20f**（`0x41b6bd/0x41b6ef push 0x14`）/ 其余 **0**；`MOF_ | 常量 `mof_core.h:555-561`、实现 `mof_core.c:400-408`；卡侧接线 `mof.c:714-723`（kill）、`mof. | 待核 | ✅ |
| **蓝点分值 + 卡内奖残** | 一颗蓝点 = `信仰 × 高度系数`：系数 = **线性插值** `TOP 1000‰ → BOTTOM 540‰`（`MOF_BOARD_Y = 448.0f`，线上 = 1.0、板底 ≈ 0.54，板底下夹住）；阈值 `MOF_BONUS_MAX = 4` 档 = `MOF_BONUS_M 20000000 / 40M 4000000 | 常量 `mof_core.h:94-107`；实现 `mof_core.c:192-203`（插值）、`241-262`（阈值/发放）；卡侧 `mof.c:51 | ❓自定 | ✅ |
| **miss 代价 + 全避代价** | miss：命数差分（`CE_CURRENT_LIVES()` 减少）⇒ `FAITH_REDUCE_MISS` = **−(信仰−50000)/3**（`FAITH_MISS_DIV 3`）+ 连击清 0；全避：`ce_spell_active()` **下降沿** ∧ `flags & CE_SPELL_FLAG_TIMED_OUT(0 | 常量 `faith_core.h:274-276`；状态机 `mof_core.c:411-428`；卡侧 `mof.c:1313-1322`（miss）、`m | 待核 | ✅ |
| **灵击（库存 0 按 B）+ 决死 B 形态** | 触发 = `CE_CURRENT_BOMBS() <= 0` ∧ `INPUT_PRESSED & 0x002`（**上升沿**）；决死窗口 = `player_state == MOF_DEATHBOMB_STATE(4)`（一手 `th18:0x45d4e4`）；形态账本 `MOF_BOMB_BY_PRESS 0 / MOF_BOMB | 卡侧 `mof.c:1366-1374`；常量 `mof_core.h:847-849`、`mof_core.h:863`（`#define MOF_BOMB_ | ✅复刻 | ✅ |
| **灵击消耗（火力门槛 / 档位）** | 常态：门槛 `MOF_BOMB_POWER_MIN = FAITH_POWER_MIN_LEVEL(2)` × `FAITH_POWER_PER_LEVEL(100)` = **≥2.00 档**、扣 `FAITH_POWER_COST(100)` = **1 档**、下限 `FAITH_POWER_FLOOR(105)` = **1.0 | 常量 `mof_core.h:40-42,743-746`、`faith_core.h:325-328`；实现 `mof_core.c:265-312`；卡侧判 | 待核 | ✅ |
| **灵击：半径曲线 / 上移 / 持续帧 / 伤害** | 半径 `16.00 → 目标`、`MOF_BOMB_R_GROW_FRAMES = 120` 帧线性后保持；目标 `灵梦 16000 / 魔理沙 11200 / 符卡中 6400`（1/100 px）；中心每帧上移 `符卡 500 / 灵梦 1000 / 魔理沙 1300`（1/1000 px）；持续 `MOF_BOMB_FRAMES = | 常量 `mof_core.h:666-689`、`712-721`；实现 `mof_core.c:520-599`；一手依据底稿 `mof.c:960-1050 | 待核 | ✅ |
| **灵击伤害载体（为什么用 `ce_damage_rect`）** | 每帧**新撒一个独立伤害源** `ce_damage_rect`（一手 `th18:0x45dfa0` 池），矩形边长 `2r`（`MOF_BOMB_DMG_CAP = 300` 抬本帧伤害上限）；消弹走引擎真圆：`ce_cancel_radius(中心, r, 不限 99999, mode 1)` —— 引擎自带「弹半径 × 0.5」 | 常量 `mof_core.h:734-736`；`bomb10.h:127-128`（`BOMB10_CANCEL_MODE 1` / `BOMB10_CANC | ❓自定 | ✅ |
| **弹字环绿色 `+N` 弹窗** | 颜色 `MOF_POPUP_GREEN = 0xff00ff00u`；槽 = 原生 POPUP_MGR（`CE_POPUP_MGR`）+ 计数器 `+0x10`、`MOF_POPUP_SLOTS = 10`、`MOF_POPUP_STRIDE`、位数 `+0x3d`、`scale 1.0`、busy **最后置**；负数写 `MOF_PO | 常量 `mof.c:181-192`；实现 `mof.c:422-464`；调用 `mof.c:528-539` | ✅复刻 | ✅ |
| **HUD（组合 UI，跨卡共享）** | 本卡**不再持有任何 HUD VM**；`on_draw` 只转 `faith_hud_draw()`；每帧 `faith_hud_declare(FAITH_SLOT_110, &v)`，交出 `v.delta_fixed / v.combo / v.signal = 0`（**不含基础值**）；块 = **188×60 表面 px（6 | 卡侧 `mof.c:1478-1485`、`mof.c:1515-1530`、`mof.c:283-296`；共享件 `faith_hud.c` / `fait | 待核 | ✅ |
| **P 点火力 +0.05 档（跨卡，110 落笔）** | 只管 TH18 `type == MOF_P_ITEM_TYPE18(1)`（小 P）；总量 `FAITH_P_TOPUP_NORMAL = 5` 单位、原生自己给 `MOF_P_NATIVE_UNITS = 1` ⇒ **本卡补差 `MOF_P_TOPUP_UNITS = 4`**；落点必须在 `on_item_commit`（`th1 | 常量 `mof_core.h:321-329`（`MOF_P_TOPUP_UNITS = 4`）；`faith_core.h:222-224`；实现 `mof_ | ❓自定 | ✅ |
| **符卡奖励作废（灵击时）** | 一手门 `[spellcard+0x3738] >= 60`（`th10:0x405500`）⇒ 命中则奖励值 `CE_SPELL_BONUS`（`th18:0x7c`）归 0 + 清 `CE_SPELL_FLAG_BONUS`（`th18:0x78`）**bit1**；本卡**额外要求「符卡进行中」**；已搬进共用机器 `bomb10_ | 卡侧（现只留说明）`mof.c:1146-1149`；一手对照 `mof.c:1003-1008`、`mof.c:1120-1126`；`bomb10.c` | ✅复刻 | ✅ |
| **擦弹倍率（只服务 110+111 同装那一支）** | `mof_core_graze_mult(graze) = min(⌊graze/100⌋, 899) × 10` 千分点（`FAITH_GRAZE_STEP 100` / `FAITH_GRAZE_STEP_MAX 899`）；读数地址 `MOF_GRAZE_ADDR = 0x4ccd0c`；**本卡内唯一读点是诊断日志 ⇒ 不参与任何 | 实现 `mof_core.c:440-448`；常量 `faith_core.h:425-427`；卡侧 `mof.c:1253-1269` | ❓自定 | ✅ |
| **难度表（大信仰点值 / 难度读数）** | 难度读数 = `MOF_ADDR_DIFFICULTY = 0x4ccd00`（0 E / 1 N / 2 H / 3 L / **4 EX**，一手：`collect_money_item` 的 `cmp eax,4` + 5 项跳转表 `0x446d40`）；值映射 `faith_big_mof`：`HARD→8000`、`LUNAT | 地址 `mof.c:109`；映射 `faith_core.c:37-47`；常量 `faith_core.h:255-257`；值注 `mof_core.h: | 待核 | ✅ |
| **主动卡入口（C 键）** | `.active_recharge = 1`（先例 111/116/118 同值）；`on_activate` **只调 `bomb10_start()`**（与 X 路径同一入口）、形态判据同一函数、`return 0` = 瞬发（不接管 `on_active_tick`）；**不加「库存 == 0」闸**；配套要求 `patch/th | 卡侧 `mof.c:1769-1786`、`mof.c:1801-1820`；`mof.c:1727-1768`（论证） | 待核 | ✅ |
| **灵击提供者注册 / 单发保证** | 本卡把 `s_bomb_host`（4 项：`gate` 对话门 / `power_spend` / `on_start` / `on_cancel`）注册进共用机器，每帧幂等补注册；「进行中不放」那道闸由机器把守（一手 `th10:0x42598d [bomb+0x28] test/jne`）；驱动者 = **id 小那张**（110  | 卡侧 `mof.c:1182-1189`、`mof.c:1621`、`mof.c:1374`；`bomb10.h` 硬语义 ①②③ | 待核 | ✅ |
| **消弹 → 微信仰点（每颗 +10 信仰 / +3f 连击）** | 每颗被消的弹 `faith_slot_add_cancel(&slot, 0, bullets, 0, 0)`（110 侧 `FAITH_MOF_WEI = 10`，落 Δ_110）；连击 `while (k-- > 0) faith_combo_add(&slot, FAITH_EV_MICRO)`（**+3f/颗、无每帧上限**）；颗 | 卡侧 `mof.c:541-568`；`faith_core.h:258`；`faith_core.c:118-125` | ❓自定 | ✅ |
| **满 P 转换：原生 P 道具 → 小信仰点** | **P 类 = TH18 type {1 小 P, 3 大 P}**（`MOF_FULLP_ITEM_SMALL_P/BIG_P` = `FAITH_TYPE18_P_SMALL/BIG`）；主路 = 共用件**每帧** `faith_convert_full_p_items()`（满 P 时原位删原生 P 道具 + 原位撒 type 9 | 常量 `mof_core.h:234-239`、`312-315`；判据 `mof_core.c:140-145`；卡侧 `mof.c:817-877`、`mo | ❓自定 | ✅ |
| **112 压缩包被吃下 → 等价计数折算** | 开关 `MOF_BOUNTY_EQ_FOLD = 1`（`#define`，默认开）；读 `ufo_bounty_eq_peek(&eq, &gen)`，**只在 `gen != s_bounty_eq_seen` 时折一次**；逐颗 `ufo_bounty_eq_log(i, &tier, &value, &mask)`，**只折自己槽 | 卡侧 `mof.c:1201-1232`（`mof_bounty_eq_fold` 由 `on_tick_2` 第一句调，`mof.c:1241`） | 待核 | ✅ |
| **中盘拿卡补注册（`on_load` 不被调的那条路）** | `mof_ensure_registered(c)` 每帧幂等调一次：`bomb10_register_host(&s_bomb_host, s)` + 填 `faith_user_t`（`slot_mask = 1<<FAITH_SLOT_110`、`on_pickup`、`on_pickup_at`、`on_cancel`、`conv | 卡侧 `mof.c:1192-1198`、`mof.c:1607-1641`；调用点 `mof.c:1243`；`on_load` 同名调用 `mof.c:16 | 待核 | ✅ |
| **跨关账本保留（Δ/连击/奖残「一局一份」）** | 文件静态镜像 `s_slot_keep`（`faith_slot_t`）+ `s_bonus_keep`（`mof_bonus_t`）+ 哨兵 `s_slot_stage`（初值 −1）；`ctor` 判据 = `s_slot_stage < 0 ∨ CE_CURRENT_STAGE() < s_slot_stage` ⇒ 新局 `fai | 卡侧 `mof.c:249-262`、`mof.c:1662-1692`、`mof.c:1700-1701`、`mof.c:1719-1721`、`mof.c: | 待核 | ✅ |
| **灵击的无敌帧（一次性）** | `BOMB10_INVULN_FRAMES = 270`（普通）/ `BOMB10_INVULN_FRAMES_DEATHBOMB = 200`（决死）；走 SDK 声明层 `ce_invuln_request()`（每帧声明**剩余**帧数，机器自己数到 0 声明 0；`bomb10_reset()` 显式清）；**TH11 的 40  | `bomb10.h:16-52`（论证）、`bomb10.h:138-139`（常量） | ✅复刻 | ✅ |
| **灵击的 RANK 旋钮 + 音效** | `BOMB10_RANK_DROP = 0x80`、`BOMB10_RANK_MIN = -0x400`、`BOMB10_RANK_MAX = 0x400`（写 `BOMB10_RANK_ADDR = 0x4ccd08`）；`BOMB10_SE = 0x2c`（TH10 `se_slash.wav` 播 id `0x26`） | `bomb10.h:113-118`（常量）、`bomb10.h:83-88`（依据） | ✅复刻 | ✅ |
| **「小灵击档」压制白名单（默认不启用）** | `k_suppress_ids[]` = **空数组** ⇒ `mof_core_bomb_suppressed(spell_id)` 恒返回 0；TH10 的一手名单 `0x5d..0x60 ∪ {0x6d}` 只留在注释里 | 实现 `mof_core.c:533-545`；接口与依据 `mof_core.h:757-766` | 待核 | ✅ |

### 111 地灵殿（`cards/signal`）

- 一句话：把 TH11 的通信强度槽 + 信仰/蓝点分 + 灵击/命碎 + 12 伙伴支援特技整层搬进 TH18：擦弹与站位养一条 0–130 的信号槽，满槽全收点，槽值当计分乘数（同台账 §111）。

| 机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态 |
| --- | --- | --- | --- | :-: |
| **信号槽的量纲与上限** | 槽 = 千分点，`SIGNAL_UNIT = 1000`（1.00 显示）；`SIGNAL_MAX = FAITH_SIGNAL_MAX = 130000`（显示 130.00）；`SIGNAL_HI_MARK = FAITH_SIGNAL_HI_MARK = 100000`（显示 100.00 = 满） | `native/cards/signal_core.h:44-47`；被别名源头 `native/cards/faith_core.h:371-372` | ✅复刻 | ✅ |
| **信号槽每帧推进（线上/线下四档）** | `faith_signal_step_gated()`：① 未满（<100000）+ 线上 ⇒ `d = FAITH_SIGNAL_GAIN_LINE = 50000`（+50.00）但**先钳到** `HI_MARK - now`（=「补满并钳到 100.00」，**不是每帧 +50 累加**）；② 已满 ⇒ TOP `+160` /  | 实现 `native/cards/faith_core.c:236-264`；常量 `native/cards/faith_core.h:373-376,392 | ✅复刻 | ✅ |
| **回收线四档的判据** | `signal_core_band(above_line_px_x100)`：`> FAITH_SIGNAL_BAND_TOP_PX*100=6400` ⇒ TOP；`> 4800` ⇒ MID；`> 0` ⇒ LOW；**`<= 0`（恰好在线）⇒ OFFLINE**；卡侧另有 `pstate == 2 |  | 待核 | ✅ |
| **擦弹 → 信号（按难度查表）** | 子弹/敌体擦弹：`EASY = 8000`、`NORMAL = 5000`、`HARD = 5000`、`LUNATIC = 5000`、`EXTRA = 5000`（**只有 EASY 不同**）；激光擦弹：`5000/4000/3000/2000/2000`（**当前无调用者**）。映射：`FAITH_DIFF_LUNATIC` 与  | 值 `native/cards/faith_core.h:398-407`；分派 `native/cards/signal_core.c:19-42`；调用 ` | ✅复刻 | ✅ |
| **擦弹倍率 B（整数阶梯）** | `mof_core_graze_mult(g) = min(⌊graze/100⌋, 899) × 10` 千分点；`FAITH_GRAZE_STEP = 100`、`FAITH_GRAZE_STEP_MAX = 899`、`FAITH_GRAZE_MULT_MAX = 8990`（⇒ 8.99）；触发点 = 89900 擦弹 | `native/cards/mof_core.c`/`mof_core.h`（`FAITH_GRAZE_*` 定义在 `native/cards/faith_c | ✅复刻 | ✅ |
| **信号强度 A（进分式的那一项）** | `signal_core_strength()`：`signal<=0 ⇒ 0`；`>= SIGNAL_HI_MARK` ⇒ 返回 `SIGNAL_UNIT = 1000`（**满槽 ≥100.00 一律 1.00，130 与 100 同值**）；否则 `a = signal/1000`（0..99），返回 `a*10`（千分点） | `native/cards/signal_core.c:105-116`；声明点 `signal.c:461`（`ce_score_mul_declare`） | ✅复刻 | ✅ |
| **蓝点分（整数式）** | `signal_blue_value()`：`Y = trunc10(信仰显示值)`（截断到 10 的倍数）；`X = A + B`（单位 0.01）；`S = ⌊X*Y/100⌋` 再 trunc10；`S <= 0 ⇒ SIGNAL_BLUE_MIN = 10`（`#define SIGNAL_BLUE_MIN 10`，`signal | `native/cards/signal_core.c:119-150` | ✅复刻 | ✅ |
| **蓝点分不参与落笔（计分层重做）** | 本卡只发两条声明：`ce_score_declare_coupled(111, Δ_11)`（**减去 `s->slot.base`**）+ `ce_score_mul_declare(111, signal_core_strength())`；`ce_score_family_declare(111, CE_COEFF_FAMILY_1 | `signal.c:448-462`、`signal.c:827-847`；旧式 `native/cards/signal_core.c:158-186` | ✅复刻 | ✅ |
| **满槽全收（≥100.00 ⇒ 全屏收道具）** | 判据 `signal_core_is_full(sig) = sig >= SIGNAL_FULL_AT`（= `FAITH_SIGNAL_HI_MARK` = 100000）；动作 = `ce_item_collect_all()`（原生道具池 state←3）**+** `faith_collect_all(~0u)`（自建绿点池） | 判据 `native/cards/signal_core.c:90-93`；常量 `signal_core.h:47`；调用 `signal.c:3441`；实 | ✅复刻 | ✅ |
| **被弹归零 + 信仰惩罚（miss）** | `on_death_after_deathbomb`：**`acc != 0` 直接 return**（前面已有卡救下）；否则 `signal_core_miss_penalty()`（`FAITH_SA_MISS_PIV_LOSS = 100000`，Δ 夹 0）+ `signal_core_reset()`（只写 `s->signal | `signal.c:3233-3258`；常量 `native/cards/faith_core.h:271`；扣减 `faith_core.c:215-225 | ✅复刻 | ✅ |
| **命碎段机（每 2 段没死过 ⇒ 撒 1 颗命碎道具）** | `SIGNAL_SEG_DROP_EVERY = 2`；`SIGNAL_SEG_MIN_FRAMES = 100`（**段长 < 100 帧只记诊断、不记账**）；`SIGNAL_SEG_DEBOUNCE = 5`；段边界 = boss 1→0 / 相位指纹变化 / 符卡位边沿；对话中 `CE_GUI_MSG_ACTIVE()` ⇒ 只吸 | 常量 `native/cards/signal_core.h:266,268,283`；状态机 `native/cards/signal_core.c:274- | ✅复刻 | ✅ |
| **命碎道具的载体 type** | `SIGNAL_ITEM_TYPE_LIFEFRAG = 0x10`（判据 `type == 0x10`）；`SIGNAL_LIFEFRAG_PER_LIFE_TH18 = 3`（3 碎 = 1 残，原生）；TH11 = 5 | `native/cards/signal_core.h:193-199`；判据 `native/cards/signal_core.c:222-228` | ✅复刻 | ✅ |
| **得分奖残（跨分数阈值 +1 残机）** | `SIGNAL_LIFEFRAG_TABLE_LEN = 1`，表里唯一一项 `1'000'000'000`；判据 `score >= thr` ⇒ `ce_add_life()` + `ce_gui_update_lives()`；索引 `frag_idx` **一局内不重置**（只在 `on_run_reset` 清） | `native/cards/signal_core.h:201`；表/判据 `native/cards/signal_core.c:196-220`；卡侧 `s | ✅复刻 | ✅ |
| **灵击（B/X）—— 实现整体在共用件** | **`signal.c` 里那四个 `SIGNAL_BOMB_*` 全是死定义**（同名 `#define` 出现两次，值与恒等重定义）：`DMG 32 / DMG_LIFE 75 / HALF_W 20 / HALF_H 30`，文件里**唯一引用只有注释**；活实现 = `bomb10_start()` / `bomb10_tick( | 死定义 `signal.c:209-217`；`signal.c:180`（`SIGNAL_BOMB_CANCEL_MAX`，**另有用**：`signal.c | ✅复刻 | ✅ |
| **灵击的触发条件（X 路径）** | `CE_CURRENT_BOMBS() <= 0` **且** `INPUT_PRESSED & SIGNAL_INPUT_BOMB(0x002u)`；形态 = `signal_core_deathbomb_window(pstate)` ⇒ `by_hit`；然后 `bomb10_start(&s_bomb_host, s, by_hi | 触发 `signal.c:3451-3458`；掩码 `signal.c:177`；窗口 `native/cards/signal_core.c:758-762 | 待核 | ✅ |
| **灵击 → 决死形态（决死 B）** | 窗口判据 `player+0x476ac == 4`（`SIGNAL_DEATHBOMB_STATE = 4`）；决死形态下：消弹**照常记账**（`SIGNAL_BOMB_BY_HIT_ACCOUNT = 1`）、**不开**荷取退款窗（`SIGNAL_BOMB_BY_HIT_REFUND = 0`）、另调 `bomb10_cancel | 常量 `native/cards/signal_core.h:922-932`；窗口函数 `signal_core.c:758-762`；救命地址 `bomb1 | ✅复刻 | ✅ |
| **灵击的门槛与消耗（TH18 映射）** | `FAITH_POWER_MIN_LEVEL = 2`（≥2.00 档）、`FAITH_POWER_COST = 100`（扣 1 档）、`FAITH_POWER_FLOOR = 105`（下限 1.05 档）；魔理沙（爱丽丝）= **半档**：`SIGNAL_ALICE_BOMB_COST_UNITS = FAITH_POWER_COS | 常量 `native/cards/faith_core.h:325-328`；爱丽丝 `native/cards/signal_core.h:634-647`、 | ✅复刻 | ✅ |
| **主动卡入口（C 键）** | `CE_CARD` 里 `.active_recharge = 1` + `.on_activate = on_activate`；`on_activate()` 只做：取 `pstate` → `signal_core_deathbomb_window()` → `bomb10_start()`，`return 0`（瞬发，**不**声 | `signal.c:4145-4151`、`signal.c:4125-4143` | 待核 | ✅ |
| **绿点三档（大/中/小）** | 大 `FAITH_SA_BIG = 100`（击破掉落，**boss 不掉**）；中 `FAITH_SA_MID = 10`（满 P 转换 type 9）；小 = **每 100 颗 +10**（`FAITH_SA_SMALL_N = 100` / `FAITH_SA_SMALL_V = 10`，`small_rem` 余数跨关保留）；` | 值 `native/cards/faith_core.h:260-267`；别名 `signal_core.h:50-53`；小绿点进位 `faith_core | ✅复刻 | ✅ |
| **大绿点的触发判据（三条或门）** | `mof_core_wants_big_main(main_drop, is_boss) |  | 待核 | ✅ |
| **大小姐（咲夜×蕾米）逐弹周期擦弹** | `SIGNAL_REMILIA_GRAZE_ON_FRAMES = 1`、`SIGNAL_REMILIA_GRAZE_OFF_FRAMES = 11`；活周期 = **1 + 11 = 12 帧**；`SIGNAL_REMILIA_TRACK_MAX = 36`（超表当帧不计）；`r->total` **跨关保留** | `native/cards/signal_core.h:1021-1035`；`signal_core.c:810-851`；卡侧 `signal.c:3355 | 待核 | ✅ |
| **二小姐（咲夜×芙兰）蓄力清屏** | `SIGNAL_FLANDRE_CHARGE_FRAMES = 300`（5 s @60fps）；半径 = 擦弹圈额外段 `SIGNAL_FLANDRE_CIRCLE_MIN = 40` px × `SIGNAL_FLANDRE_RADIUS_MUL = 2`；消弹走 `ce_cancel_radius(pos, r, SIGNAL_BO | `native/cards/signal_core.h:1075-1077`；`signal_core.c:859-913`；卡侧 `signal.c:3001 | 待核 | ✅ |
| **红美铃（咲夜×美铃）信号增速 ×1.5 与下限 50.00** | `SIGNAL_MEILING_FLOOR = FAITH_SIGNAL_HI_MARK/2 = 50000`（显示 50.00）；`GAIN_NUM/DEN = 3/2`（×1.5）；**负增量（线下 −0.30）不乘** | `native/cards/signal_core.h:1141-1143`；应用点 `signal_core.c:1015-1060`；接线 `signal. | 待核 | ✅ |
| **诹访子（早苗×诹访子）贴板底线上不扣** | 判据 `signal_core_at_board_bottom(y_fixed)`，板底常量 `SIGNAL_BOARD_BOTTOM_FIXED = 0xd800`（432.0 px @1/128）；贴底 ⇒ 线下那一档换成「不推进」 | `native/cards/signal_core.h:1164`；`signal_core.c:915-926`；接线 `signal.c:3391-3399 | 待核 | ✅ |
| **神奈子（早苗×神奈子）收点线下移** | `SIGNAL_KANAKO_POC_DROP_PX = 96`（追加量）；基准 `SIGNAL_KANAKO_POC_BASE_DEFAULT_PX = 128`；基准 > 默认（已有别人动过，如原生 22 号卡写成 224）⇒ **追加减半 48**；走共享 `ce_poc_*` 声明层 | `native/cards/signal_core.h:1212-1221`；`signal_core.c:928-967`；接线 `signal.c:3896 | ❓自定 | ✅ |
| **恋恋（早苗×恋恋）放开键 4 帧无敌** | `SIGNAL_KOISHI_INVULN_FRAMES = 4`；`ce_invuln_request(111, 4)`，粘性声明层，释放时显式撤 | `native/cards/signal_core.h:1332`；`signal_core.c:975-1013`；撤销 `signal.c:3141` | ✅复刻 | ✅ |
| **紫（灵梦×紫）缺口瞬移** | `SIGNAL_GAP_EDGE_FIXED = 0x5c00`（TH18 的 x 钳位值）、`SIGNAL_GAP_WINDOW_VERDICT = 8` 帧（TH11 窗口）、方向 `SIGNAL_GAP_DIR_LEFT/RIGHT = 1/2`、`SIGNAL_GAP_SE_TH11 = 0x30`、`SIGNAL_GAP_FRA | `native/cards/signal_core.h:471-480`；`signal_core.c:395-479`；瞬移调用 `signal.c:1673 | 待核 | ✅ |
| **文（灵梦×文）超高速** | `SIGNAL_HYPER_SPEED_PERMILLE = 1500`（1.50×）；进入门 `SIGNAL_SUPPORT_HOLD_FRAMES = 10`（`SIGNAL_HYPER_MIN_FRAMES = 11`）；按住 Z/Shift 族；写完引擎每帧自己复位（卡侧删卡时显式写回 1.0） | 常量 `native/cards/signal_core.h:522,543,545`；`signal_core.c:481-505`；卡侧 `signal.c | ❓自定 | ✅ |
| **帕秋莉（魔理沙×帕秋莉）五档子机阵型** | `SIGNAL_FORMATION_COUNT = 5`、`SIGNAL_FORMATION_CELLS = 4`、`SIGNAL_FORMATION_LEVELS = 4`；`SIGNAL_PATCHOULI_FRESH_FRAMES = 5`（同时按 Z+Shift 的新鲜度窗，一手 `0x43605e/0x43606d` 的两个 ` | 常量 `native/cards/signal_core.h:423,665-678`；几何 `signal_core.c:563-620`；卡侧 `signa | ✅复刻 | ✅ |
| **子机格数 = 原生火力档 + 原生子机抑制** | `signal_power_cells(power, per_level, cells_max) = clamp(power/per_level, 0, cells_max)`（**0 档 = 0 格**）；上限 `SIGNAL_OPTION_MAX = CE_OPTION_POSE_CELLS`（4）；子机贴图脚本「只在申请那一刻定死」 | `signal.c:299-335`（静态槽 + 核对）、`signal_core.c:637-644`、`signal_core.h:675`；落笔 `sig | 待核 | ✅ |
| **信仰点 / 绿点池是自建 VM（不是原生道具）** | 共用件 `faith.c` 静态池；撒点 O(1) 只写池、**ANM VM 由 `faith_pool_tick()` 隔 1–4 帧再建**（不在敌人死亡栈里建引擎对象）；`slot_mask` 分账 | `signal.c:632-641`（注释）、`709-714`；`signal.c:857-861`（全收自建池） | ✅复刻 | ✅ |
| **HUD 已全部搬进共享件（本卡不持有 VM）** | 合并块 = **188×60 表面 px**（640 空间 94×30）；脚本 212/213/214；块由共享件按「在场的、id 较小的那张」认领（110 优先）；本卡 `on_draw` 只转发 `faith_hud_draw()` | `signal.c:237-295`（历史+去向）、`3687-3748`（退役说明 + `on_draw`）；`signal.c:3868-3877`（不改认 | ❓自定 | ✅ |
| **HUD 填充条宽度（如仍被引用）** | `width_px = 9 + 4 × level`，`level = min(⌊signal/1000⌋,100)/10 ∈ 0..10` ⇒ **9..49 px**（`SIGNAL_BAR_BASE_PX = 9` / `STEP_PX = 4` / `LEVEL_MAX = 10`） | `native/cards/signal_core.h:327-329`；`signal_core.c:372-390` | ✅复刻 | ✅ |
| **跨关账本（Δ / 小绿点余数跨关保留，信号槽每关归零）** | 文件静态 `s_slot_keep111` + 哨兵 `s_slot_stage111`；`ctor`：`stage < 哨兵` ⇒ `faith_slot_init_111()` 新局；否则恢复镜像并 `faith_signal_reset(&s->slot)`（**只清 signal**）；`on_stage_start` 也 `si | `signal.c:102-108`、`3900-3924`、`3970-4001`；归零只写 signal `faith_core.c:279` | 待核 | ✅ |
| **retry / 回主菜单的账本归零（P21/P25 修的 bug）** | `signal_clear()` 尾部：`faith_slot_init_111(&s_slot_keep111, signal_difficulty())` + `s_slot_stage111 = -1`；`on_run_reset` 也补同样两行；`ctor` 判据用 `s_slot_stage111 < 0 |  | 待核 | ✅ |
| **卡私有状态上限** | `_Static_assert(sizeof(signal_card_t) <= 496, ...)`；`ce_option_t s_opt[4]` 因超限**搬进文件静态区**；`s_bomb10` / `s_nitori` / `s_tint_*` 同理 | `signal.c:425`（static_assert）、`308-315`（s_opt 搬家）、`380-383`、`398-400` | 待核 | ✅ |
| **灵击/伤害/特效的跨帧状态在共用件（110/111 同装只跑一份）** | `bomb10_start()` / `bomb10_tick(driver)`；**驱动者 = id 小者**：`bomb10_tick(ce_owned(110) ? 0 : 1)`（110 在 ⇒ 111 不推）；`s_bomb_host.gate = 0`（卡私有闸全在机器里） | `signal.c:3493-3500`、`1617-1625`；`native/cards/bomb10.c:368,557` | 待核 | ✅ |
| **河童罩子（魔理沙×荷取）** | `SIGNAL_NITORI_SHIELD_FRAMES = 920`（= 淡入 10 + 长大 20 + 缩 900）；`SIGNAL_NITORI_END_SCALE_MILLI = 100`（0.1）；退款 = `SIGNAL_NITORI_REFUND_NUM/DEN = 1/2`（半档）；破罩 `BREAK_FRAMES = 3 | 常量 `native/cards/signal_core.h:796-814`；状态机 `signal_core.c:649-757`；卡侧 `signal.c | ❓自定 | ✅ |
| **灵击的「范围环 / 电流边 / 火星」视觉** | `BOMB10_RING_VMS = 6`（一手 script400 六次 `scriptNew`）、`RING_LEAF_FIRST = 31`、`RING_LEAF_EVERY = 30`、`RING_LEAF_UNITS = 4`；这些 VM **不会自灭** ⇒ `bomb10_reset()` 必须显式删 | `native/cards/bomb10.h:123-127`；清理由 `signal.c:3160-3165`（`bomb10_reset()`） | 待核 | ✅ |

### 114 辉针城（`cards/collect2x`）

- 一句话：TH14 式收点批 —— 把「真空飞行收点」的 P / 钱（外加 110/111 的自建绿点）攒成一批，批闭时按批大小走档位倍率给分数补差、每批弹 1 枚碎片卡；另有一座被动：非聚焦命中敌人掉 P（同 CARD-BEHAVIOR-AUDIT.md §114）。

| 机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态 |
| --- | --- | --- | --- | :-: |
| 注册与钩子集 | `CE_CARD(114, …)` 只挂 6 钩子：`on_item_frame`/`on_item_collect`/`on_item_commit`/`on_shot_damage`/`on_stage_start`/`on_run_reset`；**无 ctor / on_load / on_delete** | `collect2x.c:516-518` | ✅复刻 | ✅ |
| 帧源 = 自计游玩帧 | `static int32_t s_play_frame`（`:97`）；`on_item_frame` 首 `++s_play_frame`（`:238`）→ `now`；节流/延迟/idle 全用它，**不用** `card+0x20` zTimer | `collect2x.c:91-102, 238-239` | 待核 | ✅ |
| 收点批成员门（开批/计数） | `type ∈ {1(P), 2(钱)}`（`type_in_batch`）**且** `item + CE_ITEM_STATE(0xc74) == CE_ITEM_STATE_VACUUM(3)`；否则 return（触收 state1 / 70px 环收 state4 / 转换点 9–0xe 不入批） | `collect2x.c:449-453`；`collect2x_core.c:8-11,13-20`；`engine.h:898` | ✅复刻 | ✅ |
| presence 续批 + 批闭 idle | 批开时每帧 `ce_item_pool_any_attracting()`（实现见下）或 `ce_vitem_flying()>0` → `c2x_batch_presence()` 写 `last_event_at=frame`；关批门 `frame − last_event_at ≥ C2X_BATCH_SETTLE_IDLE` | `collect2x.c:284-300`；`collect2x_core.c:22-27,53`；`core.h:112`（`#define C2X_BATC | ✅复刻 | ✅ |
| ↳ presence 谓词覆盖面（**与 .h 注释冲突**） | 活代码：`sdk.c:3225` `if (state == CE_ITEM_STATE_VACUUM) return 1;` ⇒ **只认 state 3**；接线同（`collect2x.c:451`） | `sdk.c:3223-3225`；`collect2x.c:284` | 待核 | ✅ |
| ↳ 同段过期值（**文件内自相矛盾**） | 活值 8（`core.h:112`） | `collect2x.c:375` | 待核 | ✅ |
| 档位阈值 | `C2X_TABLE_T1..T5 = 20/30/40/50/60`；`count < T1` → 关批无奖（out 全零，返回 1） | `collect2x_core.h:36-40`；`collect2x_core.c:63-68,68` | ✅复刻 | ✅ |
| 档位倍率 | `C2X_TABLE_M1..M5 = 500/800/1200/1600/2000`（permille）；`core.c` 五档 `if/else` | `collect2x_core.h:42-46`；`collect2x_core.c:63-69` | ✅复刻 | ✅ |
| 分数补差（倍率只作用分数） | `delta = out.batch_score × (mult_permille − 1000) / 1000`（int64，截断向零）；`delta != 0 → ce_add_score_delta(delta)`（可负，钳在帮手内） | `collect2x.c:340-341`；`collect2x_core.h:27-28`；`collect2x_core.c:59-60` | ✅复刻 | ✅ |
| 批分捕获（stash） | 收集断点存 `s_stash = CE_SCORE()`、`s_stash_valid=1`（在 type 分派前）；提交断点 `delta = CE_SCORE() − s_stash`，`batch_open` 时 `batch_score += delta`，否则只打日志 | `collect2x.c:80-81,459-460,463-474` | 待核 | ✅ |
| 自建实体成员通道（绿点入批） | `ce_vitem_take_collect() > 0` → `c2x_collect_virtual(s, n, now)`：**只加 count、开批 + 写批位，不碰 batch_score**；在 `batch_open` 门之前 | `collect2x.c:255-268`；`collect2x_core.c:29-40`；`sdk_core.h:947`（`CE_VITEM_HOLD_F | ❓自定 | ✅ |
| 碎片奖励：4:1 循环 + ≥60 命碎 | `C2X_FRAGMENT_CYCLE 4`；`count ≥ 60` → `C2X_FRAG_LIFE`（游标不动）；否则 `cycle ≥ 4` → 归 0 且 `C2X_FRAG_LIFE`，else `cycle++` → `C2X_FRAG_BOMB`；`out.fragment_count = 1` | `collect2x_core.h:48`；`collect2x_core.c:71-81` | ✅复刻 | ✅ |
| 碎片载体（TH18 原生卡道具） | `C2X_FRAG_BOMB 0x11`（BOMB2 卡道具 → `CardBombFragment(id 4)` 符卡碎）/ `C2X_FRAG_LIFE 0x10`（EXTEND2 卡道具 → `CardLifeFragment(id 3)` 命碎） | `collect2x_core.h:54-55`；`collect2x.c:342-348` | ✅复刻 | ✅ |
| 碎片弹出延迟 | `#define C2X_FRAG_SPAWN_DELAY_FRAMES 20`；结算帧只登记 `pending_frag_*` + `pending_frag_at = now + 20`；`on_item_frame` 到点才 `spawn_pending_frags` | `collect2x.c:195,240-241,349-360` | ❓自定 | ✅ |
| bonus 锚点（TH14「批位」） | 成员加入时 `note_bonus_anchor()` 写 `anchor_x/y = *(float*)(CE_PLAYER()+CE_PLAYER_X/Y)`（成员门 `:458`、虚拟通道 `:264` 两处）；消费者：弹字 `pz = {ax, ay − C2X_POPUP_Y_OFFSET}`、碎片 `sy = pending_ | `collect2x.c:104-138,215-221,325-327`；`core.h:80`（`C2X_POPUP_Y_OFFSET 48.0f`）、`: | ✅复刻 | ✅ |
| 碎片生成点/初速/角（物性） | `spawn_item_at(type, x, y, speed)` → `ce_spawn_item(type, p3, −1.5707964f, speed)`；碎片 = 批位快照 −48、**2.2f**；横向散开 ±12 只在 `n>1`（core 恒 `n=1`）；x 钳 ±192 由引擎做、y 不夹 | `collect2x.c:140-145,215-221` | ❓自定 | ✅ |
| 低批 ×0 灰字「NO BONUS」 | `out.mult_permille == 0` 且 `count ≥ 5` → `ce_popup_mult(pz, −1, 0x80a0a0a0, 0.0f)`；<5 无弹字；无音、无碎片、无分 | `collect2x.c:329-338` | ✅复刻 | ✅ |
| 倍率弹字（原生 PopupManager） | `CE_POPUP_RING_RATE 0x9cc` + `CE_POPUP_RATE_SLOTS 5`、stride `0x48`；值 int（= batch_score）；色 `tier_color()`：2000→`0xffffff00` / 1600→`0xe0ffff60` / 1200→`0xd0c0ff80` / 800→`0xc0a0ffa` | `collect2x.c:148-157,165-191,366`；`engine.h:942-943` | 待核 | ✅ |
| 结算钟声 + 声像 | `ce_play_sound(CE_SE_PIN00, pz[0])`；`CE_SE_PIN00 0x39`；声像 = 批位 x | `collect2x.c:367-377`；`engine.h:537` | ✅复刻 | ✅ |
| 满 P 道具 → 共享 PIV +100 显示 | `type == C2X_ITEM_TYPE18_FULL_POWER(8u)`；`mx = *(volatile int32_t*)CE_ADDR_MAX_POWER(0x4ccd3c)`；`mx > 0 && CE_CURRENT_POWER() >= mx` → `ce_piv_add(114, C2X_PIV_FULLP_ITEM | `collect2x.c:418-420,425-439,234`；`sdk_core.c:872,880,892` | ✅复刻 | ✅ |
| 高速射击掉 P（被动二） | `on_shot_damage` → `c2x_maybe_drop(s, frame_now(), CE_CURRENT_POWER())`；`frame ≥ next_drop_at` → `next_drop_at = frame + interval`；`interval = 96 − power_x100×16/100`（截断向 | `collect2x.c:476-490`；`collect2x_core.h:114-116`（`BASE 96`/`PER 16`/`MIN 1`）；`co | ✅复刻 | ✅ |
| 换关复位 | `on_stage_start` 清：`batch_open/batch_count/batch_score`、`pending_frag_count/type`、`anchor_valid`、`s_stash_valid`、`s_batch_virtual/s_batch_vhold`、`next_drop_at`；**run 级保留* | `collect2x.c:492-501` | 待核 | ✅ |
| 新局复位 | `on_run_reset`：`memset(s, 0, sizeof *s)`（含 4:1 游标）+ `s_stash_valid=0` + `s_batch_virtual/s_batch_vhold/s_virtual_total=0` + `s_play_frame=0` | `collect2x.c:503-514` | 待核 | ✅ |
| 断点/站点 | `ce_item_frame 0x446ee3` / `ce_item_collect 0x44625f` / `ce_item_commit 0x44658a` / `ce_shot_damage 0x45f2a2` | `sites.py:1097,1100,1104,1108`；文件头 `collect2x.c:61-63` | 待核 | ✅ |

### 115 绀珠传（`cards/lolk`）

- 一句话：把 TH15 的章节系统搬到 TH18——擦弹判定区里有敌弹时该弹染黄、自机周围起擦弹圈、场上掉落物减速 ×0.3；boss 战按「一段攻击 = 一波」切波，波末按擦弹数 × 击破率结算章节奖，左侧一块 TH15 风格面板显示实时读数（同 `CARD-BEHAVIOR-AUDIT.md` §115 ✓）。

| 机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态 |
| --- | --- | --- | --- | :-: |
| **章节奖（bonus）**：波末按「擦弹 × 击破率」折奖 | `LOLK_BONUS_COEF 50`；`lolk_bonus = (int64)(graze*rate*50)`，`return (raw/10)*10`（向下取整到 10）；`graze<=0 \ | \ | 待核 | ✅ |
| **分数增量**：bonus 进总分 | `LOLK_SCORE_DIV 10`；`out->score = bonus / 10`；`ce_add_score_delta(out->score)` | `lolk_core.h:69`；`lolk_core.c:144`；`lolk.c:632-634` | ✅复刻 | ✅ |
| **PIV（最大得点）增量**：章节奖换 PIV 写共享池 | `LOLK_PIV_DIV 50000`；`piv_add = (bonus / 50000) * 10`（**两步取整**），`>0` 才写；HUD 第 5 行同式 `(shown/50000)*10` | `lolk.c:614`、`:657-661`、`:1049-1055` | ✅复刻 | ✅ |
| **碎片奖励（阈值 + 类型）** | `LOLK_FRAG_THRESHOLD 1000000`；`bonus >= 1e6 → frag_type = LOLK_FRAG_LIFE (0x10)`；**恒发命碎**，从不发 `LOLK_FRAG_BOMB (0x11)`；`LOLK_FRAG_CYCLE 5` 在实现里**零引用**（仅单测 `tests/test_lolk | `lolk_core.h:68,70,71,72`；`lolk_core.c:145-153` | ✅复刻 | ✅ |
| **擦弹减速（掉落物 ×0.3）**：判定区有弹就慢 | 写值 `LOLK_SLOW_START 30`（`slow_apply()` 写 `30/100 = 0.3f` 到 `mgr+0xe6bb14`）；曲线 `LOLK_SLOW_STEP 10`（+0.1/帧）、`LOLK_SLOW_FULL 100`；自愈 `slow<30 \ | \ | 待核 | ✅ |
| **擦弹判定区（圆 / 矩形两模式）** | 矩形：弹心 vs 判定框外扩 `LOLK_GRAZE_RECT_MARGIN 24.0f`；圆（`flags & 0x10`）：`rr = 3.6×判定半径 + max(弹半径/2.5, 40.0f)`，判据 `dx²+dy² < rr²+br²`；`LOLK_GRAZE_CIRCLE_DIV 2.5f`、`LOLK_GRAZE_CIRC | `lolk.c:63-81`、`:471-528` | ✅复刻 | ✅ |
| **擦弹染色（进圈弹染 TH15 黄）** | `LOLK_GRAZED_COLOR 0xffffd080u`（BGRA A=ff R=ff G=d0 B=80）；只改 RGB、每帧开头还原上一帧；表 `LOLK_TINT_MAX 256`（放卡外文件级 static），满表不再染；死弹按 `CE_BULLET_STATE 0xf68 == 0` 跳过 | `lolk.c:72-77`、`:428-469` | ✅复刻 | ✅ |
| **擦弹圈 VM** | 脚本 `CE_ANM_ABILITY_SCRIPT_LOLK_RING`（`anm_ids.h:1430` = 106）；`ce_anm_spawn(CE_ABILITY_ANM(), 106, 12)`；可见时每帧 `ce_anm_set_pos` 钉自机；不可见 → `ce_anm_interrupt(id, 1)` 后句柄清零 | `lolk.c:533-552`；`assets/ability/scripts/106_lolk_ring.anm.txt:20-31` | ✅复刻 | ✅ |
| **波的定义：一段攻击 = 一波** | 非符 / 符卡各一波；boss 出现开波、boss 消失结波（**只结 boss 波**）；boss 相位指纹变 → 新波；道中章优先于 boss | `lolk.c:9-11`；`lolk_core.c:215-255` | ✅复刻 | ✅ |
| **道中章边界 = MainSub 编号（去尾部字母）** | 前缀 `"MainSub"`、最多读 6 位数字（`LOLK_MAINSUB_DIGITS_MAX 6`）、>99999 夹到 99999；`MainSub01/01b/01c` 同章；`main`/`MainFront`/`MainLatter`（`stage_chapter<0` 但 `stage_sub>=0`）**不开波**；判据 | `lolk.c:96-99`、`:201-217`、`:224-252`；`lolk_core.c:72-104` | ✅复刻 | ✅ |
| **认不出关卡脚本时的退路** | `LOLK_MID_QUIET_GAP 240`（静默 ≥240 帧后再刷出算新章）；`LOLK_DEBOUNCE 5`（波边沿去抖）；退路第一波允许「无静默直接开」 | `lolk_core.h:82`、`:79`；`lolk_core.c:95`、`:249-255`、`:165`、`:278-280` | ❓自定 | ✅ |
| **击破率（道中 / boss / 时符）** | 道中 `rate = killed*100/spawned`，`spawned<=0 → 100`，上限 100；boss：击破→100 / 超时→0 / **时符恒 100**；两信号都没抓到当击破（不惩罚）；判据取引擎自己的：`life.current(+0x6220) <= 中断槽.hp_value`、`槽.time <= time | `lolk_core.c:6-28`、`:107-120`；`lolk_core.h:41-55`；`lolk.c:260-274`、`:292-322` | ✅复刻 | ✅ |
| **收道具的 PIV 涨源（消弹小道具 +2）** | TH18 item `type ∈ [9, 0x0e]`（`LOLK_ITEM_TYPE18_POINT_CONV_LO/HI`）→ `ce_piv_add(id, LOLK_PIV_CANCEL_ITEM = 2)`，一颗一笔；挂既有共享站点 `ce_item_collect`（`th18:0x44625f`） | `lolk.c:379-393`、`:1143-1149` | ✅复刻 | ✅ |
| **已撤项：满 P 收 type 8 ⇒ PIV +100** | 代码里**不存在**（`on_item_collect` 只有上面那支；函数体只剩两行） | `lolk.c:341-347`、`:382-393` | 待核 | ✅ |
| **HUD 五行数值 + 面板六条** | 面板 6 条 VM（`LOLK_PANEL_N 6`），摆位 `LOLK_PANEL_AX 639.5f` / `LOLK_PANEL_AY 401.5f` + `DX{0,−64,−64,−64,−64,0}` / `DY{0,50,90,130,170,240}`；可见块按**原生内容 192×32**（`LOLK_PANEL_ART | `lolk.c:563-618`、`:685-691`、`:818-823`、`:1016-1057`、`:833-851` | ✅复刻 | ✅ |
| **碎片条二选一（第 6 条）** | `panel_script(5, last)`：`frag_type == LOLK_FRAG_LIFE` → `PANEL7 = 116`（粉/命碎），否则 `PANEL6 = 112`（绿/符卡碎）；`frag_type == 0` 时该条**根本不起**（`panel_show()` 的 `continue`） | `lolk.c:697-705`、`:710-711`；`anm_ids.h:1436,1440` | ✅复刻 | ✅ |
| **面板数字跳动（滚动 + SE）** | `LOLK_ROLL_START 90` 帧后开始；每帧 `shown += bonus/rate`、`left−−`；`(left & 3)==0` 播 `LOLK_SE_TICK 0x27`；跳完 `shown = bonus` + `LOLK_SE_DONE 0x2f` | `lolk.c:614-617`、`:993-1005`、`:671-675` | ✅复刻 | ✅ |
| **面板保留时长 + 退场淡出** | `LOLK_HUD_HOLD 300`（`last_age > 300` 触发退场）；六条由脚本自己淡（`interruptLabel(1); alphaTime(10,0,0)`）；五行由 C 打 `LOLK_PANEL_FADE_FRAMES 20` 帧线性淡出 | `lolk.c:618`、`:1068-1087`、`:812-816`、`:888-894` | ✅复刻 | ✅ |
| **面板防遮挡压暗（靠近自机就变淡）** | **全部走共享件**（`lolk.c:870` `hud_dim_tick()`），本卡不自持；活值在 `hud_dim.h`：`HUD_DIM_ALPHA_FULL 255`、`HUD_DIM_ALPHA_DIM 64`（=25%）、`HUD_DIM_FRAMES 8` 帧线性、进 = 矩形外扩 `HUD_DIM_MARGIN 28.0 | `lolk.c:739-810`、`:833-883`；`cards/hud_dim.h:113,114,121,127,129` | ✅复刻 | ✅ |
| **波结算的落笔（一次结算做的事）** | `score>0 → ce_add_score_delta`；`piv_add>0 → ce_piv_add`；`frag_type != 0 → ce_spawn_item(type, x, y−76, speed 1.2f)`；记 `last` / `last_age=0` / `waves_total++` / `fade_t=-1 | `lolk.c:620-681`、`:662-666`；`lolk_core.c:122-157`、`:133`；`lolk_core.h:78` | ✅复刻 | ✅ |
| **擦弹计数来源：原生计数差分（不补道具擦弹）** | 读 `LOLK_GRAZE_ADDR 0x4ccd0c`，帧间正增量累进本波；死字段 `0x4ccd10`（GRAZE_IN_CHAPTER）不用；道具 type 13/14/15 的 add_graze(5/10/25) **不补** | `lolk.c:35-38`、`:937-943`、`:1011-1014` | ✅复刻 | ✅ |
| **刷出 / 消失计数（分母 / 分子）** | `zEnemyManager+0x198 enemy_count_real` 帧间差分：正增量 → `lolk_add_spawned`、负增量 → `lolk_add_killed`；仅本波开着时累计 | `lolk.c:83-88`、`:945-954`；`lolk_core.c:301-319` | ✅复刻 | ✅ |
| **道中刷出信号（本帧新建敌人数）** | `zEnemyManager+0x90 next_enemy_id`（`CE_EM_NEXT_ENEMY_ID`）差分，只增不减；`d >= last` 才取差，否则 0 | `lolk.c:117-118`、`:254-258`、`:956-963` | ✅复刻 | ✅ |
| **共享 PIV 池族籍 + 可写上限** | `ce_piv_claim_auto(ce_card_id(c), ce_difficulty())`（每帧幂等）+ 上限查表 `k_piv_cap[3][d] − k_piv_init[3][d]`：EASY **250000−10000 = 240000**、N/H/L/EX **500000−10000 = 490000**（显示单 | `lolk.c:921-925`；`sdk_core.c:869-892`（`115 th15` 行 = `:873` / `:881`） | ✅复刻 | ✅ |
| **擦弹族声明** | `ce_graze_family_claim(ce_card_id(c))`（每帧幂等一行） | `lolk.c:925`；`sdk_core.h:439` | ✅复刻 | ✅ |
| **关卡脚本识别（引擎建的 "main" 敌人）** | `parent_enemy_id(+0x6834) == 0` 且当前子程序名以 `Main` 开头或正好 `main`；子程序号链 `+0xc → ctx+0x4`；名字链 `+0x1218 → fm+0x8c`（每项 8 字节 {name, bytecode}）、`fm+8` 表长；`s_stage_id` 缓存「按 id 复核，指针 | `lolk.c:119-193`、`:224-252` | ✅复刻 | ✅ |
| **相位指纹（判断「换了一段攻击」）** | `phase_sig = (i+1)*1000003 ^ hp*31 ^ tm*131 ^ *(slot+8)`；仅在「有活动中断槽」时更新，槽清空那帧（0）不算边界 | `lolk.c:308-321`；`lolk_core.c:244-248`、`:283-285` | ❓自定 | ✅ |
| **boss 消失只在「当前是 boss 波」时结波** | `if (s->boss_present && !boss)` 且 `wave_kind == LOLK_KIND_BOSS` 才 `close_wave` | `lolk_core.c:215-225` | ❓自定 | ✅ |
| **开局/换关重置** | `on_stage_start`：`lolk_reset` + 三 sentinel + `panel_dim_reset` + 收圈/面板 + `s_stage_id=0`；`on_run_reset`：收圈/面板 + `memset` + `panel_dim_reset` + `lolk_reset` + sentinel；`lol | `lolk.c:1095-1109`、`:1123-1141`；`lolk_core.c:62-70` | ❓自定 | ✅ |
| **删卡收尾** | `on_delete`：收 `ring_id` + `panel_hide()`（六条发 interrupt、五行 `ce_hud_vm_text_clear`） | `lolk.c:1111-1121` | 待核 | ✅ |

### 116 天空璋（`cards/season`）

- 一句话：TH16 季节系统复刻——擦弹/击破/解放清弹产出「季节点」，累计成 0..6 档，档 = 季节子机数，按 C 发动季节解放（清弹 + 伤害 + 无敌），双击 Z 切副季节，左下角画季节槽（同台账 §116）。

| 机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态 |
| --- | --- | --- | --- | :-: |
| **季节槽档位**（累计季节点 0..6 档） | `k_req = {100,230,390,590,840,1140}`；`points < k_req[i]` 返回 i，≥1140 = 满档；`SEASON_LEVEL_MAX 6`；`SEASON_POINTS_MAX 1140` | `season_core.c:5`（表）`:51-59`（判据）；`season_core.h:25-26` | ✅复刻 | ✅ |
| **释放档位表**（持续帧 / 清弹半径 / 每帧伤害） | `k_dur[5][7]`：春 0,10,13,16,20,20,20；夏恒 30；秋 0,40,55,70,90,110,130；冬 0,30,70,110,150,170,190；土用恒 40。`k_rad[5][7]`：春 60→300、夏 40→50、秋恒 60、冬 30→60、土用 50→60。`k_dmg = {100,100 | `season_core.c:11-28` | ✅复刻 | ✅ |
| **释放无敌帧** | `k_invuln = {-1, 9, 30, 30, 10}`（-1 = `duration + 60`，`#define SEASON_SPRING_TAIL_INVULN 60`）；`invuln = (k_invuln[sub] < 0) ? duration + 60 : k_invuln[sub]`；消费点写 `CE_PLAY | `season_core.c:31-32`、`:78`；`season.c:1332-1337` | 待核 | ✅ |
| **释放的槽消耗**（春/秋/冬 清空，夏/土用 只扣一档） | `consume_one_level = (sub == SUMMER |  | 待核 | ✅ |
| **子机数 = 档位** | `opts_for(level) = min(level, SEASON_OPTS)`；`SEASON_OPTS 6`（`k_opt_start = {0,1,3,6,10,15}` ⇒ 档 6 = 6 颗）；`SEASON_OPT_DX 24`（兜底阵距）；`SEASON_RETRY 60`（拿不到槽的重试间隔） | `season.c:64-68`、`:385`、`:544-568`、`:1542-1559` | ✅复刻 | ✅ |
| **子机阵位照 TH16 副 sht 的 `option_pos`** | 21 项表 `k_opt_pos[5][21]`（春/夏/土用小扇、秋最宽 ±48、**冬 x 恒 0** y 32..52）；`idx = k_opt_start[level-1] + i`；写选项记录 4 个定点字段（×128） | `season_core.c:86-92`、`:95-109`；`season.c:397-408`、`:466-478` | ✅复刻 | ✅ |
| **五副季节各一条子机脚本**（换季 = 整批重申请） | 脚本号 = 生成物宏 `CE_ANM_ABILITY_SCRIPT_SEASON_OPT_*`，`#else` 回退 = **364/365/366/367/368**；换季走 `refresh_options()`（全 `ce_option_drop` → `alloc_options`）；贴图按副季节 | `season.c:426-450`、`:508-517`、`:486-492`、`:1528-1533` | ✅复刻 | ✅ |
| **子机弹型 = 5 组 shooterset**（春寻的 / 夏直飞 / 秋匀加速 / 冬激光 / 土用向后） | `season_shooterset()` 映射到 `CE_SHT_SET_SEASON_{SPRING,SUMMER,FALL,WINTER,DOYOU}`；每颗子机各发自己那发，节奏用 `s->frame` 当 `short_timer` | `season.c:1594-1628` | ✅复刻 | ✅ |
| **释放 = 半径清弹（mode 4）+ 每帧伤害源** | `ce_cancel_radius(pos, R, SEASON_CANCEL_MAX=9999, 4)`；`ce_damage_rect(pos, 0.0f, life=2, rel_dmg, 2R, 2R)`；抬上限 `SEASON_DMG_CAP 300`；**顺序：先撒季节点、再消弹**（消弹会删掉弹，之后取不到坐标） | `season.c:106-107`、`:1373-1392` | ✅复刻 | ✅ |
| **消弹 → 每颗弹 1 颗季节点（竖直上升）** | 命中判据 `state∈{1,2}` 且 `bullet+0x24 == 0` 且 `dist² ≤ (bullet+0x658 × 0.5 + R)²`；出点角 `-π/2 + rand[0,10°]`（`0.17453292`）、速 `2.2f`；比例硬编码在循环里 = **每颗弹 1 颗点** | `season.c:95-102`、`:707-731`、`:1373-1376` | ✅复刻 | ✅ |
| **擦弹 → 每颗擦到的弹撒一颗点** | `SEASON_GRAZE_PER_FRAME 2`（一帧上限）；出生在弹坐标、角 = `atan2(弹−自机)`、速 **1.9f**；读原生擦弹计数 `SEASON_GRAZE_ADDR 0x4ccd0c`；取弹判据仿引擎（方弹 `+0x479a4/a8` +`SEASON_GRAZE_RECT_MARGIN 24.0f`、圆弹 `m | `season.c:93-94`、`:678-688`、`:1416-1421`、`:1467-1482` | ✅复刻 | ✅ |
| **敌人掉落**（boss 每相位 200 / 小怪按出场计时衰减） | `SEASON_DROP_BOSS 200`；小怪 `n = (N−M)·rem/T + M`，`SEASON_DROP_MID 8 / MID_TIME 60 / MID_MIN 1`；T = 敌人自己的 `timed_ex_drop_time`，N = `season_drop_native_tier(native)`（1→8 / 2 | `season_core.c:167-186`；`season.c:141-146`、`:1030-1051`、`:1070-1119` | ✅复刻 | ✅ |
| **季节点运动学**（待发射错帧 → 飞散 → 飘落 → 磁吸/过线） | 错帧 `delay = (spawn_seq & 3) + 1`（1..4 帧）；飞散 `lr -= 0.03`（初速击破 0.2..2.1、消弹 2.2、擦弹 1.9）、角固定；飘落 `vy += 0.03`、上限 `2.0`、`vy>=0 → vx=0`；出界 `y > 472` / ` | x | 待核 | ✅ |
| **季节点池** | `SEASON_POINT_POOL 512`（DLL 静态区 `s_pt[512]`，非 `ce_state` —— `ce_state` 上限 496 字节）；层 `SEASON_POINT_LAYER 13` | `season.c:117-118`、`:314-315` | ✅复刻 | ✅ |
| **冬 ×1.5 弹伤 / 秋 ×1.5 移速** | `SEASON_DMG_WINTER 1500`（permille，`ce_set_bullet_damage_permille`）；`SEASON_SPEED_FALL 1.5f`（**乘**进 `CE_PLAYER_SPEED_MULT`）；二者都只在释放期生效 | `season.c:108-109`、`:1338`、`:1402`、`:1394-1398` | ✅复刻 | ✅ |
| **收集结算**（+1 点 / 加分 / 升档弹字 / 音效） | 一颗点 = `SEASON_POINTS_PER_ITEM 1`；`SEASON_POINT_SCORE 10`；跨档才 `alloc_options` + 弹字（原生 `PopupManager__sub_4645f0` `0x4645f0`，参 `(pos, -1, 0xffffff40)`）+ 升档音 `SEASON_SE_LEVE | `season.c:69-70`、`:132`、`:140`、`:765-771`、`:774-801` | ✅复刻 | ✅ |
| **副季节切换 = 完整双击 Z** | ❗活常量 `SEASON_DTAP_WINDOW = CE_DTAP_WINDOW_DEFAULT` = **8 帧**（从**第一次按下**起算）；状态机在共享件 `dtap_core.c`；判据 = `INPUT_HELD & 0x001`，只在第二次按下那帧返回 1；**一个输入掩码位都不写** | `season_core.h:101` + `dtap_core.h:29`（=8）；`season.c:88`、`:1528-1533`；`season_co | ✅复刻 | ✅ |
| **HUD = 游玩区左下角季节计量槽 + 压暗** | 钉位 `SEASON_HUD_PIN_X 68 / PIN_Y 924`（`CE_HUD_SIDE_PINNED`，不进货架）；声明块 `SEASON_HUD_BLOCK_W 134 / H 36`；外框 4 条 1px 矩形（上/下 103×1 @ y910/920，左/右 1×9）；条 `100×8`；块内偏移 BAR `(+58,− | `season.c:162-251`（几何/`k_season_frame[]`）、`:177-180`、`:212-213`、`:1831-1840`；`hu | ✅复刻 | ✅ |
| **HUD 状态机**（进度色 / 档位亮度 / releaseable / 淡出闩锁 / 档位数字） | interrupt 组合 `st = (lvl_up?2:3) \ | ((lvl_up?level+7:3)<<4) \ | 待核 | ✅ |
| **92「伟大之作」清扫：季节点换金钱** | `SEASON_SWEEP_MONEY_PER_POINT 1`（每颗 1 枚，池满走 `ce_money_pocket(1)`）；倒序扫 + `ce_anm_delete` + `point_remove` | `season.c:585-611`、`:1508` | ❓自定 | ✅ |
| **88「杰拉」复制代数闸** | 每帧 `ce_dup_publish(id, n_pt)`；代数变化则照原档各重撒一颗（同角/同初速），先存 `n0` 防递归复制 | `season.c:1485-1505` | ✅复刻 | ✅ |
| **解放圈特效（两圈同心）** | 内圈半径 = 外圈 `+16.0f`；`k_ring`：边数 春 32 / 其余 12，自转 `SPIN_TH16 0.17453292f`（10°/帧，春 0），土用 `drift_y 48`（60 帧上浮）；时间轴 前 10 帧长大、alpha 255→64（250 帧）、末 20 帧 →0；配色按副季节查表（★ 曾把 RGB 写反， | `season_core.c:122-142`；`season.c:1150-1153`、`:1184-1227` | ✅复刻 | ✅ |
| **秋/冬 加速残像** | 只对 `rel_sub ∈ {FALL, WINTER}`、且自机非无敌（`inv->cur <= 0`）时每帧建 1 个 VM；脚本 `{17,19,14,14}`（按**角色**，不是副季节）；脚本本体 `layer(13); type(1); sprite(sprite0); color(60,0,192); alphaTime(2 | `season.c:1244-1278`、`:1367` | ✅复刻 | ✅ |
| **主动卡发动门** | `.active_recharge = 1`；`on_activate` 门 = `level_of(s) <= 0` → refuse、`rel_frames > 0` → refuse；持续态由 `on_active_tick` 返回 `rel_frames > 0` | `season.c:1289-1304`、`:2076-2080` | ✅复刻 | ✅ |

### 117 鬼形兽（`cards/animal`）

- 一句话：TH17 动物灵/咆哮的 TH18 卡侧复刻——敌人掉兽灵，收满 5 格自动起咆哮（就绪窗口），按 X 用成 40 帧霊撃，中弹时可用咆哮抵命（同台账 §117）。

| 机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态 |
| --- | --- | --- | --- | :-: |
| 5 格灵队列（收灵入队） | `ANIMAL_SLOT_MAX 5`；`ANIMAL_PENDING_MAX 5`；`fill<5` 时 `slots[fill++]=type`，满 5 立刻 `animal_ready_compute()` | `animal_core.h:24-25`；`animal_core.c:68-82` | ✅复刻 | ✅ |
| 满槽就绪判定（多数 ≥3 → 暴走；否则混合） | 判据：`fill>=5` 且数 type 1/2/3 出现次数；`best_n>=3` → `hyper_type=best_type`(1..3)，否则 `hyper_type=4`(混合)；同时 `ready=1 / time_left=duration / extends_used=0` | `animal_core.c:23-50` | ✅复刻 | ✅ |
| 咆哮时长表 | 混合(4) **1140**；多数 count 3/4/5 → **840 / 1140 / 1500**（switch + `clamp(count,3,5)`，不是读 .h 注释） | `animal_core.c:11-20`；常量口径见 `animal_core.h:70-72` | ✅复刻 | ✅ |
| 凭依兽灵 → 时长 **+480** | `ANIMAL_GOAST_BONUS 480`；判据 `st->goast == ANIMAL_GOAST_ANY \ | \ | 待核 | ✅ |
| 收动物灵 → 就绪窗口延长 | `tbl[11] = {90, 60, 42, 28, 18, 12, 8, 5, 3, 2, 1}`；`ANIMAL_EXTEND_COUNT 11`；越用越小，用完停在 1；`time_left` 夹到 `duration` 上限 | `animal_core.c:95-106`；`animal_core.h:47` | ❓自定 | ✅ |
| 收鱼灵 → 立刻换道具（不进队列、不延长） | 队列已满 + `animal_type_is_fish(type)`（type 4..7）→ 返回 **3**，调用方立刻 `fish_to_items()` | `animal_core.c:84-85`；`animal.c:3374-3382` | ✅复刻 | ✅ |
| 就绪窗口每帧 −1；有存活自机弹再 −1 | `int dec = paused ? 0 : (1 + player_has_live_bullets());`；`player_has_live_bullets()` 扫 `CE_PB_COUNT` 池的 `CE_PB_STATE==1` | `animal.c:4076-4079`；`animal.c:719-727` | ✅复刻 | ✅ |
| 霊撃发动 = **X（炸弹键）**，本卡是被动卡不占 C 键 | 判据：`INPUT_PRESSED & 0x002` && `core.ready` && `!paused` → `hyper_begin(s,0)`；`CE_CARD(117)` **没有** `.active_recharge` / `.on_activate`（已删） | `animal.c:3983`；注册块 `animal.c:4235-4255`；被动化说明 `animal.c:3887-3903` | ✅复刻 | ✅ |
| 霊撃本体参数（40 帧 / 半径 / 无敌 / 每帧伤害源） | `ANIMAL_HYPER_FRAMES 40`；半径 `(1 − f⁴) × 200`（`ANIMAL_RADIUS_MAX 200.0f`）；`set_invuln(60)`；每帧 `ce_damage_rect(pp, 0.0f, 2, 40, r*2, r*2)` | `animal_core.h:36,46`；`animal_core.c:169-179`；`animal.c:3164`；`animal.c:3201,321 | ✅复刻 | ✅ |
| 水獭暴走 = 3 颗旋转光球（消弹 + 小伤害源） | `ANIMAL_ORB_ORBIT_RADIUS 96.0f`；`CANCEL_PHASE -0.34906584`（20°）；**`CANCEL_RADIUS 28.0f`**；`CANCEL_MODE 1`；`DAMAGE 2` / `DAMAGE_LIFE 5`；初始角 `0 / −120° / +120°`；脚本 layer 13 | `animal.c:2988-2995`；`orb_cancel` `animal.c:3089-3110`；`orb_hit_pos` `animal.c:3 | ✅复刻 | ✅ |
| 鹫暴走（魔理沙 pl01）= 卡侧 5 条扇形粗激光 | `SCALE 4.0f`；`BASE_LEN 512 / BASE_W 32`；`LEN = 512*4 = 2048`；`HIT_W 34.0f`（不缩放）；**`DAMAGE 20`**；`CAP 300`；`GROW_FRAMES 6`；`SHOT_FRAMES 15`；`N 5`；角度 −90/−75/−105/−60/−120° | `animal.c:327-348`；角度表与实现 `animal.c:2607,2675-2768` | ✅复刻 | ✅ |
| 鹫暴走（咲夜 pl02）= 6 点扫荡 + 正中正面口，真引擎弹 | `N 6`；`R 16.0f`；`THETA0 0.6f`；`OMEGA 0.03f`；`PERIOD 6`；`FRONT_PERIOD 2`；`DAMAGE 20`；`FRONT_DAMAGE 40`；`DPS_TARGET 40.0f`；发弹 = `Player__shoot_one_bullet` `0x45e930` | `animal.c:383-401`；实现 `animal.c:2851-2996` | ❓自定 | ✅ |
| 咆哮自然结束 → 释放动画 + 2 只变色动物灵 | `ANIMAL_RELEASE_FRAMES 120`；`ANIMAL_BURST_DY 64.0f`；`long_life=1`；暴走(1..3) → 2 只都是**本次那一只**；混合(4) → `1 + rand%3`；`by_hyper` 路径**不放** | `animal.c:91,86`；`release_spawn/tick` `animal.c:3218-3263`；`hyper_end` `animal.c | ✅复刻 | ✅ |
| Miss（真死）→ 2 只动物灵，**左右各一、各自独立随机** | `type_l = 1 + ce_rand()%3`、`type_r = 1 + ce_rand()%3`（两次独立）；`long_life = 1`（**会变色**）；基座 `(death_x, death_y − 64)`；方向强制 `135° / 45°`；用中弹点而非 respawn 点 | `animal.c:3850-3885` | ✅复刻 | ✅ |
| 中弹不扣命（deathbomb 取消 = 用掉咆哮） | 判据：`on_death_before_deathbomb` 里 `core.ready && !core.hypering` → `(void)ce_rescue_note_claim(117); hyper_begin(s, 1)`；`by_hit=1` 时调 `cancel_impending_death()`（thiscall 调 | `animal.c:3828-3848`；`cancel_impending_death` `animal.c:699-703`；`hyper_begin` ` | ✅复刻 | ✅ |
| 鱼灵换道具的产物与展开 | 4→`0x11`(BOMB2 卡道具)×1；5→`0x10`(EXTEND2)×1；7→type **2 金钱**×**30**；其余(6)→type **1 火力**×30；`ANIMAL_P_ITEM_N 30`；椭圆 `ANIMAL_ITEM_R_MAX 58.0f`；出生点 x clamp `±184.0f`；intro 倒计时  | 常量 `animal.c:428-441,1784`；实现 `animal.c:1884-1950`；安全区 `animal.c:443-452` | ✅复刻 | ✅ |
| 道中「大怪」击破 → 1 只动物灵（AL55 帧内队列） | `ANIMAL_BIG_DROP_QUEUE 32`；判据 = `(w>=64 && h>=64)` **或** `(w==48 && h==48 && sprite ∈ 72..87 / 292..307)`；`ANIMAL_ODD_GROUP_MAX 3`；`ANIMAL_ODD_MIN_SAMPLE 1`；严格过半 `max/tot | 常量 `animal.c:486-508,868-882`；判据 `animal_enemy_drop_measure` `animal.c:1398-1510 | ✅复刻 | ✅ |
| 全屏消弹「本帧第一家动手」闩锁 | 霊撃半径圈：`animal_cancel_claim()` 认领；`ce_cancel_radius` 返回 ≤0 则 `ce_cancel_latch_release(117)`；水獭 **r=28 局部消弹不认领**；鹫束消弹已删除 | `animal.c:607-614`；`hyper_frame` `animal.c:3208-3212`；`orb_cancel` `animal.c:309 | ✅复刻 | ✅ |
| HUD：左下 5 格灵槽 + 自机靠近压暗 | `ANIMAL_HUD_X0 88.0f` / `Y 904.0f` / `DX 36.0f`；声明给槽位的块 `192×48`；压暗走共享件 `hud_dim`：终点 **64** / **8 帧**线性 / 进 = 外扩 **28**、出 = **44**（滞回 16） | 常量 `animal.c:133-149`；`hud_sync/hud_fade_tick` `animal.c:2103-2200`；共享件 `cards/h | ✅复刻 | ✅ |
| 变色 / 寿命循环（long_life 动物灵） | `LIFE_FAR 240` / `LIFE_NEAR 90` / `LIFE_AFTER 180` / `FLICKER_LEAD 60`；变形 `type % 3 + 1`（1→2→3→1）；靠近冻结；变色时 `interrupt 2/3`、发 SE `0x4e` | 常量 `animal.c:106-109`；`tokens_tick` `animal.c:3408-3441`；`animal_core.c:214-217` | ✅复刻 | ✅ |
| 动物灵运动 / 收集 | `COLLECT_R2 576`（24 px）+ `GRACE 10` 帧；`SLOW_R2 3600`（60 px）→ ×0.5；`ABSORB 0.25` / `ABSORB_FRAMES 9`；离屏 ` | x | 待核 | ✅ |
| 动物灵池上限 | `ANIMAL_TOKEN_MAX 40`（池满 `token_spawn` 返回 0，掉落被 block 并打日志） | `animal.c:79`；`token_spawn_heading` `animal.c:1619` | ✅复刻 | ✅ |
| 跨卡：给 108 的「暴走咆哮中」只读标志 | 判据：`roaring(s) && (hyper_type == 1 \ | \ | 待核 | ✅ |
| 跨卡：92「清扫」/ Jera「复制」 | `ce_dup_publish(id, s_n_tok)` + `ce_dup_should_run(&s_dup_seen)` → `animal_dup_run()`；`ce_sweep_should_run(&s_sweep_seen)` → 每只灵换 **10** 枚金钱（`ANIMAL_SWEEP_MONEY_PER_TOKEN | `animal.c:3953-3955`；`animal_dup_run` `animal.c:3932-3944`；`animal_sweep_to_mone | ❓自定 | ✅ |
| 音效（SE id / 音量旋钮 / 播音策略） | 一手：生成 `0x3b`、收集 `0x3f`、就绪 `0x4d`、霊撃 `0x2c`、变色 `0x4e`；正常退出 **`0x42`**；撒道具 **`0x3e`**（用户耳听定案）；撒道具序列封顶 `ANIMAL_SE_SEQ_MAX 30`、2 帧一声、对话中丢弃；`se_volume_fix()` 写引擎 SE 表 `0x4c9b8 | `animal.c:71-76`；`1769,1777,1780,1811-1826`；播放 `animal.c:4037-4048` | ✅复刻 | ✅ |
| 大怪之外的原版掉落对齐（BOSS 相位 / `dropItems`） | 符卡相位结束 → 1 只随机动物灵；普通/血条相位结束 → 2 只资源灵（4+5）；`on_enemy_drop_pre` 已结账则只推进指纹不重复发；`boss_dead_sub` 含 `"Dead"` 判真死 | `animal.c:3671-3826`（`animal_phase_track` 3584-3670；`animal_drop_resource_pair`  | ✅复刻 | ✅ |

### 112 星莲船（`cards/ufo`）

- 一句话：宿主小怪/Boss 击破掉令牌 → 自机碰到收入左下角三格槽 → 三枚成组即召唤一只 ECL 真敌机 UFO → 它吸走全场 P / 金钱道具与 110/111 信仰点 → 击破后按吸收量开奖（压缩包 + 返还令牌 + 加成道具）；超时飞走无奖。（同台账；行为表 / 成本 / 完整跨卡与未测见 `CARD-BEHAVIOR-AUDIT.md` §112）

| 机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态 |
| --- | --- | --- | --- | :-: |
| **令牌从哪来**：宿主敌人击破时掉，帧末兑现 | `on_enemy_death` 里**只登记** `pend_drop*`；建 VM 在 `on_tick_2` ②b。门 = `CE_EDEATH_F_REAL` + `ufo_body_kill_counts(kind)`（只认 KILLED / BOMB_KILL，`KILL_ALL` 不算）+ `!s->pend_drop` + | `ufo.c:5769-5841`（登记）、`ufo.c:6268-6291`（兑现）、`ufo_core.c:744-747`（死因过滤） | 待核 | ✅ |
| **谁能掉令牌 / 换皮宿主表** | 判据 = `anmSelect`(N=`enemy+0x1494`) + 主脚本(S=`enemy+0x1498`) 命中 `UFO_CARRIER_KEYS[]`（**11 条**）；key 里带 `hp_min/hp_max`（`≤400 = 一碰就碎` 分档），kind = FAIRY / BFLY；110/111 已认领的妖精** | `ufo_carrier_table.h:38-54`（表）、`ufo.c:2621-2666`（判据）、`ufo.c:2832-2839`（护栏） | 待核 | ✅ |
| **每波挑几只当宿主** | `UFO_CARRIER_PER_WAVE 2`；组里**第一只**=宿主 #1，之后**同类同皮**且 \ | x1+x2\ | 待核 | ✅ |
| **掉落枚数：非 boss 1 枚 / boss 2 枚** | `UFO_CARRIER_TOKENS_DROP 1`、`UFO_CARRIER_TOKENS_BOSS 2`；`want = is_boss ? BOSS : DROP`（宿主/boss 这条路**不吃** `next_token_at` 节流） | `ufo.c:2572-2573`、`ufo.c:5818` | 待核 | ✅ |
| **掉出来的令牌颜色** | 代码里 **恒为随机变色令牌**：`col = UFO_COLOR_NONE`（`ufo.c:5807`，之后**没有任何分支给它赋值**）⇒ `on_tick_2` 走 `ufo_tok_spawn()` ⇒ 自建 LCG 随机取红/蓝/绿（`>>16` 段取模）→ `cycler = 1`；每 `UFO_GOLD_CYCLE 2` 次 | `ufo.c:5807`、`ufo.c:6280-6283`（`const int32_t c = col;` 注释「恒为 `UFO_COLOR_NONE`」） | 待核 | ✅ |
| **同屏令牌上限 / 池容量** | `UFO_TOKEN_MAX 6`（同时活几个）；`UFO_TOKEN_POOL_SLACK 2` ⇒ `UFO_TOKEN_CAP 8`；编译期 `_Static_assert(UFO_TOKEN_CAP == UFO_TOKEN_MAX + UFO_TOKEN_POOL_SLACK)`、`>= UFO_TOKEN_MAX` | `ufo_core.h:759`、`ufo_core.h:843-844`、`ufo.c:6470-6473` | ❓自定 | ✅ |
| **令牌运动**：直线飞 + 四边反弹 | 初速 `UFO_TOK_FLY_SPEED 1.5`，角度按本局序号奇偶交替：偶 π/4(0.785398185)、奇 3π/4(2.3561945)（⇒ 序号 0 = 左下）；反弹边 ±176 / y 160..370；`UFO_TOK_NO_BOUNCE_FRAMES 1200` 帧后不反弹；外框 ±208 / y −16..464  | `ufo_core.h:809-823`、`ufo_core.c:533-567` | 待核 | ✅ |
| **令牌寿命兜底（TTL）** | `UFO_TOKEN_TTL_FRAMES 2400`（到点自己静默撤） | `ufo_core.h:764-771` | ❓自定 | ✅ |
| **令牌收取 = 自机接触**（不是子弹打） | 半径 = **引擎的**自机道具收集半径 `p + CE_PLAYER_ITEM_COLLECT_R`（读不到/为 0 ⇒ 不收）；判据 `d² ≤ r²` | `ufo.c:3697-3712` | 待核 | ✅ |
| **变色令牌节奏**（一手 type 13/14/15） | `UFO_TOK_NEAR_R2 3600.0`（60 px 圈，进/出各发一次中断 7 / 3 或 2）；`UFO_TOK_BLINK_AT 90` 起发中断 2 快闪；`UFO_TOK_CYCLE_TICKS 150` 到点换色（`rb_state = (rb_state+1)%3`）+ 重起 VM + 播音；圈内计时器**冻结**（ | `ufo.c:516-522`、`ufo.c:1762-1806` | 待核 | ✅ |
| **凑组规则 → 召唤** | 3 格槽：`UFO_SLOT_COUNT 3`；**3 同色**或**3 互不相同** ⇒ 召唤（互不相同 = 彩虹 UFO）；**两同一异 ⇒ 丢最旧、保留最近两个**（一手 KEEP2 三条指令）；槽满且已成一组的**原样报 READY、不收不丢** | `ufo_core.c:89-194`、`ufo_core.c:157-165` | 待核 | ✅ |
| **召唤不清槽（P1）** | `ufo_group_take_ready()` 只快照 `last_group` / `last_slot2`，**不动 3 格**；清槽只有一处 = `ufo_teardown()` 里的 `ufo_group_clear(&s->grp)`（击破/飞走/撤场/删卡四条路） | `ufo_core.c:226-239`、`ufo.c:5081-5084` | 待核 | ✅ |
| **UFO 是单例真敌人**（不是自建 VM） | `ufo_can_spawn()` = `ufo_id == 0` 才允许再召唤；场上已有 UFO ⇒ 组留在槽里等下一位；本体 = ECL 真敌机 `UFO_{Red,Blue,Green,Rainbow}_TTL`（**金借蓝**）；本体贴图由卡侧跟随 VM（脚本 164..167）画，ECL 侧 `anmSetSprite(0,-1 | `ufo_core.c:262-282`、`ufo.c:366-370`、`ufo.c:3784-3793`、`ufo.c:419` | 待核 | ✅ |
| **UFO 真血量 = 一手表 × 火力档** | 表 `k_ufo_hp_by_power = {1600,1900,2100,2300,2500}`，档 = `power / per_level` 夹 0..4；`UFO_HP_ONEHAND_SCALE_PERMILLE 1000`（唯一旋钮，默认不缩放）；**写进 `spawn+0x14` 的是钳制值 ≤ `UFO_LIFE_MAX | `ufo_core.c:719`、`ufo_core.c:600-605`、`ufo_core.h:689-709`、`ufo.c:3798-3800`、`uf | 待核 | ✅ |
| **UFO 在场寿命上限** | `UFO_LIFE_MAX 720`（`frames` 到顶只报"达到上限"，真正离场信号 = id 不在了） | `ufo_core.h:266`、`ufo_core.c:284-289` | 待核 | ✅ |
| **吸收门 / 只吸什么** | 能吸 = UFO 存活帧数 **> `UFO_ABSORB_PAUSE 59`**（首吸帧 = `UFO_ABSORB_FIRST_FRAME 60`）；只吸 `UFO_ITEM_TYPE_P 1` / `_MONEY 2`；跳过 state 3/4/0；state 不在 0..6 ⇒ **跳过不赌** | `ufo_core.h:271-272`、`ufo_core.c:304-327`、`ufo.c:3577-3587` | 待核 | ✅ |
| **吸收捕获半径** | 到 UFO 中心 `d² < 324`（= `UFO_CAPTURE_DIST 18.0` px）；捕获时 **state=0 + 删 anm VM + anm id 写 0 + freelist LRU 摘挂 + 清 vel + 丢拉拽累计** | `ufo_core.h:319-320`、`ufo.c:3474-3488` | 待核 | ✅ |
| **道具拉拽速度 = 匀速 3.0**（一手累计公式**已关**） | `UFO_DRAG_ACCEL_ENABLE 0` ⇒ `ufo_pull_speed()` 直接 `return ufo_drag_speed_const()`；`UFO_DRAG_SPEED_CONST_MILLI 3000`（= 3.0 单位/帧 = 6 表面 px/帧）。一手闭式 `0.02 + (frames/600)×0.05 | `ufo_core.h:368-376`、`ufo.c:1201-1214`、`ufo.c:3525-3528` | ❓自定 | ✅ |
| **吸收只朝 UFO 拉**（撤掉了"别让自机抢收"栅栏） | `ufo_step_allowed()` 现在**只判目标点在屏内**（\ | x\ | 待核 | ✅ |
| **填充率 / 吸满阈值** | 单颗增量 = `(32 − 2×stage) × 1000` 百万分比（= 一手 `0.032 − stage×0.002`），再乘 `UFO_FILL_SCALE_PERMILLE 1000`（🟡占位 = 不缩放）；严格 `> UFO_FILL_UNIT(1.0)` 才吐，满了夹回 1.0；**fill 已 ≥1.0 ⇒ 整段收工**（ | `ufo_core.h:443-448`、`ufo_core.c:438-465` | 待核 | ✅ |
| **吸满吐什么**（按色） | 红 → `UFO_REWARD_LIFE_FRAG 0x10`（残机碎片，走 `ce_spawn_item`）；绿 → **自建整颗 Bomb 实体**（不走 TH18 type 6）；蓝 / 金 → **什么都不吐**；彩虹 → **1 枚变色令牌**（位置 = 本体现坐标，按 `last_slot2` 选起始态）；先发闪烁中断（每 2 | `ufo_core.c:467-478`、`ufo.c:3403-3442` | 待核 | ✅ |
| **击破开奖** | UFO 位置：×N 弹字 + 爆炸闪光 + `UFO_SEV_BREAK` 音 + `UFO_EtBreak2`（扩圈消弹 `life 3000`、半径 16→640 每帧 +16、40 帧自删）+ 大消弹音；按色额外掉 `bonus`（红→0x10、绿→0x11、蓝/金无）；**每色返还恰好 1 枚令牌**（红→态 0、蓝→1、绿→2、 | `ufo.c:5093-5157`、`ufo_core.c:481-489`、`ufo_core.c:496-517`、`ufo_core.h:415-416` | 待核 | ✅ |
| **离场三条路** | ① **对话中**（`CE_GUI_MSG_ACTIVE()`）⇒ 场上令牌切"离场态"（不反弹、速度 ×2，不主动删）+ UFO **原地自爆 = 击破全额开奖**；② **Boss 在场** ⇒ 每帧写 `$I0 = 999`（`enemy + 0x122c + 0x28c`）让 ECL 自己飞走；**Boss 上升沿**时已在场的那 | `ufo.c:1395-1421`、`ufo.c:6046-6099`、`ufo_core.h:862` | 待核 | ✅ |
| **满槽补偿**（槽里已有成组再收一枚令牌） | `UFO_SLOT_FULL_PIV_ADD 100000`（存储）/ `UFO_SLOT_FULL_POPUP_VALUE 1000`（显示弹字）/ 弹字色 `0xff80ff80`；实装落笔 = `ce_piv_add(UFO_ID_NUM, 1000)` + `ce_popup`（**一次调用 = 一枚令牌被消耗**，多枚各加一次） | `ufo_core.h:186-188`、`ufo.c:4992-5059` | 待核 | ✅ |
| **信仰点（跨卡）吸收** | 自建实体（110/111 的声明式列表）走同一拍：先扫道具池、再扫声明式列表；大/中信仰点**按 +1 / 颗**计数；P 等价物通道按色分流（红/彩虹 → `UFO_ABSORB_P`，蓝/绿/金 → `MONEY`）；满 P + 协同卡在场时「金钱→P」按**注册表**产出樱点/中信仰点 | `ufo.c:3595-3695`、`ufo.c:3444-3465`、`ufo.c:3490-3504` | 待核 | ✅ |
| **HUD 三格令牌槽** | 3 格（`UFO_SLOT_COUNT 3`），sprite 基数 `457`，四态 `+0..3`（空/红/蓝/绿），亮版 `+4`（`UFO_HUD_SLOT_BLINK_OFFSET`），**每帧直接写 sprite**（不是发中断）；闪烁相 = `s->u.frames % UFO_HUD_BLINK_PERIOD(16) ==  | `ufo.c:789-830`、`ufo.c:5365-5445`、`ufo.c:5235-5244` | 待核 | ✅ |
| **吸收环 / 血量条（实体上的两件 UI）** | 4 只 VM：填充弧 `UFO_GAUGE_SCRIPT_ARC 228`、外/内描边 `229/230`、血条父 `231`（子 232 = 1px 投影）；层 = 环 `7` / 条 `10`；血条色 `0xffff6060`、满宽 `32.0`、高 `4.0`、`BAR_DX +1.0`、`BAR_DY −15.0`；条宽 = `c | `ufo.c:3116-3162`、`ufo.c:3199-3245`、`ufo_core.c:733-739` | 待核 | ✅ |
| **击破计时器（剩余帧数）** | `UFO_TIMER_FIRST_FRAME 60`（存活 ≥60 帧才画）；位置 = UFO 正下方 `TIMER_DY640 21.0`、`TIMER_COLOR 0xc080ffc0`；三条"不画"出口都显式收 VM（防"上一只的时间冻在屏上"）；用 `ce_hud_vm_text`（常驻 VM），**必须在 `on_draw` 相 | `ufo.c:863-877`、`ufo.c:5452-5506` | 待核 | ✅ |
| **×N 击破弹字** | `UFO_XN_LIFETIME 120` 帧；ASCII（`ce_hud_text`，只能 draw 相位）；scale `2.0`，灰 `0xff808080` / 黄 `0xffffff00`，闪法 = 剩余帧 `% 10 >= 5`；开在**击破**那一刻、位置 = UFO（不是收包处） | `ufo.c:4220-4240`、`ufo.c:4298-4432`、`ufo.c:5114-5118` | 待核 | ✅ |
| **记录面板（吸收数 ASCII 面板）** | **关闭**：`UFO_PANEL_ENABLED 0`（整支不编，`ufo_panel_kill/tick` 变空函数） | `ufo.c:4459-4485`、`ufo.c:4589` | ❓自定 | ✅ |
| **召唤特效** | 大横幅（脚本 324，`+75` 帧自删）+ 紫环（`UFO_RING_FX_ENABLE 1`，`UFO_RING_FX_COUNT 1`）+ 蓝闪折线（`UFO_BOLT_FX_ENABLE 1`：每帧 2 条、每段 16 段、步长 `8+rand·8`、每步转 ±18°、寿命 `50` 帧）；**碎屑 `UFO_SUMMON_DEB | `ufo.c:2054-2144`、`ufo.c:2326-2397`、`ufo.c:2126` | 待核 | ✅ |
| **UFO 入场/在场音效（手动循环）** | 在场期间每 `UFO_SE_ALERT_PERIOD 28` 帧重放 `UFO_SE_ENTER`（= `CE_VOICE_UFOALERT`）；**SDK 没有停循环音的通路**，UFO 一消失就**自然停**（不再调用）；`s_alert_left` 是文件静态 `uint16_t` | `ufo.c:3243-3257`、`ufo.c:6117` | ❓自定 | ✅ |
| **SE 表（逐事件 id）** | spawn `0x14`（一手 th12:`0x44a9d9`）+ owner 指定 `CE_VOICE_UFO`；absorb `0x0c`；fill-full `0x2e`；break `0x06`（随色）；EtBreak2 `0x1d`；token take `0x0c`/full `…`；recolor `0x4e`；package  | `ufo.c:141-264` | ❓自定 | ✅ |
| **自建掉落物实体**（赏金压缩包 / 整颗 Bomb） | 池 `UFO_DROP_POOL 4`；层 = 包 `14` / Bomb `13`；包：前 `UFO_DROP_HOME_DELAY_FRAMES 60` 帧**原地不动**，之后 `+0.2/帧` 加速、封顶 `12.0`，朝自机比例步进；Bomb：原生 type 7，`vy += 0.03` 重力 | `ufo.c:1035-1057`、`ufo_core.c:667-713`、`ufo_core.h:611-618` | 待核 | ✅ |
| **赏金倍率 / 结算公式** | `ufo_drop_mult_permille()`：红 = 满火力 `2000` 否则 `1000`；绿恒 `2000`；蓝/金 = `fill≥1 → 8000`、`≥0.5 → 6000`、否则 `2000 + 8·fill`；彩虹 = `4000 / 3000 / 2000+2·fill`。结算：`ufo_drop_settle_ | `ufo_core.c:612-665` | 待核 | ✅ |
| **难度/存档相关的活旋钮** | `UFO_FILL_SCALE_PERMILLE 1000`（🟡占位）、`UFO_HP_ONEHAND_SCALE_PERMILLE 1000`（🟡旋钮）、`UFO_DRAG_SPEED_CONST_MILLI 3000`（❓owner）、`UFO_GOLD_CYCLE 2`（❓自定美术）、`UFO_GOLD_COUNTS_AS_BLUE | `ufo_core.h:448`、`ufo_core.h:709`、`ufo_core.h:374`、`ufo_core.h:128`、`ufo_core.h: | 待核 | ✅ |

### 113 神灵庙（`cards/trance`）

- 一句话：收神灵充灵界槽；槽满按 C 开灵界（强化机体/主炮）；中弹时取消这次死亡先进灵界，窗口结束再补死。（同台账 §113 ✓）

| 机制 | 值 / 判据 | 出处 | 复刻 or ❓自定 | 状态 |
| --- | --- | --- | --- | :-: |
| **灵界槽（600 满 / 3 格 / 每颗灵 +2）** | `TRANCE_GAUGE_MAX 600`；`TIER 200`；`TIERS 3`；`PER_SPIRIT 2`；`TYPE3_BONUS 1`；`DRAIN 1`/帧；`END_PENALTY 200`（夹 0）；`MIN_ENTER 200`；`COOLDOWN_FRAMES 30`；`INITIAL = TIER = 200` | `trance_core.h:95-103`、`:113`；`trance_core.c:19-52`、`131-145` | ✅复刻 | ✅ |
| **收灵奖励表（四型各给什么）** | type0 命碎 1/2；type1 最大得点 **+10 / +100**；type2 符卡碎 1/2；type3 槽 +1（灵界中 0）；槽统一 +2（灵界中 **0**）；type 4 ≡ type 1 | `trance_core.c:390-427`；`trance_core.h:174-184` | 待核 | ✅ |
| **主动发动（D 切卡 + C，槽满）+ 全 8 条门** | 相位 = IDLE ∧ `!in_trance` ∧ `!bombing` ∧ `full (≥600)` ∧ `enemies` ∧ `cooldown ≥ 30` ∧ `!dialogue` ∧ (`key` ∨ `auto_mode`) ∧ `game_flags_ok`。卡侧实际填 **`key = 0`、`auto_mode = | `trance_core.c:153-166`；`trance.c:956-968`、`936-943`、`2401-2425` | 待核 | ✅ |
| **中弹救援 = 延时死（不是免死）** —— 落点 `+0x0c`，救援层**最后一家** | `on_death_after_deathbomb` 里：门⓪-① `acc != 0` ⇒ 让位；门⓪-② 原生 35（金 ≥ `TRANCE_NATIVE_ROKUMON_COST 200`，**不看 acc**）/ 原生 23（`can_bomb`）会出手 ⇒ 让位；门① `ce_rescue_note_owner()`；门② 槽  | `trance.c:2583-2654`；地址/原生卡 `:238-249`；让位判据 `:2555-2577`；core `trance_core.h:351 | 待核 | ✅ |
| **补死（窗口结束再死一次）+ 允许链式** | 死亡强制档 `TRANCE_PHASE_DEATH_FREE = 2`；`trance_end()` 报 `re_die = 1` 且**不**扣 200；道具帧只挂 `redie_pending`，**玩家帧 `on_tick_2`**（`state == 1` 才调）真调 `Player__die 0x45d3a0`；调用前先 `ce | `trance_core.c:210-238`、`:347-351`；`trance.c:3034-3054`、`:2305-2319`、`:2520-2538 | 待核 | ✅ |
| **击破掉灵几颗 = TH13 通道 B 衰减**（杀得越快越多） | `n = (T − alive)/(T/N) + 1`；`alive > T ⇒ 保底 1`；难度 `== 2 ‖ == 3` 再 +1（EXTRA=4 **不加**）；夹取 `[MIN 1, MAX 64]`；`T,N` 读敌人自己的 opcode 574/573 字段（`enemy+0xa4` / `+0x54[0..19]`），`a | 公式 `trance_core.c:575-603`；常量 `trance_core.h:582`、`:591-592`；卡侧读面 `trance.c:4655 | ✅复刻 | ✅ |
| **兜底档：没配窗口的敌人按 HP 分档给 (T,N)** | `TRANCE_KILL_FB_HP_BIG 1000`；**HP < 1000 ⇒ `(60,1)`**（瞬杀 2 颗 / 1 s 窗口）；**HP ≥ 1000 ⇒ `(120,4)`**（瞬杀 5 颗）；`hp_max < 1` ⇒ 小怪档（保守）；`selfcheck` 把 4 个数 + 「只降不升」不变式钉死 | `trance_core.h:641-647`；`trance_core.c:566-571`、`793-799`；卡侧 `trance.c:4677-4684 | ❓自定 | ✅ |
| **Boss 相位击破掉一大批灵**（符卡/非符门 + 终符不掉） | 逐关固定表 `s_phase_batch_boss[7] = {22,22,22,32,37,32,47}`、`midboss[7] = {5,9,24,19,19,19,25}`；构成 `1×type0 + 1×type2 + 其余 type3`；只认 `CE_ENEMY_FLAG_BOSS`；去重 = `counts[]` 求和 >  | 表 `trance_core.c:651-675`；常量 `trance_core.h:838-841`；门/入队 `trance.c:411-457`、`47 | 待核 | ✅ |
| **Boss 随血量减小掉灵 = TH13 通道 A** | 距离分档阈值增量 `d² < 48² ⇒ +200` / `< 96² ⇒ +400` / `< 192² ⇒ +600` / 否则 `+1000`；每敌累计伤害越过 `threshold` 就入队，`threshold = accum + inc`（**累加**不是重置）；同敌冷却 `TRANCE_BOSS_DROP_COOLDOWN  | `trance.c:2721-2748`、`:2697-2718`、`:4851-4911` | ❓自定 | ✅ |
| **连锁（第 10 颗奖励一颗白灵）** | 窗口 `TRANCE_CHAIN_WINDOW 60` 帧（每颗**重置**、每帧 −1）；阈值 `TRANCE_CHAIN_THRESHOLD 10`（`count >= 10`）；奖励 `type 3`、初速写死 `0.5f`；**推进点 = `trance_spirit_spawn()`（掉落时刻）**，type 3/4 不推进 | 常量 `trance_core.h:115-125`；逻辑 `trance_core.c:355-386`；推进点 `trance.c:1836-1858` | 待核 | ✅ |
| **灵界中「中弹无伤」= 闸门（不是无敌）** | 相位非 IDLE ⇒ `ce_no_hit_declare(113,1)`（闸门开在 `Player__die 0x45d3a0` 入口）；**灵界中 `ce_invuln_request(113,0)`**；主动结束给 `TRANCE_END_INVULN_FRAMES 70` 帧无敌；`TRANCE_INVULN_DECLARE 30 | `trance.c:3140-3142`、`:3066-3070`、`:580`、`:568-570`、`:3521-3527` | 待核 | ✅ |
| **机体特技表（四人各一条）** | 表 `s_traits[]`：灵梦 `{drain_div 1, life‰ 1000, 慢吸 1, 判定点小 1}`；魔理沙 `{1,1000, marisa_buff 1}`；**咲夜 `{drain_div 2, life‰ 2000, …}`**；早苗 `{1,1000, sanae_type1_bonus 1}`；越界角色回落  | 表 `trance_core.c:83-97`；`.h:45-88`；卡侧 `trance.c:845-863`、`:2294-2388`、`:2021-202 | 待核 | ✅ |
| **灵界主炮组 + 子机停火（TH13 set10 复刻）** | `on_shot_set_index` 在灵界窗口内返回 `CE_SHT_SET_TRANCE`（**永久专属槽 0x28**），窗口外 `-1`（原生主炮，一个字节不动）；优先级 `CE_SHT_PRIO_113 = 10` < `CE_SHT_PRIO_117 = 20`（**117 赢**）；`on_option_shot_gate | `trance.c:2877-2884`、`:2891-2902`、`:2800-2870`、`:4947-4957` | ✅复刻 | ✅ |
| **神灵实体（自建 VM 池，不是道具、不是敌人）** | 池 `TRANCE_SPIRIT_POOL 128`；寿命 `TRANCE_SPIRIT_LIFE_FRAMES 480`；消失段 `SCRIPT_TAIL_FRAMES 40`（⇒ 可见寿命 ≈ 520 帧）；缩放 `1000‰`（前 200 帧）→ `400‰`（再 200 帧）；拾取半径 `TRANCE_SPIRIT_PICK_RA | 常量 `trance_core.h:871-949`、`:965-966`；策略 `trance_core.c:682-763`；运动常量 `trance.c: | ❓自定 | ✅ |
| **左下角指示槽 HUD（4 支叶子脚本）+ 倒计时文字** | 4 支 VM：框 / 总条 / 第二条 / 游标（`TRANCE_HUD_N 4`，脚本 175/176/177/178）；走 `ce_hud_slot_claim_pinned_card(113, 144×96, (60,936))`；框 `gauge < 200` ⇒ 暗图（边沿换）；总条 `scale.x = uv_scale.u  | = 0x18`；时间 `%.2fs`（厘秒 = `(gauge×drain_div×100 + 30)/60`），封顶 `TRANCE_HUD_TIME_CAP | 待核 | ✅ |
| **魔理沙灵界加成 / 灵梦判定点缩小** | 魔理沙：`CE_PLAYER_SPEED_MULT *= TRANCE_MARISA_SPEED_MUL 1.5f`（写在 `on_tick_2`，须早于 Player tick）；伤害走 SDK 千分比层（`ce_set_bullet_damage_permille`，**删掉了自定 1250**，中性值 `CE_DMG_UNIT`）。 | `trance.c:2384-2387`、`:862-863`、`:2349-2382`、`:845-848` | 待核 | ✅ |
| **通道 E：死亡动画期间每帧撒白灵** | `on_death_frame2`（`+0x14`）里**只在 `gauge < 200`** 时开窗 `TRANCE_DEATH_ECHO_FRAMES 30`；`on_item_frame` 帧末每帧一颗 `type 3`、速度 `2.0f`，位置 = 自机 | 常量 `trance.c:711-717`；开窗 `:3754-3781`；帧末撒 `:3601-3616` | 待核 | ✅ |
| **灵界视觉叠层：形态 / astral / 幕布 / 遮罩 / 弹窗 / 满格提示** | 本体形态 sprite 按 `TRANCE_FORM_BASE[4]` 加 `0..23`（`TRANCE_FORM_FRAMES 24`）并在退出/删卡还原；astral `TRANCE_BG_ENABLE 1`、`TRANCE_BG_LAYER 5`、`TRANCE_BG_ANGLE_VEL 0.010471975f`；进出幕布 `T | 形态 `trance.c:4213-4322`；背景 `:1519-1524`、`:3370-3415`；幕布 `:1526-1613`、`:3198-3449 | ✅复刻 | ✅ |
| **计分：蓝灵最大得点 + 点道具 +100（载体 = 共享 PIV 池 + 计分层）** | 收 type1 ⇒ 卡内累计 `max_point_bonus += 10`（普通）/`+100`（灵界内，档位按**出生旗标** `born_trance`）；落笔 `ce_piv_add(card_id, r.max_point)`；**同时**每帧 `ce_score_declare_delta(113, max_point_bon | `trance.c:3827-3843`、`:986-1000`、`:3647`、`:3800-3805`、`:4412`、`:2963-2988`；常量 `: | 待核 | ✅ |
| **收集表现的音效/声像对齐（按 wav 名逐枚对齐 TH13↔TH18）** | 收集 `0x3f se_lgodsget`（声像 = 神灵实体 x）；出生 `0x3b..0x3e se_lgods1..4`（按 type）；灵界进入 `0x34 se_boon01`（声像 = 自机 x，**手动路才播**）；跨格 `CE_VOICE_ASTRALUP`（新扩容 id `0x57`，源自 TH13 独有的 `se_as | `trance.c:251-293`；调用点 `:3848`、`:3901`、`:2422`、`:3533`；出生音 `:1822-1834` | ✅复刻 | ✅ |
| **灵界 BGM：DLL 侧第二路流 + 交叉淡化** | `TRANCE_BGM_XFADE_FRAMES 20`；自建 `CStreamingSound`；相位进灵界即起、回 IDLE 即淡出；删卡/局末 `ce_trance_bgm_release(1)`（**immediate**）；暂停期 `on_draw` 里 `ce_trance_bgm_pause_poll()` | `trance_bgm_core.h:86`；`trance_bgm.c:1059-1068`；`trance.c:3175-3185`、`:4423-4427 | 待核 | ✅ |
| **Jera(88) 复制代数闸 / 跨卡只读标志（108）** | 每帧 `ce_dup_publish(113, 可见神灵数)`；代数一变按原档重撒（`trance_spirit_spawn` 是唯一出生口），传 `core = NULL`（不推进连锁）+ 静音；每帧 `ce_card_flag_set(113, 相位≠IDLE ? CE_CARD_FLAG_TRANCE : 0)`（粘性） | `trance.c:2261-2292`、`:2300-2301`、`:2993-2998` | ❓自定 | ✅ |
| **明确不做（负结论同样是一手机制）** | ❌ 伤害倍率（每帧把 `ce_set_bullet_damage_permille` 拉回 `CE_DMG_UNIT`）；❌ 清弹 / 全屏收集 / 得分倍率；❌ 写 `player+0x47774`（复活无敌 zTimer）；❌ 直写 sht 内存；❌ 本卡全文件 0 处 `ce_item_collect_all` / `ce_canc | `trance.c:22-32`、`:857-860`、`:3655-3660`、`:4395-4417` | 待核 | ✅ |
| **调试键（发布前置 0）** | `TRANCE_DEBUG_KEY 1`（当前开）；`SPACE` = 爆一批（蓝 type1 ×32 / 白 type3 ×6 / 绿 type2 ×1 / 红 type0 ×1）围绕自机半径 `60 px`（`CTRL+SPACE` = `200 px`）；`SHIFT+SPACE` = 槽 `+200`；**不推进连锁** | `trance.c:783-888`、`:3667-3686`、`:1886-1924` | 待核 | ✅ |
| **通道 E 接没接线** | 文件头「⛔ 仍未接线」里写 `**通道 E（玩家死亡：死亡动画期间每帧一颗 type 3，一手 th13:0x4434d8）** —— **仍未接线**` | **已接线**：`on_death_frame2` 开 30 帧窗（`s->death_echo = TRANCE_DEATH_ECHO_FRAMES`），`o | 待核 | ✅ |
| **兜底档 (T,N) 数值** | `trance.c` 的 2026-09-23 段写 `if (hp >= TRANCE_KILL_FB_HP_BIG) { T = 300; N = 10; } // 大妖精档` 与 `else { T = 120; N = 1; }`（第一轮旧值）；同段还写 `兜底与原生档**同档同量**`；`.h:614` 写 `` `N` 的两档 | `TRANCE_KILL_FB_T_SMALL 60 / N_SMALL 1`、`T_BIG 120 / N_BIG 4`；`selfcheck` 逐条钉死（` | 待核 | ✅ |
| **计分声明停用还是接线** | 文件头整段 `## ⛔ 计分声明：**已停用**`＋`**一个字节都不往计分层写**`；`trance.c:3645-3646` 也写 `★ **已停用**…只记本地账，不发声明`；`trance.c:4397-4398` 写 `本卡现在**根本不进计分层**`；`trance_on_spirit_collected` 的日志仍写 `sc | **已接线**：`trance_declare_score()` 每帧 `ce_score_declare_delta(113, decl)`（`:992`）， | 待核 | ✅ |
| **astral 背景默认开还是关** | 「为什么默认关」段（2026-09-13）写 `⚠★ **为什么默认关**（不是没做，是**不许把近似包装成复刻**）`、`⇒ 置 1 打开` | **`TRANCE_BG_ENABLE 1`（已开）**；`TRANCE_BG_LAYER` 已从 3 改成 **5**（第十轮） | 待核 | ✅ |
| **`+0x14` 只做日志** | 文件头 `2. **+0x14（on_death_frame2）只做日志**`；body 顶部 `本回调不做任何动作` | 它还**开一个 30 帧的通道 E 窗口**（写 `s->death_echo`）并做 `gauge >= 200` 的门 | 待核 | ✅ |
| **BGM / 背景「资产未做、不起背景 VM」** | 文件头 `**BGM 变体 / 灵界背景 anm / 神灵专属切图**：SPEC §5 全部 ⏳（资产未做）⇒ 不起背景 VM`；`trance_core.h:488-491` 也写 `TH18 侧 astral.anm **还没有切图 / 入 ability.anm**（SPEC §5）⇒ 卡侧目前不用它` | BGM 第二路已接线（`trance_bgm.h` 开关 = 1）；astral 脚本 251 + 层 5 + 幕布 262–269 + 清零幕 271 全部实 | 待核 | ✅ |
| **★ 角色特技表「技能生效吗」（最重）** | `AUDIT.md:8762`：`卡侧每帧 trance_set_character() **幂等**写入`；`:8768`：`卡侧缓存在文件级 s_spirit_life…每帧刷新`；`trance.c:261-265`：`trance_set_character() 由卡侧写一次（ctor / on_stage_start / 换人时 | **`trance_set_character()` 在卡侧从未被调用**（全仓只有 `tests/test_trance_core.c` 调它）⇒ `core | 待核 | ✅ |
| **`TRANCE_HUD_GAUGE_Y` / `TRANCE_INVULN_DECLARE` 等「留痕」常量** | 注释写「（槽已改图形 HUD，这一行不再使用）」「保留常量只为留痕；真要回到…就把 tick 里那个 0 改回它」 | 两者**全文件零引用**（真死常量） | 待核 | ✅ |
| **共享件 `hud_dim` 的坐标定式（影响 113 的块/门限）** | `hud_dim.h` 文件头 + `hud_dim.c:23` 写 `自机坐标 = 表面 px / 2 − (320, 16)`（旧记法） | **活宏 `HUD_DIM_SURF_DX 224.0f`**（`hud_dim.h` 后半段的 ★★★★ 2026-10-03z29 订正：320 是"离屏  | 待核 | ✅ |

### 109 花映塚 — **只列跨卡耦合**
- **小弹过滤器归属**：可消弹判据（弹半径 ≤ 4.0）**只服务暴风** ✓；C1–C4 消弹圈与破罩搜弹**不筛** ✓ 出处 `pofv.c` 的 `cancel_white_in_circle(..., white_only)` ✓ ｜ owner 2026-10-14 裁定 ✓ ｜ ✅（P60）｜ 109 内部**本轮不审** ✓。

## 3. 其它卡（58–92 等，属其它作者）

- 卡面文字与口径见 [`patch/th18/cards.js`](patch/th18/cards.js) ✓；机制细节见各卡的 `native/cards/*.c` 文件头 ✓。
- **纪律**：改它们**先问** ✓；确有必需（共享件升级 / 审计查出真 bug）⇒ `AUDIT.md` §BE 逐条登记（`文件 → 改了什么 → 为什么 → 谁改的 → 怎么验证的`）+ 文件内注释 ✓。

## 4. 待裁（⏳）

见 [`INTENT-AUDIT.md`](INTENT-AUDIT.md) §3。**⏳ 项一律不许当既定设计** ✓。
