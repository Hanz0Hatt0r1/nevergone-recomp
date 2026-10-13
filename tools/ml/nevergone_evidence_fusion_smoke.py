#!/usr/bin/env python3
"""Dependency-light smoke checks for evidence-fusion summaries."""
from __future__ import annotations

from nevergone_nested_evidence_fusion_cv import (
    merge_dependency_contracts,
    summarize_fused_accept,
    summarize_rows,
)


def main() -> int:
    merged = merge_dependency_contracts(
        {"np": object(), "StratifiedGroupKFold": object()},
        {"sparse": object(), "normalize": object(), "TfidfVectorizer": object()},
    )
    assert "np" in merged
    assert "StratifiedGroupKFold" in merged
    assert "sparse" in merged
    assert "normalize" in merged
    assert "TfidfVectorizer" in merged

    rows = [
        {
            "truth": "runtime_library",
            "classifier_prediction": "engine",
            "classifier_action": "review",
            "transfer_prediction": "runtime_library",
            "transfer_trusted": True,
        },
        {
            "truth": "media_library",
            "classifier_prediction": "media_library",
            "classifier_action": "accept",
            "transfer_prediction": "game",
            "transfer_trusted": False,
        },
        {
            "truth": "game",
            "classifier_prediction": "game",
            "classifier_action": "escalate",
            "transfer_prediction": "runtime_library",
            "transfer_trusted": True,
        },
    ]
    promotion = lambda row: row["classifier_action"] != "accept" and row["transfer_trusted"]
    report = summarize_rows(rows, promotion, label_source="transfer_prediction")
    assert report["examples"] == 2
    assert abs(report["precision"] - 0.5) < 1e-12

    fused = summarize_fused_accept(rows, promotion)
    assert fused["examples"] == 3
    assert abs(fused["precision"] - (2 / 3)) < 1e-12
    print("evidence fusion smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
