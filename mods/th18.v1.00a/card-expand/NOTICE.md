# NOTICE —— 素材来源 · 版权 · 瑕疵声明

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。文中引别作的地址一律带 `thNN:` 前缀。
> **适用**：`mods/th18.v1.00a/card-expand/`（TH18 卡表扩容 mod）的**发布包**与**仓库内素材**。
> **整理**：2026-10-19。owner 口径：「从其他东方作里面切过来的图像需要加一个声明是从其他作品里面切过来的，
> 原则上有版权瑕疵，但社区意义上问题不大」。
> ★ 本文是**来源登记 + 版权口径**，不是法律意见；★ 每一条都能顺着「证据」一栏自己复核。

---

## 0. 一句话

本 mod 的**代码 / 脚本 / 排版 / 程序生成**部分是我们的（§1）。
**相当一部分图像与音效素材切自其他东方 Project 作品**，版权属**上海アリス幻樂団 / ZUN**，
**我们不主张任何权利**（§2、§5）；另有少量**非东方商业作品**素材（§4）。
这些都属于**社区惯例下的二次使用**，★ **存在版权瑕疵**；★ **权利方要求即立即移除**（§8）。

---

## 1. 属于我们的部分（自绘 / 程序生成 / 代码）

这一栏是**版权上干净**的部分：由本仓脚本**按参数或几何画出来**，不读任何原版像素，或由我们手写。

| 类别 | 具体 | 证据 |
| --- | --- | --- |
| 代码 / 数据 | 全部 C / 汇编 cave / binhack / ECL / JSON 卡表 | `native/`、`patch/`、`assets/ecl/` |
| 卡框 | 原生卡的框**按量出来的参数程序合成**（仓库里不放原生像素） | [`assets/cardframe.py`](assets/cardframe.py)（头注：外 3 px 黑 → 13 px 斜面 → 内 2 px 黑，画面区 220×284） |
| 占位卡面 | 白底 + 花色/点数矢量绘制 | [`assets/gen_placeholder.py`](assets/gen_placeholder.py) |
| 横幅文字 | 「ROYAL FLUSH」等金字横幅 = 字体渲染 + 金属渐变 + 描边烘焙 | [`assets/ability/gen_banner.py`](assets/ability/gen_banner.py) |
| 几何特效 | 红环 / 蓄力标记 / 结界环等**按几何画**（`drawPoly` 一族），不做切图 | `assets/ability/make_pofv109_c_ring_art.py`、`make_pofv109_chg_mark_art.py`、`make_pofv109_barrier_ring_art.py`、`make_flan_ring_art.py` |
| 合成音 | 皇家同花顺等**纯合成** wav（stdlib 合成，无采样） | [`assets/voice/make_melodies.py`](assets/voice/make_melodies.py)、`assets/voice/ORDER.txt` 头注 |
| 用户原创图 / 语音 | 用户手绘或提供的整图、立绘、语音 ogg | [`assets/cards/_src/README.md`](assets/cards/_src/README.md)、[`assets/voice/_src/README.md`](assets/voice/_src/README.md) |

⚠ 注意区分：`assets/ability/make_*_art.py` 这个命名里**既有生成器也有切图器** ——
判定标准只有一条：**脚本里有没有 `SRC = … local/thNN…`**。有的就是切图（§2），没有的才是生成（本表）。

---

## 2. ★ 跨作切图清单（核心）

**做法**：把这些作品解包后的 PNG/anm 按矩形**1:1 裁原版像素**（不重画、不改色、不重采样），
存成 `assets/**` 里的中间 PNG，构建期再编进 `ability.anm` / `pl0X.anm` / `pl0X.sht` / `abcard.anm`。
**源字节只在 `local/`（gitignored）；入库的是派生件。**

