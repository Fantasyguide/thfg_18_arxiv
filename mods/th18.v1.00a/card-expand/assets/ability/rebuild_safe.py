#!/usr/bin/env python3
"""安全重建 `native/build/ability.anm`：**先构建到临时件 → 过六道门 → 才允许覆盖**。

    python3 assets/ability/rebuild_safe.py                 # 构建 + 门禁（**不覆盖**，只报告）
    python3 assets/ability/rebuild_safe.py --install       # 门禁全过才 cp 成 native/build/ability.anm
    python3 assets/ability/rebuild_safe.py --check-only F  # 只对一个已有的 anm 跑门禁（自测用）
    python3 assets/ability/rebuild_safe.py --baseline F    # 换基线（默认见下）

为什么要有（2026-10-20 事故）：有人用**旧分支的树**重建了 ability.anm（15,771,536 B，缺两个
PvZ 植物图集 = 3.3 MB），覆盖发布件 ⇒ 所有卡图按错 entry 解释、**全部消失**。本脚本把
「重建件必须 ⊇ 发布件」写成硬门：**任何一条不过就拒绝覆盖**，并打印**哪条不过**。

基线（`--baseline`）优先级：
  1. 命令行给的；
  2. `native/build/ability.anm.release-20261020`（本仓备份的发布件，md5 96b24118… / 19,116,372 B）；
  3. `native/build/ability.anm`。
⚠ 基线**不要**指向会被本脚本覆盖的那个文件 —— 否则第二次跑就是「自己跟自己比」，门禁形同虚设。
   备份件在时永远优先用它。

六道门（全过才允许 --install）：
  G1 计数：entry / sprite / script 三项**都 ≥ 基线**；
  G2 内容：基线的**每一个 entry**（名字 + 宽高）都在新件里；
  G3 108：`ATLAS_YK_FAM` / `ATLAS_YK_OPT` 两个图集 entry 在；
  G4 109：`POFV109_CD00..CD15` 16 个 entry 在 + `pofv109_c4m_star` 脚本在（且它引用的 sprite 在新件里存在）；
  G5 标记：`raw_char` / `[C4M]` 标记仍在（它们**不在 anm 里** —— 是 DLL 侧
     `native/cards/pofv.c` 的日志；本门验"同一批的 DLL 源还在/没被改丢"，并明确打印出处）；
  G6 体积：新件 **≥** 基线（**不放宽** —— 这条是 3.3 MB 事故的兜底）。

退出码：0 = 门禁通过；2 = 有门不过（打印清单）；1 = 构建/解析失败。
"""
import argparse
import hashlib
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent                  # assets/ability/
ASSETS = HERE.parent                                     # assets/（anmlib.py / build_ability.py 在这层）
sys.path.insert(0, str(ASSETS))
import anmlib as L                                       # noqa: E402
import build_ability as B                                # noqa: E402

MOD = ASSETS.parent                                      # card-expand/
BUILD = MOD / "native" / "build"
TARGET = BUILD / "ability.anm"
BACKUP = BUILD / "ability.anm.release-20261020"
CAND = BUILD / "ability.anm.new"
IDS_H = MOD / "native" / "anm_ids.h"
DLL_SRC = MOD / "native" / "cards" / "pofv.c"

YK_ATLASES = ("ability/atlas/ATLAS_YK_FAM.png", "ability/atlas/ATLAS_YK_OPT.png")
CD_NAMES = [f"ability/POFV109_CD{i:02d}.png" for i in range(16)]
C4M_SCRIPT = "pofv109_c4m_star"                          # assets/ability/scripts/464_pofv109_c4m_star.anm.txt
C4M_SPRITE_NAME = "POFV109_C4M_STAR16"                   # 脚本 464 正文里的 sprite(@POFV109_C4M_STAR16)
MARKERS = ("raw_char", "[C4M]")


def label(p: Path) -> str:
    """仓内文件显示相对路径；仓外（/tmp 的候选件）显示绝对路径。"""
    try:
        return str(p.relative_to(MOD))
    except ValueError:
        return str(p)


def md5(p: Path) -> str:
    h = hashlib.md5()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def spec_of(anm: Path) -> str:
    th, td, mapdir = L.find_tools()
    return L.thanm(["-l", L.VERSION, anm, "-m", str(mapdir / "v8.anmm")], anm.parent, (th, td))


