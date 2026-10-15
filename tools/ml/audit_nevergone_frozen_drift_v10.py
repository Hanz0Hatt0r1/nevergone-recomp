#!/usr/bin/env python3
"""Audit distribution shift between Nevergone OOF reference data and frozen v10 holdout.

This is an unlabeled deployment diagnostic. It compares class/action distributions,
base confidence/OOD features, accept-router intervention rates, and evidence-key
coverage. It never reports semantic accuracy for the frozen set.
"""
from __future__ import annotations

import argparse
import json
import math
import statistics
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

FORMAT = "NEVERGONE.FrozenDriftAuditV10/1"
CORE_NUMERIC = (
    "confidence",
    "raw_confidence",
    "family_similarity",
    "competing_similarity",
    "contrast_margin",
    "max_probability",
    "top2_gap",
    "entropy",
)
CORE_EVIDENCE = ("has_callgraph", "has_receiver_types", "has_receiver_inference")


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            rows.append(row)
    return rows


def quantile(values: list[float], q: float) -> float | None:
    clean = sorted(value for value in values if math.isfinite(value))
    if not clean:
        return None
    if len(clean) == 1:
        return clean[0]
    position = (len(clean) - 1) * q
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return clean[lower]
    weight = position - lower
    return clean[lower] * (1.0 - weight) + clean[upper] * weight


def numeric_summary(values: Iterable[float]) -> dict[str, float | int | None]:
    clean = [float(value) for value in values if math.isfinite(float(value))]
    if not clean:
        return {"count": 0, "mean": None, "p10": None, "median": None, "p90": None, "iqr": None}
    p25 = quantile(clean, 0.25)
    p75 = quantile(clean, 0.75)
    return {
        "count": len(clean),
        "mean": float(statistics.fmean(clean)),
        "p10": quantile(clean, 0.10),
        "median": quantile(clean, 0.50),
        "p90": quantile(clean, 0.90),
        "iqr": (float(p75 - p25) if p25 is not None and p75 is not None else None),
    }


def probability_features(row: dict[str, Any]) -> tuple[float, float, float]:
    payload = row.get("probabilities")
    if not isinstance(payload, dict):
        return 0.0, 0.0, 0.0
    values = [float(value) for value in payload.values() if isinstance(value, (int, float)) and float(value) >= 0.0]
    if not values:
        return 0.0, 0.0, 0.0
    values.sort(reverse=True)
    maximum = values[0]
    gap = maximum - values[1] if len(values) > 1 else maximum
    entropy = -sum(value * math.log(value) for value in values if value > 0.0)
    return maximum, gap, entropy


def numeric_value(row: dict[str, Any], key: str) -> float:
    if key in {"max_probability", "top2_gap", "entropy"}:
        maximum, gap, entropy = probability_features(row)
        return {"max_probability": maximum, "top2_gap": gap, "entropy": entropy}[key]
    raw = row.get(key, 0.0)
    if isinstance(raw, (int, float)) and math.isfinite(float(raw)):
        return float(raw)
    return 0.0


def categorical_distribution(values: Iterable[str]) -> dict[str, float]:
    counts = Counter(str(value) for value in values)
    total = sum(counts.values())
    if not total:
        return {}
    return {key: count / total for key, count in sorted(counts.items())}


def js_divergence_bits(left: dict[str, float], right: dict[str, float]) -> float:
    keys = sorted(set(left) | set(right))
    if not keys:
        return 0.0
    result = 0.0
    for key in keys:
        p = float(left.get(key, 0.0))
        q = float(right.get(key, 0.0))
        m = (p + q) / 2.0
        if p > 0.0:
            result += 0.5 * p * math.log2(p / m)
        if q > 0.0:
            result += 0.5 * q * math.log2(q / m)
    return result


def distribution_payload(reference: Iterable[str], frozen: Iterable[str]) -> dict[str, Any]:
    ref = categorical_distribution(reference)
    new = categorical_distribution(frozen)
    return {
        "reference": ref,
        "frozen": new,
        "js_divergence_bits": js_divergence_bits(ref, new),
    }


def numeric_shift(reference_rows: list[dict[str, Any]], frozen_rows: list[dict[str, Any]], key: str) -> dict[str, Any]:
    reference = numeric_summary(numeric_value(row, key) for row in reference_rows)
    frozen = numeric_summary(numeric_value(row, key) for row in frozen_rows)
    ref_median = reference["median"]
    frozen_median = frozen["median"]
    iqr = reference["iqr"]
    median_delta = None
    robust_shift = None
    if ref_median is not None and frozen_median is not None:
        median_delta = float(frozen_median - ref_median)
        if iqr is not None and abs(float(iqr)) > 1e-12:
            robust_shift = median_delta / float(iqr)
    return {
        "reference": reference,
        "frozen": frozen,
        "median_delta": median_delta,
        "robust_shift_iqr": robust_shift,
    }


