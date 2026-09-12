#!/usr/bin/env python3
"""永夜抄（系统卡 108）**早苗侧的妖机体 = 射命丸文**：TH16 零售切图 + 脚本生成。

    python3 make_sanae_youkai_art.py        # 生成 + 自检（幂等，重跑结果一致）

来源（零售字节只在 `local/`，gitignored；构建时取，不入库）：
  贴图+脚本  `local/th16.v1.00a/dat/pl03.anm`
              —— **anm version 8**（文件头自述，本脚本每次运行都回读打印）。
              贴图在 .anm 内部，entry 名 = `player/pl03/pl03.png`（256×256，本体贴图，完整自机图）。
  复核命令   `local/vendor/thtk/build/thanm/thanm -l 16 -m local/vendor/thpages/static/mapfile/v8.anmm \
                 local/th16.v1.00a/dat/pl03.anm`
             `local/vendor/thtk/build/thanm/thanm -x 16 local/th16.v1.00a/dat/pl03.anm`

── ★ 一手结论 ①：anm 版本必须对（2026-09-12，本脚本每次运行都打印这两条）──────────────────────
`thanm -l` 打印的 `version:` 是**文件头自己的字段**，不是命令行给的作品号 —— 实测：
    `-l 8 -m v2.anmm  th16/pl03.anm   → version: 8`
    `-l 8 -m v2.anmm  th08/player00.anm → version: 3`
    `-l 18 -m v8.anmm th18/pl00.anm    → version: 8`
⇒ **TH16 的 anm 是 version 8，配 `v8.anmm`**（TH18 也是 8 ⇒ 同一个 mapfile 同时管 `-l 16` 与 `-c 18`）。
**错版本的症状**（实测，反证）：拿 `v8.anmm` 去读 TH08 的 v3 文件，`sprite(sprite11)` 被译成
`stop(sprite11)`（整文件 `sprite(` 0 行、`stop(` 91 行）；反过来拿 `v0/v2/v4.anmm` 读 TH16 的 v8 文件，
指令全部退化成占位名 `ins_300(sprite0)`（那几张表里没有 TH16 那批 opcode 的名字）。
本文件在 v8 下读出的是真助记符（`type(0)` / `layer(14)` / `resampleMode(1)` / `sprite(spriteN)`），
`stop()` 只出现在脚本收尾处（23 支脚本共 28 处）⇒ 版本判定成立。

── ★ 一手结论 ②：`pl03` 就是**射命丸文**（2026-09-12，色相/主色判据，脚本每次运行都重算）──────
判据 = **主色占比 + 色相**，对象是**脸部图** `face/pl0X/face_pl0Xno.png`（发色/眼色在那张里最干净）
与**本体 24 格** `player/pl0X/pl0X.png` 的 sprite0..23（本脚本从 `pl0X.anm` 的脚本 0..4 现推，不写死）：

    pl00  脸: 黑 61% / 红 15% / 白 16%         本体: 红 35% / 白 33% / 黑 15%   → 霊夢（黑发，红白巫女服）
    pl01  脸: 黑 39% / 金 26% / 白 23%         本体: 黑 49% / 白 22% / 蓝反光 12% → 魔理沙（金发 + 黑帽黑白）
    pl02  脸: **蓝 62%** / 肤 21% / 黑 8%      本体: **蓝 54%** / 白 31% / 紫 12% → チルノ（蓝发蓝裙白围裙）
    pl03  脸: **黑 64%** / 白 21% / **红 8%**  本体: 白 44% / 黑 29% / 红 10%   → 射命丸文（黑发 + 红发带 + 白衬黑裙）

  ★ **四张脸一张都没有绿发**（绿占比 0.0%）—— 而同一次运行拿来当**阳性对照**的 TH18 `pl03.anm`
    （= 早苗，一手见 `AUDIT.md:3057`「`pl02`（咲夜）`pl03`（早苗）」）**脸是 48% 绿**。
    ⇒ 判据本身能测出绿发；⇒ **TH16 里没有早苗**，`pl02` 是チルノ不是早苗
      （⚠ 这是个坑：按「第三个自机」直觉会以为是早苗 —— 不是）。
    ⇒ 规格 §5 的「早苗 → 射命丸文」是**跨作品置换**，不是「同一槽位换皮」。
  ★ 与仓内既有结论同向（独立来源）：[`engine/sht/th16/03`](../../../../../engine/sht/th16/03-th16-funcstar-jumptables.md) §4
    「**正确**：pl00=灵梦、pl01=魔理沙、pl02=琪露诺、pl03=射命丸文」，以及
    [`engine/player/th16/02`](../../../../../engine/player/th16/02-season-release-and-bombs.md) §1 的炸弹 switch
    `CHARACTER = {0:Reimu, 1:Cirno, 2:Aya, 3:Marisa}`（那是**菜单序**，与文件号不同）。
    ⚠ 规格 §5 原先只写「`pl03.anm` 含完整自机贴图」而**没写它的身份判据** —— 本次补上（见设计文档 §11.1）。

── ★ 一手结论 ③：本体 = `script0..4`，24 格 30×46（2026-09-12）──────────────────────────────
`pl03.anm` 共 23 支脚本。**只有 script0..4 画本体**：它们引用的 sprite 恰好是
`player/pl03/pl03.png` 表里 24 个 **30×46** 的格子（`0..23`，3 行 × 8 列，行距 48、列距 32），
且都 `layer(14); type(0); resampleMode(1);`。其余 18 支不画本体：
  script5（`sprite24..27` 32×16）、script6..10（`sprite28..31` 64×64，`rotateAuto/scaleTime`）、
  script11/12（`sprite32..39` 16×16）、**script13 = 残像**（`sprite(sprite0)` + `color` + `alphaTime(30,0,0)` + `delete`，
  由 script14 `scriptNew(script15..18)` 起）、script15..18、script19..22（HUD，`layer(24..27)`）。
⇒ 本生成器**只切本体那 24 格**（`script0..4` 的 sprite 引用并集，运行期现算）；
  script13 那种「引用本体 sprite 的特效」**不切**，但它在路由上有后果（见设计文档 §11.3 的 (a) 条）。

产出（每条都不动别人的文件）：
  ability/sanae_youkai/YOUKAI_AYA_S<n>.png —— 24 张零售切图（30×46），**居中补透明**到 **32×64**。
    ★ 为什么居中：① 零售的 sprite 四边形以 **VM 位置为中心**画（`anchor` 位为 0 时），
      所以「内容居中于画布」与零售落点逐像素一致；② 本仓构建期对小于声明尺寸的源图**也是居中补**
      （`assets/build_ability.py:119-120`，「小图居中补到声明的 2 的幂尺寸」）⇒ 两边同一条公式，
      构建期不会再补一次。补透明只是给 30×46 套一层 2 的幂画布（1 列/9 行余量），
      **可见像素一个不动**（自检里逐像素比对源矩形）。
  ability/scripts/218..222_youkai_aya_s<0..4>.anm.txt
    —— 正文**逐条照抄** TH16 `pl03.anm` 的 script0..4，**只**把 `sprite(spriteN)` 换成 `sprite(@名字@)`；
      **连零售自己的 `layer(14)` 一起抄**（TH16 本体脚本确实带 `layer(14)`，不是我们编的；
      TH18 的世界层是 3–19、别用 HUD 的 24/25 ⇒ 14 合法）。注释写在 `script scriptNNN {` **里面**。
  ability/entries/ORDER.sanae_youkai.txt —— 待合并进 ORDER.txt 的片段（整段重写，幂等）。
    ⚠ **不动 `entries/ORDER.txt`**（多人并发，由主 agent 统一合并）。
  stdout —— ORDER 片段 + 自检（尺寸/非空/逐像素/占位符/逐行 diff/能否编过/身份互证）+ md5。
"""
import difflib
import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent          # assets/ability
REPO = HERE.parents[4]                          # assets/ability → card-expand → th18.v1.00a → mods → renkolab
OUT_ART = HERE / "sanae_youkai"
OUT_SCRIPTS = HERE / "scripts"
OUT_ORDER = HERE / "entries" / "ORDER.sanae_youkai.txt"

THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
ANMMAP_V8 = REPO / "local/vendor/thpages/static/mapfile/v8.anmm"   # TH16 anm = version 8（TH18 也是 8）
TH16_DAT = REPO / "local/th16.v1.00a/dat"
TH18_DAT = REPO / "local/th18.v1.00a/dat"
TH16_VERSION = "16"                             # thanm 的 VERSION 参数 = 作品号
TH18_VERSION = "18"

ANM_STEM = "pl03"                               # 早苗 → 射命丸文 的来源文件（身份判据见文件头 ②）
CHAR, CN = "AYA", "射命丸文"
BODY_SCRIPTS = (0, 1, 2, 3, 4)                  # ★ 零售的本体脚本（依据见文件头 ③）
SCRIPT0 = 218                                   # 脚本文件编号从这里起连续（218..222）
PL_FILES = ("pl00", "pl01", "pl02", "pl03")     # 身份互证要扫的四张自机图

_SPRITE_RE = re.compile(r"sprite(\d+):\s*\{\s*x:\s*(\d+),\s*y:\s*(\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)\s*\}")
_TOKEN_RE = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)")
_FILE_RE = re.compile(r"^(\d+)_([A-Za-z_][A-Za-z0-9_]*)\.anm\.txt$")


# ── 取源（thanm 现场解，仓里不落零售字节）──────────────────────────────

def _require(p, what):
    if not p.is_file():
        sys.exit(f"缺 {what}：{p}\n（零售 `.dat` 用 thdat 解到 `local/`；"
                 f"缺 thanm 就先 `bash tooling/thtk/build.sh`）")


