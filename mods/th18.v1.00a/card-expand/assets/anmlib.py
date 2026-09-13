"""thanm spec 的读写与重建（build_abcard.py / build_ability.py 共用）。

原生 spec / 贴图来自 tooling/thtk/unpack.py 解出的 local/th18.v1.00a/anm/<名>/。
约定：只**追加** entry / 脚本，绝不改原生的；重建后逐项校验原生部分没变。
"""
import filecmp
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
    """→ [{idx, name, keys, fields, sprite, block, span}]；fields 不含 name，keys 记字段顺序（含 name）。
    只处理一 entry 一 sprite 的文件（abcard 全是；ability 的多 sprite entry 用 parse_entries_loose）。"""
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
def make_entry(template: dict, idx: int, png_rel: str, sprite_idx: int = None) -> str:
    """照抄模板字段，只换 name、entry 号与 sprite 号（sprite 号默认 = entry 号，abcard 的约定）。"""
    if template["sprite"] is None:
        raise SystemExit(f"模板 entry{template['idx']} 不是单 sprite，不能当模板")
    sid = idx if sprite_idx is None else sprite_idx
    lines = [f"entry entry{idx} {{"]
    for k in template["keys"]:
        v = f'"{png_rel}"' if k == "name" else template["fields"][k]
        lines.append(f"    {k}: {v},")
    s = template["sprite"]
    lines += ["    sprites: {",
              f"        sprite{sid}: {{ x: {s['x']}, y: {s['y']}, w: {s['w']}, h: {s['h']} }}",
              "    }", "}", ""]
    return "\n".join(lines)


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


def read_order(path: Path):
    """ORDER.txt：一行一个条目（空白分隔），跳过 # 与空行；名字（首列）不许重复。"""
    if not path.is_file():
        raise SystemExit(f"没有 {path}")
    rows = [l.split() for l in path.read_text().splitlines() if l.strip() and not l.lstrip().startswith("#")]
    names = [r[0] for r in rows]
    dup = {n for n in names if names.count(n) > 1}
    if dup:
        raise SystemExit(f"{path.name} 有重复：{sorted(dup)}")
    return rows


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


def verify_rebuilt(out_anm: Path, retail_text: str, retail_dir: Path, tools, anmmap: Path):
    """重建文件自检：原生 entry / 脚本块逐个一致、原生贴图逐张一致。→ (rebuilt_text, entries, scripts)。"""
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
    return text, n_e, n_s


def fresh_dir(p: Path):
    if p.exists():
        shutil.rmtree(p)
    p.mkdir(parents=True)
    return p
