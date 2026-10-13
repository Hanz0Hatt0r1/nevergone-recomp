#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONVERTER = ROOT / "tools/ml/convert_nevergone_dispatch_runtime_trace_to_seeds.py"
CONTEXT = ROOT / "tools/unidbg/src/main/java/nevergone/DispatchEvidenceContext.java"
TRACER = ROOT / "tools/unidbg/src/main/java/nevergone/DispatchTargetRuntimeTrace.java"
FALLBACK = ROOT / "tools/unidbg/src/main/java/nevergone/ActionDataFallbackPathControl.java"
CREATE = ROOT / "tools/unidbg/src/main/java/nevergone/ActionDataCreateProbe.java"


def evidence(row: dict) -> str:
    return "EVIDENCE " + json.dumps(row, sort_keys=True)


def target(source: str, call: int, target_id: str, vtable: str, slot: str, **extra) -> dict:
    row = {
        "schema": 1,
        "kind": "dispatch_runtime_target",
        "source_id": source,
        "call_index": call,
        "call_address": f"00{call + 1:06x}",
        "slot_group_id": f"shape@{slot}",
        "ordered_position": call,
        "ordered_group_id": "ordered_sequence:test",
        "ordered_group_size": 2,
        "target_id": target_id,
        "target_module": "libcocos2dcpp.so",
        "target_module_offset": "0x1234",
        "target_thumb": True,
        "receiver_pointer": "0x14051000",
        "receiver_vtable_id": vtable,
        "receiver_vtable_module": "libcocos2dcpp.so",
        "receiver_vtable_module_offset": "0x8f0000",
        "receiver_vtable_ghidra_address": "0x900000",
        "dispatch_slot_offset": slot,
        "receiver_dispatch_key": f"{vtable}@slot={slot}",
        "receiver_slot_target_raw": "0x12345679",
        "receiver_slot_matches_blx_target": True,
        "receiver_provenance_status": "verified_slot_match",
    }
    row.update(extra)
    return row


def main() -> int:
    source_a = "libcocos2dcpp.so:0029ec10"
    source_b = "libcocos2dcpp.so:002ace4c"
    source_c = "libcocos2dcpp.so:00300000"

    rows = [
        # New tracer: explicit natural origin before bounded intervention.
        target(
            source_a, 0, "libcocos2dcpp.so:0029e9ba", "libcocos2dcpp.so:008f3928", "0x24",
            evidence_origin="natural_runtime", eligible_for_natural_seed=True,
            natural_path_reachability_verified=True,
        ),
        # Legacy fallback marker. The following old-style target lacks origin fields
        # and must still be excluded from natural seeds.
        {
            "schema": 1,
            "kind": "action_data_fallback_return_override",
            "source_id": source_a,
            "forced_return_r0": "0x00000000",
        },
        target(
            source_a, 1, "libcocos2dcpp.so:0029ea5c", "libcocos2dcpp.so:008f3928", "0x8"
        ),
        {
            "schema": 4,
            "kind": "action_data_create_entry",
            "source_id": source_a,
            "fallback_path_requested": True,
        },
        # New tracer explicit intervention is excluded even without a preceding
        # marker in this synthetic log.
        target(
            source_b, 1, "libcocos2dcpp.so:00343444", "libcocos2dcpp.so:009029c8", "0x8",
            evidence_origin="interventional_fallback", eligible_for_natural_seed=False,
            natural_path_reachability_verified=False,
            intervention_kind="forced_r0_zero_after_first_virtual_call_return",
            intervention_source_id=source_b,
        ),
        # Legacy natural row remains accepted for backward compatibility.
        target(
            source_c, 0, "libcocos2dcpp.so:00400000", "libcocos2dcpp.so:00910008", "0x10"
        ),
    ]

    with tempfile.TemporaryDirectory() as raw_tmp:
        tmp = Path(raw_tmp)
        log = tmp / "mixed.log"
        seeds = tmp / "natural.jsonl"
        intervention = tmp / "interventional.jsonl"
        log.write_text("loader noise\n" + "\n".join(evidence(row) for row in rows) + "\n", encoding="utf-8")

        subprocess.run([
            sys.executable,
            str(CONVERTER),
            str(log),
            "--output", str(seeds),
            "--interventional-output", str(intervention),
        ], check=True)

        natural_rows = [json.loads(x) for x in seeds.read_text().splitlines() if x.strip()]
        interventional_rows = [json.loads(x) for x in intervention.read_text().splitlines() if x.strip()]

        assert {(x["source_id"], x["call_index"]) for x in natural_rows} == {
            (source_a, 0),
            (source_c, 0),
        }, natural_rows
        assert all(x["format"] == "NEVERGONE.DispatchTargetSeed/1" for x in natural_rows)
        assert all(x["evidence_origin"] == "natural_runtime" for x in natural_rows)
        assert all(x["eligible_for_natural_seed"] is True for x in natural_rows)
        assert all(x["natural_path_reachability_verified"] is True for x in natural_rows)

        assert {(x["source_id"], x["call_index"]) for x in interventional_rows} == {
            (source_a, 1),
            (source_b, 1),
        }, interventional_rows
        assert all(
            x["format"] == "NEVERGONE.DispatchInterventionalRuntimeEvidence/1"
            for x in interventional_rows
        )
        assert all(x["evidence_origin"] == "interventional_fallback" for x in interventional_rows)
        assert all(x["eligible_for_natural_seed"] is False for x in interventional_rows)
        assert all(x["natural_path_reachability_verified"] is False for x in interventional_rows)
        assert all(x["runtime_blx_target_verified"] is True for x in interventional_rows)
        assert all(x["receiver_slot_binding_verified"] is True for x in interventional_rows)

    context = CONTEXT.read_text(encoding="utf-8")
    tracer = TRACER.read_text(encoding="utf-8")
    fallback = FALLBACK.read_text(encoding="utf-8")
    create = CREATE.read_text(encoding="utf-8")

    assert 'INTERVENTIONAL_FALLBACK = "interventional_fallback"' in context
    assert 'row.put("eligible_for_natural_seed", natural)' in context
    assert 'row.put("natural_path_reachability_verified", natural)' in context
    assert "DispatchEvidenceContext.annotate(row)" in tracer
    assert "DispatchEvidenceContext.markInterventionalFallback(sourceId)" in fallback
    assert fallback.index("reg_write(ArmConst.UC_ARM_REG_R0, 0)") < fallback.index(
        "DispatchEvidenceContext.markInterventionalFallback(sourceId)"
    )
    assert create.count("DispatchEvidenceContext.reset()") >= 2

    print("nevergone dispatch intervention isolation smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
