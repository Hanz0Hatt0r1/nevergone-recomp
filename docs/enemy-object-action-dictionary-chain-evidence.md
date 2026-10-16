# EnemyObject action dictionary-chain evidence

## Trigger

The focused `EnemyObject::initActionsSystem` report proved that the unique
`createWithFile` call at Ghidra site `002c3a80` receives:

- `param[0]` from `cocos2d::CCString::getCString`;
- `param[1]` from `EnemyObject + 0x2a0`;
- `param[2]` from `EnemyObject + 0x2a4`;
- `param[7]` from byte field `EnemyObject + 0x34d`.

It also showed that the `getCString` receiver does **not** reduce to the same
root as `EnemyObject *this`. The recovered expression for `param[0]` contains a
nested dictionary pipeline:

```text
CCString::create(...)
  -> CCString::getCString
  -> CCDictionary::createWithContentsOfFile
  -> CCDictionary::objectForKey
  -> CCDictionary::objectForKey
  -> CCString::getCString
  -> EnemyActionsSystem::createWithFile param[0]
```

The key arguments appear as addresses derived from another local/frame root, so
assigning them to the sole `CCString *param_1` formal would be an unsupported ABI
guess.

## Bounded probe

`tools/run_enemy_object_action_dictionary_chain.py` reuses the already-exported
High-P-code TSV from the focused call-site probe. It does **not** decompile the
function again.

The runner first invokes `ExportPcodeDefinedStrings.java`. When the shared
`revenginstr` checkout is available, this adapter calls its generic
`ExportGhidraPcodeDefinedStrings.java` exporter and restores the historical
Nevergone TSV format expected by this analyzer. The legacy local exporter remains
the fallback. Both read only constant tokens already present in the P-code TSV
and ask Ghidra whether each address lies in defined String data. Only matching
defined strings are exported, base64 encoded with their addresses; unrelated
program strings and original bytes are not exported.

`tools/enemy_object_action_dictionary_chain.py` then:

1. requires a unique `createWithFile` call in the focused function;
2. requires `param[0]` to be a direct `getCString` result;
3. walks the bounded SSA dependency graph of that value;
4. records the nested calls and their data arguments;
5. associates reachable P-code constants with only the bounded defined strings
   exported in the first phase;
6. recognizes affine local-address expressions used as `objectForKey` keys;
7. scans the same function for CALL/STORE/LOAD operations touching each exact
   affine slot, exposing constructors or stores that initialize those key
   objects.

The structural chain is considered complete only when the dependency graph
contains at least two `getCString` calls, one `createWithContentsOfFile`, two
`objectForKey` calls and one `CCString::create`.

## Run

The previous focused probe must already have produced:

```text
build/enemy-object-action-init-evidence/
  enemy-object-init-actions-system-pcode.tsv
```

Then run:

```bash
python3 tools/run_enemy_object_action_dictionary_chain.py \
  --ghidra /opt/ghidra \
  --project-dir /path/to/ghidra/projects \
  --project-name Nevergone \
  --program libcocos2dcpp.so
```

Outputs:

```text
build/enemy-object-action-init-evidence/
  enemy-object-init-actions-defined-strings.tsv
  enemy-object-action-dictionary-chain-report.txt
```

## Evidence boundary

A literal recovered from the dictionary construction is evidence that the
original function references that string along the `param[0]` dependency path.
It is not by itself proof that tutorial enemy ID `3` selects that value.

The runtime bridge remains fail-closed until the recovered dictionary path/key
or final action-data value can be independently tied to the reviewed enemy
record / action-data mapping used for concrete enemy ID `3`.
