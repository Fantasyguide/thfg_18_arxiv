"""test_sound_sites.py —— 拿真 exe 验音效表站点。跑：python3 tests/test_sound_sites.py"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import sites                      # noqa: E402
import sound_sites as S           # noqa: E402

fail = []
n_check = 0


def check(cond, msg):
    global n_check
    n_check += 1
    if not cond:
        fail.append(msg)


text, text_va, _ = sites.load_text_section(sites.EXE)      # 返回 3 元组

# ① 站点数：49 引用 + 2 计数 = 51 改写；另 1 处刻意不改
check(len(S.SND_REFS) == 49, "SND_REFS 应 49 项，实际 %d" % len(S.SND_REFS))
check(len(S.SND_COUNTS) == 2, "SND_COUNTS 应 2 项，实际 %d" % len(S.SND_COUNTS))
check(len(S.SND_UNTOUCHED) == 1, "SND_UNTOUCHED 应 1 项")

# ② 每一处都能在 exe 里对上（scan 内部会抛）
rec = S.scan(text, text_va)
check(len(rec) == 51, "scan 应返回 51 条，实际 %d" % len(rec))

# ③ 地址不重复
vas = [r["va"] for r in rec]
check(len(set(vas)) == len(vas), "站点地址有重复")

# ④ 每条的 pre + 常量 + post 必须正好等于 exe 里那条指令
import struct                                             # noqa: E402
for r in rec:
    off = r["va"] - text_va
    body = struct.pack("<I", r["old"]) if r["width"] == 4 else bytes([r["old"] & 0xff])
    check(r["pre"] + body + r["post"] == text[off:off + r["len"]],
          "0x%06x：pre/const/post 拼不回原指令" % r["va"])

# ⑤ 尺寸算术
check(S.CAVE_SIZE[S.CAVE_CFG] == 0x910, "cfg cave 尺寸应 0x910，实际 0x%x" % S.CAVE_SIZE[S.CAVE_CFG])
check(S.CAVE_SIZE[S.CAVE_SLOTS] == 0xae0, "slot cave 尺寸应 0xae0，实际 0x%x" % S.CAVE_SIZE[S.CAVE_SLOTS])
check(S.CAVE_SIZE[S.CAVE_NAMES] == 0x1a4, "names cave 尺寸应 0x1a4")
check(S.CAVE_SIZE[S.CAVE_BLOBS] == 0x1a0, "blob cave 尺寸应 0x1a0")

# ⑥ 原生尾界的算术必须与 exe 里的立即数一致
check(S.CFG_END == 0x4ca214, "cfg 尾界（+4 游标）应 0x4ca214，实际 0x%x" % S.CFG_END)
check(S.SLOT_END == 0x56cfe4, "slot 尾界应 0x56cfe4")
check(S.BLOB_END == 0x56d104, "blob 尾界应 0x56d104")
for va, want in ((0x4766ef, S.CFG_END), (0x444dbf, S.SLOT_END),
                 (0x4713ad, S.SLOT_END), (0x45a4c5, S.SLOT_END + 8), (0x4713d8, S.BLOB_END)):
    got = [r["old"] for r in rec if r["va"] == va][0]
    check(got == want, "0x%06x 的原生尾界是 0x%x，算出来的是 0x%x" % (va, got, want))

# ⑦ 完整性审计：未覆盖的必须全在白名单里
covered = set()
for r in rec:
    for k in range(r["len"]):
        covered.add(r["va"] + k)

# 同值不同物，不许改。每个值都用 Ghidra 操作数级引用（tooling/ghidra/scripts/find_imm_refs.py）
# 交叉验证过真身，2026-09-05。
FOREIGN = {
    0x4ca210: "输入状态全局：TEST dword [0x4ca210],0x80103（GameThread__on_tick）、"
              "MOVZX EAX,word [0x4ca210]（ReplayManager / MainMenu）。恰好落在 cfg 表体之后",
    0x4ca214: "另一个普通全局：MOV [0x4ca214],EAX（FUN_00474850）、MOV [0x4ca214],ECX（FUN_00401c50）。"
              "只有 0x4766ef 把这个值当 cfg 尾界",
    0x56d104: "SoundManager +0x2388 的字段（blob 数组之后）：MOV byte [0x56d104],0（"
              "GameThread__teardown_and_recount_cards / MainMenu__on_tick）、MOV ESI,0x56d104（消费者 0x477406）",
    0x4b48c0: "ANM 的另一张表：MOV EAX,[ESI*4+0x4b48c0]（AnmVm__run ×2）。紧接在 wav 名表的 NULL 之后",
    0x56cee8: "字节级假阳性：不是 slot 对齐（0x6e4/0x18 = 73.5），classify 无法归类，"
              "Ghidra 操作数级引用一条都没有",
}
un = S.audit(text, text_va, covered)
for va, v, cave, kind, desc in un:
    check(v in FOREIGN, "未覆盖站点 0x%06x（值 0x%x，%s，%s）：%s —— 不在白名单里，"
                        "要么补进 SND_REFS，要么写清它为什么不是站点" % (va, v, cave, kind, desc))
seen_vals = {u[1] for u in un}
missing = set(FOREIGN) - seen_vals
check(not missing, "白名单里有值没被扫到，说明形状变了：%s" % [hex(x) for x in missing])
# 数量也钉住：形状一变就会露出来
check(len(un) == 26, "未覆盖站点应 26 处，实际 %d" % len(un))

print("sound_sites: %d checks, %d failed" % (n_check, len(fail)))
for m in fail:
    print("  FAIL", m)
sys.exit(1 if fail else 0)
