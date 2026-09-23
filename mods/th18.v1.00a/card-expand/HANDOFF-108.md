# 108 永夜抄（`IN_HUMAN_YOUKAI`）交接文档

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。裸地址默认属该版本；引用其他版本带前缀
> （`th12:…` / `th08:…` / `th16:…`）。
> **姊妹文档**：112 星莲船在 [`HANDOFF-112.md`](HANDOFF-112.md)。
> **逐条台账**在 [`SINGLE-CARD-TEST.md`](SINGLE-CARD-TEST.md) 的 `§th08` 段；一手过程在
> [`AUDIT.md`](AUDIT.md) §BE 详录；自定值在 [`TUNING.md`](TUNING.md)。**本文只讲：现在什么状态 / 下一步 / 判据。**

## §0 一句话现状

**代码侧已全部做完并逐批发版**：人妖槽定位、子机那条（三步修法）、刻符尺寸、决死染色、光环边沿、
切换音效、松开 Shift 的恢复、快速高低切加固 —— 都在版上。
**唯一剩下的是「验收」**：每一步的判据已写成本文 §3 的关键字，**需要 owner 跑一次游戏**（我读日志即可判定）。

### 部署面（单一来源 = `RELEASE-STATE.json`）

| 项 | 现值 |
| --- | --- |
| DLL | `9bf20bc5f6c84ed1ef24856221d8ed65`（modkit 四处同 md5；同步 = `local/sync_modkit_dll.sh`）|
| patch | `df14d5707689ca4b66cdfab2b0f642b3` |
| anm / sht | 见 `RELEASE-STATE.json` 快照；`make release` 会一起同步进 modkit |

> **部署面审计（2026-09-23 全量核对通过）**：DLL 与 modkit 两处同 md5 且 = `RELEASE-STATE` 的
> `last_release.dll_md5`；`ability.anm` / `abcard.anm` / 四份 `pl0X.anm` / 四份 `pl0X.sht`（`build/sht/`）
> **逐个 md5 与 `native/build/` 相同**。⇒「实跑看到的是不是我发的版本」用 `md5sum` 对一次即可，不用猜。

## §1 怎么干（每批固定动作）

1. **闸三件**：`cd native && make check`（**rc=0**）、`python3 tooling/check-docs.py`（全绿）、
   `make dllx87`（**0 条**）。⚠ 文档表格**单元格 ≤ 200 字符**（超了 `check` 直接红，细节挪到小节）。
2. **改了行为必须发版**：`make release MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1`，
   再 `DLL_SRC=$PWD/native/th18_card_expand.dll bash /home/nipporita/renkolab/local/sync_modkit_dll.sh`
   （必须打印 4 条同 md5），最后一起提交 `RELEASE-STATE.json` 与手改的文档。
3. **实跑日志**：`/mnt/d/项目/幻想指南/18/th18_modkit/game/th18_card_expand.log`。
   本机铁律（`local/GAME-LOG.md`）：**用户说「跑了 / 崩了 / 看到 X」，先自己读日志尾巴，别问用户**；
   按 `=== th18_card_expand <时间> ===` **先切出本次**再看。
4. **共享件**（`sdk.c` / `sdk.h` / `sdk_core.*` / `engine.h` / `sites.py` / `Makefile` / `patch/**`）改动，
   必须在 `AUDIT.md` §BE **逐条登记**（文件 → 改了什么 → 为什么 → 谁改的 → 怎么验证的）。
5. **不加新断点站点**（预算 31，`sites.py` 有账）。

## §2 已完成（逐条：做了什么 + 怎么验的）

