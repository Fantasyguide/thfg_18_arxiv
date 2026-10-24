import os
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

    # ⑤ `BP_EXEC_ORIGINAL` 普查：站点 ← handler 调用链 → 窗口判据（§BE-ER 普查的复跑版）
    python3 native/tools/sitecheck.py --replay-census

## 为什么要有它

2026-09-18 那次「一按 Z 就崩」不是玩法 bug，是**站点/落点记账**出的错：
`stack_clear_size` 少写了 4 字节（桩的 `ret` 弹的槽比调用者压的返回地址低一格）⇒ 调用链的栈整体偏低 4
⇒ 上层 `pop ebx` 弹出 `3` ⇒ 派发器 `th18:0x4013ba` 读 `0x53` 违例。
而站点本身还有四条**成文规矩**（窗口 ≥5 字节、整指令边界、窗内无相对寻址、与任何 binhack/站点零重叠），
以前全靠人眼 + 单点脚本。这个工具把这五件事变成一次可复跑的输出 —— **加站点前先跑它**。

判定口径与一手指令表：`engine/_shared/thcrap-breakpoint-pools.md`（§2 桩语义 / §8 栈中性审计）。
机器对账的用法：新站点先跑 ①，要改落点再跑 ②/③；`make check` 跑 ④ 兜住既有的一批。

## ★★ 回放禁令（W5/W6，2026-09-20 立，出处 = `AUDIT.md` §BE-ER + `WORK-PACKAGES.md` §7-B）

一手机制：thcrap 给每条声明在跳板池里留一格放**站点原始字节**，handler 返回 `BP_EXEC_ORIGINAL`(=1) 时
**从池里回放**那几字节；而 `cave_fix` **只重定位「窗口第一条就是 `e8`/`e9`」**那一种
⇒ 窗口里其余近相对分支（`0f 8x` 的 jcc / `e9` / **非首条**的 `e8`）按**原相对位移**从**池的地址**跳
⇒ 落进池的未写区 ⇒ 执行 `00 00`（`add byte ptr [eax],al`）、此刻 `Eax = 1` ⇒ **写 `0x1` 的访问违例**。
逐字节复算见 §BE-ER：`ce_poc_gate_a` 的池副本 `0x03c20250`，`+6+0x3a4 = 0x03c205fa` **正是 dump 的 `Eip`**。

⇒ 两条硬规则（都写在**声明源**上，不靠人肉例外名单）：

| | 规则 | 判红条件 |
| --- | --- | --- |
| **W5** | 窗口含上述分支的站点，**必须**在 `native/sites.py` 的 `_SDK_SITE_FIELDS` 里声明 `no_replay`（值 = 前提说明）| 含分支**且**未声明 ⇒ **失败**（不是警告）。判据 = 逐指令反汇编（capstone），可复跑 |
| **W6** | 声明了 `no_replay` 的站点，其 handler **禁止**返回 `BP_EXEC_ORIGINAL` | `BP_<站名>`（及其在 `native/*.c` 里能追到的被调函数）里出现 `BP_EXEC_ORIGINAL` ⇒ **失败** |

W6 正是 2026-09-19 那次崩溃的形态：有人给 `ce_poc_gate_common()` 加了一句
`if (…< 0) return BP_EXEC_ORIGINAL;` ⇒ 池里回放那条 `ja` ⇒ 跳进未写区。
**唯一例外**：`patch-test` 里的**故意复现实验**站点（`ce_canary_a/b`，见 §BE-DW 的验证配方）就是要把这条
分支回放出来验算术 ⇒ 它们只打 ⚠ 大声提示，**不判红**（patch-test 默认不进发布面）。
**park 的站点**（`sites.py` 里在 `_SDK_KEEP_NAMED` 之外的）也一起查：现在没装 = 没风险，
但**放回发布面前**必须过 —— 那时 W6 会自动从 ⚠ 升级成 ✗。

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
MODKIT = Path(os.environ.get("MODKIT_DIR", "") or ".")
BASE_TSA = MODKIT / "thcrap" / "repos" / "nmlgc" / "base_tsa" / "th18.v1.00a.js"

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
except ImportError:                                # 只读体检，没有 capstone 就别硬跑
    Cs = None

# ★ 权威**声明源**：直接 import `native/sites.py`（不是正则扫它）—— `no_replay` 这类元数据字段
#   必须由生成器自己说的那份声明来判，工具另抄一份就等着漂移。import 失败（缺 shape.py 一类）时
#   退回正则扫描 `parked_sites()`，并在输出里说清楚「字段判据不可用」。
sys.path.insert(0, str(NATIVE))
try:
    import sites as SITE_DECL
    SITE_DECL_ERR = None
except Exception as _e:                            # noqa: BLE001 —— 只读体检，不因此中断
    SITE_DECL, SITE_DECL_ERR = None, _e

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


# ── 声明源：`no_replay`（回放禁令）────────────────────────────────────────
def no_replay_decls():
    """→ `{站点名: 前提说明}`：`native/sites.py` 里**已声明 `no_replay`** 的站点（含 park 的）。

    字段语义（`sites.py` 的 `_SDK_SITE_FIELDS` 顶上那段是唯一出处）：窗口含 `cave_fix` 修不了的近相对
    分支 ⇒ handler **禁止**返回 `BP_EXEC_ORIGINAL`。这里是 W5/W6 的**判据源**。"""
    if SITE_DECL is None:
        return {}
    try:
        return SITE_DECL.sdk_no_replay()
    except Exception:                              # noqa: BLE001
        return {}


