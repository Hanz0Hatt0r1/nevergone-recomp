#!/usr/bin/env python3
"""Replay the committed frozen receiver coverage benchmark."""
from __future__ import annotations

import json
import tempfile
from pathlib import Path

from revenginstr.benchmark import run_benchmark


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    evidence = root / "docs/evidence/frozen-unseen500-v10-receiver-coverage"
    manifest = evidence / "benchmark-manifest.json"
    expected = json.loads((evidence / "benchmark-summary.json").read_text(encoding="utf-8"))
    with tempfile.TemporaryDirectory(prefix="nevergone-receiver-coverage-") as temporary:
        actual, code = run_benchmark(manifest, mode="smoke", output_path=Path(temporary) / "summary.json")
    if code or actual != expected:
        raise SystemExit("frozen receiver coverage summary does not match the pinned inputs")
    metrics = actual["projections"][0]["receiver"]
    functions = metrics["function_coverage"]
    print(
        f"receiver coverage replay passed: {metrics['resolved_rows']}/{actual['projections'][0]['row_count']} queries; "
        f"{functions['functions_with_resolved_queries']}/{functions['population_functions']} functions"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
