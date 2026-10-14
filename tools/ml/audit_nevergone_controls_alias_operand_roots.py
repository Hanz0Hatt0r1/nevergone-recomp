#!/usr/bin/env python3
"""Fail-closed audit of 15 bounded Controls alias operand-root sites."""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path

FORMAT = 'NEVERGONE.GhidraControlsAliasOperandRoots/1'
OUT_FORMAT = 'NEVERGONE.ControlsAliasOperandRootAudit/1'
KINDS = {'store_loaded_pointer_base': 9,
         'indirect_unresolved': 3,
         'controlsbutton_factory_callback_candidate': 3}


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding='utf-8', newline='') as handle:
        return list(csv.DictReader(handle, delimiter='\t'))


def key(row: dict, from_plan: bool) -> tuple[str, str, str, str]:
    return (row['kind'], row['function_entry'],
            row['op_address'] if from_plan else row['site_address'],
            row['op_time'] if from_plan else row['site_time'])


def audit(plan: list[dict], evidence: list[dict]) -> dict:
    errors: list[str] = []
    expected = Counter(r.get('kind') for r in plan)
    if expected != Counter(KINDS) or len(plan) != 15:
        raise ValueError('expected exact 9/3/3 alias probe plan')
    keys = [key(r, True) for r in plan]
    if len(set(keys)) != 15 or any(not r['function_entry'].isalnum() or
                                    len(r['function_entry']) != 8 or
                                    not r['op_time'].isdigit() for r in plan):
        raise ValueError('invalid or duplicate alias plan sites')
    if any(r.get('format') != FORMAT or r.get('row_type') not in {'summary', 'operand'}
           for r in evidence):
        raise ValueError('unexpected Ghidra operand-root format')
    by_site: dict[tuple, list[dict]] = defaultdict(list)
    for row in evidence:
        k = key(row, False)
        if k not in keys:
            raise ValueError('Ghidra row not in pinned alias plan: ' + str(k))
        by_site[k].append(row)
    detail = []
    load_offsets = Counter()
    nested_offsets = Counter()
    call_target_offsets = Counter()
    for p in plan:
        k = key(p, True)
        rows = by_site.get(k, [])
        summaries = [r for r in rows if r['row_type'] == 'summary']
        operands = [r for r in rows if r['row_type'] == 'operand']
        site_errors = []
        if len(summaries) != 1:
            site_errors.append('missing_or_duplicate_summary')
        elif summaries[0]['site_status'] != 'ok':
            site_errors.append('site_' + summaries[0]['site_status'])
        else:
            expected_count = int(summaries[0]['operand_count'])
            if expected_count != len(operands) or expected_count < 1 or expected_count > 32:
                site_errors.append('operand_count_mismatch')
        operand_indices = [r['operand_index'] for r in operands]
        if len(operand_indices) != len(set(operand_indices)) or any(
                r['site_status'] != 'ok' or not r['operand_index'].isdigit() or
                r['operand_token'] == '-' for r in operands):
            site_errors.append('invalid_operand_identity')
        lookup = {r['operand_role']: r for r in operands}
        if p['kind'] == 'store_loaded_pointer_base':
            if set(lookup) != {'store_destination', 'store_value'}:
                site_errors.append('store_operands_missing')
            else:
                dest = lookup['store_destination']
                if dest['root_kind'] not in {'load', 'base_plus_constant'} or not (
                        dest['root_token'].startswith('def:LOAD@')):
                    site_errors.append('store_load_base_not_recovered')
                load_offsets[dest['load_address_offset']] += 1
                nested_offsets[dest['nested_load_address_offset']] += 1
        else:
            if 'call_target' not in lookup:
                site_errors.append('call_target_missing')
            else:
                target = lookup['call_target']
                if p['kind'] == 'indirect_unresolved':
                    if not target['root_token'].startswith('def:LOAD@'):
                        site_errors.append('indirect_target_load_not_recovered')
                    call_target_offsets[target['load_address_offset']] += 1
                if p['kind'] == 'controlsbutton_factory_callback_candidate' and (
                        'call_arg_2' not in lookup):
                    site_errors.append('factory_callback_argument_missing')
        errors.extend(f'{p["op_address"]}:{reason}' for reason in site_errors)
        detail.append({
            'kind': p['kind'], 'function_entry': p['function_entry'],
            'op_address': p['op_address'], 'op_time': p['op_time'],
            'status': 'ok' if not site_errors else 'incomplete',
            'reasons': site_errors,
            'operands': [{
                'role': r['operand_role'], 'root_kind': r['root_kind'],
                'root_token': r['root_token'], 'root_offset': r['root_offset'],
                'root_parameter_index': r['root_parameter_index'],
                'load_address_root_token': r['load_address_root_token'],
                'load_address_offset': r['load_address_offset'],
                'load_address_parameter_index': r['load_address_parameter_index'],
                'nested_load_address_offset': r['nested_load_address_offset'],
                'nested_load_address_parameter_index': r['nested_load_address_parameter_index'],
            } for r in operands],
            'affects_routing': False,
        })
    return {
        'format': OUT_FORMAT, 'status': 'bounded_operand_roots_complete' if not errors else
        'bounded_operand_roots_incomplete',
        'sites_expected': 15, 'sites_complete': sum(d['status'] == 'ok' for d in detail),
        'sites_by_kind': dict(sorted(expected.items())),
        'store_load_address_offsets': dict(sorted(load_offsets.items())),
        'store_nested_load_address_offsets': dict(sorted(nested_offsets.items())),
        'indirect_target_load_address_offsets': dict(sorted(call_target_offsets.items())),
        'sites': detail, 'reasons': sorted(set(errors)),
        'all_alias_writers_excluded': False, 'receiver_class_proven': False,
        'affects_routing': False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('plan_tsv', type=Path)
    parser.add_argument('ghidra_operand_roots_tsv', type=Path)
    parser.add_argument('--details-output', type=Path, required=True)
    parser.add_argument('--require-complete', action='store_true')
    args = parser.parse_args()
    report = audit(read_tsv(args.plan_tsv), read_tsv(args.ghidra_operand_roots_tsv))
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(json.dumps(report, sort_keys=True) + '\n', encoding='utf-8')
    print('format:', OUT_FORMAT)
    print('status:', report['status'])
    print('sites:', report['sites_complete'], '/', report['sites_expected'])
    print('store LOAD address offsets:', report['store_load_address_offsets'])
    print('indirect target LOAD offsets:', report['indirect_target_load_address_offsets'])
    print('reasons:', report['reasons'])
    print('alias exclusion: not proven; affects routing: no')
    print('details:', args.details_output)
    return 2 if args.require_complete and report['reasons'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
