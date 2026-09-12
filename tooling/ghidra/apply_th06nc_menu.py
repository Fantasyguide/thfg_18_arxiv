#!/usr/bin/env python3
"""Apply the reviewed TH06NC result-screen unlock-cheat map to its Ghidra DB."""

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from _driver import open_program, resolve  # noqa: E402
import th06nc_menu_spec as spec  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="validate and preview without saving")
    args = parser.parse_args()

    errors = spec.validate()
    if errors:
        raise SystemExit("invalid TH06NC menu spec: " + "; ".join(errors))

    resolved = resolve("th06nc")
    with open_program(
        resolved["proj_dir"],
        resolved["proj_name"],
        resolved["program"],
        tx="apply TH06NC result-screen unlock cheat map",
        commit=not args.dry_run,
        expect_md5=spec.EXPECTED_MD5,
    ) as program:
        stats = apply(program)

    mode = "dry-run/rolled-back" if args.dry_run else "saved"
    print(
        "[th06nc-menu] %s functions=%d globals=%d labels=%d structs=%d enums=%d"
        % (mode, stats["functions"], stats["globals"], stats["labels"], stats["structs"], stats["enums"])
    )


def apply(program):
    from ghidra.app.util.cparser.C import CParser
    from ghidra.program.model.data import DataTypeConflictHandler, EnumDataType
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

    stats = {key: 0 for key in ("functions", "globals", "labels", "structs", "enums")}

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

    for address, name, comment in spec.GLOBALS:
        location = va(address)
        if symbols.getGlobalSymbol(name, location) is None:
            symbols.createLabel(location, name, user)
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment)
        stats["globals"] += 1

    for address, name, comment in spec.LABELS:
        location = va(address)
        if symbols.getGlobalSymbol(name, location) is None:
            symbols.createLabel(location, name, user)
        listing.setComment(location, CodeUnit.PRE_COMMENT, comment)
        stats["labels"] += 1

    c_parser = CParser(data_types)
    for declaration in spec.STRUCT_DECLS:
        struct_name = declaration.split("{", 1)[0].split()[-1]
        if data_types.getDataType("/" + struct_name) is None:
            data_types.addDataType(c_parser.parse(declaration), DataTypeConflictHandler.KEEP_HANDLER)
        stats["structs"] += 1

    enum_specs = (
        ("Th06ncResultScreenState", tuple(spec.RESULT_SCREEN_STATES.items())),
        ("Th06ncPadCheatButton", tuple((bit, name) for bit, name in spec.PAD_CHEAT_BITS)),
    )
    for enum_name, members in enum_specs:
        if data_types.getDataType("/" + enum_name) is None:
            enum_type = EnumDataType(enum_name, 4)
            for value, member in members:
                enum_type.add(member, value)
            data_types.addDataType(enum_type, DataTypeConflictHandler.KEEP_HANDLER)
        stats["enums"] += 1

    return stats


if __name__ == "__main__":
    main()
