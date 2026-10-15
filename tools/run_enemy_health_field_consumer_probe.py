#!/usr/bin/env python3
"""Run the bounded EnemyObject health-field Ghidra consumer probe."""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path


def build_ghidra_command(
    ghidra_root: Path,
    project_dir: Path,
    project_name: str,
    program: str,
    script_dir: Path,
    function_token: str,
    raw_output: Path,
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
        "ExportEnemyHealthFieldConsumer.java",
        function_token,
        str(raw_output),
    ]


def build_report_command(analyzer: Path, raw_output: Path, report_output: Path) -> list[str]:
    return [
        sys.executable,
        str(analyzer),
        str(raw_output),
        "--output",
        str(report_output),
    ]


def main(argv: list[str] | None = None) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ghidra", type=Path, default=Path("/opt/ghidra"))
    parser.add_argument("--project-dir", type=Path, required=True)
    parser.add_argument("--project-name", required=True)
    parser.add_argument("--program", required=True, help="Ghidra program path/name passed to -process")
    parser.add_argument(
        "--function",
        default="loadEnemyObjectFile",
        help="exact/unique partial function name or exact Ghidra address",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=repo_root / "build" / "enemy-health-field-consumer",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    ghidra_root = args.ghidra.expanduser().resolve()
    project_dir = args.project_dir.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()
    script_dir = repo_root / "tools" / "ghidra"
    analyzer = repo_root / "tools" / "enemy_health_field_consumer_evidence.py"
    raw_output = output_dir / "enemy-health-field-consumer.tsv"
    report_output = output_dir / "enemy-health-field-consumer-report.txt"

    ghidra_command = build_ghidra_command(
        ghidra_root,
        project_dir,
        args.project_name,
        args.program,
        script_dir,
        args.function,
        raw_output,
    )
    report_command = build_report_command(analyzer, raw_output, report_output)

    if args.dry_run:
        print(shlex.join(ghidra_command))
        print(shlex.join(report_command))
        return 0

    analyze_headless = ghidra_root / "support" / "analyzeHeadless"
    if not analyze_headless.is_file():
        print(f"missing analyzeHeadless: {analyze_headless}", file=sys.stderr)
        return 2
    if not project_dir.is_dir():
        print(f"missing Ghidra project directory: {project_dir}", file=sys.stderr)
        return 2
    if not (script_dir / "ExportEnemyHealthFieldConsumer.java").is_file():
        print("repository EnemyObject health consumer exporter is missing", file=sys.stderr)
        return 2
    if not analyzer.is_file():
        print("repository EnemyObject health consumer analyzer is missing", file=sys.stderr)
        return 2

    output_dir.mkdir(parents=True, exist_ok=True)
    ghidra = subprocess.run(ghidra_command, check=False)
    if ghidra.returncode != 0:
        print(f"Ghidra health-field export failed with exit code {ghidra.returncode}", file=sys.stderr)
        return ghidra.returncode or 1
    if not raw_output.is_file() or raw_output.stat().st_size == 0:
        print(f"Ghidra export did not produce {raw_output}", file=sys.stderr)
        return 3

    report = subprocess.run(report_command, check=False)
    if report.returncode != 0:
        print(f"health-field evidence analysis failed with exit code {report.returncode}", file=sys.stderr)
        return report.returncode

    print(f"Raw metadata: {raw_output}")
    print(f"Evidence report: {report_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
