#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path
from typing import Any

PROV_FMT = "NEVERGONE.GhidraSingletonStoreReturnProvenance/1"
CATALOG_FMT = "NEVERGONE.StaticVtableSlotCatalog/3"
OUT_FMT = "NEVERGONE.SingletonConstructorStoreReturnAudit/1"
THUMB_SINGLETON_SHA256 = "ceb4bb6eb7801f478a82b5d83fa1932c28d9fb202951d3929ea1e041b2720907"
THUMB_SINGLETON_ENTRY = "003e95e0"
THUMB_SINGLETON_CELL = "0x94efd0"


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


def constructor_class(name: str | None) -> str | None:
    if not name or name == "-":
        return None
    base = name.split("(", 1)[0].strip()
    parts = base.split("::")
    if len(parts) < 2 or parts[-1] != parts[-2]:
        return None
    return "::".join(parts[:-1])


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit constructor -> singleton store -> return-load provenance")
    parser.add_argument("provenance_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--expected-function", required=True)
    parser.add_argument("--expected-class", required=True)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    provenance = read_tsv(args.provenance_tsv)
    catalog = read_jsonl(args.catalog_jsonl)
    for line_no, row in enumerate(provenance, 2):
        if row.get("format") != PROV_FMT:
            raise ValueError(f"{args.provenance_tsv}:{line_no}: expected {PROV_FMT}")
    class_vtables: dict[str, set[str]] = defaultdict(set)
    for line_no, row in enumerate(catalog, 1):
        if row.get("format") != CATALOG_FMT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FMT}")
        cls = row.get("class_name")
        vt = row.get("receiver_vtable_id")
        if isinstance(cls, str) and cls and isinstance(vt, str) and vt:
            class_vtables[cls].add(vt)

    reasons: list[str] = []
    function_names = {row.get("function_name") for row in provenance if row.get("function_name") not in (None, "", "-")}
    function_entries = {row.get("function_entry") for row in provenance if row.get("function_entry") not in (None, "", "-")}
    if len(function_names) != 1 or not any(args.expected_function in str(name) for name in function_names):
        reasons.append("function_identity_mismatch")

    calls = [row for row in provenance if row.get("row_type") == "call"]
    stores = [row for row in provenance if row.get("row_type") == "store"]
    returns = [row for row in provenance if row.get("row_type") == "return"]
    thumb_rows = [row for row in provenance if row.get("row_type") == "thumb_exact_cell_proof"]
    thumb_proof_ok = (
        args.expected_function == "sharedGameSceneUI"
        and args.expected_class == "GameSceneUI"
        and function_entries == {THUMB_SINGLETON_ENTRY}
        and len(thumb_rows) == 1
        and thumb_rows[0].get("function_entry") == THUMB_SINGLETON_ENTRY
        and thumb_rows[0].get("value_root_token") == THUMB_SINGLETON_CELL
        and thumb_rows[0].get("address_root_token") == THUMB_SINGLETON_CELL
        and thumb_rows[0].get("call_output_root_token") == "sha256:" + THUMB_SINGLETON_SHA256
    )
    if thumb_rows and not thumb_proof_ok:
        reasons.append("thumb_singleton_cell_proof_invalid")

    constructor_calls = [
        row for row in calls
        if constructor_class(row.get("call_target_name")) == args.expected_class
        and row.get("call_arg0_root_token") not in (None, "", "-")
    ]
    constructor_roots = {row.get("call_arg0_root_token") for row in constructor_calls}
    if len(constructor_calls) != 1 or len(constructor_roots) != 1:
        reasons.append("constructor_proven_object_not_unique")

    stores_from_constructor = [row for row in stores if row.get("value_root_token") in constructor_roots]
    storage_keys = {row.get("address_root_token") for row in stores_from_constructor if row.get("address_root_token") not in (None, "", "-")}
    if len(stores_from_constructor) != 1 or len(storage_keys) != 1:
        reasons.append("constructor_object_singleton_store_not_unique")

    storage_key = next(iter(storage_keys), None)
    all_stores_to_cell = [row for row in stores if storage_key is not None and row.get("address_root_token") == storage_key]
    if storage_key is not None and any(row.get("value_root_token") not in constructor_roots for row in all_stores_to_cell):
        reasons.append("singleton_cell_has_nonconstructor_store")

    bad_returns = [
        row for row in returns
        if not (
            (storage_key is not None and row.get("address_root_token") == storage_key)
            or row.get("value_root_token") in constructor_roots
        )
    ]
    # A different high-P-code alias root is not a negative machine-level
    # observation. Only the exact retained Thumb signature can independently
    # prove that the constructor-rooted STORE and final LOAD share one cell.
    # All normal constructor/store uniqueness gates still apply.
    thumb_fallback = (
        thumb_proof_ok
        and len(returns) == 1
        and len(constructor_calls) == 1
        and len(stores_from_constructor) == 1
        and len(stores) == 1
        and len(all_stores_to_cell) == 1
    )
    if not returns:
        reasons.append("singleton_function_has_no_return")
    elif bad_returns and not thumb_fallback:
        reasons.append("return_not_from_constructor_singleton_cell")
    return_provenance_method = (
        "high_pcode_same_cell" if not bad_returns and returns
        else "exact_thumb_singleton_cell" if bad_returns and thumb_fallback
        else "unproven"
    )

    vtables = sorted(class_vtables.get(args.expected_class, set()))
    if len(vtables) != 1:
        reasons.append("expected_class_vtable_not_unique")

    status = "singleton_constructor_store_return_class" if not reasons else "singleton_constructor_store_return_not_proven"
    detail = {
        "format": OUT_FMT,
        "function_entries": sorted(str(x) for x in function_entries),
        "function_names": sorted(str(x) for x in function_names),
        "expected_function": args.expected_function,
        "expected_class": args.expected_class,
        "constructor_calls": len(constructor_calls),
        "constructor_object_roots": sorted(str(x) for x in constructor_roots),
        "constructor_stores": len(stores_from_constructor),
        "singleton_storage_key": storage_key,
        "returns": len(returns),
        "return_roots": [
            {"value": row.get("value_root_token"), "address": row.get("address_root_token")}
            for row in returns
        ],
        "thumb_cell_proof_rows": len(thumb_rows),
        "thumb_cell_proof_valid": thumb_proof_ok,
        "return_provenance_method": return_provenance_method,
        "candidate_receiver_class_names": [args.expected_class] if not reasons else [],
        "candidate_receiver_vtable_ids": vtables if not reasons else [],
        "status": status,
        "reasons": reasons,
        "affects_routing": False,
    }

    if args.details_output is not None:
        args.details_output.parent.mkdir(parents=True, exist_ok=True)
        args.details_output.write_text(json.dumps(detail, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", OUT_FMT)
    print("function entries:", detail["function_entries"])
    print("function names:", detail["function_names"])
    print("expected class:", args.expected_class)
    print("constructor calls:", len(constructor_calls))
    print("constructor stores:", len(stores_from_constructor))
    print("singleton storage key:", storage_key)
    print("returns:", len(returns))
    print("return roots:", detail["return_roots"])
    print("Thumb cell proof:", thumb_proof_ok)
    print("return provenance method:", return_provenance_method)
    print("candidate vtables:", vtables)
    print("status:", status)
    print("reasons:", reasons)
    if args.details_output is not None:
        print("details output:", args.details_output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
