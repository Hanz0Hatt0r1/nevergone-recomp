#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
STORE_FORMAT = "NEVERGONE.GhidraExactParameterFieldFactoryStore/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
DETAIL_FORMAT = "NEVERGONE.ResidualReceiverRecovery/1"
FINAL_FORMAT = "NEVERGONE.ResidualReceiverRecoveryFinalization/1"
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match", "resolved_converged_target"}

ANDROID_SOURCE = "libcocos2dcpp.so:002a08fc"
ANDROID_TARGET_CALL = 0
ANDROID_ANCHOR_CALL = 1
JOY_SOURCE = "libcocos2dcpp.so:002a647e"
JOY_CALL_ROOT_ANCHOR = 1
JOY_CALL_ROOT_TARGET = 2
JOY_FIELD_ANCHOR = 4
JOY_FIELD_TARGETS = (0, 3, 5)
JOY_OWNER = "CCJoyStick"
JOY_ANCHOR_FIELD = 0x104
JOY_TARGET_FIELD = 0x108


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows=[]
    with path.open(encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            if not raw.strip():
                continue
            row=json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{n}: expected object")
            rows.append(row)
    return rows


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, sort_keys=True)+"\n")


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def key(row: dict[str, Any], where: str) -> tuple[str, int]:
    source=row.get("source_id"); call=row.get("call_index")
    if not isinstance(source, str) or not isinstance(call, int):
        raise ValueError(f"{where}: invalid source/call")
    return source, call


def norm_hex(value: Any) -> str | None:
    if value in (None, "", "-"):
        return None
    try:
        return f"0x{int(str(value), 0):x}"
    except Exception:
        return None


def parse_int(value: Any) -> int | None:
    if value in (None, "", "-"):
        return None
    try:
        return int(str(value), 0)
    except Exception:
        return None


def load_slices(slice_dir: Path) -> dict[tuple[str, int], dict[str, Any]]:
    out: dict[tuple[str, int], dict[str, Any]] = {}
    for path in sorted(slice_dir.glob("*.tsv")):
        rows=read_tsv(path)
        by_call: dict[tuple[str,int], list[dict[str,str]]] = {}
        for n,row in enumerate(rows,2):
            if row.get("format") != SLICE_FORMAT:
                raise ValueError(f"{path}:{n}: expected {SLICE_FORMAT}")
            entry=(row.get("function_entry") or "").lower()
            call=parse_int(row.get("call_index"))
            if not entry or call is None:
                raise ValueError(f"{path}:{n}: invalid function/call")
            source=f"libcocos2dcpp.so:{entry}"
            by_call.setdefault((source,call), []).append(row)
        for k, group in by_call.items():
            if k in out:
                raise ValueError(f"duplicate structural slice group: {k}")
            summaries=[r for r in group if r.get("row_type")=="summary"]
            nodes=[r for r in group if r.get("row_type")=="node"]
            if len(summaries)!=1:
                raise ValueError(f"{path}: expected one summary for {k}")
            summary=summaries[0]
            root=[r for r in nodes if parse_int(r.get("node_id"))==0]
            root_row=root[0] if len(root)==1 else None
            out[k]={"summary":summary,"nodes":nodes,"root":root_row,"path":str(path)}
    return out


def complete_slice(s: dict[str, Any] | None) -> bool:
    if not s: return False
    summary=s["summary"]
    return (
        s.get("root") is not None
        and summary.get("recovery_status")=="canonical_receiver"
        and parse_int(summary.get("truncated_by_budget"))==0
        and parse_int(summary.get("depth_truncation_count"))==0
        and parse_int(summary.get("cycle_count"))==0
    )


def child_token(s: dict[str, Any], parent: int, input_index: int) -> str | None:
    matches=[r for r in s["nodes"] if parse_int(r.get("parent_node_id"))==parent and parse_int(r.get("input_index"))==input_index]
    if len(matches)!=1:
        return None
    token=matches[0].get("node_token")
    return token if token not in (None,"","-") else None


