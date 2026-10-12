#!/usr/bin/env python3
"""Summarize the bounded SceneActions Ghidra export without retaining raw decompiler text.

The exporter in tools/ghidra/ExportSceneActionTriggerSlice.java intentionally writes a
small local-only evidence slice. This helper turns that slice into a compact review
report that can be committed or shared safely: it records target resolution plus
short candidate lines around the two still-open boundaries (spatial trigger and the
TrapActionsData +0xbc frame-count edge). It never copies complete instruction or
C-decompiler sections into the report.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

SEPARATOR = "=" * 60
EXPECTED_TARGETS = (
    "GameScene::TriggerSceneAction(float)",
    "GameSceneClassLayer::showEventObjTip(...)",
    "GameSceneObject::triggerSceneAction()",
    "GameSceneObject::isContainsRect(CCRect)",
    "GameSceneObject::isContainsRect(CCRect,const char*)",
    "GameSceneObject::getObjectRect()",
    "SceneActionsSystem::waUpdate(float)",
    "TrapActionsData::loadWBGFile(...)",
)

SPATIAL_TARGETS = EXPECTED_TARGETS[:6]
COUNT_TARGETS = EXPECTED_TARGETS[6:]

SECTION_MARKERS = {
    "-- CALLERS --": "callers",
    "-- INSTRUCTIONS --": "instructions",
    "-- DECOMPILE --": "decompile",
}

# Deliberately lexical only. These patterns surface local evidence; they do not
# claim that an offset or branch has already been semantically identified.
COUNT_PATTERNS = (
    re.compile(r"\b0x0*bc\b", re.IGNORECASE),
    re.compile(r"\b188\b"),
    re.compile(r"#0x0*bc\b", re.IGNORECASE),
    re.compile(r"#188\b"),
)
SPATIAL_PATTERNS = (
    re.compile(r"isContainsRect", re.IGNORECASE),
    re.compile(r"getObjectRect", re.IGNORECASE),
    re.compile(r"showEventObjTip", re.IGNORECASE),
    re.compile(r"CCRect", re.IGNORECASE),
    re.compile(r"contains", re.IGNORECASE),
)
BRANCH_PATTERN = re.compile(
    r"\b(?:cmp|cmn|bgt|bge|blt|ble|beq|bne|bhi|bhs|blo|bls)\b",
    re.IGNORECASE,
)


@dataclass
class TargetBlock:
    target: str
    metadata: dict[str, str]
    callers: list[str]
    instructions: list[str]
    decompile: list[str]

    @property
    def resolved(self) -> bool:
        return self.metadata.get("status") != "FUNCTION_NOT_FOUND_OR_AMBIGUOUS" and bool(
            self.metadata.get("function")
        )


def _clean_candidate(line: str, limit: int = 240) -> str:
    value = " ".join(line.strip().split())
    if len(value) <= limit:
        return value
    return value[: limit - 1] + "…"


def parse_slice(text: str) -> tuple[dict[str, str], dict[str, TargetBlock]]:
    if not text.startswith("# Nevergone SceneActions spatial-trigger/count slice"):
        raise ValueError("not a SceneActions spatial-trigger/count slice")

    chunks = text.split(SEPARATOR)
    header_lines = [line.strip() for line in chunks[0].splitlines() if line.strip()]
    header: dict[str, str] = {}
    for line in header_lines[1:]:
        if "=" in line:
            key, value = line.split("=", 1)
            header[key.strip()] = value.strip()

    blocks: dict[str, TargetBlock] = {}
    for raw in chunks[1:]:
        lines = raw.splitlines()
        metadata: dict[str, str] = {}
        sections: dict[str, list[str]] = {
            "callers": [],
            "instructions": [],
            "decompile": [],
        }
        current: str | None = None
        target = ""
        for line in lines:
            stripped = line.strip()
            if stripped in SECTION_MARKERS:
                current = SECTION_MARKERS[stripped]
                continue
            if current is None and "=" in line:
                key, value = line.split("=", 1)
                key = key.strip()
                value = value.strip()
                metadata[key] = value
                if key == "target":
                    target = value
                continue
            if current is not None and stripped:
                sections[current].append(line.rstrip())
        if not target:
            continue
        if target in blocks:
            raise ValueError(f"duplicate target block: {target}")
        blocks[target] = TargetBlock(
            target=target,
            metadata=metadata,
            callers=sections["callers"],
            instructions=sections["instructions"],
            decompile=sections["decompile"],
        )
    return header, blocks


def _candidate_lines(lines: Iterable[str], patterns: Iterable[re.Pattern[str]], limit: int = 12) -> list[str]:
    result: list[str] = []
    seen: set[str] = set()
    for line in lines:
        if not any(pattern.search(line) for pattern in patterns):
            continue
        cleaned = _clean_candidate(line)
        if cleaned in seen:
            continue
        seen.add(cleaned)
        result.append(cleaned)
        if len(result) >= limit:
            break
    return result


def build_report(header: dict[str, str], blocks: dict[str, TargetBlock]) -> dict[str, object]:
    missing = [target for target in EXPECTED_TARGETS if target not in blocks]
    unresolved = [
        target for target in EXPECTED_TARGETS if target in blocks and not blocks[target].resolved
    ]

    targets: list[dict[str, object]] = []
    for target in EXPECTED_TARGETS:
        block = blocks.get(target)
        if block is None:
            targets.append({"target": target, "present": False, "resolved": False})
            continue
        targets.append(
            {
                "target": target,
                "present": True,
                "resolved": block.resolved,
                "function": block.metadata.get("function", ""),
                "entry": block.metadata.get("entry", ""),
                "prototype": block.metadata.get("prototype", ""),
                "caller_count": len([line for line in block.callers if line.strip() != "<none>"]),
            }
        )

    spatial_candidates: dict[str, list[str]] = {}
    for target in SPATIAL_TARGETS:
        block = blocks.get(target)
        if block is None or not block.resolved:
            spatial_candidates[target] = []
            continue
        spatial_candidates[target] = _candidate_lines(
            [*block.instructions, *block.decompile], SPATIAL_PATTERNS
        )

    count_candidates: dict[str, dict[str, list[str]]] = {}
    for target in COUNT_TARGETS:
        block = blocks.get(target)
        if block is None or not block.resolved:
            count_candidates[target] = {"offset_bc": [], "branches": []}
            continue
        count_candidates[target] = {
            "offset_bc": _candidate_lines(
                [*block.instructions, *block.decompile], COUNT_PATTERNS, limit=16
            ),
            "branches": _candidate_lines(block.instructions, (BRANCH_PATTERN,), limit=20),
        }

    return {
        "format": "nevergone-scene-actions-slice-report-v1",
        "source": {
            "program": header.get("program", ""),
            "imageBase": header.get("imageBase", ""),
            "language": header.get("language", ""),
        },
        "complete": not missing and not unresolved,
        "missing_targets": missing,
        "unresolved_targets": unresolved,
        "targets": targets,
        "spatial_trigger_candidates": spatial_candidates,
        "frame_count_candidates": count_candidates,
        "interpretation_note": (
            "Candidate lines are lexical evidence only. The report intentionally does not infer "
            "rectangle semantics, a +0xbc mutation, count/index equivalence, or PRNG ordering."
        ),
    }


def render_markdown(report: dict[str, object]) -> str:
    source = report["source"]
    assert isinstance(source, dict)
    lines = [
        "# SceneActions Ghidra slice report",
        "",
        f"- Program: `{source.get('program', '')}`",
        f"- Image base: `{source.get('imageBase', '')}`",
        f"- Language: `{source.get('language', '')}`",
        f"- Complete target set: `{'yes' if report['complete'] else 'no'}`",
        "",
        "## Target resolution",
        "",
    ]
    for target in report["targets"]:
        assert isinstance(target, dict)
        status = "resolved" if target.get("resolved") else "missing/unresolved"
        entry = target.get("entry", "")
        suffix = f" @ `{entry}`" if entry else ""
        lines.append(f"- `{target.get('target', '')}` — {status}{suffix}")

    lines.extend(["", "## Spatial-trigger candidates", ""])
    spatial = report["spatial_trigger_candidates"]
    assert isinstance(spatial, dict)
    for target, candidates in spatial.items():
        lines.append(f"### `{target}`")
        if candidates:
            for candidate in candidates:
                lines.append(f"- `{candidate}`")
        else:
            lines.append("- No lexical candidate surfaced.")
        lines.append("")

    lines.extend(["## Frame-count candidates", ""])
    count = report["frame_count_candidates"]
    assert isinstance(count, dict)
    for target, groups in count.items():
        assert isinstance(groups, dict)
        lines.append(f"### `{target}`")
        offset_lines = groups.get("offset_bc", [])
        branch_lines = groups.get("branches", [])
        lines.append("- `+0xbc` / decimal-188 candidates:")
        if offset_lines:
            for candidate in offset_lines:
                lines.append(f"  - `{candidate}`")
        else:
            lines.append("  - none")
        lines.append("- compare/branch instruction candidates:")
        if branch_lines:
            for candidate in branch_lines:
                lines.append(f"  - `{candidate}`")
        else:
            lines.append("  - none")
        lines.append("")

    lines.append(f"> {report['interpretation_note']}")
    lines.append("")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("slice", type=Path, help="local output from ExportSceneActionTriggerSlice.java")
    parser.add_argument("--json", action="store_true", help="emit JSON instead of Markdown")
    parser.add_argument("--output", type=Path, help="write report to this file instead of stdout")
    parser.add_argument(
        "--allow-incomplete",
        action="store_true",
        help="return success even when one of the expected targets is missing/unresolved",
    )
    args = parser.parse_args(argv)

    try:
        text = args.slice.read_text(encoding="utf-8")
        header, blocks = parse_slice(text)
        report = build_report(header, blocks)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"scene_actions_slice_report: {error}", file=sys.stderr)
        return 2

    rendered = (
        json.dumps(report, indent=2, sort_keys=True) + "\n"
        if args.json
        else render_markdown(report)
    )
    if args.output is None:
        sys.stdout.write(rendered)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")

    if not report["complete"] and not args.allow_incomplete:
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
