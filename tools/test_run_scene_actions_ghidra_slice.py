#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "run_scene_actions_ghidra_slice.py"


class SceneActionsGhidraWrapperTest(unittest.TestCase):
    def test_dry_run_builds_bounded_pipeline(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            result = subprocess.run(
                [
                    "python3",
                    str(SCRIPT),
                    "--ghidra",
                    "/opt/ghidra",
                    "--project-dir",
                    str(root / "projects"),
                    "--project-name",
                    "nevergone",
                    "--program",
                    "/com.hippiegame.nevergone.apk/lib/armeabi-v7a/libcocos2dcpp.so",
                    "--output-dir",
                    str(root / "out"),
                    "--dry-run",
                ],
                text=True,
                capture_output=True,
                check=False,
            )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = [line for line in result.stdout.splitlines() if line.strip()]
        self.assertEqual(len(lines), 3)
        self.assertIn("analyzeHeadless", lines[0])
        self.assertIn("ExportSceneActionTriggerSlice.java", lines[0])
        self.assertIn("-noanalysis", lines[0])
        self.assertIn("scene_actions_slice_report.py", lines[1])
        self.assertIn("scene-actions-trigger-report.md", lines[1])
        self.assertIn("--json", lines[2])
        self.assertIn("scene-actions-trigger-report.json", lines[2])

    def test_missing_ghidra_fails_before_execution(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            project = root / "project"
            project.mkdir()
            result = subprocess.run(
                [
                    "python3",
                    str(SCRIPT),
                    "--ghidra",
                    str(root / "missing-ghidra"),
                    "--project-dir",
                    str(project),
                    "--project-name",
                    "nevergone",
                    "--program",
                    "libcocos2dcpp.so",
                ],
                text=True,
                capture_output=True,
                check=False,
            )
        self.assertEqual(result.returncode, 2)
        self.assertIn("missing analyzeHeadless", result.stderr)


if __name__ == "__main__":
    unittest.main()
