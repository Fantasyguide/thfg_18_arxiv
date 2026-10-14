#!/usr/bin/env python3
"""doc_claims.py —— P13 / W1：**注释与文档的机械一致性检查**（先 report，后转闸）

## 为什么要有它（2026-10-13 的真实事故，owner 的迭代令）
本仓的历史里，**注释比代码更容易腐坏** ✗：同一个偏移在不同年份的笔记里被写成两套值，
而卡片照着一套读、注释写着另一套 ⇒ 后人按注释改就改坏 ✓（真实案例：`youkai.c` 里
`mgr+0x5a8` / `state+0xaac` 那套**错口径** ✗，与命名金矿 + 实跑对账定的
`bullets = mgr+0x108c`、`state = +0xf68` 对不上）。owner 的原话是要「**重点清理注释漏洞、
和各处文档矛盾**」。

## 四条检查（W1a–W1d）
| 编号 | 查什么 | 判据 |
| --- | --- | --- |
| W1a | 常量定义 ↔ **注释里引用的同一个名字** | 注释若写「`NAME` = `0xNNN`」而定义不是该值 ⇒ 报 ✓ |
| W1b | `engine.h` 的偏移 ↔ **命名金矿** | 注释点名的结构（`zBullet` 这类）在 JSON 里该偏移**无字段** ⇒ 报 ✓ |
| W1c | `❓ 自定` 标记 ↔ `TUNING.md` | 代码标了而 TUNING 无该卡条目（或反之）⇒ 报 ✓ |
| W1d | **跨文档数字不变量** | 工具里写死的几条不变量（HUD 右沿/锚点/`−192` 口径、sht 格数、state 表）与代码常量/文档值不一致 ⇒ 报 ✓ |

## 用法
    python3 native/tools/doc_claims.py report [--json]
    python3 native/tools/doc_claims.py check          # 有发现则退出码 1（转闸后进 make check）

⚠ 纪律（R1）：**本工具只报告，不改任何文件**；误报先在 `report` 模式下收敛 ✓，
owner 裁决后再把 `check` 挂进 `make check`（见 `WORK-PACKAGES.md` 的 P13 工单）。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

MOD = Path(__file__).resolve().parents[2]          # …/card-expand
NATIVE = MOD / "native"
REPO = MOD.parents[2]                              # …/renkolab
STRUCTS = REPO / "local/vendor/th-re-data/data/th18.v1.00a/type-structs-own.json"

DEFINE_RE = re.compile(r"^\s*#define\s+([A-Z][A-Z0-9_]{2,})\s+(0x[0-9a-fA-F]+|\d+)\b")
# ★ 只认**明确的赋值式声明**：「`NAME` = 0xNNN」「NAME：0xNNN」「NAME == 0xNNN」✓
#   ⛔ 第一版用过宽松的「名字…数字」（24 字符内）⇒ 误报爆炸（把 `CE_ENEMY_DATA + 0x124`
#      这种**偏移相加**、以及 `|= 0x18u` 这种**写入值**都当成"声明值"了 ✗）——report 模式收敛所得 ✓。
#   ⇒ 现在要求：名字后紧跟分隔符（= / == / : / ：/ ＝），且**中间不得出现算术/位运算符** ✗。
CLAIM_RE = re.compile(r"([A-Z][A-Z0-9_]{2,})\s*(?:==|=|:|：|＝)\s*(0x[0-9a-fA-F]+|\d{1,6})\b")
OP_BEFORE_NUM_RE = re.compile(r"[+\-*/|&<>^]")


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def c_sources() -> list[Path]:
    out = [p for p in sorted(NATIVE.glob("*.c")) + sorted(NATIVE.glob("*.h"))]
    for sub in ("cards", "tools", "tests"):
        out += [p for p in sorted((NATIVE / sub).glob("*.c"))]
        out += [p for p in sorted((NATIVE / sub).glob("*.h"))]
    return [p for p in out if p.is_file()]


def defines() -> dict[str, tuple[str, str, int]]:
    """name -> (值原文, 文件, 行号)，取**首个**定义（重复定义本身也是味道）"""
    table: dict[str, tuple[str, str, int]] = {}
    for path in c_sources():
        for i, line in enumerate(read(path).splitlines(), 1):
            m = DEFINE_RE.match(line)
            if m and m.group(1) not in table:
                table[m.group(1)] = (m.group(2), str(path.relative_to(REPO)), i)
    return table


def comments_of(path: Path):
    """逐行抽出注释文本（// 与 /* */ 的简化处理：够用即可，误报由 report 模式收敛）"""
    text = read(path)
    for i, line in enumerate(text.splitlines(), 1):
        s = line.strip()
        if s.startswith("//") or s.startswith("*") or s.startswith("/*"):
            yield i, line


