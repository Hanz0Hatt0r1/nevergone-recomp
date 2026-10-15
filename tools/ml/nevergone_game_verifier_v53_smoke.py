#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ML = ROOT / "tools/ml"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        dataset = root / "dataset"
        dataset.mkdir()
        families = root / "families.jsonl"
        evidence = root / "evidence.jsonl"
        base_oof = root / "base-oof.jsonl"
        output = root / "out"

        dataset_rows = []
        family_rows = []
        evidence_rows = []
        oof_rows = []
        for index in range(24):
            label = "game" if index < 8 else "engine"
            record_id = f"id-{index:02d}"
            if label == "game":
                text = (
                    "OP LOAD out=v:4 addr=reg:4\n"
                    "OP CALLIND call:indirect arg=v:4\n"
                    "OP CALLIND call:indirect arg=v:4\n"
                    "OP INT_ADD out=v:4 in=const:small:0x3:4\n"
                    "OP RETURN ret=v:4"
                )
            else:
                text = (
                    "OP COPY out=v:4 in=reg:4\n"
                    "OP CALL call:internal arg=v:4\n"
                    "OP INT_ADD out=v:4 in=const:one:4\n"
                    "OP RETURN ret=v:4"
                )
            dataset_rows.append(
                {
                    "id": record_id,
                    "text": text,
                    "labels": {"code_domain": label},
                    "weights": {"code_domain": 1.0},
                }
            )
            family_rows.append({"id": record_id, "family_id": f"family-{index:02d}"})
            evidence_rows.append(
                {
                    "format": "NEVERGONE.ProductionEvidence/1",
                    "id": record_id,
                    "features": {
                        "receiver.call_count": 2.0 if label == "game" else 1.0,
                        "receiver.resolved_ratio": 1.0 if label == "game" else 0.0,
                    },
                }
            )

            # Deliberately inject engine->game false positives into the base OOF.
            if label == "engine" and index % 3 == 0:
                prediction = "game"
            elif label == "game" and index == 1:
                prediction = "engine"
            else:
                prediction = label
            oof_rows.append(
                {
                    "id": record_id,
                    "truth": label,
                    "prediction": prediction,
                }
            )

        write_jsonl(dataset / "train.jsonl", dataset_rows)
        (dataset / "validation.jsonl").write_text("", encoding="utf-8")
        (dataset / "test.jsonl").write_text("", encoding="utf-8")
        write_jsonl(families, family_rows)
        write_jsonl(evidence, evidence_rows)
        write_jsonl(base_oof, oof_rows)

        completed = subprocess.run(
            [
                sys.executable,
                str(ML / "evaluate_nevergone_game_verifier_v53.py"),
                str(dataset),
                str(families),
                str(base_oof),
                str(output),
                "--evidence",
                str(evidence),
                "--folds",
                "2",
                "--inner-folds",
                "2",
                "--target-game-precision",
                "0.40",
            ],
            check=True,
            text=True,
            capture_output=True,
        )
        assert "training feedback: no" in completed.stdout, completed.stdout

        summary = json.loads((output / "summary.json").read_text(encoding="utf-8"))
        assert summary["format"] == "NEVERGONE.GameVerifierV53/1"
        assert summary["demotion_only"] is True
        assert summary["leakage_policy"]["threshold_selected_on_outer_test"] is False
        assert summary["leakage_policy"]["outer_test_family_seen_by_verifier"] is False
        assert summary["flips"] > 0, summary
        assert summary["helpful_flips"] >= summary["harmful_flips"], summary
        assert summary["verified_game_precision"] >= summary["base_game_precision"], summary

        rows_text = (output / "oof_verifier_predictions.jsonl").read_text(encoding="utf-8")
        assert "SecretGameClass" not in rows_text
        assert "SecretEngineClass" not in rows_text

    print("ProductionClassifier v5.3 game verifier smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
