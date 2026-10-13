#!/usr/bin/env python3
"""Build a conservative class-aware dispatch catalog from runtime receiver keys.

The catalog joins verified receiver-vtable@slot observations with statically resolved
vtable class provenance. It never transfers a target across a different receiver
vtable or class, never votes across conflicts, and never changes routing.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.ClassDispatchCatalog/1"


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receiver_keys", type=Path)
    parser.add_argument("vtable_provenance", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    key_rows = read_jsonl(args.receiver_keys)
    provenance_rows = read_jsonl(args.vtable_provenance)
    provenance_by_vtable = {
        row.get("receiver_vtable_id"): row
        for row in provenance_rows
        if isinstance(row.get("receiver_vtable_id"), str)
    }

    out: list[dict[str, Any]] = []
    for key in key_rows:
        vtable_id = key.get("receiver_vtable_id")
        dispatch_key = key.get("receiver_dispatch_key")
        slot_offset = key.get("dispatch_slot_offset")
        targets = key.get("target_ids")
        key_status = key.get("status")
        if not isinstance(vtable_id, str) or not isinstance(dispatch_key, str):
            raise ValueError("receiver key missing identity")
        if not isinstance(targets, list) or not all(isinstance(x, str) for x in targets):
            raise ValueError(f"{dispatch_key}: invalid target_ids")

        provenance = provenance_by_vtable.get(vtable_id)
        class_name = provenance.get("resolved_class_name") if provenance else None
        class_status = provenance.get("status") if provenance else "missing_vtable_provenance"

        if key_status == "conflict" or len(targets) > 1:
            catalog_status = "runtime_conflict"
        elif not isinstance(class_name, str) or not class_name:
            catalog_status = "class_unresolved"
        elif len(targets) == 1:
            catalog_status = "verified_single_target"
        else:
            catalog_status = "target_unobserved"

        class_slot_key = None
        if isinstance(class_name, str) and class_name and isinstance(slot_offset, str):
            class_slot_key = f"{class_name}@slot={slot_offset}"

        out.append({
            "format": FORMAT,
            "receiver_dispatch_key": dispatch_key,
            "receiver_vtable_id": vtable_id,
            "resolved_class_name": class_name,
            "class_resolution_status": class_status,
            "dispatch_slot_offset": slot_offset,
            "class_slot_key": class_slot_key,
            "slot_group_ids": key.get("slot_group_ids"),
            "receiver_key_status": key_status,
            "observation_count": key.get("observation_count"),
            "source_call_count": key.get("source_call_count"),
            "target_ids": targets,
            "target_count": len(targets),
            "catalog_status": catalog_status,
            "target_transfer_scope": "same_receiver_vtable_only",
            "cross_vtable_transfer_allowed": False,
            "cross_class_transfer_allowed": False,
            "class_inferred_from_target_name": False,
        })

    out.sort(key=lambda row: row["receiver_dispatch_key"])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in out:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    print("format:", FORMAT)
    print("catalog entries:", len(out))
    print("verified single target:", sum(x["catalog_status"] == "verified_single_target" for x in out))
    print("runtime conflicts:", sum(x["catalog_status"] == "runtime_conflict" for x in out))
    print("class unresolved:", sum(x["catalog_status"] == "class_unresolved" for x in out))
    print("cross-vtable transfer allowed: no")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