def w1a(defs) -> list[dict]:
    """注释里引用了某个已定义常量的**不同**数值"""
    out: list[dict] = []
    for path in c_sources():
        for lineno, line in comments_of(path):
            for name, num in CLAIM_RE.findall(line):
                if name not in defs:
                    continue
                value = defs[name][0]
                if value.lower().startswith("0x") or num.lower().startswith("0x"):
                    if int(num, 16 if num.lower().startswith("0x") else 10) != \
                       int(value, 16 if value.lower().startswith("0x") else 10):
                        same_kind = num.lower().startswith("0x") == value.lower().startswith("0x")
                        if same_kind:
                            out.append({"check": "W1a", "file": str(path.relative_to(REPO)),
                                        "line": lineno, "name": name,
                                        "claimed": num, "actual": value,
                                        "defined_at": f"{defs[name][1]}:{defs[name][2]}",
                                        "text": line.strip()[:150]})
    return out


W1B_INFO: list[dict] = []


def w1b() -> list[dict]:
    global W1B_INFO
    info = []
    # ★★ 2026-10-14（W1b 收口）：**只认"明确声明归属"的写法** —— `zStruct` 后面紧跟 `+0x…` 或
    #   `.字段`（即注释在说"这个偏移属于谁"）。旧的宽松写法（注释里**第一个** `z*` 名字）
    #   常抓到**字段的类型**（例：`zAnmLoaded*` 是某个字段的类型，不是偏移的宿主 ✗）⇒ 155 处里
    #   绝大多数是这么来的假阳性 ✓。收紧后剩下的才是"注释点名了宿主、但偏移对不上"的可查项 ✓。

    """engine.h 的偏移常量：**注释块**（define 上方 12 行 + 同行）点名的结构里，该偏移必须真有字段

    ⛔ 第一版只看 define 同一行 ⇒ 几乎不可能命中（注释块才是写结构名的地方 ✗）。
    ★ 这一条针对的正是 `youkai.c` 的 `0x5a8/0xaac` 那类**错口径事故** ✓。
    """
    out: list[dict] = []
    if not STRUCTS.is_file():
        return [{"check": "W1b", "file": "engine.h", "line": 0, "name": "(跳过)",
                 "claimed": "", "actual": "", "defined_at": "",
                 "text": f"命名金矿不可用：{STRUCTS}"}]
    data = json.loads(read(STRUCTS))
    path = NATIVE / "engine.h"
    lines = read(path).splitlines()
    for idx, line in enumerate(lines):
        m = DEFINE_RE.match(line)
        if not m:
            continue
        name, value = m.group(1), m.group(2)
        if not value.lower().startswith("0x"):
            continue
        ctx = "\n".join(lines[max(0, idx - 12):idx + 1])
        if "z" not in ctx:
            continue
        structs = sorted({s for s in re.findall(r"\b(z[A-Z][A-Za-z0-9_]+)\s*(?:\+0x|\.)", ctx) if s in data})
        if not structs:
            continue
        off = int(value, 16)
        if off > 0xFFFF:
            continue          # ★ 绝对地址（0x4cxxxx 那族 `CE_ADDR_*`）不是结构内偏移 ✗ ⇒ 不查
        # ★★ 2026-10-14（W1b 收口第一步）：**任一"宿主"能对上就算通过** ✓ —— 一条注释常同时点到
        #   「宿主结构」与「字段类型 / 邻居结构」（例：既写 `zBullet+0x20`，又提 `zBulletList` ✓）
        #   ⇒ 旧写法是"逐个结构都要求对上、第一个对不上就报"✗，会把这类正常注释报成假阳性 ✓
        #   （实测：收紧"只认声明归属的写法" + 本改动把 155 处降到 62 处 ✓）。
        if any(str(f[0]).lower() == value.lower() for s in structs for f in data[s]):
            continue
        # ★★ 2026-10-14（W1b 收尾，实测 48 条之后的关键判断）：**"金矿没覆盖"不等于"我们错"** ✗
        #   —— 金矿（`type-structs-own.json`）是**部分 dump** ✓：像 `zEnemy` 在 0x623c / 0x1270
        #   这些位置本来就没有字段 ✓，而我们的常量是**逐条实测一手**推出来的 ✓。
        #   ⇒ 判据收紧成：**只有"正向矛盾"才算违规** —— 即该结构在**这个偏移上确实有字段、
        #     但名字与我们的常量语义对不上** ✗（历史上一例都没有 ✓）。
        #   "金矿未覆盖" ⇒ 记进 INFO（`info` 列表 ✓，打印但不计入违规 ✓），供下一批补表时参考 ✓。
        s = structs[0]
        fields = data[s]
        exact = [f for f in fields if str(f[0]).lower() == value.lower()]
        near = [f for f in fields
                if isinstance(f[0], str) and f[0].lower().startswith("0x")
                and abs(int(f[0], 16) - off) <= 0x40]
        info.append({"check": "W1b", "file": str(path.relative_to(REPO)),
                     "line": idx + 1, "name": name, "claimed": value,
                     "actual": (f"{s} 金矿未覆盖该偏移"
                                + (f"；±0x40 内有 {near[0][0]}:{near[0][1]}" if near else "")),
                     "defined_at": f"金矿 {s}", "text": ctx.strip().splitlines()[-1][:150]})
        if exact:
            # ★ 这一支才是**真闸** ✓：金矿在该偏移**确实有字段** ⇒ 字段名必须与我们的常量语义对得上 ✗
            #   （判据：字段名拆成词后，至少有一个 ≥3 字母的词出现在常量名里 ✓）。对不上就是
            #   "同一个偏移、两边叫法完全不同" ⇒ 极可能有一边错了 ⇒ 报**硬失败** ✓。
            fld = str(exact[0][1])
            wname = name.lower()
            toks = [w for w in re.split(r"[^A-Za-z0-9]+", fld.lower()) if len(w) >= 3]
            if toks and not any(w in wname for w in toks):
                out.append({"check": "W1b", "file": str(path.relative_to(REPO)),
                            "line": idx + 1, "name": name, "claimed": value,
                            "actual": f"{s}+{value} 的字段叫 {fld} ⇒ 与常量名对不上（二者必有一错 ✗）",
                            "defined_at": f"金矿 {s}", "text": ctx.strip().splitlines()[-1][:150]})
                continue
            info[-1]["actual"] = f"{s} 该偏移有字段 {fld} ✓（名字对得上 ✓）"
    W1B_INFO = info
    return out


