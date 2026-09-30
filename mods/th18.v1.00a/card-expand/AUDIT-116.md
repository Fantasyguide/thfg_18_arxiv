# AUDIT-116 —— 系统卡 116（东方天空璋・季节系统）本轮留痕（2026-09-13）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）+ TH16 v1.00a / TH13 / TH12 / TH07 等按需引用。
> 本文地址一律带版本前缀（`th16:0x…` / `th18:0x…`）；**裸地址默认属 TH18 v1.00a**。

> **为什么单独一个文件**：`AUDIT.md` / `TUNING.md` 当时正被**另一条会话**改着（未提交），
> 按「只动自己的」纪律，本轮的 116 留痕写在这里，避免把别人的在改内容卷进我的提交。
> 等那些改动落库后，可以把本文件并回 `AUDIT.md`（或只在那边留一行索引）。
>
> 三条需求都来自用户（2026-09-13），并逐条过了 `CLAUDE.md` 的「系统卡六问」。

## §1 用户报的三条

> 1、th16 由**季节解放**出来的**季节点**应当是：**产生 —— 无敌上升 —— 自动回收**
> 2、th16 的**季节子机**的大小应当是**现在的两倍**，并且**应当有各自的动画**
> 3、季节解放之后，收齐产生的季节道具之后，会在收齐的那个点有一个 **`+…` 的 UI 显示**（用户确信没复刻）

## §2 六问逐条（第 4 问最关键）

- **问**：回答
- **1 原版是什么**：见 §3/§4/§5（全部落到 `th16:` 地址、脚本号与常量）
- **2 TH18 语义映射**：子机 = 世界层 anm VM（`Player::allocate_option` 的脚本号栈参）；季节点 = 本卡自建 VM 池；**没有重新发明机制**
- **3 TH18 有没有现成代码**：**有，而且是同一套**：`ability.anm` entry0 = `player/option/op00.png`（256×512，**16×16** 子机 sprite），其 **script2 / script16** 就是原生子机脚本（`-1: layer(13) blendMode(1) type(1)` / `0: interruptLabel(2) scale(0,0) scaleTime(10,4,1.4,1.4) sprite(sprite0) angleVel(0,0,0.31415927)` / `+10: scaleTime(6,1,0.7,0.7)`…）—— 与 TH16 `pl00sub` script1 **逐条同形**
- **4 ★ TH18 遗留能不能借**：**子机**：TH16 的五条配方**没**留在 TH18 的 anm 里，但**同形配方在**（上述 script2/16）；⇒ **借配方 + 借 TH16 贴图**（`local/` 只读切图），逐行搬成 5 支新脚本。**弹字**：**整套借** TH18 原生 adder `PopupManager__sub_4645f0` `th18:0x4645f0`，**没有自己写弹字**。⇒ 没有一处在需要借的地方自造
- **5 如何恰当实现**：能复刻的全复刻（脚本逐行、角度/速度/scale/层全用一手原值）；只在「TH16 有、TH18 没有这个道具/通道」处做**最小桥接**并标 ❓自定
- **6 落笔要求**：每处都标了 ✅复刻（给地址/脚本号）或 ❓自定（给理由），见 §3–§5；❓ 项见 §7

## §3 季节子机的尺寸：**「两倍」差在 scale 常量，不在贴图、也不在量纲换算**

- **原版 sprite 矩形 = 16×16**（`pl00sub` entry1 `sprite14=(32,0,16,16)` 等；**五季全是 16×16**）。
- **原版 scale（一手，逐季不同）**：

| 季 | 脚本（一手） | 出场 `scaleTime` | 落定 | 场上尺寸（ECL 单位）|
| --- | --- | --- | --- | --- |
| 春 | `pl00sub` script1 | `(10,4,0.8,0.8)` | `(6,1,0.6,0.6)` | 12.8 → 9.6 |
| 夏 | `pl02sub` script1 | `(10,4,1.0,1.0)` | `(6,1,0.6,0.6)` | 16 → 9.6 |
| 秋 | `pl03sub` script1 | `(10,4,1.0,1.0)` | 不缩 | **16** |
| 冬 | `pl01sub` **script18** | `(10,4,1.0,1.0)` | `(6,1,0.6,0.6)` + 15 帧 0.5↔0.6 呼吸 | 16 → 9.6 |
| 土用 | `pl04sub` script1 | `(10,4,0.8,0.8)` | `(6,1,0.6,0.6)` | 12.8 → 9.6 |

