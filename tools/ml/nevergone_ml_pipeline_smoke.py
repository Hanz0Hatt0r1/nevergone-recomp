#!/usr/bin/env python3
from __future__ import annotations

import gzip
import json
import tempfile
from pathlib import Path

import build_nevergone_dataset as dataset
import build_nevergone_seed_labels as seed


def write_corpus(path: Path) -> None:
    rows = [
        {
            "format": "NEVERGONE.MlPcodeCorpus/1",
            "program": "libcocos2dcpp.so",
            "source_entry": "00100000",
            "source_name": "EnemyActionsData::loadWBGFile(char const*)",
            "external": False,
            "decompile_status": "ok",
            "model_text": "LANG=ARM:LE:32:v7\nCALLCONV=__thiscall\nPARAMS=1\nOPS=2\no0 LOAD\no1 CALL call:internal\n",
        },
        {
            "format": "NEVERGONE.MlPcodeCorpus/1",
            "program": "libcocos2dcpp.so",
            "source_entry": "00100100",
            "source_name": "cocos2d::CCNode::visit()",
            "external": False,
            "decompile_status": "ok",
            "model_text": "LANG=ARM:LE:32:v7\nCALLCONV=__thiscall\nPARAMS=0\nOPS=1\no0 RETURN\n",
        },
        {
            "format": "NEVERGONE.MlPcodeCorpus/1",
            "program": "libcocos2dcpp.so",
            "source_entry": "00100200",
            "source_name": "UnknownThing::helper()",
            "external": False,
            "decompile_status": "ok",
            "model_text": "LANG=ARM:LE:32:v7\nCALLCONV=unknown\nPARAMS=0\nOPS=1\no0 COPY\n",
        },
        {
            "format": "NEVERGONE.MlPcodeCorpus/1",
            "program": "libcocos2dcpp.so",
            "source_entry": "00100300",
            "source_name": "Broken::function()",
            "external": False,
            "decompile_status": "failed",
            "error": "test failure",
        },
    ]
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def write_functions(path: Path) -> None:
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write("entry\tname\taddress_count\tis_external\tsignature\n")
        handle.write("00100000\tEnemyActionsData::loadWBGFile(char const*)\t42\tfalse\tvoid f(char const*)\n")
        handle.write("00100100\tcocos2d::CCNode::visit()\t12\tfalse\tvoid visit()\n")
        handle.write("00100200\tUnknownThing::helper()\t4\tfalse\tvoid helper()\n")
        handle.write("EXTERNAL:1\tmemcpy\t0\ttrue\tvoid *memcpy(...)\n")


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    rules = root / "tools" / "ml" / "nevergone_seed_rules.json"

    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        functions = work / "functions.tsv.gz"
        corpus = work / "corpus.jsonl"
        labels = work / "labels.jsonl"
        out = work / "dataset"
        write_functions(functions)
        write_corpus(corpus)

        seed_report = seed.generate(
            functions,
            "libcocos2dcpp.so",
            rules,
            labels,
        )
        assert seed_report["functions_seen"] == 4
        assert seed_report["matched_functions"] == 2
        assert seed_report["label_rows"] == 3

        report = dataset.build_dataset(corpus, labels, out)
        assert report["corpus_stats"]["usable"] == 3
        assert report["corpus_stats"]["decompile_failed"] == 1
        assert report["labelled_functions"] == 2
        assert report["emitted_functions"] == 2
        assert report["unlabelled_usable_functions"] == 1
        assert report["policy"]["unlabelled_becomes_negative"] is False

        emitted = []
        for split in ("train", "validation", "test"):
            for line in (out / f"{split}.jsonl").read_text(encoding="utf-8").splitlines():
                if line.strip():
                    emitted.append(json.loads(line))
        assert len(emitted) == 2
        for row in emitted:
            assert "source_entry" not in row
            assert "source_name" not in row
            assert "text" in row and "labels" in row and "weights" in row

        traces = [
            json.loads(line)
            for line in (out / "trace.jsonl").read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        assert len(traces) == 2
        assert {row["source_name"] for row in traces} == {
            "EnemyActionsData::loadWBGFile(char const*)",
            "cocos2d::CCNode::visit()",
        }

    print("Nevergone ML pipeline smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
