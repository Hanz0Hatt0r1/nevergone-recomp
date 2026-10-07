#!/usr/bin/env python3
"""Decode Never Gone assets using the transform recovered from CCFileUtilsAndroid.

The original Android build applies cocos2d::Decode(data, size, out, 1) to files
whose names contain .png, .hpc, .csv or .lua. This tool reproduces that transform
for a user-provided original APK or an extracted asset directory.

By default the tool only validates/prints a summary. Pass --output to write
locally decoded files. Decoded proprietary assets must not be committed to this
repository.
"""
from __future__ import annotations

import argparse
import json
import zipfile
from dataclasses import asdict, dataclass
from pathlib import Path, PurePosixPath
from typing import Iterable, Iterator

DEFAULT_EXTENSIONS = (".png", ".hpc", ".csv", ".lua")
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


@dataclass
class Result:
    path: str
    extension: str
    size: int
    validation: str
    text_encoding: str | None = None


def decode_bytes(data: bytes, key: int = 1) -> bytes:
    """Reproduce cocos2d::Decode from the shipped ARMv7 library."""
    output = bytearray(len(data))
    counter = 0
    for index, value in enumerate(data):
        output[index] = ((value ^ key) - counter) & 0xFF
        counter += 1
        if counter == 0x7F:
            counter = 0
    return bytes(output)


def text_encoding(data: bytes) -> str | None:
    # Most recovered files are UTF-8. A small subset of Lua comments use the
    # original Simplified-Chinese Windows encoding while code remains ASCII.
    for encoding in ("utf-8-sig", "gb18030"):
        try:
            data.decode(encoding)
            return encoding
        except UnicodeDecodeError:
            pass
    return None


def validate(extension: str, data: bytes) -> tuple[str, str | None]:
    if extension == ".png":
        return ("png-signature-ok" if data.startswith(PNG_SIGNATURE) else "invalid-png", None)

    encoding = text_encoding(data)
    if not encoding:
        return "non-text", None
    stripped = data.lstrip(b"\xef\xbb\xbf\r\n\t ")
    if extension == ".hpc":
        status = "text-json-like" if stripped.startswith((b"{", b"[")) else "text"
    elif extension == ".lua":
        status = "lua-text"
    else:
        status = "text"
    return status, encoding


def safe_relative(name: str) -> Path:
    posix = PurePosixPath(name)
    if posix.is_absolute() or ".." in posix.parts:
        raise ValueError(f"unsafe archive path: {name}")
    return Path(*posix.parts)


def iter_apk(path: Path, extensions: set[str]) -> Iterator[tuple[str, bytes]]:
    with zipfile.ZipFile(path) as archive:
        for info in archive.infolist():
            # Android resource PNGs in res/ are ordinary APK resources and do
            # not pass through this native decoder. Limit APK mode to assets/.
            if info.is_dir() or not info.filename.startswith("assets/"):
                continue
            ext = PurePosixPath(info.filename).suffix.lower()
            if ext in extensions:
                yield info.filename, archive.read(info)


def iter_directory(path: Path, extensions: set[str]) -> Iterator[tuple[str, bytes]]:
    for file in sorted(path.rglob("*")):
        if file.is_file() and file.suffix.lower() in extensions:
            yield file.relative_to(path).as_posix(), file.read_bytes()


def source_files(path: Path, extensions: set[str]) -> Iterable[tuple[str, bytes]]:
    if path.is_dir():
        return iter_directory(path, extensions)
    if zipfile.is_zipfile(path):
        return iter_apk(path, extensions)
    if path.is_file() and path.suffix.lower() in extensions:
        return [(path.name, path.read_bytes())]
    raise SystemExit(f"unsupported source: {path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="original APK, extracted directory, or one encoded asset")
    parser.add_argument("--output", type=Path, help="local output directory for decoded files")
    parser.add_argument("--json", dest="json_path", type=Path, help="write a metadata-only JSON report")
    parser.add_argument(
        "--extensions",
        default=",".join(ext.lstrip(".") for ext in DEFAULT_EXTENSIONS),
        help="comma-separated extensions to process (default: png,hpc,csv,lua)",
    )
    parser.add_argument("--key", type=lambda x: int(x, 0), default=1, help="Decode XOR key (known build: 1)")
    args = parser.parse_args()

    extensions = {
        "." + value.strip().lower().lstrip(".")
        for value in args.extensions.split(",")
        if value.strip()
    }
    if not extensions:
        raise SystemExit("no extensions selected")

    results: list[Result] = []
    counts: dict[str, int] = {}
    invalid = 0

    for name, encoded in source_files(args.source, extensions):
        extension = PurePosixPath(name).suffix.lower()
        decoded = decode_bytes(encoded, args.key)
        status, encoding = validate(extension, decoded)
        if status.startswith("invalid") or status == "non-text":
            invalid += 1
        counts[extension] = counts.get(extension, 0) + 1
        results.append(Result(name, extension, len(encoded), status, encoding))

        if args.output:
            target = args.output / safe_relative(name)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(decoded)

    report = {
        "source": str(args.source),
        "key": args.key,
        "processed": len(results),
        "counts": dict(sorted(counts.items())),
        "validation_failures": invalid,
        "files": [asdict(item) for item in results],
    }

    if args.json_path:
        args.json_path.parent.mkdir(parents=True, exist_ok=True)
        args.json_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    print(f"processed: {len(results)}")
    for extension, count in sorted(counts.items()):
        print(f"{extension}: {count}")
    print(f"validation failures: {invalid}")
    if args.output:
        print(f"decoded output: {args.output}")
    else:
        print("dry run only; pass --output DIR to write decoded files")
    return 1 if invalid else 0


if __name__ == "__main__":
    raise SystemExit(main())
