# HANDOFF-109：TH09 左上角 HUD（`front.png`）切图清单 + 画法

> **版本**：跨版本 —— 素材取自 `th09` v1.50a（`front.anm` / `front.png`），落地到 `th18` v1.00a 的卡 109。
> 地址/矩形出自一手：`engine/versus/th09/01-versus-loop.md` §2.9–§2.11（本文只重复「怎么切/怎么画」要用的部分）。
> 交给**负责切图的 agent**；卡侧代码不在本文范围。

## 0. 素材来源（两条路，等价 ✓）

| 路 | 位置 |
| --- | --- |
| ★ **本仓样本（推荐 ✓ 可复现）** | `local/th09.v1.50a/th09.dat` 里的 **`front.anm`**（1194104 B ✓）|
| owner 实机（已解包 ✓）| `…\[th09] 东方花映塚…\data\front.anm` ＋ `data/ANM/data/front/front.png`（512×512 ✓）|

```bash
# 取 anm（本仓样本）
thdat -x 9 local/th09.v1.50a/th09.dat front.anm        # → ./front.anm
# 把它引用的贴图导出来（entry0 = data/front/front.png ✓；entry1 = 名牌 mname.png ✓）
thanm -x 9 -u front.anm                                # → data/front/front.png …
# 看 sprite 表（切图就照这张表 ✓）
thanm -l 9 front.anm
```

## 1. 切图清单（**最小可用集** = 框 + 数字 + 单位符 + 图标 ✓）

| 目标文件名 | 源 sprite | rect `x,y,w,h` | 用途 |
| --- | --- | --- | --- |
| `spellbox.png` | **7** | `320,0,80,32` | ★ **「Spell Point」牌 + 数字空框**（= 那个框 ✓）|
| `digit_g0.png` … `digit_g9.png` | **8–17** | `320+(i-8)*8, 32, 8, 11` | 灰白小数字 0–9 ✓ |
| `digit_r0.png` … `digit_r9.png` | **25–34** | `320+(i-25)*8, 48, 8, 11` | 红小数字 0–9 ✓（第二套）|
| `slash.png` | **18** | `400,32,16,10` | 数字行尾单位/斜杠 ✓ |
| `icon0.png` … `icon4.png` | 37／36／35／38／39 | `400,0` / `416,0` / `432,0` / `400,16` / `416,16`（16×16 ✓）| 状态图标（圆点×2／空圈／×／樱 ✓）|

**可选（做整块 HUD 才要）**：`bar_top0..3.png` = sprite 0–3（`0,0/16/32/48,320,16` ✓）、`side_l.png` = sprite4（`496,0,16,448` ✓）、`side_r.png` = sprite5（`480,0,16,448` ✓）、`panel.png` = sprite6（`448,0,32,448` ✓）、名牌 sprites 21–24（`256,96+16k,112,16` ✓）。

**裁切规则（照抄 ✓）**：**1:1 直裁、不缩放** ✓、保留原 PNG 的 alpha ✓、一块一张文件 ✓、文件名用上表（大写 sprite 名可加前缀 `TH09_HUD_` ✓）。

## 2. 放哪 + 登记（TH18 卡 109）

1. PNG ⇒ `assets/ability/th09front/<上面那些名字>.png` ✓（新目录）；
2. `assets/ability/entries/ORDER.txt` **文件末尾追加**（⚠ 仓规：**不许插已定稿段** —— 插行会平移所有 sprite 号，DLL 与 `ability.anm` 必须同批 ✓）：

