#!/usr/bin/env python3
"""card-expand 站点扫描器 / thcrap patch 生成器。

    python3 sites.py list             # 25 个查表实例的全表
    python3 sites.py check            # 只校验(CI / make check 用)
    python3 sites.py gen --rows 58 -o ../patch/th18.v1.00a.js

不依赖任何外部反汇编器，不写死任何机器上的路径。

**为什么必须生成而不是手写**：99 处 `expected` 手写等于必然出错，而 thcrap
对 `expected` 不匹配的处理是「记一行日志然后跳过」（binhack.cpp:1420）——
对整表搬迁来说，部分应用就是灾难。

两层结构：
  shape.py  —— **权威**站点来源，整体匹配内联查表的骨架，
                每次命中自带「四条臂配套」的保证；
  x86imm.py —— **完整性审计**，扫出所有撞上这些值的地方，
                凡不在已匹配站点内的都列出来人工过目。
"""
import argparse, hashlib, json, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from shape import find_all, find_walks, ShapeError             # noqa: E402
from x86imm import classify, UnknownEncoding, Ambiguous       # noqa: E402
import sound_sites as SND                                            # noqa: E402
from sound_emit import (emit_sound_codecaves, emit_sound_binhacks,   # noqa: E402
                        emit_sound_breakpoints, verify_sound_binhack)

REPO    = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
VERSION = "th18.v1.00a"
EXE     = os.path.join(REPO, "local", VERSION, "th18.exe")
EXE_MD5 = "9969cac756098c1da05a81de45437a70"

# ---- 死绑量（出处：engine/card/th18/11-sentinels-56-57.md 的一手穷举）----
TABLE_BASE = 0x4c53c0     # zTableCardData[]
ROW_SIZE   = 0x34
ROW_COUNT  = 58           # 原生行数
NULL_ROW   = 56           # 查不到时的回退行 = 行 56 ("NULL")
TABLE_END  = TABLE_BASE + ROW_COUNT * ROW_SIZE        # 0x4c5f88
FALLBACK   = TABLE_BASE + NULL_ROW  * ROW_SIZE        # 0x4c5f20
CODECAVE   = "th18_card_table"
CAVE_INIT  = CODECAVE + "_patch_init"
TABLE_RVA  = 0x0c53c0     # = TABLE_BASE - imagebase；用 thcrap 的 Rx 记法,不写死基址

# ---- 战线 B：分配器（engine/card/th18/11 §2.3 / §3；PLAN-255-ids §2 战线 B）----
JT_RVA      = 0x012dac    # allocate_new_card 的跳转表 0x412dac，57 项
JT_COUNT    = 57
CASE56_RVA  = 0x011489    # case 56 的函数体：new(0x54) → memset → 挂基类虚表 → jmp 公共尾段
ALLOC_CMP   = 0x411479    # cmp ebx, 0x38   (83 fb 38)
ALLOC_JMP   = 0x411482    # jmp [0x412dac + ebx*4]   (ff 24 9d ac 2d 41 00)
JT_CODECAVE = "th18_card_jumptable"
# ---- 战线 C：zAbilityManager 扩容（PLAN-255-ids §2 战线 C）----
MGR_SIZE      = 0xd70     # operator_new / memset / sized delete 的实参
OWNED_OLD     = 0xc84     # 「本局是否拥有」int[56]，止于 0xd64，对象止于 0xd70
OWNED_NEW     = 0xd70     # 搬到对象尾部
SHOP_RETAIL   = 56        # 原生商店三处循环只看 id 0..55（止于 +0xd64）。现在上界 = rows：
                          #   幻影 id 查表回落到 NULL 行 56，装载器把 cave 里 56/57 两行的 +0x14 写成 6，
                          #   三个循环（≠0&&≠6 / ==0 / dmode 1-5）就都过不了。AUDIT §N1。
# 测试钩子（只进 patch-test）
TEST_MOVZX  = 0x407ee3    # reset_cards 读初始卡组：movzx eax, byte [eax+esi+0x5f608]（8 字节）
TEST_TRACE  = 0x411469    # allocate_new_card 序言：cmp [edi+0x28], 0x100（7 字节）
# 自检门的断点：ScoreFile__load 入口，只被调一次(0x452cde)，是最早碰卡表的函数
GATE_ADDR   = 0x4637d0    # 55 8b ec 6a ff = push ebp; mov ebp,esp; push -1（5 字节，无相对寻址）
# ---- 战线 D：存档影子数组（PLAN-255-ids §2 战线 D；NEXT.md）----
UNLOCKED_OFF   = 0x5f588  # zScoreFile.unlocked_cards：uint8_t[57]（ExpHP type-structs-own.json）
UNLOCKED_CAVE  = "th18_card_unlocked"   # 影子数组：256 字节，下标 = card id
UNLOCK_WRITE   = 0x418e04 # mark_obtained 的写：mov byte [esi+edi+0x5f588], 1（8 字节）→ 断点
SAVE_LOADED    = 0x46398a # ScoreFile__load 尾段：lea esi,[ebx+0x5f4b8]（6 字节），SCOREFILE_PTR 刚写好，ebx = 存档
UNLOCK_ALL     = 0x4648fe # ScoreFile__unlock_all：lea eax,[ebx+0x5f588]（6 字节），下一条就是 memset(…,1,0x38)
SUBOBJ_A_OFF   = 0x5f4b8  # unlocked_cards 所在子对象（init_from_table 的 this）；影子初始化只用 0x5f588
SCOREFILE_PTR  = 0x4cf41c # 全局 SCOREFILE_PTR；每处读的前几条里都有一条 mov r32,[0x4cf41c]
# ---- 战线 E（第一块）：文案重定向。zAbilityText 只有 57 张 × 0x1c0，id≥57 的文案改指向 DLL 自己的缓冲 ----
ABILITY_TXT_PTR = 0x4cf29c  # 全局 ABILITY_TXT_PTR
TEXT_ENTRY      = 0x1c0     # 7 行 × 0x40
# ---- 战线 E 第二块：图鉴 / 编成 —— 显示顺序表搬迁 + zAbilityMenu.__card_ids 扩容 ----
ORDER_TABLE = 0x4b3600      # 显示顺序表：57 dword（id），.rdata；紧接着 0x4b36e4 是另一张表（0x4337f7 用）
ORDER_COUNT = 57
CE_MAX_ROWS = 255           # 与 sites_gen.h 的 CE_MAX_ROWS 同义
ORDER_END   = ORDER_TABLE + ORDER_COUNT * 4          # 0x4b36e4，只有 0x414b54 的 cmp 用它当尾界
ORDER_RVA   = ORDER_TABLE - 0x400000
ORDER_CAVE  = "th18_card_order"                      # 255 dword；DLL 重排：[原生 0..55, 新 id…, 56, 57 填充]
ORDER_EXCLUDE = {0x4337f7}                           # ★ 同值不同表：mov eax,[eax*4+0x4b36e4]，不许改
# --- 商店随机池：pick_weighted_random_offer 0x416f50 的 0x8c0 栈缓冲 → codecave ---
# 那个缓冲在 ebp-0x8c4，长 0x8c0（560 dword），**末尾紧贴 ebp-0x4 的栈金丝雀**，
# 填池子的两处写全程无越界检查 ⇒ 超过 560 份直接踩烂金丝雀与返回地址（AUDIT §AH）。
# 搬到 codecave 后份数上限只受 cave 大小限制；栈帧一个字节都不动（sub esp 保持 0x8d4）。
POOL_CAVE   = "th18_card_pool"
POOL_SLOTS  = 8192          # dword；原生最坏 362 份，255 张卡 × 权重 27 也放得下
POOL_DISP   = 0xfffff73c    # 缓冲基址在栈上的位移（= -0x8c4），9 处站点都以它为基
POOL_SITES  = (             # (地址, 原字节, 相对 cave 基址的偏移)  —— 全部原地等长改写
    (0x416f6e, "68c0080000", None),   # push 0x8c0            → push <cave 字节数>（memset 大小）
    (0x416f73, "8d853cf7ffff", None), # lea eax,[ebp-0x8c4]   → mov eax,<cave>; nop
    (0x417100, "8d853cf7ffff", None), # 同上（填池子那段）
    (0x41712f, "8994bd3cf7ffff", 0x00),  # mov [ebp+edi*4-0x8c4],edx → mov [edi*4+<cave>],edx
    (0x417136, "8994bd40f7ffff", 0x04),
    (0x41713d, "8994bd44f7ffff", 0x08),
    (0x417144, "8994bd48f7ffff", 0x0c),
    (0x41714b, "8994bd4cf7ffff", 0x10),
    (0x41718f, "8b8c953cf7ffff", 0x00),  # mov ecx,[ebp+edx*4-0x8c4] → mov ecx,[edx*4+<cave>]
)

MENU_SIZE     = 0x13fc      # zAbilityMenu 大小（operator new / memset / sized delete）
CARD_IDS_OLD  = 0x304       # __card_ids int32[0x38]，后面紧跟 num_total_cards，原地扩不了
CARD_IDS_NEW  = 0x13fc      # 搬到对象尾部，255 项
MENU_CLEAR    = 0x41495c    # 编成前清 anm id 数组的循环：mov edi,0x38 → 0xff（数组本来就是 [0x100]）
# 图鉴条目数 0x38 的 7 处：patch 不动，DLL 在门里按「56 + 已注册新卡数」现写。
#   两处是 cmp r,imm8（符号扩展）→ 条目数 ≤ 127 → 新卡 ≤ 71。超了得把这两处改成断点。
MENU_COUNT_SITES = (        # (地址, 原字节, 立即数偏移, 宽度)
    (0x4137bb, "c787c401000038000000", 6, 4),   # initialize: [this+0x1c4] = 0x38
    (0x414394, "c7870003000038000000", 6, 4),   # 图鉴 fill: [this+0x300] = 0x38
    (0x41439e, "c787c401000038000000", 6, 4),   # 图鉴 fill: [this+0x1c4] = 0x38
    (0x4145e2, "83f838", 2, 1),                  # 图鉴 fill 循环: cmp eax,0x38
    (0x41570d, "b838000000", 1, 4),              # 行数 = 0x38 / 列数
    (0x4157cb, "83fa38", 2, 1),                  # 高亮循环: cmp edx,0x38
    (0x415817, "bf38000000", 1, 4),              # 退出时遍历 vm: mov edi,0x38
)
TEXT_SITES = (              # 三处 `imul r, id, 0x1c0`，后面紧跟 add 基址；断点里按 id 改写 r
    ("ce_text_name",   0x416694, 6, "69cbc0010000",   "FUN_00416540 卡名：imul ecx, ebx, 0x1c0"),
    ("ce_text_desc",   0x416779, 7, "69450cc0010000", "FUN_00416540 说明 6 行：imul eax, [ebp+0xc], 0x1c0"),
    ("ce_text_notify", 0x41926a, 6, "69c3c0010000",   "获得通知：imul eax, ebx, 0x1c0"),
)

PART_ORDER = ("start", "end", "fallback", "hit")


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_text_section(path):
    """极简 PE 解析：返回 (.text 字节, .text 的 VA, .text 的文件偏移)。"""
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    assert data[pe:pe + 4] == b"PE\0\0", "不是 PE"
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    imagebase = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    sec = pe + 24 + opt_size
    for i in range(nsec):
        off = sec + i * 40
        name = data[off:off + 8].rstrip(b"\0").decode()
        vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
        if name == ".text":
            return data[rawptr:rawptr + rawsize], imagebase + va, rawptr
    raise SystemExit("找不到 .text")


