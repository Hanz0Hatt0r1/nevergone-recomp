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
        source = tmp / "generic-inference.jsonl"
        output = tmp / "legacy-inference.jsonl"
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
            "receiver_type_format": "REVENGINSTR.GhidraIndirectReceiverType/1",
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
        assert adapted["receiver_type_format"] == "NEVERGONE.GhidraIndirectReceiverType/2"

        generic_types = tmp / "generic-receiver-types.tsv"
        legacy_types = tmp / "legacy-receiver-types.tsv"
        generic_types.write_bytes(
            b'format\tsource_id\tclass_name\tclass_evidence\n'
            b'REVENGINSTR.GhidraIndirectReceiverType/1\tlib.so:00100000\tSyntheticThing\t{"name":"Thing","quote":"\\"x\\""}\n'
        )
        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/ml/adapt_revenginstr_receiver_types.py"),
                str(generic_types),
                "--output",
                str(legacy_types),
            ],
            check=True,
        )
        adapted_types = legacy_types.read_bytes()
        assert adapted_types == generic_types.read_bytes().replace(
            b"REVENGINSTR.GhidraIndirectReceiverType/1",
            b"NEVERGONE.GhidraIndirectReceiverType/2",
            1,
        )

        generic_evidence = tmp / "generic-evidence.jsonl"
        legacy_evidence = tmp / "legacy-evidence.jsonl"
        features = {
            "has_callgraph": 1.0,
            "receiver.resolved_count": 1.0,
            "receiver.evidence_direct_receiver_type_count": 1.0,
        }
        generic_evidence.write_text(
            json.dumps({
                "format": "REVENGINSTR.Evidence/1",
                "id": "f1",
                "features": features,
            }) + "\n",
            encoding="utf-8",
        )
        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/ml/adapt_revenginstr_evidence.py"),
                str(generic_evidence),
                "--output",
                str(legacy_evidence),
            ],
            check=True,
        )
        adapted_evidence = json.loads(legacy_evidence.read_text(encoding="utf-8"))
        assert adapted_evidence["format"] == "NEVERGONE.ProductionEvidence/1"
        assert adapted_evidence["id"] == "f1"
        assert adapted_evidence["features"] == features
    print("revenginstr receiver/evidence compatibility adapters smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
