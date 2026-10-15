#!/usr/bin/env python3
"""Combine runtime enemy-record identity evidence with Ghidra health-consumer evidence."""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

from enemy_health_field_consumer_evidence import (
    HealthFieldCandidate,
    health_field_candidate,
    read_rows as read_health_rows,
)
from game_levels_enemy_record_evidence import (
    EnemyRecordEvidence,
    identity_field_candidates,
    parse_diagnostics,
)


@dataclass(frozen=True)
class CrossEvidenceMapping:
    identity_field: str
    base_health_field: str
    health_source_parameter_index: int
    health_source_offset: int
    known_enemy_ids: tuple[int, ...]
    global_enemy_record_count: int


def resolve_mapping(
    runtime: EnemyRecordEvidence,
    health: HealthFieldCandidate | None,
    known_enemy_ids: set[int],
) -> tuple[CrossEvidenceMapping | None, str]:
    if not known_enemy_ids:
        return None, "no independently recovered enemy IDs supplied"
    if runtime.truncated:
        return None, "runtime enemy-record report is truncated"
    if not runtime.records:
        return None, "runtime enemy-record report contains no records"

    identity_candidates = identity_field_candidates(runtime, known_enemy_ids)
    if len(identity_candidates) != 1:
        return None, (
            "identity field is not unique: "
            + (",".join(identity_candidates) if identity_candidates else "none")
        )
    if health is None:
        return None, "Ghidra consumer evidence did not yield a health-field candidate"

    identity_field = identity_candidates[0]
    if identity_field == health.record_field:
        return None, "identity and base-health evidence resolve to the same raw field"

    return (
        CrossEvidenceMapping(
            identity_field=identity_field,
            base_health_field=health.record_field,
            health_source_parameter_index=health.source_parameter_index,
            health_source_offset=health.source_offset,
            known_enemy_ids=tuple(sorted(known_enemy_ids)),
            global_enemy_record_count=runtime.total_count,
        ),
        "unique runtime identity column and independent shared health consumer source",
    )


def mapping_candidate_document(mapping: CrossEvidenceMapping) -> dict[str, object]:
    """Return the deterministic candidate handoff consumed only after review.

    This artifact intentionally says candidate rather than reviewed. Production
    code must not treat it as an enabled semantic mapping without a separate
    review/promote step.
    """

    return {
        "schema": "NEVERGONE.EnemyRecordMappingCandidate/1",
        "semantic_status": "cross-evidence-candidate",
        "identity_field": mapping.identity_field,
        "base_health_field": mapping.base_health_field,
        "health_source_parameter_index": mapping.health_source_parameter_index,
        "health_source_offset": mapping.health_source_offset,
        "known_enemy_ids": list(mapping.known_enemy_ids),
        "global_enemy_record_count": mapping.global_enemy_record_count,
    }


def render_report(
    runtime: EnemyRecordEvidence,
    health: HealthFieldCandidate | None,
    known_enemy_ids: set[int],
) -> str:
    mapping, reason = resolve_mapping(runtime, health, known_enemy_ids)
    identity_candidates = identity_field_candidates(runtime, known_enemy_ids)
    lines = [
        f"runtime global enemy records: {runtime.total_count}",
        f"runtime records omitted: {runtime.omitted_count}",
        "known enemy ids: " + (
            ",".join(str(value) for value in sorted(known_enemy_ids))
            if known_enemy_ids else "none"
        ),
        "identity field candidates: " + (
            ",".join(identity_candidates) if identity_candidates else "none"
        ),
        "Ghidra health field candidate: " + (
            health.record_field if health is not None else "none"
        ),
        f"cross-evidence status: {'candidate' if mapping else 'none'}",
        f"reason: {reason}",
    ]
    if mapping is not None:
        lines.extend(
            [
                f"identity field candidate: {mapping.identity_field}",
                f"base health field candidate: {mapping.base_health_field}",
                f"health source parameter index: {mapping.health_source_parameter_index}",
                f"health source offset: 0x{mapping.health_source_offset:x}",
                "semantic status: cross-evidence candidate; no source field is renamed automatically",
            ]
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtime_diagnostics", type=Path)
    parser.add_argument("health_consumer_tsv", type=Path)
    parser.add_argument(
        "--known-enemy-id",
        dest="known_enemy_ids",
        type=lambda value: int(value, 0),
        action="append",
        default=[],
        help="independently recovered tutorial enemy ID; repeatable",
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--mapping-output",
        type=Path,
        help=(
            "write a deterministic NEVERGONE.EnemyRecordMappingCandidate/1 JSON "
            "only when the cross-evidence mapping is unique"
        ),
    )
    args = parser.parse_args()

    try:
        runtime = parse_diagnostics(args.runtime_diagnostics.read_text(encoding="utf-8"))
        health_rows = read_health_rows(args.health_consumer_tsv)
        health, _ = health_field_candidate(health_rows)
        known_enemy_ids = set(args.known_enemy_ids)
        mapping, reason = resolve_mapping(runtime, health, known_enemy_ids)
        report = render_report(runtime, health, known_enemy_ids)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")

    if args.mapping_output is not None:
        # Never leave a stale candidate artifact behind after a failed rerun.
        try:
            args.mapping_output.unlink(missing_ok=True)
        except OSError as error:
            parser.error(str(error))
        if mapping is None:
            parser.error(f"cannot write mapping candidate: {reason}")
        args.mapping_output.parent.mkdir(parents=True, exist_ok=True)
        args.mapping_output.write_text(
            json.dumps(mapping_candidate_document(mapping), indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    print(report, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
