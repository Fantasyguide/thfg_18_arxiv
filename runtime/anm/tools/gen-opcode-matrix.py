#!/usr/bin/env python3
"""从 thpages 的 anm.ts 生成 `runtime/anm/OVERVIEW.md` 里的 opcode 状态矩阵。

**为什么不手写 135 行**：thpages（`local/vendor/thpages/js/tables/reference/anm.ts`）
按游戏继承记录 opcode 表——`refByOpcode.set('16', new Map([...refByOpcode.get('15')!.entries(), ...]))`
这类链式 spread，th16（游戏键 `'16'`）= th15 的表 + 两条新增。助记名在 `byRefId` 里，
少数几条带 `wip: 2`（社区自己标"不确定"，对应 Global Constraints 里"社区标 wip:2 的 6 条"）。

    python3 runtime/anm/tools/gen-opcode-matrix.py           # 重写 OVERVIEW.md 里两个
                                                               # 注释锚点之间的表格
    python3 runtime/anm/tools/gen-opcode-matrix.py --check    # 只校验+跑自校验，不写文件（CI 用）

**解析方式**：这是 TypeScript，不是 JSON，这里用正则抠字段，不引入 Node/ts 工具链
（本项目零依赖，见 `runtime/anm/package.json`）。这类正则解析天然脆弱——已知的两个前提，
任何一条被 thpages 上游打破，本脚本都可能**静默算错而不报错**：

1. `refByOpcode` 的条目里除 `ref`/`wip` 外没有第三个字段（探测过程见 T0 report）。
2. 继承链只有两种写法：`refByOpcode.set('X', new Map([...refByOpcode.get('Y')!.entries(), 新增]))`
   （新增/覆盖）和 `refByOpcode.get('X')!.delete(N)`（删除，`anm.ts:471` 的 `'17'` 就靠它
   把 `165` 带来的 439 号删掉）。**2026-09-09 复核时发现脚本最初漏看了 `.delete()`**——
   之所以 th16 当时算对是运气：th16 所在的 `06→…→15→16` 这条链上恰好没有任何 `.delete()`，
   `.delete()` 出现在更下游的 `'17'`。现在两种写法都按**源码里出现的先后顺序**统一回放
   （`_STATEMENT_RE` 一次扫出 set/delete 两类语句，靠 `re.finditer` 保证顺序），
   delete 发生在哪个版本被 spread 出去*之前*就不影响该版本，这与 JS `Map` 的值语义一致。
   `main()` 每次跑都会拿 th16/th17/th18 做一次回归自检（见 `self_check()`）：
   `refByOpcode` 的定义决定了 th17 = th16（`'17'` = `'165'`(=`'16'`+439) 删掉 439，
   净效果等于没变过），th18 = th17 + 439(`fadeNearCamera`) + 614(`drawArc`) = 137 条。
   这条判据比"行数还是不是 135"更强——上次的坑正是"行数凑巧还是 135，但集合已经错了"。

若 thpages 上游又加了新的语句形态（比如 `.set()` 单条插入、多层继承分叉），
`_STATEMENT_RE` 不认识的语句会被跳过且不报错——自检失败是唯一的报警信号，
**跑完这个脚本一定要看 stderr 里的自检结果，不能只看退出码。**

产物：`runtime/anm/OVERVIEW.md` 里 `<!-- gen-opcode-matrix:begin -->` 到
`<!-- gen-opcode-matrix:end -->` 之间的表格内容（135 行，初始状态全部「未开始」）。
"""
import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
ANM_TS = REPO / "local/vendor/thpages/js/tables/reference/anm.ts"
OVERVIEW = REPO / "runtime/anm/OVERVIEW.md"

GAME = "16"  # th16 —— 阶段 1 主攻版本，见 brief Global Constraints

BEGIN_MARK = "<!-- gen-opcode-matrix:begin -->"
END_MARK = "<!-- gen-opcode-matrix:end -->"

# 与 anm.ts 里的 ANM_GROUPS_V8 保持一致（找不到就在这改，别指望自动同步）
GROUPS = [
    (0, 99, "System"),
    (100, 199, "Math"),
    (200, 299, "Jumps"),
    (300, 399, "Settings"),
    (400, 499, "General"),
    (500, 599, "Child management"),
    (600, 699, "Drawing"),
]

