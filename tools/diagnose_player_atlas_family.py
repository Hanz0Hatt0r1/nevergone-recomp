#!/usr/bin/env python3
"""Diagnose Never Gone player action-atlas vs createActions preload-family coverage.

This tool reads a user-owned original OBB without extracting or modifying it. It
answers one narrow reconstruction question: when an action document names a
logical player plist such as ``p01_02_default.plist``, do the nine recovered
``MEPlayer::createActions`` preload plists contain the requested frame already?
"""

from __future__ import annotations

import argparse
import re
import sys
import zipfile
from pathlib import Path

MAX_PLIST_BYTES = 4 * 1024 * 1024
GENERIC_RE = re.compile(r"^p(?P<player>\d+)_(?P<selector>\d+)_default\.plist$", re.IGNORECASE)
TEXTURE_RE = re.compile(
    rb"<key>(?:textureFileName|realTextureFileName)</key>\s*<string>([^<]+)</string>",
    re.IGNORECASE,
)


def basename(name: str) -> str:
    return name.rsplit("/", 1)[-1]


def find_entries(names: list[str], wanted_basename: str) -> list[str]:
    wanted = wanted_basename.casefold()
    return [name for name in names if basename(name).casefold() == wanted]


def read_bounded(zf: zipfile.ZipFile, entry: str) -> bytes:
    info = zf.getinfo(entry)
    if info.file_size <= 0 or info.file_size > MAX_PLIST_BYTES:
        raise ValueError(f"unsafe plist size for {entry}: {info.file_size}")
    return zf.read(entry)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("obb", type=Path)
    parser.add_argument("--action-plist", default="p01_02_default.plist")
    parser.add_argument("--frame", default="P01_I01_001.png")
    args = parser.parse_args()

    match = GENERIC_RE.fullmatch(args.action_plist)
    if match is None:
        print(f"unsupported action plist shape: {args.action_plist}", file=sys.stderr)
        return 2
    if not args.obb.is_file():
        print(f"OBB not found: {args.obb}", file=sys.stderr)
        return 2

    player = match.group("player")
    selector = match.group("selector")
    frame_bytes = args.frame.encode("utf-8")

    with zipfile.ZipFile(args.obb) as zf:
        names = zf.namelist()
        generic = find_entries(names, args.action_plist)
        print(f"action_plist={args.action_plist}")
        print(f"frame={args.frame}")
        print(f"generic_entries={len(generic)}")
        for entry in generic:
            print(f"generic: {entry}")

        family_entries: list[str] = []
        missing_family: list[str] = []
        for index in range(1, 10):
            family_name = f"p{player}_{selector}_{index:02d}_default.plist"
            hits = find_entries(names, family_name)
            if len(hits) == 1:
                family_entries.append(hits[0])
            elif not hits:
                missing_family.append(family_name)
                print(f"family[{index:02d}]: MISSING {family_name}")
            else:
                print(f"family[{index:02d}]: AMBIGUOUS {family_name} ({len(hits)} entries)")
                for hit in hits:
                    print(f"  {hit}")

        frame_hits: list[str] = []
        parse_failures: list[str] = []
        for entry in family_entries:
            try:
                payload = read_bounded(zf, entry)
            except (KeyError, ValueError, OSError) as error:
                parse_failures.append(f"{entry}: {error}")
                continue
            texture_match = TEXTURE_RE.search(payload)
            texture = texture_match.group(1).decode("utf-8", "replace").strip() if texture_match else "<implicit .png>"
            contains = frame_bytes in payload
            print(f"family: {entry} frame={'yes' if contains else 'no'} texture={texture}")
            if contains:
                frame_hits.append(entry)

        for error in parse_failures:
            print(f"parse_failure: {error}")

        print(f"family_resolved={len(family_entries)}/9")
        print(f"frame_hits={len(frame_hits)}")
        for entry in frame_hits:
            print(f"frame_hit: {entry}")

        # Exit status is intentionally evidence-oriented rather than treating a
        # missing generic plist as an error. 0 means the recovered preload family
        # is complete and the requested frame has deterministic membership.
        if parse_failures or missing_family or len(family_entries) != 9:
            return 1
        if len(frame_hits) != 1:
            return 1
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