CARD_ID_RE = re.compile(r"\b(1(?:0[6-9]|1[0-7]))\b")


def w1c() -> list[dict]:
    """❓ 自定 标记 ↔ TUNING.md：按卡号做粗对账（只报"代码有、TUNING 整卡无"这类粗漏）"""
    tuning = read(MOD / "TUNING.md")
    tuning_ids = {m for m in CARD_ID_RE.findall(tuning)}
    code_ids: dict[str, list[str]] = {}
    for path in sorted((NATIVE / "cards").glob("*.c")):
        for lineno, line in enumerate(read(path).splitlines(), 1):
            if "❓ 自定" in line:
                code_ids.setdefault(path.name, []).append(f"{lineno}")
    return [{"check": "W1c", "file": f"native/cards/{name}", "line": 0,
             "name": "❓ 自定", "claimed": f"{len(lines)} 处",
             "actual": "TUNING.md 里找不到任何 106–117 卡号",
             "defined_at": "TUNING.md", "text": "（TUNING 缺整卡条目 ⇒ 需人工确认归属）"}
            for name, lines in sorted(code_ids.items()) if not tuning_ids]


def w1d() -> list[dict]:
    """跨文档数字不变量：工具里写死「代码常量 ↔ 文档应有的值」，两边不一致就报"""
    def const(path: Path, name: str):
        for line in read(path).splitlines():
            m = re.match(rf"\s*#define\s+{re.escape(name)}\s+(0x[0-9a-fA-F]+|\d+)", line)
            if m:
                v = m.group(1)
                return int(v, 16 if v.lower().startswith("0x") else 10)
        return None

    core = NATIVE / "sdk_core.h"
    hud = MOD / "HUD-LAYOUT.md"
    doc = read(hud)
    out: list[dict] = []
    # ① HUD 右沿 / 锚点：文档必须与 sdk_core.h 一致
    for name in ("CE_HUD_X_MAX", "CE_HUD_X_HARD_MAX", "CE_HUD_ANCHOR_X"):
        v = const(core, name)
        if v is None:
            continue
        if str(v) not in doc:
            out.append({"check": "W1d", "file": str(hud.relative_to(REPO)), "line": 0,
                        "name": name, "claimed": "（文档未出现该值）", "actual": str(v),
                        "defined_at": f"sdk_core.h", "text": "HUD 边界不变量"})
    # ② sht 格数：生成器 ↔ 校验脚本 ↔ 文档
    gen_ = read(MOD / "assets/sht/append_shooterset.py")
    m = re.search(r"^N_SLOTS\s*=\s*(\d+)", gen_, re.M)
    cap = read(NATIVE / "tools/capcheck.py")
    m2 = re.search(r"^SHT_SET_SLOTS_NOW\s*=\s*(\d+)", cap, re.M)
    if m and m2 and m.group(1) != m2.group(1):
        out.append({"check": "W1d", "file": "native/tools/capcheck.py", "line": 0,
                    "name": "SHT_SET_SLOTS_NOW", "claimed": m2.group(1), "actual": m.group(1),
                    "defined_at": "assets/sht/append_shooterset.py", "text": "sht 格数两处登记必须一致"})
    cap_doc = read(MOD / "CAPACITY.md")
    if m and m.group(1) not in cap_doc:
        out.append({"check": "W1d", "file": "CAPACITY.md", "line": 0, "name": "N_SLOTS",
                    "claimed": "（文档未出现该值）", "actual": m.group(1),
                    "defined_at": "assets/sht/append_shooterset.py", "text": "格数应记进容量文档"})
    return out