| 来源作 | 切了什么（源资源） | 用在本 mod 哪里 | 证据（文件:行） |
| --- | --- | --- | --- |
| TH07 妖妖梦 | `etama.anm` → `etama2.png`（樱点/小樱点）、`etama4.png`（森罗结界） | **107**（PCB_CHERRY）樱点 / 结界实体 | `assets/ability/make_sakura_art.py:69,71`、`make_senro_art.py:70` |
| TH07 | `ascii.anm` → `ascii.png`（HUD 数字 / 固定标签 / 结界指示） | 107 HUD | `assets/ability/make_senro_hud_art.py:99,101` |
| TH08 永夜抄 | `etama.anm` → `etama2.png`（时点 / 刻符） | **108**（IN_HUMAN_YOUKAI）道具贴图 | `assets/ability/make_tpoint_art.py:54,56` |
| TH08 | `player0{0,1,2}.anm` 的**妖侧本体 55 张** sprite（30×46 / 46×46 原矩形） | 108 妖机体本体（编进 `pl0X.anm` 新 entry） | `assets/ability/make_youkai_art.py:7`、`assets/build_player_anm.py:346-360` |
| TH08 | `enemy.anm`（妖侧附属 / 使魔图） | 108 使魔 | `assets/ability/make_youkai_familiar_art.py:7` |
| TH08 | `playerNN.anm` 的 option sprite | 108 子机 | `assets/ability/make_youkai_option_art.py:7` |
| TH08 | `plyNNas.sht` 的行 `+0x24`（弹体脚本号）+ 那些脚本用到的 sprite（`sprite30/32/35/36`、`sprite22/23`） | 108 妖侧弹（弹体贴图搬进 `ability.anm`，脚本照抄） | `assets/ability/make_youkai_bullet_art.py:1-25`、`assets/build_player_anm.py:611` |
| TH08 | `etama.anm` **sprite219 `(0,96,16,16)`** 火花 | leveledit `st02` 使魔拖尾火花（**规格件**，不进本包） | `assets/mystia/build_st02.py:14` |
| TH09 花映塚 | `front.anm`（从 `local/th09.v1.50a/th09.dat` 现取）的 HUD 框 / 字形 | **109**（POFV_BLAST）左上 HUD | `assets/ability/make_th09_front_art.py:13,42,100` |
| TH09 | `etama.anm` → `etama3.png`（竖向回绕长条带） | 109 环带特效 | `assets/ability/make_pofv109_ring_strip_art.py:8,25` |
| TH10 风神录 | `bullet.anm` → `etama2.png`（绿点信仰）、`etama3.png`（消弹条） | **110**（MOF_FAITH）+ **111** 共用的绿点信仰池 / 灵击 | `assets/ability/make_mof_art.py:254`、`make_mof_bomb_art.py:56,57` |
| TH10 | `front.anm` → `front00.png`（信仰条那一组 7 台 VM 的贴图） | 110 HUD | `assets/ability/make_mof_hud_art.py:110` |
| TH11 地灵殿 | `pl01.anm` → `pl01.png`（子机元素弹） | **111**（SA_COMM）子机弹 | `assets/ability/make_marisa_elem_art.py:61` |
| TH11 | `pl01.anm` → `pl01c.png`（气泡） | 111 子机 / 特效 | `assets/ability/make_nitori_bubble_art.py:54` |
| TH11 | `front.anm` → `front00.png`（信号槽 4 台 VM 的贴图） | 111 HUD | `assets/ability/make_signal_hud_art.py:104` |
| TH11 | `ascii.anm` entry0 **sprite10 `(140,0,14,14)`** 的墨迹字形 | 110 / 111 档位字 | [`assets/ability/README.md`](assets/ability/README.md) §「1:1 切图」第 44 行 |
| TH12 星莲船 | `enemy.anm` 的 `ufo_anim.png`（飞碟 8 帧）+ `enemy2.png` | **112**（UFO）飞碟本体 / 令牌载体 | `assets/ability/make_ufo_art.py:58,59`、`make_ufo_carrier_art.py:61` |
| TH12 | `front.anm` → `front00.png` / `front_ufo.png` / `front02.png` | 112 HUD / 召唤大横幅 | `assets/ability/make_ufo_hud_art.py:154-156` |
| TH12 | `bullet.anm` → `etama3.png`（UFO 槽框）、`etama8.png`（令牌 / 赏金道具） | 112 槽 / 道具 | `assets/ability/make_ufo_gauge_art.py:58`、`make_ufo_token_art.py:86`、`make_ufo_drop_art.py:65` |
| TH12 | `enemy.anm` → `enemy2.png`（召唤爆发） | 112 召唤特效 | `assets/ability/make_ufo_summon_burst_art.py:22` |
| TH12 | `ascii.anm` → `ascii.png` **倒数第三行 16×16 数字** | 112 击破弹字 | `assets/ability/make_th12_font_art.py:46,47` |
| TH13 神灵庙 | `anm/astral`、`anm/front`、`anm/effect` 三处的贴图（神灵实体 / 灵界槽 / 特效） | **113**（TD_SPIRIT）神灵池 + 灵界槽 | `assets/ability/make_trance_art.py:138,139,153` |
| TH13 | `ascii/ascii.png`（灵界剩余时间的**大字体**） | 113 HUD | `assets/ability/make_th13_ascii_art.py:41` |
| TH13 | `pl0X.anm` 的**灵界主炮贴图**（1:1 现切，零缩放零改色） | 113 灵界主炮（追加进 `pl00/01/03.anm`） | `assets/build_player_anm.py:21,163-183,185`（`TH13_TRANCE_ENTRIES`） |
| TH15 绀珠传 | `effect.anm` → `eff_sloweffect.png`（擦弹环）、`front02.png` | **115**（LOLK_CHAPTER）擦弹环 | `assets/ability/make_lolk_ring_art.py:36,51` |
| TH16 天空璋 | `bullet/item.png`（季节点 4 色 + 土用）、`front00.png`（HUD 字形 5 格） | **116**（SEASON）季节点 / HUD | `assets/ability/make_season_art.py:120,121`、`assets/ability/README.md:64,70` |
| TH16 | `pl0Xsub` 副自机贴图（逐季节动画帧） | 116 子机动画 | `assets/ability/make_season_opt_anim_art.py:78,79` |
| TH16 | `pl03.anm`（早苗 = 108 的妖侧） | **108** 早苗妖侧本体 | `assets/ability/make_sanae_youkai_art.py:7,12` |
| TH17 鬼形兽 | `beast/bullet/beast1@beast@0.png` 等 5 张 + `beast.anm.txt` 的脚本原文 | **117**（WBAWC）兽灵令牌 / 咆哮 | `assets/ability/make_animal_art.py:5-10` |
| TH19 獣王園 | `bullet/bullet{1,6}.png`、`enemy2.png`（亡灵） | **109** 合成卡的弹 / 亡灵 | `assets/ability/make_pofv109_ball_art.py:36,37,65` |
| TH19 | `pl00.anm` → `pl00s.png`（蓄力弹） | 109 C1 | `assets/ability/make_pofv109_c1_bullet_art.py:28` |
| TH19 | `effect/etbreak.png`（消弹） | 109 | `assets/ability/make_pofv109_etbreak_art.py:26` |
| TH06–TH17（+ TH18 自己） | **exe 里的图标资源**（PE `RT_GROUP_ICON` / `RT_ICON`） | **106–117 的系统卡卡面**（白底 + 图标）+ **118** 十二合一拼图 | `tools/extract_exe_icons.py` 头注 + [`assets/README.md`](assets/README.md)「系统卡（106–118）卡面」 |

