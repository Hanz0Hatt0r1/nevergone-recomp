#!/usr/bin/env python3

from __future__ import annotations

import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

import obb_unpack


class ObbUnpackTests(unittest.TestCase):
    def test_zip_extracts_safely(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            archive = root / "main.obb"
            with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as output:
                output.writestr("gamescene_ui/ServerList/XMLFile1.xml", b"<root/>")
                output.writestr("Common/btn_standard_a.png", b"not-a-real-png")

            extracted = root / "out"
            ok, message = obb_unpack.unpack(archive, extracted, None)
            self.assertTrue(ok, message)
            self.assertEqual(
                (extracted / "gamescene_ui/ServerList/XMLFile1.xml").read_bytes(),
                b"<root/>",
            )
            self.assertTrue((extracted / "Common/btn_standard_a.png").exists())

    def test_zip_rejects_path_traversal(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            archive = root / "bad.obb"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("../escape.bin", b"bad")

            with self.assertRaises(ValueError):
                obb_unpack._extract_zip(archive, root / "out")
            self.assertFalse((root / "escape.bin").exists())

    def test_parses_android_jobb_footer(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "main.9.com.hippiegame.nevergone.obb"
            payload = bytearray(4096)
            payload[3:11] = b"MSDOS5.0"
            payload[54:59] = b"FAT16"
            payload[510:512] = b"\x55\xaa"

            package = b"com.hippiegame.nevergone"
            flags = obb_unpack.JOBB_OVERLAY | obb_unpack.JOBB_SALTED
            footer = (
                struct.pack("<III", 1, 9, flags)
                + b"12345678"
                + struct.pack("<I", len(package))
                + package
            )
            tag = struct.pack("<II", len(footer), obb_unpack.JOBB_SIGNATURE)
            path.write_bytes(bytes(payload) + footer + tag)

            parsed = obb_unpack._parse_jobb_footer(path)
            self.assertIsNotNone(parsed)
            assert parsed is not None
            self.assertTrue(parsed["valid"])
            self.assertEqual(parsed["package_name"], "com.hippiegame.nevergone")
            self.assertEqual(parsed["package_version"], 9)
            self.assertTrue(parsed["overlay"])
            self.assertTrue(parsed["salted_encrypted"])
            self.assertEqual(parsed["payload_size"], len(payload))
            self.assertTrue(obb_unpack._looks_like_fat(path, len(payload))["possible"])

    def test_inspect_finds_project_needles_and_writes_small_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            path = root / "custom.obb"
            path.write_bytes(
                b"CUSTOM\x00" +
                b"gamescene_ui/ServerList/border1.png\x00" +
                b"Common/btn_standard_a.png\x00" +
                b"x" * 2048
            )
            report = obb_unpack.inspect(path)
            strings = report["interesting_strings"]
            self.assertIn("ServerList", strings)
            self.assertIn("btn_standard", strings)

            diagnostics = root / "diag"
            obb_unpack._write_diagnostics(path, diagnostics, report)
            self.assertTrue((diagnostics / "report.json").exists())
            self.assertTrue((diagnostics / "head.bin").exists())
            self.assertTrue((diagnostics / "tail.bin").exists())
            loaded = json.loads((diagnostics / "report.json").read_text(encoding="utf-8"))
            self.assertEqual(loaded["size"], path.stat().st_size)


if __name__ == "__main__":
    unittest.main()
