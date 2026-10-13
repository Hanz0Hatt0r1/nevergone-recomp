#!/usr/bin/env python3
"""Class-conditional audit of nested family-grouped selective routing.

This repeats the production-faithful nested routing evaluation, but records every
held-out outer-fold decision so accept/review precision can be inspected by
predicted class and for fold-to-fold stability.
"""
from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path

from nevergone_nested_routing_cv import collect_predictions, learn_policy
from nevergone_nested_selective_metrics import (
    summarize_accept_stability,
    summarize_class_actions,
)
from train_nevergone_production_classifier import (
    _load_dataset,
    _make_model,
    _require_dependencies,
    _selective_summary,
    calibrate_confidence,
)
from train_nevergone_production_classifier_v4 import route_action_with_contrastive_ood


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir")
    parser.add_argument("families")
    parser.add_argument("--task", default="code_domain")
    parser.add_argument("--classes", nargs="+", required=True)
    parser.add_argument("--outer-folds", type=int, default=5)
    parser.add_argument("--inner-folds", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--accept-precision", type=float, default=0.95)
    parser.add_argument("--review-precision", type=float, default=0.85)
    parser.add_argument("--min-class-calibration-examples", type=int, default=25)
    parser.add_argument("--min-class-threshold-examples", type=int, default=10)
    parser.add_argument("--min-ood-threshold-examples", type=int, default=10)
    args = parser.parse_args()

    if args.outer_folds < 2 or args.inner_folds < 2:
        raise SystemExit("outer and inner folds must both be >= 2")
    if args.review_precision > args.accept_precision:
        raise SystemExit("--review-precision must be <= --accept-precision")

    deps = _require_dependencies()
    np = deps["np"]
    rows = _load_dataset(
        Path(args.dataset_dir), Path(args.families), task=args.task, classes=args.classes
    )
    texts = np.asarray([row["text"] for row in rows], dtype=object)
    labels = np.asarray([row["label"] for row in rows], dtype=object)
    groups = np.asarray([row["family_id"] for row in rows], dtype=object)

    print("model: NevergoneNestedRoutingAudit/1")
    print("task:", args.task)
    print("functions:", len(rows))
    print("families:", len(set(groups.tolist())))
    print("classes:", Counter(labels.tolist()))
    print()

    outer = deps["StratifiedGroupKFold"](
        n_splits=args.outer_folds, shuffle=True, random_state=args.seed
    )
    records: list[dict[str, object]] = []

    for outer_fold, (outer_train_idx, outer_test_idx) in enumerate(
        outer.split(texts, labels, groups), 1
    ):
        inner_texts = texts[outer_train_idx]
        inner_labels = labels[outer_train_idx]
        inner_groups = groups[outer_train_idx]

        inner_pred = np.empty(len(outer_train_idx), dtype=object)
        inner_raw = np.zeros(len(outer_train_idx), dtype=np.float64)
        inner_sim = np.zeros(len(outer_train_idx), dtype=np.float64)
        inner_margin = np.zeros(len(outer_train_idx), dtype=np.float64)

        inner = deps["StratifiedGroupKFold"](
            n_splits=args.inner_folds,
            shuffle=True,
            random_state=args.seed + 1000 + outer_fold,
        )
        for inner_fold, (fit_idx, policy_idx) in enumerate(
            inner.split(inner_texts, inner_labels, inner_groups), 1
        ):
            model = _make_model(deps, args.seed + 100 * outer_fold + inner_fold)
            fit_text = inner_texts[fit_idx].tolist()
            fit_label = inner_labels[fit_idx].tolist()
            fit_group = inner_groups[fit_idx].tolist()
            policy_text = inner_texts[policy_idx].tolist()
            model.fit(fit_text, fit_label)
            result = collect_predictions(
                model=model,
                train_texts=fit_text,
                train_labels=fit_label,
                train_groups=fit_group,
                test_texts=policy_text,
                classes=args.classes,
                np=np,
            )
            inner_pred[policy_idx] = result["predictions"]
            inner_raw[policy_idx] = result["raw_confidence"]
            inner_sim[policy_idx] = result["pred_similarity"]
            inner_margin[policy_idx] = result["margin"]

        global_calibrator, class_calibrators, policies = learn_policy(
            predictions=inner_pred,
            raw_confidence=inner_raw,
            pred_similarity=inner_sim,
            margin=inner_margin,
            truth=inner_labels,
            classes=args.classes,
            deps=deps,
            accept_precision=args.accept_precision,
            review_precision=args.review_precision,
            min_class_calibration_examples=args.min_class_calibration_examples,
            min_class_threshold_examples=args.min_class_threshold_examples,
            min_ood_threshold_examples=args.min_ood_threshold_examples,
        )

        model = _make_model(deps, args.seed + outer_fold)
        train_text = texts[outer_train_idx].tolist()
        train_label = labels[outer_train_idx].tolist()
        train_group = groups[outer_train_idx].tolist()
        test_text = texts[outer_test_idx].tolist()
        model.fit(train_text, train_label)
        result = collect_predictions(
            model=model,
            train_texts=train_text,
            train_labels=train_label,
            train_groups=train_group,
            test_texts=test_text,
            classes=args.classes,
            np=np,
        )

        fold_actions: list[str] = []
        fold_correct: list[bool] = []
        for local_index, prediction in enumerate(result["predictions"]):
            confidence, _ = calibrate_confidence(
                str(prediction),
                float(result["raw_confidence"][local_index]),
                global_calibrator=global_calibrator,
                class_calibrators=class_calibrators,
            )
            action = route_action_with_contrastive_ood(
                str(prediction),
                float(confidence),
                float(result["pred_similarity"][local_index]),
                float(result["margin"][local_index]),
                policies,
            )
            truth = str(labels[outer_test_idx[local_index]])
            correct = str(prediction) == truth
            fold_actions.append(action)
            fold_correct.append(correct)
            records.append(
                {
                    "outer_fold": outer_fold,
                    "prediction": str(prediction),
                    "truth": truth,
                    "action": action,
                    "correct": correct,
                    "confidence": float(confidence),
                    "similarity": float(result["pred_similarity"][local_index]),
                    "margin": float(result["margin"][local_index]),
                }
            )

        fold_summary = _selective_summary(
            fold_actions, np.asarray(fold_correct, dtype=bool), np
        )
        print(
            f"outer_fold={outer_fold} actions={dict(Counter(fold_actions))} "
            f"selective={fold_summary}"
        )

    action_summary = summarize_class_actions(records, args.classes)
    stability = summarize_accept_stability(records, args.classes, args.outer_folds)

    print()
    print("nested selective by predicted class:")
    for label in args.classes:
        print(f"  {label}:")
        for action in ("accept", "review", "escalate"):
            stats = action_summary[label][action]
            print(
                f"    {action}: examples={stats['examples']} correct={stats['correct']} "
                f"precision={stats['precision']:.4f}"
            )

    print("nested accept stability by predicted class:")
    for label in args.classes:
        stats = stability[label]
        minimum = stats["min_fold_precision"]
        mean = stats["mean_fold_precision"]
        min_text = "None" if minimum is None else f"{float(minimum):.4f}"
        mean_text = "None" if mean is None else f"{float(mean):.4f}"
        print(
            f"  {label}: accept_examples={stats['accept_examples']} "
            f"folds_with_accept={stats['folds_with_accept']}/{stats['total_folds']} "
            f"min_fold_precision={min_text} mean_fold_precision={mean_text}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