```
# 109 TH09 左上角 HUD（front.png 直裁；rect 见 engine/versus/th09/01-versus-loop.md §2.11）
ATLAS ATLAS_TH09_HUD 512x512
TH09_HUD_SPELLBOX    ability/th09front/spellbox.png    @0,0,80x32
TH09_HUD_DIGIT_G0    ability/th09front/digit_g0.png    @0,0,8x11
TH09_HUD_DIGIT_G1    ability/th09front/digit_g1.png    @8,0,8x11
…（G2..G9 依次 @16,0 / @24,0 …；R0..R9；SLASH；ICON0..4）
```
3. 生成器会把这批写成 `native/anm_ids.h` 的 `CE_ANM_ABILITY_SPRITE_TH09_HUD_*` ✓（卡里用这个宏，不写裸号 ✓）。

## 3. 怎么画（一手几何，来自 `front.anm` 的 script7 / script8 / script9… ✓）

| 元素 | 一手参数（`front.anm`）|
| --- | --- |
| **框** `script7` | `sprite(sprite7)`；**`pos(-144, 0)`**；`alpha(0)` → `+60: alphaTime(60,0,255)`（**60 帧淡入** ✓）；`op_30(1)`；然后每 60 帧循环 ✓ |
| 单位符 `script8` | `sprite(sprite18)`；`pos(-121, 13)`；**`color(192,128,128)`** ✓；同样 60 帧淡入 ✓ |
| 数字 `script9..18` | `sprite(sprite8)`（**数字贴图由游戏逐帧/逐位覆写** ✓）；`pos(-139 + 8·k, 13)`（**8 px 步进** ✓）；`color(192,128,128)` ✓；60 帧淡入 ✓ |

**卡侧（TH18）落地建议**
* **框**：`ce_anm_spawn(CE_ABILITY_ANM(), <框脚本>, <世界层/UI 层>)` ＋ `ce_anm_set_sprite(vm, CE_ANM_ABILITY_SPRITE_TH09_HUD_SPELLBOX)` ✓（80×32 一块就够 ✓，不需要九宫格 ✓）；
* **数字**：优先 `ce_hud_number_set(&n, x, y, spacing, value, digits, 0, tag)` ✓（= 逐位 VM，天然对上 TH09 的"每位一台 VM" ✓，`spacing = 8` ✓）；
  想 1:1 复刻淡入/tint ⇒ 用逐字 VM（`ce_hud_vm_text_set_font` ✗ 字形集不同 ⇒ 更稳的是自己 spawn 多台 VM ✓ 每台 `set_sprite(digit_XX)` ＋ `ce_anm_set_color` ✓）；
* **顺序**：**先框、后数字** ✓（原版就是框在底、数字写在框里 ✓）；
* **坐标空间**：`ce_hud_*` 系是 **640×480** ✓；TH09 的 `front.anm` 坐标也是 **640×480** 系 ✓（旁证：`script0 pos(0,0)` / `script1 pos(320,0)` / `script5 pos(624,16)` ✓），但 HUD 那几台的 **x 是负的**（`-144` / `-139`）⇒ ⇒ 它们是**相对锚点**的偏移 ✓；**锚点 = 由游戏按侧设置的 HUD 原点** ✓（🟡 半场偏移的确切值要在实机里对一次：TH18 单人宿主下建议以"左上角留边 ≈ 16 px"起手，再按实机微调 ✓）；
* **颜色**：灰白行 = 原图 ✓；红行 = 原图第二套 ✓（也可以用 `ce_anm_set_color(192,128,128)` 染 ✓，两者等价 ✓）。

## 4. 纪律（照仓库规矩 ✓）

* 只**追加** `ORDER.txt` ✓；改完必须 `make anm`（重建 `ability.anm`）⇒ **DLL 与 `ability.anm` 同批发布** ✓（`local/DLL-SYNC.md` ✓）；
* 数值/几何若与一手不同（例如你决定改淡入帧数、改步进），必须在注释里标 **❓自定** ✓，不许写成"复刻" ✗；
* 切完请回报：新 sprite 号（`anm_ids.h` 里的 `CE_ANM_ABILITY_SPRITE_TH09_HUD_*` ✓）＋ 是否与 `ability.anm` 同批 ✓。
