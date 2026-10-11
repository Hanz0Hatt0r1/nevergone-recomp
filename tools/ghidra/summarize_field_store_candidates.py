#!/usr/bin/env python3
"""Validate and summarize bounded field-store metadata exported from Ghidra."""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path

REQUIRED_COLUMNS = {
    "function_entry",
    "function_name",
    "store_address",
    "store_time",
    "field_offset_hex",
    "address_opcode",
    "address_op_address",
    "address_op_time",
    "base_token",
    "value_token",
    "value_is_constant",
    "value_constant_hex",
}


@dataclass(frozen=True)
class Candidate:
    function_entry: str
    function_name: str
    store_address: str
    store_time: int
    field_offset: int
    address_opcode: str
    base_token: str
    value_token: str
    value_is_constant: bool
    value_constant: int | None


def _parse_hex(value: str, field: str) -> int:
    try:
        parsed = int(value, 16)
    except ValueError as error:
        raise ValueError(f"invalid {field}: {value!r}") from error
    if parsed < 0:
        raise ValueError(f"negative {field}: {value!r}")
    return parsed


def read_candidates(path: Path, expected_offset: int | None = None) -> list[Candidate]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None or not REQUIRED_COLUMNS.issubset(reader.fieldnames):
            missing = sorted(REQUIRED_COLUMNS.difference(reader.fieldnames or []))
            raise ValueError("missing field-store columns: " + ", ".join(missing))

        candidates: list[Candidate] = []
        seen: set[tuple[str, int]] = set()
        for raw in reader:
            field_offset = _parse_hex(raw["field_offset_hex"].strip(), "field offset")
            if expected_offset is not None and field_offset != expected_offset:
                raise ValueError(
                    f"unexpected field offset 0x{field_offset:x}; expected 0x{expected_offset:x}"
                )
            try:
                store_time = int(raw["store_time"], 10)
            except ValueError as error:
                raise ValueError(f"invalid store time: {raw['store_time']!r}") from error
            if store_time < 0:
                raise ValueError("store time must be non-negative")

            constant_flag = raw["value_is_constant"].strip()
            if constant_flag not in ("0", "1"):
                raise ValueError(f"invalid constant flag: {constant_flag!r}")
            is_constant = constant_flag == "1"
            constant_cell = raw["value_constant_hex"].strip()
            if is_constant:
                value_constant = _parse_hex(constant_cell, "constant value")
            else:
                if constant_cell not in ("", "-"):
                    raise ValueError("non-constant row carries a constant value")
                value_constant = None

            candidate = Candidate(
                function_entry=raw["function_entry"].strip(),
                function_name=raw["function_name"].strip(),
                store_address=raw["store_address"].strip(),
                store_time=store_time,
                field_offset=field_offset,
                address_opcode=raw["address_opcode"].strip(),
                base_token=raw["base_token"].strip(),
                value_token=raw["value_token"].strip(),
                value_is_constant=is_constant,
                value_constant=value_constant,
            )
            if not candidate.function_entry or not candidate.function_name or not candidate.store_address:
                raise ValueError("empty function/store identity")
            if candidate.address_opcode not in ("PTRSUB", "INT_ADD"):
                raise ValueError(f"unexpected address opcode: {candidate.address_opcode!r}")
            key = (candidate.store_address, candidate.store_time)
            if key in seen:
                raise ValueError(f"duplicate store candidate: {key}")
            seen.add(key)
            candidates.append(candidate)
    return candidates


def render_summary(candidates: list[Candidate]) -> str:
    constant_rows = [row for row in candidates if row.value_is_constant]
    zero_rows = [row for row in constant_rows if row.value_constant == 0]
    lines = [
        f"candidate_stores={len(candidates)}",
        f"constant_stores={len(constant_rows)}",
        f"constant_zero_stores={len(zero_rows)}",
    ]
    for index, row in enumerate(candidates):
        value = f"0x{row.value_constant:x}" if row.value_is_constant else "derived"
        lines.append(
            f"candidate[{index}]={row.function_entry}|{row.function_name}|"
            f"{row.store_address}:{row.store_time}|value={value}|base={row.base_token}"
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tsv", type=Path)
    parser.add_argument("--field-offset", default="0x294")
    args = parser.parse_args()
    expected_offset = _parse_hex(args.field_offset, "expected field offset")
    candidates = read_candidates(args.tsv, expected_offset)
    print(render_summary(candidates), end="")
    return 0 if candidates else 2


if __name__ == "__main__":
    raise SystemExit(main())
