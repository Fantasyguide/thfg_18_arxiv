# 108（永夜抄）/ 112（星莲船）交接文档 —— 接手从这里开始

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；
> 引用其他版本一律带前缀（`th12:0x44ab00`、`th08:0x44e8d0`、`th16:…`）。
>
> **这份文档是干什么的**：2026-09-23 那一整轮里，108/112 的排查跨了很多批，过程散在
> `SINGLE-CARD-TEST.md` 的几十行表格、`AUDIT.md` §BE 详录、`TUNING.md` 各节里。
> 这里只回答三个问题：**现在是什么状态**、**下一步做什么**、**每一句结论的判据是什么**。
> 细则仍以原文件为准（本文每节都写了该回哪份文件看全文）。

## §0 一句话现状

- **112 星莲船**：HUD 那条线（槽位 / 槽底横幅 / 音效）已做完并实跑验过；剩 **#14 召唤紫环+雷电**、**#20 击破 ×N** 两件美术未实现。
- **108 永夜抄**：人妖槽 / 刻符 / 决死染色 / 光环边沿已做完并验过；**只剩「子机」这一条大坑**（妖侧 `slot≥1` 的行不发弹、人侧子机滞留），而且根因的**两种可能还没分流** —— 分流只差一次实跑日志。

### 部署面（单一来源 = `RELEASE-STATE.json`）

| 项 | 现值 |
| --- | --- |
| DLL | `bf3d12ff8a3b6baf37be2b991798837d`（modkit 四处同 md5；同步脚本 `local/sync_modkit_dll.sh`）|
| patch | `df14d5707689ca4b66cdfab2b0f642b3` |
| anm | abcard `bf91a0f5…` / ability `058cec74…`（后者含 112 #17 的毛笔横幅）|
| 记账 | `make release MODKIT_DIR=… NO_GIT=1` 会自动跑 `release-check` → `dist` → `release.py` → `record` |

## §1 怎么接手（十分钟上手）

1. 读本文 §2（已完成）→ §3（待办）→ §4（两条互斥分支）→ §6（工具与坑）。
2. 闸三件（每批必过）：`cd native && make check`（**rc=0**）、`python3 tooling/check-docs.py`（全绿）、`make dllx87`（**0 条**）。
3. 改了行为就要发版：`make release MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1`，
   再 `DLL_SRC=$PWD/native/th18_card_expand.dll bash /home/nipporita/renkolab/local/sync_modkit_dll.sh`
   （必须打印 4 条同 md5），最后把 `RELEASE-STATE.json` 一起提交。
4. 实跑日志：`/mnt/d/项目/幻想指南/18/th18_modkit/game/th18_card_expand.log`（本机铁律 `local/GAME-LOG.md`：
   **用户说「跑了 / 崩了」，先自己读日志尾巴，别问用户**）。日志按 `=== th18_card_expand <时间> ===` 分次，**先切出本次**。
5. 用户跑完一轮后，§3 的「等实跑关键字」就是验收清单 —— 逐条 grep 即可。

## §2 已完成（含验证方式）

### 112 星莲船

| 条 | 做了什么 | 怎么验的 |
| --- | --- | --- |
| #21 槽带位置 | 锚点改成 **113 的块基准 248/928**（`trance.c` 的 `TRANCE_HUD_BASE_X/Y`）⇒ 槽心 **272/314/356** | 日志 `ufo: HUD up … pinned at surface (248, 928) … aligned to card 113's block` ✅ 已验；一手也定案（TH12 那个 anm 空间 = 640 屏幕空间 x 方向 +96）|
| #17 槽底毛笔横幅 | 生成器加 `BANNER_APPEAR`（TH12 `front00.png` (336,368,96×16)）+ ability.anm **script315**；C 侧 `ufo_hud_ensure()` 先起、`ufo_hud_kill()` 收 | 日志 `ufo: brush banner under the slots up …` ✅ 已验（观感待眼判）|
| #18 token 生成音效 | 新 SE 通道项（构建期从 TH12 派生 wav）| `ufo/se tally …` 计数 |
| #19 召唤清槽 | 代码侧本来就是一手口径（只在「UFO 结束」时清） | 那一局**无** `ufo: slots cleared`、而 tally 有 `ufo-fill-full 2 / ufo-break 2` ✅ 不重现 |

