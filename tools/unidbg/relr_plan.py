#!/usr/bin/env python3
"""Build an ELF32 DT_RELR plan; never rewrite an input ELF."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def decode_relr(words):
    cursor = None
    offsets = []
    for word in words:
        if not word & 1:
            if word % 4:
                raise ValueError('Unaligned RELR address')
            offsets.append(word)
            cursor = word + 4
        else:
            if cursor is None:
                raise ValueError('RELR bitmap before address')
            offsets.extend(cursor + 4 * bit for bit in range(31) if word & (1 << (bit + 1)))
            cursor += 31 * 4
    if len(offsets) != len(set(offsets)):
        raise ValueError('Duplicate RELR targets')
    return offsets


def inspect(path):
    data = path.read_bytes()
    if data[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<H', data, 18)[0] != 40:
        raise ValueError(f'Expected ELF32 little-endian ARM: {path.name}')
    phoff = struct.unpack_from('<I', data, 28)[0]
    entsize, count = struct.unpack_from('<HH', data, 42)
    headers = [struct.unpack_from('<8I', data, phoff + i * entsize) for i in range(count)]
    loads = [h for h in headers if h[0] == 1]
    if min(h[2] for h in loads) != 0:
        raise ValueError('Nonzero ELF image base unsupported')
    def offset(va, size=4):
        for h in loads:
            if h[2] <= va and va + size <= h[2] + h[4]:
                return h[1] + va - h[2]
        raise ValueError(f'Address 0x{va:x} is not file backed')
    tags = {}
    for h in headers:
        if h[0] == 2:
            for pos in range(h[1], h[1] + h[4], 8):
                tag, value = struct.unpack_from('<II', data, pos)
                if not tag:
                    break
                tags[tag] = value
    if 36 not in tags:
        return None
    size = tags[35]
    if tags.get(37) != 4 or size % 4:
        raise ValueError('Invalid RELR size/entry size')
    start = offset(tags[36], size)
    words = struct.unpack_from('<' + 'I' * (size // 4), data, start)
    entries = []
    for va in decode_relr(words):
        raw = struct.unpack_from('<I', data, offset(va))[0]
        entries.append([va, raw])
    return {'module': path.name, 'sha256': hashlib.sha256(data).hexdigest(),
            'relr_vaddr': tags[36], 'entries': entries}


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--directory', action='append', required=True)
    p.add_argument('--output', required=True)
    a = p.parse_args()
    found = {}
    for folder in a.directory:
        for path in sorted(Path(folder).glob('*.so')):
            if path.name not in found:
                found[path.name] = path
    result = [item for path in found.values() if (item := inspect(path)) is not None]
    Path(a.output).parent.mkdir(parents=True, exist_ok=True)
    Path(a.output).write_text(json.dumps(result) + '\n')


if __name__ == '__main__':
    main()
