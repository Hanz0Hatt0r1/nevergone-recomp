#!/usr/bin/env python3
"""Validate retained Android evidence for the first reconstructed GameScene render."""

from __future__ import annotations

import argparse
import hashlib
import re
import struct
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
TIMESTAMP_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
REQUIRED_KEYS = {
    "status",
    "timestamp_utc",
    "android_release",
    "android_sdk",
    "manufacturer",
    "model",
    "device",
    "primary_abi",
    "page_size",
    "diagnostics_file",
    "diagnostics_sha256",
    "screenshot_file",
    "screenshot_sha256",
}


@dataclass(frozen=True)
class Evidence:
    values: dict[str, str]
    report_path: Path


@dataclass(frozen=True)
class SceneProof:
    render_sprites: int
    deferred_pairs: int
    unresolved_render_objects: int
    staging_requests: int
    drawn_frames: int
    drawn_sprites: int


def parse_report(path: Path) -> Evidence:
    values: dict[str, str] = {}
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ValueError(f"cannot read evidence report {path}: {error}") from error

    for line_number, line in enumerate(lines, start=1):
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise ValueError(f"line {line_number} is not key=value")
        key, value = line.split("=", 1)
        if not key or key in values:
            raise ValueError(f"invalid or duplicate key on line {line_number}: {key!r}")
        values[key] = value

    missing = sorted(REQUIRED_KEYS - values.keys())
    if missing:
        raise ValueError(f"missing required evidence keys: {', '.join(missing)}")
    return Evidence(values=values, report_path=path)


def _basename_file(evidence: Evidence, key: str) -> Path:
    name = evidence.values[key]
    if Path(name).name != name:
        raise ValueError(f"{key} must be a basename")
    return evidence.report_path.parent / name


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as error:
        raise ValueError(f"cannot read evidence file {path}: {error}") from error
    return digest.hexdigest()


def _single_match(pattern: str, text: str, label: str, flags: int = 0) -> re.Match[str]:
    matches = list(re.finditer(pattern, text, flags))
    if len(matches) != 1:
        raise ValueError(f"expected exactly one {label}, found {len(matches)}")
    return matches[0]


def _single_int(pattern: str, text: str, label: str) -> int:
    return int(_single_match(pattern, text, label, re.MULTILINE).group(1))


def extract_diagnostics_from_ui(xml_path: Path) -> str:
    try:
        root = ET.parse(xml_path).getroot()
    except (ET.ParseError, OSError) as error:
        raise ValueError(f"invalid uiautomator XML {xml_path}: {error}") from error

    candidates: list[str] = []
    for node in root.iter("node"):
        text = node.attrib.get("text", "")
        if "GameScene static renderer" in text and "GameScene asset staging" in text:
            candidates.append(text)
    if len(candidates) != 1:
        raise ValueError(f"expected exactly one GameScene diagnostics node, found {len(candidates)}")
    return candidates[0]


def _png_dimensions(path: Path) -> tuple[int, int]:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise ValueError(f"cannot read screenshot {path}: {error}") from error
    if len(data) < 24 or data[:8] != PNG_SIGNATURE or data[12:16] != b"IHDR":
        raise ValueError("screenshot is not a PNG with an IHDR chunk")
    width, height = struct.unpack(">II", data[16:24])
    if width <= 0 or height <= 0:
        raise ValueError("screenshot has invalid dimensions")
    return width, height