### 108 永夜抄

| 条 | 做了什么 | 怎么验的 |
| --- | --- | --- |
| #33 / #38 松 Shift 后人侧子机消失/定住 | `sdk.c` 的 `BP_ce_item_frame` 加**隐藏→可见边沿补 `repopulate`**（先写 `CE_OFF_OPT_LAST_LEVEL=-1` 破短路缓存）；只在**真 sweep 掉过 VM** 时触发 + **20 帧冷却**（重建会广播 `on_power_level_change`，116 会跟着重分子机）| 日志 `sdk: native options restored (char N, owner card -1, frame …)` ✅ 已出现（每 20 帧最多一次）|
| #42 刻符尺寸 | 先补 `scale(2.0)`（错，抄了 UI 层规矩）→ owner 报「大了 3 倍」→ 改成**不注入**（刻符是**世界层**，本来就吃 ×DAT=2）| 产物回读 `script48` 已无 `ins_402`；实跑判据 `youkai/orb.vm … scale` 应打 **1.00** |
| #26 决死弹幕变红 | 染色链照 115 的机制（只动 RGB、每帧还原）；颜色按 owner 口径改**纯红** `0xffff0000` | 日志 `youkai: deathbomb window - tinted N …` ✅ 链已验；够不够红待眼判 |
| #29 / #32 shift 光环持续触发 | 改成**每个形态边沿只起一次** | 日志 `youkai: body Shift aura ON` 每进一次妖侧**恰好一行** ✅ 已验 |
| #30 人妖槽看不见 | 生成器注入 `layer(25)`（暂停模糊罩组）+ 脚本 `pos` 归零（位置全归 C 侧）| 产物回读 `script49` = `layer(25)+pos(0,0,0)` ✅；几何自述 `anchor (257,924) bar (257,860)` ⇒ **数值本就落左下角**；但 owner 报「**在画面上方被挡**」⇒ 见 §3 |

## §3 待办（按优先级）

### ① 108 子机（**最大的一条，四机体共通**）

**症状（owner 逐字）**：妖侧「本体只发一列（正常两列）、子机不发」；人侧「本体消失、定在那发弹」；最新一轮又报「**依旧子机滞留**」。

**已证（日志）**：`ce: shot-set-index first claim (frame N): edi=0x… [player+0x47940]=0x… eax(tier)=… edx(set before)=… stack_slot=… winner id=108 prio=0 set=0x55`
⇒ 我们认领的 set 号**落在对的行表上**（前几轮怀疑的「set 进了别的表」**排除**）。

**机制（代码侧一手）**：TH18 主炮行循环按行里 `+0x20` 选膛口 —— `slot 0` = 自机本体，`slot ≥ 1` =
`PLAYER+0x580+slot*0xf0` = **引擎自己那 4 格原生子机**（一手 `th18:0x45ec4c`–`0x45ec5d`；`engine.h` 的
`CE_NATIVE_OPT_*` 一节），**不是**卡申请的池（`inner+0x410`；那条路是 `Player__tick_shooters_for_ability_card`
`0x40a9c0`，**不读 `+0x20`**）。而我们的 TH08 行是按 TH08 的子机编号写的（`+0x20` = 0..4，见
`assets/sht/append_shooterset.py` 的 `YOUKAI_TIER_ORIGINS`）⇒ `slot ≥ 1` 的行要的正是原生那 4 格 ——
**可我们低速把它们藏了**（`ce_native_option_hide_declare` + 每帧 `ce_native_option_sweep()` + 隐藏断点把
`lvl` 压 0 ⇒ 连**记录**都不建）⇒ 那些行没有活膛口 ⇒ 只有本体那一列发得出来。

**下一步 = 先分流，别乱改**（两种可能的修法方向**相反**）：见 §4。

