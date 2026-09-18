#!/usr/bin/env python3
"""release_guard.py —— **发布前闸 + 发布后记账**：把「能不能发 / 发了什么」变成机器可读的单一状态源。

## 为什么要有它（2026-09-18，一次真实事故）

那天上午，一个会话在 `local/DLL-SYNC.md` 的 ⛔ 冻结期内跑了 3 次 `make release`（**它只读了那份
文件的开头**，没读到 474 / 540 两节 ⛔）。冻结状态当时只存在于**人读的文档**里，其中一半还在
`local/`（不入库，协作者 clone 看不到）⇒ 「冻结」对脚本**不存在**。

配套的另一个洞：发布完只留下一句「已发版」，**没有一处**同时记着
「patch js / DLL / 四份 `pl0X.sht` / `abcard.anm` / `ability.anm` / `cards.js` 是哪一批」——
于是「部署侧现在到底是什么组合」只能靠翻日志与 md5 猜（AUDIT §BE 详录 #49 就卡在这上面）。

## 状态文件（**入库**，`RELEASE-STATE.json`）

```json
{
  "freeze": { "active": false, "since": "...", "reason": "...", "ref": "AUDIT §BE 详录 #49" },
  "last_release": { "at": "...", "agent": "...", "modkit": "...",
                    "patch_md5": "...", "dll_md5": "...", "sht_md5": ["...×4"],
                    "abcard_md5": "...", "ability_md5": "...", "cards_md5": "...", "files_md5": "..." },
  "history": [ ... 最近 10 次，倒序 ... ]
}
```

入库的理由：发布内容属于**所有人的事实**（协作者要能看见"部署侧现在是哪一批"）；
`freeze` 也放这里 ⇒ 抬冻结 = 一次提交（有记录、有理由），不再是某人本机文件里的一句话。

## 用法

    python3 tools/release_guard.py status                      # 看状态 + 与上次发布记录的漂移
    python3 tools/release_guard.py check  [--modkit DIR]       # 发布前闸（冻结 ⇒ 拒；+ 跑既有不变式门）
    python3 tools/release_guard.py record [--modkit DIR]       # 发布后记账（写状态 + 追加本机日志）
    python3 tools/release_guard.py freeze   --reason TEXT [--ref TEXT]
    python3 tools/release_guard.py unfreeze --reason TEXT

`check` 里跑的都是**已有**的门（不重复实现判据）：`sites.py check`（patch 声明 ↔ `.def` 导出双向对账、
站点预算、生成物形状）与 `tools/check_deploy_pair.py`（DLL ↔ 游戏里 `pl0X.sht` 同批）。
退出码：0 = 可发；2 = 冻结中（拒发）；3 = 状态文件坏；其它非 0 = 门不过。
"""
import argparse
import hashlib
import json
import os
import subprocess
import sys
from datetime import datetime, timezone, timedelta

HERE = os.path.dirname(os.path.abspath(__file__))          # native/tools
NATIVE = os.path.dirname(HERE)                             # native
MOD = os.path.dirname(NATIVE)                              # card-expand
def _repo_root(start):
    """向上找 .git（别用固定的 `../../..` —— 沙箱/副本里深度不同会算错）。"""
    d = start
    for _ in range(8):
        if os.path.isdir(os.path.join(d, ".git")):
            return d
        nd = os.path.dirname(d)
        if nd == d:
            break
        d = nd
    return os.path.abspath(os.path.join(start, "..", "..", ".."))


REPO = _repo_root(MOD)
STATE = os.path.join(MOD, "RELEASE-STATE.json")
PATCH = os.path.join(MOD, "patch", "th18.v1.00a.js")
DLL = os.path.join(NATIVE, "th18_card_expand.dll")
SHT = [os.path.join(NATIVE, "build", "sht", "pl%02d.sht" % i) for i in range(4)]
ANMS = [os.path.join(NATIVE, "build", n) for n in ("abcard.anm", "ability.anm")]
CARDS = os.path.join(MOD, "patch", "th18", "cards.js")
FILES = os.path.join(MOD, "patch", "files.js")
MDLOG = os.path.join(REPO, "local", "DLL-SYNC.md")         # 只在本机生效的日志（没有就跳过）
SHA = "0123456789abcdef"


