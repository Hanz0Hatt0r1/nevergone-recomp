#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

ACCESSOR_FORMAT = "NEVERGONE.GhidraAccessorReturnField/1"
SUBTREE_FORMAT = "NEVERGONE.GhidraCallerArgumentSubtree/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
OUT_FORMAT = "NEVERGONE.GameSceneUIControlsAccessorAudit/1"


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            value = json.loads(raw)
            if not isinstance(value, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            rows.append(value)
    return rows


def terminal(name: str) -> str:
    return name.rsplit("::", 1)[-1].strip()


def clean_type(text: str | None) -> str:
    if not text or text == "-":
        return ""
    value = text.strip()
    while value.endswith("*"):
        value = value[:-1].strip()
    for prefix in ("class ", "struct "):
        if value.startswith(prefix):
            value = value[len(prefix):].strip()
    return value


def map_type(type_name: str, catalog: set[str], by_terminal: dict[str, set[str]]) -> set[str]:
    name = clean_type(type_name)
    if not name:
        return set()
    if name in catalog:
        return {name}
    return set(by_terminal.get(terminal(name), set()))


def parse_hex(text: str | None) -> int | None:
    if not text or text == "-":
        return None
    try:
        return int(text, 0)
    except ValueError:
        try:
            return int(text, 16)
        except ValueError:
            return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit GameSceneUI GetControlsObj return field against caller subtree evidence")
    parser.add_argument("accessor_tsv", type=Path)
    parser.add_argument("caller_subtrees_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    accessor_rows = read_tsv(args.accessor_tsv)
    subtree_rows = read_tsv(args.caller_subtrees_tsv)
    catalog_rows = read_jsonl(args.catalog_jsonl)

    class_vtables: dict[str, set[str]] = defaultdict(set)
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
        cls = row.get("class_name")
        vt = row.get("receiver_vtable_id")
        if isinstance(cls, str) and cls and isinstance(vt, str) and vt:
            class_vtables[cls].add(vt)
    catalog = set(class_vtables)
    by_terminal: dict[str, set[str]] = defaultdict(set)
    for cls in catalog:
        by_terminal[terminal(cls)].add(cls)

    for line_no, row in enumerate(accessor_rows, 2):
        if row.get("format") != ACCESSOR_FORMAT:
            raise ValueError(f"{args.accessor_tsv}:{line_no}: expected {ACCESSOR_FORMAT}")
    for line_no, row in enumerate(subtree_rows, 2):
        if row.get("format") != SUBTREE_FORMAT:
            raise ValueError(f"{args.caller_subtrees_tsv}:{line_no}: expected {SUBTREE_FORMAT}")

    field_rows = [r for r in accessor_rows if r.get("status") == "return_load_from_parameter_constant_offset"]
    offsets = sorted({parse_hex(r.get("load_offset")) for r in field_rows if parse_hex(r.get("load_offset")) is not None})
    classes: set[str] = set()
    typed_evidence: list[dict[str, Any]] = []
    for row in field_rows:
        mapped = set()
        mapped.update(map_type(row.get("return_pointee_type") or "", catalog, by_terminal))
        mapped.update(map_type(row.get("return_value_type") or "", catalog, by_terminal))
        mapped.update(map_type(row.get("declared_return_type") or "", catalog, by_terminal))
        if mapped:
            classes.update(mapped)
            typed_evidence.append({
                "return_index": row.get("return_index"),
                "declared_return_type": row.get("declared_return_type"),
                "return_value_type": row.get("return_value_type"),
                "return_pointee_type": row.get("return_pointee_type"),
                "mapped_classes": sorted(mapped),
            })

    vtables = sorted({vt for cls in classes for vt in class_vtables.get(cls, set())})
    nodes = [r for r in subtree_rows if r.get("row_type") == "node"]
    summaries = [r for r in subtree_rows if r.get("row_type") == "summary"]
    target_keys = sorted({(r.get("source_id") or "", r.get("source_call_index") or "") for r in summaries})

    direct_singleton_value = [
        r for r in nodes
        if r.get("defining_opcode") == "CALL"
        and r.get("call_target_name") == "sharedGameSceneUI"
        and r.get("edge_kind") in {"value", "root"}
    ]
    singleton_base = [
        r for r in nodes
        if r.get("defining_opcode") == "CALL"
        and r.get("call_target_name") == "sharedGameSceneUI"
        and r.get("edge_kind") == "base"
    ]
    offset_134 = [
        r for r in nodes
        if parse_hex(r.get("constant_value")) == 0x134
        and r.get("edge_kind") in {"offset", "offset_index"}
    ]

    reasons: list[str] = []
    if len(field_rows) != 1:
        reasons.append("accessor_field_return_not_unique")
    if offsets != [0x134]:
        reasons.append("accessor_offset_not_0x134")
    if not singleton_base or not offset_134:
        reasons.append("caller_subtree_missing_shared_ui_field_0x134_evidence")

    if not reasons and len(classes) == 1 and len(vtables) == 1:
        status = "accessor_0x134_unique_catalog_class"
    elif not reasons:
        status = "accessor_0x134_type_unresolved"
    else:
        status = "accessor_or_caller_evidence_incomplete"

    details = {
        "format": OUT_FORMAT,
        "accessor_rows": len(accessor_rows),
        "field_return_rows": len(field_rows),
        "accessor_offsets": [f"0x{x:x}" for x in offsets],
        "accessor_classes": sorted(classes),
        "accessor_vtables": vtables,
        "typed_evidence": typed_evidence,
        "caller_target_rows": len(target_keys),
        "caller_direct_shared_ui_value_leaves": len(direct_singleton_value),
        "caller_shared_ui_base_leaves": len(singleton_base),
        "caller_offset_0x134_leaves": len(offset_134),
        "mixed_direct_singleton_value_present": bool(direct_singleton_value),
        "status": status,
        "reasons": reasons,
        "affects_routing": False,
    }

    if args.details_output is not None:
        args.details_output.parent.mkdir(parents=True, exist_ok=True)
        args.details_output.write_text(json.dumps(details, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", OUT_FORMAT)
    print("accessor rows:", len(accessor_rows))
    print("field return rows:", len(field_rows))
    print("accessor offsets:", [f"0x{x:x}" for x in offsets])
    print("accessor classes:", sorted(classes))
    print("accessor vtables:", vtables)
    print("caller target rows:", len(target_keys))
    print("caller direct sharedGameSceneUI value leaves:", len(direct_singleton_value))
    print("caller sharedGameSceneUI base leaves:", len(singleton_base))
    print("caller 0x134 offset leaves:", len(offset_134))
    print("mixed direct singleton value present:", bool(direct_singleton_value))
    print("status:", status)
    print("reasons:", reasons)
    for row in accessor_rows:
        print(
            "accessor", row.get("function_entry"), row.get("function_name"),
            "return", row.get("return_index"),
            "declared=", row.get("declared_return_type"),
            "value=", row.get("return_value_type"),
            "pointee=", row.get("return_pointee_type"),
            "root=", row.get("root_opcode"),
            "base_param=", row.get("load_base_parameter_index"),
            "base_type=", row.get("load_base_parameter_type"),
            "offset=", row.get("load_offset"),
            "status=", row.get("status"),
        )
    if args.details_output is not None:
        print("details output:", args.details_output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
