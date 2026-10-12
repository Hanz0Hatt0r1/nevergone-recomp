# Android device player-route smoke (2026-10-11)

Tested on a Redmi Note 8 Pro (Android 13, arm64-v8a) with a locally built debug APK and user-imported original APK/OBB. No original binary or asset is included here.

The first local `assembleDebug` failed to link because `game_scene_player_bootstrap.cpp` was absent from the Android CMake source list. Adding it produced an installable APK. The original APK and OBB were imported through the app; runtime diagnostics reported both present. `GameLevels` parsed and verified one port node.

The fresh-role path reached `CharacterName`. Its native name panel was initially invisible because the loader looked for `ServerList/*` and `Common/*` under the asset root while the imported OBB places those files under `gamescene_ui/`. After correcting those paths, the original name panel and its Cancel/Confirm controls rendered on the device.

The Confirm touch was accepted: diagnostics reported a valid CharacterName submit and successful Lua dispatch, but no `cpp_OnCreateTheRole` callback followed. A later retained capture on the same device sharpened that boundary:

- original APK assets: present;
- original OBB assets: present;
- `offline startup route: opening-dialogue`;
- `SingleSelectHero BGM: playing (loop)`;
- SingleSelectHero state active with input enabled;
- `SingleSelectHero staging`: background yes, HeroTable `4/4`, career runes `10/10`, Confirm `4/4`, no staging failures;
- CharacterName actions reached valid submits while `completion=0`;
- persistent login Lua remained ready and reported zero dispatch failures;
- client callback events remained `0`;
- `GameLevels enter boundary` remained `idle`.

This proves that the device blocker is no longer atlas staging, SingleSelectHero input, CharacterName validation, or the `CreateCharacter` call boundary. The missing online service callback is the remaining discontinuity for this no-save data set.

## Serverless preservation handoff

The project now treats that discontinuity explicitly instead of fabricating an original callback or synthetic `CharacterID`.

When all of the following are true:

1. the recovered CharacterName tag-1 path validates the name;
2. `g_UILogin.CreateCharacter(name, career)` dispatches successfully;
3. the active project-owned offline route is the fresh-role `opening-dialogue` path;
4. imported local `GameLevels` data can be loaded all the way to `runtime-render-queue-ready`;

then the preservation runtime closes CharacterName and moves `offline_startup_flow` to a distinct `game-levels` route. No `cpp_OnCreateTheRole` event is claimed, no role ID is invented, and the online callback/auto-enter implementation remains unchanged for genuine callback-driven paths.

If the local GameLevels load stops before `runtime-render-queue-ready`, the fresh-role UI remains active and diagnostics retain the exact GameLevels boundary rather than hiding the failure.

The next device run should therefore show, after one valid Confirm:

```text
offline startup route: game-levels
offline fresh-role local enters: 1
character name layer: inactive
completion=1
GameLevels enter boundary
state: runtime-render-queue-ready
```

At that point the same run can proceed directly into the retained P7 scene/render evidence harness.

## Player-resource boundary update (2026-10-12)

The earlier device run correctly observed that no standalone `p01_i01.actData` exists in the tested imported tree. A subsequent user-supplied writable-data capture resolved that apparent gap.

The original app stores:

```text
player/actions_data/adata/p01/normal/player_01.actionsData
```

and the original runtime was already observed passing `player_01.actionsData` to `ActionDataManager::LoadPlayerActionDataWithFile()` after hero selection.

A bounded scan of that file proves it is an aggregate container of 125 named action-data payloads. The `p01_actions.plist` `action28` value `p01_i01.actData` is present exactly once inside that container with an 11,682-byte payload. All 53 action names referenced by the supplied `p01_actions.plist` have matching container entries.

The preservation runtime therefore no longer treats the missing standalone file as a resource blocker. It resolves the requested plist action to the exact embedded `actionsData` entry and passes only that bounded payload into the existing action-data parser. See `docs/evidence/player-actionsdata-container-2026-10-12.md`.
