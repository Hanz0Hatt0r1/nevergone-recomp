#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile

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


def diagnostics(
    *,
    begins: int,
    moves: int,
    generation: int,
    state: str,
    flip: bool,
    motion_ready: bool = True,
    motion_generation: int = 20,
    spawn_generation: int = 3,
    player_index: int = 1,
    game_tick: int = 100,
    steps: int = 10,
    offset_x: float = 0.0,
    offset_y: float = 0.0,
) -> str:
    return SCENE + f"""GameScene player joystick
captured: false
pointer id: -1
accepted begins: {begins}
accepted moves: {moves}
design point: 204,173
power: 0
angle degrees: 0
GameScene player motion
ready: {'true' if motion_ready else 'false'}
generation: {motion_generation}
spawn generation: {spawn_generation}
player index: {player_index}
last game tick: {game_tick}
applied steps: {steps}
offset: {offset_x},{offset_y}
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


def expect_swipe_invalid(xml: Path) -> None:
    try:
        module.surface_swipe(xml, "right")
    except ValueError:
        return
    raise AssertionError("expected GameSurfaceView bounds lookup to fail closed")


def main() -> int:
    before = diagnostics(begins=4, moves=9, generation=7, state="none", flip=False)
    after = diagnostics(
        begins=5,
        moves=11,
        generation=8,
        state="0x13",
        flip=False,
        motion_generation=21,
        game_tick=105,
        steps=15,
        offset_x=40.0,
    )
    parsed_before, parsed_after = module.validate_transition(before, after)
    assert parsed_before.joystick.accepted_begins == 4
    assert parsed_after.joystick.accepted_moves == 11
    assert parsed_after.control.primary_state == "0x13"
    assert not parsed_after.control.flip_x
    assert parsed_after.motion.applied_steps == 15
    assert parsed_after.motion.offset_x == 40.0
    assert module.choose_direction(before) == "right"
    assert module.choose_direction(after) == "left"

    before_left = diagnostics(
        begins=5,
        moves=11,
        generation=8,
        state="0x13",
        flip=False,
        motion_generation=21,
        game_tick=105,
        steps=15,
        offset_x=40.0,
    )
    after_left = diagnostics(
        begins=6,
        moves=14,
        generation=9,
        state="0x12",
        flip=True,
        motion_generation=22,
        game_tick=110,
        steps=20,
        offset_x=0.0,
    )
    module.validate_transition(before_left, after_left)

    expect_invalid(before, diagnostics(
        begins=4, moves=11, generation=8, state="0x13", flip=False,
        motion_generation=21, game_tick=105, steps=15, offset_x=40.0))
    expect_invalid(before, diagnostics(
        begins=5, moves=9, generation=8, state="0x13", flip=False,
        motion_generation=21, game_tick=105, steps=15, offset_x=40.0))
    expect_invalid(before, diagnostics(
        begins=5, moves=11, generation=7, state="0x13", flip=False,
        motion_generation=21, game_tick=105, steps=15, offset_x=40.0))
    expect_invalid(before_left, diagnostics(
        begins=6, moves=14, generation=9, state="0x13", flip=False,
        motion_generation=22, game_tick=110, steps=20, offset_x=80.0))
    expect_invalid(before_left, diagnostics(
        begins=6, moves=14, generation=9, state="0x12", flip=False,
        motion_generation=22, game_tick=110, steps=20, offset_x=0.0))
    expect_invalid(before, after.replace("state: runtime-render-queue-ready", "state: runtime-model-ready"))
    expect_invalid(before, after.replace("captured: false", "captured: true", 1))

    # Motion proof is part of the same transition contract now.
    expect_invalid(before, diagnostics(
        begins=5, moves=11, generation=8, state="0x13", flip=False,
        motion_ready=False, motion_generation=21, game_tick=105, steps=15, offset_x=40.0))
    expect_invalid(before, diagnostics(
        begins=5, moves=11, generation=8, state="0x13", flip=False,
        motion_generation=21, spawn_generation=4, game_tick=105, steps=15, offset_x=40.0))
    expect_invalid(before, diagnostics(
        begins=5, moves=11, generation=8, state="0x13", flip=False,
        motion_generation=21, game_tick=105, steps=10, offset_x=40.0))
    expect_invalid(before, diagnostics(
        begins=5, moves=11, generation=8, state="0x13", flip=False,
        motion_generation=21, game_tick=105, steps=15, offset_x=0.0))
    expect_invalid(before, diagnostics(
        begins=5, moves=11, generation=8, state="0x13", flip=False,
        motion_generation=21, game_tick=105, steps=15, offset_x=-40.0))
    expect_invalid(before_left, diagnostics(
        begins=6, moves=14, generation=9, state="0x12", flip=True,
        motion_generation=22, game_tick=110, steps=20, offset_x=80.0))

    with tempfile.TemporaryDirectory() as temp_dir:
        xml = Path(temp_dir) / "window.xml"
        xml.write_text(
            '<?xml version="1.0" encoding="UTF-8"?>'
            '<hierarchy>'
            '<node class="org.nevergone.recomp.GameSurfaceView" bounds="[0,100][1136,740]" />'
            '</hierarchy>',
            encoding="utf-8",
        )
        right = module.surface_swipe(xml, "right")
        assert right.start_x == 204
        assert right.start_y == 567
        assert right.end_x == 304
        assert right.end_y == 567
        left = module.surface_swipe(xml, "left")
        assert left.end_x == 104
        assert left.end_y == 567

        # Real-device Android 13 UIAutomator evidence flattens the custom
        # GameSurfaceView to an app-owned plain android.view.View. The surface
        # is uniquely focusable and id-less; the navigation view is not.
        xml.write_text(
            '<?xml version="1.0" encoding="UTF-8"?>'
            '<hierarchy>'
            '<node class="android.view.View" package="org.nevergone.recomp" '
            'resource-id="" focusable="true" bounds="[76,0][2340,612]" />'
            '<node class="android.view.View" package="org.nevergone.recomp" '
            'resource-id="android:id/navigationBarBackground" focusable="false" '
            'bounds="[76,1021][2340,1080]" />'
            '</hierarchy>',
            encoding="utf-8",
        )
        right = module.surface_swipe(xml, "right")
        assert right.start_x == 483
        assert right.start_y == 447
        assert right.end_x == 682
        assert right.end_y == 447
        left = module.surface_swipe(xml, "left")
        assert left.end_x == 283
        assert left.end_y == 447

        xml.write_text(
            '<hierarchy>'
            '<node class="android.view.View" package="org.nevergone.recomp" '
            'resource-id="" focusable="true" bounds="[0,0][100,100]" />'
            '<node class="android.view.View" package="org.nevergone.recomp" '
            'resource-id="" focusable="true" bounds="[100,0][200,100]" />'
            '</hierarchy>',
            encoding="utf-8",
        )
        expect_swipe_invalid(xml)

        xml.write_text(
            '<hierarchy><node class="android.widget.TextView" bounds="[0,0][100,100]" /></hierarchy>',
            encoding="utf-8",
        )
        expect_swipe_invalid(xml)

    print("Android player input runtime evidence validator smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
