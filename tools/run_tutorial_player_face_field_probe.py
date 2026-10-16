#!/usr/bin/env python3
"""Run bounded Ghidra probes for MEPlayer isFace and PLAYERFACEDIR fields."""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
from pathlib import Path


def ghidra_command(
    ghidra_root: Path,
    project_dir: Path,
    project_name: str,
    program: str,
    script_dir: Path,
    script: str,
    function: str,
    output: Path,
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
        script,
        function,
        str(output),
    ]


def main(argv: list[str] | None = None) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ghidra", type=Path, default=Path("/opt/ghidra"))
    parser.add_argument("--project-dir", type=Path, required=True)
    parser.add_argument("--project-name", required=True)
    parser.add_argument("--program", required=True)
    parser.add_argument(
        "--is-face-function",
        default="0032c748",
        help="Ghidra entry for MEPlayer::isFace (default matches the analyzed 1.0.9 image)",
    )
    parser.add_argument(
        "--player-dir-function",
        default="0032c75a",
        help="Ghidra entry for MEPlayer::setPlayerDir (default matches the analyzed 1.0.9 image)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=repo_root / "build" / "tutorial-player-face-fields",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    ghidra_root = args.ghidra.expanduser().resolve()
    project_dir = args.project_dir.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()
    script_dir = repo_root / "tools" / "ghidra"
    analyzer = repo_root / "tools" / "tutorial_player_face_field_evidence.py"
    adapter = script_dir / "adapt_revenginstr_parameter_store_field.py"
    shared_repo_value = os.environ.get("REVENGINSTR_REPO")
    shared_repo = (
        Path(shared_repo_value).expanduser()
        if shared_repo_value
        else repo_root.parent / "revenginstr"
    )
    shared_runner = shared_repo / "scripts" / "run_ghidra_parameter_store_field.sh"
    shared_exporter = shared_repo / "ghidra" / "ExportGhidraParameterStoreField.java"
    use_shared_parameter_exporter = (
        shared_runner.is_file()
        and os.access(shared_runner, os.X_OK)
        and shared_exporter.is_file()
        and adapter.is_file()
    )
    is_face_tsv = output_dir / "is-face-field.tsv"
    player_dir_tsv = output_dir / "player-face-dir-field.tsv"
    generic_player_dir_tsv = output_dir / "player-face-dir-field.revenginstr.tsv"
    report = output_dir / "tutorial-player-face-field-report.txt"

    commands = [
        ghidra_command(
            ghidra_root,
            project_dir,
            args.project_name,
            args.program,
            script_dir,
            "ExportAccessorReturnField.java",
            args.is_face_function,
            is_face_tsv,
        ),
    ]
    if use_shared_parameter_exporter:
        commands.extend([
            [
                str(shared_runner),
                str(project_dir),
                args.project_name,
                args.program,
                args.player_dir_function,
                str(generic_player_dir_tsv),
            ],
            [sys.executable, str(adapter), str(generic_player_dir_tsv), str(player_dir_tsv)],
        ])
    else:
        commands.append(ghidra_command(
            ghidra_root,
            project_dir,
            args.project_name,
            args.program,
            script_dir,
            "ExportParameterStoreField.java",
            args.player_dir_function,
            player_dir_tsv,
        ))
    commands.append([
        sys.executable,
        str(analyzer),
        str(is_face_tsv),
        str(player_dir_tsv),
        "--output",
        str(report),
    ])

    if args.dry_run:
        for command in commands:
            print(shlex.join(command))
        return 0

    analyze_headless = ghidra_root / "support" / "analyzeHeadless"
    if not analyze_headless.is_file():
        print(f"missing analyzeHeadless: {analyze_headless}", file=sys.stderr)
        return 2
    if not project_dir.is_dir():
        print(f"missing Ghidra project directory: {project_dir}", file=sys.stderr)
        return 2
    required_tools = [script_dir / "ExportAccessorReturnField.java", analyzer]
    if not use_shared_parameter_exporter:
        required_tools.append(script_dir / "ExportParameterStoreField.java")
    else:
        required_tools.append(adapter)
    for required in required_tools:
        if not required.is_file():
            print(f"missing repository tool: {required}", file=sys.stderr)
            return 2

    output_dir.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["GHIDRA_HOME"] = str(ghidra_root)
    for command in commands:
        result = subprocess.run(command, check=False, env=environment)
        if result.returncode != 0:
            print(f"command failed with exit code {result.returncode}: {shlex.join(command)}", file=sys.stderr)
            return result.returncode or 1
    if use_shared_parameter_exporter:
        generic_player_dir_tsv.unlink(missing_ok=True)

    print(f"isFace metadata: {is_face_tsv}")
    print(f"PLAYERFACEDIR metadata: {player_dir_tsv}")
    print(f"Evidence report: {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
