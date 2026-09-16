#!/usr/bin/env python3
"""往原生 ability.anm 追加贴图 entry 与特效脚本，重编到 native/build/ability.anm，并生成 native/anm_ids.h。

    python3 build_ability.py                # 构建 + 自检 + 写 anm_ids.h
    python3 build_ability.py --verify-only  # 只自检已有的 build/ability.anm（含图集逐像素比对）
    python3 build_ability.py --freeze-order N   # 重划「已定稿段」（见下）

输入：
    ability/entries/ORDER.txt   三种行（**只许 append，见「重排检测」**）：
      · 单图行   `NAME 源图(相对 assets/) [WxH]`  —— 一行 = 一个 entry = 一个 sprite；
                 不写尺寸 = 卡图（256×320，字段抄 abcard BLANK_max）；写 WxH = 整张贴图一个 sprite
      · 图集段   `ATLAS NAME 画布WxH[@列x行]` 开一段，段内一行一个成员：
                   `成员名 源图 @x,y,WxH`      ← 显式矩形（画布坐标）
                   `成员名 源图 [WxH]`         ← 网格段（段头带 @列x行）按行优先取第 k 格
                 ⇒ 整段编译成**一个 entry**（name = `ability/atlas/<NAME>.png`），成员拿**连续** sprite 号
      · 退役行   `REF 名字 目标名`             —— 只保 token 能解析（指向目标名的 sprite），**不占 entry**
    成员矩形 = **成员原图自己的像素尺寸**（不补边、不缩放）—— 依据见 `ATLAS-MERGE-AUDIT.md` §4.1 规则①：
    VM 的 `scale.x` 乘的是 **sprite 矩形宽**（`th18:0x481703`），矩形一变，脚本里的 `scale()` 就跟着变。
    `REF` 用于「美术暂时不进图集、但还有脚本在写这个占位」的名字（见 ORDER.txt 的「退役段」）。

输出：
    native/build/ability.anm、native/anm_ids.h（`CE_ANM_ABILITY_SPRITE_<NAME>` /
    `CE_ANM_ABILITY_ATLAS_<图集>` / `CE_ANM_ABILITY_SCRIPT_<name>`）

硬门（任一不满足就红着退出）：
    ① entry 总数 ≤ 256（贴图键 = `(槽号<<8) | entry 下标`，绑定只取低 8 位 ⇒ ≥256 会**绑错贴图**，
       `th18:0x486d38` / `th18:0x47e1a4`）；本项目要求**离 256 有明显余量**，给以后的新卡美术留位；
    ② 图集成员的矩形必须**恰好**等于原图像素尺寸（不补边）；
    ③ 每个图集成员在**从 anm 回读出来的**画布上按矩形裁出来，必须与原图**逐像素完全相同**；
    ④ 「重排检测」：`ORDER.frozen.txt` 里记下的已定稿行（A/B/C 段）不许挪动 / 删改，新增只能 append。
"""
import argparse
import hashlib
import re
import sys
from pathlib import Path

import anmlib as L

HERE, MOD = L.HERE, L.MOD
ENTRIES = HERE / "ability" / "entries" / "ORDER.txt"
FROZEN = HERE / "ability" / "entries" / "ORDER.frozen.txt"
SCRIPTS = HERE / "ability" / "scripts"
TEX = L.BUILD / "tex-ability"
OUT_ANM = L.BUILD / "ability.anm"
IDS_H = MOD / "native" / "anm_ids.h"
TEMPLATE = ("abcard", "ability/BLANK_max.png")
MAX_ENTRIES = 256                     # 贴图键低 8 位（一手：th18:0x486d38 / th18:0x47e1a4）
_FILE = re.compile(r"^(\d+)_([A-Za-z_][A-Za-z0-9_]*)\.anm\.txt$")
_ATLAS_DIR = "ability/atlas"


def src_path(rel: str):
    """ORDER 里的源图路径：默认相对 assets/；`local/` 开头的相对仓库根（原生贴图搬用——版权字节只在 local/，构建时取，不入库）。"""
    return (L.REPO / rel) if rel.startswith("local/") else (HERE / rel)


