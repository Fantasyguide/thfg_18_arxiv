#!/usr/bin/env python3
"""神灵庙（id 113）的**神灵实体**与**灵界槽**贴图 —— 全部从 TH13 原版图切。

    python3 make_trance_art.py

======================================================================================
一手考证（这一节是文件头要求的「先考证再动手」，结论都在 `local/th13.v1.00c/`）
======================================================================================

■ 用哪几个 littlegods sprite、脚本怎么动 ✅（一手：`thanm -l 13` 解出 `astral.anm`，
  脚本正文见下；`astral.anm` 共 1 个 entry 组（2 entry）+ 25 个脚本 0..24）

  `astral.anm` = **effect/eff_astral.png**（entry0，512×512，灵界背景）+
  **enemy/littlegods.png**（entry1，256×64，10 个 **30×30** sprite，矩形全带 1px 边距）：

      sprite1  (1,1)    sprite2  (1,33)     ← 普通态的「外环」「内芯」
      sprite3  (33,1)   sprite7  (33,33)    ← type 0 灵界态（白环 / 品红芯）
      sprite4  (65,1)   sprite8  (65,33)    ← type 1 灵界态
      sprite5  (97,1)   sprite9  (97,33)    ← type 2 灵界态
      sprite6  (129,1)  sprite10 (129,33)   ← type 3 灵界态

  **10 个 sprite 一个不多一个不少**，分配如下（一手，逐个脚本读出来的）：

  | 脚本 | 谁 | sprite | layer | 说明 |
  | --- | --- | --- | --- | --- |
  | 0 | 灵界背景（eff_astral）| sprite0 | — | 不属于本次切图 |
  | 1, 7, 13, 19 | 神灵**普通态父**（type 0/1/2/3）| 不画（只 scriptNew 两个子）| 6 | `alpha(0); scale(1.5)`（1/13/19）/ `scale(1.0)`（7）|
  | 2, 8, 14, 20 | └ **外环** | **sprite1** | 6 | `color()` 四型不同（绿 / 橄榄 / 紫 / 灰），blendMode 2 |
  | 3, 9, 15, 21 | └ **内芯** | **sprite2** | 6 | `color()` 四型不同（品红 / 蓝 / 绿 / 灰），blendMode 1 |
  | 4, 10, 16, 22 | 神灵**灵界态父**（type 0/1/2/3）| 不画 | 13 | 直接 `scriptNew` 两个子 |
  | 5, 11, 17, 23 | └ **白环** | **sprite3/4/5/6** | 13 | 无 `color()`（贴图自带白），blendMode 1 |
  | 6, 12, 18, 24 | └ **glow** | **sprite7/8/9/10** | 13 | blendMode 1 + `angleVel(0,0,0.05235988f)` = **3°/帧 自转** |

  ⇒ **普通态共用 sprite1/sprite2、靠 `color()` 区分四型；灵界态各型各一对 sprite、不染色**。
  这正是引擎侧 `script = type*6 + 1`（普通）/ `type*6 + 4`（灵界）的由来
  （`engine/spirit/th13/01-spirit-and-trance.md` §2.3，一手 `th13:0x43907e`）。

■ 生命周期 / 淡出 ✅（一手 `astral.anm` script1 原文逐字）

      script1:  -1: alpha(0); scale(1.5); layer(6);
                  0: scriptNew(script2); scriptNew(script3);
               +200: scaleTime(200, 0, 0.4, 0.4); stop(); interruptLabel(1);
               +240: delete();
      script2/3（外环 / 内芯）:  -1: …; 0: 20 帧 1.5→1.6 呼吸 / +20: 20 帧 →1.5，jmp 循环
                              interruptLabel(1): scaleTime(40, 1, 0.0, 0.0); +40: delete();

  ⇒ **寿命 200 帧后开始淡出（scale 1.5→0.4 用 200 帧，模式 0），第 240 帧 delete()**。
  ⚠️ 这里有个**与引擎文档的一手差异，以 anm 原文为准**：`SPIRIT:59` 把 `timer_1 == 480` 记成
  「寿命 → 发中断 1」，但 anm 侧的中断 1 在 **第 240 帧**就已经 `delete()`；480 更像是引擎侧
  的一个独立计时（发中断 + 清 flag bit1）。**本次只做原生切图 + anm 脚本，不碰引擎计时**，
  所以脚本按 anm 原文的 200/240 写；480 那一路留给卡侧 C 决定（见 §5 未做到）。
  另外脚本 7/19 的父是 `scale(1.0)`、脚本 13 是 `scale(1.5)`、脚本 1 是 `scale(1.5)`
  —— 父的初缩放**逐型不同**（一手逐条读出），本文件的脚本照抄不统一。

■ 灵界槽（HUD）✅（一手 `thanm -l 13` 解出 `front.anm` script171 + 子 172/173/174/175）

  | 脚本 | sprite | 源图 / 矩形 | 锚点 / 位置 | 引擎每帧写什么 |
  | --- | --- | --- | --- | --- |
  | 171（根）| 不画 | — | `layer(17); pos(-196, 448)` | — |
  | 172 | **sprite121/122** | `front/agauge1.png` (0,0,32,48) / (32,0,32,48) | `anchor(1,2); pos(0,4)` | `interrupt 3`（槽 <200）切到 sprite122 |
  | 173 | **sprite123** | `front/agauge2.png` (0,0,48,32) | `anchor(1,2); pos(24,0)` | `scale.x = uv_scale.x = (槽/200)/3.0` |
  | 174 | **sprite124** | `front/agauge2.png` (0,32,48,32) | `anchor(1,2); pos(24,0)` | **引擎不写**（只靠 interruptLabel 4/5 淡入淡出）|
  | 175 | **sprite125** | `front/agauge2.png` (0,64,16,32) | `anchor(1,2); pos(24,0)` | `scale.x = uv_scale.x = (槽%200)/200`；`entity_pos.x = 比例 × 16.0` |

  `front/agauge2.png` **不是 16×16**：文件是 **16 宽 × 128 高**、`THTXHeight: 96`，
  三个 sprite 的 w 比文件宽 —— 这是「sprite 矩形可越界取到相邻像素」的原生写法，**照抄**。
  172/173 的 `colorTime(30, 9, 192,192,255)` ↔ `(255,255,255)` = 蓝↔白呼吸，照抄。

  ⚠️ **层**：一手是 `layer(17)`。本仓 `HUD-LAYOUT.md` §4.1 实测：TH18 的暂停模糊罩只盖
  **UI 层 24/25**（`AnmManager__render_layer` `0x4882b8` 的 `0x4882f1`：`layer-24 <= 8 → 组 = layer+13`，
  别的层一律折到组 39 ⇒ 罩不住），而**世界层 3–19 永远全分辨率**（不许画 HUD）。脚本一旦放进
  `ability.anm` 就是 TH18 的 VM ⇒ 这里**必须**是 24/25，原值 17 **改了**，报告里已标出。
  选 **24**（= 组 37，在罩子之前画）与季节槽脚本 123–127 一致。

■ 产物尺寸为什么这么切（★ 2026-09-13「图集合并」后重写）

  **先划掉两条已作废的旧理由**（本文件以前就是靠它们写的，别再照抄）：
  · ❌「ORDER 里写死尺寸的 entry 必须是 2 的幂（`build_ability.py` 的硬门）」—— 那条门 2026-09-13 已取消；
    而且**引擎侧从来就没有**这条要求：建贴图 `th18:0x486488` 零尺寸检查，原生 `help.anm` 就有 768×1024
    （`engine/anm/th18/06-entry-limit-and-multi-sprite.md` §3）。
  · ❌「一个 entry 只能一个 sprite」—— 同样不是限制：原生一个 entry 放几百个 sprite 是常态
    （同文档 §2.1 / §2.5）。本批 27 个名字现在住在 **4 个图集段**里（段表见文末 `main()` 打印）。

  ⇒ 现在尺寸只有一条规矩（`ATLAS-MERGE-AUDIT.md` §4.1 规则①）：
    **每个成员的矩形 = 成员自己那张 PNG 的像素尺寸**，`build_ability.py` 会硬校验（不匹配直接 raise）。
    「矩形不是自由项」的理由是 `scale`：VM 的 `scale.x` 乘的是 **sprite 矩形宽**
    （`th18:0x481703`：`[vm+0x74] × [vm+0x54]`）⇒ 矩形一改，脚本里的 `scale()` 与 C 侧
    `ce_anm_set_scale` 就跟着变（`ATLAS-MERGE-AUDIT.md` §6.1 坑①）。

  ⇒ 本文件里两处「补边 / 拼图」的真实理由（都与 2 的幂无关）：
    · `SPIRIT_*_CELL_*.png`（32×32）：源 sprite 是 30×30，**居中**补 1px 正好等于原生图自带的
      1px 边距 ⇒ 视觉同心、可见像素 1:1 不动。30→32 是**放大画布**（用留白），不是重采样。
    · `GAUGE_AC.png`（64×64）：两张 32×48 横向拼 + 纵向居中，它是**拼图**（矩形 = 这张 PNG 的尺寸）。
      ⚠ 该成员（`TRANCE_GAUGE_TOP`）眼下**没有任何脚本 / C 代码引用**（脚本 175 已改用
      `GAUGE_FRAME_A/B`），保留只是因为它在 `ATLAS_TRANCE_GAUGE` 的货架上。
    · 其余产物全部**按原生尺寸出图、零补边**（下表尺寸 = 源矩形尺寸）。

  | 产物 | 尺寸 | 源（sprite / 矩形）|
  | --- | --- | --- |
  | `SPIRIT_PLAIN_CELL_C0R0.png` | 32×32 | `littlegods.png` sprite1 (1,1,30,30)（普通态外环，四型共用）|
  | `SPIRIT_PLAIN_CELL_C0R1.png` | 32×32 | sprite2 (1,33,30,30)（普通态内芯，四型共用）|
  | `SPIRIT_TRANCE_CELL_C{0..3}R0.png` | 32×32 | sprite3..6 (33/65/97/129,1,30,30)（灵界态白环，type 0..3）|
  | `SPIRIT_TRANCE_CELL_C{0..3}R1.png` | 32×32 | sprite7..10（同 x，y=33）（灵界态 glow，type 0..3）|
  | `SPIRIT_PLAIN.png` / `SPIRIT_TRANCE.png` | 128×64 | 上面逐格的整图拼版（**不在 ORDER 里**，只作备用）|
  | `GAUGE_AC.png` | 64×64 | `agauge1.png` sprite121/122（各 32×48，横向拼、纵向居中）|
  | `GAUGE_BAR.png` / `GAUGE_BAR2.png` | 48×32 | `agauge2.png` sprite123 / sprite124（**原生尺寸**）|
  | `GAUGE_MARK.png` | 16×32 | `agauge2.png` sprite125（**原生尺寸**）|
  | `GAUGE_FRAME_{A,B}.png` | 32×48 | `agauge1.png` sprite121 亮 / sprite122 暗（**原生尺寸**）|
  | `TIERUP_{1,2,MAX}.png` | 32×16 | `front00.png` sprite78/79/80（**原生尺寸**）|
  | `POPUP_*.png` | 24×8（`POINT*` 32×8）| `front00.png` sprite66/67/69/70 / 72/73（**原生尺寸**）|
  | `ASTRAL.png` | 512×512 | `eff_astral.png` **原样拷** |

  ⚠️ 逐格切图 / 拼图都用**不带 mask** 的 `Image.paste`（见 `_paste` 的 docstring）—— 带 mask 会被 PIL
  做 alpha 合成、把边缘像素预乘成暗色，就不是原样切图了。
  ⇒ 逐格图与拼版是**同一份源像素切两次，逐字节一致**。

■ 本批脚本号（父 → 子，**合并压缩重编号时要一起重映射**）

  | 号 | 文件 | 引用同批脚本号 |
  | --- | --- | --- |
  | 174 | `174_trance_gauge.anm.txt` | 175 / 176 / 177 / 178（`scriptNew`）|
  | 175–178 | `175..178_trance_gauge_*.anm.txt` | 无（叶子）|
  | 179 | `179_spirit_plain.anm.txt` | 180 / 181（`scriptNew`）|
  | 180 / 181 | 普通态外环 / 内芯 | 无（叶子）|
  | 182 | `182_spirit_trance.anm.txt` | 183 / 184（`scriptNew`）|
  | 183 / 184 | 灵界态白环 / glow | 无（叶子）|
  | 185 | `185_faith_dot.anm.txt` | 无（叶子；属 110/111，见 `make_mof_art.py`）|

  脚本里引用同批脚本号的地方写成 `@SCRIPT_XXX@` 占位（`build_ability.py` 的 `substitute` 只认
  ORDER 里的 sprite 名，不认这些）⇒ **合并时按最终号替换**，对照 `entries/ORDER.trance.txt` 末尾。
"""
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC_ASTRAL = REPO / "local/th13.v1.00c/anm/astral"
SRC_FRONT = REPO / "local/th13.v1.00c/anm/front/front"
OUT = HERE / "trance"