- **我们改前**：旧脚本 **113** 写的是 `scaleTime(10,4,0.42,0.42)`，**`0.42 = 27/64`** —— 那是**按占位贴图
  `SEASON_OPT.png`（64×64 程序生成球）**算的；而**实际显示的 sprite 是 16×16 的 `OPT_SPRING` 等**
  （`set_option_art()` 在 `ce_option_alloc` 建完 VM **之后**写，赢过脚本 init 段的 `sprite(@SEASON_OPT)`；
  一手 VM 语义：`create_managed_*` 建时就把 `zTimer` 置 0 并跑一次 run，`ins.time <= vm.time` 的闸门让
  `-1:` 与 `0:` 段**都在建的那一刻执行完**，见 `engine/anm/th16/02-vm-core.md` §8/§8.1）。
  ⇒ **改前真实尺寸 = 16 × 0.42 = 6.72 ECL 单位**；**改后** 春/土用出场 12.8 = **1.90×**、落定 9.6 = 1.43×；
  夏/秋/冬 16 = **2.38×** ⇒ **用户说的「两倍」完全对上**（0.42 ≈ 0.8/2）。
- **量纲核对（重要，别再想当然）**：世界层 VM 两作**同量纲** —— **1 sprite texel = 1 ECL 单位**
  （`engine/anm/th18/01-vm-instantiate.md` §3；子机脚本**没有** `resolutionMode` ⇒ 尺寸 ×1）；
  游玩区两作都是 384×448 单位 ⇒ **sprite 矩形 × scale 可直接比**。
  交叉验证：TH18 **原生**子机用同一张 16×16 表 + 同一套配方，scale **1.4 → 0.7**（= 22.4 → 11.2 单位）；
  我们的 0.42 连原生的一半都不到。
  ⇒ **只需改 scale**（现在由原版脚本原值带着走）⇒ **不需要换/放大贴图，也不涉及 640/表面换算**
  （那条只对 **UI 层** VM 成立，例如 116 自己的 HUD 用了 `resolutionMode(2)`）。

## §4 每季各自的动画（改前**一条都没有**：五季共用 script113 的「呼吸 alpha」）

| 季 | 一手脚本 | 帧数 | 关键指令 | 层 | 我们改前 |
| --- | --- | --- | --- | --- | --- |
| 春 | `pl00sub` script1 | 1 帧 | `angleVel(0,0,+0.31415927)` → `+6帧: angleVel(0,0,−0.10471976)`（自转 18°/帧 → −6°/帧）；`blendMode(1)→(0)` | `layer(13)` | ❌ 缺 |
| 夏 | `pl02sub` script1 | **4 帧循环**（sprite4→5→6→7，每 4 帧） | `+16: sprite4 … +28: sprite7; jmp(offset136,16)`；`type(3)`；scale 1.0→0.6 | `layer(13)` | ❌ 缺（连 sprite5/6/7 三张都没有）|
| 秋 | `pl03sub` script1 | **8 帧循环**（sprite4..11，每 4 帧，32 帧一轮） | `offset88: sprite4 … jmp(offset88,0)`；`type(0)+resampleMode(1)`；scale 恒 1.0 | `layer(13)` | ❌ 缺（连 sprite5..11 七张都没有）|
| 冬 | `pl01sub` **script18** | 1 帧 + 缩放呼吸 | `offset136: scaleTime(15,9,0.5,0.5) ↔ (15,9,0.6,0.6); jmp(offset136,16)`；`type(2)` | `layer(13)` | ❌ 缺 |
| 土用 | `pl04sub` script1 | 1 帧 | 同春（sprite6） | `layer(13)` | ❌ 缺 |

⇒ **缺的就是夏/秋那 10 帧贴图**，已全部补齐（切图见 §6）。

