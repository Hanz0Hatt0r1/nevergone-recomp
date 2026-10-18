#!/usr/bin/env python3
"""Smoke test the path-free router-gate availability audit."""
from __future__ import annotations

import json
import re
import tempfile
from pathlib import Path

from audit_nevergone_frozen_router_gate_availability import build_report


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def write_rows(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="nevergone-router-gate-availability-") as temporary:
        root = Path(temporary)
        model = root / "models/production-code-domain-v5.2"
        dataset = root / "dataset-4class"
        router = root / "models/accept-error-router-v10"
        frozen = root / "frozen-validation/unseen500-v10"
        manual = root / "dataset-manual-restoration-v70"

        training = [{"id": f"train-{i}", "truth": "engine"} for i in range(3)]
        write_rows(model / "oof_predictions.jsonl", training)
        write_json(model / "manifest.json", {"format": "NEVERGONE.ProductionClassifier/5"})
        write_json(dataset / "manifest.json", {"splits": {"train": 1, "validation": 1, "test": 1}})
        write_rows(dataset / "train.jsonl", [{"id": "train-0"}])
        write_rows(dataset / "validation.jsonl", [{"id": "train-1"}])
        write_rows(dataset / "test.jsonl", [{"id": "train-2"}])
        write_json(router / "manifest.json", {
            "threshold": 0.15,
            "calibration": {
                "method": "outer-fold-crossfit-oof",
                "training_feedback": False,
                "kept_precision": 0.97301,
                "kept_coverage": 0.560759,
                "kept_examples": 2,
            },
        })
        (router / "accept_error_router.joblib").write_bytes(b"synthetic artifact")
        write_json(frozen / "manifest.json", {
            "tag": "unseen500-v10",
            "frozen_corpus": {"sha256": "c" * 64},
        })
        write_json(frozen / "validation-summary.json", {
            "labels_available": False,
            "rows": 500,
            "routed_action_counts": {"accept": 200, "review": 200, "escalate": 100},
            "router_changed": 10,
        })
        write_json(manual / "manifest.json", {"format": "NEVERGONE.MlDataset/2"})
        write_rows(manual / "train.jsonl", [
            {"id": "train-0", "labels": {"code_domain": "engine"}},
            {"id": "new-1", "labels": {"code_domain": "game"}},
        ])
        write_rows(manual / "validation.jsonl", [{"id": "train-1", "labels": {"code_domain": "engine"}}])
        write_rows(manual / "test.jsonl", [{"id": "train-2", "labels": {"code_domain": "engine"}}])

        report = build_report(root)
        assert report["format"] == "NEVERGONE.RouterGateAvailability/1"
        assert report["status"] == "not_evaluated"
        assert report["gate_metrics"]["evaluated"] is False
        assert report["current_crossfit_candidate"]["accepted_precision"] == 0.97301
        assert report["available_holdouts"]["nominal_test_split"]["overlap_with_classifier_oof_rows"] == 1
        assert report["available_holdouts"]["unseen500_v10"]["labels_available"] is False
        assert report["available_holdouts"]["manual_restoration_v70_new_identities"]["new_code_domain_labeled_rows"] == 1
        serialized = json.dumps(report)
        assert temporary not in serialized
        assert "accepted_precision\": null" in serialized

    evidence = Path(__file__).resolve().parents[2] / "docs/evidence/frozen-router-gate-availability-v10/availability.json"
    committed = json.loads(evidence.read_text(encoding="utf-8"))
    assert committed["format"] == "NEVERGONE.RouterGateAvailability/1"
    assert committed["status"] == "not_evaluated"
    assert committed["generic_gate_evaluator"]["revision"] == "0d05223391729fcb2dec2d7187a371d0ba84d690"
    assert committed["generic_gate_evaluator"]["consumer_adapter_required"] is True
    assert committed["gate_metrics"]["evaluated"] is False
    assert committed["gate_metrics"]["accepted_precision"] is None
    assert committed["current_crossfit_candidate"]["accepted_precision"] == 0.9730099745746137
    assert committed["current_crossfit_candidate"]["coverage"] == 0.5607589383636763
    assert committed["available_holdouts"]["nominal_test_split"]["overlap_with_classifier_oof_rows"] == 808
    assert committed["available_holdouts"]["unseen500_v10"]["labels_available"] is False
    assert committed["available_holdouts"]["manual_restoration_v70_new_identities"]["new_code_domain_labeled_rows"] == 1
    assert all(re.fullmatch(r"[a-f0-9]{64}", value) for value in committed["source_sha256"].values())
    assert "/home/" not in json.dumps(committed)

    dashboard_dir = Path(__file__).resolve().parents[2] / "docs/evidence/nevergone-kpi-dashboard-v1"
    dashboard = json.loads((dashboard_dir / "dashboard.json").read_text(encoding="utf-8"))
    markdown = (dashboard_dir / "dashboard.md").read_text(encoding="utf-8")
    assert dashboard["format"] == "REVENGINSTR.KPIDashboard/1"
    assert dashboard["status"] == "fail"
    kpis = {row["id"]: row for row in dashboard["kpis"]}
    assert kpis["unseen_receiver_function_coverage"]["value"] == 0.106
    assert kpis["unseen_receiver_function_coverage"]["status"] == "fail"
    assert kpis["router_frozen_accepted_precision"]["status"] == "unavailable"
    assert "/home/" not in json.dumps(dashboard) + markdown

    print("nevergone frozen router gate availability smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
