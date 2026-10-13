#!/usr/bin/env python3
"""Summarize static entry/caller evidence for exact dispatch-equivalence functions.

This tool does not call native code and does not declare any function safe to call.
It joins existing exact ordered dispatch groups with the Ghidra function/call indexes
and emits objective inspection evidence: signature, size proxy, fan-in/out and direct
caller contexts. A deterministic per-group static rank is provided only to choose
which function to inspect first; it is not an ABI-safety verdict.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import json
from collections import defaultdict
from pathlib import Path
from typing import Any, Iterable, TextIO

FORMAT = "NEVERGONE.EntryProbeFeasibility/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            rows.append(row)
    return rows


def write_jsonl(path: Path, rows: Iterable[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def open_tsv(path: Path) -> TextIO:
    if path.suffix == ".gz":
        return gzip.open(path, "rt", encoding="utf-8", errors="replace", newline="")
    return path.open("r", encoding="utf-8", errors="replace", newline="")


def normalize_entry(value: Any) -> str:
    text = str(value or "").strip().lower()
    if text.startswith("0x"):
        text = text[2:]
    return text


def parse_int(value: Any, default: int = 0) -> int:
    try:
        return int(str(value))
    except (TypeError, ValueError):
        return default


def parameter_count_proxy(signature: str) -> int | None:
    """Count top-level comma-separated parameters in a Ghidra signature."""
    left = signature.find("(")
    right = signature.rfind(")")
    if left < 0 or right <= left:
        return None
    body = signature[left + 1:right].strip()
    if not body or body == "void":
        return 0

    depth = 0
    count = 1
    pairs = {"<": ">", "(": ")", "[": "]"}
    opens = set(pairs)
    closes = set(pairs.values())
    for ch in body:
        if ch in opens:
            depth += 1
        elif ch in closes and depth > 0:
            depth -= 1
        elif ch == "," and depth == 0:
            count += 1
    return count


def load_functions(path: Path) -> dict[str, dict[str, Any]]:
    out: dict[str, dict[str, Any]] = {}
    with open_tsv(path) as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        for row in reader:
            entry = normalize_entry(row.get("entry"))
            if not entry:
                continue
            signature = str(row.get("signature") or "")
            out[entry] = {
                "entry": entry,
                "name": str(row.get("name") or ""),
                "address_count": parse_int(row.get("address_count")),
                "is_external": str(row.get("is_external") or "").lower() in {"1", "true", "yes"},
                "signature": signature,
                "parameter_count_proxy": parameter_count_proxy(signature),
            }
    return out


def load_calls(path: Path) -> tuple[
    dict[str, set[str]],
    dict[str, set[str]],
    dict[tuple[str, str], list[str]],
]:
    incoming: dict[str, set[str]] = defaultdict(set)
    outgoing: dict[str, set[str]] = defaultdict(set)
    sites: dict[tuple[str, str], list[str]] = defaultdict(list)
    with open_tsv(path) as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        for row in reader:
            caller = normalize_entry(row.get("caller_entry"))
            callee = normalize_entry(row.get("callee_entry"))
            site = normalize_entry(row.get("call_site"))
            if not caller or not callee:
                continue
            outgoing[caller].add(callee)
            incoming[callee].add(caller)
            if site:
                sites[(caller, callee)].append(site)
    return incoming, outgoing, sites


def source_entry(record_id: str) -> str:
    if ":" not in record_id:
        raise ValueError(f"record id has no program prefix: {record_id}")
    return normalize_entry(record_id.rsplit(":", 1)[1])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("functions_tsv", type=Path)
    parser.add_argument("calls_tsv", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-callers", type=int, default=32)
    args = parser.parse_args()
    if args.max_callers < 1:
        raise SystemExit("--max-callers must be >= 1")

    analysis = read_jsonl(args.analysis)
    functions = load_functions(args.functions_tsv)
    incoming, outgoing, call_sites = load_calls(args.calls_tsv)

    selected: list[dict[str, Any]] = []
    groups: dict[str, list[dict[str, Any]]] = defaultdict(list)

    for row in analysis:
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            continue
        eq = row.get("dispatch_equivalence_evidence")
        if not isinstance(eq, dict) or not eq.get("present"):
            continue
        group_id = eq.get("ordered_sequence_group_id")
        group_size = eq.get("ordered_sequence_group_size")
        if not isinstance(group_id, str) or not isinstance(group_size, int) or group_size <= 1:
            continue

        entry = source_entry(record_id)
        fn = functions.get(entry)
        if fn is None:
            raise ValueError(f"function index missing selected entry {entry} ({record_id})")

        callers = sorted(incoming.get(entry, set()))
        caller_rows: list[dict[str, Any]] = []
        for caller in callers[: args.max_callers]:
            caller_fn = functions.get(caller, {
                "entry": caller,
                "name": "",
                "address_count": 0,
                "is_external": False,
                "signature": "",
                "parameter_count_proxy": None,
            })
            caller_rows.append({
                **caller_fn,
                "call_sites": sorted(call_sites.get((caller, entry), [])),
                "fan_in": len(incoming.get(caller, set())),
                "fan_out": len(outgoing.get(caller, set())),
            })

        family = row.get("dispatch_family_evidence")
        dispatch_call_count = 0
        if isinstance(family, dict) and isinstance(family.get("memberships"), list):
            dispatch_call_count = len(family["memberships"])

        record = {
            "format": FORMAT,
            "id": record_id,
            "entry": entry,
            "ordered_group_id": group_id,
            "ordered_group_size": group_size,
            "signature": fn["signature"],
            "parameter_count_proxy": fn["parameter_count_proxy"],
            "address_count": fn["address_count"],
            "is_external": fn["is_external"],
            "fan_in": len(callers),
            "fan_out": len(outgoing.get(entry, set())),
            "dispatch_call_count": dispatch_call_count,
            "direct_callers_total": len(callers),
            "direct_callers_truncated": len(callers) > args.max_callers,
            "direct_callers": caller_rows,
            "static_rank_basis": [
                "parameter_count_proxy",
                "address_count",
                "fan_out",
                "fan_in",
                "entry",
            ],
            "safety_status": "inspection_required",
        }
        selected.append(record)
        groups[group_id].append(record)

    for group_id, members in groups.items():
        def key(item: dict[str, Any]) -> tuple[Any, ...]:
            params = item["parameter_count_proxy"]
            return (
                999 if params is None else params,
                item["address_count"],
                item["fan_out"],
                item["fan_in"],
                item["entry"],
            )

        ordered = sorted(members, key=key)
        for rank, item in enumerate(ordered, 1):
            item["static_inspection_rank"] = rank
            item["group_static_inspection_candidate"] = rank == 1

    selected.sort(key=lambda item: (item["ordered_group_id"], item["static_inspection_rank"], item["entry"]))
    write_jsonl(args.output, selected)

    print("format:", FORMAT)
    print("analysis functions:", len(analysis))
    print("exact-equivalence groups:", len(groups))
    print("selected functions:", len(selected))
    print("indexed functions:", len(functions))
    print("resolved call edges:", sum(len(v) for v in call_sites.values()))
    print("output:", args.output)
    for group_id in sorted(groups):
        candidate = min(groups[group_id], key=lambda item: item["static_inspection_rank"])
        print(
            "group candidate:", group_id,
            candidate["id"],
            "params=", candidate["parameter_count_proxy"],
            "addresses=", candidate["address_count"],
            "fan_in=", candidate["fan_in"],
            "fan_out=", candidate["fan_out"],
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
