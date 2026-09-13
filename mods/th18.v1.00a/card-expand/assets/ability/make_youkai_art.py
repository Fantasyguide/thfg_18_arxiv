#!/usr/bin/env python3
"""永夜抄（系统卡 108）**妖侧机体本体**：TH08 原版切图 + 脚本生成。

    python3 make_youkai_art.py            # 生成 + 自检（可重复跑，结果一致）

来源（原生字节只在 `local/`，gitignored；构建时取，不入库）：
  贴图+脚本  `local/th08.v1.00d/dat/player00.anm` / `player01.anm` / `player02.anm`
             —— TH08 v1.00d（anm **version 3**）。**贴图在 .anm 内部**，entry 名就是 `data/player00/playerNN.png`。
             本脚本运行时用 thanm 现场 `-l`（读矩形/脚本）与 `-x`（取贴图）到**临时目录**，仓里不落原生字节。
  复核命令   `local/vendor/thtk/build/thanm/thanm -l 8 -m local/vendor/thpages/static/mapfile/v2.anmm \
                 local/th08.v1.00d/dat/player00.anm`
             ★ anmmap 必须用 **v2.anmm**（文件头自述「for version 2: PCB and version 3: **IN**, PoFV」）——
               TH08 的 anm 是 v3；v4 是 StB/MoF 起的表，v8（TH13+）会把 `sprite` 译成 `stop`。

── ★ 一手结论 ①：妖侧本体在 `playerNN.png` 里，**不在 `playerNNb.png`**（2026-09-12）────────────
`playerNN.png` 的**本体** sprite 分两半：**前半 = 人侧、后半 = 妖侧**，对应 `playerNN.anm` 的
**script0..4 = 人侧 / script5..9 = 妖侧**（每侧 5 支 = TH08 的 5 个朝向/动作态）。

选表依据（一手，反汇编 `local/_scratch/th08_full.asm`，`Player::move` `th08:0x44aec0`）：

    44b65f: movzx  edx,BYTE PTR ds:0x164d0b1    ; 机体/队伍号
    44b666: cmp    edx,0x4
    44b669: jge    0x44b860                     ; （正常游玩恒 <4）
    44b66f: mov    eax,DWORD PTR [ebp-0x80]     ; player
    44b672: movzx  ecx,BYTE PTR [eax+0x3]       ; ★ player+0x3 = 低速位（Shift）
    44b676: test   ecx,ecx
    44b678: jne    0x44b76f                     ; 低速 ⇒ 走 0x44b76f 那支
    44b67e: …（未按 Shift 支）… 0x44b6a4 push 0x1 / 0x44b6ef push 0x2 / 0x44b729 push 0x3 / 0x44b765 push 0x4
    44b76f: …（低速支）…        0x44b795 push 0x6 / 0x44b7e0 push 0x7 / 0x44b81a push 0x8 / 0x44b856 push 0x9

    ⇒ **低速 = 妖侧 = script6/7/8/9（+ 静止用的 5）**；未按 Shift = 人侧 = script1/2/3/4（+ 0）。
    与 `docs/superpowers/specs/2026-09-12-youkai-108-design.md` §2 的一手同向
    （该处记的是 `th08:0x450f6f` 用同一个 `player+0x3` 选人/妖**射击**表 `0xe2a74`/`0xe2a78`）。

色相互证（本脚本每次运行都会重算并打印，见 stdout 的「色相互证」一节）：前半/后半在位图里就是**两个人**——
    pl00 前半 红(hue≈0°，Reimu) / 后半 洋红(hue≈351°，Yukari 八雲紫)
    pl01 前半 金(hue≈49°，Marisa) / 后半 蓝紫(hue≈242°，Alice アリス)
    pl02 前半 紫(hue≈271°，Sakuya) / 后半 粉(hue≈339°，Remilia レミリア)
    （pl03 冥界組 前半 青绿 Youmu / 后半 淡紫 Yuyuko —— TH18 没有妖夢 ⇒ 本卡不做，未切。）

── ★ 一手结论 ②：`playerNNb.png` 是**符卡/炸弹特效**，不是妖侧（2026-09-12）──────────────
证据链三条：
  1. **只有 pl00/pl01 有 b entry**（`player02.anm`/`player03.anm` 各只剩 1 个 entry）⇒ 如果是「妖侧机体」，
     四个队伍都该有。
  2. 内容对不上机体：pl00b 只有 3 个大 sprite —— `sprite44` 94×94（灰白同心环，均值 163/163/163）、
     `sprite45`/`sprite46` 各 126×126（深紫渐变，均值 ≈(36,21,90)/(26,13,68)），**没有 30×46 的机体格**。
  3. 谁装它：`Player::bomb_on_tick__reimu_single` `th08:0x40c0d8` 装 **script19**、
     `…_reimu_double` `0x40cdce` 装 **script20**、`…_yukari_double` `0x411088`/`0x41109f` 装 **script21/22**，
     `PlayerBombArea::reset` `0x44e3e3`/`0x44ea7b`/`0x44f30f` 装 18/29/21 —— 全是**炸弹（符卡）**路径。
     （pl01b 同理：`…_marisa_single` `0x40e455`–`0x40e4b0` 装 script30..34，`…_marisa_double` 装 35..39。）

── ❓ 一手结论 ③：「使魔」贴图 **没找到**，按方法纪律**停手**（不自己画、不拿别的顶）─────────
TH08 有「使い魔」这个概念，但它**不是一张贴图**：
  * `Enemy::enemy_create_familiar` `th08:0x41f280` 与 `Enemy::create_enemy_familiar_2` `0x41f110`
    都是 **ECL opcode handler**：读 ECL 参数（含 `anm`/脚本号），最后都走
    `EnemyManager::create_enemy` `0x42a680` 生成**一个普通敌人**。
  * ⇒ 关卡里「使い魔」长什么样由**那一关的 ECL + 该关敌人图集**决定，
    TH08 没有任何 entry / 没有任何 anm 是「使魔专用图」（`local/th08.v1.00d/dat/*.anm` 的 entry 名逐个查过：
    `playerNN/playerNNb`、`stgenm/stgNenm`、`etama`、`eff0N`、`face_*`、`title/sl_pl*` …，没有一个叫 familiar/使い魔，
    也没有第二处「发射口实体」的小图）。
  * ⇒ 108 的使魔是**卡侧自定**（设计文档 §3.5 已经这么定性），本仓对它**没有原生原图可借**。
    任务书原文：「若查不到明确的『使魔』图 ⇒ 不要自己画、也不要用别的东西顶，如实报告『没找到』并停手」
    —— 本生成器因此**只做妖侧机体本体**，使魔一项在 stdout 里显式报 NOT FOUND。

产出（每条都不动别人的文件）：
  ability/youkai/YOUKAI_<角色>_S<原生 sprite 号>.png  —— 30×46（pl00/pl01）或 46×46（pl02）的原生切图，
        **居中补透明**到 2 的幂画布（32×64 / 64×64）。
       ★ 居中不是随手挑的：TH08 的 sprite 四边形以 **VM 位置为中心**画
         （`AnmManager::draw_std_quad_type_0` `th08:0x463877`/`0x463904`：`(flags_lo>>11)&3&1`、
           `…&3&2` 两位为 0 时 `left = pos.x − w*scale/2`；而 `AnmVm::reset` `0x406926` 把 flags 低字设成 `0x7`
           ⇒ 那两位都是 0），所以「内容居中于画布」与原生的落点**逐像素一致**。
         build_ability.py 对小于声明尺寸的源图也是居中补（`assets/build_ability.py:119`），两边同一条公式；
         本脚本直接把补好的 2 的幂图写盘，构建期不会再补。
  ability/scripts/189..203_youkai_<角色>_s<原生脚本号>.anm.txt
       —— 正文逐条照抄 TH08 `playerNN.anm` 的 **script5..9**，**只**把 `sprite(spriteNN)` 换成 `sprite(@NAME)`；
          `flipX()` / `jmp()` / 标签 `offsetN:` / `+N:` 一个不改（注释写在 `script scriptNN {` **里面**）。
       ★ **没有 `layer()` 可抄**：原生这 5 支脚本里**一条 `layer()` 都没有**（自机本体 VM 的层由引擎 / 自机脚本自己定），
         所以本脚本**不替它编一个层号**。⚠ 如果卡侧把这几支挂在**独立 VM** 上（而不是写自机本体 `player+0x14` 的 sprite id），
         那台 VM 必须**自己**补 `layer(...)` —— 世界层 3–19，别用 HUD 的 24/25。
  ability/entries/ORDER.youkai.txt  —— 待合并进 ORDER.txt 的片段（整段重写，幂等）。
       ⚠ **不动 `entries/ORDER.txt`**（多人并发，由主 agent 统一合并）。
  stdout —— ORDER 片段 + 自检（尺寸/非空/占位符/逐行 diff/能否编过/色相互证）+ 使魔一节的 NOT FOUND 说明。
"""
import difflib
import hashlib
import math
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent          # assets/ability
REPO = HERE.parents[4]                          # assets/ability → card-expand → th18.v1.00a → mods → renkolab
OUT_ART = HERE / "youkai"
OUT_SCRIPTS = HERE / "scripts"
OUT_ORDER = HERE / "entries" / "ORDER.youkai.txt"

THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
ANMMAP_TH08 = REPO / "local/vendor/thpages/static/mapfile/v2.anmm"   # TH08 anm = version 3
ANMMAP_TH18 = REPO / "local/vendor/thpages/static/mapfile/v8.anmm"   # 目标：TH18 构建用的表
TH08_DAT = REPO / "local/th08.v1.00d/dat"
TH08_VERSION = "8"                              # thanm 的 VERSION 参数 = 作品号（TH08 是 8）
TH18_VERSION = "18"

# (TH08 队伍 anm 名, 妖侧角色名, 中文名) —— 只做 TH18 有对应自机的那三个妖侧
# （设计文档 §5「妖侧贴图（灵梦 → 紫 / 魔理沙 → 爱丽丝 / 咲夜 → 蕾米）⏳ 来源已定 = TH08」；
#   冥界組 幽々子 对应的是 TH18 没有的妖夢 ⇒ 不做。）
TEAMS = [
    ("player00", "YUKARI", "八雲紫"),
    ("player01", "ALICE", "アリス・マーガトロイド"),
    ("player02", "REMILIA", "レミリア・スカーレット"),
]
HUMAN_SCRIPTS = (0, 1, 2, 3, 4)     # 前半 = 人侧（一手：0x44b678 的「没按 Shift」支）
YOUKAI_SCRIPTS = (5, 6, 7, 8, 9)    # 后半 = 妖侧（一手：0x44b678 jne 0x44b76f 那支）
SCRIPT0 = 189                       # 脚本文件编号从这里起连续（原生 ability.anm 有 68 个脚本、最后一个是 188）

