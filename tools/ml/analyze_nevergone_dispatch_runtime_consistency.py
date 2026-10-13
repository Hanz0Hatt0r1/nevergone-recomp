#!/usr/bin/env python3
"""Compare verified runtime CALLIND targets across exact ordered dispatch groups.

This is evidence-only. It never promotes a structural candidate into a resolved
callee and never changes ``final_action``. For every position in a repeated exact
ordered dispatch group it reports how many peer call sites have direct verified
runtime observations and whether those observations agree.

Statuses:
* unobserved: no peer call site has a verified runtime target.
* single_observation: exactly one peer call site has verified runtime evidence.
* consistent: at least two peer call sites were observed and all observed sites
  resolve to exactly one identical target.
* conflict: an observed site has multiple verified targets or observed peers
  disagree on target identity.
"""
from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.DispatchRuntimeConsistencyEvidence/1"
POSITION_FORMAT = "NEVERGONE.DispatchRuntimeConsistencyPosition/1"
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
        and isinstance(item.get("call_index"), int)
    ]
    return sorted(
        valid,
        key=lambda item: (
            int(item["call_index"]),
            str(item.get("call_address") or ""),
            item["slot_group_id"],
        ),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("seeds", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--positions-output", type=Path)
    args = parser.parse_args()

    analysis = read_jsonl(args.analysis)
    seeds = read_jsonl(args.seeds)
    if not analysis:
        raise SystemExit("analysis input is empty")

    rows_by_id: dict[str, dict[str, Any]] = {}
    group_members: dict[str, list[str]] = defaultdict(list)
    calls_by_id: dict[str, list[dict[str, Any]]] = {}

    for row in analysis:
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("analysis row missing id")
        rows_by_id[record_id] = row
        calls_by_id[record_id] = ordered_calls(row)
        eq = row.get("dispatch_equivalence_evidence")
        if not isinstance(eq, dict) or not eq.get("present"):
            continue
        group_id = eq.get("ordered_sequence_group_id")
        group_size = eq.get("ordered_sequence_group_size")
        if isinstance(group_id, str) and isinstance(group_size, int) and group_size > 1:
            group_members[group_id].append(record_id)

    site_targets: dict[tuple[str, int], set[str]] = defaultdict(set)
    site_seed_rows: dict[tuple[str, int], list[dict[str, Any]]] = defaultdict(list)
    for index, seed in enumerate(seeds, 1):
        if seed.get("verified") is not True:
            continue
        source_id = seed.get("source_id")
        call_index = seed.get("call_index")
        target_id = seed.get("target_id")
        if not isinstance(source_id, str) or source_id not in rows_by_id:
            raise ValueError(f"seed {index}: unknown source_id {source_id!r}")
        if not isinstance(call_index, int):
            raise ValueError(f"seed {index}: call_index must be int")
        if not isinstance(target_id, str) or not target_id:
            raise ValueError(f"seed {index}: verified seed missing target_id")
        key = (source_id, call_index)
        site_targets[key].add(target_id)
        site_seed_rows[key].append(seed)

    position_rows: list[dict[str, Any]] = []
    group_summaries: dict[str, list[dict[str, Any]]] = defaultdict(list)

    for group_id in sorted(group_members):
        members = sorted(group_members[group_id])
        if len(members) < 2:
            continue
        call_lengths = {len(calls_by_id.get(member, [])) for member in members}
        if len(call_lengths) != 1:
            raise ValueError(f"{group_id}: exact ordered group has inconsistent call counts {sorted(call_lengths)}")
        call_count = next(iter(call_lengths), 0)

        for position in range(call_count):
            slot_ids = {calls_by_id[member][position]["slot_group_id"] for member in members}
            if len(slot_ids) != 1:
                raise ValueError(f"{group_id} position {position}: slot mismatch {sorted(slot_ids)}")
            slot_group_id = next(iter(slot_ids))

            observations: list[dict[str, Any]] = []
            all_targets: set[str] = set()
            multi_target_site = False
            for member in members:
                call = calls_by_id[member][position]
                key = (member, int(call["call_index"]))
                targets = sorted(site_targets.get(key, set()))
                if len(targets) > 1:
                    multi_target_site = True
                all_targets.update(targets)
                observations.append(
                    {
                        "source_id": member,
                        "call_index": int(call["call_index"]),
                        "call_address": call.get("call_address"),
                        "observed": bool(targets),
                        "target_ids": targets,
                        "verified_seed_count": len(site_seed_rows.get(key, [])),
                    }
                )

            observed_sites = sum(int(item["observed"]) for item in observations)
            if observed_sites == 0:
                status = "unobserved"
            elif multi_target_site or len(all_targets) > 1:
                status = "conflict"
            elif observed_sites == 1:
                status = "single_observation"
            else:
                status = "consistent"

            row = {
                "format": POSITION_FORMAT,
                "ordered_group_id": group_id,
                "ordered_group_size": len(members),
                "ordered_position": position,
                "slot_group_id": slot_group_id,
                "peer_call_site_count": len(members),
                "observed_call_site_count": observed_sites,
                "coverage_fraction": round(observed_sites / len(members), 6),
                "target_ids": sorted(all_targets),
                "target_count": len(all_targets),
                "status": status,
                "observations": observations,
            }
            position_rows.append(row)
            group_summaries[group_id].append(row)

    output_rows: list[dict[str, Any]] = []
    for source in analysis:
        record_id = source["id"]
        before = source.get("final_action")
        eq = source.get("dispatch_equivalence_evidence")
        group_id = eq.get("ordered_sequence_group_id") if isinstance(eq, dict) else None
        positions = group_summaries.get(group_id, []) if isinstance(group_id, str) else []
        enriched = dict(source)
        enriched["dispatch_runtime_consistency_evidence"] = {
            "format": FORMAT,
            "present": bool(positions),
            "ordered_group_id": group_id if positions else None,
            "position_count": len(positions),
            "consistent_position_count": sum(item["status"] == "consistent" for item in positions),
            "conflict_position_count": sum(item["status"] == "conflict" for item in positions),
            "single_observation_position_count": sum(item["status"] == "single_observation" for item in positions),
            "unobserved_position_count": sum(item["status"] == "unobserved" for item in positions),
        }
        if enriched.get("final_action") != before:
            raise AssertionError(f"{record_id}: final_action changed")
        output_rows.append(enriched)

    write_jsonl(args.output, output_rows)
    positions_output = args.positions_output or args.output.with_suffix(".positions.jsonl")
    write_jsonl(positions_output, position_rows)

    counts: dict[str, int] = defaultdict(int)
    for row in position_rows:
        counts[row["status"]] += 1
    print("format:", FORMAT)
    print("functions:", len(output_rows))
    print("exact ordered groups:", len(group_summaries))
    print("positions:", len(position_rows))
    for status in ("unobserved", "single_observation", "consistent", "conflict"):
        print(f"{status} positions:", counts.get(status, 0))
    print("final_action unchanged: yes")
    print("output:", args.output)
    print("positions output:", positions_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
