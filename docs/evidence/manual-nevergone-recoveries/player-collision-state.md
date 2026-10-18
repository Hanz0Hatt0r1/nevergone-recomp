# Manual reconstruction: player collision-state gate

> Address convention: RevEngInstr executable addresses use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Selection

| Function | RevEngInstr entry | ELF VMA | P-code ops | Branches |
| --- | ---: | ---: | ---: | ---: |
| `MEPlayer::isCollisionState` | `0x00328ad0` | `0x00318ad0` | 93 | 18 |

The function is called by `GameScene::updatePrePlayerPos` immediately after
`BattleManager::playerIntersectsEnemy`. The latter always refreshes its
intersection fields; this predicate decides whether that result participates
in the player's position correction.

## Predicate

The byte at `MEPlayer +0x3e8` disables collision response when nonzero. With
that byte clear, only player indices `1` and `2` can enter the collision
response path. For those indices, the routine returns true for every action
state except these exact sets:

- Player index `2`: `0x11`, `0x2f..0x31`, `0x17..0x1a`, `0x20..0x21`,
  `0x0d..0x0f`, `0x35..0x36`, `0x38`, `0x2d`, `0x3c`.
- Player index `1`: `0x11`, `0x2f..0x31`, `0x2d`, `0x17..0x1a`, `0x20..0x21`,
  `0x0f`, `0x0c..0x0d`, `0x3c`.

All other player indices return false. The checked-in contract mirrors these
sets in `player_collision_state_contract::is_collision_state()`.

`MEPlayer::setCurActionNoCollision()` at ELF `0x00318ac8` sets the byte to one.
Its two call sites are both inside `BattleManager::playerHitEnemy()` and both
select player index `2`; one hit-reaction branch sets it only when the enemy is
not erupting. `MEPlayer::initWithMEName()` initializes the byte to zero, and
`MEPlayer::setActionState()` clears it on an accepted action transition. The
simulated combat path does not yet generate those incoming-hit transitions, so
the live renderer still has no nonzero value to supply and collision response
remains disconnected from movement.

The predicate and setter function-role gold labels are recorded in
`player-collision-state-labels.jsonl`. The initial Lesson Attack enemy-contact
path now uses this predicate. Its `collision_disabled` input is currently false
because the reconstructed tutorial does not yet simulate the incoming-hit path
that sets `+0x3e8`; wire that state when player damage is restored. The
RevEngInstr corpus and generated dataset remain outside Git.
