#!/usr/bin/env python3
"""Retrieve nearest known Nevergone function families for portable P-code queries."""
from __future__ import annotations

import argparse
import hashlib
import heapq
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


def stable_query_id(row: dict[str, Any]) -> str | None:
    program = row.get("program")
    entry = row.get("source_entry")
    if isinstance(program, str) and program and isinstance(entry, str) and entry:
        digest = hashlib.sha256(f"{program}\0{entry}".encode("utf-8")).hexdigest()
        return digest[:24]
    record_id = row.get("id")
    if isinstance(record_id, str) and record_id:
        return record_id
    return None


def load_family_map(path: Path | None) -> dict[str, str]:
    if path is None:
        return {}
    result: dict[str, str] = {}
    for row in read_jsonl(path):
        record_id = row.get("id")
        family_id = row.get("family_id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError(f"{path}: family row missing id")
        if not isinstance(family_id, str) or not family_id:
            raise ValueError(f"{path}: family row {record_id} missing family_id")
        previous = result.get(record_id)
        if previous is not None and previous != family_id:
            raise ValueError(f"{path}: conflicting family id for {record_id}")
        result[record_id] = family_id
    return result


def has_task_label(family: dict[str, Any], task: str) -> bool:
    labels = family.get("labels")
    if not isinstance(labels, dict):
        return False
    summary = labels.get(task)
    if not isinstance(summary, dict):
        return False
    distribution = summary.get("distribution")
    return isinstance(distribution, dict) and bool(distribution)


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
    exclude_family_id: str | None = None,
) -> list[dict[str, Any]]:
    if len(scores) != len(families):
        raise ValueError("score/family length mismatch")
    if top_k <= 0:
        raise ValueError("top_k must be positive")
    ranked = sorted(
        (
            index for index, family in enumerate(families)
            if family.get("family_id") != exclude_family_id
        ),
        key=lambda index: (-float(scores[index]), str(families[index].get("family_id", ""))),
    )[:top_k]
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