**已加的探针（都在最新 DLL 里，等一次实跑）**：
- 低速：`youkai: native option slots at low speed - N/4 live …` + 四行 `native opt slot k: state/vm/muzzle`
- 高速：`youkai: native option slots at HIGH speed - N/4 live …` + 同样四行
- 交出子机：`youkai: option release - N slot(s) held, VMs to delete: v0 v1 v2 v3 …`

### ② 108 人妖槽「在画面上方被挡」（owner 最新一轮）

**已有两条证据**：几何自述 = `anchor (257,924) bar (257,860) text (385,854) scale x2.0`（**写的数在左下角** ✓）；
`youkai/hud.vm …` 逐 VM 实况**没打出来**（原来挂在 60 秒节拍上，owner 那两局太短）。
⇒ 最新 DLL 已改成 **HUD 起来时必打一次**。判据（下次实跑）：
- `draw` = 我们写的 `(257,860)` 一类 ⇒ 数值生效、问题在**绘制组 / pass**（层 25 → 组 38 那一趟）⇒
  下一轮按 113 的口径（**写死表面坐标、不进模糊组**）重摆，别再算偏移；
- `draw` = `(0,0)` 之类 ⇒ 写没落到 VM 上（句柄陈旧 / VM 被引擎重建）⇒ 修在 `yk_gauge_hud_spawn` 的自愈那一段。

### ③ 108 #34 / #35（灵梦妖侧不发弹 + 子机不转）

资产侧**已验**：产物 `pl00.anm` 的 `script5` = `layer(13)` + `sprite(24/25)`（真弹贴图 + 世界层）；
`make sht` 回读 `妖侧弹脚本号逐文件对账 OK：{'pl00':5,'pl01':5,'pl02':9,'pl03':5}`。
**旋转** = 另一条 #9（`th08:0x44e8d0` 弹簧 + **旋转标签 5**，已改 · 待实跑）—— 不是新活。
行为面仍取决于 §4 那条。

### ④ 108 #36（切换音效方向反了）

owner：「是**进** shift 有『使魔→魔法阵』的音效，退出没有」⇒ 现有两颗 wav 的两向挂反（与我们一手的判向相反）。
挂点 = `assets/voice/ORDER.txt` 与 `patch/th18/voice.js`；改法 = 交换两个事件名，或双向都播同一颗（一手判向要再核一次）。

### ⑤ 112 #20 击破 UFO 的 ×N（**规格已全**，实现即可）

见 §5。

### ⑥ 112 #14 召唤紫环 + 雷电（一手未定，方法已备）

见 §6.2。

## §4 108 子机：两条互斥分支（**等日志分流**）

| 分支 | 触发判据（日志）| 修法 |
| --- | --- | --- |
| **A. 我们自己的旧 VM 没死** | `youkai: option release …` 里**同一格 ptr 反复出现**，而 `option allocated …` 的 **anm id 每次都是新的** | 修 `yk_option_release()` / `ce_option_drop()` 链：per-slot VM id（`option+0xb0`）的删除、`ce_option_kill_bullets()` 收激光、以及「退位通知」那条路是否真的走到 |
| **B. 引擎原生那 4 格回不来 / 不动** | 高速侧 `native option slots at HIGH speed - 0/4 live`，或四行 `muzzle (x,y)` **恒定不动** | 修上批那个「隐藏→可见边沿补 `repopulate`」；或改成**低速不藏原生记录、只杀显示**（行循环要的正是它们的膛口）；两条都要先核 **111 魔理沙侧**对同一层声明的依赖 |

**背景线索（owner 18:49 那局日志）**：`youkai: option allocated (ptr 183eb908, anm id …)` 出现 7 次以上，
**ptr 恒定、anm id 每次不同**（另有一次 `22637`–`22640` 四格 ptr 各差 `0xf0` = 正常的「声明 4 格」路径）
⇒ 这正是 A/B 必须分开的原因。

## §5 112 #20 ×N 的实现规格（一手，照做即可）

