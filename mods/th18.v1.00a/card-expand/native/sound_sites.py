"""sound_sites.py —— TH18 音效表扩容的站点表与扫描器。

四张原生表要整体搬进 codecave，`.text` 里 52 处引用它们。本模块只回答两件事：
  ① 这 52 处每一处在 exe 里长什么样、该换成什么（scan）
  ② 除了这 52 处，还有没有别的地方撞上这些地址（audit）

一手：`engine/_shared/th18-sound-table.md`；设计 `docs/superpowers/specs/2026-09-05-voice-expand-design.md`。

★ 不用 x86imm.classify 的**回溯**，用 `_decode` 从**已知的指令起点正向解码**。
  回溯在这批站点上会挑错解释（`b8 80 9b 4c 00` = `mov eax,imm32` 会被前一条指令的
  尾字节 `ff` 接成 `ff /7`，两种读法都自洽，tier 判给了后者）。指令起点是从 Ghidra
  的完整反汇编取的，比回溯可靠。

★ 不认识就报错。`_decode` 抛异常时**不许 except 掉**——静默跳过一个站点 =
  表搬了一半，且不崩不报错。
"""
import struct

from x86imm import _decode, classify, UnknownEncoding, Ambiguous   # noqa: F401

IMAGEBASE = 0x400000

# ---- 原生形状（一手：engine/_shared/th18-sound-table.md）--------------------
CFG_BASE   = 0x4c9b80
CFG_ROW    = 0x14
CFG_ROWS   = 84
# ★ init 循环 2 的游标是「行 +4」，所以尾界是 CFG_BASE + 4 + 84*0x14 = 0x4ca214，
#   **不是** CFG_BASE + 84*0x14（那是 0x4ca210）。差这 4 个字节会少建一行。
CFG_CURSOR = 4
CFG_END    = CFG_BASE + CFG_CURSOR + CFG_ROWS * CFG_ROW      # 0x4ca214
NAMES_BASE = 0x4b47a0
NAMES_N    = 72                                              # 72 项 + NULL 终止
SLOT_BASE  = 0x56c804
SLOT_SIZE  = 0x18
SLOT_N     = 84
SLOT_END   = SLOT_BASE + SLOT_N * SLOT_SIZE                  # 0x56cfe4
BLOB_BASE  = 0x56cfe4
BLOB_N     = 72
BLOB_END   = BLOB_BASE + BLOB_N * 4                          # 0x56d104

# ---- 扩容后 ---------------------------------------------------------------
NEW_N      = 32                                              # 新 id 0x54..0x73
CFG_ROWS_N = CFG_ROWS + NEW_N                                # 116
NAMES_N_N  = NAMES_N + NEW_N                                 # 104
FIRST_ID   = CFG_ROWS                                        # 0x54

# ★★ **新 id 的分配表**（只读快照，2026-09-20）--------------------------------------------
# 单一事实源 = `assets/voice/ORDER.txt`：**行号 k（0 起，跳过注释与空行）→ id = 0x54+k**，
# 并在 `patch/th18/voice.js` 里显式登记同一个 id（`assets/build_voice.py` 构建期对账，错了直接停）；
# 本文件只管 id 空间的**宽度**（`NEW_N`）与四张表的搬迁，**不写第二份 id 表**（会漂移）。
# 下表是给人看的：谁在用哪一格、一手出处在哪。
#
#   id     NAME            用在哪（一手出处见 ORDER.txt 与各处调用点注释）
#   0x54   ROYAL_RAGTIME   58-62 皇家同花顺（自己合成的拉格泰姆）
#   0x55   FIRELORD_SUMMON 72 炎魔之王召唤（用户语音素材）
#   0x56   FIRELORD_ATTACK 72 炎魔之王投火球（用户语音素材）
#   0x57   ASTRALUP        113 灵界槽跨格（TH13 `th13:0x43d574` → `se_astralup.wav`）
#   0x58   UFOALERT        112 UFO 出场第二声（TH12 `th12:0x44a9fe` → `se_ufoalert.wav`）
#   0x59   CHARGE00        109/111 蓄力起手（TH09 `th09:0x41fa5e` → `se_charge00.wav`）
#   0x5a   CHARGEUP        109 蓄力每升一级（TH09 `th09:0x41fb31` → `se_chargeup.wav`）
#   0x5b   MOFPOWERUP      110 满 P 上涨音（TH10 `th10:0x41b446`/`0x41b68c` → `se_powerup.wav`；
#                          ★ 与 TH18 的同名文件**不是同一次录音**）
#   0x5c   PCB_BORDER      107 森罗结界展开（TH07 `th07:0x441b91` → `se_border.wav`；TH07 独有）
#   0x5d.. 0x73            **空闲**（23 格）
#
# ⚠ 每作只能加一枚、且**别作原版的 wav 不入库**：新 SE 走
# `assets/voice/derive_retail_se.py` 的 `TABLE`（构建期从 `local/<版本>/dat` 逐字节现取）。

