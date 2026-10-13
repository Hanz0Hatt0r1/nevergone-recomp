#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RESOLVER = ROOT / "tools/ml/resolve_nevergone_receiver_vtable_slots.py"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def read_jsonl(path: Path) -> list[dict]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def catalog_row(vtable: str, slot: str, targets: list[str], *, runtime: str = "runtime_unobserved", cls: str = "X") -> dict:
    return {
        "format": "NEVERGONE.StaticVtableSlotCatalog/3",
        "receiver_vtable_id": vtable,
        "dispatch_slot_offset": slot,
        "candidate_target_ids": targets,
        "runtime_validation_status": runtime,
        "runtime_target_ids": targets if runtime == "runtime_match" else [],
        "class_name": cls,
        "class_evidence_kind": "itanium_vtable_symbol",
        "vtable_symbol_name": f"vtable for {cls}",
        "static_status": "single_function_xref" if len(targets) == 1 else "ambiguous_function_xrefs",
        "primary_prefix_slot_count": 8,
        "source_kind": "ghidra_program_memory_pointer",
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        catalog = tmp / "catalog.jsonl"
        queries = tmp / "queries.jsonl"
        output = tmp / "resolved.jsonl"

        write_jsonl(catalog, [
            catalog_row("libcocos2dcpp.so:00900008", "0x24", ["libcocos2dcpp.so:0029e9ba"], cls="ActionData"),
            catalog_row("libcocos2dcpp.so:00900108", "0x24", ["libcocos2dcpp.so:0029eb4a"], runtime="runtime_match", cls="EenemyActionDataSet"),
            catalog_row("libcocos2dcpp.so:00900208", "0x8", ["libcocos2dcpp.so:00100000", "libcocos2dcpp.so:00100010"], cls="Ambiguous"),
            catalog_row("libcocos2dcpp.so:00900308", "0x10", ["libcocos2dcpp.so:00100020"], runtime="runtime_mismatch", cls="Mismatch"),
        ])
        write_jsonl(queries, [
            {
                "format": "NEVERGONE.DispatchReceiverObservation/1",
                "source_id": "libcocos2dcpp.so:0029ec10",
                "call_index": 0,
                "receiver_vtable_id": "libcocos2dcpp.so:00900008",
                "dispatch_slot_offset": "0x24",
            },
            {
                "source_id": "libcocos2dcpp.so:0029ed0c",
                "call_index": 0,
                "receiver_vtable_id": "libcocos2dcpp.so:00900108",
                "dispatch_slot_offset": 0x24,
            },
            {
                "receiver_vtable_id": "libcocos2dcpp.so:00900208",
                "dispatch_slot_offset": "0x8",
            },
            {
                "receiver_vtable_id": "libcocos2dcpp.so:00900308",
                "dispatch_slot_offset": "0x10",
            },
            {
                "receiver_vtable_id": "libcocos2dcpp.so:00900008",
                "dispatch_slot_offset": "0x28",
            },
            {
                "receiver_vtable_id": None,
                "dispatch_slot_offset": "0x24",
            },
        ])

        proc = subprocess.run(
            [sys.executable, str(RESOLVER), str(catalog), str(queries), "--output", str(output)],
            check=True,
            capture_output=True,
            text=True,
        )
        rows = read_jsonl(output)
        assert len(rows) == 6

        assert rows[0]["status"] == "exact_key_single_candidate"
        assert rows[0]["exact_candidate_target_id"] == "libcocos2dcpp.so:0029e9ba"
        assert rows[0]["class_name"] == "ActionData"
        assert rows[0]["runtime_verified"] is False

        assert rows[1]["status"] == "exact_key_runtime_verified"
        assert rows[1]["exact_candidate_target_id"] == "libcocos2dcpp.so:0029eb4a"
        assert rows[1]["runtime_verified"] is True

        assert rows[2]["status"] == "exact_key_ambiguous"
        assert rows[2]["exact_candidate_target_id"] is None
        assert rows[2]["candidate_target_count"] == 2

        assert rows[3]["status"] == "blocked_runtime_mismatch"
        assert rows[3]["exact_candidate_target_id"] is None

        assert rows[4]["status"] == "exact_key_not_found"
        assert rows[5]["status"] == "invalid_query"

        # Most important invariant: the two vtables share slot 0x24 but resolve
        # to different targets. No same-slot cross-vtable transfer occurs.
        assert rows[0]["candidate_target_ids"] != rows[1]["candidate_target_ids"]
        assert all(row["cross_vtable_transfer_allowed"] is False for row in rows)
        assert all(row["affects_routing"] is False for row in rows)

        assert "exact-key runtime verified: 1" in proc.stdout
        assert "exact-key single candidates: 1" in proc.stdout
        assert "exact-key ambiguous: 1" in proc.stdout
        assert "exact-key not found: 1" in proc.stdout
        assert "blocked runtime mismatch: 1" in proc.stdout
        assert "invalid queries: 1" in proc.stdout

    print("receiver-vtable exact-slot resolver smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
