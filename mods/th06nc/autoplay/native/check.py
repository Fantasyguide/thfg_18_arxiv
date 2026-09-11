#!/usr/bin/env python3
"""autoplay 的机器核对：地址与 spec 模块对账、序言字节、跳板机器码、栈对齐、代理导出、宿主单测。

`AUDIT.md` 里每一条 [x] 都由这里的某段支撑。改了 hook.c / th06nc.h 就重跑这个。
照 mouse-control/native/check.py 改：C–F 节逐字相同（跳板与代理没变），B 节换成本 mod 的地址集。
"""
import json, re, struct, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[4]
EXE = REPO / "local/th06nc/th06nc.exe"
HDR = Path(__file__).resolve().parent / "th06nc.h"
SRC = Path(__file__).resolve().parent / "hook.c"
EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

fails = []
def ok(cond, msg):
    print(("  ok   " if cond else "  FAIL ") + msg)
    if not cond: fails.append(msg)

def defines():
    """按出现顺序求值，允许 `#define A (B | C)` 这类引用先前宏的表达式。"""
    txt = HDR.read_text(encoding="utf-8")
    out = {}
    for m in re.finditer(r"^#define\s+(\w+)\s+([^/\n]+)", txt, re.M):
        name, body = m.group(1), m.group(2).strip()
        if not re.fullmatch(r"[\w\s()|+\-*x0-9A-Fa-f]+", body):
            continue
        try:
            out[name] = eval(body, {"__builtins__": {}}, dict(out))
        except Exception:
            pass
    return out

def sections(d):
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    n = struct.unpack_from("<H", d, pe + 6)[0]
    osz = struct.unpack_from("<H", d, pe + 20)[0]
    ib = struct.unpack_from("<Q", d, pe + 24 + 24)[0]
    s = [struct.unpack_from("<IIII", d, pe + 24 + osz + i * 40 + 8) for i in range(n)]
    return ib, s

