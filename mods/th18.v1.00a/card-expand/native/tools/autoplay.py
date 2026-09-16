#!/usr/bin/env python3
"""TH18 自动测试代打（第一版：**0 站点**的物理按键注入 + 时间轴脚本 + 收日志 + 接判读）。

设计口径（见 `docs/superpowers/plans/2026-09-17-th18-autotest-framework.md`）
- **为什么走物理按键**：在游戏进程里"每帧覆盖输入位"需要一条新的断点站点，而预算只剩 2 条；
  物理注入（user32 `keybd_event`）走游戏自己的输入路径，**站点 0**，而且更真。
  代价：需要游戏窗口在前台（本脚本会把窗口拉到前台再发键）。
- **时间轴**：场景 = 一串 `(时刻, 动作)`；动作是"按住/松开某键"，键名用**意图名**而不是物理键，
  由 `KEYS` 表映射（`SHOT`=Z、`BOMB`=X、`SLOW`=Shift、`CARD_SWITCH`=D、`CARD_USE`=C、
  方向 = 方向键）⇒ 与同僚契约的动作位（`SHOT=4 / BOMB=5 / SLOW=6 / CARD_USE=8 / CARD_SWITCH=9`）同名。
- **离线可验证**：`--dry-run` 只打印时间轴与将要发的键（不启动、不注入）⇒ 判读/编排逻辑可在无游戏时自测。

用法
----
    python3 native/tools/autoplay.py --scenario z-intercept --dry-run
    python3 native/tools/autoplay.py --scenario all12 --seconds 90          # 真跑
    python3 native/tools/autoplay.py --scenario all12 --check-only          # 只判读现有日志

退出码：0 = 判读通过；1 = 判读未过；2 = 编排/环境错误（例如场景不存在）。
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
VERIFY = os.path.join(HERE, "verify.py")

# 意图名 → 按键（SendKeys 记号）。方向键用 {UP} 这类；字母/Shift 直接写。
KEYS = {
    "SHOT": "z", "BOMB": "x", "SLOW": "+", "CARD_USE": "c", "CARD_SWITCH": "d",
    "UP": "{UP}", "DOWN": "{DOWN}", "LEFT": "{LEFT}", "RIGHT": "{RIGHT}",
    "ENTER": "{ENTER}", "PAUSE": "{ESC}",
}

# 场景：每个原语配一条最小时间轴（"要触发什么动作"）。`deck` = 用哪副卡组（见 --deck）。
SCENARIOS = {
    "z-intercept": dict(deck="sys12", seconds=60, why="短按 Z 出点射、长按 Z 蓄力，验证 109 的拦 Z 与 CA",
                        timeline=[(1.0, "hold SHOT"), (2.0, "release SHOT"), (3.0, "hold SHOT"),
                                  (8.0, "release SHOT")]),
    "charge": dict(deck="sys12", seconds=60, why="长按 Z 到档再放开 = 蓄力攻击（同一动作，看 CA 日志）",
                   timeline=[(1.0, "hold SHOT"), (12.0, "release SHOT")]),
    "storm-chain": dict(deck="sys12", seconds=90, why="清一片小怪（含 Shift 固灵移动去找密集怪）",
                        timeline=[(1.0, "hold SHOT"), (3.0, "hold UP"), (9.0, "release UP"),
                                  (12.0, "hold SLOW"), (20.0, "release SLOW"), (30.0, "release SHOT")]),
    "spirit": dict(deck="sys12", seconds=90, why="击破小怪掉灵魂 + Shift 固灵吸灵（灵魂账在收尾打）",
                   timeline=[(1.0, "hold SHOT"), (6.0, "hold SLOW"), (14.0, "hold UP"),
                             (22.0, "release UP"), (28.0, "release SLOW"), (40.0, "release SHOT")]),
    "collect": dict(deck="sys12", seconds=120, why="上线收点（P 点/金钱）+ 令牌落地",
                    timeline=[(1.0, "hold SHOT"), (5.0, "hold UP"), (40.0, "release UP"),
                              (42.0, "release SHOT")]),
    "spawn-enemy": dict(deck="sys12", seconds=120, why="令牌走到 ce_spawn_enemy（需 112 的令牌路径被触发）",
                        timeline=[(1.0, "hold SHOT"), (5.0, "hold UP"), (40.0, "release UP"),
                                  (42.0, "release SHOT")]),
    "familiar": dict(deck="sys12", seconds=120, why="使魔随 power 出现；**一局收尾**才会打 run done",
                     timeline=[(1.0, "hold SHOT"), (10.0, "hold SLOW"), (30.0, "release SLOW"),
                               (60.0, "hold BOMB"), (61.0, "release BOMB"),
                               (100.0, "release SHOT")]),
    "emitter-close": dict(deck="sys12", seconds=120, why="打掉某只敌机的发弹口（gate 生效）",
                          timeline=[(1.0, "hold SHOT"), (20.0, "hold SLOW"), (50.0, "release SLOW"),
                                    (60.0, "release SHOT")]),
    "card118": dict(deck="sys12", seconds=90, why="切到 118 并按 C 发动（应发 12 张卡）",
                    timeline=[(1.0, "hold CARD_SWITCH"), (1.2, "release CARD_SWITCH"),
                              (2.0, "hold CARD_USE"), (2.2, "release CARD_USE")]),
    "all12": dict(deck="sys12", seconds=180, why="全装混沌跑：12 张同时在编成，看有没有崩 + 各原语判据",
                  timeline=[(1.0, "hold SHOT"), (6.0, "hold SLOW"), (20.0, "release SLOW"),
                            (25.0, "hold UP"), (45.0, "release UP"), (60.0, "hold BOMB"),
                            (61.0, "release BOMB"), (90.0, "hold CARD_USE"), (90.5, "release CARD_USE"),
                            (150.0, "release SHOT")]),
}



# ---------------------------------------------------------------- WSL → Windows 桥
# 分工（用户 2026-09-17 口径）：**测试动作必须由 Windows 侧走**；WSL 只做编排与判读。
# 实测（见 docs/superpowers/plans/2026-09-17-th18-autotest-framework.md §5）：
#   · 本环境 WSLInterop 已注册；cmd.exe / powershell.exe 不在 PATH，但**绝对路径可用**；
#   · Windows 侧用 **uv** 管 Python（`uv run --no-project --python 3.11 <script>`）⇒ 不硬编码解释器路径；
#   · 引号过 `cmd /c` 会被吃 ⇒ 写一支 ASCII 的 .bat 再调；中文路径过 cmd 有编码风险 ⇒ 参数走 JSON。
CMD_EXE = "/mnt/c/Windows/System32/cmd.exe"
WIN_RUNNER = "autoplay_win.py"



def to_win_path(p):
    """/mnt/d/… ⇒ D:\\…；已是 Windows 路径则原样返回。"""
    if not p:
        return p
    m = re.match(r"^/mnt/([a-zA-Z])/(.*)$", p)
    if m:
        return m.group(1).upper() + ":\\" + m.group(2).replace("/", "\\")
    return p


def to_wsl_path(p):
    """"D:\\… ⇒ /mnt/d/…；已是 WSL 路径则原样返回。"""
    if not p:
        return p
    m = re.match(r"^([a-zA-Z]):[\\/](.*)$", p)
    if m:
        return "/mnt/%s/%s" % (m.group(1).lower(), m.group(2).replace("\\", "/"))
    return p


