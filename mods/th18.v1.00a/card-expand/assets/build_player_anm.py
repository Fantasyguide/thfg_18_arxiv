#!/usr/bin/env python3
"""咆哮兽灵弹的 anm 侧：**不改 pl0X.anm**（2026-09-11 四修，AUDIT §AL33）。

    python3 build_player_anm.py                # 拷零售 pl0X.anm 到 native/build/ + 硬校验
    python3 build_player_anm.py --verify-only  # 只校验已有的 native/build/pl0X.anm

## 为什么不再追加任何 entry / 脚本

一手（本脚本每一步都会回读核对，见 `_assert_sources`）：

- TH17 `PlayerBullet__fire`（`th17:0x44a180`）在 `0x44a535`–`0x44a539` 做
  `movsx eax, word [shooter+0x22]; add eax, 5`，所以 TH17 实际播
  **`pl0X.anm` script(anm+5)**。由此：
  `pl00` 狼 `anm=5→script10`、鹫 `anm=6→script11`，`pl01` 狼 `anm=8→script13`、鹫 `anm=11→script16`；
  `hit5` 硬编码同文件 `script8`（`th17:0x44c462`；TH18 改读 shooter `+0x26`）。
- **TH18 零售 `pl00.anm` / `pl01.anm` 已经逐字节含有这几支兽灵脚本**，而且 entry0 的
  贴图与 sprite 表也和 TH17 同名自机逐字节一致。也就是说：
  - 追加一份 entry0 只会得到一份**重复贴图**；
  - 把脚本“抄简”（去掉 `scriptNew` / `interruptLabel`）才是前几轮
    「魔理沙大鹫灵闪退 + 狼灵无爆炸 + 命中处出现自机图」的根因。
  ⇒ 正确做法是 **零售脚本直接用**，`append_shooterset.py` 的 `HYPER_ANM_MAP` 把
  TH17 `anm` 指到零售 `script(anm+5)`（`hit_fx=8`）。pl0X.anm 一个字节都不改。

- `pl02`（咲夜）/ `pl03`（早苗）没有 TH17 对应自机，零售文件里没有兽灵脚本/贴图。
  本轮**安全回退**：`append_shooterset.py` 对这两个文件把 TH17 `anm` 按 identity
  指回各自零售脚本（画 TH18 普通自机弹，不会崩、不会出自机图）。视觉目标未达成，
  见 AUDIT §AL33 的「pl02/pl03 未做」。

本脚本因此只做两件事：
1. 把零售 `local/th18.v1.00a/dat/pl0X.anm` **原样拷**到 `native/build/pl0X.anm`；
2. **硬校验**（任一失败非 0 退出、不产出文件）：
   - 目标脚本号在文件里存在；
   - 每个脚本里所有 `spriteN` 的 N < 该文件 sprite 总数，且该 sprite 确实存在；
   - 脚本里 `scriptNew(N)` / `scriptNewPos(N, …)` 的目标脚本也存在；
   - `pl00/pl01` 的兽灵脚本与 TH17 源逐字节一致；
   - 打印每文件的 sprite / script 总数与用到的 sprite 号。
"""
import argparse
import re
import shutil
import sys
from pathlib import Path

import anmlib as L

HERE, MOD = L.HERE, L.MOD
REPO = L.REPO
TH17_DAT = REPO / "local" / "th17.v1.00b" / "dat"
TH18_DAT = REPO / "local" / "th18.v1.00a" / "dat"
OUT = L.BUILD
PLAYERS = ("pl00", "pl01", "pl02", "pl03")

# TH17 shooter `anm` 字段 -> 该文件里的实际脚本号（= anm+5；slot7 是 hit5 硬编码 script8）。
# pl00/pl01 的零售文件里就有这些脚本；pl02/pl03 没有，走 FALLBACK。
TH17_ANM_TO_SCRIPT = {5: 10, 6: 11, 7: 8, 8: 13, 11: 16}
# pl00/pl01：TH17 `anm` -> 零售脚本号（就是同一支兽灵脚本）。
BEAST_SCRIPTS = {"pl00": {10, 11}, "pl01": {13, 16}}
HIT_FX_SCRIPT = 8
# 会飞的主弹脚本（只有它们要求「主飞行路径无 delete」；hit_fx=8 是一次性特效，本来就会自删）。
FLYING_SCRIPTS = {"pl00": (10, 11), "pl01": (13, 16)}
# pl02/pl03：安全回退，identity（各自零售普通自机弹脚本）。见 AUDIT §AL33。
FALLBACK_ANM_TO_SCRIPT = {5: 5, 6: 6, 7: 7, 8: 8, 11: 11}

_SPRITE_REF = re.compile(r"\bsprite\(sprite(\d+)\)")
_SPRITE_DEF = re.compile(r"\bsprite(\d+):")
_SCRIPT_NEW = re.compile(r"\bscriptNewPos?\(script(\d+)")


def _scripts(text: str):
    return {int(m.group(1)): m.group(2)
            for m in re.finditer(r"^script script(\d+) \{\n(.*?)^\}",
                                 text, re.M | re.S)}


def _sprite_ids(text: str):
    ids = set()
    for m in _SPRITE_DEF.finditer(text):
        ids.add(int(m.group(1)))
    return ids


