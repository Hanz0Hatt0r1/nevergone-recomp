#!/usr/bin/env python3
"""Smoke-test ProductionClassifier/5 evidence aggregation, training and inference."""
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import joblib

ROOT = Path(__file__).resolve().parents[2]
ML = ROOT / "tools/ml"
sys.path.insert(0, str(ML))

from build_nevergone_v5_evidence import FORMAT as EVIDENCE_FORMAT  # noqa: E402
from train_nevergone_production_classifier_v5 import ARTIFACT_FORMAT_V5  # noqa: E402


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        dataset = root / "dataset"
        dataset.mkdir()
        trace = dataset / "trace.jsonl"
        families = root / "families.jsonl"
        callgraph = root / "callgraph.jsonl"
        receivers = root / "receiver.tsv"
        inference = root / "inference.jsonl"
        evidence = root / "evidence.jsonl"
        model_dir = root / "model"
        classified = root / "classified.jsonl"

        dataset_rows: list[dict] = []
        trace_rows: list[dict] = []
        family_rows: list[dict] = []
        callgraph_rows: list[dict] = []
        receiver_rows: list[dict[str, str]] = []
        inference_rows: list[dict] = []

        for index in range(20):
            label = "game" if index < 10 else "engine"
            record_id = f"id-{index:02d}"
            entry = f"{0x1000 + index * 4:08x}"
            source_id = f"libcocos2dcpp.so:{entry}"
            if label == "game":
                text = (
                    "OP LOAD out=v:4 addr=reg:4\n"
                    "OP CALLIND call:indirect arg=v:4\n"
                    f"OP INT_ADD out=v:4 in=const:small:0x{(index % 3) + 2:x}:4\n"
                    "OP RETURN ret=v:4"
                )
            else:
                text = (
                    "OP COPY out=v:4 in=reg:4\n"
                    "OP INT_ADD out=v:4 in=const:one:4\n"
                    "OP CALL call:internal arg=v:4\n"
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
            trace_rows.append(
                {
                    "id": record_id,
                    "program": "libcocos2dcpp.so",
                    "source_entry": entry,
                    "source_name": f"synthetic_{index}",
                    "split": "train",
                    "labels": {"code_domain": label},
                }
            )
            family_rows.append({"id": record_id, "family_id": f"family-{index:02d}"})
            callgraph_rows.append(
                {
                    "format": "NEVERGONE.CallGraphMetrics/1",
                    "id": source_id,
                    "metrics": {
                        "fan_in": index % 4,
                        "fan_out": 3 if label == "game" else 1,
                        "resolved_call_ratio_proxy": 0.5 if label == "game" else 1.0,
                    },
                }
            )
            receiver_rows.append(
                {
                    "source_id": source_id,
                    "receiver_present": "1",
                    "receiver_pointer_depth": "1",
                    "receiver_storage_kind": "unique",
                    "pointee_type_display": "SecretGameClass" if label == "game" else "SecretEngineClass",
                    "flow_single_pointer_pointee_displays_json": json.dumps(
                        ["SecretGameClass" if label == "game" else "SecretEngineClass"]
                    ),
                }
            )
            inference_rows.append(
                {
                    "format": "NEVERGONE.StaticReceiverVtableInference/1",
                    "source_id": source_id,
                    "call_index": 0,
                    "status": "resolved_unique_vtable",
                    "class_name": "SecretGameClass" if label == "game" else "SecretEngineClass",
                    "candidate_receiver_class_names": [
                        "SecretGameClass" if label == "game" else "SecretEngineClass"
                    ],
                    "class_evidence_source": "direct_receiver_type",
                    "dispatch_slot_offset": "0x24",
                }
            )

        write_jsonl(dataset / "train.jsonl", dataset_rows)
        (dataset / "validation.jsonl").write_text("", encoding="utf-8")
        (dataset / "test.jsonl").write_text("", encoding="utf-8")
        write_jsonl(trace, trace_rows)
        write_jsonl(families, family_rows)
        write_jsonl(callgraph, callgraph_rows)
        write_jsonl(inference, inference_rows)

        receiver_fields = [
            "source_id",
            "receiver_present",
            "receiver_pointer_depth",
            "receiver_storage_kind",
            "pointee_type_display",
            "flow_single_pointer_pointee_displays_json",
        ]
        with receivers.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=receiver_fields, delimiter="\t")
            writer.writeheader()
            writer.writerows(receiver_rows)

        subprocess.run(
            [
                sys.executable,
                str(ML / "build_nevergone_v5_evidence.py"),
                str(trace),
                "--callgraph-metrics",
                str(callgraph),
                "--receiver-types",
                str(receivers),
                "--receiver-inference",
                str(inference),
                "--output",
                str(evidence),
            ],
            check=True,
        )
        evidence_rows = [
            json.loads(line)
            for line in evidence.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        assert len(evidence_rows) == 20
        assert all(row["format"] == EVIDENCE_FORMAT for row in evidence_rows)
        serialized = evidence.read_text(encoding="utf-8")
        assert "SecretGameClass" not in serialized
        assert "SecretEngineClass" not in serialized
        assert "0x24" not in serialized

        subprocess.run(
            [
                sys.executable,
                str(ML / "train_nevergone_production_classifier_v5.py"),
                str(dataset),
                str(families),
                str(model_dir),
                "--evidence",
                str(evidence),
                "--classes",
                "game",
                "engine",
                "--folds",
                "2",
                "--min-class-calibration-examples",
                "2",
                "--min-class-threshold-examples",
                "1",
                "--min-ood-threshold-examples",
                "1",
            ],
            check=True,
        )

        artifact = joblib.load(model_dir / "classifier.joblib")
        assert artifact["format"] == ARTIFACT_FORMAT_V5
        manifest = json.loads((model_dir / "manifest.json").read_text(encoding="utf-8"))
        assert manifest["format"] == ARTIFACT_FORMAT_V5
        assert manifest["leakage_policy"]["receiver_class_names_used"] is False

        subprocess.run(
            [
                sys.executable,
                str(ML / "classify_nevergone_pcode.py"),
                str(model_dir),
                str(dataset / "train.jsonl"),
                "--evidence",
                str(evidence),
                "--output",
                str(classified),
                "--no-probabilities",
            ],
            check=True,
        )
        results = [
            json.loads(line)
            for line in classified.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        assert len(results) == 20
        assert all(row["format"] == ARTIFACT_FORMAT_V5 for row in results)
        assert all(row["evidence_token_count"] > 0 for row in results)

    print("ProductionClassifier/5 smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