- **创建点**：`th12:0x44ab00`–`0x44ab5f` ——
  `push $0x86` → `call 0x4615a0` → 存 `+0x40`（兄弟 VM = 脚本 134）；
  `push $0x88` → `call 0x4615a0` → 存 `+0x44`（**★ ×N = 脚本 136**）；
  `movl $0x2d0,0x28(%esi)` = **寿命 720 帧**。
- **脚本原文**（`local/th12.v1.00b/anm/front/front.anm.txt`）：
  `script136 { scriptNew(137); sprite(-1); drawRect(32,4); layer(10); color(255,96,96); pos(0,-28); stop(); 中断1=delete }`
  `script137 { sprite(-1); drawRect(32,4); layer(9); color(0,0,0); pos(1,1); stop(); 中断1=delete }`
  ⇒ 亮红 + 黑、错开 (0,−28)/(1,1) = **描边式文字**（不是贴图）。
- **每帧更新**：`th12:0x44a80b`–`0x44a83b`（取 `[ebx+0x44]` → `call 0x461920` 查 VM，返回 0 就清句柄 →
  `orl $0x8,0x47c(%eax)` → `fstps 0x40(%eax)`）⇒ 文字内容与坐标**由 C 每帧写**。
- **我们的实现要点**：用 112 现有 HUD 字形通道拼「×」+ 数字（**不新造贴图**）；颜色 `(255,96,96)` +
  黑描边错开 `(1,1)`；寿命 **720 帧**；生成时机 = 击破 / 结算那一刻；位置每帧写。

## §6 工具与方法（都是本轮验过能用的）

### 6.1 全 exe 的 VM 创建清单（找「某特效是哪个脚本」的通用办法）

```bash
objdump -d local/th12.v1.00b/th12.exe > /tmp/th12.asm        # 约 205782 行
# 只认 call 0x4615a0（anm 实例化），脚本号 = 它前面最近的 push $0xNN
```

TH12 v1.00b 全 exe 共 **142 处**创建点。脚本号 ≥ 0x40 里与 UFO / GUI 家族相邻的：

| 地址 | 脚本 | 结论 |
| --- | --- | --- |
| `0x421371` | `0x55` | ❌ 排除：紧接**分数运算**（`imul $0xf4240`、`cmp $0x3b9aca00`、写 `0x4b0c44`）+ 逐槽读表 ⇒ **收令牌 / 算分**那条路 |
| `0x421619` | `0x51` | ❌ 排除：建完 `lea 0x7(%ebx),%edx` = **中断 7+格号** ⇒ **槽 VM 创建**（脚本 81 = 槽闪光，已知）|
| `0x4314e3` / `0x431593` | `0x51` / `0x52` | 大横幅 82 的另一处调用点 |
| `0x436622` | `0x4d` | ⚠ **候选①已降级**（切图看过，见 6.2）|
| `0x438429` | `0x50` | 未查 |
| `0x448ab1` / `0x448adf` | `0x63` / `0x75` | 未查（`0x447xxx`–`0x448xxx` = 结算 / 分数族）|
| `0x44a96e` | `0x52` | ✅ **召唤 C 路径唯一那一支** = 大横幅（`front02.png` 304×32、`layer(6)`、`pos(224,208)`）|

### 6.2 #14 紫环 + 雷电：已排除与下一步

- **排除①**：召唤的 C 路径（`th12:0x44a8xx`–`0x44a97x`）里**只建一支 VM**（脚本 0x52 = 大横幅）。
- **排除②**：**不是靠 `color` 染的** —— 扫全部 TH12 anm dump，紫色系只有 `bullet.anm` 的纯品红 `(255,0,255)`
  ⇒ 紫环是**贴图**（或颜色由代码后写）。
- **候选①降级**：`0x436622` 建的是 `bullet.anm` **script77**（`sprite380` + `scale 0.3→1.0` +
  `rotate(−π)→rotateTime(π)` + `angleVel`）；把 `bullet/etama2.png` 的 (0,16,64×64) 切出来看过 ——
  是**红/粉色的圆环 + 放射状尖刺**（形状对、**颜色不对**），且紧接着是**逐档读表算分** ⇒ 更像「收令牌那一圈」。
