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
# ---- 战线 F：pl0X.sht 组数扩容到 96（2026-09-14；AUDIT.md §CQ、NEXT.md §1w、CAPACITY.md §3.3）----
#
# 零售 `pl0X.sht` 的偏移数组只有 40 格（`[0xe0,0x180)`），本 mod 到 2026-09-13 已用满
# （原生 23 + APPEND 17）⇒ 扩容写法：**数组区原地长到 256 格（预留）**，数据区整体后移
# `(256-40)*4 = 0x360` 字节 ⇒ 数据区起点（= 解析器里硬编码的**重定位基址**）`0x180 → 0x4e0`。
# 宣布可用的是 96 格（= 影像头 `+0x02`，加载器上界）；计数是纯数据，以后加格子只改文件。
#
# ★ 为什么只需改这一处（与 §1w 最早那版「数组搬文件尾」的 14 条 binhack 是两条路）：
#   · 偏移项存的是**相对数据区**的偏移（`stored = 文件内偏移 − 数据区起点`）；装载器
#     `0x45a836 add [esi+edx],eax` 把 `eax = 影像 + 基点` 加上去 ⇒ 记录与基点**同时 +0xe0**
#     ⇒ 40 个零售偏移项的值、每条记录的字节**都不用改**；
#   · 13 个读者指令里的位移是 `sht + 0xe0 + set*4`，**数组起点没变** ⇒ 一条都不用改；
#   · 全 exe 里 `0x180` 的 memory disp 只有 6 处（`0x41b7fc` / `0x429988` / `0x4299a5` /
#     `0x43e2c0` / `0x45a830` / `0x4855f3`；字节级扫 `80 01 00 00` 再逐条反汇编分类），
#     与 pl0X 有关的**只有 `0x45a830` 这一处**（另 5 处的基址寄存器都不是 sht 影像）；
#   · 文件长大是安全的：影像内存 = `malloc(GetFileSize())`（`0x40217e` 一带：
#     `0x4ad088` GetFileSize → `0x491a34` 分配 → `0x4ad090` ReadFile），不是定长内嵌数组。
#   · 结构性恒等式（下面 `emit_sht_binhacks()` 每次生成都核）：新基点 == 老基点 + (96-40)*4。
SHT_DBASE_VA     = 0x45A830   # Player__initialize 的解析循环：lea eax,[edx+0x180]（6 字节）
SHT_DBASE_OLD    = 0x180      # 零售立即数（= 数据区起点，也是重定位加数）
SHT_SLOTS_RETAIL = 40         # 零售格数（影像头 +0x02）
SHT_SLOTS_NEW    = 96         # ★ 宣布可用格数（用户 2026-09-13 拍板）；= 影像头 +0x02
SHT_SLOTS_RESERVED = 256      # ★ 数组区容量（**预留**）：数据区起点按它算 ⇒ 以后加格子只改文件、exe 不动
SHT_ARRAY_OFF    = 0xE0       # 偏移数组起点 = 14 个读者指令里的位移（**本次一个都不改**）
# 数据区起点 = **预留**数组区的末尾；两条等价算法给同一个数（0x4e0），互为对账
SHT_DBASE_NEW    = SHT_DBASE_OLD + (SHT_SLOTS_RESERVED - SHT_SLOTS_RETAIL) * 4     # = 0x4e0
SHT_DBASE_EXPECT = "8d8280010000"      # lea eax,[edx+0x180] = 8d 82 + imm32
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

# ---- ★ 站点（thcrap 断点入场时刻）**记账口径**（不是硬闸；2026-09-19 订正）----
# 声明数 = 本 patch 送进 thcrap 的 `breakpoints` **总条数**（SDK_SITES + SHOP_SITES + 音效/文案/unlock 那批；
# 不含 base_tsa 自己声明的那些）。判据用「实际条数」而不是「某几个列表的长度」—— 2026-09-17 第一版就数错了。
#
# ★★ 出处与存疑（owner 2026-09-19 追问「这余量是真的吗，还是人为判定」时的结论）：
#   `24 条安全 / 26 条必崩` 这句的出处是 `local/MEMORY.md` §1.3h–§1.3l —— 也就是**后来变成
#   `AUDIT.md` §BE 详录 #41 的那条实验线**（§1.3h 对照局 / §1.3j 只挂 patch / §1.3k 栈漂移假说 /
#   §1.3l 证伪）。而 #41 的结论（"声明数越线 ⇒ 池崩"）**至今未复现**，机制上又已被 `§BE-DW` 的
#   **窗口**定案取代（一条裸 `ja rel32` 的窗口 ⇒ `cave_fix` 不修 ⇒ 跳进池子未写格 ⇒ `00 00`/写 `0x1`）。
#   ⇒ 这个数**不是硬安全线**，独立性很差（同一次存疑实验的快照）；`GATES.md` §8.1 也早就写着
#   「**条数不是决定因素**」（同一个 40 既崩过也跑通过）。
#
# ⇒ 要加站点时，**真判据是这三条**（都会在本文件/工具里给出读数）：
#   ① `native/tools/sitecheck.py <addr> <len> [--ret …]`：窗口 ≥5 字节 / 整指令边界 /
#      窗内没有 `cave_fix` 修不了的近相对分支（§BE-DW 那一类）/ 与既有站点·binhack 零重叠；
#   ② **池子实测余格**：启动日志的 `pool: 可执行页 N、跳板格 M、首个全零格 @…`（离线对账见
#      `tooling/poolmap.py`、`GATES.md` §8.5/§8.6）；
#   ③ **实跑一局**验收（这条永远要）。
#   下面这个常量只用于**记账与提醒**（超了不再判红，见 check 里那段）；新功能仍应优先并进
#   已有每帧/每敌/家务站点（METHOD.md 的铁律）。
SITE_BUDGET = 24          # 记账线（历史经验值，出处见上；超了 = 要记账 + owner 拍板，不是"必崩"）

# ★ 2026-09-17 定案（用户口径：gate = 站点预算闸；细节由我决定、以实测为准）
# 为什么要把"外部消费者"算进来：thcrap 的断点池是**进程级**的（分片实验已证 per-patch 是错的），
# 本机除我们之外还挂着 base_tsa（它自己的 th18.v1.00a.js 声明 18 条，handler 在 thcrap_tsa.dll）。
# 只要不把它算进来，闸看到的数字就永远是假的（我们 22 / 真实 40）。
# 覆盖方式：环境变量 CE_EXTERNAL_SITES="base_tsa=18,别的=3"；不设则用下表（已知清单）。
# ⚠ 口径修正（2026-09-18 实测对账）：base_tsa 的 th18.v1.00a.js 有 **18 个条目**，但其中两条
# （music_cmt / ruby_offset）是**双地址表** ⇒ 实际声明 **20 个地址**。池内实测 42 桩 = 我们 22 + 它 20 ✓
EXTERNAL_SITES = [("base_tsa (nmlgc)", 20, "repos/nmlgc/base_tsa/th18.v1.00a.js（18 条 entry = 20 个地址）")]
# 已知的"会崩"级别：2026-09-17 实测 total=40 崩过两次（池崩，Eip 落在 RESERVE 未提交区）。
# ⇒ 安全线必须**低于**它；先取 38（留 2 格），并把"实测安全线"当成后续用它测出来的目标。
SITE_TOTAL_KNOWN_BAD = 42   # 实测：该配置（我们 22 + base_tsa 20）崩过两次
SITE_TOTAL_BUDGET = 42      # 只作告警；条数不是触发条件（同配置既崩过也跑通过）

# ★★★ 记账（**owner 拍板** 2026-09-13）：站点 `ce_input_synth`（`th18:0x462966`）
#   —— 调试件「逐帧合成方向输入」（`cards/input_synth.{h,c}`）**唯一**的落笔点。
#
#   · 需求（owner 原话）：「**没有进 replay**」⇒ 要合成输入**进录像**，带工具的实验才可复现。
#   · **为什么必须新站点、不能并进任何既有站点**（这一条就是"值不值"的全部理由）：
#       录制读点在 `0x4629f7`（`held16 = (uint16)INPUT_HELD`），它住在 `ReplayManager` 的
#       **节点 `0x11`**；而本 mod 现有的**每一条**站点都在节点 ≥ `0x16`
#       （AbilityManager `0x16` / Player `0x17` / ItemManager `0x1e` …，见 `ce_item_frame` 的注释）。
#       ⇒ **没有任何一条既有站点早于录制读点**；而写 `INPUT_HELD` 又必须同时早于
#       `0x42abc0`（算边沿）与 `0x4629f7`（录录像）。唯一满足的窗口只剩 `0x462940` 函数体内那两条
#       —— `0x46295a`（写 raw 键盘）或 **`0x462966`（写 `INPUT_HELD`，本件取它）**。
#   · 记账口径 = `build_site_accounting()`（含测试归档与进程级外部占用；数与 `sites.py check` 一致）：
#       改动**前**：本 patch 声明 **33** 条（正式 31 + 测试归档 2）；进程级 = 33 + base_tsa 20 = **53**。
#       改动**后**：本 patch 声明 **34** 条（正式 32 + 测试归档 2）；进程级 = **54**。
#       `SITE_BUDGET` = 24 是**记账线**（超线不判红，见上面那段）⇒ 改动前后都在线外，本件 +1 由 owner 拍板。
#     ⚠ 任务书写的是「27 → 28」——那是 2026-09-13 早先的快照；此后别的会话又往
#       `_SDK_KEEP_NAMED` 里放回了 `ce_graze` / `ce_bullet_hit` 等（见那里的 ★ 段），
#       所以**现测**是 33 → 34。精确数一律以 `python3 sites.py check` 的输出为准（与本注释同步）。
#   · 三条真判据（本件的读数）：① `python3 tools/sitecheck.py 0x462966 5` ⇒ W1/W2/W4 ✅、
#     W3 = 「窗口首条 e8/e9 ⇒ cave_fix 会重定位」（fixable，**不需要 `no_replay`**）；
#     ② 启动日志的 `pool:` 实测格数（`sdk.c` 的 `ce_pool_count`）；③ 实跑一局（owner 验收）。


def external_sites():
    """进程里**别人**占用的断点条数（默认已知清单；可用 CE_EXTERNAL_SITES 覆盖）。"""
    env = os.environ.get("CE_EXTERNAL_SITES", "")
    if env:
        out = []
        for item in env.split(","):
            if "=" in item:
                nm, n = item.split("=", 1)
                try:
                    out.append((nm.strip(), int(n), "(来自 CE_EXTERNAL_SITES)"))
                except ValueError:
                    pass
        return out
    return list(EXTERNAL_SITES)


