#!/usr/bin/env python3
"""Check native GameSaveData leaf-transform evidence against independent Python logic."""
import argparse
import json
from pathlib import Path

from save_probe_plan import plan


def transform(data, key, encode):
    output = bytearray()
    counter = 0
    for value in data:
        output.append(((((value + counter) & 255) ^ key) if encode else ((value ^ key) - counter)) & 255)
        counter = (counter + 1) % 127
    return bytes(output)


def verify(path):
    rows = [json.loads(line.removeprefix('EVIDENCE ')) for line in Path(path).read_text().splitlines()
            if line.startswith('EVIDENCE ') or line.startswith('{')]
    specs = plan()
    assert len(rows) == len(specs), (len(rows), len(specs))
    for index, (row, spec) in enumerate(zip(rows, specs)):
        assert row['schema'] == 1 and row['kind'] == 'probe_symbol' and row['case'] == index
        assert row['module'] == 'libcocos2dcpp.so' and row['arch'] == 'ARM32 Thumb'
        assert row['symbol'] == spec['symbol'] and row['offset'] == spec['offset']
        assert row['args'] == ['null_this', 'source', len(bytes.fromhex(spec['input_hex'])), 'destination', spec['key']]
        before = row['memory_before']; after = row['memory_after']
        source = bytes.fromhex(spec['input_hex'])
        assert before['source_hex'] == spec['input_hex'] and before['guard'] == 90
        assert before['destination_hex'] == '00' * len(source)
        assert after['source_hex'] == spec['input_hex'] and after['guard'] == 90
        assert bytes.fromhex(after['destination_hex']) == transform(source, spec['key'], 'Encode' in spec['symbol'])
        assert row['return_value'] == len(source)
        assert row['trace']['mode'] == 'none'
        assert all(k in row['registers_before_call'] and k in row['registers_after_call'] for k in ('r0', 'r1', 'r2', 'r3', 'sp', 'lr', 'pc'))
    print(f'PASS: {len(rows)} native GameSaveData Encode/Decode comparisons and guards')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('evidence', type=Path)
    verify(parser.parse_args().evidence)
