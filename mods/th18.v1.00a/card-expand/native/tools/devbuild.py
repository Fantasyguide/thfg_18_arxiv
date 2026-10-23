#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""devbuild.py —— ★ owner 的唯一构建/发布入口：**一条命令跑完「校验 → 构建 → 发布 → 回读 → 汇总」**。

    python3 tools/devbuild.py --modkit "/path/to/th18_modkit"
    python3 tools/devbuild.py --modkit "..." --skip-build      # 只校验 + 发布已构建好的产物
    python3 tools/devbuild.py --modkit "..." --allow-dirty     # 明确放行未提交的源文件
    python3 tools/devbuild.py --modkit "..." --full            # 额外核对发布 zip 与 modkit 同批

等价的 make 入口（推荐 owner 用这条）：

    make dev MODKIT_DIR="/path/to/th18_modkit"

为什么要有它（**全是今天的一手事故** ✗）：
  · 109 的 `desc` 写到第 7 行 ⇒ DLL 运行期自检失败 ⇒ `start_deck: disabled` +
    `mitigation: allocator bound restored to retail (56)` ⇒ **所有 mod 卡从游戏里消失** ✗；
  · 201 调试卡 `deck_visible` 还是 1 ⇒ 调试卡进商店/图鉴/指南 ✗；
  · 发布包里 BGM（wav）是旧批次 ⇒ 玩家听到旧音乐 ✗；
  · 指南 `guide.tex` 带着冲突标记（连续 7 个 `<`）被编译 ✗。
  ⇒ 这些都不是「写代码时能记住」的事，只能靠**构建期闸**。本脚本把闸串成一条链，
    **任何一步失败立即退出**，并打印「哪一步、为什么、怎么修」。

设计约束：
  · 纯 Python 3 标准库，无第三方依赖；
  · **不写死任何机器上的路径**：modkit 由 `--modkit` / `MODKIT_DIR` 传入；
  · 只读的校验在前，写动作（DLL / 发布）在后；
  · 发布只走 `tools/deploy.py`（唯一被认可的发布路径），本脚本不再手写第二套复制逻辑。

退出码：0 = 全绿；1 = 被闸拦下；2 = 用法错误（例如没给 --modkit）。
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import zipfile
import zlib

# ── 路径：全部从本文件位置推出来，不写死本机路径 ──────────────────────────────
HERE = os.path.dirname(os.path.abspath(__file__))          # …/card-expand/native/tools
NATIVE = os.path.dirname(HERE)                             # …/card-expand/native
MOD = os.path.dirname(NATIVE)                              # …/card-expand
REPO = os.path.dirname(os.path.dirname(os.path.dirname(MOD)))   # 仓库根
PATCH = os.path.join(MOD, "patch")
CARDS = os.path.join(PATCH, "th18", "cards.js")
FILES_JS = os.path.join(PATCH, "files.js")
GUIDE_MD = os.path.join(MOD, "PLAYER-GUIDE.md")
GUIDE_TOOL = os.path.join(MOD, "tools", "make_guide.py")
CARD_IMAGES = os.path.join(MOD, "assets", "cards")
DLL = os.path.join(NATIVE, "th18_card_expand.dll")
GUIDE_DIR = os.path.join(NATIVE, "build", "guide")
ZIP_DEFAULT = os.path.join(MOD, "dist", "th18_card_expand_bundle.zip")

PKG = "th18_card_expand_255"
PKG_REL = os.path.join("thcrap", "repos", "Renko_1055", PKG)

CARD_DESC_LINES = 6          # 与 native/cards_def.h 的 CE_CARD_DESC_LINES 同步
CARD_TEXT_LINE = 64          # 与 native/cards_def.h 的 CE_CARD_TEXT_LINE 同步
# 调试 / 测试卡：internal_name 命中这些前缀的就是「不许被玩家看到」的卡
DEBUG_RE = re.compile(r"^(?:DEBUG_DEL|DEBUG_.*|TEST_.*)$", re.I)
# ★ 调试卡三项判据（按**商店机制**，不是「全 0」）：
#   initial_unlocked=0（新档不解禁）、deck_visible=0（编成菜单不可见）、
#   weight=6（CE_SHOP_NEVER_WEIGHT：永不进商店随机池）。
#   ⚠ weight=0 **不是**「不出现」——0 是商店 loop1 的「必上架」哨兵（每关必出）✗。
DEBUG_FLAGS = (("initial_unlocked", 0), ("deck_visible", 0), ("weight", 6))

# 哪些未跟踪文件算「源文件」⇒ 默认闸会拦。构建产物（.o/.dll/.anm/.sht/.wav/图片）不算。
SOURCE_EXTS = {
    ".c", ".h", ".cc", ".cpp", ".cxx", ".hpp", ".hh", ".py", ".js", ".ts",
    ".sh", ".bash", ".md", ".json", ".txt", ".def", ".mk", ".yml", ".yaml",
    ".toml", ".cfg", ".ini", ".tex", ".ecl", ".sht", ".anm",
}

C_OK, C_BAD, C_WARN, C_DIM, C_END = "\033[32m", "\033[31m", "\033[33m", "\033[2m", "\033[0m"


def color(s, c):
    return s if not sys.stdout.isatty() else c + s + C_END


