#!/usr/bin/env python3
"""Find structural tutorial scene-limit candidates in exported high-P-code metadata.

The recovered LessonLayer geometry reaches a scene-limits object through the
pointer chain GameScene +0x238 -> +0x20.  This analyzer finds only loads below
that structural prefix and classifies their downstream use by the already
recovered 500- and 20-unit comparison constants.  It does not assign map-field
names or values to arbitrary offsets.
"""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict, deque
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

PREFIX = (0x238, 0x20)
INTERESTING_CONSTANTS = {
    20: "int20",
    500: "int500",
    0x41A00000: "float20",
    0x43FA0000: "float500",
}
TRANSPARENT = {
    "COPY",
    "CAST",
    "INDIRECT",
    "INT_ZEXT",
    "INT_SEXT",
    "SUBPIECE",
}


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
    def site(self) -> str:
        return f"{self.function_entry}:{self.op_address}:{self.op_time}"


@dataclass(frozen=True)
class PointerExpr:
    root: str
    deref_path: tuple[int, ...]
    pending_offset: int


@dataclass(frozen=True)
class Candidate:
    function_entry: str
    field_offset: int
    sites: tuple[str, ...]
    constants: tuple[int, ...]
    classification: str


def _inputs(cell: str) -> tuple[str, ...]:
    cell = cell.strip()
    if cell in ("", "-"):
        return ()
    return tuple(value for value in cell.split(";") if value and value != "-")


def read_rows(path: Path) -> list[OpRow]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None or not REQUIRED_COLUMNS.issubset(reader.fieldnames):
            missing = sorted(REQUIRED_COLUMNS.difference(reader.fieldnames or []))
            raise ValueError(f"{path}: missing P-code flow columns: {', '.join(missing)}")
        return [
            OpRow(
                function_entry=row["function_entry"].strip(),
                op_address=row["op_address"].strip(),
                op_time=int(row["op_time"], 10),
                opcode=row["opcode"].strip(),
                output=row["output"].strip(),
                inputs=_inputs(row["inputs"]),
                callee_entry=row["callee_entry"].strip(),
                callee_name=row["callee_name"].strip(),
            )
            for row in reader
        ]


def constant_value(token: str) -> int | None:
    if not token.startswith("const:0x"):
        return None
    value_text = token[len("const:0x"):].split(":", 1)[0]
    try:
        return int(value_text, 16)
    except ValueError:
        return None


def _definition_map(rows: Iterable[OpRow]) -> dict[str, OpRow]:
    result: dict[str, OpRow] = {}
    for row in rows:
        if row.output in ("", "-"):
            continue
        previous = result.get(row.output)
        if previous is not None and previous.site != row.site:
            raise ValueError(f"duplicate defining token {row.output!r}")
        result[row.output] = row
    return result


def _decode_pointer(
    token: str,
    definitions: dict[str, OpRow],
    *,
    depth: int = 0,
    active: frozenset[str] = frozenset(),
) -> PointerExpr | None:
    if depth > 32 or token in active:
        return None
    row = definitions.get(token)
    if row is None:
        if constant_value(token) is not None:
            return None
        return PointerExpr(token, (), 0)

    next_active = active.union((token,))
    if row.opcode in TRANSPARENT and row.inputs:
        return _decode_pointer(
            row.inputs[0], definitions, depth=depth + 1, active=next_active
        )

    if row.opcode == "LOAD" and row.inputs:
        location = _decode_pointer(
            row.inputs[-1], definitions, depth=depth + 1, active=next_active
        )
        if location is None:
            return None
        return PointerExpr(
            location.root,
            location.deref_path + (location.pending_offset,),
            0,
        )

    def base_plus(base_token: str, delta: int) -> PointerExpr | None:
        base = _decode_pointer(
            base_token, definitions, depth=depth + 1, active=next_active
        )
        if base is None:
            return None
        return PointerExpr(base.root, base.deref_path, base.pending_offset + delta)

    if row.opcode in ("INT_ADD", "PTRSUB") and len(row.inputs) >= 2:
        left = constant_value(row.inputs[0])
        right = constant_value(row.inputs[1])
        if row.opcode == "INT_ADD":
            if right is not None:
                return base_plus(row.inputs[0], right)
            if left is not None:
                return base_plus(row.inputs[1], left)
        elif right is not None:
            return base_plus(row.inputs[0], right)

    if row.opcode == "INT_SUB" and len(row.inputs) >= 2:
        right = constant_value(row.inputs[1])
        if right is not None:
            return base_plus(row.inputs[0], -right)

    if row.opcode == "PTRADD" and len(row.inputs) >= 3:
        index = constant_value(row.inputs[1])
        scale = constant_value(row.inputs[2])
        if index is not None and scale is not None:
            return base_plus(row.inputs[0], index * scale)

    return None


