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


def compose_art(icon: Image.Image | None) -> Image.Image:
    """★ 用户 2026-09-18 口径：「把 icon 补齐成卡面的样子，**最大居中，剩下补充白色**」。

    产出 = **画面区那一块**（220×284，无框）：纯白底 + 图标按最大等比放大后居中，其余全白。
    **不画框** —— 加框交给 `cardframe.apply_frame()`（原生同款，见 `assets/cardframe.py`）。
    `❓ 自定`：`PAD` 是画面区里图标四周的留白（默认 6 px）。"""
    im = Image.new("RGBA", (AW, AH), WHITE)
    d = ImageDraw.Draw(im)
    avail = min(AW - 2 * PAD, AH - 2 * PAD)
    if icon is not None:
        big = fit(icon, avail)
        im.alpha_composite(big, ((AW - big.width) // 2, (AH - big.height) // 2))
    else:
        f_q = load_font(LAT_FONT, 72)
        d.text((AW // 2, AH // 2), "?", font=f_q, fill=(200, 200, 205, 255), anchor="mm")
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


def compose_min(icon: Image.Image | None) -> Image.Image:
    """小卡面（64×80）：★ **原生 `_min` 不带框**（`cardframe.py` 头注：「HUD 卡组图标不带框」）
    ⇒ 白底 + 图标最大居中，**不画任何框**。
    `❓ 自定`：直接按 64×80 合成（而不是把 220×284 的画面区缩下来）—— 32×32 图标正好 ×2 = 64，
    像素级对齐；照 `save_min_from_art()` 缩会有 ~3% 形变。"""
    w, h = cardframe.MIN_W, cardframe.MIN_H
    im = Image.new("RGBA", (w, h), WHITE)
    d = ImageDraw.Draw(im)
    if icon is not None:
        big = fit(icon, min(w, h))
        im.alpha_composite(big, ((w - big.width) // 2, (h - big.height) // 2))
    else:
        f_q = load_font(LAT_FONT, 28)
        d.text((w // 2, h // 2), "?", font=f_q, fill=(200, 200, 205, 255), anchor="mm")
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
    icons = Path(a.icons)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)

    rows = []
    for cid, nm, tag, cn in SYSTEM:
        p = icons / f"{cid}_{tag}.png"
        icon = Image.open(p).convert("RGBA") if p.is_file() else None
        art = compose_art(icon)
        (out / "art").mkdir(exist_ok=True)
        art.save(out / "art" / f"{cid}_{tag}_art.png")          # 无框画面（留档，便于重出）
        mx = out / f"{cid}_{tag}_max.png"
        mn = out / f"{cid}_{tag}_min.png"
        if a.style == "white":
            cardframe.save_framed(cardframe.apply_frame(art), mx)   # ★ 原生同款框 + `renkolab-frame=1` 标记
        else:
            compose(icon, cn, tag, cid).save(mx)
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
