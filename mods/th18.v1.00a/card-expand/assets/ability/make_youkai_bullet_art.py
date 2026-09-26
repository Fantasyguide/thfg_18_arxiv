#!/usr/bin/env python3
"""卡 108（TH08 永夜抄）**妖侧弹的弹体贴图** —— 把原作那一支 anm 脚本与它的 sprite 搬进 `ability.anm`。

owner 口径（2026-09-26 实跑）：「**妖机体的弹幕样式应该都不对，发的弹幕本身得搬过来**」。
此前所有妖侧行都填 `anm = 21`（TH18 原生 `ability.anm` 的通用弹），所以样式全是本作的、不是永夜抄的。

## 一手（怎么知道该搬哪一支）

TH08 的 `plyNNas.sht` 每一行有 `+0x24 anm 脚本基号`，引擎**用它 +10** 装进弹的 VM：
一手 `th08:0x44fd32 movsx edx,WORD PTR [ecx+0x24]` → `0x44fd36 add edx,0xa` → `push edx` → `call 0x4069f0`
（`AnmLoaded::SetAndExecuteScriptIdx`）⇒ **弹体脚本 = 行里的 `+0x24` + 10**（`engine/sht/th08/01` §3）。

逐档读出来（本机 `local/th08.v1.00d/dat/plyNNas.sht` 直读）→ 弹体脚本：

| 角色 | 主炮行 `+0x24` → 脚本 | 子机行 `+0x24` → 脚本 | 脚本用到哪几张 sprite |
| --- | --- | --- | --- |
| 霊夢・紫（pl00）| 4 → **14** | 6 → **16** | sprite35（64×16）/ sprite36（64×16）|
| 魔理沙・アリス（pl01）| 12 → **22** | 14/15/16/17 → **24/25/26/27** | sprite30（64×16）/ sprite32（448×32，逐档只差 `scale.y` 0.5/0.6/0.7/0.8）|
| 咲夜・レミリア（pl02）| 2 → **12** | 4 → **14** | sprite22（48×16）/ sprite23（16×32）|

早苗那一侧是 TH16：`pl03sub.sht` 的行 `+0x22 anm = 0` ⇒ 弹体脚本 = **`pl03sub.anm` script0**
（一手：`engine/season/th16/01` §5.6「副季节弹的贴图 = `pl0Xsub.anm` 里 `sht+0x22` 号脚本画的 sprite」），
它用 sprite0..3（每张 64×64）。

## 产物（都在 `assets/ability/` 下；**只追加**，不动既有 entry/sprite 号）

- `yk_bullet/<名字>.png` —— 源图里那一张 sprite 的 1:1 切图，**居中补到 2 的幂画布**
  （与 `make_youkai_option_art.py` 同一条公式：TH18 的绘制以 sprite 矩形中心为锚点）。
- `scripts/3NN_yk_bullet_*.anm.txt` —— 逐条照抄原脚本，只做三件事：
  ① `sprite(spriteN)` → `sprite(@名字@)`；
  ② 补 `type(1)`（**TH18 v8 的渲染模式**：不写就落进模式 0 = 绘制定格，见 `AUDIT.md` §BE #95）；
  ③ 省掉 v8 里**不存在**的助记符（`ins_83` / `ins_25` / `scrollX`），逐条在脚本注释里点名。
- `entries/ORDER.yk_bullet.txt` —— 待合并进 `ORDER.txt` 末尾的片段（整段重写、幂等）；
  本脚本**同时**把这段 append 进 `ORDER.txt`（它自己是共享件，按纪律登记 `AUDIT.md` §BE）。
- stdout —— ORDER 片段 + 自检（尺寸/非空/占位符/能否编过/重名/幂等 md5）。

用法：`python3 assets/ability/make_youkai_bullet_art.py`（在 `mods/th18.v1.00a/card-expand` 下任意路径）
"""
import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent          # assets/ability
REPO = HERE.parents[4]                          # → renkolab
OUT_ART = HERE / "yk_bullet"
OUT_SCRIPTS = HERE / "scripts"
OUT_ORDER = HERE / "entries" / "ORDER.yk_bullet.txt"
MAIN_ORDER = HERE / "entries" / "ORDER.txt"

THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
TH08_DAT = REPO / "local/th08.v1.00d/dat"
TH16_DAT = REPO / "local/th16.v1.00a/dat"
TH08_ANM = REPO / "local/th08.v1.00d/anm"
TH16_ANM = REPO / "local/th16.v1.00a/anm"

