# Native binary analysis baseline

This document records reproducible facts from the original Android APK without committing proprietary binaries.

## `libcocos2dcpp.so`

Observed original library:

- path: `lib/armeabi-v7a/libcocos2dcpp.so`
- size: `9,684,428` bytes
- ELF class: `ELF32`
- architecture: ARM / EABI5
- type: shared object (`ET_DYN`)
- status: stripped, but a large dynamic symbol table, RTTI, vtables and diagnostic strings remain

### Engine identification

The binary contains the literal version string:

```text
cocos2d-2.1rc0-x-2.1.2
```

This identifies the engine baseline as Cocos2d-x 2.1.2-era code. This is important because stock engine functions can be matched against upstream source instead of being reconstructed manually.

The binary also preserves a compiler/toolchain string referring to Android NDK r11b and an `arm-linux-androideabi-gcc` Android 9 sysroot. Treat this as evidence about the original build environment, not as a requirement for the recompilation.

### Dynamic dependencies

The original game library declares these `DT_NEEDED` entries:

```text
./obj/local/armeabi-v7a/libffmpeg.so
liblog.so
libz.so
libGLESv2.so
libdl.so
libstdc++.so
libm.so
libc.so
```

The FFmpeg entry embeds a build-machine-relative path rather than a normal SONAME. A compatibility-patched APK has already demonstrated that rewriting it to `libffmpeg.so` improves compatibility with modern Android linkers.

### JNI surface

22 exported JNI entry points were found:

```text
JNI_OnLoad
Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnFailed
Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnPurchased
Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnReceiveItemInfo
Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnRestore
Java_org_cocos2dx_lib_Cocos2dxAccelerometer_onSensorChanged
Java_org_cocos2dx_lib_Cocos2dxBitmap_nativeInitBitmapDC
Java_org_cocos2dx_lib_Cocos2dxHelper_SendInfo
Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetApkPath
Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetEditTextDialogResult
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeDeleteBackward
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeGetContentText
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInsertText
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeKeyDown
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnPause
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnResume
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesBegin
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesCancel
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesEnd
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesMove
```

Only four JNI methods are clearly game-specific; the rest are the old Cocos2d-x Android bridge. This makes the Java/native boundary a relatively small reconstruction target.

### Preserved game RTTI / symbols

The dynamic symbol table and RTTI expose many game types. Confirmed examples include:

```text
AppDelegate
DataManager
LogicManager
NPCManager
EquipManager
GuideManager
ShootManager
BattleManager
ShaderManager
DropDataManager
MEPlayerManager
ActionDataManager
LansquenetManager
MissionDataManager
StateMachineManager
ClothPropertyManager
EquipPropertyManager
PlayerPreviewManager
ToolsUseEffectManager
WeaponsPropertyManager
StateMachineSubManager
ActionsStateMachineManager
```

This is a major advantage over a fully stripped binary: class names, vtables, typeinfo and many exported C++ symbols can seed Ghidra labels automatically.

### Crypto/protocol symbols

The binary also preserves symbols for custom protocol encryption code, including:

```text
hProtocolEncryptInterface
hProtoclEncryptByKey
hProtoclEncryptBySeekAndBitMove
hProtoclEncryptBySeekEvenOdd
hProtocolEncryptMgr
EncryptMemory
DecryptMemory
```

These are currently treated as networking/payment-era implementation details rather than a first boot milestone. They should not block reconstruction of offline startup and rendering.

## `libffmpeg.so`

Observed original library:

- path: `lib/armeabi-v7a/libffmpeg.so`
- size: `14,712,492` bytes
- ELF class: `ELF32`
- architecture: ARM / EABI5

The first modern build should avoid making this old binary a hard requirement. Video playback can be replaced with Android Media APIs or a current FFmpeg build after startup/resource compatibility is established.

## Reconstruction implications

1. Match Cocos2d-x 2.1.2 stock symbols first.
2. Import/demangle the remaining game symbols into Ghidra.
3. Reconstruct `AppDelegate` and the startup path before gameplay classes.
4. Recreate the small Java/JNI bridge using a modern Android project.
5. Keep legacy Google Play Billing and obsolete networking optional until core offline boot works.
6. Build the new native library for both `armeabi-v7a` (comparison) and `arm64-v8a` (primary modern target).

## Next native-analysis tasks

- generate a complete address → demangled-symbol index;
- identify `AppDelegate::applicationDidFinishLaunching` and its direct callees;
- classify symbols as stock Cocos2d-x / third-party / Never Gone;
- map constructors/destructors and vtables for startup-related game classes;
- create Ghidra import scripts for known names;
- locate Lua loader/decode path and determine the script encoding algorithm.
