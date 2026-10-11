# Android device player-route smoke (2026-10-11)

Tested on a Redmi Note 8 Pro (Android 13, arm64-v8a) with a locally built debug APK and user-imported original APK/OBB. No original binary or asset is included here.

The first local `assembleDebug` failed to link because `game_scene_player_bootstrap.cpp` was absent from the Android CMake source list. Adding it produced an installable APK. The original APK and OBB were imported through the app; runtime diagnostics reported both present. `GameLevels` parsed and verified one port node.

The fresh-role path reached `CharacterName`. Its native name panel was initially invisible because the loader looked for `ServerList/*` and `Common/*` under the asset root while the imported OBB places those files under `gamescene_ui/`. After correcting those paths, the original name panel and its Cancel/Confirm controls rendered on the device.

The Confirm touch was accepted: diagnostics reported `character name layer: active`, `validation=valid`, `submit=1`, and `login Lua dispatches: 1 failures=0`. No `cpp_OnCreateTheRole` callback followed (`login callback events: 0`), so this run did not enter `GameScene`; its enter boundary remained idle and static renderer drawn frames remained zero. The next runtime task is to complete the offline create-character response and auto-enter path, then repeat the scene and player rendering check.

The imported APK/OBB also do not contain the `p01_i01.actData` referenced by the imported `p01_actions.plist` action28 entry. A first visible player sprite cannot be verified from these particular resources. Keep the existing fail-closed player-frame behavior until a matching user-provided action resource is available.
