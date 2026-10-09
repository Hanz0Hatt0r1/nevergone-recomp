# Native CharacterName presentation

This increment turns the previously recovered CharacterName state/layout into a visible modal while keeping text/IME entry on the temporary Android compatibility path until the original edit-box behavior is reconstructed.

## Original resources only

The loader stages the exact user-imported resources referenced by the recovered CharacterName construction:

- `Login/ChooseHero/Redbottom.png`
- `ServerList/RANDOMName.png`
- `ServerList/RANDOM.png`
- `Common/btn_standard_a.png`
- `Common/btn_standard_b.png`
- `Common/btn_standard_c.png`

Localized label bitmaps are resolved from the imported login CSV for:

- `Prompt9`
- `Confirm`
- `Cancel`

Labels use the recovered Arial/20 presentation. Fixed-button labels preserve the existing type-1 maximum-width contract of 115 design pixels.

No fallback/replacement art is embedded. The asset store becomes ready only when the complete recovered set is available. If an APK-only import lacks update/OBB resources such as `ServerList/RANDOMName.png`, the native modal remains inactive and the functional compatibility UI remains available.

## CPU/GLES boundary

`character_name_assets` is a mutex-protected CPU pixel store with generation tracking. Java decodes user-owned PNGs with Android `BitmapFactory`; JNI copies ARGB pixels into the store. The GL compositor copies a snapshot only when the generation changes and lazily creates GLES textures on the render thread.

A host smoke verifies:

- invalid slots/dimensions do not mutate generation;
- all six images and all three labels are required for readiness;
- every successful upload advances generation;
- renderer snapshots remain owned copies after a live-store clear.

## Layout and draw order

The compositor consumes `character_name_layout` rather than duplicating recovered coordinates. It maps the 1136×640 design surface into the actual GLSurfaceView with aspect-fit letterboxing and draws, in order:

1. `Redbottom`
2. localized `Prompt9`
3. `RANDOMName`
4. `RANDOM`
5. Confirm fixed button + localized label
6. Cancel fixed button + localized label

The transparent `TouMing.png` edit-box background and modal `Button_C` blocker have no visible pixels that need to be reproduced in this first draw-only step. Current role-name text is intentionally not invented as a GLES text layer; the existing Android `EditText` remains the temporary IME/text consumer.

## Frame integration

The repository previously had no reconstructed `JNI_OnLoad` implementation. This increment registers wrappers for exactly two existing `GameSurfaceView` native methods:

- `nativeOnServerSelectionSurfaceCreated()`
- `nativeDrawServerSelectionLayer()`

Each wrapper first calls the existing server-selection compositor unchanged. Surface creation additionally initializes the CharacterName GL program; per-frame draw then appends the CharacterName modal when its semantic state is active.

This avoids editing the large `GameSurfaceView` or `single_select_hero_compositor` files while preserving their current behavior. All other Java native methods continue using the existing static `Java_*` exports.

On the first frame of a new active CharacterName generation, the wrapper invokes `CharacterNameAssetLoader.reloadFromFilesDir(...)` once. A missing resource therefore causes one bounded load attempt, not repeated disk I/O each frame.

## Remaining boundary

This increment is presentation-only. Native CharacterName hit testing/pressed states and replacement of the temporary Android edit-box/IME compatibility controls remain separate steps. The already merged `character_name_action_executor` remains the only semantic submit/random/cancel execution boundary.
