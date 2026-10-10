# 108 · 使魔本体与使魔魔法阵的尺寸：TH08 一手 vs 我们（只读核对）

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th08:0x41affe`、`th18:0x481210`）。
> **方法**：一手 —— ① 两侧 anm 直读（`thanm -l/-x`，命令见 §6，可复算）；② TH08 反编译
> `local/vendor/th08_dec/src/`（`EclManager.cpp` / `EnemyManager.cpp` / `GameManager.cpp` / `AnmManager.cpp`）
> 与 `local/_scratch/c108/th08.asm` 逐指令；③ 用 PIL 做**逐像素**比对 + alpha 包围盒测量。
> **可信度**：两侧「贴图尺寸 / 脚本 / scale / 逐像素内容」✅；「TH08 场地 1:1 上屏」✅（viewport + 绘制路径）；
> 🟡 见 §7（`th08:0xf54e0c` 的归属、TH08 贴图上传 `scaleFactor`、owner 说的到底是哪一类使魔）。
> **被核的我们**：`native/cards/youkai.c` md5 `abdb401c486940cf1b3522f35969c183`（9590 行）；
> `native/build/ability.anm` md5 `5858726adc4b9bb54cc8187a6a10fec0` —— 与**出货的**
> `dist/patch-step3/th18/ability.anm` 逐字节相同（所以「现在画出来的」就是下面这些数）。

## 0. 结论

1. **不比 TH08 小。** 本体：两侧都是 `32×32` 贴图、`scale 1.0`、场上 **32 单位**（比值 **1.00**）；
   魔法阵环：两侧都是同一份 `62×62` 环图、同 `scale 0.5 / 0.8`、外径 **31 / 49.6 单位**（比值 **1.00**）。
   两侧**不是「差不多」而是逐像素同图**：本体 32/32 张、环 1/1 张（比对命令与结果见 §3.3 / §4.2）。
2. 差异**不是**贴图尺寸、**不是**缩放、**不是**分辨率换算（§5 逐条排除）：两作的场地都是 384×448、
   都占窗口宽 **60%**，场上 1 单位都是「场地宽的 1/384」；TH18 只是把同一套世界单位整体 ×2 上屏
   （位置与尺寸同步放大）⇒ **一切比值不变**。按窗口宽占比：本体两侧都 **5.00%**，环两侧都 **7.75%**。
3. **唯一一处可见的不对称**（一手，代码事实）：**妖侧（低速/聚焦）我们只画环 —— 本体 `alpha=0`**
   （`youkai.c:1551` / `:2155`），而 TH08 **人妖两侧都画本体**、只换环的中断（`th08:0x41affe` 建时 /
   `th08:0x42c282`、`th08:0x42c322` Shift 时）。
   ⇒ TH08 的环里始终有 **24 px 的实体芯**，我们的妖侧只有空心环。owner「看着比我们大」若真成立，
   **来源是这一条，不是尺寸**（是否就是 owner 看到的那一条 = 🟡，见 §7）。
4. 前人记录里那句「我们的环同 scale 大 ~3.2%」（[TUNING.md](TUNING.md) 第 9158 行、[AUDIT.md](AUDIT.md) §BE #155）
   **对本卡的可见尺寸不成立**：那 3.2% 只落在**透明画布**（64 vs 62），环的内容是 62×62 **居中补 1 px 边**
   （实测 alpha 包围盒 `(1,1,63,63)`）⇒ 同 scale 画出来的**环外径一字不差**。见 §4.3。
5. 所以：**尺寸不用改**；要动的是「妖侧本体要不要可见」那一条（1 行，且它是**复刻 TH08**，不是自定）。
   若 owner 仍要「比 TH08 更大」，§8 给了具体倍数与像素值（全部标 ❓自定）。

## 1. 尺寸对照表（使魔本体 / 魔法阵环 各一行）

| 对象 | 侧 | 脚本（出处） | 原图尺寸 | scale | 最终（世界单位） | 占场地宽（384） |
| --- | --- | --- | --- | --- | --- | --- |
| 使魔本体 | TH08 | `enemy.anm` script **53..58**（`th08:0x419812` 装载） | **32×32**（可见 24×24） | **1.0** | **32** | **8.33%** |
| 使魔本体 | 我们 | `ability.anm` script **206..209**（`youkai.c:1509` / `:2140`） | **32×32**（逐像素同图） | **1.0** | **32** | **8.33%** |
| 魔法阵环 | TH08 | `etama.anm` script **48**（模板 `th08:0x4c6eb0`） | **62×62**（sprite197） | **0.5 / 0.8** | **31 / 49.6** | **8.1% / 12.9%** |
| 魔法阵环 | 我们 | `ability.anm` script **309**（`youkai.c:1560` / `:2177`） | 64 画布（**内容 62×62 同图**） | **0.5 / 0.8** | **31 / 49.6** | 同 |

- 屏幕像素（用于换算）：TH08 640×480 窗口、场地 `(32,16,384,448)` 视口、**绘制路径不加缩放**
  ⇒ **1 世界单位 = 1 屏像素** ⇒ 本体 32 px、环 49.6 / 31 px。
  TH18 场地 `(640 + x×2, 32 + y×2)`（1280×960 表面，一手 + 实跑钉死）
  ⇒ 本体 64 表面 px、环 99.2 / 62 表面 px。**占窗口宽：本体 32/640 = 64/1280 = 5.00%；环 49.6/640 = 99.2/1280 = 7.75%。**
- 比值：本体 **1.00**；环外径 **1.00**（画布 64/62 = 1.032，全在透明边里，不可见）。

## 2. TH08 侧一手

1. **使魔是谁**：ECL opcode 90/91/92 → `th08:0x41af5b` / `th08:0x41b10b` / `th08:0x41b2bb` →
   `th08:0x41f110` / `th08:0x41f280` → `EnemyManager::create_enemy` `th08:0x42a680`。它是**普通敌人**。
2. **外观脚本**：使魔自己的 ECL 子程序里 `anmSet(K)`（opcode 54，handler `th08:0x419812`）→
   `enemy.anm`（共享敌机图集）script 53..58（调用点加权 53×347 / 57×165 / 54×162 / 56×48 / 58×46 / 55×22）。
3. **贴图尺寸（本次现dump）**：`enemy.anm` entry0 `data/stgenm/enemy.png` 512×512；
   sprite**69..100**（x 256..480，y 128/160/192/224）与 sprite**101..108**（x 128..224，y 128/160）
   **全部 32×32**；可见内容 alpha 包围盒最大 **24×24**（例 sprite69 `(5,5,29,29)`）。
4. **没有缩放**（三条独立证据）：① script 53..58 正文**只有 `sprite()` 与 `jmp()`**（57/58 多一条
   `ins_25(1)`，只写不读的死存）——脚本里没有任何 `scale`；② `AnmVm` 构造把 scale 置 **1.0**
   （`AnmManager.cpp:418-419`）；③ ECL 侧没有 scale 类 opcode（`th08.eclm` 全表无 scale/size 助记符），
   而全库只有 `EnemyManager.cpp:965-1113`（`aiFlags` 拖尾路径）会写 `primaryVm.prefix.scale`。
5. **魔法阵环的身份（一手，本次直读 .data）**：`familiarCreate*` 生成后内联
   `th08:0x41afc0`–`th08:0x41afd9`：`push -1 / push 1 / push &enemy+0x2d34 / push 0x20` →
   `ecx = th08:0x4ece60`（`g_EffectManager`）→ `call th08:0x425970`（= `AllocEffectSlot`），
   句柄存 `enemy+0x53c8`；紧接着 `th08:0x41affe call th08:0x406f20`（`SetInterrupt`），
   参数 = `IsYoukai() ? 2 : 1`（`th08:0x41afe2` 读 `g_Player = th08:0x17d5ef8`，`call th08:0x40ba40`）。
   模板表 `th08:0x4c6d30` 步长 0xc：本次读 `th08:0x4c6eb0`（= `[0x20]`）**field0 = 48**
   ⇒ **`etama.anm` script 48**；该脚本唯一引用 `sprite197`，实读矩形 `(193,113,62,62)`
   （在 entry1 `data/etama/etama2.png`），`blendAdditiveSet(1)` 加法、`angleVel(0,0,-0.05235988)`
   = **−3°/帧**自转；中断 1 = 紫 α255 **scale 0.5**（人侧）、中断 2 = 蓝 α200 **scale 0.8**（妖侧）、
   中断 3 = 缩到 0 后 `delete()`。`AllocEffectSlot` 的第 3 实参 `1` 经逐指令核对是**重试次数**（`th08:0x425b12`），
   **不是缩放**；第 4 实参 `-1` 落 `p+0x1f0`（`spawnParam`）。
6. **场地与上屏**：`arcadeRegionTopLeftPos = (32,16)`、`arcadeRegionSize = (384,448)`
   （`GameManager.cpp:1642-1645`），并被直接当 D3D **viewport** 用（`GameManager.cpp:376-379`）；
   敌机绘制只做 `pos + g_PlayerPos`（=`arcadeRegionTopLeftPos`，`EnemyManager.cpp` OnDrawImpl）
   ⇒ **1 世界单位 = 1 屏幕像素**，窗口 640×480、场地占宽 384/640 = **60%**。

## 3. 我们侧一手

### 3.1 映射层壳（发射口使魔）与绕转使魔：走同一套素材、同一套代码

1. **本体**：`ability.anm` script **206..209**（宏 `CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53..S56`，
   `native/anm_ids.h:1513-1516`），来源文件
   [206_youkai_familiar_s53.anm.txt](assets/ability/scripts/206_youkai_familiar_s53.anm.txt) 等四支
   —— 正文逐条照抄 TH08 `enemy.anm` script 53..56，**没有 `scale()` / `layer()` / `alpha()`**。
   贴图：`ability.anm` entry37 `ability/atlas/ATLAS_YK_FAM.png`（128×256，ORDER `ATLAS_YK_FAM 128x256@4x8`），
   sprite **289..320** 每个 **32×32**。生成器 [make_youkai_familiar_art.py](assets/ability/make_youkai_familiar_art.py)。
2. **挂载点**：映射层 `youkai.c:1509`、绕转使魔 `youkai.c:2140` 都是
   `ce_anm_spawn(CE_ABILITY_ANM(), script, YK_FAM_ANM_LAYER)`，`YK_FAM_ANM_LAYER = 7`（`youkai.c:1338`）。
3. **层 7 ⇒ 尺寸 ×1**：TH18 `set_layer`（`th18:0x4066b0`）对层 3..19 只置 `flags_hi` bit20，
   **不碰 `resolutionMode` 掩码**（bit22–24），而 `draw_vm`（`th18:0x4817c6` 一族，见 01-vm-instantiate §3c）按该掩码分支
   = 0 时**尺寸 ×1**；世界层「1 art px = 1 ECL 单位」（[01-vm-instantiate.md](../../../engine/anm/th18/01-vm-instantiate.md) §3b/§3c）。

### 3.2 魔法阵环

1. **脚本**：`ability.anm` script **309**（宏 `CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING`，
   `native/anm_ids.h:1616`），来源 [309_youkai_familiar_ring.anm.txt](assets/ability/scripts/309_youkai_familiar_ring.anm.txt)
   —— 逐条照抄 TH08 `etama.anm` script48（唯一翻译：v8 无 `blendAdditiveSet` ⇒ `blendMode(1)`；补一行 `type(1)`）。
2. **贴图**：entry65 `ability/YOUKAI_FAM_RING.png` 64×64，sprite483 = `(0,0,64,64)`
   （ORDER `YOUKAI_FAM_RING … 64x64`）；`lowResScale: 0`（不会被折半）。
3. **挂载点**：映射层 `youkai.c:1560` + 换侧发中断 `youkai.c:1566`（`side ? 2 : 1`）；
   绕转使魔 `youkai.c:2177` + `youkai.c:2195`（`youkai_side ? 2 : 1`）。**两侧常驻**，换侧只换中断。
4. **妖侧本体不可见**：`youkai.c:1551`（`side ? 0x00ffffffu : 0xffffffffu`）与
   `youkai.c:2155`（`youkai_side ? …`）——`0x00ffffffu` 的 A 字节 = 0 ⇒ 本体全透（VM 不删）。

### 3.3 逐像素比对（本次实测，不是「差不多」）

把出货 anm 的贴图解出来、与 TH08 的原图逐格比（命令见 §6）：

| 比对 | 结果 |
| --- | --- |
| 本体：我们 `ATLAS_YK_FAM` sprite289..320 vs TH08 `enemy.anm` sprite69..100 | **32/32 张逐字节相同** |
| 环：我们 `YOUKAI_FAM_RING.png` 的 62×62 内容（偏移 1,1）vs TH08 `etama.anm` sprite197 | **逐字节相同**（内容 alpha 包围盒 `(1,1,63,63)`） |
| 妖子机（顺带）：[ORDER.youkai_option.txt](assets/ability/entries/ORDER.youkai_option.txt) 的 `YOUKAI_OPT_YUKARI_S22/S25/S30` vs TH08 `player00.anm` sprite22/25/30 | **逐字节相同**（30×46 / 46×46 内容居中补到 32×64 / 64×64） |

## 4. 结论：是不是更小、差多少、差异来自哪里

1. **不是更小；比值 1.00（本体）/ 1.00（环外径）。** 两边的贴图**同图**、脚本**同正文**、VM scale **同值**。
2. **不是贴图尺寸**：本体两边都是 32×32；环两边都是同一张 62×62（我们只是在画布上补了 1 px 透明边）。
3. **不是缩放**：本体 scale 1.0（TH08 `AnmVm` 默认 + 脚本无 scale；我们脚本无 scale + 层 7 不派生放大）；
   环 scale 0.5/0.8 两边逐字相同。**我们没有任何一处额外缩小。**
4. **不是分辨率换算**：两作场地都是 384×448、都占窗口宽 60%、场上单位都是场地宽的 1/384；
   TH18 的 ×2 只是**同一套世界单位整体上屏**（位置与尺寸同步）⇒ 比值恒等（§1 的 5.00% / 7.75% 两列相同）。
5. **差的是「画面里有没有实体」**：妖侧我们只画环（本体 alpha 0），TH08 两侧都画本体 ⇒
   TH08 的合成视觉是「24 px 实体 + 49.6 px 环」，我们是「只有 49.6 px 空心环」。
6. 附带订正：前后两次记录（[TUNING.md](TUNING.md) 第 9158 行、[AUDIT.md](AUDIT.md) §BE #155）里
   「同 scale 大 ~3.2% / 要乘 62÷64 对齐」的说法**只对透明画布成立**，对可见尺寸不成立 —— 内容已逐像素相同。

## 5. 建议（只给数值与出处，**不实现**）

| 想要的效果 | 改哪里 | 具体数值 | 性质 |
| --- | --- | --- | --- |
| **保持现状（推荐）** | 不动 | 本体 32 单位、环 49.6（妖）/ 31（人）单位 | ✅ **复刻一手**（逐像素同图同 scale，§2/§3） |
| 让**妖侧观感**与 TH08 一致 | `youkai.c:1551`、`:2155` 的 `0x00ffffffu` → `0xffffffffu`（2 行） | 本体恢复可见（24 px 芯）＋环 49.6 | ✅ **复刻一手**（TH08 人妖两侧都画本体：Shift 分支只对环发中断 `th08:0x42c282`/`th08:0x42c322`，不动本体 sprite/alpha）⚠ 与 owner 早前「按 Shift 变成魔法阵」的 standing spec 冲突 ⇒ **要 owner 拍板** |
| 只想让**环**更大 | [309_youkai_familiar_ring.anm.txt](assets/ability/scripts/309_youkai_familiar_ring.anm.txt) 的两处 `scaleTime(8,0,0.5f/0.8f)` | 外径 = **62k** 单位（`k=1.0` → 62 = 16.1% 场地宽；`k=1.25` → 77.5） | ❓ **自定**（破坏「逐条照抄」，要重生成 ability.anm + 登记 [TUNING.md](TUNING.md)） |
| 只想让**本体**更大 | 脚本 206..209 加一行 `scale(k,k)`，或在 `youkai.c:1509`/`:2140` 后 `ce_anm_set_scale(id,k,k)` | 外廓 = **32k** 单位；要让本体可见芯（24）与环外径（49.6）齐平 ⇒ `k ≈ 2.07`（32×2.07 = 66） | ❓ **自定**（同上；注意 CE 侧设了 scale 后**别**再让脚本/引擎覆盖，本卡脚本不写 scale ⇒ 不会被覆盖） |
| 逐像素对齐环的**画布**（可选、无可见收益） | 同上一格的 script309 | `0.5 → 0.484`、`0.8 → 0.775`（= 0.5×62/64、0.8×62/64） | ❓ **自定**（只让透明画布与 TH08 等宽；环外径本来就已相同） |

换算口径（给「改成多少 px」用）：**1 世界单位 = 1 屏像素（TH08）／= 2 表面像素（TH18 @1280×960，@960×720 为 1.5、@640×480 为 1）**；
场地宽恒 384 单位。例：本体 k=1.5 ⇒ 48 单位 = 场地宽 12.5% = 96 表面 px（1280×960）。

## 6. 复现命令（两侧都可重算）

```bash
T=local/vendor/thtk/build/thanm/thanm
M2=local/vendor/thpages/static/mapfile/v2.anmm     # TH08 anm 是 v3 ⇒ 必须用 v2.anmm
M8=local/vendor/thpages/static/mapfile/v8.anmm
$T -l 8 -m $M2 local/th08.v1.00d/dat/enemy.anm      # script53..58 + sprite69..108 的矩形
$T -l 8 -m $M2 local/th08.v1.00d/dat/etama.anm      # script48 + sprite197 = (193,113,62,62)
$T -l 8 -m $M2 local/th08.v1.00d/dat/player00.anm   # 妖子机 script18 + sprite22..30（30×46 / 46×46）
$T -l 18 -m $M8 mods/th18.v1.00a/card-expand/native/build/ability.anm   # script206..209 / 309 + sprite483
$T -x 18 mods/th18.v1.00a/card-expand/native/build/ability.anm && $T -x 8 -m $M2 local/th08.v1.00d/dat/enemy.anm
# 逐像素比对（PIL）：ATLAS_YK_FAM 的 sprite289+k vs TH08 enemy.png 的 sprite(69+k) ⇒ 32/32 相同；
#   YOUKAI_FAM_RING 的 (1,1,63,63) vs etama2.png 的 (193,113,193+62,113+62) ⇒ 相同
```

## 7. 🟡 与缺口（缺什么、怎么补）

1. 🟡 **`th08:0xf54e0c` 是哪个 .anm**（使魔外观图集的归属）：按「K 上界 95 ⇒ 只可能是 `enemy.anm`(101 支)
   或 `etama.anm`(115 支)」+「写的是敌机本体 VM」判为 `enemy.anm`，**没有找到写入点**。补法：反查
   `th08:0xf54e0c` 的写点（`EnemyManager` 装载处），或在 Ghidra 里看该全局的 xref。**不影响本报告结论**
   （两种候选的 32×32 那一批 sprite 是同一套编号段，且尺寸一致）。
2. 🟡 **TH08 贴图上传倍率**：`AnmManager::SetSprite` 里还有一条
   `scaleFactor = 上传面宽 ÷ entry 宽`（`AnmManager.cpp:2109-2110`，640×480 正常上传时为 1.0），本次**未实测**。
   若它真 < 1.0，TH08 的一切都会**更小**（与 owner 的方向相反）⇒ 不改变「我们不更小」这一结论。
3. 🟡 **owner 说的「使魔」是哪一类**：本报告主体按任务口径 = **敌方使魔**（`familiarCreate*`）。
   若 owner 指的是**自机的妖子机（option）或本体 Shift 光环**，§8 也逐项对过 —— 同样 1.00。
   **补法**：请 owner 指认截图/场景（哪一面、什么怪、看到的是「实心小怪 + 环」还是「只有环」）。
4. **没有「找不到」的对象**：两侧的贴图、脚本、scale 都拿到了（TH08 的 `enemy.anm`/`etama.anm`/`player00.anm`
   无需再解包；`thanm -x` 直接出图）。若将来某个 th08 图集没解出来，命令同 §6 的 `-l/-x`。

## 8. 顺带：本卡其它「使魔类」VM 的尺寸（口径确认用）

| 对象 | TH08 一手 | 我们 | 比值 |
| --- | --- | --- | --- |
| 妖子机（option，四机体） | `player00.anm` script18 sprite22..30 = **30×46** / **46×46**，scale 收在 **1.0** | script 224..227，`YOUKAI_OPT_*` 画布 32×64 / 64×64（**内容同图**），scale 收在 1.0 | **1.00** |
| 使魔拖尾火花 | `etama.anm` script71（模板 `th08:0x4c6ef8`）sprite219 = **16×16**，scale 1.1–2.1 随机 | script 370 `YOUKAI_FAM_TRAIL` **16×16**（我们只把 alpha/color 提亮过 = ❓自定） | **1.00**（尺寸） |
| 本体 Shift 光环 | `etama.anm` script54（模板 `th08:0x4c6e38`，field4 = `th08:0x426c40`）sprite218 = **64×64** | script 312 `YOUKAI_FAM_AURA` **64×64** | **1.00** |

⚠ 本表只核**尺寸**；妖子机的 `ins_83(2)` 省略、中断重编号、拖尾亮度提亮那些**行为/观感**差异另见
[TUNING.md](TUNING.md) 与 [AUDIT.md](AUDIT.md)，不在本文范围。
