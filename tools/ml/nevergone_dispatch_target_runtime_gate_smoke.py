#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/ml/gate_nevergone_dispatch_target_candidates.py"


def dump(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(x) + "\n" for x in rows), encoding="utf-8")


def load(path: Path) -> list[dict]:
    return [json.loads(x) for x in path.read_text(encoding="utf-8").splitlines() if x.strip()]


def candidate(source_id: str, call_index: int, address: str, target_a: str, target_b: str) -> dict:
    return {
        "format": "NEVERGONE.DispatchTargetCandidate/1",
        "id": source_id,
        "call_index": call_index,
        "call_address": address,
        "slot_group_id": "shape@0x24",
        "ordered_position": 0,
        "candidate_target_ids": [target_a, target_b],
        "candidate_target_count": 2,
        "conflict": True,
        "status": "conflict",
        "evidence": [
            {
                "target_id": target_a,
                "basis": "exact_ordered_position",
                "seed_source_id": "lib.so:A",
                "seed_call_index": 0,
            },
            {
                "target_id": target_a,
                "basis": "same_slot_group",
                "seed_source_id": "lib.so:A",
                "seed_call_index": 0,
            },
            {
                "target_id": target_b,
                "basis": "exact_ordered_position",
                "seed_source_id": "lib.so:B",
                "seed_call_index": 0,
            },
            {
                "target_id": target_b,
                "basis": "same_slot_group",
                "seed_source_id": "lib.so:B",
                "seed_call_index": 0,
            },
        ],
    }


def main() -> int:
    subprocess.run([sys.executable, "-m", "py_compile", str(TOOL)], check=True)

    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        analysis = tmp / "analysis.jsonl"
        positions = tmp / "positions.jsonl"
        seeds = tmp / "seeds.jsonl"
        output = tmp / "output.jsonl"
        candidates = tmp / "candidates.jsonl"
        transfer = tmp / "transfer.jsonl"

        target_a = "lib.so:TA"
        target_b = "lib.so:TB"
        analysis_rows = []
        for source_id, address in (("lib.so:A", "1000"), ("lib.so:B", "2000"), ("lib.so:C", "3000")):
            analysis_rows.append(
                {
                    "id": source_id,
                    "final_action": "review",
                    "dispatch_target_candidate_evidence": {
                        "present": True,
                        "calls": [candidate(source_id, 0, address, target_a, target_b)],
                    },
                }
            )
        dump(analysis, analysis_rows)

        dump(
            positions,
            [
                {
                    "ordered_group_id": "ordered_sequence:test",
                    "ordered_position": 0,
                    "slot_group_id": "shape@0x24",
                    "status": "conflict",
                    "target_ids": [target_a, target_b],
                    "observations": [
                        {"source_id": "lib.so:A", "call_index": 0, "call_address": "1000", "observed": True, "target_ids": [target_a]},
                        {"source_id": "lib.so:B", "call_index": 0, "call_address": "2000", "observed": True, "target_ids": [target_b]},
                        {"source_id": "lib.so:C", "call_index": 0, "call_address": "3000", "observed": False, "target_ids": []},
                    ],
                }
            ],
        )

        dump(
            seeds,
            [
                {"source_id": "lib.so:A", "call_index": 0, "target_id": target_a, "verified": True},
                {"source_id": "lib.so:B", "call_index": 0, "target_id": target_b, "verified": True},
            ],
        )

        subprocess.run(
            [
                sys.executable,
                str(TOOL),
                str(analysis),
                str(positions),
                str(seeds),
                "--output",
                str(output),
                "--candidates-output",
                str(candidates),
                "--transferability-output",
                str(transfer),
            ],
            check=True,
        )

        rows = {x["id"]: x for x in load(candidates)}
        assert rows["lib.so:A"]["status"] == "verified_local_only"
        assert rows["lib.so:A"]["candidate_target_ids"] == [target_a]
        assert rows["lib.so:B"]["status"] == "verified_local_only"
        assert rows["lib.so:B"]["candidate_target_ids"] == [target_b]
        assert rows["lib.so:C"]["status"] == "blocked_runtime_conflict"
        assert rows["lib.so:C"]["candidate_target_ids"] == []
        assert rows["lib.so:C"]["slot_group_runtime_nontransferable"] is True
        assert all(x["removed_evidence_count"] > 0 for x in rows.values())

        transfer_rows = load(transfer)
        assert transfer_rows == [
            {
                "format": "NEVERGONE.DispatchTargetRuntimeGate/1",
                "slot_group_id": "shape@0x24",
                "target_transferability": "runtime_disproven",
                "conflict_position_count": 1,
                "conflict_positions": [
                    {
                        "ordered_group_id": "ordered_sequence:test",
                        "ordered_position": 0,
                        "slot_group_id": "shape@0x24",
                        "target_ids": [target_a, target_b],
                        "call_keys": [["lib.so:A", 0], ["lib.so:B", 0], ["lib.so:C", 0]],
                    }
                ],
            }
        ]

        enriched = load(output)
        assert all(x["final_action"] == "review" for x in enriched)

    print("dispatch target runtime gate smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
