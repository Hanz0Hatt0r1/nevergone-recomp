#!/usr/bin/env python3
"""Synthetic smoke for v8 monotone correctness routing."""
from __future__ import annotations

import numpy as np

from evaluate_nevergone_monotone_correctness_routing_v8 import (
    monotone_actions,
    select_monotone_thresholds,
)


def main() -> int:
    legacy = [
        "accept", "accept", "accept", "accept", "accept",
        "review", "review", "escalate", "escalate", "accept",
    ]
    scores = np.asarray([0.99, 0.95, 0.90, 0.20, 0.10, 0.85, 0.15, 0.99, 0.05, 0.88])
    correct = np.asarray([True, True, True, False, False, True, False, False, False, True])

    actions = monotone_actions(legacy, scores, 0.80, 0.30)
    assert actions == [
        "accept", "accept", "accept", "escalate", "escalate",
        "review", "escalate", "escalate", "escalate", "accept",
    ]

    order = {"accept": 0, "review": 1, "escalate": 2}
    assert all(order[new] >= order[old] for old, new in zip(legacy, actions))

    selected, reports = select_monotone_thresholds(
        legacy,
        scores,
        correct,
        np=np,
        target_accept_precision=1.0,
        target_review_precision=0.5,
        max_accept_coverage_loss=0.25,
        min_accept_examples=2,
    )
    assert reports
    assert selected is not None
    routed = monotone_actions(
        legacy,
        scores,
        float(selected["accept_threshold"]),
        float(selected["review_threshold"]),
    )
    accepted = [i for i, action in enumerate(routed) if action == "accept"]
    assert accepted
    assert all(correct[i] for i in accepted)
    assert all(order[new] >= order[old] for old, new in zip(legacy, routed))

    print("NEVERGONE.MonotoneCorrectnessRoutingV8 smoke: ok")
    print("selected:", selected)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
