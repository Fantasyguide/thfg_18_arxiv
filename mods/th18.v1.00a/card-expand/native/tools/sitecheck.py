#!/usr/bin/env python3
"""sitecheck.py —— **新站点/新落点入库前的机器体检**（只读：读真 exe + 三份声明，不写任何文件）。

    # ① 加站点前：查窗口（长度 / 整指令边界 / 相对寻址 / 与既有 binhack·站点的重叠）
    python3 native/tools/sitecheck.py 0x45e930 5

    # ② 站点要「跳过原指令 / 改落点」时：再加落点，查**栈中性**（esp 与寄存器）
    python3 native/tools/sitecheck.py 0x45c048 7 --ret 0x45c056

    # ③ 「跳过整个函数」（落点 = 调用者返回地址）：报出该函数自己的 ret 与应有的 stack_clear_size
    python3 native/tools/sitecheck.py 0x45e930 5 --ret caller

    # ④ 回归：把**已声明的一切**（含 sdk.c 里自动扫出的所有落点）跑一遍（`make check` 里挂的就是它）
    python3 native/tools/sitecheck.py --all

## 为什么要有它

2026-09-18 那次「一按 Z 就崩」不是玩法 bug，是**站点/落点记账**出的错：
`stack_clear_size` 少写了 4 字节（桩的 `ret` 弹的槽比调用者压的返回地址低一格）⇒ 调用链的栈整体偏低 4
⇒ 上层 `pop ebx` 弹出 `3` ⇒ 派发器 `th18:0x4013ba` 读 `0x53` 违例。
而站点本身还有四条**成文规矩**（窗口 ≥5 字节、整指令边界、窗内无相对寻址、与任何 binhack/站点零重叠），
以前全靠人眼 + 单点脚本。这个工具把这五件事变成一次可复跑的输出 —— **加站点前先跑它**。

判定口径与一手指令表：`engine/_shared/thcrap-breakpoint-pools.md`（§2 桩语义 / §8 栈中性审计）。
机器对账的用法：新站点先跑 ①，要改落点再跑 ②/③；`make check` 跑 ④ 兜住既有的一批。

## 三条「栈中性」合法形态（工具按这三条判，其余一律 ⚠ 要人确认）

1. **落点是裸 `ret`**（cdecl）：跳过的序言与尾声一起被跳过 ⇒ 落点时 esp 仍指着调用者压的返回地址；
2. **落点会重设栈顶**（`mov esp,ebp` / `leave`，通常在函数尾声前）；
3. **被跳过的指令本身栈中性**（无 push/pop/sub esp，或它的 push 由被调方 `ret N` 清算）。

⚠ 本工具**只证明这三条**，不判断「落点处需要的寄存器是否被跳过了」——那部分会列出来供人看。
"""
import argparse
import json
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent            # native/tools
NATIVE = HERE.parent                              # native/
MOD = NATIVE.parent                               # mods/th18.v1.00a/card-expand
REPO = MOD.parents[2]                             # 仓库根

EXE = REPO / "local" / "th18.v1.00a" / "th18.exe"
PATCH = MOD / "patch" / "th18.v1.00a.js"
PATCH_TEST = MOD / "patch-test" / "th18.v1.00a.js"
MODKIT = Path("/mnt/d/项目/幻想指南/18/th18_modkit")
BASE_TSA = MODKIT / "thcrap" / "repos" / "nmlgc" / "base_tsa" / "th18.v1.00a.js"

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
except ImportError:                                # 只读体检，没有 capstone 就别硬跑
    Cs = None

IMG_BASE = 0x400000