def declaration_table():
    """→ `{站点名: (地址, 窗口字节数, park?)}`：sites.py 的**全部** SDK/商店站点（含 park 的）。

    park 的用 `SDK_SITES_ALL`（过滤前）拿 —— 它们没进 patch，但**放回发布面时用的就是这份声明**，
    所以 W5/W6 现在就查。"""
    out = {}
    if SITE_DECL is None:
        return out
    live = {s[0] for s in getattr(SITE_DECL, "SDK_SITES", ())}
    for name, va, n, _exp, _title in getattr(SITE_DECL, "SDK_SITES_ALL", ()):
        out[name] = (va, n, name not in live)
    for name, va, n, _exp, _title in getattr(SITE_DECL, "SHOP_SITES", ()):
        out.setdefault(name, (va, n, False))
    return out


# ── 回放判据（W5/W6 共用）：窗口里有没有 `cave_fix` 修不了的近相对分支 ────────
_REL_PREFIXES = (0x66, 0x67, 0xf2, 0xf3, 0x2e, 0x36, 0x3e, 0x26, 0x64, 0x65)


def _first_opcode(raw):
    """跳过常见前缀后的第一个 opcode 字节（判首条是不是 `e8`/`e9` 用）。"""
    i = 0
    while i < len(raw) and raw[i] in _REL_PREFIXES and i < 4:
        i += 1
    return raw[i] if i < len(raw) else None


def rel_kind(raw):
    """按**指令字节**判相对分支的宽度：`0` = 不是相对分支；`8` = rel8；`32` = rel32。

    ⚠ 为什么必须看字节、不能看 capstone 的 `op_str`：capstone 把 jcc/jmp 的目标**解析成绝对地址**
      打印（`eb 00` 打印成 `jmp 0x…`，与 `e9 rel32` 长得一模一样）⇒ 按字符串判会把**短跳当成 rel32**，
      判据直接失真（这一条就是自检 `--selftest` 抓出来的，2026-09-20）。
    判据本身：`0f 8x` = `jcc rel32`；`e8/e9` = `call/jmp rel32`；`70..7f / eb / e0..e3` = rel8
    （自地址无关 ⇒ 搬进池仍指向池内的同一处，安全）。间接转移（`ff /2`、`ff /4`）不带相对量。"""
    b0 = _first_opcode(raw)
    if b0 is None:
        return 0
    i = raw.index(b0)
    if b0 == 0x0f and i + 1 < len(raw) and 0x80 <= raw[i + 1] <= 0x8f:
        return 32
    if b0 in (0xe8, 0xe9):
        return 32
    if 0x70 <= b0 <= 0x7f or b0 == 0xeb or 0xe0 <= b0 <= 0xe3:
        return 8
    return 0


def rel32_branches(va, insns, blob=None):
    """→ (fatal, fixable, rel8)

    `fatal`   = 会在池里**跳飞**的近相对分支：`jcc rel32` / `jmp rel32` / **非首条**的 `call rel32`
                （thcrap 的 `cave_fix` 只重定位「窗口第一条就是 `e8`/`e9`」那一种 ⇒ 其余按原位移从池的
                地址跳 ⇒ 落进池的未写区，AUDIT §BE-ER / §BE-DW）；
    `fixable` = 窗口第一条的 `e8/e9 rel32`（cave_fix 会修）。⚠ **`0f 8x` 的首条不算** ——
                那正是 `ce_poc_gate_a/b` 那条 `ja` 的形态（§BE-ER 的崩点就是它）；
    `rel8`    = 窗内短跳（自地址无关，进 cave 安全）。

    ★ 判据是「**rel32 分支转移**」，不是「跳转」：`e8`（call）在窗内非首条时同样致命 —— 从池里执行会
      call 到「池地址 + 5 + 原位移」，一样落进未写区。`blob` = 窗口那 cs 个字节（判宽度用，见 rel_kind）。"""
    fatal, fixable, rel8 = [], [], []
    for i in insns:
        raw = blob[i.address - va:i.address - va + i.size] if blob else None
        kind = rel_kind(raw) if raw else 0
        if kind == 32:
            # 窗口第一条就是 `e8/e9 rel32` ⇒ cave_fix 会重定位好（thcrap 唯一会修的那种）
            if i.address == va and _first_opcode(raw) in (0xe8, 0xe9) and not fixable:
                fixable.append(f"{i.mnemonic} {i.op_str}@+0x0")
            else:
                fatal.append(f"{i.mnemonic} {i.op_str}@+{i.address - va:#x}")
        elif kind == 8:
            rel8.append(f"{i.mnemonic} {i.op_str}@+{i.address - va:#x}")
    return fatal, fixable, rel8


