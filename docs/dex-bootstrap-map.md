# DEX bootstrap map

This note records metadata-only Dalvik evidence for the original Android launcher and Cocos2d-x lifecycle path. The results are produced directly from a user-owned `classes.dex` by `tools/dex_bootstrap_map.py`; no decompiled Java source or proprietary bytecode is committed.

## Manifest launcher

The launcher selected by the original manifest is:

```text
com.hippiegame.nevergone.TJ_P_01
    -> org.cocos2dx.lib.Cocos2dxActivity
    -> android.app.Activity
```

Its source-file metadata is `TJ_P_01.java`.

A second class named `com.cocos2dx.org.TJ_P_01` also exists in the DEX and also extends `Cocos2dxActivity`, but it is not the manifest launcher. This distinction matters for the legacy gamepad integration described below.

## Verified native library load order

The actual launcher class initializer contains three direct `System.loadLibrary(...)` calls in this order:

1. `System.loadLibrary("ffmpeg")`
2. `System.loadLibrary("cocos2dcpp")`
3. `System.loadLibrary("cocos2dcpp")`

The duplicate `cocos2dcpp` call is present in the shipped DEX and is recorded as evidence rather than normalized away. The duplicate `com.cocos2dx.org.TJ_P_01` class contains the same class-initializer sequence.

There is no try/catch around these load calls in the observed code item. This explains why the original malformed `DT_NEEDED` dependency in `libcocos2dcpp.so` is a hard startup compatibility problem rather than a recoverable optional-media path.

## Launcher `onCreate`

The verified call flow from `com.hippiegame.nevergone.TJ_P_01.onCreate(Bundle)` is:

```text
TJ_P_01.onCreate
    -> Cocos2dxActivity.onCreate
       -> Activity.onCreate
       -> new Cocos2dxHandler(...)
       -> Cocos2dxActivity.init
       -> Cocos2dxHelper.init(context, listener)
    -> getWindow().getDecorView()
    -> TJ_P_01.checkPlayServices
    -> new GooglePlayIABPlugin(activity)          [when Play Services check succeeds]
    -> GooglePlayIABPlugin.onCreate(bundle)
```

`Cocos2dxActivity.init()` constructs the Android rendering/view side:

```text
FrameLayout
  + Cocos2dxEditText
  + Cocos2dxGLSurfaceView
        -> setCocos2dxRenderer(new Cocos2dxRenderer())
        -> setCocos2dxEditText(...)

Activity.setContentView(FrameLayout)
```

This gives the clean-room recompilation a verified responsibility map without requiring ABI compatibility with the original Java/C++ bridge.

## Pause/resume forwarding

The original pause path is:

```text
com.hippiegame.nevergone.TJ_P_01.onPause
    -> Cocos2dxActivity.onPause
       -> Activity.onPause
       -> Cocos2dxHelper.onPause
       -> Cocos2dxGLSurfaceView.onPause
          -> queueEvent(Cocos2dxGLSurfaceView$4)
             -> Cocos2dxRenderer.handleOnPause
                -> Cocos2dxRenderer.nativeOnPause
```

The resume path is symmetric:

```text
com.hippiegame.nevergone.TJ_P_01.onResume
    -> Cocos2dxActivity.onResume
       -> Activity.onResume
       -> Cocos2dxHelper.onResume
       -> Cocos2dxGLSurfaceView.onResume
          -> queueEvent(Cocos2dxGLSurfaceView$3)
             -> Cocos2dxRenderer.handleOnResume
                -> Cocos2dxRenderer.nativeOnResume
```

The renderer callback is therefore executed on the GLSurfaceView event queue rather than directly from the Activity lifecycle callback.

## Surface/render bridge

The DEX also verifies the core renderer responsibilities:

- `Cocos2dxGLSurfaceView.setCocos2dxRenderer(...)` installs the renderer through `GLSurfaceView.setRenderer(...)`;
- `Cocos2dxGLSurfaceView.onSizeChanged(...)` forwards width/height to `Cocos2dxRenderer.setScreenWidthAndHeight(...)` when not in edit mode;
- `Cocos2dxRenderer.onSurfaceCreated(...)` calls native `nativeInit(width, height)` and initializes frame timing;
- `Cocos2dxRenderer.onDrawFrame(...)` calls native `nativeRender()` with Java-side frame timing/sleep behavior around it;
- touch handling is queued from `Cocos2dxGLSurfaceView` to renderer action handlers, which then reach the native touch JNI surface already mapped in `docs/jni-map.md`.

## Google Play / IAP boundary

`TJ_P_01.checkPlayServices()` calls `GooglePlayServicesUtil.isGooglePlayServicesAvailable(...)`. Recoverable errors can produce the standard Google Play error dialog. On a successful check, the launcher creates `GooglePlayIABPlugin` and calls its `onCreate(Bundle)`.

`TJ_P_01.onActivityResult(...)` first offers the result to `GooglePlayIABPlugin.handleActivityResult(...)`, logs the handled case, and still forwards through `Cocos2dxActivity.onActivityResult(...)`.

This confirms that Play Services / billing is a Java-side launcher integration, but the clean-room offline boot path does not need to make obsolete billing infrastructure a startup dependency. Purchase-state compatibility can remain behind a separate platform-service boundary.

## `GamepadBridge` classification

DEX contains six native declarations on `com.ngds.cocos.GamepadBridge` without matching static `Java_*` exports. Direct call-site analysis narrows their reachability substantially:

- the manifest launcher `com.hippiegame.nevergone.TJ_P_01` does not directly call `GamepadBridge`;
- the duplicate non-launcher `com.cocos2dx.org.TJ_P_01` calls `GamepadBridge.getInstance()` from its pause/resume path and invokes gamepad state handling;
- direct callers of the duplicate `TJ_P_01` found by the metadata scan are limited to its own inner helper class.

This supports classifying the unmatched `GamepadBridge` JNI surface as an isolated legacy/vendor activity path rather than a normal manifest-launcher boot blocker. It does not prove that reflection, dynamic registration or external/vendor entry points are impossible, so those possibilities remain separate evidence-gathering questions.

## Reproduction

Extract only `classes.dex` from a legally obtained original APK and run:

```bash
unzip -p /path/to/com.hippiegame.nevergone.apk classes.dex > build/classes.dex

python3 tools/dex_bootstrap_map.py build/classes.dex \
  --json build/dex-bootstrap.json \
  --markdown build/dex-bootstrap.md
```

For the normal launcher, the default `--launcher-class` is `com.hippiegame.nevergone.TJ_P_01`.

The tool intentionally operates on DEX metadata/code units only. It records class hierarchy, invoke targets, literal `System.loadLibrary` arguments, selected lifecycle chains and direct reachability evidence without emitting method bodies as decompiled Java source.