def build_site_accounting(text, text_va):
    """→ (rows, ours, external, total, verdict)
    rows 每行 = (类别, 条数, 出处)；verdict = None(通过) / 字符串(告警)。"""
    names = emitted_breakpoint_names(text, text_va)
    ours = sorted(names)
    formal = []
    test = []
    for n in ours:
        # 测试归档的断点（patch-test 里声明的那两个）单独算：不想要观察者时可以关掉，等于免费腾位
        (test if n in ("ce_test_deck", "ce_trace_alloc") else formal).append(n)
    ext = external_sites()
    total = len(formal) + len(test) + sum(n for _, n, _ in ext)
    rows = [("正式 patch（我们）", len(formal), "patch/th18.v1.00a.js"),
            ("测试归档（我们，可关）", len(test), "patch-test/th18.v1.00a.js")]
    for nm, n, src in ext:
        rows.append(("外部：" + nm, n, src))
    rows.append(("合计（进程级）", total, "← 闸的真正口径"))
    warn = None
    if total > SITE_TOTAL_BUDGET:
        warn = ("合计 %d > 安全线 %d —— 已知 %d 会崩过（池崩）。"
                "先用 deploy 之外的开关减档（关测试归档腾 2；或与同僚协调 base_tsa），"
                "或在 COMPAT-MATRIX §1 记账后再动。"
                % (total, SITE_TOTAL_BUDGET, SITE_TOTAL_KNOWN_BAD))
    elif len(formal) > SITE_BUDGET:
        warn = "我们自己的正式声明 %d > SITE_BUDGET %d" % (len(formal), SITE_BUDGET)
    return rows, len(formal), len(test), ext, total, warn
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
# 图鉴条目数 0x38 的 7 处：立即数一律 patch 不动，DLL 在门里按「56 + 已注册新卡数」现写。
#   其中两处是循环上界 `cmp r,imm8`（**符号**扩展）⇒ 立即数 ≥ 0x80 就变负、上界只能到 127
#   （旧账：条目数 128 = imm8 0x80 = −128 ⇒ 循环一圈不跑、图鉴整表空掉，AUDIT §AG 的症状链）。
#   2026-09-19 的做法**不是**把这两处改成断点（那时要 `e9 rel32` 指向运行时才知道地址的 cave，
#   而 thcrap 的 `code` 串只支持绝对地址占位、算不出 rel32），而是**原地换编码**：
#   把「有符号 32 位比较 + jl」改成「**无符号 8 位**比较 + jb」，逐字节同长、立即数位置不动：
#       0x4145e2  cmp eax,0x38 ; jl rel32  →  cmp al,0x38 ; jb rel32   （只改 2 字节）
#       0x4157cb  cmp edx,0x38 ; jl rel8   →  cmp dl,0x38 ; jb rel8    （只改 2 字节）
#   ⇒ 上界升到 255（= 表行数上限），DLL 写的那个立即数**还是一个字节**，写入点/宽度完全不变。
#   等价性（一手，ABILITY menu 的 tick `AbilityMenu__on_tick` = `th18:0x413af0`）：
#     两处比的都是**同一个循环计数器** `[ebp-0xc]`，初始化 `mov dword ptr [ebp-0xc],0`
#     （`th18:0x414377` / `th18:0x415737`），循环体里 `inc` 后再比。计数器恒在 0..255
#     ⇒ 低字节就是它自己，「无符号字节比较」与原来的「有符号 dword 比较」**逐值同真值**；
#     `jb`/`jl` 的跳转目标与位移一字节不动（`0f 8c`→`0f 82`、`7c`→`72` 同长同类，位移照抄）。
#   一手字节与反汇编、等价性论证、落地清单见 engine/card/th18/10-extensibility-limits.md §8.11。
MENU_COUNT_SITES = (        # (地址, 原字节, 立即数偏移, 宽度)
    (0x4137bb, "c787c401000038000000", 6, 4),   # initialize: [this+0x1c4] = 0x38
    (0x414394, "c7870003000038000000", 6, 4),   # 图鉴 fill: [this+0x300] = 0x38
    (0x41439e, "c787c401000038000000", 6, 4),   # 图鉴 fill: [this+0x1c4] = 0x38
    (0x4145e2, "83f838", 2, 1),                  # 图鉴 fill 循环: cmp eax,0x38
    (0x41570d, "b838000000", 1, 4),              # 行数 = 0x38 / 列数
    (0x4157cb, "83fa38", 2, 1),                  # 高亮循环: cmp edx,0x38
    (0x415817, "bf38000000", 1, 4),              # 退出时遍历 vm: mov edi,0x38
)
# 上面第 4/6 两处的**编码改写**（补足上面那段理由）：整条比较+jcc 一起换，原样进 expected，
# 立即数位置（+2）与宽度（1 字节）在两个串里完全相同 ⇒ DLL 的 7 处写入点一个都不用改。
# (地址, 原 9/5 字节, 改后 9/5 字节, 说明)
MENU_COUNT_BCMP = (
    (0x4145e2, "83f8380f8cd5fdffff", "80f8380f82d5fdffff", "图鉴 fill 循环上界：cmp eax,imm8 + jl rel32 → cmp al,imm8 + jb rel32"),
    (0x4157cb, "83fa387c80",         "80fa387280",         "高亮循环上界：cmp edx,imm8 + jl rel8 → cmp dl,imm8 + jb rel8"),
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


def emit_count_binhacks(text, text_va):
    """图鉴条目数上界的编码改写（`MENU_COUNT_BCMP`）：原地换编码，不搬 cave、不加断点。

    两条都**只改 2 个字节**（`83`→`80`、jcc 的第二个字节），长度与立即数位置不变：
      · `cmp r,imm8`（有符号）→ `cmp r8,imm8`（无符号）
      · `jl` → `jb`（同长、同位移：`0f 8c`→`0f 82` / `7c`→`72`）
    ⇒ 上界 127 → 255。expected 取整条（比较 + jcc），exe 里现读现比，形状不对就抛。
    """
    B = {}
    for va, exp, new, why in MENU_COUNT_BCMP:
        off = va - text_va
        raw = bytes.fromhex(exp)
        if text[off:off + len(raw)] != raw:
            raise ShapeError("0x%06x：exe 里是 %s（期望 %s）" % (va, text[off:off + len(raw)].hex(), exp))
        assert len(new) == len(exp)
        B["count_%06x" % va] = {"addr": "0x%06x" % va, "code": new, "expected": exp,
                                "title": why}
    return B


def count_op_changes(exp_hex, new_hex):
    """`MENU_COUNT_BCMP` 一行的「改了哪几字节」→ [(偏移, 改后字节)]。

    只允许两处、且形状固定（其余一律抛，别让 patch 悄悄改到别的地方）：
      · 偏移 0：`83`（cmp r,imm8）→ `80`（cmp r8,imm8）
      · jcc 的**第二字节**：`8c`→`82`（jl rel32 → jb rel32）或 `7c`→`72`（jl rel8 → jb rel8）
    立即数字节（偏移 2）**必须原样** —— DLL 按「偏移 2 / 宽度 1」现写。
    """
    exp, new = bytes.fromhex(exp_hex), bytes.fromhex(new_hex)
    if len(exp) != len(new):
        raise ShapeError("改后 %d 字节 != 原 %d 字节" % (len(new), len(exp)))
    diff = [(i, new[i]) for i in range(len(exp)) if exp[i] != new[i]]
    if len(diff) != 2 or diff[0] != (0, 0x80) or diff[1][1] not in (0x82, 0x72):
        raise ShapeError("改动不是「cmp 首字节 + jcc 第二字节」：%s" % diff)
    if exp[2] != new[2]:
        raise ShapeError("立即数字节被改了")
    return diff


def verify_count_binhack(name, bh, text, text_va):
    """`count_*` 的独立对账：通用分支要求「只换常量」，而这两条**就是要换 opcode**。

    核四件事：地址在 `MENU_COUNT_BCMP` 表里（patch 与生成器不许漂移）、exe 里是 expected、
    改后与改前**同长且只有那两字节动**、**立即数字节（偏移 2）一字不动**。
    """
    va = _addr(bh["addr"])
    got = [row for row in MENU_COUNT_BCMP if row[0] == va]
    if not got:
        return ["%s：地址不在 MENU_COUNT_BCMP 表里" % name]
    _, exp, new, _ = got[0]
    if bh["expected"] != exp or bh["code"] != new:
        return ["%s：patch 与 MENU_COUNT_BCMP 不一致（expected %s / code %s）" % (name, bh["expected"], bh["code"])]
    try:
        count_op_changes(exp, new)
    except ShapeError as e:
        return ["%s：%s" % (name, e)]
    off = va - text_va
    raw = bytes.fromhex(exp)
    if text[off:off + len(raw)] != raw:
        return ["%s：exe 里是 %s，expected 写的是 %s" % (name, text[off:off + len(raw)].hex(), exp)]
    return []


def emit_sht_binhacks(text, text_va):
    """pl0X.sht 组数扩容 —— **只有 1 条**：把解析器的数据区基址 `0x180` 换成 `0x4e0`。

    （`0x4e0 = 0xe0 + 256×4`：数组区**预留 256 格**，宣布可用 96 格；预留之后再加格子只改文件。）

    只换 `lea eax,[edx+imm32]` 的最后 4 字节（opcode+modrm = `8d 82` 原样），长度 6 → 6。
    ⚠ 三条结构性自检（缺一条就是「扩容只成功一半」那种静默错位）：
      ① exe 里那 6 字节必须还是 `8d8280010000`（`ShapeError` 而不是静默跳过 —— thcrap 的
         `expected` 对不上时**只打一行日志就跳过**，binhack.cpp:398，等于没打）；
      ② `SHT_ARRAY_OFF + SHT_SLOTS_RESERVED*4 == SHT_DBASE_NEW`（**预留**数组区必须**正好**顶到
         数据区，否则读者读出去 / 数据区第一项被压在数组下）；
      ③ `SHT_DBASE_NEW == SHT_DBASE_OLD + (SHT_SLOTS_RESERVED-SHT_SLOTS_RETAIL)*4`（与生成器同式）；
      ⑤ `SHT_SLOTS_NEW <= SHT_SLOTS_RESERVED`（宣布可用不能超过预留）。
    ④ 与生成物的交叉对账（`sht_ids.h` 的 `CE_SHT_DATA_BASE` / `CE_SHT_SLOTS`）在 `capcheck.py`，
       因为在 `gen` 之前 sht_ids.h 不一定存在（新 clone 上 `make gen` 不许依赖产物）。
    """
    if SHT_ARRAY_OFF + SHT_SLOTS_RESERVED * 4 != SHT_DBASE_NEW:
        raise ShapeError("shooterset 扩容：预留数组末尾 %#x != 数据区起点 %#x"
                         % (SHT_ARRAY_OFF + SHT_SLOTS_RESERVED * 4, SHT_DBASE_NEW))
    if SHT_DBASE_NEW != SHT_DBASE_OLD + (SHT_SLOTS_RESERVED - SHT_SLOTS_RETAIL) * 4:
        raise ShapeError("shooterset 扩容：数据区起点与数组区长大不同步（%#x vs %#x）"
                         % (SHT_DBASE_NEW, SHT_DBASE_OLD + (SHT_SLOTS_RESERVED - SHT_SLOTS_RETAIL) * 4))
    if not SHT_SLOTS_RETAIL <= SHT_SLOTS_NEW <= SHT_SLOTS_RESERVED:
        raise ShapeError("shooterset 扩容：宣布可用格数 %d 不在 [%d, %d] 内"
                         % (SHT_SLOTS_NEW, SHT_SLOTS_RETAIL, SHT_SLOTS_RESERVED))
    off = SHT_DBASE_VA - text_va
    raw = text[off:off + 6]
    if raw.hex() != SHT_DBASE_EXPECT:
        raise ShapeError("0x%06x 是 %s，应为 %s（数据区基址那处 LEA）"
                         % (SHT_DBASE_VA, raw.hex(), SHT_DBASE_EXPECT))
    code = raw[:2].hex() + struct.pack("<I", SHT_DBASE_NEW).hex()
    return {"sht_dbase_%06x" % SHT_DBASE_VA: {
        "addr": "0x%06x" % SHT_DBASE_VA, "code": code, "expected": SHT_DBASE_EXPECT,
        "title": "pl0X.sht 组数 40 → %d（数组区预留 %d 格）：数据区基址 0x%x → 0x%x"
                 "（数组 [0x%x,0x%x)，14 个读者的位移不改）"
                 % (SHT_SLOTS_NEW, SHT_SLOTS_RESERVED, SHT_DBASE_OLD, SHT_DBASE_NEW,
                    SHT_ARRAY_OFF, SHT_DBASE_NEW)}}


def verify_sht_binhack(name, bh, text, text_va):
    """对账一条 `sht_*`：expected 与磁盘原版 exe 逐字节一致；code 只换立即数、长度不变、前缀不动。

    这批是**纯 hex 的 code**（没有 `<…>` 表达式）⇒ `verify_patch()` 的通用分支会在
    `code.index("<")` 上抛 ValueError，所以单独一条分支（不是放行）。
    """
    bad = []
    va = int(bh["addr"], 16)
    off = va - text_va
    exp = bytes.fromhex(bh["expected"])
    if text[off:off + len(exp)] != exp:
        bad.append("%s：exe 里是 %s，expected 写的是 %s"
                   % (name, text[off:off + len(exp)].hex(), bh["expected"]))
        return bad
    code = bytes.fromhex(bh["code"])
    if len(code) != len(exp):
        bad.append("%s：code %d 字节 != expected %d 字节（thcrap 会静默跳过校验）"
                   % (name, len(code), len(exp)))
        return bad
    if code[:2] != exp[:2]:
        bad.append("%s：code 换掉了 opcode/modrm（只许换立即数）" % name)
    want = struct.pack("<I", SHT_DBASE_NEW)
    if code[2:6] != want:
        bad.append("%s：立即数是 %s，应为 %s（数据区基址必须 = SHT_DBASE_NEW）"
                   % (name, code[2:6].hex(), want.hex()))
    return bad


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
    ("ce_enemy_damage", 0x42f2cb, 6, "01b308500000",
     "EnemyData::step_game_logic 0x42ed40 里 `add [ebx+0x5008],esi`（ebx = zEnemyData* = zEnemy*+0x122c；"
     "esi = 本帧最终伤害，已过自机倍率/ECL 扩展/炸弹倍率）。"
     "6 字节、无相对寻址、窗内无跳转靶；0x42ed40 每敌每帧恰好一次（唯一调用方 0x42fff0）。"
     "站点在两道免伤闸（0x42f2b9 `test byte [ebx+0x5130],0x10` / 0x42f2c2 `cmp [ebx+0x50f4],0`）**之后** "
     "⇒ 语义 = 「HP 即将减少」；handler **必须返回 BP_EXEC_ORIGINAL**（那条 add 是引擎记账）。"
     "用途 = 113 的「Boss 随血量掉灵」（TH13 通道 A）。一手全链见 REPORT-BOSS-SPIRIT-DROP.md。"),
    ("ce_card_bind",  0x412cec, 6, "895e048b4728",
     "allocate_new_card 公共尾段 mov [esi+4],ebx（esi=卡对象, ebx=id）+ mov eax,[edi+0x28]：登记了行为的 id 换虚表"),
    # ★★ 2026-09-13 第二次修订：计分层的**注入点搬到入口**（原 `ce_item_score` 那两个站点改成纯观察者）。
    #    为什么搬：那两个站点都在**引擎算完身价之后**（`esi` = 引擎自己的最终值），handler 一写 `regs->esi`
    #    就把引擎刚做的高度系数（`0x446bf9`–`0x446cf3`）整个丢掉 ⇒ 实际结构 = `coeff × G_0 + ΣG_i`，
    #    而用户规格要的是 `coeff × (G_0 + ΣG_i)`。改 `ecx` = 改「引擎接下来要乘系数的那个输入」⇒ 系数自动乘在整笔上。
    # ★★ 2026-09-22 **恢复**（owner 2026-09-22 晚：「我估计其中的部分 bug 你重新查一下名额回收静默摘掉」）：
    #    这条 2026-09-16zr 的二分注释只写了「临时」，**从来没回填** —— 而它是**计分层唯一的落笔点**
    #    （`sdk.c` 的 `score-in:` 那一整套注入逻辑挂在这条上）⇒ 摘着 = 107/110/111/113 的
    #    `ce_score_declare` 全部**静默失效**（分值悄悄回到原生，日志上一句抱怨都没有）。
    #    恢复前逐项过 `sitecheck.py 0x446bf9 8`：W1/W2/W3/R/W4 全绿。
    ("ce_item_score_in", 0x446bf9, 8, "f30f108698790400",
     "collect_money_item 0x446b00 的**单一汇合点**：movss xmm0,[esi+0x47998]（回收线；8 字节、无相对寻址）。"
     "三条难度支（0x446b30 EASY / 0x446b86 其余 / 0x446ba3 EXTRA）与 `ja 0x446bf3` 那条都汇到这里；"
     "此刻 **ecx = 引擎钳位后的身价**（单位 1/1000 点，恒等于 [0x4ccd24]）、**esi = PLAYER**（0x446b0c 从 0x4cf410 载入）、"
     "**edi = ITEM**（= [ebp-8]）。handler 把它改成 `1000 × (G_0 + ΣG_i)` ⇒ 引擎自己那两段高度系数"
     "（满额 0x446cb4 / 线下折扣 0x446c17）就乘在**整笔**上，满额支的 `SCORE += floor(ecx/1000)` 精确给出我们声明的条数。"),
    # ⚗ 2026-10-01 回收名额：ce_item_score 是**纯观察者**（日志 `score-out: … delta=0` 已反复确认）
    #     ("ce_item_score", 0x446cf6, 6, "8d872c0c0000",
    #      "collect_money_item 的**满额支**出口 lea eax,[edi+0xc2c]（弹窗与 SCORE 入账之前，esi = 引擎算好的最终身价）。"
    #      "★ 2026-09-13 起本站点**只观察不落笔**：注入已搬到 0x446bf9（ce_item_score_in），这里只把引擎的最终值与"
    #      "我们的预测（10 × G_total）比一行日志（`score-out:`）—— 满额支应当完全相等、线下支应更低（那是系数）。"
    #      "handler **不写 regs->esi、不再调卡回调**（声明已在新站点收过，再收一次 = 双计）。"),
    # ⚗ 2026-10-01 回收名额：ce_item_score_below 是**纯观察者**（日志 `score-out: … delta=0` 已反复确认）
    #     ("ce_item_score_below", 0x446caa, 8, "8b45f8052c0c0000",
    #      "collect_money_item 0x446b00 的**线下分支**出口：mov eax,[ebp-8] / add eax,0xc2c（8 字节、无相对寻址）。"
    #      "一手：0x446c01 comiss + 0x446c04 jae 0x446cb4 / 0x446c0a cmp [edi+0xc74],3 + 0x446c11 je 0x446cb4 都不成立时"
    #      "（= 自机在回收线**下方**且道具非真空飞行）走这一支算分，走到 0x446cb2 `jmp 0x446cfc` **绕过** `ce_item_score`。"
    #      "★ 与 ce_item_score 一样：2026-09-13 起**纯观察者**（不写 regs->esi、不调卡回调），只打 `score-out:` 那行；"
    #      "两支互斥 ⇒ 每次收集只出**一行** `score-in:` 与**一行** `score-out:`，可逐颗对照。"),
    ("ce_item_money", 0x446d28, 6, "ff0530cd4c00",
     "collect_money_item：inc [MONEY_TOTAL]（下一条 inc [MONEY]）；沿卡链表调 on_item_money(&bonus)，两个全局一起 += bonus"),
    # ★★ 2026-09-22 **恢复**（同 `ce_item_score_in`）：这条 2026-09-16zs 的二分注释也只写「临时」，
    #    而它有**六个活的消费方**（`cards/{corruption,senro,signal,th06rank,truesilver,whirlwind}.c`
    #    都注册了 `.on_bomb_spent`）⇒ 摘着 = 这六张卡的「炸弹被消耗」回调**永远不被调用**（静默失效）。
    #    恢复前逐项过 `sitecheck.py 0x4203bc 5`：W1/W2/W3/R/W4 全绿。
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
    # ★ 2026-09-22 恢复：这条 2026-09-16zs 的二分（临时）摘除**没有留下任何理由**，而它摘掉的是
    #   **功能站点**（117 的「咆哮期间 X 不放下原版炸弹」，handler 在 `animal.c` 的 `on_bomb_gate`）
    #   ⇒ 摘着的时候那个功能是**静默失效**的（没有站点 = handler 永远不被调用，日志上一个字都没有）。
    #   恢复前逐项过过 `sitecheck.py 0x45c048 7`：W1 长度 7≥5 / W2 一条指令恰好 7 字节 /
    #   W3 窗内无分支 / R 回放安全（窗内没有 `cave_fix` 修不了的近相对分支）/ W4 与既有站点零重叠 —— 全绿。
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
    # ★★★ 2026-09-20（WORK-PACKAGES §7-B）：本条与 `ce_poc_gate_b` 的声明里带 `no_replay`
    #   —— 窗口 = **一条裸的 `ja rel32`**，是本 patch 里唯一的「cave_fix 修不了的近相对分支」窗口
    #   （字段语义与一手依据见 `_SDK_SITE_FIELDS` 顶上那段）。两条现在 park 中；一旦放回发布面，
    #   `tools/sitecheck.py` 的 W5/W6 会立刻拦住「handler 里还留着 `return BP_EXEC_ORIGINAL`」这一版
    #   —— 历史上正是那句守卫 ⇒ 池里回放那条 `ja` ⇒ 跳进池的未写区 ⇒ 写 `0x1` 的访问违例（§BE-ER）。
    ("ce_poc_gate_a", 0x445bb9, 6, "0f87a4030000",
     "ItemManager__on_tick__body 的『过线 ⇒ 道具无条件真空』分支 A：`445baa movss xmm0,[esi+0x47998]`（收点线）"
     "→ `445bb2 comiss xmm0,[esi+0x624]`（自机 y）→ **本站点 `ja 0x445f63`**（0x445f63 = `mov [item+0xc74],3` 真空态）。"
     "★ **窗口就是这条 `ja`（近相对分支）**：thcrap 的 cave_fix 只重定位窗口首条的 e8/e9 ⇒ **不能**靠回放它"
     "（回放会跳到「本格 + 0x3aa」= 池页零尾巴，就是头号崩溃，AUDIT §BE-DW）。"
     "所以 handler **恒返回 0** 并自己给落点：声明层开着（106 说「不是满 P」）⇒ 落回站点后一条 `0x445bbf`"
     "（= 那条 ja 不成立那一支）；否则 ⇒ 落到真空态入口 `0x445f63`。落空支不看 flags（一手：0x445bbf 是 "
     "`mov eax,[0x4cf2b8]` 起头、自带 cmp）⇒ 不回放不会踩标志位。"
     "⚠ 也不能改挂 `0x445f63`：那里是**四条**支路的汇聚（两条 ja + 紧随的两条 jl）⇒ 分不出「过线」。"
     "一手依据 = TH06 `ItemManager.cpp` 的道具 tick `state==1 || (128 <= currentPower && player.y < 128.0f)`"
     " ⇒ 『满 P + 在线上』才是过线收点的充要条件。"),
    ("ce_poc_gate_b", 0x446003, 6, "0f875affffff",
     "同上，**第二条** `ja 0x445f63`（向后跳；0x446003 处、6 字节）。handler 恒返回 0 + 自己给落点："
     "开着 ⇒ `0x446009`（不成立那一支），否则 ⇒ `0x445f63`。"
     "两条都要挂：只挂一条 = 另一条支路照旧把道具拉进真空态（静默半失效）。"),
    ("ce_player_die", 0x45d3a0, 6, "558bec83ec14",
     "Player__die 0x45d3a0 入口：push ebp/mov ebp,esp/sub esp,0x14（6 字节，无反/相对寻址、无窗内跳转目标）。"
     "thiscall，ecx = zPlayer；全 exe 只有 3 处 call（0x45cd27/0x45ce81/0x45d060，全在中弹判定链上）、"
     "0 处 jmp、0 处数据表引用 ⇒ 一个站点覆盖子弹/敌本体/三种激光**全部**命中路径。"
     "handler 读声明层 ce_no_hit_active()：开着就 **返回 0 并把 regs->retaddr 改成 0x45d584**"
     "（该函数唯一的 c3；此刻栈上只有调用者返回地址 ⇒ 栈干净），等于整段 die 体不执行"
     "= 一手 TH13 的「撞了但不算撞弹」（弹照样被吃掉、擦弹不受影响）。"
     "⚠ 尾部的卡虚表 +0x10 救援广播也一并被跳过 ⇒ 要靠本闸门自救的那张卡必须先撤声明再调 die。"
     "一手链见 engine/player/th18/03-player-hit-gate.md"),
    ("ce_shot_set_index", 0x45ebfb, 7, "8bbc97e0000000",
     "Player__tick_shooting_state 0x45ea00 查主炮 shooterset 前的 mov edi,[edi+edx*4+0xe0]（7 字节，无相对寻址）。"
     "edx = 本帧 fire-time set index（0..9 主炮；eax = 火力档），ebp-8 暂存同一 set index，随后 0x45ec69 会打包进 bullet+0xac。\n"
     "handler 沿卡链表问 on_shot_set_index(card, eax)：返回 >=0 时改写 edx 与 [ebp-8] 为咆哮组 base+power，放行原指令；"
     "不咆哮/无卡时返回 -1，原版主炮 0..9 一个字节都不动。"),
    # ★★ 2026-09-16（111 的魔理沙侧：**原生那 4 格子机不许出现**；`sdk.c` 顶部的长注释是唯一出处）
    ("ce_repopulate_native_option", 0x45d610, 7, "538981dc710400",
     "Player__repopulate_options_and_notify_cards 0x45d5e0 里**算完火力档之后那一条**："
     "`push ebx` + `mov [ecx+0x471dc],eax`（7 字节；该 mov 的 imm32 = 0x471dc，**不是相对寻址**）。"
     "此刻 ecx = &zPlayer.inner（thiscall this = PLAYER+0x620；一手 0x45d5fc）、"
     "eax = `CURRENT_POWER / POWER_PER_LEVEL`（一手 0x45d604–0x45d60a 的 idiv）。"
     "handler 把 eax 改成 0 后**放行原字节**：后面两段（满火力形子机 VM 的建/清、普通形 4 格的建/清）"
     "读到的档位都是 0 ⇒ 不建新子机、并走引擎自己的「清空 4 格」那条路；handler 另外自己 interrupt 掉"
     "4 格 × 2 支（+0xb0 普通形 / +0xb4 满火力形，照 0x45d823 那段同款写法）。"
     "只在 `ce_native_option_hidden(当前机体)` 为真时动手 ⇒ 其余机体 / 没装卡时逐字节不变。"),
    ("ce_native_option_muzzle", 0x45e930, 5, "558bec5153",
     "Player__shoot_one_bullet 0x45e930 的**函数入口**（5 字节 = push ebp / mov ebp,esp / push ecx / push ebx，"
     "恰好在指令边界上；无相对寻址、窗内无跳转靶。⚠ 取 6 字节会**从 `mov ebx,[ebp+8]` 中间切开**"
     "（那条是 3 字节、落在 0x45e935）⇒ cave 会回放半条指令，必须取 5 或 8）。"
     "thiscall ⇒ ecx = zPlayer*；栈（低→高）= 返回地址、packed id、timer、pos*、"
     "**option*（膛口）**（调用方压栈序一手：0x45ec5f push 膛口 / 0x45ec68 push pos / 0x45ec71 push timer / 0x45ec72 push id）。"
     "handler 读第 5 个栈参：落在**原生子机数组区间** `[PLAYER+0x670, PLAYER+0xa30)` 时**整发跳过**"
     "（★ 2026-09-17 订正：本版 thcrap 的桩**没有** `esp_diff`，改 `regs->esp` 无效；改用原生 `stack_clear_size: 0x10`"
     " ⇒ 桩走 `bp_entry0s` 变体的 `ret 0x10`，handler 只设 `retaddr = sp[1]`（真正的调用者返回地址；`sp[0]` 是 thcrap 自己的蹦床返回地址 = 站点+5）"
     " —— 一手见 engine/_shared/thcrap-breakpoint-pools.md §2；返回值调用方不检查，一手 0x45ec78；"
     "返回值调用方不检查，一手 0x45ec78）。卡申请的池 = `PLAYER+0xa30` 起（一手 0x40a7a4 `add edi,0xa30`）"
     "⇒ 区间不相交，117 / 116 / 108 的卡子机零影响。"),
    ("ce_option_shot_gate", 0x40a9c3, 6, "8b550883ec0c",
     "Player__tick_shooters_for_ability_card 0x40a9c0 的序言后 `mov edx,[ebp+8]; sub esp,0xc`（两条完整指令，"
     "6 字节，无相对寻址 / 无窗内跳转目标）。选 6 字节是因为 thcrap 的 sourcecave 下限是 5 字节：上一版只取 "
     "3 字节的 `mov edx,[ebp+8]`，thcrap 直接以 `cavesize too small` 拒绝装载，SDK 门回读失败后触发 "
     "`restore_alloc_bound` 降级路径 —— 存档/编成里已有新 id 时会踩出「卡 id 变垃圾 / HUD 槽消失」。"
     "**不能用 `0x40a9cb` 的 6 字节 `je`**：thcrap 的 cave_fix 不重定位 `0f 84 rel32`，复制进 cave 后会跳飞。"
     "handler 返回 1 = 执行完整两条原指令；返回 0 且 retaddr=0x40aa9e 才跳过本组（`mov esp,ebp` 重设栈顶）。"
     "⚠ 当前 C 侧 `CE_OPTION_SHOT_GATE_ENABLE` 默认 0（不启用），只为让门回读通过并保留实现。"),
    # ★★ 2026-09-13（原生卡冲突审计 §B-1；用户已拍「按语义叠加」）：卡链遍历**之后**重施我们的伤害倍率。
    ("ce_bullet_damage_final", 0x45e7ff, 9, "ff77088b0d5cf65100",
     "PlayerBullet__create 0x45e320 的**卡链遍历出口**：0x45e7dc mov eax,[0x4cf298]（ABILITY_MGR）→ "
     "0x45e7e4 mov esi,[eax+0x1c]（卡链头）→ 0x45e7e9 je 0x45e7ff（**没卡也跳这里**）→ "
     "0x45e7f0 mov ecx,[esi] / 0x45e7f2 push edi（bullet）/ 0x45e7f5 call [eax+0x28] / "
     "0x45e7f8 mov esi,[esi+4] / 0x45e7fb test esi,esi / 0x45e7fd jne 0x45e7f0 "
     "—— **五家（含原生 25 Saki 0x40c310 / 54 Momoyo 0x411010）全跑完**。"
     "覆盖字节 = `ff 77 08`（push [edi+8]）+ `8b 0d 5c f6 51 00`（mov ecx,[0x51f65c] = ANM_MANAGER_PTR，"
     "下一条 0x45e808 call 0x488b40 = AnmManager__get_vm_with_id），两条都无相对寻址；"
     "窗内无跳转靶（0x45e7e9 的 je 落在**站点本身** ⇒ 跳到站点 = 走断点，合法）；"
     "EDI 此刻仍是 bullet（0x45e837 才 push [edi+0x9c] 把它交给伤害源 0x45dfa0），ESI/EDX 已换用 ⇒ **只信 EDI**。\n"
     "为什么在这里：我们的倍率原来落在**同槽** `+0x28` 的桩里，而原生 25/54 是**从 shooter 重算**派 "
     "⇒ 谁在链里靠后谁说了算，我们的 ×1.25 会被**静默抹掉**（卡链顺序 = 获得/存档顺序，不可控）。"
     "搬到遍历出口 = 永远乘在**别人的最终值**上（语义 = 乘法叠加，×1.25 × 1.4 = ×1.75，不是覆盖）。\n"
     "★ **不重复施加**：本站点是**唯一**落笔点 —— `CE_CARD` 的 `+0x28` 桩不再缩放（普查：全 exe `call [reg+0x28]` 8 处，"
     "只有 0x45e7f5 在卡链上，其余 7 处是别的对象类型）⇒ 不需要「本帧是否已施加」的标记，也就没有标记失效这一档。"
     "详见 BP30-IMPLEMENTATION.md §1"),
    # ★★ 2026-09-13（原生卡冲突审计 §A-2）：死亡结果闩锁 —— 引擎判完「五家 OR」之后那一条。
    ("ce_death_result", 0x45c127, 6, "85ff8b7c2414",
     "Player__on_tick__body 0x45be90 的 case 4（决死窗口走完那一帧）里**唯一的那次判定**："
     "0x45c106 mov eax,[0x4cf298] → 0x45c10b mov esi,[eax+0x1c] → 0x45c110 je 0x45c133（没卡 ⇒ 没人取消）；"
     "0x45c112 xor edi,edi（acc=0）→ 0x45c116 push edi（当参数）→ 0x45c11c call [eax+0xc] → 0x45c121 or edi,eax → "
     "0x45c123/0x45c125 循环把**五家全跑完** → **0x45c127 test edi,edi（★ 本站点）** → "
     "0x45c129 mov edi,[esp+0x14]（还原 EDI = player）→ 0x45c12d jne 0x45c3d1（acc≠0 ⇒ 取消死亡）→ "
     "0x45c135 call 0x45d090（acc=0 ⇒ 扣命）。handler 读 **EDI = acc**：1 = 这次死亡已被取消、0 = 会扣命，"
     "写进 sdk_core 的**死亡结果闩锁**（只读 getter 供卡侧在 `+0x0c`/`+0x14`/记账回调里查）。\n"
     "窗口 = `85 ff`（test edi,edi）+ `8b 7c 24 14`（mov edi,[esp+0x14]）共 **6 字节**：两条都无相对寻址、"
     "窗内无跳转靶（0x45c110 的 je 落 0x45c133、0x45c12d 的 jne 落窗后），且**两条指令在两条臂上都要执行**"
     "（直线码、分支在窗后）⇒ **一个站点就看得到两种结果**。\n"
     "★ 为什么不是 0x45c3d1（取消臂）/ 0x45c135（扣命臂）：0x45c3d1 是 case 3 与 case 4 的**共用尾**"
     "（一手不止字节：Ghidra 反编译把这个尾部直接标成 `LAB_0045c3d1`，`case 3` 与 `case 4` 都 `goto` 它；"
     "汇编侧 0x45c0db 的 je / 0x45c0ef 的 jmp 也跳它）⇒ 会把非死亡帧记成「取消」；0x45c135 是 `call rel32`"
     "（窗内相对寻址，纪律禁止）。\n"
     "★ `[esp+0x14]` 在 cave 里读的是**同一个栈**：一手 thcrap/src/bp_entry.asm（pusha/pushf → handler → "
     "popf/popa → 回放原指令 → jmp 站点+len ⇒ cave 里 ESP = 站点处 ESP），与已实跑的 `ce_option_shot_gate`"
     "（窗内含 `sub esp,0xc`）、`ce_shop_bought`（窗内三条 push）同机制。\n"
     "本站点**只记事实、不改控制流**：返回 BP_EXEC_ORIGINAL，原两条指令原样回放，`test` 设的 ZF 正好给 0x45c12d 用。"),
    # ★★★ 2026-09-27（卡 108 第三十九批 · owner 实跑：「**妖逢魔即使在 boss 也不生成永点**」）：
    #   **把这条 2026-09-10 起 park 的站点放回发布面**。原 park 理由逐字：「临时摘掉（二分
    #   「游戏打不开」）：先确认它是不是元凶，确认后再放回」—— 那次启动崩溃后来定案在**别处**
    #   （§BE-EG/§BE-ER 的更新链节点悬垂，`ce_chain_node_guard` 才是止血点），本站在那份名单里
    #   从未被坐实；放回前逐项过 `sitecheck.py 0x42497f 5`：**W1/W2/W3/R/W4 五项全绿**
    #   （5 字节、恰好一条整指令 `a1 bc f2 4c 00` = `mov eax,ds:0x4cf2bc`、窗内无分支、
    #    handler 返 `BP_EXEC_ORIGINAL`、零重叠），`sites.py verify` 也过（expected == exe 字节）。
    # ★ 为什么 108 需要它：一手擦弹刻符撒在**被擦到的那颗弹**的位置（`th08:0x44a930` 的 pos 参数
    #   = 弹的坐标），而此前拿不到那颗弹 ⇒ 我们只能撒在**自机身上** ⇒ 距离 0 < 抓取盒 20/24
    #   ⇒ 擦弹刻符**当帧就被收掉**（state 5 上抛期间可收）⇒ 屏幕上什么都看不见 = owner 报的现象。
    #   ESI = 刚被擦到的弹（`0x42496f` 已判过 `[esi+0x20] & 4` 没擦过、`0x424984` 才置位），
    #   位置在 `esi+0x638`（`CE_BULLET_POS`）；卡用 SDK 的 `ce_graze_take(&x,&y)` 取。
    # ⚠ 同时修好 116（季节卡）：它的季节点本来就该撒在弹上，站点 park 期间那条功能**静默失效**
    #   （代码里写着「改读引擎自己的擦弹断点 `0x42497f`」，而站点不在 patch 里 ⇒ `ce_graze_take` 恒 0）。
    ("ce_graze", 0x42497f, 5, "a1bcf24c00",
     "FUN_004248a0（子弹判定）的**擦弹路径**、引擎自己算完一次擦弹之后那一条（单条 5 字节，"
     "无相对寻址）：`mov eax,ds:0x4cf2bc`。ESI = 刚被擦到的弹（`0x42496f` 判过 `[esi+0x20] & 4` "
     "没擦过、`0x424984` 才置位），位置在 `esi+0x638` ⇒ handler 只记标量并返回 BP_EXEC_ORIGINAL。"
     "消费者 = 108（擦弹刻符撒在弹的位置）与 116（季节点同）。"),
    # ★★ 2026-09-16ze（头号崩溃的止血点 + 取证；机制全文 = `sdk.c` 的 `BP_ce_chain_node_guard`，唯一出处）：
    # ⚗ 2026-09-16zn 二分：站点从 0x401352（`mov eax,[edi+8]; call eax`，**在 call 之前**）挪到
    # `0x401357`（`cmp byte [0x5217be],0`，7 字节、esp 中性、**在 call 之后**）—— 这样 handler
    # 能读到**回调的返回值**（`regs->eax`）与节点字段，用来判断「返回值选了哪一支派发」。
    # ⚠ 副作用：站点点在 call 之后 ⇒ **再也拦不住那次 call**（闸的止血能力暂时关闭，闸的代码仍在）。
    ("ce_chain_node_guard", 0x401357, 7, "803dbe17520000",
     "UpdateFuncRegistry__run_all_on_tick 0x4012e0 的**派发**上：0x40131f mov edi,[ecx]（更新链节点）→ "
     "0x401323 cmp [edi+8],0 / je（没 on_tick 就跳过）→ 0x401329 test byte [edi+4],2（暂停位）→ "
     "0x40134f mov ecx,[edi+0x24]（this）→ **本站点 0x401352 mov eax,[edi+8]** → 0x401355 call eax（下一条 0x401357）。"
     "窗口 = `8b 47 08` + `ff d0`：两条都无相对寻址、窗内无跳转靶（分派在 0x401373 之后）。\n"
     "节点 = `UpdateFunc__operator_new` 0x401530 的 0x28 字节对象（布局一手见 sdk.c）："
     "+0x00 排序键 / +0x04 flags / +0x08 **on_tick** / +0x0c 注册期回调 / +0x10 第二回调 / "
     "+0x14 链格（自指；+0x18 next、+0x1c prev，register__on_tick 0x401180 按 +0x00 升序插入）/ +0x24 **this**。"
     "注册者 = Supervisor/AbilityManager/ItemManager/EnemyManager/BulletManager/EffectManager/Player/Stage/"
     "AsciiManager/Bomb*（运行期动态）等；摘链者 = 各管理器自己的析构（0x4015a0 的调用者）。\n"
     "崩溃签名（crashdumps 5 次全同）：0xC0000005 **写 0x1** + Eax=1 + EIP 在堆（无 DEP ⇒ 堆数据当指令执行）"
     "⇒ `[edi+8]` 悬垂。触发器（用户实测）= 射击中 + 上线收点那一刻。\n"
     "handler 三件事：① 首见每个节点打一行「链地图」（node/order/flags/on_tick/this/vt + 各自模块名）；"
     "② 已知节点的 +8/+0x24 变了就打 ★ CHANGED（启动期一次分配、正常永不改变 ⇒ 直接抓到改写者）；"
     "③ on_tick 不是可执行地址（VirtualQuery 判页保护）⇒ **跳过这次 call**：eax=1、retaddr=0x401357 "
     "= 跳转表 0x4013f8[1]=0x4013b7 的纯空转路径（不碰 edi、不摘链；[0]=0x401397 会去 0x4015a0 摘链，对垃圾节点更危险）。"
     "返回 0 = 不回放 cave 里的两条原指令（栈上没有 call 的压栈 ⇒ 与「调用后」同形）。"),
    # ★★★ 2026-09-27（卡 108 · owner：「3 可以贴一手，**关键在站点写法**」）：
    #   **「一发自机弹 × 一个敌机」的命中那一刻** —— 一手 TH08 的「極人 + 每一发自机弹碰到敌机
    #   ⇒ 在**弹的位置**掉 1 颗刻符」写在 `Player::compute_damage_to_enemy` 体内
    #   （`th08:0x451860 call 0x406d10`（極人判）→ `:0x451871 mov eax,[bullet+0x480]` /
    #    `:0x451877 movsx ecx,WORD [eax+0x1e]` / `jge` 跳过 → `:0x45187f push 3` / `:0x451881 push 7` /
    #    `:0x451892 call 0x4400a0`）；TH18 **没有等价函数** —— 伤害是在**敌人侧**算的
    #   （`enm_compute_damage_sources` `0x45f0f0` 遍历 1024 槽伤害源）⇒ 取「**这个源第一次命中这个敌**」。
    #   · 窗口 `0x45f466` = `mov ecx,[ebp-0x68]` + `mov eax,[ebp-0x38]` + `test ecx,ecx`
    #     （8 字节 `8b4d988b45c885c9`；全是寄存器 / ebp 相对，**无相对寻址**）。
    #   · 它是 `0x45f458 je 0x45f466` 的**靶**（落在站点本身 = 走断点，合法 —— 那条 je 只在 `esi == 0`
    #     （这次没带敌指针）时跳 ⇒ handler 用 `regs->esi != 0` 排除）；窗内 `0x45f467..0x45f46d` 无跳转靶。
    #   · 走到这里已经过两关：`0x45f45d je 0x45f25f`（**这个源这一辈子已经打过这个敌** ⇒ 跳过本迭代）
    #     与 `0x45f437 jg 0x45f25f`（累计上限）⇒ 落在本站点 = 「这个源**第一次**命中这个敌」
    #     = **一发弹命中一次**（正是一手的颗粒度）。
    #   · 此刻寄存器：**esi = 敌指针**（本迭代 `0x45f254 mov esi,[ebx+0x20]` 刚重载、其后无人写它）、
    #     **edi = 伤害源槽 + 0x20**（循环 `lea edi,[eax+0x20594]` 起、每轮 `add edi,0x9c`）⇒ 槽 = edi − 0x20。
    #   · 落点：一手撒在**弹的位置**；本槽的 `+0x1c/+0x20` 就是弹的**当前位置**（弹的 tick
    #     `0x45efd2`–`0x45efe3` 每帧 `movq xmm0,[bullet+0x48]` → `movq [PLAYER+0x204f4+idx*0x9c],xmm0` 同步）
    #     ⇒ handler 直接读槽里那对坐标，不用另找弹。
    #   · 「这一发是不是**自机弹**」：只有弹会把伤害源下标写进 `bullet+0xb0`
    #     （`0x45e861 call 0x45dfa0` 之后 `0x45e866 mov [edi+0xb0],eax`，= `CE_PB_DAMAGE_IDX`）
    #     ⇒ handler 扫弹池（`PLAYER+0x1570`、512×0xf8、`+0x8c == 1` 活动）找 `+0xb0 == 本槽下标`。
    #     ⚠ 不用槽自己的 `+0x98`（特殊 handler 索引）判：弹把它写成 1（`0x45e890`），但两个 create
    #       （`0x45de40` / `0x45dfa0`）**都不清它** ⇒ 槽回收后可能残留 ⇒ 不可作判据。
    #   · handler = `sdk.c` 的 `BP_ce_bullet_hit`（机制全文的唯一出处）。
    ("ce_bullet_hit", 0x45f466, 8, "8b4d988b45c885c9",
     "enm_compute_damage_sources 0x45f0f0 的**新命中**点（一源一敌一次；跳过条件见上面注释）："
     "esi = 敌指针、edi = 伤害源槽 + 0x20、ebx = 帧基。handler 反推槽下标"
     "（`((edi-0x20) - (PLAYER+0x204d8)) / 0x9c`）、扫弹池（`PLAYER+0x1570`、512×0xf8、"
     "`+0x8c == 1`、`+0xb0 == 下标`）判定「这一发是**自机弹**」，再沿卡链表调 on_bullet_hit(&ev)"
     "（先扫一遍卡表看有没有声明，没有就整段早退 ⇒ 无卡时零开销）。窗口 = mov/mov/test 三条原样回放"
     "（返回 BP_EXEC_ORIGINAL）、无相对寻址。一手：極人 + 每发自机弹命中敌机 ⇒ 在弹位置掉 1 颗刻符"
     "（`th08:0x451860`–`:0x451892`）。"),
    # ★★★ 2026-09-13（owner 口径：「**没有进 replay**」→「要它进录像」）：调试件
    #   「逐帧合成方向输入」（`cards/input_synth.{h,c}`，用法见 `DEV-INPUT-SYNTH.md`）的落点。
    #   窗口 = `ReplayManager__on_tick__record_replay`（`0x462940`）里的
    #   **`0x462966 call Input__compute_edges`**（5 字节 `e8 55 82 fc ff`）。
    #   一手帧内顺序（逐条反汇编，见 `engine/input/th18/01-key-mapping.md` §4.1）：
    #     `0x462950` 存 `INPUT_HELD_PREV` → `0x46295a` 读 raw 键盘 → **`0x462961` 写 `INPUT_HELD`**
    #     → **本站点 `0x462966`** → `0x46296b` 长按射击自动低速 → `0x4629f7`/`0x462a0d` **录制读点**
    #     → `0x463060` 落盘（6 字节/帧）。
    #   ★ 本仓站点机制：**handler 在原件之前跑**（`thcrap_bp.h`：返回 `BP_EXEC_ORIGINAL` 才回放窗口
    #   那几字节）。所以站点取 `0x462966` 时，进入 handler 时 `0x462961` 那条 `mov` **已经执行完**
    #   ⇒ `INPUT_HELD` 里已经是本帧 raw（**拷贝已完成**），我们在这之后把四个方向位**先抹后 OR** 写进去
    #   ⇒ 随后的 `0x42abc0` 会为我们的位算 `INPUT_PRESSED`/`INPUT_RELEASED`，`0x4629f7` 又把
    #   `INPUT_HELD` 录进 replay，同帧 Player（节点 `0x17`）读到 ⇒ **玩法与录像同时正确**。
    #   ⚠ 窗内唯一一条 `e8 rel32` **正是窗口首条** ⇒ thcrap 的 `cave_fix` 会重定位它
    #   （`sitecheck.py 0x462966 5`：W1/W2/W4 ✅、W3 判 "fixable"，**不需要 `no_replay`**）；
    #   同地址先例 = `mods/th18.v1.00a/mouse-control` 的 hook 2（同一个 exe 上已实跑，其 AUDIT §B
    #   记「录 replay 的 0x463060 在注入点之后 ⇒ 输入会一致地录进录像」）。
    #   为什么不取 `0x462961`：handler 在**那条 `mov` 之前**跑 ⇒ 无法「拷贝之后写」；
    #   备选路（改 `regs->eax`、让原件自己把我们的位搬进 `INPUT_HELD`）效果等价但不符合 owner 的落点口径。
    #   · 回放门：站点其实**只在录制 tick 上命中**（回放走 `0x462a50`，见 §4.2），
    #     但 handler 仍与写入/绘制/计数共用 `input_synth_blocked()`（`CE_GT_REPLAY_PLAYING` / 无自机）⇒ 双保险。
    # ★★★ 2026-09-13 **已拆线**（owner 口径：「那个无敌脚本空格的都拆掉，代码保留，注释一下这是什么」）
    #   ⇒ 下面这条站点**不再安装**（连同 `sdk.c` 的 handler、`sdk.h` 的 on_draw 调用一起撤）；
    #     整件代码保留在 `cards/input_synth.{h,c}` + 单测，**怎么重挂三步**见 `DEV-INPUT-SYNTH.md` §0。
    #   ⇒ 记账随之回落：声明 34 → **33**、进程级 54 → **53**（本注释以下的原文是拆线前的记录）。
    #     ("ce_input_synth", 0x462966, 5, "e85582fcff",
    #      "调试件「逐帧合成方向输入」→ 写 `INPUT_HELD`（四个方向位，先抹后 OR；Z/X/Shift/C/D 一个都不碰）。"
    #      "站点 = 录制 tick `0x462940` 里的 `call Input__compute_edges`（`0x462966`，5 字节；"
    #      "**在 `0x462961` 的 raw→HELD 拷贝之后**、边沿与录制读点之前）：handler 在原件之前跑 ⇒ "
    #      "我们写下的位会被 `0x42abc0` 算成边沿、被 `0x4629f7` 录进 replay、同帧 Player（`0x17`）读到。"
    #      "默认（不按调试键 / 回放中 / 无自机）**一个字节都不写**。"
    #      "窗内唯一 rel32 = 窗口首条 `e8` ⇒ `cave_fix` 重定位（sitecheck W3 fixable），不用 `no_replay`。"
    #      "一手与落点论证见 `DEV-INPUT-SYNTH.md` §4 / `engine/input/th18/01-key-mapping.md` §3.3–§4。"),
]

