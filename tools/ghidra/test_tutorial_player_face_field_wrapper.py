#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WRAPPER = ROOT / "tools/run_tutorial_player_face_field_probe.py"


class TutorialPlayerFaceWrapperTests(unittest.TestCase):
    def run_dry(self, shared_repo: Path) -> str:
        environment = os.environ.copy()
        environment["REVENGINSTR_REPO"] = str(shared_repo)
        with tempfile.TemporaryDirectory(prefix="player-face-wrapper-") as output_dir:
            result = subprocess.run(
                [
                    sys.executable,
                    str(WRAPPER),
                    "--project-dir", "/tmp/unopened-ghidra-project",
                    "--project-name", "project",
                    "--program", "program.bin",
                    "--output-dir", output_dir,
                    "--dry-run",
                ],
                check=False,
                capture_output=True,
                text=True,
                env=environment,
            )
        if result.returncode:
            raise AssertionError(result.stderr)
        return result.stdout

    def test_uses_shared_runner_and_compatibility_adapter_when_available(self) -> None:
        with tempfile.TemporaryDirectory(prefix="shared-revenginstr-") as temp:
            shared_repo = Path(temp)
            runner = shared_repo / "scripts/run_ghidra_parameter_store_field.sh"
            runner.parent.mkdir(parents=True)
            runner.write_text("#!/usr/bin/env bash\nexit 0\n", encoding="utf-8")
            runner.chmod(0o755)
            exporter = shared_repo / "ghidra/ExportGhidraParameterStoreField.java"
            exporter.parent.mkdir(parents=True)
            exporter.write_text("// generic test stub\n", encoding="utf-8")
            output = self.run_dry(shared_repo)
        self.assertIn("run_ghidra_parameter_store_field.sh", output)
        self.assertIn("adapt_revenginstr_parameter_store_field.py", output)
        self.assertIn("player-face-dir-field.revenginstr.tsv", output)
        self.assertNotIn("ExportParameterStoreField.java", output)

    def test_falls_back_to_local_exporter_when_shared_runner_is_missing(self) -> None:
        with tempfile.TemporaryDirectory(prefix="missing-revenginstr-") as temp:
            output = self.run_dry(Path(temp))
        self.assertIn("ExportParameterStoreField.java", output)
        self.assertNotIn("run_ghidra_parameter_store_field.sh", output)


if __name__ == "__main__":
    unittest.main()
