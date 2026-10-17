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

The checked-in contract keeps the `+0x3e8` value explicit. The live renderer
does not currently observe that native field, so no value is guessed and this
recovery does not yet change simulated movement.

The function-role gold label is recorded in
`player-collision-state-labels.jsonl`. Adding it to local manual-restoration
dataset v49 and rebuilding as v50 produces 9,204 labelled functions, 6,911
structural groups, and 9,826 gold rows (7,394 train / 818 test / 992
validation). The RevEngInstr corpus and generated dataset remain outside Git.
