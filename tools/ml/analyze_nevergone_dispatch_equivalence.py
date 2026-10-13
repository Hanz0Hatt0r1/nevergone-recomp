#!/usr/bin/env python3
"""Build deterministic cross-function dispatch equivalence evidence.

This tool operates only on structural slot-group memberships produced by
``analyze_nevergone_dispatch_families.py``. It compares functions by three exact
representations: ordered slot sequence, slot multiset, and slot set. It also emits
pairwise set-overlap metrics. No semantic labels are consumed and ``final_action``
is never modified.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.DispatchEquivalenceEvidence/1"
GROUP_FORMAT = "NEVERGONE.DispatchEquivalenceGroup/1"
PAIR_FORMAT = "NEVERGONE.DispatchEquivalencePair/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            rows.append(row)
    return rows


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def stable_hash(parts: list[str]) -> str:
    payload = "\n".join(parts).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()[:16]


def ordered_memberships(row: dict[str, Any]) -> list[dict[str, Any]]:
    evidence = row.get("dispatch_family_evidence")
    if not isinstance(evidence, dict) or not evidence.get("present"):
        return []
    memberships = evidence.get("memberships")
    if not isinstance(memberships, list):
        return []
    valid = [item for item in memberships if isinstance(item, dict) and isinstance(item.get("slot_group_id"), str)]
    return sorted(
        valid,
        key=lambda item: (
            int(item.get("call_index") if item.get("call_index") is not None else 1 << 30),
            str(item.get("call_address") or ""),
            item["slot_group_id"],
        ),
    )


def multiset_tokens(sequence: list[str]) -> list[str]:
    counts = Counter(sequence)
    return [f"{key}*{counts[key]}" for key in sorted(counts)]


def set_tokens(sequence: list[str]) -> list[str]:
    return sorted(set(sequence))


def group_catalog(kind: str, signatures: dict[str, list[str]], profiles: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    for signature_id in sorted(signatures):
        functions = sorted(signatures[signature_id])
        if len(functions) < 2:
            continue
        representative = profiles[functions[0]]
        if kind == "ordered_sequence":
            representation = representative["ordered_slot_sequence"]
        elif kind == "multiset":
            representation = representative["multiset_tokens"]
        else:
            representation = representative["slot_set"]
        out.append(
            {
                "format": GROUP_FORMAT,
                "kind": kind,
                "group_id": f"{kind}:{signature_id}",
                "signature_id": signature_id,
                "function_count": len(functions),
                "functions": functions,
                "representation": representation,
            }
        )
    return sorted(out, key=lambda item: (-item["function_count"], item["kind"], item["signature_id"]))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--groups-output", type=Path)
    parser.add_argument("--pairs-output", type=Path)
    args = parser.parse_args()

    rows = read_jsonl(args.analysis)
    if not rows:
        raise SystemExit("analysis input is empty")

    profiles: dict[str, dict[str, Any]] = {}
    signature_members: dict[str, dict[str, list[str]]] = {
        "ordered_sequence": defaultdict(list),
        "multiset": defaultdict(list),
        "slot_set": defaultdict(list),
    }

    for row in rows:
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("analysis row missing id")
        memberships = ordered_memberships(row)
        if not memberships:
            continue
        sequence = [item["slot_group_id"] for item in memberships]
        multiset = multiset_tokens(sequence)
        slot_set = set_tokens(sequence)
        ordered_hash = stable_hash(sequence)
        multiset_hash = stable_hash(multiset)
        set_hash = stable_hash(slot_set)
        profile = {
            "id": record_id,
            "call_count": len(sequence),
            "ordered_slot_sequence": sequence,
            "ordered_sequence_hash": ordered_hash,
            "multiset_tokens": multiset,
            "multiset_hash": multiset_hash,
            "slot_set": slot_set,
            "slot_set_hash": set_hash,
        }
        profiles[record_id] = profile
        signature_members["ordered_sequence"][ordered_hash].append(record_id)
        signature_members["multiset"][multiset_hash].append(record_id)
        signature_members["slot_set"][set_hash].append(record_id)

    groups: list[dict[str, Any]] = []
    for kind in ("ordered_sequence", "multiset", "slot_set"):
        groups.extend(group_catalog(kind, signature_members[kind], profiles))

    repeated_group_sizes: dict[str, dict[str, int]] = {kind: {} for kind in signature_members}
    for kind, mapping in signature_members.items():
        for signature_id, members in mapping.items():
            repeated_group_sizes[kind][signature_id] = len(members)

    pair_rows: list[dict[str, Any]] = []
    profile_ids = sorted(profiles)
    for left_index, left_id in enumerate(profile_ids):
        left = profiles[left_id]
        left_set = set(left["slot_set"])
        for right_id in profile_ids[left_index + 1 :]:
            right = profiles[right_id]
            right_set = set(right["slot_set"])
            shared = left_set & right_set
            if not shared:
                continue
            union = left_set | right_set
            pair_rows.append(
                {
                    "format": PAIR_FORMAT,
                    "left_id": left_id,
                    "right_id": right_id,
                    "left_call_count": left["call_count"],
                    "right_call_count": right["call_count"],
                    "shared_slot_group_count": len(shared),
                    "union_slot_group_count": len(union),
                    "slot_set_jaccard": round(len(shared) / len(union), 6) if union else 0.0,
                    "left_containment": round(len(shared) / len(left_set), 6) if left_set else 0.0,
                    "right_containment": round(len(shared) / len(right_set), 6) if right_set else 0.0,
                    "ordered_sequence_equal": left["ordered_sequence_hash"] == right["ordered_sequence_hash"],
                    "multiset_equal": left["multiset_hash"] == right["multiset_hash"],
                    "slot_set_equal": left["slot_set_hash"] == right["slot_set_hash"],
                    "shared_slot_groups": sorted(shared),
                }
            )
    pair_rows.sort(
        key=lambda item: (
            -int(item["ordered_sequence_equal"]),
            -int(item["multiset_equal"]),
            -int(item["slot_set_equal"]),
            -float(item["slot_set_jaccard"]),
            -int(item["shared_slot_group_count"]),
            item["left_id"],
            item["right_id"],
        )
    )

    output_rows: list[dict[str, Any]] = []
    for source in rows:
        record_id = source["id"]
        before = source.get("final_action")
        enriched = dict(source)
        profile = profiles.get(record_id)
        if profile is None:
            evidence = {"format": FORMAT, "present": False}
        else:
            ordered_size = repeated_group_sizes["ordered_sequence"][profile["ordered_sequence_hash"]]
            multiset_size = repeated_group_sizes["multiset"][profile["multiset_hash"]]
            set_size = repeated_group_sizes["slot_set"][profile["slot_set_hash"]]
            evidence = {
                "format": FORMAT,
                "present": True,
                **profile,
                "ordered_sequence_group_id": f"ordered_sequence:{profile['ordered_sequence_hash']}",
                "ordered_sequence_group_size": ordered_size,
                "multiset_group_id": f"multiset:{profile['multiset_hash']}",
                "multiset_group_size": multiset_size,
                "slot_set_group_id": f"slot_set:{profile['slot_set_hash']}",
                "slot_set_group_size": set_size,
            }
        enriched["dispatch_equivalence_evidence"] = evidence
        if enriched.get("final_action") != before:
            raise AssertionError(f"{record_id}: final_action changed")
        output_rows.append(enriched)

    write_jsonl(args.output, output_rows)
    groups_output = args.groups_output or args.output.with_suffix(".groups.jsonl")
    pairs_output = args.pairs_output or args.output.with_suffix(".pairs.jsonl")
    write_jsonl(groups_output, groups)
    write_jsonl(pairs_output, pair_rows)

    ordered_groups = sum(1 for item in groups if item["kind"] == "ordered_sequence")
    multiset_groups = sum(1 for item in groups if item["kind"] == "multiset")
    set_groups = sum(1 for item in groups if item["kind"] == "slot_set")
    exact_pairs = sum(int(item["ordered_sequence_equal"]) for item in pair_rows)
    print("format:", FORMAT)
    print("functions:", len(output_rows))
    print("functions with dispatch evidence:", len(profiles))
    print("repeated ordered groups:", ordered_groups)
    print("repeated multiset groups:", multiset_groups)
    print("repeated slot-set groups:", set_groups)
    print("overlap pairs:", len(pair_rows))
    print("exact ordered pairs:", exact_pairs)
    print("final_action unchanged: yes")
    print("output:", args.output)
    print("groups output:", groups_output)
    print("pairs output:", pairs_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
