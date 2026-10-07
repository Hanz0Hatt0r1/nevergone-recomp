#!/usr/bin/env python3
"""Build a categorized CSV/Markdown symbol map from an Android ELF library."""
from __future__ import annotations

import argparse
import collections
import csv
import re
import shutil
import subprocess
from pathlib import Path


def run(args: list[str], *, input_text: str | None = None) -> str:
    return subprocess.check_output(args, input=input_text, text=True, errors="replace")


def parse_symbols(elf: Path):
    text = run(["readelf", "-Ws", str(elf)])
    result = []
    seen = set()
    for line in text.splitlines():
        parts = line.split(None, 7)
        if len(parts) != 8 or not parts[0].rstrip(":").isdigit():
            continue
        _, value, size, typ, bind, vis, ndx, name = parts
        if typ != "FUNC" or ndx == "UND" or not name:
            continue
        key = (value, size, name)
        if key in seen:
            continue
        seen.add(key)
        result.append({
            "address": int(value, 16) & ~1,
            "raw_address": int(value, 16),
            "size": int(size),
            "binding": bind,
            "visibility": vis,
            "section": ndx,
            "mangled": name,
        })
    return result


def demangle(symbols):
    if not shutil.which("c++filt"):
        for s in symbols:
            s["demangled"] = s["mangled"]
        return
    names = "\n".join(s["mangled"] for s in symbols)
    output = run(["c++filt"], input_text=names).splitlines()
    for s, d in zip(symbols, output):
        s["demangled"] = d


def classify(name: str, demangled: str) -> str:
    low = (name + " " + demangled).lower()
    if name == "JNI_OnLoad" or name.startswith("Java_"):
        return "jni"
    if "cocos2d::" in demangled:
        return "cocos2d"
    if "google::protobuf" in demangled or "protobuf" in low:
        return "protobuf"
    if re.search(r"(^|::)lua[_a-zA-Z0-9]*\b", demangled) or name.startswith("lua") or "lua_" in low:
        return "lua"
    if "tinyxml" in low:
        return "tinyxml"
    if demangled.startswith("std::") or " std::" in demangled or name.startswith(("_ZSt", "_ZNSt")):
        return "stdlib"
    if name.startswith("_") and not name.startswith("_Z"):
        return "runtime"
    return "game-or-third-party"


def owner(demangled: str) -> str:
    head = demangled.split("(", 1)[0]
    head = re.sub(r"^(non-virtual thunk to |virtual thunk to )", "", head)
    if "::" in head:
        return head.split("::", 1)[0].strip()
    return "<global>"


def write_csv(path: Path, symbols):
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = ["address", "raw_address", "size", "binding", "visibility", "section", "category", "owner", "mangled", "demangled"]
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for s in symbols:
            row = dict(s)
            row["address"] = f"0x{s['address']:08x}"
            row["raw_address"] = f"0x{s['raw_address']:08x}"
            w.writerow(row)


def write_markdown(path: Path, elf: Path, symbols):
    cats = collections.Counter(s["category"] for s in symbols)
    owners = collections.Counter(s["owner"] for s in symbols if s["category"] == "game-or-third-party")
    jni = [s for s in symbols if s["category"] == "jni"]
    out = [
        "# Native symbol baseline",
        "",
        f"Source library: `{elf.name}`",
        "",
        f"Defined function symbols: **{len(symbols)}**",
        "",
        "## Categories",
        "",
        "| Category | Functions |",
        "| --- | ---: |",
    ]
    for cat, n in cats.most_common():
        out.append(f"| `{cat}` | {n} |")
    out += ["", "## Largest game/third-party owners", "", "| Owner / class | Functions |", "| --- | ---: |"]
    for name, n in owners.most_common(40):
        out.append(f"| `{name}` | {n} |")
    out += ["", "## JNI exports", ""]
    for s in sorted(jni, key=lambda x: x["address"]):
        out.append(f"- `0x{s['address']:08x}` — `{s['demangled']}`")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("elf", type=Path)
    p.add_argument("--csv", type=Path)
    p.add_argument("--markdown", type=Path)
    args = p.parse_args()

    symbols = parse_symbols(args.elf)
    demangle(symbols)
    for s in symbols:
        s["category"] = classify(s["mangled"], s["demangled"])
        s["owner"] = owner(s["demangled"])
    symbols.sort(key=lambda s: (s["address"], s["mangled"]))

    if args.csv:
        write_csv(args.csv, symbols)
    if args.markdown:
        write_markdown(args.markdown, args.elf, symbols)
    if not args.csv and not args.markdown:
        counts = collections.Counter(s["category"] for s in symbols)
        for k, v in counts.most_common():
            print(f"{k:24s} {v:6d}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
