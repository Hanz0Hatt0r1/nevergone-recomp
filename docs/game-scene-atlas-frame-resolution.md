# GameScene atlas frame and pixel staging boundary

The recovered default type-0 `GameSceneObject` path calls `CCSpriteFrameCache::spriteFrameByName(serialized_name)`. The project reconstructs the ordered `gsresfile04` preload list for the active `pvp_scene.glData` entry, resolves each user-imported plist to its texture image, resolves unique frame metadata and now stages the referenced atlas pixels into the same project-owned transactional texture path used by direct-file sprites.

## Inputs

The metadata resolver accepts:

- the app-private imported asset root;
- the ordered, already validated `GameSceneAtlasResolver.Atlas` list;
- one logical frame name requested by a render-queue `spriteFrameByName` command.

Every plist and texture path is canonicalized beneath the imported asset root. Plists larger than 4 MiB are rejected.

## Resolved TexturePacker fields

For a unique matching frame, the resolver retains only fields already observed in the user's TexturePacker metadata inventory:

- `textureRect`;
- `spriteColorRect`;
- `spriteOffset`;
- `spriteSize`;
- `spriteSourceSize`;
- `textureRotated`;
- `aliases` for name lookup.

The returned value also records the atlas index, canonical frame name, resolved plist path and resolved texture path. Trim rectangles must remain inside the declared source size and all size fields must be positive.

Exact frame names and aliases are both supported. Membership must be unique across the recovered preload list; ambiguous matches fail closed rather than inventing an atlas priority or overwrite rule.

## Pixel reconstruction

`GameSceneAtlasFramePixels` turns one resolved frame plus its bounded atlas crop into a standalone transparent ARGB sprite:

- normal frames copy the `textureRect` pixels directly;
- `textureRotated=true` uses the Cocos/TexturePacker clockwise 90-degree packing contract, where the logical rect width/height are unrotated while the physical atlas extent is swapped;
- the crop is rotated back to logical orientation before publication;
- `spriteOffset` and `spriteSourceSize` restore the trimmed pixels into the original untrimmed sprite rectangle;
- transparent pixels outside the trim are retained so the existing centered-anchor native quad has the same source-size geometry as the original Cocos sprite frame.

The helper rejects inconsistent logical/packed sizes, escaped trim placement and source images above the existing 16 Mi-pixel per-asset bound. `tools/GameSceneAtlasFramePixelsSmoke.java` covers ordinary, rotated, offset and invalid reconstruction cases without proprietary pixels.

## Transactional staging

The existing GameScene asset snapshot now contains both proven static type-0 resource kinds in render-queue order:

- `CCSprite::create(file)` direct-file resources;
- `spriteFrameByName` atlas-backed resources.

The request revision hashes resource kind, command index and path/frame name. Java stages every request for that exact revision before `GameSceneDirectAssetStore.finish()` publishes anything. Atlas list discovery and frame resolution happen against the same app-private imported root; referenced atlas bitmaps are bounded and cached only for the current staging pass. A missing, malformed, ambiguous or out-of-bounds frame cancels the pending revision, leaving the previous complete texture set untouched.

The native texture cache and GLES renderer do not need a second atlas-specific store. Reconstructed atlas frames arrive as ordinary standalone untrimmed textures keyed by the original sprite-command index, so direct-file and atlas-backed sprites share one revision, one upload/cache boundary and one original layer/object traversal order.

## Security and failure behavior

XML parsing disables external entities and DTD loading. Files are confined to the imported asset root, malformed frame dictionaries are rejected, excessive aliases are bounded, invalid geometry returns no frame, and atlas decode/staging is bounded by per-image and per-pass pixel limits.

No proprietary image or plist payload is committed to the repository. Synthetic fixtures cover metadata lookup and pixel reconstruction; Android compile/renderer workflows cover the integration surface.

## Remaining P7 boundary

The frame-cache metadata/pixel bridge is no longer the P7 blocker. The remaining proof is runtime integration:

1. exercise the complete `server-selection -> ChooseHero -> enter-game -> GameLevels -> static GameScene` route on a real Android target with user-owned imported data;
2. confirm that a real first-scene revision stages every required direct and atlas-backed static type-0 sprite without fallback;
3. capture diagnostics for unsupported nonzero object types or scene-action-pair objects that still prevent the complete scene from matching the original;
4. only after that real-device visual proof close P7 and move the contiguous critical path to player creation/spawn.
