#!/usr/bin/env python3
"""Summarize structural patterns in Nevergone CALLIND target expressions.

The input evidence comes from ExportIndirectCallEvidence.java. This tool does not
attempt to resolve semantic identities or alter routing. It attaches deterministic
shape summaries for target expressions such as nested loads, address arithmetic,
phi-derived values, and bounded-slice truncation.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.IndirectCallPatternEvidence/1"
OP_RE = re.compile(r"\b([A-Z][A-Z0-9_]*)\(")
CONST_RE = re.compile(r"const:0x([0-9a-fA-F]+):(?:\d+)")
ARITHMETIC_OPS = {"PTRADD", "PTRSUB", "INT_ADD", "INT_SUB"}
COPY_OPS = {"COPY", "CAST"}


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
    out: set[str] = set()
    for item in recommendations:
        if isinstance(item, dict) and isinstance(item.get("method"), str):
            out.add(item["method"])
    return out


def root_operator(expression: str) -> str:
    match = OP_RE.match(expression.strip())
    if match:
        return match.group(1)
    token = expression.strip().split(":", 1)[0]
    return token or "empty"


def operator_sequence(expression: str) -> list[str]:
    return OP_RE.findall(expression)


def nesting_depth(expression: str) -> int:
    depth = 0
    maximum = 0
    for char in expression:
        if char == "(":
            depth += 1
            maximum = max(maximum, depth)
        elif char == ")" and depth > 0:
            depth -= 1
    return maximum


def classify_expression(expression: str) -> str:
    expression = expression.strip()
    root = root_operator(expression)
    ops = operator_sequence(expression)
    nested = ops[1:] if len(ops) > 1 else []

    if root == "LOAD":
        if any(op in ARITHMETIC_OPS for op in nested):
            return "load_from_address_arithmetic"
        if "LOAD" in nested:
            return "load_from_loaded_base"
        if "MULTIEQUAL" in nested:
            return "load_from_phi_value"
        if any(op in COPY_OPS for op in nested):
            return "load_from_copied_value"
        if any(token in expression for token in ("register:", "input:", "address:", "unique:")):
            return "load_from_leaf_value"
        return "load_from_other_value"

    if root in COPY_OPS:
        return "copied_target"
    if root == "MULTIEQUAL":
        return "phi_target"
    if root in ARITHMETIC_OPS:
        return "address_arithmetic_target"
    if root == "const":
        return "constant_target"
    if root in {"register", "input", "address", "unique"}:
        return "leaf_target"
    return "other_target"


def small_constants(expression: str, maximum: int) -> list[int]:
    values: list[int] = []
    for token in CONST_RE.findall(expression):
        value = int(token, 16)
        if value <= maximum:
            values.append(value)
    return values


def summarize(path: Path, row: dict[str, Any], small_constant_max: int) -> dict[str, Any]:
    requested = "resolve_indirect_calls" in recommendation_methods(row)
    base: dict[str, Any] = {
        "format": FORMAT,
        "requested": requested,
        "present": path.is_file(),
    }
    if not path.is_file():
        return base

    evidence = read_tsv(path)
    patterns: Counter[str] = Counter()
    roots: Counter[str] = Counter()
    operators: Counter[str] = Counter()
    constants: Counter[str] = Counter()
    depths: list[int] = []
    depth_limited = 0
    cyclic = 0
    address_arithmetic = 0
    nested_load = 0

    for item in evidence:
        expression = item.get("target_expression", "")
        pattern = classify_expression(expression)
        ops = operator_sequence(expression)
        patterns[pattern] += 1
        roots[root_operator(expression)] += 1
        operators.update(ops)
        depths.append(nesting_depth(expression))
        depth_limited += int("{depth-limit}" in expression)
        cyclic += int("{cycle}" in expression)
        address_arithmetic += int(any(op in ARITHMETIC_OPS for op in ops))
        nested_load += int(ops.count("LOAD") >= 2)
        for value in small_constants(expression, small_constant_max):
            constants[f"0x{value:x}"] += 1

    count = len(evidence)
    base.update(
        {
            "call_count": count,
            "pattern_counts": dict(sorted(patterns.items())),
            "root_counts": dict(sorted(roots.items())),
            "operator_counts": dict(sorted(operators.items())),
            "address_arithmetic_expression_count": address_arithmetic,
            "nested_load_expression_count": nested_load,
            "depth_limited_expression_count": depth_limited,
            "depth_limited_fraction": round(depth_limited / count, 6) if count else 0.0,
            "cyclic_expression_count": cyclic,
            "nesting_depth_min": min(depths) if depths else None,
            "nesting_depth_max": max(depths) if depths else None,
            "nesting_depth_mean": round(sum(depths) / len(depths), 6) if depths else None,
            "small_constant_max": small_constant_max,
            "small_constant_counts": dict(sorted(constants.items(), key=lambda item: int(item[0], 16))),
        }
    )
    return base


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("evidence_root", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--small-constant-max", type=lambda value: int(value, 0), default=0x1000)
    args = parser.parse_args()
    if args.small_constant_max < 0:
        raise SystemExit("--small-constant-max must be non-negative")

    rows = read_jsonl(args.analysis)
    if not rows:
        raise SystemExit("analysis input is empty")

    indirect_dir = args.evidence_root / "indirect-calls"
    output_rows: list[dict[str, Any]] = []
    present = 0
    missing_requested = 0
    aggregate_patterns: Counter[str] = Counter()
    aggregate_operators: Counter[str] = Counter()

    for source in rows:
        record_id = source.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("analysis row missing id")
        before_action = source.get("final_action")
        evidence_path = indirect_dir / (safe_id(record_id) + ".tsv")
        pattern_evidence = summarize(evidence_path, source, args.small_constant_max)

        row = dict(source)
        row["indirect_call_pattern_evidence"] = pattern_evidence
        if row.get("final_action") != before_action:
            raise AssertionError(f"{record_id}: final_action changed")

        present += int(pattern_evidence["present"])
        missing_requested += int(pattern_evidence["requested"] and not pattern_evidence["present"])
        aggregate_patterns.update(pattern_evidence.get("pattern_counts", {}))
        aggregate_operators.update(pattern_evidence.get("operator_counts", {}))
        output_rows.append(row)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in output_rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    print("format:", FORMAT)
    print("functions:", len(output_rows))
    print("indirect pattern evidence present:", present)
    print("missing requested:", missing_requested)
    print("patterns:", dict(aggregate_patterns))
    print("operators:", dict(aggregate_operators))
    print("final_action unchanged: yes")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