# ── W5/W6 的**判定表**（纯函数：把「判据」与「打印」分开，判据本身才可穷举自检）──
def replay_verdict(fatal, no_replay, replay, live=True, source=None):
    """→ `(level, 说明)`；`level ∈ {"fail","warn","ok"}`（`fail` = `make check` 判红）。

    | 窗口含致命分支 | 声明 `no_replay` | handler 会回放 | live | 判定 |
    | --- | --- | --- | --- | --- |
    | 是 | 否 | — | — | **fail**（W5）—— 除 `patch-test` 的故意复现实验（⇒ warn） |
    | 是 | 是 | 是 | 是 | **fail**（W6）—— 声明了禁令却还回放 = §BE-ER 的崩法 |
    | 是 | 是 | 是 | 否 | warn（park 中，放回发布面前必须删掉回放） |
    | 是 | 是 | 否 | — | ok（观测者/恒返 0：窗口合法） |
    | 否 | 否 | 是 | — | ok（窗内无分支 ⇒ 回放 = 原地执行，§BE-ER 的普查口径） |
    | 否 | 是 | 是 | 是 | **fail**（W6）—— 声明是**承诺**：声明了就得做到，与窗口为什么被声明无关 |
    """
    if fatal and not no_replay:
        if source == "test":
            return "warn", "W5：窗内含近相对分支且未声明 `no_replay` —— 但它是 `patch-test` 的故意复现实验"
        return "fail", ("W5：窗内含 cave_fix 修不了的近相对分支、且站点未声明 `no_replay` ⇒ "
                        "handler 一旦回放就跳进池的未写区（§BE-ER）")
    if replay and no_replay:
        if live:
            return "fail", ("W6：站点已声明 `no_replay`（承诺「handler 恒返回 0 + 自己给落点」），"
                            "handler 却仍返回 `BP_EXEC_ORIGINAL` ⇒ 一旦窗口里有近相对分支，"
                            "回放就是 §BE-ER 的崩法")
        return "warn", "W6：已声明 `no_replay`，park 中的 handler 仍会回放（放回发布面前必须删掉那条）"
    return "ok", ""


# ── 单站点五项检查 ──────────────────────────────────────────────────────
def check_window(exe, va, cs, bps, bhs, self_name=None, ctx=None):
    """ctx（可选）= {"no_replay": 前提说明|None, "replay": [证据]|None, "live": bool, "source": str}

    `replay` = 该站点的 handler（`BP_<名>` 及其被调函数）**可能返回 `BP_EXEC_ORIGINAL`** 的证据。"""
    ctx = ctx or {}
    no_replay = ctx.get("no_replay")
    replay = ctx.get("replay")
    live = ctx.get("live", True)
    source = ctx.get("source")
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

    # W3 相对寻址 + W5 回放禁令（判据 = 逐指令反汇编；口径见文件头「回放禁令」那节）
    fatal = []
    if insns is None:
        print("  ⚠ W3/W5 相对寻址·回放禁令：没装 capstone 或地址读不到 ⇒ **判据不可用**，跳过"
              "（这一档要人工补：装 capstone 后重跑）")
    else:
        fatal, fixable, rel8 = rel32_branches(va, insns, exe.read(va, cs))
        if fatal and (self_name or "") in DANGER_W3:
            print(f"  ⚠ W3/W5 相对寻址·回放禁令：{DANGER_W3[self_name]}")
            fatal = []
        elif fixable:
            print(f"  ✓ W3 相对寻址：窗口首条是 `{fixable[0]}` = `e8/e9 rel32` ⇒ "
                  f"**cave_fix 会重定位**，安全（这是 thcrap 唯一会修的那种）")
        if fatal:
            premise = (f"窗口 {va:#x}+{cs} 含 **cave_fix 修不了的近相对分支** {fatal}"
                       f"（thcrap 只重定位「窗口第一条就是 e8/e9」那一种）⇒ 从跳板池回放会**跳飞**到"
                       f"「池地址 + 原相对位移」= 池的未写区 ⇒ `00 00`、`Eax=1` ⇒ 写 `0x1` 的访问违例"
                       f"（AUDIT §BE-ER）")
            if no_replay:
                print(f"  ✓ W3 相对寻址：{premise}")
                print(f"  ✓ W5 回放禁令：站点**已声明 `no_replay`** ⇒ 该窗口合法；前提 = {no_replay}")
            elif source == "test":
                print(f"  ⚠ W5 回放禁令：{premise}；而站点**未声明** `no_replay` —— 但它在 `patch-test`"
                      f"（默认关、不进发布面的**故意复现实验**归档，见 AUDIT §BE-DW 的验证配方）⇒ 只提示、**不判红**")
            else:
                print(f"  ✗ W5 回放禁令：{premise}；而站点**未声明** `no_replay` ⇒ 必须二选一："
                      f"① 换一个不含这种分支的窗口；② 在 `native/sites.py` 的 `_SDK_SITE_FIELDS` 里给 "
                      f"`{self_name or hex(va)}` 加 `\"no_replay\": \"<前提说明>\"`（= 承诺 handler 恒返回 0 + "
                      f"自己给落点）。判据是逐指令反汇编，不是例外名单（WORK-PACKAGES §7-B）")
                # 判红不在这里做：见本函数尾部 `replay_verdict()`（打印与判定只许有一个出处）
        elif rel8:
            print(f"  ✓ W3 相对寻址：只有短跳（rel8，自地址无关）{rel8} —— 进 cave 安全")
        else:
            print("  ✓ W3 相对寻址：窗内无分支")

    # W6 handler 闸：声明了 `no_replay` 的站点，handler 里出现 `BP_EXEC_ORIGINAL` 就是事故形态
    if replay:
        ev = "、".join(replay)
        if no_replay:
            if live:
                print(f"  ✗ W6 回放禁令：站点已声明 `no_replay`，但它的 handler 会返回 "
                      f"`BP_EXEC_ORIGINAL`（{ev}）⇒ 回放那条分支 = 池里跳飞（AUDIT §BE-ER 的崩法）。"
                      f"删掉那条 `return BP_EXEC_ORIGINAL`（改成恒返回 0 + 自己给落点）")
            else:
                print(f"  ⚠ W6 回放禁令：站点已声明 `no_replay`，但它的 handler 仍会返回 "
                      f"`BP_EXEC_ORIGINAL`（{ev}）—— 现在 park（没装）⇒ 只提示；**放回发布面前必须删掉那条**"
                      f"（否则 W5/W6 一起判红：这正是 2026-09-19 那次崩溃的形态）")
        elif fatal:
            pass                        # W5 已经判红了，这里不重复刷屏
        else:
            print(f"  ✓ R 回放安全：handler 会返回 `BP_EXEC_ORIGINAL`（{ev}），但**窗内没有** cave_fix 修不了的"
                  f"近相对分支 ⇒ 从池里回放等价于原地执行（§BE-ER 的普查口径）")
    elif no_replay:
        print(f"  · W6：找不到 `BP_{self_name}` 的源码，或它不返回 `BP_EXEC_ORIGINAL` ⇒ 声明未被违反")

    # ★ 判红的**唯一出处** = 判定表（打印与判定不许各说一套；表本身由 `--selftest` 穷举自检）
    level, why = replay_verdict(bool(fatal), no_replay, replay, live, source)
    if level == "fail":
        ok = False
        print(f"  ✗ 回放禁令判定：{why}")

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


