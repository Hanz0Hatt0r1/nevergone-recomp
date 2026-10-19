from __future__ import annotations

import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).parent))
from extract_player_tick_trace import extract


class ExtractPlayerTickTraceTest(unittest.TestCase):
    def test_extracts_ordered_update_and_touch_records(self) -> None:
        report = """Player tick trace (U=update, I=input)
retained rows: 2
17 U 9 41 1 1 3 0x11 0 0x1 19 0 0x1c 8 12.5 -4.25 run
18 I 9 41 0 3 500 250
"""
        rows = extract(report)
        self.assertEqual([row["sequence"] for row in rows], ["17", "18"])
        self.assertEqual(rows[0]["kind"], "U")
        self.assertEqual(rows[0]["game_tick"], "41")
        self.assertEqual(rows[0]["direction_combo"], "0x11")
        self.assertEqual(rows[0]["offset_x"], "12.5")
        self.assertEqual(rows[1]["kind"], "I")
        self.assertEqual(rows[1]["input_action"], "0")
        self.assertEqual(rows[1]["input_y"], "250")

    def test_requires_trace_section(self) -> None:
        with self.assertRaisesRegex(ValueError, "do not include"):
            extract("no trace")


if __name__ == "__main__":
    unittest.main()
