# assets/ability — `ability.anm` 追加（卡牌场上特效）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

`ability.anm` 是主动卡场上特效的家（原生 7 张贴图 / 68 个脚本，Tenshi 的要石是 script28）。
脚本只能用同文件里的 sprite，所以要在特效里露出卡图，就把卡图**副本**追加成一个 entry。

| 放什么 | 在哪 | 规则 |
| --- | --- | --- |
| 贴图 | `entries/ORDER.txt`（**v2 语法**，三种行 —— 见下）| 语法与规则写在 ORDER.txt 的文件头；**成员矩形 = 原图自己的像素尺寸**，第 k 个数据行 → entry 7+k / sprite 109+k。源图 `local/` 开头 = 原生贴图（gitignored，构建时取，不入库）|
| 脚本 | `scripts/NN_name.anm.txt`，`NN` 从 68 起连续 | 一个文件一个 `script scriptNN { … }`；正文用 `@NAME` 引用上面的 sprite（图集成员还能按序号写成 `@图集名_k@`，`@图集名@` = 第 0 个成员）|

**`ORDER.txt` 的三种行（v2）**：

1. **单图行** `NAME 源图(相对 assets/) [WxH]` —— 一行 = 一个 entry = 一个 sprite。不写尺寸 = 卡图 256×320；
   写 `WxH` = 整张贴图一个 sprite；源图小于 `WxH` 时**居中补透明**。
2. **图集段** `ATLAS <名字> <画布WxH[@列x行]>`，段内一行一个成员：网格段 `成员名 源图 WxH`、
   显式段 `成员名 源图 @x,y,WxH` ⇒ **整段是一个 entry、里面多个 sprite**（成员按段内顺序拿连续 sprite 号）。
3. **退役行** `REF 名字 目标名` —— 只保 `@名字` 能解析成目标名的 sprite，**不占 entry**。

**★ 2026-09-13 图集合并（现行做法，细节见 [`../../ATLAS-MERGE-AUDIT.md`](../../ATLAS-MERGE-AUDIT.md)）**：

- **为什么**：贴图键 = `(anm 槽号 << 8) | entry 下标`（`th18:0x486d38` / `th18:0x47e1a4`）⇒ **一个 anm 最多 256 entry**。
  我们原来 367 entry（后 111 个画错图）⇒ 把同一张卡的几十张小图并进**一个** entry（引擎原生支持：
  `bullet.anm entry0` 一个 entry 有 368 个 sprite）。现在 **48 entry / 上限 256 ⇒ 余量 208**。
- **成员矩形 = 成员原图自己的像素尺寸**（不补边、不缩放）：VM 的 `scale.x` 乘的是 **sprite 矩形宽**
  （`th18:0x481703`），补边会让缩放"既缩又移"。`build_ability.py` 会硬校验，尺寸不匹配直接报错。
- **只许追加**：`entries/ORDER.frozen.txt` 记下了已定稿的 A/B/C 段（共享 HUD / 别人的卡 / 114–117）的逐行 sha256
  + 结构指纹，`make anm` 每次逐行核对，**挪动 / 删改就报错退出**。新美术一律 **append 到 ORDER.txt 末尾**；
  换图请追加新图集，别改老行的号（`entry = 原生 7 + 行序`）。真要重排得
  `python3 build_ability.py --allow-reorder` + 重新 `--freeze-order N`，并在 `AUDIT.md` §BE 登记。
- ⚠ **DLL 与 `ability.anm` 必须同时发布**（DLL 里烘着 `anm_ids.h` 的宏值）。

