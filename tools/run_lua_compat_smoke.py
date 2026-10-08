#!/usr/bin/env python3
"""Execute the embedded Lua compatibility script with a host Lua 5.2 binary."""
from __future__ import annotations

import argparse
import re
import subprocess
import tempfile
from pathlib import Path


def extract_compat_script(source: str) -> str:
    match = re.search(r'R"LUA\((.*?)\)LUA"', source, re.DOTALL)
    if not match:
        raise SystemExit("cannot find R\"LUA(...)LUA\" compatibility script")
    return match.group(1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("lua", type=Path, help="host Lua 5.2 executable")
    parser.add_argument(
        "--source",
        type=Path,
        default=Path("android/app/src/main/cpp/lua_compat.cpp"),
        help="C++ source containing the embedded compatibility script",
    )
    args = parser.parse_args()

    script = extract_compat_script(args.source.read_text(encoding="utf-8"))
    smoke = r'''
assert(type(unpack) == "function")
assert(unpack({4, 5}) == 4)
assert(type(bit) == "table")
assert(bit.band(0xf3, 0x0f) == 3)
assert(bit.bor(1, 4) == 5)
assert(bit.lshift(1, 4) == 16)
assert(bit.rshift(16, 4) == 1)

assert(type(cjson) == "table")
local encoded = cjson.encode({name="Never Gone", count=3, flags={true, false}, quote=[[a"b]]})
local decoded = cjson.decode(encoded)
assert(decoded.name == "Never Gone")
assert(decoded.count == 3)
assert(decoded.flags[1] == true and decoded.flags[2] == false)
assert(decoded.quote == [[a"b]])
assert(cjson.decode([[{"value":null}]]).value == cjson.null)
assert(cjson.decode([[{"unicode":"\u041d\u0435\u0432\u0435\u0440"}]]).unicode == "Невер")

local xml = {native = true}
_G.xml = xml
local function module_chunk()
    module("xml")
    value = 7
end
module_chunk()
assert(package.loaded.xml == xml)
assert(xml.native == true and xml.value == 7)
print("lua compatibility smoke: ok")
'''

    with tempfile.NamedTemporaryFile("w", suffix=".lua", encoding="utf-8", delete=False) as handle:
        handle.write(script)
        handle.write("\n")
        handle.write(smoke)
        temp_path = Path(handle.name)

    try:
        result = subprocess.run(
            [str(args.lua), str(temp_path)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        print(result.stdout, end="")
        if result.returncode != 0:
            raise SystemExit(result.returncode)
    finally:
        temp_path.unlink(missing_ok=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