★ 另外：卡 108 的**灵界形态**若没有 TH13 成品可切（咲夜），走的是「她自己 TH18 帧 + 从 TH13 三对原版图量出的色调变换」
（`assets/trance_form_transform.py`）—— 那一条**一半是我们的**、一半像素仍是 TH18 本作。

---

## 3. 本作 TH18 的原生像素（也在包里）

发布包里的 `ability.anm` / `abcard.anm` / `pl0X.anm` 是**整文件替换**，因此**夹带 TH18 原版像素**
（原生 ability entry 0–6、原生 118 张卡面、原生机体帧…）。这部分同样是 **ZUN 版权**，我们同样不主张权利；
thcrap 生态的通行做法就是分发「替换后的整文件」，玩家需自备游戏本体。

以下生成器读的是**本作** TH18（不是跨作）：`make_font2_art.py`、`make_faith_hud_art.py`、
`make_flan_art.py`、`make_marisa_bullet_art.py`、`make_ufo_debris_art.py`、`make_ufo_drop_art.py`（`abitem/abframe`）等。

---

## 4. 非东方（第三方商业作品 / 公共素材）

这些**不是东方 Project** 的素材，来源登记在 [`assets/cards/_src/README.md`](assets/cards/_src/README.md)（逐文件表格）：