**ORDER / id 的并发纪律（2026-09-12 十三修补充；2026-09-13 图集合并后更新）**：`entries/ORDER.txt` 是**只追加**表，
第 k 个数据行决定 `entry 7+k` / `sprite 109+k`，因此任何插入 / 重排都会移动后面所有 sprite。各批生成器**不许改主表**：
`make_animal_art.py` 只 stdout 打印待追加行，`make_youkai_option_art.py` 只整段重写自己的
`entries/ORDER.youkai_option.txt`；主 agent 按追加顺序合并（**图集合并后这批片段文件已并进主表**，
`ORDER.trance.txt` 等只作历史记录，别再往上加东西）。脚本号由 `scripts/NN_*.anm.txt` 的数值连续
编号决定，两批各自占自己的号段（当前 117 = 137–163、108 option = 224–227），互不共享计数器。
`assets/check_ability_anm_bounds.py`（`make anm-verify`）会在最终 `ability.anm` 上复核 117 的脚本 / sprite /
entry（**以及矩形**）对应关系；`sites.py` 现在也会在断点 `cavesize < 5` 时硬失败。

`make_faith_hud_art.py`：110/111 的**组合 UI**（`NEXT.md` §2A）行 2 里那个「×」字形 ——
从 **TH11 `ascii.anm` entry0 的 sprite10**（`(140,0,14,14)`，墨迹 `(4,0,11,8)`）**原样切**成
`faith_hud/MUL.png`（14×14，不裁墨迹、不补边）；ORDER 里是 `ATLAS_FAITH_HUD`（16×16 画布 + 显式矩形
`@0,0,14x14`）的唯一成员，脚本 **275**（「×」VM）/ **276**（倍率 `M.MM` 的字形模板 VM，用 TH18 font 2）。
★ 为什么要借：本仓 `FONT2_*` **没有 `'*'`**（`native/sdk.c` 的 `hud_vm_glyph()` 只有 `0-9 . % + -`），
TH10 `front.anm` 的 sprite40/41/42 也不是乘号 ⇒ 原版只有 TH11 那一行有它。
合并块的块尺寸 / 三行坐标 / 逐项「复刻 vs ❓ 自定」见 `engine/player/th11/04-merged-hud-spec.md`
与 `AUDIT.md` §DB；`--dump` 可把字形打成 ASCII 复核（邻居 11=`+` / 12=`,` / 13=`-` 交叉对名）。

`make_blue_eyes_art.py`：青眼白龙的场上贴图——从 `blue_eyes/_src/` 的黑底俯视图抠底出 256×256 `DRAGON.png`（泛洪掩码 + 边缘亮度斜坡）。光束照魔理沙 Master Spark（`pl01.anm` script22–28）：父脚本 79 起白核 80（blendMode 9）+ 四层彩 81–84（additive、±2.8° 摆动）+ 每 8 帧一颗沿光束飞的彩光 85；**父不转、子各自 `rotate −90°`、彩光直接沿 −y 飞**——不押注子 VM 是否继承父旋转。
`make_broken_core_art.py`：破损核心的三张图 —— `CORE.png`（128×128 电球子机，用户原创 `broken_core/_src/LightningOrb.png`
放大居中）、`BOLT.png`（256×64 黄白闪电链，程序生成、耐横向拉伸：C 把它拉到「电球 → 敌人」那么长）、
卡图画面源图（用户原创 `cards/_src/BROKEN_CORE.png` 铺白底放大居中 → 再过 `fit_card.py`）。
`make_firelord_art.py`：炎魔之王的三张图 —— `RAGNAROS.png`（256×256 本体，用户提供的白底正面像 `firelord/_src/FIRELORD_TOPDOWN.png`
抠白底：四角泛洪 + 边缘按「离白多远」做 alpha 斜坡；script91 缩放 0.5）、`FIREBALL.png`（64×64 火球，程序生成：白芯 → 黄 → 橙 → 红、
边缘按角度噪声抖出火舌，固定种子；script92 本体 / script93 拖尾共用）、`BLAST.png`（128×128 爆炸：内圈闪光 + 外圈光环，script94）。

