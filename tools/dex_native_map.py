#!/usr/bin/env python3
"""List native methods declared by classes in an Android DEX file.

This is a deliberately small DEX parser used by the Never Gone reverse-
engineering workflow. It does not attempt to decompile bytecode; it only reads
string/type/method/class tables and class_data_item structures well enough to
identify methods carrying ACC_NATIVE.

The tool uses only Python's standard library.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Iterable

ACC_NATIVE = 0x0100


@dataclass(frozen=True)
class NativeMethod:
    class_descriptor: str
    name: str
    access_flags: int
    method_kind: str

    @property
    def java_class(self) -> str:
        value = self.class_descriptor
        if value.startswith("L") and value.endswith(";"):
            value = value[1:-1]
        return value.replace("/", ".")

    @property
    def expected_jni_prefix(self) -> str:
        escaped_class = self.java_class.replace("_", "_1").replace(".", "_")
        escaped_method = self.name.replace("_", "_1")
        return f"Java_{escaped_class}_{escaped_method}"


class DexFormatError(ValueError):
    pass


class DexReader:
    def __init__(self, data: bytes):
        self.data = data
        if len(data) < 0x70 or not data.startswith(b"dex\n"):
            raise DexFormatError("not a DEX file")

        self.string_ids_size = self.u32(0x38)
        self.string_ids_off = self.u32(0x3C)
        self.type_ids_size = self.u32(0x40)
        self.type_ids_off = self.u32(0x44)
        self.method_ids_size = self.u32(0x58)
        self.method_ids_off = self.u32(0x5C)
        self.class_defs_size = self.u32(0x60)
        self.class_defs_off = self.u32(0x64)

        self.strings = [
            self.read_string(self.u32(self.string_ids_off + 4 * index))
            for index in range(self.string_ids_size)
        ]
        self.types = [
            self.strings[self.u32(self.type_ids_off + 4 * index)]
            for index in range(self.type_ids_size)
        ]
        self.methods = [self.read_method_id(index) for index in range(self.method_ids_size)]

    def require(self, offset: int, size: int) -> None:
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise DexFormatError(f"out-of-range DEX read at 0x{offset:x} size {size}")

    def u16(self, offset: int) -> int:
        self.require(offset, 2)
        return struct.unpack_from("<H", self.data, offset)[0]

    def u32(self, offset: int) -> int:
        self.require(offset, 4)
        return struct.unpack_from("<I", self.data, offset)[0]

    def uleb128(self, offset: int) -> tuple[int, int]:
        result = 0
        shift = 0
        for _ in range(5):
            self.require(offset, 1)
            byte = self.data[offset]
            offset += 1
            result |= (byte & 0x7F) << shift
            if not (byte & 0x80):
                return result, offset
            shift += 7
        raise DexFormatError("invalid ULEB128 value")

    def read_string(self, offset: int) -> str:
        _, offset = self.uleb128(offset)  # UTF-16 code-unit length
        end = self.data.find(b"\x00", offset)
        if end < 0:
            raise DexFormatError("unterminated string_data_item")
        # DEX uses modified UTF-8. Class/method names used by this tool are
        # ASCII-compatible, so UTF-8 with replacement is sufficient here.
        return self.data[offset:end].decode("utf-8", "replace")

    def read_method_id(self, index: int) -> tuple[str, str]:
        offset = self.method_ids_off + index * 8
        class_idx = self.u16(offset)
        name_idx = self.u32(offset + 4)
        return self.types[class_idx], self.strings[name_idx]

    def skip_encoded_fields(self, offset: int, count: int) -> int:
        field_index = 0
        for _ in range(count):
            diff, offset = self.uleb128(offset)
            _, offset = self.uleb128(offset)  # access_flags
            field_index += diff
        return offset

    def read_encoded_methods(
        self, offset: int, count: int, kind: str
    ) -> tuple[list[NativeMethod], int]:
        method_index = 0
        native: list[NativeMethod] = []
        for _ in range(count):
            diff, offset = self.uleb128(offset)
            flags, offset = self.uleb128(offset)
            _, offset = self.uleb128(offset)  # code_off
            method_index += diff
            if method_index >= len(self.methods):
                raise DexFormatError("method index outside method_ids table")
            if flags & ACC_NATIVE:
                descriptor, name = self.methods[method_index]
                native.append(NativeMethod(descriptor, name, flags, kind))
        return native, offset

    def native_methods(self) -> list[NativeMethod]:
        result: list[NativeMethod] = []
        for class_index in range(self.class_defs_size):
            definition = self.class_defs_off + class_index * 32
            class_data_off = self.u32(definition + 24)
            if not class_data_off:
                continue

            offset = class_data_off
            static_fields, offset = self.uleb128(offset)
            instance_fields, offset = self.uleb128(offset)
            direct_methods, offset = self.uleb128(offset)
            virtual_methods, offset = self.uleb128(offset)

            offset = self.skip_encoded_fields(offset, static_fields)
            offset = self.skip_encoded_fields(offset, instance_fields)

            methods, offset = self.read_encoded_methods(offset, direct_methods, "direct")
            result.extend(methods)
            methods, offset = self.read_encoded_methods(offset, virtual_methods, "virtual")
            result.extend(methods)

        return result


def markdown(methods: Iterable[NativeMethod]) -> str:
    methods = list(methods)
    lines = [
        "# DEX native methods",
        "",
        f"Native method declarations: **{len(methods)}**",
        "",
        "| Java class | Method | Flags | Kind | Expected JNI symbol prefix |",
        "| --- | --- | ---: | --- | --- |",
    ]
    for method in methods:
        lines.append(
            f"| `{method.java_class}` | `{method.name}` | `0x{method.access_flags:x}` | "
            f"{method.method_kind} | `{method.expected_jni_prefix}` |"
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dex", type=Path, help="path to classes.dex")
    parser.add_argument("--json", dest="json_path", type=Path)
    parser.add_argument("--markdown", dest="markdown_path", type=Path)
    args = parser.parse_args()

    methods = DexReader(args.dex.read_bytes()).native_methods()
    methods.sort(key=lambda value: (value.java_class, value.name))

    if args.json_path:
        args.json_path.parent.mkdir(parents=True, exist_ok=True)
        payload = []
        for method in methods:
            row = asdict(method)
            row["java_class"] = method.java_class
            row["expected_jni_prefix"] = method.expected_jni_prefix
            payload.append(row)
        args.json_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    if args.markdown_path:
        args.markdown_path.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_path.write_text(markdown(methods), encoding="utf-8")

    if not args.json_path and not args.markdown_path:
        print(markdown(methods), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
