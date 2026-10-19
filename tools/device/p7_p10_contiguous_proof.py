#!/usr/bin/env python3
"""Capture one contiguous real-device P7-P10 Never Gone Recomp proof run.

This harness intentionally reuses the repository's existing strict P7 renderer and
P8/P9 player-input evidence collectors. It adds the missing process-restart P10
proof and validates the project-owned minimal-save-v1 sidecar against retained
runtime diagnostics before and after the restart.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIR))

from android_game_scene_runtime_evidence import validate_diagnostics  # noqa: E402
from android_player_input_runtime_evidence import parse_snapshot  # noqa: E402

DEFAULT_PACKAGE = "org.nevergone.recomp"
DEFAULT_COMPONENT = "org.nevergone.recomp/.MainActivity"
DEFAULT_SIDECAR = "files/nevergone-recomp/minimal-save-v1.bin"
MAGIC = b"NGSV"
FORMAT_VERSION = 1
HEADER_BYTES = 16
FIXED_PAYLOAD_BYTES = 20
MAX_GUID_BYTES = 256


class ProofError(RuntimeError):
    pass


@dataclass(frozen=True)
class SidecarRecord:
    scene_index: int
    player_index: int
    offset_x: float
    offset_y: float
    scene_guid: str


def fnv1a32(data: bytes) -> int:
    value = 2166136261
    for byte in data:
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def decode_sidecar(data: bytes) -> SidecarRecord:
    if len(data) < HEADER_BYTES + FIXED_PAYLOAD_BYTES:
        raise ProofError(f"sidecar is too small: {len(data)} bytes")
    if len(data) > HEADER_BYTES + FIXED_PAYLOAD_BYTES + MAX_GUID_BYTES:
        raise ProofError(f"sidecar is too large: {len(data)} bytes")
    if data[:4] != MAGIC:
        raise ProofError("sidecar magic is not NGSV")
    version, payload_size, expected_checksum = struct.unpack_from("<III", data, 4)
    if version != FORMAT_VERSION:
        raise ProofError(f"unsupported sidecar version: {version}")
    payload = data[HEADER_BYTES:]
    if payload_size != len(payload):
        raise ProofError(f"sidecar payload length mismatch: header={payload_size}, actual={len(payload)}")
    actual_checksum = fnv1a32(payload)
    if actual_checksum != expected_checksum:
        raise ProofError(
            f"sidecar checksum mismatch: expected=0x{expected_checksum:08x}, actual=0x{actual_checksum:08x}"
        )
    scene_index, player_index, offset_x, offset_y, guid_size = struct.unpack_from("<IIffI", payload, 0)
    if player_index not in (1, 2, 3):
        raise ProofError(f"invalid sidecar player index: {player_index}")
    if guid_size < 1 or guid_size > MAX_GUID_BYTES:
        raise ProofError(f"invalid sidecar scene GUID size: {guid_size}")
    if guid_size != len(payload) - FIXED_PAYLOAD_BYTES:
        raise ProofError("sidecar GUID length does not consume the payload exactly")
    try:
        scene_guid = payload[FIXED_PAYLOAD_BYTES:].decode("utf-8")
    except UnicodeDecodeError as error:
        raise ProofError(f"sidecar scene GUID is not UTF-8: {error}") from error
    if any(ch in scene_guid for ch in ("\x00", "\n", "\r")):
        raise ProofError("sidecar scene GUID contains a forbidden control character")
    for label, value in (("offset_x", offset_x), ("offset_y", offset_y)):
        if not (-1_000_000.0 <= value <= 1_000_000.0):
            raise ProofError(f"sidecar {label} is outside the bounded proof range: {value}")
    return SidecarRecord(scene_index, player_index, offset_x, offset_y, scene_guid)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_scene_identity(text: str) -> tuple[int, str]:
    indexes = re.findall(r"^current scene index:\s*(\d+)\s*$", text, re.MULTILINE)
    guids = re.findall(r"^current scene guid:\s*([^\n]+?)\s*$", text, re.MULTILINE)
    if len(indexes) != 1 or len(guids) != 1:
        raise ProofError(
            f"expected exactly one current scene index/GUID in diagnostics, got {len(indexes)}/{len(guids)}"
        )
    return int(indexes[0]), guids[0]


def run(command: list[str], *, env: dict[str, str] | None = None, timeout: int = 120,
        capture: bool = True, check: bool = True) -> subprocess.CompletedProcess:
    result = subprocess.run(
        command,
        cwd=Path(__file__).resolve().parents[2],
        env=env,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
        timeout=timeout,
    )
    if check and result.returncode != 0:
        stdout = (result.stdout or b"").decode(errors="replace")
        stderr = (result.stderr or b"").decode(errors="replace")
        raise ProofError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"--- stdout ---\n{stdout[-8000:]}\n--- stderr ---\n{stderr[-8000:]}"
        )
    return result


def adb_args(serial: str, *args: str) -> list[str]:
    return ["adb", "-s", serial, *args]


def connected_device(explicit: str | None) -> str:
    if explicit:
        run(adb_args(explicit, "get-state"), timeout=10)
        return explicit
    result = run(["adb", "devices"], timeout=10)
    lines = result.stdout.decode(errors="replace").splitlines()[1:]
    devices = [line.split()[0] for line in lines if len(line.split()) >= 2 and line.split()[1] == "device"]
    if len(devices) != 1:
        raise ProofError(f"expected exactly one adb device, found {devices}")
    return devices[0]


def require_foreground(serial: str, package: str) -> None:
    pid = run(adb_args(serial, "shell", "pidof", package), timeout=10).stdout.decode().strip()
    if not pid:
        raise ProofError(f"{package} has no live process")
    state = run(adb_args(serial, "shell", "dumpsys", "activity", "activities"), timeout=20).stdout.decode(errors="replace")
    if not re.search(rf"(?:mResumedActivity|topResumedActivity).*{re.escape(package)}", state):
        raise ProofError(f"{package} is not the resumed Activity; reach the live first GameLevels scene first")


def read_private_file(serial: str, package: str, relative_path: str) -> bytes:
    result = run(
        adb_args(serial, "exec-out", "run-as", package, "cat", relative_path),
        timeout=15,
        check=False,
    )
    if result.returncode != 0 or not result.stdout:
        stderr = result.stderr.decode(errors="replace")
        raise ProofError(f"cannot read app-private {relative_path}: {stderr.strip() or 'empty output'}")
    return result.stdout


def wait_for_pid(serial: str, package: str, *, different_from: str | None = None, seconds: int = 20) -> str:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        result = run(adb_args(serial, "shell", "pidof", package), timeout=5, check=False)
        pid = result.stdout.decode(errors="replace").strip()
        if pid and (different_from is None or pid != different_from):
            return pid
        time.sleep(0.5)
    raise ProofError(f"{package} did not obtain the expected process within {seconds}s")


def resume_first_scene(serial: str) -> None:
    """Replay the known fresh-role onboarding after a cold process restart."""
    size = run(adb_args(serial, "shell", "wm", "size"), timeout=10).stdout.decode()
    match = re.search(r"(?:Physical|Override) size:\s*(\d+)x(\d+)", size)
    if not match:
        raise ProofError(f"cannot read Android display size: {size.strip()}")
    width, height = map(int, match.groups())
    if width < height:
        width, height = height, width
    scale_x, scale_y = width / 2340.0, height / 1080.0

    ui_dump = run(adb_args(
        serial, "shell", "uiautomator", "dump", "/sdcard/nevergone-resume-ui.xml"),
        timeout=20, check=False)
    if ui_dump.returncode == 0:
        xml = run(adb_args(
            serial, "exec-out", "cat", "/sdcard/nevergone-resume-ui.xml"),
            timeout=10, check=False).stdout.decode(errors="replace")
        hidden_tools = re.search(
            r'<node\b(?=[^>]*text="HIDE DEVELOPER TOOLS")(?=[^>]*bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]")[^>]*>',
            xml,
        )
        if hidden_tools:
            x1, y1, x2, y2 = map(int, hidden_tools.groups())
            run(adb_args(
                serial, "shell", "input", "tap",
                str((x1 + x2) // 2), str((y1 + y2) // 2)), timeout=5)
            time.sleep(1.0)

    # Coordinates are the tested Redmi Note 8 Pro fresh-role path. Scale them
    # to the connected device's physical display dimensions.
    taps = ((1200, 500, 4.0), (1580, 800, 2.0), (2210, 215, 7.0),
            (1790, 840, 4.0), (800, 650, 2.0))
    for x, y, settle in taps:
        run(adb_args(
            serial, "shell", "input", "tap",
            str(round(x * scale_x)), str(round(y * scale_y))), timeout=5)
        time.sleep(settle)


def run_evidence_script(serial: str, script: str, evidence_dir: Path) -> None:
    env = os.environ.copy()
    env["ANDROID_SERIAL"] = serial
    env["NEVERGONE_EVIDENCE_DIR"] = str(evidence_dir)
    result = run(["bash", script], env=env, timeout=180, capture=True, check=False)
    (evidence_dir / "collector-stdout.txt").write_bytes(result.stdout or b"")
    (evidence_dir / "collector-stderr.txt").write_bytes(result.stderr or b"")
    if result.returncode != 0:
        raise ProofError(
            f"{script} failed with status {result.returncode}; see {evidence_dir}/collector-*.txt"
        )


def near(a: float, b: float, tolerance: float = 1e-3) -> bool:
    return abs(a - b) <= tolerance


def validate_restart(pre_text: str, post_text: str, sidecar: SidecarRecord) -> dict[str, object]:
    validate_diagnostics(pre_text)
    validate_diagnostics(post_text)
    pre = parse_snapshot(pre_text)
    post = parse_snapshot(post_text)
    pre_scene_index, pre_scene_guid = parse_scene_identity(pre_text)
    post_scene_index, post_scene_guid = parse_scene_identity(post_text)

    if not pre.motion.ready or not post.motion.ready:
        raise ProofError("player motion runtime is not ready before/after restart")
    if pre.motion.player_index != sidecar.player_index or post.motion.player_index != sidecar.player_index:
        raise ProofError(
            "sidecar player index does not match runtime snapshots: "
            f"sidecar={sidecar.player_index}, pre={pre.motion.player_index}, post={post.motion.player_index}"
        )
    if pre_scene_index != sidecar.scene_index or post_scene_index != sidecar.scene_index:
        raise ProofError(
            "sidecar scene index does not match runtime snapshots: "
            f"sidecar={sidecar.scene_index}, pre={pre_scene_index}, post={post_scene_index}"
        )
    if pre_scene_guid != sidecar.scene_guid or post_scene_guid != sidecar.scene_guid:
        raise ProofError("sidecar scene GUID does not match the pre/post restart runtime scene")
    if not near(pre.motion.offset_x, sidecar.offset_x) or not near(pre.motion.offset_y, sidecar.offset_y):
        raise ProofError(
            "sidecar motion does not match the retained post-input motion: "
            f"sidecar=({sidecar.offset_x},{sidecar.offset_y}) "
            f"pre=({pre.motion.offset_x},{pre.motion.offset_y})"
        )
    if not near(post.motion.offset_x, sidecar.offset_x) or not near(post.motion.offset_y, sidecar.offset_y):
        raise ProofError(
            "restart did not restore the retained motion offset: "
            f"sidecar=({sidecar.offset_x},{sidecar.offset_y}) "
            f"post=({post.motion.offset_x},{post.motion.offset_y})"
        )
    if abs(sidecar.offset_x) <= 1e-6 and abs(sidecar.offset_y) <= 1e-6:
        raise ProofError("sidecar proves only a zero movement offset; P10 requires visible moved state")

    return {
        "scene_index": sidecar.scene_index,
        "scene_guid": sidecar.scene_guid,
        "player_index": sidecar.player_index,
        "offset_x": sidecar.offset_x,
        "offset_y": sidecar.offset_y,
        "pre_restart_motion_generation": pre.motion.generation,
        "post_restart_motion_generation": post.motion.generation,
        "pre_restart_spawn_generation": pre.motion.spawn_generation,
        "post_restart_spawn_generation": post.motion.spawn_generation,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device-id")
    parser.add_argument("--package", default=DEFAULT_PACKAGE)
    parser.add_argument("--component", default=DEFAULT_COMPONENT)
    parser.add_argument("--sidecar", default=DEFAULT_SIDECAR)
    parser.add_argument("--restart-settle-seconds", type=float, default=4.0)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()

    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out = args.out or Path("evidence") / f"android-p7-p10-{stamp}"
    out.mkdir(parents=True, exist_ok=True)
    summary: dict[str, object] = {
        "status": "fail",
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "package": args.package,
        "component": args.component,
    }

    try:
        serial = connected_device(args.device_id)
        summary["device_id"] = serial
        require_foreground(serial, args.package)
        initial_pid = wait_for_pid(serial, args.package)
        summary["initial_pid"] = initial_pid

        p7_dir = out / "p7-render"
        p7_dir.mkdir(parents=True, exist_ok=True)
        run_evidence_script(serial, "tools/android_game_scene_render_evidence.sh", p7_dir)

        p89_dir = out / "p8-p9-player-input"
        p89_dir.mkdir(parents=True, exist_ok=True)
        run_evidence_script(serial, "tools/android_player_input_runtime_evidence.sh", p89_dir)
        pre_restart_path = p89_dir / "after-diagnostics.txt"
        if not pre_restart_path.is_file():
            raise ProofError(f"missing retained P8/P9 diagnostics: {pre_restart_path}")
        pre_restart_text = pre_restart_path.read_text(encoding="utf-8")

        sidecar_before = read_private_file(serial, args.package, args.sidecar)
        sidecar_record = decode_sidecar(sidecar_before)
        (out / "p10-sidecar-before.bin").write_bytes(sidecar_before)
        summary["sidecar_before_sha256"] = sha256_bytes(sidecar_before)
        summary["sidecar_record"] = asdict(sidecar_record)

        run(adb_args(serial, "shell", "am", "force-stop", args.package), timeout=15)
        time.sleep(0.5)
        stopped_pid = run(adb_args(serial, "shell", "pidof", args.package), timeout=5, check=False).stdout.decode().strip()
        if stopped_pid:
            raise ProofError(f"force-stop left a live process: {stopped_pid}")

        run(adb_args(serial, "shell", "am", "start", "-W", "-n", args.component), timeout=30)
        restarted_pid = wait_for_pid(serial, args.package, different_from=initial_pid)
        summary["restarted_pid"] = restarted_pid
        time.sleep(max(0.0, args.restart_settle_seconds))
        require_foreground(serial, args.package)
        resume_first_scene(serial)
        require_foreground(serial, args.package)

        p10_dir = out / "p10-restart-restore"
        p10_dir.mkdir(parents=True, exist_ok=True)
        run_evidence_script(serial, "tools/android_game_scene_render_evidence.sh", p10_dir)
        post_restart_path = p10_dir / "runtime-diagnostics.txt"
        if not post_restart_path.is_file():
            raise ProofError(f"missing retained post-restart diagnostics: {post_restart_path}")
        post_restart_text = post_restart_path.read_text(encoding="utf-8")

        sidecar_after = read_private_file(serial, args.package, args.sidecar)
        (out / "p10-sidecar-after.bin").write_bytes(sidecar_after)
        summary["sidecar_after_sha256"] = sha256_bytes(sidecar_after)
        if sidecar_after != sidecar_before:
            raise ProofError("minimal-save-v1 sidecar changed across the no-input process restart")

        summary["restart_restore"] = validate_restart(pre_restart_text, post_restart_text, sidecar_record)
        summary["p7_report_sha256"] = sha256_file(p7_dir / "runtime-report.txt")
        summary["p89_report_sha256"] = sha256_file(p89_dir / "runtime-report.txt")
        summary["p10_report_sha256"] = sha256_file(p10_dir / "runtime-report.txt")
        summary["status"] = "pass"
    except (OSError, ProofError, subprocess.TimeoutExpired, ValueError) as error:
        summary["error"] = str(error)
        (out / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        (out / "proof.md").write_text(
            "# P7-P10 contiguous proof\n\n"
            "- status: **FAIL**\n"
            f"- error: `{str(error).replace('`', "'")}`\n"
            "- retained artifacts: see sibling evidence directories and collector logs\n",
            encoding="utf-8",
        )
        print(f"P7-P10 proof failed: {error}", file=sys.stderr)
        print(f"Retained evidence: {out}", file=sys.stderr)
        return 1

    (out / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    restored = summary["restart_restore"]
    assert isinstance(restored, dict)
    (out / "proof.md").write_text(
        "# P7-P10 contiguous proof\n\n"
        "- status: **PASS**\n"
        f"- device: `{summary['device_id']}`\n"
        f"- process restart: `{summary['initial_pid']}` -> `{summary['restarted_pid']}`\n"
        f"- scene: `{restored['scene_index']}` / `{restored['scene_guid']}`\n"
        f"- player: `{restored['player_index']}`\n"
        f"- restored motion: `({restored['offset_x']}, {restored['offset_y']})`\n"
        f"- sidecar sha256: `{summary['sidecar_before_sha256']}`\n"
        "- P7 renderer proof: `p7-render/`\n"
        "- P8/P9 spawn + visible input proof: `p8-p9-player-input/`\n"
        "- P10 restart + restored renderer proof: `p10-restart-restore/`\n",
        encoding="utf-8",
    )
    print(f"P7-P10 contiguous proof PASS: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
