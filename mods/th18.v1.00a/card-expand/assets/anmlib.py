"""thanm spec 的读写与重建（build_abcard.py / build_ability.py 共用）。

原生 spec / 贴图来自 tooling/thtk/unpack.py 解出的 local/th18.v1.00a/anm/<名>/。
约定：只**追加** entry / 脚本，绝不改原生的；重建后逐项校验原生部分没变。
"""
import filecmp
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent            # assets/
MOD = HERE.parent                                 # card-expand/
REPO = MOD.parents[2]                             # renkolab/
sys.path.insert(0, str(REPO / "tooling" / "thtk"))
from unpack import find_tools  # noqa: E402,F401  —— (thanm, thdat, anmmap_dir)

VERSION = "18"
RETAIL_ROOT = REPO / "local" / "th18.v1.00a" / "anm"
BUILD = MOD / "native" / "build"

_ENTRY = re.compile(r"^entry entry(\d+) \{\n(.*?)^\}\n?", re.S | re.M)
_SCRIPT = re.compile(r"^script script(\d+) \{\n(.*?)^\}\n?", re.S | re.M)
_FIELD = re.compile(r"^\s{4}(\w+): (.+),$", re.M)
_SPRITE = re.compile(r"sprite(\d+): \{ x: (\S+), y: (\S+), w: (\S+), h: (\S+) \}")
_SPRITE_ID = re.compile(r"^\s+sprite(\d+):", re.M)
# 占位写法：`@NAME`（推荐）或 `@NAME@`（收尾那个 `@` 也一起吃掉）。
# ★ 2026-09-12 真踩过：写成 `scriptNew(@SCRIPT_X@)` 而正则不吃收尾 `@` 时，替换后会在 `)` 前
#   留一个裸 `@`，`thanm -c` 直接报 `illegal token: @` 并整条构建失败。
#   多消费一个可选 `@` 对老写法（纯 `@NAME`）零影响。
_TOKEN = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)@?")


# ── 解析 ──────────────────────────────────────────────────────────────
def parse_entries(spec_text: str):
    """→ [{idx, name, keys, fields, sprite, n_sprites, block, span}]；fields 不含 name，keys 记字段顺序（含 name）。

    `sprite` 只在**恰好一个** sprite 时给矩形（多 sprite entry 时为 None，用 `parse_sprite_rects(block)`
    逐个取；`n_sprites` 两种情况都有）。
    """
    out = []
    for m in _ENTRY.finditer(spec_text):
        body = m.group(2)
        head = body.split("    sprites: {", 1)[0]
        keys, fields, name = [], {}, None
        for fm in _FIELD.finditer(head):
            k, v = fm.group(1), fm.group(2)
            keys.append(k)
            if k == "name":
                name = v.strip('"')
            else:
                fields[k] = v
        sprites = _SPRITE.findall(body)
        s = sprites[0] if len(sprites) == 1 else None
        out.append({"idx": int(m.group(1)), "name": name, "keys": keys, "fields": fields,
                    "sprite": {"x": s[1], "y": s[2], "w": s[3], "h": s[4]} if s else None,
                    "n_sprites": len(sprites), "block": m.group(0), "span": m.span()})
    return out


def parse_scripts(spec_text: str):
    return [{"idx": int(m.group(1)), "block": m.group(0), "span": m.span()} for m in _SCRIPT.finditer(spec_text)]


def max_sprite_id(spec_text: str) -> int:
    ids = [int(x) for x in _SPRITE_ID.findall(spec_text)]
    return max(ids) if ids else -1


