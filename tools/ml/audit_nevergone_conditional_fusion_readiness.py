#!/usr/bin/env python3
"""Turn nested conditional-fusion CV stdout into a fail-closed readiness artifact.

The nested evaluator intentionally does not change production routing. This audit
adds a machine-readable validation gate on top of its untouched outer-family
results. It can identify policy keys worth independent production validation,
but it still does not modify a classifier artifact or the hierarchical router.
"""
from __future__ import annotations

import argparse
import ast
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.ConditionalFusionReadiness/1"
SOURCE_FORMAT = "NevergoneNestedConditionalFusionCV/1"
SCOPES = ("per_label", "per_label_action")

OUTER_RE = re.compile(
    r"^outer_fold=(?P<fold>\d+) n=(?P<n>\d+) "
    r"per_label=(?P<label>\{.*?\}) "
    r"per_label_action=(?P<action>\{.*\})$"
)
GROUP_RE = re.compile(
    r"^\s{2}(?P<key>[^:]+): examples=(?P<examples>\d+) "
    r"correct=(?P<correct>\d+) precision=(?P<precision>[0-9.]+)$"
)
STABILITY_RE = re.compile(
    r"^\s{2}(?P<key>[^:]+): enabled_folds=(?P<enabled>\d+)/(?P<total>\d+) "
    r"sim_thresholds=(?P<sims>\[.*?\]) gap_thresholds=(?P<gaps>\[.*?\]) "
    r"inner_precision_min=(?P<inner>None|[0-9.eE+-]+)$"
)
ERROR_RE = re.compile(r"^\s{2}(?P<id>\S+) truth=(?P<truth>\S+) ")


def literal_dict(text: str, where: str) -> dict[str, Any]:
    try:
        value = ast.literal_eval(text)
    except (SyntaxError, ValueError) as exc:
        raise ValueError(f"{where}: invalid Python literal") from exc
    if not isinstance(value, dict):
        raise ValueError(f"{where}: expected dict")
    return value


def literal_list(text: str, where: str) -> list[Any]:
    try:
        value = ast.literal_eval(text)
    except (SyntaxError, ValueError) as exc:
        raise ValueError(f"{where}: invalid Python literal") from exc
    if not isinstance(value, list):
        raise ValueError(f"{where}: expected list")
    return value


def read_summary_after(lines: list[str], title: str, prefix: str | None = None) -> dict[str, Any]:
    try:
        index = lines.index(title)
    except ValueError as exc:
        raise ValueError(f"missing section: {title}") from exc
    if index + 1 >= len(lines):
        raise ValueError(f"missing summary after {title}")
    line = lines[index + 1].strip()
    if prefix is not None:
        if not line.startswith(prefix + " "):
            raise ValueError(f"{title}: expected {prefix} summary first")
        line = line[len(prefix) + 1 :]
    return literal_dict(line, title)


def section_lines(lines: list[str], title: str, next_title: str | None) -> list[str]:
    try:
        start = lines.index(title) + 1
    except ValueError as exc:
        raise ValueError(f"missing section: {title}") from exc
    if next_title is None:
        end = len(lines)
    else:
        try:
            end = lines.index(next_title, start)
        except ValueError as exc:
            raise ValueError(f"missing section after {title}: {next_title}") from exc
    return lines[start:end]


def scope_block(lines: list[str], title: str, scope: str, next_title: str) -> list[str]:
    body = section_lines(lines, title, next_title)
    indices = [i for i, line in enumerate(body) if line.strip() in SCOPES]
    selected = None
    for pos, index in enumerate(indices):
        if body[index].strip() == scope:
            end = indices[pos + 1] if pos + 1 < len(indices) else len(body)
            selected = body[index + 1 : end]
            break
    if selected is None:
        raise ValueError(f"{title}: missing scope {scope}")
    return selected


