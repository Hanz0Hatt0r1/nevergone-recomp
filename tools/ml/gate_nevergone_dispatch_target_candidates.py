#!/usr/bin/env python3
"""Gate structural dispatch target candidates with direct runtime consistency evidence.

This is deliberately conservative. A runtime conflict for one exact ordered
(group, position) proves that structural equivalence does not determine one
concrete target there. In that case:

* cross-function exact_ordered_position proposals are removed for that position;
* same_slot_group proposals are removed globally for that slot group;
* directly observed verified targets remain attached only to their own call site;
* unobserved peers receive no target and are marked blocked_runtime_conflict.

The tool never changes final_action and never resolves a target by voting.
"""
from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.DispatchTargetRuntimeGate/1"
CANDIDATE_FORMAT = "NEVERGONE.DispatchTargetCandidate/1"


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


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def key(source_id: str, call_index: int) -> tuple[str, int]:
    return source_id, call_index


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis_with_candidates", type=Path)
    parser.add_argument("runtime_positions", type=Path)
    parser.add_argument("verified_seeds", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--candidates-output", type=Path, required=True)
    parser.add_argument("--transferability-output", type=Path)
    args = parser.parse_args()

    analysis = read_jsonl(args.analysis_with_candidates)
    positions = read_jsonl(args.runtime_positions)
    seeds = read_jsonl(args.verified_seeds)

    conflict_call_keys: set[tuple[str, int]] = set()
    conflict_slot_groups: set[str] = set()
    conflict_positions: list[dict[str, Any]] = []

    for row in positions:
        if row.get("status") != "conflict":
            continue
        slot = row.get("slot_group_id")
        if not isinstance(slot, str) or not slot:
            raise ValueError("conflict runtime position missing slot_group_id")
        conflict_slot_groups.add(slot)
        observations = row.get("observations")
        if not isinstance(observations, list):
            raise ValueError("conflict runtime position missing observations")
        call_keys: list[tuple[str, int]] = []
        for obs in observations:
            if not isinstance(obs, dict):
                continue
            source_id = obs.get("source_id")
            call_index = obs.get("call_index")
            if not isinstance(source_id, str) or not isinstance(call_index, int):
                continue
            item = key(source_id, call_index)
            conflict_call_keys.add(item)
            call_keys.append(item)
        conflict_positions.append(
            {
                "ordered_group_id": row.get("ordered_group_id"),
                "ordered_position": row.get("ordered_position"),
                "slot_group_id": slot,
                "target_ids": list(row.get("target_ids") or []),
                "call_keys": call_keys,
            }
        )

    verified_by_call: dict[tuple[str, int], set[str]] = defaultdict(set)
    for seed in seeds:
        if seed.get("verified") is not True:
            continue
        source_id = seed.get("source_id")
        call_index = seed.get("call_index")
        target_id = seed.get("target_id")
        if not isinstance(source_id, str) or not isinstance(call_index, int) or not isinstance(target_id, str):
            raise ValueError("invalid verified seed")
        verified_by_call[key(source_id, call_index)].add(target_id)

    original_candidates: list[dict[str, Any]] = []
    for row in analysis:
        evidence = row.get("dispatch_target_candidate_evidence")
        if not isinstance(evidence, dict):
            continue
        calls = evidence.get("calls")
        if isinstance(calls, list):
            original_candidates.extend(x for x in calls if isinstance(x, dict))

    original_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for candidate in original_candidates:
        source_id = candidate.get("id")
        call_index = candidate.get("call_index")
        if isinstance(source_id, str) and isinstance(call_index, int):
            original_by_key[key(source_id, call_index)] = candidate

    # Include conflict-position peers even when an upstream propagation pass emitted
    # no row for them, so blocked/unobserved state is explicit.
    all_keys = set(original_by_key) | conflict_call_keys | set(verified_by_call)
    gated_rows: list[dict[str, Any]] = []

    for call_key in sorted(all_keys):
        source_id, call_index = call_key
        original = original_by_key.get(call_key, {})
        slot = original.get("slot_group_id")
        if not isinstance(slot, str):
            for position in conflict_positions:
                if call_key in position["call_keys"]:
                    slot = position["slot_group_id"]
                    break

        original_evidence = original.get("evidence")
        if not isinstance(original_evidence, list):
            original_evidence = []

        retained: list[dict[str, Any]] = []
        removed: list[dict[str, Any]] = []
        for item in original_evidence:
            if not isinstance(item, dict):
                continue
            basis = item.get("basis")
            seed_source = item.get("seed_source_id")
            seed_call_index = item.get("seed_call_index")
            direct_same_call = seed_source == source_id and seed_call_index == call_index

            block = False
            reason = None
            if slot in conflict_slot_groups and basis == "same_slot_group" and not direct_same_call:
                block = True
                reason = "runtime_conflict_disproves_global_slot_target_transfer"
            if call_key in conflict_call_keys and basis == "exact_ordered_position" and not direct_same_call:
                block = True
                reason = "runtime_conflict_disproves_exact_position_target_transfer"

            if block:
                removed.append({**item, "gate_reason": reason})
            else:
                retained.append(item)

        direct_targets = sorted(verified_by_call.get(call_key, set()))
        propagated_targets = sorted({
            str(item.get("target_id"))
            for item in retained
            if isinstance(item.get("target_id"), str)
            and not (
                item.get("seed_source_id") == source_id
                and item.get("seed_call_index") == call_index
            )
        })

        if direct_targets:
            status = "verified_local_only" if call_key in conflict_call_keys else "verified_local"
            target_ids = direct_targets
        elif call_key in conflict_call_keys:
            status = "blocked_runtime_conflict"
            target_ids = []
        else:
            target_ids = propagated_targets
            status = "conflict" if len(target_ids) > 1 else ("candidate_only" if target_ids else "no_candidate")

        row = {
            "format": CANDIDATE_FORMAT,
            "runtime_gate_format": FORMAT,
            "id": source_id,
            "call_index": call_index,
            "call_address": original.get("call_address"),
            "slot_group_id": slot,
            "ordered_position": original.get("ordered_position"),
            "candidate_target_ids": target_ids,
            "candidate_target_count": len(target_ids),
            "direct_verified_target_ids": direct_targets,
            "propagated_target_ids": propagated_targets,
            "conflict": len(target_ids) > 1,
            "status": status,
            "runtime_conflict_position": call_key in conflict_call_keys,
            "slot_group_runtime_nontransferable": slot in conflict_slot_groups,
            "evidence": retained,
            "removed_evidence": removed,
            "removed_evidence_count": len(removed),
        }
        gated_rows.append(row)

    gated_by_id: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in gated_rows:
        gated_by_id[row["id"]].append(row)

    output_rows: list[dict[str, Any]] = []
    for source in analysis:
        before = source.get("final_action")
        source_id = source.get("id")
        calls = sorted(gated_by_id.get(source_id, []), key=lambda x: x["call_index"])
        enriched = dict(source)
        enriched["dispatch_target_runtime_gate_evidence"] = {
            "format": FORMAT,
            "present": bool(calls),
            "call_count": len(calls),
            "verified_local_call_count": sum(x["status"].startswith("verified_local") for x in calls),
            "blocked_call_count": sum(x["status"] == "blocked_runtime_conflict" for x in calls),
            "calls": calls,
        }
        if enriched.get("final_action") != before:
            raise AssertionError(f"{source_id}: final_action changed")
        output_rows.append(enriched)

    write_jsonl(args.output, output_rows)
    write_jsonl(args.candidates_output, gated_rows)

    transfer_rows = []
    for slot in sorted(conflict_slot_groups):
        related = [x for x in conflict_positions if x["slot_group_id"] == slot]
        transfer_rows.append(
            {
                "format": FORMAT,
                "slot_group_id": slot,
                "target_transferability": "runtime_disproven",
                "conflict_position_count": len(related),
                "conflict_positions": related,
            }
        )
    if args.transferability_output:
        write_jsonl(args.transferability_output, transfer_rows)

    print("format:", FORMAT)
    print("analysis functions:", len(output_rows))
    print("runtime-conflict positions:", len(conflict_positions))
    print("runtime-nontransferable slot groups:", len(conflict_slot_groups))
    print("verified local call sites:", sum(x["status"].startswith("verified_local") for x in gated_rows))
    print("blocked unobserved call sites:", sum(x["status"] == "blocked_runtime_conflict" for x in gated_rows))
    print("removed propagated evidence:", sum(x["removed_evidence_count"] for x in gated_rows))
    print("final_action unchanged: yes")
    print("output:", args.output)
    print("candidates output:", args.candidates_output)
    if args.transferability_output:
        print("transferability output:", args.transferability_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