def win_tmp(explicit=""):
    """Windows 侧的暂存目录（纯 ASCII）。可用 --win-tmp 覆盖。"""
    if explicit:
        return explicit
    user = os.environ.get("USER") or os.environ.get("USERNAME") or ""
    return "/mnt/c/Users/%s/AppData/Local/Temp/renkolab" % user


def win_bridge(extra_args, tmp, verbose=True):
    """把 runner 拷到 Windows 暂存目录 + 写 .bat（uv 启动）→ 经 cmd.exe 调用。返回 (rc, 输出)。"""
    os.makedirs(tmp, exist_ok=True)
    src = os.path.join(HERE, WIN_RUNNER)
    if not os.path.exists(src):
        return 2, "%s 不存在" % src
    shutil.copyfile(src, os.path.join(tmp, WIN_RUNNER))
    bat = os.path.join(tmp, "run_autoplay.bat")
    with open(bat, "w", newline="\r\n") as f:
        f.write("@echo off\r\ncd /d C:\\\r\n"
                'uv run --no-project --python 3.11 "%~dp0' + WIN_RUNNER + '" %*\r\n')
    winbat = "C:\\Users\\%s\\AppData\\Local\\Temp\\renkolab\\run_autoplay.bat" % (
        os.environ.get("USER") or os.environ.get("USERNAME") or "")
    cmd = [CMD_EXE, "/c", winbat] + list(extra_args)
    if verbose:
        print("   桥：%s" % " ".join(cmd))
    try:
        # ⚠ Windows 控制台是代码页（GBK/CP936）⇒ 不能用 text=True 按 UTF-8 解；
        #   二进制收 + 容错解码（runner 已改成 ASCII 转义输出，正常路径不会再遇到非 ASCII）。
        r = subprocess.run(cmd, capture_output=True, timeout=900)
        dec = lambda b: (b or b"").decode("utf-8", "replace")
        return r.returncode, dec(r.stdout) + dec(r.stderr)
    except FileNotFoundError:
        return 2, "cmd.exe 不可用（%s）—— 不在 WSL？" % CMD_EXE
    except subprocess.TimeoutExpired:
        return 2, "Windows runner 超时"



