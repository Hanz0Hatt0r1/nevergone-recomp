#!/usr/bin/env python3
"""Add conservative function-role labels to an existing Nevergone ML dataset.

Recovered Ghidra names are label provenance only. They are read exclusively from
trace.jsonl and are never copied into model input rows. Unmatched functions stay
unlabelled for function_role; there is no synthetic "other" class.
"""
from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

from nevergone_function_origin import classify_function_origin

ROLE_TASK = "function_role"
ROLE_QUALITY = "gold"


def read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            yield row


def method_parts(name: str) -> tuple[str | None, str]:
    parts = [part for part in name.split("::") if part]
    method = parts[-1] if parts else name
    owner = parts[-2] if len(parts) >= 2 else None
    return owner, method


def _starts_camel(method: str, prefix: str) -> bool:
    if not method.startswith(prefix) or len(method) <= len(prefix):
        return False
    next_char = method[len(prefix)]
    return next_char.isupper() or next_char == "_"


def infer_function_role(name: str) -> tuple[str, str] | None:
    """Return a high-precision role label inferred only from recovered name."""
    if not name or classify_function_origin(name) != "internal":
        return None
    if name.startswith("FUN_") or name.startswith("LAB_"):
        return None

    owner, method = method_parts(name)
    if not method or method.startswith("operator"):
        return None

    # C++ special members: owner::owner and owner::~owner.
    if owner:
        owner_base = re.sub(r"<.*>$", "", owner).split("::")[-1]
        method_base = re.sub(r"<.*>$", "", method)
        if method_base == f"~{owner_base}":
            return "destructor", "qualified C++ destructor name"
        if method_base == owner_base:
            return "constructor", "qualified C++ constructor name"

    # Deliberately conservative lexical roles. These names are provenance only.
    if _starts_camel(method, "set"):
        return "setter", "setX-style recovered method name"
    if _starts_camel(method, "get") or _starts_camel(method, "is") or _starts_camel(method, "has"):
        return "getter", "get/is/has-style recovered method name"

    lower = method.lower()
    if lower == "update" or _starts_camel(method, "update"):
        return "update", "update-style recovered method name"
    if lower == "parse" or _starts_camel(method, "parse") or lower == "deserialize" or _starts_camel(method, "deserialize"):
        return "parser", "parse/deserialize-style recovered method name"
    if lower == "serialize" or _starts_camel(method, "serialize"):
        return "serializer", "serialize-style recovered method name"
    if _starts_camel(method, "on"):
        return "callback", "onX-style recovered method name"

    return None


def augment_dataset(input_dir: Path, output_dir: Path) -> dict[str, Any]:
    if input_dir.resolve() == output_dir.resolve():
        raise ValueError("output dataset must differ from input dataset")

    trace_path = input_dir / "trace.jsonl"
    trace_rows = list(read_jsonl(trace_path))
    trace_by_id: dict[str, dict[str, Any]] = {}
    inferred: dict[str, tuple[str, str]] = {}
    distribution: Counter[str] = Counter()

    for row in trace_rows:
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("trace row missing id")
        if record_id in trace_by_id:
            raise ValueError(f"duplicate trace id: {record_id}")
        trace_by_id[record_id] = row
        name = row.get("source_name")
        if not isinstance(name, str):
            continue
        result = infer_function_role(name)
        if result is None:
            continue
        inferred[record_id] = result
        distribution[result[0]] += 1

    output_dir.mkdir(parents=True, exist_ok=True)
    split_counts: Counter[str] = Counter()
    labelled_in_rows = 0

    for split in ("train", "validation", "test"):
        source_path = input_dir / f"{split}.jsonl"
        target_path = output_dir / f"{split}.jsonl"
        with target_path.open("w", encoding="utf-8") as out:
            for row in read_jsonl(source_path):
                record_id = row.get("id")
                if not isinstance(record_id, str) or record_id not in trace_by_id:
                    raise ValueError(f"dataset row missing trace: {record_id!r}")
                split_counts[split] += 1
                role_info = inferred.get(record_id)
                if role_info is not None:
                    role, _ = role_info
                    labels = dict(row.get("labels") or {})
                    weights = dict(row.get("weights") or {})
                    previous = labels.get(ROLE_TASK)
                    if previous is not None and previous != role:
                        raise ValueError(
                            f"conflicting {ROLE_TASK} for {record_id}: {previous!r} vs {role!r}"
                        )
                    labels[ROLE_TASK] = role
                    weights[ROLE_TASK] = 1.0
                    row = dict(row)
                    row["labels"] = labels
                    row["weights"] = weights
                    labelled_in_rows += 1
                out.write(json.dumps(row, sort_keys=True) + "\n")

    with (output_dir / "trace.jsonl").open("w", encoding="utf-8") as out:
        for row in trace_rows:
            record_id = row["id"]
            role_info = inferred.get(record_id)
            if role_info is not None:
                role, evidence = role_info
                row = dict(row)
                labels = dict(row.get("labels") or {})
                qualities = dict(row.get("qualities") or {})
                evidences = dict(row.get("evidence") or {})
                previous = labels.get(ROLE_TASK)
                if previous is not None and previous != role:
                    raise ValueError(
                        f"conflicting trace {ROLE_TASK} for {record_id}: {previous!r} vs {role!r}"
                    )
                labels[ROLE_TASK] = role
                qualities[ROLE_TASK] = ROLE_QUALITY
                evidences[ROLE_TASK] = evidence
                row["labels"] = labels
                row["qualities"] = qualities
                row["evidence"] = evidences
            out.write(json.dumps(row, sort_keys=True) + "\n")

    manifest_path = input_dir / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    manifest = dict(manifest)
    task_distribution = dict(manifest.get("task_distribution") or {})
    task_distribution[ROLE_TASK] = dict(sorted(distribution.items()))
    manifest["task_distribution"] = task_distribution
    manifest["function_role_derivation"] = {
        "labelled_functions": labelled_in_rows,
        "quality": ROLE_QUALITY,
        "source_names_are_model_input": False,
        "unmatched_becomes_other": False,
        "roles": sorted(distribution),
        "policy": "conservative recovered-name provenance",
    }
    manifest["splits"] = dict(split_counts)
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    return {
        "functions": sum(split_counts.values()),
        "role_labelled": labelled_in_rows,
        "role_unlabelled": sum(split_counts.values()) - labelled_in_rows,
        "distribution": dict(sorted(distribution.items())),
        "splits": dict(split_counts),
        "output": str(output_dir),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_dataset", type=Path)
    parser.add_argument("output_dataset", type=Path)
    args = parser.parse_args()

    report = augment_dataset(args.input_dataset, args.output_dataset)
    print("functions:", report["functions"])
    print("function_role labelled:", report["role_labelled"])
    print("function_role unlabelled:", report["role_unlabelled"])
    print("distribution:", report["distribution"])
    print("splits:", report["splits"])
    print("output:", report["output"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
