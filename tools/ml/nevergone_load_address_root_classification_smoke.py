#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "tools/ml/plan_nevergone_load_address_root_probe.py"
AUDIT = ROOT / "tools/ml/audit_nevergone_load_address_root_classification.py"
HEADER = "format\trow_type\tfunction_entry\tcall_index\tcall_address\tcall_time\trecovery_status\tslot_offset\tstructural_receiver_token\tnode_id\tparent_node_id\tinput_index\tdepth\tnode_token\ttype_display\tpointer_depth\tpointee_type_display\tconstant_value\tdefining_opcode\tdefining_address\tdefining_time\tcall_target_entry\tcall_target_name\tcall_target_return_type\tcall_target_is_thunk\tcall_target_thunk_target\ttruncation\tnode_count\ttruncated_by_budget\tdepth_truncation_count\tcycle_count\tmax_nodes\tmax_depth"
FMT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
CLASS_HEADER = "format\taddress\tblock\tblock_start\tblock_end\tread\twrite\texecute\tprimary_symbol\tfunction_at_entry\tfunction_at_name\tfunction_at_body\tfunction_at_is_thunk\tfunction_at_thunk_target\tfunction_containing_entry\tfunction_containing_name\tfunction_containing_body\tinstruction_mnemonic\tdata_type\traw_u32"
CFMT = "NEVERGONE.GhidraAddressClassification/1"


def slice_file(path: Path, entry: str, call: int, call_addr: str, root_addr: str) -> None:
    summary = [FMT,"summary",entry,str(call),call_addr,"1","canonical_receiver","0x8","def:LOAD@x:1:4","-1","-1","-1","-1","-","-","0","-","-","-","-","-1","-","-","-","-","-","-","4","0","0","0","96","16"]
    root = [FMT,"node",entry,str(call),call_addr,"1","canonical_receiver","0x8","def:LOAD@x:1:4","0","-1","-1","0","def:LOAD@x:1:4","int","0","-","-","LOAD",call_addr,"1","-","-","-","-","-","-","4","0","0","0","96","16"]
    expr = [FMT,"node",entry,str(call),call_addr,"1","canonical_receiver","0x8","def:LOAD@x:1:4","1","0","1","1","def:INT_ADD@x:2:4","int *","1","int","-","INT_ADD",call_addr,"2","-","-","-","-","-","-","4","0","0","0","96","16"]
    address_leaf = [FMT,"node",entry,str(call),call_addr,"1","canonical_receiver","0x8","def:LOAD@x:1:4","2","1","0","2",f"address:{root_addr}:4","int *","1","int","-","-","-","-1","-","-","-","-","-","-","4","0","0","0","96","16"]
    constant_leaf = [FMT,"node",entry,str(call),call_addr,"1","canonical_receiver","0x8","def:LOAD@x:1:4","3","1","1","2","const:0x0:4","int","0","-","0x0","-","-","-1","-","-","-","-","-","-","4","0","0","0","96","16"]
    path.write_text(
        HEADER+"\n"+"\t".join(summary)+"\n"+"\t".join(root)+"\n"+"\t".join(expr)+"\n"+"\t".join(address_leaf)+"\n"+"\t".join(constant_leaf)+"\n",
        encoding="utf-8",
    )


def class_row(address: str, block: str, execute: str, instruction: str, raw: str, symbol: str = "-") -> str:
    vals=[CFMT,address,block,"00001000","0000ffff","1","0",execute,symbol,"-","-","-","-","-","-","-","-",instruction,"-",raw]
    return "\t".join(vals)


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        d=Path(tmp); slices=d/"slices"; slices.mkdir()
        failures=d/"failures.jsonl"; plan=d/"plan.tsv"; roots=d/"roots.tsv"; targets=d/"targets.tsv"; details=d/"details.jsonl"
        rows=[
            {"source_id":"test.so:00100000","call_index":0,"call_address":"00100010","dispatch_slot_offset":"0x8","status":"receiver_not_single_pointer"},
            {"source_id":"test.so:00100100","call_index":0,"call_address":"00100110","dispatch_slot_offset":"0x8","status":"receiver_not_single_pointer"},
        ]
        failures.write_text("\n".join(json.dumps(x) for x in rows)+"\n",encoding="utf-8")
        slice_file(slices/"test.so_00100000.tsv","00100000",0,"00100010","00100020")
        slice_file(slices/"test.so_00100100.tsv","00100100",0,"00100110","00100120")
        out=subprocess.check_output(["python3",str(PLAN),str(failures),str(slices),"--output",str(plan)],text=True)
        assert "planned LOAD address roots: 2" in out, out
        assert "address_root= INT_ADD" in out, out
        roots.write_text(CLASS_HEADER+"\n"+class_row("00100020",".text","1","-","0x00009000")+"\n"+class_row("00100120",".text","1","ldr","0xe5900000")+"\n",encoding="utf-8")
        targets.write_text(CLASS_HEADER+"\n"+class_row("00009000",".got","0","-","0x0000a000","PTR_test")+"\n"+class_row("e5900000","-","-","-","-")+"\n",encoding="utf-8")
        audit=subprocess.check_output(["python3",str(AUDIT),str(plan),str(roots),str(targets),"--details-output",str(details)],text=True)
        assert "executable_block_literal_points_to_mapped_address" in audit, audit
        assert "root_is_instruction_not_literal" in audit, audit
        parsed=[json.loads(x) for x in details.read_text().splitlines() if x]
        assert len(parsed)==2
        assert {x["status"] for x in parsed}=={"executable_block_literal_points_to_mapped_address","root_is_instruction_not_literal"}
    print("LOAD address-root classification smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
