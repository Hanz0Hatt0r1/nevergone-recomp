#!/usr/bin/env python3
from __future__ import annotations
import json, subprocess, tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
FINAL=ROOT/"tools/ml/finalize_nevergone_ccjoystick_field_recovery_v13.py"
SOURCE="libcocos2dcpp.so:002a647e"


def write(path:Path, rows:list[dict]):
    path.write_text("".join(json.dumps(r)+"\n" for r in rows),encoding="utf-8")


def run(tmp:Path,bad_target:bool=False):
    inf=tmp/"inf.jsonl"; q=tmp/"q.jsonl"; audit=tmp/"audit.jsonl"; cat=tmp/"cat.jsonl"
    out=tmp/"out.jsonl"; outq=tmp/"outq.jsonl"; details=tmp/"details.jsonl"
    write(inf,[{
        "format":"NEVERGONE.StaticReceiverVtableInference/1","source_id":SOURCE,"call_index":4,
        "call_address":"002a64f0","dispatch_slot_offset":"0x58","status":"receiver_not_single_pointer",
        "receiver_type_name":None,"receiver_type_display":"<missing>"
    }])
    write(q,[])
    write(audit,[{
        "format":"NEVERGONE.CCJoyStickFieldFactoryProvenanceAudit/1","source_id":SOURCE,"call_index":4,
        "call_address":"002a64f0","dispatch_slot_offset":"0x58","owner_class":"CCJoyStick","field_offset":0x104,
        "factory_return_class":"cocos2d::CCSprite","factory_return_vtable_id":"libcocos2dcpp.so:009363d8",
        "slot_target_ids":["libcocos2dcpp.so:00525d18"],"runtime_validation_status":"runtime_unobserved",
        "status":"ccjoystick_field_factory_receiver_unique_class","reasons":[],"affects_routing":False
    }])
    target="libcocos2dcpp.so:00525d99" if bad_target else "libcocos2dcpp.so:00525d18"
    write(cat,[{
        "format":"NEVERGONE.StaticVtableSlotCatalog/3","class_name":"cocos2d::CCSprite",
        "receiver_vtable_id":"libcocos2dcpp.so:009363d8","dispatch_slot_offset":"0x58",
        "static_status":"single_function_memory_pointer","candidate_target_ids":[target],
        "target_functions":[{"name":"cocos2d::CCNode::getPosition"}],"runtime_validation_status":"runtime_unobserved"
    }])
    subprocess.check_call(["python3",str(FINAL),str(inf),str(q),str(audit),str(cat),
                           "--output",str(out),"--queries-output",str(outq),"--details-output",str(details)])
    return json.loads(out.read_text().strip()), [json.loads(x) for x in outq.read_text().splitlines() if x.strip()], json.loads(details.read_text().strip())


def main():
    with tempfile.TemporaryDirectory() as raw:
        row,queries,detail=run(Path(raw),False)
        assert row["status"]=="resolved_unique_vtable",row
        assert row["class_name"]=="cocos2d::CCSprite",row
        assert row["receiver_vtable_id"]=="libcocos2dcpp.so:009363d8",row
        assert detail["accepted"] is True,detail
        assert len(queries)==1 and queries[0]["dispatch_slot_offset"]=="0x58",queries
    with tempfile.TemporaryDirectory() as raw:
        row,queries,detail=run(Path(raw),True)
        assert row["status"]=="receiver_not_single_pointer",row
        assert detail["accepted"] is False and "catalog_target_mismatch" in detail["reasons"],detail
        assert queries==[],queries
    print("CCJoyStick field recovery v13 smoke: ok")
    return 0

if __name__=="__main__": raise SystemExit(main())
