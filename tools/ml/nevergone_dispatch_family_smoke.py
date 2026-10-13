#!/usr/bin/env python3
"""Dependency-light smoke test for structural dispatch family analysis."""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        source = root / "analysis.jsonl"
        output = root / "out.jsonl"
        families = root / "families.jsonl"

        rows = [
            {
                "id": "lib.so:001000",
                "final_action": "review",
                "indirect_call_slice_evidence": {
                    "present": True,
                    "calls": [
                        {
                            "call_index": 0,
                            "call_address": "001010",
                            "shape_hash": "shapeA",
                            "primary_slot_offset_candidate": "0x10",
                            "node_count": 10,
                            "complete": True,
                        },
                        {
                            "call_index": 1,
                            "call_address": "001020",
                            "shape_hash": "shapeA",
                            "primary_slot_offset_candidate": "0x14",
                            "node_count": 10,
                            "complete": True,
                        },
                    ],
                },
            },
            {
                "id": "lib.so:002000",
                "final_action": "escalate",
                "indirect_call_slice_evidence": {
                    "present": True,
                    "calls": [
                        {
                            "call_index": 0,
                            "call_address": "002010",
                            "shape_hash": "shapeA",
                            "primary_slot_offset_candidate": "0x10",
                            "node_count": 11,
                            "complete": True,
                        },
                        {
                            "call_index": 1,
                            "call_address": "002020",
                            "shape_hash": "shapeB",
                            "primary_slot_offset_candidate": None,
                            "node_count": 7,
                            "complete": True,
                        },
                    ],
                },
            },
        ]
        with source.open("w", encoding="utf-8") as handle:
            for row in rows:
                handle.write(json.dumps(row) + "\n")

        tool = Path(__file__).with_name("analyze_nevergone_dispatch_families.py")
        subprocess.run(
            [
                sys.executable,
                str(tool),
                str(source),
                "--output",
                str(output),
                "--families-output",
                str(families),
            ],
            check=True,
        )

        out_rows = [json.loads(line) for line in output.read_text(encoding="utf-8").splitlines() if line]
        assert [row["final_action"] for row in out_rows] == ["review", "escalate"]
        first = out_rows[0]["dispatch_family_evidence"]
        assert first["present"] is True
        assert first["family_ids"] == ["shape:shapeA"]
        assert set(first["slot_group_ids"]) == {"shapeA@0x10", "shapeA@0x14"}

        family_rows = [json.loads(line) for line in families.read_text(encoding="utf-8").splitlines() if line]
        shape_a = next(row for row in family_rows if row["shape_hash"] == "shapeA")
        assert shape_a["kind"] == "repeated_shape_multi_slot"
        assert shape_a["call_count"] == 3
        assert shape_a["function_count"] == 2
        assert shape_a["distinct_slot_count"] == 2
        assert shape_a["slot_offset_counts"] == {"0x10": 2, "0x14": 1}
        assert shape_a["slot_stride_gcd"] == "0x4"
        slots = {item["offset"]: item for item in shape_a["slot_groups"]}
        assert slots["0x10"]["call_count"] == 2
        assert slots["0x10"]["function_count"] == 2

        shape_b = next(row for row in family_rows if row["shape_hash"] == "shapeB")
        assert shape_b["kind"] == "singleton_shape"
        assert shape_b["slot_offset_counts"] == {"none": 1}

    print("dispatch family smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
