# 108 永夜抄（`IN_HUMAN_YOUKAI`）交接文档 —— 接手从这里开始

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。裸地址默认属该版本；
> 引用其他版本带前缀（`th12:…` / `th08:…` / `th16:…`）。
> **姊妹文档**：112 星莲船在 [`HANDOFF-112.md`](HANDOFF-112.md)（两张卡已拆给不同的人/agent）。
> 逐条台账在 [`SINGLE-CARD-TEST.md`](SINGLE-CARD-TEST.md) 的 `§th08` 段，本文只留「状态 / 下一步 / 判据」。

## §0 一句话现状

人妖槽 / 刻符 / 决死染色 / 光环边沿 / 松开 Shift 的恢复**都已做完并实跑验过**；
**只剩「子机」这一条大坑**：妖侧 `slot ≥ 1` 的行不发弹（本体只发一列）、人侧子机滞留。
根因已由实跑**定案 = 分支 B**（owner 19:02 那局打出 `native option slots at low speed - 0/4 live`）⇒ 直接按 §4 的 B 修。

### 部署面（单一来源 = `RELEASE-STATE.json`）

| 项 | 现值 |
| --- | --- |
| DLL | `bf3d12ff8a3b6baf37be2b991798837d`（modkit 四处同 md5）|
| patch | `df14d5707689ca4b66cdfab2b0f642b3` |
| anm / sht | `bf91a0f5…` / `058cec74…`；四份 `pl0X.sht` 见快照 |

## §1 怎么干（每批固定动作）

1. **闸三件**：`cd native && make check`（**rc=0**）、`python3 tooling/check-docs.py`（全绿）、`make dllx87`（**0 条**）。
2. **改了行为必须发版**：`make release MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1`，
   再 `DLL_SRC=$PWD/native/th18_card_expand.dll bash /home/nipporita/renkolab/local/sync_modkit_dll.sh`
   （必须打印 4 条同 md5），最后一起提交 `RELEASE-STATE.json` 与手改的文档。
3. **实跑日志**：`/mnt/d/项目/幻想指南/18/th18_modkit/game/th18_card_expand.log`。
   本机铁律（`local/GAME-LOG.md`）：**用户说「跑了 / 崩了 / 看到 X」，先自己读日志尾巴，别问用户**；
   按 `=== th18_card_expand <时间> ===` **先切出本次**再看。
4. **共享件**（`sdk.c` / `sdk.h` / `sdk_core.*` / `engine.h` / `sites.py` / `Makefile` / `patch/**`）改动，
   必须在 `AUDIT.md` §BE **逐条登记**（文件 → 改了什么 → 为什么 → 谁改的 → 怎么验证的）。

## §2 已完成（逐条给「做了什么 + 怎么验的」）

| 条 | 做了什么 | 怎么验的 |
| --- | --- | --- |
| #33 / #38 松 Shift 后**人侧子机消失 / 定住** | 修 `yk_option_release()` / `ce_option_drop()` 链（per-slot VM id 的删除 / 收激光 / 退位通知那条路，详见本节末）| 日志 `sdk: native options restored (char N, owner card -1, frame …)` ✅ 已出现，且每 20 帧最多一次 |
| #42 刻符尺寸 | 先注入 `scale(2.0)`（**错**：抄了 UI 层规矩）→ owner 报「大了 3 倍」→ 改成**不注入**（刻符是**世界层** VM，本来就吃 `×DAT = ×2`）| 产物回读 `pl00.anm` 的 `script48` 已无 `ins_402`；实跑关键字 `youkai/orb.vm … scale` 应打 **1.00** |
| #26 决死弹幕变红 | 染色链照 115 的**机制**（记原色 / 每帧还原 / 只动 RGB 不碰 alpha）；颜色按 owner 口径改**纯红 `0xffff0000`** | 日志 `youkai: deathbomb window - tinted N live enemy bullet(s) red …` ✅ 链已验；够不够红待眼判 |
| #29 / #32 shift 光环持续触发 | 改成**每个形态边沿只起一次** | 日志 `youkai: body Shift aura ON …` 每进一次妖侧**恰好一行** ✅ 已验 |
| #30 人妖槽看不见 | 生成器给四支 HUD 脚本注入 `layer(25)`（暂停模糊罩那一组）+ 脚本 `pos`/`posTime` 归零（位置全归 C 侧写）| 产物回读 `script49` = `layer(25) + pos(0,0,0)` ✅；几何自述 `anchor (257,924) bar (257,860) text (385,854)` ⇒ **写的数就在左下角**；但 owner 报「**在画面上方被挡**」⇒ 见 §3② |
| #34 / #35 灵梦妖侧不发弹 + 子机不转 | 资产侧已换**逐机体**的妖侧弹脚本（`YK_BULLET_ANM` = pl00/01/03 → 5、pl02 → 9）| `make sht` 回读 `妖侧弹脚本号逐文件对账 OK：{'pl00':5,'pl01':5,'pl02':9,'pl03':5}` ✅；产物 `script5` = `layer(13) + sprite(24/25)`；**旋转**属另一条 #9（`th08:0x44e8d0` 弹簧 + 旋转标签 5，已改待实跑）|

