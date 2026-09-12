#!/usr/bin/env python3
"""神灵庙（id 113）的**神灵实体**与**灵界槽**贴图 —— 全部从 TH13 零售图切。

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
  的一个独立计时（发中断 + 清 flag bit1）。**本次只做零售切图 + anm 脚本，不碰引擎计时**，
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
  三个 sprite 的 w 比文件宽 —— 这是「sprite 矩形可越界取到相邻像素」的零售写法，**照抄**。
  172/173 的 `colorTime(30, 9, 192,192,255)` ↔ `(255,255,255)` = 蓝↔白呼吸，照抄。

  ⚠️ **层**：一手是 `layer(17)`。本仓 `HUD-LAYOUT.md` §4.1 实测：TH18 的暂停模糊罩只盖
  **UI 层 24/25**（`AnmManager__render_layer` `0x4882b8` 的 `0x4882f1`：`layer-24 <= 8 → 组 = layer+13`，
  别的层一律折到组 39 ⇒ 罩不住），而**世界层 3–19 永远全分辨率**（不许画 HUD）。脚本一旦放进
  `ability.anm` 就是 TH18 的 VM ⇒ 这里**必须**是 24/25，原值 17 **改了**，报告里已标出。
  选 **24**（= 组 37，在罩子之前画）与季节槽脚本 123–127 一致。

■ 产物尺寸为什么这么切

  `build_ability.py` 的硬门：ORDER 里写死尺寸的 entry **必须是 2 的幂**。30×30 不是 ⇒
  **30×30 居中进 32×32**（上下左右各 1px 透明，正好等于零售图自带的 1px 边距 ⇒ 视觉同心）。
  ❌ **不能只用「一张拼图 + `scaleUV` 选格」**：本仓实测 `build_ability.py` 是
  **「一个 entry 一个 sprite」**（`L.make_entry` 的 `sprite_idx` 是整张图一个 sprite），
  而 `scaleUV` 在本仓**一处都没用过**（`local/th18.v1.00a/anm/ability/ability.anm.txt` 里也没有），
  不押注未实测的 opcode ⇒ 脚本要用的每一格**各出一张 32×32**。同时**另存**两张拼图
  （`SPIRIT_PLAIN.png` / `SPIRIT_TRANCE.png`，128×64 = 4 列 × 2 行 × 32，给卡侧预览 / 将来
  换成「一 entry + scaleUV」时用）；拼图与逐格图是**同一份源像素切两次，逐字节一致**。

  | 产物 | 尺寸 | 对应 `littlegods.png` 的 sprite |
  | --- | --- | --- |
  | `SPIRIT_PLAIN_CELL_C0R0.png` | 32×32 | sprite1（普通态外环，四型共用）|
  | `SPIRIT_PLAIN_CELL_C0R1.png` | 32×32 | sprite2（普通态内芯，四型共用）|
  | `SPIRIT_TRANCE_CELL_C{0..3}R0.png` | 32×32 | sprite3..6（灵界态白环，type 0..3）|
  | `SPIRIT_TRANCE_CELL_C{0..3}R1.png` | 32×32 | sprite7..10（灵界态 glow，type 0..3）|
  | `GAUGE_AC.png` | 64×64 | sprite121 / sprite122（各 32×48，横向拼、纵向居中）|
  | `GAUGE_BAR/BAR2/MARK.png` | 64×64 | sprite123 / sprite124 / sprite125（各 48×32、48×32、16×32，居中）|

  ⚠️ 30→32 是**放大**（用留白，不是重采样）；要 1:1 得放开 `build_ability.py` 的 2 的幂门（未做）。
  ⚠️ 逐格切图用**不带 mask** 的 `Image.paste`（见 `_paste` 的 docstring）—— 带 mask 会被 PIL 做
  alpha 合成、把边缘像素预乘成暗色，就不是原样切图了。

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


def _load(p):
    if not p.is_file():
        raise SystemExit(
            f"缺零售贴图 {p}\n（先 `python3 tooling/thtk/unpack.py th13.v1.00c`；"
            "本地已切过的话检查 local/th13.v1.00c/anm/ 是否被清过）"
        )
    return Image.open(p).convert("RGBA")


def _paste(dst, tile, xy):
    """把 tile **原样**贴进 dst（RGBA 逐字节一致，含 alpha=0 像素的 RGB）。

    ⚠️ 只能用**不带 mask** 的 `dst.paste(tile, xy)`。
    用 `dst.paste(tile, xy, tile)`（拿自身 alpha 当 mask）时 PIL 做的是 **alpha 合成**，
    会把「alpha 小、RGB 有值」的像素**预乘**成暗色 —— 实测零售图里的 `(255,255,255,68)`
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

    # ★ 逐格再各存一张 32×32：`build_ability.py` 实测只支持「一个 entry 一个 sprite」，
    #   没法只取一张图里的某一格（`ce_anm_set_sprite` 只能换 sprite 号、不能换 UV 子区），
    #   所以脚本要用的格子必须各自成为一个 ORDER entry。**同像素切两次，不是另画一套。**
    for col in range(4):
        for row in range(2):
            _cell_at(plain, col, row).save(OUT / f"SPIRIT_PLAIN_CELL_C{col}R{row}.png")
            _cell_at(trance, col, row).save(OUT / f"SPIRIT_TRANCE_CELL_C{col}R{row}.png")

    print("trance: SPIRIT_PLAIN.png / SPIRIT_TRANCE.png 128x64（4 列 × 2 行 × 32 格，"
          "格内居中 30x30；行 0 = 外环，行 1 = 内芯/glow）")
    print("trance: SPIRIT_{PLAIN,TRANCE}_CELL_C{0..3}R{0,1}.png 32x32（同一像素逐格再存，供 ORDER 用）")


