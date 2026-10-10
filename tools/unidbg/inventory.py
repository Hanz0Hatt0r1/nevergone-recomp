#!/usr/bin/env python3
"""Inventory Nevergone ELF dynamic symbols without modifying the binaries."""

import csv
import os
import shutil
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
LIB_DIR = Path(os.environ["NEVERGONE_LIB_DIR"])
OUT = ROOT / "data"


def run(*argv):
    return subprocess.check_output(argv, text=True)


def symbols(path):
    lines = run("readelf", "--dyn-syms", "-W", str(path)).splitlines()
    parsed = []
    for line in lines:
        match = re.match(r"\s*(\d+):\s*([0-9a-fA-F]+)\s+(\d+)\s+(\w+)\s+(\w+)\s+(\w+)\s+(\S+)\s+(.+)", line)
        if not match:
            continue
        number, value, size, typ, bind, vis, section, name = match.groups()
        if name == "":
            continue
        parsed.append(dict(index=int(number), value=int(value, 16), size=int(size),
                           type=typ, binding=bind, visibility=vis, section=section,
                           name=name, defined=section != "UND"))
    names = "\n".join(item["name"] for item in parsed) + "\n"
    demangled = subprocess.run(["c++filt"], input=names, text=True,
                               check=True, capture_output=True).stdout.splitlines()
    for item, human in zip(parsed, demangled):
        item["demangled"] = human
        item["thumb"] = item["defined"] and item["type"] == "FUNC" and bool(item["value"] & 1)
        item["code_offset"] = item["value"] & ~1 if item["thumb"] else item["value"]
    return parsed


def main():
    OUT.mkdir(exist_ok=True)
    manifest = {}
    for path in sorted(LIB_DIR.glob("*.so")):
        rows = symbols(path)
        with (OUT / f"{path.name}.symbols.tsv").open("w", newline="") as stream:
            writer = csv.writer(stream, delimiter="\t")
            writer.writerow(("index", "value_hex", "code_offset_hex", "size", "type", "binding",
                             "visibility", "section", "thumb", "name", "demangled"))
            for row in rows:
                writer.writerow((row["index"], f"0x{row['value']:08x}",
                                 f"0x{row['code_offset']:08x}", row["size"], row["type"],
                                 row["binding"], row["visibility"], row["section"],
                                 int(row["thumb"]), row["name"], row["demangled"]))
        data = path.read_bytes()
        dynamic = run("readelf", "-d", str(path))
        needed = re.findall(r"\(NEEDED\).*?\[(.*?)\]", dynamic)
        soname = re.search(r"\(SONAME\).*?\[(.*?)\]", dynamic)
        init_addr = re.search(r"\(INIT_ARRAY\)\s+0x([0-9a-fA-F]+)", dynamic)
        init_size = re.search(r"\(INIT_ARRAYSZ\)\s+(\d+)", dynamic)
        manifest[path.name] = {
            "path": str(path), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "elf": "ELF32 little-endian ARM EABI5 shared object", "abi": "armeabi-v7a",
            "needed": needed, "soname": soname.group(1) if soname else None,
            "init_array_vaddr": "0x" + init_addr.group(1) if init_addr else None,
            "init_array_bytes": int(init_size.group(1)) if init_size else None,
            "defined_functions": sum(r["defined"] and r["type"] == "FUNC" for r in rows),
            "defined_objects": sum(r["defined"] and r["type"] == "OBJECT" for r in rows),
            "undefined_symbols": sum(not r["defined"] for r in rows),
        }
    (OUT / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    cocos = symbols(LIB_DIR / "libcocos2dcpp.so")
    groups = {
        "jni": lambda r: r["name"] == "JNI_OnLoad" or r["name"].startswith("Java_"),
        "lua": lambda r: "lua_" in r["name"] or "luaL_" in r["name"] or "Lua" in r["demangled"] or "ScriptEngine" in r["demangled"],
        "game": lambda r: any(s in r["demangled"] for s in ("EnemyActionsData", "PlayerActionData", "ActionDataManager", "GameScene", "GameSaveData", "DataManager", "MEPlayer", "AppDelegate")),
        "cocos": lambda r: "cocos2d::" in r["demangled"] and any(s in r["demangled"] for s in ("CCDirector", "CCApplication", "CCFileUtils", "CCEGLView", "CCLuaEngine")),
    }
    for group, pred in groups.items():
        selected = [r for r in cocos if r["defined"] and r["type"] == "FUNC" and pred(r)]
        with (OUT / f"{group}.tsv").open("w", newline="") as stream:
            writer = csv.writer(stream, delimiter="\t")
            writer.writerow(("value_hex", "code_offset_hex", "size", "thumb", "name", "demangled"))
            for r in selected:
                writer.writerow((f"0x{r['value']:08x}", f"0x{r['code_offset']:08x}",
                                 r["size"], int(r["thumb"]), r["name"], r["demangled"]))
    print(json.dumps(manifest, indent=2))
    print("Groups:", {g: sum(r["defined"] and r["type"] == "FUNC" and p(r) for r in cocos)
                      for g, p in groups.items()})


if __name__ == "__main__":
    main()
