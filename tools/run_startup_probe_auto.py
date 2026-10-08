#!/usr/bin/env python3
"""Prepare verified Lua 5.2.3 automatically and run the Never Gone startup probe."""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LUA_ROOT = ROOT / "third_party" / "_local" / "lua-5.2.3"
LUA_EXE = LUA_ROOT / "src" / "lua"
FETCH_LUA = ROOT / "tools" / "fetch_lua_5_2_3.py"
STARTUP_PROBE = ROOT / "tools" / "run_startup_probe.py"


def ensure_host_lua() -> Path:
    if LUA_EXE.is_file():
        return LUA_EXE

    subprocess.run([sys.executable, str(FETCH_LUA)], cwd=ROOT, check=True)

    make = shutil.which("make")
    if make is None:
        raise SystemExit("make was not found; install base-devel/build tools before running the probe")

    subprocess.run([make, "-C", str(LUA_ROOT), "generic"], cwd=ROOT, check=True)
    if not LUA_EXE.is_file():
        raise SystemExit(f"Lua build completed but executable was not found: {LUA_EXE}")
    return LUA_EXE


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path, help="legally obtained original Never Gone APK")
    parser.add_argument("--keep-temp", action="store_true", help="keep decoded temporary scripts for debugging")
    args = parser.parse_args()

    if not args.apk.is_file():
        raise SystemExit(f"APK not found: {args.apk}")

    lua = ensure_host_lua()
    command = [
        sys.executable,
        str(STARTUP_PROBE),
        str(args.apk.resolve()),
        "--lua",
        str(lua),
        "--compat-source",
        str(ROOT / "android" / "app" / "src" / "main" / "cpp" / "lua_compat.cpp"),
    ]
    if args.keep_temp:
        command.append("--keep-temp")

    return subprocess.run(command, cwd=ROOT, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
