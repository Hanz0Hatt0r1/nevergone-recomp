# Android bootstrap map

This document consolidates the reconstructed Android → JNI → native scene bootstrap for the known original Never Gone APK. It intentionally separates confirmed evidence from unresolved Java/vendor behavior so the modern recompilation does not depend on guessed startup details.

## Known original application identity

- Package: `com.hippiegame.nevergone`
- Version: `1.0.9` (`versionCode 9`)
- Launcher activity: `com.hippiegame.nevergone.TJ_P_01`
- Native game library: `lib/armeabi-v7a/libcocos2dcpp.so`
- Bundled media library: `lib/armeabi-v7a/libffmpeg.so`
- Engine family/version evidence: `cocos2d-2.1rc0-x-2.1.2`
- Lua runtime evidence: `Lua 5.2.3`

The shipped APK has no arm64 native library. The recompilation therefore treats ARMv7 as a behavior-comparison target and arm64-v8a as the primary modern target rather than attempting to load the original binary.

## DEX/native boundary

`classes.dex` declares 27 native methods. Static ELF analysis finds 21 matching `Java_*` JNI exports, all in `libcocos2dcpp.so`; `libffmpeg.so` exposes no Java JNI entry points.

The matched JNI surface consists of:

- standard/upstream-style Cocos2d-x Android helpers (`Cocos2dxRenderer`, `Cocos2dxHelper`, bitmap and accelerometer glue), and
- four Never Gone Google Play billing callbacks.

Six native declarations in `com.ngds.cocos.GamepadBridge` have no matching static `Java_*` exports. Their implementation/reachability remains unresolved and must not be assumed to exist merely because the Java declarations are present.

See `docs/jni-map.md` for the complete declaration/export cross-check.

## `JNI_OnLoad`

The original `JNI_OnLoad` is minimal. Recovered behavior is equivalent to:

```cpp
jint JNI_OnLoad(JavaVM* vm, void*) {
    cocos2d::JniHelper::setJavaVM(vm);
    return JNI_VERSION_1_4;
}
```

No game initialization or `RegisterNatives` call is visible there. This means the first game-owned scene is reached through the ordinary Cocos2d-x application/bootstrap path rather than through custom registration inside `JNI_OnLoad`.

## Original renderer-facing JNI

The original DEX declares and the native library exports the following `org.cocos2dx.lib.Cocos2dxRenderer` methods:

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

These names establish the original Java/native responsibilities at a high level: surface initialization/rendering, Activity/surface pause-resume forwarding, text input, key input and touch forwarding.

The exact Java call graph from `TJ_P_01` through the Cocos activity/view classes still requires a JADX metadata pass. In particular, this document does **not** claim a verified order for `System.loadLibrary(...)` calls until that DEX call graph is preserved explicitly.

## First confirmed game-owned native scene

The recovered native flow after Cocos initialization is:

```text
Android launcher activity (TJ_P_01)
        |
        v
Cocos2d-x Android view/renderer JNI
        |
        v
AppDelegate::applicationDidFinishLaunching()
        |
        +--> CCDirector::sharedDirector()
        +--> CCEGLView::sharedOpenGLView()
        +--> director->setOpenGLView(...)
        +--> AppDelegate::AddAllSearchPath()
        +--> director timing configuration (~35 Hz evidence)
        |
        v
HelloWorld::scene()
        |
        v
HelloWorld::init() / ShowUI()
        |
        v
splash/logo action chain
        |
        v
HelloWorld::createUI()
        |
        v
ManagementLayer::initLoginLayer()
```

`AppDelegate::applicationDidFinishLaunching()` loads approximately `1 / 35` second immediately before director timing configuration. The recompilation records this as original game/update timing evidence but does not force the modern display presentation loop to 35 Hz.

See `docs/startup-flow.md` for addresses, symbols and the splash/login transition evidence.

## Modern recompilation mapping

The recompilation replaces the old Java/Cocos native boundary with project-owned equivalents while preserving responsibilities rather than ABI compatibility:

