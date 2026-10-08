# Modern Android shell

This directory is the clean-room Android/NDK bootstrap for the recompilation. It does **not** contain original Never Gone binaries or assets.

## Purpose

The shell now proves that a new package can:

- target a current Android SDK;
- load a project-owned C++ shared library;
- build for `arm64-v8a` and `armeabi-v7a`;
- report pointer width and runtime page size;
- configure an app-local runtime root and stable app-local device ID;
- exercise the reconstructed semantic contract used directly by `Game.StartLua`.

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

These are deliberately **not** exported yet as guessed Lua C ABI functions. The Lua VM/version and exact registration signatures must be confirmed first. Keeping the semantic implementation separate lets the project test behavior without baking an incorrect ABI into the runtime.

Current behavior:

- `Lua_GetPlatformString` returns `android`;
- `Lua_GetDeviceUUID` uses a random app-local UUID persisted in Android `SharedPreferences` rather than an obsolete hardware identifier;
- `CAddDoString` records requested module names until the Lua loader is integrated;
- the four UI functions enqueue typed diagnostic events until the final Android/game UI bridge is implemented.

At launch the JNI bootstrap runs a smoke test against this contract. A successful shell displays `startup contract: ok`.

## Requirements

- JDK 17+
- Android SDK Platform 36
- Android NDK installed through the SDK manager
- CMake 3.22.1 or newer
- Gradle compatible with Android Gradle Plugin 9.4

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
files dir configured: yes
startup contract: ok
platform: android
device id configured: yes
module requests: 2
UI events: 4
```

On a 16 KiB-page device the page-size line should report `16384 bytes`.

## Next implementation step

1. confirm and integrate the Lua runtime compatible with the shipped scripts;
2. add thin Lua wrappers around the seven startup-contract functions;
3. make `CAddDoString` execute modules from locally imported/decoded user assets;
4. reconstruct `AppDelegate` startup behavior and resource search paths;
5. replace diagnostic UI events with the real Android/game UI bridge;
6. reach `Game.StartLua` execution without linking the original `libcocos2dcpp.so`.
