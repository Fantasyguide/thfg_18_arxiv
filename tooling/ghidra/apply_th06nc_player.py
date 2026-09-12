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
        "[th06nc-player] %s functions=%d globals=%d inlined=%d stale-cleared=%d"
        % (mode, stats["functions"], stats["globals"], stats["inlined"], stats["stale"])
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

    stats = {key: 0 for key in ("functions", "globals", "inlined", "stale")}

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

    # 先清掉上一轮留下的、这一轮已经不在 spec 里的内联标记。
    # 起因：本轮把 UpdatePlayerBullets 的起点从 0x1400697c0 改到 0x140069740，
    # 旧地址上的标签和注释却留在库里 —— apply 对「地址改了」并不幂等。
    # ⚠️ 必须把**所有**会造 INLINED_ 标签的来源都算进来，否则会把自己刚要写的
    # 标签当成陈旧的删掉再重建 —— 净效果无害，但 stale-cleared 变成噪声，
    # 真正的陈旧标签就藏在噪声里看不见了。
    wanted = {start for start, _e, _n, _w in spec.INLINED_RANGES}
    wanted |= {start for start, _e, _h in spec.DIE_INLINED_AT}
    for symbol in list(symbols.getAllSymbols(False)):
        name = symbol.getName()
        if not name.startswith("INLINED_"):
            continue
        location = symbol.getAddress()
        if location.getOffset() in wanted:
            continue
        listing.setComment(location, CodeUnit.PRE_COMMENT, None)
        symbols.removeSymbolSpecial(symbol)
        stats["stale"] += 1

    # 内联区间：SpawnBullets / UpdatePlayerBullets 在 NC 里没有独立函数，
    # 但库里必须留下标记 —— 否则下次打开只看到 Player__OnUpdate 里一大片裸循环。
    for start, end, name, why in spec.INLINED_RANGES:
        location = va(start)
        label = "INLINED_" + name.split("__")[-1]
        if symbols.getGlobalSymbol(label, location) is None:
            symbols.createLabel(location, label, user)
        listing.setComment(
            location, CodeUnit.PRE_COMMENT,
            "=== %s (inlined here; a standalone function in CC0 TH06) ===\n%s\n"
            "range 0x%x-0x%x"
            % (name, why, start, end))
        stats["inlined"] += 1

    # Die() 的内联点（在 CalcKillBoxCollision / CalcLaserHitbox 里，不在 OnUpdate 里，
    # 所以单独走一遍，不混进 INLINED_RANGES）。
    for start, end, host in spec.DIE_INLINED_AT:
        location = va(start)
        label = "INLINED_Die_in_" + host.split("__")[-1]
        if symbols.getGlobalSymbol(label, location) is None:
            symbols.createLabel(location, label, user)
        listing.setComment(
            location, CodeUnit.PRE_COMMENT,
            "=== Player::Die (inlined here; a standalone function in CC0 TH06) ===\n"
            "Sets playerState=DEAD, InitializeForPopup on invulnerabilityTimer, "
            "respawnTimer=8 (the deathbomb window; TH06 does NOT write it here), "
            "sound 4 = PICHUN, laserTimer[0..1].SetCurrent(2)."
            + ("" if end is None else "\nrange 0x%x-0x%x" % (start, end)))
        stats["inlined"] += 1

    return stats


if __name__ == "__main__":
    main()
