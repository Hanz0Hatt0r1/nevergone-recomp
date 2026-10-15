#!/usr/bin/env python3
"""Trace the param[0] dictionary/string construction inside EnemyObject::initActionsSystem."""

from __future__ import annotations

import argparse
import base64
import json
from collections import defaultdict
from pathlib import Path

from enemy_object_init_actions_callsite import (
    PcodeGraph,
    constant_value,
    format_affine,
    read_tsv,
    split_inputs,
)

STRING_FORMAT = "NEVERGONE.GhidraPcodeDefinedString/1"
TRANSPARENT = {"COPY", "CAST", "INDIRECT", "INT_ZEXT", "INT_SEXT", "SUBPIECE"}


def load_strings(path: Path) -> dict[int, str]:
    strings: dict[int, str] = {}
    for row in read_tsv(path):
        if (row.get("format") or "").strip() != STRING_FORMAT:
            raise ValueError(f"{path}: expected {STRING_FORMAT}")
        address_text = (row.get("constant_address") or "").strip()
        encoded = (row.get("utf8_base64") or "").strip()
        if not address_text.startswith("0x") or not encoded:
            raise ValueError(f"{path}: malformed string row")
        address = int(address_text, 16)
        text = base64.b64decode(encoded, validate=True).decode("utf-8")
        if address in strings and strings[address] != text:
            raise ValueError(f"{path}: conflicting string for {address_text}")
        strings[address] = text
    return strings


def call_name(row: dict[str, str]) -> str:
    return (row.get("callee_name") or row.get("callee_entry") or "-").strip()


def token_strings(
    graph: PcodeGraph,
    token: str,
    strings: dict[int, str],
    seen: set[str] | None = None,
    depth: int = 0,
) -> set[tuple[int, str]]:
    if seen is None:
        seen = set()
    if depth > 32 or token in seen:
        return set()
    value = constant_value(token)
    if value is not None:
        text = strings.get(value)
        return set() if text is None else {(value, text)}
    row = graph.definitions.get(token)
    if row is None:
        return set()
    inputs = split_inputs(row.get("inputs", ""))
    opcode = (row.get("opcode") or "").strip()
    if opcode == "LOAD" and inputs:
        inputs = (inputs[-1],)
    elif opcode in TRANSPARENT and inputs:
        inputs = (inputs[0],)
    next_seen = set(seen)
    next_seen.add(token)
    result: set[tuple[int, str]] = set()
    for child in inputs:
        result.update(token_strings(graph, child, strings, next_seen, depth + 1))
    return result


def dependency_calls(
    graph: PcodeGraph,
    token: str,
    seen_tokens: set[str] | None = None,
    depth: int = 0,
) -> dict[tuple[str, str], dict[str, str]]:
    if seen_tokens is None:
        seen_tokens = set()
    if depth > 40 or token in seen_tokens or constant_value(token) is not None:
        return {}
    row = graph.definitions.get(token)
    if row is None:
        return {}
    next_seen = set(seen_tokens)
    next_seen.add(token)
    result: dict[tuple[str, str], dict[str, str]] = {}
    if (row.get("opcode") or "").strip() == "CALL":
        result[((row.get("op_address") or "-").strip(), (row.get("op_time") or "-").strip())] = row
    inputs = split_inputs(row.get("inputs", ""))
    if (row.get("opcode") or "").strip() == "LOAD" and inputs:
        inputs = (inputs[-1],)
    for child in inputs:
        result.update(dependency_calls(graph, child, next_seen, depth + 1))
    return result


def same_affine(a: tuple[dict[str, int], int] | None, b: tuple[dict[str, int], int] | None) -> bool:
    return a is not None and b is not None and a == b


def slot_touches(
    rows: list[dict[str, str]],
    graph: PcodeGraph,
    slot: tuple[dict[str, int], int],
    strings: dict[int, str],
) -> list[str]:
    touches: list[str] = []
    for row in rows:
        opcode = (row.get("opcode") or "").strip()
        inputs = split_inputs(row.get("inputs", ""))
        site = (row.get("op_address") or "-").strip()
        if opcode == "CALL":
            for index, token in enumerate(inputs):
                if not same_affine(graph.affine(token), slot):
                    continue
                other_strings: set[tuple[int, str]] = set()
                for other_index, other in enumerate(inputs):
                    if other_index != index:
                        other_strings.update(token_strings(graph, other, strings))
                suffix = ""
                if other_strings:
                    suffix = " strings=" + ",".join(
                        json.dumps(text, ensure_ascii=False)
                        for _address, text in sorted(other_strings)
                    )
                touches.append(
                    f"CALL {site} {call_name(row)} arg[{index}]{suffix}"
                )
        elif opcode == "STORE" and len(inputs) >= 3:
            address_token = inputs[1]
            if same_affine(graph.affine(address_token), slot):
                value = inputs[2]
                refs = token_strings(graph, value, strings)
                suffix = ""
                if refs:
                    suffix = " strings=" + ",".join(
                        json.dumps(text, ensure_ascii=False) for _address, text in sorted(refs)
                    )
                touches.append(
                    f"STORE {site} value={graph.expression(value)}{suffix}"
                )
        elif opcode == "LOAD" and inputs:
            address_token = inputs[-1]
            if same_affine(graph.affine(address_token), slot):
                touches.append(f"LOAD {site}")
    return sorted(set(touches))


