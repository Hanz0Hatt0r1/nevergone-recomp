#!/usr/bin/env python3
"""Cross-fitted accept-only error routing for ProductionClassifier v9.

v8 showed that an all-row correctness meta-model improves REVIEW quality but barely
finds errors inside the already selective ACCEPT set. v9 therefore trains the
meta-model only on leakage-safe legacy ACCEPT rows. Class predictions and all
legacy REVIEW/ESCALATE actions are frozen. A legacy ACCEPT can only remain ACCEPT
or be downgraded directly to ESCALATE.

Inputs are existing OOF artifacts: v5.2 supplies held-out probabilities/similarity,
v8 supplies family-held-out legacy actions/fold ids, and v5.2 evidence supplies
sanitized numeric structural features. No Ghidra or base-model retraining is needed.
For each target outer fold, meta fitting and threshold selection use only other
outer folds. Threshold selection itself uses leave-one-fold-out meta OOF scores.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

from evaluate_nevergone_correctness_meta_routing_v7 import (
    CLASSES,
    accept_attribution,
    fit_meta_models,
    predict_meta_models,
    safe_evidence_keys,
    summarize_actions,
    threshold_grid,
)
from train_nevergone_production_classifier import _require_dependencies
from train_nevergone_production_classifier_v5 import load_evidence

FORMAT = "NEVERGONE.AcceptErrorRoutingV9/1"


def read_jsonl(path: Path) -> dict[str, dict[str, Any]]:
    rows: dict[str, dict[str, Any]] = {}
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            record_id = str(row.get("id", ""))
            if not record_id:
                raise ValueError(f"{path}:{line_no}: missing id")
            if record_id in rows:
                raise ValueError(f"{path}:{line_no}: duplicate id {record_id}")
            rows[record_id] = row
    return rows


def entropy(values) -> float:
    total = 0.0
    for raw in values:
        value = float(raw)
        if value > 0.0:
            total -= value * math.log(value)
    return total


def build_matrix(rows: list[dict[str, Any]], evidence, evidence_keys, np):
    matrix = np.zeros((len(rows), 14 + len(evidence_keys)), dtype=np.float64)
    class_index = {label: idx for idx, label in enumerate(CLASSES)}
    for index, row in enumerate(rows):
        probs_map = row["probabilities"]
        probs = [float(probs_map.get(label, 0.0)) for label in CLASSES]
        ordered = sorted(probs, reverse=True)
        matrix[index, 0:4] = probs
        matrix[index, 4] = ordered[0] - ordered[1] if len(ordered) > 1 else ordered[0]
        matrix[index, 5] = entropy(probs)
        matrix[index, 6] = max(probs)
        matrix[index, 7] = float(row.get("family_similarity", 0.0))
        matrix[index, 8] = float(row.get("competing_similarity", 0.0))
        matrix[index, 9] = float(row.get("contrast_margin", 0.0))
        prediction = str(row["prediction"])
        if prediction in class_index:
            matrix[index, 10 + class_index[prediction]] = 1.0
        features = evidence.get(str(row["id"]), {})
        for offset, key in enumerate(evidence_keys, 14):
            value = features.get(key, 0.0)
            if isinstance(value, (int, float)) and math.isfinite(float(value)):
                matrix[index, offset] = float(value)
    return matrix


def select_keep_threshold(
    scores,
    correct,
    *,
    train_total: int,
    legacy_accept_count: int,
    target_precision: float,
    max_accept_coverage_loss: float,
    min_accept_examples: int,
    np,
):
    reports: list[dict[str, Any]] = []
    base_coverage = legacy_accept_count / max(1, train_total)
    for threshold in threshold_grid(scores, np):
        keep = scores >= float(threshold)
        count = int(keep.sum())
        precision = float(correct[keep].mean()) if count else 0.0
        coverage = count / max(1, train_total)
        eligible = bool(
            count >= min_accept_examples
            and precision >= target_precision
            and coverage >= base_coverage - max_accept_coverage_loss
        )
        reports.append(
            {
                "threshold": float(threshold),
                "eligible": eligible,
                "examples": count,
                "precision": precision,
                "coverage": coverage,
            }
        )
    eligible = [row for row in reports if row["eligible"]]
    if not eligible:
        return None, reports
    selected = max(
        eligible,
        key=lambda row: (row["coverage"], row["precision"], -row["threshold"]),
    )
    return {"threshold": float(selected["threshold"]), "fallback": False}, reports


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base_oof", type=Path, help="v5.2 oof_predictions.jsonl")
    parser.add_argument("v8_oof", type=Path, help="v8 oof_monotone_correctness_routing.jsonl")
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--target-accept-precision", type=float, default=0.97)
    parser.add_argument("--max-accept-coverage-loss", type=float, default=0.04)
    parser.add_argument("--min-class-meta-examples", type=int, default=100)
    parser.add_argument("--min-accept-examples", type=int, default=100)
    args = parser.parse_args()

    if not 0.0 <= args.target_accept_precision <= 1.0:
        raise SystemExit("--target-accept-precision must be in [0,1]")
    if not 0.0 <= args.max_accept_coverage_loss <= 1.0:
        raise SystemExit("--max-accept-coverage-loss must be in [0,1]")

    deps = _require_dependencies()
    np = deps["np"]
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
            raise ValueError(f"missing v8 fold for {record_id}")
        rows.append(
            {
                "id": record_id,
                "family_id": str(base_row.get("family_id", v8_row.get("family_id", ""))),
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
    legacy_actions = [str(row["legacy_action"]) for row in rows]
    unique_folds = sorted(set(folds.tolist()))
    if len(unique_folds) < 3:
        raise ValueError("need at least three outer folds")

    routed = list(legacy_actions)
    scores_out = np.full(len(rows), np.nan, dtype=np.float64)
    selected_thresholds: list[dict[str, Any]] = []
    fold_reports: list[dict[str, Any]] = []

    for target_fold in unique_folds:
        train_all = np.where(folds != target_fold)[0]
        train_accept = np.asarray(
            [idx for idx in train_all.tolist() if legacy_actions[idx] == "accept"], dtype=np.int64
        )
        test_accept = np.asarray(
            [idx for idx in np.where(folds == target_fold)[0].tolist() if legacy_actions[idx] == "accept"],
            dtype=np.int64,
        )
        if len(train_accept) < args.min_accept_examples or not len(test_accept):
            selected_thresholds.append({"fold": target_fold, "threshold": None, "fallback": True})
            continue

        train_fold_values = sorted(set(folds[train_accept].tolist()))
        meta_oof = np.full(len(train_accept), 0.5, dtype=np.float64)
        for calibration_fold in train_fold_values:
            fit_local = np.where(folds[train_accept] != calibration_fold)[0]
            valid_local = np.where(folds[train_accept] == calibration_fold)[0]
            if not len(fit_local) or not len(valid_local):
                continue
            fit_global = train_accept[fit_local]
            valid_global = train_accept[valid_local]
            targets = correct[fit_global].astype(np.int64)
            models = fit_meta_models(
                matrix[fit_global],
                predictions[fit_global],
                targets,
                seed=args.seed + target_fold * 1000 + calibration_fold,
                min_class_examples=args.min_class_meta_examples,
            )
            meta_oof[valid_local] = predict_meta_models(
                models, matrix[valid_global], predictions[valid_global], np
            )

        selected, candidates = select_keep_threshold(
            meta_oof,
            correct[train_accept],
            train_total=len(train_all),
            legacy_accept_count=len(train_accept),
            target_precision=args.target_accept_precision,
            max_accept_coverage_loss=args.max_accept_coverage_loss,
            min_accept_examples=args.min_accept_examples,
            np=np,
        )
        if selected is None:
            selected_payload = {"fold": target_fold, "threshold": None, "fallback": True}
            selected_thresholds.append(selected_payload)
            fold_reports.append(
                {
                    "fold": target_fold,
                    "train_accept": len(train_accept),
                    "test_accept": len(test_accept),
                    "eligible_candidates": 0,
                    "selected": selected_payload,
                }
            )
            continue

        final_models = fit_meta_models(
            matrix[train_accept],
            predictions[train_accept],
            correct[train_accept].astype(np.int64),
            seed=args.seed + 900000 + target_fold,
            min_class_examples=args.min_class_meta_examples,
        )
        test_scores = predict_meta_models(
            final_models, matrix[test_accept], predictions[test_accept], np
        )
        scores_out[test_accept] = test_scores
        threshold = float(selected["threshold"])
        for global_index, score in zip(test_accept.tolist(), test_scores.tolist()):
            if float(score) < threshold:
                routed[global_index] = "escalate"

        selected_payload = {"fold": target_fold, **selected}
        selected_thresholds.append(selected_payload)
        fold_reports.append(
            {
                "fold": target_fold,
                "train_accept": len(train_accept),
                "test_accept": len(test_accept),
                "eligible_candidates": sum(bool(row["eligible"]) for row in candidates),
                "selected": selected_payload,
            }
        )

    legacy_summary = summarize_actions(legacy_actions, correct, np)
    routed_summary = summarize_actions(routed, correct, np)
    changed = [idx for idx in range(len(rows)) if routed[idx] != legacy_actions[idx]]
    downgraded_wrong = sum(not bool(correct[idx]) for idx in changed)
    downgraded_correct = len(changed) - downgraded_wrong
    promotions = sum(
        (legacy_actions[idx], routed[idx]) in {
            ("review", "accept"), ("escalate", "accept"), ("escalate", "review")
        }
        for idx in range(len(rows))
    )

    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    rows_path = output_dir / "oof_accept_error_routing.jsonl"
    summary_path = output_dir / "summary.json"
    with rows_path.open("w", encoding="utf-8") as handle:
        for idx, row in enumerate(rows):
            payload = {
                "format": FORMAT,
                "id": row["id"],
                "family_id": row["family_id"],
                "fold": row["fold"],
                "truth": row["truth"],
                "prediction": row["prediction"],
                "correct": row["correct"],
                "legacy_action": legacy_actions[idx],
                "v9_action": routed[idx],
                "accept_correctness_probability": (
                    None if not math.isfinite(float(scores_out[idx])) else float(scores_out[idx])
                ),
            }
            handle.write(json.dumps(payload, sort_keys=True) + "\n")

    summary = {
        "format": FORMAT,
        "training_feedback": False,
        "class_predictions_changed": False,
        "rows": len(rows),
        "evidence_feature_count": len(evidence_keys),
        "meta_feature_count": 14 + len(evidence_keys),
        "legacy_routing": legacy_summary,
        "v9_routing": routed_summary,
        "legacy_accept_attribution": accept_attribution(rows, legacy_actions),
        "v9_accept_attribution": accept_attribution(rows, routed),
        "selected_thresholds": selected_thresholds,
        "changed_actions": len(changed),
        "downgraded_wrong": downgraded_wrong,
        "downgraded_correct": downgraded_correct,
        "promotions": promotions,
        "folds": fold_reports,
        "rows_path": str(rows_path),
    }
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"format: {FORMAT}")
    print("rows:", len(rows))
    print("class predictions changed: no")
    print("promotions:", promotions)
    print("evidence features:", len(evidence_keys))
    print("meta features:", 14 + len(evidence_keys))
    print("selected thresholds:", selected_thresholds)
    print("legacy routing:", legacy_summary)
    print("v9 routing:", routed_summary)
    print("legacy accept attribution:", summary["legacy_accept_attribution"])
    print("v9 accept attribution:", summary["v9_accept_attribution"])
    print(
        "changed actions:", len(changed),
        "downgraded-wrong:", downgraded_wrong,
        "downgraded-correct:", downgraded_correct,
    )
    print("training feedback: no")
    print("rows:", rows_path)
    print("summary:", summary_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
