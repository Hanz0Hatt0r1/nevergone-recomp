#!/usr/bin/env python3
from __future__ import annotations

import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FINAL = ROOT / "tools/ml/finalize_nevergone_controlsbutton_sprite_field_recovery_v17.py"
CLASS = "cocos2d::CCSprite"
VTABLE = "libcocos2dcpp.so:009363d8"
TARGET = "libcocos2dcpp.so:00549f1e"
CALLS = {
    ("libcocos2dcpp.so:002a87d2", 3): ("002a8800", "0x104", {"0x2984cc", "0x2985c4"}),
    ("libcocos2dcpp.so:002a87d2", 4): ("002a8820", "0x10c", {"0x298510"}),
    ("libcocos2dcpp.so:002a87d2", 5): ("002a8840", "0x108", {"0x2984ea", "0x2985de"}),
}


def write(path: Path, rows: list[dict]) -> None:
    path.write_text(
        "".join(json.dumps(row, sort_keys=True) + "\n" for row in rows),
        encoding="utf-8",
    )


def fixture():
    inference = []
    candidates = []
    for (source, call), (address, field, stores) in CALLS.items():
        inference.append(
            {
                "format": "NEVERGONE.StaticReceiverVtableInference/1",
                "source_id": source,
                "call_index": call,
                "call_address": address,
                "dispatch_slot_offset": "0x80",
                "status": "ambiguous_receiver_class",
                "candidate_receiver_class_names": ["ControlsButton", "cocos2d::CCTouch"],
            }
        )
        candidates.append(
            {
                "format": "NEVERGONE.Slot80FactoryFieldCandidate/1",
                "source_id": source,
                "call_index": call,
                "call_address": address,
                "dispatch_slot_offset": "0x80",
                "owner_class": "ControlsButton",
                "owner_field_offset": field,
                "factory_class_candidate": CLASS,
                "factory_witnesses": [
                    {"store_elf": store} for store in sorted(stores)
                ],
                "candidate_receiver_vtable_id": VTABLE,
                "candidate_target_id": TARGET,
                "field_writes_exhaustive": False,
                "receiver_class_proven": False,
                "status": "construction_linked_target_candidate",
                "affects_routing": False,
            }
        )
    stability = [
        {
            "format": "NEVERGONE.ControlsButtonOwnerFieldPackagedStability/1",
            "scope": "exact_packaged_apk_normal_code_execution",
            "target_fields": ["0x104", "0x108", "0x10c"],
            "first_hop_closed": True,
            "ccpoint_subobject_channel": {
                "subobject_offset": "0x120",
                "call_count": 7,
                "can_recover_owner": False,
            },
            "targeted_touch_channel": {
                "retained_delegate": "ControlsButton+0xe0",
                "callback_methods_in_19_method_inventory": True,
                "target_fields_written_by_touch_callbacks_or_first_hop": False,
            },
            "parent_backref_channel": {"parent_targets_write_sprite_fields": False},
            "cclayer_lifecycle_channel": {"lifecycle_directly_writes_sprite_fields": False},
            "packaged_script_engine_write_through_reachable": False,
            "packaged_dlsym_setter_lookup_excluded": True,
            "owner_fields_stable": True,
            "receiver_class_proven": False,
            "whole_program_alias_writes_excluded": False,
            "external_injected_code_excluded": False,
            "affects_routing": False,
        }
    ]
    catalog = [
        {
            "format": "NEVERGONE.StaticVtableSlotCatalog/3",
            "class_name": CLASS,
            "receiver_vtable_id": VTABLE,
            "dispatch_slot_offset": "0x80",
            "static_status": "single_function_memory_pointer",
            "candidate_target_ids": [TARGET],
            "runtime_validation_status": "runtime_unobserved",
            "cross_class_transfer_allowed": False,
        }
    ]
    return inference, [], stability, candidates, catalog


def run(inference, queries, stability, candidates, catalog, expect_ok: bool) -> None:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        inputs = [root / name for name in ("inf", "queries", "stability", "candidates", "catalog")]
        for path, rows in zip(inputs, (inference, queries, stability, candidates, catalog)):
            write(path, rows)
        output = root / "out"
        out_queries = root / "out-queries"
        details = root / "details"
        proc = subprocess.run(
            [
                sys.executable,
                str(FINAL),
                *[str(path) for path in inputs],
                "--output",
                str(output),
                "--queries-output",
                str(out_queries),
                "--details-output",
                str(details),
            ],
            capture_output=True,
            text=True,
        )
        if not expect_ok:
            assert proc.returncode != 0, (proc.stdout, proc.stderr)
            return
        assert proc.returncode == 0, (proc.stdout, proc.stderr)
        rows = [json.loads(line) for line in output.read_text().splitlines() if line.strip()]
        qrows = [json.loads(line) for line in out_queries.read_text().splitlines() if line.strip()]
        drows = [json.loads(line) for line in details.read_text().splitlines() if line.strip()]
        assert len(rows) == 3 and len(qrows) == 3 and len(drows) == 3
        assert all(row["status"] == "resolved_unique_vtable" for row in rows)
        assert all(row["class_name"] == CLASS for row in rows)
        assert all(row["accepted"] is True for row in drows)


def main() -> None:
    inf, queries, stability, candidates, catalog = fixture()
    run(inf, queries, stability, candidates, catalog, True)

    bad = copy.deepcopy(stability)
    bad[0]["owner_fields_stable"] = False
    run(inf, queries, bad, candidates, catalog, False)

    bad = copy.deepcopy(stability)
    bad[0]["targeted_touch_channel"]["target_fields_written_by_touch_callbacks_or_first_hop"] = True
    run(inf, queries, bad, candidates, catalog, False)

    bad = copy.deepcopy(candidates)
    bad[0]["owner_field_offset"] = "0x10c"
    run(inf, queries, stability, bad, catalog, False)

    bad = copy.deepcopy(candidates)
    bad[0]["factory_witnesses"] = [{"store_elf": "0x2984cc"}]
    run(inf, queries, stability, bad, catalog, False)

    bad = copy.deepcopy(inf)
    bad[0]["status"] = "unresolved_receiver_class"
    run(bad, queries, stability, candidates, catalog, False)

    bad = copy.deepcopy(inf)
    bad[0]["call_address"] = "002a9999"
    run(bad, queries, stability, candidates, catalog, False)

    bad = copy.deepcopy(catalog)
    bad[0]["candidate_target_ids"] = ["dead"]
    run(inf, queries, stability, candidates, bad, False)

    print("ControlsButton sprite field recovery v17 smoke: ok")


if __name__ == "__main__":
    main()
