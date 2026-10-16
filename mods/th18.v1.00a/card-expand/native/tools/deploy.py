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
发布内容：patch js + DLL（3 个路径）+ **资产（`ability.anm` / `pl0X.sht`）**。
★ 2026-10-15 起**资产也由本脚本发**（走 `tools/sync_assets.py`：md5 相同跳过 / 不同先备份再拷 /
回读校验，**不一致就中止发布**）—— 之前那句「资产交给机器本地的 `local/sync_modkit_dll.sh`」**是错的** ✗：
那个脚本只发 DLL + BGM + `start_deck`，**anm 没人发** ⇒ modkit 里的图集可能是别的批次
⇒ 所有 sprite 索引按错图集解释 ⇒ **全屏错位** ✗（owner 实跑受害一次）。
其余 modkit 同步（BGM / `start_deck`）仍交给 `local/sync_modkit_dll.sh`（它知道本机路径那一半）。
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

    # ★ 数据文件同步（2026-09-17 补）：deploy 原来只发 patch 入口 + DLL ⇒ `th18/cards.js` 这类
    # **数据**永远进不了 modkit。加卡 118 时踩到：DLL 里有行为、归档里没有数据行 ⇒ SDK 自检
    # `FAIL: card N has a behavior in the DLL but no entry in cards.js` ⇒ **新 id 全体停用**
    # （回落零售 56）⇒ 存档/卡组还指着新 id ⇒ 编成闪退。发布路径必须连数据一起发。
    if a.modkit:
        patch_root = os.path.abspath(os.path.join(NATIVE, "..", "patch"))
        if os.path.isdir(patch_root):
            arch_root = os.path.join(a.modkit, "thcrap", "repos", "Renko_1055", PKG)
            for dirpath, _dirs, names in os.walk(patch_root):
                for n in names:
                    src = os.path.join(dirpath, n)
                    rel = os.path.relpath(src, patch_root)
                    if rel == os.path.basename(PATCH):      # 已由 PATCH 那条覆盖
                        continue
                    targets.append((src, os.path.join(arch_root, rel)))

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
            # ★ 2026-09-17：回读必须**按来源各自**比 md5 —— 数据文件（`th18/cards.js` 等）与
            # patch 入口的 md5 本来就不同，旧写法「.js 一律比 patch」会把正常发布判成失败。
            for src, dst in targets:
                want = md5(src)
                got = md5(dst) if os.path.exists(dst) else "(缺失)"
                if got != want:
                    bad.append("%s: %s != %s" % (dst, got, want))
            if bad:
                print("❌ 发布后校验失败：")
                for b in bad:
                    print("   " + b)
                return 1
            n_dll = sum(1 for s2, _ in targets if s2 == DLL)
            n_pat = sum(1 for s2, _ in targets if s2 == PATCH)
            print("  ✅ 全部回读一致：patch %s（%d 份）、DLL %s（%d 份）、数据文件 %d 份" %
                  (pmd5, n_pat, dmd5, n_dll, len(targets) - n_pat - n_dll))
        # ★★ 2026-10-15 新增：**资产（anm / sht）必须与 DLL、patch 同批**。
        #   事故形态：deploy 只发 patch+DLL、本机脚本只发 DLL+BGM ⇒ modkit 里的
        #   `th18/ability.anm` 是**别人批次**的 ⇒ 所有 sprite 索引按错图集解释 ⇒ **全屏错位** ✗
        #   （DLL 是新的、sht 也对，只有图集是旧的 —— md5 对比才看出来）。
        #   ⇒ 把资产同步做成发布的**一步**，且**不一致就中止**（别让半批出去）。
        print("⑤ 资产同步（anm / sht，必须与 DLL 同批）")
        rc = run([sys.executable, os.path.join(HERE, "sync_assets.py"), "--modkit", a.modkit], a.dry_run)
        if rc:
            print("❌ 资产同步失败（rc=%d）⇒ **不要拿去实跑**：modkit 现在可能与构建不同批" % rc)
            return 1
        print("  ✅ 资产回读一致（与 DLL/patch 同一次构建）" if not a.dry_run
              else "  （dry-run：只打印命令，未真校验）")
        local = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(MOD)))),
                             "local", "sync_modkit_dll.sh")
        if os.path.exists(local):
            print("⑥ 机器本地同步（BGM / start_deck 等）：%s" % local)
            rc = run(["bash", local], a.dry_run)
            if rc:
                print("⚠ 本地同步脚本返回 %d（BGM / start_deck 可能没同步上）" % rc)
    print("✅ deploy 完成" + ("（dry-run，未改任何文件）" if a.dry_run else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
