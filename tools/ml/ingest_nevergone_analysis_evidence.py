#!/usr/bin/env python3
"""Attach local expanded-P-code and indirect-call evidence to Nevergone analysis rows.

The evidence is diagnostic only. This tool copies every input row, adds deterministic
summaries, and verifies that ``final_action`` is unchanged. Raw Ghidra TSV files stay
local and are never embedded wholesale into the output JSONL.
"""
from __future__ import annotations

import argparse
import csv
import json
import statistics
from collections import Counter
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.AnalysisEvidenceIngest/1"
EXPANDED_FORMAT = "NEVERGONE.ExpandedPcodeEvidence/1"
INDIRECT_FORMAT = "NEVERGONE.IndirectCallEvidence/1"
CALL_OPS = {"CALL", "CALLIND"}
BRANCH_OPS = {"BRANCH", "CBRANCH", "BRANCHIND"}


def read_jsonl(path: Path) -> list[dict[str, Any]]:
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


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def safe_id(record_id: str) -> str:
    return record_id.replace(":", "_").replace("/", "_")


def recommendation_methods(row: dict[str, Any]) -> set[str]:
    suggestion = row.get("suggested_analysis_method")
    if not isinstance(suggestion, dict):
        return set()
    recommendations = suggestion.get("recommendations")
    if not isinstance(recommendations, list):
        return set()
    methods: set[str] = set()
    for item in recommendations:
        if isinstance(item, dict) and isinstance(item.get("method"), str):
            methods.add(item["method"])
    return methods


def expression_root(expression: str) -> str:
    expression = expression.strip()
    if not expression:
        return "empty"
    if "(" in expression:
        return expression.split("(", 1)[0]
    return expression.split(":", 1)[0]


def expression_class(expression: str, target: str) -> str:
    root = expression_root(expression)
    if root == "LOAD":
        return "load_target"
    if root == "MULTIEQUAL":
        return "phi_target"
    if root in {"PTRADD", "PTRSUB", "INT_ADD", "INT_SUB"}:
        return "pointer_arithmetic_target"
    if root in {"COPY", "CAST"}:
        return "copied_target"
    if root == "INDIRECT":
        return "indirect_dataflow_target"
    target_kind = target.split(":", 1)[0] if target else ""
    if target_kind in {"register", "input", "address", "unique", "const"}:
        return "leaf_target"
    return "other_dataflow_target"


def summarize_expanded(path: Path, row: dict[str, Any], requested: bool) -> dict[str, Any]:
    base: dict[str, Any] = {
        "format": EXPANDED_FORMAT,
        "requested": requested,
        "present": path.is_file(),
    }
    if not path.is_file():
        return base

    evidence = read_tsv(path)
    opcode_counts = Counter(item.get("opcode", "") for item in evidence if item.get("opcode"))
    call_rows = [item for item in evidence if item.get("opcode") in CALL_OPS]
    direct_calls = [item for item in call_rows if item.get("opcode") == "CALL"]
    indirect_calls = [item for item in call_rows if item.get("opcode") == "CALLIND"]
    resolved_direct = [item for item in direct_calls if item.get("callee_entry") not in {None, "", "-"}]
    unique_callees = sorted({item["callee_entry"] for item in resolved_direct})

    structural = row.get("structural_metrics") if isinstance(row.get("structural_metrics"), dict) else {}
    portable_ops = structural.get("op_count")
    portable_coverage = structural.get("definition_coverage_proxy")
    full_ops = len(evidence)

    base.update(
        {
            "function_entries": sorted({item.get("function_entry", "") for item in evidence if item.get("function_entry")}),
            "op_count": full_ops,
            "unique_opcode_count": len(opcode_counts),
            "opcode_counts": dict(sorted(opcode_counts.items())),
            "call_count": len(call_rows),
            "direct_call_count": len(direct_calls),
            "indirect_call_count": len(indirect_calls),
            "resolved_direct_call_count": len(resolved_direct),
            "unresolved_direct_call_count": len(direct_calls) - len(resolved_direct),
            "unique_resolved_callee_count": len(unique_callees),
            "load_count": opcode_counts.get("LOAD", 0),
            "store_count": opcode_counts.get("STORE", 0),
            "branch_count": sum(opcode_counts.get(name, 0) for name in BRANCH_OPS),
            "decision_point_count": opcode_counts.get("CBRANCH", 0),
            "portable_op_count": portable_ops,
            "portable_definition_coverage_proxy": portable_coverage,
            "portable_was_capped": isinstance(portable_ops, int) and portable_ops >= 256,
            "expansion_delta_ops": full_ops - portable_ops if isinstance(portable_ops, int) else None,
            "expansion_factor": round(full_ops / portable_ops, 6) if isinstance(portable_ops, int) and portable_ops > 0 else None,
        }
    )
    return base


