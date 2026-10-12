# Narrow Ghidra evidence exporters

These scripts export small, reviewable clean-room slices from an already analyzed local Nevergone program. Raw decompiler output is local evidence and must **not** be committed to this repository.

## SceneActions trigger and count boundary

`ExportSceneActionTriggerSlice.java` exports only the functions needed to recover the two remaining SceneActions runtime gaps.

Spatial trigger slice:

- `GameScene::TriggerSceneAction(float)`;
- `GameSceneClassLayer::showEventObjTip(...)`;
- `GameSceneObject::triggerSceneAction()`;
- both known `GameSceneObject::isContainsRect(...)` overloads;
- `GameSceneObject::getObjectRect()`.

Frame-count slice:

- `SceneActionsSystem::waUpdate(float)`;
- `TrapActionsData::loadWBGFile(...)`.

The loader is resolved by its qualified function-name substring instead of an unverified hardcoded address. The exporter fails closed if that name is missing or ambiguous.

For every resolved target it records the function prototype, direct callers, instruction listing and decompiler C. This is enough to verify the remaining `showEventObjTip` rectangle condition and whether/how `TrapActionsData +0xbc` changes between serialized load and the `waUpdate` one-past comparison.

Example headless invocation:

```bash
/path/to/ghidra/support/analyzeHeadless \
  /path/to/project-dir project-name \
  -process '/program/path/in/project' \
  -noanalysis \
  -scriptPath "$PWD/tools/ghidra" \
  -postScript ExportSceneActionTriggerSlice.java /tmp/nevergone-scene-action-trigger.txt
```

Then inspect or share `/tmp/nevergone-scene-action-trigger.txt`. Keep that raw output outside the repository; only distilled behavioral findings, tests, and evidence notes belong in version control.
