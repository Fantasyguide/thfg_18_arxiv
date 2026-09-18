#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_system_cardart.py —— 给系统卡（106–118）**合成卡面草图**：底框 + 各作 exe 图标 + 作名。

    python3 tools/extract_exe_icons.py  --out <dir>/icons      # 先抽图标（见那个脚本）
    python3 tools/make_system_cardart.py --icons <dir>/icons --out <dir>/system

## 为什么这么做（用户 2026-09-18 口径，逐字）

- 「我看一下现在所有非原生卡的卡面美术，你复制到windows侧」⇒ 导出一看：**13 张系统卡共用原生
  `abcard.anm` 的 `116`/`117`**，而那两个 entry 就是 `ability/empty_max.png`（一个**只有黑框、
  中间全透明**的空卡）与 `ability/dummy.png`（64×128，不透明像素 0.8%）⇒ 系统卡**在牌组/商店里是空白的**。
- 「**你把每作的 exe 图标做进来图片**」⇒ 本脚本把每作 exe 的图标（`extract_exe_icons.py` 抽出来的）
  放进卡面：**原生黑框当边框**（保持 TH18 的卡形）+ 中间一块底板 + 该作图标 + 作名。
- 「**卡图就用这个吧**」（2026-09-18：白底 + 图标最大居中 + `cardframe.py` 的原生框）⇒ 106–117 定稿样式：
  `--style white`（默认）就是它；旧的深色底板 + 作名版（`--style plate`）留作对照。
- 「**然后十二合一就拼一个 3*4 的，然后最大居中**」⇒ **118** 特殊处理：把 106–117 那 12 个图标按卡号
  摆成 **3 列 × 4 行**的图标墙（左→右、上→下），整墙尽量大且居中（详见 `compose_art_mosaic()` 的算式）。

⚠ 这是**草图/提案**（`❓ 自定`）：底框与图标是真资源，配色/排版是我定的；没进 `assets/cards/`、
没进任何 patch —— 只是导到能看的目录里，等你点头再走正式的卡面流程
（`assets/cards/ORDER.txt` 追加 + `_max`/`_min` + `make abcard`，build 期有硬门校验）。

## 输出

