#!/usr/bin/env python3
"""把 card-expand 的构建产物同步进发布仓库 th18_modkit，并提交。

    make release          # 构建 dist → 同步 → 在 modkit 里提交（不推）
    make release PUSH=1   # 同上，再 push

renkolab 是开发仓库，th18_modkit 是发布仓库（朋友 clone 下来一键启动）。
Windows 那边只负责 `git pull`。

**同步到别的 clone**（例如直接喂 WSL 里能看到的 `D:\…\th18_modkit`）：
`MODKIT_DIR=<那个 clone> NO_GIT=1 make release` —— 只写文件 + 重算 files.js，
不 fetch/pull/commit，也不要求目标仓库工作区干净（`patch.js` / 侧车 `.json` / README 是那边手工维护的，同步集里没有它们）。

只覆盖**生成物**：两个 patch 的 th18.v1.00a.js、三个 patch 的 files.js、DLL、_255 的 th18/cards.js、th18/voice.js 与
th18/voice/*.wav 与 th18/*.sht（目录镜像：dist 里没有就删）与重建的 th18/abcard.anm / ability.anm、_test 的 cards_dev.js、
_devstage 的 patch.js + 六关 ECL（这个 patch 整个以 renkolab 为准）。
patch.js / 侧车 .json / README 是 modkit 里手工维护的文案，这里**不碰**。
"""
import json, os, subprocess, sys, zlib, hashlib

HERE   = os.path.dirname(os.path.abspath(__file__))
REPO   = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
DIST   = os.path.join(HERE, "..", "dist")
MODKIT = os.environ.get("MODKIT_DIR") or os.path.join(REPO, "local", "vendor", "th18_modkit")
R      = os.path.join(MODKIT, "thcrap", "repos", "Renko_1055")
NO_GIT = os.environ.get("NO_GIT") == "1"   # 只写文件 + 重算 files.js，不 fetch/pull/commit

MAP = {                                   # dist 里的 → modkit 里的
    "patch-step3/th18.v1.00a.js": "thcrap/repos/Renko_1055/th18_card_expand_255/th18.v1.00a.js",
    "patch-step3/th18/cards.js":  "thcrap/repos/Renko_1055/th18_card_expand_255/th18/cards.js",
    "patch-step3/th18/abcard.anm": "thcrap/repos/Renko_1055/th18_card_expand_255/th18/abcard.anm",
    "patch-step3/th18/ability.anm": "thcrap/repos/Renko_1055/th18_card_expand_255/th18/ability.anm",
    "patch-test/th18.v1.00a.js":  "thcrap/repos/Renko_1055/th18_card_expand_test/th18.v1.00a.js",
    "patch-test/th18/cards_dev.js": "thcrap/repos/Renko_1055/th18_card_expand_test/th18/cards_dev.js",
    "patch-devstage/patch.js":      "thcrap/repos/Renko_1055/th18_card_expand_devstage/patch.js",   # 这个 patch 的 patch.js 以 renkolab 为准
    "patch-devstage/th18/st01.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st01.ecl",
    "patch-devstage/th18/st01bs.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st01bs.ecl",
    "patch-devstage/th18/st02.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st02.ecl",
    "patch-devstage/th18/st02bs.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st02bs.ecl",
    "patch-devstage/th18/st03.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st03.ecl",
    "patch-devstage/th18/st03bs.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st03bs.ecl",
    "patch-devstage/th18/st04.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st04.ecl",
    "patch-devstage/th18/st04bs.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st04bs.ecl",
    "patch-devstage/th18/st05.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st05.ecl",
    "patch-devstage/th18/st05bs.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st05bs.ecl",
    "patch-devstage/th18/st06.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st06.ecl",
    "patch-devstage/th18/st06bs.ecl": "thcrap/repos/Renko_1055/th18_card_expand_devstage/th18/st06bs.ecl",
    "patch-step3/th18/voice.js":  "thcrap/repos/Renko_1055/th18_card_expand_255/th18/voice.js",
    "bin/th18_card_expand.dll":   "mods/th18_card_expand.dll",
}
# 整目录镜像（多出来的文件会被删掉）：语音 wav 是按 ORDER.txt 增删的，逐个写进 MAP 会漏
MAP_DIRS = {
    "patch-step3/th18/voice": ("thcrap/repos/Renko_1055/th18_card_expand_255/th18/voice", (".wav",)),
    # 子机弹幕的 pl0X.sht：有则同步、没有则把 modkit 里的删掉（目前没有卡在用，见 assets/sht/append_shooterset.py）
    "patch-step3/th18": ("thcrap/repos/Renko_1055/th18_card_expand_255/th18", (".sht",)),
}
PATCH_DIRS = ("th18_card_expand_255", "th18_card_expand_test", "th18_card_expand_devstage")


def blob_bytes(data):
    """files.js 的 crc 必须等于 **thcrap 实际下载到的字节** = git blob。

    modkit 的 `.gitattributes` 是 `* text=auto eol=crlf` ⇒ Windows 工作区里文本文件是 CRLF，
    但 blob 里是 LF；二进制文件（含 NUL）原样存。所以这里照 git 的 text=auto 口径：
    **前 8000 字节里没有 NUL 就按文本处理（CRLF→LF），否则原样**。

    ⚠️ 2026-09-10 踩坑：原来直接 crc32(工作区字节) ⇒ 在 CRLF 工作区里算出的 crc 与 blob 不符，
    `_devstage/patch.js` 因此被写成错的 crc（上游那份是对的 2243114325，我们写成 796875985）。"""
    if b"\0" in data[:8000]:
        return data
    return data.replace(b"\r\n", b"\n")


