# Player-start scene-list metadata evidence

This note records the current evidence boundary for issue #148. It deliberately separates scene-list parsing facts from player-placement semantics.

## Proven from checked-in metadata

The metadata-only query in `tools/ghidra/player_spawn_evidence.py` derives the Ghidra-to-ELF load delta from `GameScene::loadGameSceneList(char const*)` instead of hard-coding it. The checked-in Nevergone index produces a delta of `0x00010000`.

The two scene-list keys have exact Ghidra string references inside `GameScene::loadGameSceneList(char const*)`:

| Key | String address | Ghidra reference | ELF code offset |
| --- | ---: | ---: | ---: |
| `playerStartXPos` | `0x007821df` | `0x003512da` | `0x003412da` |
| `playerStartYPos` | `0x007821ef` | `0x003512fa` | `0x003412fa` |

The resolved calls immediately following those references establish the scalar conversion used by the original loader:

- X: `CCDictionary::objectForKey` at Ghidra `0x003512e4`, then `CCString::floatValue` at `0x003512e8`;
- Y: `CCDictionary::objectForKey` at Ghidra `0x00351304`, then `CCString::floatValue` at `0x00351308`.

Therefore `playerStartXPos` and `playerStartYPos` are evidence-backed scene-list **floating-point values**. Treating them as integer-only or textual runtime state would contradict the recovered call sequence.

Later in the same function, the resolved call graph contains `cocos2d::CCPoint::CCPoint` at Ghidra `0x003513b8` and `cocos2d::CCPoint::operator=` at `0x003513c2`. This proves that the loader also constructs/assigns a `CCPoint` after parsing the scene entry, but the metadata index does not encode register/stack dataflow, so it does not by itself prove that the two start-position floats feed that exact point.

## Not yet proven

The checked-in resolved call graph contains no caller edge to `GameScene::adjustPlayerPos(int)`. Indirect calls and unresolved references are outside the exported call graph, so absence of an edge is not proof that the function is unused.

Current evidence therefore does **not** prove:

- when the parsed start-position pair is applied to the local player;
- whether the later `CCPoint` assignment is the retained player-start point or another scene-entry point;
- what the `int` argument of `GameScene::adjustPlayerPos(int)` selects;
- the player node/class field that ultimately receives the position;
- camera or control initialization relative to the first placement.

## Safe implementation consequence

It is now safe to preserve `playerStartXPos` and `playerStartYPos` as typed floating-point **scene-list metadata** when extending the project-owned scene-list model. It is not yet safe to apply them to a reconstructed player transform or claim first-player spawn parity.

The next evidence step should recover the dataflow/consumer boundary around the parsed floats, the later `CCPoint` assignment, or `GameScene::adjustPlayerPos(int)`. A targeted local Ghidra/unidbg probe can close that gap without guessing player internals.

## Reproduction

Run:

```bash
python tools/ghidra/player_spawn_evidence.py
python tools/ghidra/query_full_index.py callees GameScene::loadGameSceneList --limit 100
```

Both commands consume only metadata already checked into the repository. They do not require the original APK/OBB or a Ghidra project at query time.
