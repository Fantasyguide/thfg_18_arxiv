# scan_vm_field_readers.py — 扫「谁读了 AnmVm 的某个字段」（TEST/AND/CMP [reg+disp], imm）。
#   source tooling/env.sh && python3 tooling/ghidra/scripts/scan_vm_field_readers.py th18 0x538 [mask_hex]
# 用途：`flags_hi`（+0x538）这类位的**读点**是判断「某条指令写了这个位到底影响什么」的唯一入口
# （写点用 find_struct_writes.py，读点用这个）。只读，不改库。
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from _driver import open_program, resolve  # noqa: E402

version = sys.argv[1]
disp = sys.argv[2]
mask = sys.argv[3] if len(sys.argv) > 3 else None
r = resolve(version)
with open_program(r["proj_dir"], r["proj_name"], r["program"]) as prog:
    fm = prog.getFunctionManager()
    it = prog.getListing().getInstructions(True)
    n = 0
    while it.hasNext():
        ins = it.next()
        s = str(ins)
        if disp not in s:
            continue
        mn = ins.getMnemonicString().upper()
        if mn not in ("TEST", "AND", "CMP"):
            continue
        if mask and mask.lower().lstrip("0x") not in s.lower().replace("0x", ""):
            continue
        f = fm.getFunctionContaining(ins.getAddress())
        print(f"{ins.getAddress()}  {f.getName() if f else '?':<44} {s}")
        n += 1
    print(f"# {n} reads of [{disp}]" + (f" with mask {mask}" if mask else ""))
