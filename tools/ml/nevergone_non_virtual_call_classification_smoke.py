#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def base_row() -> dict:
    return {
        "format": "NEVERGONE.StaticReceiverVtableInference/1",
        "source_id": "libcocos2dcpp.so:0029902c",
        "source_entry": "0029902c",
        "call_index": 0,
        "call_address": "0067aab0",
        "dispatch_slot_offset": None,
        "status": "missing_slot_offset",
        "receiver_vtable_id": None,
        "affects_routing": False,
        "target_transfer_scope": "same_receiver_vtable_and_slot_only",
        "cross_vtable_transfer_allowed": False,
        "cross_class_transfer_allowed": False,
    }


def provenance_row(**overrides: str) -> dict[str, str]:
    row = {
        "format": "NEVERGONE.NonVirtualIndirectCallProvenance/1",
        "source_id": "libcocos2dcpp.so:0029902c",
        "call_index": "0",
        "call_address": "0067aab0",
        "evidence_kind": "arm_pc_relative_linker_veneer",
        "veneer_address": "0067aab0",
        "veneer_literal_address": "0067aab4",
        "veneer_literal_u32": "0xffc0d74c",
        "pc_base": "0067aab8",
        "fixed_target": "00288204",
        "target_block": ".plt",
        "target_symbol": "<EXTERNAL>::memset",
        "target_is_thunk": "1",
        "target_thunk_target": "EXTERNAL:00000003:<EXTERNAL>::memset",
        "affects_routing": "0",
    }
    row.update(overrides)
    return row


def write_provenance(path: Path, rows: list[dict[str, str]]) -> None:
    fields = list(provenance_row().keys())
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def run_case(tmp: Path, name: str, inf_rows: list[dict], prov_rows: list[dict[str, str]], expect_ok: bool) -> Path:
    inference = tmp / f"{name}-inference.jsonl"
    provenance = tmp / f"{name}-provenance.tsv"
    output = tmp / f"{name}-output.jsonl"
    write_jsonl(inference, inf_rows)
    write_provenance(provenance, prov_rows)
    result = subprocess.run([
        sys.executable,
        str(ROOT / "tools/ml/classify_nevergone_non_virtual_indirect_calls.py"),
        str(inference), str(provenance), "--output", str(output),
    ], text=True, capture_output=True)
    if expect_ok:
        assert result.returncode == 0, result.stderr
    else:
        assert result.returncode != 0, (result.stdout, result.stderr)
    return output


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        output = run_case(tmp, "valid", [base_row()], [provenance_row()], True)
        row = json.loads(output.read_text(encoding="utf-8").strip())
        assert row["status"] == "non_virtual_indirect_call", row
        assert row["non_virtual_fixed_target"] == "00288204", row
        assert row["non_virtual_target_symbol"] == "<EXTERNAL>::memset", row
        assert row["target_transfer_scope"] == "non_virtual_indirect_call_no_vtable_transfer", row
        assert row["receiver_vtable_id"] is None, row
        assert row["affects_routing"] is False, row

        run_case(tmp, "wrong-address", [base_row()], [provenance_row(call_address="0067aabc")], False)
        run_case(tmp, "not-plt", [base_row()], [provenance_row(target_block=".text")], False)
        run_case(tmp, "not-thunk", [base_row()], [provenance_row(target_is_thunk="0")], False)

        already_resolved = base_row()
        already_resolved["status"] = "resolved_unique_vtable"
        already_resolved["dispatch_slot_offset"] = "0x8"
        run_case(tmp, "resolved-row", [already_resolved], [provenance_row()], False)

    print("non-virtual CALLIND classification smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
