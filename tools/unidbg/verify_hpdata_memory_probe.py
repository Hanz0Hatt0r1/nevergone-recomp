#!/usr/bin/env python3
"""Static safety contract for the bounded in-memory HPData unidbg probe."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / "tools/unidbg/src/main/java/nevergone/HpDataMemoryProbe.java"
HARNESS = ROOT / "tools/unidbg/src/main/java/nevergone/NevergoneHarness.java"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def verify() -> None:
    probe = PROBE.read_text()
    harness = HARNESS.read_text()

    expected = {
        '_ZN6HPDataC1EPKhm': 'CONSTRUCTOR_OFFSET = 0x2c64acL',
        '_ZNK6HPData8getBytesEPc7HPRange': 'GET_CHARS_OFFSET = 0x2c6568L',
        '_ZNK6HPData8getBytesERi7HPRange': 'GET_INT_OFFSET = 0x2c658aL',
        '_ZNK6HPData8getBytesERj7HPRange': 'GET_UNSIGNED_OFFSET = 0x2c65acL',
        '_ZNK6HPData8getBytesERf7HPRange': 'GET_FLOAT_OFFSET = 0x2c65ceL',
        '_ZNK6HPData8getBytesERb7HPRange': 'GET_BOOL_OFFSET = 0x2c65f0L',
        '_ZN6HPDataD1Ev': 'DESTRUCTOR_OFFSET = 0x2c6470L',
    }
    for symbol, offset in expected.items():
        require(symbol in probe, f'missing HPData symbol {symbol}')
        require(offset in probe, f'wrong/missing offset contract for {symbol}')

    require('OBJECT_BYTES = 0x1c' in probe, 'HPData object size contract drifted')
    require('GUARD_BYTES = 16' in probe, 'HPData object guard drifted')
    require('object.getInt(0x14) != INPUT.length' in probe, 'HPData length slot check missing')
    require('object.getInt(0x18)' in probe, 'HPData owned-buffer slot check missing')
    require('owned.getByteArray(0, INPUT.length + 1)' in probe, 'HPData owned-copy check missing')
    require('ownedBytes[INPUT.length] != 0' in probe, 'HPData owned NUL check missing')
    require('object.getInt(0x14) != 0 || object.getInt(0x18) != 0' in probe,
            'HPData destructor clear check missing')
    require('Arrays.equals(guardBefore, guardAfterCtor)' in probe,
            'HPData constructor guard check missing')
    require('Arrays.equals(guardBefore, guardAfterDtor)' in probe,
            'HPData destructor guard check missing')
    require('& ~1L' in probe, 'Thumb offset normalization missing')

    calls = re.findall(r"module\.callFunction\s*\((.*?)\);", probe, flags=re.DOTALL)
    normalized = [re.sub(r"\s+", " ", call).strip() for call in calls]
    require(normalized == [
        'emulator, CONSTRUCTOR, object, input, INPUT.length',
        'emulator, GET_INT, object, output.share(0), 0, 4',
        'emulator, GET_FLOAT, object, output.share(4), 4, 4',
        'emulator, GET_UNSIGNED, object, output.share(8), 8, 4',
        'emulator, GET_BOOL, object, output.share(12), 12, 1',
        'emulator, GET_CHARS, object, output.share(13), 13, 3',
        'emulator, DESTRUCTOR, object',
    ], f'unexpected native call surface: {normalized}')

    require('output.getInt(0) != 0x78563412' in probe, 'int result check missing')
    require('output.getInt(4) != 0x3fc00000' in probe, 'float-bit result check missing')
    require('0x89abcdefL' in probe, 'unsigned result check missing')
    require('output.getByte(12) != 1' in probe, 'bool result check missing')
    require("new byte[] {'W', 'B', 'G'}" in probe, 'char result check missing')

    require('--probe-hpdata-memory' in harness, 'harness HPData memory flag missing')
    require('HpDataMemoryProbe.run(emulator, module)' in harness,
            'harness HPData memory dispatch missing')

    # No file-backed HPData or WBG parser symbol may enter this bounded probe.
    require('_ZN6HPData24createWithContentsOfFileEPKc' not in probe,
            'file-backed HPData constructor must not enter memory probe')
    require('_ZN16EnemyActionsData11loadWBGFile' not in probe,
            'EnemyActionsData loadWBGFile must not enter memory probe')


if __name__ == '__main__':
    verify()
    print('HPData memory probe contract: OK')
