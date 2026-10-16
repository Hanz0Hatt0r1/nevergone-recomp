#!/usr/bin/env python3
"""Attach shared structural P-code metrics using the Nevergone output format."""
from __future__ import annotations

import argparse
import json
import os
import sys
from functools import lru_cache
from pathlib import Path
from typing import Any, Iterable

FORMAT = "NEVERGONE.StructuralMetrics/1"


@lru_cache(maxsize=1)
def _load_structural_metrics():
    repository = Path(
        os.environ.get(
            "REVENGINSTR_REPO",
            str(Path(__file__).resolve().parents[2].parent / "revenginstr"),
        )
    )
    source = repository / "src"
    if not (source / "revenginstr/evidence/structural.py").is_file():
        raise ModuleNotFoundError(
            "revenginstr is required; install it or set REVENGINSTR_REPO to its checkout"
        )
    if str(source) not in sys.path:
        sys.path.insert(0, str(source))
    from revenginstr.evidence.structural import structural_metrics

    return structural_metrics


def structural_metrics(text: str) -> dict[str, Any]:
    return _load_structural_metrics()(text)


def _read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def _iter_input(path: Path) -> Iterable[dict[str, Any]]:
    if path.is_dir():
        found = False
        for split in ("train", "validation", "test"):
            candidate = path / f"{split}.jsonl"
            if not candidate.exists():
                continue
            found = True
            yield from _read_jsonl(candidate)
        if not found:
            raise ValueError(f"{path}: no train/validation/test JSONL files")
        return
    yield from _read_jsonl(path)


def _extract_record(row: dict[str, Any], line_no: int) -> tuple[str, str]:
    text = row.get("model_text")
    if not isinstance(text, str) or not text:
        text = row.get("text")
    if not isinstance(text, str) or not text:
        raise ValueError(f"input row {line_no} has neither model_text nor text")

    record_id = row.get("id")
    if not isinstance(record_id, str) or not record_id:
        program = row.get("program")
        source_entry = row.get("source_entry")
        if isinstance(program, str) and program and isinstance(source_entry, str) and source_entry:
            record_id = f"{program}:{source_entry}"
        else:
            raise ValueError(
                f"input row {line_no} has neither id nor program/source_entry identity"
            )
    return record_id, text


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="JSONL corpus/dataset or dataset directory")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    rows = list(_iter_input(args.input))
    if not rows:
        raise SystemExit("input is empty")

    seen: set[str] = set()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for line_no, row in enumerate(rows, 1):
            record_id, text = _extract_record(row, line_no)
            if record_id in seen:
                raise ValueError(f"duplicate id: {record_id}")
            output = {
                "format": FORMAT,
                "id": record_id,
                "metrics": structural_metrics(text),
            }
            handle.write(json.dumps(output, sort_keys=True) + "\n")
            seen.add(record_id)

    print("format:", FORMAT)
    print("functions:", len(seen))
    print("engine: revenginstr")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
