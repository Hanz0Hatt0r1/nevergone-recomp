#!/usr/bin/env python3
"""Dependency-light smoke checks for conditional fusion policy selection."""
from __future__ import annotations

from nevergone_nested_conditional_fusion_cv import (
    conditional_accepts,
    select_conditional_policies,
)


def row(label, action, sim, gap, correct):
    return {
        "classifier_action": action,
        "transfer_trusted": True,
        "transfer_prediction": label,
        "transfer_similarity": sim,
        "transfer_gap": gap,
        "transfer_correct": correct,
    }


def main() -> int:
    rows = [
        row("runtime_library", "review", 0.96, 0.20, True),
        row("runtime_library", "review", 0.95, 0.18, True),
        row("runtime_library", "review", 0.93, 0.01, False),
        row("media_library", "escalate", 0.97, 0.16, True),
        row("media_library", "escalate", 0.96, 0.15, True),
    ]
    per_label = select_conditional_policies(
        rows, scope="per_label", target_precision=1.0, min_examples=2
    )
    assert per_label[("runtime_library",)]["enabled"]
    assert per_label[("media_library",)]["enabled"]
    assert conditional_accepts(rows[0], per_label, scope="per_label")
    assert not conditional_accepts(rows[2], per_label, scope="per_label")

    by_action = select_conditional_policies(
        rows, scope="per_label_action", target_precision=1.0, min_examples=2
    )
    assert by_action[("runtime_library", "review")]["enabled"]
    assert by_action[("media_library", "escalate")]["enabled"]
    print("conditional fusion smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
