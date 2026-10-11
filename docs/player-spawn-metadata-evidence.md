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

## Proven ARMv7 value flow into GameLevelsData

A bounded Thumb-2 disassembly check against the user-owned 1.0.9 `libcocos2dcpp.so` (SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`) closes the previous call-proximity ambiguity without retaining the proprietary binary in Git.

Inside `GameScene::loadGameSceneList(char const*)`:

- the X `CCString::floatValue` result at ELF `0x003412e8` is stored at stack offset `+0x18`;
- the Y result at ELF `0x00341308` is stored at stack offset `+0x1c`;
- at ELF `0x003413b2` and `0x003413b6`, those exact stack words are loaded into `r1` and `r2` while `r0` points at a temporary `CCPoint`;
- the call at ELF `0x003413b8` is `cocos2d::CCPoint::CCPoint(float, float)`;
- immediately afterward, `r0` is formed as `GameLevelsData + 0x34`, the temporary point is passed in `r1`, and `CCPoint::operator=` is called at ELF `0x003413c2`.

This establishes real value flow, not address proximity: the parsed `playerStartXPos`/`playerStartYPos` pair constructs a `CCPoint`, and that point is retained in the scene entry's `GameLevelsData` object at offset `+0x34`.

## Project-owned retention

`GameSceneAtlasListDiscovery.Result` retains both values as finite `float` scene-entry metadata alongside the already recovered scene-list path and ordered `gsresfile04` atlas list. Missing, malformed or non-finite start-position values make that candidate fail closed.

`GameSceneStartPoint` now provides the project-owned typed representation of the proven `CCPoint` pair and can be constructed directly from a discovered scene entry. It deliberately carries only finite X/Y values. It does not move a node, instantiate a player, alter the render queue, or infer any camera/control state.

Existing atlas staging still consumes only `atlasPlists`; the start point is evidence-bearing state for the next player-construction boundary.

## Not yet proven

The checked-in resolved call graph contains no caller edge to `GameScene::adjustPlayerPos(int)`. Indirect calls and unresolved references are outside the exported call graph, so absence of an edge is not proof that the function is unused.

Current evidence therefore does **not** prove:

- when the retained `GameLevelsData + 0x34` start point is applied to the local player;
- what the `int` argument of `GameScene::adjustPlayerPos(int)` selects;
- the player node/class field that ultimately receives the position;
- camera or control initialization relative to the first placement.

## Safe implementation consequence

It is now safe to preserve `playerStartXPos` and `playerStartYPos` as one typed project-owned scene start point corresponding to the original `GameLevelsData + 0x34` `CCPoint` field. It is still not safe to apply that point to a reconstructed player transform or claim first-player spawn parity until an onward consumer is proven.

The next bounded evidence step is to trace consumers of the `GameLevelsData + 0x34` field and/or recover the caller/value contract of `GameScene::adjustPlayerPos(int)`. `tools/ghidra/ExportFunctionPcodeFlow.java` plus `tools/ghidra/trace_pcode_flow.py` remain useful for any path whose register/stack dataflow is not obvious from a bounded disassembly window; see `docs/ghidra/function-pcode-flow.md`.

## Reproduction

The checked-in metadata evidence can be reproduced without the original APK/OBB or Ghidra project:

```bash
python tools/ghidra/player_spawn_evidence.py
python tools/ghidra/query_full_index.py callees GameScene::loadGameSceneList --limit 100
```

The instruction-level value-flow observation above comes from the user-owned binary identified by hash. Only the derived addresses, register/stack flow, and field offset are retained in this repository; the original binary remains outside Git.
