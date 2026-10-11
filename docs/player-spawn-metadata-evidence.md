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
- Y: `CCDictionary::objectForKey` at `0x00351304`, then `CCString::floatValue` at `0x00351308`.

Therefore `playerStartXPos` and `playerStartYPos` are evidence-backed scene-list **floating-point values**. Treating them as integer-only or textual runtime state would contradict the recovered call sequence.

Later in the same function, the resolved call graph contains `cocos2d::CCPoint::CCPoint` at Ghidra `0x003513b8` and `cocos2d::CCPoint::operator=` at `0x003513c2`. This proves that the loader also constructs/assigns a `CCPoint` after parsing the scene entry, but the broad metadata index does not encode register/stack/SSA dataflow, so call proximity alone does not prove that the two start-position floats feed that exact point.

## Project-owned retention

`GameSceneAtlasListDiscovery.Result` retains both values as finite `float` scene-entry metadata alongside the already recovered scene-list path and ordered `gsresfile04` atlas list. Missing, malformed or non-finite start-position values make that candidate fail closed.

This retention does not move a node, instantiate a player, alter the render queue, or infer any camera/control state. Existing atlas staging consumes only `atlasPlists`; the start values are evidence-bearing metadata for the next player-construction boundary.

## Not yet proven

The checked-in resolved call graph contains no caller edge to `GameScene::adjustPlayerPos(int)`. Indirect calls and unresolved references are outside the exported call graph, so absence of an edge is not proof that the function is unused.

Current evidence therefore does **not** prove:

- when the parsed start-position pair is applied to the local player;
- whether the later `CCPoint` assignment is the retained player-start point or another scene-entry point;
- what the `int` argument of `GameScene::adjustPlayerPos(int)` selects;
- the player node/class field that ultimately receives the position;
- camera or control initialization relative to the first placement.

## Safe implementation consequence

It is safe to preserve `playerStartXPos` and `playerStartYPos` as typed floating-point **scene-list metadata**. It is not yet safe to apply them to a reconstructed player transform or claim first-player spawn parity.

The next bounded evidence step is `tools/ghidra/ExportFunctionPcodeFlow.java` plus `tools/ghidra/trace_pcode_flow.py`. Export high P-code metadata for `GameScene::loadGameSceneList`, then test whether each `CCString::floatValue` output reaches the `CCPoint` constructor through actual varnode dependencies rather than address proximity. See `docs/ghidra/function-pcode-flow.md`.

Even a proven `floatValue -> CCPoint` path would only identify the point construction. Player-node placement still requires an onward consumer/dataflow edge or independent evidence for `GameScene::adjustPlayerPos(int)`.

## Reproduction

The checked-in evidence can be reproduced without the original APK/OBB or Ghidra project:

```bash
python tools/ghidra/player_spawn_evidence.py
python tools/ghidra/query_full_index.py callees GameScene::loadGameSceneList --limit 100
```

The deeper value-flow export runs only against the user's analyzed Ghidra project and should remain local unless its sanitized metadata output is intentionally added as new evidence.
