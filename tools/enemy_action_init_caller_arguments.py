#!/usr/bin/env python3
"""Plan and summarize bounded caller-argument subtrees for EnemyActions init."""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path

PCODE_REQUIRED = {
    "function_entry",
    "op_address",
    "op_time",
    "opcode",
    "inputs",
    "callee_entry",
    "callee_name",
}
PLAN_FIELDS = [
    "source_id",
    "source_entry",
    "source_call_index",
    "parameter_index",
    "dispatch_slot_offset",
    "expected_direct_caller_functions",
    "expected_matched_caller_calls",
]
SUBTREE_FORMAT = "NEVERGONE.GhidraCallerArgumentSubtree/1"
TARGET_NAME = "EnemyActionsSystem::initWithFile"


def split_inputs(cell: str) -> tuple[str, ...]:
    text = (cell or "").strip()
    if text in ("", "-"):
        return ()
    return tuple(token for token in text.split(";") if token and token != "-")


def read_dict_rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None:
            raise ValueError(f"{path}: missing TSV header")
        return [dict(row) for row in reader]


def build_plan(reverse_pcode: Path, output: Path) -> list[dict[str, object]]:
    with reverse_pcode.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        fields = set(reader.fieldnames or ())
        missing = sorted(PCODE_REQUIRED.difference(fields))
        if missing:
            raise ValueError(f"{reverse_pcode}: missing columns: {', '.join(missing)}")
        matches: list[dict[str, str]] = []
        for row in reader:
            if row.get("opcode", "").strip() != "CALL":
                continue
            name = row.get("callee_name", "").strip()
            if TARGET_NAME not in name:
                continue
            matches.append(dict(row))

    if not matches:
        raise ValueError(f"{reverse_pcode}: no resolved {TARGET_NAME} call")

    target_entries = {row["callee_entry"].strip() for row in matches}
    if "-" in target_entries or "" in target_entries or len(target_entries) != 1:
        raise ValueError(
            f"{reverse_pcode}: ambiguous {TARGET_NAME} target entries: "
            + ", ".join(sorted(target_entries))
        )
    target_entry = next(iter(target_entries))
    callers = {row["function_entry"].strip() for row in matches}
    if "" in callers or "-" in callers:
        raise ValueError(f"{reverse_pcode}: unresolved caller entry for {TARGET_NAME}")

    input_counts = {len(split_inputs(row.get("inputs", ""))) for row in matches}
    if len(input_counts) != 1:
        raise ValueError(
            f"{reverse_pcode}: inconsistent {TARGET_NAME} data-input counts: "
            + ", ".join(str(value) for value in sorted(input_counts))
        )
    data_inputs = next(iter(input_counts))
    if data_inputs < 1:
        raise ValueError(f"{reverse_pcode}: {TARGET_NAME} call has no receiver input")

    planned: list[dict[str, object]] = []
    for raw_index in range(data_inputs):
        label = "receiver" if raw_index == 0 else f"arg[{raw_index - 1}]"
        planned.append(
            {
                "source_id": f"enemy-actions-init:{label}",
                "source_entry": target_entry,
                "source_call_index": raw_index,
                "parameter_index": raw_index,
                "dispatch_slot_offset": "-",
                "expected_direct_caller_functions": len(callers),
                "expected_matched_caller_calls": len(matches),
            }
        )

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=PLAN_FIELDS, delimiter="\t")
        writer.writeheader()
        writer.writerows(planned)
    return planned


def _int(row: dict[str, str], key: str, default: int = -1) -> int:
    text = (row.get(key) or "").strip()
    if text in ("", "-"):
        return default
    return int(text, 10)


def _render_node(row: dict[str, str]) -> str:
    pieces = [f"token={row.get('node_token', '-')}" ]
    type_display = (row.get("type_display") or "-").strip()
    pointer_depth = (row.get("pointer_depth") or "-").strip()
    pointee = (row.get("pointee_type_display") or "-").strip()
    if type_display not in ("", "-"):
        pieces.append(f"type={type_display}")
    if pointer_depth not in ("", "-", "0"):
        pieces.append(f"pointer_depth={pointer_depth}")
    if pointee not in ("", "-"):
        pieces.append(f"pointee={pointee}")
    constant = (row.get("constant_value") or "-").strip()
    if constant not in ("", "-"):
        pieces.append(f"constant={constant}")
    opcode = (row.get("defining_opcode") or "-").strip()
    if opcode not in ("", "-"):
        pieces.append(f"def={opcode}")
    target = (row.get("call_target_name") or "-").strip()
    if target not in ("", "-"):
        pieces.append(f"call_target={target}")
    truncation = (row.get("truncation") or "-").strip()
    if truncation not in ("", "-"):
        pieces.append(f"truncation={truncation}")
    return " ".join(pieces)


