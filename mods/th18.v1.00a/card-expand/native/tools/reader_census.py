#!/usr/bin/env python3
"""reader_census.py —— 「读者普查」的可复现实现（**只读 exe**）。

    python3 native/tools/reader_census.py            # 普查 + 与登记表对账（无样本时自动跳过）
    python3 native/tools/reader_census.py --all      # 连「锚点引用的全部读者」一起打出来

★ **它不在 `make check` 里**（`check` 那条链要能在没有样本的新 clone 上跑；`capcheck.py` 才是
  `make check` 里那道容量闸）。本条普查要读 exe，所以按 `CAPACITY.md` §4 的配方**手动跑**：
  每条「长大」落地前跑一次、落地后再跑一次对账。没有样本/没装 capstone 时它自己跳过并 exit 0。

## 它解决什么

本 mod 的失败模式不是「数组满了」，而是「**有一个读者我们没 patch**」（`AUDIT.md` §CC / §AH）。
人肉找读者一定会漏——`NEXT.md` §1w 最早只列了 6 个站点（加载器 + 5 个读者），字节级普查是
**13 个读者 + 1 个加载器 = 14 处引用**；§1w 已按这份普查改正（2026-09-14）。
所以做法固定成四步（`CAPACITY.md` §4）：找锚点 → 扫全部引用 → 逐个分类 → 每个读者必须
「已 patch」或「可证不受影响」。本脚本是第 ① ② 步的机器实现 + 第 ③ 步的登记表（`READERS`）
与 §1w 计划的镜像（`PATCH_PLANNED`）。

## 方法（为什么不是「整段线性反汇编」）

x86 线性扫会**跑飞**：`0x45ebfb` 的 `mov edi,[edi+edx*4+0xe0]` 在整段线性扫里就漏掉了。
所以反过来做：

  ① 找**锚点**：谁引用了「sht 影像指针」这个字段（`zPlayer+0x47940`，`CE_PLAYER_SHT_PTR`）；
  ② 在锚点之后的窗口里**跟着寄存器**找 `+0xe0`：加载器把数组每一项就地重定位成绝对指针，
     所以「拿到 sht 指针、再按 `set*4+0xe0` 取」就是这条数组的读者，一个不漏；
  ③ 候选编码用**字节模式** `E0 00 00 00` 找，再往前 1..7 字节逐个试反汇编
     （必须试满：`84 b2 e0 00 00 00`（test）与 `8b 84 b2 e0 00 00 00`（mov）会撞同一个 disp32）；
  ④ 加载器形态不同（`0xe0` 放在**索引**寄存器里 + 就地重定位），单独按**字节签名**钉住。

## 不判红的情形

* 没有样本（`local/th18.v1.00a/th18.exe`，gitignored）或没装 `capstone` ⇒ 打印一行「跳过」并 **exit 0**
  —— 别让缺样本把「跑不了普查」伪装成「普查通过」；
* 某条登记过的读者**不见了** ⇒ 只提示（引擎换了/样本换了才会发生）；
* 出现**新的读者**（不在登记表里）⇒ **判红**。这条就是「漏一个读者」的那道闸。

## ★ 2026-09-14：扩容**已落地**，机制与 §1w 最早那版不同（`AUDIT.md` §CQ）

落地走的是**「数组原地长到 96 格、数据区跟着后移 0xE0」**，不是「数组搬文件尾」：

* **13 个读者的位移 `+0xe0` 一个字节都不改**（数组起点没动）⇒ 本文件原来的那面「§1w 计划镜像」
  （`PATCH_PLANNED`）就失效了（它永远全 ✅，等于没查）。所以换成两道**仍然会红**的闸：
  - **闸 A（读者不许被改指）**：patch 里凡落在登记读者 / 加载器地址上的 binhack，其新位移必须
    **还是 `0xe0`**（加载器的 `mov esi,imm32` 同理）。谁把某条读者改指到别处，就和「影像里数组
    仍在 `+0xe0`」直接矛盾 ⇒ 判红；
  - **闸 B（数据区常量只许有一处）**：数据区基点 `0x180`（`th18:0x45a830` 的 `lea eax,[edx+imm32]`）
    是**唯一**会被本方案改的常量。全 exe 里 `0x180` 的 disp32/imm32 命中逐条登记，
    出现**没登记过的**就判红 —— 那意味着还有第二处也假设「数据区在 +0x180」。

> 教训（保留）：**「加载时会就地重定位」这条理由不能豁免任何读者**（`CAPACITY.md` §4）。
> 反过来，本方案之所以敢不改读者，是因为**数组起点本身没搬** —— 这两句话不矛盾：
> 前者说「读者取指针的位置是写死的、搬数组必须跟着改」，后者说「我们不搬数组，只搬数据区」。
"""
import argparse
import hashlib
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent            # native/tools
MOD = HERE.parent.parent                          # card-expand/
REPO = MOD.parents[2]                             # renkolab/
EXE = REPO / "local" / "th18.v1.00a" / "th18.exe"
SITES_PY = HERE.parent / "sites.py"

