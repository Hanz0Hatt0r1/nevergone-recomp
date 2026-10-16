#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PREP = ROOT / "tools/ml/prepare_nevergone_revenginstr_validation_inputs.py"
AUDIT = ROOT / "tools/ml/audit_nevergone_revenginstr_validation_parity.py"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def corpus_row(
    entry: str,
    name: str,
    *,
    external: bool = False,
    status: str = "ok",
    corpus_format: str = "NEVERGONE.MlPcodeCorpus/2",
) -> dict:
    row = {
        "format": corpus_format,
        "program": "libcocos2dcpp.so",
        "source_entry": entry,
        "source_name": name,
        "external": external,
        "decompile_status": status,
    }
    if status == "ok":
        row["metrics"] = {"emitted_ops": 1, "truncated": False}
        row["model_text"] = "ARCH=ARM32\nENDIAN=LE\nPTR_SIZE=4\no0 RETURN\n"
    else:
        row["error"] = "synthetic"
    return row


def synthetic_drift(fmt: str) -> dict:
    shift = {
        "reference": {"engine": 0.5, "game": 0.5},
        "frozen": {"engine": 0.6, "game": 0.4},
        "js_divergence_bits": 0.01,
    }
    numeric = {
        "confidence": {
            "reference": {"count": 2, "mean": 0.8, "p10": 0.7, "median": 0.8, "p90": 0.9, "iqr": 0.1},
            "frozen": {"count": 2, "mean": 0.75, "p10": 0.65, "median": 0.75, "p90": 0.85, "iqr": 0.1},
            "median_delta": -0.05,
            "robust_shift_iqr": -0.5,
        }
    }
    return {
        "format": fmt,
        "labels_available": False,
        "reference_rows": 10,
        "frozen_rows": 5,
        "categorical_shift": {"prediction": shift, "base_action": shift, "routed_action": shift},
        "router_intervention": {
            "reference_legacy_accept": 6,
            "reference_changed": 1,
            "reference_change_fraction_all": 0.1,
            "reference_change_fraction_accept": 1 / 6,
            "frozen_legacy_accept": 3,
            "frozen_changed": 1,
            "frozen_change_fraction_all": 0.2,
            "frozen_change_fraction_accept": 1 / 3,
            "accept_change_rate_ratio": 2.0,
        },
        "numeric_shift_all": numeric,
        "numeric_shift_base_accept": numeric,
        "evidence_shift": {
            "reference_rows": 10,
            "frozen_rows": 5,
            "core": {
                "has_callgraph": {"reference": 1.0, "frozen": 1.0, "delta": 0.0}
            },
            "top_feature_coverage_shifts": [],
        },
        "severity": {
            "level": "moderate",
            "diagnostic_only": True,
            "thresholds": {"x": 1.0},
            "observed": {"x": 2.0},
        },
        "note": "format-specific explanatory text",
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        source = root / "corpus.jsonl"
        generic = root / "generic.jsonl"
        eligible = root / "eligible.jsonl"
        routing = root / "routing.jsonl"
        actions = root / "actions.jsonl"
        write_jsonl(
            source,
            [
                corpus_row("00100000", "GameScene::update(float)"),
                corpus_row("00100010", "_FINI_0", corpus_format="REVENGINSTR.PcodeCorpus/1"),
                corpus_row("00100020", "<EXTERNAL>::puts", external=True),
                corpus_row("00100030", "FUN_00100030", status="failed"),
            ],
        )
        write_jsonl(
            routing,
            [
                {"id": "a", "legacy_action": "accept"},
                {"id": "b", "legacy_action": "review"},
                {"id": "c", "legacy_action": "escalate"},
            ],
        )
        result = subprocess.run(
            [
                sys.executable,
                str(PREP),
                str(source),
                str(generic),
                str(eligible),
                "--routing-oof",
                str(routing),
                "--reference-actions-output",
                str(actions),
            ],
            check=True,
            text=True,
            capture_output=True,
        )
        assert "eligible rows: 1" in result.stdout
        generic_rows = [json.loads(line) for line in generic.read_text().splitlines()]
        assert len(generic_rows) == 4
        assert all(row["format"] == "REVENGINSTR.PcodeCorpus/1" for row in generic_rows)
        assert generic_rows[0]["model_text"] == "ARCH=ARM32\nENDIAN=LE\nPTR_SIZE=4\no0 RETURN\n"
        eligible_rows = [json.loads(line) for line in eligible.read_text().splitlines()]
        assert eligible_rows == [
            {
                "consumer_origin": "internal",
                "program": "libcocos2dcpp.so",
                "source_entry": "00100000",
            }
        ]
        action_rows = [json.loads(line) for line in actions.read_text().splitlines()]
        assert action_rows == [
            {"action": "accept", "id": "a"},
            {"action": "review", "id": "b"},
            {"action": "escalate", "id": "c"},
        ]

        historical = root / "historical.json"
        current = root / "generic-drift.json"
        parity = root / "parity.json"
        historical.write_text(
            json.dumps(synthetic_drift("NEVERGONE.FrozenDriftAuditV10/1")), encoding="utf-8"
        )
        generic_payload = synthetic_drift("REVENGINSTR.DriftAudit/1")
        generic_payload["note"] = "generic wording may differ"
        current.write_text(json.dumps(generic_payload), encoding="utf-8")
        passed = subprocess.run(
            [
                sys.executable,
                str(AUDIT),
                str(historical),
                str(current),
                "--strict",
                "--output",
                str(parity),
            ],
            check=True,
            text=True,
            capture_output=True,
        )
        assert "validation parity pass: True" in passed.stdout
        report = json.loads(parity.read_text())
        assert report["parity_pass"] is True
        assert report["mismatch_count"] == 0

        generic_payload["router_intervention"]["frozen_changed"] = 2
        current.write_text(json.dumps(generic_payload), encoding="utf-8")
        failed = subprocess.run(
            [sys.executable, str(AUDIT), str(historical), str(current), "--strict"],
            text=True,
            capture_output=True,
        )
        assert failed.returncode == 2
        assert "validation parity pass: False" in failed.stdout

    print("nevergone revenginstr validation parity smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
