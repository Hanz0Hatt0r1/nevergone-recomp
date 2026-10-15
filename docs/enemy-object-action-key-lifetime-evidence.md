# EnemyObject action dictionary key-lifetime evidence

## Trigger

The bounded dictionary-chain probe for `EnemyObject::initActionsSystem` proved the
`EnemyActionsSystem::createWithFile` `param[0]` path contains two
`CCDictionary::objectForKey` calls. Both key arguments reduce to the same affine
local address, `r54 - 0x30` in the current Ghidra image.

The same report also showed dozens of unrelated calls touching that address. The
slot is therefore reused across multiple C++ temporary lifetimes. Treating every
touch of the affine address as one semantic object would merge independent
constructors, key lookups and destructors.

For the two `objectForKey` calls on the actual `createWithFile param[0]`
dependency path, the observed local sequence is:

```text
0x002c3572  FUN_0073d100(slot, ...)
0x002c357a  CCDictionary::objectForKey(..., slot)
0x002c3582  FUN_0073c028(slot, ...)

0x002c3594  FUN_0073d100(slot, ...)
0x002c359c  CCDictionary::objectForKey(..., slot)
0x002c35a4  FUN_0073c028(slot, ...)
```

This shape is consistent with repeated construction/use/cleanup of one stack
location, but the helper functions are not labelled constructor/destructor from
shape alone.

## Bounded lifetime probe

`tools/enemy_object_action_key_lifetime.py` reuses the focused High-P-code TSV.
It does not decompile another function and does not enumerate program strings.

The `plan` phase:

1. locates the unique `createWithFile` call;
2. walks only `param[0]` dependencies;
3. selects the `objectForKey` calls on that dependency path;
4. reduces each key argument to an affine local slot;
5. selects the nearest preceding and nearest following resolved CALL that touch
   the same slot;
6. records those exact helper entries/sites in a fail-closed TSV plan.

`ExportActionKeyLifetimeFunctionIdentities.java` then asks Ghidra for only those
planned helper entries. It exports entry, short/qualified name, signature and
external status. It does not export bytes or decompiler text.

The `report` phase emits all arguments for each predecessor/`objectForKey`/
successor triple. For every argument it records the structural expression,
affine form, roots, and bounded CALL dependencies. This is specifically intended
to expose the source passed into `FUN_0073d100` for the two relevant key
lifetimes.

## Run

The focused P-code from the earlier call-site probe must already exist:

```text
build/enemy-object-action-init-evidence/
  enemy-object-init-actions-system-pcode.tsv
```

Then run:

```bash
python3 tools/run_enemy_object_action_key_lifetime.py \
  --ghidra /opt/ghidra \
  --project-dir /home/pes/ghidra_projects/nevergone \
  --project-name nevergone \
  --program libcocos2dcpp.so
```

Outputs:

```text
build/enemy-object-action-init-evidence/
  enemy-object-action-key-lifetime-plan.tsv
  enemy-object-action-key-lifetime-helper-identities.tsv
  enemy-object-action-key-lifetime-report.txt
```

## Evidence boundary

A repeated predecessor/successor pair around the same stack slot is temporal
lifetime evidence only. The probe does not call either helper a C++ string
constructor/destructor unless its Ghidra identity/signature or independent native
evidence supports that conclusion.

Likewise, any value feeding the predecessor helper is not automatically an enemy
ID, dictionary key, action-data filename or tutorial resource. Those semantics
must be tied independently to the reviewed EnemyObject/action-data mapping.

The runtime tutorial enemy bridge therefore remains fail-closed until the two
key lifetimes expose enough provenance to bind the final `createWithFile`
resource selection to concrete tutorial enemy ID `3`.
