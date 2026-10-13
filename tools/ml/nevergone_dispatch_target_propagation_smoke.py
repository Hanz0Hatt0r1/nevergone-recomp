#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def make_row(record_id: str, final_action: str = "review") -> dict:
    memberships = [
        {
            "call_index": 0,
            "call_address": "00100010",
            "slot_group_id": "shapeA@0x10",
        },
        {
            "call_index": 1,
            "call_address": "00100020",
            "slot_group_id": "shapeB@0x14",
        },
    ]
    return {
        "id": record_id,
        "final_action": final_action,
        "dispatch_family_evidence": {
            "present": True,
            "memberships": memberships,
        },
        "dispatch_equivalence_evidence": {
            "present": True,
            "ordered_sequence_group_id": "ordered_sequence:test",
            "ordered_sequence_group_size": 3,
        },
    }


def run_tool(root: Path, analysis: Path, seeds: Path, output: Path, candidates: Path) -> str:
    tool = root / "tools/ml/propagate_nevergone_dispatch_target_evidence.py"
    result = subprocess.run(
        [
            sys.executable,
            str(tool),
            str(analysis),
            str(seeds),
            "--output",
            str(output),
            "--candidates-output",
            str(candidates),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout


def read_jsonl(path: Path) -> list[dict]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        analysis = tmpdir / "analysis.jsonl"
        write_jsonl(
            analysis,
            [
                make_row("prog:00100000", "review"),
                make_row("prog:00200000", "escalate"),
                make_row("prog:00300000", "accept"),
            ],
        )

        seeds = tmpdir / "seeds.jsonl"
        output = tmpdir / "out.jsonl"
        candidates = tmpdir / "candidates.jsonl"
        write_jsonl(
            seeds,
            [
                {
                    "source_id": "prog:00100000",
                    "call_index": 0,
                    "target_id": "target:T1",
                    "verified": True,
                    "evidence_kind": "synthetic",
                }
            ],
        )
        stdout = run_tool(root, analysis, seeds, output, candidates)
        assert "verified seeds: 1" in stdout
        cand = read_jsonl(candidates)
        assert len(cand) == 3, cand
        for item in cand:
            assert item["candidate_target_ids"] == ["target:T1"]
            assert item["conflict"] is False
            assert item["has_exact_ordered_position_evidence"] is True
            assert "exact_ordered_position" in item["propagation_bases"]
        enriched = read_jsonl(output)
        assert [row["final_action"] for row in enriched] == ["review", "escalate", "accept"]

        write_jsonl(
            seeds,
            [
                {
                    "source_id": "prog:00100000",
                    "call_index": 0,
                    "target_id": "target:T1",
                    "verified": True,
                },
                {
                    "source_id": "prog:00200000",
                    "call_index": 0,
                    "target_id": "target:T2",
                    "verified": True,
                },
            ],
        )
        stdout = run_tool(root, analysis, seeds, output, candidates)
        assert "verified seeds: 2" in stdout
        cand = read_jsonl(candidates)
        assert len(cand) == 3, cand
        assert all(item["conflict"] is True for item in cand)
        assert all(item["candidate_target_ids"] == ["target:T1", "target:T2"] for item in cand)
        enriched = read_jsonl(output)
        assert [row["final_action"] for row in enriched] == ["review", "escalate", "accept"]

    print("dispatch target propagation smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