| 来源 | 用在哪 |
| --- | --- |
| **Terraria**（Re-Logic）原版物品贴图 `Item_197.png` / `Item_4956.xnb` | 卡 89 星星炮、87 天顶剑（及其剑阵图集） |
| **Minecraft**（Mojang）`textures/item/mace.png` | 卡 91 鞘翅重锤 |
| **Noita**（Nolla Games）`sampo.png` | 卡 92 伟大之作 |
| **Slay the Spire**（Mega Crit）`largeRelics/iceCream.png`、送死亡整卡 | 卡 90 冰淇淋、86 东尼的悲恸 |
| **UNO**（Mattel）反转牌图 | 卡 64 REVERSE |
| **Plants vs. Zombies**（PopCap / EA）的 reanim + 零件图 | 卡 77 豌豆射手 / 向日葵 / 金盏花（场上实体 + 卡面）；来源见下注 |
| **Wikimedia Commons**，Dmitry Fomin，**CC0** | 黑桃 10/J/Q/K/A（58–62）牌面 |
| **用户提供 / 用户原创** | 强欲之壶、神之宣告、青眼白龙、破损核心、炎魔、古锭刀、旋风斩、衔尾蛇、杰拉等 |

★ **PvZ 的逐文件来源**：reanim 与零件图取自公开复刻项目
`ruslan831/PlantsVsZombies-decompilation`、`BhavyaC16/Plants-Vs-Zombies`；三张卡的明细分别在
[`peashooter/_src/README.md`](assets/ability/peashooter/_src/README.md)、
[`sunflower/_src/README.md`](assets/ability/sunflower/_src/README.md)、
[`marigold/_src/README.md`](assets/ability/marigold/_src/README.md)。

⚠ 上面 Terraria / Minecraft / Noita / StS / UNO 同样**有版权瑕疵**（比东方素材更"硬"），
只是 owner 口径是「致敬 + 小体量 + 不商用」；★ 权利方要求即移除。

---

## 5. 音效（SE）

发布包 `th18/voice/*.wav` 里有 **10 枚逐字节取自各作原版 SE**（`derive_retail_se.py` 的 `TABLE` 表驱动）：

| wav id 名 | 取自 | 用在 | 证据 |
| --- | --- | --- | --- |
| `ASTRALUP` | TH13 `se_astralup.wav` | 113 灵界槽升格 | `assets/voice/derive_retail_se.py` `TABLE`；★ 实测 `ASTRALUP.wav` 与 `local/th13.v1.00c/dat/se_astralup.wav` **md5 相同**（`ecf645b5b7efd91d61451da1d616e423`） |
| `UFOALERT` / `UFO` / `UFO_TOKEN_SPAWN` | TH12 `se_ufoalert` / `se_ufo` / `se_piyo` | 112 | 同上 |
| `CHARGE00` / `CHARGEUP` | TH09 `se_charge00` / `se_chargeup` | 109、111 | 同上 |
| `MOFPOWERUP` | TH10 `se_powerup.wav`（TH10 与 TH18 同名文件**不是**同一次录音） | 110 满 P | 同上 |
| `PCB_BORDER` | TH07 `se_border.wav` | 107 开/崩结界 | 同上 |
| `YOUKAI_FAM_SPAWN` / `YOUKAI_FAM_SHIFT` | TH08 `se_option` / `se_ophide` | 108 使魔出现/切换 | 同上 |

其余：`FIRELORD_SUMMON` / `FIRELORD_ATTACK` = **用户提供**语音（`assets/voice/_src/README.md`）；
`STAR_CANNON_SHOT` = **Terraria** 原版音效；`TONY_GRIEF_TRIGGER` = TH18 本体 `se_wolf.wav`；
`ROYAL_RAGTIME` / `ROYAL_FANFARE` = **我们合成**（§1）。

⚠ **口径冲突（记在这里，别让下一个人猜）**：[`assets/voice/README.md`](assets/voice/README.md) 写的是
「这些字节只在本机构建期用，绝不入库、绝不随包分发」，但**发布包里确实带着它们**（本次核对：
`th18/voice/` 15 个 wav，其中 10 枚是上表那些原版字节）。理由也说得通（不然玩家听不到原作的音色），
但**以本文为准**：这些枚是「随包分发的原版音频」，属**版权瑕疵**资产。

---

## 6. 音乐：TH13 灵界版 BGM（重编曲）

- **性质**：**程序生成的重编曲**（不是采样；也不是原曲直接复制）。
- **输入素材**：第三方同人 **MIDI 合集** ——
  `https://github.com/AyHa1810/touhou-midi-collection`
  （clone commit `5a11a0a184102aa28ea85605e20a678fe68ea42e`，路径 `1 - Shooter Games/18 - Unconnected Marketeers/`）