# ══════════════════════════════════════════════════════════════════════════════
# 基础工具
# ══════════════════════════════════════════════════════════════════════════════
def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def crc32_blob(path):
    """按 thcrap 下载到的字节算 crc32（与 release.py 的 `blob_bytes` 同口径）。

    modkit 的 `.gitattributes` 是 `* text=auto eol=crlf`：工作区可能是 CRLF，
    但 thcrap 下载到的是 git blob（LF）。前 8000 字节没有 NUL 就按文本处理（CRLF→LF）。
    """
    with open(path, "rb") as f:
        data = f.read()
    if b"\0" not in data[:8000]:
        data = data.replace(b"\r\n", b"\n")
    return zlib.crc32(data) & 0xffffffff


class StepFail(Exception):
    """一个步骤失败：message = 一句话结论；output = 命令输出 / 明细（会打印出来）。"""

    def __init__(self, message, output=""):
        super().__init__(message)
        self.message = message
        self.output = output


def sh(cmd, cwd=NATIVE, env=None):
    """跑一条命令；非零退出抛 StepFail（保留完整输出给失败报告）。"""
    try:
        p = subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
    except FileNotFoundError as e:
        raise StepFail("命令不存在：%s" % cmd[0], str(e))
    out = ((p.stdout or "") + (p.stderr or "")).strip()
    if p.returncode != 0:
        raise StepFail("命令失败（rc=%d）：%s" % (p.returncode, " ".join(cmd)), out)
    return p.stdout or "", out


def tail(text, n=40):
    lines = [l for l in (text or "").splitlines() if l.strip()]
    return "\n".join(lines[-n:])


def last_line(text):
    """最后一条有用输出。

    ⚠ 从 `make dev` 里调用时，子 make 会继承 MAKEFLAGS，末尾多出
    `make[1]: Leaving directory …` ⇒ 汇总表的「备注」会变成这句噪声。
    这里跳过 make 的进出目录行，只取真正的结论行。
    """
    lines = [l.strip() for l in (text or "").splitlines() if l.strip()]
    for line in reversed(lines):
        if re.match(r"^make\[\d+\]:\s+(Entering|Leaving) directory", line):
            continue
        if line.startswith("make: "):
            continue
        return line
    return ""


def disp_width(s):
    """CJK 字符在终端占两格 —— 汇总表对齐用。"""
    import unicodedata
    return sum(2 if unicodedata.east_asian_width(ch) in ("W", "F") else 1 for ch in s)


def pad(s, width):
    return s + " " * max(0, width - disp_width(s))


# ══════════════════════════════════════════════════════════════════════════════
# 卡表：调试卡不可见 + 109/201 关键项
# ══════════════════════════════════════════════════════════════════════════════
def load_cards(path):
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
    except FileNotFoundError:
        raise StepFail("找不到卡表：%s" % path,
                       "确认 patch/th18/cards.js 存在；本闸只读，不会生成它。")
    except json.JSONDecodeError as e:
        raise StepFail("cards.js 不是合法 JSON：%s" % e,
                       "先修 JSON 语法（常见：多/少逗号、尾逗号、注释）。")
    if not isinstance(data, dict):
        raise StepFail("cards.js 顶层不是对象（应是从卡号到卡定义的字典）", repr(type(data)))
    return data


def debug_cards(cards):
    """返回 [(id, card)]：internal_name 命中调试/测试禁用名单的卡。"""
    out = []
    for cid, c in cards.items():
        if isinstance(c, dict) and DEBUG_RE.match(str(c.get("internal_name", ""))):
            out.append((str(cid), c))
    return out


def _flag(c, key):
    v = c.get(key)
    return v if isinstance(v, int) and not isinstance(v, bool) else None


def check_debug_visibility(cards):
    """★★ 新闸：调试/测试卡必须 initial_unlocked=0、deck_visible=0、weight=6。

    判据按**商店机制**写（一手见 native/cards_def.h:98-105、
    engine/card/th18/05-shop-and-money.md §4；旧闸「三项全 0」与机制相反 ✗）：
      · 商店随机池 = `+0x14 ∉ {0, 6}` 按权重压份 ⇒ weight=6（CE_SHOP_NEVER_WEIGHT）
        = 永不进随机池 ✓；
      · weight=0 是 loop1 的「必上架」哨兵 ⇒ 每关都上架 ✗，还会挤占 guaranteed 名额；
    """
    bad = []
    found = debug_cards(cards)
    for cid, c in found:
        row = []
        for key, want in DEBUG_FLAGS:
            v = _flag(c, key)
            if v != want:
                row.append("%s=%s（必须=%d）" % (key, "缺失" if v is None else v, want))
        if row:
            bad.append("  · 卡 %s（internal_name=%s）：%s"
                       % (cid, c.get("internal_name"), " / ".join(row)))
    if bad:
        raise StepFail(
            "有 %d 张调试/测试卡不符合「不可见」判据" % len(bad),
            "\n".join(bad) + "\n\n"
            "判定：cards.js 里 internal_name 命中 DEBUG_DEL / DEBUG_* / TEST_* 的卡，\n"
            "必须 initial_unlocked=0、deck_visible=0、weight=6（缺字段按不合规处理）。\n"
            "★ weight 只认 6：6 = CE_SHOP_NEVER_WEIGHT ⇒ 退出商店随机池，永远不上架 ✓；\n"
            "  weight=0 不是「不出现」，而是 loop1 的「必上架」哨兵 ⇒ 每次进店都会上架 ✗。")
    return found


def card_109_lines(cards):
    c = cards.get("109")
    if not isinstance(c, dict):
        raise StepFail("cards.js 里找不到 109（花映塚）",
                       "确认卡表里有 id 109；本闸只读，不会补卡。")
    desc = c.get("desc")
    if not isinstance(desc, list):
        raise StepFail("109 的 desc 不是数组", repr(desc))
    return len(desc)