def parse_log(path: Path, scope: str, expected_folds: int) -> dict[str, Any]:
    lines = [line.rstrip("\n") for line in path.read_text(encoding="utf-8").splitlines()]
    if f"model: {SOURCE_FORMAT}" not in lines:
        raise ValueError("input is not nested conditional fusion CV output")

    task_line = next((line for line in lines if line.startswith("task: ")), None)
    functions_line = next((line for line in lines if line.startswith("functions: ")), None)
    families_line = next((line for line in lines if line.startswith("families: ")), None)
    trusted_line = next((line for line in lines if line.startswith("trusted transfer classes: ")), None)
    if None in (task_line, functions_line, families_line, trusted_line):
        raise ValueError("missing conditional fusion CV header fields")

    trusted = literal_list(trusted_line.split(": ", 1)[1], "trusted transfer classes")
    if not all(isinstance(item, str) for item in trusted):
        raise ValueError("trusted transfer classes must be strings")

    outer: dict[int, dict[str, Any]] = {}
    for line in lines:
        match = OUTER_RE.match(line)
        if not match:
            continue
        fold = int(match.group("fold"))
        if fold in outer:
            raise ValueError(f"duplicate outer fold: {fold}")
        outer[fold] = {
            "n": int(match.group("n")),
            "per_label": literal_dict(match.group("label"), f"outer fold {fold} per_label"),
            "per_label_action": literal_dict(match.group("action"), f"outer fold {fold} per_label_action"),
        }
    if set(outer) != set(range(1, expected_folds + 1)):
        raise ValueError(
            f"outer folds mismatch: expected 1..{expected_folds}, got {sorted(outer)}"
        )

    base = read_summary_after(lines, "BASE CLASSIFIER ACCEPT")

    promotion_body = section_lines(lines, "CONDITIONAL PROMOTION", "FUSED ACCEPT")
    promotion: dict[str, dict[str, Any]] = {}
    for line in promotion_body:
        stripped = line.strip()
        for candidate in SCOPES:
            if stripped.startswith(candidate + " "):
                promotion[candidate] = literal_dict(
                    stripped[len(candidate) + 1 :], f"conditional promotion {candidate}"
                )
    if set(promotion) != set(SCOPES):
        raise ValueError("conditional promotion summaries incomplete")

    fused_body = section_lines(lines, "FUSED ACCEPT", "PROMOTION BY LABEL/ACTION")
    fused: dict[str, dict[str, Any]] = {}
    for line in fused_body:
        stripped = line.strip()
        for candidate in SCOPES:
            if stripped.startswith(candidate + " "):
                fused[candidate] = literal_dict(
                    stripped[len(candidate) + 1 :], f"fused accept {candidate}"
                )
    if set(fused) != set(SCOPES):
        raise ValueError("fused accept summaries incomplete")

    groups: dict[str, dict[str, Any]] = {}
    for line in scope_block(
        lines,
        "PROMOTION BY LABEL/ACTION",
        scope,
        "POLICY STABILITY",
    ):
        match = GROUP_RE.match(line)
        if not match:
            continue
        key = match.group("key")
        if key in groups:
            raise ValueError(f"duplicate promotion group: {key}")
        examples = int(match.group("examples"))
        correct = int(match.group("correct"))
        precision = float(match.group("precision"))
        if not (0 <= correct <= examples):
            raise ValueError(f"invalid promotion group counts: {key}")
        groups[key] = {
            "examples": examples,
            "correct": correct,
            "precision": precision,
        }

    stability: dict[str, dict[str, Any]] = {}
    for line in scope_block(lines, "POLICY STABILITY", scope, "PROMOTED ERRORS"):
        match = STABILITY_RE.match(line)
        if not match:
            continue
        key = match.group("key")
        if key in stability:
            raise ValueError(f"duplicate stability row: {key}")
        total = int(match.group("total"))
        if total != expected_folds:
            raise ValueError(f"policy stability fold count changed for {key}: {total}")
        inner_text = match.group("inner")
        stability[key] = {
            "enabled_folds": int(match.group("enabled")),
            "total_folds": total,
            "similarity_thresholds": literal_list(match.group("sims"), f"{key} sim thresholds"),
            "gap_thresholds": literal_list(match.group("gaps"), f"{key} gap thresholds"),
            "inner_precision_min": None if inner_text == "None" else float(inner_text),
        }

    errors = 0
    for line in scope_block(lines, "PROMOTED ERRORS", scope, "__END__") if False else []:
        if ERROR_RE.match(line):
            errors += 1
    # PROMOTED ERRORS is the final section. Parse the selected scope manually.
    error_body = section_lines(lines, "PROMOTED ERRORS", None)
    indices = [i for i, line in enumerate(error_body) if line.strip() in SCOPES]
    selected_errors: list[str] = []
    for pos, index in enumerate(indices):
        if error_body[index].strip() == scope:
            end = indices[pos + 1] if pos + 1 < len(indices) else len(error_body)
            selected_errors = error_body[index + 1 : end]
            break
    errors = sum(1 for line in selected_errors if ERROR_RE.match(line))

    return {
        "task": task_line.split(": ", 1)[1],
        "functions": int(functions_line.split(": ", 1)[1]),
        "families": int(families_line.split(": ", 1)[1]),
        "trusted_transfer_classes": trusted,
        "outer": outer,
        "base": base,
        "promotion": promotion[scope],
        "fused": fused[scope],
        "groups": groups,
        "stability": stability,
        "printed_promoted_errors": errors,
    }


