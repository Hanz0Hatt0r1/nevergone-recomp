#!/usr/bin/env python3
"""Exact-ELF, evidence-only triage of Controls STORE bases and this-bearing calls."""
from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import io
import json
import re
import struct
import tarfile
import zipfile
from pathlib import Path

FORMAT = 'NEVERGONE.ControlsFieldAliasTriage/1'
NATIVE = 'NEVERGONE.ControlsFieldNativeStoreInventory/1'
GHIDRA = 'NEVERGONE.GhidraControlsFieldWriteAliasInventory/1'
AUDIT = 'NEVERGONE.ControlsFieldWriteAliasAudit/1'
SHA = '94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e'
SHIFT = 0x10000
FIELDS = {'002a8120': '0x12c', '002a8172': '0x130', '002a81c2': '0x134'}
MEMBERS = {
    'native': 'controls-field-native-store-inventory.jsonl',
    'manifest': 'controls-field-native-methods.tsv',
    'ghidra': 'evidence/controls-field-write-aliases.tsv',
    'audit': 'controls-field-write-alias-audit.jsonl',
}
TARGET = re.compile(r'^address:([0-9a-f]{8}):8$')


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def classify_callee(symbol: str | None, call_kind: str) -> str:
    if call_kind == 'CALLIND':
        return 'indirect_unresolved'
    require(symbol is not None, 'unresolved direct callee')
    if symbol.startswith('_ZN7cocos2d7CCPoint'):
        return 'ccpoint_subobject_candidate'
    if symbol.startswith(('_ZN8Controls', '_ZNK8Controls')):
        return 'controls_method_candidate'
    if symbol.startswith('_ZN14ControlsButton6create'):
        return 'controlsbutton_factory_callback_candidate'
    if symbol.startswith(('_ZN7cocos2d6CCNode', '_ZN7cocos2d7CCLayer')):
        return 'cocos_base_method_candidate'
    if symbol.startswith('_ZN7cocos2d11CCCallFuncN'):
        return 'callback_factory_candidate'
    if symbol == '_ZdlPv':
        return 'operator_delete_candidate'
    return 'other_direct_candidate'


def original_symbols(elf: bytes) -> dict[int, str]:
    require(hashlib.sha256(elf).hexdigest() == SHA, 'original ELF SHA-256 mismatch')
    require(elf[:6] == b'\x7fELF\x01\x01', 'not original ELF32 little endian')
    shoff = struct.unpack_from('<I', elf, 32)[0]
    shentsize, shnum = struct.unpack_from('<HH', elf, 46)
    require(shentsize >= 40 and shnum > 0 and shoff + shentsize * shnum <= len(elf),
            'ELF section table bounds invalid')
    sections = [struct.unpack_from('<IIIIIIIIII', elf, shoff + i * shentsize)
                for i in range(shnum)]
    result: dict[int, str] = {}
    dynsym_count = 0
    for section in sections:
        if section[1] != 11:
            continue
        dynsym_count += 1
        require(section[9] == 16 and section[5] % 16 == 0 and section[6] < shnum,
                'unexpected ELF dynsym layout')
        strings = sections[section[6]]
        require(section[4] + section[5] <= len(elf) and
                strings[4] + strings[5] <= len(elf), 'ELF section exceeds file')
        strtab = elf[strings[4]:strings[4] + strings[5]]
        for pos in range(section[4], section[4] + section[5], 16):
            name_offset, value, size, info, _other, _shndx = struct.unpack_from(
                '<IIIBBH', elf, pos)
            if (info & 15) != 2 or value == 0 or size == 0:
                continue
            require(name_offset < len(strtab), 'invalid symbol name offset')
            end = strtab.find(b'\x00', name_offset)
            require(end >= 0, 'unterminated symbol name')
            name = strtab[name_offset:end].decode('utf-8', 'replace')
            key = (value & ~1) + SHIFT
            # ELF C1/C2 and D1/D2 aliases legitimately share a code address.
            if key in result and result[key] != name:
                require(classify_callee(result[key], 'CALL') == classify_callee(name, 'CALL'),
                        'conflicting ELF alias class at one address')
            else:
                result[key] = name
    require(dynsym_count == 1, 'expected exactly one dynsym')
    return result


def read_archive(archive: Path) -> dict[str, str]:
    with tarfile.open(archive, 'r:gz') as tar:
        files = [m for m in tar.getmembers() if m.isfile()]
        names = [m.name for m in files]
        require(len(names) == len(set(names)), 'duplicate archive member')
        require(set(MEMBERS.values()).issubset(names), 'missing evidence member')
        require(all(m.size <= 20_000_000 for m in files), 'oversized archive member')
        return {key: tar.extractfile(tar.getmember(name)).read().decode('utf-8')
                for key, name in MEMBERS.items()}


