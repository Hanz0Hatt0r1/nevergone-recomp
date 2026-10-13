#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def row(record_id: str, action: str, slots: list[str]) -> dict:
    memberships = []
    for index, slot in enumerate(slots):
        shape, offset = slot.split("@", 1)
        memberships.append(
            {
                "family_id": f"shape:{shape}",
                "shape_hash": shape,
                "slot_group_id": slot,
                "slot_offset": offset,
                "call_index": index,
                "call_address": f"0x{0x1000 + index * 4:x}",
            }
        )
    return {
        "id": record_id,
        "final_action": action,
        "dispatch_family_evidence": {
            "present": True,
            "memberships": memberships,
        },
    }


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    tool = repo / "tools/ml/analyze_nevergone_dispatch_equivalence.py"
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = root / "input.jsonl"
        output = root / "output.jsonl"
        groups = root / "groups.jsonl"
        pairs = root / "pairs.jsonl"
        source_rows = [
            row("lib.so:0010", "review", ["aaa@0x10", "bbb@0x14"]),
            row("lib.so:0020", "accept", ["aaa@0x10", "bbb@0x14"]),
            row("lib.so:0030", "escalate", ["bbb@0x14", "aaa@0x10"]),
            row("lib.so:0040", "review", ["aaa@0x10"]),
        ]
        source.write_text("".join(json.dumps(item) + "\n" for item in source_rows), encoding="utf-8")
        subprocess.run(
            [
                sys.executable,
                str(tool),
                str(source),
                "--output",
                str(output),
                "--groups-output",
                str(groups),
                "--pairs-output",
                str(pairs),
            ],
            check=True,
        )

        out_rows = [json.loads(line) for line in output.read_text(encoding="utf-8").splitlines() if line]
        by_id = {item["id"]: item for item in out_rows}
        assert by_id["lib.so:0010"]["dispatch_equivalence_evidence"]["ordered_sequence_group_size"] == 2
        assert by_id["lib.so:0010"]["dispatch_equivalence_evidence"]["multiset_group_size"] == 3
        assert by_id["lib.so:0010"]["dispatch_equivalence_evidence"]["slot_set_group_size"] == 3
        assert [item["final_action"] for item in out_rows] == [item["final_action"] for item in source_rows]

        group_rows = [json.loads(line) for line in groups.read_text(encoding="utf-8").splitlines() if line]
        ordered = [item for item in group_rows if item["kind"] == "ordered_sequence"]
        multisets = [item for item in group_rows if item["kind"] == "multiset"]
        sets = [item for item in group_rows if item["kind"] == "slot_set"]
        assert len(ordered) == 1 and ordered[0]["function_count"] == 2
        assert len(multisets) == 1 and multisets[0]["function_count"] == 3
        assert len(sets) == 1 and sets[0]["function_count"] == 3

        pair_rows = [json.loads(line) for line in pairs.read_text(encoding="utf-8").splitlines() if line]
        lookup = {(item["left_id"], item["right_id"]): item for item in pair_rows}
        ab = lookup[("lib.so:0010", "lib.so:0020")]
        ac = lookup[("lib.so:0010", "lib.so:0030")]
        ad = lookup[("lib.so:0010", "lib.so:0040")]
        assert ab["ordered_sequence_equal"] is True
        assert ac["ordered_sequence_equal"] is False and ac["multiset_equal"] is True
        assert ad["slot_set_jaccard"] == 0.5
        assert ad["right_containment"] == 1.0
    print("dispatch equivalence smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
