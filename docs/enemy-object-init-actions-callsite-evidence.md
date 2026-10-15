# EnemyObject initActionsSystem call-site evidence

## Trigger

The 2026-10-15 `createWithFile` per-call matrix completed with 7 direct caller
functions, 35 call sites and 8 parameters per call. Exactly one structural
intersection combined a `param[0]` root produced by `getCString` with
`EnemyObject`-typed provenance in the same call:

```text
caller:    002c32a0 EnemyObject::initActionsSystem
call site: 002c3a80
signature: initActionsSystem(EnemyObject * this, CCString * param_1)

param[0]: CALL getCString
param[1]: LOAD, EnemyObject provenance
param[2]: LOAD, EnemyObject provenance
param[3]: 0
param[4]: derived uint
param[5]: 0
param[6]: 0
param[7]: LOAD, EnemyObject provenance
```

The other `getCString`-root calls belong to `LansquenetObject` and `PetObject` and
do not carry the same `EnemyObject` provenance. NPC, blacksmith and mercenary
paths are also structurally distinct. The next justified probe is therefore the
single `EnemyObject::initActionsSystem` call, not another broad reverse traversal.

## Focused probe

`tools/run_enemy_object_init_actions_callsite.py` decompiles only
`EnemyObject::initActionsSystem` with the existing metadata-only
`ExportFunctionPcodeFlow.java`. It then joins that High-P-code with the already
captured `createWithFile` caller subtree and qualified caller identity TSVs.

The analyzer:

1. derives the `createWithFile` target entry and exact call site from the existing
   subtree evidence rather than hard-coding a numeric factory address;
2. requires exactly one matching factory call in `EnemyObject::initActionsSystem`;
3. traces the `param[0]` definition and inputs of its `getCString` call;
4. symbolically reduces `LOAD` addresses for the EnemyObject-derived parameters
   through `COPY`/`CAST`/extensions, integer add/sub, `PTRSUB`, `PTRADD` and stable
   `MULTIEQUAL` joins;
5. reports the common object base root and exact constant offsets when they are
   structurally recoverable;
6. cross-checks the existing Ghidra signature and reports whether the direct
   `getCString` receiver is distinct from the common `EnemyObject` base.

No ARM register number is assigned to `this` or `param_1` by convention. The
report only calls the `getCString` receiver consistent with the sole `CCString`
formal after the signature shape, direct-input root and distinct EnemyObject base
all agree.

## Run

The earlier action-init and call-matrix probes must already have produced:

```text
build/enemy-object-action-init-evidence/
  enemy-actions-create-with-file-caller-parameter-subtrees.tsv
  enemy-actions-create-with-file-caller-identities.tsv
```

Then run:

```bash
python3 tools/run_enemy_object_init_actions_callsite.py \
  --ghidra /opt/ghidra \
  --project-dir /path/to/ghidra/projects \
  --project-name Nevergone \
  --program libcocos2dcpp.so
```

Outputs:

```text
build/enemy-object-action-init-evidence/
  enemy-object-init-actions-system-pcode.tsv
  enemy-object-init-actions-callsite-report.txt
```

## Acceptance boundary

A successful report should prove the local structural relation between the
`CCString` input, `getCString`, the `createWithFile` call and concrete
`EnemyObject` field offsets. It still does not, by itself, prove the runtime value
for tutorial enemy ID `3` or authorize inventing an action filename.

The next runtime step is allowed only after the recovered filename/input field is
connected independently to the reviewed enemy-record/action-data mapping for the
concrete tutorial enemy.
