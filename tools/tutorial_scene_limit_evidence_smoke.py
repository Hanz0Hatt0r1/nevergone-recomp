#!/usr/bin/env python3

from __future__ import annotations

from tutorial_scene_limit_evidence import OpRow, find_candidates, render, structural_status


def row(
    address: str,
    opcode: str,
    output: str,
    inputs: str,
    *,
    time: int = 0,
) -> OpRow:
    return OpRow(
        function_entry="00420000",
        op_address=address,
        op_time=time,
        opcode=opcode,
        output=output,
        inputs=tuple(inputs.split(";")) if inputs != "-" else (),
        callee_entry="-",
        callee_name="-",
    )


def fixture() -> list[OpRow]:
    return [
        row("00420010", "INT_ADD", "def:00420010:0:4", "register:r0:4;const:0x238:4"),
        row("00420014", "LOAD", "def:00420014:0:4", "const:0x1a1:4;def:00420010:0:4"),
        row("00420018", "PTRSUB", "def:00420018:0:4", "def:00420014:0:4;const:0x20:4"),
        row("0042001c", "LOAD", "def:0042001c:0:4", "const:0x1a1:4;def:00420018:0:4"),
        # right-side X field: downstream use reaches recovered 500.0f threshold.
        row("00420020", "PTRADD", "def:00420020:0:4", "def:0042001c:0:4;const:0x3:4;const:0x4:4"),
        row("00420024", "LOAD", "def:00420024:0:4", "const:0x1a1:4;def:00420020:0:4"),
        row("00420028", "INT2FLOAT", "def:00420028:0:4", "def:00420024:0:4"),
        row("0042002c", "FLOAT_SUB", "def:0042002c:0:4", "def:00420028:0:4;const:0x43fa0000:4"),
        # first vertical raw field: use reaches recovered 20.0f threshold.
        row("00420030", "INT_ADD", "def:00420030:0:4", "def:0042001c:0:4;const:0x10:4"),
        row("00420034", "LOAD", "def:00420034:0:4", "const:0x1a1:4;def:00420030:0:4"),
        row("00420038", "INT2FLOAT", "def:00420038:0:4", "def:00420034:0:4"),
        row("0042003c", "FLOAT_LESS", "def:0042003c:0:1", "def:00420038:0:4;const:0x41a00000:4"),
        # second vertical raw field, with an extra transparent COPY.
        row("00420040", "PTRSUB", "def:00420040:0:4", "def:0042001c:0:4;const:0x14:4"),
        row("00420044", "LOAD", "def:00420044:0:4", "const:0x1a1:4;def:00420040:0:4"),
        row("00420048", "COPY", "def:00420048:0:4", "def:00420044:0:4"),
        row("0042004c", "FLOAT_LESS", "def:0042004c:0:1", "def:00420048:0:4;const:0x14:4"),
        # Noise: same numeric field offset but not under the recovered pointer prefix.
        row("00420050", "INT_ADD", "def:00420050:0:4", "register:r7:4;const:0xc:4"),
        row("00420054", "LOAD", "def:00420054:0:4", "const:0x1a1:4;def:00420050:0:4"),
    ]


def main() -> int:
    candidates = find_candidates(fixture())
    assert [(item.field_offset, item.classification) for item in candidates] == [
        (0xC, "right-edge-x-candidate"),
        (0x10, "vertical-edge-candidate"),
        (0x14, "vertical-edge-candidate"),
    ]
    assert structural_status(candidates) == "three-limit-candidate"

    text = render(candidates)
    assert "required pointer prefix: 0x238 -> 0x20" in text
    assert "status: three-limit-candidate" in text
    assert "field=0xc" in text
    assert "threshold constants: float500" in text
    assert text.count("classification: vertical-edge-candidate") == 2
    assert "vertical A/B ordering remains unassigned" in text

    incomplete = candidates[:-1]
    assert structural_status(incomplete) == "incomplete-or-ambiguous"

    print("Tutorial scene-limit evidence smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
