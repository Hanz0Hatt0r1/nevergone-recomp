#!/usr/bin/env python3
"""Analyze verified runtime dispatch targets by receiver-vtable provenance.

This evidence layer does not infer class names and does not modify routing. It asks
whether a structural dispatch slot becomes stable after partitioning observations by
the concrete receiver vtable observed at runtime.
"""
from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path
from typing import Any

OBS_FORMAT = "NEVERGONE.DispatchReceiverObservation/1"
KEY_FORMAT = "NEVERGONE.DispatchReceiverKey/1"
SLOT_FORMAT = "NEVERGONE.DispatchReceiverSlot/1"


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("seeds", type=Path)
    parser.add_argument("--observations-output", type=Path, required=True)
    parser.add_argument("--keys-output", type=Path, required=True)
    parser.add_argument("--slots-output", type=Path, required=True)
    args = parser.parse_args()

    seeds = read_jsonl(args.seeds)
    observations: list[dict[str, Any]] = []
    missing_provenance = 0

    for seed in seeds:
        if seed.get("verified") is not True:
            continue
        target_id = seed.get("target_id")
        source_id = seed.get("source_id")
        call_index = seed.get("call_index")
        slot_group_id = seed.get("slot_group_id")
        vtable_id = seed.get("receiver_vtable_id")
        dispatch_key = seed.get("receiver_dispatch_key")
        status = seed.get("receiver_provenance_status")

        if not isinstance(target_id, str) or not isinstance(source_id, str) or not isinstance(call_index, int):
            raise ValueError("invalid verified seed identity")
        if not isinstance(slot_group_id, str):
            raise ValueError(f"{source_id}#{call_index}: missing slot_group_id")

        if not isinstance(vtable_id, str) or not isinstance(dispatch_key, str):
            missing_provenance += 1
            continue

        observations.append(
            {
                "format": OBS_FORMAT,
                "source_id": source_id,
                "call_index": call_index,
                "call_address": seed.get("call_address"),
                "slot_group_id": slot_group_id,
                "ordered_group_id": seed.get("ordered_group_id"),
                "ordered_position": seed.get("ordered_position"),
                "receiver_vtable_id": vtable_id,
                "receiver_vtable_ghidra_address": seed.get("receiver_vtable_ghidra_address"),
                "dispatch_slot_offset": seed.get("dispatch_slot_offset"),
                "receiver_dispatch_key": dispatch_key,
                "target_id": target_id,
                "receiver_slot_matches_blx_target": seed.get("receiver_slot_matches_blx_target"),
                "receiver_provenance_status": status,
                "evidence_ref": seed.get("evidence_ref"),
            }
        )

    by_key: dict[str, list[dict[str, Any]]] = defaultdict(list)
    by_slot: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for obs in observations:
        by_key[obs["receiver_dispatch_key"]].append(obs)
        by_slot[obs["slot_group_id"]].append(obs)

    key_rows: list[dict[str, Any]] = []
    key_status_by_name: dict[str, str] = {}
    for dispatch_key in sorted(by_key):
        items = by_key[dispatch_key]
        targets = sorted({x["target_id"] for x in items})
        sources = sorted({(x["source_id"], x["call_index"]) for x in items})
        slot_groups = sorted({x["slot_group_id"] for x in items})
        if len(targets) > 1:
            status = "conflict"
        elif len(items) > 1:
            status = "consistent"
        else:
            status = "single_observation"
        key_status_by_name[dispatch_key] = status
        key_rows.append(
            {
                "format": KEY_FORMAT,
                "receiver_dispatch_key": dispatch_key,
                "receiver_vtable_id": items[0]["receiver_vtable_id"],
                "dispatch_slot_offset": items[0].get("dispatch_slot_offset"),
                "slot_group_ids": slot_groups,
                "observation_count": len(items),
                "source_call_count": len(sources),
                "target_ids": targets,
                "target_count": len(targets),
                "status": status,
                "observations": items,
            }
        )

    slot_rows: list[dict[str, Any]] = []
    for slot_group_id in sorted(by_slot):
        items = by_slot[slot_group_id]
        vtables = sorted({x["receiver_vtable_id"] for x in items})
        targets = sorted({x["target_id"] for x in items})
        dispatch_keys = sorted({x["receiver_dispatch_key"] for x in items})
        key_conflict = any(key_status_by_name[k] == "conflict" for k in dispatch_keys)

        target_sets_by_vtable: dict[str, list[str]] = {}
        for vtable in vtables:
            target_sets_by_vtable[vtable] = sorted(
                {x["target_id"] for x in items if x["receiver_vtable_id"] == vtable}
            )

        if key_conflict:
            status = "receiver_ambiguous"
        elif len(vtables) == 1:
            status = "single_receiver"
        elif len(targets) == 1:
            status = "shared_target_across_receivers"
        else:
            status = "receiver_partitioned"

        slot_rows.append(
            {
                "format": SLOT_FORMAT,
                "slot_group_id": slot_group_id,
                "observation_count": len(items),
                "receiver_vtable_ids": vtables,
                "receiver_vtable_count": len(vtables),
                "target_ids": targets,
                "target_count": len(targets),
                "receiver_dispatch_keys": dispatch_keys,
                "target_ids_by_receiver_vtable": target_sets_by_vtable,
                "status": status,
            }
        )

    write_jsonl(args.observations_output, observations)
    write_jsonl(args.keys_output, key_rows)
    write_jsonl(args.slots_output, slot_rows)

    print("observations:", len(observations))
    print("verified seeds missing receiver provenance:", missing_provenance)
    print("receiver dispatch keys:", len(key_rows))
    print("receiver-key conflicts:", sum(x["status"] == "conflict" for x in key_rows))
    print("structural slots:", len(slot_rows))
    print("receiver-partitioned slots:", sum(x["status"] == "receiver_partitioned" for x in slot_rows))
    print("observations output:", args.observations_output)
    print("keys output:", args.keys_output)
    print("slots output:", args.slots_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
