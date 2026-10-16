#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        source = tmp / "generic.jsonl"
        output = tmp / "legacy.jsonl"
        row = {
            "format": "REVENGINSTR.ReceiverInference/1",
            "source_id": "libcocos2dcpp.so:00100000",
            "source_entry": "00100000",
            "call_index": 0,
            "dispatch_slot_offset": None,
            "slot_evidence_present": False,
            "resolution_scope": "receiver_identity_only_no_dispatch_slot",
            "receiver_type_evidence_present": True,
            "class_name": "SyntheticThing",
            "class_match_kind": "exact_catalog_class_name",
            "class_evidence_source": "direct_receiver_type",
            "candidate_receiver_class_names": ["SyntheticThing"],
            "candidate_receiver_vtable_ids": ["lib.so:00900000"],
            "receiver_vtable_id": "lib.so:00900000",
            "status": "resolved_unique_vtable",
            "affects_routing": False,
        }
        source.write_text(json.dumps(row) + "\n", encoding="utf-8")
        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/ml/adapt_revenginstr_receiver_inference.py"),
                str(source),
                "--output",
                str(output),
            ],
            check=True,
        )
        adapted = json.loads(output.read_text(encoding="utf-8"))
        assert adapted["format"] == "NEVERGONE.StaticReceiverVtableInference/1"
        assert adapted["mass_inference_format"] == "NEVERGONE.MassReceiverVtableInference/1"
        assert adapted["status"] == "resolved_unique_vtable"
        assert adapted["class_name"] == "SyntheticThing"
        assert adapted["runtime_validation_status"] == "runtime_unobserved"
        assert adapted["parameter_provenance_present"] is False
        assert adapted["structural_parameter_provenance_present"] is False
        assert adapted["affects_routing"] is False
    print("revenginstr receiver compatibility adapter smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
