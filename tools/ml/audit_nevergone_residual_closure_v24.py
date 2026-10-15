#!/usr/bin/env python3
"""Summarize targeted v24 evidence for the corrected residual receiver frontier.

Evidence-only. This stage does not promote receiver classes. It verifies that the
namespaced protobuf owner is actually recognized after the v23 exporter fix,
checks native nested-message writer patterns, and inventories exact CALLIND slots
for the finite BeatenEffect/CCJoyStick blocker functions.
"""
from __future__ import annotations
import argparse,csv,json
from pathlib import Path
from typing import Any

FLOW_FMT="NEVERGONE.GhidraClassOwnerAliasFlow/1"
WRITE_FMT="NEVERGONE.GhidraClassParameterFieldWrite/1"
SLICE_FMT="NEVERGONE.GhidraStructuralReceiverSlice/1"
OUT_FMT="NEVERGONE.ResidualReceiverClosureV24/1"
PROTO="PVEDATA::equipToolsListMsg"
NESTED="PVEDATA::equipToolsDataMsg"
PROTO_FIELDS={0x8:"00299a66",0xc:"00299a88",0x10:"00299aaa",0x14:"00299acc"}
TARGETS={
    "BeatenEffect":{0x128,0x12c,0x130,0x134,0x138,0x14c,0x150,0x154,0x158},
    "CCJoyStick":{0x10c,0x14c},
    PROTO:set(PROTO_FIELDS),
}

def tsv(p:Path)->list[dict[str,str]]:
    with p.open(encoding="utf-8",newline="") as f:return list(csv.DictReader(f,delimiter="\t"))
def num(v:Any)->int|None:
    if v in (None,"","-"):return None
    try:return int(str(v),0)
    except ValueError:return None

def validate(rows,path,fmt):
    for i,r in enumerate(rows,2):
        if r.get("format")!=fmt:raise ValueError(f"{path}:{i}: expected {fmt}")

def audit_flow(rows,cls):
    summaries=[r for r in rows if r.get("row_type")=="summary"]
    owner=[r for r in summaries if r.get("owner_parameter_proven")=="1"]
    whole=[]; fieldaddr=[]; unknown=[]; unknown_store=[]; fieldvalues=[]
    targets=TARGETS[cls]
    for r in rows:
        if r.get("owner_parameter_proven")!="1":continue
        if r.get("row_type")=="store" and r.get("address_flow_kind")=="owner_derived_unknown":
            unknown_store.append([r.get("function_entry"),r.get("op_address"),r.get("address_token")])
        if r.get("row_type")!="call_arg":continue
        k=r.get("arg_flow_kind");o=num(r.get("arg_owner_offset"));t=r.get("call_target_name") or "-"
        item=[r.get("function_entry"),r.get("op_address"),t,r.get("call_kind"),r.get("arg_index")]
        if k=="owner_address" and o==0 and not t.startswith(cls+"::"):whole.append(item)
        elif k=="owner_address" and o in targets:fieldaddr.append(item+[f"0x{o:x}"])
        elif k=="owner_derived_unknown":unknown.append(item)
        elif k=="owner_field_value":fieldvalues.append(item+[f"0x{o:x}" if o is not None else None])
    reasons=[]
    if whole:reasons.append("whole_owner_call_frontier_open")
    if fieldaddr:reasons.append("target_field_address_escape")
    if unknown:reasons.append("owner_derived_unknown_call_arg_present")
    if unknown_store:reasons.append("owner_derived_unknown_store_present")
    return {"class_name":cls,"method_summaries":len(summaries),"owner_method_summaries":len(owner),
            "whole_owner_calls":whole,"target_field_address_escapes":fieldaddr,
            "owner_derived_unknown_call_args":unknown,"owner_derived_unknown_stores":unknown_store,
            "owner_field_value_calls":fieldvalues,"reasons":reasons,"closed":not reasons}

def pcode_info(path:Path):
    rows=tsv(path)
    direct=[r for r in rows if r.get("opcode")=="CALL"]
    ind=[r for r in rows if r.get("opcode")=="CALLIND"]
    stores=[r for r in rows if r.get("opcode")=="STORE"]
    return {"function_entry":rows[0].get("function_entry") if rows else None,
            "rows":len(rows),"direct_calls":[[r.get("op_address"),r.get("callee_entry"),r.get("callee_name"),r.get("output"),r.get("inputs")] for r in direct],
            "callind_addresses":[r.get("op_address") for r in ind],"store_count":len(stores)}