_SPRITE_RE = re.compile(r"sprite(\d+):\s*\{\s*x:\s*(\d+),\s*y:\s*(\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)\s*\}")
_TOKEN_RE = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)")
_FILE_RE = re.compile(r"^(\d+)_([A-Za-z_][A-Za-z0-9_]*)\.anm\.txt$")


# ── 取源（thanm 现场解，仓里不落原生字节）──────────────────────────────

def _require(p, what):
    if not p.is_file():
        sys.exit(f"缺 {what}：{p}\n（TH08 的 `.dat` 用 thtk 解不开，`local/th08.v1.00d/dat/` 已解好；"
                 f"缺 thanm 就先 `bash tooling/thtk/build.sh`）")


def run_thanm(args, cwd):
    r = subprocess.run([str(THANM), *args], cwd=str(cwd), capture_output=True, text=True)
    # ⚠ v2.anmm 里有一条 `74: 'acos' is a keyword, ignoring` 的**无害警告**（thanm 解析助记符表时发的）；
    #   只有这类 `… ignoring` 警告放行，其它 stderr 一律当失败（与 build_ability.py 同款严格度）。
    noise = [l for l in r.stderr.splitlines() if l.strip() and "ignoring" not in l]
    if r.returncode != 0 or noise:
        sys.exit(f"thanm {' '.join(map(str, args))} 失败：\n{r.stderr or r.stdout}")
    return r.stdout


