#!/usr/bin/env python3
"""Synthetic fail-closed tests for quarantined CALLIND receiver value slices."""
from __future__ import annotations

import copy
import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from audit_nevergone_mixed_receiver_value_probe import (
    CALLS, ENTRY, INF_FMT, SLICE_FMT, SLOT, SOURCE,
    audit_one, select_inference,
)

HERE = Path(__file__).resolve().parent
AUDIT = HERE / "audit_nevergone_mixed_receiver_value_probe.py"


def make_inference(call: int) -> dict:
    return {
        "format": INF_FMT, "source_id": SOURCE, "call_index": call,
        "call_address": f"002a76{call:02x}", "dispatch_slot_offset": SLOT,
        "status": "receiver_not_single_pointer",
    }


def make_slice(call: int) -> list[dict[str, str]]:
    base = {
        "format": SLICE_FMT, "function_entry": ENTRY, "call_index": str(call),
        "call_address": f"002a76{call:02x}", "slot_offset": SLOT,
        "recovery_status": "canonical_receiver",
    }
    root = f"def:LOAD@002a76{call:02x}:1:4"
    return [
        dict(base, row_type="summary", structural_receiver_token=root,
             node_count="1", truncated_by_budget="0", depth_truncation_count="0",
             cycle_count="0"),
        dict(base, row_type="node", node_id="0", parent_node_id="-1",
             node_token=root, defining_opcode="LOAD", type_display="int *",
             constant_value="-", truncation="-", call_target_entry="-"),
    ]


def expect_bad(rows: list[dict], expected: str, call: int = 3) -> None:
    audited = audit_one(call, make_inference(call), rows)
    assert audited["status"] == "receiver_slice_incomplete"
    assert expected in audited["reasons"], audited


def main() -> int:
    inf = [make_inference(call) for call in CALLS]
    selected = select_inference(inf)
    assert sorted(selected) == list(CALLS)
    assert select_inference(inf + [make_inference(0)]) == selected
    for bad in (inf[:2], inf + [make_inference(3)]):
        try:
            select_inference(bad)
        except ValueError:
            pass
        else:
            raise AssertionError("missing or duplicated quarantined call accepted")
    stale = copy.deepcopy(inf)
    stale[0]["status"] = "resolved_converged_target"
    try:
        select_inference(stale)
    except ValueError:
        pass
    else:
        raise AssertionError("stale v11 mixed recovery accepted")

    rows = make_slice(3)
    result = audit_one(3, inf[0], rows)
    assert result["status"] == "complete_virtual_receiver_slice"
    assert result["root_opcode"] == "LOAD"
    assert result["receiver_class_proven"] is False
    assert result["affects_routing"] is False

    invalid = copy.deepcopy(rows)
    invalid[1]["call_address"] = "002a7600"
    expect_bad(invalid, "callsite_identity_mismatch")
    invalid = copy.deepcopy(rows)
    invalid[0]["truncated_by_budget"] = "1"
    expect_bad(invalid, "incomplete_receiver_slice")
    invalid = copy.deepcopy(rows)
    invalid[1]["node_token"] = "def:CALL@bad:1:4"
    expect_bad(invalid, "root_identity_mismatch")
    expect_bad(rows[:1], "incomplete_receiver_slice")

    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        inference = d / "inference.jsonl"
        slices = d / "slices.tsv"
        details = d / "details.jsonl"
        inference.write_text("".join(json.dumps(row) + "\n" for row in inf), encoding="utf-8")
        all_rows = [row for call in CALLS for row in make_slice(call)]
        columns = sorted({key for row in all_rows for key in row})
        with slices.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, columns, delimiter="\t")
            writer.writeheader()
            writer.writerows(all_rows)
        command = [sys.executable, str(AUDIT), str(inference), str(slices),
                   "--details-output", str(details), "--require-complete"]
        ok = subprocess.run(command, capture_output=True, text=True)
        assert ok.returncode == 0, ok.stderr + ok.stdout
        assert len(details.read_text().splitlines()) == 3
        all_rows[0]["truncated_by_budget"] = "1"
        with slices.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, columns, delimiter="\t")
            writer.writeheader()
            writer.writerows(all_rows)
        failed = subprocess.run(command, capture_output=True, text=True)
        assert failed.returncode == 2, failed.stderr + failed.stdout

    print("mixed CALLIND receiver value slice smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