- `<out>/<id>_<THtag>_max.png`（256×320，与 `assets/cards/*_max.png` 同规格）
- `<out>/<id>_<THtag>_min.png`（64×80，= max 的 1/4，同 `*_min.png` 规格）
- `<out>/index.html`（并排看；缺样本的作画成占位卡并写明原因）
"""
import argparse
import html
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

REPO = Path(__file__).resolve().parents[4]
ASSETS = Path(__file__).resolve().parents[1] / "assets"
sys.path.insert(0, str(ASSETS))          # ★ 复用仓库既有的卡框脚本（别再自己画框！）
import cardframe  # noqa: E402
CARDS_JS = REPO / "mods" / "th18.v1.00a" / "card-expand" / "patch" / "th18" / "cards.js"
CJK_FONT = "/usr/share/fonts/truetype/arphic/uming.ttc"      # AR PL UMing（有简体字；系统自带）
LAT_FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

# 系统卡 ↔ 作（与 `patch/th18/cards.js` 的 id/内部名一致；THtag 用来找图标）
SYSTEM = [
    ("106", "EOSD_PLAYRANK",  "th06", "红魔乡"),
    ("107", "PCB_CHERRY",     "th07", "妖妖梦"),
    ("108", "IN_HUMAN_YOUKAI","th08", "永夜抄"),
    ("109", "POFV_BLAST",     "th09", "花映塚"),
    ("110", "MOF_FAITH",      "th10", "风神录"),
    ("111", "SA_COMM",        "th11", "地灵殿"),
    ("112", "UFO",            "th12", "星莲船"),
    ("113", "TD_SPIRIT",      "th13", "神灵庙"),
    ("114", "COLLECT_2X",     "th14", "辉针城"),
    ("115", "LOLK_CHAPTER",   "th15", "绀珠传"),
    ("116", "SEASON",         "th16", "天空璋"),
    ("117", "WBAWC",          "th17", "鬼形兽"),
    ("118", "GIVE12",         "th18", "十二合一"),
]

W, H = 256, 320                 # 与 assets/cards/*_max.png 同规格
# ★★ 画面区 = `assets/cardframe.py` 的一手口径：外圈 3 px 黑 → 13 px 斜面 → 2 px 黑 ⇒ 每边内缩 18，
#    画面区 **220×284**。**框一律交给 cardframe.apply_frame()**（它逐像素量自原生 KANAME_max/ALICE_OP_max），
#    我们不自己画框（第一版手画的 8 px 黑框被用户当场否了）。
AW, AH = cardframe.W - 2 * cardframe.INSET, cardframe.H - 2 * cardframe.INSET
M = cardframe.INSET             # 兼容旧命名（= 18）
PLATE_TOP = (34, 38, 52, 255)   # 旧「深色底板」样式的渐变色（`--style plate` 用；❓ 自定）
PLATE_BOT = (16, 18, 26, 255)
ACCENT    = (255, 226, 120, 255)
WHITE     = (255, 255, 255, 255)
PAD       = 6                   # 白底样式里图标与黑框之间留的空白（❓ 自定；0 = 贴着框）


def load_font(path, size):
    try:
        return ImageFont.truetype(path, size, index=0)
    except Exception:
        try:
            return ImageFont.truetype(path, size)
        except Exception:
            return ImageFont.load_default()


def plate(dst: Image.Image):
    """在黑框内部铺一块竖直渐变底板 + 细内边。"""
    d = ImageDraw.Draw(dst)
    inner = [M, M, W - M - 1, H - M - 1]
    for i in range(inner[3] - inner[1] + 1):
        t = i / max(1, inner[3] - inner[1])
        c = tuple(int(PLATE_TOP[k] + (PLATE_BOT[k] - PLATE_TOP[k]) * t) for k in range(4))
        d.line([inner[0], inner[1] + i, inner[2], inner[1] + i], fill=c)
    d.rectangle([inner[0] + 2, inner[1] + 2, inner[2] - 2, inner[3] - 2], outline=(90, 96, 120, 255))


SCALE_MODE = "nearest"          # nearest = 整数倍最近邻（像素画**不糊**，最大 224/32）；fill = 铺满内区（更big但有插值）


def fit(icon: Image.Image, avail: int) -> Image.Image:
    """把图标放进 `avail×avail` 的方框：`nearest` 走整数倍（默认），`fill` 铺满（插值）。"""
    if SCALE_MODE == "fill":
        k = avail / max(icon.width, icon.height)
        return icon.resize((max(1, round(icon.width * k)), max(1, round(icon.height * k))), Image.LANCZOS)
    k = max(1, avail // max(icon.width, icon.height))
    return icon.resize((icon.width * k, icon.height * k), Image.NEAREST)


MOSAIC_COLS, MOSAIC_ROWS = 3, 4       # ★ 118「十二合一」= 3×4 拼图（正好 12 作：106–117）


def compose_art_mosaic(icons: list[Image.Image]) -> Image.Image:
    """★ 用户 2026-09-18 口径：「**十二合一就拼一个 3×4 的，然后最大居中**」。

    画面区（220×284）里铺一张 **3 列 × 4 行** 的图标墙（顺序 = 卡 106→117 = TH06→TH17，逐行排），
    **尽量大且居中**、其余纯白。格子是正方形 ⇒ 受两者约束：
      `3c ≤ 220−2·PAD` 且 `4c ≤ 284−2·PAD` ⇒ `c ≤ 68`（PAD=6）；
      `nearest` 模式再取整到 32 的整数倍（×2 = **64**，不糊），`fill` 模式直接 **68**（有插值）。
    """
    im = Image.new("RGBA", (W, H), WHITE)
    avail_w, avail_h = AW - 2 * PAD, AH - 2 * PAD
    cell = max(1, min(avail_w // MOSAIC_COLS, avail_h // MOSAIC_ROWS))
    if SCALE_MODE == "nearest" and icons:
        k = max(1, cell // max(icons[0].width, icons[0].height))
        cell = min(cell, icons[0].width * k, icons[0].height * k)
    gw, gh = cell * MOSAIC_COLS, cell * MOSAIC_ROWS
    ox, oy = (W - gw) // 2, (H - gh) // 2
    for i, ic in enumerate(icons[:MOSAIC_COLS * MOSAIC_ROWS]):
        r, c = divmod(i, MOSAIC_COLS)
        if SCALE_MODE == "fill":
            tile = ic.resize((cell, cell), Image.LANCZOS)
        else:
            tile = ic.resize((cell, cell), Image.NEAREST)
        im.alpha_composite(tile, (ox + c * cell, oy + r * cell))
    return im


def compose_art(icon: Image.Image | None) -> Image.Image:
    """★ 用户 2026-09-18 口径：「把 icon 补齐成卡面的样子，**最大居中，剩下补充白色**」。

    产出 = **256×320 无框画面**（`_art` 口径，带 `renkolab-art=area` 标记）：纯白底 + 图标按最大
    等比放大后居中。图标尺寸照**画面区**（220×284）算 ⇒ 外圈那 18 px 会被 `cardframe.frame_card()`
    1:1 裁掉、换成原生框，效果与设计稿逐像素一致；**不画框** —— 加框交给 `cardframe.py`。
    `❓ 自定`：`PAD` 是画面区里图标四周的留白（默认 6 px）。"""
    im = Image.new("RGBA", (W, H), WHITE)
    d = ImageDraw.Draw(im)
    avail = min(AW - 2 * PAD, AH - 2 * PAD)
    if icon is not None:
        big = fit(icon, avail)
        im.alpha_composite(big, ((W - big.width) // 2, (H - big.height) // 2))
    else:
        f_q = load_font(LAT_FONT, 72)
        d.text((W // 2, H // 2), "?", font=f_q, fill=(200, 200, 205, 255), anchor="mm")
    return im


def compose(icon: Image.Image | None, cn: str, tag: str, cid: str) -> Image.Image:
    """旧样式（深色渐变底板 + 中文作名 + THxx / CARD nnn）—— `--style plate` 时用。"""
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    plate(im)
    d = ImageDraw.Draw(im)
    f_cn = load_font(CJK_FONT, 22)
    f_sm = load_font(LAT_FONT, 13)
    f_id = load_font(LAT_FONT, 15)
    if icon is not None:
        k = 4                                   # 32×32 → 128×128（最近邻保像素感）
        big = icon.resize((icon.width * k, icon.height * k), Image.NEAREST)
        if big.width > W - 2 * M - 24:
            big = icon.resize((W - 2 * M - 24, W - 2 * M - 24), Image.NEAREST)
        im.alpha_composite(big, ((W - big.width) // 2, 74))
    else:
        # 缺样本：画一个占位问号，别拿别作的图标冒充
        d.rectangle([70, 106, W - 70, 106 + 96], outline=(150, 150, 160, 255), width=3)
        f_q = load_font(LAT_FONT, 64)
        d.text((W // 2, 106 + 48), "?", font=f_q, fill=(150, 150, 160, 255), anchor="mm")
    # 作名（中文）+ TH 号（ASCII）
    d.text((W // 2, 236), cn, font=f_cn, fill=(240, 240, 245, 255), anchor="mm")
    d.text((W // 2, 262), tag.upper(), font=f_sm, fill=(170, 176, 200, 255), anchor="mm")
    d.text((W // 2, 286), "CARD " + cid, font=f_id, fill=ACCENT, anchor="mm")
    # 原生黑框盖在最上面（保持 TH18 卡形）
    d.rectangle([0, 0, W - 1, H - 1], outline=(0, 0, 0, 255), width=M)
    return im


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--icons", required=True, help="extract_exe_icons.py 的输出目录")
    ap.add_argument("--out", required=True)
    ap.add_argument("--scale", choices=("nearest", "fill"), default="nearest",
                    help="nearest（默认，整数倍最近邻=像素画不糊，最大 224）/ fill（铺满内区=更大但有插值）")
    ap.add_argument("--style", choices=("white", "plate"), default="white",
                    help="white（默认，用户口径：白底 + 图标最大居中）/ plate（旧：深色底板 + 作名）")
    a = ap.parse_args()
    global SCALE_MODE                      # ★ 修 bug：以前只 declare 了 --scale，却没把它接进来（选项被静默忽略）
    SCALE_MODE = a.scale
    icons = Path(a.icons)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)

    # ★ 118 的拼图要按 106→117 的顺序取 12 个图标
    mosaic_icons = []
    for cid, nm, tag, cn in SYSTEM:
        if cid == "118":
            continue
        q = icons / f"{cid}_{tag}.png"
        if q.is_file():
            mosaic_icons.append(Image.open(q).convert("RGBA"))

    rows = []
    for cid, nm, tag, cn in SYSTEM:
        p = icons / f"{cid}_{tag}.png"
        icon = Image.open(p).convert("RGBA") if p.is_file() else None
        art = compose_art_mosaic(mosaic_icons) if cid == "118" else compose_art(icon)
        (out / "art").mkdir(exist_ok=True)
        art_path = out / "art" / f"{cid}_{tag}_art.png"
        cardframe.save_art(art, art_path)                       # ★ 无框画面（带 `renkolab-art=area` 标记）= 真源

        mx = out / f"{cid}_{tag}_max.png"
        mn = out / f"{cid}_{tag}_min.png"
        if a.style == "white":
            # ★ 原生同款框，1:1 贴（`area=True` 不吃重采样 ⇒ 整数倍放大的像素画不会被缩糊）
            cardframe.save_framed(cardframe.frame_card(art, area=True), mx)
        else:
            compose(icon, cn, tag, cid).save(mx)
        # ★ `_min` 也走 `cardframe.py` 的同一套函数（单一真源；`area=True` = 内容外框等比缩进 64×80）
        cardframe.min_image_from_art(art, area=True).save(mn)
        n_icons = len(mosaic_icons) if cid == "118" else (1 if p.is_file() else 0)
        rows.append((cid, cn, nm, tag, mx.name, mn.name, n_icons > 0))
        print(f"  {cid} {cn:6s} {tag} " + (f"3×4 拼图（{n_icons} 个图标）" if cid == "118"
              else ('图标✅' if p.is_file() else '图标❌（缺样本）')) + f" → {mx.name} / {mn.name}")

    h = ['<!doctype html><meta charset="utf-8"><title>系统卡卡面草图（exe 图标版）</title>',
         '<style>body{background:#16161a;color:#eee;font:14px/1.6 sans-serif}'
         'table{border-collapse:collapse}td,th{border:1px solid #444;padding:6px 10px;vertical-align:middle}'
         'img{image-rendering:pixelated}th{background:#26262c}</style>',
         '<h2>系统卡（106–118）卡面草图 —— 底框 = 原生空卡面，图 = 该作 exe 图标</h2>',
         '<p>这是<b>提案草图</b>：黑框与图标是真资源，配色/排版是我定的。带 ❌ 的两作<b>本机没有样本</b>，'
         '所以画了占位问号（不拿别作的图标冒充）。</p>',
         '<table><tr><th>id</th><th>作</th><th>内部名</th><th>图标</th><th>大卡面 256×320</th><th>小卡面 64×80</th></tr>']
    for cid, cn, nm, tag, mx, mn, has in rows:
        h.append('<tr><td>{}</td><td>{} {}</td><td>{}</td><td>{}</td>'
                 '<td><img src="{}" height="240"></td><td><img src="{}"></td></tr>'.format(
                     cid, html.escape(cn), tag, nm, "✅" if has else "❌ 缺样本", mx, mn))
    h.append('</table>')
    (out / "index.html").write_text("\n".join(h), encoding="utf-8")
    print(f"写出 {len(rows)} 张卡的草图 → {out}（打开 index.html 看）")
    print("⚠ 草图不是正式卡面：要落地走 assets/cards/ORDER.txt + make abcard（build 期硬门）。")


if __name__ == "__main__":
    sys.exit(main())
