#!/usr/bin/env python3
"""Synthetic fail-closed regression for eleven ControlsButton slot-0x80 v15 recoveries."""
from __future__ import annotations

import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
FINAL=ROOT/"tools/ml/finalize_nevergone_controlsbutton_slot80_field_recovery_v15.py"
CLASS="ControlsButton"; VTABLE="libcocos2dcpp.so:008f46f8"; TARGET="libcocos2dcpp.so:00525d2e"
CALLS={
 ("libcocos2dcpp.so:002a74c0",0):("002a74fc","0x12c"),
 ("libcocos2dcpp.so:002a74c0",1):("002a750a","0x130"),
 ("libcocos2dcpp.so:002a74c0",2):("002a7518","0x134"),
 ("libcocos2dcpp.so:002a7594",0):("002a75b4","0x12c"),
 ("libcocos2dcpp.so:002a7594",1):("002a75c2","0x130"),
 ("libcocos2dcpp.so:002a7594",2):("002a75d0","0x134"),
 ("libcocos2dcpp.so:002a7594",5):("002a764a","0x12c"),
 ("libcocos2dcpp.so:002a7594",6):("002a7658","0x134"),
 ("libcocos2dcpp.so:002a78cc",0):("002a78e6","0x12c"),
 ("libcocos2dcpp.so:002a78cc",1):("002a78f4","0x130"),
 ("libcocos2dcpp.so:002a78cc",2):("002a7902","0x134"),
}
STORES={"0x12c":"0x298120","0x130":"0x298172","0x134":"0x2981c2"}


def write(path,rows): path.write_text("".join(json.dumps(x,sort_keys=True)+"\n" for x in rows),encoding="utf-8")


def fixture():
 inf=[]; cand=[]
 for (source,call),(address,field) in CALLS.items():
  inf.append({"format":"NEVERGONE.StaticReceiverVtableInference/1","source_id":source,"call_index":call,
              "call_address":address,"dispatch_slot_offset":"0x80","status":"unresolved_receiver_class",
              "receiver_type_name":"int *","receiver_type_display":"int *"})
  cand.append({"format":"NEVERGONE.Slot80FactoryFieldCandidate/1","source_id":source,"call_index":call,
               "call_address":address,"dispatch_slot_offset":"0x80","owner_class":"Controls",
               "owner_field_offset":field,"factory_class_candidate":CLASS,
               "factory_witnesses":[{"store_elf":STORES[field]}],
               "candidate_receiver_vtable_id":VTABLE,"candidate_target_id":TARGET,
               "field_writes_exhaustive":False,"receiver_class_proven":False,
               "status":"construction_linked_target_candidate","affects_routing":False})
 # Add six non-v15 candidates to ensure they are ignored, not promoted.
 for i,(cls,field,target) in enumerate([
     ("CCJoyStick","0x124",TARGET),("CCJoyStick","0x124",TARGET),
     ("cocos2d::CCSprite","0x174","libcocos2dcpp.so:00549f1e"),
     ("cocos2d::CCSprite","0x104","libcocos2dcpp.so:00549f1e"),
     ("cocos2d::CCSprite","0x10c","libcocos2dcpp.so:00549f1e"),
     ("cocos2d::CCSprite","0x108","libcocos2dcpp.so:00549f1e")]):
  cand.append({"format":"NEVERGONE.Slot80FactoryFieldCandidate/1","source_id":"other", "call_index":100+i,
               "call_address":f"dead{i:04x}","dispatch_slot_offset":"0x80","owner_class":"Other",
               "owner_field_offset":field,"factory_class_candidate":cls,"factory_witnesses":[],
               "candidate_receiver_vtable_id":"other","candidate_target_id":target,
               "field_writes_exhaustive":False,"receiver_class_proven":False,
               "status":"construction_linked_target_candidate","affects_routing":False})
 stability=[{"format":"NEVERGONE.ControlsButtonPackagedFieldStability/1","record_type":"summary",
             "scope":"exact_packaged_apk_normal_code_execution","target_fields":["0x12c","0x130","0x134"],
             "packaged_controlsbutton_field_stability_proven":True,"packaged_receiver_class_proven":True,
             "caller_argument_quarantine_preserved":True,"external_full_this_static_frontier_closed":True,
             "external_full_this_sites_closed":14,"known_external_frontier_writes_receiver_fields":False,
             "whole_program_alias_writes_excluded":False,"external_injected_code_excluded":False,"affects_routing":False}]
 for call,field in ((3,"0x134"),(4,"0x130"),(7,"0x12c")):
  stability.append({"format":"NEVERGONE.ControlsButtonPackagedFieldStability/1","record_type":"receiver_field",
                    "source_id":"libcocos2dcpp.so:002a7594","call_index":call,"receiver_field_offset":field,
                    "packaged_field_stability_proven":True,"packaged_receiver_class_proven":True,
                    "external_full_this_static_frontier_closed":True,"caller_argument_provenance_used":False,
                    "affects_routing":False})
 catalog=[{"format":"NEVERGONE.StaticVtableSlotCatalog/3","class_name":CLASS,"receiver_vtable_id":VTABLE,
           "dispatch_slot_offset":"0x80","static_status":"single_function_memory_pointer",
           "candidate_target_ids":[TARGET],"runtime_validation_status":"runtime_unobserved"}]
 return inf,[],stability,cand,catalog


