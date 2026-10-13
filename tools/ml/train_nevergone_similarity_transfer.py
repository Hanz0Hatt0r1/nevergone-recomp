#!/usr/bin/env python3
"""Train a task-specific nearest-family transfer artifact.

The production artifact mirrors the validated nested protocol:
- portable P-code only;
- family-grouped OOF policy selection;
- task-specific unanimous family prototypes;
- optional allow-list for classes permitted to emit trusted evidence.

The artifact is evidence-only. It does not change any hierarchy final_action.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

from build_nevergone_similarity_index import require_dependencies
from nevergone_nested_similarity_transfer_cv import (
    _fit_reference,
    _inner_oof,
    select_transfer_policy,
)
from nevergone_similarity_transfer_cv import _load_rows

FORMAT = "NEVERGONE.SimilarityTransfer/1"


def apply_allowlist(
    policies: dict[str, dict[str, Any]],
    allowed_classes: set[str],
) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for label, policy in sorted(policies.items()):
        updated = dict(policy)
        updated["validated_for_trusted_evidence"] = label in allowed_classes
        updated["trusted_evidence_enabled"] = bool(policy.get("enabled")) and label in allowed_classes
        result[label] = updated
    return result


def train(
    dataset_dir: Path,
    families_path: Path,
    output_dir: Path,
    *,
    task: str,
    classes: list[str] | None,
    allowed_classes: set[str],
    folds: int,
    target_precision: float,
    min_policy_examples: int,
    seed: int,
) -> dict[str, Any]:
    deps = require_dependencies()
    try:
        import joblib
        import numpy as np
        from sklearn.model_selection import StratifiedGroupKFold
    except ImportError as exc:
        raise SystemExit("requires numpy, scipy, scikit-learn and joblib") from exc

    rows = _load_rows(
        dataset_dir,
        families_path,
        task=task,
        classes=classes,
    )
    labels = Counter(str(row["label"]) for row in rows)
    families = len({str(row["family_id"]) for row in rows})

    oof = _inner_oof(
        deps,
        np,
        StratifiedGroupKFold,
        rows,
        folds,
        seed,
    )
    learned = select_transfer_policy(
        oof,
        target_precision=target_precision,
        min_examples=min_policy_examples,
    )
    policies = apply_allowlist(learned, allowed_classes)

    reference = _fit_reference(deps, rows)
    vectorizer, prototypes, family_ids, family_labels, omitted_mixed = reference

    output_dir.mkdir(parents=True, exist_ok=True)
    artifact_path = output_dir / "transfer.joblib"
    joblib.dump(
        {
            "format": FORMAT,
            "task": task,
            "classes": sorted(labels),
            "allowed_classes": sorted(allowed_classes),
            "target_precision": target_precision,
            "min_policy_examples": min_policy_examples,
            "folds": folds,
            "seed": seed,
            "policies": policies,
            "vectorizer": vectorizer,
            "prototypes": prototypes,
            "family_ids": family_ids,
            "family_labels": family_labels,
        },
        artifact_path,
    )

    manifest = {
        "format": FORMAT,
        "task": task,
        "functions": len(rows),
        "families": families,
        "classes": dict(sorted(labels.items())),
        "allowed_classes": sorted(allowed_classes),
        "target_precision": target_precision,
        "min_policy_examples": min_policy_examples,
        "folds": folds,
        "policies": policies,
        "prototype_families": len(family_ids),
        "mixed_families_omitted": omitted_mixed,
        "representation": "portable_pcode",
        "routing_effect": "evidence_only",
        "metadata_policy": {
            "function_names_used": False,
            "addresses_used": False,
            "strings_used": False,
            "project_goal_used": False,
        },
        "artifact": str(artifact_path),
    }
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--task", required=True)
    parser.add_argument("--classes", nargs="*")
    parser.add_argument("--allow-trusted-class", action="append", default=[])
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--target-precision", type=float, default=0.95)
    parser.add_argument("--min-policy-examples", type=int, default=10)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()
    if args.folds < 2:
        raise SystemExit("--folds must be >= 2")
    if not (0.0 < args.target_precision <= 1.0):
        raise SystemExit("--target-precision must be in (0,1]")
    if args.min_policy_examples < 1:
        raise SystemExit("--min-policy-examples must be >= 1")

    manifest = train(
        args.dataset_dir,
        args.families,
        args.output_dir,
        task=args.task,
        classes=args.classes or None,
        allowed_classes=set(args.allow_trusted_class),
        folds=args.folds,
        target_precision=args.target_precision,
        min_policy_examples=args.min_policy_examples,
        seed=args.seed,
    )
    print("format:", manifest["format"])
    print("task:", manifest["task"])
    print("functions:", manifest["functions"])
    print("families:", manifest["families"])
    print("classes:", manifest["classes"])
    print("allowed trusted classes:", manifest["allowed_classes"])
    print("prototype families:", manifest["prototype_families"])
    print("mixed families omitted:", manifest["mixed_families_omitted"])
    print("policies:")
    for label, policy in manifest["policies"].items():
        print(" ", label, policy)
    print("artifact:", manifest["artifact"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
