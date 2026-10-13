#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/ml/build_nevergone_class_dispatch_catalog.py"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def main() -> int:
    subprocess.run([sys.executable, "-m", "py_compile", str(TOOL)], check=True)

    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        keys = tmp / "keys.jsonl"
        provenance = tmp / "provenance.jsonl"
        out = tmp / "catalog.jsonl"

        write_jsonl(keys, [
            {
                "format": "NEVERGONE.DispatchReceiverKey/1",
                "receiver_dispatch_key": "libcocos2dcpp.so:008f3928@slot=0x24",
                "receiver_vtable_id": "libcocos2dcpp.so:008f3928",
                "dispatch_slot_offset": "0x24",
                "slot_group_ids": ["67db8756fb2cfb7b@0x24"],
                "observation_count": 1,
                "source_call_count": 1,
                "target_ids": ["libcocos2dcpp.so:0029e9ba"],
                "status": "single_observation",
            },
            {
                "format": "NEVERGONE.DispatchReceiverKey/1",
                "receiver_dispatch_key": "libcocos2dcpp.so:008f3958@slot=0x24",
                "receiver_vtable_id": "libcocos2dcpp.so:008f3958",
                "dispatch_slot_offset": "0x24",
                "slot_group_ids": ["67db8756fb2cfb7b@0x24"],
                "observation_count": 1,
                "source_call_count": 1,
                "target_ids": ["libcocos2dcpp.so:0029eb4a"],
                "status": "single_observation",
            },
        ])
        write_jsonl(provenance, [
            {
                "format": "NEVERGONE.VtableStaticProvenance/2",
                "receiver_vtable_id": "libcocos2dcpp.so:008f3928",
                "status": "resolved_explicit_symbol",
                "resolved_class_name": "ActionData",
            },
            {
                "format": "NEVERGONE.VtableStaticProvenance/2",
                "receiver_vtable_id": "libcocos2dcpp.so:008f3958",
                "status": "resolved_explicit_symbol",
                "resolved_class_name": "EenemyActionDataSet",
            },
        ])

        subprocess.run([
            sys.executable, str(TOOL), str(keys), str(provenance),
            "--output", str(out),
        ], check=True)

        rows = [json.loads(line) for line in out.read_text(encoding="utf-8").splitlines() if line.strip()]
        assert len(rows) == 2
        by_class = {row["resolved_class_name"]: row for row in rows}
        a = by_class["ActionData"]
        e = by_class["EenemyActionDataSet"]
        assert a["catalog_status"] == "verified_single_target"
        assert e["catalog_status"] == "verified_single_target"
        assert a["class_slot_key"] == "ActionData@slot=0x24"
        assert e["class_slot_key"] == "EenemyActionDataSet@slot=0x24"
        assert a["target_ids"] == ["libcocos2dcpp.so:0029e9ba"]
        assert e["target_ids"] == ["libcocos2dcpp.so:0029eb4a"]
        assert a["target_transfer_scope"] == "same_receiver_vtable_only"
        assert a["cross_vtable_transfer_allowed"] is False
        assert a["cross_class_transfer_allowed"] is False
        assert a["class_inferred_from_target_name"] is False

    print("class dispatch catalog smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
