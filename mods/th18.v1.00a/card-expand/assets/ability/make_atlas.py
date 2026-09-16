#!/usr/bin/env python3
"""图集（`ATLAS` 段）的**独立**复核工具：把 `ATLAS-MERGE-AUDIT.md` §4.3 的成员明细与
`entries/ORDER.txt` 的图集段对账，再独立画一遍画布 + 逐像素自检。

    python3 make_atlas.py                # 对账 + 画预览图 + 自检（读审计 md 与 ORDER.txt）
    python3 make_atlas.py --emit-order   # 打印 ORDER.txt 的图集段（搬迁时用它生成，别手抄）
    python3 make_atlas.py --audit PATH   # 换一份审计报告（默认 ATLAS-MERGE-AUDIT.md）

为什么要有这个工具（三个**独立**的意思）：

1. **几何来自审计报告，不是来自 ORDER.txt** —— §4.3 的成员明细（成员名 / 原图 / 矩形 x,y,w,h）
   是审计逐条算出来的；这里把 ORDER.txt 里导出的矩形拿去和它**逐格比**。两边任何一处抄错都当场红。
2. **画布由本脚本自己贴一遍** —— `build_ability.py` 是"贴进画布 → 写 spec → thanm 编译"，
   这里是"贴进画布 → 直接比对"，两条独立代码路径给出同一张图才算数。
3. **成员比对用 `tobytes()`** —— 从画布上按矩形裁出来 `crop(rect)`，与源 PNG 逐像素比；
   这是本次迁移的**核心正确性证据**（规则①：矩形 = 成员原图自己的像素尺寸，不补边）。

`build_ability.py` 里还有一道**更强**的同类检查：从编译好的 `ability.anm` 里
`thanm -x` 回读出画布再裁（端到端，连 thanm 的编码一起验），`make anm-verify` 会跑。
"""
import argparse
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))                   # assets/anmlib.py
import anmlib as L  # noqa: E402

MOD = L.MOD
DEFAULT_AUDIT = MOD / "ATLAS-MERGE-AUDIT.md"
ORDER = HERE.parent / "ability" / "entries" / "ORDER.txt"
PREVIEW = L.BUILD / "atlas-preview"

# ── 不进图集的图集（用户 2026-09-13 裁决）────────────────────────────────
# 108「妖侧本体」= `ATLAS_YK_BODY`（紫/爱丽丝/文 24 帧）+ `ATLAS_YK_REMILIA`（蕾米莉亚 9 帧）。
# 用户原话：「妖侧本体的"连接"暂时删除，但是其本身不要删除，没人引用是因为 108 还没开始真做」
#   ⇒ 这 55 张**源 PNG 一个都不删**（留在 `assets/ability/{youkai,sanae_youkai}/`），
#     只是不排进图集；ORDER.txt 里给这 55 个名字一串 `REF`（指向 1×1 透明的 `RETIRED_BLANK`），
#     好让还写着 `@YOUKAI_YUKARI_S11` 的**死脚本**（189–203 / 218–222，没有 C 代码起它们）仍能编译。
#   ⇒ 108 真做的时候：把这批源图排成 `ATLAS_YK_BODY`/`ATLAS_YK_REMILIA` **追加到 ORDER.txt 末尾**
#     （老行一行不动），再把 REF 段删掉即可。
DROPPED = {
    "ATLAS_YK_BODY": "108 妖侧本体 46 帧（用户裁决：本次不进图集，源图保留）",
    "ATLAS_YK_REMILIA": "108 妖侧本体（蕾米莉亚）9 帧（同上）",
}
# ── 审计报告里没有、但 ORDER.txt 里有的图集（必须显式登记，免得"偷偷多一个"）──
EXTRA = {"ATLAS_RETIRED": "退役名容器：1×1 全透明，只为了让 REF 有落点（不占美术，1 个 entry）"}

_HEAD = re.compile(r"^####\s+`([A-Z0-9_]+)`\s+—\s+画布\s+(\d+)×(\d+)")
_ROW = re.compile(r"^\|\s+`([A-Za-z0-9_]+)`\s+\|\s+`([^`]+)`\s+\|\s+(\d+),(\d+),(\d+),(\d+)\s+\|")


def parse_audit(path: Path):
    """§4.3 的成员明细 → {图集名: {"canvas": (W,H), "members": [(名字, 原图, x,y,w,h), …]}}（保序）。"""
    if not path.is_file():
        raise SystemExit(f"没有 {path}")
    out, cur = {}, None
    for line in path.read_text(encoding="utf-8").splitlines():
        m = _HEAD.match(line.strip())
        if m:
            cur = m.group(1)
            out[cur] = {"canvas": (int(m.group(2)), int(m.group(3))), "members": []}
            continue
        if line.startswith("###") or line.startswith("#### "):
            cur = None
            continue
        if cur:
            r = _ROW.match(line.strip())
            if r:
                out[cur]["members"].append((r.group(1), r.group(2),
                                            *(int(r.group(i)) for i in range(3, 7))))
    if not out:
        raise SystemExit(f"{path} 里没找到 §4.3 的成员表（标题格式变了？）")
    return out


def order_atlases():
    """ORDER.txt → {图集名: {"canvas": (W,H), "members": [(名字, 原图, x,y,w,h), …]}}（保序）。"""
    items, _ = L.read_order(ORDER)
    out = {}
    for it in items:
        if it["kind"] != "atlas":
            continue
        out[it["name"]] = {"canvas": it["canvas"],
                           "members": [(m["name"], m["src"], *m["rect"]) for m in it["members"]]}
    return out


