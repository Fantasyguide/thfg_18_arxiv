#!/usr/bin/env bash
# 打发布包：只有 DLL、示例 ini、玩家 README 三个文件。产物在 dist/（gitignored）。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"
python3 native/check.py >/dev/null || { echo "check.py 未通过，不打包" >&2; exit 1; }
make -C native/tests test >/dev/null || { echo "宿主单测未通过，不打包" >&2; exit 1; }
bash native/build.sh >/dev/null
VER="${1:-$(date +%Y%m%d)}"
OUT="dist/th06nc-autoplay-$VER"
rm -rf "$OUT" && mkdir -p "$OUT"
cp native/VCRUNTIME140_1.dll release/th06nc_autoplay.ini release/README.txt "$OUT/"
(cd dist && rm -f "th06nc-autoplay-$VER.zip" && zip -q -r "th06nc-autoplay-$VER.zip" "th06nc-autoplay-$VER")
echo "→ $HERE/dist/th06nc-autoplay-$VER.zip"
unzip -l "dist/th06nc-autoplay-$VER.zip" | tail -n +4 | head -n -2
