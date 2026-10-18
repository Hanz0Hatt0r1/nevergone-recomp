# Manual reconstruction: player-to-enemy melee contact candidates

> Address convention: RevEngInstr/Ghidra executable addresses include the
> project's recovered image base. No original binary or proprietary game asset
> is committed.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native hit path

`BattleManager::playerHitEnemy()` at `0x002b7be0` first reads
`EnemyObject::enemyOrgHLPos()` and `MEPlayer::playerHLPoint()`. The vertical
distance must be strictly less than `50.0f` (the original constant is at ELF
address `0x002a8324`) before it checks `MEPlayer::isAttackState()` and iterates
the current Section-E hitpoints through `MEPlayer::beatHitPoint()`.

For each transformed player hitpoint, the routine calls `EnemyObject::isBeaten`
and then checks `MEPlayer::isHitFlag()` before entering the damage and reaction
branches. `EnemyObject::isBeaten()` rejects a missing action system, the
action-system `+0x2b5` not-hit flag, and its recovered special-state guard;
otherwise it uses the inclusive `CCRect::containsPoint()` test against
`EnemyObject::enemyBoundingBox()`.

`enemyBoundingBox()` uses the action sprite rectangle when the action system's
`+0x26c` readiness byte is set. In state `0x1c`, it translates by
`EnemyObject +0x280/+0x284` only when action-system `+0x2b6` is nonzero; otherwise
it uses `EnemyObject +0x118/+0x11c`. That readiness byte is set by the successful
`EnemyActionsSystem::loadingTex()` path. The selected tutorial enemy's state,
origin, flags, action frame, and source sprite geometry are all associated with
the current attack-wave generation in the live queue.

## Reconstruction connection

After a successful frame draw, the renderer takes the player HL point published
for that same spawn generation, action state, and game tick. It adds the player
node position (`MEPlayer +0x268`) to current action hitpoint centers, matches
them to the same current action frame, and applies the strict 50-unit vertical
gate against enemy 3's state-`0x1c` origin.

For the tutorial enemy body, the renderer transforms the resolved source frame
with its actual texture dimensions and action-frame transform, then removes the
render-only camera translation. The selected state-`0x1c` action has `go=false`.
For this state `isEnemyMoveState()` therefore reads the action-system `+0x2b6`
flag and selects EnemyObject `+0x118/+0x11c`, which remain zero from
`initWithID()`; it does not add `+0x280/+0x284`. The action-frame transform
already carries the recovered action-system origin, so the renderer preserves
that in the rectangle and supplies the zero non-move offsets to
`enemy_collision_contract`. The enemy origin used by the separate vertical
gate is still the exact spawn point selected by `enemyOrgHLPos()` for state
`0x1c`. The contract applies the native not-hit gate, special-state gate, and
inclusive point-in-rectangle comparison. Only visible, texture-ready enemy
bodies and generation-matched live waves can contribute a candidate.

The renderer exposes geometric contacts as `tutorial player-enemy contact
candidates`. It separately exposes `tutorial player-enemy hit-window
candidates` after applying `MEPlayer::isHitFlag()`. For states reset by
`setPlayerStateWithIdx()`, the native saved hit counter is exactly `-1`, and the
current action-system counter comes from the live playback cursor's `+0x1fc`
boundary index. Other states fail closed because their saved counter is not
known. The counter is not advanced here: no attack or enemy damage has been
applied, so native `addHitFlagCount()` has not occurred in this reconstruction.
`MEPlayer::inputAtkValue()`, combo accounting, and enemy reaction effects
remain incomplete.

The function-role label for `BattleManager::playerHitEnemy()` is recorded in
`player-enemy-contact-candidates-labels.jsonl`; its `combat` subsystem label
was already present. Generated RevEngInstr corpora and datasets remain outside
Git.