# 两种会改变 refByOpcode 的语句，按源码里出现的先后顺序统一扫描（顺序错了结果就错，
# 见模块 docstring）。SET 捕获 (版本, Map 字面量体)；DELETE 捕获 (版本, 被删的 opcode)。
_STATEMENT_RE = re.compile(
    r"refByOpcode\.set\('([^']+)',\s*new Map\(\[(.*?)\]\)\);"
    r"|refByOpcode\.get\('([^']+)'\)!\.delete\((\d+)\);",
    re.S,
)
_ENTRY_RE = re.compile(r"\[(\d+),\s*\{ref:\s*'anm:([^']+)'(?:,\s*wip:\s*(\d+))?\s*\}\]")
_BASE_RE = re.compile(r"refByOpcode\.get\('([^']+)'\)!\.entries\(\)")

Entry = tuple[str, "int | None"]          # (助记名不带 'anm:' 前缀, 该条目自带的 wip 或 None)
OpMap = dict[int, Entry]


def group_of(opcode: int) -> str:
    for lo, hi, title in GROUPS:
        if lo <= opcode <= hi:
            return title
    return "?"


def parse_all_ref_by_opcode(text: str) -> dict[str, OpMap]:
    """按源码顺序回放整条 `refByOpcode` 继承链（含 `.delete()`），返回**所有**游戏键的表。
    调用方按需取某个版本；`self_check()` 需要同时看多个版本，所以这里不只返回 GAME 那张。
    """
    maps: dict[str, OpMap] = {}
    for m in _STATEMENT_RE.finditer(text):
        set_ver, body, del_ver, del_opc = m.groups()
        if set_ver is not None:
            base_m = _BASE_RE.search(body)
            merged: OpMap = dict(maps[base_m.group(1)]) if base_m else {}
            for opc, mnemonic, wip in _ENTRY_RE.findall(body):
                merged[int(opc)] = (mnemonic, int(wip) if wip else None)
            maps[set_ver] = merged
        else:
            opc = int(del_opc)
            if del_ver not in maps:
                raise SystemExit(f"'{del_ver}'!.delete({opc}) 但该版本还没被 set 过——语句顺序有问题？")
            if opc not in maps[del_ver]:
                raise SystemExit(
                    f"'{del_ver}'!.delete({opc}) 但表里已经没有这个 opcode——"
                    "thpages 变了，本脚本的语句识别跟不上了，先别信结果"
                )
            del maps[del_ver][opc]
    return maps


def parse_by_ref_id_wip(text: str) -> dict[str, int]:
    """扫 `byRefId` 里字面量 key 的定义块（`'mnemonic': { ... }`），抠出 `wip: N`。
    循环生成的助记名（jmpEq/set/add3 之类，用 `${mnemonic}` 拼 key）没有 wip 字段，
    天然不会出现在返回值里——这些助记名不需要 wip 信息。
    """
    start = text.index("export const byRefId")
    body = text[start:]
    key_re = re.compile(r"\n\s*'([a-zA-Z0-9_.-]+)':\s*\{")
    matches = list(key_re.finditer(body))
    wip: dict[str, int] = {}
    for i, m in enumerate(matches):
        end = matches[i + 1].start() if i + 1 < len(matches) else len(body)
        chunk = body[m.end():end]
        wip_m = re.search(r"\bwip:\s*(\d+)", chunk)
        if wip_m:
            wip[m.group(1)] = int(wip_m.group(1))
    return wip


