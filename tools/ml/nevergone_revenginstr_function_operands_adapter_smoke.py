#!/usr/bin/env python3
"""Check the shared operand adapter preserves the historical TSV contract."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    adapter = root / "tools/ml/adapt_revenginstr_function_operands.py"
    rows = [
        {
            "format": "REVENGINSTR.GhidraFunctionOperand/1",
            "program": "sample",
            "requested": "00401000",
            "function_entry": "0x00401000",
            "instruction": "0x00401004",
            "operand": 1,
            "kind": "immediate",
            "bit_length": 32,
            "signed_value": "-1",
            "unsigned_hex": "0xffffffff",
            "target": None,
            "target_type": None,
            "number_value": None,
        },
        {
            "format": "REVENGINSTR.GhidraFunctionOperand/1",
            "program": "sample",
            "requested": "00401000",
            "function_entry": "0x00401000",
            "instruction": "0x00401008",
            "operand": 0,
            "kind": "referenced-scalar",
            "bit_length": 16,
            "signed_value": "32767",
            "unsigned_hex": "0x7fff",
            "target": "0x00402000",
            "target_type": "word",
            "number_value": None,
        },
        {
            "format": "REVENGINSTR.GhidraFunctionOperand/1",
            "program": "sample",
            "requested": "00401000",
            "function_entry": "0x00401000",
            "instruction": "0x0040100c",
            "operand": 0,
            "kind": "referenced-number",
            "bit_length": None,
            "signed_value": None,
            "unsigned_hex": None,
            "target": "0x00402004",
            "target_type": "dword",
            "number_value": "4294967295",
        },
    ]
    with tempfile.TemporaryDirectory(prefix="nevergone-function-operands-") as temp:
        source = Path(temp) / "generic.jsonl"
        output = Path(temp) / "legacy.tsv"
        source.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")
        result = subprocess.run(
            [sys.executable, str(adapter), str(source), str(output)],
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode:
            raise RuntimeError(result.stderr)
        expected = (
            "function_entry\tinstruction\toperand\tkind\tbit_length\tsigned_value\t"
            "unsigned_hex\ttarget\ttarget_type\tnumber_value\n"
            "00401000\t00401004\t1\timmediate\t32\t-1\t0xffffffff\t-\t-\t-\n"
            "00401000\t00401008\t0\treferenced-scalar\t16\t32767\t0x7fff\t"
            "00402000\tword\t-\n"
            "00401000\t0040100c\t0\treferenced-number\t-\t-\t-\t00402004\t"
            "dword\t4294967295\n"
        )
        if output.read_text(encoding="utf-8") != expected:
            raise AssertionError("generic operand rows did not preserve legacy TSV output")

        broken = Path(temp) / "broken.jsonl"
        broken.write_text('{"format":"wrong"}\n', encoding="utf-8")
        rejected = subprocess.run(
            [sys.executable, str(adapter), str(broken), str(output)],
            check=False,
            capture_output=True,
            text=True,
        )
        if rejected.returncode == 0:
            raise AssertionError("adapter accepted an invalid generic row")
    print("Nevergone shared function operand adapter smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
