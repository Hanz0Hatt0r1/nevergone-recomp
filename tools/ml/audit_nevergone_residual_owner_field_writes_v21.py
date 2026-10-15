#!/usr/bin/env python3
"""Audit exact parameter-0 field writes for the remaining receiver owners.

Evidence-only. This stage summarizes how the target fields are assigned across
all Ghidra functions owned by BeatenEffect and CCJoyStick. It does not infer a
receiver class or emit routing queries.
"""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path
from typing import Any

WRITE_FORMAT = "NEVERGONE.GhidraClassParameterFieldWrite/1"
OUT_FORMAT = "NEVERGONE.ResidualOwnerFieldWriteAudit/1"

TARGETS = {
    "BeatenEffect": [0x128, 0x12C, 0x130, 0x134, 0x138, 0x14C, 0x150, 0x154, 0x158],
    "CCJoyStick": [0x14C],
}


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def parse_num(value: Any) -> int | None:
    if value in (None, "", "-"):
        return None
    s = str(value).strip()
    try:
        return int(s, 0)
    except ValueError:
        return None


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, sort_keys=True) + "\n")


def classify_root(row: dict[str, str]) -> str:
    tok = row.get("value_root_token") or "-"
    op = row.get("value_root_opcode") or "-"
    if op == "CALL" and row.get("factory_target_entry") not in {None, "", "-"}:
        return "direct_call_return"
    if parse_num(row.get("value_root_parameter_index")) is not None:
        return "function_parameter"
    if tok.startswith("const:0x"):
        try:
            value = int(tok.split(":", 2)[1], 16)
        except Exception:
            value = None
        return "null_constant" if value == 0 else "constant"
    if op == "LOAD":
        return "loaded_value"
    if op in {"MULTIEQUAL", "PIECE", "SUBPIECE"}:
        return "merged_or_composed_value"
    return "other"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("beateneffect_writes_tsv", type=Path)
    p.add_argument("ccjoystick_writes_tsv", type=Path)
    p.add_argument("--details-output", type=Path)
    p.add_argument("--require-observed-targets", action="store_true")
    a = p.parse_args()

    rows: list[dict[str, str]] = []
    for path, owner in ((a.beateneffect_writes_tsv, "BeatenEffect"), (a.ccjoystick_writes_tsv, "CCJoyStick")):
        part = read_tsv(path)
        for n, row in enumerate(part, 2):
            if row.get("format") != WRITE_FORMAT:
                raise ValueError(f"{path}:{n}: expected {WRITE_FORMAT}")
            if row.get("class_name") != owner:
                raise ValueError(f"{path}:{n}: class mismatch: {row.get('class_name')} != {owner}")
            if parse_num(row.get("base_parameter_index")) != 0:
                raise ValueError(f"{path}:{n}: exporter emitted non-parameter0 write")
        rows.extend(part)

    details: list[dict[str, Any]] = []
    counts = Counter()
    for owner, fields in TARGETS.items():
        owner_rows = [r for r in rows if r.get("class_name") == owner]
        for field in fields:
            matches = [r for r in owner_rows if parse_num(r.get("address_offset")) == field]
            root_kinds = Counter(classify_root(r) for r in matches)
            factories = sorted({
                (r.get("factory_target_entry"), r.get("factory_target_name"))
                for r in matches
                if r.get("factory_target_entry") not in {None, "", "-"}
            })
            parameter_sources = sorted({
                (parse_num(r.get("value_root_parameter_index")), r.get("value_root_parameter_type_display"))
                for r in matches
                if parse_num(r.get("value_root_parameter_index")) is not None
            })
            root_types = sorted({r.get("value_root_type_display") for r in matches if r.get("value_root_type_display") not in {None, "", "-"}})
            writers = sorted({(r.get("function_entry"), r.get("function_name")) for r in matches})
            status = "exact_field_writes_observed" if matches else "field_write_not_observed"
            if matches:
                counts["observed"] += 1
            else:
                counts["missing"] += 1
            details.append({
                "format": OUT_FORMAT,
                "owner_class": owner,
                "field_offset": f"0x{field:x}",
                "write_count": len(matches),
                "writer_functions": [{"entry": e, "name": n} for e, n in writers],
                "root_kind_counts": dict(sorted(root_kinds.items())),
                "factory_targets": [{"entry": e, "name": n} for e, n in factories],
                "parameter_sources": [{"index": i, "type_display": t} for i, t in parameter_sources],
                "value_root_type_displays": root_types,
                "status": status,
                "receiver_class_proven": False,
                "affects_routing": False,
            })

    if a.require_observed_targets and counts["missing"]:
        missing=[f"{r['owner_class']}+{r['field_offset']}" for r in details if r['status']=='field_write_not_observed']
        raise SystemExit("target fields without exact parameter0 writes: " + ", ".join(missing))

    print("format:", OUT_FORMAT)
    print("target fields:", len(details))
    print("observed fields:", counts["observed"])
    print("missing fields:", counts["missing"])
    for row in details:
        print(
            row["owner_class"], row["field_offset"],
            "writes=", row["write_count"],
            "roots=", row["root_kind_counts"],
            "factories=", [(x["entry"], x["name"]) for x in row["factory_targets"]],
            "params=", [(x["index"], x["type_display"]) for x in row["parameter_sources"]],
            "types=", row["value_root_type_displays"],
            "status=", row["status"],
        )
    if a.details_output:
        write_jsonl(a.details_output, details)
        print("details output:", a.details_output)
    print("receiver class proven: no")
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
