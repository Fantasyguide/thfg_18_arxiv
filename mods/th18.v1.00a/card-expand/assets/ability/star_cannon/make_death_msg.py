#!/usr/bin/env python3
"""星星炮（id 89）被动用的**死亡讯息贴图** —— 照泰拉瑞亚死亡界面的**一手规格**烘一张 PNG。

    python3 assets/ability/star_cannon/make_death_msg.py

产物：`assets/ability/star_cannon/_src/DEATH_MSG.png`（RGBA），再由
`assets/ability/entries/ORDER.txt` 的 `STAR_CANNON_DEATH_MSG` 那一行进 `ability.anm`
（脚本 383 把它钉在 layer 25；位置 / 显隐由 `cards/star_cannon.c` 每帧写）。

为什么是**烘图**：TH18 的 ASCII 字体图集（`font2` / `th13_big` / `th12`）**没有汉字字形**，
而讯息是中文 ⇒ 只能预渲染（owner 2026-10-02 也是这么建议的「推荐是按照泰拉瑞亚的样式做一个贴图」）。

## ★ 规格来源：`Main.DrawInterface_35_YouDied()` 的 **IL 级一手**（见 AUDIT.md §BE 详录 #213 追加 B）
本机 Steam 版实际是 **1.4.5.8**，参照版是 tModLoader 的 **1.4.4.9** —— 这块代码两版逐条一致。

| 项 | 一手结论 | 本文件 |
| --- | --- | --- |
| 第一行文本 | `Lang.inter[38]` = key **`LegacyInterface.38`**，zh-Hans = **「你被杀死了……」**（两个 U+2026）| 同 |
| 第二行文本 | `Language.GetTextValue("Game.DroppedCoins", lostCoinString)`，zh-Hans = **「掉了{0}」**；`{0}` 来自 `Main.ValueToCoins`，其格式串 `"{0} {1} "` **硬编码在 exe、带空格、不可本地化** ⇒ 实机是 **「掉了591 铂金 60 金 15 银 3 铜」** | 同 |
| 颜色 | `player.GetDeathAlpha(Color.Transparent)`：`Color.Transparent` = RGBA(0,0,0,0)，死亡期间 `immuneAlpha` 恒 255 ⇒ **RGBA(229,127,127,102)** = 暖红 `#E57F7F` | `DEATH_TEXT = (229,127,127)`（预乘 alpha 下"看得见的就是这个 RGB" ⇒ 直接画不透明）|
| 描边 / 阴影 | **没有**（走 ReLogic 的 12 参 `DrawString` 扩展，两个尾数组传 `null`；`DrawBorderStringBig` 不在这条路径上）| `OUTLINE_WIDTH = 0` |
| 第一行字体 | `FontAssets.DeathText`（`Fonts/Death_Text`，`lineSpacing = 72`）| 系统细无衬线近似（见 `CJK_FONT` 注）|
| 第二行字体 | **`FontAssets.MouseText`**（`Fonts/Mouse_Text`，`lineSpacing = 29`）—— **与第一行不是同一字体** | 同左（更小更窄）|
| 排版 | 两行**各自** `MeasureString` 居中；`origin = Vector2.Zero` ⇒ 记的是**文本盒左上角**；第一行 y = `H/2 − 60`、第二行 `H/2 − 10`（**盒顶差 50 px**）、scale 都 1.0 | `LINE_GAP = 50`、各自居中 |
| 生命周期 | 死亡瞬间出现、**无淡入无淡出**、复活下一帧直接消失 | C 侧照做（见 `star_cannon.c`）|

⚠ **已知差距（🟡，别当成逐像素复刻）**：原版字形在 `Death_Text.xnb` / `Mouse_Text.xnb` 里
（ReLogic `DynamicSpriteFont`：LZX + 159 页 Dxt3、38636 字形）。`xnbcli` 认不出那个自定义 reader；
一次性写的 reader + DXT3 解码器**解出来的图集是横条纹**（不可用）⇒ 这里用系统字体近似，
**字宽/字距与原版不会完全一致**。原版是两行独立绘制、没有"贴图"这个概念；我们是烘成一张图按 sprite 居中放。"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "_src", "DEATH_MSG.png")

# ── 一手规格（改这里之前先读文件头那张表）─────────────────────────────────────────
LINE1 = "你被杀死了……"                        # LegacyInterface.38（zh-Hans）
LINE2 = "掉了591 铂金 60 金 15 银 3 铜"         # Game.DroppedCoins = "掉了{0}"；{0} = ValueToCoins 的实机输出（带空格）
# 一手真值 = RGB(229,127,127)（`GetDeathAlpha(Color.Transparent)`）；★ owner 2026-10-03 实测
# 「有点暗，感觉得调亮一点」⇒ 按 owner 口径提亮到 (255,152,152)（约 1.15×、红不过曝）。
# ⚠ 这是**偏离一手值**的 owner 口径，只影响这一个常量（重烘即可）。
DEATH_TEXT = (255, 152, 152)
LINE_GAP = 50                                  # 一手：两行**盒顶**差 50 px（H/2−60 → H/2−10）
GLYPH_GAP = 2                                  # ★ 但原版大字在那 50 px 里是**靠盒底**画的 ⇒ 实测「第一行字形下沿
                                               #   → 第二行字形上沿」只有 **4 px**（owner 的实机截图量出来是空 4 行）。
                                               #   第一版按 50 px 摆字形的**上沿**，两行当场叠在一起 —— 这里改按
                                               #   **实测字形包围盒**排：第二行盒顶 = 第一行字形下沿 + GLYPH_GAP。
OUTLINE_WIDTH = 0                              # 一手：原版**没有**描边（留成常量，实跑嫌糊再调）

# ★★ 2026-10-03 owner 实测「你被杀死了太大了」⇒ 字号要按**屏幕比例**换算，不能把 1080p 的字号
#   直接搬进 640×480 空间：
#     一手：那行死亡大字在 1080p 屏上高约 **52 px** ⇒ 占屏高 4.8%；我们的空间高 480
#     ⇒ 场上字形高该是 480 × 0.048 ≈ **23 单位**（PIL 的 size 是 em 尺寸，汉字墨高约 0.85×size
#     ⇒ size ≈ 27）。第二行（Mouse_Text，1080p 上约 19 px）同比例 ⇒ 约 9 单位 ⇒ size ≈ 11。
#   ⚠ 上一版写 52 / 21（= 直接把 1080p 的字号当场上单位用）⇒ 整整大一倍多，owner 一眼就看出来了。
#   字号与 `GLYPH_GAP` 是一组：改这两个数重烘即可，贴图尺寸会变（ORDER.txt 与 SC_MSG_Y 要跟着改）。
FONT1_PX = 27
FONT2_PX = 11
CJK_FONT = "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf"   # 汉字：细无衬线（⚠ 它没有 ASCII 数字）
LAT_FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"             # 数字 / 拉丁：细体、不加重

W = 320
H = FONT1_PX + LINE_GAP + FONT2_PX + 8

_CMAP_CACHE = {}


def has_glyph(path, ch):
    """该字体有没有这个字的字形（Droid Sans Fallback **没有 ASCII 数字** ⇒ 不能只按"是不是汉字"分）。"""
    cm = _CMAP_CACHE.get(path)
    if cm is None:
        from fontTools.ttLib import TTFont, TTCollection
        f = TTCollection(path).fonts[0] if path.endswith(".ttc") else TTFont(path)
        cm = f.getBestCmap()
        _CMAP_CACHE[path] = cm
    return ord(ch) in cm


def split_runs(text):
    """按「谁有这个字形」切成若干 (子串, 用哪个字体)。"""
    runs = []
    for ch in text:
        use_cjk = has_glyph(CJK_FONT, ch) or not has_glyph(LAT_FONT, ch)
        if runs and runs[-1][1] == use_cjk:
            runs[-1][0] += ch
        else:
            runs.append([ch, use_cjk])
    return [(s, 1 if c else 0) for s, c in runs]


def load(path, size):
    try:
        return ImageFont.truetype(path, size)
    except Exception:
        return ImageFont.load_default()


def layout(text, cjk, lat):
    return [(sub, cjk if is_cjk else lat) for sub, is_cjk in split_runs(text)]


def line_width(d, runs):
    return sum(d.textlength(s, font=f) for s, f in runs)


def bbox_of(d, runs, y):
    """这一行字形在本行内的包围盒（多段取并集；PIL 的 textbbox 带字体上下留白，用 anchor 拿真实字形框）。"""
    lo, hi = None, None
    for s, f in runs:
        bb = d.textbbox((0, 0), s, font=f)
        _, top, _, bot = bb
        lo = top if lo is None else min(lo, top)
        hi = bot if hi is None else max(hi, bot)
    if lo is None:
        return (0, y, 0, y)
    return (0, y + lo, 0, y + hi)


def draw_centered(d, y, runs):
    x = (W - line_width(d, runs)) / 2.0                  # 一手：两行**各自**居中
    for s, f in runs:
        d.text((x, y), s, font=f, fill=DEATH_TEXT + (255,),
               stroke_width=OUTLINE_WIDTH, stroke_fill=(0, 0, 0, 0))
        x += d.textlength(s, font=f)


def main():
    global W, H
    f1c, f1l = load(CJK_FONT, FONT1_PX), load(LAT_FONT, FONT1_PX)
    f2c, f2l = load(CJK_FONT, FONT2_PX), load(LAT_FONT, FONT2_PX)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r1, r2 = layout(LINE1, f1c, f1l), layout(LINE2, f2c, f2l)
    need = int(max(line_width(d, r1), line_width(d, r2))) + 8
    if need > W:                                         # 放不下就把画布放宽再重画
        W = need
        img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
    draw_centered(d, 0, r1)
    b1 = bbox_of(d, r1, 0)
    y2 = b1[3] + GLYPH_GAP                               # ★ 见 GLYPH_GAP 的注：按字形下沿排，不按盒顶
    draw_centered(d, y2, r2)
    b2 = bbox_of(d, r2, y2)
    top = min(b1[1], b2[1]) - 2
    bot = max(b1[3], b2[3]) + 2
    img = img.crop((0, max(0, top), W, bot))             # 裁掉上下留白（贴图按 sprite 居中放，留白会把它顶偏）
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT)
    print(f"DEATH_MSG: {OUT} {img.width}x{img.height} · 色 RGB{DEATH_TEXT} · 字形间距 {GLYPH_GAP} px（一手盒顶差 {LINE_GAP}）· 描边 {OUTLINE_WIDTH}")


if __name__ == "__main__":
    main()
