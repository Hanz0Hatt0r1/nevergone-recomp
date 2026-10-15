#!/usr/bin/env python3
from __future__ import annotations

import json

import numpy as np

from evaluate_nevergone_game_meta_verifier_v56 import (
    CLASSES,
    apply_meta,
    build_meta_matrix,
    make_meta_model,
    safe_evidence_keys,
)


def main() -> int:
    ids = [f"f{i}" for i in range(8)]
    evidence = {
        "f0": {"receiver.resolved_ratio": 0.95, "callgraph.fan_in": 8.0, "class_name": "SecretGameClass"},
        "f1": {"receiver.resolved_ratio": 0.90, "callgraph.fan_in": 7.0},
        "f2": {"receiver.resolved_ratio": 0.85, "callgraph.fan_in": 6.0},
        "f3": {"receiver.resolved_ratio": 0.80, "callgraph.fan_in": 5.0},
        "f4": {"receiver.resolved_ratio": 0.10, "callgraph.fan_in": 1.0, "vtable_identity": "0x1234"},
        "f5": {"receiver.resolved_ratio": 0.15, "callgraph.fan_in": 1.0},
        "f6": {"receiver.resolved_ratio": 0.20, "callgraph.fan_in": 2.0},
        "f7": {"receiver.resolved_ratio": 0.05, "callgraph.fan_in": 0.0},
    }
    keys = safe_evidence_keys(evidence)
    serialized_keys = json.dumps(keys)
    assert "class_name" not in serialized_keys
    assert "vtable_identity" not in serialized_keys
    assert "receiver.resolved_ratio" in keys

    game = CLASSES.index("game")
    engine = CLASSES.index("engine")
    probs = np.zeros((8, len(CLASSES)), dtype=np.float64)
    probs[:, game] = [0.62, 0.66, 0.61, 0.64, 0.58, 0.60, 0.57, 0.59]
    probs[:, engine] = [0.30, 0.26, 0.31, 0.28, 0.34, 0.32, 0.35, 0.33]
    probs[:, CLASSES.index("runtime_library")] = 0.04
    probs[:, CLASSES.index("media_library")] = 1.0 - probs[:, game] - probs[:, engine] - 0.04

    matrix = build_meta_matrix(ids, probs, evidence, keys, np)
    assert matrix.shape == (8, 9 + len(keys))
    targets = np.asarray([1, 1, 1, 1, 0, 0, 0, 0], dtype=np.int64)
    model = make_meta_model(42)
    model.fit(matrix, targets)
    keep = model.predict_proba(matrix)[:, model.named_steps["classifier"].classes_.tolist().index(1)]
    assert float(np.mean(keep[:4])) > float(np.mean(keep[4:]))

    base = np.asarray(["game"] * 8, dtype=object)
    corrected = apply_meta(base, keep, 0.5, np)
    assert sum(value == "game" for value in corrected[:4]) >= 3
    assert sum(value == "engine" for value in corrected[4:]) >= 3

    serialized_evidence = json.dumps({key: evidence[key] for key in ids}, sort_keys=True)
    assert "SecretGameClass" in serialized_evidence  # source fixture contains forbidden raw identity
    assert "SecretGameClass" not in json.dumps(keys)  # model feature schema does not

    print("ProductionClassifier v5.6 game meta verifier smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
