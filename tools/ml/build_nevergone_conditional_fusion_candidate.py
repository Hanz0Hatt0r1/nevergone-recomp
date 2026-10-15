#!/usr/bin/env python3
"""Freeze an exact conditional-fusion policy after the readiness gate passes.

This builder consumes only development-set OOF evidence.  Readiness decides which
predeclared label/action keys are allowed to become candidates; this script then
learns the exact transfer and conditional thresholds on the full development OOF
surface and freezes them for later independent validation.

It never changes production routing and never consumes independent-validation
labels.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.ConditionalFusionCandidate/1"
READINESS_FORMAT = "NEVERGONE.ConditionalFusionReadiness/1"
PRODUCTION_FORMAT = "NEVERGONE.ProductionClassifier/4"
TRANSFER_OOF_FORMAT = "NEVERGONE.SimilarityTransferOOF/1"
SIM_THRESHOLDS = (0.98, 0.95, 0.92, 0.90, 0.88, 0.85, 0.82, 0.80, 0.75, 0.70, 0.60)
GAP_THRESHOLDS = (0.30, 0.20, 0.15, 0.10, 0.075, 0.05, 0.025, 0.00)


def read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected JSON object")
    return value


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open(encoding="utf-8") as handle:
        for line_no, line in enumerate(handle, 1):
            if not line.strip():
                continue
            value = json.loads(line)
            if not isinstance(value, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            rows.append(value)
    return rows


def canonical_sha256(value: Any) -> str:
    payload = json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def index_unique(rows: list[dict[str, Any]], *, path: Path, required: tuple[str, ...]) -> dict[str, dict[str, Any]]:
    output: dict[str, dict[str, Any]] = {}
    for line_no, row in enumerate(rows, 1):
        for field in required:
            if field not in row:
                raise ValueError(f"{path}:{line_no}: missing {field}")
        identity = row.get("id")
        if not isinstance(identity, str) or not identity:
            raise ValueError(f"{path}:{line_no}: invalid id")
        if identity in output:
            raise ValueError(f"{path}: duplicate id: {identity}")
        output[identity] = row
    return output


def select_policy(
    rows: list[dict[str, Any]],
    *,
    prediction_field: str,
    correct_field: str,
    similarity_field: str,
    gap_field: str,
    target_precision: float,
    min_examples: int,
) -> dict[str, dict[str, Any]]:
    policies: dict[str, dict[str, Any]] = {}
    labels = sorted({str(row[prediction_field]) for row in rows})
    for label in labels:
        group = [row for row in rows if str(row[prediction_field]) == label]
        best: tuple[int, float, float, float] | None = None
        for sim_threshold in SIM_THRESHOLDS:
            for gap_threshold in GAP_THRESHOLDS:
                selected = [
                    row for row in group
                    if float(row[similarity_field]) >= sim_threshold
                    and float(row[gap_field]) >= gap_threshold
                ]
                if len(selected) < min_examples:
                    continue
                precision = sum(bool(row[correct_field]) for row in selected) / len(selected)
                if precision + 1e-12 < target_precision:
                    continue
                candidate = (len(selected), precision, sim_threshold, gap_threshold)
                if best is None or candidate > best:
                    best = candidate
        if best is None:
            policies[label] = {
                "enabled": False,
                "examples": 0,
                "precision": None,
                "similarity_threshold": None,
                "gap_threshold": None,
            }
        else:
            examples, precision, sim_threshold, gap_threshold = best
            policies[label] = {
                "enabled": True,
                "examples": examples,
                "precision": precision,
                "similarity_threshold": sim_threshold,
                "gap_threshold": gap_threshold,
            }
    return policies


def policy_accepts(row: dict[str, Any], policy: dict[str, Any]) -> bool:
    return bool(policy.get("enabled")) and (
        float(row["transfer_similarity"]) >= float(policy["similarity_threshold"])
        and float(row["transfer_gap"]) >= float(policy["gap_threshold"])
    )


def select_conditional_policy(
    rows: list[dict[str, Any]],
    *,
    key: str,
    target_precision: float,
    min_examples: int,
) -> dict[str, Any]:
    try:
        label, action = key.rsplit("/", 1)
    except ValueError as exc:
        raise ValueError(f"invalid per_label_action key: {key}") from exc
    if action not in {"review", "escalate"}:
        raise ValueError(f"candidate key action must be review/escalate: {key}")

    group = [
        row for row in rows
        if row["classifier_action"] == action
        and row["transfer_prediction"] == label
        and bool(row["transfer_trusted"])
    ]
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
        return {
            "enabled": False,
            "examples": 0,
            "precision": None,
            "similarity_threshold": None,
            "gap_threshold": None,
        }
    examples, precision, sim_threshold, gap_threshold = best
    return {
        "enabled": True,
        "examples": examples,
        "precision": precision,
        "similarity_threshold": sim_threshold,
        "gap_threshold": gap_threshold,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("readiness_json", type=Path)
    parser.add_argument("production_manifest_json", type=Path)
    parser.add_argument("classifier_oof_jsonl", type=Path)
    parser.add_argument("transfer_oof_manifest_json", type=Path)
    parser.add_argument("transfer_oof_jsonl", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--transfer-precision", type=float, default=0.95)
    parser.add_argument("--conditional-precision", type=float, default=0.95)
    parser.add_argument("--min-transfer-examples", type=int, default=10)
    parser.add_argument("--min-conditional-examples", type=int, default=10)
    args = parser.parse_args()

    for name, value in (
        ("transfer_precision", args.transfer_precision),
        ("conditional_precision", args.conditional_precision),
    ):
        if not (0.0 < value <= 1.0):
            raise SystemExit(f"--{name.replace('_', '-')} must be in (0,1]")
    if args.min_transfer_examples < 1 or args.min_conditional_examples < 1:
        raise SystemExit("minimum evidence counts must be >= 1")

    readiness = read_json(args.readiness_json)
    production = read_json(args.production_manifest_json)
    transfer_manifest = read_json(args.transfer_oof_manifest_json)

    if readiness.get("format") != READINESS_FORMAT:
        raise SystemExit("readiness format mismatch")
    if readiness.get("scope") != "per_label_action":
        raise SystemExit("candidate builder requires per_label_action readiness")
    if readiness.get("ready_for_independent_validation") is not True:
        raise SystemExit("readiness gate did not pass")
    if readiness.get("production_routing_changed") is not False:
        raise SystemExit("readiness artifact unexpectedly changed production routing")
    eligible = readiness.get("eligible_policy_keys")
    if not isinstance(eligible, list) or not eligible or not all(isinstance(key, str) for key in eligible):
        raise SystemExit("readiness has no eligible policy keys")
    if len(set(eligible)) != len(eligible):
        raise SystemExit("readiness contains duplicate eligible policy keys")

    if production.get("format") != PRODUCTION_FORMAT:
        raise SystemExit("production classifier must be NEVERGONE.ProductionClassifier/4")
    if production.get("task") != readiness.get("task"):
        raise SystemExit("production/readiness task mismatch")
    if transfer_manifest.get("format") != TRANSFER_OOF_FORMAT:
        raise SystemExit("transfer OOF manifest format mismatch")
    if transfer_manifest.get("task") != readiness.get("task"):
        raise SystemExit("transfer/readiness task mismatch")

    production_fingerprint = production.get("dataset_fingerprint_sha256")
    transfer_fingerprint = transfer_manifest.get("dataset_fingerprint_sha256")
    if not isinstance(production_fingerprint, str) or production_fingerprint != transfer_fingerprint:
        raise SystemExit("classifier and transfer OOF dataset fingerprints differ")
    if production.get("functions") != transfer_manifest.get("functions"):
        raise SystemExit("classifier and transfer OOF function counts differ")
    if production.get("families") != transfer_manifest.get("families"):
        raise SystemExit("classifier and transfer OOF family counts differ")

    classifier_rows = read_jsonl(args.classifier_oof_jsonl)
    transfer_rows = read_jsonl(args.transfer_oof_jsonl)
    classifier = index_unique(
        classifier_rows,
        path=args.classifier_oof_jsonl,
        required=("id", "family_id", "truth", "prediction", "action"),
    )
    transfer = index_unique(
        transfer_rows,
        path=args.transfer_oof_jsonl,
        required=("id", "family_id", "truth", "prediction", "correct", "similarity", "gap"),
    )
    if set(classifier) != set(transfer):
        missing_classifier = sorted(set(transfer) - set(classifier))[:10]
        missing_transfer = sorted(set(classifier) - set(transfer))[:10]
        raise SystemExit(
            f"classifier/transfer OOF identity mismatch: "
            f"missing_classifier={missing_classifier} missing_transfer={missing_transfer}"
        )
    if len(classifier) != production.get("functions"):
        raise SystemExit("OOF row count does not match production manifest")

    trusted_classes = set(readiness.get("trusted_transfer_classes") or [])
    if not trusted_classes:
        raise SystemExit("readiness trusted transfer class set is empty")
    eligible_labels = {key.rsplit("/", 1)[0] for key in eligible}
    if not eligible_labels <= trusted_classes:
        raise SystemExit("eligible keys contain a label outside trusted transfer classes")

    joined: list[dict[str, Any]] = []
    for identity in sorted(classifier):
        c = classifier[identity]
        t = transfer[identity]
        if c.get("family_id") != t.get("family_id") or c.get("truth") != t.get("truth"):
            raise SystemExit(f"classifier/transfer OOF provenance mismatch: {identity}")
        action = c.get("action")
        if action not in {"accept", "review", "escalate"}:
            raise SystemExit(f"invalid classifier action for {identity}: {action!r}")
        joined.append({
            "id": identity,
            "family_id": c["family_id"],
            "truth": c["truth"],
            "classifier_prediction": c["prediction"],
            "classifier_action": action,
            "transfer_prediction": t["prediction"],
            "transfer_correct": bool(t["correct"]),
            "transfer_similarity": float(t["similarity"]),
            "transfer_gap": float(t["gap"]),
        })

    transfer_policies = select_policy(
        joined,
        prediction_field="transfer_prediction",
        correct_field="transfer_correct",
        similarity_field="transfer_similarity",
        gap_field="transfer_gap",
        target_precision=args.transfer_precision,
        min_examples=args.min_transfer_examples,
    )
    for row in joined:
        label = str(row["transfer_prediction"])
        policy = transfer_policies.get(label, {})
        row["transfer_trusted"] = label in trusted_classes and policy_accepts(row, policy)

    conditional: dict[str, dict[str, Any]] = {}
    for key in sorted(eligible):
        policy = select_conditional_policy(
            joined,
            key=key,
            target_precision=args.conditional_precision,
            min_examples=args.min_conditional_examples,
        )
        if not policy.get("enabled"):
            raise SystemExit(
                f"readiness-eligible policy could not be frozen on full development OOF: {key}"
            )
        conditional[key] = policy

    promoted: list[dict[str, Any]] = []
    for row in joined:
        if row["classifier_action"] == "accept" or not row["transfer_trusted"]:
            continue
        key = f"{row['transfer_prediction']}/{row['classifier_action']}"
        policy = conditional.get(key)
        if policy is not None and policy_accepts(row, policy):
            promoted.append(row)

    base_accept = [row for row in joined if row["classifier_action"] == "accept"]
    base_correct = sum(row["classifier_prediction"] == row["truth"] for row in base_accept)
    promoted_correct = sum(bool(row["transfer_correct"]) for row in promoted)
    fused_examples = len(base_accept) + len(promoted)
    fused_correct = base_correct + promoted_correct
    functions = len(joined)

    candidate = {
        "format": FORMAT,
        "task": readiness["task"],
        "scope": "per_label_action",
        "development_dataset_fingerprint_sha256": production_fingerprint,
        "readiness_sha256": canonical_sha256(readiness),
        "production_manifest_sha256": canonical_sha256(production),
        "transfer_oof_manifest_sha256": canonical_sha256(transfer_manifest),
        "trusted_transfer_classes": sorted(trusted_classes),
        "eligible_policy_keys": sorted(eligible),
        "transfer_policy": {
            "target_precision": args.transfer_precision,
            "min_examples": args.min_transfer_examples,
            "threshold_grid": {
                "similarity": list(SIM_THRESHOLDS),
                "gap": list(GAP_THRESHOLDS),
            },
            "class_policies": {
                label: policy
                for label, policy in sorted(transfer_policies.items())
                if label in trusted_classes
            },
        },
        "conditional_policy": {
            "target_precision": args.conditional_precision,
            "min_examples": args.min_conditional_examples,
            "policies": conditional,
        },
        "development_oof": {
            "functions": functions,
            "base_accept_examples": len(base_accept),
            "base_accept_precision": base_correct / len(base_accept) if base_accept else 0.0,
            "promoted_examples": len(promoted),
            "promoted_precision": promoted_correct / len(promoted) if promoted else 0.0,
            "fused_accept_examples": fused_examples,
            "fused_accept_coverage": fused_examples / functions if functions else 0.0,
            "fused_accept_precision": fused_correct / fused_examples if fused_examples else 0.0,
        },
        "validation_state": "pending_independent_validation",
        "production_routing_changed": False,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(candidate, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("format:", FORMAT)
    print("eligible policy keys:", candidate["eligible_policy_keys"])
    print("frozen conditional policies:", conditional)
    print("development OOF:", candidate["development_oof"])
    print("validation state: pending_independent_validation")
    print("production routing changed: no")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
