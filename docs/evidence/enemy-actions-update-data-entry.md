# EnemyActions `updateData()` entry evidence

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native entry

`EnemyActionsSystem::updateData()` is the 12-byte function at `0x2ad498`:

- `0x2ad498`: load byte `system+0x26c`;
- `0x2ad49c`: if the byte is zero, branch to the return at `0x2ad4a2`;
- `0x2ad49e`: otherwise tail-branch to `0x2ac368`, which is `EnemyActionsSystem::updateActionFrameMoveValue()+0xda`.

The reachable tail begins with a bounded precondition chain:

- obtains `BattleManager::sharedBattleManager()`;
- requires `system+0x108` (`EnemyActionsData`) to be non-null;
- requires `EnemyActionsData+0x88` (the primary `ActionFrameData` array) to be non-null;
- requires that array count to be nonzero;
- reads current frame `system+0x294` and selects `objectAtIndex(current_frame)` from `+0x88`.

The original tail does not visibly range-check `+0x294` against count before `objectAtIndex`. The project-owned helper rejects negative/out-of-range indices safely instead of reproducing invalid native access.

## `+0x26c` lifecycle evidence

The same binary establishes that `+0x26c` is a readiness gate for this visual/frame-update path:

- `EnemyActionsSystem::init()` at `0x2ade5a` stores byte `1` to `+0x26c` at `0x2ade6c`;
- `EnemyActionsSystem::initWithFile(...)` at `0x2adcb0` stores byte `0` to `+0x26c` at `0x2addc8` before resource loading;
- `EnemyActionsSystem::loadingTex(...)` stores byte `1` to `+0x26c` at `0x2abc08` after its texture-loading sequence;
- multiple other visual helpers such as `resetVisible()`, `filpAction(...)`, and `beatenColorFlag(...)` also test the same byte before touching sprites.

This evidence supports retaining the field as offset-named `flag_26c`; broader gameplay semantics are intentionally not assigned.

## Reconstruction

`enemy_actions_runtime_state::State` now retains `flag_26c` and `apply_update_data_entry(document, state)` reconstructs only the proven entry contract:

1. zero `flag_26c` stops immediately;
2. a parsed document with no primary frames stops before frame selection;
3. a valid current frame selects the matching `document.primary_records` index and reports `would_enter_frame_update=true`;
4. malformed project-owned negative/out-of-range current frames stop safely.

The helper does not mutate runtime state and does not claim to reconstruct the large body reached at `0x2ac368`.

## Still unresolved

The tail beginning at `0x2ac368` is substantially larger than the entry gate. It includes sprite-frame/resource selection, object-specific branches and later movement/render-state updates. Those operations require separate bounded reconstruction and are not inferred by this entry helper.

No proprietary payload or decompiler-derived source is included.
