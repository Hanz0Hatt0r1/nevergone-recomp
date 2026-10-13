#!/usr/bin/env python3
"""Build leakage-resistant ML splits from a local Nevergone P-code corpus.

The corpus is produced by tools/ghidra/ExportMlPcodeCorpus.java. Labels are
explicit evidence records. Unlabelled functions are never converted into a
negative/"other" class automatically.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable

FORMAT = "NEVERGONE.MlDataset/1"
DEFAULT_QUALITY_WEIGHTS = {
    "gold": 1.0,
    "silver": 0.6,
    "bronze": 0.3,
}


def read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            line = raw.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_no}: invalid JSON: {exc}") from exc
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def stable_id(program: str, entry: str) -> str:
    digest = hashlib.sha256(f"{program}\0{entry}".encode("utf-8")).hexdigest()
    return digest[:24]


def split_name(record_id: str) -> str:
    bucket = int(hashlib.sha256(record_id.encode("ascii")).hexdigest()[:8], 16) % 100
    if bucket < 80:
        return "train"
    if bucket < 90:
        return "validation"
    return "test"


def label_weight(row: dict[str, Any]) -> float:
    explicit = row.get("weight")
    if explicit is not None:
        value = float(explicit)
        if value <= 0.0 or value > 1.0:
            raise ValueError(f"label weight must be in (0, 1], got {value}")
        return value
    quality = row.get("quality")
    if not isinstance(quality, str) or quality not in DEFAULT_QUALITY_WEIGHTS:
        raise ValueError(
            "label requires quality=gold|silver|bronze or an explicit weight"
        )
    return DEFAULT_QUALITY_WEIGHTS[quality]


def load_corpus(path: Path) -> tuple[dict[tuple[str, str], dict[str, Any]], dict[tuple[str, str], list[tuple[str, str]]], Counter]:
    by_entry: dict[tuple[str, str], dict[str, Any]] = {}
    by_name: dict[tuple[str, str], list[tuple[str, str]]] = defaultdict(list)
    stats: Counter = Counter()
    for row in read_jsonl(path):
        stats["rows"] += 1
        if row.get("decompile_status") != "ok":
            stats["decompile_failed"] += 1
            continue
        program = row.get("program")
        entry = row.get("source_entry")
        name = row.get("source_name")
        text = row.get("model_text")
        if not all(isinstance(value, str) and value for value in (program, entry, name, text)):
            stats["invalid_ok_rows"] += 1
            continue
        key = (program, entry)
        if key in by_entry:
            raise ValueError(f"duplicate corpus key: {program} {entry}")
        by_entry[key] = row
        by_name[(program, name)].append(key)
        stats["usable"] += 1
    return by_entry, by_name, stats


def resolve_label_target(
    row: dict[str, Any],
    by_entry: dict[tuple[str, str], dict[str, Any]],
    by_name: dict[tuple[str, str], list[tuple[str, str]]],
) -> tuple[str, str]:
    program = row.get("program")
    if not isinstance(program, str) or not program:
        raise ValueError("label row requires non-empty program")

    entry = row.get("source_entry")
    name = row.get("source_name")
    if isinstance(entry, str) and entry:
        key = (program, entry)
        if key not in by_entry:
            raise ValueError(f"label target not present in corpus: {program} {entry}")
        if isinstance(name, str) and name and by_entry[key].get("source_name") != name:
            raise ValueError(
                f"label source_name mismatch for {program} {entry}: "
                f"{name!r} != {by_entry[key].get('source_name')!r}"
            )
        return key

    if isinstance(name, str) and name:
        matches = by_name.get((program, name), [])
        if len(matches) == 1:
            return matches[0]
        if not matches:
            raise ValueError(f"label name not present in corpus: {program} {name}")
        raise ValueError(f"ambiguous label name in corpus: {program} {name}")

    raise ValueError("label row requires source_entry or source_name")


def build_dataset(corpus_path: Path, labels_path: Path, output_dir: Path) -> dict[str, Any]:
    by_entry, by_name, corpus_stats = load_corpus(corpus_path)

    labels_by_key: dict[tuple[str, str], dict[str, str]] = defaultdict(dict)
    weights_by_key: dict[tuple[str, str], dict[str, float]] = defaultdict(dict)
    quality_by_key: dict[tuple[str, str], dict[str, str]] = defaultdict(dict)
    evidence_by_key: dict[tuple[str, str], dict[str, str]] = defaultdict(dict)
    label_rows = 0

    for row in read_jsonl(labels_path):
        label_rows += 1
        key = resolve_label_target(row, by_entry, by_name)
        task = row.get("task")
        label = row.get("label")
        quality = row.get("quality")
        evidence = row.get("evidence")
        if not isinstance(task, str) or not task:
            raise ValueError("label row requires non-empty task")
        if not isinstance(label, str) or not label:
            raise ValueError("label row requires non-empty label")
        if quality is not None and not isinstance(quality, str):
            raise ValueError("quality must be a string")
        if evidence is not None and not isinstance(evidence, str):
            raise ValueError("evidence must be a string")

        previous = labels_by_key[key].get(task)
        if previous is not None and previous != label:
            raise ValueError(
                f"conflicting labels for {key[0]} {key[1]} task={task}: "
                f"{previous!r} vs {label!r}"
            )
        labels_by_key[key][task] = label
        weights_by_key[key][task] = label_weight(row)
        quality_by_key[key][task] = quality or "explicit-weight"
        if evidence:
            evidence_by_key[key][task] = evidence

    output_dir.mkdir(parents=True, exist_ok=True)
    split_paths = {
        name: output_dir / f"{name}.jsonl"
        for name in ("train", "validation", "test")
    }
    trace_path = output_dir / "trace.jsonl"

    split_counts: Counter = Counter()
    task_counts: dict[str, Counter] = defaultdict(Counter)
    quality_counts: Counter = Counter()
    emitted = 0

    handles = {name: path.open("w", encoding="utf-8") for name, path in split_paths.items()}
    trace = trace_path.open("w", encoding="utf-8")
    try:
        for key in sorted(labels_by_key):
            row = by_entry[key]
            program, entry = key
            record_id = stable_id(program, entry)
            split = split_name(record_id)
            labels = labels_by_key[key]
            weights = weights_by_key[key]
            qualities = quality_by_key[key]

            training_row = {
                "id": record_id,
                "text": row["model_text"],
                "labels": labels,
                "weights": weights,
            }
            handles[split].write(json.dumps(training_row, sort_keys=True) + "\n")

            trace_row = {
                "id": record_id,
                "program": program,
                "source_entry": entry,
                "source_name": row["source_name"],
                "split": split,
                "labels": labels,
                "qualities": qualities,
                "evidence": evidence_by_key.get(key, {}),
            }
            trace.write(json.dumps(trace_row, sort_keys=True) + "\n")

            emitted += 1
            split_counts[split] += 1
            for task, value in labels.items():
                task_counts[task][value] += 1
                quality_counts[qualities[task]] += 1
    finally:
        for handle in handles.values():
            handle.close()
        trace.close()

    report = {
        "format": FORMAT,
        "corpus": str(corpus_path),
        "labels": str(labels_path),
        "corpus_stats": dict(corpus_stats),
        "label_rows": label_rows,
        "labelled_functions": len(labels_by_key),
        "emitted_functions": emitted,
        "unlabelled_usable_functions": max(0, len(by_entry) - len(labels_by_key)),
        "splits": dict(split_counts),
        "task_distribution": {
            task: dict(counts) for task, counts in sorted(task_counts.items())
        },
        "quality_distribution": dict(quality_counts),
        "policy": {
            "unlabelled_becomes_negative": False,
            "source_entry_in_model_text": False,
            "source_name_in_model_text": False,
            "split_is_deterministic_by_source_identity": True,
        },
    }
    (output_dir / "manifest.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("labels", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()

    report = build_dataset(args.corpus, args.labels, args.output_dir)
    print(f"format: {report['format']}")
    print(f"usable corpus functions: {report['corpus_stats'].get('usable', 0)}")
    print(f"labelled functions: {report['labelled_functions']}")
    print(f"splits: {report['splits']}")
    print(f"output: {args.output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
