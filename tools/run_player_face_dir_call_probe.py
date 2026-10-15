#!/usr/bin/env python3
"""Run a bounded Ghidra probe for PLAYERFACEDIR call arguments."""

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
        "--set-controls-state",
        default="0032ddee",
        help="Ghidra entry for MEPlayer::setControlsState in the analyzed 1.0.9 image",
    )
    parser.add_argument(
        "--set-player-dir",
        default="0032c75a",
        help="Ghidra entry for MEPlayer::setPlayerDir in the analyzed 1.0.9 image",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=repo_root / "build" / "player-face-dir-call-evidence",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    ghidra_root = args.ghidra.expanduser().resolve()
    project_dir = args.project_dir.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()
    script_dir = repo_root / "tools" / "ghidra"
    exporter = script_dir / "ExportFunctionPcodeFlow.java"
    analyzer = repo_root / "tools" / "player_face_dir_call_evidence.py"
    flow_tsv = output_dir / "set-controls-state-pcode.tsv"
    report = output_dir / "player-face-dir-call-report.txt"

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
        args.set_controls_state,
        str(flow_tsv),
    ]
    analyze_command = [
        sys.executable,
        str(analyzer),
        str(flow_tsv),
        "--callee-entry",
        args.set_player_dir,
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
    ]
    for path in required:
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
    print(f"PLAYERFACEDIR call evidence: {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
