#!/usr/bin/env python3
"""Synthetic fail-closed checks for the slot-0x80 evidence-only probe."""
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
PLAN = HERE / "plan_nevergone_slot80_receiver_slices.py"
AUDIT = HERE / "audit_nevergone_slot80_receiver_slices.py"
SLICE_FMT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
CAT_FMT = "NEVERGONE.StaticVtableSlotCatalog/3"
INF_FMT = "NEVERGONE.StaticReceiverVtableInference/1"
HEAD = ["format", "row_type", "function_entry", "call_index", "call_address", "call_time", "recovery_status",
        "slot_offset", "structural_receiver_token", "node_id", "parent_node_id", "input_index", "depth", "node_token",
        "type_display", "pointer_depth", "pointee_type_display", "constant_value", "defining_opcode", "defining_address",
        "defining_time", "call_target_entry", "call_target_name", "call_target_return_type", "call_target_is_thunk",
        "call_target_thunk_target", "truncation", "node_count", "truncated_by_budget", "depth_truncation_count",
        "cycle_count", "max_nodes", "max_depth"]


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def run(*args: str, good: bool = True) -> None:
    p = subprocess.run([sys.executable, *args], text=True, capture_output=True)
    if (p.returncode == 0) != good:
        raise AssertionError(f"unexpected return code {p.returncode}: {p.stdout}\n{p.stderr}")


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        d = Path(td)
        failures, inference, catalog = (d / f for f in ("failures.jsonl", "inference.jsonl", "catalog.jsonl"))
        plan, details, slices = d / "plan.tsv", d / "details.jsonl", d / "slices"
        slices.mkdir()
        src = "libcocos2dcpp.so:002a87d2"
        f = {"source_id": src, "call_index": 3, "call_address": "002a8866", "dispatch_slot_offset": "0x80",
             "status": "ambiguous_receiver_class", "candidate_receiver_class_names": ["ControlsButton", "cocos2d::CCTouch"]}
        write_jsonl(failures, [f])
        write_jsonl(inference, [dict(f, format=INF_FMT)])
        cat = []
        for cls, vt, count in (("ControlsButton", "libcocos2dcpp.so:008f46f8", 119),
                               ("cocos2d::CCTouch", "libcocos2dcpp.so:00935f20", 9)):
            for slot in range(count):
                cat.append({"format": CAT_FMT, "class_name": cls, "receiver_vtable_id": vt,
                            "primary_prefix_slot_count": count, "dispatch_slot_offset": f"0x{slot*4:x}",
                            "candidate_target_count": 1, "candidate_target_ids": ["libcocos2dcpp.so:00525d2e"]})
        write_jsonl(catalog, cat)
        run(str(PLAN), str(failures), str(inference), "--output", str(plan),
            "--expected-calls", "1", "--expected-sources", "1")
        run(str(PLAN), str(failures), str(inference), "--output", str(plan),
            "--expected-calls", "17", good=False)
        run(str(AUDIT), str(plan), str(failures), str(inference), str(catalog), str(slices),
            "--details-output", str(details), "--require-complete", good=False)
        row = json.loads(details.read_text().splitlines()[0])
        assert row["status"] == "structural_slice_not_proven_complete"
        assert row["candidate_class_slot_evidence"]["ControlsButton"]["status"] == "catalog_single_slot_target"
        assert row["candidate_class_slot_evidence"]["cocos2d::CCTouch"]["status"] == "slot_outside_observed_primary_prefix"

        path = slices / "libcocos2dcpp.so_002a87d2.tsv"
        summary = {k: "-" for k in HEAD}
        summary.update(format=SLICE_FMT, row_type="summary", function_entry="002a87d2", call_index="3",
                       call_address="002a8866", recovery_status="canonical_receiver", slot_offset="0x80",
                       node_count="1", truncated_by_budget="0", depth_truncation_count="0", cycle_count="0")
        node = {k: "-" for k in HEAD}
        node.update(format=SLICE_FMT, row_type="node", function_entry="002a87d2", call_index="3",
                    call_address="002a8866", slot_offset="0x80", node_id="0", parent_node_id="-1",
                    node_token="def:LOAD@002a8860:4:4", defining_opcode="LOAD")
        def write_slice() -> None:
            with path.open("w", encoding="utf-8", newline="") as h:
                w = csv.DictWriter(h, fieldnames=HEAD, delimiter="\t")
                w.writeheader()
                w.writerows([summary, node])
        write_slice()
        run(str(AUDIT), str(plan), str(failures), str(inference), str(catalog), str(slices),
            "--details-output", str(details), "--require-complete")
        row = json.loads(details.read_text().splitlines()[0])
        assert row["status"] == "complete_structural_slice" and row["root_opcode"] == "LOAD"
        node["call_address"] = "002a8868"
        write_slice()
        run(str(AUDIT), str(plan), str(failures), str(inference), str(catalog), str(slices),
            "--details-output", str(details), "--require-complete", good=False)
        row = json.loads(details.read_text().splitlines()[0])
        assert "slice_node_identity_mismatch" in row["reasons"]
        write_jsonl(inference, [dict(f, format=INF_FMT, call_address="002a8868")])
        run(str(PLAN), str(failures), str(inference), "--output", str(plan), good=False)
    print("slot 0x80 receiver slice smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
