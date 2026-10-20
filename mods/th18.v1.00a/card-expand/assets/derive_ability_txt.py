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

★★ **只改 `ability.txt` 不够 —— 会被 `base_tsa` 的 gentext 再盖掉**（2026-10-19 定位，owner 实测症状：
空白卡显示的是「原生文本的中文版」✗）：
  · `base_tsa/th18.js` 的 `breakpoints` 里有两个钩子，`"file": "abilities.js"`：
      `gentext#card_name`（`addr: Rx1669c`，`line: 0`）        — 卡名，取 `abilities.js["<id>"]`（字符串）
      `gentext#card_desc`（`addr: Rx167b0`，`line: "6 - [esp + 0x18]"`，`ids: ["[ebp + 0xC]", 0]`）
                                                              — 说明 6 行，取 `abilities.js["<id>_0"]`（数组）
  · 一手机制（反汇编）：`0x4167a0` 的循环把 `zAbilityText` 的 **第 1..6 行**（`edi = base + id*0x1c0 + 0x40`，
    每次 `+0x40`，共 6 次）逐行送渲染；钩子取 `line = 6 - [esp+0x18]`（`[esp+0x18]` 就是 `0x416783` 存的
    行计数 6→1）⇒ **`abilities.js` 数组下标 0..5 ↔ 说明第 1..6 行**（卡名是第 0 行，走 `gentext#card_name`）。
  · `lang_zh-hans` 自带 `th18/abilities.js`（`"0_0"` = 中文版原生文本），而 thcrap 的 JSON 文件
    **按补丁栈逐层合并**（同 `th18/cards.js`：我们的那份只有 49 个新 id，靠合并才进表）⇒
    **它在运行时把从 `ability.txt` 解析出来的 6 行说明整段换掉**，所以光盖 `ability.txt` 永远不生效 ✗。
  · 实测日志（`thcrap/logs/thcrap_log.txt`，2026-10-19 03:27 那次）两行并存：
      `(JSON) Resolving th18/abilities.js...  + .../thpatch/lang_zh-hans/th18/abilities.js`
      `(Data) Resolving th18/ability.txt...  + .../Renko_1055/th18_card_expand_255/th18/ability.txt`
  ⇒ 正解：**两个文件一起出** —— `ability.txt` 管「没有被 gentext 覆盖的行/兜底」，
    `patch/th18/abilities.js` 里的 `"0_0"` 才是**真正赢的那份**（我们排在 `lang_zh-hans` 之后 ⇒ 后合并者胜）。
    本脚本一次生成两者，避免漂移。

★ owner 口径：2026-10-03「原版卡的『空白卡片』的描述改为：『**删牌大于一切啊噶人闷**』」。
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                          # mods/th18.v1.00a/card-expand
_REPO = os.path.dirname(os.path.dirname(os.path.dirname(ROOT)))   # card-expand → th18.v1.00a → mods → 仓根
SRC = os.path.join(_REPO, "local", "th18.v1.00a", "dat", "ability.txt")
OUT = os.path.join(ROOT, "patch", "th18", "ability.txt")
OUT_ABI = os.path.join(ROOT, "patch", "th18", "abilities.js")

SECTION = b"@BLANK"          # 空白卡（注册表 `zTableCardData[]` 的 id 0 = BLANK）
NEW_DESC = "删牌大于一切啊噶人闷"      # owner 指定（UTF-8）
# `gentext#card_desc` 的数组下标 0..5 = 说明第 1..6 行（见头注的反汇编）⇒ 补满 6 项，
# 后面 5 项留空 = 空白卡只剩这一行说明。
DESC_LINES = [NEW_DESC] + [""] * 5


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

    # ★ gentext 真正消费的那份（见头注）：thcrap 把栈里所有 patch 的 th18/abilities.js 按补丁顺序合并，
    #   同一 key 后写者胜 ⇒ 我们这份排在 lang_zh-hans 之后，`"0_0"` 就会盖掉它的中文原生文本。
    import json
    abi = json.dumps({"0_0": DESC_LINES}, ensure_ascii=False, indent=2) + "\n"
    open(OUT_ABI, "w", encoding="utf-8", newline="\n").write(abi)
    print(f"abilities.js: {OUT_ABI}（JSON 合并后 `0_0` = 说明第 1..6 行，第 1 行 = {NEW_DESC!r}）")
    return 0



if __name__ == "__main__":
    sys.exit(main())
