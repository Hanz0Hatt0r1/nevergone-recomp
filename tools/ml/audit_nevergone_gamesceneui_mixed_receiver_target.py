#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

PROV_FMT = "NEVERGONE.GhidraFunctionReturnObjectProvenance/1"
FACTORY_FMT = "NEVERGONE.CallRootFactoryReturnAudit/1"
SINGLETON_FMT = "NEVERGONE.SingletonConstructorStoreReturnAudit/1"
ACCESSOR_FMT = "NEVERGONE.GameSceneUIControlsAccessorAudit/1"
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


def target_name_matches(value: Any, suffix: str) -> bool:
    return isinstance(value, str) and value != "-" and value.split("(", 1)[0].endswith(suffix)


def defining_site(value: Any) -> str | None:
    """Normalize defining-site tokens emitted by two independent exporters."""
    if not isinstance(value, str) or not value.startswith("def:"):
        return None
    text = value[4:]
    if "@" in text:
        _, text = text.split("@", 1)
    if not re.fullmatch(r"[^:]+:[0-9]+:[0-9]+", text):
        return None
    return text


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prove a unique virtual target across the mixed GameSceneUI/Controls receiver paths"
    )
    parser.add_argument("field_stores_tsv", type=Path)
    parser.add_argument("function_provenance_tsv", type=Path)
    parser.add_argument("factory_audit_jsonl", type=Path)
    parser.add_argument("singleton_audit_jsonl", type=Path)
    parser.add_argument("accessor_audit_jsonl", type=Path)
    parser.add_argument("caller_subtrees_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    stores = read_tsv(args.field_stores_tsv)
    provenance = read_tsv(args.function_provenance_tsv)
    factory = read_jsonl(args.factory_audit_jsonl)
    singleton_audit = read_jsonl(args.singleton_audit_jsonl)
    accessor = read_jsonl(args.accessor_audit_jsonl)
    subtrees = read_tsv(args.caller_subtrees_tsv)
    catalog = read_jsonl(args.catalog_jsonl)

    for line_no, row in enumerate(provenance, 2):
        if row.get("format") != PROV_FMT:
            raise ValueError(f"{args.function_provenance_tsv}:{line_no}: expected {PROV_FMT}")
    for line_no, row in enumerate(factory, 1):
        if row.get("format") != FACTORY_FMT:
            raise ValueError(f"{args.factory_audit_jsonl}:{line_no}: expected {FACTORY_FMT}")
    for line_no, row in enumerate(singleton_audit, 1):
        if row.get("format") != SINGLETON_FMT:
            raise ValueError(f"{args.singleton_audit_jsonl}:{line_no}: expected {SINGLETON_FMT}")
    for line_no, row in enumerate(accessor, 1):
        if row.get("format") != ACCESSOR_FMT:
            raise ValueError(f"{args.accessor_audit_jsonl}:{line_no}: expected {ACCESSOR_FMT}")
    for line_no, row in enumerate(subtrees, 2):
        if row.get("format") != SUBTREE_FMT:
            raise ValueError(f"{args.caller_subtrees_tsv}:{line_no}: expected {SUBTREE_FMT}")
    for line_no, row in enumerate(catalog, 1):
        if row.get("format") != CATALOG_FMT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FMT}")

    reasons: list[str] = []

    if len(singleton_audit) != 1:
        reasons.append("shared_gamesceneui_singleton_audit_not_unique")
    else:
        singleton = singleton_audit[0]
        if (
            singleton.get("status") != "singleton_constructor_store_return_class"
            or singleton.get("expected_class") != "GameSceneUI"
            or singleton.get("candidate_receiver_class_names") != ["GameSceneUI"]
        ):
            reasons.append("shared_gamesceneui_not_constructor_proven_gamesceneui")

    factory_by_source = {row.get("source_id"): row for row in factory}
    controls_factory = factory_by_source.get("probe:initControls")
    if controls_factory is None or controls_factory.get("status") != "unique_factory_return_class" or controls_factory.get("candidate_receiver_class_names") != ["Controls"]:
        reasons.append("initcontrols_not_constructor_proven_controls")

    init_calls = [
        row for row in provenance
        if row.get("source_id") == "probe:initGameSceneUI"
        and row.get("row_type") == "call"
        and target_name_matches(row.get("call_target_name"), "Controls::initControls")
    ]
    control_output_sites = {
        site for row in init_calls
        if (site := defining_site(row.get("call_output_root_token"))) is not None
    }
    target_stores = [
        row for row in stores
        if norm_slot(row.get("field_offset_hex")) == "0x134"
        and "GameSceneUI::initGameSceneUI" in (row.get("function_name") or "")
    ]
    matching_stores = [
        row for row in target_stores
        if defining_site(row.get("value_token")) in control_output_sites
    ]
    if len(init_calls) != 1 or len(control_output_sites) != 1:
        reasons.append("initgamesceneui_controls_factory_call_not_unique")
    if len(target_stores) != 1 or len(matching_stores) != 1:
        reasons.append("gamesceneui_0x134_store_not_unique_controls_factory_output")

    if len(accessor) != 1:
        reasons.append("accessor_audit_not_unique")
    else:
        acc = accessor[0]
        if acc.get("accessor_offsets") != ["0x134"] or acc.get("reasons") not in ([], None):
            reasons.append("getcontrolsobj_not_exact_0x134_accessor")
        if not acc.get("mixed_direct_singleton_value_present"):
            reasons.append("caller_mix_missing_direct_singleton_value")
        if dec(acc.get("caller_shared_ui_base_leaves"), 0) <= 0 or dec(acc.get("caller_offset_0x134_leaves"), 0) <= 0:
            reasons.append("caller_mix_missing_singleton_0x134_field_path")

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
    for cls in proven_classes:
        vtables = sorted(class_vtables.get(cls, set()))
        if len(vtables) != 1:
            reasons.append(f"{cls}_vtable_not_unique")
            continue
        vt = vtables[0]
        class_to_vtable[cls] = vt
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
            "proven_receiver_class_names": proven_classes,
            "proven_receiver_vtable_ids": [class_to_vtable.get(cls) for cls in proven_classes if cls in class_to_vtable],
            "class_target_ids": class_to_targets,
            "convergent_target_id": convergent_target if not row_reasons else None,
            "controls_field_offset": "0x134",
            "controls_factory_output_site": next(iter(control_output_sites), None),
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
