#!/usr/bin/env python3
from __future__ import annotations

import gzip
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/ml/build_nevergone_static_vtable_slot_catalog_from_export.py"


def write_gz(path: Path, text: str) -> None:
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(text)


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def main() -> int:
    subprocess.run([sys.executable, "-m", "py_compile", str(TOOL)], check=True)

    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        slots = tmp / "slots.tsv.gz"
        runtime = tmp / "runtime.jsonl"
        out = tmp / "catalog.jsonl"

        header = (
            "vtable_symbol_start\tvtable_symbol_name\taddress_point\tslot_offset\t"
            "slot_address\traw_value\ttarget_thumb\ttarget_entry\ttarget_name\t"
            "target_signature\tresolution_mode\tnext_vtable_symbol_start\n"
        )
        rows = [
            "0x8f3920\t_ZTV10ActionData\t0x8f3928\t0x8\t0x8f3930\t0x28ea5d\ttrue\t0029ea5c\tActionData::~ActionData\tundefined __thiscall ~ActionData(ActionData * this)\timage_base_plus_module_offset\t0x8f3950\n",
            "0x8f3920\t_ZTV10ActionData\t0x8f3928\t0xc\t0x8f3934\t0x280001\ttrue\t00290000\tActionData::staticOnly\tundefined __thiscall staticOnly(ActionData * this)\timage_base_plus_module_offset\t0x8f3950\n",
            "0x8f3920\t_ZTV10ActionData\t0x8f3928\t0x24\t0x8f394c\t0x28e9bb\ttrue\t0029e9ba\tActionData::init\tundefined __thiscall init(ActionData * this)\timage_base_plus_module_offset\t0x8f3950\n",
            "0x8f3950\t_ZTV19EenemyActionDataSet\t0x8f3958\t0x8\t0x8f3960\t0x28eaed\ttrue\t0029eaec\tEenemyActionDataSet::~EenemyActionDataSet\tundefined __thiscall ~EenemyActionDataSet(EenemyActionDataSet * this)\timage_base_plus_module_offset\t-\n",
            "0x8f3950\t_ZTV19EenemyActionDataSet\t0x8f3958\t0x24\t0x8f397c\t0x28eb4b\ttrue\t0029eb4a\tEenemyActionDataSet::init\tundefined __thiscall init(EenemyActionDataSet * this)\timage_base_plus_module_offset\t-\n",
        ]
        write_gz(slots, header + "".join(rows))

        write_jsonl(runtime, [
            {
                "format": "NEVERGONE.ClassDispatchCatalog/2",
                "receiver_vtable_id": "libcocos2dcpp.so:008f3928",
                "dispatch_slot_offset": "0x8",
                "target_ids": ["libcocos2dcpp.so:0029ea5c"],
            },
            {
                "format": "NEVERGONE.ClassDispatchCatalog/2",
                "receiver_vtable_id": "libcocos2dcpp.so:008f3928",
                "dispatch_slot_offset": "0x24",
                "target_ids": ["libcocos2dcpp.so:0029e9ba"],
            },
            {
                "format": "NEVERGONE.ClassDispatchCatalog/2",
                "receiver_vtable_id": "libcocos2dcpp.so:008f3958",
                "dispatch_slot_offset": "0x8",
                "target_ids": ["libcocos2dcpp.so:0029eaec"],
            },
            {
                "format": "NEVERGONE.ClassDispatchCatalog/2",
                "receiver_vtable_id": "libcocos2dcpp.so:008f3958",
                "dispatch_slot_offset": "0x24",
                "target_ids": ["libcocos2dcpp.so:0029eb4a"],
            },
        ])

        subprocess.run([
            sys.executable, str(TOOL), str(slots),
            "--runtime-catalog", str(runtime),
            "--output", str(out),
        ], check=True)

        catalog = [json.loads(line) for line in out.read_text(encoding="utf-8").splitlines() if line.strip()]
        assert len(catalog) == 5
        by_key = {(x["class_name"], x["dispatch_slot_offset"]): x for x in catalog}

        expected = {
            ("ActionData", "0x8"): "libcocos2dcpp.so:0029ea5c",
            ("ActionData", "0x24"): "libcocos2dcpp.so:0029e9ba",
            ("EenemyActionDataSet", "0x8"): "libcocos2dcpp.so:0029eaec",
            ("EenemyActionDataSet", "0x24"): "libcocos2dcpp.so:0029eb4a",
        }
        for key, target in expected.items():
            row = by_key[key]
            assert row["candidate_target_ids"] == [target]
            assert row["runtime_validation_status"] == "runtime_match"
            assert row["runtime_verified"] is True
            assert row["static_status"] == "single_function_memory_pointer"
            assert row["source_kind"] == "ghidra_program_memory_pointer"
            assert row["static_candidate_affects_routing"] is False
            assert row["cross_vtable_transfer_allowed"] is False
            assert row["cross_class_transfer_allowed"] is False

        static_only = by_key[("ActionData", "0xc")]
        assert static_only["candidate_target_ids"] == ["libcocos2dcpp.so:00290000"]
        assert static_only["runtime_validation_status"] == "runtime_unobserved"
        assert static_only["runtime_verified"] is False
        assert static_only["target_functions"][0]["thumb_observed_in_pointer"] is True
        assert static_only["target_functions"][0]["resolution_modes"] == ["image_base_plus_module_offset"]

    print("static vtable slot export smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
