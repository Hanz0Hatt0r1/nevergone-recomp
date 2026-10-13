#!/usr/bin/env python3
"""Unbiased nested family-grouped evaluation of selective ML routing.

The outer fold is never used to fit the semantic classifier, confidence
calibrators, or accept/review/OOD thresholds. Each outer training partition
creates its own inner family-grouped OOF predictions; routing policy is learned
only from those inner predictions and then applied once to the held-out outer
fold.

This evaluator is task-agnostic and works for code_domain, subsystem, or future
heads that use NEVERGONE.ProductionClassifier/4 semantics.
"""
from __future__ import annotations

import argparse
from collections import Counter
from typing import Any

from nevergone_ood_similarity import build_family_prototypes, contrastive_family_scores
from train_nevergone_production_classifier import (
    _fit_isotonic,
    _load_dataset,
    _make_model,
    _require_dependencies,
    _selective_summary,
    calibrate_confidence,
    choose_precision_threshold,
)
from train_nevergone_production_classifier_v4 import route_action_with_contrastive_ood


def align_probabilities(model, probabilities, classes: list[str], np):
    model_classes = model.named_steps["classifier"].classes_.tolist()
    class_index = {label: index for index, label in enumerate(classes)}
    aligned = np.zeros((len(probabilities), len(classes)), dtype=np.float64)
    for source_index, label in enumerate(model_classes):
        if label in class_index:
            aligned[:, class_index[label]] = probabilities[:, source_index]
    return aligned


def collect_predictions(
    *,
    model,
    train_texts: list[str],
    train_labels: list[str],
    train_groups: list[str],
    test_texts: list[str],
    classes: list[str],
    np,
) -> dict[str, Any]:
    probabilities = model.predict_proba(test_texts)
    aligned = align_probabilities(model, probabilities, classes, np)
    pred_index = aligned.argmax(axis=1)
    predictions = np.asarray([classes[index] for index in pred_index], dtype=object)
    raw_conf = aligned.max(axis=1)

    vectorizer = model.named_steps["tfidf"]
    prototypes = build_family_prototypes(vectorizer, train_texts, train_labels, train_groups)
    scores = contrastive_family_scores(vectorizer, prototypes, test_texts, predictions.tolist())

    pred_similarity = np.asarray(
        [float(score["predicted_similarity"]) for score in scores], dtype=np.float64
    )
    competing_similarity = np.asarray(
        [float(score["competing_similarity"]) for score in scores], dtype=np.float64
    )
    margin = np.asarray([float(score["contrast_margin"]) for score in scores], dtype=np.float64)
    return {
        "predictions": predictions,
        "raw_confidence": raw_conf,
        "probabilities": aligned,
        "pred_similarity": pred_similarity,
        "competing_similarity": competing_similarity,
        "margin": margin,
    }