# ── ★ 这里**没有**「已论证的例外」名单了（2026-09-20 删）────────────────────
# 老版本在这个位置有一张 `EXEMPT_W3` 表，给 `ce_poc_gate_a/b` 写一句「已论证的例外：handler 恒返回 0、
# cave 永不执行」。**前提是人肉维持的** —— 2026-09-19 有人往 handler 里加了一句
# `return BP_EXEC_ORIGINAL` 守卫，那条前提当场失效、游戏崩在池里（AUDIT §BE-ER）。
# ⇒ 现在改成两条**机器判据**：W5（含分支必须声明 `no_replay`，声明源 = `sites.py` 的
#   `_SDK_SITE_FIELDS`）+ W6（声明了却仍回放 ⇒ 判红）。判据由反汇编与源码扫描产生，不靠这张表。
#
# `DANGER_W3` 只剩**故意**的复现实验站点：`CE_CANARY_POC_GATE=1` 时进 `patch-test`（默认关），
# 它的**唯一用途**就是让 cave 回放那条 `ja` 来验证 §BE-DW 的算术 ⇒ 这里只大声提示、不判红。
# patch-test 的其它站点同理（见 check_window 里 `source == "test"` 那一支）。
DANGER_W3 = {
    "ce_canary_a": "★★ 复现实验站点（CE_CANARY_POC_GATE=1）：**故意**让 cave 回放那条 ja ⇒ 预期崩在"
                   "「首个全零格 − 0x10 + 0x3aa」。跑完必须撤 env 复原（AUDIT §BE-DW）。",
    "ce_canary_b": "★★ 同上（第二条 ja）。",
}

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


# ── `BP_EXEC_ORIGINAL` 可达性扫描（W6 + `--replay-census` 的唯一判据源）──────
# 为什么不能只在 `BP_<名>` 函数体里 grep：真正的形态是 **handler 调一个公共 helper**，
# `BP_EXEC_ORIGINAL` 写在 helper 里 —— 2026-09-19 那次崩溃正是
# `BP_ce_poc_gate_a/b` → `ce_poc_gate_common()` → `return BP_EXEC_ORIGINAL`。
# 所以这里做**站内调用图的可达性**（函数体内的调用按已知函数名解析，够用且看得懂）。
C_FUNC_RE = re.compile(
    r"^(?!#)[A-Za-z_][A-Za-z0-9_ \t\*]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)\s*\{", re.M)
CALL_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
REPLAY_TOKEN = "BP_EXEC_ORIGINAL"
C_STRIP_RE = re.compile(r"/\*.*?\*/|//[^\n]*|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'", re.S)


def _strip_c(src):
    """去掉注释与字符串字面量（**保留换行**，行号才对得上）。

    ⚠ 必须去注释：`sdk.c` 的注释里到处是 `BP_EXEC_ORIGINAL` 这个词（解释为什么不许回放），
    不去掉就会把「文档」当成「代码」，W6 变成永远的假阳性。"""
    return C_STRIP_RE.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), src)


def _brace_body(src, i):
    """从 `{`（下标 i）起做括号配对，返回体内全文（含花括号）。"""
    depth, j, n = 0, i, len(src)
    while j < n:
        c = src[j]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[i:j + 1]
        j += 1
    return src[i:]