SCRIPT0 = 364          # 脚本文件编号从这里起连续（现有序号最大 363 = ufo_summon_spark）
ATLAS = "ATLAS_YK_BULLET"

# 成员：ORDER 名 → (来源, 作品, anm 名, sprite 号, 是否裁一刀, 裁剪框)
MEMBERS = [
    # 霊夢・紫（TH08 player00 script14/16，sprite35/36 = 64×16）
    ("YKB_R_MAIN", "th08", "player00", 35, None),
    ("YKB_R_OPT",  "th08", "player00", 36, None),
    # 魔理沙・アリス（TH08 player01 script22/24..27，sprite30 = 64×16；sprite32 = 448×32）
    ("YKB_M_MAIN", "th08", "player01", 30, None),
    # ★ sprite32 是**一条 448×32 的滚动纹理**（原脚本 `scrollX(0.0625)` 让它横向流动；v8 没有 scrollX）
    #   ⇒ 只切**前 256×32**（那一张里非空像素就在 0..255，见生成器自检），静止显示。
    ("YKB_M_OPT",  "th08", "player01", 32, (0, 0, 256, 32)),
    # 咲夜・レミリア（TH08 player02 script12/14，sprite22 = 48×16、sprite23 = 16×32）
    ("YKB_S_MAIN", "th08", "player02", 22, None),
    ("YKB_S_OPT",  "th08", "player02", 23, None),
    # 早苗（TH16 pl03sub script0，sprite0..3 = 64×64，四个成员一位一图）
    ("YKB_A_0", "th16", "pl03sub", 0, None),
    ("YKB_A_1", "th16", "pl03sub", 1, None),
    ("YKB_A_2", "th16", "pl03sub", 2, None),
    ("YKB_A_3", "th16", "pl03sub", 3, None),
]

# 脚本：ORDER 名 → (作品, anm 名, 源脚本号, [sprite 号…], 说明)
SCRIPTS = [
    ("YKB_R_MAIN", "th08", "player00", 14, [35],        "霊夢・紫 妖侧**主炮**弹（`ply00as` 主炮行 `+0x24`=4 ⇒ +10）"),
    ("YKB_R_OPT",  "th08", "player00", 16, [36],        "霊夢・紫 妖侧**子机**弹（`ply00as` 子机行 `+0x24`=6 ⇒ +10）"),
    ("YKB_M_MAIN", "th08", "player01", 22, [30],        "魔理沙・アリス 妖侧主炮弹（`+0x24`=12 ⇒ +10）"),
    ("YKB_M_OPT",  "th08", "player01", 24, [32],        "魔理沙・アリス 妖侧子机弹（`+0x24`=14 ⇒ +10；逐档 25/26/27 只差 `scale.y`）"),
    ("YKB_S_MAIN", "th08", "player02", 12, [22],        "咲夜・レミリア 妖侧主炮弹（`+0x24`=2 ⇒ +10）"),
    ("YKB_S_OPT",  "th08", "player02", 14, [23],        "咲夜・レミリア 妖侧子机弹（`+0x24`=4 ⇒ +10）"),
    ("YKB_A_0",    "th16", "pl03sub",   0, [0, 1, 2, 3], "早苗（TH16 秋/文）妖侧弹（`pl03sub` script0，四帧循环）"),
]

_SPRITE_DEF_RE = re.compile(r"sprite(\d+):\s*\{\s*x:\s*(\d+),\s*y:\s*(\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)\s*\}")
_SPRITE_CALL_RE = re.compile(r"sprite\(sprite(\d+)\)")
_NAME_RE = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)@")
# v8 里没有的助记符（逐条在产物脚本里点名；不要静默丢）
_DROP_RE = re.compile(r"^\s*(ins_83\(\d+\);|ins_25\(\d+\);|scrollX\([^;]*\);)\s*$")


def _require(p, what):
    if not p.is_file():
        raise SystemExit(f"缺 {what}：{p}（样本由用户放进 `local/`，别去下载；缺 thanm 先 bash tooling/thtk/build.sh）")


ANMMAP_V2 = REPO / "local/vendor/thpages/static/mapfile/v2.anmm"   # TH08（anm v3 那一代）
ANMMAP_V8 = REPO / "local/vendor/thpages/static/mapfile/v8.anmm"   # TH16 / TH18