# ── 真 exe ──────────────────────────────────────────────────────────────
class Exe:
    def __init__(self, path):
        d = self.d = path.read_bytes()
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        opt = pe + 24
        self.base = struct.unpack_from("<I", d, opt + 28)[0]
        secoff = opt + struct.unpack_from("<H", d, pe + 20)[0]
        self.secs = []
        for i in range(nsec):
            o = secoff + 40 * i
            name = d[o:o + 8].rstrip(b"\0").decode("latin1")
            vsz, va, rsz, ra = struct.unpack_from("<IIII", d, o + 8)
            self.secs.append((name, va, vsz, ra, rsz))

    def read(self, va, n):
        rva = va - self.base
        for _n, sva, vsz, ra, rsz in self.secs:
            if sva <= rva < sva + max(vsz, rsz):
                o = ra + (rva - sva)
                return self.d[o:o + n]
        return None

    def disasm(self, va, n):
        if Cs is None:
            return None
        blob = self.read(va, n)
        if blob is None:
            return None
        return list(Cs(CS_ARCH_X86, CS_MODE_32).disasm(blob, va))


# ── 三份声明 ────────────────────────────────────────────────────────────
def load_js(p):
    if not p.exists():
        return None
    s = p.read_text(encoding="utf-8")
    s = re.sub(r"^\s*//.*$", "", s, flags=re.M)
    s = re.sub(r",(\s*[}\]])", r"\1", s)
    try:
        return json.loads(s)
    except json.JSONDecodeError:
        return None


def norm_addr(a):
    if isinstance(a, int):
        return a
    if isinstance(a, str) and a.startswith("Rx"):
        return IMG_BASE + int(a[2:], 16)
    return int(a, 16)


def code_size(code):
    """binhack 的 `code` 字节数：占位引用（<...> / [...]）按 4 字节算（x86 地址）。"""
    n = len(re.findall(r"<[^>]*>|\[[^\]]*\]", code))
    hexs = re.sub(r"[^0-9a-fA-F]", "", re.sub(r"<[^>]*>|\[[^\]]*\]", "", code))
    return len(hexs) // 2 + 4 * n


SITES_PY_RE = re.compile(
    r'\(\s*"([A-Za-z0-9_]+)"\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(\d+)\s*,')


def parked_sites():
    """从 `native/sites.py` 的 `SDK_SITES` 表取**全部**站点（含被 `i<10` 截断 park 掉的）。

    为什么需要它：park = 没进 patch = **没装**，所以本身没风险；但一旦有人把它放回发布面，
    真正要用的就是它那条规则检查 ⇒ 现在就把它们一起查了，别等出事再查（2026-09-18 的教训）。"""
    out = {}
    src = (NATIVE / "sites.py").read_text(encoding="utf-8", errors="replace")
    for m in SITES_PY_RE.finditer(src):
        out.setdefault(m.group(1), (int(m.group(2), 16), int(m.group(3))))
    return out


def collect_items():
    """返回 (breakpoints, binhacks)：每项 = (来源, 名字, 地址, 字节数)。"""
    bps, bhs = [], []
    for src, p in (("ours", PATCH), ("test", PATCH_TEST), ("base_tsa", BASE_TSA)):
        doc = load_js(p)
        if not isinstance(doc, dict):
            continue
        for name, v in (doc.get("breakpoints") or {}).items():
            if not isinstance(v, dict) or "addr" not in v:
                continue
            addrs = v["addr"] if isinstance(v["addr"], list) else [v["addr"]]
            for a in addrs:
                try:
                    bps.append((src, name, norm_addr(a), int(v.get("cavesize") or 0)))
                except (TypeError, ValueError):
                    pass
        for name, v in (doc.get("binhacks") or {}).items():
            if not isinstance(v, dict) or "addr" not in v:
                continue
            sz = None
            if v.get("expected"):
                sz = len(re.sub(r"[^0-9a-fA-F]", "", v["expected"])) // 2
            elif v.get("code"):
                sz = code_size(v["code"])
            addrs = v["addr"] if isinstance(v["addr"], list) else [v["addr"]]
            for a in addrs:
                if sz:
                    try:
                        bhs.append((src, name, norm_addr(a), sz))
                    except (TypeError, ValueError):
                        pass
    return bps, bhs


