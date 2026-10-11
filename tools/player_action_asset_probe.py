#!/usr/bin/env python3
"""Resolve one Never Gone player action resource from user-owned local assets.

The default probe is intentionally bounded: it reads only the selected
pNN_actions.plist, returns one action dictionary's `name` field, and reports
matching .actData paths. With --validator it additionally passes exactly one
matched action-data payload to the project-owned WBG validator and reports only
bounded parser metadata; proprietary payload bytes are never emitted.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import plistlib
import re
import subprocess
import sys
import tempfile
import zipfile

MAX_PLIST_BYTES = 8 * 1024 * 1024
MAX_ACTION_DATA_BYTES = 64 * 1024 * 1024
MAX_MATCHES = 32
VALIDATOR_TIMEOUT_SECONDS = 30
_ACTION_KEY_RE = re.compile(r"^action([0-9]+)$")
_VALIDATOR_STRING_FIELDS = {
    "parse_status",
    "eof_policy",
    "primary_record0_frame_key_hex",
}
_VALIDATOR_INTEGER_FIELDS = {
    "input_bytes",
    "bytes_consumed",
    "trailing_bytes",
    "primary_records",
    "primary_record0_frame_key_length",
}


def action_plist_name(career: int) -> str:
    if career < 1 or career > 99:
        raise ValueError("career must be in 1..99")
    return f"p{career:02d}_actions.plist"


def validate_action_key(action_key: str) -> str:
    match = _ACTION_KEY_RE.fullmatch(action_key)
    if match is None or int(match.group(1)) < 1:
        raise ValueError("action key must look like action28")
    return action_key


def _resolve_name(payload: bytes, action_key: str) -> str:
    if len(payload) > MAX_PLIST_BYTES:
        raise ValueError(f"plist exceeds {MAX_PLIST_BYTES} byte probe bound")
    root = plistlib.loads(payload)
    if not isinstance(root, dict):
        raise ValueError("actions plist root is not a dictionary")
    entry = root.get(action_key)
    if not isinstance(entry, dict):
        raise KeyError(f"{action_key} not found as a dictionary entry")
    name = entry.get("name")
    if not isinstance(name, str) or not name:
        raise ValueError(f"{action_key}.name is missing or not a non-empty string")
    return name


def _matching_actdata_paths(paths: list[str], action_name: str) -> list[str]:
    wanted = PurePosixPath(action_name.replace("\\", "/")).name.casefold()
    if not wanted:
        return []
    matches = []
    for path in paths:
        normalized = path.replace("\\", "/")
        if PurePosixPath(normalized).name.casefold() == wanted:
            matches.append(normalized)
            if len(matches) >= MAX_MATCHES:
                break
    return matches


def _parse_validator_output(text: str) -> dict[str, object]:
    result: dict[str, object] = {}
    for raw_line in text.splitlines():
        if "=" not in raw_line:
            continue
        key, value = raw_line.split("=", 1)
        if key in _VALIDATOR_STRING_FIELDS:
            result[key] = value
        elif key in _VALIDATOR_INTEGER_FIELDS:
            try:
                result[key] = int(value)
            except ValueError:
                raise ValueError(f"validator returned non-integer {key}") from None
    return result


def _run_validator(validator: Path, action_data_path: Path) -> dict[str, object]:
    validator = validator.resolve()
    if not validator.is_file():
        raise FileNotFoundError(f"validator not found: {validator}")
    try:
        completed = subprocess.run(
            [str(validator), str(action_data_path)],
            capture_output=True,
            text=True,
            check=False,
            timeout=VALIDATOR_TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired as exc:
        raise ValueError("validator timed out") from exc

    parsed = _parse_validator_output(completed.stdout)
    parsed["validator_exit"] = completed.returncode
    if completed.returncode != 0 or parsed.get("parse_status") != "ok":
        raise ValueError(f"action-data validator failed with exit {completed.returncode}")
    return parsed


def _validate_directory_match(root: Path, relative_path: str, validator: Path) -> dict[str, object]:
    root = root.resolve()
    candidate = (root / Path(relative_path)).resolve()
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise ValueError("matched action-data path escapes asset root") from exc
    if not candidate.is_file():
        raise FileNotFoundError(f"matched action-data file not found: {relative_path}")
    size = candidate.stat().st_size
    if size > MAX_ACTION_DATA_BYTES:
        raise ValueError(f"action-data exceeds {MAX_ACTION_DATA_BYTES} byte validation bound")
    return _run_validator(validator, candidate)


def _validate_zip_match(archive_path: Path, relative_path: str, validator: Path) -> dict[str, object]:
    with zipfile.ZipFile(archive_path) as archive:
        info = archive.getinfo(relative_path)
        if info.file_size > MAX_ACTION_DATA_BYTES:
            raise ValueError(f"action-data exceeds {MAX_ACTION_DATA_BYTES} byte validation bound")
        payload = archive.read(info)

    with tempfile.TemporaryDirectory(prefix="nevergone-action-probe-") as temp:
        candidate = Path(temp) / "selected.actData"
        candidate.write_bytes(payload)
        return _run_validator(validator, candidate)


def _attach_validation(
    input_path: Path,
    result: dict[str, object],
    validator: Path,
) -> None:
    matches = result.get("matching_actdata_paths")
    if not isinstance(matches, list) or len(matches) != 1 or not isinstance(matches[0], str):
        count = len(matches) if isinstance(matches, list) else 0
        raise ValueError(f"--validator requires exactly one matching action-data path, found {count}")

    if result.get("container") == "directory":
        validation = _validate_directory_match(input_path, matches[0], validator)
    elif result.get("container") == "zip":
        validation = _validate_zip_match(input_path, matches[0], validator)
    else:
        raise ValueError("unsupported probe container for validation")
    result["actdata_validation"] = validation


def probe_directory(root: Path, career: int, action_key: str) -> dict[str, object]:
    plist_name = action_plist_name(career)
    candidates = sorted(
        path for path in root.rglob("*")
        if path.is_file() and path.name.casefold() == plist_name.casefold()
    )
    if not candidates:
        raise FileNotFoundError(f"{plist_name} not found under {root}")
    if len(candidates) > 1:
        raise ValueError(f"ambiguous {plist_name}: found {len(candidates)} copies")

    plist_path = candidates[0]
    size = plist_path.stat().st_size
    if size > MAX_PLIST_BYTES:
        raise ValueError(f"plist exceeds {MAX_PLIST_BYTES} byte probe bound")
    action_name = _resolve_name(plist_path.read_bytes(), action_key)

    all_files = [str(path.relative_to(root)).replace("\\", "/") for path in root.rglob("*") if path.is_file()]
    return {
        "container": "directory",
        "career": career,
        "actions_plist": str(plist_path.relative_to(root)).replace("\\", "/"),
        "action_key": action_key,
        "action_name": action_name,
        "matching_actdata_paths": _matching_actdata_paths(all_files, action_name),
    }


def probe_zip(path: Path, career: int, action_key: str) -> dict[str, object]:
    plist_name = action_plist_name(career)
    with zipfile.ZipFile(path) as archive:
        names = [info.filename for info in archive.infolist() if not info.is_dir()]
        candidates = [name for name in names if PurePosixPath(name).name.casefold() == plist_name.casefold()]
        if not candidates:
            raise FileNotFoundError(f"{plist_name} not found in {path}")
        if len(candidates) > 1:
            raise ValueError(f"ambiguous {plist_name}: found {len(candidates)} copies")
        info = archive.getinfo(candidates[0])
        if info.file_size > MAX_PLIST_BYTES:
            raise ValueError(f"plist exceeds {MAX_PLIST_BYTES} byte probe bound")
        payload = archive.read(info)
        action_name = _resolve_name(payload, action_key)

    return {
        "container": "zip",
        "career": career,
        "actions_plist": candidates[0],
        "action_key": action_key,
        "action_name": action_name,
        "matching_actdata_paths": _matching_actdata_paths(names, action_name),
    }


def probe(
    path: Path,
    career: int,
    action_key: str,
    validator: Path | None = None,
) -> dict[str, object]:
    action_key = validate_action_key(action_key)
    if path.is_dir():
        result = probe_directory(path, career, action_key)
    elif path.is_file() and zipfile.is_zipfile(path):
        result = probe_zip(path, career, action_key)
    else:
        raise ValueError("input must be an extracted asset directory or a ZIP-compatible OBB")

    if validator is not None:
        _attach_validation(path, result, validator)
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="extracted asset directory or ZIP-compatible OBB")
    parser.add_argument("--career", type=int, required=True, help="one-based player Career index")
    parser.add_argument("--action", default="action28", help="one-based plist action key (default: action28)")
    parser.add_argument(
        "--validator",
        type=Path,
        help="optional compiled enemy_actions_wbg_validate binary for the unique matched action-data file",
    )
    parser.add_argument("--output", type=Path, help="optional JSON report path")
    args = parser.parse_args(argv)

    try:
        result = probe(args.input, args.career, args.action, args.validator)
    except (OSError, ValueError, KeyError, plistlib.InvalidFileException, zipfile.BadZipFile) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    text = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
