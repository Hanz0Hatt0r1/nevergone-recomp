#!/usr/bin/env python3
"""Extract the bounded recomp player trace embedded in runtime diagnostics."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path
import re
import sys


FIELDS = (
    "sequence", "kind", "frame", "game_tick", "controls_allowed", "captured",
    "pointer_id", "direction_combo", "combo_released", "movement_action",
    "primary_state", "flip_x", "function_state", "applied_steps", "offset_x",
    "offset_y", "lesson_step", "input_action", "input_pointer", "input_x", "input_y",
)
HEADER = "Player tick trace ("


def extract(text: str) -> list[dict[str, str]]:
    if HEADER not in text:
        raise ValueError("runtime diagnostics do not include a player tick trace")
    result: list[dict[str, str]] = []
    for line in text.splitlines():
        if not re.match(r"^\d+ [UI] ", line):
            continue
        parts = line.split()
        row = dict.fromkeys(FIELDS, "")
        row["sequence"] = parts[0]
        row["kind"] = parts[1]
        if parts[1] == "U" and len(parts) == 17:
            keys = FIELDS[2:17]
            row.update(zip(keys, parts[2:]))
        elif parts[1] == "I" and len(parts) == 8:
            row.update(zip(("frame", "game_tick", "input_action", "input_pointer", "input_x", "input_y"), parts[2:]))
        else:
            raise ValueError(f"malformed player trace row: {line}")
        result.append(row)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("diagnostics", type=Path)
    parser.add_argument("-o", "--output", type=Path, help="CSV output (default: stdout)")
    args = parser.parse_args()
    try:
        rows = extract(args.diagnostics.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if not rows:
        parser.error("no player trace rows were retained")
    output = args.output.open("w", encoding="utf-8", newline="") if args.output else sys.stdout
    try:
        writer = csv.DictWriter(output, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    finally:
        if args.output:
            output.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