def read_anm(anm: Path, workdir: Path):
    """→ (spec 文本, 解出来的贴图 PNG 路径)。

    `-l` 拿脚本/sprite 表；`-x` 把 entry 里的贴图解到 workdir（entry 名自带相对路径 `data/player00/…`）。
    """
    text = run_thanm(["-l", TH08_VERSION, "-m", str(ANMMAP_TH08), str(anm)], workdir)
    run_thanm(["-x", TH08_VERSION, str(anm)], workdir)
    png = workdir / "data" / "player00" / f"{anm.stem}.png"
    if not png.is_file():
        sys.exit(f"{anm.name}: thanm -x 没解出 {png.relative_to(workdir)}（entry 名变了？）")
    return text, png


def parse_entry(text, entry_name):
    """→ ({sprite 号: (x, y, w, h)}, (贴图 W, H))。矩形只从 spec 的 sprites 表读，不写死。"""
    i = text.find(f'name: "{entry_name}"')
    if i < 0:
        sys.exit(f"spec 里找不到 entry {entry_name}")
    j = text.find("sprites:", i)
    depth, start, end = 0, text.index("{", j), None
    for k in range(start, len(text)):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                end = k
                break
    rects = {int(m.group(1)): tuple(int(x) for x in m.groups()[1:])
             for m in _SPRITE_RE.finditer(text[start:end])}
    if not rects:
        sys.exit(f"entry {entry_name} 的 sprites 表是空的")
    wm = re.search(r'name: "%s",\n\s*format: \d+,\n\s*width: (\d+),\n\s*height: (\d+),'
                   % re.escape(entry_name), text)
    if not wm:
        sys.exit(f"entry {entry_name} 的 width/height 没解析出来")
    return rects, (int(wm.group(1)), int(wm.group(2)))


def extract_script(text, n):
    m = re.search(rf"^script script{n} \{{.*?^\}}$", text, re.S | re.M)
    if not m:
        sys.exit(f"TH08 spec 里找不到 script{n}")
    return m.group(0)


def script_sprites(text, nums):
    """→ {脚本号: [sprite 号…]}（保持出现顺序，去重）。"""
    out = {}
    for n in nums:
        seen = []
        for s in (int(x) for x in re.findall(r"sprite\(sprite(\d+)\)", extract_script(text, n))):
            if s not in seen:
                seen.append(s)
        out[n] = seen
    return out


# ── 切图 ─────────────────────────────────────────────────────────────

def pow2_ceil(v):
    p = 1
    while p < v:
        p <<= 1
    return p


def canvas_size(w, h):
    """→ (W, H)：都是 2 的幂、且 ≥ 内容（写进 ORDER 的尺寸必须是 2 的幂，构建硬门）。"""
    return pow2_ceil(w), pow2_ceil(h)


def dominant_hue(im):
    """→ (饱和像素的平均色相(度) | None, 可见像素平均 RGB)。用来做「前后半是两个人」的独立互证。"""
    raw = im.convert("RGBA").tobytes()          # 不用 getdata()：它在 Pillow 14 起被弃用
    vis = [(raw[i], raw[i + 1], raw[i + 2]) for i in range(0, len(raw), 4) if raw[i + 3] > 32]
    if not vis:
        return None, (0.0, 0.0, 0.0)
    mean = tuple(sum(c[i] for c in vis) / len(vis) for i in range(3))
    sat = [c for c in vis if max(c) > 60 and (max(c) - min(c)) / max(max(c), 1) > 0.25]
    if not sat:
        return None, mean
    r, g, b = (sum(c[i] for c in sat) / len(sat) / 255.0 for i in range(3))
    mx, mn = max(r, g, b), min(r, g, b)
    if mx == mn:
        return 0.0, mean
    d = mx - mn
    h = ((g - b) / d) % 6 if mx == r else ((b - r) / d + 2 if mx == g else (r - g) / d + 4)
    return h * 60.0, mean


