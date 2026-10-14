#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

PROVENANCE_FORMAT = "NEVERGONE.GhidraFunctionReturnObjectProvenance/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
AUDIT_FORMAT = "NEVERGONE.CallRootFactoryReturnAudit/1"


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


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


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def dec(value: Any, default: int = -1) -> int:
    if value in (None, "", "-"):
        return default
    return int(str(value), 10)


def parse_hex(value: Any) -> int | None:
    if value in (None, "", "-"):
        return None
    text = str(value).strip().lower()
    try:
        return int(text, 16) if not text.startswith("0x") else int(text, 0)
    except ValueError:
        return None


def constructor_class(name: str | None) -> str | None:
    if not name or name == "-":
        return None
    base = name.split("(", 1)[0].strip()
    parts = base.split("::")
    if len(parts) < 2 or parts[-1] != parts[-2]:
        return None
    return "::".join(parts[:-1])


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit CALL-root factory return constructor provenance")
    parser.add_argument("provenance_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    catalog_rows = read_jsonl(args.catalog_jsonl)
    class_vtables: dict[str, set[str]] = defaultdict(set)
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
        class_name = row.get("class_name")
        vtable = row.get("receiver_vtable_id")
        if isinstance(class_name, str) and class_name and isinstance(vtable, str) and vtable:
            class_vtables[class_name].add(vtable)

    rows = read_tsv(args.provenance_tsv)
    grouped: dict[tuple[str, int], list[dict[str, str]]] = defaultdict(list)
    for line_no, row in enumerate(rows, 2):
        if row.get("format") != PROVENANCE_FORMAT:
            raise ValueError(f"{args.provenance_tsv}:{line_no}: expected {PROVENANCE_FORMAT}")
        source_id = row.get("source_id") or ""
        call_index = dec(row.get("source_call_index"), -1)
        if source_id and call_index >= 0:
            grouped[(source_id, call_index)].append(row)

    details: list[dict[str, Any]] = []
    bucket_counts: Counter[str] = Counter()
    total_returns = 0
    total_null = 0
    total_nonnull = 0
    total_proven = 0

    for key in sorted(grouped):
        call_rows = grouped[key]
        summary = next((r for r in call_rows if r.get("row_type") == "summary"), None)
        calls = [r for r in call_rows if r.get("row_type") == "call"]
        return_nodes = [r for r in call_rows if r.get("row_type") == "return_node"]

        constructor_by_arg: dict[str, set[str]] = defaultdict(set)
        constructor_rows: list[dict[str, Any]] = []
        for row in calls:
            cls = constructor_class(row.get("call_target_name"))
            token = row.get("call_arg0_root_token") or "-"
            if cls is None or token == "-" or cls not in class_vtables:
                continue
            constructor_by_arg[token].add(cls)
            constructor_rows.append({
                "call_address": row.get("defining_address"),
                "target_entry": row.get("call_target_entry"),
                "target_name": row.get("call_target_name"),
                "arg0_root_token": token,
                "class_name": cls,
            })

        children: dict[tuple[int, int], int] = Counter()
        for row in return_nodes:
            ret = dec(row.get("return_index"), -1)
            parent = dec(row.get("parent_node_id"), -1)
            if ret >= 0 and parent >= 0:
                children[(ret, parent)] += 1

        leaves: list[dict[str, Any]] = []
        candidate_classes: set[str] = set()
        blocked_nonnull = 0
        return_indices = sorted({dec(r.get("return_index"), -1) for r in return_nodes if dec(r.get("return_index"), -1) >= 0})
        total_returns += len(return_indices)

        for row in return_nodes:
            ret = dec(row.get("return_index"), -1)
            node_id = dec(row.get("node_id"), -1)
            if ret < 0 or node_id < 0 or children.get((ret, node_id), 0) != 0:
                continue
            token = row.get("token") or "-"
            constant = parse_hex(row.get("constant_value"))
            is_null = row.get("constant_value") not in (None, "", "-") and constant == 0
            if is_null:
                total_null += 1
                leaves.append({
                    "return_index": ret,
                    "node_id": node_id,
                    "token": token,
                    "kind": "null",
                    "classes": [],
                })
                continue

            total_nonnull += 1
            classes = sorted(constructor_by_arg.get(token, set()))
            if len(classes) == 1:
                total_proven += 1
                candidate_classes.add(classes[0])
                kind = "constructor_proven"
            else:
                blocked_nonnull += 1
                kind = "unproven_nonnull" if not classes else "ambiguous_constructor_class"
            leaves.append({
                "return_index": ret,
                "node_id": node_id,
                "token": token,
                "defining_opcode": row.get("defining_opcode"),
                "call_target_name": row.get("call_target_name"),
                "kind": kind,
                "classes": classes,
            })

        if summary is None:
            bucket = "missing_summary"
        elif not return_indices:
            bucket = "no_return_values"
        elif total_nonnull == 0 and not any(x["kind"] != "null" for x in leaves):
            bucket = "null_only_factory"
        elif blocked_nonnull:
            bucket = "nonnull_return_not_constructor_proven"
        elif len(candidate_classes) != 1:
            bucket = "constructor_class_not_unique"
        else:
            chosen = next(iter(candidate_classes))
            vtables = sorted(class_vtables.get(chosen, set()))
            bucket = "unique_factory_return_class" if len(vtables) == 1 else "factory_class_vtable_not_unique"

        bucket_counts[bucket] += 1
        chosen_class = next(iter(candidate_classes)) if len(candidate_classes) == 1 else None
        vtables = sorted(class_vtables.get(chosen_class, set())) if chosen_class else []
        detail = {
            "format": AUDIT_FORMAT,
            "source_id": key[0],
            "call_index": key[1],
            "callee_entry": summary.get("callee_entry") if summary else None,
            "callee_name": summary.get("callee_name") if summary else None,
            "callee_declared_return_type": summary.get("callee_declared_return_type") if summary else None,
            "return_indices": return_indices,
            "constructor_calls": constructor_rows,
            "return_leaves": leaves,
            "candidate_receiver_class_names": sorted(candidate_classes),
            "candidate_receiver_vtable_ids": vtables,
            "status": bucket,
            "affects_routing": False,
        }
        details.append(detail)

    if args.details_output is not None:
        write_jsonl(args.details_output, details)

    print("format:", AUDIT_FORMAT)
    print("factory candidates:", len(details))
    print("return statements:", total_returns)
    print("null return leaves:", total_null)
    print("non-null return leaves:", total_nonnull)
    print("constructor-proven non-null leaves:", total_proven)
    print("bucket counts:", dict(bucket_counts))
    print("\nrows:")
    for row in details:
        print(
            row["source_id"], "call", row["call_index"],
            "callee=", row["callee_entry"], row["callee_name"],
            "declared_return=", row["callee_declared_return_type"],
            "classes=", row["candidate_receiver_class_names"],
            "vtables=", row["candidate_receiver_vtable_ids"],
            "status=", row["status"],
        )
        for leaf in row["return_leaves"]:
            print("  leaf", leaf)
    if args.details_output is not None:
        print("details output:", args.details_output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
