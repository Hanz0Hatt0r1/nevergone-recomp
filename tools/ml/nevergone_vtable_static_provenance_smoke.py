#!/usr/bin/env python3
from __future__ import annotations

import csv
import gzip
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/ml/analyze_nevergone_vtable_static_provenance.py"


def write_gz_tsv(path: Path, header: list[str], rows: list[list[str]]) -> None:
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(header)
        writer.writerows(rows)


def main() -> int:
    subprocess.run([sys.executable, "-m", "py_compile", str(TOOL)], check=True)

    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        keys = tmp / "keys.jsonl"
        symbols = tmp / "symbols.tsv.gz"
        xrefs = tmp / "xrefs.tsv.gz"
        out = tmp / "out.jsonl"

        keys.write_text(
            json.dumps({
                "receiver_vtable_id": "libcocos2dcpp.so:008f3928",
                "receiver_dispatch_key": "libcocos2dcpp.so:008f3928@slot=0x24",
                "target_ids": ["libcocos2dcpp.so:0029e9ba"],
            }) + "\n" +
            json.dumps({
                "receiver_vtable_id": "libcocos2dcpp.so:008f3958",
                "receiver_dispatch_key": "libcocos2dcpp.so:008f3958@slot=0x24",
                "target_ids": ["libcocos2dcpp.so:0029eb4a"],
            }) + "\n" +
            json.dumps({
                "receiver_vtable_id": "libcocos2dcpp.so:008f3988",
                "receiver_dispatch_key": "libcocos2dcpp.so:008f3988@slot=0x10",
                "target_ids": ["libcocos2dcpp.so:00290000"],
            }) + "\n",
            encoding="utf-8",
        )

        write_gz_tsv(
            symbols,
            ["address", "name", "type", "source", "is_primary"],
            [
                ["008f3920", "_ZTV10ActionData", "Label", "IMPORTED", "true"],
                ["008f3950", "_ZTV19EenemyActionDataSet", "Label", "IMPORTED", "true"],
                ["008f3980", "_ZTVN4game8DataNodeE", "Label", "IMPORTED", "true"],
                ["008f3930", "unrelated_symbol", "Label", "DEFAULT", "true"],
            ],
        )
        write_gz_tsv(
            xrefs,
            ["from", "to", "type", "operand", "source"],
            [
                ["0029e8f0", "008f3920", "DATA", "1", "ANALYSIS"],
                ["0029e900", "008f3928", "DATA", "0", "ANALYSIS"],
                ["0029ea80", "008f3950", "DATA", "1", "ANALYSIS"],
                ["00290010", "008f3980", "DATA", "1", "ANALYSIS"],
            ],
        )

        subprocess.run([
            sys.executable, str(TOOL), str(keys), str(symbols), str(xrefs),
            "--output", str(out),
        ], check=True)

        rows = [json.loads(line) for line in out.read_text(encoding="utf-8").splitlines() if line.strip()]
        assert len(rows) == 3
        assert {row["format"] for row in rows} == {"NEVERGONE.VtableStaticProvenance/2"}
        by_id = {row["receiver_vtable_id"]: row for row in rows}

        a = by_id["libcocos2dcpp.so:008f3928"]
        b = by_id["libcocos2dcpp.so:008f3958"]
        nested = by_id["libcocos2dcpp.so:008f3988"]
        assert a["status"] == "resolved_explicit_symbol"
        assert b["status"] == "resolved_explicit_symbol"
        assert nested["status"] == "resolved_explicit_symbol"
        assert a["resolved_class_name"] == "ActionData"
        assert b["resolved_class_name"] == "EenemyActionDataSet"
        assert nested["resolved_class_name"] == "game::DataNode"
        assert a["best_symbol"]["evidence_kind"] == "itanium_mangled_vtable_symbol_decoded"
        assert b["best_symbol"]["evidence_kind"] == "itanium_mangled_vtable_symbol_decoded"
        assert a["best_symbol"]["delta_to_address_point"] == 8
        assert b["best_symbol"]["delta_to_address_point"] == 8
        assert a["best_symbol"]["layout_match"] == "arm32_itanium_address_point_plus_8"
        assert a["class_inferred_from_target_name"] is False
        assert len(a["address_point_xrefs"]) == 1
        assert len(a["vtable_symbol_start_xrefs"]) == 1

    print("vtable static provenance smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
