#!/usr/bin/env python3
"""Static safety contract for the bounded EnemyActionsData constructor probe."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / "tools/unidbg/src/main/java/nevergone/EnemyActionsConstructorProbe.java"
HARNESS = ROOT / "tools/unidbg/src/main/java/nevergone/NevergoneHarness.java"

EXPECTED_ZERO_POINTER_OFFSETS = [
    0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30,
    0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C, 0x50,
    0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C, 0x70,
    0x74, 0x78, 0x7C, 0x80,
    0x84, 0x88, 0x8C, 0x90, 0x94, 0x98, 0x9C, 0xA0,
    0xC8, 0xCC,
]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def parse_hex_array(source: str, field: str) -> list[int]:
    match = re.search(
        rf"{re.escape(field)}\s*=\s*\{{(?P<body>.*?)\}};",
        source,
        flags=re.DOTALL,
    )
    require(match is not None, f"missing {field} array")
    return [int(token, 16) for token in re.findall(r"0x[0-9a-fA-F]+", match.group("body"))]


def verify() -> None:
    probe = PROBE.read_text()
    harness = HARNESS.read_text()

    require('_ZN16EnemyActionsDataC1Ev' in probe, 'wrong/missing constructor symbol')
    require('CONSTRUCTOR_OFFSET = 0x28f394L' in probe, 'wrong constructor offset')
    require('OBJECT_BYTES = 0x400' in probe, 'probe object must stay at 0x400 bytes')
    require('GUARD_BYTES = 16' in probe, 'probe guard changed')
    require('minimum_observed_object_bytes", 0x3f8' in probe, 'minimum observed span missing')

    offsets = parse_hex_array(probe, 'EXPECTED_ZERO_POINTER_OFFSETS')
    require(offsets == EXPECTED_ZERO_POINTER_OFFSETS, 'constructor zero-slot evidence drifted')

    calls = re.findall(r"module\.callFunction\s*\((.*?)\);", probe, flags=re.DOTALL)
    require(len(calls) == 1, f'expected exactly one native call, found {len(calls)}')
    normalized_call = re.sub(r"\s+", " ", calls[0]).strip()
    require(normalized_call == 'emulator, CONSTRUCTOR, object', f'unexpected native call: {normalized_call}')

    require('module.findSymbolByName(CONSTRUCTOR, false)' in probe, 'runtime symbol lookup missing')
    require('& ~1L' in probe, 'Thumb offset normalization missing')
    require('Arrays.equals(guardBefore, guardAfter)' in probe, 'guard preservation check missing')
    require('readLe32(after, offset)' in probe, 'zero-slot runtime validation missing')

    require('--probe-enemy-actions-ctor' in harness, 'harness flag missing')
    require('EnemyActionsConstructorProbe.run(emulator, module)' in harness, 'harness dispatch missing')

    # File-backed methods are mentioned in comments only; the executable probe may
    # not reference their mangled symbols or invoke them through callFunction.
    require('_ZN16EnemyActionsData11loadWBGFile' not in probe, 'loadWBGFile symbol must not enter constructor probe')
    require('_ZN16EnemyActionsData12initWithFile' not in probe, 'initWithFile symbol must not enter constructor probe')


if __name__ == '__main__':
    verify()
    print('EnemyActionsData constructor probe contract: OK')
