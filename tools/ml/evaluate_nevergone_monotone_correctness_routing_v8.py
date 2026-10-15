#!/usr/bin/env python3
"""Nested monotone correctness routing for ProductionClassifier v8.

v7 proved that an all-class correctness score can exceed the 0.97 accept-precision
target, but it lost too much coverage because it rebuilt routing from scratch and
could promote legacy REVIEW/ESCALATE rows into ACCEPT. v8 keeps the same nested
base/meta training protocol and changes only routing policy: meta evidence may
*only downgrade* the leakage-safe legacy action.

Outer labels never participate in base fitting, meta fitting, or threshold
selection. Thresholds are selected from meta-OOF predictions inside each outer
training partition.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from evaluate_nevergone_correctness_meta_routing_v7 import (
    ACTIONS,
    BLOCKED_EVIDENCE_TERMS,
    CLASSES,
    accept_attribution,
    build_correctness_matrix,
    class_metrics,
    fit_meta_models,
    predict_meta_models,
    safe_evidence_keys,
    summarize_actions,
    threshold_grid,
)
from evaluate_nevergone_selective_game_verifier_v54 import read_oof, safe_split_count
from nevergone_nested_routing_cv import collect_predictions, learn_policy
from train_nevergone_production_classifier import (
    _load_dataset,
    _make_model,
    _require_dependencies,
    calibrate_confidence,
)
from train_nevergone_production_classifier_v4 import route_action_with_contrastive_ood
from train_nevergone_production_classifier_v5 import augment_text, load_evidence

FORMAT = "NEVERGONE.MonotoneCorrectnessRoutingV8/1"


def monotone_actions(
    legacy_actions: list[str],
    scores,
    accept_threshold: float,
    review_threshold: float,
) -> list[str]:
    """Use meta evidence only to make the legacy action more conservative."""
    result: list[str] = []
    for legacy, raw_score in zip(legacy_actions, scores):
        score = float(raw_score)
        if legacy == "escalate":
            result.append("escalate")
            continue
        if legacy == "review":
            result.append("review" if score >= review_threshold else "escalate")
            continue
        if score >= accept_threshold:
            result.append("accept")
        elif score >= review_threshold:
            result.append("review")
        else:
            result.append("escalate")
    return result


def select_monotone_thresholds(
    legacy_actions: list[str],
    scores,
    correct,
    *,
    np,
    target_accept_precision: float,
    target_review_precision: float,
    max_accept_coverage_loss: float,
    min_accept_examples: int,
) -> tuple[dict[str, Any] | None, list[dict[str, Any]]]:
    base = summarize_actions(legacy_actions, correct, np)
    base_accept_coverage = float(base["accept"]["coverage"])
    grid = threshold_grid(scores, np)
    reports: list[dict[str, Any]] = []
    for review_threshold in grid:
        for accept_threshold in grid:
            if accept_threshold < review_threshold:
                continue
            actions = monotone_actions(
                legacy_actions,
                scores,
                float(accept_threshold),
                float(review_threshold),
            )
            summary = summarize_actions(actions, correct, np)
            accept = summary["accept"]
            review = summary["review"]
            eligible = bool(
                accept["examples"] >= min_accept_examples
                and accept["precision"] >= target_accept_precision
                and accept["coverage"] >= base_accept_coverage - max_accept_coverage_loss
                and (review["examples"] == 0 or review["precision"] >= target_review_precision)
            )
            reports.append(
                {
                    "accept_threshold": float(accept_threshold),
                    "review_threshold": float(review_threshold),
                    "eligible": eligible,
                    "actions": summary,
                }
            )
    eligible = [row for row in reports if row["eligible"]]
    if not eligible:
        return None, reports
    selected = max(
        eligible,
        key=lambda row: (
            row["actions"]["accept"]["coverage"],
            row["actions"]["accept"]["precision"],
            row["actions"]["review"]["precision"],
            -row["actions"]["escalate"]["coverage"],
            -row["accept_threshold"],
            -row["review_threshold"],
        ),
    )
    return {
        "accept_threshold": float(selected["accept_threshold"]),
        "review_threshold": float(selected["review_threshold"]),
        "fallback": False,
    }, reports


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("base_oof", type=Path, help="v5.2 OOF predictions; reproduction audit only")
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--outer-folds", type=int, default=5)
    parser.add_argument("--inner-folds", type=int, default=4)
    parser.add_argument("--meta-folds", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--legacy-accept-precision", type=float, default=0.95)
    parser.add_argument("--legacy-review-precision", type=float, default=0.85)
    parser.add_argument("--target-accept-precision", type=float, default=0.97)
    parser.add_argument("--target-review-precision", type=float, default=0.80)
    parser.add_argument("--max-accept-coverage-loss", type=float, default=0.03)
    parser.add_argument("--min-class-calibration-examples", type=int, default=25)
    parser.add_argument("--min-class-threshold-examples", type=int, default=10)
    parser.add_argument("--min-ood-threshold-examples", type=int, default=10)
    parser.add_argument("--min-class-meta-examples", type=int, default=100)
    parser.add_argument("--min-accept-examples", type=int, default=50)
    args = parser.parse_args()

    if min(args.outer_folds, args.inner_folds, args.meta_folds) < 2:
        raise SystemExit("outer, inner, and meta folds must be >= 2")
    if args.target_review_precision > args.target_accept_precision:
        raise SystemExit("--target-review-precision must be <= --target-accept-precision")
    if not 0.0 <= args.max_accept_coverage_loss <= 1.0:
        raise SystemExit("--max-accept-coverage-loss must be in [0, 1]")

    deps = _require_dependencies()
    np = deps["np"]
    rows = _load_dataset(args.dataset_dir, args.families, task="code_domain", classes=list(CLASSES))
    evidence = load_evidence(args.evidence)
    reference_oof = read_oof(args.base_oof)

    texts = np.asarray(
        [augment_text(row["text"], evidence.get(row["id"], {}))[0] for row in rows],
        dtype=object,
    )
    labels = np.asarray([row["label"] for row in rows], dtype=object)
    groups = np.asarray([row["family_id"] for row in rows], dtype=object)
    ids = [str(row["id"]) for row in rows]
    evidence_keys = safe_evidence_keys(evidence)
    missing = [record_id for record_id in ids if record_id not in reference_oof]
    if missing:
        raise ValueError(f"base OOF is missing {len(missing)} rows; first={missing[0]}")

    outer_folds = safe_split_count(labels.tolist(), groups.tolist(), args.outer_folds)
    outer = deps["StratifiedGroupKFold"](
        n_splits=outer_folds, shuffle=True, random_state=args.seed
    )

    output_rows: list[dict[str, Any] | None] = [None] * len(rows)
    selected_thresholds: list[dict[str, Any]] = []
    fold_reports: list[dict[str, Any]] = []

    for outer_fold, (outer_train, outer_test) in enumerate(outer.split(texts, labels, groups), 1):
        train_labels = labels[outer_train]
        train_groups = groups[outer_train]
        inner_folds = safe_split_count(train_labels.tolist(), train_groups.tolist(), args.inner_folds)
        inner = deps["StratifiedGroupKFold"](
            n_splits=inner_folds,
            shuffle=True,
            random_state=args.seed + 8000 + outer_fold,
        )
        m = len(outer_train)
        inner_pred = np.empty(m, dtype=object)
        inner_raw = np.zeros(m, dtype=np.float64)
        inner_probs = np.zeros((m, len(CLASSES)), dtype=np.float64)
        inner_pred_sim = np.zeros(m, dtype=np.float64)
        inner_comp_sim = np.zeros(m, dtype=np.float64)
        inner_margin = np.zeros(m, dtype=np.float64)

        for inner_fold, (fit_local, valid_local) in enumerate(
            inner.split(texts[outer_train], train_labels, train_groups), 1
        ):
            fit_global = outer_train[fit_local]
            valid_global = outer_train[valid_local]
            model = _make_model(deps, args.seed + outer_fold * 100 + inner_fold)
            fit_texts = texts[fit_global].tolist()
            fit_labels = labels[fit_global].tolist()
            fit_groups = groups[fit_global].tolist()
            model.fit(fit_texts, fit_labels)
            result = collect_predictions(
                model=model,
                train_texts=fit_texts,
                train_labels=fit_labels,
                train_groups=fit_groups,
                test_texts=texts[valid_global].tolist(),
                classes=list(CLASSES),
                np=np,
            )
            inner_pred[valid_local] = result["predictions"]
            inner_raw[valid_local] = result["raw_confidence"]
            inner_probs[valid_local] = result["probabilities"]
            inner_pred_sim[valid_local] = result["pred_similarity"]
            inner_comp_sim[valid_local] = result["competing_similarity"]
            inner_margin[valid_local] = result["margin"]

        global_calibrator, class_calibrators, legacy_policies = learn_policy(
            predictions=inner_pred,
            raw_confidence=inner_raw,
            pred_similarity=inner_pred_sim,
            margin=inner_margin,
            truth=train_labels,
            classes=list(CLASSES),
            deps=deps,
            accept_precision=args.legacy_accept_precision,
            review_precision=args.legacy_review_precision,
            min_class_calibration_examples=args.min_class_calibration_examples,
            min_class_threshold_examples=args.min_class_threshold_examples,
            min_ood_threshold_examples=args.min_ood_threshold_examples,
        )

        inner_legacy_actions: list[str] = []
        for index, prediction in enumerate(inner_pred):
            confidence, _ = calibrate_confidence(
                str(prediction),
                float(inner_raw[index]),
                global_calibrator=global_calibrator,
                class_calibrators=class_calibrators,
            )
            inner_legacy_actions.append(
                route_action_with_contrastive_ood(
                    str(prediction),
                    float(confidence),
                    float(inner_pred_sim[index]),
                    float(inner_margin[index]),
                    legacy_policies,
                )
            )

        inner_ids = [ids[index] for index in outer_train.tolist()]
        inner_matrix = build_correctness_matrix(
            inner_ids,
            inner_probs,
            inner_pred,
            inner_pred_sim,
            inner_comp_sim,
            inner_margin,
            evidence,
            evidence_keys,
            np,
        )
        inner_targets = (inner_pred == train_labels).astype(np.int64)
        meta_folds = safe_split_count(
            [str(value) for value in inner_targets.tolist()],
            train_groups.tolist(),
            args.meta_folds,
        )
        meta = deps["StratifiedGroupKFold"](
            n_splits=meta_folds,
            shuffle=True,
            random_state=args.seed + 8800 + outer_fold,
        )
        meta_oof = np.full(m, 0.5, dtype=np.float64)
        for meta_fold, (meta_fit, meta_valid) in enumerate(
            meta.split(inner_matrix, inner_targets, train_groups), 1
        ):
            models = fit_meta_models(
                inner_matrix[meta_fit],
                inner_pred[meta_fit],
                inner_targets[meta_fit],
                seed=args.seed + outer_fold * 1000 + meta_fold,
                min_class_examples=args.min_class_meta_examples,
            )
            meta_oof[meta_valid] = predict_meta_models(
                models,
                inner_matrix[meta_valid],
                inner_pred[meta_valid],
                np,
            )

        selected, candidates = select_monotone_thresholds(
            inner_legacy_actions,
            meta_oof,
            inner_targets.astype(bool),
            np=np,
            target_accept_precision=args.target_accept_precision,
            target_review_precision=args.target_review_precision,
            max_accept_coverage_loss=args.max_accept_coverage_loss,
            min_accept_examples=args.min_accept_examples,
        )
        final_meta_models = fit_meta_models(
            inner_matrix,
            inner_pred,
            inner_targets,
            seed=args.seed + 800000 + outer_fold,
            min_class_examples=args.min_class_meta_examples,
        )

        base_model = _make_model(deps, args.seed + outer_fold)
        train_texts = texts[outer_train].tolist()
        train_labels_list = labels[outer_train].tolist()
        train_groups_list = groups[outer_train].tolist()
        base_model.fit(train_texts, train_labels_list)
        outer_result = collect_predictions(
            model=base_model,
            train_texts=train_texts,
            train_labels=train_labels_list,
            train_groups=train_groups_list,
            test_texts=texts[outer_test].tolist(),
            classes=list(CLASSES),
            np=np,
        )
        outer_ids = [ids[index] for index in outer_test.tolist()]
        outer_matrix = build_correctness_matrix(
            outer_ids,
            outer_result["probabilities"],
            outer_result["predictions"],
            outer_result["pred_similarity"],
            outer_result["competing_similarity"],
            outer_result["margin"],
            evidence,
            evidence_keys,
            np,
        )
        outer_scores = predict_meta_models(
            final_meta_models, outer_matrix, outer_result["predictions"], np
        )

        legacy_actions: list[str] = []
        for local_index, prediction in enumerate(outer_result["predictions"]):
            confidence, _ = calibrate_confidence(
                str(prediction),
                float(outer_result["raw_confidence"][local_index]),
                global_calibrator=global_calibrator,
                class_calibrators=class_calibrators,
            )
            legacy_actions.append(
                route_action_with_contrastive_ood(
                    str(prediction),
                    float(confidence),
                    float(outer_result["pred_similarity"][local_index]),
                    float(outer_result["margin"][local_index]),
                    legacy_policies,
                )
            )

        if selected is None:
            routed_actions = list(legacy_actions)
            selected_payload = {
                "fold": outer_fold,
                "fallback": True,
                "accept_threshold": None,
                "review_threshold": None,
            }
        else:
            routed_actions = monotone_actions(
                legacy_actions,
                outer_scores,
                float(selected["accept_threshold"]),
                float(selected["review_threshold"]),
            )
            selected_payload = {"fold": outer_fold, **selected}
        selected_thresholds.append(selected_payload)

        outer_correct = outer_result["predictions"] == labels[outer_test]
        fold_reports.append(
            {
                "fold": outer_fold,
                "train_examples": int(len(outer_train)),
                "test_examples": int(len(outer_test)),
                "eligible_candidates": sum(bool(row["eligible"]) for row in candidates),
                "selected": selected_payload,
                "legacy": summarize_actions(legacy_actions, outer_correct, np),
                "monotone": summarize_actions(routed_actions, outer_correct, np),
            }
        )

        for local_index, global_index in enumerate(outer_test.tolist()):
            prediction = str(outer_result["predictions"][local_index])
            truth = str(labels[global_index])
            output_rows[global_index] = {
                "format": FORMAT,
                "id": ids[global_index],
                "family_id": str(groups[global_index]),
                "fold": outer_fold,
                "truth": truth,
                "prediction": prediction,
                "correct": prediction == truth,
                "correctness_probability": float(outer_scores[local_index]),
                "legacy_action": legacy_actions[local_index],
                "monotone_action": routed_actions[local_index],
            }

    if any(row is None for row in output_rows):
        raise RuntimeError("not every dataset row received an outer-fold result")
    final_rows = [row for row in output_rows if row is not None]
    correct = np.asarray([bool(row["correct"]) for row in final_rows], dtype=bool)
    legacy_actions = [str(row["legacy_action"]) for row in final_rows]
    monotone = [str(row["monotone_action"]) for row in final_rows]
    base_predictions = [str(row["prediction"]) for row in final_rows]

    reproduction_matches = sum(
        base_predictions[index] == str(reference_oof[ids[index]].get("prediction"))
        for index in range(len(ids))
    )
    reproduction_rate = reproduction_matches / max(1, len(ids))
    legacy_summary = summarize_actions(legacy_actions, correct, np)
    monotone_summary = summarize_actions(monotone, correct, np)
    changed = [i for i in range(len(final_rows)) if legacy_actions[i] != monotone[i]]
    promotions = sum(ACTIONS.index(monotone[i]) < ACTIONS.index(legacy_actions[i]) for i in changed)
    downgraded_wrong = sum(not bool(final_rows[i]["correct"]) for i in changed)
    downgraded_correct = len(changed) - downgraded_wrong

    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    rows_path = output_dir / "oof_monotone_correctness_routing.jsonl"
    summary_path = output_dir / "summary.json"
    with rows_path.open("w", encoding="utf-8") as handle:
        for row in final_rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    summary = {
        "format": FORMAT,
        "training_feedback": False,
        "class_predictions_changed": False,
        "monotone_only": True,
        "rows": len(final_rows),
        "evidence_feature_count": len(evidence_keys),
        "meta_feature_count": 14 + len(evidence_keys),
        "blocked_evidence_terms": list(BLOCKED_EVIDENCE_TERMS),
        "class_metrics": class_metrics(final_rows, deps),
        "legacy_routing": legacy_summary,
        "monotone_routing": monotone_summary,
        "legacy_accept_attribution": accept_attribution(final_rows, legacy_actions),
        "monotone_accept_attribution": accept_attribution(final_rows, monotone),
        "selected_thresholds": selected_thresholds,
        "changed_actions": len(changed),
        "promotions": promotions,
        "downgraded_wrong": downgraded_wrong,
        "downgraded_correct": downgraded_correct,
        "folds": fold_reports,
        "v52_oof_reproduction_rate": reproduction_rate,
        "rows_path": str(rows_path),
    }
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"format: {FORMAT}")
    print("rows:", len(final_rows))
    print("class predictions changed: no")
    print("monotone promotions:", promotions)
    print("evidence features:", len(evidence_keys))
    print("meta features:", 14 + len(evidence_keys))
    print("class metrics:", summary["class_metrics"])
    print("selected thresholds:", selected_thresholds)
    print("legacy routing:", legacy_summary)
    print("monotone routing:", monotone_summary)
    print("legacy accept attribution:", summary["legacy_accept_attribution"])
    print("monotone accept attribution:", summary["monotone_accept_attribution"])
    print(
        "changed actions:", len(changed),
        "downgraded-wrong:", downgraded_wrong,
        "downgraded-correct:", downgraded_correct,
    )
    print(f"v5.2 OOF reproduction rate: {reproduction_rate:.4f}")
    print("training feedback: no")
    print("rows:", rows_path)
    print("summary:", summary_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
