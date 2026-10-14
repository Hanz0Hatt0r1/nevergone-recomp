#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / "tools/ml/audit_nevergone_exact_singleton_cell_provenance.py"
PROV_FMT = "NEVERGONE.GhidraExactSingletonCellProvenance/1"
OWNER_FMT = "NEVERGONE.LoadEffectiveAddressOwnerAudit/1"
CAT_FMT = "NEVERGONE.StaticVtableSlotCatalog/3"
FIELDS = [
    "format","source_id","call_index","function_entry","function_name","cell_address","row_type",
    "op_address","op_time","value_root_token","call_target_entry","call_target_name",
    "call_arg0_root_token","call_output_root_token","exact_cell_reference",
]


def write_tsv(path: Path, rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, delimiter="\t")
        w.writeheader()
        for row in rows:
            w.writerow({k: row.get(k, "-") for k in FIELDS})


def base(source: str, call: int, entry: str, name: str, cell: str, kind: str) -> dict[str, object]:
    return {
        "format": PROV_FMT, "source_id": source, "call_index": str(call),
        "function_entry": entry, "function_name": name, "cell_address": cell,
        "row_type": kind, "op_address": "00101000", "op_time": "1",
        "value_root_token": "-", "call_target_entry": "-", "call_target_name": "-",
        "call_arg0_root_token": "-", "call_output_root_token": "-", "exact_cell_reference": "0",
    }


def case_rows(source: str, call: int, entry: str, accessor: str, cell: str, cls: str, root: str, bad_ctor: bool = False) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    r = base(source, call, entry, accessor, cell, "call")
    r.update({
        "call_target_entry": "00102000", "call_target_name": f"{cls}::{cls}()",
        "call_arg0_root_token": "def:CALL@00100ff0:1:4" if bad_ctor else root,
        "call_output_root_token": "-",
    })
    rows.append(r)
    r = base(source, call, entry, accessor, cell, "store")
    r.update({"value_root_token": root, "exact_cell_reference": "1"})
    rows.append(r)
    load_root = "def:LOAD@00101010:2:4"
    r = base(source, call, entry, accessor, cell, "load")
    r.update({"value_root_token": load_root, "exact_cell_reference": "1"})
    rows.append(r)
    r = base(source, call, entry, accessor, cell, "return")
    r.update({"value_root_token": load_root})
    rows.append(r)
    return rows


def owner(source: str, call: int, slot: str, cell: str, accessor: str) -> dict[str, object]:
    return {
        "format": OWNER_FMT, "source_id": source, "call_index": call,
        "dispatch_slot_offset": slot, "effective_address": cell,
        "owner_functions": [accessor], "status": "writable_effective_cell_with_refs",
    }


def catalog(cls: str, vtable: str, slot: str, target: str, target_name: str) -> dict[str, object]:
    return {
        "format": CAT_FMT, "class_name": cls, "receiver_vtable_id": vtable,
        "dispatch_slot_offset": slot, "static_status": "single_function_memory_pointer",
        "candidate_target_ids": [target], "target_functions": [{"name": target_name}],
        "runtime_validation_status": "runtime_unobserved",
    }


def run_case(tmp: Path, bad_android: bool) -> list[dict[str, object]]:
    owner_path = tmp / ("owners-bad.jsonl" if bad_android else "owners.jsonl")
    cat_path = tmp / "catalog.jsonl"
    action_path = tmp / ("action-bad.tsv" if bad_android else "action.tsv")
    android_path = tmp / ("android-bad.tsv" if bad_android else "android.tsv")
    details = tmp / ("details-bad.jsonl" if bad_android else "details.jsonl")

    owner_rows = [
        owner("test.so:00100000", 0, "0x8", "00009010", "ActionDataManager::sharedActionDataManager"),
        owner("test.so:00200000", 1, "0x8", "00009020", "AndroidSdkMgr::getInstance"),
    ]
    owner_path.write_text("".join(json.dumps(r) + "\n" for r in owner_rows), encoding="utf-8")
    cat_rows = [
        catalog("ActionDataManager", "test.so:0000a000", "0x8", "test.so:0000b000", "ActionDataManager::~ActionDataManager"),
        catalog("AndroidSdkMgr", "test.so:0000a100", "0x8", "test.so:0000b100", "AndroidSdkMgr::~AndroidSdkMgr"),
    ]
    cat_path.write_text("".join(json.dumps(r) + "\n" for r in cat_rows), encoding="utf-8")
    write_tsv(action_path, case_rows(
        "test.so:00100000", 0, "00100100", "ActionDataManager::sharedActionDataManager",
        "00009010", "ActionDataManager", "def:CALL@001000f0:1:4",
    ))
    write_tsv(android_path, case_rows(
        "test.so:00200000", 1, "00200100", "AndroidSdkMgr::getInstance",
        "00009020", "AndroidSdkMgr", "def:CALL@002000f0:1:4", bad_ctor=bad_android,
    ))
    subprocess.check_call([
        "python3", str(AUDIT), str(owner_path), str(cat_path), str(action_path), str(android_path),
        "--details-output", str(details),
    ])
    return [json.loads(line) for line in details.read_text().splitlines() if line]


def main() -> int:
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        good = run_case(tmp, False)
        assert len(good) == 2, good
        assert all(r["status"] == "singleton_cell_constructor_store_return_unique_class" for r in good), good
        by_source = {r["source_id"]: r for r in good}
        assert by_source["test.so:00100000"]["candidate_classes"] == ["ActionDataManager"]
        assert by_source["test.so:00200000"]["candidate_classes"] == ["AndroidSdkMgr"]
        assert by_source["test.so:00100000"]["slot_target_ids"] == ["test.so:0000b000"]

        bad = run_case(tmp, True)
        by_source = {r["source_id"]: r for r in bad}
        assert by_source["test.so:00100000"]["status"] == "singleton_cell_constructor_store_return_unique_class"
        assert by_source["test.so:00200000"]["status"] == "singleton_cell_class_not_proven"
        assert "constructor_proven_store_class_not_unique" in by_source["test.so:00200000"]["reasons"]

    print("exact singleton cell provenance smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
