#!/usr/bin/env python3
"""Generate a focused reverse-engineering report for a native Android ELF.

Requires GNU/LLVM `readelf`, `nm`, `c++filt` and optionally `strings` on PATH.
The output is intended to separate stock engine symbols from likely game code
and to preserve addresses/names for later Ghidra work.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path
from typing import Any

ENGINE_PREFIXES = (
    "cocos2d::",
    "std::",
    "__gnu_cxx::",
    "google::",
    "boost::",
)


def run(args: list[str]) -> str:
    return subprocess.check_output(args, text=True, errors="replace", stderr=subprocess.STDOUT)


def require_tool(name: str) -> None:
    if not shutil.which(name):
        raise SystemExit(f"required tool not found on PATH: {name}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def demangled_symbols(path: Path) -> list[dict[str, Any]]:
    raw = run(["nm", "-D", "--defined-only", str(path)])
    lines = [line for line in raw.splitlines() if line.strip()]
    names = []
    metadata = []
    for line in lines:
        parts = line.split(maxsplit=2)
        if len(parts) != 3:
            continue
        address, kind, mangled = parts
        names.append(mangled)
        metadata.append((address, kind, mangled))

    proc = subprocess.run(
        ["c++filt"],
        input="\n".join(names) + ("\n" if names else ""),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    demangled = proc.stdout.splitlines()
    result = []
    for (address, kind, mangled), display in zip(metadata, demangled):
        result.append(
            {
                "address": f"0x{address}",
                "kind": kind,
                "mangled": mangled,
                "name": display,
            }
        )
    return result


def is_engine_name(name: str) -> bool:
    normalized = name
    for prefix in ("typeinfo for ", "typeinfo name for ", "vtable for "):
        if normalized.startswith(prefix):
            normalized = normalized[len(prefix):]
            break
    return normalized.startswith(ENGINE_PREFIXES)


def analyze(path: Path) -> dict[str, Any]:
    for tool in ("readelf", "nm", "c++filt"):
        require_tool(tool)

    header = run(["readelf", "-h", str(path)])
    dynamic = run(["readelf", "-d", str(path)])
    symbol_table = run(["readelf", "-Ws", str(path)])
    symbols = demangled_symbols(path)

    result: dict[str, Any] = {
        "path": str(path),
        "sha256": sha256(path),
        "size": path.stat().st_size,
        "needed": re.findall(r"\(NEEDED\).*?\[(.*?)\]", dynamic),
        "jni_exports": [],
        "engine_version_strings": [],
        "startup_symbols": [],
        "game_rtti_types": [],
    }

    for key in ("Class", "Data", "Type", "Machine"):
        match = re.search(rf"^\s*{re.escape(key)}:\s*(.+)$", header, re.MULTILINE)
        if match:
            result[key.lower()] = match.group(1).strip()

    exports = set()
    for line in symbol_table.splitlines():
        parts = line.split()
        if not parts:
            continue
        name = parts[-1]
        if name == "JNI_OnLoad" or name.startswith("Java_"):
            exports.add(name)
    result["jni_exports"] = sorted(exports)

    startup_patterns = (
        "AppDelegate::",
        "applicationDidFinishLaunching",
        "applicationDidEnterBackground",
        "applicationWillEnterForeground",
        "JNI_OnLoad",
    )
    result["startup_symbols"] = [
        symbol for symbol in symbols if any(token in symbol["name"] for token in startup_patterns)
    ]

    rtti = set()
    for symbol in symbols:
        name = symbol["name"]
        if not name.startswith("typeinfo for "):
            continue
        value = name[len("typeinfo for "):]
        if not is_engine_name(value):
            rtti.add(value)
    result["game_rtti_types"] = sorted(rtti)

    if shutil.which("strings"):
        text = run(["strings", str(path)])
        candidates = []
        for line in text.splitlines():
            lower = line.lower()
            if "cocos2d-" in lower or line.startswith("$LuaVersion:"):
                candidates.append(line)
        result["engine_version_strings"] = sorted(set(candidates))

    return result


def markdown(report: dict[str, Any]) -> str:
    out = [
        "# Native ELF map",
        "",
        f"- Path: `{report['path']}`",
        f"- SHA-256: `{report['sha256']}`",
        f"- Size: `{report['size']}` bytes",
    ]
    for key in ("class", "machine", "type"):
        if key in report:
            out.append(f"- {key.title()}: `{report[key]}`")
    if report["needed"]:
        out.append("- DT_NEEDED: " + ", ".join(f"`{item}`" for item in report["needed"]))

    out += ["", "## Engine/version strings", ""]
    out.extend(f"- `{item}`" for item in report["engine_version_strings"] or ["none found"])

    out += ["", "## JNI exports", ""]
    out.extend(f"- `{item}`" for item in report["jni_exports"] or ["none found"])

    out += ["", "## Startup symbols", ""]
    for symbol in report["startup_symbols"]:
        out.append(f"- `{symbol['address']}` `{symbol['name']}`")
    if not report["startup_symbols"]:
        out.append("- none found")

    out += ["", "## Non-engine RTTI types", ""]
    out.extend(f"- `{item}`" for item in report["game_rtti_types"] or ["none found"])
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--json", dest="json_path", type=Path)
    parser.add_argument("--markdown", dest="markdown_path", type=Path)
    args = parser.parse_args()

    report = analyze(args.elf)
    if args.json_path:
        args.json_path.parent.mkdir(parents=True, exist_ok=True)
        args.json_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.markdown_path:
        args.markdown_path.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_path.write_text(markdown(report), encoding="utf-8")
    if not args.json_path and not args.markdown_path:
        print(markdown(report), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
