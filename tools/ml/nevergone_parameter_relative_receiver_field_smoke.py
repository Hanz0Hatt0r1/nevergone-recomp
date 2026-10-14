#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
AUDIT=ROOT/"tools/ml/audit_nevergone_parameter_relative_receiver_field.py"
SOURCE="libcocos2dcpp.so:002a647e"


def write_jsonl(path:Path, rows:list[dict]):
    path.write_text("".join(json.dumps(r)+"\n" for r in rows),encoding="utf-8")


def write_tsv(path:Path, header:list[str], rows:list[list[object]]):
    with path.open("w",encoding="utf-8",newline="") as f:
        w=csv.writer(f,delimiter="\t",lineterminator="\n"); w.writerow(header); w.writerows(rows)


def fixture(tmp:Path,bad_base:bool=False,bad_name:bool=False):
    failures=tmp/"failures.jsonl"; prov=tmp/"prov.tsv"; slices=tmp/"slices"; slices.mkdir()
    fnclass=tmp/"fnclass.tsv"; symbols=tmp/"symbols.tsv"; catalog=tmp/"catalog.jsonl"; details=tmp/"details.jsonl"
    write_jsonl(failures,[{"format":"NEVERGONE.StaticReceiverTypeFailureAudit/1","source_id":SOURCE,"call_index":4,"call_address":"002a64f0","dispatch_slot_offset":"0x58","status":"receiver_not_single_pointer"}])
    root="register:00000014:4"
    write_tsv(prov,["format","source_id","call_index","status","structural_receiver_is_load","parameter_index","matched_caller_calls","root_token"],[["NEVERGONE.GhidraStructuralReceiverParameterProvenance/1",SOURCE,4,"parameter_no_direct_caller_calls",1,0,0,root]])
    other="register:00000018:4" if bad_base else root
    write_tsv(slices/"libcocos2dcpp.so_002a647e.tsv",["format","row_type","source_id","call_index","node_id","parent_node_id","input_index","defining_opcode","node_token","constant_value"],[
        ["NEVERGONE.GhidraStructuralReceiverSlice/1","node",SOURCE,4,0,-1,-1,"LOAD","def:LOAD@002a64e0:1:4","-"],
        ["NEVERGONE.GhidraStructuralReceiverSlice/1","node",SOURCE,4,1,0,0,"CAST","def:CAST@002a64dc:2:4","-"],
        ["NEVERGONE.GhidraStructuralReceiverSlice/1","node",SOURCE,4,2,1,0,"INT_ADD","def:INT_ADD@002a64d8:3:4","-"],
        ["NEVERGONE.GhidraStructuralReceiverSlice/1","node",SOURCE,4,3,2,0,"-",other,"-"],
        ["NEVERGONE.GhidraStructuralReceiverSlice/1","node",SOURCE,4,4,2,1,"-","const:0x104:4","0x104"],
    ])
    name="FUN_002a647e" if bad_name else "CCJoyStick::ccTouchCancelled"
    write_tsv(fnclass,["format","address","function_at_entry","function_at_name"],[["NEVERGONE.GhidraAddressClassification/1","002a647e","002a647e",name]])
    # Real game.tsv can omit this local/non-exported function; header-only is valid supplemental evidence.
    write_tsv(symbols,["value_hex","code_offset_hex","size","thumb","name","demangled"],[])
    write_jsonl(catalog,[{"format":"NEVERGONE.StaticVtableSlotCatalog/3","class_name":"CCJoyStick","receiver_vtable_id":"libcocos2dcpp.so:008f5000","dispatch_slot_offset":"0x58","static_status":"single_function_memory_pointer","candidate_target_ids":["libcocos2dcpp.so:00500000"],"target_functions":[{"name":"cocos2d::CCNode::example"}],"runtime_validation_status":"runtime_unobserved"}])
    return failures,prov,slices,fnclass,symbols,catalog,details


def run(tmp:Path,bad_base:bool=False,bad_name:bool=False):
    a=fixture(tmp,bad_base,bad_name)
    subprocess.check_call(["python3",str(AUDIT),str(a[0]),str(a[1]),str(a[2]),str(a[3]),str(a[4]),str(a[5]),"--details-output",str(a[6])])
    return json.loads(a[6].read_text().strip())


def main():
    with tempfile.TemporaryDirectory() as raw:
        tmp=Path(raw)/"good"; tmp.mkdir(); row=run(tmp)
        assert row["field_offset"]==0x104,row
        assert row["ghidra_owner_class"]=="CCJoyStick",row
        assert row["native_owner_classes"]==[],row
        assert row["owner_vtable_ids"]==["libcocos2dcpp.so:008f5000"],row
        assert row["status"]=="parameter_relative_field_with_function_owner_evidence",row
    with tempfile.TemporaryDirectory() as raw:
        tmp=Path(raw)/"badbase"; tmp.mkdir(); row=run(tmp,bad_base=True)
        assert row["status"]=="parameter_relative_field_evidence_incomplete",row
    with tempfile.TemporaryDirectory() as raw:
        tmp=Path(raw)/"badname"; tmp.mkdir(); row=run(tmp,bad_name=True)
        assert row["status"]=="parameter_relative_field_evidence_incomplete",row
        assert "ghidra_function_owner_unresolved" in row["reasons"],row
    print("parameter-relative receiver field smoke: ok")
    return 0

if __name__=="__main__": raise SystemExit(main())
