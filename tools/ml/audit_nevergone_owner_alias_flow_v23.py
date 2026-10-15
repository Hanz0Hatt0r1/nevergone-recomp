#!/usr/bin/env python3
"""Audit exact owner-address vs owner-field-value flows for residual receivers."""
from __future__ import annotations
import argparse,csv,json
from collections import Counter
from pathlib import Path
from typing import Any

FLOW_FORMAT="NEVERGONE.GhidraClassOwnerAliasFlow/1"
OUT_FORMAT="NEVERGONE.OwnerAliasFlowAudit/1"
DEFAULT_TARGETS={
    "BeatenEffect":{0x128,0x12c,0x130,0x134,0x138,0x14c,0x150,0x154,0x158},
    "CCJoyStick":{0x10c,0x14c},
    "PVEDATA::equipToolsListMsg":{0x8,0xc,0x10,0x14},
}

def read_tsv(path:Path):
    with path.open(encoding="utf-8",newline="") as f:return list(csv.DictReader(f,delimiter="\t"))
def num(v:Any):
    if v in (None,"","-"):return None
    try:return int(str(v),0)
    except ValueError:return None

def validate(rows,path):
    for i,r in enumerate(rows,2):
        if r.get("format")!=FLOW_FORMAT: raise ValueError(f"{path}:{i}: expected {FLOW_FORMAT}")

def audit_class(rows:list[dict[str,str]],cls:str,targets:set[int])->dict[str,Any]:
    summaries=[r for r in rows if r.get("row_type")=="summary"]
    methods=[r for r in summaries if r.get("owner_parameter_proven")=="1"]
    static_like=[r for r in summaries if r.get("owner_parameter_proven")!="1"]
    reasons=[]
    if not summaries: reasons.append("missing_method_summaries")
    # A method without a proven owner parameter is not interpreted as an owner-flow method.
    # This deliberately excludes static factory helpers whose parameter 0 is user data.
    unknown_stores=[]; owner_escapes=[]; internal_owner_calls=[]; field_addr_escapes=[]; unknown_args=[]; field_value_calls=[]
    for r in rows:
        if r.get("owner_parameter_proven")!="1": continue
        rt=r.get("row_type")
        if rt=="store" and r.get("address_flow_kind")=="owner_derived_unknown":
            unknown_stores.append((r.get("function_entry"),r.get("op_address"),r.get("address_token")))
        if rt!="call_arg": continue
        kind=r.get("arg_flow_kind"); off=num(r.get("arg_owner_offset")); target=r.get("call_target_name") or "-"
        if kind=="owner_address":
            if off==0:
                item=(r.get("function_entry"),r.get("op_address"),target,r.get("call_kind"),r.get("arg_index"))
                if target.startswith(cls+"::"): internal_owner_calls.append(item)
                else: owner_escapes.append(item)
            elif off in targets:
                field_addr_escapes.append((r.get("function_entry"),r.get("op_address"),target,f"0x{off:x}",r.get("arg_index")))
        elif kind=="owner_field_value":
            field_value_calls.append((r.get("function_entry"),r.get("op_address"),target,f"0x{off:x}" if off is not None else None,r.get("arg_index")))
        elif kind=="owner_derived_unknown":
            unknown_args.append((r.get("function_entry"),r.get("op_address"),target,r.get("call_kind"),r.get("arg_index")))
    if unknown_stores: reasons.append("owner_derived_unknown_store_present")
    if field_addr_escapes: reasons.append("target_field_address_escape")
    if unknown_args: reasons.append("owner_derived_unknown_call_arg_present")
    if owner_escapes: reasons.append("whole_owner_call_frontier_open")
    return {
        "format":OUT_FORMAT,"class_name":cls,"target_fields":[f"0x{x:x}" for x in sorted(targets)],
        "method_summaries":len(summaries),"owner_method_summaries":len(methods),
        "non_owner_parameter_methods":len(static_like),
        "owner_derived_unknown_stores":unknown_stores,
        "whole_owner_call_frontier":owner_escapes,
        "internal_owner_calls":internal_owner_calls,
        "target_field_address_escapes":field_addr_escapes,
        "owner_derived_unknown_call_args":unknown_args,
        "owner_field_value_calls":field_value_calls,
        "pointer_slot_alias_stability_proven":not reasons,
        "reasons":reasons,"affects_routing":False,
    }

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--class-flow",action="append",nargs=2,metavar=("CLASS","TSV"),required=True)
    ap.add_argument("--details-output",type=Path)
    ap.add_argument("--require-closed",action="store_true")
    a=ap.parse_args()
    details=[]
    for cls,path_s in a.class_flow:
        path=Path(path_s);rows=read_tsv(path);validate(rows,path)
        targets=DEFAULT_TARGETS.get(cls)
        if targets is None: raise ValueError(f"No target field set configured for {cls}")
        details.append(audit_class(rows,cls,targets))
    print("format:",OUT_FORMAT)
    print("classes:",len(details))
    for r in details:
        print(r["class_name"],"owner_methods=",r["owner_method_summaries"],"static_or_unproven=",r["non_owner_parameter_methods"],"field_value_calls=",len(r["owner_field_value_calls"]),"owner_frontier=",len(r["whole_owner_call_frontier"]),"field_address_escapes=",len(r["target_field_address_escapes"]),"unknown_args=",len(r["owner_derived_unknown_call_args"]),"unknown_stores=",len(r["owner_derived_unknown_stores"]),"closed=",r["pointer_slot_alias_stability_proven"],"reasons=",r["reasons"])
    if a.details_output:
        a.details_output.parent.mkdir(parents=True,exist_ok=True)
        a.details_output.write_text("".join(json.dumps(x,sort_keys=True)+"\n" for x in details),encoding="utf-8")
        print("details output:",a.details_output)
    print("affects routing: no")
    if a.require_closed and any(not r["pointer_slot_alias_stability_proven"] for r in details): return 3
    return 0
if __name__=="__main__": raise SystemExit(main())