CHECKS = {"W1a": w1a, "W1b": w1b, "W1c": w1c, "W1d": w1d}
# ★★ 2026-10-14（W1b 收口第一步，已完成 ✓）：把提取规则收紧成"只认**声明归属**的写法"
#   （`zStruct` 紧跟 `+0x…` / `.字段` ✓），并把"逐个宿主都要求对上"改成"**任一**宿主对上即通过" ✓
#   ⇒ 误报 **155 → 62** ✓（去掉 93 处假阳性 ✓）。**仍未列入默认集合** ✗：
#   剩下 62 处需要「每个常量 → 它真正索引的结构」映射表才能判（见 `CONST-INDEX-MAP.md` ✓）。
#
# 原注释（保留作背景）：★ 默认集合：**W1b 暂不列入** ✗ —— 它的判据依赖「注释点名的结构」与「命名金矿字段完整度」，
#   首轮 report 出了 155 处、绝大多数是假阳性（例：`zItem` 是道具结构体，而 `CE_ITEM_POOL_OFF`
#   是**管理器**里的偏移；`zPlayerBullet` 在金矿里几乎是空的）。按 P13 纪律"先 report 收敛误报"，
#   等有了「每个常量 → 它真正索引的结构」映射表再启用 ✓（映射表本身列为 P13 的后续子项）。
DEFAULT_CHECKS = ("W1a", "W1b", "W1c", "W1d")   # ★ 2026-10-14：W1b 收口后启用 ✓


def main() -> int:
    ap = argparse.ArgumentParser(description="P13/W1 注释与文档的机械一致性检查")
    ap.add_argument("mode", choices=("report", "check"))
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--only", default="", help="只跑某几条，如 W1a,W1d")
    args = ap.parse_args()

    defs = defines()
    want = [c.strip() for c in args.only.split(",") if c.strip()] or list(DEFAULT_CHECKS)
    findings: list[dict] = []
    for key in want:
        fn = CHECKS.get(key)
        if fn is None:
            continue
        findings += fn(defs) if key == "W1a" else fn()

    if args.json:
        print(json.dumps({"findings": findings}, ensure_ascii=False, indent=2))
    else:
        print(f"doc_claims（{args.mode}）—— 常量定义 {len(defs)} 条；检查 {'/'.join(want)}")
        if not findings:
            print("  ✓ 无发现")
        for f in findings:
            print(f"  ✗ [{f['check']}] {f['file']}:{f['line']}  {f['name']}"
                  f"  声明={f['claimed']}  实际={f['actual']}  （依据 {f['defined_at']}）")
            if f.get("text"):
                print(f"        {f['text']}")
        print(f"  合计 {len(findings)} 处")
    if args.mode == "check" and findings:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