# ── 单站点四项检查 ──────────────────────────────────────────────────────
def check_window(exe, va, cs, bps, bhs, self_name=None):
    ok = True
    print(f"站点 {va:#08x}（cavesize {cs}）")

    # W1 长度
    if cs < 5:
        print(f"  ✗ W1 长度：{cs} < 5 —— thcrap 的 sourcecave 下限是 5，短于它会被拒装"
              f"（随后 SDK 门回读失败 ⇒ 新 id 全停用）")
        ok = False
    else:
        print(f"  ✓ W1 长度：{cs} ≥ 5")

    # W2 整指令边界
    insns = exe.disasm(va, cs)
    if insns is None:
        print("  ⚠ W2 整指令边界：没装 capstone 或地址读不到，跳过")
    else:
        total = sum(i.size for i in insns)
        if total != cs:
            print(f"  ✗ W2 整指令边界：窗口里只覆盖 {total} 字节、最后一条指令被切断"
                  f"（cave 会回放半条指令）—— 改成 {total} 或再加长到 {cs} 之后的指令边界")
            ok = False
        else:
            print(f"  ✓ W2 整指令边界：{len(insns)} 条指令恰好 {cs} 字节")

    # W3 相对寻址
    if insns is not None:
        bad, note = [], []
        for i in insns:
            m, o = i.mnemonic, i.op_str
            rel32 = (m in ("call", "jmp") and o.startswith("0x")) or (m.startswith("j") and o.startswith("0x"))
            rel8 = not rel32 and (m == "jmp" or m.startswith("j")) and not o.startswith("0x")
            if rel32:
                bad.append(f"{m} {o}@+{i.address - va:#x}")
            elif rel8:
                note.append(f"{m} {o}@+{i.address - va:#x}")
        fixable = []
        if bad and insns:
            first = insns[0]
            if first.mnemonic in ("call", "jmp") and first.address == va and first.op_str.startswith("0x"):
                # 窗口第一条就是 `e8/e9 rel32` ⇒ thcrap 的 cave_fix 会把它重定位好（一手：它只认这两种）
                fixable = [bad[0]]
                bad = bad[1:]
                if not bad:
                    print(f"  ✓ W3 相对寻址：窗口首条是 `{fixable[0]}` = `e8/e9 rel32` ⇒ "
                          f"**cave_fix 会重定位**，安全（这是 thcrap 唯一会修的那种）")
        if bad:
            print(f"  ✗ W3 相对寻址：窗内有**近**相对分支 {bad} —— thcrap 的 cave_fix 只修"
                  f"「窗口第一条就是 e8/e9」那一种 ⇒ 这些进 cave 会**跳飞**（跳到 本格 + (原目标−站点)，"
                  f"通常正好落在池页没写过的那一段 ⇒ `00 00`、写 `0x1` 的访问违例，AUDIT §BE-DW）。换窗口。")
            ok = False
        elif note:
            print(f"  ✓ W3 相对寻址：只有短跳（rel8，自地址无关）{note} —— 进 cave 安全")
        else:
            print("  ✓ W3 相对寻址：窗内无分支")

    # W4 重叠
    hits = []
    for src, name, a, sz in bhs:
        if sz and max(va, a) < min(va + cs, a + sz):
            hits.append(f"binhack {src}:{name} @{a:#x}+{sz}")
    for src, name, a, sz in bps:
        if not sz:
            continue
        if a == va:                       # 同址：站点自己（或**同址重复声明** —— 后者要单说）
            if sz != cs or (self_name and name != self_name):
                hits.append(f"同址另有断点 {src}:{name} @{a:#x}+{sz}（重复声明/尺寸不同）")
            continue
        if max(va, a) < min(va + cs, a + sz):
            hits.append(f"断点 {src}:{name} @{a:#x}+{sz}")
    if hits:
        print(f"  ✗ W4 重叠：与 {len(hits)} 项相交 —— {hits[:6]}{' …' if len(hits) > 6 else ''}")
        ok = False
    else:
        print("  ✓ W4 重叠：与所有 binhack / 其它断点零重叠")
    return ok


# ── 落点（跳过点）的栈中性 ──────────────────────────────────────────────
def callee_ret_imm(exe, target, cache={}):
    """被调函数第一条 `ret` 的 imm16（0 = 裸 ret；None = 扫不到）。用来判「push 的栈参有没有被清」。"""
    if target in cache:
        return cache[target]
    imm = None
    for i in (exe.disasm(target, 0x200) or []):
        if i.mnemonic == "ret":
            imm = int(i.op_str, 16) if i.op_str else 0
            break
    cache[target] = imm
    return imm