# ★★★ 2026-09-18（用户实跑报「魔理沙原子机还有小激光 / shift 还会发导弹」时挖出来的）：
#   **这里原来是「按【下标】截断」的那一行，是一颗定时炸弹** ——
#   当初写它时列表只有 12 条（截掉最后两条**观察者**站点给新卡腾名额，注释也是这么写的）；
#   后来往列表里**插 / 加了新站点** ⇒ 被截掉的从「2 条观察者」变成 **16 条**，其中一半是**功能**站点
#   （111 的原生子机两道门 / 106 的 POC 闸 / 113 的 option 闸与 `ce_player_die` /
#     `ce_shot_set_index` 主炮组覆盖 / `ce_bullet_damage_final` = **别的批次的伤害倍率** …）
#   ⇒ 它们**静默失效**，而 sites.py 的「已声明」表里仍然写着它们（对账一直是绿的 —— 绿得毫无意义）。
#   ⇒ 现在改成**按名字**保留：只停用真正冗余的观察者，其余一律进 patch。
#   ⚠ 预算：`SITE_BUDGET`（我们自己的线）与进程级 `SITE_TOTAL_BUDGET` 见本文件末尾的 check；
#     池子容量是**运行期实测**的（`sdk.c` 的 `ce_pool_count()` 每局开机打一行 `pool: 可执行页/跳板格`），
#     条数本身不是判据（`GATES.md` §8.1 的实测结论），所以这里按「功能优先」取舍。
#   本轮先恢复**用户当场报的那两条**（111 的原生子机门：少一条 = 静默半失效，见 sdk.c 的长注）。
_SDK_KEEP_NAMED = {
    # ★ 2026-09-22：原先靠「下标 < 10」兜底的那 10 条**逐条点名**（下标兜底已删，见下面的过滤行）。
    #   一条都不能漏：漏了 = 该站点静默掉出 patch（`sites_gen.h` 宏变坏值 `0x-400000`）。
    "ce_enemy_damage",                # 0x42f2cb：Boss 随血量掉灵（113 通道 A 的事件源）
    "ce_card_bind",
    "ce_item_money",
    "ce_enemy_drop",
    "ce_enemy_shot_slot",
    "ce_item_frame",
    "ce_item_collect",
    "ce_item_commit",
    "ce_shot_damage",
    "ce_bomb_gate",                   # 0x45c048：117「咆哮期间 X 不放下原版炸弹」
    # ★ 2026-09-22 恢复：这条被 2026-09-18 的「名额回收」摘掉（为了腾站点名额），**但它有活的消费方**：
    #   `cards/trance.c` 的 `on_option_shot_gate`（灵界期间停掉装备卡子机弹）与 `cards/animal.c` 的同名回调
    #   （117 咆哮期间），而 `sdk.c` 的 `CE_OPTION_SHOT_GATE_ENABLE` 早就是 **1**（2026-09-12 十四修）
    #   ⇒ 站点不在 patch 里 = 那两个回调**永远不会被调用**（静默失效，日志一个字都没有）。
    #   恢复前逐项过 `sitecheck.py 0x40a9c3 6`：W1/W2/W3/R/W4 全绿。
    "ce_option_shot_gate",            # 0x40a9c3：装备卡子机开火闸（113 灵界 / 117 咆哮）
    "ce_item_score_in",               # 0x446bf9：**计分层唯一的落笔点**（107/110/111/113 的 ce_score_declare）
    "ce_bomb_spent",                  # 0x4203bc：六张卡的 .on_bomb_spent（corruption/senro/signal/th06rank/truesilver/whirlwind）
    "ce_repopulate_native_option",   # 111：不建原生 4 格子机（并走引擎自己的清空）
    "ce_native_option_muzzle",       # 111：原生子机膛口发出的弹**整发跳过**（激光与导弹都在这条上）
    # ★★ 2026-09-19（owner 口径「对于明确是bug的部分，先解决吧」）：把**功能站点**从 `i < 10` 的
    #   下标截断里放出来。下面五条每一条都对应一个**已报的 bug**（或一条静默失效的功能），
    #   窗口四项检查（W1–W4）逐条过过（`sitecheck.py <addr> <len>`）：
    "ce_shot_set_index",             # 0x45ebfb：主炮组号覆盖 ⇒ 修 th13 #5「灵界发弹更改消失」+ th17 #2「咆哮发弹更改消失」
    "ce_enemy_death",                # 0x430240：共享「敌机死亡」事件（一敌一生一次）—— 2026-09-22 补点名（原靠下标兜底）
    "ce_player_die",                 # 0x45d3a0：中弹无伤闸门 ⇒ 修 th13 #6「主动灵界中弹⇒结束后死亡」+ #3「中弹音效未屏蔽」
    # ⛔ 2026-09-19 二次二分：**接管这条 `ja` 本身有害**（106 在场时道具只减速、不变橙不抖）⇒ 再 park。
    #    106 的「不满 P 过线不吸」改走**非接管**的路（POC 线高度：不满 P 时声明成永远不满足 `line > py` 的值）。
    # ⛔ 同上：B 成对 park（只留一条 = 另一条支路照旧真空，更不能留）。
    # ⚠ 历史（留着）：2026-09-19 晚这两条曾被**撤回**一次（owner 实跑：「同装 th14 和 15 的时候，一直在全屏吸收」）——
    #   `ce_poc_gate_a` / `ce_poc_gate_b` 两条**不能就这样放出来** —— 它们的 handler
    #   （`sdk.c` 的 `ce_poc_gate_common`）**替换掉的是一条条件分支**（窗口 = 那条 `ja`），
    #   但它只按 `ce_full_p_gate_active()` 选落点、**从不复现那条比较的语义**（`comiss` 的结果还活在
    #   `regs->eflags` 里，handler 没读）⇒ 没人声明闸门时它**恒落真空入口 `0x445f63`** ⇒
    #   道具**每帧无条件真空** = 实跑看到的"一直在全屏吸收"。见 `AUDIT.md` §BE-EN。
    #   ⇒ 想装它，先把 handler 改成「按 `regs->eflags` 复现 `ja`（CF==0 && ZF==0）**再**与满 P 闸与一下」，
    #     并且实跑验收之后再放。th06 #1 因此**暂时仍挂**。
    "ce_bullet_damage_final",         # 0x45e7ff：**伤害层唯一起作用点**（`bullet_damage_permille`）——
                                      #   它被截掉 = 所有走伤害层的卡**静默失效**（古锭刀/衔尾蛇/季节/测试卡 200…）
    # ★ 2026-09-27（108）：一发自机弹 × 一个敌机的命中（一手：極人 + 每发命中 ⇒ 掉 1 颗刻符）
    "ce_bullet_hit",                  # 0x45f466：`enm_compute_damage_sources` 的「新命中」点
    # ★ 2026-09-27（108/116）：引擎自己的子弹擦弹事件（坐标 = 被擦到的那颗弹）
    "ce_graze",                      # 0x42497f：放回发布面（见上面那一段的放回论证）
    # ★★★ 2026-09-13（owner 拍板：「没有进 replay」→「要它进录像」）：调试件「逐帧合成方向输入」的落点。
    #   记账在文件头「站点记账」那一段（27→28 的旧口径 vs 现测 33→34）；判据 = sitecheck 五项 + 实跑。
    # "ce_input_synth",              # ★ 2026-09-13 已拆线（见上）；重挂时取消注释
}
# 过滤**之前**的全表（含 park 的）—— `tools/sitecheck.py` 的体检要连 park 的一起查：
# park = 没进 patch = 现在没装，所以本身没风险；但**一旦放回发布面**，用的就是这一份声明。
SDK_SITES_ALL = list(SDK_SITES)
# ★★ 2026-09-22：**去掉「下标 < 10」那条兜底**（它就是 `AUDIT.md` §BE-EG 那颗定时炸弹的同款）——
#   今天加回 `ce_bomb_gate` 时它当场咬了人：新条目插在中间 ⇒ 后面整体右移一位 ⇒
#   `ce_enemy_death`（在第 10 位、又没写进 `_SDK_KEEP_NAMED`）**静默掉出 patch**
#   （`sites_gen.h` 里它的宏变成坏值 `0x-400000`，只有 `gen_drift`/`sdk.c` 那个核对表会侧面暴露）。
#   ⇒ 现在**一律按名字**：在 `_SDK_KEEP_NAMED` 里 = 进 patch；不在 = park（`SDK_SITES_ALL` 仍留全表，
#     给 `sitecheck.py` 体检用）。加站点的人只要往 keep 表里补一行，不会再被下标暗算。
SDK_SITES = [s2 for s2 in SDK_SITES if s2[0] in _SDK_KEEP_NAMED]