def exact_parameter_field(s: dict[str, Any]) -> tuple[str,int] | None:
    if not complete_slice(s) or s["root"].get("defining_opcode")!="LOAD":
        return None
    level1=[r for r in s["nodes"] if parse_int(r.get("parent_node_id"))==0 and parse_int(r.get("input_index"))==1]
    if len(level1)!=1: return None
    node=level1[0]
    node_id=parse_int(node.get("node_id"))
    if node.get("defining_opcode") in {"CAST","COPY"}:
        nxt=[r for r in s["nodes"] if parse_int(r.get("parent_node_id"))==node_id and parse_int(r.get("input_index"))==0]
        if len(nxt)!=1: return None
        node=nxt[0]; node_id=parse_int(node.get("node_id"))
    if node.get("defining_opcode") not in {"PTRADD","INT_ADD"}: return None
    children=[r for r in s["nodes"] if parse_int(r.get("parent_node_id"))==node_id]
    base=[r for r in children if parse_int(r.get("input_index"))==0]
    consts=[r for r in children if parse_int(r.get("input_index"))==1]
    if len(base)!=1 or len(consts)!=1: return None
    field=parse_int(consts[0].get("constant_value"))
    base_token=base[0].get("node_token")
    if field is None or base_token in (None,"","-"): return None
    return str(base_token), field


def catalog_target(catalog: list[dict[str,Any]], cls: str, vt: str, slot: str, reasons: list[str]) -> tuple[str|None, str]:
    rows=[r for r in catalog if r.get("class_name")==cls and r.get("receiver_vtable_id")==vt and norm_hex(r.get("dispatch_slot_offset"))==slot]
    if len(rows)!=1:
        reasons.append("catalog_exact_class_vtable_slot_not_unique")
        return None, "runtime_unobserved"
    row=rows[0]
    if row.get("static_status")!="single_function_memory_pointer": reasons.append("catalog_slot_not_single_function")
    targets=row.get("candidate_target_ids")
    if not isinstance(targets,list) or len(targets)!=1 or not isinstance(targets[0],str):
        reasons.append("catalog_target_not_unique"); target=None
    else: target=targets[0]
    if row.get("runtime_validation_status")=="runtime_mismatch": reasons.append("catalog_runtime_mismatch")
    return target, str(row.get("runtime_validation_status") or "runtime_unobserved")