def run(inf,q,st,cand,cat,ok):
 with tempfile.TemporaryDirectory() as td:
  d=Path(td); paths=[d/x for x in ("i","q","s","f","c")]; out=d/"out"; outq=d/"outq"; det=d/"det"
  for p,rows in zip(paths,(inf,q,st,cand,cat)): write(p,rows)
  r=subprocess.run([sys.executable,str(FINAL),*[str(p) for p in paths],"--output",str(out),
                    "--queries-output",str(outq),"--details-output",str(det)],capture_output=True,text=True)
  if ok:
   assert r.returncode==0,(r.stdout,r.stderr)
   rows=[json.loads(x) for x in out.read_text().splitlines() if x.strip()]
   qs=[json.loads(x) for x in outq.read_text().splitlines() if x.strip()]
   ds=[json.loads(x) for x in det.read_text().splitlines() if x.strip()]
   assert len(rows)==11 and len(qs)==11 and len(ds)==11
   assert all(x["status"]=="resolved_unique_vtable" and x["class_name"]==CLASS for x in rows)
   assert all(x["accepted"] is True and x["packaged_field_stability_proven"] is True for x in ds)
  else: assert r.returncode!=0,(r.stdout,r.stderr)


def main():
 inf,q,st,cand,cat=fixture(); run(inf,q,st,cand,cat,True)
 bad=copy.deepcopy(st); bad[0]["external_full_this_static_frontier_closed"]=False; run(inf,q,bad,cand,cat,False)
 bad=copy.deepcopy(cand); bad[0]["owner_field_offset"]="0x124"; run(inf,q,st,bad,cat,False)
 bad=copy.deepcopy(cand); bad[0]["factory_class_candidate"]="CCJoyStick"; run(inf,q,st,bad,cat,False)
 bad=copy.deepcopy(cand); bad[0]["factory_witnesses"]=[{"store_elf":"0xdead"}]; run(inf,q,st,bad,cat,False)
 bad=copy.deepcopy(inf); bad[0]["status"]="resolved_unique_vtable"; run(bad,q,st,cand,cat,False)
 bad=copy.deepcopy(cat); bad[0]["candidate_target_ids"]=["libcocos2dcpp.so:deadbeef"]; run(inf,q,st,cand,bad,False)
 print("ControlsButton slot80 field recovery v15 smoke: ok")

if __name__=="__main__": main()