def check_invariants(inst):
    """两条不变式 —— 它们就是「搬表不会静默算错」的全部依据。

    ① `END - TABLE_END == LOOP_START - TABLE_BASE`
       循环比较的字段和终止条件必须是同一个字段。
    ② `FALLBACK - 回退行基址 == HIT_ARM - TABLE_BASE`
       两条臂必须取同一个字段；否则「查到」和「查不到」返回的不是一种东西。

    ★ 违反 ② 正是整表搬迁唯一会**静默算错**的失败模式：LOOP_START 指了新表、
      HIT_ARM 还指旧表时，`下标 * 0x34 + 旧基址` 会返回另一张卡的行——
      不崩、不报错。所以必须按实例校验，不能按站点。
    """
    bad = []
    for x in inst:
        p = x["parts"]
        f_start = p["start"]["value"] - TABLE_BASE
        f_end   = p["end"]["value"]   - TABLE_END
        f_fb    = p["fallback"]["value"] - FALLBACK
        f_hit   = p["hit"]["value"]   - TABLE_BASE
        if not (0 <= f_start < ROW_SIZE and 0 <= f_fb < ROW_SIZE):
            bad.append("0x%06x：字段偏移越界" % x["anchor"])
        elif f_start != f_end:
            bad.append("0x%06x：循环比较 +0x%x 但终止于 +0x%x"
                       % (x["anchor"], f_start, f_end))
        elif f_fb != f_hit:
            bad.append("0x%06x：★ 两条臂字段不一致 命中+0x%x vs 回退+0x%x"
                       % (x["anchor"], f_hit, f_fb))
        x["field_cmp"], x["field_ret"] = f_start, f_fb
    return bad


def collect_sites(inst, walks):
    """去重后的待改站点：{va: {part, rec, insts}}。"""
    sites = {}
    for x in walks:
        p = x["parts"]["start"]
        sites[p["va"]] = {"part": "start", "rec": p, "insts": 1}
    for x in inst:
        for k in PART_ORDER:
            p = x["parts"][k]
            cur = sites.setdefault(p["va"], {"part": k, "rec": p, "insts": 0})
            cur["insts"] += 1
            if cur["part"] != k:
                raise ShapeError("0x%06x 同时被当成 %s 和 %s" % (p["va"], cur["part"], k))
    return sites


def audit_uncovered(text, text_va, sites):
    """完整性审计：扫出所有撞上这三段值的地方，列出不在已匹配站点内的。

    这些**不该被改**——它们要么是同名的热全局（`0x4c5f88` / `0x4c5f8c`
    同时是两个被大量读写的 int 全局），要么是别的表。列出来是为了让人
    确认「漏掉的都是该漏的」。
    """
    covered = set()
    for va, s in sites.items():
        for k in range(s["rec"]["len"]):
            covered.add(va + k)
    ranges = list(range(TABLE_BASE, TABLE_BASE + ROW_SIZE)) + \
             list(range(FALLBACK, FALLBACK + ROW_SIZE)) + \
             list(range(TABLE_END, TABLE_END + 5))
    out = []
    for v in ranges:
        needle = struct.pack("<I", v)
        p = text.find(needle)
        while p >= 0:
            if (text_va + p) not in covered:
                try:
                    info = classify(text, p, text_va, 0)
                    desc, kind = info["text"], info["kind"]
                except (UnknownEncoding, Ambiguous):
                    desc, kind = "(无法归类)", "?"
                out.append((text_va + p, v, kind, desc))
            p = text.find(needle, p + 1)
    out.sort()
    return out


def audit_interior(path, sites):
    """完整性检查之二：**整张表的地址区间**在全文件里还有没有别的引用。

    前一项审计只扫「表基 / 回退行 / 表尾」三小段，扫不到「直接引用第 12 行」
    这种写法（`0x4c53c0 + 12*0x34` 落在三段之外）。这里把
    `[表基, 表尾)` 整段 0xbc8 字节的地址全扫一遍，分两路：
      · `.text` 里 —— 漏掉就是漏掉一个必须改的站点；
      · 数据节里 4 字节对齐的 —— 那会是「指向表内某行的指针表」，同样要跟着搬。
    两路当前都是 **0 处**，所以那 100 处就是全集。
    """
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt = struct.unpack_from("<H", data, pe + 20)[0]
    ib = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    secs = []
    for i in range(nsec):
        o = pe + 24 + opt + i * 40
        nm = data[o:o + 8].rstrip(b"\0").decode()
        vs, va, rs, rp = struct.unpack_from("<IIII", data, o + 8)
        secs.append((nm, ib + va, rp, rs))
    covered = set()
    for va, s_ in sites.items():
        for k in range(s_["rec"]["len"]):
            covered.add(va + k)
    lo, hi = TABLE_BASE, TABLE_BASE + ROW_COUNT * ROW_SIZE
    in_text, in_data = [], []
    for nm, sva, rp, rs in secs:
        step = 1 if nm == ".text" else 4
        for off in range(rp, rp + max(rs - 4, 0), step):
            v = struct.unpack_from("<I", data, off)[0]
            if not (lo <= v < hi):
                continue
            va = sva + (off - rp)
            if nm == ".text":
                if any((va - k) in covered for k in range(8)):
                    continue
                in_text.append((va, v))
            else:
                in_data.append((nm, va, v))
    return secs, in_text, in_data


def new_offset(part, rec, rows):
    """这个站点在新表里应该指向的偏移（相对 codecave 基址）。"""
    if part == "start":
        return rec["value"] - TABLE_BASE
    if part == "hit":
        return rec["value"] - TABLE_BASE
    if part == "fallback":
        return NULL_ROW * ROW_SIZE + (rec["value"] - FALLBACK)
    return rows * ROW_SIZE + (rec["value"] - TABLE_END)      # end


def emit(sites, rows):
    """生成 thcrap binhacks。

    ⚠️ `<codecave:NAME+OFF>` 里的 OFF **按十六进制解析**
       （expression.cpp `GetCodecaveAddress` → `strtouz(…, 16)`），
       写成看起来像十进制的 `+58` 会被当成 0x58。这里一律输出裸十六进制。
    ⚠️ `expected` 与 `code` **渲染后的长度必须相等**，否则 thcrap 会
       「记一行日志然后跳过校验」（binhack.cpp:1264）——静默丢掉护栏。
       这里由同一份记录同时产出两者，长度天然相等。
    """
    out = {}
    for va in sorted(sites):
        s = sites[va]
        part, rec = s["part"], s["rec"]
        pre = rec["bytes"][:rec["const_at"]].hex()
        off = new_offset(part, rec, rows)
        ref = "<codecave:%s%s>" % (CODECAVE, ("+%x" % off) if off else "")
        out["cardtable_%s_%06x" % (part, va)] = {
            "addr": "0x%06x" % va,
            "code": pre + ref,
            "expected": rec["bytes"].hex(),
            "title": "%s | %s | +0x%x" % (part, rec["text"], off),
        }
    return out


def emit_codecaves(rows, alloc):
    """新表 codecave + 一段把原生 58 行**运行时**拷进去的初始化代码。

    ★ 为什么不把表的内容直接写进 patch：那是 ZUN 的数据，仓库不留任何版权字节。
      改成开机时从**用户自己那份 exe**里 memcpy 过来，patch 里只有源码和地址。

    ★ 为什么初始化能放在 `*_patch_init`：它由 `patch_func_init` 调用，而后者在
      `codecaves_apply` 的末尾（binhack.cpp:1724），**早于** `binhacks_apply`
      （runconfig.cpp:655-656）。对「先把表填好、再让改过的代码去读它」来说，
      这个时机正好。
      ⚠️ 反过来说，`*_patch_init` **不能**用来验证 binhack 是否都打上了 ——
      它跑的时候一个 binhack 都还没打。那件事得靠游戏内的断点做。

    调用约定：`typedef void (TH_CDECL *mod_call_type)(void *param)`（plugin.h:71）
    —— cdecl、一个参数、调用方清栈，所以结尾是裸 `ret`，不是 `ret 4`。
    `pushad`/`popad` 覆盖了 cdecl 要求被调方保留的 ebx/esi/ebp/edi；
    `cld` 是为 `rep movsd` 显式确保 DF=0（ABI 本就保证，写出来省得复核时争论）。
    """
    copy_dwords = ROW_COUNT * ROW_SIZE // 4          # 58 * 0x34 / 4 = 754
    assert ROW_COUNT * ROW_SIZE % 4 == 0
    code = [
        "fc",                                        # cld
        "60",                                        # pushad
        "bf<codecave:%s>" % CODECAVE,                # mov edi, 新表
        "be<Rx%x>" % TABLE_RVA,                      # mov esi, 原生表(相对模块基址)
        "b9%s" % struct.pack("<I", copy_dwords).hex(),   # mov ecx, 754
        "f3a5",                                      # rep movsd
    ]
    if rows > ROW_COUNT:
        # 多出来的行全部填成「行 56(NULL)」的副本 —— 它们是**休眠**数据：
        # 查表查到它们只会得到那个无害的哨兵行。真正的新卡数据由游戏内的
        # 激活步骤在**验证过全部 binhack 都生效之后**才写进去。
        row_dwords = ROW_SIZE // 4                   # 13
        assert ROW_SIZE % 4 == 0
        body = ("be<codecave:%s+%x>" % (CODECAVE, NULL_ROW * ROW_SIZE)  # mov esi, 新表+NULL行
                + "b9%s" % struct.pack("<I", row_dwords).hex()          # mov ecx, 13
                + "f3a5"                                                # rep movsd
                + "4b")                                                 # dec ebx
        body_len = 5 + 5 + 2 + 1
        code.append("bb%s" % struct.pack("<I", rows - ROW_COUNT).hex())  # mov ebx, 行数差
        code.append(body)
        code.append("75%02x" % ((256 - (body_len + 2)) & 0xff))          # jnz 回到 body
    out = {
        CODECAVE:  {"size": "0x%x" % (rows * ROW_SIZE), "access": "RW",
                    "title": "zTableCardData[] 搬迁目标（%d 行 × 0x%x）" % (rows, ROW_SIZE)},
    }
    if alloc:
        # 战线 B：跳转表也搬。0..56 原样拷，57..rows-1 全指向 case 56 的函数体
        # —— 未注册的 id 得到一张挂基类虚表的无行为卡，card->id 由公共尾段
        # 0x412cec 写成真实 id。这就是「克隆现有卡」最干净的形式：一个字节机器码都不用手写。
        # ⚠️ 这一段**必须**有保险：跳转表若全零，任何一次 allocate 都是 jmp [0] → 崩。
        code += [
            "bf<codecave:%s>" % JT_CODECAVE,                 # mov edi, 新跳转表
            "be<Rx%x>" % JT_RVA,                             # mov esi, 原生跳转表
            "b9%s" % struct.pack("<I", JT_COUNT).hex(),      # mov ecx, 57
            "f3a5",                                          # rep movsd
            "b8<Rx%x>" % CASE56_RVA,                         # mov eax, case56
            "b9%s" % struct.pack("<I", rows - JT_COUNT).hex(),  # mov ecx, rows-57
            "f3ab",                                          # rep stosd
        ]
        out[JT_CODECAVE] = {"size": "0x%x" % (rows * 4), "access": "RW",
                            "title": "allocate_new_card 跳转表搬迁目标（%d 项）" % rows}
        # 战线 D：影子数组。thcrap 把只给 size 的 codecave 清零；内容由 DLL 在
        # BP_ce_save_loaded 里填（原生存档 + side-car）。没有 DLL = 全部「未获取」。
        out[UNLOCKED_CAVE] = {"size": "0x100", "access": "RW",
                              "title": "unlocked_cards 影子数组（256 字节，下标 = card id；DLL 填）"}
        # 战线 E：显示顺序表。保险：57 项原样拷 + 其余填 57（BACK：编成里不可见、图鉴按条目数不会走到）。
        # DLL 权威：[原生 0..55, 新 id…, 56, 57…] 并把图鉴条目数写成 56+N。
        code += [
            "bf<codecave:%s>" % ORDER_CAVE,
            "be<Rx%x>" % ORDER_RVA,
            "b9%s" % struct.pack("<I", ORDER_COUNT).hex(),
            "f3a5",
            "b8%s" % struct.pack("<I", 57).hex(),
            "b9%s" % struct.pack("<I", rows - ORDER_COUNT).hex(),
            "f3ab",
        ]
        out[ORDER_CAVE] = {"size": "0x%x" % (rows * 4), "access": "RW",
                           "title": "显示顺序表搬迁目标（%d 项；DLL 重排并追加新卡）" % rows}
    code += ["61", "c3"]                             # popad ; ret
    out[CAVE_INIT] = {"code": "".join(code), "export": True, "access": "RX",
                      "title": "开机把原生表（与跳转表）拷进 codecave；BP_ce_gate 完成运行时提交与回读"}
    return out


