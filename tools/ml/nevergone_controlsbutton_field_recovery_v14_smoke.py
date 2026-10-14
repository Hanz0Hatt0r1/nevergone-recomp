#!/usr/bin/env python3
"""Synthetic regression gates for the three-case ControlsButton v14 finalizer."""
from __future__ import annotations

import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FINAL = ROOT / "tools/ml/finalize_nevergone_controlsbutton_field_recovery_v14.py"
SOURCE = "libcocos2dcpp.so:002a7594"
CALLS = {3:("002a7612","0x134"),4:("002a7634","0x130"),7:("002a7686","0x12c")}
VTABLE = "libcocos2dcpp.so:008f46f8"
TARGET = "libcocos2dcpp.so:00525d2e"


def write(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row, sort_keys=True) + "\n" for row in rows), encoding="utf-8")


def fixtures():
    inference=[]
    for call,(address,_field) in CALLS.items():
        inference.append({
            "format":"NEVERGONE.StaticReceiverVtableInference/1",
            "source_id":SOURCE,"call_index":call,"call_address":address,
            "dispatch_slot_offset":"0x80","status":"receiver_not_single_pointer",
            "receiver_type_name":"-","receiver_type_display":"-",
            "mixed_slot_target_recovery":{
                "accepted":False,
                "reasons":["caller_argument_not_virtual_receiver_provenance"],
            },
        })
    stability=[{
        "format":"NEVERGONE.ControlsButtonPackagedFieldStability/1",
        "record_type":"summary","scope":"exact_packaged_apk_normal_code_execution",
        "target_fields":["0x12c","0x130","0x134"],"target_calls":[3,4,7],
        "packaged_controlsbutton_field_stability_proven":True,
        "packaged_receiver_class_proven":True,
        "caller_argument_quarantine_preserved":True,
        "whole_program_alias_writes_excluded":False,
        "external_injected_code_excluded":False,"affects_routing":False,
    }]
    for call,(address,field) in CALLS.items():
        stability.append({
            "format":"NEVERGONE.ControlsButtonPackagedFieldStability/1",
            "record_type":"receiver_field","source_id":SOURCE,"call_index":call,
            "call_address":address,"dispatch_slot_offset":"0x80",
            "receiver_field_offset":field,"receiver_class_name":"ControlsButton",
            "receiver_vtable_id":VTABLE,"slot_target_id":TARGET,
            "actual_nested_receiver_provenance":True,
            "packaged_field_stability_proven":True,
            "packaged_receiver_class_proven":True,
            "caller_argument_provenance_used":False,"affects_routing":False,
        })
    catalog=[{
        "format":"NEVERGONE.StaticVtableSlotCatalog/3",
        "class_name":"ControlsButton","receiver_vtable_id":VTABLE,
        "dispatch_slot_offset":"0x80","static_status":"single_function_memory_pointer",
        "candidate_target_ids":[TARGET],"runtime_validation_status":"runtime_unobserved",
    }]
    return inference, [], stability, catalog


def run_case(inference, queries, stability, catalog, expect_ok: bool):
    with tempfile.TemporaryDirectory() as td:
        d=Path(td)
        inf=d/"i.jsonl"; q=d/"q.jsonl"; st=d/"s.jsonl"; cat=d/"c.jsonl"
        out=d/"out.jsonl"; outq=d/"outq.jsonl"; det=d/"det.jsonl"
        for path,rows in ((inf,inference),(q,queries),(st,stability),(cat,catalog)):
            write(path,rows)
        p=subprocess.run([sys.executable,str(FINAL),str(inf),str(q),str(st),str(cat),
                          "--output",str(out),"--queries-output",str(outq),
                          "--details-output",str(det)],capture_output=True,text=True)
        if expect_ok:
            assert p.returncode==0,(p.stdout,p.stderr)
            rows=[json.loads(x) for x in out.read_text().splitlines() if x.strip()]
            qs=[json.loads(x) for x in outq.read_text().splitlines() if x.strip()]
            ds=[json.loads(x) for x in det.read_text().splitlines() if x.strip()]
            assert len(rows)==3 and len(qs)==3 and len(ds)==3
            assert all(r["status"]=="resolved_unique_vtable" and
                       r["class_name"]=="ControlsButton" and
                       r["receiver_vtable_id"]==VTABLE for r in rows)
            assert all(d["accepted"] is True and
                       d["caller_argument_provenance_used"] is False for d in ds)
        else:
            assert p.returncode!=0,(p.stdout,p.stderr)


def main():
    import audit_nevergone_controlsbutton_packaged_field_stability as stability_module
    assert stability_module.TARGET_CALLS == {
        3:("002a7612","0x134","002a81c2"),
        4:("002a7634","0x130","002a8172"),
        7:("002a7686","0x12c","002a8120"),
    }
    assert set(stability_module.EXTERNAL_BASE_FUNCTIONS)=={
        "_ZN7cocos2d6CCNode22unscheduleAllSelectorsEv",
        "_ZN7cocos2d7CCLayerD2Ev","_ZN7cocos2d7CCLayerC2Ev",
    }
    assert not ({0x124,0x12c,0x130,0x134,0x174} &
                {x for data in stability_module.EXTERNAL_BASE_FUNCTIONS.values() for x in data[3]})

    inference,queries,stability,catalog=fixtures()
    run_case(inference,queries,stability,catalog,True)

    bad=copy.deepcopy(stability); bad[0]["packaged_controlsbutton_field_stability_proven"]=False
    run_case(inference,queries,bad,catalog,False)
    bad=copy.deepcopy(inference); bad[0]["mixed_slot_target_recovery"]["reasons"]=["stale_caller_type"]
    run_case(bad,queries,stability,catalog,False)
    bad=copy.deepcopy(stability); bad[1]["caller_argument_provenance_used"]=True
    run_case(inference,queries,bad,catalog,False)
    bad=copy.deepcopy(catalog); bad[0]["candidate_target_ids"]=["libcocos2dcpp.so:deadbeef"]
    run_case(inference,queries,stability,bad,False)
    bad=copy.deepcopy(inference); bad[2]["call_address"]="002a7688"
    run_case(bad,queries,stability,catalog,False)
    print("ControlsButton packaged field recovery v14 smoke: ok")


if __name__=="__main__":
    main()
