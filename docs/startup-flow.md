# Native startup flow

This note records the first reconstructed control-flow path from Cocos2d-x startup into Never Gone-owned UI code.

## `JNI_OnLoad`

The main library's `JNI_OnLoad` is minimal:

```cpp
jint JNI_OnLoad(JavaVM* vm, void*) {
    cocos2d::JniHelper::setJavaVM(vm);
    return JNI_VERSION_1_4;
}
```

No game initialization occurs here.

## `AppDelegate::applicationDidFinishLaunching()`

Symbol:

```text
0x00291ac1 AppDelegate::applicationDidFinishLaunching()
```

The function is Thumb code. Its call sequence is sufficiently clear to express as pseudocode:

```cpp
bool AppDelegate::applicationDidFinishLaunching() {
    CCDirector* director = CCDirector::sharedDirector();
    CCEGLView* view = CCEGLView::sharedOpenGLView();
    director->setOpenGLView(view);

    AddAllSearchPath();

    // A director/display configuration call follows here.
    // Exact virtual method name still needs type/vtable confirmation.

    CCScene* scene = HelloWorld::scene();
    this->initialScene = scene;
    director->runWithScene(scene);
    return true;
}
```

The important result is that the first game-owned scene is **`HelloWorld`**, not `LoadingLayer` or `LoginScreen` directly.

## `HelloWorld::scene()`

Symbol:

```text
0x002c7449 HelloWorld::scene()
```

Observed sequence:

```cpp
CCScene* HelloWorld::scene() {
    CCScene* scene = CCScene::create();
    HelloWorld* layer = HelloWorld::sharedHelloWorld();
    scene->addChild(layer);
    return scene;
}
```

`HelloWorld::init()` initializes a black `CCLayerColor`, resets internal flags and calls `ShowUI()`.

## Splash / logo stage

`HelloWorld::ShowUI()`:

- configures anchor/scale behavior from visible size and GL view dimensions;
- creates an `AdjustScreenResolution` node;
- adds it to the layer;
- schedules a one-shot member callback.

The class contains a chain of functions named:

```text
HelloWorld::ShowLogo()
HelloWorld::GoShowLogo(float)
HelloWorld::FuncNEND(...)
HelloWorld::FuncNEND2(...)
HelloWorld::FuncNEND5(...)
HelloWorld::FuncNEND6(...)
HelloWorld::FuncNEND7(...)
HelloWorld::FuncNENDTest(...)
HelloWorld::BBfuncCall(...)
```

`FuncNEND6` builds a sequence of sprites, delays and fade actions. The APK assets include multiple Hippie Game logo images (`HIPPIEGOLO01.png` through `HIPPIEGOLO06.png`), matching the observed splash-chain behavior.

Near the end of this path, `FuncNEND6` calls:

```text
AppParameters::sharedAppParameters()
AppParameters::getRegionVersions()
```

which indicates region/version state participates in the transition after the logo sequence.

## Transition into the login UI

`HelloWorld::createUI()` is the first confirmed bridge from the splash layer into the game's higher-level management UI.

Observed call:

```text
HelloWorld::createUI()
  -> ManagementLayer::sharedManagementLayer()   [singleton setup path]
  -> ManagementLayer::initLoginLayer()
```

The exact singleton call should be confirmed in Ghidra, but the direct call to `ManagementLayer::initLoginLayer()` is present at approximately `0x002c7cec`.

`ManagementLayer` exposes the following high-value login/entry methods:

```text
ManagementLayer::initLoginLayer()
ManagementLayer::GoToTapToStart()
ManagementLayer::GoToChooseRole()
ManagementLayer::GoToLoginScreen()
ManagementLayer::GoToGameLayer(int, bool)
ManagementLayer::NormalLoginWithName(...)
ManagementLayer::NormalRegisterWithName(...)
ManagementLayer::LoginSuccessful()
ManagementLayer::CreateWaitForTheServerToRespond()
ManagementLayer::RemoveWaitForTheServerToRespond()
ManagementLayer::restartGame()
ManagementLayer::androidSdkCallBack(int, void*)
```

This makes `ManagementLayer` one of the primary reconstruction targets for boot-to-menu work.

## Current startup graph

```text
Android launcher activity (TJ_P_01)
        |
        v
Cocos2d-x Android renderer/JNI
        |
        v
AppDelegate::applicationDidFinishLaunching
        |
        +--> CCDirector / CCEGLView initialization
        |
        +--> AppDelegate::AddAllSearchPath
        |
        +--> HelloWorld::scene
                 |
                 v
          HelloWorld::init
                 |
                 v
          HelloWorld::ShowUI
                 |
                 v
          splash/logo action chain
                 |
                 +--> AppParameters region/version checks
                 |
                 v
          HelloWorld::createUI
                 |
                 v
          ManagementLayer::initLoginLayer
                 |
                 +--> Tap-to-start / login / character selection
                 |
                 v
          ManagementLayer::GoToGameLayer
```

## Next reverse-engineering targets

1. Fully decompile `AppDelegate::AddAllSearchPath()` and record every search path and configuration filename.
2. Decompile `ManagementLayer::initLoginLayer()` and identify which path is usable without the original online backend.
3. Trace `GoToTapToStart()`, `GoToLoginScreen()` and `GoToGameLayer()`.
4. Determine where `DataManager`, `LogicManager` and `GameSaveData` are initialized.
5. Locate the decoder invoked when the encoded `.lua` assets are loaded.
6. Identify whether a local/offline path can bypass unavailable account/server services while preserving normal gameplay initialization.
