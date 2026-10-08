#!/usr/bin/env python3
"""Export file and TexturePacker metadata from user-owned Nevergone archives.

The output contains no archive member bytes or decoded assets.
"""

import argparse
import csv
import gzip
import hashlib
import io
import json
import plistlib
import zipfile
from pathlib import Path
from xml.parsers.expat import ExpatError


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tsv_writer(path: Path, header: list[str]):
    binary = path.open("wb")
    compressed = gzip.GzipFile(filename="", mode="wb", fileobj=binary, mtime=0)
    text = io.TextIOWrapper(compressed, encoding="utf-8", newline="")
    writer = csv.writer(text, delimiter="\t", lineterminator="\n")
    writer.writerow(header)
    return writer, text, compressed, binary


def index_archive(label: str, path: Path, entries, frames, unreadable) -> dict:
    entry_count = 0
    atlas_count = 0
    frame_count = 0
    invalid_plists = 0
    with zipfile.ZipFile(path) as archive:
        for info in archive.infolist():
            if info.is_dir():
                continue
            entries.writerow((label, info.filename, info.file_size,
                             info.compress_size, f"{info.CRC:08x}"))
            entry_count += 1
            if not info.filename.lower().endswith(".plist"):
                continue
            try:
                content = plistlib.loads(archive.read(info))
            except (ValueError, TypeError, plistlib.InvalidFileException, ExpatError) as error:
                unreadable.writerow((label, info.filename, type(error).__name__))
                invalid_plists += 1
                continue
            if not isinstance(content, dict) or not isinstance(content.get("frames"), dict):
                continue
            atlas_count += 1
            for name, frame in content["frames"].items():
                if not isinstance(frame, dict):
                    continue
                frames.writerow((
                    label, info.filename, name,
                    frame.get("textureRect", ""), frame.get("spriteColorRect", ""),
                    frame.get("spriteOffset", ""), frame.get("spriteSize", ""),
                    frame.get("spriteSourceSize", ""), frame.get("textureRotated", ""),
                    json.dumps(frame.get("aliases", []), ensure_ascii=False, separators=(",", ":")),
                ))
                frame_count += 1
    return {
        "archive": path.name,
        "sha256": sha256(path),
        "entries": entry_count,
        "texturepacker_atlases": atlas_count,
        "texturepacker_frames": frame_count,
        "unreadable_plists": invalid_plists,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", type=Path, required=True)
    parser.add_argument("--obb", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    entries, entries_text, entries_gzip, entries_file = tsv_writer(
        args.output / "archive_entries.tsv.gz",
        ["archive", "path", "uncompressed_size", "compressed_size", "zip_crc32"])
    frames, frames_text, frames_gzip, frames_file = tsv_writer(
        args.output / "atlas_frames.tsv.gz",
        ["archive", "plist", "frame", "texture_rect", "color_rect", "offset",
         "sprite_size", "source_size", "rotated", "aliases_json"])
    unreadable, unreadable_text, unreadable_gzip, unreadable_file = tsv_writer(
        args.output / "unreadable_plists.tsv.gz", ["archive", "path", "error_type"])
    try:
        manifest = {
            "schema_version": 1,
            "archives": [
                index_archive("apk", args.apk, entries, frames, unreadable),
                index_archive("obb", args.obb, entries, frames, unreadable),
            ],
        }
    finally:
        entries_text.close()
        frames_text.close()
        unreadable_text.close()
        entries_gzip.close()
        frames_gzip.close()
        unreadable_gzip.close()
        entries_file.close()
        frames_file.close()
        unreadable_file.close()
    (args.output / "archive_manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
