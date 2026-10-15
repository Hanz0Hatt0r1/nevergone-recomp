#!/usr/bin/env python3
"""Verify tutorial scene-limit field offsets/order in the shipped ARMv7 binary.

The verifier accepts either the original APK or an extracted libcocos2dcpp.so.
It checks the exact known library SHA-256, maps ELF virtual addresses through
PT_LOAD segments, and verifies bounded Thumb signatures inside
LessonLayer::nextLesson().  This proves raw field offsets and comparison order;
it does not manufacture current runtime values for those fields.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

EXPECTED_SHA256 = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"
APK_MEMBER = "lib/armeabi-v7a/libcocos2dcpp.so"
PT_LOAD = 1
EM_ARM = 40

VERTICAL_EDGE_A_FIELD_OFFSET = 0x4AC
VERTICAL_EDGE_B_FIELD_OFFSET = 0x4B0
RIGHT_EDGE_X_FIELD_OFFSET = 0x4B4


@dataclass(frozen=True)
class Signature:
    name: str
    vaddr: int
    data: bytes
    meaning: str


SIGNATURES = (
    Signature(
        "GameScene current-level pointer chain",
        0x003E2156,
        bytes.fromhex("d0 f8 38 32 28 46 d3 f8 20 90"),
        "load GameScene +0x238, then dereference +0x20 into the scene-limit owner",
    ),
    Signature(
        "vertical proximity threshold",
        0x003E2128,
        bytes.fromhex("b3 ee 04 8a"),
        "load the recovered 20.0f vertical-nearness threshold",
    ),
    Signature(
        "right-edge field load",
        0x003E2172,
        bytes.fromhex("d9 f8 b4 34 20 46 05 ee 90 3a"),
        "load owner +0x4b4 before the right-side room comparison",
    ),
    Signature(
        "first vertical field load",
        0x003E21C2,
        bytes.fromhex("d9 f8 b0 34 dd ed 03 7a 05 ee 90 3a"),
        "first vertical proximity branch loads owner +0x4b0",
    ),
    Signature(
        "second vertical field load",
        0x003E2224,
        bytes.fromhex("d9 f8 ac 94 37 ee 09 7a 20 46 06 ee"),
        "fallback vertical proximity branch loads owner +0x4ac",
    ),
    Signature(
        "mirrored first vertical field load",
        0x003E22CC,
        bytes.fromhex("d9 f8 b0 34 dd ed 03 7a 05 ee 90 3a"),
        "mirrored path repeats +0x4b0 as the first vertical branch",
    ),
    Signature(
        "mirrored second vertical field load",
        0x003E233E,
        bytes.fromhex("d9 f8 ac 94 37 ee 49 7a 20 46 06 ee"),
        "mirrored path repeats +0x4ac as the second vertical branch",
    ),
    Signature(
        "horizontal room literal",
        0x003E2428,
        bytes.fromhex("00 00 fa 43"),
        "500.0f literal used with the +0x4b4 right-edge field",
    ),
)


class Elf32Image:
    def __init__(self, blob: bytes):
        self.blob = blob
        if len(blob) < 52 or blob[:4] != b"\x7fELF":
            raise ValueError("not an ELF image")
        if blob[4] != 1:
            raise ValueError("expected ELF32")
        if blob[5] != 1:
            raise ValueError("expected little-endian ELF")

        header = struct.unpack_from("<16sHHIIIIIHHHHHH", blob, 0)
        if header[2] != EM_ARM:
            raise ValueError(f"expected ARM e_machine={EM_ARM}, got {header[2]}")
        phoff = header[5]
        phentsize = header[9]
        phnum = header[10]
        if phentsize < 32:
            raise ValueError(f"invalid ELF32 program-header size: {phentsize}")

        self.loads: list[tuple[int, int, int]] = []
        for index in range(phnum):
            offset = phoff + index * phentsize
            if offset + 32 > len(blob):
                raise ValueError("truncated ELF program headers")
            p_type, p_offset, p_vaddr, _p_paddr, p_filesz, _p_memsz, _flags, _align = (
                struct.unpack_from("<IIIIIIII", blob, offset)
            )
            if p_type != PT_LOAD or p_filesz == 0:
                continue
            if p_offset + p_filesz > len(blob):
                raise ValueError("PT_LOAD extends beyond file")
            self.loads.append((p_vaddr, p_offset, p_filesz))
        if not self.loads:
            raise ValueError("ELF has no file-backed PT_LOAD segment")

    def read(self, vaddr: int, size: int) -> bytes:
        for segment_vaddr, segment_offset, segment_size in self.loads:
            relative = vaddr - segment_vaddr
            if relative < 0 or relative + size > segment_size:
                continue
            start = segment_offset + relative
            return self.blob[start : start + size]
        raise ValueError(f"virtual address {vaddr:#x}+{size:#x} is not file-backed")


def read_library(path: Path) -> tuple[bytes, str]:
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            try:
                return archive.read(APK_MEMBER), f"{path}:{APK_MEMBER}"
            except KeyError as exc:
                raise ValueError(f"APK is missing {APK_MEMBER}") from exc
    return path.read_bytes(), str(path)


def verify_signatures(reader: Callable[[int, int], bytes]) -> list[tuple[Signature, bool, bytes]]:
    results: list[tuple[Signature, bool, bytes]] = []
    for signature in SIGNATURES:
        actual = reader(signature.vaddr, len(signature.data))
        results.append((signature, actual == signature.data, actual))
    return results


def mapping_available(digest: str, results: list[tuple[Signature, bool, bytes]]) -> bool:
    return digest == EXPECTED_SHA256 and all(match for _signature, match, _actual in results)


def render(source: str, digest: str, results: list[tuple[Signature, bool, bytes]]) -> str:
    resolved = mapping_available(digest, results)
    lines = [
        "Tutorial scene-limit ARMv7 evidence",
        f"source: {source}",
        f"libcocos2dcpp.so sha256: {digest}",
        f"expected sha256: {EXPECTED_SHA256}",
        f"hash matches: {'yes' if digest == EXPECTED_SHA256 else 'no'}",
    ]
    for signature, matched, actual in results:
        lines.append(
            f"{signature.vaddr:#010x} {signature.name}: "
            f"{'match' if matched else 'MISMATCH'}"
        )
        lines.append(f"  meaning: {signature.meaning}")
        if not matched:
            lines.append(f"  expected: {signature.data.hex(' ')}")
            lines.append(f"  actual:   {actual.hex(' ')}")
    lines.append(f"all signatures match: {'yes' if all(x[1] for x in results) else 'no'}")
    if resolved:
        lines.extend(
            [
                "resolved field mapping:",
                "  vertical_edge_b: owner +0x4b0 (first vertical comparison)",
                "  vertical_edge_a: owner +0x4ac (second vertical comparison)",
                "  right_edge_x: owner +0x4b4 (500.0f right-side room comparison)",
                "runtime values resolved: no",
            ]
        )
    else:
        lines.append("resolved field mapping: unavailable")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary_or_apk", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    try:
        blob, source = read_library(args.binary_or_apk)
        digest = hashlib.sha256(blob).hexdigest()
        image = Elf32Image(blob)
        results = verify_signatures(image.read)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.error(str(error))

    report = render(source, digest, results)
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0 if mapping_available(digest, results) else 2


if __name__ == "__main__":
    raise SystemExit(main())
