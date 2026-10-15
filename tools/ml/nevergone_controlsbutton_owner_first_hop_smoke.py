#!/usr/bin/env python3
"""Synthetic fail-closed smoke for ControlsButton owner first-hop classification."""
from __future__ import annotations

from audit_nevergone_controlsbutton_owner_first_hop import (
    EXPECTED_DIRECT,
    EXPECTED_INDIRECT,
    EXPECTED_UNKNOWN,
    SLOT_TARGETS,
    TARGET_FIELDS,
    parse_direct,
)


def main() -> int:
    assert TARGET_FIELDS == {0x104, 0x108, 0x10C}
    assert EXPECTED_UNKNOWN == ("002a865a", "002a865e")
    assert set(EXPECTED_DIRECT) == {
        "005353f8", "0070ea70", "00535b2a", "00535b7c", "00535204"
    }
    assert len(EXPECTED_DIRECT) == 5
    assert len(EXPECTED_INDIRECT) == 10
    assert list(EXPECTED_INDIRECT.values()).count(0xB8) == 5
    assert list(EXPECTED_INDIRECT.values()).count(0x84) == 3
    assert list(EXPECTED_INDIRECT.values()).count(0x100) == 2
    assert SLOT_TARGETS == {
        0x84: "_ZN7cocos2d6CCNode9isVisibleEv",
        0xB8: "_ZN7cocos2d6CCNode8addChildEPS0_",
        0x100: "_ZN7cocos2d6CCNode6getTagEv",
    }

    assert parse_direct("address:005353f8:8") == "005353f8"
    assert parse_direct("address:005353f8:4") == "005353f8"
    assert parse_direct("def:LOAD@002a84d2:89:4") is None
    assert parse_direct("address:005353f8:16") is None

    set_enabled_offset = 0x11C
    assert set_enabled_offset not in TARGET_FIELDS

    print("ControlsButton owner first-hop smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
