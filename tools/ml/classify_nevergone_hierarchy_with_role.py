#!/usr/bin/env python3
"""Run Nevergone hierarchy with a parallel function-role production head.

The existing hierarchical router remains authoritative for ``final_action``:
objective origin -> code_domain -> optional subsystem.  This wrapper adds a
parallel ``function_role`` prediction for every internal function.  Function
role does not upgrade the hierarchy's final action; it is auxiliary semantic
information for downstream analysis selection.

Only independently validated role labels may retain model ``accept``.  By
default only ``callback`` is allowed.  Any model accept for a non-whitelisted
role is demoted to ``review`` and annotated with a production guard.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path
from typing import Any


FORMAT = "NEVERGONE.HierarchicalFunctionRouterWithRole/1"
DEFAULT_ACCEPT_ROLES = {"callback"}


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


def run_base_hierarchy(
    *,
    code_domain_model: Path,
    subsystem_model: Path,
    input_path: Path,
    output_path: Path,
    no_probabilities: bool,
    blocked_subsystem_accept: list[str],
) -> None:
    script = Path(__file__).with_name("classify_nevergone_hierarchy.py")
    command = [
        sys.executable,
        str(script),
        str(code_domain_model),
        str(subsystem_model),
        str(input_path),
        "--output",
        str(output_path),
        "--block-subsystem-accept",
        *blocked_subsystem_accept,
    ]
    if no_probabilities:
        command.append("--no-probabilities")
    subprocess.run(command, check=True)


def run_role_classifier(
    *,
    model_dir: Path,
    rows: list[dict[str, Any]],
    no_probabilities: bool,
) -> list[dict[str, Any]]:
    if not rows:
        return []
    with tempfile.TemporaryDirectory(prefix="nevergone-function-role-") as temporary:
        root = Path(temporary)
        input_path = root / "input.jsonl"
        output_path = root / "output.jsonl"
        write_jsonl(input_path, rows)

        script = Path(__file__).with_name("classify_nevergone_pcode.py")
        command = [
            sys.executable,
            str(script),
            str(model_dir),
            str(input_path),
            "--output",
            str(output_path),
        ]
        if no_probabilities:
            command.append("--no-probabilities")
        subprocess.run(command, check=True)

        classified = read_jsonl(output_path)
        if len(classified) != len(rows):
            raise RuntimeError(
                f"function_role classifier row count mismatch: "
                f"expected {len(rows)}, got {len(classified)}"
            )
        for result in classified:
            if result.get("task") != "function_role":
                raise RuntimeError(
                    "expected function_role classifier artifact, "
                    f"got task={result.get('task')!r}"
                )
        return classified


def apply_role_policy(
    role_result: dict[str, Any],
    *,
    allowed_accept_roles: set[str],
) -> dict[str, Any]:
    prediction = role_result.get("prediction")
    action = role_result.get("action")
    if not isinstance(prediction, str) or action not in {"accept", "review", "escalate"}:
        raise ValueError("invalid function_role classifier result")

    routed = dict(role_result)
    if action == "accept" and prediction not in allowed_accept_roles:
        routed["model_action"] = "accept"
        routed["action"] = "review"
        routed["production_guard"] = "function_role_accept_not_validated"
    return routed


def attach_roles(
    *,
    source_rows: list[dict[str, Any]],
    hierarchy_rows: list[dict[str, Any]],
    role_rows: list[dict[str, Any]],
    allowed_accept_roles: set[str],
) -> None:
    if len(source_rows) != len(hierarchy_rows):
        raise RuntimeError("base hierarchy row count mismatch")

    internal_indices = [
        index
        for index, row in enumerate(hierarchy_rows)
        if isinstance(row.get("function_origin"), dict)
        and row["function_origin"].get("prediction") == "internal"
    ]
    if len(internal_indices) != len(role_rows):
        raise RuntimeError(
            f"function_role join mismatch: internal={len(internal_indices)} "
            f"role_rows={len(role_rows)}"
        )

    for row in hierarchy_rows:
        row["format"] = FORMAT
        row["function_role"] = None

    for index, role_result in zip(internal_indices, role_rows):
        routed = apply_role_policy(
            role_result,
            allowed_accept_roles=allowed_accept_roles,
        )
        hierarchy_rows[index]["function_role"] = routed
        invoked = hierarchy_rows[index].setdefault("semantic_models_invoked", [])
        if "function_role" not in invoked:
            invoked.append("function_role")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("code_domain_model", type=Path)
    parser.add_argument("subsystem_model", type=Path)
    parser.add_argument("function_role_model", type=Path)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--no-probabilities", action="store_true")
    parser.add_argument(
        "--block-subsystem-accept",
        nargs="*",
        default=["combat"],
        metavar="LABEL",
        help="subsystem labels that may never produce final accept (default: combat)",
    )
    parser.add_argument(
        "--allow-function-role-accept",
        nargs="*",
        default=sorted(DEFAULT_ACCEPT_ROLES),
        metavar="LABEL",
        help="role labels independently validated for auto-accept (default: callback)",
    )
    args = parser.parse_args()

    source_rows = read_jsonl(args.input)
    allowed_roles = set(args.allow_function_role_accept)

    with tempfile.TemporaryDirectory(prefix="nevergone-hierarchy-role-") as temporary:
        root = Path(temporary)
        base_output = root / "hierarchy.jsonl"
        run_base_hierarchy(
            code_domain_model=args.code_domain_model,
            subsystem_model=args.subsystem_model,
            input_path=args.input,
            output_path=base_output,
            no_probabilities=args.no_probabilities,
            blocked_subsystem_accept=args.block_subsystem_accept,
        )
        hierarchy_rows = read_jsonl(base_output)

    internal_source_rows: list[dict[str, Any]] = []
    if len(source_rows) != len(hierarchy_rows):
        raise RuntimeError("base hierarchy output count mismatch")
    for source, routed in zip(source_rows, hierarchy_rows):
        origin = routed.get("function_origin")
        if isinstance(origin, dict) and origin.get("prediction") == "internal":
            internal_source_rows.append(source)

    role_results = run_role_classifier(
        model_dir=args.function_role_model,
        rows=internal_source_rows,
        no_probabilities=args.no_probabilities,
    )
    attach_roles(
        source_rows=source_rows,
        hierarchy_rows=hierarchy_rows,
        role_rows=role_results,
        allowed_accept_roles=allowed_roles,
    )

    write_jsonl(args.output, hierarchy_rows)

    final_actions = Counter(row.get("final_action") for row in hierarchy_rows)
    role_predictions = Counter(
        row["function_role"]["prediction"]
        for row in hierarchy_rows
        if isinstance(row.get("function_role"), dict)
    )
    role_actions = Counter(
        row["function_role"]["action"]
        for row in hierarchy_rows
        if isinstance(row.get("function_role"), dict)
    )
    guarded_roles = Counter(
        row["function_role"]["prediction"]
        for row in hierarchy_rows
        if isinstance(row.get("function_role"), dict)
        and row["function_role"].get("production_guard")
    )

    print("functions:", len(hierarchy_rows))
    print("final actions:", final_actions)
    print("function_role invoked:", len(role_results))
    print("function_role predictions:", role_predictions)
    print("function_role actions:", role_actions)
    print("function_role guarded accepts:", guarded_roles)
    print("allowed function_role accepts:", sorted(allowed_roles))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
