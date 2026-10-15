#!/usr/bin/env python3
"""Build label-free evidence features for ProductionClassifier/5.

The output intentionally contains only objective numeric/categorical summaries.
Raw function names, source addresses, receiver class names, vtable identities and
project labels are never emitted as model features.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable

FORMAT = "NEVERGONE.ProductionEvidence/1"
INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
RESOLVED_STATUSES = {
    "resolved_unique_vtable",
    "resolved_unique_vtable_runtime_match",
}
MISSING = {"", "-", "<missing>", None}


def _read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def _safe_key(value: Any) -> str:
    text = str(value or "missing").strip().lower()
    text = re.sub(r"[^a-z0-9_]+", "_", text).strip("_")
    return text or "missing"


def _number(value: Any) -> float | None:
    if isinstance(value, bool):
        return float(value)
    if isinstance(value, (int, float)) and math.isfinite(float(value)):
        return float(value)
    if isinstance(value, str):
        try:
            parsed = float(value)
        except ValueError:
            try:
                parsed = float(int(value, 0))
            except ValueError:
                return None
        return parsed if math.isfinite(parsed) else None
    return None


def _int(value: Any, default: int = 0) -> int:
    if value in MISSING:
        return default
    try:
        return int(str(value), 0)
    except (TypeError, ValueError):
        return default


def _json_list(value: Any) -> list[Any]:
    if isinstance(value, list):
        return value
    if not isinstance(value, str) or not value.strip():
        return []
    try:
        parsed = json.loads(value)
    except json.JSONDecodeError:
        return []
    return parsed if isinstance(parsed, list) else []


def load_trace(path: Path) -> tuple[dict[str, str], set[str]]:
    source_to_id: dict[str, str] = {}
    dataset_ids: set[str] = set()
    for row in _read_jsonl(path):
        record_id = row.get("id")
        program = row.get("program")
        entry = row.get("source_entry")
        if not all(isinstance(x, str) and x for x in (record_id, program, entry)):
            continue
        source_id = f"{program}:{entry}"
        previous = source_to_id.get(source_id)
        if previous is not None and previous != record_id:
            raise ValueError(f"trace has conflicting IDs for {source_id}")
        source_to_id[source_id] = record_id
        dataset_ids.add(record_id)
    if not dataset_ids:
        raise ValueError(f"{path}: no usable trace rows")
    return source_to_id, dataset_ids


def resolve_id(raw_id: Any, source_to_id: dict[str, str], dataset_ids: set[str]) -> str | None:
    if not isinstance(raw_id, str) or not raw_id:
        return None
    if raw_id in dataset_ids:
        return raw_id
    return source_to_id.get(raw_id)


def add(features: dict[str, float], key: str, value: Any, *, mode: str = "set") -> None:
    number = _number(value)
    if number is None:
        return
    if mode == "sum":
        features[key] = features.get(key, 0.0) + number
    elif mode == "max":
        features[key] = max(features.get(key, number), number)
    else:
        features[key] = number


def merge_metric_files(
    paths: list[Path],
    *,
    source_to_id: dict[str, str],
    dataset_ids: set[str],
    features_by_id: dict[str, dict[str, float]],
    prefix: str,
) -> Counter:
    stats: Counter = Counter()
    for path in paths:
        for row in _read_jsonl(path):
            raw_id = row.get("id")
            record_id = resolve_id(raw_id, source_to_id, dataset_ids)
            if record_id is None:
                stats["unmatched"] += 1
                continue
            metrics = row.get("metrics")
            if not isinstance(metrics, dict):
                stats["missing_metrics"] += 1
                continue
            target = features_by_id[record_id]
            target[f"has_{prefix}"] = 1.0
            for key, value in metrics.items():
                add(target, f"{prefix}.{_safe_key(key)}", value)
            stats["matched"] += 1
    return stats


def merge_receiver_types(
    path: Path,
    *,
    source_to_id: dict[str, str],
    dataset_ids: set[str],
    features_by_id: dict[str, dict[str, float]],
) -> Counter:
    stats: Counter = Counter()
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        for row in reader:
            record_id = resolve_id(row.get("source_id"), source_to_id, dataset_ids)
            if record_id is None:
                stats["unmatched"] += 1
                continue
            target = features_by_id[record_id]
            target["has_receiver_types"] = 1.0
            add(target, "receiver.call_count", 1, mode="sum")
            add(target, "receiver.present_count", _int(row.get("receiver_present")), mode="sum")
            depth = _int(row.get("receiver_pointer_depth"))
            add(target, "receiver.pointer_depth_sum", depth, mode="sum")
            add(target, "receiver.pointer_depth_max", depth, mode="max")
            add(target, f"receiver.pointer_depth_{min(max(depth, 0), 4)}_count", 1, mode="sum")
            storage = _safe_key(row.get("receiver_storage_kind"))
            add(target, f"receiver.storage_{storage}_count", 1, mode="sum")

            direct_pointee = row.get("pointee_type_display")
            if depth == 1 and direct_pointee not in MISSING and str(direct_pointee).strip() != "void":
                add(target, "receiver.direct_single_pointer_count", 1, mode="sum")

            flow = _json_list(row.get("flow_single_pointer_pointee_displays_json"))
            unique_flow = sorted({str(item) for item in flow if item not in MISSING})
            add(target, "receiver.flow_candidate_total", len(unique_flow), mode="sum")
            add(target, "receiver.flow_candidate_max", len(unique_flow), mode="max")
            if len(unique_flow) == 1:
                add(target, "receiver.flow_unique_count", 1, mode="sum")
            elif len(unique_flow) > 1:
                add(target, "receiver.flow_ambiguous_count", 1, mode="sum")
            stats["matched"] += 1
    return stats


def _slot_bucket(value: Any) -> str:
    offset = _int(value, -1)
    if offset < 0:
        return "missing"
    if offset < 0x20:
        return "lt20"
    if offset < 0x80:
        return "20_7f"
    if offset < 0x200:
        return "80_1ff"
    return "200_plus"


def merge_receiver_inference(
    path: Path,
    *,
    source_to_id: dict[str, str],
    dataset_ids: set[str],
    features_by_id: dict[str, dict[str, float]],
) -> Counter:
    stats: Counter = Counter()
    for row in _read_jsonl(path):
        if row.get("format") != INFERENCE_FORMAT:
            stats["foreign_format"] += 1
            continue
        record_id = resolve_id(row.get("source_id"), source_to_id, dataset_ids)
        if record_id is None:
            stats["unmatched"] += 1
            continue
        target = features_by_id[record_id]
        target["has_receiver_inference"] = 1.0
        add(target, "receiver.inference_count", 1, mode="sum")

        status = _safe_key(row.get("status"))
        add(target, f"receiver.status_{status}_count", 1, mode="sum")
        if row.get("status") in RESOLVED_STATUSES:
            add(target, "receiver.resolved_count", 1, mode="sum")
        if row.get("status") == "resolved_unique_vtable_runtime_match":
            add(target, "receiver.runtime_match_count", 1, mode="sum")
        if row.get("status") == "non_virtual_indirect_call":
            add(target, "receiver.non_virtual_count", 1, mode="sum")

        evidence_source = row.get("class_evidence_source")
        if isinstance(evidence_source, str) and evidence_source:
            add(target, f"receiver.evidence_{_safe_key(evidence_source)}_count", 1, mode="sum")

        candidates = row.get("candidate_receiver_class_names")
        if not isinstance(candidates, list):
            candidates = []
        add(target, "receiver.candidate_class_total", len(candidates), mode="sum")
        add(target, "receiver.candidate_class_max", len(candidates), mode="max")
        add(target, f"receiver.slot_{_slot_bucket(row.get('dispatch_slot_offset'))}_count", 1, mode="sum")
        stats["matched"] += 1
    return stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="dataset trace.jsonl")
    parser.add_argument("--callgraph-metrics", type=Path, action="append", default=[])
    parser.add_argument("--receiver-types", type=Path)
    parser.add_argument("--receiver-inference", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    source_to_id, dataset_ids = load_trace(args.trace)
    features_by_id: dict[str, dict[str, float]] = defaultdict(dict)
    stats: dict[str, Any] = {}

    if args.callgraph_metrics:
        stats["callgraph"] = dict(
            merge_metric_files(
                args.callgraph_metrics,
                source_to_id=source_to_id,
                dataset_ids=dataset_ids,
                features_by_id=features_by_id,
                prefix="callgraph",
            )
        )
    if args.receiver_types is not None:
        stats["receiver_types"] = dict(
            merge_receiver_types(
                args.receiver_types,
                source_to_id=source_to_id,
                dataset_ids=dataset_ids,
                features_by_id=features_by_id,
            )
        )
    if args.receiver_inference is not None:
        stats["receiver_inference"] = dict(
            merge_receiver_inference(
                args.receiver_inference,
                source_to_id=source_to_id,
                dataset_ids=dataset_ids,
                features_by_id=features_by_id,
            )
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    covered = 0
    feature_counts: Counter = Counter()
    with args.output.open("w", encoding="utf-8") as handle:
        for record_id in sorted(dataset_ids):
            features = features_by_id.get(record_id, {})
            if features:
                covered += 1
            for key in features:
                feature_counts[key] += 1
            handle.write(
                json.dumps(
                    {
                        "format": FORMAT,
                        "id": record_id,
                        "features": dict(sorted(features.items())),
                    },
                    sort_keys=True,
                )
                + "\n"
            )

    print("format:", FORMAT)
    print("dataset functions:", len(dataset_ids))
    print("functions with external evidence:", covered)
    print("coverage:", round(covered / len(dataset_ids), 6))
    print("feature keys:", len(feature_counts))
    print("sources:", json.dumps(stats, sort_keys=True))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
