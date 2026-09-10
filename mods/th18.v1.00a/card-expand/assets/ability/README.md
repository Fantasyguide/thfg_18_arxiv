# assets/ability — `ability.anm` 追加（卡牌场上特效）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

`ability.anm` 是主动卡场上特效的家（零售 7 张贴图 / 68 个脚本，Tenshi 的要石是 script28）。
脚本只能用同文件里的 sprite，所以要在特效里露出卡图，就把卡图**副本**追加成一个 entry。

| 放什么 | 在哪 | 规则 |
| --- | --- | --- |
| 贴图 | `entries/ORDER.txt`：`NAME  源图(相对 assets/)  [WxH]` | 不写尺寸 = 卡图 256×320；写 `WxH`（2 的幂）= 整张贴图一个 sprite（横幅等）；源图小于 WxH 时居中补透明。**源图 `local/` 开头 = 零售贴图**（gitignored，构建时取，不入库；青眼的光束用魔理沙 `pl01b` / `pl01b2`）。只追加不重排。第 k 行 → entry 7+k / sprite 109+k |
| 脚本 | `scripts/NN_name.anm.txt`，`NN` 从 68 起连续 | 一个文件一个 `script scriptNN { … }`；正文用 `@NAME` 引用上面的 sprite |

`make_blue_eyes_art.py`：青眼白龙的场上贴图——从 `blue_eyes/_src/` 的黑底俯视图抠底出 256×256 `DRAGON.png`（泛洪掩码 + 边缘亮度斜坡）。光束照魔理沙 Master Spark（`pl01.anm` script22–28）：父脚本 79 起白核 80（blendMode 9）+ 四层彩 81–84（additive、±2.8° 摆动）+ 每 8 帧一颗沿光束飞的彩光 85；**父不转、子各自 `rotate −90°`、彩光直接沿 −y 飞**——不押注子 VM 是否继承父旋转。
`make_broken_core_art.py`：破损核心的三张图 —— `CORE.png`（128×128 电球子机，用户原创 `broken_core/_src/LightningOrb.png`
放大居中）、`BOLT.png`（256×64 黄白闪电链，程序生成、耐横向拉伸：C 把它拉到「电球 → 敌人」那么长）、
卡图画面源图（用户原创 `cards/_src/BROKEN_CORE.png` 铺白底放大居中 → 再过 `fit_card.py`）。
`make_firelord_art.py`：炎魔之王的三张图 —— `RAGNAROS.png`（256×256 本体，用户提供的白底正面像 `firelord/_src/FIRELORD_TOPDOWN.png`
抠白底：四角泛洪 + 边缘按「离白多远」做 alpha 斜坡；script91 缩放 0.5）、`FIREBALL.png`（64×64 火球，程序生成：白芯 → 黄 → 橙 → 红、
边缘按角度噪声抖出火舌，固定种子；script92 本体 / script93 拖尾共用）、`BLAST.png`（128×128 爆炸：内圈闪光 + 外圈光环，script94）。

`make_lolk_ring_art.py`：绀珠传（id 118）的擦弹圈 —— 从 TH15 零售贴图 `local/th15.v1.00b/anm/effect/effect/eff_sloweffect.png`（128×64，两个 64×64）抠**右半的空心环**，LANCZOS 放大到 128×128 → `lolk/RING.png`；ORDER 一行 `LOLK_RING`（128x128），script106 `lolk_ring` 挂在自机位置、C 用 `ce_anm_set_color` 的 alpha 控制显隐。

