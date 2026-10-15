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

        classes = ["game", "engine", "runtime_library", "media_library"]
        dataset_rows = []
        family_rows = []
        evidence_rows = []
        oof_rows = []

        index = 0
        for class_index, label in enumerate(classes):
            for local in range(10):
                record_id = f"id-{index:03d}"
                if label == "game":
                    text = (
                        "OP LOAD out=v:4 addr=reg:4\n"
                        "OP CALLIND call:indirect arg=v:4\n"
                        "OP INT_ADD out=v:4 in=const:small:0x3:4\n"
                        "OP RETURN ret=v:4"
                    )
                elif label == "engine":
                    if local % 4 == 0:
                        text = (
                            "OP LOAD out=v:4 addr=reg:4\n"
                            "OP CALLIND call:indirect arg=v:4\n"
                            "OP INT_ADD out=v:4 in=const:small:0x2:4\n"
                            "OP RETURN ret=v:4"
                        )
                    else:
                        text = (
                            "OP COPY out=v:4 in=reg:4\n"
                            "OP CALL call:internal arg=v:4\n"
                            "OP INT_ADD out=v:4 in=const:one:4\n"
                            "OP RETURN ret=v:4"
                        )
                elif label == "runtime_library":
                    text = (
                        "OP COPY out=v:4 in=reg:4\n"
                        "OP PTRADD out=v:4 in=reg:4\n"
                        "OP RETURN ret=v:4"
                    )
                else:
                    text = (
                        "OP LOAD out=v:8 addr=reg:4\n"
                        "OP INT_MULT out=v:8 in=const:small:0x8:4\n"
                        "OP CALL call:external arg=v:8\n"
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
                family_rows.append({"id": record_id, "family_id": f"family-{index:03d}"})
                evidence_rows.append(
                    {
                        "format": "NEVERGONE.ProductionEvidence/1",
                        "id": record_id,
                        "features": {
                            "receiver.call_count": 2.0 if label == "game" else (1.0 if label == "engine" else 0.0),
                            "receiver.resolved_ratio": 0.8 if label == "game" else (0.2 if label == "engine" else 0.0),
                            "callgraph.fan_out": float((class_index + 1) * 2),
                        },
                    }
                )
                oof_rows.append(
                    {
                        "id": record_id,
                        "truth": label,
                        "prediction": label,
                    }
                )
                index += 1

        write_jsonl(dataset / "train.jsonl", dataset_rows)
        (dataset / "validation.jsonl").write_text("", encoding="utf-8")
        (dataset / "test.jsonl").write_text("", encoding="utf-8")
        write_jsonl(families, family_rows)
        write_jsonl(evidence, evidence_rows)
        write_jsonl(base_oof, oof_rows)

        completed = subprocess.run(
            [
                sys.executable,
                str(ML / "evaluate_nevergone_selective_game_verifier_v54.py"),
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
                "--max-balanced-loss",
                "0.02",
                "--max-game-recall-loss",
                "0.10",
            ],
            check=True,
            text=True,
            capture_output=True,
        )
        assert "training feedback: no" in completed.stdout, completed.stdout

        summary = json.loads((output / "summary.json").read_text(encoding="utf-8"))
        assert summary["format"] == "NEVERGONE.SelectiveGameVerifierV54/1"
        assert summary["training_feedback"] is False
        leakage = summary["leakage_policy"]
        assert leakage["outer_test_used_for_policy_selection"] is False
        assert leakage["outer_test_family_seen_by_base"] is False
        assert leakage["outer_test_family_seen_by_verifier"] is False
        assert leakage["inner_base_recomputed"] is True
        assert leakage["precomputed_base_oof_used_for_selection"] is False
        assert len(summary["folds"]) == 2
        assert all(fold["inner_selected"]["eligible"] for fold in summary["folds"])

        rows = [
            json.loads(line)
            for line in (output / "oof_selective_verifier_predictions.jsonl").read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        assert len(rows) == 40
        assert all(row["format"] == "NEVERGONE.SelectiveGameVerifierV54/1" for row in rows)
        serialized = json.dumps(rows)
        assert "SecretGameClass" not in serialized
        assert "SecretEngineClass" not in serialized

    print("ProductionClassifier v5.4 selective game verifier smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
