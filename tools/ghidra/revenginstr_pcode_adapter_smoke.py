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
    shared_repo = repo_root.parent / "revenginstr"
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
        for shared_path, expected in (
            (shared_repo, f"{shared_repo}/ghidra;{repo_root}/tools/ghidra"),
            (missing_shared, f"{repo_root}/tools/ghidra"),
        ):
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

        helper = repo_root / "tools/ghidra/revenginstr_pcode_flow_script_path.sh"
        expression = 'source "$1"; revenginstr_pcode_flow_script_path "$2"'
        for shared_path, expected in (
            (shared_repo, f"{shared_repo}/ghidra;{repo_root}/tools/ghidra"),
            (missing_shared, f"{repo_root}/tools/ghidra"),
        ):
            environment = os.environ.copy()
            environment["REVENGINSTR_REPO"] = str(shared_path)
            result = subprocess.run(
                ["bash", "-c", expression, "smoke", str(helper), str(repo_root)],
                check=True,
                capture_output=True,
                text=True,
                env=environment,
            )
            if result.stdout != expected:
                raise AssertionError(f"shell helper expected {expected!r}, got {result.stdout!r}")

    print("revenginstr P-code adapters shared-first and fallback: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
