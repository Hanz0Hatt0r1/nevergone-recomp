#!/usr/bin/env python3
"""Dependency-light smoke test for call-graph metrics and attachment."""
from __future__ import annotations

import csv
import gzip
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_gzip_tsv(path: Path, fieldnames: list[str], rows: list[dict[str, str]]) -> None:
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        corpus = root / "corpus.jsonl"
        functions = root / "functions.tsv.gz"
        calls = root / "calls.tsv.gz"
        metrics = root / "metrics.jsonl"
        analysis = root / "analysis.jsonl"
        joined = root / "joined.jsonl"

        corpus.write_text(
            json.dumps({
                "program": "libcocos2dcpp.so",
                "source_entry": "00100000",
                "metrics": {"calls": 4, "indirect_calls": 1},
            }) + "\n",
            encoding="utf-8",
        )
        write_gzip_tsv(
            functions,
            ["entry", "name", "address_count", "is_external", "signature"],
            [
                {"entry": "00100000", "name": "f", "address_count": "10", "is_external": "false", "signature": "void f()"},
                {"entry": "00110000", "name": "g", "address_count": "10", "is_external": "false", "signature": "void g()"},
                {"entry": "EXTERNAL::1", "name": "puts", "address_count": "0", "is_external": "true", "signature": "void puts()"},
                {"entry": "00120000", "name": "caller", "address_count": "10", "is_external": "false", "signature": "void caller()"},
            ],
        )
        write_gzip_tsv(
            calls,
            ["caller_entry", "call_site", "callee_entry"],
            [
                {"caller_entry": "00100000", "call_site": "00100010", "callee_entry": "00110000"},
                {"caller_entry": "00100000", "call_site": "00100020", "callee_entry": "EXTERNAL::1"},
                {"caller_entry": "00120000", "call_site": "00120010", "callee_entry": "00100000"},
            ],
        )

        extract = Path(__file__).with_name("extract_nevergone_callgraph_metrics.py")
        subprocess.run(
            [sys.executable, str(extract), str(corpus), str(functions), str(calls), "--output", str(metrics)],
            check=True,
            capture_output=True,
            text=True,
        )
        row = json.loads(metrics.read_text(encoding="utf-8"))
        m = row["metrics"]
        assert row["id"] == "libcocos2dcpp.so:00100000"
        assert m["fan_in"] == 1
        assert m["fan_out"] == 2
        assert m["resolved_call_site_count"] == 2
        assert m["internal_call_site_count"] == 1
        assert m["external_call_site_count"] == 1
        assert m["corpus_call_count"] == 4
        assert m["corpus_indirect_call_count"] == 1
        assert abs(m["resolved_call_ratio_proxy"] - 0.5) < 1e-12
        assert abs(m["unknown_call_ratio_proxy"] - 0.5) < 1e-12

        analysis.write_text(
            json.dumps({"id": row["id"], "final_action": "review"}) + "\n",
            encoding="utf-8",
        )
        attach = Path(__file__).with_name("attach_nevergone_callgraph_metrics.py")
        subprocess.run(
            [sys.executable, str(attach), str(analysis), str(metrics), "--output", str(joined)],
            check=True,
            capture_output=True,
            text=True,
        )
        out = json.loads(joined.read_text(encoding="utf-8"))
        assert out["final_action"] == "review"
        assert out["callgraph_metrics"]["fan_out"] == 2

    print("callgraph metrics smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
