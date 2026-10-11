#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path
import sys

MODULE_PATH = Path(__file__).with_name("android_player_input_runtime_evidence.py")
spec = importlib.util.spec_from_file_location("android_player_input_runtime_evidence", MODULE_PATH)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)

SCENE = """Never Gone Recomp

Original APK assets: present
Original OBB assets: present

GameLevels enter boundary
state: runtime-render-queue-ready
GameLevels runtime model
state: ready
scene instance: ready
scene construction plan: ready
scene render queue: ready
render sprites: 12
deferred scene-action pairs: 1
unresolved render objects: 1
GameScene static renderer
shader: ready
surface: 1080x1920
draw attempts: 18
drawn frames: 16
last static sprites: 10
GameScene asset staging
state: ready
attempts: 1
successes: 1
revision: 123456
requests: 10
staged: 10
"""


def diagnostics(*, begins: int, moves: int, generation: int, state: str, flip: bool) -> str:
    return SCENE + f"""GameScene player joystick
captured: false
pointer id: -1
accepted begins: {begins}
accepted moves: {moves}
design point: 204,173
power: 0
angle degrees: 0
Player control runtime
generation: {generation}
primary state: {state}
flip x: {'true' if flip else 'false'}
"""


def expect_invalid(before: str, after: str) -> None:
    try:
        module.validate_transition(before, after)
    except ValueError:
        return
    raise AssertionError("expected invalid player input evidence")


def main() -> int:
    before = diagnostics(begins=4, moves=9, generation=7, state="none", flip=False)
    after = diagnostics(begins=5, moves=11, generation=8, state="0x13", flip=False)
    parsed_before, parsed_after = module.validate_transition(before, after)
    assert parsed_before.joystick.accepted_begins == 4
    assert parsed_after.joystick.accepted_moves == 11
    assert parsed_after.control.primary_state == "0x13"
    assert not parsed_after.control.flip_x

    before_left = diagnostics(begins=5, moves=11, generation=8, state="0x13", flip=False)
    after_left = diagnostics(begins=6, moves=14, generation=9, state="0x12", flip=True)
    module.validate_transition(before_left, after_left)

    expect_invalid(before, diagnostics(begins=4, moves=11, generation=8, state="0x13", flip=False))
    expect_invalid(before, diagnostics(begins=5, moves=9, generation=8, state="0x13", flip=False))
    expect_invalid(before, diagnostics(begins=5, moves=11, generation=7, state="0x13", flip=False))
    expect_invalid(before_left, diagnostics(begins=6, moves=14, generation=9, state="0x13", flip=False))
    expect_invalid(before_left, diagnostics(begins=6, moves=14, generation=9, state="0x12", flip=False))
    expect_invalid(before, after.replace("state: runtime-render-queue-ready", "state: runtime-model-ready"))
    expect_invalid(before, after.replace("captured: false", "captured: true", 1))

    print("Android player input runtime evidence validator smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
