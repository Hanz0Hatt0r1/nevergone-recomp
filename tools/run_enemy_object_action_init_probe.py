#!/usr/bin/env python3
"""Run the bounded EnemyObject -> EnemyActions action-init evidence probe."""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path


def main(argv: list[str] | None = None) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ghidra", type=Path, default=Path("/opt/ghidra"))
    parser.add_argument("--project-dir", type=Path, required=True)
    parser.add_argument("--project-name", required=True)
    parser.add_argument("--program", required=True)
    parser.add_argument(
        "--load-enemy-object",
        default="loadEnemyObjectFile",
        help="exact/unique function token or Ghidra address for EnemyObject::loadEnemyObjectFile",
    )
    parser.add_argument("--init-entry", help="optional exact Ghidra entry for EnemyActionsSystem::initWithFile")
    parser.add_argument("--add-entry", help="optional exact Ghidra entry for ActionDataManager::addActionDataWithID")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=repo_root / "build" / "enemy-object-action-init-evidence",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    ghidra_root = args.ghidra.expanduser().resolve()
    project_dir = args.project_dir.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()
    script_dir = repo_root / "tools" / "ghidra"
    exporter = script_dir / "ExportFunctionPcodeFlow.java"
    analyzer = repo_root / "tools" / "enemy_object_action_init_evidence.py"
    flow_tsv = output_dir / "load-enemy-object-pcode.tsv"
    report = output_dir / "enemy-object-action-init-report.txt"

    export_command = [
        str(ghidra_root / "support" / "analyzeHeadless"),
        str(project_dir),
        args.project_name,
        "-process",
        args.program,
        "-noanalysis",
        "-scriptPath",
        str(script_dir),
        "-postScript",
        "ExportFunctionPcodeFlow.java",
        args.load_enemy_object,
        str(flow_tsv),
    ]
    analyze_command = [sys.executable, str(analyzer), str(flow_tsv), "--output", str(report)]
    if args.init_entry:
        analyze_command.extend(["--init-entry", args.init_entry])
    if args.add_entry:
        analyze_command.extend(["--add-entry", args.add_entry])

    if args.dry_run:
        print(shlex.join(export_command))
        print(shlex.join(analyze_command))
        return 0

    for path in (ghidra_root / "support" / "analyzeHeadless", exporter, analyzer):
        if not path.is_file():
            print(f"missing required tool: {path}", file=sys.stderr)
            return 2
    if not project_dir.is_dir():
        print(f"missing Ghidra project directory: {project_dir}", file=sys.stderr)
        return 2

    output_dir.mkdir(parents=True, exist_ok=True)
    for command in (export_command, analyze_command):
        result = subprocess.run(command, check=False)
        if result.returncode != 0:
            print(
                f"command failed with exit code {result.returncode}: {shlex.join(command)}",
                file=sys.stderr,
            )
            return result.returncode or 1

    print(f"P-code flow: {flow_tsv}")
    print(f"Enemy action-init evidence: {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
