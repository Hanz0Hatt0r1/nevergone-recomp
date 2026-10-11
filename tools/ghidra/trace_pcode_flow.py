#!/usr/bin/env python3
"""Trace bounded call-to-call data flow in ExportFunctionPcodeFlow TSV metadata."""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

REQUIRED_COLUMNS = {
    "function_entry",
    "op_address",
    "op_time",
    "opcode",
    "output",
    "inputs",
    "callee_entry",
    "callee_name",
}
DEFAULT_MAX_DEPTH = 64
DEFAULT_MAX_PATHS = 64


@dataclass(frozen=True)
class OpRow:
    function_entry: str
    op_address: str
    op_time: int
    opcode: str
    output: str
    inputs: tuple[str, ...]
    callee_entry: str
    callee_name: str

    @property
    def key(self) -> tuple[str, int]:
        return (self.op_address, self.op_time)


@dataclass(frozen=True)
class FlowPath:
    rows: tuple[OpRow, ...]

    @property
    def source(self) -> OpRow:
        return self.rows[0]

    @property
    def sink(self) -> OpRow:
        return self.rows[-1]


def read_rows(path: Path) -> list[OpRow]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None or not REQUIRED_COLUMNS.issubset(reader.fieldnames):
            missing = sorted(REQUIRED_COLUMNS.difference(reader.fieldnames or []))
            raise ValueError("missing P-code flow columns: " + ", ".join(missing))
        rows: list[OpRow] = []
        for raw in reader:
            input_cell = raw["inputs"].strip()
            inputs = () if input_cell in ("", "-") else tuple(
                token for token in input_cell.split(";") if token and token != "-"
            )
            rows.append(
                OpRow(
                    function_entry=raw["function_entry"].strip(),
                    op_address=raw["op_address"].strip(),
                    op_time=int(raw["op_time"], 10),
                    opcode=raw["opcode"].strip(),
                    output=raw["output"].strip(),
                    inputs=inputs,
                    callee_entry=raw["callee_entry"].strip(),
                    callee_name=raw["callee_name"].strip(),
                )
            )
    return rows


def _call_matches(row: OpRow, token: str) -> bool:
    return row.opcode in ("CALL", "CALLIND") and token in row.callee_name


def find_call_paths(
    rows: Iterable[OpRow],
    source_token: str,
    sink_token: str,
    *,
    max_depth: int = DEFAULT_MAX_DEPTH,
    max_paths: int = DEFAULT_MAX_PATHS,
) -> list[FlowPath]:
    if not source_token or not sink_token:
        raise ValueError("source and sink tokens must be non-empty")
    if max_depth < 1 or max_paths < 1:
        raise ValueError("max_depth and max_paths must be positive")

    materialized = list(rows)
    definitions: dict[str, OpRow] = {}
    for row in materialized:
        if row.output in ("", "-"):
            continue
        previous = definitions.get(row.output)
        if previous is not None and previous.key != row.key:
            raise ValueError(
                f"duplicate defining token {row.output!r}: {previous.key} and {row.key}"
            )
        definitions[row.output] = row

    source_keys = {
        row.key for row in materialized
        if row.output not in ("", "-") and _call_matches(row, source_token)
    }
    sinks = [row for row in materialized if _call_matches(row, sink_token)]

    paths: list[FlowPath] = []
    seen_paths: set[tuple[tuple[str, int], ...]] = set()

    def walk(token: str, depth: int, active: frozenset[str]) -> list[tuple[OpRow, ...]]:
        if depth > max_depth or token in active:
            return []
        producer = definitions.get(token)
        if producer is None:
            return []
        if producer.key in source_keys:
            return [(producer,)]

        next_active = active.union((token,))
        found: list[tuple[OpRow, ...]] = []
        for input_token in producer.inputs:
            for prefix in walk(input_token, depth + 1, next_active):
                found.append(prefix + (producer,))
                if len(found) >= max_paths:
                    return found
        return found

    for sink in sinks:
        for input_token in sink.inputs:
            for prefix in walk(input_token, 1, frozenset()):
                path_rows = prefix + (sink,)
                key = tuple(row.key for row in path_rows)
                if key in seen_paths:
                    continue
                seen_paths.add(key)
                paths.append(FlowPath(path_rows))
                if len(paths) >= max_paths:
                    return paths
    return paths


def render_paths(paths: Iterable[FlowPath]) -> str:
    materialized = list(paths)
    if not materialized:
        return "no data-flow path found\n"
    lines: list[str] = []
    for index, path in enumerate(materialized, 1):
        lines.append(
            f"path {index}: {path.source.callee_name} -> {path.sink.callee_name}"
        )
        for row in path.rows:
            callee = f" callee={row.callee_name}" if row.callee_name not in ("", "-") else ""
            lines.append(
                f"  {row.op_address}:{row.op_time} {row.opcode} output={row.output}{callee}"
            )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tsv", type=Path)
    parser.add_argument("--source", required=True, help="callee-name substring for source calls")
    parser.add_argument("--sink", required=True, help="callee-name substring for sink calls")
    parser.add_argument("--max-depth", type=int, default=DEFAULT_MAX_DEPTH)
    parser.add_argument("--max-paths", type=int, default=DEFAULT_MAX_PATHS)
    args = parser.parse_args()

    rows = read_rows(args.tsv)
    paths = find_call_paths(
        rows,
        args.source,
        args.sink,
        max_depth=args.max_depth,
        max_paths=args.max_paths,
    )
    print(render_paths(paths), end="")
    return 0 if paths else 2


if __name__ == "__main__":
    raise SystemExit(main())
