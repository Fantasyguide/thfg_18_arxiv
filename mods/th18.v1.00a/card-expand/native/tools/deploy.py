#!/usr/bin/env python3
"""deploy.py —— **唯一被认可的发布路径**：先过闸，再把 patch + DLL 一次性发齐。

为什么需要它（2026-09-17 的两类事故，都是「发布」这一步出的）：
  1. **混源**：只同步 DLL（或只同步 patch）⇒ modkit 里出现「patch 声明的站点集 ≠ DLL 提供的站点集」，
     启动器会报 `[!] dll 导出了 … 但没有已选 patch 声明断点`，而游戏里可能**静默不生效**或直接崩；
  2. **失败仍发布**：`make gen` / `make dll` 报错时照样 `cp` ⇒ 发出去的是半成品（我当天干过两次）。
本脚本把「构建 → 校验 → 发布 → 回读校验」串成一条链，**任何一步失败即中止**（退出码非 0，什么都不发）。

⚠ 依赖表坑（当天踩过两次，改产物后必须强制重链）：
  · `sites_gen.h` 不在 Makefile 依赖表里（`make gen` 重写它，但 `make dll` 不知道它变了）；
  · `th18_card_expand.def` 同理。
  ⇒ 所以这里**强制 `rm -f th18_card_expand.dll` 再 `make dll`**，保证 patch js 与 DLL 出自同一次构建。

用法：
  MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" python3 tools/deploy.py
  python3 tools/deploy.py --modkit <dir> [--dry-run] [--skip-build]
发布内容：patch js + DLL（3 个路径）。资产（pl0X.sht / ability.anm）与其余 modkit 同步交给机器本地的
`local/sync_modkit_dll.sh`（若存在则自动执行；它才是知道本机路径那一半）。
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
MOD = os.path.dirname(NATIVE)                      # mods/th18.v1.00a/card-expand
PATCH = os.path.join(MOD, "patch", "th18.v1.00a.js")
DLL = os.path.join(NATIVE, "th18_card_expand.dll")
PKG = "th18_card_expand_255"


def md5(p):
    return hashlib.md5(open(p, "rb").read()).hexdigest()


def run(cmd, dry):
    print("  $", " ".join(cmd), flush=True)
    if dry:
        return 0
    return subprocess.call(cmd, cwd=NATIVE)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--modkit", default=os.environ.get("MODKIT_DIR", ""),
                    help="modkit 根目录（也可用 MODKIT_DIR 环境变量；本脚本不写死任何机器路径）")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--skip-build", action="store_true", help="只发布已构建好的产物（仍会校验）")
    a = ap.parse_args()

    targets = []
    if a.modkit:
        targets = [
            (PATCH, os.path.join(a.modkit, "thcrap", "repos", "Renko_1055", PKG, "th18.v1.00a.js")),
            (DLL,   os.path.join(a.modkit, "mods", "th18_card_expand.dll")),
            (DLL,   os.path.join(a.modkit, "thcrap", "bin", "mod_th18_card_expand.dll")),
            (DLL,   os.path.join(a.modkit, "thcrap", "repos", "Renko_1055", PKG, "th18_card_expand.dll")),
        ]

    print("① 构建（任一步失败即中止，什么都不发）")
    if not a.skip_build:
        for cmd in (["make", "gen"], ["rm", "-f", DLL], ["make", "dll"]):
            rc = run(cmd, a.dry_run)
            if rc:
                print("❌ 构建失败（%s），已中止，未发布任何文件" % " ".join(cmd))
                return 1
    print("② 校验（站点预算 / 双向导出对账 / def 与声明一致）")
    for cmd in (["python3", "sites.py", "check"], ["python3", "tools/gen_def.py", "--check"]):
        rc = run(cmd, a.dry_run)
        if rc:
            print("❌ 校验失败（%s），已中止，未发布任何文件" % " ".join(cmd))
            return 1
    if not a.dry_run and not (os.path.exists(PATCH) and os.path.exists(DLL)):
        print("❌ 产物缺失：先跑 `make gen` / `make dll`")
        return 1
    if not a.dry_run and os.path.getmtime(PATCH) < os.path.getmtime(os.path.join(NATIVE, "sites.py")):
        print("⚠ patch js 比 sites.py 旧 —— 多半是忘了 `make gen`（这里仍继续，但请确认）")

    print("③ 发布（patch 与 DLL 必须同一次构建 ⇒ 一起发）")
    if not a.modkit:
        print("ℹ 未给 --modkit/MODKIT_DIR ⇒ 只做构建与校验（不发文件）")
    else:
        pmd5, dmd5 = md5(PATCH), md5(DLL)
        for src, dst in targets:
            print("  %s -> %s" % (os.path.basename(src), dst), flush=True)
            if a.dry_run:
                continue
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(src, dst)
        print("④ 回读校验（同源 + 站点集合一致）")
        if not a.dry_run:
            bad = []
            for _, dst in targets:
                want = pmd5 if dst.endswith(".js") else dmd5
                got = md5(dst) if os.path.exists(dst) else "(缺失)"
                if got != want:
                    bad.append("%s: %s != %s" % (dst, got, want))
            if bad:
                print("❌ 发布后校验失败：")
                for b in bad:
                    print("   " + b)
                return 1
            print("  ✅ patch md5 = %s（1 份）；DLL md5 = %s（3 份，四路径同值）" % (pmd5, dmd5))
        local = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(MOD)))),
                             "local", "sync_modkit_dll.sh")
        if os.path.exists(local):
            print("⑤ 机器本地同步（资产等）：%s" % local)
            rc = run(["bash", local], a.dry_run)
            if rc:
                print("⚠ 本地同步脚本返回 %d（资产可能没同步上）" % rc)
    print("✅ deploy 完成" + ("（dry-run，未改任何文件）" if a.dry_run else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
