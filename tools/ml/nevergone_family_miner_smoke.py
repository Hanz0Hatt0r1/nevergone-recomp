#!/usr/bin/env python3
from __future__ import annotations

import json
import tempfile
from pathlib import Path

import mine_nevergone_function_families as miner


HEADER = (
    "ARCH=ARM32\n"
    "ENDIAN=LE\n"
    "PTR_SIZE=4\n"
    "CALLCONV=__thiscall\n"
    "PARAMS=1\n"
    "BODY_ADDRS_BUCKET=32-63\n"
    "OPS_BUCKET=8-15\n"
    "CALLS_BUCKET=1\n"
    "INDIRECT_CALLS_BUCKET=0\n"
    "BRANCHES_BUCKET=2-3\n"
    "LOADS_BUCKET=2-3\n"
    "STORES_BUCKET=1\n"
    "TRUNCATED=0\n"
)

FAMILY_A = HEADER + (
    "o0 COPY out=v0:4 in=reg:4\n"
    "o1 PTRADD out=v1:4 in=v0:4 in=const:small:0x84:4 in=const:one:4\n"
    "o2 CAST out=v2:4 in=v1:4\n"
    "o3 LOAD out=v3:4 addr=v2:4\n"
    "o4 INT_NOTEQUAL out=v4:1 in=v3:4 in=const:zero:4\n"
    "o5 CBRANCH cond=v4:1\n"
    "o6 CALL call:internal arg=v3:4\n"
    "o7 STORE addr=v2:4 value=const:zero:4\n"
    "o8 RETURN ret=v0:4\n"
)

# Same structural family with one extra cast and different SSA/op ordinals/offset.
FAMILY_B = HEADER + (
    "o10 COPY out=v11:4 in=reg:4\n"
    "o11 PTRADD out=v12:4 in=v11:4 in=const:small:0x88:4 in=const:one:4\n"
    "o12 CAST out=v13:4 in=v12:4\n"
    "o13 LOAD out=v14:4 addr=v13:4\n"
    "o14 CAST out=v15:4 in=v14:4\n"
    "o15 INT_NOTEQUAL out=v16:1 in=v15:4 in=const:zero:4\n"
    "o16 CBRANCH cond=v16:1\n"
    "o17 CALL call:internal arg=v15:4\n"
    "o18 STORE addr=v13:4 value=const:zero:4\n"
    "o19 RETURN ret=v11:4\n"
)

OTHER = HEADER + (
    "o0 COPY out=v0:4 in=const:one:4\n"
    "o1 INT_ADD out=v1:4 in=v0:4 in=const:one:4\n"
    "o2 INT_ADD out=v2:4 in=v1:4 in=const:one:4\n"
    "o3 RETURN ret=v2:4\n"
)


def write_dataset(root: Path) -> None:
    rows = [
        {"id": "a", "text": FAMILY_A, "labels": {"code_domain": "game"}, "weights": {"code_domain": 1.0}},
        {"id": "b", "text": FAMILY_B, "labels": {"code_domain": "game"}, "weights": {"code_domain": 1.0}},
        {"id": "c", "text": OTHER, "labels": {"code_domain": "engine"}, "weights": {"code_domain": 1.0}},
    ]
    for name in ("train", "validation", "test"):
        (root / f"{name}.jsonl").write_text("", encoding="utf-8")
    with (root / "train.jsonl").open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        dataset = root / "dataset"
        dataset.mkdir()
        write_dataset(dataset)
        output = root / "families.jsonl"
        report = miner.mine_dataset(dataset, output, similarity_threshold=0.60)
        assert report["functions"] == 3
        assert report["families"] == 2
        rows = [json.loads(line) for line in output.read_text(encoding="utf-8").splitlines() if line]
        by_id = {row["id"]: row for row in rows}
        assert all(row["format"] == miner.FORMAT for row in rows)
        assert by_id["a"]["family_id"] == by_id["b"]["family_id"]
        assert by_id["a"]["family_id"] != by_id["c"]["family_id"]
        assert by_id["a"]["family_size"] == 2
        assert report["mixed_label_families"]["code_domain"] == 0
        assert report["policy"]["hash_profile"] == "nevergone-v1"
        assert {
            row_id: (row["family_id"], row["portable_hash"], row["feature_count"])
            for row_id, row in by_id.items()
        } == {
            "a": ("family-a5774bde8e083339f4b8", "e2ea352e57dc092618aafbda", 49),
            "b": ("family-a5774bde8e083339f4b8", "a5774bde8e083339f4b85e60", 52),
            "c": ("family-376d9f86503eb7eaeed4", "376d9f86503eb7eaeed4351f", 16),
        }

    print("Nevergone function family miner smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
