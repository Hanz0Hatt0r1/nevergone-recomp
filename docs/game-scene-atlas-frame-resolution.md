# GameScene atlas frame metadata boundary

The recovered default type-0 `GameSceneObject` path calls `CCSpriteFrameCache::spriteFrameByName(serialized_name)`. The project already reconstructs the ordered `gsresfile04` preload list for the active `pvp_scene.glData` entry and resolves each user-imported plist to its texture image. `GameSceneAtlasFrameResolver` closes the next metadata-only boundary without decoding or redistributing any game asset bytes.

## Inputs

The resolver accepts:

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

## Security and failure behavior

XML parsing disables external entities and DTD loading. Files are confined to the imported asset root, malformed frame dictionaries are rejected, excessive aliases are bounded, and invalid geometry returns no frame.

No source atlas pixels are read by this component. `tools/GameSceneAtlasFrameResolverSmoke.java` builds synthetic plists and dummy texture files to cover exact lookup, alias lookup, rotated metadata, missing frames, duplicate membership, invalid trim bounds and path escape rejection.

## Next renderer boundary

The remaining P7 staging step is pixel-side rather than metadata discovery:

1. snapshot the live render queue's `spriteFrameByName` commands;
2. discover the selected scene's atlas preload list and resolve its atlas files;
3. resolve every requested frame through this metadata boundary;
4. decode only the referenced user-owned atlas textures;
5. preserve TexturePacker trim/rotation/source-size semantics when publishing textures/quads to native GLES;
6. replace the entered-game fallback only after the complete queue revision is staged successfully.

Direct-file sprites already use the same transactional revision rule. Atlas-backed staging should keep that property so a scene transition can never mix frame metadata or texture handles from different revisions.