def scan_calls(srcs):
    """`srcs = {文件名: 源码}` → `{"may": {站点名: [证据]}, "funcs": {...}, "orphans": [证据]}`

    `may[站点]` 非空 ⇔ 该站点的 handler（`BP_<站点>`）**可能返回 `BP_EXEC_ORIGINAL`**；
    证据 = 调用链上每一处 `BP_EXEC_ORIGINAL` 的 `文件:行 \`函数()\``。

    ★ 输入是**源码字典**（不是文件列表）⇒ `--selftest` 能喂合成源码验这套可达性逻辑，
      不必依赖 `sdk.c` 当刻的内容（那份文件随时有人在改，判据自检不能跟着漂）。"""
    srcs = {k: _strip_c(v) for k, v in srcs.items()}
    funcs = {}
    for fname, src in srcs.items():
        for m in C_FUNC_RE.finditer(src):
            name = m.group(1)
            if name in funcs:
                continue
            b0 = m.end() - 1                       # `{` 的下标
            body = _brace_body(src, b0)
            funcs[name] = {
                "file": fname, "line": src.count("\n", 0, m.start()) + 1, "body": body,
                "replay_lines": [src.count("\n", 0, b0 + p) + 1
                                 for p in [x.start() for x in re.finditer(r"\b" + REPLAY_TOKEN + r"\b", body)]],
            }
    for name, f in funcs.items():
        f["calls"] = {c for c in CALL_RE.findall(f["body"]) if c in funcs and c != name}

    cache = {}

    def reach(name):
        if name in cache:
            return cache[name]
        cache[name] = []                       # 先占位：递归里有环也不会死循环
        f = funcs.get(name)
        ev = []
        if f:
            ev += [f"{f['file']}:{ln} `{name}()`" for ln in f["replay_lines"]]
            for c in sorted(f["calls"]):
                ev += reach(c)
        cache[name] = ev
        return ev

    may = {}
    for name in sorted(funcs):
        if name.startswith("BP_"):
            ev = sorted(set(reach(name)))
            if ev:
                may[name[3:]] = ev
    # 兜底：有没有哪处 `BP_EXEC_ORIGINAL` 谁都到不了（改完 handler 忘了删 / 写错函数名）
    reached = set()
    for name in funcs:
        if name.startswith("BP_"):
            for ev in reach(name):
                reached.add(ev.split(" ")[0])
    orphans = []
    for name, f in sorted(funcs.items()):
        for ln in f["replay_lines"]:
            if f"{f['file']}:{ln}" not in reached:
                orphans.append(f"{f['file']}:{ln} `{name}()`（没有 `BP_*` handler 调得到它）")
    return {"may": may, "funcs": funcs, "orphans": sorted(set(orphans))}


def scan_replay():
    """按**仓库现状**跑 `scan_calls()`（读 `native/*.c` + `native/cards/*.c`；读不到就当空结果）。

    ⚠ 它的结果**随源码变化**（`sdk.c` 随时有人在改）⇒ 自检不许拿它当期望值（见 `selftest` 的合成用例）。"""
    srcs = {}
    for f in sorted(NATIVE.glob("*.c")) + sorted((NATIVE / "cards").glob("*.c")):
        try:
            srcs[f.name] = f.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
    try:
        return scan_calls(srcs)
    except Exception:                              # noqa: BLE001 —— 体检工具不该因解析失败而崩
        return {"may": {}, "funcs": {}, "orphans": []}


def replay_map():
    try:
        return scan_replay()["may"]
    except Exception:                              # noqa: BLE001 —— 体检工具不该因解析失败而崩
        return {}


def site_info(exe=None, bps=None):
    """→ `{站名: {"va","cs","live","source","park"}}`：**并集**（三份 patch 的声明 + sites.py 全表）。

    两个来路都要（少一个就漏判）：
      · `collect_items()` 的三份 patch ⇒ **真声明**（含 base_tsa 与 inline 的 ce_gate / 音效 / unlock 站点）；
      · `sites.py` 的 `SDK_SITES_ALL` / `SHOP_SITES` + 正则兜底 ⇒ **park 的与注释掉的候选站点**
        （现在没装，但放回发布面时用的就是这份声明，所以 W5/W6 现在就查）。"""
    out = {}
    for name, (va, cs, park) in declaration_table().items():
        out[name] = {"va": va, "cs": cs, "live": False, "source": None, "park": park}
    for name, (va, cs) in parked_sites().items():           # 注释掉的候选（正则兜底）
        out.setdefault(name, {"va": va, "cs": cs, "live": False, "source": None, "park": True})
    for src, name, va, cs in (bps if bps is not None else collect_items()[0]):
        it = out.setdefault(name, {"va": va, "cs": cs, "live": False, "source": None, "park": False})
        it["live"] = True
        if it.get("source") in (None, "base_tsa"):
            it["source"] = src
        if cs:
            it["cs"] = cs
    return out


