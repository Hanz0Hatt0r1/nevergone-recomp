# JNI map

This document records the initial Java ↔ native boundary for the original Android build.

## Summary

`classes.dex` declares **27 native methods**.

The two bundled native libraries expose **21 `Java_*` JNI functions** in total, all from `libcocos2dcpp.so`. `libffmpeg.so` exposes none.

The 21 exports correspond to all Cocos2d-x and Never Gone billing native methods declared in DEX. The remaining six declarations belong to `com.ngds.cocos.GamepadBridge` and have no matching exported `Java_*` functions in either bundled `.so` file.

`JNI_OnLoad` itself is extremely small: in Thumb mode it calls `cocos2d::JniHelper::setJavaVM(JavaVM*)` and returns JNI version `0x00010004` (JNI 1.4). No `RegisterNatives` call is visible in this function.

This makes the `GamepadBridge` implementation an explicit investigation item: it may be dead/incomplete integration, resolved through another mechanism, or rely on code absent from this APK.

## Never Gone-specific JNI

Java class:

```text
com.hippiegame.nevergone.GooglePlayIABPlugin
```

Native methods and matching exports:

| Java method | Native export |
| --- | --- |
| `nativeOnFailed` | `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnFailed` |
| `nativeOnPurchased` | `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnPurchased` |
| `nativeOnReceiveItemInfo` | `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnReceiveItemInfo` |
| `nativeOnRestore` | `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnRestore` |

These callbacks should eventually be isolated behind a platform service interface in the recompilation so obsolete Google Play Billing code cannot block the offline game.

## Cocos2d-x JNI

### `org.cocos2dx.lib.Cocos2dxAccelerometer`

- `onSensorChanged`

### `org.cocos2dx.lib.Cocos2dxBitmap`

- `nativeInitBitmapDC`

### `org.cocos2dx.lib.Cocos2dxHelper`

- `SendInfo`
- `nativeSetApkPath`
- `nativeSetEditTextDialogResult`

`SendInfo` should be compared with the exact Cocos2d-x 2.1.2 Android glue to determine whether it is an upstream function or a Never Gone/vendor modification.

### `org.cocos2dx.lib.Cocos2dxRenderer`

Declared methods:

```text
nativeDeleteBackward
nativeGetContentText
nativeInit
nativeInsertText
nativeKeyDown
nativeOnPause
nativeOnResume
nativeRender
nativeTouchesBegin
nativeTouchesCancel
nativeTouchesEnd
nativeTouchesMove
```

All have matching exported JNI functions in `libcocos2dcpp.so`.

## Gamepad bridge discrepancy

Java class:

```text
com.ngds.cocos.GamepadBridge
```

DEX declares six native methods:

```text
onKeyDown
onKeyPressure
onKeyUp
onLeftStick
onRightStick
onStateEvent
```

The expected static JNI exports would be:

```text
Java_com_ngds_cocos_GamepadBridge_onKeyDown
Java_com_ngds_cocos_GamepadBridge_onKeyPressure
Java_com_ngds_cocos_GamepadBridge_onKeyUp
Java_com_ngds_cocos_GamepadBridge_onLeftStick
Java_com_ngds_cocos_GamepadBridge_onRightStick
Java_com_ngds_cocos_GamepadBridge_onStateEvent
```

None are exported by either bundled `.so` file.

The main native library does, however, contain a C++ `NGGamepadListener` class with symbols including:

```text
NGGamepadListener::GetInstance()
NGGamepadListener::onAxisEvent(NGDSGamePad*, NGDSGamePadJoystick*)
NGGamepadListener::onPressureChange(NGDSGamePad*, NGDSGamePadPressureButton*)
NGGamepadListener::onKeyDown(NGDSGamePad*, NGDSGamePadStateButton*)
NGGamepadListener::onKeyUp(NGDSGamePad*, NGDSGamePadStateButton*)
NGGamepadListener::getHandleConnectState()
NGGamepadListener::callHandleConnectState()
```

This strongly suggests there is or was a native gamepad integration, but the Java native declaration path does not line up with the shipped export table.

### Next checks

1. Search all native code for references to `com/ngds/cocos/GamepadBridge`.
2. Search for `JNINativeMethod`-like tables containing the six Java method names/signatures.
3. Inspect callers of `NGGamepadListener` methods.
4. Decompile `GamepadBridge` to determine whether the native methods are reachable in the shipped build.
5. Confirm whether an omitted vendor library was expected on specific devices.

## `JNI_OnLoad`

The exported symbol address is Thumb-tagged in the ELF symbol table (`0x00284699`). Decoding from the aligned address gives:

```text
push {r3, lr}
bl cocos2d::JniHelper::setJavaVM(JavaVM*)
ldr r0, =0x00010004
pop {r3, pc}
```

Equivalent pseudocode:

```cpp
jint JNI_OnLoad(JavaVM* vm, void*) {
    cocos2d::JniHelper::setJavaVM(vm);
    return JNI_VERSION_1_4;
}
```

No game-specific native registration occurs in this entry point.

## Reproducing the cross-check

Extract `classes.dex`, `libcocos2dcpp.so` and `libffmpeg.so` from a legally obtained original APK, then run:

```bash
python3 tools/dex_native_map.py classes.dex \
  --markdown build/dex-native.md

python3 tools/jni_crosscheck.py classes.dex \
  lib/armeabi-v7a/libcocos2dcpp.so \
  lib/armeabi-v7a/libffmpeg.so \
  --json build/jni-crosscheck.json \
  --markdown build/jni-crosscheck.md
```

Expected baseline result for the known original APK:

```text
DEX native declarations:        27
ELF Java_* exports:             21
Matched declarations:           21
Missing static exports:          6
```

The six missing static exports are the `GamepadBridge` methods listed above. A missing static export does not by itself prove a method is unimplemented; dynamic registration must also be ruled out.
