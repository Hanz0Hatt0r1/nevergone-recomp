#!/usr/bin/env python3
"""Evaluate a leakage-safe structured meta verifier for false GAME predictions.

v5.4 remains the routing champion. v5.5 showed that broader hand-written policy
search does not improve that frontier. v5.6 changes the problem instead of adding
more thresholds: for each outer family-held-out fold, it trains a small structured
meta classifier to answer whether an OOF base prediction of GAME should be trusted.

Meta training rows are only inner-OOF base GAME predictions from outer-train. Thus
for every meta-training example the base probabilities were produced by a model
that did not train on that example. The outer test fold is never used for base or
meta fitting. The meta model sees normalized production evidence plus base
probabilities; it never sees function names, addresses, raw class names or vtable
identities. A fixed keep threshold is configured before evaluation rather than
selected from outer labels.
"""
from __future__ import annotations

import argparse
import json
import math
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from evaluate_nevergone_selective_game_verifier_v54 import (
    CLASSES,
    align_probabilities,
    full_metrics,
    game_metrics,
    read_oof,
    safe_split_count,
    transition_counts,
)
from train_nevergone_production_classifier import _load_dataset, _make_model, _require_dependencies
from train_nevergone_production_classifier_v5 import augment_text, load_evidence

FORMAT = "NEVERGONE.GameMetaVerifierV56/1"
BLOCKED_EVIDENCE_TERMS = (
    "label",
    "class_name",
    "classname",
    "vtable_id",
    "vtable_identity",
    "address",
    "source_entry",
    "function_name",
    "symbol_name",
)


def safe_evidence_keys(evidence: dict[str, dict[str, float]]) -> list[str]:
    keys: set[str] = set()
    for features in evidence.values():
        for key, value in features.items():
            lowered = str(key).lower()
            if any(term in lowered for term in BLOCKED_EVIDENCE_TERMS):
                continue
            if isinstance(value, (int, float)) and math.isfinite(float(value)):
                keys.add(str(key))
    return sorted(keys)


def _entropy(probabilities) -> float:
    total = 0.0
    for value in probabilities:
        probability = float(value)
        if probability > 0.0:
            total -= probability * math.log(probability)
    return total


def build_meta_matrix(ids, base_probabilities, evidence, evidence_keys, np):
    game_index = CLASSES.index("game")
    engine_index = CLASSES.index("engine")
    runtime_index = CLASSES.index("runtime_library")
    media_index = CLASSES.index("media_library")
    matrix = np.zeros((len(ids), 9 + len(evidence_keys)), dtype=np.float64)
    for row_index, record_id in enumerate(ids):
        probs = base_probabilities[row_index]
        game = float(probs[game_index])
        engine = float(probs[engine_index])
        runtime = float(probs[runtime_index])
        media = float(probs[media_index])
        ordered = sorted((float(value) for value in probs), reverse=True)
        matrix[row_index, 0] = game
        matrix[row_index, 1] = engine
        matrix[row_index, 2] = runtime
        matrix[row_index, 3] = media
        matrix[row_index, 4] = game - engine
        matrix[row_index, 5] = ordered[0] - ordered[1] if len(ordered) > 1 else ordered[0]
        matrix[row_index, 6] = _entropy(probs)
        matrix[row_index, 7] = max(probs)
        matrix[row_index, 8] = 1.0 - game
        features = evidence.get(str(record_id), {})
        for feature_index, key in enumerate(evidence_keys, 9):
            value = features.get(key, 0.0)
            if isinstance(value, (int, float)) and math.isfinite(float(value)):
                matrix[row_index, feature_index] = float(value)
    return matrix


