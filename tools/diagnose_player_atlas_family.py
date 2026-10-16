#!/usr/bin/env python3
"""Diagnose Never Gone player action-atlas vs createActions preload-family coverage.

This tool reads a user-owned original OBB without extracting or modifying it. It
answers one narrow reconstruction question: when an action document names a
logical player plist such as ``p01_02_default.plist``, do the recovered
``MEPlayer::createActions`` preload plists contain the requested frame as an
actual SpriteFrameCache key (or alias), rather than merely as matching bytes?
"""

from __future__ import annotations

import argparse
import plistlib
import re
import sys
import zipfile
from pathlib import Path
from typing import Any
from xml.parsers.expat import ExpatError

MAX_PLIST_BYTES = 4 * 1024 * 1024
GENERIC_RE = re.compile(r"^p(?P<player>\d+)_(?P<selector>\d+)_default\.plist$", re.IGNORECASE)
TEXTURE_RE = re.compile(
    rb"<key>(?:textureFileName|realTextureFileName)</key>\s*<string>([^<]+)</string>",
    re.IGNORECASE,
)
LEGACY_PLIST_DTD = (
    b'<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" '
    b'"http://www.apple.com/DTDs/PropertyList-1.0.dtd"/>'
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


def plist_membership(payload: bytes, wanted_frame: str) -> tuple[bool, list[str], list[str], int, Any]:
    # The shipped OBB contains this one malformed Apple DTD terminator.
    # Normalize it before plistlib parses the actual frame dictionary.
    payload = payload.replace(LEGACY_PLIST_DTD, LEGACY_PLIST_DTD[:-2] + b'>', 1)
    root = plistlib.loads(payload)
    if not isinstance(root, dict):
        raise ValueError("plist root is not a dict")
    frames = root.get("frames")
    if not isinstance(frames, dict):
        raise ValueError("frames is not a dict")

    keys = [key for key in frames.keys() if isinstance(key, str)]
    exact = wanted_frame in frames
    casefold_matches = [key for key in keys if key.casefold() == wanted_frame.casefold()]

    aliases: list[str] = []
    for canonical, frame in frames.items():
        if not isinstance(canonical, str) or not isinstance(frame, dict):
            continue
        raw_aliases = frame.get("aliases")
        if not isinstance(raw_aliases, list):
            continue
        for alias in raw_aliases:
            if isinstance(alias, str) and alias == wanted_frame:
                aliases.append(canonical)

    metadata = root.get("metadata")
    fmt: Any = None
    if isinstance(metadata, dict):
        fmt = metadata.get("format")
    return exact, casefold_matches, aliases, len(keys), fmt


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

        byte_hits: list[str] = []
        exact_key_hits: list[str] = []
        alias_hits: list[tuple[str, str]] = []
        parse_failures: list[str] = []
        for entry in family_entries:
            try:
                payload = read_bounded(zf, entry)
                exact, casefold_matches, aliases, frame_count, fmt = plist_membership(payload, args.frame)
            except (KeyError, ValueError, OSError, ExpatError, plistlib.InvalidFileException) as error:
                parse_failures.append(f"{entry}: {error}")
                continue

            texture_match = TEXTURE_RE.search(payload)
            texture = (
                texture_match.group(1).decode("utf-8", "replace").strip()
                if texture_match
                else "<implicit .png>"
            )
            contains = frame_bytes in payload
            print(
                f"family: {entry} bytes={'yes' if contains else 'no'} "
                f"frame_key={'yes' if exact else 'no'} frames={frame_count} "
                f"format={fmt!r} texture={texture}"
            )
            if casefold_matches and not exact:
                print(f"  casefold_key_matches: {casefold_matches!r}")
            if aliases:
                print(f"  alias_targets: {aliases!r}")

            if contains:
                byte_hits.append(entry)
            if exact:
                exact_key_hits.append(entry)
            for canonical in aliases:
                alias_hits.append((entry, canonical))

        for error in parse_failures:
            print(f"parse_failure: {error}")

        print(f"family_resolved={len(family_entries)}/9")
        print(f"byte_hits={len(byte_hits)}")
        print(f"exact_frame_key_hits={len(exact_key_hits)}")
        print(f"alias_hits={len(alias_hits)}")
        for entry in exact_key_hits:
            print(f"exact_frame_key_hit: {entry}")
        for entry, canonical in alias_hits:
            print(f"alias_hit: {entry} -> {canonical}")

        # Keep the original strict evidence exit contract: zero means the full
        # recovered family is present and the requested frame has deterministic
        # exact/alias membership. Missing family members remain diagnostically
        # visible even when the particular requested frame is already resolved.
        if parse_failures or missing_family or len(family_entries) != 9:
            return 1
        if len(exact_key_hits) + len(alias_hits) != 1:
            return 1
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
