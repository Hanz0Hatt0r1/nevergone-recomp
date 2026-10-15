#!/usr/bin/env python3
"""Resolve bounded strings and trace the EnemyObject action dictionary chain."""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path


def run(command: list[str]) -> int:
    return subprocess.run(command, check=False).returncode


def main(argv: list[str] | None = None) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    default_output_dir = repo_root / "build" / "enemy-object-action-init-evidence"

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ghidra", type=Path, default=Path("/opt/ghidra"))
    parser.add_argument("--project-dir", type=Path, required=True)
    parser.add_argument("--project-name", required=True)
    parser.add_argument("--program", required=True)
    parser.add_argument(
        "--pcode",
        type=Path,
        default=default_output_dir / "enemy-object-init-actions-system-pcode.tsv",
    )
    parser.add_argument("--output-dir", type=Path, default=default_output_dir)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    ghidra_root = args.ghidra.expanduser().resolve()
    project_dir = args.project_dir.expanduser().resolve()
    pcode = args.pcode.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()
    script_dir = repo_root / "tools" / "ghidra"
    exporter = script_dir / "ExportPcodeDefinedStrings.java"
    analyzer = repo_root / "tools" / "enemy_object_action_dictionary_chain.py"
    strings = output_dir / "enemy-object-init-actions-defined-strings.tsv"
    report = output_dir / "enemy-object-action-dictionary-chain-report.txt"

    export_command = [
        str(ghidra_root / "support" / "analyzeHeadless"),
        str(project_dir),
        args.project_name,
        "-process",
        args.program,
        "-recursive",
        "-noanalysis",
        "-scriptPath",
        str(script_dir),
        "-postScript",
        "ExportPcodeDefinedStrings.java",
        str(pcode),
        str(strings),
    ]
    analyze_command = [
        sys.executable,
        str(analyzer),
        str(pcode),
        str(strings),
        "--output",
        str(report),
    ]

    if args.dry_run:
        print(shlex.join(export_command))
        print(shlex.join(analyze_command))
        return 0

    required = [
        ghidra_root / "support" / "analyzeHeadless",
        exporter,
        analyzer,
        pcode,
    ]
    for path in required:
        if not path.is_file():
            print(f"missing required file: {path}", file=sys.stderr)
            if path == pcode:
                print(
                    "run tools/run_enemy_object_init_actions_callsite.py first to create the focused P-code TSV",
                    file=sys.stderr,
                )
            return 2
    if not project_dir.is_dir():
        print(f"missing Ghidra project directory: {project_dir}", file=sys.stderr)
        return 2

    output_dir.mkdir(parents=True, exist_ok=True)
    for command in (export_command, analyze_command):
        code = run(command)
        if code != 0:
            print(f"command failed with exit code {code}: {shlex.join(command)}", file=sys.stderr)
            return code or 1

    print(f"Bounded defined strings: {strings}")
    print(f"EnemyObject action dictionary-chain report: {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
