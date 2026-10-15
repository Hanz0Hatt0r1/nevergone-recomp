#!/usr/bin/env python3
"""Resolve structural MEPlayer isFace/PLAYERFACEDIR field candidates from Ghidra metadata."""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path

ACCESSOR_FORMAT = "NEVERGONE.GhidraAccessorReturnField/1"
STORE_FORMAT = "NEVERGONE.ParameterStoreField/1"


@dataclass(frozen=True)
class FaceFieldEvidence:
    is_face_offset: int
    player_face_dir_offset: int


def parse_hex(text: str) -> int:
    text = text.strip()
    if text.startswith("-0x"):
        return -int(text[3:], 16)
    if text.startswith("0x"):
        return int(text[2:], 16)
    raise ValueError(f"expected hexadecimal offset, got {text!r}")


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    if not rows:
        raise ValueError(f"no evidence rows in {path}")
    return rows


def resolve_is_face_offset(rows: list[dict[str, str]]) -> int:
    candidates: set[int] = set()
    for row in rows:
        if row.get("format") != ACCESSOR_FORMAT:
            continue
        if row.get("status") != "return_load_from_parameter_constant_offset":
            continue
        if row.get("load_base_parameter_index") != "0":
            continue
        offset = row.get("load_offset", "-")
        if offset != "-":
            candidates.add(parse_hex(offset))
    if len(candidates) != 1:
        raise ValueError(
            "MEPlayer::isFace field is not unique: "
            + (",".join(hex(value) for value in sorted(candidates)) if candidates else "none")
        )
    return next(iter(candidates))


def resolve_player_face_dir_offset(rows: list[dict[str, str]]) -> int:
    candidates: set[int] = set()
    for row in rows:
        if row.get("format") != STORE_FORMAT:
            continue
        if row.get("status") != "parameter_to_parameter_field":
            continue
        if row.get("destination_parameter_index") != "0":
            continue
        if row.get("source_parameter_index") != "1":
            continue
        offset = row.get("destination_offset", "-")
        if offset != "-":
            candidates.add(parse_hex(offset))
    if len(candidates) != 1:
        raise ValueError(
            "MEPlayer::setPlayerDir destination field is not unique: "
            + (",".join(hex(value) for value in sorted(candidates)) if candidates else "none")
        )
    return next(iter(candidates))


def resolve(is_face_rows: list[dict[str, str]], player_dir_rows: list[dict[str, str]]) -> FaceFieldEvidence:
    return FaceFieldEvidence(
        is_face_offset=resolve_is_face_offset(is_face_rows),
        player_face_dir_offset=resolve_player_face_dir_offset(player_dir_rows),
    )


def render(evidence: FaceFieldEvidence) -> str:
    distinct = evidence.is_face_offset != evidence.player_face_dir_offset
    return (
        "Tutorial player face field evidence\n"
        f"MEPlayer::isFace field candidate: {evidence.is_face_offset:#x}\n"
        f"MEPlayer::setPlayerDir field candidate: {evidence.player_face_dir_offset:#x}\n"
        f"fields distinct: {'true' if distinct else 'false'}\n"
        "semantic status: structural field candidates only\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("is_face_tsv", type=Path)
    parser.add_argument("player_dir_tsv", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        evidence = resolve(read_tsv(args.is_face_tsv), read_tsv(args.player_dir_tsv))
    except (OSError, ValueError) as error:
        parser.error(str(error))
    report = render(evidence)
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
