#!/usr/bin/env python3
"""Train and package the production portable-P-code classifier.

The classifier is deliberately the strongest validated simple baseline:
portable P-code -> TF-IDF unigram/bigram -> balanced logistic regression.

Confidence routing is calibrated only from family-grouped out-of-fold (OOF)
predictions. Function names, source addresses, corpus headers and project goals
are never model inputs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

from nevergone_pcode_features import portable_pcode

LEGACY_ARTIFACT_FORMAT = "NEVERGONE.ProductionClassifier/1"
ARTIFACT_FORMAT = "NEVERGONE.ProductionClassifier/2"


def _require_dependencies():
    try:
        import joblib
        import numpy as np
        from sklearn.feature_extraction.text import TfidfVectorizer
        from sklearn.isotonic import IsotonicRegression
        from sklearn.linear_model import LogisticRegression
        from sklearn.metrics import accuracy_score, balanced_accuracy_score, f1_score
        from sklearn.model_selection import StratifiedGroupKFold
        from sklearn.pipeline import Pipeline
    except ImportError as exc:
        raise SystemExit(
            "Training requires numpy, joblib and scikit-learn. Install with: "
            "python -m pip install -U numpy scikit-learn joblib"
        ) from exc
    return {
        "joblib": joblib,
        "np": np,
        "TfidfVectorizer": TfidfVectorizer,
        "IsotonicRegression": IsotonicRegression,
        "LogisticRegression": LogisticRegression,
        "accuracy_score": accuracy_score,
        "balanced_accuracy_score": balanced_accuracy_score,
        "f1_score": f1_score,
        "StratifiedGroupKFold": StratifiedGroupKFold,
        "Pipeline": Pipeline,
    }


def _read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            try:
                row = json.loads(raw)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_no}: invalid JSON: {exc}") from exc
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def _load_family_map(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for row in _read_jsonl(path):
        record_id = row.get("id")
        family_id = row.get("family_id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("family row missing id")
        if not isinstance(family_id, str) or not family_id:
            raise ValueError(f"family row {record_id} missing family_id")
        previous = result.get(record_id)
        if previous is not None and previous != family_id:
            raise ValueError(f"conflicting family IDs for {record_id}")
        result[record_id] = family_id
    return result


def normalize_portable(text: str) -> str:
    """Accept corpus/dataset model text or an already-portable P-code view."""
    normalized = portable_pcode(text)
    if normalized:
        return normalized
    portable_lines = [line for line in text.splitlines() if line.startswith("OP ")]
    if portable_lines:
        return "\n".join(portable_lines)
    raise ValueError("input contains no portable P-code operations")


def _load_dataset(
    dataset_dir: Path,
    families_path: Path,
    *,
    task: str,
    classes: list[str],
) -> list[dict[str, str]]:
    family_map = _load_family_map(families_path)
    allowed = set(classes)
    rows: list[dict[str, str]] = []
    seen: set[str] = set()
    for split in ("train", "validation", "test"):
        path = dataset_dir / f"{split}.jsonl"
        for row in _read_jsonl(path):
            record_id = row.get("id")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError(f"{path}: row missing id")
            if record_id in seen:
                raise ValueError(f"duplicate dataset id: {record_id}")
            seen.add(record_id)
            labels = row.get("labels")
            if not isinstance(labels, dict):
                continue
            label = labels.get(task)
            if label not in allowed:
                continue
            text = row.get("text")
            if not isinstance(text, str) or not text:
                raise ValueError(f"dataset row {record_id} missing text")
            family_id = family_map.get(record_id)
            if family_id is None:
                raise ValueError(f"dataset id missing from family map: {record_id}")
            rows.append(
                {
                    "id": record_id,
                    "text": normalize_portable(text),
                    "label": label,
                    "family_id": family_id,
                }
            )
    if not rows:
        raise ValueError("no labelled rows selected")
    return rows


def _make_model(deps, seed: int):
    return deps["Pipeline"](
        [
            (
                "tfidf",
                deps["TfidfVectorizer"](
                    tokenizer=str.split,
                    token_pattern=None,
                    lowercase=False,
                    ngram_range=(1, 2),
                    min_df=2,
                    max_features=80000,
                    sublinear_tf=True,
                ),
            ),
            (
                "classifier",
                deps["LogisticRegression"](
                    solver="lbfgs",
                    max_iter=4000,
                    class_weight="balanced",
                    random_state=seed,
                ),
            ),
        ]
    )


def choose_precision_threshold(
    confidences: list[float],
    correct: list[bool],
    *,
    target_precision: float,
    min_examples: int,
) -> dict[str, float | int] | None:
    """Return the maximum-coverage threshold meeting an empirical precision target."""
    if len(confidences) != len(correct):
        raise ValueError("confidence/correct length mismatch")
    if not 0.0 < target_precision <= 1.0:
        raise ValueError("target_precision must be in (0, 1]")
    if min_examples < 1:
        raise ValueError("min_examples must be positive")
    pairs = sorted(zip(confidences, correct), key=lambda item: item[0], reverse=True)
    if not pairs:
        return None
    total = len(pairs)
    accepted = 0
    hits = 0
    best: dict[str, float | int] | None = None
    index = 0
    while index < total:
        threshold = pairs[index][0]
        while index < total and math.isclose(
            pairs[index][0], threshold, rel_tol=0.0, abs_tol=1e-12
        ):
            accepted += 1
            hits += int(bool(pairs[index][1]))
            index += 1
        if accepted < min_examples:
            continue
        precision = hits / accepted
        if precision + 1e-12 < target_precision:
            continue
        candidate = {
            "threshold": float(threshold),
            "precision": float(precision),
            "coverage": float(accepted / total),
            "examples": int(accepted),
        }
        if best is None or int(candidate["examples"]) > int(best["examples"]):
            best = candidate
    return best


def route_action(
    confidence: float,
    *,
    accept_threshold: float | None,
    review_threshold: float | None,
) -> str:
    if accept_threshold is not None and confidence >= accept_threshold:
        return "accept"
    if review_threshold is not None and confidence >= review_threshold:
        return "review"
    return "escalate"


def route_action_for_class(
    prediction: str,
    confidence: float,
    class_policies: dict[str, dict[str, Any]],
) -> str:
    policy = class_policies.get(prediction)
    if not isinstance(policy, dict):
        return "escalate"
    return route_action(
        confidence,
        accept_threshold=policy.get("accept_threshold"),
        review_threshold=policy.get("review_threshold"),
    )


def calibrate_confidence(
    prediction: str,
    raw_confidence: float,
    *,
    global_calibrator,
    class_calibrators: dict[str, Any],
) -> tuple[float, str]:
    """Calibrate correctness probability for the predicted class.

    A class-specific calibrator is preferred. The family-grouped global OOF
    calibrator is used only when a class does not have enough varied OOF evidence
    to fit a stable isotonic mapping.
    """
    calibrator = class_calibrators.get(prediction)
    source = "classwise"
    if calibrator is None:
        calibrator = global_calibrator
        source = "global_fallback"
    return float(calibrator.predict([raw_confidence])[0]), source


def _fit_isotonic(deps, x, y, *, min_examples: int):
    if len(x) < min_examples:
        return None
    values = {int(bool(item)) for item in y}
    if len(values) < 2:
        return None
    calibrator = deps["IsotonicRegression"](
        y_min=0.0,
        y_max=1.0,
        out_of_bounds="clip",
    )
    calibrator.fit(x, y)
    return calibrator


def _selective_summary(actions: list[str], correct, np) -> dict[str, dict[str, float | int]]:
    result: dict[str, dict[str, float | int]] = {}
    total = len(actions)
    action_array = np.asarray(actions, dtype=object)
    for action in ("accept", "review", "escalate"):
        mask = action_array == action
        count = int(mask.sum())
        result[action] = {
            "examples": count,
            "coverage": float(count / total) if total else 0.0,
            "precision": float(correct[mask].mean()) if count else 0.0,
        }
    return result


def _sha256_rows(rows: list[dict[str, str]]) -> str:
    digest = hashlib.sha256()
    for row in sorted(rows, key=lambda item: item["id"]):
        digest.update(row["id"].encode("utf-8"))
        digest.update(b"\0")
        digest.update(row["label"].encode("utf-8"))
        digest.update(b"\0")
        digest.update(row["family_id"].encode("utf-8"))
        digest.update(b"\0")
        digest.update(row["text"].encode("utf-8"))
        digest.update(b"\n")
    return digest.hexdigest()


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
    parser.add_argument("--min-threshold-examples", type=int, default=25)
    parser.add_argument("--min-class-calibration-examples", type=int, default=25)
    parser.add_argument("--min-class-threshold-examples", type=int, default=10)
    args = parser.parse_args()

    if args.folds < 2:
        raise SystemExit("--folds must be >= 2")
    if args.review_precision > args.accept_precision:
        raise SystemExit("--review-precision must be <= --accept-precision")
    if args.min_class_calibration_examples < 2:
        raise SystemExit("--min-class-calibration-examples must be >= 2")
    if args.min_class_threshold_examples < 1:
        raise SystemExit("--min-class-threshold-examples must be >= 1")

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

    print(f"format: {ARTIFACT_FORMAT}")
    print("functions:", len(rows))
    print("families:", len(set(groups.tolist())))
    print("classes:", Counter(labels.tolist()))
    print()

    splitter = deps["StratifiedGroupKFold"](
        n_splits=args.folds,
        shuffle=True,
        random_state=args.seed,
    )
    oof_pred = np.empty(len(rows), dtype=object)
    oof_raw_conf = np.zeros(len(rows), dtype=np.float64)
    oof_probs = np.zeros((len(rows), len(args.classes)), dtype=np.float64)
    class_index = {label: index for index, label in enumerate(args.classes)}

    for fold, (train_idx, test_idx) in enumerate(
        splitter.split(texts, labels, groups), 1
    ):
        model = _make_model(deps, args.seed + fold)
        model.fit(texts[train_idx].tolist(), labels[train_idx].tolist())
        probabilities = model.predict_proba(texts[test_idx].tolist())
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

    global_accept = choose_precision_threshold(
        calibrated.tolist(),
        correct.tolist(),
        target_precision=args.accept_precision,
        min_examples=args.min_threshold_examples,
    )
    global_review = choose_precision_threshold(
        calibrated.tolist(),
        correct.tolist(),
        target_precision=args.review_precision,
        min_examples=args.min_threshold_examples,
    )

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
        }

    actions = [
        route_action_for_class(
            str(oof_pred[index]),
            float(calibrated[index]),
            class_policies,
        )
        for index in range(len(rows))
    ]
    action_counts = Counter(actions)
    selective = _selective_summary(actions, correct, np)

    overall = {
        "accuracy": float(deps["accuracy_score"](labels, oof_pred)),
        "balanced_accuracy": float(
            deps["balanced_accuracy_score"](labels, oof_pred)
        ),
        "macro_f1": float(
            deps["f1_score"](
                labels, oof_pred, average="macro", zero_division=0
            )
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
    print(
        "confidence brier:",
        f"raw={overall['raw_confidence_brier']:.4f}",
        f"calibrated={overall['calibrated_confidence_brier']:.4f}",
    )
    print("global accept diagnostic:", global_accept)
    print("global review diagnostic:", global_review)
    print("class policies:")
    for label in args.classes:
        policy = class_policies[label]
        print(
            f"  {label}: predicted={policy['predicted_examples']} "
            f"precision={policy['oof_precision']:.4f} "
            f"calibrator={policy['confidence_calibrator']} "
            f"accept={policy['accept']} review={policy['review']}"
        )
    print("OOF actions:", dict(sorted(action_counts.items())))
    print("selective OOF:", selective)

    final_model = _make_model(deps, args.seed)
    final_model.fit(texts.tolist(), labels.tolist())

    args.output_dir.mkdir(parents=True, exist_ok=True)
    artifact_path = args.output_dir / "classifier.joblib"
    deps["joblib"].dump(
        {
            "format": ARTIFACT_FORMAT,
            "task": args.task,
            "classes": args.classes,
            "model": final_model,
            "confidence_calibrator": global_calibrator,
            "class_confidence_calibrators": class_calibrators,
            "class_policies": class_policies,
        },
        artifact_path,
    )

    manifest = {
        "format": ARTIFACT_FORMAT,
        "task": args.task,
        "classes": args.classes,
        "functions": len(rows),
        "families": len(set(groups.tolist())),
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
            "fallback": "global_family_grouped_oof_isotonic",
            "source": "family_grouped_oof",
            "folds": args.folds,
            "seed": args.seed,
            "min_class_calibration_examples": args.min_class_calibration_examples,
            "classwise_calibrators": sorted(class_calibrators),
            "metrics": overall,
        },
        "routing": {
            "kind": "predicted_class_precision_thresholds",
            "accept_target_precision": args.accept_precision,
            "review_target_precision": args.review_precision,
            "min_class_threshold_examples": args.min_class_threshold_examples,
            "class_policies": class_policies,
            "global_accept_diagnostic": global_accept,
            "global_review_diagnostic": global_review,
            "oof_action_counts": dict(sorted(action_counts.items())),
            "selective_oof": selective,
        },
        "leakage_policy": {
            "function_names_used": False,
            "addresses_used": False,
            "raw_headers_used": False,
            "project_goal_used": False,
            "family_grouped_calibration": True,
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
                "action": actions[index],
                "accept_threshold": policy.get("accept_threshold"),
                "review_threshold": policy.get("review_threshold"),
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
