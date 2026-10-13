#!/usr/bin/env python3
from __future__ import annotations

import gzip
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/ml/build_nevergone_static_vtable_slot_catalog.py"


def write_gz(path: Path, text: str) -> None:
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(text)


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def main() -> int:
    subprocess.run([sys.executable, "-m", "py_compile", str(TOOL)], check=True)

    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        symbols = tmp / "symbols.tsv.gz"
        xrefs = tmp / "xrefs.tsv.gz"
        functions = tmp / "functions.tsv.gz"
        runtime = tmp / "runtime.jsonl"
        out = tmp / "out.jsonl"

        write_gz(symbols, "address\tname\ttype\tsource\tis_primary\n"
                 "008f3920\t_ZTV10ActionData\tLABEL\tIMPORTED\ttrue\n"
                 "008f3950\t_ZTV19EenemyActionDataSet\tLABEL\tIMPORTED\ttrue\n")

        write_gz(functions, "entry\tname\taddress_count\tis_external\tsignature\n"
                 "0029e9ba\tActionData::init\t8\tfalse\tundefined __thiscall init(ActionData * this)\n"
                 "0029ea5c\tActionData::~ActionData\t12\tfalse\tundefined __thiscall ~ActionData(ActionData * this)\n"
                 "0029eb4a\tEenemyActionDataSet::init\t20\tfalse\tundefined __thiscall init(EenemyActionDataSet * this)\n"
                 "0029eaec\tEenemyActionDataSet::~EenemyActionDataSet\t12\tfalse\tundefined __thiscall ~EenemyActionDataSet(EenemyActionDataSet * this)\n"
                 "00290000\tUnobserved::method\t4\tfalse\tundefined method(void)\n")

        # Address point = _ZTV + 8. Therefore ActionData@0x8 is 0x8f3930
        # and EenemyActionDataSet@0x8 is 0x8f3960.
        write_gz(xrefs, "from\tto\ttype\toperand\tsource\n"
                 "008f3930\t0029ea5c\tDATA\t0\tANALYSIS\n"
                 "008f394c\t0029e9ba\tDATA\t0\tANALYSIS\n"
                 "008f3934\t00290000\tDATA\t0\tANALYSIS\n"
                 "008f3960\t0029eaec\tDATA\t0\tANALYSIS\n"
                 "008f397c\t0029eb4a\tDATA\t0\tANALYSIS\n")

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
            sys.executable, str(TOOL), str(symbols), str(xrefs), str(functions),
            "--runtime-catalog", str(runtime),
            "--output", str(out),
        ], check=True)

        rows = [json.loads(line) for line in out.read_text(encoding="utf-8").splitlines() if line.strip()]
        assert len(rows) == 5
        by_key = {(r["receiver_vtable_id"], r["dispatch_slot_offset"]): r for r in rows}

        expected = {
            ("libcocos2dcpp.so:008f3928", "0x8"): "libcocos2dcpp.so:0029ea5c",
            ("libcocos2dcpp.so:008f3928", "0x24"): "libcocos2dcpp.so:0029e9ba",
            ("libcocos2dcpp.so:008f3958", "0x8"): "libcocos2dcpp.so:0029eaec",
            ("libcocos2dcpp.so:008f3958", "0x24"): "libcocos2dcpp.so:0029eb4a",
        }
        for key, target in expected.items():
            row = by_key[key]
            assert row["candidate_target_ids"] == [target]
            assert row["runtime_validation_status"] == "runtime_match"
            assert row["runtime_verified"] is True
            assert row["static_candidate_affects_routing"] is False
            assert row["cross_vtable_transfer_allowed"] is False
            assert row["cross_class_transfer_allowed"] is False

        action_dtor = by_key[("libcocos2dcpp.so:008f3928", "0x8")]
        assert action_dtor["class_name"] == "ActionData"
        assert action_dtor["class_slot_key"] == "ActionData@slot=0x8"
        assert action_dtor["slot_address"] == "0x8f3930"
        assert action_dtor["target_functions"][0]["name"] == "ActionData::~ActionData"
        assert action_dtor["static_status"] == "single_function_xref"

        enemy_dtor = by_key[("libcocos2dcpp.so:008f3958", "0x8")]
        assert enemy_dtor["slot_address"] == "0x8f3960"

        static_only = [r for r in rows if r["candidate_target_ids"] == ["libcocos2dcpp.so:00290000"]]
        assert len(static_only) == 1
        assert static_only[0]["receiver_vtable_id"] == "libcocos2dcpp.so:008f3928"
        assert static_only[0]["dispatch_slot_offset"] == "0xc"
        assert static_only[0]["runtime_validation_status"] == "runtime_unobserved"
        assert static_only[0]["runtime_verified"] is False

    print("static vtable slot catalog smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
