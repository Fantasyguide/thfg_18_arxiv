# 112 星莲船（`UFO`）交接文档

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。裸地址默认属该版本；引用其他版本带前缀（`th12:…`）。
> **姊妹文档**：108 永夜抄在 [`HANDOFF-108.md`](HANDOFF-108.md)。
> **逐条台账**在 [`SINGLE-CARD-TEST.md`](SINGLE-CARD-TEST.md) 的 `§th12` 段（#1..#23）；
> 一手过程在 [`AUDIT.md`](AUDIT.md) §BE/§BO；自定值在 [`TUNING.md`](TUNING.md)（§2.0zv / §2.0zw / §2.0zx / §2.0zz）。
> **本文只讲：现在什么状态 / 下一步 / 判据。**

## §0 一句话现状

**owner 迄今四批实跑反馈（12 条 + 7 条 + 2 条口径 + 3 条 + 3 条）全部实现并发版**：槽带位置（照 113 后再按 owner 左移）、
槽底毛笔横幅、token 音效、召唤清槽（不重现）、击破 **×N**（ASCII，含底下那行得分）、**吸收数记录面板**（owner 第 1 条要求关掉 ⇒ 已关）、
**召唤动画**（第 ② 轮回一手：`front.anm` script82/83/84 = ability.anm **324/325/326**；旧紫三角/六边形已停用）
+ 第 ④ 轮补的**闪电碎屑 + 中心洋红**（ability.anm **346..350**，一手 = TH18 自己的 etbreak 家族）、
**满 B**（第 ② 轮改成 **TH18 原生道具 type 7** + 一手卡面配方 + 背光 + 摇摆）、击破音 `se_enep02`。
**剩下的是「owner 眼判」**（见 §3①）：HUD 左移的量、满 B 的运动与外观、召唤大横幅与闪电/洋红的观感、×N 与得分行的观感。

### 部署面（单一来源 = `RELEASE-STATE.json`）

| 项 | 现值 |
| --- | --- |
| DLL | 见 `RELEASE-STATE.json` 的 `last_release.dll_md5`（modkit 四处同 md5；同步 = `local/sync_modkit_dll.sh`）|
| patch | 见 `RELEASE-STATE.json` 快照 |
| **ability.anm** | 见 `RELEASE-STATE.json` 快照 ← 2026-09-24 因 **召唤闪电碎屑（新图集 `ATLAS_UFO_DEBRIS` + 脚本 346..350）** 变过 |
| abcard.anm / sht | 见 `RELEASE-STATE.json` 快照 |

> **部署面审计（2026-09-23 全量核对通过）**：DLL 两处同 md5 且 = `RELEASE-STATE` 的 `last_release.dll_md5`；
> `ability.anm` / `abcard.anm` / 四份 `pl0X.anm` / 四份 `pl0X.sht`（`build/sht/`）**逐个 md5 与 `native/build/` 相同**；
> `voice/` 11 颗 wav 在场。⇒「实跑看到的是不是我发的版本」用 `md5sum` 对一次即可。

## §1 怎么干（每批固定动作）

1. **闸三件**：`cd native && make check`（**rc=0**）、`python3 tooling/check-docs.py`（全绿）、
   `make dllx87`（**0 条**）。⚠ 文档表格**单元格 ≤ 200 字符**。
2. **改了行为（含只改 anm/sht 资产）必须发版**：`make release MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1`
   ⇒ 它会把 `dist/patch-step3/*.anm`、`build/sht/pl0X.sht` 一起同步进 modkit；
   再 `DLL_SRC=$PWD/native/th18_card_expand.dll bash /home/nipporita/renkolab/local/sync_modkit_dll.sh`
   （必须打印 4 条同 md5），最后一起提交 `RELEASE-STATE.json` 与手改文档。
3. **实跑日志**：`/mnt/d/项目/幻想指南/18/th18_modkit/game/th18_card_expand.log`；本机铁律见 `local/GAME-LOG.md`
   （**用户说「跑了 / 看到 X」，先自己读日志尾巴**）；按 `=== th18_card_expand <时间> ===` 先切出本次。
4. **改 `assets/ability/**`（含 ORDER 与脚本）算改共享件** ⇒ 在 `AUDIT.md` §BE 登记。
5. **不加新断点站点**（预算 31）。
6. **调试键位**（`UFO_DEBUG_KEY`）见 §2 末行与 `TUNING.md` §2.0m：空格 = 令牌 / Shift+空格 = 爆点 / Ctrl+空格 = 变色令牌。

