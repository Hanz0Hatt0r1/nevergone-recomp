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

    // Director timing/display configuration follows here.
    // The loaded interval strongly indicates an approximately 35 Hz target;
    // the exact virtual slot still needs final vtable confirmation.

    CCScene* scene = HelloWorld::scene();
    this->initialScene = scene;
    director->runWithScene(scene);
    return true;
}
```

The important result is that the first game-owned scene is **`HelloWorld`**, not `LoadingLayer` or `LoginScreen` directly.

### Frame pacing evidence

Immediately before the director configuration call, `AppDelegate::applicationDidFinishLaunching()` loads the double value:

```text
0.02857142873108387 seconds
```

which is effectively `1 / 35` second. The shipped ELF also retains the named Cocos2d-x symbol:

```text
cocos2d::CCDisplayLinkDirector::setAnimationInterval(double)
```

Together these are strong evidence that the original client targeted approximately **35 updates/frames per second** through the display-link director. The specific virtual call slot in `applicationDidFinishLaunching()` has not yet been formally matched to the vtable entry, so the recomp currently records 35 Hz as the original timing target rather than forcing the new GLES surface to that rate prematurely.

The modern renderer should keep this distinction: Android presentation may remain synchronized to the device display, while the reconstructed game/update loop can adopt the verified original interval once the scene scheduler is restored.

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

A direct call to:

```text
ManagementLayer::initLoginLayer()
```

is present at approximately `0x002c7cec`.

`ManagementLayer` exposes the following high-value entry-flow methods:

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
        +--> director timing target (~35 Hz evidence)
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
                 +--> tap-to-start / login / character selection
                 |
                 v
          ManagementLayer::GoToGameLayer
```

## Reverse-engineering implications

The startup path is now narrow enough to keep reconstruction dependency-driven:

1. Finish the Android/Cocos lifecycle map around `TJ_P_01`, `CCEGLView`, pause/resume and display timing.
2. Restore the minimum scene/scheduler behavior needed to reproduce the `HelloWorld` splash path on the project-owned GLES surface.
3. Reconstruct `ManagementLayer::initLoginLayer()`, `GoToTapToStart()`, `GoToLoginScreen()` and `GoToGameLayer()` around the already recovered Lua/client callback boundary.
4. Identify where `DataManager`, `LogicManager` and `GameSaveData` become mandatory for the first offline scene.
5. Preserve the original approximately 35 Hz game timing separately from physical display refresh unless later evidence shows they must be coupled.

The 59 startup search paths and the transformed Lua/resource decoder have already been recovered and implemented in the modern runtime/import path, so further native work should focus on scene, timing, UI and gameplay behavior rather than re-solving resource discovery.

A modern recompilation does not need to preserve the obsolete online login stack exactly. The intended preservation target should isolate or stub external services while keeping the original local initialization and gameplay state transitions.
