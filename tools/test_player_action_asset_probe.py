#!/usr/bin/env python3

from __future__ import annotations

import plistlib
from pathlib import Path
import tempfile
import unittest
import zipfile

import player_action_asset_probe as probe


class PlayerActionAssetProbeTests(unittest.TestCase):
    @staticmethod
    def _plist() -> bytes:
        return plistlib.dumps({
            "action01": {"name": "idle.actData"},
            "action28": {"name": "player_start.actData"},
        })

    def test_directory_resolves_action28_and_actdata(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            data = root / "player" / "p01_actions.plist"
            data.parent.mkdir(parents=True)
            data.write_bytes(self._plist())
            action = root / "actions" / "player_start.actData"
            action.parent.mkdir(parents=True)
            action.write_bytes(b"synthetic")

            result = probe.probe(root, 1, "action28")
            self.assertEqual(result["action_name"], "player_start.actData")
            self.assertEqual(result["matching_actdata_paths"], ["actions/player_start.actData"])

    def test_zip_resolves_without_extracting_payloads(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "main.obb"
            with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as output:
                output.writestr("cfg/p05_actions.plist", self._plist())
                output.writestr("packed/player_start.actData", b"synthetic")
                output.writestr("other.bin", b"ignored")

            result = probe.probe(archive, 5, "action28")
            self.assertEqual(result["container"], "zip")
            self.assertEqual(result["actions_plist"], "cfg/p05_actions.plist")
            self.assertEqual(result["matching_actdata_paths"], ["packed/player_start.actData"])

    def test_missing_or_malformed_action_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            plist = root / "p01_actions.plist"
            plist.write_bytes(plistlib.dumps({"action28": {"name": ""}}))
            with self.assertRaises(ValueError):
                probe.probe(root, 1, "action28")
            with self.assertRaises(ValueError):
                probe.validate_action_key("action0")
            with self.assertRaises(ValueError):
                probe.validate_action_key("28")

    def test_duplicate_player_plists_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for folder in ("a", "b"):
                path = root / folder / "p01_actions.plist"
                path.parent.mkdir(parents=True)
                path.write_bytes(self._plist())
            with self.assertRaises(ValueError):
                probe.probe(root, 1, "action28")


if __name__ == "__main__":
    unittest.main()
