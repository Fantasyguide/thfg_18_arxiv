#!/usr/bin/env python3
"""engine_abi_check.py —— 引擎间接调用的**调用约定对账**（`make check` 的一环，只读 exe）。

    python3 native/tools/engine_abi_check.py          # 全表对账（无样本/无 capstone 时跳过并 exit 0）
    python3 native/tools/engine_abi_check.py --all    # 连"没被任何调用点用到"的 typedef 也列出来

## 为什么要有它（真事故）

2026-09-14「进灵界几秒后闪退」的**真因**：`0x48a730`（补 buffer）的收尾是 `ret 4`
（它吃一个栈上参数，函数体一次都没读），而我们按 `fastcall(void*)`（0 个栈参）调它
⇒ 每调一次少吃 4 字节栈 ⇒ 后面某次 `ret` 跳进垃圾/0（转储 `120072` EIP = `0x3EC7AE14`、
转储 `59844` EIP = `0`）。补 buffer 每 ~2 秒来一次 ⇒ 表现成"几秒后闪退"。
两次转储的形状（在野地址取指）都指到了那个**症状**，真因却是**声明与实际不符**——
这种错静态上看不出来、跑起来也不报错，只在某个时机炸。

⇒ 这道闸把「**我们声明的调用约定**」与「**真 exe 里那个函数的 `ret N`**」对上：
我们的 typedef 决定了 caller 压多少栈参，callee 的 `ret N` 决定了它弹多少
——**两者必须相等**，否则栈就会一天天歪下去（歪到哪一帧炸是运气问题）。

## 判据（`ret N` ↔ 声明）

| 我们的声明 | caller 压的栈字节 | 期望 callee `ret N` |
| --- | --- | --- |
| `thiscall(a, b, c…)` | `4 × (参数数 − 1)`（第一个参数在 `ecx`）| 同左 |
| `fastcall(a, b, c…)`（MSVC 约定：ecx/edx）| `4 × max(参数数 − 2, 0)` | 同左 |
| `cdecl` | caller 自己清（`ret`，N = 0）| 0 |
| `stdcall` | `4 × 参数数` | 同左 |

## 怎么找「我们调了谁」

扫 `native/**/*.{c,h}` 里 `((某个_typedef)(uintptr_t)某个_VA宏)(…)` 这个形状
（本仓所有引擎间接调用都照这个写法），把 typedef 名 → 参数数（从同文件的 `typedef` 声明里数）
与 VA 宏 → 地址（从 `#define` 里读 `0x…`）配起来。
`--all` 会把「声明了 typedef 但没在任何调用点用到」的条目单列（不算失败）。

## 不判红的情形

- 没样本（`local/<版本>/th18.exe`，gitignored）或没装 `capstone` ⇒ 打印一行并 **exit 0**；
- 调用点写成了别的新形状（扫不到）⇒ 只打印提示，不判红 —— 但 `--all` 会把它列出来让人过目；
- 一个地址在多个 typedef 下被调用（`union` 式用法）⇒ 逐条判，任一条不符就红。
"""
import argparse
import hashlib
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent            # native/tools
NATIVE = HERE.parent                              # native/
MOD = NATIVE.parent                               # card-expand/
REPO = MOD.parents[2]                             # renkolab/
VERSION = "th18.v1.00a"
EXE = REPO / "local" / VERSION / "th18.exe"
SITES_PY = NATIVE / "sites.py"

SRC_GLOBS = ("*.c", "*.h", "cards/*.c", "cards/*.h", "sdk/*.h")

# `((tbgm_fn_refill_t)(uintptr_t)TBGM_FN_REFILL)(args…)` —— 本仓引擎间接调用的标准写法
CALL_RE = re.compile(r"\(\(\s*(\w+)\s*\)\s*\(uintptr_t\)\s*(\w+)\s*\)\s*\(")
# `typedef int (__attribute__((thiscall)) *name_t)(void *s, int unused);`
TYPEDEF_RE = re.compile(
    r"typedef\s+[\w \*]+\(\s*__attribute__\(\(\s*(?P<conv>\w+)\s*\)\)\s*\*\s*(?P<name>\w+)\s*\)"
    r"\s*\((?P<args>[^;]*)\)\s*;", re.S)