def validate_diagnostics(text: str) -> SceneProof:
    if text.count("Original APK assets: present") != 1:
        raise ValueError("original APK assets are not reported present exactly once")
    if text.count("Original OBB assets: present") != 1:
        raise ValueError("original OBB assets are not reported present exactly once")

    _single_match(
        r"^GameLevels enter boundary\nstate: runtime-render-queue-ready$",
        text,
        "runtime-render-queue-ready GameLevels boundary",
        re.MULTILINE,
    )
    _single_match(
        r"^GameLevels runtime model\nstate: ready$",
        text,
        "ready GameLevels runtime model",
        re.MULTILINE,
    )
    _single_match(r"^scene instance: ready$", text, "ready scene instance", re.MULTILINE)
    _single_match(
        r"^scene construction plan: ready$",
        text,
        "ready scene construction plan",
        re.MULTILINE,
    )
    _single_match(r"^scene render queue: ready$", text, "ready scene render queue", re.MULTILINE)

    render_sprites = _single_int(r"^render sprites:\s*(\d+)\s*$", text, "render sprite count")
    deferred_pairs = _single_int(
        r"^deferred scene-action pairs:\s*(\d+)\s*$",
        text,
        "deferred scene-action pair count",
    )
    unresolved_render_objects = _single_int(
        r"^unresolved render objects:\s*(\d+)\s*$",
        text,
        "unresolved render object count",
    )
    if render_sprites <= 0:
        raise ValueError("render queue contains no sprites")

    staging = _single_match(
        r"^GameScene asset staging\n"
        r"state:\s*([^\n]+)\n"
        r"attempts:\s*(\d+)\n"
        r"successes:\s*(\d+)\n"
        r"revision:\s*(\d+)\n"
        r"requests:\s*(\d+)\n"
        r"staged:\s*(\d+)\s*$",
        text,
        "GameScene asset staging report",
        re.MULTILINE,
    )
    staging_state = staging.group(1)
    attempts = int(staging.group(2))
    successes = int(staging.group(3))
    revision = int(staging.group(4))
    requests = int(staging.group(5))
    staged = int(staging.group(6))
    if staging_state != "ready":
        raise ValueError(f"GameScene staging is not ready: {staging_state!r}")
    if attempts < 1 or successes < 1 or revision <= 0 or requests <= 0:
        raise ValueError("GameScene staging counters do not prove a completed live revision")
    if staged != requests:
        raise ValueError(f"GameScene staging is incomplete: staged={staged}, requests={requests}")

    renderer = _single_match(
        r"^GameScene static renderer\n"
        r"shader:\s*([^\n]+)\n"
        r"surface:\s*(\d+)x(\d+)\n"
        r"draw attempts:\s*(\d+)\n"
        r"drawn frames:\s*(\d+)\n"
        r"last static sprites:\s*(\d+)\s*$",
        text,
        "GameScene static renderer report",
        re.MULTILINE,
    )
    shader = renderer.group(1)
    surface_width = int(renderer.group(2))
    surface_height = int(renderer.group(3))
    draw_attempts = int(renderer.group(4))
    drawn_frames = int(renderer.group(5))
    drawn_sprites = int(renderer.group(6))
    if shader != "ready":
        raise ValueError(f"GameScene shader is not ready: {shader!r}")
    if surface_width <= 0 or surface_height <= 0:
        raise ValueError("GameScene renderer has no live surface")
    if draw_attempts < 1 or drawn_frames < 1 or drawn_frames > draw_attempts:
        raise ValueError("GameScene renderer counters do not prove a successful draw")
    if drawn_sprites <= 0:
        raise ValueError("GameScene renderer drew no static sprites")
    if drawn_sprites != requests:
        raise ValueError(
            "drawn static sprite count does not match the staged request count: "
            f"drawn={drawn_sprites}, requests={requests}"
        )

    # render_sprites is the base GameLevels queue. The live renderer composes
    # player/joystick overlays before it creates staging requests, so the
    # authoritative post-composition invariant is drawn_sprites == requests;
    # it is valid for that count to exceed the base queue.
    return SceneProof(
        render_sprites=render_sprites,
        deferred_pairs=deferred_pairs,
        unresolved_render_objects=unresolved_render_objects,
        staging_requests=requests,
        drawn_frames=drawn_frames,
        drawn_sprites=drawn_sprites,
    )


def validate(evidence: Evidence, require_pass: bool = True) -> SceneProof | None:
    values = evidence.values
    status = values["status"]
    if status not in {"pass", "fail"}:
        raise ValueError(f"invalid status: {status!r}")
    if require_pass and status != "pass":
        raise ValueError(f"evidence status is not pass: {status!r}")
    if not TIMESTAMP_RE.fullmatch(values["timestamp_utc"]):
        raise ValueError(f"invalid UTC timestamp: {values['timestamp_utc']!r}")
    try:
        sdk = int(values["android_sdk"])
    except ValueError as error:
        raise ValueError(f"invalid Android SDK: {values['android_sdk']!r}") from error
    if sdk < 23 or sdk > 99:
        raise ValueError(f"unsupported Android SDK: {sdk}")
    try:
        page_size = int(values["page_size"])
    except ValueError as error:
        raise ValueError(f"invalid page size: {values['page_size']!r}") from error
    if page_size <= 0 or page_size & (page_size - 1):
        raise ValueError(f"page size is not a positive power of two: {page_size}")

    diagnostics = _basename_file(evidence, "diagnostics_file")
    screenshot = _basename_file(evidence, "screenshot_file")
    for key, path in (
        ("diagnostics_sha256", diagnostics),
        ("screenshot_sha256", screenshot),
    ):
        expected = values[key]
        if not SHA256_RE.fullmatch(expected):
            raise ValueError(f"{key} is not a lowercase SHA-256 digest")
        actual = _sha256(path)
        if actual != expected:
            raise ValueError(f"{key} does not match {path.name}")

    _png_dimensions(screenshot)
    text = diagnostics.read_text(encoding="utf-8")
    if not text.strip():
        raise ValueError("runtime diagnostics evidence is empty")
    if status != "pass":
        return None
    return validate_diagnostics(text)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    extract_parser = subparsers.add_parser("extract-ui")
    extract_parser.add_argument("xml", type=Path)
    extract_parser.add_argument("output", type=Path)

    validate_parser = subparsers.add_parser("validate")
    validate_parser.add_argument("report", type=Path)
    validate_parser.add_argument("--allow-failure", action="store_true")

    args = parser.parse_args()
    try:
        if args.command == "extract-ui":
            text = extract_diagnostics_from_ui(args.xml)
            args.output.write_text(text + ("" if text.endswith("\n") else "\n"), encoding="utf-8")
            print(f"GameScene diagnostics extracted: {args.output}")
            return 0

        evidence = parse_report(args.report)
        proof = validate(evidence, require_pass=not args.allow_failure)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    if proof is None:
        print(f"GameScene failure evidence valid: {args.report}")
    else:
        print(
            "GameScene runtime evidence valid: "
            f"drawn_frames={proof.drawn_frames} "
            f"drawn_sprites={proof.drawn_sprites} "
            f"unresolved_render_objects={proof.unresolved_render_objects} "
            f"deferred_pairs={proof.deferred_pairs}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