- **生成工具**：本仓库 [`tools/make_trance_rearr.py`](tools/make_trance_rearr.py)
  （SMF 解析 → 判主旋律 → 改 velocity → **SoundFont 渲染** → 与原曲等长对齐；`--render=naive` 保留旧的自写合成器）
- **生成口径**：压低主旋律（×0.35）、抬伴奏、贝斯 ×1.6；与 TH13 原版"灵界版"同向
  （一手：TH13 `NN`/`NNb` 时长严格相等、时间轴零延迟；AUDIT CS-6）
- **MIDI 与生成物均不入库**（只在 `local/`，gitignored）；★ 本仓库**不含任何 MIDI 文件**（已核：`git ls-files` 里 0 个 `.mid/.midi`）
- **版权**：MIDI 版权属**各 MIDI 的编曲者**（本批 = Karl Zuñiga；该合集本身是 **reupload**，
  README 自述原托管在 Gamebanana —— 合集作者不是编曲者，署名要认音轨 meta 的 `MIDI by:`）；
  东方原曲版权属 **上海アリス幻樂団 / ZUN**；
  本 mod 不主张这些权利，若权利方要求将立即移除对应生成物。

### 6.1 来源与编号口径

- **编号口径**：MIDI 文件名里的 `N.` = **音乐室编号**（合集按此命名），内部曲名（`th18_NN`）与之**不等**：
  `th18_01, 03…14, 15, 16, 17, 18` ↔ No.`1,2…13,16,17,14,15`（一手表 = 游戏自带 `dat/musiccmt.txt`；
  `midi_index_for()` 就查这张表）。
- **逐文件清单**：`local/th18.v1.00a/midi/MANIFEST.tsv`（2026-10-19 重建：17 个文件 + 编曲者 + 内部曲名 + md5；
  该文件在 `local/`，不入库）；逐曲 md5 已抄进 §6.3 的表。
- **许可状态**：该合集**没有 LICENSE 文件**，只有 §6.3 引的那段 Usage Policy ——
  **不要**把它转述成"没有许可所以不能用"。

### 6.2 音色库（SoundFont）—— 2026-10-19 起：THfont

**目标音色**：ZUN 自 TH06 起用的是 **Roland Edirol SD-90**（2001，已停产、无 VST）。
本 mod 现在用的就是 SD-90 采样系的 **THfont**（owner 提供），不再用 GeneralUser GS 近似。

★ **旁证（可复核）**：Karl Zuñiga 这 17 个 MIDI 的**轨名**里有 **18 处**直接写着 `SD-90 (…)`
（如 `SD-90 (Romantic Tp)`、`SD-90 (Sop. Sax vib)`、`SD-90 (St.ChoirAahs)`）—— 编曲者自己就是
**按 SD-90 的音色**写的，换 THfont 正是冲着这条去的。

**实际采用（2026-10-19 owner 拍板：3 首试点听感优于 GeneralUser ⇒ 全量 17 首换 THfont）**：

| 项 | 值 |
| --- | --- |
| 音色库 | **THfont**（SD-90 采样系；社区从 Roland SD-90 硬件/软音源里 rip 出来的采样集合） |
| 文件 / 原名 | `thfont.sf2` ← 原名 `Touhou.sf2`，**owner 提供** `D:\Downloads\Touhou.sf2`；只落在 `local/th18.v1.00a/sf2/`（gitignored） |
| 大小 / sha256 | 261,192,820 B（249.1 MiB）/ `c05372f76164aac916f9913550fab9d30d650cf205f000da27513ab98054b419` |
| 容器 / 元数据 | RIFF `sfbk`；`INAM=Touhou.sf2`，`IPRD=ISFT=IENG=SynthFont Viena`（**打包工具**，不是作者/来源 —— 原始元数据已被打包过程抹掉） |
| 预设 | **186 个**：bank 0 = 128 个（**GM 0–127 全覆盖**）、bank 128 鼓组 = 14 个、bank 8 = 22 个，另 bank 1/2/9/16/17/18/24/27/33/255 少量 |
| THfont 判据 | **bank 128 prog 127 = `SD90`**、prog 25 = `TR-909 [SC-55]`、prog 24 = `ELECTRONIC`；鼓组名 `KICK & SNARE`/`ORCHESTRA`/`ETHNIC`；bank 0 音色名亦为 SD-90/SC 系 ⇒ 判定社区 THfont 系，非 GM 官方库（详见下方"判据"） |
| 取回时间 | 2026-10-19（owner 本地提供） |
| 许可 | ★ **未声明**（权利状态不明）⇒ **只本机自用**；★ **发布包只带渲染出的 wav，不带 sf2**；★ 社区惯例使用、**存在瑕疵**、**权利方要求即立即移除**（完整口径见下方"许可"） |
| 渲染器 | `fluidsynth 2.2.5`（Ubuntu 22.04 `apt-get download` + `dpkg-deb -x` 解到 `local/vendor/fluidsynth-2.2.5/`，**不动系统、不需要 root**；LGPL） |

