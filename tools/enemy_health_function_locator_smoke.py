#!/usr/bin/env python3

from __future__ import annotations

import tempfile
from pathlib import Path

from enemy_health_function_locator import FORMAT, read_candidates, render_report, unique_candidate

HEADER = "format\tfunction_entry\tfunction_name\tstores_0x160\tstores_0x164\n"


def row(entry: str, name: str = "FUN_test", stores_160: int = 1, stores_164: int = 1) -> str:
    return f"{FORMAT}\t{entry}\t{name}\t{stores_160}\t{stores_164}\n"


def parse(text: str):
    with tempfile.TemporaryDirectory(prefix="nevergone-health-locator-") as temp:
        path = Path(temp) / "candidates.tsv"
        path.write_text(text, encoding="utf-8")
        return read_candidates(path)


def expect_error(text: str, needle: str) -> None:
    try:
        parse(text)
    except ValueError as error:
        assert needle in str(error), (needle, str(error))
        return
    raise AssertionError(f"expected ValueError containing {needle!r}")


def main() -> int:
    candidates = parse(HEADER + row("002b1000", "FUN_002b1000"))
    candidate, reason = unique_candidate(candidates)
    assert candidate is not None, reason
    assert candidate.function_entry == "002b1000"
    assert candidate.stores_0x160 == 1
    assert candidate.stores_0x164 == 1
    report = render_report(candidates)
    assert "locator status: unique" in report
    assert "resolved function entry: 002b1000" in report
    assert "consumer data-flow proof still required" in report

    candidate, reason = unique_candidate([])
    assert candidate is None
    assert "no function" in reason

    candidates = parse(
        HEADER
        + row("002b1000", "FUN_002b1000")
        + row("002b2000", "EnemyObject::other", 2, 1)
    )
    candidate, reason = unique_candidate(candidates)
    assert candidate is None
    assert "ambiguous" in reason

    expect_error(HEADER + row("002b1000", stores_160=0), "both recovered")
    expect_error(
        HEADER + row("002b1000") + row("002b1000", "duplicate"),
        "duplicate function entry",
    )
    expect_error(
        HEADER + row("002b1000").replace(FORMAT, "BAD/1", 1),
        "unsupported format",
    )
    expect_error(
        "format\tfunction_entry\tfunction_name\tstores_0x160\n",
        "missing health-function locator columns",
    )

    print("Enemy health function locator smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
