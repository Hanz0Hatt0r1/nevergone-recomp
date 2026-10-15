#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ML = Path(__file__).resolve().parent


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        base = root / "base.jsonl"
        v9 = root / "v9.jsonl"
        manifest = root / "manifest.json"
        train_evidence = root / "train-evidence.jsonl"
        frozen = root / "frozen.jsonl"
        frozen_evidence = root / "frozen-evidence.jsonl"
        output = root / "drift.json"

        base_rows = [
            {
                "id": "a", "prediction": "engine", "confidence": 0.95, "raw_confidence": 0.9,
                "family_similarity": 0.8, "competing_similarity": 0.2, "contrast_margin": 0.6,
                "probabilities": {"engine": 0.9, "game": 0.1},
            },
            {
                "id": "b", "prediction": "engine", "confidence": 0.9, "raw_confidence": 0.85,
                "family_similarity": 0.7, "competing_similarity": 0.3, "contrast_margin": 0.4,
                "probabilities": {"engine": 0.8, "game": 0.2},
            },
            {
                "id": "c", "prediction": "game", "confidence": 0.8, "raw_confidence": 0.75,
                "family_similarity": 0.6, "competing_similarity": 0.4, "contrast_margin": 0.2,
                "probabilities": {"engine": 0.3, "game": 0.7},
            },
            {
                "id": "d", "prediction": "media_library", "confidence": 0.7, "raw_confidence": 0.65,
                "family_similarity": 0.5, "competing_similarity": 0.4, "contrast_margin": 0.1,
                "probabilities": {"media_library": 0.6, "engine": 0.4},
            },
        ]
        write_jsonl(base, base_rows)
        write_jsonl(
            v9,
            [
                {"id": "a", "legacy_action": "accept"},
                {"id": "b", "legacy_action": "accept"},
                {"id": "c", "legacy_action": "review"},
                {"id": "d", "legacy_action": "escalate"},
            ],
        )
        manifest.write_text(
            json.dumps({"calibration": {"legacy_accept_examples": 2, "kept_examples": 1}}),
            encoding="utf-8",
        )
        write_jsonl(
            train_evidence,
            [
                {"id": "a", "features": {"has_callgraph": 1, "has_receiver_types": 1, "x": 1}},
                {"id": "b", "features": {"has_callgraph": 1, "x": 1}},
                {"id": "c", "features": {"has_callgraph": 1, "x": 1}},
                {"id": "d", "features": {"has_callgraph": 1}},
            ],
        )
        write_jsonl(
            frozen,
            [
                {
                    "id": "u1", "prediction": "game", "base_action": "accept", "action": "escalate",
                    "accept_router_changed": True, "confidence": 0.55, "raw_confidence": 0.5,
                    "family_similarity": 0.3, "competing_similarity": 0.5, "contrast_margin": -0.2,
                    "probabilities": {"engine": 0.45, "game": 0.55},
                },
                {
                    "id": "u2", "prediction": "game", "base_action": "review", "action": "review",
                    "accept_router_changed": False, "confidence": 0.5, "raw_confidence": 0.45,
                    "family_similarity": 0.25, "competing_similarity": 0.55, "contrast_margin": -0.3,
                    "probabilities": {"engine": 0.48, "game": 0.52},
                },
            ],
        )
        write_jsonl(
            frozen_evidence,
            [
                {"id": "u1", "features": {"has_callgraph": 1}},
                {"id": "u2", "features": {"has_callgraph": 1}},
            ],
        )

        subprocess.run(
            [
                sys.executable,
                str(ML / "audit_nevergone_frozen_drift_v10.py"),
                str(base), str(v9), str(manifest), str(train_evidence), str(frozen), str(frozen_evidence),
                "--output", str(output),
            ],
            check=True,
        )
        summary = json.loads(output.read_text(encoding="utf-8"))
        assert summary["format"] == "NEVERGONE.FrozenDriftAuditV10/1"
        assert summary["labels_available"] is False
        assert summary["categorical_shift"]["prediction"]["js_divergence_bits"] > 0.0
        assert summary["router_intervention"]["reference_change_fraction_accept"] == 0.5
        assert summary["router_intervention"]["frozen_change_fraction_accept"] == 1.0
        assert summary["router_intervention"]["accept_change_rate_ratio"] == 2.0
        assert summary["evidence_shift"]["core"]["has_callgraph"]["frozen"] == 1.0
        assert summary["evidence_shift"]["core"]["has_receiver_types"]["frozen"] == 0.0
        assert summary["severity"]["level"] in {"moderate", "high"}

    print("nevergone frozen drift v10 smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