def sh(*args, cwd=None, check=True):
    return subprocess.run(args, cwd=cwd, check=check, text=True, capture_output=True).stdout.strip()


def main():
    push = os.environ.get("PUSH") == "1"
    if not NO_GIT and not os.path.isdir(os.path.join(MODKIT, ".git")):
        raise SystemExit("找不到发布仓库：%s\n先 git clone https://github.com/Renko6626/th18_modkit.git 到 local/vendor/" % MODKIT)
    for src in MAP:
        if not os.path.exists(os.path.join(DIST, src)):
            raise SystemExit("dist 不完整，缺 %s —— 先 make dist" % src)
    for src in MAP_DIRS:
        if not os.path.isdir(os.path.join(DIST, src)):
            raise SystemExit("dist 不完整，缺目录 %s —— 先 make dist" % src)

    # 发布仓库必须干净、且在 main 的最新（NO_GIT 模式跳过：那是「同步到别人/别的 clone」）
    if not NO_GIT:
        if sh("git", "status", "--porcelain", cwd=MODKIT):
            raise SystemExit("th18_modkit 工作区不干净，先处理掉再发布")
        sh("git", "fetch", "-q", "origin", cwd=MODKIT)
        behind = sh("git", "rev-list", "--count", "HEAD..origin/main", cwd=MODKIT)
        if behind != "0":
            sh("git", "pull", "-q", "--ff-only", "origin", "main", cwd=MODKIT)
            print("th18_modkit：已快进 %s 个远端提交" % behind)

    changed = []
    for src, dst in MAP.items():
        s, d = os.path.join(DIST, src), os.path.join(MODKIT, dst)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        data = open(s, "rb").read()
        if not os.path.exists(d) or open(d, "rb").read().replace(b"\r\n", b"\n") != data.replace(b"\r\n", b"\n"):
            open(d, "wb").write(data)
            changed.append(dst)
    for src, (dst, sufs) in MAP_DIRS.items():
        s_dir, d_dir = os.path.join(DIST, src), os.path.join(MODKIT, dst)
        os.makedirs(d_dir, exist_ok=True)
        want = {n for n in sorted(os.listdir(s_dir)) if n.endswith(sufs)}
        for n in sorted(want):
            data = open(os.path.join(s_dir, n), "rb").read()
            d = os.path.join(d_dir, n)
            if not os.path.exists(d) or open(d, "rb").read() != data:
                open(d, "wb").write(data)
                changed.append("%s/%s" % (dst, n))
        for n in sorted(os.listdir(d_dir)):          # 镜像：dist 里没有了的删掉
            if n.endswith(sufs) and n not in want:
                os.remove(os.path.join(d_dir, n))
                changed.append("删除 %s/%s" % (dst, n))
    for p in PATCH_DIRS:
        d = os.path.join(R, p)
        out = {}
        for root, _dirs, names in os.walk(d):           # 递归：th18/cards.js 这类子目录文件也要进清单
            for n in sorted(names):
                rel = os.path.relpath(os.path.join(root, n), d).replace(os.sep, "/")
                if rel.endswith((".js", ".anm", ".ecl", ".wav", ".sht")) and rel != "files.js":
                    out[rel] = zlib.crc32(blob_bytes(open(os.path.join(root, n), "rb").read())) & 0xffffffff
        out = dict(sorted(out.items()))
        f = os.path.join(d, "files.js")
        txt = json.dumps(out, indent=2) + "\n"
        if not os.path.exists(f) or open(f, encoding="utf-8").read().replace("\r\n", "\n") != txt:
            open(f, "w", encoding="utf-8").write(txt)
            changed.append("thcrap/repos/Renko_1055/%s/files.js" % p)

    if NO_GIT:
        print("NO_GIT=1：已写入 %d 处改动（未提交、未推送）" % len(changed))
        for c in changed:
            print("  " + c)
        return 0
    if not sh("git", "status", "--porcelain", cwd=MODKIT):
        print("th18_modkit 已是最新，无需提交")
        return 0
    dll_md5 = hashlib.md5(open(os.path.join(DIST, "bin/th18_card_expand.dll"), "rb").read()).hexdigest()[:8]
    src_sha = sh("git", "rev-parse", "--short", "HEAD", cwd=REPO)
    msg = ("card-expand: 同步构建产物（renkolab %s）\n\n"
           "DLL md5 %s…\n改动：\n  %s\n\n由 renkolab mods/th18.v1.00a/card-expand/native/release.py 生成。"
           % (src_sha, dll_md5, "\n  ".join(changed)))
    sh("git", "add", "-A", cwd=MODKIT)
    sh("git", "-c", "user.name=Renko6626", "-c", "user.email=renko6626@gmail.com",
       "commit", "-q", "-m", msg, cwd=MODKIT)
    print("th18_modkit 已提交：%s" % sh("git", "log", "--oneline", "-1", cwd=MODKIT))
    if push:
        print(sh("git", "push", "origin", "main", cwd=MODKIT, check=False) or "已推送")
        print("origin/main =", sh("git", "log", "--oneline", "-1", "origin/main", cwd=MODKIT))
    else:
        print("未推送。要推：make release PUSH=1")
    return 0


if __name__ == "__main__":
    sys.exit(main())