# ── 锚点：数组在哪（`native/engine.h` 的 `CE_PLAYER_SHT_PTR`）──────────────
ANCHOR_DISP = 0x47940          # zPlayer.sht_file*（`0x45a801` 写、21 处读）
ARRAY_DISP = 0xE0              # 组偏移数组基址（影像内）；界 = 影像头 `+0x02` 的 u16
SLOTS_RESERVED = 256           # ★ 2026-09-14 扩容：数组区**预留**格数（`AUDIT.md` §CQ）；零售 40、宣布可用 96

# ── 登记表（2026-09-13 一手普查；出处 `CAPACITY.md` §4 的表）───────────────
#   键 = 读者地址；值 = 它是谁 / 干什么。**新的读者不在这张表里就判红。**
READERS = {
    0x40A9F7: "装备卡子机路径取组（`Player__tick_shooters_for_ability_card`）",
    0x40C335: "卡侧按 set 取行（索引来自运行期对象字段 `arg+0xac`）",
    0x40C464: "卡侧按 set 取行（同上，另一条支路）",
    0x411047: "当前主炮组（`0x411047` 一带）",
    0x45E347: "`PlayerBullet__create` `0x45e320` 序言取行",
    0x45E94D: "主炮开火路径取行（`0x45e930` 一带）",
    0x45EBFB: "`Player__tick_shooting_state` 主炮取组（★ 已挂 `BP_ce_shot_set_index`）",
    0x45EE16: "开火路径取行（`0x45ede0` 一带）",
    0x45FCBE: "命中/伤害路径取行（`0x45fca0` 一带）",
    0x46035B: "取组（`0x460300` 一带；索引 = 打包的 `set<<8|row`）",
    0x4609F4: "取组（`0x4609c0` 一带）",
    0x460BA8: "取组（`0x460b60` 一带）",
    0x460CDD: "取组（`0x460cb0` 一带）",
    # ★ 2026-09-14 补（折叠位移形态，见下面扫描的形态 ⓑ）：`0x138 = 0xe0 + 0x16*4`
    0x410609: "取组（**折叠位移** `[eax+0x138]` = set `0x16`；逐字节复核 2026-09-14）",
}
# ── 加载器：形态不同，按签名钉住（改了这四条就说明引擎变了/已被 patch）─────
#   ⚠ 2026-09-13 修：`reloc` 原来写的是 `0186`（= `add [esi+disp32],eax`）—— 与本机 exe 对不上，
#     于是**每次运行**都报「加载器签名变了」并 exit 1 ⇒ 这份普查永远出不了结果。
#     实测字节是 `010416`：`01 /r` + ModRM `04`（mod=00, reg=eax, rm=SIB）+ SIB `16`
#     （scale=1, index=edx, base=esi）= `add dword [esi+edx], eax`，正是下面注释写的那条。
#     签名的对照物是**真实 exe**：对不上就先反汇编 `0x45a836`，别照抄这里的旧结论。
LOADER = {
    "va": 0x45A81D,
    "bytes": "bee0000000",              # mov esi, 0xe0
    "reloc": (0x45A836, "010416"),      # add dword [esi+edx], eax  —— 就地重定位
    "step": (0x45A897, "83c604"),       # add esi, 4
    "bound": (0x45A892, "0fb74202"),    # movzx eax, word [edx+2]  —— 循环上界 = 影像头 +0x02
}
# 签名对不上时逐条报「哪一条」，别只说「有一条对不上」（旧版就是这么说，等于没说）
LOADER_LABEL = {
    "bytes": "`0x45a81d` `mov esi,0xe0`（数组基址）",
    "reloc": "`0x45a836` `add [esi+edx],eax`（就地重定位）",
    "step": "`0x45a897` `add esi,4`（下一格）",
    "bound": "`0x45a892` `movzx eax,word[edx+2]`（循环上界 = 影像头 `+0x02`）",
}
# ── 闸 A 的登记表：patch 里**不许**把读者改指到别处（2026-09-14 换掉旧的 §1w 计划镜像）────
#   ★ 为什么删掉 `PATCH_PLANNED`：那是「数组搬文件尾」方案（要改 14 处）的镜像。落地方案是
#     **数组原地长大**（`AUDIT.md` §CQ）⇒ 14 处的位移一个都不改，那张表会永远全 ✅（= 没查）。
#     换成两道仍会红的闸：闸 A = 读者/加载器的位移不许被 patch 改掉（下面 `check_reader_patch`），
#     闸 B = 数据区常量 `0x180` 在全 exe 只许有一处与 sht 相关（上面 `DBASE_SITES`）。
#   ⚠ 谁要改这些读者，必须先证明「改完两边仍然自洽」：数组起点 `0xe0`、13 处位移、影像头计数
#     三者是一组，动一处就要同步 `sht_ids.h` 与 `CAPACITY.md` §4 的表。
READER_DISP_EXPECT = "e0000000"      # 13 个读者的 disp32（`[base + idx*4 + 0xe0]`）
LOADER_IMM_EXPECT = "e0000000"       # 加载器 `0x45a81d` 的 `mov esi,0xe0`
PATCH_JS = MOD / "patch" / "th18.v1.00a.js"

