#!/usr/bin/env python3
"""Resolve eleven remaining ControlsButton slot-0x80 field receivers after v14.

v14 proves the exact packaged-program stability of Controls+0x12c/+0x130/+0x134.
This finalizer reuses that *field-global* proof only after independently requiring
an exact Ghidra receiver-field/factory candidate for each selected CALLIND.
CCJoyStick and CCSprite candidates are deliberately left untouched.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
STABILITY_FORMAT = "NEVERGONE.ControlsButtonPackagedFieldStability/1"
CANDIDATE_FORMAT = "NEVERGONE.Slot80FactoryFieldCandidate/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
DETAIL_FORMAT = "NEVERGONE.ControlsButtonSlot80FieldRecovery/1"
FINAL_FORMAT = "NEVERGONE.ControlsButtonSlot80FieldRecoveryFinalization/1"
CLASS = "ControlsButton"
VTABLE = "libcocos2dcpp.so:008f46f8"
TARGET = "libcocos2dcpp.so:00525d2e"
SLOT = "0x80"
FIELD_STORE = {"0x12c": "0x298120", "0x130": "0x298172", "0x134": "0x2981c2"}
CALLS = {
    ("libcocos2dcpp.so:002a74c0", 0): ("002a74fc", "0x12c"),
    ("libcocos2dcpp.so:002a74c0", 1): ("002a750a", "0x130"),
    ("libcocos2dcpp.so:002a74c0", 2): ("002a7518", "0x134"),
    ("libcocos2dcpp.so:002a7594", 0): ("002a75b4", "0x12c"),
    ("libcocos2dcpp.so:002a7594", 1): ("002a75c2", "0x130"),
    ("libcocos2dcpp.so:002a7594", 2): ("002a75d0", "0x134"),
    ("libcocos2dcpp.so:002a7594", 5): ("002a764a", "0x12c"),
    ("libcocos2dcpp.so:002a7594", 6): ("002a7658", "0x134"),
    ("libcocos2dcpp.so:002a78cc", 0): ("002a78e6", "0x12c"),
    ("libcocos2dcpp.so:002a78cc", 1): ("002a78f4", "0x130"),
    ("libcocos2dcpp.so:002a78cc", 2): ("002a7902", "0x134"),
}
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match", "resolved_converged_target"}


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows=[]
    with path.open(encoding="utf-8") as handle:
        for line_number,line in enumerate(handle,1):
            if not line.strip(): continue
            row=json.loads(line)
            if not isinstance(row,dict): raise ValueError(f"{path}:{line_number}: expected JSON object")
            rows.append(row)
    return rows


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text("".join(json.dumps(row,sort_keys=True)+"\n" for row in rows),encoding="utf-8")


def key(row: dict[str,Any], where: str) -> tuple[str,int]:
    source=row.get("source_id"); call=row.get("call_index")
    if not isinstance(source,str) or not isinstance(call,int):
        raise ValueError(f"{where}: invalid source/call identity")
    return source,call


def norm_slot(value: Any) -> str|None:
    try: return f"0x{int(str(value),0):x}"
    except Exception: return None


def verify_stability(rows: list[dict[str,Any]]) -> dict[str,Any]:
    summaries=[r for r in rows if r.get("format")==STABILITY_FORMAT and r.get("record_type")=="summary"]
    if len(summaries)!=1: raise ValueError("expected one gated v14 stability summary")
    s=summaries[0]
    if not (s.get("scope")=="exact_packaged_apk_normal_code_execution" and
            s.get("target_fields")==["0x12c","0x130","0x134"] and
            s.get("packaged_controlsbutton_field_stability_proven") is True and
            s.get("packaged_receiver_class_proven") is True and
            s.get("caller_argument_quarantine_preserved") is True and
            s.get("external_full_this_static_frontier_closed") is True and
            s.get("external_full_this_sites_closed")==14 and
            s.get("known_external_frontier_writes_receiver_fields") is False and
            s.get("whole_program_alias_writes_excluded") is False and
            s.get("external_injected_code_excluded") is False and
            s.get("affects_routing") is False):
        raise ValueError("gated v14 packaged field stability is not acceptable")
    details=[r for r in rows if r.get("format")==STABILITY_FORMAT and r.get("record_type")=="receiver_field"]
    if len(details)!=3 or {r.get("receiver_field_offset") for r in details}!={"0x12c","0x130","0x134"}:
        raise ValueError("gated v14 stability receiver details changed")
    if any(r.get("packaged_field_stability_proven") is not True or
           r.get("packaged_receiver_class_proven") is not True or
           r.get("external_full_this_static_frontier_closed") is not True or
           r.get("caller_argument_provenance_used") is not False or
           r.get("affects_routing") is not False for r in details):
        raise ValueError("gated v14 stability receiver contract changed")
    return s


def verify_candidates(rows: list[dict[str,Any]]) -> dict[tuple[str,int],dict[str,Any]]:
    selected={}
    for number,row in enumerate(rows,1):
        if row.get("format")!=CANDIDATE_FORMAT:
            raise ValueError(f"candidate:{number}: expected {CANDIDATE_FORMAT}")
        k=key(row,f"candidate:{number}")
        if k not in CALLS: continue
        if k in selected: raise ValueError(f"duplicate selected candidate: {k}")
        address,field=CALLS[k]
        if not (row.get("call_address")==address and norm_slot(row.get("dispatch_slot_offset"))==SLOT and
                row.get("owner_class")=="Controls" and row.get("owner_field_offset")==field and
                row.get("factory_class_candidate")==CLASS and row.get("candidate_receiver_vtable_id")==VTABLE and
                row.get("candidate_target_id")==TARGET and row.get("status")=="construction_linked_target_candidate" and
                row.get("field_writes_exhaustive") is False and row.get("receiver_class_proven") is False and
                row.get("affects_routing") is False):
            raise ValueError(f"selected factory-field candidate changed: {k}")
        witnesses=row.get("factory_witnesses")
        if not isinstance(witnesses,list) or not witnesses or FIELD_STORE[field] not in {w.get("store_elf") for w in witnesses if isinstance(w,dict)}:
            raise ValueError(f"selected candidate lacks exact factory STORE witness: {k}")
        selected[k]=row
    if set(selected)!=set(CALLS):
        raise ValueError(f"expected exactly eleven ControlsButton candidates; missing={sorted(set(CALLS)-set(selected))}")
    return selected


def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("v14_inference_jsonl",type=Path)
    p.add_argument("v14_queries_jsonl",type=Path)
    p.add_argument("gated_packaged_field_stability_jsonl",type=Path)
    p.add_argument("slot80_factory_candidates_jsonl",type=Path)
    p.add_argument("catalog_jsonl",type=Path)
    p.add_argument("--output",required=True,type=Path)
    p.add_argument("--queries-output",required=True,type=Path)
    p.add_argument("--details-output",type=Path)
    a=p.parse_args()

    inference=read_jsonl(a.v14_inference_jsonl); queries=read_jsonl(a.v14_queries_jsonl)
    stability=read_jsonl(a.gated_packaged_field_stability_jsonl)
    candidates=verify_candidates(read_jsonl(a.slot80_factory_candidates_jsonl))
    catalog=read_jsonl(a.catalog_jsonl)
    verify_stability(stability)

    exact=[r for r in catalog if r.get("format")==CATALOG_FORMAT and r.get("class_name")==CLASS and
           r.get("receiver_vtable_id")==VTABLE and norm_slot(r.get("dispatch_slot_offset"))==SLOT]
    if len(exact)!=1: raise ValueError("exact ControlsButton slot catalog row is not unique")
    cat=exact[0]
    if not (cat.get("static_status")=="single_function_memory_pointer" and
            cat.get("candidate_target_ids")==[TARGET] and
            cat.get("runtime_validation_status")!="runtime_mismatch"):
        raise ValueError("ControlsButton slot catalog evidence changed")

    query_by_key={}
    for n,q in enumerate(queries,1):
        if q.get("format")!=QUERY_FORMAT: raise ValueError(f"query:{n}: wrong format")
        k=key(q,f"query:{n}")
        if k in query_by_key: raise ValueError(f"duplicate query: {k}")
        query_by_key[k]=dict(q)
    if set(CALLS)&set(query_by_key): raise ValueError("v14 already contains query for v15 target")

    output=[]; details=[]; seen=set(); accepted=0
    for n,original in enumerate(inference,1):
        if original.get("format")!=INFERENCE_FORMAT: raise ValueError(f"inference:{n}: wrong format")
        row=dict(original); k=key(row,f"inference:{n}")
        if k not in CALLS:
            output.append(row); continue
        seen.add(k); address,field=CALLS[k]; candidate=candidates[k]; reasons=[]
        if row.get("status")!="unresolved_receiver_class": reasons.append("v14_row_not_unresolved_receiver_class")
        if row.get("call_address")!=address: reasons.append("v14_call_address_mismatch")
        if norm_slot(row.get("dispatch_slot_offset"))!=SLOT: reasons.append("v14_slot_mismatch")
        if candidate.get("call_address")!=address or candidate.get("owner_field_offset")!=field:
            reasons.append("candidate_receiver_field_mismatch")
        recovery={
            "format":DETAIL_FORMAT,"source_id":k[0],"call_index":k[1],"call_address":address,
            "dispatch_slot_offset":SLOT,"receiver_field_offset":field,"accepted":False,
            "receiver_class_name":CLASS,"receiver_vtable_id":VTABLE,"candidate_target_ids":[TARGET],
            "actual_receiver_field_slice_proven":True,"packaged_field_stability_proven":True,
            "external_full_this_static_frontier_closed":True,"packaged_scope_only":True,
            "whole_program_alias_writes_excluded":False,"affects_routing":False,"reasons":reasons,
        }
        if not reasons:
            recovery["accepted"]=True; accepted+=1
            row["class_name"]=CLASS
            row["class_match_kind"]="controlsbutton_slot80_field_packaged_stability"
            row["class_evidence_source"]="slot80_receiver_field_factory_plus_packaged_alias_closure"
            row["candidate_receiver_class_names"]=[CLASS]
            row["candidate_receiver_vtable_ids"]=[VTABLE]
            row["receiver_vtable_id"]=VTABLE
            row["status"]="resolved_unique_vtable"
            row["controlsbutton_slot80_field_recovery_v15"]=recovery
            query_by_key[k]={
                "format":QUERY_FORMAT,"source_id":k[0],"call_index":k[1],"call_address":address,
                "receiver_vtable_id":VTABLE,"dispatch_slot_offset":SLOT,"receiver_class_name":CLASS,
                "receiver_class_match_kind":row["class_match_kind"],
                "receiver_class_evidence_source":row["class_evidence_source"],
                "receiver_type_name":row.get("receiver_type_name"),"receiver_type_display":row.get("receiver_type_display"),
                "runtime_validation_status":cat.get("runtime_validation_status","runtime_unobserved"),
                "evidence_ref":f"{k[0]}#call={k[1]}","affects_routing":False,
            }
        else:
            row["controlsbutton_slot80_field_recovery_v15"]=recovery
        details.append(recovery); output.append(row)

    if seen!=set(CALLS): raise ValueError(f"v14 inference missing v15 rows: {sorted(set(CALLS)-seen)}")
    if accepted!=11: raise ValueError(f"expected eleven v15 recoveries, got {accepted}")
    out_queries=[query_by_key[k] for k in sorted(query_by_key)]
    resolved={key(r,"output") for r in output if r.get("status") in RESOLVED}
    if resolved!=set(query_by_key): raise ValueError("v15 resolved/query identity mismatch")

    write_jsonl(a.output,output); write_jsonl(a.queries_output,out_queries)
    if a.details_output: write_jsonl(a.details_output,details)
    print("format:",FINAL_FORMAT)
    print("ControlsButton slot-0x80 field recoveries accepted:",accepted)
    print("input queries:",len(queries),"output queries:",len(out_queries))
    print("CCJoyStick/CCSprite candidates promoted: no")
    print("whole-program alias exclusion: no")
    print("output:",a.output); print("queries output:",a.queries_output)
    if a.details_output: print("details output:",a.details_output)
    return 0


if __name__=="__main__": raise SystemExit(main())
