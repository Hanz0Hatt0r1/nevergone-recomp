#!/usr/bin/env python3
"""Smoke tests for the post-inference Ghidra audit join."""
from __future__ import annotations

from audit_nevergone_classifications import (
    assign_address_clusters,
    namespace_hint,
    normalize_entry,
    parse_record_id,
)


def main() -> int:
    assert normalize_entry("0x288294") == "00288294"
    assert normalize_entry("00288294") == "00288294"
    assert parse_record_id("libcocos2dcpp.so:00288294") == (
        "libcocos2dcpp",
        "00288294",
    )
    assert namespace_hint("cocos2d::CCNode::visit()") == "cocos2d"
    assert namespace_hint("avcodec_open2") == "avcodec"

    rows = [
        {"program": "libcocos2dcpp", "entry": "00001000"},
        {"program": "libcocos2dcpp", "entry": "00001040"},
        {"program": "libcocos2dcpp", "entry": "00001200"},
        {"program": "libffmpeg", "entry": "00002000"},
    ]
    assign_address_clusters(rows, 0x100)
    assert rows[0]["cluster_id"] == rows[1]["cluster_id"]
    assert rows[1]["cluster_id"] != rows[2]["cluster_id"]
    assert rows[2]["cluster_id"] != rows[3]["cluster_id"]

    print("classification audit smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