def is_epilogue_ret(exe, target):
    """落点是不是「函数尾声那条 ret」（前面几条里有 `pop ebp`）——用来区分函数自己的 `ret` 与**内部调用**的 `ret N`。"""
    insns = exe.disasm(max(0, target - 0x20), 0x20) or []
    tail = [i for i in insns if i.address <= target]
    return any(i.mnemonic == "pop" and i.op_str == "ebp" for i in tail[-6:])


def is_function_entry(insns):
    """窗口是不是「函数序言」（`push ebp` / `mov ebp,esp` 那一套）⇒ 用于判「跳过整个函数」。"""
    if not insns:
        return False
    txt = [(i.mnemonic, i.op_str) for i in insns[:4]]
    return any(m == "push" and o == "ebp" for m, o in txt) or any(
        m == "mov" and o.replace(" ", "").startswith("ebp,esp") for m, o in txt)


def check_landing(exe, va, cs, target):
    print(f"落点 {target:#08x}：跳过 {va + cs:#x} .. {target:#x}")
    if target == va or (va < target < va + cs):
        print("  ✗ L0 落点在**站点自身/窗口内** ⇒ 站点前几字节已被改成 `call <桩>`，"
              "跳过去就是**重新进入桩**（无限重入）。换落点。")
        return False
    _ = exe.disasm(va, cs) or []
    insns = exe.disasm(va + cs, max(0, target - (va + cs)))
    land = exe.disasm(target, 0x20) or []
    if insns is None or not land:
        print("  ⚠ L1 栈中性：没装 capstone 或读不到字节，跳过")
        return False

    # ★ 口径：handler 返回 0 时**被跳过的指令不会执行** ⇒ 落点处 esp 就等于**站点处的 esp**（相对 0）。
    #   所以真正要问的是「落点（及其后续直线）是不是按站点那个 esp 活的」。
    resets = any((i.mnemonic == "mov" and i.op_str.replace(" ", "").startswith("esp,")) or i.mnemonic == "leave"
                 for i in land[:6])
    bare_ret = land[0].mnemonic == "ret"
    entry = is_function_entry(exe.disasm(va, cs) or [])

    # 跳过区的「真实净 esp」：push 的栈参若由被调方的 `ret N` 清掉，就不算不平衡。
    esp, pending, unproven, regs, notes = 0, 0, [], set(), []
    for i in insns:
        m, o = i.mnemonic, i.op_str
        if m == "push":
            esp -= 4
            pending += 4
            unproven.append(f"push@{i.address:#x}")
        elif m == "pop":
            esp += 4
            pending = max(0, pending - 4)
        elif m == "sub" and o.startswith("esp,"):
            esp -= int(o.split(",")[1].strip(), 16)
        elif m == "add" and o.startswith("esp,"):
            esp += int(o.split(",")[1].strip(), 16)
        elif m == "call":
            if pending:
                imm = callee_ret_imm(exe, int(o, 16)) if o.startswith("0x") else None
                if imm:
                    cleaned = min(pending, imm)
                    pending -= cleaned
                    esp += cleaned
                    notes.append(f"call@{i.address:#x} 的被调方 `ret {imm:#x}` 清掉 {cleaned:#x} 字节栈参")
        elif m in ("ret", "jmp") or m.startswith("j"):
            notes.append(f"区内有 {m}@{i.address:#x}（跳过区不是直线，本条判定只按直线算）")
        for r in ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp"):
            if o.startswith(r + ",") and m in ("mov", "lea"):
                regs.add(r)

    if bare_ret and entry and is_epilogue_ret(exe, target):
        print("  ✓ L3 站点是**函数序言**、落点是该函数**尾声的裸 `ret`** ⇒ 等于「整个函数没跑」"
              "（序言/函数体/尾声一起被跳过），`ret` 正好弹调用者压的返回地址（cdecl 形态）")
        return True

    print(f"  · 落点处 esp = 站点处 esp（被跳过的指令不执行）；跳过区真实净 esp = {esp:+#x}"
          + (f"，仍有未清的 push {pending:#x}" if pending else ""))
    for n in notes:
        print(f"  · {n}")
    if regs:
        print(f"  · 区内写过的寄存器：{sorted(regs)}（落点若依赖它们就要人确认）")

    if resets:
        print("  ✓ L2 落点前 6 条内有 `mov esp,ebp` / `leave` ⇒ **漂移清零**，栈中性成立")
        return True
    if bare_ret and entry and pending == 0:
        print("  ✓ L3 站点是**函数序言** + 落点是裸 `ret` + 跳过区栈中性 ⇒ 等于「整个函数没跑」，"
              "`ret` 正好弹调用者压的返回地址（cdecl 形态）")
        return True
    if esp == 0 and pending == 0:
        print("  ✓ L1 跳过区真实净 esp = 0 ⇒ 落点与站点同 esp，栈中性成立")
        return True
    print("  ⚠ L4 三条都没同时成立 ⇒ **要人确认**（口径见本文件头：三条合法形态）")
    return False


