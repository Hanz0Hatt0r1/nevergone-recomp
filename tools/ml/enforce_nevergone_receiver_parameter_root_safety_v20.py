#!/usr/bin/env python3
"""Demote caller-derived receiver classes whose receiver is not the typed parameter.

This is a post-v19 fail-closed correction. Caller argument types may identify a
function parameter's dynamic class, but they cannot be transferred through a
LOAD of a field owned by that parameter. The correction consumes independent
Ghidra target-receiver provenance and removes queries for rows that fail the
parameter-root identity gate. It never creates a new class or vtable.
"""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
TARGET_FORMAT = "NEVERGONE.GhidraTargetReceiverProvenance/1"
RECOVERY_FORMAT = "NEVERGONE.ReceiverParameterRootSafetyV20/1"
CALLER_SOURCES = {"parameter_caller_type", "structural_parameter_caller_type"}
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match"}
KNOWN_FIELD_LEAKS = {("libcocos2dcpp.so:002a47c6", i) for i in range(6)}


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    out=[]
    with path.open(encoding="utf-8") as f:
        for n,line in enumerate(f,1):
            if not line.strip(): continue
            row=json.loads(line)
            if not isinstance(row,dict): raise ValueError(f"{path}:{n}: expected object")
            out.append(row)
    return out


def read_tsv(path: Path) -> list[dict[str,str]]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        for row in rows: f.write(json.dumps(row, sort_keys=True)+"\n")


def key(row: dict[str, Any], where: str) -> tuple[str,int]:
    s=row.get("source_id"); i=row.get("call_index")
    if not isinstance(s,str) or not s or not isinstance(i,int) or i<0:
        raise ValueError(f"{where}: invalid source/call")
    return s,i


def norm_slot(v: Any) -> str|None:
    if v in (None,"","-"): return None
    try: return f"0x{int(str(v),0):x}"
    except ValueError: return None


def dec(v: Any, default: int=-1) -> int:
    try: return int(str(v),0)
    except Exception: return default


