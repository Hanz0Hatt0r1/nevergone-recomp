#!/usr/bin/env python3
"""Check generic indirect-call evidence retains Nevergone TSV compatibility."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    adapter = root / "tools/ml/adapt_revenginstr_indirect_call_evidence.py"
    row = {
        "format": "REVENGINSTR.GhidraIndirectCallEvidence/1",
        "program": "libcocos2dcpp.so",
        "requested": "00001000",
        "function_entry": "0x00001000",
        "call_address": "0x00001008",
        "call_time": 2,
        "target": "def:INT_ADD@00001004:1:4",
        "target_expression": "INT_ADD(register:00002000:4,const:0x4:4)",
        "arg_count": 2,
    }
    with tempfile.TemporaryDirectory(prefix="nevergone-indirect-call-") as temp:
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
            "function_entry\tcall_address\tcall_time\ttarget\ttarget_expression\targ_count\n"
            "00001000\t00001008\t2\tdef:INT_ADD@00001004:1:4\t"
            "INT_ADD(register:00002000:4,const:0x4:4)\t2\n"
        )
        if output.read_text(encoding="utf-8") != expected:
            raise AssertionError("generic indirect-call evidence changed legacy TSV output")

        source.write_text("", encoding="utf-8")
        subprocess.run([sys.executable, str(adapter), str(source), str(output)], check=True)
        if output.read_text(encoding="utf-8") != expected.splitlines()[0] + "\n":
            raise AssertionError("empty generic export did not preserve the legacy TSV header")
    print("Nevergone shared indirect-call adapter smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