| Original responsibility | Recomp implementation |
| --- | --- |
| Load original ARMv7 game library | Load project-owned `libnevergone_recomp.so` |
| Cocos renderer surface callbacks | `GameSurfaceView` + `render_bridge` GLES2 callbacks |
| Touch JNI | `GameSurfaceView.nativeOnTouch` → project-owned render/input bridge |
| Renderer pause/resume | Activity/GLSurfaceView lifecycle + project-owned native lifecycle state |
| Original asset APK access | User-selected original APK imported through SAF into app-private storage |
| Custom resource decode | Recovered clean-room `cocos2d::Decode` transform in importer/tooling |
| Lua 5.2.3 runtime | Verified source-fetched Lua 5.2.3 built into project-owned native runtime |
| Original startup Lua/native globals | Clean-room binding registry and compatibility implementations |
| Native login/client callbacks | Typed diagnostic/event bridge + persistent `ClientUiSnapshot` |
| Original rendering | Project-owned GLES2 substrate; original scene/UI behavior still under reconstruction |

This mapping deliberately avoids loading `libcocos2dcpp.so` from the user APK. The original APK is used only as a user-supplied source for game resources and reverse-engineering evidence.

## Modern lifecycle sequence

The current recomp application performs the following high-level sequence:

```text
MainActivity class load
        |
        +--> System.loadLibrary("nevergone_recomp")
        |
MainActivity.onCreate()
        |
        +--> configure app-private files root / UUID / version through JNI
        +--> create GameSurfaceView (GLES2)
        +--> expose original-APK SAF importer and diagnostics
        |
GameSurfaceView renderer
        |
        +--> nativeOnSurfaceCreated
        +--> nativeOnSurfaceChanged
        +--> nativeOnDrawFrame
        +--> nativeOnTouch
        |
Activity lifecycle
        |
        +--> GLSurfaceView pause/resume
        +--> project-owned native lifecycle state
```

After user-owned assets are imported, startup diagnostics can execute decoded `Game.StartLua` through the reconstructed Lua runtime and update the persistent client/UI state observed by the GLES diagnostic renderer.

## Google Play billing boundary

The original APK has four Never Gone-specific JNI callbacks on `GooglePlayIABPlugin`:

```text
nativeOnFailed
nativeOnPurchased
nativeOnReceiveItemInfo
nativeOnRestore
```

These should remain isolated from the offline preservation path. The recompilation should expose a platform-service boundary if purchase state is eventually needed, rather than making obsolete Google Play Billing a boot dependency.

No replacement billing implementation is currently required for title/login/offline boot reconstruction.

## GamepadBridge discrepancy

DEX declares six native GamepadBridge methods:

```text
onKeyDown
onKeyPressure
onKeyUp
onLeftStick
onRightStick
onStateEvent
```

No static JNI exports exist for them in either bundled native library. The game library does retain an `NGGamepadListener` C++ subsystem, so likely explanations include dynamic registration, dead/incomplete vendor integration or an omitted device-specific component. This remains an evidence-gathering task rather than a modern bootstrap blocker.

## Remaining Android bootstrap questions

The following items are intentionally still open:

1. Preserve a JADX-derived class/lifecycle map for `TJ_P_01` and the Cocos Java glue.
2. Record the exact original `System.loadLibrary(...)` call sites and order from DEX, including whether `libffmpeg.so` is loaded explicitly or through another path.
3. Determine whether the six GamepadBridge declarations are reachable and whether any dynamic native registration table exists outside `JNI_OnLoad`.
4. Document the original Activity/view pause/resume call graph around `Cocos2dxRenderer.nativeOnPause/nativeOnResume` rather than inferring it from method names alone.
5. Classify original Google Play/IAP Java entry points by reachability and isolate any local game-state dependency from obsolete service calls.
6. Validate the reconstructed lifecycle/render path on physical/emulated Android targets, including 16 KiB-page arm64 devices.

## Reproduction inputs

The metadata in this map can be regenerated from a legally obtained original APK using the existing repository tools. The key machine-generated checks are:

```bash
python3 tools/dex_native_map.py classes.dex --markdown build/dex-native.md

python3 tools/jni_crosscheck.py classes.dex \
  lib/armeabi-v7a/libcocos2dcpp.so \
  lib/armeabi-v7a/libffmpeg.so \
  --json build/jni-crosscheck.json \
  --markdown build/jni-crosscheck.md
```

Known baseline:

```text
DEX native declarations: 27
ELF Java_* exports:      21
Matched declarations:   21
Missing static exports:  6
```

The six missing static exports are the GamepadBridge methods above. Missing static exports alone do not prove the methods are unimplemented; dynamic registration and Java reachability still need to be ruled out.
