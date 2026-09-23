# 112 星莲船（`UFO`）交接文档 —— 接手从这里开始

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。裸地址默认属该版本；
> 引用其他版本带前缀（`th12:…`）。
> **姊妹文档**：108 永夜抄在 [`HANDOFF-108.md`](HANDOFF-108.md)（两张卡已拆给不同的人/agent）。
> 逐条台账在 [`SINGLE-CARD-TEST.md`](SINGLE-CARD-TEST.md) 的 `§th12` 段，本文只留「状态 / 下一步 / 判据」。

## §0 一句话现状

**HUD 那条线做完了**（槽位位置、槽底毛笔横幅、音效、召唤清槽都对并且实跑验过）；
**两件美术都已实现并发版**：#20 击破的 **×N**（`ufo_xn_*`，UI 层字形）与 #14 的**紫环**
（`ability.anm` 316/317/318，逐条照抄 TH12 `enemy.anm` script139/140/141）⇒ 都只差 owner 实跑眼判。
**真正还缺的只有一件**：#14 的**雷电**那一半 —— 一手还没找到（见 §3② 的候选与「问一句形状」）。

### 部署面（单一来源 = `RELEASE-STATE.json`）

| 项 | 现值 |
| --- | --- |
| DLL | `0d55e6bd215aba92f8d4a7225557a57d`（modkit 四处同 md5；`RELEASE-STATE.json` 为准）|
| patch | `df14d5707689ca4b66cdfab2b0f642b3` |
| **ability.anm** | `058cec74…` ← **2026-09-23 因 #17 毛笔横幅变过**（旧值 `f06ba70d…`）|
| abcard.anm | `bf91a0f5…`；四份 `pl0X.sht` 见快照 |


> **★ 部署面审计（2026-09-23，本轮全量核对通过）**：`native/th18_card_expand.dll` 与 modkit 两处**同 md5** 且 = `RELEASE-STATE.json` 的 `last_release.dll_md5`（`65f78eab…`）；`ability.anm` / `abcard.anm` / 四份 `pl0X.anm` / 四份 `pl0X.sht` **逐个 md5 与 `native/build/`（含 `build/sht/`）相同**；`voice/` 11 颗 wav 在场。⇒ 「实跑看到的是不是我发的版本」这类疑问不用再猜：用 `md5sum` 对一次即可。

## §1 怎么干（每批固定动作）

1. **闸三件**：`cd native && make check`（**rc=0**）、`python3 tooling/check-docs.py`（全绿）、`make dllx87`（**0 条**）。
2. **改了行为必须发版**：`make release MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1`，
   再 `DLL_SRC=$PWD/native/th18_card_expand.dll bash /home/nipporita/renkolab/local/sync_modkit_dll.sh`
   （必须打印 4 条同 md5），最后一起提交 `RELEASE-STATE.json` 与手改的文档。
   ⚠ 只改 anm 资产也要发版（`make release` 会把 `dist/patch-step3/*.anm` 一起同步进 modkit）。
3. **实跑日志**：`/mnt/d/项目/幻想指南/18/th18_modkit/game/th18_card_expand.log`。
   本机铁律（`local/GAME-LOG.md`）：**用户说「跑了 / 看到 X」，先自己读日志尾巴，别问用户**；
   按 `=== th18_card_expand <时间> ===` **先切出本次**再看。
4. **共享件**（`assets/ability/**` 的 ORDER / 脚本、`sdk.*`、`sites.py`、`Makefile`）改动要在 `AUDIT.md` §BE 登记。

## §2 已完成（逐条给「做了什么 + 怎么验的」）

| 条 | 做了什么 | 怎么验的 |
| --- | --- | --- |
| #21 槽带位置 | 锚点改成 **113 的块基准 `(248, 928)`**（`trance.c` 的 `TRANCE_HUD_BASE_X/Y`）⇒ 槽心 **272/314/356**（块 248..380 / 896..928）| 日志 `ufo: HUD up … pinned at surface (248, 928) … aligned to card 113's block` ✅ 已验。一手也定了案：TH12 那个 anm 空间 = 640 屏幕空间 **x 方向 +96**（该文件把场地中心写成 `pos(224,240)`，而屏幕空间下该写 `(320,240)`，**全文件没有 320**）|
| #17 槽底毛笔横幅 | 生成器加 `BANNER_APPEAR`（TH12 `front00.png` (336,368,96×16)，与槽图**同一张 sheet**）+ ability.anm **script315**；`ufo.c` 的 `ufo_hud_ensure()` **先**起（同层 25 ⇒ 画在槽下面）、`ufo_hud_kill()` 一并收 | 日志 `ufo: brush banner under the slots up (ability.anm script 315 <= front.anm script79, sprite67 …)` ✅ VM 已起（观感待眼判）|
| #18 token 生成音效 | 新 SE 通道项（构建期从 TH12 派生 wav，`assets/voice/ORDER.txt` + `derive_retail_se.py`）| `ufo/se tally …` 里应有对应计数 |
| #19 召唤清槽 | 代码侧本来就是一手口径（唯一的清槽点 = **UFO 结束**那一刻）| 那一局**无** `ufo: slots cleared`，而 tally 有 `ufo-fill-full 2 / ufo-break 2`（确实召唤过两轮）✅ 不重现 |
| 槽四态 / 闪光 | 槽 VM 每帧写 sprite（`457 + 四态 + 闪烁×4`）、入槽闪光走 flash VM（script 303）| 日志 `ufo: slot 1 flash (state 3) - ability.anm script 303 + slot interrupt 7/13` ✅ |