def check_caller_skip(exe, va, cs):
    """落点 = 调用者返回地址（= 跳过整个函数）：报出函数自己的 ret 与应有的 stack_clear_size。"""
    print("落点 = 调用者返回地址（跳过整个函数）")
    body = exe.disasm(va, 0x400) or []
    for i in body:
        if i.mnemonic == "ret" and is_epilogue_ret(exe, i.address):
            if i.op_str:
                n = int(i.op_str, 16)
                print(f"  · 该函数自己的 ret 在 {i.address:#x}：`ret {n:#x}`（stdcall/thiscall，被调方清栈）"
                      f"\n    ⇒ 声明里写 `stack_clear_size: {4 + n:#x}`（= 4 + 函数自身清理量；桩那条 `call` 的返回地址也要弹掉）")
            else:
                print(f"  · 该函数自己的 ret 在 {i.address:#x}：裸 `ret`（cdecl，调用者清栈）"
                      f"\n    ⇒ 只需要把落点设成调用者返回地址即可，**不要**再写 `stack_clear_size`")
            return True
    print("  ⚠ 在 0x400 字节内没扫到「尾声 ret」（`pop ebp` 后那条）—— 函数可能更长，手工确认")
    return False


# ── sdk.c 里自动扫落点（回归用）──────────────────────────────────────────
SKIP_RE = re.compile(r"ce_retaddr_set\(regs,\s*(0x[0-9a-fA-F]+|sp\[\d+\]|CE_[A-Z0-9_]+)\s*\)")
FN_RE = re.compile(r"^int\s+__cdecl\s+BP_([A-Za-z0-9_]+)\s*\(", re.M)


MACRO_RE = re.compile(r"^#define\s+(CE_[A-Z0-9_]+)\s+(0x[0-9a-fA-F]+)u?\s*(?:/\*.*)?$", re.M)


def load_macros():
    """把 `native/*.h` 里的 `#define CE_X 0x…` 收成字典（落点常量多是宏，如 CE_CHAIN_RESUME）。"""
    out = {}
    for f in sorted(NATIVE.glob("*.h")) + sorted(NATIVE.glob("cards/*.h")):
        try:
            src = f.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for m in MACRO_RE.finditer(src):
            out.setdefault(m.group(1), int(m.group(2), 16))
    return out


def scan_skip_points():
    """从 native/sdk.c 扫出 (站点名, 落点表达式) 列表 —— 站点名 = 所在 handler 的 BP_<名>。"""
    out = []
    for f in sorted(NATIVE.glob("*.c")) + sorted((NATIVE / "cards").glob("*.c")):
        src = f.read_text(encoding="utf-8", errors="replace")
        fns = [(m.start(), m.group(1)) for m in FN_RE.finditer(src)]
        if not fns:
            continue
        for m in SKIP_RE.finditer(src):
            site = None
            for off, name in fns:
                if off < m.start():
                    site = name
                else:
                    break
            if site:
                out.append((f.name, site, m.group(1)))
    return out


