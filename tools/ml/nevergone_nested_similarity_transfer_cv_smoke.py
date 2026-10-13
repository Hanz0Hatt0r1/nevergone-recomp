#!/usr/bin/env python3
from __future__ import annotations

from nevergone_nested_similarity_transfer_cv import policy_accepts, select_transfer_policy


def main() -> int:
    rows = []
    for _ in range(12):
        rows.append({
            "prediction": "game",
            "similarity": 0.95,
            "gap": 0.10,
            "correct": True,
        })
    for i in range(8):
        rows.append({
            "prediction": "game",
            "similarity": 0.80,
            "gap": 0.02,
            "correct": i < 2,
        })
    for i in range(12):
        rows.append({
            "prediction": "engine",
            "similarity": 0.96,
            "gap": 0.20,
            "correct": i < 6,
        })

    policies = select_transfer_policy(rows, target_precision=0.95, min_examples=10)
    assert policies["game"]["enabled"] is True
    assert policies["engine"]["enabled"] is False
    assert policy_accepts({"prediction": "game", "similarity": 0.96, "gap": 0.11}, policies)
    assert not policy_accepts({"prediction": "game", "similarity": 0.79, "gap": 0.01}, policies)
    assert not policy_accepts({"prediction": "engine", "similarity": 1.0, "gap": 1.0}, policies)
    print("nevergone nested similarity transfer smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
