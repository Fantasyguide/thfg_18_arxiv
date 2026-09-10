#!/usr/bin/env python3
"""Apply the reviewed TH06-baseline/TH06NC-delta ECL map to its Ghidra DB."""

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from _driver import REPO, open_program, resolve  # noqa: E402

REPO_EXE = REPO / "local/th06nc/th06nc.exe"
import th06nc_ecl_spec as spec  # noqa: E402
import th06nc_ecl_opcodes as opcodes  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="validate and preview without saving")
    args = parser.parse_args()

    errors = spec.validate()
    if errors:
        raise SystemExit("invalid TH06NC ECL spec: " + "; ".join(errors))

    resolved = resolve("th06nc")
    with open_program(
        resolved["proj_dir"],
        resolved["proj_name"],
        resolved["program"],
        tx="apply TH06NC ECL baseline and deltas",
        commit=not args.dry_run,
        expect_md5=spec.EXPECTED_MD5,
    ) as program:
        stats = apply(program)

    mode = "dry-run/rolled-back" if args.dry_run else "saved"
    print(
        "[th06nc-ecl] %s functions=%d callees=%d(skip %d) globals=%d labels=%d "
        "opcode_labels=%d structs=%d enums=%d param_types=%d"
        % (
            mode,
            stats["functions"],
            stats["callees"],
            stats["callees_skipped"],
            stats["globals"],
            stats["labels"],
            stats["opcode_labels"],
            stats["structs"],
            stats["enums"],
            stats["param_types"],
        )
    )