def run_all(exe, verbose=False):
    bps, bhs = collect_items()
    declared = {name for _src, name, _va, _cs in bps}
    park = parked_sites()
    print(f"== 声明 {len(bps)} 个断点窗口 / {len(bhs)} 条 binhack（ours + test + base_tsa）；"
          f"sites.py 全表 {len(park)} 个站点（含 park）==")
    bad = 0
    warn = 0
    print("-- ① 站点窗口检查（声明的 + park 的）--")
    for name, (va, cs) in sorted(park.items(), key=lambda kv: kv[1][0]):
        if not cs:
            continue
        live = name in declared
        if not live:
            print(f"站点 {va:#08x}（{name}，**park/未声明**：现在没装；放进发布面前必须过）")
        if not check_window(exe, va, cs, bps, bhs, name):
            if live:
                bad += 1
            else:
                warn += 1
        elif verbose:
            print()
        else:
            print("  ✓ 四项全过")
        print()
    print("-- ② 跳过点（sdk.c 自动扫出）的落点检查 --")
    by_name = {name: (va, cs) for _src, name, va, cs in bps}
    for name, (va, cs) in park.items():
        by_name.setdefault(name, (va, cs))
    macros = load_macros()
    seen = set()
    for fname, site, expr in scan_skip_points():
        key = (site, expr)
        if key in seen:
            continue
        seen.add(key)
        if site not in by_name:
            print(f"  ⚠ {fname}: BP_{site} 的站点在 sites.py 里也找不到 ⇒ 跳过")
            continue
        va, cs = by_name[site]
        tag = "" if site in declared else "（**park/未声明**：现在没装，放进发布面前必须过）"
        print(f"  【{fname}: BP_{site}】{tag}", end=" ")
        tgt = int(expr, 16) if expr.startswith("0x") else macros.get(expr)
        if tgt is not None:
            if not check_landing(exe, va, cs, tgt):
                bad += 1
        else:
            check_caller_skip(exe, va, cs)      # 动态落点（sp[…]=调用者返回地址）
        print()
    tail = f"（窗口 {len(park)} 个 / 落点 {len(seen)} 处）"
    if bad == 0 and warn == 0:
        print("✅ 全部通过" + tail)
    elif bad == 0:
        print(f"✅ 已声明的一切通过；另有 {warn} 处 **park 站点**的隐患（现在没装，但别原样放进发布面）" + tail)
    else:
        print(f"✗ 合计 {bad} 处必须处理（另有 {warn} 处 park 隐患）" + tail)
    return 0 if bad == 0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("addr", nargs="?", help="站点虚拟地址，如 0x45e930")
    ap.add_argument("cavesize", nargs="?", type=int, help="窗口字节数（thcrap 的 cavesize）")
    ap.add_argument("--ret", help="落点：地址（0x…）、`caller`（跳过整个函数）")
    ap.add_argument("--all", action="store_true", help="回归：站点全表（含 park）+ sdk.c 扫出的落点")
    ap.add_argument("-v", "--verbose", action="store_true", help="--all 时连通过的也逐条打")
    ap.add_argument("--exe", default=str(EXE))
    a = ap.parse_args()

    exe_path = Path(a.exe)
    if not exe_path.exists():
        print(f"⚠ 找不到样本 exe：{exe_path}（本工具只读真 exe；没有样本就跳过，退出码 0）")
        return 0
    exe = Exe(exe_path)
    if Cs is None:
        print("⚠ 没装 capstone（`pip install capstone`）⇒ 只能做重叠检查")

    if a.all:
        return run_all(exe, a.verbose)
    if not a.addr or not a.cavesize:
        ap.error("要么给 `--all`，要么给 `地址 窗口字节数 [--ret …]`")
    va = int(a.addr, 16)
    bps, bhs = collect_items()
    ok = check_window(exe, va, a.cavesize, bps, bhs)
    if a.ret:
        print()
        if a.ret == "caller":
            ok = check_caller_skip(exe, va, a.cavesize) and ok
        else:
            ok = check_landing(exe, va, a.cavesize, int(a.ret, 16)) and ok
    print("\n" + ("✅ 通过" if ok else "✗ 有要处理的地方（详见上面每条的说明）"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
