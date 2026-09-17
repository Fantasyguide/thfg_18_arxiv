#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []          # 只用标准库（ctypes/user32）⇒ 无需第三方依赖
# ///
"""Windows 侧测试 runner —— 真正"跑游戏"的那一半（在 Windows 上执行，由 WSL 侧经互操作调用）。

为什么要有这一半（用户 2026-09-17 口径）：**测试必须由 Windows 侧走** —— 启动游戏、把窗口拉到前台、
注入按键、读 Windows 侧的日志与崩溃 dump，都只能在 Windows 上做。WSL 侧只负责编排与判读。

与 `autoplay.py` 的分工
- `autoplay.py`（WSL 侧）：场景/时间轴 → 生成 job JSON → 用 `cmd.exe` 调本脚本 → 收结果 → 调 `verify.py` 判读。
- 本脚本（Windows 侧）：读 job JSON → （可选）启动游戏 → 等窗口 → 按时间轴注入按键 → 等 → 收集日志段/新 dump
  → 写 `result.json`。

用法（Windows 侧 Python 3.11）：
    python autoplay_win.py --smoke --modkit "D:\\项目\\幻想指南\\18\\th18_modkit"
    python autoplay_win.py --job <job.json>

`--smoke` 是**不需要游戏**的自检（本文件的关键设计）：它证明"WSL→Windows 桥"通了、路径对了、
Python 与 ctypes 可用、以及（若游戏正在跑）能不能找到窗口 —— 也就是把"必须由人来验证"的部分压到最小。
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import json
import os
import re
import subprocess
import sys
import time

KEYMAP = {"SHOT": "SHOT", "BOMB": "BOMB", "SLOW": "SLOW", "CARD_USE": "CARD_USE",
          "CARD_SWITCH": "CARD_SWITCH", "UP": "UP", "DOWN": "DOWN", "LEFT": "LEFT", "RIGHT": "RIGHT"}
VK = {"SHOT": 0x5A, "BOMB": 0x58, "CARD_USE": 0x43, "CARD_SWITCH": 0x44, "SLOW": 0x10,
      "UP": 0x26, "DOWN": 0x28, "LEFT": 0x25, "RIGHT": 0x27, "ENTER": 0x0D, "PAUSE": 0x1B}
KEYUP = 0x0002
BOOT_RE = re.compile(r"^=== th18_card_expand .*===$")


def log(*a):
    print(*a, flush=True)


def find_game_window():
    """按进程名找 th18.exe 的顶层窗口 —— 不依赖窗口标题（标题可能被 thcrap/汉化改掉）。"""
    u = ctypes.windll.user32
    k = ctypes.windll.kernel32
    found = []

    def proc_name(pid):
        h = k.OpenProcess(0x1000, False, pid)          # PROCESS_QUERY_LIMITED_INFORMATION
        if not h:
            return ""
        buf = ctypes.create_unicode_buffer(1024)
        sz = wt.DWORD(1024)
        ok = k.QueryFullProcessImageNameW(h, 0, buf, ctypes.byref(sz))
        k.CloseHandle(h)
        return os.path.basename(buf.value) if ok else ""

    CB = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)

    def cb(hwnd, _):
        if not u.IsWindowVisible(hwnd):
            return True
        pid = wt.DWORD()
        u.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if proc_name(pid.value).lower() == "th18.exe":
            found.append(hwnd)
        return True

    u.EnumWindows(CB(cb), 0)
    return found[0] if found else None



DECKS = {"sys12": dict(mode="exact", cap=12, deck=list(range(106, 118))),
         "one118": dict(mode="exact", cap=0, deck=[118]),
         "off": dict(mode="off", cap=0, deck=[])}


def here(name):
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), name)


def load_scenarios():
    p = here("scenarios.json")
    return json.load(open(p, encoding="utf-8")) if os.path.exists(p) else {}


def write_deck(mk, name):
    d = DECKS[name]
    obj = {"note": "由 autotest 写入的临时测试卡组（复原：改成 mode off 或删本文件）",
           "mode": d["mode"], "deck": d["deck"]}
    if d["cap"]:
        obj["cap"] = d["cap"]
    p = os.path.join(mk, "mods", "start_deck.js")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)
    log("   卡组 %s → %s" % (name, p))


def judge(mk, prim, log_path, rep):
    """本地判读（同目录 verify.py；纯标准库，Windows 上照跑）。"""
    v = here("verify.py")
    if not os.path.exists(v):
        log("   （没有 verify.py，跳过判读）"); return 0
    cmd = [sys.executable, v, prim, "--check", "--log", log_path, "--report", rep]
    log("   判读：%s" % " ".join(cmd[-6:]))
    return subprocess.run(cmd).returncode


def key(vkname, down):
    vk = VK[vkname]
    ctypes.windll.user32.keybd_event(vk, 0, 0 if down else KEYUP, 0)


def boots(text):
    lines = text.splitlines()
    idx = [i for i, l in enumerate(lines) if BOOT_RE.match(l.strip())]
    return "\n".join(lines[idx[-1]:]) if idx else text


def main():
    ap = argparse.ArgumentParser(description="Windows 侧测试 runner")
    ap.add_argument("--modkit", default=os.environ.get("MODKIT_DIR", ""))
    ap.add_argument("--job", default="", help="job JSON（含 scenario/seconds/scale/steps/launch）")
    ap.add_argument("--smoke", action="store_true", help="只做环境自检（不需要游戏）")
    ap.add_argument("--list", action="store_true", help="列出场景（读同目录 scenarios.json）")
    ap.add_argument("--scenario", default="", help="跑指定场景")
    ap.add_argument("--all", action="store_true", help="跑全部场景（八原语 + all12）")
    ap.add_argument("--deck", default="", help="写起手卡组：sys12 | one118 | off")
    ap.add_argument("--judge", action="store_true", help="跑完用同目录 verify.py 判读出报告")
    ap.add_argument("--seconds", type=float, default=0, help="覆盖场景时长")
    a = ap.parse_args()

    scns0 = load_scenarios()
    if a.list:
        if not scns0:
            print("(没有 scenarios.json —— 用 --install-win 从 WSL 侧部署，或把它放到本文件旁边)"); return 2
        for k, v in scns0.items():
            print("%-14s %-6s %4ds  %s" % (k, v.get("deck", "?"), v.get("seconds", 0), v.get("why", "")))
        return 0

    mk = a.modkit
    if not mk:                      # 参数走 JSON（避免中文路径过 cmd 的编码坑）
        for nm in ("autotest.cfg.json", "autoplay_cfg.json"):
            cfg = os.path.join(os.path.dirname(os.path.abspath(__file__)), nm)
            if os.path.exists(cfg):
                mk = json.load(open(cfg, encoding="utf-8")).get("modkit", "")
                if mk:
                    break
    out = {"ok": False, "platform": sys.platform, "python": sys.version.split()[0],
           "modkit": mk, "steps": 0, "window": None, "log_section": "", "log_lines": 0,
           "new_dumps": [], "notes": []}

    if not mk or not os.path.isdir(mk):
        out["notes"].append("modkit 目录不存在：%r（给 --modkit 或设 MODKIT_DIR）" % mk)
        print(json.dumps(out, ensure_ascii=True)); return 2
    out["launcher"] = os.path.join(mk, "启动TH18mod.bat")
    out["launcher_exists"] = os.path.exists(out["launcher"])
    out["game_exe"] = os.path.join(mk, "game", "th18.exe")
    out["game_exe_exists"] = os.path.exists(out["game_exe"])
    out["log"] = os.path.join(mk, "game", "th18_card_expand.log")
    out["dumps_dir"] = os.path.join(mk, "crashdumps")

    hwnd = find_game_window()
    out["window"] = hex(hwnd) if hwnd else None
    if not hwnd:
        out["notes"].append("没找到 th18.exe 的窗口（游戏没在跑 = 正常；--smoke 不启动游戏）")

    scns = load_scenarios()

    if a.list:
        for k, v in scns.items():
            print("%-14s %-6s %4ds  %s" % (k, v.get("deck", "?"), v.get("seconds", 0), v.get("why", "")))
        return 0

    if a.scenario or a.all:
        names = list(scns) if a.all else [a.scenario]
        bad = [n for n in names if n not in scns]
        if bad:
            print("✗ 未知场景：%s（--list 看台账）" % ", ".join(bad)); return 2
        rc_all = 0
        for i, n in enumerate(names, 1):
            print("\n---- [%d/%d] %s ----" % (i, len(names), n))
            scn = scns[n]
            if a.deck:
                write_deck(mk, a.deck)
            steps = scn.get("timeline", [])
            dur = a.seconds or float(scn.get("seconds", 60))
            env = dict(os.environ)
            sub = [sys.executable, os.path.abspath(__file__), "--modkit", mk,
                   "--job", "", "--scenario-run", n]
            # 直接把时间轴喂给本文件的主流程（避免递归解析）：改成写 job 再调自己
            jobp = here("job_last.json")
            jd = {"scenario": n, "seconds": dur, "scale": 1.0, "launch": bool(a.launch),
                  "steps": [{"t": t, "act": act, "key": KEYMAP.get(act.split(" ", 1)[1], ""),
                             "down": act.startswith("hold")} for t, act in steps]}
            with open(jobp, "w", encoding="utf-8") as f:
                json.dump(jd, f, ensure_ascii=False)
            r = subprocess.run([sys.executable, os.path.abspath(__file__), "--modkit", mk, "--job", jobp])
            rc = r.returncode
            if a.judge and not rc:
                prim = scn.get("prim", "all")
                rp = os.path.join(mk, "autotest", "report_%s.md" % n)
                rc |= judge(mk, prim, os.path.join(mk, "autotest", "last_section.log"), rp)
            rc_all |= rc
        mk_at = os.path.join(mk, "autotest")
        os.makedirs(mk_at, exist_ok=True)
        with open(os.path.join(mk_at, "summary.md"), "w", encoding="utf-8") as f:
            f.write("# TH18 自动验收（Windows 侧跑）\n\n")
            f.write("| 场景 | 结果 | 明细 |\n| --- | --- | --- |\n")
            for n in names:
                f.write("| %s | 见明细 | `report_%s.md` |\n" % (n, n))
        print("\n汇总：%s" % os.path.join(mk_at, "summary.md"))
        return rc_all

    if a.smoke:
        out["ok"] = bool(out["launcher_exists"] and out["game_exe_exists"])
        out["notes"].append("smoke：桥通了（WSL 能调起 Windows Python 并拿到这张表）")
        print(json.dumps(out, ensure_ascii=True, indent=2))
        return 0 if out["ok"] else 1

    job = json.load(open(a.job, encoding="utf-8")) if a.job else {}
    steps = job.get("steps") or []
    scale = float(job.get("scale", 1.0))
    dur = float(job.get("seconds", 0))
    out["steps"] = len(steps)

    dumps_before = set(os.listdir(out["dumps_dir"])) if os.path.isdir(out["dumps_dir"]) else set()

    if job.get("launch") and not hwnd:
        log("① 启动游戏：%s" % out["launcher"])
        subprocess.Popen(["cmd", "/c", "start", "", out["launcher"]], cwd=mk, shell=False)
        for _ in range(120):                       # 最多等 60s
            time.sleep(0.5)
            hwnd = find_game_window()
            if hwnd:
                break
        out["window"] = hex(hwnd) if hwnd else None
        if not hwnd:
            out["notes"].append("启动后 60s 内没找到窗口 ⇒ 中止注入（游戏没起来？）")
            print(json.dumps(out, ensure_ascii=True, indent=2)); return 1
    if not hwnd:
        out["notes"].append("没有窗口可注入 ⇒ 中止（给 job.launch=true 或先手动开游戏）")
        print(json.dumps(out, ensure_ascii=True, indent=2)); return 1

    log("② 窗口 %s，把它拉到前台" % out["window"])
    ctypes.windll.user32.SetForegroundWindow(hwnd)
    time.sleep(0.5)

    log("③ 按时间轴注入 %d 步（scale %.2f）" % (len(steps), scale))
    t0 = time.time()
    for st in steps:
        want = float(st["t"]) * scale
        while time.time() - t0 < want:
            time.sleep(0.02)
        key(st["key"], bool(st["down"]))
        log("   t=%6.2fs %-14s" % (time.time() - t0, st.get("act", st["key"])))
    while time.time() - t0 < dur * scale:
        time.sleep(0.1)

    log("④ 收尾：读日志 + 找新 dump")
    if os.path.exists(out["log"]):
        text = open(out["log"], encoding="utf-8", errors="replace").read()
        sec = boots(text)
        out["log_section"] = sec
        out["log_lines"] = len(sec.splitlines())
        sec_path = os.path.join(mk, "autotest")
        os.makedirs(sec_path, exist_ok=True)
        open(os.path.join(sec_path, "last_section.log"), "w", encoding="utf-8").write(sec)
        out["log_section_path"] = os.path.join(sec_path, "last_section.log")
    if os.path.isdir(out["dumps_dir"]):
        out["new_dumps"] = sorted(set(os.listdir(out["dumps_dir"])) - dumps_before)
    out["ok"] = True
    print(json.dumps({k: v for k, v in out.items() if k != "log_section"},
                     ensure_ascii=True, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
