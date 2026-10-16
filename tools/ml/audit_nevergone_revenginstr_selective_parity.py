#!/usr/bin/env python3
"""Compare historical Nevergone v4 OOF with REVENGINSTR.SelectiveClassifier/1.

The audit is intentionally strict: migration is considered parity-safe only when
both artifacts cover the same ids, make the same class/action decisions and
produce numerically equivalent calibration/OOD fields within tolerance.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.RevenginstrSelectiveParity/1"
NUMERIC_FIELDS = (
    "raw_confidence",
    "confidence",
    "family_similarity",
    "competing_similarity",
    "contrast_margin",
)


def read_jsonl(path: Path) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            record_id = row.get("id")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError(f"{path}:{line_no}: missing id")
            if record_id in result:
                raise ValueError(f"{path}:{line_no}: duplicate id {record_id}")
            result[record_id] = row
    return result


def numeric(value: Any) -> float | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, (int, float)):
        return float(value)
    return None


def manifest_metrics(path: Path) -> dict[str, float | None]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    calibration = payload.get("calibration", {})
    routing = payload.get("routing", {})
    metrics = calibration.get("metrics", {}) if isinstance(calibration, dict) else {}
    selective = routing.get("selective_oof", {}) if isinstance(routing, dict) else {}
    accept = selective.get("accept", {}) if isinstance(selective, dict) else {}
    return {
        "balanced_accuracy": numeric(metrics.get("balanced_accuracy")),
        "macro_f1": numeric(metrics.get("macro_f1")),
        "accept_precision": numeric(accept.get("precision")),
        "accept_coverage": numeric(accept.get("coverage")),
    }


def audit(reference_oof: Path, candidate_oof: Path, reference_manifest: Path,
          candidate_manifest: Path, *, tolerance: float) -> dict[str, Any]:
    reference = read_jsonl(reference_oof)
    candidate = read_jsonl(candidate_oof)
    reference_ids = set(reference)
    candidate_ids = set(candidate)
    common = sorted(reference_ids & candidate_ids)

    prediction_matches = 0
    action_matches = 0
    numeric_max_delta = {field: 0.0 for field in NUMERIC_FIELDS}
    numeric_missing = {field: 0 for field in NUMERIC_FIELDS}
    for record_id in common:
        left = reference[record_id]
        right = candidate[record_id]
        prediction_matches += int(left.get("prediction") == right.get("prediction"))
        action_matches += int(left.get("action") == right.get("action"))
        for field in NUMERIC_FIELDS:
            a, b = numeric(left.get(field)), numeric(right.get(field))
            if a is None or b is None:
                numeric_missing[field] += 1
                continue
            numeric_max_delta[field] = max(numeric_max_delta[field], abs(a - b))

    ref_metrics = manifest_metrics(reference_manifest)
    candidate_metrics = manifest_metrics(candidate_manifest)
    metric_delta: dict[str, float | None] = {}
    for key in ref_metrics:
        a, b = ref_metrics[key], candidate_metrics[key]
        metric_delta[key] = None if a is None or b is None else abs(a - b)

    exact = (
        reference_ids == candidate_ids
        and prediction_matches == len(common)
        and action_matches == len(common)
        and all(value <= tolerance for value in numeric_max_delta.values())
        and all(value == 0 for value in numeric_missing.values())
        and all(value is not None and value <= tolerance for value in metric_delta.values())
    )
    return {
        "format": FORMAT,
        "tolerance": tolerance,
        "reference_rows": len(reference),
        "candidate_rows": len(candidate),
        "common_rows": len(common),
        "missing_candidate_ids": len(reference_ids - candidate_ids),
        "extra_candidate_ids": len(candidate_ids - reference_ids),
        "prediction_matches": prediction_matches,
        "prediction_agreement": prediction_matches / len(common) if common else 0.0,
        "action_matches": action_matches,
        "action_agreement": action_matches / len(common) if common else 0.0,
        "numeric_max_abs_delta": numeric_max_delta,
        "numeric_missing": numeric_missing,
        "reference_metrics": ref_metrics,
        "candidate_metrics": candidate_metrics,
        "metric_abs_delta": metric_delta,
        "exact_parity": exact,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference_oof", type=Path)
    parser.add_argument("candidate_oof", type=Path)
    parser.add_argument("reference_manifest", type=Path)
    parser.add_argument("candidate_manifest", type=Path)
    parser.add_argument("--tolerance", type=float, default=1e-10)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()
    if args.tolerance < 0:
        raise SystemExit("--tolerance must be >= 0")
    report = audit(
        args.reference_oof,
        args.candidate_oof,
        args.reference_manifest,
        args.candidate_manifest,
        tolerance=args.tolerance,
    )
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("format:", FORMAT)
    print("rows: reference=", report["reference_rows"], "candidate=", report["candidate_rows"])
    print("prediction agreement:", round(report["prediction_agreement"], 8))
    print("action agreement:", round(report["action_agreement"], 8))
    print("numeric max abs delta:", report["numeric_max_abs_delta"])
    print("metric abs delta:", report["metric_abs_delta"])
    print("exact parity:", report["exact_parity"])
    if args.output:
        print("output:", args.output)
    if args.strict and not report["exact_parity"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
