# Narrow Ghidra evidence exporters

These scripts export small, reviewable clean-room slices from an already analyzed local Nevergone program. Raw decompiler output is local evidence and must **not** be committed to this repository.

## SceneActions spatial trigger

`ExportSceneActionTriggerSlice.java` exports only the functions needed to recover the remaining `showEventObjTip` / `isContainsRect` trigger geometry:

- `GameScene::TriggerSceneAction(float)`;
- `GameSceneClassLayer::showEventObjTip(...)`;
- `GameSceneObject::triggerSceneAction()`;
- both known `GameSceneObject::isContainsRect(...)` overloads;
- `GameSceneObject::getObjectRect()`.

It records the function prototype, direct callers, instructions and decompiler C for those functions only.

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