# 站点级的**原生声明字段**（thcrap 的 `breakpoint_from_json` 认识的那些）。
# `stack_clear_size = N` ⇒ 该站点改用 `bp_entry0s` 变体（桩末尾是 `ret imm16`），
# handler 返回 0 时桩执行 `ret N`：**回到调用者 + 弹掉 N 字节栈参**。
# 这是 thcrap 唯一的「弹栈参」机制 —— 本版桩没有 `esp_diff`，handler 改 `regs->esp` 是**无效**的
# （一手：engine/_shared/thcrap-breakpoint-pools.md §2）。
# `ce_native_option_muzzle`（0x45e930）是 stdcall 形态（调用点 0x45ec78 之后没有 `add esp,0x10`），
# 4 个栈参 = 0x10 ⇒ 必须走这条机制，handler 才有办法「整发跳过」而不破坏调用方栈。
# ⚠ 数值口径（2026-09-18 实跑订正）：`ret imm16` 弹的那个槽是**桩自己那条 `call <桩>` 压的返回地址**
#   （比调用者压的返回地址**低一格**）⇒ 要等价「被调函数自己 `ret N`」，必须写 `imm = 4 + N`。
#   本站点函数 = `ret 0x10`（一手：`th18:0x45e9cd` / `th18:0x45e9fd` 两条都是 `c2 10 00`）⇒ **写 0x14**。
#   第一版误写 0x10 ⇒ 实跑「一按 Z 就崩」：调用链栈整体偏低 4 ⇒ 上层 `pop ebx` 弹出 3 ⇒ 派发器
#   `th18:0x4013ba` 的 `mov ecx,[ebx+0x50]` 读 `0x53` 违例（dump 一手）。
#   同源旁证：本局桩页里 thcrap 自己的站点带 `c2 08 00`（= 4 + 一个栈参 4）⇒ 规律一致。
_SDK_SITE_FIELDS = {
    "ce_native_option_muzzle": {"stack_clear_size": 0x14},

    # ══ ★★★ 2026-09-20（WORK-PACKAGES §7-B）：`no_replay` —— **回放禁令** ══
    # 语义（一句话）：**本站点的窗口里含一条 `cave_fix` 修不了的近相对分支 ⇒ handler 禁止返回
    # `BP_EXEC_ORIGINAL`（=1，从跳板池里回放那几字节）；这类站点同时也不宜接管**（§BE-EQ 实测）。
    #
    # 为什么（一手机制，`AUDIT.md` §BE-ER / §BE-DW）：thcrap 给每条声明在跳板池里留一格放**站点原始字节**；
    # handler 返回 1 ⇒ 桩从池里回放那几字节。而 `cave_fix` **只重定位「窗口第一条就是 `e8`/`e9`」**那一种
    # ⇒ 窗口里其余 rel32 分支（`0f 8x` 的 jcc / `e9` / 非首条的 `e8`）**按原相对位移**从**池的地址**跳
    # ⇒ 落到池的未写区 ⇒ 执行 `00 00`（`add byte ptr [eax],al`），而那一刻 `Eax = 1` ⇒ **写 `0x1` 的访问违例**
    # ⇒ 崩点 `Eip` 落在池里（`0x03c2xxxx`）。逐字节复算：`ce_poc_gate_a` 的池副本 `0x03c20250`，
    # `+6+0x3a4 = 0x03c205fa` **正是 dump 的 `Eip`**（§BE-ER 的表）。
    #
    # 字段形态：`"no_replay": "<前提说明>"`（**非空字符串** = 已声明；值就是打印出来的那句前提）。
    # 唯一合法的「窗口含近相对分支」的站点在这里登记 —— 判据由**反汇编**产生
    # （`tools/sitecheck.py` 逐指令看窗口），不是人肉点名：**没登记又含分支 ⇒ `make check` 判红（W5）**。
    # 登记了以后还有第二道（W6）：**该站点的 handler（含它调用的函数）里出现 `BP_EXEC_ORIGINAL` ⇒ 判红**
    # —— 正是 2026-09-19 那次事故的形态（有人加了一句 `return BP_EXEC_ORIGINAL` 守卫就崩了）。
    #
    # ⚠ 本字段是**纯元数据**（`_SDK_SITE_META_KEYS`）：它**不进 patch**，加/改它不应改变生成物
    #   （`emit_sdk_breakpoints` 里过滤 + 断言；否则补一句注释就会换掉发出去的 patch 的 md5）。
    "ce_poc_gate_a": {"no_replay":
        "窗口 = 唯一一条指令 `0f 87 a4 03 00 00`（`ja th18:0x445f63`，rel=+0x3a4，§BE-ER 逐字节核过）"
        "⇒ 回放会从池的地址跳到「本格 + 0x3a4」= dump 里的 `Eip` `0x03c205fa`（池未写区）"
        "⇒ `00 00`/写 `0x1` 的访问违例。**唯一允许的形态 = handler 恒返回 0 + 自己给落点**"
        "（A `th18:0x445bbf` / 真空入口 `th18:0x445f63`）；接管本身也已实测有害（§BE-EQ：道具只减速不变橙不抖）。"
        "现 park 中（`_SDK_KEEP_NAMED` 外）；放回发布面前先过 sitecheck W5/W6（§BE-ER / §BE-EQ / WORK-PACKAGES §7-B）"},
    "ce_poc_gate_b": {"no_replay":
        "同 `ce_poc_gate_a`（窗口 = `0f 87 5a ff ff ff` = `ja th18:0x445f63`，rel=−0xa6）"
        "⇒ 回放跳进池的未写区。**唯一允许的形态 = 恒返回 0 + 自己给落点**"
        "（B `th18:0x446009` / 真空入口 `th18:0x445f63`）；两条必须成对处理（只挂一条 = 另一条支路照旧真空）。"
        "现 park 中；放回发布面前先过 sitecheck W5/W6（§BE-ER / §BE-EQ / WORK-PACKAGES §7-B）"},
}

