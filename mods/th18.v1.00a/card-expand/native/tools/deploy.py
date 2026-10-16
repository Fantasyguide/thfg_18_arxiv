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


# ════════════════════════════════════════════════════════════════════════════════════════════════
# ★★★ 2026-10-17（owner 批准 ✓「多路 agent 并行」的机器化）：**发布并发安全**三件套
#
# 为什么（**全是今天的一手事故** ✗ —— PR/worktree 管不到这些"共享可写单点" ✗）：
#   · 2026-10-17 00:58 本脚本发 `600b6d30…` ⇒ **01:04 被别人覆盖成 `f502eaae…`**（那份是从**落后
#     12 条**的主检出构建的、没有当天修复）⇒ owner 实测「360° 还在 / 无敌还是无限 / 感觉你的修改
#     没部署」✗ ⇒ 我**手工比 md5+时间戳**才发现 ✗。
#   · 同源事故：156 条 109 早苗**直接提交在主检出的 `main`** 上 ⇒ 领先 156 / 落后 240 ⇒ 从它构建的
#     产物**缺共享线内容** ✗。
# ⇒ 三件：① 来源校验（落后就拒绝发布 ✗）② 发布锁（同一 modkit 同一时刻只许一个发布者 ✓）
#     ③ 台账（谁、基于哪个 commit、发了什么 md5 ⇒ 一眼看出被覆盖 ✓）。登记：AUDIT §BE-DEPLOY-RACE。
# ════════════════════════════════════════════════════════════════════════════════════════════════
LOCK_FRESH_SEC = 30 * 60          # 锁的"新鲜"窗口：30 分钟内视为别人正在发（❓自定，登记 TUNING ✓）


def _git(*args):
    """跑一条 git（任何异常都吞掉 ⇒ 返回 rc≠0 ✓：**本闸不允许因为环境问题制造假红** ✗）。"""
    try:
        return subprocess.run(["git", *args], cwd=NATIVE, capture_output=True, text=True)
    except Exception as e:                                     # 没装 git / 不是仓库
        return subprocess.CompletedProcess(args, 127, "", str(e))


def guard_source(dry):
    """① **来源校验**：拒绝从**落后 `origin/main`** 的树发布 ✗（判据 = origin/main 是 HEAD 的祖先 ✓）。

    · 落后 ⇒ **拒绝** ✗（返回 ok=False ⇒ 调用方 return 1 ✓）；
    · HEAD ≠ origin/main 但**不落后**（未合并分支做测试发布 ✓）⇒ 只**警告** ✓（并记进台账 ✓）；
    · 拿不到 git 信息 ⇒ **警告但不拦** ✓（不制造假红 ✗）。
    返回 `(ok, head, main_commit)` ✓。"""
    # ★ 2026-10-17 订正：**dry-run 也要跑** ✓（只读 git ✓）—— 第一版给它留了"dry 直接放行"的早退 ✗，
     #   结果拿"落后 12 条的主检出"做负例时**没被拦住**、白测一次 ✗（dry-run 只打印、不写文件 ✓，
     #   本来就没有理由跳过这道只读检查 ✓）。
    head = (_git("rev-parse", "--short", "HEAD").stdout or "").strip()
    mainc = (_git("rev-parse", "--short", "origin/main").stdout or "").strip()
    if not head or not mainc:
        print("⚠ 拿不到 git 来源信息（HEAD=%r origin/main=%r）⇒ 跳过来源校验（请自行确认树是干净的 ✓）"
              % (head, mainc))
        return True, head or "?", mainc or "?"
    if _git("merge-base", "--is-ancestor", "origin/main", "HEAD").returncode != 0:
        print("❌ 来源校验不过：本树**落后** origin/main ⇒ **拒绝发布** ✗")
        print("   理由：从落后共享线的树构建 ⇒ 发出去的产物会**缺共享线内容** ✗")
        print("   先例：2026-10-17 01:04 就是这样把 modkit 的 DLL 换成旧件、owner 实测「修复没生效」✓")
        print("   HEAD=%s · origin/main=%s ⇒ 先 `git fetch && git merge --ff-only origin/main` ✓"
              % (head, mainc))
        return False, head, mainc
    if head != mainc:
        print("⚠ 来源提醒：HEAD(%s) ≠ origin/main(%s) —— 允许（测试发布 ✓），但会记进台账 ✓" % (head, mainc))
    else:
        print("✓ 来源校验通过：HEAD == origin/main == %s ✓" % head)
    return True, head, mainc


def _lock_path(modkit):
    return os.path.join(modkit, ".deploy.lock")


