# Lua 5.2.3 bootstrap runtime

The original ARMv7 library embeds **Lua 5.2.3**. The native binary contains both the generic `Lua 5.2` version string and the full 5.2.3 `$LuaVersion` marker, so the recompilation now pins the same interpreter release instead of using a newer Lua ABI.

## Dependency

The Android CMake build uses the official Lua 5.2.3 source archive and verifies:

```text
SHA-256: 13c2fb97961381f7d06d5b5cea55b743c163800896fd5c5e2356201d3619002d
```

`android/app/src/main/cpp/cmake/Lua523.cmake` can either:

- fetch the official archive during configuration; or
- use a pre-extracted local copy through `NEVERGONE_LUA_SOURCE_DIR`.

The latter is useful for offline/reproducible development environments.

## Recovered `CAddDoString` behavior

Thumb disassembly of the original `CAddDoString(lua_State*)` confirms that it:

1. reads the first Lua argument as a string;
2. constructs a path rooted under `Script/`;
3. resolves a `.lua` file;
4. reads the script through the Cocos file layer;
5. passes the resulting text to the Lua loader;
6. executes the loaded chunk with `lua_pcall`.

The reconstructed implementation uses the imported decoded tree instead of reproducing the obsolete Cocos file decoder at runtime. A module such as:

```text
Game.ClientRequire
```

maps to:

```text
<files>/assets/Script/Game/ClientRequire.lua
```

This also keeps proprietary user-owned assets outside the source repository.

## Startup bindings implemented

The first runtime layer registers all seven native-facing globals used directly by `Game.StartLua`:

```text
CAddDoString
Lua_GetPlatformString
Lua_GetDeviceUUID
cpp_ShowErrorDialogUI
cpp_ShowLoadingUI
cpp_HideLoadingUI
cpp_ShowMessageBoxUI
```

### `Lua_GetPlatformString`

The original native binary contains the exact Android platform literal `android`. The reconstructed function returns the same value.

### `Lua_GetDeviceUUID`

The modern Android Activity passes `Settings.Secure.ANDROID_ID` into the native runtime. This is currently the stable replacement identifier used by the compatibility layer. It can be changed later if comparison testing shows that the original game expected a different identifier format.

### UI bridge functions

The four `cpp_*UI` functions are currently compatibility stubs. They validate/read their known Lua arguments, log the request and return success instead of requiring the unreconstructed Cocos UI layer.

Disassembly confirms several details that guide the final implementations:

- `cpp_ShowErrorDialogUI` reads argument 1 as an integer error code and forwards it to `LUA_LOGIN::ShowErrorCode(int)`;
- `cpp_ShowLoadingUI` calls `ServerInteractionLayer::WaitForTheServerToRespond(true)`;
- `cpp_HideLoadingUI` calls the same method with `false`;
- `cpp_ShowMessageBoxUI` reads four string arguments before forwarding them to the original message-box path.

These stubs are intentionally isolated so they can later be replaced by reconstructed UI behavior without changing the Lua-facing names.

## Persistent Lua state

The Android JNI bootstrap now creates one persistent `LuaRuntime` for the process instead of creating a temporary interpreter for each call.

The runtime currently:

- opens the Lua 5.2 standard libraries;
- registers the seven startup globals;
- stores the writable script root;
- stores the Java-provided device identifier;
- runs a non-proprietary embedded smoke test.

The smoke test verifies:

```text
_VERSION == "Lua 5.2"
Lua_GetPlatformString() == "android"
Lua_GetDeviceUUID() returns a string
CAddDoString is registered
loading UI stubs can be called
```

A successful shell reports:

```text
Lua: Lua 5.2.3 startup contract: OK
```

## Current boundary

This milestone does **not** yet execute the original `Game.StartLua`. The runtime expects locally imported decoded scripts under the application's private files directory, but the device-side import/copy workflow has not been connected yet.

The next boot milestone is therefore:

1. prepare/import the user-owned decoded asset tree;
2. place or expose it at the runtime script root;
3. execute `Game.StartLua`;
4. record the first missing native global/class;
5. implement bindings iteratively until the initial UI is reached.

This turns boot reconstruction into a measurable missing-binding loop rather than a full up-front rewrite.
