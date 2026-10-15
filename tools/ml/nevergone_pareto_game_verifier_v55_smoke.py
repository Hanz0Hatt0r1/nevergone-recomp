#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ML = ROOT / "tools/ml"
sys.path.insert(0, str(ML))

from evaluate_nevergone_pareto_game_verifier_v55 import FORMAT, generate_policies  # noqa: E402


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def main() -> int:
    families_present = {policy.get("family") for policy in generate_policies()}
    assert {"noop", "single_margin", "confidence_ceiling", "two_stage_margin"} <= families_present

    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        dataset = root / "dataset"
        dataset.mkdir()
        families = root / "families.jsonl"
        evidence = root / "evidence.jsonl"
        base_oof = root / "base-oof.jsonl"
        output = root / "out"

        labels = ["game", "engine", "runtime_library", "media_library"]
        dataset_rows: list[dict] = []
        family_rows: list[dict] = []
        evidence_rows: list[dict] = []
        oof_rows: list[dict] = []

        for index in range(48):
            label = labels[index // 12]
            record_id = f"id-{index:02d}"
            if label == "game":
                text = (
                    "OP LOAD out=v:4 addr=reg:4\n"
                    "OP CALLIND call:indirect arg=v:4\n"
                    "OP INT_ADD out=v:4 in=const:small:0x3:4\n"
                    "OP RETURN ret=v:4"
                )
            elif label == "engine":
                text = (
                    "OP COPY out=v:4 in=reg:4\n"
                    "OP CALL call:internal arg=v:4\n"
                    "OP INT_ADD out=v:4 in=const:one:4\n"
                    "OP RETURN ret=v:4"
                )
            elif label == "runtime_library":
                text = (
                    "OP PTRADD out=v:4 base=reg:4 index=v:4\n"
                    "OP LOAD out=v:4 addr=v:4\n"
                    "OP RETURN ret=v:4"
                )
            else:
                text = (
                    "OP MULTIEQUAL out=v:4 in=v:4 in=v:4\n"
                    "OP INT_XOR out=v:4 in=v:4 in=const:small:0x7:4\n"
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
                        "receiver.call_count": 2.0 if label == "game" else 0.0,
                        "receiver.resolved_ratio": 1.0 if label == "game" else 0.0,
                        "callgraph.fan_out": 3.0 if label in {"game", "engine"} else 1.0,
                    },
                }
            )

            prediction = label
            if label == "engine" and index % 5 == 0:
                prediction = "game"
            oof_rows.append({"id": record_id, "truth": label, "prediction": prediction})

        write_jsonl(dataset / "train.jsonl", dataset_rows)
        (dataset / "validation.jsonl").write_text("", encoding="utf-8")
        (dataset / "test.jsonl").write_text("", encoding="utf-8")
        write_jsonl(families, family_rows)
        write_jsonl(evidence, evidence_rows)
        write_jsonl(base_oof, oof_rows)

        completed = subprocess.run(
            [
                sys.executable,
                str(ML / "evaluate_nevergone_pareto_game_verifier_v55.py"),
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
                "0.01",
                "--max-game-recall-loss",
                "0.10",
                "--target-game-precision",
                "0.45",
            ],
            check=True,
            text=True,
            capture_output=True,
        )
        assert "training feedback: no" in completed.stdout, completed.stdout

        summary = json.loads((output / "summary.json").read_text(encoding="utf-8"))
        assert summary["format"] == FORMAT
        assert summary["training_feedback"] is False
        assert summary["leakage_policy"]["outer_test_used_for_policy_selection"] is False
        assert summary["leakage_policy"]["precomputed_base_oof_used_for_selection"] is False
        assert sum(summary["policy_family_rows"].values()) == 48
        assert len(summary["folds"]) == 2
        for fold in summary["folds"]:
            assert fold["policy_family"] in families_present
            assert fold["inner_pareto_frontier_size"] >= 1

        rows_text = (output / "oof_pareto_verifier_predictions.jsonl").read_text(encoding="utf-8")
        assert "SecretGameClass" not in rows_text
        assert "SecretEngineClass" not in rows_text

    print("ProductionClassifier v5.5 Pareto game verifier smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
