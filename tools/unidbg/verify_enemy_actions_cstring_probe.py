#!/usr/bin/env python3
"""Static safety contract for the bounded EnemyActionsData CCString prerequisite probe."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / "tools/unidbg/src/main/java/nevergone/EnemyActionsCcStringProbe.java"
HARNESS = ROOT / "tools/unidbg/src/main/java/nevergone/NevergoneHarness.java"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def verify() -> None:
    probe = PROBE.read_text()
    harness = HARNESS.read_text()

    expected = {
        '_ZN7cocos2d8CCStringC1EPKc': 'CONSTRUCTOR_OFFSET = 0x519db4L',
        '_ZNK7cocos2d8CCString10getCStringEv': 'GET_CSTRING_OFFSET = 0x519e8eL',
        '_ZN7cocos2d8CCStringD1Ev': 'DESTRUCTOR_OFFSET = 0x519d04L',
    }
    for symbol, offset in expected.items():
        require(symbol in probe, f'missing CCString symbol {symbol}')
        require(offset in probe, f'wrong/missing offset contract for {symbol}')

    require('OBJECT_BYTES = 0x18' in probe, 'CCString object size contract drifted')
    require('GUARD_BYTES = 16' in probe, 'CCString guard size drifted')
    require('object.getInt(0x14)' in probe, 'CCString +0x14 payload accessor check missing')
    require('Arrays.equals(guardBefore, guardAfterCtor)' in probe,
            'constructor guard preservation check missing')
    require('Arrays.equals(guardBefore, guardAfterDtor)' in probe,
            'destructor guard preservation check missing')
    require('cString.getByteArray(0, inputBytes.length + 1)' in probe,
            'bounded getCString byte read missing')
    require('roundTripBytes[inputBytes.length] != 0' in probe,
            'NUL-terminator validation missing')
    require('Arrays.equals(inputBytes, Arrays.copyOf(roundTripBytes, inputBytes.length))' in probe,
            'CCString byte round-trip check missing')
    require('UnidbgPointer.pointer(emulator, rawCString.longValue())' in probe,
            'getCString pointer conversion missing')
    require('& ~1L' in probe, 'Thumb offset normalization missing')

    calls = re.findall(r"module\.callFunction\s*\((.*?)\);", probe, flags=re.DOTALL)
    normalized = [re.sub(r"\s+", " ", call).strip() for call in calls]
    require(normalized == [
        'emulator, CONSTRUCTOR, object, input',
        'emulator, GET_CSTRING, object',
        'emulator, DESTRUCTOR, object',
    ], f'unexpected native call surface: {normalized}')

    require('--probe-enemy-actions-cstring' in harness, 'harness CCString flag missing')
    require('EnemyActionsCcStringProbe.run(emulator, module)' in harness,
            'harness CCString dispatch missing')

    # This prerequisite probe must stay independent of game filesystem/parser state.
    require('_ZN16EnemyActionsData11loadWBGFile' not in probe,
            'loadWBGFile must not enter CCString prerequisite probe')
    require('_ZN16EnemyActionsData12initWithFile' not in probe,
            'initWithFile must not enter CCString prerequisite probe')
    require('HPData' not in probe, 'HPData must not enter CCString prerequisite probe')
    require('CCFileUtils' not in probe, 'CCFileUtils must not enter CCString prerequisite probe')


if __name__ == '__main__':
    verify()
    print('EnemyActionsData CCString prerequisite probe contract: OK')
