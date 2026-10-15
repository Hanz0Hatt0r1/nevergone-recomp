#!/usr/bin/env python3
"""Static ARM64 ELF sanity audit for Never Gone Recomp APK/native libraries."""
from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

LOAD_RE = re.compile(r"^\s*LOAD\s+.*\s+(0x[0-9a-fA-F]+)\s*$")
NEEDED_RE = re.compile(r"\(NEEDED\).*Shared library: \[([^\]]+)\]")
MACHINE_RE = re.compile(r"^\s*Machine:\s*(.+?)\s*$", re.MULTILINE)
CLASS_RE = re.compile(r"^\s*Class:\s*(.+?)\s*$", re.MULTILINE)


def readelf(path: Path, *args: str) -> str:
    result = subprocess.run(["readelf", *args, str(path)], text=True, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(f"readelf {' '.join(args)} failed for {path}: {result.stderr.strip()}")
    return result.stdout


def inspect_elf(path: Path, display_name: str, min_alignment: int, forbidden_needed: set[str]) -> dict[str, object]:
    header = readelf(path, "-hW")
    program = readelf(path, "-lW")
    dynamic = readelf(path, "-dW")
    machine_match = MACHINE_RE.search(header)
    class_match = CLASS_RE.search(header)
    machine = machine_match.group(1) if machine_match else "unknown"
    elf_class = class_match.group(1) if class_match else "unknown"
    alignments = [int(match.group(1), 16) for line in program.splitlines() if (match := LOAD_RE.match(line))]
    needed = sorted(set(NEEDED_RE.findall(dynamic)))
    failures: list[str] = []
    if "AArch64" not in machine:
        failures.append(f"expected AArch64 machine, got {machine}")
    if "ELF64" not in elf_class:
        failures.append(f"expected ELF64 class, got {elf_class}")
    if not alignments:
        failures.append("no PT_LOAD segments parsed")
    for alignment in alignments:
        if alignment < min_alignment:
            failures.append(f"PT_LOAD alignment 0x{alignment:x} < 0x{min_alignment:x}")
        if alignment & (alignment - 1):
            failures.append(f"PT_LOAD alignment is not a power of two: 0x{alignment:x}")
    forbidden = sorted(set(needed) & forbidden_needed)
    if forbidden:
        failures.append("forbidden DT_NEEDED entries: " + ", ".join(forbidden))
    return {
        "name": display_name,
        "machine": machine,
        "class": elf_class,
        "load_alignments": [f"0x{alignment:x}" for alignment in alignments],
        "needed": needed,
        "forbidden_needed": forbidden,
        "status": "pass" if not failures else "fail",
        "failures": failures,
    }


def find_default_apk() -> Path | None:
    root = Path("android/app/build/outputs/apk/debug")
    if not root.exists():
        return None
    apks = sorted(root.rglob("*.apk"))
    return apks[0] if apks else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", type=Path)
    parser.add_argument("--so", type=Path, action="append", default=[])
    parser.add_argument("--out", type=Path, default=Path("evidence/arm64-sanity"))
    parser.add_argument("--min-load-alignment", type=lambda value: int(value, 0), default=0x4000)
    parser.add_argument("--forbid-needed", action="append", default=["libcocos2dcpp.so"])
    args = parser.parse_args()

    if shutil.which("readelf") is None:
        parser.error("readelf is required")
    apk = args.apk
    if apk is None and not args.so:
        apk = find_default_apk()
    if apk is None and not args.so:
        parser.error("no --apk/--so supplied and no debug APK was found")
    if apk is not None and not apk.is_file():
        parser.error(f"APK not found: {apk}")
    for so_path in args.so:
        if not so_path.is_file():
            parser.error(f"shared library not found: {so_path}")

    forbidden_needed = set(args.forbid_needed)
    rows: list[dict[str, object]] = []
    apk_arm64_entries: list[str] = []
    try:
        if apk is not None:
            with zipfile.ZipFile(apk) as archive, tempfile.TemporaryDirectory(prefix="nevergone-arm64-") as temp:
                temp_dir = Path(temp)
                apk_arm64_entries = sorted(
                    name for name in archive.namelist()
                    if name.startswith("lib/arm64-v8a/") and name.endswith(".so")
                )
                if not apk_arm64_entries:
                    rows.append({
                        "name": str(apk),
                        "status": "fail",
                        "failures": ["APK contains no lib/arm64-v8a/*.so entries"],
                    })
                for index, name in enumerate(apk_arm64_entries):
                    target = temp_dir / f"{index}-{Path(name).name}"
                    target.write_bytes(archive.read(name))
                    rows.append(inspect_elf(target, name, args.min_load_alignment, forbidden_needed))
        for so_path in args.so:
            rows.append(inspect_elf(so_path, str(so_path), args.min_load_alignment, forbidden_needed))
    except (OSError, RuntimeError, zipfile.BadZipFile) as error:
        print(f"arm64 sanity check failed: {error}", file=sys.stderr)
        return 2

    failures = [row for row in rows if row.get("status") != "pass"]
    result = {
        "status": "pass" if rows and not failures else "fail",
        "minimum_pt_load_alignment": f"0x{args.min_load_alignment:x}",
        "forbidden_needed": sorted(forbidden_needed),
        "apk": str(apk) if apk is not None else None,
        "apk_arm64_entries": apk_arm64_entries,
        "libraries": rows,
    }
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "sanity.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    report = [
        "# ARM64 static sanity",
        "",
        f"- status: **{result['status'].upper()}**",
        f"- minimum PT_LOAD alignment: `{result['minimum_pt_load_alignment']}`",
        f"- forbidden DT_NEEDED: {', '.join(f'`{name}`' for name in result['forbidden_needed'])}",
        f"- libraries checked: `{len(rows)}`",
    ]
    for row in rows:
        report.extend([
            "",
            f"## {row['name']}",
            f"- status: **{str(row.get('status', 'fail')).upper()}**",
            f"- machine: `{row.get('machine', 'n/a')}`",
            f"- class: `{row.get('class', 'n/a')}`",
            f"- PT_LOAD alignment: `{', '.join(row.get('load_alignments', [])) or 'n/a'}`",
            f"- DT_NEEDED: `{', '.join(row.get('needed', [])) or 'none'}`",
        ])
        report.extend(f"- failure: {failure}" for failure in row.get("failures", []))
    (args.out / "report.md").write_text("\n".join(report) + "\n", encoding="utf-8")

    print(f"arm64 libraries={len(rows)} failures={len(failures)} report={args.out / 'report.md'}")
    return 1 if failures or not rows else 0


if __name__ == "__main__":
    raise SystemExit(main())
