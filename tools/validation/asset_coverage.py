#!/usr/bin/env python3
"""Check static Lua asset references against a locally decoded original asset tree.

The report is metadata-only: it records referenced paths and resolution status but
never copies decoded Lua or proprietary asset bytes into the repository.
"""
from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path, PurePosixPath

DEFAULT_EXTENSIONS = ("actData", "plist", "png", "hpc", "csv", "lua")


def normalize_reference(value: str) -> str | None:
    value = value.replace("\\", "/").strip()
    while value.startswith("./"):
        value = value[2:]
    value = value.lstrip("/")
    if not value or "\x00" in value:
        return None
    path = PurePosixPath(value)
    if ".." in path.parts:
        return None
    return path.as_posix()


def quoted_asset_pattern(extensions: tuple[str, ...]) -> re.Pattern[str]:
    ext_group = "|".join(re.escape(extension) for extension in extensions)
    return re.compile(rf"(?P<quote>['\"])(?P<path>[^'\"\r\n]+\.(?:{ext_group}))(?P=quote)", re.IGNORECASE)


def collect_references(decoded_dir: Path, extensions: tuple[str, ...]) -> list[dict[str, str]]:
    pattern = quoted_asset_pattern(extensions)
    rows: list[dict[str, str]] = []
    for lua_path in sorted(decoded_dir.rglob("*.lua")):
        if not lua_path.is_file():
            continue
        try:
            text = lua_path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            raise RuntimeError(f"cannot read {lua_path}: {error}") from error
        source = lua_path.relative_to(decoded_dir).as_posix()
        for match in pattern.finditer(text):
            reference = normalize_reference(match.group("path"))
            if reference is not None:
                rows.append({"source": source, "reference": reference})
    return rows


def inventory(decoded_dir: Path) -> tuple[set[str], dict[str, list[str]]]:
    paths: set[str] = set()
    by_suffix: dict[str, list[str]] = defaultdict(list)
    for path in sorted(decoded_dir.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(decoded_dir).as_posix()
        paths.add(relative)
        parts = PurePosixPath(relative).parts
        for index in range(len(parts)):
            by_suffix["/".join(parts[index:])].append(relative)
    return paths, by_suffix


def resolve(reference: str, exact_paths: set[str], by_suffix: dict[str, list[str]]) -> tuple[str, list[str]]:
    candidates = [reference]
    if reference.startswith("assets/"):
        candidates.append(reference[len("assets/"):])
    for candidate in candidates:
        if candidate in exact_paths:
            return "resolved-exact", [candidate]
    matches: set[str] = set()
    for candidate in candidates:
        matches.update(by_suffix.get(candidate, []))
    if len(matches) == 1:
        return "resolved-suffix", sorted(matches)
    if len(matches) > 1:
        return "ambiguous", sorted(matches)
    return "missing", []


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decoded-dir", type=Path, default=Path("build/decoded-assets"))
    parser.add_argument("--out", type=Path, default=Path("evidence/asset-coverage"))
    parser.add_argument("--extensions", default=",".join(DEFAULT_EXTENSIONS))
    parser.add_argument("--fail-on-missing", action="store_true")
    parser.add_argument("--fail-on-ambiguous", action="store_true")
    args = parser.parse_args()

    if not args.decoded_dir.is_dir():
        parser.error(f"decoded asset directory does not exist: {args.decoded_dir}")
    extensions = tuple(part.strip().lstrip(".") for part in args.extensions.split(",") if part.strip())
    if not extensions:
        parser.error("no extensions selected")

    references = collect_references(args.decoded_dir, extensions)
    exact_paths, by_suffix = inventory(args.decoded_dir)
    dedup: dict[tuple[str, str], dict[str, object]] = {}
    for row in references:
        key = (row["source"], row["reference"])
        if key in dedup:
            continue
        status, matches = resolve(row["reference"], exact_paths, by_suffix)
        dedup[key] = {**row, "status": status, "matches": matches}
    rows = sorted(dedup.values(), key=lambda row: (str(row["status"]), str(row["source"]), str(row["reference"])))

    counts = {status: sum(1 for row in rows if row["status"] == status) for status in (
        "resolved-exact", "resolved-suffix", "ambiguous", "missing"
    )}
    result = {
        "decoded_dir": str(args.decoded_dir),
        "extensions": list(extensions),
        "inventory_files": len(exact_paths),
        "reference_edges": len(rows),
        "counts": counts,
        "rows": rows,
    }
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "coverage.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    missing = [row for row in rows if row["status"] == "missing"]
    ambiguous = [row for row in rows if row["status"] == "ambiguous"]
    report = [
        "# Static Lua asset coverage",
        "",
        f"- decoded files inventoried: `{len(exact_paths)}`",
        f"- unique source/reference edges: `{len(rows)}`",
        f"- exact resolutions: `{counts['resolved-exact']}`",
        f"- unique suffix resolutions: `{counts['resolved-suffix']}`",
        f"- ambiguous references: `{counts['ambiguous']}`",
        f"- missing references: `{counts['missing']}`",
        "",
        "This is a static lower bound over quoted Lua references, not a full runtime asset tracer.",
    ]
    if missing:
        report.extend(["", "## Missing", ""] + [f"- `{row['source']}` -> `{row['reference']}`" for row in missing[:100]])
    if ambiguous:
        report.extend(["", "## Ambiguous", ""] + [
            f"- `{row['source']}` -> `{row['reference']}` -> {', '.join(f'`{m}`' for m in row['matches'])}"
            for row in ambiguous[:100]
        ])
    (args.out / "report.md").write_text("\n".join(report) + "\n", encoding="utf-8")

    print(
        f"asset references={len(rows)} missing={counts['missing']} ambiguous={counts['ambiguous']} "
        f"report={args.out / 'report.md'}"
    )
    if args.fail_on_missing and missing:
        return 1
    if args.fail_on_ambiguous and ambiguous:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
