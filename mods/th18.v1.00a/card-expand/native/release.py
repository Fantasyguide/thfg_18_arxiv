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
th18/voice/*.wav 与 th18/*.sht（目录镜像：dist 里没有就删）与重建的 th18/abcard.anm / ability.anm / pl0X.anm、_test 的 cards_dev.js、
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
    "patch-step3/th18/default.ecl": "thcrap/repos/Renko_1055/th18_card_expand_255/th18/default.ecl",
    "patch-step3/th18/pl00.anm":   "thcrap/repos/Renko_1055/th18_card_expand_255/th18/pl00.anm",
    "patch-step3/th18/pl01.anm":   "thcrap/repos/Renko_1055/th18_card_expand_255/th18/pl01.anm",
    "patch-step3/th18/pl02.anm":   "thcrap/repos/Renko_1055/th18_card_expand_255/th18/pl02.anm",
    "patch-step3/th18/pl03.anm":   "thcrap/repos/Renko_1055/th18_card_expand_255/th18/pl03.anm",
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
    "patch-step3/th18/ability.txt": "thcrap/repos/Renko_1055/th18_card_expand_255/th18/ability.txt",
    # ★ 2026-10-19：`ability.txt` **不是**空白卡说明的唯一来源 —— base_tsa 的 `gentext#card_desc` 会用
    #   `th18/abilities.js` 把说明 6 行整段换掉（`lang_zh-hans` 自带一份中文原生文本）。我们排在它后面
    #   ⇒ 同一 key 后合并者胜。两个文件必须一起发（详见 assets/derive_ability_txt.py 头注的一手证据）。
    "patch-step3/th18/abilities.js": "thcrap/repos/Renko_1055/th18_card_expand_255/th18/abilities.js",
    "patch-step3/th18/voice.js":  "thcrap/repos/Renko_1055/th18_card_expand_255/th18/voice.js",
    "bin/th18_card_expand.dll":   "mods/th18_card_expand.dll",
}
# 整目录镜像（多出来的文件会被删掉）：语音 wav 是按 ORDER.txt 增删的，逐个写进 MAP 会漏
MAP_DIRS = {
    "patch-step3/th18/voice": ("thcrap/repos/Renko_1055/th18_card_expand_255/th18/voice", (".wav",)),
    # 子机弹幕的 pl0X.sht：有则同步、没有则把 modkit 里的删掉。**必须与 DLL 同批发布**：
    #   108/117/77/119 的子机组、以及 113 的灵界组（文件尾孤儿段 —— 卡侧按
    #   `CE_SHT_TRANCE_DBASE + CE_SHT_TRANCE_ROW_OFF` 直接算行指针）都在这个文件里，
    #   旧 .sht + 新 DLL 会让灵界那一下指到影像之外。见 assets/sht/append_shooterset.py / AUDIT §CJ。
    "patch-step3/th18": ("thcrap/repos/Renko_1055/th18_card_expand_255/th18", (".sht",)),
}
PATCH_DIRS = ("th18_card_expand_255", "th18_card_expand_test", "th18_card_expand_devstage")


def _gate_package_scope():
    """★ 2026-10-15 **同开护栏**：本脚本只许写**自己这三个包**（见 `PATCH_DIRS`）✓。

    起因（owner「同开」口径 ✓）：同一份 TH18 上要同时启用 `card-expand` 与第三方 **leveledit**
    补丁 ✓，而 thcrap 的替换规则是**按相对路径覆盖、后加载者赢** ✗ ⇒ 只要我的发布链把文件写进
    别人的包、或写到与对方同名的路径上，就会静默改坏对方 ✗（`assets/mystia/build_st02.py:6` 记的
    就是这个坑 ✓：「一方坏掉，『只开一个也能用』就不成立」✗）。
    ⇒ 每个发布目标必须落在 `thcrap/repos/Renko_1055/<PATCH_DIRS 之一>/…` 之下 ✓；越界**拒绝发布** ✗。
    ⚠ 对方的**包名/仓库名不写死** ✓：本闸只要求"在我自己这三个包之内" ✓。"""
    dests = list(MAP.values()) + [v[0] for v in MAP_DIRS.values()]
    bad = []
    for dst in dests:
        p = dst.replace("\\", "/").split("/")
        if len(p) < 4 or p[0] != "thcrap" or p[1] != "repos" \
           or p[2] != "Renko_1055" or p[3] not in PATCH_DIRS:
            bad.append(dst)
    if bad:
        raise SystemExit("✗ release.py 的发布目标越界（同开护栏）：\n  " + "\n  ".join(bad))


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


EXCLUDE = {x for x in os.environ.get("YK_FILES_EXCLUDE", "").split(",") if x}


def _gate_contract():
    """★★★ 2026-10-04 **硬闸**（血的教训）：发布前必须过 JSON/行为契约预检。

    为什么必须在这儿拦（而不是只靠 `make check`）：
      · 契约不过时**实跑会 `FAIL: sdk: …` 并「禁用全部新卡」**（contract_check 的原文）；
      · 那一状态下，新卡的**解锁位保护（`unlocked.c` 的 BP_ce_unlock_write）不会生效** ✗
        ⇒ 引擎按原样写 `zScoreFile.unlocked_cards + card_id`，而那个数组**只有 57 字节**
        ⇒ 我们的新卡 id 是 106–117 ⇒ **越界写**，落点正是 **`+0x5f608` = 初始卡组区**
        ⇒ 玩家实报「**悬置的卡号把我存档搞坏了，初始 slot 又消失了**」✓。
      · 而 `release.py` 过去**不跑这个检查** ⇒ 违反契约的构建照样能发出去 ✗（本次事故的直接原因 ✓）。
    ⇒ 所以：**契约不过 = 拒绝发布** ✓。
    """
    # ★★ 2026-10-04 追加：**dist 必须与 patch 同步** —— 本次事故的第二层原因 ✗：
    #   我改了 `patch/th18/cards.js`（category 2→0）却**没先 `make dist`** ⇒ 发出去的是**旧的 dist** ✗
    #   ⇒ modkit 里那份 `cards.js` 仍然没有 category ⇒ 实跑依旧「契约不过 + 禁用全部新卡」✗。
    #   ⇒ 这里直接比 `patch/` 与 `dist/` 的那份 JSON，不一致就拒绝发布 ✓。
    import json as _json
    for rel in ("th18/cards.js",):
        src = os.path.join(HERE, "..", "patch", rel)
        dst = os.path.join(DIST, "patch-step3", rel)
        if not (os.path.exists(src) and os.path.exists(dst)):
            continue
        try:
            a = _json.load(open(src, encoding="utf-8"))
            b = _json.load(open(dst, encoding="utf-8"))
        except Exception as e:                      # noqa: BLE001
            sys.stderr.write("\n❌ 读 %s 失败：%s\n" % (rel, e)); raise SystemExit(4)
        if a != b:
            sys.stderr.write(
                "\n❌ **dist 过期**：`patch/%s` 与 `dist/patch-step3/%s` 不一致 ⇒ 拒绝发布。\n"
                "   先跑 `make dist` 再发布（本次存档事故就是漏了这一步 ✗）。\n" % (rel, rel))
            raise SystemExit(4)

    # ★★★ 2026-10-08 **第三道闸**（本次事故 ✗）：**dist 的构建产物必须与 native/build 的一致** ——
    #   来由：我在 `spirit109.ecl.txt` 里写了 `moveVel($F0, 1.0f)` ✗，thecl 的**往返**把它反编译成
    #   `[-nanf]` ⇒ `make dist` 的 `ecl-ufo` **一直失败** ✗ ⇒ 我那些 `make dist >/dev/null 2>&1 && release.py`
    #   的链子**静默跳过**了重建 ⇒ **dist 里的 ability.anm 一直停在 17:00 的旧件** ✗
    #   ⇒ owner 连续多轮报「**没有区别**」「依旧没有动」✓（我改的全在 anm 里：anchor / resolutionMode / scale ✗）。
    #   ⇒ 这条把「DLL/anm 与 native/build 不一致」直接变成**拒绝发布** ✓。
    for rel, built in (("patch-step3/th18/ability.anm", "build/ability.anm"),
                       ("patch-step3/th18/abcard.anm", "build/abcard.anm"),
                       ("bin/th18_card_expand.dll",     os.path.join("..", "th18_card_expand.dll"))):
        d = os.path.join(DIST, rel)
        b = os.path.join(HERE, built)
        if not (os.path.exists(d) and os.path.exists(b)):
            continue
        import hashlib as _h
        hd = _h.md5(open(d, "rb").read()).hexdigest()
        hb = _h.md5(open(b, "rb").read()).hexdigest()
        if hd != hb:
            sys.stderr.write(
                "\n❌ **dist 里的 %s 与 native/%s 不一致** ⇒ 拒绝发布。\n"
                "   先跑 `make dist`（**并看它的输出** —— 它可能正在失败 ✗；本次事故就是它失败了却被 `>/dev/null` 吞了）。\n"
                "   dist=%s  build=%s\n" % (rel, built, hd[:8], hb[:8]))
            raise SystemExit(5)

    r = subprocess.run([sys.executable, os.path.join(HERE, "tools", "contract_check.py")],
                       cwd=HERE, text=True, capture_output=True)
    if r.returncode != 0:
        sys.stderr.write("\n❌ 契约预检不过 —— **拒绝发布**（实跑会禁用全部新卡并可能写坏存档 ✗）\n")
        sys.stderr.write(r.stdout + r.stderr + "\n")
        raise SystemExit(3)
    print("契约预检 ✅（category ⟺ 主动行为）")


def main():
    _gate_contract()
    _gate_package_scope()
    # ★★★ 2026-10-17（owner 批准 ✓）：**发布前来源校验** —— 与 `tools/deploy.py` **同一套判据** ✓
    #   （实现只留一份 ✓：直接 import 同目录的 `deploy` ✓ —— 抄第二遍必然漂移 ✗）。
    #   事故先例（一手）：2026-10-17 01:04，modkit 的 `th18_card_expand.dll` 从 `600b6d30…`（我 00:58 发的、
    #   基于共享线）被换成 `f502eaae…`（**从落后 12 条的树**构建的、没有当天修复）⇒ owner 实测
    #   「大招还是 360° / 还会甩主机弹 / 无敌还是无限」✗（`AUDIT.md` §BE-DEPLOY-RACE-2026-10-17 ✓）。
    #   ⚠ `release.py` 是**第二条发布通道** ✓（它还会在 modkit 里**自动提交** ✓ ⇒ 落后树更容易把旧件推出去 ✗）
    #   ⇒ 这道闸必须也有 ✓。判据 = `origin/main` 是 HEAD 的祖先 ✓ ⇒ 落后即**拒绝** ✗（见 `deploy.py` ✓）。
    sys.path.insert(0, os.path.join(HERE, "tools"))          # ⚠ `deploy.py` 在 `native/tools/` ✓（不是 `native/` ✗ ——
    import deploy as _dg                                     #    第一版写成 `HERE` 直接报 ModuleNotFoundError ✓）
    _ok_src, _head_c, _main_c = _dg.guard_source(dry=False)
    if not _ok_src:
        raise SystemExit("❌ 来源校验不过（见上）⇒ 拒绝发布 ✗")
    # ★★★ 2026-10-17（② **发布锁** ✓，与 `tools/deploy.py` 同一套 ✓）：同一时刻只许一个发布者写这个 modkit ✓。
    #   先例：00:58 我发 `600b6d30…` ⇒ 01:04 被覆盖 ⇒ owner 实测到旧行为 ✗。
    #   ⚠ 释放用 `atexit` ✓ —— 本函数有**三个出口**（`NO_GIT` 那条 / 「已是最新」那条 / 正常那条 ✓）
    #     外加可能的 `SystemExit`/异常 ✓ ⇒ `atexit` 保证**任何**退出路径都释放 ✓（不必重排缩进 ✓，
    #     也不会把别人的发布卡住 ✗）。`FORCE=1` 可抢占新鲜锁 ✓（会记进台账 ✓）。
    import atexit
    if not _dg.lock_acquire(MODKIT, os.environ.get("FORCE") == "1", False):
        raise SystemExit("❌ 发布锁被占（见上）⇒ 拒绝发布 ✗")
    atexit.register(_dg.lock_release, MODKIT, False)
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
        if os.path.basename(src) in {os.path.basename(x) for x in EXCLUDE}:
            d = os.path.join(MODKIT, dst)                       # ★ 资产二分版：不同步 + 删掉发布仓库里那份
            if os.path.exists(d):
                os.remove(d); changed.append("删除 %s（YK_FILES_EXCLUDE）" % dst)
            continue
        s, d = os.path.join(DIST, src), os.path.join(MODKIT, dst)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        data = open(s, "rb").read()
        if not os.path.exists(d) or open(d, "rb").read().replace(b"\r\n", b"\n") != data.replace(b"\r\n", b"\n"):
            open(d, "wb").write(data)
            changed.append(dst)
    for src, (dst, sufs) in MAP_DIRS.items():
        s_dir, d_dir = os.path.join(DIST, src), os.path.join(MODKIT, dst)
        os.makedirs(d_dir, exist_ok=True)
        want = {n for n in sorted(os.listdir(s_dir))
                if n.endswith(sufs) and n not in {os.path.basename(x) for x in EXCLUDE}}
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
