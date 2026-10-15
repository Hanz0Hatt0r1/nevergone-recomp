#!/usr/bin/env python3
"""Check static Lua asset references against local decoded assets and optional APK inventory.

This is a conservative static inventory helper, not a runtime asset tracer. Lua
comments and obvious dynamic/log-only strings are reported separately so they do
not masquerade as missing files. Explicit requirements are supplied with
``--require``; no player ``.actData`` member is assumed to exist as a standalone
file because current device evidence shows those can live inside ``.actionsData``
containers.
"""
from __future__ import annotations

import argparse
import json
import re
import zipfile
from collections import defaultdict
from pathlib import Path, PurePosixPath

DEFAULT_EXTENSIONS = ("actData", "plist", "png", "hpc", "csv", "lua")
STATUS_ORDER = (
    "resolved-exact",
    "resolved-suffix",
    "dynamic-template",
    "non-asset-literal",
    "ambiguous",
    "missing",
)


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
    return re.compile(
        rf"(?P<quote>['\"])(?P<path>[^'\"\r\n]+\.(?:{ext_group}))(?P=quote)",
        re.IGNORECASE,
    )


def strip_lua_comments(text: str) -> str:
    """Blank Lua line/long comments while preserving quoted strings and newlines."""
    out: list[str] = []
    index = 0
    length = len(text)
    while index < length:
        ch = text[index]
        if ch in ("'", '"'):
            quote = ch
            out.append(ch)
            index += 1
            while index < length:
                current = text[index]
                out.append(current)
                index += 1
                if current == "\\" and index < length:
                    out.append(text[index])
                    index += 1
                elif current == quote:
                    break
            continue

        if text.startswith("--", index):
            long_match = re.match(r"--\[(=*)\[", text[index:])
            if long_match:
                equals = long_match.group(1)
                opener_len = long_match.end()
                closing = "]" + equals + "]"
                end = text.find(closing, index + opener_len)
                stop = length if end < 0 else end + len(closing)
                segment = text[index:stop]
                out.extend("\n" if c == "\n" else " " for c in segment)
                index = stop
                continue
            end = text.find("\n", index)
            if end < 0:
                out.extend(" " for _ in text[index:])
                break
            out.extend(" " for _ in text[index:end])
            out.append("\n")
            index = end + 1
            continue

        out.append(ch)
        index += 1
    return "".join(out)


def _line_context(text: str, start: int, end: int) -> tuple[int, str, str]:
    line_number = text.count("\n", 0, start) + 1
    line_start = text.rfind("\n", 0, start) + 1
    line_end = text.find("\n", end)
    if line_end < 0:
        line_end = len(text)
    line = text[line_start:line_end]
    prefix = text[line_start:start]
    return line_number, line, prefix


def collect_references(decoded_dir: Path, extensions: tuple[str, ...]) -> list[dict[str, object]]:
    pattern = quoted_asset_pattern(extensions)
    rows: list[dict[str, object]] = []
    for lua_path in sorted(decoded_dir.rglob("*.lua")):
        if not lua_path.is_file():
            continue
        try:
            raw_text = lua_path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            raise RuntimeError(f"cannot read {lua_path}: {error}") from error
        text = strip_lua_comments(raw_text)
        source = lua_path.relative_to(decoded_dir).as_posix()
        for match in pattern.finditer(text):
            raw_reference = match.group("path").replace("\\", "/").strip()
            line_number, _line, prefix = _line_context(text, match.start(), match.end())
            if "%" in raw_reference or "?" in raw_reference:
                rows.append({
                    "source": source,
                    "line": line_number,
                    "reference": raw_reference,
                    "status": "dynamic-template",
                    "matches": [],
                })
                continue
            if re.search(r"\bprint\s*\([^)]*$", prefix):
                rows.append({
                    "source": source,
                    "line": line_number,
                    "reference": raw_reference,
                    "status": "non-asset-literal",
                    "matches": [],
                })
                continue
            reference = normalize_reference(raw_reference)
            if reference is not None:
                rows.append({"source": source, "line": line_number, "reference": reference})
    return rows


def _inventory_variants(relative: str) -> set[str]:
    normalized = relative.replace("\\", "/").lstrip("/")
    variants = {normalized} if normalized else set()
    if normalized.startswith("assets/"):
        variants.add(normalized[len("assets/"):])
    return variants


