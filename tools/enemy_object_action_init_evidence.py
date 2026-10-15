#!/usr/bin/env python3
"""Recover structural EnemyObject -> EnemyActions action-init call arguments.

Consumes metadata-only High P-code from ExportFunctionPcodeFlow.java.  The
analyzer deliberately reports argument provenance without assigning meanings to
unknown parameters or guessing an action-data filename.
"""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path

REQUIRED_COLUMNS = {
    "function_entry", "op_address", "op_time", "opcode", "output",
    "inputs", "callee_entry", "callee_name",
}
TRANSPARENT = {"COPY", "CAST", "INDIRECT", "INT_ZEXT", "INT_SEXT", "SUBPIECE"}


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
        return f"{self.op_address}:{self.op_time}"


@dataclass(frozen=True)
class Source:
    kind: str
    constants: tuple[int, ...] = ()
    roots: tuple[str, ...] = ()


@dataclass(frozen=True)
class ArgumentEvidence:
    index: int
    token: str
    source: Source


@dataclass(frozen=True)
class CallEvidence:
    kind: str
    function_entry: str
    call_address: str
    call_time: int
    callee_entry: str
    callee_name: str
    receiver: Source | None
    arguments: tuple[ArgumentEvidence, ...]


def _split_inputs(cell: str) -> tuple[str, ...]:
    text = cell.strip()
    if text in ("", "-"):
        return ()
    return tuple(token for token in text.split(";") if token and token != "-")


def read_rows(path: Path) -> list[OpRow]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None or not REQUIRED_COLUMNS.issubset(reader.fieldnames):
            missing = sorted(REQUIRED_COLUMNS.difference(reader.fieldnames or []))
            raise ValueError(f"{path}: missing columns: {', '.join(missing)}")
        rows: list[OpRow] = []
        for raw in reader:
            rows.append(OpRow(
                function_entry=raw["function_entry"].strip(),
                op_address=raw["op_address"].strip(),
                op_time=int(raw["op_time"], 10),
                opcode=raw["opcode"].strip(),
                output=raw["output"].strip(),
                inputs=_split_inputs(raw["inputs"]),
                callee_entry=raw["callee_entry"].strip(),
                callee_name=raw["callee_name"].strip(),
            ))
        return rows


def _constant(token: str) -> int | None:
    if not token.startswith("const:0x"):
        return None
    try:
        return int(token[len("const:0x"):].split(":", 1)[0], 16)
    except ValueError:
        return None


def _definition_map(rows: list[OpRow]) -> dict[str, OpRow]:
    definitions: dict[str, OpRow] = {}
    for row in rows:
        if row.output in ("", "-"):
            continue
        previous = definitions.get(row.output)
        if previous is not None and previous.site != row.site:
            raise ValueError(f"duplicate defining token: {row.output}")
        definitions[row.output] = row
    return definitions


def _merge(sources: list[Source]) -> Source:
    if not sources:
        return Source("derived")
    constants = sorted({value for source in sources for value in source.constants})
    roots = sorted({value for source in sources for value in source.roots})
    if all(source.kind in ("constant", "constant-set") for source in sources) and constants:
        return Source("constant" if len(constants) == 1 else "constant-set", tuple(constants))
    if all(source.kind == "root" for source in sources) and len(roots) == 1:
        return Source("root", roots=tuple(roots))
    return Source("derived", tuple(constants), tuple(roots))


def trace_source(
    token: str,
    definitions: dict[str, OpRow],
    *,
    depth: int = 0,
    active: frozenset[str] = frozenset(),
) -> Source:
    if depth > 32 or token in active:
        return Source("derived", roots=(token,))
    constant = _constant(token)
    if constant is not None:
        return Source("constant", constants=(constant,))
    row = definitions.get(token)
    if row is None:
        return Source("root", roots=(token,))
    next_active = active.union((token,))
    if row.opcode in TRANSPARENT and row.inputs:
        return trace_source(row.inputs[0], definitions, depth=depth + 1, active=next_active)
    if row.opcode == "MULTIEQUAL":
        return _merge([
            trace_source(value, definitions, depth=depth + 1, active=next_active)
            for value in row.inputs
        ])
    child = _merge([
        trace_source(value, definitions, depth=depth + 1, active=next_active)
        for value in row.inputs
    ])
    return Source("derived", child.constants, child.roots or (token,))


