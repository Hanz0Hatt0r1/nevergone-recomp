#!/usr/bin/env python3
"""Evaluate a Pareto-constrained nested verifier for the game/engine boundary.

v5.4 proved that a selective verifier can improve game precision and macro-F1
without sacrificing balanced accuracy, but it remained conservative. v5.5 keeps
exactly the same leakage-resistant outer/inner family-grouped structure and
expands only the policy search.

For every outer fold the four-class v5.2 base model and binary game/engine
verifier are rebuilt from outer-train data. Policy selection uses only inner OOF
predictions. The outer test fold is never used to fit models or select policy.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

from evaluate_nevergone_selective_game_verifier_v54 import (
    CLASSES,
    PAIR,
    align_probabilities,
    full_metrics,
    game_metrics,
    read_oof,
    safe_split_count,
    transition_counts,
    verifier_game_probabilities,
)
from train_nevergone_production_classifier import _load_dataset, _make_model, _require_dependencies
from train_nevergone_production_classifier_v5 import augment_text, load_evidence

FORMAT = "NEVERGONE.ParetoGameVerifierV55/1"


def _margin(base_probabilities, index: int) -> float:
    game_index = CLASSES.index("game")
    engine_index = CLASSES.index("engine")
    return float(base_probabilities[index, game_index] - base_probabilities[index, engine_index])


def _base_game_probability(base_probabilities, index: int) -> float:
    return float(base_probabilities[index, CLASSES.index("game")])


def should_demote(index: int, base_probabilities, verifier_probabilities, policy: dict[str, Any]) -> bool:
    if policy.get("noop"):
        return False
    family = str(policy["family"])
    verifier_game = float(verifier_probabilities[index])
    margin = _margin(base_probabilities, index)

    if family == "single_margin":
        return (
            margin <= float(policy["base_margin_limit"])
            and verifier_game < float(policy["verifier_threshold"])
        )

    if family == "confidence_ceiling":
        return (
            _base_game_probability(base_probabilities, index) <= float(policy["base_game_probability_ceiling"])
            and verifier_game < float(policy["verifier_threshold"])
        )

    if family == "two_stage_margin":
        weak_limit = float(policy["weak_margin_limit"])
        broad_limit = float(policy["broad_margin_limit"])
        if margin <= weak_limit:
            return verifier_game < float(policy["weak_verifier_threshold"])
        if margin <= broad_limit:
            return verifier_game < float(policy["broad_verifier_threshold"])
        return False

    raise ValueError(f"unknown policy family: {family}")


def apply_policy(base_predictions, base_probabilities, verifier_probabilities, policy, np):
    corrected = np.asarray(base_predictions, dtype=object).copy()
    if policy.get("noop"):
        return corrected
    for index in range(len(corrected)):
        if str(base_predictions[index]) != "game":
            continue
        if should_demote(index, base_probabilities, verifier_probabilities, policy):
            corrected[index] = "engine"
    return corrected


def candidate_report(labels, base_predictions, candidate_predictions, deps) -> dict[str, Any]:
    base_game = game_metrics(labels, base_predictions)
    candidate_game = game_metrics(labels, candidate_predictions)
    metrics = full_metrics(labels, candidate_predictions, deps)
    transitions = transition_counts(labels, candidate_predictions)
    flips = [
        index
        for index in range(len(labels))
        if str(base_predictions[index]) != str(candidate_predictions[index])
    ]
    helpful = sum(str(candidate_predictions[index]) == str(labels[index]) for index in flips)
    harmful = sum(str(base_predictions[index]) == str(labels[index]) for index in flips)
    return {
        **metrics,
        "game_precision": candidate_game["precision"],
        "game_recall": candidate_game["recall"],
        "game_f1": candidate_game["f1"],
        "base_game_precision": base_game["precision"],
        "base_game_recall": base_game["recall"],
        "engine_to_game": int(transitions.get("engine->game", 0)),
        "game_to_engine": int(transitions.get("game->engine", 0)),
        "flips": len(flips),
        "helpful_flips": helpful,
        "harmful_flips": harmful,
        "net_helpful_flips": helpful - harmful,
    }


def generate_policies() -> list[dict[str, Any]]:
    policies: list[dict[str, Any]] = [{"noop": True, "family": "noop"}]

    for verifier_threshold in [0.40, 0.45, 0.50, 0.55, 0.60, 0.65, 0.70, 0.75, 0.80]:
        for margin_limit in [0.05, 0.10, 0.15, 0.20, 0.30, 0.50, 1.00]:
            policies.append(
                {
                    "noop": False,
                    "family": "single_margin",
                    "verifier_threshold": verifier_threshold,
                    "base_margin_limit": margin_limit,
                }
            )

    for verifier_threshold in [0.45, 0.50, 0.55, 0.60, 0.65, 0.70, 0.75]:
        for ceiling in [0.35, 0.40, 0.45, 0.50, 0.60, 0.70]:
            policies.append(
                {
                    "noop": False,
                    "family": "confidence_ceiling",
                    "verifier_threshold": verifier_threshold,
                    "base_game_probability_ceiling": ceiling,
                }
            )

    weak_limits = [0.05, 0.10, 0.15, 0.20]
    broad_limits = [0.20, 0.30, 0.50]
    weak_thresholds = [0.60, 0.70, 0.80]
    broad_thresholds = [0.40, 0.50, 0.60]
    for weak_limit in weak_limits:
        for broad_limit in broad_limits:
            if broad_limit <= weak_limit:
                continue
            for weak_threshold in weak_thresholds:
                for broad_threshold in broad_thresholds:
                    if weak_threshold < broad_threshold:
                        continue
                    policies.append(
                        {
                            "noop": False,
                            "family": "two_stage_margin",
                            "weak_margin_limit": weak_limit,
                            "broad_margin_limit": broad_limit,
                            "weak_verifier_threshold": weak_threshold,
                            "broad_verifier_threshold": broad_threshold,
                        }
                    )
    return policies


def _dominates(left: dict[str, Any], right: dict[str, Any]) -> bool:
    maximize = ("balanced_accuracy", "macro_f1", "game_precision", "game_recall", "net_helpful_flips")
    minimize = ("harmful_flips", "engine_to_game")
    no_worse = all(float(left[key]) >= float(right[key]) for key in maximize) and all(
        float(left[key]) <= float(right[key]) for key in minimize
    )
    strictly_better = any(float(left[key]) > float(right[key]) for key in maximize) or any(
        float(left[key]) < float(right[key]) for key in minimize
    )
    return no_worse and strictly_better


def pareto_frontier(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for row in rows:
        if any(_dominates(other, row) for other in rows if other is not row):
            continue
        result.append(row)
    return result


def select_policy(
    labels,
    base_predictions,
    base_probabilities,
    verifier_probabilities,
    *,
    deps,
    np,
    max_balanced_loss: float,
    max_game_recall_loss: float,
    target_game_precision: float,
) -> tuple[dict[str, Any], list[dict[str, Any]], list[dict[str, Any]]]:
    base_full = full_metrics(labels, base_predictions, deps)
    base_game = game_metrics(labels, base_predictions)
    reports: list[dict[str, Any]] = []

    for policy in generate_policies():
        prediction = apply_policy(
            base_predictions,
            base_probabilities,
            verifier_probabilities,
            policy,
            np,
        )
        report = candidate_report(labels, base_predictions, prediction, deps)
        report["policy"] = policy
        report["eligible"] = bool(
            report["balanced_accuracy"] >= base_full["balanced_accuracy"] - max_balanced_loss
            and report["game_recall"] >= base_game["recall"] - max_game_recall_loss
        )
        report["meets_target_precision"] = bool(report["game_precision"] >= target_game_precision)
        reports.append(report)

    eligible = [row for row in reports if row["eligible"]]
    if not eligible:
        eligible = [row for row in reports if row["policy"].get("noop")]

    frontier = pareto_frontier(eligible)
    target = [row for row in frontier if row["meets_target_precision"]]
    pool = target if target else frontier
    selected = max(
        pool,
        key=lambda row: (
            row["macro_f1"],
            row["game_precision"],
            row["balanced_accuracy"],
            row["game_f1"],
            row["net_helpful_flips"],
            -row["engine_to_game"],
            -row["harmful_flips"],
            -row["flips"],
        ),
    )
    return dict(selected["policy"]), reports, frontier


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("base_oof", type=Path, help="v5.2 OOF predictions, used only for reproduction audit")
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--evidence", type=Path)
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--inner-folds", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--max-balanced-loss", type=float, default=0.003)
    parser.add_argument("--max-game-recall-loss", type=float, default=0.04)
    parser.add_argument("--target-game-precision", type=float, default=0.45)
    args = parser.parse_args()

    if args.max_balanced_loss < 0 or args.max_game_recall_loss < 0:
        raise SystemExit("loss constraints must be non-negative")
    if not 0.0 <= args.target_game_precision <= 1.0:
        raise SystemExit("--target-game-precision must be in [0, 1]")

    deps = _require_dependencies()
    np = deps["np"]
    rows = _load_dataset(
        args.dataset_dir,
        args.families,
        task="code_domain",
        classes=CLASSES,
    )
    external = load_evidence(args.evidence)
    reference_oof = read_oof(args.base_oof)

    texts = np.asarray(
        [augment_text(row["text"], external.get(row["id"], {}))[0] for row in rows],
        dtype=object,
    )
    labels = np.asarray([row["label"] for row in rows], dtype=object)
    groups = np.asarray([row["family_id"] for row in rows], dtype=object)
    ids = [row["id"] for row in rows]

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
    verifier_probabilities = np.full(n, np.nan, dtype=np.float64)
    outer_fold_for_row = np.zeros(n, dtype=np.int64)
    policies_for_row: list[dict[str, Any] | None] = [None] * n
    fold_reports: list[dict[str, Any]] = []

    for outer_fold, (outer_train, outer_test) in enumerate(outer.split(texts, labels, groups), 1):
        train_labels = labels[outer_train]
        train_groups = groups[outer_train]
        inner_folds = safe_split_count(train_labels.tolist(), train_groups.tolist(), args.inner_folds)
        inner = deps["StratifiedGroupKFold"](
            n_splits=inner_folds,
            shuffle=True,
            random_state=args.seed + 5500 + outer_fold,
        )

        inner_base_predictions = np.empty(len(outer_train), dtype=object)
        inner_base_probabilities = np.zeros((len(outer_train), len(CLASSES)), dtype=np.float64)
        inner_verifier_probabilities = np.zeros(len(outer_train), dtype=np.float64)

        for inner_fold, (inner_train_local, inner_valid_local) in enumerate(
            inner.split(texts[outer_train], train_labels, train_groups), 1
        ):
            inner_train = outer_train[inner_train_local]
            inner_valid = outer_train[inner_valid_local]

            base_model = _make_model(deps, args.seed + outer_fold * 100 + inner_fold)
            base_model.fit(texts[inner_train].tolist(), labels[inner_train].tolist())
            aligned = align_probabilities(base_model, texts[inner_valid].tolist(), CLASSES, np)
            inner_base_probabilities[inner_valid_local] = aligned
            inner_base_predictions[inner_valid_local] = np.asarray(
                [CLASSES[index] for index in aligned.argmax(axis=1)], dtype=object
            )

            verifier_train = np.asarray(
                [index for index in inner_train.tolist() if str(labels[index]) in PAIR],
                dtype=np.int64,
            )
            verifier = _make_model(deps, args.seed + 550000 + outer_fold * 100 + inner_fold)
            verifier.fit(texts[verifier_train].tolist(), labels[verifier_train].tolist())
            inner_verifier_probabilities[inner_valid_local] = verifier_game_probabilities(
                verifier, texts[inner_valid].tolist(), np
            )

        selected_policy, policy_reports, frontier = select_policy(
            train_labels,
            inner_base_predictions,
            inner_base_probabilities,
            inner_verifier_probabilities,
            deps=deps,
            np=np,
            max_balanced_loss=args.max_balanced_loss,
            max_game_recall_loss=args.max_game_recall_loss,
            target_game_precision=args.target_game_precision,
        )

        base_model = _make_model(deps, args.seed + outer_fold)
        base_model.fit(texts[outer_train].tolist(), labels[outer_train].tolist())
        aligned = align_probabilities(base_model, texts[outer_test].tolist(), CLASSES, np)
        fold_base_predictions = np.asarray(
            [CLASSES[index] for index in aligned.argmax(axis=1)], dtype=object
        )

        verifier_train = np.asarray(
            [index for index in outer_train.tolist() if str(labels[index]) in PAIR],
            dtype=np.int64,
        )
        verifier = _make_model(deps, args.seed + 559000 + outer_fold)
        verifier.fit(texts[verifier_train].tolist(), labels[verifier_train].tolist())
        fold_verifier = verifier_game_probabilities(verifier, texts[outer_test].tolist(), np)
        fold_corrected = apply_policy(
            fold_base_predictions,
            aligned,
            fold_verifier,
            selected_policy,
            np,
        )

        base_predictions[outer_test] = fold_base_predictions
        corrected_predictions[outer_test] = fold_corrected
        base_probabilities[outer_test] = aligned
        verifier_probabilities[outer_test] = fold_verifier
        outer_fold_for_row[outer_test] = outer_fold
        for index in outer_test.tolist():
            policies_for_row[index] = selected_policy

        inner_selected = next(row for row in policy_reports if row["policy"] == selected_policy)
        fold_reports.append(
            {
                "fold": outer_fold,
                "train_examples": int(len(outer_train)),
                "test_examples": int(len(outer_test)),
                "train_families": int(len(set(groups[outer_train].tolist()))),
                "test_families": int(len(set(groups[outer_test].tolist()))),
                "policy": selected_policy,
                "policy_family": selected_policy.get("family", "noop"),
                "inner_selected": inner_selected,
                "inner_pareto_frontier_size": len(frontier),
                "inner_target_precision_candidates": sum(
                    bool(row.get("meets_target_precision")) for row in frontier
                ),
                "base": {
                    **full_metrics(labels[outer_test], fold_base_predictions, deps),
                    "game": game_metrics(labels[outer_test], fold_base_predictions),
                },
                "verified": {
                    **full_metrics(labels[outer_test], fold_corrected, deps),
                    "game": game_metrics(labels[outer_test], fold_corrected),
                },
            }
        )

    if np.isnan(verifier_probabilities).any():
        raise RuntimeError("nested verifier did not cover every row")

    reference_predictions = np.asarray(
        [str(reference_oof[record_id]["prediction"]) for record_id in ids], dtype=object
    )
    reproduction_matches = int((reference_predictions == base_predictions).sum())
    reproduction_rate = reproduction_matches / max(1, n)

    base_full = full_metrics(labels, base_predictions, deps)
    verified_full = full_metrics(labels, corrected_predictions, deps)
    base_game = game_metrics(labels, base_predictions)
    verified_game = game_metrics(labels, corrected_predictions)
    base_transitions = transition_counts(labels.tolist(), base_predictions.tolist())
    verified_transitions = transition_counts(labels.tolist(), corrected_predictions.tolist())

    flips = [
        index for index in range(n)
        if str(base_predictions[index]) != str(corrected_predictions[index])
    ]
    helpful = sum(str(corrected_predictions[index]) == str(labels[index]) for index in flips)
    harmful = sum(str(base_predictions[index]) == str(labels[index]) for index in flips)
    policy_family_counts = Counter(
        str((policies_for_row[index] or {"family": "noop"}).get("family", "noop"))
        for index in range(n)
    )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    rows_path = args.output_dir / "oof_pareto_verifier_predictions.jsonl"
    with rows_path.open("w", encoding="utf-8") as handle:
        for index, record_id in enumerate(ids):
            policy = policies_for_row[index] or {"noop": True, "family": "noop"}
            handle.write(
                json.dumps(
                    {
                        "format": FORMAT,
                        "id": record_id,
                        "family_id": str(groups[index]),
                        "truth": str(labels[index]),
                        "base_prediction": str(base_predictions[index]),
                        "verified_prediction": str(corrected_predictions[index]),
                        "changed": bool(base_predictions[index] != corrected_predictions[index]),
                        "base_game_probability": _base_game_probability(base_probabilities, index),
                        "base_engine_probability": float(base_probabilities[index, CLASSES.index("engine")]),
                        "base_game_engine_margin": _margin(base_probabilities, index),
                        "verifier_game_probability": float(verifier_probabilities[index]),
                        "outer_fold": int(outer_fold_for_row[index]),
                        "policy": policy,
                    },
                    sort_keys=True,
                )
                + "\n"
            )

    summary = {
        "format": FORMAT,
        "functions": n,
        "families": len(set(groups.tolist())),
        "outer_folds": outer_folds,
        "requested_inner_folds": args.inner_folds,
        "constraints": {
            "max_balanced_loss": args.max_balanced_loss,
            "max_game_recall_loss": args.max_game_recall_loss,
            "target_game_precision": args.target_game_precision,
        },
        "base_metrics": base_full,
        "verified_metrics": verified_full,
        "base_game": base_game,
        "verified_game": verified_game,
        "base_boundary_errors": dict(sorted(base_transitions.items())),
        "verified_boundary_errors": dict(sorted(verified_transitions.items())),
        "flips": len(flips),
        "helpful_flips": helpful,
        "harmful_flips": harmful,
        "net_helpful_flips": helpful - harmful,
        "policy_family_rows": dict(sorted(policy_family_counts.items())),
        "reference_v52_reproduction": {
            "matches": reproduction_matches,
            "rows": n,
            "rate": reproduction_rate,
        },
        "folds": fold_reports,
        "training_feedback": False,
        "leakage_policy": {
            "outer_test_used_for_policy_selection": False,
            "outer_test_family_seen_by_base": False,
            "outer_test_family_seen_by_verifier": False,
            "inner_base_recomputed": True,
            "precomputed_base_oof_used_for_selection": False,
            "raw_function_names_used": False,
            "receiver_class_names_used": False,
            "vtable_identities_used": False,
        },
    }
    summary_path = args.output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", FORMAT)
    print("functions:", n)
    print("families:", len(set(groups.tolist())))
    print("outer folds:", outer_folds)
    for report in fold_reports:
        print(
            f"fold={report['fold']} family={report['policy_family']} policy={report['policy']} "
            f"frontier={report['inner_pareto_frontier_size']} "
            f"target_candidates={report['inner_target_precision_candidates']} "
            f"base_balanced={report['base']['balanced_accuracy']:.4f} "
            f"verified_balanced={report['verified']['balanced_accuracy']:.4f} "
            f"base_game_precision={report['base']['game']['precision']:.4f} "
            f"verified_game_precision={report['verified']['game']['precision']:.4f} "
            f"base_game_recall={report['base']['game']['recall']:.4f} "
            f"verified_game_recall={report['verified']['game']['recall']:.4f}"
        )
    print()
    print(
        "base:",
        f"balanced={base_full['balanced_accuracy']:.4f}",
        f"macro_f1={base_full['macro_f1']:.4f}",
        f"game_precision={base_game['precision']:.4f}",
        f"game_recall={base_game['recall']:.4f}",
        f"game_f1={base_game['f1']:.4f}",
    )
    print(
        "pareto:",
        f"balanced={verified_full['balanced_accuracy']:.4f}",
        f"macro_f1={verified_full['macro_f1']:.4f}",
        f"game_precision={verified_game['precision']:.4f}",
        f"game_recall={verified_game['recall']:.4f}",
        f"game_f1={verified_game['f1']:.4f}",
    )
    print("base boundary errors:", dict(sorted(base_transitions.items())))
    print("pareto boundary errors:", dict(sorted(verified_transitions.items())))
    print("flips:", len(flips), "helpful:", helpful, "harmful:", harmful)
    print("policy family rows:", dict(sorted(policy_family_counts.items())))
    print("v5.2 OOF reproduction rate:", round(reproduction_rate, 6))
    print("training feedback: no")
    print("rows:", rows_path)
    print("summary:", summary_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