## §5 季节点与 `+…` 弹字

### 5.1 季节点（用户第 1 条）

| 环节 | 一手 | 我们（改后）| 定性 |
| --- | --- | --- | --- |
| **产生** | 季节解放消弹时**每颗被消的弹在原位置**生一个季节点：`th16:0x416bbb` 的 `ItemManager__spawn_item` 实参 —— 角度 `−π/2 + rand[0,10°]`（`0x49444c = 0.17453292`）、速度 **2.2**（`0x400ccccd`）| `cancel_spawn_points()`：**每颗被消的弹在弹的位置**撒 1 个（同角度/速度）；**删掉**了旧的 `SEASON_CANCEL_POINT_MAX 4`（❓自定闸）与「围着自机随机撒」的老退路 | ✅复刻 |
| **无敌上升** | 一手字段 = TH16 `player+0x1663c`；**春是每帧刷新到 50**（`th16:0x4116a0`），其余季 `BombSub*__begin` 只置 **10 帧** | 我们卡用的是 `season_core.c` 的 `k_invuln = {春: 持续+60, 夏 9, 秋 30, 冬 30, 土用 10}`，注释自标「**攻略口径**」= 🟡 **非一手**；**春的每帧刷新没复刻** | ❓**未改**（属另一件、且会动平衡）⇒ §7-3 |
| **自动回收** | 门 = 「自机在回收线上 ∨ (`BOMB+0x30 == 1` ∧ `BOMB+0x38 < 0x3c`) ∨ 对话中」 | 本卡**不用引擎的 bomb 对象** ⇒ `s->rel_frames > 0` 那条是**必需的桥**（代码注释已标 ❓自定）；另 `SEASON_POINT_INTANGIBLE 32`（出生 32 帧不吸不收）也是既有 ❓自定 | ❓自定（已标明）|

### 5.2 `+…` 弹字（用户第 3 条）—— 借 TH18 原生 adder，**没自己写弹字**

- **TH16 一手**：`ItemManager__on_tick` 的 **type 0x10 = ITEM_SEASON** 分支 `th16:0x42fe58`：
  `item_collect_season()`（`0x43de50`：季节槽 +1 封顶，**返回「档位跨了吗」**）⇒ **只有跨档那一颗**才：
  `repopulate_options()`（`0x4440e0`）+ **`PopupManager::add(item.pos, −1, 0xffffff40)`**（`0x42fe6d`–`0x42fe7b`）
  + 音效 `0x3f`；随后 `add_to_score(10)` + `Gui__update_season_gauge`，**总是** `play_sound(0x25)`。
  **位置 = 道具自己的坐标**（`lea esi,[edi+0xc08]`）。
  ⚠ **与用户措辞的差异**：一手是**按「档位跨越」触发**（随收随判），**不是**「收齐之后一次」——
  用户看到的「收齐那一下」是因为一批点把槽推过阈值，视觉上像收齐才弹。
- **TH18 侧现成机制 = 同一支**：`PopupManager__sub_4645f0` `th18:0x4645f0`，与 TH16 `0x44a460` **逐条同形**
  （ring 基址 `+0x624`、计数器 `+0x10` wrap@10、负值同样写字节 `0x0a`）。
  **交叉验证**：TH18 自己的**力量升档**分支就是 `PopupManager__sub_4645f0(&item.pos, −1, 0xffffff40)`
  ⇒ **同一配方、同一颜色**。
- **我们改前**：`grep -n "popup\|弹字" native/cards/season.c` **零命中** ⇒ 用户说的「根本没复刻」✅**属实**。
- **改后**：`point_collect()` 的**跨档分支**里调 `popup_level_up()`（`season.c`），
  **借原生 adder、不自造弹字**。ABI 实测：`th18:0x4646c8 c2 0c 00` = **`ret 0xc`（stdcall）**
  ⇒ 声明成 `__attribute__((stdcall))`（写成 cdecl 会**双重清栈**）；反汇编确认调用点
  `push color; push −1; push &pos; … call` 后面**没有** `add esp,0xc` ✓。
  弹字生命周期（约 60 帧、上飘、按距自机取 alpha）**全原生** ⇒ 无跨帧状态。

