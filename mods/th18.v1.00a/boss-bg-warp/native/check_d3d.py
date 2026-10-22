#!/usr/bin/env python3
"""check_d3d.py —— 静态检查：所有 D3D 间接调用（vtable）必须满足
   ① 用 __stdcall（否则栈由调用方清 ⇒ 与被调方约定不符 ⇒ 崩溃）
   ② 强转里声明的参数个数 == 该槽位的真实 arity
   ③ 实参个数 == 同一个 arity
   ④ 槽位必须在下面的表里（未知槽位直接报错 ⇒ 逼你查清楚再写）

为什么有这个脚本（都是实际踩过的崩）：
   · GetRenderTarget 少传 RenderTargetIndex（少 1 个实参）
   · GetTextureStageState 少传一个参数
   · 把 GetSurfaceLevel 当成 vtable[13]（那是 GetLevelCount()，不收参数）却按 2 参调用
   · 强转写成 cdecl（漏 __stdcall）⇒ 栈不平衡 ⇒ 返回地址被踩（Eip 落在栈上）

用法：python3 check_d3d.py [源文件...]     退出码 0 = 通过
"""
import re
import sys

# 槽位 -> (名字, arity 含 this)
DEVICE = {
    0x03: ("TestCooperativeLevel", 1),
    0x17: ("CreateTexture", 9),
    0x26: ("GetRenderTarget", 3),
    0x30: ("GetViewport", 2),
    0x39: ("SetRenderState", 3),
    0x3A: ("GetRenderState", 3),
    0x41: ("SetTexture", 3),
    0x42: ("GetTextureStageState", 4),
    0x43: ("SetTextureStageState", 4),
    0x53: ("DrawPrimitiveUP", 5),
    0x59: ("SetFVF", 2),
}
SURFACE = {
    0x02: ("Release", 1),
    0x0B: ("GetContainer", 3),
    0x0C: ("GetDesc", 2),
}
TEXTURE = {
    0x02: ("Release", 1),
    0x0D: ("GetLevelCount", 1),
    0x0E: ("GetLevelDesc", 2),
    0x0F: ("GetSurfaceLevel", 3),
}
IFACES = {"vt": ("device", DEVICE), "svt": ("surface", SURFACE), "tvt": ("texture", TEXTURE)}


def balanced(s, i, open_ch="(", close_ch=")"):
    """s[i] == open_ch；返回 (内容, 右括号后一位)。"""
    depth = 0
    for k in range(i, len(s)):
        if s[k] == open_ch:
            depth += 1
        elif s[k] == close_ch:
            depth -= 1
            if depth == 0:
                return s[i + 1:k], k + 1
    return None, None


def count_args(txt):
    txt = txt.strip()
    if not txt:
        return 0
    depth = 0
    n = 1
    for ch in txt:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "," and depth == 0:
            n += 1
    return n


def count_types(txt):
    """强转里的形参列表：'void *, unsigned, void **' -> 3"""
    txt = txt.strip()
    if not txt:
        return 0
    parts = [p for p in re.split(r",(?![^(]*\))", txt)]
    return len([p for p in parts if p.strip()])


def find_cast_start(src, call_end):
    """call_end = 强转右括号 ')' 的位置之后（即 var[slot] 之前）。
    从那里向前找与它配对的 '((' 的起点；返回起点或 None。"""
    i = call_end - 1                      # 指向强转右括号
    depth = 0
    while i >= 0:
        if src[i] == ")":
            depth += 1
        elif src[i] == "(":
            depth -= 1
            if depth == 0:
                # 期望再前一个是 '('（形成 '(('）
                if i >= 1 and src[i - 1] == "(":
                    return i - 1
                return None
        i -= 1
    return None


def check(path):
    src = open(path, encoding="utf-8", errors="replace").read()
    bad = []
    total = 0
    for m in re.finditer(r"\)\s*(vt|svt|tvt)\[([0-9a-fA-Fx]+)\]\s*\)\s*\(", src):
        var, slot_s = m.group(1), m.group(2)
        call_end = m.start() + 1          # ')' 之后（含）
        try:
            slot = int(slot_s, 0)
        except ValueError:
            continue
        line = src[:m.start()].count("\n") + 1
        iface, table = IFACES[var]
        if slot not in table:
            bad.append((line, "未知槽位 %s[%#x]（%s）—— 先查清 arity 再加进表" % (var, slot, iface)))
            continue
        name, arity = table[slot]

        k = m.end() - 1                   # 指向 args 的左括号
        args, _ = balanced(src, k)
        if args is None:
            bad.append((line, "调用括号不配对"))
            continue
        n_args = count_args(args)

        cs = find_cast_start(src, call_end)
        cast = src[cs + 2:call_end - 1] if cs is not None else None
        if cast is None:
            bad.append((line, "找不到强转（%s[%#x]）" % (var, slot)))
            continue
        total += 1
        if "__stdcall" not in cast:
            bad.append((line, "%s：强转缺 __stdcall（cdecl ⇒ 栈不平衡）" % name))
        q = cast.find("*)")
        n_types = -1
        if q >= 0:
            r = cast.find("(", q + 2)
            if r >= 0:
                params, _ = balanced(cast, r)
                if params is not None:
                    n_types = count_types(params)
        if n_types < 0:
            bad.append((line, "%s：强转形状不认识：%s" % (name, cast[:60])))
        elif n_types != arity:
            bad.append((line, "%s：强转声明 %d 个参数，真实 arity=%d" % (name, n_types, arity)))
        if n_args != arity:
            bad.append((line, "%s：实参 %d 个，真实 arity=%d" % (name, n_args, arity)))
    return total, bad


def main():
    files = sys.argv[1:] or ["bgwarp_core.c", "route_b.c", "th18_bgwarp.c"]
    total = 0
    bad_all = []
    for f in files:
        try:
            total_f, bad = check(f)
        except FileNotFoundError:
            continue
        total += total_f
        bad_all += [(f, l, msg) for l, msg in bad]
    print("D3D 间接调用检查：%d 处调用点" % total)
    if bad_all:
        for f, l, msg in bad_all:
            print("  ✗ %s:%d  %s" % (f, l, msg))
        print("✗ 共 %d 处问题" % len(bad_all))
        sys.exit(1)
    print("✓ 全部通过（__stdcall / 声明 arity / 实参个数 / 槽位已知）")


main()
