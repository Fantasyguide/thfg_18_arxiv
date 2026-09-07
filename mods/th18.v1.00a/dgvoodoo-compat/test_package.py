#!/usr/bin/env python3
"""Contract tests for the dgVoodoo compatibility package and modkit integration."""

import hashlib
import json
import unittest
from pathlib import Path


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = HERE / "package"
MODKIT = ROOT / "local/vendor/th18_modkit"
PUBLISHED = MODKIT / "compat/dgvoodoo"
DLL_SHA256 = "0c5704900f7b24ed1bc376f7b377f4df3e7edf3cd8506761eb959be8580b3e2d"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


class DgVoodooPackageTests(unittest.TestCase):
    def test_package_has_audited_32_bit_proxy_and_settings(self):
        metadata = load_json(SOURCE / "package.json")
        self.assertEqual(metadata["id"], "dgvoodoo")
        self.assertEqual(metadata["files"], ["D3D9.dll", "dgVoodoo.conf"])

        dll = (SOURCE / "D3D9.dll").read_bytes()
        self.assertEqual(hashlib.sha256(dll).hexdigest(), DLL_SHA256)
        self.assertEqual(dll[:2], b"MZ")
        pe = int.from_bytes(dll[0x3C:0x40], "little")
        self.assertEqual(dll[pe + 4 : pe + 6], b"\x4c\x01")  # IMAGE_FILE_MACHINE_I386

        config = (SOURCE / "dgVoodoo.conf").read_text(encoding="utf-8-sig")
        for setting in (
            "Version                              = 0x286",
            "ScalingMode                          = stretched_ar",
            "PresentationModel                    = flip_discard",
            "WindowedAttributes                   = borderless, fullscreensize",
            "dgVoodooWatermark                   = false",
        ):
            self.assertIn(setting, config)

    def test_published_package_is_identical_and_disabled_by_default(self):
        for name in ("package.json", "D3D9.dll", "dgVoodoo.conf", "README.md"):
            self.assertEqual((SOURCE / name).read_bytes(), (PUBLISHED / name).read_bytes())
        state = load_json(MODKIT / "mods/enabled.default.json")
        self.assertNotIn("dgvoodoo", state.get("compat", []))

    def test_launcher_deploys_compat_before_starting_thcrap(self):
        launcher = (MODKIT / "tools/modloader.ps1").read_text(encoding="utf-8-sig")
        for marker in (
            "$CompatDir",
            "function Get-CompatList",
            "function Sync-Compat",
            "compat =",
        ):
            self.assertIn(marker, launcher)
        self.assertLess(launcher.index("Sync-Compat "), launcher.index("Start-Process "))


if __name__ == "__main__":
    unittest.main()
