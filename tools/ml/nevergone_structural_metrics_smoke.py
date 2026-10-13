#!/usr/bin/env python3
"""Dependency-light smoke checks for structural metrics and attachment."""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

from extract_nevergone_structural_metrics import structural_metrics


def main() -> int:
    text = "\n".join([
        "OP COPY v:4 <- input:4",
        "OP LOAD v:4 <- mem:4",
        "OP STORE mem:4 <- v:4",
        "OP INT_ADD v:4 <- v:4 const:one:4",
        "OP INT_EQUAL v:1 <- v:4 const:zero:4",
        "OP CBRANCH v:1",
        "OP CALL call:direct v:4",
        "OP CALLIND call:indirect v:4 def:omitted:4",
        "OP PTRADD v:4 <- v:4 v:4",
        "OP RETURN v:4",
    ])
    m = structural_metrics(text)
    assert m["op_count"] == 10
    assert m["memory_op_count"] == 2
    assert m["call_count"] == 2
    assert m["indirect_call_count"] == 1
    assert abs(m["indirect_call_ratio"] - 0.5) < 1e-12
    assert m["conditional_branch_count"] == 1
    assert m["decision_point_count"] == 1
    assert m["cyclomatic_proxy"] == 2
    assert m["compare_count"] == 1
    assert m["arithmetic_count"] == 1
    assert m["pointer_op_count"] == 1
    assert m["omitted_def_operand_count"] == 1
    assert m["definition_coverage_proxy"] < 1.0

    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        extractor = Path(__file__).with_name("extract_nevergone_structural_metrics.py")

        # Raw corpus rows intentionally have no dataset-style `id`/`text` fields.
        corpus = root / "corpus.jsonl"
        extracted = root / "extracted.jsonl"
        corpus.write_text(
            json.dumps({
                "format": "NEVERGONE.MlPcodeCorpus/2",
                "program": "libcocos2dcpp.so",
                "source_entry": "002a1234",
                "model_text": text,
            }) + "\n",
            encoding="utf-8",
        )
        subprocess.run(
            [sys.executable, str(extractor), str(corpus), "--output", str(extracted)],
            check=True,
            capture_output=True,
            text=True,
        )
        extracted_row = json.loads(extracted.read_text(encoding="utf-8"))
        assert extracted_row["id"] == "libcocos2dcpp.so:002a1234"
        assert extracted_row["metrics"]["op_count"] == 10

        analysis = root / "analysis.jsonl"
        metrics = root / "metrics.jsonl"
        output = root / "joined.jsonl"
        analysis.write_text(json.dumps({"id": "f1", "final_action": "review"}) + "\n", encoding="utf-8")
        metrics.write_text(json.dumps({"id": "f1", "metrics": m}) + "\n", encoding="utf-8")
        tool = Path(__file__).with_name("attach_nevergone_structural_metrics.py")
        subprocess.run(
            [sys.executable, str(tool), str(analysis), str(metrics), "--output", str(output)],
            check=True,
            capture_output=True,
            text=True,
        )
        joined = json.loads(output.read_text(encoding="utf-8"))
        assert joined["final_action"] == "review"
        assert joined["structural_metrics"]["op_count"] == 10

    print("structural metrics smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
