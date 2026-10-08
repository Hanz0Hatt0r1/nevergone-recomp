# Resource analysis baseline

The original APK contains a comparatively small but information-rich resource set. No proprietary assets are committed to this repository; these notes describe the layout only.

## Inventory

The APK contains 270 files under `assets/`.

| Extension | Count |
| --- | ---: |
| `.lua` | 107 |
| `.png` | 97 |
| `.hpc` | 19 |
| `.mp3` | 18 |
| `.csv` | 14 |
| `.plist` | 8 |
| `.actdata` | 3 |
| `.wav` | 2 |
| `.mp4` | 1 |
| `.txt` | 1 |

The content layout indicates a hybrid C++/Lua game rather than a purely native C++ game.

## Lua tree

A large script tree is present under:

```text
assets/assets/Script/
```

Important paths include:

```text
Game/StartLua.lua
Game/ClientRequire.lua
Game/Engine/CocosInterface.lua
Game/Engine/FileBase.lua
Game/Engine/FileManage.lua
Game/Engine/LocalFileManage.lua
Game/Engine/RpcBase.lua
Game/Logic/...
Game/Public/...
Game/UI/UILogin.lua
ShareLogic/Base/...
ShareLogic/Logic/...
ShareLogic/Public/...
ShareLogic/require.lua
```

The filenames reveal substantial high-level game logic for achievements, chapters, equipment, friends, guilds, items, mail, mercenaries, shops, talents, tasks, users, wraiths, RPC and login flow.

### Encoding state

Despite the `.lua` extension, sampled files are not plain Lua source and do not begin with the normal Lua bytecode magic (`1B 4C 75 61`). They contain high-entropy/non-printable data mixed with printable bytes and are classified as encoded/obfuscated by the current tooling.

This changes the reconstruction priority: recovering the Lua decode/load path can expose a large amount of game behavior without reconstructing every high-level system from ARM machine code.

## Configuration formats

The APK contains 19 `.hpc` configuration files, including data associated with:

- achievements;
- chapters;
- clothes;
- endless mode;
- equipment;
- guilds;
- items;
- materials;
- mercenaries;
- shop;
- talents;
- tasks;
- users;
- wraiths.

There are also plaintext CSV files such as login data, random names and several gameplay/configuration tables.

The `.hpc` format is an early M2 target. It may be serialization, compression, encryption, or a combination.

## Media/resources

The asset set contains sprite sheets (`.png` + `.plist`), UI images, music and sound effects, plus at least one MP4 enemy video. This matches the old Cocos2d-x pipeline and suggests that early rendering can be validated using a limited subset of original resources supplied by the user locally.

## Recompilation strategy for resources

The repository must not contain the original copyrighted game data. Instead:

1. the user supplies an original APK locally;
2. project tools validate and inventory it;
3. a local extraction step creates an ignored working resource directory;
4. runtime/build tooling consumes those extracted files;
5. reverse-engineered format documentation and original replacement code are committed, but proprietary data is not.

## Priority

The highest-value resource task is now the Lua loader:

1. find calls to `luaL_loadbufferx` / `luaL_loadfilex`;
2. identify the wrapper that reads a `.lua` file;
3. trace any transform performed before the Lua API call;
4. reconstruct that transform in a standalone tool;
5. verify recovered output against Lua syntax/bytecode signatures;
6. only then investigate `.hpc` files if their loader is independent.
