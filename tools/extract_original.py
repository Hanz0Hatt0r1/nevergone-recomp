#!/usr/bin/env python3
"""Extract Never Gone inputs from a user-supplied original APK into a local work tree.

The tool is intentionally conservative: it never modifies the APK and is meant
for local reverse-engineering/build preparation only. Extracted proprietary data
must remain outside Git.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path

EXPECTED_PACKAGE_MARKER = b"com.hippiegame.nevergone"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def safe_target(root: Path, name: str) -> Path:
    target = (root / name).resolve()
    root = root.resolve()
    if root not in target.parents and target != root:
        raise ValueError(f"unsafe ZIP path: {name}")
    return target


def looks_like_nevergone(zf: zipfile.ZipFile) -> bool:
    names = set(zf.namelist())
    required = {
        "AndroidManifest.xml",
        "classes.dex",
        "lib/armeabi-v7a/libcocos2dcpp.so",
        "lib/armeabi-v7a/libffmpeg.so",
    }
    if not required.issubset(names):
        return False
    try:
        dex = zf.read("classes.dex")
    except KeyError:
        return False
    return EXPECTED_PACKAGE_MARKER in dex


def should_extract(name: str, mode: str) -> bool:
    if mode == "assets":
        return name.startswith("assets/")
    if mode == "analysis":
        return (
            name == "AndroidManifest.xml"
            or name == "classes.dex"
            or name.startswith("lib/")
            or name.startswith("assets/")
        )
    if mode == "all":
        return True
    raise ValueError(mode)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path, help="path to the user's original APK")
    parser.add_argument("output", type=Path, help="local extraction directory")
    parser.add_argument(
        "--mode",
        choices=("assets", "analysis", "all"),
        default="analysis",
        help="what to extract (default: analysis)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="replace an existing output directory",
    )
    args = parser.parse_args()

    apk = args.apk.resolve()
    out = args.output.resolve()

    if not apk.is_file():
        raise SystemExit(f"APK not found: {apk}")
    if out.exists():
        if not args.force:
            raise SystemExit(f"output already exists: {out} (use --force to replace it)")
        shutil.rmtree(out)
    out.mkdir(parents=True)

    manifest: dict[str, object] = {
        "source_apk": apk.name,
        "source_sha256": sha256_file(apk),
        "mode": args.mode,
        "files": [],
    }

    with zipfile.ZipFile(apk) as zf:
        if not looks_like_nevergone(zf):
            shutil.rmtree(out, ignore_errors=True)
            raise SystemExit("APK does not match the expected Never Gone Android layout")

        files: list[dict[str, object]] = []
        for info in zf.infolist():
            if info.is_dir() or not should_extract(info.filename, args.mode):
                continue
            target = safe_target(out, info.filename)
            target.parent.mkdir(parents=True, exist_ok=True)
            data = zf.read(info)
            target.write_bytes(data)
            files.append(
                {
                    "path": info.filename,
                    "size": len(data),
                    "sha256": hashlib.sha256(data).hexdigest(),
                }
            )
        manifest["files"] = files

    (out / "extraction-manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    print(f"extracted {len(manifest['files'])} files to {out}")
    print(f"APK SHA-256: {manifest['source_sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
