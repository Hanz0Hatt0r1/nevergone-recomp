#!/usr/bin/env python3
"""Synthetic smoke for the v6 GAME meta routing gate."""
from __future__ import annotations

from evaluate_nevergone_game_meta_routing_v6 import (
    apply_thresholds,
    select_thresholds,
    summarize_actions,
)


def make_rows():
    rows = []
    # Three pseudo-folds. Wrong GAME accepts have low trust; correct GAME accepts high trust.
    for fold in (1, 2, 3):
        rows.extend(
            [
                {
                    "id": f"g-ok-{fold}",
                    "fold": fold,
                    "truth": "game",
                    "prediction": "game",
                    "correct": True,
                    "base_action": "accept",
                    "meta_keep_game_probability": 0.90,
                },
                {
                    "id": f"g-bad-{fold}",
                    "fold": fold,
                    "truth": "engine",
                    "prediction": "game",
                    "correct": False,
                    "base_action": "accept",
                    "meta_keep_game_probability": 0.10,
                },
                {
                    "id": f"g-review-{fold}",
                    "fold": fold,
                    "truth": "game",
                    "prediction": "game",
                    "correct": True,
                    "base_action": "review",
                    "meta_keep_game_probability": 0.75,
                },
                {
                    "id": f"e-ok-{fold}",
                    "fold": fold,
                    "truth": "engine",
                    "prediction": "engine",
                    "correct": True,
                    "base_action": "accept",
                    "meta_keep_game_probability": 0.05,
                },
            ]
        )
    return rows


def main() -> int:
    rows = make_rows()
    selected, reports = select_thresholds(
        rows,
        target_accept_precision=0.95,
        target_review_precision=0.50,
        max_accept_coverage_loss=0.30,
    )
    assert reports
    assert selected["fallback"] is False
    assert selected["accept_threshold"] > 0.10
    assert selected["review_threshold"] <= selected["accept_threshold"]

    routed = apply_thresholds(
        rows,
        selected["accept_threshold"],
        selected["review_threshold"],
    )
    summary = summarize_actions(rows, routed)
    assert summary["accept"]["precision"] >= 0.95
    # Non-GAME routing is untouched even with a low meta score.
    engine_index = next(i for i, row in enumerate(rows) if row["id"] == "e-ok-1")
    assert routed[engine_index] == "accept"
    # The low-trust false GAME accept must not remain accepted.
    bad_index = next(i for i, row in enumerate(rows) if row["id"] == "g-bad-1")
    assert routed[bad_index] != "accept"

    print("Production routing v6 GAME meta gate smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