def search(
    index_dir: Path,
    input_path: Path,
    output_path: Path,
    *,
    top_k: int,
    label_task: str | None = None,
    families_path: Path | None = None,
    unlabelled_only: bool = False,
    top_candidates: int | None = None,
) -> dict[str, Any]:
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
    if unlabelled_only and (label_task is None or families_path is None):
        raise ValueError("--unlabelled-only requires --label-task and --families")
    if top_candidates is not None and top_candidates < 1:
        raise ValueError("top_candidates must be positive")

    selected_indices = [
        index for index, family in enumerate(families)
        if label_task is None or has_task_label(family, label_task)
    ]
    if not selected_indices:
        raise ValueError(f"similarity index has no families labelled for {label_task!r}")
    families = [families[index] for index in selected_indices]
    prototypes = prototypes[selected_indices]
    family_by_id = {
        str(family.get("family_id")): family
        for family in families
        if isinstance(family.get("family_id"), str)
    }
    query_family_map = load_family_map(families_path)

    rows = list(read_jsonl(input_path))
    queries: list[tuple[int, dict[str, Any], str | None]] = []
    skipped_known = 0
    skipped_without_pcode = 0
    texts: list[str] = []
    for index, row in enumerate(rows):
        identity = stable_query_id(row)
        family_id = row.get("family_id")
        if not isinstance(family_id, str) or not family_id:
            family_id = query_family_map.get(identity) if identity is not None else None
        if unlabelled_only and family_id is not None:
            family = family_by_id.get(family_id)
            if family is not None and has_task_label(family, label_task or ""):
                skipped_known += 1
                continue
        try:
            text = extract_query_text(row)
        except ValueError:
            skipped_without_pcode += 1
            continue
        queries.append((index, row, family_id if isinstance(family_id, str) else None))
        texts.append(text)
    if not queries:
        raise ValueError("no query functions remain after task filtering")

    matrix = vectorizer.transform(texts)
    similarities = matrix @ prototypes.T

    output_path.parent.mkdir(parents=True, exist_ok=True)
    top_similarities: list[float] = []
    top_gaps: list[float] = []
    ranked_candidates: list[tuple[tuple[float, float, int], dict[str, Any]]] = []
    with output_path.open("w", encoding="utf-8") as handle:
        for index, (source_index, row, family_id) in enumerate(queries):
            dense = np.asarray(similarities.getrow(index).toarray()).ravel().astype(float)
            neighbors = rank_scores(
                dense.tolist(), families, top_k=top_k, exclude_family_id=family_id
            )
            top_similarity = float(neighbors[0]["similarity"]) if neighbors else 0.0
            second_similarity = float(neighbors[1]["similarity"]) if len(neighbors) > 1 else 0.0
            top_gap = top_similarity - second_similarity
            novelty = max(0.0, min(1.0, 1.0 - top_similarity))
            top_similarities.append(top_similarity)
            top_gaps.append(top_gap)

            result: dict[str, Any] = {
                "format": FORMAT,
                "id": query_id(row, source_index),
                "top_similarity": top_similarity,
                "top_gap": top_gap,
                "semantic_novelty": novelty,
                "label_task_filter": label_task,
                "query_family_id": family_id,
                "neighbors": neighbors,
            }
            for key in ("program", "source_entry", "source_name"):
                value = row.get(key)
                if isinstance(value, str) and value:
                    result[key] = value
            if top_candidates is None:
                handle.write(json.dumps(result, sort_keys=True) + "\n")
            else:
                score = (top_similarity, top_gap, -source_index)
                item = (score, result)
                if len(ranked_candidates) < top_candidates:
                    heapq.heappush(ranked_candidates, item)
                elif score > ranked_candidates[0][0]:
                    heapq.heapreplace(ranked_candidates, item)

    if top_candidates is not None:
        ranked_candidates.sort(key=lambda item: (-item[0][0], -item[0][1], -item[0][2]))
        with output_path.open("w", encoding="utf-8") as handle:
            for _score, result in ranked_candidates:
                handle.write(json.dumps(result, sort_keys=True) + "\n")

    return {
        "functions": len(queries),
        "input_functions": len(rows),
        "skipped_known_families": skipped_known,
        "skipped_without_pcode": skipped_without_pcode,
        "indexed_families": len(families),
        "label_task_filter": label_task,
        "families": len(families),
        "top_k": top_k,
        "ranked_candidates": min(len(queries), top_candidates) if top_candidates is not None else len(queries),
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
    parser.add_argument("--label-task", help="retrieve only families carrying this label task")
    parser.add_argument("--families", type=Path, help="family IDs for query functions")
    parser.add_argument(
        "--unlabelled-only",
        action="store_true",
        help="skip queries whose own family already has --label-task labels",
    )
    parser.add_argument(
        "--top-candidates",
        type=int,
        help="write only this many highest-similarity query functions",
    )
    args = parser.parse_args()
    if args.top_k <= 0:
        raise SystemExit("--top-k must be positive")
    if args.top_candidates is not None and args.top_candidates <= 0:
        raise SystemExit("--top-candidates must be positive")
    report = search(
        args.index_dir,
        args.input,
        args.output,
        top_k=args.top_k,
        label_task=args.label_task,
        families_path=args.families,
        unlabelled_only=args.unlabelled_only,
        top_candidates=args.top_candidates,
    )
    print("format:", FORMAT)
    print("functions:", report["functions"])
    print("input functions:", report["input_functions"])
    print("skipped known families:", report["skipped_known_families"])
    print("skipped without portable P-code:", report["skipped_without_pcode"])
    print("indexed families:", report["indexed_families"])
    print("label task:", report["label_task_filter"])
    print("top_k:", report["top_k"])
    print("ranked candidates:", report["ranked_candidates"])
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
