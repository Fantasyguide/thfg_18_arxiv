#!/usr/bin/env bash
# 在这台 Linux 上交叉编译出 Windows x64 的 xinput1_4.dll。
# 用的是全局笔记里那套 xwin 环境（clang + lld + 已缓存的 CRT/SDK）。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XWIN="${XWIN_CACHE:-$HOME/.cache/cargo-xwin/xwin}"
CLANG="${CLANG:-/data/sunyunbo/miniconda3/envs/xwin/bin/clang}"

[ -d "$XWIN/crt" ] || { echo "找不到 xwin 缓存：$XWIN（见 CLAUDE.md 的 xwin 段）" >&2; exit 1; }

build() {   # build <输出名> <代理源文件> [额外链接参数…]
  local out="$1" proxy="$2"; shift 2
  "$CLANG" --target=x86_64-pc-windows-msvc -O2 -fuse-ld=lld-link \
    -isystem "$XWIN/crt/include" \
    -isystem "$XWIN/sdk/include/ucrt" \
    -isystem "$XWIN/sdk/include/um" \
    -isystem "$XWIN/sdk/include/shared" \
    -shared -o "$HERE/$out" \
    "$HERE/$proxy" "$HERE/hook.c" \
    -Wl,"/libpath:$XWIN/crt/lib/x86_64" \
    -Wl,"/libpath:$XWIN/sdk/lib/ucrt/x86_64" \
    -Wl,"/libpath:$XWIN/sdk/lib/um/x86_64" \
    -Wl,/defaultlib:user32 -Wl,/defaultlib:kernel32 "$@"
  echo "→ $HERE/$out"
}

# 首选：静态导入，加载器必须加载它，不依赖游戏跑到哪一段代码。
build VCRUNTIME140_1.dll proxy_vcr.c -Wl,/export:__CxxFrameHandler4

# 备选：运行时 LoadLibraryW，实测在这台机器上没被加载（见 AUDIT.md §E.3）。
build xinput1_4.dll proxy.c
