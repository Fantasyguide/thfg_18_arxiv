#!/usr/bin/env python3
"""Apply the CC0-string-derived TH06NC function names to its Ghidra DB."""

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from _driver import open_program, resolve  # noqa: E402
import th06nc_strname_spec as spec  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    errors = spec.validate()
    if errors:
        raise SystemExit("invalid TH06NC strname spec: " + "; ".join(errors))

    resolved = resolve("th06nc")
    with open_program(
        resolved["proj_dir"], resolved["proj_name"], resolved["program"],
        tx="apply TH06NC CC0-string derived names",
        commit=not args.dry_run, expect_md5=spec.EXPECTED_MD5,
    ) as program:
        stats = apply(program)

    mode = "dry-run/rolled-back" if args.dry_run else "saved"
    print("[th06nc-strnames] %s named=%d kept=%d reproduced=%d"
          % (mode, stats["named"], stats["kept"], stats["reproduced"]))


def apply(program):
    from ghidra.program.model.listing import CodeUnit
    from ghidra.program.model.symbol import SourceType

    user = SourceType.USER_DEFINED
    af = program.getAddressFactory()
    functions = program.getFunctionManager()
    listing = program.getListing()

    def va(number):
        return af.getDefaultAddressSpace().getAddress(number)

    stats = {"named": 0, "kept": 0, "reproduced": 0}

    for address, name, count, memo in spec.STR_NAMES:
        location = va(address)
        function = functions.getFunctionAt(location)
        if function is None:
            raise RuntimeError("expected function missing at 0x%x (%s)" % (address, name))
        current = function.getName()
        if current == name:
            stats["kept"] += 1
        else:
            # 只覆盖 Ghidra 自动名；别人手工起的名一律拒绝
            if not current.startswith("FUN_"):
                raise RuntimeError(
                    "refusing to overwrite %s at 0x%x with %s" % (current, address, name))
            function.setName(name, user)
            stats["named"] += 1
        comment = (
            "[NC first-hand] Named via CC0 string literals it references "
            "(%d literal%s, unique on both sides). %s"
            % (count, "" if count == 1 else "s", memo))
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment.strip())

    # 上一轮已入库、本轮独立复现的地址：不改名，只把自证记进 plate comment。
    for address, name in sorted(spec.REPRODUCED.items()):
        function = functions.getFunctionAt(va(address))
        if function is not None and function.getName() == name:
            stats["reproduced"] += 1

    return stats


if __name__ == "__main__":
    main()