def parse(anm: Path):
    text = spec_of(anm)
    e = L.parse_entries(text)
    s = L.parse_scripts(text)
    return {"path": anm, "size": anm.stat().st_size, "md5": md5(anm),
            "entries": e, "scripts": s, "names": {x["name"]: x for x in e},
            "n_sprites": sum(x["n_sprites"] for x in e),
            "script_idx": {x["idx"]: x for x in s}}


def why_not(rel, cand):
    """逐门检查，返回 `[(门, 说明)]`；空 = 全过。"""
    bad = []

    # G1 计数
    for what, a, b in (("entry", len(cand["entries"]), len(rel["entries"])),
                       ("sprite", cand["n_sprites"], rel["n_sprites"]),
                       ("script", len(cand["scripts"]), len(rel["scripts"]))):
        if a < b:
            bad.append(("G1 计数", f"{what} {a} < 基线 {b}（少 {b - a}）"))

    # G2 基线每个 entry 的名字 + 尺寸
    missing, resized = [], []
    for e in rel["entries"]:
        g = cand["names"].get(e["name"])
        if g is None:
            missing.append(e["name"])
            continue
        for k in ("THTXWidth", "THTXHeight", "width", "height"):
            if k in e["fields"] and e["fields"].get(k) != g["fields"].get(k):
                resized.append(f'{e["name"]} 的 {k}: {g["fields"].get(k)} ≠ 基线 {e["fields"].get(k)}')
                break
    if missing:
        bad.append(("G2 内容", f"基线有、新件没有 {len(missing)} 个 entry：\n      " + "\n      ".join(missing)))
    if resized:
        bad.append(("G2 内容", "尺寸变了：\n      " + "\n      ".join(resized)))

    # G3 108 的两个图集
    for n in YK_ATLASES:
        if n not in cand["names"]:
            bad.append(("G3 108 图集", f"缺 {n}"))

    # G4 109 的 CD00..CD15 + 彩弹脚本
    lost = [n for n in CD_NAMES if n not in cand["names"]]
    if lost:
        bad.append(("G4 109 彩弹", f"缺 {len(lost)} 个：{', '.join(x.split('/')[-1] for x in lost)}"))
    srcs = sorted((HERE / "scripts").glob(f"*_{C4M_SCRIPT}.anm.txt"))
    if not srcs:
        bad.append(("G4 109 彩弹", f"源脚本 *_{C4M_SCRIPT}.anm.txt 不在 assets/ability/scripts/"))
    else:
        idx = int(srcs[0].name.split("_", 1)[0])
        s = cand["script_idx"].get(idx)
        if s is None:
            bad.append(("G4 109 彩弹", f"新件里没有 script{idx}（{C4M_SCRIPT}）"))
        else:
            spr = cand["names"].get(f"ability/{C4M_SPRITE_NAME}.png")
            if spr is None:
                bad.append(("G4 109 彩弹", f"缺 sprite 源 entry ability/{C4M_SPRITE_NAME}.png"))
            else:
                want = next(iter(L.parse_sprite_rects(spr["block"])), None)
                # thanm 的 spec 文本里是 `sprite(sprite770)`；宏/源码里也见过 `sprite(770)` 写法，两种都认
                hit = want is not None and (f"sprite(sprite{want})" in s["block"]
                                            or f"sprite({want})" in s["block"])
                if not hit:
                    bad.append(("G4 109 彩弹",
                                f"script{idx} 正文没引用 {C4M_SPRITE_NAME} 的 sprite（期望 sprite(sprite{want})）"))

    # G5 raw_char / [C4M] 标记（DLL 侧，不在 anm 里）
    where = []
    if DLL_SRC.is_file():
        lines = DLL_SRC.read_text(encoding="utf-8", errors="replace").splitlines()
        for m in MARKERS:
            hit = [i + 1 for i, ln in enumerate(lines) if m in ln]
            (where.append(f"{m}@{DLL_SRC.relative_to(MOD)}:{hit[0]}") if hit else
             bad.append(("G5 标记", f"{m} 不在 {DLL_SRC.relative_to(MOD)}（DLL 源被改丢？）")))
    else:
        bad.append(("G5 标记", f"没有 {DLL_SRC.relative_to(MOD)}：无法确认 raw_char / [C4M] 标记"))

    # G6 体积（不放宽）
    if cand["size"] < rel["size"]:
        bad.append(("G6 体积", f"{cand['size']:,} B < 基线 {rel['size']:,} B（少 {rel['size'] - cand['size']:,}）"))
    return bad, where


