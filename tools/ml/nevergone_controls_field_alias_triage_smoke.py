#!/usr/bin/env python3
"""Synthetic, fail-closed regression for Controls alias and callee triage."""
from __future__ import annotations

import copy
import csv
import io
import json

from triage_nevergone_controls_field_alias_evidence import (
    AUDIT, FIELDS, FORMAT, GHIDRA, NATIVE, SHA, analyze, classify_callee,
)


def tsv(rows):
    output = io.StringIO()
    writer = csv.DictWriter(output, fieldnames=list(rows[0]), delimiter='\t')
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue()


def fixtures():
    entries = [f'{0x2a0000 + i * 16:08x}' for i in range(38)]
    native = [{
        'format': NATIVE, 'record_type': 'summary', 'elf_sha256': SHA,
        'controls_store_addresses': sorted(FIELDS), 'controls_unique_method_count': 38,
        'controls_symbol_count': 40, 'controls_method_entries': entries,
        'whole_elf_store_encoding_candidates': 3,
    }]
    native += [{'format': NATIVE, 'record_type': 'store_encoding_candidate'} for _ in range(3)]
    manifest = [{'function_entry': e} for e in entries]
    operations = []
    for i, e in enumerate(entries):
        operations.append({
            'format': GHIDRA, 'function_entry': e, 'row_type': 'summary',
            'decompile_status': 'ok', 'store_count': '5' if i == 0 else '0',
            'call_count': '3' if i == 0 else '0',
            'op_address': '-', 'op_time': '-', 'base_parameter_index': '-',
            'address_offset': '-', 'address_kind': '-', 'address_base_token': '-',
            'call_kind': '-', 'call_target_token': '-', 'this_argument_indices': '-',
            'function_name': 'Controls::init',
        })
    for address, offset in FIELDS.items():
        operations.append({
            **operations[0], 'row_type': 'store', 'op_address': address,
            'op_time': '0', 'base_parameter_index': '0', 'address_offset': offset,
            'address_kind': 'base_plus_constant',
        })
    for i, base in enumerate(('register:register:00000020:4', 'def:LOAD@002a0000:1:4')):
        operations.append({
            **operations[0], 'row_type': 'store', 'op_address': f'{0x2a0300+i*4:08x}',
            'op_time': '0', 'base_parameter_index': '-', 'address_offset': '0x150',
            'address_kind': 'base_plus_constant', 'address_base_token': base,
        })
    for i, (kind, target) in enumerate((
        ('CALL', 'address:00500000:8'), ('CALLIND', 'def:LOAD@002a0000:1:4'),
        ('CALL', 'address:00500010:8'),
    )):
        operations.append({
            **operations[0], 'row_type': 'call', 'op_address': f'{0x2a0400+i*4:08x}',
            'op_time': '0', 'call_kind': kind, 'call_target_token': target,
            'this_argument_indices': '0',
        })
    audit = [{
        'format': AUDIT, 'status': 'bounded_controls_methods_scanned_with_alias_caveat',
        'reasons': [], 'affects_routing': False, 'all_alias_writers_excluded': False,
        'ghidra_stores_in_controls_methods': 5, 'ghidra_calls_in_controls_methods': 3,
        'ghidra_unknown_address_stores': 2, 'ghidra_calls_passing_this': 3,
    }]
    archive = {
        'native': '\n'.join(map(json.dumps, native)), 'manifest': tsv(manifest),
        'ghidra': tsv(operations), 'audit': '\n'.join(map(json.dumps, audit)),
    }
    symbols = {0x500000: '_ZN7cocos2d7CCPointC2Ev',
               0x500010: '_ZN14ControlsButton6createEPKcS1_PN7cocos2d8CCObjectEMS3_FvvE'}
    return archive, symbols


def must_fail(archive, symbols):
    try:
        analyze(archive, symbols)
    except ValueError:
        return
    raise AssertionError('unsafe or incomplete evidence accepted')


def main():
    archive, symbols = fixtures()
    rows, plan = analyze(archive, symbols)
    s = rows[0]
    assert s['format'] == FORMAT
    assert s['unresolved_store_base_kinds'] == {'register_base': 1, 'loaded_pointer_base': 1}
    assert s['unresolved_stores_at_exact_field_offsets'] == 0
    assert s['callee_categories'] == {
        'ccpoint_subobject_candidate': 1, 'indirect_unresolved': 1,
        'controlsbutton_factory_callback_candidate': 1,
    }
    assert s['next_probe_sites'] == 3 and len(plan) == 3
    assert s['all_alias_writers_excluded'] is False and s['affects_routing'] is False
    assert classify_callee('_ZN8Controls13getControlDirEv', 'CALL') == 'controls_method_candidate'
    assert classify_callee('_ZN7cocos2d6CCNode8scheduleEv', 'CALL') == 'cocos_base_method_candidate'

    bad = copy.deepcopy(archive)
    n = [json.loads(x) for x in bad['native'].splitlines()]
    n[0]['elf_sha256'] = '0' * 64
    bad['native'] = '\n'.join(map(json.dumps, n))
    must_fail(bad, symbols)

    bad = copy.deepcopy(archive)
    rows2 = list(csv.DictReader(io.StringIO(bad['ghidra']), delimiter='\t'))
    rows2[38]['address_offset'] = '0x130'
    bad['ghidra'] = tsv(rows2)
    must_fail(bad, symbols)

    bad = copy.deepcopy(archive)
    rows2 = list(csv.DictReader(io.StringIO(bad['ghidra']), delimiter='\t'))
    rows2[0]['call_count'] = '4'
    bad['ghidra'] = tsv(rows2)
    must_fail(bad, symbols)

    bad = copy.deepcopy(archive)
    a = json.loads(bad['audit'])
    a['reasons'] = ['decompile_failed']
    bad['audit'] = json.dumps(a)
    must_fail(bad, symbols)

    bad = copy.deepcopy(archive)
    bad['manifest'] = tsv(list(csv.DictReader(io.StringIO(bad['manifest']), delimiter='\t'))[:-1])
    must_fail(bad, symbols)

    must_fail(archive, {0x500000: symbols[0x500000]})
    print('Controls field alias triage smoke: ok')


if __name__ == '__main__':
    main()
