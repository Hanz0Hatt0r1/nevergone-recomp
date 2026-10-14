#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

FORMAT = "NEVERGONE.PhiDefaultInstanceRecovery/1"


def main() -> int:
    parser = argparse.ArgumentParser(description="Summarize fail-closed PHI recovery blockers")
    parser.add_argument("details_jsonl", type=Path)
    args = parser.parse_args()

    rows = []
    with args.details_jsonl.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if row.get("format") != FORMAT:
                raise ValueError(f"{args.details_jsonl}:{line_no}: expected {FORMAT}")
            rows.append(row)

    reasons = Counter()
    accepted = 0
    for row in rows:
        if row.get("accepted") is True:
            accepted += 1
        for reason in row.get("reasons") or []:
            reasons[str(reason)] += 1

    print("format: NEVERGONE.PhiDefaultInstanceRecoveryBlockerSummary/1")
    print("detail rows:", len(rows))
    print("accepted:", accepted)
    print("blocked:", len(rows) - accepted)
    print("reason counts:", dict(reasons.most_common()))
    print("\nrows:")
    for row in rows:
        print(
            row.get("source_id"),
            "call", row.get("call_index"),
            "accepted=", row.get("accepted"),
            "primary=", row.get("primary_branch_classes"),
            "got=", row.get("secondary_got_address"),
            "default=", row.get("default_instance_address"),
            "owners=", row.get("owner_classes"),
            "owner_xrefs=", row.get("owner_direct_method_xrefs"),
            "reasons=", row.get("reasons"),
        )
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