- **下一步**：`0x448xxx`（`0x63`/`0x75`）、`enemy.anm` **script135 的 `scriptNew` 链**（UFO 本体脚本来自
  `enemy.anm` script135，见 `assets/ability/scripts/164_ufo_red.anm.txt` 文件头），以及「颜色由代码后写 VM
  （`+0x524`）」这一可能。

### 6.3 产物 / 资产怎么读（别用土办法）

- sht 的权威回读 = **`make sht` 自带的对账**（`妖侧弹脚本号逐文件对账 OK …`）；**不要**自己按
  `base 0x180/0x4e0` 猜着扫（踩过一次：`base` 猜错读出假数据）。
- anm 的权威 = `thanm -l 8 -m v8.anmm <file>` 反汇编产物；改前 / 改后逐句比。
- 「部署面 = 本地产物」的核对：直接 `md5sum` modkit 里的 `pl0X.anm` 与 `native/build/` 对。
- 切图看形状：用 PIL 从 `<anm>/<entry>.png` 按 anm dump 的 sprite 表裁（本轮就是这么确认 sprite380 的）。

## §7 纪律与坑（每批都踩得到的）

1. **闸三件**：`make check`（rc=0）/ `check-docs.py`（全绿）/ `dllx87`（0 条 x87）。文档表格**单元格 ≤ 200 字符**，
   超了就把细节挪到小节、表格只留一句（否则 `check` 直接红）。
2. **不加新断点站点**（预算 31，`sites.py` 有账）；改共享件（`sdk.*` / `engine.h` / `sites.py` / `Makefile` /
   `cards_def.c` / `patch/**` / `assets/ability/scripts/**`）必须在 `AUDIT.md` §BE **逐条登记**。
3. **脚本号由构建侧定**：`assets/ability/scripts/` 的文件名前缀必须是 `68 + 排序序号`（`make anm` 报
   「编号应为 N」就以它为准）；ORDER 片段（`entries/ORDER.ufo_hud.txt`）要**手写拼进** `entries/ORDER.txt`
   （生成器只写片段；没拼会报 `未定义的占位 @…`）。
4. **行为改动必须发版**（DLL 四路径同 md5 + `RELEASE-STATE.json` 记账）并在 `SINGLE-CARD-TEST.md` 写**实跑验收关键字**；
   自定值进 `TUNING.md`；一手结论回流 `engine/`。
5. **记录要能分辨「已修」与「待修」**：本会话最大的时间浪费就是「已修的被当成新 bug 反复报」（文档状态没跟上代码）。

## §8 owner 口径摘要（逐字，最高优先）

- **108 完整规格**（2026-09-23）：「装了 th08 之后，**不按 shift 是人侧** / 人侧的自机发弹**和不装 th08 完全一样** /
  按了 shift，**主机切换到妖侧**，**并且子机也切换到妖侧** / 妖主机和子机按照 th08 里面的一手来做（**包括跟随 p 点等级的行为**），
  **除绝对 dps** / 特别地，**早苗的妖机体 == th16 文自机** / 在不按 shift 的时候，应当能够看见在**远离敌人本体**的地方有
  **红色使魔，火焰形状** / 在按 shift 的时候，**使魔变成魔法阵**，**不会对自机造成碰撞**，但**也不会被自机打到**」
- **术语铁律**：以后说**使魔只表示敌人使魔**；自机的「使魔」是**子机**。
- 快速高低切：「原本 th08 按 shift **延迟 8 帧**切机体……**不一定要照 8 帧复刻**，要求不高，主要是**快速高低切不能出显著的 bug**」。
- 决死染色：「TH08 决死时场上弹幕的 color **完全是变成红色**，目前的颜色似乎是照着 115 卡复刻的」⇒ 已改**纯红**。
- 112 #21：「槽**偏右**……」+「**参考一下 113 的位置**」⇒ 已按 113 的块基准 248/928。
- 已定不做 / 不动：115 擦弹道具（TH18 原生有）、108 的 Power/Point、小蓝点新实体；奖命梯只记录；收点线 107/108 不补一手。