def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("v17_inference_jsonl", type=Path)
    p.add_argument("v17_queries_jsonl", type=Path)
    p.add_argument("slice_dir", type=Path)
    p.add_argument("ccjoystick_field_store_tsv", type=Path)
    p.add_argument("catalog_jsonl", type=Path)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--queries-output", type=Path, required=True)
    p.add_argument("--details-output", type=Path)
    a=p.parse_args()

    inference=read_jsonl(a.v17_inference_jsonl)
    queries=read_jsonl(a.v17_queries_jsonl)
    catalog=read_jsonl(a.catalog_jsonl)
    for n,row in enumerate(catalog,1):
        if row.get("format")!=CATALOG_FORMAT: raise ValueError(f"{a.catalog_jsonl}:{n}: expected {CATALOG_FORMAT}")
    slices=load_slices(a.slice_dir)
    stores=read_tsv(a.ccjoystick_field_store_tsv)
    for n,row in enumerate(stores,2):
        if row.get("format")!=STORE_FORMAT: raise ValueError(f"{a.ccjoystick_field_store_tsv}:{n}: expected {STORE_FORMAT}")

    by: dict[tuple[str,int],dict[str,Any]]={}
    for n,r in enumerate(inference,1):
        if r.get("format")!=INFERENCE_FORMAT: raise ValueError(f"{a.v17_inference_jsonl}:{n}: expected {INFERENCE_FORMAT}")
        k=key(r,f"inference:{n}")
        if k in by: raise ValueError(f"duplicate inference row: {k}")
        by[k]=r
    qby: dict[tuple[str,int],dict[str,Any]]={}
    for n,q in enumerate(queries,1):
        if q.get("format")!=QUERY_FORMAT: raise ValueError(f"{a.v17_queries_jsonl}:{n}: expected {QUERY_FORMAT}")
        k=key(q,f"query:{n}")
        if k in qby: raise ValueError(f"duplicate query {k}")
        qby[k]=dict(q)

    recoveries: dict[tuple[str,int], dict[str,Any]] = {}

    def add_recovery(target_key: tuple[str,int], cls: str, vt: str, proof_kind: str, evidence_source: str, extra: dict[str,Any], reasons: list[str]) -> None:
        row=by.get(target_key)
        if row is None: reasons.append("target_inference_missing"); slot=None
        else:
            slot=norm_hex(row.get("dispatch_slot_offset"))
            if row.get("status")!="unresolved_receiver_class": reasons.append("target_status_not_unresolved_receiver_class")
            if target_key in qby: reasons.append("target_query_already_exists")
        target_id=None; runtime="runtime_unobserved"
        if slot is None: reasons.append("target_slot_invalid")
        else: target_id,runtime=catalog_target(catalog,cls,vt,slot,reasons)
        rec={
            "format":DETAIL_FORMAT,"source_id":target_key[0],"call_index":target_key[1],
            "dispatch_slot_offset":slot,"accepted":not reasons and target_id is not None,
            "proof_kind":proof_kind,"receiver_class_name":cls,"receiver_vtable_id":vt,
            "candidate_target_ids":[target_id] if target_id else [],"reasons":reasons,
            "affects_routing":False, **extra,
        }
        recoveries[target_key]=rec
        if rec["accepted"]:
            assert row is not None and slot is not None and target_id is not None
            row["class_name"]=cls
            row["class_match_kind"]=proof_kind
            row["class_evidence_source"]=evidence_source
            row["candidate_receiver_class_names"]=[cls]
            row["candidate_receiver_vtable_ids"]=[vt]
            row["receiver_vtable_id"]=vt
            row["status"]="resolved_unique_vtable"
            row["residual_receiver_recovery_v19"]=rec
            qby[target_key]={
                "format":QUERY_FORMAT,"source_id":target_key[0],"call_index":target_key[1],
                "call_address":row.get("call_address"),"receiver_vtable_id":vt,
                "dispatch_slot_offset":slot,"receiver_class_name":cls,
                "receiver_class_match_kind":proof_kind,"receiver_class_evidence_source":evidence_source,
                "receiver_type_name":row.get("receiver_type_name"),"receiver_type_display":row.get("receiver_type_display"),
                "runtime_validation_status":runtime,"evidence_ref":f"{target_key[0]}#call={target_key[1]}",
                "affects_routing":False,
            }
        elif row is not None:
            row["residual_receiver_recovery_v19"]=rec

    ar=by.get((ANDROID_SOURCE,ANDROID_ANCHOR_CALL),{})
    reasons=[]
    proof=ar.get("exact_singleton_cell_recovery") if isinstance(ar.get("exact_singleton_cell_recovery"),dict) else {}
    if ar.get("status") not in RESOLVED: reasons.append("android_anchor_not_resolved")
    if proof.get("accepted") is not True: reasons.append("android_anchor_singleton_proof_missing")
    cls=ar.get("class_name"); vt=ar.get("receiver_vtable_id")
    if cls!="AndroidSdkMgr" or vt!="libcocos2dcpp.so:008f3ce8": reasons.append("android_anchor_identity_mismatch")
    s0=slices.get((ANDROID_SOURCE,ANDROID_TARGET_CALL)); s1=slices.get((ANDROID_SOURCE,ANDROID_ANCHOR_CALL))
    if not complete_slice(s0) or not complete_slice(s1): reasons.append("android_slice_not_complete")
    addr0=child_token(s0,0,1) if s0 else None; addr1=child_token(s1,0,1) if s1 else None
    if not addr0 or addr0!=addr1: reasons.append("android_receiver_address_token_not_exact_match")
    if s0 and s0.get("root") and s0["root"].get("defining_opcode")!="LOAD": reasons.append("android_target_root_not_load")
    if s1 and s1.get("root") and s1["root"].get("defining_opcode")!="LOAD": reasons.append("android_anchor_root_not_load")
    add_recovery((ANDROID_SOURCE,ANDROID_TARGET_CALL), str(cls or "AndroidSdkMgr"), str(vt or "libcocos2dcpp.so:008f3ce8"),
        "exact_same_receiver_address_cell_as_proven_singleton",
        "exact_same_structural_load_address_as_exact_singleton_cell_recovery",
        {"anchor_call_index":ANDROID_ANCHOR_CALL,"shared_address_token":addr0,"singleton_cell":proof.get("singleton_cell")}, reasons)

    jr=by.get((JOY_SOURCE,JOY_CALL_ROOT_ANCHOR),{})
    reasons=[]
    jproof=jr.get("call_root_factory_recovery") if isinstance(jr.get("call_root_factory_recovery"),dict) else {}
    if jr.get("status") not in RESOLVED or jproof.get("accepted") is not True: reasons.append("joy_call_anchor_factory_proof_missing")
    cls=jr.get("class_name"); vt=jr.get("receiver_vtable_id")
    if cls!="cocos2d::CCSprite" or vt!="libcocos2dcpp.so:009363d8": reasons.append("joy_call_anchor_identity_mismatch")
    sa=slices.get((JOY_SOURCE,JOY_CALL_ROOT_ANCHOR)); st=slices.get((JOY_SOURCE,JOY_CALL_ROOT_TARGET))
    if not complete_slice(sa) or not complete_slice(st): reasons.append("joy_call_root_slice_not_complete")
    root_a=sa["root"].get("node_token") if sa and sa.get("root") else None; root_t=st["root"].get("node_token") if st and st.get("root") else None
    if not root_a or root_a!=root_t: reasons.append("joy_call_root_token_not_exact_match")
    if sa and sa.get("root") and sa["root"].get("defining_opcode")!="CALL": reasons.append("joy_call_anchor_root_not_call")
    if st and st.get("root") and st["root"].get("defining_opcode")!="CALL": reasons.append("joy_call_target_root_not_call")
    add_recovery((JOY_SOURCE,JOY_CALL_ROOT_TARGET), str(cls or "cocos2d::CCSprite"), str(vt or "libcocos2dcpp.so:009363d8"),
        "exact_same_call_root_as_proven_factory_return",
        "exact_same_structural_call_root_as_factory_constructor_proven_receiver",
        {"anchor_call_index":JOY_CALL_ROOT_ANCHOR,"shared_call_root_token":root_t,"factory_callee_entry":jproof.get("callee_entry"),"factory_callee_name":jproof.get("callee_name")}, reasons)

    jf=by.get((JOY_SOURCE,JOY_FIELD_ANCHOR),{})
    fproof=jf.get("ccjoystick_field_recovery_v13") if isinstance(jf.get("ccjoystick_field_recovery_v13"),dict) else {}
    anchor_slice=slices.get((JOY_SOURCE,JOY_FIELD_ANCHOR)); anchor_field=exact_parameter_field(anchor_slice) if anchor_slice else None
    base_anchor, field_anchor=(anchor_field if anchor_field else (None,None))
    common_reasons=[]
    if jf.get("status") not in RESOLVED or fproof.get("accepted") is not True: common_reasons.append("joy_field_anchor_proof_missing")
    if fproof.get("owner_class")!=JOY_OWNER or parse_int(fproof.get("field_offset"))!=JOY_ANCHOR_FIELD: common_reasons.append("joy_field_anchor_owner_or_offset_mismatch")
    cls=jf.get("class_name"); vt=jf.get("receiver_vtable_id")
    if cls!="cocos2d::CCSprite" or vt!="libcocos2dcpp.so:009363d8": common_reasons.append("joy_field_anchor_identity_mismatch")
    if field_anchor!=JOY_ANCHOR_FIELD or not base_anchor: common_reasons.append("joy_field_anchor_structural_field_mismatch")

    relevant=[r for r in stores if (r.get("function_entry") or "").lower()=="002a6584" and (r.get("function_name") or "").startswith("CCJoyStick::init") and r.get("base_parameter_index")=="0"]
    def unique_store(off:int):
        xs=[r for r in relevant if parse_int(r.get("address_offset"))==off]
        return xs[0] if len(xs)==1 else None
    store104=unique_store(JOY_ANCHOR_FIELD); store108=unique_store(JOY_TARGET_FIELD)
    if store104 is None: common_reasons.append("joy_anchor_field_store_not_unique")
    if store108 is None: common_reasons.append("joy_target_field_store_not_unique")
    if store104 and store104.get("value_root_opcode")!="CALL": common_reasons.append("joy_anchor_store_root_not_call")
    if store108 and store108.get("value_root_opcode")!="CALL": common_reasons.append("joy_target_store_root_not_call")
    factory_entry=store108.get("factory_target_entry") if store108 else None
    factory_name=store108.get("factory_target_name") if store108 else None
    if store104 and store108:
        if store104.get("factory_target_entry")!=store108.get("factory_target_entry"): common_reasons.append("joy_field_factory_entry_not_exact_match")
        if store104.get("factory_target_name")!=store108.get("factory_target_name"): common_reasons.append("joy_field_factory_name_not_exact_match")
        if factory_entry in (None,"","-") or factory_name in (None,"","-"): common_reasons.append("joy_target_field_factory_unresolved")

    for call in JOY_FIELD_TARGETS:
        reasons=list(common_reasons)
        s=slices.get((JOY_SOURCE,call)); fld=exact_parameter_field(s) if s else None
        base, off=(fld if fld else (None,None))
        if off!=JOY_TARGET_FIELD: reasons.append("joy_target_structural_field_offset_mismatch")
        if not base or base!=base_anchor: reasons.append("joy_target_owner_parameter_token_not_exact_match")
        add_recovery((JOY_SOURCE,call), str(cls or "cocos2d::CCSprite"), str(vt or "libcocos2dcpp.so:009363d8"),
            "ccjoystick_exact_field_same_factory_callee_as_proven_sprite_field",
            "ccjoystick_same_owner_parameter_exact_field_store_same_factory_callee",
            {"anchor_call_index":JOY_FIELD_ANCHOR,"owner_class":JOY_OWNER,"receiver_field_offset":f"0x{JOY_TARGET_FIELD:x}",
             "owner_parameter_token":base,"anchor_field_offset":f"0x{JOY_ANCHOR_FIELD:x}","factory_callee_entry":factory_entry,"factory_callee_name":factory_name}, reasons)

    output=[]
    for original in inference:
        k=key(original,"output")
        output.append(by[k])
    out_queries=[qby[k] for k in sorted(qby)]
    resolved_keys={key(r,"resolved") for r in output if r.get("status") in RESOLVED}
    if resolved_keys != set(qby):
        raise ValueError(f"v19 resolved/query mismatch: missing={sorted(resolved_keys-set(qby))} extra={sorted(set(qby)-resolved_keys)}")

    details=[recoveries[k] for k in sorted(recoveries)]
    write_jsonl(a.output,output); write_jsonl(a.queries_output,out_queries)
    if a.details_output: write_jsonl(a.details_output,details)

    accepted=sum(1 for r in details if r["accepted"]); blocked=len(details)-accepted
    print("format:",FINAL_FORMAT)
    print("inference rows:",len(inference))
    print("input queries:",len(queries))
    print("eligible residual rows:",len(details))
    print("residual recoveries accepted:",accepted)
    print("residual recoveries blocked:",blocked)
    print("output queries:",len(out_queries))
    for r in details:
        print(r["source_id"],"call",r["call_index"],"slot",r["dispatch_slot_offset"],"accepted",str(r["accepted"]).lower(),"proof",r["proof_kind"],"reasons",r["reasons"])
    print("affects routing: no")
    print("output:",a.output)
    print("queries output:",a.queries_output)
    if a.details_output: print("details output:",a.details_output)
    return 0

if __name__=="__main__":
    raise SystemExit(main())