## §6 落地（逐条：文件 / 一手出处 / 定性）

- 处 ｜ 内容 ｜ 定性
- 脚本 **364–368** `season_opt_{spring,summer,fall,winter,doyou}` ｜ 与 TH16 零售脚本**逐行同形**（去空白/注释后 **diff 为空**，五条都验过），只把 `sprite` 换成 ORDER 占位名 ｜ ✅复刻
- 新增 10 帧贴图 `OPT_SUMMER_2..4` / `OPT_FALL_2..8` ｜ 生成器 `assets/ability/make_season_opt_anim_art.py`：矩形**从 anm 的 sprites 表读**；⚠ 顺手查清一个坑：**同批副 anm 的偏移约定不一致** —— `pl00sub/pl01sub/pl03sub/pl04sub` 的 entry1 sprite 坐标**要加** `xOffset/yOffset`，**`pl02sub` 不能加**（其坐标已是整页绝对值；`pl02.png` 只有 128×160，加了就读到页外）。生成器**不猜**：拿既有定稿的第 1 帧 PNG **逐像素比对**来判定约定，两个读法都不匹配就 `SystemExit` ｜ ✅复刻
- `season.c`：`season_opt_script()` + `ce_option_alloc(...)` 传逐季脚本号 ｜ 一手 `th16:0x492be0[SUBSEASON] = {1,1,1,0x12,1}`、`0x492cdc[SUBSEASON] = {pl00sub,pl02sub,pl03sub,pl01sub,pl04sub}` ｜ ✅复刻
- `refresh_options()` 改「全收 + 重申请」 ｜ 脚本号**只在申请时**传得进去（在役 VM 换不了脚本）；一手依据：TH16 换副季节发生在季节解放时，而 `Bomb__activate_bomb` 扣完槽**接着就调** `repopulate_options` ⇒ 原版换季本来就是**整批重建** ｜ ✅复刻
- 脚本号写死 → `#ifdef` + 生成宏 ｜ `anm_ids.h` 的 `CE_ANM_ABILITY_SCRIPT_SEASON_OPT_*`；回退形状照抄 `youkai.c`/`ufo.c` ｜ 工程性
- ❌ **没有** `zWriteDisable(1)` ｜ TH16 五支与 TH18 原生 script2/16 **都没有** —— 那是旧 script113 里我们自己加的 ｜ ✅去掉自造

- **对既有卡的影响面 = 零**：`season.c` 是 116 私有；新增脚本/贴图/ORDER 全是 append；
  **脚本 113 一个字节没动**（`grep SCRIPT_SEASON_OPT native/cards/season.c` 只剩注释）⇒
  108/111 借 113 的行为**零影响**；未动 `sdk*` / `engine.h` / `anm_ids.h`（生成物）/ `Makefile` / 别人的卡与资产。
- ⚠ **ORDER 合并与贴图不入库**：`ORDER.season_opt.txt` 的 10 行已追加进 `entries/ORDER.txt` 末尾（纯 append）。
  **这 10 张帧图属 `.gitignore:51` 覆盖的生成物**（`assets/ability/season/OPT_*.png`，与既有 `OPT_SPRING` 等**同一列**）
  ⇒ **不入库**；换机/新 clone 构建前先跑一次
  `python3 assets/ability/make_season_opt_anim_art.py`。entry 预算：并入前 **72** 个 ⇒ 并入后 **82**，离 256 余量很大。

## §7 ❓ 未定 / 需要用户拍

1. 🟡 **弹字的字形**：TH16 的 `−1` 经字符串系统 font 2 映到字形 `0x0a+0x94`；TH18 的弹字 draw 走**自己的三个基址**
   （`+0x121/+0x12c/+0x136`），`−1` ⇒ 字形 `0x12b = 299` = `ascii.anm` entry0 的 **`sprite299 = (80,248,48,8)`（48×8 宽条）**。
   ⚠ 本轮**没有读图能力**，只按引擎代码确定「画的是 299 号 sprite」，**没能肉眼确认它长什么样**。
   ⇒ **实跑看一眼**：若是怪东西，备选 = 传**本次加的分（10）**（画「10」）或传季节槽值（**要用户点头**，❓自定）。