## §2 已完成（逐条：做了什么 + 怎么验的）

| 条 | 做了什么 | 怎么验的 |
| --- | --- | --- |
| **#21 槽带位置** | 锚点改成 **113 的块基准 `(248, 928)`** ⇒ 槽心 **272/314/356**（块 248..380 / 896..928）| ✅ **实跑已验**：`ufo: HUD up … pinned at surface (248, 928) … aligned to card 113's block` |
| **#17 槽底毛笔横幅** | 生成器加 `BANNER_APPEAR`（TH12 `front00.png` (336,368,96×16)，与槽图**同一张 sheet**）+ ability.anm **315**；`ufo_hud_ensure()` 先起、`ufo_hud_kill()` 收 | ✅ **实跑已验**：`ufo: brush banner under the slots up (ability.anm script 315 <= front.anm script79, sprite67 …)` |
| **#18 token 生成音效** | 新 SE 通道项（构建期从 TH12 派生 wav）| `ufo/se tally …` 里有对应计数 |
| **#19 召唤清槽** | 代码侧本来就是一手口径（唯一清槽点 = **UFO 结束**那一刻）| ✅ **实跑已验（不重现）**：那局无 `ufo: slots cleared`，而 tally 有 `ufo-fill-full 2 / ufo-break 2` |
| **#20 击破 ×N** | `ufo.c` 新增 `ufo_xn_spawn/tick/kill`：UI 层字形通道拼「×」+整数倍率、红 `0xffff6060` + 黑描边错开 (1,1)、寿命 **720 帧**（一手 `0x2d0`）| 待眼判：`ufo: xN popup up - mult %d.%03d -> val N, M glyph(s) …` |
| **#14 紫环** | ability.anm **316/317/318**（逐条照抄 TH12 `enemy.anm` script139/140/141，`sprite(-1)`+`drawPoly` ⇒ **无贴图**）| 待眼判：`ufo: summon purple ring up …` |
| **#14 六边形（雷电候选）** | ability.anm **319/320/321**（`enemy.anm` script103 + 102/104 逐条照抄）| 待眼判（**这就是问题**：像不像「雷电」）|
| **#22 吸收数记录面板** | `ufo.c` 新增 `ufo_panel_pop/digits/tick/kill`：弹 **304/305**（一手 `pos(528,288)`）+ 字形画吸收数、静置 180 帧后中断 1 缩回；吸收数变化时**冷却 20 帧**内只原地刷数字 | 待实跑：`ufo: absorb-count panel popped …` |
| **调试键（2026-09-23 晚，owner 要求）** | 键位：**空格** = 1 枚固定色令牌 · **Shift+空格** = **爆一批 P 点 + 金钱点**（8×6 网格、两色交错、围**场上的 UFO**；无 UFO 围自机）· **Ctrl+空格** = 1 枚变色令牌（原 Shift 那件搬过来）| 日志 `ufo: DEBUG item burst by SHIFT+SPACE - 24 P + 24 money around the UFO (…)`；自定值见 `TUNING.md` §2.0m 末段 |
| **owner 第二轮 7 条 + 2 条口径（2026-09-23 更深夜）** | ① 收槽闪光只留 alpha 闪 · ② 令牌槽与计时文字**左移 1/4 游玩区半径** · ③ 召唤动画回一手（324/325/326）· ④ ×N/爆闪挪到 UFO 本体 · ⑤ **满 B 改成原生道具 `type 7`** + 卡面换一手配方与 `BOMB_max.png` · ⑥ 击破音 `0x1d` · ⑦ 信仰点吸满也触发收满 | 逐条判据见 `SINGLE-CARD-TEST.md`「本批（2026-09-23 更深夜）」+ §本批-2/3/5；分类见 `TUNING.md` §2.0zzb |
| **owner 12 条实跑反馈（2026-09-23 深夜）** | 12 条：面板关掉 · ×N 改回 ASCII · 吸收/收碟静音 · 吸满音 = `se_cardget` · 满 B 换本作完整 B 卡面 · **召唤不再吃光场上 token** · 召唤特效真的起来 + 新音效槽 `se_ufo` · 对话 token 直飞 · `se_piyo` 事件号修好 · UFO 在场持续 `se_ufoalert` · 击破爆炸特效 | 逐条判据（含日志关键字）见 [`SINGLE-CARD-TEST.md`](SINGLE-CARD-TEST.md) 的「本批（2026-09-23 深夜）」表；自定值与「复刻 / owner 口径」的分类见 [`TUNING.md`](TUNING.md) §2.0zza |
| **owner 第 8 轮 3 条（2026-09-24）** | ① 拉拽改**匀速 3.0**（压过落体末速 2.0）· ② 出生无敌 = **不做**（一手就没有） · ③ 击破音 **`se_enep02`** · ④ ×N 底下补**得分行** | 见 `SINGLE-CARD-TEST.md` §本批8-1/8-2 + `TUNING.md` §2.0zze（行 0/1a/1b/2b/2d/2e/6）|
| **owner 第 9 轮 3 条（2026-09-24）：召唤闪电 + 中心洋红** | 一手 = TH18 自己的 `bullet.anm` etbreak 家族 ⇒ 新脚本 **346**（驱动器）/ **347+348**（蓝色暗层+亮层）/ **349+350**（中心洋红）+ 图集 `ATLAS_UFO_DEBRIS`；层 **5/6** ⇒ 中心被飞碟盖住；`ufo.c` 加收尾 `ufo_summon_fx_kill()` | 见 `SINGLE-CARD-TEST.md` §本批9-1 + `TUNING.md` §2.0zzl；`AUDIT.md` §BE #86 |
| **owner 第 16 轮（2026-09-24）：妖精皮切错图 + 波分组被间隔拆开** | 妖精皮从 `sprite84..87`（= 大蝴蝶图的象限**碎片**）改成 `sprite36..39`（= 完整小妖精那一块的**第 4 行/黄族**，一手 script15）；波分组判据去掉 `\|\| 间隔`（有波号只看波号）⇒ 不再把一波拆成几波乱挑宿主 | 见 `TUNING.md` §2.0zzs + `SINGLE-CARD-TEST.md` §本批16-1；⚠ **ability.anm 与 DLL 同批发布** |
| **owner 第 15 轮（2026-09-24）：波 = ECL 自己的波计数** | 「一波」的判定从"到达间隔 ≤20 帧"（我编的启发式）改成 **ECL 的 `MainSub` 编号**（读关卡脚本的当前子程序名，`01b/01c` 同波；照 115/117 的读数链复制进本卡）；读不到才退回启发式 | 见 `TUNING.md` §2.0zzr + `SINGLE-CARD-TEST.md` §本批15-1 |
| **owner 第 14 轮（2026-09-24）：中妖精整族不是宿主 + 每波只挑 1~2 只** | 宿主表剔掉整个中妖精家族（48px，不换皮不掉，15→11 行）；新增**每波挑选**（同 (N,S)、间隔 ≤20 帧算一波 ⇒ 只挑**队列最后那只 + 它的镜像**）；`UFO_TOKEN_MAX` 3→6、宿主/boss 不再吃节流 | 见 `TUNING.md` §2.0zzq + `SINGLE-CARD-TEST.md` §本批14-1 |
| **owner 第 13 轮（2026-09-24）：杂鱼妖精判据 + 原始精灵 32×32** | 判据补第二条：**宿主原始精灵必须 32×32**（两条都满足才是杂鱼妖精皮）；不满足的走大蝴蝶皮，并按宿主原始尺寸**等比缩放**（48px ⇒ ×0.75）| 见 `TUNING.md` §2.0zzp + `SINGLE-CARD-TEST.md` §本批13-1 |
| **owner 第 12 轮（2026-09-24）：宿主分档 = 血量 + 补上真正的杂鱼妖精** | 分档判据"精灵尺寸 → **血量**"（`min(hp) ≤ 400` = 一碰就碎 ⇒ 杂鱼妖精皮）；按 owner 点名的 TH18 一面第二波把 **`(2,0)/(2,5)` 红蓝妖精族**补进宿主表（原先不在表里 ⇒ 那波掉 0 枚）| 见 `SINGLE-CARD-TEST.md` §本批12-1 + `TUNING.md` §2.0zzo；**本轮只改 DLL + 表** |
| **owner 第 11 轮（2026-09-24）：令牌掉落统一「两个 + 会变的 + 随机」** | 所有宿主与 boss 一律掉 **2 枚**、起始色随机 + 会变色（`UFO_CARRIER_TOKENS_DROP = 2`；旧的"按关固定色 / 妖精1蝴蝶2 / boss 红绿交替"全退役）| 见 `TUNING.md` §2.0zzn + `SINGLE-CARD-TEST.md` §本批11-1；**本轮只改 DLL** |
| **owner 第 10 轮（2026-09-24）：谁能掉飞碟 + 给宿主换皮** | 掉落判据换成**宿主表**（`cards/ufo_carrier_table.h`：`(N,S)` 命中 TH18 妖精/幻影族才掉，boss 每相位 1 红 + 1 绿）；宿主**换 TH12 皮**（脚本 351..355 + 3 张新图集）+ **隐掉原生贴图并还原** | 见 `SINGLE-CARD-TEST.md` §本批10-1（**含待你过目的宿主表**）+ `TUNING.md` §2.0zzm；`AUDIT.md` §BE #87 |

