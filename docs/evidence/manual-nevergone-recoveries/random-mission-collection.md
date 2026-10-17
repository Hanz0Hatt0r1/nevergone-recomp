# Manual reconstruction: random mission collections

This clean-room module restores six short `GameSaveData` collection methods
selected from the Nevergone code-domain corpus. The native library is pinned by
the Ghidra manifest to SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.
Ghidra 12.1.2 decompilation and the checked-in symbol/call indexes identify the
methods below. Ghidra entry points are shown in the index's image address space;
the corresponding ELF code offsets are 0x10000 lower, as documented in
[`reconstruction-stage-2.md`](../unidbg-2026-10-10/reconstruction-stage-2.md).

## Why these functions

`GameSaveData::getRandomMissDataWithID` at Ghidra entry `0034accc` was ranked
`game` with confidence 0.918 by the held-out v5.2 code-domain model and routed
to `accept`; it is only 23 P-code operations and is not truncated. The nearby
`addRandomMission` and equipped-mission methods form one coherent, similarly
small collection cluster. Their recovered behavior is useful as training data
because it includes both linear lookup and two distinct add policies, including
a capacity check whose order matters.

## Recovered behavior

| Native method | Entry | Behavior |
| --- | --- | --- |
| `getRandomMissDataWithID` | `0034accc` | Scan `this+0x2fc` in order; return the first record whose 32-bit ID at `+0x14` matches, else null. |
| `getRandomMissDataWithIndex` | `0034acf8` | Return the element at the requested index in `this+0x2fc` if it is below the array count, else null. |
| `addRandomMission` | `0034ac96` | Scan `this+0x2fc`; append only when no existing record has the same ID at `+0x14`. |
| `getEquipRandomMissionDataAtIdx` | `0034ad66` | Return the element at the requested index in `this+0x300` if it is below the array count, else null. |
| `getEquipRandomMissionDataWithId` | `0034ad8a` | Scan `this+0x300` in order; return the first record whose ID at `+0x14` matches, else null. |
| `addEquipRandomMission` | `0034ad24` | If `this+0x300` already has 3 elements, return immediately. Otherwise append only if its ID is absent. |

The native methods call `CCArray::count`, `objectAtIndex`, and `addObject`.
The portable implementation keeps the observed logical behavior in
`game_save_data_missions.{h,cpp}`. Its `MissionData` exposes only the observed
ID; it is not a complete ABI layout. Its vectors are non-owning and therefore
do not reproduce Cocos reference counting. Inputs are expected to be valid
mission pointers, matching the native call sites' object contract. Null-pointer
and concurrent-mutation behavior were not probed.

## Corpus labels

[`random-mission-collection-labels.jsonl`](random-mission-collection-labels.jsonl)
adds six `function_role` gold labels: four `collection_lookup` functions and
two `deduplicating_collection_add` functions. The detailed per-function
differences remain in this evidence note. Merge those rows with the existing
labels and rebuild the dataset to make the new supervision available to later
function-role models; this does not train or replace a model.

For the current local corpus, the generated dataset is kept separately so the
existing `dataset-4class` stays intact:

```sh
mkdir -p /tmp/nevergone-ml
cat /home/pes/.local/share/nevergone-ml-v2/labels/libcocos2dcpp.labels.jsonl \
  /home/pes/.local/share/nevergone-ml-v2/labels/libffmpeg.labels.jsonl \
  docs/evidence/manual-nevergone-recoveries/random-mission-collection-labels.jsonl \
  > /tmp/nevergone-ml/labels-with-manual-restorations.jsonl
python3 tools/ml/build_nevergone_dataset.py \
  /home/pes/.local/share/nevergone-ml-v2/corpus/corpus.jsonl \
  /tmp/nevergone-ml/labels-with-manual-restorations.jsonl \
  /home/pes/.local/share/nevergone-ml-v2/dataset-manual-restoration-v1
```
