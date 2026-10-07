# JNI map

This document records the initial Java ↔ native boundary for the original Android build.

## Summary

`classes.dex` declares **27 native methods**.

`libcocos2dcpp.so` exports **21 `Java_*` JNI functions** plus `JNI_OnLoad`.

The 21 normal exports correspond to all Cocos2d-x and Never Gone billing native methods declared in DEX. The remaining six declarations belong to `com.ngds.cocos.GamepadBridge` and have no matching exported `Java_*` functions in either bundled `.so` file.

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

These callbacks should eventually be replaced by a platform service interface in the recompilation so obsolete Google Play Billing code cannot block the offline game.

## Cocos2d-x JNI

### `org.cocos2dx.lib.Cocos2dxAccelerometer`

- `onSensorChanged`

Export:

```text
Java_org_cocos2dx_lib_Cocos2dxAccelerometer_onSensorChanged
```

### `org.cocos2dx.lib.Cocos2dxBitmap`

- `nativeInitBitmapDC`

Export:

```text
Java_org_cocos2dx_lib_Cocos2dxBitmap_nativeInitBitmapDC
```

### `org.cocos2dx.lib.Cocos2dxHelper`

- `SendInfo`
- `nativeSetApkPath`
- `nativeSetEditTextDialogResult`

Exports:

```text
Java_org_cocos2dx_lib_Cocos2dxHelper_SendInfo
Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetApkPath
Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetEditTextDialogResult
```

`SendInfo` is not part of the most minimal stock bridge surface and deserves comparison with the exact Cocos2d-x 2.1.2 tree to determine whether it is a Never Gone modification.

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

No matching symbols of the form below are exported by `libcocos2dcpp.so`:

```text
Java_com_ngds_cocos_GamepadBridge_*
```

No matching JNI exports were found in `libffmpeg.so` either.

The native library does, however, contain a C++ `NGGamepadListener` class with symbols including:

```text
NGGamepadListener::GetInstance()
NGGamepadListener::onAxisEvent(NGDSGamePad*, NGDSGamePadJoystick*)
NGGamepadListener::onPressureChange(NGDSGamePad*, NGDSGamePadPressureButton*)
NGGamepadListener::onKeyDown(NGDSGamePad*, NGDSGamePadStateButton*)
NGGamepadListener::onKeyUp(NGDSGamePad*, NGDSGamePadStateButton*)
NGGamepadListener::getHandleConnectState()
NGGamepadListener::callHandleConnectState()
```

This strongly suggests there is/was a native gamepad integration, but the Java native declaration path does not line up with the exported symbol table.

### Next checks

1. Search all native code for references to the `GamepadBridge` class descriptor.
2. Search for `JNINativeMethod`-like tables containing the six Java method names/signatures.
3. Inspect callers of `NGGamepadListener` methods.
4. Decompile `GamepadBridge` to determine whether these methods are reachable in the shipped build.
5. Confirm whether an omitted vendor library was expected on specific devices.

## `JNI_OnLoad`

The exported symbol address is Thumb-tagged in the ELF symbol table (`0x00284699`). Decoding from the aligned address gives the following logic:

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

No additional game registration happens in this entry point.

## Tooling

Generate the DEX side of this map with:

```bash
python3 tools/dex_native_map.py classes.dex --markdown analysis/dex-native.md
```

Generate the ELF side with:

```bash
python3 tools/elf_native_map.py libcocos2dcpp.so --markdown analysis/native-map.md
```

A later tool should join these two outputs automatically and report matched/missing JNI methods.