# ── 「重排检测」硬门 ────────────────────────────────────────────────────
def _frozen_read(path: Path):
    """→ ([(sha256, 原文)], 结构指纹|None)；文件不存在 → None（= 还没定稿，构建会提示先冻一次）。"""
    if not path.is_file():
        return None
    out, sig = [], None
    for line in path.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if not s:
            continue
        if s.startswith("#"):
            m = re.match(r"^#\s*结构指纹：([0-9a-f]{64})$", s)
            if m:
                sig = m.group(1)
            continue
        h, _, text = s.partition("  ")
        if len(h) != 64:
            raise SystemExit(f"{path.name} 格式不对（应为 `<sha256>  <原始行>`）：{s}")
        out.append((h, text.strip()))
    return out, sig


def check_frozen(order_lines, items, path: Path, allow_reorder: bool):
    """已定稿行（ORDER.txt 的**前 N 个数据行**）必须逐行、逐字节不变；新增只能 append 到末尾。

    为什么要有：`entry = 原生 entry 数 + 行序`、`sprite = 原生 sprite 数 + 行序`，**行序就是编号**。
    2026-09-13 之前已经因为「把 113 的美术搬到文件头」重排过一次（`AUDIT.md` §CC-2），
    那次是救火；合并成图集后（`ATLAS-MERGE-AUDIT.md`）**只许追加**，谁再挪一次就会把全库的号推走。
    两道检查：① 逐行 sha256（字面）；② 冻结段**结构指纹**（这些行被解释成什么 —— 防"字面没变、
    结构变了"，例如往图集段里插空行把段截断）。
    """
    frozen = _frozen_read(path)
    if frozen is None:
        print(f"⚠ 没有 {path.name}：跳过重排检测（跑 `--freeze-order N` 把当前 A/B/C 段冻下来）")
        return
    frozen, sig = frozen
    if allow_reorder:
        print(f"⚠ --allow-reorder：**跳过**重排检测（{path.name} 记了 {len(frozen)} 行）。事后请重新 --freeze-order")
        return
    if len(order_lines) < len(frozen):
        raise SystemExit(_reorder_msg(path, len(frozen), None, None,
                                      f"ORDER.txt 只剩 {len(order_lines)} 个数据行，比冻结的 {len(frozen)} 行还少"))
    for i, (h, text) in enumerate(frozen):
        no, got = order_lines[i]
        if hashlib.sha256(got.encode("utf-8")).hexdigest() == h:
            continue
        raise SystemExit(_reorder_msg(path, len(frozen), i + 1, (text, got),
                                      f"ORDER.txt:{no} 与冻结的第 {i + 1} 行不一致"))
    # 结构指纹：只覆盖冻结段（前 N 个数据行所解释出的 item）
    prefix = _prefix_items(items, len(frozen))
    got_sig = L.order_signature(prefix)
    if sig and got_sig != sig:
        raise SystemExit(_reorder_msg(path, len(frozen), None, None,
                                      "冻结段的**结构指纹**不符（字面行没变，但被解释成的结构变了 —— "
                                      "典型原因：往图集段里插了空行、或把成员行写到了段外）\n"
                                      f"    冻结 {sig}\n    现在 {got_sig}"))
    print(f"重排检测：已定稿的前 {len(frozen)} 个数据行（A/B/C 段）逐行未变 ✅"
          f"（含结构指纹；其余 {len(order_lines) - len(frozen)} 行是后加的）")


def _prefix_items(items, n):
    """取出「前 n 个数据行」覆盖到的 item（冻结边界必须落在条目边界上）。"""
    out, seen = [], 0
    for it in items:
        out.append(it)
        seen += 1 if it["kind"] == "ref" else (len(it["members"]) + 1 if it["kind"] == "atlas" else 1)
        if seen >= n:
            break
    if seen != n:
        raise SystemExit(f"冻结段边界（{n} 个数据行）没落在条目边界上（实际 {seen}）："
                         f"冻结段必须是整段（改 `--freeze-order N`）")
    return out


