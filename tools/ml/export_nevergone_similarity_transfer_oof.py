#!/usr/bin/env python3
"""Export full-development family-held-out similarity-transfer predictions.

The output is reusable evidence for freezing a conditional-fusion candidate. It
contains one OOF retrieval result per labelled development function plus a
manifest tied to the same dataset fingerprint used by ProductionClassifier/4.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

from nevergone_nested_similarity_transfer_cv import _inner_oof
from train_nevergone_production_classifier import (
    _load_dataset,
    _require_dependencies,
    _sha256_rows,
)

FORMAT = "NEVERGONE.SimilarityTransferOOF/1"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--task", default="code_domain")
    parser.add_argument("--classes", nargs="+", required=True)
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()
    if args.folds < 2:
        raise SystemExit("--folds must be >= 2")

    deps = _require_dependencies()
    np = deps["np"]
    rows = _load_dataset(
        args.dataset_dir,
        args.families,
        task=args.task,
        classes=args.classes,
    )
    by_id: dict[str, dict[str, Any]] = {}
    for row in rows:
        identity = str(row["id"])
        if identity in by_id:
            raise SystemExit(f"duplicate development id: {identity}")
        by_id[identity] = row

    scored = _inner_oof(
        deps,
        np,
        deps["StratifiedGroupKFold"],
        rows,
        args.folds,
        args.seed,
    )
    if len(scored) != len(rows):
        raise SystemExit(f"OOF row count mismatch: {len(scored)} != {len(rows)}")
    seen: set[str] = set()
    output: list[dict[str, Any]] = []
    for record in scored:
        identity = str(record["id"])
        source = by_id.get(identity)
        if source is None:
            raise SystemExit(f"OOF row not present in dataset: {identity}")
        if identity in seen:
            raise SystemExit(f"duplicate OOF id: {identity}")
        seen.add(identity)
        if str(record["truth"]) != str(source["label"]):
            raise SystemExit(f"OOF truth mismatch: {identity}")
        output.append({
            "id": identity,
            "family_id": source["family_id"],
            "truth": source["label"],
            "prediction": record["prediction"],
            "correct": bool(record["correct"]),
            "similarity": float(record["similarity"]),
            "gap": float(record["gap"]),
            "nearest_family": record["nearest_family"],
        })
    if seen != set(by_id):
        raise SystemExit("OOF export does not cover the complete dataset")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    output_path = args.output_dir / "transfer_oof_predictions.jsonl"
    with output_path.open("w", encoding="utf-8") as handle:
        for row in sorted(output, key=lambda item: str(item["id"])):
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    manifest = {
        "format": FORMAT,
        "task": args.task,
        "classes": args.classes,
        "functions": len(rows),
        "families": len({str(row["family_id"]) for row in rows}),
        "class_distribution": dict(sorted(Counter(str(row["label"]) for row in rows).items())),
        "dataset_fingerprint_sha256": _sha256_rows(rows),
        "folds": args.folds,
        "seed": args.seed,
        "representation": "portable_pcode_tfidf_family_prototype_retrieval",
        "family_grouped_oof": True,
        "production_routing_changed": False,
    }
    manifest_path = args.output_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    accuracy = sum(bool(row["correct"]) for row in output) / len(output) if output else 0.0
    print("format:", FORMAT)
    print("functions:", len(output))
    print("families:", manifest["families"])
    print("raw OOF transfer accuracy:", f"{accuracy:.4f}")
    print("manifest:", manifest_path)
    print("predictions:", output_path)
    print("production routing changed: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
