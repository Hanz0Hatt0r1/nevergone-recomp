#!/usr/bin/env python3
"""Audit the remaining non-quarantined receiver as LOAD(param0+offset).

Evidence-only. The field offset is folded only from the persisted structural
receiver slice and the independently identified parameter-0 root. Function
ownership is taken from an exact Ghidra function-at-entry classification; the
native dynamic-symbol index is supplemental and may legitimately omit the
function. No loaded-field class is inferred here and no routing query is emitted.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any

TARGET_SOURCE = "libcocos2dcpp.so:002a647e"
TARGET_CALL = 4
TARGET_SLOT = "0x58"
GHIDRA_TO_ELF_DELTA = 0x10000
PROV_FORMAT = "NEVERGONE.GhidraStructuralReceiverParameterProvenance/1"
SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
CLASS_FORMAT = "NEVERGONE.GhidraAddressClassification/1"
CAT_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
OUT_FORMAT = "NEVERGONE.ParameterRelativeReceiverFieldAudit/2"


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
    with path.open(encoding="utf-8", newline="") as f: return list(csv.DictReader(f, delimiter="\t"))


def dec(v: Any, default: int=-1) -> int:
    try: return int(str(v),10)
    except Exception: return default


def num(v: Any) -> int|None:
    if v in (None,"","-"): return None
    s=str(v).strip()
    try:
        if s.lower().startswith("0x"): return int(s,16)
        if re.fullmatch(r"-?\d+",s): return int(s,10)
    except ValueError: pass
    return None


def norm_slot(v: Any) -> str|None:
    try: return f"0x{int(str(v),0):x}"
    except Exception: return None


def safe_id(source: str) -> str: return source.replace(":","_").replace("/","_")
def source_entry(source: str) -> int: return int(source.rsplit(":",1)[1],16)


def children_for(nodes: dict[int,dict[str,str]]) -> dict[int,list[int]]:
    out: dict[int,list[int]] = defaultdict(list)
    for nid,row in nodes.items():
        parent=dec(row.get("parent_node_id"))
        if parent >= 0: out[parent].append(nid)
    for parent in out: out[parent].sort(key=lambda nid:(dec(nodes[nid].get("input_index")),nid))
    return out


def fold_affine(nid: int, nodes: dict[int,dict[str,str]], children: dict[int,list[int]], root_token: str,
                memo: dict[int,tuple[str|None,int]|None], active:set[int]) -> tuple[str|None,int]|None:
    if nid in memo: return memo[nid]
    if nid in active: memo[nid]=None; return None
    active.add(nid)
    row=nodes[nid]; token=row.get("node_token") or "-"; opcode=row.get("defining_opcode") or "-"; const=num(row.get("constant_value")); kids=children.get(nid,[])
    def child(i:int): return fold_affine(kids[i],nodes,children,root_token,memo,active)
    value=None
    if token == root_token: value=(root_token,0)
    elif const is not None: value=(None,const)
    elif opcode in {"COPY","CAST","INDIRECT"} and kids: value=child(0)
    elif opcode in {"INT_ADD","PTRSUB"} and len(kids)>=2:
        a,b=child(0),child(1)
        if a is not None and b is not None:
            if a[0] is None: value=(b[0],a[1]+b[1])
            elif b[0] is None: value=(a[0],a[1]+b[1])
    elif opcode == "INT_SUB" and len(kids)>=2:
        a,b=child(0),child(1)
        if a is not None and b is not None and b[0] is None: value=(a[0],a[1]-b[1])
    elif opcode == "PTRADD" and len(kids)>=3:
        base,index,scale=child(0),child(1),child(2)
        if base is not None and index is not None and scale is not None and index[0] is None and scale[0] is None: value=(base[0],base[1]+index[1]*scale[1])
    elif opcode == "MULTIEQUAL" and kids:
        vals=[child(i) for i in range(len(kids))]
        if vals and all(v is not None for v in vals) and len(set(vals))==1: value=vals[0]
    active.remove(nid); memo[nid]=value; return value


def owner_from_qualified(name: str|None) -> str|None:
    if not isinstance(name,str) or "::" not in name or name.startswith("FUN_"): return None
    owner=name.split("::",1)[0].strip(); return owner or None


def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("v12_failures_jsonl",type=Path); p.add_argument("structural_provenance_tsv",type=Path); p.add_argument("slice_dir",type=Path)
    p.add_argument("function_classification_tsv",type=Path); p.add_argument("symbol_tsv",type=Path); p.add_argument("catalog_jsonl",type=Path); p.add_argument("--details-output",type=Path)
    a=p.parse_args(); reasons=[]

    failures=read_jsonl(a.v12_failures_jsonl); matches=[r for r in failures if r.get("source_id")==TARGET_SOURCE and r.get("call_index")==TARGET_CALL]; failure=matches[0] if matches else {}
    if len(matches)!=1: reasons.append("target_failure_row_not_unique")
    if failure.get("status")!="receiver_not_single_pointer": reasons.append("target_not_receiver_not_single_pointer")
    if norm_slot(failure.get("dispatch_slot_offset"))!=TARGET_SLOT: reasons.append("target_slot_mismatch")

    prov_rows=read_tsv(a.structural_provenance_tsv)
    for n,row in enumerate(prov_rows,2):
        if row.get("format")!=PROV_FORMAT: raise ValueError(f"{a.structural_provenance_tsv}:{n}: expected {PROV_FORMAT}")
    prov=[r for r in prov_rows if r.get("source_id")==TARGET_SOURCE and dec(r.get("call_index"))==TARGET_CALL]; pr=prov[0] if prov else {}
    if len(prov)!=1: reasons.append("structural_provenance_not_unique")
    if pr.get("status")!="parameter_no_direct_caller_calls": reasons.append("unexpected_structural_status")
    if pr.get("structural_receiver_is_load")!="1": reasons.append("structural_receiver_not_load")
    if dec(pr.get("parameter_index"))!=0: reasons.append("load_address_root_not_parameter_zero")
    if dec(pr.get("matched_caller_calls"),0)!=0: reasons.append("unexpected_direct_caller_calls")
    root_token=pr.get("root_token") or ""
    if not root_token or root_token=="-": reasons.append("parameter_root_token_missing")

    slice_path=a.slice_dir/f"{safe_id(TARGET_SOURCE)}.tsv"; field_offset=None; address_root_opcode=None
    if not slice_path.is_file(): reasons.append("slice_file_missing")
    else:
        rows=read_tsv(slice_path)
        for n,row in enumerate(rows,2):
            if row.get("format")!=SLICE_FORMAT: raise ValueError(f"{slice_path}:{n}: expected {SLICE_FORMAT}")
        nodes={dec(r.get("node_id")):r for r in rows if dec(r.get("call_index"))==TARGET_CALL and r.get("row_type")=="node" and dec(r.get("node_id"))>=0}
        root=nodes.get(0)
        if root is None or root.get("defining_opcode")!="LOAD": reasons.append("slice_receiver_root_not_load")
        else:
            children=children_for(nodes); addr=children.get(0,[])
            if len(addr)!=1: reasons.append("load_address_child_not_unique")
            else:
                address_root_opcode=nodes[addr[0]].get("defining_opcode"); affine=fold_affine(addr[0],nodes,children,root_token,{},set())
                if affine is None: reasons.append("parameter_relative_address_not_foldable")
                else:
                    base,field_offset=affine
                    if base!=root_token: reasons.append("parameter_relative_base_mismatch")
                    if field_offset is None or field_offset<0 or field_offset>0x10000: reasons.append("parameter_relative_offset_invalid")

    class_rows=read_tsv(a.function_classification_tsv)
    for n,row in enumerate(class_rows,2):
        if row.get("format")!=CLASS_FORMAT: raise ValueError(f"{a.function_classification_tsv}:{n}: expected {CLASS_FORMAT}")
    exact=[r for r in class_rows if (r.get("address") or "").lower()=="002a647e"]; identity=exact[0] if exact else {}
    if len(exact)!=1: reasons.append("ghidra_function_identity_not_unique")
    if (identity.get("function_at_entry") or "").lower()!="002a647e": reasons.append("ghidra_function_entry_mismatch")
    ghidra_name=identity.get("function_at_name") if identity else None; ghidra_owner=owner_from_qualified(ghidra_name)
    if ghidra_owner is None: reasons.append("ghidra_function_owner_unresolved")

    elf_entry=source_entry(TARGET_SOURCE)-GHIDRA_TO_ELF_DELTA; symbol_rows=[]
    for row in read_tsv(a.symbol_tsv):
        try: off=int(row.get("code_offset_hex") or "-1",0)
        except ValueError: continue
        if off==elf_entry: symbol_rows.append(row)
    native_demangled=sorted({r.get("demangled") for r in symbol_rows if r.get("demangled")}); native_owners=sorted({x for x in (owner_from_qualified(d) for d in native_demangled) if x})
    if native_owners and ghidra_owner and native_owners != [ghidra_owner]: reasons.append("native_ghidra_owner_conflict")

    catalog=read_jsonl(a.catalog_jsonl)
    for n,row in enumerate(catalog,1):
        if row.get("format")!=CAT_FORMAT: raise ValueError(f"{a.catalog_jsonl}:{n}: expected {CAT_FORMAT}")
    owner_vtables=sorted({r.get("receiver_vtable_id") for r in catalog if r.get("class_name")==ghidra_owner and isinstance(r.get("receiver_vtable_id"),str)})
    status="parameter_relative_field_evidence_incomplete" if reasons else "parameter_relative_field_with_function_owner_evidence"
    detail={"format":OUT_FORMAT,"source_id":TARGET_SOURCE,"call_index":TARGET_CALL,"call_address":failure.get("call_address"),"dispatch_slot_offset":TARGET_SLOT,"structural_status":pr.get("status"),"parameter_index":dec(pr.get("parameter_index")) if pr else -1,"parameter_root_token":root_token or None,"address_root_opcode":address_root_opcode,"field_offset":field_offset,"ghidra_function_entry":identity.get("function_at_entry"),"ghidra_function_name":ghidra_name,"ghidra_owner_class":ghidra_owner,"elf_function_entry":f"{elf_entry:08x}","native_demangled_names":native_demangled,"native_owner_classes":native_owners,"owner_vtable_ids":owner_vtables,"status":status,"reasons":reasons,"affects_routing":False}
    print("format:",OUT_FORMAT); print("source:",TARGET_SOURCE,"call",TARGET_CALL,"slot=",TARGET_SLOT); print("structural status:",detail["structural_status"]); print("parameter index:",detail["parameter_index"]); print("parameter root token:",detail["parameter_root_token"]); print("address root opcode:",address_root_opcode); print("field offset:","-" if field_offset is None else f"0x{field_offset:x}"); print("Ghidra function entry:",detail["ghidra_function_entry"]); print("Ghidra function name:",ghidra_name); print("Ghidra owner class:",ghidra_owner); print("ELF function entry:",f"0x{elf_entry:08x}"); print("native demangled names:",native_demangled); print("native owner classes:",native_owners); print("owner vtables:",owner_vtables); print("status:",status); print("reasons:",reasons); print("affects routing: no")
    if a.details_output:
        a.details_output.parent.mkdir(parents=True,exist_ok=True); a.details_output.write_text(json.dumps(detail,sort_keys=True)+"\n",encoding="utf-8"); print("details output:",a.details_output)
    return 0

if __name__=="__main__": raise SystemExit(main())
