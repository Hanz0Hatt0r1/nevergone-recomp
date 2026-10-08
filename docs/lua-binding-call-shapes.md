# Lua native binding call shapes

`tools/lua_binding_call_shapes.py` extracts metadata about how native-looking APIs are called by the recovered Lua layer. It complements the existing Lua→ELF name map: the name map answers **which** native APIs are likely required, while this tool estimates **how** Lua calls them.

The report contains only:

- Lua-facing API name;
- global/member call classification;
- number of observed calls;
- modules containing those calls;
- observed arities;
- coarse argument kinds such as string, number, boolean, identifier, table, or expression.

Literal string contents and Lua source bodies are not emitted.

## Usage

Against a legally obtained original APK:

```bash
python3 tools/lua_binding_call_shapes.py /path/to/com.hippiegame.nevergone.apk \
  --json build/lua-binding-call-shapes.json \
  --markdown build/lua-binding-call-shapes.md
```

By default only modules reachable from `Game.StartLua` are analyzed. Pass `--include-unreachable` to include the six modules outside the normal static startup graph.

## Why call shapes matter

A retained C++ symbol or registration string can identify a Lua-facing name without proving its exact Lua stack contract. Call-shape metadata provides a second source of evidence before implementing a compatibility wrapper.

For example, the recovered scripts establish these practical shapes:

| API | Observed Lua shape |
| --- | --- |
| `Lua_GetSetFilePath` | zero arguments |
| `Lua_GetPathWithFileName` | one path/name argument |
| `Lua_CopyFile` | two path arguments |
| `LGG_IsFileExist` | one path argument |
| `Lua_SetConsoleColor` | one numeric argument |
| `Lua_GetBundleVersion` | zero arguments |
| `Lua_CheckNickName` | one string/value argument |
| `Lua_CheckStringLegal` | one string/value argument |

Those shapes do not by themselves prove return types or all edge-case semantics. Runtime use, native symbols/xrefs, and surrounding Lua control flow still determine the final compatibility implementation.

## Parser scope

The tool is deliberately lightweight rather than a full Lua parser. It tracks balanced parentheses, nested calls/tables, quoted strings, and ordinary comments so commas inside nested expressions do not inflate arity. Unit tests cover nested calls, strings containing commas, native-looking global calls, and `ProtoRPC:new(...)` member calls.

The output should be treated as reconstruction evidence, not as an ABI specification generated from the original binary.
