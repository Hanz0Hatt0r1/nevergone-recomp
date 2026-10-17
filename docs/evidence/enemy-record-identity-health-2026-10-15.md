# Enemy record identity/base-health mapping — 2026-10-15

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note closes the two `LoadGL_Global` enemy-record fields needed to
materialize the first tutorial Attack wave without guessed HP.

## Serialized record order

The recovered `GameLevels::LoadGL_Global(HPData*, int&)` parser consumes each
32-byte enemy record as:

```text
int32 first_i32
int32 second_i32
float first_float
int32 third_i32
int32 fourth_i32
int32 fifth_i32
int32 sixth_i32
int32 seventh_i32
```

At `0x002c1fc0`, the original loader stores:

```text
first_i32  -> GameLevelsEnemyData +0x14
second_i32 -> GameLevelsEnemyData +0x18
second_i32 -> GameLevelsEnemyData +0x1c  (duplicate)
first_float -> +0x20
third_i32   -> +0x24
fourth_i32  -> +0x28
fifth_i32   -> +0x2c
sixth_i32   -> +0x30
seventh_i32 -> +0x34
```

The duplicate second store is original behavior rather than a clean-room
normalization.

## Enemy identity

`GameLevels::GetEnemyDataWithEnemyID(unsigned int)` loads
`GameLevelsEnemyData +0x14` at `0x002c261c` and compares it directly with the
requested enemy ID.

Therefore:

```text
enemy_id <- EnemyRecord.first_i32
```

This is the same semantic ID used by the recovered tutorial spawn plan (`3` for
the first Attack wave and `0x15` for the later air-attack enemy).

## Base health

`EnemyObject::loadEnemyObjectFile(GameLevelsEnemyData*, ...)` loads signed value
`GameLevelsEnemyData +0x18` at `0x002ae76e`, converts it to float, and writes the
result into the two EnemyObject health stores recovered by
`enemy_health_state`.

Therefore:

```text
base_health <- EnemyRecord.second_i32
```

An independent original consumer corroborates the same source value:
`GameLevels::GetLevelALLEnemyBlood()` sums `GameLevelsEnemyData +0x1c`, and
`LoadGL_Global()` writes the same `second_i32` to both `+0x18` and `+0x1c`.

## Combat input value

The same serialized record's `first_float` is stored by `LoadGL_Global()` at
`GameLevelsEnemyData +0x20`. `EnemyObject::loadEnemyObjectFile()` converts that
value with `VCVT.U32.F32` and stores it at `EnemyObject +0x2bc`. The recovered
`EnemyObject::inputAtkValue()` then multiplies it by the current-frame
`EnemyActionsSystem::DamageFactor()`.

The clean-room resolver keeps this field separate from the integer mapping:
after a reviewed ID lookup selects exactly one record,
`resolve_input_value_2bc()` converts `EnemyRecord.first_float` and rejects
missing or duplicate IDs and values outside the defined unsigned conversion
range. `resolve_at_frame()` then selects the matching Section F factor from the
current parsed action document. Attack-event scheduling and applying contact
damage to the player remain unconnected.

## Production mapping

`game_levels_enemy_record_mapping::reviewed_original_mapping()` now returns:

```text
identity_field    = first_i32
base_health_field = second_i32
review_status     = reviewed
```

The existing resolver remains fail-closed: an enemy ID must select exactly one
record and the resolved base health must be positive. Candidate mappings are
still not usable.

The live tutorial Attack path installs this exact reviewed mapping independently
of the dynamic player-facing and scene-limit providers. No HP constant is
compiled into the roster.

## Reproducible exact-binary verifier

Run against either the original APK or extracted ARMv7 library:

```bash
python3 tools/enemy_record_binary_evidence.py \
  /path/to/com.hippiegame.nevergone.apk \
  --output build/enemy-record-evidence/enemy-record-armv7.txt
```

The verifier requires the exact known library hash and all four bounded Thumb
signatures:

- `LoadGL_Global` identity/health stores;
- `GetEnemyDataWithEnemyID` identity comparison;
- `EnemyObject::loadEnemyObjectFile` base-health load;
- `GetLevelALLEnemyBlood` duplicate-health corroboration.

A changed binary or mismatched signature fails closed.

## Remaining tutorial combat boundary

With player facing, scene limits, enemy identity, and imported base HP all
resolved, the first Attack-wave roster can now materialize its project-owned
slots from real `cp_lesson.glData` records.

The next reconstruction task is not another numeric guess. It is the live enemy
actor bridge: create/bind reconstructed enemy entities for the materialized
slots, retain the roster wave generation on each binding, route actual combat
damage/death/state changes back into the roster, and only then allow
`TutorialEnemiesDefeated` to advance Attack to Block.