2. **触发口径**：一手 = **每次跨档**弹一次（一次大解放可能连弹 2–3 次）。若用户要的是「收齐一批后只弹一次」，
   那是**新机制** ⇒ 请用户明确。
3. 🟡 **季节点「无敌」那半边**：一手 = TH16 `player+0x1663c`，**春每帧刷新到 50**（`th16:0x4116a0`）、
   其余季 `BombSub*__begin` 只置 **10 帧**；我们现用的是 `k_invuln = {春: 持续+60, 夏 9, 秋 30, 冬 30, 土用 10}`
   且注释自标「攻略口径」= **非一手**，**春的每帧刷新也没复刻**。
   **本轮没改**（属另一件、且会动平衡）⇒ 要不要按一手改。
4. 🟡 **能不能放炸弹**：TH16 有 `Bomb__initiate_season_release_cooldown`（45 帧冷却，见 `engine/player/th16/02`）；
   我们卡的门是「`rel_frames > 0` 就 `ce_refuse`」= 持续期禁放，**没有 45 帧冷却** ⇒ 未复查一手冷却是否作用于季节释放。
5. ⚠ **`type(3)`（夏）/`type(2)`（冬）/`type(0)+resampleMode(1)`（秋）** 是 TH16 原值、按「复刻」照抄，
   但**没有实跑确认** TH18 的同号 type 渲染路径与之等价。
6. ⚠ `cancel_spawn_points()` 的命中判据是按 `engine.h` 对 `0x429370` 的一手描述镜像的（**那条函数体未反汇编复核**）。
7. **建议加一个共享件**（本轮**没动** `sdk*`）：`sdk.h` 增
   `ce_popup_number(const float *pos, int32_t value, uint32_t color)`（= 调 `0x4645f0`，stdcall）。
   现成封装两个都不适用：`cards/collect2x.c` 的 `ce_popup_mult()` 写的是**倍率槽**（k13–17，只能画 ×倍率 / NO BONUS）；
   `cards/mof.c` 的是**别人卡里的 static**（拷一份 = 两套弹字）。⚠ `mof.c` 早就登记过**同一需求** ⇒ 一并解决最省事。

## §8 自检（本轮全绿）

- 新 PNG：**10 张全部 16×16（2 的幂）**、非全透明（可见像素 72–115）；生成器**连跑两次 md5 逐字节一致**；
  把 anm 目录指到不存在处 ⇒ `SystemExit`；**矩形从 anm sprites 表读**。
- 脚本：与 TH16 零售**去空白/注释后逐行 diff 为空**（五条）；`thanm -c 18 <spec> -m v8.anmm` 单编 **exit 0**（/tmp）。
- C：`-fsyntax-only` **零告警**；`objdump` **x87 = 0**；`sizeof(season_card_t)` = **192 B ≤ 496**。
- `test_season_core` **120 passed / 0 failed**；`make check` **exit 0**；`make verify` **exit 0**（210 条 binhack）；
  `test-host` 24 组全 0 failed；`check-docs` 全过。
- **`on_delete` 收尾**：本轮新增的东西**没有一件跨帧留在我们手里** —— `cancel_spawn_points()` 无状态；
  弹字写在**引擎自己的 PopupManager ring**（10 槽、约 60 帧自回收，**引擎托管**）⇒ 删卡时无需也无法由我们收（已在注释写明）。

## §9 副季节切换的绑定：改成「完整双击 Z」（用户 2026-09-13 口径）

**起因**：用户报「目前似乎不能切换季节」。定向排查发现旧门是
`(held & CE_INPUT_FOCUS) && (INPUT_PRESSED & 0x800)`（= **按住 Shift + D 的上升沿**），
且紧随其后有一段 **无条件**的 `if (held & CE_INPUT_FOCUS) INPUT_PRESSED &= ~0x800;`（抹掉 D 的上升沿，
目的是不让 D 去切主动卡）—— 形状上是个**死键**：只要切换那一支没走到，D 的边沿就已经被吃掉。

