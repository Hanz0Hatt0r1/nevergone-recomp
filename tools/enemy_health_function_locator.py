#!/usr/bin/env python3
"""Validate stripped-function candidates for the EnemyObject health consumer."""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path

FORMAT = "NEVERGONE.EnemyHealthFunctionCandidates/1"
REQUIRED_COLUMNS = {
    "format",
    "function_entry",
    "function_name",
    "stores_0x160",
    "stores_0x164",
}


@dataclass(frozen=True)
class FunctionCandidate:
    function_entry: str
    function_name: str
    stores_0x160: int
    stores_0x164: int


def read_candidates(path: Path) -> list[FunctionCandidate]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None or not REQUIRED_COLUMNS.issubset(reader.fieldnames):
            missing = sorted(REQUIRED_COLUMNS.difference(reader.fieldnames or []))
            raise ValueError("missing health-function locator columns: " + ", ".join(missing))

        rows: list[FunctionCandidate] = []
        seen_entries: set[str] = set()
        for line_number, raw in enumerate(reader, 2):
            if raw["format"].strip() != FORMAT:
                raise ValueError(f"line {line_number}: unsupported format {raw['format']!r}")
            entry = raw["function_entry"].strip()
            if not entry or entry == "-":
                raise ValueError(f"line {line_number}: missing function entry")
            if entry in seen_entries:
                raise ValueError(f"line {line_number}: duplicate function entry {entry}")
            seen_entries.add(entry)
            try:
                stores_160 = int(raw["stores_0x160"].strip(), 0)
                stores_164 = int(raw["stores_0x164"].strip(), 0)
            except ValueError as error:
                raise ValueError(f"line {line_number}: invalid store count") from error
            if stores_160 <= 0 or stores_164 <= 0:
                raise ValueError(
                    f"line {line_number}: candidate must store to both recovered health offsets"
                )
            rows.append(
                FunctionCandidate(
                    function_entry=entry,
                    function_name=raw["function_name"].strip(),
                    stores_0x160=stores_160,
                    stores_0x164=stores_164,
                )
            )
    return rows


def unique_candidate(
    candidates: list[FunctionCandidate],
) -> tuple[FunctionCandidate | None, str]:
    if not candidates:
        return None, "no function stores to both recovered health offsets"
    if len(candidates) != 1:
        return None, f"ambiguous function candidates: {len(candidates)}"
    return candidates[0], "exactly one function stores to both recovered health offsets"


def render_report(candidates: list[FunctionCandidate]) -> str:
    candidate, reason = unique_candidate(candidates)
    lines = [
        f"candidate functions: {len(candidates)}",
        f"locator status: {'unique' if candidate else 'none'}",
        f"reason: {reason}",
    ]
    for row in candidates:
        lines.append(
            "candidate: "
            f"{row.function_entry} {row.function_name or '-'} "
            f"stores_0x160={row.stores_0x160} stores_0x164={row.stores_0x164}"
        )
    if candidate is not None:
        lines.extend(
            [
                f"resolved function entry: {candidate.function_entry}",
                "semantic status: locator only; consumer data-flow proof still required",
            ]
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tsv", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--entry-output",
        type=Path,
        help="write the unique entry for the consumer probe; exits nonzero unless unique",
    )
    args = parser.parse_args()

    try:
        candidates = read_candidates(args.tsv)
        candidate, _ = unique_candidate(candidates)
        report = render_report(candidates)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")

    if args.entry_output is not None:
        if args.entry_output.exists():
            args.entry_output.unlink()
        if candidate is None:
            return 4
        args.entry_output.parent.mkdir(parents=True, exist_ok=True)
        args.entry_output.write_text(candidate.function_entry + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
