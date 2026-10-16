#!/usr/bin/env python3
"""Run the bounded EnemyObject -> EnemyActions action-init evidence probe."""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
from pathlib import Path

DEFAULT_INIT_TARGET = "EnemyActionsSystem::initWithFile"
EXPECTED_INIT_NAME = "EnemyActionsSystem::initWithFile"
FACTORY_REPORT_TITLE = "EnemyActions createWithFile caller parameter subtree evidence"


def run(command: list[str]) -> int:
    result = subprocess.run(command, check=False)
    return result.returncode


def main(argv: list[str] | None = None) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ghidra", type=Path, default=Path("/opt/ghidra"))
    parser.add_argument("--project-dir", type=Path, required=True)
    parser.add_argument("--project-name", required=True)
    parser.add_argument(
        "--program",
        required=True,
        help="Ghidra program filename; nested project folders are searched recursively",
    )
    parser.add_argument(
        "--load-enemy-object",
        default="loadEnemyObjectFile",
        help="exact/unique function token or Ghidra image address for EnemyObject::loadEnemyObjectFile",
    )
    parser.add_argument(
        "--max-depth",
        type=int,
        default=4,
        help="maximum direct-call/reverse-call depth exported (default: 4)",
    )
    parser.add_argument(
        "--max-functions",
        type=int,
        default=96,
        help="maximum number of functions decompiled by each bounded exporter (default: 96)",
    )
    parser.add_argument(
        "--init-target",
        default=DEFAULT_INIT_TARGET,
        help=(
            "symbol token used to locate EnemyActionsSystem::initWithFile in Ghidra "
            f"(default: {DEFAULT_INIT_TARGET})"
        ),
    )
    parser.add_argument(
        "--init-entry",
        help=(
            "optional exact Ghidra image address for EnemyActionsSystem::initWithFile; "
            "when supplied it overrides --init-target for reverse traversal and is also "
            "used as an analyzer fallback. Do not pass an ELF file offset here."
        ),
    )
    parser.add_argument(
        "--add-entry",
        help=(
            "optional exact Ghidra image address for ActionDataManager::addActionDataWithID; "
            "used only as an analyzer fallback. Do not pass an ELF file offset here."
        ),
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=repo_root / "build" / "enemy-object-action-init-evidence",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    if args.max_depth < 0:
        parser.error("--max-depth must be >= 0")
    if args.max_functions < 1:
        parser.error("--max-functions must be >= 1")

    ghidra_root = args.ghidra.expanduser().resolve()
    project_dir = args.project_dir.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()
    script_dir = repo_root / "tools" / "ghidra"
    shared_script_dir = Path(
        os.environ.get("REVENGINSTR_REPO", repo_root.parent / "revenginstr")
    ).expanduser().resolve() / "ghidra"
    shared_forward_exporter = shared_script_dir / "ExportBoundedCallGraphPcodeFlow.java"
    shared_reverse_exporter = shared_script_dir / "ExportBoundedReverseCallGraphPcodeFlow.java"
    shared_caller_subtree_exporter = shared_script_dir / "ExportGhidraCallerArgumentSubtree.java"
    use_shared_exporters = shared_forward_exporter.is_file() and shared_reverse_exporter.is_file()
    if use_shared_exporters:
        forward_exporter = shared_forward_exporter
        reverse_exporter = shared_reverse_exporter
        script_path = f"{shared_script_dir};{script_dir}"
    elif shared_caller_subtree_exporter.is_file():
        forward_exporter = script_dir / "ExportBoundedCallGraphPcodeFlow.java"
        reverse_exporter = script_dir / "ExportBoundedReverseCallGraphPcodeFlow.java"
        script_path = f"{shared_script_dir};{script_dir}"
    else:
        forward_exporter = script_dir / "ExportBoundedCallGraphPcodeFlow.java"
        reverse_exporter = script_dir / "ExportBoundedReverseCallGraphPcodeFlow.java"
        script_path = str(script_dir)
    caller_subtree_exporter = script_dir / "ExportCallerArgumentSubtree.java"
    analyzer = repo_root / "tools" / "enemy_object_action_init_evidence.py"
    caller_arguments = repo_root / "tools" / "enemy_action_init_caller_arguments.py"
    forward_tsv = output_dir / "load-enemy-object-callgraph-pcode.tsv"
    reverse_tsv = output_dir / "enemy-actions-init-reverse-callgraph-pcode.tsv"
    report = output_dir / "enemy-object-action-init-report.txt"
    caller_plan = output_dir / "enemy-actions-init-caller-argument-plan.tsv"
    caller_subtrees = output_dir / "enemy-actions-init-caller-argument-subtrees.tsv"
    caller_report = output_dir / "enemy-actions-init-caller-argument-report.txt"
    factory_plan = output_dir / "enemy-actions-create-with-file-caller-parameter-plan.tsv"
    factory_subtrees = output_dir / "enemy-actions-create-with-file-caller-parameter-subtrees.tsv"
    factory_report = output_dir / "enemy-actions-create-with-file-caller-parameter-report.txt"

    common_ghidra = [
        str(ghidra_root / "support" / "analyzeHeadless"),
        str(project_dir),
        args.project_name,
        "-process",
        args.program,
        "-recursive",
        "-noanalysis",
        "-scriptPath",
        script_path,
        "-postScript",
    ]
    forward_export_command = common_ghidra + [
        "ExportBoundedCallGraphPcodeFlow.java",
        args.load_enemy_object,
        str(forward_tsv),
        str(args.max_depth),
        str(args.max_functions),
    ]
    if use_shared_exporters:
        forward_export_command.extend([
            "--default-priority=3",
            "--priority=0:EnemyActionsSystem",
            "--priority=0:ActionDataManager",
            "--priority=1:EnemyObject",
            "--priority=1:Enemy",
            "--priority=1:Action",
            "--priority=2:FUN_",
            "--priority=4:cocos2d::",
            "--priority=4:std::",
            "--priority=4:__gnu_cxx::",
        ])

    reverse_target = args.init_entry or args.init_target
    reverse_export_command = common_ghidra + [
        "ExportBoundedReverseCallGraphPcodeFlow.java",
        reverse_target,
        str(reverse_tsv),
        str(args.max_depth),
        str(args.max_functions),
        EXPECTED_INIT_NAME,
    ]
    if use_shared_exporters:
        reverse_export_command.extend([
            "--default-priority=4",
            "--priority=0:EnemyObject",
            "--priority=1:EnemyActionsSystem",
            "--priority=1:ActionDataManager",
            "--priority=2:Enemy",
            "--priority=2:Action",
            "--priority=3:FUN_",
            "--priority=5:cocos2d::",
            "--priority=5:std::",
            "--priority=5:__gnu_cxx::",
        ])

    def analyze_command(tsv: Path) -> list[str]:
        command = [
            sys.executable,
            str(analyzer),
            str(tsv),
            "--output",
            str(report),
        ]
        if args.init_entry:
            command.extend(["--init-entry", args.init_entry])
        if args.add_entry:
            command.extend(["--add-entry", args.add_entry])
        return command

    def caller_plan_command(tsv: Path) -> list[str]:
        return [
            sys.executable,
            str(caller_arguments),
            "plan",
            str(tsv),
            str(caller_plan),
        ]

    def factory_plan_command(tsv: Path) -> list[str]:
        return [
            sys.executable,
            str(caller_arguments),
            "factory-plan",
            str(tsv),
            str(factory_plan),
        ]

    caller_subtree_export_command = common_ghidra + [
        "ExportCallerArgumentSubtree.java",
        str(caller_plan),
        str(caller_subtrees),
    ]
    if shared_caller_subtree_exporter.is_file():
        caller_subtree_export_command.append(str(shared_script_dir))
    caller_report_command = [
        sys.executable,
        str(caller_arguments),
        "report",
        str(caller_subtrees),
        "--output",
        str(caller_report),
    ]
    factory_subtree_export_command = common_ghidra + [
        "ExportCallerArgumentSubtree.java",
        str(factory_plan),
        str(factory_subtrees),
    ]
    if shared_caller_subtree_exporter.is_file():
        factory_subtree_export_command.append(str(shared_script_dir))
    factory_report_command = [
        sys.executable,
        str(caller_arguments),
        "report",
        str(factory_subtrees),
        "--title",
        FACTORY_REPORT_TITLE,
        "--output",
        str(factory_report),
    ]

    forward_analyze_command = analyze_command(forward_tsv)
    reverse_analyze_command = analyze_command(reverse_tsv)

    if args.dry_run:
        print(shlex.join(forward_export_command))
        print(shlex.join(forward_analyze_command))
        print("# fallback only when the forward analyzer returns status 2 (no init call)")
        print(shlex.join(reverse_export_command))
        print(shlex.join(reverse_analyze_command))
        print("# after a structural init hit, extract receiver + explicit caller arguments")
        print(shlex.join(caller_plan_command(reverse_tsv)))
        print(shlex.join(caller_subtree_export_command))
        print(shlex.join(caller_report_command))
        print("# then trace every direct createWithFile call parameter one caller level higher")
        print(shlex.join(factory_plan_command(reverse_tsv)))
        print(shlex.join(factory_subtree_export_command))
        print(shlex.join(factory_report_command))
        return 0

    required = [
        ghidra_root / "support" / "analyzeHeadless",
        forward_exporter,
        reverse_exporter,
        caller_subtree_exporter,
        analyzer,
        caller_arguments,
    ]
    for path in required:
        if not path.is_file():
            print(f"missing required tool: {path}", file=sys.stderr)
            return 2
    if not project_dir.is_dir():
        print(f"missing Ghidra project directory: {project_dir}", file=sys.stderr)
        return 2

    output_dir.mkdir(parents=True, exist_ok=True)

    def run_caller_subtree_phase(pcode_tsv: Path) -> int:
        commands = [
            caller_plan_command(pcode_tsv),
            caller_subtree_export_command,
            caller_report_command,
            factory_plan_command(pcode_tsv),
            factory_subtree_export_command,
            factory_report_command,
        ]
        for command in commands:
            code = run(command)
            if code != 0:
                print(
                    f"command failed with exit code {code}: {shlex.join(command)}",
                    file=sys.stderr,
                )
                return code or 1
        return 0

    def print_subtree_outputs() -> None:
        print(f"EnemyActions init caller argument plan: {caller_plan}")
        print(f"EnemyActions init caller argument subtrees: {caller_subtrees}")
        print(f"EnemyActions init caller argument report: {caller_report}")
        print(f"EnemyActions createWithFile caller parameter plan: {factory_plan}")
        print(f"EnemyActions createWithFile caller parameter subtrees: {factory_subtrees}")
        print(f"EnemyActions createWithFile caller parameter report: {factory_report}")

    code = run(forward_export_command)
    if code != 0:
        print(
            f"command failed with exit code {code}: {shlex.join(forward_export_command)}",
            file=sys.stderr,
        )
        return code or 1

    code = run(forward_analyze_command)
    if code == 0:
        code = run_caller_subtree_phase(forward_tsv)
        if code != 0:
            return code
        print(f"Bounded forward call-graph P-code: {forward_tsv}")
        print(f"Enemy action-init evidence: {report}")
        print_subtree_outputs()
        return 0
    if code != 2:
        print(
            f"command failed with exit code {code}: {shlex.join(forward_analyze_command)}",
            file=sys.stderr,
        )
        return code or 1

    print(
        "Forward graph contains no resolved EnemyActionsSystem::initWithFile call; "
        f"falling back to bounded reverse callers from {reverse_target}.",
        file=sys.stderr,
    )
    code = run(reverse_export_command)
    if code != 0:
        print(
            f"command failed with exit code {code}: {shlex.join(reverse_export_command)}",
            file=sys.stderr,
        )
        return code or 1

    code = run(reverse_analyze_command)
    if code != 0:
        print(
            f"command failed with exit code {code}: {shlex.join(reverse_analyze_command)}",
            file=sys.stderr,
        )
        return code or 1

    code = run_caller_subtree_phase(reverse_tsv)
    if code != 0:
        return code

    print(f"Bounded forward call-graph P-code: {forward_tsv}")
    print(f"Bounded reverse caller P-code: {reverse_tsv}")
    print(f"Enemy action-init evidence: {report}")
    print_subtree_outputs()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