def render_report(subtree_tsv: Path) -> tuple[str, bool]:
    rows = read_dict_rows(subtree_tsv)
    if not rows:
        raise ValueError(f"{subtree_tsv}: empty subtree export")
    if any((row.get("format") or "").strip() != SUBTREE_FORMAT for row in rows):
        raise ValueError(f"{subtree_tsv}: expected {SUBTREE_FORMAT}")

    by_source: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        by_source[(row.get("source_id") or "-").strip()].append(row)

    def source_sort_key(source_id: str) -> tuple[int, int]:
        if source_id.endswith(":receiver"):
            return (0, -1)
        if ":arg[" in source_id and source_id.endswith("]"):
            try:
                return (1, int(source_id.rsplit("[", 1)[1][:-1], 10))
            except ValueError:
                pass
        return (2, 0)

    lines = ["EnemyActions init caller argument subtree evidence"]
    all_complete = True
    for source_id in sorted(by_source, key=source_sort_key):
        group = by_source[source_id]
        summaries = [row for row in group if row.get("row_type") == "summary"]
        nodes = [row for row in group if row.get("row_type") == "node"]
        if len(summaries) != 1:
            raise ValueError(f"{subtree_tsv}: {source_id}: expected exactly one summary row")
        summary = summaries[0]
        status = (summary.get("status") or "-").strip()
        all_complete &= status == "complete_caller_argument_subtrees"
        lines.append(f"{source_id}:")
        lines.append(
            "  coverage: "
            f"status={status} "
            f"direct_callers={summary.get('direct_caller_functions', '-')} "
            f"matched_calls={summary.get('matched_caller_calls', '-')}"
        )

        roots = [row for row in nodes if _int(row, "depth") == 0]
        callers = sorted(
            {
                (
                    (row.get("caller_entry") or "-").strip(),
                    (row.get("caller_name") or "-").strip(),
                    (row.get("caller_call_site") or "-").strip(),
                )
                for row in roots
            }
        )
        for entry, name, site in callers:
            lines.append(f"  caller: {entry}:{name} call_site={site}")
        for root in roots:
            lines.append(f"  root: {_render_node(root)}")

        child_parents = {
            _int(row, "parent_node_id")
            for row in nodes
            if _int(row, "parent_node_id") >= 0
        }
        leaves = [
            row
            for row in nodes
            if _int(row, "node_id") not in child_parents and _int(row, "depth") > 0
        ]
        if leaves:
            lines.append("  leaves:")
            for leaf in sorted(
                leaves,
                key=lambda row: (
                    (row.get("caller_entry") or "").strip(),
                    _int(row, "depth"),
                    _int(row, "node_id"),
                ),
            )[:16]:
                lines.append(
                    f"    depth={_int(leaf, 'depth')} edge={leaf.get('edge_kind', '-')} "
                    + _render_node(leaf)
                )
            if len(leaves) > 16:
                lines.append(f"    ... {len(leaves) - 16} more leaves omitted")

    lines.append(
        "semantic status: structural caller argument provenance only; filename/object-type/ID meanings not assigned"
    )
    return "\n".join(lines) + "\n", all_complete


def cmd_plan(args: argparse.Namespace) -> int:
    try:
        planned = build_plan(args.reverse_pcode_tsv, args.output_tsv)
    except (OSError, ValueError) as error:
        raise SystemExit(f"error: {error}")
    print("EnemyActions init caller-argument plan")
    print(f"planned rows: {len(planned)}")
    for row in planned:
        print(
            f"{row['source_id']} target={row['source_entry']} "
            f"parameter_index={row['parameter_index']}"
        )
    print(f"output: {args.output_tsv}")
    return 0


def cmd_report(args: argparse.Namespace) -> int:
    try:
        report, complete = render_report(args.subtree_tsv)
    except (OSError, ValueError) as error:
        raise SystemExit(f"error: {error}")
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0 if complete else 2


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    plan = subparsers.add_parser("plan", help="build ExportCallerArgumentSubtree plan from reverse P-code")
    plan.add_argument("reverse_pcode_tsv", type=Path)
    plan.add_argument("output_tsv", type=Path)
    plan.set_defaults(func=cmd_plan)

    report = subparsers.add_parser("report", help="summarize ExportCallerArgumentSubtree output")
    report.add_argument("subtree_tsv", type=Path)
    report.add_argument("--output", type=Path)
    report.set_defaults(func=cmd_report)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