def summarize_indirect(path: Path, row: dict[str, Any], requested: bool) -> dict[str, Any]:
    base: dict[str, Any] = {
        "format": INDIRECT_FORMAT,
        "requested": requested,
        "present": path.is_file(),
    }
    if not path.is_file():
        return base

    evidence = read_tsv(path)
    roots: Counter[str] = Counter()
    classes: Counter[str] = Counter()
    target_kinds: Counter[str] = Counter()
    arg_counts: list[int] = []
    expressions: set[str] = set()
    depth_limited = 0
    cyclic = 0

    for item in evidence:
        expr = item.get("target_expression", "")
        target = item.get("target", "")
        roots[expression_root(expr)] += 1
        classes[expression_class(expr, target)] += 1
        target_kinds[target.split(":", 1)[0] if target else "empty"] += 1
        expressions.add(expr)
        depth_limited += int("{depth-limit}" in expr)
        cyclic += int("{cycle}" in expr)
        try:
            arg_counts.append(int(item.get("arg_count", "0")))
        except ValueError:
            pass

    structural = row.get("structural_metrics") if isinstance(row.get("structural_metrics"), dict) else {}
    portable_indirect = structural.get("indirect_call_count")
    count = len(evidence)
    base.update(
        {
            "call_count": count,
            "unique_target_expression_count": len(expressions),
            "expression_root_counts": dict(sorted(roots.items())),
            "expression_class_counts": dict(sorted(classes.items())),
            "target_kind_counts": dict(sorted(target_kinds.items())),
            "depth_limited_expression_count": depth_limited,
            "cyclic_expression_count": cyclic,
            "arg_count_min": min(arg_counts) if arg_counts else None,
            "arg_count_median": statistics.median(arg_counts) if arg_counts else None,
            "arg_count_max": max(arg_counts) if arg_counts else None,
            "portable_indirect_call_count": portable_indirect,
            "evidence_delta_vs_portable": count - portable_indirect if isinstance(portable_indirect, int) else None,
        }
    )
    return base


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("evidence_root", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    rows = read_jsonl(args.analysis)
    if not rows:
        raise SystemExit("analysis input is empty")

    expanded_dir = args.evidence_root / "expanded-pcode"
    indirect_dir = args.evidence_root / "indirect-calls"
    output_rows: list[dict[str, Any]] = []
    expanded_present = 0
    indirect_present = 0
    expanded_missing_requested = 0
    indirect_missing_requested = 0

    for source in rows:
        record_id = source.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("analysis row missing id")
        before_action = source.get("final_action")
        methods = recommendation_methods(source)
        stem = safe_id(record_id) + ".tsv"

        row = dict(source)
        expanded = summarize_expanded(expanded_dir / stem, source, "expand_pcode" in methods)
        indirect = summarize_indirect(indirect_dir / stem, source, "resolve_indirect_calls" in methods)
        row["expanded_pcode_evidence"] = expanded
        row["indirect_call_evidence"] = indirect

        if row.get("final_action") != before_action:
            raise AssertionError(f"{record_id}: final_action changed during evidence ingest")

        expanded_present += int(expanded["present"])
        indirect_present += int(indirect["present"])
        expanded_missing_requested += int(expanded["requested"] and not expanded["present"])
        indirect_missing_requested += int(indirect["requested"] and not indirect["present"])
        output_rows.append(row)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in output_rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    print("format:", FORMAT)
    print("functions:", len(output_rows))
    print("expanded pcode present:", expanded_present)
    print("expanded pcode missing requested:", expanded_missing_requested)
    print("indirect evidence present:", indirect_present)
    print("indirect evidence missing requested:", indirect_missing_requested)
    print("final_action unchanged: yes")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
