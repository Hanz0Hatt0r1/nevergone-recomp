# Native CharacterName presentation

This increment turns the recovered CharacterName state/layout into a native GLES modal while keeping text/IME entry and CharacterName actions on the temporary Android compatibility path until the original edit-box/input behavior is reconstructed.

## Original resources only

The loader stages the exact user-imported resources referenced by the recovered CharacterName construction:

- `Login/ChooseHero/Redbottom.png`
- `ServerList/RANDOMName.png`
- `ServerList/RANDOM.png`
- `Common/btn_standard_a.png`
- `Common/btn_standard_b.png`
- `Common/btn_standard_c.png`

Localized label bitmaps are resolved from the imported login CSV for `Prompt9`, `Confirm`, and `Cancel`. Labels use the recovered Arial/20 presentation. Fixed-button labels preserve the existing type-1 maximum-width contract of 115 design pixels.

No fallback or replacement art is embedded. The asset store becomes ready only when the complete recovered set is available. If an APK-only import lacks update/OBB resources such as `ServerList/RANDOMName.png`, the native modal remains inactive and the functional compatibility UI remains available.

## CPU/GLES boundary

`character_name_assets` is a mutex-protected CPU pixel store with generation tracking. Java decodes user-owned PNGs with Android `BitmapFactory`; JNI copies ARGB pixels into the store. The GL compositor copies a snapshot only when the generation changes and lazily creates GLES textures on the render thread.

The host smoke verifies invalid uploads, all-or-nothing readiness, generation changes, and renderer snapshot ownership.

## Layout and draw order

The compositor consumes `character_name_layout` and maps the 1136×640 design surface into the actual GLSurfaceView with aspect-fit letterboxing. Draw order is:

1. `Redbottom`
2. localized `Prompt9`
3. `RANDOMName`
4. `RANDOM`
5. Confirm fixed button + localized label
6. Cancel fixed button + localized label

The transparent `TouMing.png` edit-box background and modal `Button_C` blocker have no visible pixels to reproduce in this draw-only step. Current role-name text is intentionally not invented as a GLES text layer; the existing Android `EditText` remains the temporary IME/text consumer.

## Frame integration

`post_scene_overlay_jni.cpp` is the single reconstructed `JNI_OnLoad` owner. It now registers three wrappers on the existing `GameSurfaceView` ABI: server-selection surface creation, server-selection draw, and the post-SingleSelect splash draw slot used by the native Confirm compositor.

The server-selection wrappers preserve the existing compositor first, then initialize/draw CharacterName. This resolves the older CharacterName branch conflict without introducing a second `JNI_OnLoad` or replacing the Confirm registration added on main.

On the first frame of a new active CharacterName generation, the wrapper invokes `CharacterNameAssetLoader.reloadFromFilesDir(...)` once. Missing resources therefore cause one bounded load attempt, not repeated disk I/O each frame.

## Remaining boundary

This increment is presentation-only. Native CharacterName hit testing/pressed states and replacement of the temporary Android edit-box/IME compatibility controls remain separate steps. The merged `character_name_action_executor` remains the semantic submit/random/cancel execution boundary.
