#!/usr/bin/env python3
"""Summarize Lua call shapes for native-looking Never Gone APIs.

The report is metadata-only. APK Lua is decoded in memory and no source text or
literal string values are emitted. The goal is to recover practical binding
signatures (arity and coarse argument kinds) before implementing compatibility
wrappers.
"""
from __future__ import annotations

import argparse
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

try:
    from asset_decoder import decode_bytes
    from lua_dependency_map import analyze as dependency_analyze, decode_text, iter_source, module_name
except ImportError as exc:
    raise SystemExit("asset_decoder.py and lua_dependency_map.py must be next to this script") from exc

DIRECT_RE = re.compile(r"(?<![\w.:])([A-Za-z_][A-Za-z0-9_]*)\s*\(")
MEMBER_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*:\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(")
NATIVE_PREFIXES = ("cpp_", "Lua_", "CAdd", "LGG_", "Protocol")
NATIVE_RECEIVER_PREFIXES = ("ProtoRPC", "Protocol", "CC", "Cocos", "SimpleAudioEngine")


def read_scripts(source: Path, key: int) -> dict[str, str]:
    scripts: dict[str, str] = {}
    for path, payload, encoded in iter_source(source):
        decoded = decode_bytes(payload, key) if encoded else payload
        text, _encoding = decode_text(decoded)
        scripts[module_name(path)] = text
    return scripts


def native_name(name: str) -> bool:
    return name.startswith(NATIVE_PREFIXES) or name.endswith("_shared")


def native_receiver(name: str) -> bool:
    return name.startswith(NATIVE_RECEIVER_PREFIXES)


def find_matching_paren(text: str, open_index: int) -> int | None:
    depth = 0
    quote: str | None = None
    escaped = False
    i = open_index
    while i < len(text):
        ch = text[i]
        if quote is not None:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
            i += 1
            continue

        if text.startswith("--[[", i):
            end = text.find("]]", i + 4)
            if end < 0:
                return None
            i = end + 2
            continue
        if text.startswith("--", i):
            end = text.find("\n", i + 2)
            if end < 0:
                return None
            i = end + 1
            continue
        if ch in ("'", '"'):
            quote = ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return None


def split_args(body: str) -> list[str]:
    if not body.strip():
        return []
    args: list[str] = []
    start = 0
    paren = brace = bracket = 0
    quote: str | None = None
    escaped = False
    i = 0
    while i < len(body):
        ch = body[i]
        if quote is not None:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
            i += 1
            continue
        if ch in ("'", '"'):
            quote = ch
        elif ch == "(":
            paren += 1
        elif ch == ")":
            paren = max(0, paren - 1)
        elif ch == "{":
            brace += 1
        elif ch == "}":
            brace = max(0, brace - 1)
        elif ch == "[":
            bracket += 1
        elif ch == "]":
            bracket = max(0, bracket - 1)
        elif ch == "," and paren == brace == bracket == 0:
            args.append(body[start:i].strip())
            start = i + 1
        i += 1
    args.append(body[start:].strip())
    return args


def arg_kind(value: str) -> str:
    value = value.strip()
    if not value:
        return "empty"
    if value[0:1] in ("'", '"'):
        return "string"
    if value in ("true", "false"):
        return "boolean"
    if value == "nil":
        return "nil"
    if re.fullmatch(r"[-+]?\d+(?:\.\d+)?", value):
        return "number"
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", value):
        return "identifier"
    if value.startswith("{"):
        return "table"
    if value.startswith("function"):
        return "function"
    return "expression"


def iter_calls(text: str):
    matches: list[tuple[int, str, str, int]] = []
    for match in DIRECT_RE.finditer(text):
        name = match.group(1)
        if native_name(name):
            matches.append((match.start(), name, "global", match.end() - 1))
    for match in MEMBER_RE.finditer(text):
        receiver, method = match.group(1), match.group(2)
        if native_receiver(receiver):
            matches.append((match.start(), f"{receiver}:{method}", "member", match.end() - 1))

    for _position, api, kind, open_index in sorted(matches):
        close_index = find_matching_paren(text, open_index)
        if close_index is None:
            continue
        args = split_args(text[open_index + 1 : close_index])
        yield api, kind, args


def analyze(source: Path, key: int, entry: str, include_unreachable: bool) -> dict:
    dependencies = dependency_analyze(source, key, entry)
    scripts = read_scripts(source, key)
    reachable = {item["module"] for item in dependencies["modules"] if item["reachable"]}
    selected = set(scripts) if include_unreachable else reachable

    grouped: dict[tuple[str, str], list[dict]] = defaultdict(list)
    for module in sorted(selected):
        for api, kind, args in iter_calls(scripts[module]):
            grouped[(api, kind)].append(
                {
                    "module": module,
                    "arity": len(args),
                    "arg_kinds": [arg_kind(arg) for arg in args],
                }
            )

    apis = []
    for (api, kind), calls in sorted(grouped.items()):
        arities = Counter(call["arity"] for call in calls)
        shapes = Counter(tuple(call["arg_kinds"]) for call in calls)
        modules = sorted({call["module"] for call in calls})
        apis.append(
            {
                "api": api,
                "kind": kind,
                "calls": len(calls),
                "modules": modules,
                "arities": {str(k): v for k, v in sorted(arities.items())},
                "argument_shapes": [
                    {"kinds": list(shape), "calls": count}
                    for shape, count in sorted(shapes.items(), key=lambda item: (-item[1], item[0]))
                ],
            }
        )

    apis.sort(key=lambda item: (-item["calls"], item["api"]))
    return {
        "source": str(source),
        "entry": dependencies["entry"],
        "analyzed_modules": len(selected),
        "api_count": len(apis),
        "apis": apis,
    }


def markdown(report: dict) -> str:
    out = [
        "# Lua native call shapes",
        "",
        f"- Entry: `{report['entry']}`",
        f"- Modules analyzed: **{report['analyzed_modules']}**",
        f"- Native-looking APIs with calls: **{report['api_count']}**",
        "",
        "| API | Calls | Arities | Most common argument shape |",
        "| --- | ---: | --- | --- |",
    ]
    for item in report["apis"]:
        arities = ", ".join(f"{arity}×{count}" for arity, count in item["arities"].items())
        shape = item["argument_shapes"][0] if item["argument_shapes"] else {"kinds": [], "calls": 0}
        kinds = ", ".join(shape["kinds"]) if shape["kinds"] else "(none)"
        out.append(f"| `{item['api']}` | {item['calls']} | {arities} | {kinds} ({shape['calls']}) |")
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="original APK or decoded Lua directory")
    parser.add_argument("--entry", default="Game.StartLua")
    parser.add_argument("--key", type=lambda value: int(value, 0), default=1)
    parser.add_argument("--include-unreachable", action="store_true")
    parser.add_argument("--json", dest="json_path", type=Path)
    parser.add_argument("--markdown", dest="markdown_path", type=Path)
    args = parser.parse_args()

    report = analyze(args.source, args.key, args.entry, args.include_unreachable)
    if args.json_path:
        args.json_path.parent.mkdir(parents=True, exist_ok=True)
        args.json_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    if args.markdown_path:
        args.markdown_path.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_path.write_text(markdown(report), encoding="utf-8")
    if not args.json_path and not args.markdown_path:
        print(markdown(report), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
