# Lua loader investigation

## Current finding

The APK contains 107 files with a `.lua` extension, but sampled files are neither plaintext Lua nor standard Lua bytecode. This strongly suggests that Never Gone transforms script bytes before passing them to the embedded Lua runtime.

The native library exports/contains standard Lua loader functions including:

```text
luaL_loadstring
luaL_loadfilex
luaL_loadbufferx
```

The immediate reverse-engineering target is therefore the call chain into `luaL_loadbufferx` / `luaL_loadfilex`.

## Method

1. Import `libcocos2dcpp.so` into Ghidra as ARM little-endian.
2. Apply known dynamic symbol names from `nm -D`/`readelf`.
3. Find all callers/xrefs of `luaL_loadbufferx` and `luaL_loadfilex`.
4. Identify callers that reference `.lua`, `StartLua.lua`, script search paths or Cocos Lua engine classes.
5. Trace file bytes from read/allocation through any transform into the Lua call.
6. Reimplement the transform in `tools/` and validate it against all 107 scripts.

## Validation criteria

A candidate decoder is accepted only when it produces one of:

- valid Lua source with plausible lexical structure; or
- Lua bytecode beginning with the expected Lua signature.

It must work consistently across the corpus rather than on a single sample.

## Notes

The binary also contains generic cryptographic and protocol-encryption code. Do not assume the Lua transform uses those routines merely because they exist. The decoder should be derived from xrefs/call flow or corpus-wide evidence.
