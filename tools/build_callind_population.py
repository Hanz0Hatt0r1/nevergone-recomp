#!/usr/bin/env python3
"""Adapt Nevergone's frozen P-code corpus and Ghidra acquisition to CallindPopulation/1."""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import Counter
from pathlib import Path
from typing import Any

CORPUS_FORMAT = "NEVERGONE.MlPcodeCorpus/2"
ACQUISITION_FORMAT = "REVENGINSTR.GhidraPcodeFlowAcquisition/1"
POPULATION_FORMAT = "REVENGINSTR.CallindPopulation/1"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _jsonl(path: Path):
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            try:
                row = json.loads(raw)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_no}: invalid JSON: {exc.msg}") from exc
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected a JSON object")
            yield line_no, row


def _address_key(value: Any, where: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{where}: expected a function address")
    token = value.strip().lower()
    if token.startswith("0x"):
        token = token[2:]
    try:
        return f"0x{int(token, 16):x}"
    except ValueError as exc:
        raise ValueError(f"{where}: unsupported function address {value!r}") from exc


def build_population(corpus: Path, source_manifest: Path, acquisition: Path) -> list[dict[str, Any]]:
    corpus_sha256 = _sha256(corpus)
    manifest = json.loads(source_manifest.read_text(encoding="utf-8"))
    expected_source = manifest.get("frozen_corpus", {})
    if expected_source.get("sha256") != corpus_sha256:
        raise ValueError("canonical corpus SHA-256 does not match the frozen source manifest")

    statuses: dict[tuple[str, str], dict[str, Any]] = {}
    for line_no, status in _jsonl(acquisition):
        where = f"{acquisition}:{line_no}"
        if status.get("format") != ACQUISITION_FORMAT:
            raise ValueError(f"{where}: expected {ACQUISITION_FORMAT}")
        program = status.get("program")
        state = status.get("state")
        if not isinstance(program, str) or not program:
            raise ValueError(f"{where}: program must be non-empty")
        if state not in {"complete", "no_pcode", "decompile_failed"}:
            raise ValueError(f"{where}: unsupported acquisition state {state!r}")
        key = (program, _address_key(status.get("function_entry"), where))
        if key in statuses:
            raise ValueError(f"{where}: duplicate acquisition function {key}")
        statuses[key] = status

    rows: list[dict[str, Any]] = []
    seen: set[tuple[str, str]] = set()
    state_counts: Counter[str] = Counter()
    for record_index, record in _jsonl(corpus):
        where = f"{corpus}:{record_index}"
        if record.get("format") != CORPUS_FORMAT:
            raise ValueError(f"{where}: expected {CORPUS_FORMAT}")
        program = record.get("program")
        entry = record.get("source_entry")
        if not isinstance(program, str) or not program:
            raise ValueError(f"{where}: program must be non-empty")
        function_key = _address_key(entry, where)
        identity = (program, function_key)
        if identity in seen:
            raise ValueError(f"{where}: duplicate canonical function {identity}")
        seen.add(identity)
        status = statuses.pop(identity, None)
        if status is None:
            raise ValueError(f"{where}: no matching Ghidra acquisition row for {identity}")
        metrics = record.get("metrics") or {}
        expected = metrics.get("indirect_calls")
        if expected is not None and (isinstance(expected, bool) or not isinstance(expected, int) or expected < 0):
            raise ValueError(f"{where}: metrics.indirect_calls must be a non-negative integer")
        truncated = metrics.get("truncated", False)
        if not isinstance(truncated, bool):
            raise ValueError(f"{where}: metrics.truncated must be boolean")
        rows.append({
            "format": POPULATION_FORMAT,
            "program_id": program,
            "function_entry": entry,
            "acquisition_state": status["state"],
            "expected_callind_count": expected,
            "canonical_truncated": truncated,
            "canonical_source": {
                "source_format": CORPUS_FORMAT,
                "artifact_sha256": corpus_sha256,
                "record_index": record_index,
            },
        })
        state_counts[status["state"]] += 1

    if len(rows) != expected_source.get("rows"):
        raise ValueError(f"canonical corpus row count {len(rows)} does not match the frozen source manifest")
    if statuses:
        raise ValueError(f"{len(statuses)} Ghidra acquisition rows are outside the canonical corpus")
    print(f"population functions: {len(rows)}")
    print(f"acquisition states: {dict(sorted(state_counts.items()))}")
    print(f"canonical corpus SHA-256: {corpus_sha256}")
    return rows


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path, help=f"frozen {CORPUS_FORMAT} JSONL")
    parser.add_argument("source_manifest", type=Path, help="path-scrubbed frozen corpus manifest")
    parser.add_argument("acquisition", type=Path, help=f"Ghidra {ACQUISITION_FORMAT} JSONL")
    parser.add_argument("output", type=Path, help=f"normalized {POPULATION_FORMAT} JSONL")
    args = parser.parse_args(argv)
    try:
        rows = build_population(args.corpus, args.source_manifest, args.acquisition)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("w", encoding="utf-8", newline="") as handle:
            for row in rows:
                handle.write(json.dumps(row, separators=(",", ":")) + "\n")
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError) as exc:
        print(f"Nevergone CALLIND population: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
