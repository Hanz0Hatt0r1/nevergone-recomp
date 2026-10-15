#!/usr/bin/env python3
"""Synthetic smoke for v5.7 constrained GAME meta calibration."""
from __future__ import annotations

from evaluate_nevergone_constrained_game_meta_v57 import apply_meta, select_threshold, threshold_grid
from evaluate_nevergone_game_meta_verifier_v56 import safe_evidence_keys
from train_nevergone_production_classifier import _require_dependencies


def main() -> int:
    deps = _require_dependencies()
    np = deps["np"]

    assert threshold_grid()[0] == 0.0
    safe = safe_evidence_keys({
        "a": {
            "receiver_resolved_ratio": 0.5,
            "class_name": 1.0,
            "vtable_identity": 2.0,
            "source_entry": 3.0,
            "address_bucket": 4.0,
        }
    })
    assert "receiver_resolved_ratio" in safe
    assert "class_name" not in safe
    assert "vtable_identity" not in safe
    assert "source_entry" not in safe
    assert "address_bucket" not in safe

    labels = np.asarray([
        "game", "game", "engine", "engine", "game", "engine",
        "runtime_library", "media_library",
    ], dtype=object)
    base = np.asarray([
        "game", "game", "game", "game", "game", "game",
        "runtime_library", "media_library",
    ], dtype=object)
    candidates = np.asarray([0, 1, 2, 3, 4, 5], dtype=np.int64)
    keep = np.asarray([0.95, 0.85, 0.10, 0.20, 0.75, 0.30], dtype=np.float64)

    threshold, reports = select_threshold(
        labels,
        base,
        candidates,
        keep,
        deps=deps,
        np=np,
        max_balanced_loss=0.003,
        max_game_recall_loss=0.03,
        target_game_precision=0.80,
    )
    corrected = apply_meta(base, candidates, keep, threshold, np)
    assert threshold > 0.0
    assert corrected.tolist()[:6] == ["game", "game", "engine", "engine", "game", "engine"]
    selected = next(row for row in reports if row["threshold"] == threshold)
    assert selected["eligible"]
    assert selected["game_precision"] == 1.0
    assert selected["game_recall"] == 1.0

    print("ProductionClassifier v5.7 constrained GAME meta smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
