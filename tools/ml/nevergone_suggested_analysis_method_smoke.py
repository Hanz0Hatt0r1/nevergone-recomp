#!/usr/bin/env python3
"""Dependency-light smoke checks for deterministic analysis-method suggestions."""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

from suggest_nevergone_analysis_method import recommend_analysis_method


def base_row() -> dict:
    return {
        "id": "f",
        "final_action": "review",
        "structural_metrics": {
            "op_count": 20,
            "definition_coverage_proxy": 1.0,
            "cyclomatic_proxy": 2,
            "memory_op_ratio": 0.1,
            "indirect_call_ratio": 0.0,
        },
        "callgraph_metrics": {
            "fan_in": 1,
            "fan_out": 2,
            "corpus_call_count": 2,
            "unknown_call_ratio_proxy": 0.0,
        },
        "similarity_transfer": {
            "similarity": 0.7,
            "gap": 0.01,
            "prediction": "game",
            "trusted": False,
        },
    }


def main() -> int:
    row = base_row()
    assert recommend_analysis_method(row)["primary"] == "standard_decompilation_review"

    indirect = base_row()
    indirect["callgraph_metrics"]["corpus_call_count"] = 7
    indirect["callgraph_metrics"]["unknown_call_ratio_proxy"] = 1.0
    indirect["structural_metrics"]["indirect_call_ratio"] = 1.0
    assert recommend_analysis_method(indirect)["primary"] == "resolve_indirect_calls"

    truncated = base_row()
    truncated["structural_metrics"]["op_count"] = 256
    truncated["structural_metrics"]["definition_coverage_proxy"] = 0.90
    assert recommend_analysis_method(truncated)["primary"] == "expand_pcode"

    callers = base_row()
    callers["callgraph_metrics"]["fan_in"] = 455
    assert recommend_analysis_method(callers)["primary"] == "inspect_callers"

    callees = base_row()
    callees["callgraph_metrics"]["fan_out"] = 43
    assert recommend_analysis_method(callees)["primary"] == "inspect_callees"

    complex_row = base_row()
    complex_row["structural_metrics"]["cyclomatic_proxy"] = 34
    assert recommend_analysis_method(complex_row)["primary"] == "control_flow_review"

    similar = base_row()
    similar["similarity_transfer"] = {
        "similarity": 0.99,
        "gap": 0.2,
        "prediction": "runtime_library",
        "trusted": True,
    }
    assert recommend_analysis_method(similar)["primary"] == "compare_nearest_family"

    # Semantic labels and final routing must not affect the deterministic method.
    same_signals = base_row()
    same_signals["final_action"] = "accept"
    same_signals["code_domain"] = {"prediction": "engine"}
    same_signals["similarity_transfer"]["prediction"] = "media_library"
    assert recommend_analysis_method(same_signals) == recommend_analysis_method(base_row())

    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        input_path = root / "input.jsonl"
        output_path = root / "output.jsonl"
        input_path.write_text(json.dumps(indirect) + "\n", encoding="utf-8")
        tool = Path(__file__).with_name("suggest_nevergone_analysis_method.py")
        subprocess.run(
            [sys.executable, str(tool), str(input_path), "--output", str(output_path)],
            check=True,
            capture_output=True,
            text=True,
        )
        out = json.loads(output_path.read_text(encoding="utf-8"))
        assert out["final_action"] == "review"
        assert out["suggested_analysis_method"]["primary"] == "resolve_indirect_calls"

    print("suggested analysis method smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
