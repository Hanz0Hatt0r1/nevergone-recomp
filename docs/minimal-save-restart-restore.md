# Minimal save → restart → restore slice

## Scope

P10 now has a deliberately small project-owned persistence boundary for the first offline playable scene. It does **not** write or claim compatibility with the original `DMG_*.sData` format.

The existing `GameSaveData::Encode/Decode` reconstruction remains useful evidence for the original title, but the technical-alpha persistence slice uses an independent sidecar so unproven original save sections cannot silently become part of the runtime contract.

## Sidecar format

Path under the app-private files directory:

```text
nevergone-recomp/minimal-save-v1.bin
```

The bounded binary record contains:

- magic `NGSV`;
- project format version `1`;
- exact payload byte count;
- project-owned FNV-1a payload checksum;
- scene index;
- player index (`1..3`, matching the currently recovered movement domain);
- finite bounded player motion X/Y offsets;
- bounded scene GUID (`1..256` bytes).

The decoder rejects wrong magic/version, truncation, trailing bytes, checksum mismatches, invalid player indices, empty/oversized GUIDs, non-finite offsets and offsets outside the explicit project bound.

Writes use a sibling temporary file followed by rename into the final app-private path. A failed write removes the temporary file and does not replace the last accepted sidecar.

## Runtime capture

`PlayerActionRuntimeLoader` already runs on the GLSurfaceView render thread and synchronizes the recovered player spawn/motion state. The persistence hook is intentionally placed after that normal spawn path.

Autosave receives coherent caller-owned `spawn` and `motion` snapshots. It ignores:

- missing/unready players;
- motion bound to another spawn generation;
- players outside the recovered `1..3` movement domain;
- a player that has never moved;
- unchanged motion generations.

The first changed motion is written immediately. Later changed generations are written no more often than once per 35 logical game ticks, approximately once per second at the recovered fixed game clock. This provides a restart point without relying on Android delivering `onPause()` before process death and without performing one file write per movement tick.

## Restart restore

At process start the persistence runtime has no in-memory state. Once normal game flow has independently reconstructed a ready player spawn, `restore_for_spawn()` attempts to load the sidecar once.

A record can change motion only when all of these match the already-created runtime:

- scene index;
- scene GUID;
- player index.

A valid record for a different scene/player remains pending; it cannot force a route change, select a different hero, create a scene or create a player. If normal flow later reaches that same identity, the pending offset may then be applied.

Restore calls `game_scene_player_motion_state::restore_offset()` only after `sync_spawn()` has already bound the motion cursor to the exact spawn generation/player. The restored offset replaces the accumulated local movement, resets movement counters, and updates the motion tick baseline so elapsed time from the previous process cannot be replayed as movement.

The record is applied at most once per process. A corrupt/truncated sidecar fails before any player-motion mutation.

## Deliberate boundaries

This first P10 slice persists only the minimum state that produces a visible restart difference in the first playable scene: scene identity, player identity and movement offset.

It does not yet persist:

- arbitrary GameLevels navigation;
- combat/enemy state;
- tutorial dialogue progress;
- current transient action animation frame;
- inventory/equipment/currency;
- compatibility with original Never Gone save containers.

Those require separate evidence and are not inferred from the existence of the original `GameSaveData` symbols.
