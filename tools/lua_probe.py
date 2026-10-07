#!/usr/bin/env python3
"""Inspect Lua-like payloads in the original APK without extracting game assets."""
from __future__ import annotations

import argparse
import collections
import math
import zipfile
from pathlib import Path


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = collections.Counter(data)
    n = len(data)
    return -sum((c / n) * math.log2(c / n) for c in counts.values())


def printable_ratio(data: bytes) -> float:
    if not data:
        return 0.0
    return sum((32 <= b < 127) or b in (9, 10, 13) for b in data) / len(data)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("apk", type=Path)
    p.add_argument("--limit", type=int, default=40, help="number of header groups to print")
    args = p.parse_args()

    with zipfile.ZipFile(args.apk) as zf:
        infos = [i for i in zf.infolist() if i.filename.lower().endswith(".lua")]
        headers: dict[bytes, list[str]] = collections.defaultdict(list)
        ratios = []
        entropies = []
        plain = bytecode = encoded = 0
        for info in infos:
            data = zf.read(info)
            headers[data[:8]].append(info.filename)
            sample = data[: min(4096, len(data))]
            r = printable_ratio(sample)
            e = entropy(sample)
            ratios.append(r)
            entropies.append(e)
            if data.startswith(b"\x1bLua"):
                bytecode += 1
            elif r > 0.85 and any(x in sample for x in (b"function ", b"local ", b"require", b"return ")):
                plain += 1
            else:
                encoded += 1

        print(f"Lua-like files: {len(infos)}")
        print(f"Plain source: {plain}")
        print(f"Lua bytecode: {bytecode}")
        print(f"Encoded/obfuscated: {encoded}")
        if ratios:
            print(f"Average printable ratio: {sum(ratios)/len(ratios):.3f}")
            print(f"Average entropy: {sum(entropies)/len(entropies):.3f} bits/byte")
        print(f"Unique 8-byte headers: {len(headers)}")
        print("\nMost common headers:")
        ranked = sorted(headers.items(), key=lambda kv: (-len(kv[1]), kv[0]))[: args.limit]
        for header, names in ranked:
            sample_name = names[0]
            print(f"{len(names):4d}  {header.hex():16s}  {header!r}  {sample_name}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
