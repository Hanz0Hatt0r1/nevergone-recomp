#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import io
import tempfile
from pathlib import Path

from run_enemy_health_function_locator import (
    build_analyzer_command,
    build_consumer_command,
    build_locator_command,
    main,
)


def test_commands() -> None:
    locator = build_locator_command(
        Path("/opt/ghidra"),
        Path("/tmp/project"),
        "Nevergone",
        "libcocos2dcpp.so",
        Path("/repo/tools/ghidra"),
        Path("/tmp/out/candidates.tsv"),
    )
    assert locator == [
        "/opt/ghidra/support/analyzeHeadless",
        "/tmp/project",
        "Nevergone",
        "-process",
        "libcocos2dcpp.so",
        "-noanalysis",
        "-scriptPath",
        "/repo/tools/ghidra",
        "-postScript",
        "ExportEnemyHealthFunctionCandidates.java",
        "/tmp/out/candidates.tsv",
    ]

    analyzer = build_analyzer_command(
        Path("/repo/tools/enemy_health_function_locator.py"),
        Path("/tmp/out/candidates.tsv"),
        Path("/tmp/out/report.txt"),
        Path("/tmp/out/entry.txt"),
    )
    assert analyzer[1:] == [
        "/repo/tools/enemy_health_function_locator.py",
        "/tmp/out/candidates.tsv",
        "--output",
        "/tmp/out/report.txt",
        "--entry-output",
        "/tmp/out/entry.txt",
    ]

    consumer = build_consumer_command(
        Path("/repo/tools/run_enemy_health_field_consumer_probe.py"),
        Path("/opt/ghidra"),
        Path("/tmp/project"),
        "Nevergone",
        "libcocos2dcpp.so",
        Path("/tmp/out"),
        "002b1000",
    )
    assert consumer[1:] == [
        "/repo/tools/run_enemy_health_field_consumer_probe.py",
        "--ghidra",
        "/opt/ghidra",
        "--project-dir",
        "/tmp/project",
        "--project-name",
        "Nevergone",
        "--program",
        "libcocos2dcpp.so",
        "--function",
        "002b1000",
        "--output-dir",
        "/tmp/out",
    ]


def test_dry_run() -> None:
    with tempfile.TemporaryDirectory(prefix="nevergone-health-locator-wrapper-") as temp:
        root = Path(temp)
        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            result = main(
                [
                    "--ghidra",
                    str(root / "ghidra"),
                    "--project-dir",
                    str(root / "project"),
                    "--project-name",
                    "Nevergone",
                    "--program",
                    "libcocos2dcpp.so",
                    "--output-dir",
                    str(root / "out"),
                    "--dry-run",
                ]
            )
        assert result == 0
        output = stdout.getvalue()
        assert "ExportEnemyHealthFunctionCandidates.java" in output
        assert "enemy_health_function_locator.py" in output
        assert "<resolved-function-entry>" in output
        assert "run_enemy_health_field_consumer_probe.py" in output


def main_test() -> int:
    test_commands()
    test_dry_run()
    print("Enemy health function locator wrapper smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main_test())
