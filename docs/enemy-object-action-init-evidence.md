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
- `EnemyActionsSystem::initWithFile(char const*, ...)` is at `0x002adcb0` and
  receives its action-data filename from its caller;
- `ActionDataManager::addActionDataWithID(int, int, char const*)` is at
  `0x0028ee34`;
- object-type selector 0 routes action-data registration through that
  `ActionDataManager` call;
- selector-specific resource construction later uses the reconstructed
  EnemyActions document/frame pipeline.

What was not yet proven is how the `EnemyObject` load path supplies those action
initialization arguments for a concrete enemy such as tutorial enemy ID `3`.

## Probe

`tools/run_enemy_object_action_init_probe.py` first exports metadata-only High
P-code starting at `EnemyObject::loadEnemyObjectFile` with
`ExportBoundedCallGraphPcodeFlow.java`, then runs
`tools/enemy_object_action_init_evidence.py`.

Two live runs on 2026-10-15 established:

1. `loadEnemyObjectFile` itself has no direct `EnemyActionsSystem::initWithFile`
   or `ActionDataManager::addActionDataWithID` call;
2. a forward direct-call traversal of depth 4 covered 45 functions / 1553
   P-code rows with `capped=false` and still contained no resolved init call.

That means increasing the forward depth is not the justified next step. When the
forward analyzer returns its fail-closed status code `2`, the wrapper now
launches `ExportBoundedReverseCallGraphPcodeFlow.java` from the reviewed
`EnemyActionsSystem::initWithFile` entry. The reverse exporter includes the
`initWithFile` function itself plus bounded direct callers discovered from
Ghidra call references. This exposes both:

- the actual functions that directly call `initWithFile` and their arguments;
- the internal `addActionDataWithID` call made by the target path when present.

Both traversals are explicitly bounded:

```text
max call depth: 4
max decompiled functions per traversal: 96
```

Only resolved direct call references are traversed. `CALLIND` targets are not
guessed. No action filename, object type, or enemy ID is assigned from an
unresolved value.

The analyzer recognizes calls found anywhere in either bounded export to:

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
arithmetic or unresolved values remain `derived`.

## Run on the analyzed original library

```bash
python3 tools/run_enemy_object_action_init_probe.py \
  --ghidra /opt/ghidra \
  --project-dir /path/to/ghidra/projects \
  --project-name Nevergone \
  --program libcocos2dcpp.so
```

The reviewed original ARMv7 entries are defaults:

```text
EnemyActionsSystem::initWithFile = 002adcb0
ActionDataManager::addActionDataWithID = 0028ee34
```

Override them only when intentionally probing a different analyzed image:

```bash
python3 tools/run_enemy_object_action_init_probe.py ... \
  --init-entry <entry> \
  --add-entry <entry>
```

The wrapper passes `-recursive` to Ghidra's `-process` mode so an imported
library can live below a nested Ghidra project folder.

Outputs can include:

```text
build/enemy-object-action-init-evidence/load-enemy-object-callgraph-pcode.tsv
build/enemy-object-action-init-evidence/enemy-actions-init-reverse-callgraph-pcode.tsv
build/enemy-object-action-init-evidence/enemy-object-action-init-report.txt
```

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
