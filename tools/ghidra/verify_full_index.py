#!/usr/bin/env python3
"""Validate the checked-in Ghidra and archive metadata bundles."""

import argparse
import base64
import csv
import gzip
import json
from pathlib import Path


HEADERS = {
    "functions": ["entry", "name", "address_count", "is_external", "signature"],
    "symbols": ["address", "name", "type", "source", "is_primary"],
    "strings": ["address", "utf8_base64", "length"],
    "xrefs": ["from", "to", "type", "operand", "source"],
    "calls": ["caller_entry", "call_site", "callee_entry"],
    "archive_entries": ["archive", "path", "uncompressed_size", "compressed_size", "zip_crc32"],
    "atlas_frames": ["archive", "plist", "frame", "texture_rect", "color_rect",
                     "offset", "sprite_size", "source_size", "rotated", "aliases_json"],
    "unreadable_plists": ["archive", "path", "error_type"],
}


def rows(path: Path, schema: str):
    with gzip.open(path, "rt", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream, delimiter="\t")
        if reader.fieldnames != HEADERS[schema]:
            raise ValueError(f"{path}: unexpected header {reader.fieldnames!r}")
        for number, row in enumerate(reader, start=2):
            if None in row or any(value is None for value in row.values()):
                raise ValueError(f"{path}:{number}: malformed TSV row")
            yield row


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    directory = args.directory
    counts = {}

    for program in ("libcocos2dcpp", "libffmpeg"):
        function_entries = set()
        for row in rows(directory / f"{program}.functions.tsv.gz", "functions"):
            if row["entry"] in function_entries:
                raise ValueError(f"{program}: duplicate function entry {row['entry']}")
            function_entries.add(row["entry"])
        counts[f"{program}.functions"] = len(function_entries)

        for schema in ("symbols", "strings", "xrefs", "calls"):
            count = 0
            for row in rows(directory / f"{program}.{schema}.tsv.gz", schema):
                if schema == "strings":
                    base64.b64decode(row["utf8_base64"], validate=True).decode("utf-8")
                elif schema == "calls" and row["caller_entry"] not in function_entries:
                    raise ValueError(f"{program}: call from unknown function {row['caller_entry']}")
                count += 1
            counts[f"{program}.{schema}"] = count

    for schema in ("archive_entries", "atlas_frames", "unreadable_plists"):
        count = 0
        for row in rows(directory / f"{schema}.tsv.gz", schema):
            if schema == "atlas_frames":
                json.loads(row["aliases_json"])
            count += 1
        counts[schema] = count

    archive_manifest = json.loads((directory / "archive_manifest.json").read_text(encoding="utf-8"))
    if counts["archive_entries"] != sum(item["entries"] for item in archive_manifest["archives"]):
        raise ValueError("archive entry count does not match manifest")
    if counts["atlas_frames"] != sum(item["texturepacker_frames"] for item in archive_manifest["archives"]):
        raise ValueError("atlas frame count does not match manifest")
    if counts["unreadable_plists"] != sum(item["unreadable_plists"] for item in archive_manifest["archives"]):
        raise ValueError("unreadable plist count does not match manifest")
    native_manifest = json.loads((directory / "native_manifest.json").read_text(encoding="utf-8"))
    if native_manifest["source_apk_sha256"] != archive_manifest["archives"][0]["sha256"]:
        raise ValueError("native and archive manifests name different APKs")
    for program in native_manifest["programs"]:
        stem = program["program"].removesuffix(".so")
        for schema, expected in program["counts"].items():
            if counts[f"{stem}.{schema}"] != expected:
                raise ValueError(f"{stem}.{schema} count does not match manifest")
    print(json.dumps(counts, sort_keys=True, indent=2))


if __name__ == "__main__":
    main()
