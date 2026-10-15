# Enemy record mapping handoff

This stage bridges the recovered `LoadGL_Global` enemy records to the tutorial
roster without silently turning a heuristic into a semantic field name.

## Candidate artifact

After capturing a complete real-device GameLevels diagnostic report and running
the independent Ghidra health-consumer probe, create the bounded cross-evidence
report and a machine-readable candidate together:

```bash
python3 tools/enemy_record_cross_evidence.py \
  artifacts/android-game-scene-runtime/runtime-diagnostics.txt \
  build/enemy-health-field-consumer/enemy-health-field-consumer.tsv \
  --known-enemy-id 3 \
  --output artifacts/android-game-scene-runtime/enemy-record-cross-evidence.txt \
  --mapping-output artifacts/android-game-scene-runtime/enemy-record-mapping-candidate.json
```

Add another independently recovered enemy ID only when it is known to belong to
the same captured record set. The tool fails closed when the runtime record dump
is truncated, the identity column is ambiguous, the Ghidra consumer evidence
has no unique health source, or both evidence paths resolve to the same raw
field.

The JSON schema is:

```text
NEVERGONE.EnemyRecordMappingCandidate/1
```

and explicitly carries:

```text
semantic_status = cross-evidence-candidate
```

It is a review artifact, not a production switch.

## Runtime gate

`game_levels_enemy_record_mapping` is the project-owned semantic boundary used
by the future live tutorial integration. It exposes only the seven integer
slots from the recovered 32-byte record. The float slot cannot accidentally be
selected as identity or base health.

A mapping is usable only when:

- it has `ReviewStatus::kReviewed`;
- identity and base-health fields are different;
- the requested enemy ID selects exactly one imported record;
- the resolved base health is positive.

Duplicate identity rows, missing IDs, candidate-only mappings, zero/negative HP,
and invalid field combinations all fail closed.

`roster_base_health_resolver()` has the exact callback signature expected by
`tutorial_enemy_roster::BaseHealthResolver`, so no additional semantic adapter
is required after the mapping is reviewed.

## Remaining production boundary

The clean-room runtime still does not ship a hard-coded mapping. The next step
requires real evidence from the user's original game data:

1. capture a complete `runtime-diagnostics.txt` from the first tutorial scene;
2. run the Ghidra health-consumer probe against the analyzed original
   `libcocos2dcpp.so`;
3. generate and review the cross-evidence candidate;
4. install the reviewed mapping into the tutorial enemy runtime;
5. connect live damage/death state to the roster and publish
   `kTutorialEnemiesDefeated` only after the recovered death predicate says the
   full Attack wave is dead.

No default enemy HP or guessed record field is introduced by this stage.
