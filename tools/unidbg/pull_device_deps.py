#!/usr/bin/env python3
"""Copy 32-bit Android ELF dependencies from the USB device into device-libs/."""

import os
import shutil
import hashlib
import json
import re
import subprocess
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCE = Path(os.environ["NEVERGONE_LIB_DIR"])
DEST = ROOT / "device-libs"
RUNTIME = ROOT / "device-runtime"
ADB = Path(os.environ.get("ADB", shutil.which("adb") or str(ROOT / "tools" / "adb")))
NAME_RE = re.compile(r"^[A-Za-z0-9_.@+\-]+\.so$")
NEEDED_RE = re.compile(r"\(NEEDED\).*?\[(.*?)\]")
# These are provided by unidbg's SDK 23 resolver, not copied from the phone.
BUNDLED = {"libc.so", "libm.so", "libdl.so", "liblog.so", "libz.so",
           "libstdc++.so", "libc++.so"}
RUNTIME_SOURCES = {
    "libc.so": "/apex/com.android.runtime/lib/bionic/libc.so",
    "libm.so": "/apex/com.android.runtime/lib/bionic/libm.so",
    "libdl.so": "/apex/com.android.runtime/lib/bionic/libdl.so",
    "libc++.so": "/system/lib/libc++.so",
    "liblog.so": "/system/lib/liblog.so",
}


def output(*args):
    return subprocess.check_output(args, text=True).strip()


def needed(path):
    return [Path(name).name for name in NEEDED_RE.findall(output("readelf", "-d", str(path)))]


def remote_path(name):
    candidate = "/system/lib/" + name
    result = subprocess.run([str(ADB), "shell", "test", "-f", candidate], capture_output=True)
    if result.returncode == 0:
        return candidate
    # Names have been constrained to the safe character set above.
    script = ('for f in /apex/*/lib/' + name + '; do '
              'if [ -f "$f" ]; then echo "$f"; break; fi; done')
    return output(str(ADB), "shell", script)


def main():
    if not ADB.is_file():
        raise SystemExit("Missing local ADB binary: " + str(ADB))
    devices = output(str(ADB), "devices", "-l")
    ready = [line.split()[0] for line in devices.splitlines()[1:] if " device " in line]
    if len(ready) != 1:
        raise SystemExit("Expected exactly one authorized device; got: " + devices)
    serial = ready[0]
    sdk = output(str(ADB), "shell", "getprop", "ro.build.version.sdk")
    abis = output(str(ADB), "shell", "getprop", "ro.product.cpu.abilist")
    if "armeabi-v7a" not in abis:
        raise SystemExit("Phone does not offer 32-bit ARM libraries: " + abis)
    fingerprint = output(str(ADB), "shell", "getprop", "ro.build.fingerprint")
    previous = ROOT / "data" / "device-libs-manifest.json"
    if previous.is_file():
        old = json.loads(previous.read_text())
        if old.get("fingerprint") != fingerprint:
            raise SystemExit("Device build changed; move old device-libs/ and device-runtime/ before pulling")
    (ROOT / "data").mkdir(exist_ok=True)
    DEST.mkdir(exist_ok=True)
    RUNTIME.mkdir(exist_ok=True)
    for name, remote in RUNTIME_SOURCES.items():
        local = RUNTIME / name
        if not local.is_file():
            subprocess.run([str(ADB), "pull", remote, str(local)], check=True,
                           stdout=subprocess.DEVNULL)
            print("Pulled runtime", name, "from", remote)
    roots = [SOURCE / name for name in ("libcocos2dcpp.so", "libffmpeg.so",
                                      "libGLESv2.so", "libEGL.so", "libcutils.so")]
    queue = deque(roots + sorted(DEST.glob("*.so")))
    seen = set()
    origins = {path.name: "/system/lib/" + path.name for path in DEST.glob("*.so")}
    unavailable = set()
    while queue:
        path = queue.popleft()
        if path.name in seen:
            continue
        seen.add(path.name)
        for name in needed(path):
            if name in seen or name in unavailable or name in BUNDLED or (SOURCE / name).is_file():
                continue
            if not NAME_RE.fullmatch(name):
                raise SystemExit("Unexpected dependency name: " + name)
            local = DEST / name
            if not local.is_file():
                remote = remote_path(name)
                if not remote:
                    unavailable.add(name)
                    continue
                subprocess.run([str(ADB), "pull", remote, str(local)], check=True,
                               stdout=subprocess.DEVNULL)
                origins[name] = remote
                header = output("readelf", "-h", str(local))
                if "Class:                             ELF32" not in header or "Machine:                           ARM" not in header:
                    local.unlink()
                    raise SystemExit("Unexpected ELF type: " + name)
                print("Pulled", name, "from", remote)
            queue.append(local)
            if len(list(DEST.glob("*.so"))) > 100:
                raise SystemExit("Dependency closure exceeded 100 libraries; stopped")
    files = {}
    for path in sorted(DEST.glob("*.so")):
        files[path.name] = {"remote_path": origins.get(path.name), "bytes": path.stat().st_size,
                            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                            "needed": needed(path)}
    runtime = {}
    for name, remote in RUNTIME_SOURCES.items():
        path = RUNTIME / name
        runtime[name] = {"remote_path": remote, "bytes": path.stat().st_size,
                         "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                         "needed": needed(path)}
    report = {"serial": serial, "sdk": sdk, "abis": abis, "fingerprint": fingerprint,
              "copied_libraries": files, "runtime_libraries": runtime,
              "unavailable": sorted(unavailable),
              "bundled_by_unidbg": sorted(BUNDLED)}
    (ROOT / "data" / "device-libs-manifest.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Copied libraries:", len(files), "Unavailable:", sorted(unavailable))


if __name__ == "__main__":
    main()
