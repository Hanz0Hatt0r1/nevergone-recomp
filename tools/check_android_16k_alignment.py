#!/usr/bin/env python3
"""Validate 16 KiB page compatibility of native libraries packaged in an APK.

Checks both parts that matter for modern Android devices with 16 KiB pages:
- every ELF PT_LOAD segment in lib/*/*.so has p_align >= 16 KiB
- every stored native library starts at a 16 KiB-aligned offset inside the APK

The checker uses only Python's standard library so it can run in CI and on a
local Linux development machine without Android-specific inspection tools.
"""
from __future__ import annotations

import argparse
import io
import struct
import sys
import zipfile
from pathlib import Path

PAGE_SIZE = 16 * 1024
PT_LOAD = 1
ELF_MAGIC = b"\x7fELF"


class AlignmentError(ValueError):
    pass


def load_alignments(blob: bytes) -> list[int]:
    if len(blob) < 16 or blob[:4] != ELF_MAGIC:
        raise AlignmentError("not an ELF file")

    elf_class = blob[4]
    data_encoding = blob[5]
    if data_encoding == 1:
        endian = "<"
    elif data_encoding == 2:
        endian = ">"
    else:
        raise AlignmentError(f"unsupported ELF data encoding {data_encoding}")

    if elf_class == 1:  # ELF32
        header_format = endian + "16sHHIIIIIHHHHHH"
        header_size = struct.calcsize(header_format)
        if len(blob) < header_size:
            raise AlignmentError("truncated ELF32 header")
        header = struct.unpack_from(header_format, blob, 0)
        phoff = header[5]
        phentsize = header[9]
        phnum = header[10]
        ph_format = endian + "IIIIIIII"
        expected_ph_size = struct.calcsize(ph_format)
        type_index = 0
        align_index = 7
    elif elf_class == 2:  # ELF64
        header_format = endian + "16sHHIQQQIHHHHHH"
        header_size = struct.calcsize(header_format)
        if len(blob) < header_size:
            raise AlignmentError("truncated ELF64 header")
        header = struct.unpack_from(header_format, blob, 0)
        phoff = header[5]
        phentsize = header[9]
        phnum = header[10]
        ph_format = endian + "IIQQQQQQ"
        expected_ph_size = struct.calcsize(ph_format)
        type_index = 0
        align_index = 7
    else:
        raise AlignmentError(f"unsupported ELF class {elf_class}")

    if phentsize < expected_ph_size:
        raise AlignmentError(
            f"program-header entry too small: {phentsize} < {expected_ph_size}"
        )

    alignments: list[int] = []
    for index in range(phnum):
        offset = phoff + index * phentsize
        if offset + expected_ph_size > len(blob):
            raise AlignmentError("truncated program-header table")
        program = struct.unpack_from(ph_format, blob, offset)
        if program[type_index] == PT_LOAD:
            alignments.append(int(program[align_index]))

    if not alignments:
        raise AlignmentError("ELF has no PT_LOAD segments")
    return alignments


def zip_data_offset(apk: io.BufferedReader, info: zipfile.ZipInfo) -> int:
    apk.seek(info.header_offset)
    local_header = apk.read(30)
    if len(local_header) != 30:
        raise AlignmentError("truncated ZIP local header")
    signature, *_unused, name_length, extra_length = struct.unpack(
        "<IHHHHHIIIHH", local_header
    )
    if signature != 0x04034B50:
        raise AlignmentError("invalid ZIP local-header signature")
    return info.header_offset + 30 + name_length + extra_length


def validate_apk(apk_path: Path) -> list[str]:
    errors: list[str] = []
    checked = 0

    with apk_path.open("rb") as raw, zipfile.ZipFile(raw) as archive:
        native_entries = [
            info
            for info in archive.infolist()
            if info.filename.startswith("lib/") and info.filename.endswith(".so")
        ]
        if not native_entries:
            return ["APK contains no lib/*/*.so native libraries"]

        for info in native_entries:
            checked += 1
            name = info.filename

            if info.compress_type != zipfile.ZIP_STORED:
                errors.append(f"{name}: native library is compressed in APK")
            else:
                try:
                    offset = zip_data_offset(raw, info)
                    if offset % PAGE_SIZE != 0:
                        errors.append(
                            f"{name}: APK data offset 0x{offset:x} is not 16 KiB aligned"
                        )
                except AlignmentError as error:
                    errors.append(f"{name}: {error}")

            try:
                blob = archive.read(info)
                aligns = load_alignments(blob)
                bad = [value for value in aligns if value < PAGE_SIZE]
                if bad:
                    formatted = ", ".join(f"0x{value:x}" for value in aligns)
                    errors.append(
                        f"{name}: PT_LOAD alignments [{formatted}] are not all >= 0x4000"
                    )
            except (AlignmentError, OSError, zipfile.BadZipFile) as error:
                errors.append(f"{name}: ELF validation failed: {error}")

    if not errors:
        print(f"16 KiB alignment: ok ({checked} native libraries)")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path, help="APK to validate")
    args = parser.parse_args()

    if not args.apk.is_file():
        parser.error(f"APK not found: {args.apk}")

    errors = validate_apk(args.apk)
    if errors:
        print("16 KiB alignment: failed", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
