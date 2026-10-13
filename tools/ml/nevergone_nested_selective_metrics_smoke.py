#!/usr/bin/env python3
from nevergone_nested_selective_metrics import summarize_accept_stability, summarize_class_actions

records = [
    {"outer_fold": 1, "prediction": "a", "action": "accept", "correct": True},
    {"outer_fold": 1, "prediction": "a", "action": "accept", "correct": False},
    {"outer_fold": 2, "prediction": "a", "action": "accept", "correct": True},
    {"outer_fold": 2, "prediction": "b", "action": "review", "correct": True},
    {"outer_fold": 3, "prediction": "b", "action": "escalate", "correct": False},
]

summary = summarize_class_actions(records, ["a", "b"])
assert summary["a"]["accept"]["examples"] == 3
assert summary["a"]["accept"]["correct"] == 2
assert abs(summary["a"]["accept"]["precision"] - 2 / 3) < 1e-12
assert summary["b"]["review"]["precision"] == 1.0

stability = summarize_accept_stability(records, ["a", "b"], 3)
assert stability["a"]["accept_examples"] == 3
assert stability["a"]["folds_with_accept"] == 2
assert stability["a"]["min_fold_precision"] == 0.5
assert stability["a"]["mean_fold_precision"] == 0.75
assert stability["b"]["accept_examples"] == 0
assert stability["b"]["min_fold_precision"] is None

print("nested selective metrics smoke: ok")
