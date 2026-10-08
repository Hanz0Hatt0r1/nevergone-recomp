#!/usr/bin/env python3
"""Validate recovered ChooseHeroBackground atlas metadata used by the runtime.

This consumes only the checked-in TexturePacker metadata index. It does not
read asset bytes or proprietary archive contents.
"""

from __future__ import annotations

import csv
import gzip
import re
import sys
from pathlib import Path

DEFAULT_INDEX = Path("docs/ghidra/full-index/atlas_frames.tsv.gz")
DESIGN_SIZE = (1136, 640)

ATLAS_01 = "LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.plist"
ATLAS_02 = "LevelUI/Gate_Background_UI/Gate_BackgroundPNG_02.plist"

EXPECTED = {
    "yueliang.png": ATLAS_01,
    "bejingyueliang.png": ATLAS_01,
    "bejingyueliang01.png": ATLAS_01,
    "diguang.png": ATLAS_01,
    "yueliangzhezhao.png": ATLAS_02,
    "xingkong.png": ATLAS_02,
    "bejingwuyun.png": ATLAS_02,
    "qianjingyun01.png": ATLAS_02,
    "qianjingyun02.png": ATLAS_02,
    "qianjingyun03.png": ATLAS_02,
}

EFFECT_FRAMES = tuple(
    [f"shandian{index:02d}.png" for index in range(1, 7)]
    + [f"menlei{index:02d}.png" for index in range(1, 7)]
)

DESIGN_CANVAS_FRAMES = {
    "yueliang.png",
    "yueliangzhezhao.png",
    "xingkong.png",
    "bejingyueliang.png",
    "bejingyueliang01.png",
    "bejingwuyun.png",
    "diguang.png",
}

FULL_CANVAS_FRAMES = {"bejingyueliang.png", "bejingwuyun.png"}


def numbers(value: str) -> tuple[int, ...]:
    return tuple(int(item) for item in re.findall(r"-?\d+", value or ""))


def size(value: str) -> tuple[int, int] | None:
    found = numbers(value)
    return (found[0], found[1]) if len(found) >= 2 else None


def rect(value: str) -> tuple[int, int, int, int] | None:
    found = numbers(value)
    return tuple(found[:4]) if len(found) >= 4 else None


def is_false(value: str) -> bool:
    return (value or "").strip().casefold() in {"false", "0", "no"}


def gate_atlas_suffix(plist: str) -> str | None:
    normalized = (plist or "").replace("\\", "/")
    for atlas in (ATLAS_01, ATLAS_02):
        if normalized.endswith(atlas):
            return atlas
    return None


def main() -> int:
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_INDEX
    if not path.is_file():
        raise SystemExit(f"missing atlas metadata: {path}")

    matches: dict[str, list[dict[str, str]]] = {name: [] for name in EXPECTED}
    effect_matches: dict[str, list[dict[str, str]]] = {
        name: [] for name in EFFECT_FRAMES
    }
    with gzip.open(path, "rt", encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            name = row.get("frame", "")
            expected_plist = EXPECTED.get(name)
            if expected_plist is not None:
                plist = row.get("plist", "").replace("\\", "/")
                if plist.endswith(expected_plist):
                    matches[name].append(row)
            if name in effect_matches and gate_atlas_suffix(row.get("plist", "")) is not None:
                effect_matches[name].append(row)

    errors: list[str] = []
    for name, expected_plist in EXPECTED.items():
        rows = matches[name]
        if len(rows) != 1:
            errors.append(
                f"{name}: expected exactly one row in {expected_plist}, found {len(rows)}"
            )
            continue

        row = rows[0]
        if name in DESIGN_CANVAS_FRAMES and size(row.get("source_size", "")) != DESIGN_SIZE:
            errors.append(
                f"{name}: source_size={row.get('source_size', '')!r}, expected 1136x640"
            )

        if name in FULL_CANVAS_FRAMES:
            if size(row.get("sprite_size", "")) != DESIGN_SIZE:
                errors.append(
                    f"{name}: sprite_size={row.get('sprite_size', '')!r}, expected 1136x640"
                )
            if rect(row.get("texture_rect", "")) != (0, 0, 1136, 640):
                errors.append(
                    f"{name}: texture_rect={row.get('texture_rect', '')!r}, expected {{0,0,1136,640}}"
                )
            if not is_false(row.get("rotated", "")):
                errors.append(f"{name}: expected non-rotated full-canvas frame")

    for name in EFFECT_FRAMES:
        rows = effect_matches[name]
        if len(rows) != 1:
            errors.append(
                f"{name}: expected exactly one row across Gate_Background atlases, found {len(rows)}"
            )
            continue
        row = rows[0]
        print(
            "ChooseHero effect metadata: "
            f"{name} atlas={gate_atlas_suffix(row.get('plist', ''))} "
            f"sprite_size={row.get('sprite_size', '')} "
            f"source_size={row.get('source_size', '')} "
            f"rotated={row.get('rotated', '')} "
            f"texture_rect={row.get('texture_rect', '')}"
        )

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print(
        "ChooseHero background atlas contract OK: "
        f"{len(EXPECTED)} staged frames + {len(EFFECT_FRAMES)} effect frames, "
        f"design canvas {DESIGN_SIZE[0]}x{DESIGN_SIZE[1]}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
