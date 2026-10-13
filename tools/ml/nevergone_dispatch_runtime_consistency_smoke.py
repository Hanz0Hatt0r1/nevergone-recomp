#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/ml/analyze_nevergone_dispatch_runtime_consistency.py"


def row(record_id: str, calls: list[tuple[int, str, str]], group_id: str, group_size: int, action: str) -> dict:
    return {
        "id": record_id,
        "final_action": action,
        "dispatch_family_evidence": {
            "present": True,
            "memberships": [
                {"call_index": idx, "call_address": address, "slot_group_id": slot}
                for idx, address, slot in calls
            ],
        },
        "dispatch_equivalence_evidence": {
            "present": True,
            "ordered_sequence_group_id": group_id,
            "ordered_sequence_group_size": group_size,
        },
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        analysis = tmp / "analysis.jsonl"
        seeds = tmp / "seeds.jsonl"
        output = tmp / "output.jsonl"
        positions = tmp / "positions.jsonl"

        gid = "ordered_sequence:test"
        rows = [
            row("lib:a", [(0, "1000", "shapeA@0x24"), (1, "1002", "shapeB@0x8")], gid, 3, "accept"),
            row("lib:b", [(0, "2000", "shapeA@0x24"), (1, "2002", "shapeB@0x8")], gid, 3, "review"),
            row("lib:c", [(0, "3000", "shapeA@0x24"), (1, "3002", "shapeB@0x8")], gid, 3, "escalate"),
        ]
        analysis.write_text("".join(json.dumps(x) + "\n" for x in rows), encoding="utf-8")

        seed_rows = [
            {"format": "NEVERGONE.DispatchTargetSeed/1", "source_id": "lib:a", "call_index": 0, "target_id": "lib:t0", "verified": True},
            {"format": "NEVERGONE.DispatchTargetSeed/1", "source_id": "lib:b", "call_index": 0, "target_id": "lib:t0", "verified": True},
            {"format": "NEVERGONE.DispatchTargetSeed/1", "source_id": "lib:a", "call_index": 1, "target_id": "lib:x", "verified": True},
            {"format": "NEVERGONE.DispatchTargetSeed/1", "source_id": "lib:b", "call_index": 1, "target_id": "lib:y", "verified": True},
        ]
        seeds.write_text("".join(json.dumps(x) + "\n" for x in seed_rows), encoding="utf-8")

        subprocess.run(
            [sys.executable, str(SCRIPT), str(analysis), str(seeds), "--output", str(output), "--positions-output", str(positions)],
            check=True,
        )

        pos = [json.loads(x) for x in positions.read_text(encoding="utf-8").splitlines() if x.strip()]
        assert len(pos) == 2
        p0 = next(x for x in pos if x["ordered_position"] == 0)
        p1 = next(x for x in pos if x["ordered_position"] == 1)
        assert p0["status"] == "consistent"
        assert p0["observed_call_site_count"] == 2
        assert p0["target_ids"] == ["lib:t0"]
        assert p1["status"] == "conflict"
        assert p1["target_ids"] == ["lib:x", "lib:y"]

        enriched = [json.loads(x) for x in output.read_text(encoding="utf-8").splitlines() if x.strip()]
        assert [x["final_action"] for x in enriched] == ["accept", "review", "escalate"]
        for x in enriched:
            evidence = x["dispatch_runtime_consistency_evidence"]
            assert evidence["present"] is True
            assert evidence["consistent_position_count"] == 1
            assert evidence["conflict_position_count"] == 1

    print("dispatch runtime consistency smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
