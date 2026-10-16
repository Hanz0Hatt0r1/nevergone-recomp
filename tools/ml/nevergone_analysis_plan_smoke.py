#!/usr/bin/env python3
"""Dependency-light smoke for the Nevergone batch analysis plan builder."""
from __future__ import annotations

import csv
import gzip
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_tsv_gz(path: Path, header: list[str], rows: list[list[str]]) -> None:
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t")
        writer.writerow(header)
        writer.writerows(rows)


def rec(method: str, score: int) -> dict:
    return {"method": method, "score": score, "reasons": [method]}


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        analysis = root / "analysis.jsonl"
        functions = root / "functions.tsv.gz"
        calls = root / "calls.tsv.gz"
        out = root / "plan"

        rows = [
            {
                "id": "libcocos2dcpp.so:00001000",
                "final_action": "review",
                "suggested_analysis_method": {
                    "primary": "expand_pcode",
                    "recommendations": [rec("expand_pcode", 100), rec("inspect_callees", 68)],
                },
            },
            {
                "id": "libcocos2dcpp.so:00002000",
                "final_action": "escalate",
                "suggested_analysis_method": {
                    "primary": "resolve_indirect_calls",
                    "recommendations": [rec("resolve_indirect_calls", 96)],
                },
            },
            {
                "id": "libcocos2dcpp.so:00003000",
                "final_action": "review",
                "suggested_analysis_method": {
                    "primary": "inspect_callers",
                    "recommendations": [rec("inspect_callers", 84)],
                },
            },
        ]
        analysis.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")
        write_tsv_gz(
            functions,
            ["entry", "name", "address_count", "is_external", "signature"],
            [
                ["00001000", "A", "1", "false", "void A()"],
                ["00002000", "B", "1", "false", "void B()"],
                ["00003000", "C", "1", "false", "void C()"],
                ["00004000", "D", "1", "false", "void D()"],
            ],
        )
        write_tsv_gz(
            calls,
            ["caller_entry", "call_site", "callee_entry"],
            [
                ["00001000", "00001004", "00004000"],
                ["00004000", "00004004", "00003000"],
                ["00001000", "00001008", "00003000"],
            ],
        )

        tool = Path(__file__).with_name("build_nevergone_analysis_plan.py")
        subprocess.run(
            [sys.executable, str(tool), str(analysis), str(functions), str(calls), "--output-dir", str(out)],
            check=True,
            capture_output=True,
            text=True,
        )

        manifest = json.loads((out / "manifest.json").read_text(encoding="utf-8"))
        assert manifest["format"] == "NEVERGONE.AnalysisPlan/1"
        assert manifest["functions"] == 3
        assert manifest["primary_counts"]["expand_pcode"] == 1
        assert manifest["recommendation_counts"]["inspect_callees"] == 1
        assert manifest["routing_modified"] is False

        callees = [json.loads(line) for line in (out / "queues" / "inspect_callees.jsonl").read_text(encoding="utf-8").splitlines()]
        assert callees[0]["id"] == "libcocos2dcpp.so:00001000"
        assert callees[0]["callees"] == ["00003000", "00004000"]

        callers = [json.loads(line) for line in (out / "queues" / "inspect_callers.jsonl").read_text(encoding="utf-8").splitlines()]
        assert callers[0]["caller_count"] == 2
        assert callers[0]["callers"] == ["00001000", "00004000"]

        expand = out / "commands" / "run_expand_pcode.sh"
        indirect = out / "commands" / "run_indirect_call_evidence.sh"
        assert "ExportFunctionPcodeFlow.java" in expand.read_text(encoding="utf-8")
        assert "revenginstr_pcode_flow_script_path.sh" in expand.read_text(encoding="utf-8")
        assert "ExportIndirectCallEvidence.java" in indirect.read_text(encoding="utf-8")
        subprocess.run(["bash", "-n", str(expand)], check=True)
        subprocess.run(["bash", "-n", str(indirect)], check=True)

    print("analysis plan smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
