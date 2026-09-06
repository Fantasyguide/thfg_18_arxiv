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

`make_truesilver_art.py`：真银圣剑 HUD 耐久数字 —— `truesilver/DIGIT_0..9.png` 10 张 32×32（DejaVu Serif Bold 白字 2 px 黑描边），ORDER 里 `TS_DIGIT_0..9`；script99 `hud_digit` 与零售 HUD 图标同款 `layer(26); resolutionMode(2)`，C 用 `ce_anm_set_sprite` 换数字。
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
