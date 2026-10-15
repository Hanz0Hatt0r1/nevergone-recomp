#!/usr/bin/env python3
"""Validate Ghidra evidence linking imported enemy data to EnemyObject health fields."""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path

FORMAT = "NEVERGONE.EnemyHealthFieldConsumer/1"
MAX_HEALTH_OFFSET = 0x160
CURRENT_HEALTH_OFFSET = 0x164
INTEGER_RECORD_FIELDS = {
    0x00: "first_i32",
    0x04: "second_i32",
    0x0C: "third_i32",
    0x10: "fourth_i32",
    0x14: "fifth_i32",
    0x18: "sixth_i32",
    0x1C: "seventh_i32",
}
REQUIRED_COLUMNS = {
    "format",
    "function_entry",
    "function_name",
    "store_address",
    "store_time",
    "store_base_parameter_index",
    "store_offset",
    "value_size",
    "source_count",
    "source_parameter_index",
    "source_offset",
    "source_size",
    "source_int_to_float",
    "source_load_address",
    "source_load_time",
    "trace_status",
}


@dataclass(frozen=True)
class StoreEvidence:
    function_entry: str
    function_name: str
    store_address: str
    store_offset: int
    store_base_parameter_index: int
    value_size: int
    source_count: int
    source_parameter_index: int | None
    source_offset: int | None
    source_size: int | None
    source_int_to_float: bool | None
    trace_status: str


@dataclass(frozen=True)
class HealthFieldCandidate:
    function_entry: str
    function_name: str
    source_parameter_index: int
    source_offset: int
    record_field: str


def _parse_int(value: str, label: str) -> int:
    try:
        return int(value, 0)
    except ValueError as error:
        raise ValueError(f"invalid {label}: {value!r}") from error


def _optional_int(value: str, label: str) -> int | None:
    value = value.strip()
    if value in ("", "-"):
        return None
    return _parse_int(value, label)


def read_rows(path: Path) -> list[StoreEvidence]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None or not REQUIRED_COLUMNS.issubset(reader.fieldnames):
            missing = sorted(REQUIRED_COLUMNS.difference(reader.fieldnames or []))
            raise ValueError("missing enemy-health consumer columns: " + ", ".join(missing))

        rows: list[StoreEvidence] = []
        for line_number, raw in enumerate(reader, 2):
            if raw["format"].strip() != FORMAT:
                raise ValueError(f"line {line_number}: unsupported format {raw['format']!r}")
            source_int_to_float_raw = raw["source_int_to_float"].strip()
            if source_int_to_float_raw in ("", "-"):
                source_int_to_float = None
            elif source_int_to_float_raw in ("0", "1"):
                source_int_to_float = source_int_to_float_raw == "1"
            else:
                raise ValueError(
                    f"line {line_number}: invalid source_int_to_float {source_int_to_float_raw!r}"
                )

            rows.append(
                StoreEvidence(
                    function_entry=raw["function_entry"].strip(),
                    function_name=raw["function_name"].strip(),
                    store_address=raw["store_address"].strip(),
                    store_offset=_parse_int(raw["store_offset"].strip(), "store_offset"),
                    store_base_parameter_index=_parse_int(
                        raw["store_base_parameter_index"].strip(),
                        "store_base_parameter_index",
                    ),
                    value_size=_parse_int(raw["value_size"].strip(), "value_size"),
                    source_count=_parse_int(raw["source_count"].strip(), "source_count"),
                    source_parameter_index=_optional_int(
                        raw["source_parameter_index"], "source_parameter_index"
                    ),
                    source_offset=_optional_int(raw["source_offset"], "source_offset"),
                    source_size=_optional_int(raw["source_size"], "source_size"),
                    source_int_to_float=source_int_to_float,
                    trace_status=raw["trace_status"].strip(),
                )
            )
    return rows


def health_field_candidate(rows: list[StoreEvidence]) -> tuple[HealthFieldCandidate | None, str]:
    targets = {
        offset: [row for row in rows if row.store_offset == offset]
        for offset in (MAX_HEALTH_OFFSET, CURRENT_HEALTH_OFFSET)
    }
    if len(targets[MAX_HEALTH_OFFSET]) != 1 or len(targets[CURRENT_HEALTH_OFFSET]) != 1:
        return None, "need exactly one +0x160 and one +0x164 store"

    maximum = targets[MAX_HEALTH_OFFSET][0]
    current = targets[CURRENT_HEALTH_OFFSET][0]
    pair = (maximum, current)

    if maximum.function_entry != current.function_entry:
        return None, "health stores belong to different functions"
    if any(row.store_base_parameter_index != 0 for row in pair):
        return None, "health store destination is not parameter 0 (this)"
    if any(row.value_size != 4 for row in pair):
        return None, "health store value is not 4 bytes"
    if any(row.source_count != 1 or row.trace_status != "unique_parameter_load" for row in pair):
        return None, "health store does not have one unique parameter-load source"
    if any(row.source_parameter_index is None or row.source_parameter_index <= 0 for row in pair):
        return None, "source is not a non-this parameter"
    if any(row.source_size != 4 for row in pair):
        return None, "source load is not 4 bytes"
    if any(row.source_int_to_float is not True for row in pair):
        return None, "source path lacks recovered integer-to-float conversion"

    source_signatures = {
        (row.source_parameter_index, row.source_offset, row.source_size)
        for row in pair
    }
    if len(source_signatures) != 1:
        return None, "max/current health stores do not share one source field"

    source_parameter_index, source_offset, _ = next(iter(source_signatures))
    if source_offset is None:
        return None, "source offset is unresolved"
    record_field = INTEGER_RECORD_FIELDS.get(source_offset)
    if record_field is None:
        return None, "source offset is not an integer field in the recovered 32-byte record"

    return (
        HealthFieldCandidate(
            function_entry=maximum.function_entry,
            function_name=maximum.function_name,
            source_parameter_index=source_parameter_index,
            source_offset=source_offset,
            record_field=record_field,
        ),
        "shared unique integer source feeds both recovered health stores",
    )


def render_report(rows: list[StoreEvidence]) -> str:
    candidate, reason = health_field_candidate(rows)
    lines = [
        f"target health-store rows: {len(rows)}",
        f"candidate status: {'candidate' if candidate else 'none'}",
        f"reason: {reason}",
    ]
    if candidate is not None:
        lines.extend(
            [
                f"function: {candidate.function_entry} {candidate.function_name}",
                f"source parameter index: {candidate.source_parameter_index}",
                f"source offset: 0x{candidate.source_offset:x}",
                f"raw EnemyRecord field candidate: {candidate.record_field}",
                "semantic status: candidate only; requires independent identity/record correspondence evidence",
            ]
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tsv", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    try:
        rows = read_rows(args.tsv)
        report = render_report(rows)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
