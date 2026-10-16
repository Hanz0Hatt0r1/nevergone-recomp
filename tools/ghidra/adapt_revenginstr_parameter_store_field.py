#!/usr/bin/env python3
"""Adapt the generic parameter-store TSV to the established Nevergone contract."""
from __future__ import annotations

import argparse
import csv
from pathlib import Path

GENERIC_FORMAT = "REVENGINSTR.GhidraParameterStoreField/1"
NEVERGONE_FORMAT = "NEVERGONE.ParameterStoreField/1"
FIELDS = (
    "format",
    "function_entry",
    "function_name",
    "store_address",
    "store_time",
    "destination_parameter_index",
    "destination_offset",
    "destination_pattern",
    "source_parameter_index",
    "value_size",
    "status",
)


def adapt_file(source: Path, destination: Path) -> None:
    rows: list[dict[str, str]] = []
    with source.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames != list(FIELDS):
            raise ValueError(f"{source}: unexpected parameter-store TSV header")
        for line_no, row in enumerate(reader, 2):
            if None in row or any(value is None for value in row.values()):
                raise ValueError(f"{source}:{line_no}: malformed parameter-store row")
            if row["format"] != GENERIC_FORMAT:
                raise ValueError(f"{source}:{line_no}: expected {GENERIC_FORMAT}")
            row["format"] = NEVERGONE_FORMAT
            rows.append(row)

    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    try:
        adapt_file(args.source, args.destination)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