**分支 A 的细目（修哪里）**：① `option+0xb0` 那个 per-slot VM id 的删除；② `ce_option_kill_bullets()` 收激光
（束只认射击键，子机没了它不会自己走）；③ `on_option_pose_apply()` 里 `winner != 我们` 那一支（退位通知）
是否真的走到 —— 它才是「手里还攥着子机」时唯一的收干净点。

## §3 待办（按优先级）

### ① 子机（**最大的一条，四机体共通**）

**症状（owner 逐字）**：妖侧「本体只发一列（正常两列）、**子机不发**」；人侧「本体消失、**定在那发弹**」；
最新一轮：「**依旧子机滞留**」。

**已证（日志，2026-09-23 那局）**：
```
ce: shot-set-index first claim (frame N): edi=0x… [player+0x47940]=0x…
    eax(tier)=… edx(set before)=… stack_slot=… winner id=108 prio=0 set=0x55
```
⇒ 认领的 set 号 **落在对的行表上**（前几轮怀疑的「set 进了别的表」**已排除**）。

**机制（一手，代码侧）**：TH18 主炮行循环按行里 `+0x20` 选膛口 ——
`slot 0` = 自机本体；`slot ≥ 1` = `PLAYER+0x580+slot*0xf0` = **引擎自己那 4 格原生子机**
（一手 `th18:0x45ec4c`–`0x45ec5d`；`engine.h` 的 `CE_NATIVE_OPT_*` 一节），
**不是**卡申请的池（`inner+0x410`；那条路是 `Player__tick_shooters_for_ability_card` `0x40a9c0`，**不读 `+0x20`**）。
我们的 TH08 行按 TH08 的子机编号写（`+0x20` = 0..4，见 `assets/sht/append_shooterset.py` 的 `YOUKAI_TIER_ORIGINS`）
⇒ 那些行要的正是原生那 4 格 —— **可我们低速把它们藏了**（`ce_native_option_hide_declare` + 每帧
`ce_native_option_sweep()` + 隐藏断点把 `lvl` 压 0 ⇒ 连**记录**都不建）⇒ 没有活膛口 ⇒ 只有本体那一列发得出来。

**★★ B-1 已实现并发版（DLL `65f78eab…`，登记 `AUDIT.md` §BE #78）**：① 108 不再声明隐藏；② 新增 `ce_native_option_sweep_keep_state()`（只杀显示、保留 `state`/坐标）。★ 连带修掉第二条机制：`BP_ce_native_option_muzzle` 只在 `ce_native_option_hidden()` 为真时动手 ⇒ 旧版会把**我们自己的** `slot ≥ 1` 行的弹也拦掉（膛口正是原生那 4 格）⇒ 现在是两条一起解开。
⚠ **剩下的一处**：装 108+111 且**机体 = 魔理沙**时，111 仍声明隐藏 ⇒ 隐藏断点把 `lvl` 压 0 ⇒ 那一路 `slot ≥ 1` 仍无膛口。修法与 108 同形（把 111 也改成「只杀显示、保留记录」，并在构建断点里**去掉 `lvl = 0` 那一写**）—— 但**等 B-1 实跑验证过再动**（一次只改一处）。

