#!/usr/bin/env python3

import json
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "scene_actions_slice_report.py"

TARGETS = (
    "GameScene::TriggerSceneAction(float)",
    "GameSceneClassLayer::showEventObjTip(...) ",
    "GameSceneObject::triggerSceneAction()",
    "GameSceneObject::isContainsRect(CCRect)",
    "GameSceneObject::isContainsRect(CCRect,const char*)",
    "GameSceneObject::getObjectRect()",
    "SceneActionsSystem::waUpdate(float)",
    "TrapActionsData::loadWBGFile(...) ",
)


def make_slice(unresolved: str | None = None) -> str:
    parts = [
        "# Nevergone SceneActions spatial-trigger/count slice",
        "program=libcocos2dcpp.so",
        "imageBase=00100000",
        "language=ARM:LE:32:v7",
        "",
    ]
    for index, target in enumerate(TARGETS):
        parts.extend(["=" * 60, f"target={target}", f"requestedAddress=0x{index + 1:08x}"])
        if target == unresolved:
            parts.extend(["status=FUNCTION_NOT_FOUND_OR_AMBIGUOUS", ""])
            continue
        parts.extend(
            [
                f"function={target}",
                f"entry=00{index + 1:06x}",
                f"prototype=void {target}",
                "body=[00,01]",
                "",
                "-- CALLERS --",
                "00001000 -> caller @ 00001000",
                "",
                "-- INSTRUCTIONS --",
            ]
        )
        if "waUpdate" in target:
            parts.extend(["00353210\tldr r3,[r4,#0xbc]", "00353214\tcmp r5,r3", "00353218\tbgt 0x00353240"])
        elif "loadWBGFile" in target:
            parts.extend(["00390010\tstr r2,[r4,#188]", "00390014\tcmp r2,#0"])
        else:
            parts.append("0034b100\tbl GameSceneObject::isContainsRect")
        parts.extend(["", "-- DECOMPILE --"])
        if target == "GameSceneClassLayer::showEventObjTip(...) ":
            parts.append("if (obj->isContainsRect(rect)) obj->triggerSceneAction();")
        elif "loadWBGFile" in target:
            parts.append("*(uint *)(this + 0xbc) = count;")
        else:
            parts.append("return;")
        parts.append("")
    return "\n".join(parts)


class SceneActionsSliceReportTest(unittest.TestCase):
    def run_report(self, text: str, *extra: str) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "slice.txt"
            source.write_text(text, encoding="utf-8")
            return subprocess.run(
                ["python3", str(SCRIPT), str(source), "--json", *extra],
                text=True,
                capture_output=True,
                check=False,
            )

    def test_complete_slice_surfaces_bounded_candidates(self) -> None:
        result = self.run_report(make_slice())
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertTrue(report["complete"])
        self.assertEqual(len(report["targets"]), len(TARGETS))
        wa_update = report["frame_count_candidates"]["SceneActionsSystem::waUpdate(float)"]
        self.assertTrue(any("0xbc" in line for line in wa_update["offset_bc"]))
        self.assertTrue(any("bgt" in line.lower() for line in wa_update["branches"]))
        loader = report["frame_count_candidates"]["TrapActionsData::loadWBGFile(...) "]
        self.assertTrue(any("0xbc" in line or "188" in line for line in loader["offset_bc"]))
        spatial = report["spatial_trigger_candidates"]["GameSceneClassLayer::showEventObjTip(...) "]
        self.assertTrue(any("isContainsRect" in line for line in spatial))

    def test_unresolved_target_fails_closed(self) -> None:
        unresolved = "TrapActionsData::loadWBGFile(...) "
        result = self.run_report(make_slice(unresolved))
        self.assertEqual(result.returncode, 3)
        report = json.loads(result.stdout)
        self.assertFalse(report["complete"])
        self.assertIn(unresolved, report["unresolved_targets"])

        allowed = self.run_report(make_slice(unresolved), "--allow-incomplete")
        self.assertEqual(allowed.returncode, 0, allowed.stderr)

    def test_rejects_wrong_file_type(self) -> None:
        result = self.run_report("not a slice\n")
        self.assertEqual(result.returncode, 2)
        self.assertIn("not a SceneActions", result.stderr)


if __name__ == "__main__":
    unittest.main()
