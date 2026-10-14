#!/usr/bin/env python3
"""Validate structural virtual-slot offsets against known CALLIND slots.

Evidence-only. For canonical CALLIND(LOAD(LOAD(receiver)+slot)) sites, compare the
slot decoded by ExportIndirectCallTargetReceiverProvenance.java with the existing
analysis/inference slot. Missing slots are reported as candidates only after the
same decoder has matched all comparable known slots. Nothing here changes routing.
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
PROVENANCE_FORMAT = "NEVERGONE.GhidraTargetReceiverProvenance/1"
AUDIT_FORMAT = "NEVERGONE.StructuralSlotRecoveryAudit/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            if row.get("format") != INFERENCE_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {INFERENCE_FORMAT}")
            rows.append(row)
    return rows


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    for line_no, row in enumerate(rows, 2):
        if row.get("format") != PROVENANCE_FORMAT:
            raise ValueError(f"{path}:{line_no}: expected {PROVENANCE_FORMAT}")
    return rows


def parse_slot(value: Any) -> int | None:
    if value in (None, "", "-"):
        return None
    try:
        parsed = int(str(value), 0)
    except ValueError:
        return None
    return parsed if parsed >= 0 else None


def slot_text(value: int | None) -> str | None:
    return None if value is None else f"0x{value:x}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("target_receiver_tsv", type=Path)
    parser.add_argument("--details-output", type=Path)
    parser.add_argument("--samples", type=int, default=20)
    args = parser.parse_args()

    inference_rows = read_jsonl(args.inference_jsonl)
    target_rows = read_tsv(args.target_receiver_tsv)

    target_by_key: dict[tuple[str, int], dict[str, str]] = {}
    for line_no, row in enumerate(target_rows, 2):
        source_id = row.get("source_id")
        try:
            call_index = int(row.get("call_index", "-1"))
        except ValueError:
            continue
        if not source_id or call_index < 0:
            continue
        key = (source_id, call_index)
        if key in target_by_key:
            raise ValueError(f"{args.target_receiver_tsv}:{line_no}: duplicate key {key}")
        target_by_key[key] = row

    known_slot_rows = 0
    missing_slot_rows = 0
    canonical_target_rows = 0
    known_canonical_rows = 0
    known_matches = 0
    known_mismatches = 0
    known_canonical_missing_target_offset = 0
    missing_canonical_recoveries = 0
    missing_canonical_unaligned = 0
    missing_without_canonical_recovery = 0
    joined: list[dict[str, Any]] = []

    for inf in inference_rows:
        source_id = inf.get("source_id")
        call_index = inf.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        key = (source_id, call_index)
        prov = target_by_key.get(key)
        existing = parse_slot(inf.get("dispatch_slot_offset"))
        canonical = bool(prov and prov.get("structural_status") == "canonical_receiver")
        recovered = parse_slot(prov.get("target_slot_offset")) if prov else None
        if canonical:
            canonical_target_rows += 1

        status: str
        if existing is not None:
            known_slot_rows += 1
            if canonical:
                known_canonical_rows += 1
                if recovered is None:
                    known_canonical_missing_target_offset += 1
                    status = "known_slot_canonical_missing_target_offset"
                elif recovered == existing:
                    known_matches += 1
                    status = "known_slot_match"
                else:
                    known_mismatches += 1
                    status = "known_slot_mismatch"
            else:
                status = "known_slot_noncanonical_target"
        else:
            missing_slot_rows += 1
            if canonical and recovered is not None and recovered % 4 == 0:
                missing_canonical_recoveries += 1
                status = "missing_slot_canonical_recovery"
            elif canonical and recovered is not None:
                missing_canonical_unaligned += 1
                status = "missing_slot_canonical_unaligned"
            else:
                missing_without_canonical_recovery += 1
                status = "missing_slot_no_canonical_recovery"

        joined.append({
            "format": AUDIT_FORMAT,
            "source_id": source_id,
            "call_index": call_index,
            "call_address": inf.get("call_address"),
            "inference_status": inf.get("status"),
            "existing_dispatch_slot_offset": slot_text(existing),
            "structural_status": prov.get("structural_status") if prov else None,
            "target_pattern": prov.get("target_pattern") if prov else None,
            "recovered_target_slot_offset": slot_text(recovered),
            "recovered_slot_aligned": recovered is not None and recovered % 4 == 0,
            "audit_status": status,
            "affects_routing": False,
        })

    comparable_gate = (
        known_canonical_rows > 0
        and known_mismatches == 0
        and known_canonical_missing_target_offset == 0
    )
    production_candidates = [
        row for row in joined
        if row["audit_status"] == "missing_slot_canonical_recovery" and comparable_gate
    ]

    print("format:", AUDIT_FORMAT)
    print("inference rows:", len(inference_rows))
    print("target receiver rows:", len(target_rows))
    print("canonical target rows:", canonical_target_rows)
    print("known slot rows:", known_slot_rows)
    print("known slot rows with canonical target:", known_canonical_rows)
    print("known slot structural matches:", known_matches)
    print("known slot structural mismatches:", known_mismatches)
    print("known canonical rows missing target offset:", known_canonical_missing_target_offset)
    print("missing slot rows:", missing_slot_rows)
    print("missing slot canonical recoveries:", missing_canonical_recoveries)
    print("missing slot canonical unaligned:", missing_canonical_unaligned)
    print("missing slot without canonical recovery:", missing_without_canonical_recovery)
    print("known-slot validation gate:", "pass" if comparable_gate else "fail")
    print("production-safe missing-slot candidates:", len(production_candidates))

    print("\nmissing-slot candidates:")
    shown = 0
    for row in joined:
        if not row["audit_status"].startswith("missing_slot_"):
            continue
        if shown >= max(0, args.samples):
            break
        shown += 1
        print(
            row["source_id"],
            "call", row["call_index"],
            "inference_status=", row["inference_status"],
            "structural=", row["structural_status"],
            "target_pattern=", row["target_pattern"],
            "recovered_slot=", row["recovered_target_slot_offset"],
            "audit=", row["audit_status"],
        )

    if known_mismatches:
        print("\nknown-slot mismatches:")
        for row in joined:
            if row["audit_status"] != "known_slot_mismatch":
                continue
            print(
                row["source_id"],
                "call", row["call_index"],
                "existing=", row["existing_dispatch_slot_offset"],
                "recovered=", row["recovered_target_slot_offset"],
                "pattern=", row["target_pattern"],
            )

    if args.details_output is not None:
        args.details_output.parent.mkdir(parents=True, exist_ok=True)
        with args.details_output.open("w", encoding="utf-8") as handle:
            for row in joined:
                handle.write(json.dumps(row, sort_keys=True) + "\n")
        print("details output:", args.details_output)

    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
