#!/usr/bin/env python3
"""sync_assets.py —— 把**构建出的资产**（`ability.anm` / `pl0X.sht`）发进 modkit，并逐一回读校验。

## 为什么要有它（2026-10-15 事故）

`deploy.py` 负责 patch + DLL，`local/sync_modkit_dll.sh` 负责 DLL + BGM + `start_deck` ——
**没有任何一步负责 anm/sht** ✗。于是 modkit 里的 `th18/ability.anm` 可能是**别人批次**的：
DLL 是新的、sht 也对，只有图集是旧的 ⇒ 所有 sprite 索引按错的图集解释 ⇒ **全屏错位** ✗
（owner 实跑受害；真因是 md5 对比出来的两个不同值）。

## 它做什么（对每一条「构建产物 → modkit 目标」）

1. **md5 相同** ⇒ 跳过（不碰时间戳、不重写盘）；
2. **不同 / 目标不存在** ⇒ 先把旧目标**备份**到 `<modkit>/_asset-backup-<时间戳>/<相对路径>`（存在才备），再复制；
3. 全部做完后**回读校验**（重新算两边的 md5）并打印表格；**任何一条不一致 ⇒ 退出码 1** ✗。

## 用法

    python3 tools/sync_assets.py --modkit "/mnt/d/项目/幻想指南/18/th18_modkit"
    python3 tools/sync_assets.py --modkit "..." --check      # 只校验、不写（测试前先跑这个）

`MODKIT_DIR` 环境变量可代替 `--modkit`。资产的**目标目录**只认一处（见 `DEST_REL`）：
modkit 里 `th18_card_expand_255/th18/` —— 2026-10-15 已确认 modkit 内 `ability.anm` 与 `pl0X.sht` 各只有这一份。
"""
import argparse
import hashlib
import os
import shutil
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
NATIVE = HERE.parent                                   # …/card-expand/native
BUILD = NATIVE / "build"
DEST_REL = Path("thcrap/repos/Renko_1055/th18_card_expand_255/th18")


def md5(p: Path) -> str:
    h = hashlib.md5()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ★ 游戏侧真正会读的资产 = **显式清单**（2026-10-15 与 modkit 实际内容逐一核对过）。
#   ⚠ 故意**不用** glob 收 `build/*.anm`：那里还躺着 `pl01.anm` / `abcard.anm` / `proto-pl00.anm`
#     这些**不进游戏**的中间产物（第一版 glob 就被它们绊了一下 ✗）。
#   新增资产时把它加进来；下面的 `warn_unlisted()` 会在"build 里有、清单里没有"时报出来 ✓。
SHIPPED = [
    ("ability.anm", ""),
    ("pl00.sht", "sht"),
    ("pl01.sht", "sht"),
    ("pl02.sht", "sht"),
    ("pl03.sht", "sht"),
]


def collect_assets() -> list[Path]:
    """显式清单里的构建产物（缺任何一个都算错，直接退出）。"""
    if not BUILD.is_dir():
        print("✗ 找不到构建产物目录：%s（先 `make anm` / `make sht`）" % BUILD, file=sys.stderr)
        sys.exit(2)
    out, missing = [], []
    for name, sub in SHIPPED:
        p = BUILD / sub / name if sub else BUILD / name
        (out if p.is_file() else missing).append(p if p.is_file() else p)
    if missing:
        print("✗ 构建产物缺这几件：%s（先 `make anm` / `make sht`）" % ", ".join(str(m) for m in missing),
              file=sys.stderr)
        sys.exit(2)
    return out


def warn_unlisted() -> None:
    """反向巡检：`build/` 里有 .anm/.sht 但不在清单里 ⇒ 提醒（可能是新资产忘了发布）。"""
    listed = {p.name for p in collect_assets()}
    extra = sorted(p for p in list(BUILD.glob("*.anm")) + list((BUILD / "sht").glob("*.sht"))
                   if p.name not in listed)
    if extra:
        print("⚠ build/ 里还有这些不在发布清单里的 .anm/.sht（是中间产物就忽略；是**新资产**就得加进 SHIPPED）：")
        for p in extra:
            print("    %s" % p.relative_to(NATIVE))



def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--modkit", default=os.environ.get("MODKIT_DIR", ""))
    ap.add_argument("--check", action="store_true", help="只校验，不写")
    args = ap.parse_args()

    if not args.modkit:
        print("✗ 需要 --modkit <dir>（或 MODKIT_DIR 环境变量）", file=sys.stderr)
        return 2
    modkit = Path(args.modkit)
    dest_dir = modkit / DEST_REL
    if not dest_dir.is_dir():
        print("✗ modkit 里找不到资产目录：%s" % dest_dir, file=sys.stderr)
        print("  （如果你改过 modkit 布局，请更新本脚本的 DEST_REL）", file=sys.stderr)
        return 2

    assets = collect_assets()
    warn_unlisted()
    stamp = time.strftime("%Y%m%d-%H%M%S")
    backup_dir = modkit / ("_asset-backup-%s" % stamp)

    rows, n_same, n_copy, n_backup = [], 0, 0, 0
    for src in assets:
        dst = dest_dir / src.name
        s_md5 = md5(src)
        d_md5 = md5(dst) if dst.is_file() else None
        if d_md5 == s_md5:
            rows.append((src.name, s_md5, d_md5, "同 md5，跳过"))
            n_same += 1
            continue
        if args.check:
            rows.append((src.name, s_md5, d_md5, "★ 不一致（--check 未修）"))
            continue
        if dst.is_file():
            bak = backup_dir / DEST_REL / src.name
            bak.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(dst, bak)
            n_backup += 1
        else:
            dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        n_copy += 1
        rows.append((src.name, s_md5, md5(dst), "已复制"))

    # ---- 回读校验（写模式与 check 模式都做）----
    bad = []
    print("\n资产同步 %s（modkit=%s）" % ("校验" if args.check else "结果", modkit))
    print("  %-14s %-34s %-34s %s" % ("文件", "构建产物 md5", "modkit md5", "动作"))
    for name, s, d, act in rows:
        now = md5(dest_dir / name) if (dest_dir / name).is_file() else None
        if not args.check and now != s:
            act += " ✗ 校验不过"
        if now != s:
            bad.append(name)
        print("  %-14s %-34s %-34s %s" % (name, s, now or "(缺)", act))
    print("\n  同 md5 跳过 %d / 复制 %d / 备份 %d" % (n_same, n_copy, n_backup))
    if n_backup:
        print("  旧文件备份在：%s" % backup_dir)
    if bad:
        print("\n✗ %d 个文件与构建产物不一致：%s" % (len(bad), ", ".join(bad)))
        return 1
    print("✓ 资产全部与构建产物同 md5（%d 个）" % len(assets))
    return 0


if __name__ == "__main__":
    sys.exit(main())
