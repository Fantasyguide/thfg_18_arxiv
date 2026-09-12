#!/usr/bin/env python3
"""星莲船（系统卡 112）的飞碟本体：TH12 零售切图 + 脚本生成。

来源（零售字节只在 `local/`，gitignored；构建时取，不入库）：
  贴图  `local/th12.v1.00b/anm/enemy/enemy/ufo_anim.png`
        —— 256×256，entry `enemy/ufo_anim.png`，16 个 64×64 = sprite205..220
  脚本  `local/th12.v1.00b/anm/enemy/enemy.anm.txt` 的 **script135..138**（TH12 v1.00b，约 3237–3371 行）
        等价副本 `local/_scratch/th12anm/enemy.anm.txt`（2026-09-12 核对：两份在 script135..138 上逐行一致）
        复核命令：`local/vendor/thtk/build/thanm/thanm -l 12 local/th12.v1.00b/dat/enemy.anm -m local/vendor/thpages/static/mapfile/v4.anmm`
        （★ anmmap 必须用 **v4.anmm**：v8/v0 会把 TH12 的指令翻译错，连 `sprite()` 都译成 `stop()`；
          v2 只是助记符更旧（layer→fje 之类）；v4 = 与 TH18 侧同一套叫法，故取 v4。）

本生成器 `parse_sprite_rects()` **从上面那个 entry 里读矩形**，再断言与这张表一致（两个独立来源互证）：
    sprite205..208 = y=0   红 4 帧（x=0/64/128/192）
    sprite209..212 = y=64  蓝 4 帧
    sprite213..216 = y=128 绿 4 帧
    sprite217..220 = y=192 第 4 组 4 帧

★ 色 ↔ 脚本号 ↔ sprite（一手：TH12 `enemy.anm` 里哪个脚本引用哪些 sprite，逐条 grep 可得）
    script135 = 红    ：sprite205 / 206 / 207 / 208
    script136 = 蓝    ：sprite209 / 210 / 211 / 212
    script137 = 绿    ：sprite213 / 214 / 215 / 216
    script138 = 彩虹  ：**sprite205 / 210 / 215 / 220** ← 各色取一帧（红第 0 帧 / 蓝第 1 帧 / 绿第 2 帧 / 第 4 组第 3 帧）
  ⇒ 138 的「彩虹」不是另有一张彩图，而是**同一支脚本里每隔 6 帧换一种机体颜色**。
  ⇒ 本生成器 2026-09-12 实测像素（alpha>32 的可见像素均值）：205..208 ≈ (204,105,105) 红、
     209..212 ≈ (112,130,197) 蓝、213..216 ≈ (115,195,120) 绿、217..220 ≈ (195,204,104) **黄**
     （色相直方图集中在 60°–80°）。即「第 4 组」本体是**黄**飞碟；`RAINBOW` 这个名字沿用卡侧语义
     （= script138 那支混色脚本），不是说第 4 组贴图本身是彩虹色。

产出：
  ability/ufo/UFO_{RED,BLUE,GREEN,RAINBOW}_0..3.png   —— 16 张 64×64（2 的幂）。
       彩虹 4 张 = 上面对应帧的切图**另存成独立文件**（内容与 RED_0 / BLUE_1 / GREEN_2 / 第4组第3帧 相同），
       这样 ORDER 里每帧一个名字、可对账（不靠同一张图在 ORDER 里被引用两次）。
  ability/scripts/164..167_ufo_{red,blue,green,rainbow}.anm.txt
       —— 正文逐条照抄 TH12 script135..138，**只**把 `sprite(spriteNNN)` 换成 `sprite(@NAME)`；
          `layer(7)` 保持 7（世界层；HUD 才用 24/25），alpha/alphaTime/rotateTime/isetDiv/jmpDec/jmp/stop/标签一个不改。
          注释写在 `script scriptNN {` **里面**（build_ability.py 检查正文第一行就是脚本头）。
  ability/entries/ORDER.ufo.txt                        —— 待合并进 ORDER.txt 的片段（本生成器整段重写，幂等）。
  stdout                                               —— ORDER 片段 + 自检四项 + 与 TH12 原文的逐行 diff。

        python3 make_ufo_art.py            # 生成 + 自检（可重复跑，结果一致）
"""
import difflib
import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]              # assets/ability → card-expand → th18.v1.00a → mods → renkolab
OUT_ART = HERE / "ufo"
OUT_SCRIPTS = HERE / "scripts"
OUT_ORDER = HERE / "entries" / "ORDER.ufo.txt"