`make_lolk_ring_art.py`：绀珠传（id 115）的擦弹圈 —— 从 TH15 原版贴图 `local/th15.v1.00b/anm/effect/effect/eff_sloweffect.png`（128×64，两个 64×64）抠**右半的空心环**，LANCZOS 放大到 128×128 → `lolk/RING.png`；ORDER 一行 `LOLK_RING`（128x128），script106 `lolk_ring` 挂在自机位置、C 用 `ce_anm_set_color` 的 alpha 控制显隐。

`make_season_art.py`：季节（id 116）的图 —— `season/SEASON_OPT.png`（64×64 子机本体，script113）与
`season/SEASON_SEED.png`（32×32 子机打出的种子，script114）**程序生成**；
**季节点本体（script115）走原版切图**：从 TH16 `local/th16.v1.00a/anm/bullet/bullet/item.png` 切 5 个
**16×16** 单元 → `season/PT_{SPRING,SUMMER,FALL,WINTER,DOYOU}.png` —— 右上 2×2 = 春/夏（上行）、秋/冬（下行），
再加一行 `(224,32)` = 土用；切片尺寸与 P 点 / 蓝点一致（TH16 用「脚本号 = 129 + SUBSEASON」选，
一手 `th16:0x430cb7 add eax,0x81`）。卡按副季节 `ce_anm_set_sprite` 换图；出场动画照 TH16 `bullet.anm`
script129 **逐条抄**（加算、10 帧淡入、16 帧 2 倍缩到 1 倍、`rotate(%RANDRAD)` + `fsetMul(%F0,%RANDF2,0.08726646f)`
+ `angleVel(0,0,%F0)` = **0..5°/帧 随机自转**、之后 300 帧缩到 0.7 并淡到 alpha128、第 36 帧关加算）。
季节 HUD 三件套也走原生切图 / 矩形：`GLYPH_*.png`（32×32，`front00.png` (832,80)+32k 的五个单元，script117）、
`118_season_bar_frame`（`drawRectBorder(102,10)`）、`119_season_bar_fill`（`drawRect(100,8)`）——
几何照 TH16 `front.anm` script114–117（`pos(-377,888)` 一族 = 游玩区左下角）。
`make_truesilver_art.py`：真银圣剑 HUD 耐久数字 —— `truesilver/DIGIT_0..9.png` 10 张 32×32（DejaVu Serif Bold 白字 2 px 黑描边），ORDER 里 `TS_DIGIT_0..9`；script99 `hud_digit` 与原生 HUD 图标同款 `layer(26); resolutionMode(2)`，C 用 `ce_anm_set_sprite` 换数字。
`make_peashooter_art.py`：豌豆射手的三张图，**全程序生成**（距离场 → 平滑掩码 → 球面着色，无外部素材）——
`peashooter/PEA.png`（32×32 亮绿豌豆，script100 平时那颗 scale 0.80 / script101 开大那些 scale 0.50 共用；
script103 的开大绿光晕也复用它，加色混合胀开）、`peashooter/PEASHOOTER.png`（64×64 子机本体，**朝上**：
绿球 + 上伸的吹管 + 一截茎两片叶）、`peashooter/_src/PEASHOOTER_face.png`（512×640 卡图画面：侧面像斜朝右上 +
三颗飞出去的豌豆，白底 → `fit_card.py --no-detect --trim 0 --margin 0`）。
⚠️ 弹与子机的贴图都必须在 `ability.anm` 里：引擎开火期间把 `player+0x10` 换成它（AUDIT Z7）。

