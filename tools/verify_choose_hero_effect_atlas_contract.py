#!/usr/bin/env python3
"""Validate and report ChooseHero PartThree effect-frame atlas metadata.

The tool reads only the checked-in TexturePacker metadata index. It deliberately
accepts either of the two recovered Gate_Background plists while reporting the
actual unique membership, so the first CI run can establish the exact clean-room
contract before Java staging is wired to a specific atlas.
"""

from __future__ import annotations

import csv
import gzip
import sys
from pathlib import Path

DEFAULT_INDEX = Path("docs/ghidra/full-index/atlas_frames.tsv.gz")
ATLAS_01 = "LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.plist"
ATLAS_02 = "LevelUI/Gate_Background_UI/Gate_BackgroundPNG_02.plist"
ALLOWED_ATLASES = (ATLAS_01, ATLAS_02)

EFFECT_FRAMES = tuple(
    [f"shandian{index:02d}.png" for index in range(1, 7)]
    + [f"menlei{index:02d}.png" for index in range(1, 7)]
)


def normalized_plist(value: str) -> str:
    return (value or "").replace("\\", "/")


def atlas_suffix(plist: str) -> str | None:
    for allowed in ALLOWED_ATLASES:
        if plist.endswith(allowed):
            return allowed
    return None


def main() -> int:
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_INDEX
    if not path.is_file():
        raise SystemExit(f"missing atlas metadata: {path}")

    matches: dict[str, list[dict[str, str]]] = {name: [] for name in EFFECT_FRAMES}
    with gzip.open(path, "rt", encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            name = row.get("frame", "")
            if name not in matches:
                continue
            plist = normalized_plist(row.get("plist", ""))
            if atlas_suffix(plist) is not None:
                matches[name].append(row)

    errors: list[str] = []
    for name in EFFECT_FRAMES:
        rows = matches[name]
        if len(rows) != 1:
            errors.append(
                f"{name}: expected exactly one row across Gate_Background atlases, found {len(rows)}"
            )
            continue
        row = rows[0]
        plist = normalized_plist(row.get("plist", ""))
        atlas = atlas_suffix(plist)
        print(
            "EFFECT "
            f"{name} atlas={atlas} "
            f"sprite_size={row.get('sprite_size', '')} "
            f"source_size={row.get('source_size', '')} "
            f"rotated={row.get('rotated', '')} "
            f"texture_rect={row.get('texture_rect', '')}"
        )

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print(f"ChooseHero effect atlas contract OK: {len(EFFECT_FRAMES)} unique frames")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
