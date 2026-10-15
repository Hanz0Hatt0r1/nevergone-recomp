#!/usr/bin/env python3
"""Package the v9 accept-error meta-router for production inference.

This step does not claim a new evaluation result. v9 remains the unbiased
cross-fitted evaluation. v10 uses all existing OOF rows to choose one global
production threshold from cross-fitted accept correctness scores, then fits the
final accept-only meta-model on every leakage-safe legacy ACCEPT row.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from evaluate_nevergone_accept_error_routing_v9 import (
    build_matrix,
    read_jsonl,
    select_keep_threshold,
)
from evaluate_nevergone_correctness_meta_routing_v7 import (
    fit_meta_models,
    predict_meta_models,
    safe_evidence_keys,
)
from train_nevergone_production_classifier import _require_dependencies
from train_nevergone_production_classifier_v5 import load_evidence

FORMAT = "NEVERGONE.AcceptErrorRouter/1"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base_oof", type=Path)
    parser.add_argument("v8_oof", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--target-accept-precision", type=float, default=0.97)
    parser.add_argument("--max-accept-coverage-loss", type=float, default=0.04)
    parser.add_argument("--min-class-meta-examples", type=int, default=100)
    parser.add_argument("--min-accept-examples", type=int, default=100)
    args = parser.parse_args()

    deps = _require_dependencies()
    np = deps["np"]
    try:
        import joblib
    except ImportError as exc:
        raise SystemExit("joblib is required") from exc

    base = read_jsonl(args.base_oof)
    v8 = read_jsonl(args.v8_oof)
    if set(base) != set(v8):
        raise ValueError(f"OOF id mismatch: base={len(base)} v8={len(v8)}")
    evidence = load_evidence(args.evidence)
    evidence_keys = safe_evidence_keys(evidence)

    rows: list[dict[str, Any]] = []
    for record_id, base_row in base.items():
        v8_row = v8[record_id]
        prediction = str(base_row.get("prediction"))
        truth = str(base_row.get("truth"))
        if prediction != str(v8_row.get("prediction")) or truth != str(v8_row.get("truth")):
            raise ValueError(f"prediction/truth mismatch for {record_id}")
        fold = int(v8_row.get("fold", 0))
        if fold <= 0:
            raise ValueError(f"missing fold for {record_id}")
        rows.append(
            {
                "id": record_id,
                "fold": fold,
                "truth": truth,
                "prediction": prediction,
                "correct": bool(base_row.get("correct", prediction == truth)),
                "legacy_action": str(v8_row.get("legacy_action")),
                "probabilities": dict(base_row.get("probabilities", {})),
                "family_similarity": float(base_row.get("family_similarity", 0.0)),
                "competing_similarity": float(base_row.get("competing_similarity", 0.0)),
                "contrast_margin": float(base_row.get("contrast_margin", 0.0)),
            }
        )

    matrix = build_matrix(rows, evidence, evidence_keys, np)
    predictions = np.asarray([row["prediction"] for row in rows], dtype=object)
    correct = np.asarray([bool(row["correct"]) for row in rows], dtype=bool)
    folds = np.asarray([int(row["fold"]) for row in rows], dtype=np.int64)
    accept = np.asarray(
        [idx for idx, row in enumerate(rows) if row["legacy_action"] == "accept"],
        dtype=np.int64,
    )
    if len(accept) < args.min_accept_examples:
        raise ValueError("not enough legacy ACCEPT rows")

    crossfit_scores = np.full(len(accept), 0.5, dtype=np.float64)
    for fold in sorted(set(folds[accept].tolist())):
        fit_local = np.where(folds[accept] != fold)[0]
        valid_local = np.where(folds[accept] == fold)[0]
        fit_global = accept[fit_local]
        valid_global = accept[valid_local]
        models = fit_meta_models(
            matrix[fit_global],
            predictions[fit_global],
            correct[fit_global].astype(np.int64),
            seed=args.seed + 1000 + fold,
            min_class_examples=args.min_class_meta_examples,
        )
        crossfit_scores[valid_local] = predict_meta_models(
            models, matrix[valid_global], predictions[valid_global], np
        )

    selected, candidates = select_keep_threshold(
        crossfit_scores,
        correct[accept],
        train_total=len(rows),
        legacy_accept_count=len(accept),
        target_precision=args.target_accept_precision,
        max_accept_coverage_loss=args.max_accept_coverage_loss,
        min_accept_examples=args.min_accept_examples,
        np=np,
    )
    if selected is None:
        raise RuntimeError("no feasible production threshold on cross-fitted ACCEPT scores")
    threshold = float(selected["threshold"])
    keep = crossfit_scores >= threshold
    calibration_examples = int(keep.sum())
    calibration_precision = float(correct[accept][keep].mean()) if calibration_examples else 0.0
    calibration_coverage = calibration_examples / max(1, len(rows))

    global_model, class_models = fit_meta_models(
        matrix[accept],
        predictions[accept],
        correct[accept].astype(np.int64),
        seed=args.seed + 1000000,
        min_class_examples=args.min_class_meta_examples,
    )

    artifact = {
        "format": FORMAT,
        "threshold": threshold,
        "classes": list(sorted(set(predictions.tolist()))),
        "evidence_keys": evidence_keys,
        "meta_feature_count": 14 + len(evidence_keys),
        "global_model": global_model,
        "class_models": class_models,
        "policy": {
            "scope": "legacy_accept_only",
            "below_threshold_action": "escalate",
            "non_accept_action": "preserve",
            "promotions_allowed": False,
        },
        "calibration": {
            "method": "outer-fold-crossfit-oof",
            "training_feedback": False,
            "target_accept_precision": args.target_accept_precision,
            "max_accept_coverage_loss": args.max_accept_coverage_loss,
            "legacy_accept_examples": int(len(accept)),
            "kept_examples": calibration_examples,
            "kept_precision": calibration_precision,
            "kept_coverage": calibration_coverage,
            "candidate_count": len(candidates),
        },
    }

    args.output_dir.mkdir(parents=True, exist_ok=True)
    artifact_path = args.output_dir / "accept_error_router.joblib"
    manifest_path = args.output_dir / "manifest.json"
    joblib.dump(artifact, artifact_path)
    manifest = {
        "format": FORMAT,
        "threshold": threshold,
        "evidence_feature_count": len(evidence_keys),
        "meta_feature_count": 14 + len(evidence_keys),
        "calibration": artifact["calibration"],
        "policy": artifact["policy"],
        "artifact": str(artifact_path),
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", FORMAT)
    print("threshold:", threshold)
    print("crossfit accept precision:", f"{calibration_precision:.6f}")
    print("crossfit accept coverage:", f"{calibration_coverage:.6f}")
    print("accept rows:", len(accept))
    print("evidence features:", len(evidence_keys))
    print("artifact:", artifact_path)
    print("manifest:", manifest_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
