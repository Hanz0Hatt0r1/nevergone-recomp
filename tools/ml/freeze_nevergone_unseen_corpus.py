#!/usr/bin/env python3
"""Freeze a deterministic identity-disjoint Nevergone validation corpus.

The input is a raw MlPcodeCorpus/2 export. The tool removes every identity already
present in a training dataset, keeps only internal functions, selects rows by the
existing stable hash ranking, assigns deployment-friendly `program:source_entry`
IDs, and writes immutable corpus/trace/manifest files with SHA-256 provenance.

The source/name metadata remain outer audit fields only; model input is still the
portable `model_text` representation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from select_nevergone_internal_test_corpus import load_training_ids, select_rows

FORMAT = "NEVERGONE.FrozenValidationCorpus/1"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("training_dataset", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--limit", type=int, default=500)
    parser.add_argument("--tag", default="unseen500-v10")
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args()

    if args.limit < 1:
        parser.error("--limit must be >= 1")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    corpus_out = args.output_dir / "corpus.jsonl"
    trace_out = args.output_dir / "trace.jsonl"
    manifest_out = args.output_dir / "manifest.json"
    if manifest_out.exists() and not args.overwrite:
        raise SystemExit(
            f"frozen manifest already exists: {manifest_out}; refuse to replace without --overwrite"
        )

    selected, stats = select_rows(args.corpus, args.training_dataset, args.limit)
    if len(selected) != args.limit:
        raise SystemExit(
            f"only {len(selected)} unseen internal functions available; need exactly {args.limit}"
        )

    indexed: list[dict[str, Any]] = []
    trace: list[dict[str, Any]] = []
    identities: set[str] = set()
    for row in selected:
        program = row.get("program")
        entry = row.get("source_entry")
        if not isinstance(program, str) or not program or not isinstance(entry, str) or not entry:
            raise ValueError("selected row missing program/source_entry")
        record_id = f"{program}:{entry}"
        if record_id in identities:
            raise ValueError(f"duplicate selected identity: {record_id}")
        identities.add(record_id)
        output = dict(row)
        output["id"] = record_id
        indexed.append(output)
        trace.append({"id": record_id, "program": program, "source_entry": entry})

    training_ids = load_training_ids(args.training_dataset)
    write_jsonl(corpus_out, indexed)
    write_jsonl(trace_out, trace)

    split_hashes = {}
    for split in ("train", "validation", "test"):
        path = args.training_dataset / f"{split}.jsonl"
        split_hashes[split] = sha256_file(path)

    manifest = {
        "format": FORMAT,
        "tag": args.tag,
        "selection": "stable_hash_internal_identity_disjoint",
        "limit": args.limit,
        "record_id_scheme": "program:source_entry",
        "training_identity_count": len(training_ids),
        "source_corpus": {
            "path": str(args.corpus),
            "sha256": sha256_file(args.corpus),
            "input_rows": int(stats["input_rows"]),
            "eligible_unseen_internal": int(stats["eligible_unseen_internal"]),
            "training_overlap_removed": int(stats["training_overlap"]),
        },
        "training_dataset": {
            "path": str(args.training_dataset),
            "split_sha256": split_hashes,
        },
        "frozen_corpus": {
            "path": str(corpus_out),
            "sha256": sha256_file(corpus_out),
            "rows": len(indexed),
        },
        "trace": {
            "path": str(trace_out),
            "sha256": sha256_file(trace_out),
            "rows": len(trace),
        },
    }
    manifest_out.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", FORMAT)
    print("training identities:", len(training_ids))
    print("source rows:", stats["input_rows"])
    print("training overlap removed:", stats["training_overlap"])
    print("eligible unseen internal:", stats["eligible_unseen_internal"])
    print("frozen rows:", len(indexed))
    print("corpus sha256:", manifest["frozen_corpus"]["sha256"])
    print("corpus:", corpus_out)
    print("trace:", trace_out)
    print("manifest:", manifest_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
