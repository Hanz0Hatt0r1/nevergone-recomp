#!/usr/bin/env python3
"""Extract deterministic structural metrics from portable Nevergone P-code.

The output is descriptive evidence only. Function names, addresses, project goals,
and learned labels are not used to compute metrics.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

from train_nevergone_production_classifier import normalize_portable

FORMAT = "NEVERGONE.StructuralMetrics/1"

COMPARE_OPS = {
    "INT_EQUAL", "INT_NOTEQUAL", "INT_LESS", "INT_LESSEQUAL",
    "INT_SLESS", "INT_SLESSEQUAL", "FLOAT_EQUAL", "FLOAT_NOTEQUAL",
    "FLOAT_LESS", "FLOAT_LESSEQUAL", "FLOAT_NAN",
}
ARITHMETIC_OPS = {
    "INT_ADD", "INT_SUB", "INT_MULT", "INT_DIV", "INT_SDIV", "INT_REM", "INT_SREM",
    "FLOAT_ADD", "FLOAT_SUB", "FLOAT_MULT", "FLOAT_DIV", "FLOAT_NEG", "FLOAT_ABS",
    "FLOAT_SQRT", "FLOAT_CEIL", "FLOAT_FLOOR", "FLOAT_ROUND",
}
BITWISE_OPS = {
    "INT_AND", "INT_OR", "INT_XOR", "INT_LEFT", "INT_RIGHT", "INT_SRIGHT",
    "BOOL_AND", "BOOL_OR", "BOOL_XOR", "BOOL_NEGATE",
}
CONVERSION_OPS = {
    "CAST", "INT_ZEXT", "INT_SEXT", "INT_2COMP", "INT_NEGATE",
    "FLOAT_INT2FLOAT", "FLOAT_FLOAT2FLOAT", "FLOAT_TRUNC",
}
POINTER_OPS = {"PTRADD", "PTRSUB"}
MEMORY_OPS = {"LOAD", "STORE"}
DIRECT_CALL_OPS = {"CALL"}
INDIRECT_CALL_OPS = {"CALLIND"}
CALL_OPS = DIRECT_CALL_OPS | INDIRECT_CALL_OPS
DIRECT_BRANCH_OPS = {"BRANCH"}
CONDITIONAL_BRANCH_OPS = {"CBRANCH"}
INDIRECT_BRANCH_OPS = {"BRANCHIND"}
BRANCH_OPS = DIRECT_BRANCH_OPS | CONDITIONAL_BRANCH_OPS | INDIRECT_BRANCH_OPS
RETURN_OPS = {"RETURN"}
CONTROL_FLOW_OPS = CALL_OPS | BRANCH_OPS | RETURN_OPS
SSA_MERGE_OPS = {"MULTIEQUAL"}
COPY_OPS = {"COPY"}
SLICE_OPS = {"PIECE", "SUBPIECE"}


def _read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def _iter_input(path: Path) -> Iterable[dict[str, Any]]:
    if path.is_dir():
        found = False
        for split in ("train", "validation", "test"):
            candidate = path / f"{split}.jsonl"
            if not candidate.exists():
                continue
            found = True
            yield from _read_jsonl(candidate)
        if not found:
            raise ValueError(f"{path}: no train/validation/test JSONL files")
        return
    yield from _read_jsonl(path)


def _ratio(numerator: int | float, denominator: int | float) -> float:
    return float(numerator) / float(denominator) if denominator else 0.0


def _portable_lines(text: str) -> list[str]:
    portable = normalize_portable(text)
    return [line.strip() for line in portable.splitlines() if line.strip()]


def _opcode(line: str) -> str:
    parts = line.split()
    if len(parts) < 2 or parts[0] != "OP":
        return "UNKNOWN"
    return parts[1]


def structural_metrics(text: str) -> dict[str, Any]:
    lines = _portable_lines(text)
    opcodes = [_opcode(line) for line in lines]
    counts = Counter(opcodes)
    op_count = len(opcodes)

    load_count = counts["LOAD"]
    store_count = counts["STORE"]
    memory_count = sum(counts[op] for op in MEMORY_OPS)
    direct_call_count = sum(counts[op] for op in DIRECT_CALL_OPS)
    indirect_call_count = sum(counts[op] for op in INDIRECT_CALL_OPS)
    call_count = direct_call_count + indirect_call_count
    direct_branch_count = sum(counts[op] for op in DIRECT_BRANCH_OPS)
    conditional_branch_count = sum(counts[op] for op in CONDITIONAL_BRANCH_OPS)
    indirect_branch_count = sum(counts[op] for op in INDIRECT_BRANCH_OPS)
    branch_count = direct_branch_count + conditional_branch_count + indirect_branch_count
    return_count = sum(counts[op] for op in RETURN_OPS)
    control_flow_count = sum(counts[op] for op in CONTROL_FLOW_OPS)

    compare_count = sum(counts[op] for op in COMPARE_OPS)
    arithmetic_count = sum(counts[op] for op in ARITHMETIC_OPS)
    bitwise_count = sum(counts[op] for op in BITWISE_OPS)
    conversion_count = sum(counts[op] for op in CONVERSION_OPS)
    pointer_count = sum(counts[op] for op in POINTER_OPS)
    phi_count = sum(counts[op] for op in SSA_MERGE_OPS)
    copy_count = sum(counts[op] for op in COPY_OPS)
    slice_count = sum(counts[op] for op in SLICE_OPS)

    omitted_def_operand_count = sum(line.count("def:omitted") for line in lines)
    omitted_def_line_count = sum("def:omitted" in line for line in lines)
    unknown_opcode_count = counts["UNKNOWN"]

    # A portable, conservative decision-point proxy. It intentionally avoids
    # claiming exact CFG cyclomatic complexity because branch targets are removed.
    decision_point_count = conditional_branch_count + indirect_branch_count
    cyclomatic_proxy = 1 + decision_point_count if op_count else 0

    return {
        "op_count": op_count,
        "unique_opcode_count": len(set(opcodes)),
        "unknown_opcode_count": unknown_opcode_count,
        "load_count": load_count,
        "store_count": store_count,
        "memory_op_count": memory_count,
        "memory_op_ratio": _ratio(memory_count, op_count),
        "direct_call_count": direct_call_count,
        "indirect_call_count": indirect_call_count,
        "call_count": call_count,
        "call_density": _ratio(call_count, op_count),
        "indirect_call_ratio": _ratio(indirect_call_count, call_count),
        "direct_branch_count": direct_branch_count,
        "conditional_branch_count": conditional_branch_count,
        "indirect_branch_count": indirect_branch_count,
        "branch_count": branch_count,
        "branch_density": _ratio(branch_count, op_count),
        "return_count": return_count,
        "control_flow_op_count": control_flow_count,
        "control_flow_density": _ratio(control_flow_count, op_count),
        "decision_point_count": decision_point_count,
        "cyclomatic_proxy": cyclomatic_proxy,
        "compare_count": compare_count,
        "arithmetic_count": arithmetic_count,
        "bitwise_count": bitwise_count,
        "conversion_count": conversion_count,
        "pointer_op_count": pointer_count,
        "phi_count": phi_count,
        "copy_count": copy_count,
        "slice_op_count": slice_count,
        "omitted_def_operand_count": omitted_def_operand_count,
        "omitted_def_line_count": omitted_def_line_count,
        "omitted_def_line_ratio": _ratio(omitted_def_line_count, op_count),
        "definition_coverage_proxy": 1.0 - _ratio(omitted_def_line_count, op_count) if op_count else 0.0,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="JSONL corpus/dataset or dataset directory")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    rows = list(_iter_input(args.input))
    if not rows:
        raise SystemExit("input is empty")

    seen: set[str] = set()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in rows:
            record_id = row.get("id")
            text = row.get("text")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError("row missing id")
            if record_id in seen:
                raise ValueError(f"duplicate id: {record_id}")
            if not isinstance(text, str) or not text:
                raise ValueError(f"row {record_id} missing text")
            output = {
                "format": FORMAT,
                "id": record_id,
                "metrics": structural_metrics(text),
            }
            handle.write(json.dumps(output, sort_keys=True) + "\n")
            seen.add(record_id)

    print("format:", FORMAT)
    print("functions:", len(seen))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
