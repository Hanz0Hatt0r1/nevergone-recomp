#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / "tools/ml/audit_nevergone_singleton_store_return.py"

FIELDS = [
    "format", "function_entry", "function_name", "row_type", "op_address", "op_time",
    "value_root_token", "address_root_token", "call_target_entry", "call_target_name",
    "call_arg0_root_token", "call_output_root_token",
]


def write_tsv(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def run_case(root: Path, wrong_return: bool, thumb_proof: str | None = None) -> dict:
    prov = root / ("bad.tsv" if wrong_return else "ok.tsv")
    catalog = root / "catalog.jsonl"
    details = root / ("bad.jsonl" if wrong_return else "ok.jsonl")
    object_root = "def:CALL@003e9604:5:4"
    singleton_cell = "0x9755c4"
    rows = [
        {
            "format": "NEVERGONE.GhidraSingletonStoreReturnProvenance/1",
            "function_entry": "003e95e0", "function_name": "GameSceneUI::sharedGameSceneUI()",
            "row_type": "call", "op_address": "003e960c", "op_time": "6",
            "value_root_token": "-", "address_root_token": "-", "call_target_entry": "003e631c",
            "call_target_name": "GameSceneUI::GameSceneUI()", "call_arg0_root_token": object_root,
            "call_output_root_token": "-",
        },
        {
            "format": "NEVERGONE.GhidraSingletonStoreReturnProvenance/1",
            "function_entry": "003e95e0", "function_name": "GameSceneUI::sharedGameSceneUI()",
            "row_type": "store", "op_address": "003e9618", "op_time": "8",
            "value_root_token": object_root, "address_root_token": singleton_cell,
            "call_target_entry": "-", "call_target_name": "-", "call_arg0_root_token": "-",
            "call_output_root_token": "-",
        },
        {
            "format": "NEVERGONE.GhidraSingletonStoreReturnProvenance/1",
            "function_entry": "003e95e0", "function_name": "GameSceneUI::sharedGameSceneUI()",
            "row_type": "return", "op_address": "003e962c", "op_time": "10",
            "value_root_token": "def:LOAD@003e9628:9:4",
            "address_root_token": "0x9755c8" if wrong_return else singleton_cell,
            "call_target_entry": "-", "call_target_name": "-", "call_arg0_root_token": "-",
            "call_output_root_token": "-",
        },
    ]
    if thumb_proof is not None:
        rows.append({
            "format": "NEVERGONE.GhidraSingletonStoreReturnProvenance/1",
            "function_entry": "003e95e0", "function_name": "GameSceneUI::sharedGameSceneUI()",
            "row_type": "thumb_exact_cell_proof", "op_address": "-", "op_time": "-",
            "value_root_token": "0x94efd4" if thumb_proof == "wrong_cell" else "0x94efd0",
            "address_root_token": "0x94efd0", "call_target_entry": "-", "call_target_name": "-",
            "call_arg0_root_token": "-",
            "call_output_root_token": (
                "sha256:incorrect" if thumb_proof == "wrong_digest"
                else "sha256:ceb4bb6eb7801f478a82b5d83fa1932c28d9fb202951d3929ea1e041b2720907"
            ),
        })
    write_tsv(prov, rows)
    write_jsonl(catalog, [{
        "format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "GameSceneUI",
        "receiver_vtable_id": "lib.so:vt_gamesceneui", "dispatch_slot_offset": "0x80",
        "candidate_target_ids": ["lib.so:00515d2f"], "runtime_validation_status": "runtime_unobserved",
    }])
    subprocess.run([
        sys.executable, str(AUDIT), str(prov), str(catalog),
        "--expected-function", "sharedGameSceneUI", "--expected-class", "GameSceneUI",
        "--details-output", str(details),
    ], check=True, capture_output=True, text=True)
    return json.loads(details.read_text(encoding="utf-8"))


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        ok = run_case(root, False)
        assert ok["status"] == "singleton_constructor_store_return_class", ok
        assert ok["candidate_receiver_class_names"] == ["GameSceneUI"], ok
        bad = run_case(root, True)
        assert bad["status"] == "singleton_constructor_store_return_not_proven", bad
        assert "return_not_from_constructor_singleton_cell" in bad["reasons"], bad
        rescued = run_case(root, True, "valid")
        assert rescued["status"] == "singleton_constructor_store_return_class", rescued
        assert rescued["return_provenance_method"] == "exact_thumb_singleton_cell", rescued
        assert rescued["thumb_cell_proof_valid"] is True, rescued
        for invalid in ("wrong_cell", "wrong_digest"):
            rejected = run_case(root, True, invalid)
            assert rejected["status"] == "singleton_constructor_store_return_not_proven", rejected
            assert "thumb_singleton_cell_proof_invalid" in rejected["reasons"], rejected
    print("Nevergone singleton store-return smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