# ── 判据自检（离线：不需要样本 exe / 不改任何声明）────────────────────────
def selftest(quiet=False):
    """用**真字节**造几个窗口，验证 W5/W6 的判据本身（`--selftest`；`--all` 也会跑一遍）。

    为什么值得有：这两条判据拦的是**崩游戏**的事故（§BE-ER），而它们自己若写错（比如把
    「窗口首条 e8/e9」也算致命、或把注释里的 `BP_EXEC_ORIGINAL` 当成代码）就会**静默失灵**。
    所以判据本身也要有一条可复跑的证伪路径 —— 这里是「喂已知答案的窗口，答错就报红」。"""
    cases, fails = [], []
    if Cs is None:
        # ⚠ 与全工具的契约一致：没有样本/没有 capstone 时**跳过并 exit 0**，不把环境缺失当失败。
        if not quiet:
            print("-- ⓪ 判据自检：跳过（没装 capstone ⇒ 判据本身也跑不了；这一档不算失败）--\n")
        return 0, []
    cs = Cs(CS_ARCH_X86, CS_MODE_32)

    def run(label, va, raw, want_fatal, want_fixable=0):
        insns = list(cs.disasm(raw, va))
        fatal, fixable, _rel8 = rel32_branches(va, insns, raw)
        got = (len(fatal), len(fixable))
        cases.append((label, got, (want_fatal, want_fixable)))
        if got != (want_fatal, want_fixable):
            fails.append(f"{label}：判据给 {got}，期望 {(want_fatal, want_fixable)}")

    run("窗口 = 一条 `ja rel32`（ce_poc_gate_a 的字节）", 0x445bb9, bytes.fromhex("0f87a4030000"), 1)
    run("窗口 = 一条向后 `ja rel32`（ce_poc_gate_b 的字节）", 0x446003, bytes.fromhex("0f875affffff"), 1)
    run("窗口首条 = `e8 rel32`（cave_fix 会重定位 ⇒ 不致命）", 0x1000, bytes.fromhex("e800000000"), 0, 1)
    run("窗内非首条 `e9 rel32`（致命）", 0x2000, bytes.fromhex("90e900000000"), 1)
    run("窗内非首条 `jcc rel32`（致命），前面一条无关指令", 0x3000, bytes.fromhex("8b45080f8400000000"), 1)
    run("窗内只有 rel8 短跳（自地址无关 ⇒ 不致命）", 0x4000, bytes.fromhex("90eb00"), 0)
    run("窗内无分支（`mov edx,[ebp+8]; sub esp,0xc`）", 0x40a9c3, bytes.fromhex("8b550883ec0c"), 0)

    # 注释/字符串里的 `BP_EXEC_ORIGINAL` 不许被当成代码（sdk.c 的注释里到处是这个词）
    stripped = _strip_c("/* return BP_EXEC_ORIGINAL; */\nint f(void){ return 0; } // BP_EXEC_ORIGINAL\n")
    if REPLAY_TOKEN in stripped:
        fails.append("`_strip_c` 没剥掉注释里的 `BP_EXEC_ORIGINAL` ⇒ W6 会假阳性")
    else:
        cases.append(("`_strip_c` 剥掉注释里的 `BP_EXEC_ORIGINAL`", ("ok",), ("ok",)))
    stripped2 = _strip_c('int f(void){ return BP_EXEC_ORIGINAL; }')
    if REPLAY_TOKEN not in stripped2:
        fails.append("`_strip_c` 把真代码里的 `BP_EXEC_ORIGINAL` 也剥掉了 ⇒ W6 会假阴性")
    else:
        cases.append(("`_strip_c` 保留代码里的 `BP_EXEC_ORIGINAL`", ("ok",), ("ok",)))
    # 可达性（**合成源码**，不看仓库现状）：`BP_EXEC_ORIGINAL` 写在 handler 调的 helper 里也要抓到
    # —— 2026-09-19 的形态就是 handler → 公共 helper → 回放（sdk.c 随时有人在改，自检不能跟着漂）。
    syn = {
        "x.c": "static int helper(int a)\n{\n    if (a < 0) return BP_EXEC_ORIGINAL;\n    return 0;\n}\n"
               "int __cdecl BP_site_a(x86_reg_t *regs, void *bp_info)\n{\n    return helper(1);\n}\n"
               "/* BP_EXEC_ORIGINAL 只是注释，不许被算进来 */\n"
               "int __cdecl BP_site_b(x86_reg_t *regs, void *bp_info)\n{\n    return 0;\n}\n",
    }
    r = scan_calls(syn)
    if any("x.c:3" in e and "helper" in e for e in r["may"].get("site_a", [])):
        cases.append(("调用链可达性：`BP_site_a` → `helper()` 里的回放被抓到（合成源码）", ("ok",), ("ok",)))
    else:
        fails.append("调用链可达性没抓到「handler → helper 里的回放」⇒ W6 会漏（这正是 2026-09-19 的形态）")
    if "site_b" in r["may"]:
        fails.append("注释里的 `BP_EXEC_ORIGINAL` 被当成了代码 ⇒ W6 假阳性")
    else:
        cases.append(("注释里的 `BP_EXEC_ORIGINAL` 不算数（合成源码）", ("ok",), ("ok",)))

    # 判定表穷举（W5/W6 的**全部**输入组合 —— 含「必须判红」的两格；错了就报红）
    table = [
        # (窗口含致命分支, 声明 no_replay, handler 会回放, live, source, 期望 level)
        (True,  False, False, True,  "ours", "fail"),    # 新的分支窗口、没声明 ⇒ 判红（W5）
        (True,  False, True,  True,  "ours", "fail"),    # 同上（且 handler 会回放）
        (True,  False, True,  False, "ours", "fail"),    # park 的候选也一样要声明（W5 与装没装无关）
        (True,  True,  True,  True,  "ours", "fail"),    # 声明了却仍回放 ⇒ 判红（W6 = 本次事故形态）
        (True,  True,  True,  False, "ours", "warn"),    # park 中仍会回放 ⇒ 只提示，放回发布面即判红
        (True,  True,  False, True,  "ours", "ok"),      # 声明 + handler 恒返 0 ⇒ 合法
        (True,  False, False, True,  "test", "warn"),    # patch-test 的故意复现实验 ⇒ 不判红
        (False, False, True,  True,  "ours", "ok"),      # 窗内无分支 ⇒ 回放安全（普查口径）
        (False, True,  True,  True,  "ours", "fail"),    # 声明了禁令却回放（与窗口有没有分支无关：这是承诺）
        (False, False, False, True,  "ours", "ok"),      # 两者都不涉及
    ]
    for f, d, r, lv, src, want in table:
        got, _why = replay_verdict(f, d, r, lv, src)
        if got != want:
            fails.append(f"判定表：含分支={f} 声明={d} 回放={r} live={lv} source={src} ⇒ 给 {got}，期望 {want}")
    cases.append(("判定表 10 组合（含 W5/W6 两格必须判红）", ("ok",), ("ok",)))
    if not quiet:
        print("-- ⓪ 判据自检（离线；喂已知答案的窗口，判据答错就报红）--")
        for label, got, want in cases:
            print(f"  {'✓' if got == want else '✗'} {label}")
        for f in fails:
            print(f"  ⚠ {f}")
        print()
    return len(cases), fails


