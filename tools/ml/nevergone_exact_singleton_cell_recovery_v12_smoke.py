#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FINALIZER = ROOT / "tools/ml/finalize_nevergone_exact_singleton_cell_recovery.py"
RESOLVER = ROOT / "tools/ml/resolve_nevergone_receiver_queries_v11.py"
INF_FMT = "NEVERGONE.StaticReceiverVtableInference/1"
Q_FMT = "NEVERGONE.StaticReceiverVtableQuery/1"
AUDIT_FMT = "NEVERGONE.ExactSingletonCellProvenanceAudit/1"
CAT_FMT = "NEVERGONE.StaticVtableSlotCatalog/3"

ACTION = ("libcocos2dcpp.so:0029edb4", 0)
ANDROID = ("libcocos2dcpp.so:002a08fc", 1)
QUARANTINE = [("libcocos2dcpp.so:002a7594", i) for i in (3, 4, 7)]


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(r) + "\n" for r in rows), encoding="utf-8")


def inf(source: str, call: int, slot: str, status: str, **extra) -> dict:
    row = {
        "format": INF_FMT,
        "source_id": source,
        "call_index": call,
        "call_address": f"00{call+1:06x}",
        "dispatch_slot_offset": slot,
        "status": status,
        "runtime_validation_status": "runtime_unobserved",
    }
    row.update(extra)
    return row


def query(source: str, call: int, vt: str, cls: str, slot: str) -> dict:
    return {
        "format": Q_FMT,
        "source_id": source,
        "call_index": call,
        "call_address": "00100010",
        "receiver_vtable_id": vt,
        "dispatch_slot_offset": slot,
        "receiver_class_name": cls,
        "receiver_class_match_kind": "fixture",
        "receiver_class_evidence_source": "fixture",
        "runtime_validation_status": "runtime_unobserved",
        "evidence_ref": f"{source}#call={call}",
        "affects_routing": False,
    }


def audit(source: str, call: int, cell: str, accessor: str, cls: str, vt: str, target: str) -> dict:
    return {
        "format": AUDIT_FMT,
        "source_id": source,
        "call_index": call,
        "dispatch_slot_offset": "0x8",
        "accessor_function_entry": "00100000",
        "accessor_function_name": accessor,
        "singleton_cell": cell,
        "exact_store_count": 1,
        "exact_load_count": 2,
        "return_count": 1,
        "returned_exact_load_roots": ["def:LOAD@00100020:1:4"],
        "constructor_store_matches": [{
            "class_name": cls,
            "call_target_entry": "00100100",
            "call_target_name": f"{cls}::{cls}",
            "object_root": "def:CALL@00100010:1:4",
            "op_address": "00100014",
        }],
        "candidate_classes": [cls],
        "candidate_vtables": [vt],
        "slot_static_status": "single_function_memory_pointer",
        "slot_target_ids": [target],
        "slot_target_names": [f"{cls}::~{cls}"],
        "status": "singleton_cell_constructor_store_return_unique_class",
        "reasons": [],
        "affects_routing": False,
    }


def cat(cls: str, vt: str, slot: str, target: str) -> dict:
    return {
        "format": CAT_FMT,
        "class_name": cls,
        "receiver_vtable_id": vt,
        "dispatch_slot_offset": slot,
        "static_status": "single_function_memory_pointer",
        "candidate_target_ids": [target],
        "target_functions": [{"name": f"{cls}::~{cls}"}],
        "runtime_validation_status": "runtime_unobserved",
    }