CAVE_CFG   = "th18_snd_cfg"
CAVE_NAMES = "th18_snd_names"
CAVE_SLOTS = "th18_snd_slots"
CAVE_BLOBS = "th18_snd_blobs"
CAVE_INIT  = "th18_snd_patch_init"

CFG_RVA    = CFG_BASE   - IMAGEBASE                          # 0xc9b80
NAMES_RVA  = NAMES_BASE - IMAGEBASE                          # 0xb47a0

CAVE_SIZE = {
    CAVE_CFG:   CFG_ROWS_N * CFG_ROW,          # 0x910
    CAVE_NAMES: (NAMES_N_N + 1) * 4,           # 0x1a4（含 NULL 终止）
    CAVE_SLOTS: CFG_ROWS_N * SLOT_SIZE,        # 0xae0
    CAVE_BLOBS: NAMES_N_N * 4,                 # 0x1a0
}

# 完整性审计要扫的四段原生地址（半开区间，尾界本身也要扫到）
TABLE_RANGES = {
    CAVE_CFG:   (CFG_BASE,   CFG_END + 1),
    CAVE_NAMES: (NAMES_BASE, NAMES_BASE + (NAMES_N + 1) * 4),
    CAVE_SLOTS: (SLOT_BASE,  SLOT_END + 0xd),
    CAVE_BLOBS: (BLOB_BASE,  BLOB_END + 1),
}