def run_thanm(args, cwd):
    r = subprocess.run([str(THANM), *args], cwd=str(cwd), capture_output=True, text=True)
    # ⚠ v2.anmm 有一条 `74: 'acos' is a keyword, ignoring` 的无害警告；只有 `… ignoring` 放行。
    noise = [l for l in r.stderr.splitlines() if l.strip() and "ignoring" not in l]
    if r.returncode != 0 or noise:
        sys.exit(f"thanm {' '.join(map(str, args))} 失败：\n{r.stderr or r.stdout}")
    return r.stdout


def read_anm(anm: Path, workdir: Path, game_version: str):
    """→ (spec 文本, 解出来的贴图根目录)。`-l` 拿 entry/sprite/脚本；`-x` 把 entry 里的贴图解到 workdir。"""
    text = run_thanm(["-l", game_version, "-m", str(ANMMAP_V8), str(anm)], workdir)
    run_thanm(["-x", game_version, str(anm)], workdir)
    return text


def anm_version(text):
    m = re.search(r"^\s*version:\s*(\d+),", text, re.M)
    if not m:
        sys.exit("spec 里读不到 version 字段")
    return int(m.group(1))


def entries(text):
    """→ {entry 名: (rects, (W, H))}。矩形**只从 spec 的 sprites 表读**，不写死。"""
    out = {}
    for m in re.finditer(r'name: "([^"]+)",\n\s*format: \d+,\n\s*width: (\d+),\n\s*height: (\d+),', text):
        name, w, h = m.group(1), int(m.group(2)), int(m.group(3))
        j = text.find("sprites:", m.end())
        if j < 0:
            continue
        start = text.index("{", j)
        depth = 0
        for k in range(start, len(text)):
            if text[k] == "{":
                depth += 1
            elif text[k] == "}":
                depth -= 1
                if depth == 0:
                    end = k
                    break
        rects = {int(mm.group(1)): tuple(int(x) for x in mm.groups()[1:])
                 for mm in _SPRITE_RE.finditer(text[start:end])}
        out[name] = (rects, (w, h))
    return out


def extract_script(text, n):
    m = re.search(rf"^script script{n} \{{.*?^\}}$", text, re.S | re.M)
    if not m:
        sys.exit(f"spec 里找不到 script{n}")
    return m.group(0)


def script_sprites(text, nums):
    """→ {脚本号: [sprite 号…]}（保持出现顺序、去重）。"""
    out = {}
    for n in nums:
        seen = []
        for s in (int(x) for x in re.findall(r"sprite\(sprite(\d+)\)", extract_script(text, n))):
            if s not in seen:
                seen.append(s)
        out[n] = seen
    return out


def body_sprites(text, scripts=BODY_SCRIPTS):
    """本体 sprite 集 = **本体脚本自己的引用并集**（不写死 24 个号）。"""
    ref = script_sprites(text, scripts)
    return sorted({s for n in scripts for s in ref[n]}), ref


# ── 切图 ─────────────────────────────────────────────────────────────

def pow2_ceil(v):
    p = 1
    while p < v:
        p <<= 1
    return p


def canvas_size(w, h):
    """→ (W, H)：都是 2 的幂、且 ≥ 内容（写进 ORDER 的尺寸必须是 2 的幂，构建硬门）。"""
    return pow2_ceil(w), pow2_ceil(h)


