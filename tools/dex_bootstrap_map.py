#!/usr/bin/env python3
"""Recover launcher/library/lifecycle call order from DEX metadata and code items."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from dex_native_map import DexReader


def java_name(descriptor: str) -> str:
    if descriptor.startswith("L") and descriptor.endswith(";"):
        return descriptor[1:-1].replace("/", ".")
    return descriptor


def instruction_width(units: list[int], pc: int) -> int:
    opcode = units[pc] & 0xFF
    if opcode == 0:
        ident = units[pc]
        if ident == 0x0100:
            return 4 + 2 * units[pc + 1]
        if ident == 0x0200:
            return 2 + 4 * units[pc + 1]
        if ident == 0x0300:
            width = units[pc + 1]
            count = units[pc + 2] | (units[pc + 3] << 16)
            return 4 + (width * count + 1) // 2
        return 1
    two = {0x02,0x05,0x08,0x13,0x15,0x16,0x19,0x1A,0x1C,0x1F,0x20,0x22,0x23,0x29}
    two.update(range(0x2D,0x3E)); two.update(range(0x44,0x6E)); two.update(range(0x90,0xB0)); two.update(range(0xD0,0xE3))
    if opcode in two:
        return 2
    three = {0x03,0x06,0x09,0x14,0x17,0x1B,0x24,0x25,0x26,0x2A,0x2B,0x2C}
    three.update(range(0x6E,0x73)); three.update(range(0x74,0x79))
    if opcode in three:
        return 3
    return 5 if opcode == 0x18 else 1


def invoke_registers(units: list[int], pc: int) -> list[int]:
    first = units[pc]
    opcode = first & 0xFF
    if 0x6E <= opcode <= 0x72:
        count, reg_g = (first >> 12) & 0xF, (first >> 8) & 0xF
        packed = units[pc + 2]
        regs = [packed & 0xF, (packed >> 4)&0xF, (packed >> 8)&0xF, (packed >> 12)&0xF, reg_g]
        return regs[:count]
    if 0x74 <= opcode <= 0x78:
        count, first_reg = (first >> 8)&0xFF, units[pc + 2]
        return list(range(first_reg, first_reg + count))
    return []


def skip_fields(dex: DexReader, offset: int, count: int) -> int:
    for _ in range(count):
        _, offset = dex.uleb128(offset)
        _, offset = dex.uleb128(offset)
    return offset


def encoded_methods(dex: DexReader) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    classes: list[dict[str, Any]] = []
    methods: list[dict[str, Any]] = []
    for index in range(dex.class_defs_size):
        definition = dex.class_defs_off + index * 32
        class_idx = dex.u32(definition)
        super_idx = dex.u32(definition + 8)
        source_idx = dex.u32(definition + 16)
        class_data_off = dex.u32(definition + 24)
        descriptor = dex.types[class_idx]
        classes.append({
            "descriptor": descriptor,
            "java_class": java_name(descriptor),
            "super_descriptor": None if super_idx == 0xFFFFFFFF else dex.types[super_idx],
            "source_file": None if source_idx == 0xFFFFFFFF else dex.strings[source_idx],
        })
        if not class_data_off:
            continue
        offset = class_data_off
        static_fields, offset = dex.uleb128(offset)
        instance_fields, offset = dex.uleb128(offset)
        direct_methods, offset = dex.uleb128(offset)
        virtual_methods, offset = dex.uleb128(offset)
        offset = skip_fields(dex, offset, static_fields)
        offset = skip_fields(dex, offset, instance_fields)
        for kind, count in (("direct", direct_methods), ("virtual", virtual_methods)):
            method_idx = 0
            for _ in range(count):
                diff, offset = dex.uleb128(offset)
                flags, offset = dex.uleb128(offset)
                code_off, offset = dex.uleb128(offset)
                method_idx += diff
                methods.append({"class": descriptor, "method_idx": method_idx, "kind": kind, "flags": flags, "code_off": code_off})
    return classes, methods


def code_units(dex: DexReader, code_off: int) -> list[int]:
    if not code_off:
        return []
    count = dex.u32(code_off + 12)
    return [dex.u16(code_off + 16 + 2*i) for i in range(count)]


def method_calls(dex: DexReader, item: dict[str, Any]) -> list[dict[str, Any]]:
    units = code_units(dex, item["code_off"])
    result = []
    pc = 0
    while pc < len(units):
        opcode = units[pc] & 0xFF
        if (0x6E <= opcode <= 0x72 or 0x74 <= opcode <= 0x78) and pc + 2 < len(units):
            method_idx = units[pc + 1]
            if method_idx < len(dex.methods):
                descriptor, name = dex.methods[method_idx]
                result.append({"pc": pc, "target_class": descriptor, "target_name": name, "target": f"{java_name(descriptor)}.{name}"})
        pc += instruction_width(units, pc)
    return result


def load_library_calls(dex: DexReader, methods: list[dict[str, Any]]) -> list[dict[str, Any]]:
    result = []
    for item in methods:
        units = code_units(dex, item["code_off"])
        if not units:
            continue
        caller_class, caller_name = dex.methods[item["method_idx"]]
        strings_by_reg: dict[int, str] = {}
        pc = 0
        while pc < len(units):
            opcode = units[pc] & 0xFF
            if opcode == 0x1A and pc + 1 < len(units):
                strings_by_reg[(units[pc] >> 8)&0xFF] = dex.strings[units[pc + 1]]
            elif opcode == 0x1B and pc + 2 < len(units):
                index = units[pc + 1] | (units[pc + 2] << 16)
                strings_by_reg[(units[pc] >> 8)&0xFF] = dex.strings[index]
            elif (0x6E <= opcode <= 0x72 or 0x74 <= opcode <= 0x78) and pc + 2 < len(units):
                method_idx = units[pc + 1]
                if method_idx < len(dex.methods):
                    target_class, target_name = dex.methods[method_idx]
                    if target_class == "Ljava/lang/System;" and target_name == "loadLibrary":
                        regs = invoke_registers(units, pc)
                        result.append({"caller_class": java_name(caller_class), "caller_method": caller_name, "pc": pc, "library": strings_by_reg.get(regs[0]) if regs else None})
            pc += instruction_width(units, pc)
    return result


def find_method(dex: DexReader, methods: list[dict[str, Any]], descriptor: str, name: str) -> dict[str, Any] | None:
    for item in methods:
        if item["class"] == descriptor and dex.methods[item["method_idx"]][1] == name:
            return item
    return None


def build_report(dex: DexReader, launcher_java: str) -> dict[str, Any]:
    classes, methods = encoded_methods(dex)
    launcher = "L" + launcher_java.replace(".", "/") + ";"
    duplicates = [c["descriptor"] for c in classes if c["descriptor"].endswith("/TJ_P_01;") and c["descriptor"] != launcher]
    relevant = [launcher, "Lorg/cocos2dx/lib/Cocos2dxActivity;", "Lorg/cocos2dx/lib/Cocos2dxGLSurfaceView;", "Lorg/cocos2dx/lib/Cocos2dxGLSurfaceView$3;", "Lorg/cocos2dx/lib/Cocos2dxGLSurfaceView$4;", "Lorg/cocos2dx/lib/Cocos2dxRenderer;", *duplicates]
    by_descriptor = {c["descriptor"]: c for c in classes}
    class_rows = [by_descriptor[x] for x in relevant if x in by_descriptor]
    names = ("<clinit>","init","checkPlayServices","onActivityResult","onDestroy","onCreate","onPause","onResume","onSizeChanged","onSurfaceCreated","onSurfaceChanged","onDrawFrame","handleOnPause","handleOnResume","run")
    lifecycle: dict[str, Any] = {}
    for descriptor in relevant:
        found = {}
        for name in names:
            item = find_method(dex, methods, descriptor, name)
            if item and item["code_off"]:
                found[name] = method_calls(dex, item)
        if found:
            lifecycle[java_name(descriptor)] = found
    gamepad = []
    duplicate_callers = []
    duplicate_set = set(duplicates)
    for item in methods:
        caller_class, caller_name = dex.methods[item["method_idx"]]
        for call in method_calls(dex, item):
            if call["target_class"] == "Lcom/ngds/cocos/GamepadBridge;" and caller_class != "Lcom/ngds/cocos/GamepadBridge;":
                gamepad.append({"caller_class": java_name(caller_class), "caller_method": caller_name, "target": call["target"]})
            if call["target_class"] in duplicate_set and caller_class not in duplicate_set:
                duplicate_callers.append({"caller_class": java_name(caller_class), "caller_method": caller_name, "target": call["target"]})
    return {"launcher_class": launcher_java, "classes": class_rows, "load_library_calls": load_library_calls(dex, methods), "lifecycle_calls": lifecycle, "duplicate_tj_classes": [java_name(x) for x in duplicates], "duplicate_tj_external_callers": duplicate_callers, "gamepad_bridge_callers": gamepad}


def markdown(report: dict[str, Any]) -> str:
    out = ["# DEX bootstrap map","",f"Launcher: `{report['launcher_class']}`","","## Class hierarchy",""]
    for c in report["classes"]:
        parent = java_name(c["super_descriptor"]) if c["super_descriptor"] else "none"
        out.append(f"- `{c['java_class']}` → `{parent}` (source `{c['source_file'] or 'unknown'}`)")
    out += ["","## System.loadLibrary calls",""]
    for x in report["load_library_calls"]:
        out.append(f"- `{x['caller_class']}.{x['caller_method']}` @ unit {x['pc']}: `{x['library'] or '<unresolved>'}`")
    out += ["","## Lifecycle invoke chains",""]
    for cls, methods in report["lifecycle_calls"].items():
        out += [f"### `{cls}`",""]
        for name, calls in methods.items():
            out.append(f"- `{name}`")
            out.extend(f"  - `{call['target']}`" for call in calls)
        out.append("")
    out += ["## Duplicate TJ_P_01 direct callers",""]
    out += [f"- `{x['caller_class']}.{x['caller_method']}` → `{x['target']}`" for x in report["duplicate_tj_external_callers"]] or ["- none"]
    out += ["","## GamepadBridge direct callers outside GamepadBridge",""]
    out += [f"- `{x['caller_class']}.{x['caller_method']}` → `{x['target']}`" for x in report["gamepad_bridge_callers"]] or ["- none"]
    return "\n".join(out).rstrip()+"\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dex", type=Path)
    parser.add_argument("--launcher-class", default="com.hippiegame.nevergone.TJ_P_01")
    parser.add_argument("--json", dest="json_path", type=Path)
    parser.add_argument("--markdown", dest="markdown_path", type=Path)
    args = parser.parse_args()
    report = build_report(DexReader(args.dex.read_bytes()), args.launcher_class)
    if args.json_path:
        args.json_path.parent.mkdir(parents=True, exist_ok=True); args.json_path.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    if args.markdown_path:
        args.markdown_path.parent.mkdir(parents=True, exist_ok=True); args.markdown_path.write_text(markdown(report), encoding="utf-8")
    if not args.json_path and not args.markdown_path:
        print(markdown(report), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
