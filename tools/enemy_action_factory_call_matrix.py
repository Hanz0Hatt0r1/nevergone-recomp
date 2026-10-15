#!/usr/bin/env python3
"""Join EnemyActions createWithFile caller subtrees into per-call evidence rows."""

from __future__ import annotations

import argparse
import csv
import re
from collections import defaultdict
from pathlib import Path

SUBTREE_FORMAT = "NEVERGONE.GhidraCallerArgumentSubtree/1"
IDENTITY_FORMAT = "NEVERGONE.GhidraFunctionIdentity/1"
PARAM_RE = re.compile(r"^enemy-actions-create-with-file:param\[(\d+)\]$")


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None:
            raise ValueError(f"{path}: missing TSV header")
        return [dict(row) for row in reader]


def as_int(row: dict[str, str], key: str, default: int = -1) -> int:
    text = (row.get(key) or "").strip()
    if text in ("", "-"):
        return default
    return int(text, 10)


def param_index(source_id: str) -> int | None:
    match = PARAM_RE.match(source_id)
    return None if match is None else int(match.group(1), 10)


def render_node(row: dict[str, str]) -> str:
    pieces = [f"token={row.get('node_token', '-')}" ]
    type_display = (row.get("type_display") or "-").strip()
    pointer_depth = (row.get("pointer_depth") or "-").strip()
    pointee = (row.get("pointee_type_display") or "-").strip()
    constant = (row.get("constant_value") or "-").strip()
    opcode = (row.get("defining_opcode") or "-").strip()
    target = (row.get("call_target_name") or "-").strip()
    truncation = (row.get("truncation") or "-").strip()
    if type_display not in ("", "-"):
        pieces.append(f"type={type_display}")
    if pointer_depth not in ("", "-", "0"):
        pieces.append(f"pointer_depth={pointer_depth}")
    if pointee not in ("", "-"):
        pieces.append(f"pointee={pointee}")
    if constant not in ("", "-"):
        pieces.append(f"constant={constant}")
    if opcode not in ("", "-"):
        pieces.append(f"def={opcode}")
    if target not in ("", "-"):
        pieces.append(f"call_target={target}")
    if truncation not in ("", "-"):
        pieces.append(f"truncation={truncation}")
    return " ".join(pieces)


def load_identities(path: Path) -> dict[str, dict[str, str]]:
    rows = read_tsv(path)
    identities: dict[str, dict[str, str]] = {}
    for row in rows:
        if (row.get("format") or "").strip() != IDENTITY_FORMAT:
            raise ValueError(f"{path}: expected {IDENTITY_FORMAT}")
        requested = (row.get("requested_entry") or "").strip()
        resolved = (row.get("resolved_entry") or "").strip()
        if requested in ("", "-") or resolved in ("", "-"):
            raise ValueError(f"{path}: unresolved function identity row")
        if requested in identities:
            raise ValueError(f"{path}: duplicate requested entry {requested}")
        identities[requested] = row
    if not identities:
        raise ValueError(f"{path}: no function identities")
    return identities


