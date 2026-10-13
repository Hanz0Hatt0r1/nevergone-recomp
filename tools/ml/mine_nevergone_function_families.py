#!/usr/bin/env python3
"""Mine near-duplicate Nevergone function families from an existing ML dataset."""
from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from nevergone_pcode_features import (
    family_feature_set,
    jaccard,
    lsh_band_keys,
    minhash_signature,
    portable_pcode,
)

FORMAT = "NEVERGONE.FunctionFamilies/1"


class DisjointSet:
    def __init__(self, size: int) -> None:
        self.parent = list(range(size))
        self.rank = [0] * size

    def find(self, value: int) -> int:
        parent = self.parent[value]
        if parent != value:
            self.parent[value] = self.find(parent)
        return self.parent[value]

    def union(self, left: int, right: int) -> bool:
        left_root = self.find(left)
        right_root = self.find(right)
        if left_root == right_root:
            return False
        if self.rank[left_root] < self.rank[right_root]:
            left_root, right_root = right_root, left_root
        self.parent[right_root] = left_root
        if self.rank[left_root] == self.rank[right_root]:
            self.rank[left_root] += 1
        return True


def read_dataset(dataset_dir: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    seen_ids: set[str] = set()
    for split in ("train", "validation", "test"):
        path = dataset_dir / f"{split}.jsonl"
        for line_no, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            record_id = row.get("id")
            text = row.get("text")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError(f"{path}:{line_no}: missing id")
            if not isinstance(text, str) or not text:
                raise ValueError(f"{path}:{line_no}: missing text")
            if record_id in seen_ids:
                raise ValueError(f"duplicate dataset id: {record_id}")
            seen_ids.add(record_id)
            rows.append({
                "id": record_id,
                "text": text,
                "labels": row.get("labels", {}),
                "split": split,
            })
    return rows


def _portable_hash(text: str) -> str:
    return hashlib.sha256(portable_pcode(text).encode("utf-8")).hexdigest()


def mine_dataset(
    dataset_dir: Path,
    output_path: Path,
    *,
    similarity_threshold: float = 0.72,
    min_length_ratio: float = 0.55,
    bands: int = 8,
) -> dict[str, Any]:
    if not 0.0 < similarity_threshold <= 1.0:
        raise ValueError("similarity_threshold must be in (0, 1]")
    if not 0.0 < min_length_ratio <= 1.0:
        raise ValueError("min_length_ratio must be in (0, 1]")

    rows = read_dataset(dataset_dir)
    features: list[frozenset[str]] = []
    signatures: list[tuple[int, ...]] = []
    portable_hashes: list[str] = []
    op_counts: list[int] = []

    for row in rows:
        text = row["text"]
        feature_set = family_feature_set(text)
        features.append(feature_set)
        signatures.append(minhash_signature(feature_set))
        portable = portable_pcode(text)
        portable_hashes.append(hashlib.sha256(portable.encode("utf-8")).hexdigest())
        op_counts.append(sum(1 for line in portable.splitlines() if line))

    dsu = DisjointSet(len(rows))
    exact_groups: dict[str, list[int]] = defaultdict(list)
    for index, portable_hash in enumerate(portable_hashes):
        exact_groups[portable_hash].append(index)
    exact_merges = 0
    for members in exact_groups.values():
        for member in members[1:]:
            exact_merges += int(dsu.union(members[0], member))

    buckets: dict[tuple[int, tuple[int, ...]], list[int]] = defaultdict(list)
    for index, signature in enumerate(signatures):
        for band_index, key in enumerate(lsh_band_keys(signature, bands=bands)):
            buckets[(band_index, key)].append(index)

    candidate_pairs: set[tuple[int, int]] = set()
    for members in buckets.values():
        if len(members) < 2:
            continue
        for left_pos in range(len(members) - 1):
            left = members[left_pos]
            for right in members[left_pos + 1:]:
                if left == right:
                    continue
                candidate_pairs.add((left, right) if left < right else (right, left))

    checked = 0
    fuzzy_merges = 0
    accepted_pairs = 0
    for left, right in sorted(candidate_pairs):
        if portable_hashes[left] == portable_hashes[right]:
            continue
        low = min(op_counts[left], op_counts[right])
        high = max(op_counts[left], op_counts[right])
        ratio = (low / high) if high else 1.0
        if ratio < min_length_ratio:
            continue
        checked += 1
        similarity = jaccard(features[left], features[right])
        if similarity < similarity_threshold:
            continue
        accepted_pairs += 1
        fuzzy_merges += int(dsu.union(left, right))

    components: dict[int, list[int]] = defaultdict(list)
    for index in range(len(rows)):
        components[dsu.find(index)].append(index)

    family_by_index: dict[int, str] = {}
    for members in components.values():
        representative = min(portable_hashes[index] for index in members)
        family_id = "family-" + representative[:20]
        for index in members:
            family_by_index[index] = family_id

    family_sizes = Counter(family_by_index.values())
    task_conflicts: dict[str, int] = defaultdict(int)
    task_family_labels: dict[str, dict[str, set[str]]] = defaultdict(lambda: defaultdict(set))
    for index, row in enumerate(rows):
        family_id = family_by_index[index]
        labels = row.get("labels")
        if not isinstance(labels, dict):
            continue
        for task, label in labels.items():
            if isinstance(task, str) and isinstance(label, str):
                task_family_labels[task][family_id].add(label)
    for task, by_family in task_family_labels.items():
        task_conflicts[task] = sum(1 for labels in by_family.values() if len(labels) > 1)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as handle:
        for index, row in sorted(enumerate(rows), key=lambda item: item[1]["id"]):
            result = {
                "format": FORMAT,
                "id": row["id"],
                "family_id": family_by_index[index],
                "family_size": family_sizes[family_by_index[index]],
                "portable_hash": portable_hashes[index][:24],
                "feature_count": len(features[index]),
            }
            handle.write(json.dumps(result, sort_keys=True) + "\n")

    manifest = {
        "format": FORMAT,
        "dataset": str(dataset_dir),
        "functions": len(rows),
        "exact_portable_groups": len(exact_groups),
        "families": len(components),
        "multi_member_families": sum(1 for size in family_sizes.values() if size > 1),
        "largest_family": max(family_sizes.values(), default=0),
        "exact_merges": exact_merges,
        "lsh_candidate_pairs": len(candidate_pairs),
        "similarity_pairs_checked": checked,
        "accepted_similarity_pairs": accepted_pairs,
        "fuzzy_merges": fuzzy_merges,
        "mixed_label_families": dict(sorted(task_conflicts.items())),
        "policy": {
            "similarity": "Jaccard over portable opcode/shape/sequence features",
            "similarity_threshold": similarity_threshold,
            "min_length_ratio": min_length_ratio,
            "minhash_size": len(signatures[0]) if signatures else 32,
            "lsh_bands": bands,
            "function_names_used": False,
            "addresses_used": False,
            "headers_used": False,
        },
    }
    manifest_path = output_path.with_suffix(output_path.suffix + ".manifest.json")
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--threshold", type=float, default=0.72)
    parser.add_argument("--min-length-ratio", type=float, default=0.55)
    parser.add_argument("--bands", type=int, default=8)
    args = parser.parse_args()

    report = mine_dataset(
        args.dataset_dir,
        args.output,
        similarity_threshold=args.threshold,
        min_length_ratio=args.min_length_ratio,
        bands=args.bands,
    )
    print(f"format: {report['format']}")
    print(f"functions: {report['functions']}")
    print(f"families: {report['families']}")
    print(f"multi-member families: {report['multi_member_families']}")
    print(f"largest family: {report['largest_family']}")
    print(f"candidate pairs: {report['lsh_candidate_pairs']}")
    print(f"accepted similarity pairs: {report['accepted_similarity_pairs']}")
    print(f"mixed-label families: {report['mixed_label_families']}")
    print(f"output: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
