#!/usr/bin/env python3
"""Dependency-light smoke tests for production classifier helpers."""
from __future__ import annotations

from train_nevergone_production_classifier import (
    choose_precision_threshold,
    normalize_portable,
    route_action,
)


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
    strict = choose_precision_threshold(
        confidences,
        correct,
        target_precision=1.0,
        min_examples=2,
    )
    assert strict is not None
    assert strict["examples"] == 4
    assert strict["threshold"] == 0.80

    relaxed = choose_precision_threshold(
        confidences,
        correct,
        target_precision=0.80,
        min_examples=2,
    )
    assert relaxed is not None
    assert relaxed["examples"] == 5
    assert relaxed["threshold"] == 0.70

    assert route_action(0.97, accept_threshold=0.95, review_threshold=0.80) == "accept"
    assert route_action(0.90, accept_threshold=0.95, review_threshold=0.80) == "review"
    assert route_action(0.50, accept_threshold=0.95, review_threshold=0.80) == "escalate"
    assert route_action(0.99, accept_threshold=None, review_threshold=None) == "escalate"

    print("production classifier smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
