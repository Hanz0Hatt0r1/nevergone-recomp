#!/usr/bin/env python3
"""Check that High P-code adapters prefer the shared exporter and keep fallback."""

from __future__ import annotations

import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]
    shared_repo = Path(
        os.environ.get("REVENGINSTR_REPO", repo_root.parent / "revenginstr")
    ).expanduser().resolve()
    runners = (
        "tools/run_enemy_object_init_actions_callsite.py",
        "tools/run_tutorial_scene_limit_probe.py",
        "tools/run_player_face_dir_call_probe.py",
    )
    arguments = (
        "--ghidra", "/opt/ghidra",
        "--project-dir", "/tmp/revenginstr-adapter-smoke-project",
        "--project-name", "smoke",
        "--program", "sample.bin",
        "--dry-run",
    )

    with tempfile.TemporaryDirectory(prefix="revenginstr-adapter-smoke-") as temp:
        missing_shared = Path(temp) / "missing-revenginstr"
        generic_shared = Path(temp) / "generic-revenginstr"
        (generic_shared / "ghidra").mkdir(parents=True)
        (generic_shared / "ghidra/ExportGhidraCallerArgumentSubtree.java").touch()
        for shared_path in (shared_repo, missing_shared, generic_shared):
            function_shared = (shared_path / "ghidra/ExportFunctionPcodeFlow.java").is_file()
            expected = (
                f"{shared_path}/ghidra;{repo_root}/tools/ghidra"
                if function_shared
                else f"{repo_root}/tools/ghidra"
            )
            environment = os.environ.copy()
            environment["REVENGINSTR_REPO"] = str(shared_path)
            for relative in runners:
                result = subprocess.run(
                    [sys.executable, str(repo_root / relative), *arguments],
                    check=False,
                    capture_output=True,
                    text=True,
                    env=environment,
                )
                if result.returncode:
                    raise RuntimeError(f"{relative}: {result.stderr}")
                commands = [
                    shlex.split(line)
                    for line in result.stdout.splitlines()
                    if "ExportFunctionPcodeFlow.java" in line
                ]
                if not commands:
                    raise AssertionError(f"{relative}: no P-code export command")
                for command in commands:
                    script_path = command[command.index("-scriptPath") + 1]
                    if script_path != expected:
                        raise AssertionError(
                            f"{relative}: expected {expected!r}, got {script_path!r}"
                        )

            result = subprocess.run(
                [
                    sys.executable,
                    str(repo_root / "tools/run_enemy_object_action_init_probe.py"),
                    "--project-dir", "/tmp/revenginstr-adapter-smoke-project",
                    "--project-name", "smoke",
                    "--program", "sample.bin",
                    "--dry-run",
                ],
                check=False,
                capture_output=True,
                text=True,
                env=environment,
            )
            if result.returncode:
                raise RuntimeError(f"bounded call-graph adapter: {result.stderr}")
            commands = [
                shlex.split(line)
                for line in result.stdout.splitlines()
                if "ExportBounded" in line and "PcodeFlow.java" in line
            ]
            if len(commands) != 2:
                raise AssertionError(f"expected both bounded exporters, got {len(commands)}")
            bounded_shared = (
                (shared_path / "ghidra/ExportBoundedCallGraphPcodeFlow.java").is_file()
                and (shared_path / "ghidra/ExportBoundedReverseCallGraphPcodeFlow.java").is_file()
            )
            caller_shared = (shared_path / "ghidra/ExportGhidraCallerArgumentSubtree.java").is_file()
            bounded_expected = (
                f"{shared_path}/ghidra;{repo_root}/tools/ghidra"
                if bounded_shared or caller_shared
                else f"{repo_root}/tools/ghidra"
            )
            for command in commands:
                script_path = command[command.index("-scriptPath") + 1]
                if script_path != bounded_expected:
                    raise AssertionError(
                        f"bounded call-graph adapter expected {bounded_expected!r}, got {script_path!r}"
                    )
                configured = any(value.startswith("--priority=") for value in command)
                if configured != bounded_shared:
                    raise AssertionError("bounded call-graph priorities must follow shared exporter availability")
            caller_commands = [
                shlex.split(line)
                for line in result.stdout.splitlines()
                if "ExportCallerArgumentSubtree.java" in line
            ]
            if len(caller_commands) != 2:
                raise AssertionError(f"expected both caller subtree exports, got {len(caller_commands)}")
            for command in caller_commands:
                script_path = command[command.index("-scriptPath") + 1]
                expected_path = bounded_expected
                if script_path != expected_path:
                    raise AssertionError(f"caller subtree expected script path {expected_path!r}, got {script_path!r}")
                shared_argument = command[-1] == f"{shared_path}/ghidra"
                if shared_argument != caller_shared:
                    raise AssertionError("caller subtree adapter must receive the shared script directory when available")

        helper = repo_root / "tools/ghidra/revenginstr_pcode_flow_script_path.sh"
        expression = 'source "$1"; "$2" "$3"'
        for shared_path in (shared_repo, missing_shared):
            environment = os.environ.copy()
            environment["REVENGINSTR_REPO"] = str(shared_path)
            for function, exporters in (
                ("revenginstr_pcode_flow_script_path", ("ExportFunctionPcodeFlow.java",)),
                ("revenginstr_bounded_callgraph_script_path", (
                    "ExportBoundedCallGraphPcodeFlow.java",
                    "ExportBoundedReverseCallGraphPcodeFlow.java",
                )),
            ):
                shared_available = all((shared_path / "ghidra" / name).is_file() for name in exporters)
                expected = (
                    f"{shared_path}/ghidra;{repo_root}/tools/ghidra"
                    if shared_available
                    else f"{repo_root}/tools/ghidra"
                )
                result = subprocess.run(
                    ["bash", "-c", expression, "smoke", str(helper), function, str(repo_root)],
                    check=True,
                    capture_output=True,
                    text=True,
                    env=environment,
                )
                if result.stdout != expected:
                    raise AssertionError(f"{function} expected {expected!r}, got {result.stdout!r}")

    print("revenginstr P-code adapters shared-first and fallback: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
