#!/usr/bin/env python3
"""Cross-check 15 bounded Controls operand roots against exact original ARM ELF."""
from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import io
import json
import struct
import tarfile
import zipfile
from pathlib import Path

SHA = '94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e'
FORMAT = 'NEVERGONE.ControlsAliasNativeCrosscheck/1'
ROOT_FORMAT = 'NEVERGONE.GhidraControlsAliasOperandRoots/1'
TRIAGE_FORMAT = 'NEVERGONE.ControlsFieldAliasTriage/1'
AUDIT_FORMAT = 'NEVERGONE.ControlsAliasOperandRootAudit/1'
MEMBERS = {
    'triage': 'controls-field-alias-triage.jsonl',
    'plan': 'controls-field-next-probe-plan.tsv',
    'roots': 'evidence/controls-alias-operand-roots.tsv',
    'audit': 'controls-alias-operand-roots-audit.jsonl',
}
# Ghidra site -> (Controls owner field, child object offset,
#                 exact original Thumb instruction bytes, store width).
STORE_WITNESSES = {
    '002a7530': ('0x124', '0x150', '83f85051', 1),
    '002a758e': ('0x124', '0x150', '83f85051', 1),
    '002a76a8': ('0x124', '0x150', '83f85051', 1),
    '002a76de': ('0x124', '0x150', '83f85011', 1),
    '002a788c': ('0x124', '0x150', '83f85011', 1),
    '002a7b8e': ('0x12c', '0x11d', '82f81d31', 1),
    '002a7bb6': ('0x130', '0x11d', '82f81d31', 1),
    '002a7be0': ('0x134', '0x11d', '82f81d31', 1),
    '002a808c': ('0x124', '0x14c', 'c3f84c21', 4),
}
# Ghidra site -> (Controls vtable slot, Thumb BLX bytes, ELF target symbol).
INDIRECT_WITNESSES = {
    '002a79e6': ('0xb8', '9847', '_ZN7cocos2d6CCNode8addChildEPS0_'),
    '002a7e74': ('0x1ac', '9847', '_ZN7cocos2d7CCLayer15setTouchEnabledEb'),
    '002a7ef2': ('0xb8', '9047', '_ZN7cocos2d6CCNode8addChildEPS0_'),
}
FACTORY_WITNESSES = {'002a8118', '002a816a', '002a81bc'}
R0 = 'register:register:00000020:4'
FACTORY_SYMBOL = '_ZN14ControlsButton6createEPKcS1_PN7cocos2d8CCObjectEMS3_FvvE'


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise ValueError(reason)


def read_jsonl(value: str) -> list[dict]:
    return [json.loads(s) for s in value.splitlines() if s.strip()]


def read_tsv(value: str) -> list[dict]:
    return list(csv.DictReader(io.StringIO(value), delimiter='\t'))


def read_archive(path: Path) -> dict[str, str]:
    with tarfile.open(path, 'r:gz') as tar:
        files = [m for m in tar.getmembers() if m.isfile()]
        require(len(files) == len(set(m.name for m in files)), 'duplicate archive members')
        require(set(MEMBERS.values()).issubset(m.name for m in files), 'missing evidence member')
        require(all(m.size <= 20_000_000 for m in files), 'oversized archive member')
        return {key: tar.extractfile(tar.getmember(name)).read().decode('utf-8')
                for key, name in MEMBERS.items()}


