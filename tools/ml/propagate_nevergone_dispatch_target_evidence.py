#!/usr/bin/env python3
"""Propagate verified CALLIND target observations as structural candidates only.

The input analysis must contain dispatch family/equivalence evidence. Seed rows are
explicit verified observations for one call site. A seed never turns another call
site into a resolved target automatically. Instead, this tool emits candidate
hypotheses with an explicit propagation basis:

* exact_ordered_position: same position in the same exact ordered dispatch group.
* same_slot_group: same normalized shape@offset structural slot.

Conflicting verified seeds are preserved as candidate sets and marked as conflicts.
Semantic labels and routing fields are not consumed; ``final_action`` is unchanged.
"""
from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.DispatchTargetCandidateEvidence/1"
CANDIDATE_FORMAT = "NEVERGONE.DispatchTargetCandidate/1"
SEED_FORMAT = "NEVERGONE.DispatchTargetSeed/1"


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


def ordered_calls(row: dict[str, Any]) -> list[dict[str, Any]]:
    family = row.get("dispatch_family_evidence")
    if not isinstance(family, dict) or not family.get("present"):
        return []
    memberships = family.get("memberships")
    if not isinstance(memberships, list):
        return []
    valid = [
        item for item in memberships
        if isinstance(item, dict)
        and isinstance(item.get("slot_group_id"), str)
        and item.get("call_index") is not None
    ]
    return sorted(
        valid,
        key=lambda item: (
            int(item["call_index"]),
            str(item.get("call_address") or ""),
            item["slot_group_id"],
        ),
    )


