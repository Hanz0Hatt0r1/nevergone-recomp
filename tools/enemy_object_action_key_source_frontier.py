#!/usr/bin/env python3
"""Trace bounded value provenance feeding EnemyObject action-dictionary key lifetimes."""

from __future__ import annotations

import argparse
from pathlib import Path

from enemy_object_action_dictionary_chain import call_name, dependency_calls
from enemy_object_action_key_lifetime import order_key, read_plan, row_at_site
from enemy_object_init_actions_callsite import PcodeGraph, format_affine, read_tsv, split_inputs

TRANSPARENT = {"COPY", "CAST", "INDIRECT", "INT_ZEXT", "INT_SEXT", "SUBPIECE"}
MAX_DEPTH = 18
MAX_NODES = 128


def node_site(row: dict[str, str]) -> str:
    return f"{(row.get('op_address') or '-').strip()}:{(row.get('op_time') or '-').strip()}"


def render_value(graph: PcodeGraph, token: str) -> str:
    roots = sorted(graph.roots(token))
    return (
        f"token={token} expr={graph.expression(token)} "
        f"affine={format_affine(graph.affine(token))} "
        f"roots={','.join(roots) if roots else 'constants-only'}"
    )


def children_for(row: dict[str, str]) -> tuple[str, ...]:
    opcode = (row.get("opcode") or "").strip()
    inputs = split_inputs(row.get("inputs", ""))
    if opcode == "LOAD":
        return inputs[-1:] if inputs else ()
    if opcode in TRANSPARENT:
        return inputs[:1]
    return inputs


def source_tree(graph: PcodeGraph, token: str) -> tuple[list[str], bool]:
    lines: list[str] = []
    seen: set[str] = set()
    truncated = False

    def walk(current: str, depth: int) -> None:
        nonlocal truncated
        indent = "  " * depth
        if len(seen) >= MAX_NODES:
            truncated = True
            lines.append(f"{indent}TRUNCATED node-budget={MAX_NODES}")
            return
        if depth > MAX_DEPTH:
            truncated = True
            lines.append(f"{indent}TRUNCATED depth>{MAX_DEPTH}: {current}")
            return
        if current in seen:
            lines.append(f"{indent}cycle/ref: {current}")
            return
        seen.add(current)
        if current.startswith("const:0x"):
            lines.append(f"{indent}constant: {current}")
            return
        row = graph.definitions.get(current)
        if row is None:
            lines.append(f"{indent}root: {current}")
            return
        opcode = (row.get("opcode") or "-").strip()
        target = ""
        if opcode == "CALL":
            target = f" target={call_name(row)}"
        lines.append(
            f"{indent}{current}: {opcode} site={node_site(row)}{target} "
            f"expr={graph.expression(current)}"
        )
        if opcode == "LOAD":
            lines.append(f"{indent}  memory-boundary: address provenance only")
        for child in children_for(row):
            walk(child, depth + 1)

    walk(token, 0)
    return lines, truncated


def dependency_call_rows(graph: PcodeGraph, token: str) -> list[dict[str, str]]:
    rows = list(dependency_calls(graph, token).values())
    rows.sort(key=order_key)
    return rows


def render_call_details(lines: list[str], graph: PcodeGraph, prefix: str, row: dict[str, str]) -> None:
    site = (row.get("op_address") or "-").strip()
    target = call_name(row)
    lines.append(f"{prefix}call {site}: {target}")
    for index, token in enumerate(split_inputs(row.get("inputs", ""))):
        lines.append(f"{prefix}  arg[{index}]: {render_value(graph, token)}")


def analyze(pcode_path: Path, plan_path: Path) -> tuple[str, bool]:
    rows = read_tsv(pcode_path)
    if not rows:
        raise ValueError(f"{pcode_path}: empty P-code export")
    graph = PcodeGraph(rows)
    plan = read_plan(plan_path)
    if len(plan) != 2:
        raise ValueError(f"{plan_path}: expected exactly two key lifetimes, got {len(plan)}")

    lines = [
        "EnemyObject action dictionary key source-frontier evidence",
        f"function_entry: {(plan[0].get('function_entry') or '-').strip()}",
        f"key_lifetimes: {len(plan)}",
    ]
    complete = True

    for item in sorted(plan, key=lambda row: int((row.get("key_index") or "0").strip())):
        index = int((item.get("key_index") or "0").strip())
        predecessor = row_at_site(rows, (item.get("predecessor_site") or "-").strip())
        predecessor_inputs = split_inputs(predecessor.get("inputs", ""))
        if len(predecessor_inputs) < 2:
            raise ValueError(f"key[{index}] predecessor has fewer than two data arguments")
        source = predecessor_inputs[1]
        lines.append(f"key[{index}]:")
        lines.append(
            f"  predecessor: {(predecessor.get('op_address') or '-').strip()} "
            f"{call_name(predecessor)}"
        )
        lines.append(f"  source_arg[1]: {render_value(graph, source)}")

        deps = dependency_call_rows(graph, source)
        lines.append(f"  dependency_calls: {len(deps)}")
        for dep in deps:
            render_call_details(lines, graph, "    ", dep)

        format_calls = [row for row in deps if "createWithFormat" in call_name(row)]
        lines.append(f"  createWithFormat_calls: {len(format_calls)}")
        for call in format_calls:
            render_call_details(lines, graph, "    ", call)

        tree, truncated = source_tree(graph, source)
        lines.append(f"  source_tree_truncated: {'true' if truncated else 'false'}")
        lines.append("  source_tree:")
        lines.extend(f"    {line}" for line in tree)
        if truncated:
            complete = False

    first_source = split_inputs(row_at_site(rows, (plan[0].get("predecessor_site") or "-").strip()).get("inputs", ""))[1]
    second_source = split_inputs(row_at_site(rows, (plan[1].get("predecessor_site") or "-").strip()).get("inputs", ""))[1]
    first_formats = [row for row in dependency_call_rows(graph, first_source) if "createWithFormat" in call_name(row)]
    second_formats = [row for row in dependency_call_rows(graph, second_source) if "createWithFormat" in call_name(row)]
    lines.append(f"key0_createWithFormat_present: {'true' if first_formats else 'false'}")
    lines.append(f"key1_createWithFormat_present: {'true' if second_formats else 'false'}")
    lines.append(f"source_frontier_complete: {'true' if complete else 'false'}")
    lines.append(
        "semantic status: bounded predecessor-argument provenance only; format arguments, PHI inputs, "
        "constants and roots are not assigned dictionary-key/resource/enemy-ID semantics without independent evidence"
    )
    return "\n".join(lines) + "\n", complete


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcode_tsv", type=Path)
    parser.add_argument("lifetime_plan_tsv", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report, complete = analyze(args.pcode_tsv, args.lifetime_plan_tsv)
    except (OSError, ValueError) as error:
        raise SystemExit(f"error: {error}")
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0 if complete else 2


if __name__ == "__main__":
    raise SystemExit(main())