def elf_inventory(elf: bytes) -> tuple[dict[str, tuple[int, int, int]], dict[int, str],
                                      list[tuple[int, int, int]], set[int]]:
    """Read ELF32 sections, dynamic function/object symbols and RELATIVE relocations."""
    require(hashlib.sha256(elf).hexdigest() == SHA, 'original ELF SHA mismatch')
    require(elf[:6] == b'\x7fELF\x01\x01', 'expected little-endian ELF32')
    shoff = struct.unpack_from('<I', elf, 32)[0]
    shentsize, shnum = struct.unpack_from('<HH', elf, 46)
    require(shentsize >= 40 and shnum > 0 and shoff + shentsize * shnum <= len(elf),
            'ELF section bounds invalid')
    sections = [struct.unpack_from('<IIIIIIIIII', elf, shoff + i * shentsize)
                for i in range(shnum)]
    sym_sections = [s for s in sections if s[1] == 11]
    require(len(sym_sections) == 1, 'expected one ELF dynamic symbol section')
    dyn = sym_sections[0]
    require(dyn[9] == 16 and dyn[5] % 16 == 0 and dyn[6] < len(sections),
            'unexpected dynamic symbol layout')
    strings = sections[dyn[6]]
    require(strings[4] + strings[5] <= len(elf) and dyn[4] + dyn[5] <= len(elf),
            'symbol/string table exceeds file')
    string_table = elf[strings[4]:strings[4] + strings[5]]
    symbols: dict[str, tuple[int, int, int]] = {}
    functions: dict[int, str] = {}
    for off in range(dyn[4], dyn[4] + dyn[5], 16):
        name_offset, value, size, info, _other, shndx = struct.unpack_from('<IIIBBH', elf, off)
        if not value or not size or (info & 15) not in (1, 2):
            continue
        require(name_offset < len(string_table), 'invalid ELF symbol name offset')
        end = string_table.find(b'\x00', name_offset)
        require(end >= 0, 'unterminated ELF symbol')
        name = string_table[name_offset:end].decode('utf-8', 'replace')
        symbols[name] = (value, size, shndx)
        if (info & 15) == 2:
            functions.setdefault(value & ~1, name)
    rels: set[int] = set()
    for section in sections:
        if section[1] != 9:
            continue
        require(section[9] == 8 and section[5] % 8 == 0 and
                section[4] + section[5] <= len(elf), 'unexpected REL layout')
        for pos in range(section[4], section[4] + section[5], 8):
            addr, info = struct.unpack_from('<II', elf, pos)
            if (info & 255) == 23:  # R_ARM_RELATIVE
                rels.add(addr)
    return symbols, functions, sections, rels


def virtual_to_file(address: int, size: int, sections: list[tuple]) -> int:
    matching = [s for s in sections if s[3] <= address and address + size <= s[3] + s[5]
                and s[1] == 1]  # SHT_PROGBITS
    require(len(matching) == 1, 'virtual address not in unique PROGBITS section')
    s = matching[0]
    return s[4] + address - s[3]


def verify_vtable(elf: bytes, symbols: dict, functions: dict, sections: list,
                  relocations: set[int]) -> dict[str, dict]:
    require('_ZTV8Controls' in symbols, 'missing Controls vtable')
    base, size, _ = symbols['_ZTV8Controls']
    require(base == 0x8e4490 and size == 592, 'Controls vtable identity changed')
    out = {}
    for slot, _insn, name in INDIRECT_WITNESSES.values():
        if slot in out:
            continue
        offset = int(slot, 16)
        require(offset + 12 <= size, 'vtable slot outside table')
        address = base + 8 + offset  # Itanium ABI vptr address point
        require(address in relocations, 'vtable entry lacks R_ARM_RELATIVE relocation')
        target = struct.unpack_from('<I', elf, virtual_to_file(address, 4, sections))[0]
        require(target & 1 == 1, 'expected Thumb function target')
        require(functions.get(target & ~1) == name, 'vtable target symbol mismatch')
        out[slot] = {'original_elf_target': f'{target & ~1:08x}',
                     'ghidra_target': f'{(target & ~1) + 0x10000:08x}',
                     'symbol': name, 'vtable_entry': f'{address:08x}'}
    return out


def index_roots(plan: list[dict], roots: list[dict]) -> dict[tuple, dict]:
    expected = {(r['kind'], r['function_entry'], r['op_address'], r['op_time']): r
                for r in plan}
    require(len(expected) == 15, 'plan has duplicate sites')
    indexed = collections.defaultdict(lambda: {'summary': [], 'operand': []})
    for r in roots:
        require(r.get('format') == ROOT_FORMAT, 'unexpected operand root format')
        k = (r['kind'], r['function_entry'], r['site_address'], r['site_time'])
        require(k in expected, 'operand root outside plan')
        require(r['row_type'] in {'summary', 'operand'}, 'unexpected root row type')
        indexed[k][r['row_type']].append(r)
    require(set(indexed) == set(expected), 'missing planned root sites')
    result = {}
    for k, parts in indexed.items():
        require(len(parts['summary']) == 1 and parts['summary'][0]['site_status'] == 'ok',
                'incomplete site summary')
        operands = parts['operand']
        require(len(operands) == int(parts['summary'][0]['operand_count']),
                'operand count mismatch')
        roles = {r['operand_role']: r for r in operands}
        require(len(roles) == len(operands), 'duplicate operand role')
        result[k] = roles
    return result


