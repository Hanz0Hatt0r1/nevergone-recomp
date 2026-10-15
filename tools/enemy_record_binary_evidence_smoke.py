#!/usr/bin/env python3

from __future__ import annotations

import struct

import enemy_record_binary_evidence as evidence


def synthetic_elf() -> bytes:
    segment_vaddr = 0x002A0000
    segment_offset = 0x100
    segment_size = 0x30000
    blob = bytearray(segment_offset + segment_size)

    ident = bytearray(16)
    ident[:4] = b"\x7fELF"
    ident[4] = 1
    ident[5] = 1
    ident[6] = 1
    header = struct.pack(
        "<16sHHIIIIIHHHHHH",
        bytes(ident),
        3,
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
        start = segment_offset + (signature.vaddr - segment_vaddr)
        blob[start : start + len(signature.data)] = signature.data
    return bytes(blob)


def main() -> None:
    image = evidence.Elf32Image(synthetic_elf())
    results = evidence.verify_signatures(image.read)
    assert results
    assert all(match for _signature, match, _actual in results)
    assert evidence.mapping_available(evidence.EXPECTED_SHA256, results)

    report = evidence.render("synthetic", evidence.EXPECTED_SHA256, results)
    assert "all signatures match: yes" in report
    assert "enemy_id: first_i32" in report
    assert "base_health: second_i32" in report
    assert "total-blood mirror: second_i32" in report

    first = evidence.SIGNATURES[0]
    corrupt = bytearray(synthetic_elf())
    offset = 0x100 + (first.vaddr - 0x002A0000)
    corrupt[offset] ^= 0xFF
    bad_results = evidence.verify_signatures(evidence.Elf32Image(bytes(corrupt)).read)
    assert not evidence.mapping_available(evidence.EXPECTED_SHA256, bad_results)

    wrong_hash = evidence.render("synthetic", "0" * 64, results)
    assert "hash matches: no" in wrong_hash
    assert "resolved mapping: unavailable" in wrong_hash

    print("Enemy record ARMv7 evidence smoke: PASS")


if __name__ == "__main__":
    main()