def _normalized_entry(text: str) -> str:
    value = text.strip().lower()
    if value.startswith("0x"):
        value = value[2:]
    return value.lstrip("0") or "0"


def _entry_match(actual: str, expected: str | None) -> bool:
    return bool(expected and actual not in ("", "-") and
                _normalized_entry(actual) == _normalized_entry(expected))


def _call_kind(row: OpRow, init_entry: str | None, add_entry: str | None) -> str | None:
    if row.opcode != "CALL":
        return None
    name = row.callee_name
    if "EnemyActionsSystem" in name and "initWithFile" in name:
        return "enemy-actions-init-with-file"
    if _entry_match(row.callee_entry, init_entry):
        return "enemy-actions-init-with-file"
    if "ActionDataManager" in name and "addActionDataWithID" in name:
        return "action-data-manager-add-with-id"
    if _entry_match(row.callee_entry, add_entry):
        return "action-data-manager-add-with-id"
    return None


def collect_call_evidence(
    rows: list[OpRow],
    init_entry: str | None = None,
    add_entry: str | None = None,
) -> list[CallEvidence]:
    definitions = _definition_map(rows)
    result: list[CallEvidence] = []
    for row in rows:
        kind = _call_kind(row, init_entry, add_entry)
        if kind is None:
            continue
        receiver = trace_source(row.inputs[0], definitions) if row.inputs else None
        # ExportFunctionPcodeFlow omits the CALL target. Both targets audited
        # here are non-static C++ members, therefore data input 0 is `this`.
        arguments = tuple(
            ArgumentEvidence(index=index, token=token,
                             source=trace_source(token, definitions))
            for index, token in enumerate(row.inputs[1:])
        )
        result.append(CallEvidence(
            kind=kind,
            function_entry=row.function_entry,
            call_address=row.op_address,
            call_time=row.op_time,
            callee_entry=row.callee_entry,
            callee_name=row.callee_name,
            receiver=receiver,
            arguments=arguments,
        ))
    return sorted(result, key=lambda item: (item.call_address, item.call_time, item.kind))


def status(calls: list[CallEvidence]) -> str:
    init_calls = [call for call in calls if call.kind == "enemy-actions-init-with-file"]
    if not init_calls:
        return "no-enemy-actions-init-call"
    if any(call.receiver is None for call in init_calls):
        return "incomplete-member-call"
    return "structural-init-call-found"


def _source_text(source: Source | None) -> str:
    if source is None:
        return "missing"
    constants = ",".join(f"0x{value:x}" for value in source.constants) or "none"
    roots = ",".join(source.roots) or "none"
    return f"kind={source.kind} constants={constants} roots={roots}"


def render(calls: list[CallEvidence]) -> str:
    init_count = sum(call.kind == "enemy-actions-init-with-file" for call in calls)
    add_count = sum(call.kind == "action-data-manager-add-with-id" for call in calls)
    lines = [
        "EnemyObject action-init structural evidence",
        f"status: {status(calls)}",
        f"EnemyActionsSystem::initWithFile calls: {init_count}",
        f"ActionDataManager::addActionDataWithID calls: {add_count}",
    ]
    for call in calls:
        lines.append(
            f"call: {call.kind} {call.function_entry}:{call.call_address}:{call.call_time}"
        )
        lines.append(f"  callee: {call.callee_entry}:{call.callee_name}")
        lines.append(f"  receiver: {_source_text(call.receiver)}")
        lines.append(f"  explicit argument count: {len(call.arguments)}")
        for argument in call.arguments:
            lines.append(
                f"  arg[{argument.index}]: token={argument.token} {_source_text(argument.source)}"
            )
    lines.append(
        "semantic status: structural call arguments only; filename/object-type/ID meanings not assigned"
    )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcode_tsv", type=Path)
    parser.add_argument("--init-entry", help="optional exact Ghidra entry for EnemyActionsSystem::initWithFile")
    parser.add_argument("--add-entry", help="optional exact Ghidra entry for ActionDataManager::addActionDataWithID")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        calls = collect_call_evidence(read_rows(args.pcode_tsv), args.init_entry, args.add_entry)
        report = render(calls)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0 if status(calls) == "structural-init-call-found" else 2


if __name__ == "__main__":
    raise SystemExit(main())