def inventory(decoded_dir: Path, apk: Path | None) -> tuple[set[str], dict[str, list[str]], dict[str, int]]:
    paths: set[str] = set()
    by_suffix: dict[str, list[str]] = defaultdict(list)
    counts = {"decoded_files": 0, "apk_asset_files": 0}

    def add(relative: str) -> None:
        for variant in _inventory_variants(relative):
            if not variant or variant in paths:
                continue
            paths.add(variant)
            parts = PurePosixPath(variant).parts
            for index in range(len(parts)):
                by_suffix["/".join(parts[index:])].append(variant)

    for path in sorted(decoded_dir.rglob("*")):
        if not path.is_file():
            continue
        counts["decoded_files"] += 1
        add(path.relative_to(decoded_dir).as_posix())

    if apk is not None:
        if not apk.is_file() or not zipfile.is_zipfile(apk):
            raise ValueError(f"APK inventory source is not a ZIP-compatible file: {apk}")
        with zipfile.ZipFile(apk) as archive:
            for info in archive.infolist():
                if info.is_dir() or not info.filename.startswith("assets/"):
                    continue
                counts["apk_asset_files"] += 1
                add(info.filename)

    return paths, by_suffix, counts


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
    parser.add_argument("--apk", type=Path, help="optional original APK; adds raw assets/ entries to inventory")
    parser.add_argument("--out", type=Path, default=Path("evidence/asset-coverage"))
    parser.add_argument("--extensions", default=",".join(DEFAULT_EXTENSIONS))
    parser.add_argument(
        "--require",
        action="append",
        default=[],
        metavar="ASSET",
        help="Explicit required standalone asset path or basename (repeatable).",
    )
    parser.add_argument(
        "--no-default-required",
        action="store_true",
        help="Deprecated compatibility option; no default standalone requirements are injected.",
    )
    parser.add_argument("--fail-on-missing", action="store_true")
    parser.add_argument("--fail-on-ambiguous", action="store_true")
    args = parser.parse_args()

    if not args.decoded_dir.is_dir():
        parser.error(f"decoded asset directory does not exist: {args.decoded_dir}")
    extensions = tuple(part.strip().lstrip(".") for part in args.extensions.split(",") if part.strip())
    if not extensions:
        parser.error("no extensions selected")

    references = collect_references(args.decoded_dir, extensions)
    try:
        exact_paths, by_suffix, inventory_counts = inventory(args.decoded_dir, args.apk)
    except ValueError as error:
        parser.error(str(error))

    dedup: dict[tuple[str, int, str], dict[str, object]] = {}
    for row in references:
        source = str(row["source"])
        line = int(row["line"])
        reference = str(row["reference"])
        key = (source, line, reference)
        if key in dedup:
            continue
        if "status" in row:
            dedup[key] = row
            continue
        status, matches = resolve(reference, exact_paths, by_suffix)
        dedup[key] = {**row, "status": status, "matches": matches}

    for raw_reference in args.require:
        reference = normalize_reference(raw_reference)
        if reference is None:
            parser.error(f"invalid required standalone asset reference: {raw_reference!r}")
        key = ("<required>", 0, reference)
        if key in dedup:
            continue
        status, matches = resolve(reference, exact_paths, by_suffix)
        dedup[key] = {
            "source": "<required>",
            "line": 0,
            "reference": reference,
            "status": status,
            "matches": matches,
        }

    rows = sorted(
        dedup.values(),
        key=lambda row: (str(row["status"]), str(row["source"]), int(row["line"]), str(row["reference"])),
    )
    counts = {status: sum(1 for row in rows if row["status"] == status) for status in STATUS_ORDER}
    result = {
        "decoded_dir": str(args.decoded_dir),
        "apk": str(args.apk) if args.apk is not None else None,
        "extensions": list(extensions),
        "explicit_requirements": list(args.require),
        "inventory": inventory_counts,
        "inventory_paths": len(exact_paths),
        "reference_edges": len(rows),
        "counts": counts,
        "rows": rows,
    }
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "coverage.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    missing = [row for row in rows if row["status"] == "missing"]
    ambiguous = [row for row in rows if row["status"] == "ambiguous"]
    dynamic = [row for row in rows if row["status"] == "dynamic-template"]
    non_asset = [row for row in rows if row["status"] == "non-asset-literal"]
    report = [
        "# Static Lua asset coverage",
        "",
        f"- decoded files inventoried: `{inventory_counts['decoded_files']}`",
        f"- APK assets inventoried: `{inventory_counts['apk_asset_files']}`",
        f"- unique source/reference edges: `{len(rows)}`",
        f"- explicit standalone requirements: `{len(args.require)}`",
        f"- exact resolutions: `{counts['resolved-exact']}`",
        f"- unique suffix resolutions: `{counts['resolved-suffix']}`",
        f"- dynamic templates ignored as file requirements: `{len(dynamic)}`",
        f"- non-asset literals ignored as file requirements: `{len(non_asset)}`",
        f"- ambiguous references: `{len(ambiguous)}`",
        f"- unresolved literal references: `{len(missing)}`",
        "",
        "This is a static inventory check, not a runtime asset tracer. An unresolved literal means it was not found in the supplied decoded/APK inventories; it does not prove the runtime lacks it because writable/downloaded data may provide it.",
        "",
        "Player `.actData` names must not be assumed to be standalone files. Current device evidence shows player actions can be embedded members of `player_NN.actionsData`; use the player-action/runtime probes for that boundary.",
    ]
    if missing:
        report.extend(["", "## Unresolved literals", ""] + [
            f"- `{row['source']}:{row['line']}` -> `{row['reference']}`" for row in missing[:100]
        ])
    if ambiguous:
        report.extend(["", "## Ambiguous", ""] + [
            f"- `{row['source']}:{row['line']}` -> `{row['reference']}` -> "
            + ", ".join(f"`{match}`" for match in row["matches"])
            for row in ambiguous[:100]
        ])
    (args.out / "report.md").write_text("\n".join(report) + "\n", encoding="utf-8")

    print(
        f"asset references={len(rows)} missing={len(missing)} ambiguous={len(ambiguous)} "
        f"dynamic={len(dynamic)} non_asset={len(non_asset)} report={args.out / 'report.md'}"
    )
    if args.fail_on_missing and missing:
        return 1
    if args.fail_on_ambiguous and ambiguous:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