def card_201_flags(cards):
    c = cards.get("201")
    if not isinstance(c, dict):
        raise StepFail("cards.js 里找不到 201（调试・定向删卡）",
                       "确认卡表里有 id 201；本闸只读，不会补卡。")
    return {k: _flag(c, k) for k, _want in DEBUG_FLAGS}


# ══════════════════════════════════════════════════════════════════════════════
# 汇总表 / 失败报告
# ══════════════════════════════════════════════════════════════════════════════
ROWS = []          # [(n, label, status, seconds, detail)]
T0 = time.perf_counter()


def add_row(label, status, dt, detail=""):
    ROWS.append((len(ROWS) + 1, label, status, dt, detail))


def print_table():
    print()
    print("═" * 96)
    print("  PASS/FAIL 汇总（%s）" % time.strftime("%Y-%m-%d %H:%M:%S"))
    print("═" * 96)
    print("  %s %s %s %s  %s"
          % (pad("#", 3), pad("步骤", 58), pad("结果", 7), pad("耗时", 8), "备注"))
    print("  " + "-" * 94)
    for n, label, status, dt, detail in ROWS:
        mark = {"PASS": "✓ PASS", "FAIL": "✗ FAIL", "SKIP": "⤼ SKIP"}.get(status, status)
        mark_c = color(pad(mark, 7), C_OK if status == "PASS"
                       else (C_BAD if status == "FAIL" else C_WARN))
        print("  %02d  %s %s %8.2fs  %s"
              % (n, pad(label, 58), mark_c, dt, (detail or "")[:100]))
    print("  " + "-" * 94)
    print("  合计：%d 步，%.2fs" % (len(ROWS), time.perf_counter() - T0))
    print("═" * 96)


def run_step(label, why, fix, fn):
    """跑一步：成功打 ✓；失败打 ✗ + 为什么 + 怎么修，然后立即退出。"""
    n = len(ROWS) + 1
    print("[%02d] %s ..." % (n, pad(label, 62)), end="", flush=True)
    t = time.perf_counter()
    try:
        detail = fn() or ""
    except StepFail as e:
        fail = e
    except Exception as e:                      # 闸自己出 bug 也要按「哪一步/为什么/怎么修」报
        import traceback
        fail = StepFail("内部异常：%s: %s" % (type(e).__name__, e), traceback.format_exc())
    else:
        dt = time.perf_counter() - t
        print("%s %6.2fs  %s" % (color("✓", C_OK), dt, (detail or "")[:110]), flush=True)
        add_row(label, "PASS", dt, detail)
        return detail
    # ── 失败路径（只有异常会走到这里）──────────────────────────────────
    dt = time.perf_counter() - t
    print("%s %6.2fs" % (color("✗", C_BAD), dt), flush=True)
    add_row(label, "FAIL", dt)
    print()
    print(color("✗ 第 %02d 步失败：%s" % (n, label), C_BAD))
    print("  结论：%s" % fail.message)
    if fail.output:
        print()
        print("  ── 输出（末尾 %d 行）────────────────────────────────────────────" % 40)
        for line in tail(fail.output, 40).splitlines():
            print("  " + line)
        print("  " + "─" * 74)
    print()
    print("  为什么：%s" % why)
    print("  怎么修：%s" % fix)
    print()
    print_table()
    sys.exit(1)


def skip_step(label, note):
    n = len(ROWS) + 1
    print("[%02d] %s ... %s  %s" % (n, pad(label, 62), color("⤼ SKIP", C_WARN), note))
    add_row(label, "SKIP", 0.0, note)


# ══════════════════════════════════════════════════════════════════════════════
# 阶段 A：源码前置校验
# ══════════════════════════════════════════════════════════════════════════════
def step_cards_check():
    _, out = sh(["make", "cards-check"])
    return last_line(out)


def step_debug_visibility():
    cards = load_cards(CARDS)
    found = check_debug_visibility(cards)
    if not found:
        return "未发现 DEBUG_*/TEST_* 卡（0 张）"
    return "%d 张调试/测试卡全不可见（init=0 / deck=0 / weight=6）：%s" % (
        len(found), "、".join("%s(%s)" % (cid, c.get("internal_name")) for cid, c in found))


def step_sites_check():
    _, out = sh([sys.executable, "sites.py", "check"])
    return last_line(out)


def step_gen_def():
    _, out = sh([sys.executable, "tools/gen_def.py", "--check"])
    return last_line(out)


def step_gen_drift():
    _, out = sh([sys.executable, "tools/gen_drift.py"])
    return last_line(out)


def step_contract():
    _, out = sh([sys.executable, "tools/contract_check.py"])
    return last_line(out)


def step_test_host():
    _, out = sh(["make", "test-host"])
    return last_line(out)