def _reorder_msg(path: Path, n_frozen: int, idx, diff, head: str) -> str:
    out = [f"✖ 重排检测：{head}", f"  （{path.name} 冻结了前 {n_frozen} 个数据行；行序 = entry/sprite 编号，动了就是全库平移）"]
    if diff:
        out += [f"    冻结的那行：{diff[0]}", f"    ORDER 里的  ：{diff[1]}"]
    out += ["  怎么办（三选一）：",
            "    ① 要给某张卡换美术 / 加图：**别动老行**，在 ORDER.txt **末尾追加**新的图集段或单图行；",
            "    ② 确实要重排（例如第一次搬迁）：`python3 assets/build_ability.py --allow-reorder`，"
            "完了重新 `--freeze-order N` 并登记到 AUDIT.md §BE；",
            "    ③ 只是想把新增行插在中间：不行 —— 插中间等于把它后面的号全部推走，请移到文件末尾。",
            f"  ⚠ 改完必须**同时**重建 DLL 与 ability.anm 再发布（见 AUDIT.md §BE / local/DLL-SYNC.md）。"]
    return "\n".join(out)


def freeze_order(order_lines, items, path: Path, n: int):
    """把前 `n` 个数据行写进 ORDER.frozen.txt（重划定稿段 —— 人工动作，要登记）。"""
    if n <= 0 or n > len(order_lines):
        raise SystemExit(f"--freeze-order {n} 越界（ORDER.txt 现在 {len(order_lines)} 个数据行）")
    sig = L.order_signature(_prefix_items(items, n))
    head = ["# ORDER.txt「已定稿段」的逐行指纹 —— 由 `python3 assets/build_ability.py --freeze-order N` 生成。",
            "# 语义：ORDER.txt 的**前 N 个数据行**（注释/空行不算）不许挪动、删改；新增美术只能 append 到文件末尾。",
            "# 为什么：entry = 原生 entry 数 + 行序、sprite = 原生 sprite 数 + 行序 ⇒ **行序就是编号**，",
            "#        挪一行 = 全库 sprite 号平移；`build_ability.py` 每次构建都会逐行核对这份清单。",
            "# 依据：`ATLAS-MERGE-AUDIT.md` §4.5（A 共享 HUD / B 别人的卡 / C 已定稿 114–117 三段冻结；D 段 106–113 待定）。",
            "# 重划：`--freeze-order N`（跑之前先 `--allow-reorder` 构建过、并在 AUDIT.md §BE 登记）。",
            "# 另有一道**结构指纹**：同样这 N 行若被解释成别的结构（例如图集段里插了空行 ⇒ 段被截断），",
            "# 逐行 sha256 是看不出来的，指纹能看出来。",
            f"# 冻结行数：{n}",
            f"# 结构指纹：{sig}", ""]
    for _no, text in order_lines[:n]:
        head.append(f"{hashlib.sha256(text.encode('utf-8')).hexdigest()}  {text}")
    path.write_text("\n".join(head) + "\n", encoding="utf-8")
    print(f"已写 {path.relative_to(MOD)}：冻结 ORDER.txt 的前 {n} 个数据行（结构指纹 {sig[:12]}…）")


