"""test_sound_emit.py —— 音效表 codecave / binhack 发射器的形状检查。

跑：python3 tests/test_sound_emit.py
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import sites                                                        # noqa: E402
import sound_sites as S                                             # noqa: E402
from sound_emit import emit_sound_codecaves, emit_sound_binhacks    # noqa: E402

fail = []
n_check = 0


def check(cond, msg):
    global n_check
    n_check += 1
    if not cond:
        fail.append(msg)


def rendered_len(code):
    """thcrap 把 <…> / [.…] 表达式渲染成 4 字节。"""
    return len(re.sub(r"<[^>]+>", "xxxxxxxx", code)) // 2


# ---- codecaves ------------------------------------------------------------
c = emit_sound_codecaves()
check(set(c) == set(S.CAVE_SIZE) | {S.CAVE_INIT}, "codecave 名集合不对：%s" % sorted(c))
check(c[S.CAVE_INIT]["export"] is True, "patch_init 必须 export，否则 thcrap 不会调它")
check(S.CAVE_INIT.endswith("_patch_init"), "名字必须以 _patch_init 结尾才会被 thcrap 调用")
check(c[S.CAVE_INIT]["access"] == "RX", "patch_init 是代码，access 应为 RX")
for name in S.CAVE_SIZE:
    check(c[name]["access"] == "RW", "%s 是数据，access 应为 RW" % name)
    check(int(c[name]["size"], 16) == S.CAVE_SIZE[name], "%s 尺寸不对" % name)

check(S.CAVE_SIZE[S.CAVE_CFG] == 0x910, "cfg cave 应 0x910，实际 0x%x" % S.CAVE_SIZE[S.CAVE_CFG])
check(S.CAVE_SIZE[S.CAVE_SLOTS] == 0xae0, "slot cave 应 0xae0，实际 0x%x" % S.CAVE_SIZE[S.CAVE_SLOTS])
check(S.CAVE_SIZE[S.CAVE_NAMES] == 0x1a4, "names cave 应 0x1a4")
check(S.CAVE_SIZE[S.CAVE_BLOBS] == 0x1a0, "blob cave 应 0x1a0")

code = c[S.CAVE_INIT]["code"]
# ★ 版权红线：patch 里不许出现原生数据。原生表一律用 Rx 相对模块基址在运行时取。
check("<Rx%x>" % S.CFG_RVA in code, "cfg 必须用 <Rx…> 取，不能写死绝对地址、更不能内联表内容")
check("<Rx%x>" % S.NAMES_RVA in code, "wav 名表同上")
check(code.startswith("fc60"), "开头必须是 cld; pushad")
check(code.endswith("61c3"), "结尾必须是 popad; ret（cdecl，调用方清栈）")
check("ret 4" not in code and not code.endswith("c204"), "不能是 ret 4 —— mod_call_type 是 cdecl")

m = re.search(r"ab83c710404b75([0-9a-f]{2})", code)
check(m is not None, "找不到新行骨架循环（stosd; add edi,0x10; inc eax; dec ebx; jnz）")
if m:
    body_len = len("ab83c710404b") // 2
    check(int(m.group(1), 16) == (256 - (body_len + 2)) & 0xff,
          "jnz 回跳距离算错：%s" % m.group(1))
check(struct.pack("<I", S.CFG_ROWS * S.CFG_ROW // 4).hex() in code, "cfg 拷贝 dword 数应为 420")
check(struct.pack("<I", S.NAMES_N + 1).hex() in code, "wav 名拷贝 dword 数应为 73（含 NULL 终止）")
check(struct.pack("<I", S.CFG_ROWS).hex() in code, "骨架循环的起始槽号应为 84")
check(struct.pack("<I", S.NEW_N).hex() in code, "骨架循环的计数应为 32")

# ---- binhacks -------------------------------------------------------------
text, text_va, _ = sites.load_text_section(sites.EXE)               # 返回 3 元组
bh = emit_sound_binhacks(text, text_va)
check(len(bh) == 51, "应 51 条 binhack，实际 %d" % len(bh))

for name, b in bh.items():
    va = int(b["addr"], 16)
    exp = bytes.fromhex(b["expected"])
    check(text[va - text_va:va - text_va + len(exp)] == exp,
          "%s：expected 与 exe 不符" % name)
    check(rendered_len(b["code"]) == len(exp),
          "%s：改写后长度变了（%d vs %d）" % (name, rendered_len(b["code"]), len(exp)))
    # ★ 绝对表达式必须是尖括号；方括号是相对偏移
    check("[codecave:" not in b["code"], "%s：用了方括号（相对），必须用尖括号" % name)

# 逐条特判：这几处错了不会崩，只会静默不对
check(bh["snd_4766ef"]["code"].endswith(
        "<codecave:%s+%x>" % (S.CAVE_CFG, S.CFG_CURSOR + S.CFG_ROWS_N * S.CFG_ROW)),
      "0x4766ef 的 cfg 尾界必须是 +4 游标（base+4+116*0x14），少 4 字节就少建一行")
check(bh["snd_4713d8"]["code"].endswith("<codecave:%s+%x>" % (S.CAVE_BLOBS, S.BLOB_N * 4)),
      "0x4713d8 的释放尾界必须只到原生 72（跨堆 free 会崩）")
check(bh["snd_45ff38"]["code"].endswith("<codecave:%s+%x>" % (S.CAVE_SLOTS, 20 * S.SLOT_SIZE)),
      "0x45ff38 必须指向槽 20（se_lazer02 常驻激光音）")
check(bh["snd_45a4c5"]["code"].endswith(
        "<codecave:%s+%x>" % (S.CAVE_SLOTS, S.CFG_ROWS_N * S.SLOT_SIZE + 8)),
      "0x45a4c5 的重播尾界游标是 slot+8")
# 两处计数界：一处 imm32、一处 imm8
check(bh["snd_401139"]["code"] == "81fa" + struct.pack("<I", S.CFG_ROWS_N * S.SLOT_SIZE).hex(),
      "0x401139 应是 imm32 = 0xae0，实际 %s" % bh["snd_401139"]["code"])
check(bh["snd_476472"]["code"] == "83fa%02x" % S.CFG_ROWS_N,
      "0x476472 应是 imm8 = 0x74，实际 %s" % bh["snd_476472"]["code"])

# 刻意不改的那处不许出现在 binhack 里
check("snd_4767d8" not in bh, "0x4767d8（预加载 wav 数）刻意不改，不该生成 binhack")

print("sound_emit: %d checks, %d failed" % (n_check, len(fail)))
for msg in fail:
    print("  FAIL", msg)
sys.exit(1 if fail else 0)