# ── 闸 B 的登记表：数据区基点常量 `0x180` 在全 exe 的落点（2026-09-14 字节级普查）────
#   判据：`capstone` 逐条反汇编后 **memory disp 或最后那个 imm32 == 0x180**。
#   ★ 只有 `0x45A830` 与 pl0X.sht 有关（它的基址寄存器是 sht 影像 = `[edx+0x180]`，
#     `edx` 来自 `player+0x47940`）；其余 5 处的基址寄存器都不是 sht 影像（逐条反汇编确认过）。
#   谁发现**没登记过的**命中 ⇒ 判红：可能还有第二处也假设「数据区在 +0x180」。
DBASE_DISP = 0x180
DBASE_SITES = {
    0x45A830: "★ **唯一的那个**：`Player__initialize` 的解析循环 `lea eax,[edx+0x180]`"
              "（`edx` = sht 影像，重定位加数）⇒ 扩容把它换成 `[edx+0x4e0]`（`native/sites.py`）",
}
DBASE_KNOWN = {
    0x41B7FC: "`mov [ecx+0x180],eax` —— 基址不是 sht 影像（与 pl0X 无关）",
    0x429988: "`addps xmm3,[edi+0x180]` —— 同上",
    0x4299A5: "`adc [edi+0x180],ebx` —— 同上",
    0x43E2C0: "`movq [esi+0x180],xmm0` —— 同上",
    0x4855F3: "`adc [eax+0x180],eax` —— 同上",
}