# ── 计划 ──────────────────────────────────────────────────────────────
def plan(retail_text: str, items):
    """→ (entries, scripts, names)。`entries` 里每项都带自己的 sprite 表（单图 1 个 / 图集 N 个）。"""
    n_entry = len(L.parse_entries(retail_text))
    n_script = len(L.parse_scripts(retail_text))
    sprite0 = L.max_sprite_id(retail_text) + 1
    entries, names = [], {}
    eidx, sidx = n_entry, sprite0

    def _bind(name, sid, where):
        if name in names:
            raise SystemExit(f"{where}：名字 {name} 与已定义的（sprite{names[name]}）撞名")
        names[name] = sid

    for it in items:
        if it["kind"] == "single":
            size = it["size"]
            _bind(it["name"], sidx, f"ORDER.txt:{it['line_no']}")
            entries.append({"kind": "single", "name": it["name"], "src": src_path(it["src"]),
                            "size": size, "eidx": eidx,
                            "members": [{"name": it["name"], "sid": sidx, "src": src_path(it["src"]),
                                         "rect": None, "line_no": it["line_no"]}]})
            eidx, sidx = eidx + 1, sidx + 1
        elif it["kind"] == "atlas":
            members = []
            for k, mem in enumerate(it["members"]):
                x, y, w, h = mem["rect"]
                _bind(mem["name"], sidx, f"ORDER.txt:{mem['line_no']}")
                alias = f"{it['name']}_{k}"
                if alias != mem["name"]:
                    _bind(alias, sidx, f"ORDER.txt:{it['line_no']}（图集 {it['name']} 的序号别名）")
                members.append({"name": mem["name"], "sid": sidx, "src": src_path(mem["src"]),
                                "rect": (x, y, w, h), "line_no": mem["line_no"]})
                sidx += 1
            # `图集名` 本身 = 第 0 个成员（向后兼容「一行一个 entry」的 `@NAME` 用法）
            _bind(it["name"], members[0]["sid"], f"ORDER.txt:{it['line_no']}（图集名 = 第 0 个成员）")
            entries.append({"kind": "atlas", "name": it["name"], "src": None,
                            "canvas": it["canvas"], "grid": it["grid"], "eidx": eidx,
                            "line_no": it["line_no"], "members": members})
            eidx += 1
        else:                                              # REF：退役名 → 已有 sprite
            if it["target"] not in names:
                raise SystemExit(f"ORDER.txt:{it['line_no']} REF {it['name']} 的目标 {it['target']} "
                                 f"还没定义（目标必须写在 REF **之前**）")
            _bind(it["name"], names[it["target"]], f"ORDER.txt:{it['line_no']}（REF）")
    scripts = []
    # 按**编号数值**排（不是文件名字典序）：脚本号过 99 之后 "100_…" 的字典序会排到 "68_…" 前面
    for k, p in enumerate(sorted(SCRIPTS.glob("*.anm.txt"),
                                 key=lambda q: (int(m.group(1)) if (m := _FILE.match(q.name)) else -1, q.name))):
        m = _FILE.match(p.name)
        if not m:
            raise SystemExit(f"{p.name}：文件名要像 68_reverse_flash.anm.txt")
        want = n_script + k
        if int(m.group(1)) != want:
            raise SystemExit(f"{p.name}：编号应为 {want}（原生 {n_script} 个脚本，按文件名顺序连续追加）")
        scripts.append((m.group(2), p, want))
    return entries, scripts, names


def template_for(w: int, h: int, base: dict):
    """`WxH` → 一个 entry 的字段模板（字段顺序 / 常量抄 base = abcard BLANK_max）。

    图集与单图共用：图集把 `WxH` 填**画布**尺寸，sprite 表由成员给（`make_entry(sprites=…)`）。
    ★ 2026-09-13：**2 的幂这条限制取消了**（原本是我们自己加的硬门，不是引擎要求）。
      一手依据（`engine/anm/th18/06-entry-limit-and-multi-sprite.md` §3）：
      建贴图就是 `D3DXCreateTexture(device, entry 头的 w/h, …)`（`th18:0x486488`），**零尺寸检查**；
      内嵌图像（THTX）用 `D3DXLoadSurfaceFromMemory` **1:1 拷到画布左上角**（`0x486515`）。
      原生例：`help.anm entry1` = **768×1024** 的贴图；`text.anm` 是 640×480（运行时按屏幕 1280×960）；
      354/591 个原生 entry 的**内嵌图像**本身就非 2 次幂（`abitem.png` 40×44 等）。
      ⚠ 旧注释举的 `pl01b2 = 96×256` 是**内嵌图像**尺寸（画布 128×256），量纲不对 —— 结论仍成立，证据换成上面几条。
    ⚠⚠ 另一条**硬上限**（2026-09-13 一手）：贴图键 = `(anm 槽号 << 8) | entry 下标`，绑贴图时只取低 8 位
      （解析器 `th18:0x486d38` / 绑定 `th18:0x47e1a4`）⇒ **一个 anm 文件最多 256 个 entry（下标 0..255）**，
      ≥256 会**回绕绑错贴图**（画出来是别人的图）。⇒ 多张小图并进**一个 entry**（引擎完全支持：
      `bullet.anm entry0` 一个 entry 就有 368 个 sprite）；`main()` 里有 `MAX_ENTRIES` 硬门 + 余量打印。
      为什么要放开 2 的幂：**VM 的 `scale.x` 乘的是 sprite 矩形宽**（`th18:0x481703`：`width × scale.x`），
      把 48×32 的原图补进 64×64 画布再缩放 ⇒ 内容会"一边横向伸缩一边位移"
      （用户 2026-09-13 实跑：左下角指示槽的延展不对）。**按原生尺寸切**，缩放行为才和原版一致。
    """
    if w <= 0 or h <= 0 or w > 4096 or h > 4096:
        raise SystemExit(f"贴图尺寸不合理：{w}x{h}")
    fields = dict(base["fields"])
    fields.update({"width": str(w), "height": str(h), "THTXSize": str(w * h * 4), "THTXWidth": str(w), "THTXHeight": str(h)})
    return {"idx": -1, "name": None, "keys": base["keys"], "fields": fields,
            "sprite": {"x": "0", "y": "0", "w": str(w), "h": str(h)}}


