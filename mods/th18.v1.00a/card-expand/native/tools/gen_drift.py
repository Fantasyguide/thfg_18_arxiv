#!/usr/bin/env python3
"""gen_drift.py —— **生成物漂移闸**：`patch/*.js` 与 `native/sites_gen.h` 必须是 `sites.py` 现算出来的那一份。

## 为什么要有它（2026-09-18 的一次真实分歧）

`patch/th18.v1.00a.js` 是**生成物**（`sites.py gen` 产出），但它**没有「别手改」标记**
（`sites_gen.h` 有）。于是有人拿它做二分实验：手删一个 `breakpoints` 块（AUDIT §BE 详录 #41
的「摘掉 `ce_enemy_damage`」）。**手改只活在两次 `make gen` 之间** —— 任何会话的一次构建都会把它
静默抹掉，于是：

- 做实验的人以为"摘掉了"，后来的人看到"块又回来了" ⇒ 判成「未登记的还原」（#49 就这么错了）；
- 二分的对照组被无关构建污染 ⇒ 结论互斥（#41 的「站点数越线」vs §BE-DW 定案的「窗口里一条裸 `ja rel32`」）。

**判据（确定性）**：跑一次 `sites.py gen --rows 255` 到临时文件，逐字节比对三份生成物：
`patch/th18.v1.00a.js`、`native/sites_gen.h`、`patch-test/th18.v1.00a.js`。
不一致 ⇒ 红（说明：有人手改了生成物，或者改了声明源却忘了 `make gen`）。

**副作用**：本工具**不改工作区** —— `sites.py gen` 会就地在 `native/` 写 `sites_gen.h` 与
`patch-test/th18.v1.00a.js`，所以比完立刻从备份还原（`--fix` 才把新生成的那份留下）。

    python3 tools/gen_drift.py            # 查（进 make check）
    python3 tools/gen_drift.py --fix      # 用现算的那份覆盖（= make gen 的效果，但带对账输出）
"""
import argparse
import filecmp
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))       # native/tools
NATIVE = os.path.dirname(HERE)
MOD = os.path.dirname(NATIVE)
ROWS = 255
TARGETS = [
    (os.path.join(MOD, "patch", "th18.v1.00a.js"), "patch/th18.v1.00a.js"),
    (os.path.join(NATIVE, "sites_gen.h"), "native/sites_gen.h"),
    (os.path.join(MOD, "patch-test", "th18.v1.00a.js"), "patch-test/th18.v1.00a.js"),
]


def md5(p):
    return hashlib.md5(open(p, "rb").read()).hexdigest() if os.path.isfile(p) else None


# ── ★★ 2026-10-15（DSH agent，登记 AUDIT §BE-PEA-ELEC-2026-10-15）────────────────────────
# 77 电弹的**前方散射表** `s_rand_spread[]`（`cards/peashooter.c` ✓）也是**生成物** ✓：
# 卡侧禁 libm ✗ ⇒ 41 个方向是**离线**用 `tools/gen_pea_spread_table.py` 算好贴进 C 里的 ✓
# （owner 口径：「±20° 之间完全随机」✓）。它和 `patch/*.js` 属同一类风险 ✗ ——
# **手改表里的数字**（哪怕只改一个 0.0001）没人会发现 ⇒ 与真实物理角度悄悄分叉 ✓。
# 本段只**读**：跑一次生成器，把它的数据行与 C 文件里那段逐行比对 ✓。
# ⚠ `--fix` **也不改 C 文件** ✗（写 C 代码不是本工具的活 ✓）⇒ 只打印该跑哪条命令 ✓。
PEA_CARD = os.path.join(NATIVE, "cards", "peashooter.c")
PEA_GEN = os.path.join(HERE, "gen_pea_spread_table.py")


def pea_table_rows(text):
    """取 `s_rand_spread[...] = {` 与 `};` 之间的数据行（去掉**行尾注释**与所有空白后逐行比对 ✓）。

    ⚠ 每行数据都带一个行尾注释（`{ … },   /* -20° */` ✓）⇒ 判定前**必须**先砍掉 `/*` 之后的部分 ✗
    （第一版就是漏了这一步：两侧都解析出 0 行 ⇒ 闸变成"永远假红" ✗，2026-10-15 当场发现 ✓）。"""
    rows, inside = [], False
    for ln in text.splitlines():
        if not inside:
            if "s_rand_spread" in ln and "={" in ln.replace(" ", ""):
                inside = True
            continue
        s = ln.split("/*", 1)[0].strip()          # ★ 砍掉行尾注释 ✓
        if s.startswith("};"):
            break
        if s.startswith("{") and s.endswith("},"):
            rows.append("".join(s.split()))
    return rows


