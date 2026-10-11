#!/usr/bin/env python3
"""Validate retained Nevergone Android runtime evidence."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path

SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
TIMESTAMP_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
ALLOWED_PAGE_SIZES = {"4096", "16384"}
COUNTER_RE = re.compile(
    r"^renderer lifecycle counters \((initial launch|resume|activity recreation|cold restart)\):\s+"
    r"(\d+)\s+(\d+)\s+(\d+)\s*$",
    re.MULTILINE,
)
REQUIRED_COUNTER_PHASES = (
    "initial launch",
    "resume",
    "activity recreation",
    "cold restart",
)
REQUIRED_KEYS = {
    "status",
    "timestamp_utc",
    "android_release",
    "android_sdk",
    "manufacturer",
    "model",
    "device",
    "primary_abi",
    "abi_list",
    "page_size",
    "apk_sha256",
    "lifecycle_transcript",
}


@dataclass(frozen=True)
class Evidence:
    values: dict[str, str]
    report_path: Path


@dataclass(frozen=True)
class Counters:
    resume: int
    pause: int
    surface: int


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


def parse_renderer_counters(text: str) -> dict[str, Counters]:
    counters: dict[str, Counters] = {}
    for match in COUNTER_RE.finditer(text):
        phase = match.group(1)
        if phase in counters:
            raise ValueError(f"duplicate renderer counter phase: {phase}")
        counters[phase] = Counters(
            resume=int(match.group(2)),
            pause=int(match.group(3)),
            surface=int(match.group(4)),
        )

    missing = [phase for phase in REQUIRED_COUNTER_PHASES if phase not in counters]
    if missing:
        raise ValueError("lifecycle transcript is missing renderer counter phases: " + ", ".join(missing))
    return counters


def validate_counter_progression(counters: dict[str, Counters]) -> None:
    initial = counters["initial launch"]
    resumed = counters["resume"]
    recreated = counters["activity recreation"]
    cold = counters["cold restart"]

    if initial.resume < 1 or initial.surface < 1:
        raise ValueError("initial renderer counter baseline is invalid")
    if resumed.resume <= initial.resume:
        raise ValueError("resume events did not increase after Home/resume")
    if resumed.pause <= initial.pause:
        raise ValueError("pause events did not increase after Home/resume")
    if recreated.resume <= resumed.resume:
        raise ValueError("resume events did not increase after Activity recreation")
    if recreated.pause <= resumed.pause:
        raise ValueError("pause events did not increase after Activity recreation")
    if recreated.surface <= resumed.surface:
        raise ValueError("GL surface generation did not increase after Activity recreation")
    if cold.resume < 1 or cold.surface < 1:
        raise ValueError("cold-restart renderer counter baseline is invalid")


def validate(evidence: Evidence, require_pass: bool = True) -> None:
    values = evidence.values
    status = values["status"]
    if status not in {"running", "pass", "fail"}:
        raise ValueError(f"invalid status: {status!r}")
    if require_pass and status != "pass":
        raise ValueError(f"evidence status is not pass: {status!r}")
    if values["android_sdk"] not in {"35", "36"}:
        raise ValueError(f"unsupported Android SDK: {values['android_sdk']!r}")
    if values["primary_abi"] != "arm64-v8a":
        raise ValueError(f"primary ABI is not arm64-v8a: {values['primary_abi']!r}")
    if "arm64-v8a" not in values["abi_list"].split(","):
        raise ValueError("ABI list does not contain arm64-v8a")
    if values["page_size"] not in ALLOWED_PAGE_SIZES:
        raise ValueError(f"unexpected page size: {values['page_size']!r}")
    if not SHA256_RE.fullmatch(values["apk_sha256"]):
        raise ValueError("apk_sha256 is not a lowercase SHA-256 digest")
    if not TIMESTAMP_RE.fullmatch(values["timestamp_utc"]):
        raise ValueError(f"invalid UTC timestamp: {values['timestamp_utc']!r}")

    transcript_name = values["lifecycle_transcript"]
    transcript = evidence.report_path.parent / transcript_name
    if Path(transcript_name).name != transcript_name:
        raise ValueError("lifecycle_transcript must be a basename")
    try:
        text = transcript.read_text(encoding="utf-8")
    except OSError as error:
        raise ValueError(f"cannot read lifecycle transcript {transcript}: {error}") from error
    if not text.strip():
        raise ValueError("lifecycle transcript is empty")
    if "<adb-serial>" not in text and re.search(r"(?:emulator-\d+|[A-Za-z0-9]{12,})", text):
        if re.search(r"\b(?:emulator-\d+|[A-Fa-f0-9]{16,})\b", text):
            raise ValueError("lifecycle transcript appears to contain an unsanitized adb serial")

    if status == "pass":
        validate_counter_progression(parse_renderer_counters(text))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--allow-failure", action="store_true", help="validate diagnostic evidence without requiring status=pass")
    args = parser.parse_args()

    try:
        evidence = parse_report(args.report)
        validate(evidence, require_pass=not args.allow_failure)
    except ValueError as error:
        parser.error(str(error))
    print(f"Android runtime evidence valid: {args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
