#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile

MODULE_PATH = Path(__file__).with_name("android_game_scene_runtime_evidence.py")
spec = importlib.util.spec_from_file_location("android_game_scene_runtime_evidence", MODULE_PATH)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)

DIAGNOSTICS = """Never Gone Recomp

Original APK assets: present
Original OBB assets: present

GameLevels enter boundary
state: runtime-render-queue-ready
enter callbacks: 1
probe attempts: 1
GameLevels runtime model
state: ready
load attempts: 1
scene instance: ready
scene layers: 3
scene objects: 14
scene construction plan: ready
construction layers: 3
construction objects: 14
ignored source layers: 0
unresolved construction type kinds: 1
unresolved type histogram: 9:2
scene render queue: ready
render sprites: 12
frame-cache sprites: 7
direct-file sprites: 3
deferred scene-action pairs: 1
unresolved render objects: 1
GameScene type-0 renderer
shader: ready
surface: 1080x1920
camera eye x: 64.5
draw attempts: 18
drawn frames: 16
last sprites: 10
post-init animated sprites: 2
last minimum opacity: 240
GameScene asset staging
state: ready
attempts: 1
successes: 1
revision: 123456
requests: 10
staged: 10
"""


def make_png(path: Path, width: int = 1080, height: int = 1920) -> None:
    # The validator only needs the PNG signature and canonical IHDR dimensions.
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + struct.pack(">I", 13)
        + b"IHDR"
        + struct.pack(">II", width, height)
        + b"\x08\x06\x00\x00\x00"
    )


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_report(root: Path, *, status: str = "pass", diagnostics: str = DIAGNOSTICS) -> Path:
    diagnostics_path = root / "runtime-diagnostics.txt"
    screenshot_path = root / "scene.png"
    diagnostics_path.write_text(diagnostics, encoding="utf-8")
    make_png(screenshot_path)
    values = {
        "status": status,
        "timestamp_utc": "2026-10-11T12:00:00Z",
        "android_release": "16",
        "android_sdk": "36",
        "manufacturer": "Example",
        "model": "Device",
        "device": "device_name",
        "primary_abi": "arm64-v8a",
        "page_size": "16384",
        "diagnostics_file": diagnostics_path.name,
        "diagnostics_sha256": sha256(diagnostics_path),
        "screenshot_file": screenshot_path.name,
        "screenshot_sha256": sha256(screenshot_path),
    }
    report = root / "runtime-report.txt"
    report.write_text("".join(f"{key}={value}\n" for key, value in values.items()), encoding="utf-8")
    return report


def expect_invalid(report: Path, *, require_pass: bool = True) -> None:
    try:
        module.validate(module.parse_report(report), require_pass=require_pass)
    except ValueError:
        return
    raise AssertionError(f"expected invalid evidence: {report}")


