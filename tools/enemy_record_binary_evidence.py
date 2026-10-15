#!/usr/bin/env python3
"""Verify the recovered GameLevels enemy identity/base-health mapping.

The verifier accepts either the original Never Gone APK or an extracted
libcocos2dcpp.so. It requires the exact known ARMv7 library SHA-256 and checks
bounded Thumb signatures connecting LoadGL_Global's serialized EnemyRecord to
GameLevelsEnemyData consumers.
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


@dataclass(frozen=True)
class Signature:
    name: str
    vaddr: int
    data: bytes
    meaning: str


SIGNATURES = (
    Signature(
        "LoadGL_Global identity/health stores",
        0x002C1FC0,
        bytes.fromhex(
            "03 68 03 98 4b 61 da f8 00 30 0b 62 13 68 "
            "04 9a 8b 61 cb 61 03 68 05 98 4b 62"
        ),
        "first serialized int32 -> GameLevelsEnemyData +0x14; "
        "second serialized int32 -> +0x18 and duplicate +0x1c",
    ),
    Signature(
        "GetEnemyDataWithEnemyID identity compare",
        0x002C261C,
        bytes.fromhex("43 69 b3 42 02 d0 01"),
        "load GameLevelsEnemyData +0x14 and compare it with requested enemy ID",
    ),
    Signature(
        "EnemyObject base-health load",
        0x002AE76E,
        bytes.fromhex(
            "91 ed 06 7a f8 ee c7 7a 08 9d 04 46 08 ee 90 2a "
            "1e 46 c0 ed 59 7a c0 ed 58 7a"
        ),
        "load signed GameLevelsEnemyData +0x18, convert to float, and seed both health stores",
    ),
    Signature(
        "GetLevelALLEnemyBlood corroboration",
        0x002C2654,
        bytes.fromhex("01 34 c3 69 1d 44 f2 e7"),
        "sum GameLevelsEnemyData +0x1c, which LoadGL_Global duplicates from the second int32",
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
        "Enemy record ARMv7 evidence",
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
    lines.append(f"all signatures match: {'yes' if all(row[1] for row in results) else 'no'}")
    if resolved:
        lines.extend(
            [
                "resolved mapping:",
                "  enemy_id: first_i32 -> GameLevelsEnemyData +0x14",
                "  base_health: second_i32 -> GameLevelsEnemyData +0x18",
                "  total-blood mirror: second_i32 -> GameLevelsEnemyData +0x1c",
            ]
        )
    else:
        lines.append("resolved mapping: unavailable")
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