## §3 待办

### ① owner 眼判（当前唯一卡点）

1. ~~**#14 的「雷电」**（紫三角 + 青六边形候选）~~ ⇒ **已作废**：owner 第二轮直接判「召唤动画是紫色三角（不对）」，
   而一手 `th12:0x44a8a0`（`Ufo::spawn`）建的是 `front.anm` **script82/83/84** 的大横幅 ⇒ 本批换成 324/325/326，
   紫三角与青六边形**停用**（文件保留、不再起）。**这一条不再等眼判**（见 `SINGLE-CARD-TEST.md` §本批-3）。
2. **观感（当前卡点）**：① 令牌槽与计时文字**左移的量对不对**（owner 口径 −96 表面 px；槽组左沿 144 vs 场地左沿 257
   —— 空槽是纯黑框，**看一眼"空槽还在不在"**）② 满 B 那颗（原生绿整 B 图标 + 我们叠的卡面）**运动/收点**像不像原版
   ③ 召唤大横幅的观感 ④ ×N 弹字与底下得分行的位置/颜色 ⑤ **召唤闪电碎屑**（第 9 轮新加：数量 25 颗、飞出去 88 px、
   蓝色 + 中心一点洋红、整批压在飞碟贴图下面）—— 密度/远近/颜色都在 `TUNING.md` §2.0zzl 里各有一行可调。
