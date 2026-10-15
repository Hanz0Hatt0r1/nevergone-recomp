#!/usr/bin/env python3
"""Split reused EnemyObject action-dictionary key slots into temporal lifetimes."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

from enemy_object_action_dictionary_chain import call_name, dependency_calls, same_affine
from enemy_object_init_actions_callsite import PcodeGraph, format_affine, read_tsv, split_inputs

PLAN_FORMAT = "NEVERGONE.EnemyObjectActionKeyLifetimePlan/1"
IDENTITY_FORMAT = "NEVERGONE.GhidraFunctionIdentity/1"
PLAN_HEADER = [
    "format",
    "function_entry",
    "key_index",
    "object_for_key_site",
    "slot_affine",
    "predecessor_site",
    "predecessor_entry",
    "predecessor_name",
    "successor_site",
    "successor_entry",
    "successor_name",
]


def order_key(row: dict[str, str]) -> tuple[int, int]:
    address = (row.get("op_address") or "0").strip()
    time = (row.get("op_time") or "0").strip()
    try:
        return int(address, 16), int(time, 10)
    except ValueError as error:
        raise ValueError(f"malformed P-code operation location: {address}:{time}") from error


def call_rows(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    return [row for row in rows if (row.get("opcode") or "").strip() == "CALL"]


def call_touches_slot(row: dict[str, str], graph: PcodeGraph, slot: tuple[dict[str, int], int]) -> bool:
    if (row.get("opcode") or "").strip() != "CALL":
        return False
    return any(same_affine(graph.affine(token), slot) for token in split_inputs(row.get("inputs", "")))


def dependency_key_calls(rows: list[dict[str, str]], graph: PcodeGraph) -> list[dict[str, str]]:
    factory = [
        row
        for row in call_rows(rows)
        if "createWithFile" in call_name(row)
    ]
    if len(factory) != 1:
        raise ValueError(f"expected exactly one createWithFile call, got {len(factory)}")
    inputs = split_inputs(factory[0].get("inputs", ""))
    if not inputs:
        raise ValueError("createWithFile has no data inputs")
    calls = dependency_calls(graph, inputs[0])
    keys = [row for row in calls.values() if "objectForKey" in call_name(row)]
    keys.sort(key=order_key)
    if len(keys) < 2:
        raise ValueError(f"expected at least two objectForKey calls on param[0] dependency path, got {len(keys)}")
    return keys


def normalized_slot(graph: PcodeGraph, token: str) -> tuple[dict[str, int], int]:
    slot = graph.affine(token)
    if slot is None:
        raise ValueError(f"objectForKey key is not affine: {token}")
    terms, constant = slot
    if len(terms) != 1 or next(iter(terms.values())) != 1 or constant == 0:
        raise ValueError(f"objectForKey key is not a single-root local slot: {format_affine(slot)}")
    return slot


def build_plan(pcode_path: Path) -> list[dict[str, str]]:
    rows = read_tsv(pcode_path)
    if not rows:
        raise ValueError(f"{pcode_path}: empty P-code export")
    graph = PcodeGraph(rows)
    function_entries = {(row.get("function_entry") or "-").strip() for row in rows}
    if len(function_entries) != 1 or "-" in function_entries or "" in function_entries:
        raise ValueError(f"{pcode_path}: expected one function entry")
    function_entry = next(iter(function_entries))
    calls = sorted(call_rows(rows), key=order_key)
    keys = dependency_key_calls(rows, graph)
    plan: list[dict[str, str]] = []

    for index, target in enumerate(keys):
        inputs = split_inputs(target.get("inputs", ""))
        if len(inputs) < 2:
            raise ValueError(f"objectForKey at {target.get('op_address')} has no key argument")
        slot = normalized_slot(graph, inputs[1])
        target_order = order_key(target)
        touching = [row for row in calls if call_touches_slot(row, graph, slot)]
        before = [row for row in touching if order_key(row) < target_order]
        after = [row for row in touching if order_key(row) > target_order]
        if not before or not after:
            raise ValueError(
                f"objectForKey at {target.get('op_address')} lacks a preceding/following call touching {format_affine(slot)}"
            )
        predecessor = max(before, key=order_key)
        successor = min(after, key=order_key)
        pred_entry = (predecessor.get("callee_entry") or "-").strip()
        succ_entry = (successor.get("callee_entry") or "-").strip()
        if pred_entry in ("", "-") or succ_entry in ("", "-"):
            raise ValueError(
                f"unresolved lifetime helper around objectForKey at {target.get('op_address')}: "
                f"{pred_entry}/{succ_entry}"
            )
        plan.append(
            {
                "format": PLAN_FORMAT,
                "function_entry": function_entry,
                "key_index": str(index),
                "object_for_key_site": (target.get("op_address") or "-").strip(),
                "slot_affine": format_affine(slot),
                "predecessor_site": (predecessor.get("op_address") or "-").strip(),
                "predecessor_entry": pred_entry,
                "predecessor_name": call_name(predecessor),
                "successor_site": (successor.get("op_address") or "-").strip(),
                "successor_entry": succ_entry,
                "successor_name": call_name(successor),
            }
        )
    return plan


def write_plan(rows: list[dict[str, str]], output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=PLAN_HEADER, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def read_plan(path: Path) -> list[dict[str, str]]:
    rows = read_tsv(path)
    if not rows:
        raise ValueError(f"{path}: empty lifetime plan")
    for row in rows:
        if (row.get("format") or "").strip() != PLAN_FORMAT:
            raise ValueError(f"{path}: expected {PLAN_FORMAT}")
    return rows


def load_identities(path: Path) -> dict[str, dict[str, str]]:
    result: dict[str, dict[str, str]] = {}
    for row in read_tsv(path):
        if (row.get("format") or "").strip() != IDENTITY_FORMAT:
            raise ValueError(f"{path}: expected {IDENTITY_FORMAT}")
        requested = (row.get("requested_entry") or "-").strip()
        if requested in result:
            raise ValueError(f"{path}: duplicate identity for {requested}")
        result[requested] = row
    if not result:
        raise ValueError(f"{path}: no helper identities")
    return result


def row_at_site(rows: list[dict[str, str]], site: str) -> dict[str, str]:
    matches = [
        row
        for row in rows
        if (row.get("opcode") or "").strip() == "CALL"
        and (row.get("op_address") or "-").strip() == site
    ]
    if len(matches) != 1:
        raise ValueError(f"expected one CALL at {site}, got {len(matches)}")
    return matches[0]


def render_arg(graph: PcodeGraph, token: str) -> str:
    roots = sorted(graph.roots(token))
    return (
        f"token={token} expr={graph.expression(token)} affine={format_affine(graph.affine(token))} "
        f"roots={','.join(roots) if roots else 'constants-only'}"
    )


def render_call(
    lines: list[str],
    label: str,
    row: dict[str, str],
    graph: PcodeGraph,
    identity: dict[str, str] | None,
) -> None:
    site = (row.get("op_address") or "-").strip()
    entry = (row.get("callee_entry") or "-").strip()
    lines.append(f"  {label}: {site} {entry}:{call_name(row)}")
    if identity is not None:
        lines.append(
            "    identity: "
            f"{(identity.get('resolved_entry') or '-').strip()}:"
            f"{(identity.get('qualified_name') or '-').strip()}"
        )
        lines.append(f"    signature: {(identity.get('signature') or '-').strip()}")
    inputs = split_inputs(row.get("inputs", ""))
    for index, token in enumerate(inputs):
        lines.append(f"    arg[{index}]: {render_arg(graph, token)}")
        deps = sorted(dependency_calls(graph, token).values(), key=order_key)
        if deps:
            rendered = ", ".join(
                f"{(dep.get('op_address') or '-').strip()}:{call_name(dep)}"
                for dep in deps[-8:]
            )
            lines.append(f"      dependency_calls: {rendered}")


def render_report(pcode_path: Path, plan_path: Path, identities_path: Path) -> str:
    rows = read_tsv(pcode_path)
    if not rows:
        raise ValueError(f"{pcode_path}: empty P-code export")
    graph = PcodeGraph(rows)
    plan = read_plan(plan_path)
    identities = load_identities(identities_path)

    slots = {(row.get("slot_affine") or "-").strip() for row in plan}
    helper_pairs = {
        (
            (row.get("predecessor_entry") or "-").strip(),
            (row.get("successor_entry") or "-").strip(),
        )
        for row in plan
    }
    lines = [
        "EnemyObject action dictionary key-lifetime evidence",
        f"function_entry: {(plan[0].get('function_entry') or '-').strip()}",
        f"key_lifetimes: {len(plan)}",
        f"unique_slots: {len(slots)}",
        f"reused_slot_across_keys: {'true' if len(slots) < len(plan) else 'false'}",
        f"unique_helper_pairs: {len(helper_pairs)}",
    ]

    for row in sorted(plan, key=lambda item: int((item.get("key_index") or "0").strip())):
        index = (row.get("key_index") or "-").strip()
        site = (row.get("object_for_key_site") or "-").strip()
        predecessor_site = (row.get("predecessor_site") or "-").strip()
        successor_site = (row.get("successor_site") or "-").strip()
        predecessor = row_at_site(rows, predecessor_site)
        target = row_at_site(rows, site)
        successor = row_at_site(rows, successor_site)
        if not (order_key(predecessor) < order_key(target) < order_key(successor)):
            raise ValueError(f"key[{index}] lifetime ordering changed")
        pred_entry = (row.get("predecessor_entry") or "-").strip()
        succ_entry = (row.get("successor_entry") or "-").strip()
        if pred_entry not in identities or succ_entry not in identities:
            raise ValueError(f"missing helper identity for key[{index}]")
        lines.append(f"key[{index}]: objectForKey={site}")
        lines.append(f"  slot: {(row.get('slot_affine') or '-').strip()}")
        render_call(lines, "predecessor", predecessor, graph, identities[pred_entry])
        render_call(lines, "objectForKey", target, graph, None)
        render_call(lines, "successor", successor, graph, identities[succ_entry])
        lines.append("  lifetime_order_complete: true")

    lines.append(
        "semantic status: temporal stack-slot lifetime and helper-call identity evidence only; "
        "constructor/destructor/key/resource meanings are not assigned without independent evidence"
    )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    plan_parser = subparsers.add_parser("plan")
    plan_parser.add_argument("pcode_tsv", type=Path)
    plan_parser.add_argument("output_tsv", type=Path)

    report_parser = subparsers.add_parser("report")
    report_parser.add_argument("pcode_tsv", type=Path)
    report_parser.add_argument("plan_tsv", type=Path)
    report_parser.add_argument("identities_tsv", type=Path)
    report_parser.add_argument("--output", type=Path)

    args = parser.parse_args()
    try:
        if args.command == "plan":
            plan = build_plan(args.pcode_tsv)
            write_plan(plan, args.output_tsv)
            print(f"key lifetimes planned: {len(plan)}")
            print(f"output: {args.output_tsv}")
            return 0
        report = render_report(args.pcode_tsv, args.plan_tsv, args.identities_tsv)
        if args.output is not None:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(report, encoding="utf-8")
        print(report, end="")
        return 0
    except (OSError, ValueError) as error:
        raise SystemExit(f"error: {error}")


if __name__ == "__main__":
    raise SystemExit(main())
