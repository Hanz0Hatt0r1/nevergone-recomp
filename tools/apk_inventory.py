#!/usr/bin/env python3
"""Generate a reproducible inventory of a Never Gone Android APK.

The script intentionally uses only Python's standard library. Optional ELF
metadata is collected with the host `readelf`/`strings` utilities when they are
available.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import re
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path
from typing import Any


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run_text(args: list[str]) -> str:
    try:
        return subprocess.check_output(args, stderr=subprocess.STDOUT, text=True, errors="replace")
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def elf_metadata(data: bytes, name: str) -> dict[str, Any]:
    result: dict[str, Any] = {
        "path": name,
        "sha256": sha256_bytes(data),
        "size": len(data),
        "needed": [],
        "jni_exports": [],
    }
    if not shutil.which("readelf"):
        result["note"] = "readelf not found; ELF details skipped"
        return result

    with tempfile.NamedTemporaryFile(suffix=".so") as tmp:
        tmp.write(data)
        tmp.flush()

        header = run_text(["readelf", "-h", tmp.name])
        for key in ("Class", "Data", "Type", "Machine"):
            m = re.search(rf"^\s*{re.escape(key)}:\s*(.+)$", header, re.MULTILINE)
            if m:
                result[key.lower()] = m.group(1).strip()

        dynamic = run_text(["readelf", "-d", tmp.name])
        result["needed"] = re.findall(r"\(NEEDED\).*?\[(.*?)\]", dynamic)

        symbols = run_text(["readelf", "-Ws", tmp.name])
        exports: set[str] = set()
        for line in symbols.splitlines():
            parts = line.split()
            if not parts:
                continue
            symbol = parts[-1]
            if symbol == "JNI_OnLoad" or symbol.startswith("Java_"):
                exports.add(symbol)
        result["jni_exports"] = sorted(exports)

        if shutil.which("strings"):
            strings = run_text(["strings", tmp.name])
            cocos = re.findall(r"cocos2d[^\s\x00]{0,80}", strings, re.IGNORECASE)
            result["cocos_version_strings"] = sorted(set(cocos))[:20]

    return result


def printable_ratio(data: bytes) -> float:
    if not data:
        return 0.0
    good = sum((32 <= b < 127) or b in (9, 10, 13) for b in data)
    return good / len(data)


def lua_classification(data: bytes) -> str:
    if data.startswith(b"\x1bLua"):
        return "lua-bytecode"
    sample = data[:512]
    ratio = printable_ratio(sample)
    text_markers = (b"function ", b"local ", b"require", b"return ", b"--")
    if ratio > 0.85 and any(marker in sample for marker in text_markers):
        return "lua-source"
    return "encoded-or-obfuscated"


def build_inventory(apk_path: Path) -> dict[str, Any]:
    with zipfile.ZipFile(apk_path) as zf:
        infos = [i for i in zf.infolist() if not i.is_dir()]
        assets = [i for i in infos if i.filename.startswith("assets/")]
        dex = [i.filename for i in infos if re.fullmatch(r"classes\d*\.dex", Path(i.filename).name)]
        native = [i for i in infos if i.filename.startswith("lib/") and i.filename.endswith(".so")]
        lua = [i for i in assets if i.filename.lower().endswith(".lua")]

        extensions = collections.Counter(Path(i.filename).suffix.lower() or "<none>" for i in assets)
        lua_classes = collections.Counter()
        lua_headers = collections.Counter()
        for info in lua:
            data = zf.read(info)
            lua_classes[lua_classification(data)] += 1
            lua_headers[data[:8].hex()] += 1

        libraries = [elf_metadata(zf.read(info), info.filename) for info in native]

        return {
            "apk": {
                "path": str(apk_path),
                "sha256": sha256_file(apk_path),
                "size": apk_path.stat().st_size,
                "zip_entries": len(infos),
                "uncompressed_size": sum(i.file_size for i in infos),
                "compressed_size": sum(i.compress_size for i in infos),
            },
            "dex_files": sorted(dex),
            "assets": {
                "count": len(assets),
                "uncompressed_size": sum(i.file_size for i in assets),
                "extensions": dict(sorted(extensions.items())),
            },
            "lua": {
                "count": len(lua),
                "classification": dict(sorted(lua_classes.items())),
                "most_common_headers": lua_headers.most_common(20),
            },
            "native_libraries": libraries,
        }


def markdown(inv: dict[str, Any]) -> str:
    apk = inv["apk"]
    out = [
        "# APK inventory",
        "",
        f"- SHA-256: `{apk['sha256']}`",
        f"- APK size: `{apk['size']}` bytes",
        f"- ZIP entries: `{apk['zip_entries']}`",
        f"- DEX files: `{', '.join(inv['dex_files']) or 'none'}`",
        f"- Asset files: `{inv['assets']['count']}`",
        f"- Lua-like files: `{inv['lua']['count']}`",
        "",
        "## Asset extensions",
        "",
        "| Extension | Count |",
        "| --- | ---: |",
    ]
    for ext, count in sorted(inv["assets"]["extensions"].items(), key=lambda x: (-x[1], x[0])):
        out.append(f"| `{ext}` | {count} |")

    out += ["", "## Lua classification", ""]
    for kind, count in inv["lua"]["classification"].items():
        out.append(f"- `{kind}`: {count}")

    out += ["", "## Native libraries", ""]
    for lib in inv["native_libraries"]:
        out += [
            f"### `{lib['path']}`",
            "",
            f"- SHA-256: `{lib['sha256']}`",
            f"- Size: `{lib['size']}` bytes",
        ]
        for key in ("class", "machine", "type"):
            if key in lib:
                out.append(f"- {key.title()}: `{lib[key]}`")
        if lib.get("needed"):
            out.append("- DT_NEEDED: " + ", ".join(f"`{x}`" for x in lib["needed"]))
        if lib.get("cocos_version_strings"):
            out.append("- Cocos strings: " + ", ".join(f"`{x}`" for x in lib["cocos_version_strings"]))
        out += ["", "JNI exports:", ""]
        if lib.get("jni_exports"):
            out.extend(f"- `{x}`" for x in lib["jni_exports"])
        else:
            out.append("- none found")
        out.append("")
    return "\n".join(out).rstrip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("apk", type=Path)
    parser.add_argument("--json", dest="json_path", type=Path)
    parser.add_argument("--markdown", dest="md_path", type=Path)
    args = parser.parse_args()

    inventory = build_inventory(args.apk)
    if args.json_path:
        args.json_path.parent.mkdir(parents=True, exist_ok=True)
        args.json_path.write_text(json.dumps(inventory, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    if args.md_path:
        args.md_path.parent.mkdir(parents=True, exist_ok=True)
        args.md_path.write_text(markdown(inventory), encoding="utf-8")
    if not args.json_path and not args.md_path:
        print(json.dumps(inventory, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
