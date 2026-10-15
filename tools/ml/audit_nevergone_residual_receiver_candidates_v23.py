#!/usr/bin/env python3
"""Join v21/v22 native field evidence with v23 alias-flow precision.

No class is promoted unless exact field mapping, exact factory provenance and the
class-level pointer-slot alias gate all agree. Protobuf schema is reported only
as an expectation and never acts as native class proof.
"""
from __future__ import annotations
import argparse,csv,json,re
from collections import defaultdict
from pathlib import Path
from typing import Any
WRITE_FMT="NEVERGONE.GhidraClassParameterFieldWrite/1"
SLICE_FMT="NEVERGONE.GhidraStructuralReceiverSlice/1"
FLOW_AUDIT_FMT="NEVERGONE.OwnerAliasFlowAudit/1"
OUT_FMT="NEVERGONE.ResidualReceiverCandidateV23/1"
CCSPRITE_FACTORIES={"cocos2d::CCSprite::create","cocos2d::CCSprite::createWithSpriteFrameName"}
BEAT_FIELDS={0x128,0x12c,0x130,0x134,0x138,0x14c,0x150,0x154,0x158}

def tsv(p:Path):
    with p.open(encoding="utf-8",newline="") as f:return list(csv.DictReader(f,delimiter="\t"))
def js(p:Path):return [json.loads(x) for x in p.read_text(encoding="utf-8").splitlines() if x.strip()]
def num(v:Any):
    if v in (None,"","-"):return None
    try:return int(str(v),0)
    except:return None
def dec(v,d=-1):
    try:return int(str(v),10)
    except:return d
def validate(rows,p,fmt):
    for i,r in enumerate(rows,2):
        if r.get("format")!=fmt:raise ValueError(f"{p}:{i}: expected {fmt}")
def writes_by_offset(rows):
    out=defaultdict(list)
    for r in rows:
        off=num(r.get("address_offset"))
        if off is not None:out[off].append(r)
    return out
def fields_from_slice(rows,known):
    nodes=defaultdict(list);slots={}
    for r in rows:
        idx=dec(r.get("call_index"));
        if idx<0:continue
        if r.get("row_type")=="summary":slots[idx]=r.get("slot_offset")
        elif r.get("row_type")=="node":nodes[idx].append(r)
    out={}
    for idx,rs in nodes.items():
        cs={num(r.get("constant_value")) for r in rs}; fs=sorted(x for x in cs if x in known)
        if idx==6 and 0x14c in fs and 4 in cs:fs=[0x14c,0x150,0x154]
        out[idx]=(slots.get(idx),fs)
    return out