`make_season_art.py`：季节（id 119）的图 —— `season/SEASON_OPT.png`（64×64 子机本体，script113）与
`season/SEASON_SEED.png`（32×32 子机打出的种子，script114）**程序生成**；
**季节点本体（script115）走零售切图**：从 TH16 `local/th16.v1.00a/anm/bullet/bullet/item.png` 切 5 个
**16×16** 单元 → `season/PT_{SPRING,SUMMER,FALL,WINTER,DOYOU}.png` —— 右上 2×2 = 春/夏（上行）、秋/冬（下行），
再加一行 `(224,32)` = 土用；切片尺寸与 P 点 / 蓝点一致（TH16 用「脚本号 = 129 + SUBSEASON」选，
一手 `th16:0x430cb7 add eax,0x81`）。卡按副季节 `ce_anm_set_sprite` 换图；出场动画照 TH16 `bullet.anm`
script129 **逐条抄**（加算、10 帧淡入、16 帧 2 倍缩到 1 倍、`rotate(%RANDRAD)` + `fsetMul(%F0,%RANDF2,0.08726646f)`
+ `angleVel(0,0,%F0)` = **0..5°/帧 随机自转**、之后 300 帧缩到 0.7 并淡到 alpha128、第 36 帧关加算）。
季节 HUD 三件套也走零售切图 / 矩形：`GLYPH_*.png`（32×32，`front00.png` (832,80)+32k 的五个单元，script117）、
`118_season_bar_frame`（`drawRectBorder(102,10)`）、`119_season_bar_fill`（`drawRect(100,8)`）——
几何照 TH16 `front.anm` script114–117（`pos(-377,888)` 一族 = 游玩区左下角）。
`make_truesilver_art.py`：真银圣剑 HUD 耐久数字 —— `truesilver/DIGIT_0..9.png` 10 张 32×32（DejaVu Serif Bold 白字 2 px 黑描边），ORDER 里 `TS_DIGIT_0..9`；script99 `hud_digit` 与零售 HUD 图标同款 `layer(26); resolutionMode(2)`，C 用 `ce_anm_set_sprite` 换数字。
`make_peashooter_art.py`：豌豆射手的三张图，**全程序生成**（距离场 → 平滑掩码 → 球面着色，无外部素材）——
`peashooter/PEA.png`（32×32 亮绿豌豆，script100 平时那颗 scale 0.80 / script101 开大那些 scale 0.50 共用；
script103 的开大绿光晕也复用它，加色混合胀开）、`peashooter/PEASHOOTER.png`（64×64 子机本体，**朝上**：
绿球 + 上伸的吹管 + 一截茎两片叶）、`peashooter/_src/PEASHOOTER_face.png`（512×640 卡图画面：侧面像斜朝右上 +
三颗飞出去的豌豆，白底 → `fit_card.py --no-detect --trim 0 --margin 0`）。
⚠️ 弹与子机的贴图都必须在 `ability.anm` 里：引擎开火期间把 `player+0x10` 换成它（AUDIT Z7）。

`make_font2_art.py`：**TH18 font 2 的字形切图**（`font2/FONT2_{0..9,MINUS,PERCENT,DOT,PLUS}.png`，各 16×20）——
从零售 `local/th18.v1.00a/anm/ascii1280/ascii/ascii_1280.png` 按 `ascii1280.anm.txt` 的精灵矩形切
（数字 = sprite 196..205，`-`/`%`/`.`/`+` = 208/210/211/212，一手映射见
[`engine/anm/th18/03-ascii-text.md`](../../../../../engine/anm/th18/03-ascii-text.md) §2f）。
**连 16×20 的单元一起切**（`.` 的墨迹在单元第 12–17 行，按墨迹裁会把它抬到基线上方）。
ORDER 里按 `16x32` 声明（构建脚本把 16×20 居中补透明），脚本 `136_hud_glyph` 逐字起 VM ——
这是「数字也要被暂停模糊罩盖住」的唯一出路（ASCII 文字做不到，见同一篇 §2f 末尾）。
`gen_banner.py "ROYAL FLUSH"`：金色金属光泽 + 黑描边的大字横幅（512×128，`banner/`），皇家同花顺的亮字用它。

`make anm` 跑 `build_ability.py`：重编 → `native/build/ability.anm`，自检零售部分没变，
并生成 `native/anm_ids.h`（`CE_ANM_ABILITY_SPRITE_<NAME>` / `CE_ANM_ABILITY_SCRIPT_<name>`），C 里用它起脚本：

```c
ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_REVERSE_FLASH, 16);   /* 场地中央 */
```

写脚本参照 `local/th18.v1.00a/anm/ability/ability.anm.txt` 里的零售脚本和 thpages 的指令表；
`type(8)` 是三维渲染模式，`rotateTime(t, mode, rx, ry, rz)` 绕 Y 轴转就是翻牌。
`type(8)` 照零售写在层 20：`layer(20); resolutionMode(1); type(8);`，不写 `originMode`；层 20–23 由相机 1 画、不写不测深度
（`engine/anm/th18/02-render-stages.md`）。层 12–19（相机 3）理论也能放，未实测。模式 8 的锚点表现为顶边对齐，`pos` 的 y 要上移半张卡高。
实体坐标 (0,0,0)（`ce_anm_spawn` 默认）= **弹幕区上边框中点**（ECL 坐标 x 居中、y 从顶部起算），场地正中要 `pos(0, 224, 0)`。
