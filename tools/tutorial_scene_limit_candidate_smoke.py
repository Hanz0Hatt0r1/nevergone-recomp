#!/usr/bin/env python3

from __future__ import annotations

from tutorial_scene_limit_candidate import SCHEMA, build_manifest, render_manifest
from tutorial_scene_limit_evidence import find_candidates
from tutorial_scene_limit_evidence_smoke import fixture


def main() -> int:
    candidates = find_candidates(fixture())
    manifest = build_manifest(candidates)

    assert manifest["schema"] == SCHEMA
    assert manifest["semantic_status"] == "structural-candidate"
    assert manifest["review_status"] == "candidate"
    assert manifest["pointer_prefix_offsets"] == [0x238, 0x20]
    assert manifest["right_edge_x_field_offset"] == 0x0C
    assert manifest["vertical_field_offsets"] == [0x10, 0x14]
    assert manifest["vertical_order_status"] == "unresolved"
    assert len(manifest["candidates"]) == 3

    rendered = render_manifest(candidates)
    assert '"schema": "NEVERGONE.TutorialSceneLimitCandidate/1"' in rendered
    assert '"vertical_order_status": "unresolved"' in rendered

    try:
        build_manifest(candidates[:-1])
    except ValueError as error:
        assert "not uniquely reviewable" in str(error)
    else:
        raise AssertionError("incomplete candidate must fail closed")

    print("Tutorial scene-limit candidate smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
