#!/usr/bin/env python3
"""Apply the reviewed TH06NC player-subsystem map to its Ghidra DB."""

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from _driver import open_program, resolve  # noqa: E402
import th06nc_player_spec as spec  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="validate and preview without saving")
    args = parser.parse_args()

    errors = spec.validate()
    if errors:
        raise SystemExit("invalid TH06NC player spec: " + "; ".join(errors))

    resolved = resolve("th06nc")
    with open_program(
        resolved["proj_dir"],
        resolved["proj_name"],
        resolved["program"],
        tx="apply TH06NC player subsystem map",
        commit=not args.dry_run,
        expect_md5=spec.EXPECTED_MD5,
    ) as program:
        stats = apply(program)

    mode = "dry-run/rolled-back" if args.dry_run else "saved"
    print(
        "[th06nc-player] %s functions=%d globals=%d" % (mode, stats["functions"], stats["globals"])
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

    stats = {key: 0 for key in ("functions", "globals")}

    for address, name, comment in spec.FUNCTIONS:
        location = va(address)
        function = functions.getFunctionAt(location)
        if function is None:
            raise RuntimeError("expected function missing at 0x%x (%s)" % (address, name))
        current = function.getName()
        if current != name:
            # 允许改掉我们自己上一轮起错的名字（记在 OUR_OLD_NAMES 里），
            # 但别人手工起的名仍然拒绝覆盖。
            ours = current.startswith("FUN_") or current in spec.OUR_OLD_NAMES
            if function.getSymbol().getSource() == user and not ours:
                raise RuntimeError("refusing to overwrite user function %s at 0x%x" % (current, address))
            function.setName(name, user)
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment)
        stats["functions"] += 1


    for address, name, comment in spec.GRAZE_GLOBALS_LABELS:
        location = va(address)
        if symbols.getGlobalSymbol(name, location) is None:
            symbols.createLabel(location, name, user)
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment)
        stats["globals"] += 1

    return stats


if __name__ == "__main__":
    main()
