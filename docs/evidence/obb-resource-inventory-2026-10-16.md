# Original OBB resource inventory (2026-10-16)

The user-owned `main.9.com.hippiegame.nevergone.obb` was inspected locally. No archive payload or full path index is committed. The existing `tools/resource_index.py` wrote a local metadata index to `/tmp/nevergone-obb-index.json` from the extracted asset tree.

| Check | Result |
| --- | ---: |
| ZIP entries under `assets/` | 8,141 |
| Uncompressed bytes | 678,685,026 |
| PNG | 3,749 |
| plist | 970 |
| CSV | 276 |
| MP3 | 1,066 |
| `.actData` | 1,111 |
| Failed ZIP CRC entries | 0 |
| Invalid decoded PNG signature/IHDR headers | 0 |
| Invalid plist XML/binary headers | 0 |
| Basenames appearing in multiple directories | 342 |

The original APK contains 270 asset paths; all 270 also occur in the OBB. The importer already preserves the complete relative directory tree and decodes the shipped PNG/CSV/Lua/HPC wrapper while copying resources. A flattened basename map would be ambiguous for 342 names and would change Cocos first-match semantics. Resource lookups therefore use the existing `GameSceneAtlasResolver.resolveResource()` search order for logical Cocos names.

The first tutorial exposed two callers that bypassed this search order. `lessonsUI/*` and `Dialogue_UI/*` live below `gamescene_ui/` in the OBB. The lesson and dialogue loaders now resolve those logical names through the shared search path. The imported localization CSV contains quoted multiline cells; the previous line-based reader stopped before the tutorial keys. The CSV reader now parses complete quoted records. Host checks found `FGSub1`, `FGSub2`, `FGSub3`, and `NPCName3` in the imported file after this change. On the device, the lesson illustration and localized dialogue rendered and the session advanced to interactive input.

A follow-up audit of Java loaders found the same mismatch in the Choose Hero and server-selection buttons (`Common/btn_standard_{a,b,c}.png`) and in both login thunder sound sets (`SingleLogin_UI/*.mp3`). Their physical files are under `gamescene_ui/`; all four loaders now use the shared Cocos path resolver. Other direct Java paths checked in this audit already match physical OBB paths.

This inventory validates the container and the PNG/plist headers. It does not claim semantic decoding of every game-specific `.actData`, `.glData`, audio, or rendering format. The selected first player action still requests a frame absent from its proven preload family; that is the P8 resource boundary.