def emit_alloc_binhacks(rows):
    """战线 B 的两处，都同长。"""
    assert 59 <= rows <= 255
    return {
        "alloc_bound_%06x" % ALLOC_CMP: {
            "addr": "0x%06x" % ALLOC_CMP,
            "code": "83fb%02x" % (rows - 1),
            "expected": "83fb38",
            "title": "allocate_new_card: cmp ebx, 0x38 → 0x%x（可分配 id 上界）" % (rows - 1),
        },
        "alloc_jumptable_%06x" % ALLOC_JMP: {
            "addr": "0x%06x" % ALLOC_JMP,
            "code": "ff249d<codecave:%s>" % JT_CODECAVE,
            "expected": "ff249dac2d4100",
            "title": "allocate_new_card: jmp [0x412dac+ebx*4] → 新跳转表",
        },
    }


def emit_grow_binhacks(rows):
    """战线 C：zAbilityManager 扩容。12 处，全部同长（push imm32 / disp32 / imm32）。

    新大小 = 0xd70 + rows*4；owned[] 从 +0xc84 搬到 +0xd70（旧区留着不用）；
    reset_cards 的 rep stosd 清 rows 项；商店循环起点跟着搬、上界 = rows（幻影由 +0x14=6 排除，见 SHOP_RETAIL）。
    """
    new_size = MGR_SIZE + rows * 4
    shop_end = OWNED_NEW + rows * 4
    le = lambda v: struct.pack("<I", v).hex()
    B = {}
    def bh(addr, code, expected, title):
        B["grow_%06x" % addr] = {"addr": "0x%06x" % addr, "code": code, "expected": expected, "title": title}
    for a, what in ((0x4082d6, "operator_new 分配"), (0x4082ec, "operator_new 的 memset"), (0x40860a, "sized delete")):
        bh(a, "68" + le(new_size), "68" + le(MGR_SIZE), "zAbilityManager %s：0x%x → 0x%x" % (what, MGR_SIZE, new_size))
    bh(0x407eb0, "8dbb" + le(OWNED_NEW), "8dbb" + le(OWNED_OLD), "reset_cards：lea edi,[mgr+owned]")
    bh(0x407eb6, "b9" + le(rows),        "b9" + le(56),         "reset_cards：rep stosd 项数 56 → %d" % rows)
    bh(0x412d42, "c78487" + le(OWNED_NEW) + "01000000", "c78487" + le(OWNED_OLD) + "01000000",
       "allocate_new_card 尾段：owned[id] = 1")
    for a, reg in ((0x416f8f, "b9"), (0x41744a, "bb"), (0x417535, "bb")):
        bh(a, reg + le(OWNED_NEW), reg + le(OWNED_OLD), "商店循环起点 → +0x%x" % OWNED_NEW)
    for a, reg in ((0x41716b, "81f9"), (0x417527, "81fb"), (0x4175e7, "81fb")):
        bh(a, reg + le(shop_end), reg + le(OWNED_OLD + SHOP_RETAIL * 4),
           "商店循环上界 → +0x%x（%d 个 id；幻影由 NULL/BACK 行 +0x14=6 排除）" % (shop_end, rows))
    return B



# ---- 战线 D：影子数组 ----------------------------------------------------------
# 9 处读全是 `op [base+idx+0x5f588]` 的 SIB 形态。改成 `op [idx+SHADOW]`：去掉 SIB、
# disp32 换成影子数组的绝对地址，每处短 1 字节，nop 补齐。这是本 mod 第一次改 ModRM
# 而不只是换常量，所以生成器与对账器各带一份**独立**的解码：生成器从原指令算新指令，
# 对账器把 patch 里的新旧两条都拆开来比字段。

_REG8 = ("al", "cl", "dl", "bl", "ah", "ch", "dh", "bh")
_REG32 = ("eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi")


def decode_sib_byte_op(b):
    """解 `op ModRM SIB disp32 [imm8]`，只认本战线用到的四种 opcode。

    返回 {op, reg, base, index, disp, imm, len}；base/index 是寄存器号。
    只接受 mod=10（disp32）、scale=1、rm=100（有 SIB）的形态——其余一律报错，
    因为那说明扫描器抓到了不该抓的东西。
    """
    op = b[0]
    has_imm = op == 0x80                      # 80 /7 ib = cmp r/m8, imm8
    if op not in (0x3a, 0x38, 0x8a, 0x80):
        raise ShapeError("0x%02x：不是战线 D 认识的 opcode" % op)
    modrm = b[1]
    mod, reg, rm = modrm >> 6, (modrm >> 3) & 7, modrm & 7
    if mod != 2 or rm != 4:
        raise ShapeError("ModRM %02x：不是 [base+idx+disp32] 形态" % modrm)
    if has_imm and reg != 7:
        raise ShapeError("80 /%d：不是 cmp" % reg)
    sib = b[2]
    scale, index, base = sib >> 6, (sib >> 3) & 7, sib & 7
    if scale != 0 or index == 4 or base == 5:
        raise ShapeError("SIB %02x：scale/index/base 不在预期内" % sib)
    disp = struct.unpack_from("<I", b, 3)[0]
    n = 7 + (1 if has_imm else 0)
    return {"op": op, "reg": reg, "base": base, "index": index, "disp": disp,
            "imm": b[7] if has_imm else None, "len": n}


def decode_disp32_op(b):
    """解 `op ModRM disp32 [imm8]`（改写后的形态）。返回同上结构，base=None。"""
    op = b[0]
    has_imm = op == 0x80
    modrm = b[1]
    mod, reg, rm = modrm >> 6, (modrm >> 3) & 7, modrm & 7
    if mod != 2 or rm in (4, 5):
        raise ShapeError("ModRM %02x：不是 [reg+disp32] 形态" % modrm)
    disp = struct.unpack_from("<I", b, 2)[0]
    n = 6 + (1 if has_imm else 0)
    return {"op": op, "reg": reg, "base": None, "index": rm, "disp": disp,
            "imm": b[6] if has_imm else None, "len": n}


def _looks_like_jcc_rel32(text, p):
    """p 指向一个 4 字节 imm 的起点：前面若是 `0f 8x`（jcc rel32），那就是位移撞上了值。"""
    return p >= 2 and text[p - 2] == 0x0f and 0x80 <= text[p - 1] <= 0x8f


def scorefile_reg_before(text, p, window=48):
    """站点 p 之前 window 字节内最近一条 `mov r32, [SCOREFILE_PTR]` 装进了哪个寄存器。

    ★ 为什么必须看上下文：`[base+index*1+disp]` 的 base/index 在语义上是对称的，
    编译器把存档指针放哪一格是随机的——9 处里有 3 处放在 index。只按 SIB 槽位
    决定「留哪个寄存器」会把 3 处改成用存档指针去下标影子数组（不崩、全错）。
    编码：`a1 imm32` = mov eax；`8b /r` mod=00 rm=101 = mov r32,[disp32]。
    窗口 48 字节：最远的一处是 0x418e15（装载在 45 字节前的函数序言）。这里不做
    「中间没有改写该寄存器」的证明——9 处的中间指令已人工过目（AUDIT §K）；
    换 build 后要重看。
    """
    needle = struct.pack("<I", SCOREFILE_PTR)
    found = []
    for q in range(max(0, p - window), p):
        if text[q:q + 4] != needle:
            continue
        if q >= 1 and text[q - 1] == 0xa1 and q + 4 <= p:
            found.append((q, 0))                                  # eax
        elif q >= 2 and text[q - 2] == 0x8b and (text[q - 1] & 0xc7) == 0x05 and q + 4 <= p:
            found.append((q, (text[q - 1] >> 3) & 7))
    if not found:
        raise ShapeError("0x%x 之前 %d 字节内没有 mov r32,[SCOREFILE_PTR]" % (p, window))
    return found[-1][1]


def find_unlock_sites(text, text_va):
    """扫 .text 里所有 imm32 == 0x5f588 的位置，按能否解成 SIB 读分成两堆。

    返回 (reads, others)：reads 是 10 处可改写的读；others 是撞上同一值但**不是**
    这种形态的（写入点 0x418e04、unlock_all 的 lea、以及 jcc rel32 的假阳性），
    全部列出来让人过目——它们要么走断点，要么本来就不该动。
    """
    needle = struct.pack("<I", UNLOCKED_OFF)
    reads, others = [], []
    p = text.find(needle)
    while p >= 0:
        va = text_va + p
        start = p - 3                         # op modrm sib 之后才是 disp32
        try:
            d = decode_sib_byte_op(text[start:start + 8])
            sf = scorefile_reg_before(text, start)
            if sf == d["base"]:
                d["keep"] = d["index"]
            elif sf == d["index"]:
                d["keep"] = d["base"]
            else:
                raise ShapeError("0x%06x：存档指针在 %s，既不是 base 也不是 index" % (va - 3, _REG32[sf]))
            reads.append((va - 3, d, text[start:start + d["len"]]))
        except (ShapeError, IndexError):
            d = None
        if d is None:
            others.append((va, text[p - 3:p + 5].hex()))
        p = text.find(needle, p + 1)
    return reads, others


def encode_unlock_rewrite(d):
    """从解出来的原指令算改写后的 (前缀字节, 后缀字节)；中间 4 字节是影子数组地址。"""
    modrm = (2 << 6) | (d["reg"] << 3) | d["keep"]
    pre = bytes([d["op"], modrm])
    post = bytes([d["imm"]]) if d["imm"] is not None else b""
    post += b"\x90" * (d["len"] - (len(pre) + 4 + len(post)))
    assert len(pre) + 4 + len(post) == d["len"]
    return pre, post


def emit_unlock_binhacks(reads):
    """战线 D 的 9 处读。expected 是 exe 原字节；code = 前缀 + <codecave> + 后缀。"""
    B = {}
    for va, d, raw in reads:
        pre, post = encode_unlock_rewrite(d)
        B["unlock_%06x" % va] = {
            "addr": "0x%06x" % va,
            "code": pre.hex() + "<codecave:%s>" % UNLOCKED_CAVE + post.hex(),
            "expected": raw.hex(),
            "title": "unlocked_cards 读 → 影子数组：%s [%s+%s+0x5f588] → [%s+SHADOW]（%s = 存档指针）" % (
                {0x3a: "cmp r8,m8", 0x38: "cmp m8,r8", 0x8a: "mov r8,m8", 0x80: "cmp m8,imm8"}[d["op"]],
                _REG32[d["base"]], _REG32[d["index"]], _REG32[d["keep"]],
                _REG32[d["index"] if d["keep"] == d["base"] else d["base"]]),
        }
    return B


def emit_unlock_breakpoints(text, text_va):
    """战线 D 的三个断点。expected 从 exe 现取，cavesize = 指令长。"""
    def bp(name, va, n, title):
        raw = text[va - text_va:va - text_va + n]
        return name, {"addr": "0x%06x" % va, "cavesize": n, "expected": raw.hex(), "title": title}
    return dict([
        bp("ce_unlock_write", UNLOCK_WRITE, 8,
           "mark_obtained 的写 → BP_ce_unlock_write：影子[id]=1；id<57 放行原指令写原生存档，否则写 side-car"),
        bp("ce_save_loaded", SAVE_LOADED, 6,
           "ScoreFile__load 尾段 → BP_ce_save_loaded：影子[0..56] ← 原生存档，[57..] ← side-car"),
        bp("ce_unlock_all", UNLOCK_ALL, 6,
           "ScoreFile__unlock_all → BP_ce_unlock_all：影子[0..55]=1（镜像紧接着的 memset）"),
    ])


