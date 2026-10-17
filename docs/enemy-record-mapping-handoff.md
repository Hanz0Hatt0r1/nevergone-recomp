# Enemy record mapping handoff

This boundary connects the recovered 32-byte `LoadGL_Global` enemy records to
the tutorial roster without guessed IDs or HP.

## Reviewed original mapping

Exact ARMv7 consumer evidence now closes both fields for Never Gone 1.0.9:

```text
EnemyRecord.first_i32  -> GameLevelsEnemyData +0x14 -> enemy_id
EnemyRecord.second_i32 -> GameLevelsEnemyData +0x18 -> base_health
EnemyRecord.second_i32 -> GameLevelsEnemyData +0x1c -> total-blood mirror
```

`GameLevels::GetEnemyDataWithEnemyID(unsigned int)` compares `+0x14` directly
with its requested ID. `EnemyObject::loadEnemyObjectFile(...)` loads `+0x18`,
converts it to float, and seeds both recovered health stores.
`GameLevels::GetLevelALLEnemyBlood()` independently sums the `+0x1c` duplicate.

`game_levels_enemy_record_mapping::reviewed_original_mapping()` therefore
returns:

```text
identity_field    = first_i32
base_health_field = second_i32
review_status     = reviewed
```

The combat-value investigation also proves that
`EnemyObject::loadEnemyObjectFile()` reads `GameLevelsEnemyData +0x20`, converts
it with `VCVT.U32.F32`, and stores it at `EnemyObject +0x2bc`. This value is
consumed by `EnemyObject::inputAtkValue()` with the current
`EnemyActionsSystem::DamageFactor()`. Its correspondence to one of the raw
32-byte `LoadGL_Global` record fields is not yet proven, so it remains outside
the runtime enemy-record mapping.

The live tutorial Attack gate installs this reviewed mapping independently from
its player-facing and scene-limit providers.

See `docs/evidence/enemy-record-identity-health-2026-10-15.md` and reproduce the
exact binary check with:

```bash
python3 tools/enemy_record_binary_evidence.py \
  /path/to/com.hippiegame.nevergone.apk
```

The verifier requires the exact known `libcocos2dcpp.so` SHA-256 and all bounded
Thumb signatures to match.

## Runtime guard

`game_levels_enemy_record_mapping` still exposes only the seven integer slots
from the recovered record; the intervening float cannot accidentally become an
identity or health field.

A mapping is usable only when:

- it has `ReviewStatus::kReviewed`;
- identity and base-health fields are different;
- the requested enemy ID selects exactly one imported record;
- the resolved base health is positive.

Duplicate IDs, missing IDs, candidate-only mappings, zero/negative HP, and
aliased fields all fail closed. `roster_base_health_resolver()` remains the
adapter consumed by `tutorial_enemy_roster`.

## Legacy cross-evidence candidate

`tools/enemy_record_cross_evidence.py` and the diagnostic/Ghidra candidate
workflow remain useful as independent corroboration and regression evidence.
Their JSON deliberately retains `semantic_status = cross-evidence-candidate`;
it is not used to promote production fields at runtime now that exact original
consumer evidence exists.

## Remaining production boundary

The numerical data needed to materialize the first Attack roster is now
resolved from imported scene data. The next boundary is the live enemy actor
bridge:

1. create/bind reconstructed enemy entities for materialized roster slots;
2. retain the active `wave_generation` on every binding;
3. route actual combat damage, death, enemy-state, and no-death changes into the
   roster using that token;
4. publish `kTutorialEnemiesDefeated` only when all three recovered Attack-wave
   enemies pass the original death predicate.

No default HP constant or button-press completion shortcut is introduced.
