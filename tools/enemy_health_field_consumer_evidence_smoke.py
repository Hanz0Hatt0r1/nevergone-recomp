#!/usr/bin/env python3

from __future__ import annotations

import tempfile
from pathlib import Path

from enemy_health_field_consumer_evidence import (
    FORMAT,
    health_field_candidate,
    read_rows,
    render_report,
)

HEADER = (
    "format\tfunction_entry\tfunction_name\tstore_address\tstore_time\t"
    "store_base_parameter_index\tstore_offset\tvalue_size\tsource_count\t"
    "source_parameter_index\tsource_offset\tsource_size\t"
    "source_int_to_float\tsource_load_address\tsource_load_time\ttrace_status\n"
)


def row(
    store_offset: str,
    source_offset: str,
    *,
    store_base: str = "0",
    value_size: str = "4",
    source_count: str = "1",
    source_parameter: str = "1",
    source_size: str = "4",
    int_to_float: str = "1",
    trace_status: str = "unique_parameter_load",
    store_address: str = "00300000",
) -> str:
    return (
        f"{FORMAT}\t002f0000\tEnemyObject::loadEnemyObjectFile\t{store_address}\t0\t"
        f"{store_base}\t{store_offset}\t{value_size}\t{source_count}\t"
        f"{source_parameter}\t{source_offset}\t{source_size}\t{int_to_float}\t"
        "002f0010\t1\t"
        f"{trace_status}\n"
    )


def parse(text: str):
    with tempfile.TemporaryDirectory(prefix="nevergone-enemy-health-consumer-") as temp:
        path = Path(temp) / "evidence.tsv"
        path.write_text(text, encoding="utf-8")
        return read_rows(path)


def expect_none(text: str, needle: str) -> None:
    candidate, reason = health_field_candidate(parse(text))
    assert candidate is None
    assert needle in reason, (needle, reason)


def main() -> int:
    valid = HEADER + row("0x160", "0x10") + row(
        "0x164", "0x10", store_address="00300020"
    )
    rows = parse(valid)
    candidate, reason = health_field_candidate(rows)
    assert candidate is not None, reason
    assert candidate.function_entry == "002f0000"
    assert candidate.source_parameter_index == 1
    assert candidate.source_offset == 0x10
    assert candidate.record_field == "fourth_i32"
    report = render_report(rows)
    assert "candidate status: candidate" in report
    assert "source offset: 0x10" in report
    assert "raw EnemyRecord field candidate: fourth_i32" in report
    assert "candidate only" in report

    expect_none(
        HEADER + row("0x160", "0x10", int_to_float="0") + row("0x164", "0x10"),
        "integer-to-float",
    )
    expect_none(
        HEADER + row("0x160", "0x10") + row("0x164", "0x14"),
        "do not share",
    )
    expect_none(
        HEADER + row("0x160", "0x10", source_parameter="0") + row(
            "0x164", "0x10", source_parameter="0"
        ),
        "non-this parameter",
    )
    expect_none(
        HEADER + row(
            "0x160",
            "-",
            source_count="2",
            source_parameter="-",
            source_size="-",
            int_to_float="-",
            trace_status="ambiguous_parameter_loads",
        ) + row("0x164", "0x10"),
        "one unique",
    )
    expect_none(
        HEADER + row("0x160", "0x08") + row("0x164", "0x08"),
        "not an integer field",
    )
    expect_none(
        HEADER + row("0x160", "0x10") + row("0x160", "0x10", store_address="00300004") + row("0x164", "0x10"),
        "exactly one",
    )
    expect_none(
        HEADER + row("0x160", "0x10", value_size="8") + row("0x164", "0x10"),
        "not 4 bytes",
    )

    bad_format = HEADER + row("0x160", "0x10").replace(FORMAT, "BAD/1", 1)
    try:
        parse(bad_format)
    except ValueError as error:
        assert "unsupported format" in str(error)
    else:
        raise AssertionError("expected unsupported-format failure")

    print("Enemy health field consumer evidence smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