def now():
    return datetime.now(timezone(timedelta(hours=8))).strftime("%Y-%m-%d %H:%M:%S%z")


def md5(p):
    if not os.path.isfile(p):
        return None
    h = hashlib.md5()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def agent_label():
    """谁在发：优先环境变量（会话自报），否则 git 身份 + 主机名 —— 单一 git 身份下归属不可考，
    这一行至少把「哪台机、哪个会话标签」记下来。"""
    lab = os.environ.get("AGENT_SESSION")
    if lab:
        return lab
    who = subprocess.run(["git", "config", "user.name"], cwd=REPO, capture_output=True, text=True).stdout.strip()
    import socket
    return "%s@%s" % (who or "?", socket.gethostname())


def load_state():
    if not os.path.isfile(STATE):
        return {"freeze": {"active": False, "since": None, "reason": None, "ref": None},
                "last_release": None, "history": []}
    try:
        with open(STATE, encoding="utf-8") as f:
            s = json.load(f)
    except (ValueError, OSError) as e:
        print("❌ %s 读不出来（%s）—— 修好它再发布" % (os.path.relpath(STATE, REPO), e))
        sys.exit(3)
    s.setdefault("freeze", {"active": False, "since": None, "reason": None, "ref": None})
    s.setdefault("last_release", None)
    s.setdefault("history", [])
    return s


def save_state(s):
    s["history"] = s["history"][:10]
    tmp = STATE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(s, f, indent=2, ensure_ascii=False, sort_keys=False)
        f.write("\n")
    os.replace(tmp, STATE)
    print("状态已写：%s" % os.path.relpath(STATE, REPO))


def snapshot():
    return {
        "patch_md5": md5(PATCH),
        "dll_md5": md5(DLL),
        "sht_md5": [md5(p) for p in SHT],
        "abcard_md5": md5(ANMS[0]),
        "ability_md5": md5(ANMS[1]),
        "cards_md5": md5(CARDS),
        "files_md5": md5(FILES),
    }


def show(snap, prefix="  "):
    print(prefix + "patch  th18.v1.00a.js  %s" % snap.get("patch_md5"))
    print(prefix + "DLL    th18_card_expand  %s" % snap.get("dll_md5"))
    print(prefix + "sht    pl00..pl03        %s" % ", ".join(str(x) for x in snap.get("sht_md5", [])))
    print(prefix + "anm    abcard / ability  %s / %s" % (snap.get("abcard_md5"), snap.get("ability_md5")))
    print(prefix + "cards / files            %s / %s" % (snap.get("cards_md5"), snap.get("files_md5")))


def run(cmd):
    print("  $ " + " ".join(cmd), flush=True)
    return subprocess.run(cmd, cwd=NATIVE).returncode


def cmd_status(s, a):
    fz = s["freeze"]
    print("发布状态：%s" % ("⛔ **冻结中**" if fz.get("active") else "✅ 未冻结"))
    if fz.get("active"):
        print("   自 %s 起：%s%s" % (fz.get("since"), fz.get("reason"),
                                    ("（%s）" % fz["ref"]) if fz.get("ref") else ""))
    last = s.get("last_release")
    if not last:
        print("   还没有发布记录（第一次跑 `record` 之后这里会有一份快照）")
        return 0
    print("   上次发布：%s by %s → %s" % (last.get("at"), last.get("agent"), last.get("modkit")))
    show(last)
    cur = snapshot()
    drift = [k for k in cur if cur[k] != last.get(k)]
    print("   与工作区现状相比：%s" % ("**全部一致**" if not drift else "有 %d 项不同（%s）"
                                     % (len(drift), ", ".join(drift))))
    return 0