def check_pea_spread():
    """返回 (rc, 消息)。生成器或 C 文件缺了 ⇒ 跳过（rc=0 ✓，不制造无关红灯 ✗）。"""
    if not os.path.isfile(PEA_GEN) or not os.path.isfile(PEA_CARD):
        return 0, "⚠ 跳过散射表漂移检查（缺 %s 或 cards/peashooter.c）" % os.path.relpath(PEA_GEN, MOD)
    r = subprocess.run([sys.executable, PEA_GEN], cwd=HERE, capture_output=True, text=True)
    if r.returncode != 0:
        return 1, "❌ `gen_pea_spread_table.py` 自己失败（%d）\n%s" % (r.returncode, (r.stderr or "")[-500:])
    want = pea_table_rows(r.stdout)
    have = pea_table_rows(open(PEA_CARD, encoding="utf-8").read())
    if not want:
        return 1, "❌ 生成器没吐出任何数据行（生成器坏了 ✗）"
    if have == want:
        return 0, "✅ 散射表无漂移（%d 项与 `gen_pea_spread_table.py` 现算的一致）" % len(want)
    diff = next((i for i, (x, y) in enumerate(zip(have, want)) if x != y), min(len(have), len(want)))
    return 1, ("❌ 散射表漂移：`cards/peashooter.c` 的 `s_rand_spread[]` ≠ 现算那一份\n"
               "   工作区 %d 项 · 现算 %d 项 · 第一处不同在第 %d 项\n"
               "   工作区[%d] = %s\n   现算  [%d] = %s\n"
               "   ⇒ 表是**生成物** ✗（卡侧禁 libm ✓）⇒ 别手改数字 ✗；重跑\n"
               "        python3 native/tools/gen_pea_spread_table.py\n"
               "      用它打印的那段整块替换 `cards/peashooter.c` 里的表 ✓（θ 要改就改生成器的 --half-deg ✓）"
               % (len(have), len(want), diff,
                  diff, have[diff] if diff < len(have) else "（缺）",
                  diff, want[diff] if diff < len(want) else "（缺）"))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--fix", action="store_true", help="把现算的那份留下（= 刷新生成物）")
    a = ap.parse_args()

    pea_rc, pea_msg = check_pea_spread()
    print(pea_msg)

    for p, _ in TARGETS:
        if not os.path.isfile(p):
            print("❌ 缺生成物：%s（先跑 `make gen`）" % os.path.relpath(p, MOD))
            return 1

    with tempfile.TemporaryDirectory() as td:
        # ① 备份现场（sites.py gen 会就地在 native/ 与 patch-test/ 写文件）
        bak = {}
        for i, (p, _) in enumerate(TARGETS):
            # ★ 备份名必须**逐目标唯一**：两个目标的 basename 都叫 `th18.v1.00a.js`
            #   （主 patch 在 `patch/`、测试 patch 在 `patch-test/`）⇒ 只用 basename 会互相覆盖，
            #   还原时把**测试 patch 的内容写进主 patch**（2026-09-18 本工具上线第一次跑就踩了，
            #   见 AUDIT §BE-EG 的「首次实战」）。加序号前缀。
            b = os.path.join(td, "bak%02d_%s" % (i, os.path.basename(p)))
            shutil.copy2(p, b)
            bak[p] = b
        before = {ln: md5(p) for p, ln in TARGETS}

        # ② 现算（输出到临时文件；其余两份由 sites.py 就地写）
        out = os.path.join(td, "regen.js")
        r = subprocess.run([sys.executable, "sites.py", "gen", "--rows", str(ROWS), "-o", out],
                           cwd=NATIVE, capture_output=True, text=True)
        if r.returncode != 0:
            print("❌ `sites.py gen --rows %d` 自己失败（%d）—— 先修生成器" % (ROWS, r.returncode))
            print((r.stdout or "")[-800:] + (r.stderr or "")[-800:])
            for p, b in bak.items():
                shutil.copy2(b, p)
            return 1
        after = {}
        regen = {ln: (out if ln == "patch/th18.v1.00a.js" else p) for p, ln in TARGETS}
        for _, ln in TARGETS:
            after[ln] = md5(regen[ln])

        # ③ 还原（除非 --fix）
        if not a.fix:
            for p, b in bak.items():
                shutil.copy2(b, p)

    bad = [ln for ln in after if after[ln] != before[ln]]
    if not bad:
        print("✅ 生成物无漂移（三份都与 `sites.py gen --rows %d` 现算的一致）" % ROWS)
        return pea_rc          # ★ 三份 sites 产物没问题 ≠ 散射表没问题 ✓（两件事都过才算过 ✓）
    print("❌ 生成物漂移：%d 份与现算结果不一致" % len(bad))
    for ln in bad:
        print("   %-28s 工作区 %s ≠ 现算 %s" % (ln, (before[ln] or "?")[:12], (after[ln] or "?")[:12]))
    print("""
  两种可能，二选一：
    · **有人手改了生成物** ⇒ 生成物不许手改（`patch/*.js` 是 `sites.py` 的产物；做二分实验要改**声明源**
      或加开关，别改产物 —— 否则任何一次 `make gen` 都会把实验静默抹掉）；
    · **改了声明源却忘了重建** ⇒ 跑 `make gen`（或 `python3 tools/gen_drift.py --fix`），把新产物一起提交。""")
    return 1


if __name__ == "__main__":
    sys.exit(main())
