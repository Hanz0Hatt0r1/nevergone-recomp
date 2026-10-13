#!/usr/bin/env python3
"""Attach deterministic next-analysis recommendations to Nevergone function evidence.

This layer is descriptive workflow evidence only. It does not use project goals,
function names, semantic labels, or ``final_action`` to choose a method, and it
never changes routing. Recommendations are derived from portable structural
metrics, Ghidra call-graph metrics, and label-free similarity strength.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.SuggestedAnalysisMethod/1"


def _read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            rows.append(row)
    return rows


def _number(mapping: dict[str, Any], key: str, default: float = 0.0) -> float:
    value = mapping.get(key, default)
    if isinstance(value, bool):
        return float(value)
    if isinstance(value, (int, float)):
        return float(value)
    return float(default)


def _add(
    recommendations: list[dict[str, Any]],
    method: str,
    score: int,
    *reasons: str,
) -> None:
    recommendations.append({
        "method": method,
        "score": int(score),
        "reasons": [reason for reason in reasons if reason],
    })


def recommend_analysis_method(row: dict[str, Any]) -> dict[str, Any]:
    structural = row.get("structural_metrics")
    callgraph = row.get("callgraph_metrics")
    similarity = row.get("similarity_transfer")

    if not isinstance(structural, dict):
        raise ValueError("row missing structural_metrics")
    if not isinstance(callgraph, dict):
        raise ValueError("row missing callgraph_metrics")
    if not isinstance(similarity, dict):
        similarity = {}

    recommendations: list[dict[str, Any]] = []

    op_count = int(_number(structural, "op_count"))
    definition_coverage = _number(structural, "definition_coverage_proxy", 1.0)
    cyclomatic = int(_number(structural, "cyclomatic_proxy"))
    memory_ratio = _number(structural, "memory_op_ratio")
    indirect_ratio = _number(structural, "indirect_call_ratio")

    fan_in = int(_number(callgraph, "fan_in"))
    fan_out = int(_number(callgraph, "fan_out"))
    corpus_calls = int(_number(callgraph, "corpus_call_count"))
    unknown_ratio = _number(callgraph, "unknown_call_ratio_proxy")

    sim_value = _number(similarity, "similarity")
    sim_gap = _number(similarity, "gap")
    if sim_value == 0.0:
        sim_value = _number(similarity, "top_similarity")
    if sim_gap == 0.0:
        sim_gap = _number(similarity, "top_gap")

    # The portable corpus currently caps emitted P-code at 256 ops. Reaching
    # that ceiling, especially with omitted definitions, is a direct signal to
    # inspect a larger/full function export before drawing semantic conclusions.
    if op_count >= 256:
        score = 100 if definition_coverage < 0.98 else 92
        _add(
            recommendations,
            "expand_pcode",
            score,
            f"portable op_count reached corpus ceiling ({op_count})",
            (
                f"definition_coverage_proxy={definition_coverage:.3f}"
                if definition_coverage < 1.0 else ""
            ),
        )
    elif definition_coverage < 0.95:
        _add(
            recommendations,
            "expand_pcode",
            88,
            f"definition_coverage_proxy={definition_coverage:.3f}",
        )

    # Missing resolved call edges are most actionable when the function really
    # contains calls. Indirect-heavy functions receive an additional reason but
    # the recommendation does not assume every unresolved call is indirect.
    if corpus_calls > 0 and unknown_ratio >= 0.75:
        _add(
            recommendations,
            "resolve_indirect_calls",
            96,
            f"unknown_call_ratio_proxy={unknown_ratio:.3f}",
            f"corpus_call_count={corpus_calls}",
            (
                f"portable indirect_call_ratio={indirect_ratio:.3f}"
                if indirect_ratio > 0 else ""
            ),
        )
    elif corpus_calls >= 2 and unknown_ratio >= 0.40:
        _add(
            recommendations,
            "resolve_indirect_calls",
            84,
            f"unknown_call_ratio_proxy={unknown_ratio:.3f}",
            f"corpus_call_count={corpus_calls}",
        )
    elif indirect_ratio >= 0.75 and corpus_calls > 0:
        _add(
            recommendations,
            "resolve_indirect_calls",
            80,
            f"portable indirect_call_ratio={indirect_ratio:.3f}",
        )

    if fan_in >= 50:
        _add(recommendations, "inspect_callers", 94, f"fan_in={fan_in}")
    elif fan_in >= 20:
        _add(recommendations, "inspect_callers", 84, f"fan_in={fan_in}")
    elif fan_in >= 10:
        _add(recommendations, "inspect_callers", 72, f"fan_in={fan_in}")

    if fan_out >= 30:
        _add(recommendations, "inspect_callees", 92, f"fan_out={fan_out}")
    elif fan_out >= 15:
        _add(recommendations, "inspect_callees", 82, f"fan_out={fan_out}")
    elif fan_out >= 8:
        _add(recommendations, "inspect_callees", 68, f"fan_out={fan_out}")

    if cyclomatic >= 25:
        _add(
            recommendations,
            "control_flow_review",
            90,
            f"cyclomatic_proxy={cyclomatic}",
        )
    elif cyclomatic >= 15:
        _add(
            recommendations,
            "control_flow_review",
            78,
            f"cyclomatic_proxy={cyclomatic}",
        )
    elif cyclomatic >= 8:
        _add(
            recommendations,
            "control_flow_review",
            64,
            f"cyclomatic_proxy={cyclomatic}",
        )

    # Similarity labels themselves are not used here. Only strength and
    # separation determine whether nearest-family comparison is likely useful.
    if sim_value >= 0.95 and sim_gap >= 0.10:
        _add(
            recommendations,
            "compare_nearest_family",
            76,
            f"similarity={sim_value:.3f}",
            f"gap={sim_gap:.3f}",
        )
    elif sim_value >= 0.90 and sim_gap >= 0.05:
        _add(
            recommendations,
            "compare_nearest_family",
            66,
            f"similarity={sim_value:.3f}",
            f"gap={sim_gap:.3f}",
        )

    if memory_ratio >= 0.30 and op_count >= 20:
        _add(
            recommendations,
            "inspect_memory_dataflow",
            58,
            f"memory_op_ratio={memory_ratio:.3f}",
        )

    if not recommendations:
        _add(
            recommendations,
            "standard_decompilation_review",
            20,
            "no stronger deterministic analysis trigger",
        )

    recommendations.sort(key=lambda item: (-item["score"], item["method"]))
    return {
        "format": FORMAT,
        "primary": recommendations[0]["method"],
        "recommendations": recommendations,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    rows = _read_jsonl(args.analysis)
    if not rows:
        raise SystemExit("input is empty")

    before_actions = [row.get("final_action") for row in rows]
    counts: dict[str, int] = {}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in rows:
            out = dict(row)
            suggestion = recommend_analysis_method(row)
            out["suggested_analysis_method"] = suggestion
            primary = suggestion["primary"]
            counts[primary] = counts.get(primary, 0) + 1
            handle.write(json.dumps(out, sort_keys=True) + "\n")

    written = _read_jsonl(args.output)
    after_actions = [row.get("final_action") for row in written]
    if before_actions != after_actions:
        args.output.unlink(missing_ok=True)
        raise AssertionError("suggested analysis method changed final_action")

    print("format:", FORMAT)
    print("functions:", len(written))
    print("primary methods:", dict(sorted(counts.items())))
    print("final_action unchanged: yes")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