def _verify_scripts(player: str, text: str, targets: dict, label: str):
    """硬校验：目标脚本存在 + sprite / scriptNew 引用全部在界内。-> (n_sprites, used)"""
    sprites = _sprite_ids(text)
    n_sprites = (max(sprites) + 1) if sprites else 0
    scripts = _scripts(text)
    used = {}
    for anm, sid in sorted(targets.items()):
        if sid not in scripts:
            raise SystemExit(f"{player}.anm: {label} anm={anm} -> script{sid} 不存在"
                             f"（文件只有 {len(scripts)} 支脚本：{sorted(scripts)[:5]}…）")
        body = scripts[sid]
        refs = sorted({int(m.group(1)) for m in _SPRITE_REF.finditer(body)})
        bad = [s for s in refs if s not in sprites or s >= n_sprites]
        if bad:
            raise SystemExit(
                f"{player}.anm: {label} script{sid} 引用不存在的 sprite {bad}"
                f"（该文件 sprite 总数 {n_sprites}，id 最大 {max(sprites) if sprites else -1}）")
        sn = sorted({int(m.group(1)) for m in _SCRIPT_NEW.finditer(body)})
        miss = [s for s in sn if s not in scripts]
        if miss:
            raise SystemExit(f"{player}.anm: {label} script{sid} 的 scriptNew "
                             f"指向不存在的 script {miss}")
        # 会飞的主弹脚本：普通飞行路径（第一个 interruptLabel 之前）绝不能有 delete()，
        # 否则 TH18 的「VM 没了 = 弹没了」会让射程变短（AUDIT §AL28/§AL33）。
        if sid in FLYING_SCRIPTS.get(player, ()):
            main = body.split("interruptLabel", 1)[0]
            if "delete(" in main:
                raise SystemExit(f"{player}.anm: {label} script{sid} 的普通飞行路径含 delete()")
        used[sid] = refs
    return n_sprites, len(scripts), used


def _spec_of(path: Path, tools, anmmap):
    return L.thanm(["-l", L.VERSION, str(path), "-m", str(anmmap)], path.parent, tools)


def _assert_sources(player: str, text: str, tools, anmmap):
    """TH17 源与 TH18 零售的兽灵脚本必须逐字节一致（这是「直接用零售」的硬前提）。"""
    src = TH17_DAT / f"{player}.anm"
    if not src.is_file():
        raise SystemExit(f"缺 TH17 样本 {src}")
    src_text = _spec_of(src, tools, anmmap)
    a, b = _scripts(src_text), _scripts(text)
    for sid in sorted(BEAST_SCRIPTS[player] | {HIT_FX_SCRIPT}):
        if sid not in a:
            raise SystemExit(f"TH17 {player}.anm 没有 script{sid}")
        if sid not in b:
            raise SystemExit(f"TH18 {player}.anm 没有 script{sid}")
        if a[sid] != b[sid]:
            raise SystemExit(
                f"{player}.anm: script{sid} 在 TH17 与 TH18 零售之间不一致 —— "
                f"不能直接引用零售脚本，需要重新评估追加方案")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--verify-only", action="store_true")
    a = ap.parse_args()

    th, td, mapdir = L.find_tools()
    tools, anmmap = (th, td), mapdir / "v8.anmm"

    OUT.mkdir(parents=True, exist_ok=True)
    for player in PLAYERS:
        retail_anm = TH18_DAT / f"{player}.anm"
        if not retail_anm.is_file():
            raise SystemExit(f"缺零售样本 {retail_anm}（用户放进 local/，见 local/README.md）")
        out_anm = OUT / f"{player}.anm"

        # ★ 先校验（拿零售文件作为待校验对象），全部通过后才落盘 —— 任何越界 / 缺脚本 /
        #   与 TH17 不一致都在写文件之前非 0 退出，绝不产出一个坏文件。
        text = _spec_of(retail_anm, tools, anmmap)
        if player in BEAST_SCRIPTS:
            _assert_sources(player, text, tools, anmmap)
            targets = {anm: TH17_ANM_TO_SCRIPT[anm] for anm in (5, 6, 7, 8, 11)}
            label = "TH17 兽灵脚本"
        else:
            targets = dict(FALLBACK_ANM_TO_SCRIPT)
            label = "安全回退"
        n_sprites, n_scripts, used = _verify_scripts(player, text, targets, label)
        flat_used = sorted({s for refs in used.values() for s in refs})

        if not a.verify_only:
            shutil.copyfile(retail_anm, out_anm)
        if not out_anm.is_file():
            raise SystemExit(f"缺 {out_anm}：先跑 python3 assets/build_player_anm.py")
        if out_anm.read_bytes() != retail_anm.read_bytes():
            raise SystemExit(f"{player}.anm: native/build 与零售不一致 —— 绝不允许追加/改动")

        print(f"  {player}: {n_sprites} sprites, {n_scripts} scripts, 不改动；"
              f"{label} " + " ".join(f"{k}->{v}" for k, v in sorted(targets.items())) +
              f"；用到 sprite {flat_used}")

    print("verify: pl00/pl01 的兽灵脚本与 TH17 逐字节一致、直接用零售脚本；"
          "pl02/pl03 安全回退到零售普通自机弹；pl0X.anm 零改动，无越界 sprite / script。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