def step_git_branch():
    out, _ = sh(["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=REPO)
    branch = out.strip()
    if branch == "main":
        raise StepFail("当前分支是 main —— 禁止在 main 上构建/发布",
                       "当前分支 = main\n"
                       "本仓纪律：main 只接收合并；开发与发布都在特性分支上。")
    if branch == "HEAD":
        raise StepFail("当前是 detached HEAD —— 没有分支名，不能安全发布",
                       "git rev-parse --abbrev-ref HEAD = HEAD")
    return "branch=%s" % branch


def step_git_unmerged():
    out, _ = sh(["git", "-c", "core.quotepath=false", "status", "--porcelain"], cwd=REPO)
    unmerged = [l for l in out.splitlines()
                if len(l) >= 2 and l[:2] in {"UU", "AA", "DD", "AU", "UA", "DU", "UD"}]
    if unmerged:
        raise StepFail("工作区有 %d 个未解决冲突（UU/AA/DD/…）" % len(unmerged),
                       "\n".join(unmerged[:20]))
    return "无冲突（无 UU/AA/DD/…）"


def _is_source_path(path):
    p = path.strip().replace("\\", "/")
    if p.startswith('"') and p.endswith('"'):
        p = p[1:-1]
    parts = p.split("/")
    if any(part in ("build", "dist", "__pycache__", ".pytest_cache", ".git") for part in parts):
        return False
    if os.path.basename(p) in ("Makefile", "makefile", "GNUmakefile"):
        return True
    return os.path.splitext(p)[1].lower() in SOURCE_EXTS


def step_git_dirty(allow_dirty):
    out, _ = sh(["git", "-c", "core.quotepath=false", "status", "--porcelain",
                 "--untracked-files=all"], cwd=REPO)
    lines = [l for l in out.splitlines() if l.strip()]
    tracked = [l for l in lines if not l.startswith("??")]
    untracked_src = [l for l in lines if l.startswith("??") and _is_source_path(l[3:])]
    dirty = tracked + untracked_src
    if not dirty:
        return "工作区干净（源文件都已 commit）"
    detail = "\n".join(dirty[:20])
    if allow_dirty:
        return "⚠ --allow-dirty 放行 %d 个未提交源文件" % len(dirty)
    raise StepFail("有 %d 个未提交的源文件（tracked %d / untracked %d）"
                   % (len(dirty), len(tracked), len(untracked_src)),
                   detail + "\n\n"
                   "判定：tracked 的改动一律算；untracked 只算源码/文档类扩展名"
                   "（.o/.dll/.anm/.sht/.wav/图片等构建产物不算）。\n"
                   "放行：确认这些改动不该进本次构建时加 `--allow-dirty`。")


def step_check_docs():
    _, out = sh([sys.executable, os.path.join(REPO, "tooling", "check-docs.py")], cwd=REPO)
    return last_line(out)


def step_conflicts():
    """冲突标记（连续 7 个 `<` 或 `>`）必须 0（只扫文本文件；二进制里的随机字节不算）。"""
    hits = []
    patterns = ("<" * 7, ">" * 7)
    for dirpath, dirs, names in os.walk(os.path.join(REPO, "mods")):
        dirs[:] = [d for d in dirs if d not in (".git", "__pycache__", ".pytest_cache")]
        for name in names:
            p = os.path.join(dirpath, name)
            try:
                if os.path.getsize(p) > 32 * 1024 * 1024:
                    continue
                with open(p, "rb") as f:
                    data = f.read()
            except OSError:
                continue
            if b"\0" in data[:8000]:
                continue                          # 二进制（anm/sht/dll/png…）不扫
            try:
                text = data.decode("utf-8")
            except UnicodeDecodeError:
                continue
            for i, line in enumerate(text.splitlines(), 1):
                if any(pat in line for pat in patterns):
                    rel = os.path.relpath(p, REPO)
                    hits.append("%s:%d: %s" % (rel, i, line.strip()[:100]))
                    break
    if hits:
        raise StepFail("发现 %d 个文件带冲突标记（连续 7 个 < 或 >）" % len(hits),
                       "\n".join(hits[:30]))
    return "0 个文本文件带冲突标记"


# ══════════════════════════════════════════════════════════════════════════════
# 阶段 A5：指南
# ══════════════════════════════════════════════════════════════════════════════
def _prepare_guide_dir():
    os.makedirs(GUIDE_DIR, exist_ok=True)
    link = os.path.join(GUIDE_DIR, "cards")
    if not os.path.exists(link):
        try:
            os.symlink("../../../assets/cards", link, target_is_directory=True)
        except (OSError, NotImplementedError):
            # Windows 无符号链接权限：把绝对路径交给 make_guide.py。
            # LaTeX 的 \includegraphics 吃正斜杠，不吃反斜杠 ⇒ 统一转成 "/"。
            return str(CARD_IMAGES).replace("\\", "/")
    return "cards"


def _font_env():
    """WSL 上默认的 `Microsoft YaHei` 来自 `/mnt/c/Windows/Fonts`；给它一个 fontconfig 映射。

    不写死本机路径的替代：只有 `/mnt/c/Windows/Fonts` 真的存在时才加这一条；
    其它机器继续用系统 fontconfig（或用户自己设的 `FONTCONFIG_FILE`）。
    """
    env = os.environ.copy()
    win_fonts = "/mnt/c/Windows/Fonts"
    if os.path.isdir(win_fonts) and not env.get("FONTCONFIG_FILE"):
        conf = os.path.join(GUIDE_DIR, "fonts.conf")
        with open(conf, "w", encoding="utf-8") as f:
            f.write(
                '<?xml version="1.0"?>\n'
                '<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">\n'
                "<fontconfig>\n"
                '  <include ignore_missing="yes">/etc/fonts/fonts.conf</include>\n'
                "  <dir>%s</dir>\n"
                "  <cachedir>%s</cachedir>\n"
                "</fontconfig>\n" % (win_fonts, os.path.join(GUIDE_DIR, "fontcache")))
        env["FONTCONFIG_FILE"] = conf
    return env


def step_guide_tex():
    imgarg = _prepare_guide_dir()
    for name in ("guide.tex", "guide.pdf", "guide.aux", "guide.log", "guide.out", "guide.toc"):
        try:
            os.remove(os.path.join(GUIDE_DIR, name))
        except OSError:
            pass
    _, out = sh([sys.executable, GUIDE_TOOL, CARDS, GUIDE_MD, "guide.tex", imgarg], cwd=GUIDE_DIR)
    tex_path = os.path.join(GUIDE_DIR, "guide.tex")
    if not os.path.isfile(tex_path):
        raise StepFail("make_guide.py 跑完了但没写出 guide.tex", out)
    with open(tex_path, encoding="utf-8") as f:
        tex = f.read()

    problems = []
    cards = load_cards(CARDS)
    # ★ 今天踩过的那一条：调试・定向删卡（201）不许出现
    if "调试・定向删卡" in tex:
        problems.append("guide.tex 里出现「调试・定向删卡」（201 调试卡进了玩家图册）")
    for cid, c in debug_cards(cards):
        name = str(c.get("internal_name", ""))
        if name and name in tex:
            problems.append("guide.tex 里出现调试卡内部名 %s（卡 %s）" % (name, cid))
        if re.search(r"\\cardtitle\{\s*%s(?:\s|（|\})" % re.escape(cid), tex):
            problems.append("guide.tex 里有调试卡的标题 \\cardtitle{%s …}" % cid)
    if problems:
        raise StepFail("指南里混进了调试/测试卡（%d 处）" % len(problems),
                       "\n".join("  · " + p for p in problems) + "\n\n"
                       "修法：把该卡加进 tools/make_guide.py 的 skip（或删掉 PLAYER-GUIDE.md 里它的 ### 节），"
                       "再重跑本命令。")
    n_titles = len(re.findall(r"\\cardtitle\{", tex))
    if n_titles == 0:
        raise StepFail("guide.tex 里一张卡的标题都没有（生成器没产出内容）", out)
    return "tex 已生成：%d 张卡、调试卡 0" % n_titles


def step_guide_pdf(skip_pdf):
    if skip_pdf:
        raise StepFail("内部调用错误：--skip-guide-pdf 时不应进入本步")
    if not shutil.which("xelatex"):
        raise StepFail("找不到 xelatex（无法编译指南 PDF）",
                       "装 TeX Live（含 XeLaTeX + xeCJK），或本机只做代码闸时加 --skip-guide-pdf。")
    env = _font_env()
    pages = None
    for i in (1, 2):
        p = subprocess.run(["xelatex", "-interaction=nonstopmode", "-halt-on-error", "guide.tex"],
                           cwd=GUIDE_DIR, env=env, capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        out = ((p.stdout or "") + (p.stderr or "")).strip()
        log_path = os.path.join(GUIDE_DIR, "guide.log")
        log = ""
        if os.path.isfile(log_path):
            with open(log_path, encoding="utf-8", errors="replace") as f:
                log = f.read()
        blob = out + "\n" + log
        if p.returncode != 0:
            raise StepFail("xelatex 第 %d 遍失败（rc=%d）" % (i, p.returncode), tail(blob, 40))
        m = re.search(r"Output written on .*?\((\d+) pages?", blob)
        if not m:
            raise StepFail("xelatex 第 %d 遍没输出页数（找不到 'Output written … (N pages)'）" % i,
                           tail(blob, 40))
        pages = int(m.group(1))
    return "pdf %d 页" % pages


# ══════════════════════════════════════════════════════════════════════════════
# 阶段 B：构建 + 发布 + 回读
# ══════════════════════════════════════════════════════════════════════════════
def step_dll():
    # 与 deploy.py 的强制重链同理：sites_gen.h / .def 变了 make 未必知道
    try:
        os.remove(DLL)
    except OSError:
        pass
    sh(["make", "dll"])
    return "DLL md5 %s…" % md5(DLL)[:10]


def step_deploy(modkit):
    if not modkit:
        raise StepFail("没有给 modkit", "本脚本不猜路径：用 --modkit <dir> 或环境变量 MODKIT_DIR 传入。")
    if not os.path.isdir(modkit):
        raise StepFail("modkit 目录不存在：%s" % modkit,
                       "确认路径拼写；这是发布目标仓库（th18_modkit）的根目录，不是本仓。")
    sh([sys.executable, "tools/deploy.py", "--modkit", modkit, "--skip-build", "--force"])
    return "deploy 完成（patch + DLL + 资产）"


def _deployed(modkit, rel):
    return os.path.join(modkit, rel)


def step_readback_md5(modkit):
    """部署件与源件逐个比 md5（deploy 已做，这里独立再验一遍关键项）。"""
    checks = [
        (CARDS, _deployed(modkit, os.path.join(PKG_REL, "th18", "cards.js")), "cards.js"),
        (FILES_JS, _deployed(modkit, os.path.join(PKG_REL, "files.js")), "files.js"),
        (DLL, _deployed(modkit, os.path.join("mods", "th18_card_expand.dll")), "DLL(mods)"),
        (DLL, _deployed(modkit, os.path.join("thcrap", "bin", "mod_th18_card_expand.dll")), "DLL(bin)"),
        (DLL, _deployed(modkit, os.path.join(PKG_REL, "th18_card_expand.dll")), "DLL(pkg)"),
    ]
    bad = []
    for src, dst, label in checks:
        want = md5(src)
        if not os.path.isfile(dst):
            bad.append("  · %-9s 缺失：%s" % (label, dst))
            continue
        got = md5(dst)
        if got != want:
            bad.append("  · %-9s md5 不一致：部署 %s ≠ 源 %s\n      %s" % (label, got, want, dst))
    if bad:
        raise StepFail("部署件与源件不一致（%d 项）" % len(bad), "\n".join(bad))
    return "cards.js / files.js / DLL×3 全部同 md5"


def step_readback_cards(modkit):
    """★ 从**部署件**读 109 desc 行数 / 201 三项，不看源。"""
    dep = _deployed(modkit, os.path.join(PKG_REL, "th18", "cards.js"))
    if not os.path.isfile(dep):
        raise StepFail("部署件 cards.js 不存在：%s" % dep,
                       "deploy 没有把数据文件发过去；重跑 deploy 并看它的输出。")
    cards = load_cards(dep)
    lines = card_109_lines(cards)
    flags = card_201_flags(cards)
    found = check_debug_visibility(cards)

    bad = []
    if lines != CARD_DESC_LINES:
        bad.append("  · 109 的 desc 是 %d 行（必须 = %d 行）" % (lines, CARD_DESC_LINES))
    for k, want in DEBUG_FLAGS:
        v = flags.get(k)
        if v != want:
            bad.append("  · 201 的 %s=%s（必须 = %d）" % (k, "缺失" if v is None else v, want))
    if bad:
        raise StepFail("部署件 cards.js 的关键项不对（%d 项）" % len(bad),
                       "\n".join(bad) + "\n\n"
                       "注意：本步读的是 **modkit 里的部署件**，不是源；"
                       "出现这种不一致说明 deploy 写进去的是旧件/半批。")
    return "109 desc=%d 行；201 init=0 / deck=0 / weight=6；调试卡 %d 张全不可见" % (lines, len(found))


def step_readback_files(modkit):
    """★ 部署件 files.js 里的每个 crc 与该文件现算的 crc 一致。"""
    root = _deployed(modkit, PKG_REL)
    files_js = os.path.join(root, "files.js")
    if not os.path.isfile(files_js):
        raise StepFail("部署件 files.js 不存在：%s" % files_js,
                       "deploy 没有把 files.js 发过去；重跑 deploy。")
    try:
        with open(files_js, encoding="utf-8") as f:
            entries = json.load(f)
    except json.JSONDecodeError as e:
        raise StepFail("部署件 files.js 不是合法 JSON：%s" % e,
                       "重跑 make files 生成，再 deploy。")
    bad = []
    for rel, want in sorted(entries.items()):
        p = os.path.join(root, rel.replace("/", os.sep))
        if not os.path.isfile(p):
            bad.append("  · %s：文件缺失（清单 crc=%s）" % (rel, want))
            continue
        got = crc32_blob(p)
        if got != want:
            bad.append("  · %s：现算 crc=%s ≠ 清单 crc=%s" % (rel, got, want))
    if bad:
        raise StepFail("部署件 files.js 与文件不一致（%d 项）" % len(bad),
                       "\n".join(bad[:30]) + "\n\n"
                       "修法：在 native/ 跑 `python3 mkfiles.py` 刷新 patch/files.js，"
                       "commit 后再跑本命令；不要手改 files.js。")
    return "%d 个条目的 crc 全部一致" % len(entries)


# ══════════════════════════════════════════════════════════════════════════════
# 阶段 C：发布 zip 与 modkit 同批（--full / --with-zip）
# ══════════════════════════════════════════════════════════════════════════════
def step_zip(modkit, zip_path):
    """抽 3 个 wav + cards.js + DLL，比「zip 内 md5」vs「modkit 现件」。

    ★ 只报，不自动重打：重打是 `make dist` 的活；本闸只负责让旧批次出不了门。
    """
    if not os.path.isfile(zip_path):
        raise StepFail("找不到发布包：%s" % zip_path,
                       "先 `make dist` 生成 dist/th18_card_expand_bundle.zip，"
                       "或把 --zip 指到真正的发布包。")
    root = _deployed(modkit, PKG_REL)
    checks = [
        ("bin/th18_card_expand.dll",
         os.path.join(modkit, "mods", "th18_card_expand.dll"), "DLL(mods)"),
        ("bin/th18_card_expand.dll",
         os.path.join(modkit, "thcrap", "bin", "mod_th18_card_expand.dll"), "DLL(bin)"),
        ("bin/th18_card_expand.dll",
         os.path.join(modkit, PKG_REL, "th18_card_expand.dll"), "DLL(pkg)"),
        ("patch-step3/th18/cards.js", os.path.join(root, "th18", "cards.js"), "cards.js"),
    ]
    bad = []
    with zipfile.ZipFile(zip_path) as z:
        names = set(z.namelist())
        wavs = sorted(n for n in names
                      if n.startswith("patch-step3/th18/voice/") and n.endswith(".wav"))
        if len(wavs) < 3:
            raise StepFail("发布包里只有 %d 个 wav（至少要能抽 3 个来对账）" % len(wavs),
                           "包内容：\n" + "\n".join(sorted(names)[:40]))
        for n in wavs[:3]:
            checks.append((n, os.path.join(root, "th18", "voice", os.path.basename(n)),
                           "wav:" + os.path.basename(n)))
        for entry, dst, label in checks:
            if entry not in names:
                bad.append("  · %-18s zip 内缺失条目：%s" % (label, entry))
                continue
            with z.open(entry) as f:
                z_md5 = hashlib.md5(f.read()).hexdigest()
            if not os.path.isfile(dst):
                bad.append("  · %-18s modkit 缺失：%s" % (label, dst))
                continue
            d_md5 = md5(dst)
            if z_md5 != d_md5:
                bad.append("  · %-18s **旧**：zip=%s  modkit=%s\n      %s"
                           % (label, z_md5, d_md5, entry))
        if bad:
            raise StepFail("发布 zip 与 modkit 不同批（%d 个条目旧/缺）" % len(bad),
                           "\n".join(bad) + "\n\n"
                           "本闸**不会自动重打**：确认包是拿当前源码 `make dist` 出来的，"
                           "再重新发布；不要拿旧包去覆盖 modkit。")
    return "zip 与 modkit 同批（DLL + cards.js + 3 wav）"


# ══════════════════════════════════════════════════════════════════════════════
# main
# ══════════════════════════════════════════════════════════════════════════════
def main():
    ap = argparse.ArgumentParser(
        description="一条命令的构建闸：校验 → 构建 → 发布 → 回读 → 汇总（任何一步红就停）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="例：python3 tools/devbuild.py --modkit /path/to/th18_modkit --skip-build")
    ap.add_argument("--modkit", default=os.environ.get("MODKIT_DIR", ""),
                    help="modkit 根目录（也可用 MODKIT_DIR 环境变量；不写死本机路径）")
    ap.add_argument("--skip-build", action="store_true",
                    help="跳过 make dll（只校验 + 发布已构建好的产物）")
    ap.add_argument("--allow-dirty", action="store_true",
                    help="放行未提交的源文件（默认拦下并提示先 commit）")
    ap.add_argument("--full", "--with-zip", dest="with_zip", action="store_true",
                    help="额外校验发布 zip 与 modkit 同批（抽 3 wav + cards.js + DLL）")
    ap.add_argument("--zip", default=ZIP_DEFAULT,
                    help="发布 zip 路径（默认 native/../dist/th18_card_expand_bundle.zip）")
    ap.add_argument("--skip-guide-pdf", action="store_true",
                    help="只生成 guide.tex 并查调试卡，不跑 xelatex（无 TeX 的机器用）")
    a = ap.parse_args()

    print("═" * 96)
    print("  devbuild —— card-expand 构建闸")
    print("  仓库：%s" % REPO)
    print("  modkit：%s" % (a.modkit or color("（未给：发布步骤会停）", C_WARN)))
    print("  模式：%s%s%s" % ("--skip-build " if a.skip_build else "",
                            "--allow-dirty " if a.allow_dirty else "",
                            "--full" if a.with_zip else "默认"))
    print("═" * 96)

    # ── A. 源码前置校验（不通过就不构建）──────────────────────────────────
    print("\n── A. 源码前置校验（不通过就不构建）" + "─" * 60)
    run_step("cards-check：desc≤6 行 / 每行<64B / 无 ASCII %",
             "DLL 运行期自检逐卡检查，任何一张不合规就整批新卡停用；owner 会看到「所有 mod 卡消失」。",
             "改 patch/th18/cards.js 的 desc（≤6 行、每行<64 字节、不能有 ASCII %），"
             "然后 `python3 mkfiles.py` 刷新 patch/files.js，commit。",
             step_cards_check)
    run_step("调试/测试卡不可见（init=0 / deck=0 / weight=6）",
             "调试卡会进商店 / 图鉴 / 卡组 / 指南，owner 实跑被污染（201 曾进图册、也曾每关必上架）。",
             "在 patch/th18/cards.js 把该卡写成 initial_unlocked=0、deck_visible=0、weight=6；"
             "★ weight 只认 6（CE_SHOP_NEVER_WEIGHT = 不进随机池），写 0 反而每次进店必上架 ✗。"
             "然后 `python3 mkfiles.py` 刷新 crc，commit。",
             step_debug_visibility)
    run_step("sites.py check：站点不变式 + 声明↔def 对账 + 预算",
             "站点表与 patch/def 不同步会让补丁静默不生效，或把跳板池用超（崩）。",
             "按输出改站点声明/生成物；改了 sites.py 先 `make gen`；预算问题见 GATES.md §8 记账。",
             step_sites_check)
    run_step("gen_def.py --check：导出表与 patch 声明一致",
             "patch 声明了但 def 没导出 ⇒ GetProcAddress 拿不到，游戏里静默不生效。",
             "`python3 tools/gen_def.py` 重写 th18_card_expand.def，commit。",
             step_gen_def)
    run_step("gen_drift.py：生成物无漂移",
             "手改 patch/*.js 只活在两次 make gen 之间，会让二分实验/发布拿到假状态。",
             "`make gen`（或 `python3 tools/gen_drift.py --fix`）重新生成，commit。",
             step_gen_drift)
    run_step("contract_check.py：JSON/行为契约",
             "有行为没登记 / category 与主动行为不一致时，DLL 会禁用全部新卡。",
             "按输出改 cards.js 或 DLL 行为登记；先单独跑 `python3 tools/contract_check.py`。",
             step_contract)
    run_step("test-host：主机 C 单测",
             "纯逻辑回归失败说明行为核心被改坏了，不该进入构建。",
             "`make test-host` 单独复现，修对应的 cards/*_core.c 或 tests/*.c。",
             step_test_host)
    run_step("git 分支不是 main（也不是 detached HEAD）",
             "在 main 上构建/发布会把未评审的改动直接带进发布件。",
             "`git switch -c feat/<你的分支>`（或切回特性分支）；绝不在 main 上跑发布。",
             step_git_branch)
    run_step("git 无未解决冲突（UU/AA/DD/…）",
             "冲突未解时源文件是半成品，构建出来的东西不可信。",
             "解决冲突并 `git add`；或 `git merge --abort` / `git rebase --abort` 回到干净态。",
             step_git_unmerged)
    run_step("git 无未提交源文件（或 --allow-dirty）",
             "发布件必须能从某个 commit 复现；工作区脏 ⇒ 发出去的东西没进历史。",
             "先 `git add -A && git commit -m \"...\"`；确认不该进本次构建时加 `--allow-dirty`。",
             lambda: step_git_dirty(a.allow_dirty))
    run_step("check-docs.py：文档规范",
             "文档链接/地址写法/版本声明/表格超长会污染发布仓。",
             "按输出改文档；`python3 tooling/check-docs.py --only links|addr|version|tables` 定位。",
             step_check_docs)
    run_step("冲突标记：mods/ 下文本文件无连续 7 个 < 或 >",
             "今天指南就是带着冲突标记被编译的；标记进构建 = 把半成品发给玩家。",
             "删掉冲突标记并完成合并；自查：搜 mods/ 下连续的 7 个 `<` 或 `>`（只看文本文件）。",
             step_conflicts)
    run_step("make_guide.py + 指南里无调试/测试卡",
             "调试卡进玩家图册是 owner 今天点名的翻车项（201）。",
             "把该卡加进 tools/make_guide.py 的 skip，或从 PLAYER-GUIDE.md 删掉它的 ### 节，重生成。",
             step_guide_tex)
    if a.skip_guide_pdf:
        skip_step("指南 PDF（xelatex）", "--skip-guide-pdf")
    else:
        run_step("指南 PDF：xelatex 两遍 + 打印页数",
                 "指南要能真的编译出来；字体/缺图/LaTeX 错误都应在发布前暴露。",
                 "装 XeLaTeX + 中文字体；WSL 上本脚本会自动映射 /mnt/c/Windows/Fonts；"
                 "也可设 GUIDE_CJK_FONT 指定字体名。没有 TeX 时用 --skip-guide-pdf。",
                 lambda: step_guide_pdf(a.skip_guide_pdf))

    # ── B. 构建 + 发布 + 回读 ────────────────────────────────────────────
    print("\n── B. 构建 + 发布 + 回读" + "─" * 70)
    if a.skip_build:
        skip_step("make dll", "--skip-build")
    else:
        run_step("make dll（先删旧 DLL 强制重链）",
                 "sites_gen.h / .def 是生成物，make 的依赖表曾经漏掉它们 ⇒ 旧 DLL 配新 patch。",
                 "看编译错误；先 `make gen` / `python3 tools/gen_def.py`；单独 `make dll` 复现。",
                 step_dll)
    run_step("deploy.py：发布 patch + DLL + 资产",
             "deploy 是唯一被认可的发布路径；它内部还会跑来源校验、发布锁、cards-check、资产同步。",
             "看 deploy 输出：来源校验（落后 origin/main）/ 锁（--force）/ cards-check / 资产同步。"
             "确认 --modkit 指向正确的 th18_modkit 根目录。",
             lambda: step_deploy(a.modkit))
    run_step("回读①：部署件 cards.js / files.js / DLL×3 与源同 md5",
             "复制失败或被并发发布覆盖时，modkit 里会是旧件/半批。",
             "重跑 deploy；检查 modkit 写权限与 .deploy.lock；不要手工改 modkit 里的产物。",
             lambda: step_readback_md5(a.modkit))
    run_step("回读②：部署件 109 desc=6 行 / 201 init=0 deck=0 weight=6",
             "今天两次翻车的直接判据；必须从部署件读，不能只看源。",
             "说明 deploy 写进去的是旧件/半批：重跑 deploy；确认 patch/th18/cards.js 已 commit。",
             lambda: step_readback_cards(a.modkit))
    run_step("回读③：部署件 files.js 的 crc 与文件现算一致",
             "crc 不对 ⇒ thcrap 认为文件没变/下载到旧件，线上表现就是「改了没用」。",
             "在 native/ 跑 `python3 mkfiles.py` 刷新 patch/files.js，commit 后重跑本命令。",
             lambda: step_readback_files(a.modkit))

    # ── C. 发布 zip（可选） ──────────────────────────────────────────────
    if a.with_zip:
        print("\n── C. 发布 zip 与 modkit 同批" + "─" * 64)
        run_step("发布 zip：抽 3 wav + cards.js + DLL 比 md5",
                 "包里是旧 BGM/旧卡表 ⇒ 玩家听到/看到的都是旧批次（owner 今天踩过）。",
                 "确认包是拿当前源码 `make dist` 出来的；本闸只报不重打，"
                 "按上面列出的旧条目重做发布包。",
                 lambda: step_zip(a.modkit, a.zip))

    # ── 汇总 ──────────────────────────────────────────────────────────────
    print_table()
    print()
    print("  ★ 本次要发的东西：")
    try:
        print("    DLL       : %s" % DLL)
        print("                md5 %s" % md5(DLL))
    except OSError:
        print("    DLL       : （缺失）")
    try:
        print("    cards.js  : %s" % CARDS)
        print("                md5 %s" % md5(CARDS))
    except OSError:
        print("    cards.js  : （缺失）")
    pdf = os.path.join(GUIDE_DIR, "guide.pdf")
    if os.path.isfile(pdf) and not a.skip_guide_pdf:
        pages = "?"
        log = os.path.join(GUIDE_DIR, "guide.log")
        if os.path.isfile(log):
            with open(log, encoding="utf-8", errors="replace") as f:
                m = re.search(r"Output written on .*?\((\d+) pages?", f.read())
            if m:
                pages = m.group(1)
        print("    guide.pdf : %s（%s 页）" % (pdf, pages))
    else:
        print("    guide.pdf : （本次跳过编译；tex 在 %s/guide.tex）" % GUIDE_DIR)
    print("    modkit    : %s" % (a.modkit or "（未发布）"))
    print()
    print(color("✅ 全绿：可以发布/实跑。", C_OK))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n✗ 被中断", file=sys.stderr)
        sys.exit(130)