def find_order_sites(text, text_va):
    """扫 .text 里落在 [顺序表基, 尾界] 的 imm32：6 处引用 + 1 处尾界 + 1 处别的表（排除）。

    返回 [(va_of_instruction, prefix_bytes, value)]。前缀 = 该指令 imm32 之前的字节，
    由 x86imm 归类保证它确实是一条以该 imm32 收尾的指令。
    """
    out, seen = [], []
    for v in range(ORDER_TABLE, ORDER_END + 1, 4):
        needle = struct.pack("<I", v); p = text.find(needle)
        while p >= 0:
            va = text_va + p
            if va in ORDER_EXCLUDE:
                seen.append(va)
            else:
                info = classify(text, p, text_va, 0)          # 不认识就抛 → 停
                ins_va = info["va"]; pre = text[ins_va - text_va:p]
                out.append((ins_va, pre, v))
            p = text.find(needle, p + 1)
    if sorted(seen) != sorted(ORDER_EXCLUDE):
        raise ShapeError("顺序表：预期要排除的站点没扫到：%s" % [hex(x) for x in ORDER_EXCLUDE])
    if len(out) != 7 or sum(1 for _, _, v in out if v == ORDER_END) != 1:
        raise ShapeError("顺序表：扫到 %d 处（应 6 引用 + 1 尾界）" % len(out))
    return sorted(out)


def emit_order_binhacks(order_sites):
    """顺序表 6 处引用 → cave，尾界 → cave+0x3fc（255 项）。全部只换 imm32。"""
    B = {}
    for va, pre, v in order_sites:
        off = CE_MAX_ROWS * 4 if v == ORDER_END else v - ORDER_TABLE
        assert off == 0 or v == ORDER_END
        ref = "<codecave:%s%s>" % (ORDER_CAVE, ("+%x" % off) if off else "")
        B["order_%06x" % va] = {"addr": "0x%06x" % va, "code": pre.hex() + ref,
                                "expected": (pre + struct.pack("<I", v)).hex(),
                                "title": "显示顺序表 → codecave（%s）" % ("尾界 = 255 项" if v == ORDER_END else "引用")}
    return B


MENU_SITES = (   # zAbilityMenu：(地址, 原字节, 新字节)。全部同长。
    (0x413817, "68fc130000", None), (0x413831, "68fc130000", None), (0x413abb, "68fc130000", None),   # 大小
    (MENU_CLEAR, "bf38000000", "bfff000000"),                                                            # 清 255 槽
    (0x4145d2, "898618fbffff", "8986100c0000"),   # __card_ids[i] 经 +0x7ec 游标：-0x4e8 → +0xc10
    (0x414b3f, "89b018f7ffff", "89b010080000"),   # __card_ids[n] 经 +0xbec 游标：-0x8e8 → +0x810
) + tuple((a, None, None) for a in (0x414b81, 0x414beb, 0x414e9f, 0x414eba, 0x415049, 0x415115, 0x415129,
                                     0x41514a, 0x4151ef, 0x41520c, 0x4152b4, 0x4152d5, 0x415868, 0x415e83))


def _pool_header_row(va, exp, off):
    """把一条池站点编成 DLL 能自查的记录：改后字节（disp32 位置置 0）+ disp32 的位置与加数。"""
    raw = bytes.fromhex(exp)
    if raw[0] == 0x68:                       # push <字节数>：不依赖 cave
        want, disp_at, uses_cave, add = bytearray(b"\x68\x00\x00\x00\x00"), 1, 0, POOL_SLOTS * 4
    elif raw[0] == 0x8d:                     # mov eax,<cave>; nop
        want, disp_at, uses_cave, add = bytearray(b"\xb8\x00\x00\x00\x00\x90"), 1, 1, 0
    else:                                    # mov 的 modrm 去掉 ebp 基址，disp32 紧随 SIB
        want, disp_at, uses_cave, add = bytearray([raw[0], raw[1] & 0x3f, raw[2], 0, 0, 0, 0]), 3, 1, off
    assert len(want) == len(raw)
    return "    { 0x%06x, %d, %d, %d, { %s }, 0x%x }," % (
        va - 0x400000, len(want), disp_at, uses_cave,
        ", ".join("0x%02x" % b for b in want) + ", 0" * (8 - len(want)), add)


def emit_pool_codecave():
    """商店随机池的搬迁目标。只给 size ⇒ thcrap 清零（同 th18_card_unlocked）。"""
    return {POOL_CAVE: {"size": "0x%x" % (POOL_SLOTS * 4), "access": "RW",
                        "title": "商店随机池搬迁目标（%d dword；原生是 ebp-0x8c4 的 560 格栈缓冲）" % POOL_SLOTS}}


def emit_pool_binhacks(text, text_va):
    """9 处，全部原地等长。x86 的「无基址 + 变址 + disp32」与「ebp + 变址 + disp32」同长，
    所以 5 处 mov 与 1 处读都是 7 → 7；两处 lea 6 字节改成 `mov eax,imm32` 5 字节 + 一个 nop。"""
    B = {}
    for va, exp, off in POOL_SITES:
        raw = bytes.fromhex(exp)
        got = text[va - text_va: va - text_va + len(raw)]
        if got != raw:
            raise ShapeError("0x%06x：exe 里是 %s，表里写的是 %s" % (va, got.hex(), exp))
        ref = "<codecave:%s%s>" % (POOL_CAVE, ("+%x" % off) if off else "")
        if raw[0] == 0x68:                                  # push imm32（memset 大小）
            code = "68" + "%08x" % 0                        # 占位，下面换成真实字节数
            code = "68" + "".join("%02x" % b for b in (POOL_SLOTS * 4).to_bytes(4, "little"))
        elif raw[0] == 0x8d:                                # lea eax,[ebp+disp32] → mov eax,<cave>; nop
            code = "b8" + ref + "90"
        else:                                               # mov 的 modrm：mod 10 → mod 00（去掉 ebp 基址）
            code = "%02x%02x%02x" % (raw[0], raw[1] & 0x3f, raw[2]) + ref
        B["pool_%06x" % va] = {"addr": "0x%06x" % va, "code": code, "expected": exp,
                               "title": "商店随机池 → codecave（%d dword，栈缓冲只有 560）" % POOL_SLOTS}
    return B


def emit_menu_binhacks(text, text_va):
    """zAbilityMenu 扩容：3 处大小、16 处 __card_ids、1 处清理上界。新旧字节各自从 exe 现取/现算。"""
    new_size = MENU_SIZE + CE_MAX_ROWS * 4
    B = {}
    for va, exp, new in MENU_SITES:
        off = va - text_va
        if exp is None:                       # +0x304 的直接引用：disp32 在指令末 4 字节，长度靠归类
            info = classify(text, off + 2 if text[off] == 0x8d else off + 3, text_va, 0)
            n = info["len"]; raw = text[off:off + n]
            d = raw.find(struct.pack("<I", CARD_IDS_OLD))
            assert d >= 0 and d + 4 == n, "0x%06x：不是以 +0x304 收尾的指令" % va
            new_b = raw[:d] + struct.pack("<I", CARD_IDS_NEW)
        else:
            raw = bytes.fromhex(exp)
            if text[off:off + len(raw)] != raw:
                raise ShapeError("0x%06x：exe 里是 %s" % (va, text[off:off + len(raw)].hex()))
            new_b = bytes.fromhex(new) if new else b"\x68" + struct.pack("<I", new_size)
        assert len(new_b) == len(raw)
        B["menu_%06x" % va] = {"addr": "0x%06x" % va, "code": new_b.hex(), "expected": raw.hex(),
                               "title": "zAbilityMenu 扩容 / __card_ids → +0x%x" % CARD_IDS_NEW}
    return B


def emit_text_breakpoints(text, text_va):
    """战线 E 第一块：三处 imul 挂断点。expected 写死在表里，这里再与 exe 核对一遍。"""
    out = {}
    for name, va, n, exp, title in TEXT_SITES:
        raw = text[va - text_va:va - text_va + n]
        if raw.hex() != exp:
            raise ShapeError("%s @ 0x%06x：exe 里是 %s，表里写的是 %s" % (name, va, raw.hex(), exp))
        out[name] = {"addr": "0x%06x" % va, "cavesize": n, "expected": exp,
                     "title": "文案重定向 → BP_%s：id<57 照算 id*0x1c0，否则指向 DLL 的扩展文案缓冲（%s）" % (name, title)}
    return out


def verify_unlock_binhack(name, bh, text, text_va):
    """对账战线 D 的一条：把 expected（原）和 code（新）各自解开，逐字段比。"""
    bad = []
    va = int(bh["addr"], 16); off = va - text_va
    exp = bytes.fromhex(bh["expected"])
    if text[off:off + len(exp)] != exp:
        return ["%s：exe 里是 %s" % (name, text[off:off + len(exp)].hex())]
    code = bh["code"]
    m = re.fullmatch(r"([0-9a-f]*)<codecave:%s>([0-9a-f]*)" % UNLOCKED_CAVE, code)
    if not m:
        return ["%s：code 不是「前缀 + <codecave:%s> + 后缀」" % (name, UNLOCKED_CAVE)]
    pre, post = bytes.fromhex(m.group(1)), bytes.fromhex(m.group(2))
    rendered = pre + b"\0\0\0\0" + post
    if len(rendered) != len(exp):
        bad.append("%s：code 渲染 %d 字节 != expected %d" % (name, len(rendered), len(exp)))
        return bad
    try:
        o = decode_sib_byte_op(exp)
        n = decode_disp32_op(rendered)
    except ShapeError as e:
        return ["%s：解码失败 %s" % (name, e)]
    if o["disp"] != UNLOCKED_OFF:
        bad.append("%s：原指令 disp 不是 0x5f588" % name)
    if n["op"] != o["op"] or n["reg"] != o["reg"] or n["imm"] != o["imm"]:
        bad.append("%s：opcode / reg / imm8 变了" % name)
    sf = scorefile_reg_before(text, off)
    keep = {o["base"], o["index"]} - {sf}
    if len(keep) != 1 or n["index"] not in keep:
        bad.append("%s：★ 留下的寄存器不对：新指令用 %s，存档指针在 %s，原指令 [%s+%s]"
                   % (name, _REG32[n["index"]], _REG32[sf], _REG32[o["base"]], _REG32[o["index"]]))
    tail = rendered[n["len"]:]
    if tail != b"\x90" * len(tail):
        bad.append("%s：补位不是 nop" % name)
    return bad


