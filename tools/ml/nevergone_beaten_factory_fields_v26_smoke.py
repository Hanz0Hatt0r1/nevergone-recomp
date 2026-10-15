#!/usr/bin/env python3
"""Check factory-store proof and its rejection of unrelated values."""
from __future__ import annotations

from audit_nevergone_beaten_factory_fields_v26 import FACTORY, factory_stores, owner_path


def row(opcode: str, output: str, inputs: str, address: str = "002a3aca", callee: str = "-") -> dict[str, str]:
    return {"opcode": opcode, "output": output, "inputs": inputs,
            "op_address": address, "callee_name": callee}


def main() -> None:
    direct = [
        row("CALL", "def:factory", "-", "002a3ac4", FACTORY),
        row("CAST", "def:value", "def:factory"),
        row("INT_ADD", "def:address", "register:register:00000020:4;const:0x158:4"),
        row("CAST", "def:cast_address", "def:address"),
        row("STORE", "-", "const:0x1a1:4;def:cast_address;def:value"),
    ]
    found = factory_stores(direct, 0x158)
    assert len(found) == 1 and found[0]["factory_name"] == FACTORY
    assert found[0]["owner_path"]["kind"] == "direct_this"
    unrelated = [dict(item) for item in direct]
    unrelated[0]["callee_name"] = "unrelated_factory"
    assert factory_stores(unrelated, 0x158)[0]["factory_name"] != FACTORY

    phi = {
        "def:phi": row("MULTIEQUAL", "def:phi", "def:initial;def:back"),
        "def:initial": row("COPY", "def:initial", "register:register:00000020:4"),
        "def:back": row("COPY", "def:back", "def:step"),
        "def:step": row("INT_ADD", "def:step", "def:phi;const:0x4:4"),
    }
    assert owner_path("def:phi", phi) == {"kind": "loop_this_plus_four", "stride": 4}
    phi["def:step"]["inputs"] = "def:phi;const:0x8:4"
    assert owner_path("def:phi", phi)["kind"] == "unknown"
    print("v26 beaten factory field smoke: pass")


if __name__ == "__main__":
    main()
