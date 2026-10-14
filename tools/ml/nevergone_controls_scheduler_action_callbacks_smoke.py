#!/usr/bin/env python3
"""Synthetic regression for Controls scheduler/action callback closure."""
from __future__ import annotations

from audit_nevergone_controls_scheduler_action_callbacks import (
    CALLBACKS,
    DANGEROUS,
    GOT,
    REMOVE_CHILD,
    REMOVE_CHILD_CLEANUP,
    SHOW_KILL_TIPS_CALLBACK_BYTES,
    WINDOWS,
    verify_show_kill_tips_dispatch_identity,
)


class FakeDispatchElf:
    def __init__(self, target: int):
        self.target = target
        self.relative_relocations = {0x8E4570: 23}

    def at(self, address: int, size: int) -> bytes:
        if (address, size) == (0x2966D4, 12):
            return SHOW_KILL_TIPS_CALLBACK_BYTES
        raise AssertionError((address, size))

    def symbol(self, name: str, address: int | None = None, size: int | None = None):
        values = {
            "_ZTV8Controls": (0x8E4490, 592),
            REMOVE_CHILD: (0x516BF1, 12),
        }
        value, actual_size = values[name]
        if address is not None:
            assert (value & ~1) == address
        if size is not None:
            assert actual_size == size
        return value, actual_size

    def u32(self, address: int) -> int:
        assert address == 0x8E4570
        return self.target


def main() -> int:
    assert set(CALLBACKS) == {
        "_ZN8Controls8comboEndEf",
        "_ZN8Controls4tickEf",
        "_ZN8Controls20showKillTipsCallbackEPN7cocos2d6CCNodeE",
    }
    assert GOT == {
        0x92EE08: "_ZN8Controls8comboEndEf",
        0x92EE0C: "_ZN8Controls20showKillTipsCallbackEPN7cocos2d6CCNodeE",
        0x92EE1C: "_ZN8Controls4tickEf",
    }
    assert {0x124, 0x12C, 0x130, 0x134, 0x174} == DANGEROUS
    assert set(WINDOWS) == {
        "DownDownA_scheduleOnce",
        "DownDownB_scheduleOnce",
        "init_schedule",
        "showAToJTip_CCCallFuncN",
        "CCNode_schedule_core",
        "CCNode_schedule",
        "CCNode_scheduleOnce",
        "CCCallFunc_initWithTarget",
        "CCCallFuncN_initWithTarget",
        "CCCallFuncN_execute",
        "CCNode_removeChild_cleanup",
    }
    assert WINDOWS["CCNode_removeChild_cleanup"] == (
        0x516C1E,
        0x516C42,
        "23274966faa32352b2b3b5792e65cc0182b38084c80c7981319f6705908471fa",
    )
    assert REMOVE_CHILD == "_ZN7cocos2d6CCNode11removeChildEPS0_"
    assert REMOVE_CHILD_CLEANUP == "_ZN7cocos2d6CCNode11removeChildEPS0_b"
    assert SHOW_KILL_TIPS_CALLBACK_BYTES.hex() == "08b50368d3f8d830984708bd"
    assert all(start < end and len(digest) == 64 for start, end, digest in WINDOWS.values())
    assert all(size > 0 and len(digest) == 64 for _address, size, digest in CALLBACKS.values())

    # Regression for the real v14 failure: slot 0xd8 must resolve to the
    # one-argument removeChild wrapper. The old getScheduler target must be
    # rejected even though the callback itself is otherwise unchanged.
    value, size = verify_show_kill_tips_dispatch_identity(FakeDispatchElf(0x516BF1))
    assert value == 0x516BF1 and size == 12
    try:
        verify_show_kill_tips_dispatch_identity(FakeDispatchElf(0x515E71))
    except ValueError as exc:
        assert "removeChild" in str(exc)
    else:
        raise AssertionError("stale getScheduler slot target was accepted")

    # Known callback store offsets from exact native bodies stay outside the
    # retained joystick/action-button pointer fields.
    combo_offsets = set()
    tick_offsets = {0x154, 0x110, 0x118, 0x10C}
    kill_offsets = set()
    assert not (combo_offsets | tick_offsets | kill_offsets) & DANGEROUS

    print("Controls scheduler/action callback closure smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
