#!/usr/bin/env python3
from __future__ import annotations

import gzip
import json
import tempfile
from pathlib import Path

import build_nevergone_dataset as dataset
import build_nevergone_seed_labels as seed


ENGINE_TEXT = (
    "ARCH=ARM32\n"
    "ENDIAN=LE\n"
    "PTR_SIZE=4\n"
    "CALLCONV=__thiscall\n"
    "PARAMS=0\n"
    "BODY_ADDRS_BUCKET=8-15\n"
    "OPS_BUCKET=1\n"
    "CALLS_BUCKET=0\n"
    "INDIRECT_CALLS_BUCKET=0\n"
    "BRANCHES_BUCKET=0\n"
    "LOADS_BUCKET=0\n"
    "STORES_BUCKET=0\n"
    "TRUNCATED=0\n"
    "o0 RETURN\n"
)


def write_corpus(path: Path) -> None:
    rows = [
        {
            "format": "NEVERGONE.MlPcodeCorpus/2",
            "program": "libcocos2dcpp.so",
            "source_entry": "00100000",
            "source_name": "EnemyActionsData::loadWBGFile(char const*)",
            "external": False,
            "decompile_status": "ok",
            "metrics": {
                "ops": 3,
                "emitted_ops": 3,
                "calls": 1,
                "indirect_calls": 0,
                "branches": 0,
                "loads": 1,
                "stores": 0,
                "truncated": False,
            },
            "model_text": (
                "ARCH=ARM32\n"
                "ENDIAN=LE\n"
                "PTR_SIZE=4\n"
                "CALLCONV=__thiscall\n"
                "PARAMS=1\n"
                "BODY_ADDRS_BUCKET=32-63\n"
                "OPS_BUCKET=2-3\n"
                "CALLS_BUCKET=1\n"
                "INDIRECT_CALLS_BUCKET=0\n"
                "BRANCHES_BUCKET=0\n"
                "LOADS_BUCKET=1\n"
                "STORES_BUCKET=0\n"
                "TRUNCATED=0\n"
                "o0 PTRADD out=v0:4 in=reg:4 in=const:small:0x88:4 in=const:one:4\n"
                "o1 LOAD out=v1:4 addr=v0:4\n"
                "o2 CALL call:internal arg=v1:4\n"
            ),
        },
        {
            "format": "NEVERGONE.MlPcodeCorpus/2",
            "program": "libcocos2dcpp.so",
            "source_entry": "00100100",
            "source_name": "cocos2d::CCNode::visit()",
            "external": False,
            "decompile_status": "ok",
            "metrics": {"ops": 1, "emitted_ops": 1, "truncated": False},
            "model_text": ENGINE_TEXT,
        },
        {
            "format": "NEVERGONE.MlPcodeCorpus/2",
            "program": "libcocos2dcpp.so",
            "source_entry": "00100120",
            "source_name": "cocos2d::CCNode::draw()",
            "external": False,
            "decompile_status": "ok",
            "metrics": {"ops": 1, "emitted_ops": 1, "truncated": False},
            "model_text": ENGINE_TEXT,
        },
        {
            "format": "NEVERGONE.MlPcodeCorpus/2",
            "program": "libcocos2dcpp.so",
            "source_entry": "00100200",
            "source_name": "UnknownThing::helper()",
            "external": False,
            "decompile_status": "ok",
            "metrics": {"ops": 1, "emitted_ops": 1, "truncated": False},
            "model_text": (
                "ARCH=ARM32\n"
                "ENDIAN=LE\n"
                "PTR_SIZE=4\n"
                "CALLCONV=unknown\n"
                "PARAMS=0\n"
                "BODY_ADDRS_BUCKET=4-7\n"
                "OPS_BUCKET=1\n"
                "CALLS_BUCKET=0\n"
                "INDIRECT_CALLS_BUCKET=0\n"
                "BRANCHES_BUCKET=0\n"
                "LOADS_BUCKET=0\n"
                "STORES_BUCKET=0\n"
                "TRUNCATED=0\n"
                "o0 COPY out=v0:4 in=reg:4\n"
            ),
        },
        {
            "format": "NEVERGONE.MlPcodeCorpus/2",
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
        handle.write("00100120\tcocos2d::CCNode::draw()\t12\tfalse\tvoid draw()\n")
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
        assert seed_report["functions_seen"] == 5
        assert seed_report["matched_functions"] == 3
        assert seed_report["label_rows"] == 4

        report = dataset.build_dataset(corpus, labels, out)
        assert report["format"] == "NEVERGONE.MlDataset/2"
        assert report["corpus_format"] == "NEVERGONE.MlPcodeCorpus/2"
        assert report["corpus_stats"]["usable"] == 4
        assert report["corpus_stats"]["decompile_failed"] == 1
        assert report["labelled_functions"] == 3
        assert report["emitted_functions"] == 3
        assert report["structural_groups"] == 2
        assert report["duplicate_structure_functions"] == 1
        assert report["unlabelled_usable_functions"] == 1
        assert report["policy"]["unlabelled_becomes_negative"] is False
        assert report["policy"]["split_is_deterministic_by_exact_structure"] is True

        emitted = []
        for split in ("train", "validation", "test"):
            for line in (out / f"{split}.jsonl").read_text(encoding="utf-8").splitlines():
                if line.strip():
                    emitted.append(json.loads(line))
        assert len(emitted) == 3
        for row in emitted:
            assert "source_entry" not in row
            assert "source_name" not in row
            assert "text" in row and "labels" in row and "weights" in row
            assert "LANG=" not in row["text"]
            assert "FUN_" not in row["text"]

        traces = [
            json.loads(line)
            for line in (out / "trace.jsonl").read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        assert len(traces) == 3
        by_name = {row["source_name"]: row for row in traces}
        visit = by_name["cocos2d::CCNode::visit()"]
        draw = by_name["cocos2d::CCNode::draw()"]
        assert visit["structural_group_id"] == draw["structural_group_id"]
        assert visit["split"] == draw["split"]
        assert by_name["EnemyActionsData::loadWBGFile(char const*)"]["metrics"]["ops"] == 3

    print("Nevergone ML pipeline smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
