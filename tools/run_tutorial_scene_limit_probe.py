#!/usr/bin/env python3
"""Run a two-stage Ghidra probe for LessonLayer tutorial scene-limit loads."""

from __future__ import annotations

import argparse
import csv
import os
import shlex
import subprocess
import sys
from pathlib import Path

REQUIRED_COLUMNS = {
    "function_entry",
    "op_address",
    "op_time",
    "opcode",
    "output",
    "inputs",
    "callee_entry",
    "callee_name",
}


def export_command(
    ghidra_root: Path,
    project_dir: Path,
    project_name: str,
    program: str,
    script_path: str,
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
        script_path,
        "-postScript",
        "ExportFunctionPcodeFlow.java",
        function,
        str(output),
    ]


def direct_callees(path: Path) -> list[tuple[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None or not REQUIRED_COLUMNS.issubset(reader.fieldnames):
            missing = sorted(REQUIRED_COLUMNS.difference(reader.fieldnames or []))
            raise ValueError(f"{path}: missing P-code flow columns: {', '.join(missing)}")
        values: list[tuple[str, str]] = []
        seen: set[str] = set()
        for row in reader:
            if row["opcode"].strip() != "CALL":
                continue
            entry = row["callee_entry"].strip()
            name = row["callee_name"].strip()
            if entry in ("", "-") or entry in seen:
                continue
            seen.add(entry)
            values.append((entry, name))
        return values


def unique_callee(path: Path, label: str) -> tuple[str, str]:
    values = direct_callees(path)
    if len(values) != 1:
        rendered = ", ".join(f"{entry}:{name}" for entry, name in values) or "none"
        raise ValueError(
            f"{label}: expected exactly one resolved direct callee from the thin "
            f"GameSceneUI wrapper, got {len(values)} ({rendered})"
        )
    return values[0]


def run(command: list[str]) -> None:
    result = subprocess.run(command, check=False)
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed with exit code {result.returncode}: {shlex.join(command)}"
        )


def main(argv: list[str] | None = None) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ghidra", type=Path, default=Path("/opt/ghidra"))
    parser.add_argument("--project-dir", type=Path, required=True)
    parser.add_argument("--project-name", required=True)
    parser.add_argument("--program", required=True)
    parser.add_argument(
        "--next-lesson-wrapper",
        default="003e67b8",
        help="Ghidra entry for GameSceneUI::nextLesson()",
    )
    parser.add_argument(
        "--state-next-lesson-wrapper",
        default="003e67ac",
        help="Ghidra entry for GameSceneUI::nextLesson(MEPLAYERSTATE)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=repo_root / "build" / "tutorial-scene-limit-evidence",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    ghidra_root = args.ghidra.expanduser().resolve()
    project_dir = args.project_dir.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()
    script_dir = repo_root / "tools" / "ghidra"
    shared_script_dir = Path(
        os.environ.get("REVENGINSTR_REPO", repo_root.parent / "revenginstr")
    ).expanduser().resolve() / "ghidra"
    exporter = shared_script_dir / "ExportFunctionPcodeFlow.java"
    script_path = f"{shared_script_dir};{script_dir}"
    if not exporter.is_file():
        exporter = script_dir / "ExportFunctionPcodeFlow.java"
        script_path = str(script_dir)
    analyzer = repo_root / "tools" / "tutorial_scene_limit_evidence.py"

    wrapper_noarg = output_dir / "gamesceneui-next-lesson.tsv"
    wrapper_state = output_dir / "gamesceneui-next-lesson-state.tsv"
    target_noarg = output_dir / "lesson-layer-next-lesson.tsv"
    target_state = output_dir / "lesson-layer-next-lesson-state.tsv"
    report = output_dir / "tutorial-scene-limit-report.txt"

    first_stage = [
        export_command(
            ghidra_root, project_dir, args.project_name, args.program, script_path,
            args.next_lesson_wrapper, wrapper_noarg,
        ),
        export_command(
            ghidra_root, project_dir, args.project_name, args.program, script_path,
            args.state_next_lesson_wrapper, wrapper_state,
        ),
    ]

    if args.dry_run:
        for command in first_stage:
            print(shlex.join(command))
        print("# stage 2 resolves each wrapper's unique direct callee from its TSV")
        print(
            f"{shlex.quote(sys.executable)} {shlex.quote(str(analyzer))} "
            f"{shlex.quote(str(target_noarg))} {shlex.quote(str(target_state))} "
            f"--output {shlex.quote(str(report))}"
        )
        return 0

    analyze_headless = ghidra_root / "support" / "analyzeHeadless"
    required = [
        analyze_headless,
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
    try:
        for command in first_stage:
            run(command)

        noarg_entry, noarg_name = unique_callee(wrapper_noarg, "nextLesson()")
        state_entry, state_name = unique_callee(wrapper_state, "nextLesson(MEPLAYERSTATE)")
        print(f"Resolved no-arg LessonLayer target: {noarg_entry} {noarg_name}")
        print(f"Resolved state LessonLayer target: {state_entry} {state_name}")

        second_stage = [
            export_command(
                ghidra_root, project_dir, args.project_name, args.program, script_path,
                noarg_entry, target_noarg,
            ),
            export_command(
                ghidra_root, project_dir, args.project_name, args.program, script_path,
                state_entry, target_state,
            ),
        ]
        for command in second_stage:
            run(command)

        analyzer_command = [
            sys.executable,
            str(analyzer),
            str(target_noarg),
            str(target_state),
            "--output",
            str(report),
        ]
        run(analyzer_command)
    except (OSError, ValueError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        return 3

    print(f"Wrapper metadata: {wrapper_noarg}")
    print(f"Wrapper state metadata: {wrapper_state}")
    print(f"LessonLayer metadata: {target_noarg}")
    print(f"LessonLayer state metadata: {target_state}")
    print(f"Evidence report: {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