# `_SDK_SITE_FIELDS` 里属于**体检/记账**（不进 patch）的键。thcrap 的 `breakpoint_from_json` 不认识
# `no_replay` —— 写进 patch 只会变成一条没人读的杂字段，且会让「纯文档性改动」改变 patch 的 md5。
_SDK_SITE_META_KEYS = ("no_replay",)


def sdk_site_meta(name):
    """某站点声明的**元数据字段** `{key: value}`（去掉 `_SDK_SITE_META_KEYS` 之外的）。
    给 `tools/sitecheck.py` 当**权威声明源**用 —— 别在工具里正则扫本文件。"""
    return {k: v for k, v in _SDK_SITE_FIELDS.get(name, {}).items() if k in _SDK_SITE_META_KEYS}


def sdk_no_replay():
    """→ `{站点名: 前提说明}`：**已声明 `no_replay`** 的站点（含 park 的；判据源 = `_SDK_SITE_FIELDS`）。"""
    return {n: f["no_replay"] for n, f in _SDK_SITE_FIELDS.items() if f.get("no_replay")}


def _sdk_rva(name):
    """按**名字**取 SDK 断点地址 —— 别再用 `SDK_SITES[i]` 下标：插一条就全体错位
    （2026-09-12 踩过：插入 `ce_enemy_death` 后 `CE_BP_SHOT_SET_INDEX_RVA` 变成了 0x030240）。"""
    for site in SDK_SITES:
        if site[0] == name:
            return site[1]
    return 0   # 容忍缺失（配合下面的切片过滤）
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
        # 只有**原生字段**（`stack_clear_size` 这类 thcrap 认识的）进 patch；
        # `no_replay` 之类的元数据留在 sites.py 里做体检（见 `_SDK_SITE_META_KEYS` 那段）。
        for k, v in _SDK_SITE_FIELDS.get(name, {}).items():
            if k in _SDK_SITE_META_KEYS:
                continue
            out[name][k] = v
        assert not (set(out[name]) & set(_SDK_SITE_META_KEYS)), \
            "%s：元数据字段泄进了 patch（它不该改变生成物）" % name
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
    out = {
        "breakpoints": {"ce_test_deck": {
            "addr": "0x%06x" % TEST_MOVZX, "cavesize": 8,
            "expected": "0fb6843008f60500",
            "title": "测试：reset_cards 读初始卡组，空槽 → cards_dev.js start_deck 里的下一个 id"},
        "ce_trace_alloc": {
            "addr": "0x%06x" % TEST_TRACE, "cavesize": 7,
            "expected": "817f2800010000",
            "title": "测试：记录每次 allocate_new_card(id, mode)"}},
    }
    # ★★★ 一次性复现闸（`CE_CANARY_POC_GATE=1`，见 AUDIT.md §BE-DW 的验证配方）：
    #   把「窗口 = 一条 `ja rel32`」的两条站点按**原样**声明进**测试归档**，配 `BP_ce_canary_a/b`
    #   （恒回放 ⇒ thcrap 让 cave 原样回放那条 ja ⇒ 跳到「本格 + 0x3aa」= 池页零尾巴）。
    #   目的：验证定案的算术（预期崩点 = 首个全零格 − 0x10 + 0x3aa）。**默认关**；跑完必须复原。
    if os.environ.get("CE_CANARY_POC_GATE"):
        out["breakpoints"]["ce_canary_a"] = {
            "addr": "0x445bb9", "cavesize": 6, "expected": "0f87a4030000",
            "title": "★★ 复现实验：窗口=一条 ja（0f 8x rel32），handler 恒回放 ⇒ cave 里跳飞（§BE-DW）"}
        out["breakpoints"]["ce_canary_b"] = {
            "addr": "0x446003", "cavesize": 6, "expected": "0f875affffff",
            "title": "★★ 复现实验：同上（第二条 ja）"}
    return out


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
        if name.startswith("sht_"):
            # pl0X.sht 扩容：纯 hex code（无 `<…>` 表达式），通用分支会抛 ValueError
            bad += verify_sht_binhack(name, bh, text, text_va)
            continue
        if name.startswith("count_"):
            # 图鉴条目数上界的编码改写（cmp r,imm8 → cmp r8,imm8 + jb）：故意换 opcode，
            # 通用分支的「只许换常量」对不上，单独核。
            bad += verify_count_binhack(name, bh, text, text_va)
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
              "#define CE_BP_ITEM_SCORE_IN_RVA 0x%06x" % (_sdk_rva("ce_item_score_in") - 0x400000),
              # ⚗ 旧行（留着对照）：
              # "#define CE_BP_ITEM_SCORE_IN_RVA ...
              "#define CE_BP_ITEM_MONEY_RVA 0x%06x" % (_sdk_rva("ce_item_money") - 0x400000),
              "#define CE_BP_BOMB_SPENT_RVA 0x%06x" % (_sdk_rva("ce_bomb_spent") - 0x400000),
              "#define CE_BP_ENEMY_DROP_RVA 0x%06x" % (_sdk_rva("ce_enemy_drop") - 0x400000),
              "#define CE_BP_ITEM_FRAME_RVA 0x%06x" % (_sdk_rva("ce_item_frame") - 0x400000),
              "#define CE_BP_ITEM_COLLECT_RVA 0x%06x" % (_sdk_rva("ce_item_collect") - 0x400000),
              "#define CE_BP_ITEM_COMMIT_RVA 0x%06x" % (_sdk_rva("ce_item_commit") - 0x400000),
              "#define CE_BP_SHOT_DAMAGE_RVA 0x%06x" % (_sdk_rva("ce_shot_damage") - 0x400000),
              "#define CE_BP_BULLET_HIT_RVA 0x%06x" % (_sdk_rva("ce_bullet_hit") - 0x400000),
              "#define CE_BP_GRAZE_RVA 0x%06x" % (_sdk_rva("ce_graze") - 0x400000),
              # ★ 2026-09-13 **已拆线**：`CE_BP_INPUT_SYNTH_RVA` 不再生成（站点与 handler 一起撤）。
              #   重挂时把下面这行取消注释即可（窗口/时序见本文件 SDK_SITES 里那条注释）。
              # "#define CE_BP_INPUT_SYNTH_RVA 0x%06x" % (_sdk_rva("ce_input_synth") - 0x400000),
              "#define CE_BP_BOMB_GATE_RVA 0x%06x" % (_sdk_rva("ce_bomb_gate") - 0x400000),
              "#define CE_BP_PLAYER_DIE_RVA 0x%06x" % (_sdk_rva("ce_player_die") - 0x400000),
              "#define CE_BP_SHOT_SET_INDEX_RVA 0x%06x" % (_sdk_rva("ce_shot_set_index") - 0x400000),
              "#define CE_BP_OPTION_SHOT_GATE_RVA 0x%06x" % (_sdk_rva("ce_option_shot_gate") - 0x400000),
              "#define CE_BP_ENEMY_DAMAGE_RVA 0x%06x" % (_sdk_rva("ce_enemy_damage") - 0x400000),
              "#define CE_BP_ENEMY_DEATH_RVA 0x%06x" % (_sdk_rva("ce_enemy_death") - 0x400000),
              "#define CE_BP_ENEMY_SHOT_SLOT_RVA 0x%06x" % (_sdk_rva("ce_enemy_shot_slot") - 0x400000),
              "#define CE_BP_BULLET_DAMAGE_FINAL_RVA 0x%06x" % (_sdk_rva("ce_bullet_damage_final") - 0x400000),
              "#define CE_BP_DEATH_RESULT_RVA 0x%06x" % (_sdk_rva("ce_death_result") - 0x400000),
              # ★ 2026-09-16：111 魔理沙侧「原生子机不许出现」的两条（sdk.c 的长注释 = 唯一出处）
              "#define CE_BP_REPOP_NATIVE_OPT_RVA 0x%06x" % (_sdk_rva("ce_repopulate_native_option") - 0x400000),
              "#define CE_BP_NATIVE_OPT_MUZZLE_RVA 0x%06x" % (_sdk_rva("ce_native_option_muzzle") - 0x400000),
              # ★ 2026-09-16za：106 的「上线收点当且仅当满 P」两条（sdk.c 的 BP_ce_poc_gate_a/b = 唯一出处）
              "#define CE_BP_POC_GATE_A_RVA 0x%06x" % (_sdk_rva("ce_poc_gate_a") - 0x400000),
              "#define CE_BP_POC_GATE_B_RVA 0x%06x" % (_sdk_rva("ce_poc_gate_b") - 0x400000),
              # ★ 2026-09-16ze：更新链节点闸（头号崩溃的止血点；sdk.c 的 BP_ce_chain_node_guard = 唯一出处）
              "#define CE_BP_CHAIN_NODE_GUARD_RVA 0x%06x" % (_sdk_rva("ce_chain_node_guard") - 0x400000),
              # 战线 F：pl0X.sht 组数扩容那处 binhack 的落点。DLL 在门里回读它
              # （binhack 的 expected 不匹配时 thcrap **只打一行日志就跳过** ⇒ 没有回读就等于没护栏）。
              "#define CE_BH_SHT_DBASE_RVA 0x%06x   /* lea eax,[edx+0x%x→0x%x]，AUDIT §CQ */"
              % (SHT_DBASE_VA - 0x400000, SHT_DBASE_OLD, SHT_DBASE_NEW),
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
              "#define CE_MENU_COUNT_MAX 255   /* 两处循环上界已改成无符号字节比较（见 CE_MENU_COUNT_OP）*/",
              "typedef struct { uint32_t rva; uint8_t imm_off, width; } ce_count_site_t;",
              "static const ce_count_site_t CE_MENU_COUNT[] = {",
              ] + ["    { 0x%06x, %d, %d }," % (a - 0x400000, o, w) for a, _, o, w in MENU_COUNT_SITES] + [
              "};", "#define CE_NMENU_COUNT (sizeof(CE_MENU_COUNT)/sizeof(CE_MENU_COUNT[0]))",
              "/* 两处循环上界的编码改写（`MENU_COUNT_BCMP`）：每条恰好改 2 字节 ——",
              "   off/want[0] = 比较指令首字节（83→80），off/want[1] = jcc 第二字节（8c→82 / 7c→72）。",
              "   门里核对，打不上就 FAIL —— 少这两条时条目数 > 127 会**静默**按有符号比（imm8 ≥ 0x80",
              "   变负）⇒ 循环零圈、图鉴空掉（AUDIT §AG 的症状链）。立即数字节不受影响（偏移 2）。*/",
              "typedef struct { uint32_t rva; uint8_t off[2], want[2]; } ce_count_op_t;",
              "static const ce_count_op_t CE_MENU_COUNT_OP[] = {",
              ] + ["    { 0x%06x, { %d, %d }, { 0x%02x, 0x%02x } },   /* %s */"
                   % (va - 0x400000, count_op_changes(exp, new)[0][0], count_op_changes(exp, new)[1][0],
                      count_op_changes(exp, new)[0][1], count_op_changes(exp, new)[1][1], why.split("：")[0])
                   for va, exp, new, why in MENU_COUNT_BCMP] + [
              "};", "#define CE_NMENU_COUNT_OP (sizeof(CE_MENU_COUNT_OP)/sizeof(CE_MENU_COUNT_OP[0]))",
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
    ap.add_argument("cmd", choices=["list", "check", "gen", "verify", "conflicts", "explain"])
    ap.add_argument("others", nargs="*", help="conflicts：其它 patch 的 th18.v1.00a.js")
    ap.add_argument("--rows", type=int, default=ROW_COUNT,
                    help="新表行数（第 1 步用 58 = 行为零变化）")
    ap.add_argument("-o", "--out")
    ap.add_argument("--alloc", action="store_true",
                    help="战线 B（跳转表搬迁）。rows > 58 时自动开")
    a = ap.parse_args()

    if a.cmd == "explain":
        # ★ 2026-09-17（用户口径：把站点预算闸定下来）：把"闸为什么这么定"写进工具本身 ——
        # 同僚不必翻文档就能看懂依据、以及怎么把真实上限实测钉死。
        print("站点预算闸 —— 依据与用法（不必读文档）\n")
        print("【它是什么】thcrap 给每条声明的断点在内存里发一格跳板（`0x10` 字节/格）。")
        print("            这些格是**全进程共用**的（分片实验已证：按 patch 分开也照样崩）⇒")
        print("            凡是在同一进程里声明断点的 mod 都在抢同一排格子。")
        print("\n【为什么按进程算】本机实测（2026-09-18）：")
        for nm, n, src in external_sites():
            print("    %-26s %3d 条  %s" % (nm, n, src))
        print("    我们正式 patch              20 条  patch/th18.v1.00a.js")
        print("    我们测试归档（可关，免费腾 2）  2 条  patch-test/th18.v1.00a.js")
        print("    合计（进程级）              %d 条  ← 闸的真正口径（池内实测「桩数」应与它相等）"
              % (20 + 2 + sum(n for _, n, _ in external_sites())))
        print("\n【安全线】我们自己的硬线 SITE_BUDGET=%d；进程级 SITE_TOTAL_BUDGET=%d（**只作告警**）"
              % (SITE_BUDGET, SITE_TOTAL_BUDGET))
        print("    ⚠ 2026-09-17 14:33 实测修订：**同一个声明总数（40）既崩过也跑通过**（那次实测 41 格、12 张卡、无 dump）")
        print("      ⇒ **条数不是决定因素**（池页 256 格/页，41 格远没满）⇒ 总数只在构建期做可见性与告警，真正的信号是：")
        print("      运行期 `pool:` 实测格数 + **池崩签名**（`Eip` 不在任何模块 / 落 `RESERVE` 段、`eax=1` 写 `0x1`）。")
        print("      （38 这个数现在只是「拍脑袋的保守值」，等下面第 2 步的对照跑完就把它换掉。）")
        print("      依据仍在：已知 total=40 **崩过两次**（池崩：跳进未提交的池区，`Eip` 落 `RESERVE` 段）。")
        print("\n【怎么把池崩的触发条件找出来（对照实验）】")
        print("    1) 跑一局（进游戏即可，不必玩），看日志里这一行：")
        print("         pool: 可执行页 N、跳板格 M、首个全零格 @…   ← DLL 开机实测（sdk.c 的 ce_pool_count）")
        print("       M = 本配置**实际装了多少格**；与 `tooling/poolmap.py <日志>` 的离线解析互校。")
        print("    2) 用两档对照**改变池子的布局/占用**（不只是条数）：关掉测试归档（−2）、与 `base_tsa` 对照（−18）、")
        print("       或换一次启动顺序；哪一档开始崩（看 crashdumps + 池崩签名）就说明**触发条件在那里**。")
        print("    3) 把结论写回 GATES.md §8（实测记录表）与 `SITE_TOTAL_BUDGET`，并在 COMPAT-MATRIX §1 记账。")
        print("\n【配额（GATES.md §6）】还没真做的卡默认 **0 条新站点**（能调用/数据层/并进既有站点就不加）；")
        print("    确需则 ≤1 条，并写明：为何不能并进既有站点 / 一手依据 / 与哪张卡共享。")
        print("\n【提交前自查】`python3 sites.py check` ⇒ 看「合计」有没有涨过安全线。")
        return 0

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
        n_alloc = sum(1 for k in json.load(open(path, encoding="utf-8"))["binhacks"] if k.startswith(("alloc_", "grow_", "unlock_", "order_", "menu_", "pool_", "snd_", "sht_", "count_")))
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
        n_sites = len(bp_names)          # ★ 用 patch 实际声明条数（别用列表长度：会漏掉音效/文案/unlock 那批）
        # ★ 2026-09-17：闸改成**进程级记账**（用户口径：gate = 站点预算闸；细节由我定，以实测为准）。
        # 原来只数我们自己的 patch ⇒ 数字永远是假的（我们 22 / 真实 40，而 40 崩过）。
        _rows, _ours, _test, _ext, _total, _warn = build_site_accounting(text, text_va)
        print("\n站点预算核对（**进程级**：谁都在吃同一个跳板池）：")
        for _c, _n, _src in _rows:
            print("   %-22s %3d 条   %s" % (_c, _n, _src))
        print("   安全线：我们自己的 SITE_BUDGET=%d；进程级 SITE_TOTAL_BUDGET=%d"
              "（已知 total=%d 崩过）" % (SITE_BUDGET, SITE_TOTAL_BUDGET, SITE_TOTAL_KNOWN_BAD))
        if _warn:
            print("   ⚠ " + _warn)
        print("   （本 patch 声明 %d 条 = SDK %d + 商店 %d + 其它）"
              % (n_sites, len(SDK_SITES), len(SHOP_SITES)))
        if n_sites > SITE_BUDGET:
            # ★ 2026-09-19：**不再判红**（owner 口径：这余量是"人为判定"）。理由见文件头那段 ——
            #   24 的出处是 #41 那条未复现的实验；`GATES.md` §8.1 也写着"条数不是决定因素"。
            #   超线只要求：① 记账（GATES.md §8 + AUDIT §BE）；② 走三条真判据（sitecheck 窗口 /
            #   池实测余格 / 实跑）；③ owner 拍板。
            print("   ⚠ 超记账线：%d > %d（记账口径，不是「必崩」）—— 加站点的真判据是："
                  "`sitecheck.py <addr> <len>` 窗口检查 + 启动日志 `pool:` 的实测余格 + 实跑一局；"
                  "然后在 `GATES.md` §8 记账、`AUDIT.md` §BE 登记，并让 owner 拍板。"
                  % (n_sites, SITE_BUDGET))
        elif SITE_BUDGET - n_sites == 0:
            # ★ 2026-09-18（AUDIT §BE-EG）：余量 0 不是「没问题」，是「下一条就超线」。
            #   预算是**进程级共享资源**：两个会话同一天各加各的，各自的门都是绿的，合起来越线。
            print("   ⚠ 记账线余量 0（%d / %d）：下一条加进来就要走那次记账（`GATES.md` §8 + "
                  "`AUDIT.md` §BE + owner 拍板）；**判据不是这个数**，是 `sitecheck.py` 的窗口检查 + "
                  "启动日志 `pool:` 的实测余格 + 实跑。跨会话共享同一个池子（见 `AUDIT.md` §BE-EG）"
                  % (n_sites, SITE_BUDGET))
        else:
            print("   ✅ 在预算内（余量 %d 条）" % (SITE_BUDGET - n_sites))
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
    # ---- pl0X.sht 组数扩容（AUDIT §CQ）----
    # ★ **无条件**加：发出去的 `pl0X.sht` 永远是 96 格（生成器不看行数），而这条 binhack 是
    #   「数组 → 数据区」那 0xe0 字节错位的唯一解药。只挂在 `alloc` 下面的话，
    #   `make ROWS=58`（step1 回归对分）会发出一份「96 格影像 + 老基点」的 patch = 主炮读垃圾。
    doc["binhacks"].update(emit_sht_binhacks(text, text_va))
    if alloc:
        doc["binhacks"].update(emit_alloc_binhacks(a.rows))
        doc["binhacks"].update(emit_grow_binhacks(a.rows))
        doc["binhacks"].update(emit_unlock_binhacks(unlock_reads))
        doc["binhacks"].update(emit_order_binhacks(find_order_sites(text, text_va)))
        doc["binhacks"].update(emit_menu_binhacks(text, text_va))
        doc["binhacks"].update(emit_count_binhacks(text, text_va))
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
