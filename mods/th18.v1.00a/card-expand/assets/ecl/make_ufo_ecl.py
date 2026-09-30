#!/usr/bin/env python3
"""把 `assets/ecl/ufo.ecl.txt` 里 112 的 9 个 UFO 子程序合进原生 `default.ecl` → `native/build/ecl/default.ecl`。

    python3 make_ufo_ecl.py             # 现场解原生 default.ecl → 合入 → 编回 → 写入 build/ecl/
    python3 make_ufo_ecl.py --verify-only   # 只跑自洽验证（不写产物）

为什么是 `default.ecl`：TH18 每关 `st0X.ecl` 头部都有 `ecli { "default.ecl"; … }` ⇒ 它是**全关共用的父 ECL**
（与 TH12 同款），UFO 那四色本体在 TH12 里就住在它里面（`th12.dat` 顶层，不是 st0X）。
一手与移植说明见 `assets/ecl/ufo.ecl.txt` 的文件头（出处 / 2 条 Δ / TODO-ASSET / 三部落地方案）。

⚠ **本脚本只产出 `native/build/ecl/default.ecl`，不碰任何 patch**：
   要真的发出去还需要（① 由本脚本产出 ✅ ② `native/release.py` 加一行 ✅ release.py:33 ③ 贴图）——
   ③ 的落点 2026-09-12 变了：贴图不再走 ECL，改由**卡侧的跟随 VM** 画（`ability.anm` script 164..167，
   ECL 侧 `anmSetSprite(0, -1)` = 原版「拆槽」⇒ 本体隐形）。原文见 `ufo.ecl.txt` 的文件头 Δ2 / 文件尾。
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MOD = HERE.parents[1]
REPO = MOD.parents[2]

VERSION = "18"
FRAGMENT = HERE / "ufo.ecl.txt"               # 112 的 9 个 UFO 子程序（入库）
GREEN_FRAGMENT = HERE / "green_extra.ecl.txt"  # ★ 110 的「绿妖精」载体 1 个子程序（入库）
SPIRIT_FRAGMENT = HERE / "spirit109.ecl.txt"   # ★ 109 合成卡的「灵」真敌人载体 1 个子程序（入库）
OUT = MOD / "native" / "build" / "ecl" / "default.ecl"
DAT = REPO / "local" / "th18.v1.00a" / "dat"
ECLMAP = REPO / "local" / "vendor" / "eclmap" / "eclmap" / "th18.eclm"
ENC = dict(encoding="utf-8", errors="surrogateescape")

# 我们的子程序名（与 TH12 真名一致；`_TTL` 后缀那 4 个是自计时等价改写）
OURS = ["UFO_EtBreak", "UFO_EtBreak2", "UFO_Red", "UFO_Blue", "UFO_Green", "UFO_Rainbow",
        "UFO_Red_TTL", "UFO_Blue_TTL", "UFO_Green_TTL", "UFO_Rainbow_TTL"]
# ★ 2026-09-18：同一份 `default.ecl` 里再加 **110 的绿妖精载体**（用户「每波多刷一只绿的」）。
#   放在同一个合并步里是**必须的**：两个脚本各写一次 `build/ecl/default.ecl` 会互相覆盖，
#   而 `make dist` 只跑 `ecl-ufo` 这一个目标 ⇒ 漏掉谁都是「发布出去少一支子程序」。
OURS_GREEN = ["CEGreenFairy"]
# ★ 2026-10-01：再加 **109 合成卡的「灵」载体**（owner 口径 8/9/10：灵 = 自建真敌人）。
#   同上：必须并进同一个合并步 —— 各写一次 build/ecl/default.ecl 会互相覆盖，
#   而 `make dist` 只跑 `ecl-ufo` 这一个目标。
OURS_SPIRIT = ["CESpirit109"]

_SUB_RE = re.compile(r"^void (\w+)\(\)\n\{\n.*?^\}\n", re.S | re.M)
_BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.S)


def thecl():
    p = REPO / "local" / "vendor" / "thtk" / "build" / "thecl" / "thecl"
    if not p.is_file():
        raise SystemExit("没有 thecl：bash tooling/thtk/build.sh")
    return p


def subs(text):
    """名字 → 子程序原文（`void X() { … }`）。"""
    return {m.group(1): m.group(0) for m in _SUB_RE.finditer(text)}


def _opcodes(block):
    """子程序体里「像指令」的行（去掉标签行/注释/空行/括号行）。
    ★ 2026-10-01：先剥掉 `/* … */` **块注释**（多行）—— 旧版只跳 `//` 行注释，
    于是任何带块注释的子程序（`green_extra.ecl.txt` 就是）在「源 vs 往返」的**指令行数**
    比对里会凭空多出几行，把这条自洽检查判红（110 那支因此一直被排除在检查外）。
    这是**工具口径修正**，不动任何 ECL 的语义。"""
    block = _BLOCK_COMMENT_RE.sub("", block)
    out = []
    for ln in block.split("\n"):
        t = ln.strip()
        if not t or t.startswith("//") or t.startswith("*") or t.startswith("{") or t.startswith("}"):
            continue
        if t.startswith("_") and t.endswith(":"):
            continue
        out.append(t)
    return out


def run(cmd, **kw):
    r = subprocess.run([str(c) for c in cmd], capture_output=True, **kw)
    if r.returncode != 0:
        sys.stderr.write(r.stderr.decode("utf-8", "replace")[:2000])
        raise SystemExit("命令失败：%s" % " ".join(str(c) for c in cmd))
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--verify-only", action="store_true")
    a = ap.parse_args()

    if not FRAGMENT.is_file():
        raise SystemExit("没有 %s" % FRAGMENT)
    frag = subs(FRAGMENT.read_text(**ENC))
    missing = [n for n in OURS if n not in frag]
    if missing:
        raise SystemExit("ufo.ecl.txt 少了子程序：%s" % ", ".join(missing))
    if len(frag) != len(OURS):
        extra = sorted(set(frag) - set(OURS))
        raise SystemExit("ufo.ecl.txt 里有多余子程序：%s" % ", ".join(extra))

    if not GREEN_FRAGMENT.is_file():
        raise SystemExit("没有 %s" % GREEN_FRAGMENT)
    green = subs(GREEN_FRAGMENT.read_text(**ENC))
    g_missing = [n for n in OURS_GREEN if n not in green]
    if g_missing:
        raise SystemExit("green_extra.ecl.txt 少了子程序：%s" % ", ".join(g_missing))
    if len(green) != len(OURS_GREEN):
        g_extra = sorted(set(green) - set(OURS_GREEN))
        raise SystemExit("green_extra.ecl.txt 里有多余子程序：%s" % ", ".join(g_extra))

    if not SPIRIT_FRAGMENT.is_file():
        raise SystemExit("没有 %s" % SPIRIT_FRAGMENT)
    spirit = subs(SPIRIT_FRAGMENT.read_text(**ENC))
    s_missing = [n for n in OURS_SPIRIT if n not in spirit]
    if s_missing:
        raise SystemExit("spirit109.ecl.txt 少了子程序：%s" % ", ".join(s_missing))
    if len(spirit) != len(OURS_SPIRIT):
        s_extra = sorted(set(spirit) - set(OURS_SPIRIT))
        raise SystemExit("spirit109.ecl.txt 里有多余子程序：%s" % ", ".join(s_extra))
    frag.update(green)
    frag.update(spirit)
    ALL_OURS = OURS + OURS_GREEN + OURS_SPIRIT

    # 原生 default.ecl 走的是**已解包**的那份（与 make_dev_ecl.py 同一口径）
    raw = DAT / "default.ecl"
    if not raw.is_file():
        raise SystemExit("没有 %s：先 python3 tooling/thtk/unpack.py th18.v1.00a" % raw)

    tmp = MOD / "native" / "build" / "ecl" / "_ufo_tmp"
    tmp.mkdir(parents=True, exist_ok=True)
    retail_txt = tmp / "default.retail.txt"
    run([thecl(), "-d", VERSION, "-m", ECLMAP, raw, retail_txt])
    retail_text = retail_txt.read_text(**ENC)
    retail = subs(retail_text)
    if not retail:
        raise SystemExit("原生 default.ecl 反编译不出子程序 —— 口径变了？")

    clash = sorted(set(retail) & set(ALL_OURS))
    if clash:
        raise SystemExit("子程序重名（原生里已有）：%s" % ", ".join(clash))

    # 合：原生原文 + 我们的全部子程序（UFO 9 + 绿妖精 1，追加在末尾，不影响原生的入口/结构）
    text = retail_text.rstrip("\n") + "\n\n" + "\n".join(frag[n] for n in ALL_OURS)
    merged_txt = tmp / "merged.ecl.txt"
    merged_txt.write_text(text, **ENC)

    built = tmp / "merged.ecl"
    run([thecl(), "-c", VERSION, "-m", ECLMAP, merged_txt, built], cwd=tmp)

    # 自洽 1：编回后**再编一次**，两个二进制字节一致（= 编译无损；与 make_dev_ecl.py 同一口径）
    back_txt = tmp / "merged.back.txt"
    run([thecl(), "-d", VERSION, "-m", ECLMAP, built, back_txt])
    again = tmp / "merged.again.ecl"
    run([thecl(), "-c", VERSION, "-m", ECLMAP, back_txt, again])
    if again.read_bytes() != built.read_bytes():
        raise SystemExit("往返二进制不一致（%d vs %d B）—— 说明有损"
                         % (again.stat().st_size, built.stat().st_size))

    # 自洽 2：往返后我们的全部子程序都在，且「指令行数」与源片段一致（结构级）
    back = subs(back_txt.read_text(**ENC))
    for n in ALL_OURS:
        if n not in back:
            raise SystemExit("往返后少了 %s" % n)
        ops_src, ops_back = _opcodes(frag[n]), _opcodes(back[n])
        if len(ops_src) != len(ops_back):
            raise SystemExit("%s 的指令行数变了：源 %d → 往返 %d"
                             % (n, len(ops_src), len(ops_back)))

    n_subs = len(retail) + len(ALL_OURS)
    print("原生 %d 子 + 我们 %d 子（UFO %d + 绿妖精 %d + 灵 %d）= %d 子；无重名 ✓"
          % (len(retail), len(ALL_OURS), len(OURS), len(OURS_GREEN), len(OURS_SPIRIT), n_subs))
    print("往返：编回 → 再编一次，二进制字节一致 ✓（%d B）；我们 %d 个子程序的指令行数不变 ✓"
          % (built.stat().st_size, len(ALL_OURS)))

    if a.verify_only:
        print("--verify-only：不写产物")
        return 0

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(built.read_bytes())
    print("写出 %s（%d B）" % (OUT.relative_to(MOD), OUT.stat().st_size))
    print("已接进构建：`make dist` 会把它拷进 patch-step3/th18/，`release.py` 也已加映射行")
    print("本体贴图走**卡侧的跟随 VM**（ability.anm script 164..167），ECL 侧 `anmSetSprite(0, -1)` "
          "= 原版「拆槽」⇒ 敌机本体隐形（一手 th18:0x436bf6；见 ufo.ecl.txt 文件头 Δ2）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
