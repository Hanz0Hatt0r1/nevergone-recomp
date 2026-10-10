#!/usr/bin/env python3
"""Compare native synthetic results with the repository's existing decoder."""
import argparse
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from asset_decoder import decode_bytes

def verify(path):
    rows = [json.loads(line.removeprefix('EVIDENCE ')) for line in path.read_text().splitlines()
            if line.startswith('EVIDENCE ') or line.startswith('{')]
    decode = [r for r in rows if r['kind'] == 'asset_decode']
    expected_cases = {(size,key,inplace) for size in (0,1,2,126,127,128,253,254,255,1024)
                      for key in (0,1,127,255,0x123) for inplace in (False,True)}
    assert len(decode) == 100
    assert {(r['size'],r['key'],r['inplace']) for r in decode} == expected_cases
    for r in decode:
        source = bytes((i*73+19)&255 for i in range(r['size']))
        assert bytes.fromhex(r['output_hex']) == decode_bytes(source,r['key']), r
        assert r['guard'] == 90, r
    contains = [r for r in rows if r['kind'] == 'contains_point']
    intersects = [r for r in rows if r['kind'] == 'intersects_rect']
    assert [r['result'] for r in contains] == [1,1,1,0,0,0]
    assert [r['result'] for r in intersects] == [1,0,1,0,1,0]
    assert len(rows) == 112
    print('PASS: 100 native decoder comparisons, 100 trailing guards, 12 geometry observations')

if __name__ == '__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('evidence',type=Path)
    verify(parser.parse_args().evidence)
