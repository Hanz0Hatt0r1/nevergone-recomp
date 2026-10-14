#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def inf(source_id: str, idx: int, status: str, source: str | None, cls: str | None, vtable: str | None) -> dict:
    return {
        "format": "NEVERGONE.StaticReceiverVtableInference/1",
        "source_id": source_id,
        "source_entry": source_id.split(":", 1)[1],
        "call_index": idx,
        "call_address": f"00{idx + 1:06x}",
        "status": status,
        "class_evidence_source": source,
        "class_name": cls,
        "receiver_vtable_id": vtable,
        "dispatch_slot_offset": "0x80",
        "affects_routing": False,
    }


def query(source_id: str, idx: int, vtable: str) -> dict:
    return {
        "format": "NEVERGONE.StaticReceiverVtableQuery/1",
        "source_id": source_id,
        "call_index": idx,
        "call_address": f"00{idx + 1:06x}",
        "receiver_vtable_id": vtable,
        "dispatch_slot_offset": "0x80",
        "receiver_class_name": "X",
        "affects_routing": False,
    }


def audit(source_id: str, idx: int, source: str, cls: str, *, safe: bool, slot_match=None) -> dict:
    return {
        "format": "NEVERGONE.ResolvedCallerSafetyAudit/1",
        "source_id": source_id,
        "call_index": idx,
        "call_address": f"00{idx + 1:06x}",
        "class_evidence_source": source,
        "inferred_class_name": cls,
        "status": "safe_caller_recovery" if safe else "unsafe_caller_recovery",
        "structural_slot_match": slot_match,
        "reasons": [] if safe else ["synthetic_failure"],
        "affects_routing": False,
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        inference = tmp / "inference.jsonl"
        queries = tmp / "queries.jsonl"
        audits = tmp / "audit.jsonl"
        output = tmp / "out.jsonl"
        outq = tmp / "outq.jsonl"

        rows = [
            inf("lib.so:1", 0, "resolved_unique_vtable", "parameter_caller_type", "A", "lib.so:v1"),
            inf("lib.so:2", 0, "resolved_unique_vtable", "structural_parameter_caller_type", "B", "lib.so:v2"),
            inf("lib.so:3", 0, "resolved_unique_vtable", "parameter_caller_type", "C", "lib.so:v3"),
            inf("lib.so:4", 0, "resolved_unique_vtable", "direct_receiver_type", "D", "lib.so:v4"),
            inf("lib.so:5", 0, "non_virtual_indirect_call", None, None, None),
        ]
        write_jsonl(inference, rows)
        write_jsonl(queries, [query("lib.so:1", 0, "lib.so:v1"), query("lib.so:2", 0, "lib.so:v2"), query("lib.so:3", 0, "lib.so:v3"), query("lib.so:4", 0, "lib.so:v4")])
        write_jsonl(audits, [
            audit("lib.so:1", 0, "parameter_caller_type", "A", safe=True),
            audit("lib.so:2", 0, "structural_parameter_caller_type", "B", safe=True, slot_match=True),
            audit("lib.so:3", 0, "parameter_caller_type", "C", safe=False),
        ])

        subprocess.run([
            sys.executable,
            str(ROOT / "tools/ml/enforce_nevergone_caller_recovery_safety.py"),
            str(inference), str(queries), str(audits),
            "--output", str(output), "--queries-output", str(outq),
        ], check=True)

        final = [json.loads(line) for line in output.read_text(encoding="utf-8").splitlines() if line.strip()]
        by_id = {row["source_id"]: row for row in final}
        assert by_id["lib.so:1"]["status"] == "resolved_unique_vtable"
        assert by_id["lib.so:2"]["status"] == "resolved_unique_vtable"
        assert by_id["lib.so:3"]["status"] == "caller_provenance_safety_blocked"
        assert by_id["lib.so:3"]["receiver_vtable_id"] is None
        assert by_id["lib.so:3"]["blocked_receiver_vtable_id"] == "lib.so:v3"
        assert by_id["lib.so:4"]["status"] == "resolved_unique_vtable"
        assert by_id["lib.so:5"]["status"] == "non_virtual_indirect_call"

        final_queries = [json.loads(line) for line in outq.read_text(encoding="utf-8").splitlines() if line.strip()]
        assert {row["source_id"] for row in final_queries} == {"lib.so:1", "lib.so:2", "lib.so:4"}

        # Missing audit is also fail-closed.
        write_jsonl(audits, [audit("lib.so:2", 0, "structural_parameter_caller_type", "B", safe=True, slot_match=True)])
        subprocess.run([
            sys.executable,
            str(ROOT / "tools/ml/enforce_nevergone_caller_recovery_safety.py"),
            str(inference), str(queries), str(audits),
            "--output", str(output), "--queries-output", str(outq),
        ], check=True)
        final = [json.loads(line) for line in output.read_text(encoding="utf-8").splitlines() if line.strip()]
        by_id = {row["source_id"]: row for row in final}
        assert by_id["lib.so:1"]["status"] == "caller_provenance_safety_blocked"
        assert "missing_caller_safety_audit" in by_id["lib.so:1"]["caller_safety_reasons"]

    print("caller recovery safety finalization smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
