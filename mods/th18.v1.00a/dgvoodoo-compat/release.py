#!/usr/bin/env python3
"""Publish the audited dgVoodoo package into the local th18_modkit checkout."""

import shutil
from pathlib import Path


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = HERE / "package"
MODKIT = ROOT / "local/vendor/th18_modkit"
DEST = MODKIT / "compat/dgvoodoo"
FILES = ("package.json", "D3D9.dll", "dgVoodoo.conf", "README.md")


def main() -> int:
    if not (MODKIT / ".git").is_dir():
        raise SystemExit(f"找不到 th18_modkit 发布仓库：{MODKIT}")
    DEST.mkdir(parents=True, exist_ok=True)
    for name in FILES:
        source = SOURCE / name
        if not source.is_file():
            raise SystemExit(f"兼容包不完整，缺少 {source}")
        shutil.copyfile(source, DEST / name)
        print(f"同步 {DEST.relative_to(MODKIT) / name}")
    print("未提交、未推送；请先运行 test_package.py 并检查两个仓库的 diff。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
