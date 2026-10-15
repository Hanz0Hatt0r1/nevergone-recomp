#!/usr/bin/env python3

from __future__ import annotations

import struct

import player_face_dir_binary_evidence as evidence


def synthetic_elf() -> bytes:
    segment_vaddr = 0x00310000
    segment_offset = 0x100
    segment_size = 0x10000
    blob = bytearray(segment_offset + segment_size)

    ident = bytearray(16)
    ident[:4] = b"\x7fELF"
    ident[4] = 1  # ELF32
    ident[5] = 1  # little endian
    ident[6] = 1  # current version
    header = struct.pack(
        "<16sHHIIIIIHHHHHH",
        bytes(ident),
        3,  # ET_DYN
        evidence.EM_ARM,
        1,
        0,
        52,
        0,
        0,
        52,
        32,
        1,
        0,
        0,
        0,
    )
    blob[: len(header)] = header
    phdr = struct.pack(
        "<IIIIIIII",
        evidence.PT_LOAD,
        segment_offset,
        segment_vaddr,
        segment_vaddr,
        segment_size,
        segment_size,
        5,
        0x1000,
    )
    blob[52 : 52 + len(phdr)] = phdr

    for signature in evidence.SIGNATURES:
        relative = signature.vaddr - segment_vaddr
        start = segment_offset + relative
        blob[start : start + len(signature.data)] = signature.data
    return bytes(blob)


def main() -> None:
    image = evidence.Elf32Image(synthetic_elf())
    results = evidence.verify_signatures(image.read)
    assert results
    assert all(match for _signature, match, _actual in results)

    first = evidence.SIGNATURES[0]
    corrupt = bytearray(synthetic_elf())
    offset = 0x100 + (first.vaddr - 0x00310000)
    corrupt[offset] ^= 0xFF
    bad_image = evidence.Elf32Image(bytes(corrupt))
    bad_results = evidence.verify_signatures(bad_image.read)
    assert not bad_results[0][1]
    assert any(not match for _signature, match, _actual in bad_results)

    report = evidence.render("synthetic", evidence.EXPECTED_SHA256, results)
    assert "all signatures match: yes" in report
    assert "CONTROLSSTATE 0x12 -> PLAYERFACEDIR 1" in report
    assert "CONTROLSSTATE 0x13 -> PLAYERFACEDIR 0" in report

    print("PLAYERFACEDIR binary evidence smoke: PASS")


if __name__ == "__main__":
    main()
