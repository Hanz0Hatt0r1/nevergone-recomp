#!/usr/bin/env python3

from __future__ import annotations

import tempfile
from pathlib import Path

from game_levels_enemy_record_evidence import (
    identity_field_candidates,
    parse_diagnostics,
    write_tsv,
)


def expect_error(text: str, needle: str) -> None:
    try:
        parse_diagnostics(text)
    except ValueError as error:
        assert needle in str(error), (needle, str(error))
        return
    raise AssertionError(f"expected ValueError containing {needle!r}")


def main() -> int:
    diagnostics = """prefix
GameLevels runtime model
state: ready
global enemy records: 3
global enemy[0]: 3,100,1.5,7,70,700,7000,70000
global enemy[1]: 21,200,2.5,8,80,800,8000,80000
global enemy[2]: 99,300,-3.25,9,90,900,9000,90000
suffix
"""
    evidence = parse_diagnostics(diagnostics)
    assert evidence.total_count == 3
    assert evidence.omitted_count == 0
    assert not evidence.truncated
    assert len(evidence.records) == 3
    assert evidence.records[0].first_i32 == 3
    assert evidence.records[1].first_i32 == 21
    assert evidence.records[2].first_float == -3.25
    assert identity_field_candidates(evidence, {3, 21}) == ("first_i32",)
    assert identity_field_candidates(evidence, {3}) == ("first_i32",)
    assert identity_field_candidates(evidence, {3, 12345}) == ()

    with tempfile.TemporaryDirectory(prefix="nevergone-enemy-evidence-") as temp:
        output = Path(temp) / "records.tsv"
        write_tsv(evidence, output)
        assert output.read_text(encoding="utf-8") == (
            "record_index\tfirst_i32\tsecond_i32\tfirst_float\tthird_i32\t"
            "fourth_i32\tfifth_i32\tsixth_i32\tseventh_i32\n"
            "0\t3\t100\t1.5\t7\t70\t700\t7000\t70000\n"
            "1\t21\t200\t2.5\t8\t80\t800\t8000\t80000\n"
            "2\t99\t300\t-3.25\t9\t90\t900\t9000\t90000\n"
        )

    truncated = parse_diagnostics(
        "global enemy records: 18\n"
        + "\n".join(
            f"global enemy[{index}]: {index},0,0,0,0,0,0,0"
            for index in range(16)
        )
        + "\nglobal enemy records omitted: 2\n"
    )
    assert truncated.total_count == 18
    assert len(truncated.records) == 16
    assert truncated.omitted_count == 2
    assert truncated.truncated

    expect_error(
        "global enemy records: 2\n"
        "global enemy[1]: 3,0,0,0,0,0,0,0\n"
        "global enemy[0]: 21,0,0,0,0,0,0,0\n",
        "indices are not contiguous",
    )
    expect_error(
        "global enemy records: 2\n"
        "global enemy[0]: 3,0,0,0,0,0,0,0\n",
        "accounting mismatch",
    )
    expect_error(
        "global enemy records: 1\n"
        "global enemy[0]: 3,0,nan,0,0,0,0,0\n",
        "non-finite float",
    )
    expect_error(
        "global enemy records: 18\n"
        "global enemy[0]: 3,0,0,0,0,0,0,0\n"
        "global enemy records omitted: 1\n",
        "accounting mismatch",
    )

    print("GameLevels enemy record evidence extractor smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