def run_all(exe, verbose=False):
    n_cases, st_fails = selftest()
    if st_fails:
        print("✗ 判据自检未过（下面每一条都会让 W5/W6 静默失灵）：")
        for f in st_fails:
            print("   " + f)
        print()
    bps, bhs = collect_items()
    info = site_info(bps=bps)
    npr = no_replay_decls()
    rmap = replay_map()
    print(f"== 声明 {len(bps)} 个断点窗口 / {len(bhs)} 条 binhack（ours + test + base_tsa）；"
          f"站点全表 {len(info)} 个（含 park/候选）==")
    print(f"   回放禁令声明源：{'native/sites.py' if SITE_DECL else '**读不到 sites.py ⇒ W5/W6 判据不可用**'}"
          f"（已声明 `no_replay` 的站点 {len(npr)} 个：{', '.join(sorted(npr)) or '—'}）")
    if SITE_DECL_ERR is not None:
        print(f"   ⚠ import sites.py 失败：{SITE_DECL_ERR!r}")
    if rmap:
        print(f"   handler 会返回 `BP_EXEC_ORIGINAL` 的站点 {len(rmap)} 个（普查见 `--replay-census`）")
    bad = len(st_fails)          # 判据自己失灵 = 判红（否则 W5/W6 会静默不生效）
    warn = 0
    print("-- ① 站点窗口检查（声明 + park/候选）--")
    for name, it in sorted(info.items(), key=lambda kv: kv[1]["va"]):
        va, cs, live = it["va"], it["cs"], it["live"]
        if not cs:
            continue
        tag = "" if live else "，**park/未声明**：现在没装；放进发布面前必须过"
        print(f"站点 {va:#08x}（{name}{tag}）")
        ctx = {"no_replay": npr.get(name), "replay": rmap.get(name),
               "live": live, "source": it.get("source"), "park": it.get("park")}
        if not check_window(exe, va, cs, bps, bhs, name, ctx):
            if live:
                bad += 1
            else:
                warn += 1
        elif verbose:
            print()
        else:
            print("  ✓ 窗口检查全过")
        print()
    print("-- ② 跳过点（sdk.c 自动扫出）的落点检查 --")
    by_name = {name: (it["va"], it["cs"]) for name, it in info.items()}
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
        tag = "" if site in {n for _s, n, _v, _c in bps} else "（**park/未声明**：现在没装，放进发布面前必须过）"
        print(f"  【{fname}: BP_{site}】{tag}", end=" ")
        tgt = int(expr, 16) if expr.startswith("0x") else macros.get(expr)
        if tgt is not None:
            if not check_landing(exe, va, cs, tgt):
                bad += 1
        else:
            check_caller_skip(exe, va, cs)      # 动态落点（sp[…]=调用者返回地址）
        print()
    # ③ 回放禁令的收尾：orca —— 有没有 `BP_EXEC_ORIGINAL` 谁都到不了（漏删/写错函数名）
    scan = scan_replay()
    if scan["orphans"]:
        print("-- ③ `BP_EXEC_ORIGINAL` 可达性（漏网检查）--")
        for o in scan["orphans"]:
            print(f"  ⚠ {o}")
        print()
    tail = f"（窗口 {len(info)} 个 / 落点 {len(seen)} 处 / 判据自检 {n_cases} 例）"
    if bad == 0 and warn == 0:
        print("✅ 全部通过" + tail)
    elif bad == 0:
        print(f"✅ 已声明的一切通过；另有 {warn} 处 **park/候选站点**的隐患（现在没装，但别原样放进发布面）" + tail)
    else:
        print(f"✗ 合计 {bad} 处必须处理（另有 {warn} 处 park/候选隐患）" + tail)
    return 0 if bad == 0 else 1