DECKS = {
    # 卡组道具（写 modkit 的 mods/start_deck.js —— 官方机制；cap 是 opt-in，只在需要 >3 张时写）
    "sys12": dict(mode="exact", cap=12, deck=list(range(106, 118))),
    "one118": dict(mode="exact", cap=0, deck=[118]),
    "off":   dict(mode="off", cap=0, deck=[]),
}


def write_deck(mk, name, dry):
    """按场景写起手卡组（写失败不影响代打 —— 只是卡组可能不是你要的那副）。"""
    d = DECKS[name]
    path = os.path.join(mk, "mods", "start_deck.js")
    obj = {"note": "由 native/tools/autoplay.py --deck %s 写入（临时测试卡组；复原=改成 mode off 或删本文件）" % name,
           "mode": d["mode"], "deck": d["deck"]}
    if d["cap"]:
        obj["cap"] = d["cap"]
    txt = json.dumps(obj, ensure_ascii=False, indent=2)
    if dry:
        print("   [dry] 卡组 %s → %s\n%s" % (name, path, txt))
        return True
    try:
        with open(path, "w", encoding="utf-8") as f:
            f.write(txt)
        print("   卡组 %s 已写入 %s" % (name, path))
        return True
    except OSError as e:
        print("   ⚠ 卡组写入失败（%s）—— 用当前卡组继续" % e)
        return False


def attribute_dumps(mk, dumps):
    """有新 dump 就调 tooling/crashdump/dmp.py 出签名（自动归因）。"""
    if not dumps:
        return
    dmp = os.path.join(HERE, "..", "..", "..", "..", "..", "tooling", "crashdump", "dmp.py")
    dmp = os.path.normpath(dmp)
    for d in dumps:
        path = os.path.join(mk, "crashdumps", d)
        print("   ★ 新 dump：%s" % d)
        if not os.path.exists(dmp):
            print("     （找不到 %s，跳过归因）" % dmp)
            continue
        try:
            r = subprocess.run([sys.executable, dmp, path, "--frames", "3"],
                               capture_output=True, timeout=600)
            for line in r.stdout.decode("utf-8", "replace").splitlines():
                if line.startswith(("异常", "寄存器", "EIP 在", "访问")):
                    print("     " + line.strip())
        except Exception as e:
            print("     归因失败：%s" % e)


def find_modkit(explicit):
    if explicit:
        return explicit
    return os.environ.get("MODKIT_DIR", "")



# 场景 → 它要判的原语（`all` = 全判）。★ 修掉一个设计缺陷：以前每个场景都判**全部**原语，
# 于是"场景退出码"反映的是全局，汇总表里连该通过的场景也显示未过。
SCEN_PRIM = {
    "z-intercept": "z-intercept", "charge": "charge", "storm-chain": "storm-chain",
    "spirit": "spirit", "collect": "collect", "spawn-enemy": "spawn-enemy",
    "familiar": "familiar", "emitter-close": "emitter-close",
    "card118": "all", "all12": "all",
}


