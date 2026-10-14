#!/usr/bin/env python3
"""Resolve the last non-quarantined missing receiver from CCJoyStick field provenance.

This v13 post-finalizer is deliberately single-case. It accepts only
libcocos2dcpp.so:002a647e call 4 (slot 0x58) from the current v12 baseline.
The required evidence is NEVERGONE.CCJoyStickFieldFactoryProvenanceAudit/1,
which independently proves:

  CCJoyStick::setStickTexture receiver == LOAD(this + 0x104)
  CCJoyStick::init STORE this+0x104 <- CCSprite::create return
  CCSprite::create non-null return is constructor-proven cocos2d::CCSprite
  exact cocos2d::CCSprite vtable slot 0x58 has one static target

The finalizer re-checks the class/vtable/slot/target against
StaticVtableSlotCatalog/3 before emitting a normal exact-vtable query.
No field-offset-only, owner-name-only, or neighboring-object inference is used.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
AUDIT_FORMAT = "NEVERGONE.CCJoyStickFieldFactoryProvenanceAudit/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
DETAIL_FORMAT = "NEVERGONE.CCJoyStickFieldRecovery/1"
FINAL_FORMAT = "NEVERGONE.CCJoyStickFieldRecoveryFinalization/1"

SOURCE = "libcocos2dcpp.so:002a647e"
CALL = 4
SLOT = "0x58"
OWNER = "CCJoyStick"
FIELD = 0x104
CLASS = "cocos2d::CCSprite"
VTABLE = "libcocos2dcpp.so:009363d8"

RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match", "resolved_converged_target"}


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows=[]
    with path.open(encoding="utf-8") as f:
        for n,line in enumerate(f,1):
            if not line.strip():
                continue
            row=json.loads(line)
            if not isinstance(row,dict):
                raise ValueError(f"{path}:{n}: expected object")
            rows.append(row)
    return rows


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, sort_keys=True)+"\n")


def key(row: dict[str, Any], where: str) -> tuple[str,int]:
    source=row.get("source_id"); call=row.get("call_index")
    if not isinstance(source,str) or not isinstance(call,int):
        raise ValueError(f"{where}: invalid source/call")
    return source,call


def norm_slot(value: Any) -> str|None:
    try: return f"0x{int(str(value),0):x}"
    except Exception: return None


def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("v12_inference_jsonl", type=Path)
    p.add_argument("v12_queries_jsonl", type=Path)
    p.add_argument("field_audit_jsonl", type=Path)
    p.add_argument("catalog_jsonl", type=Path)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--queries-output", type=Path, required=True)
    p.add_argument("--details-output", type=Path)
    a=p.parse_args()

    inference=read_jsonl(a.v12_inference_jsonl)
    queries=read_jsonl(a.v12_queries_jsonl)
    audits=read_jsonl(a.field_audit_jsonl)
    catalog=read_jsonl(a.catalog_jsonl)

    target_key=(SOURCE,CALL)
    matching_audits=[r for r in audits if r.get("source_id")==SOURCE and r.get("call_index")==CALL]
    if len(matching_audits)!=1:
        raise ValueError(f"expected exactly one CCJoyStick field audit row, got {len(matching_audits)}")
    audit=matching_audits[0]
    if audit.get("format")!=AUDIT_FORMAT:
        raise ValueError(f"expected {AUDIT_FORMAT}")

    query_by_key: dict[tuple[str,int],dict[str,Any]]={}
    for n,q in enumerate(queries,1):
        if q.get("format")!=QUERY_FORMAT:
            raise ValueError(f"{a.v12_queries_jsonl}:{n}: expected {QUERY_FORMAT}")
        k=key(q,f"query:{n}")
        if k in query_by_key:
            raise ValueError(f"duplicate query {k}")
        query_by_key[k]=dict(q)

    catalog_rows=[]
    for n,row in enumerate(catalog,1):
        if row.get("format")!=CATALOG_FORMAT:
            raise ValueError(f"{a.catalog_jsonl}:{n}: expected {CATALOG_FORMAT}")
        if row.get("class_name")==CLASS and row.get("receiver_vtable_id")==VTABLE and norm_slot(row.get("dispatch_slot_offset"))==SLOT:
            catalog_rows.append(row)

    output=[]; details=[]; seen=False; accepted=0; blocked=0
    for n,original in enumerate(inference,1):
        if original.get("format")!=INFERENCE_FORMAT:
            raise ValueError(f"{a.v12_inference_jsonl}:{n}: expected {INFERENCE_FORMAT}")
        row=dict(original); k=key(row,f"inference:{n}")
        if k!=target_key:
            output.append(row); continue
        seen=True
        reasons=[]
        if row.get("status")!="receiver_not_single_pointer": reasons.append("v12_row_not_receiver_not_single_pointer")
        if norm_slot(row.get("dispatch_slot_offset"))!=SLOT: reasons.append("v12_slot_mismatch")
        if k in query_by_key: reasons.append("query_already_exists")

        if audit.get("status")!="ccjoystick_field_factory_receiver_unique_class": reasons.append("field_audit_status_not_accepted")
        if audit.get("reasons")!=[]: reasons.append("field_audit_has_reasons")
        if audit.get("affects_routing") is not False: reasons.append("field_audit_routing_contract_invalid")
        if audit.get("owner_class")!=OWNER: reasons.append("owner_class_mismatch")
        if audit.get("field_offset")!=FIELD: reasons.append("field_offset_mismatch")
        if norm_slot(audit.get("dispatch_slot_offset"))!=SLOT: reasons.append("field_audit_slot_mismatch")
        if audit.get("factory_return_class")!=CLASS: reasons.append("factory_return_class_mismatch")
        if audit.get("factory_return_vtable_id")!=VTABLE: reasons.append("factory_return_vtable_mismatch")
        targets=audit.get("slot_target_ids")
        if not isinstance(targets,list) or len(targets)!=1 or not isinstance(targets[0],str): reasons.append("field_audit_target_not_unique")
        target=targets[0] if isinstance(targets,list) and len(targets)==1 and isinstance(targets[0],str) else None
        if audit.get("runtime_validation_status")=="runtime_mismatch": reasons.append("field_audit_runtime_mismatch")

        catalog_row=catalog_rows[0] if len(catalog_rows)==1 else None
        if len(catalog_rows)!=1: reasons.append("catalog_exact_class_vtable_slot_not_unique")
        if catalog_row:
            if catalog_row.get("static_status")!="single_function_memory_pointer": reasons.append("catalog_slot_not_single_function")
            ctargets=catalog_row.get("candidate_target_ids")
            if target is None or ctargets!=[target]: reasons.append("catalog_target_mismatch")
            if catalog_row.get("runtime_validation_status")=="runtime_mismatch": reasons.append("catalog_runtime_mismatch")

        recovery={
            "format":DETAIL_FORMAT,
            "source_id":SOURCE,"call_index":CALL,"dispatch_slot_offset":SLOT,
            "accepted":False,"owner_class":OWNER,"field_offset":FIELD,
            "receiver_class_name":CLASS,"receiver_vtable_id":VTABLE,
            "candidate_target_ids":[target] if target else [],
            "reasons":reasons,"affects_routing":False,
        }
        if not reasons and target and catalog_row:
            recovery["accepted"]=True; accepted+=1
            row["class_name"]=CLASS
            row["class_match_kind"]="ccjoystick_exact_field_factory_constructor_catalog_class"
            row["class_evidence_source"]="ccjoystick_exact_field_factory_constructor_provenance"
            row["candidate_receiver_class_names"]=[CLASS]
            row["candidate_receiver_vtable_ids"]=[VTABLE]
            row["receiver_vtable_id"]=VTABLE
            row["status"]="resolved_unique_vtable"
            row["ccjoystick_field_recovery_v13"]=recovery
            query_by_key[k]={
                "format":QUERY_FORMAT,"source_id":SOURCE,"call_index":CALL,
                "call_address":row.get("call_address"),"receiver_vtable_id":VTABLE,
                "dispatch_slot_offset":SLOT,"receiver_class_name":CLASS,
                "receiver_class_match_kind":row["class_match_kind"],
                "receiver_class_evidence_source":row["class_evidence_source"],
                "receiver_type_name":row.get("receiver_type_name"),
                "receiver_type_display":row.get("receiver_type_display"),
                "runtime_validation_status":catalog_row.get("runtime_validation_status","runtime_unobserved"),
                "evidence_ref":f"{SOURCE}#call={CALL}","affects_routing":False,
            }
        else:
            blocked+=1; row["ccjoystick_field_recovery_v13"]=recovery
        details.append(recovery); output.append(row)

    if not seen: raise ValueError("v12 target row missing")
    out_queries=[query_by_key[k] for k in sorted(query_by_key)]
    resolved_keys={key(r,"output") for r in output if r.get("status") in RESOLVED}
    if resolved_keys!=set(query_by_key):
        raise ValueError(f"v13 resolved/query mismatch: missing={sorted(resolved_keys-set(query_by_key))} extra={sorted(set(query_by_key)-resolved_keys)}")

    write_jsonl(a.output,output); write_jsonl(a.queries_output,out_queries)
    if a.details_output: write_jsonl(a.details_output,details)
    print("format:",FINAL_FORMAT)
    print("inference rows:",len(inference))
    print("input queries:",len(queries))
    print("eligible CCJoyStick field rows: 1")
    print("CCJoyStick field recoveries accepted:",accepted)
    print("CCJoyStick field recoveries blocked:",blocked)
    print("output queries:",len(out_queries))
    print("affects routing: no")
    print("output:",a.output)
    print("queries output:",a.queries_output)
    if a.details_output: print("details output:",a.details_output)
    return 0

if __name__=="__main__": raise SystemExit(main())
