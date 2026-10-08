#!/usr/bin/env python3
"""Fast queries over the checked-in Nevergone Ghidra/archive metadata index.

The index contains metadata only. This tool never reads the original APK, OBB,
shared objects, Ghidra project databases, decompiler output, or asset contents.
"""

from __future__ import annotations

import argparse
import base64
import csv
import gzip
from pathlib import Path
from typing import Iterable, Iterator

DEFAULT_ROOT = Path("docs/ghidra/full-index")
PROGRAMS = ("libcocos2dcpp", "libffmpeg")
TABLE_SUFFIXES = {
    "functions": "functions.tsv.gz",
    "symbols": "symbols.tsv.gz",
    "strings": "strings.tsv.gz",
    "xrefs": "xrefs.tsv.gz",
    "calls": "calls.tsv.gz",
}
ARCHIVE_TABLES = {
    "assets": "archive_entries.tsv.gz",
    "atlases": "atlas_frames.tsv.gz",
}


def open_tsv(path: Path) -> Iterator[dict[str, str]]:
    with gzip.open(path, "rt", encoding="utf-8", newline="") as handle:
        yield from csv.DictReader(handle, delimiter="\t")


def decode_string(row: dict[str, str]) -> str:
    encoded = row.get("utf8_base64", "")
    try:
        return base64.b64decode(encoded, validate=True).decode("utf-8")
    except (ValueError, UnicodeDecodeError):
        return "<invalid-base64>"


def row_text(kind: str, row: dict[str, str]) -> str:
    if kind == "strings":
        return "\t".join((row.get("address", ""), decode_string(row), row.get("length", "")))
    return "\t".join(row.get(key, "") for key in row)


def matches(kind: str, row: dict[str, str], needle: str, exact: bool) -> bool:
    candidates = list(row.values())
    if kind == "strings":
        candidates.append(decode_string(row))
    if exact:
        return any(value == needle for value in candidates)
    folded = needle.casefold()
    return any(folded in value.casefold() for value in candidates)


def search_rows(
    rows: Iterable[dict[str, str]],
    *,
    kind: str,
    needle: str,
    exact: bool,
    limit: int,
) -> list[dict[str, str]]:
    found: list[dict[str, str]] = []
    for row in rows:
        if matches(kind, row, needle, exact):
            found.append(row)
            if len(found) >= limit:
                break
    return found


def normalize_address(value: str) -> str:
    value = value.strip().lower()
    if value.startswith("0x"):
        value = value[2:]
    if not value:
        raise ValueError("empty address")
    number = int(value, 16)
    return f"{number:08x}"


def resolve_function(root: Path, program: str, token: str) -> tuple[str, str]:
    path = root / f"{program}.functions.tsv.gz"
    try:
        normalized = normalize_address(token)
    except ValueError:
        normalized = None

    candidates: list[tuple[str, str]] = []
    for row in open_tsv(path):
        entry = row.get("entry", "").lower().removeprefix("0x")
        name = row.get("name", "")
        if normalized is not None and entry == normalized:
            return row["entry"], name
        if token.casefold() in name.casefold():
            candidates.append((row["entry"], name))
            if len(candidates) > 1:
                break
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        raise SystemExit(f"no function matches {token!r}")
    raise SystemExit(f"function query {token!r} is ambiguous; use an entry address")


def call_edges(root: Path, program: str, token: str, callers: bool, limit: int) -> int:
    entry, name = resolve_function(root, program, token)
    entry_norm = entry.lower().removeprefix("0x")
    functions = {
        row.get("entry", "").lower().removeprefix("0x"): row.get("name", "")
        for row in open_tsv(root / f"{program}.functions.tsv.gz")
    }
    key = "callee_entry" if callers else "caller_entry"
    other = "caller_entry" if callers else "callee_entry"
    label = "callers" if callers else "callees"
    print(f"# {label} of {entry}\t{name}")
    count = 0
    for row in open_tsv(root / f"{program}.calls.tsv.gz"):
        if row.get(key, "").lower().removeprefix("0x") != entry_norm:
            continue
        other_entry = row.get(other, "")
        other_name = functions.get(other_entry.lower().removeprefix("0x"), "<unknown>")
        print(f"{other_entry}\t{other_name}\tcall_site={row.get('call_site', '')}")
        count += 1
        if count >= limit:
            break
    return count


def table_path(root: Path, program: str, kind: str) -> Path:
    if kind in ARCHIVE_TABLES:
        return root / ARCHIVE_TABLES[kind]
    return root / f"{program}.{TABLE_SUFFIXES[kind]}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=tuple(TABLE_SUFFIXES) + tuple(ARCHIVE_TABLES) + ("callers", "callees"))
    parser.add_argument("query", help="case-insensitive substring, exact address, or function name")
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--program", choices=PROGRAMS, default="libcocos2dcpp")
    parser.add_argument("--exact", action="store_true", help="require an exact field match")
    parser.add_argument("--limit", type=int, default=50)
    args = parser.parse_args()

    if args.limit < 1:
        parser.error("--limit must be >= 1")

    if args.kind in ("callers", "callees"):
        count = call_edges(args.root, args.program, args.query, args.kind == "callers", args.limit)
        return 0 if count else 1

    path = table_path(args.root, args.program, args.kind)
    if not path.is_file():
        raise SystemExit(f"missing index table: {path}")
    rows = search_rows(open_tsv(path), kind=args.kind, needle=args.query, exact=args.exact, limit=args.limit)
    for row in rows:
        print(row_text(args.kind, row))
    return 0 if rows else 1


if __name__ == "__main__":
    raise SystemExit(main())
