# Modern Android shell

This directory is the clean-room Android/NDK bootstrap for the recompilation. It does **not** contain original Never Gone binaries or assets.

## Purpose

The first shell milestone proves that a new package can:

- target a current Android SDK;
- load a project-owned C++ shared library;
- build for `arm64-v8a` and `armeabi-v7a`;
- report pointer width and runtime page size;
- provide the Java/JNI boundary that will later host the reconstructed Cocos2d-x startup path.

It is intentionally not a game build yet.

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
...
```

On a 16 KiB-page device the page-size line should report `16384 bytes`. This is one of the compatibility properties the original ARMv7-only binary cannot guarantee.

## Next implementation step

Replace the text-only shell incrementally:

1. vendor or adapt the required Cocos2d-x 2.1.2 interfaces;
2. reconstruct `AppDelegate` startup behavior;
3. implement an asset provider that reads files extracted locally from a user-supplied original APK;
4. recreate the old Cocos2d-x renderer/input JNI bridge only where still necessary;
5. reach the first scene without linking the original `libcocos2dcpp.so`.
