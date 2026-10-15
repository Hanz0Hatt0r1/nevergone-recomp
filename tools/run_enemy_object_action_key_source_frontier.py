#!/usr/bin/env python3
"""Build the key-lifetime plan and trace predecessor source frontiers."""

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
    parser.add_argument(
        "--pcode",
        type=Path,
        default=default_output_dir / "enemy-object-init-actions-system-pcode.tsv",
    )
    parser.add_argument("--output-dir", type=Path, default=default_output_dir)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    pcode = args.pcode.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()
    lifetime_tool = repo_root / "tools" / "enemy_object_action_key_lifetime.py"
    frontier_tool = repo_root / "tools" / "enemy_object_action_key_source_frontier.py"
    plan = output_dir / "enemy-object-action-key-lifetime-plan.tsv"
    report = output_dir / "enemy-object-action-key-source-frontier-report.txt"

    commands = [
        [sys.executable, str(lifetime_tool), "plan", str(pcode), str(plan)],
        [sys.executable, str(frontier_tool), str(pcode), str(plan), "--output", str(report)],
    ]
    if args.dry_run:
        for command in commands:
            print(shlex.join(command))
        return 0

    for path in (pcode, lifetime_tool, frontier_tool):
        if not path.is_file():
            print(f"missing required file: {path}", file=sys.stderr)
            if path == pcode:
                print(
                    "run tools/run_enemy_object_init_actions_callsite.py first to create the focused P-code TSV",
                    file=sys.stderr,
                )
            return 2

    output_dir.mkdir(parents=True, exist_ok=True)
    for command in commands:
        code = run(command)
        if code != 0:
            print(f"command failed with exit code {code}: {shlex.join(command)}", file=sys.stderr)
            return code or 1

    print(f"EnemyObject action key source-frontier report: {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
