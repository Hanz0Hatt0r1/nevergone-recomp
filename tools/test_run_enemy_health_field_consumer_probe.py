#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import io
import tempfile
from pathlib import Path

from run_enemy_health_field_consumer_probe import (
    build_ghidra_command,
    build_report_command,
    main,
)


def test_build_commands() -> None:
    ghidra = Path("/opt/ghidra")
    project_dir = Path("/tmp/project")
    scripts = Path("/repo/tools/ghidra")
    raw = Path("/tmp/out/raw.tsv")
    command = build_ghidra_command(
        ghidra,
        project_dir,
        "Nevergone",
        "libcocos2dcpp.so",
        scripts,
        "loadEnemyObjectFile",
        raw,
    )
    assert command == [
        "/opt/ghidra/support/analyzeHeadless",
        "/tmp/project",
        "Nevergone",
        "-process",
        "libcocos2dcpp.so",
        "-noanalysis",
        "-scriptPath",
        "/repo/tools/ghidra",
        "-postScript",
        "ExportEnemyHealthFieldConsumer.java",
        "loadEnemyObjectFile",
        "/tmp/out/raw.tsv",
    ]

    report = build_report_command(
        Path("/repo/tools/enemy_health_field_consumer_evidence.py"),
        raw,
        Path("/tmp/out/report.txt"),
    )
    assert report[1:] == [
        "/repo/tools/enemy_health_field_consumer_evidence.py",
        "/tmp/out/raw.tsv",
        "--output",
        "/tmp/out/report.txt",
    ]


def test_dry_run() -> None:
    with tempfile.TemporaryDirectory(prefix="nevergone-health-wrapper-") as temp:
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
                    "--function",
                    "002f0000",
                    "--output-dir",
                    str(root / "out"),
                    "--dry-run",
                ]
            )
        assert result == 0
        output = stdout.getvalue()
        assert "ExportEnemyHealthFieldConsumer.java" in output
        assert "002f0000" in output
        assert "enemy_health_field_consumer_evidence.py" in output
        assert "enemy-health-field-consumer.tsv" in output
        assert "enemy-health-field-consumer-report.txt" in output


def main_test() -> int:
    test_build_commands()
    test_dry_run()
    print("Enemy health field consumer wrapper smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main_test())