def crop_body(sheet_path, rects, sprite_ids, char):
    """把该角色的本体 sprite 逐个切出、**居中**补到 2 的幂画布。

    → {ORDER 名: (路径, 画布 WxH, sprite 号, 可见像素数, 源单元字节)}
    """
    im = Image.open(sheet_path).convert("RGBA")
    OUT_ART.mkdir(parents=True, exist_ok=True)
    made = {}
    for sid in sprite_ids:
        x, y, w, h = rects[sid]
        cw, ch = canvas_size(w, h)
        cell = im.crop((x, y, x + w, y + h))
        canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        canvas.paste(cell, ((cw - w) // 2, (ch - h) // 2))   # ★ 居中 = 零售的 VM 中心锚点（见文件头）
        name = f"YOUKAI_{char}_S{sid}"
        path = OUT_ART / f"{name}.png"
        canvas.save(path)
        vis = sum(1 for a in canvas.getchannel("A").get_flattened_data() if a > 32)
        made[name] = (path, (cw, ch), sid, vis, cell.tobytes())
    return made


# ── 颜色判据（身份互证；与文件头 ② 是同一套算法）───────────────────────

def color_bucket(r, g, b):
    mx, mn = max(r, g, b), min(r, g, b)
    v = mx / 255.0
    s = 0.0 if mx == 0 else (mx - mn) / mx
    if v < 0.30 or (v < 0.45 and s < 0.35):
        return "黑/暗"
    if s < 0.15:
        return "白/灰"
    d = mx - mn
    if mx == r:
        h = ((g - b) / d) % 6
    elif mx == g:
        h = (b - r) / d + 2
    else:
        h = (r - g) / d + 4
    h *= 60.0
    if h < 15 or h >= 345:
        return "红"
    if h < 45:
        return "橙/褐(肤/棕)"
    if h < 70:
        return "金(黄)"
    if h < 100:
        return "黄绿"
    if h < 165:
        return "★绿"
    if h < 200:
        return "青"
    if h < 250:
        return "蓝"
    if h < 290:
        return "紫"
    return "洋红/粉"


def census(im):
    """→ ({色桶: 像素数}, 总数)。跳过全透明与纯白底（脸部图自带白底）。"""
    from collections import Counter
    c, tot = Counter(), 0
    raw = im.convert("RGBA").tobytes()
    for i in range(0, len(raw), 4):
        r, g, b, a = raw[i], raw[i + 1], raw[i + 2], raw[i + 3]
        if a < 64 or (r > 240 and g > 240 and b > 240):
            continue
        c[color_bucket(r, g, b)] += 1
        tot += 1
    return c, tot


def body_census(png_path, rects, sids):
    im = Image.open(png_path).convert("RGBA")
    from collections import Counter
    c, tot = Counter(), 0
    for sid in sids:
        x, y, w, h = rects[sid]
        cc, tt = census(im.crop((x, y, x + w, y + h)))
        c.update(cc)
        tot += tt
    return c, tot


def pct(c, tot, key):
    return 100.0 * c.get(key, 0) / tot if tot else 0.0


IDENT = {"pl00": "霊夢", "pl01": "魔理沙", "pl02": "チルノ", "pl03": CN}


def face_note(faces):
    """→ ORDER 片段里那行「脸的主色 → 谁」（**从本次实算的统计里生成**，不写死数字）。"""
    parts = []
    for p in PL_FILES:
        c, t = faces[p]
        top3 = "、".join(f"{k} {v * 100.0 / t:.0f}%" for k, v in c.most_common(3))
        parts.append(f"{p} [{top3}] → {IDENT[p]}")
    return "脸（`face_pl0Xno.png`）主色实算： " + "；".join(parts)


# ── 脚本 ─────────────────────────────────────────────────────────────

def header_comment(anm_name, src_num, mapping):
    return (f"    // 由 assets/ability/make_sanae_youkai_art.py 从 TH16 v1.00a `{anm_name}` script{src_num} 生成，别手改。\n"
            f"    // 这是**妖侧本体**（早苗 → {CN}）的第 {src_num} 支：零售 script0..4 = 本体的 24 格 30×46，\n"
            f"    //   一手：只有它们引用 `player/{ANM_STEM}/{ANM_STEM}.png` 的 30×46 格子（生成器文件头 ③）。\n"
            f"    // 身份一手：主色/色相判据把 `{anm_name}` 定成 {CN}（黑发 + 红发带 + 白衬黑裙；四张里唯一无绿发、\n"
            f"    //   且蓝/金都不占主导）—— 生成器文件头 ②，每次运行重算。\n"
            f"    // 指令逐条照抄（jmp/标签/`layer(14)`/`type(0)`/`resampleMode(1)` 一字未动），只把 sprite 引用换名：\n"
            f"    //   " + "，".join(f"sprite{s} → @{mapping[s]}" for s in sorted(mapping)) + "\n"
            f"    // ⚠ `layer(14)` 是**零售自己写的**（TH16 本体脚本本来就有），不是我们编的：\n"
            f"    //   TH18 的世界层 = 3–19（别用 HUD 的 24/25，见 assets/ability/README.md 与 `HUD-LAYOUT.md` §4.1），\n"
            f"    //   14 落在世界层里 ⇒ 原样搬过来合法；若将来挂到别的层由挂它的那一方改，别在这里改。")


def gen_scripts(spec_text, originals, body, order_names):
    """→ ({文件名: (正文, 期望正文, 原文, 脚本号, 角色)}, {文件名: {sprite号 → ORDER 名}})。"""
    OUT_SCRIPTS.mkdir(parents=True, exist_ok=True)
    out, maps = {}, {}
    for src_num in BODY_SCRIPTS:
        original = originals[src_num]
        dst = SCRIPT0 + src_num
        mapping = {s: f"YOUKAI_{CHAR}_S{s}" for s in body[src_num]}
        for nm in mapping.values():
            assert nm in order_names, f"{nm} 不在 ORDER 片段里"
        head_old = f"script script{src_num} {{"
        head_new = f"script script{dst} {{\n{header_comment(ANM_STEM + '.anm', src_num, mapping)}"
        body_txt = original.replace(head_old, head_new, 1)
        for sid, nm in mapping.items():
            body_txt = body_txt.replace(f"sprite(sprite{sid})", f"sprite(@{nm})")
        want = original.replace(head_old, head_new, 1)        # 只做「脚本号 + 注释头」这一处结构改动
        left = re.findall(r"sprite\(sprite(-?\d+)\)", body_txt)   # 漏换的引用（零售出现新 sprite 号时会炸）
        if left:
            sys.exit(f"{ANM_STEM}.anm script{src_num} 里还有没替换的 sprite 引用 {left}")
        name = f"{dst}_youkai_{CHAR.lower()}_s{src_num}.anm.txt"
        if not _FILE_RE.match(name):
            sys.exit(f"文件名不合 build_ability.py 的 `_FILE`：{name}")
        (OUT_SCRIPTS / name).write_text(body_txt + "\n", encoding="utf-8")
        out[name] = (body_txt, want, original, dst, CN)
        maps[name] = mapping
        print(f"[script] {name}  ← TH16 {ANM_STEM}.anm script{src_num}"
              f"（{CN}：{', '.join(f'sprite{s}' for s in sorted(mapping))}）")
    return out, maps


def order_rows(made):
    """→ [(ORDER 名, 源图(相对 assets/), WxH, sprite 号)]，按 sprite 号排。"""
    return [(name, f"ability/sanae_youkai/{name}.png", f"{size[0]}x{size[1]}", sid)
            for name, (_p, size, sid, _vis, _b) in sorted(made.items(), key=lambda kv: kv[1][2])]


def write_order(rows, body_note):
    hdr = [
        "# 永夜抄（系统卡 108）**妖侧本体：早苗 → 射命丸文** —— TH16 v1.00a `pl03.anm`（anm version 8）",
        "#   的 script0..4（本体 24 格 30×46，entry `player/pl03/pl03.png`）切图 + 脚本 218..222。",
        "# 本片段由 assets/ability/make_sanae_youkai_art.py 生成（幂等，整段重写）；合并时把下面这些行追加进 ORDER.txt 即可。",
        "# 每行 `NAME  源图(相对 assets/)  WxH`；WxH 都是 2 的幂 ⇒ 整张贴图一个 sprite。",
        "# ★ 身份判据（一手，色相/主色，生成器每次运行重算）：",
        f"#   {body_note}",
        "#   ⇒ `pl03` = 射命丸文（黑发 + 红发带 + 白衬黑裙）；**TH16 里没有早苗**（四张脸一张绿发都没有，",
        "#     而同一次运行拿来对照的 TH18 `pl03` = 早苗 脸是 48% 绿）⇒「早苗 → 文」是跨作品置换。",
        "#   ⚠ 别按「第 3 个自机」直觉把 `pl02` 当早苗：`pl02` 是チルノ（蓝发蓝裙白围裙，蓝 62%/54%）。",
        "# ★ 为什么居中：零售 sprite 以 VM 位置为中心画（anchor 位 0），本仓构建期也是居中补",
        "#   （assets/build_ability.py:119-120）⇒ 30×46 居中放进 32×64，可见像素一个不动。",
        "# 脚本：assets/ability/scripts/218..222_youkai_aya_s0..4.anm.txt（正文照抄零售 script0..4，只换 sprite 引用）。",
        "# ⚠ 本片段**不动** entries/ORDER.txt（多人并发，主 agent 统一合并）。",
    ]
    lines = hdr + [f"{n}  {p}  {s}" for n, p, s, _i in rows]
    OUT_ORDER.parent.mkdir(parents=True, exist_ok=True)
    OUT_ORDER.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[order] {OUT_ORDER.relative_to(HERE.parent)}（{len(rows)} 行；**不动** entries/ORDER.txt）")
    return lines


# ── 自检 ─────────────────────────────────────────────────────────────

def check_pngs(made, rects):
    """① 每张 PNG：尺寸是 2 的幂；② 不全是透明；且可见区与该 sprite 的零售矩形逐像素一致。"""
    files = sorted(OUT_ART.glob("YOUKAI_AYA_*.png"))
    assert len(files) == len(made), f"① sanae_youkai/ 下有 {len(files)} 张 PNG，预期 {len(made)}"
    vis_all, sizes = [], set()
    for name, (path, size, sid, vis, cell_bytes) in made.items():
        im = Image.open(path).convert("RGBA")
        assert im.size == size, f"① {name}.png 是 {im.size}，标称 {size}"
        assert (size[0] & (size[0] - 1)) == 0 and (size[1] & (size[1] - 1)) == 0, \
            f"① {name}.png 尺寸 {size} 不是 2 的幂"
        x, y, w, h = rects[sid]
        bx, by = (size[0] - w) // 2, (size[1] - h) // 2
        assert im.crop((bx, by, bx + w, by + h)).tobytes() == cell_bytes, \
            f"② {name}.png 与 pl03.anm sprite{sid} {rects[sid]} 不逐像素一致"
        assert im.getchannel("A").getbbox() is not None, f"② {name}.png 整张透明"
        assert vis >= 200, f"② {name}.png 只有 {vis} 个可见像素（源单元 {w}×{h}）"
        vis_all.append((name, vis))
        sizes.add(size)
    print(f"① {len(files)} 张 PNG 尺寸全是 2 的幂（画布 {sorted(sizes)}），每张可见区居中、"
          f"与 `player/pl03/pl03.png` 的 30×46 零售矩形**逐像素一致** ✓")
    print(f"② {len(files)} 张全部非空（可见像素 {min(v for _n, v in vis_all)}–{max(v for _n, v in vis_all)}，"
          f"阈值 200）✓")


def check_identity(bodies, faces, th18_faces):
    """③ 身份互证（色相/主色）：四张 TH16 自机图各是谁 + TH18 绿发阳性对照。

    bodies/faces/th18_faces: {plXX: (Counter, tot)}
    判据（都在文件头 ② 写死并解释）：
      pl00 红是主体色（红 ≥ 12% 面积、黑 ≥ 50%）          → 霊夢（黑发红白）
      pl01 金/暖色是大块（金+橙 ≥ 20% 面积、黑 ≥ 25%）      → 魔理沙（金发 + 黑）
      pl02 蓝是大块（≥ 50% 面积）且无绿               → チルノ（蓝白）
      pl03 白 + 黑为主、红是小块（黑 ≥ 50%、白 ≥ 15%、4% ≤ 红 ≤ 12%）且无绿 → 射命丸文
      四张 TH16 脸：绿 < 3%；TH18 pl03（早苗）脸：绿 ≥ 20%（阳性对照，证明判据能测出绿发）
    """
    print("\n# —— ③ 身份互证（主色占比 + 色相；脸 = face_pl0Xno.png，本体 = 24 格 30×46）——")
    stat = {}
    for p in PL_FILES:
        bc, bt = bodies[p]
        fc, ft = faces[p]
        stat[p] = (bc, bt, fc, ft)
        print(f"  {p}  脸(n={ft:6d}px): " + ", ".join(f"{k}={v * 100.0 / ft:.1f}%" for k, v in fc.most_common(5)))
        print(f"        本体(n={bt:6d}px): " + ", ".join(f"{k}={v * 100.0 / bt:.1f}%" for k, v in bc.most_common(5)))

    bc, bt, fc, ft = stat["pl00"]
    assert pct(fc, ft, "红") >= 12 and pct(fc, ft, "黑/暗") >= 50, "pl00 不像霊夢（黑发红白）"
    bc, bt, fc, ft = stat["pl01"]
    assert pct(fc, ft, "金(黄)") + pct(fc, ft, "橙/褐(肤/棕)") >= 20 and pct(fc, ft, "黑/暗") >= 25, \
        "pl01 不像魔理沙（金发 + 黑）"
    bc, bt, fc, ft = stat["pl02"]
    assert pct(fc, ft, "蓝") >= 50 and pct(fc, ft, "★绿") < 3, "pl02 不像チルノ（蓝、无绿）"
    bc, bt, fc, ft = stat["pl03"]
    assert pct(fc, ft, "黑/暗") >= 50 and pct(fc, ft, "白/灰") >= 15 and 4 <= pct(fc, ft, "红") <= 12 \
        and pct(fc, ft, "★绿") < 3 and pct(fc, ft, "蓝") <= 5, "pl03 不像射命丸文（黑发 + 红发带 + 白，无绿无蓝主色）"

    greens = {p: pct(faces[p][0], faces[p][1], "★绿") for p in PL_FILES}
    print(f"  ★ 四张 TH16 脸的绿占比： " + ", ".join(f"{p}={greens[p]:.1f}%" for p in PL_FILES)
          + " ⇒ TH16 无绿发自机（= 没有早苗）")
    assert all(g < 3 for g in greens.values()), f"TH16 里出现了绿发：{greens} —— 身份判据要重来"
    g18 = pct(th18_faces["pl03"][0], th18_faces["pl03"][1], "★绿")
    print(f"  ★ 阳性对照：TH18 pl03（= 早苗，一手 AUDIT.md:3057）脸的绿占比 = {g18:.1f}%"
          f" ⇒ 判据能测出绿发 ⇒「TH16 没有早苗」成立 ✓")
    assert g18 >= 20, f"阳性对照失效：TH18 pl03 的绿只占 {g18:.1f}%"
    print(f"  ⇒ 被切的 `{ANM_STEM}.anm` 是 {CN}（白 + 黑为主、红为小块的**红发带**、无绿）；与 "
          f"`engine/sht/th16/03` §4「pl03=射命丸文」同向 ✓")


def check_scripts(scripts, maps, rows):
    """④ 每个 @名字 都在 ORDER 片段里有定义；⑤ 与零售原文逐行 diff 只差 sprite 引用。"""
    names = {n for n, _p, _s, _i in rows}
    changed_total = 0
    for fname, (body, want, _orig, _idx, _who) in sorted(scripts.items(), key=lambda kv: kv[1][3]):
        missing = set(_TOKEN_RE.findall(body)) - names
        assert not missing, f"④ {fname}: ORDER 片段里没有 {sorted(missing)}"
        gen, ref = body.splitlines(), want.splitlines()
        assert len(gen) == len(ref), f"⑤ {fname}: 行数 {len(gen)} vs 期望 {len(ref)}"
        for i, (g, e) in enumerate(zip(gen, ref)):
            if g == e:
                continue
            mg = re.fullmatch(r"\s*sprite\(@(YOUKAI_[A-Z_]+_S\d+)\);", g)
            me = re.fullmatch(r"\s*sprite\(sprite(\d+)\);", e)
            assert mg and me, f"⑤ {fname}:{i + 1} 改到了 sprite 引用以外：{e!r} → {g!r}"
            assert mg.group(1) == maps[fname][int(me.group(1))], \
                f"⑤ {fname}:{i + 1} {e.strip()} 应换成 sprite(@{maps[fname][int(me.group(1))]})，实际 {g.strip()}"
            changed_total += 1
    print(f"④ {len(scripts)} 条脚本里的 @名字全部在 ORDER 片段有定义（{len(names)} 个名字） ✓")
    print(f"⑤ 与 TH16 原文逐行 diff：差异**只有** {changed_total} 行 sprite 引用"
          f"（脚本号 + 注释头之外正文一字未动，含 `layer(14)`/`type(0)`/`resampleMode(1)`）✓")


def print_script_diffs(scripts):
    print("\n# —— ⑤ diff 证据：TH16 原文（-）vs 生成脚本（+）——")
    for fname in sorted(scripts, key=lambda n: scripts[n][3]):
        body, _want, original, idx, who = scripts[fname]
        ref, gen = original.splitlines(), body.splitlines()
        print(f"\n=== {fname}（script{idx}，{who}；正文 {len(gen)} 行；下面是与 TH16 原文的**全部**差异）")
        for line in difflib.unified_diff(ref, gen, "TH16-original", "generated", lineterm="", n=0):
            print(line)


# ── 能否编过（只在 /tmp 里单独编，不碰 native/build/、不跑 make）────────

_TEST_ENTRY = """entry entry0 {
    version: 8,
    name: "ability/TEST.png",
    format: 1,
    width: 64,
    height: 64,
    memoryPriority: 0,
    lowResScale: 0,
    hasData: 1,
    THTXSize: 16384,
    THTXFormat: 1,
    THTXWidth: 64,
    THTXHeight: 64,
    THTXZero: 0,
    sprites: {
        sprite0: { x: 0, y: 0, w: 64, h: 64 }
    }
}
"""


def check_compile(scripts):
    """⑥ 每条脚本：把 `@NAME` 换成 `sprite0`、脚本号改成 0，在 /tmp 里用 thanm -c 18 单独编一次。"""
    print("\n# —— ⑥ 能否编过（thanm -c 18，/tmp 里单独编；不碰 native/build/、不跑 make）——")
    with tempfile.TemporaryDirectory(prefix="sanae-anm-") as td:
        d = Path(td)
        (d / "ability").mkdir()
        Image.new("RGBA", (64, 64), (255, 0, 255, 255)).save(d / "ability" / "TEST.png")
        ok = 0
        for fname in sorted(scripts, key=lambda n: scripts[n][3]):
            body, _want, _orig, idx, _who = scripts[fname]
            test = _TOKEN_RE.sub("sprite0", body).replace(f"script script{idx} {{", "script script0 {", 1)
            (d / "spec.txt").write_text(_TEST_ENTRY + "\n" + test + "\n", encoding="utf-8")
            out = d / "out.anm"
            if out.exists():
                out.unlink()
            run_thanm(["-c", TH18_VERSION, str(out), "spec.txt", "-m", str(ANMMAP_V8)], d)
            assert out.is_file() and out.stat().st_size > 0, f"⑥ {fname} 编出来是空的"
            ok += 1
            print(f"  {fname} → spec script0（@名字临时代 sprite0）：thanm -c 18 OK（{out.stat().st_size} B）")
        print(f"⑥ {ok}/{len(scripts)} 条脚本都能编过（临时目录已删，native/build 一行没动） ✓")


def print_md5(paths):
    h = hashlib.md5()
    for p in sorted(paths):
        h.update(p.name.encode())
        h.update(p.read_bytes())
    print(f"\n[md5] 本次全部产物（{len(paths)} 个文件的「名字+内容」汇总）= {h.hexdigest()}"
          f"   ← 连跑两次应完全一致（幂等判据）")


# ── main ─────────────────────────────────────────────────────────────

def main():
    _require(THANM, "thanm 可执行文件")
    _require(ANMMAP_V8, "anmmap v8.anmm（TH16 与 TH18 都是 anm version 8）")
    for p in PL_FILES:
        _require(TH16_DAT / f"{p}.anm", f"TH16 零售 {p}.anm")
    _require(TH16_DAT / f"{ANM_STEM}.anm", f"TH16 零售 {ANM_STEM}.anm（妖侧本体来源）")
    for p in PL_FILES:
        _require(TH18_DAT / f"{p}.anm", f"TH18 零售 {p}.anm（身份互证的阳性对照用）")

    with tempfile.TemporaryDirectory(prefix="sanae-src-") as td:
        tmp = Path(td)
        (tmp / "th16").mkdir()
        (tmp / "th18").mkdir()

        # —— 先解 TH16 四张（身份互证）与 TH18 四张（阳性对照），再切 pl03 ——
        bodies, faces, th18_faces = {}, {}, {}
        spec16 = {}
        for p in PL_FILES:
            t = read_anm(TH16_DAT / f"{p}.anm", tmp / "th16", TH16_VERSION)
            spec16[p] = t
            ent16 = entries(t)
            body_entry = f"player/{p}/{p}.png"
            if body_entry not in ent16:
                sys.exit(f"{p}.anm 里没有 entry {body_entry}（候选：{sorted(ent16)}）")
            body_png = tmp / "th16" / "player" / p / f"{p}.png"
            face_png = tmp / "th16" / "face" / p / f"face_{p}no.png"
            if not body_png.is_file() or not face_png.is_file():
                sys.exit(f"{p}.anm: thanm -x 没解出 {body_png} / {face_png}（entry 名变了？）")
            sids, _ref = body_sprites(t)
            assert len(sids) == 24, f"{p}.anm: script{BODY_SCRIPTS} 的本体 sprite 数是 {len(sids)}，预期 24"
            bodies[p] = body_census(body_png, ent16[body_entry][0], sids)
            faces[p] = census(Image.open(face_png))
        for p in PL_FILES:
            t = read_anm(TH18_DAT / f"{p}.anm", tmp / "th18", TH18_VERSION)
            fp = tmp / "th18" / "face" / p / f"face_{p}no.png"
            if not fp.is_file():
                sys.exit(f"TH18 {p}.anm: thanm -x 没解出 {fp}")
            th18_faces[p] = census(Image.open(fp))
            print(f"[对照] TH18 {p}.anm 的 anm version = {anm_version(t)}"
                  f"（早苗 = pl03，绿发占比见下面 ③）")

        text = spec16[ANM_STEM]
        print(f"[src] TH16 {ANM_STEM}.anm：anm version = **{anm_version(text)}**（⇒ mapfile 必须是 v8.anmm）")
        ent = entries(text)
        entry_name = f"player/{ANM_STEM}/{ANM_STEM}.png"
        if entry_name not in ent:
            sys.exit(f"{ANM_STEM}.anm 里没有 entry {entry_name}（候选：{sorted(ent)}）")
        rects, (sw, sh) = ent[entry_name]
        body, ref = body_sprites(text)
        assert body == list(range(24)), f"{ANM_STEM}.anm: 本体 sprite 不是 0..23，而是 {body}"
        for sid in body:
            assert sid in rects, f"{ANM_STEM}.anm: script{BODY_SCRIPTS} 引用了表里没有的 sprite{sid}"
            x, y, w, h = rects[sid]
            assert (w, h) == (30, 46), f"{ANM_STEM}.anm: sprite{sid} 是 {w}×{h}，本体格预期 30×46"
        print(f"[src] {ANM_STEM}.anm（anm v{anm_version(text)}，贴图 {sw}×{sh}，entry {entry_name}，"
              f"{len(rects)} 个 sprite）")
        for n in BODY_SCRIPTS:
            print(f"        script{n} → sprite{ref[n]}（本体格）")
        others = sorted(set(int(x) for x in re.findall(r"^script script(\d+) \{", text, re.M)) - set(BODY_SCRIPTS))
        print(f"[src] 其余 {len(others)} 支脚本不画本体（{others}）；其中 script13 = 残像"
              f"（`sprite(sprite0)` + color + alphaTime）⇒ 它仍指向**原机体**贴图，路由上要交代（设计文档 §11.3）")

        png = tmp / "th16" / "player" / ANM_STEM / f"{ANM_STEM}.png"
        made = crop_body(png, rects, body, CHAR)
        print(f"[crop] {CN}：{len(made)} 张 → {OUT_ART.relative_to(HERE.parent)}"
              f"（画布 {sorted({v[1] for v in made.values()})}）")

        rows = order_rows(made)
        note = face_note(faces)
        order_lines = write_order(rows, note)
        scripts, maps = gen_scripts(text, {n: extract_script(text, n) for n in BODY_SCRIPTS}, ref,
                                    {r[0] for r in rows})

        print("\n# —— ORDER.sanae_youkai.txt 内容（合并进 ORDER.txt 时追加这些行）——")
        for line in order_lines:
            print(line)

        check_pngs(made, rects)
        check_identity(bodies, faces, th18_faces)
        check_scripts(scripts, maps, rows)
        print_script_diffs(scripts)
        check_compile(scripts)

        print("\n# —— ❓ 剩下的路由决策（本脚本**不**决定，见设计文档 §11.3）——")
        print("  贴图/脚本已就位，但「妖侧 sprite 怎么送到画面上」有三条候选：(a) 写 vm+0x24 指向")
        print("  注入 pl0X.anm 的新 sprite（与 AUDIT §AL33 的『pl0X.anm 不改』铁律直接冲突，且要动贴图页）、")
        print("  (b) 叠一个跟随自机的独立 VM（照 112 飞碟那套）并把本体 VM 藏起来、(c) 什么都不做。")
        print("  三条的代价/风险/**玩家侧视觉后果**都写在设计文档 §11.3，等用户拍板；本生成器只保证资产就位。")

        print_md5(sorted(OUT_ART.glob("YOUKAI_AYA_*.png"))
                  + sorted(p for p in OUT_SCRIPTS.glob("22*_youkai_aya_*.anm.txt"))
                  + [OUT_ORDER])
    print("\n全部自检通过（幂等：重跑覆盖同名文件，内容不变）。")


if __name__ == "__main__":
    main()
