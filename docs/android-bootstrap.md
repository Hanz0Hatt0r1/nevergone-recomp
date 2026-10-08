# Android bootstrap map

This document consolidates the reconstructed Android → JNI → native scene bootstrap for the known original Never Gone APK. Confirmed Java/Dalvik behavior is now derived directly from `classes.dex` with project-owned metadata tooling rather than inferred from method names.

## Known original application identity

- Package: `com.hippiegame.nevergone`
- Version: `1.0.9` (`versionCode 9`)
- Launcher activity: `com.hippiegame.nevergone.TJ_P_01`
- Native game library: `lib/armeabi-v7a/libcocos2dcpp.so`
- Bundled media library: `lib/armeabi-v7a/libffmpeg.so`
- Engine family/version evidence: `cocos2d-2.1rc0-x-2.1.2`
- Lua runtime evidence: `Lua 5.2.3`

The shipped APK has no arm64 native library. The recompilation therefore treats ARMv7 as a behavior-comparison target and arm64-v8a as the primary modern target rather than attempting to load the original binary.

## Verified Java/Dalvik launcher flow

Direct DEX code-item analysis establishes the manifest launcher's hierarchy:

```text
com.hippiegame.nevergone.TJ_P_01
    -> org.cocos2dx.lib.Cocos2dxActivity
    -> android.app.Activity
```

Its static initializer calls `System.loadLibrary(...)` in the following exact order:

```text
ffmpeg
cocos2dcpp
cocos2dcpp
```

The second `cocos2dcpp` load is genuinely duplicated in the shipped bytecode. There is no observed try/catch around these three calls.

The launcher `onCreate(Bundle)` calls `Cocos2dxActivity.onCreate`, obtains the decor view, checks Google Play Services and, when that check succeeds, creates `GooglePlayIABPlugin` and calls its `onCreate(Bundle)`.

`Cocos2dxActivity.onCreate()` performs the upstream-style Cocos Android setup:

```text
Activity.onCreate
  -> new Cocos2dxHandler(...)
  -> Cocos2dxActivity.init()
  -> Cocos2dxHelper.init(context, listener)
```

`Cocos2dxActivity.init()` creates the frame layout, `Cocos2dxEditText`, `Cocos2dxGLSurfaceView` and `Cocos2dxRenderer`, attaches the renderer/edit-text bridge and installs the resulting frame as the Activity content view.

See `docs/dex-bootstrap-map.md` for the reproducible ordered-call evidence.

## DEX/native boundary

`classes.dex` declares 27 native methods. Static ELF analysis finds 21 matching `Java_*` JNI exports, all in `libcocos2dcpp.so`; `libffmpeg.so` exposes no Java JNI entry points.

The matched JNI surface consists of standard/upstream-style Cocos2d-x Android helpers plus four Never Gone Google Play billing callbacks. Six native declarations in `com.ngds.cocos.GamepadBridge` have no matching static `Java_*` exports.

See `docs/jni-map.md` for the complete declaration/export cross-check.

## `JNI_OnLoad`

The original `JNI_OnLoad` is minimal. Recovered behavior is equivalent to:

```cpp
jint JNI_OnLoad(JavaVM* vm, void*) {
    cocos2d::JniHelper::setJavaVM(vm);
    return JNI_VERSION_1_4;
}
```

No game initialization or `RegisterNatives` call is visible there. The first game-owned scene is reached through the ordinary Cocos2d-x application/bootstrap path rather than custom registration inside `JNI_OnLoad`.

## Verified pause/resume forwarding

Pause reaches native code through the GLSurfaceView event queue:

```text
TJ_P_01.onPause
  -> Cocos2dxActivity.onPause
     -> Activity.onPause
     -> Cocos2dxHelper.onPause
     -> Cocos2dxGLSurfaceView.onPause
        -> queueEvent(Cocos2dxGLSurfaceView$4)
           -> Cocos2dxRenderer.handleOnPause
              -> Cocos2dxRenderer.nativeOnPause
```

Resume is symmetric:

```text
TJ_P_01.onResume
  -> Cocos2dxActivity.onResume
     -> Activity.onResume
     -> Cocos2dxHelper.onResume
     -> Cocos2dxGLSurfaceView.onResume
        -> queueEvent(Cocos2dxGLSurfaceView$3)
           -> Cocos2dxRenderer.handleOnResume
              -> Cocos2dxRenderer.nativeOnResume
```

This confirms that native renderer lifecycle callbacks are dispatched on the GL event queue rather than directly from the Activity callback.

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

Additional DEX call-flow evidence verifies that `Cocos2dxRenderer.onSurfaceCreated(...)` calls `nativeInit(width, height)`, while `onDrawFrame(...)` calls `nativeRender()` around Java-side timing logic. `Cocos2dxGLSurfaceView.onSizeChanged(...)` forwards dimensions to the renderer, and touch work is queued from the GL view to renderer action handlers before reaching the mapped native touch JNI surface.

## First confirmed game-owned native scene

After the renderer/native bootstrap, recovered native flow is:

```text
Cocos2dxRenderer.nativeInit(...)
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

See `docs/startup-flow.md` for native addresses, symbols and splash/login transition evidence.

## Google Play / IAP boundary

The original launcher performs a direct Play Services availability check. Recoverable failures can show the standard Google Play error dialog. After a successful check, the launcher constructs `GooglePlayIABPlugin` and calls its `onCreate(Bundle)`.

`TJ_P_01.onActivityResult(...)` first offers the result to `GooglePlayIABPlugin.handleActivityResult(...)`, then forwards to `Cocos2dxActivity.onActivityResult(...)`.

The original native library exposes these Never Gone-specific billing callbacks:

```text
nativeOnFailed
nativeOnPurchased
nativeOnReceiveItemInfo
nativeOnRestore
```

For preservation, this integration remains outside the offline boot dependency chain. If purchase-state compatibility becomes necessary, it should live behind a project-owned platform-service interface instead of restoring obsolete Google Play Billing as a startup requirement.

## GamepadBridge classification

DEX declares six `com.ngds.cocos.GamepadBridge` native methods:

```text
onKeyDown
onKeyPressure
onKeyUp
onLeftStick
onRightStick
onStateEvent
```

No static JNI exports exist for them in either bundled native library. Direct DEX reachability analysis shows that the manifest launcher does not call `GamepadBridge`. Instead, gamepad calls occur from a second non-launcher activity class named `com.cocos2dx.org.TJ_P_01`, whose pause/resume path registers/removes the gamepad listener and handles state events.

The duplicate `TJ_P_01` has no direct caller from the normal manifest-launcher path in the metadata scan beyond its own inner helper. The six unmatched natives are therefore classified as an isolated legacy/vendor activity path and are not a current boot blocker. Reflection, dynamic registration or external vendor entry remain possible and are not claimed to be disproven.

## Modern recompilation mapping

The recompilation replaces the old Java/Cocos native boundary with project-owned equivalents while preserving responsibilities rather than ABI compatibility:

| Original responsibility | Recomp implementation |
| --- | --- |
| Load original ARMv7 game/media libraries | Load project-owned `libnevergone_recomp.so` |
| Cocos renderer surface callbacks | `GameSurfaceView` + `render_bridge` GLES2 callbacks |
| Touch JNI | `GameSurfaceView.nativeOnTouch` → project-owned render/input bridge |
| Renderer pause/resume | Activity/GLSurfaceView lifecycle + project-owned native lifecycle state |
| Original asset APK access | User-selected original APK imported through SAF into app-private storage |
| Custom resource decode | Recovered clean-room `cocos2d::Decode` transform in importer/tooling |
| Lua 5.2.3 runtime | Verified source-fetched Lua 5.2.3 built into project-owned native runtime |
| Original startup Lua/native globals | Clean-room binding registry and compatibility implementations |
| Native login/client callbacks | Typed diagnostic/event bridge + persistent `ClientUiSnapshot` |
| Original rendering | Project-owned GLES2 substrate; original scene/UI behavior still under reconstruction |

The original APK is used only as a user-supplied source for game resources and reverse-engineering evidence; the modern runtime does not load its native game library.

## Modern lifecycle sequence

The current recomp application performs this high-level sequence:

```text
MainActivity class load
  -> System.loadLibrary("nevergone_recomp")
MainActivity.onCreate
  -> configure app-private files root / UUID / version through JNI
  -> create GameSurfaceView (GLES2)
  -> expose original-APK SAF importer and diagnostics
GameSurfaceView renderer
  -> nativeOnSurfaceCreated
  -> nativeOnSurfaceChanged
  -> nativeOnDrawFrame
  -> nativeOnTouch
Activity lifecycle
  -> GLSurfaceView pause/resume
  -> project-owned native lifecycle state
```

After user-owned assets are imported, startup diagnostics can execute decoded `Game.StartLua` through the reconstructed Lua runtime and update persistent client/UI state observed by the GLES diagnostic renderer.

## Remaining Android bootstrap questions

The critical launcher/library/render/lifecycle path is now mapped directly from DEX. Remaining Java-side work is narrower:

1. Preserve a broader JADX-derived class map for non-critical/vendor integrations and use it as a readability cross-check against the direct DEX evidence.
2. Rule in/out reflective or external reachability of the duplicate `com.cocos2dx.org.TJ_P_01` and `GamepadBridge` path if gamepad restoration becomes relevant.
3. Classify any purchase-state data that offline progression may actually depend on before implementing a modern billing/platform-service boundary.
4. Validate the reconstructed lifecycle/render path on physical/emulated Android targets, including 16 KiB-page arm64 devices.

## Reproduction inputs

The metadata can be regenerated from a legally obtained original APK:

```bash
unzip -p /path/to/original.apk classes.dex > build/classes.dex

python3 tools/dex_bootstrap_map.py build/classes.dex \
  --json build/dex-bootstrap.json \
  --markdown build/dex-bootstrap.md

python3 tools/dex_native_map.py build/classes.dex \
  --markdown build/dex-native.md

python3 tools/jni_crosscheck.py build/classes.dex \
  lib/armeabi-v7a/libcocos2dcpp.so \
  lib/armeabi-v7a/libffmpeg.so \
  --json build/jni-crosscheck.json \
  --markdown build/jni-crosscheck.md
```

Known JNI baseline:

```text
DEX native declarations: 27
ELF Java_* exports:      21
Matched declarations:   21
Missing static exports:  6
```

The six missing static exports are the isolated `GamepadBridge` declarations discussed above.
