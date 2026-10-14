#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

SINGLETON_FMT = "NEVERGONE.SingletonConstructorStoreReturnAudit/1"
CONSUMER_FMT = "NEVERGONE.GameSceneUIControlsReturnConsumerAudit/1"
SUBTREE_FMT = "NEVERGONE.GhidraCallerArgumentSubtree/1"
CATALOG_FMT = "NEVERGONE.StaticVtableSlotCatalog/3"
OUT_FMT = "NEVERGONE.MixedReceiverSlotTargetConvergenceAudit/1"
TARGET_SOURCE = "libcocos2dcpp.so:002a7594"
TARGET_CALLS = (3, 4, 7)
TARGET_SLOT = "0x80"


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


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def norm_slot(value: Any) -> str | None:
    if value in (None, "", "-"):
        return None
    try:
        parsed = int(str(value), 0)
    except ValueError:
        try:
            parsed = int(str(value), 16)
        except ValueError:
            return None
    return f"0x{parsed:x}" if parsed >= 0 else None


def dec(value: Any, default: int = -1) -> int:
    try:
        return int(str(value), 10)
    except Exception:
        return default


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prove one virtual target across independently proven mixed GameSceneUI/Controls receiver classes"
    )
    parser.add_argument("singleton_audit_jsonl", type=Path)
    parser.add_argument("controls_consumer_audit_jsonl", type=Path)
    parser.add_argument("caller_subtrees_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    singleton_audit = read_jsonl(args.singleton_audit_jsonl)
    consumer_audit = read_jsonl(args.controls_consumer_audit_jsonl)
    subtrees = read_tsv(args.caller_subtrees_tsv)
    catalog = read_jsonl(args.catalog_jsonl)

    for line_no, row in enumerate(singleton_audit, 1):
        if row.get("format") != SINGLETON_FMT:
            raise ValueError(f"{args.singleton_audit_jsonl}:{line_no}: expected {SINGLETON_FMT}")
    for line_no, row in enumerate(consumer_audit, 1):
        if row.get("format") != CONSUMER_FMT:
            raise ValueError(f"{args.controls_consumer_audit_jsonl}:{line_no}: expected {CONSUMER_FMT}")
    for line_no, row in enumerate(subtrees, 2):
        if row.get("format") != SUBTREE_FMT:
            raise ValueError(f"{args.caller_subtrees_tsv}:{line_no}: expected {SUBTREE_FMT}")
    for line_no, row in enumerate(catalog, 1):
        if row.get("format") != CATALOG_FMT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FMT}")

    reasons: list[str] = []
    singleton_vtables: list[str] = []
    controls_vtables: list[str] = []

    if len(singleton_audit) != 1:
        reasons.append("shared_gamesceneui_singleton_audit_not_unique")
    else:
        singleton = singleton_audit[0]
        singleton_vtables = singleton.get("candidate_receiver_vtable_ids") or []
        if (
            singleton.get("status") != "singleton_constructor_store_return_class"
            or singleton.get("expected_class") != "GameSceneUI"
            or singleton.get("candidate_receiver_class_names") != ["GameSceneUI"]
            or not isinstance(singleton_vtables, list)
            or len(singleton_vtables) != 1
        ):
            reasons.append("shared_gamesceneui_not_constructor_proven_gamesceneui")

    if len(consumer_audit) != 1:
        reasons.append("controls_consumer_audit_not_unique")
    else:
        consumer = consumer_audit[0]
        controls_vtables = consumer.get("mapped_vtables") or []
        if (
            consumer.get("status") != "unique_catalog_member_consumer_class"
            or consumer.get("accessor_chain_ok") is not True
            or consumer.get("mapped_classes") != ["Controls"]
            or not isinstance(controls_vtables, list)
            or len(controls_vtables) != 1
        ):
            reasons.append("getcontrolsobj_return_not_uniquely_proven_controls")

    grouped_subtrees: dict[tuple[str, int], list[dict[str, str]]] = defaultdict(list)
    for row in subtrees:
        source = row.get("source_id") or ""
        call = dec(row.get("source_call_index"), -1)
        if source and call >= 0:
            grouped_subtrees[(source, call)].append(row)

    per_call_reasons: dict[int, list[str]] = {}
    for call in TARGET_CALLS:
        rs = grouped_subtrees.get((TARGET_SOURCE, call), [])
        local: list[str] = []
        summary = next((r for r in rs if r.get("row_type") == "summary"), None)
        nodes = [r for r in rs if r.get("row_type") == "node"]
        if summary is None or summary.get("status") != "complete_caller_argument_subtrees":
            local.append("caller_subtree_incomplete")
        if summary is not None and norm_slot(summary.get("dispatch_slot_offset")) != TARGET_SLOT:
            local.append("caller_slot_not_0x80")
        direct = any(
            r.get("defining_opcode") == "CALL"
            and r.get("call_target_name") == "sharedGameSceneUI"
            and r.get("edge_kind") in {"value", "root"}
            for r in nodes
        )
        field_base = any(
            r.get("defining_opcode") == "CALL"
            and r.get("call_target_name") == "sharedGameSceneUI"
            and r.get("edge_kind") == "base"
            for r in nodes
        )
        offset = any(
            norm_slot(r.get("constant_value")) == "0x134"
            and r.get("edge_kind") in {"offset", "offset_index"}
            for r in nodes
        )
        if not direct:
            local.append("caller_missing_direct_gamesceneui_branch")
        if not field_base or not offset:
            local.append("caller_missing_gamesceneui_0x134_branch")
        # Caller argument provenance is not CALLIND receiver provenance. These
        # branches describe the argument passed to Controls::showControlsAttackKey,
        # not the object loaded for the nested virtual dispatch. Even exact
        # singleton/Controls class proofs cannot close that semantic gap.
        local.append("caller_argument_not_virtual_receiver_provenance")
        per_call_reasons[call] = local

    class_vtables: dict[str, set[str]] = defaultdict(set)
    exact_rows: dict[tuple[str, str], list[dict[str, Any]]] = defaultdict(list)
    for row in catalog:
        cls = row.get("class_name")
        vt = row.get("receiver_vtable_id")
        slot = norm_slot(row.get("dispatch_slot_offset"))
        if isinstance(cls, str) and cls and isinstance(vt, str) and vt:
            class_vtables[cls].add(vt)
        if isinstance(vt, str) and vt and slot is not None:
            exact_rows[(vt, slot)].append(row)

    proven_classes = ["Controls", "GameSceneUI"]
    class_to_vtable: dict[str, str] = {}
    class_to_targets: dict[str, list[str]] = {}
    evidence_vtables = {"Controls": controls_vtables, "GameSceneUI": singleton_vtables}
    for cls in proven_classes:
        vtables = sorted(class_vtables.get(cls, set()))
        if len(vtables) != 1:
            reasons.append(f"{cls}_vtable_not_unique")
            continue
        vt = vtables[0]
        class_to_vtable[cls] = vt
        if evidence_vtables.get(cls) != [vt]:
            reasons.append(f"{cls}_evidence_catalog_vtable_mismatch")
        rows = exact_rows.get((vt, TARGET_SLOT), [])
        if not rows:
            reasons.append(f"{cls}_slot_0x80_missing")
            continue
        if any(row.get("runtime_validation_status") == "runtime_mismatch" for row in rows):
            reasons.append(f"{cls}_slot_0x80_runtime_mismatch")
            continue
        targets: set[str] = set()
        for row in rows:
            values = row.get("candidate_target_ids") or []
            if not isinstance(values, list) or not all(isinstance(x, str) for x in values):
                reasons.append(f"{cls}_slot_0x80_invalid_targets")
                continue
            targets.update(values)
        class_to_targets[cls] = sorted(targets)
        if len(targets) != 1:
            reasons.append(f"{cls}_slot_0x80_target_not_unique")

    target_sets = [set(class_to_targets.get(cls, [])) for cls in proven_classes]
    convergent = set.intersection(*target_sets) if target_sets and all(target_sets) else set()
    if len(convergent) != 1 or any(values != convergent for values in target_sets):
        reasons.append("proven_receiver_classes_do_not_converge_one_target")
    convergent_target = next(iter(convergent)) if len(convergent) == 1 else None

    output: list[dict[str, Any]] = []
    counts: Counter[str] = Counter()
    for call in TARGET_CALLS:
        row_reasons = list(reasons) + per_call_reasons.get(call, [])
        status = "unique_target_across_proven_receiver_classes" if not row_reasons else "mixed_receiver_target_not_proven"
        counts[status] += 1
        output.append({
            "format": OUT_FMT,
            "source_id": TARGET_SOURCE,
            "call_index": call,
            "dispatch_slot_offset": TARGET_SLOT,
            # Do not label caller-argument classes as virtual receiver classes.
            "proven_receiver_class_names": [],
            "proven_receiver_vtable_ids": [],
            "class_target_ids": {},
            "caller_argument_candidate_class_names": proven_classes,
            "caller_argument_candidate_vtable_ids": [
                class_to_vtable.get(cls) for cls in proven_classes if cls in class_to_vtable
            ],
            "caller_argument_candidate_target_ids": class_to_targets,
            "catalog_convergent_target_candidate": convergent_target,
            "convergent_target_id": None,
            "receiver_value_proven": False,
            "controls_field_offset": None,
            "caller_argument_field_offset": "0x134",
            "reasons": row_reasons,
            "status": status,
            "affects_routing": False,
        })

    if args.details_output is not None:
        write_jsonl(args.details_output, output)

    print("format:", OUT_FMT)
    print("target rows:", len(output))
    print("status counts:", dict(counts))
    print("proven classes:", proven_classes)
    print("class vtables:", class_to_vtable)
    print("class slot targets:", class_to_targets)
    print("convergent target:", convergent_target)
    print("global reasons:", reasons)
    for row in output:
        print(row["source_id"], "call", row["call_index"], "status=", row["status"], "target=", row["convergent_target_id"], "reasons=", row["reasons"])
    if args.details_output is not None:
        print("details output:", args.details_output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