# 源（新布局 `anm/<anm名>/<entry 内部路径>` 优先；兼容旧扁平布局与 _scratch 副本）
SHEET_CANDIDATES = [
    REPO / "local/th12.v1.00b/anm/enemy/enemy/ufo_anim.png",
    REPO / "local/th12.v1.00b/anm/enemy/ufo_anim.png",
    REPO / "local/_scratch/th12anm/enemy/ufo_anim.png",
]
TXT_CANDIDATES = [
    REPO / "local/th12.v1.00b/anm/enemy/enemy.anm.txt",
    REPO / "local/_scratch/th12anm/enemy.anm.txt",
]
ENTRY_NAME = "enemy/ufo_anim.png"

# sprite → (x, y)：脚本/sprite 表的形状（尺寸恒 64×64）。★ 与 entry 里读出来的矩形互证。
EXPECTED_CELLS = {205 + i: ((i % 4) * 64, (i // 4) * 64) for i in range(16)}

# (ORDER 色名, 脚本名后缀, TH12 源脚本号, 输出脚本号, [sprite 号 ×4])
# 彩虹第 4 帧 = sprite220 = 第 4 组（黄）第 3 帧。
COLORS = [
    ("RED",     "red",     135, 164, [205, 206, 207, 208]),
    ("BLUE",    "blue",    136, 165, [209, 210, 211, 212]),
    ("GREEN",   "green",   137, 166, [213, 214, 215, 216]),
    ("RAINBOW", "rainbow", 138, 167, [205, 210, 215, 220]),
]
SPRITE_RE = re.compile(r"sprite(\d+):\s*\{\s*x:\s*(\d+),\s*y:\s*(\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)\s*\}")
TOKEN_RE = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)")


def _first(cands, what):
    for p in cands:
        if p.is_file():
            return p
    sys.exit("缺 %s：\n%s\n（先 `bash tooling/thtk/build.sh`，再用 thanm 解包，见本文件头）"
             % (what, "\n".join(f"  {p}" for p in cands)))


def parse_sprite_rects(text):
    """→ (sprite → (x, y, w, h), entry 声明的 (W, H))：矩形只从 entry 的 sprites{} 表读，不写死。"""
    i = text.find(f'name: "{ENTRY_NAME}"')
    if i < 0:
        sys.exit(f"源 anm 文本里找不到 entry {ENTRY_NAME}")
    j = text.find("sprites:", i)
    if j < 0:
        sys.exit(f"entry {ENTRY_NAME} 里没有 sprites 表")
    depth, start, end = 0, text.index("{", j), None
    for k in range(start, len(text)):                       # 括号配对，取出 sprites{...} 全体
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                end = k
                break
    rects = {int(m.group(1)): (int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)))
             for m in SPRITE_RE.finditer(text[start:end])}
    if len(rects) != 16:
        sys.exit(f"entry {ENTRY_NAME} 的 sprites 表读到 {len(rects)} 个矩形，预期 16")
    wm = re.search(r'name: "%s",\n\s*format: \d+,\n\s*width: (\d+),\n\s*height: (\d+),'
                   % re.escape(ENTRY_NAME), text)
    if not wm:
        sys.exit(f"entry {ENTRY_NAME} 的 width/height 字段没解析出来")
    return rects, (int(wm.group(1)), int(wm.group(2)))


def crop_cells(sheet_path, rects, sheet_size):
    """切 16 张 64×64。彩虹 4 张按 script138 的来路另存成独立文件。"""
    im = Image.open(sheet_path).convert("RGBA")
    if im.size != sheet_size:
        sys.exit(f"{sheet_path} 是 {im.size}，entry 声明 {sheet_size}")
    OUT_ART.mkdir(parents=True, exist_ok=True)
    made = {}
    for color, _suffix, _src, _dst, sprites in COLORS:
        for idx, sid in enumerate(sprites):
            x, y, w, h = rects[sid]
            if (w, h) != (64, 64):
                sys.exit(f"sprite{sid} 是 {w}×{h}，预期 64×64（ORDER 里按 64x64 声明）")
            name = f"UFO_{color}_{idx}"
            cell = im.crop((x, y, x + w, y + h))
            cell.save(OUT_ART / f"{name}.png")
            made[name] = (sid, cell.tobytes())
    print(f"[crop] {len(made)} 张 64×64 → {OUT_ART.relative_to(HERE.parent)}"
          f"（源 {sheet_path.relative_to(REPO)}；矩形读自 entry {ENTRY_NAME} 的 sprites 表）")
    return made


def extract_script(text, n):
    m = re.search(rf"^script script{n} \{{.*?^\}}$", text, re.S | re.M)
    if not m:
        sys.exit(f"TH12 enemy.anm.txt 里找不到 script{n}")
    return m.group(0)