`make_animal_art.py`：**鬼形兽（id 117）的动物灵 / 鱼灵 / 灵槽** —— 源 = TH17 原版
`local/th17.v1.00b/anm/beast/`（`beast1.png` 160×96 的 13 个 32×32 单元：狼/獭/鹫 各 3 帧 + 4 种鱼灵；
`beastf.png` sprite34 = 槽框）。脚本 137–143 **从 `beast.anm` script0–6 生成**（`sprite(spriteN)` → `@NAME`），
咆哮光环 146–148 从 script41–43 生成（`beastAURA.png` 24 个 64×64）、霊撃三色光圈 150–153 从
script33–36 生成（`beastr.png` sprite69）、水獭消弹体 149 手写（`beastot.png` sprite70，80×64 → 128×64 补边），
槽框 / 图标 144/145 手写（layer 25 = 暂停模糊层；**新脚本一律 24/25，别用 26+**，理由见 `HUD-LAYOUT.md` §4.1）；`sprite` 号见 `native/anm_ids.h` 的
`CE_ANM_ABILITY_SPRITE_{WOLF,OTTER,EAGLE}_{0,1,2}` / `FISH_*` / `GAUGE_FRAME`。一手：
[`engine/animal/th17/01`](../../../../../engine/animal/th17/01-animal-spirits.md) §3.2（token→sprite 表）、§4、§7。

`make_font2_art.py`：**TH18 font 2 的字形切图**（`font2/FONT2_{0..9,MINUS,PERCENT,DOT,PLUS}.png`，各 16×20）——
从原生 `local/th18.v1.00a/anm/ascii1280/ascii/ascii_1280.png` 按 `ascii1280.anm.txt` 的精灵矩形切
（数字 = sprite 196..205，`-`/`%`/`.`/`+` = 208/210/211/212，一手映射见
[`engine/anm/th18/03-ascii-text.md`](../../../../../engine/anm/th18/03-ascii-text.md) §2f）。
**连 16×20 的单元一起切**（`.` 的墨迹在单元第 12–17 行，按墨迹裁会把它抬到基线上方）。
ORDER 里是 `ATLAS_FONT2` 的成员、按 `16x32` 声明（**生成器**已把 16×20 居中补成 16×32 的 PNG，
所以「矩形 = 原图尺寸」成立；补边的理由见 `make_font2_art.py`：`sdk.c:690` 的 `+5.0f * sy * dat`
是按「32 高矩形里 20 行字形居中」标定的），脚本 `136_hud_glyph` 逐字起 VM ——
这是「数字也要被暂停模糊罩盖住」的唯一出路（ASCII 文字做不到，见同一篇 §2f 末尾）。
`gen_banner.py "ROYAL FLUSH"`：金色金属光泽 + 黑描边的大字横幅（512×128，`banner/`），皇家同花顺的亮字用它。

`make_ufo_art.py`：**星莲船（id 112）飞碟本体** —— 源 = TH12 原版 `local/th12.v1.00b/anm/enemy/enemy/ufo_anim.png`
（256×256，sprite **205..220** 共 16 个 64×64）。四组同形异色：**205–208 红 / 209–212 蓝 / 213–216 绿 /
217–220 黄**（颜色由可见像素均值自证 ⇒ 第 4 组**是黄的**，不是彩虹色）；输出
`ufo/UFO_{RED,BLUE,GREEN,RAINBOW}_{0..3}.png`（各 64×64），四条脚本 **164–167** 逐条照抄 TH12 `enemy.anm`
**script135..138**（只把 `sprite(...)` 换成 `@名字`；`layer(7)` 是世界层、保持不变）。彩虹脚本 138 的 4 帧
= sprite **205/210/215/220**（各色取一帧）。一手：
[`engine/ufo/th12/01-ufo-system.md`](../../../../../engine/ufo/th12/01-ufo-system.md) §6/§9。
⚠️ 脚本落点是 `ability.anm`，但**ECL 敌机能不能画它还没定案** —— 见 `TUNING.md` §3 的 A1。

