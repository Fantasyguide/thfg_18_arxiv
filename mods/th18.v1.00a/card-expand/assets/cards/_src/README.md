# _src — 卡图用到的第三方 / 原始素材（原件 + 出处）

| 文件 | 用在 | 来源 | 处理 |
| --- | --- | --- | --- |
| `reverse_card.png` | REVERSE（反转牌，id 64） | 用户提供（UNO 反转牌图，2304×3500） | `fit_card.py REVERSE cards/_src/reverse_card.png --fill`：去白边、上下各修 3%、横向拉满 256（两侧不留白边）、白底 |

| `POT_OF_GREED.png` | 强欲之壶（id 63） | 用户原创（1122×1402，4:5 满版） | `fit_card.py POT_OF_GREED … --no-detect --trim 0 --margin 0`：1:1 缩到 256×320 |
| `JUDGMENT.png` | 神之宣告（id 66） | 同上 | 同上 |
| `BLUE_EYES.png` | 青眼白龙（id 67） | 用户原创（1122×1402，白底立绘） | 同上（`--no-detect --trim 0 --margin 0`）。场上的龙用另一张俯视图：`ability/blue_eyes/_src/BLUE_EYES_TOPDOWN.png`（黑底），`ability/make_blue_eyes_art.py` 抠黑底出 256×256 |
| `BROKEN_CORE.png` | 破损核心（id 71） | 用户原创（156×156，透明底青色裂核，自带柔和投影） | `ability/make_broken_core_art.py` 裁 alpha 包围盒、放大到画面宽 78％、居中铺白底 → `fit_card.py BROKEN_CORE … --no-detect --trim 0 --margin 0 --bg '#ffffff'`。场上电球另见下段 |
| `FIRELORD.png` | 炎魔之王拉格纳罗斯（id 72） | 用户提供（500×654 立绘，满版） | `fit_card.py FIRELORD … --no-detect --trim 0 --margin 0 --fill`：横向拉 4％ 到 4:5（不留白边）。场上本体用另一张正面像：`ability/firelord/_src/FIRELORD_TOPDOWN.png`（白底），`ability/make_firelord_art.py` 抠白底出 256×256 |
| `GUDING.png` | 古锭刀（id 76） | 用户提供（三国杀原卡整图，320×436） | `fit_card.py GUDING … --no-detect --trim 0 --margin 0`：等比覆盖 4:5，上下各裁约 4％ |
| `WHIRLWIND_face.png` | 旋风斩（id 79） | 用户提供（妖梦挥刀，1426×1103 横图，原件 `_art/旋风斩.png`） | **先裁人像再入库**，见下「旋风斩的裁法」 |
| `OUROBOROS_face.png` | 衔尾蛇（id 78） | 用户提供（黄色衔尾蛇，1254×1254 方图，原件 `_art/衔尾蛇.png`） | **方图补成 4:5**：上下各补白到 1254×1568 居中。⚠️ 填充色用原图自己的底色 **(254, 254, 254)** 而不是纯白 —— 那张图的底不是 `#ffffff`，用纯白会在接缝处留一道看得见的边。然后 `fit_card.py OUROBOROS … --no-detect --trim 0 --margin 0 --bg '#fefefe'` |
| `english_pattern/*.svg` | 黑桃 10/J/Q/K/A（id 58–62） | Wikimedia Commons，Dmitry Fomin，CC0 | 见该目录 README：渲染 640 高 → `fit_card.py --no-detect --trim 0 --bg '#ffffff'`（两侧白底填充） |

破损核心场上的电球是另一张用户原创：`ability/broken_core/_src/LightningOrb.png`（70×70 黄绿球），
同一个脚本按 alpha 包围盒放大到 128×128 出 `ability/broken_core/CORE.png`。

现成卡图一律走 `fit_card.py`，别手工缩。

**旋风斩的裁法**：原图是横构图，人物偏左，直接等比缩会把人缩得很小。所以先裁成 4:5 再入库：
`crop((224, 118, 1012, 1103))` = 788×985。人物中线由绿裙的包围盒（`x 332..904`、`y 376..922`）定，
上取到发带、下取到脚；再按**卡框每边吃掉的 7％** 把这个构图外扩一圈，免得发带和鞋被框边切掉。
之后 `fit_card.py WHIRLWIND … --no-detect --trim 0 --margin 0`。
