# GameScene null-frame hidden fallback (2026-10-12)

Target: clean-room reconstruction of the user-owned Never Gone 1.0.9 ARMv7 runtime. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

No original binary, decoded proprietary scene data, or image payload is stored here. This note retains only the bounded control-flow/resource behavior needed by the project-owned renderer.

## Recovered behavior

`GameSceneObject::initWithData(GameSceneLayerObjectData*)` enters the ordinary type-0 sprite path by asking the shared `CCSpriteFrameCache` for the serialized frame name and constructing a sprite from the result.

After the exact-name special branches, the function tests the live sprite pointer. If that pointer is null, the shipped runtime:

1. calls `cocos2d::CCSprite::create("empty.png")`;
2. stores the replacement sprite in the same `GameSceneObject` sprite field;
3. dispatches the `cocos2d::CCSprite` virtual slot resolved as `setVisible(bool)` with `false`.

The shipped APK contains `assets/empty.png`, and the project importer exposes that user-owned resource as root-relative `filesDir/assets/empty.png`.

This is a static `GameSceneObject` behavior. It does not establish equivalent fallback semantics for `MeActionsSystem` player-body frame lookup.

## Clean-room staging consequence

For an ordinary non-player `spriteFrameByName` request, a prepared scene-frame resolver that cannot produce a frame now follows the recovered observable result instead of rejecting the entire P7 staging transaction:

- the serialized frame name must still satisfy the bounded safe-frame-name rules;
- imported root `empty.png` must exist inside the canonical asset root;
- the native direct-asset store publishes a hidden slot for the same request index and sprite-command index;
- hidden slots count toward the atomic revision but carry no pixel payload;
- texture-cache synchronization preserves the slot without creating a GLES texture;
- the final renderer skips the hidden command before handle and geometry checks.

Player-body requests remain fail-closed on unresolved frames. Atlas preparation, path containment, plist parsing, pixel bounds, texture decoding, and all visible-frame reconstruction checks also remain fail-closed.

This models the original visible consequence (`empty.png` followed by `setVisible(false)`) without allocating/uploading a texture that can never produce a draw submission.
