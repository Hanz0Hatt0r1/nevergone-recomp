#!/usr/bin/env python3
"""Train ProductionClassifier/5 with label-free structural evidence fusion.

Version 5 keeps the validated sparse portable-P-code classifier, but augments each
function with bucketed objective evidence tokens. Structural metrics are derived
from portable P-code for every row; optional external evidence can add call-graph
and receiver/vtable summaries. Raw names, addresses, receiver class identities and
labels are excluded from the representation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from collections import Counter
from pathlib import Path
from typing import Any

from extract_nevergone_structural_metrics import structural_metrics
from nevergone_ood_similarity import build_family_prototypes, contrastive_family_scores
from train_nevergone_production_classifier import (
    _fit_isotonic,
    _load_dataset,
    _make_model,
    _require_dependencies,
    _selective_summary,
    _sha256_rows,
    calibrate_confidence,
    choose_precision_threshold,
)
from train_nevergone_production_classifier_v4 import route_action_with_contrastive_ood

ARTIFACT_FORMAT_V5 = "NEVERGONE.ProductionClassifier/5"
EVIDENCE_FORMAT = "NEVERGONE.ProductionEvidence/1"


def _safe_key(value: str) -> str:
    return re.sub(r"[^a-zA-Z0-9_.-]+", "_", value).strip("_") or "feature"


def _bucket_number(value: float) -> str:
    value = float(value)
    if not math.isfinite(value):
        return "nonfinite"
    if value == 0.0:
        return "0"

    # Ratios are common among structural/callgraph features. Quantize them
    # coarsely instead of exposing high-precision values as near-identifiers.
    if -1.0 <= value <= 1.0 and not math.isclose(value, round(value), abs_tol=1e-12):
        decile = int(round(value * 10.0))
        return f"ratio_{decile:+d}"

    sign = "neg_" if value < 0 else ""
    magnitude = abs(value)
    if magnitude < 1:
        return sign + "lt1"
    lower = 1
    upper = 1
    while upper < magnitude and upper < (1 << 20):
        lower = upper + 1
        upper = (upper << 1) + 1
    if upper >= (1 << 20) and magnitude > upper:
        return sign + "1048576_plus"
    if lower == upper:
        return sign + str(lower)
    return f"{sign}{lower}_{upper}"


def load_evidence(path: Path | None) -> dict[str, dict[str, float]]:
    if path is None:
        return {}
    result: dict[str, dict[str, float]] = {}
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict) or row.get("format") != EVIDENCE_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {EVIDENCE_FORMAT}")
            record_id = row.get("id")
            features = row.get("features")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError(f"{path}:{line_no}: missing id")
            if record_id in result:
                raise ValueError(f"{path}:{line_no}: duplicate id {record_id}")
            if not isinstance(features, dict):
                raise ValueError(f"{path}:{line_no}: features must be an object")
            clean: dict[str, float] = {}
            for key, value in features.items():
                if not isinstance(key, str) or not key:
                    continue
                if isinstance(value, bool):
                    clean[key] = float(value)
                elif isinstance(value, (int, float)) and math.isfinite(float(value)):
                    clean[key] = float(value)
            result[record_id] = clean
    return result


def evidence_features(text: str, external: dict[str, float] | None = None) -> dict[str, float]:
    features: dict[str, float] = {
        f"structural.{key}": float(value)
        for key, value in structural_metrics(text).items()
        if isinstance(value, (int, float)) and math.isfinite(float(value))
    }
    if external:
        for key, value in external.items():
            if isinstance(value, (int, float)) and math.isfinite(float(value)):
                features[f"external.{key}"] = float(value)
    return features


def evidence_tokens(text: str, external: dict[str, float] | None = None) -> list[str]:
    features = evidence_features(text, external)
    return [
        f"EV|{_safe_key(key)}|{_bucket_number(value)}"
        for key, value in sorted(features.items())
    ]


def augment_text(text: str, external: dict[str, float] | None = None) -> tuple[str, int]:
    tokens = evidence_tokens(text, external)
    if not tokens:
        return text, 0
    return text + "\n" + " ".join(tokens), len(tokens)


def _sha256_evidence(evidence: dict[str, dict[str, float]]) -> str:
    digest = hashlib.sha256()
    for record_id in sorted(evidence):
        digest.update(record_id.encode("utf-8"))
        digest.update(b"\0")
        payload = json.dumps(evidence[record_id], sort_keys=True, separators=(",", ":"))
        digest.update(payload.encode("utf-8"))
        digest.update(b"\n")
    return digest.hexdigest()


def _class_policies(
    *,
    args,
    deps,
    np,
    predictions,
    calibrated,
    correct,
    pred_similarity,
    margin,
    class_calibrators,
) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for label in args.classes:
        mask = predictions == label
        accept = choose_precision_threshold(
            calibrated[mask].tolist(),
            correct[mask].tolist(),
            target_precision=args.accept_precision,
            min_examples=args.min_class_threshold_examples,
        )
        review = choose_precision_threshold(
            calibrated[mask].tolist(),
            correct[mask].tolist(),
            target_precision=args.review_precision,
            min_examples=args.min_class_threshold_examples,
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
        if int(confidence_candidates.sum()) >= args.min_ood_threshold_examples:
            similarity_policy = choose_precision_threshold(
                pred_similarity[confidence_candidates].tolist(),
                correct[confidence_candidates].tolist(),
                target_precision=args.accept_precision,
                min_examples=args.min_ood_threshold_examples,
            )
        similarity_threshold = float(similarity_policy["threshold"]) if similarity_policy else None

        contrast_candidates = confidence_candidates.copy()
        if similarity_threshold is None:
            contrast_candidates[:] = False
        else:
            contrast_candidates &= pred_similarity >= similarity_threshold

        contrast_policy = None
        if int(contrast_candidates.sum()) >= args.min_ood_threshold_examples:
            contrast_policy = choose_precision_threshold(
                margin[contrast_candidates].tolist(),
                correct[contrast_candidates].tolist(),
                target_precision=args.accept_precision,
                min_examples=args.min_ood_threshold_examples,
            )
        learned_margin = float(contrast_policy["threshold"]) if contrast_policy else None
        contrast_threshold = max(0.0, learned_margin) if learned_margin is not None else None

        predicted_examples = int(mask.sum())
        result[label] = {
            "predicted_examples": predicted_examples,
            "oof_precision": float(correct[mask].mean()) if predicted_examples else 0.0,
            "confidence_calibrator": "classwise" if label in class_calibrators else "global_fallback",
            "accept": accept,
            "review": review,
            "accept_threshold": accept_threshold,
            "review_threshold": review_threshold,
            "ood_accept": similarity_policy,
            "ood_accept_threshold": similarity_threshold,
            "contrast_accept": contrast_policy,
            "contrast_accept_threshold": contrast_threshold,
            "contrast_learned_threshold": learned_margin,
            "ood_accept_candidate_examples": int(confidence_candidates.sum()),
            "contrast_candidate_examples": int(contrast_candidates.sum()),
        }
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--evidence", type=Path)
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
    if min(
        args.min_class_calibration_examples,
        args.min_class_threshold_examples,
        args.min_ood_threshold_examples,
    ) < 1:
        raise SystemExit("minimum evidence counts must be positive")

    deps = _require_dependencies()
    np = deps["np"]
    rows = _load_dataset(
        args.dataset_dir,
        args.families,
        task=args.task,
        classes=args.classes,
    )
    external = load_evidence(args.evidence)

    augmented: list[str] = []
    evidence_counts: list[int] = []
    external_rows = 0
    for row in rows:
        ext = external.get(row["id"], {})
        if ext:
            external_rows += 1
        document, count = augment_text(row["text"], ext)
        augmented.append(document)
        evidence_counts.append(count)

    texts = np.asarray(augmented, dtype=object)
    labels = np.asarray([row["label"] for row in rows], dtype=object)
    groups = np.asarray([row["family_id"] for row in rows], dtype=object)

    print(f"format: {ARTIFACT_FORMAT_V5}")
    print("functions:", len(rows))
    print("families:", len(set(groups.tolist())))
    print("classes:", Counter(labels.tolist()))
    print("external evidence rows:", external_rows)
    print("mean evidence tokens:", round(sum(evidence_counts) / len(evidence_counts), 3))
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
    oof_pred_similarity = np.zeros(len(rows), dtype=np.float64)
    oof_competing_similarity = np.zeros(len(rows), dtype=np.float64)
    oof_margin = np.zeros(len(rows), dtype=np.float64)
    oof_pred_family = np.empty(len(rows), dtype=object)
    oof_competing_family = np.empty(len(rows), dtype=object)
    oof_competing_label = np.empty(len(rows), dtype=object)
    oof_pred_family[:] = None
    oof_competing_family[:] = None
    oof_competing_label[:] = None

    for fold, (train_idx, test_idx) in enumerate(splitter.split(texts, labels, groups), 1):
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
        predictions = np.asarray([args.classes[index] for index in pred_index], dtype=object)
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
        scores = contrastive_family_scores(
            vectorizer,
            prototypes,
            test_texts,
            predictions.tolist(),
        )
        for local_index, global_index in enumerate(test_idx):
            score = scores[local_index]
            oof_pred_similarity[global_index] = float(score["predicted_similarity"])
            oof_competing_similarity[global_index] = float(score["competing_similarity"])
            oof_margin[global_index] = float(score["contrast_margin"])
            oof_pred_family[global_index] = score["predicted_family"]
            oof_competing_family[global_index] = score["competing_family"]
            oof_competing_label[global_index] = score["competing_label"]

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

    class_policies = _class_policies(
        args=args,
        deps=deps,
        np=np,
        predictions=oof_pred,
        calibrated=calibrated,
        correct=correct,
        pred_similarity=oof_pred_similarity,
        margin=oof_margin,
        class_calibrators=class_calibrators,
    )

    actions = [
        route_action_with_contrastive_ood(
            str(oof_pred[index]),
            float(calibrated[index]),
            float(oof_pred_similarity[index]),
            float(oof_margin[index]),
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
    }

    print()
    print(
        "OOF metrics:",
        f"accuracy={overall['accuracy']:.4f}",
        f"balanced={overall['balanced_accuracy']:.4f}",
        f"macro_f1={overall['macro_f1']:.4f}",
    )
    print("class policies + contrastive OOD:")
    for label in args.classes:
        policy = class_policies[label]
        print(
            f"  {label}: predicted={policy['predicted_examples']} "
            f"precision={policy['oof_precision']:.4f} "
            f"accept={policy['accept_threshold']} review={policy['review_threshold']} "
            f"sim={policy['ood_accept_threshold']} "
            f"margin={policy['contrast_accept_threshold']}"
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
            "format": ARTIFACT_FORMAT_V5,
            "task": args.task,
            "classes": args.classes,
            "model": final_model,
            "confidence_calibrator": global_calibrator,
            "class_confidence_calibrators": class_calibrators,
            "class_policies": class_policies,
            "family_prototypes": family_prototypes,
            "evidence_format": EVIDENCE_FORMAT,
            "evidence_policy": {
                "structural_metrics": "derived_from_portable_pcode",
                "external_features": "numeric_only",
                "bucketed": True,
            },
        },
        artifact_path,
    )

    manifest = {
        "format": ARTIFACT_FORMAT_V5,
        "task": args.task,
        "classes": args.classes,
        "functions": len(rows),
        "families": len(set(groups.tolist())),
        "prototype_families": len(family_prototypes["families"]),
        "class_distribution": dict(sorted(Counter(labels.tolist()).items())),
        "dataset_fingerprint_sha256": _sha256_rows(rows),
        "evidence_fingerprint_sha256": _sha256_evidence(external),
        "representation": "portable_pcode_plus_bucketed_objective_evidence",
        "evidence": {
            "format": EVIDENCE_FORMAT,
            "external_rows_available": len(external),
            "external_rows_used": external_rows,
            "mean_tokens_per_function": sum(evidence_counts) / len(evidence_counts),
            "structural_metrics_always_derived": True,
            "raw_receiver_class_names_used": False,
            "raw_vtable_ids_used": False,
            "source_addresses_used": False,
        },
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
            "kind": "classwise_confidence_plus_contrastive_family_ood",
            "accept_target_precision": args.accept_precision,
            "review_target_precision": args.review_precision,
            "min_ood_threshold_examples": args.min_ood_threshold_examples,
            "positive_margin_floor": 0.0,
            "class_policies": class_policies,
            "oof_action_counts": dict(sorted(action_counts.items())),
            "selective_oof": selective,
        },
        "leakage_policy": {
            "function_names_used": False,
            "addresses_used": False,
            "receiver_class_names_used": False,
            "vtable_identities_used": False,
            "raw_headers_used": False,
            "project_goal_used": False,
            "family_grouped_calibration": True,
            "family_grouped_ood_thresholds": True,
        },
    }
    (args.output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    with (args.output_dir / "oof_predictions.jsonl").open("w", encoding="utf-8") as handle:
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
                "family_similarity": float(oof_pred_similarity[index]),
                "nearest_training_family": oof_pred_family[index],
                "competing_similarity": float(oof_competing_similarity[index]),
                "nearest_competing_family": oof_competing_family[index],
                "nearest_competing_label": oof_competing_label[index],
                "contrast_margin": float(oof_margin[index]),
                "action": actions[index],
                "accept_threshold": policy.get("accept_threshold"),
                "review_threshold": policy.get("review_threshold"),
                "ood_accept_threshold": policy.get("ood_accept_threshold"),
                "contrast_accept_threshold": policy.get("contrast_accept_threshold"),
                "evidence_token_count": evidence_counts[index],
                "external_evidence_present": bool(external.get(row["id"])),
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
