# Manual reconstruction: enemy facing and action flip

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.
RevEngInstr `source_entry` uses image base `0x10000` above ELF VMA; subtract it
to inspect the local Thumb address.

## `EnemyObject::flipEnemy(bool)`

Entry `0x002c173c` (`0x2b173c` ELF VMA). The model in
`android/app/src/main/cpp/enemy_object_flip_enemy.{h,cpp}` preserves the
observable branch and state-update decisions without fabricating the native
`setEnemyState()`, `enemyHLPos()`, or Cocos sprite effects.

The method returns immediately for enemy states `0x1a` and `0x2e`. It also
returns when the requested direction already matches `+0x27c` exactly: value
`1` with `true`, or value `0` with `false`. Other nonzero `+0x27c` values are
not treated as `true` by this equality check.

The byte at `+0x22a` selects two distinct paths:

- If zero, an empty action array is a no-op. Otherwise every action system
  receives the requested direction through `filpAction(bool)`. This path does
  not write `+0x27c`, change enemy state, or refresh `enemyHLPos()`.
- If nonzero, action systems are visited when present. Types `5` and `6` receive
  the current direction (`+0x27c == 1`); other types receive the request. When
  the array is nonempty, the method requests state `6` if the entry state was
  `1` or `0x1e`, otherwise state `5`, with setter flags `(true, true, -1)`.
  Whether or not the array is empty, it then stores the requested direction as
  `0` or `1` at `+0x27c` and refreshes `enemyHLPos()`.

`EnemyActionsSystem::filpAction(bool)` ignores systems whose `+0x26c` byte is
zero; active systems store the argument at `+0x16a`. The native leaf also
forwards that stored value to its present child sprites through
`CCSprite::setFlipX()`. The current live enemy renderer still has no recovered
AI target-update producer, so this model is available for reconstruction and
training but is not wired to invent an enemy-facing update.

Gold subsystem and function-role labels are in
`enemy-object-flip-enemy-labels.jsonl`.