**判据（"这是 THfont 系"怎么认的）**：bank 128 prog 127 = `SD90`、prog 25 = `TR-909 [SC-55]`、
prog 24 = `ELECTRONIC`；鼓组名 `KICK & SNARE` / `ORCHESTRA` / `ETHNIC`；bank 0 音色名亦为 SD-90/SC 系
（`TyrosNylon`、`ClavinovaHarp`、`P5 Saw Lead` …）⇒ 判定为社区 THfont 系，**不是** General MIDI 官方库。

**许可（未声明 —— 所以发布包不带它）**：THfont 系的采样是从 Roland SD-90 硬件/软音源里 rip 出来的
（ShrineMaiden 上 2016 年原作者那帖被版主以「分发采样合成器的采样非法」为由锁掉），
**没有官方下载页、没有可核对的哈希、权利状态不明** ⇒ 只作**本机自用**；
★ **发布包只带渲染出的 wav，不带 sf2**；★ 与上面的东方素材**同一口径**：社区惯例使用、
**存在瑕疵**、**权利方要求即立即移除**。

**备选：GeneralUser GS v2.0.3**（保留说明，随时可切回）

2026-10-19 早些时候先用它渲了全量 —— 作者官方仓库、**许可明确**（GeneralUser GS License v2.0：
可自由用于音乐制作含商业、可再分发、可修改；作者声明没有一个采样来自商业 SoundFont 或采样 CD，
但极少数早期来源无法 100% 追溯），GM/GS 兼容、同属 Roland GS 家族，但**不是** SD-90 本身，
听感比 THfont 差一档（owner 3 首 A/B 后拍板换 THfont）。文件 32,319,396 B / sha256
`9575028c7a1f589f5770fccc8cff2734566af40cd26ed836944e9a5152688cfe`，仓库
<https://github.com/mrbumpy409/GeneralUser-GS>（作者 S. Christian Collins，主页
<https://schristiancollins.com/generaluser.php>），仍留在 `local/th18.v1.00a/sf2/` 里作对照。

> 历史：2026-10-19 早上曾尝试从公开转存取 THfont（musical-artifacts #2451 → Google Drive，
> 被 Cloudflare 挡，HTTP 403）⇒ 当时「拿不到可核验的字节，不硬凑」，退回 GeneralUser。
> 后来 owner 直接给了本地文件，本节以 THfont 为准；上面那段保留为来源溯源。

★ **换音源只改一处**：把任意 GM/GS `.sf2` 丢进 `local/th18.v1.00a/sf2/`（`find_sf2()` 优先挑文件名
带 `thfont` 的，其余按文件名排序取第一份）。换库后**必须**重跑试点重新确认听感，并把新库的来源 + sha256 补进本表。
来源与 sha256 的原始登记：`local/th18.v1.00a/sf2/SOURCE.txt`（本机，不入库）。

### 6.3 MIDI 署名（Usage Policy 原文 + 17 首逐曲清单）

合集 README 的 **Usage Policy** 原文（2026-10-19 读，`github.com/AyHa1810/touhou-midi-collection`）：

> Rules similar to Creative Commons apply for most of the part. You can use the MIDIs and modify it,
> but you must credit the original creator, which is marked with brackets in the MIDI collection.
> **Except,** you **CANNOT** use MIDIs made by ZUN in fangames due to ZUN's new policies.

⇒ 我们照它做三件事（**每一条都实测过，不是照抄承诺**）：

