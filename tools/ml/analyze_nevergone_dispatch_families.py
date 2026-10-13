#!/usr/bin/env python3
"""Build deterministic structural dispatch families from CALLIND provenance slices.

Families are defined only by normalized provenance ``shape_hash``. Calls within a
family are grouped by the observed arithmetic slot offset. This tool deliberately
avoids semantic claims such as vtable identity or resolved target identity: the
output is structural evidence only and never changes ``final_action``.
"""
from __future__ import annotations

import argparse
import json
import math
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.DispatchFamilyEvidence/1"
FAMILY_FORMAT = "NEVERGONE.DispatchFamilyCatalog/1"


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


def parse_offset(value: Any) -> int | None:
    if value in (None, "", "none", "None"):
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError:
            return None
    return None


def format_offset(value: int | None) -> str:
    return "none" if value is None else f"0x{value:x}"


def gcd_stride(offsets: list[int]) -> int | None:
    values = sorted(set(offsets))
    if len(values) < 2:
        return None
    stride = 0
    for a, b in zip(values, values[1:]):
        delta = b - a
        if delta > 0:
            stride = math.gcd(stride, delta)
    return stride or None


def family_kind(call_count: int, offsets: list[int | None]) -> str:
    distinct = set(offsets)
    if call_count <= 1:
        return "singleton_shape"
    if len(distinct) <= 1:
        return "repeated_shape_single_slot"
    return "repeated_shape_multi_slot"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--families-output", type=Path)
    parser.add_argument("--member-limit", type=int, default=100)
    args = parser.parse_args()
    if args.member_limit < 1:
        raise SystemExit("--member-limit must be positive")

    rows = read_jsonl(args.analysis)
    if not rows:
        raise SystemExit("analysis input is empty")

    calls: list[dict[str, Any]] = []
    for row in rows:
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("analysis row missing id")
        evidence = row.get("indirect_call_slice_evidence")
        if not isinstance(evidence, dict) or not evidence.get("present"):
            continue
        call_rows = evidence.get("calls")
        if not isinstance(call_rows, list):
            continue
        for call in call_rows:
            if not isinstance(call, dict):
                continue
            shape = call.get("shape_hash")
            if not isinstance(shape, str) or not shape:
                continue
            offset = parse_offset(call.get("primary_slot_offset_candidate"))
            calls.append(
                {
                    "id": record_id,
                    "final_action": row.get("final_action"),
                    "call_index": call.get("call_index"),
                    "call_address": call.get("call_address"),
                    "shape_hash": shape,
                    "offset": offset,
                    "offset_text": format_offset(offset),
                    "node_count": call.get("node_count"),
                    "complete": bool(call.get("complete")),
                }
            )

    by_shape: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for call in calls:
        by_shape[call["shape_hash"]].append(call)

    catalog_rows: list[dict[str, Any]] = []
    membership: dict[str, list[dict[str, Any]]] = defaultdict(list)
    signature_counts: Counter[str] = Counter()

    for shape in sorted(by_shape):
        members = sorted(
            by_shape[shape],
            key=lambda item: (
                item["offset"] is None,
                item["offset"] if item["offset"] is not None else 0,
                item["id"],
                str(item["call_address"]),
                int(item["call_index"] or 0),
            ),
        )
        offsets = [item["offset"] for item in members]
        numeric_offsets = [value for value in offsets if value is not None]
        offset_counts = Counter(item["offset_text"] for item in members)
        functions = sorted({item["id"] for item in members})
        actions = Counter(str(item.get("final_action")) for item in members)
        kind = family_kind(len(members), offsets)
        family_id = f"shape:{shape}"

        slot_groups: list[dict[str, Any]] = []
        by_offset: dict[str, list[dict[str, Any]]] = defaultdict(list)
        for item in members:
            by_offset[item["offset_text"]].append(item)
        for offset_text in sorted(
            by_offset,
            key=lambda token: (token == "none", parse_offset(token) or 0),
        ):
            group = by_offset[offset_text]
            signature = f"{shape}@{offset_text}"
            signature_counts[signature] += len(group)
            group_functions = sorted({item["id"] for item in group})
            slot_groups.append(
                {
                    "slot_group_id": signature,
                    "offset": offset_text,
                    "call_count": len(group),
                    "function_count": len(group_functions),
                    "functions": group_functions[: args.member_limit],
                    "members_truncated": len(group_functions) > args.member_limit,
                    "call_sites": [
                        {
                            "id": item["id"],
                            "call_index": item["call_index"],
                            "call_address": item["call_address"],
                        }
                        for item in group[: args.member_limit]
                    ],
                    "call_sites_truncated": len(group) > args.member_limit,
                }
            )

        catalog_rows.append(
            {
                "format": FAMILY_FORMAT,
                "family_id": family_id,
                "shape_hash": shape,
                "kind": kind,
                "call_count": len(members),
                "function_count": len(functions),
                "functions": functions[: args.member_limit],
                "functions_truncated": len(functions) > args.member_limit,
                "complete_call_count": sum(int(item["complete"]) for item in members),
                "distinct_slot_count": len(set(offsets)),
                "slot_offset_counts": dict(sorted(offset_counts.items())),
                "slot_stride_gcd": format_offset(gcd_stride(numeric_offsets)) if gcd_stride(numeric_offsets) is not None else None,
                "final_action_counts": dict(sorted(actions.items())),
                "slot_groups": slot_groups,
            }
        )

        family_summary = {
            "family_id": family_id,
            "shape_hash": shape,
            "kind": kind,
            "family_call_count": len(members),
            "family_function_count": len(functions),
            "distinct_slot_count": len(set(offsets)),
            "slot_offsets": sorted(set(item["offset_text"] for item in members)),
            "slot_stride_gcd": format_offset(gcd_stride(numeric_offsets)) if gcd_stride(numeric_offsets) is not None else None,
        }
        for item in members:
            membership[item["id"]].append(
                {
                    **family_summary,
                    "call_index": item["call_index"],
                    "call_address": item["call_address"],
                    "slot_group_id": f"{shape}@{item['offset_text']}",
                    "slot_offset": item["offset_text"],
                }
            )

    output_rows: list[dict[str, Any]] = []
    for source in rows:
        record_id = source["id"]
        before = source.get("final_action")
        enriched = dict(source)
        entries = sorted(
            membership.get(record_id, []),
            key=lambda item: (item["shape_hash"], item["slot_offset"], int(item["call_index"] or 0)),
        )
        enriched["dispatch_family_evidence"] = {
            "format": FORMAT,
            "present": bool(entries),
            "membership_count": len(entries),
            "family_ids": sorted({item["family_id"] for item in entries}),
            "slot_group_ids": sorted({item["slot_group_id"] for item in entries}),
            "memberships": entries,
        }
        if enriched.get("final_action") != before:
            raise AssertionError(f"{record_id}: final_action changed")
        output_rows.append(enriched)

    write_jsonl(args.output, output_rows)
    families_output = args.families_output or args.output.with_suffix(".families.jsonl")
    write_jsonl(families_output, sorted(catalog_rows, key=lambda item: (-item["call_count"], item["shape_hash"])))

    family_kind_counts = Counter(item["kind"] for item in catalog_rows)
    repeated_slots = sum(1 for count in signature_counts.values() if count > 1)
    print("format:", FORMAT)
    print("functions:", len(output_rows))
    print("slice calls:", len(calls))
    print("dispatch families:", len(catalog_rows))
    print("family kinds:", dict(family_kind_counts))
    print("repeated slot groups:", repeated_slots)
    print("unique slot groups:", len(signature_counts))
    print("final_action unchanged: yes")
    print("output:", args.output)
    print("families output:", families_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
