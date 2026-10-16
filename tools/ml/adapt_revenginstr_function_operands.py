#!/usr/bin/env python3
"""Adapt generic revenginstr function operand JSONL to Nevergone's legacy TSV."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

INPUT_FORMAT = "REVENGINSTR.GhidraFunctionOperand/1"
HEADER = (
    "function_entry\tinstruction\toperand\tkind\tbit_length\tsigned_value\t"
    "unsigned_hex\ttarget\ttarget_type\tnumber_value"
)
FIELDS = {
    "format", "program", "requested", "function_entry", "instruction", "operand",
    "kind", "bit_length", "signed_value", "unsigned_hex", "target", "target_type",
    "number_value",
}


def _legacy_address(value: Any) -> str:
    if value is None:
        return "-"
    if not isinstance(value, str) or not value.startswith("0x"):
        raise ValueError(f"invalid generic address: {value!r}")
    return value[2:]


def _cell(value: Any) -> str:
    if value is None:
        return "-"
    if isinstance(value, (str, int)):
        text = str(value)
    else:
        raise ValueError(f"invalid generic operand cell: {value!r}")
    if any(char in text for char in "\t\r\n"):
        raise ValueError("generic operand cell contains a TSV separator")
    return text


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
                row = json.loads(line)
                if not isinstance(row, dict) or set(row) != FIELDS:
                    raise ValueError("unexpected row fields")
                if row["format"] != INPUT_FORMAT:
                    raise ValueError(f"unexpected format {row['format']!r}")
                if row["kind"] not in {"immediate", "referenced-scalar", "referenced-number"}:
                    raise ValueError(f"unexpected kind {row['kind']!r}")
                cells = [
                    _legacy_address(row["function_entry"]),
                    _legacy_address(row["instruction"]),
                    _cell(row["operand"]),
                    _cell(row["kind"]),
                    _cell(row["bit_length"]),
                    _cell(row["signed_value"]),
                    _cell(row["unsigned_hex"]),
                    _legacy_address(row["target"]),
                    _cell(row["target_type"]),
                    _cell(row["number_value"]),
                ]
                outgoing.write("\t".join(cells) + "\n")
            except (KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
                raise ValueError(f"{source}:{line_number}: {exc}") from exc
            count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_jsonl", type=Path)
    parser.add_argument("output_tsv", type=Path)
    args = parser.parse_args()
    count = adapt(args.input_jsonl, args.output_tsv)
    print(f"adapted {count} generic operand rows -> {args.output_tsv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
