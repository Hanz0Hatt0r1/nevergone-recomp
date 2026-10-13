#!/usr/bin/env python3
"""Attach similarity-transfer evidence to hierarchy JSONL without changing routing."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Iterable

FORMAT = "NEVERGONE.HierarchyWithSimilarityEvidence/1"


def read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def attach(hierarchy_path: Path, evidence_path: Path, output_path: Path) -> dict[str, Any]:
    evidence_rows = list(read_jsonl(evidence_path))
    evidence_by_id: dict[str, dict[str, Any]] = {}
    for row in evidence_rows:
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("evidence row missing id")
        if record_id in evidence_by_id:
            raise ValueError(f"duplicate evidence id: {record_id}")
        evidence_by_id[record_id] = row

    hierarchy_rows = list(read_jsonl(hierarchy_path))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    matched = 0
    trusted = 0
    missing: list[str] = []
    with output_path.open("w", encoding="utf-8") as handle:
        for row in hierarchy_rows:
            record_id = row.get("id")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError("hierarchy row missing id")
            evidence = evidence_by_id.get(record_id)
            result = dict(row)
            original_final_action = result.get("final_action")
            if evidence is None:
                result["similarity_transfer"] = None
                missing.append(record_id)
            else:
                matched += 1
                if bool(evidence.get("trusted")):
                    trusted += 1
                result["similarity_transfer"] = {
                    key: evidence.get(key)
                    for key in (
                        "task",
                        "prediction",
                        "trusted",
                        "trusted_label",
                        "similarity",
                        "gap",
                        "semantic_novelty",
                        "nearest_family",
                        "second_family",
                        "second_label",
                        "policy",
                        "routing_effect",
                    )
                }
            if result.get("final_action") != original_final_action:
                raise AssertionError("similarity evidence changed final_action")
            result["format_with_similarity"] = FORMAT
            handle.write(json.dumps(result, sort_keys=True) + "\n")

    return {
        "functions": len(hierarchy_rows),
        "matched": matched,
        "missing": len(missing),
        "trusted": trusted,
        "trusted_coverage": trusted / len(hierarchy_rows) if hierarchy_rows else 0.0,
        "missing_ids": missing[:20],
        "output": str(output_path),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("hierarchy", type=Path)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = attach(args.hierarchy, args.evidence, args.output)
    print("format:", FORMAT)
    print("functions:", report["functions"])
    print("matched:", report["matched"])
    print("missing:", report["missing"])
    print("trusted evidence:", report["trusted"])
    print("trusted coverage:", f"{report['trusted_coverage']:.4f}")
    if report["missing_ids"]:
        print("missing ids sample:", report["missing_ids"])
    print("output:", report["output"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