def run_case(tmp: Path, bad_android_target: bool) -> tuple[list[dict], list[dict], list[dict]]:
    suffix = "bad" if bad_android_target else "good"
    inference_path = tmp / f"inference-{suffix}.jsonl"
    queries_path = tmp / f"queries-{suffix}.jsonl"
    audit_path = tmp / f"audit-{suffix}.jsonl"
    catalog_path = tmp / "catalog.jsonl"
    out_path = tmp / f"out-{suffix}.jsonl"
    outq_path = tmp / f"outq-{suffix}.jsonl"
    details_path = tmp / f"details-{suffix}.jsonl"
    resolution_path = tmp / f"resolution-{suffix}.jsonl"

    base_source = "test.so:00100000"
    base_vt = "test.so:0000a000"
    base_cls = "AlreadyResolved"
    rows = [
        inf(base_source, 0, "0x10", "resolved_unique_vtable", class_name=base_cls, receiver_vtable_id=base_vt),
        inf(ACTION[0], ACTION[1], "0x8", "receiver_not_single_pointer"),
        inf(ANDROID[0], ANDROID[1], "0x8", "receiver_not_single_pointer"),
    ]
    rows.extend(inf(src, call, "0x80", "receiver_not_single_pointer") for src, call in QUARANTINE)
    write_jsonl(inference_path, rows)
    write_jsonl(queries_path, [query(base_source, 0, base_vt, base_cls, "0x10")])

    action_target = "libcocos2dcpp.so:0029eb38"
    android_catalog_target = "libcocos2dcpp.so:002a0d8c"
    android_audit_target = "libcocos2dcpp.so:00dead00" if bad_android_target else android_catalog_target
    write_jsonl(audit_path, [
        audit(ACTION[0], ACTION[1], "0094cb44", "ActionDataManager::sharedActionDataManager",
              "ActionDataManager", "libcocos2dcpp.so:008f3988", action_target),
        audit(ANDROID[0], ANDROID[1], "0094cbd8", "AndroidSdkMgr::getInstance",
              "AndroidSdkMgr", "libcocos2dcpp.so:008f3ce8", android_audit_target),
    ])
    write_jsonl(catalog_path, [
        cat(base_cls, base_vt, "0x10", "test.so:0000b000"),
        cat("ActionDataManager", "libcocos2dcpp.so:008f3988", "0x8", action_target),
        cat("AndroidSdkMgr", "libcocos2dcpp.so:008f3ce8", "0x8", android_catalog_target),
    ])

    subprocess.check_call([
        "python3", str(FINALIZER), str(inference_path), str(queries_path), str(audit_path), str(catalog_path),
        "--output", str(out_path), "--queries-output", str(outq_path), "--details-output", str(details_path),
    ])
    subprocess.check_call([
        "python3", str(RESOLVER), str(catalog_path), str(outq_path), "--output", str(resolution_path),
    ])
    return (
        [json.loads(x) for x in out_path.read_text().splitlines() if x],
        [json.loads(x) for x in outq_path.read_text().splitlines() if x],
        [json.loads(x) for x in details_path.read_text().splitlines() if x],
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        rows, queries, details = run_case(tmp, False)
        by = {(r["source_id"], r["call_index"]): r for r in rows}
        assert by[ACTION]["status"] == "resolved_unique_vtable"
        assert by[ACTION]["class_name"] == "ActionDataManager"
        assert by[ACTION]["receiver_vtable_id"] == "libcocos2dcpp.so:008f3988"
        assert by[ANDROID]["status"] == "resolved_unique_vtable"
        assert by[ANDROID]["class_name"] == "AndroidSdkMgr"
        assert by[ANDROID]["receiver_vtable_id"] == "libcocos2dcpp.so:008f3ce8"
        assert len(queries) == 3, queries
        assert all(by[key]["status"] == "receiver_not_single_pointer" for key in QUARANTINE)
        assert all(r["accepted"] is True for r in details), details

        rows, queries, details = run_case(tmp, True)
        by = {(r["source_id"], r["call_index"]): r for r in rows}
        detail_by = {(r["source_id"], r["call_index"]): r for r in details}
        assert by[ACTION]["status"] == "resolved_unique_vtable"
        assert by[ANDROID]["status"] == "receiver_not_single_pointer"
        assert "catalog_target_mismatch" in detail_by[ANDROID]["reasons"], detail_by[ANDROID]
        assert len(queries) == 2, queries
        assert all(by[key]["status"] == "receiver_not_single_pointer" for key in QUARANTINE)

    print("exact singleton cell recovery v12 smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