def parse_jsonl(value: str) -> list[dict]:
    rows = [json.loads(line) for line in value.splitlines() if line.strip()]
    require(rows and all(isinstance(row, dict) for row in rows), 'invalid JSONL')
    return rows


def parse_tsv(value: str) -> list[dict[str, str]]:
    return list(csv.DictReader(io.StringIO(value), delimiter='\t'))


def analyze(archive: dict[str, str], symbols: dict[int, str]) -> tuple[list[dict], list[dict]]:
    native = parse_jsonl(archive['native'])
    manifest = parse_tsv(archive['manifest'])
    ghidra = parse_tsv(archive['ghidra'])
    audit = parse_jsonl(archive['audit'])
    require(len(audit) == 1 and audit[0].get('format') == AUDIT and
            audit[0].get('status') == 'bounded_controls_methods_scanned_with_alias_caveat' and
            audit[0].get('reasons') == [] and
            audit[0].get('affects_routing') is False and
            audit[0].get('all_alias_writers_excluded') is False,
            'original bounded audit is incomplete or unsafe')
    summary = native[0]
    require(summary.get('format') == NATIVE and summary.get('record_type') == 'summary' and
            summary.get('elf_sha256') == SHA and
            summary.get('controls_store_addresses') == sorted(FIELDS) and
            summary.get('controls_unique_method_count') == 38 and
            summary.get('controls_symbol_count') == 40,
            'native inventory identity mismatch')
    entries = [r.get('function_entry') for r in manifest]
    require(len(entries) == 38 and len(set(entries)) == 38 and
            sorted(entries) == summary.get('controls_method_entries'),
            '38-method manifest mismatch')
    require(len(native) - 1 == summary.get('whole_elf_store_encoding_candidates'),
            'native candidate count mismatch')
    require(all(r.get('format') == GHIDRA and r.get('function_entry') in entries
                for r in ghidra), 'Ghidra provenance row mismatch')
    stores = [r for r in ghidra if r.get('row_type') == 'store']
    calls = [r for r in ghidra if r.get('row_type') == 'call']
    method_summaries = [r for r in ghidra if r.get('row_type') == 'summary']
    require(len(method_summaries) == 38 and
            {r['function_entry'] for r in method_summaries} == set(entries) and
            all(r.get('decompile_status') == 'ok' for r in method_summaries),
            'Ghidra method scan incomplete')
    for row in method_summaries:
        entry = row['function_entry']
        sub_stores = [r for r in stores if r['function_entry'] == entry]
        sub_calls = [r for r in calls if r['function_entry'] == entry]
        require(int(row['store_count']) == len(sub_stores) and
                int(row['call_count']) == len(sub_calls),
                'method operation count mismatch: ' + entry)
    require(len(stores) == audit[0]['ghidra_stores_in_controls_methods'] and
            len(calls) == audit[0]['ghidra_calls_in_controls_methods'],
            'Ghidra totals disagree with prior audit')
    direct = {r['op_address']: r['address_offset'] for r in stores
              if r.get('base_parameter_index') == '0' and
              r.get('address_offset') in set(FIELDS.values()) and
              not r.get('address_kind', '').startswith('opaque_')}
    require(direct == FIELDS, 'direct field store identity mismatch')
    unresolved_stores = [r for r in stores if r.get('base_parameter_index') == '-' or
                         r.get('address_offset') == '-' or
                         r.get('address_kind', '').startswith('opaque_')]
    require(len(unresolved_stores) == audit[0]['ghidra_unknown_address_stores'],
            'unknown store count mismatch')
    this_calls = [r for r in calls if r.get('this_argument_indices') not in ('-', '', None)]
    require(len(this_calls) == audit[0]['ghidra_calls_passing_this'],
            'this-bearing call count mismatch')
    rows: list[dict] = []
    plan: list[dict] = []
    store_kinds = collections.Counter()
    for row in unresolved_stores:
        base = row.get('address_base_token', '')
        kind = ('loaded_pointer_base' if base.startswith('def:LOAD@') else
                'register_base' if base.startswith('register:') else
                'other_unresolved_base')
        store_kinds[kind] += 1
        rows.append({'format': FORMAT, 'record_type': 'unresolved_store',
                     'function_entry': row['function_entry'], 'function_name': row['function_name'],
                     'op_address': row['op_address'], 'op_time': row['op_time'],
                     'offset': row['address_offset'], 'base_token': base,
                     'base_kind': kind, 'matches_direct_field_offset':
                     row['address_offset'] in FIELDS.values(),
                     'alias_write_excluded': False, 'affects_routing': False})
        if kind == 'loaded_pointer_base':
            plan.append({'kind': 'store_loaded_pointer_base',
                         'function_entry': row['function_entry'],
                         'op_address': row['op_address'], 'op_time': row['op_time'],
                         'target_or_base': base, 'offset': row['address_offset']})
    call_kinds = collections.Counter()
    for row in this_calls:
        kind = row['call_kind']
        target = row['call_target_token']
        symbol = None
        if kind == 'CALL':
            match = TARGET.fullmatch(target)
            require(match is not None, 'unexpected direct target token: ' + target)
            address = int(match.group(1), 16)
            symbol = symbols.get(address)
            require(symbol is not None, 'direct call has no exact ELF symbol: ' + target)
        else:
            require(kind == 'CALLIND', 'unsupported call kind: ' + kind)
        category = classify_callee(symbol, kind)
        call_kinds[category] += 1
        rows.append({'format': FORMAT, 'record_type': 'this_bearing_call_candidate',
                     'function_entry': row['function_entry'], 'function_name': row['function_name'],
                     'op_address': row['op_address'], 'op_time': row['op_time'],
                     'call_kind': kind, 'call_target_token': target,
                     'callee_symbol': symbol, 'callee_category': category,
                     'this_argument_indices': row['this_argument_indices'],
                     'argument_base_offset_proven': False,
                     'this_pointer_escape_proven': False,
                     'alias_write_excluded': False, 'affects_routing': False})
        if category in {'indirect_unresolved',
                        'controlsbutton_factory_callback_candidate'}:
            plan.append({'kind': category, 'function_entry': row['function_entry'],
                         'op_address': row['op_address'], 'op_time': row['op_time'],
                         'target_or_base': target, 'offset': '-'})
    require(sum(call_kinds.values()) == len(this_calls) and
            sum(store_kinds.values()) == len(unresolved_stores),
            'classification dropped evidence')
    overview = {
        'format': FORMAT, 'record_type': 'summary', 'elf_sha256': SHA,
        'controls_methods_decompiled': len(entries),
        'all_stores': len(stores), 'all_calls': len(calls),
        'direct_field_store_addresses': sorted(direct),
        'unresolved_store_count': len(unresolved_stores),
        'unresolved_store_base_kinds': dict(sorted(store_kinds.items())),
        'unresolved_stores_at_exact_field_offsets': sum(
            r['address_offset'] in FIELDS.values() for r in unresolved_stores),
        'this_bearing_call_count': len(this_calls),
        'callee_categories': dict(sorted(call_kinds.items())),
        'next_probe_sites': len(plan),
        'argument_base_offset_proven': False, 'all_alias_writers_excluded': False,
        'receiver_class_proven': False, 'affects_routing': False,
    }
    return [overview, *rows], plan


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence_archive', type=Path)
    parser.add_argument('original_apk_or_elf', type=Path)
    parser.add_argument('--details-output', type=Path, required=True)
    parser.add_argument('--plan-output', type=Path, required=True)
    args = parser.parse_args()
    if zipfile.is_zipfile(args.original_apk_or_elf):
        with zipfile.ZipFile(args.original_apk_or_elf) as z:
            elf = z.read('lib/armeabi-v7a/libcocos2dcpp.so')
    else:
        elf = args.original_apk_or_elf.read_bytes()
    rows, plan = analyze(read_archive(args.evidence_archive), original_symbols(elf))
    for path in (args.details_output, args.plan_output):
        path.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(
        ''.join(json.dumps(r, sort_keys=True) + '\n' for r in rows), encoding='utf-8')
    with args.plan_output.open('w', encoding='utf-8', newline='') as out:
        writer = csv.DictWriter(
            out, fieldnames=['kind', 'function_entry', 'op_address', 'op_time',
                             'target_or_base', 'offset'], delimiter='\t')
        writer.writeheader()
        writer.writerows(sorted(plan, key=lambda r: (
            r['kind'], r['function_entry'], r['op_address'])))
    summary = rows[0]
    print('format:', FORMAT)
    print('methods:', summary['controls_methods_decompiled'])
    print('STORE:', summary['all_stores'], 'CALL/CALLIND:', summary['all_calls'])
    print('unknown stores:', summary['unresolved_store_base_kinds'])
    print('calls by callee category:', summary['callee_categories'])
    print('next bounded probe sites:', summary['next_probe_sites'])
    print('alias exclusion: not proven; affects routing: no')
    print('details:', args.details_output)
    print('plan:', args.plan_output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