def replace(root: Path, old: str, new: str) -> Path:
    return write_report(root, diagnostics=DIAGNOSTICS.replace(old, new))


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        report = write_report(root)
        proof = module.validate(module.parse_report(report))
        assert proof is not None
        assert proof.render_sprites == 12
        assert proof.staging_requests == 10
        assert proof.hidden_slots == 0
        assert proof.camera_eye_x == 64.5
        assert proof.drawn_frames == 16
        assert proof.drawn_sprites == 10
        assert proof.unresolved_render_objects == 1
        assert proof.deferred_pairs == 1

        # Tutorial bootstrap also reports this count after the scene has drawn.
        # Only the count beside the live render queue belongs to this proof.
        report = write_report(
            root,
            diagnostics=DIAGNOSTICS + "Tutorial scene bootstrap\nrender sprites: 12\n",
        )
        assert module.validate(module.parse_report(report)) is not None

        # The runtime model exposes the base GameLevels queue while the direct
        # renderer stages a live queue after player/joystick composition. A
        # composed overlay may therefore make requests/drawn exceed the base.
        report = replace(root, "render sprites: 12", "render sprites: 9")
        proof = module.validate(module.parse_report(report))
        assert proof is not None
        assert proof.render_sprites == 9
        assert proof.staging_requests == 10
        assert proof.drawn_sprites == 10
        assert proof.hidden_slots == 0

        # #358 introduced transactional hidden slots for the recovered ordinary
        # GameScene null-frame fallback. They are fully staged and validated by
        # the texture cache but intentionally submit no GLES quad.
        report = replace(root, "last sprites: 10", "last sprites: 8")
        proof = module.validate(module.parse_report(report))
        assert proof is not None
        assert proof.staging_requests == 10
        assert proof.drawn_sprites == 8
        assert proof.hidden_slots == 2

        xml = root / "window.xml"
        xml.write_text(
            '<?xml version="1.0" encoding="UTF-8"?>'
            '<hierarchy><node text="Refresh runtime diagnostics" />'
            '<node text="' + DIAGNOSTICS.replace("&", "&amp;").replace("\n", "&#10;") + '" />'
            '</hierarchy>',
            encoding="utf-8",
        )
        assert module.extract_diagnostics_from_ui(xml).startswith("Never Gone Recomp")

        # Preserve compatibility with retained evidence captured before the
        # runtime report was renamed from static renderer/last static sprites.
        legacy = DIAGNOSTICS.replace("GameScene type-0 renderer", "GameScene static renderer")
        legacy = legacy.replace("last sprites: 10", "last static sprites: 10")
        report = write_report(root, diagnostics=legacy)
        proof = module.validate(module.parse_report(report))
        assert proof is not None and proof.drawn_sprites == 10
        xml.write_text(
            '<?xml version="1.0" encoding="UTF-8"?>'
            '<hierarchy><node text="' + legacy.replace("&", "&amp;").replace("\n", "&#10;") + '" /></hierarchy>',
            encoding="utf-8",
        )
        assert module.extract_diagnostics_from_ui(xml).startswith("Never Gone Recomp")

        expect_invalid(replace(root, "Original OBB assets: present", "Original OBB assets: not imported"))
        expect_invalid(replace(root, "state: runtime-render-queue-ready", "state: runtime-model-ready"))
        expect_invalid(replace(root, "scene render queue: ready", "scene render queue: missing"))
        expect_invalid(replace(root, "state: ready\nattempts: 1", "state: staging\nattempts: 1"))
        expect_invalid(replace(root, "staged: 10", "staged: 9"))
        expect_invalid(replace(root, "camera eye x: 64.5", "camera eye x: nan"))
        expect_invalid(replace(root, "camera eye x: 64.5\n", ""))
        expect_invalid(replace(root, "drawn frames: 16", "drawn frames: 0"))
        expect_invalid(replace(root, "last sprites: 10", "last sprites: 0"))
        expect_invalid(replace(root, "last sprites: 10", "last sprites: 11"))
        expect_invalid(replace(root, "render sprites: 12", "render sprites: 0"))

        report = write_report(root, status="fail", diagnostics="capture failed\n")
        module.validate(module.parse_report(report), require_pass=False)
        expect_invalid(report)

        report = write_report(root)
        screenshot = root / "scene.png"
        screenshot.write_bytes(b"not a png")
        values = report.read_text(encoding="utf-8")
        report.write_text(
            values.replace(
                next(line for line in values.splitlines() if line.startswith("screenshot_sha256=")),
                "screenshot_sha256=" + sha256(screenshot),
            ),
            encoding="utf-8",
        )
        expect_invalid(report)

        report = write_report(root)
        diagnostics_path = root / "runtime-diagnostics.txt"
        diagnostics_path.write_text(DIAGNOSTICS + "tampered\n", encoding="utf-8")
        expect_invalid(report)

        report = write_report(root)
        text = report.read_text(encoding="utf-8")
        report.write_text(
            text.replace(
                "diagnostics_file=runtime-diagnostics.txt",
                "diagnostics_file=../runtime-diagnostics.txt",
            ),
            encoding="utf-8",
        )
        expect_invalid(report)

    print("Android GameScene runtime evidence validator smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
