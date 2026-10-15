#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
AUDIT = HERE / "audit_nevergone_conditional_fusion_readiness.py"


def cv_log(*, engine_correct: int = 20, folds: int = 5) -> str:
    outer = []
    for fold in range(1, folds + 1):
        outer.append(
            "outer_fold=%d n=40 per_label={'examples': 4, 'coverage': 0.1, 'precision': 1.0} "
            "per_label_action={'examples': 4, 'coverage': 0.1, 'precision': 1.0}"
            % fold
        )
    return "\n".join(
        [
            "model: NevergoneNestedConditionalFusionCV/1",
            "task: code_domain",
            "functions: 200",
            "families: 80",
            "classes: Counter({'engine': 80, 'game': 60, 'media_library': 40, 'runtime_library': 20})",
            "trusted transfer classes: ['engine', 'media_library', 'runtime_library']",
            "conditional target precision: 0.95",
            "min conditional examples: 10",
            "",
            *outer,
            "",
            "BASE CLASSIFIER ACCEPT",
            "{'examples': 100, 'coverage': 0.5, 'precision': 0.98}",
            "",
            "CONDITIONAL PROMOTION",
            "per_label {'examples': 35, 'coverage': 0.175, 'precision': 0.9428571428571428}",
            "per_label_action {'examples': 35, 'coverage': 0.175, 'precision': 0.9428571428571428}",
            "",
            "FUSED ACCEPT",
            "per_label {'examples': 135, 'coverage': 0.675, 'precision': 0.9703703703703703}",
            "per_label_action {'examples': 135, 'coverage': 0.675, 'precision': 0.9703703703703703}",
            "",
            "PROMOTION BY LABEL/ACTION",
            "per_label",
            f"  engine/review: examples=20 correct={engine_correct} precision={engine_correct/20:.4f}",
            "  media_library/review: examples=10 correct=9 precision=0.9000",
            "  engine/escalate: examples=5 correct=5 precision=1.0000",
            "per_label_action",
            f"  engine/review: examples=20 correct={engine_correct} precision={engine_correct/20:.4f}",
            "  media_library/review: examples=10 correct=9 precision=0.9000",
            "  engine/escalate: examples=5 correct=5 precision=1.0000",
            "",
            "POLICY STABILITY",
            "per_label",
            "  engine: enabled_folds=5/5 sim_thresholds=[0.9] gap_thresholds=[0.1] inner_precision_min=0.96",
            "  media_library: enabled_folds=4/5 sim_thresholds=[0.95] gap_thresholds=[0.15] inner_precision_min=0.95",
            "per_label_action",
            "  engine/review: enabled_folds=5/5 sim_thresholds=[0.9] gap_thresholds=[0.1] inner_precision_min=0.96",
            "  engine/escalate: enabled_folds=2/5 sim_thresholds=[0.95] gap_thresholds=[0.2] inner_precision_min=1.0",
            "  media_library/review: enabled_folds=4/5 sim_thresholds=[0.95] gap_thresholds=[0.15] inner_precision_min=0.95",
            "",
            "PROMOTED ERRORS",
            "per_label",
            "  x truth=engine classifier=engine:review transfer=media_library sim=0.9800 gap=0.2000 fold=1",
            "per_label_action",
            "  x truth=engine classifier=engine:review transfer=media_library sim=0.9800 gap=0.2000 fold=1",
            "",
        ]
    )


def run(log: str, *, require_ready: bool, extra: list[str] | None = None) -> tuple[subprocess.CompletedProcess[str], dict]:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        source = root / "cv.txt"
        output = root / "readiness.json"
        source.write_text(log, encoding="utf-8")
        cmd = [
            sys.executable,
            str(AUDIT),
            str(source),
            "--output",
            str(output),
            "--scope",
            "per_label_action",
            "--candidate-labels",
            "engine",
            "media_library",
            "--min-outer-precision",
            "0.95",
            "--min-examples",
            "10",
            "--min-enabled-folds",
            "3",
            "--min-inner-precision",
            "0.95",
            "--min-total-promotions",
            "10",
        ]
        if require_ready:
            cmd.append("--require-ready")
        if extra:
            cmd.extend(extra)
        proc = subprocess.run(cmd, text=True, capture_output=True)
        data = json.loads(output.read_text()) if output.is_file() else {}
        return proc, data


def main() -> int:
    proc, data = run(cv_log(), require_ready=True)
    assert proc.returncode == 0, (proc.stdout, proc.stderr)
    assert data["ready_for_independent_validation"] is True
    assert data["eligible_policy_keys"] == ["engine/review"]
    assert data["candidate_fused_accept"]["promoted_examples"] == 20
    assert data["candidate_fused_accept"]["promoted_correct"] == 20
    assert data["candidate_fused_accept"]["precision"] > data["base_accept"]["precision"]
    assert data["production_routing_changed"] is False
    assert data["policy_keys"]["media_library/review"]["reasons"] == ["outer_precision_below_gate"]
    assert "policy_not_enabled_in_enough_folds" in data["policy_keys"]["engine/escalate"]["reasons"]

    proc, data = run(cv_log(engine_correct=18), require_ready=True)
    assert proc.returncode == 2, (proc.stdout, proc.stderr)
    assert data["ready_for_independent_validation"] is False
    assert data["eligible_policy_keys"] == []
    assert "no_policy_key_passed" in data["global_reasons"]

    proc, _ = run(cv_log(folds=4), require_ready=False)
    assert proc.returncode != 0

    proc, _ = run(
        cv_log(),
        require_ready=False,
        extra=["--candidate-labels", "game"],
    )
    assert proc.returncode != 0

    print("conditional fusion readiness smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
