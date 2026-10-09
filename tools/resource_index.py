#!/usr/bin/env python3
"""Build a deterministic metadata index for a user-owned extracted APK/OBB tree.

The tool never copies file contents. Full path inventories are intended to stay local;
use --path-mode hash or --summary-only when sharing generated metadata.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from collections import Counter
from pathlib import Path
from typing import Iterable

SCHEMA_VERSION = 1

_SUBSYSTEMS = {
    "gamescene": "scene",
    "player": "player",
    "enemy": "combat",
    "npc": "combat",
    "weapons": "equipment",
    "equips": "equipment",
    "sound": "audio",
    "shader": "rendering",
    "mat": "rendering",
    "config": "config",
    "dialogueconfig": "config",
    "guide": "ui",
    "script": "script",
    "pets": "gameplay",
    "other": "other",
}


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _relative_files(root: Path) -> Iterable[Path]:
    files = (path for path in root.rglob("*") if path.is_file())
    return sorted(files, key=lambda path: path.relative_to(root).as_posix())


def _subsystem(relative_path: Path) -> str:
    if len(relative_path.parts) <= 1:
        return "root"
    top_level = relative_path.parts[0].lower()
    return _SUBSYSTEMS.get(top_level, top_level)


def _display_path(relative_path: Path, mode: str) -> str:
    value = relative_path.as_posix()
    if mode == "plain":
        return value
    if mode == "hash":
        return hashlib.sha256(value.encode("utf-8")).hexdigest()
    raise ValueError(f"unsupported path mode: {mode}")


def build_index(root: Path, *, path_mode: str = "plain", content_hash: bool = True) -> dict:
    root = root.resolve()
    if not root.is_dir():
        raise ValueError(f"resource root is not a directory: {root}")

    entries: list[dict] = []
    extension_counts: Counter[str] = Counter()
    subsystem_counts: Counter[str] = Counter()
    total_bytes = 0

    for path in _relative_files(root):
        relative = path.relative_to(root)
        size = path.stat().st_size
        extension = path.suffix.lower() or "<none>"
        subsystem = _subsystem(relative)
        entry = {
            "path": _display_path(relative, path_mode),
            "subsystem": subsystem,
            "extension": extension,
            "size": size,
            "sha256": _sha256_file(path) if content_hash else "",
        }
        entries.append(entry)
        total_bytes += size
        extension_counts[extension] += 1
        subsystem_counts[subsystem] += 1

    summary = {
        "files": len(entries),
        "bytes": total_bytes,
        "extensions": dict(sorted(extension_counts.items())),
        "subsystems": dict(sorted(subsystem_counts.items())),
    }
    return {
        "schema_version": SCHEMA_VERSION,
        "root_name": root.name,
        "path_mode": path_mode,
        "content_hash": "sha256" if content_hash else "none",
        "summary": summary,
        "entries": entries,
    }


def _write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _write_csv(path: Path, entries: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=("path", "subsystem", "extension", "size", "sha256"),
        )
        writer.writeheader()
        writer.writerows(entries)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Index metadata from a user-owned extracted Never Gone APK/OBB resource tree."
    )
    parser.add_argument("root", type=Path, help="extracted resource root (for example the local assets directory)")
    parser.add_argument(
        "--path-mode",
        choices=("plain", "hash"),
        default="plain",
        help="store relative paths verbatim or as SHA-256 path identifiers",
    )
    parser.add_argument(
        "--no-content-hash",
        action="store_true",
        help="skip SHA-256 file-content hashes for faster inventories",
    )
    parser.add_argument("--json", dest="json_path", type=Path, help="write the full index as JSON")
    parser.add_argument("--csv", dest="csv_path", type=Path, help="write entries as CSV")
    parser.add_argument(
        "--summary-only",
        action="store_true",
        help="print only aggregate counts; useful for shareable diagnostics",
    )
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    try:
        payload = build_index(
            args.root,
            path_mode=args.path_mode,
            content_hash=not args.no_content_hash,
        )
    except (OSError, ValueError) as exc:
        raise SystemExit(str(exc)) from exc

    if args.json_path:
        _write_json(args.json_path, payload)
    if args.csv_path:
        _write_csv(args.csv_path, payload["entries"])

    printed = payload["summary"] if args.summary_only else payload
    print(json.dumps(printed, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
