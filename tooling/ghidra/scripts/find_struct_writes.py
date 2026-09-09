# find_struct_writes.py — 扫「谁写了某个结构体偏移」：全 .text 找**写内存**且位移等于给定值的指令。
#   source tooling/env.sh && python tooling/ghidra/scripts/find_struct_writes.py th18 0x2c [--global 0x4cf2ac]
#
# 与 find_imm_refs.py 的分工：那个找「谁引用了某个绝对地址」，这个找「谁写 [base+disp]」。
# 结构体字段没有绝对地址（base 是寄存器），所以只能按位移扫——因此**必须再用 base 过滤**，
# 否则满屏都是别的结构体的同名偏移。--global 只保留「同一函数里引用过该全局地址」的函数。
#
# ⚠️ Ghidra 的 OperandType.WRITE 位在这份 DB 上**恒为 0**（实测 694 条 [x+0x2c] 全为 False），
# 所以「是不是写」靠助记符判：x86 里目的操作数基本都在 op0，读操作（CMP/TEST/PUSH/…）排除。
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from _driver import open_program, resolve  # noqa: E402

READONLY_MNEM = ("CMP", "TEST", "PUSH", "CALL", "JMP", "JZ", "JNZ", "JS", "JNS", "LEA",
                 "MOVSS_XMM", "MOVZX", "MOVSX")


def val(obj):
    cn = obj.getClass().getSimpleName()
    if cn == "Scalar":
        return obj.getValue() & 0xFFFFFFFF
    if "Address" in cn:
        return obj.getOffset() & 0xFFFFFFFF
    return None


args = sys.argv[1:]
version = args[0]
target = int(args[1], 16)
only_global = None
if "--global" in args:
    only_global = int(args[args.index("--global") + 1], 16)

r = resolve(version)
with open_program(r["proj_dir"], r["proj_name"], r["program"]) as prog:
    fm = prog.getFunctionManager()
    listing = prog.getListing()

    allowed = None
    if only_global is not None:
        allowed = set()
        for ins in listing.getInstructions(True):
            for i in range(ins.getNumOperands()):
                if any(val(o) == only_global for o in ins.getOpObjects(i)):
                    fn = fm.getFunctionContaining(ins.getAddress())
                    if fn:
                        allowed.add(fn.getEntryPoint().getOffset())

    for ins in listing.getInstructions(True):
        fn = fm.getFunctionContaining(ins.getAddress())
        if allowed is not None and (fn is None or fn.getEntryPoint().getOffset() not in allowed):
            continue
        if ins.getMnemonicString().upper() in READONLY_MNEM:
            continue
        disp = base = None
        for obj in ins.getOpObjects(0):          # op0 = 目的操作数
            cn = obj.getClass().getSimpleName()
            if cn == "Scalar":
                disp = obj.getValue() & 0xFFFFFFFF
            elif cn == "Register":
                base = obj.getName()
        if disp == target and base and base.upper() not in ("ESP", "EBP"):
            print(f"{ins.getAddress()}  {fn.getName() if fn else '?':<40} base={base:<5} {ins}")