def crop_team(sheet_path, rects, sprite_ids, char):
    """该队妖侧 sprite 逐个切出来、**居中**补到 2 的幂画布。

    → {ORDER 名: (路径, 画布 WxH, sprite 号, 可见像素数, 源单元字节)}
    """
    im = Image.open(sheet_path).convert("RGBA")
    OUT_ART.mkdir(parents=True, exist_ok=True)
    made = {}
    for sid in sorted(sprite_ids):
        x, y, w, h = rects[sid]
        cw, ch = canvas_size(w, h)
        cell = im.crop((x, y, x + w, y + h))
        canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        canvas.paste(cell, ((cw - w) // 2, (ch - h) // 2))   # ★ 居中 = 原生的 VM 中心锚点（见文件头 ①）
        name = f"YOUKAI_{char}_S{sid}"
        path = OUT_ART / f"{name}.png"
        canvas.save(path)
        vis = sum(1 for a in canvas.getchannel("A").get_flattened_data() if a > 32)
        made[name] = (path, (cw, ch), sid, vis, cell.tobytes())
    return made


# ── 脚本 ─────────────────────────────────────────────────────────────

def header_comment(anm_name, src_num, char, cn, mapping):
    return (f"    // 由 assets/ability/make_youkai_art.py 从 TH08 v1.00d `{anm_name}` script{src_num} 生成，别手改。\n"
            f"    // 妖侧 = 八雲紫/アリス/レミリア 之一（本支：{char} {cn}）；"
            f"一手：低速位 `th08:0x44b672 movzx ecx,[player+0x3]` ⇒ script5..9 是妖侧、0..4 是人侧。\n"
            f"    // 指令逐条照抄（sprite/flipX/jmp/标签一字未动），只把 sprite 引用换成 ORDER 名字：\n"
            f"    //   " + "，".join(f"sprite{s} → @{mapping[s]}" for s in sorted(mapping)) + "\n"
            f"    // ⚠ 原生这支脚本里**没有 layer()**（自机本体 VM 的层不由它定）⇒ 这里也不写；"
            f"挂独立 VM 时由挂它的那一方自己补 layer（世界层 3–19，别用 HUD 的 24/25）。")


def gen_scripts(team_specs, order_names):
    """→ ({文件名: (正文, 期望正文, 原文, 脚本号, 角色)}, {文件名: {sprite号 → ORDER 名}})。"""
    OUT_SCRIPTS.mkdir(parents=True, exist_ok=True)
    out, maps = {}, {}
    dst = SCRIPT0
    for (anm_name, char, cn, _png, _rects, originals, body) in team_specs:
        for src_num in YOUKAI_SCRIPTS:
            original = originals[src_num]
            mapping = {s: f"YOUKAI_{char}_S{s}" for s in body[src_num]}
            for nm in mapping.values():
                assert nm in order_names, f"{nm} 不在 ORDER 片段里"
            head_old = f"script script{src_num} {{"
            head_new = f"script script{dst} {{\n{header_comment(anm_name, src_num, char, cn, mapping)}"
            body_txt = original.replace(head_old, head_new, 1)
            for sid, nm in mapping.items():
                body_txt = body_txt.replace(f"sprite(sprite{sid})", f"sprite(@{nm})")
            want = original.replace(head_old, head_new, 1)        # 只做「脚本号 + 注释头」这一处结构改动
            left = re.findall(r"sprite\(sprite(-?\d+)\)", body_txt)   # 漏换的引用（原生出现新 sprite 号时会炸）
            if left:
                sys.exit(f"{anm_name} script{src_num} 里还有没替换的 sprite 引用 {left}")
            name = f"{dst}_youkai_{char.lower()}_s{src_num}.anm.txt"
            if not _FILE_RE.match(name):
                sys.exit(f"文件名不合 build_ability.py 的约定：{name}")
            (OUT_SCRIPTS / name).write_text(body_txt + "\n", encoding="utf-8")
            out[name] = (body_txt, want, original, dst, f"{char}/{cn}")
            maps[name] = mapping
            print(f"[script] {name}  ← TH08 {anm_name} script{src_num}"
                  f"（{char} {cn}：{', '.join(f'sprite{s}' for s in sorted(mapping))}）")
            dst += 1
    return out, maps


def order_rows(team_specs, made):
    """→ [(ORDER 名, 源图(相对 assets/), WxH, 队伍 anm, sprite 号)]，按 队伍 → sprite 号 排。"""
    rows = []
    for (anm_name, char, _cn, _png, _rects, _scripts, _body) in team_specs:
        for name, (_path, size, sid, _vis, _bytes) in sorted(made[char].items(), key=lambda kv: kv[1][2]):
            rows.append((name, f"ability/youkai/{name}.png", f"{size[0]}x{size[1]}", anm_name, sid))
    return rows


def write_order(rows):
    hdr = [
        "# 永夜抄（系统卡 108）妖侧机体本体 —— TH08 v1.00d `player00/01/02.anm` 的 script5..9 + `playerNN.png` 切图。",
        "# 本片段由 assets/ability/make_youkai_art.py 生成（幂等，整段重写）；合并时把下面这些行追加进 ORDER.txt 即可。",
        "# 每行 `NAME  源图(相对 assets/)  WxH`；WxH 都是 2 的幂 ⇒ 整张贴图一个 sprite。",
        "# ★ 一手：妖侧在 `playerNN.png` 的**后半** sprite（人侧是前半），**不是** `playerNNb.png`",
        "#   （`playerNNb.png` 是符卡/炸弹特效图，见生成器文件头「一手结论 ②」）。",
        "# 选择依据一手：`th08:0x44b672 movzx ecx,[player+0x3]`（低速位）→ script6/7/8/9 走妖侧那支。",
        "# 三队 ↔ 妖侧角色：player00 = 八雲紫 / player01 = アリス / player02 = レミリア（pl03 幽々子 对应 TH18 没有的妖夢 ⇒ 未切）。",
    ]
    lines = hdr + [f"{n}  {p}  {s}" for n, p, s, _a, _i in rows]
    OUT_ORDER.parent.mkdir(parents=True, exist_ok=True)
    OUT_ORDER.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[order] {OUT_ORDER.relative_to(HERE.parent)}（{len(rows)} 行；**不动** entries/ORDER.txt）")
    return lines


# ── 自检 ─────────────────────────────────────────────────────────────

def check_pngs(made, team_specs):
    """① 每张 PNG：尺寸是 2 的幂；② 不全是透明；且与源图对应单元逐像素一致。"""
    files = sorted(OUT_ART.glob("YOUKAI_*.png"))
    want = sum(len(made[t[1]]) for t in team_specs)
    assert len(files) == want, f"① youkai/ 下有 {len(files)} 张 PNG，预期 {want}"
    vis_all, sizes = [], set()
    for (anm_name, char, _cn, _png, rects, _sc, _body) in team_specs:
        for name, (path, size, sid, vis, cell_bytes) in made[char].items():
            im = Image.open(path).convert("RGBA")
            assert im.size == size, f"① {name}.png 是 {im.size}，标称 {size}"
            assert (size[0] & (size[0] - 1)) == 0 and (size[1] & (size[1] - 1)) == 0, \
                f"① {name}.png 尺寸 {size} 不是 2 的幂"
            x, y, w, h = rects[sid]
            bx, by = (size[0] - w) // 2, (size[1] - h) // 2
            assert im.crop((bx, by, bx + w, by + h)).tobytes() == cell_bytes, \
                f"② {name}.png 与 {anm_name} sprite{sid} {rects[sid]} 不逐像素一致"
            assert im.getchannel("A").getbbox() is not None, f"② {name}.png 整张透明"
            assert vis >= 200, f"② {name}.png 只有 {vis} 个可见像素（源单元 {w}×{h}）"
            vis_all.append((name, vis))
            sizes.add(size)
    print(f"① {len(files)} 张 PNG 尺寸全是 2 的幂（画布 {sorted(sizes)}），"
          f"每张的可见区居中、与源图单元逐像素一致 ✓")
    print(f"② {len(files)} 张全部非空（可见像素 {min(v for _n, v in vis_all)}–{max(v for _n, v in vis_all)}，"
          f"阈值 200）✓")


def check_hue(team_specs):
    """独立互证：前半/后半在位图里是两个不同的人（平均 RGB 距离 > 40 或色相相差 > 40°）。"""
    print("\n# —— 色相互证（前半 = 人侧 / 后半 = 妖侧）——")
    for (anm_name, char, cn, png, rects, _sc, body) in team_specs:
        src = Image.open(png).convert("RGBA")

        def stat(nums):
            ids = sorted({s for n in nums for s in body[n]})
            crops = []
            for sid in ids:
                x, y, w, h = rects[sid]
                crops.append(src.crop((x, y, x + w, y + h)))
            strip = Image.new("RGBA", (sum(c.width for c in crops), max(c.height for c in crops)), (0, 0, 0, 0))
            ox = 0
            for c in crops:
                strip.paste(c, (ox, 0))
                ox += c.width
            return ids, dominant_hue(strip)

        hum_ids, (h_hum, m_hum) = stat(HUMAN_SCRIPTS)
        you_ids, (h_you, m_you) = stat(YOUKAI_SCRIPTS)
        dd = math.dist(m_hum, m_you)
        assert dd > 40 or abs(h_hum - h_you) > 40, \
            f"{anm_name}: 前半/后半颜色太像（Δrgb={dd:.0f}，hue {h_hum}/{h_you}）—— 切分依据要复核"
        fmt = lambda h: "?" if h is None else f"{h:.0f}°"
        print(f"  {anm_name}  人侧 sprite{hum_ids[0]}..{hum_ids[-1]} hue={fmt(h_hum)} "
              f"rgb=({m_hum[0]:.0f},{m_hum[1]:.0f},{m_hum[2]:.0f})"
              f"   妖侧 {char} {cn} sprite{you_ids[0]}..{you_ids[-1]} hue={fmt(h_you)} "
              f"rgb=({m_you[0]:.0f},{m_you[1]:.0f},{m_you[2]:.0f})   Δrgb={dd:.0f} ✓")


def check_scripts(scripts, maps, rows):
    """③ 每个 @名字 都在 ORDER 片段里有定义；④ 与原生原文逐行 diff 只差 sprite 引用。"""
    names = {n for n, _p, _s, _a, _i in rows}
    changed_total = 0
    for fname, (body, want, _orig, _idx, _who) in sorted(scripts.items(), key=lambda kv: kv[1][3]):
        missing = set(_TOKEN_RE.findall(body)) - names
        assert not missing, f"③ {fname}: ORDER 片段里没有 {sorted(missing)}"
        gen, ref = body.splitlines(), want.splitlines()
        assert len(gen) == len(ref), f"④ {fname}: 行数 {len(gen)} vs 期望 {len(ref)}"
        for i, (g, e) in enumerate(zip(gen, ref)):
            if g == e:
                continue
            mg = re.fullmatch(r"\s*sprite\(@(YOUKAI_[A-Z_]+_S\d+)\);", g)
            me = re.fullmatch(r"\s*sprite\(sprite(\d+)\);", e)
            assert mg and me, f"④ {fname}:{i + 1} 改到了 sprite 引用以外：{e!r} → {g!r}"
            assert mg.group(1) == maps[fname][int(me.group(1))], \
                f"④ {fname}:{i + 1} {e.strip()} 应换成 sprite(@{maps[fname][int(me.group(1))]})，实际 {g.strip()}"
            changed_total += 1
    print(f"③ {len(scripts)} 条脚本里的 @名字全部在 ORDER 片段有定义（{len(names)} 个名字） ✓")
    print(f"④ 与 TH08 原文逐行 diff：差异**只有** {changed_total} 行 sprite 引用"
          f"（脚本号 + 注释头之外正文一字未动） ✓")


def print_script_diffs(scripts):
    print("\n# —— ④ diff 证据：TH08 原文（-）vs 生成脚本（+）——")
    for fname in sorted(scripts, key=lambda n: scripts[n][3]):
        body, _want, original, idx, who = scripts[fname]
        ref, gen = original.splitlines(), body.splitlines()
        print(f"\n=== {fname}（script{idx}，{who}；正文 {len(gen)} 行；下面是与 TH08 原文的**全部**差异）")
        for line in difflib.unified_diff(ref, gen, "TH08-original", "generated", lineterm="", n=0):
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
    """⑤ 每条脚本：把 `@NAME` 换成 `sprite0`、脚本号改成 0，在 /tmp 里用 thanm -c 18 单独编一次。"""
    print("\n# —— ⑤ 能否编过（thanm -c 18，/tmp 里单独编；不碰 native/build/、不跑 make）——")
    with tempfile.TemporaryDirectory(prefix="youkai-anm-") as td:
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
            run_thanm(["-c", TH18_VERSION, str(out), "spec.txt", "-m", str(ANMMAP_TH18)], d)
            assert out.is_file() and out.stat().st_size > 0, f"⑤ {fname} 编出来是空的"
            ok += 1
            print(f"  {fname} → spec script0（@名字临时代 sprite0）：thanm -c 18 OK（{out.stat().st_size} B）")
        print(f"⑤ {ok}/{len(scripts)} 条脚本都能编过（临时目录已删，native/build 一行没动） ✓")


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
    _require(ANMMAP_TH08, "TH08 用的 anmmap v2.anmm")
    _require(ANMMAP_TH18, "TH18 构建用的 anmmap v8.anmm")
    for name, _c, _cn in TEAMS:
        _require(TH08_DAT / f"{name}.anm", f"TH08 原版 {name}.anm")

    with tempfile.TemporaryDirectory(prefix="youkai-src-") as td:
        tmp = Path(td)
        team_specs, made = [], {}
        for name, char, cn in TEAMS:
            spec_text, png = read_anm(TH08_DAT / f"{name}.anm", tmp)
            entry_name = f"data/player00/{name}.png"
            rects, sheet = parse_entry(spec_text, entry_name)
            body = script_sprites(spec_text, HUMAN_SCRIPTS + YOUKAI_SCRIPTS)
            hum = sorted({s for h in HUMAN_SCRIPTS for s in body[h]})
            you = sorted({s for y in YOUKAI_SCRIPTS for s in body[y]})
            # ★ 两半的切分不是写死的：从脚本自己的 sprite 引用推出来，并断言互不相交、妖侧在后半
            assert set(hum).isdisjoint(you), f"{name}: 人侧/妖侧 sprite 集相交 —— 切分依据要复核"
            assert max(hum) < min(you), f"{name}: 妖侧 sprite 不是后半（人侧到 {max(hum)}，妖侧从 {min(you)} 起）"
            for sid in hum + you:
                assert sid in rects, f"{name}: script 引用了 entry {entry_name} 里不存在的 sprite{sid}"
            made[char] = crop_team(png, rects, you, char)
            team_specs.append((name, char, cn, png, rects,
                               {n: extract_script(spec_text, n) for n in HUMAN_SCRIPTS + YOUKAI_SCRIPTS}, body))
            print(f"[src] {name}.anm（anm v3，贴图 {sheet[0]}×{sheet[1]}，entry {entry_name}，{len(rects)} 个 sprite）"
                  f"  人侧 script0..4 = sprite{hum} / 妖侧 script5..9 = sprite{you}（{char} {cn}）")
            print(f"[crop] {char} {cn}：{len(made[char])} 张 → {OUT_ART.relative_to(HERE.parent)}"
                  f"（画布 {sorted({v[1] for v in made[char].values()})}）")

        rows = order_rows(team_specs, made)
        order_lines = write_order(rows)
        scripts, maps = gen_scripts(team_specs, {r[0] for r in rows})

        print("\n# —— ORDER.youkai.txt 内容（合并进 ORDER.txt 时追加这些行）——")
        for line in order_lines:
            print(line)

        check_pngs(made, team_specs)
        check_hue(team_specs)
        check_scripts(scripts, maps, rows)
        print_script_diffs(scripts)
        check_compile(scripts)

        print("\n# —— ❓ 使魔：NOT FOUND（本生成器**不做**，也不拿别的东西顶）——")
        print("  TH08 的「使い魔」不是一张贴图：`Enemy::enemy_create_familiar` th08:0x41f280 /")
        print("  `Enemy::create_enemy_familiar_2` th08:0x41f110 都是 ECL opcode handler，")
        print("  最终都调 `EnemyManager::create_enemy` th08:0x42a680，用 ECL 给的 anm/脚本号生成一个普通敌人。")
        print("  ⇒ 外观由关卡 ECL 决定；`local/th08.v1.00d/dat/*.anm` 里没有任何 entry 是使魔专用图。")
        print("  按任务书 + METHOD.md「UI 与动画一律先从原代码找」：不自己画、不拿别的顶 —— 见生成器文件头「一手结论 ③」。")

        print_md5(sorted(OUT_ART.glob("YOUKAI_*.png"))
                  + sorted(p for p in OUT_SCRIPTS.glob("18*_youkai_*.anm.txt"))
                  + [OUT_ORDER])
    print("\n全部自检通过（幂等：重跑覆盖同名文件，内容不变）。")


if __name__ == "__main__":
    main()
