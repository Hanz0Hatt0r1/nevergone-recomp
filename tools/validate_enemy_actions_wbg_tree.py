#!/usr/bin/env python3

import argparse
import json
import subprocess
import sys
from pathlib import Path


SUPPORTED_SUFFIXES = {".wbg", ".actdata"}


def parse_key_values(text: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw_line in text.splitlines():
        if "=" not in raw_line:
            continue
        key, value = raw_line.split("=", 1)
        if key:
            values[key] = value
    return values


def find_action_data_files(root: Path) -> list[Path]:
    return sorted(
        (
            path
            for path in root.rglob("*")
            if path.is_file() and path.suffix.lower() in SUPPORTED_SUFFIXES
        ),
        key=lambda path: path.relative_to(root).as_posix().lower(),
    )


def validate_one(validator: Path, root: Path, path: Path, require_eof: bool) -> tuple[dict[str, object], bool]:
    command = [str(validator)]
    if require_eof:
        command.append("--require-eof")
    command.append(str(path))

    completed = subprocess.run(command, capture_output=True, text=True, check=False)
    values = parse_key_values(completed.stdout)
    if not values:
        values = parse_key_values(completed.stderr)

    result: dict[str, object] = {
        "relative_path": path.relative_to(root).as_posix(),
        "validator_exit": completed.returncode,
    }
    for key in (
        "parse_status",
        "input_bytes",
        "bytes_consumed",
        "trailing_bytes",
        "header_word0",
        "primary_records",
        "compact_records",
        "nested_groups",
        "nested_records",
        "versioned_groups",
        "versioned_records",
        "fixed_tail_records",
        "combo_tuples",
        "combo_array_94",
        "combo_array_98",
        "combo_array_9c",
        "final_table_entries",
        "eof_policy",
    ):
        if key in values:
            value: object = values[key]
            if key not in {"parse_status", "eof_policy"}:
                try:
                    value = int(values[key])
                except ValueError:
                    pass
            result[key] = value

    return result, completed.returncode == 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Recursively validate user-owned EnemyActions action-data files "
            "(.actData and legacy/research .wbg names) without printing payload contents."
        )
    )
    parser.add_argument("root", type=Path, help="Imported asset root to scan recursively")
    parser.add_argument(
        "--validator",
        type=Path,
        required=True,
        help="Path to the compiled enemy_actions_wbg_validate binary",
    )
    parser.add_argument(
        "--require-eof",
        action="store_true",
        help="Pass the validator's experimental strict-EOF policy to every file",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    validator = args.validator.resolve()
    if not root.is_dir():
        print(f"error: asset root is not a directory: {root}", file=sys.stderr)
        return 1
    if not validator.is_file():
        print(f"error: validator is not a file: {validator}", file=sys.stderr)
        return 1

    files = find_action_data_files(root)
    if not files:
        print("summary files=0 passed=0 failed=0", file=sys.stderr)
        return 4

    passed = 0
    failed = 0
    for path in files:
        result, ok = validate_one(validator, root, path, args.require_eof)
        print(json.dumps(result, sort_keys=True, separators=(",", ":")))
        if ok:
            passed += 1
        else:
            failed += 1

    print(f"summary files={len(files)} passed={passed} failed={failed}", file=sys.stderr)
    return 0 if failed == 0 else 5


if __name__ == "__main__":
    raise SystemExit(main())
