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
                               (60.0, "hold BOMB"), (61.0, "release BOMB")]),
    "emitter-close": dict(deck="sys12", seconds=120, why="打掉某只敌机的发弹口（gate 生效）",
                          timeline=[(1.0, "hold SHOT"), (20.0, "hold SLOW"), (50.0, "release SLOW"),
                                    (60.0, "release SHOT")]),
    "card118": dict(deck="sys12", seconds=90, why="切到 118 并按 C 发动（应发 12 张卡）",
                    timeline=[(1.0, "hold CARD_SWITCH"), (1.2, "release CARD_SWITCH"),
                              (2.0, "hold CARD_USE"), (2.2, "release CARD_USE")]),
    "all12": dict(deck="sys12", seconds=180, why="全装混沌跑：12 张同时在编成，看有没有崩 + 各原语判据",
                  timeline=[(1.0, "hold SHOT"), (6.0, "hold SLOW"), (20.0, "release SLOW"),
                            (25.0, "hold UP"), (45.0, "release UP"), (60.0, "hold BOMB"),
                            (61.0, "release BOMB"), (90.0, "hold CARD_USE"), (90.5, "release CARD_USE")]),
}


def find_modkit(explicit):
    if explicit:
        return explicit
    return os.environ.get("MODKIT_DIR", "")


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
    ap.add_argument("--scale", type=float, default=1.0,
                    help="时间轴整体缩放（<1 = 加速跑；调试编排时用）")
    a = ap.parse_args()

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
        print("② 请先手动启动游戏（本版不自动启动 —— 启动器是 Windows 批处理），窗口前台后回车")
        try:
            input()
        except EOFError:
            print("   （非交互环境 ⇒ 直接开始注入；请确保游戏已在前台）")
        print("③ 代打：按时间轴注入")
        t0 = time.time()
        for t, act, key, down in steps:
            want = t * a.scale
            while time.time() - t0 < want:
                time.sleep(0.02)
            print("   t=%5.2fs %s" % (time.time() - t0, act))
            focus_and_send(key, down, False)
        while time.time() - t0 < dur * a.scale:
            time.sleep(0.1)
        print("④ 代打结束（%.0fs）" % (time.time() - t0))

    print("⑤ 判读（verify.py all --check）")
    if not os.path.exists(log):
        print("✗ 找不到日志：%s" % log)
        return 2
    text = last_boot_section(open(log, encoding="utf-8", errors="replace").read())
    sec_path = os.path.join(os.path.dirname(os.path.abspath(log)), "autoplay_last_section.log")
    open(sec_path, "w", encoding="utf-8").write(text)
    print("   本次启动段已存：%s（%d 行）" % (sec_path, len(text.splitlines())))
    return subprocess.run([sys.executable, VERIFY, "all", "--check", "--log", sec_path]).returncode


if __name__ == "__main__":
    sys.exit(main())