def do_build(forward=()) -> int:
    """调 build_ability 全套流程，产物落 build/ability.anm.new；失败时还原 anm_ids.h。

    `build_ability.main()` 自己又跑一遍 argparse（`--verify-only` / `--allow-reorder` /
    `--freeze-order`），所以调它之前要把本脚本的参数从 `sys.argv` 里摘掉，只留要转发的。
    """
    old_ids = IDS_H.read_bytes() if IDS_H.is_file() else None
    B.OUT_ANM, B.IDS_H, B.TEX = CAND, IDS_H, BUILD / "tex-ability"
    if CAND.exists():
        CAND.unlink()
    old_argv = sys.argv
    sys.argv = ["build_ability.py", *forward]
    try:
        return B.main()
    except SystemExit as e:
        print(f"✗ 构建失败：{e}")
        if old_ids is not None and IDS_H.is_file() and IDS_H.read_bytes() != old_ids:
            IDS_H.write_bytes(old_ids)
            print(f"  已还原 {label(IDS_H)}（拒绝一次构建不许留下生成物污染）")
        return 1
    finally:
        sys.argv = old_argv


def report(rel, cand, bad, where):
    print(f"\n基线 {label(rel['path'])}：{rel['size']:,} B  md5 {rel['md5']}"
          f"  entry {len(rel['entries'])} / sprite {rel['n_sprites']} / script {len(rel['scripts'])}")
    print(f"候选 {label(cand['path'])}：{cand['size']:,} B  md5 {cand['md5']}"
          f"  entry {len(cand['entries'])} / sprite {cand['n_sprites']} / script {len(cand['scripts'])}")
    extra = sorted(set(cand["names"]) - set(rel["names"]))
    if extra:
        print(f"  （候选多出 {len(extra)} 个 entry：{', '.join(x.split('/')[-1] for x in extra[:8])}"
              f"{' …' if len(extra) > 8 else ''}）")
    if where:
        print(f"  G5 标记在位：{', '.join(where)}")
    if not bad:
        print("\n✅ 六道门全过")
        return 0
    print(f"\n✗ **拒绝**：{len(bad)} 条门禁不过 ——")
    for gate, msg in bad:
        print(f"  [{gate}] {msg}")
    return 2


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--baseline", type=Path, help="基线 anm（默认 release-20261020 备份，其次 build/ability.anm）")
    ap.add_argument("--check-only", type=Path, metavar="ANM", help="只对一个已有 anm 跑门禁（不构建）")
    ap.add_argument("--install", action="store_true", help="门禁全过才 cp 成 native/build/ability.anm")
    ap.add_argument("--allow-reorder", action="store_true",
                    help="透传给 build_ability（改过 ORDER 老行才需要；跑完要重新 --freeze-order）")
    a = ap.parse_args()

    base = a.baseline or (BACKUP if BACKUP.is_file() else TARGET)
    if not base.is_file():
        raise SystemExit(f"没有基线 {base} —— 先把发布件备份成 {BACKUP.name}")
    rel = parse(base)

    if a.check_only:
        cand = parse(a.check_only)
    else:
        print(f"== 构建（写 {label(CAND)}，**不动** {TARGET.name}）")
        rc = do_build(["--allow-reorder"] if a.allow_reorder else [])
        if rc != 0 or not CAND.is_file():
            print("✗ 构建没产出候选件")
            return 1
        cand = parse(CAND)

    bad, where = why_not(rel, cand)
    rc = report(rel, cand, bad, where)
    if rc != 0:
        return rc

    if a.check_only or not a.install:
        print(f"\n（未覆盖：加 `--install` 才会把它 cp 成 {TARGET.relative_to(MOD)}）")
        return 0

    # 装之前再确认一次：门禁刚刚全过，且目标如果存在就先备份（内容相同则跳过）
    if TARGET.is_file() and md5(TARGET) != cand["md5"]:
        bak = TARGET.with_name(f"ability.anm.bak-{cand['md5'][:8]}")
        shutil.copy2(TARGET, bak)
        print(f"旧件已备份：{bak.relative_to(MOD)}（{md5(bak)}）")
    if TARGET.is_file() and md5(TARGET) == cand["md5"]:
        print(f"\n目标已经是同一份（md5 {cand['md5']}）—— 不重复写。")
    else:
        shutil.copy2(CAND, TARGET)
        print(f"\n✅ 已装：{TARGET.relative_to(MOD)}  md5 {md5(TARGET)}  {TARGET.stat().st_size:,} B")
    return 0


if __name__ == "__main__":
    sys.exit(main())
