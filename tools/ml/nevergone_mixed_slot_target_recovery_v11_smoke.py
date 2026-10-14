#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FINALIZER = ROOT / "tools/ml/finalize_nevergone_mixed_slot_target_recovery.py"
RESOLVER = ROOT / "tools/ml/resolve_nevergone_receiver_queries_v11.py"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as h:
        for row in rows:
            h.write(json.dumps(row, sort_keys=True) + "\n")


def read_jsonl(path: Path) -> list[dict]:
    return [json.loads(x) for x in path.read_text(encoding="utf-8").splitlines() if x.strip()]


def inference(source: str, call: int, status: str, slot: str, vt=None) -> dict:
    return {
        "format": "NEVERGONE.StaticReceiverVtableInference/1",
        "source_id": source,
        "call_index": call,
        "call_address": f"003000{call:x}",
        "dispatch_slot_offset": slot,
        "status": status,
        "receiver_vtable_id": vt,
        "candidate_receiver_class_names": [],
        "candidate_receiver_vtable_ids": [vt] if vt else [],
    }


def query(source: str, call: int, vt: str, slot: str) -> dict:
    return {
        "format": "NEVERGONE.StaticReceiverVtableQuery/1",
        "source_id": source,
        "call_index": call,
        "call_address": "00100004",
        "receiver_vtable_id": vt,
        "dispatch_slot_offset": slot,
        "evidence_ref": f"{source}#call={call}",
        "affects_routing": False,
    }


def catalog(cls: str, vt: str, slot: str, target: str) -> dict:
    return {
        "format": "NEVERGONE.StaticVtableSlotCatalog/3",
        "class_name": cls,
        "receiver_vtable_id": vt,
        "dispatch_slot_offset": slot,
        "candidate_target_ids": [target],
        "candidate_target_count": 1,
        "static_status": "single_function_memory_pointer",
        "runtime_validation_status": "runtime_unobserved",
    }


def audit(target: str) -> dict:
    return {
        "format": "NEVERGONE.GameSceneUIControlsSlotConvergenceAudit/1",
        "source_id": "libcocos2dcpp.so:002a7594",
        "call_indices": [3, 4, 7],
        "dispatch_slot_offset": "0x80",
        "branch_classes": ["GameSceneUI", "Controls"],
        "controls_field_class_proven": True,
        "target_rows_stable": True,
        "class_slots": [
            {
                "class_name": "GameSceneUI",
                "receiver_vtable_id": "libcocos2dcpp.so:00910000",
                "dispatch_slot_offset": "0x80",
                "static_status": "single_function_memory_pointer",
                "candidate_target_ids": [target],
                "target_names": ["cocos2d::CCNode::setVisible"],
                "runtime_validation_status": "runtime_unobserved",
            },
            {
                "class_name": "Controls",
                "receiver_vtable_id": "libcocos2dcpp.so:008f4498",
                "dispatch_slot_offset": "0x80",
                "static_status": "single_function_memory_pointer",
                "candidate_target_ids": [target],
                "target_names": ["cocos2d::CCNode::setVisible"],
                "runtime_validation_status": "runtime_unobserved",
            },
        ],
        "converged_target_id": target,
        "status": "mixed_receiver_slot_target_converges",
        "reasons": [],
        "affects_routing": False,
    }


def strict_audits(target: str, proven: bool = True) -> list[dict]:
    return [
        {
            "format": "NEVERGONE.MixedReceiverSlotTargetConvergenceAudit/1",
            "source_id": "libcocos2dcpp.so:002a7594",
            "call_index": call,
            "dispatch_slot_offset": "0x80",
            "proven_receiver_class_names": ["Controls", "GameSceneUI"],
            "proven_receiver_vtable_ids": [
                "libcocos2dcpp.so:008f4498", "libcocos2dcpp.so:00910000",
            ],
            "class_target_ids": {"Controls": [target], "GameSceneUI": [target]},
            "convergent_target_id": target if proven else None,
            "controls_field_offset": "0x134",
            "status": (
                "unique_target_across_proven_receiver_classes"
                if proven else "mixed_receiver_target_not_proven"
            ),
            "reasons": [] if proven else ["shared_gamesceneui_not_constructor_proven_gamesceneui"],
            "affects_routing": False,
        }
        for call in (3, 4, 7)
    ]


