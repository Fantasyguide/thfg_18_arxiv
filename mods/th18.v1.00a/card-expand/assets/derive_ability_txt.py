#!/usr/bin/env python3
"""把**原版卡**（id < 57）的文案改掉 —— 目前只有一张：**空白卡（id 0 = `BLANK`）**。

    python3 assets/derive_ability_txt.py

产物：`patch/th18/ability.txt`（**不入库**，构建期现取；见 `.gitignore`），随 `make dist` 进
`patch-step3/th18/ability.txt`，在 thcrap 栈里**盖住**游戏自己的那份 ⇒ 引擎启动时
`AbilityText__parse_ability_txt`（`th16`… 本作 `0x4160b0`）按 `@NAME` 往 `zAbilityText + id*0x1c0` 写的那 7 行
就变成我们这份。

★ 为什么走**文件**而不是 `cards.js`：`native/text.c` 的文案重定向只覆盖 **id ≥ 57**（`zAbilityText` 只有
57 张的容量，58 起才在对象之外）；原版 0..56 的文案在引擎对象里，`ce_text_set()` 对它们直接
`out of range — ignored`。而 `cards.js` 那条路会把**整行**（价格/权重/两张 sprite…）重新编码，
改文案却要冒写坏原版卡行的风险 ⇒ 正解是改文案**源头**。

★ 编码：原文件是 **Shift-JIS（cp932）**，而简体字（「删」等）**写不进 cp932** ⇒ 我们这一行写成
**UTF-8**。这样混是安全的：thcrap 的 textdisp **逐字符串**先按 UTF-8 解、失败才退 cp932
（见 `native/text.c` 的头注），而引擎把 7 行当 7 个字符串分别渲染 ⇒ 我们这一行按 UTF-8 走、
原版那些行照旧按 cp932 走，互不影响。其余字节**一个都不动**（做的是字节级外科手术）。

★ owner 口径：2026-10-03「原版卡的『空白卡片』的描述改为：『**删牌大于一切噶人闷**』」。
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                          # mods/th18.v1.00a/card-expand
_REPO = os.path.dirname(os.path.dirname(os.path.dirname(ROOT)))   # card-expand → th18.v1.00a → mods → 仓根
SRC = os.path.join(_REPO, "local", "th18.v1.00a", "dat", "ability.txt")
OUT = os.path.join(ROOT, "patch", "th18", "ability.txt")

SECTION = b"@BLANK"          # 空白卡（注册表 `zTableCardData[]` 的 id 0 = BLANK）
NEW_DESC = "删牌大于一切噶人闷"      # owner 指定（UTF-8）


def patch_section(data: bytes, key: bytes, new_lines: list) -> bytes:
    """把 `@<key>` 那一段的**说明行**换掉，段名行与结尾（`\\x0c`）保持原样。

    段结构（实测）：`\\x0c\\r\\n@KEY\\r\\n<名字行>\\r\\n\\r\\n<说明行…>\\r\\n\\r\\n…`，段尾是 `\\x0c`。"""
    i = data.find(b"\n" + key + b"\r\n")
    if i < 0:
        raise SystemExit(f"没找到段 {key!r} —— 源文件格式变了？")
    i += 1                                            # 指向 '@'
    name_end = data.find(b"\r\n", i)                  # @KEY 行末
    name_end = data.find(b"\r\n", name_end + 2)       # 名字行末
    if name_end < 0:
        raise SystemExit("段结构异常（没有名字行）")
    tail = data.find(b"\x0c", name_end)               # 段尾（下一个 \\x0c）
    if tail < 0:
        tail = len(data)
    body = b"".join(l.encode("utf-8") + b"\r\n" for l in new_lines)
    return data[:name_end + 2] + b"\r\n" + body + b"\r\n" + data[tail:]


def main() -> int:
    if not os.path.exists(SRC):
        print(f"FAIL: 源不在 {SRC}（游戏数据里的 ability.txt；见 local/README.md）", file=sys.stderr)
        return 2
    raw = open(SRC, "rb").read()
    out = patch_section(raw, SECTION, [NEW_DESC])
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, "wb").write(out)
    chk = out.decode("cp932", errors="replace")
    i = chk.find("@BLANK")
    seg = chk[i:i + 160].replace("\r\n", " | ")
    print(f"ability.txt: {OUT}（{len(raw)} → {len(out)} 字节；`@BLANK` 段改为 {NEW_DESC!r}）")
    print(f"  段内容 = {seg}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
