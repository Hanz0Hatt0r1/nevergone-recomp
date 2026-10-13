#!/usr/bin/env python3
from __future__ import annotations

from nevergone_similarity_transfer_cv import _summary


def main() -> int:
    correct = [True, False, True, True]
    selected = [True, True, False, True]
    report = _summary(correct, selected)
    assert report["examples"] == 3
    assert abs(report["coverage"] - 0.75) < 1e-12
    assert abs(report["precision"] - (2 / 3)) < 1e-12

    empty = _summary([True, False], [False, False])
    assert empty["examples"] == 0
    assert empty["coverage"] == 0.0
    assert empty["precision"] == 0.0

    print("nevergone similarity transfer CV smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