# ---- 49 处「换成 <codecave:X+off>」的引用 ---------------------------------
# off = 新表内的字节偏移。尾界站点的 off = 该循环游标的新终点。
# 形状（pre/post/kind）不写死在表里 —— 每次从 exe 现解，见 scan()。
SND_REFS = (
    # ---- cfg 表：6 引用 + 1 尾界 ----
    (0x40111a, CAVE_CFG, 0x0, "pre-main 扫描起点"),
    (0x476457, CAVE_CFG, 0x0, "init 循环1 扫描起点"),
    (0x4766bd, CAVE_CFG, 0x4, "init 循环2 游标（行+4）"),
    (0x476716, CAVE_CFG, 0x4, "init 错误路径取 wav 名下标"),
    (0x476bea, CAVE_CFG, 0xa, "play_sound 两参版取音量"),
    (0x476c8d, CAVE_CFG, 0xa, "play_sound 三参版取音量"),
    (0x4766ef, CAVE_CFG, CFG_CURSOR + CFG_ROWS_N * CFG_ROW, "init 循环2 尾界（★ 游标是行+4）"),
    # ---- slot 数组：25 引用 + 3 尾界 ----
    (0x401110, CAVE_SLOTS, 0x4,  "pre-main 写 +4 = -1"),
    (0x401129, CAVE_SLOTS, 0xc,  "pre-main 写 +0xc = 槽号"),
    (0x401130, CAVE_SLOTS, 0x8,  "pre-main 写 +8 = &cfg 行"),
    (0x444d8f, CAVE_SLOTS, 0x0,  "stop-all 起点"),
    (0x45a4a1, CAVE_SLOTS, 0x8,  "设备恢复重播循环起点（+8 游标）"),
    (0x45ff38, CAVE_SLOTS, 20 * SLOT_SIZE, "★ 硬编码槽 20（se_lazer02 常驻激光音）"),
    (0x471393, CAVE_SLOTS, 0x0,  "WinMain 释放起点"),
    (0x4766b8, CAVE_SLOTS, 0x0,  "init 循环2 起点"),
    (0x476c60, CAVE_SLOTS, 0x4,  "play_sound 两参版写音量"),
    (0x476d06, CAVE_SLOTS, 0x4,  "play_sound 三参版写音量"),
    (0x477533, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x477562, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x4775aa, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x4775bf, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x4775ce, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x4775fa, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x477652, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x47766b, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x4777c0, CAVE_SLOTS, 0x0,  "DuplicateSoundBuffer 源"),
    (0x4775f3, CAVE_SLOTS, 0x8,  "消费者读 &cfg 行"),
    (0x477664, CAVE_SLOTS, 0x8,  "消费者读 &cfg 行"),
    (0x477736, CAVE_SLOTS, 0x8,  "0x4776f0 去重扫描起点"),
    (0x4775dc, CAVE_SLOTS, 0x10, "消费者写播放态"),
    (0x47753a, CAVE_SLOTS, 0x14, "消费者清播放位"),
    (0x47755b, CAVE_SLOTS, 0x14, "消费者写播放位"),
    (0x444dbf, CAVE_SLOTS, CFG_ROWS_N * SLOT_SIZE,       "stop-all 尾界"),
    (0x4713ad, CAVE_SLOTS, CFG_ROWS_N * SLOT_SIZE,       "WinMain 释放尾界"),
    (0x45a4c5, CAVE_SLOTS, CFG_ROWS_N * SLOT_SIZE + 0x8, "重播循环尾界（+8 游标）"),
    # ---- blob 数组：9 引用 + 1 尾界 ----
    (0x4713b5, CAVE_BLOBS, 0x0, "WinMain 释放起点"),
    (0x4767cc, CAVE_BLOBS, 0x0, "预加载线程写 blob"),
    (0x477758, CAVE_BLOBS, 0x0, "0x4776f0 判空"),
    (0x47777b, CAVE_BLOBS, 0x0, "0x4776f0 判空（等待中）"),
    (0x477788, CAVE_BLOBS, 0x0, "0x4776f0 取 blob"),
    (0x477905, CAVE_BLOBS, 0x0, "0x4776f0 取 blob"),
    (0x47791f, CAVE_BLOBS, 0x0, "0x4776f0 清 blob"),
    (0x477956, CAVE_BLOBS, 0x0, "0x4776f0 取 blob"),
    (0x477970, CAVE_BLOBS, 0x0, "0x4776f0 清 blob"),
    # ★ 释放尾界刻意只到原生 72：语音 blob 来自 thcrap 的堆，交给游戏的 free 会崩。
    #   进程此刻正在退出，不释放到此为止。见 spec §3.3 E。
    (0x4713d8, CAVE_BLOBS, BLOB_N * 4, "WinMain 释放尾界（★ 只到原生 72）"),
    # ---- wav 名表：4 引用（界是计数，见 SND_UNTOUCHED）----
    (0x4766d3, CAVE_NAMES, 0x0, "init 加载取名"),
    (0x47671d, CAVE_NAMES, 0x0, "init 错误路径取名"),
    (0x4767bc, CAVE_NAMES, 0x0, "预加载取名"),
    (0x476803, CAVE_NAMES, 0x0, "预加载错误路径取名"),
)

# ---- 2 处「换立即数」的计数界 ---------------------------------------------
# 宽度从 exe 现解：0x401139 是 imm32（81 fa e0 07 00 00），0x476472 是 imm8（83 fa 54）。
SND_COUNTS = (
    (0x401139, SLOT_N * SLOT_SIZE, CFG_ROWS_N * SLOT_SIZE, "pre-main slot 初始化字节界"),
    (0x476472, SLOT_N,             CFG_ROWS_N,             "init 循环1 槽数界"),
)

# ---- 1 处刻意不改 ---------------------------------------------------------
SND_UNTOUCHED = (
    (0x4767d8, NAMES_N, "预加载 wav 数：仍只从 dat 读原生 72 个，语音 blob 由 DLL 填"),
)


class ShapeError(Exception):
    pass


def _u32(text, off):
    return struct.unpack_from("<I", text, off)[0]