def make_gauge():
    """→ trance/GAUGE_AC.png（64×64）+ GAUGE_BAR/GAUGE_BAR2/GAUGE_MARK.png（各 64×64）。

    `build_ability.py` 是「一个 entry 一个 sprite」，所以一张图能装几个 sprite 就看
    能不能用 `scaleUV` 精确取块（scaleUV = 按倍数缩放 sprite 的 UV 矩形）：

    | 产物 | 尺寸 | 源 sprite | 拼进画布的位置 | 脚本怎么只取自己那块 |
    | --- | --- | --- | --- | --- |
    | `GAUGE_AC.png` | 64×64 | `agauge1.png` sprite121 (0,0,32,48) / sprite122 (32,0,32,48) | x=0 / x=32，y=8（纵向居中）| `scaleUV(0.5f, 1.0f)` 取左 / 右半 ✅ |
    | `GAUGE_BAR.png` | 64×64 | `agauge2.png` sprite123 (0,0,48,32) | 居中 | 整张 ✅ |
    | `GAUGE_BAR2.png` | 64×64 | `agauge2.png` sprite124 (0,32,48,32) | 居中 | 整张 ✅ |
    | `GAUGE_MARK.png` | 64×64 | `agauge2.png` sprite125 (0,64,16,32) | 居中 | 整张 ✅ |

    ⚠️ **AH 三件没有拼成一张**：它们是 48/48/16 宽、拼成 128 宽后比例是 16/128 与 48/128，
    都不是 2 的幂 ⇒ `scaleUV` 取不准。宁可多两行 ORDER，也不写取不准的 UV。
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
    # ① 外框两态：横向拼成 64×64（左=sprite121，右=sprite122），scaleUV(0.5,1) 取半
    a1 = _load(SRC_FRONT / "agauge1.png")
    a2 = _load(SRC_FRONT / "agauge2.png")
    if a1.size != (64, 64):
        raise SystemExit(f"{SRC_FRONT}/agauge1.png 是 {a1.size}，预期 (64, 64)")
    # ⚠️ agauge2.png 文件是 16×**128**、THTX 只声明 16×**96**（真正的纹理到 y=96 为止），
    #    `thanm -x` 落盘的是 THTX 那一份 ⇒ 文件高 96 也正常；只要 ≥96 就能取到 sprite125(0,64,16,32)。
    if a2.size[0] != 16 or a2.size[1] < 96:
        raise SystemExit(f"{SRC_FRONT}/agauge2.png 是 {a2.size}，预期 16 宽 ≥96 高")

    OUT.mkdir(exist_ok=True)
    # ① 外框两态：横向拼成 64×64（左=sprite121，右=sprite122），scaleUV(0.5,1) 取半
    ac = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    for name in ("sprite121", "sprite122"):
        x, y, w, h = AGA1[name]
        tile = a1.crop((x, y, x + w, y + h))
        _paste(ac, tile, (x, (64 - h) // 2))
    ac.save(OUT / "GAUGE_AC.png")

    # ② AH 三件：各出一张 64×64（内容居中）—— 三种宽度不同，拼一张没法用 scaleUV 精确取块
    for fname, name in (("GAUGE_BAR.png", "sprite123"),
                        ("GAUGE_BAR2.png", "sprite124"),
                        ("GAUGE_MARK.png", "sprite125")):
        x, y, w, h = AGA2[name]
        tile = a2.crop((x, y, x + w, y + h))
        dst = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
        _paste(dst, tile, ((64 - w) // 2, (64 - h) // 2))
        dst.save(OUT / fname)

    print("trance: GAUGE_AC.png 64x64（左 sprite121 / 右 sprite122；scaleUV(0.5,1) 取半）")
    print("trance: GAUGE_BAR.png / GAUGE_BAR2.png / GAUGE_MARK.png 64x64"
          "（agauge2 的 sprite123 / sprite124 / sprite125，各自居中）")


def main():
    make_spirit()
    make_gauge()
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
    print("TRANCE_GAUGE_BAR   ability/trance/GAUGE_BAR.png  64x64")
    print("TRANCE_GAUGE_BAR2  ability/trance/GAUGE_BAR2.png  64x64")
    print("TRANCE_GAUGE_MARK  ability/trance/GAUGE_MARK.png  64x64")


if __name__ == "__main__":
    main()
