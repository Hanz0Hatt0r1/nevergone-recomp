#!/usr/bin/env python3
"""Audit the generic revenginstr accept router against Nevergone production gates.

Unlike classifier migration, exact threshold/model parity with historical v10 is
not expected: REVENGINSTR.AcceptErrorRouter/1 intentionally calibrates from its
own family-grouped crossfit over a generic base OOF contract.  The migration gate
therefore checks retained deployment invariants and reports deltas against the
immutable Nevergone v10 manifest.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.RevenginstrAcceptRouterGate/1"
REFERENCE_FORMAT = "NEVERGONE.AcceptErrorRouter/1"
CANDIDATE_FORMAT = "REVENGINSTR.AcceptErrorRouter/1"


def load(path: Path) -> dict[str, Any]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ValueError(f"{path}: expected JSON object")
    return payload


def number(mapping: dict[str, Any], key: str) -> float:
    value = mapping.get(key)
    if not isinstance(value, (int, float)) or isinstance(value, bool):
        raise ValueError(f"missing numeric field: {key}")
    return float(value)


def audit(reference_manifest: Path, candidate_manifest: Path, *,
          min_precision: float, min_coverage: float) -> dict[str, Any]:
    reference = load(reference_manifest)
    candidate = load(candidate_manifest)
    if reference.get("format") != REFERENCE_FORMAT:
        raise ValueError(f"reference format must be {REFERENCE_FORMAT}")
    if candidate.get("format") != CANDIDATE_FORMAT:
        raise ValueError(f"candidate format must be {CANDIDATE_FORMAT}")

    ref_cal = reference.get("calibration")
    cand_cal = candidate.get("calibration")
    if not isinstance(ref_cal, dict) or not isinstance(cand_cal, dict):
        raise ValueError("both manifests must contain calibration objects")

    ref_precision = number(ref_cal, "kept_precision")
    ref_coverage = number(ref_cal, "kept_coverage")
    cand_precision = number(cand_cal, "kept_precision")
    cand_coverage = number(cand_cal, "kept_coverage")
    policy = candidate.get("policy")
    if not isinstance(policy, dict):
        raise ValueError("candidate missing policy")

    promotions_allowed = policy.get("promotions_allowed")
    class_prediction_action = policy.get("class_prediction_action")
    non_accept_action = policy.get("non_accept_action")
    below_threshold_action = policy.get("below_threshold_action")
    invariant_pass = (
        promotions_allowed is False
        and class_prediction_action == "preserve"
        and non_accept_action == "preserve"
        and below_threshold_action == "escalate"
    )
    gate_pass = bool(
        cand_precision >= min_precision
        and cand_coverage >= min_coverage
        and invariant_pass
    )
    return {
        "format": FORMAT,
        "minimums": {
            "accept_precision": min_precision,
            "accept_coverage": min_coverage,
        },
        "reference": {
            "format": reference.get("format"),
            "threshold": reference.get("threshold"),
            "kept_precision": ref_precision,
            "kept_coverage": ref_coverage,
        },
        "candidate": {
            "format": candidate.get("format"),
            "threshold": candidate.get("threshold"),
            "kept_precision": cand_precision,
            "kept_coverage": cand_coverage,
            "folds": cand_cal.get("folds"),
            "legacy_accept_examples": cand_cal.get("legacy_accept_examples"),
            "kept_examples": cand_cal.get("kept_examples"),
        },
        "delta": {
            "accept_precision": cand_precision - ref_precision,
            "accept_coverage": cand_coverage - ref_coverage,
        },
        "invariants": {
            "promotions_allowed": promotions_allowed,
            "class_prediction_action": class_prediction_action,
            "non_accept_action": non_accept_action,
            "below_threshold_action": below_threshold_action,
            "pass": invariant_pass,
        },
        "migration_gate_pass": gate_pass,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference_manifest", type=Path)
    parser.add_argument("candidate_manifest", type=Path)
    parser.add_argument("--min-precision", type=float, default=0.97)
    parser.add_argument("--min-coverage", type=float, default=0.55)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()
    report = audit(
        args.reference_manifest, args.candidate_manifest,
        min_precision=args.min_precision, min_coverage=args.min_coverage,
    )
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("format:", FORMAT)
    print("reference:", report["reference"])
    print("candidate:", report["candidate"])
    print("delta:", report["delta"])
    print("invariants:", report["invariants"])
    print("migration gate pass:", report["migration_gate_pass"])
    if args.output:
        print("output:", args.output)
    if args.strict and not report["migration_gate_pass"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