3. **若某项不对**：把日志里对应的关键字行发我（每条都有），或直接说「哪里怪」—— 三个左移常量与满 B 的载体
   （原生 type 7 / 自建）各有一行可回退。

### ② #23 召唤大横幅 —— ✅ **本批已做**（owner 第 ③ 条把它提前了；下面是当时的一手全文，留档）

**一手全文**（`local/th12.v1.00b/anm/front/front.anm.txt`）：
```
script82 { scriptNew(83); sprite(sprite68); layer(6); type(2); pos(224,208); blendMode(1);
           alpha(0); alphaTime(20,0,128);
         +30: scaleTime(30,2,2,2); alphaTime(30,0,0);
         +35/+40/+45: scriptNew(84) ×3;   +75: delete();
           中断 7/8/9/10 = color(255,128,128)/(128,128,255)/(128,255,128)/(255,255,128)+colorTime }
script83 { 滚动条：sprite69(32×32)、pos(88,208)→posTime(30,0,360,208)、scrollX(0.0296875)、+30 delete }
script84 { 重复爆闪：sprite68、scaleTime(30,2,2,2)、alphaTime(30,0,0)、+30 delete }
```
★ **横幅颜色由中断 7/8/9/10 决定**（按 UFO 的红/蓝/绿/彩虹）；`script84` 里 8/9 的蓝绿与 82 相反（一手如此）。

**实现地图（已侦察，别重做）**：贴图 = `front02.png` 的 `sprite68`(0,0,**304×32**)+`sprite69`(0,0,32×32)
⇒ **要新建图集**（现有 `ATLAS_UFO_HUD` = 256×192 塞不下 304 宽；建议 `ATLAS_UFO_BANNER 336x32`）。
✅ `make_atlas.py` **完全不碰** UFO 图集（grep 无命中）⇒ 不会撞几何断言；
⚠ 但生成器是**单图集假设**：`ATLAS_NAME/ATLAS_W/ATLAS_H/ATLAS_POS`（185–188 行）、
拼片段的 `ORDER_FRAG.write_text(...)`（369–374 行，只写一条 `ATLAS` 行）、`make_sprites()` 三处都要改。

