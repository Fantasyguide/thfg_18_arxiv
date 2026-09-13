"""sound_emit.py —— 音效表扩容的 codecave 与 binhack 发射器。

★ 版权红线：cfg 表的 84 行内容与 72 个 wav 名字符串是 ZUN 的数据，
  patch 里一个字节都不留 —— 一律在 patch_init 里从**用户自己那份 exe** rep movsd 拷。

★ 时机：`*_patch_init` 由 patch_func_init 调用，在 codecaves_apply 末尾
  （binhack.cpp:1724），**早于** binhacks_apply（runconfig.cpp:655-656）。
  「先把表填好、再让改过的代码去读它」正好落在这个缝里。
  ⚠️ 反过来它**不能**用来验证 binhack —— 它跑的时候一个 binhack 都还没打。

调用约定：`typedef void (TH_CDECL *mod_call_type)(void *param)`（plugin.h:71）
—— cdecl、一参、调用方清栈，所以结尾是裸 `ret`，不是 `ret 4`。
`pushad`/`popad` 覆盖了 cdecl 要求被调方保留的 ebx/esi/ebp/edi；
`cld` 为 `rep movsd` 显式确保 DF=0（ABI 本就保证，写出来省得复核时争论）。
"""
import struct

import sound_sites as S


def emit_sound_codecaves():
    """四块 RW 内存 + 一段开机初始化代码。

    新行骨架只写 `+0 = 84+k`（满足 I1）—— 其余四个字段要的正是 0，而
    **thcrap 把只给 size 的 codecave 清零**（同 th18_card_unlocked 的依赖）。
    `+4 = 0` 让未登记的新 id 指向原生 wav 0（se_plst00），满足 I2：
    blob 非 NULL，不会掉进 0x4776f0 的 Sleep(10) 死等。
    """
    assert S.CFG_ROWS * S.CFG_ROW % 4 == 0
    cfg_dwords   = S.CFG_ROWS * S.CFG_ROW // 4          # 84 * 0x14 / 4 = 420
    names_dwords = S.NAMES_N + 1                        # 72 项 + NULL 终止

    #   L: ab           stosd            ; [edi] = eax(槽号); edi += 4
    #      83 c7 10     add edi, 0x10    ; 跳过 +4 / +8 / +0xc / +0x10
    #      40           inc eax
    #      4b           dec ebx
    #      75 xx        jnz L
    body = "ab" "83c710" "40" "4b"
    body_len = len(body) // 2
    loop = body + "75%02x" % ((256 - (body_len + 2)) & 0xff)

    code = "".join([
        "fc",                                                  # cld
        "60",                                                  # pushad
        # ① 原生 84 行 cfg → cave
        "bf<codecave:%s>" % S.CAVE_CFG,                        # mov edi, cfg cave
        "be<Rx%x>" % S.CFG_RVA,                                # mov esi, 原生 cfg 表
        "b9%s" % struct.pack("<I", cfg_dwords).hex(),          # mov ecx, 420
        "f3a5",                                                # rep movsd
        # ② 32 个新行的骨架（edi 已停在 cave + 84*0x14）
        "b8%s" % struct.pack("<I", S.CFG_ROWS).hex(),          # mov eax, 84
        "bb%s" % struct.pack("<I", S.NEW_N).hex(),             # mov ebx, 32
        loop,
        # ③ 原生 72 个 wav 名指针 + NULL → cave
        "bf<codecave:%s>" % S.CAVE_NAMES,                      # mov edi, names cave
        "be<Rx%x>" % S.NAMES_RVA,                              # mov esi, 原生名表
        "b9%s" % struct.pack("<I", names_dwords).hex(),        # mov ecx, 73
        "f3a5",                                                # rep movsd
        "61",                                                  # popad
        "c3",                                                  # ret（cdecl，调用方清栈）
    ])

    titles = {
        S.CAVE_CFG:   "音效 cfg 表搬迁目标（%d 行 × 0x%x）" % (S.CFG_ROWS_N, S.CFG_ROW),
        S.CAVE_NAMES: "wav 名表搬迁目标（%d 项 + NULL）" % S.NAMES_N_N,
        S.CAVE_SLOTS: "slot 数组搬迁目标（%d × 0x%x）" % (S.CFG_ROWS_N, S.SLOT_SIZE),
        S.CAVE_BLOBS: "blob 指针数组搬迁目标（%d 项；72.. 由 DLL 填语音）" % S.NAMES_N_N,
    }
    out = {name: {"size": "0x%x" % size, "access": "RW", "title": titles[name]}
           for name, size in S.CAVE_SIZE.items()}
    out[S.CAVE_INIT] = {
        "code": code, "export": True, "access": "RX",
        "title": "开机把原生 84 行 cfg 与 72 个 wav 名从用户的 exe 拷进 codecave，"
                 "并给 32 个新行写 +0 = 槽号 的骨架（不变式 I1 / I2）",
    }
    return out


