#!/usr/bin/env python3
"""Build a scalable receiver/type export plan from a Nevergone dataset trace.

The plan contains only source identities and entries. It deliberately leaves the
expected CALLIND count unset so the existing Ghidra receiver exporter can discover
all indirect calls for every selected function instead of validating against the
small hand-curated seed frontier.
"""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

FORMAT = "NEVERGONE.MassReceiverPlan/1"


def read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="dataset trace.jsonl")
    parser.add_argument("output", type=Path, help="output TSV plan")
    parser.add_argument(
        "--program",
        default="libcocos2dcpp.so",
        help="program to select from the trace (default: libcocos2dcpp.so)",
    )
    args = parser.parse_args()

    selected: dict[str, str] = {}
    programs = Counter()
    trace_rows = 0
    for row in read_jsonl(args.trace):
        trace_rows += 1
        program = row.get("program")
        entry = row.get("source_entry")
        if isinstance(program, str) and program:
            programs[program] += 1
        if program != args.program:
            continue
        if not isinstance(entry, str) or not entry:
            continue
        source_id = f"{program}:{entry}"
        previous = selected.get(source_id)
        if previous is not None and previous != entry:
            raise ValueError(f"conflicting entry for {source_id}")
        selected[source_id] = entry

    if not selected:
        raise SystemExit(f"no trace functions found for program {args.program}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(["source_id", "source_entry", "expected_calls"])
        for source_id, entry in sorted(selected.items(), key=lambda item: int(item[1], 16)):
            writer.writerow([source_id, entry, ""])

    print("format:", FORMAT)
    print("trace rows:", trace_rows)
    print("trace programs:", dict(sorted(programs.items())))
    print("selected program:", args.program)
    print("plan functions:", len(selected))
    print("expected CALLIND counts: discover")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
