from __future__ import annotations

import csv
import io
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parent))
from summarize_player_tick_trace import write_csv


def event(name: str, frame: int, ms: int, r1: str = "0x0") -> str:
    payload = {
        "kind": "call-enter",
        "name": name,
        "frame": frame,
        "frida_time_ms": ms,
        "registers": {"r1": r1},
    }
    return json.dumps({"frida": {"payload": payload}})


class SummarizePlayerTickTraceTest(unittest.TestCase):
    def test_groups_calls_by_game_scene_tick_boundary(self) -> None:
        trace = "\n".join((
            event("GameScene::setDirComboState", 7, 1000, "0x11"),
            event("GameScene::setControlsDirectionState", 7, 1001, "0x13"),
            event("GameScene::setFunctionState", 7, 1002, "0x0"),
            event("MEPlayer::setActionState", 7, 1003, "0x1"),
            event("MEPlayer::movePlayer", 7, 1004),
            event("GameScene::setDirComboState", 8, 1030, "0x0"),
            event("GameScene::setControlsState", 8, 1031, "0x13"),
        ))
        output = io.StringIO()
        count = write_csv(io.StringIO(trace), output)
        rows = list(csv.DictReader(io.StringIO(output.getvalue())))

        self.assertEqual(count, 2)
        self.assertEqual(rows[0]["tick"], "1")
        self.assertEqual(rows[0]["render_frame"], "7")
        self.assertEqual(rows[0]["elapsed_ms"], "0")
        self.assertEqual(rows[0]["dir_combo"], "0x11")
        self.assertEqual(rows[0]["direction"], "0x13")
        self.assertEqual(rows[0]["function_state"], "0x0")
        self.assertEqual(rows[0]["set_action_state_args"], "0x1")
        self.assertEqual(rows[0]["move_player_calls"], "1")
        self.assertEqual(rows[1]["elapsed_ms"], "30")
        self.assertEqual(rows[1]["controls_state"], "0x13")


if __name__ == "__main__":
    unittest.main()