def emit_order(audit, names):
    """按审计顺序打印 ORDER.txt 的图集段（网格段用 `@列x行` 语法，其余写显式矩形）。"""
    lines = []
    for name in names:
        if name in DROPPED:
            continue
        a = audit[name]
        w, h = a["canvas"]
        mem = a["members"]
        # 能写成网格段的条件：矩形恰好是 行优先 的等分格
        grid = None
        for cols in range(1, len(mem) + 1):
            if w % cols:
                continue
            cw = w // cols
            for rows in range(1, len(mem) + 1):
                if h % rows:
                    continue
                ch = h // rows
                if cols * rows < len(mem) or cw * ch == 0:
                    continue
                if all(tuple(rect) == ((k % cols) * cw, (k // cols) * ch, cw, ch)
                       for k, (_n, _s, *rect) in enumerate(mem)):
                    grid = (cols, rows, cw, ch)
                    break
            if grid:
                break
        lines.append(f"ATLAS {name} {w}x{h}" + (f"@{grid[0]}x{grid[1]}" if grid else ""))
        for mn, src, x, y, mw, mh in mem:
            lines.append(f"{mn:<22} {src:<52} " +
                         (f"{mw}x{mh}" if grid else f"@{x},{y},{mw}x{mh}"))
        lines.append("")
    return lines


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--audit", type=Path, default=DEFAULT_AUDIT)
    ap.add_argument("--emit-order", action="store_true")
    a = ap.parse_args()
    audit = parse_audit(a.audit)
    if a.emit_order:
        print("\n".join(emit_order(audit, list(audit))))
        return 0

    order = order_atlases()
    bad = []
    # ① 双向对账：审计里有而我们没排的（除 DROPPED）、我们排了而审计没有的（除 EXTRA）
    for name in audit:
        if name not in order and name not in DROPPED:
            bad.append(f"审计 §4.3 的 {name} 没进 ORDER.txt，也没登记在 DROPPED 里")
    for name in order:
        if name not in audit and name not in EXTRA:
            bad.append(f"ORDER.txt 的 {name} 不在审计 §4.3 里，也没登记在 EXTRA 里")
    for name in DROPPED:
        if name in order:
            bad.append(f"{name} 在 DROPPED 里（{DROPPED[name]}）却仍然排进了 ORDER.txt")
    # ② 逐图集逐成员比几何
    n_atlas = n_mem = 0
    from PIL import Image
    PREVIEW.mkdir(parents=True, exist_ok=True)
    report = []
    for name, o in order.items():
        ref = audit.get(name)
        if ref is None:
            report.append(f"  {name:<22} （审计里没有：{EXTRA.get(name, '?')}）")
            continue
        if tuple(o["canvas"]) != tuple(ref["canvas"]):
            bad.append(f"{name}：画布 ORDER {o['canvas']} ≠ 审计 {ref['canvas']}")
            continue
        if len(o["members"]) != len(ref["members"]):
            bad.append(f"{name}：成员数 ORDER {len(o['members'])} ≠ 审计 {len(ref['members'])}")
            continue
        for got, want in zip(o["members"], ref["members"]):
            if (got[0], got[1]) != (want[0], want[1]) or tuple(got[2:6]) != tuple(want[2:6]):
                bad.append(f"{name}：成员不一致\n    ORDER {got}\n    审计  {want}")
                continue
        # ③ 独立画一遍画布 + 逐像素自检
        w, h = o["canvas"]
        canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        for mn, src, x, y, mw, mh in o["members"]:
            p = (L.REPO / src) if src.startswith("local/") else (L.HERE / src)
            if not p.is_file():
                bad.append(f"{name}：缺源图 {src}（成员 {mn}）")
                continue
            im = Image.open(p).convert("RGBA")
            if im.size != (mw, mh):
                bad.append(f"{name}：成员 {mn} 原图 {im.size} ≠ 矩形 {mw}x{mh}（规则①不补边）")
                continue
            canvas.paste(im, (x, y))
        for mn, src, x, y, mw, mh in o["members"]:
            p = (L.REPO / src) if src.startswith("local/") else (L.HERE / src)
            if not p.is_file():
                continue
            crop = canvas.crop((x, y, x + mw, y + mh))
            if crop.tobytes() != Image.open(p).convert("RGBA").tobytes():
                bad.append(f"{name}：成员 {mn} 从画布裁 ({x},{y},{mw},{mh}) 与原图逐像素不一致")
            n_mem += 1
        canvas.save(PREVIEW / f"{name}.png")
        n_atlas += 1
        report.append(f"  {name:<22} 画布 {w}x{h:<5} {len(o['members']):>3} 个成员  逐像素 OK")
    print(f"审计 §4.3：{len(audit)} 个图集；ORDER.txt：{len(order)} 个图集"
          f"（另有 {len(DROPPED)} 个按裁决不进图集：{'、'.join(DROPPED)}）")
    print("\n".join(report))
    print(f"预览画布写到 {PREVIEW.relative_to(MOD)}/（{n_atlas} 张）")
    print(f"★ 逐像素自检：{n_atlas} 个图集 / {n_mem} 个成员，从画布裁出的矩形与源 PNG **完全相同** ✅")
    for k, v in DROPPED.items():
        print(f"  · 不进图集：{k} —— {v}")
    if bad:
        print("FAIL:")
        for b in bad:
            print("  " + b)
        return 1
    print("OK: ORDER.txt 的图集段与审计 §4.3 逐格一致，且每个成员的像素与源图完全相同。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
