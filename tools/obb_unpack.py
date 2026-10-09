#!/usr/bin/env python3
"""Inspect and unpack Android expansion OBB files.

The tool is intentionally conservative:
- standard ZIP/TAR expansion files are extracted with path traversal checks;
- Android JOBB footers are decoded (signature 0x01059983);
- unencrypted JOBB FAT payloads can be extracted through `jobb`, `mcopy`, or `7z`;
- encrypted JOBB files are identified and can be delegated to `jobb -dump -k ...`;
- unknown/custom containers produce a compact diagnostic report and small samples
  instead of pretending that a guessed format is understood.

No original game data is stored by this tool unless the user explicitly asks it
for extraction or diagnostic samples.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from collections import Counter
from typing import BinaryIO, Iterable

JOBB_SIGNATURE = 0x01059983
JOBB_FOOTER_TAG_SIZE = 8
JOBB_MAX_FOOTER_SIZE = 32768
JOBB_OVERLAY = 1 << 0
JOBB_SALTED = 1 << 1

CHUNK_SIZE = 1024 * 1024
SAMPLE_SIZE = 1024 * 1024
MAX_HITS_PER_PATTERN = 64

DEFAULT_NEEDLES = (
    b"ServerList",
    b"NewServerList",
    b"btn_standard",
    b"gamescene_ui",
    b"Login/ChooseHero",
    b"Gate_Background",
    b".plist",
    b".png",
    b".lua",
    b".csv",
    b".xml",
    b".ogg",
    b".mp3",
    b".bff",
)

SIGNATURES = {
    "zip_local": b"PK\x03\x04",
    "zip_eocd": b"PK\x05\x06",
    "png": b"\x89PNG\r\n\x1a\n",
    "ogg": b"OggS",
    "riff": b"RIFF",
    "dds": b"DDS ",
    "pvr3": b"PVR\x03",
    "sqlite": b"SQLite format 3\x00",
    "gzip": b"\x1f\x8b\x08",
    "bzip2": b"BZh",
    "xz": b"\xfd7zXZ\x00",
    "7z": b"7z\xbc\xaf'\x1c",
    "rar4": b"Rar!\x1a\x07\x00",
    "rar5": b"Rar!\x1a\x07\x01\x00",
    "elf": b"\x7fELF",
    "unityfs": b"UnityFS\x00",
}


def _human_size(value: int) -> str:
    units = ("B", "KiB", "MiB", "GiB", "TiB")
    size = float(value)
    for unit in units:
        if size < 1024.0 or unit == units[-1]:
            return f"{size:.2f} {unit}"
        size /= 1024.0
    return f"{value} B"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(CHUNK_SIZE), b""):
            digest.update(block)
    return digest.hexdigest()


def _entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = Counter(data)
    length = len(data)
    return -sum((count / length) * math.log2(count / length) for count in counts.values())


def _read_sample(path: Path, offset: int, length: int) -> bytes:
    with path.open("rb") as stream:
        stream.seek(max(0, offset))
        return stream.read(max(0, length))


def _sample_entropies(path: Path, sample_size: int = SAMPLE_SIZE) -> dict[str, float]:
    size = path.stat().st_size
    head = _read_sample(path, 0, min(sample_size, size))
    middle_offset = max(0, size // 2 - sample_size // 2)
    middle = _read_sample(path, middle_offset, min(sample_size, size))
    tail_offset = max(0, size - sample_size)
    tail = _read_sample(path, tail_offset, min(sample_size, size))
    return {
        "head": round(_entropy(head), 5),
        "middle": round(_entropy(middle), 5),
        "tail": round(_entropy(tail), 5),
    }


def _ascii_context(data: bytes) -> str:
    return "".join(chr(value) if 32 <= value <= 126 else "." for value in data)


def _scan_patterns(
    path: Path,
    patterns: dict[str, bytes],
    limit_per_pattern: int = MAX_HITS_PER_PATTERN,
) -> dict[str, list[int]]:
    if not patterns:
        return {}
    longest = max(len(pattern) for pattern in patterns.values())
    overlap = max(0, longest - 1)
    hits: dict[str, list[int]] = {name: [] for name in patterns}
    previous = b""
    absolute = 0

    with path.open("rb") as stream:
        while True:
            block = stream.read(CHUNK_SIZE)
            if not block:
                break
            data = previous + block
            data_base = absolute - len(previous)
            for name, pattern in patterns.items():
                if len(hits[name]) >= limit_per_pattern:
                    continue
                start = 0
                while len(hits[name]) < limit_per_pattern:
                    index = data.find(pattern, start)
                    if index < 0:
                        break
                    global_offset = data_base + index
                    if global_offset >= 0 and (
                        not hits[name] or hits[name][-1] != global_offset
                    ):
                        hits[name].append(global_offset)
                    start = index + 1
            absolute += len(block)
            previous = data[-overlap:] if overlap else b""

    return {name: offsets for name, offsets in hits.items() if offsets}


def _needle_contexts(path: Path, needles: Iterable[bytes]) -> dict[str, list[dict[str, object]]]:
    patterns = {needle.decode("ascii", errors="replace"): needle for needle in needles}
    hits = _scan_patterns(path, patterns)
    result: dict[str, list[dict[str, object]]] = {}
    for name, offsets in hits.items():
        entries = []
        pattern = patterns[name]
        for offset in offsets:
            context_start = max(0, offset - 80)
            context = _read_sample(path, context_start, 200 + len(pattern))
            entries.append({
                "offset": offset,
                "context": _ascii_context(context),
            })
        result[name] = entries
    return result


def _parse_jobb_footer(path: Path) -> dict[str, object] | None:
    size = path.stat().st_size
    if size < 33:
        return None
    with path.open("rb") as stream:
        stream.seek(size - JOBB_FOOTER_TAG_SIZE)
        tag = stream.read(JOBB_FOOTER_TAG_SIZE)
        if len(tag) != JOBB_FOOTER_TAG_SIZE:
            return None
        footer_size, signature = struct.unpack("<II", tag)
        if signature != JOBB_SIGNATURE:
            return None
        if footer_size < 25 or footer_size > JOBB_MAX_FOOTER_SIZE:
            return {
                "valid": False,
                "error": f"invalid footer size {footer_size}",
                "signature": f"0x{signature:08x}",
            }
        footer_offset = size - JOBB_FOOTER_TAG_SIZE - footer_size
        if footer_offset < 0:
            return {
                "valid": False,
                "error": "footer extends before start of file",
                "signature": f"0x{signature:08x}",
            }
        stream.seek(footer_offset)
        footer = stream.read(footer_size)

    if len(footer) < 24:
        return {
            "valid": False,
            "error": "truncated footer",
            "signature": f"0x{signature:08x}",
        }

    sig_version, package_version, flags = struct.unpack_from("<III", footer, 0)
    salt = footer[12:20]
    package_name_length = struct.unpack_from("<I", footer, 20)[0]
    if package_name_length <= 0 or 24 + package_name_length > len(footer):
        return {
            "valid": False,
            "error": f"invalid package name length {package_name_length}",
            "signature": f"0x{signature:08x}",
        }
    package_name_bytes = footer[24 : 24 + package_name_length]
    package_name = package_name_bytes.decode("utf-8", errors="replace")
    return {
        "valid": sig_version == 1,
        "signature": f"0x{signature:08x}",
        "signature_version": sig_version,
        "package_version": package_version,
        "flags": flags,
        "overlay": bool(flags & JOBB_OVERLAY),
        "salted_encrypted": bool(flags & JOBB_SALTED),
        "salt_hex": salt.hex(),
        "package_name": package_name,
        "footer_size": footer_size,
        "footer_offset": footer_offset,
        "payload_size": footer_offset,
    }


def _looks_like_fat(path: Path, payload_size: int | None = None) -> dict[str, object]:
    size = path.stat().st_size if payload_size is None else min(path.stat().st_size, payload_size)
    if size < 512:
        return {"possible": False}
    sector = _read_sample(path, 0, 512)
    boot_marker = len(sector) == 512 and sector[510:512] == b"\x55\xaa"
    oem = sector[3:11].decode("ascii", errors="replace").strip() if len(sector) >= 11 else ""
    fat_strings = any(token in sector for token in (b"FAT12", b"FAT16", b"FAT32"))
    return {
        "possible": bool(boot_marker and (fat_strings or oem)),
        "boot_marker_55aa": boot_marker,
        "oem": oem,
        "fat_label_present": fat_strings,
        "payload_size": size,
    }


def _zip_summary(path: Path, max_entries: int = 200) -> dict[str, object] | None:
    if not zipfile.is_zipfile(path):
        return None
    try:
        with zipfile.ZipFile(path) as archive:
            infos = archive.infolist()
            return {
                "entries": len(infos),
                "total_uncompressed": sum(info.file_size for info in infos),
                "total_compressed": sum(info.compress_size for info in infos),
                "sample_entries": [info.filename for info in infos[:max_entries]],
            }
    except (OSError, zipfile.BadZipFile) as exc:
        return {"error": str(exc)}


def _tar_summary(path: Path, max_entries: int = 200) -> dict[str, object] | None:
    try:
        if not tarfile.is_tarfile(path):
            return None
        with tarfile.open(path, "r:*") as archive:
            members = archive.getmembers()
            return {
                "entries": len(members),
                "sample_entries": [member.name for member in members[:max_entries]],
            }
    except (OSError, tarfile.TarError) as exc:
        return {"error": str(exc)}


def inspect(path: Path) -> dict[str, object]:
    size = path.stat().st_size
    jobb = _parse_jobb_footer(path)
    payload_size = None
    if jobb and isinstance(jobb.get("payload_size"), int):
        payload_size = int(jobb["payload_size"])

    signature_hits = _scan_patterns(path, SIGNATURES)
    report: dict[str, object] = {
        "file": str(path.resolve()),
        "size": size,
        "size_human": _human_size(size),
        "sha256": _sha256(path),
        "head_hex": _read_sample(path, 0, 64).hex(),
        "tail_hex": _read_sample(path, max(0, size - 64), 64).hex(),
        "sample_entropy_bits_per_byte": _sample_entropies(path),
        "zip": _zip_summary(path),
        "tar": _tar_summary(path),
        "jobb": jobb,
        "fat": _looks_like_fat(path, payload_size),
        "signature_offsets": signature_hits,
        "interesting_strings": _needle_contexts(path, DEFAULT_NEEDLES),
        "available_helpers": {
            name: shutil.which(name)
            for name in ("7z", "7zz", "mcopy", "jobb", "bsdtar", "unzip")
            if shutil.which(name)
        },
    }
    return report


def _write_diagnostics(path: Path, output_dir: Path, report: dict[str, object]) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    size = path.stat().st_size
    sample_size = min(SAMPLE_SIZE, size)
    (output_dir / "head.bin").write_bytes(_read_sample(path, 0, sample_size))
    (output_dir / "tail.bin").write_bytes(
        _read_sample(path, max(0, size - sample_size), sample_size)
    )
    with (output_dir / "report.json").open("w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")

    lines = []
    interesting = report.get("interesting_strings")
    if isinstance(interesting, dict):
        for needle, entries in interesting.items():
            lines.append(f"[{needle}]")
            if isinstance(entries, list):
                for entry in entries:
                    if isinstance(entry, dict):
                        lines.append(
                            f"0x{int(entry.get('offset', 0)):x}: {entry.get('context', '')}"
                        )
            lines.append("")
    (output_dir / "interesting_strings.txt").write_text(
        "\n".join(lines), encoding="utf-8"
    )


def _safe_destination(root: Path, member_name: str) -> Path:
    normalized = member_name.replace("\\", "/")
    relative = Path(normalized)
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"unsafe archive path: {member_name!r}")
    destination = (root / relative).resolve()
    root_resolved = root.resolve()
    if destination != root_resolved and root_resolved not in destination.parents:
        raise ValueError(f"archive path escapes output directory: {member_name!r}")
    return destination


def _extract_zip(path: Path, output_dir: Path) -> int:
    extracted = 0
    with zipfile.ZipFile(path) as archive:
        for info in archive.infolist():
            destination = _safe_destination(output_dir, info.filename)
            if info.is_dir():
                destination.mkdir(parents=True, exist_ok=True)
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(info, "r") as source, destination.open("wb") as target:
                shutil.copyfileobj(source, target, CHUNK_SIZE)
            extracted += 1
    return extracted


def _extract_tar(path: Path, output_dir: Path) -> int:
    extracted = 0
    with tarfile.open(path, "r:*") as archive:
        for member in archive.getmembers():
            destination = _safe_destination(output_dir, member.name)
            if member.isdir():
                destination.mkdir(parents=True, exist_ok=True)
                continue
            if not member.isfile():
                continue
            source = archive.extractfile(member)
            if source is None:
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            with source, destination.open("wb") as target:
                shutil.copyfileobj(source, target, CHUNK_SIZE)
            extracted += 1
    return extracted


def _run(command: list[str]) -> tuple[bool, str]:
    try:
        result = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
    except OSError as exc:
        return False, str(exc)
    return result.returncode == 0, result.stdout


def _extract_with_7z(path: Path, output_dir: Path, password: str | None) -> tuple[bool, str]:
    executable = shutil.which("7z") or shutil.which("7zz")
    if not executable:
        return False, "7z/7zz not installed"
    command = [executable, "x", str(path), f"-o{output_dir}", "-y"]
    if password is not None:
        command.append(f"-p{password}")
    return _run(command)


def _extract_jobb(
    path: Path,
    output_dir: Path,
    footer: dict[str, object],
    password: str | None,
) -> tuple[bool, str]:
    jobb = shutil.which("jobb")
    encrypted = bool(footer.get("salted_encrypted"))
    if encrypted:
        if not password:
            return False, (
                "JOBB footer marks this file as salted/encrypted; rerun with --password. "
                "The tool will delegate decryption to the Android jobb utility if installed."
            )
        if not jobb:
            return False, (
                "Encrypted JOBB detected but `jobb` is not installed. Install an Android SDK "
                "build-tools package that contains jobb, or send report.json so the game-specific "
                "container path can be investigated."
            )
        return _run([jobb, "-dump", str(path), "-d", str(output_dir), "-k", password])

    if jobb:
        ok, output = _run([jobb, "-dump", str(path), "-d", str(output_dir)])
        if ok:
            return True, output

    payload_size = footer.get("payload_size")
    if not isinstance(payload_size, int) or payload_size <= 0:
        return False, "JOBB footer did not expose a usable payload size"

    with tempfile.TemporaryDirectory(prefix="nevergone-obb-") as temp_dir:
        image = Path(temp_dir) / "payload.img"
        with path.open("rb") as source, image.open("wb") as target:
            remaining = payload_size
            while remaining > 0:
                block = source.read(min(CHUNK_SIZE, remaining))
                if not block:
                    return False, "unexpected EOF while copying JOBB FAT payload"
                target.write(block)
                remaining -= len(block)

        mcopy = shutil.which("mcopy")
        if mcopy:
            output_dir.mkdir(parents=True, exist_ok=True)
            ok, output = _run([mcopy, "-s", "-i", str(image), "::*", str(output_dir)])
            if ok:
                return True, output

        ok, output = _extract_with_7z(image, output_dir, None)
        if ok:
            return True, output

    return False, (
        "Unencrypted JOBB detected, but extraction helpers failed. Install `mtools` "
        "(for mcopy) or `p7zip`/`7zip`, then rerun."
    )


def unpack(path: Path, output_dir: Path, password: str | None) -> tuple[bool, str]:
    output_dir.mkdir(parents=True, exist_ok=True)

    if zipfile.is_zipfile(path):
        count = _extract_zip(path, output_dir)
        return True, f"extracted {count} ZIP files"

    if tarfile.is_tarfile(path):
        count = _extract_tar(path, output_dir)
        return True, f"extracted {count} TAR files"

    footer = _parse_jobb_footer(path)
    if footer and footer.get("valid"):
        return _extract_jobb(path, output_dir, footer, password)

    ok, output = _extract_with_7z(path, output_dir, password)
    if ok:
        return True, output

    return False, (
        "Unknown/custom container. Run the inspect command and send the resulting "
        "report.json plus head.bin/tail.bin (each <= 1 MiB); the full OBB is not needed.\n\n"
        + output
    )


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Inspect/unpack Android OBB expansion files without loading them fully into RAM."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    inspect_parser = subparsers.add_parser(
        "inspect", help="identify the container and create a compact diagnostic bundle"
    )
    inspect_parser.add_argument("obb", type=Path)
    inspect_parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="diagnostic directory (default: <obb-name>.diagnostic)",
    )
    inspect_parser.add_argument(
        "--json",
        action="store_true",
        help="also print the full report JSON to stdout",
    )

    unpack_parser = subparsers.add_parser(
        "unpack", help="extract a recognized ZIP/TAR/JOBB/7z-readable container"
    )
    unpack_parser.add_argument("obb", type=Path)
    unpack_parser.add_argument("-o", "--output", type=Path, required=True)
    unpack_parser.add_argument(
        "--password",
        help="password for encrypted JOBB/archive; passed only to local extraction helper",
    )

    return parser


def main(argv: list[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    path: Path = args.obb
    if not path.is_file():
        print(f"error: file not found: {path}", file=sys.stderr)
        return 2

    if args.command == "inspect":
        output = args.output or Path(f"{path.name}.diagnostic")
        print(f"Inspecting {path} ({_human_size(path.stat().st_size)}) ...")
        report = inspect(path)
        _write_diagnostics(path, output, report)
        jobb = report.get("jobb")
        if isinstance(jobb, dict):
            print(
                "JOBB footer: "
                f"package={jobb.get('package_name')} version={jobb.get('package_version')} "
                f"encrypted={jobb.get('salted_encrypted')}"
            )
        elif report.get("zip"):
            print("Container: ZIP expansion file")
        elif report.get("tar"):
            print("Container: TAR expansion file")
        else:
            print("Container: unknown/custom; diagnostic bundle created")
        print(f"Diagnostic bundle: {output}")
        print("Share report.json first; head.bin/tail.bin are only needed if the report is insufficient.")
        if args.json:
            print(json.dumps(report, indent=2, ensure_ascii=False))
        return 0

    if args.command == "unpack":
        ok, message = unpack(path, args.output, args.password)
        print(message)
        if ok:
            print(f"Output: {args.output}")
            return 0
        print(
            f"\nTip: python3 {Path(__file__).name} inspect {path} -o {path.name}.diagnostic",
            file=sys.stderr,
        )
        return 1

    return 2


if __name__ == "__main__":
    raise SystemExit(main())
