#!/usr/bin/env python3
"""Dependency-light smoke tests for production classifier helpers."""
from __future__ import annotations

from train_nevergone_production_classifier import (
    choose_precision_threshold,
    normalize_portable,
    route_action,
    route_action_for_class,
)
from train_nevergone_production_classifier_v3 import route_action_with_ood
from train_nevergone_production_classifier_v4 import route_action_with_contrastive_ood


def main() -> int:
    raw = "\n".join(
        [
            "ARCH=ARM32",
            "CALLCONV=__thiscall",
            "o17 COPY out=v91:4 in=const:small:0x108:4",
            "o18 RETURN in=v91:4",
        ]
    )
    portable = normalize_portable(raw)
    assert "ARCH=" not in portable
    assert "o17" not in portable
    assert "v91" not in portable
    assert "0x108" not in portable
    assert "OP COPY" in portable

    already = "OP COPY out=v:4 in=const:large:4\nOP RETURN in=v:4"
    assert normalize_portable(already) == already

    confidences = [0.99, 0.98, 0.95, 0.80, 0.70, 0.60]
    correct = [True, True, True, True, False, False]
    strict = choose_precision_threshold(confidences, correct, target_precision=1.0, min_examples=2)
    assert strict is not None and strict["examples"] == 4 and strict["threshold"] == 0.80
    relaxed = choose_precision_threshold(confidences, correct, target_precision=0.80, min_examples=2)
    assert relaxed is not None and relaxed["examples"] == 5 and relaxed["threshold"] == 0.70

    assert route_action(0.97, accept_threshold=0.95, review_threshold=0.80) == "accept"
    assert route_action(0.90, accept_threshold=0.95, review_threshold=0.80) == "review"
    assert route_action(0.50, accept_threshold=0.95, review_threshold=0.80) == "escalate"

    policies = {
        "engine": {
            "accept_threshold": 0.90,
            "review_threshold": 0.70,
            "ood_accept_threshold": 0.55,
            "contrast_accept_threshold": 0.05,
        },
        "game": {
            "accept_threshold": None,
            "review_threshold": 0.80,
            "ood_accept_threshold": None,
            "contrast_accept_threshold": None,
        },
        "media_library": {
            "accept_threshold": 0.90,
            "review_threshold": 0.70,
            "ood_accept_threshold": 0.40,
            "contrast_accept_threshold": 0.10,
        },
    }
    assert route_action_for_class("engine", 0.95, policies) == "accept"
    assert route_action_with_ood("engine", 0.95, 0.70, policies) == "accept"
    assert route_action_with_ood("engine", 0.95, 0.40, policies) == "review"

    # v4 requires predicted-family support to beat competing-class support.
    assert route_action_with_contrastive_ood("engine", 0.95, 0.70, 0.20, policies) == "accept"
    assert route_action_with_contrastive_ood("engine", 0.95, 0.70, 0.01, policies) == "review"
    assert route_action_with_contrastive_ood("engine", 0.95, 0.40, 0.20, policies) == "review"
    assert route_action_with_contrastive_ood("media_library", 0.95, 0.70, -0.01, policies) == "review"
    assert route_action_with_contrastive_ood("unknown", 1.0, 1.0, 1.0, policies) == "escalate"

    print("production classifier smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