def verify_store(site: dict, operands: dict, elf: bytes, sections: list) -> dict:
    addr = site['op_address']
    require(addr in STORE_WITNESSES, 'unexpected STORE site')
    field, offset, bytes_hex, width = STORE_WITNESSES[addr]
    dest = operands['store_destination']
    value = operands['store_value']
    require(site['offset'] == offset and
            site['target_or_base'] == dest['root_token'] and
            dest['root_token'].startswith('def:LOAD@') and
            dest['root_offset'] == offset and
            dest['load_address_offset'] == field and
            dest['load_address_root_token'] == R0,
            'STORE pointer root/field mismatch: ' + addr)
    require(value['operand_token'].endswith(f':{width}'),
            'STORE width mismatch: ' + addr)
    native = int(addr, 16) - 0x10000
    observed = elf[virtual_to_file(native, 4, sections):][:4].hex()
    require(observed == bytes_hex, 'original Thumb STORE instruction mismatch: ' + addr)
    if width == 4:
        require(addr == '002a808c' and
                value['root_token'] == R0 and
                value['root_parameter_index'] == '0' and
                value['root_offset'] == '0x104',
                'joystick back-pointer value provenance mismatch')
        meaning = 'joystick_back_pointer_to_controls_plus_0x104'
    else:
        require(value['root_kind'] in {'constant', 'leaf'}, 'byte flag value root mismatch')
        meaning = 'child_object_byte_flag_write'
    owner = 'CCJoyStick_factory_candidate' if field == '0x124' else 'ControlsButton_factory_candidate'
    return {'format': FORMAT, 'kind': 'store', 'site': addr,
            'controls_pointer_field': field, 'child_object_offset': offset,
            'native_store_width_bytes': width, 'original_thumb_bytes': observed,
            'construction_linked_child_class_candidate': owner,
            'classification': meaning, 'pointer_field_overwrite_proven': False,
            'all_alias_writers_excluded': False, 'affects_routing': False}


def verify_indirect(site: dict, operands: dict, elf: bytes, sections: list,
                    vtable: dict) -> dict:
    addr = site['op_address']
    require(addr in INDIRECT_WITNESSES, 'unexpected indirect call site')
    slot, insn, symbol = INDIRECT_WITNESSES[addr]
    target = operands['call_target']
    receiver = operands['call_arg_0']
    require(site['target_or_base'] == target['root_token'] and
            target['root_token'].startswith('def:LOAD@') and
            target['load_address_root_token'].startswith('def:LOAD@') and
            target['load_address_offset'] == slot and
            target['nested_load_address_offset'] == '0x0' and
            target['nested_load_address_root_token'] == R0 and
            target['nested_load_address_parameter_index'] == '0' and
            receiver['root_token'] == R0 and receiver['root_parameter_index'] == '0',
            'indirect receiver/vtable shape mismatch: ' + addr)
    native = int(addr, 16) - 0x10000
    observed = elf[virtual_to_file(native, 2, sections):][:2].hex()
    require(observed == insn and vtable[slot]['symbol'] == symbol,
            'indirect native BLX/vtable target mismatch: ' + addr)
    return {'format': FORMAT, 'kind': 'indirect', 'site': addr,
            'vtable_slot': slot, 'original_thumb_bytes': observed,
            'controls_vtable_target_candidate': vtable[slot]['ghidra_target'],
            'controls_vtable_symbol': symbol,
            'receiver_parameter_zero_proven_for_this_call': True,
            'dynamic_receiver_class_proven': False,
            'affects_routing': False}


def verify_factory(site: dict, operands: dict, symbols: dict) -> dict:
    addr = site['op_address']
    require(addr in FACTORY_WITNESSES, 'unexpected factory call site')
    require(FACTORY_SYMBOL in symbols and
            (symbols[FACTORY_SYMBOL][0] & ~1) + 0x10000 == 0x2a85fc,
            'factory ELF symbol mismatch')
    target = operands['call_target']
    callback = operands['call_arg_2']
    require(target['root_token'] == 'address:002a85fc:8' and
            callback['root_token'] == R0 and callback['root_parameter_index'] == '0' and
            callback['root_offset'] == '0x0',
            'factory callback Controls this argument mismatch: ' + addr)
    return {'format': FORMAT, 'kind': 'factory', 'site': addr,
            'callee_symbol': FACTORY_SYMBOL,
            'callback_object_argument_is_controls_this': True,
            'factory_stores_callback_pointer_proven': False,
            'affects_routing': False}


