# Modern Android shell

This directory is the clean-room Android/NDK bootstrap for the recompilation. It does **not** contain original Never Gone binaries or assets.

## Purpose

The shell now proves that a new package can:

- target a current Android SDK;
- load a project-owned C++ shared library;
- build for `arm64-v8a` and `armeabi-v7a`;
- report pointer width and runtime page size;
- configure an app-local runtime root and stable app-local device ID;
- exercise the reconstructed semantic contract used directly by `Game.StartLua`;
- compile and smoke-test the exact **Lua 5.2.3** runtime identified in the original binary;
- register the seven known startup globals into Lua;
- resolve and execute imported Lua modules from the app-local asset tree.

It is intentionally not a playable game build yet.

## Startup compatibility layer

`startup_contract.{h,cpp}` contains clean-room semantic equivalents for the seven native-facing calls used directly by the recovered `Game.StartLua`:

```text
CAddDoString
Lua_GetPlatformString
Lua_GetDeviceUUID
cpp_ShowLoadingUI
cpp_HideLoadingUI
cpp_ShowErrorDialogUI
cpp_ShowMessageBoxUI
```

`lua_startup_bindings.cpp` exposes those semantics through the ordinary Lua 5.2 `lua_CFunction` interface. This does not reproduce the old ARM/C++ ABI; it recreates the Lua-visible API used by the scripts.

Current behavior:

- `Lua_GetPlatformString` returns `android`;
- `Lua_GetDeviceUUID` uses a random app-local UUID persisted in Android `SharedPreferences` rather than an obsolete hardware identifier;
- `CAddDoString("Game.ClientRequire")` resolves to `<files>/assets/Script/Game/ClientRequire.lua` and executes it in the current Lua state;
- the four UI functions enqueue typed diagnostic events until the final Android/game UI bridge is implemented.

At launch the JNI bootstrap runs both the semantic startup-contract smoke test and, when Lua is available, a real Lua VM smoke test.

## Lua 5.2.3

The original `libcocos2dcpp.so` contains the Lua 5.2.3 release banner. The repository does not copy third-party Lua sources directly; instead a helper downloads the official release and verifies the pinned SHA-256 before extraction:

```bash
python3 tools/fetch_lua_5_2_3.py
```

This creates the ignored local directory:

```text
third_party/_local/lua-5.2.3/
```

CMake detects that directory automatically and builds Lua into the recompilation library. If the directory is absent, the shell still builds but reports that Lua execution was skipped.

Pinned SHA-256:

```text
13c2fb97961381f7d06d5b5cea55b743c163800896fd5c5e2356201d3619002d
```

## Runtime asset layout

The new loader expects user-owned decoded scripts under the app's private files tree:

```text
<files>/assets/Script/Game/StartLua.lua
<files>/assets/Script/Game/ClientRequire.lua
<files>/assets/Script/ShareLogic/require.lua
...
```

The repository tooling can decode the original APK locally:

```bash
python3 tools/asset_decoder.py /path/to/com.hippiegame.nevergone.apk \
  --output build/decoded-assets
```

Decoded proprietary assets remain local and must not be committed.

If `Game/StartLua.lua` exists in the private runtime tree, the shell creates a fresh Lua 5.2.3 state, opens standard libraries, registers the seven startup globals, and executes `Game.StartLua`. The first missing native API or resource now appears as an explicit Lua error in the bootstrap diagnostics instead of being hidden behind the original binary.

## Requirements

- JDK 17+
- Android SDK Platform 36
- Android NDK installed through the SDK manager
- CMake 3.22.1 or newer
- Gradle compatible with Android Gradle Plugin 9.4
- Python 3 for verified Lua/bootstrap tooling

## Build

From the repository root, prepare Lua:

```bash
python3 tools/fetch_lua_5_2_3.py
```

Then build Android:

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

Without imported game scripts:

```text
startup contract: ok
lua smoke test: ok
lua runtime: Lua 5.2
startup script: not imported
```

With decoded scripts installed into the private runtime tree, `startup script:` changes to either `executed` or `failed`, with the Lua error shown on the next line. That failure is intentionally useful: it identifies the next native binding or runtime behavior to reconstruct.

On a 16 KiB-page device the page-size line should report `16384 bytes`.

## Next implementation step

1. run imported `Game.StartLua` and record the first missing native/global dependency;
2. add the next minimal binding set requested by the real startup path;
3. reconstruct `AppDelegate` behavior and resource search paths needed before scene creation;
4. replace diagnostic UI events with the real Android/game UI bridge;
5. continue until the original initial UI flow (`HelloWorld` → `ManagementLayer`) is reached without `libcocos2dcpp.so`.
