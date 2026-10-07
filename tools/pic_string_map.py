#!/usr/bin/env python3
"""Recover PC-relative C string references from a Thumb function in an ELF.

This is intentionally small and tailored to the code-generation pattern seen in
Never Gone's ARMv7 binary: a literal-pool load into a register followed by
`add <reg>, pc`. It is useful for recovering paths/config names from functions
such as AppDelegate::AddAllSearchPath without relying on a Ghidra project.
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import struct
import subprocess
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass
class PicString:
    instruction_address: str
    literal_address: str
    target_address: str
    register: str
    value: str


def run(args: list[str], *, input_text: str | None = None) -> str:
    return subprocess.check_output(
        args, input=input_text, text=True, errors="replace", stderr=subprocess.STDOUT
    )


def require(name: str) -> str:
    path = shutil.which(name)
    if not path:
        raise SystemExit(f"required tool not found on PATH: {name}")
    return path


def load_segments(elf: Path) -> list[tuple[int, int, int, int]]:
    text = run([require("readelf"), "-lW", str(elf)])
    segments = []
    for line in text.splitlines():
        parts = line.split()
        if not parts or parts[0] != "LOAD" or len(parts) < 6:
            continue
        offset = int(parts[1], 16)
        vaddr = int(parts[2], 16)
        filesz = int(parts[4], 16)
        memsz = int(parts[5], 16)
        segments.append((vaddr, vaddr + memsz, offset, filesz))
    if not segments:
        raise SystemExit("no PT_LOAD segments found")
    return segments


def vaddr_to_offset(address: int, segments: list[tuple[int, int, int, int]]) -> int:
    for start, end, offset, filesz in segments:
        if start <= address < end:
            delta = address - start
            if delta >= filesz:
                raise ValueError(f"address 0x{address:x} is in zero-filled segment tail")
            return offset + delta
    raise ValueError(f"address 0x{address:x} is not mapped by a PT_LOAD segment")


def find_symbol(elf: Path, symbol_query: str) -> tuple[int, int, str]:
    raw = run([require("readelf"), "-Ws", str(elf)])
    filt = require("c++filt")
    candidates = []
    for line in raw.splitlines():
        parts = line.split(None, 7)
        if len(parts) != 8 or parts[3] != "FUNC" or parts[6] == "UND":
            continue
        value = int(parts[1], 16)
        size = int(parts[2])
        mangled = parts[7].split("@", 1)[0]
        demangled = run([filt, mangled]).strip()
        if symbol_query == mangled or symbol_query == demangled or symbol_query in demangled:
            candidates.append((value & ~1, size, demangled))
    if not candidates:
        raise SystemExit(f"function not found: {symbol_query}")
    exact = [row for row in candidates if row[2] == symbol_query]
    chosen = exact[0] if exact else candidates[0]
    if chosen[1] <= 0:
        raise SystemExit(f"function has no usable symbol size: {chosen[2]}")
    return chosen


def disassemble(elf: Path, start: int, size: int) -> str:
    objdump = shutil.which("llvm-objdump")
    if not objdump:
        for candidate in (
            "/usr/local/swift/usr/bin/llvm-objdump",
            "/usr/bin/llvm-objdump",
        ):
            if Path(candidate).exists():
                objdump = candidate
                break
    if not objdump:
        raise SystemExit("llvm-objdump is required")
    return run([
        objdump,
        "-d",
        "--triple=thumbv7-none-linux-android",
        f"--start-address=0x{start:x}",
        f"--stop-address=0x{start + size:x}",
        str(elf),
    ])


def printable_c_string(blob: bytes, offset: int, max_len: int = 512) -> str | None:
    if not (0 <= offset < len(blob)):
        return None
    end = blob.find(b"\x00", offset, min(len(blob), offset + max_len))
    if end < 0 or end == offset:
        return "" if end == offset else None
    raw = blob[offset:end]
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        return None
    if any(ord(ch) < 0x20 and ch not in "\t\r\n" for ch in text):
        return None
    return text


def scan(elf: Path, symbol_query: str) -> tuple[str, list[PicString]]:
    start, size, display_name = find_symbol(elf, symbol_query)
    dis = disassemble(elf, start, size)
    blob = elf.read_bytes()
    segments = load_segments(elf)
    loads: dict[str, int] = {}
    results: list[PicString] = []
    load_re = re.compile(
        r"^\s*([0-9a-f]+):.*?\bldr(?:\.w)?\s+(r\d+|r9|r10|r11),\s*"
        r"\[pc[^]]*\].*?@\s*0x([0-9a-f]+)", re.I
    )
    add_re = re.compile(
        r"^\s*([0-9a-f]+):.*?\badd\s+(r\d+|r9|r10|r11),\s*pc\b", re.I
    )

    for line in dis.splitlines():
        match = load_re.search(line)
        if match:
            loads[match.group(2).lower()] = int(match.group(3), 16)
            continue
        match = add_re.search(line)
        if not match:
            continue
        instruction = int(match.group(1), 16)
        reg = match.group(2).lower()
        literal_vaddr = loads.get(reg)
        if literal_vaddr is None:
            continue
        try:
            literal_off = vaddr_to_offset(literal_vaddr, segments)
        except ValueError:
            continue
        if literal_off + 4 > len(blob):
            continue
        displacement = struct.unpack_from("<i", blob, literal_off)[0]
        # For this high-register ADD encoding, architectural PC is the current
        # instruction address + 4 rather than the word-aligned ADR semantics.
        target_vaddr = (instruction + 4 + displacement) & 0xFFFFFFFF
        try:
            target_off = vaddr_to_offset(target_vaddr, segments)
        except ValueError:
            continue
        value = printable_c_string(blob, target_off)
        if value is None:
            continue
        results.append(PicString(
            instruction_address=f"0x{instruction:08x}",
            literal_address=f"0x{literal_vaddr:08x}",
            target_address=f"0x{target_vaddr:08x}",
            register=reg,
            value=value,
        ))
    return display_name, results


def markdown(name: str, rows: list[PicString]) -> str:
    out = [
        "# PIC string map",
        "",
        f"Function: `{name}`",
        "",
        f"Recovered references: **{len(rows)}**",
        "",
        "| Instruction | Literal | Target | Register | String |",
        "| --- | --- | --- | --- | --- |",
    ]
    for row in rows:
        safe = row.value.replace("|", "\\|").replace("`", "\\`")
        out.append(
            f"| `{row.instruction_address}` | `{row.literal_address}` | "
            f"`{row.target_address}` | `{row.register}` | `{safe}` |"
        )
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("function", help="mangled, demangled, or unique function-name substring")
    parser.add_argument("--json", dest="json_path", type=Path)
    parser.add_argument("--markdown", dest="markdown_path", type=Path)
    args = parser.parse_args()

    name, rows = scan(args.elf, args.function)
    if args.json_path:
        args.json_path.parent.mkdir(parents=True, exist_ok=True)
        args.json_path.write_text(
            json.dumps({"function": name, "references": [asdict(row) for row in rows]}, indent=2) + "\n",
            encoding="utf-8",
        )
    if args.markdown_path:
        args.markdown_path.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_path.write_text(markdown(name, rows), encoding="utf-8")
    if not args.json_path and not args.markdown_path:
        print(markdown(name, rows), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
