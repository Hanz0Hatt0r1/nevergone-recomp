#!/usr/bin/env python3
"""Trace reused EnemyObject action-dictionary key stack-slot lifetimes."""

from __future__ import annotations

import argparse
import os
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
    shared_script_dir = Path(
        os.environ.get("REVENGINSTR_REPO", repo_root.parent / "revenginstr")
    ).expanduser().resolve() / "ghidra"
    shared_identity_exporter = shared_script_dir / "ExportGhidraFunctionIdentities.java"
    use_shared_identity_exporter = shared_identity_exporter.is_file()
    analyzer = repo_root / "tools" / "enemy_object_action_key_lifetime.py"
    exporter = script_dir / "ExportActionKeyLifetimeFunctionIdentities.java"
    plan = output_dir / "enemy-object-action-key-lifetime-plan.tsv"
    identities = output_dir / "enemy-object-action-key-lifetime-helper-identities.tsv"
    report = output_dir / "enemy-object-action-key-lifetime-report.txt"

    plan_command = [sys.executable, str(analyzer), "plan", str(pcode), str(plan)]
    export_command = [
        str(ghidra_root / "support" / "analyzeHeadless"),
        str(project_dir),
        args.project_name,
        "-process",
        args.program,
        "-recursive",
        "-noanalysis",
        "-scriptPath",
        f"{shared_script_dir};{script_dir}" if use_shared_identity_exporter else str(script_dir),
        "-postScript",
        "ExportActionKeyLifetimeFunctionIdentities.java",
        str(plan),
        str(identities),
    ]
    if use_shared_identity_exporter:
        export_command.append(str(shared_script_dir))
    report_command = [
        sys.executable,
        str(analyzer),
        "report",
        str(pcode),
        str(plan),
        str(identities),
        "--output",
        str(report),
    ]

    if args.dry_run:
        for command in (plan_command, export_command, report_command):
            print(shlex.join(command))
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
    for command in (plan_command, export_command, report_command):
        code = run(command)
        if code != 0:
            print(f"command failed with exit code {code}: {shlex.join(command)}", file=sys.stderr)
            return code or 1

    print(f"Key lifetime plan: {plan}")
    print(f"Key lifetime helper identities: {identities}")
    print(f"EnemyObject action key-lifetime report: {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
