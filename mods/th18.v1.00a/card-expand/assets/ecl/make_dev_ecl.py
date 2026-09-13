#!/usr/bin/env python3
"""开发辅助：st01–st06 全部做成「近乎空壳」——进关 = logo → 对话 → 血量缩百倍的 boss → 关底商店。单独一个可勾选的 patch（th18_card_expand_devstage），想打正常弹幕就不勾。

    python3 make_dev_ecl.py                 # 编 assets/ecl/stNN.ecl.txt（空壳，入库）+ 现场改原生 stNNbs.ecl → native/build/ecl/
    python3 make_dev_ecl.py --regen-shells  # 从原生重新生成六个空壳源（结构变了才需要；生成物入库）

空壳（stNN.ecl，源在仓库里）：只留头部（anim / ecli）、main、LogoEnemy、MainBoss；main 里 debug22 全删、
`@MainFront()` 起到 `setChapter(41)` 之前（道中 + 道中 boss + 中途对话）换成 wait(30)。六关 main 同构（2026-09-04 逐关看过）。
boss（stNNbs.ecl，满是 ZUN 的符卡脚本，不入库）：现场反编译原生文件，
  · 血量常数等比 ÷LIFE_DIV：lifeSet(N)、setInterrupt(槽, 血量, 超时, 子) 的血量、lifeMarker(槽, 血量, 色) 的血量（超时不动）
  · BossDead 里原生 @BossItem(...) 之后再撒 BOSS_MONEY_DROP 个金钱道具（type 2，每个 +1 金）
thecl 往返（-d 再 -c）字节一致，没改的部分不会走样。反编译文本里有 Shift-JIS 字符串，读写用 surrogateescape。
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MOD = HERE.parents[1]
REPO = MOD.parents[2]

VERSION = "18"
STAGES = ["01", "02", "03", "04", "05", "06"]
DAT = REPO / "local" / "th18.v1.00a" / "dat"
OUT = MOD / "native" / "build" / "ecl"
ECLMAP = REPO / "local" / "vendor" / "eclmap" / "eclmap" / "th18.eclm"
LIFE_DIV = 100
BOSS_MONEY_DROP = 300   # boss 死时额外掉的金钱道具数（每个 +1 金）；道具池若不够就少掉，无害
ENC = dict(encoding="utf-8", errors="surrogateescape")

_SUB = r"^void %s\([^)]*\)\n\{\n.*?^\}\n"


def thecl():
    p = REPO / "local" / "vendor" / "thtk" / "build" / "thecl" / "thecl"
    if not p.is_file():
        raise SystemExit("没有 thecl：bash tooling/thtk/build.sh（会连 thecl 一起编）")
    if not ECLMAP.is_file():
        raise SystemExit("没有 th18 eclmap：git clone https://github.com/Priw8/eclmap local/vendor/eclmap")
    return p


def run(args):
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0 or r.stderr.strip():
        raise SystemExit(f"{' '.join(map(str, args))} 失败：\n{r.stderr or r.stdout}")


def decompile(t, name):
    src = DAT / f"{name}.ecl"
    if not src.is_file():
        raise SystemExit(f"没有 {src}：先 python3 tooling/thtk/unpack.py th18.v1.00a")
    txt = OUT / f"{name}.retail.txt"
    run([t, "-d", VERSION, "-m", ECLMAP, src, txt])
    return txt.read_text(**ENC)


def sub(text, name):
    m = re.search(_SUB % re.escape(name), text, re.S | re.M)
    if not m:
        raise SystemExit(f"找不到子程序 {name}()")
    return m.group(0)


# ── 空壳 ─────────────────────────────────────────────────────────────
def gen_shell(text, stage):
    first = re.search(r"^void [A-Za-z0-9_]+\([^)]*\)\n\{", text, re.M)
    header = text[:first.start()]
    main = "\n".join(l for l in sub(text, "main").split("\n") if "debug22(" not in l)
    a, b = main.find("    @MainFront();\n"), main.find("    setChapter(41);\n")
    if a < 0 or b < a:
        raise SystemExit(f"st{stage} main 的道中段落长得和预期不一样（找不到 @MainFront() … setChapter(41)）")
    main = main[:a] + "    wait(30);\n" + main[b:]
    note = (f"// 开发辅助：st{stage} 空壳 —— 只留 logo、对话、boss（血量缩百倍、死时多掉钱的 st{stage}bs.ecl 由 make_dev_ecl.py 现场生成）。\n"
            "// 道中全部删除；结构照原生 main，原生的敌机子程序不保留。由 make_dev_ecl.py --regen-shells 生成，thecl -c 18 -m th18.eclm 编译。\n\n")
    return header + note + main + "\n" + sub(text, "LogoEnemy") + "\n" + sub(text, "MainBoss")


# ── boss ─────────────────────────────────────────────────────────────
def _scale(n: int) -> int:
    return max(1, n // LIFE_DIV) if n > 0 else n


def patch_boss(text, stage):
    n = 0

    def f_life(m):
        nonlocal n; n += 1
        return f"lifeSet({_scale(int(m.group(1)))})"

    def f_int(m):
        nonlocal n; n += 1
        return f"setInterrupt({m.group(1)}, {_scale(int(m.group(2)))}, {m.group(3)}"

    def f_mark(m):
        nonlocal n; n += 1
        return f"lifeMarker({m.group(1)}, {_scale(int(float(m.group(2))))}.0f"

    text = re.sub(r"lifeSet\((\d+)\)", f_life, text)
    text = re.sub(r"setInterrupt\((-?\d+), (\d+), (-?\d+)", f_int, text)
    text = re.sub(r"lifeMarker\((-?\d+), ([0-9.]+)f", f_mark, text)
    if n == 0:
        raise SystemExit(f"st{stage}bs.ecl 里没有血量相关指令")
    dead = sub(text, "BossDead")
    m = re.search(r"^    @BossItem\([^)]*\);\n", dead, re.M)
    if not m:
        raise SystemExit(f"st{stage}bs.ecl 的 BossDead 里找不到 @BossItem(...)")
    drop = f"    dropClear();\n    dropExtra(2, {BOSS_MONEY_DROP});\n    dropArea(320.0f, 160.0f);\n    dropItems();\n"
    dead2 = dead[:m.end()] + drop + dead[m.end():]
    return text.replace(dead, dead2, 1), n


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--regen-shells", action="store_true", help="从原生重新生成 assets/ecl/stNN.ecl.txt")
    a = ap.parse_args()
    t = thecl()
    OUT.mkdir(parents=True, exist_ok=True)
    for st in STAGES:
        shell = HERE / f"st{st}.ecl.txt"
        if a.regen_shells or not shell.is_file():
            shell.write_text(gen_shell(decompile(t, f"st{st}"), st), **ENC)
            print(f"shell: {shell.relative_to(MOD)} 已生成（{shell.read_text(**ENC).count(chr(10))} 行）")
        run([t, "-c", VERSION, "-m", ECLMAP, shell, OUT / f"st{st}.ecl"])
        bs, n = patch_boss(decompile(t, f"st{st}bs"), st)
        txt = OUT / f"st{st}bs.dev.txt"
        txt.write_text(bs, **ENC)
        run([t, "-c", VERSION, "-m", ECLMAP, txt, OUT / f"st{st}bs.ecl"])
        print(f"ecl: st{st}.ecl（空壳）、st{st}bs.ecl（{n} 处血量 ÷{LIFE_DIV}，死时 +{BOSS_MONEY_DROP} 金）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
