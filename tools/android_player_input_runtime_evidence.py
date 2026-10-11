#!/usr/bin/env python3
"""Validate before/after Android diagnostics for the recovered GameScene joystick path."""

from __future__ import annotations

import argparse
import math
import re
from dataclasses import dataclass
from pathlib import Path

from android_game_scene_runtime_evidence import validate_diagnostics


@dataclass(frozen=True)
class JoystickSnapshot:
    captured: bool
    pointer_id: int
    accepted_begins: int
    accepted_moves: int
    design_x: float
    design_y: float
    power: float
    angle_degrees: float


@dataclass(frozen=True)
class ControlSnapshot:
    generation: int
    primary_state: str
    flip_x: bool


@dataclass(frozen=True)
class PlayerInputSnapshot:
    joystick: JoystickSnapshot
    control: ControlSnapshot


def _single_match(pattern: str, text: str, label: str) -> re.Match[str]:
    matches = list(re.finditer(pattern, text, re.MULTILINE))
    if len(matches) != 1:
        raise ValueError(f"expected exactly one {label}, found {len(matches)}")
    return matches[0]


def _finite(value: str, label: str) -> float:
    try:
        parsed = float(value)
    except ValueError as error:
        raise ValueError(f"invalid {label}: {value!r}") from error
    if not math.isfinite(parsed):
        raise ValueError(f"non-finite {label}: {value!r}")
    return parsed


def parse_snapshot(text: str) -> PlayerInputSnapshot:
    joystick = _single_match(
        r"^GameScene player joystick\n"
        r"captured: (true|false)\n"
        r"pointer id: (-?\d+)\n"
        r"accepted begins: (\d+)\n"
        r"accepted moves: (\d+)\n"
        r"design point: ([^,\n]+),([^\n]+)\n"
        r"power: ([^\n]+)\n"
        r"angle degrees: ([^\n]+)$",
        text,
        "GameScene player joystick report",
    )
    control = _single_match(
        r"^Player control runtime\n"
        r"generation: (\d+)\n"
        r"primary state: (none|0x12|0x13)\n"
        r"flip x: (true|false)$",
        text,
        "player control runtime report",
    )

    joystick_snapshot = JoystickSnapshot(
        captured=joystick.group(1) == "true",
        pointer_id=int(joystick.group(2)),
        accepted_begins=int(joystick.group(3)),
        accepted_moves=int(joystick.group(4)),
        design_x=_finite(joystick.group(5), "joystick design x"),
        design_y=_finite(joystick.group(6), "joystick design y"),
        power=_finite(joystick.group(7), "joystick power"),
        angle_degrees=_finite(joystick.group(8), "joystick angle"),
    )
    control_snapshot = ControlSnapshot(
        generation=int(control.group(1)),
        primary_state=control.group(2),
        flip_x=control.group(3) == "true",
    )
    return PlayerInputSnapshot(joystick=joystick_snapshot, control=control_snapshot)


def validate_transition(before_text: str, after_text: str) -> tuple[PlayerInputSnapshot, PlayerInputSnapshot]:
    # Reuse the P7 proof gate so input evidence cannot be accepted from a menu,
    # parser-only state or failed renderer/staging snapshot.
    validate_diagnostics(before_text)
    validate_diagnostics(after_text)

    before = parse_snapshot(before_text)
    after = parse_snapshot(after_text)

    if before.joystick.captured or after.joystick.captured:
        raise ValueError("before/after evidence must be captured with the joystick released")
    if before.joystick.pointer_id != -1 or after.joystick.pointer_id != -1:
        raise ValueError("released joystick evidence must report pointer id -1")
    if after.joystick.accepted_begins <= before.joystick.accepted_begins:
        raise ValueError("no newly accepted joystick begin is proven")
    if after.joystick.accepted_moves <= before.joystick.accepted_moves:
        raise ValueError("no newly accepted joystick move is proven")
    if after.control.generation <= before.control.generation:
        raise ValueError("player control generation did not advance")
    if after.control.primary_state == "none":
        raise ValueError("post-gesture player control state is still neutral")
    if before.control.primary_state == after.control.primary_state:
        raise ValueError("post-gesture player control state did not change")

    expected_flip = after.control.primary_state == "0x12"
    if after.control.flip_x != expected_flip:
        raise ValueError(
            "post-gesture facing does not match the recovered state contract: "
            f"state={after.control.primary_state}, flip_x={after.control.flip_x}"
        )
    return before, after


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    args = parser.parse_args()

    try:
        before_text = args.before.read_text(encoding="utf-8")
        after_text = args.after.read_text(encoding="utf-8")
        before, after = validate_transition(before_text, after_text)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    print(
        "Player input runtime evidence valid: "
        f"begins={before.joystick.accepted_begins}->{after.joystick.accepted_begins} "
        f"moves={before.joystick.accepted_moves}->{after.joystick.accepted_moves} "
        f"control={before.control.primary_state}->{after.control.primary_state} "
        f"flip_x={str(after.control.flip_x).lower()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