def slice_info(path:Path):
    rows=tsv(path);validate(rows,path,SLICE_FMT)
    out=[]
    for r in rows:
        if r.get("row_type")!="summary":continue
        out.append({"call_index":int(r.get("call_index","-1")),"call_address":r.get("call_address"),
                    "slot_offset":r.get("slot_offset"),"status":r.get("recovery_status"),
                    "truncated_by_budget":r.get("truncated_by_budget"),"depth_truncation_count":r.get("depth_truncation_count"),
                    "cycle_count":r.get("cycle_count")})
    return out

def proto_writers(rows):
    validate(rows,Path("proto-writes"),WRITE_FMT)
    fields={}
    for off,mutable_entry in PROTO_FIELDS.items():
        rs=[r for r in rows if num(r.get("address_offset"))==off]
        mutable=[r for r in rs if r.get("function_entry")==mutable_entry]
        defaults=[r for r in rs if r.get("function_entry")=="0029c20c"]
        swaps=[r for r in rs if r.get("function_entry")=="00299fae"]
        nulls=[r for r in rs if r.get("value_root_token","").startswith("const:0x0:")]
        fields[f"0x{off:x}"]={
            "mutable_write_count":len(mutable),
            "mutable_operator_new_typed_nested":len(mutable)==1 and mutable[0].get("value_root_opcode")=="CALL" and mutable[0].get("factory_target_name")=="operator.new" and mutable[0].get("value_root_type_display") in {"equipToolsDataMsg *",NESTED+" *"},
            "default_instance_write_count":sum(1 for r in defaults if r.get("factory_target_name")==NESTED+"::default_instance"),
            "swap_write_count":len(swaps),"null_write_count":len(nulls),
        }
    return fields

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--flow",action="append",nargs=2,metavar=("CLASS","TSV"),required=True)
    ap.add_argument("--proto-writes",type=Path,required=True)
    ap.add_argument("--pcode-dir",type=Path,required=True)
    ap.add_argument("--slice-dir",type=Path,required=True)
    ap.add_argument("--details-output",type=Path)
    a=ap.parse_args()
    flow={}
    for cls,p in a.flow:
        path=Path(p);rows=tsv(path);validate(rows,path,FLOW_FMT);flow[cls]=audit_flow(rows,cls)
    for cls in TARGETS:
        if cls not in flow:raise ValueError(f"missing flow for {cls}")
    proto=flow[PROTO]
    namespace_fix_ok=proto["owner_method_summaries"]>0
    writers=proto_writers(tsv(a.proto_writes))
    pcode={p.stem:pcode_info(p) for p in sorted(a.pcode_dir.glob("*.tsv"))}
    slices={p.stem:slice_info(p) for p in sorted(a.slice_dir.glob("*.tsv"))}
    mutable_ctor={}
    for off,entry in PROTO_FIELDS.items():
        key=f"pcode-{entry}"
        info=pcode.get(key,{})
        calls=info.get("direct_calls",[])
        mutable_ctor[f"0x{off:x}"]={
            "operator_new_seen":any(c[2]=="operator.new" for c in calls),
            "nested_constructor_seen":any((c[2] or "").startswith(NESTED+"::equipToolsDataMsg") for c in calls),
        }
    details={"format":OUT_FMT,"namespace_owner_fix_observed":namespace_fix_ok,
             "class_alias_frontiers":flow,"protobuf_field_writers":writers,
             "protobuf_mutable_constructor_calls":mutable_ctor,
             "pcode_functions":pcode,"callind_slices":slices,"affects_routing":False}
    print("format:",OUT_FMT)
    print("protobuf owner methods recognized:",proto["owner_method_summaries"],"/",proto["method_summaries"])
    print("namespace owner fix observed:",namespace_fix_ok)
    for cls in TARGETS:
        r=flow[cls]
        print(cls,"closed=",r["closed"],"whole_owner=",len(r["whole_owner_calls"]),"unknown_args=",len(r["owner_derived_unknown_call_args"]),"unknown_stores=",len(r["owner_derived_unknown_stores"]),"field_addr=",len(r["target_field_address_escapes"]))
    for field in sorted(writers,key=lambda x:int(x,0)):
        print("protobuf",field,"writers=",writers[field],"constructor=",mutable_ctor[field])
    print("pcode functions:",len(pcode),"slice functions:",len(slices))
    if a.details_output:
        a.details_output.parent.mkdir(parents=True,exist_ok=True)
        a.details_output.write_text(json.dumps(details,sort_keys=True)+"\n",encoding="utf-8")
        print("details output:",a.details_output)
    print("affects routing: no")
    return 0
if __name__=="__main__":raise SystemExit(main())