def policy_key_from_group(group_key: str, scope: str) -> str:
    parts = group_key.rsplit("/", 1)
    if len(parts) != 2:
        raise ValueError(f"promotion group must be label/action: {group_key}")
    label, action = parts
    if scope == "per_label":
        return label
    return f"{label}/{action}"


def evaluate(
    parsed: dict[str, Any],
    *,
    scope: str,
    candidate_labels: set[str],
    min_outer_precision: float,
    min_examples: int,
    min_enabled_folds: int,
    min_inner_precision: float,
    min_total_promotions: int,
    precision_tolerance: float,
) -> dict[str, Any]:
    aggregated: dict[str, dict[str, Any]] = defaultdict(lambda: {"examples": 0, "correct": 0})
    for group_key, stats in parsed["groups"].items():
        policy_key = policy_key_from_group(group_key, scope)
        label = policy_key.split("/", 1)[0]
        if label not in candidate_labels:
            continue
        aggregated[policy_key]["examples"] += int(stats["examples"])
        aggregated[policy_key]["correct"] += int(stats["correct"])

    decisions: dict[str, dict[str, Any]] = {}
    eligible: list[str] = []
    for key in sorted(aggregated):
        stats = aggregated[key]
        examples = int(stats["examples"])
        correct = int(stats["correct"])
        precision = correct / examples if examples else 0.0
        stability = parsed["stability"].get(key)
        reasons: list[str] = []
        if examples < min_examples:
            reasons.append("insufficient_outer_promotions")
        if precision + 1e-12 < min_outer_precision:
            reasons.append("outer_precision_below_gate")
        if stability is None:
            reasons.append("missing_policy_stability")
        else:
            if int(stability["enabled_folds"]) < min_enabled_folds:
                reasons.append("policy_not_enabled_in_enough_folds")
            inner_precision = stability.get("inner_precision_min")
            if inner_precision is None or float(inner_precision) + 1e-12 < min_inner_precision:
                reasons.append("inner_precision_below_gate")
        accepted = not reasons
        if accepted:
            eligible.append(key)
        decisions[key] = {
            "eligible_for_independent_validation": accepted,
            "outer_examples": examples,
            "outer_correct": correct,
            "outer_precision": precision,
            "stability": stability,
            "reasons": reasons,
        }

    promoted_examples = sum(decisions[key]["outer_examples"] for key in eligible)
    promoted_correct = sum(decisions[key]["outer_correct"] for key in eligible)
    base_examples = int(parsed["base"].get("examples", 0))
    base_precision = float(parsed["base"].get("precision", 0.0))
    base_correct = base_examples * base_precision
    total_functions = int(parsed["functions"])
    candidate_examples = base_examples + promoted_examples
    candidate_correct = base_correct + promoted_correct
    candidate_precision = candidate_correct / candidate_examples if candidate_examples else 0.0
    candidate_coverage = candidate_examples / total_functions if total_functions else 0.0

    global_reasons: list[str] = []
    if promoted_examples < min_total_promotions:
        global_reasons.append("insufficient_total_promotions")
    if not eligible:
        global_reasons.append("no_policy_key_passed")
    if candidate_precision + precision_tolerance < base_precision:
        global_reasons.append("candidate_fused_precision_below_base")

    return {
        "format": FORMAT,
        "source_format": SOURCE_FORMAT,
        "task": parsed["task"],
        "scope": scope,
        "functions": total_functions,
        "families": parsed["families"],
        "trusted_transfer_classes": parsed["trusted_transfer_classes"],
        "candidate_labels": sorted(candidate_labels),
        "gates": {
            "min_outer_precision": min_outer_precision,
            "min_examples_per_policy": min_examples,
            "min_enabled_folds": min_enabled_folds,
            "min_inner_precision": min_inner_precision,
            "min_total_promotions": min_total_promotions,
            "precision_tolerance": precision_tolerance,
        },
        "base_accept": parsed["base"],
        "reported_scope_promotion": parsed["promotion"],
        "reported_scope_fused_accept": parsed["fused"],
        "printed_promoted_errors": parsed["printed_promoted_errors"],
        "policy_keys": decisions,
        "eligible_policy_keys": eligible,
        "candidate_fused_accept": {
            "examples": candidate_examples,
            "coverage": candidate_coverage,
            "precision": candidate_precision,
            "promoted_examples": promoted_examples,
            "promoted_correct": promoted_correct,
        },
        "ready_for_independent_validation": not global_reasons,
        "production_routing_changed": False,
        "global_reasons": global_reasons,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cv_log", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--scope", choices=SCOPES, default="per_label_action")
    parser.add_argument("--expected-folds", type=int, default=5)
    parser.add_argument(
        "--candidate-labels",
        nargs="+",
        default=["engine", "media_library", "runtime_library"],
    )
    parser.add_argument("--min-outer-precision", type=float, default=0.95)
    parser.add_argument("--min-examples", type=int, default=10)
    parser.add_argument("--min-enabled-folds", type=int, default=3)
    parser.add_argument("--min-inner-precision", type=float, default=0.95)
    parser.add_argument("--min-total-promotions", type=int, default=10)
    parser.add_argument("--precision-tolerance", type=float, default=1e-12)
    parser.add_argument("--require-ready", action="store_true")
    args = parser.parse_args()

    for name, value in (
        ("min_outer_precision", args.min_outer_precision),
        ("min_inner_precision", args.min_inner_precision),
    ):
        if not (0.0 < value <= 1.0):
            raise SystemExit(f"--{name.replace('_', '-')} must be in (0,1]")
    if args.expected_folds < 2 or args.min_enabled_folds < 1:
        raise SystemExit("fold counts must be positive and expected folds >= 2")
    if args.min_enabled_folds > args.expected_folds:
        raise SystemExit("--min-enabled-folds cannot exceed --expected-folds")
    if args.min_examples < 1 or args.min_total_promotions < 1:
        raise SystemExit("minimum example counts must be >= 1")
    if args.precision_tolerance < 0:
        raise SystemExit("--precision-tolerance must be >= 0")

    parsed = parse_log(args.cv_log, args.scope, args.expected_folds)
    candidate_labels = set(args.candidate_labels)
    unknown = candidate_labels - set(parsed["trusted_transfer_classes"])
    if unknown:
        raise SystemExit(
            "candidate labels were not predeclared as trusted in the CV run: "
            + ", ".join(sorted(unknown))
        )

    result = evaluate(
        parsed,
        scope=args.scope,
        candidate_labels=candidate_labels,
        min_outer_precision=args.min_outer_precision,
        min_examples=args.min_examples,
        min_enabled_folds=args.min_enabled_folds,
        min_inner_precision=args.min_inner_precision,
        min_total_promotions=args.min_total_promotions,
        precision_tolerance=args.precision_tolerance,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", FORMAT)
    print("scope:", args.scope)
    print("eligible policy keys:", result["eligible_policy_keys"])
    print("candidate fused accept:", result["candidate_fused_accept"])
    print("ready for independent validation:", result["ready_for_independent_validation"])
    print("global reasons:", result["global_reasons"])
    print("production routing changed: no")
    print("output:", args.output)

    if args.require_ready and not result["ready_for_independent_validation"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
