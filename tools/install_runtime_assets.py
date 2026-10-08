#!/usr/bin/env python3
"""Install locally decoded Never Gone assets into a debuggable recomp APK.

The source tree must be produced locally from the user's original APK, for
example with tools/asset_decoder.py --output build/decoded-assets. This helper
streams assets through `adb exec-out run-as` into the app's private files tree;
no proprietary assets are committed to Git.
"""
from __future__ import annotations

import argparse
import io
import shutil
import subprocess
import tarfile
from pathlib import Path

DEFAULT_PACKAGE = "org.nevergone.recomp"


def run(args: list[str], *, input_bytes: bytes | None = None) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        args,
        input=input_bytes,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )


def build_tar(source_assets: Path) -> bytes:
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w") as archive:
        for path in sorted(source_assets.rglob("*")):
            if not path.is_file():
                continue
            archive.add(path, arcname=path.relative_to(source_assets).as_posix(), recursive=False)
    return buffer.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "source",
        type=Path,
        help="decoded assets directory (normally build/decoded-assets/assets)",
    )
    parser.add_argument("--package", default=DEFAULT_PACKAGE)
    parser.add_argument("--adb", default="adb", help="adb executable")
    args = parser.parse_args()

    source = args.source.resolve()
    if not source.is_dir():
        raise SystemExit(f"decoded assets directory not found: {source}")
    if not (source / "Script" / "Game" / "StartLua.lua").is_file():
        raise SystemExit(
            "StartLua.lua not found; pass the decoded assets/ directory, not its parent"
        )
    if shutil.which(args.adb) is None:
        raise SystemExit(f"adb not found: {args.adb}")

    run([args.adb, "get-state"])
    run(
        [
            args.adb,
            "shell",
            "run-as",
            args.package,
            "sh",
            "-c",
            "rm -rf files/assets && mkdir -p files/assets",
        ]
    )

    payload = build_tar(source)
    result = run(
        [
            args.adb,
            "exec-out",
            "run-as",
            args.package,
            "sh",
            "-c",
            "tar -xf - -C files/assets",
        ],
        input_bytes=payload,
    )

    verify = run(
        [
            args.adb,
            "shell",
            "run-as",
            args.package,
            "sh",
            "-c",
            "test -f files/assets/Script/Game/StartLua.lua && echo OK",
        ]
    )
    if verify.stdout.decode("utf-8", errors="replace").strip() != "OK":
        raise SystemExit("asset installation verification failed")

    print(f"installed decoded assets into {args.package}:files/assets")
    print(f"streamed tar size: {len(payload)} bytes")
    if result.stderr:
        print(result.stderr.decode("utf-8", errors="replace"), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
