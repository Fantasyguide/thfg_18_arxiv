# TH19 能力卡（アビリティ）— 资产面：卡面 / 图标 / 选择菜单 / 文案对照

> **版本**：TH19 v1.10c（`th19.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>
> ⚠ **实测更正（必读）**：本样本 `local/th19.v1.10c/th19.exe` 的版本资源实为 **ver 1.10c**，不是 1.00a。
> 一手证据：exe 内 UTF-16 串 `0x593dd4` = `東方獣王園　～ Unfinished Dream of All Living Ghost. ver 1.10c`
> （同处英文串 `TH19 - … ver 1.10c`）；ANSI 串 `1.10c` 在 `0x5a05e4`。
> 又：`th19.dat` 内含数据侧清单 `th19_110c.ver`，而未打包 `data/` 内含 `th19_100a.ver`。
> ⇒ **本文所有裸地址与所有 `th19.dat` 资源都属 1.10c 构建**；`local/th19.v1.10c/ability.txt` 反而是 1.00a 的副本（见 §4、§6）。
>
> **方法**：`thdat -l/-x 19` 解包 + `thanm -l 19` 列 anm（THTK，`local/vendor/thtk/build/`）+ 直接读 `th19.exe`
> 的静态表 `0x5c5640` + `local/th19.v1.10c/decomp`（4740 函数语料库，`th20_show.py` 读）。
> **全程未开 Ghidra 工程锁**；对 `D:\…\th19\data\` 只读，未改动任何文件。
> **结论标记**：✅ 一手直读/实测 / 🟡 单一证据链或推断 / ❓ 未解（汇总在 §7）。
> **前置**：语义侧（枚举表 / 23 槽虚表 / 装配链）见同目录 [`01-ability-cards.md`](01-ability-cards.md)。

## 0. 一句话结论

TH19 的能力卡资产 = **三个 anm 一组**：`abcard.anm`（**卡面**，85 entry = 85 sprite，一张卡 = 一大一小两张图）、
`abmenu.anm`（**选择菜单 UI**，4 entry / 36 script，其中约 2/3 是 TH18.5 闇市場 遗留文本）、
`ability.anm`（**能力效果**动画，贴图是自机 sprite sheet，**不是卡面**）。
「哪张卡用哪个 sprite」**不靠命名、靠 exe 里的静态表 `0x5c5640` 的 `+0x14`/`+0x18` 两个字段**。

## 1. 相关 anm 清单（TH19 里与能力卡直接相关）

| anm | dat 内大小(B) | entry | script | sprite | 证据出处 | 与能力卡的关系 |
| --- | ---: | :-: | :-: | :-: | --- | --- |
| `abcard.anm` | 3036945 | **85** | **26** | **85** | `thdat -l 19` 列出；`thanm -l 19` 成功列出 85 entry | ✅ **卡面本体**（大/小两套图）+ 卡框/卡背/影 |
| `abmenu.anm` | 426110 | **4** | **36** | **28** | 同上 | ✅ **选择菜单 UI**（标题条 / 标签 / 确认框 / 各种提示文） |
| `ability.anm` | 109555 | **17** | **51** | **141** | 同上 | ✅ 能力**效果**动画；17 entry 全是自机贴图 `player/plXX/*.png`（含 `pl02c.png`） |
| `ascii.anm` | 258079 | 8 | 20 | 915 | 同上 | ❌ **无卡牌 UI**（8 entry 全是 `ascii/*.png` 字体/载入图）；但菜单文字由它提供字形 |
| `front.anm` | 1028563 | 14 | 371 | 181 | 同上 | ❌ **无卡牌 UI**（14 entry：`front00`/`demoplay`/`cleartitle`/`clear`/`pause*`/`ename`/`balloon_1024`×4/`lifebar`/`gameset`）|

**加载点（一手，三处构造器写法完全相同）**：`FUN_004dda60` `0x4dda60` 里依次
`FUN_004ce870(0x3a,"ability.anm")` → 存 `obj+0x10`；`FUN_004ce870(0x3b,"abcard.anm")` → `obj+0x14`；
`FUN_004ce870(0x3c,"abmenu.anm")` → `obj+0x18`。另外两处同形：`FUN_004de540` `0x4de540`、`FUN_004de650` `0x4de650`。
⇒ **三个 anm 是一组、一起加载**；槽号 0x3a/0x3b/0x3c（= 58/59/60）。

**`ability.anm` 覆盖了哪些自机**（entry 名实测，17 entry / 13 个自机）：
`pl00`(Reimu, 2 entry) / `pl01`(Marisa, 2) / `pl02`(Sanae, 3: `pl02s`×2 + `pl02c`) / `pl03`(Ran) /
`pl07`(Orin) / `pl08`(Tsukasa) / `pl09`(Mamizou) / `pl13`(Suika) / `pl14`(Biten) / `pl15`(Enoko) /
`pl16`(Chiyari) / `pl17`(Hisami) / `pl18`(Zanmu)。
**缺 `pl04`(Aun) / `pl05`(Nazrin) / `pl06`(Seiran) / `pl10`(Yachie) / `pl11`(Saki) / `pl12`(Toutetsu)** 六个自机（❓ 其能力效果在别处，未追）。

## 2. 卡面与图标：哪张卡 = 哪个 entry / sprite

### 2.1 `abcard.anm` 的 entry 布局（✅ 一手，`thanm -l 19` + 逐 entry 解析）

- **entry 号 = sprite 号**（实测：85 entry / 85 sprite，**每个 entry 恰好 1 个 sprite**，编号全程连续无错位）
  ⇒ 引用一张卡图只需要一个数。
- entry 是大卡/小卡**成对**排列，`大 = 5+2k`、`小 = 6+2k`（k = 卡 id，见表 §4）。

| entry | 名字（anm 内 `name`） | 贴图实际尺寸 | anm 声明 w×h | sprite 矩形 | 是什么 |
| :-: | --- | --- | --- | --- | --- |
| 0 | `ability/abframe.png` | 64×80 PNG(RGBA) | 64×128 | 64×80 | **卡框**（选中/通常两态，见 §3.4） |
| 1 | `ability/abitem.png` | 40×44 PNG(RGBA) | 64×64 | 40×44 | 小道具图标（❓ 用途未定） |
| 2 | `ability/abcardshadow.png` | 128×160 PNG(RGBA) | 128×256 | 128×160 | 卡影 |
| 3 | `ability/BACK_max.png` | 256×320 JPEG | 256×512 | 256×320 | 卡背（大） |
| 4 | `ability/dummy.png` | **8×8** JPEG | 8×8 | **64×80** ⚠ | 占位图（矩形与贴图不符，见下） |
| 5 / 6 | `ability/NARUMI_max.png` / `NARUMI_min.png` | 256×320 / 64×80 | 256×512 / 64×128 | 同图 | `@POWERUP`（成美）的卡 |
| 7 / 8 … 81 / 82 | `ability/card_<名>_max.png` / `card_<名>.png` | 256×320 / 64×80 | 256×512 / 64×128 | 同图 | 19 角色 × 2 张 = 38 张卡 |
| 83 | `ability/empty_max.png` | 256×320 JPEG | 256×512 | 256×320 | 空卡（❓ 归谁见 §7） |
| 84 | `ability/dummy.png` | 8×8 JPEG | 8×8 | **64×80** ⚠ | 同 entry 4 |

**⚠ dummy 的坑（一手）**：entry 4 与 entry 84 贴图只有 8×8，但 sprite 矩形写成了 `64×80`（`{x:0,y:0,w:64,h:80}`），
即**采样越界**。TH18 的 `dummy`（`th18` entry3/117）矩形也是 64×80 但贴图是 64×128（不越界）⇒ TH19 这一处是回归。

**贴图存储格式（一手，扫 anm 二进制 magic）**：`abcard.anm` 内 **82 个 `JFIF`（JPEG）流 + 3 个 `\x89PNG` 流**，
PNG 的正好是 entry 0/1/2（abframe / abitem / abcardshadow，带 alpha），**其余 82 张卡面全是 JPEG**。
`thanm -x` 会把 JPEG 字节写进 `.png` 文件名的文件里（`file` 报 `JPEG image data`）——**别信扩展名**。

### 2.2 「一张卡 = 哪个 entry」的唯一权威：exe 静态表 `0x5c5640`

表结构（`0x20` 字节/项，40 项 + 哨兵；语义侧见 [`01-ability-cards.md`](01-ability-cards.md) §1.1）。
本文只关心两个字段，**一手 dump**（文件偏移 `0x1c2e40`，VA `0x5c5640`）：

| 偏移 | 语义（本文结论） | 一手证据 |
| --- | --- | --- |
| `+0x14` | **大卡 = `abcard.anm` entry / sprite 号**（5,7,9,…,81） | 逐项 dump；`POWERUP` 项 = 5 而 entry5 = `NARUMI_max.png`（成美）；`BACK` 项 = 3 而 entry3 = `BACK_max.png` |
| `+0x18` | **小卡 = entry / sprite 号**（= `+0x14` + 1；`BACK` 项 = 4 → `dummy`） | 同上；21 项全为 `+0x14`+1 |

⇒ **「每个能力一张图」成立**：39 张卡各占 2 条 entry（大 256×320 + 小 64×80），加 5 条通用件 + 2 条空卡/占位 = 85。

### 2.3 命名坑（★ 借素材时必须知道）

| entry | anm 内文件名 | 表 `0x5c5640` 里指向它的键 | 图的实际内容（我导图看的） | 判定 |
| :-: | --- | --- | --- | --- |
| 27 / 28 | `card_nazurin2_max/min.png` | **`NAZRIN1`**（id 11） | **振り子（pendulum）** | ✅ 与 `@NAZRIN1` = ペンデュラム 一致；**文件名是错的** |
| 29 / 30 | `card_nazurin1_max/min.png` | **`NAZRIN2`**（id 12） | **ロッド（rod）** | ✅ 与 `@NAZRIN2` = ナズーリンロッド 一致；**文件名是错的** |

⇒ 在 **1.10c**（= `th19.dat`）里「索引 ↔ 图」是对的，**只有 Nazrin 这一对的 anm 内文件名与键相反**。
⇒ 在 **1.00a**（= 未打包 `data/abcard.anm`）里这一对的**图**也是反的（索引 27 放的是 rod）——见 §6。🟡

`dummy`/`empty_max` 之外，其余 37 张卡的 entry 名与键逐个对得上（例：`card_saki1_*` → `SAKI1`，
`card_zanmu2_*` → `ZANMU2`，`NARUMI_*` → `POWERUP`）。✅

## 3. 选择菜单：`abmenu.anm` 的 36 个 script 各画什么

`abmenu.anm` = **4 entry / 28 sprite / 36 script**。sprite 全局连续编号：entry0 → `sprite0..18`、
entry1 → `sprite19`、entry2 → `sprite20`、entry3 → `sprite21..27`。
下表的「文本」是我把每个 sprite 矩形裁出来**看图读出来的**（`thanm -x` 导图，一手）。

### 3.1 entry0 `ability/abilitymenu.png`（512×1024，18 sprite）— 主菜单文字板

| sprite | 矩形 | 文本（日 / 英） | 判定 |
| :-: | --- | --- | --- |
| 0 | `(0,0,512,64)` | 所持中のアビリティカード / Ability Card | 现役 |
| 1 | `(0,64,512,64)` | 所持カード詳細 / Ability Card | 现役 |
| 2 | `(0,128,512,64)` | 発見済みカード一覧 / Unlocked Card List | 现役（TH18 同） |
| 3 | `(0,192,256,64)` | 戻る / Quit | 现役 |
| 4 | `(256,192,64,32)` | New! | 现役（徽章） |
| 5 | `(320,192,96,32)` | Owned | **TH19 新增**（TH18 无此 sprite） |
| 6 | `(256,224,96,32)` | Discount! | **TH19 新增** |
| 7–10 | `(0,256/320/384/448,512,64)` | タイプ：使用/装備/能力/即効カード（Active/Equipment/Passive/Item Card） | 现役 |
| 11 | `(0,512,512,64)` | 新たにアビリティーカードを入手せよ / Select New Ability Card | 现役 |
| 12 | `(0,576,512,64)` | このカードで良いですか？ / Is this card OK? | 现役 |
| 13 / 14 | `(0/256,640,256,64)` | はい Yes! / いいえ No | 现役 |
| 15 / 16 | `(0,704,512,144)` **两 sprite 矩形完全相同** | 手持ちの弾貨で買えるカードがありません。「勝手に天下の四リ物」を入手して闇市場を終了します。 | 🟡 疑 遗留（闇市場/弾貨 是 TH18.5 的词） |
| 17 | `(0,864,512,64)` | 弾貨が足りません / You don't have enough "bullet money" | 🟡 疑 遗留（TH18 同位置是「お金が足りません」） |
| 18 | `(0,928,512,64)` | 初期所持カード設定 / Hold Ability Card | 现役 |

### 3.2 entry1–3：标题与 TH18.5 遗留板

| entry | 文件 | sprite | 矩形 | 文本 | 判定 |
| :-: | --- | :-: | --- | --- | --- |
| 1 | `ability/abmenutitle.png` 512×64 | 19 | `(0,0,512,64)` | 初期装備アビリティーカード / Hold Ability Card | ✅ TH19 的选卡标题 |
| 2 | `ability/abmenutitle2.png` 1024×64 | 20 | `(0,0,640,64)` | 装備コストがオーバーしています / Equipment Cost is Over!! | 🟡 **TH18.5 的装备成本系统词**，TH18 无此 entry |
| 3 | `ability/abilitymenu2.png` 512×512 | 21–27 | 见下 | 闇市場 / 弾貨 一整套提示文 | 🟡 疑 遗留（见下） |

entry3 逐 sprite：21 `資金が足りません`／22 `闇市場が開かれる……！`／23 `辿りが虹色に輝く……`／
24 `闇市場が中止された！`／25 `闇市場を終了しますか？`／26 `手持ちの弾貨で購入できるカードがありません`／
27 `購入済みカードです`。（`thanm -x` 导图逐一读过）

### 3.3 script → 界面元素（36 个 script 全覆盖）

`ins_501(scriptN)` = 生成子 VM，`ins_300(spriteN)` = 绑图，`ins_400(x,y,z)` = 定位，`ins_5(n)` = 等 n 帧。

| script | sprite | 角色 | 归类 |
| :-: | --- | --- | --- |
| 4 | 无 | **根**：`ins_501(0/1/2/3)` | 根 |
| 0 | 0 | 所持中のアビリティカード 标题条（并串起 7–13） | 标题 |
| 1 / 2 / 3 | 1 / 2 / 3 | 所持カード詳細 / 発見済みカード一覧 / 戻る 三条（各 98 次调用，结构同形） | 面板标题 |
| 5 | 7 | タイプ：使用カード | 标签 |
| 6 / 7 | 13 / 14 | はい / いいえ 按钮（各 98 次调用） | 按钮 |
| 8 | 11 | 「新たに…入手せよ」提示 | 提示 |
| 9 | 12 | 「このカードで良いですか？」确认 | 确认框 |
| 10 / 11 | 15 / 16 | 弾貨不足の说明文（两个 sprite 同矩形） | 提示 |
| 12 | 17 | 弾貨が足りません | 提示 |
| 13 / 14 / 15 / 16 | 21 / 26 / 27 / 25 | 資金が足りません / 購入できるカードがありません / 購入済み / 闇市場を終了しますか？ | 提示（疑 遗留） |
| 17 | 无 | 全画面暗転（`ins_603(768,896)` 填充矩形，α160） | 遮罩 |
| 18 / 19 | 无 | 子画面根（分别 `ins_501` 起 25/9/6/7/17/10/12/16/11/14 与 9/6/7/10/13/16/11/14/15） | 子根 |
| 20 / 21 / 22 / 23 / 24 | 4 / 5 / 6 / 4 / 5 | 徽章 New! / Owned / Discount! | 徽章 |
| 25 | 22 | 闇市場が開かれる……！ | 提示（疑 遗留） |
| 26 | 无 | 细矩形组的父（定位 640,128，`ins_500` 起 27/28/30/29） | 子根 |
| 27 / 28 / 29 / 30 | 无 | 矩形 **200×8 边框** ×2 + **199×7 填充** ×2，颜色闪烁 | ❓ 数值条/装备成本条 |
| 31 | 无 | 标题父（定位 852,210，起 32） | 子根 |
| 32 | 19 | `abmenutitle.png` 本体 | 标题 |
| 33 | 20 | `abmenutitle2.png` 本体（起 34） | 标题 |
| 34 | 无 | 全画面暗転（`ins_603(1280,960)`，α192） | 遮罩 |
| 35 | 23 / 24 | 辿りが虹色に輝く…… / 闇市場が中止された！ | 提示（疑 遗留） |

**关于「列表 / 说明框 / 选中框」**（✅ 一手）：
- **列表本体不是 abmenu 画的**：卡位由 C++ 建 VM —— `FUN_004e09f0` `0x4e09f0`（选卡界面，2 个卡位）与
  `FUN_004e0b20` `0x4e0b20`（全卡一览，39 个卡位循环）都调
  `FUN_004dcbd0(…, 0x19, <表 0x5c5640 的项指针>)`，再用 `FUN_004cb7a0(vm, 0x14)` 标「选中」、
  `0x13`/`0x15` 标左右邻。⇒ **表项指针被直接传进卡位 VM**（这就是 §2.2 那两列字段的消费点）。
- **选中框 / 卡边框在 `abcard.anm`**：`abcard.anm` script `2` 与 script `3` 都绑 `sprite0`（`abframe.png` 64×80），
  script `2` 用 `ins_404(0,32,0)`（暗绿）、script `3` 用 `ins_404(255,255,255)`（白）⇒ 两态卡框。
  其余：script `15` 绑 entry2（卡影）、script `16`/`17`/`24`/`25` 绑 entry3（`BACK_max`，162/210/29/163 次调用，翻牌/卡背）、
  script `20` 绑 entry1（abitem）、script `22`/`23` 绑 **entry7（`card_reimu1_max`）**＝**样例卡图**（TH18 里同位置绑的是 `EXTEND2_min`，同样是样例 ⇒ 🟡 该 sprite 疑似运行期被 C++ 改写）。
- ❓ **无法从资产侧判定** TH19 实际调用 abmenu 的哪几个 script（哪些是死的）。表里的「现役 / 疑 遗留」是按
  **文本内容属于哪一作的词汇**分的（闇市場・弾貨・装備コスト = TH18.5 词），不是按调用点。

### 3.4 与 TH18 同位置资产的逐项差异（✅ 一手）

| 项 | TH18（`th18.dat` 内的 `abmenu.anm`） | TH19（`th19.dat` 内的 `abmenu.anm`） |
| --- | --- | --- |
| entry / script | 3 / 19 | **4 / 36** |
| entry0 `abilitymenu.png` | 512×1024，**16 sprite** | 512×1024，**18 sprite**（多 Owned / Discount!，且 15/16 同矩形） |
| entry1 `abmenutitle.png` | 512×**256**（3 sprite） | 512×**64**（1 sprite） |
| entry2 `abilitymenu2.png` | 512×**256**（2 sprite） | 512×**512**（7 sprite） |
| entry3 | **无** | `abmenutitle2.png` 1024×64（TH18.5 装备成本词） |
| 文本 | 「お金が足りません」「POWER を支払うことでも入手出来ます」 | 「弾貨が足りません」「闇市場…」＝**TH18.5 的词** |

## 4. `ability.txt` ↔ 资产 ↔ 类名 对照表

`ability.txt`（Shift-JIS；本仓那份 = 1.00a，见 §6）按 `@KEY` 分块，每块 3 行有效：
**名前 / 効果短句 / 説明**（后接空行）。键共 **41 块 / 40 个唯一键**（`@SEIRAN1` 出现两次，第 118、145 行，内容完全相同 🟡 原文笔误）。

| `ability.txt` 键 | 显示名 | 效果短句 | `AbilityCard*Inf` 类名 | 卡 id | 资产（`abcard.anm` entry / sprite） |
| --- | --- | --- | --- | :-: | --- |
| `@POWERUP` | 肉体強化地蔵 | 対動物霊攻撃力UP | `AbilityCardNarumiInf` | 0 | entry `5`（大 `NARUMI_max.png`）/ entry `6`（小） |
| `@REIMU1` | 陰陽玉 | 追加ホーミングショット | `AbilityCardReimuOpInf` | 1 | entry `7`（`card_reimu1_max`）/ `8` |
| `@REIMU2` | ありがたい御札 | 極小当たり判定 | `AbilityCardReimu2Inf` | 2 | entry `9`（`card_reimu2_max`）/ `10` |
| `@MARISA1` | ミニ八卦炉 | 追加レーザー | `AbilityCardMarisaOpInf` | 3 | entry `11`（`card_marisa1_max`）/ `12` |
| `@MARISA2` | 蒐集家の教訓 | 広範囲アイテム回収 | **`AbilityCardNitoriInf`** ⚠ | 4 | entry `13`（`card_marisa2_max`）/ `14` |
| `@AUN1` | けしかける狛犬 | チャージ速度UP | `AbilityCardAunInf` | 9 | entry `23`（`card_aun1_max`）/ `24` |
| `@NAZRIN1` | ペンデュラム | 霊撃が二つ増える | `AbilityCardNazrin1Inf` | 11 | entry `27`（文件名 `card_nazurin2_max` ⚠ 内容＝振り子）/ `28` |
| `@NAZRIN2` | ナズーリンロッド | 霊撃の効果時間が長くなる | `AbilityCardNazrin2Inf` | 12 | entry `29`（文件名 `card_nazurin1_max` ⚠ 内容＝ロッド）/ `30` |
| `@SAKI1` | 勁牙組の威圧 | 攻撃オオカミ霊が強化 | `AbilityCardSaki1Inf` | 23 | entry `51`（`card_saki1_max`）/ `52` |
| `@ZANMU2` | 攻撃こそ地獄の華 | ゲージでボム入手 | `AbilityCardZanmu2Inf` | 38 | entry `81`（`card_zanmu2_max`）/ `82` |
| `@NULL` | （本文「装備可能です」） | — | `AbilityCardNullInf` | 39 | ❓ entry `83`（`empty_max.png`，见表里无 `NULL` 行） |

- id / 类名 / `+0x14`+`+0x18` 三列全部来自 `0x5c5640` 表 dump（`id` 在 `+0x04`，A 在 `+0x14`，B 在 `+0x18`）。✅
- `@MARISA2` → `AbilityCardNitoriInf` 是 ZUN 侧的遗留类名（RTTI 名与行为都在同目录 [`01-ability-cards.md`](01-ability-cards.md) §1.3 复核过），**其资产仍是 `card_marisa2_*`**。✅
- 表里**没有 `@NITORI` / `@NULL` 行**（第 39 项是键 `BACK`、id 40，指向 entry `3`=`BACK_max`/`4`=`dummy`）。✅

## 5. 能不能借：TH19 的卡牌 UI 资产在本仓 TH18 宿主里能否直接用

**测到的事实（只列实测，不推测）：**

| 对照项 | TH19 `abcard.anm`（1.10c） | TH18 `abcard.anm`（本仓宿主） | 能否直接用 |
| --- | --- | --- | --- |
| entry 数 | 85 | 118 | — |
| entry : sprite | **1 : 1**，sprite 号 = entry 号 | 同（1 : 1，sprite 号 = entry 号）| ✅ 索引规则同构 |
| 大卡 | 256×320，sprite 矩形 256×320 | 256×320，sprite 矩形 256×320 | ✅ **像素尺寸完全一致** |
| 小卡 | **64×80**，sprite 矩形 64×80 | **64×128**（上半 64×80 有图、下半 48 行全 α=0），sprite 矩形 64×80 | 🟡 需**补透明到 64×128 画布**才能对齐 TH18 的 anm 声明 |
| `abframe` | 64×80（THTX 64×80） | 64×128（THTX 64×128），sprite 矩形同 64×80 | 🟡 同上 |
| 贴图存储 | **JPEG**（82 个 `JFIF` 流）+ 3 个 PNG | **全部原始 RGBA**（anm 二进制里无任何 `JFIF`/`PNG` magic；`THTXSize = w×h×4`）| ❌ **anm 文件本身不能互换** |
| 卡框/选中框 | `abcard.anm` script `2`（暗绿）/ `3`（白）绑 `sprite0` | 同位置 script 绑 `sprite0`（`abframe`） | ✅ 配方同形 |
| 卡背 | `BACK_max` = entry `3` | `BACK_max` = entry `2` | ❌ **entry 号不同**，照抄编号会错位 |

**结论（🟡，限已测项）**：

1. **TH19 的 `abcard.anm` / `abmenu.anm` / `ability.anm` 文件本身不能塞进 TH18**：TH19 侧卡面是 JPEG blob，
   TH18 的 anm 存的是原始 RGBA（实测 TH18 三个 anm 二进制里 `JFIF`/`PNG` magic 计数 = 0，TH19 `abcard.anm` = 82/3）。
2. **TH19 的卡面「图」可以直接用**：`_max` 256×320 与 TH18 **逐像素尺寸一致**；`_min` 64×80 就是 TH18 那条
   `pad_min` 的输入（本仓 `assets/build_abcard.py` 里 `want_min = (64,80)`、`pad_min = (64,128)` 正是这一步）。
   ⇒ 把 TH19 的 JPEG 解码成 PNG 后喂进现成的 `build_abcard.py` 管线即可，**不需要新写格式转换**。
   （抽样核对：TH19 `card_reimu1_max.png` 与 TH18 `REIMU_OP_max.png` 图像内容一致，平均绝对差 2.8/255、最大 80 —— 是同一张画的重编码差异。）
3. **本仓已有的 TH18 卡牌资产**（`mods/th18.v1.00a/card-expand/assets/ability/`：`entries/ORDER.txt` 追加 entry、
   `scripts/NN_*.anm.txt` 从 68 起编号的脚本、`cards/ORDER.txt` 决定 `sprite_large = base+2k`）与之的差异：
   - 本仓的「一卡两 sprite（大/小相邻偶数/奇数）」排法**与 TH19 表 `0x5c5640` 的 `+0x14`/`+0x18` 完全同构**（`A=5+2k`, `B=A+1`）——**这是 TH18/TH19 共有的排法**，所以 TH19 的卡图可以不改索引直接进本仓的 2-per-card 槽位。✅
   - 本仓 `abcard` 的每张新卡「吃 2 条 entry、上限 256」，与 TH19 一样受同一个 `(anm 槽号<<8)|entry` 硬顶约束。
   - 本仓的 `ability.anm`（场上特效，原生 7 贴图 / 68 script）与 TH19 的 `ability.anm`（17 entry 全是自机 sprite sheet）
     **是两套不同的东西**，只是同名同槽位（0x3a）。❌ 不可互相替换。
4. ⚠ **借素材的命名陷阱**：TH19 的 `card_nazurin1/nazurin2` 文件名与键**相反**（§2.3）；照文件名搬会搬错两张卡。

## 6. `data/` 未打包目录（只读）

- 路径（**只读，本次未改动任何文件**）：`/mnt/d/SteamLibrary/steamapps/common/th19/data/`（= `D:\SteamLibrary\steamapps\common\th19\data\`）。
- 文件数 **352**；`th19.dat` 列 **351** 项。`data/` 比 dat 多 **2** 个：`th19_list.txt`、`th19_100a.ver`；
  dat 比 `data/` 多 **1** 个：**`th19_110c.ver`**。✅ 一手（`ls` + `thdat -l 19` 对账）

**与能力卡有关的文件（全部只读路径）：**

| `data/` 内文件 | 大小(B) | dat 内同名项 | 大小关系 | 内容关系（md5 实测） |
| --- | ---: | :-: | --- | --- |
| `ability.txt` | 5078 | ✅ 有 | **差 4 B**（dat 5074） | **不同**：`ce0ff4ea…`(data/) vs `8720e840…`(dat) |
| `ability.anm` | 109555 | ✅ 有 | 相同 | **相同** `bbab6338…` |
| `abcard.anm` | 3036945 | ✅ 有 | 相同 | **不同**：`47379f7a…`(data/) vs `5eb5e2a0…`(dat) |
| `abmenu.anm` | 426110 | ✅ 有 | 相同 | **相同** `1597dcdf…` |
| `ascii.anm` / `front.anm` | 258079 / 1028563 | ✅ 有 | 相同 | **相同** `d23bdd70…` / `982dbfe1…` |
| `help.anm` + `help_01..13.png` / `trophy.anm` + `trophy.txt` | — | ✅ 有 | 相同 | （未逐项比内容） |

**两处不一致各是什么（✅ 一手，逐字节/逐行 diff）：**

1. `ability.txt`：只差 **1 行**，在末尾 `@TOUTETSU1` 块的说明文 ——
   `data/`(=1.00a)「送り込んだオオワシ霊が**強く**なる饕餮尤魔の能力」
   vs `th19.dat`(1.10c)「自分のオオワシ霊が**弱く**なる饕餮尤魔の能力」。
2. `abcard.anm`：3036945 B 全同、**609 段 / 142835 B** 不同，全部落在 **entry 27/28 与 29/30（Nazrin 两张卡）**。
   差异是「anm 内文件名 + 贴图数据」**整体互换**：`data/`(1.00a) 的 entry27 = `card_nazurin1_max`(ロッド)、
   entry29 = `card_nazurin2_max`(振り子)；`th19.dat`(1.10c) 的 entry27 = `card_nazurin2_max`(振り子)、
   entry29 = `card_nazurin1_max`(ロッド)。其余 81 entry 的名字与贴图**逐字节相同**。

**⚠ 对本仓的直接影响**：`local/th19.v1.10c/ability.txt` 的 md5 = `ce0ff4ea…`，**等于 `data/`（1.00a）那份**，
**不等于 `th19.dat`（1.10c）那份**。而 `th19.dat` 解出来的 `abcard.anm` 是 1.10c 的。
⇒ 现在仓里的「文案」和「卡面」来自**两个不同补丁版本**，`@NAZRIN1/2` 与 `@TOUTETSU1` 尤其要注意。

## 7. 未解 / 待补

| # | 未解项 | 已知到什么程度 | 下一步怎么解 |
| :-: | --- | --- | --- |
| 1 | TH19 运行期**实际调用** `abmenu.anm` 哪些 script | 36 个 script 的「画什么」已逐一读出（§3.3）；但「哪些是死资源」没证据 | 在上位调用点断点/静态找 `anm+0x18` 的 script 实参；本工单未做 |
| 2 | `@NULL` 的卡图到底是哪张 | 表 `0x5c5640` **无 `NULL` 行**；entry `83` = `empty_max.png` 位置恰等于 `5+2×39` | 找 id=39 的取图路径（可能不走表、直接算 `5+2·id`）；不猜 |
| 3 | `abmenu.anm` script `26`–`30` 那组矩形（200×8 框 / 199×7 填充）显示什么数值 | 只知是 4 个叠放的细矩形 + 颜色闪烁；同文件有「装備コストがオーバー」标题 | 未解；需上位逻辑 |
| 4 | entry `1` `abitem.png`（40×44）与 entry `83` `empty_max` 的用途 | 只有 `abcard.anm` script `20` 绑 `sprite1` | 未解 |
| 5 | `ability.anm` 为何只覆盖 13/19 自机（缺 pl04/05/06/10/11/12） | entry 名已列全（§1）；那 6 人的能力效果在哪个 anm 未追 | 逐个 grep `plXXb/plXXf/effect.anm` |
| 6 | `abcard.anm` script `22`/`23` 绑死 `sprite7`（`card_reimu1_max`）是不是「运行期被 C++ 改写」 | TH18 同位置绑 `EXTEND2_min`，同样是某张具体卡 ⇒ 两作同形，但没找到改写点 | 未解 |
| 7 | `th19.dat` 到底是 1.10c 还是 1.00a（本文按 `th19_110c.ver` + exe 版本串判 1.10c） | exe 版本串 1.10c 是硬的；dat 侧只有文件名证据 | 与官方 1.10c 更新日（2024-07-08）比对 mtime（dat 2024-07-12） |
| 8 | TH18.5（`local/th185.v1.00a`）未解包，无法核对「闇市場/弾貨」究竟是不是 TH18.5 的词 | 只有 TH18 对照（TH18 用「お金/POWER」） | 解包 `th185.dat` 的 `abmenu.anm` 比字 |