def evidence_map(rows: list[dict[str, Any]]) -> tuple[dict[str, dict[str, float]], set[str]]:
    result: dict[str, dict[str, float]] = {}
    keys: set[str] = set()
    for row in rows:
        record_id = row.get("id")
        features = row.get("features")
        if not isinstance(record_id, str) or not record_id or not isinstance(features, dict):
            continue
        clean: dict[str, float] = {}
        for key, value in features.items():
            if isinstance(key, str) and isinstance(value, (int, float)) and math.isfinite(float(value)):
                clean[key] = float(value)
                keys.add(key)
        result[record_id] = clean
    return result, keys


def evidence_coverage(rows: dict[str, dict[str, float]], keys: Iterable[str]) -> dict[str, float]:
    total = max(1, len(rows))
    return {
        key: sum(key in features for features in rows.values()) / total
        for key in keys
    }


def severity(summary: dict[str, Any]) -> dict[str, Any]:
    prediction_js = float(summary["categorical_shift"]["prediction"]["js_divergence_bits"])
    base_action_js = float(summary["categorical_shift"]["base_action"]["js_divergence_bits"])
    robust = [
        abs(float(payload["robust_shift_iqr"]))
        for payload in summary["numeric_shift_all"].values()
        if payload.get("robust_shift_iqr") is not None
    ]
    max_robust = max(robust, default=0.0)
    core_cov = summary["evidence_shift"]["core"]
    max_core_drop = max(
        (max(0.0, float(payload["reference"]) - float(payload["frozen"])) for payload in core_cov.values()),
        default=0.0,
    )
    change_ratio = float(summary["router_intervention"]["accept_change_rate_ratio"])
    if prediction_js >= 0.10 or base_action_js >= 0.10 or max_robust >= 1.0 or max_core_drop >= 0.15:
        level = "high"
    elif prediction_js >= 0.03 or base_action_js >= 0.03 or max_robust >= 0.5 or max_core_drop >= 0.05 or change_ratio >= 2.0:
        level = "moderate"
    else:
        level = "low"
    return {
        "level": level,
        "diagnostic_only": True,
        "thresholds": {
            "categorical_js_moderate": 0.03,
            "categorical_js_high": 0.10,
            "robust_shift_iqr_moderate": 0.5,
            "robust_shift_iqr_high": 1.0,
            "core_evidence_drop_moderate": 0.05,
            "core_evidence_drop_high": 0.15,
            "accept_change_rate_ratio_moderate": 2.0,
        },
        "observed": {
            "prediction_js_bits": prediction_js,
            "base_action_js_bits": base_action_js,
            "max_abs_robust_shift_iqr": max_robust,
            "max_core_evidence_drop": max_core_drop,
            "accept_change_rate_ratio": change_ratio,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base_oof", type=Path)
    parser.add_argument("v9_oof", type=Path)
    parser.add_argument("router_manifest", type=Path)
    parser.add_argument("train_evidence", type=Path)
    parser.add_argument("frozen_classified", type=Path)
    parser.add_argument("frozen_evidence", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--top-feature-shifts", type=int, default=20)
    args = parser.parse_args()

    base_rows = read_jsonl(args.base_oof)
    v9_rows = read_jsonl(args.v9_oof)
    frozen_rows = read_jsonl(args.frozen_classified)
    train_evidence_rows = read_jsonl(args.train_evidence)
    frozen_evidence_rows = read_jsonl(args.frozen_evidence)
    manifest = json.loads(args.router_manifest.read_text(encoding="utf-8"))

    if not base_rows or not frozen_rows:
        raise SystemExit("reference and frozen rows must be non-empty")
    v9_by_id = {str(row.get("id")): row for row in v9_rows if row.get("id") is not None}
    if len(v9_by_id) != len(v9_rows):
        raise ValueError("v9 OOF contains duplicate or missing ids")
    reference_rows: list[dict[str, Any]] = []
    for row in base_rows:
        record_id = str(row.get("id", ""))
        companion = v9_by_id.get(record_id)
        if companion is None:
            raise ValueError(f"v9 OOF missing id {record_id}")
        merged = dict(row)
        merged["base_action"] = companion.get("legacy_action", row.get("action"))
        reference_rows.append(merged)

    for row in frozen_rows:
        row.setdefault("base_action", row.get("action"))

    reference_accept = [row for row in reference_rows if row.get("base_action") == "accept"]
    frozen_accept = [row for row in frozen_rows if row.get("base_action") == "accept"]

    calibration = manifest.get("calibration", {})
    legacy_accept_examples = int(calibration.get("legacy_accept_examples", len(reference_accept)))
    kept_examples = int(calibration.get("kept_examples", legacy_accept_examples))
    reference_changed = max(0, legacy_accept_examples - kept_examples)
    frozen_changed = sum(bool(row.get("accept_router_changed")) for row in frozen_rows)
    reference_change_accept = reference_changed / max(1, legacy_accept_examples)
    frozen_change_accept = frozen_changed / max(1, len(frozen_accept))

    numeric_all = {
        key: numeric_shift(reference_rows, frozen_rows, key)
        for key in CORE_NUMERIC
    }
    numeric_accept = {
        key: numeric_shift(reference_accept, frozen_accept, key)
        for key in CORE_NUMERIC
    }

    train_evidence, train_keys = evidence_map(train_evidence_rows)
    frozen_evidence, frozen_keys = evidence_map(frozen_evidence_rows)
    all_evidence_keys = sorted(train_keys | frozen_keys)
    train_cov = evidence_coverage(train_evidence, all_evidence_keys)
    frozen_cov = evidence_coverage(frozen_evidence, all_evidence_keys)
    shifts = [
        {
            "key": key,
            "reference": train_cov[key],
            "frozen": frozen_cov[key],
            "delta": frozen_cov[key] - train_cov[key],
            "abs_delta": abs(frozen_cov[key] - train_cov[key]),
        }
        for key in all_evidence_keys
    ]
    shifts.sort(key=lambda row: (-row["abs_delta"], row["key"]))
    core = {
        key: {
            "reference": train_cov.get(key, 0.0),
            "frozen": frozen_cov.get(key, 0.0),
            "delta": frozen_cov.get(key, 0.0) - train_cov.get(key, 0.0),
        }
        for key in CORE_EVIDENCE
    }

    legacy_review = sum(row.get("base_action") == "review" for row in reference_rows)
    legacy_escalate = sum(row.get("base_action") == "escalate" for row in reference_rows)
    reference_routed_actions = (
        ["accept"] * kept_examples
        + ["review"] * legacy_review
        + ["escalate"] * (legacy_escalate + reference_changed)
    )

    summary: dict[str, Any] = {
        "format": FORMAT,
        "labels_available": False,
        "reference_rows": len(reference_rows),
        "frozen_rows": len(frozen_rows),
        "categorical_shift": {
            "prediction": distribution_payload(
                (str(row.get("prediction")) for row in reference_rows),
                (str(row.get("prediction")) for row in frozen_rows),
            ),
            "base_action": distribution_payload(
                (str(row.get("base_action")) for row in reference_rows),
                (str(row.get("base_action")) for row in frozen_rows),
            ),
            "routed_action": distribution_payload(
                reference_routed_actions,
                (str(row.get("action")) for row in frozen_rows),
            ),
        },
        "router_intervention": {
            "reference_legacy_accept": legacy_accept_examples,
            "reference_changed": reference_changed,
            "reference_change_fraction_all": reference_changed / len(reference_rows),
            "reference_change_fraction_accept": reference_change_accept,
            "frozen_legacy_accept": len(frozen_accept),
            "frozen_changed": frozen_changed,
            "frozen_change_fraction_all": frozen_changed / len(frozen_rows),
            "frozen_change_fraction_accept": frozen_change_accept,
            "accept_change_rate_ratio": frozen_change_accept / max(reference_change_accept, 1e-12),
        },
        "numeric_shift_all": numeric_all,
        "numeric_shift_base_accept": numeric_accept,
        "evidence_shift": {
            "reference_rows": len(train_evidence),
            "frozen_rows": len(frozen_evidence),
            "core": core,
            "top_feature_coverage_shifts": shifts[: max(0, args.top_feature_shifts)],
        },
        "note": (
            "Unlabeled drift audit only. Distribution shift or router intervention rate does not imply semantic error."
        ),
    }
    summary["severity"] = severity(summary)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", FORMAT)
    print("reference rows:", len(reference_rows), "frozen rows:", len(frozen_rows))
    print("prediction JS bits:", round(summary["categorical_shift"]["prediction"]["js_divergence_bits"], 6))
    print("base-action JS bits:", round(summary["categorical_shift"]["base_action"]["js_divergence_bits"], 6))
    intervention = summary["router_intervention"]
    print(
        "router change among accept: reference=",
        round(intervention["reference_change_fraction_accept"], 6),
        "frozen=",
        round(intervention["frozen_change_fraction_accept"], 6),
        "ratio=",
        round(intervention["accept_change_rate_ratio"], 3),
    )
    print("core evidence:", core)
    print("drift severity:", summary["severity"]["level"], "(diagnostic only)")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
