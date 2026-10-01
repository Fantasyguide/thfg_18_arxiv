#!/usr/bin/env python3
"""星星炮（id 89）被动用的**死亡讯息贴图** —— 照泰拉瑞亚内部 UI 的样式烘一张 PNG。

    python3 assets/ability/star_cannon/make_death_msg.py

产物：`assets/ability/star_cannon/_src/DEATH_MSG.png`（384×112，RGBA），再由
`assets/ability/entries/ORDER.txt` 的 `STAR_CANNON_DEATH_MSG` 那一行进 `ability.anm`
（脚本 383 把它钉在 layer 25；位置 / 淡出由 `cards/star_cannon.c` 每帧写）。

为什么是**烘图**而不是引擎文字：TH18 的 ASCII 字体图集（`font2` / `th13_big` / `th12`）
**没有汉字字形**，而讯息是中文 ⇒ 只能预渲染（owner 2026-10-02 也是这么建议的
「推荐是按照泰拉瑞亚的样式做一个贴图」）。

样式口径（泰拉瑞亚 1.4 的死亡界面 + 掉钱提示）：
· 第一行「你被杀死了…」= 原版死亡大字（`FontAssets.DeathText`，**红色**、带**黑色描边**）；
· 第二行 = 掉落的钱财，**每个币种用自己的颜色**（铂金 / 金 / 银 / 铜），单位字与数字同色。
⚠ 颜色值是**按泰拉瑞亚的观感取的**（原版死亡大字的具体 RGBA 与币种配色仍在核，见 `AUDIT.md`
  §BE 详录 #213）；核实后只改本文件的常量即可重新烘图，别处不用动。"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "_src", "DEATH_MSG.png")

W, H = 384, 104

# ★★ 2026-10-02：owner 给了一张**实机截图**当参考（`/tmp/death_ref.png`，299×109）。按像素量出来的口径：
#   · 两行都**水平居中**，行 1 字高 ≈43 px、行 2 ≈19 px，**行间距极紧**（≈2 px）；
#   · **整条讯息是同一种浅灰/暖白**（字身均色 ≈ RGB(210,178,174)、最亮 ≈ RGB(226,221,220)），
#     逐段采样（掉了/591/铂金/60/金/15/银/3/铜）**没有币种配色差异** ⇒ 这里**不做**分币种上色；
#   · 描边/阴影是深色（接近黑），数字是细体（不是粗体）。
# ⚠ 颜色/字号最终以 subagent 反出来的**一手 Color/字号**为准（见 AUDIT §BE 详录 #213），
#   核实后只改本文件的常量重烘即可，别处不用动。
OUTLINE = (0, 0, 0, 235)          # 描边（深色，略透一点让边缘不死板）
DEATH_TEXT = (232, 226, 224)      # 全条讯息的字色（浅暖白；截图实测最亮 ≈ (226,221,220)）
COIN_TEXT = (222, 216, 214)       # 第二行同色（截图证实**不分币种**）

LINE1 = "你被杀死了…"
# 第二行分段：[(文本, RGB)] —— 逐段列出（**同一色**；分段是为了以后真要上色时一行就能改）
LINE2 = [("掉了", COIN_TEXT),
         ("591", COIN_TEXT), ("铂金", COIN_TEXT),
         ("60", COIN_TEXT),  ("金",   COIN_TEXT),
         ("15", COIN_TEXT),  ("银",   COIN_TEXT),
         ("3",  COIN_TEXT),  ("铜",   COIN_TEXT)]

# 汉字字形：Droid Sans Fallback（无衬线、最像游戏 UI）—— ⚠ 它是**fallback 字体**，
#   只有 CJK（连 ASCII 数字都没有！第一版把 591/60/15/3 烘成了豆腐块）。
CJK_FONT = "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf"
# 数字/拉丁：DejaVu Sans Bold（粗、清晰；泰拉瑞亚的 `Death_Text` 是 SpriteFont XNB，
#   xnbcli 解不出字形图集 ⇒ 拿不到原版字形，用系统里最接近的粗无衬线顶上）。
LAT_FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
# ★★ 2026-10-02：owner 给了一张**实机截图**当参考（`/tmp/death_ref.png`，299×109）。按像素量出来的口径：
#   · 两行都**水平居中**，行 1 字高 ≈43 px、行 2 ≈19 px，**行间距极紧**（≈2 px）；
#   · **整条讯息是同一种浅灰/暖白**（字身均色 ≈ RGB(210,178,174)、最亮 ≈ RGB(226,221,220)），
#     逐段采样（掉了/591/铂金/60/金/15/银/3/铜）**没有币种配色差异** —— 所以这里**不做**分币种上色；
#   · 描边/阴影是深色（接近黑），数字是细体。
# ⚠ 这里烘出来的画布比例（384×104）与截图一致；颜色/字号仍以 subagent 反出来的**一手 Color/字号**
#   为准（见 AUDIT §BE 详录 #213），核实后改本文件的常量重烘即可。
OUTLINE = (0, 0, 0, 235)          # 描边（深色，略透一点让边缘不死板）
DEATH_TEXT = (232, 226, 224)      # 全条讯息的字色（浅暖白；截图实测最亮 ≈ (226,221,220)）
COIN_TEXT = (222, 216, 214)       # 第二行同色（截图证实**不分币种**）


def load_font(path, size):
    try:
        return ImageFont.truetype(path, size)
    except Exception:
        return ImageFont.load_default()


_CMAP_CACHE = {}


def has_glyph(path, ch):
    """该字体有没有这个字的字形（用 fontTools 读 cmap；Droid Sans Fallback **没有 ASCII 数字
    也没有 U+2026**，所以不能只按"是不是汉字"分字体）。"""
    cm = _CMAP_CACHE.get(path)
    if cm is None:
        from fontTools.ttLib import TTFont, TTCollection
        f = TTCollection(path).fonts[0] if path.endswith(".ttc") else TTFont(path)
        cm = f.getBestCmap()
        _CMAP_CACHE[path] = cm
    return ord(ch) in cm


def split_runs(text, cjk_path, lat_path):
    """把一段文本按"谁有这个字形"切成若干 (子串, 用哪个字体) 的连续段。"""
    runs = []
    for ch in text:
        use_cjk = has_glyph(cjk_path, ch) or not has_glyph(lat_path, ch)
        if runs and runs[-1][1] == use_cjk:
            runs[-1][0] += ch
        else:
            runs.append([ch, use_cjk])
    return [(s, 1 if c else 0) for s, c in runs]


def layout(pieces, cjk_path, lat_path, cjk, lat):
    """把 (文本, 颜色) 展平成 (子串, 颜色, 字体) 并按**字符级**字体回退分好。"""
    out = []
    for text, col in pieces:
        for sub, is_cjk in split_runs(text, cjk_path, lat_path):
            out.append((sub, col, cjk if is_cjk else lat))
    return out


def draw_centered(d, y, pieces, cjk_path, lat_path, cjk, lat, outline=2):
    """把若干 (文本, 颜色) 段居中排在同一行（y = 该行顶部）。返回该行的宽度。"""
    runs = layout(pieces, cjk_path, lat_path, cjk, lat)
    widths = [d.textlength(s, font=f) for s, _, f in runs]
    total = sum(widths)
    x = (W - total) / 2.0
    for (s, col, f), w in zip(runs, widths):
        d.text((x, y), s, font=f, fill=col + (255,), stroke_width=outline, stroke_fill=OUTLINE)
        x += w
    return total


def main():
    global W
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # 字号按截图的比例（行 1 字高 43 / 行 2 字高 19 ⇒ 约 2.26 : 1）
    cjk1, lat1 = load_font(CJK_FONT, 42), load_font(LAT_FONT, 42)
    cjk2, lat2 = load_font(CJK_FONT, 19), load_font(LAT_FONT, 19)
    # 先量一遍，把画布放宽到能装下（两行都留 4 px 边），再按最终宽度重画一次
    need = max(sum(d.textlength(s, font=f) for s, _, f in layout([(LINE1, DEATH_TEXT)], CJK_FONT, LAT_FONT, cjk1, lat1)),
               sum(d.textlength(s, font=f) for s, _, f in layout(LINE2, CJK_FONT, LAT_FONT, cjk2, lat2))) + 8
    if need > W:
        W = int(need + 0.5)
        img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
    draw_centered(d, 2, [(LINE1, DEATH_TEXT)], CJK_FONT, LAT_FONT, cjk1, lat1, outline=3)
    draw_centered(d, 62, LINE2, CJK_FONT, LAT_FONT, cjk2, lat2, outline=2)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT)
    print(f"DEATH_MSG: {OUT} {img.width}x{img.height}")


if __name__ == "__main__":
    main()
