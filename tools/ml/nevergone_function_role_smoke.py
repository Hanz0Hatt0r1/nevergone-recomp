#!/usr/bin/env python3
from __future__ import annotations

import json
import tempfile
from pathlib import Path

from augment_nevergone_function_roles import augment_dataset, infer_function_role


def assert_role(name: str, expected: str | None) -> None:
    result = infer_function_role(name)
    actual = result[0] if result is not None else None
    assert actual == expected, (name, actual, expected)


def main() -> int:
    assert_role("game::Hero::Hero", "constructor")
    assert_role("game::Hero::~Hero", "destructor")
    assert_role("game::Hero::getHP", "getter")
    assert_role("game::Hero::setHP", "setter")
    assert_role("game::Hero::update", "update")
    assert_role("game::Codec::parseHeader", "parser")
    assert_role("game::Codec::serializeState", "serializer")
    assert_role("game::Layer::onEnter", "callback")
    assert_role("FUN_00123456", None)
    assert_role("game::Hero::attack", None)

    with tempfile.TemporaryDirectory(prefix="nevergone-role-smoke-") as temp:
        root = Path(temp)
        source = root / "source"
        output = root / "output"
        source.mkdir()

        trace = [
            {"id": "a", "source_name": "game::Hero::Hero", "labels": {"code_domain": "game"}},
            {"id": "b", "source_name": "game::Hero::attack", "labels": {"code_domain": "game"}},
        ]
        (source / "trace.jsonl").write_text(
            "".join(json.dumps(row) + "\n" for row in trace), encoding="utf-8"
        )
        train = [
            {"id": "a", "text": "OP COPY out=v:4 in0=const:tiny", "labels": {"code_domain": "game"}, "weights": {"code_domain": 1.0}},
            {"id": "b", "text": "OP RETURN", "labels": {"code_domain": "game"}, "weights": {"code_domain": 1.0}},
        ]
        (source / "train.jsonl").write_text(
            "".join(json.dumps(row) + "\n" for row in train), encoding="utf-8"
        )
        (source / "validation.jsonl").write_text("", encoding="utf-8")
        (source / "test.jsonl").write_text("", encoding="utf-8")
        (source / "manifest.json").write_text(
            json.dumps({"task_distribution": {"code_domain": {"game": 2}}}), encoding="utf-8"
        )

        report = augment_dataset(source, output)
        assert report["role_labelled"] == 1, report
        out_rows = [json.loads(line) for line in (output / "train.jsonl").read_text(encoding="utf-8").splitlines()]
        assert out_rows[0]["labels"]["function_role"] == "constructor"
        assert "function_role" not in out_rows[1]["labels"]
        assert "source_name" not in out_rows[0]

    print("function-role smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
