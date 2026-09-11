#!/usr/bin/env python3
"""Apply the TH06NC in-exe shot-table map to its Ghidra DB."""

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from _driver import open_program, resolve  # noqa: E402
import th06nc_sht_spec as spec  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    errors = spec.validate()
    if errors:
        raise SystemExit("invalid TH06NC sht spec: " + "; ".join(errors))

    resolved = resolve("th06nc")
    with open_program(
        resolved["proj_dir"], resolved["proj_name"], resolved["program"],
        tx="apply TH06NC in-exe shot tables",
        commit=not args.dry_run, expect_md5=spec.EXPECTED_MD5,
    ) as program:
        stats = apply(program)

    mode = "dry-run/rolled-back" if args.dry_run else "saved"
    print("[th06nc-sht] %s globals=%d comments=%d" % (mode, stats["globals"], stats["comments"]))


def apply(program):
    from ghidra.program.model.listing import CodeUnit
    from ghidra.program.model.symbol import SourceType

    user = SourceType.USER_DEFINED
    address_factory = program.getAddressFactory()
    listing = program.getListing()
    symbols = program.getSymbolTable()

    def va(number):
        return address_factory.getDefaultAddressSpace().getAddress(number)

    stats = {"globals": 0, "comments": 0}

    for address, name, comment in spec.GLOBALS:
        location = va(address)
        for symbol in symbols.getSymbols(location):
            if symbol.getName() == name:
                break
        else:
            symbols.createLabel(location, name, user)
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment)
        stats["globals"] += 1

    for address, comment in spec.FUNCTION_COMMENTS:
        listing.setComment(va(address), CodeUnit.PLATE_COMMENT, comment)
        stats["comments"] += 1

    return stats


if __name__ == "__main__":
    main()
