#!/usr/bin/env python3
"""Bound the two residual Controls Cocos back-reference hazards in the exact APK.

This checker joins the retained-parent/touch facts with the original Controls
vtable, inherited CCNode parent consumers and packaged script-engine setter
reachability. It is deliberately conservative about dynamic symbol lookup and
runtime code injection, so it does not authorize receiver routing.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zipfile
from collections import Counter
from pathlib import Path

from audit_nevergone_controls_cocos_backrefs import Elf, SHA, req

FORMAT = 'NEVERGONE.ControlsParentScriptStaticClosure/1'
DANGEROUS = {0x124, 0x12c, 0x130, 0x134, 0x174}
SETTER = '_ZN7cocos2d21CCScriptEngineManager15setScriptEngineEPNS_22CCScriptEngineProtocolE'
SHARED = '_ZN7cocos2d21CCScriptEngineManager13sharedManagerEv'

HASHES = {
    '_ZN7cocos2d6CCNode12reorderChildEPS0_i': (0x515e20, 52, '630393dddb2bdc5563c9829e56cad1d8235b09e809226b63c0426db95e0e8ff1'),
    '_ZN7cocos2d6CCNode11removeChildEPS0_b': (0x516c1e, 36, '23274966faa32352b2b3b5792e65cc0182b38084c80c7981319f6705908471fa'),
    '_ZN7cocos2d6CCNode21nodeToParentTransformEv': (0x516200, 600, 'a4d7fb38ffaa9deb036430df007f682f79d6a0f59b7180a7e99b8f4f96372e17'),
    '_ZN7cocos2d6CCNode9getParentEv': (0x515d46, 6, '91a7a13cf1190052522b521426e4d512bb2c17cd4bc3b4708eb961838cd14b27'),
    '_ZN7cocos2d6CCNode9transformEv': (0x516cc8, 134, '2ab32f86809750ba2e251ce30ffe2f01092db8d7b06eaef96231abc9d2e161fd'),
    SETTER: (0x536ccc, 14, '5c78fbaa10ff0291be22e10d29a30581daeec6516dfce4c5cc67dd142a21d6fa'),
    SHARED: (0x536cdc, 36, 'f844044568aa40dd76c54caad74d12ea39ac263e1c89055a38a22d036a0b6c64'),
    '_ZN10lua_system7initLuaEv': (0x4ca17c, 236, 'c97d79ae61ad2c9506bec45a0f3a60a6600632434e527b075679230f0e2e4f2c'),
    '_ZN10lua_system9uninitLuaEv': (0x4ca268, 24, '6572959d9ab477bee5f0f3c2b038fd46204cf562589bc3e8564ff323a5a60985'),
}

# Parent-consuming virtual slots when the parent object is Controls.
PARENT_SLOTS = {
    0x0cc: ('_ZN7cocos2d6CCNode9getParentEv', 0x515d46),
    0x0dc: ('_ZN7cocos2d6CCNode11removeChildEPS0_b', 0x516c1e),
    0x0f0: ('_ZN7cocos2d6CCNode12reorderChildEPS0_i', 0x515e20),
    0x160: ('_ZN7cocos2d6CCNode21nodeToParentTransformEv', 0x516200),
    # CCSprite's recursive dirty propagation can hit this offset on a non-sprite
    # parent. For Controls it resolves to inherited CCLayer multi-touch ended,
    # which collapses into the separately bounded script-engine channel.
    0x210: ('_ZThn224_N7cocos2d7CCLayer14ccTouchesEndedEPNS_5CCSetEPNS_7CCEventE', 0x525390),
}

EXPECTED_PARENT_TARGET_STORES = {
    '_ZN7cocos2d6CCNode9getParentEv': [],
    '_ZN7cocos2d6CCNode11removeChildEPS0_b': [],
    '_ZN7cocos2d6CCNode12reorderChildEPS0_i': [(0x515e24, 0xd2, 1)],
    '_ZN7cocos2d6CCNode21nodeToParentTransformEv': [(0x51641a, 0xcf, 1), (0x516420, 0xcd, 1)],
    '_ZN7cocos2d6CCNode9transformEv': [],
}


def function_hashes(elf: Elf) -> None:
    for name, (address, size, digest) in HASHES.items():
        elf.sym(name, address, size)
        req(hashlib.sha256(elf.at(address, size)).hexdigest() == digest,
            'function changed: ' + name)


def immediate_stores(elf: Elf, address: int, size: int) -> list[tuple[int, int, int]]:
    result = []
    for pc in range(address, address + size - 3, 2):
        first, second = struct.unpack('<HH', elf.at(pc, 4))
        opcode = first & 0xfff0
        if opcode in (0xf8c0, 0xf880):
            result.append((pc, second & 0xfff, 4 if opcode == 0xf8c0 else 1))
    return result


def verify_parent_targets(elf: Elf) -> dict:
    controls, size = elf.sym('_ZTV8Controls', 0x8e4490, 592)
    req((controls & ~1) == 0x8e4490 and size == 592, 'Controls vtable changed')
    resolved = {}
    for slot, (name, address) in PARENT_SLOTS.items():
        entry = 0x8e4490 + 8 + slot
        req(elf.rels.get(entry) == 23, 'Controls parent slot relocation changed')
        target, _ = elf.sym(name, address)
        req(elf.u32(entry) == target, 'Controls parent slot target changed')
        resolved[hex(slot)] = {'symbol': name, 'target': f'0x{target & ~1:08x}'}

    stores = {}
    for name, expected in EXPECTED_PARENT_TARGET_STORES.items():
        address, size, _ = HASHES[name]
        actual = immediate_stores(elf, address, size)
        req(actual == expected, 'parent target STORE inventory changed: ' + name)
        req(not ({offset for _, offset, _ in actual} & DANGEROUS),
            'parent target directly writes a dangerous Controls offset: ' + name)
        stores[name] = [
            {'address': f'0x{pc:08x}', 'offset': hex(offset), 'width': width}
            for pc, offset, width in actual
        ]

    # transformAncestors directly tails to CCNode::transform on each parent.
    req(elf.at(0x516e10, 22).hex() ==
        'd0f8a80038b1fff7f9ffd4f8a800bde81040fff751bf',
        'transformAncestors parent path changed')
    return {
        'controls_parent_vtable_slots': resolved,
        'native_parent_target_direct_stores': stores,
        'native_parent_targets_write_dangerous_fields': False,
        'sprite_slot_0x210_collapses_to_script_touch_path': True,
    }


def decode_thumb_branch_target(pc: int, first: int, second: int) -> int | None:
    # Thumb-2 BL or unconditional B.W T4. This is sufficient for a target that
    # is itself Thumb code; literal/register targets are checked separately.
    if (first & 0xf800) != 0xf000:
        return None
    pattern = second & 0xd000
    if pattern not in (0xd000, 0x9000):
        return None
    s = (first >> 10) & 1
    j1 = (second >> 13) & 1
    j2 = (second >> 11) & 1
    i1 = 1 ^ (j1 ^ s)
    i2 = 1 ^ (j2 ^ s)
    imm = ((s << 24) | (i1 << 23) | (i2 << 22) |
           ((first & 0x3ff) << 12) | ((second & 0x7ff) << 1))
    if s:
        imm -= 1 << 25
    return (pc + 4 + imm) & 0xffffffff


def static_branches_to(elf: Elf, target: int) -> list[int]:
    section = elf.byname['.text']
    start, size = section[3], section[5]
    data = elf.at(start, size)
    result = []
    for offset in range(0, len(data) - 3, 2):
        first, second = struct.unpack_from('<HH', data, offset)
        if decode_thumb_branch_target(start + offset, first, second) == target:
            result.append(start + offset)
    return result


def progbits_pointer_refs(elf: Elf, pointer: int) -> list[dict]:
    needle = struct.pack('<I', pointer)
    result = []
    for name, section in elf.byname.items():
        if section[1] != 1 or section[5] == 0:
            continue
        blob = elf.data[section[4]:section[4] + section[5]]
        begin = 0
        while True:
            pos = blob.find(needle, begin)
            if pos < 0:
                break
            result.append({'section': name, 'virtual_address': f'0x{section[3] + pos:08x}'})
            begin = pos + 1
    return result


def verify_script_static_reachability(elf: Elf, apk: zipfile.ZipFile) -> dict:
    setter, _ = elf.sym(SETTER, 0x536ccc, 14)
    setter_address = setter & ~1
    branches = static_branches_to(elf, setter_address)
    req(branches == [], 'packaged native text directly calls setScriptEngine')

    refs_even = progbits_pointer_refs(elf, setter_address)
    refs_thumb = progbits_pointer_refs(elf, setter_address | 1)
    req(refs_even == [] and refs_thumb == [],
        'packaged PROGBITS retains setScriptEngine function pointer')

    dex_names = sorted(name for name in apk.namelist() if name.startswith('classes') and name.endswith('.dex'))
    req(dex_names, 'APK has no classes*.dex')
    dex_needles = [b'setScriptEngine', b'CCScriptEngineManager', SETTER.encode()]
    dex_hits = []
    for name in dex_names:
        data = apk.read(name)
        for needle in dex_needles:
            if needle in data:
                dex_hits.append({'dex': name, 'needle': needle.decode('ascii')})
    req(dex_hits == [], 'DEX refers to Cocos script engine setter/manager')

    native_libs = sorted(name for name in apk.namelist() if name.startswith('lib/') and name.endswith('.so'))
    req(native_libs == ['lib/armeabi-v7a/libcocos2dcpp.so', 'lib/armeabi-v7a/libffmpeg.so'],
        'APK native library inventory changed')

    concrete = sorted(name for name in elf.symbols if
                      ('CCLuaEngine' in name or 'CCScriptEngineProtocol' in name) and
                      name != SETTER)
    # Protocol method references may legitimately occur in mangled signatures;
    # there must be no linked concrete CCLuaEngine implementation.
    req(not [name for name in concrete if 'CCLuaEngine' in name],
        'linked CCLuaEngine implementation appeared')

    # The manager singleton starts its sole engine field at zero when allocated.
    req(elf.at(0x536ce6, 10).hex() == '0420c7f122fb05600060',
        'CCScriptEngineManager zero initialization changed')
    # CCNode construction stores manager->engine (or zero) at node +0xdc.
    req(elf.at(0x516a48, 18).hex() == '20f048f9006810b103689b689847c4f8dc00',
        'CCNode script engine capture changed')

    return {
        'direct_thumb_branches_to_set_script_engine': [],
        'progbits_function_pointer_refs_to_set_script_engine': [],
        'dex_setter_or_manager_references': [],
        'packaged_native_libraries': native_libs,
        'linked_ccluaengine_symbols': [],
        'manager_engine_initial_value': 'null',
        'packaged_static_script_engine_assignment_found': False,
        # dlopen/dlsym, injected code, native hooks, or external runtime mutation
        # are deliberately outside this static APK proof.
        'dynamic_symbol_lookup_or_runtime_injection_excluded': False,
    }


def analyze(apk_path: Path) -> dict:
    req(zipfile.is_zipfile(apk_path), 'exact APK is required for DEX/native inventory checks')
    with zipfile.ZipFile(apk_path) as apk:
        data = apk.read('lib/armeabi-v7a/libcocos2dcpp.so')
        elf = Elf(data)
        function_hashes(elf)
        parent = verify_parent_targets(elf)
        script = verify_script_static_reachability(elf, apk)
    return {
        'format': FORMAT,
        'elf_sha256': SHA,
        'parent_channel': parent,
        'script_engine_channel': script,
        'known_parent_native_dangerous_write_through_excluded': True,
        'packaged_static_script_engine_write_through_reachable': False,
        'dynamic_runtime_script_engine_assignment_excluded': False,
        'whole_program_alias_writes_excluded': False,
        'receiver_class_proven': False,
        'affects_routing': False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original_apk', type=Path)
    parser.add_argument('--details-output', type=Path, required=True)
    args = parser.parse_args()
    report = analyze(args.original_apk)
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(json.dumps(report, sort_keys=True) + '\n', encoding='utf-8')
    print('format:', FORMAT)
    print('parent native dangerous write-through excluded: yes')
    print('packaged static script-engine setter reachable: no')
    print('remaining residual: dynamic symbol lookup/runtime script engine injection')
    print('whole-program alias exclusion: no; affects routing: no')
    print('details:', args.details_output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
