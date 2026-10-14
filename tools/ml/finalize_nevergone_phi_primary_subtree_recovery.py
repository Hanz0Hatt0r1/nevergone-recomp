#!/usr/bin/env python3
"""Resolve v7 PHI rows whose only blocker was shallow primary-branch typing.

This fail-closed v8 post-finalizer considers only v7 rows blocked solely by
`primary_branch_class_not_unique`. It traverses the bounded structural-receiver
primary PHI subtree already exported by Ghidra, requires all catalog-mappable
one-level pointer evidence in that subtree to converge to exactly one class,
requires that class to equal the independently proven protobuf default-instance
owner from v7, and requires a unique catalog vtable. No sibling, target, or
address-proximity class inference is introduced.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
DETAIL_FORMAT = "NEVERGONE.PhiDefaultInstanceRecovery/1"
SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
RECOVERY_FORMAT = "NEVERGONE.PhiPrimarySubtreeRecovery/1"
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match"}


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            rows.append(row)
    return rows


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def parse_decimal(value: Any, default: int = -1) -> int:
    if value in (None, "", "-"):
        return default
    text = str(value).strip()
    if not re.fullmatch(r"-?[0-9]+", text):
        raise ValueError(f"expected decimal integer, got {value!r}")
    return int(text, 10)


def normalize_slot(value: Any) -> str | None:
    if value in (None, "", "-"):
        return None
    text = str(value).strip()
    try:
        parsed = int(text, 0)
    except ValueError:
        if re.fullmatch(r"[0-9a-fA-F]+", text):
            parsed = int(text, 16)
        else:
            return None
    return f"0x{parsed:x}" if parsed >= 0 else None


def clean_type(value: Any) -> str | None:
    if not isinstance(value, str):
        return None
    text = value.strip()
    if not text or text == "-":
        return None
    text = re.sub(r"^(?:class|struct)\s+", "", text)
    text = re.sub(r"\s+", " ", text).strip()
    while text.endswith(" *"):
        text = text[:-2].rstrip()
    return text or None


def terminal(name: str) -> str:
    return name.rsplit("::", 1)[-1]


def map_type(value: Any, catalog: set[str], by_terminal: dict[str, set[str]]) -> set[str]:
    cleaned = clean_type(value)
    if cleaned is None:
        return set()
    if cleaned in catalog:
        return {cleaned}
    matches = by_terminal.get(terminal(cleaned), set())
    return set(matches) if len(matches) == 1 else set()


def source_entry(source_id: str) -> str:
    return source_id.split(":", 1)[1] if ":" in source_id else source_id


def key_of(row: dict[str, Any], where: str) -> tuple[str, int]:
    source_id = row.get("source_id")
    call_index = row.get("call_index")
    if not isinstance(source_id, str) or not source_id or not isinstance(call_index, int) or call_index < 0:
        raise ValueError(f"{where}: invalid source_id/call_index")
    return source_id, call_index


def load_slices(slice_dir: Path) -> dict[tuple[str, int], dict[str, Any]]:
    result: dict[tuple[str, int], dict[str, Any]] = {}
    for path in sorted(slice_dir.glob("*.tsv")):
        rows = read_tsv(path)
        for line_no, row in enumerate(rows, 2):
            if row.get("format") != SLICE_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {SLICE_FORMAT}")
        grouped: dict[tuple[str, int], list[dict[str, str]]] = defaultdict(list)
        for row in rows:
            entry = row.get("function_entry") or ""
            call_index = parse_decimal(row.get("call_index"), -1)
            if entry and call_index >= 0:
                grouped[(entry, call_index)].append(row)
        for key, call_rows in grouped.items():
            summary = next((r for r in call_rows if r.get("row_type") == "summary"), None)
            nodes = [r for r in call_rows if r.get("row_type") == "node"]
            if summary is None or not nodes:
                continue
            if key in result:
                raise ValueError(f"duplicate structural slice call: {key}")
            result[key] = {"summary": summary, "nodes": nodes, "path": str(path)}
    return result


def subtree_node_ids(nodes: list[dict[str, str]], root_id: int) -> set[int]:
    children: dict[int, list[int]] = defaultdict(list)
    for row in nodes:
        node_id = parse_decimal(row.get("node_id"), -1)
        parent = parse_decimal(row.get("parent_node_id"), -1)
        if node_id >= 0 and parent >= 0:
            children[parent].append(node_id)
    seen: set[int] = set()
    stack = [root_id]
    while stack:
        node_id = stack.pop()
        if node_id in seen:
            continue
        seen.add(node_id)
        stack.extend(children.get(node_id, []))
    return seen


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("v7_inference_jsonl", type=Path)
    parser.add_argument("v7_queries_jsonl", type=Path)
    parser.add_argument("v7_recovery_details_jsonl", type=Path)
    parser.add_argument("slice_dir", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--queries-output", type=Path, required=True)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    inference = read_jsonl(args.v7_inference_jsonl)
    queries = read_jsonl(args.v7_queries_jsonl)
    detail_rows = read_jsonl(args.v7_recovery_details_jsonl)
    catalog_rows = read_jsonl(args.catalog_jsonl)

    class_vtables: dict[str, set[str]] = defaultdict(set)
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
        class_name = row.get("class_name")
        vtable = row.get("receiver_vtable_id")
        if isinstance(class_name, str) and class_name and isinstance(vtable, str) and vtable:
            class_vtables[class_name].add(vtable)
    catalog = set(class_vtables)
    by_terminal: dict[str, set[str]] = defaultdict(set)
    for class_name in catalog:
        by_terminal[terminal(class_name)].add(class_name)

    v7_details: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(detail_rows, 1):
        if row.get("format") != DETAIL_FORMAT:
            raise ValueError(f"{args.v7_recovery_details_jsonl}:{line_no}: expected {DETAIL_FORMAT}")
        key = key_of(row, f"{args.v7_recovery_details_jsonl}:{line_no}")
        if key in v7_details:
            raise ValueError(f"duplicate v7 recovery detail: {key}")
        v7_details[key] = row

    slices = load_slices(args.slice_dir)

    query_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(queries, 1):
        if row.get("format") != QUERY_FORMAT:
            raise ValueError(f"{args.v7_queries_jsonl}:{line_no}: expected {QUERY_FORMAT}")
        key = key_of(row, f"{args.v7_queries_jsonl}:{line_no}")
        if key in query_by_key:
            raise ValueError(f"duplicate query: {key}")
        query_by_key[key] = dict(row)

    output: list[dict[str, Any]] = []
    details: list[dict[str, Any]] = []
    counts: Counter[str] = Counter()

    for line_no, original in enumerate(inference, 1):
        if original.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"{args.v7_inference_jsonl}:{line_no}: expected {INFERENCE_FORMAT}")
        row = dict(original)
        key = key_of(row, f"{args.v7_inference_jsonl}:{line_no}")
        v7 = v7_details.get(key)

        if row.get("status") != "receiver_not_single_pointer" or v7 is None:
            output.append(row)
            continue
        reasons = v7.get("reasons")
        if reasons != ["primary_branch_class_not_unique"]:
            output.append(row)
            continue
        owner_classes = v7.get("owner_classes")
        owner_xrefs = v7.get("owner_direct_method_xrefs")
        if not isinstance(owner_classes, list) or len(owner_classes) != 1 or not isinstance(owner_classes[0], str):
            output.append(row)
            continue
        if not isinstance(owner_xrefs, int) or owner_xrefs <= 0:
            output.append(row)
            continue

        counts["eligible"] += 1
        recovery: dict[str, Any] = {
            "format": RECOVERY_FORMAT,
            "source_id": key[0],
            "call_index": key[1],
            "accepted": False,
            "owner_class": owner_classes[0],
            "owner_direct_method_xrefs": owner_xrefs,
            "primary_subtree_classes": [],
            "primary_subtree_typed_nodes": [],
            "reasons": [],
            "affects_routing": False,
        }
        block_reasons: list[str] = recovery["reasons"]

        slice_info = slices.get((source_entry(key[0]), key[1]))
        if slice_info is None:
            block_reasons.append("missing_structural_slice")
        else:
            summary = slice_info["summary"]
            nodes: list[dict[str, str]] = slice_info["nodes"]
            if summary.get("recovery_status") != "canonical_receiver":
                block_reasons.append("slice_not_canonical_receiver")
            if normalize_slot(summary.get("slot_offset")) != normalize_slot(row.get("dispatch_slot_offset")):
                block_reasons.append("slice_slot_mismatch")

            root = next((n for n in nodes if parse_decimal(n.get("node_id"), -1) == 0), None)
            if root is None or root.get("defining_opcode") != "MULTIEQUAL":
                block_reasons.append("root_not_multiequal")
            else:
                primary = next(
                    (
                        n for n in nodes
                        if parse_decimal(n.get("parent_node_id"), -1) == 0
                        and parse_decimal(n.get("input_index"), -1) == 0
                    ),
                    None,
                )
                if primary is None:
                    block_reasons.append("primary_branch_missing")
                else:
                    primary_id = parse_decimal(primary.get("node_id"), -1)
                    ids = subtree_node_ids(nodes, primary_id)
                    classes: set[str] = set()
                    typed_nodes: list[dict[str, Any]] = []
                    for node in nodes:
                        node_id = parse_decimal(node.get("node_id"), -1)
                        if node_id not in ids:
                            continue
                        pointer_depth = parse_decimal(node.get("pointer_depth"), 0)
                        if pointer_depth != 1:
                            continue
                        mapped = map_type(node.get("pointee_type_display"), catalog, by_terminal)
                        if not mapped:
                            continue
                        classes.update(mapped)
                        typed_nodes.append({
                            "node_id": node_id,
                            "opcode": node.get("defining_opcode"),
                            "type_display": node.get("type_display"),
                            "pointee_type_display": node.get("pointee_type_display"),
                            "mapped_classes": sorted(mapped),
                        })
                    recovery["primary_subtree_classes"] = sorted(classes)
                    recovery["primary_subtree_typed_nodes"] = typed_nodes
                    if len(classes) != 1:
                        block_reasons.append("primary_subtree_class_not_unique")
                    else:
                        primary_class = next(iter(classes))
                        if primary_class != owner_classes[0]:
                            block_reasons.append("primary_subtree_owner_mismatch")
                        vtables = sorted(class_vtables.get(primary_class, set()))
                        recovery["candidate_vtables"] = vtables
                        if len(vtables) != 1:
                            block_reasons.append("primary_subtree_class_vtable_not_unique")
                        elif not block_reasons:
                            vtable = vtables[0]
                            recovery["accepted"] = True
                            counts["accepted"] += 1
                            row["class_name"] = primary_class
                            row["class_match_kind"] = "phi_primary_subtree_exact_catalog_class_name"
                            row["class_evidence_source"] = "structural_phi_primary_subtree_default_instance_owner"
                            row["candidate_receiver_class_names"] = [primary_class]
                            row["candidate_receiver_vtable_ids"] = [vtable]
                            row["receiver_vtable_id"] = vtable
                            row["status"] = "resolved_unique_vtable"
                            row["phi_primary_subtree_recovery"] = recovery

                            if key in query_by_key:
                                raise ValueError(f"query already exists for newly recovered row: {key}")
                            query_by_key[key] = {
                                "format": QUERY_FORMAT,
                                "source_id": key[0],
                                "call_index": key[1],
                                "call_address": row.get("call_address"),
                                "receiver_vtable_id": vtable,
                                "dispatch_slot_offset": normalize_slot(row.get("dispatch_slot_offset")),
                                "receiver_class_name": primary_class,
                                "receiver_class_match_kind": row["class_match_kind"],
                                "receiver_class_evidence_source": row["class_evidence_source"],
                                "receiver_type_name": row.get("receiver_type_name"),
                                "receiver_type_display": row.get("receiver_type_display"),
                                "runtime_validation_status": row.get("runtime_validation_status", "runtime_unobserved"),
                                "evidence_ref": f"{key[0]}#call={key[1]}",
                                "affects_routing": False,
                            }

        if not recovery["accepted"]:
            counts["blocked"] += 1
            row["phi_primary_subtree_recovery"] = recovery
        details.append(recovery)
        output.append(row)

    output_queries = [query_by_key[key] for key in sorted(query_by_key)]
    resolved_keys = {key_of(row, "output") for row in output if row.get("status") in RESOLVED}
    query_keys = set(query_by_key)
    if resolved_keys != query_keys:
        raise ValueError(
            f"final query/inference resolved-key mismatch: "
            f"missing={sorted(resolved_keys - query_keys)} extra={sorted(query_keys - resolved_keys)}"
        )

    write_jsonl(args.output, output)
    write_jsonl(args.queries_output, output_queries)
    if args.details_output is not None:
        write_jsonl(args.details_output, details)

    print("format: NEVERGONE.PhiPrimarySubtreeRecoveryFinalization/1")
    print("inference rows:", len(inference))
    print("input queries:", len(queries))
    print("eligible primary-subtree rows:", counts["eligible"])
    print("primary-subtree recoveries accepted:", counts["accepted"])
    print("primary-subtree recoveries blocked:", counts["blocked"])
    print("output queries:", len(output_queries))
    print("affects routing: no")
    print("output:", args.output)
    print("queries output:", args.queries_output)
    if args.details_output is not None:
        print("details output:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
