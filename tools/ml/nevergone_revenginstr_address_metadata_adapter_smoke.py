#!/usr/bin/env python3
"""Check generic address metadata stays compatible with Nevergone TSV."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    adapter = root / "tools/ml/adapt_revenginstr_address_metadata.py"
    row = {
        "format": "REVENGINSTR.GhidraAddressMetadata/1",
        "program": "sample",
        "requested": "00401000",
        "address": "0x00401000",
        "block": {
            "name": ".text\tsegment", "start": "0x00401000", "end": "0x00401fff",
            "read": True, "write": False, "execute": True,
        },
        "primary_symbol": "sample_function\nalias",
        "function_at": {
            "entry": "0x00401000", "name": "sample\tfunction",
            "body": "0x00401000..0x00401010", "is_thunk": True,
            "thunk_target": {"entry": "0x00402000", "name": "target_function"},
        },
        "function_containing": {
            "entry": "0x00401000", "name": "sample_function",
            "body": "0x00401000..0x00401010",
        },
        "instruction_mnemonic": "JMP",
        "data_type": None,
        "raw_u32": "0x12345678",
    }
    with tempfile.TemporaryDirectory(prefix="nevergone-address-metadata-") as temp:
        source = Path(temp) / "generic.jsonl"
        output = Path(temp) / "legacy.tsv"
        source.write_text(json.dumps(row) + "\n", encoding="utf-8")
        result = subprocess.run(
            [sys.executable, str(adapter), str(source), str(output)],
            check=False, capture_output=True, text=True,
        )
        if result.returncode:
            raise RuntimeError(result.stderr)
        expected = (
            "format\taddress\tblock\tblock_start\tblock_end\tread\twrite\texecute\t"
            "primary_symbol\tfunction_at_entry\tfunction_at_name\tfunction_at_body\t"
            "function_at_is_thunk\tfunction_at_thunk_target\tfunction_containing_entry\t"
            "function_containing_name\tfunction_containing_body\tinstruction_mnemonic\t"
            "data_type\traw_u32\n"
            "NEVERGONE.GhidraAddressClassification/1\t00401000\t.text segment\t00401000\t"
            "00401fff\t1\t0\t1\tsample_function alias\t00401000\tsample function\t"
            "00401000..00401010\t1\t00402000:target_function\t00401000\t"
            "sample_function\t00401000..00401010\tJMP\t-\t0x12345678\n"
        )
        if output.read_text(encoding="utf-8") != expected:
            raise AssertionError("generic address metadata did not preserve legacy TSV output")
    print("Nevergone shared address metadata adapter smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
