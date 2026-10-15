#!/usr/bin/env python3
"""Synthetic fail-closed checks for the post-v17 non-slot80 frontier probe."""
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
PLAN = HERE / "plan_nevergone_non_slot80_receiver_frontier_v18.py"
AUDIT = HERE / "audit_nevergone_non_slot80_receiver_frontier_v18.py"
INF_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
HEAD = [
    "format", "row_type", "function_entry", "call_index", "call_address",
    "call_time", "recovery_status", "slot_offset", "structural_receiver_token",
    "node_id", "parent_node_id", "input_index", "depth", "node_token",
    "type_display", "pointer_depth", "pointee_type_display", "constant_value",
    "defining_opcode", "defining_address", "defining_time", "call_target_entry",
    "call_target_name", "call_target_return_type", "call_target_is_thunk",
    "call_target_thunk_target", "truncation", "node_count",
    "truncated_by_budget", "depth_truncation_count", "cycle_count", "max_nodes",
    "max_depth",
]

CASES = [
    ("libcocos2dcpp.so:00100000", 0, "00100020", "0x4", "LOAD"),
    ("libcocos2dcpp.so:00100000", 1, "00100040", "0x10", "CALL"),
    ("libcocos2dcpp.so:00100000", 2, "00100060", "0x24", "MULTIEQUAL"),
    ("libcocos2dcpp.so:00200000", 0, "00200020", "0x30", "INT_ADD"),
    ("libcocos2dcpp.so:00200000", 1, "00200040", "0x44", "LOAD"),
    ("libcocos2dcpp.so:00300000", 0, "00300020", "0x58", "CALL"),
    ("libcocos2dcpp.so:00300000", 1, "00300040", "0x100", "LOAD"),
]


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text(
        "".join(json.dumps(row, sort_keys=True) + "\n" for row in rows),
        encoding="utf-8",
    )


def run(*args: str, good: bool = True) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run([sys.executable, *args], text=True, capture_output=True)
    if (proc.returncode == 0) != good:
        raise AssertionError(
            f"unexpected return code {proc.returncode}:\n{proc.stdout}\n{proc.stderr}"
        )
    return proc


def failure_rows() -> list[dict]:
    rows = []
    for source, call, address, slot, opcode in CASES:
        rows.append(
            {
                "source_id": source,
                "call_index": call,
                "call_address": address,
                "dispatch_slot_offset": slot,
                "status": "unresolved_receiver_class",
                "receiver_storage_kind": "unique",
                "receiver_token": f"def:{opcode}@{address}:0:4",
                "receiver_def_mnemonic": opcode,
                "receiver_type_display": "void *",
                "receiver_pointer_depth": 1,
                "pointee_type_display": "void",
                "flow_type_node_count": 1,
                "candidate_receiver_class_names": [],
            }
        )
    return rows