def decode_ref(text, text_va, va, lo, hi):
    """从已知指令起点正向解码，挑出唯一落在 [lo, hi) 的 4 字节字段。

    返回 (pre, old, post, kind, ins_len)：
      pre / post —— 该 4 字节之前 / 之后的指令字节（post 非空的例子：
                    `mov r/m32, imm32` 的 disp32 后面还跟着 imm32）
    """
    off = va - text_va
    end, fields, txt, _has_modrm = _decode(text, off)          # 不认识 → 抛，别接
    got = [(fo, k) for fo, k in fields if lo <= _u32(text, fo) < hi]
    if len(got) != 1:
        raise ShapeError("0x%06x（%s）：落在表区间内的 4 字节字段有 %d 个，应为 1"
                         % (va, txt, len(got)))
    fo, kind = got[0]
    return text[off:fo], _u32(text, fo), text[fo + 4:end], kind, end - off


def decode_count(text, text_va, va, old):
    """计数界：返回 (pre, width, post)。width ∈ {1, 4}，从 exe 现解。"""
    off = va - text_va
    end, fields, txt, _ = _decode(text, off)
    imm = [fo for fo, k in fields if k == "imm"]
    if imm:                                                     # imm32
        fo = imm[0]
        got = _u32(text, fo)
        if got != old:
            raise ShapeError("0x%06x（%s）：imm32 是 0x%x，表里写的是 0x%x" % (va, txt, got, old))
        return text[off:fo], 4, text[fo + 4:end]
    raw = text[off:end]                                         # imm8（<alu> r/m32, imm8）
    if not txt.endswith("imm8"):
        raise ShapeError("0x%06x（%s）：既没有 imm32 也不是 imm8 形式" % (va, txt))
    if raw[-1] != (old & 0xff) or old > 0x7f:
        raise ShapeError("0x%06x（%s）：imm8 是 0x%02x，表里写的是 0x%x" % (va, txt, raw[-1], old))
    return raw[:-1], 1, b""


def scan(text, text_va):
    """把 SND_REFS / SND_COUNTS 逐条对回 exe。顺带校验 SND_UNTOUCHED 没被动。

    返回 [{"va","pre","post","old","new","cave","off","width","title","len","kind"}]。
    cave = None 表示纯立即数界（new 是新的立即数）。
    """
    out = []
    for va, cave, off, title in SND_REFS:
        lo, hi = TABLE_RANGES[cave]
        pre, old, post, kind, n = decode_ref(text, text_va, va, lo, hi)
        out.append({"va": va, "pre": pre, "post": post, "old": old, "new": None,
                    "cave": cave, "off": off, "width": 4, "title": title,
                    "len": n, "kind": kind})
    for va, old, new, title in SND_COUNTS:
        pre, width, post = decode_count(text, text_va, va, old)
        if width == 1 and new > 0x7f:
            raise ShapeError("0x%06x：新值 0x%x 塞不进 imm8" % (va, new))
        out.append({"va": va, "pre": pre, "post": post, "old": old, "new": new,
                    "cave": None, "off": None, "width": width, "title": title,
                    "len": len(pre) + width + len(post), "kind": "imm"})
    for va, val, why in SND_UNTOUCHED:
        pre, width, post = decode_count(text, text_va, va, val)   # 只求它别变
        del pre, width, post
    return out


def audit(text, text_va, covered):
    """扫四段原生地址区间里所有 4 字节出现处，列出不在已覆盖站点内的。

    这些**不该被改** —— 要么是同值的热全局（`0x4ca214` 本身就是一个被
    `0x401e0e` / `0x474b45` 读写的普通 int 全局），要么是别的表。
    列出来是为了让人确认「漏掉的都是该漏的」。
    """
    out = []
    for cave, (lo, hi) in TABLE_RANGES.items():
        for v in range(lo, hi, 4):
            needle = struct.pack("<I", v)
            p = text.find(needle)
            while p >= 0:
                if (text_va + p) not in covered:
                    try:
                        info = classify(text, p, text_va, 0)
                        desc, kind = info["text"], info["kind"]
                    except (UnknownEncoding, Ambiguous):
                        desc, kind = "(无法归类)", "?"
                    out.append((text_va + p, v, cave, kind, desc))
                p = text.find(needle, p + 1)
    out.sort()
    return out