| 条 | 做了什么 | 怎么验的 |
| --- | --- | --- |
| **子机 第 0 步（B-1）** | 108 **不再声明隐藏** + 低速改用**新增的** `ce_native_option_sweep_keep_state()`（只杀 VM 的显示、`state`/坐标不动）| 待实跑（§3）；`AUDIT §BE #78` |
| **子机 第 2 步** | 膛口断点加早退「**本帧已有卡认领 set ⇒ 放行**」—— 原来它会把**我们那些行**的弹也拦掉（膛口正是原生那 4 格）| 待实跑；`§BE #79` |
| **子机 第 1+3 步** | 新增每卡声明 `ce_native_option_keep_records()`（108 低速声明）⇒ 构建断点**什么都不做**（不压 `lvl`）⇒ 连 **111 同时声明隐藏（魔理沙机体）**时记录/坐标也照建 | 待实跑（魔理沙那一路）；`§BE #80` |
| **人妖槽定位** | 按 owner「**照着 113** 定位」：锚点写死 **`YK_GAUGE_BASE_X/Y = 248/928`**（= 113 的 `TRANCE_HUD_BASE_*`），**不再吃货架锚点** | 待实跑（§3）；`TUNING §2.0zy` |
| **#42 刻符尺寸** | 先补 `scale(2.0)`（错，抄了 UI 层规矩）→ owner 报「大了 3 倍」→ 改成**不注入**（刻符是**世界层**，本来就吃 `×DAT=2`）| ✅ **实跑已验**：`youkai/orb.vm … scale 1.00 / alpha 255 / layer 7 / draw = (87,216)` |
| **#26 决死弹幕变红** | 染色链照 115 的**机制**（记原色 / 每帧还原 / 只动 RGB 不碰 alpha）；颜色按 owner 口径改**纯红 `0xffff0000`** | ✅ 链已验（`youkai: deathbomb window - tinted N …`）；够不够红待眼判 |
| **#29 / #32 光环持续触发** | 改成**每个形态边沿只起一次** | ✅ **实跑已验**：`youkai: body Shift aura ON` 每进一次妖侧**恰好一行** |
| **#30 人妖槽看不见** | 生成器给四支 HUD 脚本注入 `layer(25)` + 脚本 `pos`/`posTime` **归零**（位置全归 C 侧写）| 资产侧已验（产物 `script49` = `layer(25)+pos(0,0,0)`）；定位法见上（照 113）|
| **#34 / #35 灵梦妖侧弹** | 妖侧弹脚本改**逐机体**（`YK_BULLET_ANM` = pl00/01/03→5、pl02→9）| 资产侧已验（`make sht` 回读 + 产物 `script5` = `layer(13)+sprite(24/25)`）；行为面取决于子机那条 |
| **#36 切换音效方向** | 退出那一声原来套着 `if (fam_live_count)`（「真有使魔才响」）⇒ 表空时**一声不响**；改成**形态边沿无条件播**，两向各加一行日志 | 待实跑（§3）|
| **#37 第一半 人侧火焰使魔** | **自查：资产 / 脚本 / 层 / alpha 四项都没问题**（贴图浅粉 219,164,164、无 `layer()` 由 C 补 7、人侧 alpha 0xff）| 待实跑重看；若仍不见再加逐 VM 实况行 |
| **快速高低切加固** | owner：「不一定要照 8 帧复刻，主要是**快速高低切不能出显著的 bug**」⇒ 补重建只在**真 sweep 掉过 VM** 时触发 + **20 帧冷却**（重建会广播 `on_power_level_change`，116 会跟着重分子机）| 已发版；体现在下方日志节奏里 |
| **高速安全带** | 高速（人侧）每帧兜底：只要我们还攥着子机就当场交回（幂等）+ 一行日志 | 待实跑（§3）|

## §3 待办 = **只等一次实跑**的验收清单（每行都给判据）

> 用当前 DLL 跑一次，然后读日志尾巴（或把这几行发我）。**任何一行不符 ⇒ 最右列就是下一步**。

