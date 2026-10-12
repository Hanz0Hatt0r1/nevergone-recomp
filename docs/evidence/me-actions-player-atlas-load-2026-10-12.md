# MeActions player atlas load evidence (2026-10-12)

Target: clean-room reconstruction of Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

No original binary, plist, texture, action payload, or other proprietary resource is committed to this repository.

## Physical-device boundary after actionsData support

The retained `run-339` Android capture was recorded after the player action loader learned to extract the requested `.actData` entry from the original `player_01.actionsData` aggregate.

That run proves the action document and player request are now live:

```text
GameScene player spawn state: ready
GameScene player motion: ready
player index: 1
GameScene asset staging requests: 133
```

The earlier 132-request queue therefore advanced by exactly one player-body request. Staging then stopped at request index 121:

```text
state: frame-resolution-failed
requests: 133
staged: 121
failed request index: 121
failed request kind: sprite-frame-by-name
```

This isolates the next boundary to player sprite-frame atlas resolution rather than Career selection, player spawn, movement, or action-document loading.

## User-owned action-frame observation

A user-supplied copy of the writable player data was inspected locally without adding it to the repository. For the initial Career-1 action selected through `p01_actions.plist/action28`, the embedded action document's first primary record contains:

```text
first string  -> p01_02_default.plist
third string  -> P01_I01_001.png
```

The existing recovered ActionFrameData mapping assigns those fields as:

```text
first string -> ActionFrameData +0x60
third string -> ActionFrameData +0x68
```

The names above are retained only as bounded evidence metadata; the corresponding original files are not stored here.

## Native load order

A tighter ARMv7 disassembly of the original player path establishes the missing ordering constraint.

`MEPlayer::createActions(unsigned int)` at `0x00318d8c` preloads the selected player's ordinary default sprite plists using the formatted `p%02d_%02d_%02d_default.plist` family. That preload set is not the complete rule for the current action frame.

Inside `MeActionsSystem::initWithFile(...)`, around `0x0032c026..0x0032c038`, native reads the selected `ActionFrameData +0x60`, obtains its C string, and calls:

```text
CCSpriteFrameCache::addSpriteFramesWithFile(AFD+0x60)
```

Later, `MeActionsSystem::updateData()` around `0x00329da8..0x00329dbc` reads the selected `ActionFrameData +0x68` and calls:

```text
CCSpriteFrameCache::spriteFrameByName(AFD+0x68)
```

Therefore the body-frame lookup is ordered as:

```text
ActionFrameData +0x60 plist
    -> addSpriteFramesWithFile()
    -> ActionFrameData +0x68 frame key
    -> spriteFrameByName()
```

## Reconstruction consequence

The previous Android staging path scanned every `pNN_*_default.plist` in the player resource directory, then asked the frame resolver to search all of them. That is broader than the original operation and can fail closed on unrelated malformed or duplicate membership even when the exact current action atlas is known.

The reconstructed player render command now carries the selected primary record's `first_string` as its player atlas plist. The direct-asset request bridge retains that value, and Android staging resolves exactly that plist through the already recovered Cocos search-path order before resolving the `third_string` frame key.

Scene sprite-frame requests continue using the GameLevels atlas-preload list. The change is isolated to the evidence-backed player-body request.
