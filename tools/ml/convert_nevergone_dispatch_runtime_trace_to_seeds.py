#!/usr/bin/env python3
"""Convert dispatch runtime observations into natural seeds and intervention evidence.

Input may be a mixed harness log. Natural `dispatch_runtime_target` observations
become `NEVERGONE.DispatchTargetSeed/1` rows. Dispatches reached after a bounded
fallback mutation are never eligible for natural verified seeds; they can be
preserved separately as `NEVERGONE.DispatchInterventionalRuntimeEvidence/1`.

New tracer rows carry explicit `evidence_origin` metadata. Older logs are also
handled conservatively: after an `action_data_fallback_return_override` event for a
source, later dispatch rows for that source are classified as interventional until
the corresponding `action_data_create_entry` closes the workload.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.DispatchTargetSeed/1"
INTERVENTIONAL_FORMAT = "NEVERGONE.DispatchInterventionalRuntimeEvidence/1"
NATURAL_ORIGIN = "natural_runtime"
INTERVENTIONAL_ORIGIN = "interventional_fallback"


def iter_evidence(path: Path):
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line_no, raw in enumerate(handle, 1):
            line = raw.strip()
            if line.startswith("EVIDENCE "):
                line = line[len("EVIDENCE ") :]
            elif not line.startswith("{"):
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(row, dict):
                row["_line_no"] = line_no
                yield row


def classify_dispatches(path: Path) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    natural: list[dict[str, Any]] = []
    interventional: list[dict[str, Any]] = []
    active_interventions: dict[str, dict[str, Any]] = {}

    for row in iter_evidence(path):
        kind = row.get("kind")
        source_id = row.get("source_id")

        if kind == "action_data_fallback_return_override" and isinstance(source_id, str):
            active_interventions[source_id] = {
                "kind": "forced_r0_zero_after_first_virtual_call_return",
                "evidence_ref": f"{path.name}:line:{row['_line_no']}",
            }
            continue

        if kind == "action_data_create_entry" and isinstance(source_id, str):
            active_interventions.pop(source_id, None)
            continue

        if kind != "dispatch_runtime_target":
            continue

        explicit_origin = row.get("evidence_origin")
        explicit_eligible = row.get("eligible_for_natural_seed")
        legacy_intervention = active_interventions.get(source_id) if isinstance(source_id, str) else None

        is_interventional = (
            explicit_origin == INTERVENTIONAL_ORIGIN
            or explicit_eligible is False
            or legacy_intervention is not None
        )
        copy = dict(row)
        if is_interventional:
            copy["_classified_origin"] = INTERVENTIONAL_ORIGIN
            if not copy.get("intervention_kind"):
                copy["intervention_kind"] = (
                    legacy_intervention or {}
                ).get("kind", "forced_r0_zero_after_first_virtual_call_return")
            if not copy.get("intervention_evidence_ref") and legacy_intervention:
                copy["intervention_evidence_ref"] = legacy_intervention.get("evidence_ref")
            interventional.append(copy)
        else:
            copy["_classified_origin"] = NATURAL_ORIGIN
            natural.append(copy)

    return natural, interventional


def validate_target_row(row: dict[str, Any]) -> tuple[str, int, str]:
    source_id = row.get("source_id")
    call_index = row.get("call_index")
    target_id = row.get("target_id")
    line_no = row.get("_line_no")
    if not isinstance(source_id, str) or not source_id:
        raise ValueError(f"trace line {line_no}: missing source_id")
    if not isinstance(call_index, int):
        raise ValueError(f"trace line {line_no}: call_index must be int")
    if not isinstance(target_id, str) or not target_id:
        raise ValueError(f"trace line {line_no}: missing target_id")
    return source_id, call_index, target_id


def natural_seed(row: dict[str, Any], trace_name: str) -> dict[str, Any]:
    source_id, call_index, target_id = validate_target_row(row)
    return {
        "format": FORMAT,
        "source_id": source_id,
        "call_index": call_index,
        "call_address": row.get("call_address"),
        "slot_group_id": row.get("slot_group_id"),
        "ordered_position": row.get("ordered_position"),
        "ordered_group_id": row.get("ordered_group_id"),
        "ordered_group_size": row.get("ordered_group_size"),
        "target_id": target_id,
        "verified": True,
        "evidence_kind": "unidbg_runtime_blx_target",
        "evidence_origin": NATURAL_ORIGIN,
        "eligible_for_natural_seed": True,
        "natural_path_reachability_verified": True,
        "evidence_ref": f"{trace_name}:line:{row['_line_no']}",
        "target_module": row.get("target_module"),
        "target_module_offset": row.get("target_module_offset"),
        "target_thumb": row.get("target_thumb"),
        "receiver_pointer": row.get("receiver_pointer"),
        "receiver_vtable_id": row.get("receiver_vtable_id"),
        "receiver_vtable_module": row.get("receiver_vtable_module"),
        "receiver_vtable_module_offset": row.get("receiver_vtable_module_offset"),
        "receiver_vtable_ghidra_address": row.get("receiver_vtable_ghidra_address"),
        "dispatch_slot_offset": row.get("dispatch_slot_offset"),
        "receiver_dispatch_key": row.get("receiver_dispatch_key"),
        "receiver_slot_target_raw": row.get("receiver_slot_target_raw"),
        "receiver_slot_matches_blx_target": row.get("receiver_slot_matches_blx_target"),
        "receiver_provenance_status": row.get("receiver_provenance_status"),
    }


def interventional_row(row: dict[str, Any], trace_name: str) -> dict[str, Any]:
    source_id, call_index, target_id = validate_target_row(row)
    return {
        "format": INTERVENTIONAL_FORMAT,
        "source_id": source_id,
        "call_index": call_index,
        "call_address": row.get("call_address"),
        "slot_group_id": row.get("slot_group_id"),
        "ordered_position": row.get("ordered_position"),
        "ordered_group_id": row.get("ordered_group_id"),
        "ordered_group_size": row.get("ordered_group_size"),
        "target_id": target_id,
        "runtime_blx_target_verified": True,
        "evidence_origin": INTERVENTIONAL_ORIGIN,
        "eligible_for_natural_seed": False,
        "natural_path_reachability_verified": False,
        "intervention_kind": row.get("intervention_kind") or "forced_r0_zero_after_first_virtual_call_return",
        "intervention_source_id": row.get("intervention_source_id") or source_id,
        "intervention_evidence_ref": row.get("intervention_evidence_ref"),
        "evidence_ref": f"{trace_name}:line:{row['_line_no']}",
        "target_module": row.get("target_module"),
        "target_module_offset": row.get("target_module_offset"),
        "target_thumb": row.get("target_thumb"),
        "receiver_pointer": row.get("receiver_pointer"),
        "receiver_vtable_id": row.get("receiver_vtable_id"),
        "receiver_vtable_module": row.get("receiver_vtable_module"),
        "receiver_vtable_module_offset": row.get("receiver_vtable_module_offset"),
        "receiver_vtable_ghidra_address": row.get("receiver_vtable_ghidra_address"),
        "dispatch_slot_offset": row.get("dispatch_slot_offset"),
        "receiver_dispatch_key": row.get("receiver_dispatch_key"),
        "receiver_slot_target_raw": row.get("receiver_slot_target_raw"),
        "receiver_slot_matches_blx_target": row.get("receiver_slot_matches_blx_target"),
        "receiver_provenance_status": row.get("receiver_provenance_status"),
        "receiver_slot_binding_verified": row.get("receiver_slot_matches_blx_target") is True,
    }


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace_log", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--interventional-output", type=Path)
    args = parser.parse_args()

    natural_observations, interventional_observations = classify_dispatches(args.trace_log)

    seeds_by_key: dict[tuple[str, int, str, str], dict[str, Any]] = {}
    site_targets: dict[tuple[str, int], set[str]] = defaultdict(set)
    for row in natural_observations:
        source_id, call_index, target_id = validate_target_row(row)
        receiver_vtable_id = row.get("receiver_vtable_id")
        receiver_key = receiver_vtable_id if isinstance(receiver_vtable_id, str) else ""
        key = (source_id, call_index, target_id, receiver_key)
        site_targets[(source_id, call_index)].add(target_id)
        seeds_by_key.setdefault(key, natural_seed(row, args.trace_log.name))

    seeds = sorted(
        seeds_by_key.values(),
        key=lambda row: (
            row["source_id"],
            row["call_index"],
            str(row.get("receiver_vtable_id") or ""),
            row["target_id"],
        ),
    )
    write_jsonl(args.output, seeds)

    interventional = [interventional_row(row, args.trace_log.name) for row in interventional_observations]
    interventional.sort(
        key=lambda row: (
            row["source_id"],
            row["call_index"],
            str(row.get("receiver_vtable_id") or ""),
            row["target_id"],
        )
    )
    if args.interventional_output is not None:
        write_jsonl(args.interventional_output, interventional)

    conflicts = {key: values for key, values in site_targets.items() if len(values) > 1}
    target_counts = Counter(seed["target_id"] for seed in seeds)
    receiver_counts = Counter(
        str(seed.get("receiver_vtable_id"))
        for seed in seeds
        if isinstance(seed.get("receiver_vtable_id"), str)
    )
    provenance_complete = sum(
        seed.get("receiver_provenance_status") == "verified_slot_match"
        for seed in seeds
    )
    interventional_binding_verified = sum(
        row.get("receiver_slot_binding_verified") is True for row in interventional
    )

    print("format:", FORMAT)
    print("natural runtime observations:", len(natural_observations))
    print("interventional observations excluded from natural seeds:", len(interventional_observations))
    print("unique verified natural seeds:", len(seeds))
    print("observed natural call sites:", len(site_targets))
    print("multi-target natural call sites:", len(conflicts))
    print("receiver-provenance verified natural seeds:", provenance_complete)
    print("interventional receiver-slot bindings verified:", interventional_binding_verified)
    print("receiver vtables:", dict(receiver_counts.most_common()))
    print("targets:", dict(target_counts.most_common()))
    print("output:", args.output)
    if args.interventional_output is not None:
        print("interventional format:", INTERVENTIONAL_FORMAT)
        print("interventional output:", args.interventional_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