def read_anm(kind: str, name: str, workdir: Path) -> str:
    """解出该 anm 的文本（thanm 现场解、仓里不落原生字节）。

    ★ **必须带 `-m <anmmap>`**：不带的话 thanm 打的是 `ins_300(...)` 这种**裸 opcode**，
    我们的 `sprite(spriteN)` 替换就一条都匹配不上（第一次跑就踩了：早苗那支整段没换名）。
    """
    if kind == "th08":
        _require(TH08_DAT / f"{name}.anm", "TH08 anm")
        ver, path, mp = "8", TH08_DAT / f"{name}.anm", ANMMAP_V2
    else:
        _require(TH16_DAT / f"{name}.anm", "TH16 anm")
        ver, path, mp = "16", TH16_DAT / f"{name}.anm", ANMMAP_V8
    _require(mp, "anmmap")
    r = subprocess.run([str(THANM), "-l", ver, "-m", str(mp), str(path)], capture_output=True, text=True)
    noise = [l for l in r.stderr.splitlines() if l.strip() and "ignoring" not in l]
    if r.returncode != 0 or noise or not r.stdout.strip():
        raise SystemExit(f"thanm 解 {path} 失败：\n{r.stderr[:400] or r.stdout[:400]}")
    return r.stdout


def sheet_path(kind: str, name: str) -> Path:
    """源图（TH08 = `anm/<anm名>/data/player00/<anm名>.png`；TH16 = `anm/pl03/player/pl03.png`）。"""
    if kind == "th08":
        return TH08_ANM / name / "data" / "player00" / f"{name}.png"
    # TH16：`pl03sub.anm` 的贴图在 `anm/pl03sub/player/pl03/pl03.png`（**与 pl03.anm 各一份**，
    # 别混用 —— 两边的 sprite 号空间不同）
    return TH16_ANM / name / "player" / "pl03" / "pl03.png"


def entry_rects(text: str, name: str) -> dict:
    """该 anm 里 **entry0** 的 sprite 矩形（子弹都在 entry0）。"""
    m = re.search(r"entry entry0 \{(.*?)^\}", text, re.S | re.M)
    if not m:
        raise SystemExit(f"{name}: 找不到 entry0")
    return {int(x.group(1)): (int(x.group(2)), int(x.group(3)), int(x.group(4)), int(x.group(5)))
            for x in _SPRITE_DEF_RE.finditer(m.group(1))}


def extract_script(text: str, n: int):
    m = re.search(rf"^script script{n} \{{.*?^\}}$", text, re.S | re.M)
    if not m:
        raise SystemExit(f"anm 里找不到 script{n}")
    return m.group(0)


def pow2_ceil(v):
    p = 1
    while p < v:
        p <<= 1
    return p


def canvas_size(w, h):
    return pow2_ceil(w), pow2_ceil(h)


