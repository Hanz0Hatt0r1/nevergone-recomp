#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/ml/adapt_nevergone_evidence_to_revenginstr.py"


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        source = root / "source.jsonl"
        output = root / "output.jsonl"
        features = {
            "has_callgraph": 1.0,
            "callgraph.out_degree": 7.0,
            "receiver.ratio": 0.25,
        }
        source.write_text(json.dumps({
            "format": "NEVERGONE.ProductionEvidence/1",
            "id": "lib:00100000",
            "features": features,
        }) + "\n", encoding="utf-8")
        subprocess.run([sys.executable, str(SCRIPT), str(source), str(output)], check=True)
        row = json.loads(output.read_text(encoding="utf-8"))
        assert row["format"] == "REVENGINSTR.Evidence/1"
        assert row["id"] == "lib:00100000"
        assert row["features"] == features

    print("nevergone -> revenginstr evidence adapter smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
