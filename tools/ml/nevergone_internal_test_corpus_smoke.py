#!/usr/bin/env python3
from __future__ import annotations

import json
import tempfile
from pathlib import Path

from select_nevergone_internal_test_corpus import select_rows, stable_id


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def corpus_row(entry: str, name: str) -> dict:
    return {
        "format": "NEVERGONE.MlPcodeCorpus/2",
        "program": "libcocos2dcpp.so",
        "source_entry": entry,
        "source_name": name,
        "external": False,
        "decompile_status": "ok",
        "metrics": {},
        "model_text": "ARCH=ARM32\nENDIAN=LE\nPTR_SIZE=4\n",
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        corpus = root / "corpus.jsonl"
        dataset = root / "dataset"

        overlap_entry = "00100000"
        write_jsonl(
            corpus,
            [
                corpus_row("00010000", "<EXTERNAL>::cos"),
                corpus_row("00020000", "_FINI_0"),
                corpus_row(overlap_entry, "FUN_00100000"),
                corpus_row("00110000", "FUN_00110000"),
                corpus_row("00120000", "GameScene::update(float)"),
            ],
        )

        overlap_id = stable_id("libcocos2dcpp.so", overlap_entry)
        write_jsonl(dataset / "train.jsonl", [{"id": overlap_id}])
        write_jsonl(dataset / "validation.jsonl", [])
        write_jsonl(dataset / "test.jsonl", [])

        selected, stats = select_rows(corpus, dataset, 2)
        assert len(selected) == 2
        assert {row["source_entry"] for row in selected} == {"00110000", "00120000"}
        assert stats["origin_external_import"] == 1
        assert stats["origin_runtime_scaffold"] == 1
        assert stats["training_overlap"] == 1
        assert stats["eligible_unseen_internal"] == 2

    print("nevergone internal test corpus smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
