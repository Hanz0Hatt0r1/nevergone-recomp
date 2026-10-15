#!/usr/bin/env python3
"""Audit family-held-out game<->engine boundary errors after classifier training.

This is post-hoc analysis only. It never feeds truth/prediction labels back into the
training representation. The output contains dataset IDs/family IDs and sanitized
receiver evidence summaries, not function names, addresses, receiver class names or
vtable identities.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

EVIDENCE_FORMAT = "NEVERGONE.ProductionEvidence/1"
SUMMARY_FORMAT = "NEVERGONE.GameEngineBoundaryAudit/1"
PAIR = {"game", "engine"}


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            rows.append(row)
    return rows


def load_evidence(path: Path) -> dict[str, dict[str, float]]:
    result: dict[str, dict[str, float]] = {}
    for line_no, row in enumerate(read_jsonl(path), 1):
        if row.get("format") != EVIDENCE_FORMAT:
            raise ValueError(f"{path}:{line_no}: expected {EVIDENCE_FORMAT}")
        record_id = row.get("id")
        features = row.get("features")
        if not isinstance(record_id, str) or not isinstance(features, dict):
            continue
        result[record_id] = {
            key: float(value)
            for key, value in features.items()
            if isinstance(key, str)
            and (key.startswith("receiver.") or key.startswith("has_receiver"))
            and isinstance(value, (int, float))
        }
    return result


def mean_features(rows: list[dict[str, Any]]) -> dict[str, float]:
    sums: defaultdict[str, float] = defaultdict(float)
    counts: Counter[str] = Counter()
    for row in rows:
        for key, value in row.get("receiver_features", {}).items():
            sums[key] += float(value)
            counts[key] += 1
    return {key: sums[key] / counts[key] for key in sorted(sums) if counts[key]}


def transition_summary(rows: list[dict[str, Any]]) -> dict[str, Any]:
    if not rows:
        return {
            "count": 0,
            "mean_confidence": None,
            "actions": {},
            "top_families": [],
            "mean_receiver_features": {},
        }
    families = Counter(str(row.get("family_id")) for row in rows)
    return {
        "count": len(rows),
        "mean_confidence": sum(float(row.get("confidence", 0.0)) for row in rows) / len(rows),
        "mean_game_probability": sum(float(row.get("probabilities", {}).get("game", 0.0)) for row in rows) / len(rows),
        "mean_engine_probability": sum(float(row.get("probabilities", {}).get("engine", 0.0)) for row in rows) / len(rows),
        "actions": dict(sorted(Counter(str(row.get("action")) for row in rows).items())),
        "top_families": [{"family_id": family, "count": count} for family, count in families.most_common(20)],
        "mean_receiver_features": mean_features(rows),
    }


def feature_contrast(errors: list[dict[str, Any]], correct: list[dict[str, Any]]) -> list[dict[str, Any]]:
    error_mean = mean_features(errors)
    correct_mean = mean_features(correct)
    items: list[dict[str, Any]] = []
    for key in sorted(set(error_mean) | set(correct_mean)):
        e = error_mean.get(key, 0.0)
        c = correct_mean.get(key, 0.0)
        items.append({"feature": key, "error_mean": e, "correct_mean": c, "delta": e - c})
    items.sort(key=lambda item: (-abs(float(item["delta"])), item["feature"]))
    return items[:30]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("oof_predictions", type=Path)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--summary-output", type=Path, required=True)
    args = parser.parse_args()

    evidence = load_evidence(args.evidence)
    oof = read_jsonl(args.oof_predictions)
    boundary = [
        row for row in oof
        if row.get("truth") in PAIR and row.get("prediction") in PAIR
    ]
    errors = [row for row in boundary if row.get("truth") != row.get("prediction")]
    correct = [row for row in boundary if row.get("truth") == row.get("prediction")]

    emitted: list[dict[str, Any]] = []
    grouped_errors: defaultdict[str, list[dict[str, Any]]] = defaultdict(list)
    grouped_correct: defaultdict[str, list[dict[str, Any]]] = defaultdict(list)

    for row in boundary:
        record_id = row.get("id")
        receiver_features = evidence.get(record_id, {}) if isinstance(record_id, str) else {}
        item = {
            "id": record_id,
            "family_id": row.get("family_id"),
            "truth": row.get("truth"),
            "prediction": row.get("prediction"),
            "correct": bool(row.get("correct")),
            "confidence": float(row.get("confidence", 0.0)),
            "raw_confidence": float(row.get("raw_confidence", 0.0)),
            "action": row.get("action"),
            "family_similarity": float(row.get("family_similarity", 0.0)),
            "contrast_margin": float(row.get("contrast_margin", 0.0)),
            "probabilities": {
                "game": float(row.get("probabilities", {}).get("game", 0.0)),
                "engine": float(row.get("probabilities", {}).get("engine", 0.0)),
            },
            "receiver_features": dict(sorted(receiver_features.items())),
        }
        transition = f"{item['truth']}->{item['prediction']}"
        if item["correct"]:
            grouped_correct[transition].append(item)
        else:
            grouped_errors[transition].append(item)
            emitted.append(item)

    game_to_engine = grouped_errors["game->engine"]
    engine_to_game = grouped_errors["engine->game"]
    correct_game = grouped_correct["game->game"]
    correct_engine = grouped_correct["engine->engine"]

    summary = {
        "format": SUMMARY_FORMAT,
        "oof_rows": len(oof),
        "boundary_rows": len(boundary),
        "boundary_correct": len(correct),
        "boundary_errors": len(errors),
        "boundary_error_rate": (len(errors) / len(boundary)) if boundary else 0.0,
        "transitions": {
            "game->engine": transition_summary(game_to_engine),
            "engine->game": transition_summary(engine_to_game),
            "game->game": transition_summary(correct_game),
            "engine->engine": transition_summary(correct_engine),
        },
        "feature_contrasts": {
            "game->engine_vs_game->game": feature_contrast(game_to_engine, correct_game),
            "engine->game_vs_engine->engine": feature_contrast(engine_to_game, correct_engine),
        },
        "training_feedback": False,
        "raw_function_names_used": False,
        "raw_receiver_class_names_used": False,
        "raw_vtable_ids_used": False,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in emitted:
            handle.write(json.dumps(row, sort_keys=True) + "\n")
    args.summary_output.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", SUMMARY_FORMAT)
    print("OOF rows:", len(oof))
    print("game/engine boundary rows:", len(boundary))
    print("boundary errors:", len(errors))
    print("game->engine:", len(game_to_engine))
    print("engine->game:", len(engine_to_game))
    print("training feedback: no")
    print("output:", args.output)
    print("summary:", args.summary_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