def learn_policy(
    *,
    predictions,
    raw_confidence,
    pred_similarity,
    margin,
    truth,
    classes: list[str],
    deps,
    accept_precision: float,
    review_precision: float,
    min_class_calibration_examples: int,
    min_class_threshold_examples: int,
    min_ood_threshold_examples: int,
) -> tuple[Any, dict[str, Any], dict[str, dict[str, Any]]]:
    np = deps["np"]
    correct = predictions == truth

    global_calibrator = deps["IsotonicRegression"](
        y_min=0.0, y_max=1.0, out_of_bounds="clip"
    )
    global_calibrator.fit(raw_confidence, correct.astype(np.float64))

    class_calibrators: dict[str, Any] = {}
    for label in classes:
        mask = predictions == label
        calibrator = _fit_isotonic(
            deps,
            raw_confidence[mask],
            correct[mask].astype(np.float64),
            min_examples=min_class_calibration_examples,
        )
        if calibrator is not None:
            class_calibrators[label] = calibrator

    calibrated = np.zeros(len(truth), dtype=np.float64)
    for index in range(len(truth)):
        calibrated[index], _ = calibrate_confidence(
            str(predictions[index]),
            float(raw_confidence[index]),
            global_calibrator=global_calibrator,
            class_calibrators=class_calibrators,
        )

    class_policies: dict[str, dict[str, Any]] = {}
    for label in classes:
        mask = predictions == label
        accept = choose_precision_threshold(
            calibrated[mask].tolist(),
            correct[mask].tolist(),
            target_precision=accept_precision,
            min_examples=min_class_threshold_examples,
        )
        review = choose_precision_threshold(
            calibrated[mask].tolist(),
            correct[mask].tolist(),
            target_precision=review_precision,
            min_examples=min_class_threshold_examples,
        )
        accept_threshold = float(accept["threshold"]) if accept else None
        review_threshold = float(review["threshold"]) if review else None
        if accept_threshold is not None and review_threshold is not None:
            accept_threshold = max(accept_threshold, review_threshold)

        confidence_candidates = mask.copy()
        if accept_threshold is None:
            confidence_candidates[:] = False
        else:
            confidence_candidates &= calibrated >= accept_threshold

        similarity_policy = None
        if int(confidence_candidates.sum()) >= min_ood_threshold_examples:
            similarity_policy = choose_precision_threshold(
                pred_similarity[confidence_candidates].tolist(),
                correct[confidence_candidates].tolist(),
                target_precision=accept_precision,
                min_examples=min_ood_threshold_examples,
            )
        similarity_threshold = (
            float(similarity_policy["threshold"]) if similarity_policy else None
        )

        contrast_candidates = confidence_candidates.copy()
        if similarity_threshold is None:
            contrast_candidates[:] = False
        else:
            contrast_candidates &= pred_similarity >= similarity_threshold

        contrast_policy = None
        if int(contrast_candidates.sum()) >= min_ood_threshold_examples:
            contrast_policy = choose_precision_threshold(
                margin[contrast_candidates].tolist(),
                correct[contrast_candidates].tolist(),
                target_precision=accept_precision,
                min_examples=min_ood_threshold_examples,
            )
        learned_margin = float(contrast_policy["threshold"]) if contrast_policy else None
        contrast_threshold = max(0.0, learned_margin) if learned_margin is not None else None

        class_policies[label] = {
            "accept_threshold": accept_threshold,
            "review_threshold": review_threshold,
            "ood_accept_threshold": similarity_threshold,
            "contrast_accept_threshold": contrast_threshold,
            "predicted_examples": int(mask.sum()),
            "empirical_precision": float(correct[mask].mean()) if int(mask.sum()) else 0.0,
        }

    return global_calibrator, class_calibrators, class_policies


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

    from pathlib import Path

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

    print("model: NevergoneNestedRoutingCV/1")
    print("task:", args.task)
    print("functions:", len(rows))
    print("families:", len(set(groups.tolist())))
    print("classes:", Counter(labels.tolist()))
    print()

    outer = deps["StratifiedGroupKFold"](
        n_splits=args.outer_folds, shuffle=True, random_state=args.seed
    )
    all_true: list[str] = []
    all_pred: list[str] = []
    all_actions: list[str] = []
    all_correct: list[bool] = []
    per_class_action: dict[str, Counter] = {label: Counter() for label in args.classes}

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

        outer_actions: list[str] = []
        outer_correct: list[bool] = []
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
            is_correct = str(prediction) == truth
            outer_actions.append(action)
            outer_correct.append(is_correct)
            per_class_action[str(prediction)][action] += 1

        predictions = result["predictions"]
        truth = labels[outer_test_idx]
        balanced = deps["balanced_accuracy_score"](truth, predictions)
        macro = deps["f1_score"](truth, predictions, average="macro", zero_division=0)
        selective = _selective_summary(outer_actions, np.asarray(outer_correct, dtype=bool), np)
        print(
            f"outer_fold={outer_fold} balanced={balanced:.4f} macro_f1={macro:.4f} "
            f"n={len(outer_test_idx)} actions={dict(Counter(outer_actions))} "
            f"selective={selective}"
        )

        all_true.extend(truth.tolist())
        all_pred.extend(predictions.tolist())
        all_actions.extend(outer_actions)
        all_correct.extend(outer_correct)

    correct_array = np.asarray(all_correct, dtype=bool)
    overall_balanced = deps["balanced_accuracy_score"](all_true, all_pred)
    overall_macro = deps["f1_score"](all_true, all_pred, average="macro", zero_division=0)
    overall_selective = _selective_summary(all_actions, correct_array, np)

    print()
    print(f"nested balanced: {overall_balanced:.4f}")
    print(f"nested macro_f1: {overall_macro:.4f}")
    print("nested actions:", dict(sorted(Counter(all_actions).items())))
    print("nested selective:", overall_selective)
    print("prediction/action by class:")
    for label in args.classes:
        print(f"  {label}: {dict(sorted(per_class_action[label].items()))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
