#!/usr/bin/env python3
"""Build a metadata-only dependency graph for Never Gone Lua modules.

The original APK stores Lua source through the reversible asset transform used
by CCFileUtilsAndroid. APK input is decoded in memory; no script contents are
written unless another tool is explicitly used for extraction.

This is a static loader-call index, not a complete Lua parser. It records direct
string-literal calls to require(), CAddDoString(), and dofile().
"""
from __future__ import annotations

import argparse
import json
import re
import zipfile
from collections import Counter, deque
from dataclasses import asdict, dataclass
from pathlib import Path, PurePosixPath
from typing import Iterator

try:
    from asset_decoder import decode_bytes
except ImportError:
    raise SystemExit("asset_decoder.py must be available next to this script")

CALL_PATTERNS = {
    "require": re.compile(r'''\brequire[ \t]*(?:\([ \t]*)?(["'])([^"'\r\n]+)\1'''),
    "CAddDoString": re.compile(r'''\bCAddDoString[ \t]*\([ \t]*(["'])([^"'\r\n]+)\1'''),
    "dofile": re.compile(r'''\bdofile[ \t]*\([ \t]*(["'])([^"'\r\n]+)\1'''),
}


@dataclass(frozen=True)
class Dependency:
    source: str
    kind: str
    requested: str
    resolved: str | None


def decode_text(data: bytes) -> tuple[str, str]:
    """Decode recovered Lua text while preserving the observed legacy encoding."""
    for encoding in ("utf-8-sig", "gb18030"):
        try:
            return data.decode(encoding), encoding
        except UnicodeDecodeError:
            pass
    raise ValueError("decoded Lua payload is neither UTF-8 nor GB18030 text")


def module_name(path: str) -> str:
    normalized = path.replace("\\", "/")
    marker = "/Script/"
    if marker in normalized:
        relative = normalized.split(marker, 1)[1]
    elif normalized.startswith("Script/"):
        relative = normalized[len("Script/"):]
    else:
        relative = PurePosixPath(normalized).name

    if relative.lower().endswith(".lua"):
        relative = relative[:-4]
    return relative.replace("/", ".")


def iter_source(source: Path) -> Iterator[tuple[str, bytes, bool]]:
    """Yield (path, bytes, needs_legacy_decode). Directories are assumed decoded."""
    if source.is_dir():
        for file in sorted(source.rglob("*.lua")):
            yield file.relative_to(source).as_posix(), file.read_bytes(), False
        return

    if zipfile.is_zipfile(source):
        with zipfile.ZipFile(source) as archive:
            infos = sorted(archive.infolist(), key=lambda item: item.filename)
            for info in infos:
                if (
                    not info.is_dir()
                    and info.filename.startswith("assets/")
                    and info.filename.lower().endswith(".lua")
                ):
                    yield info.filename, archive.read(info), True
        return

    if source.is_file() and source.suffix.lower() == ".lua":
        yield source.name, source.read_bytes(), False
        return

    raise SystemExit(f"unsupported source: {source}")


def strip_lua_comments(text: str) -> str:
    """Remove ordinary Lua line comments and --[[...]] block comments.

    This is intentionally conservative and keeps quoted strings intact. It is
    sufficient for loader calls in the recovered Never Gone script set.
    """
    output: list[str] = []
    i = 0
    in_block_comment = False
    quote: str | None = None
    escaped = False

    while i < len(text):
        if in_block_comment:
            if text.startswith("]]", i):
                in_block_comment = False
                i += 2
            else:
                if text[i] == "\n":
                    output.append("\n")
                i += 1
            continue

        char = text[i]
        if quote is not None:
            output.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
            i += 1
            continue

        if char in ('"', "'"):
            quote = char
            output.append(char)
            i += 1
            continue

        if text.startswith("--[[", i):
            in_block_comment = True
            i += 4
            continue

        if text.startswith("--", i):
            newline = text.find("\n", i)
            if newline < 0:
                break
            output.append("\n")
            i = newline + 1
            continue

        output.append(char)
        i += 1

    return "".join(output)


def normalize_requested(value: str) -> str:
    value = value.strip().replace("\\", "/").replace("/", ".")
    if value.lower().endswith(".lua"):
        value = value[:-4]
    if value.startswith("Script."):
        value = value[len("Script."):]
    return value


def resolve_module(requested: str, modules: set[str]) -> str | None:
    normalized = normalize_requested(requested)
    if normalized in modules:
        return normalized

    suffix_matches = sorted(
        module
        for module in modules
        if module == normalized or module.endswith("." + normalized)
    )
    if len(suffix_matches) == 1:
        return suffix_matches[0]
    return None


def dependencies_for(module: str, text: str, modules: set[str]) -> list[Dependency]:
    cleaned = strip_lua_comments(text)
    dependencies: list[Dependency] = []
    for kind, pattern in CALL_PATTERNS.items():
        for match in pattern.finditer(cleaned):
            requested = match.group(2)
            dependencies.append(
                Dependency(module, kind, requested, resolve_module(requested, modules))
            )
    dependencies.sort(key=lambda item: (item.source, item.kind, item.requested))
    return dependencies


