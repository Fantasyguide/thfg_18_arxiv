#!/usr/bin/env python3
"""鬼形兽（id 117）的原生切图 + 脚本生成。

源（都在 gitignored 的 `local/`，构建时取）：
  local/th17.v1.00b/anm/beast/bullet/beast1@beast@0.png     —— 160×96，13 个 32×32 单元（token）
  local/th17.v1.00b/anm/beast/bullet/beastAURA@beast@11.png —— 256×512，24 个 64×64 单元（咆哮光环）
  local/th17.v1.00b/anm/beast/bullet/beastot@beast@10.png   —— 256×256，sprite70 80×64（水獭消弹体）
  local/th17.v1.00b/anm/beast/bullet/beastr@beast@9.png     —— 128×128，sprite69（霊撃三色光圈）
  local/th17.v1.00b/anm/beast/bullet/beastf@beast@8.png     —— 256×256，槽框 sprite34（48×48）
  local/th17.v1.00b/anm/beast/beast.anm.txt                 —— script0..6 / 33..36 / 41..43 的原文

产出：
  ability/animal/*.png                       —— 13 token + 24 光环 + 消弹体 + 光圈 + 槽框
  ability/scripts/137..143_*.anm.txt         —— 7 个 token 脚本（sprite(spriteN) → @NAME）
  ability/scripts/146..148_aura_*.anm.txt    —— 3 个咆哮光环脚本（源 script41..43）
  ability/scripts/150..153_strike*.anm.txt   —— 霊撃三色光圈（源 script33..36）
  144/145 槽框 / 图标、149 消弹体是手写的，不在本生成器里
  屏幕上打印 ORDER.txt 要追加的行

一手：`engine/animal/th17/01-animal-spirits.md` §3.2（token→sprite 表）、§4、§7。
"""
import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]          # assets/ability → card-expand → th18.v1.00a → mods → renkolab
BEAST = REPO / "local" / "th17.v1.00b" / "anm" / "beast"
# ★ 2026-09-10 纠偏：TH18 的道具图标**不在** `bullet1.png`（那 112..117 号是弹的图元），
#   而在 `bullet/item.png`（`bullet.anm` 的 entry3）——脚本 `script112..125` 画的是 `sprite500..`：
#   script112(火力)→sprite500 = (192,0)16×16、script113(金钱)→sprite502 = (208,0)16×16、
#   script115(命碎)→sprite506 = (32,0)32×32、script117(符卡碎)→sprite510 = (64,0)32×32。
#   ORDER 里那四张按 16×16 声明，所以 32×32 的两张在这里缩到 16×16。
TH18_ITEM_PNG = REPO / "local" / "th18.v1.00a" / "anm" / "bullet" / "bullet" / "item.png"
# ★ 2026-09-11 九修（AUDIT §AL39）：魔理沙 EAGLE 卡侧矩形束的视觉。
# TH17 `pl01.anm` entry0 sprite29 = (0,288,512,32)，TH17/TH18 同名 `pl01.png` 逐字节一致
# （md5 10ff96a027419498baceb70ca199dd23）。源 PNG 实际是 256×320，sprite29 横跨 0..512
# 而图片只有 256 宽，说明引擎按 256 宽的激光条铺满/镜像；这里按左右两半无缝相接补成 ORDER
# 要求的 512×32 画布。
EAGLE_BEAM_PNG = REPO / "local" / "th18.v1.00a" / "anm" / "pl01" / "player" / "pl01" / "pl01.png"
OUT_ART = HERE / "animal"
OUT_SCRIPTS = HERE / "scripts"

# ---- beast1.png 160×96：13 个 32×32（token 1..7 + 稀有 8..14 的前 13 个）----
TOKEN_SRC = "beast1@beast@0.png"
TOKEN_CELLS = {
    "WOLF_0":  (0, 0),   "WOLF_1":  (0, 32),  "WOLF_2":  (0, 64),
    "OTTER_0": (32, 0),  "OTTER_1": (32, 32), "OTTER_2": (32, 64),
    "EAGLE_0": (64, 0),  "EAGLE_1": (64, 32), "EAGLE_2": (64, 64),
    "FISH_BOMB": (96, 0), "FISH_LIFE": (128, 0), "FISH_POWER": (96, 32), "FISH_POINT": (128, 32),
}

