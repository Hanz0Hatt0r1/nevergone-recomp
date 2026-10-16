#!/usr/bin/env python3
"""Compatibility CLI for the shared revenginstr function-family miner."""
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any

from nevergone_pcode_features import (
    NEVERGONE_FAMILY_HASH_PROFILE,
    family_feature_set,
    jaccard,
    lsh_band_keys,
    minhash_signature,
    portable_pcode,
)
from revenginstr.family.mine import mine_dataset as _mine_dataset
from revenginstr.family.mine import read_dataset

FORMAT = "NEVERGONE.FunctionFamilies/1"


def mine_dataset(
    dataset_dir: Path,
    output_path: Path,
    *,
    similarity_threshold: float = 0.72,
    min_length_ratio: float = 0.55,
    bands: int = 8,
) -> dict[str, Any]:
    """Preserve the Nevergone API and output contract over the shared miner."""
    return _mine_dataset(
        dataset_dir,
        output_path,
        similarity_threshold=similarity_threshold,
        min_length_ratio=min_length_ratio,
        bands=bands,
        hash_profile=NEVERGONE_FAMILY_HASH_PROFILE,
        output_format=FORMAT,
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--threshold", type=float, default=0.72)
    parser.add_argument("--min-length-ratio", type=float, default=0.55)
    parser.add_argument("--bands", type=int, default=8)
    args = parser.parse_args(argv)

    report = mine_dataset(
        args.dataset_dir,
        args.output,
        similarity_threshold=args.threshold,
        min_length_ratio=args.min_length_ratio,
        bands=args.bands,
    )
    print(f"format: {report['format']}")
    print(f"functions: {report['functions']}")
    print(f"families: {report['families']}")
    print(f"multi-member families: {report['multi_member_families']}")
    print(f"largest family: {report['largest_family']}")
    print(f"candidate pairs: {report['lsh_candidate_pairs']}")
    print(f"accepted similarity pairs: {report['accepted_similarity_pairs']}")
    print(f"mixed-label families: {report['mixed_label_families']}")
    print(f"output: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
