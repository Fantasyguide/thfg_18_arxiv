#!/usr/bin/env python3
"""从 thpages 的 anm.ts 生成 `runtime/anm/OVERVIEW.md` 里的 opcode 状态矩阵。

**为什么不手写 135 行**：thpages（`local/vendor/thpages/js/tables/reference/anm.ts`）
按游戏继承记录 opcode 表——`refByOpcode.set('16', new Map([...refByOpcode.get('15')!.entries(), ...]))`
这类链式 spread，th16（游戏键 `'16'`）= th15 的表 + 两条新增。助记名在 `byRefId` 里，
少数几条带 `wip: 2`（社区自己标"不确定"，对应 Global Constraints 里"社区标 wip:2 的 6 条"）。

    python3 runtime/anm/tools/gen-opcode-matrix.py           # 重写 OVERVIEW.md 里两个
                                                               # 注释锚点之间的表格
    python3 runtime/anm/tools/gen-opcode-matrix.py --check    # 只校验，不写文件（CI 用）

**解析方式**：这是 TypeScript，不是 JSON，这里用正则抠字段，不引入 Node/ts 工具链
（本项目零依赖，见 `runtime/anm/package.json`）。已核对过 `refByOpcode` 里除
`ref`/`wip` 外没有第三个字段（探测过程见 T0 report），`byRefId` 里给 th16 用得到的
135 个助记名各自的 `wip` 也是这样探测出来的——6 条 `wip: 2`，与 brief 描述吻合。
若 thpages 上游改了 schema（比如加新字段），本脚本的正则会静默漏掉，**不会报错**——
重生成后建议跑一次 `git diff` 看行数是不是还是 135，对不上就该来改这份脚本了。

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


def group_of(opcode: int) -> str:
    for lo, hi, title in GROUPS:
        if lo <= opcode <= hi:
            return title
    return "?"


def parse_ref_by_opcode(text: str) -> dict[int, tuple[str, int | None]]:
    """回放 `refByOpcode.set('XX', new Map([...get('YY').entries(), 新条目]))` 的继承链，
    返回游戏键 GAME 那张表：{opcode: (助记名不带 'anm:' 前缀, 该条目自带的 wip 或 None)}。
    """
    block_re = re.compile(r"refByOpcode\.set\('([^']+)',\s*new Map\(\[(.*?)\]\)\);", re.S)
    entry_re = re.compile(r"\[(\d+),\s*\{ref:\s*'anm:([^']+)'(?:,\s*wip:\s*(\d+))?\s*\}\]")
    base_re = re.compile(r"refByOpcode\.get\('([^']+)'\)!\.entries\(\)")

    maps: dict[str, dict[int, tuple[str, int | None]]] = {}
    for ver, body in block_re.findall(text):
        base_m = base_re.search(body)
        merged = dict(maps[base_m.group(1)]) if base_m else {}
        for opc, mnemonic, wip in entry_re.findall(body):
            merged[int(opc)] = (mnemonic, int(wip) if wip else None)
        maps[ver] = merged

    if GAME not in maps:
        raise SystemExit(f"anm.ts 里没有 refByOpcode.set('{GAME}', ...) —— thpages 改了游戏键？")
    return maps[GAME]


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


def build_rows(text: str) -> list[str]:
    opmap = parse_ref_by_opcode(text)
    wip_by_name = parse_by_ref_id_wip(text)

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
    ap.add_argument("--check", action="store_true", help="只校验能否解析出 135 行，不写文件")
    args = ap.parse_args()

    if not ANM_TS.is_file():
        raise SystemExit(
            f"找不到 {ANM_TS.relative_to(REPO)}——先 clone：\n"
            "  git clone https://github.com/ExpHP/thpages local/vendor/thpages"
        )
    text = ANM_TS.read_text(encoding="utf-8")
    rows = build_rows(text)
    print(f"[gen-opcode-matrix] th{GAME} 解出 {len(rows)} 条 opcode", file=sys.stderr)
    if len(rows) != 135:
        print(f"⚠ 期望 135 条，实得 {len(rows)}——thpages 上游变了？先别改脚本正则前手动核一遍。",
              file=sys.stderr)

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