def _candidate_field(chain: tuple[int, ...]) -> int | None:
    if len(chain) < 3:
        return None
    for index in range(len(chain) - 2):
        if chain[index:index + 2] == PREFIX:
            return chain[index + 2]
    return None


def _consumer_map(rows: Iterable[OpRow]) -> dict[str, list[OpRow]]:
    result: dict[str, list[OpRow]] = defaultdict(list)
    for row in rows:
        for token in row.inputs:
            result[token].append(row)
    return result


def _downstream_constants(
    start_token: str,
    consumers: dict[str, list[OpRow]],
    *,
    max_depth: int = 16,
) -> set[int]:
    found: set[int] = set()
    queue: deque[tuple[str, int]] = deque([(start_token, 0)])
    visited: set[str] = set()
    while queue:
        token, depth = queue.popleft()
        if depth > max_depth or token in visited:
            continue
        visited.add(token)
        for row in consumers.get(token, ()):
            for input_token in row.inputs:
                value = constant_value(input_token)
                if value in INTERESTING_CONSTANTS:
                    found.add(value)
            if row.output not in ("", "-"):
                queue.append((row.output, depth + 1))
    return found


def _classification(constants: set[int]) -> str:
    has_500 = 500 in constants or 0x43FA0000 in constants
    has_20 = 20 in constants or 0x41A00000 in constants
    if has_500 and not has_20:
        return "right-edge-x-candidate"
    if has_20 and not has_500:
        return "vertical-edge-candidate"
    if has_20 and has_500:
        return "mixed-threshold-candidate"
    return "unclassified"


def find_candidates(rows: Iterable[OpRow]) -> list[Candidate]:
    materialized = list(rows)
    definitions = _definition_map(materialized)
    consumers = _consumer_map(materialized)
    grouped: dict[tuple[str, int], dict[str, object]] = {}

    for row in materialized:
        if row.opcode != "LOAD" or row.output in ("", "-") or not row.inputs:
            continue
        location = _decode_pointer(row.inputs[-1], definitions)
        if location is None:
            continue
        chain = location.deref_path + (location.pending_offset,)
        field = _candidate_field(chain)
        if field is None:
            continue
        key = (row.function_entry, field)
        state = grouped.setdefault(key, {"sites": set(), "constants": set()})
        state["sites"].add(row.site)
        state["constants"].update(_downstream_constants(row.output, consumers))

    result: list[Candidate] = []
    for (function_entry, field), state in grouped.items():
        sites = tuple(sorted(state["sites"]))
        constants = tuple(sorted(state["constants"]))
        result.append(
            Candidate(
                function_entry=function_entry,
                field_offset=field,
                sites=sites,
                constants=constants,
                classification=_classification(set(constants)),
            )
        )
    return sorted(result, key=lambda item: (item.function_entry, item.field_offset))


def structural_status(candidates: Iterable[Candidate]) -> str:
    materialized = list(candidates)
    right = {c.field_offset for c in materialized if c.classification == "right-edge-x-candidate"}
    vertical = {c.field_offset for c in materialized if c.classification == "vertical-edge-candidate"}
    if len(right) == 1 and len(vertical) == 2 and right.isdisjoint(vertical):
        return "three-limit-candidate"
    return "incomplete-or-ambiguous"


def render(candidates: Iterable[Candidate]) -> str:
    values = list(candidates)
    lines = [
        "Tutorial scene-limit structural evidence",
        "required pointer prefix: 0x238 -> 0x20",
        f"status: {structural_status(values)}",
        f"candidate count: {len(values)}",
    ]
    for candidate in values:
        labels = ",".join(INTERESTING_CONSTANTS[value] for value in candidate.constants)
        lines.extend(
            [
                f"candidate: function={candidate.function_entry} field={candidate.field_offset:#x}",
                f"  classification: {candidate.classification}",
                f"  threshold constants: {labels or 'none'}",
                f"  load sites: {','.join(candidate.sites)}",
            ]
        )
    lines.append(
        "semantic status: structural candidates only; vertical A/B ordering remains unassigned"
    )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tsv", nargs="+", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    rows: list[OpRow] = []
    try:
        for path in args.tsv:
            rows.extend(read_rows(path))
        candidates = find_candidates(rows)
        report = render(candidates)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0 if candidates else 2


if __name__ == "__main__":
    raise SystemExit(main())