# ---- 行为 SDK（SDK.md §1、§6）：两个断点 ----
SDK_SITES = [
    ("ce_card_bind",  0x412cec, 6, "895e048b4728",
     "allocate_new_card 公共尾段 mov [esi+4],ebx（esi=卡对象, ebx=id）+ mov eax,[edi+0x28]：登记了行为的 id 换虚表"),
    ("ce_item_score", 0x446cf6, 6, "8d872c0c0000",
     "collect_money_item：esi=道具身价，弹窗与计分之前；沿卡链表调 on_item_score(&esi)"),
    ("ce_item_score_below", 0x446caa, 8, "8b45f8052c0c0000",
     "collect_money_item 0x446b00 的**线下分支**计分汇合点：mov eax,[ebp-8] / add eax,0xc2c（8 字节、无相对寻址）。"
     "一手：0x446c01 comiss + 0x446c04 jae 0x446cb4 / 0x446c0a cmp [edi+0xc74],3 + 0x446c11 je 0x446cb4 都不成立时"
     "（= 自机在回收线**下方**且道具非真空飞行）走这一支算分，esi 已算好最终身价，走到 0x446cb2 `jmp 0x446cfc` "
     "**绕过** `ce_item_score` 那个断点 ⇒ 只挂 0x446cf6 会漏掉线下收的金钱（110/111 的 PV 注入对它完全不生效）。"
     "handler = 与 ce_item_score 同一份（两个站点互斥、每次收集只走一支 ⇒ 事件仍是一颗道具一次）。"),
    ("ce_item_money", 0x446d28, 6, "ff0530cd4c00",
     "collect_money_item：inc [MONEY_TOTAL]（下一条 inc [MONEY]）；沿卡链表调 on_item_money(&bonus)，两个全局一起 += bonus"),
    ("ce_bomb_spent", 0x4203bc, 5, "a1c0f24c00",
     "do_bomb 里 consume_bomb(0x4574d0) **刚返回**那一条：mov eax,ds:0x4cf2c0，5 字节绝对寻址无相对量。"
     "0x4574d0 全库只有 0x4203b7 一个调用方，所以这里不多不少覆盖每一次炸弹消耗；沿卡链表调 on_bomb_spent()"),
    ("ce_enemy_drop", 0x430510, 6, "558bec83ec20",
     "Enemy__drop_items_and_notify_cards 入口（thiscall，**ecx = zEnemyDrop = enemy+0x623c，不是敌人对象**；"
     "一手 0x430499 `lea ecx,[edi+0x623c]`、ECL dropItems 0x42d0c0 同）：push ebp/mov ebp,esp/sub esp,0x20，无相对寻址。"
     "在引擎撒附加表**之前**沿卡链表调 on_enemy_drop_pre(counts)，counts = zEnemyDrop+0x04 起的 20 个 int32（type 1..0x13，"
     "主掉落 +0x00 已经撒过）；卡循环之后 SDK 施加掉落乘法层 ce_drop_apply()（EVENTS-CONTRACT §2）"),
    ("ce_enemy_shot_slot", 0x433ffa, 8, "f30f108f44510000",
     "敌人 ECL opcode 0x259（etOn、handler 0x433f34）里「发弹三步」的汇聚点：movss xmm1,[edi+0x5144]（8 字节、"
     "**窗内无相对寻址**）。edi = &enemy->data（enemy+0x122c，一手 0x430d30 `add ecx,0x122c`）、"
     "槽号 i 在 [ebp-0x56c]（0x433f3d 写）、esi = &data + i*0x488（0x433fcf/0x433fd9）。"
     "handler 返回非 0 时 regs->retaddr = 0x43402d（= 该 opcode 的收尾 `jmp 0x436417`）⇒ **本帧不发射这一槽**，"
     "跳过 et_protect_range 的 set / call 0x427810(__fire) / 复位三步。零内存写、不动敌人生命周期。"
     "详见 local/_scratch/research-108-shot-slot-off.md"),
    # collect-2x 四断点（AUDIT §Z；辉针城卡）：窗口单条/两条指令、无相对寻址、窗内无跳转目标 ——
    # 逐条与原始 exe 字节对过（emit_sdk_breakpoints 每跑必验），objdump aligned 扫过窗口边界。
    ("ce_item_frame", 0x446ee3, 7, "833da4f24c0000",
     "ItemManager tick 0x446ec0 的门卫 CMP（cmp dword [0x4cf2a4],0，**单条 7 字节**）：每个正常游玩帧、道具池 body 前到达一次"
     "（菜单/暂停帧跳走不经过）；handler 不读 CMP 结果、不改标志。沿卡链表调 on_item_frame()"),
    ("ce_item_collect", 0x44625f, 6, "8b97780c0000",
     "collect 分派前一条 mov edx,[edi+0xc78]（edi = 道具槽，**单条 6 字节**）：此时本道具的副作用都没发生"
     "（分值未入 SCORE）。handler 从**内存** *(uint32_t*)(edi+0xc78) 读 type（EDX 还没写），不是读寄存器。"
     "沿卡链表调 on_item_collect(card, type, item)"),
    ("ce_item_commit", 0x44658a, 6, "f30f10166a25",
     "共享收尾起点：movss xmm2,[esi]（4 字节）+ push 0x25（2 字节），本道具分值已按 type 计入 SCORE、"
     "公共收尾（清 state、接 commit tail）之前；各收集路线（线/半径）在此汇聚。两条都无相对寻址。"
     "沿卡链表调 on_item_commit()"),
    ("ce_shot_damage", 0x45f2a2, 7, "b867666666f7ee",
     "enm_compute_damage_sources 0x45f0f0 里可计分命中路径：mov eax,0xee666667 + imul esi（两条，都无相对寻址；"
     "0x45f2dc 因 guard-fail 路 0x45f14e jmp 0x45f2de 落在窗内而**被否**，此窗已对齐 objdump 确认无窗内跳转目标）。"
     "esi = 本帧本敌伤害（钳 cap 后 ≥1），ebx = 帧基 → pos 在 [ebx+8]（敌世界坐标 x,y）、param_6 在 [ebx+0x1c]。"
     "沿卡链表调 on_shot_damage(card, dmg, pos)"),
    ("ce_bomb_gate", 0x45c048, 7, "f60534a44c0002",
     "Player__on_tick__body 0x45be90 的普通炸弹分支：test BYTE PTR ds:0x4ca434,0x2（7 字节，输入上升沿 bit1）。"
     "handler 沿卡链表问 on_bomb_gate()；任一卡返回 1 时用 retaddr 跳到 0x45c056 跳过 call do_bomb 0x45c051。"
     "只在咆哮 ready/hypering 的卡返回 1；其余帧/无卡时放行原指令（真实炸弹不受影响）。"),
    ("ce_enemy_death", 0x430240, 6, "558bec83ec14",
     "FUN_00430240（SDK 名 Enemy__die）入口：push ebp/mov ebp,esp/sub esp,0x14，无相对寻址。"
     "thiscall，ecx = zEnemy；**引擎撒主掉落之前**（0x430493 才是撒、0x43049f 才通知卡）、链表仍挂着，"
     "恒定 mov eax,1 返回 ⇒ 一敌一生正好一次。handler 用 regs->retaddr 分死因"
     "（0x42f3b7 = 打死主路 / 0x42ee90 = 炸弹秒杀 / 0x430f59 = ECL die / killAll 三兄弟），"
     "沿卡链表调 on_enemy_death(&ev)。详见 docs/superpowers/specs/2026-09-12-shared-enemy-death-event-design.md"),
    ("ce_shot_set_index", 0x45ebfb, 7, "8bbc97e0000000",
     "Player__tick_shooting_state 0x45ea00 查主炮 shooterset 前的 mov edi,[edi+edx*4+0xe0]（7 字节，无相对寻址）。"
     "edx = 本帧 fire-time set index（0..9 主炮；eax = 火力档），ebp-8 暂存同一 set index，随后 0x45ec69 会打包进 bullet+0xac。\n"
     "handler 沿卡链表问 on_shot_set_index(card, eax)：返回 >=0 时改写 edx 与 [ebp-8] 为咆哮组 base+power，放行原指令；"
     "不咆哮/无卡时返回 -1，原版主炮 0..9 一个字节都不动。"),
    ("ce_option_shot_gate", 0x40a9c3, 6, "8b550883ec0c",
     "Player__tick_shooters_for_ability_card 0x40a9c0 的序言后 `mov edx,[ebp+8]; sub esp,0xc`（两条完整指令，"
     "6 字节，无相对寻址 / 无窗内跳转目标）。选 6 字节是因为 thcrap 的 sourcecave 下限是 5 字节：上一版只取 "
     "3 字节的 `mov edx,[ebp+8]`，thcrap 直接以 `cavesize too small` 拒绝装载，SDK 门回读失败后触发 "
     "`restore_alloc_bound` 降级路径 —— 存档/编成里已有新 id 时会踩出「卡 id 变垃圾 / HUD 槽消失」。"
     "**不能用 `0x40a9cb` 的 6 字节 `je`**：thcrap 的 cave_fix 不重定位 `0f 84 rel32`，复制进 cave 后会跳飞。"
     "handler 返回 1 = 执行完整两条原指令；返回 0 且 retaddr=0x40aa9e 才跳过本组（`mov esp,ebp` 重设栈顶）。"
     "⚠ 当前 C 侧 `CE_OPTION_SHOT_GATE_ENABLE` 默认 0（不启用），只为让门回读通过并保留实现。"),
    # ★ 2026-09-10（天空璋卡）：引擎自己算完一次**子弹擦弹**之后那一条。ESI = 刚被擦到的弹
    # （0x42496f 已判过 `[esi+0x20] & 4` 没擦过、0x424984 才置位），位置在 esi+0x638。
    # 卡用它把季节点撒在**弹的位置**上（此前只能扫弹池猜「最近的已擦弹」，会撒到自机背后）。
    # ⚠️ 2026-09-10 临时摘掉（二分「游戏打不开」）：先确认它是不是元凶，确认后再放回。
    # ("ce_graze", 0x42497f, 5, "a1bcf24c00",
     # "FUN_004248a0 的子弹擦弹路径 …"),
]

def _sdk_rva(name):
    """按**名字**取 SDK 断点地址 —— 别再用 `SDK_SITES[i]` 下标：插一条就全体错位
    （2026-09-12 踩过：插入 `ce_enemy_death` 后 `CE_BP_SHOT_SET_INDEX_RVA` 变成了 0x030240）。"""
    for site in SDK_SITES:
        if site[0] == name:
            return site[1]
    raise KeyError("SDK_SITES 里没有断点 %r（sites_gen.h 生成时才发现 = 太晚）" % name)



def emit_sdk_breakpoints(text, text_va):
    out = {}
    for name, va, n, exp, title in SDK_SITES:
        raw = text[va - text_va:va - text_va + n]
        if raw.hex() != exp:
            raise ShapeError("%s @ 0x%06x：exe 里是 %s，表里写的是 %s" % (name, va, raw.hex(), exp))
        if n < 5:
            raise ShapeError("%s @ 0x%06x：cavesize=%d < thcrap 下限 5（短于 5 字节的站点会被拒绝装载，"
                             "SDK 门回读失败后走降级路径）" % (name, va, n))
        out[name] = {"addr": "0x%06x" % va, "cavesize": n, "expected": exp, "title": "行为 SDK → BP_%s：%s" % (name, title)}
    return out


# ---- 商店走两遍（AUDIT §P、engine/card/th18/05-shop-and-money.md §3.5）：两个断点，都放行原指令 ----
SHOP_SITES = [
    ("ce_shop_bought", 0x4183ea, 12, "6a066a006a068d8f28020000",
     "AbilityShop__on_tick 成交分支（状态已置 5）：push 6 / push 0 / push 6 / lea ecx,[edi+0x228]，无相对寻址；只记「本次进店成交」"),
    ("ce_shop_reopen", 0x443b05, 5, "a900000200",
     "GameThread__on_tick：test eax,0x20000（eax = GameThread+0xb0，esi = this）；店刚关且成交过且有名额 → eax |= 0x20000 再开一家"),
]


def emit_shop_breakpoints(text, text_va):
    out = {}
    for name, va, n, exp, title in SHOP_SITES:
        raw = text[va - text_va:va - text_va + n]
        if raw.hex() != exp:
            raise ShapeError("%s @ 0x%06x：exe 里是 %s，表里写的是 %s" % (name, va, raw.hex(), exp))
        out[name] = {"addr": "0x%06x" % va, "cavesize": n, "expected": exp, "title": "商店走两遍 → BP_%s：%s" % (name, title)}
    return out


def emit_test_patch():
    """patch-test：只在验证战线 B 时进栈。两个钩子：

    ① 0x407ee3 `movzx eax, byte [eax+esi+0x5f608]`（8 字节，reset_cards 读初始卡组的一格）
       挂断点 → BP_ce_test_deck：空槽(56) 依次换成 cards_dev.js `start_deck` 里的 id（默认 58）。
       不写存档、不改任何文件；卡组编成里把前几格清空，开局就会分配这些卡。
    ② 0x411469 `cmp [edi+0x28], 0x100`（7 字节）挂 thcrap 断点 → BP_ce_trace_alloc
       把每次 allocate_new_card(id, mode) 记进日志。定论用它，不靠肉眼。
    """
    # 初始卡组钩子改为断点（无手写机器码）：BP_ce_test_deck 执行原 movzx 的语义，读到空槽(56) 且
    # cards_dev.js 的 start_deck 还有 id 就换成下一个；返回 0 跳过原指令。esi = 槽序号（0 起）、eax = 存档偏移基。
    return {
        "breakpoints": {"ce_test_deck": {
            "addr": "0x%06x" % TEST_MOVZX, "cavesize": 8,
            "expected": "0fb6843008f60500",
            "title": "测试：reset_cards 读初始卡组，空槽 → cards_dev.js start_deck 里的下一个 id"},
        "ce_trace_alloc": {
            "addr": "0x%06x" % TEST_TRACE, "cavesize": 7,
            "expected": "817f2800010000",
            "title": "测试：记录每次 allocate_new_card(id, mode)"}},
    }


