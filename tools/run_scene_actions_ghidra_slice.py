#!/usr/bin/env python3
"""Run the bounded SceneActions Ghidra export and compact report in one command."""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path


def build_command(
    ghidra_root: Path,
    project_dir: Path,
    project_name: str,
    program: str,
    script_dir: Path,
    raw_output: Path,
) -> list[str]:
    analyze_headless = ghidra_root / "support" / "analyzeHeadless"
    return [
        str(analyze_headless),
        str(project_dir),
        project_name,
        "-process",
        program,
        "-noanalysis",
        "-scriptPath",
        str(script_dir),
        "-postScript",
        "ExportSceneActionTriggerSlice.java",
        str(raw_output),
    ]


def main(argv: list[str] | None = None) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument("--ghidra", type=Path, default=Path("/opt/ghidra"))
    parser.add_argument("--project-dir", type=Path, required=True)
    parser.add_argument("--project-name", required=True)
    parser.add_argument("--program", required=True, help="Ghidra program path/name passed to -process")
    parser.add_argument("--output-dir", type=Path, default=repo_root / "build" / "scene-actions-ghidra")
    parser.add_argument("--keep-raw", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    ghidra_root = args.ghidra.expanduser().resolve()
    project_dir = args.project_dir.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()
    script_dir = repo_root / "tools" / "ghidra"
    reporter = repo_root / "tools" / "scene_actions_slice_report.py"
    raw_output = output_dir / "scene-actions-trigger-slice.txt"
    markdown_output = output_dir / "scene-actions-trigger-report.md"
    json_output = output_dir / "scene-actions-trigger-report.json"

    analyze_headless = ghidra_root / "support" / "analyzeHeadless"
    if not args.dry_run:
        if not analyze_headless.is_file():
            print(f"missing analyzeHeadless: {analyze_headless}", file=sys.stderr)
            return 2
        if not project_dir.is_dir():
            print(f"missing project directory: {project_dir}", file=sys.stderr)
            return 2
        if not script_dir.is_dir() or not reporter.is_file():
            print("repository SceneActions analysis tools are missing", file=sys.stderr)
            return 2
        output_dir.mkdir(parents=True, exist_ok=True)

    ghidra_command = build_command(
        ghidra_root,
        project_dir,
        args.project_name,
        args.program,
        script_dir,
        raw_output,
    )
    markdown_command = [
        sys.executable,
        str(reporter),
        str(raw_output),
        "--output",
        str(markdown_output),
    ]
    json_command = [
        sys.executable,
        str(reporter),
        str(raw_output),
        "--json",
        "--output",
        str(json_output),
    ]

    if args.dry_run:
        for command in (ghidra_command, markdown_command, json_command):
            print(shlex.join(command))
        return 0

    ghidra = subprocess.run(ghidra_command, check=False)
    if ghidra.returncode != 0:
        print(f"Ghidra export failed with exit code {ghidra.returncode}", file=sys.stderr)
        return ghidra.returncode or 1
    if not raw_output.is_file() or raw_output.stat().st_size == 0:
        print(f"Ghidra export did not produce {raw_output}", file=sys.stderr)
        return 3

    for command in (markdown_command, json_command):
        result = subprocess.run(command, check=False)
        if result.returncode != 0:
            print(f"SceneActions compact report failed with exit code {result.returncode}", file=sys.stderr)
            return result.returncode

    if not args.keep_raw:
        raw_output.unlink(missing_ok=True)

    print(f"Markdown report: {markdown_output}")
    print(f"JSON report: {json_output}")
    if args.keep_raw:
        print(f"Raw local slice: {raw_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
