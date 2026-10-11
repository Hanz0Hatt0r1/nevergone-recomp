# GameScene atlas frame and pixel staging boundary

The recovered default type-0 `GameSceneObject` path calls `CCSpriteFrameCache::spriteFrameByName(serialized_name)`. For the first-playable `pvp_scene.glData` route, the project now uses the ordered resource names retained from the binary GameLevels `LoadGL_Global()` section, resolves each user-imported plist to its texture image, resolves unique frame metadata and stages the referenced atlas pixels into the same project-owned transactional texture path used by direct-file sprites.

The original binary stores that ordered array at `GameLevels + 0x24`: `LoadGL_Global()` appends each leading length-prefixed string to it, and `ChooseHeroReadyUIScene::loadGameSceneData(GameLevels*)` later iterates the same array and calls `CCSpriteFrameCache::addSpriteFramesWithFile()` for each name. Generic `GameScene::loadingTex()` routes that operate on `GameLevelsData` continue to have their separate recovered `gsresfile04` source.

## Inputs

The metadata resolver accepts:

- the app-private imported asset root;
- the ordered, already validated `GameSceneAtlasResolver.Atlas` list;
- one logical frame name requested by a render-queue `spriteFrameByName` command.

Every plist and texture path is canonicalized beneath the imported asset root. Plists larger than 4 MiB are rejected.

## Recovered atlas filename lookup

`CCSpriteFrameCache::addSpriteFramesWithFile()` resolves the supplied plist through `CCFileUtils`, so the filename contract is the same one installed by `AppDelegate::AddAllSearchPath()`.

The recovered search vector contains the writable `assets` root first and then exactly 59 child directories in fixed order. The clean-room resolver now reproduces that complete sequence. For a basename or relative resource name it returns the first existing canonical plist in that order, matching Cocos search-path precedence. A later duplicate basename no longer causes a false ambiguity failure. Path traversal or canonical escape outside the imported root still fails closed.

This matters for the first-playable runtime evidence: after the PVP preload source was corrected, the device reported `atlas preload names: 1` but `atlas-resolution-failed`. That proves the binary `LoadGL_Global` name reached Java and localizes the next failure to filename lookup. The previous Java implementation searched only six GameScene directories and rejected duplicate matches even though the original runtime has a complete ordered search path. The resolver now follows the recovered original behavior instead.

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

Exact frame names and aliases are both supported. Frame membership must still be unique across the already ordered preload list; ambiguous frame definitions fail closed rather than inventing atlas overwrite semantics.

## Pixel reconstruction

`GameSceneAtlasFramePixels` turns one resolved frame plus its bounded atlas crop into a standalone transparent ARGB sprite:

- normal frames copy the `textureRect` pixels directly;
- `textureRotated=true` uses the Cocos/TexturePacker clockwise 90-degree packing contract, where the logical rect width/height are unrotated while the physical atlas extent is swapped;
- the crop is rotated back to logical orientation before publication;
- `spriteOffset` and `spriteSourceSize` restore the trimmed pixels into the original untrimmed sprite rectangle;
- transparent pixels outside the trim are retained so the existing centered-anchor native quad has the same source-size geometry as the original Cocos sprite frame.

The helper rejects inconsistent logical/packed sizes, escaped trim placement and source images above the existing 16 Mi-pixel per-asset bound. `tools/GameSceneAtlasFramePixelsSmoke.java` covers ordinary, rotated, offset and invalid reconstruction cases without proprietary pixels.

## Transactional staging

The existing GameScene asset snapshot contains both proven static type-0 resource kinds in render-queue order:

- `CCSprite::create(file)` direct-file resources;
- `spriteFrameByName` atlas-backed resources.

The request revision hashes resource kind, command index and path/frame name. The same JNI refresh captures the retained GameLevels preload names together with that request snapshot, preventing one GL staging pass from mixing two scene revisions. For PVP, no scene-list plist rediscovery is performed: Java resolves exactly the ordered binary GameLevels names retained by the native parser.

Java stages every request for that exact revision before `GameSceneDirectAssetStore.finish()` publishes anything. Frame resolution happens against the same app-private imported root; referenced atlas bitmaps are bounded and cached only for the current staging pass. A missing, malformed, ambiguous or out-of-bounds frame cancels the pending revision, leaving the previous complete texture set untouched.

The native texture cache and GLES renderer do not need a second atlas-specific store. Reconstructed atlas frames arrive as ordinary standalone untrimmed textures keyed by the original sprite-command index, so direct-file and atlas-backed sprites share one revision, one upload/cache boundary and one original layer/object traversal order.

## Runtime diagnostics

`GameSceneAssetStagingDiagnostics` records a resource-name-free staging summary that is safe to surface through the existing runtime diagnostics view. It retains only:

- the current staging state/failure category;
- staging attempt and success counters;
- the observed request revision;
- total and successfully staged request counts;
- the failed request index when applicable;
- the failed request resource kind (`direct-file` or `sprite-frame-by-name`).

`GameLevels runtime model` additionally reports only the number of retained atlas preload names. No serialized frame name, plist name, texture path or imported asset path is emitted.

Failure categories distinguish the important clean-room boundaries, including unavailable/invalid preload lists, atlas resolution, frame resolution, image decode, pixel/crop bounds, frame reconstruction, native store begin/upload/finish, and I/O/runtime failures. Repeated render frames for the same already-failed revision do not repeat expensive decode work or erase the original failure category.

The native bootstrap diagnostics also include the static GameScene renderer state (`shader`, surface size, draw attempts, successful drawn frames and last submitted static sprite count). Together these reports distinguish parsing/construction readiness, Java pixel staging and final GLES submission during an on-device proof.

`tools/GameSceneAssetStagingDiagnosticsSmoke.java` verifies state transitions, counters, resource-kind reporting and that no resource name is present in the report.

## Security and failure behavior

XML parsing disables external entities and DTD loading. Files are confined to the imported asset root, malformed frame dictionaries are rejected, excessive aliases are bounded, invalid geometry returns no frame, and atlas decode/staging is bounded by per-image and per-pass pixel limits.

No proprietary image or plist payload is committed to the repository. Synthetic fixtures cover ordered search-path lookup, metadata lookup, pixel reconstruction and diagnostic state; Android compile/renderer workflows cover the integration surface.

## Remaining P7 boundary

The current device evidence has progressed through both earlier boundaries: `atlas-list-discovery-failed` was eliminated by using the binary GameLevels preload array, and the resulting `atlas-resolution-failed` is now addressed by reproducing the complete recovered Cocos search path and first-match precedence.

The next runtime integration pass should:

1. exercise the fresh-role offline handoff into `GameLevels` with user-owned imported data;
2. confirm `GameLevels runtime model` still reports the nonzero atlas preload count;
3. confirm whether the real first-scene revision now reaches `GameScene asset staging: ready`, or records the next concrete frame/decode boundary;
4. if staging succeeds, confirm the static GLES renderer records successful drawn frames and visually inspect the retained screenshot;
5. then run the joystick/player-input evidence harness against that live scene.
