#!/usr/bin/env python3
"""Retrieve nearest known Nevergone function families for portable P-code queries."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Iterable

from train_nevergone_production_classifier import normalize_portable

FORMAT = "NEVERGONE.SemanticSimilaritySearch/1"
INDEX_FORMAT = "NEVERGONE.SemanticSimilarityIndex/1"


def read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def query_id(row: dict[str, Any], index: int) -> str:
    record_id = row.get("id")
    if isinstance(record_id, str) and record_id:
        return record_id
    program = row.get("program")
    entry = row.get("source_entry")
    if isinstance(program, str) and isinstance(entry, str):
        return f"{program}:{entry}"
    return f"row-{index + 1}"


def extract_query_text(row: dict[str, Any]) -> str:
    for key in ("text", "model_text"):
        value = row.get(key)
        if isinstance(value, str) and value:
            return normalize_portable(value)
    raise ValueError("query row has neither text nor model_text portable P-code")


def rank_scores(
    scores: list[float],
    families: list[dict[str, Any]],
    *,
    top_k: int,
) -> list[dict[str, Any]]:
    if len(scores) != len(families):
        raise ValueError("score/family length mismatch")
    if top_k <= 0:
        raise ValueError("top_k must be positive")
    ranked = sorted(
        range(len(scores)),
        key=lambda index: (-float(scores[index]), str(families[index].get("family_id", ""))),
    )[: min(top_k, len(scores))]
    result: list[dict[str, Any]] = []
    for rank, index in enumerate(ranked, 1):
        family = families[index]
        result.append({
            "rank": rank,
            "family_id": family.get("family_id"),
            "similarity": float(scores[index]),
            "family_size": family.get("family_size"),
            "labels": family.get("labels", {}),
            "examples": family.get("examples", []),
        })
    return result


def require_runtime():
    try:
        import joblib
        import numpy as np
    except ImportError as exc:
        raise SystemExit(
            "Similarity search requires numpy, scikit-learn and joblib. Install with: "
            "python -m pip install -U numpy scikit-learn joblib"
        ) from exc
    return joblib, np


def search(index_dir: Path, input_path: Path, output_path: Path, *, top_k: int) -> dict[str, Any]:
    joblib, np = require_runtime()
    artifact_path = index_dir / "index.joblib"
    artifact = joblib.load(artifact_path)
    if artifact.get("format") != INDEX_FORMAT:
        raise ValueError(f"unsupported similarity index format: {artifact.get('format')!r}")
    vectorizer = artifact.get("vectorizer")
    prototypes = artifact.get("prototypes")
    families = artifact.get("families")
    if vectorizer is None or prototypes is None or not isinstance(families, list):
        raise ValueError("invalid similarity index artifact")
    if len(families) != int(prototypes.shape[0]):
        raise ValueError("prototype/family metadata mismatch")

    rows = list(read_jsonl(input_path))
    texts = [extract_query_text(row) for row in rows]
    matrix = vectorizer.transform(texts)
    similarities = matrix @ prototypes.T

    output_path.parent.mkdir(parents=True, exist_ok=True)
    top_similarities: list[float] = []
    top_gaps: list[float] = []
    with output_path.open("w", encoding="utf-8") as handle:
        for index, row in enumerate(rows):
            dense = np.asarray(similarities.getrow(index).toarray()).ravel().astype(float)
            neighbors = rank_scores(dense.tolist(), families, top_k=top_k)
            top_similarity = float(neighbors[0]["similarity"]) if neighbors else 0.0
            second_similarity = float(neighbors[1]["similarity"]) if len(neighbors) > 1 else 0.0
            top_gap = top_similarity - second_similarity
            novelty = max(0.0, min(1.0, 1.0 - top_similarity))
            top_similarities.append(top_similarity)
            top_gaps.append(top_gap)

            result: dict[str, Any] = {
                "format": FORMAT,
                "id": query_id(row, index),
                "top_similarity": top_similarity,
                "top_gap": top_gap,
                "semantic_novelty": novelty,
                "neighbors": neighbors,
            }
            for key in ("program", "source_entry"):
                value = row.get(key)
                if isinstance(value, str) and value:
                    result[key] = value
            handle.write(json.dumps(result, sort_keys=True) + "\n")

    return {
        "functions": len(rows),
        "families": len(families),
        "top_k": top_k,
        "similarity_min": min(top_similarities, default=0.0),
        "similarity_avg": (sum(top_similarities) / len(top_similarities)) if top_similarities else 0.0,
        "similarity_max": max(top_similarities, default=0.0),
        "gap_avg": (sum(top_gaps) / len(top_gaps)) if top_gaps else 0.0,
        "output": str(output_path),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("index_dir", type=Path)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--top-k", type=int, default=5)
    args = parser.parse_args()
    if args.top_k <= 0:
        raise SystemExit("--top-k must be positive")
    report = search(args.index_dir, args.input, args.output, top_k=args.top_k)
    print("format:", FORMAT)
    print("functions:", report["functions"])
    print("indexed families:", report["families"])
    print("top_k:", report["top_k"])
    print(
        "top similarity:",
        f"min={report['similarity_min']:.4f}",
        f"avg={report['similarity_avg']:.4f}",
        f"max={report['similarity_max']:.4f}",
    )
    print("avg top1-top2 gap:", f"{report['gap_avg']:.4f}")
    print("output:", report["output"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
