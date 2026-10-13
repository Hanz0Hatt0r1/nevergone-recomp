#!/usr/bin/env python3
"""Nested family-held-out evaluation for classifier + similarity-transfer fusion.

This evaluator answers a narrow production question: may trusted nearest-family
code-domain evidence safely promote functions that the existing classifier sent
to review/escalate?

Nothing in this file changes production routing. Every outer family is excluded
from classifier training, confidence/OOD policy fitting, transfer-threshold
selection, and transfer prototypes. Candidate fusion strategies are reported
separately rather than selected on the same outer results.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from nevergone_nested_routing_cv import collect_predictions, learn_policy
from nevergone_nested_similarity_transfer_cv import (
    _fit_reference as fit_transfer_reference,
    _inner_oof as transfer_inner_oof,
    _score_rows as score_transfer_rows,
    policy_accepts as transfer_policy_accepts,
    select_transfer_policy,
)
from train_nevergone_production_classifier import (
    _load_dataset,
    _make_model,
    _require_dependencies,
    calibrate_confidence,
)
from train_nevergone_production_classifier_v4 import route_action_with_contrastive_ood

FORMAT = "NevergoneNestedEvidenceFusionCV/1"


def summarize_rows(rows: list[dict[str, Any]], selector, *, label_source: str) -> dict[str, Any]:
    selected = [row for row in rows if selector(row)]
    correct = sum(str(row[label_source]) == str(row["truth"]) for row in selected)
    return {
        "examples": len(selected),
        "coverage": len(selected) / len(rows) if rows else 0.0,
        "precision": correct / len(selected) if selected else 0.0,
    }


def summarize_fused_accept(rows: list[dict[str, Any]], promote) -> dict[str, Any]:
    """Combine existing classifier accepts with one fixed promotion rule."""
    accepted: list[tuple[str, str]] = []
    for row in rows:
        if row["classifier_action"] == "accept":
            accepted.append((str(row["classifier_prediction"]), str(row["truth"])))
        elif promote(row):
            accepted.append((str(row["transfer_prediction"]), str(row["truth"])))
    correct = sum(pred == truth for pred, truth in accepted)
    return {
        "examples": len(accepted),
        "coverage": len(accepted) / len(rows) if rows else 0.0,
        "precision": correct / len(accepted) if accepted else 0.0,
    }


def disable_untrusted_classes(
    policies: dict[str, dict[str, Any]], allowed: set[str]
) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for label, policy in policies.items():
        copy = dict(policy)
        if label not in allowed:
            copy["enabled"] = False
        result[label] = copy
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("--task", default="code_domain")
    parser.add_argument("--classes", nargs="+", required=True)
    parser.add_argument(
        "--trusted-transfer-classes",
        nargs="+",
        default=["media_library", "runtime_library"],
    )
    parser.add_argument("--outer-folds", type=int, default=5)
    parser.add_argument("--inner-folds", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--accept-precision", type=float, default=0.95)
    parser.add_argument("--review-precision", type=float, default=0.85)
    parser.add_argument("--transfer-precision", type=float, default=0.95)
    parser.add_argument("--min-class-calibration-examples", type=int, default=25)
    parser.add_argument("--min-class-threshold-examples", type=int, default=10)
    parser.add_argument("--min-ood-threshold-examples", type=int, default=10)
    parser.add_argument("--min-transfer-policy-examples", type=int, default=10)
    args = parser.parse_args()

    if args.outer_folds < 2 or args.inner_folds < 2:
        raise SystemExit("outer/inner folds must be >= 2")
    if args.review_precision > args.accept_precision:
        raise SystemExit("--review-precision must be <= --accept-precision")
    if not (0.0 < args.transfer_precision <= 1.0):
        raise SystemExit("--transfer-precision must be in (0,1]")

    trusted_classes = set(args.trusted_transfer_classes)
    unknown = trusted_classes - set(args.classes)
    if unknown:
        raise SystemExit(f"trusted transfer classes not in --classes: {sorted(unknown)}")

    deps = _require_dependencies()
    np = deps["np"]
    rows = _load_dataset(
        args.dataset_dir, args.families, task=args.task, classes=args.classes
    )
    texts = np.asarray([row["text"] for row in rows], dtype=object)
    labels = np.asarray([row["label"] for row in rows], dtype=object)
    groups = np.asarray([row["family_id"] for row in rows], dtype=object)
    dummy = np.arange(len(rows))

    print("model:", FORMAT)
    print("task:", args.task)
    print("functions:", len(rows))
    print("families:", len(set(groups.tolist())))
    print("classes:", Counter(labels.tolist()))
    print("trusted transfer classes:", sorted(trusted_classes))
    print()

    outer = deps["StratifiedGroupKFold"](
        n_splits=args.outer_folds,
        shuffle=True,
        random_state=args.seed,
    )
    all_rows: list[dict[str, Any]] = []

    for outer_fold, (outer_train_idx, outer_test_idx) in enumerate(
        outer.split(dummy, labels, groups), 1
    ):
        outer_train = [rows[int(i)] for i in outer_train_idx]
        outer_test = [rows[int(i)] for i in outer_test_idx]
        train_texts = texts[outer_train_idx]
        train_labels = labels[outer_train_idx]
        train_groups = groups[outer_train_idx]

        # Classifier inner OOF -> confidence/OOD routing policy.
        inner_pred = np.empty(len(outer_train_idx), dtype=object)
        inner_raw = np.zeros(len(outer_train_idx), dtype=np.float64)
        inner_sim = np.zeros(len(outer_train_idx), dtype=np.float64)
        inner_margin = np.zeros(len(outer_train_idx), dtype=np.float64)
        inner_splitter = deps["StratifiedGroupKFold"](
            n_splits=args.inner_folds,
            shuffle=True,
            random_state=args.seed + 1000 + outer_fold,
        )
        for inner_fold, (fit_idx, policy_idx) in enumerate(
            inner_splitter.split(train_texts, train_labels, train_groups), 1
        ):
            model = _make_model(deps, args.seed + outer_fold * 100 + inner_fold)
            fit_text = train_texts[fit_idx].tolist()
            fit_label = train_labels[fit_idx].tolist()
            fit_group = train_groups[fit_idx].tolist()
            policy_text = train_texts[policy_idx].tolist()
            model.fit(fit_text, fit_label)
            scored = collect_predictions(
                model=model,
                train_texts=fit_text,
                train_labels=fit_label,
                train_groups=fit_group,
                test_texts=policy_text,
                classes=args.classes,
                np=np,
            )
            inner_pred[policy_idx] = scored["predictions"]
            inner_raw[policy_idx] = scored["raw_confidence"]
            inner_sim[policy_idx] = scored["pred_similarity"]
            inner_margin[policy_idx] = scored["margin"]

        global_calibrator, class_calibrators, classifier_policies = learn_policy(
            predictions=inner_pred,
            raw_confidence=inner_raw,
            pred_similarity=inner_sim,
            margin=inner_margin,
            truth=train_labels,
            classes=args.classes,
            deps=deps,
            accept_precision=args.accept_precision,
            review_precision=args.review_precision,
            min_class_calibration_examples=args.min_class_calibration_examples,
            min_class_threshold_examples=args.min_class_threshold_examples,
            min_ood_threshold_examples=args.min_ood_threshold_examples,
        )

        # Transfer inner OOF -> per predicted class similarity/gap policy.
        transfer_inner = transfer_inner_oof(
            deps,
            np,
            deps["StratifiedGroupKFold"],
            outer_train,
            args.inner_folds,
            args.seed + outer_fold * 2003,
        )
        transfer_policies = disable_untrusted_classes(
            select_transfer_policy(
                transfer_inner,
                target_precision=args.transfer_precision,
                min_examples=args.min_transfer_policy_examples,
            ),
            trusted_classes,
        )

        # Fit both evidence sources on all outer-train families, then touch outer test once.
        model = _make_model(deps, args.seed + outer_fold)
        model.fit(train_texts.tolist(), train_labels.tolist())
        classifier_scored = collect_predictions(
            model=model,
            train_texts=train_texts.tolist(),
            train_labels=train_labels.tolist(),
            train_groups=train_groups.tolist(),
            test_texts=texts[outer_test_idx].tolist(),
            classes=args.classes,
            np=np,
        )
        transfer_reference = fit_transfer_reference(deps, outer_train)
        transfer_scored = score_transfer_rows(deps, np, transfer_reference, outer_test)

        fold_rows: list[dict[str, Any]] = []
        for local_index, global_index in enumerate(outer_test_idx):
            classifier_prediction = str(classifier_scored["predictions"][local_index])
            confidence, _ = calibrate_confidence(
                classifier_prediction,
                float(classifier_scored["raw_confidence"][local_index]),
                global_calibrator=global_calibrator,
                class_calibrators=class_calibrators,
            )
            classifier_action = route_action_with_contrastive_ood(
                classifier_prediction,
                float(confidence),
                float(classifier_scored["pred_similarity"][local_index]),
                float(classifier_scored["margin"][local_index]),
                classifier_policies,
            )

            transfer_row = transfer_scored[local_index]
            transfer_prediction = str(transfer_row["prediction"])
            transfer_trusted = (
                transfer_prediction in trusted_classes
                and transfer_policy_accepts(transfer_row, transfer_policies)
            )
            record = {
                "id": rows[int(global_index)]["id"],
                "truth": str(labels[int(global_index)]),
                "outer_fold": outer_fold,
                "classifier_prediction": classifier_prediction,
                "classifier_action": classifier_action,
                "classifier_correct": classifier_prediction == str(labels[int(global_index)]),
                "transfer_prediction": transfer_prediction,
                "transfer_trusted": bool(transfer_trusted),
                "transfer_correct": transfer_prediction == str(labels[int(global_index)]),
                "transfer_similarity": float(transfer_row["similarity"]),
                "transfer_gap": float(transfer_row["gap"]),
            }
            fold_rows.append(record)
            all_rows.append(record)

        trusted_nonaccept = summarize_rows(
            fold_rows,
            lambda r: r["classifier_action"] != "accept" and r["transfer_trusted"],
            label_source="transfer_prediction",
        )
        agree_nonaccept = summarize_rows(
            fold_rows,
            lambda r: r["classifier_action"] != "accept"
            and r["transfer_trusted"]
            and r["classifier_prediction"] == r["transfer_prediction"],
            label_source="transfer_prediction",
        )
        print(
            f"outer_fold={outer_fold} n={len(fold_rows)} "
            f"trusted_nonaccept={trusted_nonaccept} agree_nonaccept={agree_nonaccept}"
        )

    print("\nBASE CLASSIFIER ACCEPT")
    print(
        summarize_rows(
            all_rows,
            lambda r: r["classifier_action"] == "accept",
            label_source="classifier_prediction",
        )
    )

    print("\nPROMOTION CANDIDATES")
    promotion_rules = {
        "trusted_agreement_nonaccept": lambda r: r["classifier_action"] != "accept"
        and r["transfer_trusted"]
        and r["classifier_prediction"] == r["transfer_prediction"],
        "trusted_override_review": lambda r: r["classifier_action"] == "review"
        and r["transfer_trusted"],
        "trusted_override_escalate": lambda r: r["classifier_action"] == "escalate"
        and r["transfer_trusted"],
        "trusted_override_any_nonaccept": lambda r: r["classifier_action"] != "accept"
        and r["transfer_trusted"],
    }
    for name, rule in promotion_rules.items():
        print(name, summarize_rows(all_rows, rule, label_source="transfer_prediction"))

    print("\nFUSED ACCEPT STRATEGIES")
    for name, rule in promotion_rules.items():
        print(name, summarize_fused_accept(all_rows, rule))

    print("\nTRUSTED NONACCEPT BY TRANSFER LABEL")
    for label in sorted(trusted_classes):
        selected = [
            row for row in all_rows
            if row["classifier_action"] != "accept"
            and row["transfer_trusted"]
            and row["transfer_prediction"] == label
        ]
        correct = sum(row["transfer_correct"] for row in selected)
        print(
            f"  {label}: examples={len(selected)} correct={correct} "
            f"precision={correct/len(selected) if selected else 0.0:.4f}"
        )

    print("\nTRUSTED NONACCEPT CLASSIFIER -> TRANSFER PAIRS")
    pair_rows: dict[tuple[str, str], list[dict[str, Any]]] = defaultdict(list)
    for row in all_rows:
        if row["classifier_action"] != "accept" and row["transfer_trusted"]:
            pair_rows[(row["classifier_prediction"], row["transfer_prediction"])].append(row)
    for pair, selected in sorted(pair_rows.items(), key=lambda item: (-len(item[1]), item[0])):
        correct = sum(row["transfer_correct"] for row in selected)
        print(
            f"  {pair[0]} -> {pair[1]}: examples={len(selected)} correct={correct} "
            f"precision={correct/len(selected) if selected else 0.0:.4f}"
        )

    print("\nPROMOTED ERRORS: TRUSTED OVERRIDE ANY NONACCEPT")
    errors = [
        row for row in all_rows
        if row["classifier_action"] != "accept"
        and row["transfer_trusted"]
        and not row["transfer_correct"]
    ]
    for row in sorted(
        errors,
        key=lambda r: (-float(r["transfer_similarity"]), -float(r["transfer_gap"])),
    )[:30]:
        print(
            f"  {row['id']} truth={row['truth']} "
            f"classifier={row['classifier_prediction']}:{row['classifier_action']} "
            f"transfer={row['transfer_prediction']} "
            f"sim={row['transfer_similarity']:.4f} gap={row['transfer_gap']:.4f} "
            f"fold={row['outer_fold']}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
