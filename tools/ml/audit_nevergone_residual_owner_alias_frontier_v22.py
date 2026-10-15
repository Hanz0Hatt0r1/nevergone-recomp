#!/usr/bin/env python3
"""Audit residual owner-field receiver candidates against class-wide alias evidence.

Evidence-only. Exact non-null field assignments may identify a factory class, but
that class is not promoted unless the bounded class-wide alias inventory also
proves that no unresolved parameter-0 store/call channel can replace the field.
"""
from __future__ import annotations
import argparse,csv,json
from collections import defaultdict
from pathlib import Path
from typing import Any

WRITE_FORMAT="NEVERGONE.GhidraClassParameterFieldWrite/1"
ALIAS_FORMAT="NEVERGONE.GhidraClassOwnerAliasInventory/1"
SLICE_FORMAT="NEVERGONE.GhidraStructuralReceiverSlice/1"
OUT_FORMAT="NEVERGONE.ResidualOwnerAliasFrontier/1"
BEAT_FIELDS={0x128,0x12c,0x130,0x134,0x138,0x14c,0x150,0x154,0x158}
JOY_FIELDS={0x14c}
CCSPRITE_FACTORIES={
    "cocos2d::CCSprite::create",
    "cocos2d::CCSprite::createWithSpriteFrameName",
}

def tsv(path:Path)->list[dict[str,str]]:
    with path.open(encoding="utf-8",newline="") as f:return list(csv.DictReader(f,delimiter="\t"))
def num(v:Any)->int|None:
    if v in (None,"","-"):return None
    try:return int(str(v),0)
    except ValueError:return None
def dec(v:Any,d=0):
    try:return int(str(v),10)
    except:return d
def parse_csv_offsets(v:str|None)->list[int]:
    if not v or v=="-":return []
    out=[]
    for x in v.split(","):
        n=num(x)
        if n is not None:out.append(n)
    return out

def validate(rows,path,fmt):
    for i,r in enumerate(rows,2):
        if r.get("format")!=fmt:raise ValueError(f"{path}:{i}: expected {fmt}")

def write_summary(rows:list[dict[str,str]],cls:str,target_fields:set[int]):
    summaries=[r for r in rows if r.get("row_type")=="summary"]
    if not summaries:return {"complete":False,"reasons":["missing_method_summaries"]}
    reasons=[]
    failed=[r for r in summaries if r.get("decompile_status")!="ok"]
    if failed:reasons.append("class_method_decompilation_incomplete")
    unknown_stores=sum(dec(r.get("param0_unknown_store_count")) for r in summaries)
    if unknown_stores:reasons.append("parameter0_unknown_store_present")
    owner_escapes=[]; field_escapes=[]; unknown_calls=[]
    prefix=cls+"::"
    for r in rows:
        if r.get("row_type")!="call":continue
        offsets=parse_csv_offsets(r.get("param0_arg_offsets"))
        unknown=(r.get("param0_unknown_arg_indices") not in (None,"","-"))
        target=r.get("call_target_name") or "-"
        internal=target.startswith(prefix)
        if unknown:
            unknown_calls.append((r.get("function_entry"),r.get("op_address"),target,r.get("param0_unknown_arg_indices")))
        if internal:continue
        if 0 in offsets:owner_escapes.append((r.get("function_entry"),r.get("op_address"),target))
        hit=sorted(set(offsets)&target_fields)
        if hit:field_escapes.append((r.get("function_entry"),r.get("op_address"),target,[f"0x{x:x}" for x in hit]))
    if owner_escapes:reasons.append("whole_owner_external_call_escape")
    if field_escapes:reasons.append("target_field_address_call_escape")
    if unknown_calls:reasons.append("parameter0_unknown_call_escape")
    return {
        "complete":not failed,
        "method_count":len(summaries),
        "unknown_param0_stores":unknown_stores,
        "whole_owner_external_calls":owner_escapes,
        "target_field_address_calls":field_escapes,
        "unknown_param0_calls":unknown_calls,
        "reasons":reasons,
        "alias_stability_proven":not reasons,
    }

def direct_field_writes(rows:list[dict[str,str]],fields:set[int]):
    by=defaultdict(list)
    for r in rows:
        off=num(r.get("address_offset"))
        if off in fields:by[off].append(r)
    out={}
    for field in sorted(fields):
        rs=by.get(field,[]); factories=[]; other_nonnull=[]; nulls=0
        for r in rs:
            root=r.get("value_root_token") or ""
            if root.startswith("const:0x0:"):nulls+=1;continue
            if r.get("value_root_opcode")=="CALL" and r.get("factory_target_name") not in (None,"","-"):
                factories.append({"entry":r.get("factory_target_entry"),"name":r.get("factory_target_name"),"writer":r.get("function_name"),"op":r.get("op_address")})
            else:other_nonnull.append({"root":root,"opcode":r.get("value_root_opcode"),"writer":r.get("function_name"),"op":r.get("op_address")})
        out[field]={"write_count":len(rs),"null_write_count":nulls,"factory_writes":factories,"other_nonnull_writes":other_nonnull}
    return out