DEFINE_RE = re.compile(r"^\s*#define\s+(?P<name>\w+)\s+(?P<val>0[xX][0-9a-fA-F]+)")


def read(p: Path) -> str:
    try:
        return p.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def arg_count(args: str) -> int:
    """参数个数（`void` / 空 = 0；不看类型只看逗号，够用——本仓没有函数指针参数）。"""
    args = args.strip()
    if not args or args == "void":
        return 0
    depth, n, i = 0, 1, 0
    while i < len(args):
        c = args[i]
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        elif c == "," and depth == 0:
            n += 1
        i += 1
    return n


def expected_ret(conv: str, argc: int):
    """→ (期望的 `ret N`，人类可读的算式)；None = 这个约定不由 callee 清栈（cdecl）。"""
    conv = conv.lower()
    if conv == "thiscall":
        return 4 * max(argc - 1, 0), f"thiscall, {argc} 参 ⇒ {4 * max(argc - 1, 0)} B"
    if conv == "fastcall":
        return 4 * max(argc - 2, 0), f"fastcall, {argc} 参 ⇒ {4 * max(argc - 2, 0)} B"
    if conv == "stdcall":
        return 4 * argc, f"stdcall, {argc} 参 ⇒ {4 * argc} B"
    if conv == "cdecl":
        return None, f"cdecl, {argc} 参（caller 清栈）⇒ 期望 ret"
    return None, f"不认识约定 {conv}"


def collect():
    """→ (calls: {(typedef, macro): [调用点...]}, typedefs: {name: (conv, argc)}, defines: {macro: va})"""
    calls, typedefs, defines = {}, {}, {}
    for pat in SRC_GLOBS:
        for f in NATIVE.glob(pat):
            txt = read(f)
            rel = f.relative_to(MOD)
            for m in TYPEDEF_RE.finditer(txt):
                typedefs[m.group("name")] = (m.group("conv"), arg_count(m.group("args")), str(rel))
            for line in txt.splitlines():
                m = DEFINE_RE.match(line)
                if m:
                    defines.setdefault(m.group("name"), int(m.group("val"), 0))
            for m in CALL_RE.finditer(txt):
                key = (m.group(1), m.group(2))
                calls.setdefault(key, []).append(str(rel))
    return calls, typedefs, defines


def pe_text(path: Path):
    import struct
    d = path.read_bytes()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    if d[pe:pe + 4] != b"PE\0\0":
        raise SystemExit(f"{path} 不是 PE")
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    opt = pe + 24
    base = struct.unpack_from("<I", d, opt + 28)[0]
    secoff = opt + struct.unpack_from("<H", d, pe + 20)[0]
    for i in range(nsec):
        o = secoff + 40 * i
        name = d[o:o + 8].rstrip(b"\0").decode(errors="replace")
        vsz, va, rsz, ra = struct.unpack_from("<IIII", d, o + 8)
        if name == ".text":
            return d[ra:ra + rsz], base + va
    raise SystemExit("找不到 .text")


