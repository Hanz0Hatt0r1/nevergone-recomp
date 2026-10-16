#!/usr/bin/env python3
"""Select a deterministic truly-unseen internal corpus for Nevergone ML tests.

The selector operates on exported corpus metadata only. It removes external/import
and runtime-scaffold rows before model evaluation, excludes every identity already
present in a training dataset directory, and then uses a stable hash ranking so the
selected sample is not just one contiguous address range.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

from nevergone_function_origin import classify_function_origin

CORPUS_FORMAT = "NEVERGONE.MlPcodeCorpus/2"
GENERIC_CORPUS_FORMAT = "REVENGINSTR.PcodeCorpus/1"
SUPPORTED_CORPUS_FORMATS = frozenset((CORPUS_FORMAT, GENERIC_CORPUS_FORMAT))
SPLITS = ("train", "validation", "test")


def read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def stable_id(program: str, entry: str) -> str:
    return hashlib.sha256(f"{program}\0{entry}".encode("utf-8")).hexdigest()[:24]


def selection_key(program: str, entry: str) -> str:
    return hashlib.sha256(f"external-test\0{program}\0{entry}".encode("utf-8")).hexdigest()


def load_training_ids(dataset_dir: Path) -> set[str]:
    ids: set[str] = set()
    for split in SPLITS:
        path = dataset_dir / f"{split}.jsonl"
        if not path.is_file():
            raise ValueError(f"missing dataset split: {path}")
        for row in read_jsonl(path):
            value = row.get("id")
            if not isinstance(value, str) or not value:
                raise ValueError(f"{path}: dataset row missing id")
            ids.add(value)
    return ids


def select_rows(corpus: Path, dataset_dir: Path, limit: int) -> tuple[list[dict[str, Any]], Counter]:
    training_ids = load_training_ids(dataset_dir)
    stats: Counter = Counter()
    candidates: list[tuple[str, dict[str, Any]]] = []
    seen_identities: set[tuple[str, str]] = set()

    for row in read_jsonl(corpus):
        stats["input_rows"] += 1
        row_format = row.get("format")
        if not isinstance(row_format, str) or row_format not in SUPPORTED_CORPUS_FORMATS:
            raise ValueError(f"unsupported corpus format: {row.get('format')!r}")
        stats[f"format_{row_format}"] += 1
        if row.get("decompile_status") != "ok":
            stats["decompile_failed"] += 1
            continue

        program = row.get("program")
        entry = row.get("source_entry")
        name = row.get("source_name")
        if not isinstance(program, str) or not isinstance(entry, str):
            stats["invalid_identity"] += 1
            continue

        origin = classify_function_origin(name if isinstance(name, str) else None)
        stats[f"origin_{origin}"] += 1
        if origin != "internal":
            continue

        identity = (program, entry)
        if identity in seen_identities:
            stats["duplicate_identity"] += 1
            continue
        seen_identities.add(identity)

        record_id = stable_id(program, entry)
        if record_id in training_ids:
            stats["training_overlap"] += 1
            continue

        candidates.append((selection_key(program, entry), row))

    candidates.sort(key=lambda item: item[0])
    stats["eligible_unseen_internal"] = len(candidates)
    selected = [row for _, row in candidates[:limit]]
    stats["selected"] = len(selected)
    return selected, stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("training_dataset", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--limit", type=int, default=500)
    parser.add_argument(
        "--allow-short",
        action="store_true",
        help="write fewer than --limit rows instead of failing when the candidate pool is too small",
    )
    args = parser.parse_args()

    if args.limit < 1:
        parser.error("--limit must be >= 1")

    selected, stats = select_rows(args.corpus, args.training_dataset, args.limit)
    if len(selected) < args.limit and not args.allow_short:
        raise SystemExit(
            f"only {len(selected)} unseen internal functions are available; "
            f"need {args.limit}. Export a larger corpus or pass --allow-short."
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in selected:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    print(f"input rows: {stats['input_rows']}")
    print(
        "origins:",
        {
            "internal": stats["origin_internal"],
            "external_import": stats["origin_external_import"],
            "runtime_scaffold": stats["origin_runtime_scaffold"],
        },
    )
    print(f"training overlap removed: {stats['training_overlap']}")
    print(f"eligible unseen internal: {stats['eligible_unseen_internal']}")
    print(f"selected: {stats['selected']}")
    print(f"output: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
