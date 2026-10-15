#!/usr/bin/env python3
"""Locate a stripped EnemyObject health loader and run the bounded consumer probe."""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path


def build_locator_command(
    ghidra_root: Path,
    project_dir: Path,
    project_name: str,
    program: str,
    script_dir: Path,
    output_tsv: Path,
) -> list[str]:
    return [
        str(ghidra_root / "support" / "analyzeHeadless"),
        str(project_dir),
        project_name,
        "-process",
        program,
        "-noanalysis",
        "-scriptPath",
        str(script_dir),
        "-postScript",
        "ExportEnemyHealthFunctionCandidates.java",
        str(output_tsv),
    ]


def build_analyzer_command(
    analyzer: Path,
    output_tsv: Path,
    report_output: Path,
    entry_output: Path,
) -> list[str]:
    return [
        sys.executable,
        str(analyzer),
        str(output_tsv),
        "--output",
        str(report_output),
        "--entry-output",
        str(entry_output),
    ]


def build_consumer_command(
    consumer_wrapper: Path,
    ghidra_root: Path,
    project_dir: Path,
    project_name: str,
    program: str,
    output_dir: Path,
    function_entry: str,
) -> list[str]:
    return [
        sys.executable,
        str(consumer_wrapper),
        "--ghidra",
        str(ghidra_root),
        "--project-dir",
        str(project_dir),
        "--project-name",
        project_name,
        "--program",
        program,
        "--function",
        function_entry,
        "--output-dir",
        str(output_dir),
    ]


def main(argv: list[str] | None = None) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ghidra", type=Path, default=Path("/opt/ghidra"))
    parser.add_argument("--project-dir", type=Path, required=True)
    parser.add_argument("--project-name", required=True)
    parser.add_argument("--program", required=True)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=repo_root / "build" / "enemy-health-function-locator",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    ghidra_root = args.ghidra.expanduser().resolve()
    project_dir = args.project_dir.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()
    script_dir = repo_root / "tools" / "ghidra"
    analyzer = repo_root / "tools" / "enemy_health_function_locator.py"
    consumer_wrapper = repo_root / "tools" / "run_enemy_health_field_consumer_probe.py"
    locator_tsv = output_dir / "enemy-health-function-candidates.tsv"
    locator_report = output_dir / "enemy-health-function-locator-report.txt"
    entry_output = output_dir / "enemy-health-function-entry.txt"

    locator_command = build_locator_command(
        ghidra_root,
        project_dir,
        args.project_name,
        args.program,
        script_dir,
        locator_tsv,
    )
    analyzer_command = build_analyzer_command(
        analyzer,
        locator_tsv,
        locator_report,
        entry_output,
    )

    if args.dry_run:
        print(shlex.join(locator_command))
        print(shlex.join(analyzer_command))
        placeholder = build_consumer_command(
            consumer_wrapper,
            ghidra_root,
            project_dir,
            args.project_name,
            args.program,
            output_dir,
            "<resolved-function-entry>",
        )
        print(shlex.join(placeholder))
        return 0

    analyze_headless = ghidra_root / "support" / "analyzeHeadless"
    if not analyze_headless.is_file():
        print(f"missing analyzeHeadless: {analyze_headless}", file=sys.stderr)
        return 2
    if not project_dir.is_dir():
        print(f"missing Ghidra project directory: {project_dir}", file=sys.stderr)
        return 2
    if not (script_dir / "ExportEnemyHealthFunctionCandidates.java").is_file():
        print("repository stripped-function locator exporter is missing", file=sys.stderr)
        return 2
    if not analyzer.is_file() or not consumer_wrapper.is_file():
        print("repository health locator/consumer tooling is missing", file=sys.stderr)
        return 2

    output_dir.mkdir(parents=True, exist_ok=True)
    locator = subprocess.run(locator_command, check=False)
    if locator.returncode != 0:
        print(f"Ghidra health-function locator failed with exit code {locator.returncode}", file=sys.stderr)
        return locator.returncode or 1
    if not locator_tsv.is_file() or locator_tsv.stat().st_size == 0:
        print(f"locator did not produce {locator_tsv}", file=sys.stderr)
        return 3

    analyzed = subprocess.run(analyzer_command, check=False)
    if analyzed.returncode != 0:
        print(
            "health-function locator did not resolve exactly one candidate; "
            f"see {locator_report}",
            file=sys.stderr,
        )
        return analyzed.returncode
    if not entry_output.is_file():
        print(f"locator analyzer did not produce {entry_output}", file=sys.stderr)
        return 3
    function_entry = entry_output.read_text(encoding="utf-8").strip()
    if not function_entry:
        print("resolved function entry is empty", file=sys.stderr)
        return 3

    consumer_command = build_consumer_command(
        consumer_wrapper,
        ghidra_root,
        project_dir,
        args.project_name,
        args.program,
        output_dir,
        function_entry,
    )
    consumer = subprocess.run(consumer_command, check=False)
    if consumer.returncode != 0:
        return consumer.returncode

    print(f"Resolved function entry: {function_entry}")
    print(f"Locator report: {locator_report}")
    print(f"Consumer evidence: {output_dir / 'enemy-health-field-consumer-report.txt'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
