#!/usr/bin/env python3
"""Audit unresolved static receiver-vtable inference failures.

This diagnostic tool joins Ghidra receiver-type TSV rows to
NEVERGONE.StaticReceiverVtableInference/1 output and summarizes why calls remain
unresolved. It is evidence-only and never changes routing.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match"}


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


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


def parse_int(value: Any) -> int:
    if value in (None, "", "-"):
        return 0
    try:
        return int(str(value), 0)
    except ValueError:
        return 0


def token_def_mnemonic(token: Any) -> str:
    if not isinstance(token, str) or not token:
        return "missing"
    m = re.match(r"def:([^@]+)@", token)
    if m:
        return m.group(1)
    return token.split(":", 1)[0]


def clean_type(value: Any) -> str:
    if not isinstance(value, str) or value in ("", "-"):
        return "<missing>"
    return re.sub(r"\s+", " ", value.strip())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receiver_types_tsv", type=Path)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    parser.add_argument("--samples", type=int, default=40)
    args = parser.parse_args()

    receiver_rows = read_tsv(args.receiver_types_tsv)
    inference_rows = read_jsonl(args.inference_jsonl)

    receiver_by_key: dict[tuple[str, int], dict[str, str]] = {}
    for row in receiver_rows:
        source_id = row.get("source_id")
        try:
            call_index = int(row.get("call_index", "-1"))
        except ValueError:
            continue
        if source_id and call_index >= 0:
            receiver_by_key[(source_id, call_index)] = row

    joined: list[dict[str, Any]] = []
    statuses = Counter()
    mnemonics = Counter()
    pointer_depths = Counter()
    direct_types = Counter()
    pointee_types = Counter()
    flow_nodes = Counter()
    storage_kinds = Counter()

    for inf in inference_rows:
        if inf.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"expected {INFERENCE_FORMAT}")
        status = inf.get("status")
        if status in RESOLVED:
            continue
        source_id = inf.get("source_id")
        call_index = inf.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        rec = receiver_by_key.get((source_id, call_index), {})
        token = rec.get("receiver_token")
        mnemonic = token_def_mnemonic(token)
        depth = parse_int(rec.get("receiver_pointer_depth"))
        direct_type = clean_type(rec.get("receiver_type_display"))
        pointee_type = clean_type(rec.get("pointee_type_display"))
        flow_count = parse_int(rec.get("flow_type_node_count"))
        storage = rec.get("receiver_storage_kind") or "<missing>"

        statuses[str(status)] += 1
        mnemonics[mnemonic] += 1
        pointer_depths[depth] += 1
        direct_types[direct_type] += 1
        pointee_types[pointee_type] += 1
        flow_nodes[flow_count] += 1
        storage_kinds[storage] += 1

        joined.append({
            "source_id": source_id,
            "call_index": call_index,
            "call_address": inf.get("call_address"),
            "status": status,
            "dispatch_slot_offset": inf.get("dispatch_slot_offset"),
            "receiver_storage_kind": storage,
            "receiver_token": token,
            "receiver_def_mnemonic": mnemonic,
            "receiver_type_display": rec.get("receiver_type_display"),
            "receiver_pointer_depth": depth,
            "pointee_type_display": rec.get("pointee_type_display"),
            "flow_type_node_count": flow_count,
            "flow_single_pointer_pointee_names_json": rec.get("flow_single_pointer_pointee_names_json"),
            "flow_single_pointer_pointee_displays_json": rec.get("flow_single_pointer_pointee_displays_json"),
            "candidate_receiver_class_names": inf.get("candidate_receiver_class_names") or [],
        })

    print("format: NEVERGONE.StaticReceiverTypeFailureAudit/1")
    print("receiver rows:", len(receiver_rows))
    print("inference rows:", len(inference_rows))
    print("unresolved rows:", len(joined))
    print("status counts:", dict(statuses.most_common()))
    print("receiver def mnemonics:", dict(mnemonics.most_common()))
    print("receiver storage kinds:", dict(storage_kinds.most_common()))
    print("pointer depths:", dict(sorted(pointer_depths.items())))
    print("flow node counts:", dict(sorted(flow_nodes.items())))
    print("top receiver types:", dict(direct_types.most_common(20)))
    print("top pointee types:", dict(pointee_types.most_common(20)))

    print("\nsamples:")
    for row in joined[: max(0, args.samples)]:
        print(
            row["source_id"],
            "call", row["call_index"],
            "status=", row["status"],
            "slot=", row["dispatch_slot_offset"],
            "def=", row["receiver_def_mnemonic"],
            "depth=", row["receiver_pointer_depth"],
            "type=", clean_type(row["receiver_type_display"]),
            "pointee=", clean_type(row["pointee_type_display"]),
            "flow_nodes=", row["flow_type_node_count"],
        )

    if args.details_output is not None:
        args.details_output.parent.mkdir(parents=True, exist_ok=True)
        with args.details_output.open("w", encoding="utf-8") as handle:
            for row in joined:
                handle.write(json.dumps(row, sort_keys=True) + "\n")
        print("details output:", args.details_output)

    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