`make_ufo_token_art.py`：**星莲船（id 112）令牌**（= 用户口径里的「飞碟」）—— 源 = TH12 原版
`local/th12.v1.00b/anm/bullet/bullet/etama8.png`（256×256）里两行同形异色的 **30×22** 单元：
`y=145` 的 sprite **444/447 红、445/448 蓝、446/449 绿**（固定色令牌 = 道具类型 10/11/12）与
`y=169` 的 **453/456、454/457、455/458**（彩虹令牌用的亮一档版本 = 类型 13/14/15）。
输出 12 张 **32×32**（源单元居中放进 2 的幂画布）→ `ufo/TOKEN_*.png`；六条脚本 **168–173**
从 TH12 `bullet.anm` **script175..180** 生成。
★ **彩虹那三条（178/179/180）的三段 `interruptLabel` 已补齐**：另出 **219–221**
（`{219,220,221}_ufo_token_rainbow_{red,blue,green}.anm.txt`，由 `SCRIPT_FULL_PORTS` 生成，
生成时断言「恰好 3 段、依次 3/2/7、标签 7 下一条是该色固定色 sprite」），168–173 一字未改。
送的时机已查明 = **玩家距离 + 换色计时器**驱动（不是「被打掉 / 状态转换」）：
标签 `3` = 离开 60 px 圈且计时器 `<90`、`2` = 计时器 `==90` 或离开圈时 `>=90`、
`7` = 进入 60 px 圈那一帧。卡侧用**已存在**的 `ce_anm_interrupt(id, n)` 送（`sdk.h`，无新 SDK 面）；
**彩虹令牌每 150 帧换色**用 `ce_anm_delete` + `ce_anm_spawn(新脚本)` 等价达成
（一手 TH12 是同一 VM 原地换脚本）。一手：TH12 `ItemManager__on_tick__body` 令牌分支 `0x426276`–`0x4264f6`、
60 px 圈常量 `0x4a4410` = `3600.0`、150 帧出处 `0x426483 cmp eax,0x96`。
一手：[`engine/ufo/th12/01-ufo-system.md`](../../../../../engine/ufo/th12/01-ufo-system.md) §9。

`223_ufo_pentagon.anm.txt`：**112 的五边形提示框** —— TH12 v1.00b `bullet.anm` **script211** 逐条照抄
（`sprite(-1)` + `drawPolyBorder(1.0f, 5)` ⇒ **引擎程序画的多边形，本条目不需要贴图、也没有生成器**）。
⚠ 层号照抄原生 **`layer(12)`**（**不是** HUD 的 25 —— 框是跟着令牌在游玩区动的**场上实体**；
见 `AUDIT.md` §BO-7）。触发条件一手 = `th12:0x425c20` 的令牌分支（已收 < 3 枚 + 当前令牌「再来一枚即能成组」），
**框的颜色 = 收了这一枚会得到的那只 UFO 的颜色**。

`make anm` 跑 `build_ability.py`：重编 → `native/build/ability.anm`，自检原生部分没变，
并生成 `native/anm_ids.h`（`CE_ANM_ABILITY_SPRITE_<NAME>` / `CE_ANM_ABILITY_SCRIPT_<name>`），C 里用它起脚本：

```c
ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_REVERSE_FLASH, 16);   /* 场地中央 */
```

写脚本参照 `local/th18.v1.00a/anm/ability/ability.anm.txt` 里的原生脚本和 thpages 的指令表；
`type(8)` 是三维渲染模式，`rotateTime(t, mode, rx, ry, rz)` 绕 Y 轴转就是翻牌。
`type(8)` 照原生写在层 20：`layer(20); resolutionMode(1); type(8);`，不写 `originMode`；层 20–23 由相机 1 画、不写不测深度
（`engine/anm/th18/02-render-stages.md`）。层 12–19（相机 3）理论也能放，未实测。模式 8 的锚点表现为顶边对齐，`pos` 的 y 要上移半张卡高。
实体坐标 (0,0,0)（`ce_anm_spawn` 默认）= **弹幕区上边框中点**（ECL 坐标 x 居中、y 从顶部起算），场地正中要 `pos(0, 224, 0)`。
