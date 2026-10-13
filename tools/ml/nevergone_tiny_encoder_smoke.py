#!/usr/bin/env python3
from __future__ import annotations

from nevergone_tiny_encoder_features import hashed_feature_counts, parameter_count


BASE = (
    "ARCH=ARM32\n"
    "ENDIAN=LE\n"
    "PTR_SIZE=4\n"
    "CALLCONV=__thiscall\n"
    "PARAMS=1\n"
    "BODY_ADDRS_BUCKET=16-31\n"
    "OPS_BUCKET=2-3\n"
    "CALLS_BUCKET=0\n"
    "INDIRECT_CALLS_BUCKET=0\n"
    "BRANCHES_BUCKET=0\n"
    "LOADS_BUCKET=1\n"
    "STORES_BUCKET=0\n"
    "TRUNCATED=0\n"
    "o0 PTRADD out=v0:4 in=reg:4 in=const:small:0x104:4 in=const:one:4\n"
    "o1 LOAD out=v1:4 addr=v0:4\n"
)

VARIANT = (
    "ARCH=ARM64\n"
    "ENDIAN=BE\n"
    "PTR_SIZE=8\n"
    "CALLCONV=unknown\n"
    "PARAMS=9\n"
    "BODY_ADDRS_BUCKET=256-511\n"
    "OPS_BUCKET=256-511\n"
    "CALLS_BUCKET=16-31\n"
    "INDIRECT_CALLS_BUCKET=8-15\n"
    "BRANCHES_BUCKET=32-63\n"
    "LOADS_BUCKET=64-127\n"
    "STORES_BUCKET=64-127\n"
    "TRUNCATED=1\n"
    "o913 PTRADD out=v77:4 in=reg:4 in=const:small:0x108:4 in=const:one:4\n"
    "o1024 LOAD out=v99:4 addr=v77:4\n"
)


def main() -> int:
    left = hashed_feature_counts(BASE, buckets=1024)
    right = hashed_feature_counts(VARIANT, buckets=1024)
    assert left == right, (left, right)
    assert left

    params = parameter_count(
        buckets=16384,
        embedding_dim=96,
        hidden_dim=64,
        classes=4,
    )
    assert params == 1_579_332, params

    try:
        hashed_feature_counts(BASE, buckets=0)
        raise AssertionError("expected invalid bucket count to fail")
    except ValueError:
        pass

    print("Nevergone tiny encoder feature smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
