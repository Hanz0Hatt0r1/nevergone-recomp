#!/usr/bin/env python3
"""Parse Nevergone runtime diagnostics from an Android uiautomator XML dump."""

from __future__ import annotations

import argparse
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

REFRESH_TEXT = "Refresh runtime diagnostics"
SCROLL_VIEW_CLASS = "android.widget.ScrollView"
BOUNDS_RE = re.compile(r"^\[(\d+),(\d+)\]\[(\d+),(\d+)\]$")
RESUME_RE = re.compile(r"(?:^|\s)resume events:\s*(\d+)(?:\s|$)")
PAUSE_RE = re.compile(r"(?:^|\s)pause events:\s*(\d+)(?:\s|$)")
SURFACE_RE = re.compile(r"(?:^|\s)GL surface generation:\s*(\d+)(?:\s|$)")


@dataclass(frozen=True)
class Counters:
    resume: int
    pause: int
    surface: int


def _root(path: Path) -> ET.Element:
    try:
        return ET.parse(path).getroot()
    except (ET.ParseError, OSError) as error:
        raise ValueError(f"invalid uiautomator XML {path}: {error}") from error


def _bounds(node: ET.Element, label: str) -> tuple[int, int, int, int]:
    raw_bounds = node.attrib.get("bounds", "")
    match = BOUNDS_RE.fullmatch(raw_bounds)
    if match is None:
        raise ValueError(f"{label} has invalid bounds: {raw_bounds!r}")
    left, top, right, bottom = (int(value) for value in match.groups())
    if right <= left or bottom <= top:
        raise ValueError(f"{label} has empty bounds: {raw_bounds!r}")
    return left, top, right, bottom


def _normalized_label(value: str) -> str:
    # Android Button rendering commonly applies textAllCaps before exposing the
    # accessibility text. Match semantic label content rather than presentation
    # casing while preserving the exactly-one-control invariant below.
    return " ".join(value.split()).casefold()


def refresh_center(path: Path) -> tuple[int, int]:
    matches: list[tuple[int, int]] = []
    expected = _normalized_label(REFRESH_TEXT)
    for node in _root(path).iter("node"):
        text = _normalized_label(node.attrib.get("text", ""))
        content_desc = _normalized_label(node.attrib.get("content-desc", ""))
        if text != expected and content_desc != expected:
            continue
        left, top, right, bottom = _bounds(node, "refresh button")
        matches.append(((left + right) // 2, (top + bottom) // 2))
    if len(matches) != 1:
        raise ValueError(f"expected exactly one refresh button, found {len(matches)}")
    return matches[0]


def _scroll_node(path: Path) -> ET.Element:
    nodes = list(_root(path).iter("node"))
    explicit = [node for node in nodes if node.attrib.get("scrollable") == "true"]
    if len(explicit) == 1:
        return explicit[0]
    if len(explicit) > 1:
        raise ValueError(f"expected exactly one scrollable node, found {len(explicit)}")

    # Android's accessibility snapshot can expose a real ScrollView with
    # scrollable=false when descendants are clipped outside the current
    # viewport. The Java hierarchy still gives us an unambiguous class and
    # bounds, so use exactly one ScrollView as a fail-closed fallback.
    fallback = [node for node in nodes if node.attrib.get("class") == SCROLL_VIEW_CLASS]
    if len(fallback) != 1:
        raise ValueError(
            "expected exactly one scrollable node or one Android ScrollView fallback, "
            f"found {len(fallback)} fallbacks"
        )
    return fallback[0]


def scroll_swipe(path: Path) -> tuple[int, int, int, int]:
    node = _scroll_node(path)
    left, top, right, bottom = _bounds(node, "scrollable node")
    width = right - left
    height = bottom - top
    if height < 4:
        raise ValueError("scrollable node is too short for a bounded swipe")
    x = left + width // 2
    start_y = top + (height * 3) // 4
    end_y = top + height // 4
    return x, start_y, x, end_y


def renderer_counters(path: Path) -> Counters:
    candidates: list[Counters] = []
    for node in _root(path).iter("node"):
        text = node.attrib.get("text", "")
        if not text:
            continue
        resume = RESUME_RE.search(text)
        pause = PAUSE_RE.search(text)
        surface = SURFACE_RE.search(text)
        matched = sum(item is not None for item in (resume, pause, surface))
        if matched == 0:
            continue
        if matched != 3:
            raise ValueError("renderer diagnostics node contains only a partial lifecycle counter set")
        candidates.append(
            Counters(
                resume=int(resume.group(1)),
                pause=int(pause.group(1)),
                surface=int(surface.group(1)),
            )
        )
    if len(candidates) != 1:
        raise ValueError(f"expected exactly one renderer diagnostics node, found {len(candidates)}")
    return candidates[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    center_parser = subparsers.add_parser("refresh-center")
    center_parser.add_argument("xml", type=Path)

    swipe_parser = subparsers.add_parser("scroll-swipe")
    swipe_parser.add_argument("xml", type=Path)

    counters_parser = subparsers.add_parser("renderer-counters")
    counters_parser.add_argument("xml", type=Path)

    args = parser.parse_args()
    if args.command == "refresh-center":
        x, y = refresh_center(args.xml)
        print(f"{x} {y}")
        return 0
    if args.command == "scroll-swipe":
        start_x, start_y, end_x, end_y = scroll_swipe(args.xml)
        print(f"{start_x} {start_y} {end_x} {end_y}")
        return 0

    counters = renderer_counters(args.xml)
    print(f"{counters.resume} {counters.pause} {counters.surface}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
