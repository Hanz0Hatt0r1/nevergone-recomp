#!/usr/bin/env python3
"""Run ProductionClassifier/5 and apply a packaged accept-error router."""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from evaluate_nevergone_accept_error_routing_v9 import build_matrix
from evaluate_nevergone_correctness_meta_routing_v7 import predict_meta_models
from train_nevergone_production_classifier import _require_dependencies
from train_nevergone_production_classifier_v5 import load_evidence
from train_nevergone_accept_error_router_v10 import FORMAT


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base_model_dir", type=Path)
    parser.add_argument("router_dir", type=Path)
    parser.add_argument("input", type=Path)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    deps = _require_dependencies()
    np = deps["np"]
    try:
        import joblib
    except ImportError as exc:
        raise SystemExit("joblib is required") from exc

    router_path = args.router_dir / "accept_error_router.joblib"
    router = joblib.load(router_path)
    if not isinstance(router, dict) or router.get("format") != FORMAT:
        raise SystemExit(f"unsupported accept router: {router_path}")

    classifier = Path(__file__).with_name("classify_nevergone_pcode.py")
    with tempfile.TemporaryDirectory(prefix="nevergone-v10-") as tmp:
        base_output = Path(tmp) / "base.jsonl"
        subprocess.run(
            [
                sys.executable,
                str(classifier),
                str(args.base_model_dir),
                str(args.input),
                "--evidence",
                str(args.evidence),
                "--output",
                str(base_output),
            ],
            check=True,
        )
        rows = [json.loads(line) for line in base_output.read_text(encoding="utf-8").splitlines() if line.strip()]

    evidence = load_evidence(args.evidence)
    evidence_keys = list(router["evidence_keys"])
    matrix = build_matrix(rows, evidence, evidence_keys, np)
    predictions = np.asarray([str(row["prediction"]) for row in rows], dtype=object)
    models = (router.get("global_model"), dict(router.get("class_models", {})))
    scores = predict_meta_models(models, matrix, predictions, np)
    threshold = float(router["threshold"])

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for index, row in enumerate(rows):
            base_action = str(row.get("action"))
            action = base_action
            score = None
            if base_action == "accept":
                score = float(scores[index])
                if score < threshold:
                    action = "escalate"
            payload = dict(row)
            payload.update(
                {
                    "base_action": base_action,
                    "action": action,
                    "accept_router_format": FORMAT,
                    "accept_router_threshold": threshold,
                    "accept_correctness_probability": score,
                    "accept_router_changed": action != base_action,
                }
            )
            handle.write(json.dumps(payload, sort_keys=True) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
