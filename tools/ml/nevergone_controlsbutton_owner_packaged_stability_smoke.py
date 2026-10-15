#!/usr/bin/env python3
"""Fail-closed smoke for ControlsButton packaged owner-field stability."""
from __future__ import annotations

from audit_nevergone_controlsbutton_owner_packaged_stability import (
    DLSYM,
    FIRST,
    NONZERO_CALLS,
    PARENT_ACCESSES,
    PARENT_SCRIPT,
    SECONDARY,
    TARGET_FIELDS,
    validate_prerequisites,
)


def must_fail(first, parent, dlsym):
    try:
        validate_prerequisites(first, parent, dlsym)
    except ValueError:
        return
    raise AssertionError("prerequisite gate unexpectedly accepted unsafe evidence")


def main() -> int:
    assert TARGET_FIELDS == {0x104, 0x108, 0x10C}
    assert len(NONZERO_CALLS) == 9
    assert len(PARENT_ACCESSES) == 12
    assert SECONDARY[0x8].startswith("_ZThn224_N14ControlsButton12ccTouchBegan")
    assert SECONDARY[0xC].startswith("_ZThn224_N14ControlsButton12ccTouchMoved")
    assert SECONDARY[0x10].startswith("_ZThn224_N14ControlsButton12ccTouchEnded")

    first = {
        "format": FIRST,
        "status": "controlsbutton_owner_first_hop_classified",
        "target_fields_written_by_first_hop_external_targets": False,
    }
    parent = {
        "format": PARENT_SCRIPT,
        "known_parent_native_dangerous_write_through_excluded": True,
        "packaged_static_script_engine_write_through_reachable": False,
    }
    dlsym = {
        "format": DLSYM,
        "packaged_dlsym_lookup_of_set_script_engine_excluded": True,
        "packaged_cross_library_lookup_of_set_script_engine_excluded": True,
    }
    validate_prerequisites(first, parent, dlsym)

    bad = dict(first)
    bad["target_fields_written_by_first_hop_external_targets"] = True
    must_fail(bad, parent, dlsym)

    bad_parent = dict(parent)
    bad_parent["packaged_static_script_engine_write_through_reachable"] = True
    must_fail(first, bad_parent, dlsym)

    bad_dlsym = dict(dlsym)
    bad_dlsym["packaged_dlsym_lookup_of_set_script_engine_excluded"] = False
    must_fail(first, parent, bad_dlsym)

    print("ControlsButton packaged owner-field stability smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