**★★★ 2026-09-23 实跑定案 = 分支 B**：owner 19:02 那局打出 `youkai: native option slots at low speed - 0/4 live`
（偶见 1/4）⇒ 低速时引擎那 4 格原生子机**基本是空的** ⇒ 我们 TH08 行的 `slot ≥ 1` 没有膛口 ⇒ 只有本体那一列
⇒ 与 owner「本体只发一列、子机不发」逐字对上。**不用再分流，按 §4 的 B 修。**

**已在最新 DLL 里的探针（等一次实跑，三条关键字）**：
- 低速：`youkai: native option slots at low speed - N/4 live …` + 四行 `native opt slot k: state/vm/muzzle`
- 高速：`youkai: native option slots at HIGH speed - N/4 live …` + 同样四行
- 交出子机：`youkai: option release - N slot(s) held, VMs to delete: v0 v1 v2 v3 …`
- ★ **高速安全带（2026-09-23 补，DLL `c85ce2c1…`）**：`youkai: HIGH-SPEED safety release #N - we still held options while unfocused …`
  ⇒ **出现**说明帧界那条「退位交回」没跑到（**那就是「子机滞留」的机制 ⇒ 走分支 A**）；
    **一行都没有** ⇒ 滞留不是这条路 ⇒ 看分支 B 的判据。

### ② 人妖槽「在画面上方被挡」（owner 最新一轮）

**已有两条证据**：几何自述 = `anchor (257,924) bar (257,860) text (385,854) scale x2.0`（**写的数在左下角**）；
而 `youkai/hud.vm …` 逐 VM 实况**一行都没打**（它原来挂在 60 秒节拍上，owner 那两局太短）。
⇒ 最新 DLL 已改成 **HUD 起来时必打一次**。判据：
- `draw` = 我们写的 `(257,860)` 一类 ⇒ 数值生效，问题在**绘制组 / pass**（层 25 → 组 38 那一趟）⇒
  下一轮按 **113 的口径**（写死表面坐标、不进模糊组）重摆，别再在偏移算式里绕；
- `draw` = `(0,0)` 之类 ⇒ 写没落到 VM 上（句柄陈旧 / VM 被引擎重建）⇒ 修 `yk_gauge_hud_spawn` 的自愈那一段。

### ③ #36 切换音效方向反了

owner：「是**进** shift 有『使魔 → 魔法阵』的音效，退出没有」⇒ 现在两颗 wav 的两向挂反（与我们一手的判向相反）。
挂点 = `assets/voice/ORDER.txt` 与 `patch/th18/voice.js`（两向的事件名）；改法 = 交换两个事件名，或双向都播同一颗。
⚠ 一手判向要再核一次再动手。

## §4 子机：两条互斥分支（**已定案 = B**，见下）

| 分支 | 触发判据（日志）| 修法 |
| --- | --- | --- |
| **A. 我们自己的旧 VM 没死** | `youkai: option release …` 里**同一格 ptr 反复出现**，而 `option allocated …` 的 **anm id 每次都是新的** | 修 `yk_option_release()` / `ce_option_drop()` 链：per-slot VM id（`option+0xb0`）的删除、`ce_option_kill_bullets()` 收激光、以及「退位通知」（`on_option_pose_apply` 里 `winner != 我们` 那一支）是否真走到 |
| **B. 引擎原生那 4 格回不来 / 不动** | 高速侧 `native option slots at HIGH speed - 0/4 live`，或四行 `muzzle (x,y)` **恒定不动** | 修上批那个「隐藏 → 可见边沿补 `repopulate`」；或改成**低速不藏原生记录、只杀显示**（行循环要的正是它们的膛口）。两条都要先核 **111 魔理沙侧**对同一层声明的依赖 |