def write_slices(root: Path, *, bad_slot_key: tuple[str, int] | None = None) -> None:
    by_source: dict[str, list[dict[str, str]]] = {}
    for source, call, address, slot, opcode in CASES:
        target_slot = "0x7c" if bad_slot_key == (source, call) else slot
        summary = {field: "-" for field in HEAD}
        summary.update(
            format=SLICE_FORMAT,
            row_type="summary",
            function_entry=source.split(":", 1)[1],
            call_index=str(call),
            call_address=address,
            recovery_status="canonical_receiver",
            slot_offset=slot,
            node_count="1",
            truncated_by_budget="0",
            depth_truncation_count="0",
            cycle_count="0",
        )
        node = {field: "-" for field in HEAD}
        node.update(
            format=SLICE_FORMAT,
            row_type="node",
            function_entry=source.split(":", 1)[1],
            call_index=str(call),
            call_address=address,
            slot_offset=target_slot,
            node_id="0",
            parent_node_id="-1",
            node_token=f"def:{opcode}@{address}:0:4",
            defining_opcode=opcode,
            type_display="void *",
            pointer_depth="1",
            pointee_type_display="void",
        )
        if opcode == "CALL":
            node.update(
                call_target_entry="00400000",
                call_target_name="syntheticFactory",
                call_target_return_type="void *",
            )
        by_source.setdefault(source, []).extend([summary, node])

    root.mkdir(parents=True, exist_ok=True)
    for source, rows in by_source.items():
        safe = source.replace(":", "_").replace("/", "_")
        with (root / f"{safe}.tsv").open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=HEAD, delimiter="\t")
            writer.writeheader()
            writer.writerows(rows)


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        failures = root / "failures.jsonl"
        inference = root / "inference.jsonl"
        plan = root / "plan.tsv"
        details = root / "details.jsonl"
        slices = root / "slices"

        rows = failure_rows()
        write_jsonl(failures, rows)
        write_jsonl(inference, [dict(row, format=INF_FORMAT) for row in rows])

        run(
            str(PLAN), str(failures), str(inference), "--output", str(plan),
            "--expected-calls", "7", "--expected-sources", "3",
        )
        run(
            str(PLAN), str(failures), str(inference), "--output", str(plan),
            "--expected-calls", "6", good=False,
        )

        unexpected = [dict(row) for row in rows]
        unexpected[0]["status"] = "ambiguous_receiver_class"
        write_jsonl(failures, unexpected)
        write_jsonl(inference, [dict(row, format=INF_FORMAT) for row in unexpected])
        run(str(PLAN), str(failures), str(inference), "--output", str(plan), good=False)

        slot80 = [dict(row) for row in rows]
        slot80[0]["dispatch_slot_offset"] = "0x80"
        write_jsonl(failures, slot80)
        write_jsonl(inference, [dict(row, format=INF_FORMAT) for row in slot80])
        run(str(PLAN), str(failures), str(inference), "--output", str(plan), good=False)

        write_jsonl(failures, rows)
        inf = [dict(row, format=INF_FORMAT) for row in rows]
        inf[0]["call_address"] = "00100022"
        write_jsonl(inference, inf)
        run(str(PLAN), str(failures), str(inference), "--output", str(plan), good=False)

        write_jsonl(inference, [dict(row, format=INF_FORMAT) for row in rows])
        run(
            str(PLAN), str(failures), str(inference), "--output", str(plan),
            "--expected-calls", "7", "--expected-sources", "3",
        )
        run(
            str(AUDIT), str(plan), str(failures), str(inference), str(slices),
            "--details-output", str(details), "--require-complete", good=False,
        )
        incomplete = [json.loads(line) for line in details.read_text().splitlines()]
        assert len(incomplete) == 7
        assert all(row["status"] == "structural_slice_not_proven_complete" for row in incomplete)

        write_slices(slices)
        run(
            str(AUDIT), str(plan), str(failures), str(inference), str(slices),
            "--details-output", str(details), "--require-complete",
        )
        audited = [json.loads(line) for line in details.read_text().splitlines()]
        assert len(audited) == 7
        assert all(row["status"] == "complete_structural_slice" for row in audited)
        next_kinds = {row["root_opcode"]: row["next_probe_kind"] for row in audited}
        assert next_kinds["LOAD"] == "receiver_load_write_provenance"
        assert next_kinds["CALL"] == "receiver_call_return_provenance"
        assert next_kinds["MULTIEQUAL"] == "receiver_phi_input_provenance"
        assert next_kinds["INT_ADD"] == "receiver_address_expression_provenance"
        assert all(row["receiver_class_proven"] is False for row in audited)
        assert all(row["affects_routing"] is False for row in audited)

        write_slices(slices, bad_slot_key=(CASES[0][0], CASES[0][1]))
        run(
            str(AUDIT), str(plan), str(failures), str(inference), str(slices),
            "--details-output", str(details), "--require-complete", good=False,
        )
        audited = [json.loads(line) for line in details.read_text().splitlines()]
        first = next(row for row in audited if row["source_id"] == CASES[0][0] and row["call_index"] == CASES[0][1])
        assert "slice_node_identity_mismatch" in first["reasons"]

    print("non-slot80 receiver frontier v18 smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
