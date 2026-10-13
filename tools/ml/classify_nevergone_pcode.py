#!/usr/bin/env python3
"""Classify exported Nevergone P-code with a packaged production model."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Iterable, TextIO

from train_nevergone_production_classifier import (
    ARTIFACT_FORMAT,
    LEGACY_ARTIFACT_FORMAT,
    calibrate_confidence,
    normalize_portable,
    route_action,
    route_action_for_class,
)


def _require_joblib():
    try:
        import joblib
    except ImportError as exc:
        raise SystemExit(
            "Inference requires joblib/scikit-learn. Install with: "
            "python -m pip install -U scikit-learn joblib"
        ) from exc
    return joblib


def _read_jsonl(handle: TextIO, source_name: str) -> Iterable[dict[str, Any]]:
    for line_no, raw in enumerate(handle, 1):
        if not raw.strip():
            continue
        try:
            row = json.loads(raw)
        except json.JSONDecodeError as exc:
            raise ValueError(f"{source_name}:{line_no}: invalid JSON: {exc}") from exc
        if not isinstance(row, dict):
            raise ValueError(f"{source_name}:{line_no}: expected JSON object")
        yield row


def _extract_text(row: dict[str, Any], line_no: int) -> tuple[str, str]:
    text = row.get("model_text")
    if not isinstance(text, str) or not text:
        text = row.get("text")
    if not isinstance(text, str) or not text:
        raise ValueError(f"input row {line_no} has neither model_text nor text")

    record_id = row.get("id")
    if not isinstance(record_id, str) or not record_id:
        program = row.get("program")
        source_entry = row.get("source_entry")
        if isinstance(program, str) and isinstance(source_entry, str):
            record_id = f"{program}:{source_entry}"
        else:
            record_id = f"row-{line_no}"
    return record_id, normalize_portable(text)


def _open_input(path: str) -> tuple[TextIO, str, bool]:
    if path == "-":
        return sys.stdin, "<stdin>", False
    handle = Path(path).open("r", encoding="utf-8")
    return handle, path, True


def _open_output(path: str | None) -> tuple[TextIO, bool]:
    if path is None or path == "-":
        return sys.stdout, False
    handle = Path(path).open("w", encoding="utf-8")
    return handle, True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model_dir", type=Path)
    parser.add_argument("input", help="JSONL corpus/dataset path, or - for stdin")
    parser.add_argument("--output", help="JSONL result path; default stdout")
    parser.add_argument(
        "--no-probabilities",
        action="store_true",
        help="omit the full per-class probability map",
    )
    args = parser.parse_args()

    joblib = _require_joblib()
    artifact_path = args.model_dir / "classifier.joblib"
    artifact = joblib.load(artifact_path)
    artifact_format = artifact.get("format") if isinstance(artifact, dict) else None
    if artifact_format not in {ARTIFACT_FORMAT, LEGACY_ARTIFACT_FORMAT}:
        raise SystemExit(f"unsupported classifier artifact: {artifact_path}")

    model = artifact["model"]
    classes = list(artifact["classes"])

    input_handle, source_name, close_input = _open_input(args.input)
    output_handle, close_output = _open_output(args.output)
    try:
        rows = list(_read_jsonl(input_handle, source_name))
        if not rows:
            return 0
        ids: list[str] = []
        texts: list[str] = []
        for line_no, row in enumerate(rows, 1):
            record_id, portable = _extract_text(row, line_no)
            ids.append(record_id)
            texts.append(portable)

        probabilities = model.predict_proba(texts)
        model_classes = list(model.named_steps["classifier"].classes_)
        class_positions = {label: index for index, label in enumerate(model_classes)}

        for row_index, record_id in enumerate(ids):
            probability_map = {
                label: float(probabilities[row_index, class_positions[label]])
                for label in classes
            }
            prediction = max(probability_map, key=probability_map.get)
            raw_confidence = float(probability_map[prediction])

            if artifact_format == ARTIFACT_FORMAT:
                confidence, confidence_source = calibrate_confidence(
                    prediction,
                    raw_confidence,
                    global_calibrator=artifact["confidence_calibrator"],
                    class_calibrators=artifact.get(
                        "class_confidence_calibrators", {}
                    ),
                )
                class_policies = artifact.get("class_policies", {})
                action = route_action_for_class(
                    prediction,
                    confidence,
                    class_policies,
                )
                policy = class_policies.get(prediction, {})
                accept_threshold = policy.get("accept_threshold")
                review_threshold = policy.get("review_threshold")
            else:
                calibrator = artifact["confidence_calibrator"]
                confidence = float(calibrator.predict([raw_confidence])[0])
                confidence_source = "legacy_global"
                accept_threshold = artifact.get("accept_threshold")
                review_threshold = artifact.get("review_threshold")
                action = route_action(
                    confidence,
                    accept_threshold=accept_threshold,
                    review_threshold=review_threshold,
                )

            result = {
                "format": artifact_format,
                "id": record_id,
                "task": artifact["task"],
                "prediction": prediction,
                "confidence": confidence,
                "confidence_source": confidence_source,
                "raw_confidence": raw_confidence,
                "action": action,
                "accept_threshold": accept_threshold,
                "review_threshold": review_threshold,
            }
            if not args.no_probabilities:
                result["probabilities"] = probability_map
            output_handle.write(json.dumps(result, sort_keys=True) + "\n")
    finally:
        if close_input:
            input_handle.close()
        if close_output:
            output_handle.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