def main():
    D = defines()
    print("== A. 样本与地址 ==")
    exe = got = None
    if not EXE.is_file():
        print("  skip 样本不在（gitignored），只做静态检查")
    else:
        import hashlib
        exe = d = EXE.read_bytes()
        ok(hashlib.md5(d).hexdigest() == EXPECTED_MD5, "exe MD5 与记录一致")
        ib, secs = sections(d)
        def v2f(va):
            rva = va - ib
            for vs, sva, rs, ptr in secs:
                if sva <= rva < sva + max(vs, rs): return ptr + (rva - sva)
        # B 节：expected 原字节逐字节核对
        want = re.search(r"k_expected_prologue\[PROLOGUE_LEN\]\s*=\s*\{([^}]*)\}",
                         HDR.read_text(encoding="utf-8")).group(1)
        want = bytes(int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+", want))
        got = d[v2f(ib + D["RVA_PLAYER_ONUPDATE"]):][:len(want)]
        ok(got == want, "Player__OnUpdate 序言 13 字节与 header 逐字节相同")
        ok(len(want) == D["PROLOGUE_LEN"], "PROLOGUE_LEN 与字节数组长度一致")

    print("== B. 地址与仓库文档对账 ==")
    sym = json.loads((REPO / "games/th06nc/symbols.json").read_text(encoding="utf-8"))
    byname = {r["name"]: int(r["addr"], 16) for r in sym["funcs"]}
    sbyname = {r["name"]: int(r["addr"], 16) for r in sym.get("statics", []) + sym.get("labels", [])}
    IB = 0x140000000
    ok(byname.get("Player__OnUpdate") == IB + D["RVA_PLAYER_ONUPDATE"],
       "RVA_PLAYER_ONUPDATE 指向 symbols.json 里的 Player__OnUpdate")
    for name, key in (("g_Bullets", "RVA_G_BULLETS"), ("g_BulletCount", "RVA_G_BULLET_COUNT"),
                      ("g_Enemies", "RVA_G_ENEMIES"), ("g_Score", "RVA_SCORE"),
                      ("g_GrazeInStage", "RVA_GRAZE_IN_STAGE"), ("g_BombInUse", "RVA_BOMB_IN_USE")):
        ok(sbyname.get(name) == IB + D[key], f"{key} = symbols.json 的 {name}")
    sys.path.insert(0, str(REPO / "tooling/ghidra"))
    import th06nc_player_spec as ps, th06nc_bullet_spec as bs, th06nc_enemy_spec as es, th06nc_ecl_spec as cs
    pf = {n: o for o, n, _s, _e in ps.PLAYER_FIELDS}
    ok(IB + D["RVA_G_PLAYER"] == ps.G_PLAYER, "RVA_G_PLAYER = player_spec.G_PLAYER")
    ok(D["OFF_POSITION_X"] == pf["positionCenter"], "OFF_POSITION_X = positionCenter")
    ok(D["OFF_HITBOX_RADIUS"] == pf["hitboxRadius"], "OFF_HITBOX_RADIUS = hitboxRadius")
    ok(D["OFF_IS_FOCUSED"] == pf["isFocused"], "OFF_IS_FOCUSED = isFocused")
    ok(D["OFF_PLAYER_STATE"] == pf["playerState"], "OFF_PLAYER_STATE = playerState")
    ok(D["OFF_CHAR_DATA"] == pf["characterData_orthogonalMovementSpeed"], "OFF_CHAR_DATA = characterData")
    ok(IB + D["RVA_LIVES"] == ps.G_LIVES_REMAINING and IB + D["RVA_BOMBS"] == ps.G_BOMBS_REMAINING
       and IB + D["RVA_POWER"] == ps.G_CURRENT_POWER, "命 / 炸弹 / 火力全局 = player_spec")
    ok(IB + D["RVA_BULLET_MANAGER"] == bs.G_BULLET_MANAGER
       and D["RVA_G_BULLETS"] == D["RVA_BULLET_MANAGER"] + bs.BULLETS_OFFSET, "弹池 = BulletManager + BULLETS_OFFSET")
    ok(D["BULLET_SIZE"] == bs.BULLET_SIZE and D["BULLET_COUNT"] == bs.BULLET_COUNT, "弹步长 / 数量 = bullet_spec")
    bf = {n: o for o, n, _s, _e in bs.BULLET_FIELDS}
    for key, name in (("OFF_B_SPEED", "speed"), ("OFF_B_VELOCITY", "velocity.x"), ("OFF_B_POS", "pos.x"),
                      ("OFF_B_ANGLE", "angle"), ("OFF_B_STATE", "state"), ("OFF_B_GRAZE_SIZE", "grazeSize.x"),
                      ("OFF_B_IS_GRAZED", "isGrazed")):
        ok(D[key] == bf[name], f"{key} = bullet_spec 的 {name}")
    ok(IB + D["RVA_G_ENEMIES"] == es.ENEMY_MANAGER + es.ENEMIES_OFFSET, "敌池 = EnemyManager + ENEMIES_OFFSET")
    ok(D["ENEMY_SIZE"] == es.ENEMY_SIZE and D["ENEMY_COUNT"] == es.ENEMY_COUNT, "敌步长 / 数量 = enemy_spec")
    ok(D["OFF_E_FLAGS"] == es.ENEMY_SLOT_FLAGS, "OFF_E_FLAGS = ENEMY_SLOT_FLAGS（bit7 占用）")
    sw = {n: o for o, n, _s, _e in es.SPAWN_WRITES}
    ok(D["OFF_E_POS"] == sw["position"] and D["OFF_E_LIFE"] == sw["life"] and D["OFF_E_MAX_LIFE"] == sw["maxLife"],
       "敌 position / life / maxLife = enemy_spec.SPAWN_WRITES")
    ef = {n: (o, s) for o, n, s, _e in cs.ENEMY_FIELDS}
    ok(D["OFF_E_BOSS_ID"] == ef["bossId"][0] and D["OFF_E_HITBOX"] == ef["hitboxDimensions"][0],
       "敌 bossId / hitboxDimensions = ecl_spec.ENEMY_FIELDS")
    ok(ef["hitboxDimensions"][1] == "code", "hitboxDimensions 已是 code 档（player/01 §2.7 完整公式）")
    print("       注意：bossId 仍是 %s 档（弱锚），只影响次要目标" % ef["bossId"][1])
    ok(D["IN_SHOOT"] == 1 and D["IN_BOMB"] == 2 and D["IN_FOCUS"] == 4, "按键位与 spec 的 BUTTON_BITS 一致")
    ok(D["IN_ALL_MASK"] == 0xF7, "整体覆盖的掩码 = 7 个逻辑按钮位")

    print("== C. 跳板机器码：反汇编 hook.c 实际写入的字节 ==")
    src = SRC.read_text(encoding="utf-8")
    stub_size = int(re.search(r"#define STUB_SIZE\s+(0x[0-9a-fA-F]+)", src).group(1), 0)

    # 不看注释——直接解析 build_trampoline() 里对 s[] 的写入，按源码顺序重放。
    body = src.split("s = g_stub;", 1)[1].split("if (i != STUB_SIZE)", 1)[0]
    FN_MARK, BACK_MARK = 0xF17F17F17F17F100, 0xBACCBACCBACCBAC0
    stub = bytearray()
    for tok in re.finditer(
            r"s\[i\+\+\]\s*=\s*(0x[0-9a-fA-F]+)|memcpy\(\s*s\s*\+\s*i\s*,\s*&(\w+)\s*,\s*8\)"
            r"|memcpy\(\s*s\s*\+\s*i\s*,\s*(\w+)\s*,\s*PROLOGUE_LEN\s*\)", body):
        if tok.group(1):
            stub.append(int(tok.group(1), 0))
        elif tok.group(2):
            stub += struct.pack("<Q", FN_MARK if tok.group(2) == "fn" else BACK_MARK)
        else:
            stub += b"\x00" * D["PROLOGUE_LEN"]   # 原序言，运行期从 target 拷来
    ok(len(stub) == stub_size, f"解析出的写入共 {len(stub)} 字节 == STUB_SIZE {stub_size:#x}")

    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    # 原序言那 13 字节运行期才拷进来，这里用真样本的字节填上再反汇编
    filled = bytearray(stub)
    pro_at = len(stub) - D["PROLOGUE_LEN"] - 14
    if exe is not None:
        filled[pro_at:pro_at + D["PROLOGUE_LEN"]] = got
    # 末尾 8 字节是 jmp 的绝对目标地址（数据），不参与反汇编
    ins = list(md.disasm(bytes(filled[:-8]), 0))
    text = [f"{x.mnemonic} {x.op_str}".strip() for x in ins]
    for line in text:
        print("       " + line)

    DESIGN = [
        "push rcx", "push rdx", "push r8", "push r9", "push r10", "push r11",
        "sub rsp, 0x28",
        f"movabs rax, {FN_MARK:#x}", "call rax",
        "add rsp, 0x28",
        "pop r11", "pop r10", "pop r9", "pop r8", "pop rdx", "pop rcx",
    ]
    ok(text[:len(DESIGN)] == DESIGN, "前 16 条指令与设计表逐条相同（保存易失寄存器→对齐→调用→还原）")
    if exe is not None:
        ok(text[len(DESIGN):len(DESIGN) + 3] == ["mov rax, rsp", "push rdi", "push r12"],
           "紧接着是从样本拷来的原序言")
    ok(text[-1] == "jmp qword ptr [rip]", "最后一条是 jmp [rip+0]，不碰任何寄存器")
    ok(struct.unpack_from("<Q", filled, len(filled) - 8)[0] == BACK_MARK,
       "jmp 后跟的 8 字节是返回地址 target+13")
    ok(sum(len(x.bytes) for x in ins) == len(filled) - 8,
       "除末尾 8 字节地址外，写入的每一字节都落在某条完整指令里")

    if exe is not None:
        # 最关键的一条：被覆盖的 13 字节必须正好是若干条完整指令。
        # 若从某条指令中间劈开，拷进跳板的就是半条指令，必崩。
        pins = list(md.disasm(bytes(got), 0))
        ok(sum(len(x.bytes) for x in pins) == D["PROLOGUE_LEN"],
           "序言 13 字节 = %d 条完整指令，没有从指令中间截断"
           % len(pins))
        # 这 4 条会被原样搬到别处执行，因此**必须位置无关**：
        # 任何 RIP 相对寻址或相对跳转/调用搬走后都会指向错的地方。
        md.detail = True
        bad = []
        for x in md.disasm(bytes(got), 0x140000000 + D["RVA_PLAYER_ONUPDATE"]):
            if capstone.CS_GRP_JUMP in x.groups or capstone.CS_GRP_CALL in x.groups \
               or capstone.CS_GRP_BRANCH_RELATIVE in x.groups:
                bad.append("%s %s（相对分支）" % (x.mnemonic, x.op_str))
            for o in x.operands:
                if o.type == capstone.x86.X86_OP_MEM and o.mem.base == capstone.x86.X86_REG_RIP:
                    bad.append("%s %s（RIP 相对）" % (x.mnemonic, x.op_str))
        md.detail = False
        ok(not bad, "搬走的 4 条指令都位置无关" + ("：" + "; ".join(bad) if bad else ""))

    print("== D. 栈对齐（按上面反汇编出的指令算） ==")
    rsp = 8                       # call 进来时 rsp ≡ 8 (mod 16)
    pushes = sum(1 for t in text[:len(DESIGN)] if t.startswith("push"))
    rsp = (rsp - 8 * pushes) % 16
    ok(pushes == 6 and rsp == 8, f"{pushes} 次 push 后 rsp ≡ 8 (mod 16)")
    sub = int(re.search(r"sub rsp, (0x[0-9a-f]+)", " ".join(text)).group(1), 0)
    rsp = (rsp - sub) % 16
    ok(rsp == 0, f"sub rsp,{sub:#x} 后 rsp ≡ 0 (mod 16) —— x64 ABI 要求 call 前如此")
    ok(sub >= 0x20, f"{sub:#x} ≥ 32 字节 shadow space")
    ok(text.count(f"add rsp, {sub:#x}") == 1 and pushes == text[:len(DESIGN)].count("pop r11")
       + text[:len(DESIGN)].count("pop r10") + text[:len(DESIGN)].count("pop r9")
       + text[:len(DESIGN)].count("pop r8") + text[:len(DESIGN)].count("pop rdx")
       + text[:len(DESIGN)].count("pop rcx"), "push/pop 与 sub/add 一一配平，净栈位移为 0")
    order = [t.split()[1] for t in text[:len(DESIGN)] if t.startswith("push")]
    unorder = [t.split()[1] for t in text[:len(DESIGN)] if t.startswith("pop")]
    ok(order == unorder[::-1], "pop 顺序是 push 的逆序")
    ok("rax" not in order, "没保存 rax —— 原序言第一条 mov rax,rsp 会覆盖它，无需保存")

    print("== E. 入口补丁 ==")
    pbody = src.split("/* 入口补丁 */", 1)[1].split("VirtualProtect", 1)[0]
    patch = bytearray(b"\x90" * D["PROLOGUE_LEN"])   # memset 0x90
    ok("memset(patch, 0x90" in pbody, "补丁先用 0x90 填满，长度不足处即 nop")
    for m in re.finditer(r"patch\[(\d+)\]\s*=\s*(0x[0-9a-fA-F]+)", pbody):
        patch[int(m.group(1))] = int(m.group(2), 0)
    STUB_MARK = 0x57ABEEF57ABEEF00
    struct.pack_into("<Q", patch, 2, STUB_MARK)
    ptext = [f"{x.mnemonic} {x.op_str}".strip() for x in md.disasm(bytes(patch), 0)]
    for line in ptext:
        print("       " + line)
    ok(ptext == [f"movabs rax, {STUB_MARK:#x}", "jmp rax", "nop"],
       "入口补丁 = movabs rax,stub / jmp rax / nop，正好 13 字节")
    ok(len(patch) == D["PROLOGUE_LEN"], "补丁长度 == 被覆盖的序言长度，不多写一字节")

    print("== F. VCRUNTIME140_1 代理：转发 thunk ==")
    dll = Path(__file__).resolve().parent / "VCRUNTIME140_1.dll"
    if not dll.is_file():
        print("  skip 未构建（bash build.sh）")
    else:
        b = dll.read_bytes()
        dpe = struct.unpack_from("<I", b, 0x3C)[0]
        dn = struct.unpack_from("<H", b, dpe + 6)[0]
        dosz = struct.unpack_from("<H", b, dpe + 20)[0]
        dib = struct.unpack_from("<Q", b, dpe + 24 + 24)[0]
        dsecs = []
        for i in range(dn):
            o = dpe + 24 + dosz + i * 40
            dsecs.append((b[o:o + 8].rstrip(b"\0").decode(),)
                         + struct.unpack_from("<IIII", b, o + 8))
        def dv2f(va):
            r = va - dib
            for _nm, vs, sva, rs, ptr in dsecs:
                if sva <= r < sva + max(vs, rs): return ptr + (r - sva)
        def dsec(va):
            r = va - dib
            for nm, vs, sva, rs, ptr in dsecs:
                if sva <= r < sva + max(vs, rs): return nm
        ok(struct.unpack_from("<H", b, dpe + 4)[0] == 0x8664, "PE32+ x86-64")
        ok(bool(struct.unpack_from("<H", b, dpe + 22)[0] & 0x2000), "是 DLL")
        eo = dv2f(dib + struct.unpack_from("<II", b, dpe + 24 + 112)[0])
        na = struct.unpack_from("<I", b, eo + 24)[0]
        nameo = dv2f(dib + struct.unpack_from("<I", b, eo + 32)[0])
        names = [b[dv2f(dib + struct.unpack_from("<I", b, nameo + i * 4)[0]):].split(b"\0")[0].decode()
                 for i in range(na)]
        ok(names == ["__CxxFrameHandler4"],
           "导出正好是 exe 静态导入的那一个：%s" % names)
        va = dib + struct.unpack_from("<I", b, dv2f(dib + struct.unpack_from("<I", b, eo + 28)[0]))[0]
        md2 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64); md2.detail = True
        ins2 = list(md2.disasm(b[dv2f(va):][:16], va))
        print("       " + " ".join(ins2[0].bytes.hex(" ").split()) +
              "   " + ins2[0].mnemonic + " " + ins2[0].op_str)
        ok(ins2[0].mnemonic == "jmp" and ins2[0].size == 6,
           "转发是单条 6 字节 jmp —— 不建栈帧、不碰寄存器，unwinder 的调用现场原样保留")
        slot = ins2[0].address + ins2[0].size + ins2[0].operands[0].mem.disp
        ok(dsec(slot) == ".data", "跳转经由 .data 里的函数指针槽（可写，DllMain 填入真身）")
        ok(struct.unpack_from("<Q", b, dv2f(slot))[0] == 0,
           "槽的静态初值为 0 —— 真身地址只在运行时写入，不硬编码任何系统 DLL 地址")
        if exe is not None:
            # 对账：exe 到底向 VCRUNTIME140_1 要了哪些函数？少导出一个，进程就起不来。
            epe = struct.unpack_from("<I", exe, 0x3C)[0]
            io = v2f(ib + struct.unpack_from("<II", exe, epe + 24 + 112 + 8)[0])
            want = None
            while True:
                oft, _t, _f, nm, fta = struct.unpack_from("<IIIII", exe, io)
                if oft == 0 and nm == 0: break
                dllnm = exe[v2f(ib + nm):].split(b"\0")[0].decode()
                if dllnm.lower() == "vcruntime140_1.dll":
                    want, t = [], v2f(ib + (oft or fta))
                    while True:
                        e = struct.unpack_from("<Q", exe, t)[0]
                        if e == 0: break
                        want.append("ordinal%d" % (e & 0xFFFF) if e >> 63
                                    else exe[v2f(ib + e + 2):].split(b"\0")[0].decode())
                        t += 8
                io += 20
            ok(want is not None, "exe 确实静态导入 VCRUNTIME140_1.dll（这条路才成立）")
            ok(want == names,
               "exe 要的 %s 与我们导出的 %s 完全一致" % (want, names))

        src2 = (Path(__file__).resolve().parent / "proxy_vcr.c").read_text(encoding="utf-8")
        ok("return FALSE;" in src2.split("GetProcAddress")[1],
           "取不到真身时 DllMain 返回 FALSE —— 进程启动期响亮失败，而不是留 NULL 等异常时才炸")

    print("== G. 宿主单测（策略 + 抽取器，gcc）==")
    import subprocess
    r = subprocess.run(["make", "-C", str(Path(__file__).resolve().parent / "tests"), "test"],
                       capture_output=True, text=True)
    ok(r.returncode == 0 and "test_policy ok" in r.stdout and "test_extract ok" in r.stdout,
       "make -C tests test 两项都过" + ("" if r.returncode == 0 else "：\n" + r.stdout[-800:] + r.stderr[-800:]))

    print()
    if fails:
        print(f"✗ {len(fails)} 条未通过"); return 1
    print("✓ 全部通过"); return 0

sys.exit(main())
