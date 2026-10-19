#!/usr/bin/env python3
"""Merge tracked manual Nevergone labels into a local aggregate safely."""
from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path
from typing import Any, Iterable


def read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def name_key(name: str) -> str:
    return name.split("(", 1)[0].strip()


def load_corpus(path: Path):
    by_entry: dict[tuple[str, str], dict[str, Any]] = {}
    by_name: dict[tuple[str, str], list[dict[str, Any]]] = defaultdict(list)
    for row in read_jsonl(path):
        if row.get("decompile_status") != "ok":
            continue
        program, entry, name = (row.get(key) for key in ("program", "source_entry", "source_name"))
        if not all(isinstance(value, str) and value for value in (program, entry, name)):
            continue
        by_entry[(program, entry)] = row
        by_name[(program, name_key(name))].append(row)
    return by_entry, by_name


def resolve_label(row: dict[str, Any], by_entry, by_name) -> tuple[dict[str, Any] | None, str]:
    program = row.get("program")
    name = row.get("source_name")
    if not isinstance(program, str) or not program or not isinstance(name, str) or not name:
        return None, "missing_identity"

    entry = row.get("source_entry")
    candidate = by_entry.get((program, entry)) if isinstance(entry, str) else None
    if candidate is not None and name_key(candidate["source_name"]) == name_key(name):
        return candidate, "entry"

    matches = by_name.get((program, name_key(name)), [])
    if len(matches) == 1:
        return matches[0], "unique_name"
    return None, "ambiguous_name" if matches else "unmatched_name"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("base_labels", type=Path)
    parser.add_argument("manual_labels", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    by_entry, by_name = load_corpus(args.corpus)
    base_rows = list(read_jsonl(args.base_labels))
    base_labels: dict[tuple[str, str, str], str] = {}
    for row in base_rows:
        key = (str(row.get("program", "")), str(row.get("source_entry", "")), str(row.get("task", "")))
        label = row.get("label")
        previous = base_labels.get(key)
        if previous is not None and previous != label:
            raise ValueError(f"base labels conflict for {key}: {previous!r} vs {label!r}")
        base_labels[key] = label

    supplemental: dict[tuple[str, str, str], list[dict[str, Any]]] = defaultdict(list)
    counts: dict[str, int] = defaultdict(int)
    for path in sorted(args.manual_labels.glob("*labels.jsonl")):
        for row in read_jsonl(path):
            counts["input_rows"] += 1
            target, method = resolve_label(row, by_entry, by_name)
            counts[f"resolved_by_{method}"] += 1
            if target is None:
                continue
            normalized = dict(row)
            normalized["source_entry"] = target["source_entry"]
            normalized["source_name"] = target["source_name"]
            key = (target["program"], target["source_entry"], str(row.get("task", "")))
            supplemental[key].append(normalized)

    additions: list[dict[str, Any]] = []
    conflicts: list[dict[str, Any]] = []
    for key, rows in sorted(supplemental.items()):
        labels = {row.get("label") for row in rows}
        current = base_labels.get(key)
        if current is not None:
            if labels != {current}:
                conflicts.append({"key": key, "base": current, "manual": sorted(str(x) for x in labels)})
            else:
                counts["already_in_base"] += len(rows)
            continue
        if len(labels) != 1:
            conflicts.append({"key": key, "base": None, "manual": sorted(str(x) for x in labels)})
            continue
        additions.append(rows[0])
        counts["added_rows"] += 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in base_rows + additions:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    print(json.dumps({
        "base_rows": len(base_rows),
        "manual_rows": dict(sorted(counts.items())),
        "conflicts_skipped": conflicts,
        "output": str(args.output),
    }, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
