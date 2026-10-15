#!/usr/bin/env python3
"""Synthetic smoke for v9 accept-only threshold selection."""
from __future__ import annotations

import numpy as np

from evaluate_nevergone_accept_error_routing_v9 import select_keep_threshold


def main() -> int:
    # Low scores contain most errors. Keeping >=0.4 reaches perfect precision while
    # losing only 2/10 of total coverage.
    scores = np.asarray([0.05, 0.10, 0.20, 0.30, 0.60, 0.70, 0.80, 0.90], dtype=float)
    correct = np.asarray([False, False, True, True, True, True, True, True], dtype=bool)
    selected, reports = select_keep_threshold(
        scores,
        correct,
        train_total=10,
        legacy_accept_count=8,
        target_precision=0.95,
        max_accept_coverage_loss=0.40,
        min_accept_examples=3,
        np=np,
    )
    assert reports
    assert selected is not None
    threshold = float(selected["threshold"])
    keep = scores >= threshold
    assert int(keep.sum()) >= 3
    assert float(correct[keep].mean()) >= 0.95

    # Impossible precision target with zero allowed coverage loss must fallback.
    selected2, _ = select_keep_threshold(
        scores,
        correct,
        train_total=10,
        legacy_accept_count=8,
        target_precision=1.0,
        max_accept_coverage_loss=0.0,
        min_accept_examples=8,
        np=np,
    )
    assert selected2 is None
    print("Nevergone accept-error routing v9 smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
