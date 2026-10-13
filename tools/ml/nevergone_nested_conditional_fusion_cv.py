#!/usr/bin/env python3
"""Nested family-held-out evaluation of conditional classifier/transfer promotion.

This is a stricter follow-up to NevergoneNestedEvidenceFusionCV/1.  The base
classifier routing policy and nearest-family transfer policy are learned on
inner family-grouped OOF predictions.  A second conditional policy is then
learned only from classifier non-accept rows that already satisfy the trusted
transfer gate.  The untouched outer families are used exactly once to evaluate
promotion precision.

Two predeclared policy scopes are reported independently:
- per_label: one additional similarity/gap policy per trusted transfer label;
- per_label_action: one policy per trusted label and classifier action
  (review/escalate).

The evaluator never changes production routing.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from build_nevergone_similarity_index import require_dependencies as _require_similarity_dependencies
from nevergone_nested_evidence_fusion_cv import (
    disable_untrusted_classes,
    merge_dependency_contracts,
    summarize_fused_accept,
    summarize_rows,
)
from nevergone_nested_routing_cv import collect_predictions, learn_policy
from nevergone_nested_similarity_transfer_cv import (
    GAP_THRESHOLDS,
    SIM_THRESHOLDS,
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

FORMAT = "NevergoneNestedConditionalFusionCV/1"


def _policy_key(row: dict[str, Any], scope: str) -> tuple[str, ...]:
    label = str(row["transfer_prediction"])
    if scope == "per_label":
        return (label,)
    if scope == "per_label_action":
        return (label, str(row["classifier_action"]))
    raise ValueError(f"unknown policy scope: {scope}")


def select_conditional_policies(
    rows: list[dict[str, Any]],
    *,
    scope: str,
    target_precision: float,
    min_examples: int,
) -> dict[tuple[str, ...], dict[str, Any]]:
    """Learn the highest-coverage stricter gate within trusted non-accept rows."""
    eligible = [
        row for row in rows
        if row["classifier_action"] != "accept" and bool(row["transfer_trusted"])
    ]
    keys = sorted({_policy_key(row, scope) for row in eligible})
    policies: dict[tuple[str, ...], dict[str, Any]] = {}
    for key in keys:
        group = [row for row in eligible if _policy_key(row, scope) == key]
        best: tuple[int, float, float, float] | None = None
        for sim_threshold in SIM_THRESHOLDS:
            for gap_threshold in GAP_THRESHOLDS:
                selected = [
                    row for row in group
                    if float(row["transfer_similarity"]) >= sim_threshold
                    and float(row["transfer_gap"]) >= gap_threshold
                ]
                if len(selected) < min_examples:
                    continue
                precision = sum(bool(row["transfer_correct"]) for row in selected) / len(selected)
                if precision + 1e-12 < target_precision:
                    continue
                candidate = (len(selected), precision, sim_threshold, gap_threshold)
                if best is None or candidate > best:
                    best = candidate
        if best is None:
            policies[key] = {
                "enabled": False,
                "examples": 0,
                "precision": None,
                "similarity_threshold": None,
                "gap_threshold": None,
            }
        else:
            examples, precision, sim_threshold, gap_threshold = best
            policies[key] = {
                "enabled": True,
                "examples": examples,
                "precision": precision,
                "similarity_threshold": sim_threshold,
                "gap_threshold": gap_threshold,
            }
    return policies


def conditional_accepts(
    row: dict[str, Any],
    policies: dict[tuple[str, ...], dict[str, Any]],
    *,
    scope: str,
) -> bool:
    if row["classifier_action"] == "accept" or not bool(row["transfer_trusted"]):
        return False
    policy = policies.get(_policy_key(row, scope))
    if not policy or not policy.get("enabled"):
        return False
    return (
        float(row["transfer_similarity"]) >= float(policy["similarity_threshold"])
        and float(row["transfer_gap"]) >= float(policy["gap_threshold"])
    )


def _render_policies(policies: dict[tuple[str, ...], dict[str, Any]]) -> dict[str, Any]:
    rendered: dict[str, Any] = {}
    for key, policy in sorted(policies.items()):
        if not policy.get("enabled"):
            continue
        rendered["/".join(key)] = {
            "sim": policy["similarity_threshold"],
            "gap": policy["gap_threshold"],
            "inner_n": policy["examples"],
            "inner_precision": policy["precision"],
        }
    return rendered


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
    parser.add_argument("--conditional-precision", type=float, default=0.95)
    parser.add_argument("--min-class-calibration-examples", type=int, default=25)
    parser.add_argument("--min-class-threshold-examples", type=int, default=10)
    parser.add_argument("--min-ood-threshold-examples", type=int, default=10)
    parser.add_argument("--min-transfer-policy-examples", type=int, default=10)
    parser.add_argument("--min-conditional-policy-examples", type=int, default=10)
    args = parser.parse_args()

    if args.outer_folds < 2 or args.inner_folds < 2:
        raise SystemExit("outer/inner folds must be >= 2")
    if args.review_precision > args.accept_precision:
        raise SystemExit("--review-precision must be <= --accept-precision")
    for name, value in (
        ("transfer", args.transfer_precision),
        ("conditional", args.conditional_precision),
    ):
        if not (0.0 < value <= 1.0):
            raise SystemExit(f"--{name}-precision must be in (0,1]")
    if args.min_conditional_policy_examples < 1:
        raise SystemExit("--min-conditional-policy-examples must be >= 1")

    trusted_classes = set(args.trusted_transfer_classes)
    unknown = trusted_classes - set(args.classes)
    if unknown:
        raise SystemExit(f"trusted transfer classes not in --classes: {sorted(unknown)}")

    deps = merge_dependency_contracts(
        _require_dependencies(),
        _require_similarity_dependencies(),
    )
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
    print("conditional target precision:", args.conditional_precision)
    print("min conditional examples:", args.min_conditional_policy_examples)
    print()

    outer = deps["StratifiedGroupKFold"](
        n_splits=args.outer_folds,
        shuffle=True,
        random_state=args.seed,
    )
    all_rows: list[dict[str, Any]] = []
    policy_history: dict[str, dict[tuple[str, ...], list[dict[str, Any]]]] = {
        "per_label": defaultdict(list),
        "per_label_action": defaultdict(list),
    }

    for outer_fold, (outer_train_idx, outer_test_idx) in enumerate(
        outer.split(dummy, labels, groups), 1
    ):
        outer_train = [rows[int(i)] for i in outer_train_idx]
        outer_test = [rows[int(i)] for i in outer_test_idx]
        train_texts = texts[outer_train_idx]
        train_labels = labels[outer_train_idx]
        train_groups = groups[outer_train_idx]

        # Classifier inner OOF predictions.
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

        # Transfer inner OOF and its base trusted policy.
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
        transfer_by_id = {str(row["id"]): row for row in transfer_inner}

        # Build inner fusion rows and learn the second conditional gate only here.
        inner_fusion: list[dict[str, Any]] = []
        for index, source in enumerate(outer_train):
            classifier_prediction = str(inner_pred[index])
            confidence, _ = calibrate_confidence(
                classifier_prediction,
                float(inner_raw[index]),
                global_calibrator=global_calibrator,
                class_calibrators=class_calibrators,
            )
            classifier_action = route_action_with_contrastive_ood(
                classifier_prediction,
                float(confidence),
                float(inner_sim[index]),
                float(inner_margin[index]),
                classifier_policies,
            )
            transfer_row = transfer_by_id[str(source["id"])]
            transfer_prediction = str(transfer_row["prediction"])
            transfer_trusted = (
                transfer_prediction in trusted_classes
                and transfer_policy_accepts(transfer_row, transfer_policies)
            )
            inner_fusion.append({
                "id": source["id"],
                "truth": source["label"],
                "classifier_prediction": classifier_prediction,
                "classifier_action": classifier_action,
                "transfer_prediction": transfer_prediction,
                "transfer_trusted": bool(transfer_trusted),
                "transfer_correct": bool(transfer_row["correct"]),
                "transfer_similarity": float(transfer_row["similarity"]),
                "transfer_gap": float(transfer_row["gap"]),
            })

        conditional: dict[str, dict[tuple[str, ...], dict[str, Any]]] = {}
        for scope in ("per_label", "per_label_action"):
            policies = select_conditional_policies(
                inner_fusion,
                scope=scope,
                target_precision=args.conditional_precision,
                min_examples=args.min_conditional_policy_examples,
            )
            conditional[scope] = policies
            for key, policy in policies.items():
                policy_history[scope][key].append(policy)

        # Fit both evidence sources on all outer-train families, then score outer once.
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
            for scope in ("per_label", "per_label_action"):
                record[f"promote_{scope}"] = conditional_accepts(
                    record, conditional[scope], scope=scope
                )
            fold_rows.append(record)
            all_rows.append(record)

        print(
            f"outer_fold={outer_fold} n={len(fold_rows)} "
            f"per_label={summarize_rows(fold_rows, lambda r: r['promote_per_label'], label_source='transfer_prediction')} "
            f"per_label_action={summarize_rows(fold_rows, lambda r: r['promote_per_label_action'], label_source='transfer_prediction')}"
        )
        print("  per_label policies:", _render_policies(conditional["per_label"]))
        print("  per_label_action policies:", _render_policies(conditional["per_label_action"]))

    base = summarize_rows(
        all_rows,
        lambda r: r["classifier_action"] == "accept",
        label_source="classifier_prediction",
    )
    print("\nBASE CLASSIFIER ACCEPT")
    print(base)

    print("\nCONDITIONAL PROMOTION")
    for scope in ("per_label", "per_label_action"):
        selector = lambda r, s=scope: bool(r[f"promote_{s}"])
        print(scope, summarize_rows(all_rows, selector, label_source="transfer_prediction"))

    print("\nFUSED ACCEPT")
    for scope in ("per_label", "per_label_action"):
        selector = lambda r, s=scope: bool(r[f"promote_{s}"])
        print(scope, summarize_fused_accept(all_rows, selector))

    print("\nPROMOTION BY LABEL/ACTION")
    for scope in ("per_label", "per_label_action"):
        print(scope)
        selected = [row for row in all_rows if row[f"promote_{scope}"]]
        grouped: dict[tuple[str, str], list[dict[str, Any]]] = defaultdict(list)
        for row in selected:
            grouped[(row["transfer_prediction"], row["classifier_action"])].append(row)
        for key, group in sorted(grouped.items(), key=lambda item: (-len(item[1]), item[0])):
            correct = sum(bool(row["transfer_correct"]) for row in group)
            print(
                f"  {key[0]}/{key[1]}: examples={len(group)} correct={correct} "
                f"precision={correct/len(group) if group else 0.0:.4f}"
            )

    print("\nPOLICY STABILITY")
    for scope in ("per_label", "per_label_action"):
        print(scope)
        for key, history in sorted(policy_history[scope].items()):
            enabled = [policy for policy in history if policy.get("enabled")]
            sims = sorted({policy["similarity_threshold"] for policy in enabled})
            gaps = sorted({policy["gap_threshold"] for policy in enabled})
            precisions = [float(policy["precision"]) for policy in enabled]
            print(
                f"  {'/'.join(key)}: enabled_folds={len(enabled)}/{len(history)} "
                f"sim_thresholds={sims} gap_thresholds={gaps} "
                f"inner_precision_min={min(precisions) if precisions else None}"
            )

    print("\nPROMOTED ERRORS")
    for scope in ("per_label", "per_label_action"):
        print(scope)
        errors = [
            row for row in all_rows
            if row[f"promote_{scope}"] and not row["transfer_correct"]
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