# ══════════════════════════════════════════════════════════════════════════
def pe_text(path: Path):
    d = path.read_bytes()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    if d[pe:pe + 4] != b"PE\0\0":
        raise SystemExit(f"{path} 不是 PE")
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    opt = pe + 24
    base = struct.unpack_from("<I", d, opt + 28)[0]
    secoff = opt + struct.unpack_from("<H", d, pe + 20)[0]
    secs = []
    for i in range(nsec):
        o = secoff + 40 * i
        name = d[o:o + 8].rstrip(b"\0").decode(errors="replace")
        vsz, va, rsz, ra = struct.unpack_from("<IIII", d, o + 8)
        secs.append((name, base + va, vsz, ra, rsz))
    for name, sva, vsz, ra, rsz in secs:
        if name == ".text":
            return d[ra:ra + rsz], sva
    raise SystemExit("找不到 .text")


def exe_md5():
    m = re.search(r'EXE_MD5\s*=\s*"([0-9a-f]{32})"', SITES_PY.read_text(encoding="utf-8"))
    return m.group(1) if m else None


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--all", action="store_true", help="连锚点的全部引用一起打出来")
    a = ap.parse_args()

    if not EXE.is_file():
        print(f"reader_census：跳过（没有样本 {EXE.relative_to(REPO)} —— 它是 gitignored 的，"
              f"`python3 tooling/thtk/unpack.py th18.v1.00a` 之后才有）")
        return 0
    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
        from capstone.x86 import X86_OP_MEM, X86_OP_IMM, X86_OP_REG, X86_REG_INVALID
    except ImportError:
        print("reader_census：跳过（没装 capstone；`pip install capstone` 之后才有这份普查）")
        return 0

    want = exe_md5()
    got = hashlib.md5(EXE.read_bytes()).hexdigest()
    if want and got != want:
        print(f"reader_census：跳过（exe md5 {got} ≠ `sites.py` 钉的 {want} —— 不是同一个 build）")
        return 0

    code, tva = pe_text(EXE)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True

    def one(buf, va):
        for ins in md.disasm(buf, va):
            return ins
        return None

    def find_disp(disp, scale=None):
        """字节模式找 disp32 → 往前 1..7 字节逐个试反汇编（必须试满，见文件头 ③）。"""
        n = struct.pack("<I", disp)
        out, p = [], code.find(n)
        while p >= 0:
            for back in range(1, 8):
                s = p - back
                if s < 0:
                    continue
                ins = one(code[s:s + 16], tva + s)
                if ins is None or ins.size != back + 4:
                    continue
                for op in ins.operands:
                    if op.type != X86_OP_MEM or op.mem.disp != disp:
                        continue
                    if scale is not None and op.mem.scale != scale:
                        continue
                    if scale is not None and op.mem.index == X86_REG_INVALID:
                        continue
                    out.append(ins)
                    break
            p = code.find(n, p + 1)
        return sorted({i.address: i for i in out}.values(), key=lambda x: x.address)

    # ── ① 锚点：谁引用了 zPlayer+0x47940 ──────────────────────────────────
    anchors = find_disp(ANCHOR_DISP)
    if a.all:
        print(f"锚点 `zPlayer+{ANCHOR_DISP:#x}`（`CE_PLAYER_SHT_PTR`）的全部引用（{len(anchors)} 处）：")
        for i in anchors:
            print(f"  {i.address:#08x}  {i.mnemonic:<6} {i.op_str}")
        print()

    # ── ② 跟着寄存器找 +0xe0 的读者 ──────────────────────────────────────
    def holds(ins):
        """→ (写入的目标寄存器, 读到的寄存器列表)。只认 32 位通用寄存器。"""
        dst, srcs = None, []
        if ins.operands and ins.operands[0].type == X86_OP_REG:
            dst = ins.operands[0].reg
        for op in ins.operands[1:]:
            if op.type == X86_OP_REG:
                srcs.append(op.reg)
        return dst, srcs

    readers = {}
    for anc in anchors:
        dst, _ = holds(anc)
        if dst is None:
            continue
        regs = {dst}
        for step, ins in enumerate(md.disasm(code[anc.address - tva + anc.size:][:400],
                                             anc.address + anc.size)):
            if step > 60 or ins.mnemonic in ("ret", "retn", "int3", "jmp"):
                break
            for op in ins.operands:
                if op.type != X86_OP_MEM or op.mem.base not in regs:
                    continue
                # 形态 ⓐ：变址取组 `[base + idx*4 + 0xe0]`
                if op.mem.disp == ARRAY_DISP and op.mem.scale == 4 and op.mem.index != X86_REG_INVALID:
                    readers[ins.address] = (anc.address, ins.mnemonic + " " + ins.op_str)
                # ★ 形态 ⓑ：**折叠位移**（常量 set 号被编译进 disp）：
                #   `0x410609 mov edi,[eax+0x138]` 里 `0x138 = 0xe0 + 0x16*4` —— 它读的就是
                #   set 0x16，紧接着是 `cmp byte [edi],0 / jl` + `lea edi,[edi+0x5c]` 的行游标
                #   惯例（随后 `call 0x45e930`）。**2026-09-14 由子代理查出来、父代理逐字节复核**：
                #   原普查只有形态 ⓐ ⇒ 少算这一处（`CAPACITY.md` §4 的表已随之从 14 改 15）。
                #   这条也是「数组搬文件尾」那条路线的致命漏项（漏一个读者 = 静默错位）；
                #   现行走的「数组原地长到 96 格」路线里 disp 不变 ⇒ 对该形态天然免疫。
                elif op.mem.scale == 1 and op.mem.index == X86_REG_INVALID \
                        and ARRAY_DISP <= op.mem.disp < ARRAY_DISP + SLOTS_RESERVED * 4 \
                        and (op.mem.disp - ARRAY_DISP) % 4 == 0:
                    readers[ins.address] = (anc.address, ins.mnemonic + " " + ins.op_str)
            d, s = holds(ins)
            if d is not None:
                regs = {d} | (regs & set(s)) if (regs & set(s)) else (regs - {d})
                if any(x in regs for x in s):
                    regs.add(d)

    # ── ③ 加载器（形态不同，按签名钉）────────────────────────────────────
    def sig(va, hexs):
        return code[va - tva:va - tva + len(hexs) // 2].hex() == hexs

    loader_ok = all([sig(LOADER["va"], LOADER["bytes"]),
                     sig(*LOADER["reloc"]), sig(*LOADER["step"]), sig(*LOADER["bound"])])
    if not loader_ok:
        miss = []
        if not sig(LOADER["va"], LOADER["bytes"]):
            miss.append(LOADER_LABEL["bytes"])
        for k in ("reloc", "step", "bound"):
            if not sig(*LOADER[k]):
                miss.append(LOADER_LABEL[k])
        print("reader_census：**红** —— 加载器 `0x45a81d` 的签名变了，对不上的是：")
        for one in miss:
            print(f"  · {one}")
        print("  要么引擎/样本换了，要么这里已经被 patch 改过 ⇒ 必须重新做一次读者普查，"
              "别照抄旧结论。\n  （改 `LOADER` 之前先把那几条地址反汇编出来，以真实 exe 为准。）")
        return 1

    # ── ④ 对账 ──────────────────────────────────────────────────────────
    print(f"reader_census —— shooterset 组数组（影像 `+{ARRAY_DISP:#x}`，界 = 影像头 `+0x02` 的 u16；"
          f"**2026-09-14 起产物是 96 格**，`AUDIT.md` §CQ）")
    print(f"  样本 {EXE.relative_to(REPO)}（md5 {got}）  `.text` {len(code):,} B\n")
    print(f"{'地址':<12}{'类型':<8}{'说明'}")
    print("-" * 100)
    for va in sorted(readers):
        print(f"{hex(va):<12}{'读者':<8}{READERS.get(va, '★ 未登记！')}")
    print(f"{hex(LOADER['va']):<12}{'加载器':<8}"
          f"就地重定位 + 建行（循环上界 = 影像头 `+0x02`）")
    print(f"\n合计 **{len(readers) + 1}** 处引用 = {len(readers)} 个读者 + 1 个加载器；"
          f"**这些位移在扩容后一个都没改**（数组起点仍是 `+{ARRAY_DISP:#x}`）")

    bad = []
    new = sorted(set(readers) - set(READERS))
    gone = sorted(set(READERS) - set(readers))
    if new:
        bad.append(f"出现**新的读者**（不在登记表里）：{[hex(v) for v in new]} —— "
                   f"它们读的是 `+{ARRAY_DISP:#x}` 那一段，先搞清它读哪一格再登记")
    if gone:
        print(f"\n（登记过的 {[hex(v) for v in gone]} 这次没扫到 —— 引擎/样本换了才会发生；"
              f"别照抄旧结论，先把这几条反汇编出来。）")

    # ── 闸 A：读者 / 加载器的位移不许被 patch 改掉 ────────────────────────
    print("\n闸 A（读者位移不动）：")
    if not PATCH_JS.is_file():
        print(f"  · 跳过（没有 {PATCH_JS.relative_to(MOD)}）")
    else:
        import json
        doc = json.loads(PATCH_JS.read_text(encoding="utf-8"))
        hits = {0x400000 + int(v["addr"], 16) if v["addr"].startswith("0x") else 0: (k, v)
                for k, v in doc.get("binhacks", {}).items()}
        watch = dict(readers)
        watch[LOADER["va"]] = "加载器"
        n_ok = 0
        for va, who in sorted(watch.items()):
            entry = hits.get(va)
            if entry is None:
                n_ok += 1
                continue
            name, bh = entry
            c = bh.get("code", "")
            if "<" in c or "[" in c:
                print(f"  · ⚠ {hex(va)}（{who}）被 binhack `{name}` 覆盖，且 code 带表达式 ⇒ "
                      f"无法静态核对位移，**人工过目**：{c}")
                continue
            want = LOADER_IMM_EXPECT if va == LOADER["va"] else READER_DISP_EXPECT
            if want in c:
                n_ok += 1
                print(f"  · ✅ {hex(va)}（{who}）被 patch 改过，但位移仍是 `+{ARRAY_DISP:#x}`（自洽）")
            else:
                bad.append(f"{hex(va)}（{who}）被 binhack `{name}` 改指到别处：code = {c} —— "
                           f"影像里的数组仍在 `+{ARRAY_DISP:#x}`，两边已经矛盾")
        print(f"  {n_ok}/{len(watch)} 处的位移保持 `+{ARRAY_DISP:#x}`（没被 patch 改 ⇒ 天然自洽）")

    # ── 闸 B：数据区基点常量 `0x180` 的落点必须全是登记过的 ───────────────
    #   ⚠ 不能用上面的 `find_disp()`：它为了不放过读者，会把**同一个 disp 的多种起始对齐**
    #     全收进来（正确读者需要这种宽容），于是每个真站点旁边会带一串「错位重解幽灵」
    #     （例：`0f 58 9f 80 01 00 00` 在 0x429988，而 0x429987 起的重解也算「命中」）。
    #     闸 B 关心的是「这条指令的最后一个字段是不是 0x180」，所以这里取**最近起始**的唯一解。
    def const_field_hits(value):
        """→ {指令地址: 指令}：`value` 作为 disp32（带基址）或最后那个 imm32 出现的**规范解**。"""
        pat = struct.pack("<I", value)
        out, p = {}, code.find(pat)
        while p >= 0:
            for back in range(1, 8):
                s = p - back
                if s < 0:
                    continue
                ins = one(code[s:s + 16], tva + s)
                if ins is None or ins.size != back + 4 or not ins.operands:
                    continue
                # ⚠ 操作数顺序 != 编码顺序：`mov [ecx+0x180],eax` 的 disp32 是**最后编码**的字段，
                #   但操作数表里最后一个是 `eax` ⇒ 不能只看 operands[-1]，要逐个看。
                disp_hit = any(op.type == X86_OP_MEM and op.mem.disp == value for op in ins.operands)
                imm_hit = (ins.operands[-1].type == X86_OP_IMM and ins.operands[-1].imm == value)
                if not (disp_hit or imm_hit):
                    continue
                out[ins.address] = ins
                break
            p = code.find(pat, p + 1)
        return out

    print(f"\n闸 B（数据区基点常量 `+{DBASE_DISP:#x}` 的落点，全 `.text` 扫）：")
    all_hits = const_field_hits(DBASE_DISP)
    #   ★ 只判「能承载基点」的形态：**基址寄存器 + disp32**（`[reg+0x180]`，读或写都算），
    #     以及 `add/sub/mov/or/xor reg, 0x180` 这类寄存器算术。
    #     纯立即数形态（`and eax,0x180` / `test [mem],0x180` / `push 0x180` / `mov [abs],0x180`）
    #     不可能把基点加到 sht 影像指针上 —— 它们只列数，不进判定。
    def carries_base(ins):
        for op in ins.operands:
            if op.type == X86_OP_MEM and op.mem.base != X86_REG_INVALID and op.mem.disp == DBASE_DISP:
                return True
        return (ins.mnemonic in ("add", "sub", "mov", "or", "xor")
                and ins.operands and ins.operands[0].type == X86_OP_REG
                and ins.operands[-1].type == X86_OP_IMM and ins.operands[-1].imm == DBASE_DISP)

    judged = {va: ins for va, ins in all_hits.items() if carries_base(ins)}
    ref = {va: ins for va, ins in all_hits.items() if va not in judged}
    for va in sorted(judged):
        ins = judged[va]
        if va in DBASE_SITES:
            tag, note = "★ patch 改这里", DBASE_SITES[va]
        elif va in DBASE_KNOWN:
            tag, note = "已登记（无关）", DBASE_KNOWN[va]
        else:
            tag, note = "❌ **没登记**", "—— 新出现的 `[reg+0x180]` 落点"
        print(f"  {hex(va):<12}{tag:<16}{ins.mnemonic} {ins.op_str}   {note}")
    if ref:
        kinds = {}
        for ins in ref.values():
            kinds[ins.mnemonic] = kinds.get(ins.mnemonic, 0) + 1
        print(f"  （另有 {len(ref)} 处纯立即数形态，不进判定："
              + "、".join(f"{k} {v}" for k, v in sorted(kinds.items())) + "）")
    stray_dbase = [va for va in judged if va not in DBASE_SITES and va not in DBASE_KNOWN]
    if stray_dbase:
        bad.append(f"数据区常量 `{DBASE_DISP:#x}` 出现了没登记的 `[reg+0x180]` 落点 "
                   f"{[hex(v) for v in stray_dbase]} —— "
                   f"先确认它是不是也假设「数据区在 +{DBASE_DISP:#x}」（是 ⇒ 扩容要一起改；"
                   f"不是 ⇒ 加进 `DBASE_KNOWN` 并写明理由）")
    if DBASE_SITES and not any(va in judged for va in DBASE_SITES):
        bad.append(f"登记的那处 `{DBASE_DISP:#x}` 站点不见了（{[hex(v) for v in DBASE_SITES]}）—— "
                   f"要么已经被 patch 改成新基点（以 `sites.py` 的 `SHT_DBASE_EXPECT` 为准则正确），"
                   f"要么引擎换了。**这条要人工判一次**，别自动放行。")

    if bad:
        print()
        print("reader_census：**红**")
        for b in bad:
            print(f"  · {b}")
        return 1
    print("\nreader_census：普查与登记表一致 ✅")
    return 0


if __name__ == "__main__":
    sys.exit(main())