def verify_pool_binhack(name, bh, text, text_va):
    """逐条把 code 解回字节（<codecave:…> 当 4 字节占位），核三件事：
    expected 与 exe 一致、改后与改前等长、改动只发生在允许的位置（memset 大小的立即数、
    lea→mov 的 opcode、modrm 的 mod 位、以及 disp32）。"""
    out = []
    va = int(bh["addr"], 16); off = va - text_va
    exp = bytes.fromhex(bh["expected"])
    if text[off:off + len(exp)] != exp:
        out.append("%s：exe 里是 %s，expected 写的是 %s" % (name, text[off:off + len(exp)].hex(), bh["expected"]))
        return out
    code = bh["code"]
    n_ref = code.count("<")
    if n_ref > 1:
        out.append("%s：code 里有 %d 个 codecave 引用，应至多 1 个" % (name, n_ref))
        return out
    if n_ref:
        i, j = code.index("<"), code.index(">")
        pre, post = code[:i], code[j + 1:]
        if len(pre) % 2 or len(post) % 2:
            out.append("%s：code 的前/后缀不是整字节" % name)
            return out
        rendered = len(pre) // 2 + 4 + len(post) // 2
        head = bytes.fromhex(pre)
    else:
        rendered, head, post = len(code) // 2, bytes.fromhex(code), ""
    if rendered != len(exp):
        out.append("%s：code 渲染 %d 字节 != expected %d（thcrap 会静默跳过校验）" % (name, rendered, len(exp)))
        return out
    if exp[0] == 0x68:                                   # push imm32：opcode 必须原样
        if head[:1] != exp[:1]:
            out.append("%s：push 的 opcode 被改了" % name)
    elif exp[0] == 0x8d:                                 # lea eax,[ebp+disp32] → mov eax,imm32; nop
        if head != b"\xb8" or post != "90":
            out.append("%s：lea 应改成 `b8 <cave>` + 一个 nop，实得 %s…%s" % (name, head.hex(), post))
    else:                                                # mov：只许把 modrm 的 mod 10 改成 00
        if len(head) != 3 or head[0] != exp[0] or head[2] != exp[2]:
            out.append("%s：mov 的 opcode / SIB 被改了" % name)
        elif head[1] != (exp[1] & 0x3f) or (exp[1] >> 6) != 0b10:
            out.append("%s：modrm 改法不对（应 mod 10 → 00，reg/rm 不变）" % name)
    return out