## §2b 2026-09-23 后新增（都已实现发版，等 owner 眼判）

| 条 | 做了什么 |
| --- | --- |
| #20 ×N | `ufo.c` 新增 `ufo_xn_spawn/tick/kill`：UI 层字形通道拼「×」+整数倍率，红 `0xffff6060` + 黑描边错开 (1,1)、寿命 **720 帧**（一手 `0x2d0`）；关键字 `ufo: xN popup up …` |
| #14 紫环 | `ability.anm` **316/317/318**（逐条照抄 TH12 `enemy.anm` script139/140/141，**无贴图**）⇒ 召唤时 6 个反向旋转的紫多边形；关键字 `ufo: summon purple ring up …` |
| #14 六边形（雷电候选）| `ability.anm` **319/320/321**（`enemy.anm` script103 + 102/104 逐条照抄）⇒ 召唤时青六边形爆闪；**待你眼判「这是不是雷电」** |
| #22 吸收数记录面板 | `ufo.c` 新增 `ufo_panel_pop/tick/kill`：弹 304/305（一手 `pos(528,288)`）+ 字形画吸收数、静置 180 帧后中断 1 缩回；5 个 ❓自定登记 `TUNING §2.0zz`；关键字 `ufo: absorb-count panel popped …` |

## §3 待办

> **2026-09-23 更新**：① **#20 已实现并发版**（DLL `7abf08c7…`）⇒ 下面是规格存档 + 实跑判据；
> ② **#14 的紫环已实现并发版**（DLL `dad6b621…`，ability.anm `2e57e4cf…`）⇒ 只剩**雷电**未找到。

### ① #20 击破 UFO 的 ×N —— **已实现（2026-09-23，DLL `7abf08c7…`）；下面是规格与判据**

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
- **我们的实现要点**：用 112 现有 HUD 字形通道拼「×」+ 数字（**不新造贴图**，`ability.anm` 已有
  `SCRIPT_HUD_DIGIT` / `SCRIPT_HUD_GLYPH` 一族与 `ATLAS_HUD_DIGIT`）；颜色 `(255,96,96)` + 黑描边错开 `(1,1)`；
  寿命 **720 帧**；生成时机 = **击破 / 结算那一刻**（`ufo.c` 的结算路径同一处）；位置每帧写。
- **★ 落地所需的三个名字（2026-09-23 本轮查好，写码时不用再找）**：
  · **乘数从哪来**：`native/cards/ufo.c` 的开奖那一段 —— `mult = ufo_drop_mult_permille(color, …)`（**千分比**，
    例 `1500` = ×1.5），日志行 `ufo: PACKAGE opened - color %d, cntA %d + cntB %d, mult %d.%03d, PV %d -> score %d`
    就在 `~ufo.c:2635` ⇒ **生成时机 = 这一处**（owner 说的「击破 UFO 时」= 开奖那一刻）。
  · **× 字形**：`SIGNAL_MUL` = `ability/faith_hud/MUL.png`（16×32，共享资产，`ORDER.txt:555`）⇒ 直接用。
  · **数字**：`ATLAS ATLAS_HUD_DIGIT 128x96@4x3`（`ORDER.txt:50`）= 0..9 的图集 + 现成的 HUD 数字脚本族。
- **★ 用哪个脚本（2026-09-23 已反汇编确认，不必再查）**：`CE_ANM_ABILITY_SCRIPT_HUD_GLYPH`（= 136）=
  `sprite(119) + layer(25) + resolutionMode(2) + pos(0,0,0) + stop()`，中断 1 = `delete()` ⇒
  **它是 UI 列表脚本**（层 25、**表面 px**）⇒ #20 **只能走 UI 层**（不能走世界层）。
  调用形状：`ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_HUD_GLYPH, pos_surface)`（第 3 参就是初位置）
  → 每支 VM 用 `ce_anm_set_sprite(anm, id, …)` 换成「×」(`CE_ANM_ABILITY_SPRITE_SIGNAL_MUL` = 430)
  或数字（`CE_ANM_ABILITY_SPRITE_ATLAS_HUD_DIGIT_0`..`_9` = 109..118）→ 每帧 `ce_anm_set_pos` / `ce_anm_set_color`。
  **世界 → 表面**：`surface_x = 640 + world_x*2`、`surface_y = 480 + world_y*2`（居中系，换算口径见 `trance.c` 的注释）。
