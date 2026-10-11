#!/usr/bin/env python3
"""Capture private runtime memory from the original Android game."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import shutil
from datetime import datetime, timezone


PACKAGE = "com.hippiegame.nevergone"
MODULES = ("libcocos2dcpp.so", "libGLES_mali.so", "libEGL.so")
MAX_SEGMENT_BYTES = 32 * 1024 * 1024


def adb(*args: str, capture: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["adb", *args],
        check=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True,
                        help="new, empty local directory for the private dump")
    parser.add_argument("--full", action="store_true",
                        help="dump every readable process mapping, including heap and stacks")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error(f"output directory is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(output, 0o700)

    rooted = False
    try:
        adb("root", capture=False)
        rooted = True
        adb("wait-for-device", capture=False)
        pids = adb("shell", "pidof", PACKAGE).stdout.decode().strip().split()
        if not pids:
            parser.error(f"{PACKAGE} is not running; open the game first")
        pid = pids[0]
        maps = adb("shell", "cat", f"/proc/{pid}/maps").stdout.decode()
        (output / "maps.txt").write_text(maps)
        try:
            rollup = adb("shell", "cat", f"/proc/{pid}/smaps_rollup").stdout
            (output / "smaps_rollup.txt").write_bytes(rollup)
        except subprocess.CalledProcessError:
            pass

        screenshot = adb("exec-out", "screencap", "-p").stdout
        (output / "frame.png").write_bytes(screenshot)

        segments = []
        failures = []
        attempted = 0
        for line in maps.splitlines():
            if not args.full and not any(module in line for module in MODULES):
                continue
            fields = line.split()
            address, permissions = fields[:2]
            if "r" not in permissions:
                continue
            module = Path(fields[-1]).name
            if not args.full and module != "libcocos2dcpp.so" and "w" not in permissions:
                continue
            start, end = (int(value, 16) for value in address.split("-"))
            size = end - start
            if size <= 0 or size % 4096 or (not args.full and size > MAX_SEGMENT_BYTES):
                continue
            attempted += 1
            name = (f"mem.{start:08x}-{end:08x}.bin" if args.full else
                    f"{module}.{start:08x}-{end:08x}.{permissions}.bin")
            destination = output / name
            temporary = output / f"{name}.part"
            if shutil.disk_usage(output).free < size + 1024 * 1024 * 1024:
                raise RuntimeError(f"less than 1 GiB free after reserving {size} bytes for {name}")
            with temporary.open("wb") as handle:
                result = subprocess.run(
                    ["adb", "exec-out", "dd", f"if=/proc/{pid}/mem", "bs=4096",
                     f"skip={start // 4096}", f"count={size // 4096}", "status=none"],
                    stdout=handle, stderr=subprocess.PIPE,
                )
            actual = temporary.stat().st_size
            if result.returncode or actual != size:
                temporary.unlink()
                error = result.stderr.decode(errors="replace").strip()
                if not error:
                    error = (f"dd exited {result.returncode}; "
                             f"/proc/{pid}/mem returned {actual} of {size} bytes")
                failures.append({"start": f"0x{start:08x}", "end": f"0x{end:08x}",
                                 "permissions": permissions, "mapping": line,
                                 "error": error,
                                 "bytes_read": actual, "expected_bytes": size})
                if not args.full:
                    raise RuntimeError(f"failed to read {name}: {actual} of {size} bytes")
                continue
            temporary.rename(destination)
            digest = hashlib.sha256()
            with destination.open("rb") as handle:
                for block in iter(lambda: handle.read(1024 * 1024), b""):
                    digest.update(block)
            segments.append({"file": name, "start": f"0x{start:08x}",
                             "end": f"0x{end:08x}", "permissions": permissions,
                             "mapping": line, "bytes": size,
                             "sha256": digest.hexdigest()})
            if args.full and attempted % 100 == 0:
                print(f"Processed {attempted} mappings; captured {len(segments)}, "
                      f"failed {len(failures)}", flush=True)

        manifest = {"package": PACKAGE, "pid": pid,
                    "mode": "full" if args.full else "render",
                    "captured_utc": datetime.now(timezone.utc).isoformat(),
                    "screenshot_sha256": hashlib.sha256(screenshot).hexdigest(),
                    "segments": segments, "failed_mappings": failures}
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(f"Captured {len(segments)} segments ({sum(s['bytes'] for s in segments)} bytes) "
              f"to {output}; failed mappings: {len(failures)}")
    finally:
        if rooted:
            adb("unroot", capture=False)
            adb("wait-for-device", capture=False)


if __name__ == "__main__":
    main()
