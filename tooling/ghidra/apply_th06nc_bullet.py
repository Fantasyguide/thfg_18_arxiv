#!/usr/bin/env python3
"""Apply the reviewed TH06NC bullet-subsystem map to its Ghidra DB."""

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from _driver import open_program, resolve  # noqa: E402
import th06nc_bullet_spec as spec  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="validate and preview without saving")
    args = parser.parse_args()

    errors = spec.validate()
    if errors:
        raise SystemExit("invalid TH06NC bullet spec: " + "; ".join(errors))

    resolved = resolve("th06nc")
    with open_program(
        resolved["proj_dir"],
        resolved["proj_name"],
        resolved["program"],
        tx="apply TH06NC bullet subsystem map",
        commit=not args.dry_run,
        expect_md5=spec.EXPECTED_MD5,
    ) as program:
        stats = apply(program)

    mode = "dry-run/rolled-back" if args.dry_run else "saved"
    print("[th06nc-bullet] %s functions=%d pools=%d" % (mode, stats["functions"], stats["pools"]))


def apply(program):
    from ghidra.program.model.listing import CodeUnit
    from ghidra.program.model.symbol import SourceType

    user = SourceType.USER_DEFINED
    address_factory = program.getAddressFactory()
    functions = program.getFunctionManager()
    listing = program.getListing()
    symbols = program.getSymbolTable()

    def va(number):
        return address_factory.getDefaultAddressSpace().getAddress(number)

    stats = {"functions": 0, "pools": 0}

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

    for address, name, comment in spec.POOL_LABELS:
        location = va(address)
        if symbols.getGlobalSymbol(name, location) is None:
            symbols.createLabel(location, name, user)
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment)
        stats["pools"] += 1

    return stats


if __name__ == "__main__":
    main()