def plan(scn):
    """把场景展开成 [(绝对秒, 动作, 键, 是按下吗)]，做静态检查。"""
    out, held, errs = [], set(), []
    for i, item in enumerate(scn["timeline"]):
        try:
            t, act = item
            verb, key = act.split(" ", 1)
        except Exception:
            errs.append("timeline[%d] 格式应为 (秒, 'hold|release 键名')：%r" % (i, item))
            continue
        if key not in KEYS:
            errs.append("timeline[%d] 未知键名 %r（可选：%s）" % (i, key, "/".join(sorted(KEYS))))
            continue
        if verb == "hold":
            if key in held:
                errs.append("timeline[%d] %s 已经按下（重复 hold）" % (i, key))
            held.add(key)
            out.append((float(t), act, KEYS[key], True))
        elif verb == "release":
            if key not in held:
                errs.append("timeline[%d] %s 没按下就 release" % (i, key))
            held.discard(key)
            out.append((float(t), act, KEYS[key], False))
        else:
            errs.append("timeline[%d] 未知动作 %r（只有 hold / release）" % (i, verb))
    for k in sorted(held):
        errs.append("时间轴结束时 %s 还按着（补一条 release）" % k)
    out.sort(key=lambda x: x[0])
    return out, errs


def inject_powershell(script):
    """经 powershell 发键（WSL → Windows）。返回 (ok, 输出)。"""
    try:
        p = subprocess.run(["powershell.exe", "-NoProfile", "-Command", script],
                           capture_output=True, text=True, timeout=30)
        return p.returncode == 0, (p.stdout or "") + (p.stderr or "")
    except FileNotFoundError:
        return False, "powershell.exe 不可用（不在 WSL 里？）"
    except subprocess.TimeoutExpired:
        return False, "powershell 超时"


def focus_and_send(key, down, dry):
    """把 th18 窗口拉到前台并发一次键。dry=True 只打印。"""
    verb = "down" if down else "up"
    if dry:
        print("   [dry] %-4s %s" % (verb, key))
        return True
    ps = (
        "Add-Type -Namespace W -Name U -MemberDefinition '"
        "[DllImport(\"user32.dll\")] public static extern IntPtr FindWindow(string c, string n);"
        "[DllImport(\"user32.dll\")] public static extern bool SetForegroundWindow(IntPtr h);"
        "[DllImport(\"user32.dll\")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);';"
        "$h=[W.U]::FindWindow($null,'Touhou 18'); if($h -ne [IntPtr]::Zero){[void][W.U]::SetForegroundWindow($h)};"
        "$VK=@{'{UP}'=0x26;'{DOWN}'=0x28;'{LEFT}'=0x25;'{RIGHT}'=0x27;'{ENTER}'=0x0D;'{ESC}'=0x1B};"
        "$k='%s'; if($VK.ContainsKey($k)){$vk=[byte]$VK[$k]}else{$vk=[byte][char]([string]$k).ToUpper()};"
        "$up=%d; [W.U]::keybd_event($vk,0,$up,0)" % (key, 2 if not down else 0)
    )
    ok, out = inject_powershell(ps)
    if not ok:
        print("   ⚠ 注入失败：%s" % out.strip()[:200])
    return ok


def last_boot_section(text):
    lines = text.splitlines()
    idx = [i for i, l in enumerate(lines) if re.match(r"^=== th18_card_expand .*===$", l.strip())]
    return "\n".join(lines[idx[-1]:]) if idx else text