def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("v19_inference_jsonl",type=Path)
    p.add_argument("v19_queries_jsonl",type=Path)
    p.add_argument("target_receiver_tsv",type=Path)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--queries-output",type=Path,required=True)
    p.add_argument("--details-output",type=Path)
    p.add_argument("--require-known-field-leaks",action="store_true")
    a=p.parse_args()

    inference=read_jsonl(a.v19_inference_jsonl)
    queries=read_jsonl(a.v19_queries_jsonl)
    targets=read_tsv(a.target_receiver_tsv)

    target_by: dict[tuple[str,int],dict[str,str]]={}
    for n,row in enumerate(targets,2):
        if row.get("format")!=TARGET_FORMAT: raise ValueError(f"{a.target_receiver_tsv}:{n}: expected {TARGET_FORMAT}")
        s=row.get("source_id")
        try: i=int(row.get("call_index","-1"))
        except ValueError: continue
        if not s or i<0: continue
        k=(s,i)
        if k in target_by: raise ValueError(f"duplicate target provenance: {k}")
        target_by[k]=row

    query_by: dict[tuple[str,int],dict[str,Any]]={}
    for n,row in enumerate(queries,1):
        if row.get("format")!=QUERY_FORMAT: raise ValueError(f"{a.v19_queries_jsonl}:{n}: expected {QUERY_FORMAT}")
        k=key(row,f"query:{n}")
        if k in query_by: raise ValueError(f"duplicate query: {k}")
        query_by[k]=row

    output=[]; details=[]; keep_queries=set(); counts=Counter(); blocked=set()
    for n,original in enumerate(inference,1):
        if original.get("format")!=INFERENCE_FORMAT: raise ValueError(f"{a.v19_inference_jsonl}:{n}: expected {INFERENCE_FORMAT}")
        row=dict(original); k=key(row,f"inference:{n}")
        caller_resolved=row.get("status") in RESOLVED and row.get("class_evidence_source") in CALLER_SOURCES
        if not caller_resolved:
            if row.get("status") in RESOLVED: keep_queries.add(k)
            output.append(row); continue

        counts["caller_resolved"]+=1
        t=target_by.get(k); reasons=[]
        canonical=bool(t and t.get("structural_status")=="canonical_receiver")
        param=dec(t.get("parameter_index"),-1) if t else -1
        call_match=bool(t and t.get("call_address")==row.get("call_address"))
        slot_match=bool(t and norm_slot(t.get("target_slot_offset"))==norm_slot(row.get("dispatch_slot_offset")))
        direct_ok=bool(not t or t.get("direct_receiver_present")!="1" or t.get("direct_receiver_matches_structural")=="1")
        if t is None: reasons.append("missing_target_receiver_provenance")
        else:
            if not canonical: reasons.append("target_receiver_not_canonical")
            if not call_match: reasons.append("target_receiver_call_address_mismatch")
            if not slot_match: reasons.append("target_receiver_slot_mismatch")
            if param<0: reasons.append("receiver_not_parameter_root")
            if not direct_ok: reasons.append("direct_receiver_structural_mismatch")

        detail={
            "format":RECOVERY_FORMAT,"source_id":k[0],"call_index":k[1],"call_address":row.get("call_address"),
            "previous_status":row.get("status"),"previous_class_name":row.get("class_name"),
            "previous_receiver_vtable_id":row.get("receiver_vtable_id"),"class_evidence_source":row.get("class_evidence_source"),
            "target_structural_status":t.get("structural_status") if t else None,
            "target_parameter_index":param,"target_parameter_type_display":t.get("parameter_type_display") if t else None,
            "target_slot_offset":norm_slot(t.get("target_slot_offset")) if t else None,
            "receiver_roots_at_parameter":canonical and param>=0,"accepted":not reasons,"reasons":reasons,"affects_routing":False,
        }
        details.append(detail)
        if reasons:
            counts["demoted"]+=1; blocked.add(k)
            row["receiver_parameter_root_safety_v20"]=detail
            row["blocked_status_v20"]=row.get("status")
            row["blocked_class_name_v20"]=row.get("class_name")
            row["blocked_receiver_vtable_id_v20"]=row.get("receiver_vtable_id")
            row["class_name"]=None
            row["class_match_kind"]=None
            row["class_evidence_source"]=None
            row["candidate_receiver_class_names"]=[]
            row["candidate_receiver_vtable_ids"]=[]
            row["receiver_vtable_id"]=None
            row["status"]="unresolved_receiver_class"
        else:
            counts["retained"]+=1; keep_queries.add(k)
            row["receiver_parameter_root_safety_v20"]=detail
        output.append(row)

    if a.require_known_field_leaks:
        missing=sorted(KNOWN_FIELD_LEAKS-blocked)
        if missing: raise SystemExit(f"known BeatenEffect field leaks were not demoted: {missing}")

    output_queries=[query_by[k] for k in sorted(query_by) if k in keep_queries]
    resolved={key(r,"output") for r in output if r.get("status") in RESOLVED}
    if set(keep_queries)!=resolved:
        raise ValueError(f"resolved/query keep mismatch: resolved={len(resolved)} queries={len(keep_queries)}")
    if {key(r,"output query") for r in output_queries}!=resolved:
        raise ValueError("output query identity mismatch")

    write_jsonl(a.output,output); write_jsonl(a.queries_output,output_queries)
    if a.details_output: write_jsonl(a.details_output,details)

    print("format:",RECOVERY_FORMAT)
    print("inference rows:",len(inference))
    print("caller-derived resolved rows:",counts["caller_resolved"])
    print("retained parameter-root rows:",counts["retained"])
    print("demoted non-parameter-root rows:",counts["demoted"])
    print("known BeatenEffect field leaks demoted:",len(KNOWN_FIELD_LEAKS & blocked),"/",len(KNOWN_FIELD_LEAKS))
    print("input queries:",len(queries))
    print("output queries:",len(output_queries))
    print("affects routing: no")
    print("output:",a.output)
    print("queries output:",a.queries_output)
    if a.details_output: print("details output:",a.details_output)
    return 0

if __name__=="__main__": raise SystemExit(main())
