#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile

MODULE_PATH = Path(__file__).with_name("android_runtime_evidence.py")
spec = importlib.util.spec_from_file_location("android_runtime_evidence", MODULE_PATH)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)


TRANSCRIPT = """renderer lifecycle counters (initial launch): 1 0 1
renderer lifecycle counters (resume): 2 1 1
renderer lifecycle counters (activity recreation): 3 2 2
renderer lifecycle counters (cold restart): 1 0 1
"""


def write_report(root: Path, **overrides: str) -> Path:
    values = {
        "status": "pass",
        "timestamp_utc": "2026-10-11T02:30:00Z",
        "android_release": "16",
        "android_sdk": "36",
        "manufacturer": "Example",
        "model": "Device",
        "device": "device_name",
        "primary_abi": "arm64-v8a",
        "abi_list": "arm64-v8a,armeabi-v7a",
        "page_size": "16384",
        "apk_sha256": "a" * 64,
        "lifecycle_transcript": "lifecycle-smoke.txt",
    }
    values.update(overrides)
    report = root / "runtime-report.txt"
    report.write_text("".join(f"{key}={value}\n" for key, value in values.items()), encoding="utf-8")
    (root / "lifecycle-smoke.txt").write_text(TRANSCRIPT, encoding="utf-8")
    return report


def expect_invalid(report: Path, *, require_pass: bool = True) -> None:
    try:
        module.validate(module.parse_report(report), require_pass=require_pass)
    except ValueError:
        return
    raise AssertionError(f"expected invalid evidence: {report}")


def expect_invalid_transcript(root: Path, text: str) -> None:
    report = write_report(root)
    (root / "lifecycle-smoke.txt").write_text(text, encoding="utf-8")
    expect_invalid(report)


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        report = write_report(root)
        module.validate(module.parse_report(report))

        expect_invalid(write_report(root, status="fail"))
        failed = write_report(root, status="fail")
        (root / "lifecycle-smoke.txt").write_text(
            "Activity start failed during initial launch\n",
            encoding="utf-8",
        )
        module.validate(module.parse_report(failed), require_pass=False)

        expect_invalid(write_report(root, android_sdk="34"))
        expect_invalid(write_report(root, primary_abi="x86_64"))
        expect_invalid(write_report(root, abi_list="armeabi-v7a"))
        expect_invalid(write_report(root, page_size="8192"))
        expect_invalid(write_report(root, apk_sha256="xyz"))
        expect_invalid(write_report(root, lifecycle_transcript="../secret.txt"))

        expect_invalid_transcript(
            root,
            "renderer lifecycle counters (initial launch): 1 0 1\n",
        )
        expect_invalid_transcript(
            root,
            """renderer lifecycle counters (initial launch): 1 0 1
renderer lifecycle counters (resume): 1 1 1
renderer lifecycle counters (activity recreation): 2 2 2
renderer lifecycle counters (cold restart): 1 0 1
""",
        )
        expect_invalid_transcript(
            root,
            """renderer lifecycle counters (initial launch): 1 0 1
renderer lifecycle counters (resume): 2 1 1
renderer lifecycle counters (activity recreation): 3 2 1
renderer lifecycle counters (cold restart): 1 0 1
""",
        )
        expect_invalid_transcript(
            root,
            TRANSCRIPT + "renderer lifecycle counters (resume): 4 3 2\n",
        )
        expect_invalid_transcript(
            root,
            """renderer lifecycle counters (initial launch): 0 0 0
renderer lifecycle counters (resume): 1 1 0
renderer lifecycle counters (activity recreation): 2 2 1
renderer lifecycle counters (cold restart): 1 0 1
""",
        )
        expect_invalid_transcript(
            root,
            """renderer lifecycle counters (initial launch): 1 0 1
renderer lifecycle counters (resume): 2 1 1
renderer lifecycle counters (activity recreation): 3 2 2
renderer lifecycle counters (cold restart): 0 0 0
""",
        )

        report = write_report(root)
        text = report.read_text(encoding="utf-8")
        report.write_text(text.replace("android_sdk=36\n", ""), encoding="utf-8")
        expect_invalid(report)

        report = write_report(root)
        with report.open("a", encoding="utf-8") as handle:
            handle.write("android_sdk=36\n")
        expect_invalid(report)

    print("Android runtime evidence validator smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