def analyze(pcode_path: Path, strings_path: Path) -> tuple[str, bool]:
    rows = read_tsv(pcode_path)
    if not rows:
        raise ValueError(f"{pcode_path}: empty P-code export")
    graph = PcodeGraph(rows)
    strings = load_strings(strings_path)

    factory_calls = [
        row for row in rows
        if (row.get("opcode") or "").strip() == "CALL"
        and "createWithFile" in call_name(row)
    ]
    if len(factory_calls) != 1:
        raise ValueError(f"{pcode_path}: expected one createWithFile call, got {len(factory_calls)}")
    factory_call = factory_calls[0]
    factory_inputs = split_inputs(factory_call.get("inputs", ""))
    if not factory_inputs:
        raise ValueError(f"{pcode_path}: createWithFile call has no data inputs")
    param0 = factory_inputs[0]
    param0_def = graph.definitions.get(param0)
    if param0_def is None or (param0_def.get("opcode") or "").strip() != "CALL":
        raise ValueError(f"{pcode_path}: createWithFile param[0] is not a direct CALL result")
    if "getCString" not in call_name(param0_def):
        raise ValueError(
            f"{pcode_path}: createWithFile param[0] call is not getCString: {call_name(param0_def)}"
        )

    calls = dependency_calls(graph, param0)
    ordered_calls = sorted(
        calls.values(),
        key=lambda row: (
            int((row.get("op_address") or "0").strip(), 16),
            int((row.get("op_time") or "0").strip()),
        ),
    )
    names = [call_name(row) for row in ordered_calls]
    getc_count = sum("getCString" in name for name in names)
    dict_load_count = sum("createWithContentsOfFile" in name for name in names)
    key_count = sum("objectForKey" in name for name in names)
    ccstring_create_count = sum(
        "CCString::create" in name and "createWith" not in name for name in names
    )

    function_entries = {(row.get("function_entry") or "-").strip() for row in rows}
    function_entry = next(iter(function_entries)) if len(function_entries) == 1 else "-"
    lines = [
        "EnemyObject action dictionary-chain evidence",
        f"function_entry: {function_entry}",
        f"factory_call_site: {(factory_call.get('op_address') or '-').strip()}",
        f"factory_param0: {param0}",
        f"dependency_calls: {len(ordered_calls)}",
        f"shape: getCString={getc_count} createWithContentsOfFile={dict_load_count} objectForKey={key_count} CCString::create={ccstring_create_count}",
        f"resolved_defined_strings: {len(strings)}",
        "call chain:",
    ]

    local_slots: dict[tuple[tuple[tuple[str, int], ...], int], tuple[dict[str, int], int]] = {}
    for row in ordered_calls:
        site = (row.get("op_address") or "-").strip()
        target = call_name(row)
        inputs = split_inputs(row.get("inputs", ""))
        output = (row.get("output") or "-").strip()
        lines.append(f"  call {site}: {target} -> {output}")
        for index, token in enumerate(inputs):
            affine = graph.affine(token)
            refs = token_strings(graph, token, strings)
            parts = [
                f"    arg[{index}]: token={token}",
                f"expr={graph.expression(token)}",
                f"affine={format_affine(affine)}",
            ]
            if refs:
                parts.append(
                    "strings=" + ",".join(
                        f"0x{address:x}:{json.dumps(text, ensure_ascii=False)}"
                        for address, text in sorted(refs)
                    )
                )
            lines.append(" ".join(parts))
            if "objectForKey" in target and index > 0 and affine is not None:
                terms, constant = affine
                if len(terms) == 1 and next(iter(terms.values())) == 1 and constant != 0:
                    key = (tuple(sorted(terms.items())), constant)
                    local_slots[key] = affine

    if local_slots:
        lines.append("local-slot evidence:")
        for slot in sorted(local_slots.values(), key=format_affine):
            lines.append(f"  slot: {format_affine(slot)}")
            touches = slot_touches(rows, graph, slot, strings)
            if not touches:
                lines.append("    touches: none")
            else:
                for touch in touches[:32]:
                    lines.append(f"    {touch}")
                if len(touches) > 32:
                    lines.append(f"    ... {len(touches) - 32} more touches omitted")

    all_param0_strings = token_strings(graph, param0, strings)
    lines.append("param[0] reachable defined strings:")
    if all_param0_strings:
        for address, text in sorted(all_param0_strings):
            lines.append(f"  0x{address:x}: {json.dumps(text, ensure_ascii=False)}")
    else:
        lines.append("  none")

    complete = (
        getc_count >= 2
        and dict_load_count >= 1
        and key_count >= 2
        and ccstring_create_count >= 1
    )
    lines.append(f"structural_chain_complete: {'true' if complete else 'false'}")
    lines.append(
        "semantic status: dictionary/string construction provenance only; literal strings are bounded defined-data references from this function and do not by themselves assign tutorial enemy ID/resource semantics"
    )
    return "\n".join(lines) + "\n", complete


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcode_tsv", type=Path)
    parser.add_argument("defined_strings_tsv", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report, complete = analyze(args.pcode_tsv, args.defined_strings_tsv)
    except (OSError, ValueError, UnicodeError) as error:
        raise SystemExit(f"error: {error}")
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0 if complete else 2


if __name__ == "__main__":
    raise SystemExit(main())
