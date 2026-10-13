#!/usr/bin/env python3
"""Resolve runtime receiver-vtable IDs against bounded Ghidra metadata.

The tool is intentionally conservative. It never derives a class name from a
virtual target name alone. A class label is emitted only when the Ghidra symbol
index contains explicit vtable-like symbol evidence near the observed receiver
address point. For ARM32 Itanium ABI, an address point 8 bytes after the vtable
symbol is treated as the strongest layout match.

Simple Itanium ABI vtable symbols (for example ``_ZTV10ActionData`` and nested
source-name forms such as ``_ZTVN4game8DataNodeE``) are decoded locally without
calling an external demangler. Unsupported mangling constructs remain unresolved
rather than being guessed.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.VtableStaticProvenance/2"
ITANIUM_ARM32_ADDRESS_POINT_DELTA = 8
WINDOW = 0x20


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


def open_tsv(path: Path):
    return gzip.open(path, "rt", encoding="utf-8", errors="replace", newline="")


def addr(value: str | None) -> int | None:
    if not value or value == "-":
        return None
    value = value.lower().replace("0x", "")
    if not re.fullmatch(r"[0-9a-f]+", value):
        return None
    return int(value, 16)


def vtable_id_address(vtable_id: str) -> int:
    if ":" not in vtable_id:
        raise ValueError(f"unsupported receiver_vtable_id: {vtable_id}")
    module, value = vtable_id.rsplit(":", 1)
    if module != "libcocos2dcpp.so" or not re.fullmatch(r"[0-9a-fA-F]{1,16}", value):
        raise ValueError(f"unsupported receiver_vtable_id: {vtable_id}")
    return int(value, 16)


def _parse_itanium_source_names(payload: str, *, nested: bool) -> list[str] | None:
    """Decode only a bounded sequence of Itanium <source-name> components.

    A source-name is ``<decimal length><identifier>``. Nested names are wrapped
    in N...E. Substitutions, templates, operators, anonymous namespaces, ABI tags,
    and other richer mangling constructs are intentionally unsupported.
    """
    parts: list[str] = []
    pos = 0
    limit = len(payload)
    while pos < limit:
        if nested and payload[pos] == "E":
            pos += 1
            break
        start = pos
        while pos < limit and payload[pos].isdigit():
            pos += 1
        if pos == start:
            return None
        length = int(payload[start:pos])
        if length <= 0 or pos + length > limit:
            return None
        ident = payload[pos:pos + length]
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", ident):
            return None
        parts.append(ident)
        pos += length
        if not nested:
            # A non-nested Itanium name contains one unqualified source-name.
            break
    if not parts or pos != limit:
        return None
    return parts


def decode_itanium_vtable_class(name: str) -> str | None:
    """Decode conservative `_ZTV<type-name>` class names without guessing."""
    leaf = name.rsplit("::", 1)[-1]
    if not leaf.startswith("_ZTV"):
        return None
    payload = leaf[4:]
    if not payload:
        return None
    if payload.startswith("N"):
        parts = _parse_itanium_source_names(payload[1:], nested=True)
    else:
        parts = _parse_itanium_source_names(payload, nested=False)
    return "::".join(parts) if parts else None


def symbol_class_name(name: str) -> tuple[str | None, str | None]:
    low = name.lower()
    markers = ("vtable for ", "construction vtable for ")
    for marker in markers:
        pos = low.find(marker)
        if pos >= 0:
            return name[pos + len(marker):].strip(), "demangled_vtable_symbol"

    leaf = name.rsplit("::", 1)[-1]
    if leaf.startswith("_ZTV"):
        decoded = decode_itanium_vtable_class(leaf)
        if decoded is not None:
            return decoded, "itanium_mangled_vtable_symbol_decoded"
        return None, "itanium_mangled_vtable_symbol_unsupported"
    return None, None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receiver_keys", type=Path,
                        help="receiver key JSONL from analyze_nevergone_dispatch_receiver_provenance.py")
    parser.add_argument("symbols_tsv_gz", type=Path)
    parser.add_argument("xrefs_tsv_gz", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    key_rows = read_jsonl(args.receiver_keys)
    vtable_ids = sorted({
        row.get("receiver_vtable_id")
        for row in key_rows
        if isinstance(row.get("receiver_vtable_id"), str)
    })
    if not vtable_ids:
        raise ValueError("no receiver_vtable_id values found")

    addresses = {vtable_id: vtable_id_address(vtable_id) for vtable_id in vtable_ids}
    min_addr = min(addresses.values()) - WINDOW
    max_addr = max(addresses.values()) + WINDOW

    symbols: list[dict[str, Any]] = []
    with open_tsv(args.symbols_tsv_gz) as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            a = addr(row.get("address"))
            if a is None or a < min_addr or a > max_addr:
                continue
            symbols.append({
                "address": a,
                "address_hex": f"{a:08x}",
                "name": row.get("name"),
                "type": row.get("type"),
                "source": row.get("source"),
                "is_primary": row.get("is_primary"),
            })

    # Only references directly to the address point or the canonical -8 vtable
    # symbol start are collected. Wider nearby references would be too ambiguous.
    interesting_targets = set(addresses.values()) | {
        a - ITANIUM_ARM32_ADDRESS_POINT_DELTA for a in addresses.values()
    }
    xrefs_by_target: dict[int, list[dict[str, Any]]] = defaultdict(list)
    with open_tsv(args.xrefs_tsv_gz) as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            target = addr(row.get("to"))
            if target not in interesting_targets:
                continue
            xrefs_by_target[target].append({
                "from": row.get("from"),
                "to": row.get("to"),
                "type": row.get("type"),
                "operand": row.get("operand"),
                "source": row.get("source"),
            })

    out_rows: list[dict[str, Any]] = []
    for vtable_id, address_point in addresses.items():
        candidates: list[dict[str, Any]] = []
        for symbol in symbols:
            delta = address_point - symbol["address"]
            if abs(delta) > WINDOW:
                continue
            class_name, evidence_kind = symbol_class_name(str(symbol.get("name") or ""))
            if evidence_kind is None:
                continue
            score = 0
            layout = "nearby_only"
            if delta == ITANIUM_ARM32_ADDRESS_POINT_DELTA:
                score = 3
                layout = "arm32_itanium_address_point_plus_8"
            elif delta == 0:
                score = 2
                layout = "symbol_at_address_point"
            elif 0 < delta <= WINDOW:
                score = 1
                layout = "symbol_precedes_address_point"
            candidates.append({
                **symbol,
                "delta_to_address_point": delta,
                "layout_match": layout,
                "evidence_kind": evidence_kind,
                "class_name": class_name,
                "score": score,
            })

        candidates.sort(key=lambda x: (-x["score"], abs(x["delta_to_address_point"]), x["address"]))
        best = candidates[0] if candidates and candidates[0]["score"] > 0 else None
        tied = [x for x in candidates if best and x["score"] == best["score"]]
        resolved_class = None
        status = "unresolved"
        if best and len(tied) == 1 and best.get("class_name"):
            resolved_class = best["class_name"]
            status = "resolved_explicit_symbol"
        elif best and len(tied) == 1:
            status = "vtable_symbol_without_decodable_class"
        elif len(tied) > 1:
            status = "ambiguous_vtable_symbols"

        symbol_start = address_point - ITANIUM_ARM32_ADDRESS_POINT_DELTA
        out_rows.append({
            "format": FORMAT,
            "receiver_vtable_id": vtable_id,
            "receiver_vtable_address_point": f"0x{address_point:x}",
            "expected_arm32_itanium_symbol_start": f"0x{symbol_start:x}",
            "expected_address_point_delta": ITANIUM_ARM32_ADDRESS_POINT_DELTA,
            "status": status,
            "resolved_class_name": resolved_class,
            "best_symbol": best,
            "candidate_symbols": candidates,
            "address_point_xrefs": xrefs_by_target.get(address_point, []),
            "vtable_symbol_start_xrefs": xrefs_by_target.get(symbol_start, []),
            "class_inferred_from_target_name": False,
        })

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in out_rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    print("format:", FORMAT)
    print("vtables:", len(out_rows))
    print("resolved explicit symbols:", sum(x["status"] == "resolved_explicit_symbol" for x in out_rows))
    print("unresolved:", sum(x["status"] == "unresolved" for x in out_rows))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
