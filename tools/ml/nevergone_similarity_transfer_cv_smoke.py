#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_jsonl(path: Path, rows) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="nevergone-sim-transfer-") as tmp:
        root = Path(tmp)
        dataset = root / "dataset"
        dataset.mkdir()
        rows = []
        families = []
        specs = [
            ("a1", "fa1", "game", "OP COPY v:4 const:tiny:4\nOP INT_ADD v:4 v:4"),
            ("a2", "fa1", "game", "OP COPY v:4 const:tiny:4\nOP INT_ADD v:4 v:4"),
            ("a3", "fa2", "game", "OP COPY v:4 const:small:4\nOP INT_ADD v:4 v:4"),
            ("a4", "fa2", "game", "OP COPY v:4 const:small:4\nOP INT_ADD v:4 v:4"),
            ("b1", "fb1", "engine", "OP LOAD v:4 mem:4\nOP STORE mem:4 v:4"),
            ("b2", "fb1", "engine", "OP LOAD v:4 mem:4\nOP STORE mem:4 v:4"),
            ("b3", "fb2", "engine", "OP LOAD v:8 mem:8\nOP STORE mem:8 v:8"),
            ("b4", "fb2", "engine", "OP LOAD v:8 mem:8\nOP STORE mem:8 v:8"),
        ]
        for record_id, family_id, label, text in specs:
            rows.append({"id": record_id, "text": text, "labels": {"code_domain": label}})
            families.append({"id": record_id, "family_id": family_id})
        write_jsonl(dataset / "train.jsonl", rows[:4])
        write_jsonl(dataset / "validation.jsonl", rows[4:6])
        write_jsonl(dataset / "test.jsonl", rows[6:])
        write_jsonl(dataset / "trace.jsonl", [])
        family_path = root / "families.jsonl"
        write_jsonl(family_path, families)

        script = Path(__file__).with_name("nevergone_similarity_transfer_cv.py")
        result = subprocess.run(
            [
                sys.executable,
                str(script),
                str(dataset),
                str(family_path),
                "--task",
                "code_domain",
                "--classes",
                "game",
                "engine",
                "--folds",
                "2",
            ],
            check=False,
            text=True,
            capture_output=True,
        )
        if result.returncode:
            print(result.stdout)
            print(result.stderr, file=sys.stderr)
            raise SystemExit(result.returncode)
        assert "model: NevergoneSimilarityTransferCV/1" in result.stdout
        assert "family-held-out transfer accuracy:" in result.stdout
        assert "PRECISION/COVERAGE GRID" in result.stdout
    print("nevergone similarity transfer CV smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
