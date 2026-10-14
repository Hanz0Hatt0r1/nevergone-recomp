#!/usr/bin/env python3
"""Exact original Thumb witnesses for 37 register-rooted Controls STORE sites."""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import struct
import tarfile
import zipfile
from pathlib import Path

SHA = '94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e'
FORMAT = 'NEVERGONE.ControlsRegisterStoreNativeAudit/1'
TRIAGE = 'NEVERGONE.ControlsFieldAliasTriage/1'
BASE = 'register:register:00000020:4'
# Method Ghidra entry -> original Thumb start, last STORE end (exclusive),
# MOV r4,r0 site, SHA-256 of complete fixed native prefix through last STORE.
METHODS = {
 '002a713c': (0x29713c, 0x2973b8, 0x29714a, 'c0af8bef2605c28f8eba94134f99a41c4148233830986927c77c069682704f69'),
 '002a74c0': (0x2974c0, 0x297522, 0x2974c2, '004d9b7c65897493cf46c79c6eeef8aa068c73b46a2de4fe001b27cb898316e8'),
 '002a7536': (0x297536, 0x297580, 0x297538, '30e4861715f432b42bcfb2a9bab1901e26e0eb533a14541cbfe2da2d7d2943d1'),
 '002a7696': (0x297696, 0x2976ce, 0x2976a4, '84f4226a692a07eb51dbbbb0b56444d649145dbba86621e6c91cc8c04d65747a'),
 '002a78cc': (0x2978cc, 0x2978de, 0x2978ce, '359a5e3ddf064a051cc19ab6552486f3818f6466c2117a1ab3430a2029f589da'),
}
# Exact original ELF Ghidra STORE sites and constant owner offsets.
SITES = {
 '002a713c': '716e:154 7172:110 7176:151 71b2:154 71e0:151 71e4:154 71ea:110 71fa:120 722e:154 7256:158 728c:158 7298:151 729c:158 72a0:110 72d8:158 72f8:158 731e:110 7322:151 7356:158 7362:151 7366:158 73ae:158 73b4:110',
 '002a74c0': '74c8:10c 74cc:164 74d0:165 74d4:118 751e:171',
 '002a7536': '753c:164 7540:165 7546:10c 754c:118 757c:171',
 '002a7696': '769a:164 76c6:10c 76ca:11c',
 '002a78cc': '78da:171',
}
EXPECTED = {('002a'+item.split(':')[0], method): int(item.split(':')[1], 16)
            for method, line in SITES.items() for item in line.split()}


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise ValueError(reason)


def decode_thumb_store(elf: bytes, address: int) -> tuple[int, int, int, int, str]:
    h1, h2 = struct.unpack_from('<HH', elf, address)
    opcode = h1 & 0xfff0
    require(opcode in (0xf8c0, 0xf880), 'expected STR.W or STRB.W immediate')
    width = 4 if opcode == 0xf8c0 else 1
    return width, h1 & 15, (h2 >> 12) & 15, h2 & 0xfff, elf[address:address+4].hex()


