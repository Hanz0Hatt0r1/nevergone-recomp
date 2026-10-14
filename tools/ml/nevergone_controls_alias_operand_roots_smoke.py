#!/usr/bin/env python3
"""Fail-closed synthetic tests for the bounded 15-site operand-root audit."""
from __future__ import annotations

import copy
from audit_nevergone_controls_alias_operand_roots import FORMAT, audit


def fixtures():
    plan, evidence = [], []
    kinds = ['store_loaded_pointer_base'] * 9 + ['indirect_unresolved'] * 3 + [
        'controlsbutton_factory_callback_candidate'] * 3
    for i, kind in enumerate(kinds):
        addr = f'{0x2a1000 + 4 * i:08x}'
        plan.append({'kind': kind, 'function_entry': '002a7e2c',
                     'op_address': addr, 'op_time': str(i),
                     'target_or_base': '-', 'offset': '0x150'})
        prefix = {'format': FORMAT, 'kind': kind, 'function_entry': '002a7e2c',
                  'site_address': addr, 'site_time': str(i), 'site_status': 'ok',
                  'root_kind': 'leaf', 'root_token': 'register:register:00000000:4',
                  'root_offset': '0x0', 'root_parameter_index': '0',
                  'load_address_root_token': '-', 'load_address_offset': '-',
                  'load_address_parameter_index': '-', 'nested_load_address_offset': '-',
                  'nested_load_address_parameter_index': '-',
                  'operand_token': 'register:register:00000000:4'}
        if kind == 'store_loaded_pointer_base':
            operands = [('1', 'store_destination'), ('2', 'store_value')]
        elif kind == 'indirect_unresolved':
            operands = [('0', 'call_target'), ('1', 'call_arg_0')]
        else:
            operands = [('0', 'call_target'), ('3', 'call_arg_2')]
        evidence.append({**prefix, 'row_type': 'summary', 'operand_index': '-',
                         'operand_role': '-', 'operand_count': str(len(operands))})
        for index, role in operands:
            row = {**prefix, 'row_type': 'operand', 'operand_index': index,
                   'operand_role': role, 'operand_count': '-'}
            if role in {'store_destination', 'call_target'} and kind != 'controlsbutton_factory_callback_candidate':
                row['root_kind'] = 'load'
                row['root_token'] = 'def:LOAD@002a0000:1:4'
                row['load_address_offset'] = '0x124' if kind == 'store_loaded_pointer_base' else '0x80'
            evidence.append(row)
    return plan, evidence


def must_fail(plan, evidence, expected):
    result = audit(plan, evidence)
    assert result['status'] == 'bounded_operand_roots_incomplete', result
    assert any(expected in reason for reason in result['reasons']), result


def main():
    plan, evidence = fixtures()
    result = audit(plan, evidence)
    assert result['status'] == 'bounded_operand_roots_complete', result
    assert result['sites_complete'] == 15
    assert result['store_load_address_offsets'] == {'0x124': 9}
    assert result['indirect_target_load_address_offsets'] == {'0x80': 3}
    assert result['all_alias_writers_excluded'] is False
    assert result['receiver_class_proven'] is False
    assert result['affects_routing'] is False

    bad = copy.deepcopy(evidence)
    bad.pop(0)
    must_fail(plan, bad, 'missing_or_duplicate_summary')
    bad = copy.deepcopy(evidence)
    bad[0]['operand_count'] = '1'
    must_fail(plan, bad, 'operand_count_mismatch')
    bad = copy.deepcopy(evidence)
    bad[1]['root_token'] = 'register:register:00000020:4'
    must_fail(plan, bad, 'store_load_base_not_recovered')
    bad = copy.deepcopy(evidence)
    bad[9 * 3 + 1]['root_token'] = 'register:register:00000020:4'
    must_fail(plan, bad, 'indirect_target_load_not_recovered')
    bad = copy.deepcopy(evidence)
    bad[-1]['operand_role'] = 'call_arg_1'
    must_fail(plan, bad, 'factory_callback_argument_missing')
    bad = copy.deepcopy(evidence)
    bad[1]['operand_token'] = '-'
    must_fail(plan, bad, 'invalid_operand_identity')
    try:
        audit(plan[:-1], evidence)
    except ValueError:
        pass
    else:
        raise AssertionError('incomplete plan accepted')
    bad = copy.deepcopy(evidence)
    bad[-1]['site_address'] = 'deadbeef'
    try:
        audit(plan, bad)
    except ValueError:
        pass
    else:
        raise AssertionError('unexpected site accepted')
    print('Controls alias operand-root audit smoke: ok')


if __name__ == '__main__':
    main()