**为什么暂缓**：① 它不是 owner 报的（优先级低）；② 这条链改错会把**已验证过的 112 HUD（槽/横幅/面板）一起搞坏**
—— 用没人要的 extras 去冒退化已验证功能的风险不划算。等 owner 反馈清完或明确要求时再做。

## §4 各件的实现要点与一手依据（照这个改，别重推）

| 件 | 一手出处 | 实现要点 |
| --- | --- | --- |
| #20 ×N | `th12:0x44ab00`–`0x44ab5f`（`push $0x88` → 句柄 `+0x44`；兄弟 = 脚本 134）+ `front.anm` script136/137 | ⑧ **没有「移植了但没接」**：全部卡侧脚本宏在 `ufo.c` 的引用已逐个核过（0 引用那几支都是 `scriptNew` 子脚本或一手不用的） |
| #14 紫环 | `enemy.anm` **139/140/141** | `drawPoly(192,3)` + `color2(255,128,255)`（紫）+ `angleVel(±0.0982)`；141 = 发射器（3 对 ⇒ 6 环）；层 **7**（世界层）；**无贴图** |
| #14 六边形 | `enemy.anm` **102/104/103** | `drawPoly(96,6)` + `color2(0,128,128)`（青）+ 固定 `rotate(π/2)`；103 = 发射器（3× 102）⇒ 层 **7**、无贴图 |
| #22 面板 | `front.anm` **134/135** | 面板位 `pos(528,288)`（脚本自带）+ `scale(0,0)`→放大 + 中断 1 = 缩回；数字走 #20 同套字形通道；触发 = `cnt_p+cnt_money` 变化 |
| 槽/横幅 | `front.anm` script80/81（槽/闪光）、script79（毛笔横幅）| 槽位置由 C 每帧补差值（Δ7 自校准）；四态 = 中断 10..13；亮版 = 基址 +4 |

## §5 资产 / 产物怎么读（别用土办法）

| 想读什么 | 怎么做 |
| --- | --- |
| 编出来的 anm | `thanm -l 8 -m v8.anmm native/build/ability.anm`（改前/改后逐句比）|
| sht 的组/行 | **`make sht` 自带的对账**（`妖侧弹脚本号逐文件对账 OK …`）；**别**自己按 `base 0x180/0x4e0` 猜着扫（踩过）|
| 部署面 | `md5sum` modkit 里的 `pl0X.anm/pl0X.sht` 与 `native/build/` 对 |
| 切图/认形状 | 用 PIL 从 `<anm>/<entry>.png` 按 anm dump 的 sprite 表裁（本轮就是这么确认 sprite380 的）|
| 脚本号 | 由构建侧定：`assets/ability/scripts/` 文件名前缀必须 = `68 + 排序序号`（`make anm` 报「编号应为 N」就以它为准）|
| ORDER | 生成器只写**片段**（如 `entries/ORDER.ufo_hud.txt`），要**手写拼进** `entries/ORDER.txt`；没拼会报 `未定义的占位 @…` |

**关键位置**：卡主体 `native/cards/ufo.c`（+ `ufo_core.{h,c}`）；HUD 生成器 `assets/ability/make_ufo_hud_art.py`；
本体/令牌生成器 `make_ufo_art.py` / `make_ufo_token_art.py`；ORDER 片段 `assets/ability/entries/ORDER.ufo_hud.txt`；
台账 `SINGLE-CARD-TEST.md` §th12。

**已定口径**：槽带 = 113 的 `248/928` 基准、槽心 `272/314/356`；token 收取音保持一手 `0x0c`（owner 已拍）。

## §6 坑

1. 文档表格单元格 ≤ 200 字符（`make check` 会红）。
2. **不加新断点站点**（预算 31）。
3. **改 anm 资产也要发版**（否则游戏里看不到）；`make anm` 会重编 `ability.anm`/`pl0X.anm`。
4. 跨版本比脚本先确认是哪一份 anm（本轮踩过：拿 `etama.anm` 的同号脚本比 `enemy.anm` 的）。
5. **「移植了但没接」是这类卡的常见坑**（#22 面板就挂了很久）⇒ 新移植一件就问一句「C 侧真的引用它了吗」（grep 宏名）。
6. **状态别落后于代码**：已修的要立刻改台账状态列，否则下一轮会被当新 bug 重报。