1. **署名编曲者**。本批 **17 首全部是 Karl Zuñiga 的编曲**（唯一覆盖音乐室 No.1–17 全集的编曲者）。
   不是只看文件名：**17 个 MIDI 的 meta 文本里都有编曲者署名 `MIDI by:` / `MIDI By: Karl Zuñiga`**
   （前 12 首小写 `by:`、后 5 首大写 `By:`；`ñ` 存成单字节 `0xf1` = Latin-1。2026-10-19 实测 17/17）。
   同曲另有的 KR.Palto47 / TouhouMIDICreator / EpreTroll / Dekyo ongen / Tybone 版本**均未被采用**
   （逐曲「同曲其它版本」列见 `MANIFEST.tsv`）。
2. **不碰 ZUN 本家的 MIDI**。这 17 首都是同人编曲（ZUN 不发行 MIDI）；对 17 个文件的 meta 文本
   检索 `ZUN` **命中 0**（2026-10-19 实测）⇒ **本批无 ZUN 本家**。
3. **可修改 / 可再分发**按 CC-like 理解（合集没有 LICENSE 文件，只有上面这段原文）；
   我们分发的是**生成物**，MIDI 原文件只在 `local/`（gitignored，不入库）。

逐曲对照（md5 与 `MANIFEST.tsv` **逐行核对，0 处不符**；内部曲名 `th18_NN` → 发布物 `th18_NNb_rearr.wav`）：

| No. | MIDI 文件 | 内部曲名 | 曲名（musiccmt） | md5 |
| --- | --- | --- | --- | --- |
| 1 | `th18_01.mid` | `th18_01` | 虹の架かる幻想郷 | `b735206f44bb4fa12eb7959d9c6ca9c0` |
| 2 | `th18_02.mid` | `th18_03` | 妖異達の通り雨 | `22ce8c3461e5b85d26ebb4ebb9874284` |
| 3 | `th18_03.mid` | `th18_04` | 大吉キトゥン | `5be29c1bd52f7ed9f733c3657b0167de` |
| 4 | `th18_04.mid` | `th18_05` | 深緑に隠された断崖 | `0cf1965c442e797b2a41615978612197` |
| 5 | `th18_05.mid` | `th18_06` | バンデットリィテクノロジー | `c234eb2f2ae1af6b3550b3c7f92212e7` |
| 6 | `th18_06.mid` | `th18_07` | 駒草咲くパーペチュアルスノー | `fcec5d3417a136c0aca9d1d6cb34b843` |
| 7 | `th18_07.mid` | `th18_08` | スモーキングドラゴン | `dc0f1a6978b9c2565ae82ff647356d6c` |
| 8 | `th18_08.mid` | `th18_09` | 廃れゆく産業遺構 | `d950145821fc34a32d18d924a1678baf` |
| 9 | `th18_09.mid` | `th18_10` | 神代鉱石 | `1d8bce2b4bcf9bf177ba35fdc7e70f2c` |
| 10 | `th18_10.mid` | `th18_11` | 待ちわびた逢魔が時 | `9b00bf8e6fd603ada57e589f53c19b29` |
| 11 | `th18_11.mid` | `th18_12` | 星降る天魔の山 | `e7338b714fbbd707a7887371e4f3ee5e` |
| 12 | `th18_12.mid` | `th18_13` | ルナレインボー | `7a16f95c78d859d93f38b3d9868e10d9` |
| 13 | `th18_13.mid` | `th18_14` | あの賑やかな市場は今どこに ～ Immemorial Marketeers | `41b1dfcadb7ad384e9503dfab227b5ad` |
| 14 | `th18_14.mid` | `th18_17` | 幻想の地下大線路網 | `2898fa78d9b42426ac783b05cffabf3b` |
| 15 | `th18_15.mid` | `th18_18` | 龍王殺しのプリンセス | `83315a505f0018d4309a7cc60e1cf91d` |
| 16 | `th18_16.mid` | `th18_15` | 嵐の後の日曜日 | `dbc5f3b392a4083262641cd8d63bbea0` |
| 17 | `th18_17.mid` | `th18_16` | 虹色の世界 | `3f0b8953e22f2041ec25f7c57650c1c1` |

