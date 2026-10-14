#!/usr/bin/env python3
"""Synthetic fail-closed tests for Controls alias original-ELF cross-check."""
from __future__ import annotations

import copy

import audit_nevergone_controls_alias_native_crosscheck as m


def must_fail(fn):
    try:
        fn()
    except (ValueError, KeyError):
        return
    raise AssertionError('invalid evidence accepted')


def fake_text():
    base = 0x297000
    data = bytearray(0x1a00)
    sections = [(0, 1, 0, base, 0, len(data), 0, 0, 4, 0)]
    for address, (_, _, hexbytes, _) in m.STORE_WITNESSES.items():
        pos = int(address, 16) - 0x10000 - base
        data[pos:pos+4] = bytes.fromhex(hexbytes)
    for address, (_, hexbytes, _) in m.INDIRECT_WITNESSES.items():
        pos = int(address, 16) - 0x10000 - base
        data[pos:pos+2] = bytes.fromhex(hexbytes)
    for address, raw in {
        0x29860a: '1646', 0x29861e: '3346', 0x29862c: 'fff7acff',
        0x29858e: 'c0f81031', 0x298994: 'd4f81031', 0x2989b0: '9047',
    }.items():
        pos = address - base
        data[pos:pos+len(raw)//2] = bytes.fromhex(raw)
    return data, sections


def operand(role, **kw):
    return {'operand_role': role, 'operand_token': 'const:0x1:1',
            'root_token': 'const:0x1:1', 'root_kind': 'constant',
            'root_offset': '0x0', 'root_parameter_index': '-',
            'load_address_offset': '-', 'load_address_root_token': '-',
            'nested_load_address_offset': '-', 'nested_load_address_root_token': '-',
            'nested_load_address_parameter_index': '-', **kw}


def main():
    elf, sections = fake_text()
    for address, (field, offset, _bytes, width) in m.STORE_WITNESSES.items():
        site = {'op_address': address, 'offset': offset,
                'target_or_base': 'def:LOAD@abc:1:4'}
        dest = operand('store_destination', root_token=site['target_or_base'],
                       root_offset=offset, load_address_offset=field,
                       load_address_root_token=m.R0)
        if width == 4:
            value = operand('store_value', operand_token='def:PTRADD@abc:2:4',
                            root_token=m.R0, root_offset='0x104',
                            root_parameter_index='0')
        else:
            value = operand('store_value')
        ops = {'store_destination': dest, 'store_value': value}
        result = m.verify_store(site, ops, elf, sections)
        assert result['native_store_width_bytes'] == width
        assert result['affects_routing'] is False
        bad = copy.deepcopy(ops)
        bad['store_destination']['load_address_offset'] = '0x999'
        must_fail(lambda: m.verify_store(site, bad, elf, sections))
        bad_elf = bytearray(elf)
        bad_elf[int(address, 16) - 0x10000 - 0x297000] ^= 1
        must_fail(lambda: m.verify_store(site, ops, bad_elf, sections))

    vtable = {slot: {'symbol': sym, 'ghidra_target': '00000000'}
              for slot, _, sym in m.INDIRECT_WITNESSES.values()}
    for address, (slot, _bytes, _sym) in m.INDIRECT_WITNESSES.items():
        site = {'op_address': address, 'target_or_base': 'def:LOAD@abc:1:4'}
        target = operand('call_target', root_token=site['target_or_base'],
                         load_address_root_token='def:LOAD@abc:0:4',
                         load_address_offset=slot, nested_load_address_offset='0x0',
                         nested_load_address_root_token=m.R0,
                         nested_load_address_parameter_index='0')
        receiver = operand('call_arg_0', root_token=m.R0, root_parameter_index='0')
        ops = {'call_target': target, 'call_arg_0': receiver}
        result = m.verify_indirect(site, ops, elf, sections, vtable)
        assert result['receiver_parameter_zero_proven_for_this_call']
        assert result['dynamic_receiver_class_proven'] is False
        bad = copy.deepcopy(ops)
        bad['call_target']['nested_load_address_parameter_index'] = '-'
        must_fail(lambda: m.verify_indirect(site, bad, elf, sections, vtable))

    for address in m.FACTORY_WITNESSES:
        site = {'op_address': address}
        ops = {'call_target': operand('call_target', root_token='address:002a85fc:8'),
               'call_arg_2': operand('call_arg_2', root_token=m.R0,
                                     root_parameter_index='0')}
        result = m.verify_factory(site, ops, {m.FACTORY_SYMBOL: (0x2985fd, 86, 8)})
        assert result['callback_object_argument_is_controls_this']
        bad = copy.deepcopy(ops)
        bad['call_arg_2']['root_parameter_index'] = '-'
        must_fail(lambda: m.verify_factory(site, bad,
                                          {m.FACTORY_SYMBOL: (0x2985fd, 86, 8)}))

    callback_symbols = {
        m.FACTORY_SYMBOL: (0x2985fd, 86, 8),
        '_ZN14ControlsButton19initWithNormalImageEPKcS1_PN7cocos2d8CCObjectEMS3_FvvE':
            (0x298589, 116, 8),
        '_ZN14ControlsButton8touchEndEN7cocos2d7CCPointE':
            (0x2988e3, 212, 8),
    }
    assert m.verify_factory_callback_retention(elf, sections, callback_symbols)
    bad_elf = bytearray(elf)
    bad_elf[0x29858e - 0x297000] ^= 1
    must_fail(lambda: m.verify_factory_callback_retention(
        bad_elf, sections, callback_symbols))
    must_fail(lambda: m.verify_factory_callback_retention(
        elf, sections, {m.FACTORY_SYMBOL: (0x2985fd, 86, 8)}))
    must_fail(lambda: m.virtual_to_file(0x300000, 4, sections))
    print('Controls alias native cross-check smoke: ok')


if __name__ == '__main__':
    main()
