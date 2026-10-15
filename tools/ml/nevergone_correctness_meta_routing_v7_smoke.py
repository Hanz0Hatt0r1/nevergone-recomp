#!/usr/bin/env python3
"""Synthetic smoke checks for v7 all-class correctness meta routing."""
from __future__ import annotations

import numpy as np

from evaluate_nevergone_correctness_meta_routing_v7 import (
    accept_attribution,
    actions_from_scores,
    fit_meta_models,
    predict_meta_models,
    select_meta_thresholds,
    summarize_actions,
)


def main() -> int:
    rng = np.random.default_rng(7)
    n = 320
    predictions = np.asarray(
        ["game", "engine", "runtime_library", "media_library"] * (n // 4),
        dtype=object,
    )
    matrix = rng.normal(size=(n, 8))
    latent = 1.7 * matrix[:, 0] - 0.9 * matrix[:, 1]
    latent += np.asarray(
        [
            -0.5 if value == "game" else (-0.25 if value == "runtime_library" else 0.4)
            for value in predictions
        ]
    )
    targets = (latent + rng.normal(scale=0.7, size=n) > -0.25).astype(np.int64)

    models = fit_meta_models(
        matrix,
        predictions,
        targets,
        seed=7,
        min_class_examples=30,
    )
    scores = predict_meta_models(models, matrix, predictions, np)
    assert scores.shape == (n,)
    assert float(scores[targets == 1].mean()) > float(scores[targets == 0].mean())

    selected, reports = select_meta_thresholds(
        scores,
        targets.astype(bool),
        np=np,
        target_accept_precision=0.80,
        target_review_precision=0.60,
        min_accept_examples=20,
    )
    assert reports
    assert selected is not None
    assert selected["accept_threshold"] >= selected["review_threshold"]

    actions = actions_from_scores(
        scores,
        float(selected["accept_threshold"]),
        float(selected["review_threshold"]),
    )
    summary = summarize_actions(actions, targets.astype(bool), np)
    assert summary["accept"]["examples"] >= 20
    assert summary["accept"]["precision"] >= 0.80

    rows = [
        {"prediction": str(predictions[index]), "correct": bool(targets[index])}
        for index in range(n)
    ]
    attribution = accept_attribution(rows, actions)
    assert set(attribution) == {"game", "engine", "runtime_library", "media_library"}
    assert sum(value["examples"] for value in attribution.values()) == summary["accept"]["examples"]

    print("v7 correctness meta routing smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
