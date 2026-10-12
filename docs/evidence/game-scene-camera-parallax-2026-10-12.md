# GameScene camera/parallax evidence: 2026-10-12

## Scope and provenance

This note records derived clean-room observations from the user-owned Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so` (SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`). No original binary, asset, screenshot, or decompiler listing is committed.

The purpose of this slice is limited to the horizontal camera transform needed by the first reconstructed GameScene renderer. Live camera actions, zoom, vertical camera behavior, combat camera events, and collision response remain separate boundaries.

## Serialized layer factor

`GameScene::loadingTex()` starts at aligned Thumb address `0x00346678`. It creates 11 `GameSceneClassLayer` instances with IDs `1..11`, then visits source scene layers by ordered index `0..10`.

For every visited source layer, the shipped code loads `GameSceneLayerData +0x14` and writes it to the corresponding `GameSceneClassLayer +0x140` at `0x0034675a..0x00346764`. The parser field that reaches this location is `LayerRecord::first_float`.

The field is therefore not a layer Z value. Its downstream use in `GameSceneClassLayer::move(CCPoint)` proves it is the class-layer movement/camera factor.

## Per-frame camera delivery

`GameScene::updateCameraTrack(float)` starts at `0x00341076`. It reads the GameScene camera eye with `CCCamera::getEyeXYZ()`, compares eye X with the retained previous eye X at `GameScene +0x1d0`, and, when X changed, visits exactly 11 class-layer pointers starting at `GameScene +0x114`.

Each layer receives:

```text
GameSceneClassLayer::move(CCPoint(cameraEyeX, 0))
```

After the loop the current eye X is retained at `GameScene +0x1d0`.

`GameSceneObject::move(CCPoint)` at `0x0034d4d0` dispatches the CCNode position setter on the individual GameSceneObject parent. The serialized sprite position remains local to that parent. A flat renderer therefore cannot draw only the serialized sprite coordinates: it must compose this parent movement with the GameScene camera transform.

## Horizontal class-layer formula

`GameSceneClassLayer::move(CCPoint)` starts at `0x0034a234`.

For class-layer IDs other than `8`, the X coordinate sent to each contained GameSceneObject is:

```text
objectParentX = (1.0 - layerFactor) * cameraEyeX
```

Class-layer ID `8` is special. With `updateCameraTrack()` supplying input Y=0, it sends parent position `(0, 0)` instead. Because source layer index `7` is created as class-layer ID `8`, this special case belongs to source layer `7`.

The GameScene camera moves both eye X and center X together. That translates the complete GameScene world by `-cameraEyeX`. The equivalent flat-renderer horizontal translations are therefore:

```text
source layer 7:
    staticScreenX = serializedX - cameraEyeX

all other source layers:
    staticScreenX = serializedX - layerFactor * cameraEyeX

BattleManager / MEPlayer world content:
    screenX = worldX - cameraEyeX
```

The fixed Controls joystick is outside the serialized GameScene world-layer transform and remains in screen space.

## Initial horizontal camera position

The matching type-6 entry-marker path in `GameScene::loadingTex()` first recovers the player scene position using the already documented 1136x640 Cocos win size. The same path then computes the initial horizontal camera displacement.

The literal at `0x00346a24` is `0.12f`. With `W = CCDirector::getWinSize().width`, the shipped calculation is:

```text
threshold = W * (0.5 + 0.12)
rawEyeX = markerSourceX - threshold
maxEyeX = portExtentX - W * 0.5
cameraEyeX = clamp(rawEyeX, 0, maxEyeX)
```

`portExtentX` is loaded from the current `GameScenePortNodeData +0x4b4`. At the beginning of `loadingTex()`, that value is copied to `GameScene +0x4b0`. In the clean-room parser it is retained as the selected `PortNodeRecord::fifth_u32`.

The original converts the `+0x4b4` word through a signed-int-to-float conversion before subtracting half the win width; the reconstruction preserves that conversion.

The camera displacement is added to both the retained initial camera center X and eye X before `CCCamera::setCenterXYZ()` / `setEyeXYZ()` are called. `CCCamera::restore()` at `0x005097fe` initializes eye X and center X to zero, establishing the normal first-scene horizontal base used by this path.

## Reconstructed renderer boundary

The project-owned renderer now retains each layer camera factor in its sprite command and, before inserting the BattleManager/player branch, applies the flattened static-world camera transform. The player receives the complete `-cameraEyeX` world translation and the joystick is appended afterward without camera translation.

This patch intentionally reconstructs the initial `loadingTex()` camera position plus its class-layer parallax composition. It does **not** yet make the camera follow later player movement or reproduce queued `MyCameraMove`/zoom/event camera actions. Those require a separate evidence slice rather than extrapolating the initial formula.
