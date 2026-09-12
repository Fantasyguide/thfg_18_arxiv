#!/usr/bin/env python3
"""Apply the TH06NC STD (stage script) runtime map to its Ghidra DB."""

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from _driver import open_program, resolve  # noqa: E402
import th06nc_std_spec as spec  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="validate and preview without saving")
    args = parser.parse_args()

    errors = spec.validate()
    if errors:
        raise SystemExit("invalid TH06NC std spec: " + "; ".join(errors))

    resolved = resolve("th06nc")
    with open_program(
        resolved["proj_dir"], resolved["proj_name"], resolved["program"],
        tx="apply TH06NC stage script runtime map",
        commit=not args.dry_run, expect_md5=spec.EXPECTED_MD5,
    ) as program:
        stats = apply(program)

    mode = "dry-run/rolled-back" if args.dry_run else "saved"
    print("[th06nc-std] %s renamed=%d functions=%d globals=%d labels=%d structs=%d"
          % (mode, stats["renamed"], stats["functions"], stats["globals"],
             stats["labels"], stats["structs"]))


def apply(program):
    from ghidra.app.util.cparser.C import CParser
    from ghidra.program.model.data import DataTypeConflictHandler
    from ghidra.program.model.listing import CodeUnit
    from ghidra.program.model.symbol import SourceType

    user = SourceType.USER_DEFINED
    address_factory = program.getAddressFactory()
    functions = program.getFunctionManager()
    listing = program.getListing()
    symbols = program.getSymbolTable()
    data_types = program.getDataTypeManager()

    def va(number):
        return address_factory.getDefaultAddressSpace().getAddress(number)

    stats = {key: 0 for key in ("renamed", "functions", "globals", "labels", "structs")}

    # 更正：只允许把**明确写出来的旧名**改掉，别的用户名一律拒绝。
    for address, expected_old, new_name, comment in spec.RENAMES:
        location = va(address)
        function = functions.getFunctionAt(location)
        if function is None:
            raise RuntimeError("expected function missing at 0x%x" % address)
        current = function.getName()
        if current not in (expected_old, new_name):
            raise RuntimeError(
                "refusing to rename 0x%x: expected %s (or the corrected %s), found %s"
                % (address, expected_old, new_name, current))
        if current != new_name:
            function.setName(new_name, user)
            stats["renamed"] += 1
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment)

    for address, name, comment in spec.FUNCTIONS:
        location = va(address)
        function = functions.getFunctionAt(location)
        if function is None:
            raise RuntimeError("expected function missing at 0x%x (%s)" % (address, name))
        current = function.getName()
        if current != name:
            if not current.startswith("FUN_"):
                raise RuntimeError("refusing to overwrite %s at 0x%x with %s"
                                   % (current, address, name))
            function.setName(name, user)
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment)
        stats["functions"] += 1

    def put_label(address, name, comment):
        location = va(address)
        for symbol in symbols.getSymbols(location):
            if symbol.getName() == name:
                break
        else:
            symbols.createLabel(location, name, user)
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment)

    for address, name, comment in spec.GLOBALS:
        put_label(address, name, comment)
        stats["globals"] += 1

    for opcode, address, name, comment in spec.OPCODES:
        put_label(address, name, "STD opcode %d. %s" % (opcode, comment))
        stats["labels"] += 1

    for address, name, comment in spec.NC_ONLY_LABELS:
        put_label(address, name, comment)
        stats["labels"] += 1

    for address, comment in spec.RENDER_HELPER_COMMENTS:
        listing.setComment(va(address), CodeUnit.PLATE_COMMENT, comment)

    parser = CParser(data_types)
    for declaration in spec.STRUCT_DECLS:
        data_types.addDataType(parser.parse(declaration), DataTypeConflictHandler.REPLACE_HANDLER)
        stats["structs"] += 1

    return stats


if __name__ == "__main__":
    main()