def cut_sprites(workdir: Path):
    """把每个成员 1:1 切出来、居中补到 2 的幂画布。→ {(名字): (路径, WxH, 可见像素, 源字节)}"""
    OUT_ART.mkdir(parents=True, exist_ok=True)
    made = {}
    for name, kind, anm, sid, crop in MEMBERS:
        text = read_anm(kind, anm, workdir)
        rects = entry_rects(text, anm)
        if sid not in rects:
            raise SystemExit(f"{anm}: 没有 sprite{sid}（源 ank 变了？）")
        x, y, w, h = rects[sid]
        if crop:                                        # ★ 只对「滚动纹理」那一张裁（见 MEMBERS 的注）
            cx, cy, cw_, ch_ = crop
            if cx + cw_ > w or cy + ch_ > h:
                raise SystemExit(f"{anm} sprite{sid}: 裁剪框 {crop} 超出 {w}x{h}")
            x, y, w, h = x + cx, y + cy, cw_, ch_
        im = Image.open(sheet_path(kind, anm)).convert("RGBA")
        cell = im.crop((x, y, x + w, y + h))
        cw, ch = canvas_size(w, h)
        canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        canvas.paste(cell, ((cw - w) // 2, (ch - h) // 2))     # 居中 = 原生的中心锚点
        path = OUT_ART / f"{name}.png"
        canvas.save(path)
        vis = sum(1 for a in canvas.getchannel("A").get_flattened_data() if a > 32)
        made[name] = (path, (cw, ch), vis, cell.tobytes())
    return made


def gen_script(name: str, kind: str, anm: str, src_num: int, sids, note: str, sprite_map: dict) -> str:
    """逐条照抄 TH08/TH16 那一支；只做三件事（换 sprite 名 / 补 type(1) / 省掉 v8 没有的助记符）。"""
    workdir = Path(tempfile.mkdtemp(prefix="ykbullet."))
    src = extract_script(read_anm(kind, anm, workdir), src_num)
    out, dropped = [], []
    for line in src.splitlines():
        s = line.strip()
        if s.startswith("script script"):
            out.append(f"script script{SCRIPT0 + [x[0] for x in SCRIPTS].index(name)} {{")
            out += [
                f"    // 由 assets/ability/make_youkai_bullet_art.py 从 "
                f"{'TH08 v1.00d' if kind == 'th08' else 'TH16 v1.00a'} `{anm}` script{src_num} 生成，别手改。",
                f"    // {note}。",
                "    // 指令逐条照抄（段序/时序/数值一字未动），只做三件事：",
                "    //   ① `sprite(N)` → `sprite(@名字@)`；② 补 `type(1)`（TH18 v8 的**渲染模式**：不写就落进",
                "    //   模式 0 = 绘制定格 ⇒ `AUDIT.md` §BE #95）；③ 省掉 v8 里不存在的助记符（下面逐条点名）。",
                "    // ★ 为什么补 `layer(13)`：原脚本没有（TH08 那代由 VM 自带层），TH18 的原生弹脚本都显式写",
                "    //   （`ability.anm` script21 起手 `layer(13)`）⇒ 照原生那一支写。",
                "    layer(13);",
                "    type(1);",
            ]
            continue
        if s == "}":
            out.append(line)
            continue
        m = _DROP_RE.match(s)
        if m:
            indent = line[:len(line) - len(line.lstrip())]
            dropped.append(s)
            out.append(f"{indent}// ★ 原生原文此处为 `{s}` —— TH18 的 anm v8 没有这个助记符 ⇒ 省略")
            continue
        out.append(_SPRITE_CALL_RE.sub(lambda mm: f"sprite(@{sprite_map[int(mm.group(1))]}@)", line))
    for sid in sids:
        if not any(f"@{sprite_map[sid]}@" in l for l in out):
            raise SystemExit(f"{name}: 产物里没有引用 sprite{sid}（sprite 表或源脚本对不上）")
    return "\n".join(out) + "\n"


def pack_atlas(made):
    """把成员按「货架」摆进一张 2 的幂画布里 → {名字: (x, y, w, h)}, (W, H)。

    ⚠ 成员画布高度不一（TH08 的 16/32、TH16 的 64）⇒ **必须多行**，而且每行按高度分组
    （第一版写成单行 512x64，生成器自己在 `write_png_row` 里报错拦下了 —— 那道自检留着）。
    """
    rows, maxw = [], 512
    cur, curw, curh = [], 0, 0
    for name, _k, _a, _s, _c in MEMBERS:
        w, h = made[name][1]
        if cur and (curw + w > maxw or h > curh):
            rows.append((cur, curw, curh))
            cur, curw, curh = [], 0, 0
        cur.append(name)
        curw += w
        curh = max(curh, h)
    if cur:
        rows.append((cur, curw, curh))
    place, y = {}, 0
    for names, _w, h in rows:
        x = 0
        for n in names:
            w, hh = made[n][1]
            place[n] = (x, y + (h - hh) // 2, w, hh)      # 行内垂直居中
            x += w
        y += h
    W = pow2_ceil(maxw if maxw else 1)
    H = pow2_ceil(y)
    return place, (W, H)


def write_order(made):
    place, (W, H) = pack_atlas(made)
    lines = [
        "# 卡 108（TH08 永夜抄）**妖侧弹的弹体贴图** —— 原作的弹，逐张 1:1 切图。",
        "# 本片段由 assets/ability/make_youkai_bullet_art.py 生成（幂等，整段重写）；主 agent 已 append 进 ORDER.txt 末尾。",
        "# 一手：弹体脚本 = 行里 `+0x24` **+10**（`th08:0x44fd32`/`0x44fd36` ⇒ `AnmLoaded::SetAndExecuteScriptIdx`）；",
        "#   霊夢 s14/s16（sprite35/36）· 魔理沙 s22/s24（sprite30/sprite32）· 咲夜 s12/s14（sprite22/23）·",
        "#   早苗 = TH16 `pl03sub.anm` script0（sprite0..3）。",
        f"ATLAS {ATLAS} {W}x{H}",
    ]
    for name, _kind, _anm, _sid, _crop in MEMBERS:
        x, y, w, h = place[name]
        lines.append(f"{name:<17} ability/yk_bullet/{name}.png   @{x},{y},{w}x{h}")
    text = "\n".join(lines) + "\n"
    OUT_ORDER.write_text(text, encoding="utf-8")
    return text, (W, H)


def check_compile(scripts):
    """逐支用 thanm 编一遍（v8）——保证产物不是「写出来编不过」。"""
    with tempfile.TemporaryDirectory(prefix="ykbulletchk.") as d:
        d = Path(d)
        Image.new("RGBA", (32, 32), (255, 255, 255, 255)).save(d / "x.png")
        entry = (f"entry entry0 {{\n    version: 8,\n    name: \"x.png\",\n    format: 1,\n"
                 f"    width: 32,\n    height: 32,\n    sprites: {{ sprite0: {{ x: 0, y: 0, w: 16, h: 16 }} }}\n}}\n")
        ok = 0
        for name, text in scripts.items():
            body = _NAME_RE.sub("sprite0", text)
            (d / "spec.txt").write_text(entry + "\n" + body, encoding="utf-8")
            # ⚠ thanm 的 `-c` 用法与 `-l` 不同（先给 out、再给 in，且要 `-m`）——
            #   照 `make_youkai_option_art.py` 的 check_compile 那条实测过的写法。
            r = subprocess.run([str(THANM), "-c", "18", "out.anm", "spec.txt", "-m", str(ANMMAP_V8)],
                               cwd=str(d), capture_output=True, text=True)
            if r.returncode != 0:
                raise SystemExit(f"{name} 编不过：{r.stderr[:300]}")
            ok += 1
        return ok


def main():
    workdir = Path(tempfile.mkdtemp(prefix="ykbullet."))
    made = cut_sprites(workdir)

    print("== 切图（1:1，居中补到 2 的幂）")
    for name, (path, (w, h), vis, _cell) in made.items():
        if vis == 0:
            raise SystemExit(f"{name}: 切出来是空图（源矩形错了？）")
        print(f"   {name:<12} {w}x{h}  可见像素 {vis:<6} ← {path.relative_to(HERE)}")

    # sprite → ORDER 成员名（按 **(作品, anm 名, sprite 号)** 查；早苗那支一个脚本用 4 张图，
    #   四张各有自己的成员名 ⇒ 不能用「脚本名 + 序号」现编 —— 名字必须与 ORDER 片段逐字一致，
    #   否则 `make anm` 会在占位符替换那一步报「引用了不存在的成员」。）
    member_of = {(k, a, sid): nm for nm, k, a, sid, _c in MEMBERS}
    scripts = {}
    for name, kind, anm, src_num, sids, note in SCRIPTS:
        smap = {}
        for sid in sids:
            key = (kind, anm, sid)
            if key not in member_of:
                raise SystemExit(f"{name}: sprite{sid} 没有对应的 ORDER 成员（MEMBERS 要补一行）")
            smap[sid] = member_of[key]
        scripts[name] = gen_script(name, kind, anm, src_num, sids, note, smap)

    OUT_SCRIPTS.mkdir(parents=True, exist_ok=True)
    for i, (name, *_rest) in enumerate(SCRIPTS):
        fname = f"{SCRIPT0 + i}_yk_bullet_{name.lower()}.anm.txt"
        (OUT_SCRIPTS / fname).write_text(scripts[name], encoding="utf-8")
    print(f"== 脚本 {len(SCRIPTS)} 支 → {OUT_SCRIPTS}（编号 {SCRIPT0}..{SCRIPT0 + len(SCRIPTS) - 1}）")

    order, (AW, AH) = write_order(made)
    used = sum(made[n][1][0] * made[n][1][1] for n, *_ in MEMBERS)
    print(f"== ORDER 片段 → {OUT_ORDER.relative_to(HERE)}（图集 {ATLAS} {AW}x{AH}；成员总面积 {used} ≤ {AW*AH} ✓）")

    # 名字查重（对主表）
    main_names = {l.split()[0] for l in MAIN_ORDER.read_text(encoding="utf-8").splitlines()
                  if l.strip() and not l.strip().startswith("#") and not l.startswith("ATLAS")}
    dup = [n for n, *_ in MEMBERS if n in main_names]
    if dup:
        raise SystemExit(f"主表里已经有这些名字了：{dup}（改名或先合并）")

    ok = check_compile(scripts)
    print(f"== 自检：{ok}/{len(scripts)} 支脚本能编过（thanm -c 18）；名字无重复；空图 0")
    print(f"== md5（幂等判据）：{hashlib.md5((order + ''.join(scripts.values())).encode()).hexdigest()}")
    print("== 下一步：把这些行 append 进 entries/ORDER.txt 末尾，再 `make -C native anm`")


if __name__ == "__main__":
    main()