| # | 关键字 | 期望 | 不符时看哪 |
| --- | --- | --- | --- |
| 1 | `youkai: native option slots at low speed - N/4 live` | **不再是 `0/4`**（灵梦与**魔理沙**都应 ≠0/4；预期 = 当前火力档格数）| 仍 0/4 ⇒ `keep_records` 没生效（§BE #80）或 sweep 仍清 `state`（#78）|
| 2 | 妖侧弹幕 | 应出现**第二列**（子机那一列）| 无第二列 ⇒ 膛口断点仍拦（#79）或行 `+0x20` 与膛口对不上 |
| 3 | `youkai: gauge HUD geometry … anchor` | **248/928**（照 113 的常量）| 还是 257/924 ⇒ 跑的是旧 DLL |
| 4 | `youkai/hud.vm bar/human/youkai/cursor … draw = pos+pos2+ent = 表面 (…)` | 四件 `draw` ≈ 左下角（y 约 860–930）| `draw` 对却看着在上方 ⇒ 绘制组/pass；`draw` = (0,0) ⇒ 句柄陈旧 |
| 5 | `youkai: shift SE 人→妖 … played` 与 `… 妖→人 … played` | **两向各一行** | 缺「妖→人」⇒ 门还在别处；两向都有却听不见 ⇒ 音量/wav |
| 6 | `youkai: HIGH-SPEED safety release #N …` | **一行都没有**最好（有 ⇒ 帧界那条退位交回没跑到 = 「滞留」的机制）| 反复出现 ⇒ 修 `yk_option_release` / `on_option_pose_apply` 的退位分支 |
| 7 | `youkai/orb.vm … scale` | **1.00**（#42；19:02 那局已验）| 若 2.00 ⇒ 跑的是旧 DLL |

## §4 子机那条的来龙去脉（含两条机制的定案）

**症状**（owner 逐字）：妖侧「本体只发一列（正常两列）、**子机不发**」；人侧「本体消失、定在那发弹」；后又报「依旧子机滞留」。

**实跑定案（owner 19:02 那局，日志）**：
```
youkai: native option slots at low speed - 0/4 live
  native opt slot 1: state 0 vm 00000000 muzzle (-7.8, 312.6)
  native opt slot 2/3/4: state 0 vm 00000000 muzzle (0.0, -400.0)
```
⇒ 引擎那 4 格原生子机**全是 `state 0`（未启用）** ⇒ 主炮行循环按行里 `+0x20` 选膛口
（`slot ≥ 1` 取 `PLAYER+0x580+slot*0xf0`，一手 `th18:0x45ec4c`–`0x45ec5d`）时**认不出膛口**。

**机制（两条叠加，都修了）**：
1. **记录根本没建**：108 声明隐藏 ⇒ 隐藏断点把 `lvl` 压 0；且每帧 sweep 把 `state` 写 0。
2. **自己的膛口断点拦弹**：`BP_ce_native_option_muzzle` 只看 `ce_native_option_hidden(机体)` ⇒ 会把**膛口落在原生数组里的每一发**都拦掉 —— 而我们 `slot ≥ 1` 的膛口**正是**那 4 格 ⇒ **就算记录活着，那一列也会被自己吞掉**。

**修法三步**（都已发版，`AUDIT §BE #78` / `#79` / `#80`）：
- **B-1**：108 不再声明隐藏 + 新增 `ce_native_option_sweep_keep_state()`（只杀显示、保留记录）。
- **第 2 步**：膛口断点「本帧有卡认领 set ⇒ 放行」（用**帧号**比较：选 set 与投弹在同一玩家 tick 内）。
- **第 1+3 步**：新增 `ce_native_option_keep_records()`；构建断点在有卡这么声明时**什么都不做**。
  ⚠ **为什么不能「只留记录不留 VM」**：引擎建记录的那一刻就同时建 VM（同一支 `0x45d610`）⇒ 只能让**显示**
  交给每帧的 sweep 收。⇒ **111 的「原生不可见」与 108 的「记录活着、行循环有膛口」并存** ✓

## §5 关键位置（省得全仓 grep）

