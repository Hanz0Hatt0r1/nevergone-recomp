#!/usr/bin/env python3
"""Extract raw LoadGL_Global enemy records from Nevergone runtime diagnostics.

This tool deliberately does not assign semantic names to unresolved fields. It
can compare integer columns against independently recovered enemy IDs, but a
matching value is reported only as a mapping candidate, never as proof of the
base-health field or any other unresolved meaning.
"""

from __future__ import annotations

import argparse
import csv
import math
import re
from dataclasses import dataclass
from pathlib import Path

COUNT_RE = re.compile(r"^global enemy records:\s*(\d+)\s*$", re.MULTILINE)
OMITTED_RE = re.compile(r"^global enemy records omitted:\s*(\d+)\s*$", re.MULTILINE)
RECORD_RE = re.compile(
    r"^global enemy\[(\d+)\]:\s*"
    r"(-?\d+),(-?\d+),([^,\n]+),"
    r"(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)\s*$",
    re.MULTILINE,
)

INTEGER_FIELDS = (
    "first_i32",
    "second_i32",
    "third_i32",
    "fourth_i32",
    "fifth_i32",
    "sixth_i32",
    "seventh_i32",
)
TSV_FIELDS = (
    "record_index",
    "first_i32",
    "second_i32",
    "first_float",
    "third_i32",
    "fourth_i32",
    "fifth_i32",
    "sixth_i32",
    "seventh_i32",
)


@dataclass(frozen=True)
class EnemyRecord:
    record_index: int
    first_i32: int
    second_i32: int
    first_float: float
    third_i32: int
    fourth_i32: int
    fifth_i32: int
    sixth_i32: int
    seventh_i32: int


@dataclass(frozen=True)
class EnemyRecordEvidence:
    total_count: int
    omitted_count: int
    records: tuple[EnemyRecord, ...]

    @property
    def truncated(self) -> bool:
        return self.omitted_count != 0


def _single_int(pattern: re.Pattern[str], text: str, label: str) -> int:
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        raise ValueError(f"expected exactly one {label}, found {len(matches)}")
    return int(matches[0].group(1))


def parse_diagnostics(text: str) -> EnemyRecordEvidence:
    total_count = _single_int(COUNT_RE, text, "global enemy record count")
    record_matches = list(RECORD_RE.finditer(text))

    records: list[EnemyRecord] = []
    for expected_index, match in enumerate(record_matches):
        record_index = int(match.group(1))
        if record_index != expected_index:
            raise ValueError(
                "global enemy record indices are not contiguous from zero: "
                f"expected {expected_index}, found {record_index}"
            )
        try:
            first_float = float(match.group(4).strip())
        except ValueError as error:
            raise ValueError(
                f"global enemy[{record_index}] has invalid float field"
            ) from error
        if not math.isfinite(first_float):
            raise ValueError(
                f"global enemy[{record_index}] has non-finite float field"
            )
        records.append(
            EnemyRecord(
                record_index=record_index,
                first_i32=int(match.group(2)),
                second_i32=int(match.group(3)),
                first_float=first_float,
                third_i32=int(match.group(5)),
                fourth_i32=int(match.group(6)),
                fifth_i32=int(match.group(7)),
                sixth_i32=int(match.group(8)),
                seventh_i32=int(match.group(9)),
            )
        )

    omitted_matches = list(OMITTED_RE.finditer(text))
    if len(omitted_matches) > 1:
        raise ValueError(
            f"expected at most one omitted-record count, found {len(omitted_matches)}"
        )
    omitted_count = int(omitted_matches[0].group(1)) if omitted_matches else 0

    if len(records) + omitted_count != total_count:
        raise ValueError(
            "global enemy diagnostic accounting mismatch: "
            f"reported={len(records)} omitted={omitted_count} total={total_count}"
        )
    if omitted_count == 0 and len(records) != total_count:
        raise ValueError("untruncated global enemy diagnostic is incomplete")
    if omitted_count != 0 and len(records) == 0:
        raise ValueError("truncated global enemy diagnostic contains no records")

    return EnemyRecordEvidence(
        total_count=total_count,
        omitted_count=omitted_count,
        records=tuple(records),
    )


def identity_field_candidates(
    evidence: EnemyRecordEvidence,
    known_enemy_ids: set[int],
) -> tuple[str, ...]:
    if not known_enemy_ids or not evidence.records:
        return ()
    candidates: list[str] = []
    for field in INTEGER_FIELDS:
        observed = {getattr(record, field) for record in evidence.records}
        if known_enemy_ids.issubset(observed):
            candidates.append(field)
    return tuple(candidates)


def write_tsv(evidence: EnemyRecordEvidence, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(TSV_FIELDS)
        for record in evidence.records:
            writer.writerow(
                (
                    record.record_index,
                    record.first_i32,
                    record.second_i32,
                    format(record.first_float, ".9g"),
                    record.third_i32,
                    record.fourth_i32,
                    record.fifth_i32,
                    record.sixth_i32,
                    record.seventh_i32,
                )
            )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("diagnostics", type=Path)
    parser.add_argument(
        "--known-enemy-id",
        dest="known_enemy_ids",
        type=lambda value: int(value, 0),
        action="append",
        default=[],
        help="independently recovered enemy ID to compare against integer columns; repeatable",
    )
    parser.add_argument("--output", type=Path, help="optional TSV output path")
    args = parser.parse_args()

    try:
        text = args.diagnostics.read_text(encoding="utf-8")
        evidence = parse_diagnostics(text)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    if args.output is not None:
        write_tsv(evidence, args.output)

    known_ids = set(args.known_enemy_ids)
    candidates = identity_field_candidates(evidence, known_ids)

    print(f"global enemy records: {evidence.total_count}")
    print(f"records available in diagnostics: {len(evidence.records)}")
    print(f"records omitted by bounded report: {evidence.omitted_count}")
    if known_ids:
        ids_text = ",".join(str(value) for value in sorted(known_ids))
        print(f"known enemy ids checked: {ids_text}")
        if candidates:
            print("identity field candidates: " + ",".join(candidates))
        else:
            print("identity field candidates: none")
        if evidence.truncated:
            print("identity candidate coverage: incomplete (diagnostics truncated)")
        else:
            print("identity candidate coverage: complete for reported array")
    else:
        print("identity field candidates: not evaluated (no known IDs supplied)")
    if args.output is not None:
        print(f"TSV: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