def cmd_check(s, a):
    if s["freeze"].get("active"):
        fz = s["freeze"]
        print("⛔ 发布被冻结（%s 起）：%s%s" % (fz.get("since"), fz.get("reason"),
                                              ("（%s）" % fz["ref"]) if fz.get("ref") else ""))
        print("   抬冻结：python3 tools/release_guard.py unfreeze --reason \"<为什么现在可以发>\"")
        print("   （冻结可以是「等裁决」也可以是「有并发会话在动这棵树」——先看理由再决定）")
        return 2
    bad = 0
    print("① 站点/声明/生成物不变式（sites.py check）")
    if run([sys.executable, "sites.py", "check"]) != 0:
        bad += 1
    if a.modkit:
        print("② DLL ↔ 游戏里的 pl0X.sht 同批（check_deploy_pair.py）")
        if run([sys.executable, os.path.join(HERE, "check_deploy_pair.py"), DLL, a.modkit]) != 0:
            bad += 1
    else:
        print("② DLL ↔ pl0X.sht：没给 `--modkit <dir>` ⇒ 跳过（发布前请带上）")
    if bad:
        print("❌ 发布闸不过（%d 项红）—— 别发" % bad)
        return 1
    print("✅ 发布闸全过（冻结未生效、既有不变式全绿）")
    return 0


def cmd_record(s, a):
    snap = snapshot()
    miss = [k for k, v in snap.items() if v is None]
    entry = {"at": now(), "agent": agent_label(), "modkit": a.modkit or "(unknown)"}
    entry.update(snap)
    s["history"] = [entry] + [h for h in s.get("history", []) if h.get("at") != entry["at"]]
    s["last_release"] = entry
    save_state(s)
    print("本次发布快照：")
    show(snap)
    if miss:
        print("  ⚠ 缺文件（记成 null，说明没构建或路径变了）：%s" % ", ".join(miss))
    if os.path.isfile(MDLOG):
        with open(MDLOG, "a", encoding="utf-8") as f:
            f.write("\n### 发布记录 %s（`release_guard.py record`）\n\n" % entry["at"])
            f.write("| 项 | md5 |\n| --- | --- |\n")
            f.write("| `th18.v1.00a.js` | `%s` |\n" % snap["patch_md5"])
            f.write("| `th18_card_expand.dll` | `%s` |\n" % snap["dll_md5"])
            f.write("| `pl0X.sht` ×4 | `%s` |\n" % "`, `".join(str(x) for x in snap["sht_md5"]))
            f.write("| `abcard.anm` / `ability.anm` | `%s` / `%s` |\n" % (snap["abcard_md5"], snap["ability_md5"]))
            f.write("| `cards.js` / `files.js` | `%s` / `%s` |\n" % (snap["cards_md5"], snap["files_md5"]))
            f.write("\n发布者：`%s`；目标 modkit：`%s`\n" % (entry["agent"], entry["modkit"]))
        print("已追加到 %s" % os.path.relpath(MDLOG, REPO))
    return 0


def cmd_freeze(s, a, active):
    s["freeze"] = {"active": active, "since": now(), "reason": a.reason,
                   "ref": getattr(a, "ref", None), "by": agent_label()}
    save_state(s)
    print("%s：%s" % ("⛔ 已冻结发布" if active else "✅ 已抬冻结", a.reason))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("status"); p.set_defaults(fn=cmd_status)
    p = sub.add_parser("check"); p.add_argument("--modkit", default=None); p.set_defaults(fn=cmd_check)
    p = sub.add_parser("record"); p.add_argument("--modkit", default=None); p.set_defaults(fn=cmd_record)
    p = sub.add_parser("freeze")
    p.add_argument("--reason", required=True); p.add_argument("--ref", default=None)
    p.set_defaults(fn=lambda s, a: cmd_freeze(s, a, True))
    p = sub.add_parser("unfreeze"); p.add_argument("--reason", required=True)
    p.set_defaults(fn=lambda s, a: cmd_freeze(s, a, False))
    a = ap.parse_args()
    return a.fn(load_state(), a)


if __name__ == "__main__":
    sys.exit(main())