def analyze(triage: list[dict], elf: bytes) -> list[dict]:
    require(hashlib.sha256(elf).hexdigest() == SHA, 'original ELF SHA mismatch')
    require(elf[:6] == b'\x7fELF\x01\x01', 'expected ELF32 little-endian')
    require(len(EXPECTED) == 37, 'invalid pinned STORE inventory')
    require(triage and triage[0].get('format') == TRIAGE and
            triage[0].get('elf_sha256') == SHA and
            triage[0].get('unresolved_store_base_kinds') == {
                'register_base': 37, 'loaded_pointer_base': 9},
            'original 38-method triage mismatch')
    for method, (start, end, mov, digest) in METHODS.items():
        require(hashlib.sha256(elf[start:end]).hexdigest() == digest,
                'native Controls method prefix changed: ' + method)
        require(elf[mov:mov+2] == bytes.fromhex('0446'),
                'missing MOV r4,r0: ' + method)
    stores = [r for r in triage[1:] if r.get('record_type') == 'unresolved_store' and
              r.get('base_kind') == 'register_base']
    require(len(stores) == 37, 'expected 37 register-rooted stores')
    observed = {(r['op_address'], r['function_entry']): r for r in stores}
    require(len(observed) == 37 and set(observed) == set(EXPECTED),
            'register STORE site identity mismatch')
    rows = []
    for (addr, method), offset in sorted(EXPECTED.items()):
        row = observed[(addr, method)]
        require(row.get('format') == TRIAGE and row.get('base_token') == BASE and
                row.get('offset') == hex(offset), 'Ghidra STORE root/offset mismatch: ' + addr)
        native = int(addr, 16) - 0x10000
        start, end, mov, _ = METHODS[method]
        require(start <= native and native + 4 <= end, 'STORE outside method prefix: ' + addr)
        width, base, value, immediate, raw = decode_thumb_store(elf, native)
        require(immediate == offset and base in (0, 4),
                'Thumb direct Controls STORE offset/base mismatch: ' + addr)
        # r0 is incoming Controls this before any calls; r4 was set to r0
        # in the pinned prefix and never redefined before these sites
        # (manual Thumb disassembly inspection, SHA-pinned above).
        if base == 0:
            require(method in {'002a74c0', '002a7536', '002a7696'} and
                    native < {'002a74c0': 0x2974d8, '002a7536': 0x29754e,
                              '002a7696': 0x2976a0}[method],
                    'r0 no longer guaranteed to be entry Controls this: ' + addr)
        else:
            require(mov < native, 'r4=this assignment after STORE: ' + addr)
        rows.append({'format': FORMAT, 'record_type': 'direct_owner_store',
                     'function_entry': method, 'ghidra_store_address': addr,
                     'controls_field_offset': hex(offset), 'width_bytes': width,
                     'base_register': f'r{base}', 'value_register': f'r{value}',
                     'original_thumb_bytes': raw,
                     'direct_controls_owner_write': True,
                     'writes_button_or_joystick_pointer_field': False,
                     'all_alias_writers_excluded': False, 'affects_routing': False})
    widths = collections.Counter(r['width_bytes'] for r in rows)
    bases = collections.Counter(r['base_register'] for r in rows)
    offsets = collections.Counter(r['controls_field_offset'] for r in rows)
    require(widths == {4: 24, 1: 13} and bases == {'r4': 28, 'r0': 9},
            'native STORE width/base counts changed')
    require(not (set(offsets) & {'0x124', '0x12c', '0x130', '0x134', '0x174'}),
            'unexpected pointer field overwrite')
    return [{'format': FORMAT, 'record_type': 'summary', 'elf_sha256': SHA,
             'direct_owner_store_count': len(rows),
             'widths': {str(k): v for k, v in sorted(widths.items())},
             'base_registers': dict(sorted(bases.items())),
             'owner_offsets': dict(sorted(offsets.items())),
             'unresolved_local_store_sites_after_native_crosschecks': 0,
             'all_alias_writers_excluded': False,
             'receiver_class_proven': False, 'affects_routing': False}, *rows]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('operand_roots_archive', type=Path)
    p.add_argument('original_apk_or_elf', type=Path)
    p.add_argument('--details-output', required=True, type=Path)
    args = p.parse_args()
    with tarfile.open(args.operand_roots_archive, 'r:gz') as tar:
        m = tar.getmember('controls-field-alias-triage.jsonl')
        require(m.isfile() and m.size < 20_000_000, 'missing/oversized triage')
        triage = [json.loads(x) for x in tar.extractfile(m).read().decode().splitlines()
                  if x.strip()]
    if zipfile.is_zipfile(args.original_apk_or_elf):
        with zipfile.ZipFile(args.original_apk_or_elf) as apk:
            elf = apk.read('lib/armeabi-v7a/libcocos2dcpp.so')
    else:
        elf = args.original_apk_or_elf.read_bytes()
    rows = analyze(triage, elf)
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(''.join(json.dumps(r, sort_keys=True) + '\n' for r in rows))
    print('format:', FORMAT)
    print('direct Controls STOREs:', rows[0]['direct_owner_store_count'])
    print('widths:', rows[0]['widths'], 'bases:', rows[0]['base_registers'])
    print('unresolved local STORE sites:', 0)
    print('whole-program alias exclusion: not proven; affects routing: no')
    print('details:', args.details_output)


if __name__ == '__main__':
    main()
