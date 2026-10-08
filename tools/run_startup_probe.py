#!/usr/bin/env python3
"""Run decoded Never Gone Game.StartLua under a host Lua 5.2 interpreter.

The probe is diagnostic only. It decodes Lua from a user-provided original APK
into a temporary directory, installs the clean-room compatibility shims already
kept in the repository, provides boot-safe offline bindings, and executes the
same CAddDoString module chain used by the Android runtime.

No decoded scripts are retained unless --keep-temp is supplied.
"""
from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

from asset_decoder import decode_bytes

APK_SCRIPT_PREFIX = "assets/assets/Script/"

RUNNER = r'''
local root = assert(arg[1], "script root required")
local modules = {}
local missing = {}
local callbacks = {}

local function path_for(module_name)
    assert(type(module_name) == "string" and module_name ~= "", "module name expected")
    assert(not module_name:find("%.%."), "unsafe module path")
    local relative = module_name
    if relative:sub(-4) ~= ".lua" then
        relative = relative:gsub("%.", "/") .. ".lua"
    end
    relative = relative:gsub("\\", "/")
    return root .. "/" .. relative
end

function CAddDoString(module_name)
    modules[#modules + 1] = module_name
    local chunk, load_error = loadfile(path_for(module_name))
    if not chunk then
        error("CAddDoString(" .. module_name .. "): " .. tostring(load_error), 2)
    end
    local ok, run_error = xpcall(chunk, debug.traceback)
    if not ok then
        error("CAddDoString(" .. module_name .. "): " .. tostring(run_error), 2)
    end
end

function Lua_GetPlatformString() return "android" end
function Lua_GetDeviceUUID() return "host-startup-probe" end
LGG_Device_UUID = Lua_GetDeviceUUID
function Lua_GetBundleVersion() return "recomp-probe" end
function Lua_SetConsoleColor() end
function cpp_ShowLoadingUI() end
function cpp_HideLoadingUI() end
function cpp_ShowErrorDialogUI() end
function cpp_ShowMessageBoxUI() end

local runtime_root = root:gsub("/Script$", "")
function Lua_GetSetFilePath() return runtime_root .. "/set/" end
function Lua_GetPathWithFileName(name) return runtime_root .. "/" .. tostring(name or "") end
function Lua_CopyFile() return false end
function LGG_IsFileExist(path)
    local handle = io.open(path, "rb")
    if handle then handle:close(); return true end
    return false
end
function Lua_IsXmlValid() return false end

-- These checks have clean-room runtime implementations. The host startup probe
-- only needs boot-safe semantics so initialization can advance to the next
-- genuinely missing binding; it does not reproduce the imported word list.
function Lua_CheckStringLegal(value)
    return type(value) == "string" and value ~= ""
end

function Lua_CheckNickName(value)
    if type(value) ~= "string" or value == "" then return false end
    return value:match("^[%w\128-\255]+$") ~= nil
end

local function capture_callback(name, ...)
    callbacks[#callbacks + 1] = {name = name, argc = select("#", ...)}
end

function cpp_OnGetServerList(...) capture_callback("cpp_OnGetServerList", ...) end
function cpp_OnGetRoleList(...) capture_callback("cpp_OnGetRoleList", ...) end
function cpp_OnCreateTheRole(...) capture_callback("cpp_OnCreateTheRole", ...) end
function cpp_OnGameAnnoucement(...) capture_callback("cpp_OnGameAnnoucement", ...) end
function cpp_OnEnterGame(...) capture_callback("cpp_OnEnterGame", ...) end

xml = {
    load = function() return nil end,
    encode = function(value) return tostring(value or "") end,
    _save = function() return false end,
}
package.loaded.xml = xml

ProtoRPC = {}
local ProtoRPCInstance = {}
ProtoRPCInstance.__index = ProtoRPCInstance
function ProtoRPC:new() return setmetatable({}, ProtoRPCInstance) end
function ProtoRPCInstance:SetID(value) self.id = value end
function ProtoRPCInstance:SetProtoFileRootDir(value) self.proto_root = value end
function ProtoRPCInstance:ImportProtoFile() return true end
function ProtoRPCInstance:CheckConnection() return false end
function ProtoRPCInstance:StartRPC() return false end
function ProtoRPCInstance:NewRequest() return nil end
function ProtoRPCInstance:NewMessage() return nil end
function ProtoRPCInstance:DeleteMessage() end
function ProtoRPCInstance:CallMethod() return nil end
function ProtoRPCInstance:Close() end
function ProtoRPCInstance:release() end
function ProtoRPCInstance:SetRpcID(value) self.rpc_id = value end
function ProtoRPCInstance:SetRpcSessionID(value) self.rpc_session_id = value end
function ProtoRPCInstance:CleanStackMsg() end

setmetatable(_G, {
    __index = function(_, name)
        missing[name] = (missing[name] or 0) + 1
        return nil
    end,
})

local ok, failure = xpcall(function()
    CAddDoString("Game.StartLua")
end, debug.traceback)

io.write("startup probe: ", ok and "executed" or "failed", "\n")
io.write("modules requested: ", #modules, "\n")
for index, name in ipairs(modules) do
    io.write(string.format("%03d %s\n", index, name))
end

io.write("client callbacks observed: ", #callbacks, "\n")
for index, callback in ipairs(callbacks) do
    io.write(string.format("%03d %s argc=%d\n", index, callback.name, callback.argc))
end

local names = {}
for name in pairs(missing) do names[#names + 1] = name end
table.sort(names)
io.write("missing globals: ", #names, "\n")
for _, name in ipairs(names) do
    io.write("  ", name, " (", missing[name], ")\n")
end

if not ok then
    io.write("startup traceback:\n", tostring(failure), "\n")
    os.exit(1)
end
'''