# ---- beastAURA.png 256×512：24 个 64×64。script41(狼)=sprite79..86 / 42(獭)=87..94 / 43(鹫)=71..78 ----
AURA_SRC = "beastAURA@beast@11.png"
AURA_CELLS = {}
for _i in range(8):
    AURA_CELLS["AURA_EAGLE_%d" % _i] = ((_i % 4) * 64, 0   + (_i // 4) * 64)
    AURA_CELLS["AURA_WOLF_%d"  % _i] = ((_i % 4) * 64, 128 + (_i // 4) * 64)
    AURA_CELLS["AURA_OTTER_%d" % _i] = ((_i % 4) * 64, 256 + (_i // 4) * 64)

# ---- 本作（TH18）的道具图标：`ITEM_TYPE_TABLE_sprite_and_card` `th18:0x4b4020`（stride 8，存 sprite 号）
#      type1(P)=112 / type2(金钱)=113 / type4(残碎)=115 / type6(B碎)=117，都在 bullet1.png 的 y=32 一行。
#      槽内鱼灵图标改成本作的样子（用户 2026-09-10 要求）----
ITEM_CELLS = {          # 名 → (x, y, 源 w, h)：源取自 bullet/item.png（见上面的纠偏注）
    "ITEM_P":     (192, 0, 16, 16),
    "ITEM_MONEY": (208, 0, 16, 16),
    "ITEM_LIFE":  (32,  0, 32, 32),
    "ITEM_BOMB":  (64,  0, 32, 32),
}

# ---- 资源卡的**卡面**（★ 用户 2026-09-10 口径：道具灵在**槽里**要显示「咆哮结束后拿到的东西」）----
#   咆哮结束的兑换：4(B鱼)→BOMB2 卡、5(残鱼)→EXTEND2 卡、7(金钱鱼)→PENDULUM 卡（道具 type 0x11/0x10/0x12）。
#   卡道具在场上**画的就是那张卡本身**（一手 `th18:0x447089` 段：卡类 type 不走 bullet 图集，而是
#   `0x409310` 建一个 UI VM、再用卡 id 取 `abcard.anm` 的卡面）⇒ 槽里也用原生卡面，两边一致。
ABILITY_CARDS = REPO / "local" / "th18.v1.00a" / "anm" / "abcard" / "ability"
CARD_ITEM = {
    "CARD_BOMB":  "BOMB2_min.png",      # 符卡碎片卡（item type 0x11）
    "CARD_LIFE":  "EXTEND2_min.png",    # 残机碎片卡（item type 0x10）
    "CARD_MONEY": "PENDULUM_min.png",   # 金钱卡（item type 0x12，+50 金）
}

# ---- entry12 `beastap.png`：「咆哮**自然结束**时的释放动画」（TH17 script44 父 + 45/46 两个子）----
#   sprite95 = (0,0) 272×32（script45：淡入 + 淡出，70 帧）、sprite96 = (0,32) 272×32
#   （script46：10 次 5 帧的**闪烁** + 15 帧压扁）。ORDER 要 2 的幂 ⇒ 各补成 512×32（图居中，
#   这样「精灵 VM 默认锚心」时图心 = VM 位置，和 TH17 的 272×32 原图一致）。
BEASTAP_CELLS = {
    "BEASTAP_A": (0, 0, 272, 32),
    "BEASTAP_B": (0, 32, 272, 32),
}

# ---- beastf.png sprite37..47（y=48 的 11 个 16×16）：空槽的「动态小灵魂」序列（TH17 script19）----
SOUL_SRC = "beastf@beast@8.png"
SOUL_CELLS = {"SOUL_%d" % _i: (_i * 16, 48) for _i in range(11)}

# beastf.png sprite53..64（48×48）：槽满/咆哮时槽上「燃起的火焰」（TH17 script24）
FLAME_CELLS = {}
for _i in range(12):
    FLAME_CELLS["FLAME_%d" % _i] = ((_i % 5) * 48, 64 + (_i // 5) * 48)

# ---- 其余单元：(源图, x, y, w, h, 画布 WxH) —— 画布必须是 2 的幂（ORDER 要求）----
EXTRA = {
    "OTTER_ORB":   ("beastot@beast@10.png", 0, 0, 80, 64, (128, 64)),
    "STRIKE_RING": ("beastr@beast@9.png",   0, 0, 128, 128, (128, 128)),
    "GAUGE_FRAME": ("beastf@beast@8.png",   0, 0, 48, 48, (64, 64)),
    # ★ 槽内图标（用户 2026-09-10：要用 beastf.png 的槽位图，不是道具图）——
    #   TH17 script14 的 interrupt label = type+6：7/8/9→34/35/36（狼/獭/鹫），10..13→65..68（B/残/P/点）
    "ICON_WOLF":        ("beastf@beast@8.png", 0,   0,   48, 48, (64, 64)),
    "ICON_OTTER":       ("beastf@beast@8.png", 48,  0,   48, 48, (64, 64)),
    "ICON_EAGLE":       ("beastf@beast@8.png", 96,  0,   48, 48, (64, 64)),
    "ICON_FISH_BOMB":   ("beastf@beast@8.png", 128, 160, 32, 32, (32, 32)),
    "ICON_FISH_LIFE":   ("beastf@beast@8.png", 160, 160, 32, 32, (32, 32)),
    "ICON_FISH_POWER":  ("beastf@beast@8.png", 192, 160, 32, 32, (32, 32)),
    "ICON_FISH_POINT":  ("beastf@beast@8.png", 224, 160, 32, 32, (32, 32)),
}

# 霊撃父脚本里 scriptNew 的源→输出重映射
SCRIPT_REF_REMAP = {34: 151, 35: 152, 36: 153, 45: 161, 46: 162}
# (输出脚本号, 输出名, 源脚本号, spriteN → ORDER 名)
SCRIPTS = [
    (137, "animal_wolf",  0,  {0: "WOLF_0", 1: "WOLF_1", 2: "WOLF_2"}),
    (138, "animal_otter", 1,  {3: "OTTER_0", 4: "OTTER_1", 5: "OTTER_2"}),
    (139, "animal_eagle", 2,  {6: "EAGLE_0", 7: "EAGLE_1", 8: "EAGLE_2"}),
    (140, "fish_bomb",    3,  {9: "FISH_BOMB"}),
    (141, "fish_life",    4,  {10: "FISH_LIFE"}),
    (142, "fish_power",   5,  {11: "FISH_POWER"}),
    # ★ 用户 2026-09-10 手绘：金钱鱼灵（原生蓝点鱼改金）= `assets/ability/animal_custom/FISH_MONEY.png`
    #   （**手绘源**放在 animal_custom/ 不进本生成器的产出目录，ORDER 直接指向它；生成器只管原生切图）
    (143, "fish_point",   6,  {12: "FISH_MONEY"}),
    (146, "aura_wolf",    41, {79: "AURA_WOLF_0", 80: "AURA_WOLF_1", 81: "AURA_WOLF_2", 82: "AURA_WOLF_3",
                               83: "AURA_WOLF_4", 84: "AURA_WOLF_5", 85: "AURA_WOLF_6", 86: "AURA_WOLF_7"}),
    (147, "aura_otter",   42, {87: "AURA_OTTER_0", 88: "AURA_OTTER_1", 89: "AURA_OTTER_2", 90: "AURA_OTTER_3",
                               91: "AURA_OTTER_4", 92: "AURA_OTTER_5", 93: "AURA_OTTER_6", 94: "AURA_OTTER_7"}),
    # 咆哮自然结束的释放动画（entry12 beastap.png）
    (160, "beastap",      44, {}),                              # 父：只 scriptNew(45/46)
    (161, "beastap_a",    45, {95: "BEASTAP_A"}),
    (162, "beastap_b",    46, {96: "BEASTAP_B"}),
    (148, "aura_eagle",   43, {71: "AURA_EAGLE_0", 72: "AURA_EAGLE_1", 73: "AURA_EAGLE_2", 74: "AURA_EAGLE_3",
                               75: "AURA_EAGLE_4", 76: "AURA_EAGLE_5", 77: "AURA_EAGLE_6", 78: "AURA_EAGLE_7"}),
    (150, "strike",       33, {69: "STRIKE_RING"}),
    (151, "strike_red",   34, {69: "STRIKE_RING"}),
    (152, "strike_green", 35, {69: "STRIKE_RING"}),
    (153, "strike_purple",36, {69: "STRIKE_RING"}),
    # 空槽的「动态小灵魂」：TH17 script19（beastf.png sprite37..47 的 16×16 序列 + angleVel 自转）
    # 槽满/咆哮时槽上的「火焰」：TH17 script24（beastf.png sprite53..64 的 12 帧序列）
    (156, "slot_flame",   24, {53: "FLAME_0", 54: "FLAME_1", 55: "FLAME_2", 56: "FLAME_3", 57: "FLAME_4",
                               58: "FLAME_5", 59: "FLAME_6", 60: "FLAME_7", 61: "FLAME_8", 62: "FLAME_9",
                               63: "FLAME_10", 64: "FLAME_11"}),
    # 咆哮期间自机的「全屏渐变圆」：TH17 script32（drawPoly 32 边形 + 渐变 + scaleTime 到 600）
    (155, "self_aura",    32, {}),
    (154, "slot_soul",    19, {37: "SOUL_0", 38: "SOUL_1", 39: "SOUL_2", 40: "SOUL_3", 41: "SOUL_4",
                               42: "SOUL_5", 43: "SOUL_6", 44: "SOUL_7", 45: "SOUL_8", 46: "SOUL_9",
                               47: "SOUL_10"}),
]


def _sheet(fn):
    p = BEAST / "bullet" / fn
    if not p.exists():
        sys.exit(f"缺原生贴图 {p}（先 source tooling/env.sh 并 thanp/thdat 解包）")
    return Image.open(p).convert("RGBA")


def crop_sheets():
    OUT_ART.mkdir(parents=True, exist_ok=True)
    im = _sheet(TOKEN_SRC)
    for name, (x, y) in TOKEN_CELLS.items():
        im.crop((x, y, x + 32, y + 32)).save(OUT_ART / f"{name}.png")
    im = _sheet(AURA_SRC)
    for name, (x, y) in AURA_CELLS.items():
        im.crop((x, y, x + 64, y + 64)).save(OUT_ART / f"{name}.png")
    print(f"[sheet] token {len(TOKEN_CELLS)} 张 32×32 + 光环 {len(AURA_CELLS)} 张 64×64 → {OUT_ART}")
    for name, (fn, x, y, w, h, canvas) in EXTRA.items():
        src = _sheet(fn)
        pad = Image.new("RGBA", canvas, (0, 0, 0, 0))
        pad.paste(src.crop((x, y, x + w, y + h)), ((canvas[0] - w) // 2, (canvas[1] - h) // 2))
        pad.save(OUT_ART / f"{name}.png")
    im = _sheet(SOUL_SRC)
    for name, (x, y) in SOUL_CELLS.items():
        im.crop((x, y, x + 16, y + 16)).save(OUT_ART / f"{name}.png")
    print(f"[soul] {len(SOUL_CELLS)} 张 16×16 → {OUT_ART}")
    im = _sheet(SOUL_SRC)
    for name, (x, y) in FLAME_CELLS.items():
        # ★ 用户 2026-09-10：TH17 的火焰放大 1.5 倍时是**往上长**（不是从左上角长）——那支脚本用的是
        #   `anchor(0,2)`（锚心 + 底边，和 TH17 就绪条 script30/31 同一口径）。为了 1 倍时位置不跑，
        #   贴图在 64×64 画布里改**底对齐**（48 宽居中、底部贴边）⇒ 锚心/底 = 图的底边中点。
        pad = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
        pad.paste(im.crop((x, y, x + 48, y + 48)), (8, 16))
        pad.save(OUT_ART / f"{name}.png")
    print(f"[flame] {len(FLAME_CELLS)} 张 48×48 → 64×64 画布 → {OUT_ART}")
    if not TH18_ITEM_PNG.exists():
        sys.exit(f"缺 TH18 道具贴图 {TH18_ITEM_PNG}（先 python3 tooling/thtk/unpack.py th18.v1.00a）")
    im = Image.open(TH18_ITEM_PNG).convert("RGBA")
    for name, (x, y, w, h) in ITEM_CELLS.items():
        cell = im.crop((x, y, x + w, y + h))
        if (w, h) != (16, 16):
            cell = cell.resize((16, 16), Image.LANCZOS)
        cell.save(OUT_ART / f"{name}.png")
    print(f"[item] {len(ITEM_CELLS)} 张 16×16（TH18 道具图标，源 bullet/item.png）→ {OUT_ART}")
    for name, fn in CARD_ITEM.items():
        p = ABILITY_CARDS / fn
        if not p.exists():
            sys.exit(f"缺原生卡面 {p}（先 python3 tooling/thtk/unpack.py th18.v1.00a）")
        art = Image.open(p).convert("RGBA")
        bb = art.getchannel("A").getbbox()               # 原生 _min.png 是 64×128 画布，卡面只占上半 64×80
        if bb is None:
            sys.exit(f"{p}: 整张透明，切不出卡面")
        art = art.crop(bb)
        w, h = art.size
        sc = min(64.0 / w, 64.0 / h)                     # 64×80 → 等比缩到塞得进 64×64（→51×64）
        art = art.resize((max(1, int(w * sc)), max(1, int(h * sc))), Image.LANCZOS)
        pad = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
        pad.paste(art, ((64 - art.size[0]) // 2, (64 - art.size[1]) // 2))
        pad.save(OUT_ART / f"{name}.png")
    im = _sheet("beastap@beast@12.png")
    for name, (x, y, w, h) in BEASTAP_CELLS.items():
        pad = Image.new("RGBA", (512, 32), (0, 0, 0, 0))
        pad.paste(im.crop((x, y, x + w, y + h)), ((512 - w) // 2, 0))
        pad.save(OUT_ART / f"{name}.png")
    print(f"[beastap] {len(BEASTAP_CELLS)} 张 272×32 → 512×32 画布 → {OUT_ART}")
    # EAGLE 卡侧矩形束：TH17 pl01.anm entry0 sprite29 的 512×32 激光条
    if not EAGLE_BEAM_PNG.is_file():
        sys.exit(f"缺 {EAGLE_BEAM_PNG}（先 python3 tooling/thtk/unpack.py th18.v1.00a）")
    im = Image.open(EAGLE_BEAM_PNG).convert("RGBA")
    strip = im.crop((0, 288, 256, 320))            # sprite29 实际像素行；左右无缝接成 512
    pad = Image.new("RGBA", (512, 32), (0, 0, 0, 0))
    pad.paste(strip, (0, 0))
    pad.paste(strip, (256, 0))
    pad.save(OUT_ART / "EAGLE_BEAM.png")
    print("[eagle_beam] TH17 pl01.anm sprite29 → 512×32 → {OUT_ART / 'EAGLE_BEAM.png'}")
    print(f"[card] {len(CARD_ITEM)} 张资源卡面（原生 _min 64×80 → 64×64）→ {OUT_ART}")
    print(f"[extra] {len(EXTRA)} 张 → {OUT_ART}")


def extract_script(text: str, n: int) -> str:
    m = re.search(rf"^script script{n} \{{.*?^\}}$", text, re.S | re.M)
    if not m:
        sys.exit(f"beast.anm.txt 里找不到 script{n}")
    return m.group(0)


def gen_scripts():
    text = (BEAST / "beast.anm.txt").read_text(encoding="utf-8")
    OUT_SCRIPTS.mkdir(parents=True, exist_ok=True)
    for num, out_name, src_num, mapping in SCRIPTS:
        body = extract_script(text, src_num)
        # ★ 注释必须写在 `script scriptNN {` **里面**：build_ability.py 检查正文第一行就是脚本头
        body = body.replace(
            f"script script{src_num} {{",
            f"script script{num} {{\n"
            f"    // 由 assets/ability/make_animal_art.py 从 TH17 `beast.anm` script{src_num} 生成，别手改。\n"
            f"    // 一手：engine/animal/th17/01-animal-spirits.md §4 / §7；贴图 = 原版 beast*.png 切图（见 ORDER.txt）。", 1)
        for sidx, sprite_name in mapping.items():
            body = body.replace(f"sprite(sprite{sidx})", f"sprite(@{sprite_name})")
        if out_name == "slot_flame":
            # ★ 锚心 + 底边（TH17 script24 没写 anchor ⇒ 事后核对：引擎发的 27..31 放大后是**往上长**的，
            #   等价于锚在底边）。C 侧按 (32,56) 补位置，见 animal.c 的 ANIMAL_FLAME_ANCHOR_*。
            body = body.replace("sprite(@FLAME_0);", "sprite(@FLAME_0);\n    anchor(0, 2);", 1)
        if out_name == "beastap":
            # 父脚本 script44 自带 `pos(0,-128)`，位置交给 C 每帧写 ⇒ 删掉；layer 20 → 25（本卡 UI VM 的层）
            body = "\n".join(l for l in body.split("\n") if not l.strip().startswith("pos("))
            body = body.replace("layer(20);", "layer(25);")
        if out_name in ("slot_soul", "slot_flame"):
            # ★ 原版 script19/20.. 每个槽的脚本里自带 `pos(-360+36i, 872)`（TH17 的槽位）；
            #   我们是 C 侧 `ce_anm_set_pos` 定位，**必须把这行删掉**，否则第一帧会把灵魂顶到屏幕外。
            body = "\n".join(l for l in body.split("\n") if not l.strip().startswith("pos("))
            # ★ layer 20 = 相机 1，和图标（layer 25 = 相机 2）不是同一绘制空间 → 实跑空槽整体偏右半屏。
            #   统一到 25（`engine/anm/th18/02` §0：层 24+ 才是清零 2D 偏移的相机 2 UI）。
            body = body.replace("layer(20);", "layer(25);")   # 20=相机1，25=相机2（与图标同空间）
        for a, b in SCRIPT_REF_REMAP.items():
            body = body.replace(f"scriptNew(script{a})", f"scriptNew(script{b})")
        (OUT_SCRIPTS / f"{num}_{out_name}.anm.txt").write_text(body + "\n", encoding="utf-8")
        print(f"[script] {num}_{out_name}.anm.txt  ← beast.anm script{src_num}")


def print_order():
    print("\n# —— 追加到 ability/entries/ORDER.txt（只追加）——")
    for name in TOKEN_CELLS:
        print(f"{name}  ability/animal/{name}.png  32x32")
    for name in AURA_CELLS:
        print(f"{name}  ability/animal/{name}.png  64x64")
    for name in SOUL_CELLS:
        print(f"{name}  ability/animal/{name}.png  16x16")
    for name in FLAME_CELLS:
        print(f"{name}  ability/animal/{name}.png  64x64")
    for name in ITEM_CELLS:
        print(f"{name}  ability/animal/{name}.png  16x16")
    for name in CARD_ITEM:
        print(f"{name}  ability/animal/{name}.png  64x64")
    for name in BEASTAP_CELLS:
        print(f"{name}  ability/animal/{name}.png  512x32")
    print("EAGLE_BEAM  ability/animal/EAGLE_BEAM.png  512x32")
    for name, (fn, x, y, w, h, canvas) in EXTRA.items():
        print(f"{name}  ability/animal/{name}.png  {canvas[0]}x{canvas[1]}")


def main():
    crop_sheets()
    gen_scripts()
    print_order()


if __name__ == "__main__":
    main()
