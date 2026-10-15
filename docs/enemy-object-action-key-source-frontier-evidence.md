# EnemyObject action dictionary key source-frontier evidence

## Trigger

The reviewed key-lifetime report proves two `objectForKey` lifetimes share one reused stack slot while receiving different source values through the predecessor helper `FUN_0073d100`.

For key 0, predecessor `arg[1]` is produced by:

```text
CCString::createWithFormat(...)
  -> CCString::getCString(...)
  -> FUN_0073d100(slot, value, aux-slot)
  -> CCDictionary::objectForKey(..., slot)
```

For key 1, predecessor `arg[1]` is instead a PHI/arithmetic expression. Therefore the two key values must be traced independently even though their temporary storage address is reused.

## Bounded source-frontier probe

`tools/enemy_object_action_key_source_frontier.py` consumes only the already exported `EnemyObject::initActionsSystem` High-P-code and the key-lifetime plan.

For each lifetime it:

1. selects predecessor helper `arg[1]`;
2. enumerates bounded dependency CALLs;
3. prints full argument provenance for every `CCString::createWithFormat` dependency;
4. walks the source SSA through arithmetic, transparent copies/casts, CALLs and PHIs;
5. treats LOAD as a memory boundary and reports only its address provenance;
6. records constants and unresolved root tokens without assigning semantics.

The traversal is bounded to 18 levels and 128 unique tokens per source and fails the report completeness flag if the budget is exhausted.

## Run

The focused P-code must already exist from the earlier call-site probe:

```text
build/enemy-object-action-init-evidence/
  enemy-object-init-actions-system-pcode.tsv
```

Run:

```bash
python3 tools/run_enemy_object_action_key_source_frontier.py
```

Output:

```text
build/enemy-object-action-init-evidence/
  enemy-object-action-key-source-frontier-report.txt
```

## Evidence boundary

A `createWithFormat` argument, PHI input, constant, root register or memory-address expression is structural provenance only. It is not called an action-data filename, dictionary key, enemy ID or tutorial selector until independent native/resource evidence establishes that meaning.

The tutorial enemy runtime bridge remains fail-closed until the dictionary route can be tied to the concrete reviewed enemy ID `3`.