def replay_census(exe):
    """`--replay-census`：把「`BP_EXEC_ORIGINAL` 的可能返回者」逐条对它**自己站点**的窗口核一遍。

    口径（`AUDIT.md` §BE-ER）：回放安全 ⟺ 窗口里**没有** `cave_fix` 修不了的近相对分支。
    输出表：站点 → 窗口 → 窗内致命分支 → handler 是否可能返回 1 → 是否声明 `no_replay` → 结论。"""
    bps, _bhs = collect_items()
    info = site_info(bps=bps)
    npr = no_replay_decls()
    scan = scan_replay()
    rmap = scan["may"]
    print("== `BP_EXEC_ORIGINAL` 普查（站点 ← handler 调用链 → 窗口判据）==")
    print(f"   出处：AUDIT.md §BE-ER / WORK-PACKAGES.md §7-B；判据：逐指令反汇编（capstone）+ 源码调用链")
    print(f"   handler 可能返回 1 的站点 {len(rmap)} 个；已声明 `no_replay` 的站点 "
          f"{len(npr)} 个（{', '.join(sorted(npr)) or '—'}）")
    print()
    hdr = ("%-26s %-10s %-5s %-26s %-4s %-4s %s"
           % ("站点", "地址", "窗口", "窗内致命分支", "回放", "禁令", "结论"))
    print(hdr)
    print("-" * len(hdr))
    allnames = sorted(set(rmap) | set(npr))
    n_bad = 0
    for name in allnames:
        it = info.get(name)
        nprs = npr.get(name)
        ev = rmap.get(name)
        have_fn = f"BP_{name}" in scan["funcs"]
        if it is None:
            why = (DANGER_W3[name] if name in DANGER_W3 else
                   "⚠ sites.py 里找不到这个站点（handler 名对不上？）")
            print("%-26s %-10s %-5s %-26s %-4s %-4s %s"
                  % (name, "?", "?", "?", "是" if ev else "—", "是" if nprs else "—", why))
            continue
        va, cs = it["va"], it["cs"]
        insns = exe.disasm(va, cs) if cs else None
        fatal = rel32_branches(va, insns, exe.read(va, cs))[0] if insns else None
        if fatal:
            fs = ",".join(b.split("@")[0] for b in fatal)
        elif fatal is None:
            fs = "（判不了）"
        else:
            fs = "无"
        is_test = it.get("source") == "test"
        if fatal and nprs:
            verdict = "✅ 合法（禁令已声明：只许恒返 0 + 自己给落点）"
        elif fatal and ev and is_test:
            verdict = "⚠ 故意（`patch-test` 复现实验：就是要回放这条分支验算术，AUDIT §BE-DW；不判红）"
        elif fatal and ev:
            verdict = "✗ 回放会把那条分支跳飞（§BE-ER 的崩法）"
            n_bad += 1
        elif fatal and not ev:
            verdict = "✅ 窗口含分支，但 handler 不回放（观测者/恒 0）—— 仍须声明 `no_replay` 才能进发布面"
        elif fatal is None:
            verdict = "⚠ 判不了（装 capstone 后重跑）"
        elif not have_fn:
            verdict = f"⚠ 源码里找不到 `BP_{name}`（handler 可能写在别处）⇒ W6 对它是盲的"
        elif ev:
            verdict = "✅ 回放安全（窗内无分支 ⇒ 回放 = 原地执行）"
        else:
            verdict = "· handler 不返回 1"
        live = "" if it["live"] else "（park）"
        print("%-26s %-10s %-5s %-26s %-4s %-4s %s%s"
              % (name, hex(va), cs, fs, "是" if ev else "—", "是" if nprs else "—", verdict, live))
        if ev:
            print("%-26s %s" % ("", "回放证据：" + "、".join(ev)))
    print()
    print(f"合计：{len(allnames)} 个站点；**违规 {n_bad} 个**（含分支 + handler 回放 + 未声明禁令）")
    if scan["orphans"]:
        print("另有 `BP_EXEC_ORIGINAL` 没有 `BP_*` handler 到得了（漏删/写错名）：")
        for o in scan["orphans"]:
            print("  ⚠ " + o)
    return 0 if n_bad == 0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("addr", nargs="?", help="站点虚拟地址，如 0x45e930")
    ap.add_argument("cavesize", nargs="?", type=int, help="窗口字节数（thcrap 的 cavesize）")
    ap.add_argument("--ret", help="落点：地址（0x…）、`caller`（跳过整个函数）")
    ap.add_argument("--all", action="store_true", help="回归：站点全表（含 park）+ sdk.c 扫出的落点")
    ap.add_argument("--replay-census", action="store_true",
                    help="普查：每个「handler 可能返回 BP_EXEC_ORIGINAL」的站点 ← 它自己窗口的判据")
    ap.add_argument("--selftest", action="store_true",
                    help="只跑 W5/W6 判据自身的自检（离线，不需要样本 exe）")
    ap.add_argument("-v", "--verbose", action="store_true", help="--all 时连通过的也逐条打")
    ap.add_argument("--exe", default=str(EXE))
    a = ap.parse_args()

    exe_path = Path(a.exe)
    if a.selftest:                                 # 离线自检：不需要样本 exe
        n, fails = selftest()
        if not n and not fails:
            print("⚠ 没装 capstone ⇒ 判据自检跳过（rc=0）")
            return 0
        print("\n" + (f"✅ 判据自检通过（{n} 例）" if not fails else f"✗ 判据自检 {len(fails)} 处未过"))
        return 0 if not fails else 1
    if not exe_path.exists():
        print(f"⚠ 找不到样本 exe：{exe_path}（本工具只读真 exe；没有样本就跳过，退出码 0）")
        return 0
    exe = Exe(exe_path)
    if Cs is None:
        print("⚠ 没装 capstone（`pip install capstone`）⇒ 只能做重叠检查")

    if a.all:
        return run_all(exe, a.verbose)
    if a.replay_census:
        return replay_census(exe)
    if not a.addr or not a.cavesize:
        ap.error("要么给 `--all` / `--replay-census`，要么给 `地址 窗口字节数 [--ret …]`")
    va = int(a.addr, 16)
    bps, bhs = collect_items()
    # 按地址反查站点名（回放禁令声明、以及 W4 的「同址重复声明」都靠它）
    info = site_info(bps=bps)
    name = next((n for _src, n, v, _cs in bps if v == va), None) \
        or next((n for n, it in info.items() if it["va"] == va), None)
    if name:
        print(f"（站点名：{name}）")
    ctx = {"no_replay": no_replay_decls().get(name), "replay": replay_map().get(name),
           "live": name in {n for _s, n, _v, _c in bps},
           "source": (info.get(name) or {}).get("source")}
    ok = check_window(exe, va, a.cavesize, bps, bhs, name, ctx)
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
