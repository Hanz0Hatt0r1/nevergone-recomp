#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

TRIAGE_FORMAT = "NEVERGONE.ControlsFieldAliasTriage/1"
SOURCE_PLAN_FORMAT = "NEVERGONE.ControlsThisCallArgumentPlan/1"
OFFSET_FORMAT = "NEVERGONE.ControlsThisCallArgumentOffsetAudit/1"
EVIDENCE_FORMAT = "NEVERGONE.GhidraControlsFullThisCalleeParameterUses/1"
ELF_SHA = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"

def write_tsv(path: Path, fields: list[str], rows: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)

def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    planner = repo / "tools/ml/plan_nevergone_controls_full_this_callee_frontier.py"
    auditor = repo / "tools/ml/audit_nevergone_controls_full_this_callee_frontier.py"

    with tempfile.TemporaryDirectory() as tmp_s:
        tmp = Path(tmp_s)
        triage = tmp / "triage.jsonl"
        offset = tmp / "offset.jsonl"
        source = tmp / "source.tsv"
        plan = tmp / "frontier.tsv"
        evidence = tmp / "uses.tsv"
        details = tmp / "details.jsonl"

        triage.write_text(json.dumps({
            "format": TRIAGE_FORMAT, "record_type": "summary", "elf_sha256": ELF_SHA,
            "controls_methods_decompiled": 38, "this_bearing_call_count": 78,
        }) + "\n", encoding="utf-8")

        source_fields = [
            "format", "function_entry", "op_address", "op_time", "call_kind",
            "call_target_token", "callee_category", "callee_symbol", "argument_index",
        ]
        source_rows: list[dict[str, str]] = []
        full: list[dict[str, str]] = []
        nonzero: list[dict[str, str]] = []

        def add(category: str, count: int, *, offset_hex: str, call_kind: str = "CALL",
                arg: str = "0") -> None:
            for _ in range(count):
                idx = len(source_rows)
                func = f"{0x002a0000 + idx * 0x20:08x}"
                op = f"{0x002a0004 + idx * 0x20:08x}"
                target = (
                    f"address:{0x00500000 + idx * 0x10:08x}:8"
                    if call_kind == "CALL" else f"def:LOAD@{op}:{100 + idx}:4"
                )
                symbol = {
                    "controls_method_candidate": "_ZN8Controls4testEv",
                    "cocos_base_method_candidate": "_ZN7cocos2d6CCNode4testEv",
                    "controlsbutton_factory_callback_candidate": "_ZN14ControlsButton6createEv",
                    "callback_factory_candidate": "_ZN7cocos2d11CCCallFuncN6createEv",
                    "operator_delete_candidate": "_ZdlPv",
                    "ccpoint_subobject_candidate": "_ZN7cocos2d7CCPointC2Ev",
                    "indirect_unresolved": "-",
                }[category]
                row = {
                    "format": SOURCE_PLAN_FORMAT, "function_entry": func, "op_address": op,
                    "op_time": str(100 + idx), "call_kind": call_kind, "call_target_token": target,
                    "callee_category": category, "callee_symbol": symbol, "argument_index": arg,
                }
                source_rows.append(row)
                compact = {
                    "function_entry": func, "op_address": op, "callee_category": category,
                    "callee_symbol": symbol, "argument_index": arg, "address_offset": offset_hex,
                }
                (full if offset_hex == "0x0" else nonzero).append(compact)

        add("controls_method_candidate", 21, offset_hex="0x0")
        add("cocos_base_method_candidate", 6, offset_hex="0x0")
        add("controlsbutton_factory_callback_candidate", 3, offset_hex="0x0", arg="2")
        add("callback_factory_candidate", 1, offset_hex="0x0")
        add("operator_delete_candidate", 1, offset_hex="0x0")
        add("indirect_unresolved", 3, offset_hex="0x0", call_kind="CALLIND")
        add("ccpoint_subobject_candidate", 43, offset_hex="0x190")

        assert len(source_rows) == 78 and len(full) == 35 and len(nonzero) == 43
        write_tsv(source, source_fields, source_rows)
        offset.write_text(json.dumps({
            "format": OFFSET_FORMAT,
            "status": "bounded_78_argument_offsets_complete",
            "full_controls_this_call_count": 35,
            "nonzero_controls_relative_call_count": 43,
            "unresolved_offset_call_count": 0,
            "full_controls_this_calls": full,
            "nonzero_controls_relative_calls": nonzero,
            "unresolved_offset_calls": [],
        }) + "\n", encoding="utf-8")

        subprocess.run([
            sys.executable, str(planner), str(triage), str(offset), str(source),
            "--output", str(plan),
        ], check=True, capture_output=True, text=True)

        with plan.open(encoding="utf-8", newline="") as f:
            planned = list(csv.DictReader(f, delimiter="\t"))
        direct = [r for r in planned if r["site_kind"] == "direct_external"]
        indirect = [r for r in planned if r["site_kind"] == "indirect_external"]
        assert len(planned) == 14 and len(direct) == 11 and len(indirect) == 3

        fields = [
            "format", "row_type", "source_function_entry", "source_op_address",
            "callee_entry", "callee_name", "callee_parameter_index", "decompile_status",
            "parameter_instances", "direct_write_count", "retention_store_count",
            "forwarded_call_count", "return_escape_count", "dereference_read_count",
            "opaque_use_count", "use_depth", "use_opcode", "use_address",
            "use_input_index", "use_kind", "value_offset", "target_entry", "target_name",
            "target_argument_index", "destination_token", "value_token", "status",
        ]
        rows: list[dict[str, str]] = []
        for i, item in enumerate(direct):
            kinds: list[tuple[str, str, str]] = []
            if i == 0:
                kinds.append(("direct_field_write", "STORE", "0x20"))
            elif i == 1:
                kinds.append(("direct_forward_call", "CALL", "0x0"))
            elif i == 2:
                kinds.append(("retention_store", "STORE", "0x0"))
            elif i == 3:
                kinds.append(("dereference_read", "LOAD", "0x10"))
            elif i == 4:
                kinds.append(("opaque_use", "INT_AND", "0x0"))
            counts = {
                "direct_field_write": 0, "retention_store": 0, "forwarded_call": 0,
                "return_escape": 0, "dereference_read": 0, "opaque_use": 0,
            }
            for kind, _opcode, _off in kinds:
                if kind in {"direct_forward_call", "indirect_forward_call"}:
                    counts["forwarded_call"] += 1
                else:
                    counts[kind] += 1
            rows.append({
                "format": EVIDENCE_FORMAT, "row_type": "summary",
                "source_function_entry": item["source_function_entry"],
                "source_op_address": item["source_op_address"],
                "callee_entry": item["callee_entry"], "callee_name": item["callee_symbol"],
                "callee_parameter_index": item["callee_parameter_index"],
                "decompile_status": "ok", "parameter_instances": "1",
                "direct_write_count": str(counts["direct_field_write"]),
                "retention_store_count": str(counts["retention_store"]),
                "forwarded_call_count": str(counts["forwarded_call"]),
                "return_escape_count": str(counts["return_escape"]),
                "dereference_read_count": str(counts["dereference_read"]),
                "opaque_use_count": str(counts["opaque_use"]),
                "use_depth": "-", "use_opcode": "-", "use_address": "-", "use_input_index": "-",
                "use_kind": "-", "value_offset": "-", "target_entry": "-", "target_name": "-",
                "target_argument_index": "-", "destination_token": "-", "value_token": "-",
                "status": "complete_parameter_use_export",
            })
            for kind, opcode, off in kinds:
                rows.append({
                    "format": EVIDENCE_FORMAT, "row_type": "use",
                    "source_function_entry": item["source_function_entry"],
                    "source_op_address": item["source_op_address"],
                    "callee_entry": item["callee_entry"], "callee_name": item["callee_symbol"],
                    "callee_parameter_index": item["callee_parameter_index"],
                    "decompile_status": "-", "parameter_instances": "1",
                    "direct_write_count": "-", "retention_store_count": "-",
                    "forwarded_call_count": "-", "return_escape_count": "-",
                    "dereference_read_count": "-", "opaque_use_count": "-",
                    "use_depth": "0", "use_opcode": opcode, "use_address": "00600000",
                    "use_input_index": "1", "use_kind": kind, "value_offset": off,
                    "target_entry": "00610000" if kind == "direct_forward_call" else "-",
                    "target_name": "Synthetic::sink" if kind == "direct_forward_call" else "-",
                    "target_argument_index": "0" if kind == "direct_forward_call" else "-",
                    "destination_token": "address:00900000:4" if kind == "retention_store" else "-",
                    "value_token": "register:register:00000000:4", "status": "observed",
                })
        write_tsv(evidence, fields, rows)

        subprocess.run([
            sys.executable, str(auditor), str(plan), str(evidence),
            "--details-output", str(details), "--require-complete",
        ], check=True, capture_output=True, text=True)
        result = json.loads(details.read_text(encoding="utf-8"))
        assert result["status"] == "bounded_external_callee_frontier_complete"
        assert result["target_field_direct_writer_count"] == 0
        assert result["frontier_count"] == 6

        with evidence.open(encoding="utf-8", newline="") as f:
            bad = list(csv.DictReader(f, delimiter="\t"))
        for row in bad:
            if row["row_type"] == "use" and row["use_kind"] == "direct_field_write":
                row["value_offset"] = "0x12c"
                break
        write_tsv(evidence, fields, bad)
        subprocess.run([
            sys.executable, str(auditor), str(plan), str(evidence),
            "--details-output", str(details), "--require-complete",
        ], check=True, capture_output=True, text=True)
        result = json.loads(details.read_text(encoding="utf-8"))
        assert result["target_field_direct_writer_count"] == 1

    print("controls full-this callee frontier smoke: ok")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
