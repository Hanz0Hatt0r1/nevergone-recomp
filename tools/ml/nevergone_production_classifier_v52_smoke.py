#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        base = tmp / "base.jsonl"
        evidence = tmp / "evidence.jsonl"
        oof = tmp / "oof.jsonl"
        errors = tmp / "errors.jsonl"
        summary = tmp / "summary.json"

        write_jsonl(
            base,
            [
                {
                    "format": "NEVERGONE.ProductionEvidence/1",
                    "id": "g1",
                    "features": {
                        "has_receiver_types": 1.0,
                        "has_receiver_inference": 1.0,
                        "receiver.call_count": 4.0,
                        "receiver.present_count": 3.0,
                        "receiver.direct_single_pointer_count": 2.0,
                        "receiver.flow_unique_count": 1.0,
                        "receiver.flow_ambiguous_count": 1.0,
                        "receiver.pointer_depth_sum": 6.0,
                        "receiver.pointer_depth_1_count": 2.0,
                        "receiver.pointer_depth_2_count": 2.0,
                        "receiver.storage_unique_count": 3.0,
                        "receiver.storage_register_count": 1.0,
                        "receiver.inference_count": 4.0,
                        "receiver.resolved_count": 3.0,
                        "receiver.status_resolved_unique_vtable_count": 3.0,
                        "receiver.status_unresolved_receiver_class_count": 1.0,
                        "receiver.evidence_direct_receiver_type_count": 2.0,
                        "receiver.evidence_transparent_type_flow_count": 1.0,
                        "receiver.slot_missing_count": 4.0,
                        "receiver.candidate_class_total": 3.0,
                        "receiver.flow_candidate_total": 5.0,
                    },
                },
                {
                    "format": "NEVERGONE.ProductionEvidence/1",
                    "id": "e1",
                    "features": {"callgraph.fan_in": 2.0},
                },
                {
                    "format": "NEVERGONE.ProductionEvidence/1",
                    "id": "g2",
                    "features": {
                        "receiver.call_count": 2.0,
                        "receiver.present_count": 2.0,
                        "receiver.inference_count": 2.0,
                        "receiver.resolved_count": 1.0,
                    },
                },
                {
                    "format": "NEVERGONE.ProductionEvidence/1",
                    "id": "e2",
                    "features": {
                        "receiver.call_count": 2.0,
                        "receiver.present_count": 1.0,
                        "receiver.inference_count": 2.0,
                        "receiver.resolved_count": 2.0,
                    },
                },
            ],
        )

        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/ml/augment_nevergone_v52_receiver_ratios.py"),
                str(base),
                "--output",
                str(evidence),
            ],
            check=True,
        )

        rows = {row["id"]: row for row in map(json.loads, evidence.read_text().splitlines())}
        f = rows["g1"]["features"]
        assert f["receiver.present_ratio"] == 0.75, f
        assert f["receiver.direct_single_pointer_ratio"] == 0.5, f
        assert f["receiver.pointer_depth_mean"] == 1.5, f
        assert f["receiver.pointer_depth_1_ratio"] == 0.5, f
        assert f["receiver.storage_unique_ratio"] == 0.75, f
        assert f["receiver.resolved_ratio"] == 0.75, f
        assert f["receiver.status_resolved_unique_vtable_ratio"] == 0.75, f
        assert f["receiver.evidence_direct_receiver_type_ratio"] == 0.5, f
        assert f["receiver.slot_missing_ratio"] == 1.0, f
        assert f["receiver.candidate_class_mean"] == 0.75, f
        assert f["receiver.flow_candidate_mean"] == 1.25, f
        assert "present_ratio" not in rows["e1"]["features"], rows["e1"]

        write_jsonl(
            oof,
            [
                {
                    "id": "g1", "family_id": "fam-g1", "truth": "game", "prediction": "engine",
                    "correct": False, "confidence": 0.82, "raw_confidence": 0.74, "action": "review",
                    "family_similarity": 0.5, "contrast_margin": 0.1,
                    "probabilities": {"game": 0.26, "engine": 0.74},
                },
                {
                    "id": "g2", "family_id": "fam-g2", "truth": "game", "prediction": "game",
                    "correct": True, "confidence": 0.9, "raw_confidence": 0.8, "action": "accept",
                    "family_similarity": 0.6, "contrast_margin": 0.2,
                    "probabilities": {"game": 0.8, "engine": 0.2},
                },
                {
                    "id": "e1", "family_id": "fam-e1", "truth": "engine", "prediction": "game",
                    "correct": False, "confidence": 0.7, "raw_confidence": 0.6, "action": "escalate",
                    "family_similarity": 0.4, "contrast_margin": 0.0,
                    "probabilities": {"game": 0.6, "engine": 0.4},
                },
                {
                    "id": "e2", "family_id": "fam-e2", "truth": "engine", "prediction": "engine",
                    "correct": True, "confidence": 0.95, "raw_confidence": 0.88, "action": "accept",
                    "family_similarity": 0.7, "contrast_margin": 0.3,
                    "probabilities": {"game": 0.12, "engine": 0.88},
                },
                {
                    "id": "m1", "family_id": "fam-m1", "truth": "media_library", "prediction": "media_library",
                    "correct": True, "confidence": 0.99, "raw_confidence": 0.99, "action": "accept",
                    "family_similarity": 0.8, "contrast_margin": 0.4,
                    "probabilities": {"game": 0.0, "engine": 0.0},
                },
            ],
        )

        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/ml/audit_nevergone_game_engine_boundary.py"),
                str(oof),
                str(evidence),
                "--output",
                str(errors),
                "--summary-output",
                str(summary),
            ],
            check=True,
        )
        payload = json.loads(summary.read_text())
        assert payload["boundary_rows"] == 4, payload
        assert payload["boundary_errors"] == 2, payload
        assert payload["transitions"]["game->engine"]["count"] == 1, payload
        assert payload["transitions"]["engine->game"]["count"] == 1, payload
        assert payload["training_feedback"] is False, payload
        error_rows = [json.loads(line) for line in errors.read_text().splitlines() if line.strip()]
        assert len(error_rows) == 2, error_rows
        serialized = errors.read_text() + summary.read_text()
        assert "GameThing" not in serialized
        assert "EngineThing" not in serialized

    print("ProductionClassifier/5.2 receiver ratio + boundary audit smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
