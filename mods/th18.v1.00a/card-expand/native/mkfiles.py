#!/usr/bin/env python3
"""刷新 files.js —— thcrap 分发用的 {相对路径: crc32} 清单，递归子目录，收 .js / .anm / .ecl / .wav。

.wav 是音效表扩容的语音（patch/th18/voice/）：DLL 用 stack_game_file_resolve 取它们，
不进 files.js 的话远端分发时不会被下载。

    python3 mkfiles.py                 # patch/ 与 patch-test/
    python3 mkfiles.py <dir> [<dir>…]  # 只刷给定目录（make dist 对 dist/patch-step3 用）
    python3 mkfiles.py --check         # 只校验 cards.js（不写 files.js）
"""
import subprocess
import sys
import json, os, zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def gitignored(paths, patch_dir):
    """问 git 哪些文件是 gitignored 的 —— **入库的** files.js 不该列它们。

    语音 wav 在本仓是 gitignored 的（不留版权 / 大二进制字节），所以 patch/files.js
    不列它；而 make dist 的目标 dist/ **整个目录**就是 gitignored 的 —— 那份是要发出去的，
    必须全收。所以先看目录本身：被忽略 = 出包用，不过滤。
    """
    if not paths:
        return set()
    if subprocess.run(["git", "check-ignore", "-q", patch_dir], cwd=HERE).returncode == 0:
        return set()                       # dist/ 之类：整个目录在仓库之外，全收
    r = subprocess.run(["git", "check-ignore", "--stdin"], input="\n".join(paths),
                       capture_output=True, text=True, cwd=HERE)
    return set(r.stdout.split("\n")) - {""}


# ★★★ 2026-10-01（owner 工单⑦ · **资产二分版**）：`YK_FILES_EXCLUDE=th18/pl01.sht[,th18/pl01.anm]`
#   把指定文件从 files.js 里**摘掉**（= thcrap 不再替换它 ⇒ 游戏读到零售原文件）。
#   机制说明：thcrap 的替换清单**就是本文件生成的 `files.js` + 该文件是否在 patch 目录里**
#   （`refresh()` 用「目录里有没有这个文件」+ 后缀白名单决定条目）。所以二分版 = 文件不进 dist + 不进 files.js。
EXCLUDE = {x for x in os.environ.get("YK_FILES_EXCLUDE", "").split(",") if x}


def refresh(patch_dir):
    cand = []
    for root, _dirs, names in os.walk(patch_dir):
        for name in sorted(names):
            full = os.path.join(root, name)
            rel = os.path.relpath(full, patch_dir).replace(os.sep, "/")
            if rel == "files.js" or not rel.endswith((".js", ".anm", ".ecl", ".wav", ".sht")):
                continue
            if rel in EXCLUDE:                      # ★ 资产二分版：不进清单 = 不替换
                print("  [YK_FILES_EXCLUDE] 跳过 %s（本版不替换它）" % rel)
                continue
            cand.append((rel, full))
    skip = gitignored([f for _, f in cand], patch_dir)
    out = {}
    for rel, full in cand:
        if full in skip:
            continue
        with open(full, "rb") as f:
            out[rel] = zlib.crc32(f.read()) & 0xffffffff
    with open(os.path.join(patch_dir, "files.js"), "w", encoding="utf-8") as f:
        json.dump(dict(sorted(out.items())), f, indent=2, ensure_ascii=False)
        f.write("\n")
    print("%s/files.js:" % os.path.basename(patch_dir), out)


# ── cards.js 静态校验（2026-09-09 加）──────────────────────────────────────
# 为什么要有：DLL 的自检在**运行时**逐卡校验（cards_def.c `ce_card_validate`），任何一张卡不合规
# 就走「mitigation: allocator bound restored to retail (56)」——**整批新卡全部失效**，玩家只看到
# 「改的卡全没了」。2026-09-09 真的踩了：119 的 desc 写了 7 行（上限 6）。所以在**构建期**先查一遍，
# 让它在 make files/dist/release 就红着退出，而不是等实跑。
CARD_TEXT_LINE = 64          # 与 cards_def.h 的 CE_CARD_TEXT_LINE 同步（< 64 字节 UTF-8）
CARD_DESC_LINES = 6          # 与 CE_CARD_DESC_LINES 同步


def _card_text_ok(s):
    return isinstance(s, str) and 0 < len(s.encode("utf-8")) < CARD_TEXT_LINE and "%" not in s


def check_cards(patch_dir):
    path = os.path.join(patch_dir, "th18", "cards.js")
    if not os.path.isfile(path):
        return
    with open(path, encoding="utf-8") as f:
        cards = json.load(f)
    bad = []
    for cid, c in cards.items():
        if not isinstance(c, dict):
            continue
        if not _card_text_ok(c.get("name")):
            bad.append("%s.name" % cid)
        desc = c.get("desc")
        if not isinstance(desc, list) or len(desc) > CARD_DESC_LINES:
            bad.append("%s.desc (>%d lines)" % (cid, CARD_DESC_LINES))
        else:
            for i, line in enumerate(desc):
                if not _card_text_ok(line):
                    bad.append("%s.desc[%d]" % (cid, i))
        for k, lo, hi in (("price_tier", 0, 14), ("weight", 0, 255)):
            v = c.get(k)
            if not isinstance(v, int) or not lo <= v <= hi:
                bad.append("%s.%s" % (cid, k))
        for k in ("sprite_large", "sprite_small"):
            if not isinstance(c.get(k), int):
                bad.append("%s.%s" % (cid, k))
    if bad:
        print("cards.js 校验失败（DLL 会因此把全部新卡禁用！）:", file=sys.stderr)
        for b in bad:
            print("  - " + b, file=sys.stderr)
        sys.exit(1)
    print("cards.js: %d 张卡全部合规（desc<=%d 行、每行<%d 字节、无 ASCII %%）"
          % (len(cards), CARD_DESC_LINES, CARD_TEXT_LINE))


if len(sys.argv) > 1 and sys.argv[1] == "--check":
    for d in ("patch", "patch-test"):
        check_cards(os.path.join(HERE, "..", d))
elif len(sys.argv) > 1:
    for d in sys.argv[1:]:
        check_cards(os.path.abspath(d))
        refresh(os.path.abspath(d))
else:
    for d in ("patch", "patch-test"):
        check_cards(os.path.join(HERE, "..", d))
        refresh(os.path.join(HERE, "..", d))
