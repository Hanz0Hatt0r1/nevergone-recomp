#!/usr/bin/env python3
"""Prepare Nevergone-owned inputs for revenginstr frozen validation/drift parity.

This adapter keeps project policy in Nevergone:

- historical `NEVERGONE.MlPcodeCorpus/2` rows are adapted to the generic outer
  `REVENGINSTR.PcodeCorpus/1` contract; generic rows are passed through;
- the eligibility trace uses Nevergone's origin policy, including its narrow
  runtime-scaffold exclusion;
- optional historical routing OOF is reduced to `{id, action}` using the retained
  `legacy_action` field so generic drift tooling does not learn Nevergone version
  names.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

from nevergone_function_origin import classify_function_origin

SOURCE_CORPUS_FORMAT = "NEVERGONE.MlPcodeCorpus/2"
TARGET_CORPUS_FORMAT = "REVENGINSTR.PcodeCorpus/1"
SUPPORTED_SOURCE_CORPUS_FORMATS = frozenset((SOURCE_CORPUS_FORMAT, TARGET_CORPUS_FORMAT))


def read_jsonl(path: Path) -> Iterable[tuple[int, dict[str, Any]]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield line_no, row


def write_jsonl(path: Path, rows: Iterable[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def adapt_corpus(
    source: Path,
    generic_output: Path,
    eligibility_output: Path,
) -> dict[str, int]:
    generic_rows: list[dict[str, Any]] = []
    eligible_rows: list[dict[str, Any]] = []
    stats: Counter = Counter()
    seen_physical: set[tuple[str, str]] = set()

    for line_no, row in read_jsonl(source):
        stats["rows"] += 1
        row_format = row.get("format")
        if (
            not isinstance(row_format, str)
            or row_format not in SUPPORTED_SOURCE_CORPUS_FORMATS
        ):
            raise ValueError(
                f"{source}:{line_no}: expected one of {sorted(SUPPORTED_SOURCE_CORPUS_FORMATS)}, "
                f"got {row_format!r}"
            )
        stats[f"source_format_{row_format}"] += 1
        program = row.get("program")
        entry = row.get("source_entry")
        source_name = row.get("source_name")
        if not isinstance(program, str) or not program:
            raise ValueError(f"{source}:{line_no}: missing program")
        if not isinstance(entry, str) or not entry:
            raise ValueError(f"{source}:{line_no}: missing source_entry")
        if not isinstance(source_name, str) or not source_name:
            raise ValueError(f"{source}:{line_no}: missing source_name")
        physical = (program, entry)
        if physical in seen_physical:
            raise ValueError(f"{source}:{line_no}: duplicate physical identity {program}:{entry}")
        seen_physical.add(physical)

        generic = dict(row)
        generic["format"] = TARGET_CORPUS_FORMAT
        generic_rows.append(generic)

        if row.get("decompile_status") != "ok":
            stats["decompile_failed"] += 1
            continue
        if bool(row.get("external")):
            stats["external"] += 1
            continue
        origin = classify_function_origin(source_name)
        stats[f"origin_{origin}"] += 1
        if origin != "internal":
            continue
        eligible_rows.append(
            {
                "program": program,
                "source_entry": entry,
                "consumer_origin": origin,
            }
        )

    write_jsonl(generic_output, generic_rows)
    write_jsonl(eligibility_output, eligible_rows)
    stats["generic_rows"] = len(generic_rows)
    stats["eligible_rows"] = len(eligible_rows)
    return dict(stats)


def adapt_reference_actions(source: Path, output: Path) -> int:
    rows: list[dict[str, str]] = []
    seen: set[str] = set()
    for line_no, row in read_jsonl(source):
        record_id = row.get("id")
        action = row.get("legacy_action")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError(f"{source}:{line_no}: missing id")
        if record_id in seen:
            raise ValueError(f"{source}:{line_no}: duplicate id {record_id}")
        seen.add(record_id)
        if not isinstance(action, str) or action not in {"accept", "review", "escalate"}:
            raise ValueError(f"{source}:{line_no}: missing/invalid legacy_action")
        rows.append({"id": record_id, "action": action})
    write_jsonl(output, rows)
    return len(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("generic_corpus", type=Path)
    parser.add_argument("eligibility_trace", type=Path)
    parser.add_argument("--routing-oof", type=Path)
    parser.add_argument("--reference-actions-output", type=Path)
    args = parser.parse_args()

    if (args.routing_oof is None) != (args.reference_actions_output is None):
        parser.error("--routing-oof and --reference-actions-output must be supplied together")

    stats = adapt_corpus(args.corpus, args.generic_corpus, args.eligibility_trace)
    print("source formats:", sorted(SUPPORTED_SOURCE_CORPUS_FORMATS))
    print("target format:", TARGET_CORPUS_FORMAT)
    print("corpus rows:", stats["rows"])
    print("generic rows:", stats["generic_rows"])
    print("eligible rows:", stats["eligible_rows"])
    print("runtime scaffold removed:", stats.get("origin_runtime_scaffold", 0))
    print("external removed:", stats.get("external", 0) + stats.get("origin_external_import", 0))
    print("generic corpus:", args.generic_corpus)
    print("eligibility trace:", args.eligibility_trace)

    if args.routing_oof is not None and args.reference_actions_output is not None:
        count = adapt_reference_actions(args.routing_oof, args.reference_actions_output)
        print("reference action rows:", count)
        print("reference actions:", args.reference_actions_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
