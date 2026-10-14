#!/usr/bin/env python3
"""Synthetic fail-closed checks for exact Controls register-rooted STORE audit."""
from __future__ import annotations

import struct

import audit_nevergone_controls_register_store_native as m


def must_fail(data):
    try:
        m.decode_thumb_store(data, 0)
    except ValueError:
        return
    raise AssertionError('unexpected opcode accepted')


def main():
    assert len(m.EXPECTED) == 37
    assert len(m.METHODS) == 5
    assert {x[1] for x in m.EXPECTED} == set(m.METHODS)
    assert not ({v for v in m.EXPECTED.values()} & {0x124, 0x12c, 0x130, 0x134, 0x174})
    assert m.decode_thumb_store(bytes.fromhex('c4f85431'), 0) == (
        4, 4, 3, 0x154, 'c4f85431')
    assert m.decode_thumb_store(bytes.fromhex('84f85101'), 0) == (
        1, 4, 0, 0x151, '84f85101')
    assert m.decode_thumb_store(bytes.fromhex('c0f80c51'), 0) == (
        4, 0, 5, 0x10c, 'c0f80c51')
    must_fail(bytes.fromhex('d4f85431'))
    must_fail(bytes.fromhex('00bf00bf'))
    print('Controls register STORE native audit smoke: ok')


if __name__ == '__main__':
    main()
