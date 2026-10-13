#!/usr/bin/env python3
"""Convert unidbg dispatch runtime observations into verified target seed JSONL.

Input may be a mixed harness log. Only `EVIDENCE` rows with kind
`dispatch_runtime_target` are consumed. Multiple runtime targets observed at the
same source call site are deliberately preserved as separate verified seeds so the
propagation layer can surface a conflict instead of choosing one. Receiver/vtable
provenance is preserved when present; different receiver vtables at the same site
remain distinct observations even when they resolve to the same target.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.DispatchTargetSeed/1"


def read_evidence(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
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
            if isinstance(row, dict) and row.get("kind") == "dispatch_runtime_target":
                row["_line_no"] = line_no
                rows.append(row)
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace_log", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    observations = read_evidence(args.trace_log)
    seeds_by_key: dict[tuple[str, int, str, str], dict[str, Any]] = {}
    site_targets: dict[tuple[str, int], set[str]] = defaultdict(set)
    for row in observations:
        source_id = row.get("source_id")
        call_index = row.get("call_index")
        target_id = row.get("target_id")
        if not isinstance(source_id, str) or not source_id:
            raise ValueError(f"trace line {row['_line_no']}: missing source_id")
        if not isinstance(call_index, int):
            raise ValueError(f"trace line {row['_line_no']}: call_index must be int")
        if not isinstance(target_id, str) or not target_id:
            raise ValueError(f"trace line {row['_line_no']}: missing target_id")

        receiver_vtable_id = row.get("receiver_vtable_id")
        receiver_key = receiver_vtable_id if isinstance(receiver_vtable_id, str) else ""
        key = (source_id, call_index, target_id, receiver_key)
        site_targets[(source_id, call_index)].add(target_id)
        seeds_by_key.setdefault(
            key,
            {
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
                "evidence_ref": f"{args.trace_log.name}:line:{row['_line_no']}",
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
            },
        )

    seeds = sorted(
        seeds_by_key.values(),
        key=lambda row: (
            row["source_id"],
            row["call_index"],
            str(row.get("receiver_vtable_id") or ""),
            row["target_id"],
        ),
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for seed in seeds:
            handle.write(json.dumps(seed, sort_keys=True) + "\n")

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
    print("format:", FORMAT)
    print("runtime observations:", len(observations))
    print("unique verified seeds:", len(seeds))
    print("observed call sites:", len(site_targets))
    print("multi-target call sites:", len(conflicts))
    print("receiver-provenance verified seeds:", provenance_complete)
    print("receiver vtables:", dict(receiver_counts.most_common()))
    print("targets:", dict(target_counts.most_common()))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
