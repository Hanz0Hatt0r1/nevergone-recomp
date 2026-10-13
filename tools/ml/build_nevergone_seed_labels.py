#!/usr/bin/env python3
"""Generate conservative seed labels from the checked-in Nevergone function index.

Rules operate only on Ghidra program identity and recovered qualified function
names. The names are used to create labels, never as model input.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import json
import re
from collections import Counter
from pathlib import Path
from typing import Any

from nevergone_function_origin import classify_function_origin

FORMAT = "NEVERGONE.MlSeedLabels/1"


def load_rules(path: Path) -> list[dict[str, Any]]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict) or not isinstance(payload.get("rules"), list):
        raise ValueError("rules file must contain a rules array")
    rules = []
    for index, rule in enumerate(payload["rules"]):
        if not isinstance(rule, dict):
            raise ValueError(f"rule {index}: expected object")
        pattern = rule.get("pattern")
        labels = rule.get("labels")
        if not isinstance(pattern, str) or not pattern:
            raise ValueError(f"rule {index}: non-empty pattern required")
        if not isinstance(labels, dict) or not labels:
            raise ValueError(f"rule {index}: non-empty labels object required")
        compiled = dict(rule)
        compiled["_pattern"] = re.compile(pattern)
        program_pattern = rule.get("program_pattern")
        compiled["_program_pattern"] = (
            re.compile(program_pattern) if isinstance(program_pattern, str) and program_pattern else None
        )
        rules.append(compiled)
    return rules


def generate(functions_path: Path, program: str, rules_path: Path, output_path: Path) -> dict[str, Any]:
    rules = load_rules(rules_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    functions = 0
    matched_functions = 0
    label_rows = 0
    per_task: dict[str, dict[str, int]] = {}
    excluded_origins: Counter[str] = Counter()

    with gzip.open(functions_path, "rt", encoding="utf-8", newline="") as source, \
         output_path.open("w", encoding="utf-8") as out:
        reader = csv.DictReader(source, delimiter="\t")
        expected = {"entry", "name", "address_count", "is_external", "signature"}
        if set(reader.fieldnames or []) != expected:
            raise ValueError(
                f"unexpected functions header: {reader.fieldnames}; expected {sorted(expected)}"
            )

        for row in reader:
            functions += 1
            if row["is_external"].lower() == "true":
                excluded_origins["ghidra_external"] += 1
                continue
            name = row["name"]
            origin = classify_function_origin(name)
            if origin != "internal":
                excluded_origins[origin] += 1
                continue
            entry = row["entry"]
            selected: dict[str, tuple[str, str, str]] = {}

            for rule in rules:
                program_pattern = rule["_program_pattern"]
                if program_pattern is not None and program_pattern.search(program) is None:
                    continue
                if rule["_pattern"].search(name) is None:
                    continue

                quality = rule.get("quality", "gold")
                evidence = rule.get("evidence", "qualified Ghidra function name")
                for task, label in rule["labels"].items():
                    if not isinstance(task, str) or not isinstance(label, str):
                        raise ValueError("rule labels must map strings to strings")
                    previous = selected.get(task)
                    candidate = (label, quality, evidence)
                    if previous is not None and previous[0] != label:
                        raise ValueError(
                            f"conflicting rules for {program} {entry} {name!r} task={task}: "
                            f"{previous[0]!r} vs {label!r}"
                        )
                    selected[task] = candidate

            if not selected:
                continue
            matched_functions += 1
            for task, (label, quality, evidence) in sorted(selected.items()):
                payload = {
                    "format": FORMAT,
                    "program": program,
                    "source_entry": entry,
                    "source_name": name,
                    "task": task,
                    "label": label,
                    "quality": quality,
                    "evidence": evidence,
                }
                out.write(json.dumps(payload, sort_keys=True) + "\n")
                label_rows += 1
                per_task.setdefault(task, {}).setdefault(label, 0)
                per_task[task][label] += 1

    return {
        "format": FORMAT,
        "program": program,
        "functions_seen": functions,
        "matched_functions": matched_functions,
        "label_rows": label_rows,
        "excluded_function_origins": dict(sorted(excluded_origins.items())),
        "task_distribution": per_task,
        "output": str(output_path),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("functions_tsv_gz", type=Path)
    parser.add_argument("program")
    parser.add_argument("rules_json", type=Path)
    parser.add_argument("output_jsonl", type=Path)
    args = parser.parse_args()

    report = generate(args.functions_tsv_gz, args.program, args.rules_json, args.output_jsonl)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