def self_check(maps: dict[str, OpMap]) -> list[str]:
    """回归判据，专治"行数凑巧对但集合已经错了"这种静默坏法：

    - th17 应当**恰好等于** th16（`'17'` = `'165'`(=`'16'`+439) 删掉 439，净效果不变）。
    - th18 应当是 th17 + 439(`fadeNearCamera`) + 614(`drawArc`) = 137 条。

    返回空列表 = 通过；否则每条字符串是一个具体的错误。调用方应当当场 SystemExit，
    不能只打日志了事——这正是这次要修的"静默出错"。
    """
    problems = []
    for ver in ("16", "17", "18"):
        if ver not in maps:
            problems.append(f"anm.ts 里没有 refByOpcode.set('{ver}', ...) —— 游戏键变了？")
    if problems:
        return problems

    th16, th17, th18 = maps["16"], maps["17"], maps["18"]

    if len(th16) != 135:
        problems.append(f"th16 应为 135 条，实得 {len(th16)}")
    if th17 != th16:
        only16 = sorted(set(th16) - set(th17))
        only17 = sorted(set(th17) - set(th16))
        problems.append(
            f"th17 应与 th16 完全相同（135 条），实得 {len(th17)} 条；"
            f"th16 独有 {only16}，th17 独有 {only17}"
        )
    if len(th18) != 137:
        problems.append(f"th18 应为 137 条，实得 {len(th18)}")
    else:
        added = sorted(set(th18) - set(th17))
        if added != [439, 614]:
            problems.append(f"th18 相对 th17 新增的 opcode 应为 [439, 614]，实得 {added}")
        elif th18[439][0] != "fadeNearCamera" or th18[614][0] != "drawArc":
            problems.append(
                f"th18 的 439/614 助记名应为 fadeNearCamera/drawArc，"
                f"实得 {th18[439][0]}/{th18[614][0]}"
            )
    return problems


def build_rows(opmap: OpMap, wip_by_name: dict[str, int]) -> list[str]:
    rows = []
    for opcode in sorted(opmap):
        mnemonic, entry_wip = opmap[opcode]
        wip = wip_by_name.get(mnemonic, entry_wip)
        confidence = "🟡（社区 wip:2，需过额外闸门）" if wip == 2 else "—"
        rows.append(
            f"| {opcode} | {group_of(opcode)} | `{mnemonic}` | 未开始 | 未开始 | {confidence} |"
        )
    return rows


def render_table(rows: list[str]) -> str:
    header = (
        "| opcode | 组 | thpages 名 | 文档 | 实现 | 可信度 |\n"
        "| ---: | --- | --- | --- | --- | --- |\n"
    )
    return header + "\n".join(rows) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true", help="只解析+自检，不写文件")
    args = ap.parse_args()

    if not ANM_TS.is_file():
        raise SystemExit(
            f"找不到 {ANM_TS.relative_to(REPO)}——先 clone：\n"
            "  git clone https://github.com/ExpHP/thpages local/vendor/thpages"
        )
    text = ANM_TS.read_text(encoding="utf-8")
    maps = parse_all_ref_by_opcode(text)

    problems = self_check(maps)
    for ver in ("16", "17", "18"):
        if ver in maps:
            print(f"[gen-opcode-matrix] th{ver} 解出 {len(maps[ver])} 条 opcode", file=sys.stderr)
    if problems:
        for p in problems:
            print(f"✗ [self-check] {p}", file=sys.stderr)
        raise SystemExit("自校验失败——上面的 opcode 表不可信，先别用，见脚本头部注释")
    print("[gen-opcode-matrix] 自校验通过：th17 == th16（135 条），th18 == th17 + 439/614（137 条）",
          file=sys.stderr)

    if GAME not in maps:
        raise SystemExit(f"anm.ts 里没有 refByOpcode.set('{GAME}', ...) —— thpages 改了游戏键？")

    wip_by_name = parse_by_ref_id_wip(text)
    rows = build_rows(maps[GAME], wip_by_name)

    if args.check:
        return 0

    table = render_table(rows)
    doc = OVERVIEW.read_text(encoding="utf-8")
    if BEGIN_MARK not in doc or END_MARK not in doc:
        raise SystemExit(f"{OVERVIEW.relative_to(REPO)} 里没有 {BEGIN_MARK}/{END_MARK} 锚点")
    pre, rest = doc.split(BEGIN_MARK, 1)
    _, post = rest.split(END_MARK, 1)
    new_doc = f"{pre}{BEGIN_MARK}\n\n{table}\n{END_MARK}{post}"
    OVERVIEW.write_text(new_doc, encoding="utf-8")
    print(f"[gen-opcode-matrix] 已写回 {OVERVIEW.relative_to(REPO)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
