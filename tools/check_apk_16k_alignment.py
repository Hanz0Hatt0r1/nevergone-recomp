#!/usr/bin/env python3
"""Validate PT_LOAD alignment of native libraries packaged in an Android APK."""
from __future__ import annotations

import argparse
import struct
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path

PT_LOAD = 1
MIN_ALIGNMENT = 16 * 1024


@dataclass(frozen=True)
class ElfInfo:
    elf_class: int
    byte_order: str
    program_offset: int
    program_entry_size: int
    program_count: int


def parse_elf_header(data: bytes, name: str) -> ElfInfo:
    if len(data) < 16 or data[:4] != b"\x7fELF":
        raise ValueError(f"{name}: not an ELF file")

    elf_class = data[4]
    encoding = data[5]
    if encoding == 1:
        byte_order = "<"
    elif encoding == 2:
        byte_order = ">"
    else:
        raise ValueError(f"{name}: unsupported ELF byte order {encoding}")

    if elf_class == 1:
        fmt = byte_order + "HHIIIIIHHHHHH"
    elif elf_class == 2:
        fmt = byte_order + "HHIQQQIHHHHHH"
    else:
        raise ValueError(f"{name}: unsupported ELF class {elf_class}")

    header_size = struct.calcsize(fmt)
    if len(data) < 16 + header_size:
        raise ValueError(f"{name}: truncated ELF header")
    fields = struct.unpack_from(fmt, data, 16)
    program_offset = fields[4]
    program_entry_size = fields[8]
    program_count = fields[9]
    return ElfInfo(
        elf_class=elf_class,
        byte_order=byte_order,
        program_offset=program_offset,
        program_entry_size=program_entry_size,
        program_count=program_count,
    )


def load_segment_alignments(data: bytes, name: str) -> list[int]:
    info = parse_elf_header(data, name)
    if info.elf_class == 1:
        fmt = info.byte_order + "IIIIIIII"
        expected_size = struct.calcsize(fmt)
        align_index = 7
    else:
        fmt = info.byte_order + "IIQQQQQQ"
        expected_size = struct.calcsize(fmt)
        align_index = 7

    if info.program_entry_size < expected_size:
        raise ValueError(
            f"{name}: program header size {info.program_entry_size} is smaller than {expected_size}"
        )

    alignments: list[int] = []
    for index in range(info.program_count):
        offset = info.program_offset + index * info.program_entry_size
        if offset + expected_size > len(data):
            raise ValueError(f"{name}: truncated program header {index}")
        fields = struct.unpack_from(fmt, data, offset)
        if fields[0] == PT_LOAD:
            alignments.append(fields[align_index])
    if not alignments:
        raise ValueError(f"{name}: no PT_LOAD program headers")
    return alignments


def validate_apk(apk: Path) -> int:
    failures: list[str] = []
    libraries = 0
    with zipfile.ZipFile(apk) as archive:
        names = sorted(
            info.filename
            for info in archive.infolist()
            if not info.is_dir()
            and info.filename.startswith("lib/")
            and info.filename.endswith(".so")
        )
        if not names:
            print(f"{apk}: no packaged native libraries found", file=sys.stderr)
            return 2

        for name in names:
            libraries += 1
            try:
                alignments = load_segment_alignments(archive.read(name), name)
            except (OSError, ValueError) as error:
                failures.append(str(error))
                continue

            minimum = min(alignments)
            values = ", ".join(f"0x{value:x}" for value in alignments)
            status = "ALIGNED" if minimum >= MIN_ALIGNMENT else "UNALIGNED"
            print(f"{status:9} {name}: PT_LOAD alignments [{values}]")
            if minimum < MIN_ALIGNMENT:
                failures.append(
                    f"{name}: PT_LOAD alignment 0x{minimum:x} is below 0x{MIN_ALIGNMENT:x}"
                )

    if failures:
        print("\n16 KiB ELF alignment validation failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print(f"\n16 KiB ELF alignment validation passed for {libraries} native libraries.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path, help="APK to inspect")
    args = parser.parse_args()
    if not args.apk.is_file():
        parser.error(f"APK not found: {args.apk}")
    return validate_apk(args.apk)


if __name__ == "__main__":
    raise SystemExit(main())
