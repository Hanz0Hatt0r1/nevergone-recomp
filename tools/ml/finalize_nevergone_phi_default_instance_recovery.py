#!/usr/bin/env python3
"""Resolve structural receiver PHIs only when both branches prove one class.

This is a fail-closed finalizer over the v6 receiver inference. It accepts only
receiver_not_single_pointer rows whose structural receiver is a canonical
MULTIEQUAL and whose evidence chain proves:

  typed primary PHI branch -> catalog class
  secondary PHI branch -> corrected literal-pool fold -> one GOT slot
  GOT slot -> one .bss default_instance object
  default-instance xrefs -> the same class
  canonical target slot -> the original dispatch slot

No class is inferred from a virtual target, address proximity, or sibling call.
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
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
PHI_AUDIT_FORMAT = "NEVERGONE.PhiSecondaryLoadTreeAudit/2"
ADDRESS_FORMAT = "NEVERGONE.GhidraAddressClassification/1"
OWNER_FORMAT = "NEVERGONE.GhidraAddressReferenceOwners/1"
TARGET_PROVENANCE_FORMAT = "NEVERGONE.GhidraTargetReceiverProvenance/1"
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match"}
MASK32 = 0xFFFFFFFF


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            out.append(row)
    return out


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def parse_int(value: Any, default: int | None = None) -> int | None:
    if value in (None, "", "-"):
        return default
    text = str(value).strip()
    try:
        if re.fullmatch(r"[0-9a-fA-F]+", text) and not text.lower().startswith("0x"):
            return int(text, 16)
        return int(text, 0)
    except ValueError:
        return default


def normalize_slot(value: Any) -> str | None:
    parsed = parse_int(value)
    return f"0x{parsed:x}" if parsed is not None and parsed >= 0 else None


def source_entry(source_id: str) -> str:
    return source_id.split(":", 1)[1] if ":" in source_id else source_id


def key_of(row: dict[str, Any], *, where: str) -> tuple[str, int]:
    source_id = row.get("source_id")
    call_index = row.get("call_index")
    if not isinstance(source_id, str) or not source_id or not isinstance(call_index, int) or call_index < 0:
        raise ValueError(f"{where}: invalid source_id/call_index")
    return source_id, call_index


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


def address_key(value: Any) -> int | None:
    parsed = parse_int(value)
    return parsed & MASK32 if parsed is not None else None


def load_slice_rows(slice_dir: Path) -> dict[tuple[str, int], dict[str, Any]]:
    grouped: dict[tuple[str, int], dict[str, Any]] = {}
    for path in sorted(slice_dir.glob("*.tsv")):
        rows = read_tsv(path)
        for line_no, row in enumerate(rows, 2):
            if row.get("format") != SLICE_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {SLICE_FORMAT}")
        by_call: dict[tuple[str, int], list[dict[str, str]]] = defaultdict(list)
        for row in rows:
            entry = row.get("function_entry") or ""
            idx = parse_int(row.get("call_index"), -1)
            if entry and idx is not None and idx >= 0:
                by_call[(entry, idx)].append(row)
        for key, call_rows in by_call.items():
            summary = next((r for r in call_rows if r.get("row_type") == "summary"), None)
            nodes = [r for r in call_rows if r.get("row_type") == "node"]
            if summary is None or not nodes:
                continue
            if key in grouped:
                raise ValueError(f"duplicate structural slice call: {key}")
            grouped[key] = {"summary": summary, "nodes": nodes, "path": str(path)}
    return grouped


def direct_children(nodes: list[dict[str, str]], parent_id: int) -> list[dict[str, str]]:
    out = []
    for row in nodes:
        if parse_int(row.get("parent_node_id"), -1) == parent_id:
            out.append(row)
    out.sort(key=lambda r: (parse_int(r.get("input_index"), -1) or -1, parse_int(r.get("node_id"), -1) or -1))
    return out


def parse_owner_class(symbol: str | None) -> str | None:
    if not symbol or symbol == "-":
        return None
    match = re.fullmatch(r"(.+)::default_instance_", symbol.strip())
    return match.group(1) if match else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("queries_jsonl", type=Path)
    parser.add_argument("slice_dir", type=Path)
    parser.add_argument("phi_secondary_audit_jsonl", type=Path)
    parser.add_argument("got_classification_tsv", type=Path)
    parser.add_argument("got_target_classification_tsv", type=Path)
    parser.add_argument("owner_references_tsv", type=Path)
    parser.add_argument("target_provenance_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--queries-output", type=Path, required=True)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    inference = read_jsonl(args.inference_jsonl)
    queries = read_jsonl(args.queries_jsonl)
    catalog_rows = read_jsonl(args.catalog_jsonl)

    class_vtables: dict[str, set[str]] = defaultdict(set)
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
        name = row.get("class_name")
        vtable = row.get("receiver_vtable_id")
        if isinstance(name, str) and name and isinstance(vtable, str) and vtable:
            class_vtables[name].add(vtable)
    catalog = set(class_vtables)
    by_terminal: dict[str, set[str]] = defaultdict(set)
    for name in catalog:
        by_terminal[terminal(name)].add(name)

    slices = load_slice_rows(args.slice_dir)

    phi_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(read_jsonl(args.phi_secondary_audit_jsonl), 1):
        if row.get("format") != PHI_AUDIT_FORMAT:
            raise ValueError(f"{args.phi_secondary_audit_jsonl}:{line_no}: expected {PHI_AUDIT_FORMAT}")
        entry = row.get("function_entry")
        idx = row.get("call_index")
        if isinstance(entry, str) and isinstance(idx, int):
            key = (entry, idx)
            if key in phi_by_key:
                raise ValueError(f"duplicate PHI secondary audit row: {key}")
            phi_by_key[key] = row

    got_by_address: dict[int, dict[str, str]] = {}
    for line_no, row in enumerate(read_tsv(args.got_classification_tsv), 2):
        if row.get("format") != ADDRESS_FORMAT:
            raise ValueError(f"{args.got_classification_tsv}:{line_no}: expected {ADDRESS_FORMAT}")
        address = address_key(row.get("address"))
        if address is not None:
            got_by_address[address] = row

    target_by_address: dict[int, dict[str, str]] = {}
    for line_no, row in enumerate(read_tsv(args.got_target_classification_tsv), 2):
        if row.get("format") != ADDRESS_FORMAT:
            raise ValueError(f"{args.got_target_classification_tsv}:{line_no}: expected {ADDRESS_FORMAT}")
        address = address_key(row.get("address"))
        if address is not None:
            target_by_address[address] = row

    owners_by_target: dict[int, list[dict[str, str]]] = defaultdict(list)
    for line_no, row in enumerate(read_tsv(args.owner_references_tsv), 2):
        if row.get("format") != OWNER_FORMAT:
            raise ValueError(f"{args.owner_references_tsv}:{line_no}: expected {OWNER_FORMAT}")
        target = address_key(row.get("target_address"))
        if target is not None:
            owners_by_target[target].append(row)

    target_provenance: dict[tuple[str, int], dict[str, str]] = {}
    for line_no, row in enumerate(read_tsv(args.target_provenance_tsv), 2):
        if row.get("format") != TARGET_PROVENANCE_FORMAT:
            raise ValueError(f"{args.target_provenance_tsv}:{line_no}: expected {TARGET_PROVENANCE_FORMAT}")
        source_id = row.get("source_id")
        idx = parse_int(row.get("call_index"), -1)
        if source_id and idx is not None and idx >= 0:
            key = (source_id, idx)
            if key in target_provenance:
                raise ValueError(f"duplicate target provenance row: {key}")
            target_provenance[key] = row

    query_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(queries, 1):
        if row.get("format") != QUERY_FORMAT:
            raise ValueError(f"{args.queries_jsonl}:{line_no}: expected {QUERY_FORMAT}")
        key = key_of(row, where=f"{args.queries_jsonl}:{line_no}")
        if key in query_by_key:
            raise ValueError(f"duplicate query row: {key}")
        query_by_key[key] = dict(row)

    output: list[dict[str, Any]] = []
    details: list[dict[str, Any]] = []
    counts: Counter[str] = Counter()

    for line_no, original in enumerate(inference, 1):
        if original.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"{args.inference_jsonl}:{line_no}: expected {INFERENCE_FORMAT}")
        row = dict(original)
        key = key_of(row, where=f"{args.inference_jsonl}:{line_no}")
        reasons: list[str] = []
        evidence: dict[str, Any] = {
            "format": "NEVERGONE.PhiDefaultInstanceRecovery/1",
            "source_id": key[0],
            "call_index": key[1],
            "eligible": False,
            "accepted": False,
            "reasons": reasons,
            "affects_routing": False,
        }

        if row.get("status") != "receiver_not_single_pointer":
            output.append(row)
            continue

        entry = source_entry(key[0])
        slice_info = slices.get((entry, key[1]))
        phi = phi_by_key.get((entry, key[1]))
        if slice_info is None or phi is None:
            output.append(row)
            continue
        evidence["eligible"] = True
        counts["eligible"] += 1

        dispatch_slot = normalize_slot(row.get("dispatch_slot_offset"))
        call_address = row.get("call_address")

        summary = slice_info["summary"]
        nodes = slice_info["nodes"]
        root = next((n for n in nodes if parse_int(n.get("node_id"), -1) == 0), None)
        if root is None or root.get("defining_opcode") != "MULTIEQUAL":
            reasons.append("root_not_multiequal")
        if normalize_slot(summary.get("slot_offset")) != dispatch_slot:
            reasons.append("slice_slot_mismatch")
        if summary.get("recovery_status") != "canonical_receiver":
            reasons.append("slice_not_canonical_receiver")
        if root is not None and root.get("call_address") != call_address:
            reasons.append("slice_call_address_mismatch")

        primary_classes: set[str] = set()
        if root is not None:
            children = direct_children(nodes, 0)
            primary = next((n for n in children if parse_int(n.get("input_index"), -1) == 0), None)
            secondary = next((n for n in children if parse_int(n.get("input_index"), -1) == 1), None)
            if primary is None or secondary is None:
                reasons.append("phi_branches_missing")
            else:
                primary_depth = parse_int(primary.get("pointer_depth"), 0) or 0
                if primary_depth != 1:
                    reasons.append("primary_branch_not_single_pointer")
                primary_classes = map_type(primary.get("pointee_type_display"), catalog, by_terminal)
                if len(primary_classes) != 1:
                    reasons.append("primary_branch_class_not_unique")
                evidence["primary_branch_type"] = primary.get("type_display")
                evidence["primary_branch_pointee"] = primary.get("pointee_type_display")
                evidence["primary_branch_classes"] = sorted(primary_classes)

        if phi.get("call_address") != call_address:
            reasons.append("phi_audit_call_address_mismatch")
        if normalize_slot(phi.get("slot_offset")) != dispatch_slot:
            reasons.append("phi_audit_slot_mismatch")
        unresolved = phi.get("unresolved_load_addresses")
        got_address: int | None = None
        if not isinstance(unresolved, list) or len(unresolved) != 1 or not isinstance(unresolved[0], int):
            reasons.append("secondary_branch_not_one_got_load")
        else:
            got_address = unresolved[0] & MASK32
            evidence["secondary_got_address"] = f"0x{got_address:08x}"

        target_row = target_provenance.get(key)
        if target_row is None:
            reasons.append("missing_target_provenance")
        else:
            if target_row.get("structural_status") != "canonical_receiver":
                reasons.append("target_not_canonical_receiver")
            if normalize_slot(target_row.get("target_slot_offset")) != dispatch_slot:
                reasons.append("target_slot_mismatch")

        default_target: int | None = None
        if got_address is not None:
            got = got_by_address.get(got_address)
            if got is None:
                reasons.append("missing_got_classification")
            else:
                if got.get("block") != ".got":
                    reasons.append("secondary_load_not_got")
                if not (got.get("primary_symbol") or "").startswith("PTR_default_instance__"):
                    reasons.append("got_symbol_not_default_instance_pointer")
                default_target = address_key(got.get("raw_u32"))
                if default_target is None:
                    reasons.append("got_target_missing")
                else:
                    evidence["default_instance_address"] = f"0x{default_target:08x}"

        owner_class: str | None = None
        owner_support = 0
        if default_target is not None:
            classified = target_by_address.get(default_target)
            if classified is None:
                reasons.append("missing_default_instance_classification")
            elif classified.get("block") != ".bss":
                reasons.append("default_instance_not_bss")

            owner_rows = owners_by_target.get(default_target, [])
            owner_classes = {
                cls for cls in (parse_owner_class(r.get("target_symbol")) for r in owner_rows) if cls
            }
            evidence["owner_classes"] = sorted(owner_classes)
            if len(owner_classes) != 1:
                reasons.append("owner_class_not_unique")
            else:
                owner_class = next(iter(owner_classes))
                owner_support = sum(
                    1 for r in owner_rows
                    if (r.get("containing_function_name") or "").startswith(owner_class + "::")
                )
                if owner_support == 0:
                    reasons.append("owner_class_has_no_direct_method_xref")
            evidence["owner_direct_method_xrefs"] = owner_support

        primary_class = next(iter(primary_classes)) if len(primary_classes) == 1 else None
        if primary_class is not None and owner_class is not None and primary_class != owner_class:
            reasons.append("phi_branch_class_mismatch")

        chosen_class = primary_class if primary_class == owner_class else None
        vtable: str | None = None
        if chosen_class is not None:
            vtables = sorted(class_vtables.get(chosen_class, set()))
            evidence["candidate_vtables"] = vtables
            if len(vtables) != 1:
                reasons.append("owner_class_vtable_not_unique")
            else:
                vtable = vtables[0]

        if not reasons and chosen_class is not None and vtable is not None:
            counts["accepted"] += 1
            evidence["accepted"] = True
            row["class_name"] = chosen_class
            row["class_match_kind"] = "phi_default_instance_exact_catalog_class_name"
            row["class_evidence_source"] = "structural_phi_default_instance_owner"
            row["candidate_receiver_class_names"] = [chosen_class]
            row["candidate_receiver_vtable_ids"] = [vtable]
            row["receiver_vtable_id"] = vtable
            row["status"] = "resolved_unique_vtable"
            row["phi_default_instance_recovery"] = evidence

            if key in query_by_key:
                raise ValueError(f"query already exists for newly recovered row: {key}")
            query_by_key[key] = {
                "format": QUERY_FORMAT,
                "source_id": key[0],
                "call_index": key[1],
                "call_address": call_address,
                "receiver_vtable_id": vtable,
                "dispatch_slot_offset": dispatch_slot,
                "receiver_class_name": chosen_class,
                "receiver_class_match_kind": row["class_match_kind"],
                "receiver_class_evidence_source": row["class_evidence_source"],
                "receiver_type_name": row.get("receiver_type_name"),
                "receiver_type_display": row.get("receiver_type_display"),
                "runtime_validation_status": row.get("runtime_validation_status", "runtime_unobserved"),
                "evidence_ref": f"{key[0]}#call={key[1]}",
                "affects_routing": False,
            }
        else:
            counts["blocked"] += 1
            row["phi_default_instance_recovery"] = evidence

        details.append(evidence)
        output.append(row)

    output_queries = [query_by_key[key] for key in sorted(query_by_key)]
    resolved_keys = {key_of(row, where="output") for row in output if row.get("status") in RESOLVED}
    query_keys = set(query_by_key)
    if resolved_keys != query_keys:
        missing = sorted(resolved_keys - query_keys)
        extra = sorted(query_keys - resolved_keys)
        raise ValueError(f"final query/inference resolved-key mismatch: missing={missing} extra={extra}")

    write_jsonl(args.output, output)
    write_jsonl(args.queries_output, output_queries)
    if args.details_output is not None:
        write_jsonl(args.details_output, details)

    print("format: NEVERGONE.PhiDefaultInstanceRecoveryFinalization/1")
    print("inference rows:", len(inference))
    print("input queries:", len(queries))
    print("eligible PHI rows:", counts["eligible"])
    print("PHI recoveries accepted:", counts["accepted"])
    print("PHI recoveries blocked:", counts["blocked"])
    print("output queries:", len(output_queries))
    print("affects routing: no")
    print("output:", args.output)
    print("queries output:", args.queries_output)
    if args.details_output is not None:
        print("details output:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
