# Modern Android shell

This directory is the clean-room Android/NDK bootstrap for the recompilation. It does **not** contain original Never Gone binaries or assets.

## Current capabilities

The shell now provides:

- current Android SDK targeting;
- project-owned C++ shared library loading;
- `arm64-v8a` and `armeabi-v7a` targets;
- ABI, pointer-width and runtime page-size diagnostics;
- a persistent **Lua 5.2.3** runtime matching the version embedded in the original game;
- the seven native-facing Lua globals used directly by `Game.StartLua`;
- a non-proprietary Lua smoke test;
- a script root under the app-private files directory for future imported user-owned assets.

It is still a reconstruction shell rather than a playable game build.

## Requirements

- JDK 17+
- Android SDK Platform 36
- Android NDK installed through the SDK manager
- CMake 3.22.1 or newer
- Gradle compatible with Android Gradle Plugin 9.4
- network access during first CMake configure for the pinned Lua source archive, **or** a local Lua 5.2.3 source tree supplied through `NEVERGONE_LUA_SOURCE_DIR`

## Lua dependency

The original native binary identifies its interpreter as Lua 5.2.3. The CMake build therefore uses the same release rather than silently upgrading the script ABI.

The official source archive is pinned by SHA-256 in:

```text
app/src/main/cpp/cmake/Lua523.cmake
```

For offline builds, configure CMake with a directory containing an extracted Lua 5.2.3 tree:

```text
-DNEVERGONE_LUA_SOURCE_DIR=/path/to/lua-5.2.3
```

## Build

The repository does not commit the Gradle wrapper JAR yet. With a compatible local Gradle installation:

```bash
cd android
gradle wrapper
./gradlew assembleDebug
```

The resulting debug APK is normally located under:

```text
app/build/outputs/apk/debug/
```

## Expected result

Launching the shell should display information similar to:

```text
Never Gone Recomp

native bootstrap loaded
ABI: arm64-v8a
pointer width: 64 bit
page size: 4096 bytes
kernel: ...
machine: ...
Lua: Lua 5.2.3 startup contract: OK
script root: /data/user/0/org.nevergone.recomp/files/assets/Script
```

On a 16 KiB-page device the page-size line should report `16384 bytes`.

## Reconstructed startup contract

The runtime currently registers:

```text
CAddDoString
Lua_GetPlatformString
Lua_GetDeviceUUID
cpp_ShowErrorDialogUI
cpp_ShowLoadingUI
cpp_HideLoadingUI
cpp_ShowMessageBoxUI
```

`CAddDoString("Game.ClientRequire")` resolves to:

```text
<files>/assets/Script/Game/ClientRequire.lua
```

The UI bridge functions are temporary logging stubs until the corresponding Cocos/UI behavior is reconstructed.

See `../docs/lua-bootstrap-runtime.md` and `../docs/lua-native-api-map.md` for the recovered behavior and API evidence.

## Next implementation step

1. integrate the user-owned decoded asset import workflow with the app-private files tree;
2. execute the real `Game.StartLua` using the new Lua runtime;
3. capture the first missing native binding/class;
4. reconstruct bindings incrementally from the existing Lua/native API map;
5. begin replacing UI stubs with the `ManagementLayer`/Cocos startup path.
