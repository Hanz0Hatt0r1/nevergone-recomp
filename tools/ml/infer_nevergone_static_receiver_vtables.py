#!/usr/bin/env python3
"""Infer exact receiver vtables from conservative Ghidra receiver type evidence.

This layer never infers a class from the virtual target. It accepts direct one-level
pointer receiver types and, for receiver evidence format /2, bounded transparent
COPY/CAST/INDIRECT/MULTIEQUAL type-flow candidates exported by Ghidra. Optionally,
when those local sources yield no catalog class, it can use bounded function-parameter
and direct-caller type provenance as a fallback. All usable class candidates must
converge to one catalog class, and that class must have exactly one catalog vtable
address point. Runtime observations, when supplied, only validate or block the
inference. Routing is never modified.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
OUTPUT_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
PARAMETER_PROVENANCE_FORMAT = "NEVERGONE.GhidraReceiverParameterProvenance/1"
RECEIVER_FORMATS = {
    "NEVERGONE.GhidraIndirectReceiverType/1",
    "NEVERGONE.GhidraIndirectReceiverType/2",
}


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


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def normalize_slot(value: Any) -> str | None:
    if isinstance(value, int):
        return f"0x{value:x}" if value >= 0 else None
    if not isinstance(value, str) or not value:
        return None
    try:
        parsed = int(value, 0)
    except ValueError:
        return None
    return f"0x{parsed:x}" if parsed >= 0 else None


def parse_int_cell(value: Any, *, default: int = 0) -> int:
    if value in (None, "", "-"):
        return default
    try:
        return int(str(value), 0)
    except ValueError as error:
        raise ValueError(f"invalid integer cell: {value!r}") from error


def clean_type_name(value: Any) -> str | None:
    if not isinstance(value, str):
        return None
    text = value.strip()
    if not text or text == "-":
        return None
    text = re.sub(r"^(?:class|struct)\s+", "", text)
    text = re.sub(r"\s+", " ", text).strip()
    return text or None


def terminal_class_name(name: str) -> str:
    return name.rsplit("::", 1)[-1]


def source_entry(source_id: str) -> str:
    return source_id.split(":", 1)[1] if ":" in source_id else source_id


def parse_json_string_list(value: Any, *, where: str) -> list[str]:
    if value in (None, "", "-"):
        return []
    if not isinstance(value, str):
        raise ValueError(f"{where}: expected JSON string list cell")
    try:
        parsed = json.loads(value)
    except json.JSONDecodeError as error:
        raise ValueError(f"{where}: invalid JSON string list") from error
    if not isinstance(parsed, list) or not all(isinstance(x, str) for x in parsed):
        raise ValueError(f"{where}: expected JSON array of strings")
    return parsed


def resolve_type_to_catalog_classes(
    raw: Any,
    class_vtables: dict[str, set[str]],
    terminal_to_classes: dict[str, set[str]],
) -> list[tuple[str, str]]:
    cleaned = clean_type_name(raw)
    if cleaned is None:
        return []
    if cleaned in class_vtables:
        return [(cleaned, "exact_catalog_class_name")]
    terminal = terminal_class_name(cleaned)
    terminal_classes = terminal_to_classes.get(terminal, set())
    if len(terminal_classes) == 1:
        return [(next(iter(terminal_classes)), "unique_terminal_class_name")]
    return []


def append_candidates(
    typed_sources: list[tuple[Any, str]],
    class_vtables: dict[str, set[str]],
    terminal_to_classes: dict[str, set[str]],
) -> list[tuple[str, str, str]]:
    candidates: list[tuple[str, str, str]] = []
    seen_raw: set[tuple[str, str]] = set()
    for raw, source_kind in typed_sources:
        cleaned = clean_type_name(raw)
        if cleaned is None or (cleaned, source_kind) in seen_raw:
            continue
        seen_raw.add((cleaned, source_kind))
        for class_name, match_kind in resolve_type_to_catalog_classes(
            cleaned, class_vtables, terminal_to_classes
        ):
            candidates.append((class_name, match_kind, source_kind))
    return candidates


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("receiver_types_tsv", type=Path)
    parser.add_argument("catalog", type=Path)
    parser.add_argument("--runtime-observations", type=Path)
    parser.add_argument("--parameter-provenance", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--queries-output", type=Path, required=True)
    args = parser.parse_args()

    analysis_rows = read_jsonl(args.analysis)
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

    receivers: dict[tuple[str, int], dict[str, str]] = {}
    for line_no, row in enumerate(receiver_rows, 2):
        if row.get("format") not in RECEIVER_FORMATS:
            raise ValueError(
                f"{args.receiver_types_tsv}:{line_no}: expected one of {sorted(RECEIVER_FORMATS)}"
            )
        source_id = row.get("source_id")
        try:
            call_index = int(row.get("call_index", "-1"))
        except ValueError as error:
            raise ValueError(f"{args.receiver_types_tsv}:{line_no}: invalid call_index") from error
        if not source_id or call_index < 0:
            raise ValueError(f"{args.receiver_types_tsv}:{line_no}: invalid receiver key")
        key = (source_id, call_index)
        if key in receivers:
            raise ValueError(f"duplicate receiver type row: {source_id}#{call_index}")
        row["_line_no"] = str(line_no)
        receivers[key] = row

    parameter_provenance: dict[tuple[str, int], dict[str, str]] = {}
    if args.parameter_provenance is not None:
        for line_no, row in enumerate(read_tsv(args.parameter_provenance), 2):
            if row.get("format") != PARAMETER_PROVENANCE_FORMAT:
                raise ValueError(
                    f"{args.parameter_provenance}:{line_no}: expected {PARAMETER_PROVENANCE_FORMAT}"
                )
            source_id = row.get("source_id")
            try:
                call_index = int(row.get("call_index", "-1"))
            except ValueError as error:
                raise ValueError(
                    f"{args.parameter_provenance}:{line_no}: invalid call_index"
                ) from error
            if not source_id or call_index < 0:
                raise ValueError(f"{args.parameter_provenance}:{line_no}: invalid provenance key")
            key = (source_id, call_index)
            if key in parameter_provenance:
                raise ValueError(f"duplicate parameter provenance row: {source_id}#{call_index}")
            row["_line_no"] = str(line_no)
            parameter_provenance[key] = row

    runtime: dict[tuple[str, int], dict[str, Any]] = {}
    if args.runtime_observations is not None:
        for row in read_jsonl(args.runtime_observations):
            source_id = row.get("source_id")
            call_index = row.get("call_index")
            if isinstance(source_id, str) and isinstance(call_index, int):
                runtime[(source_id, call_index)] = row

    output: list[dict[str, Any]] = []
    queries: list[dict[str, Any]] = []
    counts: Counter[str] = Counter()

    for source in analysis_rows:
        source_id = source.get("id")
        if not isinstance(source_id, str) or not source_id:
            raise ValueError("analysis row missing id")
        evidence = source.get("indirect_call_slice_evidence")
        if not isinstance(evidence, dict) or not evidence.get("present"):
            continue
        calls = evidence.get("calls")
        if not isinstance(calls, list):
            continue

        for call in calls:
            call_index = call.get("call_index")
            if not isinstance(call_index, int):
                continue
            key = (source_id, call_index)
            slot = normalize_slot(call.get("primary_slot_offset_candidate"))
            receiver = receivers.get(key)
            provenance = parameter_provenance.get(key)

            pointer_depth = parse_int_cell(receiver.get("receiver_pointer_depth")) if receiver else 0
            flow_names: list[str] = []
            flow_displays: list[str] = []
            if receiver and receiver.get("format") == "NEVERGONE.GhidraIndirectReceiverType/2":
                line_no = receiver.get("_line_no", "?")
                flow_names = parse_json_string_list(
                    receiver.get("flow_single_pointer_pointee_names_json"),
                    where=f"{args.receiver_types_tsv}:{line_no}:flow names",
                )
                flow_displays = parse_json_string_list(
                    receiver.get("flow_single_pointer_pointee_displays_json"),
                    where=f"{args.receiver_types_tsv}:{line_no}:flow displays",
                )

            caller_names: list[str] = []
            caller_displays: list[str] = []
            provenance_status = provenance.get("status") if provenance else None
            parameter_index = -1
            matched_caller_calls = 0
            direct_caller_count = 0
            if provenance is not None:
                line_no = provenance.get("_line_no", "?")
                parameter_index = parse_int_cell(provenance.get("parameter_index"), default=-1)
                matched_caller_calls = parse_int_cell(provenance.get("matched_caller_calls"))
                direct_caller_count = parse_int_cell(provenance.get("direct_caller_count"))
                caller_names = parse_json_string_list(
                    provenance.get("caller_single_pointer_pointee_names_json"),
                    where=f"{args.parameter_provenance}:{line_no}:caller names",
                )
                caller_displays = parse_json_string_list(
                    provenance.get("caller_single_pointer_pointee_displays_json"),
                    where=f"{args.parameter_provenance}:{line_no}:caller displays",
                )

            base: dict[str, Any] = {
                "format": OUTPUT_FORMAT,
                "source_id": source_id,
                "source_entry": source_entry(source_id),
                "call_index": call_index,
                "call_address": call.get("call_address"),
                "dispatch_slot_offset": slot,
                "receiver_type_evidence_present": receiver is not None,
                "receiver_type_format": receiver.get("format") if receiver else None,
                "receiver_type_name": receiver.get("receiver_type_name") if receiver else None,
                "receiver_type_display": receiver.get("receiver_type_display") if receiver else None,
                "receiver_pointer_depth": pointer_depth,
                "pointee_type_name": receiver.get("pointee_type_name") if receiver else None,
                "pointee_type_display": receiver.get("pointee_type_display") if receiver else None,
                "receiver_storage_kind": receiver.get("receiver_storage_kind") if receiver else None,
                "flow_type_node_count": parse_int_cell(receiver.get("flow_type_node_count")) if receiver else 0,
                "flow_single_pointer_pointee_names": flow_names,
                "flow_single_pointer_pointee_displays": flow_displays,
                "parameter_provenance_present": provenance is not None,
                "parameter_provenance_status": provenance_status,
                "parameter_index": parameter_index,
                "direct_caller_count": direct_caller_count,
                "matched_caller_calls": matched_caller_calls,
                "caller_single_pointer_pointee_names": caller_names,
                "caller_single_pointer_pointee_displays": caller_displays,
                "class_name": None,
                "class_match_kind": None,
                "class_evidence_source": None,
                "candidate_receiver_class_names": [],
                "candidate_receiver_vtable_ids": [],
                "receiver_vtable_id": None,
                "runtime_validation_status": "runtime_unobserved",
                "affects_routing": False,
                "target_transfer_scope": "same_receiver_vtable_and_slot_only",
                "cross_vtable_transfer_allowed": False,
                "cross_class_transfer_allowed": False,
            }

            if receiver is None:
                status = "missing_receiver_type_evidence"
            elif slot is None:
                status = "missing_slot_offset"
            else:
                local_sources: list[tuple[Any, str]] = []
                if pointer_depth == 1:
                    local_sources.extend([
                        (receiver.get("pointee_type_display"), "direct_receiver_type"),
                        (receiver.get("pointee_type_name"), "direct_receiver_type"),
                    ])
                local_sources.extend((value, "transparent_type_flow") for value in flow_displays)
                local_sources.extend((value, "transparent_type_flow") for value in flow_names)

                candidates = append_candidates(local_sources, class_vtables, terminal_to_classes)
                class_names = sorted({name for name, _kind, _source in candidates})

                # Caller evidence is a strict fallback. Never combine it with a local class
                # candidate: local ambiguity or a concrete local class remains authoritative.
                if not class_names and provenance is not None:
                    provenance_usable = (
                        provenance_status == "caller_type_candidate"
                        and parameter_index >= 0
                        and matched_caller_calls > 0
                    )
                    if provenance_usable:
                        caller_sources: list[tuple[Any, str]] = []
                        caller_sources.extend(
                            (value, "parameter_caller_type") for value in caller_displays
                        )
                        caller_sources.extend(
                            (value, "parameter_caller_type") for value in caller_names
                        )
                        caller_candidates = append_candidates(
                            caller_sources, class_vtables, terminal_to_classes
                        )
                        candidates.extend(caller_candidates)
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
                    exact = [x for x in candidates if x[0] == class_name and x[1] == "exact_catalog_class_name"]
                    chosen = exact[0] if exact else next(x for x in candidates if x[0] == class_name)
                    _name, match_kind, source_kind = chosen
                    if source_kind == "transparent_type_flow":
                        match_kind = "transparent_flow_" + match_kind
                    elif source_kind == "parameter_caller_type":
                        match_kind = "parameter_caller_" + match_kind
                    vtables = sorted(class_vtables[class_name])
                    base["class_name"] = class_name
                    base["class_match_kind"] = match_kind
                    base["class_evidence_source"] = source_kind
                    base["candidate_receiver_vtable_ids"] = vtables
                    if len(vtables) != 1:
                        status = "ambiguous_class_vtable"
                    else:
                        inferred = vtables[0]
                        base["receiver_vtable_id"] = inferred
                        observed = runtime.get(key)
                        if observed is not None:
                            observed_vtable = observed.get("receiver_vtable_id")
                            observed_slot = normalize_slot(observed.get("dispatch_slot_offset"))
                            if observed_vtable != inferred or observed_slot != slot:
                                status = "runtime_mismatch_blocked"
                                base["runtime_validation_status"] = "runtime_mismatch"
                                base["runtime_receiver_vtable_id"] = observed_vtable
                                base["runtime_dispatch_slot_offset"] = observed_slot
                            else:
                                status = "resolved_unique_vtable_runtime_match"
                                base["runtime_validation_status"] = "runtime_match"
                        else:
                            status = "resolved_unique_vtable"
                        if source_kind == "transparent_type_flow":
                            counts["resolved_via_transparent_type_flow"] += 1
                        elif source_kind == "parameter_caller_type":
                            counts["resolved_via_parameter_caller_type"] += 1

            base["status"] = status
            counts[status] += 1
            output.append(base)

            if status in {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match"}:
                queries.append({
                    "format": QUERY_FORMAT,
                    "source_id": source_id,
                    "call_index": call_index,
                    "call_address": call.get("call_address"),
                    "receiver_vtable_id": base["receiver_vtable_id"],
                    "dispatch_slot_offset": slot,
                    "receiver_class_name": base["class_name"],
                    "receiver_class_match_kind": base["class_match_kind"],
                    "receiver_class_evidence_source": base["class_evidence_source"],
                    "receiver_type_name": base["receiver_type_name"],
                    "receiver_type_display": base["receiver_type_display"],
                    "runtime_validation_status": base["runtime_validation_status"],
                    "evidence_ref": f"{source_id}#call={call_index}",
                    "affects_routing": False,
                })

    write_jsonl(args.output, output)
    write_jsonl(args.queries_output, queries)

    print("format:", OUTPUT_FORMAT)
    print("analysis functions:", len(analysis_rows))
    print("receiver type rows:", len(receiver_rows))
    print("parameter provenance rows:", len(parameter_provenance))
    print("calls considered:", len(output))
    print("resolved unique vtable:", counts["resolved_unique_vtable"])
    print("resolved unique vtable runtime match:", counts["resolved_unique_vtable_runtime_match"])
    print("resolved via transparent type flow:", counts["resolved_via_transparent_type_flow"])
    print("resolved via parameter caller type:", counts["resolved_via_parameter_caller_type"])
    print("runtime mismatch blocked:", counts["runtime_mismatch_blocked"])
    print("ambiguous class vtable:", counts["ambiguous_class_vtable"])
    print("ambiguous receiver class:", counts["ambiguous_receiver_class"])
    print("unresolved receiver class:", counts["unresolved_receiver_class"])
    print("receiver not single pointer:", counts["receiver_not_single_pointer"])
    print("missing receiver type evidence:", counts["missing_receiver_type_evidence"])
    print("missing slot offset:", counts["missing_slot_offset"])
    print("queries emitted:", len(queries))
    print("affects routing: no")
    print("output:", args.output)
    print("queries output:", args.queries_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
