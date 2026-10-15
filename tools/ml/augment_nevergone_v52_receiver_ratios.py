#!/usr/bin/env python3
"""Augment ProductionClassifier/5 evidence with normalized receiver ratios for v5.2."""
from __future__ import annotations

import argparse
import json
import math
from collections import Counter
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.ProductionEvidence/1"
RATIO_FORMAT = "NEVERGONE.ReceiverRatioAugmentation/1"


def _read_jsonl(path: Path):
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield line_no, row


def _finite(value: Any) -> float | None:
    if isinstance(value, bool):
        return float(value)
    if isinstance(value, (int, float)) and math.isfinite(float(value)):
        return float(value)
    return None


def _ratio(features: dict[str, Any], numerator: str, denominator: float, output: str) -> None:
    if denominator <= 0:
        return
    value = _finite(features.get(numerator))
    if value is None:
        return
    features[output] = value / denominator


def add_receiver_ratio_features(features: dict[str, Any]) -> int:
    before = set(features)
    call_count = _finite(features.get("receiver.call_count")) or 0.0
    inference_count = _finite(features.get("receiver.inference_count")) or 0.0

    if call_count > 0:
        for numerator, output in (
            ("receiver.present_count", "receiver.present_ratio"),
            ("receiver.direct_single_pointer_count", "receiver.direct_single_pointer_ratio"),
            ("receiver.flow_unique_count", "receiver.flow_unique_ratio"),
            ("receiver.flow_ambiguous_count", "receiver.flow_ambiguous_ratio"),
            ("receiver.pointer_depth_sum", "receiver.pointer_depth_mean"),
            ("receiver.flow_candidate_total", "receiver.flow_candidate_mean"),
            ("receiver.inference_count", "receiver.inference_to_call_ratio"),
        ):
            _ratio(features, numerator, call_count, output)

        for key in list(features):
            if not key.endswith("_count"):
                continue
            if key.startswith("receiver.pointer_depth_") or key.startswith("receiver.storage_"):
                _ratio(features, key, call_count, key[:-6] + "_ratio")

    if inference_count > 0:
        for numerator, output in (
            ("receiver.resolved_count", "receiver.resolved_ratio"),
            ("receiver.runtime_match_count", "receiver.runtime_match_ratio"),
            ("receiver.non_virtual_count", "receiver.non_virtual_ratio"),
            ("receiver.candidate_class_total", "receiver.candidate_class_mean"),
        ):
            _ratio(features, numerator, inference_count, output)

        for key in list(features):
            if not key.endswith("_count"):
                continue
            if (
                key.startswith("receiver.status_")
                or key.startswith("receiver.evidence_")
                or key.startswith("receiver.slot_")
            ):
                _ratio(features, key, inference_count, key[:-6] + "_ratio")

    return len(set(features) - before)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    rows = 0
    receiver_rows = 0
    ratio_rows = 0
    added_total = 0
    feature_counts: Counter[str] = Counter()

    with args.output.open("w", encoding="utf-8") as out:
        for line_no, row in _read_jsonl(args.input):
            if row.get("format") != FORMAT:
                raise ValueError(f"{args.input}:{line_no}: expected {FORMAT}")
            features = row.get("features")
            if not isinstance(features, dict):
                raise ValueError(f"{args.input}:{line_no}: features must be object")
            clean = dict(features)
            rows += 1
            if (_finite(clean.get("receiver.call_count")) or 0.0) > 0:
                receiver_rows += 1
            added = add_receiver_ratio_features(clean)
            if added:
                ratio_rows += 1
                added_total += added
            for key in clean:
                if key.startswith("receiver.") and (key.endswith("_ratio") or key.endswith("_mean")):
                    feature_counts[key] += 1
            out.write(json.dumps({"format": FORMAT, "id": row.get("id"), "features": dict(sorted(clean.items()))}, sort_keys=True) + "\n")

    print("format:", RATIO_FORMAT)
    print("evidence format:", FORMAT)
    print("rows:", rows)
    print("rows with receiver calls:", receiver_rows)
    print("rows with ratio features:", ratio_rows)
    print("ratio values added:", added_total)
    print("ratio feature keys:", len(feature_counts))
    print("ratio feature coverage:", json.dumps(dict(sorted(feature_counts.items())), sort_keys=True))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