def make_meta_model(seed: int):
    try:
        from sklearn.linear_model import LogisticRegression
        from sklearn.pipeline import Pipeline
        from sklearn.preprocessing import StandardScaler
    except ImportError as exc:  # pragma: no cover
        raise RuntimeError("scikit-learn is required for v5.6 meta verifier") from exc
    return Pipeline(
        [
            ("scale", StandardScaler()),
            (
                "classifier",
                LogisticRegression(
                    class_weight="balanced",
                    max_iter=3000,
                    random_state=seed,
                    solver="liblinear",
                ),
            ),
        ]
    )


def meta_keep_probabilities(model, matrix, np):
    raw = model.predict_proba(matrix)
    classes = model.named_steps["classifier"].classes_.tolist()
    if 1 not in classes:
        return np.zeros(len(matrix), dtype=np.float64)
    return raw[:, classes.index(1)]


def apply_meta(base_predictions, keep_probabilities, keep_threshold: float, np):
    corrected = np.asarray(base_predictions, dtype=object).copy()
    for index in range(len(corrected)):
        if str(base_predictions[index]) == "game" and float(keep_probabilities[index]) < keep_threshold:
            corrected[index] = "engine"
    return corrected


def report_metrics(labels, base_predictions, predictions, deps) -> dict[str, Any]:
    base_game = game_metrics(labels, base_predictions)
    current_game = game_metrics(labels, predictions)
    transitions = transition_counts(labels, predictions)
    flips = [
        index for index in range(len(labels))
        if str(base_predictions[index]) != str(predictions[index])
    ]
    helpful = sum(str(predictions[index]) == str(labels[index]) for index in flips)
    harmful = sum(str(base_predictions[index]) == str(labels[index]) for index in flips)
    return {
        **full_metrics(labels, predictions, deps),
        "game_precision": current_game["precision"],
        "game_recall": current_game["recall"],
        "game_f1": current_game["f1"],
        "base_game_precision": base_game["precision"],
        "base_game_recall": base_game["recall"],
        "engine_to_game": int(transitions.get("engine->game", 0)),
        "game_to_engine": int(transitions.get("game->engine", 0)),
        "flips": len(flips),
        "helpful_flips": helpful,
        "harmful_flips": harmful,
        "net_helpful_flips": helpful - harmful,
    }


