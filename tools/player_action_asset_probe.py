#!/usr/bin/env python3
"""Resolve one Never Gone player action resource from user-owned local assets.

The probe is intentionally bounded: it reads only the selected pNN_actions.plist,
returns one action dictionary's `name` field, and reports matching .actData paths.
It never copies proprietary resource payloads into the report.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import plistlib
import re
import sys
import zipfile

MAX_PLIST_BYTES = 8 * 1024 * 1024
MAX_MATCHES = 32
_ACTION_KEY_RE = re.compile(r"^action([0-9]+)$")


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


def probe(path: Path, career: int, action_key: str) -> dict[str, object]:
    action_key = validate_action_key(action_key)
    if path.is_dir():
        return probe_directory(path, career, action_key)
    if path.is_file() and zipfile.is_zipfile(path):
        return probe_zip(path, career, action_key)
    raise ValueError("input must be an extracted asset directory or a ZIP-compatible OBB")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="extracted asset directory or ZIP-compatible OBB")
    parser.add_argument("--career", type=int, required=True, help="one-based player Career index")
    parser.add_argument("--action", default="action28", help="one-based plist action key (default: action28)")
    parser.add_argument("--output", type=Path, help="optional JSON report path")
    args = parser.parse_args(argv)

    try:
        result = probe(args.input, args.career, args.action)
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