def call_key(record_id: str, call_index: int) -> str:
    return f"{record_id}#{call_index}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("seeds", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--candidates-output", type=Path)
    parser.add_argument("--seed-template-output", type=Path)
    args = parser.parse_args()

    rows = read_jsonl(args.analysis)
    if not rows:
        raise SystemExit("analysis input is empty")
    seeds = read_jsonl(args.seeds)

    row_by_id: dict[str, dict[str, Any]] = {}
    calls_by_id: dict[str, list[dict[str, Any]]] = {}
    call_lookup: dict[str, dict[str, Any]] = {}
    slot_members: dict[str, list[dict[str, Any]]] = defaultdict(list)
    ordered_group_members: dict[str, list[str]] = defaultdict(list)

    for row in rows:
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("analysis row missing id")
        row_by_id[record_id] = row
        calls = ordered_calls(row)
        calls_by_id[record_id] = calls
        equivalence = row.get("dispatch_equivalence_evidence")
        if isinstance(equivalence, dict) and equivalence.get("present"):
            group_id = equivalence.get("ordered_sequence_group_id")
            if isinstance(group_id, str):
                ordered_group_members[group_id].append(record_id)
        for position, call in enumerate(calls):
            item = {
                "id": record_id,
                "call_index": int(call["call_index"]),
                "call_address": call.get("call_address"),
                "slot_group_id": call["slot_group_id"],
                "position": position,
            }
            call_lookup[call_key(record_id, item["call_index"])] = item
            slot_members[item["slot_group_id"]].append(item)

    template_rows: list[dict[str, Any]] = []
    for key in sorted(call_lookup):
        call = call_lookup[key]
        row = row_by_id[call["id"]]
        eq = row.get("dispatch_equivalence_evidence")
        group_id = eq.get("ordered_sequence_group_id") if isinstance(eq, dict) else None
        group_size = eq.get("ordered_sequence_group_size") if isinstance(eq, dict) else None
        template_rows.append(
            {
                "format": SEED_FORMAT,
                "source_id": call["id"],
                "call_index": call["call_index"],
                "call_address": call["call_address"],
                "slot_group_id": call["slot_group_id"],
                "ordered_position": call["position"],
                "ordered_group_id": group_id,
                "ordered_group_size": group_size,
                "target_id": "",
                "verified": False,
                "evidence_kind": "",
                "evidence_ref": "",
            }
        )
    if args.seed_template_output:
        write_jsonl(args.seed_template_output, template_rows)

    verified_seeds: list[dict[str, Any]] = []
    for index, seed in enumerate(seeds, 1):
        source_id = seed.get("source_id")
        call_index = seed.get("call_index")
        target_id = seed.get("target_id")
        verified = seed.get("verified")
        if verified is not True:
            continue
        if not isinstance(source_id, str) or not source_id:
            raise ValueError(f"seed {index}: missing source_id")
        if not isinstance(call_index, int):
            raise ValueError(f"seed {index}: call_index must be int")
        if not isinstance(target_id, str) or not target_id:
            raise ValueError(f"seed {index}: verified seed missing target_id")
        key = call_key(source_id, call_index)
        source_call = call_lookup.get(key)
        if source_call is None:
            raise ValueError(f"seed {index}: unknown call site {key}")
        supplied_slot = seed.get("slot_group_id")
        if supplied_slot not in (None, "", source_call["slot_group_id"]):
            raise ValueError(
                f"seed {index}: slot_group_id mismatch: {supplied_slot!r} != {source_call['slot_group_id']!r}"
            )
        verified_seeds.append(
            {
                "seed_index": index,
                "source_id": source_id,
                "call_index": call_index,
                "call_address": source_call["call_address"],
                "slot_group_id": source_call["slot_group_id"],
                "position": source_call["position"],
                "target_id": target_id,
                "evidence_kind": str(seed.get("evidence_kind") or "verified_observation"),
                "evidence_ref": str(seed.get("evidence_ref") or ""),
            }
        )

    proposals: dict[str, list[dict[str, Any]]] = defaultdict(list)

    def add_proposal(target_call: dict[str, Any], seed: dict[str, Any], basis: str) -> None:
        key = call_key(target_call["id"], target_call["call_index"])
        proposals[key].append(
            {
                "target_id": seed["target_id"],
                "basis": basis,
                "seed_source_id": seed["source_id"],
                "seed_call_index": seed["call_index"],
                "seed_call_address": seed["call_address"],
                "seed_evidence_kind": seed["evidence_kind"],
                "seed_evidence_ref": seed["evidence_ref"],
            }
        )

    for seed in verified_seeds:
        source_row = row_by_id[seed["source_id"]]
        eq = source_row.get("dispatch_equivalence_evidence")
        group_id = eq.get("ordered_sequence_group_id") if isinstance(eq, dict) else None
        group_size = eq.get("ordered_sequence_group_size") if isinstance(eq, dict) else 0
        if isinstance(group_id, str) and isinstance(group_size, int) and group_size > 1:
            for peer_id in sorted(ordered_group_members.get(group_id, [])):
                peer_calls = calls_by_id.get(peer_id, [])
                if seed["position"] >= len(peer_calls):
                    continue
                peer = peer_calls[seed["position"]]
                if peer["slot_group_id"] != seed["slot_group_id"]:
                    continue
                add_proposal(
                    {
                        "id": peer_id,
                        "call_index": int(peer["call_index"]),
                        "call_address": peer.get("call_address"),
                    },
                    seed,
                    "exact_ordered_position",
                )

        for peer in slot_members.get(seed["slot_group_id"], []):
            add_proposal(peer, seed, "same_slot_group")

    candidate_rows: list[dict[str, Any]] = []
    candidate_by_call: dict[str, dict[str, Any]] = {}
    for key in sorted(proposals):
        target_call = call_lookup[key]
        raw = proposals[key]
        dedup: dict[tuple[str, str, str, int], dict[str, Any]] = {}
        for item in raw:
            token = (
                item["target_id"],
                item["basis"],
                item["seed_source_id"],
                item["seed_call_index"],
            )
            dedup[token] = item
        evidence = sorted(
            dedup.values(),
            key=lambda item: (
                0 if item["basis"] == "exact_ordered_position" else 1,
                item["target_id"],
                item["seed_source_id"],
                item["seed_call_index"],
            ),
        )
        targets = sorted({item["target_id"] for item in evidence})
        bases = sorted({item["basis"] for item in evidence})
        candidate = {
            "format": CANDIDATE_FORMAT,
            "id": target_call["id"],
            "call_index": target_call["call_index"],
            "call_address": target_call["call_address"],
            "slot_group_id": target_call["slot_group_id"],
            "ordered_position": target_call["position"],
            "candidate_target_ids": targets,
            "candidate_target_count": len(targets),
            "conflict": len(targets) > 1,
            "propagation_bases": bases,
            "has_exact_ordered_position_evidence": "exact_ordered_position" in bases,
            "evidence": evidence,
            "status": "conflict" if len(targets) > 1 else "candidate_only",
        }
        candidate_rows.append(candidate)
        candidate_by_call[key] = candidate

    output_rows: list[dict[str, Any]] = []
    for source in rows:
        record_id = source["id"]
        before = source.get("final_action")
        calls = []
        for call in calls_by_id.get(record_id, []):
            key = call_key(record_id, int(call["call_index"]))
            if key in candidate_by_call:
                calls.append(candidate_by_call[key])
        enriched = dict(source)
        enriched["dispatch_target_candidate_evidence"] = {
            "format": FORMAT,
            "present": bool(calls),
            "candidate_call_count": len(calls),
            "conflict_call_count": sum(int(item["conflict"]) for item in calls),
            "calls": calls,
        }
        if enriched.get("final_action") != before:
            raise AssertionError(f"{record_id}: final_action changed")
        output_rows.append(enriched)

    write_jsonl(args.output, output_rows)
    candidates_output = args.candidates_output or args.output.with_suffix(".candidates.jsonl")
    write_jsonl(candidates_output, candidate_rows)

    exact_candidates = sum(int(item["has_exact_ordered_position_evidence"]) for item in candidate_rows)
    conflicts = sum(int(item["conflict"]) for item in candidate_rows)
    print("format:", FORMAT)
    print("functions:", len(output_rows))
    print("verified seeds:", len(verified_seeds))
    print("candidate call sites:", len(candidate_rows))
    print("exact-ordered candidate sites:", exact_candidates)
    print("conflicting candidate sites:", conflicts)
    print("final_action unchanged: yes")
    print("output:", args.output)
    print("candidates output:", candidates_output)
    if args.seed_template_output:
        print("seed template output:", args.seed_template_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
