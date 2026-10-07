#!/usr/bin/env python3
"""Compare native methods declared in DEX with JNI symbols exported by ELF files.

The tool intentionally checks the static Java_* JNI convention only. Native
methods missing from the export table are candidates for RegisterNatives,
missing vendor libraries, dead code, or broken integrations and therefore need
manual investigation.
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from dataclasses import asdict
from pathlib import Path

from dex_native_map import DexReader, NativeMethod


def read_java_exports(elf: Path) -> set[str]:
    if not shutil.which("readelf"):
        raise SystemExit("readelf is required")
    text = subprocess.check_output(
        ["readelf", "-Ws", str(elf)],
        text=True,
        errors="replace",
        stderr=subprocess.STDOUT,
    )
    result: set[str] = set()
    for line in text.splitlines():
        parts = line.split()
        if not parts:
            continue
        symbol = parts[-1]
        if symbol.startswith("Java_"):
            result.add(symbol)
    return result


def matches(method: NativeMethod, exports: set[str]) -> list[str]:
    prefix = method.expected_jni_prefix
    # Overloaded JNI methods may append a mangled argument signature after __.
    return sorted(
        symbol for symbol in exports
        if symbol == prefix or symbol.startswith(prefix + "__")
    )


def analyze(dex: Path, elves: list[Path]) -> dict:
    methods = DexReader(dex.read_bytes()).native_methods()
    methods.sort(key=lambda value: (value.java_class, value.name))

    by_elf = {str(elf): sorted(read_java_exports(elf)) for elf in elves}
    all_exports: set[str] = set()
    for symbols in by_elf.values():
        all_exports.update(symbols)

    rows = []
    matched_symbols: set[str] = set()
    for method in methods:
        found = matches(method, all_exports)
        matched_symbols.update(found)
        row = asdict(method)
        row.update(
            {
                "java_class": method.java_class,
                "expected_jni_prefix": method.expected_jni_prefix,
                "status": "matched" if found else "missing-static-export",
                "matching_exports": found,
            }
        )
        rows.append(row)

    return {
        "dex": str(dex),
        "elf_exports": by_elf,
        "summary": {
            "declared_native_methods": len(methods),
            "java_exports": len(all_exports),
            "matched_methods": sum(row["status"] == "matched" for row in rows),
            "missing_static_exports": sum(
                row["status"] == "missing-static-export" for row in rows
            ),
            "unmatched_java_exports": len(all_exports - matched_symbols),
        },
        "methods": rows,
        "unmatched_java_exports": sorted(all_exports - matched_symbols),
    }


def markdown(report: dict) -> str:
    summary = report["summary"]
    out = [
        "# JNI cross-check",
        "",
        f"- DEX native declarations: **{summary['declared_native_methods']}**",
        f"- ELF `Java_*` exports: **{summary['java_exports']}**",
        f"- Matched declarations: **{summary['matched_methods']}**",
        f"- Declarations without static export: **{summary['missing_static_exports']}**",
        f"- Exports without a matching DEX declaration: **{summary['unmatched_java_exports']}**",
        "",
        "| Java class | Method | Status | Matching export |",
        "| --- | --- | --- | --- |",
    ]
    for row in report["methods"]:
        symbols = ", ".join(f"`{value}`" for value in row["matching_exports"]) or "—"
        out.append(
            f"| `{row['java_class']}` | `{row['name']}` | "
            f"{row['status']} | {symbols} |"
        )

    if report["unmatched_java_exports"]:
        out += ["", "## ELF exports without matching DEX declaration", ""]
        out.extend(f"- `{value}`" for value in report["unmatched_java_exports"])

    out += [
        "",
        "> A missing static export is not proof that a method is unimplemented. "
        "It may be registered with `RegisterNatives`. Investigate those entries "
        "before classifying them as broken or dead code.",
    ]
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dex", type=Path, help="path to classes.dex")
    parser.add_argument("elf", type=Path, nargs="+", help="one or more native .so files")
    parser.add_argument("--json", dest="json_path", type=Path)
    parser.add_argument("--markdown", dest="markdown_path", type=Path)
    parser.add_argument(
        "--fail-on-missing",
        action="store_true",
        help="return exit code 1 when a DEX native declaration has no static Java_* export",
    )
    args = parser.parse_args()

    report = analyze(args.dex, args.elf)

    if args.json_path:
        args.json_path.parent.mkdir(parents=True, exist_ok=True)
        args.json_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.markdown_path:
        args.markdown_path.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_path.write_text(markdown(report), encoding="utf-8")
    if not args.json_path and not args.markdown_path:
        print(markdown(report), end="")

    if args.fail_on_missing and report["summary"]["missing_static_exports"]:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
