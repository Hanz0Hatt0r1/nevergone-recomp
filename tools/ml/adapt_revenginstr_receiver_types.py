#!/usr/bin/env python3
"""Preserve the Nevergone receiver TSV format around shared Ghidra output."""
from __future__ import annotations

import argparse
from pathlib import Path

INPUT_FORMAT = "REVENGINSTR.GhidraIndirectReceiverType/1"
OUTPUT_FORMAT = "NEVERGONE.GhidraIndirectReceiverType/2"


def adapt(source: Path, output: Path) -> int:
    output.parent.mkdir(parents=True, exist_ok=True)
    rows = 0
    with source.open("r", encoding="utf-8", newline="") as reader:
        header = reader.readline()
        if header.rstrip("\r\n").split("\t")[0] != "format":
            raise ValueError(f"{source}: expected format column in TSV header")
        with output.open("w", encoding="utf-8", newline="") as writer:
            writer.write(header)
            for line_no, raw in enumerate(reader, 2):
                if not raw.strip():
                    writer.write(raw)
                    continue
                value, separator, rest = raw.partition("\t")
                if not separator:
                    raise ValueError(f"{source}:{line_no}: expected tab-separated receiver row")
                if value != INPUT_FORMAT:
                    raise ValueError(
                        f"{source}:{line_no}: expected {INPUT_FORMAT}, got {value!r}"
                    )
                writer.write(OUTPUT_FORMAT + separator + rest)
                rows += 1
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    rows = adapt(args.input, args.output)
    print("input format:", INPUT_FORMAT)
    print("output format:", OUTPUT_FORMAT)
    print("rows:", rows)
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