def emit_sound_binhacks(text, text_va):
    """51 处：把指令里那 4（或 1）字节常量换掉，pre / post 原样保留。长度必须不变。

    · cave 引用 → <codecave:名+偏移>（**绝对**表达式用尖括号；方括号是相对偏移，
      写错会把相对量当指针用 —— mods/thcrap-platform.md §3.4）
    · 计数界   → 新立即数，宽度与原生一致（imm8 的必须仍塞得下）
    """
    B = {}
    for r in S.scan(text, text_va):
        if r["cave"] is None:
            if r["width"] == 4:
                body_new = struct.pack("<I", r["new"]).hex()
                body_old = struct.pack("<I", r["old"]).hex()
            else:
                body_new = "%02x" % (r["new"] & 0xff)
                body_old = "%02x" % (r["old"] & 0xff)
        else:
            suffix = ("+%x" % r["off"]) if r["off"] else ""
            body_new = "<codecave:%s%s>" % (r["cave"], suffix)
            body_old = struct.pack("<I", r["old"]).hex()
        B["snd_%06x" % r["va"]] = {
            "addr": "0x%06x" % r["va"],
            "code": r["pre"].hex() + body_new + r["post"].hex(),
            "expected": r["pre"].hex() + body_old + r["post"].hex(),
            "title": "音效表扩容：%s" % r["title"],
        }
    assert len(B) == 51, "应生成 51 条 binhack，实际 %d" % len(B)
    return B


SND_GATE = 0x476410          # SoundManager::init 入口
SND_GATE_BYTES = "558bec6aff"   # push ebp; mov ebp,esp; push -1 —— 5 字节，无相对寻址


def emit_sound_breakpoints(text, text_va):
    """自检门：0x476410 = SoundManager::init 入口。

    语音 blob 必须在引擎建 buffer 之前就位，这里正好在循环 1 / 循环 2 之前。
    入口形状与 ce_gate 用的 0x4637d0 同款（55 8b ec 6a ff），可整段搬进 cave。
    """
    raw = text[SND_GATE - text_va:SND_GATE - text_va + 5]
    if raw.hex() != SND_GATE_BYTES:
        raise S.ShapeError("0x%06x 入口是 %s，应为 %s" % (SND_GATE, raw.hex(), SND_GATE_BYTES))
    return {"ce_snd_gate": {
        "addr": "0x%06x" % SND_GATE, "cavesize": 5, "expected": SND_GATE_BYTES,
        "title": "音效表门：SoundManager::init 入口 → BP_ce_snd_gate"
                 "（填语音 blob 与新行配置 + I1/I2 自检，然后放行让引擎建 buffer）"}}


def verify_sound_binhack(name, bh, text, text_va):
    """对账一条：expected 与 exe 逐字节一致；code 只换常量、长度不变、不碰 opcode。

    音效表这批比卡表那批多两种形状，通用分支认不了：
      · 常量后面还有字节（`mov r/m32, imm32` 的 disp32 后跟 imm32）
      · 纯立即数、没有 <…> 表达式（两处计数界）
    """
    bad = []
    va = int(bh["addr"], 16)
    off = va - text_va
    exp = bytes.fromhex(bh["expected"])
    if text[off:off + len(exp)] != exp:
        bad.append("%s：exe 里是 %s，expected 写的是 %s"
                   % (name, text[off:off + len(exp)].hex(), bh["expected"]))
        return bad

    code = bh["code"]
    n_expr = code.count("<")
    if n_expr != code.count(">") or n_expr > 1:
        bad.append("%s：code 里的表达式不是 0 或 1 个" % name)
        return bad

    if n_expr == 1:
        i, j = code.index("<"), code.index(">")
        pre, post = code[:i], code[j + 1:]
        if len(pre) % 2 or len(post) % 2:
            bad.append("%s：code 的前后缀不是整字节" % name)
            return bad
        if len(pre) // 2 + 4 + len(post) // 2 != len(exp):
            bad.append("%s：code 渲染 %d 字节 != expected %d 字节（thcrap 会静默跳过校验）"
                       % (name, len(pre) // 2 + 4 + len(post) // 2, len(exp)))
            return bad
        if bytes.fromhex(pre) != exp[:len(pre) // 2]:
            bad.append("%s：code 换掉了 opcode 而不只是常量" % name)
        if post and bytes.fromhex(post) != exp[len(exp) - len(post) // 2:]:
            bad.append("%s：code 改了常量之后的字节" % name)
        return bad

    # 纯立即数：差异必须是一段位于末尾的 1 或 4 字节，前面一个字节都不许动
    new = bytes.fromhex(code)
    if len(new) != len(exp):
        bad.append("%s：code %d 字节 != expected %d 字节" % (name, len(new), len(exp)))
        return bad
    # 差异必须全部落在末尾那 1 或 4 个字节里。注意 imm32 的高位字节常常相同
    # （0x7e0 → 0xae0 只差第 1 字节），所以不能要求「最后一个字节变了」。
    for w in (1, 4):
        if len(exp) >= w and new[:len(exp) - w] == exp[:len(exp) - w] and new != exp:
            return bad
    bad.append("%s：差异不止末尾的 1 或 4 字节（exp %s / code %s）"
               % (name, exp.hex(), new.hex()))
    return bad