def header_comment(src_num, color, sprites):
    """脚本头后面那段注释（★ 必须写在 `script scriptNN {` 里面）。"""
    return (f"    // 由 assets/ability/make_ufo_art.py 从 TH12 v1.00b `enemy.anm` script{src_num} 生成，别手改。\n"
            f"    // 指令逐条照抄（layer(7) 是世界层，保持 7），只把 sprite 引用换成 ORDER 名字：\n"
            f"    //   " + "，".join(f"sprite{s} → @UFO_{color}_{i}" for i, s in enumerate(sprites)))


def gen_scripts(anm_text):
    """→ ({文件名: (正文, 期望正文, 原文)}, {文件名: {sprite号 → ORDER 名}})。

    「期望正文」= 原文只做「脚本号 + 注释头」这一处结构改动后的文本 —— 自检拿它逐行对，
    只要正文还动过别的字符就会在对账时报出来（原样留着，供打印真·diff 用）。
    """
    OUT_SCRIPTS.mkdir(parents=True, exist_ok=True)
    out, maps = {}, {}
    for color, suffix, src_num, dst_num, sprites in COLORS:
        original = extract_script(anm_text, src_num)
        mapping = {sid: f"UFO_{color}_{i}" for i, sid in enumerate(sprites)}
        head_old = f"script script{src_num} {{"
        head_new = f"script script{dst_num} {{\n{header_comment(src_num, color, sprites)}"
        body = original.replace(head_old, head_new, 1)
        for sid, nm in mapping.items():
            body = body.replace(f"sprite(sprite{sid})", f"sprite(@{nm})")
        want = original.replace(head_old, head_new, 1)
        name = f"{dst_num}_ufo_{suffix}.anm.txt"
        left = re.findall(r"sprite\(sprite(-?\d+)\)", body)     # 漏换的 sprite 引用（原文里出现新 sprite 号时会炸）
        assert not left, f"{name}: 还有没替换的 sprite 引用 {left}（COLORS 表要补号）"
        (OUT_SCRIPTS / name).write_text(body + "\n", encoding="utf-8")
        out[name] = (body, want, original)
        maps[name] = mapping
        print(f"[script] {name}  ← TH12 enemy.anm script{src_num}"
              f"（{color}：{', '.join(f'sprite{s}' for s in sprites)}）")
    return out, maps


def order_rows():
    return [(f"UFO_{color}_{i}", f"ability/ufo/UFO_{color}_{i}.png", "64x64")
            for color, _suffix, _s, _d, _sp in COLORS for i in range(4)]


def write_order():
    hdr = [
        "# 星莲船（系统卡 112）飞碟本体 —— TH12 v1.00b `enemy.anm` script135..138 + `enemy/ufo_anim.png` 切图。",
        "# 本片段由 assets/ability/make_ufo_art.py 生成（幂等，整段重写）；合并时把下面 16 行追加进 ORDER.txt 即可。",
        "# 每行 `NAME  源图(相对 assets/)  WxH`；64×64 是 2 的幂 ⇒ 整张贴图一个 sprite（不写尺寸会被当成 256×320 卡图）。",
        "# 色 ↔ TH12 脚本：红=script135 / 蓝=script136 / 绿=script137 / 彩虹=script138（= sprite205,210,215,220 各取一帧）。",
    ]
    lines = hdr + [f"{n}  {p}  {s}" for n, p, s in order_rows()]
    OUT_ORDER.parent.mkdir(parents=True, exist_ok=True)
    OUT_ORDER.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[order] {OUT_ORDER.relative_to(HERE.parent)}（{len(order_rows())} 行；**不动** entries/ORDER.txt）")
    return lines


# ── 自检 ──────────────────────────────────────────────────────────────

def check_pngs(made, sheet_path):
    """① 16 张都是 64×64；② 每张不空（可见像素够多）且与源图对应单元逐字节一致。"""
    files = sorted(OUT_ART.glob("UFO_*.png"))
    assert len(files) == 16, f"① ufo/ 下有 {len(files)} 张 PNG，预期 16"
    want_sid = {f"UFO_{color}_{i}": sid
                for color, _s, _n, _d, sprites in COLORS for i, sid in enumerate(sprites)}
    counts = []
    for f in files:
        im = Image.open(f).convert("RGBA")
        assert im.size == (64, 64), f"① {f.name} 是 {im.size}，预期 64×64"
        alpha = im.getchannel("A")
        assert alpha.getbbox() is not None, f"② {f.name} 整张透明"
        op = sum(1 for a in alpha.get_flattened_data() if a > 32)
        assert op >= 500, f"② {f.name} 只有 {op} 个可见像素（预期 ≥500）"
        assert im.tobytes() == made[f.stem][1], f"② {f.name} 与源图单元不一致"
        assert made[f.stem][0] == want_sid[f.stem], f"② {f.name} 来路 sprite 不对"
        counts.append(op)
    print("① 16 张 PNG 全部 64×64 / RGBA ✓")
    print(f"② 16 张全部非空（可见像素 {min(counts)}–{max(counts)}，阈值 500）"
          f"且与源图单元逐字节一致，来路 sprite 号正确 ✓（源 {sheet_path.relative_to(REPO)}）")


