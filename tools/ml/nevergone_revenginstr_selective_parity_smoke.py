#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / "tools/ml/audit_nevergone_revenginstr_selective_parity.py"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        ref_oof = root / "ref.jsonl"
        candidate_oof = root / "candidate.jsonl"
        ref_manifest = root / "ref-manifest.json"
        candidate_manifest = root / "candidate-manifest.json"
        report = root / "report.json"
        rows = [
            {
                "id": "f0", "prediction": "engine", "action": "accept",
                "raw_confidence": 0.9, "confidence": 0.95,
                "family_similarity": 0.8, "competing_similarity": 0.4,
                "contrast_margin": 0.4,
            },
            {
                "id": "f1", "prediction": "game", "action": "review",
                "raw_confidence": 0.6, "confidence": 0.7,
                "family_similarity": 0.5, "competing_similarity": 0.45,
                "contrast_margin": 0.05,
            },
        ]
        write_jsonl(ref_oof, rows)
        write_jsonl(candidate_oof, rows)
        manifest = {
            "calibration": {"metrics": {"balanced_accuracy": 0.8, "macro_f1": 0.75}},
            "routing": {"selective_oof": {"accept": {"precision": 0.97, "coverage": 0.6}}},
        }
        ref_manifest.write_text(json.dumps(manifest), encoding="utf-8")
        candidate_manifest.write_text(json.dumps(manifest), encoding="utf-8")
        result = subprocess.run(
            [sys.executable, str(AUDIT), str(ref_oof), str(candidate_oof),
             str(ref_manifest), str(candidate_manifest), "--strict", "--output", str(report)],
            text=True, capture_output=True,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        payload = json.loads(report.read_text(encoding="utf-8"))
        assert payload["exact_parity"] is True
        assert payload["prediction_agreement"] == 1.0
        assert payload["action_agreement"] == 1.0

        broken = [dict(row) for row in rows]
        broken[1]["action"] = "accept"
        write_jsonl(candidate_oof, broken)
        result = subprocess.run(
            [sys.executable, str(AUDIT), str(ref_oof), str(candidate_oof),
             str(ref_manifest), str(candidate_manifest), "--strict"],
            text=True, capture_output=True,
        )
        assert result.returncode == 2
        assert "exact parity: False" in result.stdout

    print("nevergone revenginstr selective parity smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
