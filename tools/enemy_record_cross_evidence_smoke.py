#!/usr/bin/env python3

from __future__ import annotations

from enemy_health_field_consumer_evidence import HealthFieldCandidate
from enemy_record_cross_evidence import render_report, resolve_mapping
from game_levels_enemy_record_evidence import parse_diagnostics


def runtime_text(*, ambiguous: bool = False, truncated: bool = False) -> str:
    second_a = 3 if ambiguous else 100
    second_b = 21 if ambiguous else 200
    body = (
        "global enemy records: 3\n"
        f"global enemy[0]: 3,{second_a},1.5,7,70,700,7000,70000\n"
        f"global enemy[1]: 21,{second_b},2.5,8,80,800,8000,80000\n"
        "global enemy[2]: 99,300,3.5,9,90,900,9000,90000\n"
    )
    if not truncated:
        return body
    return (
        "global enemy records: 4\n"
        f"global enemy[0]: 3,{second_a},1.5,7,70,700,7000,70000\n"
        f"global enemy[1]: 21,{second_b},2.5,8,80,800,8000,80000\n"
        "global enemy[2]: 99,300,3.5,9,90,900,9000,90000\n"
        "global enemy records omitted: 1\n"
    )


def health(field: str = "fourth_i32", offset: int = 0x10) -> HealthFieldCandidate:
    return HealthFieldCandidate(
        function_entry="002f0000",
        function_name="EnemyObject::loadEnemyObjectFile",
        source_parameter_index=1,
        source_offset=offset,
        record_field=field,
    )


def expect_none(runtime, candidate, ids, needle: str) -> None:
    mapping, reason = resolve_mapping(runtime, candidate, ids)
    assert mapping is None
    assert needle in reason, (needle, reason)


def main() -> int:
    runtime = parse_diagnostics(runtime_text())
    mapping, reason = resolve_mapping(runtime, health(), {3, 21})
    assert mapping is not None, reason
    assert mapping.identity_field == "first_i32"
    assert mapping.base_health_field == "fourth_i32"
    assert mapping.health_source_parameter_index == 1
    assert mapping.health_source_offset == 0x10
    assert mapping.known_enemy_ids == (3, 21)
    assert mapping.global_enemy_record_count == 3

    report = render_report(runtime, health(), {3, 21})
    assert "identity field candidates: first_i32" in report
    assert "Ghidra health field candidate: fourth_i32" in report
    assert "cross-evidence status: candidate" in report
    assert "no source field is renamed automatically" in report

    expect_none(runtime, health(), set(), "no independently recovered")
    expect_none(
        parse_diagnostics(runtime_text(truncated=True)),
        health(),
        {3, 21},
        "truncated",
    )
    expect_none(
        parse_diagnostics(runtime_text(ambiguous=True)),
        health(),
        {3, 21},
        "not unique",
    )
    expect_none(runtime, None, {3, 21}, "did not yield")
    expect_none(
        runtime,
        health(field="first_i32", offset=0x00),
        {3, 21},
        "same raw field",
    )
    expect_none(runtime, health(), {12345}, "not unique")

    print("Enemy record cross-evidence smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
