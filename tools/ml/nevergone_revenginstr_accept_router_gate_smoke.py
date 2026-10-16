#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / "tools/ml/audit_nevergone_revenginstr_accept_router.py"


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        reference = root / "reference.json"
        candidate = root / "candidate.json"
        output = root / "report.json"
        reference.write_text(json.dumps({
            "format": "NEVERGONE.AcceptErrorRouter/1",
            "threshold": 0.15,
            "calibration": {"kept_precision": 0.97301, "kept_coverage": 0.560759},
        }), encoding="utf-8")
        candidate.write_text(json.dumps({
            "format": "REVENGINSTR.AcceptErrorRouter/1",
            "threshold": 0.2,
            "calibration": {
                "kept_precision": 0.974,
                "kept_coverage": 0.555,
                "folds": 5,
                "legacy_accept_examples": 5300,
                "kept_examples": 5060,
            },
            "policy": {
                "scope": "base_accept_only",
                "below_threshold_action": "escalate",
                "non_accept_action": "preserve",
                "class_prediction_action": "preserve",
                "promotions_allowed": False,
            },
        }), encoding="utf-8")
        result = subprocess.run(
            [sys.executable, str(AUDIT), str(reference), str(candidate),
             "--strict", "--output", str(output)],
            text=True, capture_output=True,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        report = json.loads(output.read_text(encoding="utf-8"))
        assert report["migration_gate_pass"] is True
        assert report["invariants"]["pass"] is True

        broken = json.loads(candidate.read_text(encoding="utf-8"))
        broken["calibration"]["kept_coverage"] = 0.40
        candidate.write_text(json.dumps(broken), encoding="utf-8")
        result = subprocess.run(
            [sys.executable, str(AUDIT), str(reference), str(candidate), "--strict"],
            text=True, capture_output=True,
        )
        assert result.returncode == 2
        assert "migration gate pass: False" in result.stdout

    print("nevergone revenginstr accept-router gate smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