# ── 生成 ──────────────────────────────────────────────────────────────
def make_entry(template: dict, idx: int, png_rel: str, sprite_idx: int = None, sprites=None) -> str:
    """照抄模板字段，只换 name、entry 号与 sprite 号（sprite 号默认 = entry 号，abcard 的约定）。

    `sprites` = `[(sprite 号, x, y, w, h), …]`：**一个 entry 多个 sprite**（图集）。
    引擎完全支持（贴图键挂在 entry 上、sprite 表是**全文件扁平**的，见
    `engine/anm/th18/06-entry-limit-and-multi-sprite.md` §2）；给 `sprites` 时忽略 `sprite_idx`。
    """
    if template["sprite"] is None and sprites is None:
        raise SystemExit(f"模板 entry{template['idx']} 不是单 sprite，不能当模板")
    lines = [f"entry entry{idx} {{"]
    for k in template["keys"]:
        v = f'"{png_rel}"' if k == "name" else template["fields"][k]
        lines.append(f"    {k}: {v},")
    if sprites is None:
        s = template["sprite"]
        sid = idx if sprite_idx is None else sprite_idx
        sprites = [(sid, s["x"], s["y"], s["w"], s["h"])]
    lines.append("    sprites: {")
    for k, (sid, x, y, w, h) in enumerate(sprites):
        # 尾逗号：thanm 的 spec 解析器要求列表项之间有 `,`（只有一项时才能省）
        lines.append(f"        sprite{sid}: {{ x: {x}, y: {y}, w: {w}, h: {h} }}"
                     + ("," if k + 1 < len(sprites) else ""))
    lines += ["    }", "}", ""]
    return "\n".join(lines)


def parse_sprite_rects(block: str):
    """entry 块 → {sprite 号: (x, y, w, h)}（整数，用于多 sprite entry 的逐条核对）。"""
    return {int(m.group(1)): tuple(int(m.group(i)) for i in (2, 3, 4, 5))
            for m in _SPRITE.finditer(block)}


def read_order_names(path: Path):
    """一列名字的 ORDER（abcard 的 `assets/cards/ORDER.txt`）：一行一个 `NAME`，跳过 `#` 与空行。
    行序就是编号（abcard：第 k 行 → sprite 118+2k / 119+2k）。名字不许重复。"""
    if not path.is_file():
        raise SystemExit(f"没有 {path}")
    names = [l.strip() for l in path.read_text(encoding="utf-8").splitlines()
             if l.strip() and not l.lstrip().startswith("#")]
    dup = sorted({n for n in names if names.count(n) > 1})
    if dup:
        raise SystemExit(f"{path.name} 有重复：{dup}")
    return names


def order_signature(items) -> str:
    """`items` 的**结构指纹**（sha256）—— 给「重排检测」用：逐行哈希只看字面，
    它另外锁住"这些行**被解释成什么**"（例如有人往图集段里插了一个空行 ⇒ 段被截断、
    后面的成员变成单图 entry：数据行一个字没变，但结构变了）。"""
    norm = []
    for it in items:
        if it["kind"] == "single":
            norm.append(["single", it["name"], it["src"], it["size"]])
        elif it["kind"] == "ref":
            norm.append(["ref", it["name"], it["target"]])
        else:
            norm.append(["atlas", it["name"], list(it["canvas"]),
                         list(it["grid"]) if it["grid"] else None,
                         [[m["name"], m["src"], list(m["rect"])] for m in it["members"]]])
    blob = json.dumps(norm, sort_keys=True, ensure_ascii=False)
    return hashlib.sha256(blob.encode("utf-8")).hexdigest()


def insert_entries(spec_text: str, blocks) -> str:
    """新 entry 块插在最后一个 entry 之后、第一个 script 之前。"""
    entries = parse_entries(spec_text)
    if not entries:
        raise SystemExit("spec 里没有 entry")
    end = entries[-1]["span"][1]
    return spec_text[:end] + "\n" + "\n".join(b.rstrip("\n") + "\n" for b in blocks) + spec_text[end:]


def append_scripts(spec_text: str, blocks) -> str:
    """新脚本追加到文件末尾（脚本号 = 出现顺序，所以只能追加）。"""
    return spec_text.rstrip("\n") + "\n\n" + "\n".join(b.rstrip("\n") + "\n" for b in blocks)