def apply(program):
    from ghidra.app.util.cparser.C import CParser
    from ghidra.program.model.data import (
        DataTypeConflictHandler,
        DataUtilities,
        EnumDataType,
        IntegerDataType,
        PointerDataType,
        VoidDataType,
    )
    from ghidra.program.model.data.DataUtilities import ClearDataMode
    from ghidra.program.model.listing import CodeUnit, Function, ParameterImpl
    from ghidra.program.model.symbol import SourceType

    user = SourceType.USER_DEFINED
    address_factory = program.getAddressFactory()
    functions = program.getFunctionManager()
    listing = program.getListing()
    symbols = program.getSymbolTable()
    data_types = program.getDataTypeManager()

    def va(number):
        return address_factory.getDefaultAddressSpace().getAddress(number)

    stats = {key: 0 for key in
             ("functions", "globals", "labels", "structs", "enums", "param_types",
              "opcode_labels", "callees", "callees_skipped")}

    # ── 120 个 opcode handler：从 exe 现场解分派表，不手抄 ──────────────
    exe = Path(program.getExecutablePath())
    if not exe.is_file():                      # Ghidra 记的是导入时的路径
        exe = REPO_EXE
    if exe.is_file():
        mapping = opcodes.decode(exe.read_bytes())
        for address, _ops, label, comment in opcodes.group(
                mapping, spec.TH06_OPCODES, spec.NC_ECL_DELTAS):
            location = va(address)
            if symbols.getGlobalSymbol(label, location) is None:
                symbols.createLabel(location, label, user)
            listing.setComment(location, CodeUnit.PRE_COMMENT, comment)
            stats["opcode_labels"] += 1
    else:
        print("      ⚠️ 找不到 exe，跳过 opcode handler 命名")

    for address, name, comment in spec.FUNCTIONS:
        location = va(address)
        function = functions.getFunctionAt(location)
        if function is None:
            raise RuntimeError("expected function missing at 0x%x (%s)" % (address, name))
        current = function.getName()
        if current != name:
            if function.getSymbol().getSource() == user and not current.startswith("FUN_"):
                raise RuntimeError("refusing to overwrite user function %s at 0x%x" % (current, address))
            function.setName(name, user)
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment)
        stats["functions"] += 1

    # ── 交叉比对定位到的子系统函数（CC0 名字）────────────────────────
    for address, name, ops_, confidence in spec.ECL_CALLEE_NAMES:
        location = va(address)
        function = functions.getFunctionAt(location)
        if function is None:
            raise RuntimeError("expected function missing at 0x%x (%s)" % (address, name))
        current = function.getName()
        if current != name:
            if function.getSymbol().getSource() == user and not current.startswith("FUN_"):
                # 已经有人手工命名过就别覆盖——交叉比对是线索，不是权威
                stats["callees_skipped"] += 1
                continue
            function.setName(name, user)
        listing.setComment(
            location, CodeUnit.PLATE_COMMENT,
            "[ECL cross-match, %s] Located by matching the ECL opcodes whose handler calls it "
            "against the CC0 th06-decomp source: opcodes %s. Name follows that upstream."
            % (confidence, ", ".join(str(o) for o in ops_)))
        stats["callees"] += 1

    for address, name, comment in spec.GLOBALS:
        location = va(address)
        existing = symbols.getGlobalSymbol(name, location)
        if existing is None:
            symbols.createLabel(location, name, user)
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment)
        stats["globals"] += 1

    for address, name, comment in spec.LABELS:
        location = va(address)
        existing = symbols.getGlobalSymbol(name, location)
        if existing is None:
            symbols.createLabel(location, name, user)
        listing.setComment(location, CodeUnit.PRE_COMMENT, comment)
        stats["labels"] += 1

    c_parser = CParser(data_types)
    for declaration in spec.STRUCT_DECLS:
        struct_name = declaration.split("{", 1)[0].split()[-1]
        if data_types.getDataType("/" + struct_name) is None:
            parsed = c_parser.parse(declaration)
            data_types.addDataType(parsed, DataTypeConflictHandler.KEEP_HANDLER)
        stats["structs"] += 1

    enum_specs = (
        ("Th06EclOpcode", tuple(enumerate(spec.TH06_OPCODES))),
        ("Th06ncEclOpcodeDelta", tuple(spec.NC_ECL_DELTAS.items())),
        ("Th06ncEclTimelineDelta", tuple(spec.NC_TIMELINE_DELTAS.items())),
    )
    for enum_name, members in enum_specs:
        if data_types.getDataType("/" + enum_name) is None:
            enum_type = EnumDataType(enum_name, 4)
            for value, member in members:
                enum_type.add(member if isinstance(member, str) else member[0], value)
            data_types.addDataType(enum_type, DataTypeConflictHandler.KEEP_HANDLER)
        stats["enums"] += 1

    manager_type = data_types.getDataType("/Th06ncEclManager")
    manager_address = va(0x140A6EB70)
    # 幂等：上一轮已经把结构体铺在这儿的话，CLEAR_ALL_UNDEFINED_CONFLICT_DATA 会
    # 因为「冲突的是已定义数据」直接抛 CodeUnitInsertionException——脚本跑第二遍就炸。
    # 已经是想要的类型就跳过，否则连已定义数据一起清掉再铺。
    existing = listing.getDataAt(manager_address)
    already = (existing is not None and existing.isDefined()
               and existing.getDataType().isEquivalent(manager_type))
    if not already:
        DataUtilities.createData(
            program,
            manager_address,
            manager_type,
            -1,
            False,
            ClearDataMode.CLEAR_ALL_CONFLICT_DATA,
        )

    run_ecl = functions.getFunctionAt(va(0x140022060))
    params = [
        ParameterImpl("self", PointerDataType(manager_type), program),
        ParameterImpl("enemy", PointerDataType(VoidDataType.dataType), program),
    ]
    run_ecl.updateFunction(
        None,
        None,
        Function.FunctionUpdateType.DYNAMIC_STORAGE_FORMAL_PARAMS,
        True,
        user,
        params,
    )
    run_ecl.setReturnType(IntegerDataType.dataType, user)
    stats["param_types"] += 1

    return stats


if __name__ == "__main__":
    main()