def verify_patch(path, text, text_va):
    """对账：把**已生成的 patch 文件**逐条拿回真 exe 核对。

    生成器和被核对的对象分开，才算对账；只信生成器等于没查。
    三件事：
      ① `expected` 与 exe 里该地址的字节完全一致；
      ② `code` 渲染后的长度 == `expected` 的长度
         —— 不等时 thcrap 会**静默跳过校验**（binhack.cpp:1264），护栏白装；
      ③ `code` 的前缀（opcode 部分）与 `expected` 的前缀一致
         —— 只准换常量，不准换指令。
    """
    doc = json.load(open(path, encoding="utf-8"))
    bad = []
    for name, bp in (doc.get("breakpoints") or {}).items():
        va = int(bp["addr"], 16); off = va - text_va
        exp = bytes.fromhex(bp["expected"])
        if text[off:off + len(exp)] != exp:
            bad.append("断点 %s：exe 里是 %s" % (name, text[off:off + len(exp)].hex()))
        if bp["cavesize"] != len(exp):
            bad.append("断点 %s：cavesize %d != expected 长度 %d" % (name, bp["cavesize"], len(exp)))
        if int(bp["cavesize"]) < 5:
            bad.append("断点 %s：cavesize %d < thcrap 下限 5（会被拒绝装载）" % (name, int(bp["cavesize"])))
    for name, bh in doc["binhacks"].items():
        if name.startswith("unlock_"):
            bad += verify_unlock_binhack(name, bh, text, text_va)
            continue
        if name.startswith("snd_"):
            # 音效表这批有「常量后还有字节」与「纯立即数」两种形状，通用分支认不了
            bad += verify_sound_binhack(name, bh, text, text_va)
            continue
        if name.startswith("pool_"):
            # 池搬迁有三种形状：纯立即数（memset 大小）、`mov eax,<cave>` + nop、
            # 以及把 modrm 的 mod 从 10 改成 00（去掉 ebp 基址）后接 disp32 —— 通用分支的
            # 「只许换常量、常量收尾」两条都对不上，单独核。
            bad += verify_pool_binhack(name, bh, text, text_va)
            continue
        if name.startswith(("alloc_", "grow_", "menu_")):
            va = int(bh["addr"], 16); off = va - text_va
            exp = bytes.fromhex(bh["expected"])
            if text[off:off + len(exp)] != exp:
                bad.append("%s：exe 里是 %s" % (name, text[off:off + len(exp)].hex()))
            continue
        va = int(bh["addr"], 16)
        off = va - text_va
        exp = bytes.fromhex(bh["expected"])
        if text[off:off + len(exp)] != exp:
            bad.append("%s：exe 里是 %s，expected 写的是 %s"
                       % (name, text[off:off + len(exp)].hex(), bh["expected"]))
            continue
        code = bh["code"]
        i = code.index("<")
        pre = code[:i]
        if code.count("<") != 1 or not code.rstrip().endswith(">"):
            bad.append("%s：code 里的表达式不是「前缀 + 单个 <…>」" % name)
            continue
        if len(pre) % 2:
            bad.append("%s：code 前缀不是整字节" % name)
            continue
        rendered = len(pre) // 2 + 4
        if rendered != len(exp):
            bad.append("%s：code 渲染 %d 字节 != expected %d 字节（thcrap 会静默跳过校验）"
                       % (name, rendered, len(exp)))
        if bytes.fromhex(pre) != exp[:len(pre) // 2]:
            bad.append("%s：code 换掉了 opcode 而不只是常量" % name)
    return len(doc["binhacks"]), bad


def _addr(a):
    """thcrap 地址两种写法：绝对 VA `0x45b170`，或 `Rx5b170`（相对 imagebase）。"""
    if isinstance(a, dict):
        a = a.get("addr")
    if not isinstance(a, str):
        return None
    t = a.strip()
    if t.lower().startswith("0x"):
        return int(t, 16)
    if t[:2].lower() == "rx":
        return 0x400000 + int(t[2:], 16)
    return None


def conflicts(ours_path, other_paths):
    """和**同一 patch 栈里其它 patch** 的 hackpoint 求区间交集。

    100 处 5–7 字节的写入点，任何一处被别的 binhack / breakpoint 覆盖都是冲突：
    后应用的那个会看到「expected 不匹配」然后**静默跳过**。

    base_tsa 把 `cavesize` 写在 `th18.js`、把 `addr` 写在 `th18.v1.00a.js`，
    所以要把传进来的文件按 hackpoint 名合并后再算。断点没给 cavesize 的按 5 算
    （thcrap 的下限，实际会被拒绝装载）。
    """
    doc = json.load(open(ours_path, encoding="utf-8"))
    mine = [(int(b["addr"], 16), int(b["addr"], 16) + len(bytes.fromhex(b["expected"])), n)
            for n, b in doc["binhacks"].items()]
    mine += [(int(b["addr"], 16), int(b["addr"], 16) + int(b["cavesize"]), "bp:" + n)
             for n, b in (doc.get("breakpoints") or {}).items()]
    merged = {}          # (sect, name) -> {addrs:[], size:int}
    for path in other_paths:
        d = json.load(open(path, encoding="utf-8"))
        for sect in ("binhacks", "breakpoints", "codecaves"):
            for name, item in (d.get(sect) or {}).items():
                if not isinstance(item, dict):
                    continue
                m = merged.setdefault((sect, name), {"addrs": [], "size": None, "src": []})
                m["src"].append(os.path.basename(path))
                if item.get("cavesize"):
                    m["size"] = int(item["cavesize"])
                elif item.get("expected"):
                    m["size"] = len(bytes.fromhex(item["expected"].replace(" ", "")))
                elif sect == "binhacks" and item.get("code"):
                    # 粗略：去掉 <…>/[…] 后每个表达式算 4 字节
                    import re
                    c = re.sub(r"<[^>]*>|\[[^\]]*\]", "XXXXXXXX", item["code"].replace(" ", ""))
                    m["size"] = len(c) // 2
                addrs = item.get("addr")
                if addrs is None:
                    continue
                for a in (addrs if isinstance(addrs, list) else [addrs]):
                    va = _addr(a)
                    if va is not None:
                        m["addrs"].append(va)
    found, checked = [], 0
    for (sect, name), m in merged.items():
        size = m["size"] or 5
        for lo in m["addrs"]:
            checked += 1
            for mlo, mhi, mname in mine:
                if lo < mhi and mlo < lo + size:
                    found.append((", ".join(sorted(set(m["src"]))), sect, name,
                                  "0x%06x+%d" % (lo, size), mname))
    return checked, found


def emit_header(sites, unlock_reads, order_sites, menu_binhacks):
    """给 DLL 生成站点表 —— **与行数无关**，所以一个 DLL 配所有 patch。

    每个站点只记：RVA、长度、opcode 前缀（已在 patch 的 expected 里）、
    类别、字段偏移。改后应有的 4 字节 = cave + 类别决定的基偏移 + 字段偏移，
    其中「类别决定的基偏移」里唯一随行数变的是 END（rows*0x34），
    而 rows 由 DLL 在运行时从 patch 已写入的字节里反推（selfcheck.c）。
    """
    KIND = {"start": 0, "hit": 1, "fallback": 2, "end": 3}
    lines = [
        "/* 由 sites.py 生成，不要手改。与行数无关。*/",
        "#pragma once",
        "#include <stdint.h>",
        "#define CE_ROW_SIZE    0x%x" % ROW_SIZE,
        "#define CE_ROW_COUNT   %d" % ROW_COUNT,
        "#define CE_NULL_ROW    %d" % NULL_ROW,
        "#define CE_MAX_ROWS    255",
        "#define CE_TABLE_RVA   0x%06x" % TABLE_RVA,
        "#define CE_CAVE_NAME   \"codecave:%s\"" % CODECAVE,
        "#define CE_JT_RVA      0x%06x" % JT_RVA,
        "#define CE_JT_COUNT    %d" % JT_COUNT,
        "#define CE_CASE56_RVA  0x%06x" % CASE56_RVA,
        "#define CE_JT_CAVE_NAME \"codecave:%s\"" % JT_CODECAVE,
        "#define CE_ALLOC_CMP_RVA 0x%06x" % (ALLOC_CMP - 0x400000),
        "#define CE_ALLOC_JMP_RVA 0x%06x" % (ALLOC_JMP - 0x400000),
        "enum { CE_K_START = 0, CE_K_HIT = 1, CE_K_FALLBACK = 2, CE_K_END = 3 };",
        "typedef struct { uint32_t rva; uint8_t len, prefix_len, prefix[3], kind; uint32_t field; } ce_site_t;",
        "static const ce_site_t CE_SITES[] = {",
    ]
    for va in sorted(sites):
        s_ = sites[va]
        rec = s_["rec"]
        pre = rec["bytes"][:rec["const_at"]]
        assert 1 <= len(pre) <= 3
        pfx = ", ".join("0x%02x" % b for b in pre) + (", 0" * (3 - len(pre)))
        base = {"start": TABLE_BASE, "hit": TABLE_BASE, "fallback": FALLBACK, "end": TABLE_END}[s_["part"]]
        lines.append("    { 0x%06x, %d, %d, { %s }, CE_K_%s, 0x%x },"
                     % (va - 0x400000, rec["len"], len(pre), pfx,
                        s_["part"].upper(), rec["value"] - base))
    lines += ["};", "#define CE_NSITES (sizeof(CE_SITES)/sizeof(CE_SITES[0]))", ""]
    # 战线 C 的 12 处：改后字节不依赖 codecave 地址，但依赖 rows —— DLL 按运行时 rows 现算。
    # 这里只给 RVA / 长度 / 原字节；DLL 里按同一套公式算出改后字节再比对。
    lines += ["typedef struct { uint32_t rva; uint8_t len; uint8_t kind; } ce_grow_t;",
              "enum { CE_G_SIZE = 0, CE_G_OWNED_LEA = 1, CE_G_STOSD = 2, CE_G_OWNED_DISP = 3, CE_G_SHOP_START = 4, CE_G_SHOP_END = 5 };",
              "#define CE_MGR_SIZE    0x%x" % MGR_SIZE,
              "#define CE_OWNED_NEW   0x%x" % OWNED_NEW,
              "static const ce_grow_t CE_GROW[] = {",
              "    { 0x0082d6, 5, CE_G_SIZE }, { 0x0082ec, 5, CE_G_SIZE }, { 0x00860a, 5, CE_G_SIZE },",
              "    { 0x007eb0, 6, CE_G_OWNED_LEA }, { 0x007eb6, 5, CE_G_STOSD }, { 0x012d42, 11, CE_G_OWNED_DISP },",
              "    { 0x016f8f, 5, CE_G_SHOP_START }, { 0x01744a, 5, CE_G_SHOP_START }, { 0x017535, 5, CE_G_SHOP_START },",
              "    { 0x01716b, 6, CE_G_SHOP_END }, { 0x017527, 6, CE_G_SHOP_END }, { 0x0175e7, 6, CE_G_SHOP_END },",
              "};", "#define CE_NGROW (sizeof(CE_GROW)/sizeof(CE_GROW[0]))", ""]
    # 战线 D：9 处读，改后字节 = pre + cave + post；三个断点的地址给 DLL 核对「已挂上」（首字节 e8）。
    lines += ["#define CE_UNLOCKED_OFF   0x%x" % UNLOCKED_OFF,
              "#define CE_UNLOCKED_CAVE_NAME \"codecave:%s\"" % UNLOCKED_CAVE,
              "#define CE_RETAIL_UNLOCKED 57",
              "#define CE_SAVEDIR_RVA    0x168c61   /* %%APPDATA%%\\ShanghaiAlice\\th18\\ ，游戏 chdir 用的缓冲，尾带反斜杠 */",
              "#define CE_SCOREFILE_PTR_RVA 0x0cf41c",
              "#define CE_BP_UNLOCK_WRITE_RVA 0x%06x" % (UNLOCK_WRITE - 0x400000),
              "#define CE_BP_SAVE_LOADED_RVA  0x%06x" % (SAVE_LOADED - 0x400000),
              "#define CE_BP_UNLOCK_ALL_RVA   0x%06x" % (UNLOCK_ALL - 0x400000),
              "#define CE_ABILITY_TXT_PTR_RVA 0x%06x" % (ABILITY_TXT_PTR - 0x400000),
              "#define CE_TEXT_ENTRY     0x%x" % TEXT_ENTRY,
              "#define CE_TEXT_ENTRIES   57      /* 原生 zAbilityText 装 57 张；第 57 张的位置已是尾部字段 */",
              "#define CE_BP_TEXT_NAME_RVA   0x%06x" % (TEXT_SITES[0][1] - 0x400000),
              "#define CE_BP_TEXT_DESC_RVA   0x%06x" % (TEXT_SITES[1][1] - 0x400000),
              "#define CE_BP_TEXT_NOTIFY_RVA 0x%06x" % (TEXT_SITES[2][1] - 0x400000),
              "#define CE_BP_CARD_BIND_RVA 0x%06x" % (_sdk_rva("ce_card_bind") - 0x400000),
              "#define CE_BP_ITEM_SCORE_RVA 0x%06x" % (_sdk_rva("ce_item_score") - 0x400000),
              "#define CE_BP_ITEM_MONEY_RVA 0x%06x" % (_sdk_rva("ce_item_money") - 0x400000),
              "#define CE_BP_BOMB_SPENT_RVA 0x%06x" % (_sdk_rva("ce_bomb_spent") - 0x400000),
              "#define CE_BP_ENEMY_DROP_RVA 0x%06x" % (_sdk_rva("ce_enemy_drop") - 0x400000),
              "#define CE_BP_ITEM_FRAME_RVA 0x%06x" % (_sdk_rva("ce_item_frame") - 0x400000),
              "#define CE_BP_ITEM_COLLECT_RVA 0x%06x" % (_sdk_rva("ce_item_collect") - 0x400000),
              "#define CE_BP_ITEM_COMMIT_RVA 0x%06x" % (_sdk_rva("ce_item_commit") - 0x400000),
              "#define CE_BP_SHOT_DAMAGE_RVA 0x%06x" % (_sdk_rva("ce_shot_damage") - 0x400000),
              "#define CE_BP_BOMB_GATE_RVA 0x%06x" % (_sdk_rva("ce_bomb_gate") - 0x400000),
              "#define CE_BP_SHOT_SET_INDEX_RVA 0x%06x" % (_sdk_rva("ce_shot_set_index") - 0x400000),
              "#define CE_BP_OPTION_SHOT_GATE_RVA 0x%06x" % (_sdk_rva("ce_option_shot_gate") - 0x400000),
              "#define CE_BP_ENEMY_DEATH_RVA 0x%06x" % (_sdk_rva("ce_enemy_death") - 0x400000),
              "#define CE_BP_ENEMY_SHOT_SLOT_RVA 0x%06x" % (_sdk_rva("ce_enemy_shot_slot") - 0x400000),
              "#define CE_BP_ITEM_SCORE_BELOW_RVA 0x%06x" % (_sdk_rva("ce_item_score_below") - 0x400000),
              "#define CE_BP_SHOP_BOUGHT_RVA 0x%06x" % (SHOP_SITES[0][1] - 0x400000),
              "#define CE_BP_SHOP_REOPEN_RVA 0x%06x" % (SHOP_SITES[1][1] - 0x400000),
              "#define CE_TEST_DECK_SAVE_OFF 0x5f608   /* reset_cards：byte [eax+esi+0x5f608] 初始卡组一格 */",
              "#define CE_ORDER_RVA      0x%06x" % ORDER_RVA,
              "#define CE_ORDER_COUNT    %d" % ORDER_COUNT,
              "#define CE_ORDER_CAVE_NAME \"codecave:%s\"" % ORDER_CAVE,
              "typedef struct { uint32_t rva; uint8_t pre_len, is_end; } ce_order_t;",
              "static const ce_order_t CE_ORDER[] = {",
              ] + ["    { 0x%06x, %d, %d }," % (va - 0x400000, len(pre), int(v == ORDER_END)) for va, pre, v in order_sites] + [
              "};", "#define CE_NORDER (sizeof(CE_ORDER)/sizeof(CE_ORDER[0]))",
              "#define CE_MENU_SIZE      0x%x" % MENU_SIZE,
              "#define CE_CARD_IDS_NEW   0x%x" % CARD_IDS_NEW,
              "#define CE_MENU_COUNT_RETAIL 56",
              "#define CE_MENU_COUNT_MAX 127   /* 两处 cmp r,imm8 */",
              "typedef struct { uint32_t rva; uint8_t imm_off, width; } ce_count_site_t;",
              "static const ce_count_site_t CE_MENU_COUNT[] = {",
              ] + ["    { 0x%06x, %d, %d }," % (a - 0x400000, o, w) for a, _, o, w in MENU_COUNT_SITES] + [
              "};", "#define CE_NMENU_COUNT (sizeof(CE_MENU_COUNT)/sizeof(CE_MENU_COUNT[0]))",
              "typedef struct { uint32_t rva; uint8_t len; uint8_t want[7]; } ce_menu_t;",
              "static const ce_menu_t CE_MENU[] = {",
              ] + ["    { 0x%06x, %d, { %s } }," % (int(b["addr"], 16) - 0x400000, len(bytes.fromhex(b["code"])),
                                                  ", ".join("0x%02x" % x for x in bytes.fromhex(b["code"])) + ", 0" * (7 - len(bytes.fromhex(b["code"]))))
                   for b in menu_binhacks.values()] + [
              "};", "#define CE_NMENU (sizeof(CE_MENU)/sizeof(CE_MENU[0]))",
              "#define CE_POOL_CAVE_NAME \"codecave:%s\"" % POOL_CAVE,
              "#define CE_POOL_SLOTS  %d" % POOL_SLOTS,
              "/* 每条：改后应有的字节（disp32 处先置 0），uses_cave 时 disp32 = cave + off，否则 = off */",
              "typedef struct { uint32_t rva; uint8_t len, disp_at, uses_cave, want[8]; uint32_t off; } ce_pool_t;",
              "static const ce_pool_t CE_POOL[] = {",
              ] + [_pool_header_row(va, exp, off) for va, exp, off in POOL_SITES] + [
              "};", "#define CE_NPOOL (sizeof(CE_POOL)/sizeof(CE_POOL[0]))",
              "typedef struct { uint32_t rva; uint8_t len, pre_len, post_len, pre[2], post[2]; } ce_unlock_t;",
              "static const ce_unlock_t CE_UNLOCK[] = {"]
    for va, d, raw in unlock_reads:
        pre, post = encode_unlock_rewrite(d)
        assert len(pre) == 2 and len(post) <= 2
        lines.append("    { 0x%06x, %d, %d, %d, { 0x%02x, 0x%02x }, { %s } },"
                     % (va - 0x400000, d["len"], len(pre), len(post), pre[0], pre[1],
                        ", ".join("0x%02x" % b for b in post) + (", 0" * (2 - len(post)))))
    lines += ["};", "#define CE_NUNLOCK (sizeof(CE_UNLOCK)/sizeof(CE_UNLOCK[0]))", ""]
    lines += [
        "/* ---- 音效表扩容（sound_sites.py）---- */",
        "#define CE_SND_CFG_ROWS    %d      /* 原生 84 行 */" % SND.CFG_ROWS,
        "#define CE_SND_NEW_N       %d      /* 新 id 0x%02x..0x%02x */"
        % (SND.NEW_N, SND.FIRST_ID, SND.FIRST_ID + SND.NEW_N - 1),
        "#define CE_SND_ROWS_TOTAL  %d" % SND.CFG_ROWS_N,
        "#define CE_SND_CFG_ROW     0x%x" % SND.CFG_ROW,
        "#define CE_SND_SLOT_SIZE   0x%x" % SND.SLOT_SIZE,
        "#define CE_SND_NAMES_N     %d      /* 原生 wav 名 / blob 槽数 */" % SND.NAMES_N,
        "#define CE_SND_NAMES_TOTAL %d" % SND.NAMES_N_N,
        "#define CE_SND_FIRST_ID    0x%02x" % SND.FIRST_ID,
        "#define CE_SND_LAZER2_SLOT 20     /* 0x45ff38 硬编码引用的槽；wav 下标应为 0x26 */",
        "#define CE_SND_LAZER2_WAV  0x26",
        '#define CE_SND_CAVE_CFG    "codecave:%s"' % SND.CAVE_CFG,
        '#define CE_SND_CAVE_NAMES  "codecave:%s"' % SND.CAVE_NAMES,
        '#define CE_SND_CAVE_SLOTS  "codecave:%s"' % SND.CAVE_SLOTS,
        '#define CE_SND_CAVE_BLOBS  "codecave:%s"' % SND.CAVE_BLOBS,
        "",
    ]
    return "\n".join(lines)



def emitted_breakpoint_names(text, text_va):
    """`gen` 会写进 patch 的全部断点名（与 gen 的拼装**同一份代码路径**）。

    ★ 为什么需要它：patch 里声明了 `"ce_xxx"` 而 DLL 没导出 `BP_ce_xxx` ⇒ thcrap 在
    `GetProcAddress` 那一步拿不到处理函数，**游戏里静默不生效**（不报错）。
    2026-09-12 真踩过一次：`ce_enemy_death` 加了站点与 handler、忘了 `th18_card_expand.def` 的导出。
    """
    doc = {"breakpoints": {"ce_gate": {}, "ce_test_deck": {}}}
    # `patch-test/th18.v1.00a.js`（emit_test_patch）里还有几个断点，也要有导出（否则 `_test` 里静默失效）
    doc["breakpoints"].update(emit_test_patch().get("breakpoints") or {})
    doc["breakpoints"].update(emit_unlock_breakpoints(text, text_va))
    doc["breakpoints"].update(emit_text_breakpoints(text, text_va))
    doc["breakpoints"].update(emit_sdk_breakpoints(text, text_va))
    doc["breakpoints"].update(emit_shop_breakpoints(text, text_va))
    doc["breakpoints"].update(emit_sound_breakpoints(text, text_va))
    return set(doc["breakpoints"])


def check_def_exports(names):
    """断点名 ↔ `th18_card_expand.def` 的 `BP_<名>` 双向对账。返回 (缺失, 多余)。"""
    path = os.path.join(HERE, "th18_card_expand.def")
    have = set()
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            for line in f:
                m = re.match(r"\s*(BP_[A-Za-z0-9_]+)\s*@\d+", line)
                if m:
                    have.add(m.group(1)[len("BP_"):])
    return (names - have), (have - names)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["list", "check", "gen", "verify", "conflicts"])
    ap.add_argument("others", nargs="*", help="conflicts：其它 patch 的 th18.v1.00a.js")
    ap.add_argument("--rows", type=int, default=ROW_COUNT,
                    help="新表行数（第 1 步用 58 = 行为零变化）")
    ap.add_argument("-o", "--out")
    ap.add_argument("--alloc", action="store_true",
                    help="战线 B（跳转表搬迁）。rows > 58 时自动开")
    a = ap.parse_args()

    if not os.path.exists(EXE):
        raise SystemExit("找不到样本：%s\n"
                         "游戏 exe 是 ZUN 版权商业软件，由你自己放进 local/。" % EXE)
    got = md5(EXE)
    if got != EXE_MD5:
        raise SystemExit("exe md5 不符：期望 %s，实得 %s" % (EXE_MD5, got))

    text, text_va, _ = load_text_section(EXE)
    inst, anchors = find_all(text, text_va)
    bad = check_invariants(inst)
    walks = find_walks(text, text_va)
    sites = collect_sites(inst, walks)
    uncovered = audit_uncovered(text, text_va, sites)
    secs, in_text, in_data = audit_interior(EXE, sites)
    unlock_reads, unlock_others = find_unlock_sites(text, text_va)
    # 战线 D 的全集：9 处 SIB 读 + 写入点 + unlock_all 的 lea；其余撞上 0x5f588 的只能是
    # jcc rel32 之类的假阳性（它们的「前 3 字节」解不成任何一种 SIB 读）。多一处少一处都停。
    unlock_known = {UNLOCK_WRITE + 3, UNLOCK_ALL + 2}
    unlock_unexplained = [(va, hx) for va, hx in unlock_others
                          if va not in unlock_known and not _looks_like_jcc_rel32(text, va - text_va)]
    if len(unlock_reads) != 9 or unlock_unexplained:
        bad.append("战线 D：0x5f588 的读有 %d 处（应 9）；无法解释的命中 %d 处：%s"
                   % (len(unlock_reads), len(unlock_unexplained),
                      ", ".join("0x%06x(%s)" % x for x in unlock_unexplained)))

    if a.cmd == "conflicts":
        if not a.others:
            raise SystemExit("用法：sites.py conflicts <其它 patch 的 th18.v1.00a.js>…")
        path = os.path.join(HERE, "..", "patch", "%s.js" % VERSION)
        n, found = conflicts(path, a.others)
        print("对照 %d 个外部 hackpoint，与我们的写入点/断点重叠的：%d" % (n, len(found)))
        for f in found:
            print("   ❌ %s / %s / %s @ %s  撞上  %s" % f)
        return 1 if found else 0

    if a.cmd == "verify":
        path = a.out or os.path.join(HERE, "..", "patch", "%s.js" % VERSION)
        n, bad = verify_patch(path, text, text_va)
        print("对账 %s：%d 条 binhack" % (os.path.relpath(path, REPO), n))
        n_alloc = sum(1 for k in json.load(open(path, encoding="utf-8"))["binhacks"] if k.startswith(("alloc_", "grow_", "unlock_", "order_", "menu_", "pool_", "snd_")))
        if n - n_alloc != len(sites):
            bad.append("patch 里 %d 条搬表 binhack，扫描器认为应有 %d 条" % (n - n_alloc, len(sites)))
        for b in bad:
            print("   ❌ " + b)
        print("✅ 全部对上（expected == exe 字节；code 与 expected 等长；只换常量）"
              if not bad else "❌ %d 处不符" % len(bad))
        return 1 if bad else 0

    if a.cmd in ("list", "check"):
        print("样本 %s  md5 %s ✅" % (VERSION, got))
        print("锚点 %d，完整匹配 %d 个查表实例，共用 LOOP_START %d 个"
              % (anchors, len(inst), sum(x["shared_start"] for x in inst)))
        print("表遍历（计数收尾，只搬 start）%d 处：%s"
              % (len(walks), ", ".join("0x%06x(count=%d @0x%06x)"
                                       % (w["parts"]["start"]["va"], w["count"], w["count_at"])
                                       for w in walks)))
        print("去重后待改站点 %d 处" % len(sites))
        if a.cmd == "list":
            print("\n查表实例（比较字段 / 返回字段）：")
            for i, x in enumerate(inst):
                p = x["parts"]
                print(" [%02d] 锚 0x%06x  ptr=%-3s idx=%-3s  cmp +0x%02x  ret +0x%02x%s"
                      % (i, x["anchor"], x["ptr_reg"], x["idx_reg"],
                         x["field_cmp"], x["field_ret"],
                         "  (共用 start)" if x["shared_start"] else ""))
                for k in PART_ORDER:
                    print("        %-9s 0x%06x  %-22s %s"
                          % (k, p[k]["va"], p[k]["text"], p[k]["bytes"].hex()))
        print("\n战线 D：unlocked_cards（0x5f588）的读 %d 处（→ 影子数组）：" % len(unlock_reads))
        for va, d, raw in unlock_reads:
            print("   0x%06x  %-24s [%s+%s+0x5f588] → [%s+SHADOW]" % (
                va, raw.hex(), _REG32[d["base"]], _REG32[d["index"]], _REG32[d["keep"]]))
        print("   其它命中（写入点 / unlock_all 走断点；jcc 假阳性不动）：%s"
              % ", ".join("0x%06x" % va for va, _ in unlock_others))
        print("\n⛔ 撞上同样的值但**不在查表骨架里**的 %d 处（这些不该改）：" % len(uncovered))
        for va, v, kind, desc in uncovered:
            print("   0x%06x  值 0x%06x  %-5s %s" % (va, v, kind, desc))
        sec_of = next((nm for nm, va, rp, rs in secs
                       if va <= TABLE_BASE < va + rs), "?")
        print("\n完整性检查之二：整张表区间 0x%06x..0x%06x（位于 %s 节）"
              % (TABLE_BASE, TABLE_BASE + ROW_COUNT * ROW_SIZE, sec_of))
        print("   .text 里未被 100 处站点覆盖的引用：%d 处%s"
              % (len(in_text), "" if not in_text
                 else " ← ❌ 漏了站点：" + ", ".join("0x%06x" % v for v, _ in in_text)))
        print("   数据节里指向表内某行的指针：%d 处%s"
              % (len(in_data), "" if not in_data
                 else " ← ❌ 这些指针也要跟着搬"))
        print()
        if in_text or in_data:
            bad.append("表区间还有 %d 处引用不在已知站点里" % (len(in_text) + len(in_data)))
        if bad:
            print("❌ 不变式校验失败：")
            for b in bad:
                print("   " + b)
            return 1
        bp_names = emitted_breakpoint_names(text, text_va)
        missing, stale = check_def_exports(bp_names)
        print("\n断点导出对账（patch 声明 %d 个 ↔ th18_card_expand.def 的 BP_*）：" % len(bp_names))
        if missing:
            print("   ❌ patch 里有断点但 DLL 没导出（游戏里会静默不生效）："
                  + ", ".join("BP_" + n for n in sorted(missing)))
            bad.append("断点缺导出：%s" % ", ".join("BP_" + n for n in sorted(missing)))
        if stale:
            print("   ❌ def 里有导出但 patch 没有对应断点（僵尸导出）："
                  + ", ".join("BP_" + n for n in sorted(stale)))
            bad.append("def 僵尸导出：%s" % ", ".join("BP_" + n for n in sorted(stale)))
        if not missing and not stale:
            print("   ✅ 全部对上（%d 个 BP_* 双向一致）" % len(bp_names))
        if bad:
            print("❌ 不变式校验失败：")
            for b in bad:
                print("   " + b)
            return 1
        print("✅ 25 个实例的两条不变式全部成立"
              "（循环比较字段 == 终止字段；命中臂字段 == 回退臂字段）")
        return 0

    if bad:
        raise SystemExit("校验没过，拒绝生成。")
    alloc = a.alloc or a.rows > ROW_COUNT
    doc = {"codecaves": emit_codecaves(a.rows, alloc), "binhacks": emit(sites, a.rows),
           # ★ 自检门。为什么是断点而不是 *_mod_post_init：plugin.cpp 用
           #   std::unordered_map::merge 把插件的钩子并进全局表，而 thcrap.dll 自己
           #   （steam_mod_post_init / motd_mod_post_init）先注册了 post_init 这个 key，
           #   后来者被静默丢弃。断点跑在游戏线程、在全部 init stage 之后，
           #   且它声明在本 patch（最后一个 stage）里——能触发就证明 stage 已应用完。
           "breakpoints": {"ce_gate": {
               "addr": "0x%06x" % GATE_ADDR, "cavesize": 5, "expected": "558bec6aff",
               "title": "自检门：ScoreFile__load 入口 → BP_ce_gate（填表 + 回读验证 + 写日志）"}}}
    if alloc:
        doc["binhacks"].update(emit_alloc_binhacks(a.rows))
        doc["binhacks"].update(emit_grow_binhacks(a.rows))
        doc["binhacks"].update(emit_unlock_binhacks(unlock_reads))
        doc["binhacks"].update(emit_order_binhacks(find_order_sites(text, text_va)))
        doc["binhacks"].update(emit_menu_binhacks(text, text_va))
        doc["codecaves"].update(emit_pool_codecave())
        doc["binhacks"].update(emit_pool_binhacks(text, text_va))
        doc["breakpoints"].update(emit_unlock_breakpoints(text, text_va))
        doc["breakpoints"].update(emit_text_breakpoints(text, text_va))
        doc["breakpoints"].update(emit_sdk_breakpoints(text, text_va))
        doc["breakpoints"].update(emit_shop_breakpoints(text, text_va))
        # ---- 音效表扩容（sound_sites.py / sound_emit.py）----
        # 与卡表行数无关，但同门控在 alloc 下：step1（58 行）保持「行为零变化」的保守回退态。
        doc["codecaves"].update(emit_sound_codecaves())
        doc["binhacks"].update(emit_sound_binhacks(text, text_va))
        doc["breakpoints"].update(emit_sound_breakpoints(text, text_va))
    txt = json.dumps(doc, indent=2, ensure_ascii=False)
    if a.out:
        out = a.out if os.path.isabs(a.out) else os.path.join(HERE, a.out)
        open(out, "w", encoding="utf-8").write(txt + "\n")
        print("写出 %d 条 binhack + %d 个 codecave -> %s（新表 %d 行 = 0x%x 字节）"
              % (len(doc["binhacks"]), len(doc["codecaves"]), out,
                 a.rows, a.rows * ROW_SIZE))
        hdr = os.path.join(HERE, "sites_gen.h")
        open(hdr, "w", encoding="utf-8").write(emit_header(sites, unlock_reads, find_order_sites(text, text_va), emit_menu_binhacks(text, text_va)))
        print("写出 DLL 站点表 -> %s（与行数无关）" % hdr)
        if True:   # 测试 patch 与行数无关，总是产出，便于入库
            tp = os.path.join(os.path.dirname(out), "..", "patch-test", "%s.js" % VERSION)
            os.makedirs(os.path.dirname(tp), exist_ok=True)
            open(tp, "w", encoding="utf-8").write(
                json.dumps(emit_test_patch(), indent=2, ensure_ascii=False) + "\n")
            print("写出测试 patch -> %s" % os.path.normpath(tp))
    else:
        print(txt)
    return 0


if __name__ == "__main__":
    sys.exit(main())
