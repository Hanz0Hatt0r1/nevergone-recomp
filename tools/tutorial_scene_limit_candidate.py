#!/usr/bin/env python3
"""Emit a fail-closed machine-readable tutorial scene-limit mapping candidate.

This adapter consumes the same exported high-P-code rows as
`tutorial_scene_limit_evidence.py`.  It only serializes a candidate when that
analyzer has exactly one recovered right-edge field and two distinct recovered
vertical fields.  The two vertical offsets deliberately remain unordered until
a separate control-flow review proves their A/B branch order.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Iterable

from tutorial_scene_limit_evidence import Candidate, find_candidates, read_rows, structural_status

SCHEMA = "NEVERGONE.TutorialSceneLimitCandidate/1"


def build_manifest(candidates: Iterable[Candidate]) -> dict[str, object]:
    values = list(candidates)
    status = structural_status(values)
    if status != "three-limit-candidate":
        raise ValueError(
            "scene-limit evidence is not uniquely reviewable: "
            f"structural status is {status}"
        )

    right = sorted(
        {item.field_offset for item in values if item.classification == "right-edge-x-candidate"}
    )
    vertical = sorted(
        {item.field_offset for item in values if item.classification == "vertical-edge-candidate"}
    )
    if len(right) != 1 or len(vertical) != 2 or right[0] in vertical:
        raise ValueError("scene-limit candidate fields are incomplete or aliased")

    rows = []
    for item in sorted(values, key=lambda value: (value.function_entry, value.field_offset)):
        rows.append(
            {
                "function_entry": item.function_entry,
                "field_offset": item.field_offset,
                "classification": item.classification,
                "threshold_constants": list(item.constants),
                "load_sites": list(item.sites),
            }
        )

    return {
        "schema": SCHEMA,
        "semantic_status": "structural-candidate",
        "review_status": "candidate",
        "pointer_prefix_offsets": [0x238, 0x20],
        "right_edge_x_field_offset": right[0],
        "vertical_field_offsets": vertical,
        "vertical_order_status": "unresolved",
        "candidates": rows,
    }


def render_manifest(candidates: Iterable[Candidate]) -> str:
    return json.dumps(build_manifest(candidates), indent=2, sort_keys=True) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tsv", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    rows = []
    try:
        for path in args.tsv:
            rows.extend(read_rows(path))
        text = render_manifest(find_candidates(rows))
    except (OSError, ValueError) as error:
        parser.error(str(error))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="utf-8")
    print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
