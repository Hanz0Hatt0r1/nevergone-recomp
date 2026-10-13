#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/ml/analyze_nevergone_dispatch_receiver_provenance.py"


def dump(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(x) + "\n" for x in rows), encoding="utf-8")


def load(path: Path) -> list[dict]:
    return [json.loads(x) for x in path.read_text(encoding="utf-8").splitlines() if x.strip()]


def seed(source: str, call_index: int, vtable: str, target: str, key: str) -> dict:
    return {
        "format": "NEVERGONE.DispatchTargetSeed/1",
        "source_id": source,
        "call_index": call_index,
        "call_address": "00010010",
        "slot_group_id": "shape@0x24",
        "ordered_group_id": "ordered:test",
        "ordered_position": 0,
        "ordered_group_size": 3,
        "target_id": target,
        "verified": True,
        "evidence_kind": "unidbg_runtime_blx_target",
        "evidence_ref": "synthetic",
        "receiver_vtable_id": vtable,
        "receiver_vtable_ghidra_address": "0x20000",
        "dispatch_slot_offset": "0x24",
        "receiver_dispatch_key": key,
        "receiver_slot_matches_blx_target": True,
        "receiver_provenance_status": "verified_slot_match",
    }


def main() -> int:
    subprocess.run([sys.executable, "-m", "py_compile", str(TOOL)], check=True)
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        seeds = tmp / "seeds.jsonl"
        observations = tmp / "observations.jsonl"
        keys = tmp / "keys.jsonl"
        slots = tmp / "slots.jsonl"
        dump(
            seeds,
            [
                seed("lib:a", 0, "lib:vtableA", "lib:targetA", "lib:vtableA@slot=0x24"),
                seed("lib:b", 0, "lib:vtableA", "lib:targetA", "lib:vtableA@slot=0x24"),
                seed("lib:c", 0, "lib:vtableB", "lib:targetB", "lib:vtableB@slot=0x24"),
            ],
        )
        subprocess.run(
            [
                sys.executable,
                str(TOOL),
                str(seeds),
                "--observations-output", str(observations),
                "--keys-output", str(keys),
                "--slots-output", str(slots),
            ],
            check=True,
        )
        key_rows = {x["receiver_dispatch_key"]: x for x in load(keys)}
        assert key_rows["lib:vtableA@slot=0x24"]["status"] == "consistent"
        assert key_rows["lib:vtableA@slot=0x24"]["target_ids"] == ["lib:targetA"]
        assert key_rows["lib:vtableB@slot=0x24"]["status"] == "single_observation"
        slot_rows = load(slots)
        assert len(slot_rows) == 1
        row = slot_rows[0]
        assert row["status"] == "receiver_partitioned"
        assert row["receiver_vtable_count"] == 2
        assert row["target_count"] == 2
        assert row["target_ids_by_receiver_vtable"] == {
            "lib:vtableA": ["lib:targetA"],
            "lib:vtableB": ["lib:targetB"],
        }
    print("dispatch receiver provenance smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
