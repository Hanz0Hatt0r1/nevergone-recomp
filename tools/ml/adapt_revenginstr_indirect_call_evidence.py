#!/usr/bin/env python3
"""Adapt generic revenginstr indirect-call JSONL to Nevergone's legacy TSV."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any

INPUT_FORMAT = "REVENGINSTR.GhidraIndirectCallEvidence/1"
HEADER = "function_entry\tcall_address\tcall_time\ttarget\ttarget_expression\targ_count"
FIELDS = {
    "format", "program", "requested", "function_entry", "call_address",
    "call_time", "target", "target_expression", "arg_count",
}


def _address(value: Any) -> str:
    if not isinstance(value, str) or re.fullmatch(r"0x[0-9a-fA-F]+", value) is None:
        raise ValueError(f"invalid generic address {value!r}")
    return value[2:]


def _cell(value: Any) -> str:
    if not isinstance(value, str):
        raise ValueError(f"expected string cell, got {value!r}")
    return value.replace("\t", " ").replace("\n", " ").replace("\r", " ")


def _adapt_row(row: Any) -> list[str]:
    if not isinstance(row, dict) or set(row) != FIELDS:
        raise ValueError("unexpected row fields")
    if row["format"] != INPUT_FORMAT:
        raise ValueError(f"unexpected format {row['format']!r}")
    for key in ("program", "requested"):
        if not isinstance(row[key], str):
            raise ValueError(f"{key} must be a string")
    for key in ("call_time", "arg_count"):
        if type(row[key]) is not int or row[key] < 0:
            raise ValueError(f"{key} must be a non-negative integer")
    return [
        _address(row["function_entry"]),
        _address(row["call_address"]),
        str(row["call_time"]),
        _cell(row["target"]),
        _cell(row["target_expression"]),
        str(row["arg_count"]),
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
    print(f"adapted {count} generic indirect-call rows -> {args.output_tsv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
