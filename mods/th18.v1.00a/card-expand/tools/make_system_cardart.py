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
M = 8                           # 原生空卡面的黑框厚度（实测：8×10 网格里最外一圈全是 #000000）
PLATE_TOP = (34, 38, 52, 255)   # 底板渐变色（❓ 自定）
PLATE_BOT = (16, 18, 26, 255)
ACCENT    = (255, 226, 120, 255)


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


def compose(icon: Image.Image | None, cn: str, tag: str, cid: str) -> Image.Image:
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


def compose_min(icon: Image.Image | None) -> Image.Image:
    """小卡面（64×80）：1/4 尺寸下**文字读不出来**，所以只放图标（+ 原生黑框）。
    `❓ 自定`：这是"缩略图只认图标"的取舍，实测一行中文在 64 px 宽里只剩几个像素。"""
    w, h, m = 64, 80, 3
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for i in range(h - 2 * m):
        t = i / max(1, h - 2 * m - 1)
        c = tuple(int(PLATE_TOP[k] + (PLATE_BOT[k] - PLATE_TOP[k]) * t) for k in range(4))
        d.line([m, m + i, w - m - 1, m + i], fill=c)
    if icon is not None:
        side = 40
        big = icon.resize((side, side), Image.NEAREST)
        im.alpha_composite(big, ((w - side) // 2, (h - side) // 2))
    else:
        f_q = load_font(LAT_FONT, 28)
        d.text((w // 2, h // 2), "?", font=f_q, fill=(150, 150, 160, 255), anchor="mm")
    d.rectangle([0, 0, w - 1, h - 1], outline=(0, 0, 0, 255), width=m)
    return im


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--icons", required=True, help="extract_exe_icons.py 的输出目录")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    icons = Path(a.icons)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)

    rows = []
    for cid, nm, tag, cn in SYSTEM:
        p = icons / f"{cid}_{tag}.png"
        icon = Image.open(p).convert("RGBA") if p.is_file() else None
        img = compose(icon, cn, tag, cid)
        mx = out / f"{cid}_{tag}_max.png"
        mn = out / f"{cid}_{tag}_min.png"
        img.save(mx)
        compose_min(icon).save(mn)
        rows.append((cid, cn, nm, tag, mx.name, mn.name, p.is_file()))
        print(f"  {cid} {cn:6s} {tag} 图标{'✅' if p.is_file() else '❌（缺样本）'} → {mx.name} / {mn.name}")

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