**用户随后澄清**：「我之前测试的时候忘了是 shift+D 了」⇒ **旧绑定其实是好的，之前那条不是 bug**
（是测试时忘了绑定）。但用户决定**改绑定**：

> 「算了，**改成双击 Z** 吧，**必须是完整的双击**：**不按 Z，按 Z，放开，再按**」

⇒ 新口径（**用户口径、不是复刻**，注释与本文都必须标 ❓自定）：
- 一次**完整的双击 Z** = **当前没按 Z → 按下 Z → 放开 Z → 再按下 Z**（**中间必须真的有「放开」**；
  「一直按着 Z」不算双击）⇒ 切一档副季节。
- 因此 **`SEASON_INPUT_CHANGE`（D, `0x800`）那道门与「抹 D 上升沿」那段一起删掉** ——
  后者只为旧门而存在，留着会导致**按着 Shift 时 D 被我们吃掉**，引擎自己的「切换主动卡」按不动。
- **双击窗口**（两次按下之间允许的最大间隔）用户没定 ⇒ 实现里取一个值（建议 ≈20 帧 / 330 ms）
  并标 **❓自定**，作为**一个可调常量**登记。
- ⚠ Z 同时是开火键 ⇒ 双击必然伴随两次短暂开火，**这正是用户要的**，不做规避。

## §10 副季节切换改成「完整双击 Z」—— 落地（2026-09-13 用户口径）

**已落地**（上一节 §9 是口径，本节是实装）：

- **Z 的输入位一手核实（三条独立证据）**：`INPUT_HELD`（`th18:0x4ca428`）**bit0 = `0x001`** ——
  装配链 `th18:0x401ca6` → `0x401cb1 shr eax,1` → `0x401cb3 or esi,eax`（键盘 profile 里 **Shot** 那条的 VK）；
  位表 `engine/input/th18/01-key-mapping.md` §2.3；**另直读 exe 的键盘 profile `th18:0x4ca470`**：
  `{Shot=0x5a 'Z', Bomb=0x58 'X', Slow=0x10(Shift), Item=0x43 'C', Change=0x44 'D'}`；
  同仓先例 `cards/pofv.c:63` 的 `POFV_INPUT_SHOT 0x001u /* Z（= 射击）*/`、`cards/animal.c:2608`。
- **状态机（纯逻辑，`season_core.{h,c}`，可离线单测）**：`空闲 → 第一次按下 → 松手 → 第二次按下` = 触发一次；
  **只在第二次按下那一帧返回 1**；按住不算；**触发后要等松手才重新武装**（不连发）。
  ★ **窗口从「第一次按下」起算**（不是从松手）：Z 是开火键、玩家长期按住，若从松手起算，
  「停火 → 再开火」就会误切季节 ⇒ 这就是「必须是完整双击」的实现。
- **窗口 = 8 帧**（`SEASON_DTAP_WINDOW`，唯一可直接调的手感常量）—— ★ 用户口径「**双击窗口参考 111 灵梦特技穿墙**」：
  即 **TH11 灵梦×紫的「缺口瞬移」二次窗口**（`th11:0x430e50` 状态机 `cmp DWORD PTR [ebx+0x924],0x8` / `jg` 超时归零
  ⇒ **8 帧**；见 `engine/input/th11/01-special-detection.md` §3）。⚠ **语义对齐点**：TH11 那个计数器也是
  **从第一次按下起算**（≥9 帧即作废）⇒ 与本实现同口径，所以是直接取 8、不需要换算。
- **删掉**（`grep` 自证零代码引用）：旧门 `SEASON_INPUT_CHANGE 0x800u`（D）与
  `(held & 0x008) && (INPUT_PRESSED & 0x800)`；**以及那段 `if (held & 0x008) INPUT_PRESSED &= ~0x800;`**
  —— 它只为旧门存在，且会让「按住 Shift 时引擎自己的 D（切换主动卡）」按不动；
  `on_tick_2` 的 `held` 局部变量。
  ⇒ 现在本卡 `INPUT_PRESSED` / `CE_INPUT_FOCUS` / `ce_key_down` **零代码引用**、**一个输入位都不写**，
  只读 `CE_INPUT_HELD() & 0x001`；`ce_refuse` 的三处门与 D/Shift 无关（逐条核过）。
