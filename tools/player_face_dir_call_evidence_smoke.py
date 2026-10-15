#!/usr/bin/env python3

from __future__ import annotations

from player_face_dir_call_evidence import (
    OpRow,
    collect_call_evidence,
    overall_status,
    render,
)


def row(
    address: str,
    opcode: str,
    output: str,
    inputs: str,
    *,
    time: int = 0,
    callee_entry: str = "-",
    callee_name: str = "-",
) -> OpRow:
    return OpRow(
        function_entry="0032ddee",
        op_address=address,
        op_time=time,
        opcode=opcode,
        output=output,
        inputs=tuple(inputs.split(";")) if inputs != "-" else (),
        callee_entry=callee_entry,
        callee_name=callee_name,
    )


def call(address: str, arg: str, *, name: str = "MEPlayer::setPlayerDir(PLAYERFACEDIR)") -> OpRow:
    return row(
        address,
        "CALL",
        "-",
        f"register:r0:4;{arg}",
        callee_entry="0032c75a",
        callee_name=name,
    )


def main() -> int:
    rows = [
        # Direct constant argument.
        call("0032de10", "const:0x0:4"),
        # Transparent copy/cast chain to a constant.
        row("0032de20", "COPY", "def:0032de20:0:4", "const:0x1:4"),
        row("0032de24", "CAST", "def:0032de24:0:4", "def:0032de20:0:4"),
        call("0032de28", "def:0032de24:0:4"),
        # Phi containing only constants: preserve the set, do not guess branch semantics.
        row(
            "0032de30",
            "MULTIEQUAL",
            "def:0032de30:0:4",
            "const:0x0:4;const:0x1:4",
        ),
        call("0032de34", "def:0032de30:0:4"),
        # Root argument remains structural evidence only.
        call("0032de40", "register:r2:4"),
        # Derived arithmetic reports constants/roots but is not promoted to enum values.
        row(
            "0032de50",
            "INT_ADD",
            "def:0032de50:0:4",
            "register:r3:4;const:0x1:4",
        ),
        call("0032de54", "def:0032de50:0:4"),
        # Unrelated direct call must not be included.
        row(
            "0032de60",
            "CALL",
            "-",
            "register:r0:4;const:0x99:4",
            callee_entry="00400000",
            callee_name="Other::setSomething(int)",
        ),
    ]

    evidence = collect_call_evidence(rows)
    assert len(evidence) == 5
    by_address = {item.call_address: item for item in evidence}
    assert by_address["0032de10"].source.kind == "constant"
    assert by_address["0032de10"].source.constants == (0,)
    assert by_address["0032de28"].source.kind == "constant"
    assert by_address["0032de28"].source.constants == (1,)
    assert by_address["0032de34"].source.kind == "constant-set"
    assert by_address["0032de34"].source.constants == (0, 1)
    assert by_address["0032de40"].source.kind == "root"
    assert by_address["0032de40"].source.root_tokens == ("register:r2:4",)
    assert by_address["0032de54"].source.kind == "derived"
    assert by_address["0032de54"].source.constants == (1,)
    assert by_address["0032de54"].source.root_tokens == ("register:r3:4",)
    assert overall_status(evidence) == "derived-sources-present"

    text = render(evidence)
    assert "direct setPlayerDir calls: 5" in text
    assert "observed PLAYERFACEDIR constant candidates: 0x0,0x1" in text
    assert "no CONTROLSSTATE mapping assigned" in text

    # Entry matching remains useful when the current Ghidra database does not
    # preserve the demangled callee name.
    unnamed = [call("0032df00", "const:0x2:4", name="-")]
    unnamed_evidence = collect_call_evidence(unnamed, "0x0032c75a")
    assert len(unnamed_evidence) == 1
    assert unnamed_evidence[0].source.constants == (2,)
    assert overall_status(unnamed_evidence) == "constant-sources"

    # Missing enum argument is explicitly incomplete rather than silently ignored.
    missing = [
        row(
            "0032df10",
            "CALL",
            "-",
            "register:r0:4",
            callee_entry="0032c75a",
            callee_name="MEPlayer::setPlayerDir(PLAYERFACEDIR)",
        )
    ]
    missing_evidence = collect_call_evidence(missing)
    assert missing_evidence[0].source.kind == "missing-argument"
    assert overall_status(missing_evidence) == "incomplete"

    print("PLAYERFACEDIR call evidence smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