def main():
    ap = argparse.ArgumentParser(description="TH18 自动测试代打（0 站点按键注入 + 判读）")
    ap.add_argument("--scenario", default="", help="场景名（--list 看全部）")
    ap.add_argument("--list", action="store_true", help="列出场景")
    ap.add_argument("--dry-run", action="store_true", help="只打印时间轴，不启动不注入")
    ap.add_argument("--check-only", action="store_true", help="跳过代打，直接判读现有日志")
    ap.add_argument("--log", default="", help="日志路径（缺省 <modkit>/game/th18_card_expand.log）")
    ap.add_argument("--modkit", default="", help="modkit 根目录（也可用 MODKIT_DIR）")
    ap.add_argument("--seconds", type=float, default=0, help="覆盖场景时长")
    ap.add_argument("--win-smoke", action="store_true", help="只验 WSL→Windows 桥（不需要游戏）")
    ap.add_argument("--all", action="store_true", help="把全部场景跑一遍（串起八原语 + all12）")
    ap.add_argument("--selftest", action="store_true", help="离线自检：全部场景的编排（不需要游戏）")
    ap.add_argument("--deck", default="", choices=[""] + list(DECKS), help="起手卡组（写 mods/start_deck.js）")
    ap.add_argument("--launch", action="store_true", help="让 Windows runner 自己启动游戏")
    ap.add_argument("--win-tmp", default="", help="Windows 暂存目录（默认 %%TEMP%%\\renkolab）")
    ap.add_argument("--scale", type=float, default=1.0,
                    help="时间轴整体缩放（<1 = 加速跑；调试编排时用）")
    a = ap.parse_args()

    if a.selftest:
        bad = [(n, e) for n, scn in SCENARIOS.items() for e in plan(scn)[1]]
        print("场景编排自检（离线）：%d 个场景，问题 %d 条" % (len(SCENARIOS), len(bad)))
        for n, e in bad:
            print("  ✗ %s：%s" % (n, e))
        return 1 if bad else 0

    if a.win_smoke:
        print("WSL→Windows 桥自检（uv 启动 Windows 侧 runner，不需要游戏）")
        cfg = os.path.join(win_tmp(a.win_tmp), "autoplay_cfg.json")
        os.makedirs(os.path.dirname(cfg), exist_ok=True)
        with open(cfg, "w", encoding="utf-8") as f:
            json.dump({"modkit": to_win_path(find_modkit(a.modkit))}, f, ensure_ascii=False)
        rc, out = win_bridge(["--smoke"], win_tmp(a.win_tmp))
        print(out.strip())
        return rc

    if a.all and not a.dry_run:
        names = [n for n in SCENARIOS if n != "all12"] + ["all12"]
        print("== 全场景串跑：%d 个（八原语 + all12 全装）==" % len(names))
        rc_all = 0
        summary = []
        for i, n in enumerate(names, 1):
            print("\n---- [%d/%d] %s ----" % (i, len(names), n))
            sub = [sys.executable, os.path.abspath(__file__), "--scenario", n]
            if a.modkit: sub += ["--modkit", a.modkit]
            if a.deck: sub += ["--deck", a.deck]
            if a.launch: sub += ["--launch"]
            if a.check_only: sub += ["--check-only"]
            if a.log: sub += ["--log", a.log]
            if a.scale != 1.0: sub += ["--scale", str(a.scale)]
            if a.win_tmp: sub += ["--win-tmp", a.win_tmp]
            r = subprocess.run(sub)
            rc_all |= r.returncode
            summary.append((n, r.returncode))
        mk = find_modkit(a.modkit)
        sd = (os.path.join(mk, "autotest") if mk
              else os.path.dirname(os.path.abspath(a.log or ".")))   # 兜底：放日志旁边（离线彩排用）
        if True:
            os.makedirs(sd, exist_ok=True)
            mp = os.path.join(sd, "summary.md")
            with open(mp, "w", encoding="utf-8") as f:
                f.write("# TH18 自动验收 —— 全场景汇总\n\n")
                f.write("| # | 场景 | 退出码 | 明细报告 |\n| --- | --- | --- | --- |\n")
                for i, (n, rc) in enumerate(summary, 1):
                    f.write("| %d | %s | %s | `report_%s.md` |\n" % (i, n, "通过" if rc == 0 else "未过", n))
                f.write("\n**合计 %d 个场景：%d 通过 / %d 未过**\n" %
                        (len(summary), sum(1 for _, rc in summary if rc == 0),
                         sum(1 for _, rc in summary if rc)))
                f.write("\n> 判据没中时看每份明细的「分类」列：**失效** = 做了但没效果；**未触发** = 本局没走到那条路"
                        "（表头下方的 ↳ 行写了该原语为什么可能要等条件）。\n")
            print("\n汇总报告：%s" % mp)
        print("\n== 全场景结束：%s ==" % ("全部通过" if rc_all == 0 else "有未过（退出码 %d）" % rc_all))
        return rc_all

    if a.list or a.scenario not in SCENARIOS:
        print("场景台账（deck / 秒数 / 触发什么）：")
        for k, v in SCENARIOS.items():
            print("  %-14s %-6s %4ds  %s" % (k, v["deck"], v["seconds"], v["why"]))
        return 0 if a.list else 2

    scn = SCENARIOS[a.scenario]
    steps, errs = plan(scn)
    if errs:
        print("✗ 场景 %s 编排有问题：" % a.scenario)
        for e in errs:
            print("   " + e)
        return 2

    dur = a.seconds or scn["seconds"]
    print("场景 %s：%s" % (a.scenario, scn["why"]))
    print("  时长 %.0fs、卡组 %s、动作 %d 条（scale %.2f）" % (dur, scn["deck"], len(steps), a.scale))
    for t, act, key, down in steps:
        print("   t=%6.2fs  %-14s  → 键 %-6s %s" % (t * a.scale, act, key, "按下" if down else "松开"))

    if a.dry_run:
        print("（干跑：不启动游戏、不注入按键）")
        return 0

    mk = find_modkit(a.modkit)
    log = a.log or (os.path.join(mk, "game", "th18_card_expand.log") if mk else "")
    if not log and a.check_only:
        print("✗ check-only 需要 --log（或 --modkit/MODKIT_DIR）"); return 2
    if not log:
        print("✗ 没有日志路径：给 --log 或 --modkit/MODKIT_DIR")
        return 2

    if not a.check_only:
        print("① 发布（唯一路径 deploy.py）")
        r = subprocess.run([sys.executable, os.path.join(HERE, "deploy.py")] +
                           (["--modkit", mk] if mk else []))
        if r.returncode:
            print("✗ 发布失败 ⇒ 中止（不要让代打跑在旧产物上）")
            return r.returncode
        if a.deck:
            print("② 卡组：")
            write_deck(mk, a.deck, a.dry_run)
        print("③ 代打（经 WSL→Windows 桥；Windows 侧 uv 启动 runner）")
        job = {"scenario": a.scenario, "seconds": dur, "scale": a.scale, "launch": bool(a.launch),
               "steps": [{"t": t, "act": act, "key": key, "down": down} for t, act, key, down in steps]}
        tmp = win_tmp(a.win_tmp)
        job_path = os.path.join(tmp, "job.json")
        os.makedirs(tmp, exist_ok=True)
        with open(job_path, "w", encoding="utf-8") as f:
            json.dump(job, f, ensure_ascii=False)
        cfg = os.path.join(tmp, "autoplay_cfg.json")
        with open(cfg, "w", encoding="utf-8") as f:
            json.dump({"modkit": to_win_path(mk)}, f, ensure_ascii=False)
        win_job = to_win_path(job_path)
        rc, out = win_bridge(["--job", win_job.replace("/", "\\")], tmp)
        print("   runner 退出码 %d" % rc)
        res = {}
        try:
            res = json.loads(out[out.index("{"):out.rindex("}") + 1])
        except Exception:
            print("   ⚠ 读不出 runner 的 JSON，原样输出：\n" + out[-1500:])
        if res:
            print("   窗口 %s，日志段 %s 行，新 dump %d 个" %
                  (res.get("window"), res.get("log_lines"), len(res.get("new_dumps") or [])))
            if res.get("notes"):
                for n in res["notes"]:
                    print("   · %s" % n)
            attribute_dumps(mk, res.get("new_dumps") or [])
            sec = res.get("log_section_path")
            if sec:
                a.log = to_wsl_path(sec)
        if rc:
            print("⚠ runner 非零退出 ⇒ 后面的判读仅供参考")

    print("⑤ 判读（verify.py all --check）")
    if not os.path.exists(log):
        print("✗ 找不到日志：%s" % log)
        return 2
    text = last_boot_section(open(log, encoding="utf-8", errors="replace").read())
    sec_path = os.path.join(os.path.dirname(os.path.abspath(log)), "autoplay_last_section.log")
    open(sec_path, "w", encoding="utf-8").write(text)
    print("   本次启动段已存：%s（%d 行）" % (sec_path, len(text.splitlines())))
    rep_dir = (os.path.join(mk, "autotest") if mk else os.path.dirname(os.path.abspath(log)))
    os.makedirs(rep_dir, exist_ok=True)
    rep = os.path.join(rep_dir, "report_%s.md" % a.scenario)
    prim = SCEN_PRIM.get(a.scenario, "all")
    cmd = [sys.executable, VERIFY, prim, "--check", "--log", sec_path, "--report", rep]
    print("   判读：%s（%s）" % (prim, "该场景对口的原语" if prim != "all" else "全部原语"))
    rc = subprocess.run(cmd).returncode
    print("   报告：%s（退出码 %d）" % (rep, rc))
    return rc


if __name__ == "__main__":
    sys.exit(main())
