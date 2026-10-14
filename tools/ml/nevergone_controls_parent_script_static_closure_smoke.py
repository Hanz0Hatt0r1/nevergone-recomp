#!/usr/bin/env python3
"""Synthetic checks for the Controls parent/script static closure audit."""
from __future__ import annotations

import audit_nevergone_controls_parent_script_static_closure as m


def main() -> None:
    assert m.FORMAT == 'NEVERGONE.ControlsParentScriptStaticClosure/1'
    assert m.DANGEROUS == {0x124, 0x12c, 0x130, 0x134, 0x174}
    assert set(m.PARENT_SLOTS) == {0xcc, 0xdc, 0xf0, 0x160, 0x210}
    assert not ({offset for stores in m.EXPECTED_PARENT_TARGET_STORES.values()
                 for _, offset, _ in stores} & m.DANGEROUS)

    # Known BL in the original ELF: 0x516a48 -> sharedManager 0x536cdc.
    assert m.decode_thumb_branch_target(0x516a48, 0xf020, 0xf948) == 0x536cdc
    # Known B.W tail call: 0x524920 -> addStandardDelegate 0x5493f4.
    assert m.decode_thumb_branch_target(0x524920, 0xf024, 0xbd66) == 0x5493f4
    assert m.decode_thumb_branch_target(0x1000, 0xbf00, 0xbf00) is None

    # Negative displacement stays signed.
    assert m.decode_thumb_branch_target(0x536cd2, 0xf7ff, 0xffeb) == 0x536cac

    assert m.HASHES[m.SETTER][0:2] == (0x536ccc, 14)
    assert m.HASHES[m.SHARED][0:2] == (0x536cdc, 36)
    print('Controls parent/script static closure smoke: ok')


if __name__ == '__main__':
    main()
