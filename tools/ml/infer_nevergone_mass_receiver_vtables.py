#!/usr/bin/env python3
"""Mass receiver class/vtable inference from Ghidra receiver-type evidence.

This is the scalable counterpart of the bounded receiver frontier. It resolves only
receiver identity facts that do not require a dispatch-slot slice: direct one-level
pointer types and bounded transparent type-flow candidates may map to a catalog class
and, when that class owns exactly one catalog vtable address point, to a unique
receiver vtable. No virtual target or slot is inferred here.

The output intentionally reuses NEVERGONE.StaticReceiverVtableInference/1 so the
existing v5 evidence aggregator can consume it. Rows resolved without a slot use the
normal resolved_unique_vtable status plus slot_evidence_present=false and
resolution_scope=receiver_identity_only_no_dispatch_slot. No query/target transfer is
emitted by this tool.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from infer_nevergone_static_receiver_vtables import (
    CATALOG_FORMAT,
    OUTPUT_FORMAT,
    RECEIVER_FORMATS,
    append_candidates,
    parse_int_cell,
    parse_json_string_list,
    read_jsonl,
    read_tsv,
    source_entry,
    terminal_class_name,
)

FORMAT = "NEVERGONE.MassReceiverVtableInference/1"


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receiver_types_tsv", type=Path)
    parser.add_argument("catalog", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    receiver_rows = read_tsv(args.receiver_types_tsv)
    catalog_rows = read_jsonl(args.catalog)

    class_vtables: dict[str, set[str]] = defaultdict(set)
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog}:{line_no}: expected {CATALOG_FORMAT}")
        class_name = row.get("class_name")
        vtable_id = row.get("receiver_vtable_id")
        if isinstance(class_name, str) and class_name and isinstance(vtable_id, str) and vtable_id:
            class_vtables[class_name].add(vtable_id)

    terminal_to_classes: dict[str, set[str]] = defaultdict(set)
    for class_name in class_vtables:
        terminal_to_classes[terminal_class_name(class_name)].add(class_name)

    output: list[dict[str, Any]] = []
    counts: Counter[str] = Counter()
    resolved_functions: set[str] = set()
    seen_keys: set[tuple[str, int]] = set()

    for line_no, receiver in enumerate(receiver_rows, 2):
        if receiver.get("format") not in RECEIVER_FORMATS:
            raise ValueError(
                f"{args.receiver_types_tsv}:{line_no}: expected one of {sorted(RECEIVER_FORMATS)}"
            )
        source_id = receiver.get("source_id")
        try:
            call_index = int(receiver.get("call_index", "-1"))
        except ValueError as error:
            raise ValueError(f"{args.receiver_types_tsv}:{line_no}: invalid call_index") from error
        if not source_id or call_index < 0:
            raise ValueError(f"{args.receiver_types_tsv}:{line_no}: invalid receiver key")
        key = (source_id, call_index)
        if key in seen_keys:
            raise ValueError(f"duplicate receiver type row: {source_id}#{call_index}")
        seen_keys.add(key)

        pointer_depth = parse_int_cell(receiver.get("receiver_pointer_depth"))
        flow_names: list[str] = []
        flow_displays: list[str] = []
        if receiver.get("format") == "NEVERGONE.GhidraIndirectReceiverType/2":
            flow_names = parse_json_string_list(
                receiver.get("flow_single_pointer_pointee_names_json"),
                where=f"{args.receiver_types_tsv}:{line_no}:flow names",
            )
            flow_displays = parse_json_string_list(
                receiver.get("flow_single_pointer_pointee_displays_json"),
                where=f"{args.receiver_types_tsv}:{line_no}:flow displays",
            )

        base: dict[str, Any] = {
            "format": OUTPUT_FORMAT,
            "mass_inference_format": FORMAT,
            "source_id": source_id,
            "source_entry": source_entry(source_id),
            "call_index": call_index,
            "call_address": receiver.get("call_address"),
            "dispatch_slot_offset": None,
            "slot_evidence_present": False,
            "resolution_scope": "receiver_identity_only_no_dispatch_slot",
            "receiver_type_evidence_present": True,
            "receiver_type_format": receiver.get("format"),
            "receiver_type_name": receiver.get("receiver_type_name"),
            "receiver_type_display": receiver.get("receiver_type_display"),
            "receiver_pointer_depth": pointer_depth,
            "pointee_type_name": receiver.get("pointee_type_name"),
            "pointee_type_display": receiver.get("pointee_type_display"),
            "receiver_storage_kind": receiver.get("receiver_storage_kind"),
            "flow_type_node_count": parse_int_cell(receiver.get("flow_type_node_count")),
            "flow_single_pointer_pointee_names": flow_names,
            "flow_single_pointer_pointee_displays": flow_displays,
            "parameter_provenance_present": False,
            "structural_parameter_provenance_present": False,
            "class_name": None,
            "class_match_kind": None,
            "class_evidence_source": None,
            "candidate_receiver_class_names": [],
            "candidate_receiver_vtable_ids": [],
            "receiver_vtable_id": None,
            "runtime_validation_status": "runtime_unobserved",
            "affects_routing": False,
            "target_transfer_scope": "disabled_without_dispatch_slot",
            "cross_vtable_transfer_allowed": False,
            "cross_class_transfer_allowed": False,
        }

        local_sources: list[tuple[Any, str]] = []
        if pointer_depth == 1:
            local_sources.extend(
                [
                    (receiver.get("pointee_type_display"), "direct_receiver_type"),
                    (receiver.get("pointee_type_name"), "direct_receiver_type"),
                ]
            )
        local_sources.extend((value, "transparent_type_flow") for value in flow_displays)
        local_sources.extend((value, "transparent_type_flow") for value in flow_names)

        candidates = append_candidates(local_sources, class_vtables, terminal_to_classes)
        class_names = sorted({name for name, _kind, _source in candidates})
        base["candidate_receiver_class_names"] = class_names

        if not class_names:
            if pointer_depth != 1 and not flow_names and not flow_displays:
                status = "receiver_not_single_pointer"
            else:
                status = "unresolved_receiver_class"
        elif len(class_names) > 1:
            status = "ambiguous_receiver_class"
        else:
            class_name = class_names[0]
            exact = [
                candidate
                for candidate in candidates
                if candidate[0] == class_name and candidate[1] == "exact_catalog_class_name"
            ]
            chosen = exact[0] if exact else next(
                candidate for candidate in candidates if candidate[0] == class_name
            )
            _name, match_kind, source_kind = chosen
            if source_kind == "transparent_type_flow":
                match_kind = "transparent_flow_" + match_kind
            vtables = sorted(class_vtables[class_name])
            base["class_name"] = class_name
            base["class_match_kind"] = match_kind
            base["class_evidence_source"] = source_kind
            base["candidate_receiver_vtable_ids"] = vtables
            if len(vtables) != 1:
                status = "ambiguous_class_vtable"
            else:
                status = "resolved_unique_vtable"
                base["receiver_vtable_id"] = vtables[0]
                resolved_functions.add(source_id)
                counts[f"resolved_via_{source_kind}"] += 1

        base["status"] = status
        counts[status] += 1
        output.append(base)

    write_jsonl(args.output, output)

    print("format:", FORMAT)
    print("output compatibility format:", OUTPUT_FORMAT)
    print("receiver type rows:", len(receiver_rows))
    print("calls considered:", len(output))
    print("functions with resolved receiver vtable:", len(resolved_functions))
    print("resolved unique vtable:", counts["resolved_unique_vtable"])
    print("resolved via direct receiver type:", counts["resolved_via_direct_receiver_type"])
    print("resolved via transparent type flow:", counts["resolved_via_transparent_type_flow"])
    print("ambiguous class vtable:", counts["ambiguous_class_vtable"])
    print("ambiguous receiver class:", counts["ambiguous_receiver_class"])
    print("unresolved receiver class:", counts["unresolved_receiver_class"])
    print("receiver not single pointer:", counts["receiver_not_single_pointer"])
    print("dispatch slots inferred: 0")
    print("target queries emitted: 0")
    print("affects routing: no")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