def _load_reference_summary(path: Path | None) -> dict[str, Any] | None:
    if path is None or not path.exists():
        return None
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("base_oof", type=Path, help="v5.2 OOF predictions; reproduction audit only")
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--champion-summary", type=Path, help="optional v5.4 summary for reporting only")
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--inner-folds", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--keep-threshold", type=float, default=0.50)
    args = parser.parse_args()

    if not 0.0 <= args.keep_threshold <= 1.0:
        raise SystemExit("--keep-threshold must be in [0, 1]")

    deps = _require_dependencies()
    np = deps["np"]
    rows = _load_dataset(args.dataset_dir, args.families, task="code_domain", classes=CLASSES)
    external = load_evidence(args.evidence)
    reference_oof = read_oof(args.base_oof)
    champion = _load_reference_summary(args.champion_summary)

    texts = np.asarray(
        [augment_text(row["text"], external.get(row["id"], {}))[0] for row in rows],
        dtype=object,
    )
    labels = np.asarray([row["label"] for row in rows], dtype=object)
    groups = np.asarray([row["family_id"] for row in rows], dtype=object)
    ids = [str(row["id"]) for row in rows]
    evidence_keys = safe_evidence_keys(external)

    missing = [record_id for record_id in ids if record_id not in reference_oof]
    if missing:
        raise ValueError(f"base OOF is missing {len(missing)} rows; first={missing[0]}")

    outer_folds = safe_split_count(labels.tolist(), groups.tolist(), args.folds)
    outer = deps["StratifiedGroupKFold"](
        n_splits=outer_folds,
        shuffle=True,
        random_state=args.seed,
    )

    n = len(rows)
    base_predictions = np.empty(n, dtype=object)
    corrected_predictions = np.empty(n, dtype=object)
    base_probabilities = np.zeros((n, len(CLASSES)), dtype=np.float64)
    keep_probabilities = np.ones(n, dtype=np.float64)
    outer_fold_for_row = np.zeros(n, dtype=np.int64)
    fold_reports: list[dict[str, Any]] = []

    for outer_fold, (outer_train, outer_test) in enumerate(outer.split(texts, labels, groups), 1):
        train_labels = labels[outer_train]
        train_groups = groups[outer_train]
        inner_folds = safe_split_count(train_labels.tolist(), train_groups.tolist(), args.inner_folds)
        inner = deps["StratifiedGroupKFold"](
            n_splits=inner_folds,
            shuffle=True,
            random_state=args.seed + 5600 + outer_fold,
        )

        inner_base_predictions = np.empty(len(outer_train), dtype=object)
        inner_base_probabilities = np.zeros((len(outer_train), len(CLASSES)), dtype=np.float64)

        for inner_fold, (inner_train_local, inner_valid_local) in enumerate(
            inner.split(texts[outer_train], train_labels, train_groups), 1
        ):
            inner_train = outer_train[inner_train_local]
            inner_valid = outer_train[inner_valid_local]
            model = _make_model(deps, args.seed + outer_fold * 100 + inner_fold)
            model.fit(texts[inner_train].tolist(), labels[inner_train].tolist())
            aligned = align_probabilities(model, texts[inner_valid].tolist(), CLASSES, np)
            inner_base_probabilities[inner_valid_local] = aligned
            inner_base_predictions[inner_valid_local] = np.asarray(
                [CLASSES[index] for index in aligned.argmax(axis=1)], dtype=object
            )

        outer_train_ids = [ids[index] for index in outer_train.tolist()]
        inner_meta_matrix = build_meta_matrix(
            outer_train_ids,
            inner_base_probabilities,
            external,
            evidence_keys,
            np,
        )
        candidate_local = np.asarray(
            [index for index, prediction in enumerate(inner_base_predictions.tolist()) if str(prediction) == "game"],
            dtype=np.int64,
        )
        candidate_targets = np.asarray(
            [1 if str(train_labels[index]) == "game" else 0 for index in candidate_local.tolist()],
            dtype=np.int64,
        )
        target_counts = Counter(candidate_targets.tolist())
        meta_model = None
        if len(candidate_local) >= 4 and len(target_counts) == 2:
            meta_model = make_meta_model(args.seed + 560000 + outer_fold)
            meta_model.fit(inner_meta_matrix[candidate_local], candidate_targets)

        base_model = _make_model(deps, args.seed + outer_fold)
        base_model.fit(texts[outer_train].tolist(), labels[outer_train].tolist())
        aligned = align_probabilities(base_model, texts[outer_test].tolist(), CLASSES, np)
        fold_base_predictions = np.asarray(
            [CLASSES[index] for index in aligned.argmax(axis=1)], dtype=object
        )
        fold_test_ids = [ids[index] for index in outer_test.tolist()]
        fold_meta_matrix = build_meta_matrix(fold_test_ids, aligned, external, evidence_keys, np)
        if meta_model is None:
            fold_keep = np.ones(len(outer_test), dtype=np.float64)
        else:
            fold_keep = meta_keep_probabilities(meta_model, fold_meta_matrix, np)
        fold_corrected = apply_meta(
            fold_base_predictions,
            fold_keep,
            args.keep_threshold,
            np,
        )

        base_predictions[outer_test] = fold_base_predictions
        corrected_predictions[outer_test] = fold_corrected
        base_probabilities[outer_test] = aligned
        keep_probabilities[outer_test] = fold_keep
        outer_fold_for_row[outer_test] = outer_fold

        fold_reports.append(
            {
                "fold": outer_fold,
                "train_examples": int(len(outer_train)),
                "test_examples": int(len(outer_test)),
                "meta_candidates": int(len(candidate_local)),
                "meta_keep": int(target_counts.get(1, 0)),
                "meta_reject": int(target_counts.get(0, 0)),
                "meta_model_trained": bool(meta_model is not None),
                "base": report_metrics(labels[outer_test], fold_base_predictions, fold_base_predictions, deps),
                "meta": report_metrics(labels[outer_test], fold_base_predictions, fold_corrected, deps),
            }
        )

    base_report = report_metrics(labels, base_predictions, base_predictions, deps)
    meta_report = report_metrics(labels, base_predictions, corrected_predictions, deps)
    reproduction_matches = sum(
        str(base_predictions[index]) == str(reference_oof[ids[index]].get("prediction"))
        for index in range(n)
    )
    reproduction_rate = reproduction_matches / max(1, n)

    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    rows_path = output_dir / "oof_meta_verifier_predictions.jsonl"
    summary_path = output_dir / "summary.json"

    with rows_path.open("w", encoding="utf-8") as handle:
        for index, record_id in enumerate(ids):
            row = {
                "format": FORMAT,
                "id": record_id,
                "family_id": str(groups[index]),
                "fold": int(outer_fold_for_row[index]),
                "label": str(labels[index]),
                "base_prediction": str(base_predictions[index]),
                "meta_prediction": str(corrected_predictions[index]),
                "base_game_probability": float(base_probabilities[index, CLASSES.index("game")]),
                "base_engine_probability": float(base_probabilities[index, CLASSES.index("engine")]),
                "meta_keep_game_probability": float(keep_probabilities[index]),
                "changed": bool(str(base_predictions[index]) != str(corrected_predictions[index])),
            }
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    summary = {
        "format": FORMAT,
        "training_feedback": False,
        "outer_folds": outer_folds,
        "inner_folds_requested": args.inner_folds,
        "seed": args.seed,
        "keep_threshold": args.keep_threshold,
        "evidence_feature_count": len(evidence_keys),
        "meta_feature_count": 9 + len(evidence_keys),
        "blocked_evidence_terms": list(BLOCKED_EVIDENCE_TERMS),
        "base": base_report,
        "meta": meta_report,
        "base_boundary_errors": dict(transition_counts(labels, base_predictions)),
        "meta_boundary_errors": dict(transition_counts(labels, corrected_predictions)),
        "v52_oof_reproduction_rate": reproduction_rate,
        "folds": fold_reports,
        "champion_reference": champion,
        "rows": str(rows_path),
    }
    with summary_path.open("w", encoding="utf-8") as handle:
        json.dump(summary, handle, indent=2, sort_keys=True)
        handle.write("\n")

    print(f"format: {FORMAT}")
    print(f"rows: {n}")
    print(f"evidence features: {len(evidence_keys)}")
    print(f"meta features: {9 + len(evidence_keys)}")
    print(
        "base: "
        f"balanced={base_report['balanced_accuracy']:.4f} "
        f"macro_f1={base_report['macro_f1']:.4f} "
        f"game_precision={base_report['game_precision']:.4f} "
        f"game_recall={base_report['game_recall']:.4f} "
        f"game_f1={base_report['game_f1']:.4f}"
    )
    print(
        "meta: "
        f"balanced={meta_report['balanced_accuracy']:.4f} "
        f"macro_f1={meta_report['macro_f1']:.4f} "
        f"game_precision={meta_report['game_precision']:.4f} "
        f"game_recall={meta_report['game_recall']:.4f} "
        f"game_f1={meta_report['game_f1']:.4f}"
    )
    print("base boundary errors:", dict(transition_counts(labels, base_predictions)))
    print("meta boundary errors:", dict(transition_counts(labels, corrected_predictions)))
    print(
        "flips:", meta_report["flips"],
        "helpful:", meta_report["helpful_flips"],
        "harmful:", meta_report["harmful_flips"],
    )
    print(f"v5.2 OOF reproduction rate: {reproduction_rate:.4f}")
    print("training feedback: no")
    print(f"rows: {rows_path}")
    print(f"summary: {summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