# 一手矩形（`local/th13.v1.00c/anm/astral/astral.anm.txt` entry1 `enemy/littlegods.png`）
LITTLEGODS = {
    "sprite1": (1, 1, 30, 30), "sprite2": (1, 33, 30, 30),
    "sprite3": (33, 1, 30, 30), "sprite4": (65, 1, 30, 30),
    "sprite5": (97, 1, 30, 30), "sprite6": (129, 1, 30, 30),
    "sprite7": (33, 33, 30, 30), "sprite8": (65, 33, 30, 30),
    "sprite9": (97, 33, 30, 30), "sprite10": (129, 33, 30, 30),
}
# 3×3 网格序（`front.anm` entry6/entry7 一手矩形）
AGA1 = {"sprite121": (0, 0, 32, 48), "sprite122": (32, 0, 32, 48)}
AGA2 = {"sprite123": (0, 0, 48, 32), "sprite124": (0, 32, 48, 32), "sprite125": (0, 64, 16, 32)}
SRC_EFFECT = REPO / "local/th13.v1.00c/anm/effect/effect"
# 一手矩形（`local/th13.v1.00c/anm/effect/effect.anm.txt` entry0 `effect/eff_switch.png`）
#   ⚠ 贴图是 **64×64**，而这个 sprite 声明的是 **(0,1,128,128)** —— w = 2× 贴图宽、h = 2× 贴图高 + 1px 偏移。
#      见 `make_switch()` 的 docstring：这是「靠采样器寻址平铺/钳制」的原生写法，必须**预烘焙**。


