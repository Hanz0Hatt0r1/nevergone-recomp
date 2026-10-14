#!/usr/bin/env python3
"""Synthetic fail-closed smoke for retained Controls alias closure helpers."""
from audit_nevergone_controls_retained_alias_closure import (
    BUTTON_CALLBACKS,
    CCJOY_14C,
    DANGEROUS,
    DELEGATE_TARGETS,
    THUNKS,
    immediate_stores,
)


class FakeElf:
    def __init__(self, data: bytes):
        self.data = data

    def at(self, address: int, size: int) -> bytes:
        return self.data[address : address + size]


def main() -> int:
    assert set(BUTTON_CALLBACKS) == {
        "_ZN8Controls14menuABCallbackEv",
        "_ZN8Controls14menuDBCallbackEv",
        "_ZN8Controls13menuJCallbackEv",
    }
    assert set(THUNKS) == {0, 4, 8}
    assert len(DELEGATE_TARGETS) == 3
    assert CCJOY_14C == {
        0x295DD6: "load",
        0x295EF2: "store",
        0x296254: "load",
        0x296378: "load",
    }
    assert 0x118 not in DANGEROUS
    assert {0x124, 0x12C, 0x130, 0x134, 0x174} <= DANGEROUS

    # STR.W r3,[r0,#0x118]; STRB.W r1,[r4,#0x151].
    fake = FakeElf(bytes.fromhex("c0f8183184f85111"))
    assert immediate_stores(fake, 0, 8) == [
        (0, 4, 0, 3, 0x118),
        (4, 1, 4, 1, 0x151),
    ]

    # LDR.W is intentionally not classified as a write.
    fake = FakeElf(bytes.fromhex("d0f82431"))
    assert immediate_stores(fake, 0, 4) == []

    print("Controls retained alias closure smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
