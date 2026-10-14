#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FINALIZER = ROOT / "tools/ml/finalize_nevergone_caller_argument_subtree_recovery.py"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def audit_row(source_id: str, classes: list[str], calls: list[list[str]], status: str) -> dict:
    return {
        "format": "NEVERGONE.CallerArgumentSubtreeAudit/1",
        "source_id": source_id,
        "call_index": 0,
        "summary_status": "complete_caller_argument_subtrees",
        "direct_caller_functions": 2,
        "matched_caller_calls": 2,
        "candidate_value_classes": classes,
        "calls": [
            {
                "caller_entry": f"0020000{i}",
                "call_site": f"0020001{i}",
                "root_opcode": "COPY",
                "value_classes": value_classes,
                "value_typed_nodes": [],
                "leaves": [],
            }
            for i, value_classes in enumerate(calls)
        ],
        "status": status,
        "affects_routing": False,
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        inf = root / "v9.jsonl"
        q = root / "q9.jsonl"
        plan = root / "plan.tsv"
        audit = root / "audit.jsonl"
        catalog = root / "catalog.jsonl"
        out = root / "v10.jsonl"
        outq = root / "q10.jsonl"
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
            {**base, "source_id": "libtest.so:00100020", "source_entry": "00100020", "call_address": "00100024"},
            {
                **base,
                "source_id": "libtest.so:00100030",
                "source_entry": "00100030",
                "call_address": "00100034",
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
            "source_id": "libtest.so:00100030",
            "call_index": 0,
            "call_address": "00100034",
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
            fields = [
                "source_id", "source_entry", "source_call_index", "parameter_index",
                "dispatch_slot_offset", "expected_direct_caller_functions", "expected_matched_caller_calls",
            ]
            writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            for source_id, entry in [
                ("libtest.so:00100000", "00100000"),
                ("libtest.so:00100010", "00100010"),
                ("libtest.so:00100020", "00100020"),
            ]:
                writer.writerow({
                    "source_id": source_id,
                    "source_entry": entry,
                    "source_call_index": 0,
                    "parameter_index": 1,
                    "dispatch_slot_offset": "0x7c",
                    "expected_direct_caller_functions": 2,
                    "expected_matched_caller_calls": 2,
                })

        write_jsonl(audit, [
            audit_row(
                "libtest.so:00100000",
                ["Test::Sprite"],
                [["Test::Sprite"], ["Test::Sprite"]],
                "all_caller_arguments_converge_one_value_class",
            ),
            audit_row(
                "libtest.so:00100010",
                ["Test::Other", "Test::Sprite"],
                [["Test::Sprite"], ["Test::Other"]],
                "caller_argument_value_types_inconclusive",
            ),
            audit_row(
                "libtest.so:00100020",
                ["Test::Ambig"],
                [["Test::Ambig"], ["Test::Ambig"]],
                "all_caller_arguments_converge_one_value_class",
            ),
        ])
        write_jsonl(catalog, [
            {"format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Test::Sprite", "receiver_vtable_id": "vt_sprite"},
            {"format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Test::Other", "receiver_vtable_id": "vt_other"},
            {"format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Test::Ambig", "receiver_vtable_id": "vt_ambig_a"},
            {"format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Test::Ambig", "receiver_vtable_id": "vt_ambig_b"},
            {"format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Test::Existing", "receiver_vtable_id": "vt_existing"},
        ])

        subprocess.run([
            sys.executable, str(FINALIZER), str(inf), str(q), str(plan), str(audit), str(catalog),
            "--output", str(out), "--queries-output", str(outq), "--details-output", str(details),
        ], check=True, capture_output=True, text=True)

        output = [json.loads(x) for x in out.read_text().splitlines() if x.strip()]
        by_id = {x["source_id"]: x for x in output}
        accepted = by_id["libtest.so:00100000"]
        mixed = by_id["libtest.so:00100010"]
        ambiguous = by_id["libtest.so:00100020"]
        existing = by_id["libtest.so:00100030"]

        assert accepted["status"] == "resolved_unique_vtable"
        assert accepted["class_name"] == "Test::Sprite"
        assert accepted["receiver_vtable_id"] == "vt_sprite"
        assert accepted["class_evidence_source"] == "complete_direct_caller_argument_value_subtree_convergence"
        assert accepted["caller_argument_subtree_recovery"]["accepted"] is True

        assert mixed["status"] == "receiver_not_single_pointer"
        assert "caller_arguments_not_single_convergent_class" in mixed["caller_argument_subtree_recovery"]["reasons"]
        assert "caller_value_class_not_single" in mixed["caller_argument_subtree_recovery"]["reasons"]

        assert ambiguous["status"] == "receiver_not_single_pointer"
        assert ambiguous["caller_argument_subtree_recovery"]["reasons"] == ["caller_value_class_vtable_not_unique"]
        assert existing["status"] == "resolved_unique_vtable"

        qrows = [json.loads(x) for x in outq.read_text().splitlines() if x.strip()]
        assert {(x["source_id"], x["call_index"]) for x in qrows} == {
            ("libtest.so:00100000", 0),
            ("libtest.so:00100030", 0),
        }
        print("Nevergone caller argument subtree recovery smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