def _load(p):
    if not p.is_file():
        raise SystemExit(
            f"缺原版贴图 {p}\n（先 `python3 tooling/thtk/unpack.py th13.v1.00c`；"
            "本地已切过的话检查 local/th13.v1.00c/anm/ 是否被清过）"
        )
    return Image.open(p).convert("RGBA")


def _paste(dst, tile, xy):
    """把 tile **原样**贴进 dst（RGBA 逐字节一致，含 alpha=0 像素的 RGB）。

    ⚠️ 只能用**不带 mask** 的 `dst.paste(tile, xy)`。
    用 `dst.paste(tile, xy, tile)`（拿自身 alpha 当 mask）时 PIL 做的是 **alpha 合成**，
    会把「alpha 小、RGB 有值」的像素**预乘**成暗色 —— 实测原生图里的 `(255,255,255,68)`
    被写成 `(68,68,68,18)`，边缘一圈 alpha 渐变全被改色，切图就不再是原样。
    （`littlegods.png` 的 `colorKey` 是 0x00000000、`format: 5`，这种像素很多。）
    """
    dst.paste(tile, xy)


def _blit_cell(dst, src, box, cell, cell_idx):
    """把 src 的 box 贴进 dst 的第 cell_idx 格（格中心 = 内容中心）。"""
    x, y, w, h = box
    tile = src.crop((x, y, x + w, y + h))
    cx, cy = (cell - w) // 2, (cell - h) // 2          # 居中；30→32 时是 1px（= 原图自带的 1px 边距）
    ox = (cell_idx % (dst.width // cell)) * cell + cx
    oy = (cell_idx // (dst.width // cell)) * cell + cy
    _paste(dst, tile, (ox, oy))


def _cell_at(img, col, row, cell=32):
    """取拼图里第 (col,row) 格（32×32）—— 与 `_blit_cell` 的落位规则**必须一致**。"""
    return img.crop((col * cell, row * cell, col * cell + cell, row * cell + cell))


def make_spirit():
    """→ trance/SPIRIT_PLAIN.png（128×64，4 列 × 2 行 × 32）与 SPIRIT_TRANCE.png（同形）。"""
    src = _load(SRC_ASTRAL / "enemy" / "littlegods.png")
    if src.size != (256, 64):
        raise SystemExit(f"{SRC_ASTRAL}/enemy/littlegods.png 是 {src.size}，预期 (256, 64)")
    plain = Image.new("RGBA", (128, 64), (0, 0, 0, 0))
    trance = Image.new("RGBA", (128, 64), (0, 0, 0, 0))
    # SPIRIT_PLAIN：行 0 = 外环 sprite1、行 1 = 内芯 sprite2。
    # ★ 普通态**四型共用**这两个 sprite（一手：script2/8/14/20 = 四个 type 的父都用
    #   sprite(sprite1)，script3/9/15/21 都用 sprite(sprite2)，靠 `color()` 区分四型）
    #   ⇒ 四列是**同一份内容的副本**，只为与 SPIRIT_TRANCE 的格子布局对齐、卡侧换图不改 UV。
    #   （卡侧普通态只引 @SPIRIT_PLAIN_0 / @SPIRIT_PLAIN_1。）
    for t in range(4):
        _blit_cell(plain, src, LITTLEGODS["sprite1"], 32, t)
        _blit_cell(plain, src, LITTLEGODS["sprite2"], 32, 4 + t)
        # ★ 灵界态的配对（一手逐条核过 `astral.anm` 原文）：
        #   type0: script4 → script5=sprite3(白环) / script6=sprite7(glow)
        #   type1: script10→ script11=sprite4 / script12=sprite8
        #   type2: script16→ script17=sprite5 / script18=sprite9
        #   type3: script22→ script23=sprite6 / script24=sprite10
        #   ⇒ 行 0 依次 sprite 3..6、行 1 依次 sprite **7..10**。
        #   ⚠️ 别写成「行 1 = 行 0 + 4」的固定偏移：那样得到 4,5,6,7（错），必须是 7,8,9,10。
        _blit_cell(trance, src, LITTLEGODS[f"sprite{3 + t}"], 32, t)
        _blit_cell(trance, src, LITTLEGODS[f"sprite{7 + t}"], 32, 4 + t)
    OUT.mkdir(exist_ok=True)
    plain.save(OUT / "SPIRIT_PLAIN.png")
    trance.save(OUT / "SPIRIT_TRANCE.png")

    # ★ 逐格再各存一张 32×32：脚本换图只能换 **sprite 号**（`ce_anm_set_sprite` 换的是 sprite，
    #   不是 UV 子区）⇒ 每个要单独引用的格子都得是**自己的成员**（= 自己的 PNG）。
    #   这不是「build_ability.py 只支持一个 entry 一个 sprite」——那条限制不存在（原生一个 entry
    #   放几百个 sprite，见 `engine/anm/th18/06-entry-limit-and-multi-sprite.md` §2）；
    #   合并后这些格子就是 `ATLAS_TRANCE_SPIRIT` 的成员。
    #   **同像素切两次，不是另画一套。**
    for col in range(4):
        for row in range(2):
            _cell_at(plain, col, row).save(OUT / f"SPIRIT_PLAIN_CELL_C{col}R{row}.png")
            _cell_at(trance, col, row).save(OUT / f"SPIRIT_TRANCE_CELL_C{col}R{row}.png")

    print("trance: SPIRIT_PLAIN.png / SPIRIT_TRANCE.png 128x64（4 列 × 2 行 × 32 格，"
          "格内居中 30x30；行 0 = 外环，行 1 = 内芯/glow）")
    print("trance: SPIRIT_{PLAIN,TRANCE}_CELL_C{0..3}R{0,1}.png 32x32（同一像素逐格再存，供 ORDER 用）")


def make_tierup():
    """→ trance/TIERUP_{1,2,MAX}.png（各 32×16）—— **攒满一格时的图形提示**（THBWiki 口径 #7）。

    一手（`local/th13.v1.00c/anm/front/front.anm.txt` + `thanm -x` 出的 `front00.png`）：
      满一格 ⇒ `th13:0x43d530` 里 `old/200 != new/200` 时起 **`front.anm` script = `new/200 + 0x6b`**，
      即 **108 / 109 / 110**（= 满 1 格 / 满 2 格 / 满 3 格；★ 01 篇旧写的「107/108/109」是 off-by-one）。
      三个脚本除了 `sprite()` 之外**逐字节相同**：
        `sprite78/79/80; ins_431(1); randMode(1); resampleMode(1); anchor(0,2); layer(11);
         scale(1.0,0.0); scaleTime(4,0,1.0,1.0); posTime(100,1,0,-16,0); 0: wait(96);
         scaleTime(4,0,1.0,0.0); +4: delete();`
      ⇒ **寿命 100 帧、上浮 16 px、纵向撑开**。

    贴图内容（我把像素导成字符画逐格读出来的，不是按名字猜）：
      sprite78 = 图标 + 数字 **1**、sprite79 = 图标 + **2**、sprite80 = 图标 + **MAX**（对应三格）。

    ⚠ **TH18 自己的 `front.anm` 85–99 是"同形不同图"**：脚本骨架一模一样，但贴图（sprite57/60/63
      那一族，48×16 / 64×16）是**实心色条**（我同样导字符画看过），不是"1/2/MAX"图标
      ⇒ **不能白捡 TH18 的图**，必须切 TH13 的。

    尺寸：源 sprite 就是 32×16 ⇒ **按原生尺寸出图**（不补边、不切格；不是「一个 entry 一个 sprite」，
      那条限制不存在）。矩形 = 这张 PNG 的像素尺寸（`ATLAS-MERGE-AUDIT.md` §4.1 规则①），
      在 `ATLAS_TRANCE_GAUGE` 里占一格 32×16。"""
    src = _load(SRC_FRONT / "front00.png")
    if src.size != (512, 512):
        raise SystemExit(f"{SRC_FRONT}/front00.png 是 {src.size}，预期 (512, 512)")
    OUT.mkdir(exist_ok=True)
    for name, box in (("TIERUP_1.png", (416, 72, 448, 88)),      # sprite78
                      ("TIERUP_2.png", (448, 72, 480, 88)),      # sprite79
                      ("TIERUP_MAX.png", (480, 72, 512, 88))):   # sprite80
        x, y, w, h = 0, 0, 0, 0
        x, y, w, h = box[0], box[1], box[2] - box[0], box[3] - box[1]
        src.crop((x, y, x + w, y + h)).save(OUT / name)
    print("trance: TIERUP_1/2/MAX.png 32x16（front.anm sprite 78/79/80 = 满 1 格 / 2 格 / MAX）")


def make_popups():
    """→ trance/POPUP_*.png（各 32×8 画布，内容**左对齐**）—— **吃到神灵时的数字弹窗**。

    ★ 2026-09-13 第三轮（用户实跑反馈①「收掉就硬消失」+③「没有 +1/+100 弹窗」）：
      一手重读 `th13:0x438730` `SpiritItem__on_tick` 的**收集分支**，确认原版**没有**给神灵本体
      任何「被吸走」的动画 —— 收集时 `FUN_00418330` 把神灵的 anm VM **当场销毁**（置 `vm+0x1c`
      的 bit5，下一帧走 `AnmManager__sub_46f690` 摘链回收）。玩家看到的反馈 = 三件事：
        ① 收集音（`SoundManager__play_sound_at_position`，`th13:0x438b38`，SE `0x3f`）；
        ② 灵界槽 +2（`FUN_0043d530(&CURRENT_SPIRIT_GAUGE)`，`th13:0x438b45`）；
        ③ **在神灵位置起一支数字弹窗 VM**（`FUN_0046f820(&slot, script, spirit, 0, -1, 0)`）。
      ⇒ 本轮补的就是 ③（旧实现只做了 ①②）。

    脚本号（一手，同一段汇编的跳转表 `th13:0x438e3c` 按 `spirit[1]` = type 分派）：

    | type | 普通态（`+0x84` bit0=0）| 灵界态（bit0=1）| sprite（本函数切的矩形）| 内容 |
    | --- | --- | --- | --- | --- |
    | 0 | `0x60`=96 | `0x61`=97 | sprite66 / sprite67 | 命碎 +1 / +2 |
    | 1 | `0x66`=102 | `0x67`=103 | sprite72 / sprite73 | 最大得点 +10 / +100 |
    | 2 | `0x63`=99 | `0x64`=100 | sprite69 / sprite70 | 符卡碎 +1 / +2 |
    | 3 | 无 | 无 | — | 原版**不起 VM**（type 3 走 `0x44ad60` 改分数显示，见 01 §2.2）|

    脚本正文（96/97/99/100 逐字节相同，只有 `sprite()` 不同；102/103 同形）：

        sprite(spriteNN); ins_431(1); randMode(1); resampleMode(1); anchor(0, 2); layer(11);
        scale(1.0, 0.0); scaleTime(4, 0, 1.0, 1.0); posTime(50, 1, 0.0, -16.0, 0.0);
        0: wait(46); scaleTime(4, 0, 1.0, 0.0); +4: delete();

    ⇒ **寿命 54 帧（50 上浮 + 4 收尾）、上浮 16 px、从扁平撑开**。TH18 侧只改 `layer(11)`→`13`
      （= TH18 自己那份同形脚本的层，见 `assets/ability/README.md` 的同形对照）。

    尺寸/对齐：源矩形 24×8（sprite66/67/69/70）与 32×8（sprite72/73），**按原生尺寸出图、零补边**。
    ⚠ 旧注释写「按原生尺寸是因为 2 的幂限制已放开」—— 那句要订正：**引擎侧从来没有 2 的幂要求**
    （建贴图 `th18:0x486488` 零尺寸检查、原生 `help.anm` 768×1024，见
    `engine/anm/th18/06-entry-limit-and-multi-sprite.md` §3），真正的依据是「矩形 = 原图像素尺寸」
    （`ATLAS-MERGE-AUDIT.md` §4.1 规则①）。§6.1 另外订正过一句：`POPUP_SPELL1/2` 原先按 32×8 声明
    （把 24×8 的图居中补进 32×8 画布），现在按源图 24×8 出图 —— 视觉零差异，因为锚点
    `anchor(0,2)` = 水平居中 + 底对齐，居中补边本来就不产生位移。
    ⚠ 2026-09-13 订正：早先这里写「脚本是 `anchor(0,2)`（左-下）」**是误读** —— 一手锚点语义是
    `0 = 中 / 1 = 左上 / 2 = 右下`（见 `assets/ability/scripts/210_mof_faith_icon.anm.txt:39` 的注），
    所以 `anchor(0, 2)` = **水平居中 + 底对齐**；当初"补宽到 32×8 再左对齐"的做法只是把误差压小，
    **正解是按原生尺寸切**（这也是现在的做法）。
    ※ 交叉验证：`sprite72` 比 `sprite66` 宽 8 px，正好是"多一个字符"的宽度（+10 vs +1）。"""
    src = _load(SRC_FRONT / "front00.png")
    if src.size != (512, 512):
        raise SystemExit(f"{SRC_FRONT}/front00.png 是 {src.size}，预期 (512, 512)")
    OUT.mkdir(exist_ok=True)
    table = (("POPUP_LIFE1.png",    (416, 48, 440, 56), "sprite66 = 命碎 +1（脚本 96）"),
             ("POPUP_LIFE2.png",    (440, 48, 464, 56), "sprite67 = 命碎 +2（脚本 97，灵界态）"),
             ("POPUP_SPELL1.png",   (416, 56, 440, 64), "sprite69 = 符卡碎 +1（脚本 99）"),
             ("POPUP_SPELL2.png",   (440, 56, 464, 64), "sprite70 = 符卡碎 +2（脚本 100，灵界态）"),
             ("POPUP_POINT10.png",  (416, 64, 448, 72), "sprite72 = 最大得点 +10（脚本 102）"),
             ("POPUP_POINT100.png", (448, 64, 480, 72), "sprite73 = 最大得点 +100（脚本 103，灵界态）"))
    for name, box, note in table:
        w, h = box[2] - box[0], box[3] - box[1]
        # ★ 2026-09-13 第四轮：**按原生尺寸出图**（24×8 / 32×8），不再补到 32×8。
        #   旧版补宽到 32 ⇒ 贴图比原版宽 8 px（`scale` 又是 2.0 时看着就是"大了 3 倍"）。
        #   旧注释把这条挂在「`build_ability.py` 已放开 2 的幂限制」上 —— 订正：**引擎侧从来没有**
        #   2 的幂要求（`th18:0x486488` 零尺寸检查、原生 `help.anm` 768×1024，
        #   `engine/anm/th18/06-entry-limit-and-multi-sprite.md` §3）；真正的依据是
        #   「矩形 = 原图像素尺寸」（`ATLAS-MERGE-AUDIT.md` §4.1 规则①）。
        src.crop(box).save(OUT / name)
        print(f"trance: {name} {w}x{h}（**原生尺寸**，anchor(0,2) = 水平居中 + 底对齐，语义不变）—— {note}")
    print("trance: 6 张数字弹窗全部是 TH13 `front.anm` sprite66/67/69/70/72/73 的原矩形，未缩放未重画")


def make_gauge_frames():
    """→ trance/GAUGE_FRAME_{A,B}.png（各 32×64）—— 左下角指示槽的**框**两态（亮 / 暗）。

    一手（`local/th13.v1.00c/anm/front/front.anm.txt` script172 的 `interruptLabel(3)`）：
      **槽 < 200 时切到 sprite122**，否则 sprite121。我把两个矩形导成字符画看过：
      **同一张图形的亮/暗两态**（121 亮、122 暗），不是两种形状。
    ⇒ 卡侧按 `gauge >= 200` 用 `ce_anm_set_sprite()` 换这两张（= 复刻中断 3）。

    尺寸：源矩形 32×48（`agauge1.png` 的 (0,0,32,48) / (32,0,32,48)）⇒ **按原生尺寸出图**，零补边。
    ⚠ 旧注释写「`build_ability.py` 的硬门要求 2 的幂，48 不是 ⇒ 塞进 32×64 纵向居中」—— 那条门
      2026-09-13 已取消，且引擎侧从来没有（`th18:0x486488` 零尺寸检查，
      `engine/anm/th18/06-entry-limit-and-multi-sprite.md` §3）。现在矩形 = 这张 PNG 的像素尺寸
      （`ATLAS-MERGE-AUDIT.md` §4.1 规则①），在 `ATLAS_TRANCE_GAUGE` 里占一格 32×48。
      `anchor(1,2)` = 水平居中 + 底对齐 ⇒ 矩形高就是可见高度，多补的透明边反而是变量。"""
    src = _load(SRC_FRONT / "agauge1.png")
    if src.size != (64, 64):
        raise SystemExit(f"{SRC_FRONT}/agauge1.png 是 {src.size}，预期 (64, 64)")
    OUT.mkdir(exist_ok=True)
    for name, x0 in (("GAUGE_FRAME_A.png", 0), ("GAUGE_FRAME_B.png", 32)):
        src.crop((x0, 0, x0 + 32, 48)).save(OUT / name)   # 原生 32×48，不补边
    print("trance: GAUGE_FRAME_A.png / GAUGE_FRAME_B.png 32x48（agauge1 sprite121 亮 / sprite122 暗，**原生尺寸**）")


def make_astral():
    """→ trance/ASTRAL.png（512×512，**原样拷** TH13 `astral.anm` entry0 `effect/eff_astral.png`）。

    一手（`local/th13.v1.00c/anm/astral/astral.anm.txt`）：
      `entry0 = effect/eff_astral.png`、`512×512`、**只有一个 sprite0 = 整张图**、
      `script0 { -1: type(1); anchor(0,0); sprite(sprite0); angleVel(0,0,0.010471975f);
                 pos(320,224,0); scale(1.15,1.15); alpha(1); blendMode(7); 0: stop(); }`

    ⚠★ **这张图在原版里不是"一张普通 sprite"**（2026-09-13 一手复核，`th13:0x4032d0`）：
      灵界背景由 **EffectManager 的 effect 回调**画成 **自定义 D3D 全屏四边形**（自己设一组
      render state：`0xf/0xce/0x13/0x14/0xab/0xcf/0xd0/0xd1`，再用 `DrawPrimitiveUP`
      画 4 个顶点），**另外再画 4 个 astral VM**（`0x403656` 的 case 1 从模板拷 4 份）。
      ⇒ `script0` 里的 `pos(320,224)` / `scale(1.15)` / **`alpha(1)`** 在运行期**被那个回调覆盖**
        （`case 7/8/9` 把四个 VM 的 scale 写成 `320.0f`/`240.0f`，还有一个 26/20/6 帧的状态机）
        —— 这也解释了为什么 `alpha(1)` 看着像"几乎全透明"却仍然看得见。
      ⇒ 我们在 TH18 侧**只能**用普通 VM 近似（没有那个 effect 回调），**这是近似不是复刻**，
        卡侧整段挂在 `TRANCE_BG_ENABLE` 后面、**默认关**（见 `cards/trance.c` 的注释）。

    尺寸：源图就是 512×512 ⇒ **原样拷**（不补边、不切格）。`ATLAS_TRANCE_ASTRAL` 只有它一个成员
      （矩形 0,0,512,512 = 画布，`ATLAS-MERGE-AUDIT.md` §4.3）。"""
    src = _load(SRC_ASTRAL / "effect" / "eff_astral.png")
    if src.size != (512, 512):
        raise SystemExit(f"{SRC_ASTRAL}/effect/eff_astral.png 是 {src.size}，预期 (512, 512)")
    OUT.mkdir(exist_ok=True)
    src.save(OUT / "ASTRAL.png")
    print("trance: ASTRAL.png 512x512（eff_astral.png 原样拷；图集 ATLAS_TRANCE_ASTRAL 的成员矩形 0,0,512x512）")


def _tile_cols(img, cell, times, declared):
    """把 img 里**一个真实格子** `cell` 横向复制 `times` 份 → 新图（宽 = 格宽×times）。

    `cell` = **贴图上真实存在**的矩形（agauge2 是 16 宽）；`declared` = 原生 sprite 声明的矩形
    （48 宽）—— 两者必须满足 `declared == (格宽×times, 格高)`，否则本函数直接报错
    （防的就是「拿 48 宽去裁 16 宽的图」那种静默补透明的错，见 `make_gauge` 的 ★★ ①）。

    ★★ 这不是「补边」也不是「重画」，是**把 GPU 的平铺结果预先算出来**：
      原版 `agauge2.png` 的贴图只有 **16 宽 × 96 高**，而 sprite123/124 声明的是
      **48×32**（= 3 个 16 宽格子并排）。引擎照声明算 UV：`U1 = (x+w)/texw = 48/16 = 3.0`
      （一手：`th13:0x4651c0` 一带 `fld 宽; fdiv 贴图宽`，**没有任何 clamp**），
      而采样器的 U 寻址默认是 **WRAP** ⇒ 那张 16 宽的贴图被画 **3 次**，这就是「3 个灵界槽」。

      · 一手证据①（TH13）：`front.anm` entry 的 `THTXWidth: 16` vs `sprite123: {w: 48}`
        （`thanm -l 13`）；`script172/173/175` 只写 `resampleMode(1)`，**没有** `scrollMode`
        ⇒ 寻址走默认值 ⇒ `flags_hi & 3 == 0` → `ADDRESSU = WRAP`（`engine/anm/th16/09-drawing.md` §4.2(c)）。
      · 一手证据②（TH18 侧同机制活着的证明）：原生 `pl01.anm` **零 `scrollMode`**，
        却有 `sprite28 = (0,177,512,16)` / `sprite29 = (0,288,512,32)` 落在 256×320 的贴图上
        （u∈[0,2]）；`st01wl.anm` script1 更直接：`scrollMode(2,0)` + `sprite1 = (0,0,1024,512)`
        落在 256×256 贴图上 = **U 镜像平铺 / V 平铺 4×2 张**。
        ⇒ 「sprite 矩形大于贴图 = 靠寻址平铺」在 TH18 引擎里是**在用**的常规写法。
    · 为什么我们必须烘进美术：本仓的贴图**已并进图集**（`ATLAS_TRANCE_GAUGE` 128×128），
      成员矩形只能是画布内的子矩形 —— 声明 48 宽就等于去采**隔壁成员**的像素。
      ⇒ 烘出来以后「48 宽、3 个格子」与原生**画面逐像素一致**，而 UV 全落在成员矩形内。
    """
    x, y, w, h = cell
    dw, dh = declared[2], declared[3]
    if (w * times, h) != (dw, dh):
        raise SystemExit(
            f"_tile_cols: 声明矩形 {dw}x{dh} ≠ 格 {w}x{h} × {times}（{'x'.join(str(v) for v in declared)}）"
        )
    one = img.crop((x, y, x + w, y + h))
    out = Image.new("RGBA", (dw, dh), (0, 0, 0, 0))
    for k in range(times):
        _paste(out, one, (k * w, 0))
    return out


def make_gauge():
    """→ trance/GAUGE_AC.png（64×64 拼图）+ GAUGE_BAR/BAR2/MARK.png（**48×32 / 48×32 / 16×32**）。

    | 产物 | 尺寸 | 源 sprite / 矩形 | 怎么来的 |
    | --- | --- | --- | --- |
    | `GAUGE_AC.png` | 64×64 | sprite121 (0,0,32,48) + sprite122 (32,0,32,48) | 横向拼、纵向居中（**零引用**，只挂在货架上）|
    | `GAUGE_BAR.png` | 48×32 | sprite123 (0,0,**48**,32) | 贴图 cell (0,0,**16**,32) **横向平铺 3×** |
    | `GAUGE_BAR2.png` | 48×32 | sprite124 (0,32,**48**,32) | 贴图 cell (0,32,**16**,32) **横向平铺 3×** |
    | `GAUGE_MARK.png` | 16×32 | sprite125 (0,64,16,32) | 原样（w == 贴图宽 ⇒ 没有平铺）|

    ★★ 2026-09-13 第六轮（用户实跑：「扭曲程度减小了，但还是在左右动」）——**两个根因**：

    ① **`GAUGE_BAR/BAR2` 之前是「48 宽的画布、内容只占左边 16 px」**：旧代码
       `a2.crop((0,0,48,32))` 去裁一张 **16 宽**的图 —— PIL 的 `crop` 越界部分**补全透明**
       ⇒ 输出的右 2/3 是空的（`alpha=0`）。画到屏幕上就是「只有左三分之一有图」，
       而且 `uv_scale.u = 槽/600` 的裁切（从左边保留 f）在前 2/3 里**什么都裁不到**
       ⇒ 观感是「条不动、叶子在最后 1/3 才被吃掉」。**修法 = 用 `_tile_cols` 把平铺烘出来**
       （见它的 docstring：这是 GPU 真干的事，不是补边）。
    ② 见 `cards/trance.c` 的 `TRANCE_HUD_MARK_SLIDE`（游标 `entity_pos.x` 语义）。

    ⚠️ 旧注释里作废的说法（别再照抄）：「构建期要求 2 的幂」「一个 entry 一个 sprite」
    「拼成 128 宽后用 `scaleUV` 取半」（全库零处 `scaleUV`）、「矩形 = 原图尺寸」（对单图成立，
    但**这里正是矩形大于原图的合法情形** —— 依据是原生 `pl01`/`st01wl` 的同类 sprite）。
    """
    a1 = _load(SRC_FRONT / "agauge1.png")
    a2 = _load(SRC_FRONT / "agauge2.png")
    if a1.size != (64, 64):
        raise SystemExit(f"{SRC_FRONT}/agauge1.png 是 {a1.size}，预期 (64, 64)")
    # ⚠️ agauge2.png 文件是 16×**128**、THTX 只声明 16×**96**（真正的纹理到 y=96 为止），
    #    `thanm -x` 落盘的是 THTX 那一份 ⇒ 文件高 96 也正常；只要 ≥96 就能取到 sprite125(0,64,16,32)。
    if a2.size[0] != 16 or a2.size[1] < 96:
        raise SystemExit(f"{SRC_FRONT}/agauge2.png 是 {a2.size}，预期 16 宽 ≥96 高")

    OUT.mkdir(exist_ok=True)
    # ① 外框两态：横向拼成一张 64×64（左=sprite121，右=sprite122），矩形 = 这张 PNG 的尺寸。
    #    ⚠ 现在**没有**任何脚本用它：脚本 175 直接引 `TRANCE_GAUGE_FRAME_A/B`（原生 32×48 各一张）。
    ac = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    for name in ("sprite121", "sprite122"):
        x, y, w, h = AGA1[name]
        tile = a1.crop((x, y, x + w, y + h))
        _paste(ac, tile, (x, (64 - h) // 2))
    ac.save(OUT / "GAUGE_AC.png")

    # ② 三条：sprite123/124 是「16 宽贴图上的 48 宽矩形」⇒ 烘 3 次平铺；sprite125 原样。
    #    ⚠ 声明尺寸仍然是 48/48/16 —— 与原生**逐字节相同**的 48 宽 ⇒
    #      引擎算出来的 `scale.x`（槽位比例）与 `uv_scale.u` 裁切量与原版同量纲。
    #    ⚠⚠ 传给 `_tile_cols` 的**第一个**矩形必须是「贴图上真实存在的格子」（16 宽），
    #      第二个才是原生声明的 48 宽 —— 反过来就是本轮修掉的那个 bug。
    _tile_cols(a2, (0, 0, 16, 32), 3, AGA2["sprite123"]).save(OUT / "GAUGE_BAR.png")
    _tile_cols(a2, (0, 32, 16, 32), 3, AGA2["sprite124"]).save(OUT / "GAUGE_BAR2.png")
    x, y, w, h = AGA2["sprite125"]
    a2.crop((x, y, x + w, y + h)).save(OUT / "GAUGE_MARK.png")

    print("trance: GAUGE_AC.png 64x64（左 sprite121 / 右 sprite122 横向拼；零引用）")
    print("trance: GAUGE_BAR.png 48x32 / GAUGE_BAR2.png 48x32"
          "（16 宽 cell **横向平铺 3×** —— 复刻原生 sprite 的 48 宽矩形 + WRAP 寻址）")
    print("trance: GAUGE_MARK.png 16x32（agauge2 sprite125，原样）")


def make_switch():
    """→ trance/SWITCH.png（**128×128**）—— 灵界**进出幕布**的「有形状的遮罩」贴图。

    ★ 一手（`local/th13.v1.00c/anm/effect/effect.anm.txt`，`thanm -l 13`）：
      `entry0 = effect/eff_switch.png`，**THTX 64×64**，只声明一个
      `sprite0: { x: 0, y: 1, w: 128, h: 128 }` —— **w = 2× 贴图宽、h = 2× 贴图高 + 1 px 偏移**。
      用它的 4 支脚本（`script3/4/5/6` 进、`script7/8/9/10` 出）都写着
      **`scrollMode(0, 1)`**（= `ADDRESSU: 0 → WRAP`、`ADDRESSV: 1 → CLAMP`，
      `engine/anm/th16/09-drawing.md` §4.2(c)(d) 的一手映射）。
      ⇒ 原生画面 = 「U 方向把 64 宽贴图**平铺 2 次**；V 方向从 **第 1 行**起采样，
         越过第 63 行后**钳制**（下半块是第 63 行的拉伸）」。
      贴图本身是**纯 alpha 形状**（RGB 全白，alpha 0..255，斜向笔触）——
      配上脚本里的 `color(32,16,96)`（深蓝紫）= 用户口径的「**有形状的不透明 mask**」✅。

    ⇒ 本函数把这套寻址**烘成 128×128 的图**（同 `_tile_cols` 的理由：图集里不能靠越界 UV）：
        · 列 `i` ← 贴图列 `i mod 64`（WRAP 平铺 2 次）
        · 行 `j` ← 贴图行 `min(j + 1, 63)`（从第 1 行起 + 到底钳制）
      采样点逐像素落在 texel 中心（`u·64 = i+0.5`、`v·64 = j+1.5`）⇒ 与原生**逐像素等价**，
      连 LINEAR 过滤都不会有差值（采样点不在 texel 之间）。
    """
    src = _load(SRC_EFFECT / "eff_switch.png")
    if src.size != (64, 64):
        raise SystemExit(f"{SRC_EFFECT}/eff_switch.png 是 {src.size}，预期 (64, 64)")
    out = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    for j in range(128):
        ty = min(j + 1, 63)
        row = src.crop((0, ty, 64, ty + 1))          # 贴图的一整行（64 宽）
        _paste(out, row, (0, j))
        _paste(out, row, (64, j))                    # U 方向平铺第 2 份
    OUT.mkdir(exist_ok=True)
    out.save(OUT / "SWITCH.png")
    print("trance: SWITCH.png 128x128（eff_switch.png 64×64 + scrollMode(0,1) 的 WRAP/CLAMP 寻址，"
          "逐像素预烘焙；进出幕布 4×2 支脚本共用）")


def main():
    make_spirit()
    make_gauge()
    make_astral()
    make_switch()
    make_tierup()
    make_gauge_frames()
    make_popups()
    print("\n把下面几行追加到 entries/ORDER.txt（或合并 entries/ORDER.trance.txt）：")
    print("# 整图（备用，脚本不引用）")
    print("SPIRIT_PLAIN   ability/trance/SPIRIT_PLAIN.png  128x64")
    print("SPIRIT_TRANCE  ability/trance/SPIRIT_TRANCE.png  128x64")
    print("# 脚本实际引用的逐格 32x32")
    for col, row in ((0, 0), (0, 1)):
        print(f"SPIRIT_PLAIN_{col * 2 + row}    ability/trance/SPIRIT_PLAIN_CELL_C{col}R{row}.png  32x32")
    for col in range(2):
        for row in range(2):
            print(f"SPIRIT_TRANCE_{col * 2 + row}   "
                  f"ability/trance/SPIRIT_TRANCE_CELL_C{col}R{row}.png  32x32")
    print("TRANCE_GAUGE_TOP   ability/trance/GAUGE_AC.png  64x64")
    print("TRANCE_GAUGE_BAR   ability/trance/GAUGE_BAR.png  48x32")
    print("TRANCE_GAUGE_BAR2  ability/trance/GAUGE_BAR2.png  48x32")
    print("TRANCE_GAUGE_MARK  ability/trance/GAUGE_MARK.png  16x32")
    print("# 灵界背景（512×512，2 的幂 ⇒ 一个 entry 一个 sprite）")
    print("TRANCE_ASTRAL  ability/trance/ASTRAL.png  512x512")
    print("# 灵界进出幕布的「有形状遮罩」（128×128；脚本 262..265 引用）")
    print("TRANCE_SWITCH  ability/trance/SWITCH.png  128x128")
    print("# 左下角指示槽的框两态（槽 >=200 亮 / <200 暗）")
    print("TRANCE_GAUGE_FRAME_A  ability/trance/GAUGE_FRAME_A.png  32x48")
    print("TRANCE_GAUGE_FRAME_B  ability/trance/GAUGE_FRAME_B.png  32x48")
    print("# 攒满一格/两格/满槽的图形提示（脚本 252/253/254 引用）")
    for n in ("TIERUP_1", "TIERUP_2", "TIERUP_MAX"):
        print(f"{n:13s} ability/trance/{n}.png  32x16")
    print("# 吃到神灵的数字弹窗（脚本 255..260 引用；内容左对齐、画布 32x8）")
    for n in ("POPUP_LIFE1", "POPUP_LIFE2", "POPUP_SPELL1", "POPUP_SPELL2",
              "POPUP_POINT10", "POPUP_POINT100"):
        print(f"{n:14s} ability/trance/{n}.png  32x8")


if __name__ == "__main__":
    main()
