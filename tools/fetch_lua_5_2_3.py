#!/usr/bin/env python3
"""Download and verify the exact Lua runtime embedded by Never Gone.

The original Android native library contains the Lua 5.2.3 version banner. This
helper downloads the official release tarball from lua.org, verifies its SHA-256,
and extracts it into a local ignored third-party directory for Android/CMake
builds. The Lua sources are not committed by this repository.
"""
from __future__ import annotations

import argparse
import hashlib
import shutil
import tarfile
import tempfile
import urllib.request
from pathlib import Path

URL = "https://www.lua.org/ftp/lua-5.2.3.tar.gz"
SHA256 = "13c2fb97961381f7d06d5b5cea55b743c163800896fd5c5e2356201d3619002d"
DIRNAME = "lua-5.2.3"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def safe_extract(archive: tarfile.TarFile, destination: Path) -> None:
    destination = destination.resolve()
    for member in archive.getmembers():
        target = (destination / member.name).resolve()
        if destination != target and destination not in target.parents:
            raise RuntimeError(f"unsafe tar path: {member.name}")
    archive.extractall(destination)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--destination",
        type=Path,
        default=Path("third_party/_local"),
        help="parent directory for lua-5.2.3 (default: third_party/_local)",
    )
    parser.add_argument("--force", action="store_true", help="replace an existing checkout")
    args = parser.parse_args()

    destination = args.destination.resolve()
    target = destination / DIRNAME
    if target.exists():
        if not args.force:
            print(f"Lua 5.2.3 already exists: {target}")
            return 0
        shutil.rmtree(target)

    destination.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="nevergone-lua-") as temp_name:
        temp = Path(temp_name)
        archive_path = temp / f"{DIRNAME}.tar.gz"
        print(f"downloading {URL}")
        urllib.request.urlretrieve(URL, archive_path)

        actual = sha256_file(archive_path)
        if actual != SHA256:
            raise SystemExit(
                "Lua archive checksum mismatch:\n"
                f"  expected: {SHA256}\n"
                f"  actual:   {actual}"
            )

        with tarfile.open(archive_path, "r:gz") as archive:
            safe_extract(archive, destination)

    if not (target / "src" / "lua.h").is_file():
        raise SystemExit(f"extraction completed but lua.h was not found under {target}")

    print(f"Lua 5.2.3 ready: {target}")
    print(f"verified SHA-256: {SHA256}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