def run(catalog_controls_target: str, strict_proven: bool = True) -> tuple[list[dict], list[dict], list[dict]]:
    with tempfile.TemporaryDirectory() as td:
        d = Path(td)
        inf = d / "inf.jsonl"
        q = d / "q.jsonl"
        a = d / "audit.jsonl"
        strict = d / "strict.jsonl"
        cat = d / "cat.jsonl"
        out = d / "out.jsonl"
        outq = d / "outq.jsonl"
        details = d / "details.jsonl"
        resolved = d / "resolved.jsonl"
        target = "libcocos2dcpp.so:00525d2e"

        rows = [
            inference("libcocos2dcpp.so:002a7594", 3, "receiver_not_single_pointer", "0x80"),
            inference("libcocos2dcpp.so:002a7594", 4, "receiver_not_single_pointer", "0x80"),
            inference("libcocos2dcpp.so:002a7594", 7, "receiver_not_single_pointer", "0x80"),
            inference("libtest.so:00100000", 0, "resolved_unique_vtable", "0x20", "libtest.so:00900000"),
        ]
        write_jsonl(inf, rows)
        write_jsonl(q, [query("libtest.so:00100000", 0, "libtest.so:00900000", "0x20")])
        write_jsonl(a, [audit(target)])
        write_jsonl(strict, strict_audits(target, strict_proven))
        write_jsonl(cat, [
            catalog("GameSceneUI", "libcocos2dcpp.so:00910000", "0x80", target),
            catalog("Controls", "libcocos2dcpp.so:008f4498", "0x80", catalog_controls_target),
            catalog("Existing", "libtest.so:00900000", "0x20", "libtest.so:00111111"),
        ])

        subprocess.run([
            sys.executable, str(FINALIZER), str(inf), str(q), str(a),
            "--strict-mixed-audit-jsonl", str(strict),
            "--output", str(out), "--queries-output", str(outq), "--details-output", str(details),
        ], check=True, capture_output=True, text=True)
        subprocess.run([
            sys.executable, str(RESOLVER), str(cat), str(outq), "--output", str(resolved),
        ], check=True, capture_output=True, text=True)
        return read_jsonl(out), read_jsonl(outq), read_jsonl(resolved)


def main() -> int:
    target = "libcocos2dcpp.so:00525d2e"
    out, queries, resolved = run(target)
    recovered = [r for r in out if r.get("source_id") == "libcocos2dcpp.so:002a7594"]
    assert len(recovered) == 3
    # Even the stale old strict audit that *claims* success must not emit
    # receiver queries: it only traced a caller argument, not the CALLIND receiver.
    assert all(r.get("status") == "receiver_not_single_pointer" for r in recovered)
    assert all(r.get("mixed_slot_target_recovery", {}).get("accepted") is False for r in recovered)
    assert all(r["mixed_slot_target_recovery"]["candidate_receiver_vtable_ids"] == [] for r in recovered)
    assert all(r["mixed_slot_target_recovery"]["caller_argument_branch_classes"] == ["GameSceneUI", "Controls"]
               for r in recovered)
    assert all("caller_argument_not_virtual_receiver_provenance" in
               r["mixed_slot_target_recovery"]["reasons"] for r in recovered)
    assert len(queries) == 1
    assert not [r for r in resolved if r.get("resolution_mode") == "audited_multi_vtable_target_convergence"]

    unproven, unproven_queries, unproven_resolved = run(target, strict_proven=False)
    unproven_rows = [r for r in unproven if r.get("source_id") == "libcocos2dcpp.so:002a7594"]
    assert len(unproven_rows) == 3
    assert all(r.get("status") == "receiver_not_single_pointer" for r in unproven_rows)
    assert len(unproven_queries) == 1
    assert not [r for r in unproven_resolved if r.get("resolution_mode") == "audited_multi_vtable_target_convergence"]

    # A divergent catalog cannot revive a quarantined caller-argument recovery.
    divergent, divergent_queries, divergent_resolved = run("libcocos2dcpp.so:00525d30")
    assert all(r.get("status") == "receiver_not_single_pointer"
               for r in divergent if r.get("source_id") == "libcocos2dcpp.so:002a7594")
    assert len(divergent_queries) == 1
    assert not [r for r in divergent_resolved
                if r.get("resolution_mode") == "audited_multi_vtable_target_convergence"]
    print("v11 mixed-slot target recovery smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
