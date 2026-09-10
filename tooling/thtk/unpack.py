#!/usr/bin/env python3
"""把一作的 .dat 解开，并把每个 .anm 反成「一个目录 = spec + 贴图」。

    python3 tooling/thtk/unpack.py th18.v1.00a            # 幂等：dat/ 已有就跳过
    python3 tooling/thtk/unpack.py th18.v1.00a --force    # 重解

产物（全部在 local/，不入库）：
    local/<版本>/dat/*                     thdat -x
    local/<版本>/anm/<名>/<名>.anm.txt     thanm -l（带 anmmap 助记符）
    local/<版本>/anm/<名>/<entry 路径>.png thanm -x
    local/<版本>/anm/<名>/<名>.err         应为空

工具与 anmmap 靠探测（PATH → local/vendor/），不写死路径；缺什么给出修复命令。
`find_tools()` 可被别的脚本 import（mod 侧的重建脚本用它）。
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
LOCAL = REPO / "local"
VENDOR = LOCAL / "vendor"

# 版本目录前缀 → (thtk 版本参数, anmmap 文件名)
GAMES = {
    "th15": ("15", "v8.anmm"),
    "th16": ("16", "v8.anmm"),
    "th18": ("18", "v8.anmm"),
}
ANMMAP_DIRS = (
    VENDOR / "thpages" / "static" / "mapfile",   # 主：ExpHP 指令参考站（多一条 432）
    VENDOR / "truth" / "map",                     # 备：ExpHP truth（Apache-2.0）
)


def find_tools():
    """→ (thanm, thdat, anmmap_dir)，都是 Path。找不到抛 SystemExit 并带修复命令。"""
    build = VENDOR / "thtk" / "build"
    tools = []
    for name in ("thanm", "thdat"):
        p = shutil.which(name)
        cand = build / name / name
        if p:
            tools.append(Path(p))
        elif cand.is_file():
            tools.append(cand)
        else:
            raise SystemExit(f"找不到 {name}：跑  bash tooling/thtk/build.sh")
    for d in ANMMAP_DIRS:
        if (d / "v8.anmm").is_file():
            return tools[0], tools[1], d
    raise SystemExit("找不到 anmmap：git clone https://github.com/ExpHP/thpages local/vendor/thpages")


def game_of(version: str):
    key = version.split(".")[0]
    if key not in GAMES:
        raise SystemExit(f"不认识 {version}；GAMES 里只有 {', '.join(GAMES)}（在 unpack.py 里加一行即可）")
    return key, GAMES[key]


def run(args, cwd, log: Path):
    r = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    with log.open("a") as f:
        f.write(r.stderr)
    if r.returncode != 0:
        raise SystemExit(f"{' '.join(map(str, args))} 失败（exit {r.returncode}），见 {log}")
    return r.stdout


def count(spec: Path, prefix: str) -> int:
    return sum(1 for line in spec.read_text(errors="replace").splitlines() if line.startswith(prefix))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("version", help="如 th18.v1.00a（local/ 下的目录名）")
    ap.add_argument("--force", action="store_true", help="dat/ 与 anm/ 已存在也重解")
    a = ap.parse_args()

    key, (ver, mapname) = game_of(a.version)
    thanm, thdat, mapdir = find_tools()
    anmmap = mapdir / mapname
    vdir = LOCAL / a.version
    dats = sorted(vdir.glob("*.dat"))
    if not dats:
        raise SystemExit(f"{vdir} 下没有 .dat——把你自己的 {key}.dat 放进去（见 local/README.md）")
    dat = dats[0]

    # 1. dat → dat/
    datdir = vdir / "dat"
    if a.force and datdir.exists():
        shutil.rmtree(datdir)
    if datdir.exists() and any(datdir.iterdir()):
        print(f"dat/ 已有 {sum(1 for _ in datdir.iterdir())} 个文件，跳过（--force 重解）")
    else:
        datdir.mkdir(parents=True, exist_ok=True)
        run([thdat, "-x", ver, dat.resolve()], datdir, vdir / "thdat.log")
        print(f"thdat -x {ver} {dat.name} → dat/：{sum(1 for _ in datdir.iterdir())} 个文件")

    # 2. 每个 anm 一目录
    anmroot = vdir / "anm"
    if a.force and anmroot.exists():
        shutil.rmtree(anmroot)
    anmroot.mkdir(exist_ok=True)
    rows = []
    for f in sorted(datdir.glob("*.anm")):
        name = f.stem
        d = anmroot / name
        spec = d / f"{name}.anm.txt"
        err = d / f"{name}.err"
        if not spec.exists():
            d.mkdir(exist_ok=True)
            err.write_text("")
            spec.write_text(run([thanm, "-l", ver, f.resolve(), "-m", anmmap], d, err))
            run([thanm, "-x", ver, f.resolve()], d, err)
            if err.stat().st_size:
                raise SystemExit(f"{name}: thanm 有告警/错误，见 {err}")
        rows.append((name, count(spec, "entry"), count(spec, "script")))

    print(f"\nanm/：{len(rows)} 个（anmmap = {anmmap.relative_to(REPO)}）")
    print(f"{'名':<12}{'entries':>8}{'scripts':>8}")
    for n, e, s in rows:
        print(f"{n:<12}{e:>8}{s:>8}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
