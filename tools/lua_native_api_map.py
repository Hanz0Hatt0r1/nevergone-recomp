#!/usr/bin/env python3
"""Map Lua calls in Never Gone to likely native C/C++ bindings.

The report is metadata-only: APK Lua files are decoded in memory and source
contents are never emitted. By default only modules reachable from Game.StartLua
are considered.
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import tempfile
import zipfile
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

try:
    from asset_decoder import decode_bytes
    from lua_dependency_map import (
        analyze as dependency_analyze,
        decode_text,
        iter_source,
        module_name,
        strip_lua_comments,
    )
except ImportError as exc:
    raise SystemExit(
        "asset_decoder.py and lua_dependency_map.py must be available next to this script"
    ) from exc

DIRECT_CALL_RE = re.compile(r"(?<![\w.:])([A-Za-z_][A-Za-z0-9_]*)\s*\(")
MEMBER_CALL_RE = re.compile(
    r"\b([A-Za-z_][A-Za-z0-9_]*)\s*:\s*([A-Za-z_][A-Za-z0-9_]*)\s*\("
)
FUNCTION_DEF_RES = (
    re.compile(r"\bfunction\s+([A-Za-z_][A-Za-z0-9_]*)\s*\("),
    re.compile(r"\blocal\s+function\s+([A-Za-z_][A-Za-z0-9_]*)\s*\("),
    re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*=\s*function\s*\("),
)
ASSIGNMENT_RE = re.compile(
    r"(?m)^[ \t]*(?:local[ \t]+)?([A-Za-z_][A-Za-z0-9_]*)[ \t]*="
)

LUA_BUILTINS = {
    "assert", "collectgarbage", "dofile", "error", "getfenv", "getmetatable",
    "ipairs", "load", "loadfile", "loadstring", "module", "next", "pairs",
    "pcall", "print", "rawequal", "rawget", "rawset", "require", "select",
    "setfenv", "setmetatable", "tonumber", "tostring", "type", "unpack",
    "xpcall",
}
NATIVE_PREFIXES = ("cpp_", "Lua_", "CAdd", "LGG_", "Protocol")
NATIVE_CLASS_PREFIXES = ("CC", "Protocol", "SimpleAudioEngine", "Cocos")


@dataclass(frozen=True)
class CallSite:
    api: str
    kind: str
    module: str


def mask_strings(text: str) -> str:
    """Mask Lua string contents while preserving identifiers and line structure."""
    chars = list(text)
    i = 0
    while i < len(chars):
        if text.startswith("[[", i):
            end = text.find("]]", i + 2)
            if end < 0:
                end = len(text) - 2
            for pos in range(i, min(end + 2, len(chars))):
                if chars[pos] != "\n":
                    chars[pos] = " "
            i = end + 2
            continue
        if text[i] not in ("'", '"'):
            i += 1
            continue
        quote = text[i]
        i += 1
        while i < len(chars):
            if text[i] == "\\":
                if chars[i] != "\n":
                    chars[i] = " "
                if i + 1 < len(chars) and chars[i + 1] != "\n":
                    chars[i + 1] = " "
                i += 2
                continue
            if text[i] == quote:
                i += 1
                break
            if chars[i] != "\n":
                chars[i] = " "
            i += 1
    return "".join(chars)


def read_scripts(source: Path, key: int) -> dict[str, str]:
    scripts: dict[str, str] = {}
    for path, payload, encoded in iter_source(source):
        decoded = decode_bytes(payload, key) if encoded else payload
        text, _encoding = decode_text(decoded)
        scripts[module_name(path)] = text
    return scripts


def lua_definitions(texts: Iterable[str]) -> tuple[set[str], set[str]]:
    functions: set[str] = set()
    assignments: set[str] = set()
    for text in texts:
        cleaned = mask_strings(strip_lua_comments(text))
        for pattern in FUNCTION_DEF_RES:
            functions.update(pattern.findall(cleaned))
        assignments.update(ASSIGNMENT_RE.findall(cleaned))
    return functions, assignments


def collect_calls(
    scripts: dict[str, str], selected_modules: set[str]
) -> list[CallSite]:
    definitions, assigned = lua_definitions(
        scripts[module] for module in selected_modules
    )
    sites: list[CallSite] = []
    for module in sorted(selected_modules):
        cleaned = mask_strings(strip_lua_comments(scripts[module]))
        for name in DIRECT_CALL_RE.findall(cleaned):
            if name in LUA_BUILTINS or name in definitions:
                continue
            sites.append(CallSite(name, "global", module))
        for receiver, method in MEMBER_CALL_RE.findall(cleaned):
            # Lua-owned tables/instances create a large number of ordinary
            # method calls. Keep known native classes, but otherwise discard
            # locally assigned receivers.
            if receiver in assigned and not receiver.startswith(NATIVE_CLASS_PREFIXES):
                continue
            sites.append(CallSite(f"{receiver}:{method}", "member", module))
    return sites


def run(args: list[str], *, stdin: str | None = None) -> str:
    proc = subprocess.run(
        args,
        input=stdin,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
        errors="replace",
    )
    return proc.stdout


def demangled_symbols(elf: Path) -> list[str]:
    if not shutil.which("nm") or not shutil.which("c++filt"):
        return []
    raw = run(["nm", "-D", "--defined-only", str(elf)])
    mangled = []
    for line in raw.splitlines():
        parts = line.split(maxsplit=2)
        if len(parts) == 3:
            mangled.append(parts[2])
    if not mangled:
        return []
    return run(["c++filt"], stdin="\n".join(mangled) + "\n").splitlines()


def binary_strings(elf: Path) -> set[str]:
    if not shutil.which("strings"):
        return set()
    return set(run(["strings", "-a", str(elf)]).splitlines())


def auto_elf(
    source: Path, explicit: Path | None
) -> tuple[Path | None, tempfile.TemporaryDirectory | None]:
    if explicit:
        return explicit, None
    if not zipfile.is_zipfile(source):
        return None, None
    with zipfile.ZipFile(source) as archive:
        name = "lib/armeabi-v7a/libcocos2dcpp.so"
        if name not in archive.namelist():
            return None, None
        tempdir = tempfile.TemporaryDirectory(prefix="nevergone-native-")
        path = Path(tempdir.name) / "libcocos2dcpp.so"
        path.write_bytes(archive.read(name))
        return path, tempdir


def direct_symbol_matches(name: str, symbols: list[str]) -> list[str]:
    pattern = re.compile(r"(?:^|::|\s)" + re.escape(name) + r"\(")
    return [symbol for symbol in symbols if pattern.search(symbol)][:8]


def member_symbol_matches(
    receiver: str, method: str, symbols: list[str]
) -> list[str]:
    if method == "new":
        patterns = (
            f"{receiver}::{receiver}(",
            f"typeinfo for {receiver}",
            f"::{receiver}::{receiver}(",
        )
        matches = [
            symbol for symbol in symbols
            if any(token in symbol for token in patterns)
        ]
        if matches:
            return matches[:8]
        return [symbol for symbol in symbols if receiver in symbol][:8]
    token = f"{receiver}::{method}("
    return [symbol for symbol in symbols if token in symbol][:8]


def native_hint(api: str, kind: str) -> bool:
    if kind == "global":
        return api.startswith(NATIVE_PREFIXES) or api.endswith("_shared")
    receiver, _method = api.split(":", 1)
    return receiver.startswith(NATIVE_CLASS_PREFIXES)


def evidence_for(
    api: str, kind: str, symbols: list[str], strings: set[str]
) -> tuple[str, list[str]]:
    if kind == "global":
        matches = direct_symbol_matches(api, symbols)
        if matches:
            return "dynamic-symbol", matches
        if api in strings:
            return "binary-string", []
        return "none", []

    receiver, method = api.split(":", 1)
    matches = member_symbol_matches(receiver, method, symbols)
    if matches:
        exact = method != "new" and any(
            f"{receiver}::{method}(" in value for value in matches
        )
        return ("dynamic-symbol" if exact else "class-symbol"), matches
    # Separate receiver/method strings are too weak because ordinary Lua table
    # names and methods can both appear in diagnostics. Only accept a combined
    # registration-like string as textual evidence.
    if api in strings or f"{receiver}.{method}" in strings:
        return "binary-string", []
    return "none", []


def analyze(
    source: Path,
    key: int,
    entry: str,
    elf: Path | None,
    include_unreachable: bool,
) -> dict:
    dependencies = dependency_analyze(source, key, entry)
    scripts = read_scripts(source, key)
    reachable = {
        item["module"]
        for item in dependencies["modules"]
        if item["reachable"]
    }
    selected = set(scripts) if include_unreachable else reachable

    sites = collect_calls(scripts, selected)
    elf_path, tempdir = auto_elf(source, elf)
    try:
        symbols = demangled_symbols(elf_path) if elf_path else []
        strings = binary_strings(elf_path) if elf_path else set()
    finally:
        if tempdir is not None:
            tempdir.cleanup()

    by_api: dict[tuple[str, str], list[CallSite]] = defaultdict(list)
    for site in sites:
        by_api[(site.api, site.kind)].append(site)

    entries = []
    evidence_counts: Counter[str] = Counter()
    kind_counts: Counter[str] = Counter()
    for (api, kind), occurrences in sorted(by_api.items()):
        evidence, matches = evidence_for(api, kind, symbols, strings)
        hinted = native_hint(api, kind)
        if evidence == "none" and not hinted:
            continue
        # A short Lua function name may coincidentally occur in the native
        # string table. String-only evidence is meaningful only when the name
        # already has a native-binding naming signature.
        if evidence == "binary-string" and not hinted:
            continue
        modules = sorted({site.module for site in occurrences})
        item = {
            "api": api,
            "kind": kind,
            "occurrences": len(occurrences),
            "module_count": len(modules),
            "modules": modules,
            "used_by_entry": dependencies["entry"] in modules,
            "native_hint": hinted,
            "evidence": evidence,
            "symbol_examples": matches,
        }
        entries.append(item)
        evidence_counts[evidence] += 1
        kind_counts[kind] += 1

    evidence_rank = {
        "dynamic-symbol": 0,
        "class-symbol": 1,
        "binary-string": 2,
        "none": 3,
    }
    entries.sort(
        key=lambda item: (
            not item["used_by_entry"],
            evidence_rank[item["evidence"]],
            -item["module_count"],
            -item["occurrences"],
            item["api"],
        )
    )

    confirmed = sum(item["evidence"] != "none" for item in entries)
    return {
        "source": str(source),
        "entry": dependencies["entry"],
        "module_count": dependencies["module_count"],
        "reachable_modules": dependencies["reachable_modules"],
        "analyzed_modules": len(selected),
        "include_unreachable": include_unreachable,
        "native_elf": (
            str(elf)
            if elf
            else (
                "APK:lib/armeabi-v7a/libcocos2dcpp.so"
                if elf_path
                else None
            )
        ),
        "candidate_api_count": len(entries),
        "confirmed_candidate_count": confirmed,
        "evidence_counts": dict(sorted(evidence_counts.items())),
        "kind_counts": dict(sorted(kind_counts.items())),
        "apis": entries,
    }


def markdown(report: dict) -> str:
    out = [
        "# Lua → native API map",
        "",
        f"- Lua modules: **{report['module_count']}**",
        f"- Startup-reachable modules: **{report['reachable_modules']}**",
        f"- Modules analyzed: **{report['analyzed_modules']}**",
        f"- Native-shaped/evidenced API candidates: **{report['candidate_api_count']}**",
        f"- Candidates with ELF evidence: **{report['confirmed_candidate_count']}**",
        "",
        "## Evidence",
        "",
    ]
    for evidence, count in report["evidence_counts"].items():
        out.append(f"- `{evidence}`: {count}")

    out += [
        "",
        "## Highest-priority bindings",
        "",
        "| API | Kind | Calls | Modules | Evidence | Entry |",
        "| --- | --- | ---: | ---: | --- | --- |",
    ]
    for item in report["apis"]:
        out.append(
            f"| `{item['api']}` | {item['kind']} | {item['occurrences']} | "
            f"{item['module_count']} | `{item['evidence']}` | "
            f"{'yes' if item['used_by_entry'] else ''} |"
        )

    out += ["", "## Unconfirmed native-shaped names", ""]
    unconfirmed = [
        item for item in report["apis"] if item["evidence"] == "none"
    ]
    if unconfirmed:
        out.extend(f"- `{item['api']}`" for item in unconfirmed)
    else:
        out.append("- none")
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "source", type=Path, help="original APK or decoded Lua directory"
    )
    parser.add_argument("--entry", default="Game.StartLua")
    parser.add_argument("--key", type=lambda value: int(value, 0), default=1)
    parser.add_argument(
        "--elf",
        type=Path,
        help="native ELF (auto-detected from APK when omitted)",
    )
    parser.add_argument("--include-unreachable", action="store_true")
    parser.add_argument("--json", dest="json_path", type=Path)
    parser.add_argument("--markdown", dest="markdown_path", type=Path)
    args = parser.parse_args()

    report = analyze(
        args.source,
        args.key,
        args.entry,
        args.elf,
        args.include_unreachable,
    )
    if args.json_path:
        args.json_path.parent.mkdir(parents=True, exist_ok=True)
        args.json_path.write_text(
            json.dumps(report, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
    if args.markdown_path:
        args.markdown_path.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_path.write_text(markdown(report), encoding="utf-8")
    if not args.json_path and not args.markdown_path:
        print(markdown(report), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
