#!/usr/bin/env python3
"""Validate and drive retained Android evidence for the recovered GameScene joystick path."""

from __future__ import annotations

import argparse
import math
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

from android_game_scene_runtime_evidence import validate_diagnostics

DESIGN_WIDTH = 1136.0
DESIGN_HEIGHT = 640.0
JOYSTICK_CENTER_X = 204.0
JOYSTICK_CENTER_Y = 173.0
JOYSTICK_TEST_DELTA_X = 100.0
PACKAGE_NAME = "org.nevergone.recomp"
BOUNDS_RE = re.compile(r"^\[(\d+),(\d+)\]\[(\d+),(\d+)\]$")


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


@dataclass(frozen=True)
class Swipe:
    start_x: int
    start_y: int
    end_x: int
    end_y: int


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


def choose_direction(text: str) -> str:
    snapshot = parse_snapshot(text)
    # Force a state transition rather than replaying the already-applied facing.
    return "left" if snapshot.control.primary_state == "0x13" else "right"


def _node_bounds(node: ET.Element) -> tuple[int, int, int, int] | None:
    match = BOUNDS_RE.fullmatch(node.attrib.get("bounds", ""))
    if match is None:
        return None
    left, top, right, bottom = map(int, match.groups())
    if right <= left or bottom <= top:
        return None
    return left, top, right, bottom


def _surface_bounds(xml_path: Path) -> tuple[int, int, int, int]:
    try:
        root = ET.parse(xml_path).getroot()
    except (ET.ParseError, OSError) as error:
        raise ValueError(f"invalid uiautomator XML {xml_path}: {error}") from error

    explicit: list[tuple[int, int, int, int]] = []
    for node in root.iter("node"):
        class_name = node.attrib.get("class", "")
        if class_name not in {
            "android.opengl.GLSurfaceView",
            "org.nevergone.recomp.GameSurfaceView",
        } and not class_name.endswith(".GameSurfaceView"):
            continue
        bounds = _node_bounds(node)
        if bounds is not None:
            explicit.append(bounds)
    if len(explicit) == 1:
        return explicit[0]
    if len(explicit) > 1:
        raise ValueError(f"expected exactly one GameSurfaceView bounds node, found {len(explicit)}")

    # On the tested Android 13/libcore stack UIAutomator flattens the custom
    # GLSurfaceView subclass to android.view.View. The actual render surface is
    # still uniquely identifiable as the app-owned, focusable, id-less View;
    # system/navigation Views carry resource ids and are not focusable.
    fallback: list[tuple[int, int, int, int]] = []
    for node in root.iter("node"):
        if node.attrib.get("class", "") != "android.view.View":
            continue
        if node.attrib.get("package", "") != PACKAGE_NAME:
            continue
        if node.attrib.get("resource-id", ""):
            continue
        if node.attrib.get("focusable", "") != "true":
            continue
        bounds = _node_bounds(node)
        if bounds is not None:
            fallback.append(bounds)
    if len(fallback) != 1:
        raise ValueError(
            "expected exactly one GameSurfaceView or app-owned focusable plain View bounds node, "
            f"found {len(fallback)}"
        )
    return fallback[0]


def surface_swipe(xml_path: Path, direction: str) -> Swipe:
    if direction not in {"left", "right"}:
        raise ValueError(f"unsupported swipe direction: {direction!r}")
    left, top, right, bottom = _surface_bounds(xml_path)
    width = float(right - left)
    height = float(bottom - top)

    target_x = JOYSTICK_CENTER_X + (JOYSTICK_TEST_DELTA_X if direction == "right" else -JOYSTICK_TEST_DELTA_X)

    def to_screen(design_x: float, design_y: float) -> tuple[int, int]:
        x = left + design_x * width / DESIGN_WIDTH
        y = top + (DESIGN_HEIGHT - design_y) * height / DESIGN_HEIGHT
        return round(x), round(y)

    start_x, start_y = to_screen(JOYSTICK_CENTER_X, JOYSTICK_CENTER_Y)
    end_x, end_y = to_screen(target_x, JOYSTICK_CENTER_Y)
    return Swipe(start_x=start_x, start_y=start_y, end_x=end_x, end_y=end_y)


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
    subparsers = parser.add_subparsers(dest="command", required=True)

    validate_parser = subparsers.add_parser("validate")
    validate_parser.add_argument("before", type=Path)
    validate_parser.add_argument("after", type=Path)

    direction_parser = subparsers.add_parser("choose-direction")
    direction_parser.add_argument("diagnostics", type=Path)

    swipe_parser = subparsers.add_parser("surface-swipe")
    swipe_parser.add_argument("xml", type=Path)
    swipe_parser.add_argument("direction", choices=("left", "right"))

    args = parser.parse_args()
    try:
        if args.command == "choose-direction":
            print(choose_direction(args.diagnostics.read_text(encoding="utf-8")))
            return 0
        if args.command == "surface-swipe":
            swipe = surface_swipe(args.xml, args.direction)
            print(swipe.start_x, swipe.start_y, swipe.end_x, swipe.end_y)
            return 0

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
