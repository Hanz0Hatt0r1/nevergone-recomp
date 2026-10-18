#!/usr/bin/env python3
"""Record whether current Nevergone artifacts can support an independent router gate."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.RouterGateAvailability/1"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected JSON object")
    return value


def load_rows(path: Path) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    seen: set[str] = set()
    with path.open("r", encoding="utf-8") as handle:
        for line_number, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_number}: expected JSON object")
            record_id = row.get("id")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError(f"{path}:{line_number}: missing id")
            if record_id in seen:
                raise ValueError(f"{path}:{line_number}: duplicate id")
            seen.add(record_id)
            result.append(row)
    return result


def build_report(ml_root: Path) -> dict[str, Any]:
    base = ml_root / "models/production-code-domain-v5.2"
    dataset = ml_root / "dataset-4class"
    router = ml_root / "models/accept-error-router-v10"
    frozen = ml_root / "frozen-validation/unseen500-v10"
    manual = ml_root / "dataset-manual-restoration-v70"

    oof_path = base / "oof_predictions.jsonl"
    classifier_manifest_path = base / "manifest.json"
    dataset_manifest_path = dataset / "manifest.json"
    split_paths = {name: dataset / f"{name}.jsonl" for name in ("train", "validation", "test")}
    router_manifest_path = router / "manifest.json"
    router_artifact_path = router / "accept_error_router.joblib"
    frozen_manifest_path = frozen / "manifest.json"
    frozen_summary_path = frozen / "validation-summary.json"
    manual_manifest_path = manual / "manifest.json"
    manual_split_paths = {name: manual / f"{name}.jsonl" for name in ("train", "validation", "test")}
    required_paths = [
        oof_path, classifier_manifest_path, dataset_manifest_path, router_manifest_path,
        router_artifact_path, frozen_manifest_path, frozen_summary_path, manual_manifest_path,
        *split_paths.values(), *manual_split_paths.values(),
    ]
    missing = [str(path.name) for path in required_paths if not path.is_file()]
    if missing:
        raise FileNotFoundError(f"missing required Nevergone router-gate inputs: {', '.join(missing)}")

    oof_rows = load_rows(oof_path)
    oof_ids = {row["id"] for row in oof_rows}
    test_rows = load_rows(split_paths["test"])
    test_ids = {row["id"] for row in test_rows}
    test_overlap = len(oof_ids & test_ids)

    manual_rows: list[dict[str, Any]] = []
    manual_hashes: dict[str, str] = {}
    for split, path in manual_split_paths.items():
        manual_rows.extend(load_rows(path))
        manual_hashes[split] = sha256_file(path)
    manual_by_id = {row["id"]: row for row in manual_rows}
    manual_new = [row for record_id, row in manual_by_id.items() if record_id not in oof_ids]
    new_code_domain_labels = sum(
        isinstance(row.get("labels"), dict) and "code_domain" in row["labels"]
        for row in manual_new
    )

    router_manifest = load_json(router_manifest_path)
    calibration = router_manifest.get("calibration")
    if not isinstance(calibration, dict):
        raise ValueError("v10 router manifest has no calibration object")
    frozen_manifest = load_json(frozen_manifest_path)
    frozen_summary = load_json(frozen_summary_path)
    if frozen_summary.get("labels_available") is not False:
        raise ValueError("unseen500-v10 is no longer the expected unlabeled diagnostic population")

    hashes = {
        "classifier_manifest": sha256_file(classifier_manifest_path),
        "classifier_oof": sha256_file(oof_path),
        "dataset_manifest": sha256_file(dataset_manifest_path),
        "dataset_train": sha256_file(split_paths["train"]),
        "dataset_validation": sha256_file(split_paths["validation"]),
        "dataset_test": sha256_file(split_paths["test"]),
        "router_manifest": sha256_file(router_manifest_path),
        "router_artifact": sha256_file(router_artifact_path),
        "frozen_manifest": sha256_file(frozen_manifest_path),
        "frozen_validation_summary": sha256_file(frozen_summary_path),
        "manual_v70_manifest": sha256_file(manual_manifest_path),
        **{f"manual_v70_{split}": digest for split, digest in manual_hashes.items()},
    }
    return {
        "format": FORMAT,
        "status": "not_evaluated",
        "gate_contract": {
            "minimum_accepted_precision": 0.97,
            "minimum_coverage": 0.55,
            "maximum_critical_false_accept_rate_exclusive": 0.01,
            "evidence_conflicts_must_never_be_accepted": True,
            "independent_labeled_population_required": True,
        },
        "generic_gate_evaluator": {
            "repository": "Hanz0Hatt0r1/revenginstr",
            "revision": "0d05223391729fcb2dec2d7187a371d0ba84d690",
            "consumer_adapter_required": True,
        },
        "current_crossfit_candidate": {
            "router_threshold": router_manifest.get("threshold"),
            "method": calibration.get("method"),
            "training_feedback": calibration.get("training_feedback"),
            "accepted_precision": calibration.get("kept_precision"),
            "coverage": calibration.get("kept_coverage"),
            "accepted_examples": calibration.get("kept_examples"),
            "population_rows": len(oof_rows),
            "interpretation": "crossfit calibration only; not an independent frozen gate",
        },
        "available_holdouts": {
            "nominal_test_split": {
                "rows": len(test_rows),
                "overlap_with_classifier_oof_rows": test_overlap,
                "independent_from_router_calibration": test_overlap == 0,
            },
            "unseen500_v10": {
                "tag": frozen_manifest.get("tag"),
                "rows": frozen_summary.get("rows"),
                "labels_available": frozen_summary.get("labels_available"),
                "frozen_corpus_sha256": frozen_manifest.get("frozen_corpus", {}).get("sha256"),
                "routed_action_counts": frozen_summary.get("routed_action_counts"),
                "router_changed": frozen_summary.get("router_changed"),
                "interpretation": "identity-disjoint deployment diagnostic; semantic precision is unavailable",
            },
            "manual_restoration_v70_new_identities": {
                "total_unique_rows": len(manual_by_id),
                "new_identities_vs_classifier_oof": len(manual_new),
                "new_code_domain_labeled_rows": new_code_domain_labels,
                "interpretation": "too few newly labeled domain examples to substantiate the router gate",
            },
        },
        "gate_metrics": {
            "evaluated": False,
            "accepted_precision": None,
            "coverage": None,
            "critical_false_accept_rate": None,
            "evidence_conflict_accepts": None,
        },
        "reasons": [
            "The v10 threshold's precision and coverage come from crossfit calibration, not a separate frozen gate.",
            "All rows in the nominal test split occur in the v5.2 OOF population used by router calibration.",
            "The identity-disjoint unseen500-v10 population has no semantic labels.",
            "The v70 label refresh adds only one new code_domain-labeled identity beyond the router OOF population.",
        ],
        "source_sha256": hashes,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ml_root", type=Path, nargs="?", default=Path.home() / ".local/share/nevergone-ml-v2")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = build_report(args.ml_root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("format:", FORMAT)
    print("status:", report["status"])
    print("crossfit accepted precision:", report["current_crossfit_candidate"]["accepted_precision"])
    print("crossfit coverage:", report["current_crossfit_candidate"]["coverage"])
    print("unseen500 labels available:", report["available_holdouts"]["unseen500_v10"]["labels_available"])
    print("gate evaluated:", report["gate_metrics"]["evaluated"])
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