def load_png(p: Path, what: str):
    from PIL import Image
    if not p.is_file():
        raise SystemExit(f"缺 {p}" + ("（原生贴图：先 python3 tooling/thtk/unpack.py th18.v1.00a 解包到 local/）"
                                      if "local/" in str(p) else "") + f"（{what}）")
    return Image.open(p).convert("RGBA")


def render_entry(entry, tpl_of):
    """按计划把该 entry 的**画布**画出来（单图 = 原图/居中补边；图集 = 成员贴到各自矩形）。

    ★ 单图路径**照旧**（别人的卡全是单图 entry，行为必须逐字节不变：声明比原图大就**居中补透明**）。
    ★ 图集路径**不许补边**：成员原图尺寸必须**恰好**等于声明的矩形（规则①）；不等就直接报错并指路
      —— 要补边请在**美术生成器**里补（例：`make_font2_art.py` 把 16×20 的字形居中补进 16×32，
      因为 `native/sdk.c:690` 的 `+5.0f * sy * dat` 是按「32 高的矩形里 20 行字形居中」标定的）。
    """
    from PIL import Image
    if entry["kind"] == "single":
        t = tpl_of[entry["name"]]
        want = (int(t["fields"]["THTXWidth"]), int(t["fields"]["THTXHeight"]))
        im = load_png(entry["src"], entry["name"])
        if im.size != want:
            if im.width > want[0] or im.height > want[1]:
                raise SystemExit(f"{entry['src'].name} 是 {im.size}，要 ≤ {want}")
            pad = Image.new("RGBA", want, (0, 0, 0, 0))
            pad.paste(im, ((want[0] - im.width) // 2, (want[1] - im.height) // 2))
            im = pad
        return im
    canvas = Image.new("RGBA", entry["canvas"], (0, 0, 0, 0))
    for m in entry["members"]:
        x, y, w, h = m["rect"]
        im = load_png(m["src"], f"{entry['name']} 的成员 {m['name']}")
        if im.size != (w, h):
            raise SystemExit(
                f"图集 {entry['name']} 的成员 {m['name']}：原图 {im.size} ≠ 声明矩形 {w}x{h}"
                f"（ORDER.txt:{m['line_no']}）。\n"
                "  规则①（`ATLAS-MERGE-AUDIT.md` §4.1）：**矩形 = 成员原图自己的像素尺寸**，不补边 ——\n"
                "  VM 的 scale.x 乘的是 sprite 矩形宽（`th18:0x481703`），补边会让缩放既缩又移。\n"
                "  要么把 ORDER 的矩形改成原图尺寸，要么在**美术生成器**里把图补成矩形那么大"
                "（例：make_font2_art.py 的 16×20 → 16×32 居中）。")
        canvas.paste(im, (x, y))
    # 贴完立刻就地裁回来对一遍（比等 anm 回读更早暴露贴错位置）
    for m in entry["members"]:
        x, y, w, h = m["rect"]
        if canvas.crop((x, y, x + w, y + h)).tobytes() != load_png(m["src"], m["name"]).tobytes():
            raise SystemExit(f"图集 {entry['name']}：成员 {m['name']} 贴进 ({x},{y},{w},{h}) 后裁回来与原图不一致")
    return canvas


def check_entry_pixels(entries, extracted: Path, tpl_of):
    """端到端逐像素自检：**从 anm 里回读出来的**贴图 vs 我们打算写进去的画布 + 图集成员 vs 原图。"""
    from PIL import Image
    n_atlas = n_mem = 0
    for e in entries:
        rel = f"{_ATLAS_DIR}/{e['name']}.png" if e["kind"] == "atlas" else f"ability/{e['name']}.png"
        p = extracted / rel
        if not p.is_file():
            raise SystemExit(f"回读失败：{rel} 不在 `thanm -x` 的输出里（entry{e['eidx']}）")
        got = Image.open(p).convert("RGBA")
        want = render_entry(e, tpl_of)
        if got.size != want.size or got.tobytes() != want.tobytes():
            raise SystemExit(f"entry{e['eidx']}（{e['name']}）：回读出来的贴图与计划不符"
                             f"（{got.size} vs {want.size}；像素{'相同' if got.tobytes() == want.tobytes() else '不同'}）")
        if e["kind"] != "atlas":
            continue
        n_atlas += 1
        for m in e["members"]:
            x, y, w, h = m["rect"]
            crop = got.crop((x, y, x + w, y + h))
            src = load_png(m["src"], m["name"])
            if crop.tobytes() != src.tobytes():
                raise SystemExit(f"图集 {e['name']} 的成员 {m['name']}：从 anm 回读的画布上裁 ({x},{y},{w},{h}) "
                                 f"与原图 {m['src'].name} **逐像素不一致**（这就是本次迁移的核心不变式）")
            n_mem += 1
    return n_atlas, n_mem


def write_ids_h(items, entries, scripts, names):
    """逐名生成宏：成员名 / 图集名（= 第 0 个成员）/ 图集序号别名 `图集名_k` / 退役名 / 脚本名。"""
    lines = ["/* 由 assets/build_ability.py 生成 —— ability.anm 里追加的 sprite / 脚本号。别手改。 */",
             "/* 图集（一个 entry 多个 sprite）里：成员名 → 自己的 sprite 号；`图集名_k` → 段内第 k 个成员；",
             "   `图集名` → 第 0 个成员（兼容老的单图写法）；`CE_ANM_ABILITY_ATLAS_*` = entry 号（调试用）。*/",
             "#pragma once", ""]
    by_name = {e["name"]: e for e in entries}
    refs = {}
    for it in items:
        if it["kind"] == "ref":
            refs[it["name"]] = it["target"]
    for it in items:
        if it["kind"] == "ref":
            continue
        e = by_name[it["name"]]
        if it["kind"] == "single":
            m = e["members"][0]
            lines.append(f"#define CE_ANM_ABILITY_SPRITE_{m['name'].upper():<20} {m['sid']:>4}   /* entry{e['eidx']} */")
            continue
        lines.append(f"#define CE_ANM_ABILITY_ATLAS_{it['name'].upper():<21} {e['eidx']:>4}   /* 画布 "
                     f"{e['canvas'][0]}x{e['canvas'][1]}，{len(e['members'])} 个 sprite */")
        for k, m in enumerate(e["members"]):
            lines.append(f"#define CE_ANM_ABILITY_SPRITE_{m['name'].upper():<20} {m['sid']:>4}   /* entry{e['eidx']} "
                         f"{it['name']}[{k}] */")
        for k, m in enumerate(e["members"]):
            alias = f"{it['name']}_{k}"
            if alias != m["name"]:
                lines.append(f"#define CE_ANM_ABILITY_SPRITE_{alias.upper():<20} {m['sid']:>4}   /* = "
                             f"{m['name']} */")
    for name, target in sorted(refs.items()):
        lines.append(f"#define CE_ANM_ABILITY_SPRITE_{name.upper():<20} {names[name]:>4}   /* 退役名 → {target}"
                     f"（美术暂不进图集，见 ORDER.txt 退役段）*/")
    for name, _, idx in scripts:
        lines.append(f"#define CE_ANM_ABILITY_SCRIPT_{name.upper():<20} {idx:>4}")
    IDS_H.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--verify-only", action="store_true")
    ap.add_argument("--allow-reorder", action="store_true",
                    help="跳过「重排检测」（改完 ORDER 的老行才需要；跑完要重新 --freeze-order）")
    ap.add_argument("--freeze-order", type=int, metavar="N",
                    help="把 ORDER.txt 的前 N 个数据行写成 ORDER.frozen.txt（重划定稿段）")
    a = ap.parse_args()
    items, order_lines = L.read_order(ENTRIES)
    if a.freeze_order:
        freeze_order(order_lines, items, FROZEN, a.freeze_order)
        return 0
    check_frozen(order_lines, items, FROZEN, a.allow_reorder)

    th, td, mapdir = L.find_tools()
    tools, anmmap = (th, td), mapdir / "v8.anmm"
    retail_dir, retail_text = L.retail("ability")
    retail_entries = L.parse_entries(retail_text)
    tpl = {e["name"]: e for e in L.parse_entries(L.retail(TEMPLATE[0])[1])}[TEMPLATE[1]]
    entries, scripts, names = plan(retail_text, items)

    n_total = len(retail_entries) + len(entries)
    if n_total > MAX_ENTRIES:
        raise SystemExit(f"entry 总数 {n_total} > {MAX_ENTRIES}：贴图键低 8 位放不下，≥256 的 entry 会**绑错贴图**"
                         f"（`th18:0x486d38` 打包 / `th18:0x47e1a4` 解包）。\n"
                         f"  改法：把多张小图并进**一个 entry**（图集段 `ATLAS …`），或拆到另一个 anm 槽。")
    tpl_of = {}
    for e in entries:
        if e["kind"] == "single":
            tpl_of[e["name"]] = template_for(*e["size"], tpl) if e["size"] else tpl
        else:
            tpl_of[e["name"]] = template_for(*e["canvas"], tpl)
    for e in entries:                                   # 单图 entry 的矩形 = 模板矩形的左上角起、THTX 那么大
        if e["kind"] == "single":
            t = tpl_of[e["name"]]
            e["members"][0]["rect"] = (0, 0, int(t["fields"]["THTXWidth"]), int(t["fields"]["THTXHeight"]))
    mapping = {name: f"sprite{sid}" for name, sid in names.items()}
    # ★ 2026-09-12 追加：脚本**按名字**互相引用 —— `scriptNew(@SCRIPT_<ID>@)`。
    #   `<ID>` = 脚本文件名 `NN_<id>.anm.txt` 的 `<id>` 大写，与 `anm_ids.h` 的
    #   `CE_ANM_ABILITY_SCRIPT_<ID>` **同号同名**（同一份 plan 算出来的 idx）。
    #   为什么要有：一天之内同类脚本重编号过两次（174→…、200→186），父脚本里的 `scriptNew(175)`
    #   这种字面量会**静默指错**（子脚本不再生成、只在画面上少一块，不报错）。
    #   纯追加：`mapping` 只是多了 `SCRIPT_*` 这些键；既有脚本一个都没用这个前缀 ⇒ 对其它卡零影响。
    for _n, _p, _i in scripts:
        _k = f"SCRIPT_{_n.upper()}"
        if _k in mapping:
            raise SystemExit(f"脚本名 {_k} 与 ORDER 里的贴图名撞名：{mapping[_k]}")
        mapping[_k] = str(_i)

    if not a.verify_only:
        L.fresh_dir(TEX)
        L.link_retail_textures(TEX, retail_dir, retail_entries)
        (TEX / _ATLAS_DIR).mkdir(parents=True, exist_ok=True)
        blocks = []
        for e in entries:
            im = render_entry(e, tpl_of)
            if e["kind"] == "atlas":
                rel = f"{_ATLAS_DIR}/{e['name']}.png"
            else:
                rel = f"ability/{e['name']}.png"
            im.save(TEX / rel)
            sprites = ([(m["sid"], *m["rect"]) for m in e["members"]] if e["kind"] == "atlas" else None)
            blocks.append(L.make_entry(tpl_of[e["name"]], e["eidx"], rel,
                                       sprite_idx=(None if sprites else e["members"][0]["sid"]),
                                       sprites=sprites))
        text = L.insert_entries(retail_text, blocks) if blocks else retail_text
        sblocks = []
        for name, p, idx in scripts:
            body = L.substitute(p.read_text(encoding="utf-8"), mapping)
            if not body.lstrip().startswith(f"script script{idx} {{"):
                raise SystemExit(f"{p.name}：开头应为 `script script{idx} {{`")
            sblocks.append(body)
        if sblocks:
            text = L.append_scripts(text, sblocks)
        L.compile_anm(text, TEX, OUT_ANM, tools, anmmap)
        print(f"build: {OUT_ANM.relative_to(MOD)}（{OUT_ANM.stat().st_size:,} B）；原生 entry {len(retail_entries)}"
              f" + {len(entries)} = {n_total}，script {len(L.parse_scripts(retail_text))} + {len(scripts)}"
              f" = {len(L.parse_scripts(retail_text)) + len(scripts)}")

    seen = {}

    def _pixels(td):
        seen["n_atlas"], seen["n_mem"] = check_entry_pixels(entries, td, tpl_of)

    _, n_e, n_s = L.verify_rebuilt(OUT_ANM, retail_text, retail_dir, tools, anmmap, on_extract=_pixels)
    if len(n_e) != n_total or len(n_s) != len(L.parse_scripts(retail_text)) + len(scripts):
        raise SystemExit(f"entry {len(n_e)} / script {len(n_s)}，与计划不符")
    for e in entries:                                   # 字段 / 名字 / 每个 sprite 的矩形逐条核对
        got, t = n_e[e["eidx"]], tpl_of[e["name"]]
        want_name = (f"{_ATLAS_DIR}/{e['name']}.png" if e["kind"] == "atlas" else f"ability/{e['name']}.png")
        if got["name"] != want_name or got["fields"] != t["fields"]:
            raise SystemExit(f"新 entry{e['eidx']}（{e['name']}）字段与模板不一致")
        want = {m["sid"]: tuple(m["rect"]) for m in e["members"]}
        rects = L.parse_sprite_rects(got["block"])
        if rects != want:
            raise SystemExit(f"新 entry{e['eidx']}（{e['name']}）的 sprite 表与计划不符："
                             f"{sorted(rects.items())} vs {sorted(want.items())}")
    write_ids_h(items, entries, scripts, names)
    n_sprite = sum(e["n_sprites"] for e in n_e)
    n_ref = sum(1 for it in items if it["kind"] == "ref")
    room = MAX_ENTRIES - n_total
    print(f"verify: entry {n_total}、sprite {n_sprite}、script {len(n_s)}；原生 spec/贴图逐项一致；"
          f"写 {IDS_H.relative_to(MOD)}")
    print(f"  ★ 图集逐像素自检：{seen['n_atlas']} 个图集 / {seen['n_mem']} 个成员，"
          f"从 anm 回读的画布上按矩形裁出来与原 PNG **完全相同** ✅")
    print(f"  ★ entry 余量：用到 {n_total} / 上限 {MAX_ENTRIES}（低 8 位）⇒ **还剩 {room} 个 entry**"
          f"（给以后的新卡美术；见 ATLAS-MERGE-AUDIT.md §9）")
    for e in entries:
        if e["kind"] == "atlas":
            print(f"  entry{e['eidx']:<4} {e['name']:<20} 画布 {e['canvas'][0]}x{e['canvas'][1]:<4} "
                  f"sprite{e['members'][0]['sid']}…{e['members'][-1]['sid']}（{len(e['members'])} 个成员）")
        else:
            print(f"  entry{e['eidx']:<4} sprite{e['members'][0]['sid']:<5} {e['name']}"
                  f"{'  ' + str(e['size'][0]) + 'x' + str(e['size'][1]) if e['size'] else ''}")
    print(f"  （另有 {n_ref} 个退役名不占 entry；{len(scripts)} 个脚本）")
    for name, _, idx in scripts:
        print(f"  script{idx:<12} {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
