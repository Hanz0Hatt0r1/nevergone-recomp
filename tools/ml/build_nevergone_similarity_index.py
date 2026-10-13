#!/usr/bin/env python3
"""Build a semantic family-retrieval index from portable Nevergone P-code.

Similarity input is portable P-code only. Recovered names/addresses from trace.jsonl
are optional display metadata and never enter TF-IDF or family centroids.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable

from train_nevergone_production_classifier import normalize_portable

FORMAT = "NEVERGONE.SemanticSimilarityIndex/1"


def require_dependencies():
    try:
        import joblib
        from scipy import sparse
        from sklearn.feature_extraction.text import TfidfVectorizer
        from sklearn.preprocessing import normalize
    except ImportError as exc:
        raise SystemExit(
            "Similarity indexing requires scipy, scikit-learn and joblib. Install with: "
            "python -m pip install -U scipy scikit-learn joblib"
        ) from exc
    return {
        "joblib": joblib,
        "sparse": sparse,
        "TfidfVectorizer": TfidfVectorizer,
        "normalize": normalize,
    }


def read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def load_dataset(dataset_dir: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    seen: set[str] = set()
    for split in ("train", "validation", "test"):
        path = dataset_dir / f"{split}.jsonl"
        for row in read_jsonl(path):
            record_id = row.get("id")
            text = row.get("text")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError(f"{path}: row missing id")
            if record_id in seen:
                raise ValueError(f"duplicate dataset id: {record_id}")
            if not isinstance(text, str) or not text:
                raise ValueError(f"dataset row {record_id} missing text")
            labels = row.get("labels")
            rows.append({
                "id": record_id,
                "text": normalize_portable(text),
                "labels": labels if isinstance(labels, dict) else {},
                "split": split,
            })
            seen.add(record_id)
    if not rows:
        raise ValueError("dataset is empty")
    return rows


def load_family_map(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for row in read_jsonl(path):
        record_id = row.get("id")
        family_id = row.get("family_id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("family row missing id")
        if not isinstance(family_id, str) or not family_id:
            raise ValueError(f"family row {record_id} missing family_id")
        previous = result.get(record_id)
        if previous is not None and previous != family_id:
            raise ValueError(f"conflicting family id for {record_id}")
        result[record_id] = family_id
    return result


def load_trace_metadata(path: Path) -> dict[str, dict[str, Any]]:
    if not path.exists():
        return {}
    result: dict[str, dict[str, Any]] = {}
    for row in read_jsonl(path):
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            continue
        result[record_id] = {
            key: row[key]
            for key in ("program", "source_entry", "source_name")
            if isinstance(row.get(key), str) and row.get(key)
        }
    return result


def summarize_labels(members: list[dict[str, Any]]) -> dict[str, Any]:
    by_task: dict[str, Counter[str]] = defaultdict(Counter)
    for row in members:
        for task, label in row["labels"].items():
            if isinstance(task, str) and isinstance(label, str):
                by_task[task][label] += 1
    result: dict[str, Any] = {}
    for task, counts in sorted(by_task.items()):
        ordered = sorted(counts.items(), key=lambda item: (-item[1], item[0]))
        result[task] = {
            "dominant": ordered[0][0],
            "dominant_count": ordered[0][1],
            "distribution": dict(ordered),
            "mixed": len(ordered) > 1,
        }
    return result


def build_index(
    dataset_dir: Path,
    families_path: Path,
    output_dir: Path,
    *,
    max_examples: int = 3,
) -> dict[str, Any]:
    deps = require_dependencies()
    rows = load_dataset(dataset_dir)
    family_map = load_family_map(families_path)
    trace = load_trace_metadata(dataset_dir / "trace.jsonl")

    missing = [row["id"] for row in rows if row["id"] not in family_map]
    if missing:
        raise ValueError(f"dataset IDs missing from family map: {missing[:5]}")

    vectorizer = deps["TfidfVectorizer"](
        tokenizer=str.split,
        token_pattern=None,
        lowercase=False,
        ngram_range=(1, 2),
        min_df=2,
        max_features=80000,
        sublinear_tf=True,
    )
    matrix = vectorizer.fit_transform([row["text"] for row in rows])

    family_members: dict[str, list[int]] = defaultdict(list)
    for index, row in enumerate(rows):
        family_members[family_map[row["id"]]].append(index)

    family_ids = sorted(family_members)
    centroid_rows = []
    metadata: list[dict[str, Any]] = []
    for family_id in family_ids:
        indices = family_members[family_id]
        centroid = deps["sparse"].csr_matrix(matrix[indices].mean(axis=0))
        centroid = deps["normalize"](centroid, norm="l2", axis=1, copy=False)
        centroid_rows.append(centroid)

        members = [rows[index] for index in indices]
        examples: list[dict[str, Any]] = []
        for member in sorted(members, key=lambda row: row["id"])[:max_examples]:
            example = {"id": member["id"]}
            example.update(trace.get(member["id"], {}))
            examples.append(example)
        metadata.append({
            "family_id": family_id,
            "family_size": len(indices),
            "labels": summarize_labels(members),
            "examples": examples,
        })

    prototypes = deps["sparse"].vstack(centroid_rows, format="csr")
    output_dir.mkdir(parents=True, exist_ok=True)
    artifact_path = output_dir / "index.joblib"
    deps["joblib"].dump({
        "format": FORMAT,
        "vectorizer": vectorizer,
        "prototypes": prototypes,
        "families": metadata,
    }, artifact_path)

    manifest = {
        "format": FORMAT,
        "functions": len(rows),
        "families": len(family_ids),
        "features": int(matrix.shape[1]),
        "representation": "portable_pcode",
        "similarity": "cosine_to_l2_normalized_family_tfidf_centroid",
        "vectorizer": {
            "ngram_range": [1, 2],
            "min_df": 2,
            "max_features": 80000,
            "sublinear_tf": True,
        },
        "metadata_policy": {
            "function_names_used_for_similarity": False,
            "addresses_used_for_similarity": False,
            "project_goal_used": False,
            "trace_metadata_display_only": True,
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
    parser.add_argument("--max-examples", type=int, default=3)
    args = parser.parse_args()
    if args.max_examples < 0:
        raise SystemExit("--max-examples must be >= 0")
    report = build_index(
        args.dataset_dir,
        args.families,
        args.output_dir,
        max_examples=args.max_examples,
    )
    print("format:", report["format"])
    print("functions:", report["functions"])
    print("families:", report["families"])
    print("features:", report["features"])
    print("artifact:", report["artifact"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
