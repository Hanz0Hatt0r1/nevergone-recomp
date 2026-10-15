#!/usr/bin/env python3
"""Evaluate a nested family-grouped verifier for the game/engine boundary.

The v5.2 OOF audit showed that engine->game false positives dominate the remaining
boundary errors. This evaluator trains a second binary classifier only on true
`game`/`engine` rows and uses it as a *demotion-only* verifier: a base `game`
prediction is changed to `engine` when the verifier's P(game) is below a threshold.

Thresholds are learned from inner family-grouped OOF predictions inside each outer
fold. The outer test family is never used to fit the verifier or select its threshold.
No verifier output is fed back into the v5.2 training representation.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from train_nevergone_production_classifier import _load_dataset, _make_model, _require_dependencies
from train_nevergone_production_classifier_v5 import augment_text, load_evidence

FORMAT = "NEVERGONE.GameVerifierV53/1"
PAIR = ("game", "engine")


def read_oof(path: Path) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            record_id = row.get("id")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError(f"{path}:{line_no}: missing id")
            if record_id in result:
                raise ValueError(f"{path}:{line_no}: duplicate id {record_id}")
            result[record_id] = row
    return result


def game_probabilities(model, texts: list[str], np) -> Any:
    probabilities = model.predict_proba(texts)
    classes = model.named_steps["classifier"].classes_.tolist()
    if "game" not in classes or "engine" not in classes:
        raise ValueError(f"verifier model missing boundary class: {classes}")
    return probabilities[:, classes.index("game")]


def pair_metrics(truth, prediction, f1_score) -> dict[str, float]:
    truth_list = [str(value) for value in truth]
    pred_list = [str(value) for value in prediction]
    game_tp = sum(t == "game" and p == "game" for t, p in zip(truth_list, pred_list))
    game_pred = sum(p == "game" for p in pred_list)
    game_true = sum(t == "game" for t in truth_list)
    engine_true = sum(t == "engine" for t in truth_list)
    engine_ok = sum(t == "engine" and p == "engine" for t, p in zip(truth_list, pred_list))
    return {
        "accuracy": sum(t == p for t, p in zip(truth_list, pred_list)) / max(1, len(truth_list)),
        "macro_f1": float(
            f1_score(truth_list, pred_list, labels=["game", "engine"], average="macro", zero_division=0)
        ),
        "game_precision": game_tp / max(1, game_pred),
        "game_recall": game_tp / max(1, game_true),
        "engine_recall": engine_ok / max(1, engine_true),
        "game_predictions": float(game_pred),
    }


def choose_threshold(
    probabilities,
    labels,
    *,
    f1_score,
    target_game_precision: float,
    max_macro_loss: float,
) -> tuple[float, dict[str, float], list[dict[str, float]]]:
    candidates = [round(0.35 + 0.025 * index, 3) for index in range(25)]
    rows: list[dict[str, float]] = []
    for threshold in candidates:
        prediction = ["game" if float(prob) >= threshold else "engine" for prob in probabilities]
        metrics = pair_metrics(labels, prediction, f1_score)
        rows.append({"threshold": threshold, **metrics})

    best_macro = max(row["macro_f1"] for row in rows)
    eligible = [
        row
        for row in rows
        if row["game_precision"] >= target_game_precision
        and row["macro_f1"] >= best_macro - max_macro_loss
    ]
    if eligible:
        selected = max(
            eligible,
            key=lambda row: (row["macro_f1"], row["game_precision"], row["game_recall"], -row["threshold"]),
        )
    else:
        selected = max(
            rows,
            key=lambda row: (row["macro_f1"], row["game_precision"], row["game_recall"], -row["threshold"]),
        )
    return float(selected["threshold"]), selected, rows


def safe_split_count(labels, groups, requested: int) -> int:
    by_class: dict[str, set[str]] = defaultdict(set)
    for label, group in zip(labels, groups):
        by_class[str(label)].add(str(group))
    possible = min((len(values) for values in by_class.values()), default=0)
    folds = min(requested, possible)
    if folds < 2:
        raise ValueError(f"not enough independent families for nested verifier CV: {dict((k, len(v)) for k, v in by_class.items())}")
    return folds


def full_metrics(labels, predictions, deps) -> dict[str, float]:
    return {
        "accuracy": float(deps["accuracy_score"](labels, predictions)),
        "balanced_accuracy": float(deps["balanced_accuracy_score"](labels, predictions)),
        "macro_f1": float(deps["f1_score"](labels, predictions, average="macro", zero_division=0)),
    }


def game_precision(labels, predictions) -> float:
    predicted = [index for index, value in enumerate(predictions) if str(value) == "game"]
    if not predicted:
        return 0.0
    return sum(str(labels[index]) == "game" for index in predicted) / len(predicted)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("base_oof", type=Path, help="v5.2 family-held-out oof_predictions.jsonl")
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--evidence", type=Path)
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--inner-folds", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--target-game-precision", type=float, default=0.40)
    parser.add_argument("--max-inner-macro-loss", type=float, default=0.01)
    args = parser.parse_args()

    deps = _require_dependencies()
    np = deps["np"]
    rows = _load_dataset(
        args.dataset_dir,
        args.families,
        task="code_domain",
        classes=["game", "engine", "runtime_library", "media_library"],
    )
    external = load_evidence(args.evidence)
    base_oof = read_oof(args.base_oof)

    texts = np.asarray(
        [augment_text(row["text"], external.get(row["id"], {}))[0] for row in rows],
        dtype=object,
    )
    labels = np.asarray([row["label"] for row in rows], dtype=object)
    groups = np.asarray([row["family_id"] for row in rows], dtype=object)
    ids = [row["id"] for row in rows]

    missing = [record_id for record_id in ids if record_id not in base_oof]
    if missing:
        raise ValueError(f"base OOF is missing {len(missing)} dataset rows; first={missing[0]}")
    for index, record_id in enumerate(ids):
        oof_truth = base_oof[record_id].get("truth")
        if oof_truth is not None and str(oof_truth) != str(labels[index]):
            raise ValueError(f"truth mismatch for {record_id}: dataset={labels[index]} oof={oof_truth}")

    base_predictions = np.asarray([str(base_oof[record_id]["prediction"]) for record_id in ids], dtype=object)
    corrected = base_predictions.copy()
    verifier_probability = np.full(len(rows), np.nan, dtype=np.float64)
    verifier_threshold = np.full(len(rows), np.nan, dtype=np.float64)
    verifier_fold = np.full(len(rows), -1, dtype=np.int64)

    boundary_indices = np.asarray(
        [index for index, label in enumerate(labels.tolist()) if str(label) in PAIR],
        dtype=np.int64,
    )
    boundary_labels = labels[boundary_indices]
    boundary_groups = groups[boundary_indices]
    outer_folds = safe_split_count(boundary_labels.tolist(), boundary_groups.tolist(), args.folds)
    splitter = deps["StratifiedGroupKFold"](
        n_splits=outer_folds,
        shuffle=True,
        random_state=args.seed + 5300,
    )

    fold_reports: list[dict[str, Any]] = []
    for outer_fold, (train_local, test_local) in enumerate(
        splitter.split(boundary_indices, boundary_labels, boundary_groups), 1
    ):
        train_indices = boundary_indices[train_local]
        test_indices = boundary_indices[test_local]
        train_labels = labels[train_indices]
        train_groups = groups[train_indices]
        inner_folds = safe_split_count(train_labels.tolist(), train_groups.tolist(), args.inner_folds)
        inner = deps["StratifiedGroupKFold"](
            n_splits=inner_folds,
            shuffle=True,
            random_state=args.seed + 5300 + outer_fold,
        )
        inner_probs = np.zeros(len(train_indices), dtype=np.float64)
        for inner_fold, (inner_train_local, inner_valid_local) in enumerate(
            inner.split(train_indices, train_labels, train_groups), 1
        ):
            inner_train = train_indices[inner_train_local]
            inner_valid = train_indices[inner_valid_local]
            verifier = _make_model(deps, args.seed + outer_fold * 100 + inner_fold)
            verifier.fit(texts[inner_train].tolist(), labels[inner_train].tolist())
            inner_probs[inner_valid_local] = game_probabilities(
                verifier, texts[inner_valid].tolist(), np
            )

        threshold, selected, _ = choose_threshold(
            inner_probs,
            train_labels.tolist(),
            f1_score=deps["f1_score"],
            target_game_precision=args.target_game_precision,
            max_macro_loss=args.max_inner_macro_loss,
        )

        verifier = _make_model(deps, args.seed + 9000 + outer_fold)
        verifier.fit(texts[train_indices].tolist(), labels[train_indices].tolist())
        test_probs = game_probabilities(verifier, texts[test_indices].tolist(), np)
        for local_index, global_index in enumerate(test_indices.tolist()):
            probability = float(test_probs[local_index])
            verifier_probability[global_index] = probability
            verifier_threshold[global_index] = threshold
            verifier_fold[global_index] = outer_fold
            if str(base_predictions[global_index]) == "game" and probability < threshold:
                corrected[global_index] = "engine"

        fold_base = base_predictions[test_indices]
        fold_corrected = corrected[test_indices]
        fold_reports.append(
            {
                "fold": outer_fold,
                "test_examples": int(len(test_indices)),
                "train_families": int(len(set(groups[train_indices].tolist()))),
                "test_families": int(len(set(groups[test_indices].tolist()))),
                "threshold": threshold,
                "inner_selected": selected,
                "base": pair_metrics(labels[test_indices].tolist(), fold_base.tolist(), deps["f1_score"]),
                "verified": pair_metrics(labels[test_indices].tolist(), fold_corrected.tolist(), deps["f1_score"]),
            }
        )

    if np.isnan(verifier_probability[boundary_indices]).any():
        raise RuntimeError("nested verifier did not cover every game/engine row")

    base_full = full_metrics(labels, base_predictions, deps)
    verified_full = full_metrics(labels, corrected, deps)
    base_game_precision = game_precision(labels, base_predictions)
    verified_game_precision = game_precision(labels, corrected)

    def transition_counts(predictions) -> Counter:
        counts: Counter = Counter()
        for index in boundary_indices.tolist():
            truth = str(labels[index])
            pred = str(predictions[index])
            if truth != pred and pred in PAIR:
                counts[f"{truth}->{pred}"] += 1
        return counts

    base_transitions = transition_counts(base_predictions)
    verified_transitions = transition_counts(corrected)
    flips = [
        index for index in boundary_indices.tolist() if str(base_predictions[index]) != str(corrected[index])
    ]
    helpful = sum(str(corrected[index]) == str(labels[index]) for index in flips)
    harmful = sum(str(base_predictions[index]) == str(labels[index]) for index in flips)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    rows_path = args.output_dir / "oof_verifier_predictions.jsonl"
    with rows_path.open("w", encoding="utf-8") as handle:
        for index in boundary_indices.tolist():
            handle.write(
                json.dumps(
                    {
                        "format": FORMAT,
                        "id": ids[index],
                        "family_id": str(groups[index]),
                        "truth": str(labels[index]),
                        "base_prediction": str(base_predictions[index]),
                        "verified_prediction": str(corrected[index]),
                        "changed": bool(base_predictions[index] != corrected[index]),
                        "verifier_game_probability": float(verifier_probability[index]),
                        "verifier_threshold": float(verifier_threshold[index]),
                        "outer_fold": int(verifier_fold[index]),
                    },
                    sort_keys=True,
                )
                + "\n"
            )

    summary = {
        "format": FORMAT,
        "functions": len(rows),
        "boundary_functions": int(len(boundary_indices)),
        "families": len(set(groups.tolist())),
        "outer_folds": outer_folds,
        "requested_inner_folds": args.inner_folds,
        "target_game_precision": args.target_game_precision,
        "demotion_only": True,
        "base_metrics": base_full,
        "verified_metrics": verified_full,
        "base_game_precision": base_game_precision,
        "verified_game_precision": verified_game_precision,
        "base_boundary_errors": dict(sorted(base_transitions.items())),
        "verified_boundary_errors": dict(sorted(verified_transitions.items())),
        "flips": len(flips),
        "helpful_flips": helpful,
        "harmful_flips": harmful,
        "folds": fold_reports,
        "training_feedback": False,
        "leakage_policy": {
            "threshold_selected_on_outer_test": False,
            "outer_test_family_seen_by_verifier": False,
            "raw_function_names_used": False,
            "receiver_class_names_used": False,
            "vtable_identities_used": False,
        },
    }
    summary_path = args.output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", FORMAT)
    print("functions:", len(rows))
    print("boundary functions:", len(boundary_indices))
    print("outer folds:", outer_folds)
    for report in fold_reports:
        print(
            f"fold={report['fold']} threshold={report['threshold']:.3f} "
            f"base_macro_f1={report['base']['macro_f1']:.4f} "
            f"verified_macro_f1={report['verified']['macro_f1']:.4f} "
            f"base_game_precision={report['base']['game_precision']:.4f} "
            f"verified_game_precision={report['verified']['game_precision']:.4f}"
        )
    print()
    print(
        "base:",
        f"balanced={base_full['balanced_accuracy']:.4f}",
        f"macro_f1={base_full['macro_f1']:.4f}",
        f"game_precision={base_game_precision:.4f}",
    )
    print(
        "verified:",
        f"balanced={verified_full['balanced_accuracy']:.4f}",
        f"macro_f1={verified_full['macro_f1']:.4f}",
        f"game_precision={verified_game_precision:.4f}",
    )
    print("base boundary errors:", dict(sorted(base_transitions.items())))
    print("verified boundary errors:", dict(sorted(verified_transitions.items())))
    print("flips:", len(flips), "helpful:", helpful, "harmful:", harmful)
    print("training feedback: no")
    print("oof verifier predictions:", rows_path)
    print("summary:", summary_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
