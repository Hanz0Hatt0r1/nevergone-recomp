#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FINALIZER = ROOT / "tools/ml/finalize_nevergone_call_root_factory_recovery.py"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        inf = root / "v8.jsonl"
        q = root / "q8.jsonl"
        plan = root / "plan.tsv"
        audit = root / "audit.jsonl"
        out = root / "v9.jsonl"
        outq = root / "q9.jsonl"
        details = root / "details.jsonl"

        base = {
            "format": "NEVERGONE.StaticReceiverVtableInference/1",
            "call_index": 0,
            "dispatch_slot_offset": "0x7c",
            "receiver_type_name": None,
            "receiver_type_display": None,
            "candidate_receiver_class_names": [],
            "candidate_receiver_vtable_ids": [],
            "receiver_vtable_id": None,
            "runtime_validation_status": "runtime_unobserved",
            "status": "receiver_not_single_pointer",
            "affects_routing": False,
        }
        rows = [
            {**base, "source_id": "libtest.so:00100000", "source_entry": "00100000", "call_address": "00100004"},
            {**base, "source_id": "libtest.so:00100010", "source_entry": "00100010", "call_address": "00100014"},
            {
                **base,
                "source_id": "libtest.so:00100020",
                "source_entry": "00100020",
                "call_address": "00100024",
                "status": "resolved_unique_vtable",
                "class_name": "Test::Existing",
                "receiver_vtable_id": "vt_existing",
                "candidate_receiver_class_names": ["Test::Existing"],
                "candidate_receiver_vtable_ids": ["vt_existing"],
            },
        ]
        write_jsonl(inf, rows)
        write_jsonl(q, [{
            "format": "NEVERGONE.StaticReceiverVtableQuery/1",
            "source_id": "libtest.so:00100020",
            "call_index": 0,
            "call_address": "00100024",
            "receiver_vtable_id": "vt_existing",
            "dispatch_slot_offset": "0x7c",
            "receiver_class_name": "Test::Existing",
            "receiver_class_match_kind": "existing",
            "receiver_class_evidence_source": "existing",
            "receiver_type_name": None,
            "receiver_type_display": None,
            "runtime_validation_status": "runtime_unobserved",
            "evidence_ref": "existing",
            "affects_routing": False,
        }])

        with plan.open("w", encoding="utf-8", newline="") as handle:
            fields = ["source_id", "call_index", "source_entry", "call_address", "dispatch_slot_offset", "callee_entry", "callee_name"]
            writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            writer.writerow({"source_id": "libtest.so:00100000", "call_index": 0, "source_entry": "00100000", "call_address": "00100004", "dispatch_slot_offset": "0x7c", "callee_entry": "00500000", "callee_name": "Test::Sprite::create"})
            writer.writerow({"source_id": "libtest.so:00100010", "call_index": 0, "source_entry": "00100010", "call_address": "00100014", "dispatch_slot_offset": "0x7c", "callee_entry": "00500010", "callee_name": "Test::Wrong::create"})

        write_jsonl(audit, [
            {
                "format": "NEVERGONE.CallRootFactoryReturnAudit/1",
                "source_id": "libtest.so:00100000",
                "call_index": 0,
                "callee_entry": "00500000",
                "callee_name": "Test::Sprite::create",
                "candidate_receiver_class_names": ["Test::Sprite"],
                "candidate_receiver_vtable_ids": ["vt_sprite"],
                "status": "unique_factory_return_class",
                "affects_routing": False,
            },
            {
                "format": "NEVERGONE.CallRootFactoryReturnAudit/1",
                "source_id": "libtest.so:00100010",
                "call_index": 0,
                "callee_entry": "00500010",
                "callee_name": "Test::Sprite::create",
                "candidate_receiver_class_names": ["Test::Sprite"],
                "candidate_receiver_vtable_ids": ["vt_sprite"],
                "status": "unique_factory_return_class",
                "affects_routing": False,
            },
        ])

        subprocess.run([
            sys.executable, str(FINALIZER), str(inf), str(q), str(plan), str(audit),
            "--output", str(out), "--queries-output", str(outq), "--details-output", str(details),
        ], check=True, capture_output=True, text=True)

        output = [json.loads(x) for x in out.read_text().splitlines() if x.strip()]
        by_id = {x["source_id"]: x for x in output}
        accepted = by_id["libtest.so:00100000"]
        blocked = by_id["libtest.so:00100010"]
        existing = by_id["libtest.so:00100020"]
        assert accepted["status"] == "resolved_unique_vtable"
        assert accepted["class_name"] == "Test::Sprite"
        assert accepted["receiver_vtable_id"] == "vt_sprite"
        assert accepted["class_evidence_source"] == "call_root_factory_return_constructor_provenance"
        assert blocked["status"] == "receiver_not_single_pointer"
        assert blocked["call_root_factory_recovery"]["reasons"] == ["plan_callee_name_mismatch"]
        assert existing["status"] == "resolved_unique_vtable"

        qrows = [json.loads(x) for x in outq.read_text().splitlines() if x.strip()]
        assert {(x["source_id"], x["call_index"]) for x in qrows} == {
            ("libtest.so:00100000", 0),
            ("libtest.so:00100020", 0),
        }
        print("Nevergone CALL-root factory recovery smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
