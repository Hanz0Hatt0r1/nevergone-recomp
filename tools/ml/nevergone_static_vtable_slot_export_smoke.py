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


def slot_row(start: int, symbol: str, ap: int, off: int, target: int, name: str, next_start: str) -> str:
    raw = target - 0x10000
    if raw & 1 == 0:
        raw |= 1
    target_entry = target & ~1
    return (
        f"0x{start:x}\t{symbol}\t0x{ap:x}\t0x{off:x}\t0x{ap + off:x}\t"
        f"0x{raw:x}\ttrue\t{target_entry:08x}\t{name}\t"
        f"undefined __thiscall {name.split('::')[-1]}(void * this)\t"
        f"image_base_plus_module_offset\t{next_start}\n"
    )


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

        rows: list[str] = []
        action_targets = {
            0x00: (0x29e900, "ActionData::v0"),
            0x04: (0x29e920, "ActionData::v1"),
            0x08: (0x29ea5c, "ActionData::~ActionData"),
            0x0c: (0x290000, "ActionData::staticOnly"),
            0x10: (0x29e940, "ActionData::v4"),
            0x14: (0x29e960, "ActionData::v5"),
            0x18: (0x29e980, "ActionData::v6"),
            0x1c: (0x29e9a0, "ActionData::v7"),
            0x20: (0x29e9c0, "ActionData::v8"),
            0x24: (0x29e9ba, "ActionData::init"),
        }
        for off, (target, name) in action_targets.items():
            rows.append(slot_row(0x8f3920, "_ZTV10ActionData", 0x8f3928, off, target, name, "0x8f3950"))

        enemy_targets = {
            0x00: (0x29ea80, "EenemyActionDataSet::v0"),
            0x04: (0x29eaa0, "EenemyActionDataSet::v1"),
            0x08: (0x29eaec, "EenemyActionDataSet::~EenemyActionDataSet"),
            0x0c: (0x29eb00, "EenemyActionDataSet::v3"),
            0x10: (0x29eb10, "EenemyActionDataSet::v4"),
            0x14: (0x29eb20, "EenemyActionDataSet::v5"),
            0x18: (0x29eb30, "EenemyActionDataSet::v6"),
            0x1c: (0x29eb38, "EenemyActionDataSet::v7"),
            0x20: (0x29eb40, "EenemyActionDataSet::v8"),
            0x24: (0x29eb4a, "EenemyActionDataSet::init"),
        }
        for off, (target, name) in enemy_targets.items():
            rows.append(slot_row(0x8f3950, "_ZTV19EenemyActionDataSet", 0x8f3958, off, target, name, "0x900000"))

        # Separate synthetic table for contamination behavior. Its 0x8 slot is
        # deliberately absent; a later valid-looking function pointer at 0xc must
        # therefore be rejected by the contiguous-prefix filter.
        rows.append(slot_row(0x900000, "_ZTV8GapClass", 0x900008, 0x0,
                             0x2a0000, "GapClass::v0", "-"))
        rows.append(slot_row(0x900000, "_ZTV8GapClass", 0x900008, 0x4,
                             0x2a0020, "GapClass::v1", "-"))
        rows.append(slot_row(0x900000, "_ZTV8GapClass", 0x900008, 0xc,
                             0x2a1234, "GapClass::stray", "-"))

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
        assert len(catalog) == 22
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
            assert row["source_kind"] == "ghidra_primary_vtable_contiguous_memory_pointer"
            assert row["static_candidate_affects_routing"] is False
            assert row["cross_vtable_transfer_allowed"] is False
            assert row["cross_class_transfer_allowed"] is False
            assert row["primary_prefix_slot_count"] == 10

        static_only = by_key[("ActionData", "0xc")]
        assert static_only["candidate_target_ids"] == ["libcocos2dcpp.so:00290000"]
        assert static_only["runtime_validation_status"] == "runtime_unobserved"
        assert static_only["runtime_verified"] is False
        assert static_only["target_functions"][0]["thumb_observed_in_pointer"] is True
        assert static_only["target_functions"][0]["resolution_modes"] == ["image_base_plus_module_offset"]

        assert ("GapClass", "0x0") in by_key
        assert ("GapClass", "0x4") in by_key
        assert ("GapClass", "0xc") not in by_key
        assert by_key[("GapClass", "0x0")]["primary_prefix_slot_count"] == 2

    print("static vtable slot export smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
