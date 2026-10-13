#!/usr/bin/env python3
"""Shared portable-P-code normalization and structural family features."""
from __future__ import annotations

import hashlib
import re
from collections import Counter
from typing import Iterable

_OP_LINE = re.compile(r"^o\d+\s+")
_SSA = re.compile(r"\bv\d+:(\d+)")
_SMALL_CONST = re.compile(r"const:small:0x([0-9a-fA-F]+):(\d+)")
_WIDTHED_ROLE = re.compile(r"\b(v|reg|input|mem|tmp|def:omitted):(\d+)")
_CONST_WIDTH = re.compile(r"\b(const:(?:tiny|small|medium|large|xlarge|zero|one|allones)):(\d+)")

_MINHASH_SIZE = 32
_MINHASH_PRIME = (1 << 61) - 1


def _bucket_small_constant(match: re.Match[str]) -> str:
    value = int(match.group(1), 16)
    size = match.group(2)
    if value <= 0x0F:
        bucket = "tiny"
    elif value <= 0x3F:
        bucket = "small"
    elif value <= 0xFF:
        bucket = "medium"
    elif value <= 0x3FF:
        bucket = "large"
    else:
        bucket = "xlarge"
    return f"const:{bucket}:{size}"


def portable_pcode(text: str) -> str:
    """Return the experimentally validated portable model view.

    Headers are intentionally excluded. Operation ordinals and exact SSA IDs are
    removed; exact small constants are replaced by magnitude buckets while widths,
    operand roles, call kinds and operation order remain visible.
    """
    out: list[str] = []
    for raw in text.splitlines():
        if not _OP_LINE.match(raw):
            continue
        line = _OP_LINE.sub("OP ", raw, count=1)
        line = _SSA.sub(r"v:\1", line)
        line = _SMALL_CONST.sub(_bucket_small_constant, line)
        out.append(line)
    return "\n".join(out)


def _shape_line(line: str) -> str:
    line = _WIDTHED_ROLE.sub(lambda m: m.group(1), line)
    line = _CONST_WIDTH.sub(lambda m: m.group(1), line)
    return line


def _count_bucket(value: int) -> str:
    if value <= 0:
        return "0"
    if value == 1:
        return "1"
    lower = 2
    upper = 3
    while upper < value and upper < 255:
        lower = upper + 1
        upper = (upper << 1) + 1
    return f"{lower}-{upper}" if value <= upper else "256+"


def family_feature_set(text: str) -> frozenset[str]:
    """Build a name/address-free feature set for near-duplicate family mining."""
    portable = portable_pcode(text)
    lines = [line for line in portable.splitlines() if line]
    if not lines:
        return frozenset({"empty"})

    signatures: list[str] = []
    features: set[str] = set()
    opcode_counts: Counter[str] = Counter()

    for line in lines:
        parts = line.split()
        opcode = parts[1] if len(parts) >= 2 else "UNKNOWN"
        call_kind = next((part for part in parts[2:] if part.startswith("call:")), "")
        signature = opcode if not call_kind else f"{opcode}:{call_kind}"
        signatures.append(signature)
        opcode_counts[signature] += 1
        features.add(f"shape:{_shape_line(line)}")
        features.add(f"op:{signature}")

    for width in (2, 3, 4):
        if len(signatures) < width:
            continue
        for index in range(len(signatures) - width + 1):
            features.add(f"seq{width}:" + "|".join(signatures[index:index + width]))

    for opcode, count in opcode_counts.items():
        features.add(f"count:{opcode}:{_count_bucket(count)}")
    features.add(f"ops:{_count_bucket(len(lines))}")
    return frozenset(features)


def jaccard(left: frozenset[str], right: frozenset[str]) -> float:
    if not left and not right:
        return 1.0
    union = len(left | right)
    return (len(left & right) / union) if union else 1.0


def _hash64(value: str) -> int:
    return int.from_bytes(
        hashlib.blake2b(value.encode("utf-8"), digest_size=8, person=b"NVG-FAM").digest(),
        "big",
    ) & _MINHASH_PRIME


def _permutation(index: int) -> tuple[int, int]:
    digest = hashlib.sha256(f"NevergoneFamilyMinHash/{index}".encode("ascii")).digest()
    a = (int.from_bytes(digest[:8], "big") % (_MINHASH_PRIME - 1)) + 1
    b = int.from_bytes(digest[8:16], "big") % _MINHASH_PRIME
    return a, b


_PERMUTATIONS = tuple(_permutation(index) for index in range(_MINHASH_SIZE))


def minhash_signature(features: Iterable[str]) -> tuple[int, ...]:
    hashes = [_hash64(token) for token in features]
    if not hashes:
        hashes = [0]
    signature: list[int] = []
    for a, b in _PERMUTATIONS:
        signature.append(min(((a * value + b) % _MINHASH_PRIME) for value in hashes))
    return tuple(signature)


def lsh_band_keys(signature: tuple[int, ...], bands: int = 8) -> tuple[tuple[int, ...], ...]:
    if bands <= 0 or len(signature) % bands:
        raise ValueError("signature length must divide evenly by bands")
    rows = len(signature) // bands
    return tuple(signature[index:index + rows] for index in range(0, len(signature), rows))
