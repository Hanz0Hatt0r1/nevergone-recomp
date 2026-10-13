#!/usr/bin/env python3
"""Train ProductionClassifier/3 with classwise confidence + OOD family gate.

The semantic classifier is unchanged from v2:
portable P-code -> TF-IDF unigram/bigram -> balanced logistic regression.

Version 3 adds a second requirement for automatic acceptance: a function must be
sufficiently similar to a held-out-known function family of its predicted class.
Similarity thresholds are estimated only from family-grouped OOF predictions.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

from nevergone_ood_similarity import (
    build_family_prototypes,
    nearest_family_by_predicted_class,
)
from train_nevergone_production_classifier import (
    _fit_isotonic,
    _load_dataset,
    _make_model,
    _require_dependencies,
    _selective_summary,
    _sha256_rows,
    calibrate_confidence,
    choose_precision_threshold,
    route_action_for_class,
)

ARTIFACT_FORMAT_V3 = "NEVERGONE.ProductionClassifier/3"


def route_action_with_ood(
    prediction: str,
    confidence: float,
    similarity: float,
    class_policies: dict[str, dict[str, Any]],
) -> str:
    """Apply v2 classwise routing, then require OOD support for auto-accept."""
    base_action = route_action_for_class(prediction, confidence, class_policies)
    if base_action != "accept":
        return base_action

    policy = class_policies.get(prediction, {})
    similarity_threshold = policy.get("ood_accept_threshold")
    if similarity_threshold is not None and similarity >= float(similarity_threshold):
        return "accept"

    review_threshold = policy.get("review_threshold")
    if review_threshold is not None and confidence >= float(review_threshold):
        return "review"
    return "escalate"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--task", default="code_domain")
    parser.add_argument("--classes", nargs="+", required=True)
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--accept-precision", type=float, default=0.95)
    parser.add_argument("--review-precision", type=float, default=0.85)
    parser.add_argument("--min-class-calibration-examples", type=int, default=25)
    parser.add_argument("--min-class-threshold-examples", type=int, default=10)
    parser.add_argument("--min-ood-threshold-examples", type=int, default=10)
    args = parser.parse_args()

    if args.folds < 2:
        raise SystemExit("--folds must be >= 2")
    if args.review_precision > args.accept_precision:
        raise SystemExit("--review-precision must be <= --accept-precision")
    if args.min_class_calibration_examples < 2:
        raise SystemExit("--min-class-calibration-examples must be >= 2")
    if args.min_class_threshold_examples < 1 or args.min_ood_threshold_examples < 1:
        raise SystemExit("threshold minimums must be >= 1")

    deps = _require_dependencies()
    np = deps["np"]
    rows = _load_dataset(
        args.dataset_dir,
        args.families,
        task=args.task,
        classes=args.classes,
    )
    texts = np.asarray([row["text"] for row in rows], dtype=object)
    labels = np.asarray([row["label"] for row in rows], dtype=object)
    groups = np.asarray([row["family_id"] for row in rows], dtype=object)

    print(f"format: {ARTIFACT_FORMAT_V3}")
    print("functions:", len(rows))
    print("families:", len(set(groups.tolist())))
    print("classes:", Counter(labels.tolist()))
    print()

    splitter = deps["StratifiedGroupKFold"](
        n_splits=args.folds,
        shuffle=True,
        random_state=args.seed,
    )
    class_index = {label: index for index, label in enumerate(args.classes)}
    oof_pred = np.empty(len(rows), dtype=object)
    oof_raw_conf = np.zeros(len(rows), dtype=np.float64)
    oof_probs = np.zeros((len(rows), len(args.classes)), dtype=np.float64)
    oof_similarity = np.zeros(len(rows), dtype=np.float64)
    oof_nearest_family = np.empty(len(rows), dtype=object)
    oof_nearest_family[:] = None

    for fold, (train_idx, test_idx) in enumerate(
        splitter.split(texts, labels, groups), 1
    ):
        model = _make_model(deps, args.seed + fold)
        train_texts = texts[train_idx].tolist()
        train_labels = labels[train_idx].tolist()
        train_groups = groups[train_idx].tolist()
        test_texts = texts[test_idx].tolist()

        model.fit(train_texts, train_labels)
        probabilities = model.predict_proba(test_texts)
        model_classes = model.named_steps["classifier"].classes_.tolist()
        aligned = np.zeros((len(test_idx), len(args.classes)), dtype=np.float64)
        for source_index, label in enumerate(model_classes):
            aligned[:, class_index[label]] = probabilities[:, source_index]

        pred_index = aligned.argmax(axis=1)
        predictions = np.asarray(
            [args.classes[index] for index in pred_index], dtype=object
        )
        oof_pred[test_idx] = predictions
        oof_probs[test_idx] = aligned
        oof_raw_conf[test_idx] = aligned.max(axis=1)

        vectorizer = model.named_steps["tfidf"]
        prototypes = build_family_prototypes(
            vectorizer,
            train_texts,
            train_labels,
            train_groups,
        )
        nearest = nearest_family_by_predicted_class(
            vectorizer,
            prototypes,
            test_texts,
            predictions.tolist(),
        )
        for local_index, global_index in enumerate(test_idx):
            similarity, family_id = nearest[local_index]
            oof_similarity[global_index] = similarity
            oof_nearest_family[global_index] = family_id

        fold_balanced = deps["balanced_accuracy_score"](labels[test_idx], predictions)
        fold_macro = deps["f1_score"](
            labels[test_idx], predictions, average="macro", zero_division=0
        )
        print(
            f"fold={fold} balanced={fold_balanced:.4f} macro_f1={fold_macro:.4f} "
            f"n={len(test_idx)} families={len(set(groups[test_idx].tolist()))}"
        )

    correct = oof_pred == labels
    global_calibrator = deps["IsotonicRegression"](
        y_min=0.0,
        y_max=1.0,
        out_of_bounds="clip",
    )
    global_calibrator.fit(oof_raw_conf, correct.astype(np.float64))

    class_calibrators: dict[str, Any] = {}
    for label in args.classes:
        mask = oof_pred == label
        calibrator = _fit_isotonic(
            deps,
            oof_raw_conf[mask],
            correct[mask].astype(np.float64),
            min_examples=args.min_class_calibration_examples,
        )
        if calibrator is not None:
            class_calibrators[label] = calibrator

    calibrated = np.zeros(len(rows), dtype=np.float64)
    confidence_sources: list[str] = []
    for index in range(len(rows)):
        value, source = calibrate_confidence(
            str(oof_pred[index]),
            float(oof_raw_conf[index]),
            global_calibrator=global_calibrator,
            class_calibrators=class_calibrators,
        )
        calibrated[index] = value
        confidence_sources.append(source)

    class_policies: dict[str, dict[str, Any]] = {}
    for label in args.classes:
        mask = oof_pred == label
        class_conf = calibrated[mask].tolist()
        class_correct = correct[mask].tolist()

        accept = choose_precision_threshold(
            class_conf,
            class_correct,
            target_precision=args.accept_precision,
            min_examples=args.min_class_threshold_examples,
        )
        review = choose_precision_threshold(
            class_conf,
            class_correct,
            target_precision=args.review_precision,
            min_examples=args.min_class_threshold_examples,
        )
        accept_threshold = float(accept["threshold"]) if accept else None
        review_threshold = float(review["threshold"]) if review else None
        if accept_threshold is not None and review_threshold is not None:
            accept_threshold = max(accept_threshold, review_threshold)

        accept_candidate_mask = mask.copy()
        if accept_threshold is None:
            accept_candidate_mask[:] = False
        else:
            accept_candidate_mask &= calibrated >= accept_threshold

        ood_accept = None
        if int(accept_candidate_mask.sum()) >= args.min_ood_threshold_examples:
            ood_accept = choose_precision_threshold(
                oof_similarity[accept_candidate_mask].tolist(),
                correct[accept_candidate_mask].tolist(),
                target_precision=args.accept_precision,
                min_examples=args.min_ood_threshold_examples,
            )
        ood_accept_threshold = (
            float(ood_accept["threshold"]) if ood_accept is not None else None
        )

        predicted_examples = int(mask.sum())
        class_policies[label] = {
            "predicted_examples": predicted_examples,
            "oof_precision": float(correct[mask].mean()) if predicted_examples else 0.0,
            "confidence_calibrator": (
                "classwise" if label in class_calibrators else "global_fallback"
            ),
            "accept": accept,
            "review": review,
            "accept_threshold": accept_threshold,
            "review_threshold": review_threshold,
            "ood_accept": ood_accept,
            "ood_accept_threshold": ood_accept_threshold,
            "ood_accept_candidate_examples": int(accept_candidate_mask.sum()),
        }

    actions = [
        route_action_with_ood(
            str(oof_pred[index]),
            float(calibrated[index]),
            float(oof_similarity[index]),
            class_policies,
        )
        for index in range(len(rows))
    ]
    action_counts = Counter(actions)
    selective = _selective_summary(actions, correct, np)

    overall = {
        "accuracy": float(deps["accuracy_score"](labels, oof_pred)),
        "balanced_accuracy": float(deps["balanced_accuracy_score"](labels, oof_pred)),
        "macro_f1": float(
            deps["f1_score"](labels, oof_pred, average="macro", zero_division=0)
        ),
        "raw_confidence_brier": float(
            np.mean((oof_raw_conf - correct.astype(np.float64)) ** 2)
        ),
        "calibrated_confidence_brier": float(
            np.mean((calibrated - correct.astype(np.float64)) ** 2)
        ),
    }

    print()
    print(
        "OOF metrics:",
        f"accuracy={overall['accuracy']:.4f}",
        f"balanced={overall['balanced_accuracy']:.4f}",
        f"macro_f1={overall['macro_f1']:.4f}",
    )
    print("class policies + OOD:")
    for label in args.classes:
        policy = class_policies[label]
        print(
            f"  {label}: predicted={policy['predicted_examples']} "
            f"precision={policy['oof_precision']:.4f} "
            f"accept={policy['accept_threshold']} review={policy['review_threshold']} "
            f"ood_accept={policy['ood_accept_threshold']} "
            f"ood_candidates={policy['ood_accept_candidate_examples']}"
        )
    print("OOF actions:", dict(sorted(action_counts.items())))
    print("selective OOF:", selective)

    final_model = _make_model(deps, args.seed)
    final_model.fit(texts.tolist(), labels.tolist())
    final_vectorizer = final_model.named_steps["tfidf"]
    family_prototypes = build_family_prototypes(
        final_vectorizer,
        texts.tolist(),
        labels.tolist(),
        groups.tolist(),
    )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    artifact_path = args.output_dir / "classifier.joblib"
    deps["joblib"].dump(
        {
            "format": ARTIFACT_FORMAT_V3,
            "task": args.task,
            "classes": args.classes,
            "model": final_model,
            "confidence_calibrator": global_calibrator,
            "class_confidence_calibrators": class_calibrators,
            "class_policies": class_policies,
            "family_prototypes": family_prototypes,
        },
        artifact_path,
    )

    manifest = {
        "format": ARTIFACT_FORMAT_V3,
        "task": args.task,
        "classes": args.classes,
        "functions": len(rows),
        "families": len(set(groups.tolist())),
        "prototype_families": len(family_prototypes["families"]),
        "class_distribution": dict(sorted(Counter(labels.tolist()).items())),
        "dataset_fingerprint_sha256": _sha256_rows(rows),
        "representation": "portable_pcode",
        "model": {
            "kind": "tfidf_logistic_regression",
            "ngram_range": [1, 2],
            "min_df": 2,
            "max_features": 80000,
            "class_weight": "balanced",
        },
        "calibration": {
            "kind": "predicted_class_isotonic_correctness_probability",
            "source": "family_grouped_oof",
            "folds": args.folds,
            "seed": args.seed,
            "metrics": overall,
        },
        "routing": {
            "kind": "classwise_confidence_plus_family_similarity_ood_gate",
            "accept_target_precision": args.accept_precision,
            "review_target_precision": args.review_precision,
            "min_ood_threshold_examples": args.min_ood_threshold_examples,
            "class_policies": class_policies,
            "oof_action_counts": dict(sorted(action_counts.items())),
            "selective_oof": selective,
        },
        "leakage_policy": {
            "function_names_used": False,
            "addresses_used": False,
            "raw_headers_used": False,
            "project_goal_used": False,
            "binary_name_used_for_ood": False,
            "family_grouped_calibration": True,
            "family_grouped_ood_thresholds": True,
        },
    }
    (args.output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    with (args.output_dir / "oof_predictions.jsonl").open(
        "w", encoding="utf-8"
    ) as handle:
        for index, row in enumerate(rows):
            prediction = str(oof_pred[index])
            policy = class_policies[prediction]
            result = {
                "id": row["id"],
                "family_id": row["family_id"],
                "truth": row["label"],
                "prediction": prediction,
                "correct": bool(correct[index]),
                "raw_confidence": float(oof_raw_conf[index]),
                "confidence": float(calibrated[index]),
                "confidence_source": confidence_sources[index],
                "family_similarity": float(oof_similarity[index]),
                "nearest_training_family": oof_nearest_family[index],
                "action": actions[index],
                "accept_threshold": policy.get("accept_threshold"),
                "review_threshold": policy.get("review_threshold"),
                "ood_accept_threshold": policy.get("ood_accept_threshold"),
                "probabilities": {
                    label: float(oof_probs[index, class_index[label]])
                    for label in args.classes
                },
            }
            handle.write(json.dumps(result, sort_keys=True) + "\n")

    print("artifact:", artifact_path)
    print("manifest:", args.output_dir / "manifest.json")
    print("oof predictions:", args.output_dir / "oof_predictions.jsonl")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
