#!/usr/bin/env python3
from __future__ import annotations

from search_nevergone_function_families import extract_query_text, rank_scores


def main() -> int:
    row = {
        "model_text": "ARCH=ARM32\no0 INT_ADD v1:4 const:small:0x10:4 -> v2:4\n",
    }
    portable = extract_query_text(row)
    assert portable.startswith("OP INT_ADD")
    assert "0x10" not in portable

    families = [
        {"family_id": "family-b", "family_size": 1, "labels": {}, "examples": []},
        {"family_id": "family-a", "family_size": 2, "labels": {}, "examples": []},
        {"family_id": "family-c", "family_size": 3, "labels": {}, "examples": []},
    ]
    ranked = rank_scores([0.7, 0.7, 0.2], families, top_k=2)
    assert [row["family_id"] for row in ranked] == ["family-a", "family-b"]
    assert ranked[0]["rank"] == 1
    assert ranked[1]["rank"] == 2
    assert ranked[0]["similarity"] == 0.7
    print("nevergone similarity search smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
