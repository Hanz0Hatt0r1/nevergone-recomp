#!/usr/bin/env python3
"""Adapt generic revenginstr address JSONL to Nevergone's historical TSV."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

INPUT_FORMAT = "REVENGINSTR.GhidraAddressMetadata/1"
OUTPUT_FORMAT = "NEVERGONE.GhidraAddressClassification/1"
HEADER = (
    "format\taddress\tblock\tblock_start\tblock_end\tread\twrite\texecute\t"
    "primary_symbol\tfunction_at_entry\tfunction_at_name\tfunction_at_body\t"
    "function_at_is_thunk\tfunction_at_thunk_target\tfunction_containing_entry\t"
    "function_containing_name\tfunction_containing_body\tinstruction_mnemonic\t"
    "data_type\traw_u32"
)
FIELDS = {
    "format", "program", "requested", "address", "block", "primary_symbol",
    "function_at", "function_containing", "instruction_mnemonic", "data_type", "raw_u32",
}


def _cell(value: Any) -> str:
    if value is None:
        return "-"
    if isinstance(value, bool):
        result = "1" if value else "0"
    elif isinstance(value, (str, int)):
        result = str(value)
    else:
        raise ValueError(f"unexpected cell value {value!r}")
    return result.replace("\t", " ").replace("\n", " ").replace("\r", " ")


def _address(value: Any) -> str:
    if not isinstance(value, str) or not value.startswith("0x"):
        raise ValueError(f"invalid generic address {value!r}")
    return value[2:]


def _body(value: Any) -> str:
    if value is None:
        return "-"
    if not isinstance(value, str) or ".." not in value:
        raise ValueError(f"invalid generic function body {value!r}")
    start, end = value.split("..", 1)
    return f"{_address(start)}..{_address(end)}"


def _function(row: Any, with_thunk: bool = False) -> list[str]:
    if row is None:
        return ["-", "-", "-"] + (["-", "-"] if with_thunk else [])
    if not isinstance(row, dict):
        raise ValueError("function metadata must be an object or null")
    required = {"entry", "name", "body"}
    if with_thunk:
        required.update({"is_thunk", "thunk_target"})
    if set(row) != required:
        raise ValueError("unexpected function metadata fields")
    cells = [_address(row["entry"]), _cell(row["name"]), _body(row["body"])]
    if with_thunk:
        if not isinstance(row["is_thunk"], bool):
            raise ValueError("is_thunk must be a boolean")
        target = row["thunk_target"]
        if target is None:
            target_cell = "-"
        elif target == "<error>":
            target_cell = target
        elif isinstance(target, dict) and set(target) == {"entry", "name"}:
            target_cell = f"{_address(target['entry'])}:{_cell(target['name'])}"
        else:
            raise ValueError("invalid thunk target")
        cells.extend(["1" if row["is_thunk"] else "0", target_cell])
    return cells


def _adapt_row(row: Any) -> list[str]:
    if not isinstance(row, dict) or set(row) != FIELDS:
        raise ValueError("unexpected row fields")
    if row["format"] != INPUT_FORMAT:
        raise ValueError(f"unexpected format {row['format']!r}")
    block = row["block"]
    if block is None:
        block_cells = ["-"] * 6
    elif isinstance(block, dict) and set(block) == {"name", "start", "end", "read", "write", "execute"}:
        if not all(isinstance(block[key], bool) for key in ("read", "write", "execute")):
            raise ValueError("memory-block permissions must be booleans")
        block_cells = [
            _cell(block["name"]), _address(block["start"]), _address(block["end"]),
            *["1" if block[key] else "0" for key in ("read", "write", "execute")],
        ]
    else:
        raise ValueError("invalid memory block metadata")
    return [
        OUTPUT_FORMAT,
        _address(row["address"]),
        *block_cells,
        _cell(row["primary_symbol"]),
        *_function(row["function_at"], with_thunk=True),
        *_function(row["function_containing"]),
        _cell(row["instruction_mnemonic"]),
        _cell(row["data_type"]),
        _cell(row["raw_u32"]),
    ]


def adapt(source: Path, output: Path) -> int:
    output.parent.mkdir(parents=True, exist_ok=True)
    count = 0
    with source.open(encoding="utf-8") as incoming, output.open(
        "w", encoding="utf-8", newline=""
    ) as outgoing:
        outgoing.write(HEADER + "\n")
        for line_number, line in enumerate(incoming, 1):
            if not line.strip():
                continue
            try:
                cells = _adapt_row(json.loads(line))
            except (KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
                raise ValueError(f"{source}:{line_number}: {exc}") from exc
            outgoing.write("\t".join(cells) + "\n")
            count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_jsonl", type=Path)
    parser.add_argument("output_tsv", type=Path)
    args = parser.parse_args()
    count = adapt(args.input_jsonl, args.output_tsv)
    print(f"adapted {count} generic address rows -> {args.output_tsv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
