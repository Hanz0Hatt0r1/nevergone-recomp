#!/usr/bin/env python3
"""Nevergone compatibility adapter for portable P-code family features."""
from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Iterable

_repo = Path(
    os.environ.get(
        "REVENGINSTR_REPO",
        str(Path(__file__).resolve().parents[2].parent / "revenginstr"),
    )
)
_source = _repo / "src"
if _source.is_dir() and str(_source) not in sys.path:
    sys.path.insert(0, str(_source))

try:
    from revenginstr.family.features import (
        HashProfile,
        family_feature_set,
        jaccard,
        lsh_band_keys,
        minhash_signature as _minhash_signature,
        portable_pcode,
    )
except ModuleNotFoundError as error:
    if error.name and not error.name.startswith("revenginstr"):
        raise
    raise ModuleNotFoundError(
        "revenginstr is required; install it or set REVENGINSTR_REPO to its checkout"
    ) from error


# The historical Nevergone seed remains consumer-owned so existing family IDs
# stay stable while the implementation moves to revenginstr.
NEVERGONE_FAMILY_HASH_PROFILE = HashProfile(
    "nevergone-v1", b"NVG-FAM", "NevergoneFamilyMinHash/"
)


def minhash_signature(features: Iterable[str]) -> tuple[int, ...]:
    """Keep the old one-argument API and its historical Nevergone hashes."""
    return _minhash_signature(features, NEVERGONE_FAMILY_HASH_PROFILE)
