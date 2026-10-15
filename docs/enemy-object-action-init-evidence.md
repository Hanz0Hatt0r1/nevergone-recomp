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
- native/ELF disassembly places `EnemyActionsSystem::initWithFile(char const*, ...)`
  at ELF code offset `0x002adcb0` and shows that it receives its action-data
  filename from its caller;
- native/ELF symbol evidence places
  `ActionDataManager::addActionDataWithID(int, int, char const*)` at ELF code
  offset `0x0028ee34`;
- object-type selector 0 routes action-data registration through that
  `ActionDataManager` call;
- selector-specific resource construction later uses the reconstructed
  EnemyActions document/frame pipeline.

Those ELF offsets are **not automatically Ghidra image addresses**. The analyzed
project used for current probes maps this library with a `+0x10000` image delta
for these functions. A live reverse run demonstrated the consequence directly:
passing ELF offset `002adcb0` to Ghidra resolved inside `DataManager::init` at
`002adbf2`, not to `EnemyActionsSystem::initWithFile`.

For that reason the probe no longer uses numeric ELF offsets as Ghidra defaults.
It resolves `EnemyActionsSystem::initWithFile` by symbol name and validates the
resolved root identity before exporting callers. Numeric overrides are accepted
only as explicit **Ghidra image-space** addresses.

What is not yet proven is how the runtime path supplies those action
initialization arguments for a concrete enemy such as tutorial enemy ID `3`.

## Probe

`tools/run_enemy_object_action_init_probe.py` first exports metadata-only High
P-code starting at `EnemyObject::loadEnemyObjectFile` with
`ExportBoundedCallGraphPcodeFlow.java`, then runs
`tools/enemy_object_action_init_evidence.py`.

Two live forward runs on 2026-10-15 established:

1. `loadEnemyObjectFile` itself has no direct `EnemyActionsSystem::initWithFile`
   or `ActionDataManager::addActionDataWithID` call;
2. a forward direct-call traversal of depth 4 covered 45 functions / 1553
   P-code rows with `capped=false` and still contained no resolved init call.

That means increasing the forward depth is not the justified next step. When the
forward analyzer returns its fail-closed status code `2`, the wrapper launches
`ExportBoundedReverseCallGraphPcodeFlow.java` from the symbolic target
`EnemyActionsSystem::initWithFile`.

The reverse exporter validates that the resolved root function name contains
`EnemyActionsSystem::initWithFile`. If an explicit numeric override lands inside
another function because an ELF offset was confused with a Ghidra image address,
the exporter stops with an identity-mismatch error instead of exporting the wrong
call graph.

The reverse exporter includes the `initWithFile` function itself plus bounded
direct callers discovered from Ghidra call references. This exposes both:

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

The analyzer recognizes calls found anywhere in either bounded export by their
resolved symbol names. Optional numeric fallbacks can also be supplied when a
local Ghidra database lacks a useful target name:

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

No numeric target entries are required for the normal path. The wrapper resolves
`EnemyActionsSystem::initWithFile` by symbol name.

If a different Ghidra database lacks that symbol, an explicit image-space
fallback may be provided:

```bash
python3 tools/run_enemy_object_action_init_probe.py ... \
  --init-entry <Ghidra-image-address> \
  --add-entry <Ghidra-image-address>
```

Do not copy ELF offsets such as `0x002adcb0` or `0x0028ee34` into those options
without first converting/confirming them in the actual Ghidra image. The reverse
exporter still validates the `initWithFile` root name when an address override is
used.

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
