#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

from select_nevergone_internal_test_corpus import stable_id

ML = Path(__file__).resolve().parent


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def corpus_row(entry: str, name: str) -> dict:
    return {
        "format": "REVENGINSTR.PcodeCorpus/1",
        "program": "libcocos2dcpp.so",
        "source_entry": entry,
        "source_name": name,
        "external": False,
        "decompile_status": "ok",
        "metrics": {"calls": 0, "indirect_calls": 0},
        "model_text": "ARCH=ARM32\nENDIAN=LE\nPTR_SIZE=4\nOPS=1\nRETURN void",
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        source = root / "source.jsonl"
        dataset = root / "dataset"
        frozen = root / "frozen"
        overlap_entry = "00100000"
        write_jsonl(
            source,
            [
                corpus_row(overlap_entry, "FUN_00100000"),
                corpus_row("00110000", "FUN_00110000"),
                corpus_row("00120000", "GameScene::update(float)"),
                corpus_row("00130000", "EnemyActions::tick()"),
            ],
        )
        overlap_id = stable_id("libcocos2dcpp.so", overlap_entry)
        write_jsonl(dataset / "train.jsonl", [{"id": overlap_id}])
        write_jsonl(dataset / "validation.jsonl", [])
        write_jsonl(dataset / "test.jsonl", [])

        subprocess.run(
            [
                sys.executable,
                str(ML / "freeze_nevergone_unseen_corpus.py"),
                str(source),
                str(dataset),
                str(frozen),
                "--limit",
                "3",
                "--tag",
                "smoke",
            ],
            check=True,
        )

        manifest = json.loads((frozen / "manifest.json").read_text(encoding="utf-8"))
        assert manifest["format"] == "NEVERGONE.FrozenValidationCorpus/1"
        assert manifest["training_identity_count"] == 1
        assert manifest["frozen_corpus"]["rows"] == 3
        assert manifest["trace"]["rows"] == 3
        assert len(manifest["frozen_corpus"]["sha256"]) == 64

        rows = [json.loads(line) for line in (frozen / "corpus.jsonl").read_text(encoding="utf-8").splitlines()]
        assert len(rows) == 3
        assert all(row["id"] == f"{row['program']}:{row['source_entry']}" for row in rows)
        assert all(row["format"] == "REVENGINSTR.PcodeCorpus/1" for row in rows)
        assert overlap_entry not in {row["source_entry"] for row in rows}

        trace = [json.loads(line) for line in (frozen / "trace.jsonl").read_text(encoding="utf-8").splitlines()]
        assert {row["id"] for row in trace} == {row["id"] for row in rows}

        failed = subprocess.run(
            [
                sys.executable,
                str(ML / "freeze_nevergone_unseen_corpus.py"),
                str(source),
                str(dataset),
                str(frozen),
                "--limit",
                "3",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        assert failed.returncode != 0
        assert "refuse to replace" in (failed.stdout + failed.stderr)

    print("nevergone frozen validation corpus smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