- **切换动作不变**：`subseason = season_next()` → 音效 `0x4e` → `refresh_options()`（6 颗子机全收 + 重申请
  ⇒ 脚本/贴图/阵位一起换；HUD 季节字每帧按 `subseason` 重取）。**季节槽切换时不清**（🟡 见 §7）。
- ⚠ **如实报的副作用**：`refresh_options()` 会 `ce_option_drop` 6 颗子机（顺带收掉挂在冬子机上的 mode=2 激光）
  并重申请 ⇒ 旧子机外观/激光当帧重来一次（新子机重放出场动画 ~10 帧）；**开火节奏不重置**
  （`on_tick_shoors` 用本卡自己的 `frame`）、自机本体弹与输入帧**完全不受影响**。
- ⚠ **一手记录（不再据此改门）**：**TH16 根本没有局中切季节的键** —— 副季节在**菜单**里选
  （`MainMenu__do_subseason_select` `th16:0x450af0` 的四处写点），局内唯一的季节键是「季节释放」
  （`INPUT_RISING_EDGE & 0x800`）。⇒ 键位历来是**用户/owner 口径**（`Shift+C` → 2026-09-21 `Shift+D` → 本次**双击 Z**），
  **没有一手可复刻**。
- **自检**：`-fsyntax-only` 零告警 · **x87 = 0** · `sizeof(season_card_t)` = **196 ≤ 496**（旧 192 ⇒ +4 是 `dtap` 字段）·
  `test_season_core` **140 passed / 0 failed**（改前 120 ⇒ **+20 条断言**，覆盖：完整双击恰好 1 次且触发帧正确 /
  只按一下 / 一直按住 / 两次按下之间没放开 / 超窗 / 边界（还差 1 帧到期）/ 三连按 = 1 次、第四次共 2 次 / NULL 安全）·
  `make check` exit 0 · `make verify` exit 0 · `check-docs` 405 文件全过。

### §10.1 ⚠ 本轮**顺带发现、但没动**的另一条 bug（116 自己的，待裁）

`hud_slot_claim()` 与 `hud_ensure()` 读写的**不是同一个变量**：
`hud_slot_claim()` 只写**文件静态** `s_slot_ok`，而 `hud_ensure()` 查的是**卡字段** `s->hud_slot_ok`（**从没被写过**）
⇒ 两个后果：
1. **日志刷屏**：实跑日志里 `season: HUD block … claimed` 刷了 **7419 行**（每帧重认领）；
2. **HUD 位置错**：`hud_slot_sync()` 永远退回**默认锚点 `(257,924)`**，**从不读货架给它的槽位 `(521,924)`**。

⇒ 这**会改变 HUD 的实际位置**（属用户很在意的可见行为）。本轮**未动**（不在这一轮范围内）⇒ 另开一轮修。


### §10.2 ⚠ **订正**：§10.1 那条不是 bug，是**故意不走货架** —— 现按用户口径改回固定左下角

**用户 2026-09-13 订正**：「你之前找到的"bug"是**故意为之**，**目前暂时不搞货架排布**」；
并指出「`257,924` 反而还是**偏右**，查 git，很早的记录里应该是一个**更往左**的值；实在不行参考 117/112/113」。

**git 查证结果：116 从来没有更左的值** —— 从最早一版（`a7df52c`）起就是
`SEASON_GAUGE_BX 308 / BY 916`（⇒ 框 `x 263..365`），而 SDK 的 `CE_HUD_ANCHOR_X` 历史取值也只有 `257.0f`。
⇒ 按用户许可**参考同批三张卡**：
- **112**：`UFO_HUD_PIN_X = 248 + (−180) = 68`（`ufo.c` 注释里留着 owner 当年的两次「偏右」与那句订正：
  **表面 `x < 257` 那条带子不是黑底**，那是 TH18 自己的 HUD 面板区 —— 原生分数/残机/Bomb/当前卡都在那儿）；
