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

## Remaining player-resource boundary

The imported APK/OBB also do not contain the `p01_i01.actData` referenced by the imported `p01_actions.plist` action28 entry. A first visible player sprite cannot be verified from these particular resources. Keep the existing fail-closed player-frame behavior until a matching user-provided action resource is available or another evidence-backed valid player action in the supplied data is connected.