def field_factory(rows):
    factories=[];other=[];nulls=0
    for r in rows:
        root=r.get("value_root_token") or ""
        if root.startswith("const:0x0:"):nulls+=1;continue
        if r.get("value_root_opcode")=="CALL" and r.get("factory_target_name") not in (None,"","-"):
            factories.append(r.get("factory_target_name"))
        else:other.append(root or "-")
    return sorted(set(factories)),other,nulls

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("beat_writes",type=Path);ap.add_argument("joy_writes",type=Path);ap.add_argument("proto_writes",type=Path)
    ap.add_argument("beat_slice",type=Path);ap.add_argument("joy_slice",type=Path);ap.add_argument("proto_slice",type=Path)
    ap.add_argument("flow_audit_jsonl",type=Path);ap.add_argument("--details-output",type=Path);a=ap.parse_args()
    bw,jw,pw=map(tsv,[a.beat_writes,a.joy_writes,a.proto_writes]);bs,jsl,ps=map(tsv,[a.beat_slice,a.joy_slice,a.proto_slice]);fa=js(a.flow_audit_jsonl)
    for rows,p in [(bw,a.beat_writes),(jw,a.joy_writes),(pw,a.proto_writes)]:validate(rows,p,WRITE_FMT)
    for rows,p in [(bs,a.beat_slice),(jsl,a.joy_slice),(ps,a.proto_slice)]:validate(rows,p,SLICE_FMT)
    for i,r in enumerate(fa,1):
        if r.get("format")!=FLOW_AUDIT_FMT:raise ValueError(f"{a.flow_audit_jsonl}:{i}: expected {FLOW_AUDIT_FMT}")
    closed={r.get("class_name"):bool(r.get("pointer_slot_alias_stability_proven")) for r in fa}
    wb,wj,wp=map(writes_by_offset,[bw,jw,pw]);details=[]
    # BeatenEffect calls 0..6.
    for idx,(slot,fields) in sorted(fields_from_slice(bs,BEAT_FIELDS).items()):
        factories=[];others=[];nulls=0
        for f in fields:
            ff,oo,nn=field_factory(wb.get(f,[]));factories+=ff;others+=oo;nulls+=nn
        factories=sorted(set(factories));factory_only=bool(factories) and not others and set(factories)<=CCSPRITE_FACTORIES
        proven=factory_only and closed.get("BeatenEffect",False)
        reasons=[]
        if not fields:reasons.append("receiver_field_not_exactly_mapped")
        if not factories:reasons.append("no_nonnull_factory_write")
        if others:reasons.append("other_nonnull_write_present")
        if factories and not set(factories)<=CCSPRITE_FACTORIES:reasons.append("non_ccsprite_factory_present")
        if not closed.get("BeatenEffect",False):reasons.append("owner_alias_frontier_open")
        details.append({"format":OUT_FMT,"source_id":"libcocos2dcpp.so:002a47c6","call_index":idx,"owner_class":"BeatenEffect","field_offsets":[f"0x{x:x}" for x in fields],"dispatch_slot_offset":slot,"candidate_class_name":"cocos2d::CCSprite" if factory_only else None,"factory_targets":factories,"receiver_class_proven":proven,"reasons":sorted(set(reasons)),"affects_routing":False})
    # CCJoyStick setDockTexture calls both use +0x10c.
    jf,jother,jnull=field_factory(wj.get(0x10c,[]));jfactory=bool(jf) and not jother and set(jf)<=CCSPRITE_FACTORIES
    for idx,(slot,fields) in sorted(fields_from_slice(jsl,{0x10c}).items()):
        exact=fields==[0x10c];proven=exact and jfactory and closed.get("CCJoyStick",False);reasons=[]
        if not exact:reasons.append("receiver_field_not_exactly_0x10c")
        if not jfactory:reasons.append("ccsprite_factory_not_unique")
        if not closed.get("CCJoyStick",False):reasons.append("owner_alias_frontier_open")
        details.append({"format":OUT_FMT,"source_id":"libcocos2dcpp.so:002a651c","call_index":idx,"owner_class":"CCJoyStick","field_offsets":[f"0x{x:x}" for x in fields],"dispatch_slot_offset":slot,"candidate_class_name":"cocos2d::CCSprite" if exact and jfactory else None,"factory_targets":jf,"receiver_class_proven":proven,"reasons":sorted(set(reasons)),"affects_routing":False})
    # Protobuf SharedDtor: four exact fields are expected by the recovered proto,
    # but class proof still requires native non-null writes identifying the nested type.
    pmap=fields_from_slice(ps,{0x8,0xc,0x10,0x14})
    for idx,(slot,fields) in sorted(pmap.items()):
        native=[];others=[]
        for f in fields:
            ff,oo,_=field_factory(wp.get(f,[]));native+=ff;others+=oo
        native=sorted(set(native));native_class=(native==["PVEDATA::equipToolsDataMsg"])
        proven=native_class and not others and closed.get("PVEDATA::equipToolsListMsg",False)
        reasons=[]
        if len(fields)!=1:reasons.append("protobuf_receiver_field_not_exact")
        if not native_class:reasons.append("native_nested_message_class_not_proven")
        if others:reasons.append("protobuf_other_nonnull_write_present")
        if not closed.get("PVEDATA::equipToolsListMsg",False):reasons.append("owner_alias_frontier_open")
        details.append({"format":OUT_FMT,"source_id":"libcocos2dcpp.so:00299d90","call_index":idx,"owner_class":"PVEDATA::equipToolsListMsg","field_offsets":[f"0x{x:x}" for x in fields],"dispatch_slot_offset":slot,"schema_expected_class_name":"PVEDATA::equipToolsDataMsg","candidate_class_name":"PVEDATA::equipToolsDataMsg" if native_class and not others else None,"factory_targets":native,"receiver_class_proven":proven,"reasons":sorted(set(reasons)),"affects_routing":False})
    print("format:",OUT_FMT);print("rows:",len(details));print("receiver classes proven:",sum(bool(x["receiver_class_proven"]) for x in details))
    for r in details:print(r["source_id"],"call",r["call_index"],"fields=",r["field_offsets"],"candidate=",r["candidate_class_name"],"proven=",r["receiver_class_proven"],"reasons=",r["reasons"])
    if a.details_output:
        a.details_output.parent.mkdir(parents=True,exist_ok=True);a.details_output.write_text("".join(json.dumps(x,sort_keys=True)+"\n" for x in details),encoding="utf-8");print("details output:",a.details_output)
    print("affects routing: no");return 0
if __name__=="__main__":raise SystemExit(main())
