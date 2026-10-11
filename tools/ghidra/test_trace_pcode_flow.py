#!/usr/bin/env python3
from __future__ import annotations

import csv
import importlib.util
from pathlib import Path
import sys
import tempfile

MODULE_PATH = Path(__file__).with_name("trace_pcode_flow.py")
spec = importlib.util.spec_from_file_location("trace_pcode_flow", MODULE_PATH)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)

FIELDS = [
    "function_entry",
    "op_address",
    "op_time",
    "opcode",
    "output",
    "inputs",
    "callee_entry",
    "callee_name",
]


def row(
    address: str,
    time: int,
    opcode: str,
    output: str,
    inputs: str = "-",
    callee_entry: str = "-",
    callee_name: str = "-",
) -> dict[str, str]:
    return {
        "function_entry": "00351000",
        "op_address": address,
        "op_time": str(time),
        "opcode": opcode,
        "output": output,
        "inputs": inputs,
        "callee_entry": callee_entry,
        "callee_name": callee_name,
    }


def write_fixture(path: Path) -> None:
    rows = [
        row(
            "003512e8",
            0,
            "CALL",
            "def:003512e8:0:4",
            "register:r0:4",
            "00123400",
            "cocos2d::CCString::floatValue() const",
        ),
        row(
            "003512ec",
            0,
            "FLOAT2FLOAT",
            "def:003512ec:0:4",
            "def:003512e8:0:4",
        ),
        row(
            "00351308",
            0,
            "CALL",
            "def:00351308:0:4",
            "register:r0:4",
            "00123400",
            "cocos2d::CCString::floatValue() const",
        ),
        row(
            "0035130c",
            0,
            "COPY",
            "def:0035130c:0:4",
            "def:00351308:0:4",
        ),
        row(
            "00351340",
            0,
            "CALL",
            "def:00351340:0:4",
            "register:r0:4",
            "00444400",
            "unrelated::helper()",
        ),
        row(
            "003513b8",
            0,
            "CALL",
            "-",
            "register:r0:4;def:003512ec:0:4;def:0035130c:0:4",
            "00098700",
            "cocos2d::CCPoint::CCPoint(float, float)",
        ),
        row(
            "003513c2",
            0,
            "CALL",
            "-",
            "register:r0:4;register:r1:4",
            "00098800",
            "cocos2d::CCPoint::operator=(cocos2d::CCPoint const&)",
        ),
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        path = Path(temp_dir) / "flow.tsv"
        write_fixture(path)
        rows = module.read_rows(path)
        assert len(rows) == 7

        paths = module.find_call_paths(
            rows,
            "CCString::floatValue",
            "CCPoint::CCPoint",
        )
        assert len(paths) == 2
        sources = {flow.source.op_address for flow in paths}
        assert sources == {"003512e8", "00351308"}
        assert all(flow.sink.op_address == "003513b8" for flow in paths)
        assert any(
            [item.opcode for item in flow.rows] == ["CALL", "FLOAT2FLOAT", "CALL"]
            for flow in paths
        )
        assert any(
            [item.opcode for item in flow.rows] == ["CALL", "COPY", "CALL"]
            for flow in paths
        )

        no_paths = module.find_call_paths(
            rows,
            "CCString::floatValue",
            "CCPoint::operator=",
        )
        assert no_paths == []
        assert module.render_paths(no_paths) == "no data-flow path found\n"

        rendered = module.render_paths(paths)
        assert "CCString::floatValue" in rendered
        assert "CCPoint::CCPoint" in rendered
        assert "003512ec:0 FLOAT2FLOAT" in rendered

        try:
            module.find_call_paths(rows, "", "CCPoint")
        except ValueError:
            pass
        else:
            raise AssertionError("empty source token must fail")

    print("P-code flow tracer smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
