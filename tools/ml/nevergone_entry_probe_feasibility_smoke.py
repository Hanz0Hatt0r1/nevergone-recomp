#!/usr/bin/env python3
from __future__ import annotations

import gzip
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/ml/analyze_nevergone_entry_probe_candidates.py"


def main() -> int:
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        analysis = tmp / "analysis.jsonl"
        functions = tmp / "functions.tsv.gz"
        calls = tmp / "calls.tsv.gz"
        output = tmp / "out.jsonl"

        rows = [
            {
                "id": "libcocos2dcpp.so:00001000",
                "dispatch_equivalence_evidence": {
                    "present": True,
                    "ordered_sequence_group_id": "ordered_sequence:test",
                    "ordered_sequence_group_size": 2,
                },
                "dispatch_family_evidence": {"memberships": [{"call_index": 0}]},
            },
            {
                "id": "libcocos2dcpp.so:00002000",
                "dispatch_equivalence_evidence": {
                    "present": True,
                    "ordered_sequence_group_id": "ordered_sequence:test",
                    "ordered_sequence_group_size": 2,
                },
                "dispatch_family_evidence": {"memberships": [{"call_index": 0}]},
            },
            {
                "id": "libcocos2dcpp.so:00003000",
                "dispatch_equivalence_evidence": {"present": False},
            },
        ]
        analysis.write_text("".join(json.dumps(x) + "\n" for x in rows), encoding="utf-8")

        function_text = (
            "entry\tname\taddress_count\tis_external\tsignature\n"
            "00001000\tFUN_1000\t20\tfalse\tvoid FUN_1000(int a, int b)\n"
            "00002000\tFUN_2000\t8\tfalse\tvoid FUN_2000(int a)\n"
            "00004000\tFUN_4000\t4\tfalse\tvoid FUN_4000(void)\n"
            "00005000\tFUN_5000\t6\tfalse\tvoid FUN_5000(int a)\n"
        )
        with gzip.open(functions, "wt", encoding="utf-8") as f:
            f.write(function_text)

        call_text = (
            "caller_entry\tcall_site\tcallee_entry\n"
            "00004000\t00004010\t00001000\n"
            "00005000\t00005010\t00002000\n"
            "00002000\t00002020\t00005000\n"
        )
        with gzip.open(calls, "wt", encoding="utf-8") as f:
            f.write(call_text)

        subprocess.run(
            [
                sys.executable,
                str(TOOL),
                str(analysis),
                str(functions),
                str(calls),
                "--output",
                str(output),
            ],
            check=True,
        )

        out = [json.loads(x) for x in output.read_text(encoding="utf-8").splitlines() if x.strip()]
        assert len(out) == 2
        by_id = {x["id"]: x for x in out}
        a = by_id["libcocos2dcpp.so:00001000"]
        b = by_id["libcocos2dcpp.so:00002000"]
        assert a["parameter_count_proxy"] == 2
        assert b["parameter_count_proxy"] == 1
        assert b["static_inspection_rank"] == 1
        assert b["group_static_inspection_candidate"] is True
        assert a["static_inspection_rank"] == 2
        assert a["direct_callers"][0]["entry"] == "00004000"
        assert a["direct_callers"][0]["call_sites"] == ["00004010"]
        assert b["fan_out"] == 1
        assert all(x["safety_status"] == "inspection_required" for x in out)

    print("entry probe feasibility smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
