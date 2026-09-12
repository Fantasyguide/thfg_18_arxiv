#!/usr/bin/env bash
# 把 export_html.py 的产物用 http.server 端出来 —— **只绑 127.0.0.1**，不对外。
#
#   tooling/ghidra/serve.sh th18 [端口]        默认 6090
#
# 你那边开隧道：
#   ssh -L 6090:localhost:6090 sunyunbo@zhustation
# 然后浏览器开 http://localhost:6090
#
# 跟 noVNC 是同一个套路（见全局笔记）。数据不出这台机器，所以 ExpHP 那批无 LICENSE
# 的名字不存在转发问题。
set -euo pipefail

VER="${1:?用法: serve.sh <版本，如 th18> [端口]}"
PORT="${2:-6090}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

mapfile -t DIRS < <(compgen -G "$REPO/local/${VER}*/state" || true)
if [ "${#DIRS[@]}" -eq 0 ]; then
  echo "没找到 $REPO/local/${VER}*/state —— 先跑：" >&2
  echo "  tooling/ghidra/export_html.py $VER" >&2
  exit 1
fi
# 前缀通配会命中多个版本（"th06" 同时命中 th06nc 与 th06.v1.02h）。静默选第一个
# 就是端错库——和 _driver.resolve 一样，这里必须报错要求写全。
if [ "${#DIRS[@]}" -gt 1 ] && [ ! -d "$REPO/local/$VER/state" ]; then
  echo "版本号 '$VER' 有歧义，命中：" >&2
  for d in "${DIRS[@]}"; do echo "  $(basename "$(dirname "$d")")" >&2; done
  echo "请写全版本号。" >&2
  exit 1
fi
DIR="$REPO/local/$VER/state"
[ -d "$DIR" ] || DIR="${DIRS[0]}"

echo "端出 $DIR"
echo "  本机  http://127.0.0.1:$PORT"
echo "  远程  ssh -L $PORT:localhost:$PORT $(whoami)@$(hostname) → http://localhost:$PORT"
exec python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$DIR"