- **117**：`ANIMAL_HUD_PIN_X = 88`（`y = 924`，**自带锚点** `ce_hud_slot_claim_pinned_card`，货架不动它）；
- **113**：`TRANCE_HUD_BASE_X = 248 / BASE_Y = 928`。

**改法**（`season.c`）：
- 新增 **`SEASON_HUD_PIN_X = 68.0f`**（= 112 的值 = **TH18 原生「当前卡」UI 的左沿**）、
  **`SEASON_HUD_PIN_Y = 924.0f`**（= 117 量过的那条底线 = `CE_HUD_ANCHOR_Y`）。
  ⇒ **只动 x**：外框仍落 `910..920`、竖直观感**一个像素不变**；块 = `(68, 924)` 起 **104×36** ⇒ `x 68..172`。
- 认领改走 **`ce_hud_slot_claim_pinned_card`**（`blk.side = CE_HUD_SIDE_PINNED`，照 117 的写法）：
  **登记进槽位表、但货架不会动它**，别的块也不会为它让位；关卡纪元照样清 ⇒ 每关 `on_load` 重认领。
- `hud_slot_sync()` **不再每帧 `ce_hud_slot_get()`**，直接用 `SEASON_HUD_PIN_*`。
- ⚠ **保留**（与「走不走货架」无关的真改进）：认领结果写进卡字段 `s->hud_slot_ok`
  （`hud_ensure` 的补认领门读的就是它），日志**只在结果变化时**打一行
  ⇒ 原来那个「每帧重认领 + 每帧一行日志（实跑 7419 行）」不再发生。
  ⚠ 我上一轮把「`s->hud_slot_ok` 恒 0」当成 bug 并据此去读货架槽位 —— **判断错了**，这一轮已按用户口径改回。
- **要再左/右移只改 `SEASON_HUD_PIN_X` 一个数**（68 ↔ 88 之间也可以）。

**症状**（实跑日志 + 代码）：`hud_slot_claim()` 只把认领结果写进**文件静态** `s_slot_ok`，而
`hud_slot_sync()` / `hud_ensure()` 读的是**卡字段** `s->hud_slot_ok`（**从没被写过** ⇒ 恒 0）：
1. **日志刷屏**：`hud_ensure()` 每帧都以为「还没认领」⇒ **每帧重认领**，实跑刷了 **7419 行**
   `season: HUD block … claimed`；
2. ★ **HUD 位置错**：`hud_slot_sync()` 永远走「没认领」那一支 ⇒ 一直用默认锚点 `CE_HUD_ANCHOR_*`（`257,924`），
   **从不 `ce_hud_slot_get()`** ⇒ **拿不到货架按 card_id 升序排给我们的槽位**（`521,924` 那种）。

**修法**（`season.c`，三处）：
- `hud_slot_claim(season_card_t *s)` **收卡指针**，把结果写进 `s->hud_slot_ok`（**唯一真值**）；
  `s_slot_ok` 保留，但只作「上一次的值」用于**日志去重**（`ok != s_slot_ok` 才打）⇒ 重认领不再刷屏。
- `hud_ensure()` 的补认领改成 `hud_slot_claim(s)`。
- `on_load()` 里**不再**把 `s_slot_ok` 清 0（留着才能让日志只在**结果变化**时打），并传卡指针
  （`season_card_t *s = st(c);`）。

**行为变化**：HUD 从此**真的读货架给的槽位**（`ce_hud_slot_get`）⇒ 与同批其它卡的排布一致；
§4.4 的「局中删卡不当场重排」语义**未动**（释放仍只走 `on_delete` → `ce_hud_slot_release`）。

**验收**：`make check` exit 0 · `make verify` exit 0 · `test-host` 24 组 0 failed ·
`test_season_core` **140 passed / 0 failed** · `-fsyntax-only` 零告警 · **x87 = 0**。


**验收（本轮）**：`make check` exit 0 · `make verify` exit 0 · `test-host` 24 组 0 failed ·
`test_season_core` 140 passed / 0 failed · `-fsyntax-only` 零告警 · **x87 = 0**。