def beat_call_fields(slice_rows:list[dict[str,str]]):
    nodes=defaultdict(list); slots={}
    for r in slice_rows:
        idx=dec(r.get("call_index"),-1)
        if idx<0:continue
        if r.get("row_type")=="summary":slots[idx]=r.get("slot_offset")
        elif r.get("row_type")=="node":nodes[idx].append(r)
    out={}
    for idx,rs in nodes.items():
        constants={num(r.get("constant_value")) for r in rs}
        fields=sorted(x for x in constants if x in BEAT_FIELDS)
        if idx==6 and 0x14c in fields and any(num(r.get("constant_value"))==4 for r in rs):fields=[0x14c,0x150,0x154]
        out[idx]={"slot":slots.get(idx),"fields":fields}
    return out

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("beat_writes",type=Path);ap.add_argument("joy_writes",type=Path)
    ap.add_argument("beat_alias",type=Path);ap.add_argument("joy_alias",type=Path)
    ap.add_argument("beat_slice",type=Path);ap.add_argument("--details-output",type=Path);ap.add_argument("--require-proven",action="store_true")
    a=ap.parse_args()
    bw,jw,ba,ja,bs=map(tsv,[a.beat_writes,a.joy_writes,a.beat_alias,a.joy_alias,a.beat_slice])
    validate(bw,a.beat_writes,WRITE_FORMAT);validate(jw,a.joy_writes,WRITE_FORMAT);validate(ba,a.beat_alias,ALIAS_FORMAT);validate(ja,a.joy_alias,ALIAS_FORMAT);validate(bs,a.beat_slice,SLICE_FORMAT)
    beat_alias=write_summary(ba,"BeatenEffect",BEAT_FIELDS);joy_alias=write_summary(ja,"CCJoyStick",JOY_FIELDS)
    beat_w=direct_field_writes(bw,BEAT_FIELDS);joy_w=direct_field_writes(jw,JOY_FIELDS);callmap=beat_call_fields(bs)
    details=[]
    for idx in sorted(callmap):
        info=callmap[idx]; fields=info["fields"]
        factories=[];others=[];writes=0
        for f in fields:
            d=beat_w[f];writes+=d["write_count"];factories+=d["factory_writes"];others+=d["other_nonnull_writes"]
        factory_names=sorted({x["name"] for x in factories})
        factory_only=bool(factories) and not others and set(factory_names)<=CCSPRITE_FACTORIES
        proven=factory_only and beat_alias["alias_stability_proven"]
        reasons=[]
        if not fields:reasons.append("receiver_field_not_exactly_mapped")
        if not factories:reasons.append("no_nonnull_factory_write")
        if others:reasons.append("other_nonnull_write_present")
        if factory_names and not set(factory_names)<=CCSPRITE_FACTORIES:reasons.append("non_ccsprite_factory_present")
        reasons+=beat_alias["reasons"]
        details.append({"format":OUT_FORMAT,"source_id":"libcocos2dcpp.so:002a47c6","call_index":idx,"dispatch_slot_offset":info["slot"],"owner_class":"BeatenEffect","receiver_field_offsets":[f"0x{x:x}" for x in fields],"direct_write_count":writes,"factory_targets":factory_names,"factory_only_nonnull_ccsprite":factory_only,"alias_stability_proven":beat_alias["alias_stability_proven"],"receiver_class_proven":proven,"candidate_class_name":"cocos2d::CCSprite" if factory_only else None,"reasons":sorted(set(reasons)),"affects_routing":False})
    jd=joy_w[0x14c]; jnames=sorted({x["name"] for x in jd["factory_writes"]}); jfactory=bool(jd["factory_writes"]) and not jd["other_nonnull_writes"] and set(jnames)<=CCSPRITE_FACTORIES
    jproven=jfactory and joy_alias["alias_stability_proven"]
    jreasons=[]
    if not jd["factory_writes"]:jreasons.append("no_nonnull_factory_write")
    if jd["other_nonnull_writes"]:jreasons.append("other_nonnull_write_present")
    jreasons+=joy_alias["reasons"]
    details.append({"format":OUT_FORMAT,"source_id":"libcocos2dcpp.so:002a5dd0","call_index":0,"dispatch_slot_offset":"0x14c","owner_class":"CCJoyStick","receiver_field_offsets":["0x14c"],"direct_write_count":jd["write_count"],"factory_targets":jnames,"factory_only_nonnull_ccsprite":jfactory,"alias_stability_proven":joy_alias["alias_stability_proven"],"receiver_class_proven":jproven,"candidate_class_name":"cocos2d::CCSprite" if jfactory else None,"reasons":sorted(set(jreasons)),"affects_routing":False})
    proven=sum(bool(x["receiver_class_proven"]) for x in details)
    print("format:",OUT_FORMAT);print("rows:",len(details));print("receiver classes proven:",proven);print("BeatenEffect alias reasons:",beat_alias["reasons"]);print("CCJoyStick alias reasons:",joy_alias["reasons"])
    for r in details:print(r["source_id"],"call",r["call_index"],"fields=",r["receiver_field_offsets"],"factories=",r["factory_targets"],"alias=",r["alias_stability_proven"],"proven=",r["receiver_class_proven"],"reasons=",r["reasons"])
    if a.details_output:
        a.details_output.parent.mkdir(parents=True,exist_ok=True);a.details_output.write_text("".join(json.dumps(x,sort_keys=True)+"\n" for x in details),encoding="utf-8");print("details output:",a.details_output)
    print("affects routing: no")
    if a.require_proven and proven!=len(details):return 3
    return 0
if __name__=="__main__":raise SystemExit(main())
