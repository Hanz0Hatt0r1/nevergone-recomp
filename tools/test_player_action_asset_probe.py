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

    @staticmethod
    def _validator(root: Path) -> Path:
        validator = root / "fake_validator.sh"
        validator.write_text(
            "#!/bin/sh\n"
            "echo parse_status=ok\n"
            "echo input_bytes=$(wc -c < \"$1\")\n"
            "echo bytes_consumed=9\n"
            "echo trailing_bytes=0\n"
            "echo primary_records=1\n"
            "echo primary_record0_frame_key_length=9\n"
            "echo primary_record0_frame_key_hex=6672616d652e706e67\n"
            "echo eof_policy=reported\n",
            encoding="utf-8",
        )
        validator.chmod(0o755)
        return validator

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
            self.assertNotIn("actdata_validation", result)

    def test_directory_hands_unique_match_to_validator(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            data = root / "player" / "p01_actions.plist"
            data.parent.mkdir(parents=True)
            data.write_bytes(self._plist())
            action = root / "actions" / "player_start.actData"
            action.parent.mkdir(parents=True)
            action.write_bytes(b"synthetic")

            result = probe.probe(root, 1, "action28", self._validator(root))
            validation = result["actdata_validation"]
            self.assertEqual(validation["validator_exit"], 0)
            self.assertEqual(validation["input_bytes"], 9)
            self.assertEqual(validation["primary_record0_frame_key_length"], 9)
            self.assertEqual(validation["primary_record0_frame_key_hex"], "6672616d652e706e67")

    def test_zip_resolves_without_extracting_payloads_by_default(self) -> None:
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
            self.assertNotIn("actdata_validation", result)

    def test_zip_validator_uses_bounded_temporary_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            archive = root / "main.obb"
            with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as output:
                output.writestr("cfg/p05_actions.plist", self._plist())
                output.writestr("packed/player_start.actData", b"synthetic")

            result = probe.probe(archive, 5, "action28", self._validator(root))
            validation = result["actdata_validation"]
            self.assertEqual(validation["input_bytes"], 9)
            self.assertEqual(validation["parse_status"], "ok")

    def test_validator_requires_exactly_one_match(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            plist = root / "p01_actions.plist"
            plist.write_bytes(self._plist())
            with self.assertRaisesRegex(ValueError, "exactly one"):
                probe.probe(root, 1, "action28", self._validator(root))

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