def lock_acquire(modkit, force, dry):
    """② **发布锁**：`<modkit>/.deploy.lock` 存在且**新鲜**（<30 分钟 ✓）⇒ **拒绝**第二次发布 ✗。

    先例：00:58 我发 `600b6d30…` ⇒ 01:04 被覆盖 ⇒ owner 实测到旧行为 ✗。
    `--force` 可抢占 ✓（台账里写明"抢占" ✓）。返回 True = 拿到锁 ✓。"""
    import time as _t                                            # 只用一处 ⇒ 局部 import ✓
    p = _lock_path(modkit)
    if not dry and os.path.exists(p):
        try:
            age = _t.time() - os.path.getmtime(p)
        except OSError:
            age = 1e9
        if age < LOCK_FRESH_SEC and not force:
            print("❌ 发布锁被占：%s（%.1f 分钟前）⇒ 拒绝发布 ✗" % (p, age / 60.0))
            try:
                print("   持锁者：%s" % open(p, encoding="utf-8").read().strip().replace("\n", " · "))
            except OSError:
                pass
            print("   ⇒ 确认对方已结束再用 `--force` 抢占 ✓（会记进台账 ✓）")
            return False
        if age < LOCK_FRESH_SEC and force:
            print("⚠ --force：抢占**新鲜**锁（%.1f 分钟前）✓ 会记进台账 ✓" % (age / 60.0))
    if not dry:
        os.makedirs(modkit, exist_ok=True)
        with open(p, "w", encoding="utf-8") as f:
            f.write("pid=%d\nwho=%s\nhead=%s\nmain=%s\ntime=%s\n" % (
                os.getpid(), os.environ.get("DSH_SESSION", "?"),
                (_git("rev-parse", "--short", "HEAD").stdout or "?").strip(),
                (_git("rev-parse", "--short", "origin/main").stdout or "?").strip(),
                _t.strftime("%Y-%m-%d %H:%M:%S")))
    return True


def lock_release(modkit, dry):
    if not dry:
        try:
            os.remove(_lock_path(modkit))
        except OSError:
            pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--modkit", default=os.environ.get("MODKIT_DIR", ""),
                    help="modkit 根目录（也可用 MODKIT_DIR 环境变量；本脚本不写死任何机器路径）")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--skip-build", action="store_true", help="只发布已构建好的产物（仍会校验）")
    ap.add_argument("--force", action="store_true",
                    help="★ 2026-10-17：抢占**新鲜**的发布锁（默认拒绝 ✓；抢占会记进台账 ✓）")
    a = ap.parse_args()

    # ★★★ 2026-10-17：发布前两道闸（① 来源校验 ② 发布锁 ✓，实体见本文件上方那段注释 ✓）。
    #   ⚠ 在**任何写动作之前**；⚠ 锁会在下面的正常出口 release ✓ —— 若中途 `return 1` 退出，
    #     锁会留到 30 分钟新鲜窗口过期（自愈 ✓）或由 `--force` 抢占 ✓（有意如此：宁可留锁也别并发写 ✗）。
    ok_src, head_c, main_c = guard_source(a.dry_run)
    if not ok_src:
        return 1
    if a.modkit and not lock_acquire(a.modkit, a.force, a.dry_run):
        return 1

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
    # ★★★ 2026-10-17（③ **发布台账** ✓）：把「谁 · 基于哪个 commit · 发了什么 md5」追加进
    #   `<modkit>/deploy-manifest.json` ⇒ 事后一眼看出"是不是被别人覆盖了" ✓
    #   （今天 01:04 那次我是**手工比 md5+时间戳**才发现的 ✗）。**追加式** ✓（不抹历史 ✓，只留最近 50 条 ✓）。
    if a.modkit and not a.dry_run:
        import json as _j
        import time as _t
        four = [
            os.path.join(a.modkit, "mods", "th18_card_expand.dll"),
            os.path.join(a.modkit, "thcrap", "bin", "mod_th18_card_expand.dll"),
            os.path.join(a.modkit, "thcrap", "repos", "Renko_1055", PKG, "th18_card_expand.dll"),
            os.path.join(a.modkit, "thcrap", "repos", "Renko_1055", PKG, "th18.v1.00a.js"),
        ]
        got = {os.path.relpath(p, a.modkit): (md5(p) if os.path.exists(p) else "(缺失)") for p in four}
        dll_seen = {v for k, v in got.items() if k.endswith(".dll")}
        if len(dll_seen) != 1:                       # 三处 DLL 必须同 md5 ✓（不同 ⇒ 半批/被并发写花 ✗）
            print("❌ 回读异常：三处 DLL 出现 %d 种不同 md5 ⇒ 可能有**并发发布**把它写花了 ✗" % len(dll_seen))
            for k, v in got.items():
                print("   %-58s %s" % (k, v))
            lock_release(a.modkit, a.dry_run)
            return 1
        led = os.path.join(a.modkit, "deploy-manifest.json")
        try:
            hist = _j.load(open(led, encoding="utf-8")) if os.path.exists(led) else []
            if not isinstance(hist, list):
                hist = []
        except Exception:
            hist = []
        hist.append({
            "time": _t.strftime("%Y-%m-%d %H:%M:%S"),
            "who": os.environ.get("DSH_SESSION", "?"),
            "head": head_c, "origin_main": main_c, "on_origin_main": (head_c == main_c),
            "forced": bool(a.force), "targets": len(targets),
            "src_dll_md5": md5(DLL), "deployed": got,
        })
        with open(led, "w", encoding="utf-8") as f:
            _j.dump(hist[-50:], f, ensure_ascii=False, indent=2)
        print("③ 台账已记：%s（本次第 %d 条，保留最近 50 ✓）"
              % (os.path.relpath(led, a.modkit), len(hist)))
    if a.modkit:
        lock_release(a.modkit, a.dry_run)
    print("✅ deploy 完成" + ("（dry-run，未改任何文件）" if a.dry_run else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
