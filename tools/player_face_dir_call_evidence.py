#!/usr/bin/env python3
"""Trace PLAYERFACEDIR sources at direct MEPlayer::setPlayerDir call sites.

Input is the metadata-only TSV emitted by ExportFunctionPcodeFlow.java for
MEPlayer::setControlsState().  The analyzer deliberately reports structural
sources and constant values only; it does not assign semantic enum names or map
CONTROLSSTATE values to PLAYERFACEDIR automatically.
"""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path

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
        return f"{self.op_address}:{self.op_time}"


@dataclass(frozen=True)
class Source:
    kind: str
    constants: tuple[int, ...] = ()
    root_tokens: tuple[str, ...] = ()


@dataclass(frozen=True)
class CallEvidence:
    function_entry: str
    call_address: str
    call_time: int
    callee_entry: str
    callee_name: str
    source: Source


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
        for row in reader:
            rows.append(
                OpRow(
                    function_entry=row["function_entry"].strip(),
                    op_address=row["op_address"].strip(),
                    op_time=int(row["op_time"], 10),
                    opcode=row["opcode"].strip(),
                    output=row["output"].strip(),
                    inputs=_split_inputs(row["inputs"]),
                    callee_entry=row["callee_entry"].strip(),
                    callee_name=row["callee_name"].strip(),
                )
            )
        return rows


def constant_value(token: str) -> int | None:
    if not token.startswith("const:0x"):
        return None
    value_text = token[len("const:0x"):].split(":", 1)[0]
    try:
        return int(value_text, 16)
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


def _merge_sources(sources: list[Source]) -> Source:
    if not sources:
        return Source("derived")

    constants: set[int] = set()
    roots: set[str] = set()
    only_constants = True
    for source in sources:
        constants.update(source.constants)
        roots.update(source.root_tokens)
        if source.kind not in ("constant", "constant-set"):
            only_constants = False

    if only_constants and constants:
        ordered = tuple(sorted(constants))
        return Source("constant" if len(ordered) == 1 else "constant-set", ordered)

    if all(source.kind == "root" for source in sources) and len(roots) == 1:
        return Source("root", root_tokens=tuple(sorted(roots)))

    return Source(
        "derived",
        constants=tuple(sorted(constants)),
        root_tokens=tuple(sorted(roots)),
    )


def trace_source(
    token: str,
    definitions: dict[str, OpRow],
    *,
    depth: int = 0,
    active: frozenset[str] = frozenset(),
) -> Source:
    if depth > 32 or token in active:
        return Source("derived", root_tokens=(token,))

    value = constant_value(token)
    if value is not None:
        return Source("constant", constants=(value,))

    row = definitions.get(token)
    if row is None:
        return Source("root", root_tokens=(token,))

    next_active = active.union((token,))
    if row.opcode in TRANSPARENT and row.inputs:
        return trace_source(
            row.inputs[0], definitions, depth=depth + 1, active=next_active
        )

    if row.opcode == "MULTIEQUAL":
        return _merge_sources(
            [
                trace_source(value, definitions, depth=depth + 1, active=next_active)
                for value in row.inputs
            ]
        )

    # Keep bounded arithmetic structural rather than guessing enum semantics.
    child_sources = [
        trace_source(value, definitions, depth=depth + 1, active=next_active)
        for value in row.inputs
    ]
    merged = _merge_sources(child_sources)
    return Source(
        "derived",
        constants=merged.constants,
        root_tokens=merged.root_tokens or (token,),
    )


def _normalized_entry(text: str) -> str:
    value = text.strip().lower()
    if value.startswith("0x"):
        value = value[2:]
    return value.lstrip("0") or "0"


def is_set_player_dir_call(row: OpRow, expected_entry: str | None) -> bool:
    if row.opcode != "CALL":
        return False
    name_match = "setPlayerDir" in row.callee_name
    entry_match = False
    if expected_entry and row.callee_entry not in ("", "-"):
        entry_match = _normalized_entry(row.callee_entry) == _normalized_entry(expected_entry)
    return name_match or entry_match


def collect_call_evidence(
    rows: list[OpRow],
    expected_entry: str | None = "0032c75a",
) -> list[CallEvidence]:
    definitions = _definition_map(rows)
    evidence: list[CallEvidence] = []
    for row in rows:
        if not is_set_player_dir_call(row, expected_entry):
            continue
        # ExportFunctionPcodeFlow omits the CALL target itself. For a non-static
        # C++ member call data input 0 is `this`, and input 1 is PLAYERFACEDIR.
        if len(row.inputs) < 2:
            evidence.append(
                CallEvidence(
                    row.function_entry,
                    row.op_address,
                    row.op_time,
                    row.callee_entry,
                    row.callee_name,
                    Source("missing-argument"),
                )
            )
            continue
        evidence.append(
            CallEvidence(
                row.function_entry,
                row.op_address,
                row.op_time,
                row.callee_entry,
                row.callee_name,
                trace_source(row.inputs[1], definitions),
            )
        )
    return sorted(evidence, key=lambda item: (item.call_address, item.call_time))


def overall_status(evidence: list[CallEvidence]) -> str:
    if not evidence:
        return "no-direct-calls"
    if any(item.source.kind == "missing-argument" for item in evidence):
        return "incomplete"
    if all(item.source.kind in ("constant", "constant-set") for item in evidence):
        return "constant-sources"
    return "derived-sources-present"


def render(evidence: list[CallEvidence]) -> str:
    lines = [
        "PLAYERFACEDIR call-site evidence",
        f"direct setPlayerDir calls: {len(evidence)}",
        f"status: {overall_status(evidence)}",
    ]
    all_constants: set[int] = set()
    for item in evidence:
        all_constants.update(item.source.constants)
        constants = ",".join(f"0x{value:x}" for value in item.source.constants) or "none"
        roots = ",".join(item.source.root_tokens) or "none"
        lines.extend(
            [
                f"call: {item.function_entry}:{item.call_address}:{item.call_time}",
                f"  callee: {item.callee_entry}:{item.callee_name}",
                f"  source kind: {item.source.kind}",
                f"  constants: {constants}",
                f"  roots: {roots}",
            ]
        )
    lines.append(
        "observed PLAYERFACEDIR constant candidates: "
        + (",".join(f"0x{value:x}" for value in sorted(all_constants)) or "none")
    )
    lines.append(
        "semantic status: structural argument sources only; no CONTROLSSTATE mapping assigned"
    )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcode_tsv", type=Path)
    parser.add_argument(
        "--callee-entry",
        default="0032c75a",
        help="Ghidra entry for MEPlayer::setPlayerDir in the analyzed image",
    )
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        evidence = collect_call_evidence(read_rows(args.pcode_tsv), args.callee_entry)
        report = render(evidence)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0 if evidence else 2


if __name__ == "__main__":
    raise SystemExit(main())
