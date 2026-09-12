#!/usr/bin/env python3
"""Apply the log-string-derived TH06NC function names to its Ghidra DB."""

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from _driver import open_program, resolve  # noqa: E402
import th06nc_logname_spec as spec  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    errors = spec.validate()
    if errors:
        raise SystemExit("invalid TH06NC logname spec: " + "; ".join(errors))

    resolved = resolve("th06nc")
    with open_program(
        resolved["proj_dir"], resolved["proj_name"], resolved["program"],
        tx="apply TH06NC log-string derived names",
        commit=not args.dry_run, expect_md5=spec.EXPECTED_MD5,
    ) as program:
        stats = apply(program)

    mode = "dry-run/rolled-back" if args.dry_run else "saved"
    print("[th06nc-lognames] %s named=%d kept=%d" % (mode, stats["named"], stats["kept"]))


def apply(program):
    from ghidra.program.model.listing import CodeUnit
    from ghidra.program.model.symbol import SourceType

    user = SourceType.USER_DEFINED
    af = program.getAddressFactory()
    functions = program.getFunctionManager()
    listing = program.getListing()

    def va(number):
        return af.getDefaultAddressSpace().getAddress(number)

    stats = {"named": 0, "kept": 0}

    rows = [(a, n, "[NC first-hand] Named via the original TH06 log message it emits "
                   "(%d message%s, %s). %s" % (c, "" if c == 1 else "s", k, m))
            for a, n, c, k, m in spec.LOG_NAMES]
    rows.append(spec.WINMAIN)

    for address, name, comment in rows:
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
        listing.setComment(location, CodeUnit.PLATE_COMMENT, comment.strip())

    return stats


if __name__ == "__main__":
    main()
