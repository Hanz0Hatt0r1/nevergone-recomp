#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "tools/ml/plan_nevergone_ccjoystick_field_factory_return.py"
AUDIT = ROOT / "tools/ml/audit_nevergone_ccjoystick_field_factory_provenance.py"
SOURCE = "libcocos2dcpp.so:002a647e"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(r) + "\n" for r in rows), encoding="utf-8")


def write_tsv(path: Path, header: list[str], rows: list[list[object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(header); w.writerows(rows)


def fixture(tmp: Path, duplicate_slot: bool = False):
    owner = tmp / "owner.jsonl"
    stores = tmp / "stores.tsv"
    plan = tmp / "plan.tsv"
    factory = tmp / "factory.jsonl"
    catalog = tmp / "catalog.jsonl"
    details = tmp / "details.jsonl"

    write_jsonl(owner, [{
        "format": "NEVERGONE.ParameterRelativeReceiverFieldAudit/2",
        "source_id": SOURCE, "call_index": 4, "call_address": "002a64f0",
        "dispatch_slot_offset": "0x58", "ghidra_owner_class": "CCJoyStick",
        "field_offset": 0x104,
        "status": "parameter_relative_field_with_function_owner_evidence",
        "affects_routing": False,
    }])
    write_tsv(stores, [
        "format","function_entry","function_name","op_address","op_time",
        "address_kind","address_base_token","address_offset","base_parameter_index",
        "value_token","value_root_token","value_root_opcode","factory_target_entry","factory_target_name"
    ], [[
        "NEVERGONE.GhidraExactParameterFieldFactoryStore/1","002a6584","CCJoyStick::init",
        "002a65a0",1,"base_plus_constant","register:00000020:4","0x104",0,
        "def:COPY@002a659e:1:4","def:CALL@002a6598:2:4","CALL","00549614","cocos2d::CCSprite::create"
    ]])
    write_jsonl(factory, [{
        "format": "NEVERGONE.CallRootFactoryReturnAudit/1",
        "source_id": SOURCE, "call_index": 4,
        "callee_entry": "00549614", "callee_name": "cocos2d::CCSprite::create",
        "candidate_receiver_class_names": ["cocos2d::CCSprite"],
        "candidate_receiver_vtable_ids": ["libcocos2dcpp.so:009363d8"],
        "status": "unique_factory_return_class", "affects_routing": False,
    }])
    rows = [{
        "format": "NEVERGONE.StaticVtableSlotCatalog/3",
        "class_name": "cocos2d::CCSprite",
        "receiver_vtable_id": "libcocos2dcpp.so:009363d8",
        "dispatch_slot_offset": "0x58",
        "static_status": "single_function_memory_pointer",
        "candidate_target_ids": ["libcocos2dcpp.so:00525abc"],
        "target_functions": [{"name": "cocos2d::CCNode::exampleSlot58"}],
        "runtime_validation_status": "runtime_unobserved",
    }]
    if duplicate_slot:
        rows.append(dict(rows[0]))
    write_jsonl(catalog, rows)
    return owner, stores, plan, factory, catalog, details


def main() -> int:
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        owner, stores, plan, factory, catalog, details = fixture(tmp)
        subprocess.check_call(["python3", str(PLAN), str(owner), str(stores), str(plan)])
        with plan.open(encoding="utf-8", newline="") as f:
            rows = list(csv.DictReader(f, delimiter="\t"))
        assert len(rows) == 1, rows
        assert rows[0]["callee_entry"] == "00549614", rows
        subprocess.check_call(["python3", str(AUDIT), str(owner), str(stores), str(factory), str(catalog), "--details-output", str(details)])
        row = json.loads(details.read_text(encoding="utf-8").strip())
        assert row["status"] == "ccjoystick_field_factory_receiver_unique_class", row
        assert row["factory_return_class"] == "cocos2d::CCSprite", row
        assert row["factory_return_vtable_id"] == "libcocos2dcpp.so:009363d8", row
        assert row["slot_target_ids"] == ["libcocos2dcpp.so:00525abc"], row

    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        owner, stores, plan, factory, catalog, details = fixture(tmp, duplicate_slot=True)
        subprocess.check_call(["python3", str(AUDIT), str(owner), str(stores), str(factory), str(catalog), "--details-output", str(details)])
        row = json.loads(details.read_text(encoding="utf-8").strip())
        assert row["status"] == "ccjoystick_field_factory_receiver_incomplete", row
        assert "factory_class_slot_catalog_row_not_unique" in row["reasons"], row

    print("CCJoyStick field factory provenance smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