def analyze(archive: dict[str, str], elf: bytes) -> list[dict]:
    triage = read_jsonl(archive['triage'])
    require(triage and triage[0].get('format') == TRIAGE_FORMAT and
            triage[0].get('elf_sha256') == SHA and
            triage[0].get('unresolved_store_base_kinds') == {
                'loaded_pointer_base': 9, 'register_base': 37} and
            triage[0].get('next_probe_sites') == 15 and
            triage[0].get('affects_routing') is False,
            'upstream Controls triage mismatch')
    plan = read_tsv(archive['plan'])
    roots = read_tsv(archive['roots'])
    prior = read_jsonl(archive['audit'])
    require(len(prior) == 1 and prior[0].get('format') == AUDIT_FORMAT and
            prior[0].get('status') == 'bounded_operand_roots_complete' and
            prior[0].get('sites_complete') == 15 and prior[0].get('reasons') == [] and
            prior[0].get('all_alias_writers_excluded') is False and
            prior[0].get('affects_routing') is False,
            'upstream 15-site audit not complete')
    kinds = collections.Counter(p['kind'] for p in plan)
    require(kinds == {'store_loaded_pointer_base': 9, 'indirect_unresolved': 3,
                      'controlsbutton_factory_callback_candidate': 3},
            'unexpected 15-site plan shape')
    site_addresses = {p['op_address'] for p in plan}
    require(len(site_addresses) == 15 and site_addresses == (
        set(STORE_WITNESSES) | set(INDIRECT_WITNESSES) | FACTORY_WITNESSES),
        'plan site address identity mismatch')
    index = index_roots(plan, roots)
    symbols, functions, sections, relocations = elf_inventory(elf)
    vtable = verify_vtable(elf, symbols, functions, sections, relocations)
    rows = []
    for site in plan:
        k = (site['kind'], site['function_entry'], site['op_address'], site['op_time'])
        operands = index[k]
        if site['kind'] == 'store_loaded_pointer_base':
            rows.append(verify_store(site, operands, elf, sections))
        elif site['kind'] == 'indirect_unresolved':
            rows.append(verify_indirect(site, operands, elf, sections, vtable))
        else:
            rows.append(verify_factory(site, operands, symbols))
    counts = collections.Counter(r['kind'] for r in rows)
    width_counts = collections.Counter(r['native_store_width_bytes'] for r in rows if r['kind'] == 'store')
    summary = {'format': FORMAT, 'record_type': 'summary', 'elf_sha256': SHA,
               'site_count': len(rows), 'site_kinds': dict(sorted(counts.items())),
               'store_width_bytes': {str(k): v for k, v in sorted(width_counts.items())},
               'controls_vtable_slots': vtable,
               'remaining_register_based_unknown_stores': 37,
               'all_alias_writers_excluded': False,
               'dynamic_receiver_class_proven': False,
               'affects_routing': False}
    return [summary, *rows]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operand_roots_archive', type=Path)
    parser.add_argument('original_apk_or_elf', type=Path)
    parser.add_argument('--details-output', required=True, type=Path)
    args = parser.parse_args()
    if zipfile.is_zipfile(args.original_apk_or_elf):
        with zipfile.ZipFile(args.original_apk_or_elf) as apk:
            elf = apk.read('lib/armeabi-v7a/libcocos2dcpp.so')
    else:
        elf = args.original_apk_or_elf.read_bytes()
    result = analyze(read_archive(args.operand_roots_archive), elf)
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(''.join(json.dumps(r, sort_keys=True) + '\n' for r in result), encoding='utf-8')
    print('format:', FORMAT)
    print('sites:', result[0]['site_count'], result[0]['site_kinds'])
    print('STORE widths:', result[0]['store_width_bytes'])
    print('Controls vtable slots:', result[0]['controls_vtable_slots'])
    print('remaining register-based stores:', result[0]['remaining_register_based_unknown_stores'])
    print('alias exclusion: not proven; affects routing: no')
    print('details:', args.details_output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
