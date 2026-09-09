#!/usr/bin/env bash
# 构建测试夹具：把 fixtures/*.anm.txt 用 thanm 编译成 .anm（gitignored，见 .gitignore）。
#
# ⚠️ 对 brief 步骤 1 的必要修正（不是 I1 report 里裁定的那 3 处偏离，是运行时才发现的
#    thanm CLI 用法问题）：
#   1. thanm 二进制不在 `local/vendor/thtk/build/thanm`（那是源码目录），实际可执行文件在
#      `local/vendor/thtk/build/thanm/thanm`。
#   2. `-c`/`-l` 都要求显式 VERSION 位置参数（`thanm -c VERSION ARCHIVE SPEC`），
#      brief 里 `thanm -c "${f%.txt}" "$f"` 漏了这个参数——本仓固定用 8（v8）。
set -euo pipefail
ROOT="$(git rev-parse --show-toplevel)"
THANM="$(command -v thanm || echo "$ROOT/local/vendor/thtk/build/thanm/thanm")"
FIXDIR="$(dirname "$0")/fixtures"
cd "$FIXDIR"

# 8x8 纯色 PNG（RGB，无版权字节，纯脚本生成，不入库）——minimal.anm.txt 里
# `name: "minimal.png"` 引用的贴图源。thanm -c 在 hasData:1 时要求这个文件
# 真实存在（读 IHDR 定宽高、读像素转目标 format），见 thanm/thanm.c anm_defaults()。
python3 - << 'PYEOF'
import struct, zlib

def chunk(tag, data):
    c = tag + data
    return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)

W = H = 8
sig = b"\x89PNG\r\n\x1a\n"
ihdr = struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)  # 8-bit RGB, no interlace
row = b"\x00" + (b"\x40\x80\xc0" * W)  # filter=none, 纯色像素
raw = row * H
idat = zlib.compress(raw, 9)
png = sig + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b"")
with open("minimal.png", "wb") as f:
    f.write(png)
PYEOF

for f in *.anm.txt; do
    "$THANM" -c 8 "${f%.txt}" "$f"
done
