#!/usr/bin/env python3
"""Verify the recovered PLAYERFACEDIR contract in the shipped ARMv7 binary.

The verifier accepts either the original APK or an extracted libcocos2dcpp.so.
It checks the exact known library SHA-256, maps ELF virtual addresses through
PT_LOAD program headers, and verifies the bounded Thumb instruction signatures
used by the clean-room PLAYERFACEDIR provider.
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
        "MEPlayer::isFace field load",
        0x0031C748,
        bytes.fromhex("d0 f8 2c 01"),
        "load MEPlayer +0x12c",
    ),
    Signature(
        "MEPlayer::setPlayerDir enum store",
        0x0031C75A,
        bytes.fromhex("01 29 c0 f8 2c 11"),
        "compare PLAYERFACEDIR with 1 and store it to MEPlayer +0x12c",
    ),
    Signature(
        "MEPlayer::filpPlayer bool store",
        0x0031A4F6,
        bytes.fromhex("19 b1 01 23 c4 f8 2c 31 01 e0 c4 f8 2c 11"),
        "store true as 1 and false as 0 to MEPlayer +0x12c",
    ),
    Signature(
        "setControlsState state 0x12 facing branch",
        0x0031DE36,
        bytes.fromhex("12 2d 0a d1 d4 f8 2c 31 b3 b9 20 46 01 21"),
        "CONTROLSSTATE 0x12 supplies true to filpPlayer",
    ),
    Signature(
        "setControlsState state 0x13 facing branch",
        0x0031DE50,
        bytes.fromhex("13 2d 0c d1 d4 f8 2c 61 01 2e 08 d1 20 46 00 21"),
        "CONTROLSSTATE 0x13 supplies false to filpPlayer",
    ),
    Signature(
        "setControlsState later state 0x12 branch",
        0x0031DF3E,
        bytes.fromhex("12 2b 0c d1 20 46 01 21 05 e0"),
        "later facing maintenance repeats 0x12 -> true",
    ),
    Signature(
        "setControlsState later state 0x13 branch",
        0x0031DF4C,
        bytes.fromhex("13 2b 05 d1 20 46 00 21"),
        "later facing maintenance repeats 0x13 -> false",
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
        machine = header[2]
        if machine != EM_ARM:
            raise ValueError(f"expected ARM e_machine={EM_ARM}, got {machine}")
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
        if size < 0:
            raise ValueError("negative read size")
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


def render(source: str, digest: str, results: list[tuple[Signature, bool, bytes]]) -> str:
    all_match = all(matched for _signature, matched, _actual in results)
    lines = [
        "PLAYERFACEDIR binary evidence",
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
    lines.extend(
        [
            f"all signatures match: {'yes' if all_match else 'no'}",
            "resolved mapping: CONTROLSSTATE 0x12 -> PLAYERFACEDIR 1; "
            "CONTROLSSTATE 0x13 -> PLAYERFACEDIR 0"
            if all_match and digest == EXPECTED_SHA256
            else "resolved mapping: unavailable",
        ]
    )
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

    return 0 if digest == EXPECTED_SHA256 and all(match for _sig, match, _actual in results) else 2


if __name__ == "__main__":
    raise SystemExit(main())
