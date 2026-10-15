#!/usr/bin/env python3
"""Evaluate nested constrained calibration for the v5.6 GAME trust meta-classifier.

v5.6 showed that the structured GAME meta-classifier has strong signal, but its
fixed 0.50 keep threshold was too aggressive. v5.7 keeps the same meta model and
selects a keep threshold *only* from meta-level OOF predictions inside each outer
family-held-out training fold.

Leakage barriers:
- outer test labels never participate in base/meta fitting or threshold selection;
- base probabilities for meta-training rows are inner-OOF;
- keep probabilities used for threshold selection are meta-OOF across independent
  candidate families;
- the final meta model is fit only after the threshold is frozen for that outer fold.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

from evaluate_nevergone_game_meta_verifier_v56 import (
    BLOCKED_EVIDENCE_TERMS,
    build_meta_matrix,
    make_meta_model,
    meta_keep_probabilities,
    report_metrics,
    safe_evidence_keys,
)
from evaluate_nevergone_selective_game_verifier_v54 import (
    CLASSES,
    align_probabilities,
    read_oof,
    safe_split_count,
    transition_counts,
)
from train_nevergone_production_classifier import _load_dataset, _make_model, _require_dependencies
from train_nevergone_production_classifier_v5 import augment_text, load_evidence

FORMAT = "NEVERGONE.ConstrainedGameMetaV57/1"


def apply_meta(base_predictions, candidate_indices, candidate_keep_probabilities, keep_threshold: float, np):
    corrected = np.asarray(base_predictions, dtype=object).copy()
    for local_index, row_index in enumerate(candidate_indices.tolist()):
        if float(candidate_keep_probabilities[local_index]) < keep_threshold:
            corrected[row_index] = "engine"
    return corrected


def threshold_grid() -> list[float]:
    # 0.0 is an explicit no-op fallback. Fine-grained low thresholds are included
    # because v5.6/0.50 was demonstrably too aggressive.
    return [0.0, 0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.35, 0.40, 0.45, 0.50,
            0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85, 0.90, 0.95]


def select_threshold(
    labels,
    base_predictions,
    candidate_indices,
    candidate_keep_probabilities,
    *,
    deps,
    np,
    max_balanced_loss: float,
    max_game_recall_loss: float,
    target_game_precision: float,
) -> tuple[float, list[dict[str, Any]]]:
    base = report_metrics(labels, base_predictions, base_predictions, deps)
    reports: list[dict[str, Any]] = []
    for threshold in threshold_grid():
        corrected = apply_meta(
            base_predictions,
            candidate_indices,
            candidate_keep_probabilities,
            threshold,
            np,
        )
        report = report_metrics(labels, base_predictions, corrected, deps)
        report["threshold"] = threshold
        report["eligible"] = bool(
            report["balanced_accuracy"] >= base["balanced_accuracy"] - max_balanced_loss
            and report["game_recall"] >= base["game_recall"] - max_game_recall_loss
        )
        report["meets_target_precision"] = bool(report["game_precision"] >= target_game_precision)
        reports.append(report)

    eligible = [row for row in reports if row["eligible"]]
    if not eligible:
        eligible = [row for row in reports if row["threshold"] == 0.0]
    targeted = [row for row in eligible if row["meets_target_precision"]]
    pool = targeted if targeted else eligible
    selected = max(
        pool,
        key=lambda row: (
            row["macro_f1"],
            row["game_f1"],
            row["game_precision"],
            row["balanced_accuracy"],
            row["net_helpful_flips"],
            -row["harmful_flips"],
            -row["threshold"],
        ),
    )
    return float(selected["threshold"]), reports


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
    parser.add_argument("--champion-summary", type=Path, help="optional frozen v5.4 reference; reporting only")
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--inner-folds", type=int, default=4)
    parser.add_argument("--meta-folds", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--max-balanced-loss", type=float, default=0.003)
    parser.add_argument("--max-game-recall-loss", type=float, default=0.03)
    parser.add_argument("--target-game-precision", type=float, default=0.45)
    args = parser.parse_args()

    if args.max_balanced_loss < 0 or args.max_game_recall_loss < 0:
        raise SystemExit("loss constraints must be non-negative")
    if not 0.0 <= args.target_game_precision <= 1.0:
        raise SystemExit("--target-game-precision must be in [0, 1]")

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
    threshold_for_row = np.zeros(n, dtype=np.float64)
    fold_reports: list[dict[str, Any]] = []

    for outer_fold, (outer_train, outer_test) in enumerate(outer.split(texts, labels, groups), 1):
        train_labels = labels[outer_train]
        train_groups = groups[outer_train]
        inner_folds = safe_split_count(train_labels.tolist(), train_groups.tolist(), args.inner_folds)
        inner = deps["StratifiedGroupKFold"](
            n_splits=inner_folds,
            shuffle=True,
            random_state=args.seed + 5700 + outer_fold,
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
        candidate_groups = train_groups[candidate_local]
        target_counts = Counter(candidate_targets.tolist())

        selected_threshold = 0.0
        threshold_reports: list[dict[str, Any]] = []
        candidate_keep_oof = np.ones(len(candidate_local), dtype=np.float64)
        meta_oof_available = False
        if len(candidate_local) >= 8 and len(target_counts) == 2:
            try:
                meta_folds = safe_split_count(
                    [str(value) for value in candidate_targets.tolist()],
                    [str(value) for value in candidate_groups.tolist()],
                    args.meta_folds,
                )
            except ValueError:
                meta_folds = 0
            if meta_folds >= 2:
                meta_splitter = deps["StratifiedGroupKFold"](
                    n_splits=meta_folds,
                    shuffle=True,
                    random_state=args.seed + 5750 + outer_fold,
                )
                for meta_fold, (meta_train, meta_valid) in enumerate(
                    meta_splitter.split(
                        inner_meta_matrix[candidate_local],
                        candidate_targets,
                        candidate_groups,
                    ),
                    1,
                ):
                    meta_model = make_meta_model(args.seed + 570000 + outer_fold * 100 + meta_fold)
                    meta_model.fit(
                        inner_meta_matrix[candidate_local[meta_train]],
                        candidate_targets[meta_train],
                    )
                    candidate_keep_oof[meta_valid] = meta_keep_probabilities(
                        meta_model,
                        inner_meta_matrix[candidate_local[meta_valid]],
                        np,
                    )
                meta_oof_available = True
                selected_threshold, threshold_reports = select_threshold(
                    train_labels,
                    inner_base_predictions,
                    candidate_local,
                    candidate_keep_oof,
                    deps=deps,
                    np=np,
                    max_balanced_loss=args.max_balanced_loss,
                    max_game_recall_loss=args.max_game_recall_loss,
                    target_game_precision=args.target_game_precision,
                )

        final_meta_model = None
        if len(candidate_local) >= 4 and len(target_counts) == 2:
            final_meta_model = make_meta_model(args.seed + 579000 + outer_fold)
            final_meta_model.fit(inner_meta_matrix[candidate_local], candidate_targets)

        base_model = _make_model(deps, args.seed + outer_fold)
        base_model.fit(texts[outer_train].tolist(), labels[outer_train].tolist())
        aligned = align_probabilities(base_model, texts[outer_test].tolist(), CLASSES, np)
        fold_base_predictions = np.asarray(
            [CLASSES[index] for index in aligned.argmax(axis=1)], dtype=object
        )
        fold_test_ids = [ids[index] for index in outer_test.tolist()]
        fold_meta_matrix = build_meta_matrix(fold_test_ids, aligned, external, evidence_keys, np)
        fold_candidate = np.asarray(
            [index for index, prediction in enumerate(fold_base_predictions.tolist()) if str(prediction) == "game"],
            dtype=np.int64,
        )
        fold_keep_all = np.ones(len(outer_test), dtype=np.float64)
        if final_meta_model is not None and len(fold_candidate):
            fold_keep_all[fold_candidate] = meta_keep_probabilities(
                final_meta_model,
                fold_meta_matrix[fold_candidate],
                np,
            )
        fold_corrected = apply_meta(
            fold_base_predictions,
            fold_candidate,
            fold_keep_all[fold_candidate],
            selected_threshold,
            np,
        )

        base_predictions[outer_test] = fold_base_predictions
        corrected_predictions[outer_test] = fold_corrected
        base_probabilities[outer_test] = aligned
        keep_probabilities[outer_test] = fold_keep_all
        outer_fold_for_row[outer_test] = outer_fold
        threshold_for_row[outer_test] = selected_threshold

        selected_inner = None
        if threshold_reports:
            selected_inner = next(
                (row for row in threshold_reports if float(row["threshold"]) == selected_threshold),
                None,
            )
        fold_reports.append(
            {
                "fold": outer_fold,
                "train_examples": int(len(outer_train)),
                "test_examples": int(len(outer_test)),
                "meta_candidates": int(len(candidate_local)),
                "meta_keep": int(target_counts.get(1, 0)),
                "meta_reject": int(target_counts.get(0, 0)),
                "meta_oof_available": bool(meta_oof_available),
                "selected_threshold": selected_threshold,
                "inner_selected": selected_inner,
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
    rows_path = output_dir / "oof_constrained_meta_predictions.jsonl"
    summary_path = output_dir / "summary.json"

    with rows_path.open("w", encoding="utf-8") as handle:
        for index, record_id in enumerate(ids):
            handle.write(json.dumps({
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
                "selected_threshold": float(threshold_for_row[index]),
                "changed": bool(str(base_predictions[index]) != str(corrected_predictions[index])),
            }, sort_keys=True) + "\n")

    summary = {
        "format": FORMAT,
        "training_feedback": False,
        "outer_folds": outer_folds,
        "inner_folds_requested": args.inner_folds,
        "meta_folds_requested": args.meta_folds,
        "seed": args.seed,
        "max_balanced_loss": args.max_balanced_loss,
        "max_game_recall_loss": args.max_game_recall_loss,
        "target_game_precision": args.target_game_precision,
        "evidence_feature_count": len(evidence_keys),
        "meta_feature_count": 9 + len(evidence_keys),
        "blocked_evidence_terms": list(BLOCKED_EVIDENCE_TERMS),
        "base": base_report,
        "meta": meta_report,
        "base_boundary_errors": dict(transition_counts(labels, base_predictions)),
        "meta_boundary_errors": dict(transition_counts(labels, corrected_predictions)),
        "v52_oof_reproduction_rate": reproduction_rate,
        "selected_thresholds": [float(row["selected_threshold"]) for row in fold_reports],
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
    print("selected thresholds:", [round(value, 4) for value in summary["selected_thresholds"]])
    print(
        "base: "
        f"balanced={base_report['balanced_accuracy']:.4f} macro_f1={base_report['macro_f1']:.4f} "
        f"game_precision={base_report['game_precision']:.4f} game_recall={base_report['game_recall']:.4f} "
        f"game_f1={base_report['game_f1']:.4f}"
    )
    print(
        "meta: "
        f"balanced={meta_report['balanced_accuracy']:.4f} macro_f1={meta_report['macro_f1']:.4f} "
        f"game_precision={meta_report['game_precision']:.4f} game_recall={meta_report['game_recall']:.4f} "
        f"game_f1={meta_report['game_f1']:.4f}"
    )
    print("base boundary errors:", summary["base_boundary_errors"])
    print("meta boundary errors:", summary["meta_boundary_errors"])
    print(
        "flips:", meta_report["flips"],
        "helpful:", meta_report["helpful_flips"],
        "harmful:", meta_report["harmful_flips"],
    )
    print(f"v5.2 OOF reproduction rate: {reproduction_rate:.4f}")
    print("training feedback: no")
    print("rows:", rows_path)
    print("summary:", summary_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
