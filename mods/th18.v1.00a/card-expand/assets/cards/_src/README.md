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
| `ELYTRA_MACE.png` | 鞘翅重锤（id 91）的卡面 | **用户本机 Minecraft 1.21.10 客户端**：`<MC>/.minecraft/versions/1.21.10/1.21.10.jar` → `assets/minecraft/textures/item/mace.png`（16×16 调色板 PNG）| `make_white_item_card.py ELYTRA_MACE --item _src/ELYTRA_MACE.png`：白底 + **整数倍**放大（像素材质不插值）+ 原生框 |
| `ICE_CREAM.png` | 冰淇淋（id 90）卡面的**原件** | **用户本机已解包的《杀戮尖塔 1》**：`<Steam>/steamapps/common/SlayTheSpire/desktop-1.0/images/largeRelics/iceCream.png`（256×256，调色板 PNG）| 不直接进卡面（遗物原图四周留了大量透明边）|
| `ICE_CREAM_RELIC.png` | 同上，**裁掉透明边** | 由上一张按 alpha 包围盒裁出（`(102,72,160,186)` ⇒ **58×114**）| `make_white_item_card.py ICE_CREAM --item _src/ICE_CREAM_RELIC.png --fill`：白底 + 等比放大到画面区上限 + 原生框 |
| `TONY_GRIEF.png` | 东尼的悲恸（id 86）卡面的**原件** | **用户提供**（`D:\项目\幻想指南\18\th18_modkit\d3bd0a27cffc1e17610100340c90f603738de914.png`，850×1072，透明底《杀戮尖塔》「送死亡」整卡：**左上角凸出一块能量六边形**、两侧还有横幅飞边）| 不直接进卡面（下面那张是裁出来的卡体）|
| `TONY_GRIEF_CARD.png` | 同上，**卡体矩形** | 由上一张裁出（一次性：按 alpha 逐行/逐列遮盖 ≥ 70% 峰值取卡体 = `(72,53,771,1042)` ⇒ **699×989**，凸出的六边形与横幅飞边都在这个阈值之外）| `fit_card.py TONY_GRIEF cards/_src/TONY_GRIEF_CARD.png --no-detect --trim 0 --margin 0 --fill`：卡体正好铺满 256×320（横向 4:5，13% 拉伸），再套原生框 ⇒ 卡体矩形与卡面**对齐** |
| `ZENITH.png` | 天顶剑（id 87）的卡面 | **Terraria 原版物品**（用户本机 Steam 版 `Content/Images/Item_4956.xnb`，56×56；物品号 4956，**不是** 4144） | `make_white_item_card.py ZENITH --item _src/ZENITH.png --fill`：白底 + 物品缩到画面区上限（LANCZOS）+ 原生框 |
| `STAR_CANNON.png` | 星星炮（id 89）的卡面 | **Terraria 原版物品**（`Content/Images/Item_197.xnb`，52×18；`ItemID.StarCannon = 197`）| `make_white_item_card.py STAR_CANNON --item _src/STAR_CANNON.png --fill`：白底 + 物品缩到画面区上限 + 原生框 |
| `JERA.png` | 杰拉（id 88）的卡面 | **用户提供**（`D:\项目\幻想指南\18\th18_modkit\Runes_sprite.png`，256×256，透明底紫色辉光的卢恩字母 ᛃ = Jera 符文）| `fit_card.py JERA _src/JERA.png --trim 0 --margin 24 --bg '#ffffff'`：按非白包围盒等比缩到画面区、白底、套原生框 |
| `ZENITH_FIRST_FRACTAL.png` | （只作记录，不进卡面） | 同上，`Item_4144.xnb`（32×32） | **4144 是「地灵之刃 Terragrim」，不是天顶剑**；它只是天顶剑剑阵的 21 把之一（见 `ability/zenith/_src/`）|

### Terraria 物品贴图的取法（LZX 解包，一次性）

Terraria 的 `Content/Images/Item_<id>.xnb` 是 **XNB（LZX 压缩）**，需要一个能解 LZX 的工具。
本仓的做法（2026-10-02 实测）：

```bash
git clone --depth 1 https://github.com/LeonBlade/xnbcli            # Node 工具，纯 JS 的 LZX
cd xnbcli && node -e "const p=require('./package.json'); delete p.dependencies.lz4; require('fs').writeFileSync('package.json',JSON.stringify(p,null,2))"
npm install                                                        # lz4 是原生模块、编不过；Terraria 只走 LZX(0x80) 所以删掉它即可
mkdir -p node_modules/lz4 && printf '{"name":"lz4","version":"0.6.5","main":"index.js"}' > node_modules/lz4/package.json
echo "module.exports={decode(){throw 0},encode(){throw 0}};" > node_modules/lz4/index.js   # 占位，仅为过 require
cp "<Terraria>/Content/Images/Item_4956.xnb" packed/ && node xnbcli.js unpack ./packed ./unpacked
```

产物 `unpacked/Item_4956.png` 即上表那一张。**物品号怎么定**（别猜）：
- 天顶剑本体 = **4956**（一手：`Item.cs` 的 `shoot = 933` + `GetAlpha` 的 `case 4956`；wiki 的 Item ID 栏同值）；
- 剑阵的 21 个物品号 = `FinalFractalHelper..cctor` 的 IL（本机 exe 反出来的，见 `native/cards/zenith.c`
  的表与 `AUDIT.md` §BE 详录 #211），它们对应的贴图在 `assets/ability/zenith/_src/sword_<id>.png`。

### Terraria 音效的取法（同样是 xnbcli，`SoundEffect` 这一类它支持）

```bash
cd <xnbcli 目录>
cp "<Terraria>/Content/Sounds/Item_9.xnb" packed/ && node xnbcli.js unpack ./packed ./unpacked
# → unpacked/Item_9.wav（立体声 44.1k/16-bit）。**原件放 local/**（版权资产不入库）：
mkdir -p <repo>/local/terraria/dat && cp unpacked/Item_9.wav <repo>/local/terraria/dat/
# 之后走本仓既有的现取通道（**逐字节拷** + STAR_CANNON_SHOT 那条的纯增益归一化例外）：
python3 assets/voice/derive_retail_se.py STAR_CANNON_SHOT
python3 assets/build_voice.py
```

⚠ **不要**把 wav 直接提交进 `assets/voice/`：那是版权资产，`.gitignore` 里按先例单独忽略
（`STAR_CANNON_SHOT.wav` 与 ASTRALUP / UFOALERT / YOUKAI_FAM_SPAWN 同一档）。名字进 `assets/voice/ORDER.txt`
（一行 = 一个新 id）、`patch/th18/voice.js` 里登记同号（id 显式写死，别靠顺序）。

一手出处：星星炮的 `UseSound = SoundID.Item9`（`Item.cs` 的 `case 197:`）⇒ `Item_9.xnb`。

### Minecraft 物品贴图的取法（客户端 jar，无需任何工具）

```bash
python3 - <<'EOF'
import zipfile, os
jar = '<MC>/.minecraft/versions/1.21.10/1.21.10.jar'      # 1.21 才有 mace
z = zipfile.ZipFile(jar)
for n in ('assets/minecraft/textures/item/mace.png',
          'assets/minecraft/textures/item/elytra.png',
          'assets/minecraft/textures/item/wind_charge.png'):
    open(os.path.join('assets/ability/elytra_mace/_src', os.path.basename(n)), 'wb').write(z.read(n))
EOF
```

贴图是 16×16 的**调色板 PNG**（`mode=P`）⇒ 用 PIL 读的时候记得 `.convert("RGBA")`。
