#!/usr/bin/env python3
"""Query a trained task-specific nearest-family transfer artifact."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from search_nevergone_function_families import extract_query_text, query_id, read_jsonl
from train_nevergone_similarity_transfer import FORMAT

OUTPUT_FORMAT = "NEVERGONE.SimilarityTransferEvidence/1"


def trusted_by_policy(
    prediction: str,
    similarity: float,
    gap: float,
    policies: dict[str, dict[str, Any]],
) -> tuple[bool, dict[str, Any] | None]:
    policy = policies.get(prediction)
    if not isinstance(policy, dict):
        return False, None
    if not policy.get("trusted_evidence_enabled"):
        return False, policy
    sim_threshold = policy.get("similarity_threshold")
    gap_threshold = policy.get("gap_threshold")
    if sim_threshold is None or gap_threshold is None:
        return False, policy
    trusted = similarity >= float(sim_threshold) and gap >= float(gap_threshold)
    return trusted, policy


def require_runtime():
    try:
        import joblib
        import numpy as np
        from sklearn.preprocessing import normalize
    except ImportError as exc:
        raise SystemExit(
            "Similarity transfer requires numpy, scikit-learn and joblib. Install with: "
            "python -m pip install -U numpy scikit-learn joblib"
        ) from exc
    return joblib, np, normalize


def query(artifact_dir: Path, input_path: Path, output_path: Path) -> dict[str, Any]:
    joblib, np, normalize = require_runtime()
    artifact = joblib.load(artifact_dir / "transfer.joblib")
    if artifact.get("format") != FORMAT:
        raise ValueError(f"unsupported transfer artifact format: {artifact.get('format')!r}")
    vectorizer = artifact.get("vectorizer")
    prototypes = artifact.get("prototypes")
    family_ids = artifact.get("family_ids")
    family_labels = artifact.get("family_labels")
    policies = artifact.get("policies")
    task = artifact.get("task")
    if (
        vectorizer is None
        or prototypes is None
        or not isinstance(family_ids, list)
        or not isinstance(family_labels, list)
        or not isinstance(policies, dict)
        or not isinstance(task, str)
    ):
        raise ValueError("invalid similarity transfer artifact")
    if len(family_ids) != len(family_labels) or len(family_ids) != int(prototypes.shape[0]):
        raise ValueError("transfer prototype metadata mismatch")

    rows = list(read_jsonl(input_path))
    texts = [extract_query_text(row) for row in rows]
    matrix = vectorizer.transform(texts)
    matrix = normalize(matrix, norm="l2", axis=1, copy=False)
    scores = matrix @ prototypes.T

    output_path.parent.mkdir(parents=True, exist_ok=True)
    trusted_count = 0
    predictions: dict[str, int] = {}
    trusted_predictions: dict[str, int] = {}
    with output_path.open("w", encoding="utf-8") as handle:
        for index, row in enumerate(rows):
            dense = np.asarray(scores.getrow(index).toarray()).ravel().astype(float)
            order = np.argsort(-dense, kind="stable")
            first = int(order[0])
            second = int(order[1]) if len(order) > 1 else None
            similarity = float(dense[first])
            second_similarity = float(dense[second]) if second is not None else 0.0
            gap = similarity - second_similarity
            prediction = str(family_labels[first])
            trusted, policy = trusted_by_policy(prediction, similarity, gap, policies)
            predictions[prediction] = predictions.get(prediction, 0) + 1
            if trusted:
                trusted_count += 1
                trusted_predictions[prediction] = trusted_predictions.get(prediction, 0) + 1

            result: dict[str, Any] = {
                "format": OUTPUT_FORMAT,
                "id": query_id(row, index),
                "task": task,
                "prediction": prediction,
                "trusted": trusted,
                "trusted_label": prediction if trusted else None,
                "similarity": similarity,
                "gap": gap,
                "semantic_novelty": max(0.0, min(1.0, 1.0 - similarity)),
                "nearest_family": family_ids[first],
                "second_family": family_ids[second] if second is not None else None,
                "second_label": family_labels[second] if second is not None else None,
                "policy": {
                    "trusted_evidence_enabled": bool(policy.get("trusted_evidence_enabled")) if policy else False,
                    "similarity_threshold": policy.get("similarity_threshold") if policy else None,
                    "gap_threshold": policy.get("gap_threshold") if policy else None,
                    "oof_examples": policy.get("examples") if policy else None,
                    "oof_precision": policy.get("precision") if policy else None,
                },
                "routing_effect": "none",
            }
            for key in ("program", "source_entry"):
                value = row.get(key)
                if isinstance(value, str) and value:
                    result[key] = value
            handle.write(json.dumps(result, sort_keys=True) + "\n")

    return {
        "functions": len(rows),
        "task": task,
        "predictions": predictions,
        "trusted": trusted_count,
        "trusted_coverage": trusted_count / len(rows) if rows else 0.0,
        "trusted_predictions": trusted_predictions,
        "output": str(output_path),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact_dir", type=Path)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = query(args.artifact_dir, args.input, args.output)
    print("format:", OUTPUT_FORMAT)
    print("task:", report["task"])
    print("functions:", report["functions"])
    print("predictions:", report["predictions"])
    print("trusted:", report["trusted"])
    print("trusted coverage:", f"{report['trusted_coverage']:.4f}")
    print("trusted predictions:", report["trusted_predictions"])
    print("output:", report["output"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
