#!/usr/bin/env python3
"""Regression gates for the exact Controls Cocos back-reference checker."""
from __future__ import annotations

import audit_nevergone_controls_cocos_backrefs as m


class FakeElf:
    def __init__(self):
        self.symbols = {
            '_ZTV8Controls': (0x8E4490, 592),
            '_ZN7cocos2d7CCLayer15setTouchEnabledEb': (0x524DA9, 58),
        }
        self.rels = {0x8E4490 + 8 + 0x1AC: 23}
        self.words = {
            0x8E4490 + 8 + 0x1AC: 0x524DA9,
        }

    def sym(self, name, address=None, size=None):
        if name not in self.symbols:
            raise ValueError('missing ' + name)
        value, actual_size = self.symbols[name]
        if address is not None and (value & ~1) != address:
            raise ValueError('address mismatch')
        if size is not None and actual_size != size:
            raise ValueError('size mismatch')
        return value, actual_size

    def u32(self, address):
        return self.words[address]


def must_fail(fn):
    try:
        fn()
    except (ValueError, KeyError):
        return
    raise AssertionError('invalid evidence accepted')


def main() -> None:
    assert m.FORMAT == 'NEVERGONE.ControlsCocosBackrefs/1'
    assert m.DANGEROUS == {0x124, 0x12C, 0x130, 0x134, 0x174}
    assert len(m.PARENT_ACCESSES) == 12
    assert sum(v == 'store' for v in m.PARENT_ACCESSES.values()) == 3
    assert sum(v == 'load' for v in m.PARENT_ACCESSES.values()) == 9
    assert len(m.TOUCH_THUNKS) == 8
    assert sorted(m.TOUCH_THUNKS) == [0x200,0x204,0x208,0x20C,0x210,0x214,0x218,0x21C]

    fake = FakeElf()
    m.vslot(fake, '_ZTV8Controls', 0x8E4490, 592, 0x1AC,
            '_ZN7cocos2d7CCLayer15setTouchEnabledEb', 0x524DA8)

    bad = FakeElf()
    bad.rels[0x8E4490 + 8 + 0x1AC] = 22
    must_fail(lambda: m.vslot(bad, '_ZTV8Controls', 0x8E4490, 592, 0x1AC,
                              '_ZN7cocos2d7CCLayer15setTouchEnabledEb', 0x524DA8))

    bad = FakeElf()
    bad.words[0x8E4490 + 8 + 0x1AC] = 0x12345679
    must_fail(lambda: m.vslot(bad, '_ZTV8Controls', 0x8E4490, 592, 0x1AC,
                              '_ZN7cocos2d7CCLayer15setTouchEnabledEb', 0x524DA8))

    assert not (set(m.PARENT_ACCESSES) & m.DANGEROUS)
    print('Controls Cocos backrefs smoke: ok')


if __name__ == '__main__':
    main()