- **两条实现路径（挑一条）**：① **世界层**（推荐）：照 108 的 `yk_popup_*` 先例，用 `ce_anm_spawn` 在世界层起
  「×」+ 每位数字各一支 VM，位置 = 开奖时那只 UFO 的世界坐标（每帧写、随它动），颜色 `0xFFFF6060`，
  再叠一支黑色副本错开 `(1,1)` 做描边；② UI 层：`ce_anm_spawn_ui` + 把 UFO 的世界坐标换算成表面 px
  （`640 + x*2` / `480 + y*2`，见 `trance.c` 的换算注释）—— 少一层跟随逻辑但要做换算。
- **验收关键字**：实现时自己定一行（例 `ufo: xN popup up …`）+ 数字与颜色对不对由 owner 眼判。

### ② #14 召唤时的紫环 + 雷电 —— 紫环**已实现**（DLL `dad6b621…`）；**只剩雷电**

**紫环（已做完）**：一手 = `enemy.anm` **script141**（发射器）+ **139/140**（一对反向旋转的紫多边形）⇒
逐条移植成 `ability.anm` **316 / 317 / 318**（`sprite(-1)` + `drawPoly` ⇒ **不需要任何贴图/图集**），
层保持 **7**（世界层）；`ufo.c` 的 `ufo_summon_ring_try()` 在召唤那一刻起一支、每只 UFO 只一次。
产物已核验（`script316` 11 条指令一条不少；⚠ v8 助记符：`drawPoly` = **ins_604**、`color2` = **ins_406**，
用名字 grep 产物会落空）。实跑关键字 `ufo: summon purple ring up …`。

**雷电（仍缺一手，但已有强候选）**：`enemy.anm` 的多边形特效有**两族** —— 139/140（紫，三角环，已做）与
**102/104（青 `color2(0,128,128)`，`drawPoly(96,6)` 六边形）**，后者 + 发射器 **103**（3× `scriptNew(102)`）
**尚未实现**，很可能就是 owner 说的「雷电」。⇒ 实现方式与紫环**完全同形**（`sprite(-1)` + `drawPoly`，不需要贴图）。
见下面保留的排查记录。

**已排除**：
- ① 召唤的 C 路径（`th12:0x44a8xx`–`0x44a97x`）里**只建一支 VM** = `push $0x52`（脚本 82 = **大横幅**，
  `front02.png` 304×32、`layer(6)`、`pos(224,208)`、`scriptNew(83)`）。
- ② **不是靠 `color` 染的** —— 扫全部 TH12 anm dump，紫色系只有 `bullet.anm` 的纯品红 `(255,0,255)`
  ⇒ 紫环是**贴图**（或颜色由代码后写 `+0x524`）。
- ③ 候选①**已降级**：`0x436622` 建的是 `bullet.anm` **script77**（`sprite380` + `scale 0.3→1.0` +
  `rotate(−π)→rotateTime(π)` + `angleVel`）；把 `bullet/etama2.png` 的 (0,16,64×64) 切出来看过 ——
  是**红/粉色的圆环 + 放射状尖刺**（形状对、**颜色不对**），且紧接着是**逐档读表算分** ⇒ 更像「**收令牌那一圈**」。

**方法（通用，本轮验过好用）**：
```bash
objdump -d local/th12.v1.00b/th12.exe > /tmp/th12.asm      # 约 205782 行
# 只认 call 0x4615a0（anm 实例化），脚本号 = 它前面最近的 push $0xNN
```
TH12 v1.00b 全 exe 共 **142 处**创建点。已查过的：

| 地址 | 脚本 | 结论 |
| --- | --- | --- |
| `0x421371` | `0x55` | ❌ 紧接**分数运算**（`imul $0xf4240`、`cmp $0x3b9aca00`、写 `0x4b0c44`）+ 逐槽读表 ⇒ **收令牌 / 算分**那条路 |
| `0x421619` | `0x51` | ❌ 建完 `lea 0x7(%ebx),%edx` = **中断 7+格号** ⇒ **槽 VM 创建**（脚本 81 = 槽闪光，已知）|
| `0x4314e3` / `0x431593` | `0x51` / `0x52` | 大横幅 82 的另一处调用点 |
| `0x436622` | `0x4d` | ⚠ 候选①已降级（见上）|
| `0x438429` | `0x50` | **未查** |
| `0x448ab1` / `0x448adf` | `0x63` / `0x75` | **未查**（`0x447xxx`–`0x448xxx` = 结算 / 分数族）|
| `0x44a96e` | `0x52` | ✅ 召唤 C 路径唯一那一支 = 大横幅 |

