#!/usr/bin/env python3
"""Generate bounded GameSaveData Encode/Decode calls with reproducible data."""
import argparse
import json
from pathlib import Path

SYMBOLS = (
    ('_ZN12GameSaveData6EncodeEPciS0_j', '0x33a766'),
    ('_ZN12GameSaveData6DecodeEPciS0_j', '0x33a788'),
)


def plan():
    return [
        {'symbol': symbol, 'offset': offset, 'key': key,
         'input_hex': bytes((i * 73 + 19) & 255 for i in range(size)).hex()}
        for symbol, offset in SYMBOLS
        for key in (0, 1, 0x123)
        for size in (0, 1, 126, 127, 128, 255)
    ]


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(plan(), indent=2) + '\n')
