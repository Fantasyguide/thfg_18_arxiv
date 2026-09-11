#!/usr/bin/env bash
# 在这台 Linux 上交叉编译出 Windows x64 的 VCRUNTIME140_1.dll（代理 + hook + 抽取 + 策略）。
# 用的是全局笔记里那套 xwin 环境（clang + lld + 已缓存的 CRT/SDK）。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XWIN="${XWIN_CACHE:-$HOME/.cache/cargo-xwin/xwin}"
CLANG="${CLANG:-/data/sunyunbo/miniconda3/envs/xwin/bin/clang}"

[ -d "$XWIN/crt" ] || { echo "找不到 xwin 缓存：$XWIN（见 CLAUDE.md 的 xwin 段）" >&2; exit 1; }

"$CLANG" --target=x86_64-pc-windows-msvc -O2 -fuse-ld=lld-link \
  -Wall -Wextra -D_CRT_SECURE_NO_WARNINGS \
  -isystem "$XWIN/crt/include" \
  -isystem "$XWIN/sdk/include/ucrt" \
  -isystem "$XWIN/sdk/include/um" \
  -isystem "$XWIN/sdk/include/shared" \
  -shared -o "$HERE/VCRUNTIME140_1.dll" \
  "$HERE/proxy_vcr.c" "$HERE/hook.c" "$HERE/extract.c" "$HERE/policy.c" \
  -Wl,"/libpath:$XWIN/crt/lib/x86_64" \
  -Wl,"/libpath:$XWIN/sdk/lib/ucrt/x86_64" \
  -Wl,"/libpath:$XWIN/sdk/lib/um/x86_64" \
  -Wl,/defaultlib:user32 -Wl,/defaultlib:kernel32 \
  -Wl,/export:__CxxFrameHandler4
echo "→ $HERE/VCRUNTIME140_1.dll"
