#!/usr/bin/env python3
"""Deterministic hashed unigram/bigram features for the Nevergone tiny encoder."""
from __future__ import annotations

import hashlib
from collections import Counter

from nevergone_pcode_features import portable_pcode

PERSON = b"NVG-TINY"


def _hash_bucket(token: str, buckets: int) -> int:
    if buckets <= 0:
        raise ValueError("buckets must be positive")
    digest = hashlib.blake2b(
        token.encode("utf-8"),
        digest_size=8,
        person=PERSON,
    ).digest()
    return int.from_bytes(digest, "big") % buckets


def hashed_feature_counts(
    text: str,
    *,
    buckets: int = 16384,
    max_features: int = 4096,
) -> tuple[tuple[int, int], ...]:
    """Return deterministic hashed counts from portable P-code only.

    Each operation contributes token unigrams and adjacent token bigrams. Bigrams
    never cross operation-line boundaries. Exact SSA identities, operation
    ordinals, headers and exact small offsets are already removed by
    ``portable_pcode``.
    """
    if max_features <= 0:
        raise ValueError("max_features must be positive")

    counts: Counter[int] = Counter()
    emitted = 0
    for line in portable_pcode(text).splitlines():
        parts = line.split()
        if len(parts) <= 1:
            continue
        tokens = parts[1:]
        previous: str | None = None
        for token in tokens:
            counts[_hash_bucket("u:" + token, buckets)] += 1
            emitted += 1
            if emitted >= max_features:
                break
            if previous is not None:
                counts[_hash_bucket("b:" + previous + "|" + token, buckets)] += 1
                emitted += 1
                if emitted >= max_features:
                    break
            previous = token
        if emitted >= max_features:
            break

    if not counts:
        counts[_hash_bucket("u:<EMPTY>", buckets)] = 1
    return tuple(sorted(counts.items()))


def parameter_count(
    *,
    buckets: int,
    embedding_dim: int,
    hidden_dim: int,
    classes: int,
) -> int:
    """Number of trainable scalars in the averaged-embedding + tanh MLP model."""
    if min(buckets, embedding_dim, hidden_dim, classes) <= 0:
        raise ValueError("model dimensions must be positive")
    return (
        buckets * embedding_dim
        + embedding_dim * hidden_dim
        + hidden_dim
        + hidden_dim * classes
        + classes
    )
