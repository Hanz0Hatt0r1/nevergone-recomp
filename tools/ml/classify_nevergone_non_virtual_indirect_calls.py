#!/usr/bin/env python3
"""Reclassify validated non-virtual CALLIND rows after static vtable inference.

This is a post-inference evidence layer. It only changes rows explicitly listed in
NEVERGONE.NonVirtualIndirectCallProvenance/1 and requires the raw inference row to
still be missing_slot_offset with a matching call address. It emits no vtable query
and never changes routing.
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
PROVENANCE_FORMAT = "NEVERGONE.NonVirtualIndirectCallProvenance/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            rows.append(row)
    return rows


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("provenance_tsv", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    inference = read_jsonl(args.inference_jsonl)
    provenance_rows = read_tsv(args.provenance_tsv)

    by_key: dict[tuple[str, int], dict[str, str]] = {}
    for line_no, row in enumerate(provenance_rows, 2):
        if row.get("format") != PROVENANCE_FORMAT:
            raise ValueError(f"{args.provenance_tsv}:{line_no}: expected {PROVENANCE_FORMAT}")
        source_id = row.get("source_id")
        try:
            call_index = int(row.get("call_index", "-1"))
        except ValueError as error:
            raise ValueError(f"{args.provenance_tsv}:{line_no}: invalid call_index") from error
        if not source_id or call_index < 0:
            raise ValueError(f"{args.provenance_tsv}:{line_no}: invalid provenance key")
        if row.get("affects_routing") not in {"0", "no", "false", "False"}:
            raise ValueError(f"{args.provenance_tsv}:{line_no}: provenance must not affect routing")
        if row.get("evidence_kind") != "arm_pc_relative_linker_veneer":
            raise ValueError(f"{args.provenance_tsv}:{line_no}: unsupported evidence kind")
        if row.get("target_block") != ".plt" or row.get("target_is_thunk") != "1":
            raise ValueError(f"{args.provenance_tsv}:{line_no}: target must be validated PLT thunk")
        key = (source_id, call_index)
        if key in by_key:
            raise ValueError(f"duplicate provenance row: {source_id}#{call_index}")
        by_key[key] = row

    seen: set[tuple[str, int]] = set()
    output: list[dict[str, Any]] = []
    reclassified = 0

    for original in inference:
        if original.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"expected {INFERENCE_FORMAT}")
        row = dict(original)
        source_id = row.get("source_id")
        call_index = row.get("call_index")
        key = (source_id, call_index) if isinstance(source_id, str) and isinstance(call_index, int) else None
        evidence = by_key.get(key) if key is not None else None
        if evidence is not None:
            if row.get("status") != "missing_slot_offset":
                raise ValueError(
                    f"{source_id}#{call_index}: non-virtual evidence expected missing_slot_offset, "
                    f"got {row.get('status')!r}"
                )
            if row.get("dispatch_slot_offset") is not None:
                raise ValueError(f"{source_id}#{call_index}: expected no dispatch slot")
            call_address = str(row.get("call_address") or "").lower().removeprefix("0x")
            evidence_address = str(evidence.get("call_address") or "").lower().removeprefix("0x")
            if call_address != evidence_address:
                raise ValueError(
                    f"{source_id}#{call_index}: call address mismatch {call_address!r} != {evidence_address!r}"
                )
            row["status"] = "non_virtual_indirect_call"
            row["non_virtual_provenance_present"] = True
            row["non_virtual_evidence_kind"] = evidence.get("evidence_kind")
            row["non_virtual_veneer_address"] = evidence.get("veneer_address")
            row["non_virtual_veneer_literal_address"] = evidence.get("veneer_literal_address")
            row["non_virtual_veneer_literal_u32"] = evidence.get("veneer_literal_u32")
            row["non_virtual_pc_base"] = evidence.get("pc_base")
            row["non_virtual_fixed_target"] = evidence.get("fixed_target")
            row["non_virtual_target_block"] = evidence.get("target_block")
            row["non_virtual_target_symbol"] = evidence.get("target_symbol")
            row["non_virtual_target_is_thunk"] = True
            row["non_virtual_target_thunk_target"] = evidence.get("target_thunk_target")
            row["target_transfer_scope"] = "non_virtual_indirect_call_no_vtable_transfer"
            seen.add(key)
            reclassified += 1
        else:
            row.setdefault("non_virtual_provenance_present", False)
        output.append(row)

    missing = sorted(set(by_key) - seen)
    if missing:
        raise ValueError(f"provenance rows not matched in inference: {missing}")

    write_jsonl(args.output, output)
    print("format: NEVERGONE.NonVirtualIndirectCallClassification/1")
    print("inference rows:", len(inference))
    print("provenance rows:", len(provenance_rows))
    print("reclassified non-virtual indirect calls:", reclassified)
    print("remaining missing slot offset:", sum(1 for row in output if row.get("status") == "missing_slot_offset"))
    print("affects routing: no")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