def check_scripts(scripts, maps):
    """③ 每个 @名字 都在 ORDER 片段里有定义；④ 与原文逐行 diff 只差 sprite 引用。"""
    names = {n for n, _p, _s in order_rows()}
    per_file = {}
    for fname, (body, want, _orig) in sorted(scripts.items()):
        used = set(TOKEN_RE.findall(body))
        missing = used - names
        assert not missing, f"③ {fname}: ORDER 片段里没有 {sorted(missing)}"
        gen, ref = body.splitlines(), want.splitlines()
        assert len(gen) == len(ref), f"④ {fname}: 行数 {len(gen)} vs 原文 {len(ref)}"
        changed = []
        for i, (g, e) in enumerate(zip(gen, ref)):
            if g == e:
                continue
            mg = re.fullmatch(r"\s*sprite\(@(UFO_[A-Z_0-9]+)\);", g)
            me = re.fullmatch(r"\s*sprite\(sprite(\d+)\);", e)
            assert mg and me, f"④ {fname}:{i + 1} 改到了 sprite 引用以外：{e!r} → {g!r}"
            want_name = maps[fname][int(me.group(1))]
            assert mg.group(1) == want_name, \
                f"④ {fname}:{i + 1} {e.strip()} 应换成 sprite(@{want_name})，实际 {g.strip()}"
            changed.append((i + 1, e.strip(), g.strip()))
        per_file[fname] = changed
    print(f"③ 4 条脚本里的 @名字全部在 ORDER 片段有定义（{len(names)} 个名字） ✓")
    print(f"④ 与 TH12 原文逐行 diff：差异**只有** sprite 引用（共 "
          f"{sum(len(v) for v in per_file.values())} 行），脚本号/注释头之外正文一字未动 ✓")
    return per_file


def print_script_diffs(scripts, maps):
    print("\n# —— ④ diff 证据：TH12 原文（-）vs 生成脚本（+）——")
    for fname in sorted(scripts):
        body, _want, original = scripts[fname]
        ref, gen = original.splitlines(), body.splitlines()
        print(f"\n=== {fname}（正文 {len(gen)} 行；下面是与 TH12 原文的**全部**差异）")
        for line in difflib.unified_diff(ref, gen, "TH12-original", "generated", lineterm="", n=0):
            print(line)


def main():
    sheet_path = _first(SHEET_CANDIDATES, "TH12 飞碟贴图 ufo_anim.png")
    txt_path = _first(TXT_CANDIDATES, "TH12 enemy.anm 文本")
    print(f"源：{sheet_path.relative_to(REPO)}\n    {txt_path.relative_to(REPO)}\n")
    anm_text = txt_path.read_text(encoding="utf-8")

    rects, sheet_size = parse_sprite_rects(anm_text)
    for sid, (x, y) in EXPECTED_CELLS.items():
        assert rects[sid] == (x, y, 64, 64), f"sprite{sid} 矩形 {rects[sid]}，预期 {(x, y, 64, 64)}"
    print(f"[rect] entry {ENTRY_NAME} 的 16 个矩形与预期表逐项一致（{sheet_size[0]}×{sheet_size[1]}，16 个 64×64） ✓")

    made = crop_cells(sheet_path, rects, sheet_size)
    for i, src in enumerate(["UFO_RED_0", "UFO_BLUE_1", "UFO_GREEN_2"]):
        assert made[f"UFO_RAINBOW_{i}"][1] == made[src][1], f"UFO_RAINBOW_{i} 内容应等于 {src}"
    assert made["UFO_RAINBOW_3"][0] == 220, "彩虹第 4 帧应来自 sprite220"
    print("[rect] 彩虹 4 帧来路 = sprite205 / 210 / 215 / 220（红0 / 蓝1 / 绿2 / 第4组(黄)3） ✓")

    scripts, maps = gen_scripts(anm_text)
    rows = write_order()
    print("\n# —— ORDER.ufo.txt 内容（合并进 ORDER.txt 时追加这 16 行）——")
    for line in rows:
        print(line)

    check_pngs(made, sheet_path)
    check_scripts(scripts, maps)
    print_script_diffs(scripts, maps)
    print("\n全部自检通过（幂等：重跑覆盖同名文件，内容不变）。")


if __name__ == "__main__":
    main()