**背景线索（owner 18:49 那局）**：`youkai: option allocated (ptr 183eb908, anm id …)` 出现 7 次以上，
**ptr 恒定、anm id 每次不同**；另有一次 `22637`–`22640` 四格 ptr 各差 `0xf0`（= 正常的「声明 4 格」路径）。
⇒ 这正是 A / B 必须分开的原因。

## §5 与 108 相关的关键位置（省得全仓 grep）

| 事 | 位置 |
| --- | --- |
| 卡主体（人妖槽 / 子机 / 使魔 / 刻符 / 决死 / 光环）| `native/cards/youkai.c`（约 3700 行，段落都有长注）|
| 纯逻辑（无引擎地址、有主机单测）| `native/cards/youkai_core.{h,c}` + `native/tests/test_youkai_core.c` |
| 妖侧 shooterset / 行表 / 每机体弹脚本 | `assets/sht/append_shooterset.py`（`YK_BULLET_ANM` / `YOUKAI_TIER_*`）|
| 妖侧机体的 anm（本体 / 刻符 / 人妖槽 / 字形）| `assets/build_player_anm.py`（`YK_EXTRA_SCRIPTS` / `_YK_EXTRA_LAYOUT`）|
| 原生子机「藏 / 恢复」那一层 | `native/sdk.c`（`BP_ce_repopulate_native_option` / `BP_ce_native_option_muzzle` / `ce_native_option_sweep`）+ `native/sdk_core.c` 的声明层 |
| 子机阵型（逐格偏移）与声明 | `youkai_core.h` 的 `YK_OPTION_BY_CHAR` / `yk_option_offsets`、`youkai.c` 的 `yk_option_declare` |
| 台账（逐条状态与实测）| `SINGLE-CARD-TEST.md` §th08；一手过程在 `AUDIT.md` §BE 详录 #55 起 |

## §6 owner 口径（逐字，最高优先）

> 「装了 th08 之后，**不按 shift 是人侧** / 人侧的自机发弹**和不装 th08 完全一样** / 按了 shift，
> **主机切换到妖侧**，**并且子机也切换到妖侧** / 妖主机和子机按照 th08 里面的一手来做
> （**包括跟随 p 点等级的行为**），**除绝对 dps** / 特别地，**早苗的妖机体 == th16 文自机** /
> 在不按 shift 的时候，应当能够看见在**远离敌人本体**的地方有**红色使魔，火焰形状** /
> 在按 shift 的时候，**使魔变成魔法阵**，**不会对自机造成碰撞**，但**也不会被自机打到**」

- **术语铁律**：说**使魔 = 敌人使魔**；自机的「使魔」一律叫**子机**。
- 快速高低切：「原本 th08 按 shift **延迟 8 帧**切机体，因此魔理沙演化出快速高低切技巧；**不一定要照着 8 帧复刻**，
  我的要求不高，主要是**快速高低切不能出显著的 bug**」⇒ 已按此加固（真杀过才补 + 20 帧冷却）。
- 决死染色：「TH08 决死时场上弹幕的 color **完全是变成红色**，目前的颜色似乎是照着 115 卡复刻的」⇒ 已改**纯红**。
- 已定不做 / 不动：108 的 Power / Point 不动；奖命梯只记录；收点线 107/108 不补一手。

## §7 坑

1. 文档表格**单元格 ≤ 200 字符**（超了 `make check` 直接红；细节挪到小节）。
2. **不加新断点站点**（预算 31，`sites.py` 有账）。
3. 产物回读用 `thanm -l 8 -m v8.anmm native/build/pl00.anm`（别按 `base` 猜着扫 sht —— 踩过，读出过假数据；
   sht 的权威是 `make sht` 自带的对账）。
4. 「部署面 = 本地产物」：直接 `md5sum` modkit 里的 `pl0X.anm/pl0X.sht` 与 `native/build/` 对。
5. **状态别落后于代码**：本会话最大的时间浪费是「已修的被当成新 bug 反复报」。