★ **发布包里带的是生成物**：`thcrap/bin/bgm/` 35 个 wav（**382 MiB**，`mode.txt` = `rearr`）全部是
`make_trance_rearr.py` 的输出（`th18_01b_rearr` … `th18_18b_rearr`，含少数 `_both` / `_se` 变体）——
它们是**衍生作品**（同人 MIDI 重编曲 + 第三方 SoundFont 渲染），不是 ZUN 的原始音频；
分发它们仍受上面第 1/3 条约束。

**2026-10-19 全量批（现行）**：17 首 `_rearr` **全部**是 **THfont** 渲染版（`--render sf2` 为默认，
`find_sf2()` 优先挑 `thfont.sf2`）。混音不是"一刀切"：按 **GM program（音色号）/ 鼓通道** 给 dB 增益，
逐曲例外（03 小号/低音区、07 主旋律、08 铜管）登记在 `tools/make_trance_rearr.py` 的
`GLOBAL` / `PER_TRACK` 表，规则与当前取值见 [`TUNING.md`](TUNING.md) §2.0h。
**音色覆盖（同批）**：**鼓 = kit 25**（17 首 ch9，含 ch15 静音层；thfont bank128 `TR-909 [SC-55]`）、
**ZUN 号 = 56**（轨名含 `Romantic Tp` 的轨，61/58 → 56；表 = `KIT_PROGRAM` / `TRUMPET_REMAP`）。
母带与起点用 `--master-peaks` / `--starts` 锚在改版前那一版 ⇒ 除鼓/号外其它乐器逐采样不变；
`th18_07b`、`th18_13b` 与 A/B 的 `__kit25_TR808__`、`th18_08b` 与 `__kit25_and_56trumpet__`
**逐字节相同**（A/B 在 `灵界BGM试听/鼓组A-B/`）。
**上一代音源留档**：GeneralUser GS 渲染版 17 个另存为 `<modkit>\thcrap\bin\bgm\*.wav.gu.bak`
（`thcrap/bin/bgm/` 同目录，覆盖前逐字节复制并核对 md5）；
更早的自写合成器（`--render naive`）版 17 个仍在 `*.wav.naive.bak`（共 191.5 MB）；
本批（kit25 + 56 之前那一版）17 个另存为 `*.wav.pre-kit25.bak`。
各代**帧数逐首相等**（= `thbgm.fmt` 的循环体长度）⇒ 循环点 / 同时间点 seek 的口径不变。

---

## 7. 来源待查（不编，留白）

| 资源 | 状态 |
| --- | --- |
| `assets/ability/star_cannon/_src/Item_75.png` | **来源待查**：文件名像是 Terraria `Item_75`，但本仓没有生成器/文档说明它用在哪（`grep Item_75` 无命中）。若确认未使用，删掉即可 |
| `assets/cards/_art/*.png` | `fit_card.py` 的**过程留档**（无框画面），与对应 `_max` 同源；个别早期文件的上游图未见登记 |
| `assets/ability/_draft_109/*` | 109 期间的**草案 anm 文本**（未编译、未进包），其贴图引用的是 TH18 原生 `eff_magicsquare/enemy_aura/eff_aura`（本作，不是跨作） |

其余 `assets/**` 下的 PNG 都能在 `make_*_art.py` / `ORDER.txt` / `_src/README.md` 里找到出处；
★ 找不到的**不要猜**，往这张表里加一行。

---

## 8. 权利方要求时

本 mod 是非商业同人作品（thcrap 补丁生态），**不主张**上述任何原版素材的权利。
若**上海アリス幻樂団 / ZUN** 或任何**第三方权利方**（Terraria / Minecraft / Noita / Slay the Spire /
Mattel / 同人 MIDI 作者）要求，我们会**立即移除**对应素材（图像 / 音频 / BGM），
并按需重新发布去除这些素材的版本；代码与自绘部分不受影响。

---

## 9. 相关文档

- 素材总目录与合成口径：[`assets/README.md`](assets/README.md)
- 卡图来源逐文件表：[`assets/cards/_src/README.md`](assets/cards/_src/README.md)
- 音频来源逐枚表：[`assets/voice/README.md`](assets/voice/README.md)、[`assets/voice/derive_retail_se.py`](assets/voice/derive_retail_se.py)
- 能力贴图与切图口径：[`assets/ability/README.md`](assets/ability/README.md)
- 灵界 BGM 的生成与一手依据：[`tools/make_trance_rearr.py`](tools/make_trance_rearr.py)、[`AUDIT.md`](AUDIT.md) CS-6
