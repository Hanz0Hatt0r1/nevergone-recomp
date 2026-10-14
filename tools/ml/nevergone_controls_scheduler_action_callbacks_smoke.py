#!/usr/bin/env python3
"""Synthetic regression for Controls scheduler/action callback closure."""
from __future__ import annotations

from audit_nevergone_controls_scheduler_action_callbacks import (
    CALLBACKS,
    DANGEROUS,
    GOT,
    WINDOWS,
)


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
        "CCNode_getScheduler",
    }
    assert all(start < end and len(digest) == 64 for start, end, digest in WINDOWS.values())
    assert all(size > 0 and len(digest) == 64 for _address, size, digest in CALLBACKS.values())

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
