#!/usr/bin/env python3
"""Synthetic smoke for v10 accept-router packaging."""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_jsonl(path: Path, rows) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    script = root / "tools/ml/train_nevergone_accept_error_router_v10.py"
    with tempfile.TemporaryDirectory(prefix="nevergone-v10-smoke-") as tmp:
        tmpdir = Path(tmp)
        base_path = tmpdir / "base.jsonl"
        v8_path = tmpdir / "v8.jsonl"
        evidence_path = tmpdir / "evidence.jsonl"
        output_dir = tmpdir / "router"

        base_rows = []
        v8_rows = []
        evidence_rows = []
        labels = ["engine", "media_library", "runtime_library", "game"]
        for i in range(200):
            fold = i % 5 + 1
            prediction = labels[i % len(labels)]
            is_error = i % 11 == 0 or (prediction == "media_library" and i % 13 == 0)
            truth = labels[(i + 1) % len(labels)] if is_error else prediction
            base_prob = 0.58 if is_error else 0.94
            remainder = (1.0 - base_prob) / 3.0
            probabilities = {label: remainder for label in labels}
            probabilities[prediction] = base_prob
            record_id = f"synthetic:{i:04d}"
            base_rows.append(
                {
                    "id": record_id,
                    "family_id": f"family-{i % 40}",
                    "truth": truth,
                    "prediction": prediction,
                    "correct": not is_error,
                    "probabilities": probabilities,
                    "family_similarity": 0.30 if is_error else 0.92,
                    "competing_similarity": 0.70 if is_error else 0.15,
                    "contrast_margin": -0.40 if is_error else 0.77,
                }
            )
            v8_rows.append(
                {
                    "id": record_id,
                    "truth": truth,
                    "prediction": prediction,
                    "fold": fold,
                    "legacy_action": "accept" if i < 180 else "review",
                }
            )
            evidence_rows.append(
                {
                    "format": "NEVERGONE.ProductionEvidence/1",
                    "id": record_id,
                    "features": {
                        "receiver.call_count": 0.0 if is_error else 4.0,
                        "callgraph.out_degree": 1.0 if is_error else 7.0,
                    },
                }
            )

        write_jsonl(base_path, base_rows)
        write_jsonl(v8_path, v8_rows)
        write_jsonl(evidence_path, evidence_rows)
        subprocess.run(
            [
                sys.executable,
                str(script),
                str(base_path),
                str(v8_path),
                str(output_dir),
                "--evidence",
                str(evidence_path),
                "--target-accept-precision",
                "0.95",
                "--max-accept-coverage-loss",
                "0.30",
                "--min-class-meta-examples",
                "8",
                "--min-accept-examples",
                "20",
            ],
            check=True,
        )
        manifest = json.loads((output_dir / "manifest.json").read_text(encoding="utf-8"))
        assert manifest["format"] == "NEVERGONE.AcceptErrorRouter/1"
        assert 0.0 <= float(manifest["threshold"]) <= 1.0
        assert manifest["calibration"]["kept_precision"] >= 0.95
        assert manifest["policy"]["promotions_allowed"] is False
        assert (output_dir / "accept_error_router.joblib").exists()

    print("Nevergone accept-error router v10 smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