**★★ 方法修正（2026-09-23，别再踩）**：全 exe 的 anm 实例化入口**有两个** —— `call 0x4615a0`（142 处）与
`call 0x477420`（**192 处**）。§ 里那份「142 处清单」**只覆盖前者**；紫环的直建点 `0x41c9f0`
（`push $0x8c` = 脚本 140 + `call 0x477420`，句柄存全局 `0x4b43e0`）就是被漏掉的那一类。
⇒ 找雷电时**两个入口都要扫**（本轮已补扫 `0x477420` 的 0x80..0x9f：只有 `0x47448c → 144`，也不是雷电）。

**下一轮找雷电的候选**：① TH12 的 `stgenm*.anm` 等文件里找「锯齿/竖条」形状；② `effect.anm`；
③ **问 owner 一句形状**（竖劈的锯齿？环上冒的电火花？）—— 最省的一步；④ 也可能就是 `drawPoly` 那几个多边形的尖角观感。

**紫环已实现**（ability.anm 316/317/318，见 §2 与 `TUNING.md` §2.0zw）⇒ 本条只剩「雷电」。

**旧的下一步**：`0x438429` 与 `0x448xxx` 那两处；**UFO 本体脚本的 `scriptNew` 子链**
（本体贴图来自 TH12 `enemy.anm` **script135** —— 见 `assets/ability/scripts/164_ufo_red.anm.txt` 文件头，
所以**别只在 `front.anm` 里找**）；以及「颜色由代码后写 VM（`+0x524`）」这一可能。
切图认形状：PIL 从 `<anm>/<entry>.png` 按 anm dump 的 sprite 表裁（本轮就这么确认 sprite380 的）。

## §4 资产 / 产物怎么读（别用土办法）

| 想读什么 | 怎么做 |
| --- | --- |
| 编出来的 anm 长什么样 | `thanm -l 8 -m v8.anmm native/build/pl00.anm`（改前 / 改后逐句比）|
| sht 的组/行对不对 | **`make sht` 自带的对账**（`妖侧弹脚本号逐文件对账 OK …`）；**别**自己按 `base 0x180/0x4e0` 猜着扫（踩过，读出过假数据）|
| 部署面是不是本地产物 | `md5sum` modkit 里的 `pl0X.anm/pl0X.sht` 与 `native/build/` 对 |
| 112 的 HUD 资产从哪来 | `assets/ability/make_ufo_hud_art.py`（槽/闪光/面板/毛笔横幅，全从 TH12 `front.anm` 切图 + 端口脚本）；本体/令牌 = `make_ufo_art.py` / `make_ufo_token_art.py` |
| 脚本号 | 由构建侧定：`assets/ability/scripts/` 的文件名前缀必须是 `68 + 排序序号`（`make anm` 报「编号应为 N」就以它为准）|
| ORDER | 生成器只写**片段**（如 `entries/ORDER.ufo_hud.txt`），要**手写拼进** `entries/ORDER.txt`；没拼会报 `未定义的占位 @…` |

## §5 与 112 相关的关键位置

| 事 | 位置 |
| --- | --- |
| 卡主体（召唤 / 令牌 / 槽 / 结算 / 音效）| `native/cards/ufo.c`（约 4000 行）+ `native/cards/ufo_core.{h,c}` |
| HUD 槽与横幅 | `ufo.c` 的 `ufo_hud_*`（`UFO_HUD_SCRIPT_SLOT/FLASH/BANNER`、`k_ufo_slot_x[]`、`UFO_HUD_PIN_X`) |
| HUD 生成器 | `assets/ability/make_ufo_hud_art.py`（含 Δ1..Δ7 全部移植改动与一手出处）|
| 台账 | `SINGLE-CARD-TEST.md` §th12；一手过程在 `AUDIT.md` §BE / §BO |
| 已定口径 | 槽 = 113 的 248/928 基准、槽心 272/314/356；token 收取音保持一手 `0x0c`（owner 已拍）|

## §6 坑

1. 文档表格**单元格 ≤ 200 字符**（超了 `make check` 直接红）。
2. **不加新断点站点**（预算 31）。
3. 改 anm 资产要跑 `make anm`（会重编 `ability.anm` / `pl0X.anm`）并**发版**，否则游戏里看不到。
4. 「部署面 = 本地产物」要显式核一次（本会话踩过：日志里的旧现象其实是旧产物）。
5. **状态别落后于代码**：已修的要立刻把 `SINGLE-CARD-TEST.md` 的状态列改掉，否则下一轮会被当新 bug 重报。