def extract_compat_script(source: Path) -> str:
    text = source.read_text(encoding="utf-8")
    match = re.search(r'R"LUA\((.*?)\)LUA"', text, re.DOTALL)
    if not match:
        raise SystemExit(f"cannot find embedded compatibility script in {source}")
    return match.group(1)


def decode_scripts(apk: Path, destination: Path) -> int:
    count = 0
    with zipfile.ZipFile(apk) as archive:
        for info in archive.infolist():
            if info.is_dir() or not info.filename.startswith(APK_SCRIPT_PREFIX):
                continue
            if not info.filename.lower().endswith(".lua"):
                continue
            relative = info.filename[len(APK_SCRIPT_PREFIX):]
            if not relative or ".." in Path(relative).parts:
                raise SystemExit(f"unsafe APK script path: {info.filename}")
            output = destination / relative
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(decode_bytes(archive.read(info)))
            count += 1
    if count == 0:
        raise SystemExit("no Never Gone Lua scripts found in APK")
    if not (destination / "Game" / "StartLua.lua").is_file():
        raise SystemExit("decoded APK is missing Game/StartLua.lua")
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path, help="legally obtained original Never Gone APK")
    parser.add_argument("--lua", type=Path, required=True, help="Lua 5.2 executable")
    parser.add_argument(
        "--compat-source",
        type=Path,
        default=Path("android/app/src/main/cpp/lua_compat.cpp"),
        help="source containing the embedded recomp Lua compatibility script",
    )
    parser.add_argument("--keep-temp", action="store_true", help="keep decoded temporary scripts for debugging")
    args = parser.parse_args()

    if not args.apk.is_file():
        raise SystemExit(f"APK not found: {args.apk}")
    if not args.lua.is_file():
        raise SystemExit(f"Lua executable not found: {args.lua}")

    temp_root = Path(tempfile.mkdtemp(prefix="nevergone-startup-probe-"))
    try:
        scripts_root = temp_root / "Script"
        decoded = decode_scripts(args.apk, scripts_root)
        compat = temp_root / "compat.lua"
        compat.write_text(extract_compat_script(args.compat_source), encoding="utf-8")
        runner = temp_root / "runner.lua"
        runner.write_text(
            "dofile(" + repr(compat.as_posix()) + ")\n" + RUNNER,
            encoding="utf-8",
        )

        print(f"decoded Lua modules: {decoded}")
        result = subprocess.run(
            [str(args.lua), str(runner), str(scripts_root)],
            check=False,
            text=True,
        )
        if args.keep_temp:
            print(f"probe temp retained: {temp_root}")
            temp_root = None
        return result.returncode
    finally:
        if temp_root is not None:
            shutil.rmtree(temp_root, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