def analyze(source: Path, key: int, entry: str) -> dict:
    scripts: dict[str, dict[str, str]] = {}
    for path, payload, encoded in iter_source(source):
        decoded = decode_bytes(payload, key) if encoded else payload
        text, encoding = decode_text(decoded)
        name = module_name(path)
        if name in scripts:
            raise ValueError(
                f"duplicate module name {name!r}: {scripts[name]['path']} and {path}"
            )
        scripts[name] = {"path": path, "text": text, "encoding": encoding}

    modules = set(scripts)
    dependencies: list[Dependency] = []
    for name in sorted(scripts):
        dependencies.extend(dependencies_for(name, scripts[name]["text"], modules))

    adjacency = {name: set() for name in modules}
    for dependency in dependencies:
        if dependency.resolved:
            adjacency[dependency.source].add(dependency.resolved)

    normalized_entry = resolve_module(entry, modules) or normalize_requested(entry)
    reachable: set[str] = set()
    if normalized_entry in modules:
        queue = deque([normalized_entry])
        while queue:
            current = queue.popleft()
            if current in reachable:
                continue
            reachable.add(current)
            queue.extend(sorted(adjacency[current] - reachable))

    direct_counts = Counter(dependency.source for dependency in dependencies)
    unresolved = [dependency for dependency in dependencies if dependency.resolved is None]
    unique_edges = sorted(
        {
            (dependency.source, dependency.resolved, dependency.kind)
            for dependency in dependencies
            if dependency.resolved is not None
        }
    )
    entry_direct = sorted(adjacency.get(normalized_entry, set()))
    encodings = Counter(script["encoding"] for script in scripts.values())

    return {
        "source": str(source),
        "entry": normalized_entry,
        "module_count": len(modules),
        "static_dependency_references": len(dependencies),
        "resolved_references": len(dependencies) - len(unresolved),
        "unresolved_references": len(unresolved),
        "unique_resolved_edges": len(unique_edges),
        "reachable_modules": len(reachable),
        "unreachable_modules": sorted(modules - reachable),
        "entry_direct_dependencies": entry_direct,
        "text_encodings": dict(sorted(encodings.items())),
        "unresolved": [asdict(item) for item in unresolved],
        "largest_aggregators": [
            {"module": module, "dependency_references": count}
            for module, count in direct_counts.most_common(20)
        ],
        "modules": [
            {
                "module": module,
                "path": scripts[module]["path"],
                "encoding": scripts[module]["encoding"],
                "reachable": module in reachable,
                "direct_dependency_references": direct_counts[module],
            }
            for module in sorted(modules)
        ],
        "edges": [
            {"source": source_name, "target": target_name, "kind": kind}
            for source_name, target_name, kind in unique_edges
        ],
    }


def markdown(report: dict) -> str:
    out = [
        "# Lua dependency map",
        "",
        f"- Modules: **{report['module_count']}**",
        f"- Entry: `{report['entry']}`",
        f"- Static dependency references: **{report['static_dependency_references']}**",
        f"- Resolved references: **{report['resolved_references']}**",
        f"- Unresolved references: **{report['unresolved_references']}**",
        f"- Unique resolved edges: **{report['unique_resolved_edges']}**",
        f"- Reachable modules: **{report['reachable_modules']}**",
        f"- Unreachable modules: **{len(report['unreachable_modules'])}**",
        "",
        "## Entry dependencies",
        "",
    ]
    if report["entry_direct_dependencies"]:
        out.extend(f"- `{value}`" for value in report["entry_direct_dependencies"])
    else:
        out.append("- none")

    out += [
        "",
        "## Largest aggregators",
        "",
        "| Module | Direct loader references |",
        "| --- | ---: |",
    ]
    for item in report["largest_aggregators"]:
        out.append(f"| `{item['module']}` | {item['dependency_references']} |")

    out += ["", "## Unresolved references", ""]
    if report["unresolved"]:
        for item in report["unresolved"]:
            out.append(
                f"- `{item['source']}` → `{item['requested']}` ({item['kind']})"
            )
    else:
        out.append("- none")

    out += ["", "## Unreachable modules", ""]
    if report["unreachable_modules"]:
        out.extend(f"- `{module}`" for module in report["unreachable_modules"])
    else:
        out.append("- none")

    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "source",
        type=Path,
        help="original APK, decoded Lua directory, or one decoded Lua file",
    )
    parser.add_argument("--entry", default="Game.StartLua", help="entry module")
    parser.add_argument(
        "--key",
        type=lambda value: int(value, 0),
        default=1,
        help="legacy APK Decode XOR key (known build: 1)",
    )
    parser.add_argument("--json", dest="json_path", type=Path)
    parser.add_argument("--markdown", dest="markdown_path", type=Path)
    args = parser.parse_args()

    report = analyze(args.source, args.key, args.entry)
    if args.json_path:
        args.json_path.parent.mkdir(parents=True, exist_ok=True)
        args.json_path.write_text(
            json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
        )
    if args.markdown_path:
        args.markdown_path.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_path.write_text(markdown(report), encoding="utf-8")
    if not args.json_path and not args.markdown_path:
        print(markdown(report), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