def first_ret_near(code, tva, va, md, limit=0x800):
    """→ (ret 指令地址, N)：从 va 起线性反汇编，取**第一条 `ret`**（本仓这几个函数的收尾都在开头附近）。

    遇到 `int3` 填充 / 明显跑飞（`add [eax],al` 这种 `00 00` 噪声）就停 —— 说明函数体扫过头了。
    """
    off = va - tva
    if off < 0 or off >= len(code):
        return None, None
    for ins in md.disasm(code[off:off + limit], va):
        if ins.mnemonic in ("ret", "retn"):
            return ins.address, int(ins.op_str, 0) if ins.op_str else 0
        if ins.mnemonic == "int3":
            return None, None
        if ins.address - va > limit - 16:
            return None, None
    return None, None


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--all", action="store_true", help="连没被调用点用到的 typedef 也列出来")
    a = ap.parse_args()

    calls, typedefs, defines = collect()
    if not calls:
        print("engine_abi_check：源码里没有「((typedef)(uintptr_t)宏)(…)」形状的引擎调用 —— 跳过")
        return 0

    if not EXE.is_file():
        print(f"engine_abi_check：跳过（没有样本 {EXE.relative_to(REPO)} —— gitignored，"
              f"`python3 tooling/thtk/unpack.py {VERSION}` 之后才有）；"
              f"本次共 {len(calls)} 条引擎调用未核对")
        return 0
    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    except ImportError:
        print("engine_abi_check：跳过（没装 capstone；`pip install capstone` 之后才有这份对账）")
        return 0

    want = re.search(r'EXE_MD5\s*=\s*"([0-9a-f]{32})"', read(SITES_PY))
    got = hashlib.md5(EXE.read_bytes()).hexdigest()
    if want and got != want.group(1):
        print(f"engine_abi_check：跳过（exe md5 {got} ≠ sites.py 钉的 {want.group(1)}）")
        return 0

    code, tva = pe_text(EXE)
    md = Cs(CS_ARCH_X86, CS_MODE_32)

    bad, unknown, checked, unused = [], [], 0, set(typedefs)
    print(f"engine_abi_check —— 引擎间接调用的调用约定对账（样本 {EXE.relative_to(REPO)}）\n")
    print(f"{'地址':<12}{'typedef':<28}{'我们的声明':<34}{'真 exe 的收尾':<18}判定")
    print("-" * 110)
    for (tname, macro), where in sorted(calls.items(), key=lambda kv: (kv[0][0], kv[0][1])):
        unused.discard(tname)
        va = defines.get(macro)
        td = typedefs.get(tname)
        if td is None:
            unknown.append(f"{tname}（{where[0]}）：没有找到 typedef 声明，扫不到参数数")
            continue
        if va is None:
            unknown.append(f"{macro}（{where[0]}）：没有找到 `#define {macro} 0x…`")
            continue
        conv, argc, decl_file = td
        exp, human = expected_ret(conv, argc)
        raddr, rn = first_ret_near(code, tva, va, md)
        if raddr is None:
            unknown.append(f"{macro} @ {va:#x}（{where[0]}）：在该地址起没扫到 `ret`（函数头地址对吗？）")
            continue
        checked += 1
        if exp is None:
            verdict = "⚠ 不判（cdecl / 未知约定）"
            got_s = f"{raddr:#x} ret {rn or ''}".strip()
        else:
            got_s = f"{raddr:#x} ret {rn:#x}" if rn else f"{raddr:#x} ret"
            verdict = "✅" if rn == exp else f"❌ **不吃不匹配**（应 ret {exp:#x}）"
            if rn != exp:
                bad.append(f"{macro} @ {va:#x}（{where[0]}）：我们声明 {human}，"
                           f"真 exe 是 `ret {rn:#x}` ⇒ 每调一次栈歪 {(rn - exp):+#x} 字节")
        print(f"{va:<#12x}{tname:<28}{human:<34}{got_s:<18}{verdict}")

    if a.all:
        for name in sorted(unused):
            conv, argc, decl_file = typedefs[name]
            print(f"（未在调用点用到的 typedef：{name}  {conv}，{argc} 参，声明于 {decl_file}）")

    print()
    for u in unknown:
        print(f"⚠ {u}")
    if bad:
        print("\nengine_abi_check：**红**")
        for b in bad:
            print(f"  · {b}")
        print("  ⇒ 栈每天歪一点，歪到哪一帧炸是运气（见 AUDIT.md §CR-5）。改声明或改调用点，别改期望。")
        return 1
    print(f"engine_abi_check：对账通过 ✅（{checked} 条引擎调用；{len(unknown)} 条扫不到、未判）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
