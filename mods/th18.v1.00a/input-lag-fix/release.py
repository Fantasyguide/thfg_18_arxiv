#!/usr/bin/env python3
"""Publish the joystick polling patch into the local th18_modkit checkout."""

import json
import shutil
import zlib
from pathlib import Path


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = HERE / "patch"
MODKIT = ROOT / "local/vendor/th18_modkit"
DEST = MODKIT / "thcrap/repos/Renko_1055/th18_disable_controller_polling"
PATCH_FILES = ("patch.js", "th18.v1.00a.js")


def write_files_manifest(directory: Path) -> None:
    entries = {
        name: zlib.crc32((directory / name).read_bytes()) & 0xFFFFFFFF
        for name in PATCH_FILES
    }
    (directory / "files.js").write_text(
        json.dumps(entries, indent=2) + "\n", encoding="utf-8"
    )


def main() -> int:
    if not (MODKIT / ".git").is_dir():
        raise SystemExit(f"找不到 th18_modkit 发布仓库：{MODKIT}")
    for name in PATCH_FILES:
        if not (SOURCE / name).is_file():
            raise SystemExit(f"补丁源不完整，缺少 {SOURCE / name}")

    write_files_manifest(SOURCE)
    DEST.mkdir(parents=True, exist_ok=True)
    for name in (*PATCH_FILES, "files.js"):
        shutil.copyfile(SOURCE / name, DEST / name)
        print(f"同步 {DEST.relative_to(MODKIT) / name}")
    print("未提交、未推送；请先运行 test_patch.py 并检查两个仓库的 diff。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
