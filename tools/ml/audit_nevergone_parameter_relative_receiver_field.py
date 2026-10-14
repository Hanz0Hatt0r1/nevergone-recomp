#!/usr/bin/env python3
"""Audit the remaining non-quarantined missing receiver as LOAD(param0+offset).

Evidence-only. The audit consumes the current v12 failure row, the persisted
structural receiver parameter provenance and bounded receiver slice, the native
symbol index, and StaticVtableSlotCatalog/3. It symbolically folds only the LOAD
address subtree relative to the independently identified parameter-0 root.

No receiver class is inferred from a neighboring field or from the current
method name. Native symbol ownership and reverse vtable membership are reported
as independent context for the next field-write provenance step.
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
CAT_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
OUT_FORMAT = "NEVERGONE.ParameterRelativeReceiverFieldAudit/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    out=[]
    with path.open(encoding="utf-8") as f:
        for n,line in enumerate(f,1):
            if not line.strip():
                continue
            row=json.loads(line)
            if not isinstance(row,dict):
                raise ValueError(f"{path}:{n}: expected object")
            out.append(row)
    return out


def read_tsv(path: Path) -> list[dict[str,str]]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


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


def safe_id(source: str) -> str:
    return source.replace(":","_").replace("/","_")


def source_entry(source: str) -> int:
    return int(source.rsplit(":",1)[1],16)


def children_for(nodes: dict[int,dict[str,str]]) -> dict[int,list[int]]:
    out: dict[int,list[int]] = defaultdict(list)
    for nid,row in nodes.items():
        parent=dec(row.get("parent_node_id"))
        if parent >= 0: out[parent].append(nid)
    for parent in out:
        out[parent].sort(key=lambda nid:(dec(nodes[nid].get("input_index")),nid))
    return out

# Affine value is (base-token-or-None, signed offset).
def fold_affine(nid: int, nodes: dict[int,dict[str,str]], children: dict[int,list[int]],
                root_token: str, memo: dict[int,tuple[str|None,int]|None], active:set[int]) -> tuple[str|None,int]|None:
    if nid in memo: return memo[nid]
    if nid in active:
        memo[nid]=None; return None
    active.add(nid)
    row=nodes[nid]
    token=row.get("node_token") or "-"
    opcode=row.get("defining_opcode") or "-"
    const=num(row.get("constant_value"))
    kids=children.get(nid,[])

    def child(i:int):
        return fold_affine(kids[i],nodes,children,root_token,memo,active)

    value: tuple[str|None,int]|None = None
    if token == root_token:
        value=(root_token,0)
    elif const is not None:
        value=(None,const)
    elif opcode in {"COPY","CAST","INDIRECT"} and kids:
        value=child(0)
    elif opcode in {"INT_ADD","PTRSUB"} and len(kids)>=2:
        a,b=child(0),child(1)
        if a is not None and b is not None:
            if a[0] is None: value=(b[0],a[1]+b[1])
            elif b[0] is None: value=(a[0],a[1]+b[1])
    elif opcode == "INT_SUB" and len(kids)>=2:
        a,b=child(0),child(1)
        if a is not None and b is not None and b[0] is None:
            value=(a[0],a[1]-b[1])
    elif opcode == "PTRADD" and len(kids)>=3:
        base,index,scale=child(0),child(1),child(2)
        if base is not None and index is not None and scale is not None and index[0] is None and scale[0] is None:
            value=(base[0],base[1]+index[1]*scale[1])
    elif opcode == "MULTIEQUAL" and kids:
        vals=[child(i) for i in range(len(kids))]
        if vals and all(v is not None for v in vals) and len(set(vals))==1:
            value=vals[0]

    active.remove(nid)
    memo[nid]=value
    return value


def owner_from_demangled(name: str) -> str|None:
    if "::" not in name: return None
    return name.split("::",1)[0].strip() or None


def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("v12_failures_jsonl",type=Path)
    p.add_argument("structural_provenance_tsv",type=Path)
    p.add_argument("slice_dir",type=Path)
    p.add_argument("symbol_tsv",type=Path)
    p.add_argument("catalog_jsonl",type=Path)
    p.add_argument("--details-output",type=Path)
    a=p.parse_args()

    failures=read_jsonl(a.v12_failures_jsonl)
    matches=[r for r in failures if r.get("source_id")==TARGET_SOURCE and r.get("call_index")==TARGET_CALL]
    reasons=[]
    if len(matches)!=1:
        reasons.append("target_failure_row_not_unique")
        failure=matches[0] if matches else {}
    else: failure=matches[0]
    if failure.get("status")!="receiver_not_single_pointer": reasons.append("target_not_receiver_not_single_pointer")
    if norm_slot(failure.get("dispatch_slot_offset"))!=TARGET_SLOT: reasons.append("target_slot_mismatch")

    prov_rows=read_tsv(a.structural_provenance_tsv)
    for n,row in enumerate(prov_rows,2):
        if row.get("format")!=PROV_FORMAT: raise ValueError(f"{a.structural_provenance_tsv}:{n}: expected {PROV_FORMAT}")
    prov=[r for r in prov_rows if r.get("source_id")==TARGET_SOURCE and dec(r.get("call_index"))==TARGET_CALL]
    if len(prov)!=1:
        reasons.append("structural_provenance_not_unique")
        pr=prov[0] if prov else {}
    else: pr=prov[0]
    if pr.get("status")!="parameter_no_direct_caller_calls": reasons.append("unexpected_structural_status")
    if pr.get("structural_receiver_is_load")!="1": reasons.append("structural_receiver_not_load")
    if dec(pr.get("parameter_index"))!=0: reasons.append("load_address_root_not_parameter_zero")
    if dec(pr.get("matched_caller_calls"),0)!=0: reasons.append("unexpected_direct_caller_calls")
    root_token=pr.get("root_token") or ""
    if not root_token or root_token=="-": reasons.append("parameter_root_token_missing")

    slice_path=a.slice_dir/f"{safe_id(TARGET_SOURCE)}.tsv"
    field_offset=None
    address_root_opcode=None
    affine_base=None
    if not slice_path.is_file():
        reasons.append("slice_file_missing")
    else:
        rows=read_tsv(slice_path)
        for n,row in enumerate(rows,2):
            if row.get("format")!=SLICE_FORMAT: raise ValueError(f"{slice_path}:{n}: expected {SLICE_FORMAT}")
        call_rows=[r for r in rows if dec(r.get("call_index"))==TARGET_CALL]
        node_rows=[r for r in call_rows if r.get("row_type")=="node"]
        nodes={dec(r.get("node_id")):r for r in node_rows if dec(r.get("node_id"))>=0}
        root=nodes.get(0)
        if root is None or root.get("defining_opcode")!="LOAD":
            reasons.append("slice_receiver_root_not_load")
        else:
            children=children_for(nodes)
            addr=children.get(0,[])
            if len(addr)!=1:
                reasons.append("load_address_child_not_unique")
            else:
                address_root_opcode=nodes[addr[0]].get("defining_opcode")
                affine=fold_affine(addr[0],nodes,children,root_token,{},set())
                if affine is None:
                    reasons.append("parameter_relative_address_not_foldable")
                else:
                    affine_base,field_offset=affine
                    if affine_base!=root_token: reasons.append("parameter_relative_base_mismatch")
                    if field_offset is None or field_offset<0 or field_offset>0x10000: reasons.append("parameter_relative_offset_invalid")

    elf_entry=source_entry(TARGET_SOURCE)-GHIDRA_TO_ELF_DELTA
    symbol_rows=[]
    for row in read_tsv(a.symbol_tsv):
        try: off=int(row.get("code_offset_hex") or "-1",0)
        except ValueError: continue
        if off==elf_entry: symbol_rows.append(row)
    demangled=sorted({r.get("demangled") for r in symbol_rows if r.get("demangled")})
    mangled=sorted({r.get("name") for r in symbol_rows if r.get("name")})
    owners=sorted({x for x in (owner_from_demangled(d) for d in demangled) if x})
    if not symbol_rows: reasons.append("native_symbol_not_found")
    if len(owners)!=1: reasons.append("native_symbol_owner_not_unique")

    catalog=read_jsonl(a.catalog_jsonl)
    virtual_memberships=[]
    for n,row in enumerate(catalog,1):
        if row.get("format")!=CAT_FORMAT: raise ValueError(f"{a.catalog_jsonl}:{n}: expected {CAT_FORMAT}")
        targets=row.get("candidate_target_ids")
        if row.get("static_status")=="single_function_memory_pointer" and isinstance(targets,list) and targets==[TARGET_SOURCE]:
            virtual_memberships.append({
                "class_name":row.get("class_name"),
                "receiver_vtable_id":row.get("receiver_vtable_id"),
                "dispatch_slot_offset":norm_slot(row.get("dispatch_slot_offset")),
                "target_names":[x.get("name") for x in (row.get("target_functions") or []) if isinstance(x,dict) and x.get("name")],
                "runtime_validation_status":row.get("runtime_validation_status"),
            })
    membership_classes=sorted({x.get("class_name") for x in virtual_memberships if isinstance(x.get("class_name"),str)})
    owner_virtual_membership=bool(len(owners)==1 and owners[0] in membership_classes)

    if reasons:
        status="parameter_relative_field_evidence_incomplete"
    elif not owner_virtual_membership:
        status="parameter_relative_field_symbol_owner_only"
    else:
        status="parameter_relative_field_with_virtual_owner_evidence"

    detail={
        "format":OUT_FORMAT,
        "source_id":TARGET_SOURCE,
        "call_index":TARGET_CALL,
        "call_address":failure.get("call_address"),
        "dispatch_slot_offset":TARGET_SLOT,
        "structural_status":pr.get("status"),
        "parameter_index":dec(pr.get("parameter_index")) if pr else -1,
        "parameter_root_token":root_token or None,
        "address_root_opcode":address_root_opcode,
        "field_offset":field_offset,
        "ghidra_function_entry":f"{source_entry(TARGET_SOURCE):08x}",
        "elf_function_entry":f"{elf_entry:08x}",
        "native_mangled_names":mangled,
        "native_demangled_names":demangled,
        "native_owner_classes":owners,
        "virtual_memberships":virtual_memberships,
        "virtual_membership_classes":membership_classes,
        "owner_has_virtual_membership":owner_virtual_membership,
        "status":status,
        "reasons":reasons,
        "affects_routing":False,
    }

    print("format:",OUT_FORMAT)
    print("source:",TARGET_SOURCE,"call",TARGET_CALL,"slot=",TARGET_SLOT)
    print("structural status:",detail["structural_status"])
    print("parameter index:",detail["parameter_index"])
    print("parameter root token:",detail["parameter_root_token"])
    print("address root opcode:",address_root_opcode)
    print("field offset:","-" if field_offset is None else f"0x{field_offset:x}")
    print("ELF function entry:",f"0x{elf_entry:08x}")
    print("native demangled names:",demangled)
    print("native owner classes:",owners)
    print("virtual membership classes:",membership_classes)
    for item in virtual_memberships:
        print(" membership",item["class_name"],"vtable=",item["receiver_vtable_id"],"slot=",item["dispatch_slot_offset"],"runtime=",item["runtime_validation_status"])
    print("owner has virtual membership:",owner_virtual_membership)
    print("status:",status)
    print("reasons:",reasons)
    print("affects routing: no")

    if a.details_output:
        a.details_output.parent.mkdir(parents=True,exist_ok=True)
        a.details_output.write_text(json.dumps(detail,sort_keys=True)+"\n",encoding="utf-8")
        print("details output:",a.details_output)
    return 0

if __name__=="__main__":
    raise SystemExit(main())
