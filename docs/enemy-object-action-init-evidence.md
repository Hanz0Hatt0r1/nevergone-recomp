# EnemyObject action-init evidence

The first tutorial Attack wave now has evidence-backed geometry, enemy ID/base
health and generation-safe actor identities. The next missing production
boundary is the original per-enemy action-data selection used to turn a binding
into a visible/animated reconstructed enemy actor.

This probe targets that boundary without guessing an action filename, object
type, or parameter meaning.

## Existing evidence

For the original Never Gone 1.0.9 ARMv7 library, the project already proves:

- `EnemyObject::loadEnemyObjectFile(GameLevelsEnemyData*, ...)` consumes the
  reviewed enemy record and seeds the recovered health state;
- `EnemyActionsSystem::initWithFile(char const*, ...)` receives its action-data
  filename from its caller;
- object-type selector 0 routes action-data registration through
  `ActionDataManager::addActionDataWithID(int, int, char const*)`;
- selector-specific resource construction later uses the reconstructed
  EnemyActions document/frame pipeline.

What was not yet proven is how the `EnemyObject` load path supplies those action
initialization arguments for a concrete enemy such as tutorial enemy ID `3`.

## Probe

`tools/run_enemy_object_action_init_probe.py` exports metadata-only High P-code
starting at `EnemyObject::loadEnemyObjectFile` with
`ExportBoundedCallGraphPcodeFlow.java`, then runs
`tools/enemy_object_action_init_evidence.py`.

The first live run on 2026-10-15 proved that `loadEnemyObjectFile` itself does
**not** directly call `EnemyActionsSystem::initWithFile` or
`ActionDataManager::addActionDataWithID`. The exporter therefore follows only
resolved internal direct calls from that root. Traversal is explicitly bounded:

```text
max direct-call depth: 4
max decompiled functions: 96
```

Enemy/Action-named callees are prioritized before generic and library helpers so
the bounded frontier stays focused on the original action initialization path.
Indirect calls are exported as structural P-code rows but are not guessed into
direct targets.

The analyzer recognizes direct calls found anywhere in the bounded export to:

```text
EnemyActionsSystem::initWithFile
ActionDataManager::addActionDataWithID
```

For each call it reports:

- call site and resolved target;
- receiver provenance;
- every explicit argument in original call order;
- structurally traced constants and root tokens.

Transparent copies/casts/extensions and `MULTIEQUAL` joins are traced. Other
arithmetic or unresolved values remain `derived`. No argument is automatically
renamed to `filename`, `enemy_id`, `object_type`, or any other gameplay meaning.

The analyzer still fails closed when the bounded graph contains no direct
`EnemyActionsSystem::initWithFile` call. Increase the explicit traversal bounds
only when a real report shows that the current frontier is insufficient; do not
replace unresolved call edges with guessed targets.

## Run on the analyzed original library

```bash
python3 tools/run_enemy_object_action_init_probe.py \
  --ghidra /opt/ghidra \
  --project-dir /path/to/ghidra/projects \
  --project-name Nevergone \
  --program libcocos2dcpp.so
```

The wrapper passes `-recursive` to Ghidra's `-process` mode. This is required
when the imported library lives in a nested Ghidra project folder rather than at
the project root. The requested filename remains the match boundary; the probe
does not fall back to processing unrelated project programs.

The traversal bounds can be overridden explicitly:

```bash
python3 tools/run_enemy_object_action_init_probe.py ... \
  --max-depth 5 \
  --max-functions 128
```

Outputs:

```text
build/enemy-object-action-init-evidence/load-enemy-object-callgraph-pcode.tsv
build/enemy-object-action-init-evidence/enemy-object-action-init-report.txt
```

If local Ghidra symbols are incomplete, use exact analyzed-image entries:

```bash
python3 tools/run_enemy_object_action_init_probe.py ... \
  --load-enemy-object <address> \
  --init-entry <EnemyActionsSystem::initWithFile-address> \
  --add-entry <ActionDataManager::addActionDataWithID-address>
```

The entry overrides are matching aids only; they do not assign semantics to any
arguments.

## Acceptance boundary for the next runtime step

A report that merely finds an `initWithFile` call is not sufficient to hard-code
an enemy action path. Before a tutorial binding can load an action document, the
relevant argument must be tied independently to an imported, bounded asset name
or an already reconstructed ActionDataManager mapping. If the source stays
`derived` or ambiguous, the actor bridge remains fail-closed.

Once that filename/ID correspondence is proven, the existing reconstructed
`enemy_actions_wbg_document` and `enemy_actions_runtime_state` layers can drive
frame/resource requests for each active `tutorial_enemy_actor_bindings::Binding`
without introducing synthetic enemy visuals.
