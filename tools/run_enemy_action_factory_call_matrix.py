#!/usr/bin/env python3
"""Resolve qualified createWithFile callers and render a per-call parameter matrix."""

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
        "--subtrees",
        type=Path,
        default=default_output_dir / "enemy-actions-create-with-file-caller-parameter-subtrees.tsv",
    )
    parser.add_argument("--output-dir", type=Path, default=default_output_dir)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    ghidra_root = args.ghidra.expanduser().resolve()
    project_dir = args.project_dir.expanduser().resolve()
    subtrees = args.subtrees.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()
    script_dir = repo_root / "tools" / "ghidra"
    identity_exporter = script_dir / "ExportCallerFunctionIdentities.java"
    matrix_tool = repo_root / "tools" / "enemy_action_factory_call_matrix.py"
    identities = output_dir / "enemy-actions-create-with-file-caller-identities.tsv"
    report = output_dir / "enemy-actions-create-with-file-call-matrix-report.txt"

    identity_command = [
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
        "ExportCallerFunctionIdentities.java",
        str(subtrees),
        str(identities),
    ]
    report_command = [
        sys.executable,
        str(matrix_tool),
        str(subtrees),
        str(identities),
        "--output",
        str(report),
    ]

    if args.dry_run:
        print(shlex.join(identity_command))
        print(shlex.join(report_command))
        return 0

    required = [
        ghidra_root / "support" / "analyzeHeadless",
        identity_exporter,
        matrix_tool,
        subtrees,
    ]
    for path in required:
        if not path.is_file():
            print(f"missing required file: {path}", file=sys.stderr)
            return 2
    if not project_dir.is_dir():
        print(f"missing Ghidra project directory: {project_dir}", file=sys.stderr)
        return 2

    output_dir.mkdir(parents=True, exist_ok=True)
    for command in (identity_command, report_command):
        code = run(command)
        if code != 0:
            print(f"command failed with exit code {code}: {shlex.join(command)}", file=sys.stderr)
            return code or 1

    print(f"Caller identities: {identities}")
    print(f"Per-call matrix report: {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