def build_report(subtrees_path: Path, identities_path: Path) -> tuple[str, bool]:
    rows = read_tsv(subtrees_path)
    if not rows:
        raise ValueError(f"{subtrees_path}: empty subtree export")
    if any((row.get("format") or "").strip() != SUBTREE_FORMAT for row in rows):
        raise ValueError(f"{subtrees_path}: expected {SUBTREE_FORMAT}")

    relevant = [
        row
        for row in rows
        if param_index((row.get("source_id") or "").strip()) is not None
    ]
    if not relevant:
        raise ValueError(f"{subtrees_path}: no createWithFile parameter rows")

    summaries = [row for row in relevant if (row.get("row_type") or "").strip() == "summary"]
    nodes = [row for row in relevant if (row.get("row_type") or "").strip() == "node"]
    roots = [row for row in nodes if as_int(row, "depth") == 0]
    if not roots:
        raise ValueError(f"{subtrees_path}: no parameter roots")

    parameters = sorted(
        {
            index
            for row in relevant
            if (index := param_index((row.get("source_id") or "").strip())) is not None
        }
    )
    if parameters != list(range(len(parameters))):
        raise ValueError(f"{subtrees_path}: non-contiguous parameter indices: {parameters}")

    summary_by_param: dict[int, dict[str, str]] = {}
    all_complete = True
    for row in summaries:
        index = param_index((row.get("source_id") or "").strip())
        if index is None:
            continue
        if index in summary_by_param:
            raise ValueError(f"{subtrees_path}: duplicate summary for param[{index}]")
        summary_by_param[index] = row
        all_complete &= (row.get("status") or "").strip() == "complete_caller_argument_subtrees"
    if sorted(summary_by_param) != parameters:
        raise ValueError(f"{subtrees_path}: missing parameter summaries")

    roots_by_call: dict[tuple[str, str], dict[int, list[dict[str, str]]]] = defaultdict(
        lambda: defaultdict(list)
    )
    nodes_by_call_param: dict[tuple[str, str, int], list[dict[str, str]]] = defaultdict(list)
    raw_names_by_call: dict[tuple[str, str], set[str]] = defaultdict(set)
    for row in nodes:
        index = param_index((row.get("source_id") or "").strip())
        if index is None:
            continue
        caller = (row.get("caller_entry") or "").strip()
        site = (row.get("caller_call_site") or "").strip()
        if caller in ("", "-") or site in ("", "-"):
            raise ValueError(f"{subtrees_path}: node without caller/call-site")
        key = (caller, site)
        nodes_by_call_param[(caller, site, index)].append(row)
        raw_name = (row.get("caller_name") or "").strip()
        if raw_name not in ("", "-"):
            raw_names_by_call[key].add(raw_name)
        if as_int(row, "depth") == 0:
            roots_by_call[key][index].append(row)

    for key, by_param in roots_by_call.items():
        for index in parameters:
            count = len(by_param.get(index, []))
            if count != 1:
                raise ValueError(
                    f"{subtrees_path}: call {key[0]}@{key[1]} param[{index}] has {count} roots"
                )

    calls = sorted(roots_by_call)
    callers = sorted({entry for entry, _site in calls})
    expected_calls = {as_int(row, "matched_caller_calls") for row in summaries}
    expected_callers = {as_int(row, "direct_caller_functions") for row in summaries}
    if len(expected_calls) != 1 or len(expected_callers) != 1:
        raise ValueError(f"{subtrees_path}: inconsistent summary coverage")
    if next(iter(expected_calls)) != len(calls):
        raise ValueError(
            f"{subtrees_path}: summary says {next(iter(expected_calls))} calls, grouped {len(calls)}"
        )
    if next(iter(expected_callers)) != len(callers):
        raise ValueError(
            f"{subtrees_path}: summary says {next(iter(expected_callers))} callers, grouped {len(callers)}"
        )

    identities = load_identities(identities_path)
    missing_identities = [entry for entry in callers if entry not in identities]
    if missing_identities:
        raise ValueError(
            f"{identities_path}: missing identities for: {', '.join(missing_identities)}"
        )

    def has_enemy_object(nodes_for_param: list[dict[str, str]]) -> bool:
        for row in nodes_for_param:
            for key in ("type_display", "pointee_type_display"):
                if "EnemyObject" in (row.get(key) or ""):
                    return True
        return False

    candidates: list[tuple[str, str, str, list[int]]] = []
    for caller, site in calls:
        root0 = roots_by_call[(caller, site)][0][0]
        target0 = (root0.get("call_target_name") or "").strip()
        enemy_sources = [
            index
            for index in parameters
            if has_enemy_object(nodes_by_call_param[(caller, site, index)])
        ]
        if "getCString" in target0 and enemy_sources:
            identity = identities[caller]
            candidates.append(
                (
                    caller,
                    site,
                    (identity.get("qualified_name") or identity.get("short_name") or "-").strip(),
                    enemy_sources,
                )
            )

    lines = ["EnemyActions createWithFile per-call matrix evidence"]
    lines.append(
        f"coverage: callers={len(callers)} calls={len(calls)} parameters={len(parameters)} "
        f"status={'complete' if all_complete else 'incomplete'}"
    )
    lines.append(f"structural intersections: {len(candidates)}")
    for caller, site, name, enemy_sources in candidates:
        sources = ",".join(f"param[{index}]" for index in enemy_sources)
        lines.append(
            f"  candidate: {caller}:{name} call_site={site} "
            f"param[0]_call=getCString enemy_object_sources={sources}"
        )

    for caller, site in calls:
        identity = identities[caller]
        qualified = (identity.get("qualified_name") or identity.get("short_name") or "-").strip()
        signature = (identity.get("signature") or "-").strip()
        raw_names = ",".join(sorted(raw_names_by_call[(caller, site)])) or "-"
        lines.append(f"call: {caller}:{qualified} call_site={site} raw_name={raw_names}")
        lines.append(f"  signature: {signature}")
        for index in parameters:
            root = roots_by_call[(caller, site)][index][0]
            markers: list[str] = []
            if "getCString" in (root.get("call_target_name") or ""):
                markers.append("root-call=getCString")
            if has_enemy_object(nodes_by_call_param[(caller, site, index)]):
                markers.append("enemy-object-provenance")
            marker_text = "" if not markers else " [" + ",".join(markers) + "]"
            lines.append(f"  param[{index}]: {render_node(root)}{marker_text}")

    lines.append(
        "semantic status: per-call structural provenance only; getCString and EnemyObject intersections "
        "do not by themselves assign filename/object-type/ID parameter meanings"
    )
    return "\n".join(lines) + "\n", all_complete


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("subtrees_tsv", type=Path)
    parser.add_argument("identities_tsv", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report, complete = build_report(args.subtrees_tsv, args.identities_tsv)
    except (OSError, ValueError) as error:
        raise SystemExit(f"error: {error}")
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0 if complete else 2


if __name__ == "__main__":
    raise SystemExit(main())
