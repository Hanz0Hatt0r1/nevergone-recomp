#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
CONSUMER_FORMAT = "NEVERGONE.GameSceneUIControlsReturnConsumerAudit/1"
CALLER_FORMAT = "NEVERGONE.CallerArgumentSubtreeAudit/1"
OUT_FORMAT = "NEVERGONE.GameSceneUIControlsSlotConvergenceAudit/1"
TARGET_SOURCE = "libcocos2dcpp.so:002a7594"
TARGET_CALLS = [3, 4, 7]
TARGET_SLOT = "0x80"
TARGET_CLASSES = ["GameSceneUI", "Controls"]


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


def one(rows: list[dict[str, Any]], fmt: str, path: Path) -> dict[str, Any]:
    if len(rows) != 1 or rows[0].get("format") != fmt:
        raise ValueError(f"{path}: expected exactly one {fmt} row")
    return rows[0]


def class_slot_rows(catalog: list[dict[str, Any]], cls: str) -> list[dict[str, Any]]:
    return [
        r for r in catalog
        if r.get("class_name") == cls and r.get("dispatch_slot_offset") == TARGET_SLOT
    ]


def target_summary(row: dict[str, Any]) -> dict[str, Any]:
    funcs = row.get("target_functions")
    names: list[str] = []
    if isinstance(funcs, list):
        for f in funcs:
            if isinstance(f, dict) and isinstance(f.get("name"), str):
                names.append(f["name"])
    ids = row.get("candidate_target_ids")
    return {
        "class_name": row.get("class_name"),
        "receiver_vtable_id": row.get("receiver_vtable_id"),
        "dispatch_slot_offset": row.get("dispatch_slot_offset"),
        "static_status": row.get("static_status"),
        "candidate_target_ids": list(ids) if isinstance(ids, list) else [],
        "target_names": sorted(set(names)),
        "runtime_validation_status": row.get("runtime_validation_status"),
    }


def main() -> int:
    p = argparse.ArgumentParser(description="Audit slot 0x80 convergence for mixed GameSceneUI/Controls receiver provenance")
    p.add_argument("catalog_jsonl", type=Path)
    p.add_argument("consumer_audit_jsonl", type=Path)
    p.add_argument("caller_audit_jsonl", type=Path)
    p.add_argument("--details-output", type=Path)
    a = p.parse_args()

    catalog = read_jsonl(a.catalog_jsonl)
    consumer = one(read_jsonl(a.consumer_audit_jsonl), CONSUMER_FORMAT, a.consumer_audit_jsonl)
    caller = read_jsonl(a.caller_audit_jsonl)

    for i, row in enumerate(catalog, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{a.catalog_jsonl}:{i}: expected {CATALOG_FORMAT}")
    for i, row in enumerate(caller, 1):
        if row.get("format") != CALLER_FORMAT:
            raise ValueError(f"{a.caller_audit_jsonl}:{i}: expected {CALLER_FORMAT}")

    reasons: list[str] = []

    consumer_ok = (
        consumer.get("accessor_chain_ok") is True
        and consumer.get("status") == "unique_catalog_member_consumer_class"
        and consumer.get("mapped_classes") == ["Controls"]
        and isinstance(consumer.get("mapped_vtables"), list)
        and len(consumer.get("mapped_vtables")) == 1
    )
    if not consumer_ok:
        reasons.append("controls_field_class_not_uniquely_proven")

    target_rows = sorted(
        [r for r in caller if r.get("source_id") == TARGET_SOURCE and r.get("call_index") in TARGET_CALLS],
        key=lambda r: int(r.get("call_index", -1)),
    )
    caller_ok = (
        len(target_rows) == 3
        and [r.get("call_index") for r in target_rows] == TARGET_CALLS
        and all(r.get("status") == "caller_argument_requires_nontransparent_provenance" for r in target_rows)
        and all(r.get("direct_caller_functions") == 1 for r in target_rows)
        and all(r.get("matched_caller_calls") == 3 for r in target_rows)
    )
    if not caller_ok:
        reasons.append("target_mixed_receiver_rows_changed")

    selected: dict[str, dict[str, Any]] = {}
    class_summaries: list[dict[str, Any]] = []
    for cls in TARGET_CLASSES:
        rows = class_slot_rows(catalog, cls)
        if len(rows) != 1:
            reasons.append(f"{cls}_slot_0x80_row_not_unique")
            continue
        row = rows[0]
        selected[cls] = row
        class_summaries.append(target_summary(row))
        ids = row.get("candidate_target_ids")
        if row.get("static_status") != "single_function_memory_pointer":
            reasons.append(f"{cls}_slot_0x80_not_single_function")
        if not isinstance(ids, list) or len(ids) != 1 or not isinstance(ids[0], str):
            reasons.append(f"{cls}_slot_0x80_target_not_unique")

    converged_target: str | None = None
    if len(selected) == 2:
        game_ids = selected["GameSceneUI"].get("candidate_target_ids")
        controls_ids = selected["Controls"].get("candidate_target_ids")
        if isinstance(game_ids, list) and isinstance(controls_ids, list) and len(game_ids) == 1 and game_ids == controls_ids:
            converged_target = game_ids[0]
        else:
            reasons.append("slot_0x80_targets_do_not_converge")

    if not reasons and converged_target is not None:
        status = "mixed_receiver_slot_target_converges"
    else:
        status = "mixed_receiver_slot_target_not_proven"

    details = {
        "format": OUT_FORMAT,
        "source_id": TARGET_SOURCE,
        "call_indices": TARGET_CALLS,
        "dispatch_slot_offset": TARGET_SLOT,
        "branch_classes": TARGET_CLASSES,
        "controls_field_class_proven": consumer_ok,
        "target_rows_stable": caller_ok,
        "class_slots": class_summaries,
        "converged_target_id": converged_target,
        "status": status,
        "reasons": reasons,
        "affects_routing": False,
    }

    if a.details_output:
        a.details_output.parent.mkdir(parents=True, exist_ok=True)
        a.details_output.write_text(json.dumps(details, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", OUT_FORMAT)
    print("source:", TARGET_SOURCE)
    print("calls:", TARGET_CALLS)
    print("slot:", TARGET_SLOT)
    print("controls field class proven:", consumer_ok)
    print("target mixed-receiver rows stable:", caller_ok)
    print("branch classes:", TARGET_CLASSES)
    for item in class_summaries:
        print(
            "class", item["class_name"],
            "vtable=", item["receiver_vtable_id"],
            "status=", item["static_status"],
            "targets=", item["candidate_target_ids"],
            "names=", item["target_names"],
            "runtime=", item["runtime_validation_status"],
        )
    print("converged target:", converged_target)
    print("status:", status)
    print("reasons:", reasons)
    if a.details_output:
        print("details output:", a.details_output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
