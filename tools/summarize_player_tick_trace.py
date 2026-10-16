#!/usr/bin/env python3
"""Reduce a captured original Never Gone Frida trace to GameScene update rows."""

from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path
from typing import TextIO


BOUNDARY = "GameScene::setDirComboState"
FIELDS = (
    "tick",
    "render_frame",
    "elapsed_ms",
    "dir_combo",
    "direction",
    "function_state",
    "controls_state",
    "set_action_state_args",
    "move_player_calls",
    "calls",
)


def _hex_arg(event: dict, index: int) -> str:
    registers = event.get("registers") or {}
    return registers.get(f"r{index}", "")


def rows(source: TextIO):
    """Yield one row per observed GameScene tick boundary.

    The hooks are emitted in call order. A new dir-combo setter starts a row;
    subsequent GameScene setters and player movement calls belong to it.
    Frame numbers are render boundaries, not assumed to equal game ticks.
    """
    tick = 0
    first_ms: int | None = None
    current: dict | None = None

    for line_number, line in enumerate(source, 1):
        try:
            payload = json.loads(line).get("frida", {}).get("payload", {})
        except (json.JSONDecodeError, AttributeError) as error:
            raise ValueError(f"invalid trace record at line {line_number}") from error

        if payload.get("kind") != "call-enter":
            continue
        name = payload.get("name", "")
        if name == BOUNDARY:
            if current is not None:
                yield current
            tick += 1
            timestamp = payload.get("frida_time_ms")
            if isinstance(timestamp, int) and first_ms is None:
                first_ms = timestamp
            current = {
                "tick": tick,
                "render_frame": payload.get("frame", ""),
                "elapsed_ms": timestamp - first_ms if isinstance(timestamp, int) and first_ms is not None else "",
                "dir_combo": _hex_arg(payload, 1),
                "direction": "",
                "function_state": "",
                "controls_state": "",
                "set_action_state_args": [],
                "move_player_calls": 0,
                "calls": [],
            }
        if current is None:
            continue
        current["calls"].append(name)
        if name == "GameScene::setControlsDirectionState":
            current["direction"] = _hex_arg(payload, 1)
        elif name == "GameScene::setFunctionState":
            current["function_state"] = _hex_arg(payload, 1)
        elif name == "GameScene::setControlsState":
            current["controls_state"] = _hex_arg(payload, 1)
        elif name == "MEPlayer::setActionState":
            current["set_action_state_args"].append(_hex_arg(payload, 1))
        elif name == "MEPlayer::movePlayer":
            current["move_player_calls"] += 1

    if current is not None:
        yield current


def write_csv(source: TextIO, destination: TextIO) -> int:
    writer = csv.DictWriter(destination, fieldnames=FIELDS, lineterminator="\n")
    writer.writeheader()
    count = 0
    for row in rows(source):
        row["set_action_state_args"] = ",".join(row["set_action_state_args"])
        row["calls"] = ">".join(row["calls"])
        writer.writerow(row)
        count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="capture_original.py native-trace.jsonl")
    parser.add_argument("-o", "--output", type=Path, help="CSV output (default: stdout)")
    args = parser.parse_args()
    try:
        with args.trace.open(encoding="utf-8") as source:
            if args.output:
                with args.output.open("w", encoding="utf-8", newline="") as destination:
                    count = write_csv(source, destination)
            else:
                count = write_csv(source, sys.stdout)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if args.output:
        print(f"wrote {count} GameScene tick rows to {args.output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