| 事 | 位置 |
| --- | --- |
| 卡主体（人妖槽 / 子机 / 使魔 / 刻符 / 决死 / 光环 / 音效）| `native/cards/youkai.c`（约 3700 行，段落都有长注）|
| 纯逻辑（无引擎地址、有主机单测）| `native/cards/youkai_core.{h,c}` + `native/tests/test_youkai_core.c` |
| 妖侧 set / 行表 / 逐机体弹脚本 | `assets/sht/append_shooterset.py`（`YK_BULLET_ANM` / `YOUKAI_TIER_*`）|
| 妖侧机体的 anm（本体 / 刻符 / 人妖槽 / 字形）| `assets/build_player_anm.py`（`YK_EXTRA_SCRIPTS` / `_YK_EXTRA_LAYOUT`）|
| 原生子机的「藏 / 恢复 / 保留记录」层 | `native/sdk.c`（两支断点 + 两支 sweep）+ `native/sdk_core.c` 的两个声明层 |
| 子机阵型与声明 | `youkai_core.h` 的 `YK_OPTION_BY_CHAR` / `yk_option_offsets`；`youkai.c` 的 `yk_option_declare` |
| 人妖槽定位常量 | `youkai.c` 的 `YK_GAUGE_BASE_X/Y`（= 248/928，照 113）|

## §6 owner 口径（逐字，最高优先）

> 「装了 th08 之后，**不按 shift 是人侧** / 人侧的自机发弹**和不装 th08 完全一样** / 按了 shift，
> **主机切换到妖侧**，**并且子机也切换到妖侧** / 妖主机和子机按照 th08 里面的一手来做
> （**包括跟随 p 点等级的行为**），**除绝对 dps** / 特别地，**早苗的妖机体 == th16 文自机** /
> 在不按 shift 的时候，应当能够看见在**远离敌人本体**的地方有**红色使魔，火焰形状** /
> 在按 shift 的时候，**使魔变成魔法阵**，**不会对自机造成碰撞**，但**也不会被自机打到**」

- **术语铁律**：说**使魔 = 敌人使魔**；自机的「使魔」一律叫**子机**。
- 快速高低切：「原本 th08 按 shift **延迟 8 帧**切机体，因此魔理沙演化出快速高低切技巧；**不一定要照着 8 帧复刻**，
  要求不高，主要是**快速高低切不能出显著的 bug**」⇒ 已按此加固（真杀过才补 + 20 帧冷却）。
- 决死染色：「TH08 决死时场上弹幕的 color **完全是变成红色**，目前的颜色似乎是照着 115 卡复刻的」⇒ 已改**纯红**。
- 定位：「人妖槽好像在整个画面的**上方**被挡住了……你难道就不能照着 **113 116 117** 定位吗？」⇒ 已按 113 的常量口径。
- 已定不做 / 不动：108 的 Power / Point 不动；奖命梯只记录；收点线 107/108 不补一手。
- 规格第 3 条「跟随 P 点等级」**已静态核过成立**：机体 0/1/2 走手写死的 TH08 妖侧家位置（TH08 里就是 1/1/4 格、与 P 无关）；
  机体 3 早苗走 TH18 原生阵型表，格数 = `floor(P/100)`（TH16 文文口径）⇒ P 涨一格多一个子机、站位换档。

## §7 坑

1. 文档表格单元格 ≤ 200 字符（`make check` 会红）；一手结论要回流 `engine/`，自定值要进 `TUNING.md`。
2. 产物回读用 `thanm -l 8 -m v8.anmm <file>`；v8 的 `drawPoly` = **ins_604**、`color2` = **ins_406**
   （**用名字 grep 产物会落空**）。
3. 跨版本比 anm 脚本前**先确认是哪一份 anm**：我们的使魔本体来自 TH08 **`enemy.anm`** script53，
   不是 `etama.anm` 的同号脚本（踩过）。
4. 「部署面 = 本地产物」要显式核一次（`md5sum` modkit vs `native/build/`）——日志里的旧现象常常是旧产物。
5. **状态别落后于代码**：已修的要立刻改台账状态列，否则下一轮会被当新 bug 重报（本会话最大的时间浪费）。
