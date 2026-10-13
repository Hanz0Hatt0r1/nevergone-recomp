#!/usr/bin/env python3
from __future__ import annotations

import json
import tempfile
from pathlib import Path

from attach_nevergone_similarity_transfer import attach
from query_nevergone_similarity_transfer import trusted_by_policy
from train_nevergone_similarity_transfer import apply_allowlist


def main() -> int:
    learned = {
        "engine": {
            "enabled": True,
            "examples": 20,
            "precision": 1.0,
            "similarity_threshold": 0.9,
            "gap_threshold": 0.0,
        },
        "media_library": {
            "enabled": True,
            "examples": 100,
            "precision": 0.97,
            "similarity_threshold": 0.7,
            "gap_threshold": 0.05,
        },
        "runtime_library": {
            "enabled": True,
            "examples": 120,
            "precision": 0.96,
            "similarity_threshold": 0.8,
            "gap_threshold": 0.0,
        },
    }
    policies = apply_allowlist(learned, {"media_library", "runtime_library"})
    assert not policies["engine"]["trusted_evidence_enabled"]
    assert policies["media_library"]["trusted_evidence_enabled"]
    assert policies["runtime_library"]["trusted_evidence_enabled"]

    trusted, _ = trusted_by_policy("media_library", 0.75, 0.06, policies)
    assert trusted
    trusted, _ = trusted_by_policy("media_library", 0.75, 0.01, policies)
    assert not trusted
    trusted, _ = trusted_by_policy("engine", 0.99, 0.5, policies)
    assert not trusted

    with tempfile.TemporaryDirectory(prefix="nevergone-transfer-prod-") as tmp:
        root = Path(tmp)
        hierarchy = root / "hierarchy.jsonl"
        evidence = root / "evidence.jsonl"
        output = root / "out.jsonl"
        hierarchy.write_text(
            json.dumps({"id": "f1", "final_action": "review", "code_domain": {"prediction": "engine"}}) + "\n",
            encoding="utf-8",
        )
        evidence.write_text(
            json.dumps({
                "id": "f1",
                "task": "code_domain",
                "prediction": "media_library",
                "trusted": True,
                "trusted_label": "media_library",
                "similarity": 0.9,
                "gap": 0.1,
                "semantic_novelty": 0.1,
                "nearest_family": "fam-a",
                "second_family": "fam-b",
                "second_label": "engine",
                "policy": {},
                "routing_effect": "none",
            }) + "\n",
            encoding="utf-8",
        )
        report = attach(hierarchy, evidence, output)
        assert report["matched"] == 1
        assert report["trusted"] == 1
        row = json.loads(output.read_text(encoding="utf-8"))
        assert row["final_action"] == "review"
        assert row["similarity_transfer"]["trusted_label"] == "media_library"

    print("nevergone similarity transfer production smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
