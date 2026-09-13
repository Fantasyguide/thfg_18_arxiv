#!/usr/bin/env bash
# 卡片 117（AL63）源图回流：把 Windows 侧 art-src 的**根目录** PNG 拷回仓库规范输入目录。
#   bash assets/sync_player_art_src.sh [art-src 目录]
# 默认：/mnt/d/项目/幻想指南/18/art-src（= D:\项目\幻想指南\18\art-src）
# 之后：cd native && make anm   # 重新把源图嵌进 pl02/pl03.anm
#
# ⚠ 构建对拷回来的图**原样使用、不做任何自动调色**；所以请先给原图上色再跑本脚本。
# ⚠ 只同步根目录的 *.png；`_auto_default/`（默认色参考图）不会被同步。
set -euo pipefail
ART_SRC="${1:-/mnt/d/项目/幻想指南/18/art-src}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DST="$HERE/player_art_src"
[ -d "$ART_SRC" ] || { echo "找不到源图目录：$ART_SRC" >&2; exit 1; }
mkdir -p "$DST"
n=0
for f in "$ART_SRC"/*.png; do
    [ -e "$f" ] || continue
    cp -f "$f" "$DST/$(basename "$f")"
    n=$((n + 1))
done
echo "已同步 $n 张：$ART_SRC -> $DST"
echo "提醒：构建会原样使用这些图（不再自动套色）；没上色的图 = 没上色。"
echo "下一步：cd native && make anm && make check"