def substitute(text: str, mapping: dict) -> str:
    """把 `@NAME` 换成 mapping[NAME]；有没定义的 token 直接报错。"""
    def rep(m):
        k = m.group(1)
        if k not in mapping:
            raise SystemExit(f"脚本里有未定义的占位 @{k}（已知：{', '.join(sorted(mapping))}）")
        return str(mapping[k])
    return _TOKEN.sub(rep, text)


_ATLAS_HEAD = re.compile(r"^ATLAS\s+(\S+)\s+(\d+)x(\d+)(?:@(\d+)x(\d+))?$", re.I)
_RECT = re.compile(r"^@(\d+),(\d+),(\d+)x(\d+)$", re.I)
_SIZE = re.compile(r"^(\d+)x(\d+)$", re.I)


def read_order(path: Path):
    """ORDER.txt（v2）→ `(items, lines)`。

    `lines` = `[(行号, 原文)]`（**只含数据行**，注释与空行不算）—— 给「重排检测」硬门做逐行比对。
    `items` 逐项：

    | 写法 | 解析成 |
    | --- | --- |
    | `NAME 源图 [WxH]` | `{"kind":"single", name, src, size=(w,h)\|None}` |
    | `ATLAS NAME WxH[@CxR]` + 成员行 | `{"kind":"atlas", name, canvas, grid, members}` |
    | `REF 名字 目标` | `{"kind":"ref", name, target}` |

    成员行两种矩形写法（§4.1 规则①：**矩形 = 成员自己那张图的像素尺寸**，不补边）：

    * 网格段（段头带 `@CxR`）：成员行 `成员名 源图 [WxH]`；矩形 = 画布按 C×R 等分后的**第 k 格**
      （k = 段内第几个成员，**行优先**）；写了 `WxH` 就必须等于单格尺寸。
    * 显式段（段头不带 `@`）：成员行 `成员名 源图 @x,y,WxH`；矩形直接写死。

    名字（token）解析约定：成员名 → 自己的 sprite 号；`图集名_k` → 段内第 k 个成员；
    `图集名` → 第 0 个成员（向后兼容「一行一个 entry」的老写法）。

    ⚠ **段在空行处结束**：图集段头之后、直到下一个空行，都是它的成员行。所以段内**不要**夹空行
    （夹了就"段被截断、后面的成员变成单图行"）；`ORDER.frozen.txt` 里另存了一份**结构指纹**
    （`order_signature()`），这种结构性改动同样会被重排检测逮住。
    """
    if not path.is_file():
        raise SystemExit(f"没有 {path}")
    items, lines = [], []
    seen_names = {}
    cur = None                                            # 当前图集段
    for no, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        s = raw.strip()
        if not s:
            cur = None                                    # 空行 = 段结束
            continue
        if s.startswith("#"):
            continue
        lines.append((no, s))
        cols = s.split()

        def _dup(name):
            if name in seen_names:
                raise SystemExit(f"{path.name}:{no} 名字 {name} 重复（{path.name}:{seen_names[name]} 已用过）")
            seen_names[name] = no

        if cols[0].upper() == "ATLAS":
            if len(cols) != 3:
                raise SystemExit(f"{path.name}:{no} 图集段头应为 `ATLAS <名字> <画布WxH[@列x行]>`：{s}")
            m = _ATLAS_HEAD.match(s)
            if not m:
                raise SystemExit(f"{path.name}:{no} 图集段头尺寸写不对（例：`256x160` 或 `128x64@8x2`）：{s}")
            _dup(m.group(1))
            grid = (int(m.group(4)), int(m.group(5))) if m.group(4) else None
            cur = {"kind": "atlas", "line_no": no, "name": m.group(1),
                   "canvas": (int(m.group(2)), int(m.group(3))), "grid": grid, "members": []}
            items.append(cur)
            continue
        if cols[0].upper() == "REF":
            if len(cols) != 3:
                raise SystemExit(f"{path.name}:{no} REF 行应为 `REF <退役名> <目标名>`：{s}")
            _dup(cols[1])
            items.append({"kind": "ref", "line_no": no, "name": cols[1], "target": cols[2]})
            cur = None
            continue
        if cur is None:                                   # ── 单图行（老写法）
            if len(cols) < 2:
                raise SystemExit(f"{path.name}:{no} 一行要至少 2 列（`NAME 源图`）：{s}\n"
                                 f"  （一列名字的 ORDER 用 `anmlib.read_order_names()`，如 abcard）")
            if len(cols) > 3:
                raise SystemExit(f"{path.name}:{no} 单图行最多 3 列（`NAME 源图 [WxH]`）：{s}")
            _dup(cols[0])
            size = None
            if len(cols) > 2:
                mm = _SIZE.match(cols[2])
                if not mm:
                    raise SystemExit(f"{path.name}:{no} 尺寸列应为 `WxH`（图集成员请先写 `ATLAS …` 段头）：{s}")
                size = (int(mm.group(1)), int(mm.group(2)))
            items.append({"kind": "single", "line_no": no, "name": cols[0],
                          "src": cols[1], "size": size})
            continue
        # ── 图集成员行
        if len(cols) > 3:
            raise SystemExit(f"{path.name}:{no} 成员行最多 3 列：{s}")
        name, src = cols[0], cols[1]
        _dup(name)
        size = rect = None
        if len(cols) > 2:
            rm = _RECT.match(cols[2])
            if rm:
                x, y, w, h = (int(rm.group(i)) for i in (1, 2, 3, 4))
                rect, size = (x, y, w, h), (w, h)
            else:
                mm = _SIZE.match(cols[2])
                if not mm:
                    raise SystemExit(f"{path.name}:{no} 成员第 3 列应为 `WxH`（网格段）或 `@x,y,WxH`（显式段）：{s}")
                size = (int(mm.group(1)), int(mm.group(2)))
        if (rect is not None) and (cur["grid"] is not None):
            raise SystemExit(f"{path.name}:{no} 网格段（段头带 `@列x行`）里不许再写 `@x,y,WxH`：{s}")
        cur["members"].append({"line_no": no, "name": name, "src": src, "rect": rect, "size": size})
    # 收尾：补全网格段的矩形 + 段内计数校验
    for it in items:
        if it["kind"] != "atlas":
            continue
        colsn, rowsn = it["grid"] if it["grid"] else (None, None)
        if colsn is not None:
            cell = (it["canvas"][0] // colsn, it["canvas"][1] // rowsn)
            if cell[0] * colsn != it["canvas"][0] or cell[1] * rowsn != it["canvas"][1]:
                raise SystemExit(f"{path.name}:{it['line_no']} 网格 {colsn}x{rowsn} 除不尽画布 "
                                 f"{it['canvas'][0]}x{it['canvas'][1]}")
            if len(it["members"]) > colsn * rowsn:
                raise SystemExit(f"{path.name}:{it['line_no']} 图集 {it['name']} 有 {len(it['members'])} 个成员，"
                                 f"超过网格 {colsn}x{rowsn} 的 {colsn * rowsn} 格")
        for k, mem in enumerate(it["members"]):
            if mem["rect"] is None:
                if colsn is None:
                    raise SystemExit(f"{path.name}:{mem['line_no']} 显式段 {it['name']} 的成员要写 `@x,y,WxH`")
                cx, cy = (k % colsn) * cell[0], (k // colsn) * cell[1]
                mem["rect"] = (cx, cy, cell[0], cell[1])
                if mem["size"] is not None and mem["size"] != cell:
                    raise SystemExit(f"{path.name}:{mem['line_no']} 声明 {mem['size'][0]}x{mem['size'][1]} "
                                     f"≠ 单格 {cell[0]}x{cell[1]}")
                mem["size"] = cell
        # 矩形必须落在画布内（图上不允许越界；原生有越界写法，但我们的图集是货架式打包）
        for mem in it["members"]:
            x, y, w, h = mem["rect"]
            if x < 0 or y < 0 or x + w > it["canvas"][0] or y + h > it["canvas"][1]:
                raise SystemExit(f"{path.name}:{mem['line_no']} 成员 {mem['name']} 的矩形 "
                                 f"{x},{y},{w},{h} 超出画布 {it['canvas'][0]}x{it['canvas'][1]}")
    return items, lines


# ── 外部工具 ──────────────────────────────────────────────────────────
def thanm(args, cwd, tools):
    r = subprocess.run([tools[0], *args], cwd=cwd, capture_output=True, text=True)
    if r.returncode != 0 or r.stderr.strip():
        raise SystemExit(f"thanm {' '.join(map(str, args))} 失败：\n{r.stderr or r.stdout}")
    return r.stdout


def retail(name: str):
    """→ (retail_dir, spec_text)；没解包就提示。"""
    d = RETAIL_ROOT / name
    spec = d / f"{name}.anm.txt"
    if not spec.is_file():
        raise SystemExit(f"没有 {spec}：先 python3 tooling/thtk/unpack.py th18.v1.00a")
    return d, spec.read_text()


def link_retail_textures(tex_dir: Path, retail_dir: Path, retail_entries):
    """thanm -c 的贴图路径相对 cwd：把原生贴图软链进 tex_dir（按 entry name 的相对路径）。"""
    for e in retail_entries:
        src, dst = retail_dir / e["name"], tex_dir / e["name"]
        if not src.is_file():
            raise SystemExit(f"原生贴图缺 {src}：先 python3 tooling/thtk/unpack.py th18.v1.00a")
        if not dst.exists():
            dst.parent.mkdir(parents=True, exist_ok=True)
            os.symlink(src, dst)


def compile_anm(spec_text: str, tex_dir: Path, out_anm: Path, tools, anmmap: Path):
    (tex_dir / "spec.txt").write_text(spec_text)
    if out_anm.exists():
        out_anm.unlink()
    thanm(["-c", VERSION, out_anm, "spec.txt", "-m", anmmap], tex_dir, tools)


def verify_rebuilt(out_anm: Path, retail_text: str, retail_dir: Path, tools, anmmap: Path,
                   on_extract=None):
    """重建文件自检：原生 entry / 脚本块逐个一致、原生贴图逐张一致。→ (rebuilt_text, entries, scripts)。

    `on_extract(dir)`：`thanm -x` 解包出来的临时目录（含**我们追加的** entry 贴图）会被回调一次 ——
    给「图集成员逐像素比对」用（比对**从 anm 里回读出来的**画布，而不是我们刚写下的那张，才叫端到端）。
    """
    if not out_anm.is_file():
        raise SystemExit(f"没有 {out_anm}：先 make anm")
    text = thanm(["-l", VERSION, out_anm, "-m", anmmap], out_anm.parent, tools)
    r_e, r_s = parse_entries(retail_text), parse_scripts(retail_text)
    n_e, n_s = parse_entries(text), parse_scripts(text)
    if len(n_e) < len(r_e) or len(n_s) < len(r_s):
        raise SystemExit(f"{out_anm.name}: entry {len(n_e)}/{len(r_e)}、script {len(n_s)}/{len(r_s)}，比原生还少")
    for i, (a, b) in enumerate(zip(r_e, n_e)):
        if a["block"] != b["block"]:
            raise SystemExit(f"{out_anm.name}: 原生 entry{i} 的 spec 变了")
    for i, (a, b) in enumerate(zip(r_s, n_s)):
        if a["block"] != b["block"]:
            raise SystemExit(f"{out_anm.name}: 原生 script{i} 的 spec 变了")
    with tempfile.TemporaryDirectory(dir=out_anm.parent) as td:
        thanm(["-x", VERSION, out_anm], td, tools)
        bad = [e["name"] for e in r_e if not filecmp.cmp(Path(td) / e["name"], retail_dir / e["name"], shallow=False)]
        if bad:
            raise SystemExit(f"{out_anm.name}: 原生贴图被改动：{bad[:5]}{' …' if len(bad) > 5 else ''}")
        if on_extract is not None:
            on_extract(Path(td))
    return text, n_e, n_s


def fresh_dir(p: Path):
    if p.exists():
        shutil.rmtree(p)
    p.mkdir(parents=True)
    return p
